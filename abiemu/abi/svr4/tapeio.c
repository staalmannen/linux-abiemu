#ident "%W% %G%"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mtio.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>


int
svr4_tape_ioctl(int fd, u_int cmd, caddr_t data)
{
	mm_segment_t		fs;
	int			error;
	struct mtop		mtop;

	mtop.mt_count = 1;

	switch (cmd & 0xff) {
	case 1:		/* MT_RETEN */
		mtop.mt_op = MTRETEN;
		break;
	case 2:		/* MT_REWIND */
		mtop.mt_op = MTREW;
		break;
	case 3:		/* MT_ERASE */
		mtop.mt_op = MTERASE;
		break;
	case 4:		/* MT_WFM */
		mtop.mt_op = MTWEOF;
		break;
	case 5:		/* MT_RESET */
		mtop.mt_op = MTRESET;
		break;
	case 7:		/* T_SFF */
		mtop.mt_op = MTFSF;
		break;
	case 8:		/* T_SBF */
		mtop.mt_op = MTBSF;
		break;
	case 9:		/* T_LOAD */
		mtop.mt_op = MTLOAD;
		break;
	case 10:  /* MT_UNLOAD */
		mtop.mt_op = MTOFFL;
		break;
	case 15:	/* T_WRBLKLEN */
		mtop.mt_op = MTLOCK;
		mtop.mt_count = (int)data;
		break;
	case 16:	/* T_PREVMV */
		mtop.mt_op = MTLOCK;
		break;
	case 17:	/* T_ALLOMV */
		mtop.mt_op = MTUNLOCK;
		break;
	case 20:	/* T_EOD */
		mtop.mt_op = MTEOM;
		mtop.mt_count = (int)data;
		break;
	case 21:	/* T_SSFB */
		mtop.mt_op = MTBSFM;
		mtop.mt_count = (int)data;
		break;
	case 22:	/* T_SSFF */
		mtop.mt_op = MTFSFM;
		mtop.mt_count = (int)data;
		break;
	case 24:	/* T_STD */
		mtop.mt_op = MTSETDENSITY;
		mtop.mt_count = (int)data;
		break;

#if 0
	case 6:		/* T_STATUS */
	case 14:	/* T_RDBLKLEN */
	case 18:	/* T_SBB */
	case 19:	/* T_SFB */
	case 23:	/* T_STS */
#endif
	default:
		printk (KERN_ERR "iBCS: SYSV tape ioctl func=%d arg=%x unsupported\n",
				cmd & 0xff, (int)data);
			return -EINVAL;
	}

	fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, MTIOCTOP, (long)&mtop);
	set_fs (fs);
	return (error);
}
