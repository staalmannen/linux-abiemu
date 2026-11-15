/*
 * Copyright (c) 1999 Tigran Aivazian.
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
 * Support for mmap on UnixWare 7.
 */
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/mman.h>

#include <asm/uaccess.h>


/* the other MAP_XXX values are the same as on Linux/i386 */
#define UW7_MAP_ANONYMOUS	0x100

int
uw7_mmap64(u_long addr, size_t len, int prot, int flags,
	   int fd, u_int off, u_int off_hi)
{
	loff_t			off64 = (off | ((loff_t)off_hi << 32));
	u_long			pgoff = (off64 >> PAGE_SHIFT);
	int			error = -EBADF;
	struct file		*fp = NULL;

	if ((off64 + PAGE_ALIGN(len)) < off64)
		return -EINVAL;

	if (!(off64 & ~PAGE_MASK))
		return -EINVAL;

	if (flags & UW7_MAP_ANONYMOUS) {
		flags |= MAP_ANONYMOUS;
		flags &= ~UW7_MAP_ANONYMOUS;
	}

	flags &= ~(MAP_EXECUTABLE | MAP_DENYWRITE);
	if (!(flags & MAP_ANONYMOUS)) {
		if (!(fp = fget(fd)))
			goto out;
	}

	down_write(&current->mm->mmap_sem);
	error = do_mmap_pgoff(fp, addr, len, prot, flags, pgoff);
	up_write(&current->mm->mmap_sem);

	if (fp)
		fput(fp);
out:
	return (error);
}
