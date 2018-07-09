#ifndef _ABI_SVR4_SIGSET_H
#define _ABI_SVR4_SIGSET_H

typedef void (*svr4_sig_t)(int, void *, void *);
typedef struct svr4_sigset {
	u_int	setbits[4];
} svr4_sigset_t;

#endif /* _ABI_SVR4_SIGSET_H */
