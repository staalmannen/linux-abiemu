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
#ifndef _ASM_ABI_MACHDEP_H
#define _ASM_ABI_MACHDEP_H

#include <linux/ptrace.h>

/**
 * get_syscall_parameter - get syscall parameter
 * @regs: saved register contents
 * @n: nth syscall to get
 *
 * This function gets the nth syscall parameter if
 * the syscall got a &struct pt_regs * passed instead
 * of the traditional C calling convention.
 */
static __inline unsigned long
get_syscall_parameter(struct pt_regs *regs, int n)
{
	unsigned long r;

	__get_user(r, ((unsigned long *)regs->esp)+(n+1));
	return r;
}

/**
 * set_error - set error number
 * @regs: saved register contents
 * @errno: error number
 *
 * This function sets the syscall error return for lcall7
 * calling conventions to @errno.
 */
static __inline void
set_error(struct pt_regs *regs, int errno)
{
	regs->eax = errno;
	regs->eflags |= 1;
}

/**
 * clear_error - clear error return flag
 * @regs: saved register contents
 *
 * This funtion clears the flag that indicates an error
 * return for lcall7-based syscalls.
 */
static __inline void
clear_error(struct pt_regs *regs)
{
	regs->eflags &= ~1;
}

/**
 * set_result - set syscall return value
 * @regs: saved register contents
 *
 * This funtion sets the return value for syscalls
 * that have the saved register set calling convention.
 */
static __inline void
set_result(struct pt_regs *regs, int r)
{
	regs->eax = r;
}

/**
 * get_result - get syscall return value
 * @regs: saved register contents
 *
 * This funtion gets the return value for syscalls
 * that have the saved register set calling convention.
 */
static __inline int
get_result(struct pt_regs *regs)
{
	return regs->eax;
}

#endif /* _ASM_ABI_MACHDEP_H */
