#ifndef _ABI_CXENIX_SIGNAL_H
#define _ABI_CXENIX_SIGNAL_H

enum {
	SCO_SA_NOCLDSTOP =	0x001,
	SCO_SA_COMPAT =		0x080, /* 3.2.2 compatibilty. */
	SCO_SA_SIGNAL =		0x100,
};

struct sco_sigaction {
	void		(*sa_handler)(int);
	unsigned long	sa_mask;
	int		sa_flags;
};

#endif /* _ABI_CXENIX_SIGNAL_H */
