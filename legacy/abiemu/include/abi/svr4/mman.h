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
#ifndef _ABI_SVR4_MMAN_H
#define _ABI_SVR4_MMAN_H

#ident "%W% %G%"

/*
 * SVR4 memory mapped files declarations
 */

#define SVR4_MAP_FAILED		((void *)-1)

/* protections flags for mmap() and mprotect() */
#define SVR4_PROT_NONE		0x0000
#define SVR4_PROT_READ		0x0001
#define SVR4_PROT_WRITE		0x0002
#define SVR4_PROT_EXEC		0x0004

/* sharing types for mmap() */
#define SVR4_MAP_SHARED		0x0001
#define SVR4_MAP_PRIVATE	0x0002
#define SVR4_MAP_FIXED		0x0010
#define SVR4_MAP_RENAME		0x0020
#define SVR4_MAP_NORESERVE	0x0040
#define SVR4_MAP_ANONYMOUS	0x0100
#define SVR4__MAP_NEW		0x80000000

#define SVR4_MAP_UNIMPL		(SVR4_MAP_RENAME|SVR4__MAP_NEW)

/* memcntl() subfunctions */
#define SVR4_MC_SYNC		0x0001
#define SVR4_MC_LOCK		0x0002
#define SVR4_MC_UNLOCK		0x0003
#define SVR4_MC_ADVISE		0x0004
#define SVR4_MC_LOCKAS		0x0005
#define SVR4_MC_UNLOCKAS	0x0006

/* msync() flags */
#define SVR4_MS_SYNC		0x0000
#define SVR4_MS_ASYNC		0x0001
#define SVR4_MS_INVALIDATE	0x0002

/* mlockall() flags */
#define SVR4_MCL_CURRENT	0x0001
#define SVR4_MCL_FUTURE		0x0002

/* madvice() advices */
#define SVR4_MADV_NORMAL	0x0000
#define SVR4_MADV_RANDOM	0x0001
#define SVR4_MADV_SEQUENTIAL	0x0002
#define SVR4_MADV_WILLNEED	0x0003
#define SVR4_MADV_DONTNEED	0x0004

#endif /* _ABI_SVR4_MMAN_H */
