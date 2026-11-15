
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/socket.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <abi/svr4/sigset.h>


int sol_llseek(struct pt_regs * regs)
{
	unsigned int fd;
	unsigned long offset_high, offset_low;
	unsigned origin;
	long long res;
	unsigned int rvalue;
	mm_segment_t old_fs;
	struct inode *inode;
	struct file *file;
	get_user(fd, ((unsigned int *)regs->esp)+1);
	get_user(offset_low, ((unsigned long *)regs->esp)+2);
	get_user(offset_high, ((unsigned long *)regs->esp)+3);
	get_user(origin, ((unsigned int *)regs->esp)+4);

	old_fs = get_fs();
	set_fs(get_ds());
	rvalue = sys_llseek(fd,offset_high,offset_low,&res,origin);
	set_fs(old_fs);

	if ( rvalue < -ENOIOCTLCMD) {
		regs->edx = (res >> 32);
		rvalue = (res & 0xffffffff);
	}
	else if (rvalue == -ESPIPE) {
		/* Solaris allows you to seek on a pipe */
		file = fget(fd);
		if (file) {
			inode = file->f_dentry->d_inode;
			if (inode && (S_ISCHR(inode->i_mode)
				      || S_ISBLK(inode->i_mode))) {
				rvalue = 0;
				regs->edx = 0;
			}
			fput(file);
		}
	}

	return rvalue;
}

int sol_memcntl(unsigned addr, unsigned len, int cmd, unsigned arg,
		 int attr, int mask)
{
	//	printk("ibcs_memcntl being ignored\n");
	return 0;
}


enum {
	GETACL = 1,
	SETACL = 2,
	GETACLCNT = 3
};

int sol_acl(char *pathp, int cmd, int nentries, void *aclbufp)
{
	switch (cmd) {
	case GETACLCNT:
		return 0;

	case GETACL:
		return -EIO;

	case SETACL:
		return -EPERM;

	default:
		return -EINVAL;
	}
}
