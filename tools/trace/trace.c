/*
 * little utility to turn tracing on and off for certain calls
 *  Copyright (C) 1993  Joe Portman (baron@hebron.connected.com)
 *
 * Rewritten for latest iBCS emulator. Not so little now and still
 * to grow...
 *  Copyright (C) 1994  Mike Jagdis (jaggy@purplet.demon.co.uk)
 *
 * $Id: trace.c 3 2000-10-27 00:56:29Z hch $
 * $Source$
 */
#define _GNU_SOURCE
#include <features.h>

#include <linux/personality.h>
#include <stdio.h>
#include <stdlib.h>

#if __GLIBC__ >= 2
#  include <syscall.h>
#  include <unistd.h>
#else
#  include <linux/unistd.h>
#endif

#if 0
#include <ibcs/ibcs.h>
#endif
#include <ibcs/traceflags.h>


#if __GLIBC__ >= 2
#  define personality(X)	syscall(__NR_personality, (X))
#else
   _syscall1(int, personality, int, pers);
#endif


#ifdef __sparc__
static int
set_trace (int val)
{
	int res;

	__asm__ volatile ("mov %1,%%o0\n\t"
			  "mov 193,%%g1\n\t"
			  "t 8\n\t"
			  "mov %%o0,%0" : "=r" (res) : "r" (val) : "o0", "g1");
        return res;
}

static int
trace_func_set(int per, int call, int val)
{
	int res;

	__asm__ volatile ("mov %1,%%o0\n\t"
			  "mov %2,%%o1\n\t"
			  "mov %3,%%o2\n\t"
			  "mov 194,%%g1\n\t"
			  "t 8\n\t"
			  "mov %%o0,%0"
			  : "=r" (res)
			  : "r" (per), "r" (call), "r" (val)
		          : "o0", "o1", "o2", "g1");
	return res;
}

#else /* __sparc__ */

static int
set_trace(int val)
{
	int res;

	__asm__ volatile ("mov\t$0x00FF,%%eax\n\t"
		".byte\t0x9a,0,0,0,0,7,0\n\t"
		: "=a" (res));

	return res;
}


static int
trace_func_set(int per, int call, int val)
{
	int res;

	__asm__ volatile ("mov\t$0x01FF,%%eax\n\t"
			".byte\t0x9a,0,0,0,0,7,0\n\t"
			: "=a" (res));

	return res;
}
#endif /* __sparc__ */


struct t_code {
	char	*name;
	int	code;
};

struct t_code codes[] = {
	{ "all",		0x0ffffeff	},
	{ "everything",		0x0ffffeff	},
	{ "api",		TRACE_API	},
	{ "ioctl",		TRACE_IOCTL	},
	{ "failed-ioctl",	TRACE_IOCTL_F	},
	{ "signal",		TRACE_SIGNAL	},
	{ "failed-signal",	TRACE_SIGNAL_F	},
	{ "socksys",		TRACE_SOCKSYS	},
	{ "spx",		TRACE_SOCKSYS	},
	{ "sockets",		TRACE_SOCKSYS	},
	{ "coff",		TRACE_COFF_LD	},
	{ "elf",		TRACE_ELF_LD	},
	{ "xout",		TRACE_XOUT_LD	},
	{ "xout-block",		TRACE_XOUT_DB	},
	{ "streams",		TRACE_STREAMS	},
	{ "STREAMS",		TRACE_STREAMS	}
};


static int
get_code(char *s)
{
	int i;

	for (i=0; i<sizeof(codes)/sizeof(codes[0]); i++)
		if (!strcmp(s, codes[i].name))
			return codes[i].code;

	fprintf(stderr, "%s not recognised - ignored\n", s);
	return 0;
}


static void
usage(void)
{
	int i;

	fprintf(stderr,
		"usage: trace n         - set trace code to n\n"
		"          or p,n	- query tracing for syscall n under personality p\n"
		"          or p,n,[0|1] - set/clear tracing for syscall n under personality p\n"
		"          or [+|-]code - where code could be:\n"
		"                  query\n"
		"                  off\n");

	for (i=0; i<sizeof(codes)/sizeof(codes[0]); i++)
		fprintf(stderr, "                  %s\n", codes[i].name);

	fprintf(stderr, "\n"
		"WARNING: This is a debugging tool *only*. It is not robust.\n"
		"         Passing bad values can cause iBCS/kernel crashes!\n");
}


int
main(int argc, char *argv[])
{
	int old_trace_val, trace_val, i;

	if (argc == 1) {
		usage();
		exit(1);
	}

	/* We need to be in a non-native personality to get access to
	 * the emulator's trace facilities.
	 */
	personality(PER_SVR3);

	old_trace_val = trace_val = set_trace(-1);
	for (i=1; i<argc; i++) {
		if (!strcmp(argv[i], "query")) {
			printf("iBCS trace code is 0x%lx\n", set_trace(-1));
		} else if (!strcmp(argv[i], "off")) {
			trace_val = 0;
		} else if (argv[i][0] == '+') {
			trace_val |= get_code(argv[i]+1);
		} else if (argv[i][0] == '-') {
			trace_val &= (~get_code(argv[i]+1));
		} else {
			int n;
			long a1,a2,a3;

			n = sscanf(argv[i], "%li,%li,%li", &a1, &a2, &a3);
			switch (n) {
				case 1: /* Single value is a trace code */
					trace_val = a1;
					break;

				case 2: /* Two arguments is a syscall query */
					a3 = -1;
					/* and drop through... */

				case 3: /* Three arguments is a syscall set */
					n = trace_func_set(a1, a2, a3);
					if (n > 0)
						printf("personality %d, syscall 0x%x - tracing enabled\n",
							a1, a2);
					else if (n == 0)
						printf("personality %d, syscall 0x%x - tracing disabled\n",
							a1, a2);
					else
						printf("personality %d, syscall 0x%x - no such call/personality\n",
							a1, a2);
					break;

				default: /* could be a text code */
					trace_val |= get_code(argv[i]);
					break;
			}
		}
	}

	if (trace_val != old_trace_val)
		printf("iBCS trace code is 0x%lx\n", set_trace(trace_val));

	exit(0);
}
