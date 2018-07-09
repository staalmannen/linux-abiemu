#ifndef _ABI_SVR4_SIGACTION_H
#define _ABI_SVR4_SIGACTION_H

#ident "%W% %G%"

/* signal.c */
struct task_struct;
extern void deactivate_signal(struct task_struct *, int);

struct abi_sigaction {
       int          sa_flags;
       __sighandler_t sa_handler;
       unsigned long sa_mask;
       int	    sa_resv[2];  /* Reserved for something or another */
};
#define ABI_SA_ONSTACK   1
#define ABI_SA_RESETHAND 2
#define ABI_SA_RESTART   4
#define ABI_SA_SIGINFO   8
#define ABI_SA_NODEFER  16
#define ABI_SA_NOCLDWAIT 0x10000
#define ABI_SA_NOCLDSTOP 0x20000

#endif /* _ABI_SVR4_SIGACTION_H */
