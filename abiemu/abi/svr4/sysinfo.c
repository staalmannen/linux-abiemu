/*
 * Copyright (C) 1995	Eric Youngdale
 */

#ident "%W% %G%"

#include <linux/module.h>
#include <linux/version.h>
#include <linux/compile.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/utsname.h>
#include <asm/uaccess.h>

#include <abi/util/trace.h>


#define __O_SI_SYSNAME          1       /* return name of operating system */
#define __O_SI_HOSTNAME         2       /* return name of node */
#define SI_RELEASE              3       /* return release of operating system */
#define SI_VERSION              4       /* return version field of utsname */
#define __O_SI_MACHINE          5       /* return kind of machine */
#define __O_SI_ARCHITECTURE     6       /* return instruction set arch */
#define SI_HW_SERIAL            7       /* return hardware serial number */
#define __O_SI_HW_PROVIDER      8       /* return hardware manufacturer */
#define SI_SRPC_DOMAIN          9       /* return secure RPC domain */
#define SI_INITTAB_NAME         10      /* return name of inittab file used */
#define SI_ARCHITECTURE         100     /* return instruction set arch */
#define SI_BUSTYPES             101     /* return list of bus types */
#define SI_HOSTNAME             102     /* return fully-qualified node name */
#define SI_HW_PROVIDER          103     /* return hardware manufacturer */
#define SI_KERNEL_STAMP         104     /* return kernel generation timestamp */
#define SI_MACHINE              105     /* return kind of machine */
#define SI_OS_BASE              106     /* return base operating system */
#define SI_OS_PROVIDER          107     /* return operating system provider */
#define SI_SYSNAME              108     /* return name of operating system */
#define SI_USER_LIMIT           109     /* return maximum number of users */



int svr4_sysinfo(int cmd, char * buf, long count)
{
	char * return_string;
	static unsigned int serial_number = 0;
	char buffer[16];
	int error;
	int slen;

	return_string = NULL;

	switch(cmd) {
		case __O_SI_SYSNAME:
		case SI_SYSNAME:
			return_string = system_utsname.sysname;
			break;
		case __O_SI_HOSTNAME:
		case SI_HOSTNAME:
			return_string = system_utsname.nodename;
			break;
		case SI_VERSION:
			return_string = "2";
			break;
		case SI_RELEASE:
			return_string = system_utsname.release;
			break;
		case SI_MACHINE:
		case __O_SI_MACHINE:
			return_string = system_utsname.machine;
			break;
		case __O_SI_ARCHITECTURE:
		case SI_ARCHITECTURE:
			return_string = "IA32"; /* XXX: this seems wrong, the name ia32 is very new ... -- ch */
			break;
		case SI_BUSTYPES:
			return_string = "PCI ISA";
			break;
		case __O_SI_HW_PROVIDER:
		case SI_HW_PROVIDER:
			return_string = "Generic AT";
			break;
		case SI_KERNEL_STAMP:
			return_string = UTS_VERSION;
			break;
		case SI_INITTAB_NAME:
			return_string = "/etc/inittab";
			break;
		case SI_HW_SERIAL:
			if(serial_number == 0)
				serial_number = 0xdeadbeef;
			sprintf(buffer,"%8.8x", serial_number);
			return_string = buffer;
			break;
		case SI_OS_BASE:
			return_string = "Linux";
			break;
		case SI_OS_PROVIDER:
			return_string = "LBT"; /* someone's initials ? */
			break;
		case SI_SRPC_DOMAIN:
			return_string = system_utsname.domainname;
			break;
		case SI_USER_LIMIT:
			/* have you seen a Linux box with more than 500000 users? */
			return_string = "500000";
			break;
		default:
#if defined(CONFIG_ABI_TRACE)
			abi_trace(ABI_TRACE_API,
					"unsupported sysinfo call %d\n", cmd);
#endif
		return -EINVAL;
	}

	if (!return_string)
		return 0;

	down_read(&uts_sem);
	slen = (count < strlen(return_string) + 1 ? count : strlen(return_string) + 1);
	error = copy_to_user(buf, return_string, slen);
	up_read(&uts_sem);

	return error ? -EFAULT : slen;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_sysinfo);
#endif
