// SPDX-License-Identifier: GPL-2.0
/*
 * abi_netbsd - sysctl(3)
 */
#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/random.h>
#include <linux/cpumask.h>
#include <linux/mm.h>
#include <linux/sched/signal.h>

#include "netbsd.h"

static int netbsd_sysctl_value(const int *mib, unsigned int len, void *buf,
			       size_t want)
{
	u64 mem = totalram_pages() << PAGE_SHIFT;

	if (len != 2)
		return -ENOENT;
	switch (mib[0] << 16 | mib[1]) {
	case 1 << 16 | 1:	return abi_sysctl_str(buf, "NetBSD");
	case 1 << 16 | 2:	return abi_sysctl_str(buf, NETBSD_OSRELEASE);
	case 1 << 16 | 3:	return abi_sysctl_int(buf, NETBSD_OSREV);
	case 1 << 16 | 4:
		return scnprintf(buf, ABI_SYSCTL_BUFSZ, "NetBSD " NETBSD_OSRELEASE
				 " (abiemu on Linux %s)\n",
				 utsname()->release) + 1;
	case 1 << 16 | 7:	return abi_sysctl_int(buf, rlimit(RLIMIT_NOFILE));
	case 1 << 16 | 8:	return abi_sysctl_int(buf, 512 * 1024);
	case 1 << 16 | 10:	return abi_sysctl_str(buf, utsname()->nodename);
	case 1 << 16 | 18:	return abi_sysctl_int(buf, NGROUPS_MAX);
	case 1 << 16 | 22:	return abi_sysctl_str(buf, utsname()->domainname);
	case 1 << 16 | 38:	return abi_sysctl_int(buf, UIO_MAXIOV);
	case 1 << 16 | 61:	return abi_sysctl_random(buf, 4);	/* urandom */
	case 1 << 16 | 81:	return abi_sysctl_random(buf, want);	/* arandom */
	case 6 << 16 | 1:
	case 6 << 16 | 10:	return abi_sysctl_str(buf, "amd64");
	case 6 << 16 | 2:	return abi_sysctl_str(buf, "Linux abiemu virtual CPU");
	case 6 << 16 | 3:
	case 6 << 16 | 16:	return abi_sysctl_int(buf, num_online_cpus());
	case 6 << 16 | 4:	return abi_sysctl_int(buf, 1234);
	case 6 << 16 | 5:
	case 6 << 16 | 6:	return abi_sysctl_int(buf, min_t(u64, INT_MAX, mem));
	case 6 << 16 | 13:
	case 6 << 16 | 14:	return abi_sysctl_quad(buf, mem);
	case 6 << 16 | 7:	return abi_sysctl_int(buf, PAGE_SIZE);
	case 8 << 16 | 1:	return abi_sysctl_str(buf, "/usr/bin:/bin:/usr/sbin:/sbin");
	}
	return -ENOENT;
}

long netbsd_sys_sysctl(struct abi_ctx *ctx)
{
	return abi_bsd_sysctl(ctx, netbsd_sysctl_value);
}
