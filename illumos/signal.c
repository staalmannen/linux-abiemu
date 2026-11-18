// SPDX-License-Identifier: GPL-2.0
/*
 * abi_illumos - signals and waiting for children
 */
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <uapi/linux/wait.h>
#include <asm/unistd.h>
#include <asm/sigcontext.h>

#include "illumos.h"

#define IL_NSIG		74	/* incl. real time signals 42..73 */
#define IL_SIGRTMIN	42

static const unsigned char il_to_linux[42] = {
	[1] = SIGHUP, [2] = SIGINT, [3] = SIGQUIT, [4] = SIGILL,
	[5] = SIGTRAP, [6] = SIGABRT, [7] = SIGSTKFLT /* SIGEMT */,
	[8] = SIGFPE, [9] = SIGKILL, [10] = SIGBUS, [11] = SIGSEGV,
	[12] = SIGSYS, [13] = SIGPIPE, [14] = SIGALRM, [15] = SIGTERM,
	[16] = SIGUSR1, [17] = SIGUSR2, [18] = SIGCHLD, [19] = SIGPWR,
	[20] = SIGWINCH, [21] = SIGURG, [22] = SIGIO, [23] = SIGSTOP,
	[24] = SIGTSTP, [25] = SIGCONT, [26] = SIGTTIN, [27] = SIGTTOU,
	[28] = SIGVTALRM, [29] = SIGPROF, [30] = SIGXCPU, [31] = SIGXFSZ,
};

int illumos_sig_to_linux(int sig)
{
	if (sig >= 0 && sig < IL_SIGRTMIN)
		return sig ? (il_to_linux[sig] ?: -1) : 0;
	if (sig < IL_NSIG && SIGRTMIN + 2 + sig - IL_SIGRTMIN <= _NSIG)
		return SIGRTMIN + 2 + sig - IL_SIGRTMIN;
	return -1;
}

int illumos_sig_from_linux(int lsig)
{
	int sig;

	if (lsig >= SIGRTMIN + 2)
		return IL_SIGRTMIN + lsig - SIGRTMIN - 2;
	for (sig = 1; sig < IL_SIGRTMIN; sig++)
		if (il_to_linux[sig] == lsig)
			return sig;
	return 0;
}

u64 illumos_sigset_to_linux(const u32 *set)
{
	u64 lset = 0;
	int sig;

	for (sig = 1; sig < IL_NSIG; sig++) {
		int lsig = illumos_sig_to_linux(sig);

		if ((set[(sig - 1) / 32] & (1U << ((sig - 1) % 32))) && lsig > 0)
			lset |= 1ULL << (lsig - 1);
	}
	return lset;
}

void illumos_sigset_from_linux(u64 lset, u32 *set)
{
	int lsig;

	memset(set, 0, 4 * sizeof(u32));
	for (lsig = 1; lsig <= 64; lsig++) {
		int sig = illumos_sig_from_linux(lsig);

		if ((lset & (1ULL << (lsig - 1))) && sig > 0 && sig < IL_NSIG)
			set[(sig - 1) / 32] |= 1U << ((sig - 1) % 32);
	}
}

long illumos_sys_kill(struct abi_ctx *ctx)
{
	int sig = illumos_sig_to_linux(ctx->args[1]);

	if (sig < 0)
		return -EINVAL;
	return abi_native2(ctx, __NR_kill, ctx->args[0], sig);
}

/* lwp_kill(lwpid, sig): lwp 1 is the main thread */
long illumos_sys_lwp_kill(struct abi_ctx *ctx)
{
	int sig = illumos_sig_to_linux(ctx->args[1]);
	pid_t tid = ctx->args[0] == 1 ? task_tgid_vnr(current) : ctx->args[0];

	if (sig < 0)
		return -EINVAL;
	return abi_native2(ctx, __NR_tkill, tid, sig);
}

struct illumos_sigaction {
	int	sa_flags;
	u64	handler;
	u32	sa_mask[4];
	int	sa_resv[2];
};

static const struct {
	unsigned int il, lx;
} il_saflags[] = {
	{ 0x00001, SA_ONSTACK }, { 0x00002, SA_RESETHAND },
	{ 0x00004, SA_RESTART }, { 0x00008, SA_SIGINFO },
	{ 0x00010, SA_NODEFER }, { 0x10000, SA_NOCLDWAIT },
	{ 0x20000, SA_NOCLDSTOP },
};

static unsigned int il_saflags_conv(unsigned int f, bool to_linux)
{
	unsigned int out = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(il_saflags); i++)
		if (f & (to_linux ? il_saflags[i].il : il_saflags[i].lx))
			out |= to_linux ? il_saflags[i].lx : il_saflags[i].il;
	return out;
}

static long illumos_sigaction_post(struct abi_ctx *ctx, long ret)
{
	struct illumos_sigaction isa = { 0 };
	struct abi_sigact old;

	if (ret || !ctx->args[2])
		return ret;
	if (abi_sigaction_old(ctx, &old))
		return -EFAULT;
	isa.handler = old.handler;
	isa.sa_flags = il_saflags_conv(old.flags, false);
	illumos_sigset_from_linux(old.mask, isa.sa_mask);
	return copy_to_user((void __user *)ctx->args[2], &isa, sizeof(isa)) ?
		-EFAULT : 0;
}

long illumos_sys_sigaction(struct abi_ctx *ctx)
{
	int lsig = illumos_sig_to_linux(ctx->args[0]);
	struct illumos_sigaction isa;
	struct abi_sigact act;

	if (lsig <= 0)
		return -EINVAL;
	if (ctx->args[1]) {
		if (copy_from_user(&isa, (void __user *)ctx->args[1], sizeof(isa)))
			return -EFAULT;
		act.handler = isa.handler;
		act.flags = il_saflags_conv(isa.sa_flags, true);
		act.mask = illumos_sigset_to_linux(isa.sa_mask);
	}
	ctx->post = illumos_sigaction_post;
	return abi_sigaction(ctx, lsig, ctx->args[1] ? &act : NULL,
			     ctx->args[2] != 0);
}

static long illumos_sigprocmask_post(struct abi_ctx *ctx, long ret)
{
	u32 set[4];
	u64 lset;

	if (ret || !ctx->priv[1])
		return ret;
	if (get_user(lset, (u64 __user *)ctx->priv[0]))
		return -EFAULT;
	illumos_sigset_from_linux(lset, set);
	if (ctx->priv[1] == 1)		/* sigprocmask: to user memory */
		return copy_to_user((void __user *)ctx->args[2], set,
				    sizeof(set)) ? -EFAULT : 0;
	illumos_retval2(ctx, set[1]);	/* lwp_sigmask: in registers */
	return set[0];
}

static long illumos_do_sigmask(struct abi_ctx *ctx, int how, const u32 *set,
			       int ret_how)
{
	u64 __user *gset = NULL, *gold = abi_gap(ctx, sizeof(u64));

	if (set) {
		if (how < 1 || how > 3)
			return -EINVAL;
		gset = abi_gap(ctx, sizeof(u64));
		if (put_user(illumos_sigset_to_linux(set), gset))
			return -EFAULT;
	}
	ctx->priv[0] = (unsigned long)gold;
	ctx->priv[1] = ret_how;
	ctx->post = illumos_sigprocmask_post;
	return abi_native4(ctx, __NR_rt_sigprocmask, set ? how - 1 : 0,
			   (unsigned long)gset, (unsigned long)gold, 8);
}

/* sigprocmask(how, const sigset_t *set, sigset_t *oset) */
long illumos_sys_sigprocmask(struct abi_ctx *ctx)
{
	u32 set[4];

	if (ctx->args[1] &&
	    copy_from_user(set, (void __user *)ctx->args[1], sizeof(set)))
		return -EFAULT;
	return illumos_do_sigmask(ctx, ctx->args[0],
				  ctx->args[1] ? set : NULL,
				  ctx->args[2] ? 1 : 0);
}

/* lwp_sigmask(how, bits0, bits1, bits2): old bits0/bits1 in rax/rdx */
long illumos_sys_lwp_sigmask(struct abi_ctx *ctx)
{
	u32 set[4] = { ctx->args[1], ctx->args[2], ctx->args[3], 0 };

	return illumos_do_sigmask(ctx, ctx->args[0], set, 2);
}

long illumos_sys_sigsuspend(struct abi_ctx *ctx)
{
	u64 __user *gset = abi_gap(ctx, sizeof(u64));
	u32 set[4];

	if (copy_from_user(set, (void __user *)ctx->args[0], sizeof(set)))
		return -EFAULT;
	if (put_user(illumos_sigset_to_linux(set), gset))
		return -EFAULT;
	return abi_native2(ctx, __NR_rt_sigsuspend, (unsigned long)gset, 8);
}

static long illumos_sigpending_post(struct abi_ctx *ctx, long ret)
{
	u32 set[4];
	u64 lset;

	if (ret)
		return ret;
	if (get_user(lset, (u64 __user *)ctx->priv[0]))
		return -EFAULT;
	illumos_sigset_from_linux(lset, set);
	return copy_to_user((void __user *)ctx->args[1], set, sizeof(set)) ?
		-EFAULT : 0;
}

/* sigpending(int flag, sigset_t *set): flag 1 = pending, 2 = fillset */
long illumos_sys_sigpending(struct abi_ctx *ctx)
{
	void __user *g = abi_gap(ctx, sizeof(u64));

	if (ctx->args[0] == 2) {
		u32 all[4] = { ~0U, ~0U, (1U << (IL_NSIG - 65)) - 1, 0 };

		return copy_to_user((void __user *)ctx->args[1], all,
				    sizeof(all)) ? -EFAULT : 0;
	}
	ctx->priv[0] = (unsigned long)g;
	ctx->post = illumos_sigpending_post;
	return abi_native2(ctx, __NR_rt_sigpending, (unsigned long)g, 8);
}

/* stack_t is { sp, size, flags } with SS_ONSTACK 1, SS_DISABLE 2 */
struct illumos_stack {
	u64	ss_sp;
	u64	ss_size;
	int	ss_flags;
};

static long illumos_sigaltstack_post(struct abi_ctx *ctx, long ret)
{
	struct illumos_stack iss = { 0 };
	stack_t lss;

	if (ret || !ctx->args[1])
		return ret;
	if (copy_from_user(&lss, (void __user *)ctx->priv[0], sizeof(lss)))
		return -EFAULT;
	iss.ss_sp = (unsigned long)lss.ss_sp;
	iss.ss_size = lss.ss_size;
	iss.ss_flags = ((lss.ss_flags & SS_ONSTACK) ? 1 : 0) |
		       ((lss.ss_flags & SS_DISABLE) ? 2 : 0);
	return copy_to_user((void __user *)ctx->args[1], &iss, sizeof(iss)) ?
		-EFAULT : 0;
}

long illumos_sys_sigaltstack(struct abi_ctx *ctx)
{
	stack_t __user *gss = NULL, *goss = NULL;
	struct illumos_stack iss;
	stack_t lss;

	if (ctx->args[0]) {
		if (copy_from_user(&iss, (void __user *)ctx->args[0], sizeof(iss)))
			return -EFAULT;
		lss.ss_sp = (void __user *)iss.ss_sp;
		lss.ss_size = iss.ss_size;
		lss.ss_flags = (iss.ss_flags & 2) ? SS_DISABLE : 0;
		gss = abi_gap(ctx, sizeof(lss));
		if (copy_to_user(gss, &lss, sizeof(lss)))
			return -EFAULT;
	}
	if (ctx->args[1])
		goss = abi_gap(ctx, sizeof(lss));
	ctx->priv[0] = (unsigned long)goss;
	ctx->post = illumos_sigaltstack_post;
	return abi_native2(ctx, __NR_sigaltstack, (unsigned long)gss,
			   (unsigned long)goss);
}

/*
 * siginfo_t and waitsys()
 */
struct illumos_siginfo {
	int	il_signo, il_code, il_errno, il_pad;
	union {
		struct { s32 pid; u32 uid; u64 value; } kill;
		struct { s32 pid; s32 ctid; s64 utime; int status; s64 stime; } cld;
		struct { u64 addr; int trapno; } fault;
		u8 _pad[240];
	};
};

static void illumos_siginfo_from_linux(const kernel_siginfo_t *info,
				       struct illumos_siginfo *si, int lsig,
				       int trapno)
{
	memset(si, 0, sizeof(*si));
	si->il_signo = illumos_sig_from_linux(lsig);
	si->il_errno = info->si_errno;
	si->il_code = info->si_code;
	switch (lsig) {
	case SIGSEGV: case SIGBUS: case SIGILL: case SIGFPE: case SIGTRAP:
		si->fault.addr = (unsigned long)info->si_addr;
		si->fault.trapno = trapno;
		break;
	case SIGCHLD:
		si->cld.pid = info->si_pid;
		si->cld.status = info->si_code == CLD_EXITED ? info->si_status :
				 illumos_sig_from_linux(info->si_status);
		si->cld.utime = info->si_utime;
		si->cld.stime = info->si_stime;
		break;
	default:
		si->kill.pid = info->si_pid;
		si->kill.uid = info->si_uid;
		si->kill.value = (unsigned long)info->si_ptr;
	}
}

static long illumos_waitsys_post(struct abi_ctx *ctx, long ret)
{
	struct illumos_siginfo *si;
	kernel_siginfo_t info;
	struct siginfo lsi;

	if (ret || !ctx->args[2])
		return ret;
	if (copy_from_user(&lsi, (void __user *)ctx->priv[0], sizeof(lsi)))
		return -EFAULT;
	si = kmalloc(sizeof(*si), GFP_KERNEL);
	if (!si)
		return -ENOMEM;
	memcpy(&info, &lsi, sizeof(info));
	if (info.si_pid == 0)		/* WNOHANG and nobody ready */
		memset(si, 0, sizeof(*si));
	else
		illumos_siginfo_from_linux(&info, si, SIGCHLD, 0);
	ret = copy_to_user((void __user *)ctx->args[2], si, sizeof(*si)) ?
		-EFAULT : 0;
	kfree(si);
	return ret;
}

/* waitsys(idtype_t type, id_t id, siginfo_t *info, int options) */
long illumos_sys_waitsys(struct abi_ctx *ctx)
{
	unsigned int opt = ctx->args[3], lopt = 0;
	void __user *g = abi_gap(ctx, sizeof(struct siginfo));
	int type;

	switch ((int)ctx->args[0]) {
	case 0:		type = P_PID; break;
	case 2:		type = P_PGID; break;
	case 7:		type = P_ALL; break;
	default:	return -EINVAL;
	}
	if (opt & 0x01)	lopt |= WEXITED;
	if (opt & 0x02)	lopt |= WSTOPPED;	/* WTRAPPED */
	if (opt & 0x04)	lopt |= WSTOPPED;
	if (opt & 0x08)	lopt |= WCONTINUED;
	if (opt & 0x40)	lopt |= WNOHANG;
	if (opt & 0x80)	lopt |= WNOWAIT;
	if (clear_user(g, sizeof(struct siginfo)))
		return -EFAULT;
	ctx->priv[0] = (unsigned long)g;
	ctx->post = illumos_waitsys_post;
	return abi_native5(ctx, __NR_waitid, type, ctx->args[1],
			   (unsigned long)g, lopt, 0);
}

/*
 * Signal frames: handler(sig, siginfo_t *, ucontext_t *)
 */
enum {
	IR_R15, IR_R14, IR_R13, IR_R12, IR_R11, IR_R10, IR_R9, IR_R8,
	IR_RDI, IR_RSI, IR_RBP, IR_RBX, IR_RDX, IR_RCX, IR_RAX, IR_TRAPNO,
	IR_ERR, IR_RIP, IR_CS, IR_RFL, IR_RSP, IR_SS, IR_FS, IR_GS, IR_ES,
	IR_DS, IR_FSBASE, IR_GSBASE, IR_NGREG
};

struct illumos_ucontext {
	u64	uc_flags;
	u64	uc_link;
	u32	uc_sigmask[4];
	u64	ss_sp;
	u64	ss_size;
	s32	ss_flags;
	u32	pad;
	u64	gregs[IR_NGREG];
	u8	fpregs[512];		/* fxsave image */
	u32	fp_status, fp_xstatus;
	u64	uc_xrs[2];
	u64	uc_filler[3];
} __aligned(16);

#define UC_SIGMASK	0x01
#define UC_STACK	0x02
#define UC_CPU		0x04

struct illumos_sigframe {
	u64			retaddr;
	u64			pad;
	struct illumos_ucontext	uc;
	struct illumos_siginfo	si;
	u64			magic;
	u64			lframe;
};

#define ILLUMOS_FRAME_MAGIC	0x616269696c6c756dULL

int illumos_sig_deliver(struct abi_ctx *ctx, int lsig, unsigned long handler,
			unsigned long lframe)
{
	struct pt_regs *regs = ctx->regs;
	struct abi_linux_sigframe *lf;
	struct illumos_sigframe *f;
	struct sigcontext *sc;
	unsigned long sp;
	u64 *g;
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

	f->retaddr = ABI_TRAMP_RETURN;
	f->magic = ILLUMOS_FRAME_MAGIC;
	f->lframe = lframe;
	f->uc.uc_flags = UC_SIGMASK | UC_STACK | UC_CPU;
	illumos_sigset_from_linux(lf->uc.uc_sigmask.sig[0], f->uc.uc_sigmask);
	f->uc.ss_sp = (unsigned long)lf->uc.uc_stack.ss_sp;
	f->uc.ss_size = lf->uc.uc_stack.ss_size;
	f->uc.ss_flags = (lf->uc.uc_stack.ss_flags & SS_ONSTACK) ? 1 : 0;
	g = f->uc.gregs;
	g[IR_R15] = sc->r15; g[IR_R14] = sc->r14; g[IR_R13] = sc->r13;
	g[IR_R12] = sc->r12; g[IR_R11] = sc->r11; g[IR_R10] = sc->r10;
	g[IR_R9] = sc->r9; g[IR_R8] = sc->r8; g[IR_RDI] = sc->di;
	g[IR_RSI] = sc->si; g[IR_RBP] = sc->bp; g[IR_RBX] = sc->bx;
	g[IR_RDX] = sc->dx; g[IR_RCX] = sc->cx; g[IR_RAX] = sc->ax;
	g[IR_TRAPNO] = sc->trapno; g[IR_ERR] = sc->err; g[IR_RIP] = sc->ip;
	g[IR_CS] = sc->cs; g[IR_RFL] = sc->flags; g[IR_RSP] = sc->sp;
	g[IR_SS] = sc->ss;
	illumos_siginfo_from_linux((kernel_siginfo_t *)&lf->info, &f->si,
				   lsig, sc->trapno);

	sp = ((lframe - sizeof(*f)) & ~15UL) - 8;
	if (copy_to_user((void __user *)sp, f, sizeof(*f)))
		goto out;
	regs->sp = sp;
	regs->ip = handler;
	regs->di = f->si.il_signo;
	regs->si = sp + offsetof(struct illumos_sigframe, si);
	regs->dx = sp + offsetof(struct illumos_sigframe, uc);
	regs->ax = 0;
	err = 0;
out:
	kfree(lf);
	kfree(f);
	return err;
}

static long illumos_sigreturn_frame(struct abi_ctx *ctx, unsigned long uc)
{
	unsigned long base = uc - offsetof(struct illumos_sigframe, uc);
	struct illumos_sigframe *f;
	struct ucontext __user *luc;
	struct sigcontext sc;
	long ret = -EFAULT;
	u64 *g;

	f = kmalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	if (copy_from_user(f, (void __user *)base, sizeof(*f)))
		goto out;
	ret = -EINVAL;
	if (f->magic != ILLUMOS_FRAME_MAGIC)
		goto out;
	luc = (void __user *)(f->lframe +
			      offsetof(struct abi_linux_sigframe, uc));
	ret = -EFAULT;
	if (copy_from_user(&sc, &luc->uc_mcontext, sizeof(sc)))
		goto out;
	g = f->uc.gregs;
	sc.r15 = g[IR_R15]; sc.r14 = g[IR_R14]; sc.r13 = g[IR_R13];
	sc.r12 = g[IR_R12]; sc.r11 = g[IR_R11]; sc.r10 = g[IR_R10];
	sc.r9 = g[IR_R9]; sc.r8 = g[IR_R8]; sc.di = g[IR_RDI];
	sc.si = g[IR_RSI]; sc.bp = g[IR_RBP]; sc.bx = g[IR_RBX];
	sc.dx = g[IR_RDX]; sc.cx = g[IR_RCX]; sc.ax = g[IR_RAX];
	sc.ip = g[IR_RIP]; sc.sp = g[IR_RSP]; sc.flags = g[IR_RFL];
	if (copy_to_user(&luc->uc_mcontext, &sc, sizeof(sc)) ||
	    put_user(illumos_sigset_to_linux(f->uc.uc_sigmask),
		     (u64 __user *)&luc->uc_sigmask))
		goto out;
	ret = abi_sigreturn(ctx, f->lframe);
out:
	kfree(f);
	return ret;
}

long illumos_sig_return(struct abi_ctx *ctx)
{
	/* %rsp points at the pad word that follows the return address */
	return illumos_sigreturn_frame(ctx, ctx->regs->sp + 8);
}

/*
 * context(int flag, ...): GETCONTEXT 0, SETCONTEXT 1, GETUSTACK 2,
 * SETUSTACK 3.  libc's signal wrapper leaves a handler through
 * setcontext() on the ucontext it was given; that is the case we
 * support.
 */
long illumos_sys_context(struct abi_ctx *ctx)
{
	long ret;

	switch ((int)ctx->args[0]) {
	case 1:
		ret = illumos_sigreturn_frame(ctx, ctx->args[1]);
		return ret == -EINVAL ? -ENOSYS : ret;
	case 2:		/* no user level thread stack recorded */
		return put_user(0UL, (unsigned long __user *)ctx->args[1]) ?
			-EFAULT : 0;
	case 3:
		return 0;
	}
	return -ENOSYS;
}
