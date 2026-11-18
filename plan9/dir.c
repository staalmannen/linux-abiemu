// SPDX-License-Identifier: GPL-2.0
/*
 * abi_plan9 - stat, wstat and directory reads
 *
 * Plan 9 describes files with machine independent "Dir" records, both
 * for stat() and as the contents of a directory:
 *
 *	size[2] type[2] dev[4] qid.type[1] qid.vers[4] qid.path[8]
 *	mode[4] atime[4] mtime[4] length[8] name[s] uid[s] gid[s] muid[s]
 *
 * (all little endian, strings are a 2 byte length plus bytes).  Linux
 * user and group ids are presented as decimal strings.
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/namei.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/mount.h>
#include <linux/dcache.h>
#include <linux/uaccess.h>
#include <linux/cred.h>
#include <asm/unistd.h>

#include "plan9.h"

#define STATFIXLEN	49
#define P9_STATMAX	65535

static u8 *put16(u8 *p, u16 v) { p[0] = v; p[1] = v >> 8; return p + 2; }
static u8 *put32(u8 *p, u32 v) { put16(p, v); put16(p + 2, v >> 16); return p + 4; }
static u8 *put64(u8 *p, u64 v) { put32(p, v); put32(p + 4, v >> 32); return p + 8; }
static u8 *putstr(u8 *p, const char *s)
{
	size_t n = strlen(s);

	p = put16(p, n);
	memcpy(p, s, n);
	return p + n;
}

static u16 get16(const u8 *p) { return p[0] | (p[1] << 8); }
static u32 get32(const u8 *p) { return get16(p) | ((u32)get16(p + 2) << 16); }
static u64 get64(const u8 *p) { return get32(p) | ((u64)get32(p + 4) << 32); }

static u32 p9_mode(umode_t m)
{
	u32 mode = m & 0777;

	if (S_ISDIR(m))
		mode |= P9_DMDIR;
	return mode;
}

/* Encode one Dir; returns its size, or 0 if it does not fit. */
static size_t p9_convD2M(const struct kstat *k, const char *name, u8 *buf,
			 size_t room)
{
	char uid[12], gid[12];
	u32 mode = p9_mode(k->mode);
	size_t n;
	u8 *p;

	snprintf(uid, sizeof(uid), "%u",
		 from_kuid_munged(current_user_ns(), k->uid));
	snprintf(gid, sizeof(gid), "%u",
		 from_kgid_munged(current_user_ns(), k->gid));
	n = STATFIXLEN + strlen(name) + 2 * strlen(uid) + strlen(gid);
	if (n > room)
		return 0;
	p = put16(buf, n - 2);
	p = put16(p, 'M');			/* type: a mounted fs */
	p = put32(p, k->dev);
	*p++ = mode >> 24;			/* qid.type */
	p = put32(p, k->mtime.tv_sec);		/* qid.vers */
	p = put64(p, k->ino);			/* qid.path */
	p = put32(p, mode);
	p = put32(p, k->atime.tv_sec);
	p = put32(p, k->mtime.tv_sec);
	p = put64(p, S_ISDIR(k->mode) ? 0 : k->size);
	p = putstr(p, name);
	p = putstr(p, uid);
	p = putstr(p, gid);
	p = putstr(p, uid);			/* muid */
	return n;
}

static const char *p9_basename(const struct path *path)
{
	if (path->dentry == path->mnt->mnt_root && IS_ROOT(path->dentry))
		return "/";
	return path->dentry->d_name.name;
}

static long p9_put_stat(const struct path *path, void __user *ubuf, size_t n)
{
	struct kstat k;
	u8 *buf;
	long ret;

	ret = vfs_getattr(path, &k, STATX_BASIC_STATS, AT_STATX_SYNC_AS_STAT);
	if (ret)
		return ret;
	buf = kmalloc(P9_STATMAX, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	spin_lock(&path->dentry->d_lock);
	ret = p9_convD2M(&k, p9_basename(path), buf, P9_STATMAX);
	spin_unlock(&path->dentry->d_lock);
	if (ret > n) {
		/* too small: just report the size, like the Plan 9 kernel */
		if (n >= 2)
			ret = copy_to_user(ubuf, buf, 2) ? -EFAULT : 2;
		else
			ret = -EINVAL;
	} else if (copy_to_user(ubuf, buf, ret)) {
		ret = -EFAULT;
	}
	kfree(buf);
	return ret;
}

/* stat(char *name, uchar *edir, int nedir) */
long p9_sys_stat(struct abi_ctx *ctx)
{
	struct path path;
	long ret;

	ret = user_path_at(AT_FDCWD, (const char __user *)ctx->args[0],
			   LOOKUP_FOLLOW, &path);
	if (ret)
		return ret;
	ret = p9_put_stat(&path, (void __user *)ctx->args[1], ctx->args[2]);
	path_put(&path);
	return ret;
}

/* fstat(int fd, uchar *edir, int nedir) */
long p9_sys_fstat(struct abi_ctx *ctx)
{
	struct file *file = fget_raw(ctx->args[0]);
	long ret;

	if (!file)
		return -EBADF;
	ret = p9_put_stat(&file->f_path, (void __user *)ctx->args[1],
			  ctx->args[2]);
	fput(file);
	return ret;
}

/*
 * Directory reads: collect names with iterate_dir(), then stat each
 * entry once the directory lock has been dropped.
 */
struct p9_dirent {
	u16	len;
	char	name[];
};

struct p9_dirbuf {
	struct dir_context	ctx;
	char			*buf;
	size_t			used, size;
	size_t			budget;		/* bytes left in the user buffer */
};

static bool p9_filldir(struct dir_context *dc, const char *name, int len,
		       loff_t off, u64 ino, unsigned int type)
{
	struct p9_dirbuf *db = container_of(dc, struct p9_dirbuf, ctx);
	size_t need = sizeof(struct p9_dirent) + len + 1;
	size_t worst = STATFIXLEN + len + 3 * 10;
	struct p9_dirent *d;

	if ((len == 1 && name[0] == '.') ||
	    (len == 2 && name[0] == '.' && name[1] == '.'))
		return true;
	if (worst > db->budget || db->used + need > db->size)
		return false;
	d = (struct p9_dirent *)(db->buf + db->used);
	d->len = len;
	memcpy(d->name, name, len);
	d->name[len] = 0;
	db->used += ALIGN(need, 2);
	db->budget -= worst;
	return true;
}

long p9_dirread(struct abi_ctx *ctx, struct file *file, void __user *ubuf,
		size_t n)
{
	struct p9_dirbuf db = { .ctx.actor = p9_filldir };
	size_t off = 0, out = 0;
	u8 *obuf;
	long err;

	n = min_t(size_t, n, 1 << 20);
	db.size = n;
	db.budget = n;
	db.buf = kvmalloc(db.size, GFP_KERNEL);
	obuf = kvmalloc(n, GFP_KERNEL);
	if (!db.buf || !obuf) {
		err = -ENOMEM;
		goto out;
	}

	mutex_lock(&file->f_pos_lock);
	err = iterate_dir(file, &db.ctx);
	mutex_unlock(&file->f_pos_lock);
	if (err)
		goto out;

	while (off < db.used) {
		struct p9_dirent *d = (struct p9_dirent *)(db.buf + off);
		struct path path;
		struct kstat k;
		size_t len;

		off += ALIGN(sizeof(*d) + d->len + 1, 2);
		/* follow symbolic links; Plan 9 has none */
		if (vfs_path_lookup(file->f_path.dentry, file->f_path.mnt,
				    d->name, LOOKUP_FOLLOW, &path) &&
		    vfs_path_lookup(file->f_path.dentry, file->f_path.mnt,
				    d->name, 0, &path))
			continue;
		err = vfs_getattr(&path, &k, STATX_BASIC_STATS,
				  AT_STATX_SYNC_AS_STAT);
		path_put(&path);
		if (err)
			continue;
		len = p9_convD2M(&k, d->name, obuf + out, n - out);
		if (!len)
			break;		/* cannot happen: budget was reserved */
		out += len;
	}
	err = out;
	if (out && copy_to_user(ubuf, obuf, out))
		err = -EFAULT;
	else if (!out && db.used == 0 && n < STATFIXLEN + 1 + 30)
		err = p9_error(ctx, "read or write too small");
out:
	kvfree(db.buf);
	kvfree(obuf);
	return err;
}

/*
 * wstat: fields set to "don't touch" (~0 or empty) are left alone.
 * Mode, mtime, gid and length are applied here; a new name is applied
 * last, as a native renameat().
 */
struct p9_wstat {
	u32	mode, mtime;
	u64	length;
	char	name[256], gid[32];
};

static int p9_convM2D(const u8 *b, size_t n, struct p9_wstat *w)
{
	const u8 *p, *end = b + n;
	int i;

	if (n < STATFIXLEN || get16(b) + 2 != n)
		return -EINVAL;
	w->mode = get32(b + 2 + 2 + 4 + 13);
	w->mtime = get32(b + 2 + 2 + 4 + 13 + 4 + 4);
	w->length = get64(b + 2 + 2 + 4 + 13 + 4 + 4 + 4);
	p = b + 41;
	for (i = 0; i < 4; i++) {
		size_t l;

		if (p + 2 > end)
			return -EINVAL;
		l = get16(p);
		p += 2;
		if (p + l > end)
			return -EINVAL;
		if (i == 0)
			strscpy(w->name, p, min(l + 1, sizeof(w->name)));
		if (i == 2)
			strscpy(w->gid, p, min(l + 1, sizeof(w->gid)));
		p += l;
	}
	return 0;
}

static int p9_setattr(const struct path *path, struct iattr *ia)
{
	struct inode *inode = d_inode(path->dentry);
	int err;

	err = mnt_want_write(path->mnt);
	if (err)
		return err;
	inode_lock(inode);
	err = notify_change(mnt_idmap(path->mnt), path->dentry, ia, NULL);
	inode_unlock(inode);
	mnt_drop_write(path->mnt);
	return err;
}

static int p9_apply_wstat(const struct path *path, const struct p9_wstat *w)
{
	struct iattr ia;
	int err;

	if (w->mode != ~0U) {
		ia = (struct iattr){ .ia_valid = ATTR_MODE | ATTR_CTIME,
			.ia_mode = (d_inode(path->dentry)->i_mode & S_IFMT) |
				   (w->mode & 0777) };
		err = p9_setattr(path, &ia);
		if (err)
			return err;
	}
	if (w->mtime != ~0U) {
		ia = (struct iattr){ .ia_valid = ATTR_MTIME | ATTR_MTIME_SET |
					 ATTR_CTIME,
			.ia_mtime = { .tv_sec = w->mtime } };
		err = p9_setattr(path, &ia);
		if (err)
			return err;
	}
	if (w->gid[0]) {
		unsigned int gid;

		if (kstrtouint(w->gid, 10, &gid))
			return -EPERM;
		ia = (struct iattr){ .ia_valid = ATTR_GID | ATTR_CTIME,
			.ia_vfsgid = VFSGIDT_INIT(make_kgid(current_user_ns(), gid)) };
		err = p9_setattr(path, &ia);
		if (err)
			return err;
	}
	if (w->length != ~0ULL && !d_is_dir(path->dentry)) {
		err = vfs_truncate(path, w->length);
		if (err)
			return err;
	}
	return 0;
}

/* Rename within the same directory: native renameat(old, new). */
static long p9_rename(struct abi_ctx *ctx, const struct path *path,
		      const char *newname)
{
	char *buf, *old, *slash, *new;
	char __user *uold, *unew;
	long ret = -ENOMEM;

	if (strchr(newname, '/'))
		return p9_error(ctx, "bad character in file name");
	if (!strcmp(newname, p9_basename(path)))
		return 0;
	buf = kmalloc(2 * PATH_MAX, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	old = d_path(path, buf, PATH_MAX);
	if (IS_ERR(old)) {
		ret = PTR_ERR(old);
		goto out;
	}
	new = buf + PATH_MAX;
	slash = strrchr(old, '/');
	snprintf(new, PATH_MAX, "%.*s/%s", (int)(slash - old), old, newname);
	uold = abi_gap_str(ctx, old);
	unew = abi_gap_str(ctx, new);
	ret = -EFAULT;
	if (uold && unew)
		ret = abi_native2(ctx, __NR_rename, (unsigned long)uold,
				  (unsigned long)unew);
out:
	kfree(buf);
	return ret;
}

static long p9_do_wstat(struct abi_ctx *ctx, const struct path *path,
			unsigned long ubuf, size_t n)
{
	struct p9_wstat *w;
	u8 *buf;
	long ret;

	if (n > P9_STATMAX)
		return p9_error(ctx, "read or write too large");
	buf = memdup_user((void __user *)ubuf, n);
	if (IS_ERR(buf))
		return PTR_ERR(buf);
	w = kzalloc(sizeof(*w), GFP_KERNEL);
	ret = -ENOMEM;
	if (!w)
		goto out;
	ret = p9_convM2D(buf, n, w);
	if (ret) {
		ret = p9_error(ctx, "bad stat format");
		goto out;
	}
	ret = p9_apply_wstat(path, w);
	if (!ret && w->name[0])
		ret = p9_rename(ctx, path, w->name);
	if (!ret && ctx->state != ABI_ST_NATIVE)
		ret = n;
out:
	kfree(w);
	kfree(buf);
	return ret;
}

static long p9_wstat_post(struct abi_ctx *ctx, long ret)
{
	return ret ?: ctx->args[2];
}

/* wstat(char *name, uchar *edir, int nedir) */
long p9_sys_wstat(struct abi_ctx *ctx)
{
	struct path path;
	long ret;

	ret = user_path_at(AT_FDCWD, (const char __user *)ctx->args[0],
			   LOOKUP_FOLLOW, &path);
	if (ret)
		return ret;
	ctx->post = p9_wstat_post;
	ret = p9_do_wstat(ctx, &path, ctx->args[1], ctx->args[2]);
	path_put(&path);
	return ret;
}

long p9_sys_fwstat(struct abi_ctx *ctx)
{
	struct file *file = fget_raw(ctx->args[0]);
	long ret;

	if (!file)
		return -EBADF;
	ctx->post = p9_wstat_post;
	ret = p9_do_wstat(ctx, &file->f_path, ctx->args[1], ctx->args[2]);
	fput(file);
	return ret;
}
