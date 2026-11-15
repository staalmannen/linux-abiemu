#ifndef _ABI_SVR4_TERMIOS_H
#define _ABI_SVR4_TERMIOS_H

#ident "%W% %G%"

/*
 * SVR4 termios declarations.
 */

#define SVR_NCC 8
struct svr_termio {
	u_int16_t	c_iflag;
	u_int16_t	c_oflag;
	u_int16_t	c_cflag;
	u_int16_t	c_lflag;
	char		c_line;
	u_char		c_cc[SVR_NCC];
};

#define SVR4_NCCS (19)
struct svr4_termios {
	u_long		c_iflag;
	u_long		c_oflag;
	u_long		c_cflag;
	u_long		c_lflag;
	u_char		c_cc[SVR4_NCCS];
};

#endif /* _ABI_SVR4_TERMIOS_H */
