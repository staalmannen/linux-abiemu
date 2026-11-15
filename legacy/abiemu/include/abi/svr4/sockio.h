/*
 * Copyright (c) 2001 Caldera Deutschland GmbH.
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
#ifndef _ABI_SVR4_SOCKIO_H
#define _ABI_SVR4_SOCKIO_H

#ident "%W% %G%"

#include <linux/socket.h>	/* for "struct sockaddr" et al  */
#include <abi/svr4/types.h>	/* for "svr4_caddr_t" et al */


#define SVR4_IFF_UP		0x0001
#define SVR4_IFF_BROADCAST	0x0002
#define SVR4_IFF_DEBUG		0x0004
#define SVR4_IFF_LOOPBACK	0x0008
#define SVR4_IFF_POINTOPOINT	0x0010
#define SVR4_IFF_NOTRAILERS	0x0020
#define SVR4_IFF_RUNNING	0x0040
#define SVR4_IFF_NOARP		0x0080
#define SVR4_IFF_PROMISC	0x0100
#define SVR4_IFF_ALLMULTI	0x0200
#define SVR4_IFF_INTELLIGENT	0x0400
#define SVR4_IFF_MULTICAST	0x0800
#define SVR4_IFF_MULTI_BCAST	0x1000
#define SVR4_IFF_UNNUMBERED	0x2000
#define SVR4_IFF_PRIVATE	0x8000


/*
 * Struct used for one-packet mode params in if ioctls
 */
struct svr4_onepacket {
	u_int	spsize;		/* short packet size */
	u_int	spthresh;	/* short packet threshold */
};

/*
 * Interface specific tuning information that TCP can use to its
 * advantage.
 */
struct svr4_ifperf {
	u_int	ip_recvspace;	/* Receive window to use */
	u_int	ip_sendspace;	/* Send window to use */
	u_int	ip_fullsize;	/* use full-size frames */
};

#if 0
/*
 * Interface request structure used for socket ioctl's.  All interface
 * ioctl's must have parameter definitions which begin with ifr_name.  The
 * remainder may be interface specific.
 */
struct svr4_ifreq {
#define SVR4_IFNAMSIZ        16
	union {
		char	ifrn_name[SVR4_IFNAMSIZ];
	} ifr_ifrn;
	union {
		struct sockaddr		ifru_addr;
		struct sockaddr		ifru_dstaddr;
		struct sockaddr		ifru_broadaddr;
		int			ifru_int[2];
		svr4_caddr_t		ifru_data;
		char			ifru_enaddr[6];
		struct svr4_onepacket	ifru_onepacket;
		struct svr4_ifperf	ifru_perf;
	}               ifr_ifru;
#define svr4_ifr_name	    ifr_ifrn.ifrn_name	    /* if name, e.g. "en0" */
#define svr4_ifr_addr	    ifr_ifru.ifru_addr	    /* address */
#define svr4_ifr_dstaddr    ifr_ifru.ifru_dstaddr   /* other end of ptp link */
#define svr4_ifr_broadaddr  ifr_ifru.ifru_broadaddr /* broadcast address */
#define svr4_ifr_flags	    ifr_ifru.ifru_int[0]    /* flags */
#define svr4_ifr_metric	    ifr_ifru.ifru_int[1]    /* metric */
#define svr4_ifr_mtu	    ifr_ifru.ifru_int[1]    /* mtu */
#define svr4_ifr_ifindex    ifr_ifru.ifru_int[1]    /* ifindex */
#define svr4_ifr_nif	    ifr_ifru.ifru_int[1]    /* # of interfaces */
#define svr4_ifr_naddr	    ifr_ifru.ifru_int[1]    /* # of addresses */
#define svr4_ifr_type	    ifr_ifru.ifru_int[1]    /* type of interface */
#define svr4_ifr_debug	    ifr_ifru.ifru_int[1]    /* debug level */
#define svr4_ifr_muxid	    ifr_ifru.ifru_int[1]    /* multiplexor id */
#define svr4_ifr_data	    ifr_ifru.ifru_data	    /* for use by interface */
#define svr4_ifr_enaddr	    ifr_ifru.ifru_enaddr    /* ethernet address */
#define svr4_ifr_onepacket  ifr_ifru.ifru_onepacket /* one-packet mode params */
#define svr4_ifr_perf	    ifr_ifru.ifru_perf	    /* tuning parameters */
};
#endif

/* socket.c */
extern int abi_do_setsockopt(unsigned long *sp);
extern int abi_do_getsockopt(unsigned long *sp);

/* socksys.c */
/*
 * The original Linux socket file operations.
 * We use it for two thing:
 *  o initializing the socksys socket file operations
 *  o to make calls to the original poll and release routines
 *    from our implementations.
 */
extern struct file_operations socket_file_ops;

#endif /* _ABI_SVR4_SOCKIO_H */
