/*
 * elfmark - set ELF header e_flags value to an abi-specific value.
 *
 * This utility is used for marking ELF binaries (including shared libraries)
 * for use by the UW Linux kernel module that provides exec domain for PER_UW7
 * personality. Run only on IA32 architectures.
 *
 * Authors - Tigran Aivazian <tigran@veritas.com>,
 *		Christoph Hellwig <hch@caldera.de>
 *
 * This software is under GPL
 */
#ifndef _ELFMARK_H
#define _ELFMARK_H

/*
 * The e_flags value for SCO UnixWare 7/UDK binaries
 */
#ifndef EF_386_UW7
#define EF_386_UW7	0x314B4455
#endif

/*
 * The e_flags value for SCO OpenServer 5 binaries.
 */
#ifndef EF_386_OSR5
#define EF_386_OSR5	0x3552534f
#endif

/*
 * e_flags value with no special meaning.
 */
#ifndef EF_386_NONE
#define EF_386_NONE	0
#endif

#endif /* _ELFMARK_H */
