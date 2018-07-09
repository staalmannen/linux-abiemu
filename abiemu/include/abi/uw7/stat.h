#ifndef _ABI_UW7_STAT_H
#define _ABI_UW7_STAT_H

#ident "%W% %G%"


#include <abi/uw7/types.h>

struct uw7_stat64 {
	uw7_dev_t	st_dev;
	u_int32_t	st_pad1[3];
	uw7_ino_t	st_ino;
	uw7_mode_t	st_mode;
	uw7_nlink_t	st_nlink;
	uw7_uid_t	st_uid;
	uw7_gid_t	st_gid;
	uw7_dev_t	st_rdev;
	u_int32_t	st_pad2[2];
	uw7_off_t	st_size;
	struct timeval	st_atime;
	struct timeval	st_mtime;
	struct timeval	st_ctime;
	int32_t		st_blksize;
	int64_t		st_blocks;
	char		st_fstype[16];
	int32_t		st_aclcnt;
	u_int32_t	st_level;
	u_int32_t	st_flags;
	u_int32_t	st_cmwlevel;
	u_int32_t	st_pad4[4];
};

#endif /* _ABI_UW7_STAT_H */
