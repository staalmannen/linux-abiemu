/*
 * Mostly ripped from Al Viro's stat-a-AC9-10 patch, 2001 Christoph Hellwig.
 */

#ident "%W% %G%"

#include <linux/module.h>		/* needed to shut up modprobe */
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/namei.h>

#include <abi/util/revalidate.h>
#include <abi/util/stat.h>


MODULE_DESCRIPTION("Linux-ABI helper routines");
MODULE_AUTHOR("Christoph Hellwig, ripped from kernel sources/patches");
MODULE_LICENSE("GPL");

#if 0 /* LINUXABI_OBSOLETE */
int getattr_full(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;
	stat->dev = inode->i_rdev;
	stat->ino = inode->i_ino;
	stat->mode = inode->i_mode;
	stat->nlink = inode->i_nlink;
	stat->uid = inode->i_uid;
	stat->gid = inode->i_gid;
	stat->rdev = inode->i_rdev;
	stat->atime = inode->i_atime;
	stat->mtime = inode->i_mtime;
	stat->ctime = inode->i_ctime;
	stat->size = inode->i_size;
	stat->blocks = inode->i_blocks;
	stat->blksize = inode->i_blksize;
	return 0;
}

/*
 * Use minix fs values for the number of direct and indirect blocks.  The
 * count is now exact for the minix fs except that it counts zero blocks.
 * Everything is in units of BLOCK_SIZE until the assignment to
 * stat->blksize.
 */
int getattr_minix(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;
	unsigned int blocks, indirect;

	stat->dev = inode->i_rdev;
	stat->ino = inode->i_ino;
	stat->mode = inode->i_mode;
	stat->nlink = inode->i_nlink;
	stat->uid = inode->i_uid;
	stat->gid = inode->i_gid;
	stat->rdev = inode->i_rdev;
	stat->atime = inode->i_atime;
	stat->mtime = inode->i_mtime;
	stat->ctime = inode->i_ctime;
	stat->size = inode->i_size;
#define D_B	7
#define I_B	(BLOCK_SIZE / sizeof(unsigned short))

	blocks = (stat->size + BLOCK_SIZE - 1) >> BLOCK_SIZE_BITS;
	if (blocks > D_B) {
		indirect = (blocks - D_B + I_B - 1) / I_B;
		blocks += indirect;
		if (indirect > 1) {
			indirect = (indirect - 1 + I_B - 1) / I_B;
			blocks += indirect;
			if (indirect > 1)
				blocks++;
		}
	}
	stat->blocks = (BLOCK_SIZE / 512) * blocks;
	stat->blksize = BLOCK_SIZE;
	return 0;
}

int vfs_stat(char *filename, struct kstat *stat)
{
	struct nameidata nd;
	int error;

	error = user_path_walk(filename, &nd);
	if (error)
		return error;

	error = do_revalidate(nd.dentry);
	if (!error) {
		struct inode *inode = nd.dentry->d_inode;
		if (!inode->i_blksize)
			error = getattr_minix(nd.mnt, nd.dentry, stat);
		else
			error = getattr_full(nd.mnt, nd.dentry, stat);
		path_release(&nd);
	}

	return error;
}

int vfs_lstat(char *filename, struct kstat *stat)
{
	struct nameidata nd;
	int error;

	error = user_path_walk_link(filename, &nd);
	if (error)
		return error;

	error = do_revalidate(nd.dentry);
	if (!error) {
		struct inode *inode = nd.dentry->d_inode;
		if (!inode->i_blksize)
			error = getattr_minix(nd.mnt, nd.dentry, stat);
		else
			error = getattr_full(nd.mnt, nd.dentry, stat);
		path_release(&nd);
	}

	return error;
}

int vfs_fstat(int fd, struct kstat *stat)
{
	struct file *file = fget(fd);
	int error;

	if (!file)
		return -EBADF;

	error = do_revalidate(file->f_dentry);
	if (!error) {
		struct inode *inode = file->f_dentry->d_inode;
		if (!inode->i_blksize)
			error = getattr_minix(file->f_vfsmnt, file->f_dentry, stat);
		else
			error = getattr_full(file->f_vfsmnt, file->f_dentry, stat);
		fput(file);
	}
	return error;
}
#endif
