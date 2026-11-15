/*
 * Copyright (C) 1991, 1992  Linus Torvalds
 *
 * Written by Drew Sullivan.
 * Rewritten by Mike Jagdis.
 */

#ident "%W% %G%"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/termios.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <abi/svr4/ioctl.h>
#include <abi/svr4/termios.h>


#define SCO_NCCS (SVR_NCC+5)

struct sco_termios {
	u_int16_t	c_iflag;
	u_int16_t	c_oflag;
	u_int16_t	c_cflag;
	u_int16_t	c_lflag;
	char		c_line;
	u_char		c_cc[SCO_NCCS];
	char		c_ispeed;
	char		c_ospeed;
};

static int
sco_to_linux_termios(int fd, int op, struct sco_termios *it)
{
	struct termios		t;
	mm_segment_t		old_fs;
	u_short			lflag, r;
	char			sco_cc[SCO_NCCS];
	int			error;

	error = verify_area(VERIFY_READ, it, sizeof(struct sco_termios));
	if (error)
		return (error);

	old_fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, TCGETS, (long)&t);
	set_fs(old_fs);

	if (error)
		return (error);

	__get_user(t.c_iflag, &it->c_iflag);
	t.c_iflag &= ~0100000; /* DOSMODE */

	__get_user(t.c_oflag, &it->c_oflag);

	__get_user(t.c_cflag, &it->c_cflag);
	if (t.c_cflag & 0100000) /* CRTSFL - SCO only? */
		t.c_cflag |= CRTSCTS;
	t.c_cflag &= ~0170000; /* LOBLK|CTSFLOW|RTSFLOW|CRTSFL */

	lflag = t.c_lflag;
	t.c_lflag &= ~0100777;
	__get_user(r, &it->c_lflag);
	t.c_lflag |= r;

	if ((t.c_lflag & 0100000))
		sys_ioctl(fd, TIOCEXCL, 0);
	else
		sys_ioctl(fd, TIOCNXCL, 0);

	t.c_lflag &= ~0100000;
	t.c_lflag |= (t.c_lflag & 0000400) << 7; /* Move IEXTEN */
	t.c_lflag &= ~0000400;
	t.c_lflag |= (t.c_lflag & 0001000) >> 1; /* Move TOSTOP */
	t.c_lflag &= ~0001000;
	t.c_lflag |= (lflag & 0001000); /* Restore ECHOCTL */

	__get_user(t.c_line, &it->c_line); /* XXX Map this? */

	if (copy_from_user(sco_cc, &it->c_cc, SCO_NCCS))
		return -EFAULT;

	t.c_cc[0] = sco_cc[0];
	t.c_cc[1] = sco_cc[1];
	t.c_cc[2] = sco_cc[2];
	t.c_cc[3] = sco_cc[3];
	t.c_cc[7] = sco_cc[7];
	t.c_cc[8] = sco_cc[11];
	t.c_cc[9] = sco_cc[12];
	t.c_cc[10] = sco_cc[10];
	t.c_cc[16] = sco_cc[6];
	if (t.c_lflag & ICANON) {
		t.c_cc[4] = sco_cc[4];
		t.c_cc[11] = sco_cc[5];
	} else {
		t.c_cc[4] = sco_cc[8];
		t.c_cc[5] = sco_cc[5];
		t.c_cc[6] = sco_cc[4];
		t.c_cc[11] = sco_cc[9];
	}

	set_fs(get_ds());
	error = sys_ioctl(fd, op, (long)&t);
	set_fs(old_fs);

	return (error);
}

static int
linux_to_sco_termios(int fd, int op, struct sco_termios *it)
{
	struct termios		t;
	char			sco_cc[SCO_NCCS];
	mm_segment_t		old_fs;
	int			error;

	error = verify_area(VERIFY_WRITE, it, sizeof(struct sco_termios));
	if (error)
		return (error);

	old_fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, op, (long)&t);
	set_fs(old_fs);
	if (error)
		return (error);

	put_user(t.c_iflag & 0017777, &it->c_iflag);

	put_user(t.c_oflag & 0177777, &it->c_oflag);

	if (t.c_cflag & CRTSCTS)
		t.c_cflag |= 0100000; /* CRTSFL - SCO only? */
	put_user(t.c_cflag & 0177777, &it->c_cflag);

	t.c_lflag &= ~0001000;
	t.c_lflag |= (t.c_lflag & 0000400) << 1;
	t.c_lflag &= ~0000400;
	t.c_lflag |= (t.c_lflag & 0100000) >> 7;
	t.c_lflag &= ~0100000;
	put_user(t.c_lflag & 0001777, &it->c_lflag);

	put_user(t.c_line, &it->c_line); /* XXX Map this? */

	sco_cc[0] = t.c_cc[0];
	sco_cc[1] = t.c_cc[1];
	sco_cc[2] = t.c_cc[2];
	sco_cc[3] = t.c_cc[3];
	sco_cc[6] = t.c_cc[16];
	sco_cc[7] = t.c_cc[7];
	sco_cc[8] = t.c_cc[4];
	sco_cc[9] = t.c_cc[11];
	sco_cc[10] = t.c_cc[10];
	sco_cc[11] = t.c_cc[8];
	sco_cc[12] = t.c_cc[9];
	if (t.c_lflag & ICANON) {
		sco_cc[4] = t.c_cc[4];
		sco_cc[5] = t.c_cc[11];
	} else {
		sco_cc[4] = t.c_cc[6];
		sco_cc[5] = t.c_cc[5];
	}

	if (copy_to_user(&it->c_cc, sco_cc, SCO_NCCS))
		error = -EFAULT;

	return (error);
}

int
sco_term_ioctl(int fd, unsigned int func, void *arg)
{
	switch(func) {
	case 1:	/* XCGETA */
		return linux_to_sco_termios(fd, TCGETS, arg);
	case 2: /* XCSETA */
		return sco_to_linux_termios(fd, TCSETS, arg);
	case 3: /* XCSETAW */
		return sco_to_linux_termios(fd, TCSETSW, arg);
	case 4: /* XCSETAF */
		return sco_to_linux_termios(fd, TCSETSF, arg);
	}
	printk(KERN_ERR "iBCS: SCO termios ioctl %d unsupported\n", func);
	return -EINVAL;
}
