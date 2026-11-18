/* SPDX-License-Identifier: GPL-2.0 */
/*
 * p9test - exercises the Plan 9 personality through the real Plan 9
 * system call convention (see sys.S).  TAP style output.
 */
typedef unsigned long u64;
typedef long vlong;
typedef unsigned char uchar;

#include "../../plan9/syscall_nr.h"
#define SYS(x)	PLAN9_SYS_##x

long p9sys(long nr, ...);
void notetramp(void);
extern struct { char prof[40]; u64 cyclefreq; vlong kc, pc; unsigned pid, clock; } *_tos;

#define OREAD	0
#define OWRITE	1
#define ORDWR	2
#define OTRUNC	16
#define DMDIR	0x80000000UL
#define RFPROC	(1 << 4)
#define RFFDG	(1 << 2)
#define RFNAMEG	(1 << 0)
#define NCONT	0

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

static long write(int fd, const void *b, long n) { return p9sys(SYS(pwrite), fd, b, n, -1L); }
static long read(int fd, void *b, long n) { return p9sys(SYS(pread), fd, b, n, -1L); }
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

static char errbuf[128];
static const char *errstr(void)
{
	errbuf[0] = 0;
	p9sys(SYS(errstr), errbuf, sizeof(errbuf));
	return errbuf;
}

/* parse a Dir: name and length */
static unsigned g16(const uchar *p) { return p[0] | p[1] << 8; }
static u64 g64(const uchar *p) { u64 v = 0; int i; for (i = 7; i >= 0; i--) v = v << 8 | p[i]; return v; }
static void dirname_(const uchar *d, char *out)
{
	unsigned n = g16(d + 41), i;

	for (i = 0; i < n && i < 63; i++)
		out[i] = d[43 + i];
	out[i] = 0;
}

static volatile int notes;
static char lastnote[128];

void notehandler(void *ureg, char *note)
{
	int i;

	for (i = 0; note[i] && i < 127; i++)
		lastnote[i] = note[i];
	lastnote[i] = 0;
	notes++;
	p9sys(SYS(noted), NCONT);
}

int main(int argc, char **argv)
{
	static uchar buf[8192];
	char name[64];
	long fd, n, pid;
	int pfd[2], i, found;

	if (argc > 1 && streq(argv[1], "child-exec")) {
		puts_("# exec'd child says hi\n");
		p9sys(SYS(exits), "from exec");
	}

	puts_("# p9test\n");
	check(_tos && _tos->pid > 1, "Tos has our pid");
	check(argc >= 1 && contains(argv[0], slen(argv[0]), "p9test"), "argv[0]");

	fd = p9sys(SYS(open), "/nonexistent", OREAD);
	check(fd == -1 && streq(errstr(), "file does not exist"),
	      "open of a missing file sets the error string");

	fd = p9sys(SYS(create), "/tmp/p9file", ORDWR, 0644L);
	check(fd >= 0, "create");
	check(write(fd, "hello plan 9\n", 13) == 13, "write");
	{
		vlong off = -1;

		check(p9sys(SYS(seek), &off, fd, 6L, 0L) == 0 && off == 6, "seek");
	}
	n = read(fd, buf, 100);
	check(n == 7 && buf[0] == 'p', "read after seek");
	n = p9sys(SYS(fstat), fd, buf, sizeof(buf));
	dirname_(buf, name);
	check(n > 49 && streq(name, "p9file") && g64(buf + 33) == 13,
	      "fstat: name and length");
	check(p9sys(SYS(fd2path), fd, name, sizeof(name)) == 0 &&
	      streq(name, "/tmp/p9file"), "fd2path");
	p9sys(SYS(close), fd);

	/* rename through wstat: every field "don't care" except the name */
	{
		static uchar d[64];
		const char *nn = "p9renamed";
		int k, l = slen(nn), sz = 49 + l;

		for (k = 0; k < sz; k++)
			d[k] = 0xff;
		d[0] = (sz - 2) & 0xff;
		d[1] = (sz - 2) >> 8;
		d[41] = l;
		d[42] = 0;
		for (k = 0; k < l; k++)
			d[43 + k] = nn[k];
		for (k = 43 + l; k < sz; k += 2)
			d[k] = d[k + 1] = 0;	/* empty uid, gid, muid */
		check(p9sys(SYS(wstat), "/tmp/p9file", d, (long)sz) == sz,
		      "wstat renames");
		n = p9sys(SYS(stat), "/tmp/p9renamed", buf, sizeof(buf));
		dirname_(buf, name);
		check(n > 0 && streq(name, "p9renamed"), "stat of the new name");
	}

	fd = p9sys(SYS(create), "/tmp/p9dir", OREAD, DMDIR | 0755);
	check(fd >= 0, "create a directory");
	p9sys(SYS(close), fd);
	fd = p9sys(SYS(create), "/tmp/p9dir/inner", OWRITE, 0600L);
	p9sys(SYS(close), fd);
	fd = p9sys(SYS(open), "/tmp", OREAD);
	found = 0;
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		for (i = 0; i < n; i += g16(buf + i) + 2) {
			dirname_(buf + i, name);
			if (streq(name, "p9dir") && (buf[i + 21] & 0x80))
				found |= 1;
			if (streq(name, "p9renamed"))
				found |= 2;
		}
	}
	check(found == 3, "directory read returns Dir records");
	p9sys(SYS(close), fd);
	check(p9sys(SYS(remove), "/tmp/p9dir") == -1, "remove of a full directory fails");
	check(p9sys(SYS(remove), "/tmp/p9dir/inner") == 0 &&
	      p9sys(SYS(remove), "/tmp/p9dir") == 0 &&
	      p9sys(SYS(remove), "/tmp/p9renamed") == 0, "remove");

	/* pipes are bidirectional */
	check(p9sys(SYS(pipe), pfd) == 0, "pipe");
	write(pfd[1], "back", 4);
	check(read(pfd[0], buf, 4) == 4 && buf[0] == 'b', "pipe works backwards");

	/* rfork, exits and await */
	pid = p9sys(SYS(rfork), RFPROC | RFFDG);
	if (pid == 0) {
		char b[16];
		long cp = _tos->pid;

		for (i = 0; i < 12; i++) { b[11 - i] = '0' + cp % 10; cp /= 10; }
		write(pfd[0], b, 12);
		p9sys(SYS(exits), "child failed politely");
	}
	check(pid > 0, "rfork");
	n = read(pfd[1], buf, 12);
	{
		long cp = 0;

		for (i = 0; i < 12; i++) cp = cp * 10 + buf[i] - '0';
		check(n == 12 && cp == pid, "child's Tos has the child's pid");
	}
	n = p9sys(SYS(await), buf, sizeof(buf) - 1);
	buf[n > 0 ? n : 0] = 0;
	check(n > 0 && contains((char *)buf, n, "'child failed politely'"),
	      "await returns the exits() message");

	/* exec */
	pid = p9sys(SYS(rfork), RFPROC | RFFDG);
	if (pid == 0) {
		char *av[] = { argv[0], "child-exec", 0 };

		p9sys(SYS(exec), argv[0], av);
		p9sys(SYS(exits), "exec failed");
	}
	n = p9sys(SYS(await), buf, sizeof(buf) - 1);
	check(n > 0 && contains((char *)buf, n, "'from exec'"), "exec of an a.out");

	/* notes */
	check(p9sys(SYS(notify), notetramp) == 0, "notify");
	p9sys(SYS(alarm), 50L);
	p9sys(SYS(sleep), 500L);
	check(notes == 1 && streq(lastnote, "alarm"), "alarm note delivered");

	/* time */
	{
		vlong t1 = p9sys(SYS(_nsec)), t2;

		p9sys(SYS(sleep), 20L);
		t2 = p9sys(SYS(_nsec));
		check(t1 > 1000000000000000000L && t2 - t1 >= 20000000, "nsec and sleep");
	}

	/* rendezvous between two processes */
	pid = p9sys(SYS(rfork), RFPROC | RFFDG);
	if (pid == 0) {
		long v = p9sys(SYS(rendezvous), 0x1234L, 7L);

		p9sys(SYS(exits), v == 9 ? 0 : "bad rendezvous");
	}
	n = p9sys(SYS(rendezvous), 0x1234L, 9L);
	check(n == 7, "rendezvous exchanges values");
	n = p9sys(SYS(await), buf, sizeof(buf) - 1);
	check(n > 0 && contains((char *)buf, n, "''"), "rendezvous child exits cleanly");

	/* name spaces: a private bind */
	pid = p9sys(SYS(rfork), RFPROC | RFFDG | RFNAMEG);
	if (pid == 0) {
		p9sys(SYS(create), "/tmp/bindsrc", OREAD, DMDIR | 0755);
		p9sys(SYS(create), "/tmp/bindsrc/marker", OWRITE, 0644L);
		p9sys(SYS(create), "/tmp/bindtgt", OREAD, DMDIR | 0755);
		if (p9sys(SYS(bind), "/tmp/bindsrc", "/tmp/bindtgt", 0L) < 0)
			p9sys(SYS(exits), "bind failed");
		fd = p9sys(SYS(open), "/tmp/bindtgt/marker", OREAD);
		p9sys(SYS(exits), fd >= 0 ? 0 : "marker missing");
	}
	n = p9sys(SYS(await), buf, sizeof(buf) - 1);
	buf[n > 0 ? n : 0] = 0;
	check(n > 0 && contains((char *)buf, n, "''"), "bind in a private name space");
	fd = p9sys(SYS(open), "/tmp/bindtgt/marker", OREAD);
	check(fd == -1, "the bind is not visible outside");

	puts_(nfail ? "# FAILED " : "# all passed ");
	putnum(ntest - nfail);
	puts_("/");
	putnum(ntest);
	puts_("\n");
	p9sys(SYS(exits), nfail ? "fail" : 0);
	return 0;
}
