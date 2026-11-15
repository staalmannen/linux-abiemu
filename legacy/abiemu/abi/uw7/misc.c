/*
 *   abi/uw7/misc.c - various UW7 system calls.
 *
 *  This software is under GPL
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>


int uw7_sleep(int seconds)
{
	struct timespec t;
	mm_segment_t old_fs;
	int error;

	t.tv_sec = seconds;
	t.tv_nsec = 0;
	old_fs = get_fs();
	set_fs(get_ds());
	error = sys_nanosleep(&t, NULL);
	set_fs(old_fs);
	return error;
}

#define UW7_MAXUID      60002

int uw7_seteuid(int uid)
{
	if (uid < 0 || uid > UW7_MAXUID)
		return -EINVAL;
	return sys_setreuid16(-1, uid);
}

int uw7_setegid(int gid)
{
	if (gid < 0 || gid > UW7_MAXUID)
		return -EINVAL;
	return sys_setreuid16(-1, gid);
}

/* can't call sys_pread64() directly because off is 32bit on UW7 */
int uw7_pread(unsigned int fd, char * buf, int count, long off)
{
	return sys_pread64(fd, buf, count, (loff_t)off);
}

/* can't call sys_pwrite64() directly because off is 32bit on UW7 */
int uw7_pwrite(unsigned int fd, char * buf, int count, long off)
{
	return sys_pwrite64(fd, buf, count, (loff_t)off);
}

int uw7_stty(int fd, int cmd)
{
	return -EIO;
}

int uw7_gtty(int fd, int cmd)
{
	return -EIO;
}
