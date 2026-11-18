/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ABI_PLAN9_H
#define _ABI_PLAN9_H

#include <abiemu/abiemu.h>
#include <linux/uaccess.h>
#include "syscall_nr.h"

#define P9_ERRMAX	128
#define P9_UTZERO	0x200000UL
#define P9_S_MAGIC	0x8a97		/* amd64, with HDR_MAGIC */
#define P9_HDRSZ	40

/* the Tos ("top of stack") page, shared between kernel and user */
#define P9_TOS_PAGE	(P9_UTZERO - PAGE_SIZE)
#define P9_TOS_SIZE	72
#define P9_TOS		(P9_TOS_PAGE + PAGE_SIZE - P9_TOS_SIZE)
#define P9_TOS_PID	64		/* offsetof(Tos, pid) */

/* open modes */
#define P9_OREAD	0
#define P9_OWRITE	1
#define P9_ORDWR	2
#define P9_OEXEC	3
#define P9_OTRUNC	0x10
#define P9_OCEXEC	0x20
#define P9_ORCLOSE	0x40
#define P9_OEXCL	0x1000

#define P9_DMDIR	0x80000000U
#define P9_DMAPPEND	0x40000000U
#define P9_DMEXCL	0x20000000U

/* ctx->flags: the error string has already been set */
#define P9_F_ERRSET	ABI_F_PERS

struct p9_task {
	char		errstr[P9_ERRMAX];
	unsigned long	note_lframe;	/* Linux signal frame of the note */
	unsigned long	note_ureg;	/* Plan 9 Ureg handed to the handler */
	unsigned int	notify_idx;	/* notify(): signals done so far */
	unsigned long	notify_fn;
};

extern const struct abi_sysent plan9_sysent[];
extern const unsigned int plan9_nsysent;

struct p9_task *p9_task(void);
long p9_error(struct abi_ctx *ctx, const char *msg);
const char *p9_errno_str(long err, char *buf);
void p9_exitmsg_put(pid_t pid, const char *msg);
bool p9_exitmsg_take(pid_t pid, char *buf);
const char *p9_note_name(int lsig, char *buf, unsigned long addr,
			 unsigned long pc);

/* dir.c */
long p9_sys_stat(struct abi_ctx *ctx);
long p9_sys_fstat(struct abi_ctx *ctx);
long p9_sys_wstat(struct abi_ctx *ctx);
long p9_sys_fwstat(struct abi_ctx *ctx);
long p9_dirread(struct abi_ctx *ctx, struct file *file, void __user *buf,
		size_t n);

/* note.c */
long p9_sys_notify(struct abi_ctx *ctx);
long p9_sys_noted(struct abi_ctx *ctx);
int p9_sig_deliver(struct abi_ctx *ctx, int lsig, unsigned long handler,
		   unsigned long lframe);

/* sys.c */
long p9_sys_exits(struct abi_ctx *ctx);

static inline char *p9_strdup_user(unsigned long uptr, size_t max)
{
	return strndup_user((const char __user *)uptr, max);
}

#endif
