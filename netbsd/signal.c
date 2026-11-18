// SPDX-License-Identifier: GPL-2.0
/*
 * abi_netbsd - signals
 */
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm/unistd.h>
#include <asm/sigcontext.h>

#include "netbsd.h"

struct netbsd_sigaction {
	u64	handler;
	u32	sa_mask[4];
	int	sa_flags;
};

/* same bits as FreeBSD */
static const struct abi_flagmap netbsd_saflags[] = {
	{ 0x0001, SA_ONSTACK }, { 0x0002, SA_RESTART },
	{ 0x0004, SA_RESETHAND }, { 0x0008, SA_NOCLDSTOP },
	{ 0x0010, SA_NODEFER }, { 0x0020, SA_NOCLDWAIT },
	{ 0x0040, SA_SIGINFO }, { 0 }
};

static long netbsd_sigaction_post(struct abi_ctx *ctx, long ret)
{
	struct netbsd_sigaction nsa = { 0 };
	struct abi_sigact old;

	if (ret || !ctx->args[2])
		return ret;
	if (abi_sigaction_old(ctx, &old))
		return -EFAULT;
	nsa.handler = old.handler;
	nsa.sa_flags = abi_flags_from_linux(netbsd_saflags, old.flags);
	abi_bsd_sigset_from_linux(old.mask, nsa.sa_mask);
	return copy_to_user((void __user *)ctx->args[2], &nsa, sizeof(nsa)) ?
		-EFAULT : 0;
}

/*
 * __sigaction_sigtramp(int sig, const struct sigaction *nsa,
 *			struct sigaction *osa, const void *tramp, int vers)
 *
 * The trampoline libc passes is not used: handlers return into ours.
 */
long netbsd_sys_sigaction(struct abi_ctx *ctx)
{
	int lsig = abi_bsd_sig_to_linux(ctx->args[0]);
	struct netbsd_sigaction nsa;
	struct abi_sigact act;

	if (lsig <= 0)
		return -EINVAL;
	if (ctx->args[1]) {
		if (copy_from_user(&nsa, (void __user *)ctx->args[1], sizeof(nsa)))
			return -EFAULT;
		act.handler = nsa.handler;
		act.flags = abi_flags_to_linux(netbsd_saflags, nsa.sa_flags);
		act.mask = abi_bsd_sigset_to_linux(nsa.sa_mask);
	}
	ctx->post = netbsd_sigaction_post;
	return abi_sigaction(ctx, lsig, ctx->args[1] ? &act : NULL,
			     ctx->args[2] != 0);
}

long netbsd_sys_sigsuspend(struct abi_ctx *ctx)
{
	u64 __user *gset = abi_gap(ctx, sizeof(u64));
	u32 set[4];

	if (copy_from_user(set, (void __user *)ctx->args[0], sizeof(set)))
		return -EFAULT;
	if (put_user(abi_bsd_sigset_to_linux(set), gset))
		return -EFAULT;
	return abi_native2(ctx, __NR_rt_sigsuspend, (unsigned long)gset, 8);
}

static long netbsd_sigpending_post(struct abi_ctx *ctx, long ret)
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

long netbsd_sys_sigpending(struct abi_ctx *ctx)
{
	void __user *gset = abi_gap(ctx, sizeof(u64));

	ctx->priv[0] = (unsigned long)gset;
	ctx->post = netbsd_sigpending_post;
	return abi_native2(ctx, __NR_rt_sigpending, (unsigned long)gset, 8);
}

/*
 * Signal frames
 */
enum {
	NR_RDI, NR_RSI, NR_RDX, NR_RCX, NR_R8, NR_R9, NR_R10, NR_R11, NR_R12,
	NR_R13, NR_R14, NR_R15, NR_RBP, NR_RBX, NR_RAX, NR_GS, NR_FS, NR_ES,
	NR_DS, NR_TRAPNO, NR_ERR, NR_RIP, NR_CS, NR_RFLAGS, NR_RSP, NR_SS,
	NR_NGREG
};

struct netbsd_ucontext {
	u32	uc_flags;
	u32	pad0;
	u64	uc_link;
	u32	uc_sigmask[4];
	u64	ss_sp;
	u64	ss_size;
	s32	ss_flags;
	u32	pad1;
	u64	gregs[NR_NGREG];
	u64	mc_tlsbase;
	u8	fpregs[512];
};

struct netbsd_siginfo {
	int	nsi_signo, nsi_code, nsi_errno, nsi_pad;
	union {
		struct { s32 pid; u32 uid; u64 value; } rt;
		struct { s32 pid; u32 uid; int status; u64 utime, stime; } child;
		struct { u64 addr; int trap; } fault;
		u8 pad[112];
	};
};

#define NETBSD_UC_SIGMASK	0x01
#define NETBSD_UC_STACK		0x02
#define NETBSD_UC_CPU		0x04
#define NETBSD_SI_USER		0
#define NETBSD_SI_QUEUE		(-1)
#define NETBSD_SS_ONSTACK	1

struct netbsd_sigframe {
	u64			retaddr;
	u64			pad;
	struct netbsd_ucontext	uc;
	struct netbsd_siginfo	si;
	u64			magic;
	u64			lframe;
} __packed;

#define NETBSD_FRAME_MAGIC	0x6162696e65746273ULL

int netbsd_sig_deliver(struct abi_ctx *ctx, int lsig, unsigned long handler,
		       unsigned long lframe)
{
	struct pt_regs *regs = ctx->regs;
	struct abi_linux_sigframe *lf;
	struct netbsd_sigframe *f;
	kernel_siginfo_t *info;
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
	info = (kernel_siginfo_t *)&lf->info;

	f->retaddr = ABI_TRAMP_RETURN;
	f->magic = NETBSD_FRAME_MAGIC;
	f->lframe = lframe;

	f->uc.uc_flags = NETBSD_UC_SIGMASK | NETBSD_UC_STACK | NETBSD_UC_CPU;
	abi_bsd_sigset_from_linux(lf->uc.uc_sigmask.sig[0], f->uc.uc_sigmask);
	f->uc.ss_sp = (unsigned long)lf->uc.uc_stack.ss_sp;
	f->uc.ss_size = lf->uc.uc_stack.ss_size;
	f->uc.ss_flags = (lf->uc.uc_stack.ss_flags & SS_ONSTACK) ?
			 NETBSD_SS_ONSTACK : 0;
	g = f->uc.gregs;
	g[NR_RDI] = sc->di; g[NR_RSI] = sc->si; g[NR_RDX] = sc->dx;
	g[NR_RCX] = sc->cx; g[NR_R8] = sc->r8; g[NR_R9] = sc->r9;
	g[NR_R10] = sc->r10; g[NR_R11] = sc->r11; g[NR_R12] = sc->r12;
	g[NR_R13] = sc->r13; g[NR_R14] = sc->r14; g[NR_R15] = sc->r15;
	g[NR_RBP] = sc->bp; g[NR_RBX] = sc->bx; g[NR_RAX] = sc->ax;
	g[NR_TRAPNO] = sc->trapno; g[NR_ERR] = sc->err; g[NR_RIP] = sc->ip;
	g[NR_CS] = sc->cs; g[NR_RFLAGS] = sc->flags; g[NR_RSP] = sc->sp;
	g[NR_SS] = sc->ss;

	f->si.nsi_signo = abi_bsd_sig_from_linux(lsig);
	f->si.nsi_errno = info->si_errno;
	f->si.nsi_code = info->si_code == SI_QUEUE ? NETBSD_SI_QUEUE :
			 info->si_code <= 0 ? NETBSD_SI_USER : info->si_code;
	switch (lsig) {
	case SIGSEGV: case SIGBUS: case SIGILL: case SIGFPE: case SIGTRAP:
		f->si.fault.addr = (unsigned long)info->si_addr;
		f->si.fault.trap = sc->trapno;
		break;
	case SIGCHLD:
		f->si.child.pid = info->si_pid;
		f->si.child.uid = info->si_uid;
		f->si.child.status = info->si_code == CLD_EXITED ?
			info->si_status : abi_bsd_sig_from_linux(info->si_status);
		break;
	default:
		f->si.rt.pid = info->si_pid;
		f->si.rt.uid = info->si_uid;
		f->si.rt.value = (unsigned long)info->si_ptr;
	}

	sp = ((lframe - sizeof(*f)) & ~15UL) - 8;
	if (copy_to_user((void __user *)sp, f, sizeof(*f)))
		goto out;

	regs->sp = sp;
	regs->ip = handler;
	regs->di = f->si.nsi_signo;
	regs->si = sp + offsetof(struct netbsd_sigframe, si);
	regs->dx = sp + offsetof(struct netbsd_sigframe, uc);
	regs->r15 = regs->dx;
	regs->ax = 0;
	err = 0;
out:
	kfree(lf);
	kfree(f);
	return err;
}

static long netbsd_sigreturn_frame(struct abi_ctx *ctx, unsigned long uc)
{
	unsigned long base = uc - offsetof(struct netbsd_sigframe, uc);
	struct netbsd_sigframe *f;
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
	if (f->magic != NETBSD_FRAME_MAGIC)
		goto out;
	luc = (void __user *)(f->lframe +
			      offsetof(struct abi_linux_sigframe, uc));
	ret = -EFAULT;
	if (copy_from_user(&sc, &luc->uc_mcontext, sizeof(sc)))
		goto out;
	g = f->uc.gregs;
	sc.di = g[NR_RDI]; sc.si = g[NR_RSI]; sc.dx = g[NR_RDX];
	sc.cx = g[NR_RCX]; sc.r8 = g[NR_R8]; sc.r9 = g[NR_R9];
	sc.r10 = g[NR_R10]; sc.r11 = g[NR_R11]; sc.r12 = g[NR_R12];
	sc.r13 = g[NR_R13]; sc.r14 = g[NR_R14]; sc.r15 = g[NR_R15];
	sc.bp = g[NR_RBP]; sc.bx = g[NR_RBX]; sc.ax = g[NR_RAX];
	sc.ip = g[NR_RIP]; sc.sp = g[NR_RSP]; sc.flags = g[NR_RFLAGS];
	if (copy_to_user(&luc->uc_mcontext, &sc, sizeof(sc)) ||
	    put_user(abi_bsd_sigset_to_linux(f->uc.uc_sigmask),
		     (u64 __user *)&luc->uc_sigmask))
		goto out;
	ret = abi_sigreturn(ctx, f->lframe);
out:
	kfree(f);
	return ret;
}

long netbsd_sig_return(struct abi_ctx *ctx)
{
	/* %rsp points at the pad word that follows the return address */
	return netbsd_sigreturn_frame(ctx, ctx->regs->sp + 8);
}

/* setcontext(const ucontext_t *uc): only for contexts of signal frames */
long netbsd_sys_setcontext(struct abi_ctx *ctx)
{
	long ret = netbsd_sigreturn_frame(ctx, ctx->args[0]);

	if (ret == -EINVAL)
		return -ENOSYS;
	return ret;
}
