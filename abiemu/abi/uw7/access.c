/*
 *      abi/uw7/access.c - support for UnixWare access(2) system call.
 *
 *      We handle the non-POSIX EFF_ONLY_OK/EX_OK flags.
 *      This software is under GPL.
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <asm/uaccess.h>

#include <abi/util/revalidate.h>


#undef DEBUG

#ifdef DEBUG
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif

#define UW7_R_OK	004
#define UW7_W_OK	002
#define UW7_X_OK	001
#define UW7_F_OK	000
#define UW7_EFF_ONLY_OK	010
#define UW7_EX_OK	020

#define UW7_MODE_MSK	(UW7_R_OK|UW7_W_OK|UW7_X_OK|UW7_F_OK|UW7_EFF_ONLY_OK|UW7_EX_OK)

int uw7_access(char * filename, int mode)
{
	struct nameidata nd;
	int error;

	DBG(KERN_ERR "UW7[%d]: access(%p,%o)\n", current->pid, filename, mode);

	if (mode & ~UW7_MODE_MSK)
		return -EINVAL;

	if (mode & UW7_EX_OK) {
		error = user_path_walk(filename, &nd);
		if (!error) {
			error = do_revalidate(nd.dentry);
			if (error) {
				path_release(&nd);
				return -EIO;
			}
			if (!S_ISREG(nd.dentry->d_inode->i_mode)) {
				path_release(&nd);
				return -EACCES;
			}
			path_release(&nd);
		}
		mode &= ~UW7_EX_OK;
		mode |= UW7_X_OK;
	}
	if (mode & UW7_EFF_ONLY_OK) {
		uid_t old_uid = current->uid, old_gid = current->gid;

		current->uid = current->euid;
		current->gid = current->egid;
		mode &= ~UW7_EFF_ONLY_OK;
        	error =  sys_access(filename, mode);
		current->uid = old_uid;
		current->gid = old_gid;
	} else
        	error = sys_access(filename, mode);

	return error;
}
