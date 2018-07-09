/*
 * Copyright (c) 2001 Caldera Deutschland GmbH.
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
#ifndef _ABI_SCO_MMAN_H
#define _ABI_SCO_MMAN_H

#ident "%W% %G%"

/*
 * SCO OpenServer memory mapped files declarations
 */

#define SCO_MAP_FAILED		((void *)-1)

/* protections flags for mmap() and mprotect() */
#define SCO_PROT_NONE		0x0000
#define SCO_PROT_READ		0x0001
#define SCO_PROT_WRITE		0x0002
#define SCO_PROT_EXEC		0x0004

/* sharing types for mmap() */
#define SCO_MAP_SHARED		0x0001
#define SCO_MAP_PRIVATE		0x0002
#define SCO_MAP_FIXED		0x0010
#define SCO_MAP_PHMEM		0x1000
#define SCO_MAP_KVMEM		0x2000
#define SCO_MAP_ANCESTRAL	0x4000
#define SCO_MAP_NOEOF		0x8000

#define SCO_MAP_UNIMPL \
	(SCO_MAP_PHMEM|SCO_MAP_KVMEM|SCO_MAP_ANCESTRAL|SCO_MAP_NOEOF)

/* memcntl() subfunctions */
#define SCO_MC_SYNC		0x0001
#define SCO_MC_LOCK		0x0002
#define SCO_MC_UNLOCK		0x0003
#define SCO_MC_LOCKAS		0x0005
#define SCO_MC_UNLOCKAS		0x0006
#define SCO_MC_MAPCPU		0x8000
#define SCO_MC_MAPUBLK		0x8001

/* msync() flags */
#define SCO_MS_SYNC		0x0000
#define SCO_MS_ASYNC		0x0001
#define SCO_MS_INVALIDATE	0x0002

/* mlockall() flags */
#define SCO_MCL_CURRENT		0x0001
#define SCO_MCL_FUTURE		0x0002

#endif /* _ABI_SCO_MMAN_H */
