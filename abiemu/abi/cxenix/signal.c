#ident "%W% %G%"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/personality.h>
#include <linux/sched.h>
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <asm/uaccess.h>

#include <abi/cxenix/signal.h>
#include <abi/signal.h>

#include <abi/util/map.h>
#include <abi/util/sysent.h>


int
xnx_sigaction(int sco_signum, const struct sco_sigaction *action,
		struct sco_sigaction *oldaction)
{
	struct sco_sigaction	new_sa, old_sa;
	struct sigaction	nsa, osa;
	mm_segment_t		fs;
	int			error, signo;

	if (sco_signum >= NSIGNALS)
		return -EINVAL;
	signo = current_thread_info()->exec_domain->signal_map[sco_signum];

	if (oldaction) {
		error = verify_area(VERIFY_WRITE, oldaction,
				sizeof(struct sco_sigaction));
		if (error)
			return (error);
	}

	if (action) {
		error = copy_from_user(&new_sa, action,
				sizeof(struct sco_sigaction));
		if (error)
			return -EFAULT;
		nsa.sa_restorer = NULL;
		nsa.sa_handler = new_sa.sa_handler;
		nsa.sa_mask = map_sigvec_to_kernel(new_sa.sa_mask,
			current_thread_info()->exec_domain->signal_map);
		nsa.sa_flags = 0 /* | SA_NOMASK */;
		if (new_sa.sa_flags & SCO_SA_NOCLDSTOP)
			nsa.sa_flags |= SA_NOCLDSTOP;
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
	if (osa.sa_flags & SA_NOCLDSTOP)
		old_sa.sa_flags |= SCO_SA_NOCLDSTOP;

	if (copy_to_user(oldaction, &old_sa, sizeof(struct sco_sigaction)))
		return -EFAULT;
	return 0;
}

int
xnx_sigpending(u_long *setp)
{
	sigset_t		lxpending;
	u_long			pending;

	spin_lock_irq(&current->sighand->siglock);
	sigandsets(&lxpending, &current->blocked, &current->pending.signal);
	spin_unlock_irq(&current->sighand->siglock);

	pending = map_sigvec_from_kernel(lxpending,
			current_thread_info()->exec_domain->signal_invmap);

	if (copy_to_user(setp, &pending, sizeof(u_long)))
		return -EFAULT;
	return 0;
}
