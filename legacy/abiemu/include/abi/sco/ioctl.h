#ifndef _ABI_SCO_IOCTL_H
#define _ABI_SCO_IOCTL_H

/* tapeio.c */
extern int	sco_tape_ioctl(int, u_int, caddr_t);

/* termios.c */
extern int	sco_term_ioctl(int, u_int, caddr_t);

/* From vtkd.c */
extern int	sco_vtkbd_ioctl(int, u_int, caddr_t);

#endif /* _ABI_SCO_IOCTL_H */
