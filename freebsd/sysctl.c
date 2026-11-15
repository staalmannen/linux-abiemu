// SPDX-License-Identifier: GPL-2.0
/*
 * abi_freebsd - sysctl(3)
 *
 * FreeBSD userland learns almost everything about the system through
 * sysctl: uname, the page size, the number of CPUs, random numbers...
 * Only a small, fixed set of nodes is provided.
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

#include "freebsd.h"

#define CTL_SYSCTL	0
#define CTL_KERN	1
#define CTL_VM		2
#define CTL_HW		6
#define CTL_USER	8
#define FBSD_CTL_MAXNAME	24

#define PSEUDO		0x1000		/* nodes without a fixed number */

enum {
	N_OSTYPE, N_OSRELEASE, N_OSREV, N_VERSION, N_HOSTNAME, N_OSRELDATE,
	N_ARND, N_USRSTACK, N_NGROUPS, N_IOV_MAX, N_CLOCKRATE, N_ARGMAX,
	N_BOOTTIME, N_NCPU, N_IDENT, N_MACHINE, N_MODEL, N_BYTEORDER,
	N_PHYSMEM, N_PAGESIZE, N_PAGESIZES, N_OVERCOMMIT, N_CS_PATH,
	N_MAXFILES, N_ZERO,
};

static const struct freebsd_sysctl_node {
	const char	*name;
	int		mib[2];
	int		what;
} freebsd_sysctl_nodes[] = {
	{ "kern.ostype",	{ CTL_KERN, 1 },	N_OSTYPE },
	{ "kern.osrelease",	{ CTL_KERN, 2 },	N_OSRELEASE },
	{ "kern.osrevision",	{ CTL_KERN, 3 },	N_OSREV },
	{ "kern.version",	{ CTL_KERN, 4 },	N_VERSION },
	{ "kern.maxfiles",	{ CTL_KERN, 7 },	N_MAXFILES },
	{ "kern.argmax",	{ CTL_KERN, 8 },	N_ARGMAX },
	{ "kern.securelevel",	{ CTL_KERN, 9 },	N_ZERO },
	{ "kern.hostname",	{ CTL_KERN, 10 },	N_HOSTNAME },
	{ "kern.clockrate",	{ CTL_KERN, 12 },	N_CLOCKRATE },
	{ "kern.ngroups",	{ CTL_KERN, 18 },	N_NGROUPS },
	{ "kern.boottime",	{ CTL_KERN, 21 },	N_BOOTTIME },
	{ "kern.osreldate",	{ CTL_KERN, 24 },	N_OSRELDATE },
	{ "kern.maxfilesperproc", { CTL_KERN, 27 },	N_MAXFILES },
	{ "kern.usrstack",	{ CTL_KERN, 33 },	N_USRSTACK },
	{ "kern.iov_max",	{ CTL_KERN, 35 },	N_IOV_MAX },
	{ "kern.arandom",	{ CTL_KERN, 37 },	N_ARND },
	{ "kern.ident",		{ CTL_KERN, PSEUDO + 0 }, N_IDENT },
	{ "kern.smp.cpus",	{ CTL_KERN, PSEUDO + 1 }, N_NCPU },
	{ "kern.smp.maxcpus",	{ CTL_KERN, PSEUDO + 2 }, N_NCPU },
	{ "hw.machine",		{ CTL_HW, 1 },		N_MACHINE },
	{ "hw.model",		{ CTL_HW, 2 },		N_MODEL },
	{ "hw.ncpu",		{ CTL_HW, 3 },		N_NCPU },
	{ "hw.byteorder",	{ CTL_HW, 4 },		N_BYTEORDER },
	{ "hw.physmem",		{ CTL_HW, 5 },		N_PHYSMEM },
	{ "hw.usermem",		{ CTL_HW, 6 },		N_PHYSMEM },
	{ "hw.pagesize",	{ CTL_HW, 7 },		N_PAGESIZE },
	{ "hw.machine_arch",	{ CTL_HW, 11 },		N_MACHINE },
	{ "hw.realmem",		{ CTL_HW, 12 },		N_PHYSMEM },
	{ "hw.pagesizes",	{ CTL_HW, PSEUDO + 0 },	N_PAGESIZES },
	{ "vm.overcommit",	{ CTL_VM, PSEUDO + 0 },	N_OVERCOMMIT },
	{ "user.cs_path",	{ CTL_USER, 1 },	N_CS_PATH },
};

#define SYSCTL_BUFSZ	256

static int sc_str(void *buf, const char *s)
{
	return strscpy(buf, s, SYSCTL_BUFSZ) + 1;
}

static int sc_int(void *buf, int v)
{
	*(int *)buf = v;
	return sizeof(int);
}

static int sc_long(void *buf, unsigned long v)
{
	*(unsigned long *)buf = v;
	return sizeof(long);
}

/* Produce the value of a node; @want is how much the caller asked for. */
static int freebsd_sysctl_value(int what, void *buf, size_t want)
{
	switch (what) {
	case N_OSTYPE:		return sc_str(buf, "FreeBSD");
	case N_OSRELEASE:	return sc_str(buf, FREEBSD_OSRELEASE);
	case N_OSREV:		return sc_int(buf, 199506);
	case N_OSRELDATE:	return sc_int(buf, FREEBSD_OSRELDATE);
	case N_IDENT:		return sc_str(buf, "ABIEMU");
	case N_MACHINE:		return sc_str(buf, "amd64");
	case N_MODEL:		return sc_str(buf, "Linux abiemu virtual CPU");
	case N_NCPU:		return sc_int(buf, num_online_cpus());
	case N_BYTEORDER:	return sc_int(buf, 1234);
	case N_PAGESIZE:	return sc_int(buf, PAGE_SIZE);
	case N_PAGESIZES:	return sc_long(buf, PAGE_SIZE);
	case N_PHYSMEM:		return sc_long(buf, totalram_pages() << PAGE_SHIFT);
	case N_NGROUPS:		return sc_int(buf, NGROUPS_MAX);
	case N_IOV_MAX:		return sc_int(buf, UIO_MAXIOV);
	case N_ARGMAX:		return sc_int(buf, 512 * 1024);
	case N_MAXFILES:	return sc_int(buf, rlimit(RLIMIT_NOFILE));
	case N_OVERCOMMIT:
	case N_ZERO:		return sc_int(buf, 0);
	case N_CS_PATH:
		return sc_str(buf, "/usr/bin:/bin:/usr/sbin:/sbin");
	case N_VERSION:
		return scnprintf(buf, SYSCTL_BUFSZ,
				 "FreeBSD " FREEBSD_OSRELEASE " (abiemu on Linux %s)\n",
				 utsname()->release) + 1;
	case N_HOSTNAME:
		return sc_str(buf, utsname()->nodename);
	case N_USRSTACK:
		return sc_long(buf, PAGE_ALIGN(current->mm->start_stack));
	case N_ARND:
		want = min_t(size_t, want, SYSCTL_BUFSZ);
		get_random_bytes(buf, want);
		return want;
	case N_CLOCKRATE: {
		int *ci = buf;		/* struct clockinfo */

		ci[0] = HZ; ci[1] = 1000000 / HZ; ci[2] = 0;
		ci[3] = HZ; ci[4] = HZ;
		return 5 * sizeof(int);
	}
	case N_BOOTTIME: {
		struct __kernel_old_timeval *tv = buf;
		struct timespec64 ts;

		getboottime64(&ts);
		tv->tv_sec = ts.tv_sec;
		tv->tv_usec = ts.tv_nsec / 1000;
		return sizeof(*tv);
	}
	}
	return -ENOENT;
}

static const struct freebsd_sysctl_node *freebsd_sysctl_bymib(const int *mib,
							     unsigned int len)
{
	int i;

	if (len < 2)
		return NULL;
	for (i = 0; i < ARRAY_SIZE(freebsd_sysctl_nodes); i++)
		if (freebsd_sysctl_nodes[i].mib[0] == mib[0] &&
		    freebsd_sysctl_nodes[i].mib[1] == mib[1])
			return &freebsd_sysctl_nodes[i];
	return NULL;
}

static const struct freebsd_sysctl_node *freebsd_sysctl_byname(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(freebsd_sysctl_nodes); i++)
		if (!strcmp(freebsd_sysctl_nodes[i].name, name))
			return &freebsd_sysctl_nodes[i];
	return NULL;
}

/* The usual sysctl "old value" protocol. */
static long freebsd_sysctl_out(void __user *old, size_t __user *oldlenp,
			       const void *val, size_t len)
{
	size_t room;

	if (!oldlenp)
		return 0;
	if (!old)
		return put_user(len, oldlenp) ? -EFAULT : 0;
	if (get_user(room, oldlenp))
		return -EFAULT;
	if (copy_to_user(old, val, min(room, len)) ||
	    put_user(min(room, len), oldlenp))
		return -EFAULT;
	return room < len ? -ENOMEM : 0;
}

static long freebsd_sysctl_node(const struct freebsd_sysctl_node *node,
				void __user *old, size_t __user *oldlenp,
				bool writing)
{
	size_t want = SYSCTL_BUFSZ;
	char *buf;
	long ret;

	if (writing)
		return -EPERM;
	if (old && oldlenp && get_user(want, oldlenp))
		return -EFAULT;
	buf = kzalloc(SYSCTL_BUFSZ, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	ret = freebsd_sysctl_value(node->what, buf, want);
	if (ret >= 0)
		ret = freebsd_sysctl_out(old, oldlenp, buf, ret);
	kfree(buf);
	return ret;
}

static char *freebsd_sysctl_getname(const char __user *uname, size_t len)
{
	char *name;

	if (!len || len > 128)
		return ERR_PTR(-ENAMETOOLONG);
	name = memdup_user_nul(uname, len);
	return name;
}

/* __sysctl(int *name, u_int namelen, void *old, size_t *oldlenp,
 *	    const void *new, size_t newlen) */
long freebsd_sys_sysctl(struct abi_ctx *ctx)
{
	void __user *old = (void __user *)ctx->args[2];
	size_t __user *oldlenp = (void __user *)ctx->args[3];
	const struct freebsd_sysctl_node *node;
	unsigned int len = ctx->args[1];
	int mib[FBSD_CTL_MAXNAME];

	if (len < 1 || len > FBSD_CTL_MAXNAME)
		return -EINVAL;
	if (copy_from_user(mib, (void __user *)ctx->args[0], len * sizeof(int)))
		return -EFAULT;

	/* sysctl.name2oid: translate a name into a MIB vector */
	if (len == 2 && mib[0] == CTL_SYSCTL && mib[1] == 3) {
		char *name = freebsd_sysctl_getname((void __user *)ctx->args[4],
						    ctx->args[5]);

		if (IS_ERR(name))
			return PTR_ERR(name);
		node = freebsd_sysctl_byname(name);
		if (!node && abi_trace)
			pr_info("abiemu: freebsd[%d:%s]: unknown sysctl %s\n",
				task_pid_nr(current), current->comm, name);
		kfree(name);
		if (!node)
			return -ENOENT;
		return freebsd_sysctl_out(old, oldlenp, node->mib,
					  sizeof(node->mib));
	}

	node = freebsd_sysctl_bymib(mib, len);
	if (!node) {
		if (abi_trace)
			pr_info("abiemu: freebsd[%d:%s]: unknown sysctl mib %d.%d (len %u)\n",
				task_pid_nr(current), current->comm, mib[0],
				len > 1 ? mib[1] : -1, len);
		return -ENOENT;
	}
	return freebsd_sysctl_node(node, old, oldlenp, ctx->args[4] != 0);
}

/* __sysctlbyname(const char *name, size_t namelen, void *old,
 *		  size_t *oldlenp, void *new, size_t newlen) */
long freebsd_sys_sysctlbyname(struct abi_ctx *ctx)
{
	const struct freebsd_sysctl_node *node;
	char *name;

	name = freebsd_sysctl_getname((void __user *)ctx->args[0], ctx->args[1]);
	if (IS_ERR(name))
		return PTR_ERR(name);
	node = freebsd_sysctl_byname(name);
	if (!node && abi_trace)
		pr_info("abiemu: freebsd[%d:%s]: unknown sysctl %s\n",
			task_pid_nr(current), current->comm, name);
	kfree(name);
	if (!node)
		return -ENOENT;
	return freebsd_sysctl_node(node, (void __user *)ctx->args[2],
				   (void __user *)ctx->args[3],
				   ctx->args[4] != 0);
}
