#ident "%W% %G%"

#include <linux/kernel.h>
#include <linux/mtio.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <asm/uaccess.h>


int
sco_tape_ioctl(int fd, u_int cmd, caddr_t data)
{
	struct mtop		mtop;
	mm_segment_t		fs;
	int			error;

	mtop.mt_count = 1;

	switch (cmd & 0xff) {
	case 1:  /* MT_RESET */
		mtop.mt_op = MTRESET;
		break;

	case 2:  /* MT_RETEN */
		mtop.mt_op = MTRETEN;
		break;

	case 3:  /* MT_REWIND */
		mtop.mt_op = MTREW;
		break;

	case 4:  /* MT_ERASE */
	case 23:  /* HP_ERASE */
		mtop.mt_op = MTERASE;
		break;

	case 6:  /* MT_RFM */
		mtop.mt_op = MTFSF;
		break;

	case 7:  /* MT_WFM */
		mtop.mt_op = MTWEOF;
		break;

	case 8:  /* MT_LOAD */
		mtop.mt_op = MTLOAD;
		break;

	case 9:  /* MT_UNLOAD */
		mtop.mt_op = MTOFFL;
		break;

	case 19:  /* MT_RSM */
		mtop.mt_op = MTFSS;
		break;

	case 20:  /* MT_WSM */
		mtop.mt_op = MTWSM;
		break;

	case 21:  /* MT_EOD */
		mtop.mt_op = MTEOM;
		break;

	case 24:  /* MT_SETBLK */
		mtop.mt_op = MTSETBLK;
		mtop.mt_count = (int)data;
		break;

	case 25:  /* MT_LOCK */
		mtop.mt_op = MTLOCK;
		break;

	case 26:  /* MT_UNLOCK */
		mtop.mt_op = MTUNLOCK;
		break;


#if 0
/*
	The following function codes are just copied from the SCO
	include file.
*/

	case 0:  /* MT_STATUS */
	case 5:  /* MT_AMOUNT */
	case 10:  /* MT_DSTATUS */
	case 11:  /* MT_FORMAT */
	case 12:  /* MT_GETHDR */
	case 13:  /* MT_PUTHDR */
	case 14:  /* MT_GETNEWBB */
	case 15:  /* MT_PUTNEWBB */
	case 16:  /* MT_GETVTBL */
	case 17:  /* MT_PUTVTBL */
	case 18:  /* MT_SERVO */
	case 22:  /* MT_FORMPART */
	case 38:  /* MT_SETANSI */
	case 64:  /* MT_REPORT */
#endif
	default:
		printk (KERN_ERR "abi: SCO tape ioctl func=%d arg=%x unsupported\n",
			cmd & 0xff, (int)data);
		return -EINVAL;
	}

	fs = get_fs();
	set_fs(get_ds());
	error = sys_ioctl(fd, MTIOCTOP, (long)&mtop);
	set_fs(fs);
	return (error);
}
