/*
 * Copyright (C) 1993  Linus Torvalds
 * Copyright (C) 2001  Caldera Deutschland GmbH
 *
 * Modified by Eric Youngdale to include all ibcs syscalls.
 * Re-written by Drew Sullivan to handle lots more of the syscalls correctly.
 */

#ident "%W% %G%"

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/gfp.h>
#include <linux/stat.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/fcntl.h>
#include <linux/time.h>
#include <linux/personality.h>
#include <linux/syscalls.h>

#include <linux/fs.h>
#include <linux/sys.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/dirent.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#include <abi/signal.h>
#include <abi/util/trace.h>

#include <abi/svr4/types.h>


MODULE_DESCRIPTION("common code for SVR3/SVR4 based personalities");
MODULE_AUTHOR("Christoph Hellwig, partially taken from iBCS");
MODULE_LICENSE("GPL");


int
abi_time(void)
{
	return sys_time(0);
}

int
abi_brk(unsigned long brk)
{
	unsigned long newbrk = PAGE_ALIGN(brk), oldbrk, sysbrk;

	down_read(&current->mm->mmap_sem);
	if (!brk)
		goto report;
	oldbrk = PAGE_ALIGN(current->mm->brk);
	up_read(&current->mm->mmap_sem);

	if (newbrk != oldbrk) {
		sysbrk = sys_brk(brk);
		if (PAGE_ALIGN(sysbrk) != newbrk)
			return -ENOMEM;
	}
	return 0;

report:
	/* return non-pagealigned old brk value */
	oldbrk = current->mm->brk;
	up_read(&current->mm->mmap_sem);
	return oldbrk;
}

/*
 * UNIX wants a 0-edx after fork and may have set
 * the carry flag before calling fork.
 */
int
abi_fork(struct pt_regs *regs)
{
	int		retval;

	regs->eflags &= ~1;
	retval = do_fork(SIGCHLD, regs->esp, regs, 0,
                /* parent_tidptr */NULL, /* child_tidptr */NULL);
	regs->edx = 0;
	return retval;
}

/*
 * Unlike Linux, UNIX does not use C calling conventions
 * for pipe().
 */
int
abi_pipe(struct pt_regs *regs)
{
	int		filedes[2], retval;

	retval = do_pipe(filedes);
	if (retval == 0) {
		retval = filedes[0];
		regs->edx = filedes[1];
	}
	return retval;
}

/*
 * Note the double value return in eax and edx.
 */
int
abi_getpid(struct pt_regs *regs)
{
	regs->edx = current->parent->pid;
	return current->pid;
}

/*
 * Note the double value return in eax and edx.
 */
int
abi_getuid(struct pt_regs *regs)
{
	regs->edx = current->euid;
	return current->uid;
}

/*
 * Note the double value return in eax and edx.
 */
int
abi_getgid(struct pt_regs *regs)
{
	regs->edx = current->egid;
	return current->gid;
}



enum {
	FLAG_ZF	= 0x0040,
	FLAG_PF	= 0x0004,
	FLAG_SF	= 0x0080,
	FLAG_OF	= 0x0800,
};

#define MAGIC_WAITPID_FLAG (FLAG_ZF | FLAG_PF | FLAG_SF | FLAG_OF)

int
abi_wait(struct pt_regs * regs)
{
	mm_segment_t		fs;
	long			result, kopt = 0;
	int			loc, opt;
	pid_t			pid;

	/*
	 * Xenix wait() puts status to edx and returns pid.
	 *
	 * XXX	xenix should get it's own syyent table so we can
	 * XXX	rip this cruft out.
	 */
	if (is_cur_personality(PER_XENIX)) {
		fs = get_fs();
		set_fs(get_ds());
		result = sys_wait4(-1, &loc, 0, NULL);
		set_fs(fs);

		regs->edx = loc;
		return result;
	}

	/*
	 * if ZF,PF,SF,and OF are set then it is waitpid
	 */
	if ((regs->eflags & MAGIC_WAITPID_FLAG) == MAGIC_WAITPID_FLAG) {
		get_user(pid, ((u_long *)regs->esp)+1);
		get_user(loc, ((u_long *)regs->esp)+2);
		get_user(opt, ((u_long *)regs->esp)+3);

		/*
		 * Now translate the options from the SVr4 numbers
		 */
		if (opt & 0100)
			kopt |= WNOHANG;
		if (opt & 4)
			kopt |= WUNTRACED;

		result = sys_wait4(pid, (u_int *)loc, kopt, NULL);
	} else {
		get_user(loc, ((u_long *)regs->esp)+1);
		result = sys_wait4(-1, (u_int *)loc, WUNTRACED, NULL);
	}

	if (result < 0 || !loc)
		return result;

	get_user(regs->edx, (u_long *)loc);
	if ((regs->edx & 0xff) == 0x7f) {
		int		sig;

		sig = (regs->edx >> 8) & 0xff;
		if (sig < NSIGNALS)
			sig = current_thread_info()->exec_domain->signal_map[sig];
		regs->edx = (regs->edx & (~0xff00)) | (sig << 8);
		put_user(regs->edx, (u_long *)loc);
	} else if (regs->edx && regs->edx == (regs->edx & 0xff)) {
		if ((regs->edx & 0x7f) < NSIGNALS)
			regs->edx = current_thread_info()->exec_domain->signal_map[regs->edx & 0x7f];
		put_user(regs->edx, (u_long *)loc);
	}
	return result;
}

#if defined(CONFIG_ABI_TRACE)
/*
 * Trace arguments of exec().
 *
 * We show up to twenty arguments and enviroment variables.
 * This could as well be sysctl configurable.
 */
static void
trace_exec(struct pt_regs *regs, char *pgm, char **argv, char **envp)
{
	char			**v, *p = NULL, *q = NULL;
	int			i;

	q = getname(pgm);
	if (IS_ERR(q)) {
		__abi_trace("\tpgm: %p pointer error %ld\n", pgm, PTR_ERR(q));
	} else {
		__abi_trace("\tpgm: %p \"%s\"\n", pgm, q);
		putname(q);
	}

	for (i = 0, v = argv; v && i < 20; v++, i++) {
		if (get_user(p, v) || !p)
			break;

		q = getname(p);
		if (IS_ERR(q)) {
			__abi_trace("\targ: %p pointer error %ld\n",
					p, PTR_ERR(q));
		} else {
			__abi_trace("\targ: %p \"%s\"\n", p, q);
			putname(q);
		}
	}

	if (v && p)
		__abi_trace("\targ: ...\n");

	for (i = 0, v = envp; v && i < 20; v++, i++) {
		if (get_user(p, v) || !p)
			break;

		q = getname(p);
		if (IS_ERR(q)) {
			__abi_trace("\tenv: %p pointer error %ld\n",
					p, PTR_ERR(q));
		} else {
			__abi_trace("\tenv: %p \"%s\"\n", p, q);
			putname(q);
		}
	}

	if (v && p)
		__abi_trace("\tenv: ...\n");
}
#endif

/*
 * Execute a new program.
 *
 * The difference from the native version is that we
 * optionally trace the arguments.
 */
int
abi_exec(struct pt_regs *regs)
{
	char		*pgm, **argv, **envp;
	char		*filename;
	int		error;
	u_long		*ulptr;

	ulptr = (u_long *)&pgm;
	if (get_user(*ulptr, ((u_long *)regs->esp)+1))
		return -EFAULT;
	ulptr = (u_long *)&argv;
	if (get_user(*ulptr, ((u_long *)regs->esp)+2))
		return -EFAULT;
	ulptr = (u_long *)&envp;
	if (get_user(*ulptr, ((u_long *)regs->esp)+3))
		return -EFAULT;

#if defined(CONFIG_ABI_TRACE)
	if (abi_traced(ABI_TRACE_API))
		trace_exec(regs, pgm, argv, envp);
#endif

	filename = getname(pgm);
	if (!IS_ERR(filename)) {
		error = do_execve(filename, argv, envp, regs);
		putname (filename);
        } else
		error = PTR_ERR(filename);
	return error;
}

/*
 * Yet another crufy SysV multiplexed syscall.
 * This time it's all the process group and session handling.
 *
 * NOTE: we return EPERM on get_user failures as EFAULT is not
 * a valid return value for theses calls.
 */
int
abi_procids(struct pt_regs *regs)
{
	int			offset = 0, op;

	if (get_user(op, ((u_long *)regs->esp)+1))
		return -EPERM;

	/* Remap op codes for current personality if necessary. */
	switch (get_cur_personality_id()) {
	case (PERID_SVR3):
	case (PERID_SCOSVR3):
	case (PERID_WYSEV386):
	case (PERID_XENIX):
		/*
		 * SCO at least uses an interesting library to
		 * syscall mapping that leaves an extra return
		 * address between the op code and the arguments.
		 *
		 * WTF does SCO at least mean?
		 * Could someone please verify this with another
		 * SVR3 derivate as I have none.
		 * 					--hch
		 */
		offset = 1;

		if (op < 0 || op > 5)
			return -EINVAL;
		op = "\000\001\005\003\377\377"[op];
	}

	switch (op) {
	case 0: /* getpgrp */
		return process_group(current);

	case 1: /* setpgrp */
		sys_setpgid(0, 0);
		return process_group(current);

	case 2: /* getsid */
	    {
		pid_t		pid;

		if (get_user(pid, ((u_long *)regs->esp)+2 + offset))
			return -EPERM;
		return sys_getsid(pid);
	    }

	case 3: /* setsid */
		return sys_setsid();

	case 4: /* getpgid */
	    {
		pid_t		pid;

		if (get_user(pid, ((u_long *)regs->esp)+2 + offset))
			return -EPERM;
		return sys_getpgid(pid);
	    }

	case 5: /* setpgid */
	    {
		pid_t		pid, pgid;

		if (get_user(pid, ((u_long *)regs->esp)+2 + offset))
			return -EPERM;
		if (get_user(pgid, ((u_long *)regs->esp)+3 + offset))
			return -EPERM;
		return sys_setpgid(pid, pgid);
	    }
	}

	return -EINVAL;
}


/*
 * Stupid bloody thing is trying to read a directory.
 *
 * Some old programs expect this to work. It works on SCO.
 * To emulate it we have to map a dirent to a direct. This
 * involves shrinking a long inode to a short. Fortunately
 * nothing this archaic is likely to care about anything
 * but the filenames of entries with non-zero inodes.
 */
int
abi_read_dir(int fd, char *buf, int count)
{
	struct file		*fp;
	struct old_linux_dirent	*de;
	mm_segment_t		fs;
	int			error, here;
	int			posn = 0, reclen = 0;


	fp = fget(fd);
	if (!fp)
		return -EBADF;

	error = -ENOMEM;
	de = (struct old_linux_dirent *)__get_free_page(GFP_KERNEL);
	if (!de)
		goto out_fput;

	error = 0;
	while (posn + reclen < count) {
		char		*p;

		/*
		 * Save the current position and get another dirent
		 */
		here = fp->f_pos;

		fs = get_fs();
		set_fs (get_ds());
		error = old_readdir(fd, de, 1);
		set_fs(fs);

		if (error <= 0)
			break;

		/*
		 * If it'll fit in the buffer save it.
		 * Otherwise back up so it is read next time around.
		 * Oh, if we're at the beginning of the buffer there's
		 * no chance that this entry will ever fit so don't
		 * copy it and don't back off - we'll just pretend it
		 * isn't here...
		 */

		/*
		 * SCO (at least) handles long filenames by breaking
		 * them up in to 14 character chunks of which all
		 * but the last have the inode set to 0xffff.
		 * Those chunks will get aligned to a 4 byte boundary
		 * thus leaving two bytes in each entry for other purposes.
		 *
		 * Well, that's SCO E(A)FS.
		 * HTFS and DTFS should handle it better.
		 * 					--hch
		 */
		reclen = 16 * ((de->d_namlen + 13) / 14);
		if (posn + reclen > count) {
			if (posn)
				sys_lseek(fd, here, 0);
			continue;
		}

		p = de->d_name;

		/*
		 * Put all but the last chunk.
		 */
		while (de->d_namlen > 14) {
			put_user(0xffff, (u_short *)(buf+posn));
			posn += 2;
			if (copy_to_user(buf+posn, p, 14))
				goto out_fault;
			posn += 14;
			p += 14;
			de->d_namlen -= 14;
		}

		/*
		 * Put the last chunk. Note the we have to fold a
		 * long inode number down to a short avoiding
		 * giving a zero inode number since that indicates
		 * an unused directory slot. Note also that the
		 * folding used here must match that used in stat()
		 * or path finding programs that do read() on
		 * directories will fail.
		 */
#if 0
		/*
		 * This appears to match what SCO does for
		 * reads on a directory with long inodes.
		 */
		if ((u_long)de->d_ino > 0xfffe) {
			if (put_user(0xfffe, buf+posn))
				goto out_fault;
		} else {
			if (put_user((short)de->d_ino, buf+posn))
				goto out_fault;
		}
#else
		/*
		 * This attempts to match the way stat and
		 * getdents fold long inodes to shorts.
		 */
		if ((u_long)de->d_ino & 0xffff ) {
			if (put_user((u_long)de->d_ino & 0xffff, buf+posn))
				goto out_fault;
		} else {
			if (put_user(0xfffe, buf+posn))
				goto out_fault;
		}
#endif
		posn += 2;
		if (copy_to_user(buf+posn, p, de->d_namlen))
			goto out_fault;

		/*
		 * Ensure that filenames that don't fill the array
		 * completely are null filled.
		 */
		for (; de->d_namlen < 14; de->d_namlen++) {
			if (put_user('\0', buf+posn+de->d_namlen))
				goto out_fault;
		}
		posn += 14;
	}

	free_page((u_long)de);
	fput(fp);

	/*
	 * If we've put something in the buffer return the byte count
	 * otherwise return the error status.
	 */
	return (posn ? posn : error);

out_fault:
	error = -EFAULT;
	free_page((u_long)de);
out_fput:
	fput(fp);
	return error;
}

/*
 * We could use Linux read if there wouldn't be the
 * read on directory issue..
 */
int
abi_read(int fd, char *buf, int count)
{
	int		error;

	error = sys_read(fd, buf, count);
	if (error == -EISDIR)
		error = abi_read_dir(fd, buf, count);
	return error;
}

/*
 * Linux doesn't allow trailing slashes in mkdir.
 * Old UNIX apps expect it work anyway, so we have
 * to get rid of them here.
 */
int
abi_mkdir(const char *fname, int mode)
{
	mm_segment_t		fs;
	char			*tmp, *p;
	int			error;

	tmp = getname(fname);
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);

	for (p = tmp; *p; p++);
		p--;
	if (*p == '/')
		*p = '\0';

	fs = get_fs();
	set_fs(get_ds());
	error = sys_mkdir(tmp, mode);
	set_fs(fs);

	putname(tmp);
	return error;
}

/*
 * Unlike UNIX Linux doesn't allow to create
 * directories using mknod.
 */
int
svr4_mknod(char *filename, svr4_o_mode_t mode, svr4_o_dev_t dev)
{
	if ((mode & 0017000) == 0040000)
		return abi_mkdir(filename, mode);
	return sys_mknod(filename, mode,
		MKDEV (SVR4_O_MAJOR (dev), SVR4_O_MINOR (dev)));
}

static int
svr4_do_xmknod(char *filename, svr4_mode_t mode, svr4_dev_t dev)
{
	u_int minor = SVR4_MINOR (dev), major = SVR4_MAJOR (dev);

	if (minor > 0xff || major > 0xff)
		return -EINVAL;
	return svr4_mknod(filename, mode, SVR4_O_MKDEV (major, minor));
}


enum {SVR4_mknod = 1, SVR4_xmknod = 2};

int
svr4_xmknod(int vers, char *filename, svr4_mode_t mode, svr4_dev_t dev)
{
	switch (vers) {
	case SVR4_mknod:
		return svr4_mknod(filename, mode, dev);
	case SVR4_xmknod:
		return svr4_do_xmknod(filename, mode, dev);
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "xmknod version %d not supported\n", vers);
#endif
	return -EINVAL;
}

int
abi_kill(int pid, int sig)
{
	int insig, outsig;

	insig = (sig & 0xff);
	outsig = current_thread_info()->exec_domain->signal_map[insig];

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_SIGNAL, "kill: %d -> %d\n", insig, outsig);
#endif

	return sys_kill(pid, outsig);
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(abi_brk);
EXPORT_SYMBOL(abi_exec);
EXPORT_SYMBOL(abi_fork);
EXPORT_SYMBOL(abi_getgid);
EXPORT_SYMBOL(abi_getpid);
EXPORT_SYMBOL(abi_getuid);
EXPORT_SYMBOL(abi_kill);
EXPORT_SYMBOL(abi_mkdir);
EXPORT_SYMBOL(abi_pipe);
EXPORT_SYMBOL(abi_procids);
EXPORT_SYMBOL(abi_read);
EXPORT_SYMBOL(abi_time);
EXPORT_SYMBOL(abi_wait);
EXPORT_SYMBOL(svr4_mknod);
EXPORT_SYMBOL(svr4_xmknod);
#endif
