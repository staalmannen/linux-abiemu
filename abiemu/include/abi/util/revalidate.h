/*
 * This is straight from linux/fs/stat.c.
 */
#ifndef _ABI_UTIL_REVALIDATE_H
#define _ABI_UTIL_REVALIDATE_H

#ident "%W% %G%"

#include <linux/fs.h>

/* LINUXABI_TODO */
/*
 * This is required for proper NFS attribute caching (so it says there).
 * Maybe the kernel should export it - but it is basically simple...
 */
static __inline int
do_revalidate(struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
        struct iattr attr;
        int error;

        attr.ia_valid = 0; /* setup an empty flag for the inode attribute change */

        down(&inode->i_sem);
        error = notify_change(dentry, &attr);
        up(&inode->i_sem);
	return error;
}

#endif /* _ABI_UTIL_REVALIDATE_H */
