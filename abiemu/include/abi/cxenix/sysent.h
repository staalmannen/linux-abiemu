/*
 *   include/abi/xnx.h -- xenix ibcs interface
 *
 * Copyright (C) 1993  Drew Sullivan
 * Released for general use as long as this copyright remains.
 */

#ident "%W% %G%"

struct sco_sigaction;
struct timeb;

typedef unsigned short excode_t;

extern	int xnx_locking(int fd, int mode, unsigned long size);
extern	int xnx_creatsem(char *sem_name, int mode);
extern	int xnx_opensem(char *sem_name);
extern	int xnx_sigsem(int sem_num);
extern	int xnx_waitsem(int sem_num);
extern	int xnx_nbwaitsem(int sem_num);
extern	int xnx_rdchk(int fd);
extern	int xnx_ftime(struct timeb * tp);
extern	int xnx_nap(long period);
extern	int xnx_sdget(char *path, int flags, long size, int mode);
extern	int xnx_sdfree(char* addr);
extern	int xnx_sdenter(char *addr, int flags);
extern	int xnx_sdleave(char *addr);
extern	int xnx_sdgetv(char *addr);
extern	int xnx_sdwaitv(char *addr, int vnum);
extern	int xnx_proctl(int pid, int command, char *arg);
extern	int xnx_execseg(excode_t oldaddr, unsigned size);
extern	int xnx_unexecseg(excode_t addr);
extern	int xnx_eaccess(char *path, int mode);
extern	int xnx_paccess(int pid, int cmd, int offset, int count, char *ptr);
extern	int xnx_sigpending(unsigned long *set);
extern	int xnx_pathconf(char *path, int name);
extern	int xnx_fpathconf(int fildes, int name);

/* signal.c */
extern int	xnx_sigaction(int, const struct sco_sigaction *,
			struct sco_sigaction *);

/* utsname.c */
extern int	xnx_utsname(u_long addr);

/* sysent.h */
extern void     cxenix(struct pt_regs *);
