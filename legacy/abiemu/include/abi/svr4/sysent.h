#ifndef _ABI_SVR4_SYSENT_H
#define _ABI_SVR4_SYSENT_H

#ident "%W% %G%"

/*
 *  Function prototypes used for the SVR4 emulator.
 */

#include <abi/svr4/types.h>
#include <abi/svr4/statfs.h>

struct abi_sigaction;
struct ibcs_statfs;
struct svr4_stat;
struct svr4_siginfo;
struct svr4_sigset;
struct svr4_statvfs;
struct pt_regs;
struct timeval;


/* MD (lcall7.c for i386) */
extern int	lcall7_syscall(struct pt_regs *);
#define abi_syscall lcall7_syscall

/* fcntl.c */
extern unsigned short fl_svr4_to_linux[];
extern int	svr4_fcntl(int, unsigned int, unsigned long);

/* hrtsys.c */
extern int	svr4_hrtsys(struct pt_regs *);

/* ioctl.c */
extern int	svr4_ioctl(struct pt_regs *);

/* ipc.c */
extern int	svr4_semsys(struct pt_regs *);
extern int	svr4_shmsys(struct pt_regs *);
extern int	svr4_msgsys(struct pt_regs *);

/* misc.c */
extern int	abi_brk(u_long);
extern int	abi_exec(struct pt_regs *);
extern int	abi_fork(struct pt_regs *);
extern int	abi_getpid(struct pt_regs *);
extern int	abi_getuid(struct pt_regs *);
extern int	abi_getgid(struct pt_regs *);
extern int	abi_mkdir(const char *, int);
extern int	svr4_mknod(char *, svr4_o_mode_t, svr4_o_dev_t);
extern int	svr4_xmknod(int, char *, svr4_mode_t, svr4_dev_t);
extern int	abi_kill(int, int);
extern int	abi_pipe(struct pt_regs *);
extern int	abi_procids(struct pt_regs *);
extern int	abi_read(int, char *, int);
extern int	abi_select(int, void *, void *, void *, struct timeval *);
extern int	abi_time(void);
extern int	abi_wait(struct pt_regs *);

/* mmap.c */
extern u_long	svr4_mmap(u_long, size_t, int, int, int, svr4_off_t);

/* open.c */
extern int	svr4_open(const char *, int, int);
extern int	svr4_statfs(const char *, struct svr4_statfs *, int, int);
extern int	svr4_fstatfs(unsigned int, struct svr4_statfs *, int, int);
extern int	svr4_getdents(int, char *, int);

/* signal.c */
extern int	abi_sigsuspend(struct pt_regs *);
extern int	abi_sigfunc(struct pt_regs *);
extern int	abi_sigaction(int, const struct abi_sigaction *,
			struct abi_sigaction *);
extern int	abi_sigprocmask(int, u_long *, u_long *);
extern int	abi_sigsuspend(struct pt_regs *);

/* socksys.c */
extern int	socksys_syscall(u_long *);

/* stream.c */
extern int	svr4_getmsg(struct pt_regs *);
extern int	svr4_putmsg(struct pt_regs *);
extern int	svr4_getpmsg(struct pt_regs *);
extern int	svr4_putpmsg(struct pt_regs *);

/* svr4.c */
extern int	svr4_access(char *, int);
extern int	svr4_waitid(int, int, struct svr4_siginfo *, int);
extern int	svr4_waitsys(struct pt_regs *);
extern int	svr4_seteuid(int);
extern int	svr4_setegid(int);
extern int	svr4_fpathconf(int, int);
extern int	svr4_pathconf(char *, int);
extern int	svr4_sigpending(int, struct svr4_sigset *);
extern int	svr4_context(struct pt_regs *);

/* sysconf.c */
extern int	ibcs_sysconf(int);

/* sysfs.c */
extern int	svr4_sysfs(int, int, int);

/* sysinfo.c */
extern int	svr4_sysinfo(int, char *, long);

/* ulimit.c */
extern int	svr4_ulimit(int, int);
extern int	svr4_getrlimit(int, void *);
extern int	svr4_setrlimit(int, void *);

/* stat.c */
extern int	svr4_stat(char *, struct svr4_stat *);
extern int	svr4_lstat(char *, struct svr4_stat *);
extern int	svr4_fstat(u_int, struct svr4_stat *);
extern int	svr4_xstat(int, char *, void *);
extern int	svr4_fxstat(int, int, void *);
extern int	svr4_lxstat(int, char *, void *);

/* sysconf.c */
extern int	svr4_sysconfig(int);

/* sysi86.c */
extern int	svr4_sysi86(int, void *, int);

/* utsname.c */
extern int	abi_utsname(u_long);
extern int	v7_utsname(u_long);

/* statvfs.c */
extern int	svr4_statvfs(char *, struct svr4_statvfs *);
extern int	svr4_fstatvfs(int, struct svr4_statvfs *);


#endif /* _ABI_SVR4_SYSENT_H */
