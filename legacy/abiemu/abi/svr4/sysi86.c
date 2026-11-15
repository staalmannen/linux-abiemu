/*
 * Copyright 1994,1995	Mike Jagdis <jaggy@purplet.demon.co.uk>
 * Copyright 2002 Caldera Deutschland GmbH
 */

#ident "%W% %G%"

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/ptrace.h>
#include <linux/syscalls.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <asm/uaccess.h>

#include <abi/util/trace.h>


/*
 * The sysi86() call is used for machine specific functions. Only the
 * most basic are implemented here.
 */
#define SI86SWPI        1       /* General swap functions. */
#define SI86SYM         2       /* Get symbol table */
#define SI86CONF        4       /* Get configuration table */
#define SI86BOOT        5       /* Get timestamp and name of program
				 * booted.
				 */
#define SI86DMM         7       /* Double-map data segment for
				 * read/write/execute support
				 */
#define SI86AUTO        9       /* Was an auto-config boot done? */
#define SI86EDT         10      /* Copy contents of EDT to user */
#define SI86SWAP        12      /* Declare swap space */
#define SI86FPHW        40      /* what (if any?) floating-point hardware */
#  define FP_NO		0	/* No fp at all */
#  define FP_SW		1	/* using emulator */
#  define FP_HW		2	/* using hardware */
#  define FP_287	2	/* using a 287 */
#  define FP_387	3	/* using a 387 */
#define GRNON		52	/* set green light to solid on state */
#define GRNFLASH	53	/* start green light flashing */
#define STIME 		54	/* set internal time */
#define SETNAME		56	/* rename the system */
#define RNVR 		58	/* read NVRAM */
#define WNVR 		59	/* write NVRAM */
#define RTODC		60	/* read time of day clock */
#define CHKSER		61	/* check soft serial number */
#define SI86NVPRT       62      /* print an xtra_nvr structure */
#define SANUPD		63	/* sanity update of kernel buffers */
#define SI86KSTR        64      /* make a copy of a kernel string */
#define SI86MEM         65      /* return the memory size of system */
#define SI86TODEMON     66      /* Transfer control to firmware.        */
#define SI86CCDEMON     67      /* Control character access to demon.   */
#define SI86CACHE       68      /* Turn cache on and off.               */
#define SI86DELMEM      69      /* Delete available memory for testing. */
#define SI86ADDMEM      70      /* Add back deleted memory.             */
/* 71 through 74 reserved for VPIX */
#define SI86V86         71      /* V86 system calls (see v86.h)         */
#define	SI86SLTIME	72	/* Set local time correction		*/
#define SI86DSCR        75      /* Set a segment or gate descriptor     */
#define RDUBLK		76	/* Read U Block */
/* #ifdef MERGE386 */
/* NFA entry point */
#define SI86NFA         77      /* make nfa_sys system call */
#define SI86VM86	81
#define SI86VMENABLE	82
/* #endif MERGE386 */
#define SI86VM86        81
#define SI86VMENABLE    82
#define SI86LIMUSER     91      /* liscense interface */
#define SI86RDID        92      /* ROM BIOS Machid ID */
#define SI86RDBOOT      93      /* Bootable Non-SCSI Hard Disk */
/* Merged Product defines */
#define SI86SHFIL	100	/* map a file into addr space of a proc */
#define SI86PCHRGN	101	/* make globally visible change to a region */
#define SI86BADVISE	102	/* badvise subcommand - see below for   */
				/*	badvise subfunction definitions */
#define SI86SHRGN	103	/* enable/disable XENIX small model shared */
				/*	data context switching		   */
#define	SI86CHIDT	104	/* set user level int 0xf0, ... 0xff handlers */
#define	SI86EMULRDA	105	/* remove special emulator read access	*/
#define SI86GETPIPE	106	/* return the pipe filesystem */
#define SI86SETPIPE	107	/* set the pipe filesystem */
#define SI86SETPIPE_NM	108	/* set the pipe filesystem -non mountable */
#define SI86GETNPIPE	109	/* get # of pipe filesystems */
#define SI86GETPIPE_ALL	110	/* get data on all of pipe filesystems */
#define SI86POPPIPE	111	/* pop pipe file system off stack */
#define SI86APM		112	/* get APM information passed by boot(HW) */
#define SI86TIMECHG	113	/* get time before/after last timechange */
#define SI86GETFEATURES	114	/* get os features vector */

/* The SI86BADVISE command is used to set Xenix behaviour. */
#define SI86B_SET		0x0100	/* Set badvise bits */
#define SI86B_GET		0x0200	/* Get badvise bits */
#define	SI86B_LOCK		0x0001	/* XENIX advisory locking bit */
#define SI86B_PRE_SV		0x0008	/* follow pre-System V x.out behavior */
#define SI86B_XOUT		0x0010 	/* follow XENIX x.out behavior */
#define SI86B_XSDSWTCH		0x0080	/* XENIX small model shared data */
					/* context switching enabled */

/*
 *  The SI86DSCR subcommand of the sysi86() system call
 *  sets a segment or gate descriptor in the kernel.
 *  The following descriptor types are accepted:
 *    - executable and data segments in the LDT at DPL 3
 *    - a call gate in the GDT at DPL 3 that points to a segment in the LDT
 *  The request structure declared below is used to pass the values
 *  to be placed in the descriptor.  A pointer to the structure is
 *  passed as the second argument of the system call.
 *  If acc1 is zero, the descriptor is cleared.
 */
struct ssd {
	uint32_t  sel;	/* descriptor selector */
	uint32_t  bo;	/* segment base or gate offset */
	uint32_t  ls;	/* segment limit or gate selector */
	uint32_t  acc1;	/* access byte 5 */
	uint32_t  acc2;	/* access bits in byte 6 or gate count */
};


int
svr4_sysi86(int cmd, void *arg1, int arg2)
{
	switch (cmd) {
	case SI86FPHW:
		/*
		 * If we remove the 'static' from the definition
		 * of fpu_error in linux/init/main.c we can tell
		 * whether we are using hardware or software at
		 * least. For now let's lie...
		 * (actually SCO Unix 3.4 gives me -1...)
		 */
		return put_user(FP_387, (unsigned long *)arg1);
	case STIME:
		/*
		 * Set the system time. The argument is a long,
		 * sys_stime() expects a pointer to a long...
		 */
		return sys_stime(arg1);
	case SETNAME:
		/*
		 * The name is required to be string of no more
		 * than 7 characters. We don't get passed the
		 * length so we are depending upon the current
		 * implementation of sys_sethostname() here.
		 */
		return sys_sethostname(arg1, 7);
	case SI86MEM:
	    {
		/*
		 * Returns the size of physical memory.
		 */
		struct sysinfo	i;

		si_meminfo(&i);
		return (i.totalram << PAGE_SHIFT);
	    }
	case SI86DSCR:
	    {
		struct ssd s;

		if (copy_from_user(&s, arg1, sizeof(struct ssd)))
			return -EFAULT;

		printk("SI86DSCR(%x,%x,%x,%x,%x)\n",
			 s.sel, s.bo, s.ls, s.acc1, s.acc2);
		return -EINVAL;
	    }
	default:
#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_API, "unsupported sysi86 subcall %d\n", cmd);
#endif
		return -EINVAL;
	}
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_sysi86);
#endif
