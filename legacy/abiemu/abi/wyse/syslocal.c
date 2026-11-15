/*
 * Copyright (C) 1994	Mike Jagdis (jaggy@purplet.demon.co.uk)
 */

#ident "%W% %G%"

#include <linux/module.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

#include <abi/util/trace.h>


/*
 * The syslocal() call is used for machine specific functions. For
 * instance on a Wyse 9000 it give information and control of the
 * available processors.
 */
#define SL_ONLINE	0	/* Turn processor online		*/
#define SL_OFFLINE	1	/* Turn processor offline		*/
#define SL_QUERY	2	/* Query processor status		*/
#define SL_NENG		3	/* Return No. of processors configured	*/
#define SL_AFFINITY	4	/* processor binding			*/
#define SL_CMC_STAT	7	/* gather CMC performance counters info	*/
#define SL_KACC		8	/* make kernel data readable by user	*/
#define SL_MACHTYPE	9	/* return machine type (MP/AT)		*/
#define SL_BOOTNAME	10	/* return name of booted kernel		*/
#define SL_BOOTDEV	11	/* return type of booted device		*/
#define SL_UQUERY	12	/* query user status			*/

#define SL_MACH_MP	0
#define SL_MACH_AT	1
#define SL_MACH_EISA	2
#define SL_MACH_EMP	3


int
wyse_syslocal(struct pt_regs *regp)
{
	int			cmd = get_syscall_parameter(regp, 0);

	switch (cmd) {
	case SL_QUERY:
		return 0;
	case SL_NENG:
		return 1;
	case SL_MACHTYPE:
		return (EISA_bus ? SL_MACH_EISA : SL_MACH_AT);
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_UNIMPL, "unsupported syslocal call %d\n", cmd);
#endif
	return -EINVAL;
}
