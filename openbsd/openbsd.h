/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ABI_OPENBSD_H
#define _ABI_OPENBSD_H

#include <abiemu/bsd.h>
#include "syscall_nr.h"

#define OPENBSD_OSRELEASE	"7.8"
#define OPENBSD_OSREV		202510

extern const struct abi_sysent openbsd_sysent[];
extern const unsigned int openbsd_nsysent;

long openbsd_sys_sigaction(struct abi_ctx *ctx);
long openbsd_sys_sigprocmask(struct abi_ctx *ctx);
long openbsd_sys_sigsuspend(struct abi_ctx *ctx);
long openbsd_sys_sigpending(struct abi_ctx *ctx);
long openbsd_sys_sigreturn(struct abi_ctx *ctx);
long openbsd_sys_thrkill(struct abi_ctx *ctx);
int openbsd_sig_deliver(struct abi_ctx *ctx, int lsig, unsigned long handler,
			unsigned long lframe);
long openbsd_sig_return(struct abi_ctx *ctx);

/* OpenBSD's sigset_t is a single 32 bit word */
static inline u64 openbsd_sigset_to_linux(u32 mask)
{
	u32 set[2] = { mask, 0 };

	return abi_bsd_sigset_to_linux(set);
}

static inline u32 openbsd_sigset_from_linux(u64 lset)
{
	u32 set[4];

	abi_bsd_sigset_from_linux(lset, set);
	return set[0];
}

#endif
