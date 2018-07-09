/*
 * Copyright (c) 1999 Tigran Aivazian.
 * Copyright (c) 2000, 2001 Christoph Hellwig.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ident "%W% %G%"

/*
 * UnixWare 7/OpenUnix 8 personality switch.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/personality.h>
#include <linux/syscalls.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <asm/uaccess.h>

#include <abi/signal.h>

#include <abi/svr4/sysent.h>
#include <abi/uw7/sysent.h>

#include <abi/util/errno.h>
#include <abi/util/sysent.h>
#include <abi/util/socket.h>

MODULE_DESCRIPTION("UnixWare7/OpenUnix8 personality");
MODULE_AUTHOR("Tigran Aivazian, Christoph Hellwig");
MODULE_LICENSE("GPL");


/*
 * We could remove some of the long identity mapped runs but at the
 * expense of extra comparisons for each mapping at run time...
 */
static u_char uw7_err_table[] = {
/*   0 -   9 */   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,
/*  10 -  19 */  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
/*  20 -  29 */  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
/*  30 -  39 */  30,  31,  32,  33,  34,  45,  78,  46,  89,  93,
/*  40 -  49 */  90,  90,  35,  36,  37,  38,  39,  40,  41,  42,
/*  50 -  59 */  43,  44,  50,  51,  52,  53,  54,  55,  56,  57,
/*  60 -  69 */  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,
/*  70 -  79 */  70,  71,  74,  76,  77,  79,  80,  81,  82,  83,
/*  80 -  89 */  84,  85,  86,  87,  88,  91,  92,  94,  95,  96,
/*  90 -  99 */  97,  98,  99, 120, 121, 122, 123, 124, 125, 126,
/* 100 - 109 */ 127, 128, 129, 130, 131, 132, 133, 134, 143, 144,
/* 110 - 119 */ 145, 146, 147, 148, 149, 150,  22, 135, 137, 138,
/* 120 - 122 */ 139, 140,  28
};

/*
 * Map Linux RESTART* values (512,513,514) to EINTR
 */
static u_char lnx_err_table[] = {
/* 512 - 514 */ EINTR, EINTR, EINTR
};

struct map_segment uw7_err_map[] = {
	{ 0,	0+sizeof(uw7_err_table)-1,	uw7_err_table },
	{ 512,  512+sizeof(lnx_err_table)-1,	lnx_err_table },
	{ -1 }
};

static long linux_to_uw7_signals[NSIGNALS+1] = {
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

static long uw7_to_linux_signals[NSIGNALS+1] = {
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

static char uw7_socktype[] = {
	SOCK_STREAM,
	SOCK_DGRAM,
	0,
	SOCK_RAW,
	SOCK_RDM,
	SOCK_SEQPACKET
};

static struct map_segment uw7_socktype_map[] = {
	{ 1, 6, uw7_socktype },
	{ -1 }
};

static struct map_segment uw7_sockopt_map[] =  {
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

static struct map_segment uw7_af_map[] =  {
	{ 0, 2, NULL },
	{ -1 }
};


static struct sysent uw7_syscall_table[] = {
/*   0 */	{ abi_syscall,		Fast,	"syscall",	""	},
/*   2 */	{ sys_exit,		1,	"exit",		"d"	},
/*   3 */	{ abi_fork,		Spl,	"fork",		""	},
/*   4 */	{ abi_read,		3,	"read",		"dpd"	},
/*   5 */	{ sys_write,		3,	"write",	"dpd"	},
/*   6 */	{ svr4_open,		3,	"open",		"soo"	},
/*   7 */	{ sys_close,		1,	"close",	"d"	},
/*   8 */	{ abi_wait,		Spl,	"wait",		"xxx"	},
/*   9 */	{ sys_creat,		2,	"creat",	"so"	},
/*  10 */	{ sys_link,		2,	"link",		"ss"	},
/*  11 */	{ sys_unlink,		1,	"unlink",	"s"	},
/*  12 */	{ abi_exec,		Spl,	"exec",		"sxx"	},
/*  13 */	{ sys_chdir,		1,	"chdir",	"s"	},
/*  14 */	{ abi_time,		0,	"time",		""	},
/*  15 */	{ svr4_mknod,		3,	"mknod",	"soo"	},
/*  16 */	{ sys_chmod,		2,	"chmod",	"so"	},
/*  17 */	{ sys_chown,		3,	"chown",	"sdd"	},
/*  18 */	{ abi_brk,		1,	"brk/break",	"x"	},
/*  19 */	{ svr4_stat,		2,	"stat",		"sp"	},
/*  21 */	{ sys_lseek,		3,	"seek/lseek",	"ddd"	},
/*  20 */	{ abi_getpid,		Spl,	"getpid",	""	},
/*  21 */	{ 0,			Ukn,	"mount",	""	},
/*  22 */	{ sys_umount,		1,	"umount",	"s"	},
/*  23 */	{ sys_setuid,		1,	"setuid",	"d"	},
/*  24 */	{ abi_getuid,		Spl,	"getuid",	""	},
/*  25 */	{ sys_stime,		1,	"stime",	"d"	},
/*  26 */	{ 0,			4,	"ptrace",	""	},
/*  27 */	{ sys_alarm,		1,	"alarm",	"d"	},
/*  28 */	{ svr4_fstat,		2,	"fstat",	"dp"	},
/*  21 */	{ sys_pause,		0,	"pause",	""	},
/*  30 */	{ sys_utime,		2,	"utime",	"xx"	},
/*  31 */	{ uw7_stty,		2,	"stty",		"dd"	},
/*  32 */	{ uw7_gtty,		2,	"gtty",		"dd"	},
/*  33 */	{ uw7_access,		2,	"access",	"so"	},
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
/*  44 */	{ 0,			4,	"",		""	},
/*  45 */	{ 0,			Ukn,	"plock",	""	},
/*  46 */	{ sys_setgid,		1,	"setgid",	"d"	},
/*  47 */	{ abi_getgid,		Spl,	"getgid",	""	},
/*  48 */	{ abi_sigfunc,		Fast,	"sigfunc",	"xxx"	},
/*  49 */	{ svr4_msgsys,		Spl,	"msgsys",	"dxddd"	},
/*  50 */	{ svr4_sysi86,		3,	"sysi86/sys3b",	"d"	},
/*  51 */	{ sys_acct,		1,	"acct/sysacct",	"x"	},
/*  52 */	{ svr4_shmsys,		Fast,	"shmsys",	"ddxo"	},
/*  53 */	{ svr4_semsys,		Spl,	"semsys",	"dddx"	},
/*  54 */	{ uw7_ioctl,		Spl,	"ioctl",	"dxx"	},
/*  55 */	{ 0,			3,	"uadmin",	"xxx"	},
/*  56 */	{ 0,			Ukn,	"unimpl/exch",	""	},
/*  57 */	{ v7_utsname,		1,	"utsys",	"x"	},
/*  58 */	{ sys_fsync,		1,	"fsync",	"d"	},
/*  59 */	{ abi_exec,		Spl,	"execv",	"spp"	},
/*  60 */	{ sys_umask,		1,	"umask",	"o"	},
/*  61 */	{ sys_chroot,		1,	"chroot",	"s"	},
/*  62 */	{ svr4_fcntl,		3,	"fcntl",	"dxx"	},
/*  63 */	{ svr4_ulimit,		2,	"ulimit",	"xx"	},

/*
 *	64-69 were reserved for the UNIX PC, and are now use for NUMA calls.
 */
/*  64 */	{ 0,			Ukn,	"cg_ids",	""	},
/*  65 */	{ 0,			Ukn,	"cg_processors",""	},
/*  66 */	{ 0,			Ukn,	"cg_info",	""	},
/*  67 */	{ 0,			Ukn,	"cg_bind",	""	},
/*  68 */	{ 0,			Ukn,	"cg_current",	""	},
/*  69 */	{ 0,			Ukn,	"cg_memloc",	""	},
/*  70 */	{ 0,			Ukn,	"unimpl/advfs",	""	},
/*  71 */	{ 0,			Ukn,	"unimpl/unadvfs",""	},
/*  72 */	{ 0,			Ukn,	"unimpl/rmount",""	},
/*  73 */	{ 0,			Ukn,	"unimpl/rumount",""	},
/*  74 */	{ 0,			Ukn,	"unimpl/rfstart",""	},
/*  75 */	{ 0,			Ukn,	"unimpl 75",	""	},
/*  76 */	{ 0,			Ukn,	"unimpl/rdebug",""	},
/*  77 */	{ 0,			Ukn,	"unimpl/rfstop",""	},
/*  78 */	{ 0,			Ukn,	"rfsys",	""	},
/*  89 */	{ sys_rmdir,		1,	"rmdir",	"s"	},
/*  80 */	{ sys_mkdir,		2,	"mkdir",	"so"	},
/*  81 */	{ svr4_getdents,	3,	"getdents",	"dxd"	},
/*  82 */	{ 0,			Ukn,	"unimpl/libattach",""	},
/*  83 */	{ 0,			Ukn,	"unimpl/libdetach",""	},
/*  84 */	{ svr4_sysfs,		3,	"sysfs",	"dxx"	},
/*  85 */	{ svr4_getmsg,		Spl,	"getmsg",	"dxxx"	},
/*  86 */	{ svr4_putmsg,		Spl,	"putmsg",	"dxxd"	},
/*  87 */	{ sys_poll,		3,	"poll",		"xdd"	},
/*  88 */	{ svr4_lstat,		2,	"lstat",	"sp"	},
/*  89 */	{ sys_symlink,		2,	"symlink",	"ss"	},
/*  90 */	{ sys_readlink,		3,	"readlink",	"spd"	},
/*  91 */	{ sys_setgroups,	2,	"setgroups",	"dp"	},
/*  92 */	{ sys_getgroups,	2,	"getgroups",	"dp"	},
/*  93 */	{ sys_fchmod,		2,	"fchmod",	"do"	},
/*  94 */	{ sys_fchown,		3,	"fchown",	"ddd"	},
/*  95 */	{ abi_sigprocmask,	3,	"sigprocmask",	"dxx"	},
/*  96 */	{ abi_sigsuspend,	Spl,	"sigsuspend",	"x"	},
/*  97 */	{ uw7_sigaltstack,	2,	"sigaltstack",	"xx"	},
/*  98 */	{ abi_sigaction,	3,	"sigaction",	"dxx"	},
/*  99 */	{ svr4_sigpending,	2,	"sigpending",	"dp"	},
/* 100 */	{ uw7_context,		Spl,	"ucontext",	""	},
/* 101 */	{ 0,			Ukn,	"evsys",	""	},
/* 102 */	{ 0,			Ukn,	"evtrapret",	""	},
/* 103 */	{ svr4_statvfs,		2,	"statvfs",	"sp"	},
/* 104 */	{ svr4_fstatvfs,	2,	"fstatvfs",	"dp"	},
/* 105 */	{ 0,			Ukn,	"reserved 105",	""	},
/* 106 */	{ 0,			Ukn,	"nfssys",	""	},
/* 107 */	{ svr4_waitid,		4,	"waitid",	"ddxd"	},
/* 108 */	{ 0,			3,	"sigsendsys",	"ddd"	},
/* 109 */	{ svr4_hrtsys,		Spl,	"hrtsys",	"xxx"	},
/* 110 */	{ 0,			3,	"acancel",	"dxd"	},
/* 111 */	{ 0,			Ukn,	"async",	""	},
/* 112 */	{ 0,			Ukn,	"priocntlsys",	""	},
/* 113 */	{ svr4_pathconf,	2,	"pathconf",	"sd"	},
/* 114 */	{ 0,			3,	"mincore",	"xdx"	},
/* 115 */	{ svr4_mmap,		6,	"mmap",		"xxxxdx"},
/* 116 */	{ sys_mprotect,		3,	"mprotect",	"xdx"	},
/* 117 */	{ sys_munmap,		2,	"munmap",	"xd"	},
/* 118 */	{ svr4_fpathconf,	2,	"fpathconf",	"dd"	},
/* 119 */	{ abi_fork,		Spl,	"vfork",	""	},
/* 120 */	{ sys_fchdir,		1,	"fchdir",	"d"	},
/* 121 */	{ sys_readv,		3,	"readv",	"dxd"	},
/* 122 */	{ sys_writev,		3,	"writev",	"dxd"	},
/* 123 */	{ uw7_xstat,		3,	"xstat",	"dsx"	},
/* 124 */	{ uw7_lxstat,     	3,	"lxstat",	"dsx"	},
/* 125 */	{ uw7_fxstat,		3,	"fxstat",	"ddx"	},
/* 126 */	{ svr4_xmknod,		4,	"xmknod",	"dsox"	},
/* 127 */	{ 0,			Spl,	"syslocal",	"d"	},
/* 128 */	{ svr4_getrlimit,	2,	"setrlimit",	"dx"	},
/* 129 */	{ svr4_setrlimit,	2,	"getrlimit",	"dx"	},
/* 130 */	{ sys_lchown,		3,	"lchown",	"sdd"	},
/* 131 */	{ 0,			Ukn,	"memcntl",	""	},
#if defined(CONFIG_ABI_XTI)
/* 132 */	{ svr4_getpmsg,		5,	"getpmsg",	"dxxxx"	},
/* 133 */	{ svr4_putpmsg,		5,	"putpmsg",	"dxxdd"	},
#else
/* 132 */	{ 0,			5,	"getpmsg",	"dxxxx"	},
/* 133 */	{ 0,			5,	"putpmsg",	"dxxdd"	},
#endif
/* 134 */	{ sys_rename,		2,	"rename",	"ss"	},
/* 135 */	{ abi_utsname,		1,	"uname",	"x"	},
/* 136 */	{ uw7_setegid,		1,	"setegid",	"d"	},
/* 137 */	{ svr4_sysconfig,	1,	"sysconfig",	"d"	},
/* 138 */	{ 0,			Ukn,	"adjtime",	""	},
/* 139 */	{ svr4_sysinfo,		3,	"systeminfo",	"dsd"	},
/* 140 */	{ socksys_syscall,	1,	"socksys_syscall","x"	},
/* 141 */	{ uw7_seteuid,		1,	"seteuid",	"d"	},
/* 142 */	{ 0,			Ukn,	"unimpl 142",	""	},
/* 143 */	{ 0,			Ukn,	"keyctl",	""	},
/* 144 */	{ 0,			2,	"secsys",	"dx"	},
/* 145 */	{ 0,			4,	"filepriv",	"sdxd"	},
/* 146 */	{ 0,			3,	"procpriv",	"dxd"	},
/* 147 */	{ 0,			3,	"devstat",	"sdx"	},
/* 148 */	{ 0,			5,	"aclipc",	"ddddx"	},
/* 149 */	{ 0,			3,	"fdevstat",	"ddx"	},
/* 150 */	{ 0,			3,	"flvlfile",	"ddx"	},
/* 151 */	{ 0,			3,	"lvlfile",	"sdx"	},
/* 152 */	{ 0,			Ukn,	"sendv",	""	},
/* 153 */	{ 0,			2,	"lvlequal",	"xx"	},
/* 154 */	{ 0,			2,	"lvlproc",	"dx"	},
/* 155 */	{ 0,			Ukn,	"unimpl 155",	""	},
/* 156 */	{ 0,			4,	"lvlipc",	"dddx"	},
/* 157 */	{ 0,			4,	"acl",		"sddx"	},
/* 158 */	{ 0,			Ukn,	"auditevt",	""	},
/* 159 */	{ 0,			Ukn,	"auditctl",	""	},
/* 160 */	{ 0,			Ukn,	"auditdmp",	""	},
/* 161 */	{ 0,			Ukn,	"auditlog",	""	},
/* 162 */	{ 0,			Ukn,	"auditbuf",	""	},
/* 163 */	{ 0,			2,	"lvldom",	"xx"	},
/* 164 */	{ 0,			Ukn,	"lvlvfs",	""	},
/* 165 */	{ 0,			2,	"mkmld",	"so"	},
/* 166 */	{ uw7_mldmode,		1,	"mldmode",	"d"	},
/* 167 */	{ 0,			2,	"secadvise",	"xx"	},
/* 168 */	{ 0,			Ukn,	"online",	""	},
/* 169 */	{ sys_setitimer,	3,	"setitimer",	"dxx"	},
/* 170 */	{ sys_getitimer,	2,	"getitimer",	"dx"	},
/* 171 */	{ sys_gettimeofday,	2,	"gettimeofday",	"xx"	},
/* 172 */	{ sys_settimeofday,	2,	"settimeofday",	"xx"	},
/* 173 */	{ 0,			Ukn,	"lwpcreate",	""	},
/* 174 */	{ 0,			Ukn,	"lwpexit",	""	},
/* 175 */	{ 0,			Ukn,	"lwpwait",	""	},
/* 176 */	{ 0,			Ukn,	"lwpself",	""	},
/* 177 */	{ 0,			Ukn,	"lwpinfo",	""	},
/* 178 */	{ 0,			Ukn,	"lwpprivate",	""	},
/* 179 */	{ 0,			Ukn,	"processorbind",""	},
/* 180 */	{ 0,			Ukn,	"processorexbind",""	},
/* 181 */	{ 0,			Ukn,	"unimpl 181",	""	},
/* 182 */	{ 0,			Ukn, 	"sendv64",	""	},
/* 183 */	{ 0,			Ukn,	"prepblock",	""	},
/* 184 */	{ 0,			Ukn,	"block",	""	},
/* 185 */	{ 0,			Ukn,	"rdblock",	""	},
/* 186 */	{ 0,			Ukn,	"unblock",	""	},
/* 187 */	{ 0,			Ukn,	"cancelblock",	""	},
/* 188 */	{ 0,			Ukn,	"unimpl 188",	""	},
/* 189 */	{ uw7_pread,		4,	"pread",	"dsdd"	},
/* 190 */	{ uw7_pwrite,		4,	"pwrite",	"dsdd"	},
/* 191 */	{ sys_truncate,		2,	"truncate",	"sd"	},
/* 192 */	{ sys_ftruncate,	2,	"ftruncate",	"dd"	},
/* 193 */	{ 0,			Ukn,	"lwpkill",	""	},
/* 194 */	{ 0,			Ukn,	"sigwait",	""	},
/* 195 */	{ abi_fork,		Spl,	"fork1",	""	},
/* 196 */ 	{ abi_fork,		Spl,	"forkall",	""	},

/*
 *	197-202 are for loadable kernel module support.
 */
/* 197 */	{ 0,			Ukn,	"modload",	""	},
/* 198 */	{ 0,			Ukn,	"moduload",	""	},
/* 199 */	{ 0,			Ukn,	"modpath",	""	},
/* 200 */	{ 0,			Ukn,	"modstat",	""	},
/* 201 */	{ 0,			Ukn,	"modadm",	""	},
/* 202 */	{ 0,			Ukn,	"getksym",	""	},

/* 203 */	{ 0,			Ukn,	"lwpsuspend",	""	},
/* 204 */	{ 0,			Ukn,	"lwpcontinue",	""	},
/* 205 */	{ 0,			Ukn,	"priocntllst",	""	},
/* 206 */	{ uw7_sleep,		1,	"sleep",	"d"	},

/*
 *	207-209 are for post/wait synchronisation.
 */
/* 207 */	{ 0,			Ukn,	"lwp_sema_wait",""	},
/* 208 */	{ 0,			Ukn,	"lwp_sema_post",""	},
/* 209 */	{ 0,			Ukn,	"lwp_sema_trywait",""	},

/* 210 */	{ 0,			Ukn,	"reserved 210",	""	},
/* 211 */	{ 0,			Ukn,	"unused 211",	""	},
/* 212 */	{ 0,			Ukn,	"unused 212",	""	},
/* 213 */	{ 0,			Ukn,	"unused 213",	""	},
/* 214 */	{ 0,			Ukn,	"unused 214",	""	},
/* 215 */	{ 0,			Ukn,	"unused 215",	""	},

/*
 *	216-226 are for LFS (Large File Summit) support
 */
/* 216 */	{ uw7_fstatvfs64,	2,	"fstatvfs64",	"dp"	},
/* 217 */	{ uw7_statvfs64,	2,	"statvfs64",	"sp"	},
/* 218 */	{ uw7_ftruncate64,	3,	"ftruncate64",	"sdd"	},
/* 219 */	{ uw7_truncate64,	3,	"truncate64",	"ddd"	},
/* 220 */	{ uw7_getrlimit64,	2,	"getrlimit64",	"dp"	},
/* 221 */	{ uw7_setrlimit64,	2,	"setrlimit64",	"dp"	},
/* 222 */	{ uw7_lseek64,		4,	"lseek64",	"dddd"	},
/* 223 */	{ uw7_mmap64,		7,	"mmap64",      "xxxxdxx"},
/* 224 */	{ uw7_pread64,		5,	"pread64",	"dsddd"	},
/* 225 */	{ uw7_pwrite64,		5,	"pwrite64",	"dsddd"	},
/* 226 */	{ uw7_creat64,		2,	"creat64",	"so"	},

/* 227 */	{ 0,			Ukn,	"dshmsys",	""	},
/* 228 */	{ 0,			Ukn,	"invlpg",	""	},

/*
 *	229-234 are used for SSI clustering (Nonstop cluster)
 */
/* 229 */   { 0,			Ukn,	"rfork1",	""	},
/* 230 */   { 0,			Ukn,	"rforkall",	""	},
/* 231 */   { 0,			Ukn,	"rexecve",	""	},
/* 232 */   { 0,			Ukn,	"migrate",	""	},
/* 233 */   { 0,			Ukn,	"kill3",	""	},
/* 234 */   { 0,			Ukn,	"ssisys",	""	},

/*
 *	235-248 are for kernel-based sockets (Yeah, SVR5 finally got sockets)
 */
/* 235 */   { 0,			Ukn,	"xaccept",	""	},
/* 236 */   { 0,			Ukn,	"xbind",	""	},
/* 237 */   { 0,			Ukn,	"xbindresvport",""	},
/* 238 */   { 0,			Ukn,	"xconnect",	""	},
/* 239 */   { 0,			Ukn,	"xgetsockaddr",	""	},
/* 240 */   { 0,			Ukn,	"xgetsockopt",	""	},
/* 241 */   { 0,			Ukn,	"xlisten",	""	},
/* 242 */   { 0,			Ukn,	"xrecvmsg",	""	},
/* 243 */   { 0,			Ukn,	"xsendmsg",	""	},
/* 244 */   { 0,			Ukn,	"xsetsockaddr",	""	},
/* 245 */   { 0,			Ukn,	"xsetsockopt",	""	},
/* 246 */   { 0,			Ukn,	"xshutdown",	""	},
/* 247 */   { 0,			Ukn,	"xsocket",	""	},
/* 248 */   { 0,			Ukn,	"xsocketpair",	""	},

/* 249 */   { 0,			Ukn,	"unused 249",	""	},
/* 250 */   { 0,			Ukn,	"unused 250",	""	},
};


static void
uw7_lcall7(int segment, struct pt_regs *regs)
{
	int sysno = regs->eax & 0xff;

	if (sysno >= ARRAY_SIZE(uw7_syscall_table))
		set_error(regs, iABI_errors(-EINVAL));
	else
		lcall7_dispatch(regs, &uw7_syscall_table[sysno], 1);
}

static struct exec_domain uw7_exec_domain = {
	name:		"UnixWare 7",
	handler:	uw7_lcall7,
	pers_low:	14 /* PER_UW7 */,
	pers_high:	14 /* PER_UW7 */,
	signal_map:	uw7_to_linux_signals,
	signal_invmap:	linux_to_uw7_signals,
	err_map:	uw7_err_map,
	socktype_map:	uw7_socktype_map,
	sockopt_map:	uw7_sockopt_map,
	af_map:		uw7_af_map,
	module:		THIS_MODULE
};

static int __init
init_uw7(void)
{
	return register_exec_domain(&uw7_exec_domain);
}

static void __exit
cleanup_uw7(void)
{
	unregister_exec_domain(&uw7_exec_domain);
}

module_init(init_uw7);
module_exit(cleanup_uw7);
