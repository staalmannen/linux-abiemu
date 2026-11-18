/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ABI_ILLUMOS_H
#define _ABI_ILLUMOS_H

#include <abiemu/abiemu.h>
#include <linux/uaccess.h>
#include "syscall_nr.h"

#ifndef ELFOSABI_SOLARIS
#define ELFOSABI_SOLARIS	6
#endif

#define ILLUMOS_AT_FDCWD	0xffd19553U

extern const struct abi_sysent illumos_sysent[];
extern const unsigned int illumos_nsysent;

/* signals.c */
int illumos_sig_to_linux(int sig);
int illumos_sig_from_linux(int sig);
u64 illumos_sigset_to_linux(const u32 *set);
void illumos_sigset_from_linux(u64 lset, u32 *set);
long illumos_sys_sigaction(struct abi_ctx *ctx);
long illumos_sys_sigprocmask(struct abi_ctx *ctx);
long illumos_sys_lwp_sigmask(struct abi_ctx *ctx);
long illumos_sys_sigsuspend(struct abi_ctx *ctx);
long illumos_sys_sigpending(struct abi_ctx *ctx);
long illumos_sys_sigaltstack(struct abi_ctx *ctx);
long illumos_sys_kill(struct abi_ctx *ctx);
long illumos_sys_lwp_kill(struct abi_ctx *ctx);
long illumos_sys_context(struct abi_ctx *ctx);
long illumos_sys_waitsys(struct abi_ctx *ctx);
int illumos_sig_deliver(struct abi_ctx *ctx, int lsig, unsigned long handler,
			unsigned long lframe);
long illumos_sig_return(struct abi_ctx *ctx);

/* Translate a directory file descriptor argument. */
static inline long illumos_dirfd(unsigned long fd)
{
	return (u32)fd == ILLUMOS_AT_FDCWD ? AT_FDCWD : (long)(int)fd;
}

/* rval2 */
static inline void illumos_retval2(struct abi_ctx *ctx, unsigned long v)
{
	ctx->sv_dx = v;
	ctx->regs->dx = v;
}

#endif
