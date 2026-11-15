/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Helpers shared by the BSD family personalities (abi_bsd.ko).
 *
 * FreeBSD, NetBSD and OpenBSD all descend from 4.4BSD and still agree
 * on the system call convention, the classic errno and signal numbers,
 * the termios layout and much of the flag encoding.  Where they have
 * diverged, the personality supplies a struct abi_bsd_ops.
 */
#ifndef _ABIEMU_BSD_H
#define _ABIEMU_BSD_H

#include <abiemu/abiemu.h>

struct abi_flagmap {
	unsigned int foreign;
	unsigned int native;
};

struct abi_bsd_ops {
	const struct abi_flagmap *oflags;	/* open(2) flags, 0 terminated */
	const struct abi_flagmap *atflags;	/* AT_* flags */
	unsigned int		wcontinued;	/* WCONTINUED option bit */
	unsigned int		wcont_status;	/* "continued" wait status */
	int			f_dupfd_cloexec; /* fcntl() command */
};

static inline const struct abi_bsd_ops *abi_bsd_ops(struct abi_ctx *ctx)
{
	return ctx->pers->data;
}

/* errno values common to all BSDs, indexed by Linux errno */
#define BSD_ERRNO_COMMON \
	[EDEADLK] = 11, [EAGAIN] = 35, [EINPROGRESS] = 36, [EALREADY] = 37, \
	[ENOTSOCK] = 38, [EDESTADDRREQ] = 39, [EMSGSIZE] = 40, \
	[EPROTOTYPE] = 41, [ENOPROTOOPT] = 42, [EPROTONOSUPPORT] = 43, \
	[ESOCKTNOSUPPORT] = 44, [EOPNOTSUPP] = 45, [EPFNOSUPPORT] = 46, \
	[EAFNOSUPPORT] = 47, [EADDRINUSE] = 48, [EADDRNOTAVAIL] = 49, \
	[ENETDOWN] = 50, [ENETUNREACH] = 51, [ENETRESET] = 52, \
	[ECONNABORTED] = 53, [ECONNRESET] = 54, [ENOBUFS] = 55, \
	[EISCONN] = 56, [ENOTCONN] = 57, [ESHUTDOWN] = 58, \
	[ETOOMANYREFS] = 59, [ETIMEDOUT] = 60, [ECONNREFUSED] = 61, \
	[ELOOP] = 62, [ENAMETOOLONG] = 63, [EHOSTDOWN] = 64, \
	[EHOSTUNREACH] = 65, [ENOTEMPTY] = 66, [EUSERS] = 68, \
	[EDQUOT] = 69, [ESTALE] = 70, [EREMOTE] = 71, [ENOLCK] = 77, \
	[ENOSYS] = 78

/* calling convention */
int abi_bsd_decode(struct abi_ctx *ctx);
void abi_bsd_set_result(struct abi_ctx *ctx, long ret);
void abi_bsd_fork_child(struct abi_ctx *parent, struct pt_regs *child);
/* Return a second value in %rdx (pipe(), fork(), ...). */
static inline void abi_bsd_retval2(struct abi_ctx *ctx, unsigned long v)
{
	ctx->sv_dx = v;
	ctx->regs->dx = v;
}

/* signals */
#define BSD_NSIG	32
int abi_bsd_sig_to_linux(int sig);
int abi_bsd_sig_from_linux(int sig);
u64 abi_bsd_sigset_to_linux(const u32 *set);
void abi_bsd_sigset_from_linux(u64 lset, u32 *set);
int abi_bsd_wstatus(struct abi_ctx *ctx, int status);

/* flags */
unsigned int abi_flags_to_linux(const struct abi_flagmap *map, unsigned int f);
unsigned int abi_flags_from_linux(const struct abi_flagmap *map, unsigned int f);
#define BSD_O_ACCMODE	0x3

/* handlers with the same signature on all BSDs */
long bsd_sys_nosys(struct abi_ctx *ctx);
long bsd_sys_zero(struct abi_ctx *ctx);
long bsd_sys_exit(struct abi_ctx *ctx);
long bsd_sys_fork(struct abi_ctx *ctx);
long bsd_sys_vfork(struct abi_ctx *ctx);
long bsd_sys_open(struct abi_ctx *ctx);
long bsd_sys_openat(struct abi_ctx *ctx);
long bsd_sys_access(struct abi_ctx *ctx);
long bsd_sys_faccessat(struct abi_ctx *ctx);
long bsd_sys_unlinkat(struct abi_ctx *ctx);
long bsd_sys_fchmodat(struct abi_ctx *ctx);
long bsd_sys_fchownat(struct abi_ctx *ctx);
long bsd_sys_linkat(struct abi_ctx *ctx);
long bsd_sys_utimensat(struct abi_ctx *ctx);
long bsd_sys_wait4(struct abi_ctx *ctx);
long bsd_sys_kill(struct abi_ctx *ctx);
long bsd_sys_pipe(struct abi_ctx *ctx);
long bsd_sys_pipe2(struct abi_ctx *ctx);
long bsd_sys_fcntl(struct abi_ctx *ctx);
long bsd_sys_ioctl(struct abi_ctx *ctx);
long bsd_sys_getcwd(struct abi_ctx *ctx);
long bsd_sys_getlogin(struct abi_ctx *ctx);
long bsd_sys_getdtablesize(struct abi_ctx *ctx);
long bsd_sys_madvise(struct abi_ctx *ctx);
long bsd_sys_sigprocmask_set(struct abi_ctx *ctx);
long bsd_sys_sigaltstack(struct abi_ctx *ctx);
long bsd_sys_getrlimit(struct abi_ctx *ctx);
long bsd_sys_setrlimit(struct abi_ctx *ctx);

/* 4.4BSD mmap(): the flag bits that everybody still shares */
#define BSD_MAP_SHARED		0x0001
#define BSD_MAP_PRIVATE		0x0002
#define BSD_MAP_FIXED		0x0010
#define BSD_MAP_STACK		0x0400
#define BSD_MAP_ANON		0x1000
/*
 * Map memory on behalf of a BSD program.  @lflags are Linux MAP_*
 * flags, @align is a power of two or 0.
 */
long abi_bsd_mmap(unsigned long addr, unsigned long len, int prot,
		  unsigned long lflags, int fd, loff_t off,
		  unsigned long align);

/* directory entry encoders */
size_t abi_bsd_emit_dirent_freebsd(void *buf, size_t room, const char *name,
				   int namlen, u64 ino, unsigned int type,
				   loff_t next);
size_t abi_bsd_emit_dirent_netbsd(void *buf, size_t room, const char *name,
				  int namlen, u64 ino, unsigned int type,
				  loff_t next);
size_t abi_bsd_emit_dirent_openbsd(void *buf, size_t room, const char *name,
				   int namlen, u64 ino, unsigned int type,
				   loff_t next);

/* BSD dev_t <-> Linux */
static inline u64 abi_bsd_mkdev(unsigned int major, unsigned int minor)
{
	return ((u64)(major & 0xffffff00) << 32) | ((major & 0xff) << 8) |
	       ((u64)(minor & 0xff00) << 24) | (minor & 0xffff00ff);
}

#endif /* _ABIEMU_BSD_H */
