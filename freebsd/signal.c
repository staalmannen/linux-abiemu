// SPDX-License-Identifier: GPL-2.0
/*
 * abi_freebsd - signals
 */
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <asm/unistd.h>
#include <asm/sigcontext.h>

#include "freebsd.h"

struct freebsd_sigaction {
	u64	handler;
	int	sa_flags;
	u32	sa_mask[4];
};

static const struct abi_flagmap freebsd_saflags[] = {
	{ 0x0001, SA_ONSTACK }, { 0x0002, SA_RESTART },
	{ 0x0004, SA_RESETHAND }, { 0x0008, SA_NOCLDSTOP },
	{ 0x0010, SA_NODEFER }, { 0x0020, SA_NOCLDWAIT },
	{ 0x0040, SA_SIGINFO }, { 0 }
};

static long freebsd_sigaction_post(struct abi_ctx *ctx, long ret)
{
	struct freebsd_sigaction fsa = { 0 };
	struct abi_sigact old;

	if (ret || !ctx->args[2])
		return ret;
	if (abi_sigaction_old(ctx, &old))
		return -EFAULT;
	fsa.handler = old.handler;
	fsa.sa_flags = abi_flags_from_linux(freebsd_saflags, old.flags);
	abi_bsd_sigset_from_linux(old.mask, fsa.sa_mask);
	if (copy_to_user((void __user *)ctx->args[2], &fsa, sizeof(fsa)))
		return -EFAULT;
	return 0;
}

/* sigaction(int sig, const struct sigaction *act, struct sigaction *oact) */
long freebsd_sys_sigaction(struct abi_ctx *ctx)
{
	int lsig = abi_bsd_sig_to_linux(ctx->args[0]);
	struct freebsd_sigaction fsa;
	struct abi_sigact act;

	if (lsig <= 0)
		return -EINVAL;
	if (ctx->args[1]) {
		if (copy_from_user(&fsa, (void __user *)ctx->args[1], sizeof(fsa)))
			return -EFAULT;
		act.handler = fsa.handler;
		act.flags = abi_flags_to_linux(freebsd_saflags, fsa.sa_flags);
		act.mask = abi_bsd_sigset_to_linux(fsa.sa_mask);
	}
	ctx->post = freebsd_sigaction_post;
	return abi_sigaction(ctx, lsig, ctx->args[1] ? &act : NULL,
			     ctx->args[2] != 0);
}

/* sigsuspend(const sigset_t *mask) */
long freebsd_sys_sigsuspend(struct abi_ctx *ctx)
{
	u64 __user *gset = abi_gap(ctx, sizeof(u64));
	u32 set[4];

	if (copy_from_user(set, (void __user *)ctx->args[0], sizeof(set)))
		return -EFAULT;
	if (put_user(abi_bsd_sigset_to_linux(set), gset))
		return -EFAULT;
	return abi_native2(ctx, __NR_rt_sigsuspend, (unsigned long)gset, 8);
}

static long freebsd_sigpending_post(struct abi_ctx *ctx, long ret)
{
	u32 set[4] = { 0 };
	u64 lset;

	if (ret)
		return ret;
	if (get_user(lset, (u64 __user *)ctx->priv[0]))
		return -EFAULT;
	abi_bsd_sigset_from_linux(lset, set);
	return copy_to_user((void __user *)ctx->args[0], set, sizeof(set)) ?
		-EFAULT : 0;
}

long freebsd_sys_sigpending(struct abi_ctx *ctx)
{
	void __user *gset = abi_gap(ctx, sizeof(u64));

	ctx->priv[0] = (unsigned long)gset;
	ctx->post = freebsd_sigpending_post;
	return abi_native2(ctx, __NR_rt_sigpending, (unsigned long)gset, 8);
}

/* thr_kill(long id, int sig); our thread ids are Linux tids */
long freebsd_sys_thr_kill(struct abi_ctx *ctx)
{
	int sig = abi_bsd_sig_to_linux(ctx->args[1]);

	if (sig < 0)
		return -EINVAL;
	return abi_native2(ctx, __NR_tkill, ctx->args[0], sig);
}

/*
 * Signal frames
 */
struct freebsd_mcontext {
	u64	mc_onstack;
	u64	mc_rdi, mc_rsi, mc_rdx, mc_rcx, mc_r8, mc_r9, mc_rax, mc_rbx,
		mc_rbp, mc_r10, mc_r11, mc_r12, mc_r13, mc_r14, mc_r15;
	u32	mc_trapno;
	u16	mc_fs, mc_gs;
	u64	mc_addr;
	u32	mc_flags;
	u16	mc_es, mc_ds;
	u64	mc_err, mc_rip, mc_cs, mc_rflags, mc_rsp, mc_ss;
	u64	mc_len;
	u64	mc_fpformat;
	u64	mc_ownedfp;
	u64	mc_fpstate[64];
	u64	mc_fsbase, mc_gsbase;
	u64	mc_xfpustate, mc_xfpustate_len;
	u64	mc_spare[4];
};

struct freebsd_ucontext {
	u32			uc_sigmask[4];
	struct freebsd_mcontext	uc_mcontext;
	u64			uc_link;
	u64			uc_stack_sp;
	u64			uc_stack_size;
	int			uc_stack_flags;
	int			uc_flags;
	int			spare[4];
};

struct freebsd_siginfo {
	int	fsi_signo, fsi_errno, fsi_code, fsi_pid, fsi_uid, fsi_status;
	u64	fsi_addr;
	u64	fsi_value;
	u64	reason[5];
};

#define FREEBSD_SI_USER		0x10001
#define FREEBSD_SI_QUEUE	0x10002
#define FREEBSD_SI_KERNEL	0x10006
#define FREEBSD_MC_FPFMT_NODEV	0x10000
#define FREEBSD_MC_FPOWNED_NONE	0x20000

/*
 * What we push below the Linux frame.  The handler is entered with
 * %rsp pointing at @retaddr, so that it returns into the trampoline.
 * The ucontext then sits on a 16 byte boundary.
 */
struct freebsd_sigframe {
	u64			retaddr;
	struct freebsd_ucontext	uc;
	struct freebsd_siginfo	si;
	u64			magic;
	u64			lframe;
} __packed;

#define FREEBSD_FRAME_MAGIC	0x6162696672616d65ULL	/* "abiframe" */

int freebsd_sig_deliver(struct abi_ctx *ctx, int lsig, unsigned long handler,
			unsigned long lframe)
{
	struct pt_regs *regs = ctx->regs;
	struct abi_linux_sigframe *lf;
	struct freebsd_sigframe *f;
	struct freebsd_mcontext *mc;
	struct sigcontext *sc;
	kernel_siginfo_t *info;
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
	f->magic = FREEBSD_FRAME_MAGIC;
	f->lframe = lframe;

	abi_bsd_sigset_from_linux(lf->uc.uc_sigmask.sig[0], f->uc.uc_sigmask);
	mc = &f->uc.uc_mcontext;
	mc->mc_onstack = !!(lf->uc.uc_stack.ss_flags & SS_ONSTACK);
	mc->mc_rdi = sc->di; mc->mc_rsi = sc->si; mc->mc_rdx = sc->dx;
	mc->mc_rcx = sc->cx; mc->mc_r8 = sc->r8; mc->mc_r9 = sc->r9;
	mc->mc_rax = sc->ax; mc->mc_rbx = sc->bx; mc->mc_rbp = sc->bp;
	mc->mc_r10 = sc->r10; mc->mc_r11 = sc->r11; mc->mc_r12 = sc->r12;
	mc->mc_r13 = sc->r13; mc->mc_r14 = sc->r14; mc->mc_r15 = sc->r15;
	mc->mc_trapno = sc->trapno;
	mc->mc_addr = sc->cr2;
	mc->mc_err = sc->err;
	mc->mc_rip = sc->ip;
	mc->mc_cs = sc->cs;
	mc->mc_rflags = sc->flags;
	mc->mc_rsp = sc->sp;
	mc->mc_ss = sc->ss;
	mc->mc_len = sizeof(*mc);
	/* the FPU state stays in the Linux frame */
	mc->mc_fpformat = FREEBSD_MC_FPFMT_NODEV;
	mc->mc_ownedfp = FREEBSD_MC_FPOWNED_NONE;
	f->uc.uc_stack_sp = (unsigned long)lf->uc.uc_stack.ss_sp;
	f->uc.uc_stack_size = lf->uc.uc_stack.ss_size;

	f->si.fsi_signo = abi_bsd_sig_from_linux(lsig);
	f->si.fsi_errno = info->si_errno;
	if (info->si_code == SI_KERNEL)
		f->si.fsi_code = FREEBSD_SI_KERNEL;
	else if (info->si_code == SI_QUEUE)
		f->si.fsi_code = FREEBSD_SI_QUEUE;
	else if (info->si_code <= 0)
		f->si.fsi_code = FREEBSD_SI_USER;
	else
		f->si.fsi_code = info->si_code;
	switch (lsig) {
	case SIGSEGV: case SIGBUS: case SIGILL: case SIGFPE: case SIGTRAP:
		if (info->si_code > 0)
			f->si.fsi_addr = (unsigned long)info->si_addr;
		break;
	case SIGCHLD:
		f->si.fsi_pid = info->si_pid;
		f->si.fsi_uid = info->si_uid;
		f->si.fsi_status = info->si_code == CLD_EXITED ?
			info->si_status :
			abi_bsd_sig_from_linux(info->si_status);
		break;
	default:
		if (info->si_code <= 0) {
			f->si.fsi_pid = info->si_pid;
			f->si.fsi_uid = info->si_uid;
			f->si.fsi_value = (unsigned long)info->si_ptr;
		}
	}

	/* (sp + 8) must be 16 byte aligned on entry to the handler */
	sp = ((lframe - sizeof(*f)) & ~15UL) - 8;
	if (copy_to_user((void __user *)sp, f, sizeof(*f)))
		goto out;

	regs->sp = sp;
	regs->ip = handler;
	regs->di = f->si.fsi_signo;
	regs->si = sp + offsetof(struct freebsd_sigframe, si);
	regs->dx = sp + offsetof(struct freebsd_sigframe, uc);
	regs->cx = f->si.fsi_addr;
	regs->ax = 0;
	err = 0;
out:
	kfree(lf);
	kfree(f);
	return err;
}

/* Leave the handler through the frame whose ucontext is at @uc. */
static long freebsd_sigreturn_frame(struct abi_ctx *ctx, unsigned long uc)
{
	unsigned long base = uc - offsetof(struct freebsd_sigframe, uc);
	struct freebsd_sigframe *f;
	struct freebsd_mcontext *mc;
	struct ucontext __user *luc;
	struct sigcontext sc;
	long ret = -EFAULT;
	u64 lmask;

	f = kmalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	if (copy_from_user(f, (void __user *)base, sizeof(*f)))
		goto out;
	ret = -EINVAL;
	if (f->magic != FREEBSD_FRAME_MAGIC)
		goto out;

	/* propagate what the handler changed in its ucontext */
	luc = (void __user *)(f->lframe +
			      offsetof(struct abi_linux_sigframe, uc));
	ret = -EFAULT;
	if (copy_from_user(&sc, &luc->uc_mcontext, sizeof(sc)))
		goto out;
	mc = &f->uc.uc_mcontext;
	sc.di = mc->mc_rdi; sc.si = mc->mc_rsi; sc.dx = mc->mc_rdx;
	sc.cx = mc->mc_rcx; sc.r8 = mc->mc_r8; sc.r9 = mc->mc_r9;
	sc.ax = mc->mc_rax; sc.bx = mc->mc_rbx; sc.bp = mc->mc_rbp;
	sc.r10 = mc->mc_r10; sc.r11 = mc->mc_r11; sc.r12 = mc->mc_r12;
	sc.r13 = mc->mc_r13; sc.r14 = mc->mc_r14; sc.r15 = mc->mc_r15;
	sc.ip = mc->mc_rip;
	sc.sp = mc->mc_rsp;
	sc.flags = mc->mc_rflags;	/* sanitised by rt_sigreturn */
	lmask = abi_bsd_sigset_to_linux(f->uc.uc_sigmask);
	if (copy_to_user(&luc->uc_mcontext, &sc, sizeof(sc)) ||
	    put_user(lmask, (u64 __user *)&luc->uc_sigmask))
		goto out;
	ret = abi_sigreturn(ctx, f->lframe);
out:
	kfree(f);
	return ret;
}

/* The handler returned into the trampoline: %rsp is just past retaddr. */
long freebsd_sig_return(struct abi_ctx *ctx)
{
	return freebsd_sigreturn_frame(ctx, ctx->regs->sp);
}

/* sigreturn(const ucontext_t *uc), as called by libthr */
long freebsd_sys_sigreturn(struct abi_ctx *ctx)
{
	long ret = freebsd_sigreturn_frame(ctx, ctx->args[0]);

	if (ret == -EINVAL && abi_trace)
		pr_info("abiemu: freebsd[%d]: sigreturn/setcontext on a context we did not create\n",
			task_pid_nr(current));
	return ret;
}
