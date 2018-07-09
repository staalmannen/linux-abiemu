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

#ident "%W% %G%"

/*
 * Support for mmap on SCO OpenServer 5.
 */
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/mman.h>
#include <asm/uaccess.h>

#include <abi/sco/mman.h>
#include <abi/sco/types.h>

#include <abi/util/trace.h>


int
sco_mmap(u_long addr, size_t len, int prot, int flags, int fd, sco_off_t off)
{
	struct file *file;
	u_long mapaddr;

	if (flags & SCO_MAP_UNIMPL) {
#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_UNIMPL,
		    "unsupported mmap flags: 0x%x\n", flags & SCO_MAP_UNIMPL);
#endif
		flags &= ~SCO_MAP_UNIMPL;
	}

	file = fget(fd);
	if (!file)
		return -EBADF;

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	down_write(&current->mm->mmap_sem);
	mapaddr = do_mmap(file, addr, len, prot, flags | MAP_FIXED, off);
	up_write(&current->mm->mmap_sem);

	fput(file);

	if (mapaddr == addr)
		return 0;
	return mapaddr;
}
