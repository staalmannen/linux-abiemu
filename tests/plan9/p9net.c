/* SPDX-License-Identifier: GPL-2.0 */
/*
 * p9net - dial/announce/listen through /net the way the Plan 9 C
 * library does it.
 */
#include "p9lib.h"

/* csquery: "tcp!127.0.0.1!5640" -> clone path and address */
static int cs(const char *req, char *clone, char *addr)
{
	char buf[256];
	long fd = open("/net/cs", ORDWR), n, i;

	if (fd < 0)
		return -1;
	if (write(fd, req, slen(req)) < 0)
		return -1;
	p9sys(SYS(seek), &n, fd, 0L, 0L);
	n = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if (n <= 0)
		return -1;
	buf[n] = 0;
	for (i = 0; buf[i] && buf[i] != ' '; i++)
		clone[i] = buf[i];
	clone[i] = 0;
	scat(addr, buf + i + 1);
	return 0;
}

/* open clone, write the ctl message, return ctl fd and dir name */
static long conv(const char *clone, const char *msg, char *dir)
{
	char num[16];
	long ctl = open(clone, ORDWR), n;

	if (ctl < 0)
		return -1;
	n = read(ctl, num, sizeof(num) - 1);
	if (n <= 0)
		return -1;
	num[n] = 0;
	dir[0] = 0;
	scat(scat(dir, "/net/tcp/"), num);
	if (write(ctl, msg, slen(msg)) < 0)
		return -1;
	return ctl;
}

int main(int argc, char **argv)
{
	char clone[64], addr[64], dir[64], ldir[64], path[96], msg[96], buf[128];
	long actl, lfd, dfd, pid, n;

	puts_("# p9net\n");
	clone[0] = addr[0] = 0;
	check(cs("tcp!*!5640", clone, addr) == 0 && streq(clone, "/net/tcp/clone") &&
	      streq(addr, "5640"), "cs translates an announce address");

	msg[0] = 0;
	scat(scat(msg, "announce "), addr);
	actl = conv(clone, msg, dir);
	check(actl >= 0, "announce");
	path[0] = 0;
	scat(scat(path, dir), "/status");
	n = open(path, OREAD);
	n = read(n, buf, sizeof(buf));
	check(n > 0 && contains(buf, n, "Listen"), "status says Listen");

	pid = p9sys(SYS(rfork), RFPROC | RFFDG);
	if (pid == 0) {
		char cdir[64];
		long c;

		clone[0] = addr[0] = 0;
		if (cs("tcp!localhost!5640", clone, addr) < 0)
			p9sys(SYS(exits), "cs failed");
		msg[0] = 0;
		scat(scat(msg, "connect "), addr);
		c = conv(clone, msg, cdir);
		if (c < 0)
			p9sys(SYS(exits), "connect failed");
		path[0] = 0;
		scat(scat(path, cdir), "/data");
		dfd = open(path, ORDWR);
		write(dfd, "hello from the client", 21);
		n = read(dfd, buf, sizeof(buf));
		p9sys(SYS(exits), n == 5 && buf[0] == 't' ? 0 : "bad reply");
	}

	path[0] = 0;
	scat(scat(path, dir), "/listen");
	lfd = open(path, ORDWR);
	check(lfd >= 0, "listen returns a call");
	n = read(lfd, buf, sizeof(buf) - 1);
	buf[n > 0 ? n : 0] = 0;
	ldir[0] = 0;
	scat(scat(ldir, "/net/tcp/"), buf);
	path[0] = 0;
	scat(scat(path, ldir), "/remote");
	n = open(path, OREAD);
	n = read(n, buf, sizeof(buf));
	check(n > 0 && contains(buf, n, "127.0.0.1!"), "remote address of the call");
	path[0] = 0;
	scat(scat(path, ldir), "/data");
	dfd = open(path, ORDWR);
	n = read(dfd, buf, sizeof(buf));
	check(n == 21 && contains(buf, n, "hello from the client"), "data from the caller");
	check(write(dfd, "thank", 5) == 5, "reply");
	n = p9sys(SYS(await), buf, sizeof(buf) - 1);
	buf[n > 0 ? n : 0] = 0;
	check(n > 0 && contains(buf, n, "''"), "client got the reply and exited");
	close(dfd);
	close(lfd);
	close(actl);
	n = open("/net/tcp", OREAD);
	check(n >= 0 && read(n, buf, sizeof(buf)) > 0, "/net/tcp can be listed");
	return done();
}
