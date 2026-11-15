/*
 * Copyright (c) 1993  Joe Portman (baron@hebron.connected.com)
 * Copyright (c) 1993, 1994  Drew Sullivan (re-worked for iBCS2)
 * Copyright (c) 2000  Christoph Hellwig (rewrote lookup-related code)
 */

#ident "%W% %G%"

#include <linux/config.h>
#include <linux/module.h>

#include <linux/vfs.h>
#include <linux/types.h>
#include <linux/utime.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/tty.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/net.h>
#include <linux/un.h>
#include <linux/file.h>
#include <linux/dirent.h>
#include <linux/personality.h>
#include <linux/syscalls.h>
#include <linux/namei.h>

#include <asm/uaccess.h>
#include <asm/bitops.h>

#include <asm/abi_machdep.h>
#include <abi/svr4/statfs.h>
#include <abi/svr4/sysent.h>

#include <abi/util/trace.h>
#include <abi/util/map.h>


static int
copy_kstatfs(struct svr4_statfs *buf, struct kstatfs *st)
{
	struct svr4_statfs ibcsstat;

	ibcsstat.f_type = st->f_type;
	ibcsstat.f_bsize = st->f_bsize;
	ibcsstat.f_frsize = 0;
	ibcsstat.f_blocks = st->f_blocks;
	ibcsstat.f_bfree = st->f_bfree;
	ibcsstat.f_files = st->f_files;
	ibcsstat.f_ffree = st->f_ffree;
	memset(ibcsstat.f_fname, 0, sizeof(ibcsstat.f_fname));
	memset(ibcsstat.f_fpack, 0, sizeof(ibcsstat.f_fpack));

	/* Finally, copy it to the user's buffer */
	return copy_to_user(buf, &ibcsstat, sizeof(struct svr4_statfs));
}

int svr4_statfs(const char * path, struct svr4_statfs * buf, int len, int fstype)
{
	struct svr4_statfs ibcsstat;

	if (len > (int)sizeof(struct svr4_statfs))
		return -EINVAL;

	if (!fstype) {
		struct nameidata nd;
		int error;

		error = user_path_walk(path, &nd);
		if (!error) {
			struct kstatfs tmp;

			error = vfs_statfs(nd.dentry->d_inode->i_sb, &tmp);
			if (!error && copy_kstatfs(buf, &tmp))
				error = -EFAULT;
			path_release(&nd);
		}

		return error;
	}

	/*
	 * Linux can't stat unmounted filesystems so we
	 * simply lie and claim 500MB of 8GB is free. Sorry.
	 */
	ibcsstat.f_bsize = 1024;
	ibcsstat.f_frsize = 0;
	ibcsstat.f_blocks = 8 * 1024 * 1024;	/* 8GB */
	ibcsstat.f_bfree = 500 * 1024;		/* 100MB */
	ibcsstat.f_files = 60000;
	ibcsstat.f_ffree = 50000;
	memset(ibcsstat.f_fname, 0, sizeof(ibcsstat.f_fname));
	memset(ibcsstat.f_fpack, 0, sizeof(ibcsstat.f_fpack));

	/* Finally, copy it to the user's buffer */
	return copy_to_user(buf, &ibcsstat, len) ? -EFAULT : 0;
}

int svr4_fstatfs(unsigned int fd, struct svr4_statfs * buf, int len, int fstype)
{
	struct svr4_statfs ibcsstat;

	if (len > (int)sizeof(struct svr4_statfs))
		return -EINVAL;

	if (!fstype) {
		struct file * file;
		struct kstatfs tmp;
		int error;

		error = -EBADF;
		file = fget(fd);
		if (!file)
			goto out;
		error = vfs_statfs(file->f_dentry->d_inode->i_sb, &tmp);
		if (!error && copy_kstatfs(buf, &tmp))
			error = -EFAULT;
		fput(file);

out:
		return error;
	}

	/*
	 * Linux can't stat unmounted filesystems so we
	 * simply lie and claim 500MB of 8GB is free. Sorry.
	 */
	ibcsstat.f_bsize = 1024;
	ibcsstat.f_frsize = 0;
	ibcsstat.f_blocks = 8 * 1024 * 1024;	/* 8GB */
	ibcsstat.f_bfree = 500 * 1024;		/* 100MB */
	ibcsstat.f_files = 60000;
	ibcsstat.f_ffree = 50000;
	memset(ibcsstat.f_fname, 0, sizeof(ibcsstat.f_fname));
	memset(ibcsstat.f_fpack, 0, sizeof(ibcsstat.f_fpack));

	/* Finally, copy it to the user's buffer */
	return copy_to_user(buf, &ibcsstat, len) ? -EFAULT : 0;
}

int svr4_open(const char *fname, int flag, int mode)
{
#ifdef __sparc__
	return sys_open(fname, map_flags(flag, fl_svr4_to_linux), mode);
#else
	u_long args[3];
	int error, fd;
	struct file *file;
	mm_segment_t old_fs;
	char *p;
	struct sockaddr_un addr;

	fd = sys_open(fname, map_flags(flag, fl_svr4_to_linux), mode);
	if (fd < 0)
		return fd;

	/* Sometimes a program may open a pathname which it expects
	 * to be a named pipe (or STREAMS named pipe) when the
	 * Linux domain equivalent is a Unix domain socket. (e.g.
	 * UnixWare uses a STREAMS named pipe /dev/X/Nserver.0 for
	 * X :0 but Linux uses a Unix domain socket /tmp/.X11-unix/X0)
	 * It isn't enough just to make the symlink because you cannot
	 * open() a socket and read/write it. If we spot the error we can
	 * switch to socket(), connect() and things will likely work
	 * as expected however.
	 */
	file = fget(fd);
	if (!file)
		return fd; /* Huh?!? */
	if (!S_ISSOCK(file->f_dentry->d_inode->i_mode)) {
		fput(file);
		return fd;
	}
	fput(file);

	sys_close(fd);
	args[0] = AF_UNIX;
	args[1] = SOCK_STREAM;
	args[2] = 0;
	old_fs = get_fs();
	set_fs(get_ds());
	fd = sys_socketcall(SYS_SOCKET, args);
	set_fs(old_fs);
	if (fd < 0)
		return fd;

	p = getname(fname);
	if (IS_ERR(p)) {
		sys_close(fd);
		return PTR_ERR(p);
	}
	if (strlen(p) >= UNIX_PATH_MAX) {
		putname(p);
		sys_close(fd);
		return -E2BIG;
	}
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, p);
	putname(p);

	args[0] = fd;
	args[1] = (int)&addr;
	args[2] = sizeof(struct sockaddr_un);
	set_fs(get_ds());
	error = sys_socketcall(SYS_CONNECT, args);
	set_fs(old_fs);
	if (error) {
		sys_close(fd);
		return error;
	}

	return fd;
#endif
}

#define NAME_OFFSET(de)	((int) ((de)->d_name - (char *) (de)))
#define ROUND_UP(x)	(((x)+sizeof(long)-1) & ~(sizeof(long)-1))

struct svr4_getdents_callback {
	struct dirent * current_dir;
	struct dirent * previous;
	int count;
	int error;
};

static int svr4_filldir(void * __buf, const char * name, int namlen,
	loff_t offset, ino_t ino, unsigned int d_type)
{
	struct dirent * dirent;
	struct svr4_getdents_callback * buf = (struct svr4_getdents_callback *) __buf;
	int reclen = ROUND_UP(NAME_OFFSET(dirent) + namlen + 1);

	buf->error = -EINVAL;   /* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;

	dirent = buf->previous;
	if (dirent)
		put_user(offset, &dirent->d_off);
	dirent = buf->current_dir;
	buf->previous = dirent;

	if (is_cur_personality_flag(PERF_SHORT_INODE)) {
		/* read() on a directory only handles
		 * short inodes but cannot use 0 as that
		 * indicates an empty directory slot.
		 * Therefore stat() must also fold
		 * inode numbers avoiding 0. Which in
		 * turn means that getdents() must fold
		 * inodes avoiding 0 - if the program
		 * was built in a short inode environment.
		 * If we have short inodes in the dirent
		 * we also have a two byte pad so we
		 * can let the high word fall in the pad.
		 * This makes it a little more robust if
		 * we guessed the inode size wrong.
		 */
		if (!((unsigned long)dirent->d_ino & 0xffff))
			dirent->d_ino = 0xfffffffe;
	}

	put_user(ino, &dirent->d_ino);
	put_user(reclen, &dirent->d_reclen);
	if (copy_to_user(dirent->d_name, name, namlen))
		return -EFAULT;
	put_user(0, dirent->d_name + namlen);
	{	char *ptr = (char *)dirent;
		ptr += reclen;
		dirent = (struct dirent *)ptr;
	}
	buf->current_dir = dirent;
	buf->count -= reclen;
	return 0;
}



int svr4_getdents(int fd, char *dirent, int count)
{
	struct file * file;
	struct dirent * lastdirent;
	struct svr4_getdents_callback buf;
	int error;

	error = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	buf.current_dir = (struct dirent *) dirent;
	buf.previous = NULL;
	buf.count = count;
	buf.error = 0;
	error = vfs_readdir(file, svr4_filldir, &buf);
	if (error < 0)
		goto out_putf;
	error = buf.error;
	lastdirent = buf.previous;
	if (lastdirent) {
		put_user(file->f_pos, &lastdirent->d_off);
		error = count - buf.count;
	}

out_putf:
	fput(file);

out:
	return error;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_fstatfs);
EXPORT_SYMBOL(svr4_getdents);
EXPORT_SYMBOL(svr4_open);
EXPORT_SYMBOL(svr4_statfs);
#endif
