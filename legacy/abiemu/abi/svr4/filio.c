/*
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

#ident "%W% %G%"

/*
 * SVR4 file ioctls.
 */

#include <linux/sched.h>
#include <linux/file.h>
#include <linux/sockios.h>
#include <linux/syscalls.h>
#include <linux/module.h>

#include <asm/uaccess.h>
#include <asm/ioctls.h>

#include <abi/ioctl.h>


int
svr4_fil_ioctl(int fd, u_int cmd, caddr_t data)
{

       struct fdtable *fdt;
       fdt = files_fdtable(current->files);

	switch (cmd) {
	/* FIOCLEX */
	case BSD__IOV('f', 1):
	case BSD__IO('f', 1):
		FD_SET(fd, fdt->close_on_exec);
		return 0;

	/* FIONCLEX */
	case BSD__IOV('f', 2):
	case BSD__IO('f', 2):
		FD_CLR(fd, fdt->close_on_exec);
		return 0;

	case BSD__IOV('f', 3):
	case BSD__IO('f', 3): {
		int		error, nbytes;
		mm_segment_t	fs;

		fs = get_fs();
		set_fs(get_ds());
		error = sys_ioctl(fd, FIONREAD, (long)&nbytes);
		set_fs(fs);

		return (error <= 0 ? error : nbytes);
	}

	/* FGETOWN */
	case BSD__IOW('f', 123, int):
		return sys_ioctl(fd, FIOGETOWN, (long)data);

	/* FSETOWN */
	case BSD__IOW('f', 124, int):
		return sys_ioctl(fd, FIOSETOWN, (long)data);

	/* FIOASYNC */
	case BSD__IOW('f', 125, int):
		return sys_ioctl(fd, FIOASYNC, (long)data);

	/* FIONBIO */
	case BSD__IOW('f', 126, int):
		return sys_ioctl(fd, FIONBIO, (long)data);

	/* FIONREAD */
	case BSD__IOR('f', 127, int):
		return sys_ioctl(fd, FIONREAD, (long)data);
	}

	printk(KERN_ERR "%s: file ioctl 0x%08x unsupported\n", __FUNCTION__, cmd);
	return -EINVAL;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_fil_ioctl);
#endif
