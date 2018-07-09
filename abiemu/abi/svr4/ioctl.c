/*
 * Copyright (C) 1991, 1992  Linus Torvalds
 *
 * Written by Drew Sullivan.
 * Rewritten by Mike Jagdis.
 */

#ident "%W% %G%"

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <asm/abi_machdep.h>
#include <abi/svr4/ioctl.h>


/*
 * __svr4_ioctl() is a meta mapper, that is
 * it looks up the class of the ioctl and then
 * dispatchs to lower level routines to handle the
 * mapping of the actual ioctls
 */
int
__svr4_ioctl(struct pt_regs *regs, int fd, unsigned long ioctl_num, void *arg)
{
	unsigned int class = ioctl_num >> 8;
	char class_str[4];

	switch (class) {

	/*
	 * SCO ioctls on the pseudo NFS device probably.
	 */
	case 0:
		return abi_ioctl_socksys(fd, ioctl_num, arg);

	/*
	 * SCO console keyboard stuff?
	 */
	case 'A':
		return -EINVAL;

	case 't':
		return svr4_tape_ioctl(fd, ioctl_num, arg);

	case 'f':
		return svr4_fil_ioctl(fd, ioctl_num, arg);

	/*
	 * Xenix ioctl compatibility.
	 */
	case 'T':
		return svr4_term_ioctl(fd, ioctl_num & 0xFF, arg);

	case 'C':
	case 'c':
		return svr4_console_ioctl(fd, ioctl_num, arg);

	case ('i' << 16) | ('C' << 8):	/* iBCS2 POSIX */
		return svr4_video_ioctl(fd, ioctl_num & 0xFF, arg);

	/*
	 * SCO 3.2.2 uses ('X'<<8)|1 for access to the video map
	 * and the 'X' set is also used for synchronous comm
	 * lines (I think?). SVR4 uses it for tty extensions to
	 * support hardware flow control and external clocks.
	 */
	case 'X':
		return svr4_termiox_ioctl(fd, ioctl_num & 0xFF, arg);

	/*
	 * These aren't implemented and are never likely to be as they
	 * are specific to drivers for obscure hardware. (For those
	 * that don't know they're the JERQ ioctls. Says it all
	 * really!)
	 */
	case 'j':
		return -EINVAL;

	/*
	 * The 'S' set could also be display mode switch
	 * ioctls in a SCO 3.2.x x<4 environment. It should
	 * depend on the descriptor they are applied to.
	 * According to ISC the Xenix STREAMS ioctls had the
	 * high bit set on the command to differentiate them
	 * from mode switch ioctls. Yuk, yuk, yuk...
	 */
	case 'S':
		return svr4_stream_ioctl(regs, fd, ioctl_num & 0x7F, arg);

	/*
	 * These are STREAMS socket module ioctls.
	 */
	case 'I':
#if defined(CONFIG_ABI_XTI)
		return svr4_sockmod_ioctl(fd, ioctl_num & 0xFF, arg);
#else
		return -EINVAL;
#endif

	/*
	 * EUC ioctls. These are something to do with chararcter
	 * code set conversions in SVR4. If we don't support
	 * them the correct thing to do is to return EINVAL.
	 */
	case 'E' | 0x80:
		return -EINVAL;

	/*
	 * SCO channel mapping. I can't find any documentation
	 * for this. These are the LD?MAP ioctls defined in
	 * sys/termio.h and sys/emap.h. They are used by mapchan.
	 */
	case 'D':
		return -EINVAL;
	}

	/*
	 * If we haven't handled it yet it must be a BSD style ioctl
	 * with a (possible) argument description in the high word of
	 * the opcode.
	 */
	switch (class & 0xff) {

	/*
	 * From SVR4 as specified in sys/iocomm.h.
	 */
	case 'f':
		return svr4_fil_ioctl(fd, ioctl_num, arg);

	/*
	 * BSD or V7 terminal ioctls.
	 */
	case 't':
		return bsd_ioctl_termios(fd, ioctl_num, arg);

	/*
	 * "Traditional" BSD and Wyse V/386 3.2.1A TCP/IP ioctls.
	 */
	case 's':
	case 'r':
	case 'i':
		return abi_ioctl_socksys(fd, ioctl_num, arg);

	/*
	 * SVR3 streams based socket TCP/IP ioctls.
	 *
	 * These are handed over to the standard ioctl
	 * handler since /dev/socksys is an emulated device
	 * and front ends any sockets created through it.
	 * Note that 'S' ioctls without the BSDish argument
	 * type in the high bytes are STREAMS ioctls and 'I'
	 * ioctls without the BSDish type in the high bytes
	 * are the STREAMS socket module ioctls. (see above).
	 */
	case 'S':
	case 'R':
	case 'I':
		return abi_ioctl_socksys(fd, ioctl_num, arg);
	}

	/*
	 * If nothing has handled it yet someone may have to do some
	 * more work...
	 */
	class_str[0] = class & 0xFF0000 ? (char)((class >> 16) & 0xFF) : '.';
	class_str[1] = class & 0x00FF00 ? (char)((class >>  8) & 0xFF) : '.';
	class_str[2] = class & 0x0000FF ? (char)((class      ) & 0xFF) : '.';
	class_str[3] = 0;

	printk(KERN_DEBUG "svr4: ioctl(%d, %lx[%s], 0x%p) unsupported\n",
		fd, ioctl_num, class_str, arg);

	return -EINVAL;
}

int
svr4_ioctl(struct pt_regs * regs)
{
	u_int			num;
	int			fd;
	caddr_t			data;

	fd = (int)get_syscall_parameter(regs, 0);
	num = (u_int)get_syscall_parameter(regs, 1);
	data = (caddr_t)get_syscall_parameter(regs, 2);

	return __svr4_ioctl(regs, fd, num, data);
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(__svr4_ioctl);
EXPORT_SYMBOL(svr4_ioctl);
#endif
