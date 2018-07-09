/*
 *  Copyright (C) 1994  Mike Jagdis (jaggy@purplet.demon.co.uk)
 */

#ident "%W% %G%"

/*
 * Wyse extensions for 4.3 BSD TCP/IP in V/386 3.2.1A
 */
#include <linux/ioctl.h>

/* Get struct rtentry from linux/route.h - this should be compatible. */
#include <linux/route.h>
/* Get struct arpreq from linux/if_arp.h - this should be compatible. */
#include <linux/if_arp.h>
/* Get struct ifreq and struct ifconf from linux/if.h - these should
 * be compatible. */
#include <linux/if.h>


/* Wyse use BSD style ioctls. This will warn us if we haven't got compatible
 * structures :-).
 */

/* socket i/o controls */
#define	WVR3_SIOCSHIWAT		BSD__IOW('s',  0, int)	/* set high watermark */
#define	WVR3_SIOCGHIWAT		BSD__IOR('s',  1, int)	/* get high watermark */
#define	WVR3_SIOCSLOWAT		BSD__IOW('s',  2, int)	/* set low watermark */
#define	WVR3_SIOCGLOWAT		BSD__IOR('s',  3, int)	/* get low watermark */
#define	WVR3_SIOCATMARK		BSD__IOR('s',  7, int)	/* at oob mark? */
#define	WVR3_SIOCSPGRP		BSD__IOW('s',  8, int)	/* set process group */
#define	WVR3_SIOCGPGRP		BSD__IOR('s',  9, int)	/* get process group */

#define	WVR3_SIOCADDRT		BSD__IOW('r', 10, struct rtentry)	/* add route */
#define	WVR3_SIOCDELRT		BSD__IOW('r', 11, struct rtentry)	/* delete route */

#define	WVR3_SIOCSIFADDR	BSD__IOW('i', 12, struct ifreq)	/* set ifnet address */
#define	WVR3_SIOCGIFADDR	BSD__IOWR('i',13, struct ifreq)	/* get ifnet address */
#define	WVR3_SIOCSIFDSTADDR	BSD__IOW('i', 14, struct ifreq)	/* set p-p address */
#define	WVR3_SIOCGIFDSTADDR	BSD__IOWR('i',15, struct ifreq)	/* get p-p address */
#define	WVR3_SIOCSIFFLAGS	BSD__IOW('i', 16, struct ifreq)	/* set ifnet flags */
#define	WVR3_SIOCGIFFLAGS	BSD__IOWR('i',17, struct ifreq)	/* get ifnet flags */
#define	WVR3_SIOCGIFBRDADDR	BSD__IOWR('i',18, struct ifreq)	/* get broadcast addr */
#define	WVR3_SIOCSIFBRDADDR	BSD__IOW('i',19, struct ifreq)	/* set broadcast addr */
#define	WVR3_SIOCGIFCONF	BSD__IOWR('i',20, struct ifconf)	/* get ifnet list */
#define	WVR3_SIOCGIFNETMASK	BSD__IOWR('i',21, struct ifreq)	/* get net addr mask */
#define	WVR3_SIOCSIFNETMASK	BSD__IOW('i',22, struct ifreq)	/* set net addr mask */
#define	WVR3_SIOCGIFMETRIC	BSD__IOWR('i',23, struct ifreq)	/* get IF metric */
#define	WVR3_SIOCSIFMETRIC	BSD__IOW('i',24, struct ifreq)	/* set IF metric */
#define	WVR3_SIOCSIFHADDR	BSD__IOW('i', 25, struct ifreq)	/* set hardware addr */
#define	WVR3_SIOCGIFHADDR	BSD__IOWR('i',26, struct ifreq)	/* get hardware addr */
#define	WVR3_SIOCRIFHADDR	BSD__IOW('i', 27, struct ifreq)     /* reset hardware addr */

#define	WVR3_SIOCSARP		BSD__IOW('i', 30, struct arpreq)	/* set arp entry */
#define	WVR3_SIOCGARP		BSD__IOWR('i',31, struct arpreq)	/* get arp entry */
#define	WVR3_SIOCDARP		BSD__IOW('i', 32, struct arpreq)	/* delete arp entry */

#define	WVR3_SIOCADDMULTI	BSD__IOW('i', 33, struct ifreq)	/* set multicast addr */
#define	WVR3_SIOCDELMULTI	BSD__IOW('i', 34, struct ifreq)	/* set multicast addr */
