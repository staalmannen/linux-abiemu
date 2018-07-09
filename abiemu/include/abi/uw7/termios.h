#ifndef _ABI_UW7_TERMIOS_H
#define _ABI_UW7_TERMIOS_H

#define UW7_NCCS		(19)
struct uw7_termios {
	unsigned long c_iflag;
	unsigned long c_oflag;
	unsigned long c_cflag;
	unsigned long c_lflag;
	unsigned char c_cc[UW7_NCCS];
};

#endif /* _ABI_UW7_TERMIOS_H */
