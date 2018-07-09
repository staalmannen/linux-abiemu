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
#ifndef _ABI_SCO_SYSENT_H
#define _ABI_SCO_SYSENT_H

#ident "%W% %G%"

/*
 * External function declarations for the SCO OpenServer syscall table.
 */

#include <abi/sco/types.h>

struct sco_statvfs;


/* ioctl.c */
extern int	sco_ioctl(struct pt_regs *);

/* misc.c */
extern int	sco_lseek(int, u_long, int);
extern int	sco_fcntl(int, u_int, u_long);
extern int	sco_sysi86(int, void *, int);

/* mmap.c */
extern int	sco_mmap(u_long, size_t, int, int, int, sco_off_t);

/* ptrace.c */
extern int	sco_ptrace(int, int, u_long, u_long);

/* secureware.c */
extern int	sw_security(int, void *, void *, void *, void *, void *);

/* stat.c */
extern int	sco_xstat(int, char *, void *);
extern int	sco_lxstat(int, char *, void *);
extern int	sco_fxstat(int, int, void *);

/* statvfs.c */
extern int	sco_statvfs(char *, struct sco_statvfs *);
extern int	sco_fstatvfs(int, struct sco_statvfs *);

#endif /* _ABI_SCO_SYSENT_H */
