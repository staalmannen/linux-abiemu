// SPDX-License-Identifier: GPL-2.0
/*
 * abi_openbsd - system call table and OpenBSD specific calls
 */
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/statfs.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <linux/kdev_t.h>
#include <linux/utsname.h>
#include <linux/cpumask.h>
#include <linux/sched/signal.h>
#include <asm/unistd.h>
#include <asm/prctl.h>
#include <asm/ioctls.h>

#include "openbsd.h"

/* OpenBSD's dev_t is 32 bits wide */
static u32 openbsd_dev(dev_t d)
{
	u32 ma = MAJOR(d), mi = MINOR(d);

	return ((ma & 0xff) << 8) | (mi & 0xff) | ((mi & 0xffff00) << 8);
}

struct openbsd_stat {
	u32	st_mode;
	s32	st_dev;
	u64	st_ino;
	u32	st_nlink;
	u32	st_uid;
	u32	st_gid;
	s32	st_rdev;
	struct __kernel_timespec st_atim, st_mtim, st_ctim;
	s64	st_size;
	s64	st_blocks;
	s32	st_blksize;
	u32	st_flags;
	u32	st_gen;
	struct __kernel_timespec st_birthtim;
};

static long openbsd_put_stat(const struct kstat *k, void __user *ubuf)
{
	struct openbsd_stat st;

	memset(&st, 0, sizeof(st));
	st.st_mode = k->mode;
	st.st_dev = openbsd_dev(k->dev);
	st.st_ino = k->ino;
	st.st_nlink = k->nlink;
	st.st_uid = from_kuid_munged(current_user_ns(), k->uid);
	st.st_gid = from_kgid_munged(current_user_ns(), k->gid);
	st.st_rdev = openbsd_dev(k->rdev);
	st.st_atim = (struct __kernel_timespec){ k->atime.tv_sec, k->atime.tv_nsec };
	st.st_mtim = (struct __kernel_timespec){ k->mtime.tv_sec, k->mtime.tv_nsec };
	st.st_ctim = (struct __kernel_timespec){ k->ctime.tv_sec, k->ctime.tv_nsec };
	st.st_birthtim = st.st_ctim;
	st.st_size = k->size;
	st.st_blocks = k->blocks;
	st.st_blksize = k->blksize;
	return copy_to_user(ubuf, &st, sizeof(st)) ? -EFAULT : 0;
}

static long openbsd_sys_fstat(struct abi_ctx *ctx)
{
	struct kstat k;
	int err = abi_stat_fd(ctx->args[0], &k);

	return err ?: openbsd_put_stat(&k, (void __user *)ctx->args[1]);
}

static long openbsd_sys_stat(struct abi_ctx *ctx)
{
	struct kstat k;
	int err = abi_stat_path(AT_FDCWD, (void __user *)ctx->args[0], true,
				false, &k);

	return err ?: openbsd_put_stat(&k, (void __user *)ctx->args[1]);
}

static long openbsd_sys_lstat(struct abi_ctx *ctx)
{
	struct kstat k;
	int err = abi_stat_path(AT_FDCWD, (void __user *)ctx->args[0], false,
				false, &k);

	return err ?: openbsd_put_stat(&k, (void __user *)ctx->args[1]);
}

/* fstatat(fd, path, buf, flag): AT_SYMLINK_NOFOLLOW is 2 */
static long openbsd_sys_fstatat(struct abi_ctx *ctx)
{
	struct kstat k;
	int err = abi_stat_path(ctx->args[0], (void __user *)ctx->args[1],
				!(ctx->args[3] & 2), false, &k);

	return err ?: openbsd_put_stat(&k, (void __user *)ctx->args[2]);
}

static long openbsd_sys_getdents(struct abi_ctx *ctx)
{
	return abi_getdents(ctx->args[0], (void __user *)ctx->args[1],
			    ctx->args[2], NULL, abi_bsd_emit_dirent_openbsd);
}

static long openbsd_sys_fcntl(struct abi_ctx *ctx)
{
	if ((int)ctx->args[1] == 11) {	/* F_ISATTY */
		ctx->args[1] = 0;
		return abi_native3(ctx, __NR_ioctl, ctx->args[0], TCGETS,
				   (unsigned long)abi_gap(ctx, 64));
	}
	return bsd_sys_fcntl(ctx);
}

static long openbsd_sys_dup3(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_dup3, ctx->args[0], ctx->args[1],
			   (ctx->args[2] & 0x10000) ? O_CLOEXEC : 0);
}

static long openbsd_sys_closefrom(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_close_range, ctx->args[0], ~0U, 0);
}

/* pipe(int fds[2]) */
static long openbsd_sys_pipe(struct abi_ctx *ctx)
{
	return abi_native2(ctx, __NR_pipe2, ctx->args[0], 0);
}

static long openbsd_sys_mkfifo(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_mknodat, AT_FDCWD, ctx->args[0],
			   S_IFIFO | (ctx->args[1] & 07777), 0);
}

/* getentropy(void *buf, size_t len): at most 256 bytes */
static long openbsd_sys_getentropy(struct abi_ctx *ctx)
{
	if (ctx->args[1] > 256)
		return -EIO;
	ctx->post = NULL;
	return abi_native3(ctx, __NR_getrandom, ctx->args[0], ctx->args[1], 0);
}

static long openbsd_sys_getthrid(struct abi_ctx *ctx)
{
	return task_pid_vnr(current);
}

static long openbsd_sys_set_tcb(struct abi_ctx *ctx)
{
	return abi_native2(ctx, __NR_arch_prctl, ARCH_SET_FS, ctx->args[0]);
}

static long openbsd_get_tcb_post(struct abi_ctx *ctx, long ret)
{
	unsigned long v;

	if (ret)
		return ret;
	return get_user(v, (unsigned long __user *)ctx->priv[0]) ? -EFAULT : v;
}

static long openbsd_sys_get_tcb(struct abi_ctx *ctx)
{
	void __user *g = abi_gap(ctx, sizeof(long));

	ctx->priv[0] = (unsigned long)g;
	ctx->post = openbsd_get_tcb_post;
	return abi_native2(ctx, __NR_arch_prctl, ARCH_GET_FS, (unsigned long)g);
}

static long openbsd_sys_seteuid(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_setresuid, -1, ctx->args[0], -1);
}

static long openbsd_sys_setegid(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_setresgid, -1, ctx->args[0], -1);
}

/*
 * kbind(const struct __kbind *param, size_t psize, int64_t cookie)
 *
 * ld.so's lazy binding: write into (read-only) PLT/GOT pages.
 */
struct openbsd_kbind {
	u64	kb_addr;
	u64	kb_size;
};

#define KBIND_BLOCK_MAX	2
#define KBIND_DATA_MAX	24

static long openbsd_sys_kbind(struct abi_ctx *ctx)
{
	u8 buf[KBIND_BLOCK_MAX * sizeof(struct openbsd_kbind) + KBIND_DATA_MAX];
	struct openbsd_kbind *kb = (void *)buf;
	size_t psize = ctx->args[1], s;
	int count, i;
	u8 *data;

	if (!ctx->args[0])
		return 0;
	if (psize < sizeof(*kb) || psize > sizeof(buf))
		return -EINVAL;
	if (copy_from_user(buf, (void __user *)ctx->args[0], psize))
		return -EFAULT;
	for (s = psize, count = 0; s > 0 && count < KBIND_BLOCK_MAX; count++) {
		if (s < sizeof(*kb))
			return -EINVAL;
		s -= sizeof(*kb);
		if (!kb[count].kb_addr || !kb[count].kb_size ||
		    kb[count].kb_size > KBIND_DATA_MAX || s < kb[count].kb_size)
			return -EINVAL;
		s -= kb[count].kb_size;
	}
	if (s)
		return -EINVAL;
	data = (u8 *)&kb[count];
	for (i = 0; i < count; i++) {
		if (access_process_vm(current, kb[i].kb_addr, data,
				      kb[i].kb_size,
				      FOLL_FORCE | FOLL_WRITE) != kb[i].kb_size)
			return -EFAULT;
		data += kb[i].kb_size;
	}
	return 0;
}

/*
 * Memory: mmap(addr, len, prot, flags, fd, pos)
 */
#define OPENBSD_MAP_STACK	0x4000

static long openbsd_sys_mmap(struct abi_ctx *ctx)
{
	unsigned int flags = ctx->args[3];
	int prot = ctx->args[2] & (PROT_READ | PROT_WRITE | PROT_EXEC);
	unsigned long lflags;

	lflags = (flags & BSD_MAP_SHARED) ? MAP_SHARED : MAP_PRIVATE;
	if (flags & BSD_MAP_FIXED)
		lflags |= MAP_FIXED;
	if (flags & BSD_MAP_ANON)
		lflags |= MAP_ANONYMOUS;
	if (flags & OPENBSD_MAP_STACK)
		lflags |= MAP_ANONYMOUS;
	return abi_bsd_mmap(ctx->args[0], ctx->args[1], prot, lflags,
			    ctx->args[4], ctx->args[5], 0);
}

/* msync(addr, len, flags): MS_ASYNC 1, MS_SYNC 2, MS_INVALIDATE 4 */
static long openbsd_sys_msync(struct abi_ctx *ctx)
{
	unsigned int f = ctx->args[2], lf = 0;

	lf |= (f & 1) ? MS_ASYNC : MS_SYNC;
	if (f & 4)
		lf |= MS_INVALIDATE;
	return abi_native3(ctx, __NR_msync, ctx->args[0], ctx->args[1], lf);
}

/*
 * Time
 */
static int openbsd_clockid(unsigned int id)
{
	switch (id) {
	case 0:	return CLOCK_REALTIME;
	case 2:	return CLOCK_PROCESS_CPUTIME_ID;
	case 3:	return CLOCK_MONOTONIC;
	case 4:	return CLOCK_THREAD_CPUTIME_ID;
	case 5:	return CLOCK_MONOTONIC;		/* CLOCK_UPTIME */
	case 6:	return CLOCK_BOOTTIME;
	}
	return -1;
}

static long openbsd_sys_clock_gettime(struct abi_ctx *ctx)
{
	int id = openbsd_clockid(ctx->args[0]);

	return id < 0 ? -EINVAL :
		abi_native2(ctx, __NR_clock_gettime, id, ctx->args[1]);
}

static long openbsd_sys_clock_getres(struct abi_ctx *ctx)
{
	int id = openbsd_clockid(ctx->args[0]);

	return id < 0 ? -EINVAL :
		abi_native2(ctx, __NR_clock_getres, id, ctx->args[1]);
}

/* ppoll(fds, nfds, ts, const sigset_t *mask) */
static long openbsd_sys_ppoll(struct abi_ctx *ctx)
{
	u64 __user *gset = NULL;
	u32 mask;

	if (ctx->args[3]) {
		if (get_user(mask, (u32 __user *)ctx->args[3]))
			return -EFAULT;
		gset = abi_gap(ctx, sizeof(u64));
		if (put_user(openbsd_sigset_to_linux(mask), gset))
			return -EFAULT;
	}
	return abi_native5(ctx, __NR_ppoll, ctx->args[0], ctx->args[1],
			   ctx->args[2], (unsigned long)gset, 8);
}

/*
 * sysctl
 */
static int openbsd_sysctl_value(const int *mib, unsigned int len, void *buf,
				size_t want)
{
	u64 mem = totalram_pages() << PAGE_SHIFT;

	if (len != 2)
		return -ENOENT;
	switch (mib[0] << 16 | mib[1]) {
	case 1 << 16 | 1:	return abi_sysctl_str(buf, "OpenBSD");
	case 1 << 16 | 2:	return abi_sysctl_str(buf, OPENBSD_OSRELEASE);
	case 1 << 16 | 3:	return abi_sysctl_int(buf, OPENBSD_OSREV);
	case 1 << 16 | 4:
		return scnprintf(buf, ABI_SYSCTL_BUFSZ, "OpenBSD " OPENBSD_OSRELEASE
				 " (abiemu on Linux %s)\n",
				 utsname()->release) + 1;
	case 1 << 16 | 27:	return abi_sysctl_str(buf, "ABIEMU#0");
	case 1 << 16 | 7:	return abi_sysctl_int(buf, rlimit(RLIMIT_NOFILE));
	case 1 << 16 | 8:	return abi_sysctl_int(buf, 512 * 1024);
	case 1 << 16 | 10:	return abi_sysctl_str(buf, utsname()->nodename);
	case 1 << 16 | 18:	return abi_sysctl_int(buf, NGROUPS_MAX);
	case 1 << 16 | 22:	return abi_sysctl_str(buf, utsname()->domainname);
	case 6 << 16 | 1:	return abi_sysctl_str(buf, "amd64");
	case 6 << 16 | 2:	return abi_sysctl_str(buf, "Linux abiemu virtual CPU");
	case 6 << 16 | 3:
	case 6 << 16 | 25:	return abi_sysctl_int(buf, num_online_cpus());
	case 6 << 16 | 4:	return abi_sysctl_int(buf, 1234);
	case 6 << 16 | 5:
	case 6 << 16 | 6:	return abi_sysctl_int(buf, min_t(u64, INT_MAX, mem));
	case 6 << 16 | 19:
	case 6 << 16 | 20:	return abi_sysctl_quad(buf, mem);
	case 6 << 16 | 7:	return abi_sysctl_int(buf, PAGE_SIZE);
	case 8 << 16 | 1:	return abi_sysctl_str(buf, "/usr/bin:/bin:/usr/sbin:/sbin");
	}
	return -ENOENT;
}

static long openbsd_sys_sysctl(struct abi_ctx *ctx)
{
	return abi_bsd_sysctl(ctx, openbsd_sysctl_value);
}

#define S(nr)	[OPENBSD_SYS_##nr]

const struct abi_sysent openbsd_sysent[] = {
	/* processes */
	S(exit)		= ABI_EMUL("exit", 1, bsd_sys_exit),
	S(fork)		= ABI_EMUL("fork", 0, bsd_sys_fork),
	S(vfork)	= ABI_EMUL("vfork", 0, bsd_sys_vfork),
	S(execve)	= ABI_PASS("execve", 3, __NR_execve),
	S(wait4)	= ABI_EMUL("wait4", 4, bsd_sys_wait4),
	S(getpid)	= ABI_PASS("getpid", 0, __NR_getpid),
	S(getppid)	= ABI_PASS("getppid", 0, __NR_getppid),
	S(getthrid)	= ABI_EMUL("getthrid", 0, openbsd_sys_getthrid),
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
	S(seteuid)	= ABI_EMUL("seteuid", 1, openbsd_sys_seteuid),
	S(setegid)	= ABI_EMUL("setegid", 1, openbsd_sys_setegid),
	S(setreuid)	= ABI_PASS("setreuid", 2, __NR_setreuid),
	S(setregid)	= ABI_PASS("setregid", 2, __NR_setregid),
	S(setresuid)	= ABI_PASS("setresuid", 3, __NR_setresuid),
	S(setresgid)	= ABI_PASS("setresgid", 3, __NR_setresgid),
	S(getresuid)	= ABI_PASS("getresuid", 3, __NR_getresuid),
	S(getresgid)	= ABI_PASS("getresgid", 3, __NR_getresgid),
	S(getgroups)	= ABI_PASS("getgroups", 2, __NR_getgroups),
	S(setgroups)	= ABI_PASS("setgroups", 2, __NR_setgroups),
	S(getlogin_r)	= ABI_EMUL("getlogin_r", 2, bsd_sys_getlogin),
	S(issetugid)	= ABI_EMUL("issetugid", 0, bsd_sys_zero),
	S(umask)	= ABI_PASS("umask", 1, __NR_umask),
	S(getrlimit)	= ABI_EMUL("getrlimit", 2, bsd_sys_getrlimit),
	S(setrlimit)	= ABI_EMUL("setrlimit", 2, bsd_sys_setrlimit),
	S(getrusage)	= ABI_PASS("getrusage", 2, __NR_getrusage),
	S(getpriority)	= ABI_PASS("getpriority", 2, __NR_getpriority),
	S(setpriority)	= ABI_PASS("setpriority", 3, __NR_setpriority),
	S(sched_yield)	= ABI_PASS("sched_yield", 0, __NR_sched_yield),
	S(sysctl)	= ABI_EMUL("sysctl", 6, openbsd_sys_sysctl),
	S(__set_tcb)	= ABI_EMUL("__set_tcb", 1, openbsd_sys_set_tcb),
	S(__get_tcb)	= ABI_EMUL("__get_tcb", 0, openbsd_sys_get_tcb),
	S(getentropy)	= ABI_EMUL("getentropy", 2, openbsd_sys_getentropy),
	S(kbind)	= ABI_EMUL("kbind", 3, openbsd_sys_kbind),
	/* security features with nothing to enforce here */
	S(pledge)	= ABI_EMUL("pledge", 2, bsd_sys_zero),
	S(unveil)	= ABI_EMUL("unveil", 2, bsd_sys_zero),
	S(pinsyscalls)	= ABI_EMUL("pinsyscalls", 2, bsd_sys_zero),
	S(mimmutable)	= ABI_EMUL("mimmutable", 2, bsd_sys_zero),
	S(utrace)	= ABI_EMUL("utrace", 3, bsd_sys_zero),
	S(setthrname)	= ABI_EMUL("setthrname", 2, bsd_sys_zero),
	S(sendsyslog)	= ABI_EMUL("sendsyslog", 3, bsd_sys_zero),

	/* signals */
	S(kill)		= ABI_EMUL("kill", 2, bsd_sys_kill),
	S(thrkill)	= ABI_EMUL("thrkill", 3, openbsd_sys_thrkill),
	S(sigaction)	= ABI_EMUL("sigaction", 3, openbsd_sys_sigaction),
	S(sigprocmask)	= ABI_EMUL("sigprocmask", 2, openbsd_sys_sigprocmask),
	S(sigsuspend)	= ABI_EMUL("sigsuspend", 1, openbsd_sys_sigsuspend),
	S(sigpending)	= ABI_EMUL("sigpending", 0, openbsd_sys_sigpending),
	S(sigaltstack)	= ABI_EMUL("sigaltstack", 2, bsd_sys_sigaltstack),
	S(sigreturn)	= ABI_EMUL("sigreturn", 1, openbsd_sys_sigreturn),

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
	S(closefrom)	= ABI_EMUL("closefrom", 1, openbsd_sys_closefrom),
	S(dup)		= ABI_PASS("dup", 1, __NR_dup),
	S(dup2)		= ABI_PASS("dup2", 2, __NR_dup2),
	S(dup3)		= ABI_EMUL("dup3", 3, openbsd_sys_dup3),
	S(pipe)		= ABI_EMUL("pipe", 1, openbsd_sys_pipe),
	S(pipe2)	= ABI_EMUL("pipe2", 2, bsd_sys_pipe2),
	S(fcntl)	= ABI_EMUL("fcntl", 3, openbsd_sys_fcntl),
	S(ioctl)	= ABI_EMUL("ioctl", 3, bsd_sys_ioctl),
	S(lseek)	= ABI_PASS("lseek", 3, __NR_lseek),
	S(truncate)	= ABI_PASS("truncate", 2, __NR_truncate),
	S(ftruncate)	= ABI_PASS("ftruncate", 2, __NR_ftruncate),
	S(fsync)	= ABI_PASS("fsync", 1, __NR_fsync),
	S(sync)		= ABI_PASS("sync", 0, __NR_sync),
	S(flock)	= ABI_PASS("flock", 2, __NR_flock),
	S(fstat)	= ABI_EMUL("fstat", 2, openbsd_sys_fstat),
	S(stat)		= ABI_EMUL("stat", 2, openbsd_sys_stat),
	S(lstat)	= ABI_EMUL("lstat", 2, openbsd_sys_lstat),
	S(fstatat)	= ABI_EMUL("fstatat", 4, openbsd_sys_fstatat),
	S(getdents)	= ABI_EMUL("getdents", 3, openbsd_sys_getdents),
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
	S(mkfifo)	= ABI_EMUL("mkfifo", 2, openbsd_sys_mkfifo),
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
	S(utimes)	= ABI_PASS("utimes", 2, __NR_utimes),
	S(chflags)	= ABI_EMUL("chflags", 2, bsd_sys_zero),
	S(fchflags)	= ABI_EMUL("fchflags", 2, bsd_sys_zero),

	/* memory */
	S(mmap)		= ABI_EMUL("mmap", 6, openbsd_sys_mmap),
	S(munmap)	= ABI_PASS("munmap", 2, __NR_munmap),
	S(mprotect)	= ABI_PASS("mprotect", 3, __NR_mprotect),
	S(madvise)	= ABI_EMUL("madvise", 3, bsd_sys_madvise),
	S(msync)	= ABI_EMUL("msync", 3, openbsd_sys_msync),
	S(minherit)	= ABI_EMUL("minherit", 3, bsd_sys_zero),
	S(mlock)	= ABI_PASS("mlock", 2, __NR_mlock),
	S(munlock)	= ABI_PASS("munlock", 2, __NR_munlock),

	/* time and polling */
	S(gettimeofday)	= ABI_PASS("gettimeofday", 2, __NR_gettimeofday),
	S(clock_gettime) = ABI_EMUL("clock_gettime", 2, openbsd_sys_clock_gettime),
	S(clock_getres)	= ABI_EMUL("clock_getres", 2, openbsd_sys_clock_getres),
	S(nanosleep)	= ABI_PASS("nanosleep", 2, __NR_nanosleep),
	S(getitimer)	= ABI_PASS("getitimer", 2, __NR_getitimer),
	S(setitimer)	= ABI_PASS("setitimer", 3, __NR_setitimer),
	S(select)	= ABI_PASS("select", 5, __NR_select),
	S(poll)		= ABI_PASS("poll", 3, __NR_poll),
	S(ppoll)	= ABI_EMUL("ppoll", 4, openbsd_sys_ppoll),
};

const unsigned int openbsd_nsysent = ARRAY_SIZE(openbsd_sysent);
