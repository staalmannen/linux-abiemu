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
 * SCO OpenServer statvfs/fstatvfs support.
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/string.h>
#include <linux/vfs.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <asm/uaccess.h>

#include <abi/sco/types.h>


static int
report_statvfs(struct vfsmount *mnt, struct inode *ip, struct sco_statvfs *bufp)
{
	struct sco_statvfs buf;
	struct kstatfs s;
	int error;

	error = vfs_statfs(mnt->mnt_sb, &s);
	if (error)
		return error;

	memset(&buf, 0, sizeof(struct sco_statvfs));

	buf.f_bsize	= s.f_bsize;
	buf.f_frsize	= s.f_bsize;
	buf.f_blocks	= s.f_blocks;
	buf.f_bfree	= s.f_bfree;
	buf.f_bavail	= s.f_bavail;
	buf.f_files	= s.f_files;
	buf.f_free	= s.f_ffree;
	buf.f_favail	= s.f_ffree; /* SCO addition in the middle! */
	buf.f_sid	= ip->i_sb->s_dev;

	/*
	 * Get the name of the filesystem.
	 *
	 * Sadly, some code "in the wild" actually checks the name
	 * against a hard coded list to see if it is a "real" fs or not.
	 *
	 * I believe Informix Dynamic Server for SCO is one such.
	 */
	if (strncmp(ip->i_sb->s_type->name, "ext2", 4) == 0)
		strcpy(buf.f_basetype, "HTFS");
	else
		strcpy(buf.f_basetype, ip->i_sb->s_type->name);

	/* Check for a few flags statvfs wants but statfs doesn't have. */
	if (IS_RDONLY(ip))
		buf.f_flag |= 1;
	if (mnt->mnt_flags & MNT_NOSUID)
		buf.f_flag |= 2;

	buf.f_namemax	= s.f_namelen;

	if (copy_to_user(bufp, &buf, sizeof(struct sco_statvfs)))
		return -EFAULT;
	return 0;
}

int
sco_statvfs(char *filename, struct sco_statvfs *bufp)
{
	struct nameidata nd;
	int error;

	error = user_path_walk(filename, &nd);
	if (!error) {
		error = report_statvfs(nd.mnt, nd.dentry->d_inode, bufp);
		path_release(&nd);
	}
	return error;
}

int
sco_fstatvfs(int fd, struct sco_statvfs *bufp)
{
	struct file *fp;
	int error = -EBADF;

	fp = fget(fd);
	if (fp) {
		error = report_statvfs(fp->f_vfsmnt,
				fp->f_dentry->d_inode, bufp);
		fput(fp);
	}
	return error;
}
