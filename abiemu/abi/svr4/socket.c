/*
 * Copyright (c) 1994,1996 Mike Jagdis (jaggy@purplet.demon.co.uk)
 */

#ident "%W% %G%"

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/net.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/socket.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <asm/uaccess.h>
#include <asm/ioctls.h>

#include <abi/util/map.h>
#include <abi/util/trace.h>
#include <abi/util/socket.h>


int
abi_do_setsockopt(unsigned long *sp)
{
	int error;
	int level, optname;

	error = verify_area(VERIFY_READ,
			((unsigned long *)sp),
			5*sizeof(long));
	if (error)
		return error;

	get_user(level, ((unsigned long *)sp)+1);
	get_user(optname, ((unsigned long *)sp)+2);

#if defined(CONFIG_ABI_TRACE)
	if (abi_traced(ABI_TRACE_STREAMS|ABI_TRACE_SOCKSYS)) {
		u_long optval, optlen;

		get_user(optval, ((u_long *)sp) + 3);
		get_user(optlen, ((u_long *)sp) + 4);
		__abi_trace("setsockopt level=%d, optname=%d, "
				"optval=0x%08lx, optlen=0x%08lx\n",
				level, optname, optval, optlen);
	}
#endif

	switch (level) {
		case 0: /* IPPROTO_IP aka SOL_IP */
			/* This is correct for the SCO family. Hopefully
			 * it is correct for other SYSV...
			 */
			optname--;
			if (optname == 0)
				optname = 4;
			if (optname > 4) {
				optname += 24;
				if (optname <= 33)
					optname--;
				if (optname < 32 || optname > 36)
					return -EINVAL;
			}
			put_user(optname, ((unsigned long *)sp)+2);
			break;

		case 0xffff:
			put_user(SOL_SOCKET, ((unsigned long *)sp)+1);
			optname = map_value(current_thread_info()->exec_domain->sockopt_map, optname, 0);
			put_user(optname, ((unsigned long *)sp)+2);

			switch (optname) {
				case SO_LINGER: {
					unsigned long optlen;

					/* SO_LINGER takes a struct linger
					 * as the argument but some code
					 * uses an int and expects to get
					 * away without an error. Sigh...
					 */
					get_user(optlen, ((unsigned long *)sp)+4);
					if (optlen == sizeof(int))
						return 0;
					break;
				}

				/* The following are not currently implemented
				 * under Linux so we must fake them in
				 * reasonable ways. (Only SO_PROTOTYPE is
				 * documented in SCO's man page).
				 */
				case SO_PROTOTYPE:
				case SO_ORDREL:
				case SO_SNDTIMEO:
				case SO_RCVTIMEO:
					return -ENOPROTOOPT;

				case SO_USELOOPBACK:
				case SO_SNDLOWAT:
				case SO_RCVLOWAT:
					return 0;

				/* The following are not currenty implemented
				 * under Linux and probably aren't settable
				 * anyway.
				 */
				case SO_IMASOCKET:
					return -ENOPROTOOPT;
			}

		default:
			/* FIXME: We assume everything else uses the
			 * same level and option numbers. This is true
			 * for IPPROTO_TCP(/SOL_TCP) and TCP_NDELAY
			 * but is known to be incorrect for other
			 * potential options :-(.
			 */
			break;
	}

	return sys_socketcall(SYS_SETSOCKOPT, sp);
}

int
abi_do_getsockopt(unsigned long *sp)
{
	int error;
	int level, optname;
	char *optval;
	long *optlen;
	unsigned long *ulptr;

	error = verify_area(VERIFY_READ,
			((unsigned long *)sp),
			5*sizeof(long));
	if (error)
		return error;

	ulptr = (unsigned long *)&level;
	get_user(*ulptr, ((unsigned long *)sp)+1);
	ulptr = (unsigned long *)&optname;
	get_user(*ulptr, ((unsigned long *)sp)+2);
	ulptr = (unsigned long *)&optval;
	get_user(*ulptr, ((unsigned long *)sp)+3);
	ulptr = (unsigned long *)&optlen;
	get_user(*ulptr, ((unsigned long *)sp)+4);

#if defined(CONFIG_ABI_TRACE)
	if (abi_traced(ABI_TRACE_STREAMS|ABI_TRACE_SOCKSYS)) {
		long l;

		get_user(l, optlen);
		__abi_trace("getsockopt level=%d, optname=%d, optval=0x%08lx, "
				"optlen=0x%08lx[%ld]\n", level, optname,
				(u_long)optval, (u_long)optlen, l);
	}
#endif

	switch (level) {
		case 0: /* IPPROTO_IP aka SOL_IP */
			/* This is correct for the SCO family. Hopefully
			 * it is correct for other SYSV...
			 */
			optname--;
			if (optname == 0)
				optname = 4;
			if (optname > 4) {
				optname += 24;
				if (optname <= 33)
					optname--;
				if (optname < 32 || optname > 36)
					return -EINVAL;
			}
			put_user(optname, ((unsigned long *)sp)+2);
			break;

		case 0xffff:
			put_user(SOL_SOCKET, ((unsigned long *)sp)+1);
			optname = map_value(current_thread_info()->exec_domain->sockopt_map, optname, 0);
			put_user(optname, ((unsigned long *)sp)+2);

			switch (optname) {
				case SO_LINGER: {
					long l;

					/* SO_LINGER takes a struct linger
					 * as the argument but some code
					 * uses an int and expects to get
					 * away without an error. Sigh...
					 */
					get_user(l, optlen);
					if (l == sizeof(int)) {
						put_user(0, (long *)optval);
						return 0;
					}
					break;
				}

				/* The following are not currently implemented
				 * under Linux so we must fake them in
				 * reasonable ways. (Only SO_PROTOTYPE is
				 * documented in SCO's man page).
				 */
				case SO_PROTOTYPE: {
					unsigned long len;
					error = get_user(len, optlen);
					if (error)
						return error;
					if (len < sizeof(long))
						return -EINVAL;

					error = verify_area(VERIFY_WRITE,
							(char *)optval,
							sizeof(long));
					if (!error) {
						put_user(0, (long *)optval);
						put_user(sizeof(long),
							optlen);
					}
					return error;
				}

				case SO_ORDREL:
				case SO_SNDTIMEO:
				case SO_RCVTIMEO:
					return -ENOPROTOOPT;

				case SO_USELOOPBACK:
				case SO_SNDLOWAT:
				case SO_RCVLOWAT:
				case SO_IMASOCKET: {
					unsigned long len;
					error = get_user(len, optlen);
					if (error)
						return error;
					if (len < sizeof(long))
						return -EINVAL;

					error = verify_area(VERIFY_WRITE,
							(char *)optval,
							sizeof(long));
					if (!error) {
						put_user(1, (long *)optval);
						put_user(sizeof(long),
							optlen);
					}
					return error;
				}
			}

		default:
			/* FIXME: We assume everything else uses the
			 * same level and option numbers. This is true
			 * for IPPROTO_TCP(/SOL_TCP) and TCP_NDELAY
			 * but is known to be incorrect for other
			 * potential options :-(.
			 */
			break;
	}

	return sys_socketcall(SYS_GETSOCKOPT, sp);
}
