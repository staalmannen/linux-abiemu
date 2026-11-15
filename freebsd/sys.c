// SPDX-License-Identifier: GPL-2.0
/*
 * abi_freebsd - system call table and FreeBSD specific calls
 */
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/statfs.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/cpumask.h>
#include <linux/cred.h>
#include <linux/kdev_t.h>
#include <linux/time.h>
#include <asm/unistd.h>
#include <asm/prctl.h>

#include "freebsd.h"

/*
 * Files
 */
struct freebsd_stat {
	u64	st_dev;
	u64	st_ino;
	u64	st_nlink;
	u16	st_mode;
	s16	st_padding0;
	u32	st_uid;
	u32	st_gid;
	s32	st_padding1;
	u64	st_rdev;
	struct __kernel_timespec st_atim, st_mtim, st_ctim, st_birthtim;
	s64	st_size;
	s64	st_blocks;
	s32	st_blksize;
	u32	st_flags;
	u64	st_gen;
	u64	st_spare[10];
};

static long freebsd_put_stat(const struct kstat *k, void __user *ubuf)
{
	struct freebsd_stat st = {
		.st_dev = abi_bsd_mkdev(MAJOR(k->dev), MINOR(k->dev)),
		.st_ino = k->ino,
		.st_nlink = k->nlink,
		.st_mode = k->mode,
		.st_uid = from_kuid_munged(current_user_ns(), k->uid),
		.st_gid = from_kgid_munged(current_user_ns(), k->gid),
		.st_rdev = abi_bsd_mkdev(MAJOR(k->rdev), MINOR(k->rdev)),
		.st_atim = { k->atime.tv_sec, k->atime.tv_nsec },
		.st_mtim = { k->mtime.tv_sec, k->mtime.tv_nsec },
		.st_ctim = { k->ctime.tv_sec, k->ctime.tv_nsec },
		.st_birthtim = { k->btime.tv_sec, k->btime.tv_nsec },
		.st_size = k->size,
		.st_blocks = k->blocks,
		.st_blksize = k->blksize,
	};

	if (!(k->result_mask & STATX_BTIME))
		st.st_birthtim = st.st_ctim;
	return copy_to_user(ubuf, &st, sizeof(st)) ? -EFAULT : 0;
}

/* fstat(int fd, struct stat *sb) */
static long freebsd_sys_fstat(struct abi_ctx *ctx)
{
	struct kstat k;
	int err = abi_stat_fd(ctx->args[0], &k);

	return err ?: freebsd_put_stat(&k, (void __user *)ctx->args[1]);
}

/* fstatat(int fd, const char *path, struct stat *buf, int flag) */
static long freebsd_sys_fstatat(struct abi_ctx *ctx)
{
	unsigned int flag = ctx->args[3];
	struct kstat k;
	int err;

	err = abi_stat_path(ctx->args[0], (void __user *)ctx->args[1],
			    !(flag & 0x200), flag & 0x4000, &k);
	return err ?: freebsd_put_stat(&k, (void __user *)ctx->args[2]);
}

/* getdirentries(int fd, char *buf, size_t count, off_t *basep) */
static long freebsd_sys_getdirentries(struct abi_ctx *ctx)
{
	loff_t base = 0;
	long ret;

	ret = abi_getdents(ctx->args[0], (void __user *)ctx->args[1],
			   ctx->args[2], &base, abi_bsd_emit_dirent_freebsd);
	if (ret >= 0 && ctx->args[3] &&
	    put_user(base, (s64 __user *)ctx->args[3]))
		return -EFAULT;
	return ret;
}

struct freebsd_statfs {
	u32	f_version;
	u32	f_type;
	u64	f_flags;
	u64	f_bsize;
	u64	f_iosize;
	u64	f_blocks;
	u64	f_bfree;
	s64	f_bavail;
	u64	f_files;
	s64	f_ffree;
	u64	f_syncwrites, f_asyncwrites, f_syncreads, f_asyncreads;
	u32	f_nvnodelistsize;
	u32	f_spare0;
	u64	f_spare[9];
	u32	f_namemax;
	u32	f_owner;
	s32	f_fsid[2];
	char	f_charspare[80];
	char	f_fstypename[16];
	char	f_mntfromname[1024];
	char	f_mntonname[1024];
};

static long freebsd_statfs_post(struct abi_ctx *ctx, long ret)
{
	struct freebsd_statfs *fs;
	struct statfs ls;

	if (ret)
		return ret;
	if (copy_from_user(&ls, (void __user *)ctx->priv[0], sizeof(ls)))
		return -EFAULT;
	fs = kzalloc(sizeof(*fs), GFP_KERNEL);
	if (!fs)
		return -ENOMEM;
	fs->f_version = 0x20140518;
	fs->f_bsize = ls.f_frsize ?: ls.f_bsize;
	fs->f_iosize = ls.f_bsize;
	fs->f_blocks = ls.f_blocks;
	fs->f_bfree = ls.f_bfree;
	fs->f_bavail = ls.f_bavail;
	fs->f_files = ls.f_files;
	fs->f_ffree = ls.f_ffree;
	fs->f_namemax = ls.f_namelen;
	fs->f_fsid[0] = ls.f_fsid.val[0];
	fs->f_fsid[1] = ls.f_fsid.val[1];
	strscpy(fs->f_fstypename, "ufs", sizeof(fs->f_fstypename));
	strscpy(fs->f_mntfromname, "linux", sizeof(fs->f_mntfromname));
	strscpy(fs->f_mntonname, "/", sizeof(fs->f_mntonname));
	if (copy_to_user((void __user *)ctx->args[1], fs, sizeof(*fs)))
		ret = -EFAULT;
	kfree(fs);
	return ret;
}

static long freebsd_sys_statfs(struct abi_ctx *ctx)
{
	void __user *gap = abi_gap(ctx, sizeof(struct statfs));

	ctx->priv[0] = (unsigned long)gap;
	ctx->post = freebsd_statfs_post;
	return abi_native2(ctx, __NR_statfs, ctx->args[0], (unsigned long)gap);
}

static long freebsd_sys_fstatfs(struct abi_ctx *ctx)
{
	void __user *gap = abi_gap(ctx, sizeof(struct statfs));

	ctx->priv[0] = (unsigned long)gap;
	ctx->post = freebsd_statfs_post;
	return abi_native2(ctx, __NR_fstatfs, ctx->args[0], (unsigned long)gap);
}

/* F_DUP2FD is FreeBSD's own; record locks are not translated yet. */
static long freebsd_sys_fcntl(struct abi_ctx *ctx)
{
	switch ((int)ctx->args[1]) {
	case 10:	/* F_DUP2FD */
		return abi_native2(ctx, __NR_dup2, ctx->args[0], ctx->args[2]);
	case 18:	/* F_DUP2FD_CLOEXEC */
		return abi_native3(ctx, __NR_dup3, ctx->args[0], ctx->args[2],
				   O_CLOEXEC);
	}
	return bsd_sys_fcntl(ctx);
}

/* pathconf(), lpathconf(), fpathconf(): (path or fd, name) */
static long freebsd_sys_pathconf(struct abi_ctx *ctx)
{
	switch ((int)ctx->args[1]) {
	case 1:		return 32767;		/* _PC_LINK_MAX */
	case 2:					/* _PC_MAX_CANON */
	case 3:		return 255;		/* _PC_MAX_INPUT */
	case 4:		return NAME_MAX;	/* _PC_NAME_MAX */
	case 5:		return 1024;		/* _PC_PATH_MAX */
	case 6:		return PIPE_BUF;	/* _PC_PIPE_BUF */
	case 7:		return 1;		/* _PC_CHOWN_RESTRICTED */
	case 8:		return 1;		/* _PC_NO_TRUNC */
	case 9:		return 0xff;		/* _PC_VDISABLE */
	}
	return -EINVAL;				/* ACLs and friends */
}

static long freebsd_sys_eopnotsupp(struct abi_ctx *ctx)
{
	return -EOPNOTSUPP;
}

/* mkfifo(const char *path, mode_t mode) */
static long freebsd_sys_mkfifo(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_mknodat, AT_FDCWD, ctx->args[0],
			   S_IFIFO | (ctx->args[1] & 07777), 0);
}

/* closefrom(int lowfd) */
static long freebsd_sys_closefrom(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_close_range, ctx->args[0], ~0U, 0);
}

/*
 * Memory
 */
#define FREEBSD_MAP_GUARD	0x00002000
#define FREEBSD_MAP_EXCL	0x00004000
#define FREEBSD_MAP_32BIT	0x00080000
#define FREEBSD_MAP_ALIGN_SHIFT	24

/* mmap(void *addr, size_t len, int prot, int flags, int fd, off_t pos) */
static long freebsd_sys_mmap(struct abi_ctx *ctx)
{
	unsigned int flags = ctx->args[3];
	unsigned int shift = flags >> FREEBSD_MAP_ALIGN_SHIFT;
	int prot = ctx->args[2] & (PROT_READ | PROT_WRITE | PROT_EXEC);
	unsigned long lflags = 0, align = 0;

	lflags |= (flags & BSD_MAP_SHARED) ? MAP_SHARED : MAP_PRIVATE;
	if (flags & BSD_MAP_FIXED)
		lflags |= (flags & FREEBSD_MAP_EXCL) ? MAP_FIXED_NOREPLACE :
						       MAP_FIXED;
	if (flags & (BSD_MAP_ANON | BSD_MAP_STACK))
		lflags |= MAP_ANONYMOUS;
	if (flags & FREEBSD_MAP_32BIT)
		lflags |= MAP_32BIT;
	if (flags & FREEBSD_MAP_GUARD) {
		/* reserve address space that faults on access */
		lflags |= MAP_ANONYMOUS | MAP_NORESERVE;
		prot = PROT_NONE;
	}
	if (shift == 1)			/* MAP_ALIGNED_SUPER */
		shift = 21;
	if (shift >= PAGE_SHIFT && shift < 40)
		align = 1UL << shift;

	return abi_bsd_mmap(ctx->args[0], ctx->args[1], prot, lflags,
			    ctx->args[4], ctx->args[5], align);
}

/* msync(): MS_SYNC is 0 on FreeBSD */
static long freebsd_sys_msync(struct abi_ctx *ctx)
{
	unsigned int flags = ctx->args[2], lflags = 0;

	lflags |= (flags & 1) ? MS_ASYNC : MS_SYNC;
	if (flags & 2)
		lflags |= MS_INVALIDATE;
	return abi_native3(ctx, __NR_msync, ctx->args[0], ctx->args[1], lflags);
}

/*
 * Machine dependent: sysarch(int op, void *parms) sets the TLS base.
 */
static long freebsd_sys_sysarch(struct abi_ctx *ctx)
{
	unsigned long base;

	switch ((int)ctx->args[0]) {
	case 129:	/* AMD64_SET_FSBASE */
		if (get_user(base, (unsigned long __user *)ctx->args[1]))
			return -EFAULT;
		return abi_native2(ctx, __NR_arch_prctl, ARCH_SET_FS, base);
	case 131:	/* AMD64_SET_GSBASE */
		if (get_user(base, (unsigned long __user *)ctx->args[1]))
			return -EFAULT;
		return abi_native2(ctx, __NR_arch_prctl, ARCH_SET_GS, base);
	case 128:	/* AMD64_GET_FSBASE */
		return abi_native2(ctx, __NR_arch_prctl, ARCH_GET_FS, ctx->args[1]);
	case 130:	/* AMD64_GET_GSBASE */
		return abi_native2(ctx, __NR_arch_prctl, ARCH_GET_GS, ctx->args[1]);
	}
	return -EINVAL;
}

/*
 * Processes and threads
 */

/* wait6(idtype, id, status, options, wrusage, info) for the common cases */
static long freebsd_sys_wait6(struct abi_ctx *ctx)
{
	long pid;

	switch ((int)ctx->args[0]) {
	case 0:		pid = ctx->args[1]; break;		/* P_PID */
	case 2:		pid = -(long)ctx->args[1]; break;	/* P_PGID */
	case 7:		pid = -1; break;			/* P_ALL */
	default:	return -EINVAL;
	}
	ctx->args[0] = pid;
	ctx->args[3] = ctx->args[4];
	return bsd_sys_wait4(ctx);
}

/* thr_self(long *id) */
static long freebsd_sys_thr_self(struct abi_ctx *ctx)
{
	return put_user((long)task_pid_vnr(current),
			(long __user *)ctx->args[0]) ? -EFAULT : 0;
}

/* thr_exit(long *state) */
static long freebsd_sys_thr_exit(struct abi_ctx *ctx)
{
	if (ctx->args[0])
		put_user(1L, (long __user *)ctx->args[0]);
	return abi_native1(ctx, __NR_exit, 0);
}

/*
 * _umtx_op(void *obj, int op, u_long val, void *uaddr, void *uaddr2)
 *
 * Only what a single threaded process can run into.  Proper umtx
 * support (on top of futexes) is needed for libthr.
 */
static long freebsd_sys_umtx_op(struct abi_ctx *ctx)
{
	switch ((int)ctx->args[1]) {
	case 3:		/* UMTX_OP_WAKE */
	case 16:	/* UMTX_OP_WAKE_PRIVATE */
	case 21:	/* UMTX_OP_NWAKE_PRIVATE */
	case 26:	/* UMTX_OP_ROBUST_LISTS */
		return 0;
	}
	return -ENOSYS;
}

/* cpuset_getaffinity(level, which, id, setsize, mask) */
static long freebsd_sys_cpuset_getaffinity(struct abi_ctx *ctx)
{
	size_t size = ctx->args[3];
	unsigned long mask[4] = { 0 };
	unsigned int cpu;

	if (size < sizeof(long) || size > sizeof(mask))
		size = min(size, sizeof(mask));
	for_each_online_cpu(cpu)
		if (cpu < size * 8)
			mask[cpu / 64] |= 1UL << (cpu % 64);
	if (clear_user((void __user *)ctx->args[4], ctx->args[3]) ||
	    copy_to_user((void __user *)ctx->args[4], mask, size))
		return -EFAULT;
	return 0;
}

static long freebsd_sys_seteuid(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_setresuid, -1, ctx->args[0], -1);
}

static long freebsd_sys_setegid(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_setresgid, -1, ctx->args[0], -1);
}

/*
 * Time
 */
static int freebsd_clockid(unsigned int id)
{
	switch (id) {
	case 0: case 9: case 10:	return CLOCK_REALTIME;
	case 4: case 11: case 12:	return CLOCK_MONOTONIC;
	case 5: case 7: case 8:		return CLOCK_BOOTTIME;
	case 13:			return CLOCK_REALTIME_COARSE;
	case 1: case 2: case 15:	return CLOCK_PROCESS_CPUTIME_ID;
	case 14:			return CLOCK_THREAD_CPUTIME_ID;
	}
	return -1;
}

static long freebsd_sys_clock_gettime(struct abi_ctx *ctx)
{
	int id = freebsd_clockid(ctx->args[0]);

	if (id < 0)
		return -EINVAL;
	return abi_native2(ctx, __NR_clock_gettime, id, ctx->args[1]);
}

static long freebsd_sys_clock_getres(struct abi_ctx *ctx)
{
	int id = freebsd_clockid(ctx->args[0]);

	if (id < 0)
		return -EINVAL;
	return abi_native2(ctx, __NR_clock_getres, id, ctx->args[1]);
}

/* ppoll(fds, nfds, const struct timespec *ts, const sigset_t *set) */
static long freebsd_sys_ppoll(struct abi_ctx *ctx)
{
	u64 __user *gset = NULL;
	u32 set[4];

	if (ctx->args[3]) {
		if (copy_from_user(set, (void __user *)ctx->args[3], sizeof(set)))
			return -EFAULT;
		gset = abi_gap(ctx, sizeof(u64));
		if (put_user(abi_bsd_sigset_to_linux(set), gset))
			return -EFAULT;
	}
	return abi_native5(ctx, __NR_ppoll, ctx->args[0], ctx->args[1],
			   ctx->args[2], (unsigned long)gset, 8);
}

/* pselect(nd, in, ou, ex, const struct timespec *ts, const sigset_t *sm) */
static long freebsd_sys_pselect(struct abi_ctx *ctx)
{
	struct { u64 set; u64 size; } __user *garg = NULL, arg;
	u64 __user *gset;
	u32 set[4];

	if (ctx->args[5]) {
		if (copy_from_user(set, (void __user *)ctx->args[5], sizeof(set)))
			return -EFAULT;
		gset = abi_gap(ctx, sizeof(u64));
		garg = abi_gap(ctx, sizeof(arg));
		arg.set = (unsigned long)gset;
		arg.size = 8;
		if (put_user(abi_bsd_sigset_to_linux(set), gset) ||
		    copy_to_user(garg, &arg, sizeof(arg)))
			return -EFAULT;
	}
	return abi_native(ctx, __NR_pselect6, ctx->args[0], ctx->args[1],
			  ctx->args[2], ctx->args[3], ctx->args[4],
			  (unsigned long)garg);
}

#define S(nr)	[FREEBSD_SYS_##nr]

const struct abi_sysent freebsd_sysent[] = {
	/* processes */
	S(_exit)	= ABI_EMUL("exit", 1, bsd_sys_exit),
	S(fork)		= ABI_EMUL("fork", 0, bsd_sys_fork),
	S(vfork)	= ABI_EMUL("vfork", 0, bsd_sys_vfork),
	S(execve)	= ABI_PASS("execve", 3, __NR_execve),
	S(wait4)	= ABI_EMUL("wait4", 4, bsd_sys_wait4),
	S(wait6)	= ABI_EMUL("wait6", 6, freebsd_sys_wait6),
	S(getpid)	= ABI_PASS("getpid", 0, __NR_getpid),
	S(getppid)	= ABI_PASS("getppid", 0, __NR_getppid),
	S(getpgrp)	= ABI_PASS("getpgrp", 0, __NR_getpgrp),
	S(getpgid)	= ABI_PASS("getpgid", 1, __NR_getpgid),
	S(setpgid)	= ABI_PASS("setpgid", 2, __NR_setpgid),
	S(setsid)	= ABI_PASS("setsid", 0, __NR_setsid),
	S(getsid)	= ABI_PASS("getsid", 1, __NR_getsid),
	S(getuid)	= ABI_PASS("getuid", 0, __NR_getuid),
	S(geteuid)	= ABI_PASS("geteuid", 0, __NR_geteuid),
	S(getgid)	= ABI_PASS("getgid", 0, __NR_getgid),
	S(getegid)	= ABI_PASS("getegid", 0, __NR_getegid),
	S(setuid)	= ABI_PASS("setuid", 1, __NR_setuid),
	S(setgid)	= ABI_PASS("setgid", 1, __NR_setgid),
	S(seteuid)	= ABI_EMUL("seteuid", 1, freebsd_sys_seteuid),
	S(setegid)	= ABI_EMUL("setegid", 1, freebsd_sys_setegid),
	S(setreuid)	= ABI_PASS("setreuid", 2, __NR_setreuid),
	S(setregid)	= ABI_PASS("setregid", 2, __NR_setregid),
	S(setresuid)	= ABI_PASS("setresuid", 3, __NR_setresuid),
	S(setresgid)	= ABI_PASS("setresgid", 3, __NR_setresgid),
	S(getresuid)	= ABI_PASS("getresuid", 3, __NR_getresuid),
	S(getresgid)	= ABI_PASS("getresgid", 3, __NR_getresgid),
	S(getgroups)	= ABI_PASS("getgroups", 2, __NR_getgroups),
	S(setgroups)	= ABI_PASS("setgroups", 2, __NR_setgroups),
	S(getlogin)	= ABI_EMUL("getlogin", 2, bsd_sys_getlogin),
	S(issetugid)	= ABI_EMUL("issetugid", 0, bsd_sys_zero),
	S(umask)	= ABI_PASS("umask", 1, __NR_umask),
	S(getrlimit)	= ABI_EMUL("getrlimit", 2, bsd_sys_getrlimit),
	S(setrlimit)	= ABI_EMUL("setrlimit", 2, bsd_sys_setrlimit),
	S(getrusage)	= ABI_PASS("getrusage", 2, __NR_getrusage),
	S(setpriority)	= ABI_PASS("setpriority", 3, __NR_setpriority),
	S(sched_yield)	= ABI_PASS("sched_yield", 0, __NR_sched_yield),
	S(sysarch)	= ABI_EMUL("sysarch", 2, freebsd_sys_sysarch),
	S(__sysctl)	= ABI_EMUL("__sysctl", 6, freebsd_sys_sysctl),
	S(__sysctlbyname) = ABI_EMUL("__sysctlbyname", 6, freebsd_sys_sysctlbyname),
	S(thr_self)	= ABI_EMUL("thr_self", 1, freebsd_sys_thr_self),
	S(thr_exit)	= ABI_EMUL("thr_exit", 1, freebsd_sys_thr_exit),
	S(thr_kill)	= ABI_EMUL("thr_kill", 2, freebsd_sys_thr_kill),
	S(_umtx_op)	= ABI_EMUL("_umtx_op", 5, freebsd_sys_umtx_op),
	S(cpuset_getaffinity) = ABI_EMUL("cpuset_getaffinity", 5,
					 freebsd_sys_cpuset_getaffinity),
	S(utrace)	= ABI_EMUL("utrace", 2, bsd_sys_zero),
	S(sigfastblock)	= ABI_EMUL("sigfastblock", 2, bsd_sys_zero),

	/* signals */
	S(kill)		= ABI_EMUL("kill", 2, bsd_sys_kill),
	S(sigaction)	= ABI_EMUL("sigaction", 3, freebsd_sys_sigaction),
	S(sigprocmask)	= ABI_EMUL("sigprocmask", 3, bsd_sys_sigprocmask_set),
	S(sigsuspend)	= ABI_EMUL("sigsuspend", 1, freebsd_sys_sigsuspend),
	S(sigpending)	= ABI_EMUL("sigpending", 1, freebsd_sys_sigpending),
	S(sigaltstack)	= ABI_EMUL("sigaltstack", 2, bsd_sys_sigaltstack),
	S(sigreturn)	= ABI_EMUL("sigreturn", 1, freebsd_sys_sigreturn),
	S(setcontext)	= ABI_EMUL("setcontext", 1, freebsd_sys_sigreturn),

	/* files */
	S(read)		= ABI_PASS("read", 3, __NR_read),
	S(write)	= ABI_PASS("write", 3, __NR_write),
	S(readv)	= ABI_PASS("readv", 3, __NR_readv),
	S(writev)	= ABI_PASS("writev", 3, __NR_writev),
	S(pread)	= ABI_PASS("pread", 4, __NR_pread64),
	S(pwrite)	= ABI_PASS("pwrite", 4, __NR_pwrite64),
	S(open)		= ABI_EMUL("open", 3, bsd_sys_open),
	S(openat)	= ABI_EMUL("openat", 4, bsd_sys_openat),
	S(close)	= ABI_PASS("close", 1, __NR_close),
	S(close_range)	= ABI_PASS("close_range", 3, __NR_close_range),
	S(freebsd12_closefrom)	= ABI_EMUL("closefrom", 1, freebsd_sys_closefrom),
	S(dup)		= ABI_PASS("dup", 1, __NR_dup),
	S(dup2)		= ABI_PASS("dup2", 2, __NR_dup2),
	S(freebsd10_pipe) = ABI_EMUL("pipe", 0, bsd_sys_pipe),
	S(pipe2)	= ABI_EMUL("pipe2", 2, bsd_sys_pipe2),
	S(fcntl)	= ABI_EMUL("fcntl", 3, freebsd_sys_fcntl),
	S(ioctl)	= ABI_EMUL("ioctl", 3, bsd_sys_ioctl),
	S(lseek)	= ABI_PASS("lseek", 3, __NR_lseek),
	S(truncate)	= ABI_PASS("truncate", 2, __NR_truncate),
	S(ftruncate)	= ABI_PASS("ftruncate", 2, __NR_ftruncate),
	S(fsync)	= ABI_PASS("fsync", 1, __NR_fsync),
	S(fdatasync)	= ABI_PASS("fdatasync", 1, __NR_fdatasync),
	S(sync)		= ABI_PASS("sync", 0, __NR_sync),
	S(flock)	= ABI_PASS("flock", 2, __NR_flock),
	S(fstat)	= ABI_EMUL("fstat", 2, freebsd_sys_fstat),
	S(fstatat)	= ABI_EMUL("fstatat", 4, freebsd_sys_fstatat),
	S(statfs)	= ABI_EMUL("statfs", 2, freebsd_sys_statfs),
	S(fstatfs)	= ABI_EMUL("fstatfs", 2, freebsd_sys_fstatfs),
	S(getdirentries) = ABI_EMUL("getdirentries", 4, freebsd_sys_getdirentries),
	S(access)	= ABI_EMUL("access", 2, bsd_sys_access),
	S(eaccess)	= ABI_EMUL("eaccess", 2, bsd_sys_access),
	S(faccessat)	= ABI_EMUL("faccessat", 4, bsd_sys_faccessat),
	S(readlink)	= ABI_PASS("readlink", 3, __NR_readlink),
	S(readlinkat)	= ABI_PASS("readlinkat", 4, __NR_readlinkat),
	S(chdir)	= ABI_PASS("chdir", 1, __NR_chdir),
	S(fchdir)	= ABI_PASS("fchdir", 1, __NR_fchdir),
	S(chroot)	= ABI_PASS("chroot", 1, __NR_chroot),
	S(__getcwd)	= ABI_EMUL("__getcwd", 2, bsd_sys_getcwd),
	S(unlink)	= ABI_PASS("unlink", 1, __NR_unlink),
	S(unlinkat)	= ABI_EMUL("unlinkat", 3, bsd_sys_unlinkat),
	S(rmdir)	= ABI_PASS("rmdir", 1, __NR_rmdir),
	S(mkdir)	= ABI_PASS("mkdir", 2, __NR_mkdir),
	S(mkdirat)	= ABI_PASS("mkdirat", 3, __NR_mkdirat),
	S(mkfifo)	= ABI_EMUL("mkfifo", 2, freebsd_sys_mkfifo),
	S(rename)	= ABI_PASS("rename", 2, __NR_rename),
	S(renameat)	= ABI_PASS("renameat", 4, __NR_renameat),
	S(link)		= ABI_PASS("link", 2, __NR_link),
	S(linkat)	= ABI_EMUL("linkat", 5, bsd_sys_linkat),
	S(symlink)	= ABI_PASS("symlink", 2, __NR_symlink),
	S(symlinkat)	= ABI_PASS("symlinkat", 3, __NR_symlinkat),
	S(chmod)	= ABI_PASS("chmod", 2, __NR_chmod),
	S(fchmod)	= ABI_PASS("fchmod", 2, __NR_fchmod),
	S(fchmodat)	= ABI_EMUL("fchmodat", 4, bsd_sys_fchmodat),
	S(chown)	= ABI_PASS("chown", 3, __NR_chown),
	S(lchown)	= ABI_PASS("lchown", 3, __NR_lchown),
	S(fchown)	= ABI_PASS("fchown", 3, __NR_fchown),
	S(fchownat)	= ABI_EMUL("fchownat", 5, bsd_sys_fchownat),
	S(utimensat)	= ABI_EMUL("utimensat", 4, bsd_sys_utimensat),
	S(pathconf)	= ABI_EMUL("pathconf", 2, freebsd_sys_pathconf),
	S(lpathconf)	= ABI_EMUL("lpathconf", 2, freebsd_sys_pathconf),
	S(fpathconf)	= ABI_EMUL("fpathconf", 2, freebsd_sys_pathconf),
	S(getdtablesize) = ABI_EMUL("getdtablesize", 0, bsd_sys_getdtablesize),
	S(extattr_get_file) = ABI_EMUL("extattr_get_file", 5, freebsd_sys_eopnotsupp),
	S(extattr_get_link) = ABI_EMUL("extattr_get_link", 5, freebsd_sys_eopnotsupp),
	S(extattr_get_fd) = ABI_EMUL("extattr_get_fd", 5, freebsd_sys_eopnotsupp),
	S(extattr_list_file) = ABI_EMUL("extattr_list_file", 4, freebsd_sys_eopnotsupp),
	S(extattr_list_link) = ABI_EMUL("extattr_list_link", 4, freebsd_sys_eopnotsupp),
	S(extattr_list_fd) = ABI_EMUL("extattr_list_fd", 4, freebsd_sys_eopnotsupp),

	/* memory */
	S(mmap)		= ABI_EMUL("mmap", 6, freebsd_sys_mmap),
	S(munmap)	= ABI_PASS("munmap", 2, __NR_munmap),
	S(mprotect)	= ABI_PASS("mprotect", 3, __NR_mprotect),
	S(madvise)	= ABI_EMUL("madvise", 3, bsd_sys_madvise),
	S(msync)	= ABI_EMUL("msync", 3, freebsd_sys_msync),
	S(minherit)	= ABI_EMUL("minherit", 3, bsd_sys_zero),
	S(mlock)	= ABI_PASS("mlock", 2, __NR_mlock),
	S(munlock)	= ABI_PASS("munlock", 2, __NR_munlock),

	/* time and polling */
	S(gettimeofday)	= ABI_PASS("gettimeofday", 2, __NR_gettimeofday),
	S(clock_gettime) = ABI_EMUL("clock_gettime", 2, freebsd_sys_clock_gettime),
	S(clock_getres)	= ABI_EMUL("clock_getres", 2, freebsd_sys_clock_getres),
	S(nanosleep)	= ABI_PASS("nanosleep", 2, __NR_nanosleep),
	S(getitimer)	= ABI_PASS("getitimer", 2, __NR_getitimer),
	S(setitimer)	= ABI_PASS("setitimer", 3, __NR_setitimer),
	S(select)	= ABI_PASS("select", 5, __NR_select),
	S(pselect)	= ABI_EMUL("pselect", 6, freebsd_sys_pselect),
	S(poll)		= ABI_PASS("poll", 3, __NR_poll),
	S(ppoll)	= ABI_EMUL("ppoll", 4, freebsd_sys_ppoll),
	S(getrandom)	= ABI_PASS("getrandom", 3, __NR_getrandom),
};

const unsigned int freebsd_nsysent = ARRAY_SIZE(freebsd_sysent);
