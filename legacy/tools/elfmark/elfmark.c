/*
 *	elfmark - set ELF header e_flags value to EF_386_UW7 (0x314B4455) or 0.
 *
 *	This utility is used for marking ELF binaries (including shared libraries)
 *	for use by the UW Linux kernel module that provides exec domain for PER_UW7 
 *	personality. Run only on IA32 architectures.
 *
 * 	Author - Tigran Aivazian <togran@veritas.com>
 *
 *	This software is under GPL
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <elf.h>
#include <asm/elf.h>

/* we might have non abi-capable kernel headers */
#ifndef EF_386_UW7
# define EF_386_UW7	0x314B4455
#endif

const char *progname = "elfmark";

int seteflags(char *filename, int value)
{
	int fd;
	Elf32_Ehdr ex;
	
	fd = open(filename, O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "%s: open(%s), errno=%d (%s)\n",
			progname, filename, errno, strerror(errno));
		return 1;
	}
	if (read(fd, &ex, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) {
		fprintf(stderr, "%s: read(), errno=%d (%s)\n",
			progname, errno, strerror(errno));
		return 2;
	}
	if (strncmp((char *)ex.e_ident, ELFMAG, SELFMAG) ||
		(ex.e_ident[EI_CLASS] != ELFCLASS32) ||
		(ex.e_ident[EI_DATA] != ELFDATA2LSB) ||
		(ex.e_ident[EI_VERSION] != EV_CURRENT) ||
		(ex.e_machine != EM_386) ) {

		fprintf(stderr, "%s: seteflags(%s,0x%x): not a valid ELF file\n",
			progname, filename, value);
		return 4;
	}
	if (lseek(fd, 0, SEEK_SET) == -1) {
		fprintf(stderr, "%s: lseek(), errno=%d (%s)\n",
			progname, errno, strerror(errno));
		return 3;
	}
	ex.e_flags = (Elf32_Word)value;
	if (write(fd, &ex, sizeof(Elf32_Ehdr)) != sizeof(Elf32_Ehdr)) {
		fprintf(stderr, "%s: write(), errno=%d (%s)\n",
			progname, errno, strerror(errno));
		return 5;
	}
	return close(fd);
}

void usage(void)
{
	fprintf(stderr, "usage: elfmark [-t uw7|none] [filename]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int c, eflags = -1;

	while (EOF != (c = getopt(argc, argv, "t:"))) {
		switch (c) {
			case 't':
				if (!strcmp(optarg, "uw7"))
					eflags = EF_386_UW7;
				else if (!strcmp(optarg, "none"))
					eflags = 0;
				break;
			default:
				usage();
				break;
		}
	}
	if (eflags == -1 || (optind >= argc))
		usage();
	return seteflags(argv[optind], eflags);
}
