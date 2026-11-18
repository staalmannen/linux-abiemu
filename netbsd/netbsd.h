/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ABI_NETBSD_H
#define _ABI_NETBSD_H

#include <abiemu/bsd.h>
#include "syscall_nr.h"

/* What we claim to be: NetBSD 10.1 */
#define NETBSD_OSRELEASE	"10.1"
#define NETBSD_OSREV		1001000000

extern const struct abi_sysent netbsd_sysent[];
extern const unsigned int netbsd_nsysent;

long netbsd_sys_sysctl(struct abi_ctx *ctx);

long netbsd_sys_sigaction(struct abi_ctx *ctx);
long netbsd_sys_sigsuspend(struct abi_ctx *ctx);
long netbsd_sys_sigpending(struct abi_ctx *ctx);
long netbsd_sys_setcontext(struct abi_ctx *ctx);
int netbsd_sig_deliver(struct abi_ctx *ctx, int lsig, unsigned long handler,
		       unsigned long lframe);
long netbsd_sig_return(struct abi_ctx *ctx);

#endif
