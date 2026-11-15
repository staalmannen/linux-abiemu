/*
 *   abi/uw7/mac.c - mldmode(2) and friends.
 *
 *  This software is under GPL
 */

#include <linux/sched.h>
#include <linux/errno.h>

#undef DEBUG

#ifdef DEBUG
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif

#define UW7_MLD_REAL	1
#define UW7_MLD_VIRT	0
#define UW7_MLD_QUERY	2

int uw7_mldmode(int mldmode)
{
	switch (mldmode) {
		case UW7_MLD_REAL:
			DBG(KERN_ERR "UW7[%d]: mldmode(MLD_REAL)\n", current->pid);
			break;

		case UW7_MLD_VIRT:
			DBG(KERN_ERR "UW7[%d]: mldmode(MLD_VIRT)\n", current->pid);
			break;

		case UW7_MLD_QUERY:
			DBG(KERN_ERR "UW7[%d]: mldmode(MLD_QUERY)\n", current->pid);
			return UW7_MLD_REAL;

		default:
			return -EINVAL;
	}
	return 0;
}
