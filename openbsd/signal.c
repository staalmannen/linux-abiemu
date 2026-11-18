// SPDX-License-Identifier: GPL-2.0
/*
 * abi_openbsd - signals
 *
 * Signal masks are passed by value and returned in %rax.
 */
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/unistd.h>
#include <asm/sigcontext.h>

#include "openbsd.h"

struct openbsd_sigaction {
	u64	handler;
	u32	sa_mask;
	int	sa_flags;
};

static const struct abi_flagmap openbsd_saflags[] = {
	{ 0x0001, SA_ONSTACK }, { 0x0002, SA_RESTART },
	{ 0x0004, SA_RESETHAND }, { 0x0008, SA_NOCLDSTOP },
	{ 0x0010, SA_NODEFER }, { 0x0020, SA_NOCLDWAIT },
	{ 0x0040, SA_SIGINFO }, { 0 }
};

static long openbsd_sigaction_post(struct abi_ctx *ctx, long ret)
{
	struct openbsd_sigaction osa = { 0 };
	struct abi_sigact old;

	if (ret || !ctx->args[2])
		return ret;
	if (abi_sigaction_old(ctx, &old))
		return -EFAULT;
	osa.handler = old.handler;
	osa.sa_flags = abi_flags_from_linux(openbsd_saflags, old.flags);
	osa.sa_mask = openbsd_sigset_from_linux(old.mask);
	return copy_to_user((void __user *)ctx->args[2], &osa, sizeof(osa)) ?
		-EFAULT : 0;
}

/* sigaction(int sig, const struct sigaction *nsa, struct sigaction *osa) */
long openbsd_sys_sigaction(struct abi_ctx *ctx)
{
	int lsig = abi_bsd_sig_to_linux(ctx->args[0]);
	struct openbsd_sigaction osa;
	struct abi_sigact act;

	if (lsig <= 0)
		return -EINVAL;
	if (ctx->args[1]) {
		if (copy_from_user(&osa, (void __user *)ctx->args[1], sizeof(osa)))
			return -EFAULT;
		act.handler = osa.handler;
		act.flags = abi_flags_to_linux(openbsd_saflags, osa.sa_flags);
		act.mask = openbsd_sigset_to_linux(osa.sa_mask);
	}
	ctx->post = openbsd_sigaction_post;
	return abi_sigaction(ctx, lsig, ctx->args[1] ? &act : NULL,
			     ctx->args[2] != 0);
}

static long openbsd_oldmask_post(struct abi_ctx *ctx, long ret)
{
	u64 lset;

	if (ret)
		return ret;
	if (get_user(lset, (u64 __user *)ctx->priv[0]))
		return -EFAULT;
	return openbsd_sigset_from_linux(lset);
}

/* sigprocmask(int how, sigset_t mask): returns the old mask */
long openbsd_sys_sigprocmask(struct abi_ctx *ctx)
{
	u64 __user *gset = abi_gap(ctx, sizeof(u64));
	u64 __user *gold = abi_gap(ctx, sizeof(u64));
	int how = ctx->args[0];

	if (how < 1 || how > 3)
		return -EINVAL;
	if (put_user(openbsd_sigset_to_linux(ctx->args[1]), gset))
		return -EFAULT;
	ctx->priv[0] = (unsigned long)gold;
	ctx->post = openbsd_oldmask_post;
	return abi_native4(ctx, __NR_rt_sigprocmask, how - 1,
			   (unsigned long)gset, (unsigned long)gold, 8);
}

/* sigsuspend(sigset_t mask) */
long openbsd_sys_sigsuspend(struct abi_ctx *ctx)
{
	u64 __user *gset = abi_gap(ctx, sizeof(u64));

	if (put_user(openbsd_sigset_to_linux(ctx->args[0]), gset))
		return -EFAULT;
	return abi_native2(ctx, __NR_rt_sigsuspend, (unsigned long)gset, 8);
}

/* sigpending(void): returns the mask */
long openbsd_sys_sigpending(struct abi_ctx *ctx)
{
	u64 __user *gset = abi_gap(ctx, sizeof(u64));

	ctx->priv[0] = (unsigned long)gset;
	ctx->post = openbsd_oldmask_post;
	return abi_native2(ctx, __NR_rt_sigpending, (unsigned long)gset, 8);
}

/* thrkill(pid_t tid, int sig, void *tcb); tid 0 is the calling thread */
long openbsd_sys_thrkill(struct abi_ctx *ctx)
{
	int sig = abi_bsd_sig_to_linux(ctx->args[1]);
	pid_t tid = ctx->args[0] ?: task_pid_vnr(current);

	if (sig < 0)
		return -EINVAL;
	return abi_native2(ctx, __NR_tkill, tid, sig);
}

/*
 * Signal frames: handler(sig, siginfo_t *, struct sigcontext *)
 */
struct openbsd_sigcontext {
	u64	sc_rdi, sc_rsi, sc_rdx, sc_rcx, sc_r8, sc_r9, sc_r10, sc_r11,
		sc_r12, sc_r13, sc_r14, sc_r15, sc_rbp, sc_rbx, sc_rax,
		sc_gs, sc_fs, sc_es, sc_ds, sc_trapno, sc_err, sc_rip, sc_cs,
		sc_rflags, sc_rsp, sc_ss;
	u64	sc_fpstate;
	int	__sc_unused;
	int	sc_mask;
	s64	sc_cookie;
};

struct openbsd_siginfo {
	int	osi_signo, osi_code, osi_errno, pad;
	union {
		struct { s32 pid; u32 uid; u64 value; } kill;
		struct { s32 pid; u32 uid; u64 utime, stime; int status; } cld;
		struct { u64 addr; int trapno; } fault;
		u8 _pad[112];
	};
};

struct openbsd_sigframe {
	u64				retaddr;
	struct openbsd_sigcontext	sc;
	struct openbsd_siginfo		si;
	u64				magic;
	u64				lframe;
} __packed;

#define OPENBSD_FRAME_MAGIC	0x6162696f70656e62ULL

int openbsd_sig_deliver(struct abi_ctx *ctx, int lsig, unsigned long handler,
			unsigned long lframe)
{
	struct pt_regs *regs = ctx->regs;
	struct abi_linux_sigframe *lf;
	struct openbsd_sigframe *f;
	struct openbsd_sigcontext *o;
	kernel_siginfo_t *info;
	struct sigcontext *sc;
	unsigned long sp;
	int err = -EFAULT;

	lf = kmalloc(sizeof(*lf), GFP_KERNEL);
	f = kzalloc(sizeof(*f), GFP_KERNEL);
	if (!lf || !f) {
		err = -ENOMEM;
		goto out;
	}
	if (copy_from_user(lf, (void __user *)lframe, sizeof(*lf)))
		goto out;
	sc = (struct sigcontext *)&lf->uc.uc_mcontext;
	info = (kernel_siginfo_t *)&lf->info;

	f->retaddr = ABI_TRAMP_RETURN;
	f->magic = OPENBSD_FRAME_MAGIC;
	f->lframe = lframe;
	o = &f->sc;
	o->sc_rdi = sc->di; o->sc_rsi = sc->si; o->sc_rdx = sc->dx;
	o->sc_rcx = sc->cx; o->sc_r8 = sc->r8; o->sc_r9 = sc->r9;
	o->sc_r10 = sc->r10; o->sc_r11 = sc->r11; o->sc_r12 = sc->r12;
	o->sc_r13 = sc->r13; o->sc_r14 = sc->r14; o->sc_r15 = sc->r15;
	o->sc_rbp = sc->bp; o->sc_rbx = sc->bx; o->sc_rax = sc->ax;
	o->sc_trapno = sc->trapno; o->sc_err = sc->err; o->sc_rip = sc->ip;
	o->sc_cs = sc->cs; o->sc_rflags = sc->flags; o->sc_rsp = sc->sp;
	o->sc_ss = sc->ss;
	o->sc_mask = openbsd_sigset_from_linux(lf->uc.uc_sigmask.sig[0]);
	o->sc_cookie = (s64)(f->magic ^ lframe);

	f->si.osi_signo = abi_bsd_sig_from_linux(lsig);
	f->si.osi_errno = info->si_errno;
	f->si.osi_code = info->si_code;
	switch (lsig) {
	case SIGSEGV: case SIGBUS: case SIGILL: case SIGFPE: case SIGTRAP:
		f->si.fault.addr = (unsigned long)info->si_addr;
		f->si.fault.trapno = sc->trapno;
		break;
	case SIGCHLD:
		f->si.cld.pid = info->si_pid;
		f->si.cld.uid = info->si_uid;
		f->si.cld.status = info->si_code == CLD_EXITED ?
			info->si_status : abi_bsd_sig_from_linux(info->si_status);
		break;
	default:
		f->si.kill.pid = info->si_pid;
		f->si.kill.uid = info->si_uid;
		f->si.kill.value = (unsigned long)info->si_ptr;
	}

	sp = ((lframe - sizeof(*f)) & ~15UL) - 8;
	if (copy_to_user((void __user *)sp, f, sizeof(*f)))
		goto out;
	regs->sp = sp;
	regs->ip = handler;
	regs->di = f->si.osi_signo;
	regs->si = sp + offsetof(struct openbsd_sigframe, si);
	regs->dx = sp + offsetof(struct openbsd_sigframe, sc);
	regs->ax = 0;
	err = 0;
out:
	kfree(lf);
	kfree(f);
	return err;
}

static long openbsd_sigreturn_frame(struct abi_ctx *ctx, unsigned long usc)
{
	unsigned long base = usc - offsetof(struct openbsd_sigframe, sc);
	struct openbsd_sigframe *f;
	struct openbsd_sigcontext *o;
	struct ucontext __user *luc;
	struct sigcontext sc;
	long ret = -EFAULT;

	f = kmalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	if (copy_from_user(f, (void __user *)base, sizeof(*f)))
		goto out;
	ret = -EINVAL;
	if (f->magic != OPENBSD_FRAME_MAGIC)
		goto out;
	luc = (void __user *)(f->lframe +
			      offsetof(struct abi_linux_sigframe, uc));
	ret = -EFAULT;
	if (copy_from_user(&sc, &luc->uc_mcontext, sizeof(sc)))
		goto out;
	o = &f->sc;
	sc.di = o->sc_rdi; sc.si = o->sc_rsi; sc.dx = o->sc_rdx;
	sc.cx = o->sc_rcx; sc.r8 = o->sc_r8; sc.r9 = o->sc_r9;
	sc.r10 = o->sc_r10; sc.r11 = o->sc_r11; sc.r12 = o->sc_r12;
	sc.r13 = o->sc_r13; sc.r14 = o->sc_r14; sc.r15 = o->sc_r15;
	sc.bp = o->sc_rbp; sc.bx = o->sc_rbx; sc.ax = o->sc_rax;
	sc.ip = o->sc_rip; sc.sp = o->sc_rsp; sc.flags = o->sc_rflags;
	if (copy_to_user(&luc->uc_mcontext, &sc, sizeof(sc)) ||
	    put_user(openbsd_sigset_to_linux(o->sc_mask),
		     (u64 __user *)&luc->uc_sigmask))
		goto out;
	ret = abi_sigreturn(ctx, f->lframe);
out:
	kfree(f);
	return ret;
}

long openbsd_sig_return(struct abi_ctx *ctx)
{
	return openbsd_sigreturn_frame(ctx, ctx->regs->sp);
}

/* sigreturn(struct sigcontext *scp) */
long openbsd_sys_sigreturn(struct abi_ctx *ctx)
{
	return openbsd_sigreturn_frame(ctx, ctx->args[0]);
}
