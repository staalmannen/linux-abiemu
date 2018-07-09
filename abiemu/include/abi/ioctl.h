/*
 * Copyright (C) 1994  Mike Jagdis (jaggy@purplet.demon.co.uk)
 * Copyright (C) 2001  Caldera Deutschland GmbH
 */
#ifndef _ABI_IOCTL_H
#define _ABI_IOCTL_H

#ident "%W% %G%"

/*
 * Ioctl's have the command encoded in the lower word, and the size of
 * any in or out parameters in the upper word.  The high 3 bits of the
 * upper word are used to encode the in/out status of the parameter.
 *
 * Note that Linux does the same but has the IOC_IN and IOC_OUT values
 * round the other way and uses 0 for IOC_VOID.
 */
enum {
	/* parameter length, at most 13 bits */
	BSD_IOCPARM_MASK	= 0x1fff,
	/* no parameters */
	BSD_IOC_VOID		= 0x20000000,
	/* copy out parameters */
	BSD_IOC_OUT		= 0x40000000,
	/* copy in parameters */
	BSD_IOC_IN		= 0x80000000,
	/* possibly copy in and out parameters */
	BSD_IOC_INOUT		= BSD_IOC_IN|BSD_IOC_OUT,
};


#define BSD__IOC(inout,group,num,len) \
	(inout | ((len & BSD_IOCPARM_MASK) << 16) | ((group) << 8) | (num))

#define	BSD__IO(g,n)		BSD__IOC(BSD_IOC_VOID, (g), (n), 0)
#define	BSD__IOR(g,n,t)		BSD__IOC(BSD_IOC_OUT, (g), (n), sizeof(t))
#define	BSD__IOW(g,n,t)		BSD__IOC(BSD_IOC_IN, (g), (n), sizeof(t))
#define	BSD__IOWR(g,n,t)	BSD__IOC(BSD_IOC_INOUT,	(g), (n), sizeof(t))

/* Some SYSV systems exhibit "compatible" BSD ioctls without the bumf. */
#define BSD__IOV(c,d)	(((c) << 8) | (d))

#endif /* _ABI_IOCTL_H */
