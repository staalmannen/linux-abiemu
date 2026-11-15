#ifndef _ABI_SOLARIS_STAT_H
#define _ABI_SOLARIS_STAT_H

#ident "%W% %G%"

#include <abi/solaris/types.h>


struct sol_stat64 {
	sol_dev_t	st_dev;
	u_int32_t	st_pad1[3];
	sol_ino_t	st_ino;
	sol_mode_t	st_mode;
	sol_nlink_t	st_nlink;
	sol_uid_t	st_uid;
	sol_gid_t	st_gid;
	sol_dev_t	st_rdev;
	u_int32_t	st_pad2[2];
	sol_off_t	st_size;
	sol_time_t	st_atime;
	sol_time_t	st_mtime;
	sol_time_t	st_ctime;
	int32_t		st_blksize;
	int64_t		st_blocks;
	char		st_fstype[16];
	u_int32_t	st_pad4[4];
};

#endif /* _ABI_SOLARIS_STAT_H */
