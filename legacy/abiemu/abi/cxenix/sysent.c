#ident "%W% %G%"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/personality.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <abi/svr4/sysent.h>
#include <abi/cxenix/sysent.h>

#include <abi/util/errno.h>
#include <abi/util/sysent.h>


MODULE_DESCRIPTION("Xenix/OpenServer cxenix call support");
MODULE_AUTHOR("Christoph Hellwig, partially taken from iBCS");
MODULE_LICENSE("GPL");


static struct sysent cxenix_table[] = {
/*   0 */	{ 0,			Ukn,	"syscall",	""	},
/*   1 */	{ xnx_locking,		3,	"locking",	"ddd"	},
/*   2 */	{ xnx_creatsem,		2,	"creatsem",	"sd"	},
/*   3 */	{ xnx_opensem,		1,	"opensem",	"s"	},
/*   4 */	{ xnx_sigsem,		1,	"sigsem",	"d"	},
/*   5 */	{ xnx_waitsem,		1,	"waitsem",	"d"	},
/*   6 */	{ xnx_nbwaitsem,	1,	"nbwaitsem",	"d"	},
/*   7 */	{ xnx_rdchk,		1,	"rdchk",	"d"	},
/*   8 */	{ 0,			Ukn,	"stkgro",	""	},
/*   9 */	{ 0,			Ukn,	"?",		""	},
/*  10 */	{ sys_ftruncate,	2,	"chsize",	"dd"	},
/*  11 */	{ xnx_ftime,		1,	"ftime",	"x"	},
/*  12 */	{ xnx_nap,		1,	"nap",		"d"	},
/*  13 */	{ xnx_sdget,		4,	"sdget",	"sddd"	},
/*  14 */	{ xnx_sdfree,		1,	"sdfree",	"x"	},
/*  15 */	{ xnx_sdenter,		2,	"sdenter",	"xd"	},
/*  16 */	{ xnx_sdleave,		1,	"sdleave",       "x"	},
/*  17 */	{ xnx_sdgetv,		1,	"sdgetv",	"x"	},
/*  18 */	{ xnx_sdwaitv,		2,	"sdwaitv",	"xd"	},
/*  19 */	{ 0,			Ukn,	"brkctl",	""	},
/*  20 */	{ 0,			Ukn,	"?",		""	},
/*  21 */	{ 0,			2,	"sco-getcwd?",	"dx"	},
/*  22 */	{ 0,			Ukn,	"?",		""	},
/*  23 */	{ 0,			Ukn,	"?",		""	},
/*  24 */	{ 0,			Ukn,	"?",		""	},
/*  25 */	{ 0,			Ukn,	"?",		""	},
/*  26 */	{ 0,			Ukn,	"?",		""	},
/*  27 */	{ 0,			Ukn,	"?",		""	},
/*  28 */	{ 0,			Ukn,	"?",		""	},
/*  29 */	{ 0,			Ukn,	"?",		""	},
/*  30 */	{ 0,			Ukn,	"?",		""	},
/*  31 */	{ 0,			Ukn,	"?",		""	},
/*  32 */	{ xnx_proctl,		3,	"proctl",	"ddx"	},
/*  33 */	{ xnx_execseg,		2,	"execseg",	"xd"	},
/*  34 */	{ xnx_unexecseg,	1,	"unexecseg",	"x"	},
/*  35 */	{ 0,			Ukn,	"?",		""	},
/*  36 */	{ sys_select,		5,	"select",	"dxxxx"	},
/*  37 */	{ xnx_eaccess,		2,	"eaccess",	"so"	},
/*  38 */	{ xnx_paccess,		5,	"paccess",	"dddds"	},
/*  39 */	{ xnx_sigaction,	3,	"sigaction",	"dxx"	},
/*  40 */	{ abi_sigprocmask,	3,	"sigprocmask",	"dxx"	},
/*  41 */	{ xnx_sigpending,	1,	"sigpending",	"x"	},
/*  42 */	{ abi_sigsuspend,	Spl,	"sigsuspend",	"x"	},
/*  43 */	{ sys_getgroups16,	2,	"getgroups",	"dx"	},
/*  44 */	{ sys_setgroups16,	2,	"setgroups",	"dx"	},
/*  45 */	{ ibcs_sysconf,		1,	"sysconf",	"d"	},
/*  46 */	{ xnx_pathconf,		2,	"pathconf",	"sd"	},
/*  47 */	{ xnx_fpathconf,	2,	"fpathconf",	"dd"	},
/*  48 */	{ sys_rename,		2,	"rename",	"ss"	},
/*  49 */	{ 0,			Ukn,	"?",		""	},
/*  50 */	{ xnx_utsname,		1,	"utsname",	"x"	},
/*  51 */	{ 0,			Ukn,	"?",		""	},
/*  52 */	{ 0,			Ukn,	"?",		""	},
/*  53 */	{ 0,			Ukn,	"?",		""	},
/*  54 */	{ 0,			Ukn,	"?",		""	},
/*  55 */	{ sys_getitimer,	2,	"getitimer",	"dx"	},
/*  56 */	{ sys_setitimer,	3,	"setitimer",	"dxx"	}
};


void cxenix(struct pt_regs *regs)
{
	int sysno = regs->eax >> 8;

	if (sysno >= ARRAY_SIZE(cxenix_table))
		set_error(regs, iABI_errors(-EINVAL));
	else
		lcall7_dispatch(regs, &cxenix_table[sysno], 1);
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(cxenix);
#endif
