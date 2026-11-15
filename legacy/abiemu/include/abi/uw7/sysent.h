#ifndef _ABI_UW7_SYSENT_H
#define _ABI_UW7_SYSENT_H

#ident "%W% %G%"

/*
 * External function declarations for the SCO UnixWare 7 syscall table.
 */

struct pt_regs;
struct uw7_rlimit64;
struct uw7_stack;
struct uw7_statvfs64;


/* access.c */
extern int	uw7_access(char *, int);

/* context.c */
extern int	uw7_context(struct pt_regs *);
extern int	uw7_sigaltstack(const struct uw7_stack *, struct uw7_stack *);

/* misc.c */
extern int	uw7_sleep(int);
extern int	uw7_seteuid(int);
extern int	uw7_setegid(int);
extern int	uw7_pread(u_int, char *, int, long);
extern int	uw7_pwrite(u_int, char *, int, long);
extern int	uw7_stty(int, int);
extern int	uw7_gtty(int, int);

/* ioctl.c */
extern int	uw7_ioctl(struct pt_regs *);

/* lfs.c */
extern int	uw7_truncate64(const char *, u_long, u_long);
extern int	uw7_ftruncate64(int, u_long, u_long);
extern int	uw7_statvfs64(char *, struct uw7_statvfs64 *);
extern int	uw7_fstatvfs64(int, struct uw7_statvfs64 *);
extern int	uw7_getrlimit64(int, struct uw7_rlimit64 *);
extern int	uw7_setrlimit64(int, const struct uw7_rlimit64 *);
extern int	uw7_lseek64(int, u_int, u_int, int);
extern ssize_t	uw7_pread64(int, char *, int, u_int, u_int);
extern ssize_t	uw7_pwrite64(int, char *, int, u_int, u_int);
extern int	uw7_creat64(const char *, int);

/* mac.c */
extern int	uw7_mldmode(int);

/* mmap.c */
extern int	uw7_mmap64(u_long, size_t, int, int, int, u_long, u_long);

/* stat.c */
extern int	uw7_xstat(int, char *, void *);
extern int	uw7_lxstat(int, char *, void *);
extern int	uw7_fxstat(int, int, void *);

#endif /* _ABI_UW7_SYSENT_H */
