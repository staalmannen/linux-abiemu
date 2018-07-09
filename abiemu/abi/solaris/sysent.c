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
 * Solaris sysent and error tables.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/personality.h>
#include <linux/syscalls.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <asm/uaccess.h>

#include <abi/signal.h>

#include <abi/svr4/sysent.h>
#include <abi/solaris/sysent.h>
#include <abi/util/socket.h>

#include <abi/util/errno.h>
#include <abi/util/sysent.h>

/* kernel module specifications */
#ifndef EXPORT_NO_SYMBOLS
#define EXPORT_NO_SYMBOLS
#endif

MODULE_DESCRIPTION("Solaris personality");
MODULE_AUTHOR("Christoph Hellwig, partially taken from iBCS");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;


/* XXX kill this one */
#define ITR(trace, name, args)  ,name,args


/*
 * We could remove some of the long identity mapped runs but at the
 * expense of extra comparisons for each mapping at run time...
 */
static u_char solaris_err_table[] = {
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

struct map_segment solaris_err_map[] = {
	{ 0,	0+sizeof(solaris_err_table)-1,	solaris_err_table },
	{ 512,  512+sizeof(lnx_err_table)-1,	lnx_err_table },
	{ -1 }
};

static long linux_to_solaris_signals[NSIGNALS+1] = {
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

static long solaris_to_linux_signals[NSIGNALS+1] = {
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

static char solaris_socktype[] = {
	SOCK_STREAM,
	SOCK_DGRAM,
	0,
	SOCK_RAW,
	SOCK_RDM,
	SOCK_SEQPACKET
};

static struct map_segment solaris_socktype_map[] = {
	{ 1, 6, solaris_socktype },
	{ -1 }
};

static struct map_segment solaris_sockopt_map[] =  {
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

static struct map_segment solaris_af_map[] =  {
	{ 0, 2, NULL },
	{ -1 }
};


static struct sysent solaris_syscall_table[] = {
   { abi_syscall,	Fast	ITR(0, "syscall",	"")	}, /*    0 */
   { sys_exit,		1	ITR(0, "exit",		"d")	}, /*    1 */
   { abi_fork,		Spl	ITR(0, "fork",		"")	}, /*    2 */
   { abi_read,		3	ITR(0, "read",		"dpd")	}, /*    3 */
   { sys_write,		3	ITR(0, "write",		"dpd")	}, /*    4 */
   { svr4_open,		3	ITR(0, "open",		"soo")	}, /*    5 */
   { sys_close,		1	ITR(0, "close",		"d")	}, /*    6 */
   { abi_wait,		Spl	ITR(0, "wait",		"xxx")	}, /*    7 */
   { sys_creat,		2	ITR(0, "creat",		"so")	}, /*    8 */
   { sys_link,		2	ITR(0, "link",		"ss")	}, /*    9 */
   { sys_unlink,	1	ITR(0, "unlink",	"s")	}, /*   10 */
   { abi_exec,		Spl	ITR(0, "exec",		"sxx")	}, /*   11 */
   { sys_chdir,		1	ITR(0, "chdir",		"s")	}, /*   12 */
   { abi_time,		0	ITR(0, "time",		"")	}, /*   13 */
   { svr4_mknod,	3	ITR(0, "mknod",		"soo")	}, /*   14 */
   { sys_chmod,		2	ITR(0, "chmod",		"so")	}, /*   15 */
   { sys_chown,		3	ITR(0, "chown",		"sdd")	}, /*   16 */
   { abi_brk,		1	ITR(0, "brk/break",	"x")	}, /*   17 */
   { svr4_stat,		2	ITR(0, "stat",		"sp")	}, /*   18 */
   { sys_lseek,		3	ITR(0, "seek/lseek",	"ddd")	}, /*   19 */
   { abi_getpid,	Spl	ITR(0, "getpid",	"")	}, /*   20 */
   { 0,			Ukn	ITR(1, "mount",		"")	}, /*   21 */
   { sys_umount,	1	ITR(0, "umount",	"s")	}, /*   22 */
   { sys_setuid,	1	ITR(0, "setuid",	"d")	}, /*   23 */
   { abi_getuid,	Spl	ITR(0, "getuid",	"")	}, /*   24 */
   { sys_stime,		1	ITR(0, "stime",		"d")	}, /*   25 */
   { 0,			Ukn	ITR(0, "ptrace",	"")	}, /*   26 */
   { sys_alarm,		1	ITR(0, "alarm",		"d")	}, /*   27 */
   { svr4_fstat,	2	ITR(0, "fstat",		"dp")	}, /*   28 */
   { sys_pause,		0	ITR(0, "pause",		"")	}, /*   29 */
   { sys_utime,		2	ITR(0, "utime",		"xx")	}, /*   30 */
   { 0,			Ukn	ITR(0, "stty",		"")	}, /*   31 */
   { 0,			Ukn	ITR(1, "gtty",		"")	}, /*   32 */
   { sys_access,	2	ITR(0, "access",	"so")	}, /*   33 */
   { sys_nice,		1	ITR(0, "nice",		"d")	}, /*   34 */
   { svr4_statfs,	4	ITR(0, "statfs",	"spdd")	}, /*   35 */
   { sys_sync,		0	ITR(0, "sync",		"")	}, /*   36 */
   { abi_kill,		2	ITR(0, "kill",		"dd")	}, /*   37 */
   { svr4_fstatfs,	4	ITR(0, "fstatfs",	"dpdd")	}, /*   38 */
   { abi_procids,	Spl	ITR(0, "procids",	"d")	}, /*   39 */
   { 0,			Ukn	ITR(0, "cxenix",	"")	}, /*   40 */
   { sys_dup,		1	ITR(0, "dup",		"d")	}, /*   41 */
   { abi_pipe,		Spl	ITR(0, "pipe",		"")	}, /*   42 */
   { sys_times,		1	ITR(0, "times",		"p")	}, /*   43 */
   { 0,			0	ITR(0, "prof",		"")	}, /*   44 */
   { 0,			Ukn	ITR(1, "lock/plock",	"")	}, /*   45 */
   { sys_setgid,	1	ITR(0, "setgid",	"d")	}, /*   46 */
   { abi_getgid,	Spl	ITR(0, "getgid",	"")	}, /*   47 */
   { abi_sigfunc,	Fast	ITR(0, "sigfunc",	"xxx")	}, /*   48 */
   { svr4_msgsys,	Spl	ITR(0, "msgsys",	"dxddd")}, /*   49 */
   { svr4_sysi86,	3	ITR(0, "sysi86/sys3b",	"d")	}, /*   50 */
   { sys_acct,		1	ITR(0, "acct/sysacct",	"x")	}, /*   51 */
   { svr4_shmsys,	Fast	ITR(0, "shmsys",	"ddxo")}, /*   52 */
   { svr4_semsys,	Spl	ITR(0, "semsys",	"dddx")}, /*   53 */
   { svr4_ioctl,	Spl	ITR(0, "ioctl",		"dxx")	}, /*   54 */
   { 0,			3	ITR(0, "uadmin",	"xxx")	}, /*   55 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*   56 */
   { v7_utsname,	1	ITR(0, "utsys",		"x")	}, /*   57 */
   { sys_fsync,		1	ITR(0, "fsync",		"d")	}, /*   58 */
   { abi_exec,		Spl	ITR(0, "execv",		"spp")	}, /*   59 */
   { sys_umask,		1	ITR(0, "umask",		"o")	}, /*   60 */
   { sys_chroot,	1	ITR(0, "chroot",	"s")	}, /*   61 */
   { svr4_fcntl,	3	ITR(0, "fcntl",		"dxx")	}, /*   62 */
   { svr4_ulimit,	2	ITR(0, "ulimit",	"xx")	}, /*   63 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*   64 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*   65 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*   66 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*   67 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*   68 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*   69 */
   { 0,			Ukn	ITR(1, "advfs",		"")	}, /*   70 */
   { 0,			Ukn	ITR(1, "unadvfs",	"")	}, /*   71 */
   { 0,			Ukn	ITR(1, "rmount",	"")	}, /*   72 */
   { 0,			Ukn	ITR(1, "rumount",	"")	}, /*   73 */
   { 0,			Ukn	ITR(1, "rfstart",	"")	}, /*   74 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*   75 */
   { 0,			Ukn	ITR(1, "rdebug",	"")	}, /*   76 */
   { 0,			Ukn	ITR(1, "rfstop",	"")	}, /*   77 */
   { 0,			Ukn	ITR(1, "rfsys",		"")	}, /*   78 */
   { sys_rmdir,		1	ITR(0, "rmdir",		"s")	}, /*   79 */
   { abi_mkdir,		2	ITR(0, "mkdir",		"so")	}, /*   80 */
   { svr4_getdents,	3	ITR(0, "getdents",	"dxd")	}, /*   81 */
   { 0,			Ukn	ITR(1, "libattach",	"")	}, /*   82 */
   { 0,			Ukn	ITR(1, "libdetach",	"")	}, /*   83 */
   { svr4_sysfs,	3	ITR(0, "sysfs",		"dxx")	}, /*   84 */
   { svr4_getmsg,	Spl	ITR(0, "getmsg",	"dxxx")	}, /*   85 */
   { svr4_putmsg,	Spl	ITR(0, "putmsg",	"dxxd")	}, /*   86 */
   { sys_poll,		3	ITR(0, "poll",		"xdd")	}, /*   87 */
   { svr4_lstat,	2	ITR(0, "lstat",		"sp")	}, /*   88 */
   { sys_symlink,	2	ITR(0, "symlink",	"ss")	}, /*   89 */
   { sys_readlink,	3	ITR(0, "readlink",	"spd")	}, /*   90 */
   { sys_setgroups,	2	ITR(0, "setgroups",	"dp")	}, /*   91 */
   { sys_getgroups,	2	ITR(0, "getgroups",	"dp")	}, /*   92 */
   { sys_fchmod,	2	ITR(0, "fchmod",	"do")	}, /*   93 */
   { sys_fchown,	3	ITR(0, "fchown",	"ddd")	}, /*   94 */
   { abi_sigprocmask,	3	ITR(0, "sigprocmask",	"dxx")	}, /*   95 */
   { abi_sigsuspend,	Spl	ITR(0, "sigsuspend",	"x")	}, /*   96 */
   { 0,			2	ITR(1, "sigaltstack",	"xx")	}, /*   97 */
   { abi_sigaction,	3	ITR(0, "sigaction",	"dxx")	}, /*   98 */
   { svr4_sigpending,	2	ITR(1, "sigpending",	"dp")	}, /*   99 */
   { svr4_context,	Spl	ITR(0, "context",	"")	}, /*   100 */
   { 0,			Ukn	ITR(1, "evsys",		"")	}, /*   101 */
   { 0,			Ukn	ITR(1, "evtrapret",	"")	}, /*   102 */
   { svr4_statvfs,	2	ITR(0, "statvfs",	"sp")	}, /*   103 */
   { svr4_fstatvfs,	2	ITR(0, "fstatvfs",	"dp")	}, /*   104 */
   { 0,			Ukn	ITR(0, "sysisc",	"")	}, /*   105 */
   { 0,			Ukn	ITR(1, "nfssys",	"")	}, /*   106 */
   { 0,			4	ITR(0, "waitid",	"ddxd")	}, /*   107 */
   { 0,			3	ITR(1, "sigsendsys",	"ddd")	}, /*   108 */
   { svr4_hrtsys,	Spl	ITR(0, "hrtsys",	"xxx")	}, /*   109 */
   { 0,			3	ITR(1, "acancel",	"dxd")	}, /*   110 */
   { 0,			Ukn	ITR(1, "async",		"")	}, /*   111 */
   { 0,			Ukn	ITR(1, "priocntlsys",	"")	}, /*   112 */
   { svr4_pathconf,	2	ITR(1, "pathconf",	"sd")	}, /*   113 */
   { 0,			3	ITR(1, "mincore",	"xdx")	}, /*   114 */
   { svr4_mmap,		6	ITR(0, "mmap",		"xxxxdx") },/*   115 */
   { sys_mprotect,	3	ITR(0, "mprotect",	"xdx")  },/*   116 */
   { sys_munmap,	2	ITR(0, "munmap",	"xd")   },/*   117 */
   { svr4_fpathconf,	2	ITR(1, "fpathconf",	"dd")	}, /*   118 */
   { abi_fork,		Spl	ITR(0, "vfork",		"")	}, /*   119 */
   { sys_fchdir,	1	ITR(0, "fchdir",	"d")	}, /*   120 */
   { sys_readv,		3	ITR(0, "readv",		"dxd")	}, /*   121 */
   { sys_writev,	3	ITR(0, "writev",	"dxd")	}, /*   122 */
   { svr4_xstat,	3	ITR(0, "xstat",		"dsx")	}, /*   123 */
   { svr4_lxstat,     	3	ITR(0, "lxstat",	"dsx")	}, /*   124 */
   { svr4_fxstat,	3	ITR(0, "fxstat",	"ddx")	}, /*   125 */
   { svr4_xmknod,	4	ITR(0, "xmknod",	"dsox")}, /*   126 */
   { 0,			Spl	ITR(0, "syslocal",	"d")	}, /*   127 */
   { svr4_getrlimit,	2	ITR(0, "setrlimit",	"dx")	}, /*   128 */
   { svr4_setrlimit,	2	ITR(0, "getrlimit",	"dx")	}, /*   129 */
   { 0,			3	ITR(1, "lchown",	"sdd")	}, /*   130 */
   { 0,			Ukn	ITR(1, "memcntl",	"")	}, /*   131 */
#ifdef CONFIG_ABI_XTI
   { svr4_getpmsg,	5	ITR(0, "getpmsg",	"dxxxx")}, /*   132 */
   { svr4_putpmsg,	5	ITR(0, "putpmsg",	"dxxdd")}, /*   133 */
#else
   { 0,			5	ITR(0, "getpmsg",	"dxxxx")}, /*   132 */
   { 0,			5	ITR(0, "putpmsg",	"dxxdd")}, /*   133 */
#endif
   { sys_rename,	2	ITR(0, "rename",	"ss")	}, /*   134 */
   { abi_utsname,	1	ITR(0, "uname",		"x")	}, /*   135 */
   { svr4_setegid,	1	ITR(1, "setegid",	"d")	}, /*   136 */
   { svr4_sysconfig,	1	ITR(0, "sysconfig",	"d")	}, /*   137 */
   { 0,			Ukn	ITR(1, "adjtime",	"")	}, /*   138 */
   { svr4_sysinfo,	3	ITR(0, "systeminfo",	"dsd")	}, /*   139 */
   { socksys_syscall,	1	ITR(0, "socksys_syscall","x")	}, /*   140 */
   { svr4_seteuid,	1	ITR(1, "seteuid",	"d")	}, /*   141 */
   { 0,			Ukn	ITR(1, "vtrace",       	"")	}, /*   142 */
   { 0,			Ukn	ITR(1, "fork1",		"")	}, /*   143 */
   { 0,			Ukn	ITR(1, "sigtimedwait",	"")	}, /*	144 */
   { 0,			Ukn	ITR(1, "lwp_info",	"")	}, /*	145 */
   { 0,			Ukn	ITR(1, "yield",		"")	}, /*	146 */
   { 0,			Ukn	ITR(1, "lwp_sema_wait",	"")	}, /*	147 */
   { 0,			Ukn	ITR(1, "lwp_sema_post",	"")	}, /*	148 */
   { 0,			Ukn	ITR(1, "lwp_sema_trywait","")	}, /*	149 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*	150 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*	151 */
   { 0,			Ukn	ITR(1, "modctl",       	"")	}, /*	152 */
   { 0,			Ukn	ITR(1, "fchroot",	"")	}, /*	153 */
   { 0,			Ukn	ITR(1, "utimes",	"")	}, /*	154 */
   { 0,			Ukn	ITR(1, "vhangup",      	"")	}, /*	155 */
   { sys_gettimeofday,	2	ITR(0, "gettimeofday",	"xx")	}, /*	156 */
   { sys_getitimer,	2	ITR(0, "getitimer",    	"dx")	}, /*	157 */
   { sys_setitimer,	3	ITR(0, "setitimer",	"dxx")	}, /*	158 */
   { 0,			Ukn	ITR(1, "lwp_create",	"")	}, /*	159 */
   { 0,			Ukn	ITR(1, "lwp_exit",	"")	}, /*	160 */
   { 0,			Ukn	ITR(1, "lwp_suspend",	"")	}, /*	161 */
   { 0,			Ukn	ITR(1, "lwp_continue",	"")	}, /*	162 */
   { 0,			Ukn	ITR(1, "lwp_kill",	"")	}, /*	163 */
   { 0,			Ukn	ITR(1, "lwp_self",	"")	}, /*	164 */
   { 0,			Ukn	ITR(1, "lwp_setprivate","")	}, /*	165 */
   { 0,			Ukn	ITR(1, "lwp_getprivate","")	}, /*	166 */
   { 0,			Ukn	ITR(1, "lwp_wait",	"")	}, /*	167 */
   { 0,			Ukn	ITR(1, "lwp_mutex_unlock","")	}, /*	168 */
   { 0,			Ukn	ITR(1, "lwp_mutex_lock","")	}, /*	169 */
   { 0,			Ukn	ITR(1, "lwp_cond_wait",	"")	}, /*	170 */
   { 0,			Ukn	ITR(1, "lwp_cond_signal","")	}, /*	171 */
   { 0,			Ukn	ITR(1, "lwp_cond_broadcast","")	}, /*	172 */
   { sys_pread64,	-4	ITR(1, "pread",		"dpdd")	}, /*	173 */
   { sys_pwrite64,	-4	ITR(1, "pwrite",	"dpdd")	}, /*	174 */
   { sol_llseek,      	Spl	ITR(1, "llseek",	"dxxd")	}, /*	175 */
   { 0,			Ukn	ITR(1, "inst_sync",	"")	}, /*	176 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*	177 */
   { 0,			Ukn	ITR(1, "kaio",		"")	}, /*	178 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*	179 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*	180 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*	181 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*	182 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /*	183 */
   { 0,			Ukn	ITR(1, "tsolsys",      	"")	}, /*	184 */
   { sol_acl,		4	ITR(1, "acl",		"sddp")	}, /*	185 */
   { 0,			Ukn	ITR(1, "auditsys",	"")	}, /*	186 */
   { 0,			Ukn	ITR(1, "processor_bind","")	}, /*	187 */
   { 0,			Ukn	ITR(1, "processor_info","")	}, /*	188 */
   { 0,			Ukn	ITR(1, "p_online",     	"")	}, /*	189 */
   { 0,			Ukn	ITR(1, "sigqueue",	"")	}, /*	190 */
   { 0,			Ukn	ITR(1, "clock_gettime",	"")	}, /*	191 */
   { 0,			Ukn	ITR(1, "clock_settime",	"")	}, /*	192 */
   { 0,			Ukn	ITR(1, "clock_getres",	"")	}, /*	193 */
   { 0,			Ukn	ITR(1, "timer_create",	"")	}, /*	194 */
   { 0,			Ukn	ITR(1, "timer_delete",	"")	}, /*	195 */
   { 0,			Ukn	ITR(1, "timer_settime",	"")	}, /*	196 */
   { 0,			Ukn	ITR(1, "timer_gettime",	"")	}, /*	197 */
   { 0,			Ukn	ITR(1, "timer_getoverrun","")	}, /*	198 */
   { sys_nanosleep,	2	ITR(1, "nanosleep",	"pp")	}, /*	199 */
   { 0,			Ukn	ITR(1, "modstat",	"")	}, /*	200 */
   { 0,			Ukn	ITR(1, "facl",		"")	}, /*	201 */
   { sys_setreuid,	2	ITR(1, "setreuid",	"dd")	}, /*	202 */
   { sys_setregid,	2	ITR(1, "setregid",	"dd")	}, /*	203 */
   { 0,			Ukn	ITR(1, "install_utrap",	"")	}, /*	204 */
   { 0,			Ukn	ITR(1, "signotify",	"")	}, /*	205 */
   { 0,			Ukn	ITR(1, "schedctl",	"")	}, /*	206 */
   { 0,			Ukn	ITR(1, "pset",		"")	}, /*	207 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /* 208 */
   { 0,			Ukn	ITR(1, "resolvepath",	"")	}, /* 209 */
   { 0,			Ukn	ITR(1, "signotifywait",	"")	}, /* 210 */
   { 0,			Ukn	ITR(1, "lwp_sigredirect","")	}, /* 211 */
   { 0,			Ukn	ITR(1, "lwp_alarm",	"")	}, /* 212 */
   { sol_getdents64,	3  	ITR(0, "getdents64", 	"dxd")	}, /* 213 */
   { sol_mmap64,	7	ITR(1, "mmap64",      "pxdddxx")}, /*214 */
   { sol_stat64,      	2	ITR(0, "stat64",       	"sp")	}, /* 215 */
   { sol_lstat64,	2	ITR(0, "lstat64",	"sp")	}, /* 216 */
   { sol_fstat64,	2	ITR(0, "fstat64",       "dp")	}, /* 217 */
   { 0,			Ukn	ITR(1, "statvfs64",	"")	}, /* 218 */
   { 0,			Ukn	ITR(1, "fstatvfs64",	"")	}, /* 219 */
   { 0,			Ukn	ITR(1, "setrlimit64",	"")	}, /* 220 */
   { 0,			Ukn	ITR(1, "getrlimit64",	"")	}, /* 221 */
   { 0,			Ukn	ITR(1, "pread64",	"")	}, /* 222 */
   { 0,			Ukn	ITR(1, "pwrite64",	"")	}, /* 223 */
   { 0,			Ukn	ITR(1, "creat64",	"")	}, /* 224 */
   { sol_open64,      	3	ITR(0, "open64",       	"soo")	}, /* 225 */
   { 0,			Ukn	ITR(1, "rpcsys",	"")	}, /* 226 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /* 227 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /* 228 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /* 229 */
   { solaris_socket,	3	ITR(1, "so_socket",	"ddd")	}, /* 230 */
   { solaris_socketpair,1	ITR(1, "so_socketpair",	"dddx")	}, /* 231 */
   { solaris_bind,	3	ITR(1, "bind",		"dxd")	}, /* 232 */
   { solaris_listen,	2	ITR(1, "listen",	"dd")	}, /* 233 */
   { solaris_accept,	3	ITR(1, "accept",	"dxx")	}, /* 234 */
   { solaris_connect,	3	ITR(1, "connect",	"dxd")	}, /* 235 */
   { solaris_shutdown,	2	ITR(1, "shutdown",	"dd")	}, /* 236 */
   { solaris_recv,	4	ITR(1, "recv",		"dxdd")	}, /* 237 */
   { solaris_recvfrom,	6	ITR(1, "recvfrom",     "dxddxd")}, /* 238 */
   { solaris_recvmsg,	3	ITR(1, "recvmsg",	"dxd")	}, /* 239 */
   { solaris_send,	4	ITR(1, "send",		"dxdd")	}, /* 240 */
   { solaris_sendmsg,  	3	ITR(0, "sendmsg",      	"dxd")	}, /* 241 */
   { solaris_sendto,	6	ITR(1, "sendto",       "dxddxd")}, /* 242 */
   { solaris_getpeername,3	ITR(1, "getpeername",	"dxx")	}, /* 243 */
   { solaris_getsockname,3	ITR(1, "getsockname",	"dxx")	}, /* 244 */
   { solaris_getsockopt,5	ITR(1, "getsockopt",	"dddxx")}, /* 245 */
   { solaris_setsockopt,5	ITR(1, "setsockopt",	"dddxd")}, /* 246 */
   { 0,			Ukn	ITR(1, "sockconfig",	"")	}, /* 247 */
   { 0,			Ukn	ITR(1, "ntp_gettime",	"")	}, /* 248 */
   { 0,     	 	Ukn	ITR(0, "ntp_adjtime",  	"")	}, /* 249 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /* 250 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /* 251 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /* 252 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /* 253 */
   { 0,			Ukn	ITR(1, "?",		"")	}, /* 254 */
   { 0,			Ukn	ITR(1, "?",		"")	}  /* 255 */
};

static void solaris_lcall7(int segment, struct pt_regs * regs)
{
	int sysno = regs->eax & 0xff;

	if (sysno >= ARRAY_SIZE(solaris_syscall_table))
		set_error(regs, iABI_errors(-EINVAL));
	else
		lcall7_dispatch(regs, &solaris_syscall_table[sysno], 1);
}

static struct exec_domain solaris_exec_domain = {
	name:		"Solaris",
	handler:	solaris_lcall7,
	pers_low:	13 /* PER_SOLARIS */,
	pers_high:	13 /* PER_SOLARIS */,
	signal_map:	solaris_to_linux_signals,
	signal_invmap:	linux_to_solaris_signals,
	err_map:	solaris_err_map,
	socktype_map:	solaris_socktype_map,
	sockopt_map:	solaris_sockopt_map,
	af_map:		solaris_af_map,
	module:		THIS_MODULE,
};


static void __exit solaris_cleanup(void)
{
	unregister_exec_domain(&solaris_exec_domain);
}

static int __init solaris_init(void)
{
	return register_exec_domain(&solaris_exec_domain);
}

module_init(solaris_init);
module_exit(solaris_cleanup);
