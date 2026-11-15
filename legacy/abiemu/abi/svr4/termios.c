#ident "%W% %G%"

#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/termios.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <abi/svr4/ioctl.h>
#include <abi/svr4/termios.h>


static int
svr_to_linux_termio(int fd, int op, struct svr_termio *it)
{
	struct termio		t;
	mm_segment_t		fs;
	char			eof;
	u_short			lflag;
	int			error;

	error = verify_area(VERIFY_READ, it, sizeof(struct svr_termio));
	if (error)
		return (error);

	fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, TCGETA, (long)&t);
	set_fs(fs);

	if (error)
		return (error);

	/* Save things we may need later. */
	eof = t.c_cc[4];
	lflag = t.c_lflag;

	/* Copy the entire structure then fix up as necessary. */
	if (copy_from_user(&t, it, sizeof(struct svr_termio)))
		return -EFAULT;

	/* If ICANON isn't set then we've been given VMIN in place
	 * of VEOF.
	 */
	if (!(t.c_lflag & 0000002)) {
		t.c_cc[6] = t.c_cc[4];
		t.c_cc[4] = eof;
	}

	if (t.c_cflag & 0100000) /* CRTSFL - SCO only? */
		t.c_cflag |= CRTSCTS;
	t.c_cflag &= ~0170000; /* LOBLK|CTSFLOW|RTSFLOW|CRTSFL */

	set_fs(get_ds());
	error = sys_ioctl(fd, op, (long)&t);
	set_fs(fs);

	return (error);
}

static int
linux_to_svr_termio(int fd, struct svr_termio *it)
{
	struct termio		t;
	mm_segment_t		fs;
	int			error;

	error = verify_area(VERIFY_WRITE, it, sizeof(struct svr_termio));
	if (error)
		return (error);

	fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, TCGETA, (long)&t);
	set_fs(fs);

	if (error)
		return (error);

	/* If ICANON isn't set then we substitute VEOF with VMIN. */
	if (!(t.c_lflag & 0000002))
		t.c_cc[4] = t.c_cc[6];

	/* Copy to the user supplied structure. */
	if (copy_to_user(it, &t, sizeof(struct svr_termio)))
		error = -EFAULT;
	return (error);
}

static int
svr4_to_linux_termios(int fd, int op, struct svr4_termios *it)
{
	struct termios		t;
	mm_segment_t		fs;
	u_short			lflag, r;
	char			svr4_cc[SVR4_NCCS];
	int			error;

	error = verify_area(VERIFY_READ, it, sizeof(struct svr4_termios));
	if (error)
		return (error);

	fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, TCGETS, (long)&t);
	set_fs(fs);

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

	if (copy_from_user(svr4_cc, &it->c_cc, SVR4_NCCS))
		return -EFAULT;

	t.c_cc[0] = svr4_cc[0];
	t.c_cc[1] = svr4_cc[1];
	t.c_cc[2] = svr4_cc[2];
	t.c_cc[3] = svr4_cc[3];
	t.c_cc[7] = svr4_cc[7];
	t.c_cc[8] = svr4_cc[8];
	t.c_cc[9] = svr4_cc[9];
	t.c_cc[10] = svr4_cc[10];
	t.c_cc[12] = svr4_cc[12];
	t.c_cc[13] = svr4_cc[13];
	t.c_cc[14] = svr4_cc[14];
	t.c_cc[15] = svr4_cc[15];
	t.c_cc[16] = svr4_cc[16];

	if (t.c_lflag & ICANON) {
		t.c_cc[4] = svr4_cc[4];
		t.c_cc[11] = svr4_cc[5];
	} else {
		t.c_cc[5] = svr4_cc[5];
		t.c_cc[6] = svr4_cc[4];
		t.c_cc[11] = svr4_cc[6];
	}

	set_fs(get_ds());
	error = sys_ioctl(fd, op, (long)&t);
	set_fs(fs);

	return (error);
}

static int
linux_to_svr4_termios(int fd, int op, struct svr4_termios *it)
{
	struct termios		t;
	char			svr4_cc[SVR4_NCCS];
	mm_segment_t		fs;
	int			error;

	error = verify_area(VERIFY_WRITE, it, sizeof(struct svr4_termios));
	if (error)
		return (error);

	fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, op, (long)&t);
	set_fs(fs);

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

	svr4_cc[0] = t.c_cc[0];
	svr4_cc[1] = t.c_cc[1];
	svr4_cc[2] = t.c_cc[2];
	svr4_cc[3] = t.c_cc[3];
	svr4_cc[6] = t.c_cc[16];
	svr4_cc[7] = t.c_cc[7];
	svr4_cc[8] = t.c_cc[8];
	svr4_cc[9] = t.c_cc[9];
	svr4_cc[10] = t.c_cc[10];
	svr4_cc[11] = t.c_cc[10];
	svr4_cc[12] = t.c_cc[12];
	svr4_cc[13] = t.c_cc[13];
	svr4_cc[14] = t.c_cc[14];
	svr4_cc[15] = t.c_cc[15];

	if (t.c_lflag & ICANON) {
		svr4_cc[4] = t.c_cc[4];
		svr4_cc[5] = t.c_cc[11];
	} else {
		svr4_cc[4] = t.c_cc[6];
		svr4_cc[5] = t.c_cc[5];
	}

	if (copy_to_user(&it->c_cc, svr4_cc, SVR4_NCCS))
		error = -EFAULT;

	return (error);
}

int
svr4_term_ioctl(int fd, u_int cmd, caddr_t data)
{
	switch (cmd) {
	case 1: /* TCGETA  (TIOC|1) */
		return linux_to_svr_termio(fd,
			(struct svr_termio *)data);
	case 2: /* TCSETA  (TIOC|2) */
		return svr_to_linux_termio(fd, TCSETA,
			(struct svr_termio *)data);
	case 3: /* TCSETAW (TIOC|3) */
		return svr_to_linux_termio(fd, TCSETAW,
			(struct svr_termio *)data);
	case 4: /* TCSETAF (TIOC|4) */
		return svr_to_linux_termio(fd, TCSETAF,
			(struct svr_termio *)data);
	case 5: /* TCSBRK  (TIOC|5) */
		return sys_ioctl(fd, TCSBRK, (long)data);
	case 6: /* TCXONC  (TIOC|6) */
		return sys_ioctl(fd, TCXONC, (long)data);
	case 7: /* TCFLSH  (TIOC|7) */
		return sys_ioctl(fd, TCFLSH, (long)data);
	/* This group appear in SVR4 but not SVR3 (SCO). */
	case 8: /* TIOCKBON */
	case 9: /* TIOCKBOF */
	case 10: /* KBENABLED */
		return -EINVAL;

	/* This set is used by SVR4 for termios ioctls. */
	case 13: /* TCGETS */
		return linux_to_svr4_termios(fd, TCGETS,
			(struct svr4_termios *)data);
	case 14: /* TCSETS */
		return svr4_to_linux_termios(fd, TCSETS,
			(struct svr4_termios *)data);
	case 15: /* TCSETSW */
		return svr4_to_linux_termios(fd, TCSETSW,
			(struct svr4_termios *)data);
	case 16: /* TCSETSF */
		return svr4_to_linux_termios(fd, TCSETSF,
			(struct svr4_termios *)data);

	/* These two are specific to ISC. */
	case 20: /* TCSETPGRP  (TIOC|20) set pgrp of tty */
		return sys_ioctl(fd, TIOCSPGRP, (long)data);
	case 21: /* TCGETPGRP  (TIOC|21) get pgrp of tty */
		return sys_ioctl(fd, TIOCGPGRP, (long)data);

	case  34: /* TCGETSC (TIOC|34) ioctl for scancodes */
		return 0x04; /* Translates scancode to ascii */
	case  35: /* TCSETSC (TIOC|35) ioctl for scancodes */
		return 0;

	case 103: /* TIOCSWINSZ (TIOC|103) */
		return sys_ioctl(fd, TIOCSWINSZ, (long)data);
	case 104: /* TIOCGWINSZ (TIOC|104) */
		return sys_ioctl(fd, TIOCGWINSZ, (long)data);

	case 118: /* TIOCSPGRP  (TIOC|118) set pgrp of tty */
		return sys_ioctl(fd, TIOCSPGRP, (long)data);
	case 119: /* TIOCGPGRP  (TIOC|119) get pgrp of tty */
		return sys_ioctl(fd, TIOCGPGRP, (long)data);

	case  32: /* TCDSET  (TIOC|32) */
	case  33: /* RTS_TOG (TIOC|33) 386 - "RTS" toggle define 8A1 protocol */

	case 120: /* TIOSETSAK  (TIOC|120) set SAK sequence for tty */
	case 121: /* TIOGETSAK  (TIOC|121) get SAK sequence for tty */
		printk(KERN_ERR "iBCS: termio ioctl %d unimplemented\n", cmd);
		return -EINVAL;
	}

	printk(KERN_ERR "iBCS: termio ioctl %d unsupported\n", cmd);
	return -EINVAL;
}

struct termiox {
	unsigned short x_hflag;
	unsigned short x_cflag;
	unsigned short x_rflag[5];
	unsigned short x_sflag;
};

#define RTSXOFF 0x0001
#define CTSXON	0x0002

int
svr4_termiox_ioctl(int fd, u_int cmd, caddr_t data)
{
	struct termios		t;
	struct termiox		tx;
	mm_segment_t		fs;
	int			error;

	if (cmd < 1 || cmd > 4)
		return -EINVAL;

	error = verify_area(cmd == 1 ? VERIFY_WRITE : VERIFY_READ,
			data, sizeof(struct termiox));
	if (error)
		return (error);

	fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, TCGETS, (long)&t);
	set_fs(fs);

	if (error)
		return (error);

	if (cmd == 1) { /* TCGETX */
		memset(&tx, '\0', sizeof(struct termiox));
		if (t.c_cflag & CRTSCTS)
			tx.x_hflag = RTSXOFF|CTSXON;
		if (copy_to_user(data, &tx, sizeof(struct termiox)))
			return -EFAULT;
		return 0;
	}

	if (copy_from_user(&tx, data, sizeof(struct termiox)))
		return -EFAULT;

	if ((tx.x_hflag != 0 && tx.x_hflag != (RTSXOFF|CTSXON))
	|| tx.x_cflag || tx.x_rflag[0] || tx.x_rflag[1]
	|| tx.x_rflag[2] || tx.x_rflag[3] || tx.x_rflag[4]
	|| tx.x_sflag)
		return -EINVAL;

	if (tx.x_hflag)
		t.c_cflag |= CRTSCTS;
	else
		t.c_cflag &= (~CRTSCTS);

	fs = get_fs();
	set_fs(get_ds());
	switch (cmd) {
		case 2: /* TCSETX */
			error = sys_ioctl(fd, TCSETS, (long)&t);
			break;
		case 3: /* TCSETXW */
			error = sys_ioctl(fd, TCSETSW, (long)&t);
			break;
		case 4: /* TCSETXF */
			error = sys_ioctl(fd, TCSETSF, (long)&t);
			break;
	}
	set_fs(fs);
	return (error);
}

#define BSD_NCCS 20
struct bsd_termios {
	unsigned long	c_iflag;
	unsigned long	c_oflag;
	unsigned long	c_cflag;
	unsigned long	c_lflag;
	unsigned char	c_cc[BSD_NCCS];
	long		c_ispeed;
	long		c_ospeed;
};
static unsigned long speed_map[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400,
	4800, 9600, 19200, 38400
};

static unsigned long
bsd_to_linux_speed(unsigned long s)
{
	unsigned int i;

#ifdef B57600
	if (s == 57600)
		return B57600;
#endif
#ifdef B115200
	if (s == 115200)
		return B115200;
#endif

	for (i=0; i<sizeof(speed_map)/sizeof(speed_map[0]); i++)
		if (s <= speed_map[i])
			return i;
	return B38400;
}

static unsigned long
linux_to_bsd_speed(unsigned long s)
{
#ifdef B57600
	if (s == B57600)
		return 57600;
#endif
#ifdef B115200
	if (s == B115200)
		return 115200;
#endif
	return speed_map[s];
}




static int
bsd_to_linux_termios(int fd, int op, struct bsd_termios *it)
{
	struct termios t;
	mm_segment_t old_fs;
	unsigned long temp;
	char bsd_cc[BSD_NCCS];
	int error;

	error = verify_area(VERIFY_READ, it, sizeof(struct bsd_termios));
	if (error)
		return error;

	old_fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, TCGETS, (long)&t);
	set_fs(old_fs);
	if (error)
		return error;

	__get_user(t.c_iflag, &it->c_iflag);
	t.c_iflag = (t.c_iflag & ~0xc00)
			| ((t.c_iflag & 0x400) << 1)
			| ((t.c_iflag & 0x800) >> 1);

	get_user(temp, &it->c_oflag);
	t.c_oflag = (t.c_oflag & ~0x1805)
			| (temp & 9)
			| ((temp & 2) << 1)
			| ((temp & 4) << 10)
			| ((temp & 4) << 9);

	get_user(temp, &it->c_cflag);
	t.c_cflag = (t.c_cflag & ~0xfff)
			| ((temp & 0xff00) >> 4);
	if (t.c_cflag & 0x30000)
		t.c_cflag |= 020000000000;
	t.c_cflag |= bsd_to_linux_speed(({long s; get_user(s, &it->c_ospeed); s;}))
		| (bsd_to_linux_speed(({long s; get_user(s, &it->c_ispeed); s;})) << 16);

	get_user(temp, &it->c_lflag);
	t.c_lflag = (t.c_lflag & ~0157663)
			| ((temp & 1) << 12)
			| ((temp & 0x46) << 3)
			| ((temp & 0x420) << 5)
			| ((temp & 0x180) >> 7)
			| ((temp & 0x400000) >> 14)
			| ((temp & 0x2800000) >> 11)
			| ((temp & 0x80000000) >> 24);

	if (copy_from_user(bsd_cc, &it->c_cc, BSD_NCCS))
		return -EFAULT;

	t.c_cc[VEOF] = bsd_cc[0];
	t.c_cc[VEOL] = bsd_cc[1];
	t.c_cc[VEOL2] = bsd_cc[2];
	t.c_cc[VERASE] = bsd_cc[3];
	t.c_cc[VWERASE] = bsd_cc[4];
	t.c_cc[VKILL] = bsd_cc[5];
	t.c_cc[VREPRINT] = bsd_cc[6];
	t.c_cc[VSWTC] = bsd_cc[7];
	t.c_cc[VINTR] = bsd_cc[8];
	t.c_cc[VQUIT] = bsd_cc[9];
	t.c_cc[VSUSP] = bsd_cc[10];
/*	t.c_cc[VDSUSP] = bsd_cc[11];*/
	t.c_cc[VSTART] = bsd_cc[12];
	t.c_cc[VSTOP] = bsd_cc[13];
	t.c_cc[VLNEXT] = bsd_cc[14];
	t.c_cc[VDISCARD] = bsd_cc[15];
	t.c_cc[VMIN] = bsd_cc[16];
	t.c_cc[VTIME] = bsd_cc[17];
/*	t.c_cc[VSTATUS] = bsd_cc[18];*/

	set_fs(get_ds());
	error = sys_ioctl(fd, op, (long)&t);
	set_fs(old_fs);

	return error;
}


static int
linux_to_bsd_termios(int fd, int op, struct bsd_termios *it)
{
	struct termios t;
	char bsd_cc[BSD_NCCS];
	mm_segment_t old_fs;
	int error;

	error = verify_area(VERIFY_WRITE, it, sizeof(struct bsd_termios));
	if (error)
		return error;

	old_fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, op, (long)&t);
	set_fs(old_fs);
	if (error)
		return error;

	put_user((t.c_iflag & 0777)
			| ((t.c_iflag & 02000) >> 1)
			| ((t.c_iflag & 010000) >> 2)
			| ((t.c_iflag & 020000) >> 4),
		&it->c_iflag);

	put_user((t.c_oflag & 1)
			| ((t.c_oflag & 04) >> 1)
			| ((t.c_oflag & 014000) == 014000 ? 4 : 0),
		&it->c_oflag);

	put_user((t.c_cflag & ~020000007777)
			| ((t.c_cflag & 0xff0) << 4)
			| ((t.c_cflag & 020000000000) ? 0x30000 : 0),
		&it->c_cflag);

	put_user(linux_to_bsd_speed(t.c_cflag & CBAUD), &it->c_ospeed);
	if ((t.c_cflag & CIBAUD) != 0)
		put_user(linux_to_bsd_speed((t.c_cflag & CIBAUD) >> 16),
			&it->c_ispeed);
	else
		put_user(linux_to_bsd_speed(t.c_cflag & CBAUD),
			&it->c_ispeed);

	put_user((t.c_lflag & 07777626010)
			| ((t.c_lflag & 03) << 7)
			| ((t.c_lflag & 01160) >> 3)
			| ((t.c_lflag & 0400) << 14)
			| ((t.c_lflag & 02000) >> 4)
			| ((t.c_lflag & 04000) >> 11)
			| ((t.c_lflag & 010000) << 11)
			| ((t.c_lflag & 040000) << 15)
			| ((t.c_lflag & 0100000) >> 5),
		&it->c_lflag);

	bsd_cc[0] = t.c_cc[VEOF];
	bsd_cc[1] = t.c_cc[VEOL];
	bsd_cc[2] = t.c_cc[VEOL2];
	bsd_cc[3] = t.c_cc[VERASE];
	bsd_cc[4] = t.c_cc[VWERASE];
	bsd_cc[5] = t.c_cc[VKILL];
	bsd_cc[6] = t.c_cc[VREPRINT];
	bsd_cc[7] = t.c_cc[VSWTC];
	bsd_cc[8] = t.c_cc[VINTR];
	bsd_cc[9] = t.c_cc[VQUIT];
	bsd_cc[10] = t.c_cc[VSUSP];
	bsd_cc[11] = t.c_cc[VSUSP];
	bsd_cc[12] = t.c_cc[VSTART];
	bsd_cc[13] = t.c_cc[VSTOP];
	bsd_cc[14] = t.c_cc[VLNEXT];
	bsd_cc[15] = t.c_cc[VDISCARD];
	bsd_cc[16] = t.c_cc[VMIN];
	bsd_cc[17] = t.c_cc[VTIME];
	bsd_cc[18] = 0; /* t.c_cc[VSTATUS]; */
	bsd_cc[19] = 0;

	if (copy_to_user(&it->c_cc, bsd_cc, BSD_NCCS))
		error = -EFAULT;

	return error;
}




struct v7_sgttyb {
	unsigned char	sg_ispeed;
	unsigned char	sg_ospeed;
	unsigned char	sg_erase;
	unsigned char	sg_kill;
	int	sg_flags;
};

struct v7_tchars {
	char	t_intrc;
	char	t_quitc;
	char	t_startc;
	char	t_stopc;
	char	t_eofc;
	char	t_brkc;
};

struct v7_ltchars {
	char	t_suspc;
	char	t_dsuspc;
	char	t_rprntc;
	char	t_flushc;
	char	t_werasc;
	char	t_lnextc;
};


int bsd_ioctl_termios(int fd, unsigned int func, void *data)
{
	switch (func & 0xff) {
		case 0:	 {				/* TIOCGETD */
			unsigned long ldisc;
			mm_segment_t old_fs;
			int error;

			error = verify_area(VERIFY_WRITE, data,
					sizeof(unsigned short));
			if (error)
				return error;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TIOCGETD, (long)&ldisc);
			set_fs(old_fs);
			if (!error)
				put_user(ldisc, (unsigned short *)data);
			return error;
		}
		case 1: {				/* TIOCSETD */
			unsigned long ldisc;
			mm_segment_t old_fs;
			int error;

			error = verify_area(VERIFY_READ, data,
					sizeof(unsigned short));
			if (error)
				return error;

			get_user(ldisc, (unsigned short *)data);
			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TIOCSETD, (long)&ldisc);
			set_fs(old_fs);
			return error;
		}

		case 2: {				/* TIOCHPCL */
			int error;
			mm_segment_t old_fs;
			struct termios t;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCGETS, (long)&t);
			set_fs(old_fs);
			if (error)
				return error;

			if (data)
				t.c_cflag |= HUPCL;
			else
				t.c_cflag &= ~HUPCL;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCSETS, (long)&t);
			set_fs(old_fs);
			return error;
		}

		case 8: {				/* TIOCGETP */
			int error;
			mm_segment_t old_fs;
			struct termios t;
			struct v7_sgttyb sg;

			error = verify_area(VERIFY_WRITE, data, sizeof(sg));
			if (error)
				return error;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCGETS, (long)&t);
			set_fs(old_fs);
			if (error)
				return error;

			sg.sg_ispeed = sg.sg_ospeed = 0;
			sg.sg_erase = t.c_cc[VERASE];
			sg.sg_kill = t.c_cc[VKILL];
			sg.sg_flags =
				/* Old - became TANDEM instead.
				 * ((t.c_cflag & HUPCL) >> 10)
				 * |
				 */
/* O_ODDP */			((t.c_cflag & PARODD) >> 3)
/* O_EVENP */			| ((t.c_cflag & PARENB) >> 1)
/* LITOUT */			| ((t.c_cflag & OPOST) ? 0 : 0x200000)
/* O_CRMOD */			| ((t.c_oflag & ONLCR) << 2)
/* O_NL1|O_VTDELAY */		| (t.c_oflag & (NL1|VTDLY))
/* O_TBDELAY */			| ((t.c_oflag & TABDLY) ? 02000 : 0)
/* O_CRDELAY */			| ((t.c_oflag & CRDLY) << 3)
/* O_BSDELAY */			| ((t.c_oflag & BSDLY) << 2)
/* O_ECHO|O_LCASE */		| (t.c_lflag & (XCASE|ECHO))
				| ((t.c_lflag & ICANON)
/* O_CBREAK or O_RAW */		? 0 : ((t.c_lflag & ISIG) ? 0x02 : 0x20))
				/* Incomplete... */
				;

			if (copy_to_user(data, &sg, sizeof(sg)))
				return -EFAULT;
			return 0;
		}

		case 9:					/* TIOCSETP */
		case 10: {				/* TIOCSETN */
			int error;
			mm_segment_t old_fs;
			struct termios t;
			struct v7_sgttyb sg;

			error = verify_area(VERIFY_READ, data, sizeof(sg));
			if (error)
				return error;
			if (copy_from_user(&sg, data, sizeof(sg)))
				return -EFAULT;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCGETS, (long)&t);
			set_fs(old_fs);
			if (error)
				return error;

			t.c_cc[VERASE] = sg.sg_erase;
			t.c_cc[VKILL] = sg.sg_kill;
			t.c_iflag = ICRNL | IXON;
			t.c_oflag = 0;
			t.c_lflag = ISIG | ICANON;
			if (sg.sg_flags & 0x02)		/* O_CBREAK */
				t.c_lflag &= (~ICANON);
			if (sg.sg_flags & 0x08)		/* O_ECHO */
				t.c_lflag |= ECHO|ECHOE|ECHOK|ECHOCTL|ECHOKE|IEXTEN;
			if (sg.sg_flags & 0x10)	/* O_CRMOD */
				t.c_oflag |= OPOST|ONLCR;
			if (sg.sg_flags & 0x20) {	/* O_RAW */
				t.c_iflag = 0;
				t.c_lflag &= ~(ISIG|ICANON);
			}
			if (sg.sg_flags & 0x200000)	/* LITOUT */
				t.c_oflag &= (~OPOST);
			if (!(t.c_lflag & ICANON)) {
				t.c_cc[VMIN] = 1;
				t.c_cc[VTIME] = 0;
			}

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCSETS, (long)&t);
			set_fs(old_fs);
			return error;
		}

		case 17: {				/* TIOCSETC */
			int error;
			mm_segment_t old_fs;
			struct termios t;
			struct v7_tchars tc;

			error = verify_area(VERIFY_READ, data, sizeof(tc));
			if (error)
				return error;
			if (copy_from_user(&tc, data, sizeof(tc)))
				return -EFAULT;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCGETS, (long)&t);
			set_fs(old_fs);
			if (error)
				return error;

			t.c_cc[VINTR] = tc.t_intrc;
			t.c_cc[VQUIT] = tc.t_quitc;
			t.c_cc[VSTART] = tc.t_startc;
			t.c_cc[VSTOP] = tc.t_stopc;
			t.c_cc[VEOF] = tc.t_eofc;
			t.c_cc[VEOL2] = tc.t_brkc;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCSETS, (long)&t);
			set_fs(old_fs);
			return error;
		}

		case 18: {				/* TIOCGETC */
			int error;
			mm_segment_t old_fs;
			struct termios t;
			struct v7_tchars tc;

			error = verify_area(VERIFY_WRITE, data, sizeof(tc));
			if (error)
				return error;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCGETS, (long)&t);
			set_fs(old_fs);
			if (error)
				return error;

			tc.t_intrc = t.c_cc[VINTR];
			tc.t_quitc = t.c_cc[VQUIT];
			tc.t_startc = t.c_cc[VSTART];
			tc.t_stopc = t.c_cc[VSTOP];
			tc.t_eofc = t.c_cc[VEOF];
			tc.t_brkc = t.c_cc[VEOL2];

			if (copy_to_user(data, &tc, sizeof(tc)))
				return -EFAULT;
			return 0;
		}

		case 116: {				/* TIOCGLTC */
			int error;
			mm_segment_t old_fs;
			struct termios t;
			struct v7_ltchars tc;

			error = verify_area(VERIFY_WRITE, data, sizeof(tc));
			if (error)
				return error;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCGETS, (long)&t);
			set_fs(old_fs);
			if (error)
				return error;

			tc.t_suspc = t.c_cc[VSUSP];
			tc.t_dsuspc = t.c_cc[VSUSP];
			tc.t_rprntc = t.c_cc[VREPRINT];
			tc.t_flushc = t.c_cc[VEOL2];
			tc.t_werasc = t.c_cc[VWERASE];
			tc.t_lnextc = t.c_cc[VLNEXT];

			if (copy_to_user(data, &tc, sizeof(tc)))
				return -EFAULT;
			return 0;
		}

		case 117: {				/* TIOCSLTC */
			int error;
			mm_segment_t old_fs;
			struct termios t;
			struct v7_ltchars tc;

			error = verify_area(VERIFY_READ, data, sizeof(tc));
			if (error)
				return error;
			if (copy_from_user(&tc, data, sizeof(tc)))
				return -EFAULT;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCGETS, (long)&t);
			set_fs(old_fs);
			if (error)
				return error;

			t.c_cc[VSUSP] = tc.t_suspc;
			t.c_cc[VEOL2] = tc.t_dsuspc;
			t.c_cc[VREPRINT] = tc.t_rprntc;
			t.c_cc[VEOL2] = tc.t_flushc;
			t.c_cc[VWERASE] = tc.t_werasc;
			t.c_cc[VLNEXT] = tc.t_lnextc;

			old_fs = get_fs();
			set_fs(get_ds());
			error = sys_ioctl(fd, TCSETS, (long)&t);
			set_fs(old_fs);
			return error;
		}

		case 13:				/* TIOEXCL */
			return sys_ioctl(fd, TIOCEXCL, (long)data);

		case 14:				/* TIOCNXCL */
			return sys_ioctl(fd, TIOCNXCL, (long)data);

		case 16:				/* TIOCFLUSH */
			return sys_ioctl(fd, TCFLSH, (long)data);


		/* ISC (maybe SVR4 in general?) has some extensions over
		 * the sgtty stuff. So do later BSDs. Needless to say they
		 * both have different extensions.
		 */
		case 20: /* TCSETPGRP  (TIOC|20) set pgrp of tty */
			return bsd_to_linux_termios(fd, TCSETS, data);

		case 21: /* TCGETPGRP  (TIOC|21) get pgrp of tty */
			return bsd_to_linux_termios(fd, TCSETSW, data);

		case 19:				/* TIOCGETA */
			return linux_to_bsd_termios(fd, TCGETS, data);

		case 22:				/* TIOCSETAF */
 			return bsd_to_linux_termios(fd, TCSETSF, data);

		case 26:				/* TIOCGETD */
			return sys_ioctl(fd, TIOCGETD, (long)data);

		case 27:				/* TIOCSETD */
			return sys_ioctl(fd, TIOCSETD, (long)data);

		case 97:				/* TIOCSCTTY */
			return sys_ioctl(fd, TIOCSCTTY, (long)data);

		case 103:				/* TIOCSWINSZ */
			return sys_ioctl(fd, TIOCSWINSZ, (long)data);

		case 104:				/* TIOCGWINSZ */
			return sys_ioctl(fd, TIOCGWINSZ, (long)data);

		case 113:				/* TIOCNOTTY */
			return sys_ioctl(fd, TIOCNOTTY, (long)data);

		case 118:	 			/* TIOCSPGRP */
			return sys_ioctl(fd, TIOCSPGRP, (long)data);

		case 119:				/* TIOCGPGRP */
			return sys_ioctl(fd, TIOCGPGRP, (long)data);

		case 123:				/* TIOCSBRK */
			return sys_ioctl(fd, TCSBRK, (long)data);

		case 124:				/* TIOCLGET */
		case 125:				/* TIOCLSET */
			return 0;


		case 3:					/* TIOCMODG */
		case 4:					/* TIOCMODS */
		case 94:				/* TIOCDRAIN */
		case 95:				/* TIOCSIG */
		case 96:				/* TIOCEXT */
		case 98:				/* TIOCCONS */
		case 102:				/* TIOCUCNTL */
		case 105:				/* TIOCREMOTE */
		case 106:				/* TIOCMGET */
		case 107:				/* TIOCMBIC */
		case 108:				/* TIOCMBIS */
		case 109:				/* TIOCMSET */
		case 110:				/* TIOCSTART */
		case 111:				/* TIOCSTOP */
		case 112:				/* TIOCPKT */
		case 114:				/* TIOCSTI */
		case 115:				/* TIOCOUTQ */
		case 120:				/* TIOCCDTR */
		case 121:				/* TIOCSDTR */
		case 122:				/* TIOCCBRK */
                        break;
	}

	printk(KERN_ERR "BSD/V7: terminal ioctl 0x%08lx unsupported\n",
		(unsigned long)func);
	return -EINVAL;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(bsd_ioctl_termios);
EXPORT_SYMBOL(svr4_term_ioctl);
#endif
