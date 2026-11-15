/*	$Id: sockio.c 31 2006-01-24 04:40:18Z fwenzel $	*/

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sockios.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <asm/ioctls.h>

#include <abi/stream.h>
#include <abi/tli.h>
#include <abi/socksys.h> /* for socksys_fdinit */

#include <abi/svr4/ioctl.h>
#include <abi/util/trace.h>


/*
 * Check if the inode belongs to /dev/socksys.
 */
#define IS_SOCKSYS(ip) (MAJOR((ip)->i_rdev) == SOCKSYS_MAJOR)


static int
i_nread(u_int fd, struct file *fp, struct inode *ip,
		void *data, struct pt_regs *regs)
{
	int			error;

	error = verify_area(VERIFY_WRITE, data, sizeof(u_long));
	if (error)
		goto fput;

	if (S_ISSOCK(ip->i_mode)) {
		struct T_private *ti = Priv(fp);

		if (IS_SOCKSYS(ip))
			timod_update_socket(fd, fp, regs);

		if (ti && ti->pfirst) {
			put_user(ti->pfirst->length, (u_long *)data);
			fput(fp);
			return 1; /* at least 1... (FIXME) */
		}
	}

	fput(fp);

	error = sys_ioctl(fd, TIOCINQ, (long)data);
	if (error == -EINVAL)
		return 0;
	else if (error)
		return error;

	__get_user(error, (u_long *)data);
	return !!error;
fput:
	fput(fp);
	return error;
}

static int
i_peek(u_int fd, struct file *fp, struct inode *ip,
		void *data, struct pt_regs *regs)
{
#if !defined(CONFIG_ABI_XTI)
	fput(fp);
	return 0;
#else
	struct T_private	*ti = Priv(fp);
	struct T_primsg		*tp;
	struct strpeek		buf, *uap = data;
	int			error = -EFAULT;

	if (copy_from_user(&buf, uap, sizeof(buf)))
		goto fput;

	error = 0;
	if (!S_ISSOCK(ip->i_mode))
		goto fput;

	if (IS_SOCKSYS(ip))
		timod_update_socket(fd, fp, regs);

	if (!ti || !ti->pfirst)
		goto fput;
	tp = ti->pfirst;

	error = -EFAULT;
	if (!buf.flags || buf.flags == tp->pri) {
		int	l;



		if (buf.ctl.maxlen <= tp->length)
			l = buf.ctl.maxlen;
		else
			l = tp->length;

		if (copy_to_user(buf.ctl.buf,
		    ((char *)&tp->type) + ti->offset, l))
			goto fput;

		if (put_user(l, &uap->ctl.len))
			goto fput;

		if (buf.dat.maxlen >= 0 && put_user(0, &uap->dat.len))
			goto fput;

		if (put_user(tp->pri, &uap->flags))
			goto fput;

		error = 1;
	}
fput:
	fput(fp);
	return error;
#endif /* CONFIG_ABI_XTI */
}

static int
i_str(u_int fd, struct file *fp, struct inode *ip,
		void *data, struct pt_regs *regs)
{
	int			cmd;
	/*
	 * Unpack the ioctl data and forward as a normal
	 * ioctl. Timeouts are not handled (yet?).
	 */
	struct strioctl {
		int cmd, timeout, len;
		char *data;
	} it, *uap = data;

	if (copy_from_user(&it, uap, sizeof(struct strioctl)))
		return -EFAULT;
	cmd = it.cmd >> 8;

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_STREAMS, "STREAMS I_STR ioctl(%d, 0x%08x, %p)\n",
			fd, it.cmd, it.data);
#endif

#ifdef CONFIG_ABI_XTI
	if (cmd == 'T')
		return timod_ioctl(regs, fd, it.cmd & 0xff, it.data, it.len,
			&uap->len);
#endif
	return __svr4_ioctl(regs, fd, it.cmd, it.data);
}

int
svr4_stream_ioctl(struct pt_regs *regs, int fd, u_int cmd, caddr_t data)
{
	struct file		*fp;
	struct inode		*ip;
	int			error;

	fp = fget(fd);
	if (!fp)
		return -EBADF;
	ip = fp->f_dentry->d_inode;

	/*
	 * Special hack^H^Hndling for socksys fds
	 */
	if (S_ISSOCK(ip->i_mode) == 0 && IS_SOCKSYS(ip)) {
		error = socksys_fdinit(fd, 0, NULL, NULL);
		if (error < 0)
			return error;
		fput(fp);
		fp = fget(fd);
		if (!fp)
			return -EBADF;
		ip = fp->f_dentry->d_inode;
	}

	switch (cmd) {
	case 001: /* I_NREAD */
		return i_nread(fd, fp, ip, data, regs);

	case 017: /* I_PEEK */
		return i_peek(fd, fp, ip, data, regs);
	}

	fput(fp);

	switch (cmd) {
	case 010: /* I_STR */
		return i_str(fd, fp, ip, data, regs);
	case 002: { /* I_PUSH */
		char *tmp;

		/* Get the name anyway to validate it. */
		tmp = getname(data);
		if (IS_ERR(tmp))
			return PTR_ERR(tmp);

#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_STREAMS,
				"%d STREAMS I_PUSH %s\n", fd, tmp);
#endif

		putname(tmp);
		return 0;
	}
	case 003: /* I_POP */
#if defined(CONFIG_ABI_TRACE)
		  abi_trace(ABI_TRACE_STREAMS, "%d STREAMS I_POP\n", fd);
#endif
		  return 0;

	case 005: /* I_FLUSH */
		  return 0;

	case 013: { /* I_FIND */
		char *tmp;

		/* Get the name anyway to validate it. */
		tmp = getname(data);
		if (IS_ERR(tmp))
				return PTR_ERR(tmp);

#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_STREAMS,
				"%d STREAMS I_FIND %s\n", fd, tmp);
#endif
#ifdef CONFIG_ABI_XTI
		if (!strcmp(tmp, "timod")) {
			putname(tmp);
			return 1;
		}
#endif
		putname(tmp);
		return 0;
	}

	/* FIXME: These are bogus. */
	case 011: /* I_SETSIG */
		return sys_ioctl(fd, FIOSETOWN, (long)current->pid);
	case 012: /* I_GETSIG */
		return sys_ioctl(fd, FIOGETOWN, (long)data);

	case 020: /* I_FDINSERT */
#ifdef CONFIG_ABI_XTI
		return stream_fdinsert(regs, fd,
				(struct strfdinsert *)data);
#else
		return -EINVAL;
#endif

	case 004: /* I_LOOK */
	case 006: /* I_SRDOPT */
	case 007: /* I_GRDOPT */
	case 014: /* I_LINK */
	case 015: /* I_UNLINK */
	case 021: /* I_SENDFD */
	case 022: /* I_RECVFD */
	case 023: /* I_SWROPT */
	case 040: /* I_SETCLTIME */
		return 0; /* Lie... */
	case 042: /* I_CANPUT */
		/*
		 * Arg is the priority band in question. We only
		 * support one priority band so data must be 0.
		 * If the band is writable we should return 1, if
		 * the band is flow controlled we should return 0.
		 */
		if (data)
			return -EINVAL;

		/* FIXME: How can we test if a write would block? */
		return 1;

	case 024: /* I_GWROPT */
	case 025: /* I_LIST */
	case 026: /* I_PLINK */
	case 027: /* I_PUNLINK */
	case 030: /* I_SETEV */
	case 031: /* I_GETEV */
	case 032: /* I_STREV */
	case 033: /* I_UNSTREV */
	case 034: /* I_FLUSHBAND */
	case 035: /* I_CKBAND */
	case 036: /* I_GETBAND */
	case 037: /* I_ATMARK */
	case 041: /* I_GETCLTIME */
			/* Unsupported - drop out. */
                break;

        default:
                break;
	}

	printk(KERN_ERR "iBCS: STREAMS ioctl 0%o unsupported\n", cmd);
	return -EINVAL;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_stream_ioctl);
#endif
