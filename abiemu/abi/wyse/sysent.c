/*
 * Copyright (c) 2001 Christoph Hellwig.
 * All rights reserved.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ident "%W% %G%"

/*
 * Wyse/V386 personality switch.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/personality.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <asm/uaccess.h>

#include <abi/svr4/sysent.h>
#include <abi/wyse/sysent.h>

#include <abi/signal.h>

#include <abi/util/errno.h>
#include <abi/util/sysent.h>
#include <abi/util/socket.h>


MODULE_DESCRIPTION("Wyse/V386 personality");
MODULE_AUTHOR("Christoph Hellwig, partially taken from iBCS");
MODULE_LICENSE("GPL");


/*
 * local functions
 */
static void	wyse_class_nfs(struct pt_regs *);
static void	wyse_class_tcp(struct pt_regs *);


/*
 * local variables
 */
static u_char wyse_err_table[] = {
	/*   0 -   9 */   0,   1,   2,    3,   4,   5,   6,   7,   8,   9,
	/*  10 -  19 */   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
	/*  20 -  29 */   20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
	/*  30 -  39 */   30,  31,  32,  33,  34,  45,  78,  46,  228, 46,
	/*  40 -  49 */   22, 231, 227, 200,  37,  38,  39,  40,  41,  42,
	/*  50 -  59 */   43,  44,  50,  51,  52,  53,  54,  55,  56,  57,
	/*  60 -  69 */   60,  61,  62,  63,  64,  65,  66,  67,  68,  69,
	/*  70 -  79 */   70,  71,  74,  76,  77,  22,  80,  81,  82,  83,
	/*  80 -  89 */   84,  85,  86,  87,  22,   4,  22, 233, 203, 204,
	/*  90 -  99 */  205, 206, 207, 208, 209, 210, 211, 212, 213, 214,
	/* 100 - 109 */  215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
	/* 110 - 119 */  225, 226, 229, 230, 202, 201, 237, 135, 137, 138,
	/* 120 - 122 */  139, 140, 234
};

/*
 * Map Linux RESTART* values (512,513,514) to EINTR
 */
static u_char lnx_err_table[] = {
	/* 512 - 514 */ EINTR, EINTR, EINTR
};

static struct map_segment wyse_err_map[] = {
	{ 0,	0 + sizeof(wyse_err_table) - 1,		wyse_err_table },
	{ 512,	512 + sizeof(lnx_err_table) - 1,	lnx_err_table },
	{ -1 }
};

static long linux_to_wyse_signals[NSIGNALS+1] = {
/*  0 -  3 */	0,		IBCS_SIGHUP,    IBCS_SIGINT,    IBCS_SIGQUIT,
/*  4 -  7 */	IBCS_SIGILL,	IBCS_SIGTRAP,   IBCS_SIGABRT,   -1,
/*  8 - 11 */	IBCS_SIGFPE,	IBCS_SIGKILL,   IBCS_SIGUSR1,   IBCS_SIGSEGV,
/* 12 - 15 */	IBCS_SIGUSR2,	IBCS_SIGPIPE,   IBCS_SIGALRM,   IBCS_SIGTERM,
/* 16 - 19 */	IBCS_SIGSEGV,	IBCS_SIGCHLD,   IBCS_SIGCONT,   IBCS_SIGSTOP,
/* 20 - 23 */	IBCS_SIGTSTP,	IBCS_SIGTTIN,   IBCS_SIGTTOU,   IBCS_SIGURG,
/* 24 - 27 */	IBCS_SIGGXCPU,	IBCS_SIGGXFSZ,  IBCS_SIGVTALRM, IBCS_SIGPROF,
/* 28 - 31 */	IBCS_SIGWINCH,	IBCS_SIGIO,     IBCS_SIGPWR,    -1,
/*	32 */	-1
};

static long wyse_to_linux_signals[NSIGNALS+1] = {
/*  0 -  3 */	0,		SIGHUP,         SIGINT,         SIGQUIT,
/*  4 -  7 */	SIGILL,		SIGTRAP,        SIGIOT,         SIGUNUSED,
/*  8 - 11 */	SIGFPE,		SIGKILL,        SIGUNUSED,      SIGSEGV,
/* 12 - 15 */	SIGUNUSED,	SIGPIPE,        SIGALRM,        SIGTERM,
/* 16 - 19 */	SIGUSR1,	SIGUSR2,        SIGCHLD,        SIGPWR,
/* 20 - 23 */	SIGWINCH,	SIGURG,         SIGPOLL,        SIGSTOP,
/* 24 - 27 */	SIGTSTP,	SIGCONT,        SIGTTIN,        SIGTTOU,
/* 28 - 31 */	SIGVTALRM,	SIGPROF,        SIGXCPU,        SIGXFSZ,
/*	32 */	-1
};

static char wyse_socktype[] = {
	SOCK_STREAM,
	SOCK_DGRAM,
	0,
	SOCK_RAW,
	SOCK_RDM,
	SOCK_SEQPACKET
};

static struct map_segment wyse_socktype_map[] = {
	{ 1, 6, wyse_socktype },
	{ -1 }
};

static struct map_segment wyse_sockopt_map[] =  {
	{ 0x0001, 0x0001, (char *)SO_DEBUG },
	{ 0x0002, 0x0002, (char *)__SO_ACCEPTCON },
	{ 0x0004, 0x0004, (char *)SO_REUSEADDR },
	{ 0x0008, 0x0008, (char *)SO_KEEPALIVE },
	{ 0x0010, 0x0010, (char *)SO_DONTROUTE },
	{ 0x0020, 0x0020, (char *)SO_BROADCAST },
	{ 0x0040, 0x0040, (char *)SO_USELOOPBACK },
	{ 0x0080, 0x0080, (char *)SO_LINGER },
	{ 0x0100, 0x0100, (char *)SO_OOBINLINE },
	{ 0x0200, 0x0200, (char *)SO_ORDREL },
	{ 0x0400, 0x0400, (char *)SO_IMASOCKET },
	{ 0x1001, 0x1001, (char *)SO_SNDBUF },
	{ 0x1002, 0x1002, (char *)SO_RCVBUF },
	{ 0x1003, 0x1003, (char *)SO_SNDLOWAT },
	{ 0x1004, 0x1004, (char *)SO_RCVLOWAT },
	{ 0x1005, 0x1005, (char *)SO_SNDTIMEO },
	{ 0x1006, 0x1006, (char *)SO_RCVTIMEO },
	{ 0x1007, 0x1007, (char *)SO_ERROR },
	{ 0x1008, 0x1008, (char *)SO_TYPE },
	{ 0x1009, 0x1009, (char *)SO_PROTOTYPE },
	{ -1 }
};

static struct map_segment wyse_af_map[] =  {
	{ 0, 2, NULL },
	{ -1 }
};


static struct sysent wyse_nfscall_table[] = {
/*   0 */	{ 0,			Ukn,	"nfs_svc",	""	},
/*   1 */	{ 0,			Ukn,	"async_daemon",	""	},
/*   2 */	{ 0,			Ukn,	"nfs_getfh",	""	},
/*   3 */	{ 0,			Ukn,	"nfsmount",	""	},
};

static struct sysent wyse_tcpcall_table[] = {
/*   0 */	{ sys_select,		5,	"select",	"dxxxx"	},
/*   1 */	{ wyse_socket,		3,	"socket",	"ddd"	},
/*   2 */	{ sys_connect,		3,	"connect",	"dxd"	},
/*   3 */	{ sys_accept,		3,	"accept",	"dxx"	},
/*   4 */	{ wyse_send,		4,	"send",		"dxdd"	},
/*   5 */	{ wyse_recv,		4,	"recv",		"dxdd"	},
/*   6 */	{ sys_bind,		3,	"bind",		"dxd"	},
/*   7 */	{ wyse_setsockopt,	5,	"setsockopt",	"dddxx"	},
/*   8 */	{ sys_listen,		2,	"listen",	"dd"	},
/*   9 */	{ 0,			3,	"recvmsg",	"dxd"	},
/*  10 */	{ 0,			3,	"sendmsg",	"dxd"	},
/*  11 */	{ wyse_getsockopt,	5,	"getsockopt",	"dddxx"	},
/*  12 */	{ wyse_recvfrom,	6,	"recvfrom",	"dxddxd"},
/*  13 */	{ wyse_sendto,		6,	"sendto",	"dxddxd"},
/*  14 */	{ sys_shutdown,		2,	"shutdown",	"dd"	},
/*  15 */	{ sys_socketpair,	4,	"socketpair",	"dddx"	},
/*  16 */	{ 0,			Ukn,	"trace",	""	},
/*  17 */	{ sys_getpeername,	3,	"getpeername",	"dxx"	},
/*  18 */	{ sys_getsockname,	Spl,	"getsockname",	"dxx"	},
/*  19 */	{ wyse_wait3,		1,	"wait3",	"x"	},
};


static struct sysent wyse_syscall_table[] = {
/*   0 */	{ abi_syscall,		Fast,	"syscall",	""	},
/*   1 */	{ sys_exit,		1,	"exit",		"d"	},
/*   2 */	{ abi_fork,		Spl,	"fork",		""	},
/*   3 */	{ abi_read,		3,	"read",		"dpd"	},
/*   4 */	{ sys_write,		3,	"write",	"dpd"	},
/*   5 */	{ svr4_open,		3,	"open",		"soo"	},
/*   6 */	{ sys_close,		1,	"close",	"d"	},
/*   7 */	{ abi_wait,		Spl,	"wait",		"xxx"	},
/*   8 */	{ sys_creat,		2,	"creat",	"so"	},
/*   9 */	{ sys_link,		2,	"link",		"ss"	},
/*  10 */	{ sys_unlink,		1,	"unlink",	"s"	},
/*  11 */	{ abi_exec,		Spl,	"exec",		"sxx"	},
/*  12 */	{ sys_chdir,		1,	"chdir",	"s"	},
/*  13 */	{ abi_time,		0,	"time",		""	},
/*  14 */	{ svr4_mknod,		3,	"mknod",	"soo"	},
/*  15 */	{ sys_chmod,		2,	"chmod",	"so"	},
/*  16 */	{ sys_chown,		3,	"chown",	"sdd"	},
/*  17 */	{ abi_brk,		1,	"brk/break",	"x"	},
/*  18 */	{ svr4_stat,		2,	"stat",		"sp"	},
/*  19 */	{ sys_lseek,		3,	"seek/lseek",	"ddd"	},
/*  20 */	{ abi_getpid,		Spl,	"getpid",	""	},
/*  21 */	{ 0,			Ukn,	"mount",	""	},
/*  22 */	{ sys_umount,		1,	"umount",	"s"	},
/*  23 */	{ sys_setuid,		1,	"setuid",	"d"	},
/*  24 */	{ abi_getuid,		Spl,	"getuid",	""	},
/*  25 */	{ sys_stime,		1,	"stime",	"d"	},
/*  26 */	{ wyse_ptrace,		4,	"ptrace",	"xdxx"	},
/*  27 */	{ sys_alarm,		1,	"alarm",	"d"	},
/*  28 */	{ svr4_fstat,		2,	"fstat",	"dp"	},
/*  29 */	{ sys_pause,		0,	"pause",	""	},
/*  30 */	{ sys_utime,		2,	"utime",	"xx"	},
/*  31 */	{ 0,			Ukn,	"stty",		""	}, /*   31 */
/*  32 */	{ 0,			Ukn,	"gtty",		""	},
/*  33 */	{ sys_access,		2,	"access",	"so"	},
/*  34 */	{ sys_nice,		1,	"nice",		"d"	},
/*  35 */	{ svr4_statfs,		4,	"statfs",	"spdd"	},
/*  36 */	{ sys_sync,		0,	"sync",		""	},
/*  37 */	{ abi_kill,		2,	"kill",		"dd"	},
/*  38 */	{ svr4_fstatfs,		4,	"fstatfs",	"dpdd"	},
/*  39 */	{ abi_procids,		Spl,	"procids",	"d"	},
/*  40 */	{ 0,			Ukn,	"cxenix",	""	},
/*  41 */	{ sys_dup,		1,	"dup",		"d"	},
/*  42 */	{ abi_pipe,		Spl,	"pipe",		""	},
/*  43 */	{ sys_times,		1,	"times",	"p"	},
/*  44 */	{ 0,			4,	"prof",		"xxxx"	},
/*  45 */	{ 0,			Ukn,	"lock/plock",	""	},
/*  46 */	{ sys_setgid,		1,	"setgid",	"d"	},
/*  47 */	{ abi_getgid,		Spl,	"getgid",	""	},
/*  48 */	{ abi_sigfunc,		Fast,	"sigfunc",	"xxx"	},
/*  49 */	{ svr4_msgsys,		Spl,	"msgsys",	"dxddd"	},
/*  50 */	{ svr4_sysi86,		3,	"sysi86/sys3b",	"d"	},
/*  51 */	{ sys_acct,		1,	"acct/sysacct",	"x"	},
/*  52 */	{ svr4_shmsys,		Fast,	"shmsys",	"ddxo"	},
/*  53 */	{ svr4_semsys,		Spl,	"semsys",	"dddx"	},
/*  54 */	{ svr4_ioctl,		Spl,	"ioctl",	"dxx"	},
/*  55 */	{ 0,			3,	"uadmin",	"xxx"	},
/*  56 */	{ 0,			Ukn,	"?",		""	},
/*  57 */	{ v7_utsname,		1,	"utsys",	"x"	},
/*  58 */	{ sys_fsync,		1,	"fsync",	"d"	},
/*  59 */	{ abi_exec,		Spl,	"execv",	"spp"	},
/*  60 */	{ sys_umask,		1,	"umask",	"o"	},
/*  61 */	{ sys_chroot,		1,	"chroot",	"s"	},
/*  62 */	{ svr4_fcntl,		3,	"fcntl",	"dxx"	},
/*  63 */	{ svr4_ulimit,		2,	"ulimit",	"xx"	},
/*  64 */	{ 0,			Ukn,	"?",		""	},
/*  65 */	{ 0,			Ukn,	"?",		""	},
/*  66 */	{ 0,			Ukn,	"?",		""	},
/*  67 */	{ 0,			Ukn,	"?",		""	},
/*  68 */	{ 0,			Ukn,	"?",		""	},
/*  69 */	{ 0,			Ukn,	"?",		""	},
/*  70 */	{ 0,			Ukn,	"advfs",	""	},
/*  71 */	{ 0,			Ukn,	"unadvfs",	""	},
/*  72 */	{ 0,			Ukn,	"rmount",	""	},
/*  73 */	{ 0,			Ukn,	"rumount",	""	},
/*  74 */	{ 0,			Ukn,	"rfstart",	""	},
/*  75 */	{ 0,			Ukn,	"?",		""	},
/*  76 */	{ 0,			Ukn,	"rdebug",	""	},
/*  77 */	{ 0,			Ukn,	"rfstop",	""	},
/*  78 */	{ 0,			Ukn,	"rfsys",	""	},
/*  79 */	{ sys_rmdir,		1,	"rmdir",	"s"	},
/*  80 */	{ abi_mkdir,		2,	"mkdir",	"so"	},
/*  81 */	{ svr4_getdents,	3,	"getdents",	"dxd"	},
/*  82 */	{ 0,			Ukn,	"libattach",	""	},
/*  83 */	{ 0,			Ukn,	"libdetach",	""	},
/*  84 */	{ svr4_sysfs,		3,	"sysfs",	"dxx"	},
/*  85 */	{ svr4_getmsg,		Spl,	"getmsg",	"dxxx"	},
/*  86 */	{ svr4_putmsg,		Spl,	"putmsg",	"dxxd"	},
/*  87 */	{ sys_poll,		3,	"poll",		"xdd"	},
/*  88 */	{ 0,			Ukn,	"nosys88",	""	},
/*  89 */	{ 0,			Ukn,	"nosys89",	""	},
/*  90 */	{ 0,			Ukn,	"nosys90",	""	},
/*  91 */	{ 0,			Ukn,	"nosys91",	""	},
/*  92 */	{ 0,			Ukn,	"nosys92",	""	},
/*  93 */	{ 0,			Ukn,	"nosys93",	""	},
/*  94 */	{ 0,			Ukn,	"nosys94",	""	},
/*  95 */	{ 0,			Ukn,	"nosys95",	""	},
/*  96 */	{ 0,			Ukn,	"nosys96",	""	},
/*  97 */	{ 0,			Ukn,	"nosys97",	""	},
/*  98 */	{ 0,			Ukn,	"nosys98",	""	},
/*  99 */	{ 0,			Ukn,	"nosys99",	""	},
/* 100 */	{ 0,			Ukn,	"nosys100",	""	},
/* 101 */	{ 0,			Ukn,	"nosys101",	""	},
/* 102 */	{ 0,			Ukn,	"nosys102",	""	},
/* 103 */	{ 0,			Ukn,	"nosys103",	""	},
/* 104 */	{ 0,			Ukn,	"nosys104",	""	},
/* 105 */	{ 0,			Ukn,	"nosys105",	""	},
/* 106 */	{ 0,			Ukn,	"nosys106",	""	},
/* 107 */	{ 0,			Ukn,	"nosys107",	""	},
/* 108 */	{ 0,			Ukn,	"nosys108",	""	},
/* 109 */	{ 0,			Ukn,	"nosys109",	""	},
/* 110 */	{ 0,			Ukn,	"nosys110",	""	},
/* 111 */	{ 0,			Ukn,	"nosys111",	""	},
/* 112 */	{ 0,			Ukn,	"nosys112",	""	},
/* 113 */	{ 0,			Ukn,	"nosys113",	""	},
/* 114 */	{ 0,			Ukn,	"nosys114",	""	},
/* 115 */	{ 0,			Ukn,	"nosys115",	""	},
/* 116 */	{ 0,			Ukn,	"nosys116",	""	},
/* 117 */	{ 0,			Ukn,	"nosys117",	""	},
/* 118 */	{ 0,			Ukn,	"nosys118",	""	},
/* 119 */	{ 0,			Ukn,	"nosys119",	""	},
/* 120 */	{ 0,			Ukn,	"nosys120",	""	},
/* 121 */	{ 0,			Ukn,	"nosys121",	""	},
/* 122 */	{ 0,			Ukn,	"nosys122",	""	},
/* 123 */	{ 0,			Ukn,	"nosys123",	""	},
/* 124 */	{ 0,			Ukn,	"nosys124",	""	},
/* 125 */	{ 0,			Ukn,	"nosys125",	""	},
/* 126 */	{ 0,			Ukn,	"nosys126",	""	},
/* 127 */	{ 0,			Ukn, 	"nosys127",	""	},
/* 128 */	{ svr4_lstat,		2,	"lstat",	"sp"	},
/* 129 */	{ sys_readlink,		3,	"readlink",	"spd"	},
/* 130 */	{ sys_symlink,		2,	"symlink",	"ss"	},
/* 131 */	{ wyse_class_tcp,	Fast,	"",		""	},
/* 132 */	{ wyse_class_nfs,	Fast,	"",		""	},
/* 133 */	{ wyse_gethostname,	2,	"gethostname",	"xd"	},
/* 134 */	{ sys_sethostname,	2,	"sethostname",	"sd"	},
/* 135 */	{ wyse_getdomainname,	2,	"getdomainname","xd"	},
/* 136 */	{ sys_setdomainname,	2,	"setdomainname","sd"	},
/* 137 */	{ 0,			Ukn,	"?",		""	},
/* 138 */	{ sys_setreuid,		2,	"setreuid",	"dd"	},
/* 139 */	{ sys_setregid,		2,	"setregid",	"dd"	},
};

static void
wyse_class_nfs(struct pt_regs *regs)
{

	int sysno = regs->eax >> 8;

	if (sysno >= ARRAY_SIZE(wyse_nfscall_table))
		set_error(regs, iABI_errors(-EINVAL));
	else
		lcall7_dispatch(regs, &wyse_nfscall_table[sysno], 1);
}

static void
wyse_class_tcp(struct pt_regs *regs)
{
	int sysno = regs->eax >> 8;

	if (sysno >= ARRAY_SIZE(wyse_tcpcall_table))
		set_error(regs, iABI_errors(-EINVAL));
	else
		lcall7_dispatch(regs, &wyse_tcpcall_table[sysno], 1);
}

static void
wyse_lcall7(int segment, struct pt_regs *regs)
{
	int sysno = regs->eax & 0xff;

	if (sysno >= ARRAY_SIZE(wyse_syscall_table))
		set_error(regs, iABI_errors(-EINVAL));
	else
		lcall7_dispatch(regs, &wyse_syscall_table[sysno], 1);
}

static struct exec_domain wyse_exec_domain = {
	name:		"Wyse/386",
	handler:	wyse_lcall7,
	pers_low:	4 /* PER_WYSEV386 */,
	pers_high:	4 /* PER_WYSEV386 */,
	signal_map:	wyse_to_linux_signals,
	signal_invmap:	linux_to_wyse_signals,
	err_map:	wyse_err_map,
	socktype_map:	wyse_socktype_map,
	sockopt_map:	wyse_sockopt_map,
	af_map:		wyse_af_map,
	module:		THIS_MODULE
};


static int __init
wyse_module_init(void)
{
	return register_exec_domain(&wyse_exec_domain);
}

static void __exit
wyse_module_exit(void)
{
	unregister_exec_domain(&wyse_exec_domain);
}

module_init(wyse_module_init);
module_exit(wyse_module_exit);
