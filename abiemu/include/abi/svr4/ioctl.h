#ifndef _ABI_SVR4_IOCTL_H
#define _ABI_SVR4_IOCTL_H

/*
 *  Function prototypes used for SVR4 ioctl emulation.
 */

#ident "%W% %G%"

extern int      __svr4_ioctl(struct pt_regs *, int, unsigned long, void *);

/* consio.c */
extern int	svr4_console_ioctl(int, u_int, caddr_t);
extern int	svr4_video_ioctl(int, u_int, caddr_t);

/* filio.c */
extern int	svr4_fil_ioctl(int, u_int, caddr_t);

/* sockio.c */
extern int	svr4_stream_ioctl(struct pt_regs *regs, int, u_int, caddr_t);

/* socksys.c */
extern int	abi_ioctl_socksys(int, u_int, caddr_t);

/* tapeio.c */
extern int	svr4_tape_ioctl(int, u_int, caddr_t);

/* termios.c */
extern int	bsd_ioctl_termios(int, u_int, void *);
extern int	svr4_term_ioctl(int, u_int, caddr_t);
extern int	svr4_termiox_ioctl(int, u_int, caddr_t);

/* timod.c */
extern int	svr4_sockmod_ioctl(int, u_int, caddr_t);
extern int      do_getmsg(int, struct pt_regs *, char *, int,
                        int *, char *, int, int *, int *);
extern int      do_putmsg(int, struct pt_regs *, char *, int,
                        char *, int, int);

#endif /* _ABI_SVR4_IOCTL_H */
