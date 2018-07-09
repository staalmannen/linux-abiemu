/*
 * Copyright (C) 1994  Mike Jagdis (jaggy@purplet.demon.co.uk)
 */

#ident "%W% %G%"

#include <linux/module.h>
#include <linux/personality.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <abi/util/trace.h>

/*
 * The kernel sysfs() code is almost all we need but, apparently,
 * the SCO (at least) sysfs() will also accept a "magic number"
 * as an index argument and will return the name of the relevant
 * file system. Since Linux doesn't have any concept of fs magic
 * numbers outside the file system code themselves there is no
 * clean way to do it in the kernel. There isn't a clean way to
 * to it here either but it needs to be done somehow :-(.
 */
enum {
	GETFSIND = 1,
	GETFSTYP = 2,
	GETNFSTYP = 3
};

int
svr4_sysfs(int cmd, int arg1, int arg2)
{
	if (cmd == GETFSIND)
		return sys_sysfs(cmd, arg1, arg2);

	if (cmd == GETNFSTYP)
		return sys_sysfs(cmd, arg1, arg2);

	if (cmd == GETFSTYP) {
		char *buf = (char *)arg2;
		int error;

		if (arg1 & 0x80000000)
			arg1 &= 0x0000ffff;
		if (arg1 >= 0 && arg1 < sys_sysfs(GETNFSTYP,0,0))
			return sys_sysfs(cmd, arg1-1, arg2);

		/*
		 * Kludge alert! Hardcoded known magic numbers!
		 */
		switch (arg1) {
		case 0xef53: case 0xffffef53:
		case 0xef51: case 0xffffef51:
			/*
			 * Some SCO programs (i.e. Informix Dynamic
			 * Server are using this to detect "real"
			 * filesystems by checking type names :-(.
			 * So we lie :-).
			 */
			if (is_cur_personality(PER_SCOSVR3))
				error = copy_to_user(buf, "HTFS", 5);
			else
				error = copy_to_user(buf, "ext2", 5);
			break;
		case 0x137d:
			error = copy_to_user(buf, "ext", 4);
			break;
		case 0x9660: case 0xffff9660:
			error = copy_to_user(buf, "iso9660", 8);
			break;
		case 0x4d44:
			error = copy_to_user(buf, "msdos", 6);
			break;
		case 0x6969:
			error = copy_to_user(buf, "nfs", 4);
			break;
		case 0x9fa0: case 0xffff9fa0:
			error = copy_to_user(buf, "proc", 5);
			break;
		case 0xf995e849:
		case 0xe849: case 0xffffe849:
			error = copy_to_user(buf, "hpfs", 5);
			break;
		case 0x137f: /* original */
		case 0x138f: /* original + 30 char names */
		case 0x2468: /* V2 */
		case 0x2478: /* V2 + 30 char names */
			error = copy_to_user(buf, "minix", 6);
			break;
		case 0x564c:
			error = copy_to_user(buf, "ncpfs", 6);
			break;
		case 0x517b:
			error = copy_to_user(buf, "smbfs", 6);
			break;
		case 0x00011954:
			error = copy_to_user(buf, "ufs", 4);
			break;
		case 0x012fd16d: case 0xffffd16d:
			error = copy_to_user(buf, "xiafs", 6);
			break;
		case 0x012ff7b3+1: case 0xfffff7b3+1:
			error = copy_to_user(buf, "xenix", 6);
			break;
		case 0x012ff7b3+2: case 0xfffff7b3+2:
		case 0x012ff7b3+3: case 0xfffff7b3+3:
			error = copy_to_user(buf, "sysv", 5);
			break;
		case 0x012ff7b3+4: case 0xfffff7b3+4:
			error = copy_to_user(buf, "coherent", 9);
			break;
		default:
			error = copy_to_user(buf, "", 1);
			break;
		}

		if (error)
			return -EFAULT;
		return 0;
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "unsupported sysfs call %d\n", cmd);
#endif
	return -EINVAL;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_sysfs);
#endif
