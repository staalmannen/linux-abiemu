/*
 * This provides internal emulation support for the SCO <sys/vtkd.h> on
 * the multiscreen console. More or less, this involves translating the
 * input ioctl()'s into a similar Linux ioctl()'s.
 *
 * Not emulated SCO multiscreen functions:
 *   None.
 *
 * Not emulated SCO keyboard functions:
 *   KIOCDOSMODE		set DOSMODE
 *   KIOCNONDOSMODE		unset DOSMODE
 *   KDDISPINFO			get display start and size
 *   KDGKBSTATE			get state of keyboard shift keys
 *
 * Written by Scott Michel, scottm@intime.com
 * (c) 1994 Scott Michel as part of the Linux iBCS-2 emulator project.
 */

#ident "%W% %G%"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>


static struct {
	int	in;		/* only lower 8 bits */
	int	out;		/* Linux version */
} trantab[] = {
#ifdef KDDISPTYPE
	{ 1,  KDDISPTYPE   },
#endif
	{ 2,  KDMAPDISP    },
	{ 3,  KDUNMAPDISP  },
	{ 6,  KDGKBMODE    },
	{ 7,  KDSKBMODE    },
	{ 8,  KDMKTONE     },
	{ 9,  KDGETMODE    },
	{ 10, KDSETMODE    },
	{ 11, KDADDIO      },
	{ 12, KDDELIO      },
	{ 60, KDENABIO     },
	{ 61, KDDISABIO    },
#ifdef KIOCINFO
	{ 62, KIOCINFO     },
#endif
	{ 63, KIOCSOUND    },
	{ 64, KDGKBTYPE    },
	{ 65, KDGETLED     },
	{ 66, KDSETLED     },
};


int
sco_vtkbd_ioctl(int fd, u_int cmd, caddr_t data)
{
	u_int			gen = (cmd >> 8) & 0xff;
	u_int			spec = cmd & 0xff;
	int			newf, i;

	switch (gen) {
	case 'V':
		/*
		 * Could make this translation process table based, but, why
		 * waste the valuable kernel space ?
		 */

		newf = (spec == 1 ? VT_OPENQRY :
			(spec == 2 ? VT_SETMODE :
			 (spec == 3 ? VT_GETMODE :
			  (spec == 4 ? VT_RELDISP :
			   (spec == 5 ? VT_ACTIVATE : -1)))));
		if (newf != -1)
			return sys_ioctl(fd, newf, (long)data);
		break;
	case 'K':
		for (i = 0; i < ARRAY_SIZE(trantab); i++) {
			if (spec == trantab[i].in)
				return sys_ioctl(fd, trantab[i].out, (long)data);
		}
		/* FALLTHROUGH */
	}
	printk(KERN_ERR "%s: vtkd ioctl 0x%02x unsupported\n", __FILE__, cmd);
	return -EINVAL;
}
