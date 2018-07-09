/*
 *     abi/uw7/context.c
 *
 *  This software is under GPL
 */


#include <linux/ptrace.h>
#include <linux/errno.h>
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <asm/abi_machdep.h>
#include <abi/uw7/context.h>


int
uw7_sigaltstack(const uw7_stack_t *uw7_ss, uw7_stack_t *uw7_oss)
{
	stack_t ss, oss, *ssp = NULL, *ossp = NULL;
	int error;
	mm_segment_t old_fs;

	if (uw7_ss) {
		error = verify_area(VERIFY_READ, uw7_ss, sizeof(uw7_stack_t));
		if (error)
			return error;
		__get_user(ss.ss_sp, &uw7_ss->ss_sp);
		__get_user(ss.ss_size, &uw7_ss->ss_size);
		__get_user(ss.ss_flags, &uw7_ss->ss_flags);
		ssp = &ss;
	}

	if (uw7_oss) {
		error = verify_area(VERIFY_WRITE, uw7_oss, sizeof(uw7_stack_t));
		if (error)
			return error;
		__get_user(oss.ss_sp, &uw7_oss->ss_sp);
		__get_user(oss.ss_size, &uw7_oss->ss_size);
		__get_user(oss.ss_flags, &uw7_oss->ss_flags);
		ossp = &oss;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	error = sys_sigaltstack(ssp, ossp);
	set_fs(old_fs);

	if (ossp) {
		__put_user(ossp->ss_sp, &uw7_oss->ss_sp);
		__put_user(ossp->ss_size, &uw7_oss->ss_size);
		__put_user(ossp->ss_flags, &uw7_oss->ss_flags);
	}
	return error;
}

static int
getcontext(uw7_context_t * uc, struct pt_regs * regs)
{
	uw7_context_t tmp = { 0 };

	return copy_to_user(uc, &tmp, sizeof(uw7_context_t)) ? -EFAULT : 0;
}

static int
getxcontext(uw7_context_t * uc, struct pt_regs * regs)
{
	return 0;
}

static int
setcontext(uw7_context_t * uc, struct pt_regs * regs)
{
	if (!uc) /* SVR4 says setcontext(NULL) => exit(0) */
		sys_exit(0);
	return 0;
}

int
uw7_context(struct pt_regs * regs)
{
	int fcn = get_syscall_parameter(regs, 0);
	uw7_context_t * uc = (uw7_context_t *) get_syscall_parameter(regs, 1);

	switch (fcn) {
		case UW7_GETCONTEXT:
			return getcontext(uc, regs);

		case UW7_GETXCONTEXT:
			return getxcontext(uc, regs);

		case UW7_SETCONTEXT:
			return setcontext(uc, regs);
	}
	return -EINVAL;
}
