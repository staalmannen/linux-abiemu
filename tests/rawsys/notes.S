/* SPDX-License-Identifier: GPL-2.0 */
/* ELF notes that brand the test binaries */
#if defined(OS_NETBSD)
	.section .note.netbsd.ident, "a", @note
	.p2align 2
	.long 7, 4, 1
	.asciz "NetBSD"
	.p2align 2
	.long 1001000000
#elif defined(OS_OPENBSD)
	.section .note.openbsd.ident, "a", @note
	.p2align 2
	.long 8, 4, 1
	.asciz "OpenBSD"
	.p2align 2
	.long 0
#endif
	.section .note.GNU-stack, "", @progbits
