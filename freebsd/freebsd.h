/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ABI_FREEBSD_H
#define _ABI_FREEBSD_H

#include <abiemu/bsd.h>
#include "syscall_nr.h"

/* What we claim to be: FreeBSD 14.3-RELEASE */
#define FREEBSD_OSRELDATE	1403000
#define FREEBSD_OSRELEASE	"14.3-RELEASE"

#ifndef ELFOSABI_FREEBSD
#define ELFOSABI_FREEBSD	9
#endif

extern const struct abi_sysent freebsd_sysent[];
extern const unsigned int freebsd_nsysent;

/* sysctl.c */
long freebsd_sys_sysctl(struct abi_ctx *ctx);
long freebsd_sys_sysctlbyname(struct abi_ctx *ctx);

/* signal.c */
long freebsd_sys_sigaction(struct abi_ctx *ctx);
long freebsd_sys_sigreturn(struct abi_ctx *ctx);
long freebsd_sys_sigsuspend(struct abi_ctx *ctx);
long freebsd_sys_sigpending(struct abi_ctx *ctx);
long freebsd_sys_thr_kill(struct abi_ctx *ctx);
int freebsd_sig_deliver(struct abi_ctx *ctx, int lsig, unsigned long handler,
			unsigned long lframe);
long freebsd_sig_return(struct abi_ctx *ctx);

#endif
