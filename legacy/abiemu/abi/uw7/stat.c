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
 * UnixWare 7.x xstat support.
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/string.h>
#include <asm/uaccess.h>

#include <abi/uw7/types.h>
#include <abi/uw7/stat.h>
#include <abi/svr4/stat.h>

#include <abi/util/trace.h>
#include <abi/util/stat.h>


enum {SVR4_stat = 1, SVR4_xstat = 2, UW7_stat64 = 4};

int
report_uw7_stat64(struct kstat *stp, struct uw7_stat64 *bufp)
{
	struct uw7_stat64 buf;

	memset(&buf, 0, sizeof(struct uw7_stat64));

	buf.st_dev	= linux_to_uw7_dev_t(stp->dev);
	buf.st_ino	= linux_to_uw7_ino_t(stp->ino);
	buf.st_mode	= stp->mode;
	buf.st_nlink	= stp->nlink;
	buf.st_uid	= linux_to_uw7_uid_t(stp->uid);
	buf.st_gid	= linux_to_uw7_gid_t(stp->gid);
	buf.st_rdev	= linux_to_uw7_dev_t(stp->rdev);
	buf.st_size	= stp->size;

	buf.st_atime.tv_sec = stp->atime.tv_sec;
	buf.st_mtime.tv_sec = stp->mtime.tv_sec;
	buf.st_ctime.tv_sec = stp->ctime.tv_sec;

	buf.st_blksize	= stp->blksize;
	buf.st_blocks	= stp->blocks;

	strcpy(buf.st_fstype, "ext2");

	if (copy_to_user(bufp, &buf, sizeof(struct uw7_stat64)))
		return -EFAULT;
	return 0;
}

int
uw7_xstat(int vers, char *filename, void *bufp)
{
	struct kstat st;
	int error;

	error = vfs_stat(filename, &st);
	if (error)
		return error;

	switch (vers) {
	case SVR4_stat:
		return report_svr4_stat(&st, bufp);
	case SVR4_xstat:
		return report_svr4_xstat(&st, bufp);
	case UW7_stat64:
		return report_uw7_stat64(&st, bufp);
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "xstat version %d not supported\n", vers);
#endif
	return -EINVAL;
}

int
uw7_lxstat(int vers, char *filename, void *bufp)
{
	struct kstat st;
	int error;

	error = vfs_lstat(filename, &st);
	if (error)
		return error;

	switch (vers) {
	case SVR4_stat:
		return report_svr4_stat(&st, bufp);
	case SVR4_xstat:
		return report_svr4_xstat(&st, bufp);
	case UW7_stat64:
		return report_uw7_stat64(&st, bufp);
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "lxstat version %d not supported\n", vers);
#endif
	return -EINVAL;
}

int
uw7_fxstat(int vers, int fd, void *bufp)
{
	struct kstat st;
	int error;

	error = vfs_fstat(fd, &st);
	if (error)
		return error;

	switch (vers) {
	case SVR4_stat:
		return report_svr4_stat(&st, bufp);
	case SVR4_xstat:
		return report_svr4_xstat(&st, bufp);
	case UW7_stat64:
		return report_uw7_stat64(&st, bufp);
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "fxstat version %d not supported\n", vers);
#endif
	return -EINVAL;
}
