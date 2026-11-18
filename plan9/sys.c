// SPDX-License-Identifier: GPL-2.0
/*
 * abi_plan9 - system calls
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/namei.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/mount.h>
#include <linux/dcache.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>
#include <linux/hashtable.h>
#include <linux/wait.h>
#include <linux/sched/signal.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/resource.h>
#include <linux/time.h>
#include <uapi/linux/sched.h>
#include <uapi/linux/mount.h>
#include <asm/unistd.h>

#include "plan9.h"

/*
 * Errors
 */

/* errstr(char *buf, uint n): swap the error string with buf */
static long p9_sys_errstr(struct abi_ctx *ctx)
{
	struct p9_task *t = p9_task();
	char *ubuf = (char __user *)ctx->args[0];
	size_t n = min_t(size_t, ctx->args[1], P9_ERRMAX);
	char new[P9_ERRMAX];
	long len;

	if (!t)
		return -ENOMEM;
	if (!n)
		return 0;
	len = strncpy_from_user(new, ubuf, n);
	if (len < 0)
		return len;
	new[min_t(long, len, n - 1)] = 0;
	if (copy_to_user(ubuf, t->errstr, min(strlen(t->errstr) + 1, n)) ||
	    put_user(0, ubuf + n - 1))
		return -EFAULT;
	strscpy(t->errstr, new, sizeof(t->errstr));
	return 0;
}

static long p9_sys_old_errstr(struct abi_ctx *ctx)
{
	ctx->args[1] = 64;
	return p9_sys_errstr(ctx);
}

/*
 * Files
 */
static unsigned int p9_omode(unsigned long mode)
{
	unsigned int f;

	switch (mode & 3) {
	case P9_OWRITE:	f = O_WRONLY; break;
	case P9_ORDWR:	f = O_RDWR; break;
	default:	f = O_RDONLY; break;
	}
	if (mode & P9_OTRUNC)
		f |= O_TRUNC;
	if (mode & P9_OCEXEC)
		f |= O_CLOEXEC;
	if (mode & P9_OEXCL)
		f |= O_EXCL;
	return f;
}

/* open(char *name, int mode) */
static long p9_sys_open(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_openat, AT_FDCWD, ctx->args[0],
			   p9_omode(ctx->args[1]) | O_LARGEFILE, 0);
}

/* create(char *name, int mode, ulong perm) */
static long p9_sys_create(struct abi_ctx *ctx)
{
	unsigned long perm = ctx->args[2];

	if (perm & P9_DMDIR) {
		struct dentry *dentry;
		struct path path;
		char *name;
		int err;

		name = p9_strdup_user(ctx->args[0], PATH_MAX);
		if (IS_ERR(name))
			return PTR_ERR(name);
		dentry = kern_path_create(AT_FDCWD, name, &path, LOOKUP_DIRECTORY);
		kfree(name);
		if (IS_ERR(dentry))
			return PTR_ERR(dentry);
		dentry = vfs_mkdir(mnt_idmap(path.mnt), d_inode(path.dentry),
				   dentry, (perm & 0777) & ~current_umask());
		err = IS_ERR(dentry) ? PTR_ERR(dentry) : 0;
		done_path_create(&path, dentry);
		if (err)
			return err;
		return abi_native4(ctx, __NR_openat, AT_FDCWD, ctx->args[0],
				   O_RDONLY | O_DIRECTORY |
				   (ctx->args[1] & P9_OCEXEC ? O_CLOEXEC : 0), 0);
	}
	return abi_native4(ctx, __NR_openat, AT_FDCWD, ctx->args[0],
			   p9_omode(ctx->args[1]) | O_CREAT | O_TRUNC |
			   O_LARGEFILE, perm & 0777);
}

/* remove(char *name) */
static long p9_sys_remove(struct abi_ctx *ctx)
{
	struct kstat k;
	int err;

	err = abi_stat_path(AT_FDCWD, (const char __user *)ctx->args[0], false,
			    false, &k);
	if (err)
		return err;
	return abi_native1(ctx, S_ISDIR(k.mode) ? __NR_rmdir : __NR_unlink,
			   ctx->args[0]);
}

static bool p9_isdir(int fd, struct file **fp)
{
	struct file *file = fget(fd);

	*fp = NULL;
	if (!file)
		return false;
	if (S_ISDIR(file_inode(file)->i_mode)) {
		*fp = file;
		return true;
	}
	fput(file);
	return false;
}

/* pread(int fd, void *buf, long n, vlong off); off -1: current offset */
static long p9_sys_pread(struct abi_ctx *ctx)
{
	struct file *dir;
	long ret;

	if (p9_isdir(ctx->args[0], &dir)) {
		ret = p9_dirread(ctx, dir, (void __user *)ctx->args[1],
				 ctx->args[2]);
		fput(dir);
		return ret;
	}
	if ((long)ctx->args[3] == -1)
		return abi_native3(ctx, __NR_read, ctx->args[0], ctx->args[1],
				   ctx->args[2]);
	return abi_native4(ctx, __NR_pread64, ctx->args[0], ctx->args[1],
			   ctx->args[2], ctx->args[3]);
}

static long p9_sys_pwrite(struct abi_ctx *ctx)
{
	if ((long)ctx->args[3] == -1)
		return abi_native3(ctx, __NR_write, ctx->args[0], ctx->args[1],
				   ctx->args[2]);
	return abi_native4(ctx, __NR_pwrite64, ctx->args[0], ctx->args[1],
			   ctx->args[2], ctx->args[3]);
}

static long p9_sys_old_read(struct abi_ctx *ctx)
{
	ctx->args[3] = -1;
	return p9_sys_pread(ctx);
}

static long p9_sys_old_write(struct abi_ctx *ctx)
{
	ctx->args[3] = -1;
	return p9_sys_pwrite(ctx);
}

/* seek(vlong *ret, int fd, vlong n, int type) */
static long p9_seek_post(struct abi_ctx *ctx, long ret)
{
	if (abi_is_error(ret))
		return ret;
	return put_user(ret, (s64 __user *)ctx->args[0]) ? -EFAULT : 0;
}

static long p9_sys_seek(struct abi_ctx *ctx)
{
	struct file *dir;

	if (ctx->args[3] > 2)
		return -EINVAL;
	if (p9_isdir(ctx->args[1], &dir)) {
		fput(dir);
		if (ctx->args[2] || ctx->args[3] != 0)
			return p9_error(ctx, "seek on a directory");
	}
	ctx->post = p9_seek_post;
	return abi_native3(ctx, __NR_lseek, ctx->args[1], ctx->args[2],
			   ctx->args[3]);
}

/* dup(int oldfd, int newfd): newfd -1 picks the lowest free one */
static long p9_sys_dup(struct abi_ctx *ctx)
{
	if ((int)ctx->args[1] == -1)
		return abi_native1(ctx, __NR_dup, ctx->args[0]);
	return abi_native2(ctx, __NR_dup2, ctx->args[0], ctx->args[1]);
}

/* pipe(int fd[2]): Plan 9 pipes are bidirectional */
static long p9_sys_pipe(struct abi_ctx *ctx)
{
	return abi_native4(ctx, __NR_socketpair, AF_UNIX, SOCK_STREAM, 0,
			   ctx->args[0]);
}

/* fd2path(int fd, char *buf, int nbuf) */
static long p9_sys_fd2path(struct abi_ctx *ctx)
{
	struct file *file = fget_raw(ctx->args[0]);
	char *buf, *p;
	long ret;

	if (!file)
		return -EBADF;
	buf = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!buf) {
		fput(file);
		return -ENOMEM;
	}
	p = d_path(&file->f_path, buf, PATH_MAX);
	fput(file);
	if (IS_ERR(p))
		ret = PTR_ERR(p);
	else if (!ctx->args[2])
		ret = 0;
	else {
		size_t n = min_t(size_t, strlen(p) + 1, ctx->args[2]);

		ret = copy_to_user((void __user *)ctx->args[1], p, n) ||
		      put_user(0, (char __user *)ctx->args[1] + n - 1) ?
			-EFAULT : 0;
	}
	kfree(buf);
	return ret;
}

/*
 * Name spaces
 */

/* bind(char *name, char *old, int flag) */
static long p9_sys_bind(struct abi_ctx *ctx)
{
	return abi_native5(ctx, __NR_mount, ctx->args[0], ctx->args[1], 0,
			   MS_BIND | MS_REC, 0);
}

/*
 * mount(int fd, int afd, char *old, int flag, char *aname): attach the
 * 9P server on fd, using the Linux 9P client.
 */
static long p9_sys_mount(struct abi_ctx *ctx)
{
	char *aname = NULL, *opts;
	char __user *uopts, *usrc, *ufs;
	int fd = ctx->args[0];

	if (ctx->args[4]) {
		aname = p9_strdup_user(ctx->args[4], 256);
		if (IS_ERR(aname))
			return PTR_ERR(aname);
	}
	opts = kasprintf(GFP_KERNEL,
			 "trans=fd,rfdno=%d,wfdno=%d,version=9p2000,uname=none,aname=%s,cache=none",
			 fd, fd, aname ?: "");
	kfree(aname);
	if (!opts)
		return -ENOMEM;
	uopts = abi_gap_str(ctx, opts);
	usrc = abi_gap_str(ctx, "#M");
	ufs = abi_gap_str(ctx, "9p");
	kfree(opts);
	if (!uopts || !usrc || !ufs)
		return -EFAULT;
	return abi_native5(ctx, __NR_mount, (unsigned long)usrc, ctx->args[2],
			   (unsigned long)ufs, 0, (unsigned long)uopts);
}

/* unmount(char *name, char *old) */
static long p9_sys_unmount(struct abi_ctx *ctx)
{
	return abi_native2(ctx, __NR_umount2, ctx->args[1], MNT_DETACH);
}

/* fversion(int fd, int bufsize, char *version, int nversion) */
static long p9_sys_fversion(struct abi_ctx *ctx)
{
	static const char v[] = "9P2000";

	if (ctx->args[3] >= sizeof(v) &&
	    copy_to_user((void __user *)ctx->args[2], v, sizeof(v)))
		return -EFAULT;
	return sizeof(v) - 1;
}

static long p9_sys_fauth(struct abi_ctx *ctx)
{
	return p9_error(ctx, "authentication not required");
}

/*
 * Processes
 */
#define RFNAMEG		(1 << 0)
#define RFENVG		(1 << 1)
#define RFFDG		(1 << 2)
#define RFNOTEG		(1 << 3)
#define RFPROC		(1 << 4)
#define RFMEM		(1 << 5)
#define RFNOWAIT	(1 << 6)
#define RFCNAMEG	(1 << 10)
#define RFCENVG		(1 << 11)
#define RFCFDG		(1 << 12)
#define RFREND		(1 << 13)
#define RFNOMNT		(1 << 14)

/* rfork(int flags) */
static long p9_sys_rfork(struct abi_ctx *ctx)
{
	unsigned int flags = ctx->args[0];
	unsigned long cf = 0;

	if (!(flags & RFPROC)) {
		/* change the current process */
		if (flags & (RFNAMEG | RFCNAMEG))
			cf |= CLONE_NEWNS;
		if (flags & RFFDG)
			cf |= CLONE_FILES;
		if (flags & RFCFDG)
			return abi_native3(ctx, __NR_close_range, 0, ~0U, 0);
		if (flags & RFNOTEG && !cf)
			return abi_native2(ctx, __NR_setpgid, 0, 0);
		if (!cf)
			return 0;
		return abi_native1(ctx, __NR_unshare, cf);
	}

	if (flags & RFMEM)
		pr_info_once("abiemu: plan9: rfork(RFMEM) is emulated without sharing memory\n");
	cf = (flags & RFNOWAIT) ? 0 : SIGCHLD;
	if (!(flags & (RFFDG | RFCFDG)))
		cf |= CLONE_FILES;
	if (flags & (RFNAMEG | RFCNAMEG))
		cf |= CLONE_NEWNS;
	return abi_native5(ctx, __NR_clone, cf, 0, 0, 0, 0);
}

/* exec(char *name, char *argv[]): the environment lives in /env */
static long p9_sys_exec(struct abi_ctx *ctx)
{
	return abi_native3(ctx, __NR_execve, ctx->args[0], ctx->args[1], 0);
}

/* exits(char *msg) */
long p9_sys_exits(struct abi_ctx *ctx)
{
	char msg[P9_ERRMAX] = "";
	long n = 0;

	if (ctx->args[0]) {
		n = strncpy_from_user(msg, (char __user *)ctx->args[0],
				      sizeof(msg) - 1);
		if (n < 0)
			n = 0;
		msg[n] = 0;
	}
	if (n)
		p9_exitmsg_put(task_tgid_vnr(current), msg);
	return abi_native1(ctx, __NR_exit_group, n ? 1 : 0);
}

/* Plan 9's %q */
static int p9_quote(char *out, size_t size, const char *s)
{
	size_t i, o = 0;
	bool q = !*s;

	for (i = 0; s[i]; i++)
		if (strchr(" \t\n'=", s[i]))
			q = true;
	if (!q)
		return scnprintf(out, size, "%s", s);
	out[o++] = '\'';
	for (i = 0; s[i] && o + 3 < size; i++) {
		if (s[i] == '\'')
			out[o++] = '\'';
		out[o++] = s[i];
	}
	out[o++] = '\'';
	out[o] = 0;
	return o;
}

/* await(char *buf, int n): "pid utime stime realtime 'msg'" */
static long p9_await_post(struct abi_ctx *ctx, long ret)
{
	struct rusage ru;
	char msg[P9_ERRMAX], tmp[P9_ERRMAX], *out;
	u64 ut, st;
	int status, len;

	if (abi_is_error(ret))
		return ret == -ECHILD ? p9_error(ctx, "no living children") : ret;
	if (get_user(status, (int __user *)ctx->priv[0]) ||
	    copy_from_user(&ru, (void __user *)ctx->priv[1], sizeof(ru)))
		return -EFAULT;

	if (!p9_exitmsg_take(ret, msg)) {
		if ((status & 0x7f) == 0) {
			int code = (status >> 8) & 0xff;

			if (code)
				snprintf(msg, sizeof(msg), "exit %d", code);
			else
				msg[0] = 0;
		} else {
			strscpy(msg, p9_note_name(status & 0x7f, tmp, 0, 0),
				sizeof(msg));
		}
	}
	ut = ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000;
	st = ru.ru_stime.tv_sec * 1000 + ru.ru_stime.tv_usec / 1000;
	out = kmalloc(2 * P9_ERRMAX + 64, GFP_KERNEL);
	if (!out)
		return -ENOMEM;
	len = scnprintf(out, 64, "%ld %llu %llu %llu ", ret, ut, st, ut + st);
	len += p9_quote(out + len, 2 * P9_ERRMAX, msg);
	len = min_t(long, len, ctx->args[1]);
	ret = copy_to_user((void __user *)ctx->args[0], out, len) ? -EFAULT : len;
	kfree(out);
	return ret;
}

static long p9_sys_await(struct abi_ctx *ctx)
{
	void __user *gstatus = abi_gap(ctx, sizeof(int));
	void __user *gru = abi_gap(ctx, sizeof(struct rusage));

	ctx->priv[0] = (unsigned long)gstatus;
	ctx->priv[1] = (unsigned long)gru;
	ctx->post = p9_await_post;
	return abi_native4(ctx, __NR_wait4, -1, (unsigned long)gstatus, 0,
			   (unsigned long)gru);
}

/* brk_(void *addr): 0 or -1 */
static long p9_brk_post(struct abi_ctx *ctx, long ret)
{
	return (unsigned long)ret >= PAGE_ALIGN(ctx->args[0]) ? 0 : -ENOMEM;
}

static long p9_sys_brk(struct abi_ctx *ctx)
{
	ctx->post = p9_brk_post;
	return abi_native1(ctx, __NR_brk, PAGE_ALIGN(ctx->args[0]));
}

/* segbrk(void *saddr, void *addr): only for the bss segment */
static long p9_sys_segbrk(struct abi_ctx *ctx)
{
	ctx->args[0] = ctx->args[1];
	return p9_sys_brk(ctx);
}

static long p9_sys_notsupported(struct abi_ctx *ctx)
{
	return p9_error(ctx, "not implemented");
}

static long p9_sys_zero(struct abi_ctx *ctx)
{
	return 0;
}

/*
 * Time
 */

/* sleep(long ms) */
static long p9_sys_sleep(struct abi_ctx *ctx)
{
	struct __kernel_timespec ts, __user *g;
	long ms = ctx->args[0];

	if (ms <= 0)
		return abi_native0(ctx, __NR_sched_yield);
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * NSEC_PER_MSEC;
	g = abi_gap(ctx, sizeof(ts));
	if (copy_to_user(g, &ts, sizeof(ts)))
		return -EFAULT;
	ctx->flags |= ABI_F_NORESTART;
	return abi_native2(ctx, __NR_nanosleep, (unsigned long)g, 0);
}

/* alarm(ulong ms): returns the time left of the previous alarm */
static long p9_alarm_post(struct abi_ctx *ctx, long ret)
{
	struct __kernel_old_itimerval old;

	if (ret)
		return ret;
	if (copy_from_user(&old, (void __user *)ctx->priv[0], sizeof(old)))
		return -EFAULT;
	return old.it_value.tv_sec * 1000 + old.it_value.tv_usec / 1000;
}

static long p9_sys_alarm(struct abi_ctx *ctx)
{
	struct __kernel_old_itimerval it = { 0 }, __user *g, __user *gold;
	unsigned long ms = ctx->args[0];

	it.it_value.tv_sec = ms / 1000;
	it.it_value.tv_usec = (ms % 1000) * 1000;
	g = abi_gap(ctx, sizeof(it));
	gold = abi_gap(ctx, sizeof(it));
	if (copy_to_user(g, &it, sizeof(it)))
		return -EFAULT;
	ctx->priv[0] = (unsigned long)gold;
	ctx->post = p9_alarm_post;
	return abi_native3(ctx, __NR_setitimer, ITIMER_REAL, (unsigned long)g,
			   (unsigned long)gold);
}

static long p9_sys_nsec(struct abi_ctx *ctx)
{
	return ktime_get_real_ns();
}

/*
 * Synchronisation: rendezvous() and the semaphores, between all Plan 9
 * processes of the system (rendezvous groups are not separated).
 */
struct p9_rv {
	struct hlist_node	node;
	unsigned long		tag;
	unsigned long		val;
	unsigned long		other;
	bool			done;
};

static DEFINE_HASHTABLE(p9_rvs, 6);
static DEFINE_SPINLOCK(p9_rv_lock);
static DECLARE_WAIT_QUEUE_HEAD(p9_rv_wq);

/* rendezvous(void *tag, void *value) */
static long p9_sys_rendezvous(struct abi_ctx *ctx)
{
	unsigned long tag = ctx->args[0], val = ctx->args[1];
	struct p9_rv *r, me = { .tag = tag, .val = val };
	long ret;

	spin_lock(&p9_rv_lock);
	hash_for_each_possible(p9_rvs, r, node, tag) {
		if (r->tag == tag && !r->done) {
			hash_del(&r->node);
			r->other = val;
			r->done = true;
			spin_unlock(&p9_rv_lock);
			wake_up_all(&p9_rv_wq);
			return r->val;
		}
	}
	hash_add(p9_rvs, &me.node, tag);
	spin_unlock(&p9_rv_lock);

	ret = wait_event_interruptible(p9_rv_wq, READ_ONCE(me.done));
	spin_lock(&p9_rv_lock);
	if (!me.done) {
		hash_del(&me.node);
		spin_unlock(&p9_rv_lock);
		return p9_error(ctx, "interrupted");
	}
	spin_unlock(&p9_rv_lock);
	return me.other;
}

static DEFINE_MUTEX(p9_sem_lock);
static DECLARE_WAIT_QUEUE_HEAD(p9_sem_wq);

static int p9_sem_try(long __user *addr)
{
	long v;
	int ret = 0;

	mutex_lock(&p9_sem_lock);
	if (get_user(v, addr))
		ret = -EFAULT;
	else if (v > 0)
		ret = put_user(v - 1, addr) ? -EFAULT : 1;
	mutex_unlock(&p9_sem_lock);
	return ret;
}

/* semacquire(long *addr, int block) / tsemacquire(long *addr, ulong ms) */
static long p9_semacquire(struct abi_ctx *ctx, bool block, long timeout)
{
	long __user *addr = (long __user *)ctx->args[0];
	long ret;

	for (;;) {
		ret = p9_sem_try(addr);
		if (ret)
			return ret;
		if (!block || !timeout)
			return 0;
		/* semrelease() wakes us; poll in case user space bumped it */
		ret = wait_event_interruptible_timeout(p9_sem_wq, false,
						       min(timeout, HZ / 50L));
		if (ret == -ERESTARTSYS)
			return p9_error(ctx, "interrupted");
		if (timeout != MAX_SCHEDULE_TIMEOUT)
			timeout -= min(timeout, HZ / 50L);
	}
}

static long p9_sys_semacquire(struct abi_ctx *ctx)
{
	return p9_semacquire(ctx, ctx->args[1], MAX_SCHEDULE_TIMEOUT);
}

static long p9_sys_tsemacquire(struct abi_ctx *ctx)
{
	return p9_semacquire(ctx, true, msecs_to_jiffies(ctx->args[1]) ?: 1);
}

/* semrelease(long *addr, long count) */
static long p9_sys_semrelease(struct abi_ctx *ctx)
{
	long __user *addr = (long __user *)ctx->args[0];
	long v, ret;

	mutex_lock(&p9_sem_lock);
	ret = get_user(v, addr) ? -EFAULT : 0;
	if (!ret)
		ret = put_user(v + (long)ctx->args[1], addr) ? -EFAULT :
		      v + (long)ctx->args[1];
	mutex_unlock(&p9_sem_lock);
	wake_up_all(&p9_sem_wq);
	return ret;
}

#define S(nr)	[PLAN9_SYS_##nr]

const struct abi_sysent plan9_sysent[] = {
	S(sysr1)	= ABI_EMUL("sysr1", 0, p9_sys_zero),
	S(_errstr)	= ABI_EMUL("_errstr", 1, p9_sys_old_errstr),
	S(bind)		= ABI_EMUL("bind", 3, p9_sys_bind),
	S(chdir)	= ABI_PASS("chdir", 1, __NR_chdir),
	S(close)	= ABI_PASS("close", 1, __NR_close),
	S(dup)		= ABI_EMUL("dup", 2, p9_sys_dup),
	S(alarm)	= ABI_EMUL("alarm", 1, p9_sys_alarm),
	S(exec)		= ABI_EMUL("exec", 2, p9_sys_exec),
	S(exits)	= ABI_EMUL("exits", 1, p9_sys_exits),
	S(fauth)	= ABI_EMUL("fauth", 2, p9_sys_fauth),
	S(segbrk)	= ABI_EMUL("segbrk", 2, p9_sys_segbrk),
	S(open)		= ABI_EMUL("open", 2, p9_sys_open),
	S(_read)	= ABI_EMUL("_read", 3, p9_sys_old_read),
	S(sleep)	= ABI_EMUL("sleep", 1, p9_sys_sleep),
	S(rfork)	= ABI_EMUL("rfork", 1, p9_sys_rfork),
	S(_write)	= ABI_EMUL("_write", 3, p9_sys_old_write),
	S(pipe)		= ABI_EMUL("pipe", 1, p9_sys_pipe),
	S(create)	= ABI_EMUL("create", 3, p9_sys_create),
	S(fd2path)	= ABI_EMUL("fd2path", 3, p9_sys_fd2path),
	S(brk_)		= ABI_EMUL("brk_", 1, p9_sys_brk),
	S(remove)	= ABI_EMUL("remove", 1, p9_sys_remove),
	S(notify)	= ABI_EMUL("notify", 1, p9_sys_notify),
	S(noted)	= ABI_EMUL("noted", 1, p9_sys_noted),
	S(segattach)	= ABI_EMUL("segattach", 4, p9_sys_notsupported),
	S(segdetach)	= ABI_EMUL("segdetach", 1, p9_sys_notsupported),
	S(segfree)	= ABI_EMUL("segfree", 2, p9_sys_zero),
	S(segflush)	= ABI_EMUL("segflush", 2, p9_sys_zero),
	S(rendezvous)	= ABI_EMUL("rendezvous", 2, p9_sys_rendezvous),
	S(unmount)	= ABI_EMUL("unmount", 2, p9_sys_unmount),
	S(semacquire)	= ABI_EMUL("semacquire", 2, p9_sys_semacquire),
	S(semrelease)	= ABI_EMUL("semrelease", 2, p9_sys_semrelease),
	S(seek)		= ABI_EMUL("seek", 4, p9_sys_seek),
	S(fversion)	= ABI_EMUL("fversion", 4, p9_sys_fversion),
	S(errstr)	= ABI_EMUL("errstr", 2, p9_sys_errstr),
	S(stat)		= ABI_EMUL("stat", 3, p9_sys_stat),
	S(fstat)	= ABI_EMUL("fstat", 3, p9_sys_fstat),
	S(wstat)	= ABI_EMUL("wstat", 3, p9_sys_wstat),
	S(fwstat)	= ABI_EMUL("fwstat", 3, p9_sys_fwstat),
	S(mount)	= ABI_EMUL("mount", 5, p9_sys_mount),
	S(await)	= ABI_EMUL("await", 2, p9_sys_await),
	S(pread)	= ABI_EMUL("pread", 4, p9_sys_pread),
	S(pwrite)	= ABI_EMUL("pwrite", 4, p9_sys_pwrite),
	S(tsemacquire)	= ABI_EMUL("tsemacquire", 2, p9_sys_tsemacquire),
	S(_nsec)	= ABI_EMUL("nsec", 0, p9_sys_nsec),
};

const unsigned int plan9_nsysent = ARRAY_SIZE(plan9_sysent);
