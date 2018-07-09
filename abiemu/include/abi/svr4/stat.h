#ifndef _ABI_SVR4_STAT_H
#define _ABI_SVR4_STAT_H

#ident "%W% %G%"

#include <abi/svr4/types.h>

/*
 * XXX	this will need additions if we support emulation of
 * XXX	64bit SVR4 on 64bit Linux
 */

struct svr4_stat {
	svr4_o_dev_t	  st_dev;
	svr4_o_ino_t	  st_ino;
	svr4_o_mode_t	  st_mode;
	svr4_o_nlink_t	  st_nlink;
	svr4_o_uid_t	  st_uid;
	svr4_o_gid_t	  st_gid;
	svr4_o_dev_t	  st_rdev;
	svr4_off_t	  st_size;
	svr4_time_t	  st_atime;
	svr4_time_t	  st_mtime;
	svr4_time_t	  st_ctime;
};

struct svr4_xstat {
	svr4_dev_t	  st_dev;
	u_int32_t	  st_pad1[3];
	svr4_ino_t	  st_ino;
	svr4_mode_t	  st_mode;
	svr4_nlink_t	  st_nlink;
	svr4_uid_t 	  st_uid;
	svr4_uid_t 	  st_gid;
	svr4_dev_t	  st_rdev;
	u_int32_t	  st_pad2[2];
	svr4_off_t	  st_size;
	u_int32_t	  st_pad3;
	svr4_timestruc_t  st_atim;
	svr4_timestruc_t  st_mtim;
	svr4_timestruc_t  st_ctim;
	u_int32_t	  st_blksize;
	u_int32_t	  st_blocks;
	char		  st_fstype[16];
	u_int32_t	  st_pad4[8];
};

/*
 * Helpers that are used for other xstat implementations as well.
 */
struct kstat;

extern int report_svr4_stat(struct kstat *, struct svr4_stat *);
extern int report_svr4_xstat(struct kstat *, struct svr4_xstat *);

#endif /* _ABI_SVR4_STAT_H */
