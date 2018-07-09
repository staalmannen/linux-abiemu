#define NSIGNALS	32


/* These are the signal numbers for the SVr4 signal handling */
#define IBCS_SIGHUP	1
#define IBCS_SIGINT	2
#define IBCS_SIGQUIT	3
#define IBCS_SIGILL	4
#define IBCS_SIGTRAP	5
#define IBCS_SIGIOT	6
#define IBCS_SIGABRT	6
#define IBCS_SIGEMT	7
#define IBCS_SIGFPE	8
#define IBCS_SIGKILL	9
#define IBCS_SIGBUS	10
#define IBCS_SIGSEGV	11
#define IBCS_SIGSYS	12
#define IBCS_SIGPIPE	13
#define IBCS_SIGALRM	14
#define IBCS_SIGTERM	15
#define IBCS_SIGUSR1	16
#define IBCS_SIGUSR2	17
#define IBCS_SIGCLD	18
#define IBCS_SIGCHLD	18
#define IBCS_SIGPWR	19
#define IBCS_SIGWINCH	20
#define IBCS_SIGURG	21	/* not SCO, SCO uses SIGUSR2 for SIGURG */
#define IBCS_SIGPOLL	22
#define IBCS_SIGIO	22
#define IBCS_SIGSTOP	23
#define IBCS_SIGTSTP	24
#define IBCS_SIGCONT	25
#define IBCS_SIGTTIN	26
#define IBCS_SIGTTOU	27
#define IBCS_SIGVTALRM	28
#define IBCS_SIGPROF	29
#define IBCS_SIGGXCPU	30
#define IBCS_SIGGXFSZ	31

#define ISC_SIGSTOP	24
#define ISC_SIGTSTP	25
#define ISC_SIGCONT	23

/* These are the signal numbers used by BSD. */
#define	BSD_SIGHUP	1
#define	BSD_SIGINT	2
#define	BSD_SIGQUIT	3
#define	BSD_SIGILL	4
#define	BSD_SIGTRAP	5
#define	BSD_SIGABRT	6
#define	BSD_SIGEMT	7
#define	BSD_SIGFPE	8
#define	BSD_SIGKILL	9
#define	BSD_SIGBUS	10
#define	BSD_SIGSEGV	11
#define	BSD_SIGSYS	12
#define	BSD_SIGPIPE	13
#define	BSD_SIGALRM	14
#define	BSD_SIGTERM	15
#define	BSD_SIGURG	16
#define	BSD_SIGSTOP	17
#define	BSD_SIGTSTP	18
#define	BSD_SIGCONT	19
#define	BSD_SIGCHLD	20
#define	BSD_SIGTTIN	21
#define	BSD_SIGTTOU	22
#define	BSD_SIGIO	23
#define	BSD_SIGXCPU	24
#define	BSD_SIGXFSZ	25
#define	BSD_SIGVTALRM 26
#define	BSD_SIGPROF	27
#define BSD_SIGWINCH 28
#define BSD_SIGINFO	29
#define BSD_SIGUSR1 30
#define BSD_SIGUSR2 31

