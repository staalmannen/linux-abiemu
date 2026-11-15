// SPDX-License-Identifier: GPL-2.0
/*
 * abi_bsd - ioctl() and termios translation
 *
 * The ioctl encoding (_IOR/_IOW with group and number) and struct
 * termios are identical on FreeBSD, NetBSD and OpenBSD.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/termios.h>
#include <linux/sockios.h>
#include <asm/unistd.h>
#include <asm/ioctls.h>

#include <abiemu/bsd.h>

#define BSD_NCCS	20

struct bsd_termios {
	u32	c_iflag;
	u32	c_oflag;
	u32	c_cflag;
	u32	c_lflag;
	u8	c_cc[BSD_NCCS];
	u32	c_ispeed;
	u32	c_ospeed;
};

static const struct abi_flagmap bsd_iflags[] = {
	{ 0x0001, IGNBRK }, { 0x0002, BRKINT }, { 0x0004, IGNPAR },
	{ 0x0008, PARMRK }, { 0x0010, INPCK }, { 0x0020, ISTRIP },
	{ 0x0040, INLCR }, { 0x0080, IGNCR }, { 0x0100, ICRNL },
	{ 0x0200, IXON }, { 0x0400, IXOFF }, { 0x0800, IXANY },
	{ 0x2000, IMAXBEL }, { 0x4000, IUTF8 }, { 0 }
};

static const struct abi_flagmap bsd_oflags[] = {
	{ 0x0001, OPOST }, { 0x0002, ONLCR }, { 0x0004, XTABS },
	{ 0x0010, OCRNL }, { 0x0020, ONOCR }, { 0x0040, ONLRET }, { 0 }
};

static const struct abi_flagmap bsd_cflags[] = {
	{ 0x0300, CS8 }, { 0x0400, CSTOPB }, { 0x0800, CREAD },
	{ 0x1000, PARENB }, { 0x2000, PARODD }, { 0x4000, HUPCL },
	{ 0x8000, CLOCAL }, { 0x30000, CRTSCTS }, { 0 }
};

static const struct abi_flagmap bsd_lflags[] = {
	{ 0x0001, ECHOKE }, { 0x0002, ECHOE }, { 0x0004, ECHOK },
	{ 0x0008, ECHO }, { 0x0010, ECHONL }, { 0x0020, ECHOPRT },
	{ 0x0040, ECHOCTL }, { 0x0080, ISIG }, { 0x0100, ICANON },
	{ 0x0400, IEXTEN }, { 0x0800, EXTPROC }, { 0x400000, TOSTOP },
	{ 0x800000, FLUSHO }, { 0x20000000, PENDIN },
	{ 0x80000000, NOFLSH }, { 0 }
};

/* index: BSD c_cc slot, value: Linux slot (or -1) */
static const signed char bsd_cc_map[BSD_NCCS] = {
	VEOF, VEOL, VEOL2, VERASE, VWERASE, VKILL, VREPRINT, -1,
	VINTR, VQUIT, VSUSP, -1, VSTART, VSTOP, VLNEXT, VDISCARD,
	VMIN, VTIME, -1, -1,
};

static void bsd_termios_from_linux(const struct termios2 *l,
				   struct bsd_termios *b)
{
	int i;

	memset(b, 0, sizeof(*b));
	b->c_iflag = abi_flags_from_linux(bsd_iflags, l->c_iflag);
	b->c_oflag = abi_flags_from_linux(bsd_oflags, l->c_oflag);
	b->c_cflag = abi_flags_from_linux(bsd_cflags, l->c_cflag & ~CSIZE);
	b->c_cflag |= ((l->c_cflag & CSIZE) / CS6) << 8;
	b->c_lflag = abi_flags_from_linux(bsd_lflags, l->c_lflag);
	for (i = 0; i < BSD_NCCS; i++) {
		int li = bsd_cc_map[i];
		bool raw = li == VMIN || li == VTIME;

		if (li < 0)
			b->c_cc[i] = 0xff;
		else
			b->c_cc[i] = (!l->c_cc[li] && !raw) ? 0xff : l->c_cc[li];
	}
	b->c_ispeed = l->c_ispeed;
	b->c_ospeed = l->c_ospeed;
}

static void bsd_termios_to_linux(const struct bsd_termios *b,
				 struct termios2 *l)
{
	int i;

	memset(l, 0, sizeof(*l));
	l->c_iflag = abi_flags_to_linux(bsd_iflags, b->c_iflag);
	l->c_oflag = abi_flags_to_linux(bsd_oflags, b->c_oflag);
	l->c_cflag = abi_flags_to_linux(bsd_cflags, b->c_cflag & ~0x300);
	l->c_cflag |= ((b->c_cflag & 0x300) >> 8) * CS6;
	l->c_cflag |= BOTHER;
	l->c_lflag = abi_flags_to_linux(bsd_lflags, b->c_lflag);
	for (i = 0; i < BSD_NCCS; i++) {
		int li = bsd_cc_map[i];
		bool raw = li == VMIN || li == VTIME;

		if (li >= 0)
			l->c_cc[li] = (b->c_cc[i] == 0xff && !raw) ? 0 : b->c_cc[i];
	}
	l->c_ispeed = b->c_ispeed ?: 38400;
	l->c_ospeed = b->c_ospeed ?: 38400;
}

static long bsd_tiocgeta_post(struct abi_ctx *ctx, long ret)
{
	struct bsd_termios b;
	struct termios2 l;

	if (ret)
		return ret;
	if (copy_from_user(&l, (void __user *)ctx->priv[0], sizeof(l)))
		return -EFAULT;
	bsd_termios_from_linux(&l, &b);
	return copy_to_user((void __user *)ctx->args[2], &b, sizeof(b)) ?
		-EFAULT : 0;
}

static long bsd_tiocseta(struct abi_ctx *ctx, unsigned int lcmd)
{
	struct termios2 __user *gl = abi_gap(ctx, sizeof(*gl));
	struct bsd_termios b;
	struct termios2 l;

	if (copy_from_user(&b, (void __user *)ctx->args[2], sizeof(b)))
		return -EFAULT;
	bsd_termios_to_linux(&b, &l);
	if (copy_to_user(gl, &l, sizeof(l)))
		return -EFAULT;
	return abi_native3(ctx, __NR_ioctl, ctx->args[0], lcmd,
			   (unsigned long)gl);
}

/* BSD ioctl numbers that map 1:1 to a Linux request */
static const struct {
	u32		bsd;
	unsigned int	native;
} bsd_ioctl_map[] = {
	{ 0x40087468, TIOCGWINSZ },
	{ 0x80087467, TIOCSWINSZ },
	{ 0x40047477, TIOCGPGRP },
	{ 0x80047476, TIOCSPGRP },
	{ 0x40047473, TIOCOUTQ },
	{ 0x2000740d, TIOCEXCL },
	{ 0x2000740e, TIOCNXCL },
	{ 0x20007471, TIOCNOTTY },
	{ 0x4004667f, FIONREAD },
	{ 0x8004667e, FIONBIO },
	{ 0x8004667d, FIOASYNC },
	{ 0x20006601, FIOCLEX },
	{ 0x20006602, FIONCLEX },
	{ 0x40047307, SIOCATMARK },
};

long bsd_sys_ioctl(struct abi_ctx *ctx)
{
	unsigned long fd = ctx->args[0], arg = ctx->args[2];
	u32 cmd = ctx->args[1];
	int i;

	switch (cmd) {
	case 0x402c7413: {	/* TIOCGETA */
		void __user *gl = abi_gap(ctx, sizeof(struct termios2));

		ctx->priv[0] = (unsigned long)gl;
		ctx->post = bsd_tiocgeta_post;
		return abi_native3(ctx, __NR_ioctl, fd, TCGETS2,
				   (unsigned long)gl);
	}
	case 0x802c7414:	/* TIOCSETA */
		return bsd_tiocseta(ctx, TCSETS2);
	case 0x802c7415:	/* TIOCSETAW */
		return bsd_tiocseta(ctx, TCSETSW2);
	case 0x802c7416:	/* TIOCSETAF */
		return bsd_tiocseta(ctx, TCSETSF2);
	case 0x20007461:	/* TIOCSCTTY */
		return abi_native3(ctx, __NR_ioctl, fd, TIOCSCTTY, 0);
	case 0x2000745e:	/* TIOCDRAIN */
		return abi_native3(ctx, __NR_ioctl, fd, TCSBRK, 1);
	case 0x80047410:	/* TIOCFLUSH */
		return abi_native3(ctx, __NR_ioctl, fd, TCFLSH, TCIOFLUSH);
	case 0x2000746e:	/* TIOCSTART */
		return abi_native3(ctx, __NR_ioctl, fd, TCXONC, TCOON);
	case 0x2000746f:	/* TIOCSTOP */
		return abi_native3(ctx, __NR_ioctl, fd, TCXONC, TCOOFF);
	}

	for (i = 0; i < ARRAY_SIZE(bsd_ioctl_map); i++)
		if (bsd_ioctl_map[i].bsd == cmd)
			return abi_native3(ctx, __NR_ioctl, fd,
					   bsd_ioctl_map[i].native, arg);

	if (abi_trace >= ABI_TRACE_UNIMPL)
		pr_info("abiemu: %s[%d]: unsupported ioctl 0x%08x ('%c' %u)\n",
			ctx->pers->name, task_pid_nr(current), cmd,
			(cmd >> 8) & 0xff ?: '?', cmd & 0xff);
	return -ENOTTY;
}
EXPORT_SYMBOL_GPL(bsd_sys_ioctl);
