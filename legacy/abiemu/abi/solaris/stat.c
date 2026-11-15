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
 * aint32_t with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ident "%W% %G%"

/*
 * Solaris 2 stat64 & friends support.
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/string.h>
#include <asm/uaccess.h>

#include <abi/solaris/types.h>
#include <abi/solaris/stat.h>

#include <abi/util/trace.h>
#include <abi/util/stat.h>


int
report_sol_stat64(struct kstat *stp, struct sol_stat64 *bufp)
{
	struct sol_stat64 buf;

	memset(&buf, 0, sizeof(struct sol_stat64));


	buf.st_dev	= linux_to_sol_dev_t(stp->dev);
	buf.st_ino	= linux_to_sol_ino_t(stp->ino);
	buf.st_mode	= stp->mode;
	buf.st_nlink	= stp->nlink;
	buf.st_uid	= linux_to_sol_uid_t(stp->uid);
	buf.st_gid	= linux_to_sol_gid_t(stp->gid);
	buf.st_rdev	= linux_to_sol_dev_t(stp->rdev);
	buf.st_size	= stp->size;

	buf.st_atime	= stp->atime.tv_sec;
	buf.st_mtime	= stp->mtime.tv_sec;
	buf.st_ctime	= stp->ctime.tv_sec;

	buf.st_blksize	= stp->blksize;
	buf.st_blocks	= stp->blocks;

	strcpy(buf.st_fstype, "ext2");

	if (copy_to_user(bufp, &buf, sizeof(struct sol_stat64)))
		return -EFAULT;
	return 0;
}

int
sol_stat64(char *filename, struct sol_stat64 *bufp)
{
	struct kstat st;
	int error;

	error = vfs_stat(filename, &st);
	if (!error)
		error = report_sol_stat64(&st, bufp);
	return error;
}

int
sol_lstat64(char *filename, struct sol_stat64 *bufp)
{
	struct kstat st;
	int error;

	error = vfs_lstat(filename, &st);
	if (!error)
		error = report_sol_stat64(&st, bufp);
	return error;
}

int
sol_fstat64(int fd, struct sol_stat64 *bufp)
{
	struct kstat st;
	int error;

	error = vfs_fstat(fd, &st);
	if (!error)
		error = report_sol_stat64(&st, bufp);
	return error;
}
