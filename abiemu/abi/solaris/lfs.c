/*
 * Solaris Large File Summit support
 */

#ident "%W% %G%"

#include <linux/errno.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/un.h>
#include <linux/dirent.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <abi/util/map.h>
#include <abi/svr4/sysent.h>


int
sol_open64(const char *fname, int flag, int mode)
{
	u_long args[3];
	int error, fd;
	struct file *file;
	mm_segment_t old_fs;
	char *p;
	struct sockaddr_un addr;

	fd = sys_open(fname, map_flags(flag, fl_svr4_to_linux) | O_LARGEFILE, mode);
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
}


struct sol_dirent64 {
	unsigned long long d_ino;
	unsigned long long d_off;
	unsigned short d_reclen;
	char d_name[1];
};


/* If/when the readdir function is changed to read multiple entries
 * at once this should be updated to take advantage of the fact.
 *
 * N.B. For Linux the reclen in a dirent is the number of characters
 * in the filename, for SCO (at least) reclen is the total size of
 * the particular dirent rounded up to the next multiple of 4. The SCO
 * behaviour is faithfully emulated here.
 *
 * XXXX
 * We don't truncate long filenames at all when copying. If we meet a
 * long filename and the buffer supplied by the application simply isn't
 * big enough to hold it we'll return without filling the buffer (i.e
 * return 0). The application will see this as a (premature) end of
 * directory. Is there a work around for this at all???
 */
int
sol_getdents64(int fd, char *buf, int nbytes)
{
	int error, here, posn, reclen;
	struct file *file;
	struct old_linux_dirent *d;
	mm_segment_t old_fs;

	error = verify_area(VERIFY_WRITE, buf, nbytes);
	if (error)
		return error;

	/* Check the file handle here. This is so we can access the current
	 * position in the file structure safely without a tedious call
	 * to sys_lseek that does nothing useful.
	 */
	file = fget(fd);
	if (!file)
		return -EBADF;

	d = (struct old_linux_dirent *)__get_free_page(GFP_KERNEL);
	if (!d) {
		fput(file);
		return -ENOMEM;
	}

	error = posn = reclen = 0;
	while (posn + reclen < nbytes) {
                int string_size;
		struct sol_dirent64 tmpbuf;
		/* Save the current position and get another dirent */
		here = file->f_pos;
		old_fs = get_fs();
		set_fs (get_ds());
		error = old_readdir(fd, d, 1);
		set_fs(old_fs);
		if (error <= 0)
			break;

		/* If it'll fit in the buffer save it.
		 * Otherwise back up so it is read next time around.
		 * Oh, if we're at the beginning of the buffer there's
		 * no chance that this entry will ever fit so don't
		 * copy it and don't back off - we'll just pretend it
		 * isn't here...
		 */
		string_size = d->d_namlen + 1; /* chars in d_name + trailing zero */
		reclen = (sizeof(long long) /* d_ino */
			+ sizeof(long long) /* d_offset */
			+ sizeof(unsigned short) /* d_namlen */
			+ string_size
			+ 3) & (~3); /* align to a 4 byte boundary */
		if (posn + reclen <= nbytes) {
			tmpbuf.d_off = file->f_pos;
			tmpbuf.d_ino = d->d_ino;
			tmpbuf.d_off = file->f_pos;
			tmpbuf.d_reclen = reclen;
			if (copy_to_user(buf+posn, &tmpbuf,
				     sizeof(struct sol_dirent64) -1))
			{	posn = 0;
				error = -EFAULT;
				break;
			}
		        if (copy_to_user(buf+posn+sizeof(struct sol_dirent64)-2,
				     &d->d_name, string_size))
			{	posn = 0;
				error = -EFAULT;
				break;
			}
			posn += reclen;
		} else if (posn) {
			sys_lseek(fd, here, 0);
		} /* else posn == 0 */
	}

	/* Loose the intermediate buffer. */
	free_page((unsigned long)d);

	fput(file);

	/* If we've put something in the buffer return the byte count
	 * otherwise return the error status.
	 */
	return ((posn > 0) ? posn : error);
}


int
sol_mmap64(u_int addr, u_int len, int prot, int flags,
           int fd, u_int off_hi, u_int off)
{
	loff_t			off64 = (off | ((loff_t)off_hi << 32));
	u_long			pgoff = (off64 >> PAGE_SHIFT);
	struct file		*file = NULL;
	int			error;

	if ((off64 + PAGE_ALIGN(len)) < off64)
		return -EINVAL;

	if (!(off64 & ~PAGE_MASK))
		return -EINVAL;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	if (!(flags & MAP_ANONYMOUS)) {
		if (!(file = fget(fd)))
			return -EBADF;
	}

	if (!(flags & 0x80000000) && addr)
		flags |= MAP_FIXED;
	else
		flags &= 0x7fffffff;

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(file, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);
	return (error);
}
