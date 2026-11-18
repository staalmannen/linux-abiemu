// SPDX-License-Identifier: GPL-2.0
/*
 * abiemu core - foreign system call entry and exit
 *
 * abi_sys_enter() runs from the sys_enter tracepoint, i.e. after the
 * kernel has saved the user registers but before it looks at the system
 * call number.  For a foreign task we decode the call using the foreign
 * convention and then either
 *
 *   - emulate it right here, and tell the kernel to skip the system
 *     call by setting the number to -1, or
 *   - rewrite number and arguments so that the kernel runs a native
 *     Linux system call; abi_sys_exit() then converts the result.
 *
 * Since v6.13 the system call tracepoints are "faultable": probes run
 * preemptible and may touch user memory.  Handlers must however not
 * block for an unbounded time - anything that can (read, wait, sleep,
 * exit...) has to go through the native path.
 */
#define pr_fmt(fmt) "abiemu: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task_stack.h>
#include <linux/signal.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

#include <abiemu/abiemu.h>
#include "core.h"

static void abi_trace_call(struct abi_ctx *ctx, long ret, const char *how)
{
	const struct abi_sysent *ent = ctx->ent;
	char buf[160];
	int i, n = 0, nargs = ent ? ent->nargs : 6;

	for (i = 0; i < nargs && i < ABI_MAXARGS; i++)
		n += scnprintf(buf + n, sizeof(buf) - n, "%s0x%lx",
			       i ? ", " : "", ctx->args[i]);
	buf[n] = 0;
	pr_info("%s[%d:%s] %s#%u(%s) = %ld%s\n", ctx->pers->name,
		task_pid_nr(current), current->comm,
		ent ? ent->name : "?", ctx->nr, buf, ret, how);
}

static void abi_save_regs(struct abi_ctx *ctx, struct pt_regs *regs)
{
	ctx->sv_nr = regs->orig_ax;
	ctx->sv_di = regs->di;
	ctx->sv_si = regs->si;
	ctx->sv_dx = regs->dx;
	ctx->sv_r10 = regs->r10;
	ctx->sv_r8 = regs->r8;
	ctx->sv_r9 = regs->r9;
}

static void abi_restore_regs(struct abi_ctx *ctx, struct pt_regs *regs)
{
	regs->di = ctx->sv_di;
	regs->si = ctx->sv_si;
	regs->dx = ctx->sv_dx;
	regs->r10 = ctx->sv_r10;
	regs->r8 = ctx->sv_r8;
	regs->r9 = ctx->sv_r9;
}

long abi_native(struct abi_ctx *ctx, int nr, unsigned long a0,
		unsigned long a1, unsigned long a2, unsigned long a3,
		unsigned long a4, unsigned long a5)
{
	struct pt_regs *regs = ctx->regs;

	regs->orig_ax = nr;
	regs->di = a0;
	regs->si = a1;
	regs->dx = a2;
	regs->r10 = a3;
	regs->r8 = a4;
	regs->r9 = a5;
	ctx->state = ABI_ST_NATIVE;
	return 0;
}
EXPORT_SYMBOL_GPL(abi_native);

void __user *abi_gap(struct abi_ctx *ctx, size_t size)
{
	unsigned long base = ctx->gap ? ctx->gap : ctx->regs->sp - 128;

	base = (base - size) & ~15UL;
	ctx->gap = base;
	return (void __user *)base;
}
EXPORT_SYMBOL_GPL(abi_gap);

char __user *abi_gap_str(struct abi_ctx *ctx, const char *s)
{
	size_t len = strlen(s) + 1;
	char __user *p = abi_gap(ctx, len);

	if (copy_to_user(p, s, len))
		return NULL;
	return p;
}
EXPORT_SYMBOL_GPL(abi_gap_str);

/* Translate a negative Linux errno into the foreign (positive) one. */
int abi_errno(const struct abi_personality *p, long err)
{
	unsigned int e = -err;

	if (e < p->nerrno && p->errno_map[e])
		return p->errno_map[e];
	return e;
}
EXPORT_SYMBOL_GPL(abi_errno);

/* The system call is over; hand the result to the foreign program. */
static void abi_finish(struct abi_ctx *ctx, long ret, const char *how)
{
	struct pt_regs *regs = ctx->regs;

	ctx->state = ABI_ST_IDLE;
	/* Keep the kernel's own restart logic out of this. */
	regs->orig_ax = -1;
	if (!(ctx->flags & ABI_F_NORESULT))
		ctx->pers->set_result(ctx, ret);
	if (abi_trace >= ABI_TRACE_CALLS)
		abi_trace_call(ctx, ret, how);
}

void abi_sys_enter(void *data, struct pt_regs *regs, long id)
{
	unsigned int per = current->personality & PER_MASK;
	const struct abi_personality *p;
	const struct abi_sysent *ent;
	struct abi_task *t;
	struct abi_ctx *ctx;
	long ret;

	if (likely(!abi_per_foreign(per)))
		return;

	t = abi_task_current(true);
	if (!t) {
		regs->orig_ax = -1;
		regs->ax = -ENOMEM;
		return;
	}
	p = abi_task_bind(t, per);
	if (!p)
		return;

	ctx = &t->ctx;
	ctx->regs = regs;
	ctx->pers = p;
	ctx->ent = NULL;
	ctx->post = NULL;
	ctx->flags = 0;
	ctx->gap = 0;
	ctx->state = ABI_ST_EMUL;
	abi_save_regs(ctx, regs);

	/* calls made by our signal trampoline, see signal.c */
	if (unlikely(regs->ip - 2 - ABI_TRAMP_ADDR < 8)) {
		ctx->nr = regs->ip - 2 - ABI_TRAMP_ADDR;
		memset(ctx->args, 0, sizeof(ctx->args));
		ret = abi_sig_trap(ctx);
		if (ctx->state == ABI_ST_NATIVE)
			return;
		goto done;
	}

	ret = p->decode(ctx);
	if (ret)
		goto done;

	ent = ctx->nr < p->nsysent ? &p->sysent[ctx->nr] : NULL;
	if (!ent || !ent->name) {
		if (abi_trace >= ABI_TRACE_UNIMPL)
			pr_info("%s[%d:%s] unimplemented system call %u\n",
				p->name, task_pid_nr(current), current->comm,
				ctx->nr);
		ret = -ENOSYS;
		goto done;
	}
	ctx->ent = ent;
	if (ent->handler)
		ret = ent->handler(ctx);
	else
		ret = abi_native(ctx, ent->native, ctx->args[0], ctx->args[1],
				 ctx->args[2], ctx->args[3], ctx->args[4],
				 ctx->args[5]);
	if (ctx->state == ABI_ST_NATIVE)
		return;
	if (ret == -ENOSYS && abi_trace == ABI_TRACE_UNIMPL)
		abi_trace_call(ctx, ret, " (unimplemented)");
done:
	abi_finish(ctx, ret, "");
}

/*
 * A native call was interrupted by a signal.  Decide whether the
 * foreign call is restarted or fails with EINTR, the way get_signal()
 * and handle_signal() would: the first pending signal that has a
 * handler decides.  We cannot leave this to the kernel since it would
 * restart with the native number and report EINTR in the Linux
 * convention.
 */
static bool abi_should_restart(long err)
{
	struct task_struct *tsk = current;
	bool restart = true;
	sigset_t pending;
	int sig;

	if (err == -ERESTARTNOINTR)
		return true;

	spin_lock_irq(&tsk->sighand->siglock);
	sigorsets(&pending, &tsk->pending.signal,
		  &tsk->signal->shared_pending.signal);
	sigandnsets(&pending, &pending, &tsk->blocked);
	for (sig = 1; sig <= _NSIG; sig++) {
		struct k_sigaction *ka;

		if (!sigismember(&pending, sig))
			continue;
		ka = &tsk->sighand->action[sig - 1];
		if (ka->sa.sa_handler == SIG_IGN ||
		    ka->sa.sa_handler == SIG_DFL)
			continue;
		restart = err == -ERESTARTSYS &&
			  (ka->sa.sa_flags & SA_RESTART);
		break;
	}
	spin_unlock_irq(&tsk->sighand->siglock);
	return restart;
}

void abi_sys_exit(void *data, struct pt_regs *regs, long ret)
{
	unsigned int per = current->personality & PER_MASK;
	const struct abi_personality *p;
	struct abi_task *t;
	struct abi_ctx *ctx;

	if (likely(!abi_per_foreign(per)))
		return;
	t = abi_task_current(false);
	if (!t) {
		/* a new task returning from fork()/clone() for the first time */
		rcu_read_lock();
		p = rcu_dereference(abi_pers[per]);
		if (p && p->fork_return && try_module_get(p->owner)) {
			rcu_read_unlock();
			p->fork_return(regs);
			module_put(p->owner);
		} else {
			rcu_read_unlock();
		}
		return;
	}
	ctx = &t->ctx;

	if (t->exec_mm) {
		struct mm_struct *mm = t->exec_mm;
		bool setup = t->exec_setup;

		t->exec_mm = NULL;
		t->exec_setup = false;
		if (mm == current->mm) {
			/* This is the return into a freshly loaded image. */
			ctx->state = ABI_ST_IDLE;
			kfree(t->priv);
			t->priv = NULL;
			p = abi_task_bind(t, per);
			if (p && setup && p->exec_setup)
				p->exec_setup(regs);
			return;
		}
	}

	if (ctx->state != ABI_ST_NATIVE)
		return;
	ctx->regs = regs;
	if (ctx->flags & ABI_F_NORESULT) {
		/* sigreturn: the registers are those of the old context */
		abi_finish(ctx, 0, " (native)");
		return;
	}
	abi_restore_regs(ctx, regs);

	switch (ret) {
	case -ERESTARTSYS:
	case -ERESTARTNOINTR:
	case -ERESTARTNOHAND:
	case -ERESTART_RESTARTBLOCK:
		if (!(ctx->flags & ABI_F_NORESTART) &&
		    abi_should_restart(ret)) {
			/* Back up onto the syscall instruction. */
			ctx->state = ABI_ST_IDLE;
			regs->ax = ctx->sv_nr;
			regs->orig_ax = -1;
			regs->ip -= 2;
			if (abi_trace >= ABI_TRACE_CALLS)
				abi_trace_call(ctx, ret, " (restart)");
			return;
		}
		ret = -EINTR;
		break;
	}

	if (ctx->post)
		ret = ctx->post(ctx, ret);
	if (ctx->flags & ABI_F_REDO) {
		/* the handler has more native work to do: enter it again */
		ctx->state = ABI_ST_IDLE;
		regs->ax = ctx->sv_nr;
		regs->orig_ax = -1;
		regs->ip -= 2;
		return;
	}
	abi_finish(ctx, ret, " (native)");
}

/*
 * The child's registers are a copy of the parent's, which currently
 * hold a rewritten native clone()/fork() call.  Called in the parent,
 * before the child runs.
 */
void abi_tp_fork(void *data, struct task_struct *parent,
		 struct task_struct *child)
{
	unsigned int per = current->personality & PER_MASK;
	struct pt_regs *regs;
	struct abi_task *t;

	if (likely(!abi_per_foreign(per)))
		return;
	rcu_read_lock();
	t = abi_task_current(false);
	rcu_read_unlock();
	if (!t || t->ctx.state != ABI_ST_NATIVE || !t->pers)
		return;

	regs = task_pt_regs(child);
	abi_restore_regs(&t->ctx, regs);
	regs->orig_ax = -1;
	if (t->pers->fork_child)
		t->pers->fork_child(&t->ctx, regs);
}
