// SPDX-License-Identifier: GPL-2.0
/*
 * abi_illumos - system call table
 */
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/cred.h>
#include <linux/kdev_t.h>
#include <linux/utsname.h>
#include <linux/cpumask.h>
#include <linux/termios.h>
#include <linux/resource.h>
#include <linux/sched/signal.h>
#include <asm/unistd.h>
#include <asm/prctl.h>
#include <asm/ioctls.h>

#include "illumos.h"

#define IL_RELEASE	"5.11"
#define IL_VERSION	"illumos-abiemu"

static long il_zero(struct abi_ctx *ctx) { return 0; }
static long il_nosys(struct abi_ctx *ctx) { return -ENOSYS; }

/*
 * Flags
 */
static const struct { unsigned int il, lx; } il_oflags[] = {
	{ 0x000004, O_NDELAY }, { 0x000008, O_APPEND }, { 0x000010, O_SYNC },
	{ 0x000040, O_DSYNC }, { 0x000080, O_NONBLOCK }, { 0x000100, O_CREAT },
	{ 0x000200, O_TRUNC }, { 0x000400, O_EXCL }, { 0x000800, O_NOCTTY },
	{ 0x002000, O_LARGEFILE }, { 0x020000, O_NOFOLLOW },
	{ 0x200000, O_PATH }, { 0x800000, O_CLOEXEC },
	{ 0x1000000, O_DIRECTORY }, { 0x2000000, O_DIRECT },
};

static unsigned int il_oflags_to_linux(unsigned int f)
{
	unsigned int out = f & 3;
	int i;

	for (i = 0; i < ARRAY_SIZE(il_oflags); i++)
		if (f & il_oflags[i].il)
			out |= il_oflags[i].lx;
	return out;
}

static unsigned int il_oflags_from_linux(unsigned int f)
{
	unsigned int out = f & 3;
	int i;

	for (i = 0; i < ARRAY_SIZE(il_oflags); i++)
		if ((f & il_oflags[i].lx) == il_oflags[i].lx && il_oflags[i].lx)
			out |= il_oflags[i].il;
	return out;
}

/* AT_REMOVEDIR 1, AT_EACCESS 4, AT_SYMLINK_NOFOLLOW 0x1000, _FOLLOW 0x2000 */
static unsigned int il_atflags(unsigned int f)
{
	return ((f & 0x0001) ? AT_REMOVEDIR : 0) |
	       ((f & 0x0004) ? AT_EACCESS : 0) |
	       ((f & 0x1000) ? AT_SYMLINK_NOFOLLOW : 0) |
	       ((f & 0x2000) ? AT_SYMLINK_FOLLOW : 0);
}

/*
 * Processes and identity
 */
static long il_getpid(struct abi_ctx *ctx)
{
	pid_t ppid;

	rcu_read_lock();
	ppid = task_tgid_vnr(rcu_dereference(current->real_parent));
	rcu_read_unlock();
	illumos_retval2(ctx, ppid);
	return task_tgid_vnr(current);
}

static long il_getuid(struct abi_ctx *ctx)
{
	illumos_retval2(ctx, from_kuid_munged(current_user_ns(), current_euid()));
	return from_kuid_munged(current_user_ns(), current_uid());
}

static long il_getgid(struct abi_ctx *ctx)
{
	illumos_retval2(ctx, from_kgid_munged(current_user_ns(), current_egid()));
	return from_kgid_munged(current_user_ns(), current_gid());
}

static long il_exit(struct abi_ctx *ctx)
{
	return abi_native1(ctx, __NR_exit_group, ctx->args[0]);
}

static long il_fork_post(struct abi_ctx *ctx, long ret)
{
	if (!abi_is_error(ret))
		illumos_retval2(ctx, 0);
	return ret;
}

/* forksys(int subcode, int flags): 0 forkx, 1 forkallx, 2 vforkx */
static long il_forksys(struct abi_ctx *ctx)
{
	ctx->post = il_fork_post;
	if (ctx->args[0] == 2)
		return abi_native0(ctx, __NR_vfork);
	return abi_native0(ctx, __NR_fork);
}

static long il_vfork(struct abi_ctx *ctx)
{
	ctx->post = il_fork_post;
	return abi_native0(ctx, __NR_vfork);
}

/* pgrpsys(int subcode, ...) */
static long il_pgrpsys(struct abi_ctx *ctx)
{
	switch ((int)ctx->args[0]) {
	case 0: return abi_native0(ctx, __NR_getpgrp);
	case 1: return abi_native2(ctx, __NR_setpgid, 0, 0);	/* setpgrp */
	case 2: return abi_native1(ctx, __NR_getsid, ctx->args[1]);
	case 3: return abi_native0(ctx, __NR_setsid);
	case 4: return abi_native1(ctx, __NR_getpgid, ctx->args[1]);
	case 5: return abi_native2(ctx, __NR_setpgid, ctx->args[1], ctx->args[2]);
	}
	return -EINVAL;
}

/* lwp_private(int op, int which, uintptr_t base) */
static long il_lwp_private_post(struct abi_ctx *ctx, long ret)
{
	unsigned long v;

	if (ret)
		return ret;
	return get_user(v, (unsigned long __user *)ctx->priv[0]) ? -EFAULT : v;
}

static long il_lwp_private(struct abi_ctx *ctx)
{
	bool gs = ctx->args[1] == 1;

	if (ctx->args[0] == 0)
		return abi_native2(ctx, __NR_arch_prctl,
				   gs ? ARCH_SET_GS : ARCH_SET_FS, ctx->args[2]);
	if (ctx->args[0] == 1) {
		void __user *g = abi_gap(ctx, sizeof(long));

		ctx->priv[0] = (unsigned long)g;
		ctx->post = il_lwp_private_post;
		return abi_native2(ctx, __NR_arch_prctl,
				   gs ? ARCH_GET_GS : ARCH_GET_FS,
				   (unsigned long)g);
	}
	return -EINVAL;
}

/* The main thread is LWP 1 on illumos. */
static long il_lwp_self(struct abi_ctx *ctx)
{
	return task_pid_vnr(current) - task_tgid_vnr(current) + 1;
}

/*
 * Resource limits: RLIM_INFINITY is (rlim_t)-3
 */
#define IL_RLIM_INFINITY	((u64)-3)

static int il_rlimit(unsigned int r)
{
	static const signed char map[] = {
		RLIMIT_CPU, RLIMIT_FSIZE, RLIMIT_DATA, RLIMIT_STACK,
		RLIMIT_CORE, RLIMIT_NOFILE, RLIMIT_AS,
	};

	return r < ARRAY_SIZE(map) ? map[r] : -1;
}

static long il_getrlimit_post(struct abi_ctx *ctx, long ret)
{
	struct rlimit __user *u = (void __user *)ctx->args[1];
	struct rlimit r;

	if (ret)
		return ret;
	if (copy_from_user(&r, u, sizeof(r)))
		return -EFAULT;
	if (r.rlim_cur == RLIM_INFINITY)
		r.rlim_cur = IL_RLIM_INFINITY;
	if (r.rlim_max == RLIM_INFINITY)
		r.rlim_max = IL_RLIM_INFINITY;
	return copy_to_user(u, &r, sizeof(r)) ? -EFAULT : 0;
}

static long il_getrlimit(struct abi_ctx *ctx)
{
	int r = il_rlimit(ctx->args[0]);

	if (r < 0)
		return -EINVAL;
	ctx->post = il_getrlimit_post;
	return abi_native2(ctx, __NR_getrlimit, r, ctx->args[1]);
}

static long il_setrlimit(struct abi_ctx *ctx)
{
	int r = il_rlimit(ctx->args[0]);
	struct rlimit __user *g;
	struct rlimit rl;

	if (r < 0)
		return -EINVAL;
	if (copy_from_user(&rl, (void __user *)ctx->args[1], sizeof(rl)))
		return -EFAULT;
	if (rl.rlim_cur == IL_RLIM_INFINITY)
		rl.rlim_cur = RLIM_INFINITY;
	if (rl.rlim_max == IL_RLIM_INFINITY)
		rl.rlim_max = RLIM_INFINITY;
	g = abi_gap(ctx, sizeof(rl));
	if (copy_to_user(g, &rl, sizeof(rl)))
		return -EFAULT;
	return abi_native2(ctx, __NR_setrlimit, r, (unsigned long)g);
}

/*
 * System information
 */
static long il_put_str(void __user *buf, size_t len, const char *s)
{
	size_t n = strlen(s) + 1;

	if (len && copy_to_user(buf, s, min(n, len)))
		return -EFAULT;
	if (len && len < n && put_user(0, (char __user *)buf + len - 1))
		return -EFAULT;
	return n;
}

/* uname(struct utsname *): five fields of 257 bytes */
static long il_uname(struct abi_ctx *ctx)
{
	char __user *u = (char __user *)ctx->args[0];
	const char *f[5] = { "SunOS", utsname()->nodename, IL_RELEASE,
			     IL_VERSION, "i86pc" };
	int i;

	for (i = 0; i < 5; i++)
		if (il_put_str(u + 257 * i, 257, f[i]) < 0)
			return -EFAULT;
	return 1;
}

/* systeminfo(int command, char *buf, long count) */
static long il_systeminfo(struct abi_ctx *ctx)
{
	void __user *buf = (void __user *)ctx->args[1];
	size_t len = ctx->args[2];
	const char *s;

	switch ((int)ctx->args[0]) {
	case 1:		s = "SunOS"; break;
	case 2:		s = utsname()->nodename; break;
	case 3:		s = IL_RELEASE; break;
	case 4:		s = IL_VERSION; break;
	case 5:		s = "i86pc"; break;
	case 6:		s = "i386"; break;
	case 7:		s = "0"; break;
	case 8:		s = "abiemu"; break;
	case 9:		s = utsname()->domainname; break;
	case 513:	s = "i86pc"; break;
	case 514:	s = "amd64 pentium_pro+mmx pentium_pro pentium+mmx pentium i486 i386 i86"; break;
	case 516:	s = "i386"; break;
	case 517:
	case 518:
	case 519:	s = "amd64"; break;
	case 520:	s = "64"; break;
	default:	return -EINVAL;
	}
	return il_put_str(buf, len, s);
}

/* sysconfig(int name) */
static long il_sysconfig(struct abi_ctx *ctx)
{
	switch ((int)ctx->args[0]) {
	case 2:		return NGROUPS_MAX;
	case 3:		return task_rlimit(current, RLIMIT_NPROC);
	case 4:		return rlimit(RLIMIT_NOFILE);
	case 5:		return 200112;			/* _POSIX_VERSION */
	case 6:		return PAGE_SIZE;
	case 7:
	case 10:	return USER_HZ;
	case 8:		return 600;			/* _XOPEN_VERSION */
	case 11:
	case 45:	return num_possible_cpus();
	case 12:	return num_online_cpus();
	case 19:	return SIGRTMAX - SIGRTMIN - 1;
	case 23:	return 42;			/* SIGRTMIN */
	case 24:	return 73;			/* SIGRTMAX */
	case 26:
	case 27:	return totalram_pages();
	case 42:	return PID_MAX_DEFAULT;
	case 43:	return PROT_READ | PROT_WRITE;	/* _CONFIG_STACK_PROT */
	}
	return -EINVAL;
}

/* getpagesizes(size_t *buf, int nelem) */
static long il_getpagesizes(struct abi_ctx *ctx)
{
	if (!ctx->args[0] || !ctx->args[1])
		return 1;
	return put_user((unsigned long)PAGE_SIZE,
			(unsigned long __user *)ctx->args[0]) ? -EFAULT : 1;
}

/*
 * Files
 */
static long il_open(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_openat, AT_FDCWD, ctx->args[0],
			   il_oflags_to_linux(ctx->args[1]), ctx->args[2]);
}

static long il_openat(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_openat, illumos_dirfd(ctx->args[0]),
			   ctx->args[1], il_oflags_to_linux(ctx->args[2]),
			   ctx->args[3]);
}

static long il_unlinkat(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_unlinkat, illumos_dirfd(ctx->args[0]),
			   ctx->args[1], il_atflags(ctx->args[2]) & AT_REMOVEDIR);
}

static long il_mkdirat(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_mkdirat, illumos_dirfd(ctx->args[0]),
			   ctx->args[1], ctx->args[2]);
}

static long il_mknodat(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_mknodat, illumos_dirfd(ctx->args[0]),
			   ctx->args[1], ctx->args[2], ctx->args[3]);
}

static long il_renameat(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_renameat, illumos_dirfd(ctx->args[0]),
			   ctx->args[1], illumos_dirfd(ctx->args[2]),
			   ctx->args[3]);
}

static long il_readlinkat(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_readlinkat, illumos_dirfd(ctx->args[0]),
			   ctx->args[1], ctx->args[2], ctx->args[3]);
}

static long il_symlinkat(struct abi_ctx *ctx)	/* (target, fd, path) */
{
	return abi_native3(ctx, __NR_symlinkat, ctx->args[0],
			   illumos_dirfd(ctx->args[1]), ctx->args[2]);
}

static long il_faccessat(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_faccessat2, illumos_dirfd(ctx->args[0]),
			   ctx->args[1], ctx->args[2],
			   il_atflags(ctx->args[3]) & AT_EACCESS);
}

/* fchownat(fd, path, uid, gid, flag); path NULL means fd */
static long il_fchownat(struct abi_ctx *ctx)
{
	if (!ctx->args[1])
		return abi_native3(ctx, __NR_fchown, ctx->args[0],
				   ctx->args[2], ctx->args[3]);
	return abi_native5(ctx, __NR_fchownat, illumos_dirfd(ctx->args[0]),
			   ctx->args[1], ctx->args[2], ctx->args[3],
			   il_atflags(ctx->args[4]) & AT_SYMLINK_NOFOLLOW);
}

static long il_getcwd_post(struct abi_ctx *ctx, long ret)
{
	return ret > 0 ? 0 : ret;
}

static long il_getcwd(struct abi_ctx *ctx)
{
	ctx->post = il_getcwd_post;
	return abi_native2(ctx, __NR_getcwd, ctx->args[0], ctx->args[1]);
}

/* struct stat for LP64: illumos dev_t is major << 32 | minor */
struct illumos_stat {
	u64	st_dev;
	u64	st_ino;
	u32	st_mode;
	u32	st_nlink;
	u32	st_uid;
	u32	st_gid;
	u64	st_rdev;
	s64	st_size;
	struct __kernel_timespec st_atim, st_mtim, st_ctim;
	s32	st_blksize;
	s64	st_blocks;
	char	st_fstype[16];
};

static u64 il_dev(dev_t d)
{
	return ((u64)MAJOR(d) << 32) | MINOR(d);
}

static long il_put_stat(const struct kstat *k, void __user *u)
{
	struct illumos_stat st;

	memset(&st, 0, sizeof(st));
	st.st_dev = il_dev(k->dev);
	st.st_ino = k->ino;
	st.st_mode = k->mode;
	st.st_nlink = k->nlink;
	st.st_uid = from_kuid_munged(current_user_ns(), k->uid);
	st.st_gid = from_kgid_munged(current_user_ns(), k->gid);
	st.st_rdev = il_dev(k->rdev);
	st.st_size = k->size;
	st.st_atim = (struct __kernel_timespec){ k->atime.tv_sec, k->atime.tv_nsec };
	st.st_mtim = (struct __kernel_timespec){ k->mtime.tv_sec, k->mtime.tv_nsec };
	st.st_ctim = (struct __kernel_timespec){ k->ctime.tv_sec, k->ctime.tv_nsec };
	st.st_blksize = k->blksize;
	st.st_blocks = k->blocks;
	strscpy(st.st_fstype, "zfs", sizeof(st.st_fstype));
	return copy_to_user(u, &st, sizeof(st)) ? -EFAULT : 0;
}

static long il_fstat(struct abi_ctx *ctx)
{
	struct kstat k;
	int err = abi_stat_fd(ctx->args[0], &k);

	return err ?: il_put_stat(&k, (void __user *)ctx->args[1]);
}

static long il_stat_common(int dfd, unsigned long path, bool follow,
			   unsigned long buf)
{
	struct kstat k;
	int err = abi_stat_path(dfd, (void __user *)path, follow, false, &k);

	return err ?: il_put_stat(&k, (void __user *)buf);
}

static long il_stat(struct abi_ctx *ctx)
{
	return il_stat_common(AT_FDCWD, ctx->args[0], true, ctx->args[1]);
}

static long il_lstat(struct abi_ctx *ctx)
{
	return il_stat_common(AT_FDCWD, ctx->args[0], false, ctx->args[1]);
}

/* fstatat(fd, path, buf, flag): a NULL path means the fd itself */
static long il_fstatat(struct abi_ctx *ctx)
{
	if (!ctx->args[1]) {
		struct kstat k;
		int err = abi_stat_fd(ctx->args[0], &k);

		return err ?: il_put_stat(&k, (void __user *)ctx->args[2]);
	}
	return il_stat_common(illumos_dirfd(ctx->args[0]), ctx->args[1],
			      !(ctx->args[3] & 0x1000), ctx->args[2]);
}

/* struct dirent { ino_t d_ino; off_t d_off; u16 d_reclen; char d_name[]; } */
static size_t il_emit_dirent(void *buf, size_t room, const char *name,
			     int namlen, u64 ino, unsigned int type, loff_t next)
{
	size_t reclen = ALIGN(18 + namlen + 1, 8);
	u8 *d = buf;

	if (reclen > room)
		return 0;
	memset(d, 0, reclen);
	*(u64 *)d = ino;
	*(s64 *)(d + 8) = next;
	*(u16 *)(d + 16) = reclen;
	memcpy(d + 18, name, namlen);
	return reclen;
}

static long il_getdents(struct abi_ctx *ctx)
{
	return abi_getdents(ctx->args[0], (void __user *)ctx->args[1],
			    ctx->args[2], NULL, il_emit_dirent);
}

/* pipe(int *fds, int flags), or the old form returning both in registers */
static long il_pipe_post(struct abi_ctx *ctx, long ret)
{
	int fds[2];

	if (ret)
		return ret;
	if (copy_from_user(fds, (void __user *)ctx->priv[0], sizeof(fds)))
		return -EFAULT;
	illumos_retval2(ctx, fds[1]);
	return fds[0];
}

static long il_pipe(struct abi_ctx *ctx)
{
	unsigned int fl = il_oflags_to_linux(ctx->args[1]) &
			  (O_CLOEXEC | O_NONBLOCK);
	void __user *g;

	if (ctx->args[0])
		return abi_native2(ctx, __NR_pipe2, ctx->args[0], fl);
	g = abi_gap(ctx, 2 * sizeof(int));
	ctx->priv[0] = (unsigned long)g;
	ctx->post = il_pipe_post;
	return abi_native2(ctx, __NR_pipe2, (unsigned long)g, 0);
}

static long il_getfl_post(struct abi_ctx *ctx, long ret)
{
	return ret < 0 ? ret : il_oflags_from_linux(ret);
}

static long il_fcntl(struct abi_ctx *ctx)
{
	unsigned long fd = ctx->args[0], arg = ctx->args[2];

	switch ((int)ctx->args[1]) {
	case 0:  return abi_native3(ctx, __NR_fcntl, fd, F_DUPFD, arg);
	case 1:  return abi_native3(ctx, __NR_fcntl, fd, F_GETFD, 0);
	case 2:  return abi_native3(ctx, __NR_fcntl, fd, F_SETFD, arg);
	case 3:
		ctx->post = il_getfl_post;
		return abi_native3(ctx, __NR_fcntl, fd, F_GETFL, 0);
	case 4:  return abi_native3(ctx, __NR_fcntl, fd, F_SETFL,
				    il_oflags_to_linux(arg));
	case 9:  return abi_native2(ctx, __NR_dup2, fd, arg);	/* F_DUP2FD */
	case 23: return abi_native3(ctx, __NR_fcntl, fd, F_GETOWN, 0);
	case 24: return abi_native3(ctx, __NR_fcntl, fd, F_SETOWN, arg);
	case 37: return abi_native3(ctx, __NR_fcntl, fd, F_DUPFD_CLOEXEC, arg);
	case 36: return abi_native3(ctx, __NR_dup3, fd, arg, O_CLOEXEC);
	}
	return -EINVAL;
}

/*
 * Terminals: struct termios { u32 flags[4]; cc_t c_cc[19]; }
 * The flag bits are the SVR4 ones, which Linux inherited; only the
 * c_cc slots and the ioctl numbers differ.
 */
#define IL_NCCS	19

struct illumos_termios {
	u32	c_iflag, c_oflag, c_cflag, c_lflag;
	u8	c_cc[IL_NCCS];
};

static const signed char il_cc[IL_NCCS] = {
	VINTR, VQUIT, VERASE, VKILL, VEOF, VEOL, VEOL2, VSWTC, VSTART,
	VSTOP, VSUSP, -1, VREPRINT, VDISCARD, VWERASE, VLNEXT, -1, -1, -1,
};

static long il_tcgets_post(struct abi_ctx *ctx, long ret)
{
	struct illumos_termios it = { 0 };
	struct termios lt;
	int i;

	if (ret)
		return ret;
	if (copy_from_user(&lt, (void __user *)ctx->priv[0], sizeof(lt)))
		return -EFAULT;
	it.c_iflag = lt.c_iflag;
	it.c_oflag = lt.c_oflag;
	it.c_cflag = lt.c_cflag & 0xffff;
	it.c_lflag = lt.c_lflag;
	for (i = 0; i < IL_NCCS; i++)
		if (il_cc[i] >= 0)
			it.c_cc[i] = lt.c_cc[il_cc[i]];
	/* in canonical mode VMIN/VTIME share the VEOF/VEOL slots */
	if (!(lt.c_lflag & ICANON)) {
		it.c_cc[4] = lt.c_cc[VMIN];
		it.c_cc[5] = lt.c_cc[VTIME];
	}
	return copy_to_user((void __user *)ctx->args[2], &it, sizeof(it)) ?
		-EFAULT : 0;
}

static long il_tcsets(struct abi_ctx *ctx, unsigned int lcmd)
{
	struct termios __user *g = abi_gap(ctx, sizeof(struct termios));
	struct illumos_termios it;
	struct termios lt;
	int i;

	if (copy_from_user(&it, (void __user *)ctx->args[2], sizeof(it)))
		return -EFAULT;
	memset(&lt, 0, sizeof(lt));
	lt.c_iflag = it.c_iflag;
	lt.c_oflag = it.c_oflag;
	lt.c_cflag = it.c_cflag;
	lt.c_lflag = it.c_lflag;
	for (i = 0; i < IL_NCCS; i++)
		if (il_cc[i] >= 0)
			lt.c_cc[il_cc[i]] = it.c_cc[i];
	if (!(it.c_lflag & ICANON)) {
		lt.c_cc[VMIN] = it.c_cc[4];
		lt.c_cc[VTIME] = it.c_cc[5];
		lt.c_cc[VEOF] = 4;
		lt.c_cc[VEOL] = 0;
	}
	if (copy_to_user(g, &lt, sizeof(lt)))
		return -EFAULT;
	return abi_native3(ctx, __NR_ioctl, ctx->args[0], lcmd, (unsigned long)g);
}

static long il_ioctl(struct abi_ctx *ctx)
{
	unsigned long fd = ctx->args[0], arg = ctx->args[2];
	u32 cmd = ctx->args[1];

	switch (cmd) {
	case 0x540d: {					/* TCGETS */
		void __user *g = abi_gap(ctx, sizeof(struct termios));

		ctx->priv[0] = (unsigned long)g;
		ctx->post = il_tcgets_post;
		return abi_native3(ctx, __NR_ioctl, fd, TCGETS, (unsigned long)g);
	}
	case 0x540e: return il_tcsets(ctx, TCSETS);
	case 0x540f: return il_tcsets(ctx, TCSETSW);
	case 0x5410: return il_tcsets(ctx, TCSETSF);
	case 0x5405: return abi_native3(ctx, __NR_ioctl, fd, TCSBRK, arg);
	case 0x5406: return abi_native3(ctx, __NR_ioctl, fd, TCXONC, arg);
	case 0x5407: return abi_native3(ctx, __NR_ioctl, fd, TCFLSH, arg);
	case 0x5468: return abi_native3(ctx, __NR_ioctl, fd, TIOCGWINSZ, arg);
	case 0x5467: return abi_native3(ctx, __NR_ioctl, fd, TIOCSWINSZ, arg);
	case 0x7414: return abi_native3(ctx, __NR_ioctl, fd, TIOCGPGRP, arg);
	case 0x7415: return abi_native3(ctx, __NR_ioctl, fd, TIOCSPGRP, arg);
	case 0x7484: return abi_native3(ctx, __NR_ioctl, fd, TIOCSCTTY, 0);
	case 0x7471: return abi_native3(ctx, __NR_ioctl, fd, TIOCNOTTY, 0);
	case 0x4004667f: return abi_native3(ctx, __NR_ioctl, fd, FIONREAD, arg);
	case 0x8004667e: return abi_native3(ctx, __NR_ioctl, fd, FIONBIO, arg);
	case 0x20006601: return abi_native3(ctx, __NR_ioctl, fd, FIOCLEX, 0);
	case 0x20006602: return abi_native3(ctx, __NR_ioctl, fd, FIONCLEX, 0);
	}
	if (abi_trace)
		pr_info("abiemu: illumos[%d]: unsupported ioctl 0x%x\n",
			task_pid_nr(current), cmd);
	return -ENOTTY;
}

/*
 * Memory
 */
static long il_mmap(struct abi_ctx *ctx)
{
	unsigned int flags = ctx->args[3];
	int prot = ctx->args[2] & (PROT_READ | PROT_WRITE | PROT_EXEC);
	unsigned long lflags, addr = ctx->args[0];
	struct file *file = NULL;
	long ret;

	lflags = (flags & 0xf) == 1 ? MAP_SHARED : MAP_PRIVATE;
	if (flags & 0x10)
		lflags |= MAP_FIXED;
	if (flags & 0x40)
		lflags |= MAP_NORESERVE;
	if (flags & 0x100)
		lflags |= MAP_ANONYMOUS;
	if (flags & 0x200)	/* MAP_ALIGN: addr is the alignment */
		addr = 0;
	if (!(lflags & MAP_ANONYMOUS)) {
		file = fget(ctx->args[4]);
		if (!file)
			return -EBADF;
	}
	ret = vm_mmap(file, addr, ctx->args[1], prot, lflags,
		      (lflags & MAP_ANONYMOUS) ? 0 : ctx->args[5]);
	if (file)
		fput(file);
	return ret;
}

/* brk(void *addr): 0 or -1/ENOMEM */
static long il_brk_post(struct abi_ctx *ctx, long ret)
{
	return (unsigned long)ret == ctx->args[0] ? 0 : -ENOMEM;
}

static long il_brk(struct abi_ctx *ctx)
{
	ctx->post = il_brk_post;
	return abi_native1(ctx, __NR_brk, ctx->args[0]);
}

/*
 * Time
 */
static int il_clockid(unsigned int id)
{
	switch (id) {
	case 1:	return CLOCK_PROCESS_CPUTIME_ID;	/* CLOCK_VIRTUAL */
	case 2:	return CLOCK_THREAD_CPUTIME_ID;
	case 3:	return CLOCK_REALTIME;
	case 4:	return CLOCK_MONOTONIC;
	case 5:	return CLOCK_PROCESS_CPUTIME_ID;
	}
	return -1;
}

static long il_clock_gettime(struct abi_ctx *ctx)
{
	int id = il_clockid(ctx->args[0]);

	return id < 0 ? -EINVAL :
		abi_native2(ctx, __NR_clock_gettime, id, ctx->args[1]);
}

static long il_time(struct abi_ctx *ctx)
{
	return ktime_get_real_seconds();
}

/* pollsys(fds, nfds, const timespec *, const sigset_t *) */
static long il_pollsys(struct abi_ctx *ctx)
{
	u64 __user *gset = NULL;
	u32 set[4];

	if (ctx->args[3]) {
		if (copy_from_user(set, (void __user *)ctx->args[3], sizeof(set)))
			return -EFAULT;
		gset = abi_gap(ctx, sizeof(u64));
		if (put_user(illumos_sigset_to_linux(set), gset))
			return -EFAULT;
	}
	return abi_native5(ctx, __NR_ppoll, ctx->args[0], ctx->args[1],
			   ctx->args[2], (unsigned long)gset, 8);
}

#define S(nr)	[ILLUMOS_SYS_##nr]

const struct abi_sysent illumos_sysent[] = {
	/* processes */
	S(exit)		= ABI_EMUL("exit", 1, il_exit),
	S(forksys)	= ABI_EMUL("forksys", 2, il_forksys),
	S(vfork)	= ABI_EMUL("vfork", 0, il_vfork),
	S(execve)	= ABI_PASS("execve", 3, __NR_execve),
	S(waitid)	= ABI_EMUL("waitsys", 4, illumos_sys_waitsys),
	S(getpid)	= ABI_EMUL("getpid", 0, il_getpid),
	S(getuid)	= ABI_EMUL("getuid", 0, il_getuid),
	S(getgid)	= ABI_EMUL("getgid", 0, il_getgid),
	S(setuid)	= ABI_PASS("setuid", 1, __NR_setuid),
	S(setgid)	= ABI_PASS("setgid", 1, __NR_setgid),
	S(pgrpsys)	= ABI_EMUL("pgrpsys", 3, il_pgrpsys),
	S(umask)	= ABI_PASS("umask", 1, __NR_umask),
	S(getrlimit)	= ABI_EMUL("getrlimit", 2, il_getrlimit),
	S(setrlimit)	= ABI_EMUL("setrlimit", 2, il_setrlimit),
	S(lwp_private)	= ABI_EMUL("lwp_private", 3, il_lwp_private),
	S(lwp_self)	= ABI_EMUL("lwp_self", 0, il_lwp_self),
	S(uname)	= ABI_EMUL("uname", 1, il_uname),
	S(systeminfo)	= ABI_EMUL("systeminfo", 3, il_systeminfo),
	S(sysconfig)	= ABI_EMUL("sysconfig", 1, il_sysconfig),
	S(getpagesizes)	= ABI_EMUL("getpagesizes", 2, il_getpagesizes),
	S(memcntl)	= ABI_EMUL("memcntl", 6, il_zero),
	S(privsys)	= ABI_EMUL("privsys", 6, il_nosys),
	S(getrandom)	= ABI_PASS("getrandom", 3, __NR_getrandom),

	/* signals */
	S(kill)		= ABI_EMUL("kill", 2, illumos_sys_kill),
	S(lwp_kill)	= ABI_EMUL("lwp_kill", 2, illumos_sys_lwp_kill),
	S(sigaction)	= ABI_EMUL("sigaction", 3, illumos_sys_sigaction),
	S(sigprocmask)	= ABI_EMUL("sigprocmask", 3, illumos_sys_sigprocmask),
	S(lwp_sigmask)	= ABI_EMUL("lwp_sigmask", 4, illumos_sys_lwp_sigmask),
	S(sigsuspend)	= ABI_EMUL("sigsuspend", 1, illumos_sys_sigsuspend),
	S(sigpending)	= ABI_EMUL("sigpending", 2, illumos_sys_sigpending),
	S(sigaltstack)	= ABI_EMUL("sigaltstack", 2, illumos_sys_sigaltstack),
	S(context)	= ABI_EMUL("context", 2, illumos_sys_context),

	/* files */
	S(read)		= ABI_PASS("read", 3, __NR_read),
	S(write)	= ABI_PASS("write", 3, __NR_write),
	S(readv)	= ABI_PASS("readv", 3, __NR_readv),
	S(writev)	= ABI_PASS("writev", 3, __NR_writev),
	S(pread)	= ABI_PASS("pread", 4, __NR_pread64),
	S(pwrite)	= ABI_PASS("pwrite", 4, __NR_pwrite64),
	S(open)		= ABI_EMUL("open", 3, il_open),
	S(openat)	= ABI_EMUL("openat", 4, il_openat),
	S(close)	= ABI_PASS("close", 1, __NR_close),
	S(pipe)		= ABI_EMUL("pipe", 2, il_pipe),
	S(fcntl)	= ABI_EMUL("fcntl", 3, il_fcntl),
	S(ioctl)	= ABI_EMUL("ioctl", 3, il_ioctl),
	S(lseek)	= ABI_PASS("lseek", 3, __NR_lseek),
	S(fdsync)	= ABI_PASS("fdsync", 1, __NR_fsync),
	S(stat)		= ABI_EMUL("stat", 2, il_stat),
	S(lstat)	= ABI_EMUL("lstat", 2, il_lstat),
	S(fstat)	= ABI_EMUL("fstat", 2, il_fstat),
	S(fstatat)	= ABI_EMUL("fstatat", 4, il_fstatat),
	S(getdents)	= ABI_EMUL("getdents", 3, il_getdents),
	S(access)	= ABI_PASS("access", 2, __NR_access),
	S(faccessat)	= ABI_EMUL("faccessat", 4, il_faccessat),
	S(readlink)	= ABI_PASS("readlink", 3, __NR_readlink),
	S(readlinkat)	= ABI_EMUL("readlinkat", 4, il_readlinkat),
	S(chdir)	= ABI_PASS("chdir", 1, __NR_chdir),
	S(fchdir)	= ABI_PASS("fchdir", 1, __NR_fchdir),
	S(chroot)	= ABI_PASS("chroot", 1, __NR_chroot),
	S(getcwd)	= ABI_EMUL("getcwd", 2, il_getcwd),
	S(unlink)	= ABI_PASS("unlink", 1, __NR_unlink),
	S(unlinkat)	= ABI_EMUL("unlinkat", 3, il_unlinkat),
	S(rmdir)	= ABI_PASS("rmdir", 1, __NR_rmdir),
	S(mkdir)	= ABI_PASS("mkdir", 2, __NR_mkdir),
	S(mkdirat)	= ABI_EMUL("mkdirat", 3, il_mkdirat),
	S(mknod)	= ABI_PASS("mknod", 3, __NR_mknod),
	S(mknodat)	= ABI_EMUL("mknodat", 4, il_mknodat),
	S(rename)	= ABI_PASS("rename", 2, __NR_rename),
	S(renameat)	= ABI_EMUL("renameat", 4, il_renameat),
	S(link)		= ABI_PASS("link", 2, __NR_link),
	S(symlink)	= ABI_PASS("symlink", 2, __NR_symlink),
	S(symlinkat)	= ABI_EMUL("symlinkat", 3, il_symlinkat),
	S(chmod)	= ABI_PASS("chmod", 2, __NR_chmod),
	S(fchmod)	= ABI_PASS("fchmod", 2, __NR_fchmod),
	S(chown)	= ABI_PASS("chown", 3, __NR_chown),
	S(fchown)	= ABI_PASS("fchown", 3, __NR_fchown),
	S(fchownat)	= ABI_EMUL("fchownat", 5, il_fchownat),

	/* memory */
	S(mmap)		= ABI_EMUL("mmap", 6, il_mmap),
	S(munmap)	= ABI_PASS("munmap", 2, __NR_munmap),
	S(mprotect)	= ABI_PASS("mprotect", 3, __NR_mprotect),
	S(brk)		= ABI_EMUL("brk", 1, il_brk),

	/* time and polling */
	S(time)		= ABI_EMUL("time", 0, il_time),
	S(gettimeofday)	= ABI_PASS("gettimeofday", 1, __NR_gettimeofday),
	S(clock_gettime) = ABI_EMUL("clock_gettime", 2, il_clock_gettime),
	S(nanosleep)	= ABI_PASS("nanosleep", 2, __NR_nanosleep),
	S(pollsys)	= ABI_EMUL("pollsys", 4, il_pollsys),
};

const unsigned int illumos_nsysent = ARRAY_SIZE(illumos_sysent);
