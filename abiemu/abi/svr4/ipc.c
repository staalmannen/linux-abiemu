/*
 * Copyright (C) 1994	Mike Jagdis (jaggy@purplet.demon.co.uk)
 * Copyright (C) 2001   Christoph Hellwig (hch@caldera.de)
 *
 * Massive work over with a fine tooth comb, lots of rewriting. There
 * were a *lot* of bugs in this - mismatched structs that weren't
 * mapped, wrong pointers etc. I've tested this version with the
 * demo programs from the Wyse V/386 IPC documentation which exercise
 * all the functions. I don't have any major IPC using applications
 * to test it with - as far as I know...
 *
 * Again rewritten for Linux 2.4 - Linux 2.4 changes a lot of structures
 * and the cruft this file relied on has simply changed...
 *
 * Original copyright etc. follows:
 *
 * Copyright (C) 1993,1994  Joe Portman (baron@hebron.connected.com)
 *	First stab at ibcs shm, sem and msg handlers
 *
 * NOTE:
 * Please contact the author above before blindly making changes
 * to this file. You will break things.
 *
 * 04-15-1994 JLP III
 *	Still no msgsys, but IPC_STAT now works for shm calls
 *	Corrected argument order for sys_ipc calls, to accomodate Mike's
 *	changes, so that we can just call sys_ipc instead of the internal
 *	sys_* calls for ipc functions.
 *	Cleaned up translation of perm structures
 *	tstshm for Oracle now works.
 *
 * 04-23-1994 JLP III
 *	Added in msgsys calls, Tested and working
 *	Added translation for IPC_SET portions of all xxxctl functions.
 *	Added SHM_LOCK and SHM_UNLOCK to shmsys
 *
 * 04-28-1994 JLP III
 *	Special thanks to Brad Pepers for adding in the GETALL and SETALL
 *	case of semaphores. (pepersb@cuug.ab.ca)
 */

#ident "%W% %G%"

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/string.h>
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/ipc.h>
#include <linux/sem.h>
#include <linux/shm.h>
#include <linux/msg.h>

#include <asm/uaccess.h>
#include <asm/ipc.h>

#include <abi/svr4/ipc.h>
#include <abi/util/trace.h>
#include <abi/util/errno.h>


static __inline__ void
ip_to_lp(struct ibcs2_ipc_perm *ip, struct ipc_perm *lp)
{
	lp->uid = ip->uid;
	lp->gid = ip->gid;
	lp->cuid = ip->cuid;
	lp->cgid = ip->cgid;
	lp->mode = ip->mode;
	lp->seq = ip->seq;
	lp->key = ip->key;
}

static __inline__ void
lp_to_ip(struct ipc_perm *lp, struct ibcs2_ipc_perm *ip)
{
	ip->uid = lp->uid;
	ip->gid = lp->gid;
	ip->cuid = lp->cuid;
	ip->cgid = lp->cgid;
	ip->mode = lp->mode;
	ip->seq = lp->seq;
	ip->key = lp->key;
}

static __inline__ void
ip_to_lp_l(struct abi4_ipc_perm *ip, struct ipc_perm *lp)
{
	lp->uid = ip->uid;
	lp->gid = ip->gid;
	lp->cuid = ip->cuid;
	lp->cgid = ip->cgid;
	lp->mode = ip->mode;
	lp->seq = ip->seq;
	lp->key = ip->key;
}

static __inline__ void
lp_to_ip_l(struct ipc_perm *lp, struct abi4_ipc_perm *ip)
{
	ip->uid = lp->uid;
	ip->gid = lp->gid;
	ip->cuid = lp->cuid;
	ip->cgid = lp->cgid;
	ip->mode = lp->mode;
	ip->seq = lp->seq;
	ip->key = lp->key;
}

static void
isem_to_lsem(struct ibcs2_semid_ds *is, struct semid_ds *ls)
{
	ip_to_lp(&is->sem_perm, &ls->sem_perm);

	ls->sem_base = is->sem_base;
	ls->sem_nsems = is->sem_nsems;
	ls->sem_otime = is->sem_otime;
	ls->sem_ctime = is->sem_ctime;
}

static void
lsem_to_isem(struct semid_ds *ls, struct ibcs2_semid_ds *is)
{
	lp_to_ip(&ls->sem_perm, &is->sem_perm);

	is->sem_base = ls->sem_base;
	is->sem_nsems = ls->sem_nsems;
	is->sem_otime = ls->sem_otime;
	is->sem_ctime = ls->sem_ctime;
}

static void
isem_to_lsem_l(struct abi4_semid_ds *is, struct semid_ds *ls)
{
	ip_to_lp_l(&is->sem_perm, &ls->sem_perm);

	ls->sem_base = is->sem_base;
	ls->sem_nsems = is->sem_nsems;
	ls->sem_otime = is->sem_otime;
	ls->sem_ctime = is->sem_ctime;
}

static void
lsem_to_isem_l(struct semid_ds *ls, struct abi4_semid_ds *is)
{
	memset(is, 0, sizeof(*is));

	lp_to_ip_l(&ls->sem_perm, &is->sem_perm);

	is->sem_base = ls->sem_base;
	is->sem_nsems = ls->sem_nsems;
	is->sem_otime = ls->sem_otime;
	is->sem_ctime = ls->sem_ctime;
}

static int
__ibcs2_semctl(int first, int second, int third, union semun *fourth)
{
	struct ibcs2_semid_ds	is, *isp;
	struct semid_ds		ls;
	union semun		lsemun;
	mm_segment_t		fs;
	int			err;

	err = get_user(isp, (struct ibcs2_semid_ds **)&fourth->buf);
	if (err)
		return (err);

	err = copy_from_user(&is, isp, sizeof(is)) ? -EFAULT : 0;
	if (err)
		return (err);

	isem_to_lsem(&is, &ls);
	lsemun.buf = &ls;

	fs = get_fs();
	set_fs(get_ds());
	err = sys_ipc(SEMCTL, first, second, third, &lsemun,0);
	set_fs(fs);

	if (err < 0)
		return (err);

	lsem_to_isem(&ls, &is);
	return copy_to_user(isp, &is, sizeof(is)) ? -EFAULT : 0;
}

static int
__abi4_semctl(int first, int second, int third, union semun *fourth)
{
	struct abi4_semid_ds	is, *isp;
	struct semid_ds		ls;
	union semun		lsemun;
	mm_segment_t		fs;
	int			err;

	err = get_user(isp, (struct abi4_semid_ds **)&fourth->buf);
	if (err)
		return (err);

	err = copy_from_user(&is, isp, sizeof(is)) ? -EFAULT : 0;
	if (err)
		return (err);

	isem_to_lsem_l(&is, &ls);
	lsemun.buf = &ls;

	fs = get_fs();
	set_fs(get_ds());
	err = sys_ipc(SEMCTL, first, second, third, &lsemun,0);
	set_fs(fs);

	if (err < 0)
		return (err);

	lsem_to_isem_l(&ls, &is);
	return copy_to_user(isp, &is, sizeof(is)) ? -EFAULT : 0;
}

static int
svr4_semctl(int arg1, int arg2, int arg3, union semun *arg4)
{
	int			cmd = svr4sem2linux[arg3];

	switch (arg3) {
	case SVR4_SEM_SETALL:
	case SVR4_SEM_GETALL:
		return __ibcs2_semctl(arg1, 0, cmd, arg4);
	case SVR4_IPC_RMID:
	case SVR4_IPC_RMID_L:
	case SVR4_SEM_SETVAL:
	case SVR4_SEM_GETVAL:
	case SVR4_SEM_GETPID:
	case SVR4_SEM_GETNCNT:
	case SVR4_SEM_GETZCNT:
		return  sys_ipc(SEMCTL, arg1, arg2, cmd, arg4, 0);
	case SVR4_IPC_SET:
	case SVR4_IPC_STAT:
		return __ibcs2_semctl(arg1, arg2, cmd, arg4);
	case SVR4_IPC_STAT_L:
	case SVR4_IPC_SET_L:
		return __abi4_semctl(arg1, arg2, cmd, arg4);
	}

	__abi_trace("semctl: unsupported command %d\n", arg3);
	return -EINVAL;
}

static int
svr4_semget(int arg1, int arg2, int arg3)
{
#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "semget(%d, %d, %o)\n", arg1, arg2, arg3);
#endif
	return sys_semget(arg1, arg2, arg3);

}

static int
svr4_semop(int arg1, struct sembuf *arg2, int arg3)
{
#if defined(CONFIG_ABI_TRACE)
	if (abi_traced(ABI_TRACE_API)) {
		struct sembuf	tmp, *tp = arg2;
		int		i;

		for (i = 0; i < arg3; i++) {
			if (copy_from_user (&tmp, tp, sizeof(tmp)))
			{	__abi_trace("semop(-EFAULT)\n");
				break;
			}
			__abi_trace("semop(%d, %d, 0%o)\n",
					tmp.sem_num, tmp.sem_op,
					tmp.sem_flg);
			tp++;
		}
	}
#endif

	return sys_semop(arg1, arg2, arg3);
}

int
svr4_semsys(struct pt_regs *regp)
{
	int			which, arg1, arg2, arg3;
	union semun		*arg4;

	which = get_syscall_parameter(regp, 0);
	arg1 = get_syscall_parameter(regp, 1);
	arg2 = get_syscall_parameter(regp, 2);
	arg3 = get_syscall_parameter(regp, 3);

	/*
	 * XXX - The value for arg4 depends on how union
	 * passing is implement on this architecture and
	 * compiler. The following is *only* known to be
	 * right for Intel (the default else case).
	 */
#ifdef __sparc__
	arg4 = (union semun *)get_syscall_parameter(regp, 4);
#else
	arg4 = (union semun *)(((u_long *)regp->esp) + (5));
#endif

	switch (which) {
	case SVR4_semctl:
		return svr4_semctl(arg1, arg2, arg3, arg4);
	case SVR4_semget:
		return svr4_semget(arg1, arg2, arg3);
	case SVR4_semop:
		return svr4_semop(arg1, (struct sembuf *)arg2, arg3);
	}

	return -EINVAL;
}

static void
ishm_to_lshm(struct ibcs2_shmid_ds *is, struct shmid_ds *ls)
{
	ip_to_lp(&is->shm_perm, &ls->shm_perm);
	ls->shm_segsz = is->shm_segsz;
	ls->shm_lpid = is->shm_lpid;
	ls->shm_cpid = is->shm_cpid;
	ls->shm_nattch = is->shm_nattch;
	ls->shm_atime = is->shm_atime;
	ls->shm_dtime = is->shm_dtime;
	ls->shm_ctime = is->shm_ctime;
}

static void
lshm_to_ishm(struct shmid_ds *ls, struct ibcs2_shmid_ds *is)
{
	lp_to_ip(&ls->shm_perm, &is->shm_perm);
	is->shm_segsz = ls->shm_segsz;
	is->shm_lpid = ls->shm_lpid;
	is->shm_cpid = ls->shm_cpid;
	is->shm_nattch = ls->shm_nattch;
	is->shm_atime = ls->shm_atime;
	is->shm_dtime = ls->shm_dtime;
	is->shm_ctime = ls->shm_ctime;
}

static void
ishm_to_lshm_l(struct abi4_shmid_ds *is, struct shmid_ds *ls)
{
	ip_to_lp_l(&is->shm_perm, &ls->shm_perm);
	ls->shm_segsz = is->shm_segsz;
	ls->shm_lpid = is->shm_lpid;
	ls->shm_cpid = is->shm_cpid;
	ls->shm_nattch = is->shm_nattch;
	ls->shm_atime = is->shm_atime;
	ls->shm_dtime = is->shm_dtime;
	ls->shm_ctime = is->shm_ctime;
}

static void
lshm_to_ishm_l(struct shmid_ds *ls, struct abi4_shmid_ds *is)
{
	memset(is, 0, sizeof(*is));
	lp_to_ip_l(&ls->shm_perm, &is->shm_perm);
	is->shm_segsz = ls->shm_segsz;
	is->shm_lpid = ls->shm_lpid;
	is->shm_cpid = ls->shm_cpid;
	is->shm_nattch = ls->shm_nattch;
	is->shm_atime = ls->shm_atime;
	is->shm_dtime = ls->shm_dtime;
	is->shm_ctime = ls->shm_ctime;
}

static int
svr4_shmdt(struct pt_regs *regp)
{
	caddr_t			addr = (caddr_t)get_syscall_parameter(regp, 1);

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "shmdt(%p)\n", addr);
#endif
	return sys_shmdt(addr);
}

static int
svr4_shmctl(int arg1, int cmd, char *arg3)
{
	struct ibcs2_shmid_ds	is;
	struct abi4_shmid_ds	is4;
	struct shmid_ds		ls;
	mm_segment_t		fs;
	int			err;

#if defined(CONFIG_ABI_TRACE)
	abi_trace(ABI_TRACE_API, "shmctl(%d, %x, %p)\n", arg1, cmd, arg3);
#endif

	switch (cmd) {
	case SVR4_SHM_LOCK:
		return sys_shmctl(arg1, SHM_LOCK, (struct shmid_ds *)arg3);
	case SVR4_SHM_UNLOCK:
		return sys_shmctl(arg1, SHM_UNLOCK, (struct shmid_ds *)arg3);
	case SVR4_IPC_SET:
		err = copy_from_user(&is, arg3, sizeof(is)) ? -EFAULT : 0;
		if (err)
			break;
		ishm_to_lshm(&is, &ls);

		fs = get_fs();
		set_fs(get_ds());
		err = sys_shmctl(arg1, IPC_SET, &ls);
		set_fs(fs);

		if (err < 0)
			break;
		lshm_to_ishm(&ls, &is);
		err = copy_to_user(arg3, &is, sizeof(is)) ? -EFAULT : 0;
		break;
	case SVR4_IPC_SET_L:
		err = copy_from_user(&is4, arg3, sizeof(is4)) ? -EFAULT : 0;
		if (err)
			break;
		ishm_to_lshm_l(&is4, &ls);

		fs = get_fs();
		set_fs(get_ds());
		err = sys_shmctl(arg1, IPC_SET, &ls);
		set_fs(fs);

		if (err < 0)
			break;
		lshm_to_ishm_l(&ls, &is4);
		err = copy_to_user(arg3, &is4, sizeof(is4)) ? -EFAULT : 0;
		break;
	case SVR4_IPC_RMID:
	case SVR4_IPC_RMID_L:
		return sys_shmctl(arg1, IPC_RMID, (struct shmid_ds *)arg3);
	case SVR4_IPC_STAT:
		fs = get_fs();
		set_fs(get_ds());
		err = sys_shmctl(arg1, IPC_STAT, &ls);
		set_fs(fs);

		if (err < 0)
			break;

		lshm_to_ishm(&ls, &is);
		err = copy_to_user(arg3, &is, sizeof(is)) ? -EFAULT : 0;
		break;
	case SVR4_IPC_STAT_L:
		fs = get_fs();
		set_fs(get_ds());
		err = sys_shmctl(arg1, IPC_STAT, &ls);
		set_fs(fs);
		if (err < 0)
			break;

		lshm_to_ishm_l(&ls, &is4);
		err = copy_to_user((char *)arg3, &is4, sizeof(is4)) ? -EFAULT : 0;
		break;
	default:
#if defined(CONFIG_ABI_TRACE)
		__abi_trace("shmctl: unsupported command %d\n", cmd);
#endif
		err = -EINVAL;
	}

	return (err);
}

int
svr4_shmsys(struct pt_regs *regp)
{
	int			arg1, arg2, arg3, cmd, err = 0;
	u_long			raddr;
	mm_segment_t		fs;

	cmd = get_syscall_parameter(regp, 0);
	if (cmd == SVR4_shmdt) {
		err = svr4_shmdt(regp);
		goto out;
	}

	arg1 = get_syscall_parameter(regp, 1);
	arg2 = get_syscall_parameter(regp, 2);
	arg3 = get_syscall_parameter(regp, 3);

	switch (cmd) {
	case SVR4_shmat:
#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_API, "shmat(%d, %x, %o)\n", arg1, arg2, arg3);
#endif

		fs = get_fs();
		set_fs(get_ds());
		err = do_shmat(arg1, (caddr_t)arg2, arg3, &raddr);
		set_fs(fs);
		if (err >= 0)
			err = (int)raddr;

#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_API, "shmat returns %x\n", err);
#endif
		break;
	case SVR4_shmget:
#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_API, "shmget(%d, %x, %o)\n", arg1, arg2, arg3);
#endif
		err = sys_shmget(arg1, arg2, arg3);
#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_API, "shmget returns %d\n", err);
#endif
		break;
	case SVR4_shmctl:
		err = svr4_shmctl(arg1, arg2, (char *)arg3);
		break;
	default:
#if defined(CONFIG_ABI_TRACE)
		__abi_trace("shmsys: unsupported command: %x\n", cmd);
#endif
		err = -EINVAL;
	}

out:
	if (err < 0 && err > -255) {
	        set_error(regp, iABI_errors(-err));
		abi_trace(ABI_TRACE_API, "Error %d\n", get_result(regp));
		return 0;
	}

	clear_error(regp);
	set_result(regp, err);
	return 0;
}

static void
imsq_to_lmsq(struct ibcs2_msqid_ds * im, struct msqid_ds * lm)
{
	ip_to_lp(&im->msg_perm, &lm->msg_perm);
	lm->msg_first = im->msg_first;
	lm->msg_last = im->msg_last;
	lm->msg_cbytes = im->msg_cbytes;
	lm->msg_qnum = im->msg_qnum;
	lm->msg_qbytes = im->msg_qbytes;
	lm->msg_lspid = im->msg_lspid;
	lm->msg_lrpid = im->msg_lrpid;
	lm->msg_stime = im->msg_stime;
	lm->msg_rtime = im->msg_rtime;
	lm->msg_ctime = im->msg_ctime;
}

static void
lmsq_to_imsq(struct msqid_ds *lm, struct ibcs2_msqid_ds *im)
{
	lp_to_ip(&lm->msg_perm, &im->msg_perm);
	im->msg_first = lm->msg_first;
	im->msg_last = lm->msg_last;
	im->msg_cbytes = lm->msg_cbytes;
	im->msg_qnum = lm->msg_qnum;
	im->msg_qbytes = lm->msg_qbytes;
	im->msg_lspid = lm->msg_lspid;
	im->msg_lrpid = lm->msg_lrpid;
	im->msg_stime = lm->msg_stime;
	im->msg_rtime = lm->msg_rtime;
	im->msg_ctime = lm->msg_ctime;
}

static void
imsq_to_lmsq_l(struct abi4_msqid_ds *im, struct msqid_ds *lm)
{
	ip_to_lp_l(&im->msg_perm, &lm->msg_perm);
	lm->msg_first = im->msg_first;
	lm->msg_last = im->msg_last;
	lm->msg_cbytes = im->msg_cbytes;
	lm->msg_qnum = im->msg_qnum;
	lm->msg_qbytes = im->msg_qbytes;
	lm->msg_lspid = im->msg_lspid;
	lm->msg_lrpid = im->msg_lrpid;
	lm->msg_stime = im->msg_stime;
	lm->msg_rtime = im->msg_rtime;
	lm->msg_ctime = im->msg_ctime;
}

static void
lmsq_to_imsq_l(struct msqid_ds *lm, struct abi4_msqid_ds *im)
{
	memset(im, 0, sizeof(*im));
	lp_to_ip_l(&lm->msg_perm, &im->msg_perm);
	im->msg_first = lm->msg_first;
	im->msg_last = lm->msg_last;
	im->msg_cbytes = lm->msg_cbytes;
	im->msg_qnum = lm->msg_qnum;
	im->msg_qbytes = lm->msg_qbytes;
	im->msg_lspid = lm->msg_lspid;
	im->msg_lrpid = lm->msg_lrpid;
	im->msg_stime = lm->msg_stime;
	im->msg_rtime = lm->msg_rtime;
	im->msg_ctime = lm->msg_ctime;
}

static int
svr4_msgctl(int arg1, int cmd, char *arg3)
{
	struct ibcs2_msqid_ds	im;
	struct abi4_msqid_ds	im4;
	struct msqid_ds		lm;
	mm_segment_t		fs;
	int			err;

	switch (cmd) {
	case SVR4_IPC_SET:
		err = copy_from_user(&im, arg3, sizeof(im)) ? -EFAULT : 0;
		if (err)
			break;

		imsq_to_lmsq(&im, &lm);

		fs = get_fs();
		set_fs(get_ds());
		err = sys_msgctl(arg1, IPC_SET, &lm);
		set_fs(fs);

		lmsq_to_imsq(&lm, &im);
		err = copy_to_user(arg3, &im, sizeof(im)) ? -EFAULT : 0;
		break;
	case SVR4_IPC_SET_L:
		err = copy_from_user(&im4, arg3, sizeof(im4)) ? -EFAULT : 0;
		if (err)
			break;
		imsq_to_lmsq_l(&im4, &lm);

		fs = get_fs();
		set_fs(get_ds());
		err = sys_msgctl(arg1, IPC_SET, &lm);
		set_fs(fs);

		lmsq_to_imsq_l(&lm, &im4);
		err = copy_to_user(arg3, &im4, sizeof(im4)) ? -EFAULT : 0;
		break;
	case SVR4_IPC_RMID:
	case SVR4_IPC_RMID_L:
		return sys_msgctl(arg1, IPC_RMID, (struct msqid_ds *)arg3);
	case SVR4_IPC_STAT:
		fs = get_fs();
		set_fs(get_ds());
		err = sys_msgctl(arg1, IPC_STAT, &lm);
		set_fs(fs);

		if (err < 0)
			break;

		lmsq_to_imsq(&lm, &im);
		err = copy_to_user(arg3, &im, sizeof(im)) ? -EFAULT : 0;
		break;
	case SVR4_IPC_STAT_L:
		fs = get_fs();
		set_fs(get_ds());
		err = sys_msgctl(arg1, IPC_STAT, &lm);
		set_fs(fs);

		if (err < 0)
			break;

		lmsq_to_imsq_l(&lm, &im4);
		err = copy_to_user(arg3, &im4, sizeof(im4)) ? -EFAULT : 0;
		break;
	default:
		__abi_trace("msgctl: unsupported command: %x\n", cmd);
		err = -EINVAL;
	}

	return (err);
}

int
svr4_msgsys(struct pt_regs *regp)
{
	int			err, cmd, arg1, arg2, arg3, arg4, arg5;

	/*
	 * Special handling as msgrcv is ugly.
	 */
	cmd = get_syscall_parameter(regp, 0);
	arg1 = get_syscall_parameter(regp, 1);
	arg2 = get_syscall_parameter(regp, 2);

	switch (cmd) {
	case SVR4_msgget:
		return sys_msgget((key_t)arg1, arg2);
	case SVR4_msgctl:
		arg3 = get_syscall_parameter(regp, 3);
		return svr4_msgctl(arg1, arg2, (caddr_t)arg3);
	case SVR4_msgrcv:
		arg3 = get_syscall_parameter(regp, 3);
		arg4 = get_syscall_parameter(regp, 4);
		arg5 = get_syscall_parameter(regp, 5);
		return sys_msgrcv(arg1, (struct msgbuf *)arg2, arg3, arg4, arg5);
	case SVR4_msgsnd:
		arg3 = get_syscall_parameter(regp, 3);
		arg4 = get_syscall_parameter(regp, 4);
		err = sys_msgsnd(arg1, (struct msgbuf *)arg2, arg3, arg4);
		return ((err > 0) ? 0 : err);
	}

	__abi_trace("msgsys: unsupported command: %x\n", cmd);
	return -EINVAL;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_msgsys);
EXPORT_SYMBOL(svr4_semsys);
EXPORT_SYMBOL(svr4_shmsys);
#endif
