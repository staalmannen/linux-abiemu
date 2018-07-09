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

#ident "%W% %G%"

/*
 * Support for mmap on SVR4 and derivates.
 */
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/mman.h>
#include <linux/module.h>

#include <asm/uaccess.h>

#include <abi/svr4/mman.h>
#include <abi/svr4/types.h>

#include <abi/util/trace.h>


u_long
svr4_mmap(u_long addr, size_t len, int prot, int flags, int fd, svr4_off_t off)
{
	struct file *file = NULL;
	u_long mapaddr;

	if (flags & SVR4_MAP_UNIMPL) {
#if defined(CONFIG_ABI_TRACE)
		abi_trace(ABI_TRACE_UNIMPL,
		    "unsupported mmap flags: 0x%x\n", flags & SVR4_MAP_UNIMPL);
#endif
		flags &= ~SVR4_MAP_UNIMPL;
	}

	if (!(flags & SVR4_MAP_ANONYMOUS)) {
		file = fget(fd);
		if (!file)
			goto Ebadfd;

		flags &= ~SVR4_MAP_ANONYMOUS;
		flags |= MAP_ANONYMOUS;
	}

	down_write(&current->mm->mmap_sem);
	mapaddr = do_mmap(file, addr, len, prot, flags, off);
	up_write(&current->mm->mmap_sem);

	if (file)
		fput(file);
	return mapaddr;
Ebadfd:
	return -EBADFD;
}

#if defined(CONFIG_ABI_SYSCALL_MODULES)
EXPORT_SYMBOL(svr4_mmap);
#endif
