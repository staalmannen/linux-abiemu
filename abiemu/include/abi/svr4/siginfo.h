#ifndef _ABI_SVR4_SIGINFO_H
#define _ABI_SVR4_SIGINFO_H

struct svr4_siginfo {
	int	si_signo;
	int	si_code;
	int	si_errno;
	union {
		struct {	/* kill(), SIGCLD */
			long		_pid;
			union {
				struct {
					long	_uid;
				} _kill;
				struct {
					long	_utime;
					int	_status;
					long	_stime;
				} _cld;
			} _pdata;
		} _proc;
		struct {	/* SIGSEGV, SIGBUS, SIGILL, SIGFPE */
			char *	_addr;
		} _fault;
		struct {	/* SIGPOLL, SIGXFSZ */
			int	_fd;
			long	_band;
		} _file;
	} _data;
};

#endif /* _ABI_SVR4_SIGINFO_H */
