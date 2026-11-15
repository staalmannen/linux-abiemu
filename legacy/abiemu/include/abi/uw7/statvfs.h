#ifndef _ABI_UW7_STATVFS_H
#define _ABI_UW7_STATVFS_H

#ident "%W% %G%"

#include <abi/uw7/types.h>


struct uw7_statvfs64 {
	u_int32_t		f_bsize;	/* file system block size */
	u_int32_t		f_frsize;	/* file system fragment size */
	uw7_fsblkcnt64_t	f_blocks;	/* total # of fragments */
	uw7_fsblkcnt64_t	f_bfree;	/* total # of free fragments */
	uw7_fsblkcnt64_t	f_bavail;	/* # of free fragments avail */
	uw7_fsfilcnt64_t	f_files;	/* total # of inodes */
	uw7_fsfilcnt64_t	f_ffree;	/* total # of free inodes */
	uw7_fsfilcnt64_t	f_favail;	/* # of free inodes avail */
	u_int32_t		f_fsid;		/* file system id */
	char			f_basetype[16];	/* target fs type name */
	u_int32_t		f_flag;		/* bit-mask of flags */
	u_int32_t		f_namemax;	/* maximum file name length */
	char			f_fstr[32];	/* filesystem string */
	u_int32_t		f_filler[16];	/* reserved */
};

#endif /* _ABI_UW7_STATVFS_H */
