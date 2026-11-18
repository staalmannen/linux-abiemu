/* SPDX-License-Identifier: GPL-2.0 */
/* Tiny Plan 9 "libc" for the tests (see sys.S) */
typedef unsigned long u64;
typedef long vlong;
typedef unsigned char uchar;

#include "../../plan9/syscall_nr.h"
#define SYS(x)	PLAN9_SYS_##x

long p9sys(long nr, ...);

#define OREAD	0
#define OWRITE	1
#define ORDWR	2
#define RFPROC	(1 << 4)
#define RFFDG	(1 << 2)

static long slen(const char *s) { long n = 0; while (s[n]) n++; return n; }
static int streq(const char *a, const char *b)
{
	while (*a && *a == *b) a++, b++;
	return *a == *b;
}
static int contains(const char *h, long n, const char *needle)
{
	long i, j, m = slen(needle);

	for (i = 0; i + m <= n; i++) {
		for (j = 0; j < m && h[i + j] == needle[j]; j++)
			;
		if (j == m)
			return 1;
	}
	return 0;
}
static char *scat(char *d, const char *s) { while (*d) d++; while ((*d++ = *s++)); return d - 1; }

static long write(int fd, const void *b, long n) { return p9sys(SYS(pwrite), fd, b, n, -1L); }
static long read(int fd, void *b, long n) { return p9sys(SYS(pread), fd, b, n, -1L); }
static long open(const char *f, int m) { return p9sys(SYS(open), f, (long)m); }
static void close(int fd) { p9sys(SYS(close), (long)fd); }
static void puts_(const char *s) { write(1, s, slen(s)); }
static void putnum(long v)
{
	char b[24]; int i = 23;

	b[i] = 0;
	if (v < 0) { puts_("-"); v = -v; }
	do { b[--i] = '0' + v % 10; v /= 10; } while (v);
	puts_(b + i);
}

static int ntest, nfail;
static void check(int ok, const char *what)
{
	ntest++;
	nfail += !ok;
	puts_(ok ? "ok " : "not ok ");
	putnum(ntest);
	puts_(" - ");
	puts_(what);
	puts_("\n");
}

static int done(void)
{
	puts_(nfail ? "# FAILED " : "# all passed ");
	putnum(ntest - nfail);
	puts_("/");
	putnum(ntest);
	puts_("\n");
	p9sys(SYS(exits), nfail ? "fail" : 0);
	return 0;
}
