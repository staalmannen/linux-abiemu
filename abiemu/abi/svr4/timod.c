/*
 * Copyright 1995, 1996  Mike Jagdis (jaggy@purplet.demon.co.uk)
 */

#ident "%W% %G%"

#include <linux/config.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/fcntl.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/un.h>
#include <linux/file.h>
#include <linux/personality.h>
#include <linux/poll.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <net/sock.h>

#include <asm/abi_machdep.h>
#include <abi/stream.h>
#include <abi/tli.h>

#include <abi/svr4/ioctl.h>
#include <abi/util/trace.h>


/*
 * Check if the inode belongs to /dev/socksys.
 */
#define IS_SOCKSYS(ip) (MAJOR((ip)->i_rdev) == SOCKSYS_MAJOR)


int
timod_getmsg(int fd, struct inode *ip, int pmsg, struct pt_regs *regs)
{
	struct strbuf	ctl, *ctlp, dat, *datp;
	int		flags, *flagsp, *bandp;
	int		error;

	ctlp = (struct strbuf *)get_syscall_parameter(regs, 1);
	datp = (struct strbuf *)get_syscall_parameter(regs, 2);

	if (pmsg) {
		bandp = (int *)get_syscall_parameter(regs, 3);
		flagsp = (int *)get_syscall_parameter(regs, 4);
	} else
		flagsp = (int *)get_syscall_parameter (regs, 3);

	if (ctlp) {
		if (copy_from_user(&ctl, ctlp, sizeof(ctl)))
			return -EFAULT;
		if ((error = put_user(-1, &ctlp->len)))
			return error;
	} else
		ctl.maxlen = -1;

	if (datp) {
		if (copy_from_user(&dat, datp, sizeof(dat)))
			return -EFAULT;
		if ((error = put_user(-1, &datp->len)))
			return error;
	} else
		dat.maxlen = -1;

	if ((error = get_user(flags, flagsp)))
		return error;

#ifdef CONFIG_ABI_SPX
	if (IS_SOCKSYS(ip) && MINOR(ip->i_rdev) == 1) {

#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_STREAMS,
			"SPX: getmsg offers descriptor %d\n", fd);
#endif

		if ((error = put_user(fd, ctl.buf)))
			return error;
		if ((error = put_user(4, &ctlp->len)))
			return error;

		return 0;
	}
#endif /* CONFIG_ABI_SPX */

#ifdef CONFIG_ABI_XTI
	if (flags == 0 || flags == MSG_HIPRI ||
	    flags == MSG_ANY || flags == MSG_BAND) {
		struct file	*fp;

		fp = fget(fd);
		error = do_getmsg(fd, regs, ctl.buf, ctl.maxlen, &ctlp->len,
				dat.buf, dat.maxlen, &datp->len, &flags);
		fput(fp);

		if (error >= 0)
			error = put_user(flags, flagsp);
		return error;
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_STREAMS,
			"XTI: getmsg flags value bad (%d) for %d\n",
			flags, fd);
#endif /* CONFIG_ABI_TRACE */
#endif /* CONFIG_ABI_XTI */
	return -EINVAL;
}


int
timod_putmsg(int fd, struct inode *ip, int pmsg, struct pt_regs *regs)
{
	struct strbuf		ctl, *ctlp, dat, *datp;
	int			flags, band;
	int			error;

	ctlp = (struct strbuf *)get_syscall_parameter(regs, 1);
	datp = (struct strbuf *)get_syscall_parameter(regs, 2);
	if (pmsg) {
		band = get_syscall_parameter(regs, 3);
		flags = get_syscall_parameter(regs, 4);
	} else
		flags = get_syscall_parameter(regs, 3);

	if (ctlp) {
		if (copy_from_user(&ctl, ctlp, sizeof(ctl)))
			return -EFAULT;
		if (ctl.len < 0 && flags)
			return -EINVAL;
	} else {
		ctl.len = 0;
		ctl.buf = NULL;
	}

	if (datp) {
		if (copy_from_user(&dat, datp, sizeof(dat)))
			return -EFAULT;
	} else {
		dat.len = 0;
		dat.buf = NULL;
	}

#ifdef CONFIG_ABI_SPX
	if (IS_SOCKSYS(ip) && MINOR(ip->i_rdev) == 1) {
		int		newfd;

		if (ctl.len != 4)
			return -EIO;

		error = get_user(newfd, ctl.buf);
		if (error)
			return error;

#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_STREAMS,
				"SPX: putmsg on %d dups descriptor %d\n",
				fd, newfd);
#endif
		error = sys_dup2(newfd, fd);

		return (error < 0 ? error : 0);
	}
#endif /* CONFIG_ABI_SPX */

#ifdef CONFIG_ABI_XTI
	return do_putmsg(fd, regs, ctl.buf, ctl.len,
			dat.buf, dat.len, flags);
#endif
	return -EINVAL;
}

int
stream_fdinsert(struct pt_regs *regs, int fd, struct strfdinsert *arg)
{
	struct strfdinsert	sfd;

	if (copy_from_user(&sfd, arg, sizeof(sfd)))
		return -EFAULT;

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_STREAMS,
			"%u fdinsert: flags=%ld, fildes=%u, offset=%d\n",
			fd, sfd.flags, sfd.fildes, sfd.offset);
#endif
#ifdef CONFIG_ABI_XTI
	return do_putmsg(fd, regs, sfd.ctlbuf.buf, sfd.ctlbuf.len,
			sfd.datbuf.buf, sfd.datbuf.len, sfd.fildes);
#else
	return -EINVAL;
#endif
}
