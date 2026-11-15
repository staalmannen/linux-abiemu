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
 * Misc SCO syscalls.
 */
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <abi/svr4/sysent.h>



int
sco_lseek(int fd, u_long offset, int whence)
{
	int			error;

	error = sys_lseek(fd, offset, whence);
	if (error == -ESPIPE) {
		struct file	*fp = fget(fd);
		struct inode	*ip;

		if (fp == NULL)
			goto out;
		ip = fp->f_dentry->d_inode;
		if (ip && (S_ISCHR(ip->i_mode) || S_ISBLK(ip->i_mode)))
			error = 0;
		fput(fp);
	}
out:
	return (error);
}

int
sco_fcntl(int fd, u_int cmd, u_long arg)
{
	/*
	 * This could be SCO's get highest fd open if the fd we
	 * are called on is -1 otherwise it could be F_CHKFL.
	 */
	if (fd == -1 && cmd == 8) {
		struct files_struct *files = current->files;
		struct fdtable *fdt;
		int rval;

                /* compare to ./fs/open.c: get_unused_fd */
		spin_lock(&files->file_lock);
		fdt = files_fdtable(files);
		rval = find_first_zero_bit(fdt->open_fds->fds_bits, fdt->max_fdset);
		spin_unlock(&files->file_lock);

		return rval;
	}

	return svr4_fcntl(fd, cmd, arg);
}

int
sco_sysi86(int cmd, void *arg1, int arg2)
{
	switch (cmd) {
	case 114 /*SI86GETFEATURES*/ :
		/*
		 * No, I don't know what these feature flags actually
		 * _mean_. This vector just matches SCO OS 5.0.0.
		 */
#define OSR5_FEATURES	"\001\001\001\001\001\001\001\001\002\001\001\001"
		arg2 = max(arg2, 12);
		if (copy_to_user(arg1, OSR5_FEATURES, arg2))
			return -EFAULT;
		return arg2;
	default:
		return svr4_sysi86(cmd, arg1, arg2);
	}
}
