/*
 * pathconf.c - support for xenix pathconf
 *
 * Copyright (c) 1993,1994 Drew Sullivan
 * Copyright (c) 1994-1996 Mike Jagdis
 */

#ident "%W% %G%"

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/statfs.h>
#include <asm/uaccess.h>


enum {
	_PC_LINK_MAX =		0,
	_PC_MAX_CANON =		1,
	_PC_MAX_INPUT =		2,
	_PC_NAME_MAX =		3,
	_PC_PATH_MAX =		4,
	_PC_PIPE_BUF =		5,
	_PC_CHOWN_RESTRICTED =	6,
	_PC_NO_TRUNC =		7,
	_PC_VDISABLE =		8,
};

static int
xnx_name_max(char *path)
{
	struct statfs		stf;
	mm_segment_t		fs;
	char			*p;
	int			error;

	p = getname(path);
	if (IS_ERR(p))
		return PTR_ERR(p);

	fs = get_fs();
	set_fs(get_ds());
	error = sys_statfs(p, &stf);
	set_fs(fs);

	putname(p);
	if (error)
		return (error);
	return (stf.f_namelen);
}

int
xnx_pathconf(char *path, int name)
{
	switch (name) {
	case _PC_LINK_MAX:
		/*
		 * Although Linux headers define values on a per
		 * filesystem basis there is no way to access
		 * these without hard coding fs information here
		 * so for now we use a bogus value.
		 */
		return LINK_MAX;
	case _PC_MAX_CANON:
		return MAX_CANON;
	case _PC_MAX_INPUT:
		return MAX_INPUT;
	case _PC_PATH_MAX:
		return PATH_MAX;
	case _PC_PIPE_BUF:
		return PIPE_BUF;
	case _PC_CHOWN_RESTRICTED:
		/*
		 * We should really think about this and tell
		 * the truth.
		 */
		return 0;
	case _PC_NO_TRUNC:
		/* Not sure... It could be fs dependent? */
		return 1;
	case _PC_VDISABLE:
		return 1;
	case _PC_NAME_MAX:
		return xnx_name_max(path);
	}
	return -EINVAL;
}

int
xnx_fpathconf(int fildes, int name)
{
	switch (name) {
	case _PC_LINK_MAX:
		/*
		 * Although Linux headers define values on a per
		 * filesystem basis there is no way to access
		 * these without hard coding fs information here
		 * so for now we use a bogus value.
		 */
		return LINK_MAX;
	case _PC_MAX_CANON:
		return MAX_CANON;
	case _PC_MAX_INPUT:
		return MAX_INPUT;
	case _PC_PATH_MAX:
		return PATH_MAX;
	case _PC_PIPE_BUF:
		return PIPE_BUF;
	case _PC_CHOWN_RESTRICTED:
		/*
		 * We should really think about this and tell
		 * the truth.
		 */
		return 0;
	case _PC_NO_TRUNC:
		/* Not sure... It could be fs dependent? */
		return 1;
	case _PC_VDISABLE:
		return 1;
	case _PC_NAME_MAX:
		{
			struct statfs buf;
			int error;
			mm_segment_t old_fs;

			old_fs = get_fs();
			set_fs (get_ds());
			error = sys_fstatfs(fildes, &buf);
			set_fs(old_fs);
			if (!error)
				return buf.f_namelen;
			return error;
		}
	}

	return -EINVAL;
}
