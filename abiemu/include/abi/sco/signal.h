#ifndef _ABI_SCO_SIGNAL_H
#define _ABI_SCO_SIGNAL_H

/*
 * Signal numbers for the SCO emulator.
 */

#ident "%W% %G%"

#define SCO_SIGHUP  1		/* hangup */
#define SCO_SIGINT  2		/* interrupt (rubout) */
#define SCO_SIGQUIT 3		/* quit (ASCII FS) */
#define SCO_SIGILL  4		/* illegal instruction (not reset when caught) */
#define SCO_SIGTRAP 5		/* trace trap (not reset when caught) */
#define SCO_SIGIOT  6		/* IOT instruction */
#define SCO_SIGABRT 6		/* used by abort, replace SIGIOT in the future */
#define SCO_SIGEMT  7		/* EMT instruction */
#define SCO_SIGFPE  8		/* floating point exception */
#define SCO_SIGKILL 9		/* kill (cannot be caught or ignored) */
#define SCO_SIGBUS  10		/* bus error */
#define SCO_SIGSEGV 11		/* segmentation violation */
#define SCO_SIGSYS  12		/* bad argument to system call */
#define SCO_SIGPIPE 13		/* write on a pipe with no one to read it */
#define SCO_SIGALRM 14		/* alarm clock */
#define SCO_SIGTERM 15		/* software termination signal from kill */
#define SCO_SIGUSR1 16		/* user defined signal 1 */
#define SCO_SIGUSR2 17		/* user defined signal 2 */
#define SCO_SIGCLD  18		/* death of a child */
#define SCO_SIGPWR  19		/* power-fail restart */
#define SCO_SIGWINCH 20		/* window change */
#define SCO_SIGURG  21		/* urgent socket condition */
#define SCO_SIGPOLL 22		/* pollable event occurred */
#define SCO_SIGSTOP 23		/* sendable stop signal not from tty */
#define SCO_SIGTSTP 24		/* stop signal from tty */
#define SCO_SIGCONT 25		/* continue a stopped process */
#define SCO_SIGTTIN 26		/* to readers pgrp upon background tty read */
#define SCO_SIGTTOU 27		/* like TTIN for output if tp->t_local&TOSTOP */
#define SCO_SIGVTALRM 28	/* virtual timer alarm */
#define SCO_SIGPROF 29		/* profile alarm */
#define SCO_SIGXCPU 30		/* CPU time limit exceeded */
#define SCO_SIGXFSZ 31		/* File size limit exceeded */

#endif /* _ABI_SCO_SIGNAL_H */
