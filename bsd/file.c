// SPDX-License-Identifier: GPL-2.0
/*
 * abi_bsd - file related system calls shared by the BSD personalities
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <asm/unistd.h>

#include <abiemu/bsd.h>

static unsigned int bsd_oflags(struct abi_ctx *ctx, unsigned int flags)
{
	return (flags & BSD_O_ACCMODE) |
	       abi_flags_to_linux(abi_bsd_ops(ctx)->oflags, flags);
}

static unsigned int bsd_atflags(struct abi_ctx *ctx, unsigned int flags)
{
	return abi_flags_to_linux(abi_bsd_ops(ctx)->atflags, flags);
}

long bsd_sys_open(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_openat, AT_FDCWD, ctx->args[0],
			   bsd_oflags(ctx, ctx->args[1]), ctx->args[2]);
}
EXPORT_SYMBOL_GPL(bsd_sys_open);

long bsd_sys_openat(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_openat, ctx->args[0], ctx->args[1],
			   bsd_oflags(ctx, ctx->args[2]), ctx->args[3]);
}
EXPORT_SYMBOL_GPL(bsd_sys_openat);

long bsd_sys_access(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_faccessat, AT_FDCWD, ctx->args[0],
			   ctx->args[1]);
}
EXPORT_SYMBOL_GPL(bsd_sys_access);

/* faccessat(fd, path, mode, flag) */
long bsd_sys_faccessat(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_faccessat2, ctx->args[0], ctx->args[1],
			   ctx->args[2], bsd_atflags(ctx, ctx->args[3]));
}
EXPORT_SYMBOL_GPL(bsd_sys_faccessat);

/* unlinkat(fd, path, flag) */
long bsd_sys_unlinkat(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_unlinkat, ctx->args[0], ctx->args[1],
			   bsd_atflags(ctx, ctx->args[2]) & AT_REMOVEDIR);
}
EXPORT_SYMBOL_GPL(bsd_sys_unlinkat);

/* fchmodat(fd, path, mode, flag) */
long bsd_sys_fchmodat(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_fchmodat2, ctx->args[0], ctx->args[1],
			   ctx->args[2], bsd_atflags(ctx, ctx->args[3]) &
			   AT_SYMLINK_NOFOLLOW);
}
EXPORT_SYMBOL_GPL(bsd_sys_fchmodat);

/* fchownat(fd, path, uid, gid, flag) */
long bsd_sys_fchownat(struct abi_ctx *ctx)
{
	return abi_native5(ctx, __NR_fchownat, ctx->args[0], ctx->args[1],
			   ctx->args[2], ctx->args[3],
			   bsd_atflags(ctx, ctx->args[4]) &
			   (AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH));
}
EXPORT_SYMBOL_GPL(bsd_sys_fchownat);

/* linkat(fd1, path1, fd2, path2, flag) */
long bsd_sys_linkat(struct abi_ctx *ctx)
{
	return abi_native5(ctx, __NR_linkat, ctx->args[0], ctx->args[1],
			   ctx->args[2], ctx->args[3],
			   bsd_atflags(ctx, ctx->args[4]) & AT_SYMLINK_FOLLOW);
}
EXPORT_SYMBOL_GPL(bsd_sys_linkat);

/*
 * utimensat(fd, path, times[2], flag): UTIME_NOW/UTIME_OMIT are -1/-2
 * on the BSDs.
 */
long bsd_sys_utimensat(struct abi_ctx *ctx)
{
	struct __kernel_timespec ts[2];
	void __user *gts = NULL;
	int i;

	if (ctx->args[2]) {
		if (copy_from_user(ts, (void __user *)ctx->args[2], sizeof(ts)))
			return -EFAULT;
		for (i = 0; i < 2; i++) {
			if (ts[i].tv_nsec == -1)
				ts[i].tv_nsec = UTIME_NOW;
			else if (ts[i].tv_nsec == -2)
				ts[i].tv_nsec = UTIME_OMIT;
		}
		gts = abi_gap(ctx, sizeof(ts));
		if (copy_to_user(gts, ts, sizeof(ts)))
			return -EFAULT;
	}
	return abi_native4(ctx, __NR_utimensat, ctx->args[0], ctx->args[1],
			   (unsigned long)gts,
			   bsd_atflags(ctx, ctx->args[3]) & AT_SYMLINK_NOFOLLOW);
}
EXPORT_SYMBOL_GPL(bsd_sys_utimensat);

/* The original pipe() returns the descriptors in %rax and %rdx. */
static long bsd_pipe_post(struct abi_ctx *ctx, long ret)
{
	int fds[2];

	if (ret)
		return ret;
	if (copy_from_user(fds, (void __user *)ctx->priv[0], sizeof(fds)))
		return -EFAULT;
	abi_bsd_retval2(ctx, fds[1]);
	return fds[0];
}

long bsd_sys_pipe(struct abi_ctx *ctx)
{
	void __user *gfds = abi_gap(ctx, 2 * sizeof(int));

	ctx->priv[0] = (unsigned long)gfds;
	ctx->post = bsd_pipe_post;
	return abi_native2(ctx, __NR_pipe2, (unsigned long)gfds, 0);
}
EXPORT_SYMBOL_GPL(bsd_sys_pipe);

/* pipe2(int fds[2], int flags) */
long bsd_sys_pipe2(struct abi_ctx *ctx)
{
	return abi_native2(ctx, __NR_pipe2, ctx->args[0],
			   bsd_oflags(ctx, ctx->args[1]) &
			   (O_CLOEXEC | O_NONBLOCK));
}
EXPORT_SYMBOL_GPL(bsd_sys_pipe2);

static long bsd_getfl_post(struct abi_ctx *ctx, long ret)
{
	if (ret < 0)
		return ret;
	return (ret & O_ACCMODE) |
	       abi_flags_from_linux(abi_bsd_ops(ctx)->oflags, ret);
}

/*
 * fcntl(): commands 0-6 are the same everywhere.  Record locking is
 * not translated yet.
 */
long bsd_sys_fcntl(struct abi_ctx *ctx)
{
	unsigned long fd = ctx->args[0], arg = ctx->args[2];
	int cmd = ctx->args[1];

	if (cmd == abi_bsd_ops(ctx)->f_dupfd_cloexec)
		return abi_native3(ctx, __NR_fcntl, fd, F_DUPFD_CLOEXEC, arg);

	switch (cmd) {
	case 0:		/* F_DUPFD */
	case 1:		/* F_GETFD */
	case 2:		/* F_SETFD */
		return abi_native3(ctx, __NR_fcntl, fd, cmd, arg);
	case 3:		/* F_GETFL */
		ctx->post = bsd_getfl_post;
		return abi_native3(ctx, __NR_fcntl, fd, F_GETFL, 0);
	case 4:		/* F_SETFL */
		return abi_native3(ctx, __NR_fcntl, fd, F_SETFL,
				   bsd_oflags(ctx, arg));
	case 5:		/* F_GETOWN */
		return abi_native3(ctx, __NR_fcntl, fd, F_GETOWN, 0);
	case 6:		/* F_SETOWN */
		return abi_native3(ctx, __NR_fcntl, fd, F_SETOWN, arg);
	}
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(bsd_sys_fcntl);

/* __getcwd(buf, len) returns 0, Linux returns the length. */
static long bsd_getcwd_post(struct abi_ctx *ctx, long ret)
{
	return ret > 0 ? 0 : ret;
}

long bsd_sys_getcwd(struct abi_ctx *ctx)
{
	ctx->post = bsd_getcwd_post;
	return abi_native2(ctx, __NR_getcwd, ctx->args[0], ctx->args[1]);
}
EXPORT_SYMBOL_GPL(bsd_sys_getcwd);

/*
 * Directory entries
 */
struct freebsd_dirent {		/* FreeBSD 12 and later */
	u64	d_fileno;
	s64	d_off;
	u16	d_reclen;
	u8	d_type;
	u8	d_pad0;
	u16	d_namlen;
	u16	d_pad1;
	char	d_name[];
};

size_t abi_bsd_emit_dirent_freebsd(void *buf, size_t room, const char *name,
				   int namlen, u64 ino, unsigned int type,
				   loff_t next)
{
	size_t reclen = ALIGN(sizeof(struct freebsd_dirent) + namlen + 1, 8);
	struct freebsd_dirent *d = buf;

	if (reclen > room)
		return 0;
	memset(d, 0, reclen);
	d->d_fileno = ino;
	d->d_off = next;
	d->d_reclen = reclen;
	d->d_type = type;
	d->d_namlen = namlen;
	memcpy(d->d_name, name, namlen);
	return reclen;
}
EXPORT_SYMBOL_GPL(abi_bsd_emit_dirent_freebsd);

struct netbsd_dirent {		/* NetBSD 3.0 and later */
	u64	d_fileno;
	u16	d_reclen;
	u16	d_namlen;
	u8	d_type;
	char	d_name[];
};

size_t abi_bsd_emit_dirent_netbsd(void *buf, size_t room, const char *name,
				  int namlen, u64 ino, unsigned int type,
				  loff_t next)
{
	size_t reclen = ALIGN(offsetof(struct netbsd_dirent, d_name) +
			      namlen + 1, 8);
	struct netbsd_dirent *d = buf;

	if (reclen > room)
		return 0;
	memset(d, 0, reclen);
	d->d_fileno = ino;
	d->d_reclen = reclen;
	d->d_namlen = namlen;
	d->d_type = type;
	memcpy(d->d_name, name, namlen);
	return reclen;
}
EXPORT_SYMBOL_GPL(abi_bsd_emit_dirent_netbsd);

struct openbsd_dirent {
	u64	d_fileno;
	s64	d_off;
	u16	d_reclen;
	u8	d_type;
	u8	d_namlen;
	u8	d_padding[4];
	char	d_name[];
};

size_t abi_bsd_emit_dirent_openbsd(void *buf, size_t room, const char *name,
				   int namlen, u64 ino, unsigned int type,
				   loff_t next)
{
	size_t reclen = ALIGN(sizeof(struct openbsd_dirent) + namlen + 1, 8);
	struct openbsd_dirent *d = buf;

	if (reclen > room)
		return 0;
	if (namlen > 255)
		namlen = 255;
	memset(d, 0, reclen);
	d->d_fileno = ino;
	d->d_off = next;
	d->d_reclen = reclen;
	d->d_type = type;
	d->d_namlen = namlen;
	memcpy(d->d_name, name, namlen);
	return reclen;
}
EXPORT_SYMBOL_GPL(abi_bsd_emit_dirent_openbsd);
