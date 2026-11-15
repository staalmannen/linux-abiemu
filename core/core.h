/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ABIEMU_CORE_H
#define _ABIEMU_CORE_H

#include <abiemu/abiemu.h>

#define ABI_PER_MIN	0x40
#define ABI_PER_MAX	0x4f

struct linux_binprm;

extern const struct abi_personality __rcu *abi_pers[ABI_PER_MAX + 1];

static inline bool abi_per_foreign(unsigned int per)
{
	return per >= ABI_PER_MIN && per <= ABI_PER_MAX;
}

const struct abi_personality *abi_task_bind(struct abi_task *t,
					    unsigned int per);
void abi_task_release(struct abi_task *t);

/* syscall.c */
void abi_sys_enter(void *data, struct pt_regs *regs, long id);
void abi_sys_exit(void *data, struct pt_regs *regs, long ret);
void abi_tp_fork(void *data, struct task_struct *parent,
		 struct task_struct *child);

/* signal.c */
long abi_sig_trap(struct abi_ctx *ctx);
int abi_tramp_init(void);
void abi_tramp_exit(void);

/* exec.c */
void abi_tp_exec(void *data, struct task_struct *p, pid_t old_pid,
		 struct linux_binprm *bprm);
void abi_binfmt_init(void);
void abi_binfmt_exit(void);

#endif
