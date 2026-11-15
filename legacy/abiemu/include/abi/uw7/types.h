/*
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
#ifndef _ABI_UW7_TYPES_H
#define _ABI_UW7_TYPES_H

#ident "%W% %G%"

/*
 * UnixWare 7 type declarations.
 */
#include <abi/svr4/types.h>	/* UnixWare is based on SVR4 */

typedef svr4_dev_t	uw7_dev_t;
typedef u_int64_t	uw7_ino_t;
typedef svr4_mode_t	uw7_mode_t;
typedef svr4_nlink_t	uw7_nlink_t;
typedef svr4_uid_t	uw7_uid_t;
typedef svr4_gid_t	uw7_gid_t;
typedef int64_t		uw7_off_t;
typedef struct timeval	uw7_timestruc_t;

typedef u_int64_t	uw7_fsblkcnt64_t;
typedef u_int64_t	uw7_fsfilcnt64_t;


#define linux_to_uw7_dev_t(dev) \
	linux_to_svr4_dev_t(dev)

#define linux_to_uw7_ino_t(ino) \
	linux_to_svr4_ino_t(ino)

#define linux_to_uw7_uid_t(uid) \
	linux_to_svr4_uid_t(uid)

#define linux_to_uw7_gid_t(gid) \
	linux_to_svr4_gid_t(gid)

#endif /* _ABI_UW7_TYPES_H */
