/*
 * Copyright (c) 1994  Mike Jagdis (jaggy@purplet.demon.co.uk)
 */
#ifndef _ABI_XOUT_H_
#define _ABI_XOUT_H_

#ident "%W% %G%"

/*
 * This file is based on available documentation for the Xenix x.out
 * format. Much is missing here. There is just enough to allow us to
 * support 386 small model and not a lot more.
 */

/*
 * X.out header
 */
struct xexec {
	u_short		x_magic;	/* magic number			*/
	u_short		x_ext;		/* size of header extension	*/

	/*
	 * For segmented binaries the following sizes are the sums
	 * of the segment sizes.
	 */
	long		x_text;		/* size of text segments	*/
	long		x_data;		/* size of initialized data	*/
	long		x_bss;		/* size of uninitialized data	*/
	long		x_syms;		/* size of symbol table		*/
	long		x_reloc;	/* relocation table length	*/

	long		x_entry;	/* entry point			*/
	char		x_cpu;		/* cpu type and byte/word order	*/
	char		x_relsym;	/* undefined			*/
	u_short		x_renv;		/* run-time environment		*/
};

/*
 * X.out header extension
 */
struct xext {
	/*
	 * These are unused.
	 * */
	long		xe_trsize;	/* ???				*/
	long		xe_drsize;	/* ???				*/
	long		xe_tbase;	/* ???				*/
	long		xe_dbase;	/* ???				*/

	long		xe_stksize;	/* stack size (if XE_FS set)	*/

	/*
	 * The following are present if XE_SEG is set.
	 */
	long		xe_segpos;	/* offset to segment table	*/
	long		xe_segsize;	/* segment table size		*/
	long		xe_mdtpos;	/* offset to machdep table	*/
	long		xe_mdtsize;	/* machine dependent table size	*/
	char		xe_mdttype;	/* machine dependent table type	*/
	char		xe_pagesize;	/* file pagesize, in 512 units	*/
	char		xe_ostype;	/* operating system type	*/
	char		xe_osvers;	/* operating system version	*/
	u_short		xe_eseg;	/* entry segment, machdep	*/
	u_short		xe_sres;	/* reserved			*/
};

/*
 * X.out segment description.
 */
struct xseg {
	u_short		xs_type;	/* segment type			*/
	u_short		xs_attr;	/* segment attributes		*/
	u_short		xs_seg;		/* segment number		*/
	char		xs_align;	/* log base 2 of alignment	*/
	char		xs_cres;	/* unused			*/
	u_long		xs_filpos;	/* file position		*/
	u_long		xs_psize;	/* physical size (in file)	*/
	u_long		xs_vsize;	/* virtual size (in core)	*/
	u_long		xs_rbase;	/* relocation base addr/offset	*/
	u_short		xs_noff;	/* segment name table offset	*/
	u_short		xs_sres;	/* unused			*/
	long		xs_lres;	/* unused			*/
};


/*
 * Magic number for an x.out header.
 */
#define X_MAGIC		0x0206	/* indicates x.out header */

/*
 * Codes for x_cpu.
 */
#define XC_BSWAP	0x80	/* bytes swapped */
#define XC_WSWAP	0x40	/* words swapped */
#define XC_8086		0x04	/* I8086 */
#define XC_286		0x09	/* iAPX 80286 */
#define XC_286V		0x29	/* iAPX 80286, use xe_osver for version */
#define XC_386		0x0a	/* iAPX 80386 */
#define XC_186		0x0b	/* iAPX 80186 */
#define XC_CPU		0x3f	/* cpu mask */

/*
 * Flags for the run-time environment.
 */
#define XE_V2		0x4000		/* version 2.x */
#define XE_V3		0x8000		/* version 3.x */
#define XE_OSV		0xc000		/* if XE_SEG use xe_osvers ... */
#define XE_V5		XE_OSV		/* else assume v5.x */
#define XE_VERS		0xc000		/* version mask */

#define XE_5_3		0x2000		/* binary needs 5.3 functionality */
#define XE_LOCK		0x1000		/* Use Advisory locking */
#define XE_SEG		0x0800		/* segment table present */
#define XE_ABS		0x0400		/* absolute memory image (standalone) */
#define XE_ITER		0x0200		/* iterated text/data present */
#define XE_HDATA	0x0100		/* huge model data (never used) */
#define XE_VMOD		XE_HDATA	/* virtual module */
#define XE_FPH		0x0080		/* floating point hardware required */
#define XE_LTEXT	0x0040		/* large model text */
#define XE_LDATA	0x0020		/* large model data */
#define XE_OVER		0x0010		/* text overlay */
#define XE_FS		0x0008		/* fixed stack */
#define XE_PURE		0x0004		/* pure text */
#define XE_SEP		0x0002		/* separate I & D */
#define XE_EXEC		0x0001		/* executable */


/*
 * Segment types.
 */
#define	XS_TNULL	0	/* unused segment */
#define	XS_TTEXT	1	/* text segment */
#define	XS_TDATA	2	/* data segment */
#define	XS_TSYMS	3	/* symbol table segment */
#define	XS_TREL		4	/* relocation segment */
#define	XS_TSESTR	5	/* segment table's string table segment */
#define	XS_TGRPS	6	/* group definitions segment */

#define	XS_TIDATA	64	/* iterated data */
#define	XS_TTSS		65	/* tss */
#define	XS_TLFIX	66	/* lodfix */
#define	XS_TDNAME	67	/* descriptor names */
#define	XS_TDTEXT	68	/* debug text segment */
#define	XS_TIDBG	XS_TDTEXT
#define	XS_TDFIX	69	/* debug relocation */
#define	XS_TOVTAB	70	/* overlay table */
#define	XS_T71		71
#define	XS_TSYSTR	72	/* symbol string table */

/*
 * Segment attributes.
 */
#define XS_AMEM		0x8000	/* is a memory image */

/*
 * For text and data segment types
 */
#define XS_AITER	0x0001	/* contains iteration records */
#define XS_AHUGE	0x0002	/* contains huge element */
#define XS_ABSS		0x0004	/* contains implicit BSS */
#define XS_APURE	0x0008	/* read only, shareable */
#define XS_AEDOWN	0x0010	/* expands downward (stack) */
#define XS_APRIV	0x0020	/* may not be combined */
#define XS_A32BIT	0x0040	/* is 32 bit */

/*
 * File position macros, valid only if !XE_SEG.
 */
#define XEXTPOS(xp)	((long) sizeof(struct xexec))
#define XTEXTPOS(xp)	(XEXTPOS(xp) + (long) (xp)->x_ext)
#define XDATAPOS(xp)	(XTEXTPOS(xp) + (xp)->x_text)
#define XBSSPOS(xp)	(XDATAPOS(xp) + (xp)->x_data)
#define XSYMPOS(xp)	(XDATAPOS(xp) + (xp)->x_data)
#define XRELPOS(xp)	(XSYMPOS(xp) + (xp)->x_syms)
#define XENDPOS(xp)	(XRELPOS(xp) + (xp)->x_reloc)

#define XRTEXTPOS(xp, ep)	(XRELPOS(xp))
#define XRDATAPOS(xp, ep)	(XRELPOS(xp) + (ep)->xe_trsize)


/*
 * Specials for the Linux Xenix 286 emulator.
 *
 * The base address that 286 segments are loaded above. This should be
 * above memory used by the emulator overlay. Actual segment data
 * starts slightly higher than this since we map the xexec and xext
 * structures of the executable to this address.
 */
#define X286_MAP_ADDR	0x4000000

#endif /* _ABI_XOUT_H_ */
