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

#ident "%W% %G%"

/*
 * SVR4 stat & friends support.
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/string.h>
#include <linux/module.h>
#include <asm/uaccess.h>

#include <abi/svr4/types.h>
#include <abi/svr4/stat.h>

#include <abi/util/stat.h>
#include <abi/util/trace.h>


enum {SVR4_stat = 1, SVR4_xstat = 2};

int
report_svr4_stat(struct kstat *stp, struct svr4_stat *bufp)
{
	struct svr4_stat buf;

	memset(&buf, 0, sizeof(struct svr4_stat));


	buf.st_dev	= linux_to_svr4_o_dev_t(stp->dev);
	buf.st_ino	= linux_to_svr4_o_ino_t(stp->ino);
	buf.st_mode	= stp->mode;
	buf.st_nlink	= stp->nlink;
	buf.st_uid	= linux_to_svr4_o_uid_t(stp->uid);
	buf.st_gid	= linux_to_svr4_o_gid_t(stp->gid);
	buf.st_rdev	= linux_to_svr4_o_dev_t(stp->rdev);

	if (stp->size > MAX_NON_LFS)
		return -EOVERFLOW;	/* XXX: what to return for SVR4?? */

	buf.st_size	= stp->size;

	buf.st_atime	= stp->atime.tv_sec;
	buf.st_mtime	= stp->mtime.tv_sec;
	buf.st_ctime 	= stp->ctime.tv_sec;

	if (copy_to_user(bufp, &buf, sizeof(struct svr4_stat)))
		return -EFAULT;
	return 0;
}

int
report_svr4_xstat(struct kstat *stp, struct svr4_xstat *bufp)
{
	struct svr4_xstat buf;

	memset(&buf, 0, sizeof(struct svr4_xstat));


	buf.st_dev	= linux_to_svr4_dev_t(stp->dev);
	buf.st_ino	= linux_to_svr4_ino_t(stp->ino);
	buf.st_mode	= stp->mode;
	buf.st_nlink	= stp->nlink;
	buf.st_uid	= linux_to_svr4_uid_t(stp->uid);
	buf.st_gid	= linux_to_svr4_gid_t(stp->gid);
	buf.st_rdev	= linux_to_svr4_dev_t(stp->rdev);

	if (stp->size > MAX_NON_LFS)
		return -EOVERFLOW;	/* XXX: what to return for SVR4?? */

	buf.st_size	= stp->size;

	buf.st_atim.tv_sec = stp->atime.tv_sec;
	buf.st_atim.tv_usec = stp->atime.tv_nsec / 1000;
	buf.st_mtim.tv_sec = stp->mtime.tv_sec;
	buf.st_mtim.tv_usec = stp->mtime.tv_nsec / 1000;
	buf.st_ctim.tv_sec = stp->ctime.tv_sec;
	buf.st_ctim.tv_usec = stp->ctime.tv_nsec / 1000;

	buf.st_blksize	= stp->blksize;
	buf.st_blocks	= stp->blocks;

	if (copy_to_user(bufp, &buf, sizeof(struct svr4_xstat)))
		return -EFAULT;
	return 0;
}

int
svr4_stat(char *filename, struct svr4_stat *bufp)
{
	struct kstat st;
	int error;

	error = vfs_stat(filename, &st);
	if (!error)
		error = report_svr4_stat(&st, bufp);
	return error;
}

int
svr4_lstat(char *filename, struct svr4_stat *bufp)
{
	struct kstat st;
	int error;

	error = vfs_lstat(filename, &st);
	if (!error)
		error = report_svr4_stat(&st, bufp);
	return error;
}

int
svr4_fstat(int fd, struct svr4_stat *bufp)
{
	struct kstat st;
	int error;

	error = vfs_fstat(fd, &st);
	if (!error)
		error = report_svr4_stat(&st, bufp);
	return error;
}

int
svr4_xstat(int vers, char *filename, void *bufp)
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
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "xstat version %d not supported\n", vers);
#endif
	return -EINVAL;
}

int
svr4_lxstat(int vers, char *filename, void *bufp)
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
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "lxstat version %d not supported\n", vers);
#endif
	return -EINVAL;
}

int
svr4_fxstat(int vers, int fd, void *bufp)
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
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "fxstat version %d not supported\n", vers);
#endif
	return -EINVAL;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(report_svr4_stat);
EXPORT_SYMBOL(report_svr4_xstat);
EXPORT_SYMBOL(svr4_fstat);
EXPORT_SYMBOL(svr4_fxstat);
EXPORT_SYMBOL(svr4_lstat);
EXPORT_SYMBOL(svr4_lxstat);
EXPORT_SYMBOL(svr4_stat);
EXPORT_SYMBOL(svr4_xstat);
#endif
