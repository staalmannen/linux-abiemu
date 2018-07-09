/*
 * signal.c - signal emulation code
 *
 *  This module does not go through the normal processing routines for
 *  ibcs. The reason for this is that for most events, the return is a
 *  procedure address for the previous setting. This procedure address
 *  may be negative which is not an error. Therefore, the return processing
 *  for standard functions is skipped by declaring this routine as a "special"
 *  module for the decoder and dealing with the register settings directly.
 *
 * Please consider this closely if you plan on changing this mode.
 * -- Al Longyear
 */

#ident "%W% %G%"

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/fcntl.h>
#include <linux/personality.h>
#include <linux/fs.h>
#include <linux/sys.h>
#include <linux/signal.h>
#include <linux/syscalls.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include <abi/util/map.h>
#include <abi/util/errno.h>
#include <abi/util/trace.h>


#define SIG_HOLD	((__sighandler_t)2)	/* hold signal */

#include <abi/signal.h>
#include <abi/svr4/sigaction.h>

typedef void (*pfn) (void);     /* Completion function */

/*
 * Parameters to the signal functions have a common stack frame. This
 * defines the stack frame.
 */
#define SIGNAL_NUMBER(regp)	get_syscall_parameter((regp), 0)
#define HIDDEN_PARAM(regp)	(SIGNAL_NUMBER((regp)) & ~0xFF)
#define SECOND_PARAM(regp)	get_syscall_parameter((regp), 1)
#define THIRD_PARAM(regp)	((u_long)(regp)->edx)

/* Return a mask that includes SIG only.  */
#define __sigmask(sig)		(1 << ((sig) - 1))
#define _S(nr)			(1 << ((nr) - 1))
#define _BLOCKABLE		(~(_S(IBCS_SIGKILL) | _S(IBCS_SIGSTOP)))


void
deactivate_signal(struct task_struct *task, int signum)
{
	spin_lock_irq(&task->sighand->siglock);
	sigdelset(&task->pending.signal, signum);
	recalc_sigpending();
	spin_unlock_irq(&task->sighand->siglock);
}

/*
 *  Translate the signal number to the corresponding item for Linux.
 */
static __inline int
abi_mapsig(int sig)
{
	if ((u_int)sig >= NSIGNALS)
		return (-1);
	return (current_thread_info()->exec_domain->signal_map[sig]);
}

/*
 * Either we want this static or in a header...
 */
__inline int
abi_signo(struct pt_regs *regp, int *sigp)
{
	int			value;

	value = abi_mapsig(SIGNAL_NUMBER(regp) & 0xFF);
	if (value == -1) {
		set_error(regp, iABI_errors(EINVAL));
		return 0;
	} else {
		*sigp = value;
		return 1;
	}
}

/*
 * Process the signal() function from iBCS
 *
 * This version appeared in "Advanced Programming in the Unix Environment"
 * by W. Richard Stevens, page 298.
 */
void
abi_sig_handler(struct pt_regs *regp, int sig,
		__sighandler_t handler, int oneshot)
{
	struct sigaction	act, oact;
	mm_segment_t		fs;
	int			error;

	sigemptyset(&act.sa_mask);
	act.sa_restorer = NULL;
	act.sa_handler = handler;
	act.sa_flags = 0;

	if (oneshot)
		act.sa_flags |= SA_ONESHOT | SA_NOMASK;

	fs = get_fs();
	set_fs(get_ds());
	error = sys_rt_sigaction(sig, &act, &oact, sizeof(sigset_t));
	set_fs(fs);

	if (error < 0) {
		set_error(regp, iABI_errors(-error));
	} else {
		set_result(regp, (int)oact.sa_handler);
	}
}

/*
 * Process the signal() function from iBCS
 */
int
abi_signal(struct pt_regs *regp)
{
	__sighandler_t	vec;
	int		sig;

	if (abi_signo(regp, &sig)) {
		vec = (__sighandler_t)SECOND_PARAM(regp);
		abi_sig_handler(regp, sig, vec, 1);
	}

	return 0;
}

/*
 * Process the SVR4 sigset function.
 *
 * This is basically the same as the signal() routine with the
 * exception that it will accept a SIG_HOLD parameter.
 *
 * A SIG_HOLD will defer the processing of the signal until a sigrelse()
 * function is called or the signal handler is set again using this function.
 */
int
abi_sigset(struct pt_regs *regp)
{
	int		sig, error;
	sigset_t	newmask, oldmask;
	__sighandler_t	vec;
	mm_segment_t	fs;
	int		action;


	if (abi_signo(regp, &sig) == 0)
		return 0;

	vec = (__sighandler_t)SECOND_PARAM(regp);
	action = SIG_BLOCK;

	if (vec != SIG_HOLD) {
		action = SIG_UNBLOCK;
		deactivate_signal(current, sig);
		abi_sig_handler(regp, sig, vec, 0);
	}

	/*
	 * Process the signal hold/unhold function.
	 */
	sigemptyset(&newmask);
	sigaddset(&newmask, sig);

	fs = get_fs();
	set_fs(get_ds());
	error = sys_rt_sigprocmask(action, &newmask, &oldmask,
		sizeof(sigset_t));
	set_fs(fs);

	if (error < 0)
		set_error(regp, iABI_errors(-error));

	return 0;
}

/*
 * Process the iBCS sighold function.
 *
 * Suspend the signal from future recognition.
 */
void
abi_sighold(struct pt_regs *regp)
{
	sigset_t	newmask, oldmask;
	int		error, sig;
	mm_segment_t	fs;

	if (!abi_signo(regp, &sig))
		return;

	sigemptyset(&newmask);
	sigaddset(&newmask, sig);

	fs = get_fs();
	set_fs(get_ds());
	error = sys_rt_sigprocmask(SIG_BLOCK, &newmask,
			&oldmask, sizeof(sigset_t));
	set_fs(fs);

	if (error < 0)
		set_error(regp, iABI_errors(-error));
}

/*
 * Process the iBCS sigrelse.
 *
 * Re-enable the signal processing from a previously suspended
 * signal. This may have been done by calling the sighold() function
 * or a longjmp() during the signal processing routine. If you do a
 * longjmp() function then it is expected that you will call sigrelse
 * or set the handler again using sigset before going on with the program.
 */
void
abi_sigrelse(struct pt_regs *regp)
{
	sigset_t	newmask, oldmask;
	int		error, sig;
	mm_segment_t	fs;

	if (!abi_signo(regp, &sig))
		return;

	sigemptyset(&newmask);
	sigaddset(&newmask, sig);

	fs = get_fs();
	set_fs(get_ds());
	error = sys_rt_sigprocmask(SIG_UNBLOCK, &newmask,
			&oldmask, sizeof(sigset_t));
	set_fs(fs);

	if (error < 0)
		set_error(regp, iABI_errors(-error));
}

/*
 * Process the iBCS sigignore
 *
 * This is basically a signal (...,SIG_IGN) call.
 */
void
abi_sigignore(struct pt_regs *regp)
{
	struct sigaction	act, oact;
	int			error, sig;
	mm_segment_t		fs;

	if (!abi_signo(regp, &sig))
		return;

	sigemptyset(&act.sa_mask);

	act.sa_restorer = NULL;
	act.sa_handler = SIG_IGN;
	act.sa_flags   = 0;

	fs = get_fs();
	set_fs(get_ds());
	error = sys_rt_sigaction(sig, &act, &oact, sizeof(sigset_t));
	set_fs(fs);

	if (error < 0)
		set_error(regp, iABI_errors(-error));
}

/*
 * Process the iBCS sigpause
 *
 * Wait for the signal indicated to arrive before resuming the
 * processing. I do not know if the signal is processed first using
 * the normal event processing before the return. If someone can
 * shed some light on this then please correct this code. I block
 * the signal and look for it to show up in the pending list.
 */
void
abi_sigpause(struct pt_regs *regs)
{
	old_sigset_t		newset;
	int			error, sig;

	if (!abi_signo(regs, &sig))
		return;
	newset = (~0UL) & (1UL << (sig-1));

	if ((error = sys_sigsuspend(0, current->blocked.sig[0], newset) < 0))
		set_error(regs, iABI_errors(-error));
}

/*
 * This is the service routine for the syscall #48 (signal funcs).
 *
 * Examine the request code and branch on the request to the appropriate
 * function.
 */
int
abi_sigfunc(struct pt_regs *regp)
{
	int			sig_type = (int)HIDDEN_PARAM(regp);

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_SIGNAL|ABI_TRACE_SIGNAL_F,
		"sig%s(%ld, 0x%08lx, 0x%08lx)\n",
			sig_type == 0 ? "nal"
			: (sig_type == 0x100 ? "set"
			: (sig_type == 0x200 ? "hold"
			: (sig_type == 0x400 ? "relse"
			: (sig_type == 0x800 ? "ignore"
			: (sig_type == 0x1000 ? "pause"
			: "???" ))))),
			SIGNAL_NUMBER(regp) & 0xff,
			SECOND_PARAM(regp),
			THIRD_PARAM(regp));
#endif

	regp->eflags &= ~1;
	regp->eax     = 0;

	switch (sig_type) {
	case 0x0000:
		abi_signal(regp);
		break;
	case 0x0100:
		abi_sigset(regp);
		break;
	case 0x0200:
		abi_sighold(regp);
		break;
	case 0x0400:
		abi_sigrelse(regp);
		break;
	case 0x0800:
		abi_sigignore(regp);
		break;
	case 0x1000:
		abi_sigpause(regp);
		break;
	default:
		set_error(regp, EINVAL);

#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_SIGNAL|ABI_TRACE_SIGNAL_F,
				"sigfunc(%x, %ld, %lx, %lx) unsupported\n",
				sig_type, SIGNAL_NUMBER(regp),
				SECOND_PARAM(regp), THIRD_PARAM(regp));
#endif
		return 0;
	}

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_SIGNAL|ABI_TRACE_SIGNAL_F,
			"returns %d\n", get_result(regp));
#endif
	return 0;
}

/*
 * This function is used to handle the sigaction call from SVr4 binaries.
 *
 * If anyone else uses this, this function needs to be modified since the
 * order and size of the ibcs_sigaction structure is different in ibcs
 * and the SVr4 ABI
 */
asmlinkage int
abi_sigaction(int abi_signum, const struct abi_sigaction *action,
		struct abi_sigaction *oldaction)
{
	struct abi_sigaction	new_sa, old_sa;
	struct sigaction	nsa, osa;
	mm_segment_t		fs;
	int			error, signo;

	signo = abi_mapsig(abi_signum);
	if (signo == -1)
		return -EINVAL;

	if (oldaction) {
		error = verify_area(VERIFY_WRITE, oldaction,
				sizeof(struct abi_sigaction));
		if (error)
			return (-EFAULT);
	}

	if (action) {
		error = copy_from_user(&new_sa, action,
				sizeof(struct abi_sigaction));
		if (error)
			return (-EFAULT);
		nsa.sa_restorer = NULL;
		nsa.sa_handler = new_sa.sa_handler;
		nsa.sa_mask = map_sigvec_to_kernel(new_sa.sa_mask,
			current_thread_info()->exec_domain->signal_map);
		if (new_sa.sa_flags & ABI_SA_ONSTACK)
			nsa.sa_flags |= SA_ONSTACK;
		if (new_sa.sa_flags & ABI_SA_RESTART)
			nsa.sa_flags |= SA_RESTART;
		if (new_sa.sa_flags & ABI_SA_NODEFER)
			nsa.sa_flags |= SA_NODEFER;
		if (new_sa.sa_flags & ABI_SA_RESETHAND)
			nsa.sa_flags |= SA_RESETHAND;
		if (new_sa.sa_flags & ABI_SA_NOCLDSTOP)
			nsa.sa_flags |= SA_NOCLDSTOP;
		if (new_sa.sa_flags & ABI_SA_NOCLDWAIT)
			nsa.sa_flags |= SA_NOCLDWAIT;
	}

	fs = get_fs();
	set_fs(get_ds());
	error = sys_rt_sigaction(signo, action ? &nsa : NULL,
			oldaction ? &osa : NULL, sizeof(sigset_t));
	set_fs(fs);

	if (error || !oldaction)
		return (error);

	old_sa.sa_handler = osa.sa_handler;
	old_sa.sa_mask = map_sigvec_from_kernel(osa.sa_mask,
			current_thread_info()->exec_domain->signal_invmap);
	old_sa.sa_flags = 0;
	if (osa.sa_flags & SA_ONSTACK)
		old_sa.sa_flags |= ABI_SA_ONSTACK;
	if (osa.sa_flags & SA_RESTART)
		old_sa.sa_flags |= ABI_SA_RESTART;
	if (osa.sa_flags & SA_NODEFER)
		old_sa.sa_flags |= ABI_SA_NODEFER;
	if (osa.sa_flags & SA_RESETHAND)
		old_sa.sa_flags |= ABI_SA_RESETHAND;
	if (osa.sa_flags & SA_NOCLDSTOP)
		old_sa.sa_flags |= ABI_SA_NOCLDSTOP;
	if (osa.sa_flags & SA_NOCLDWAIT)
		old_sa.sa_flags |= ABI_SA_NOCLDWAIT;
	/*
	 * We already did the verify_area at the beginning.
	 */
	if (__copy_to_user(oldaction, &old_sa, sizeof(struct abi_sigaction)))
		return -EFAULT;
	return 0;
}


static short int howcnv[] = {SIG_SETMASK, SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK};

asmlinkage int
abi_sigprocmask(int how, u_long *abinset, u_long *abioset)
{
	sigset_t		new_set, *nset = NULL;
	sigset_t		old_set, *oset = NULL;
	u_long			new_set_abi, old_set_abi;
	mm_segment_t		fs;
	int			error;

	if (abinset) {
		get_user(new_set_abi, abinset);
		new_set = map_sigvec_to_kernel(new_set_abi,
			current_thread_info()->exec_domain->signal_map);
		nset = &new_set;
	}

	if (abioset)
		oset = &old_set;

	fs = get_fs();
	set_fs(get_ds());
	error = sys_rt_sigprocmask(howcnv[how], nset, oset, sizeof(sigset_t));
	set_fs(fs);

	if (!error && abioset) {
		old_set_abi = map_sigvec_from_kernel(old_set,
			current_thread_info()->exec_domain->signal_invmap);
		put_user(old_set_abi, abioset);
	}

	return (error);
}

int
abi_sigsuspend(struct pt_regs *regs)
{
	u_long			abi_mask, *abi_maskp;
	old_sigset_t		mask;

	abi_maskp = (u_long *)SIGNAL_NUMBER(regs);
	if (get_user(abi_mask, abi_maskp))
		return -EFAULT;

	mask = map_bitvec(abi_mask, current_thread_info()->exec_domain->signal_map);
#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_SIGNAL,
			"sigsuspend(mask = %lx)\n", mask);
#endif
	return sys_sigsuspend(0, current->blocked.sig[0], mask);
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(abi_sigaction);
EXPORT_SYMBOL(abi_sigfunc);
EXPORT_SYMBOL(abi_sigprocmask);
EXPORT_SYMBOL(abi_sigsuspend);
EXPORT_SYMBOL(deactivate_signal);
#endif
