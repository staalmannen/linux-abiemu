/*
 * Copyright (c) 2001 Caldera Deutschland GmbH.
 * Copyright (c) 2001 Christoph Hellwig.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _ABI_SCO_TYPES_H
#define _ABI_SCO_TYPES_H

#ident "%W% %G%"

/*
 * SCO OpenServer type declarations.
 */
#include <linux/highuid.h>
#include <linux/personality.h>
#include <linux/sched.h>
#include <linux/types.h>


typedef int16_t		sco_dev_t;
typedef u_int32_t	sco_ino_t;
typedef u_int16_t	sco_mode_t;
typedef int16_t		sco_nlink_t;
typedef u_int16_t	sco_uid_t;
typedef u_int16_t	sco_gid_t;
typedef int32_t		sco_off_t;
typedef int32_t		sco_time_t;


struct sco_statvfs {
	u_int32_t	f_bsize;
	u_int32_t	f_frsize;
	u_int32_t	f_blocks;
	u_int32_t	f_bfree;
	u_int32_t	f_bavail;
	u_int32_t	f_files;
	u_int32_t	f_free;
	u_int32_t	f_favail;
	u_int32_t	f_sid;
	char		f_basetype[16];
	u_int32_t	f_flag;
	u_int32_t	f_namemax;
	char		f_fstr[32];
	u_int32_t	f_filler[16];
};


/*
 * Stub for now, as we still have a 16 bit dev_t.
 */
static __inline sco_dev_t
linux_to_sco_dev_t(dev_t dev)
{
	return dev;
}

/*
 * If we thought we were in a short inode environment we are
 * probably already too late - getdents() will have likely
 * already assumed short inodes and "fixed" anything with
 * a zero low word (because it must match stat() which must
 * match read() on a directory).
 *
 * We will just have to go along with it.
 */
static __inline sco_ino_t
linux_to_sco_ino_t(ino_t ino)
{
	if (!is_cur_personality_flag(PERF_SHORT_INODE))
		return ino;
	if ((u_long)ino & 0xffff)
		return ino;
	return 0xfffffffe;
}

/*
 * SCO user/group IDs are the same as the old linux ones.
 */
static __inline sco_uid_t
linux_to_sco_uid_t(uid_t uid)
{
	return high2lowuid(uid);
}

static __inline sco_gid_t
linux_to_sco_gid_t(gid_t gid)
{
	return high2lowgid(gid);
}

#endif /* _ABI_SCO_TYPES_H */
