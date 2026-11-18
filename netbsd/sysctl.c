// SPDX-License-Identifier: GPL-2.0
/*
 * abi_netbsd - sysctl(3)
 */
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>
#include <linux/random.h>
#include <linux/cpumask.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include <linux/sched/mm.h>

#include "netbsd.h"

#define NB_CTL_MAXNAME	12
#define BUFSZ		256

static int nb_str(void *b, const char *s) { return strscpy(b, s, BUFSZ) + 1; }
static int nb_int(void *b, int v) { *(int *)b = v; return 4; }
static int nb_quad(void *b, u64 v) { *(u64 *)b = v; return 8; }

static int netbsd_sysctl_value(const int *mib, unsigned int len, void *buf,
			       size_t want)
{
	if (len != 2)
		return -ENOENT;
	switch (mib[0] << 16 | mib[1]) {
	case 1 << 16 | 1:	return nb_str(buf, "NetBSD");
	case 1 << 16 | 2:	return nb_str(buf, NETBSD_OSRELEASE);
	case 1 << 16 | 3:	return nb_int(buf, NETBSD_OSREV);
	case 1 << 16 | 4:
		return scnprintf(buf, BUFSZ, "NetBSD " NETBSD_OSRELEASE
				 " (abiemu on Linux %s)\n",
				 utsname()->release) + 1;
	case 1 << 16 | 7:	return nb_int(buf, rlimit(RLIMIT_NOFILE));
	case 1 << 16 | 8:	return nb_int(buf, 512 * 1024);
	case 1 << 16 | 10:	return nb_str(buf, utsname()->nodename);
	case 1 << 16 | 22:	return nb_str(buf, utsname()->domainname);
	case 1 << 16 | 18:	return nb_int(buf, NGROUPS_MAX);
	case 1 << 16 | 38:	return nb_int(buf, UIO_MAXIOV);
	case 1 << 16 | 61:	/* kern.urandom */
		get_random_bytes(buf, 4);
		return 4;
	case 1 << 16 | 81:	/* kern.arandom */
		want = min_t(size_t, want, BUFSZ);
		get_random_bytes(buf, want);
		return want;
	case 6 << 16 | 1:
	case 6 << 16 | 10:	return nb_str(buf, "amd64");
	case 6 << 16 | 2:	return nb_str(buf, "Linux abiemu virtual CPU");
	case 6 << 16 | 3:
	case 6 << 16 | 16:	return nb_int(buf, num_online_cpus());
	case 6 << 16 | 4:	return nb_int(buf, 1234);
	case 6 << 16 | 5:
	case 6 << 16 | 6:	return nb_int(buf, min_t(u64, INT_MAX,
						 totalram_pages() << PAGE_SHIFT));
	case 6 << 16 | 13:
	case 6 << 16 | 14:	return nb_quad(buf, totalram_pages() << PAGE_SHIFT);
	case 6 << 16 | 7:	return nb_int(buf, PAGE_SIZE);
	case 8 << 16 | 1:	return nb_str(buf, "/usr/bin:/bin:/usr/sbin:/sbin");
	}
	return -ENOENT;
}

/* __sysctl(const int *name, u_int namelen, void *old, size_t *oldlenp,
 *	    const void *new, size_t newlen) */
long netbsd_sys_sysctl(struct abi_ctx *ctx)
{
	void __user *old = (void __user *)ctx->args[2];
	size_t __user *oldlenp = (void __user *)ctx->args[3];
	unsigned int len = ctx->args[1];
	int mib[NB_CTL_MAXNAME];
	size_t want = BUFSZ, room;
	char *buf;
	long ret;

	if (len < 1 || len > NB_CTL_MAXNAME)
		return -EINVAL;
	if (copy_from_user(mib, (void __user *)ctx->args[0], len * sizeof(int)))
		return -EFAULT;
	if (ctx->args[4])
		return -EPERM;
	if (old && oldlenp && get_user(want, oldlenp))
		return -EFAULT;

	buf = kzalloc(BUFSZ, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	ret = netbsd_sysctl_value(mib, len, buf, want);
	if (ret < 0) {
		if (abi_trace)
			pr_info("abiemu: netbsd[%d:%s]: unknown sysctl %d.%d.%d (len %u)\n",
				task_pid_nr(current), current->comm, mib[0],
				len > 1 ? mib[1] : 0, len > 2 ? mib[2] : 0, len);
		goto out;
	}
	if (!oldlenp)
		goto out_ok;
	if (!old) {
		ret = put_user(ret, oldlenp) ? -EFAULT : 0;
		goto out;
	}
	if (get_user(room, oldlenp))
		goto fault;
	if (copy_to_user(old, buf, min_t(size_t, room, ret)) ||
	    put_user(min_t(size_t, room, ret), oldlenp))
		goto fault;
	ret = room < ret ? -ENOMEM : 0;
	goto out;
out_ok:
	ret = 0;
	goto out;
fault:
	ret = -EFAULT;
out:
	kfree(buf);
	return ret;
}
