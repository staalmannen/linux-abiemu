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
 */

#ident "%W% %G%"

/*
 * SVR4 personality switch.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/personality.h>
#include <linux/syscalls.h>
#include <linux/socket.h>
#include <linux/highuid.h>
#include <linux/net.h>
#include <asm/uaccess.h>

#include <abi/svr4/sysent.h>
#include <abi/signal.h>

#include <abi/util/errno.h>
#include <abi/util/sysent.h>
#include <abi/util/socket.h>

MODULE_DESCRIPTION("iBCS2/iABI4 personality");
MODULE_AUTHOR("Christoph Hellwig, partially taken from iBCS");
MODULE_LICENSE("GPL");


/*
 * We could remove some of the long identity mapped runs but at the
 * expense of extra comparisons for each mapping at run time...
 */
static u_char ibcs_err_table[] = {
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

struct map_segment ibcs_err_map[] = {
	{ 0,	0+sizeof(ibcs_err_table)-1,	ibcs_err_table },
	{ 512,  512+sizeof(lnx_err_table)-1,	lnx_err_table },
	{ -1 }
};

static long linux_to_ibcs_signals[NSIGNALS+1] = {
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

static long ibcs_to_linux_signals[NSIGNALS+1] = {
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

static char ibcs_socktype[] = {
	SOCK_STREAM,
	SOCK_DGRAM,
	0,
	SOCK_RAW,
	SOCK_RDM,
	SOCK_SEQPACKET
};

static struct map_segment ibcs_socktype_map[] = {
	{ 1, 6, ibcs_socktype },
	{ -1 }
};

static struct map_segment ibcs_sockopt_map[] =  {
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

static struct map_segment ibcs_af_map[] =  {
	{ 0, 2, NULL },
	{ -1 }
};


static struct sysent ibcs_syscall_table[] = {
   { abi_syscall,	Fast,	"syscall",	""	}, /*    0 */
   { sys_exit,		1,	"exit",		"d"	}, /*    1 */
   { abi_fork,		Spl,	"fork",		""	}, /*    2 */
   { abi_read,		3,	"read",		"dpd"	}, /*    3 */
   { sys_write,		3,	"write",	"dpd"	}, /*    4 */
   { svr4_open,		3,	"open",		"soo"	}, /*    5 */
   { sys_close,		1,	"close",	"d"	}, /*    6 */
   { abi_wait,		Spl,	"wait",		"xxx"	}, /*    7 */
   { sys_creat,		2,	"creat",	"so"	}, /*    8 */
   { sys_link,		2,	"link",		"ss"	}, /*    9 */
   { sys_unlink,	1,	"unlink",	"s"	}, /*   10 */
   { abi_exec,		Spl,	"exec",		"sxx"	}, /*   11 */
   { sys_chdir,		1,	"chdir",	"s"	}, /*   12 */
   { abi_time,		0,	"time",		""	}, /*   13 */
   { svr4_mknod,	3,	"mknod",	"soo"	}, /*   14 */
   { sys_chmod,		2,	"chmod",	"so"	}, /*   15 */
   { sys_chown,		3,	"chown",	"sdd"	}, /*   16 */
   { abi_brk,		1,	"brk/break",	"x"	}, /*   17 */
   { svr4_stat,		2,	"stat",		"sp"	}, /*   18 */
   { sys_lseek,		3,	"seek/lseek",	"ddd"	}, /*   19 */
   { abi_getpid,	Spl,	"getpid",	""	}, /*   20 */
   { 0,			Ukn,	"mount",	""	}, /*   21 */
   { sys_umount,	1,	"umount",	"s"	}, /*   22 */
   { sys_setuid,	1,	"setuid",	"d"	}, /*   23 */
   { abi_getuid,	Spl,	"getuid",	""	}, /*   24 */
   { sys_stime,		1,	"stime",	"d"	}, /*   25 */
   { 0,			Ukn,	"ptrace",	""	}, /*   26 */
   { sys_alarm,		1,	"alarm",	"d"	}, /*   27 */
   { svr4_fstat,	2,	"fstat",	"dp"	}, /*   28 */
   { sys_pause,		0,	"pause",	""	}, /*   29 */
   { sys_utime,		2,	"utime",	"xx"	}, /*   30 */
   { 0,			Ukn,	"stty",		""	}, /*   31 */
   { 0,			Ukn,	"gtty",		""	}, /*   32 */
   { sys_access,	2,	"access",	"so"	}, /*   33 */
   { sys_nice,		1,	"nice",		"d"	}, /*   34 */
   { svr4_statfs,	4,	"statfs",	"spdd"	}, /*   35 */
   { sys_sync,		0,	"sync",		""	}, /*   36 */
   { abi_kill,		2,	"kill",		"dd"	}, /*   37 */
   { svr4_fstatfs,	4,	"fstatfs",	"dpdd"	}, /*   38 */
   { abi_procids,	Spl,	"procids",	"d"	}, /*   39 */
   { 0,			Ukn,	"cxenix",	""	}, /*   40 */
   { sys_dup,		1,	"dup",		"d"	}, /*   41 */
   { abi_pipe,		Spl,	"pipe",		""	}, /*   42 */
   { sys_times,		1,	"times",	"p"	}, /*   43 */
   { 0,			Ukn,	"prof",		""	}, /*   44 */
   { 0,			Ukn,	"lock/plock",	""	}, /*   45 */
   { sys_setgid,	1,	"setgid",	"d"	}, /*   46 */
   { abi_getgid,	Spl,	"getgid",	""	}, /*   47 */
   { abi_sigfunc,	Fast,	"sigfunc",	"xxx"	}, /*   48 */
   { svr4_msgsys,	Spl,	"msgsys",	"dxddd"	}, /*   49 */
   { svr4_sysi86,	3,	"sysi86",	"d"	}, /*   50 */
   { sys_acct,		1,	"acct/sysacct",	"x"	}, /*   51 */
   { svr4_shmsys,	Fast,	"shmsys",	"ddxo"	}, /*   52 */
   { svr4_semsys,	Spl,	"semsys",	"dddx"	}, /*   53 */
   { svr4_ioctl,	Spl,	"ioctl",	"dxx"	}, /*   54 */
   { 0,			3,	"uadmin",	"xxx"	}, /*   55 */
   { 0,			Ukn,	"?",		""	}, /*   56 */
   { v7_utsname,	1,	"utsys",	"x"	}, /*   57 */
   { sys_fsync,		1,	"fsync",	"d"	}, /*   58 */
   { abi_exec,		Spl,	"execv",	"spp"	}, /*   59 */
   { sys_umask,		1,	"umask",	"o"	}, /*   60 */
   { sys_chroot,	1,	"chroot",	"s"	}, /*   61 */
   { svr4_fcntl,	3,	"fcntl",	"dxx"	}, /*   62 */
   { svr4_ulimit,	2,	"ulimit",	"xx"	}, /*   63 */
   { 0,			Ukn,	"?",		""	}, /*   64 */
   { 0,			Ukn,	"?",		""	}, /*   65 */
   { 0,			Ukn,	"?",		""	}, /*   66 */
   { 0,			Ukn,	"?",		""	}, /*   67 */
   { 0,			Ukn,	"?",		""	}, /*   68 */
   { 0,			Ukn,	"?",		""	}, /*   69 */
   { 0,			Ukn,	"advfs",	""	}, /*   70 */
   { 0,			Ukn,	"unadvfs",	""	}, /*   71 */
   { 0,			Ukn,	"rmount",	""	}, /*   72 */
   { 0,			Ukn,	"rumount",	""	}, /*   73 */
   { 0,			Ukn,	"rfstart",	""	}, /*   74 */
   { 0,			Ukn,	"?",		""	}, /*   75 */
   { 0,			Ukn,	"rdebug",	""	}, /*   76 */
   { 0,			Ukn,	"rfstop",	""	}, /*   77 */
   { 0,			Ukn,	"rfsys",	""	}, /*   78 */
   { sys_rmdir,		1,	"rmdir",	"s"	}, /*   79 */
   { abi_mkdir,		2,	"mkdir",	"so"	}, /*   80 */
   { svr4_getdents,	3,	"getdents",	"dxd"	}, /*   81 */
   { 0,			Ukn,	"libattach",	""	}, /*   82 */
   { 0,			Ukn,	"libdetach",	""	}, /*   83 */
   { svr4_sysfs,	3,	"sysfs",	"dxx"	}, /*   84 */
   { svr4_getmsg,	Spl,	"getmsg",	"dxxx"	}, /*   85 */
   { svr4_putmsg,	Spl,	"putmsg",	"dxxd"	}, /*   86 */
   { sys_poll,		3,	"poll",		"xdd"	}, /*   87 */
   { svr4_lstat,	2,	"lstat",	"sp"	}, /*   88 */
   { sys_symlink,	2,	"symlink",	"ss"	}, /*   89 */
   { sys_readlink,	3,	"readlink",	"spd"	}, /*   90 */
   { sys_setgroups,	2,	"setgroups",	"dp"	}, /*   91 */
   { sys_getgroups,	2,	"getgroups",	"dp"	}, /*   92 */
   { sys_fchmod,	2,	"fchmod",	"do"	}, /*   93 */
   { sys_fchown,	3,	"fchown",	"ddd"	}, /*   94 */
   { abi_sigprocmask,	3,	"sigprocmask",	"dxx"	}, /*   95 */
   { abi_sigsuspend,	Spl,	"sigsuspend",	"x"	}, /*   96 */
   { 0,			2,	"sigaltstack",	"xx"	}, /*   97 */
   { abi_sigaction,	3,	"sigaction",	"dxx"	}, /*   98 */
   { svr4_sigpending,	2,	"sigpending",	"dp"	}, /*   99 */
   { svr4_context,	Spl,	"context",	""	}, /*   100 */
   { 0,			Ukn,	"evsys",	""	}, /*   101 */
   { 0,			Ukn,	"evtrapret",	""	}, /*   102 */
   { svr4_statvfs,	2,	"statvfs",	"sp"	}, /*   103 */
   { svr4_fstatvfs,	2,	"fstatvfs",	"dp"	}, /*   104 */
   { 0,			Ukn,	"sysisc",	""	}, /*   105 */
   { 0,			Ukn,	"nfssys",	""	}, /*   106 */
   { 0,			4,	"waitid",	"ddxd"	}, /*   107 */
   { 0,			3,	"sigsendsys",	"ddd"	}, /*   108 */
   { svr4_hrtsys,	Spl,	"hrtsys",	"xxx"	}, /*   109 */
   { 0,			3,	"acancel",	"dxd"	}, /*   110 */
   { 0,			Ukn,	"async",	""	}, /*   111 */
   { 0,			Ukn,	"priocntlsys",	""	}, /*   112 */
   { svr4_pathconf,	2,	"pathconf",	"sd"	}, /*   113 */
   { 0,			3,	"mincore",	"xdx"	}, /*   114 */
   { svr4_mmap,		6,	"mmap",		"xxxxdx"},/*   115 */
   { sys_mprotect,	3,	"mprotect",	"xdx"	},/*   116 */
   { sys_munmap,	2,	"munmap",	"xd"	},/*   117 */
   { svr4_fpathconf,	2,	"fpathconf",	"dd"	}, /*   118 */
   { abi_fork,		Spl,	"vfork",	""	}, /*   119 */
   { sys_fchdir,	1,	"fchdir",	"d"	}, /*   120 */
   { sys_readv,		3,	"readv",	"dxd"	}, /*   121 */
   { sys_writev,	3,	"writev",	"dxd"	}, /*   122 */
   { svr4_xstat,	3,	"xstat",	"dsx"	}, /*   123 */
   { svr4_lxstat,     	3,	"lxstat",	"dsx"	}, /*   124 */
   { svr4_fxstat,	3,	"fxstat",	"ddx"	}, /*   125 */
   { svr4_xmknod,	4,	"xmknod",	"dsox"	}, /*   126 */
   { 0,			Ukn,	"syslocal",	"d"	}, /*   127 */
   { svr4_getrlimit,	2,	"setrlimit",	"dx"	}, /*   128 */
   { svr4_setrlimit,	2,	"getrlimit",	"dx"	}, /*   129 */
   { 0,			3,	"lchown",	"sdd"	}, /*   130 */
   { 0,			Ukn,	"memcntl",	""	}, /*   131 */
#if defined(CONFIG_ABI_XTI)
   { svr4_getpmsg,	5,	"getpmsg",	"dxxxx"	}, /*   132 */
   { svr4_putpmsg,	5,	"putpmsg",	"dxxdd"	}, /*   133 */
#else
   { 0,			5,	"getpmsg",	"dxxxx"	}, /*   132 */
   { 0,			5,	"putpmsg",	"dxxdd"	}, /*   133 */
#endif
   { sys_rename,	2,	"rename",	"ss"	}, /*   134 */
   { abi_utsname,	1,	"uname",	"x"	}, /*   135 */
   { svr4_setegid,	1,	"setegid",	"d"	}, /*   136 */
   { svr4_sysconfig,	1,	"sysconfig",	"d"	}, /*   137 */
   { 0,			Ukn,	"adjtime",	""	}, /*   138 */
   { svr4_sysinfo,	3,	"systeminfo",	"dsd"	}, /*   139 */
   { socksys_syscall,	1,	"socksys",	"x"	}, /*   140 */
   { svr4_seteuid,	1,	"seteuid",	"d"	}, /*   141 */
   { 0,			Ukn,	"?",		""	}, /*   142 */
   { 0,			Ukn,	"?",		""	}, /*   143 */
   { 0,			2,	"secsys",	"dx"	}, /*	144 */
   { 0,			4,	"filepriv",	"sdxd"	}, /*	145 */
   { 0,			3,	"procpriv",	"dxd"	}, /*	146 */
   { 0,			3,	"devstat",	"sdx"	}, /*	147 */
   { 0,			5,	"aclipc",	"ddddx"	}, /*	148 */
   { 0,			3,	"fdevstat",	"ddx"	}, /*	149 */
   { 0,			3,	"flvlfile",	"ddx"	}, /*	150 */
   { 0,			3,	"lvlfile",	"sdx"	}, /*	151 */
   { 0,			Ukn,	"?",		""	}, /*	152 */
   { 0,			2,	"lvlequal",	"xx"	}, /*	153 */
   { 0,			2,	"lvlproc",	"dx"	}, /*	154 */
   { 0,			Ukn,	"?",		""	}, /*	155 */
   { 0,			4,	"lvlipc",	"dddx"	}, /*	156 */
   { 0,			4,	"acl",		"sddx"	}, /*	157 */
   { 0,			Ukn,	"auditevt",	""	}, /*	158 */
   { 0,			Ukn,	"auditctl",	""	}, /*	159 */
   { 0,			Ukn,	"auditdmp",	""	}, /*	160 */
   { 0,			Ukn,	"auditlog",	""	}, /*	161 */
   { 0,			Ukn,	"auditbuf",	""	}, /*	162 */
   { 0,			2,	"lvldom",	"xx"	}, /*	163 */
   { 0,			Ukn,	"lvlvfs",	""	}, /*	164 */
   { 0,			2,	"mkmld",	"so"	}, /*	165 */
   { 0,			Ukn,	"mlddone",	""	}, /*	166 */
   { 0,			2,	"secadvise",	"xx"	}, /*	167 */
   { 0,			Ukn,	"online",	""	}, /*	168 */
   { sys_setitimer,	3,	"setitimer",	"dxx"	}, /*	169 */
   { sys_getitimer,	2,	"getitimer",	"dx"	}, /*	170 */
   { sys_gettimeofday,	2,	"gettimeofday",	"xx"	}, /*	171 */
   { sys_settimeofday,	2,	"settimeofday",	"xx"	}, /*	172 */
   { 0,			Ukn,	"lwpcreate",	""	}, /*	173 */
   { 0,			Ukn,	"lwpexit",	""	}, /*	174 */
   { 0,			Ukn,	"lwpwait",	""	}, /*	175 */
   { 0,			Ukn,	"lwpself",	""	}, /*	176 */
   { 0,			Ukn, 	"lwpinfo",	""	}, /*	177 */
   { 0,			Ukn,	"lwpprivate",	""	}, /*	178 */
   { 0,			Ukn,	"processorbind",""	}, /*	179 */
   { 0,			Ukn,	"processorexbind",""	}, /*	180 */
   { 0,			Ukn,	"",		""	}, /*	181 */
   { 0,			Ukn,	"sync_mailbox",	""	}, /*	182 */
   { 0,			Ukn,	"prepblock",	""	}, /*	183 */
   { 0,			Ukn,	"block",	""	}, /*	184 */
   { 0,			Ukn,	"rdblock",	""	}, /*	185 */
   { 0,			Ukn,	"unblock",	""	}, /*	186 */
   { 0,			Ukn,	"cancelblock",	""	}, /*	187 */
   { 0,			Ukn,	"?",		""	}, /*	188 */
   { 0,			Ukn,	"pread",	""	}, /*	189 */
   { 0,			Ukn,	"pwrite",	""	}, /*	190 */
   { sys_truncate,	2,	"truncate",	"sd"	}, /*	191 */
   { sys_ftruncate,	2,	"ftruncate",	"dd"	}, /*	192 */
   { 0,			Ukn,	"lwpkill",	""	}, /*	193 */
   { 0,			Ukn,	"sigwait",	""	}, /*	194 */
   { 0,			Ukn,	"fork1",	""	}, /*	195 */
   { 0,			Ukn,	"forkall",	""	}, /*	196 */
   { 0,			Ukn,	"modload",	""	}, /*	197 */
   { 0,			Ukn,	"moduload",	""	}, /*	198 */
   { 0,			Ukn,	"modpath",	""	}, /*	199 */
   { 0,			Ukn,	"modstat",	""	}, /*	200 */
   { 0,			Ukn,	"modadm",	""	}, /*	201 */
   { 0,			Ukn,	"getksym",	""	}, /*	202 */
   { 0,			Ukn,	"lwpsuspend",	""	}, /*	203 */
   { 0,			Ukn,	"lwpcontinue",	""	}, /*	204 */
   { 0,			Ukn,	"?",		""	}, /*	205 */
   { 0,			Ukn,	"?",		""	}, /*	206 */
   { 0,			Ukn,	"?",		""	}, /*	207 */
   { 0,			Ukn,	"?",		""	},
   { 0,			Ukn,	"?",		""	},
   { 0,			Ukn,	"?",		""	},
   { 0,			Ukn,	"?",		""	},
   { 0,			Ukn,	"?",		""	},
   { 0,			Ukn,	"?",		""	},
   { 0,			Ukn,	"?",		""	},
   { 0,			Ukn,	"?",		""	}
};

static void
ibcs_lcall7(int segment, struct pt_regs *regs)
{
	int sysno = regs->eax & 0xff;

	if (sysno >= ARRAY_SIZE(ibcs_syscall_table))
		set_error(regs, iABI_errors(-EINVAL));
	else
		lcall7_dispatch(regs, &ibcs_syscall_table[regs->eax & 0xff], 1);
}

static struct exec_domain ibcs_exec_domain = {
	name:		"iBCS2/iABI4",
	handler:	ibcs_lcall7,
	pers_low:	1 /* PER_SVR4 */,
	pers_high:	2 /* PER_SVR3 */,
	signal_map:	ibcs_to_linux_signals,
	signal_invmap:	linux_to_ibcs_signals,
	err_map:	ibcs_err_map,
	socktype_map:	ibcs_socktype_map,
	sockopt_map:	ibcs_sockopt_map,
	af_map:		ibcs_af_map,
	module:		THIS_MODULE
};

static int __init
ibcs_init(void)
{
	return register_exec_domain(&ibcs_exec_domain);
}


static void __exit
ibcs_exit(void)
{
	unregister_exec_domain(&ibcs_exec_domain);
}

module_init(ibcs_init);
module_exit(ibcs_exit);
