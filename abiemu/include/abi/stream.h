/*
 * Copyright (C) 1994  Mike Jagdis (jaggy@purplet.demon.co.uk)
 */
#ifndef _IBCS_STREAM_H_
#define _IBCS_STREAM_H_

#ident "%W% %G%"

#define MSG_HIPRI	1
#define RS_HIPRI	MSG_HIPRI
#define MSG_ANY		2
#define MSG_BAND	4

#define MORECTL		1
#define MOREDATA	2

struct strbuf {
	int	maxlen;		/* size of buffer */
	int	len;		/* number of bytes in buffer */
	char	*buf;		/* pointer to buffer */
};

/* Used for the I_PEEK STREAMS ioctl. */
struct strpeek {
	struct strbuf ctl;
	struct strbuf dat;
	long flags;
};

/* Used for the I_FDINSERT STREAMS ioctl. */
struct strfdinsert {
	struct strbuf	ctlbuf;
	struct strbuf	datbuf;
	long		flags;
	unsigned int	fildes;
	int		offset;
};

extern int stream_fdinsert(struct pt_regs *regs, int fd,
				struct strfdinsert *arg);

#endif
