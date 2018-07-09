/*
 * Copyright (c) 1994,1996 Mike Jagdis.
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
 *
 */

#ident "%W% %G%"

/*
 * BSD-style socket support for Wyse/V386.
 */
#include <linux/net.h>
#include <linux/personality.h>
#include <linux/syscalls.h>
#include <linux/utsname.h>
#include <linux/signal.h>
#include <linux/wait.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <asm/uaccess.h>

#include <abi/util/map.h>
#include <abi/util/socket.h>
#include <abi/wyse/sysent.h>


int wyse_gethostname(char *name, int len)
{
	int error;
	char *p;

	down_read(&uts_sem);
	error = verify_area(VERIFY_WRITE, name, len);
	if (!error) {
		--len;
		for (p = system_utsname.nodename; *p && len; p++,len--) {
			__put_user(*p, name);
			name++;
		}
		__put_user('\0', name);
	}
	up_read(&uts_sem);
	return error;
}

int wyse_getdomainname(char *name, int len)
{
	int error;
	char *p;

	down_read(&uts_sem);
	error = verify_area(VERIFY_WRITE, name, len);
	if (!error) {
		--len;
		for (p = system_utsname.domainname; *p && len; p++,len--) {
			__put_user(*p, name);
			name++;
		}
		__put_user('\0', name);
	}
	up_read(&uts_sem);
	return error;
}

int wyse_wait3(int *loc)
{
	pid_t pid;
	int res;

	pid = sys_wait4(-1, loc, WNOHANG, 0);
	if (!loc)
		return pid;

	get_user(res, (unsigned long *)loc);

	if ((res & 0xff) == 0x7f) {
		int sig = (res >> 8) & 0xff;

		sig = current_thread_info()->exec_domain->signal_map[sig];
		res = (res & (~0xff00)) | (sig << 8);
		put_user(res, (unsigned long *)loc);
	} else if (res && res == (res & 0xff)) {
		res = current_thread_info()->exec_domain->signal_map[res & 0x7f];
		put_user(res, (unsigned long *)loc);
	}

	return pid;
}

int wyse_socket(int family, int type, int protocol)
{
	family = map_value(current_thread_info()->exec_domain->af_map, family, 0);
	type = map_value(current_thread_info()->exec_domain->socktype_map, family, 0);

	return sys_socket(family, type, protocol);
}

int wyse_setsockopt(int fd, int level, int optname, char *optval, int optlen)
{
	switch (level) {
	case 0: /* IPPROTO_IP aka SOL_IP */
		if (--optname == 0)
			optname = 4;
		if (optname > 4) {
			optname += 24;
			if (optname <= 33)
				optname--;
			if (optname < 32 || optname > 36)
				return -EINVAL;
			break;
		}
	case 0xffff:
		level = SOL_SOCKET;
		optname = map_value(current_thread_info()->exec_domain->sockopt_map, optname, 0);
		switch (optname) {
		case SO_LINGER:
			/*
			 * SO_LINGER takes a struct linger as the argument
			 * but some code uses an int and expects to get
			 * away without an error. Sigh...
			 */
			if (optlen == sizeof(int))
				return 0;
			break;
		/*
		 * The following are not currently implemented under Linux
		 * so we must fake them in reasonable ways.
		 * (Only SO_PROTOTYPE is documented in SCO's man page).
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

		/*
		 * The following are not currenty implemented under Linux
		 * and probably aren't settable anyway.
		 */
		case SO_IMASOCKET:
			return -ENOPROTOOPT;
		default:
			break;
		}
	default:
		/*
		 * XXX We assume everything else uses the same level and
		 * XXX option numbers.  This is true for IPPROTO_TCP(/SOL_TCP)
		 * XXX and TCP_NDELAY but is known to be incorrect for other
		 * XXX potential options :-(.
		 */
		break;
	}

	return sys_setsockopt(fd, level, optname, optval, optlen);
}

int wyse_getsockopt(int fd, int level, int optname, char *optval, int *optlen)
{
	unsigned int len;
	int val;

	if (get_user(len,optlen))
		return -EFAULT;
	if (len < 0)
		return -EINVAL;

	switch (level) {
	case 0: /* IPPROTO_IP aka SOL_IP */
		if (--optname == 0)
			optname = 4;
		if (optname > 4) {
			optname += 24;
			if (optname <= 33)
				optname--;
			if (optname < 32 || optname > 36)
				return -EINVAL;
			break;
		}
	case 0xffff:
		level = SOL_SOCKET;
		optname = map_value(current_thread_info()->exec_domain->sockopt_map, optname, 0);
		switch (optname) {
		case SO_LINGER:
			/*
			 * SO_LINGER takes a struct linger as the argument
			 * but some code uses an int and expects to get
			 * away without an error. Sigh...
			 */
			if (len != sizeof(int))
				goto native;

			val = 0;
			break;
		/*
		 * The following are not currently implemented under Linux
		 * so we must fake them in reasonable ways.
		 * (Only SO_PROTOTYPE is documented in SCO's man page).
		 */
		case SO_PROTOTYPE:
			val = 0;
			break;

		case SO_ORDREL:
		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
			return -ENOPROTOOPT;

		case SO_USELOOPBACK:
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
			return 0;

		/*
		 * The following are not currenty implemented under Linux
		 * and probably aren't settable anyway.
		 */
		case SO_IMASOCKET:
			val = 1;
			break;
		default:
			goto native;
		}

		if (len > sizeof(int))
			len = sizeof(int);
		if (copy_to_user(optval, &val, len))
			return -EFAULT;
		if (put_user(len, optlen))
			 return -EFAULT;
		return 0;

	default:
		/*
		 * XXX We assume everything else uses the same level and
		 * XXX option numbers.  This is true for IPPROTO_TCP(/SOL_TCP)
		 * XXX and TCP_NDELAY but is known to be incorrect for other
		 * XXX potential options :-(.
		 */
		break;
	}

native:
	return sys_getsockopt(fd, level, optname, optval, optlen);
}

int wyse_recvfrom(int fd, void *buff, size_t size, unsigned flags,
		  struct sockaddr *addr, int *addr_len)
{
	int error;

	error = sys_recvfrom(fd, buff, size, flags, addr, addr_len);
	if (error == -EAGAIN)
		error = -EWOULDBLOCK;
	return error;
}

int wyse_recv(int fd, void *buff, size_t size, unsigned flags)
{
	int error;

	error = sys_recvfrom(fd, buff, size, flags, NULL, NULL);
	if (error == -EAGAIN)
		error = -EWOULDBLOCK;
	return error;
}

int wyse_sendto(int fd, void *buff, size_t len, unsigned flags,
		struct sockaddr *addr, int addr_len)
{
	int error;

	error = sys_sendto(fd, buff, len, flags, addr, addr_len);
	if (error == -EAGAIN)
		error = -EWOULDBLOCK;
	return error;
}

int wyse_send(int fd, void *buff, size_t len, unsigned flags)
{
	int error;

	error = sys_sendto(fd, buff, len, flags, NULL, 0);
	if (error == -EAGAIN)
		error = -EWOULDBLOCK;
	return error;
}
