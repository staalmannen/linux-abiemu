#ifndef _ABI_UW7_RESOURCE_H
#define _ABI_UW7_RESOURCE_H

#ident "%W% %G%"

/*
 * UnixWare 7 resource limit handling.
 */

typedef u_int64_t	uw7_rlim64_t;

struct uw7_rlim64 {
	uw7_rlim64_t	rlim_cur;	/* current limit */
	uw7_rlim64_t	rlim_max;	/* maximum value for rlim_cur */
};

enum {
	UW7_RLIMIT_CPU =	0,	/* cpu time in milliseconds */
	UW7_RLIMIT_FSIZE =	1,	/* maximum file size */
	UW7_RLIMIT_DATA =	2,	/* data size */
	UW7_RLIMIT_STACK =	3,	/* stack size */
	UW7_RLIMIT_CORE =	4,	/* core file size */
	UW7_RLIMIT_NOFILE =	5,	/* file descriptors */
	UW7_RLIMIT_VMEM =	6,	/* maximum mapped memory */
};

static int uw7_to_linux_rlimit[] = {
	RLIMIT_CPU,
	RLIMIT_DATA,
	RLIMIT_STACK,
	RLIMIT_CORE,
	RLIMIT_NOFILE,
	RLIMIT_AS
};

#endif /* _ABI_UW7_RESOURCE_H */
