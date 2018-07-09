/*
 * Copyright (C) 1995	Mike Jagdis
 */

#ident "%W% %G%"

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/fcntl.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/statfs.h>
#include <linux/sys.h>
#include <linux/slab.h>
#include <linux/personality.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <asm/uaccess.h>

#include <asm/abi_machdep.h>
#include <abi/svr4/sigset.h>
#include <abi/svr4/siginfo.h>

#include <abi/util/trace.h>


/*
 * Interactive SVR4's /bin/sh calls access(... 011) but Linux returns
 * EINVAL if the access mode has any other bits than 007 set.
 */

int
svr4_access(char *path, int mode)
{
	return sys_access(path, mode & 007);
}


enum {
	SVR4_CLD_EXITED = 1,
	SVR4_CLD_KILLED	= 2,
	SVR4_CLD_DUMPED	= 3,
	SVR4_CLD_TRAPPED = 4,
	SVR4_CLD_STOPPED = 5,
	SVR4_CLD_CONTINUED = 6
};

int
svr4_waitid(int idtype, int id, struct svr4_siginfo *infop, int options)
{
	long result, kopt;
	mm_segment_t old_fs;
	int pid, status;

	switch (idtype) {
		case 0: /* P_PID */
			pid = id;
			break;

		case 1: /* P_PGID */
			pid = -id;
			break;

		case 7: /* P_ALL */
			pid = -1;
			break;

		default:
			return -EINVAL;
	}

	if (infop) {
		result = verify_area(VERIFY_WRITE, infop,
					sizeof(struct svr4_siginfo));
		if (result)
			return result;
	}

	kopt = 0;
	if (options & 0100) kopt |= WNOHANG;
	if (options & 4) kopt |= WUNTRACED;

	old_fs = get_fs();
	set_fs(get_ds());
	result = sys_wait4(pid, &status, kopt, NULL);
	set_fs(old_fs);
	if (result < 0)
		return result;

	if (infop) {
		unsigned long op, st;

		put_user(current_thread_info()->exec_domain->signal_map[SIGCHLD],
			&infop->si_signo);
		put_user(result,
			&infop->_data._proc._pid);

		if ((status & 0xff) == 0) {
			/* Normal exit. */
			op = SVR4_CLD_EXITED;
			st = status >> 8;
		} else if ((status & 0xff) == 0x7f) {
			/* Stopped. */
			st = (status & 0xff00) >> 8;
			op = (st == SIGSTOP || st == SIGTSTP)
				? SVR4_CLD_STOPPED
				: SVR4_CLD_CONTINUED;
			st = current_thread_info()->exec_domain->signal_invmap[st];
		} else {
			st = (status & 0xff00) >> 8;
			op = (status & 0200)
				? SVR4_CLD_DUMPED
				: SVR4_CLD_KILLED;
			st = current_thread_info()->exec_domain->signal_invmap[st];
		}
		put_user(op, &infop->si_code);
		put_user(st, &infop->_data._proc._pdata._cld._status);
	}
	return 0;
}

int
svr4_seteuid(int uid)
{
	return sys_setreuid16(-1, uid);
}

int
svr4_setegid(int gid)
{
	return sys_setregid16(-1, gid);
}

/* POSIX.1 names */
#define _PC_LINK_MAX    1
#define _PC_MAX_CANON   2
#define _PC_MAX_INPUT   3
#define _PC_NAME_MAX    4
#define _PC_PATH_MAX    5
#define _PC_PIPE_BUF    6
#define _PC_NO_TRUNC    7
#define _PC_VDISABLE    8
#define _PC_CHOWN_RESTRICTED    9
/* POSIX.4 names */
#define _PC_ASYNC_IO    10
#define _PC_PRIO_IO     11
#define _PC_SYNC_IO     12

int
svr4_pathconf(char *path, int name)
{
	switch (name) {
		case _PC_LINK_MAX:
			/* Although Linux headers define values on a per
			 * filesystem basis there is no way to access
			 * these without hard coding fs information here
			 * so for now we use a bogus value.
			 */
			return LINK_MAX;

		case _PC_MAX_CANON:
			return MAX_CANON;

		case _PC_MAX_INPUT:
			return MAX_INPUT;

		case _PC_PATH_MAX:
			return PATH_MAX;

		case _PC_PIPE_BUF:
			return PIPE_BUF;

		case _PC_CHOWN_RESTRICTED:
			/* We should really think about this and tell
			 * the truth.
			 */
			return 0;

		case _PC_NO_TRUNC:
			/* Not sure... It could be fs dependent? */
			return 1;

		case _PC_VDISABLE:
			return 1;

		case _PC_NAME_MAX: {
			struct statfs buf;
			char *p;
			int error;
			mm_segment_t old_fs;

			p = getname(path);
			error = PTR_ERR(p);
			if (!IS_ERR(p)) {
				old_fs = get_fs();
				set_fs (get_ds());
				error = sys_statfs(p, &buf);
				set_fs(old_fs);
				putname(p);
				if (!error)
					return buf.f_namelen;
			}
			return error;
		}
	}

	return -EINVAL;
}

int
svr4_fpathconf(int fildes, int name)
{
	switch (name) {
		case _PC_LINK_MAX:
			/* Although Linux headers define values on a per
			 * filesystem basis there is no way to access
			 * these without hard coding fs information here
			 * so for now we use a bogus value.
			 */
			return LINK_MAX;

		case _PC_MAX_CANON:
			return MAX_CANON;

		case _PC_MAX_INPUT:
			return MAX_INPUT;

		case _PC_PATH_MAX:
			return PATH_MAX;

		case _PC_PIPE_BUF:
			return PIPE_BUF;

		case _PC_CHOWN_RESTRICTED:
			/* We should really think about this and tell
			 * the truth.
			 */
			return 0;

		case _PC_NO_TRUNC:
			/* Not sure... It could be fs dependent? */
			return 1;

		case _PC_VDISABLE:
			return 1;

		case _PC_NAME_MAX: {
			struct statfs buf;
			int error;
			mm_segment_t old_fs;

			old_fs = get_fs();
			set_fs (get_ds());
			error = sys_fstatfs(fildes, &buf);
			set_fs(old_fs);
			if (!error)
				return buf.f_namelen;
			return error;
		}
	}

	return -EINVAL;
}

int
svr4_sigpending(int which_routine, svr4_sigset_t *set)
{
	/* Solaris multiplexes on this one */
	/* Which routine has the actual routine that should be called */

	switch (which_routine){
	case 1:			/* sigpending */
		printk ("iBCS/Intel: sigpending not implemented\n");
		return -EINVAL;

	case 2:			/* sigfillset */
		set->setbits [0] = ~0;
		set->setbits [1] = 0;
		set->setbits [2] = 0;
		set->setbits [3] = 0;
		return 0;
	}
	return -EINVAL;
}

typedef void svr4_ucontext_t;

static int
svr4_setcontext(svr4_ucontext_t *c, struct pt_regs *regs)
{
	printk (KERN_DEBUG "Getting context\n");
	return 0;
}

static int
svr4_getcontext(svr4_ucontext_t *c, struct pt_regs *regs)
{
	printk (KERN_DEBUG "Setting context\n");
	return 0;
}

int
svr4_context(struct pt_regs *regs)
{
	int context_fn = get_syscall_parameter (regs, 0);
	struct svr4_ucontext_t *uc = (void *) get_syscall_parameter (regs, 1);

	switch (context_fn){
		case 0: /* getcontext */
			return svr4_getcontext (uc, regs);

		case 1: /* setcontext */
			return svr4_setcontext (uc, regs);
	}
	return -EINVAL;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_context);
EXPORT_SYMBOL(svr4_fpathconf);
EXPORT_SYMBOL(svr4_pathconf);
EXPORT_SYMBOL(svr4_setegid);
EXPORT_SYMBOL(svr4_seteuid);
EXPORT_SYMBOL(svr4_sigpending);
EXPORT_SYMBOL(svr4_waitid);
#endif
