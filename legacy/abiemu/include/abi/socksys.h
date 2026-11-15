/*
 * Copyright (C) 1994  Mike Jagdis (jaggy@purplet.demon.co.uk)
 */

#ident "%W% %G%"

#include <linux/netdevice.h>
/* Get struct rtentry from linux/route.h - this should be compatible. */
#include <linux/route.h>
/* Get struct arpreq from linux/if_arp.h - this should be compatible. */
#include <linux/if_arp.h>
/* Get struct ifreq and struct ifconf from linux/if.h - these should
 * be compatible. */
#include <linux/if.h>


struct socksysreq {
	int             args[7];
};


struct socknewproto {
	int             family;	/* address family (AF_INET, etc.) */
	int             type;	/* protocol type (SOCK_STREAM, etc.) */
	int             proto;	/* per family proto number */
	dev_t           dev;	/* major/minor to use (must be a clone) */
	int             flags;	/* protosw flags */
};


/* These are nothing to do with what we know as SO_*. I think they
 * are the command requests which appear in socksysreq structures?
 */
#define  SSYS_SO_ACCEPT		1
#define  SSYS_SO_BIND		2
#define  SSYS_SO_CONNECT	3
#define  SSYS_SO_GETPEERNAME	4
#define  SSYS_SO_GETSOCKNAME	5
#define  SSYS_SO_GETSOCKOPT	6
#define  SSYS_SO_LISTEN		7
#define  SSYS_SO_RECV		8
#define  SSYS_SO_RECVFROM	9
#define  SSYS_SO_SEND		10
#define  SSYS_SO_SENDTO		11
#define  SSYS_SO_SETSOCKOPT	12
#define  SSYS_SO_SHUTDOWN	13
#define  SSYS_SO_SOCKET		14
#define  SSYS_SO_SELECT		15
#define  SSYS_SO_GETIPDOMAIN	16
#define  SSYS_SO_SETIPDOMAIN	17
#define  SSYS_SO_ADJTIME	18
#define  SSYS_SO_SETREUID	19
#define  SSYS_SO_SETREGID	20
#define  SSYS_SO_GETTIME	21
#define  SSYS_SO_SETTIME	22
#define  SSYS_SO_GETITIMER	23
#define  SSYS_SO_SETITIMER	24
#define  SSYS_SO_RECVMSG	25	/* Here down is SCO 3.2v5 and up */
#define  SSYS_SO_SENDMSG	26
#define  SSYS_SO_SOCKPAIR	27


/* We encode the ioctl numbers using the argument size as part of
 * the number. This will warn us if we haven't got compatible
 * structures :-).
 *   Naturally the SVR3/Lachman ioctl numbers are different from the
 * BSD/SVR4-XTI ioctl numbers. What more would you expect?
 */

#define	SSYS_IOCPARM_MASK	0xff		/* parameters must be < 256 bytes */
#define	SSYS_IOC_VOID		0x20000000	/* no parameters */
#define	SSYS_IOC_OUT		0x40000000	/* copy out parameters */
#define	SSYS_IOC_IN		0x80000000	/* copy in parameters */
#define	SSYS_IOC_INOUT		(SSYS_IOC_IN|SSYS_IOC_OUT)

#define	SSYS_IOS(x,y)		(SSYS_IOC_VOID|(x<<8)|y)
#define	SSYS_IOSR(x,y,t)	(SSYS_IOC_OUT|((sizeof(t)&SSYS_IOCPARM_MASK)<<16)|(x<<8)|y)
#define	SSYS_IOSW(x,y,t)	(SSYS_IOC_IN|((sizeof(t)&SSYS_IOCPARM_MASK)<<16)|(x<<8)|y)
#define	SSYS_IOSWR(x,y,t)	(SSYS_IOC_INOUT|((sizeof(t)&SSYS_IOCPARM_MASK)<<16)|(x<<8)|y)

#define SSYS_SIOCSHIWAT		SSYS_IOSW('S', 1, int)	/* set high watermark */
#define SSYS_SIOCGHIWAT		SSYS_IOSR('S', 2, int)	/* get high watermark */
#define SSYS_SIOCSLOWAT		SSYS_IOSW('S', 3, int)	/* set low watermark */
#define SSYS_SIOCGLOWAT		SSYS_IOSR('S', 4, int)	/* get low watermark */
#define SSYS_SIOCATMARK		SSYS_IOSR('S', 5, int)	/* at oob mark? */
#define SSYS_SIOCSPGRP		SSYS_IOSW('S', 6, int)	/* set process group */
#define SSYS_SIOCGPGRP		SSYS_IOSR('S', 7, int)	/* get process group */


#define SSYS_FIONREAD		SSYS_IOSR('S', 8, int)	/* BSD compatibilty */
#define SSYS_FIONBIO		SSYS_IOSW('S', 9, int)	/* BSD compatibilty */
#define SSYS_FIOASYNC		SSYS_IOSW('S', 10, int)	/* BSD compatibilty */
#define SSYS_SIOCPROTO		SSYS_IOSW('S', 11, struct socknewproto)	/* link proto */
#define SSYS_SIOCGETNAME	SSYS_IOSR('S', 12, struct sockaddr)	/* getsockname */
#define SSYS_SIOCGETPEER	SSYS_IOSR('S', 13, struct sockaddr)	/* getpeername */
#define SSYS_IF_UNITSEL		SSYS_IOSW('S', 14, int)	/* set unit number */
#define SSYS_SIOCXPROTO		SSYS_IOS('S', 15)	/* empty proto table */

#define	SSYS_SIOCADDRT		SSYS_IOSW('R', 9, struct rtentry)	/* add route */
#define	SSYS_SIOCDELRT		SSYS_IOSW('R', 10, struct rtentry)	/* delete route */

#define	SSYS_SIOCSIFADDR	SSYS_IOSW('I', 11, struct ifreq)	/* set ifnet address */
#define	SSYS_SIOCGIFADDR	SSYS_IOSWR('I', 12, struct ifreq)	/* get ifnet address */
#define	SSYS_SIOCSIFDSTADDR	SSYS_IOSW('I', 13, struct ifreq)	/* set p-p address */
#define	SSYS_SIOCGIFDSTADDR	SSYS_IOSWR('I', 14, struct ifreq)	/* get p-p address */
#define	SSYS_SIOCSIFFLAGS	SSYS_IOSW('I', 15, struct ifreq)	/* set ifnet flags */
#define	SSYS_SIOCGIFFLAGS	SSYS_IOSWR('I', 16, struct ifreq)	/* get ifnet flags */
#define	SSYS_SIOCGIFCONF	SSYS_IOSWR('I', 17, struct ifconf)	/* get ifnet list */

#define	SSYS_SIOCSIFMTU		SSYS_IOSW('I', 21, struct ifreq)	/* get if_mtu */
#define	SSYS_SIOCGIFMTU		SSYS_IOSWR('I', 22, struct ifreq)	/* set if_mtu */

#define SSYS_SIOCIFDETACH	SSYS_IOSW('I', 26, struct ifreq)	/* detach interface */
#define SSYS_SIOCGENPSTATS	SSYS_IOSWR('I', 27, struct ifreq)	/* get ENP stats */

#define SSYS_SIOCX25XMT		SSYS_IOSWR('I', 29, struct ifreq)	/* start a slp proc in
							 * x25if */
#define SSYS_SIOCX25RCV		SSYS_IOSWR('I', 30, struct ifreq)	/* start a slp proc in
							 * x25if */
#define SSYS_SIOCX25TBL		SSYS_IOSWR('I', 31, struct ifreq)	/* xfer lun table to
							 * kernel */

#define	SSYS_SIOCGIFBRDADDR	SSYS_IOSWR('I', 32, struct ifreq)	/* get broadcast addr */
#define	SSYS_SIOCSIFBRDADDR	SSYS_IOSW('I', 33, struct ifreq)	/* set broadcast addr */
#define	SSYS_SIOCGIFNETMASK	SSYS_IOSWR('I', 34, struct ifreq)	/* get net addr mask */
#define	SSYS_SIOCSIFNETMASK	SSYS_IOSW('I', 35, struct ifreq)	/* set net addr mask */
#define	SSYS_SIOCGIFMETRIC	SSYS_IOSWR('I', 36, struct ifreq)	/* get IF metric */
#define	SSYS_SIOCSIFMETRIC	SSYS_IOSW('I', 37, struct ifreq)	/* set IF metric */

#define	SSYS_SIOCSARP		SSYS_IOSW('I', 38, struct arpreq)	/* set arp entry */
#define	SSYS_SIOCGARP		SSYS_IOSWR('I', 39, struct arpreq)	/* get arp entry */
#define	SSYS_SIOCDARP		SSYS_IOSW('I', 40, struct arpreq)	/* delete arp entry */

#define SSYS_SIOCSIFNAME	SSYS_IOSW('I', 41, struct ifreq)	/* set interface name */
#define	SSYS_SIOCGIFONEP	SSYS_IOSWR('I', 42, struct ifreq)	/* get one-packet params */
#define	SSYS_SIOCSIFONEP	SSYS_IOSW('I', 43, struct ifreq)	/* set one-packet params */

#define SSYS_SIOCGENADDR	SSYS_IOSWR('I', 65, struct ifreq)	/* Get ethernet addr */

#define SSYS_SIOCSOCKSYS	SSYS_IOSW('I', 66, struct socksysreq)	/* Pseudo socket syscall */


#define	SVR4_SIOCSHIWAT		SSYS_IOSW('s',  0, int)	/* set high watermark */
#define	SVR4_SIOCGHIWAT		SSYS_IOSR('s',  1, int)	/* get high watermark */
#define	SVR4_SIOCSLOWAT		SSYS_IOSW('s',  2, int)	/* set low watermark */
#define	SVR4_SIOCGLOWAT		SSYS_IOSR('s',  3, int)	/* get low watermark */
#define	SVR4_SIOCATMARK		SSYS_IOSR('s',  7, int)	/* at oob mark? */
#define	SVR4_SIOCSPGRP		SSYS_IOSW('s',  8, int)	/* set process group */
#define	SVR4_SIOCGPGRP		SSYS_IOSR('s',  9, int)		/* get process group */

#define	SVR4_SIOCADDRT		SSYS_IOSW('r', 10, struct rtentry)	/* add route */
#define	SVR4_SIOCDELRT		SSYS_IOSW('r', 11, struct rtentry)	/* delete route */

#define	SVR4_SIOCSIFADDR	SSYS_IOSW('i', 12, struct ifreq)	/* set ifnet address */
#define	SVR4_SIOCGIFADDR	SSYS_IOSWR('i',13, struct ifreq)	/* get ifnet address */
#define	SVR4_SIOCSIFDSTADDR	SSYS_IOSW('i', 14, struct ifreq)	/* set p-p address */
#define	SVR4_SIOCGIFDSTADDR	SSYS_IOSWR('i',15, struct ifreq)	/* get p-p address */
#define	SVR4_SIOCSIFFLAGS	SSYS_IOSW('i', 16, struct ifreq)	/* set ifnet flags */
#define	SVR4_SIOCGIFFLAGS	SSYS_IOSWR('i',17, struct ifreq)	/* get ifnet flags */
#define	SVR4_SIOCSIFMEM		SSYS_IOSW('i', 18, struct ifreq)	/* set interface mem */
#define	SVR4_SIOCGIFMEM		SSYS_IOSWR('i',19, struct ifreq)	/* get interface mem */
#define	SVR4_SIOCGIFCONF	SSYS_IOSWR('i',20, struct ifconf)	/* get ifnet list */
#define	SVR4_SIOCSIFMTU		SSYS_IOSW('i', 21, struct ifreq)	/* set if_mtu */
#define	SVR4_SIOCGIFMTU		SSYS_IOSWR('i',22, struct ifreq)	/* get if_mtu */

	/* from 4.3BSD */
#define	SVR4_SIOCGIFBRDADDR	SSYS_IOSWR('i',23, struct ifreq)	/* get broadcast addr */
#define	SVR4_SIOCSIFBRDADDR	SSYS_IOSW('i',24, struct ifreq)	/* set broadcast addr */
#define	SVR4_SIOCGIFNETMASK	SSYS_IOSWR('i',25, struct ifreq)	/* get net addr mask */
#define	SVR4_SIOCSIFNETMASK	SSYS_IOSW('i',26, struct ifreq)	/* set net addr mask */
#define	SVR4_SIOCGIFMETRIC	SSYS_IOSWR('i',27, struct ifreq)	/* get IF metric */
#define	SVR4_SIOCSIFMETRIC	SSYS_IOSW('i',28, struct ifreq)	/* set IF metric */

#define	SVR4_SIOCSARP		SSYS_IOSW('i', 30, struct arpreq)	/* set arp entry */
#define	SVR4_SIOCGARP		SSYS_IOSWR('i',31, struct arpreq)	/* get arp entry */
#define	SVR4_SIOCDARP		SSYS_IOSW('i', 32, struct arpreq)	/* delete arp entry */
#define	SVR4_SIOCUPPER		SSYS_IOSW('i', 40, struct ifreq)       /* attach upper layer */
#define	SVR4_SIOCLOWER		SSYS_IOSW('i', 41, struct ifreq)       /* attach lower layer */
#define	SVR4_SIOCSETSYNC	SSYS_IOSW('i',  44, struct ifreq)	/* set syncmode */
#define	SVR4_SIOCGETSYNC	SSYS_IOSWR('i', 45, struct ifreq)	/* get syncmode */
#define	SVR4_SIOCSSDSTATS	SSYS_IOSWR('i', 46, struct ifreq)	/* sync data stats */
#define	SVR4_SIOCSSESTATS	SSYS_IOSWR('i', 47, struct ifreq)	/* sync error stats */

#define	SVR4_SIOCSPROMISC	SSYS_IOSW('i', 48, int)		/* request promisc mode
							   on/off */
#define	SVR4_SIOCADDMULTI	SSYS_IOSW('i', 49, struct ifreq)	/* set m/c address */
#define	SVR4_SIOCDELMULTI	SSYS_IOSW('i', 50, struct ifreq)	/* clr m/c address */

/* protocol i/o controls */
#define	SVR4_SIOCSNIT		SSYS_IOSW('p',  0, struct nit_ioc)	/* set nit modes */
#define	SVR4_SIOCGNIT		SSYS_IOSWR('p', 1, struct nit_ioc)	/* get nit modes */

/* STREAMS based socket emulation */

#define SVR4_SIOCPROTO		SSYS_IOSW('s', 51, struct socknewproto)	/* link proto */
#define SVR4_SIOCGETNAME	SSYS_IOSR('s', 52, struct sockaddr)	/* getsockname */
#define SVR4_SIOCGETPEER	SSYS_IOSR('s', 53, struct sockaddr)	/* getpeername */
#define SVR4_IF_UNITSEL		SSYS_IOSW('s', 54, int)	/* set unit number */
#define SVR4_SIOCXPROTO		SSYS_IOS('s', 55)	/* empty proto table */

#define SVR4_SIOCIFDETACH	SSYS_IOSW('i', 56, struct ifreq)	/* detach interface */
#define SVR4_SIOCGENPSTATS	SSYS_IOSWR('i', 57, struct ifreq)	/* get ENP stats */
#define SVR4_SIOCX25XMT		SSYS_IOSWR('i', 59, struct ifreq)	/* start a slp proc in
							 * x25if */
#define SVR4_SIOCX25RCV		SSYS_IOSWR('i', 60, struct ifreq)	/* start a slp proc in
							 * x25if */
#define SVR4_SIOCX25TBL		SSYS_IOSWR('i', 61, struct ifreq)	/* xfer lun table to
							 * kernel */
#define SVR4_SIOCSLGETREQ	SSYS_IOSWR('i', 71, struct ifreq)	/* wait for switched
							 * SLIP request */
#define SVR4_SIOCSLSTAT		SSYS_IOSW('i', 72, struct ifreq)	/* pass SLIP info to
							 * kernel */
#define SVR4_SIOCSIFNAME	SSYS_IOSW('i', 73, struct ifreq)	/* set interface name */
#define SVR4_SIOCGENADDR	SSYS_IOSWR('i', 85, struct ifreq)	/* Get ethernet addr */
#define SVR4_SIOCSOCKSYS	SSYS_IOSW('i', 86, struct socksysreq)	/* Pseudo socket syscall */


#if 0
/* Strange, there also seem to be two byte SVR4 ioctls used which are
 * not simply the BSD style ioctl with the high word masked out. i.e the
 * class character doesn't match what I expect.
 */
#define SVRX_SIOCGIFCONF	0x8912
#define SVRX_SIOCGIFFLAGS	0x8913
#endif


/* With NFS/NIS there is also a pseudo device /dev/nfsd which understands
 * some ioctls. Since these don't conflict with the socksys ioctls we
 * just link nfsd to socksys and let socksys handle both sets.
 */
#define NIOCNFSD	1
#define NIOCOLDGETFH	2
#define NIOCASYNCD	3
#define NIOCSETDOMNAM	4
#define NIOCGETDOMNAM	5
#define NIOCCLNTHAND	6
#define NIOCEXPORTFS	7
#define NIOCGETFH	8
#define NIOCLSTAT	9

/* These ioctls take argument structures... */
struct domnam_args {
	char *name;
	int namelen;
};

struct lstat_args {
	char *fname;
	void *statb;
};

#define NFS_FHSIZE	32
typedef union {
	struct {
		unsigned short	fsid;		/* filesystem id (device) */
		unsigned long	fno;		/* file number (inode) */
		unsigned long	fgen;		/* file generation */
		unsigned short	ex_fsid;	/* exported fs id (device) */
		unsigned long	ex_fno;		/* exported file no (inode) */
		unsigned long	ex_fgen;	/* exported file gen */
	} fh;
	char pad[NFS_FHSIZE];
} fhandle_t;

struct getfh_args {
	char *fname;
	fhandle_t *fhp;
};

extern void		inherit_socksys_funcs(unsigned int fd, int state);
extern int		socksys_fdinit(int fd, int rw, const char *buf, int *count);
