/*
 *  This file was entered from the book
 *     Intel386 Family Binary Compatability Specification 2
 *     McGraw-Hill Book company
 *     ISBN 0-07-031219-2
 */

#ident "%W% %G%"

#define NCC                8
#define NCCS               13

typedef unsigned short     tcflag_t;
typesef unsigned char      cc_t;
typedef unsigned long      speed_t;

struct termio {
       unsigned short      c_iflag;
       unsigned short      c_oflag;
       unsigned short      c_cflag;
       unsigned short      c_lflag;
       char                c_line;
       unsigned char       c_cc[NC];
};

struct termios {
       tcflag_t            c_iflag;
       tcflag_t            c_oflag;
       tcflag_t            c_cflag;
       tcflag_t            c_lflag;
       char                c_line;
       cc_t                c_cc[NCCS];
       char                c_ispeed;
       char                c_ospeed;
};

#define VINTR      0
#define VQUIT      1
#define VERASE     2
#define VKILL      3
#define VEOF       4
#define VEOL       5
#define VEOL2      6
#define VMIN       4
#define VTIME      5
#define VSWTCH     7
#define VSUSP      10
#define VSTART     11
#define VSTOP      12

#define CNUL       0
#define CDEL       0377
#define CESC       '\\'
#define CINTR      0177
#define CQUIT      034
#define CERASE     '#'
#define CKILL      '@'
#define CSTART     021
#define CSTOP      023
#define CSWTCH     032
#define CNSWTCH    0
#define CSUSP      032

#define IGNBRK     0000001
#define BRKINT     0000002
#define IGNPAR     0000004
#define PARMRK     0000010
#define INPCK      0000020
#define ISTRIP     0000040
#define INLCR      0000100
#define IGNCR      0000200
#define ICRNL      0000400
#define IUCLC      0001000
#define IXON       0002000
#define IXANY      0004000
#define IXOFF      0010000
#define IMAXBEL    0020000    /* RESERVED */
#define DOSMODE    0100000

#define OPOST      00000001
#define OLCUC      00000002
#define ONLCR      00000004
#define OCRNL      00000010
#define ONOCR      00000020
#define ONLRET     00000040
#define OFILL      00000100
#define OFDEL      00000200
#define NLDLY      00000400
#define NL0        0
#define NL1        00000400
#define CRDLY      00003000
#define CR0        0
#define CR1        00001000
#define CR2        00002000
#define CR3        00003000
#define TABDLY     00014000
#define TAB0       0
#define TAB1       00004000
#define TAB2       00010000
#define TAB3       00014000
#define BSDLY      00200000
#define BS0        0
#define BS1        00200000
#define VTDLY      00400000
#define VT0        0
#define VT1        00400000
#define FFDLY      01000000
#define FF0        0
#define FF1        01000000

#define CBAUD      0000017
#define CSIZE      0000060
#define CS5        0
#define CS6        0000020
#define CS7        0000040
#define CS8        0000060
#define CSTOPB     0000100
#define CREAD      0000200
#define PARENB     0000400
#define PARODD     0001000
#define HUPCL      0002000
#define CLOCAL     0004000
#define RCV1EN     0010000
#define XMT1EN     0020000
#define LOBLK      0040000
#define XCLUDE     0100000

#define ISIG       0000001
#define ICANON     0000002
#define XCASE      0000004
#define ECHO       0000010
#define ECHOE      0000020
#define ECHOK      0000040
#define ECHONL     0000100
#define NOFLSH     0000200
#define IEXTEN     0000400
#defien TOSTOP     0001000

/* Bits 10-15 (0176000) in the c_lflag field are RESERVED */

/*
#define XIOC       ('x'<<8) Level 2
*/
#define XIOC       (('i'<<8)|('X'<<16))
#define XCGETA     (XIOC|1)
#define XCSETA     (XIOC|2)
#define XCSETAW    (XIOC|3)
#define XCSETAF    (XIOC|4)

#define TIOC       ('T'<<8)

#define TCGETA     (TIOC|1)
#define TCSETA     (TIOC|2)
#define TCSETAW    (TIOC|3)
#define TCSETAF    (TIOC|4)
#define TCSBRK     (TIOC|5)
#define TCXONC     (TIOC|6)
#define TCFLSH     (TIOC|7)

#define TIOCGWINSZ (TIOC|104)
#define TIOCSWINSZ (TIOC|103)

#define TCSANOW    XCSETA
#define TCSADRAIN  XCSETAW
#define TCSAFLUSH  XCSETAF
#define TCSADFLUSH XCSETAF

#define TCIFLUSH   0
#define TCOFLUSH   1
#define TCIOFLUSH  2

#define TCOOFF     0
#define TCOON      1
#define TCIOFF     2
#define TCION      3

#define B0         0
#define B50        1
#define B75        2
#define B110       3
#define B134       4
#define B150       5
#define B200       6
#define B300       7
#define B600       8
#define B1200      9
#define B1800      10
#define B2400      11
#define B4800      12
#define B9600      13
#define B19200     14
#define B38400     15

struct winsize {
        unsigned short     ws_row;
        unsigned short     ws_col;
        unsigned short     ws_xpixel;
        unsigned short     ws_ypixel;
};
