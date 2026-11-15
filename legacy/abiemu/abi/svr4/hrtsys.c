/*
 * Copyright (C) 1994 Eric Youngdale.
 *
 * The hrtsys interface is used by SVR4, and is effectively a way of doing
 * itimer.  I do not know why this is used instead of the regular itimer
 * stuff, but it appears to be related to bsd programs/functionality.
 */

#ident "%W% %G%"

#include <linux/module.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <asm/abi_machdep.h>
#include <abi/util/trace.h>


struct hrt_time_t {
	unsigned long secs;
	unsigned long sub_sec; /* Less than one second. */
	unsigned long resolution; /* Resolution of timer */
};

struct hrtcmd {
	int cmd;
	int clk;
	struct hrt_time_t interval;
	struct hrt_time_t tod;
	int flags;
	int error;
	int reserved[3];
};

static int
ibcs_hrtcntl (struct pt_regs * regs)
{
	unsigned int param[4];
	struct timeval * tv;
	int i, error;

	for (i=0; i<4; i++)
		param[i] = get_syscall_parameter (regs, 1+i);

	if (param[0] != 1 || param[1] != 1 || param[2] != 0)
		return -EINVAL;

	tv = (struct timeval *) param[3];

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "hrtcntl(0x%lx)\n", (u_long)tv);
#endif

	error = verify_area(VERIFY_WRITE, (char *) tv,sizeof *tv);
	if (error)
		return error;

	return sys_gettimeofday(tv, NULL);
}

static int
ibcs_hrtalarm (struct pt_regs * regs)
{
	struct itimerval get_buffer;
	struct hrtcmd * hcmd;
	int i, error, cmd, retval, which;
	mm_segment_t old_fs = get_fs();

	i = get_syscall_parameter (regs, 2);
	if(i != 1)
		return -EINVAL;

	hcmd = (struct hrtcmd *) get_syscall_parameter (regs, 1);

	error = verify_area (VERIFY_WRITE, (char *) hcmd,sizeof *hcmd);
	if (error)
		return error;

	get_user (cmd, ((unsigned long *) hcmd));

	/* Now figure out which clock we want to fiddle with */
	get_user (which, ((unsigned long *) hcmd)+1);

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "hrtalarm(0x%lx %d)\n",
			(u_long)cmd, which);
#endif

	switch (which) {
		case 4:
			which = 2;
			break;
		case 2:
			which = 1;
			break;
		case 1:
			which = 0;
			break;
		default:
			return -EINVAL;
	};

	switch (cmd) {
		case 0xc:
			if(({long r; get_user(r, ((unsigned long *) hcmd)+4); r;}) != 1000000)
				return -EINVAL;
			if (copy_from_user(&get_buffer.it_value, ((unsigned long *) hcmd)+2,
				sizeof(struct timeval)))
				return -EFAULT;
			memset(&get_buffer.it_interval, 0, sizeof(struct timeval));
			set_fs(get_ds());
			retval = sys_setitimer(which, &get_buffer, NULL);
			set_fs(old_fs);
			break;
		case 0xd:
			set_fs(get_ds());
			retval = sys_getitimer(which, &get_buffer);
			set_fs(old_fs);

#if defined(CONFIG_ABI_TRACE)
			abi_trace(ABI_TRACE_API, "hrtalarm(d %lx) %lx %lx %lx %lx\n",
					(u_long)hcmd,
					get_buffer.it_interval.tv_sec,
					get_buffer.it_interval.tv_usec,
					get_buffer.it_value.tv_sec,
					get_buffer.it_value.tv_usec);
#endif

			put_user(1000000, &hcmd->interval.resolution);
			if (copy_to_user(((unsigned long *) hcmd)+2, &get_buffer.it_interval,
				sizeof(get_buffer)))
				retval = -EFAULT;
			retval = 1;
			break;
		case 0xf:
			if(({long r; get_user(r, ((unsigned long *) hcmd)+4); r;}) != 1000000)
				return -EINVAL;
			if(({long r; get_user(r, ((unsigned long *) hcmd)+7); r;}) != 1000000)
				return -EINVAL;
			if (copy_from_user(&get_buffer.it_value, &hcmd->tod,
				sizeof(struct timeval)))
				return -EFAULT;
			if (copy_from_user(&get_buffer.it_interval, &hcmd->interval,
				sizeof(struct timeval)))
				return -EFAULT;
			set_fs(get_ds());
			retval = sys_setitimer(which, &get_buffer, NULL);
			set_fs(old_fs);
			break;
		case 0x10:
			memset(&get_buffer, 0, sizeof(get_buffer));
			set_fs(get_ds());
			retval = sys_setitimer(which, &get_buffer, NULL);
			set_fs(old_fs);
			break;
		default:
			retval = -EINVAL;
	};

	return retval;
}

int
svr4_hrtsys (struct pt_regs * regs)
{
	int func, retval;

	func  = get_syscall_parameter (regs, 0);

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "hrtsys(%d)\n", func);
#endif

	switch (func) {
		case 0:
			retval = ibcs_hrtcntl(regs);
			break;
		case 1:
			retval = ibcs_hrtalarm(regs);
			break;
		case 2:
		case 3:
		default:
			retval = -EINVAL;
	}

	return retval;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_hrtsys);
#endif
