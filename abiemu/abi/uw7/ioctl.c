/*
 *   abi/uw7/ioctl.c - Support for UnixWare 7.x ioctl(2) system call.
 *
 *  This module provides a function uw7_ioctl() which is called indirectly
 *  via uw7_funcs[] array, see abi/uw7/funcs.c.
 *  This software is under GPL
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/termios.h>
#include <asm/uaccess.h>

#include <asm/abi_machdep.h>

#include <abi/uw7/termbits.h>
#include <abi/uw7/termios.h>
#include <abi/svr4/ioctl.h>


#undef DEBUG

#ifdef DEBUG
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif


static int tioc_tcgets(int fd, struct uw7_termios * tios);
static int tioc_tcsets(int fd, int lnx_cmd, struct uw7_termios * tios);
static int ioctl_T(int fd, unsigned int cmd, void * arg);


int uw7_ioctl(struct pt_regs * regs)
{
	int fd;
	unsigned int cmd, class;
	void * arg;

	fd = (int)get_syscall_parameter(regs, 0);
	cmd = (unsigned int)get_syscall_parameter(regs, 1);
	arg = (void *)get_syscall_parameter(regs, 2);
	class = cmd >> 8;

	switch (class) {
	case 'T':
		return ioctl_T(fd, cmd, arg);
	default:
		return __svr4_ioctl(regs, fd, cmd, arg);
	}
}

static int tioc_tcsets(int fd, int lnx_cmd, struct uw7_termios * tios)
{
	struct termios t;
	struct uw7_termios tmp = {0, };
	mm_segment_t old_fs;
	int error;

	DBG(KERN_ERR "UW7[%d]: tioc_tcsets(%d,%x,%p)\n",
		current->pid, fd, lnx_cmd, tios);

	error = verify_area(VERIFY_READ, tios, sizeof(struct uw7_termios));
	if (error)
		return error;

	old_fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, TCGETS, (long)&t);
	set_fs(old_fs);
	if (error)
		return error;

	if (copy_from_user(&tmp, tios, sizeof(struct uw7_termios)))
		return -EFAULT;
	t.c_iflag = tmp.c_iflag & ~UW7_DOSMODE;
	t.c_oflag = tmp.c_oflag;
	t.c_cflag = tmp.c_cflag;
	t.c_lflag = tmp.c_lflag & ~UW7_DEFECHO;
	if (tmp.c_lflag & UW7_FLUSHO)
		t.c_lflag |= FLUSHO;
	else
		t.c_lflag &= ~FLUSHO;

	DBG(KERN_ERR
		"UW7[%d]: iflag: %lx->%lx, oflag: %lx->%lx, cflag: %lx->%lx, lflag: %lx->%lx\n",
		current->pid, tmp.c_iflag, t.c_iflag, tmp.c_oflag, t.c_oflag,
		tmp.c_cflag, t.c_cflag, tmp.c_lflag, t.c_lflag);

	t.c_cc[VINTR] = tmp.c_cc[UW7_VINTR];
	t.c_cc[VQUIT] = tmp.c_cc[UW7_VQUIT];
	t.c_cc[VERASE] = tmp.c_cc[UW7_VERASE];
	t.c_cc[VKILL] = tmp.c_cc[UW7_VKILL];
	t.c_cc[VEOL2] = tmp.c_cc[UW7_VEOL2];
	t.c_cc[VSWTC] = tmp.c_cc[UW7_VSWTCH];
	t.c_cc[VSTART] = tmp.c_cc[UW7_VSTART];
	t.c_cc[VSTOP] = tmp.c_cc[UW7_VSTOP];
	t.c_cc[VSUSP] = tmp.c_cc[UW7_VSUSP];
	t.c_cc[VREPRINT] = tmp.c_cc[UW7_VREPRINT];
	t.c_cc[VDISCARD] = tmp.c_cc[UW7_VDISCARD];
	t.c_cc[VWERASE] = tmp.c_cc[UW7_VWERASE];
	t.c_cc[VLNEXT] = tmp.c_cc[UW7_VLNEXT];
	if (t.c_lflag & ICANON) {
		t.c_cc[VEOF] = tmp.c_cc[UW7_VEOF];
		t.c_cc[VEOL] = tmp.c_cc[UW7_VEOL];
	} else {
		t.c_cc[VMIN] = tmp.c_cc[UW7_VMIN];
		t.c_cc[VTIME] = tmp.c_cc[UW7_VTIME];
		t.c_cc[VEOL] = tmp.c_cc[UW7_VEOL2];
	}


	DBG(KERN_ERR
		"UW7[%d]: "
		"VINTR: %x->%x, VQUIT: %x->%x, VERASE: %x->%x, VKILL: %x->%x\n"
		"VEOL2: %x->%x\n",
		current->pid, tmp.c_cc[UW7_VINTR], t.c_cc[VINTR],
		tmp.c_cc[UW7_VQUIT], t.c_cc[VQUIT],
		tmp.c_cc[UW7_VERASE], t.c_cc[VERASE],
		tmp.c_cc[UW7_VKILL], t.c_cc[VKILL],
		tmp.c_cc[UW7_VEOL2], t.c_cc[VEOL2]);

	old_fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, lnx_cmd, (long)&t);
	set_fs(old_fs);
	return error;
}

static int tioc_tcgets(int fd, struct uw7_termios * tios)
{
	struct termios t;
	struct uw7_termios tmp = { 0 };
	mm_segment_t old_fs;
	int error;

	DBG(KERN_ERR "UW7[%d]: tioc_tcgets(%d,%p)\n", current->pid, fd, tios);

	old_fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, TCGETS, (long)&t);
	set_fs(old_fs);
	if (error)
		return error;

	tmp.c_iflag = UW7_IFLAG_MSK & (t.c_iflag & ~UW7_DOSMODE);
	tmp.c_oflag = UW7_OFLAG_MSK & t.c_oflag;
	tmp.c_cflag = UW7_CFLAG_MSK & t.c_cflag;
	tmp.c_lflag = UW7_LFLAG_MSK & (t.c_lflag & ~UW7_DEFECHO);
	if (t.c_lflag & FLUSHO)
		tmp.c_lflag |= UW7_FLUSHO;
	else
		tmp.c_lflag &= ~UW7_FLUSHO;

	DBG(KERN_ERR
		"UW7[%d]: iflag: %lx->%lx, oflag: %lx->%lx, cflag: %lx->%lx, lflag: %lx->%lx\n",
		current->pid, tmp.c_iflag, t.c_iflag, tmp.c_oflag, t.c_oflag,
		tmp.c_cflag, t.c_cflag, tmp.c_lflag, t.c_lflag);

	if (t.c_lflag & ICANON) {
		tmp.c_cc[UW7_VEOF] = t.c_cc[VEOF];
		tmp.c_cc[UW7_VEOL] = t.c_cc[VEOL];
	} else {
		tmp.c_cc[UW7_VMIN] = t.c_cc[VMIN];
		tmp.c_cc[UW7_VTIME] = t.c_cc[VTIME];
	}

	tmp.c_cc[UW7_VINTR] = t.c_cc[VINTR];
	tmp.c_cc[UW7_VQUIT] = t.c_cc[VQUIT];
	tmp.c_cc[UW7_VERASE] = t.c_cc[VERASE];
	tmp.c_cc[UW7_VKILL] = t.c_cc[VKILL];
	tmp.c_cc[UW7_VEOL2] = t.c_cc[VEOL2];
	tmp.c_cc[UW7_VSWTCH] = t.c_cc[VSWTC];
	tmp.c_cc[UW7_VSTART] = t.c_cc[VSTART];
	tmp.c_cc[UW7_VSTOP] = t.c_cc[VSTOP];
	tmp.c_cc[UW7_VSUSP] = tmp.c_cc[UW7_VDSUSP] = t.c_cc[VSUSP];
	tmp.c_cc[UW7_VREPRINT] = t.c_cc[VREPRINT];
	tmp.c_cc[UW7_VDISCARD] = t.c_cc[VDISCARD];
	tmp.c_cc[UW7_VWERASE] = t.c_cc[VWERASE];
	tmp.c_cc[UW7_VLNEXT] = t.c_cc[VLNEXT];

	return copy_to_user(tios, &tmp, sizeof(struct uw7_termios)) ? -EFAULT : 0;
}

static int ioctl_T(int fd, unsigned int cmd, void * arg)
{
	DBG(KERN_ERR "ioctl_T(%d,%x,%p)\n", fd, cmd, arg);

	switch (cmd) {
		case UW7_TCSBRK:
			return sys_ioctl(fd, TCSBRK, (long)arg);

		case UW7_TCXONC:
			return sys_ioctl(fd, TCXONC, (long)arg);

		case UW7_TCFLSH:
			return sys_ioctl(fd, TCFLSH, (long)arg);

		case UW7_TIOCSWINSZ:
			return sys_ioctl(fd, TIOCSWINSZ, (long)arg);

		case UW7_TIOCGWINSZ:
			return sys_ioctl(fd, TIOCGWINSZ, (long)arg);

		case UW7_TCGETS:
			return tioc_tcgets(fd, arg);

		case UW7_TCSETS:
			return tioc_tcsets(fd, TCSETS, arg);

		case UW7_TCSETSW:
			return tioc_tcsets(fd, TCSETSW, arg);

		case UW7_TCSETSF:
			return tioc_tcsets(fd, TCSETSF, arg);
	}
	return -EINVAL;
}
