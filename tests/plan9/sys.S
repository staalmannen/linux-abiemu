/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Plan 9 amd64 run time for the tests: the system call convention
 * (number in RARG = %rbp, arguments on the stack above the return
 * address), _main and a note handler entry.
 */
	.section .text._main, "ax"
	.globl _main
_main:					/* AX = Tos, 0(SP) = argc */
	movq	%rax, _tos(%rip)
	movq	(%rsp), %rdi
	leaq	8(%rsp), %rsi
	andq	$-16, %rsp
	call	main
	xorl	%edi, %edi
	movq	%rdi, %rsi
	movq	$8, %rdi		/* EXITS */
	call	p9sys
	hlt

	.text
/* long p9sys(nr, a0, a1, a2, a3, a4, a5) using the SysV convention */
	.globl p9sys
p9sys:
	pushq	%rbp
	movq	16(%rsp), %rax		/* a5 */
	subq	$56, %rsp
	movq	%rsi, 8(%rsp)
	movq	%rdx, 16(%rsp)
	movq	%rcx, 24(%rsp)
	movq	%r8, 32(%rsp)
	movq	%r9, 40(%rsp)
	movq	%rax, 48(%rsp)
	movq	%rdi, %rbp
	syscall
	addq	$56, %rsp
	popq	%rbp
	ret

/* note handler: RARG = ureg, 8(SP) = ureg, 16(SP) = note */
	.weak notehandler
	.globl notetramp
notetramp:
	movq	8(%rsp), %rdi
	movq	16(%rsp), %rsi
	andq	$-16, %rsp
	call	notehandler
	hlt

	.data
	.globl _tos
_tos:	.quad 0
	.section .note.GNU-stack, "", @progbits
