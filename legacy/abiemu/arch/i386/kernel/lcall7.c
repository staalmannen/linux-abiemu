/*
 * Copyright (c) 2000,2001 Christoph Hellwig.
 * Copyright (c) 2001 Caldera Deutschland GmbH.
 * All rights resered.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ident "%W% %G%"

/*
 * Lowlevel handler for lcall7-based syscalls.
 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/ptrace.h>
#include <linux/init.h>
#include <linux/personality.h>
#include <asm/uaccess.h>

#include <abi/util/errno.h>
#include <abi/util/trace.h>
#include <abi/util/sysent.h>

MODULE_AUTHOR("Christoph Hellwig");
MODULE_DESCRIPTION("Lowlevel handler for lcall7-based syscalls");
MODULE_LICENSE("GPL");


static void get_args(int args[], struct pt_regs *regs, int of, int n)
{
	int i;

	for (i = 0; i < n; i++)
		get_user(args[i], ((unsigned long *)regs->esp) + (i+of));
}

/*
 *	lcall7_syscall    -    indirect syscall for the lcall7 entry point
 *
 *	@regs:		saved user registers
 *
 *	This function implements syscall(2) in kernelspace for the lcall7-
 *	based personalities.
 */

int lcall7_syscall(struct pt_regs *regs)
{
	__get_user(regs->eax, ((unsigned long *)regs->esp)+1);

	++regs->esp;
	current_thread_info()->exec_domain->handler(-1,regs);
	--regs->esp;

	return 0;
}

/**
 *	lcall7_dispatch    -    handle lcall7-based syscall entry
 *
 *	@regs:		saved user registers
 *	@ap:		syscall table entry
 *	@off:		argument offset
 *
 *	This function handles lcall7-based syscalls after the personality-
 *	specific rountine selected the right syscall table entry.
 */

void lcall7_dispatch(struct pt_regs *regs, struct sysent *ap, int off)
{
	short nargs = ap->se_nargs;
	int args[8], error;

	if (!ap->se_syscall) /* XXX kludge XXX */
		nargs = Unimpl;

	if (nargs <= ARRAY_SIZE(args))
		get_args(args, regs, off, nargs);

#if defined(CONFIG_ABI_TRACE)
	if (abi_traced(ABI_TRACE_API)) {
		if (nargs == Spl)
			get_args(args, regs, off, strlen(ap->se_args));
		plist(ap->se_name, ap->se_args, args);
	}
#endif

	switch (nargs) {
	case Fast:
		SYSCALL_PREGS(ap->se_syscall, regs);
		goto show_signals;
	case Spl:
		error = SYSCALL_PREGS(ap->se_syscall, regs);
		break;
	case 0:
		error = SYSCALL_VOID(ap->se_syscall);
		break;
	case 1:
		error = SYSCALL_1ARG(ap->se_syscall, args);
		break;
	case 2:
		error = SYSCALL_2ARG(ap->se_syscall, args);
		break;
	case 3:
		error = SYSCALL_3ARG(ap->se_syscall, args);
		break;
	case 4:
		error = SYSCALL_4ARG(ap->se_syscall, args);
		break;
	case 5:
		error = SYSCALL_5ARG(ap->se_syscall, args);
		break;
	case 6:
		error = SYSCALL_6ARG(ap->se_syscall, args);
		break;
	case 7:
		error = SYSCALL_7ARG(ap->se_syscall, args);
		break;
	default:
#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_UNIMPL,
			"Unsupported ABI function 0x%lx (%s)\n",
			regs->eax, ap->se_name);
#endif
		error = -ENOSYS;
	}

	if (error > -ENOIOCTLCMD && error < 0) {
		set_error(regs, iABI_errors(-error));

#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_API,
		    "%s error return %d/%ld\n",
		    ap->se_name, error, regs->eax);
#endif
	} else {
		clear_error(regs);
		set_result(regs, error);

#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_API,
		    "%s returns %ld (edx:%ld)\n",
		    ap->se_name, regs->eax, regs->edx);
#endif
	}

show_signals:
#if defined(CONFIG_ABI_TRACE)
	if (signal_pending(current) && abi_traced(ABI_TRACE_SIGNAL)) {
		unsigned long signr;

		signr = current->pending.signal.sig[0] &
			~current->blocked.sig[0];

		__asm__("bsf %1,%0\n\t"
				:"=r" (signr)
				:"0" (signr));

		__abi_trace("SIGNAL %lu, queued 0x%08lx\n",
			signr+1, current->pending.signal.sig[0]);
	}
#endif
        return;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(lcall7_syscall);
EXPORT_SYMBOL(lcall7_dispatch);
#endif
