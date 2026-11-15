// SPDX-License-Identifier: GPL-2.0
/*
 * abi_bsd - calling convention, signal and flag translation shared by
 * the FreeBSD, NetBSD and OpenBSD personalities
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/signal.h>
#include <linux/sched/signal.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/dirent.h>
#include <linux/resource.h>
#include <uapi/linux/wait.h>
#include <asm/unistd.h>
#include <asm/processor-flags.h>

#include <abiemu/bsd.h>

/*
 * Arguments are passed like in the SysV ABI, with %rcx replaced by
 * %r10.  System call 0 (and 198, "__syscall") is the indirect call:
 * the real number is the first argument.
 */
int abi_bsd_decode(struct abi_ctx *ctx)
{
	abi_decode_sysv(ctx);
	if (ctx->nr == 0 || ctx->nr == 198) {
		unsigned long a6 = 0;

		/* the seventh register-less argument lives on the stack */
		if (get_user(a6, (unsigned long __user *)(ctx->regs->sp + 8)))
			a6 = 0;
		ctx->nr = ctx->args[0];
		memmove(&ctx->args[0], &ctx->args[1], 5 * sizeof(long));
		ctx->args[5] = a6;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(abi_bsd_decode);

/* Errors are flagged with carry and a positive errno in %rax. */
void abi_bsd_set_result(struct abi_ctx *ctx, long ret)
{
	struct pt_regs *regs = ctx->regs;

	if (abi_is_error(ret)) {
		int err = abi_errno(ctx->pers, ret);

		/* a Linux errno nobody has a BSD equivalent for */
		if (err == -ret && err >= 35)
			err = EIO;
		regs->ax = err;
		regs->flags |= X86_EFLAGS_CF;
	} else {
		regs->ax = ret;
		regs->flags &= ~X86_EFLAGS_CF;
	}
	/* lets the kernel return with sysret instead of iret */
	regs->r11 = regs->flags;
}
EXPORT_SYMBOL_GPL(abi_bsd_set_result);

/* In the child fork() returns 0 in %rax and 1 in %rdx. */
void abi_bsd_fork_child(struct abi_ctx *parent, struct pt_regs *child)
{
	child->ax = 0;
	child->dx = 1;
	child->flags &= ~X86_EFLAGS_CF;
}
EXPORT_SYMBOL_GPL(abi_bsd_fork_child);

/*
 * Signal numbers.  1-15 agree apart from SIGBUS/SIGEMT/SIGSYS and the
 * user signals; the job control block is shuffled.
 */
static const unsigned char bsd_to_linux_sig[BSD_NSIG + 1] = {
	[1] = SIGHUP, [2] = SIGINT, [3] = SIGQUIT, [4] = SIGILL,
	[5] = SIGTRAP, [6] = SIGABRT, [7] = SIGSTKFLT /* SIGEMT */,
	[8] = SIGFPE, [9] = SIGKILL, [10] = SIGBUS, [11] = SIGSEGV,
	[12] = SIGSYS, [13] = SIGPIPE, [14] = SIGALRM, [15] = SIGTERM,
	[16] = SIGURG, [17] = SIGSTOP, [18] = SIGTSTP, [19] = SIGCONT,
	[20] = SIGCHLD, [21] = SIGTTIN, [22] = SIGTTOU, [23] = SIGIO,
	[24] = SIGXCPU, [25] = SIGXFSZ, [26] = SIGVTALRM, [27] = SIGPROF,
	[28] = SIGWINCH, [29] = SIGPWR /* SIGINFO */, [30] = SIGUSR1,
	[31] = SIGUSR2, [32] = 32,
};

static unsigned char linux_to_bsd_sig[BSD_NSIG + 1];

int abi_bsd_sig_to_linux(int sig)
{
	if (sig < 0)
		return -1;
	if (sig <= BSD_NSIG)
		return bsd_to_linux_sig[sig];
	return sig <= 64 ? sig : -1;
}
EXPORT_SYMBOL_GPL(abi_bsd_sig_to_linux);

int abi_bsd_sig_from_linux(int sig)
{
	if (sig >= 0 && sig <= BSD_NSIG)
		return linux_to_bsd_sig[sig];
	return sig;
}
EXPORT_SYMBOL_GPL(abi_bsd_sig_from_linux);

/* BSD sigsets are arrays of 32 bit words, bit (sig - 1). */
u64 abi_bsd_sigset_to_linux(const u32 *set)
{
	u64 bset = set[0] | ((u64)set[1] << 32), lset = 0;
	int sig;

	for (sig = 1; sig <= 64; sig++) {
		int lsig = abi_bsd_sig_to_linux(sig);

		if ((bset & (1ULL << (sig - 1))) && lsig > 0)
			lset |= 1ULL << (lsig - 1);
	}
	return lset;
}
EXPORT_SYMBOL_GPL(abi_bsd_sigset_to_linux);

void abi_bsd_sigset_from_linux(u64 lset, u32 *set)
{
	u64 bset = 0;
	int sig;

	for (sig = 1; sig <= 64; sig++) {
		int bsig = abi_bsd_sig_from_linux(sig);

		if ((lset & (1ULL << (sig - 1))) && bsig > 0)
			bset |= 1ULL << (bsig - 1);
	}
	set[0] = bset;
	set[1] = bset >> 32;
}
EXPORT_SYMBOL_GPL(abi_bsd_sigset_from_linux);

/* wait(2) status: same encoding, different signal numbers. */
int abi_bsd_wstatus(struct abi_ctx *ctx, int status)
{
	if (status == 0xffff)			/* continued */
		return abi_bsd_ops(ctx)->wcont_status;
	if ((status & 0xff) == 0x7f)		/* stopped */
		return (abi_bsd_sig_from_linux((status >> 8) & 0xff) << 8) | 0x7f;
	if (status & 0x7f)			/* killed */
		return abi_bsd_sig_from_linux(status & 0x7f) | (status & 0x80);
	return status;
}
EXPORT_SYMBOL_GPL(abi_bsd_wstatus);

unsigned int abi_flags_to_linux(const struct abi_flagmap *map, unsigned int f)
{
	unsigned int out = 0;

	for (; map->foreign; map++)
		if ((f & map->foreign) == map->foreign)
			out |= map->native;
	return out;
}
EXPORT_SYMBOL_GPL(abi_flags_to_linux);

unsigned int abi_flags_from_linux(const struct abi_flagmap *map, unsigned int f)
{
	unsigned int out = 0;

	for (; map->foreign; map++)
		if (map->native && (f & map->native) == map->native)
			out |= map->foreign;
	return out;
}
EXPORT_SYMBOL_GPL(abi_flags_from_linux);

long bsd_sys_nosys(struct abi_ctx *ctx)
{
	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(bsd_sys_nosys);

long bsd_sys_zero(struct abi_ctx *ctx)
{
	return 0;
}
EXPORT_SYMBOL_GPL(bsd_sys_zero);

/*
 * Processes
 */
long bsd_sys_exit(struct abi_ctx *ctx)
{
	return abi_native1(ctx, __NR_exit_group, ctx->args[0]);
}
EXPORT_SYMBOL_GPL(bsd_sys_exit);

static long bsd_fork_post(struct abi_ctx *ctx, long ret)
{
	if (!abi_is_error(ret))
		abi_bsd_retval2(ctx, 0);
	return ret;
}

long bsd_sys_fork(struct abi_ctx *ctx)
{
	ctx->post = bsd_fork_post;
	return abi_native0(ctx, __NR_fork);
}
EXPORT_SYMBOL_GPL(bsd_sys_fork);

long bsd_sys_vfork(struct abi_ctx *ctx)
{
	ctx->post = bsd_fork_post;
	return abi_native0(ctx, __NR_vfork);
}
EXPORT_SYMBOL_GPL(bsd_sys_vfork);

static long bsd_wait4_post(struct abi_ctx *ctx, long ret)
{
	int __user *ustatus = (int __user *)ctx->args[1];
	int status;

	if (ret > 0 && ustatus && !get_user(status, ustatus))
		put_user(abi_bsd_wstatus(ctx, status), ustatus);
	return ret;
}

/* wait4(pid, status, options, rusage); struct rusage is the same */
long bsd_sys_wait4(struct abi_ctx *ctx)
{
	unsigned int opt = ctx->args[2], lopt = opt & (WNOHANG | WUNTRACED);

	if (opt & abi_bsd_ops(ctx)->wcontinued)
		lopt |= WCONTINUED;
	ctx->post = bsd_wait4_post;
	return abi_native4(ctx, __NR_wait4, ctx->args[0], ctx->args[1], lopt,
			   ctx->args[3]);
}
EXPORT_SYMBOL_GPL(bsd_sys_wait4);

long bsd_sys_kill(struct abi_ctx *ctx)
{
	int sig = abi_bsd_sig_to_linux(ctx->args[1]);

	if (sig < 0)
		return -EINVAL;
	return abi_native2(ctx, __NR_kill, ctx->args[0], sig);
}
EXPORT_SYMBOL_GPL(bsd_sys_kill);

/*
 * Signal masks: sigprocmask(how, const sigset_t *set, sigset_t *oset)
 * as used by FreeBSD and NetBSD.
 */
static long bsd_sigprocmask_post(struct abi_ctx *ctx, long ret)
{
	u64 lset;
	u32 set[4] = { 0 };

	if (ret || !ctx->args[2])
		return ret;
	if (get_user(lset, (u64 __user *)ctx->priv[0]))
		return -EFAULT;
	abi_bsd_sigset_from_linux(lset, set);
	if (copy_to_user((void __user *)ctx->args[2], set, sizeof(set)))
		return -EFAULT;
	return 0;
}

long bsd_sys_sigprocmask_set(struct abi_ctx *ctx)
{
	int how = ctx->args[0];
	u64 __user *gset = NULL, *goset = NULL;
	u32 set[4];

	if (ctx->args[1]) {
		if (how < 1 || how > 3)
			return -EINVAL;
		if (copy_from_user(set, (void __user *)ctx->args[1], sizeof(set)))
			return -EFAULT;
		gset = abi_gap(ctx, sizeof(u64));
		if (put_user(abi_bsd_sigset_to_linux(set), gset))
			return -EFAULT;
	}
	if (ctx->args[2])
		goset = abi_gap(ctx, sizeof(u64));
	ctx->priv[0] = (unsigned long)goset;
	ctx->post = bsd_sigprocmask_post;
	/* SIG_BLOCK/SIG_UNBLOCK/SIG_SETMASK are 1/2/3 here, 0/1/2 on Linux */
	return abi_native4(ctx, __NR_rt_sigprocmask, how - 1,
			   (unsigned long)gset, (unsigned long)goset, 8);
}
EXPORT_SYMBOL_GPL(bsd_sys_sigprocmask_set);

/* BSD: { ss_sp, ss_size, ss_flags }, Linux: { ss_sp, ss_flags, ss_size } */
struct bsd_stack {
	u64	ss_sp;
	u64	ss_size;
	int	ss_flags;
};
#define BSD_SS_ONSTACK	1
#define BSD_SS_DISABLE	4

static long bsd_sigaltstack_post(struct abi_ctx *ctx, long ret)
{
	struct bsd_stack bss = { 0 };
	stack_t lss;

	if (ret || !ctx->args[1])
		return ret;
	if (copy_from_user(&lss, (void __user *)ctx->priv[0], sizeof(lss)))
		return -EFAULT;
	bss.ss_sp = (unsigned long)lss.ss_sp;
	bss.ss_size = lss.ss_size;
	bss.ss_flags = ((lss.ss_flags & SS_ONSTACK) ? BSD_SS_ONSTACK : 0) |
		       ((lss.ss_flags & SS_DISABLE) ? BSD_SS_DISABLE : 0);
	if (copy_to_user((void __user *)ctx->args[1], &bss, sizeof(bss)))
		return -EFAULT;
	return 0;
}

long bsd_sys_sigaltstack(struct abi_ctx *ctx)
{
	stack_t __user *gss = NULL, *goss = NULL;
	struct bsd_stack bss;
	stack_t lss;

	if (ctx->args[0]) {
		if (copy_from_user(&bss, (void __user *)ctx->args[0], sizeof(bss)))
			return -EFAULT;
		lss.ss_sp = (void __user *)bss.ss_sp;
		lss.ss_size = bss.ss_size;
		lss.ss_flags = (bss.ss_flags & BSD_SS_DISABLE) ? SS_DISABLE : 0;
		gss = abi_gap(ctx, sizeof(lss));
		if (copy_to_user(gss, &lss, sizeof(lss)))
			return -EFAULT;
	}
	if (ctx->args[1])
		goss = abi_gap(ctx, sizeof(lss));
	ctx->priv[0] = (unsigned long)goss;
	ctx->post = bsd_sigaltstack_post;
	return abi_native2(ctx, __NR_sigaltstack, (unsigned long)gss,
			   (unsigned long)goss);
}
EXPORT_SYMBOL_GPL(bsd_sys_sigaltstack);

/*
 * Resource limits.  The structure is the same, the numbering of the
 * resources and the value of RLIM_INFINITY are not.
 */
#define BSD_RLIM_INFINITY	0x7fffffffffffffffULL

static int bsd_rlimit_to_linux(unsigned int res)
{
	static const signed char map[] = {
		RLIMIT_CPU, RLIMIT_FSIZE, RLIMIT_DATA, RLIMIT_STACK,
		RLIMIT_CORE, RLIMIT_RSS, RLIMIT_MEMLOCK, RLIMIT_NPROC,
		RLIMIT_NOFILE, -1 /* SBSIZE */, RLIMIT_AS,
	};

	return res < ARRAY_SIZE(map) ? map[res] : -1;
}

static long bsd_getrlimit_post(struct abi_ctx *ctx, long ret)
{
	struct rlimit __user *urlim = (void __user *)ctx->args[1];
	struct rlimit rlim;

	if (ret)
		return ret;
	if (copy_from_user(&rlim, urlim, sizeof(rlim)))
		return -EFAULT;
	if (rlim.rlim_cur > BSD_RLIM_INFINITY)
		rlim.rlim_cur = BSD_RLIM_INFINITY;
	if (rlim.rlim_max > BSD_RLIM_INFINITY)
		rlim.rlim_max = BSD_RLIM_INFINITY;
	return copy_to_user(urlim, &rlim, sizeof(rlim)) ? -EFAULT : 0;
}

long bsd_sys_getrlimit(struct abi_ctx *ctx)
{
	int res = bsd_rlimit_to_linux(ctx->args[0]);

	if (res < 0) {
		struct rlimit inf = { BSD_RLIM_INFINITY, BSD_RLIM_INFINITY };

		if (copy_to_user((void __user *)ctx->args[1], &inf, sizeof(inf)))
			return -EFAULT;
		return 0;
	}
	ctx->post = bsd_getrlimit_post;
	return abi_native2(ctx, __NR_getrlimit, res, ctx->args[1]);
}
EXPORT_SYMBOL_GPL(bsd_sys_getrlimit);

long bsd_sys_setrlimit(struct abi_ctx *ctx)
{
	int res = bsd_rlimit_to_linux(ctx->args[0]);
	struct rlimit __user *grlim;
	struct rlimit rlim;

	if (res < 0)
		return 0;
	if (copy_from_user(&rlim, (void __user *)ctx->args[1], sizeof(rlim)))
		return -EFAULT;
	if (rlim.rlim_cur >= BSD_RLIM_INFINITY)
		rlim.rlim_cur = RLIM_INFINITY;
	if (rlim.rlim_max >= BSD_RLIM_INFINITY)
		rlim.rlim_max = RLIM_INFINITY;
	grlim = abi_gap(ctx, sizeof(rlim));
	if (copy_to_user(grlim, &rlim, sizeof(rlim)))
		return -EFAULT;
	return abi_native2(ctx, __NR_setrlimit, res, (unsigned long)grlim);
}
EXPORT_SYMBOL_GPL(bsd_sys_setrlimit);

long bsd_sys_getdtablesize(struct abi_ctx *ctx)
{
	return min_t(unsigned long, rlimit(RLIMIT_NOFILE), INT_MAX);
}
EXPORT_SYMBOL_GPL(bsd_sys_getdtablesize);

long bsd_sys_getlogin(struct abi_ctx *ctx)
{
	const char *name = uid_eq(current_uid(), GLOBAL_ROOT_UID) ? "root" : "user";
	size_t len = strlen(name) + 1;

	if (ctx->args[1] < len)
		return -ERANGE;
	return copy_to_user((void __user *)ctx->args[0], name, len) ? -EFAULT : 0;
}
EXPORT_SYMBOL_GPL(bsd_sys_getlogin);

/*
 * Memory
 */
long bsd_sys_madvise(struct abi_ctx *ctx)
{
	/* 0-4 (NORMAL..DONTNEED) agree; the rest are hints we may ignore */
	if (ctx->args[2] > MADV_DONTNEED)
		return 0;
	return abi_native3(ctx, __NR_madvise, ctx->args[0], ctx->args[1],
			   ctx->args[2]);
}
EXPORT_SYMBOL_GPL(bsd_sys_madvise);

long abi_bsd_mmap(unsigned long addr, unsigned long len, int prot,
		  unsigned long lflags, int fd, loff_t off,
		  unsigned long align)
{
	struct file *file = NULL;
	unsigned long ret;

	if (!len)
		return -EINVAL;
	if (!(lflags & MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			return -EBADF;
	}

	if (align > PAGE_SIZE && !(lflags & MAP_FIXED)) {
		unsigned long base, start, span = PAGE_ALIGN(len) + align;

		/* reserve an oversized window and trim it afterwards */
		base = vm_mmap(NULL, 0, span, PROT_NONE,
			       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, 0);
		ret = base;
		if (IS_ERR_VALUE(base))
			goto out;
		start = ALIGN(base, align);
		ret = vm_mmap(file, start, len, prot, lflags | MAP_FIXED, off);
		if (IS_ERR_VALUE(ret)) {
			vm_munmap(base, span);
			goto out;
		}
		if (start > base)
			vm_munmap(base, start - base);
		if (base + span > start + PAGE_ALIGN(len))
			vm_munmap(start + PAGE_ALIGN(len),
				  base + span - start - PAGE_ALIGN(len));
	} else {
		ret = vm_mmap(file, addr, len, prot, lflags, off);
	}
out:
	if (file)
		fput(file);
	return ret;
}
EXPORT_SYMBOL_GPL(abi_bsd_mmap);

static int __init abi_bsd_init(void)
{
	int sig;

	for (sig = 1; sig <= BSD_NSIG; sig++)
		if (bsd_to_linux_sig[sig] <= BSD_NSIG)
			linux_to_bsd_sig[bsd_to_linux_sig[sig]] = sig;
	return 0;
}

static void __exit abi_bsd_exit(void)
{
}

module_init(abi_bsd_init);
module_exit(abi_bsd_exit);

MODULE_DESCRIPTION("abiemu: support code shared by the BSD personalities");
MODULE_LICENSE("GPL");
