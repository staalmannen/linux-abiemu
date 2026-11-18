// SPDX-License-Identifier: GPL-2.0
/*
 * abi_plan9 - notes
 *
 * notify(fn) registers one handler for all notes; we install it for the
 * signals that correspond to Plan 9 notes.  The handler is entered the
 * way the Plan 9 kernel does it: RARG and 8(SP) hold a Ureg *, 16(SP)
 * the note string.  It leaves with noted(NCONT) or noted(NDFLT).
 */
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/unistd.h>
#include <asm/sigcontext.h>

#include "plan9.h"

/* the signals a note handler receives */
static const int p9_note_sigs[] = {
	SIGHUP, SIGINT, SIGQUIT, SIGALRM, SIGTERM, SIGPIPE, SIGUSR1, SIGUSR2,
	SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGTRAP, SIGSYS,
};

const char *p9_note_name(int lsig, char *buf, unsigned long addr,
			 unsigned long pc)
{
	switch (lsig) {
	case SIGHUP:	return "hangup";
	case SIGINT:	return "interrupt";
	case SIGQUIT:	return "quit";
	case SIGALRM:	return "alarm";
	case SIGTERM:	return "kill";
	case SIGKILL:	return "kill";
	case SIGPIPE:	return "sys: write on closed pipe";
	case SIGUSR1:	return "usr1";
	case SIGUSR2:	return "usr2";
	case SIGSEGV:
	case SIGBUS:
		snprintf(buf, P9_ERRMAX, "sys: trap: fault read addr=%#lx pc=%#lx",
			 addr, pc);
		return buf;
	case SIGFPE:
		snprintf(buf, P9_ERRMAX, "sys: fp: trap pc=%#lx", pc);
		return buf;
	case SIGILL:
		snprintf(buf, P9_ERRMAX, "sys: trap: illegal instruction pc=%#lx", pc);
		return buf;
	}
	snprintf(buf, P9_ERRMAX, "sys: signal %d", lsig);
	return buf;
}

static long p9_notify_post(struct abi_ctx *ctx, long ret)
{
	struct p9_task *t = p9_task();

	if (ret || !t)
		return ret;
	if (++t->notify_idx < ARRAY_SIZE(p9_note_sigs))
		ctx->flags |= ABI_F_REDO;	/* next signal */
	return 0;
}

/*
 * notify(void (*fn)(void *, char *)).  One rt_sigaction per signal: the
 * call is re-entered (ABI_F_REDO) until all of them are installed.
 */
long p9_sys_notify(struct abi_ctx *ctx)
{
	struct p9_task *t = p9_task();
	struct abi_sigact act = {
		.handler = ctx->args[0] ?: (unsigned long)SIG_DFL,
		.flags = SA_RESTART,
		.mask = ~0ULL,		/* notes are not nested */
	};

	if (!t)
		return -ENOMEM;
	if (t->notify_idx >= ARRAY_SIZE(p9_note_sigs) ||
	    t->notify_fn != ctx->args[0]) {
		t->notify_idx = 0;
		t->notify_fn = ctx->args[0];
	}
	ctx->post = p9_notify_post;
	return abi_sigaction(ctx, p9_note_sigs[t->notify_idx], &act, false);
}

struct p9_ureg {
	u64	ax, bx, cx, dx, si, di, bp, r8, r9, r10, r11, r12, r13, r14, r15;
	u16	ds, es, fs, gs;
	u64	type, error, pc, cs, flags, sp, ss;
};

int p9_sig_deliver(struct abi_ctx *ctx, int lsig, unsigned long handler,
		   unsigned long lframe)
{
	struct p9_task *t = p9_task();
	struct pt_regs *regs = ctx->regs;
	struct abi_linux_sigframe *lf;
	struct p9_ureg u = { 0 };
	char note[P9_ERRMAX], buf[P9_ERRMAX];
	unsigned long sp, nureg, msg, args[3];
	struct sigcontext *sc;
	int err = -EFAULT;

	if (!t)
		return -ENOMEM;
	lf = kmalloc(sizeof(*lf), GFP_KERNEL);
	if (!lf)
		return -ENOMEM;
	if (copy_from_user(lf, (void __user *)lframe, sizeof(*lf)))
		goto out;
	sc = (struct sigcontext *)&lf->uc.uc_mcontext;

	u.ax = sc->ax; u.bx = sc->bx; u.cx = sc->cx; u.dx = sc->dx;
	u.si = sc->si; u.di = sc->di; u.bp = sc->bp; u.r8 = sc->r8;
	u.r9 = sc->r9; u.r10 = sc->r10; u.r11 = sc->r11; u.r12 = sc->r12;
	u.r13 = sc->r13; u.r14 = sc->r14; u.r15 = sc->r15;
	u.type = sc->trapno; u.error = sc->err; u.pc = sc->ip; u.cs = sc->cs;
	u.flags = sc->flags; u.sp = sc->sp; u.ss = sc->ss;
	strscpy(note, p9_note_name(lsig, buf, (unsigned long)lf->info.si_addr,
				   sc->ip), sizeof(note));

	/* the same layout the Plan 9 kernel builds in notify() */
	sp = lframe - 256 - sizeof(u);
	nureg = sp &= ~15UL;
	sp -= 8 + P9_ERRMAX;
	msg = sp;
	sp -= 3 * 8;
	args[0] = 0;
	args[1] = nureg;
	args[2] = msg;
	if (copy_to_user((void __user *)nureg, &u, sizeof(u)) ||
	    copy_to_user((void __user *)msg, note, P9_ERRMAX) ||
	    copy_to_user((void __user *)sp, args, sizeof(args)))
		goto out;

	t->note_lframe = lframe;
	t->note_ureg = nureg;
	regs->sp = sp;
	regs->ip = handler;
	regs->bp = nureg;		/* first argument in RARG */
	err = 0;
out:
	kfree(lf);
	return err;
}

#define P9_NCONT	0
#define P9_NDFLT	1

/* noted(int v): leave a note handler */
long p9_sys_noted(struct abi_ctx *ctx)
{
	struct p9_task *t = p9_task();
	struct ucontext __user *luc;
	struct sigcontext sc;
	struct p9_ureg u;
	char note[P9_ERRMAX];
	unsigned long lframe;

	if (!t || !t->note_lframe)
		return p9_error(ctx, "not in a note handler");
	luc = (void __user *)(t->note_lframe +
			      offsetof(struct abi_linux_sigframe, uc));

	if (ctx->args[0] == P9_NDFLT) {
		/* the default action: die with the note as exit message */
		if (strncpy_from_user(note, (char __user *)(t->note_ureg - 8 -
							    P9_ERRMAX),
				      sizeof(note)) < 0)
			strscpy(note, "note", sizeof(note));
		note[P9_ERRMAX - 1] = 0;
		p9_exitmsg_put(task_tgid_vnr(current), note);
		return abi_native1(ctx, __NR_exit_group, 1);
	}
	if (ctx->args[0] != P9_NCONT)
		return p9_error(ctx, "bad arg in system call");

	if (copy_from_user(&u, (void __user *)t->note_ureg, sizeof(u)) ||
	    copy_from_user(&sc, &luc->uc_mcontext, sizeof(sc)))
		return -EFAULT;
	sc.ax = u.ax; sc.bx = u.bx; sc.cx = u.cx; sc.dx = u.dx;
	sc.si = u.si; sc.di = u.di; sc.bp = u.bp; sc.r8 = u.r8;
	sc.r9 = u.r9; sc.r10 = u.r10; sc.r11 = u.r11; sc.r12 = u.r12;
	sc.r13 = u.r13; sc.r14 = u.r14; sc.r15 = u.r15;
	sc.ip = u.pc; sc.sp = u.sp; sc.flags = u.flags;
	if (copy_to_user(&luc->uc_mcontext, &sc, sizeof(sc)))
		return -EFAULT;
	lframe = t->note_lframe;
	t->note_lframe = 0;
	return abi_sigreturn(ctx, lframe);
}
