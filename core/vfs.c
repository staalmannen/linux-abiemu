// SPDX-License-Identifier: GPL-2.0
/*
 * abiemu core - VFS helpers shared by the personalities
 *
 * These implement the bits of stat() and getdents() that do not depend
 * on a foreign structure layout, using exported VFS interfaces only.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/namei.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/mman.h>

#include <abiemu/abiemu.h>
#include "core.h"

int abi_stat_fd(int fd, struct kstat *st)
{
	struct file *file = fget_raw(fd);
	int err;

	if (!file)
		return -EBADF;
	err = vfs_getattr(&file->f_path, st, STATX_BASIC_STATS | STATX_BTIME,
			  AT_STATX_SYNC_AS_STAT);
	fput(file);
	return err;
}
EXPORT_SYMBOL_GPL(abi_stat_fd);

int abi_stat_path(int dfd, const char __user *upath, bool follow,
		  bool empty_ok, struct kstat *st)
{
	unsigned int flags = (follow ? LOOKUP_FOLLOW : 0) |
			     (empty_ok ? LOOKUP_EMPTY : 0);
	struct path path;
	int err;

	err = user_path_at(dfd, upath, flags, &path);
	if (err)
		return err;
	err = vfs_getattr(&path, st, STATX_BASIC_STATS | STATX_BTIME,
			  AT_STATX_SYNC_AS_STAT);
	path_put(&path);
	return err;
}
EXPORT_SYMBOL_GPL(abi_stat_path);

struct abi_dirbuf {
	struct dir_context	ctx;
	abi_dirent_fn		emit;
	char			*buf;
	size_t			size;
	size_t			used;
	bool			full;
};

static bool abi_filldir(struct dir_context *dc, const char *name, int namlen,
			loff_t offset, u64 ino, unsigned int d_type)
{
	struct abi_dirbuf *db = container_of(dc, struct abi_dirbuf, ctx);
	size_t n;

	/*
	 * @offset is the position of this entry.  The foreign formats
	 * want the position of the next one, which we do not know yet;
	 * ctx.pos is the best approximation and is what ends up in the
	 * file position anyway.
	 */
	n = db->emit(db->buf + db->used, db->size - db->used, name, namlen,
		     ino, d_type, offset + 1);
	if (!n) {
		db->full = true;
		return false;
	}
	db->used += n;
	return true;
}

long abi_getdents(int fd, void __user *ubuf, size_t count, loff_t *basep,
		  abi_dirent_fn emit)
{
	struct abi_dirbuf db = {
		.ctx.actor = abi_filldir,
		.emit = emit,
	};
	struct file *file;
	long err;

	file = fget(fd);
	if (!file)
		return -EBADF;
	count = min_t(size_t, count, 64 * 1024);
	db.size = count;
	db.buf = kvzalloc(count, GFP_KERNEL);
	if (!db.buf) {
		fput(file);
		return -ENOMEM;
	}

	mutex_lock(&file->f_pos_lock);
	if (basep)
		*basep = file->f_pos;
	err = iterate_dir(file, &db.ctx);
	mutex_unlock(&file->f_pos_lock);

	if (db.used) {
		err = db.used;
		if (copy_to_user(ubuf, db.buf, db.used))
			err = -EFAULT;
	} else if (!err && db.full) {
		err = -EINVAL;		/* buffer too small for one entry */
	}
	kvfree(db.buf);
	fput(file);
	return err;
}
EXPORT_SYMBOL_GPL(abi_getdents);

unsigned long abi_map_anon(unsigned long addr, unsigned long len,
			   unsigned long prot, unsigned long flags)
{
	return vm_mmap(NULL, addr, len, prot, MAP_PRIVATE | MAP_ANONYMOUS | flags, 0);
}
EXPORT_SYMBOL_GPL(abi_map_anon);
