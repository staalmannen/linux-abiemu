// SPDX-License-Identifier: GPL-2.0
/*
 * abi_netbsd - system call table and NetBSD specific calls
 */
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/mman.h>
#include <linux/cred.h>
#include <linux/kdev_t.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/time.h>
#include <linux/statfs.h>
#include <linux/slab.h>
#include <asm/unistd.h>
#include <asm/prctl.h>

#include "netbsd.h"

/* NetBSD makedev() */
static u64 netbsd_dev(dev_t d)
{
	u32 ma = MAJOR(d), mi = MINOR(d);

	return ((ma << 8) & 0x000fff00) | ((mi << 12) & 0xfff00000) |
	       (mi & 0xff);
}

struct netbsd_stat {
	u64	st_dev;
	u32	st_mode;
	u64	st_ino;
	u32	st_nlink;
	u32	st_uid;
	u32	st_gid;
	u64	st_rdev;
	struct __kernel_timespec st_atim, st_mtim, st_ctim, st_birthtim;
	s64	st_size;
	s64	st_blocks;
	u32	st_blksize;
	u32	st_flags;
	u32	st_gen;
	u32	st_spare[2];
};

static long netbsd_put_stat(const struct kstat *k, void __user *ubuf)
{
	struct netbsd_stat st;

	memset(&st, 0, sizeof(st));
	st.st_dev = netbsd_dev(k->dev);
	st.st_mode = k->mode;
	st.st_ino = k->ino;
	st.st_nlink = k->nlink;
	st.st_uid = from_kuid_munged(current_user_ns(), k->uid);
	st.st_gid = from_kgid_munged(current_user_ns(), k->gid);
	st.st_rdev = netbsd_dev(k->rdev);
	st.st_atim = (struct __kernel_timespec){ k->atime.tv_sec, k->atime.tv_nsec };
	st.st_mtim = (struct __kernel_timespec){ k->mtime.tv_sec, k->mtime.tv_nsec };
	st.st_ctim = (struct __kernel_timespec){ k->ctime.tv_sec, k->ctime.tv_nsec };
	st.st_birthtim = (k->result_mask & STATX_BTIME) ?
		(struct __kernel_timespec){ k->btime.tv_sec, k->btime.tv_nsec } :
		st.st_ctim;
	st.st_size = k->size;
	st.st_blocks = k->blocks;
	st.st_blksize = k->blksize;
	return copy_to_user(ubuf, &st, sizeof(st)) ? -EFAULT : 0;
}

static long netbsd_sys_fstat(struct abi_ctx *ctx)
{
	struct kstat k;
	int err = abi_stat_fd(ctx->args[0], &k);

	return err ?: netbsd_put_stat(&k, (void __user *)ctx->args[1]);
}

static long netbsd_sys_stat(struct abi_ctx *ctx)
{
	struct kstat k;
	int err = abi_stat_path(AT_FDCWD, (void __user *)ctx->args[0], true,
				false, &k);

	return err ?: netbsd_put_stat(&k, (void __user *)ctx->args[1]);
}

static long netbsd_sys_lstat(struct abi_ctx *ctx)
{
	struct kstat k;
	int err = abi_stat_path(AT_FDCWD, (void __user *)ctx->args[0], false,
				false, &k);

	return err ?: netbsd_put_stat(&k, (void __user *)ctx->args[1]);
}

/* fstatat(int fd, const char *path, struct stat *buf, int flag) */
static long netbsd_sys_fstatat(struct abi_ctx *ctx)
{
	struct kstat k;
	int err = abi_stat_path(ctx->args[0], (void __user *)ctx->args[1],
				!(ctx->args[3] & 0x200), false, &k);

	return err ?: netbsd_put_stat(&k, (void __user *)ctx->args[2]);
}

/* __getdents30(int fd, char *buf, size_t count) */
static long netbsd_sys_getdents(struct abi_ctx *ctx)
{
	return abi_getdents(ctx->args[0], (void __user *)ctx->args[1],
			    ctx->args[2], NULL, abi_bsd_emit_dirent_netbsd);
}

struct netbsd_statvfs {
	u64	f_flag, f_bsize, f_frsize, f_iosize;
	u64	f_blocks, f_bfree, f_bavail, f_bresvd;
	u64	f_files, f_ffree, f_favail, f_fresvd;
	u64	f_syncreads, f_syncwrites, f_asyncreads, f_asyncwrites;
	s32	f_fsidx[2];
	u64	f_fsid;
	u64	f_namemax;
	u32	f_owner;
	u64	f_spare[4];
	char	f_fstypename[32];
	char	f_mntonname[1024];
	char	f_mntfromname[1024];
	char	f_mntfromlabel[1024];
};

static long netbsd_statvfs_post(struct abi_ctx *ctx, long ret)
{
	struct netbsd_statvfs *v;
	struct statfs ls;

	if (ret)
		return ret;
	if (copy_from_user(&ls, (void __user *)ctx->priv[0], sizeof(ls)))
		return -EFAULT;
	v = kzalloc(sizeof(*v), GFP_KERNEL);
	if (!v)
		return -ENOMEM;
	v->f_flag = (ls.f_flags & ST_RDONLY ? 1 : 0) |
		    (ls.f_flags & ST_NOSUID ? 8 : 0);
	v->f_bsize = ls.f_bsize;
	v->f_frsize = ls.f_frsize ?: ls.f_bsize;
	v->f_iosize = ls.f_bsize;
	v->f_blocks = ls.f_blocks;
	v->f_bfree = ls.f_bfree;
	v->f_bavail = ls.f_bavail;
	v->f_bresvd = ls.f_bfree - ls.f_bavail;
	v->f_files = ls.f_files;
	v->f_ffree = v->f_favail = ls.f_ffree;
	v->f_fsidx[0] = ls.f_fsid.val[0];
	v->f_fsidx[1] = ls.f_fsid.val[1];
	v->f_fsid = (u32)ls.f_fsid.val[0];
	v->f_namemax = ls.f_namelen;
	strscpy(v->f_fstypename, "ffs", sizeof(v->f_fstypename));
	strscpy(v->f_mntonname, "/", sizeof(v->f_mntonname));
	strscpy(v->f_mntfromname, "linux", sizeof(v->f_mntfromname));
	if (copy_to_user((void __user *)ctx->args[1], v, sizeof(*v)))
		ret = -EFAULT;
	kfree(v);
	return ret;
}

/* __getvfsstat90(buf, bufsize, flags): we only report the root */
static long netbsd_getvfsstat_post(struct abi_ctx *ctx, long ret)
{
	ret = netbsd_statvfs_post(ctx, ret);
	return ret ?: 1;
}

static long netbsd_sys_getvfsstat(struct abi_ctx *ctx)
{
	void __user *g;
	char __user *root;

	if (!ctx->args[0])
		return 1;
	if (ctx->args[1] < sizeof(struct netbsd_statvfs))
		return 0;
	root = abi_gap_str(ctx, "/");
	g = abi_gap(ctx, sizeof(struct statfs));
	if (!root)
		return -EFAULT;
	ctx->args[1] = ctx->args[0];	/* where the post routine stores */
	ctx->priv[0] = (unsigned long)g;
	ctx->post = netbsd_getvfsstat_post;
	return abi_native2(ctx, __NR_statfs, (unsigned long)root, (unsigned long)g);
}

/* __statvfs190(path, buf, flags) and __fstatvfs190(fd, buf, flags) */
static long netbsd_sys_statvfs(struct abi_ctx *ctx)
{
	void __user *g = abi_gap(ctx, sizeof(struct statfs));

	ctx->priv[0] = (unsigned long)g;
	ctx->post = netbsd_statvfs_post;
	return abi_native2(ctx, __NR_statfs, ctx->args[0], (unsigned long)g);
}

static long netbsd_sys_fstatvfs(struct abi_ctx *ctx)
{
	void __user *g = abi_gap(ctx, sizeof(struct statfs));

	ctx->priv[0] = (unsigned long)g;
	ctx->post = netbsd_statvfs_post;
	return abi_native2(ctx, __NR_fstatfs, ctx->args[0], (unsigned long)g);
}

/* The calls with a padding argument in front of a 64 bit offset */
static long netbsd_sys_lseek(struct abi_ctx *ctx)	/* fd, PAD, off, whence */
{
	return abi_native3(ctx, __NR_lseek, ctx->args[0], ctx->args[2],
			   ctx->args[3]);
}

static long netbsd_sys_pread(struct abi_ctx *ctx)	/* fd, buf, n, PAD, off */
{
	return abi_native4(ctx, __NR_pread64, ctx->args[0], ctx->args[1],
			   ctx->args[2], ctx->args[4]);
}

static long netbsd_sys_pwrite(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_pwrite64, ctx->args[0], ctx->args[1],
			   ctx->args[2], ctx->args[4]);
}

static long netbsd_sys_truncate(struct abi_ctx *ctx)	/* path, PAD, len */
{
	return abi_native2(ctx, __NR_truncate, ctx->args[0], ctx->args[2]);
}

static long netbsd_sys_ftruncate(struct abi_ctx *ctx)
{
	return abi_native2(ctx, __NR_ftruncate, ctx->args[0], ctx->args[2]);
}

static long netbsd_sys_fcntl(struct abi_ctx *ctx)
{
	switch ((int)ctx->args[1]) {
	case 10:	/* F_CLOSEM */
		return abi_native3(ctx, __NR_close_range, ctx->args[0], ~0U, 0);
	case 13:	/* F_GETNOSIGPIPE */
		return 0;
	case 14:	/* F_SETNOSIGPIPE */
		return 0;
	}
	return bsd_sys_fcntl(ctx);
}

/* __dup3100(int from, int to, int flags) */
static long netbsd_sys_dup3(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_dup3, ctx->args[0], ctx->args[1],
			   (ctx->args[2] & 0x400000) ? O_CLOEXEC : 0);
}

static long netbsd_sys_mkfifo(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_mknodat, AT_FDCWD, ctx->args[0],
			   S_IFIFO | (ctx->args[1] & 07777), 0);
}

static long netbsd_sys_pathconf(struct abi_ctx *ctx)
{
	switch ((int)ctx->args[1]) {
	case 1:		return 32767;		/* _PC_LINK_MAX */
	case 2:
	case 3:		return 255;
	case 4:		return NAME_MAX;
	case 5:		return 1024;
	case 6:		return PIPE_BUF;
	case 7:
	case 8:		return 1;
	case 9:		return 0xff;
	}
	return -EINVAL;
}

/*
 * Memory
 */
#define NETBSD_MAP_STACK	0x2000
#define NETBSD_MAP_ALIGN_SHIFT	24

/* mmap(addr, len, prot, flags, fd, PAD, pos): pos is on the stack */
static long netbsd_sys_mmap(struct abi_ctx *ctx)
{
	unsigned int flags = ctx->args[3];
	unsigned int shift = flags >> NETBSD_MAP_ALIGN_SHIFT;
	int prot = ctx->args[2] & (PROT_READ | PROT_WRITE | PROT_EXEC);
	unsigned long lflags = 0, align = 0;
	loff_t pos = abi_bsd_stackarg(ctx, 6);

	lflags |= (flags & BSD_MAP_SHARED) ? MAP_SHARED : MAP_PRIVATE;
	if (flags & BSD_MAP_FIXED)
		lflags |= MAP_FIXED;
	if (flags & (BSD_MAP_ANON | NETBSD_MAP_STACK))
		lflags |= MAP_ANONYMOUS;
	if (shift >= PAGE_SHIFT && shift < 40)
		align = 1UL << shift;
	return abi_bsd_mmap(ctx->args[0], ctx->args[1], prot, lflags,
			    ctx->args[4], pos, align);
}

/* __msync13(addr, len, flags): MS_ASYNC 1, MS_INVALIDATE 2, MS_SYNC 4 */
static long netbsd_sys_msync(struct abi_ctx *ctx)
{
	unsigned int f = ctx->args[2], lf = 0;

	if (f & 1)
		lf |= MS_ASYNC;
	if (f & 2)
		lf |= MS_INVALIDATE;
	if (f & 4 || !(f & 1))
		lf |= MS_SYNC;
	return abi_native3(ctx, __NR_msync, ctx->args[0], ctx->args[1], lf);
}

/*
 * Processes, LWPs and identity
 */
static long netbsd_sys_getpid(struct abi_ctx *ctx)
{
	pid_t ppid;

	rcu_read_lock();
	ppid = task_tgid_vnr(rcu_dereference(current->real_parent));
	rcu_read_unlock();
	abi_bsd_retval2(ctx, ppid);
	return task_tgid_vnr(current);
}

static long netbsd_sys_getuid(struct abi_ctx *ctx)
{
	abi_bsd_retval2(ctx, from_kuid_munged(current_user_ns(), current_euid()));
	return from_kuid_munged(current_user_ns(), current_uid());
}

static long netbsd_sys_getgid(struct abi_ctx *ctx)
{
	abi_bsd_retval2(ctx, from_kgid_munged(current_user_ns(), current_egid()));
	return from_kgid_munged(current_user_ns(), current_gid());
}

static long netbsd_sys_seteuid(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_setresuid, -1, ctx->args[0], -1);
}

static long netbsd_sys_setegid(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_setresgid, -1, ctx->args[0], -1);
}

static long netbsd_sys_lwp_self(struct abi_ctx *ctx)
{
	return task_pid_vnr(current);
}

static long netbsd_sys_lwp_setprivate(struct abi_ctx *ctx)
{
	return abi_native2(ctx, __NR_arch_prctl, ARCH_SET_FS, ctx->args[0]);
}

static long netbsd_lwp_getprivate_post(struct abi_ctx *ctx, long ret)
{
	unsigned long v;

	if (ret)
		return ret;
	return get_user(v, (unsigned long __user *)ctx->priv[0]) ? -EFAULT : v;
}

static long netbsd_sys_lwp_getprivate(struct abi_ctx *ctx)
{
	void __user *g = abi_gap(ctx, sizeof(long));

	ctx->priv[0] = (unsigned long)g;
	ctx->post = netbsd_lwp_getprivate_post;
	return abi_native2(ctx, __NR_arch_prctl, ARCH_GET_FS, (unsigned long)g);
}

/* _lwp_kill(lwpid_t lwp, int sig) */
static long netbsd_sys_lwp_kill(struct abi_ctx *ctx)
{
	int sig = abi_bsd_sig_to_linux(ctx->args[1]);

	if (sig < 0)
		return -EINVAL;
	return abi_native2(ctx, __NR_tkill, ctx->args[0], sig);
}

/* sysarch(int op, void *parms): X86_64_GET_FSBASE 14, SET_FSBASE 15 */
static long netbsd_sys_sysarch(struct abi_ctx *ctx)
{
	unsigned long base;

	switch ((int)ctx->args[0]) {
	case 15:
		if (get_user(base, (unsigned long __user *)ctx->args[1]))
			return -EFAULT;
		return abi_native2(ctx, __NR_arch_prctl, ARCH_SET_FS, base);
	case 14:
		return abi_native2(ctx, __NR_arch_prctl, ARCH_GET_FS, ctx->args[1]);
	}
	return -EINVAL;
}

/*
 * Time
 */
static int netbsd_clockid(unsigned int id)
{
	switch (id) {
	case 0:			return CLOCK_REALTIME;
	case 3:			return CLOCK_MONOTONIC;
	case 0x20000000:	return CLOCK_THREAD_CPUTIME_ID;
	case 0x40000000:	return CLOCK_PROCESS_CPUTIME_ID;
	}
	return -1;
}

static long netbsd_sys_clock_gettime(struct abi_ctx *ctx)
{
	int id = netbsd_clockid(ctx->args[0]);

	return id < 0 ? -EINVAL :
		abi_native2(ctx, __NR_clock_gettime, id, ctx->args[1]);
}

static long netbsd_sys_clock_getres(struct abi_ctx *ctx)
{
	int id = netbsd_clockid(ctx->args[0]);

	return id < 0 ? -EINVAL :
		abi_native2(ctx, __NR_clock_getres, id, ctx->args[1]);
}

/* clock_nanosleep() reports errors as its return value, not via errno */
static long netbsd_clock_nanosleep_post(struct abi_ctx *ctx, long ret)
{
	return abi_is_error(ret) ? abi_errno(ctx->pers, ret) : 0;
}

/* clock_nanosleep(clockid, flags, rqtp, rmtp); TIMER_ABSTIME is 1 on both */
static long netbsd_sys_clock_nanosleep(struct abi_ctx *ctx)
{
	int id = netbsd_clockid(ctx->args[0]);

	if (id < 0)
		return EINVAL;
	ctx->flags |= ABI_F_NORESTART;
	ctx->post = netbsd_clock_nanosleep_post;
	return abi_native4(ctx, __NR_clock_nanosleep, id, ctx->args[1] & 1,
			   ctx->args[2], ctx->args[3]);
}

/* __pollts50(fds, nfds, const struct timespec *ts, const sigset_t *mask) */
static long netbsd_sys_pollts(struct abi_ctx *ctx)
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

#define S(nr)	[NETBSD_SYS_##nr]

const struct abi_sysent netbsd_sysent[] = {
	/* processes */
	S(exit)		= ABI_EMUL("exit", 1, bsd_sys_exit),
	S(fork)		= ABI_EMUL("fork", 0, bsd_sys_fork),
	S(__vfork14)	= ABI_EMUL("vfork", 0, bsd_sys_vfork),
	S(execve)	= ABI_PASS("execve", 3, __NR_execve),
	S(__wait450)	= ABI_EMUL("wait4", 4, bsd_sys_wait4),
	S(getpid_with_ppid) = ABI_EMUL("getpid", 0, netbsd_sys_getpid),
	S(getppid)	= ABI_PASS("getppid", 0, __NR_getppid),
	S(getpgrp)	= ABI_PASS("getpgrp", 0, __NR_getpgrp),
	S(getpgid)	= ABI_PASS("getpgid", 1, __NR_getpgid),
	S(setpgid)	= ABI_PASS("setpgid", 2, __NR_setpgid),
	S(setsid)	= ABI_PASS("setsid", 0, __NR_setsid),
	S(getsid)	= ABI_PASS("getsid", 1, __NR_getsid),
	S(getuid_with_euid) = ABI_EMUL("getuid", 0, netbsd_sys_getuid),
	S(geteuid)	= ABI_PASS("geteuid", 0, __NR_geteuid),
	S(getgid_with_egid) = ABI_EMUL("getgid", 0, netbsd_sys_getgid),
	S(getegid)	= ABI_PASS("getegid", 0, __NR_getegid),
	S(setuid)	= ABI_PASS("setuid", 1, __NR_setuid),
	S(setgid)	= ABI_PASS("setgid", 1, __NR_setgid),
	S(seteuid)	= ABI_EMUL("seteuid", 1, netbsd_sys_seteuid),
	S(setegid)	= ABI_EMUL("setegid", 1, netbsd_sys_setegid),
	S(setreuid)	= ABI_PASS("setreuid", 2, __NR_setreuid),
	S(setregid)	= ABI_PASS("setregid", 2, __NR_setregid),
	S(getgroups)	= ABI_PASS("getgroups", 2, __NR_getgroups),
	S(setgroups)	= ABI_PASS("setgroups", 2, __NR_setgroups),
	S(__getlogin)	= ABI_EMUL("__getlogin", 2, bsd_sys_getlogin),
	S(issetugid)	= ABI_EMUL("issetugid", 0, bsd_sys_zero),
	S(umask)	= ABI_PASS("umask", 1, __NR_umask),
	S(getrlimit)	= ABI_EMUL("getrlimit", 2, bsd_sys_getrlimit),
	S(setrlimit)	= ABI_EMUL("setrlimit", 2, bsd_sys_setrlimit),
	S(__getrusage50) = ABI_PASS("getrusage", 2, __NR_getrusage),
	S(getpriority)	= ABI_PASS("getpriority", 2, __NR_getpriority),
	S(setpriority)	= ABI_PASS("setpriority", 3, __NR_setpriority),
	S(sched_yield)	= ABI_PASS("sched_yield", 0, __NR_sched_yield),
	S(sysarch)	= ABI_EMUL("sysarch", 2, netbsd_sys_sysarch),
	S(__sysctl)	= ABI_EMUL("__sysctl", 6, netbsd_sys_sysctl),
	S(_lwp_self)	= ABI_EMUL("_lwp_self", 0, netbsd_sys_lwp_self),
	S(_lwp_setprivate) = ABI_EMUL("_lwp_setprivate", 1, netbsd_sys_lwp_setprivate),
	S(_lwp_getprivate) = ABI_EMUL("_lwp_getprivate", 0, netbsd_sys_lwp_getprivate),
	S(_lwp_kill)	= ABI_EMUL("_lwp_kill", 2, netbsd_sys_lwp_kill),
	S(_lwp_setname)	= ABI_EMUL("_lwp_setname", 2, bsd_sys_zero),
	S(_lwp_ctl)	= ABI_EMUL("_lwp_ctl", 2, bsd_sys_nosys),
	S(utrace)	= ABI_EMUL("utrace", 3, bsd_sys_zero),
	S(getrandom)	= ABI_PASS("getrandom", 3, __NR_getrandom),

	/* signals */
	S(kill)		= ABI_EMUL("kill", 2, bsd_sys_kill),
	S(__sigaction_sigtramp) = ABI_EMUL("__sigaction_sigtramp", 5,
					   netbsd_sys_sigaction),
	S(__sigprocmask14) = ABI_EMUL("sigprocmask", 3, bsd_sys_sigprocmask_set),
	S(__sigsuspend14) = ABI_EMUL("sigsuspend", 1, netbsd_sys_sigsuspend),
	S(__sigpending14) = ABI_EMUL("sigpending", 1, netbsd_sys_sigpending),
	S(__sigaltstack14) = ABI_EMUL("sigaltstack", 2, bsd_sys_sigaltstack),
	S(setcontext)	= ABI_EMUL("setcontext", 1, netbsd_sys_setcontext),

	/* files */
	S(read)		= ABI_PASS("read", 3, __NR_read),
	S(write)	= ABI_PASS("write", 3, __NR_write),
	S(readv)	= ABI_PASS("readv", 3, __NR_readv),
	S(writev)	= ABI_PASS("writev", 3, __NR_writev),
	S(pread)	= ABI_EMUL("pread", 5, netbsd_sys_pread),
	S(pwrite)	= ABI_EMUL("pwrite", 5, netbsd_sys_pwrite),
	S(open)		= ABI_EMUL("open", 3, bsd_sys_open),
	S(openat)	= ABI_EMUL("openat", 4, bsd_sys_openat),
	S(close)	= ABI_PASS("close", 1, __NR_close),
	S(dup)		= ABI_PASS("dup", 1, __NR_dup),
	S(dup2)		= ABI_PASS("dup2", 2, __NR_dup2),
	S(__dup3100)	= ABI_EMUL("dup3", 3, netbsd_sys_dup3),
	S(pipe)		= ABI_EMUL("pipe", 0, bsd_sys_pipe),
	S(pipe2)	= ABI_EMUL("pipe2", 2, bsd_sys_pipe2),
	S(fcntl)	= ABI_EMUL("fcntl", 3, netbsd_sys_fcntl),
	S(ioctl)	= ABI_EMUL("ioctl", 3, bsd_sys_ioctl),
	S(lseek)	= ABI_EMUL("lseek", 4, netbsd_sys_lseek),
	S(truncate)	= ABI_EMUL("truncate", 3, netbsd_sys_truncate),
	S(ftruncate)	= ABI_EMUL("ftruncate", 3, netbsd_sys_ftruncate),
	S(fsync)	= ABI_PASS("fsync", 1, __NR_fsync),
	S(fdatasync)	= ABI_PASS("fdatasync", 1, __NR_fdatasync),
	S(sync)		= ABI_PASS("sync", 0, __NR_sync),
	S(flock)	= ABI_PASS("flock", 2, __NR_flock),
	S(__fstat50)	= ABI_EMUL("fstat", 2, netbsd_sys_fstat),
	S(__stat50)	= ABI_EMUL("stat", 2, netbsd_sys_stat),
	S(__lstat50)	= ABI_EMUL("lstat", 2, netbsd_sys_lstat),
	S(fstatat)	= ABI_EMUL("fstatat", 4, netbsd_sys_fstatat),
	S(__statvfs190)	= ABI_EMUL("statvfs", 3, netbsd_sys_statvfs),
	S(__getvfsstat90) = ABI_EMUL("getvfsstat", 3, netbsd_sys_getvfsstat),
	S(__fstatvfs190) = ABI_EMUL("fstatvfs", 3, netbsd_sys_fstatvfs),
	S(__getdents30)	= ABI_EMUL("getdents", 3, netbsd_sys_getdents),
	S(access)	= ABI_EMUL("access", 2, bsd_sys_access),
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
	S(mkfifo)	= ABI_EMUL("mkfifo", 2, netbsd_sys_mkfifo),
	S(rename)	= ABI_PASS("rename", 2, __NR_rename),
	S(__posix_rename) = ABI_PASS("rename", 2, __NR_rename),
	S(renameat)	= ABI_PASS("renameat", 4, __NR_renameat),
	S(link)		= ABI_PASS("link", 2, __NR_link),
	S(linkat)	= ABI_EMUL("linkat", 5, bsd_sys_linkat),
	S(symlink)	= ABI_PASS("symlink", 2, __NR_symlink),
	S(symlinkat)	= ABI_PASS("symlinkat", 3, __NR_symlinkat),
	S(chmod)	= ABI_PASS("chmod", 2, __NR_chmod),
	S(fchmod)	= ABI_PASS("fchmod", 2, __NR_fchmod),
	S(fchmodat)	= ABI_EMUL("fchmodat", 4, bsd_sys_fchmodat),
	S(chown)	= ABI_PASS("chown", 3, __NR_chown),
	S(__posix_chown) = ABI_PASS("chown", 3, __NR_chown),
	S(lchown)	= ABI_PASS("lchown", 3, __NR_lchown),
	S(__posix_lchown) = ABI_PASS("lchown", 3, __NR_lchown),
	S(fchown)	= ABI_PASS("fchown", 3, __NR_fchown),
	S(__posix_fchown) = ABI_PASS("fchown", 3, __NR_fchown),
	S(fchownat)	= ABI_EMUL("fchownat", 5, bsd_sys_fchownat),
	S(utimensat)	= ABI_EMUL("utimensat", 4, bsd_sys_utimensat),
	S(__utimes50)	= ABI_PASS("utimes", 2, __NR_utimes),
	S(pathconf)	= ABI_EMUL("pathconf", 2, netbsd_sys_pathconf),
	S(fpathconf)	= ABI_EMUL("fpathconf", 2, netbsd_sys_pathconf),
	S(lpathconf)	= ABI_EMUL("lpathconf", 2, netbsd_sys_pathconf),
	S(chflags)	= ABI_EMUL("chflags", 2, bsd_sys_zero),
	S(fchflags)	= ABI_EMUL("fchflags", 2, bsd_sys_zero),
	S(lchflags)	= ABI_EMUL("lchflags", 2, bsd_sys_zero),

	/* memory */
	S(mmap)		= ABI_EMUL("mmap", 7, netbsd_sys_mmap),
	S(munmap)	= ABI_PASS("munmap", 2, __NR_munmap),
	S(mprotect)	= ABI_PASS("mprotect", 3, __NR_mprotect),
	S(madvise)	= ABI_EMUL("madvise", 3, bsd_sys_madvise),
	S(__msync13)	= ABI_EMUL("msync", 3, netbsd_sys_msync),
	S(minherit)	= ABI_EMUL("minherit", 3, bsd_sys_zero),
	S(mlock)	= ABI_PASS("mlock", 2, __NR_mlock),
	S(munlock)	= ABI_PASS("munlock", 2, __NR_munlock),

	/* time and polling */
	S(__gettimeofday50) = ABI_PASS("gettimeofday", 2, __NR_gettimeofday),
	S(__clock_gettime50) = ABI_EMUL("clock_gettime", 2, netbsd_sys_clock_gettime),
	S(__clock_getres50) = ABI_EMUL("clock_getres", 2, netbsd_sys_clock_getres),
	S(clock_nanosleep) = ABI_EMUL("clock_nanosleep", 4, netbsd_sys_clock_nanosleep),
	S(__nanosleep50) = ABI_PASS("nanosleep", 2, __NR_nanosleep),
	S(__getitimer50) = ABI_PASS("getitimer", 2, __NR_getitimer),
	S(__setitimer50) = ABI_PASS("setitimer", 3, __NR_setitimer),
	S(__select50)	= ABI_PASS("select", 5, __NR_select),
	S(poll)		= ABI_PASS("poll", 3, __NR_poll),
	S(__pollts50)	= ABI_EMUL("pollts", 4, netbsd_sys_pollts),
};

const unsigned int netbsd_nsysent = ARRAY_SIZE(netbsd_sysent);
