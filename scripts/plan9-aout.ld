/* SPDX-License-Identifier: GPL-2.0 */
/* Lay out an ELF the way a Plan 9 amd64 a.out is loaded (elf2aout.py) */
ENTRY(_main)
PHDRS { text PT_LOAD; data PT_LOAD; }
SECTIONS
{
	. = 0x200028;
	.text : { *(.text._main) *(.text*) *(.rodata*) } :text
	. = ALIGN(0x200000);
	.data : { *(.data*) } :data
	.bss : { *(.bss*) *(COMMON) } :data
	/DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) *(.got*) }
}
