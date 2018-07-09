/*
 * ptrace.c - Wyse V/386 ptrace(2) support.
 *
 * Copyright (c) 1995 Mike Jagdis (jaggy@purplet.demon.co.uk)
 */

#ident "%W% %G%"

/*
 * This file is nearly identical to abi/sco/ptrace.c, please keep it in sync.
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/personality.h>
#include <linux/user.h>
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

#include <asm/uaccess.h>

#include <abi/signal.h>
#include <abi/util/trace.h>


#define NREGS	19
#define U(X)	((unsigned long)&((struct user *)0)->X)


static unsigned long wysev386_to_linux_reg[NREGS] = {
	U(regs.es),	U(regs.ds),	U(regs.edi),	U(regs.esi),
	U(regs.ebp),	U(regs.esp),
	U(regs.ebx),	U(regs.edx),	U(regs.ecx),	U(regs.eax),
	U(signal	/* Trap */),
	U(reserved	/* ERR */),
	U(regs.eip),	U(regs.cs),
	U(regs.eflags),
	U(regs.esp	/* UESP */),
	U(regs.ss),
	U(regs.fs),	U(regs.gs)
};

static const char *regnam[] = {
	"EBX", "ECX", "EDX", "ESI", "EDI", "EBP", "EAX",
	"DS", "ES", "FS", "GS", "ORIG_EAX", "EIP", "CS",
	"EFL", "UESP", "SS"
};


int
wyse_ptrace(int req, int pid, u_long addr, u_long data)
{
	u_long			res;

	/*
	 * Slight variations between iBCS and Linux codes.
	 */
	if (req == PTRACE_ATTACH)
		req = 10;
	else if (req == PTRACE_DETACH)
		req = 11;

	if (req == 3 || req == 6) {
		/* get offset of u_ar0 */
		if (addr == 0x1292)
			return 0x4000;

		/* remap access to the registers. */
		if ((addr & 0xff00) == 0x4000) { /* Registers */
			addr = (addr & 0xff) >> 2;
			if (addr > NREGS)
				return -EIO;
			addr = wysev386_to_linux_reg[addr];
			if (addr == -1)
				return -EIO;
		}
	}

	if (req == 7 && data > 0) {
		if (data > NSIGNALS)
			return -EIO;
		data = current_thread_info()->exec_domain->signal_map[data];
	}

	if (req == 1 || req == 2 || req == 3) {
		mm_segment_t	old_fs = get_fs();
		int		error;

		set_fs(get_ds());
		error = sys_ptrace(req, pid, addr, (long)&res);
		set_fs(old_fs);

		if (error)
			return (error);
	}

#if defined(CONFIG_ABI_TRACE)
	if (req == 3 || req == 6) {
		abi_trace(ABI_TRACE_API, "%ld [%s] = 0x%08lx\n",
			addr >> 2, (addr >> 2) < ARRAY_SIZE(regnam) ?
				regnam[addr >> 2] : "???",
			req == 3 ? res : data);
	}
#endif

	if (req == 1 || req == 2 || req == 3)
		return (res);

	return sys_ptrace(req, pid, addr, data);
}
