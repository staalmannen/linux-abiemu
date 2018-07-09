/*
 *  Copyright (C) 1993  Joe Portman (baron@hebron.connected.com)
 *	 First stab at ulimit
 *
 *  April 9 1994, corrected file size passed to/from setrlimit/getrlimit
 *    -- Graham Adams (gadams@ddrive.demon.co.uk)
 *
 */

#ident "%W% %G%"

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/fs.h>
#include <linux/resource.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <abi/util/trace.h>


/*
 * Arguments to ulimit - it's one of the stupid multipled calls...
 */
#define U_GETFSIZE 	(1)		  /* get max file size in blocks */
#define U_SETFSIZE 	(2)		  /* set max file size in blocks */
#define U_GETMEMLIM	(3)		  /* get process size limit */
#define U_GETMAXOPEN	(4)		  /* get max open files for this process */
#define U_GTXTOFF		(64)		  /* get text offset */

/*
 * Define nominal block size parameters.
 */
#define ULIM_BLOCKSIZE_BITS   9           /* block size = 512 */
#define ULIM_MAX_BLOCKSIZE (INT_MAX >> ULIM_BLOCKSIZE_BITS)


int
svr4_ulimit (int cmd, int val)
{
	switch (cmd) {
	case U_GETFSIZE:
		return (current->signal->rlim[RLIMIT_FSIZE].rlim_cur)
		 >> ULIM_BLOCKSIZE_BITS;

	case U_SETFSIZE:
		if ((val > ULIM_MAX_BLOCKSIZE) || (val < 0))
			return -ERANGE;
		val <<= ULIM_BLOCKSIZE_BITS;
		if (val > current->signal->rlim[RLIMIT_FSIZE].rlim_max) {
			if (!capable(CAP_SYS_RESOURCE))
				return -EPERM;
			current->signal->rlim[RLIMIT_FSIZE].rlim_max = val;
		}
		current->signal->rlim[RLIMIT_FSIZE].rlim_cur = val;
		return 0;

	case U_GETMEMLIM:
		return current->signal->rlim[RLIMIT_DATA].rlim_cur;

	case U_GETMAXOPEN:
		return current->signal->rlim[RLIMIT_NOFILE].rlim_cur;

	default:
#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_API, "unsupported ulimit call %d\n", cmd);
#endif
		return -EINVAL;
	}
}

/*
 * getrlimit/setrlimit args.
 */
#define U_RLIMIT_CPU	0
#define U_RLIMIT_FSIZE	1
#define U_RLIMIT_DATA	2
#define U_RLIMIT_STACK	3
#define U_RLIMIT_CORE	4
#define U_RLIMIT_NOFILE	5
#define U_RLIMIT_AS	6


int
svr4_getrlimit(int cmd, void *val)
{
	switch (cmd) {
	case U_RLIMIT_CPU:
		cmd = RLIMIT_CPU;
		break;
	case U_RLIMIT_FSIZE:
		cmd = RLIMIT_FSIZE;
		break;
	case U_RLIMIT_DATA:
		cmd = RLIMIT_DATA;
		break;
	case U_RLIMIT_STACK:
		cmd = RLIMIT_STACK;
		break;
	case U_RLIMIT_CORE:
		cmd = RLIMIT_CORE;
		break;
	case U_RLIMIT_NOFILE:
		cmd = RLIMIT_NOFILE;
		break;
	case U_RLIMIT_AS:
		cmd = RLIMIT_AS;
		break;
	default:
		return -EINVAL;
	}

	return sys_getrlimit(cmd, val);
}

int
svr4_setrlimit(int cmd, void *val)
{
	switch (cmd) {
	case U_RLIMIT_CPU:
		cmd = RLIMIT_CPU;
		break;
	case U_RLIMIT_FSIZE:
		cmd = RLIMIT_FSIZE;
		break;
	case U_RLIMIT_DATA:
		cmd = RLIMIT_DATA;
		break;
	case U_RLIMIT_STACK:
		cmd = RLIMIT_STACK;
		break;
	case U_RLIMIT_CORE:
		cmd = RLIMIT_CORE;
		break;
	case U_RLIMIT_NOFILE:
		cmd = RLIMIT_NOFILE;
		break;
	case U_RLIMIT_AS:
		cmd = RLIMIT_AS;
		break;
	default:
		return -EINVAL;
	}

	return sys_getrlimit(cmd, val);
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_getrlimit);
EXPORT_SYMBOL(svr4_setrlimit);
EXPORT_SYMBOL(svr4_ulimit);
#endif
