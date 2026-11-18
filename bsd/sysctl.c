// SPDX-License-Identifier: GPL-2.0
/*
 * abi_bsd - the sysctl(2) protocol for personalities with a fixed MIB
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/random.h>

#include <abiemu/bsd.h>

int abi_sysctl_str(void *buf, const char *s)
{
	return strscpy(buf, s, ABI_SYSCTL_BUFSZ) + 1;
}
EXPORT_SYMBOL_GPL(abi_sysctl_str);

int abi_sysctl_int(void *buf, int v)
{
	*(int *)buf = v;
	return sizeof(int);
}
EXPORT_SYMBOL_GPL(abi_sysctl_int);

int abi_sysctl_quad(void *buf, u64 v)
{
	*(u64 *)buf = v;
	return sizeof(u64);
}
EXPORT_SYMBOL_GPL(abi_sysctl_quad);

int abi_sysctl_random(void *buf, size_t want)
{
	want = min_t(size_t, want, ABI_SYSCTL_BUFSZ);
	get_random_bytes(buf, want);
	return want;
}
EXPORT_SYMBOL_GPL(abi_sysctl_random);

/*
 * sysctl(const int *name, u_int namelen, void *old, size_t *oldlenp,
 *	  const void *new, size_t newlen)
 *
 * @value fills a buffer of ABI_SYSCTL_BUFSZ bytes and returns the
 * length of the value, or -ENOENT.  Nothing can be written.
 */
long abi_bsd_sysctl(struct abi_ctx *ctx, abi_sysctl_fn value)
{
	void __user *old = (void __user *)ctx->args[2];
	size_t __user *oldlenp = (void __user *)ctx->args[3];
	unsigned int len = ctx->args[1];
	int mib[ABI_SYSCTL_MAXNAME];
	size_t want = ABI_SYSCTL_BUFSZ, room;
	char *buf;
	long ret;

	if (len < 1 || len > ABI_SYSCTL_MAXNAME)
		return -EINVAL;
	if (copy_from_user(mib, (void __user *)ctx->args[0], len * sizeof(int)))
		return -EFAULT;
	if (old && oldlenp && get_user(want, oldlenp))
		return -EFAULT;

	buf = kzalloc(ABI_SYSCTL_BUFSZ, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	ret = value(mib, len, buf, want);
	if (ret < 0) {
		if (abi_trace)
			pr_info("abiemu: %s[%d:%s]: unknown sysctl %d.%d.%d (len %u)\n",
				ctx->pers->name, task_pid_nr(current),
				current->comm, mib[0], len > 1 ? mib[1] : 0,
				len > 2 ? mib[2] : 0, len);
		goto out;
	}
	if (ctx->args[4]) {
		ret = -EPERM;
		goto out;
	}
	if (!oldlenp) {
		ret = 0;
	} else if (!old) {
		ret = put_user(ret, oldlenp) ? -EFAULT : 0;
	} else if (get_user(room, oldlenp) ||
		   copy_to_user(old, buf, min_t(size_t, room, ret)) ||
		   put_user(min_t(size_t, room, ret), oldlenp)) {
		ret = -EFAULT;
	} else {
		ret = room < ret ? -ENOMEM : 0;
	}
out:
	kfree(buf);
	return ret;
}
EXPORT_SYMBOL_GPL(abi_bsd_sysctl);
