#ifndef _ABI_SCO_STAT_H
#define _ABI_SCO_STAT_H

#ident "%W% %G%"

#include <abi/sco/types.h>


struct sco_xstat {
	sco_dev_t	st_dev;
	u_int32_t	__pad1[3];
	sco_ino_t	st_ino;
	sco_mode_t	st_mode;
	sco_nlink_t	st_nlink;
	sco_uid_t	st_uid;
	sco_gid_t	st_gid;
	sco_dev_t	st_rdev;
	u_int32_t	__pad2[2];
	sco_off_t	st_size;
	u_int32_t	__pad3;
	sco_time_t	st_atime;
	sco_time_t	st_mtime;
	sco_time_t	st_ctime;
	int32_t		st_blksize;
	int32_t		st_blocks;
	char		st_fstype[16];
	u_int32_t	__pad4[7];
	int32_t		st_sco_flags;
};

#endif /* _ABI_SCO_STAT_H */
