/* SPDX-License-Identifier: GPL-2.0 */
/*
 * rawsys - a libc free system call test for the abiemu personalities
 *
 * Built once per target OS (-DOS_FREEBSD, -DOS_NETBSD, -DOS_OPENBSD,
 * -DOS_ILLUMOS) with that system's call numbers, structure layouts and
 * ELF branding.  It runs on Linux only through abiemu.  Every check
 * prints "ok"/"not ok" lines in TAP style.
 */
typedef unsigned long u64;
typedef long s64;
typedef unsigned int u32;

#if defined(OS_FREEBSD)
# include "../../freebsd/syscall_nr.h"
# define P(x)		FREEBSD_SYS_##x
# define NR_exit	P(_exit)
# define NR_open	P(open)
# define NR_fork	P(fork)
# define NR_fstat	P(fstat)
# define NR_getdents	P(getdirentries)
# define NR_wait4	P(wait4)
# define NR_sigaction	P(sigaction)
# define NR_pipe	P(pipe2)
# define NR_nanosleep	P(nanosleep)
# define NR_getpid	P(getpid)
# define NR_uname	0
# define O_CREAT_	0x200
# define O_TRUNC_	0x400
# define MAP_ANON_	0x1000
# define SIGUSR1_	30
# define STAT_SIZE_OFF	112
# define OSNAME		"FreeBSD"
#elif defined(OS_NETBSD)
# include "../../netbsd/syscall_nr.h"
# define P(x)		NETBSD_SYS_##x
# define NR_exit	P(exit)
# define NR_open	P(open)
# define NR_fork	P(fork)
# define NR_fstat	P(__fstat50)
# define NR_getdents	P(__getdents30)
# define NR_wait4	P(__wait450)
# define NR_sigaction	P(__sigaction_sigtramp)
# define NR_pipe	P(pipe2)
# define NR_nanosleep	P(__nanosleep50)
# define NR_getpid	P(getpid_with_ppid)
# define NR_uname	0
# define O_CREAT_	0x200
# define O_TRUNC_	0x400
# define MAP_ANON_	0x1000
# define SIGUSR1_	30
# define STAT_SIZE_OFF	112
# define OSNAME		"NetBSD"
#elif defined(OS_OPENBSD)
# include "../../openbsd/syscall_nr.h"
# define P(x)		OPENBSD_SYS_##x
# define NR_exit	P(exit)
# define NR_open	P(open)
# define NR_fork	P(fork)
# define NR_fstat	P(fstat)
# define NR_getdents	P(getdents)
# define NR_wait4	P(wait4)
# define NR_sigaction	P(sigaction)
# define NR_pipe	P(pipe2)
# define NR_nanosleep	P(nanosleep)
# define NR_getpid	P(getpid)
# define NR_uname	0
# define O_CREAT_	0x200
# define O_TRUNC_	0x400
# define MAP_ANON_	0x1000
# define SIGUSR1_	30
# define STAT_SIZE_OFF	80
# define OSNAME		"OpenBSD"
#elif defined(OS_ILLUMOS)
# include "../../illumos/syscall_nr.h"
# define P(x)		ILLUMOS_SYS_##x
# define NR_exit	P(exit)
# define NR_open	P(open)
# define NR_fstat	P(fstat)
# define NR_getdents	P(getdents)
# define NR_wait4	0
# define NR_sigaction	P(sigaction)
# define NR_pipe	P(pipe)
# define NR_nanosleep	P(nanosleep)
# define NR_getpid	P(getpid)
# define NR_uname	P(uname)
# define O_CREAT_	0x100
# define O_TRUNC_	0x200
# define MAP_ANON_	0x100
# define SIGUSR1_	16
# define STAT_SIZE_OFF	40
# define OSNAME		"SunOS"
#else
# error "define OS_<name>"
#endif

#define NR_write	P(write)
#define NR_read		P(read)
#define NR_close	P(close)
#define NR_kill		P(kill)
#define NR_mkdir	P(mkdir)
#define NR_unlink	P(unlink)
#define NR_rmdir	P(rmdir)
#define NR_mmap		P(mmap)
#define NR_munmap	P(munmap)

static u64 rdx_out;
static int err_out;

/* All four use %rax/args in rdi rsi rdx r10 r8 r9 and set CF on error. */
static s64 sys6(long nr, u64 a, u64 b, u64 c, u64 d, u64 e, u64 f)
{
	register u64 r10 __asm__("r10") = d;
	register u64 r8 __asm__("r8") = e;
	register u64 r9 __asm__("r9") = f;
	u64 ret, rdx = c;
	unsigned char cf;

	__asm__ volatile("syscall; setc %2"
			 : "=a"(ret), "+d"(rdx), "=r"(cf), "+r"(r10),
			   "+r"(r8), "+r"(r9), "+D"(a), "+S"(b)
			 : "a"(nr)
			 : "rcx", "r11", "memory");
	rdx_out = rdx;
	if (cf) {
		err_out = ret;
		return -1;
	}
	err_out = 0;
	return ret;
}

#define sys0(n)			sys6(n, 0, 0, 0, 0, 0, 0)
#define sys1(n, a)		sys6(n, (u64)(a), 0, 0, 0, 0, 0)
#define sys2(n, a, b)		sys6(n, (u64)(a), (u64)(b), 0, 0, 0, 0)
#define sys3(n, a, b, c)	sys6(n, (u64)(a), (u64)(b), (u64)(c), 0, 0, 0)
#define sys4(n, a, b, c, d)	sys6(n, (u64)(a), (u64)(b), (u64)(c), (u64)(d), 0, 0)

static u64 slen(const char *s) { u64 n = 0; while (s[n]) n++; return n; }

static void puts_(const char *s) { sys3(NR_write, 1, s, slen(s)); }

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
	if (!ok)
		nfail++;
	puts_(ok ? "ok " : "not ok ");
	putnum(ntest);
	puts_(" - ");
	puts_(what);
	puts_("\n");
}

static int memfind(const char *h, long n, const char *needle)
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

static volatile int got_signal;

static void handler(int sig)
{
	got_signal = sig;
}

static int install_handler(int sig)
{
#if defined(OS_FREEBSD)
	struct { u64 h; int flags; u32 mask[4]; } sa = { (u64)handler, 2, {0} };
	return sys3(NR_sigaction, sig, &sa, 0);
#elif defined(OS_NETBSD)
	struct { u64 h; u32 mask[4]; int flags; } sa = { (u64)handler, {0}, 2 };
	return sys6(NR_sigaction, sig, (u64)&sa, 0, 0, 0, 0);
#elif defined(OS_OPENBSD)
	struct { u64 h; u32 mask; int flags; } sa = { (u64)handler, 0, 2 };
	return sys3(NR_sigaction, sig, &sa, 0);
#elif defined(OS_ILLUMOS)
	struct { int flags; u64 h; u32 mask[4]; int resv[2]; } sa =
		{ 4 /* SA_RESTART */, (u64)handler, {0}, {0} };
	return sys3(NR_sigaction, sig, &sa, 0);
#endif
}

static long wait_child(long pid, int *status)
{
#if defined(OS_ILLUMOS)
	/* waitsys(P_PID, pid, siginfo_t *, WEXITED) */
	int si[64] = { 0 };
	long r = sys4(P(waitid), 0, pid, si, 1);

	/* si_code CLD_EXITED (1), si_status (offset 32) = exit code */
	*status = (si[1] == 1) ? (si[8] << 8) : -1;
	return r < 0 ? r : pid;
#else
	return sys4(NR_wait4, pid, status, 0, 0);
#endif
}

int main(void)
{
	static char buf[4096];
	const char *path = "/tmp/rawsys.file";
	long fd, n, pid;
	int fds[2], status = 0;

	puts_("# rawsys for " OSNAME "\n");

	check(sys0(NR_getpid) > 1, "getpid");

	fd = sys3(NR_open, "/nonexistent/x", 0, 0);
	check(fd == -1 && err_out == 2, "open of a missing file fails with ENOENT");

	fd = sys3(NR_open, path, 1 | O_CREAT_ | O_TRUNC_, 0644);
	check(fd >= 0, "open(O_CREAT)");
	check(sys3(NR_write, fd, "hello world\n", 12) == 12, "write");
	check(sys1(NR_close, fd) == 0, "close");

	fd = sys3(NR_open, path, 0, 0);
	n = sys3(NR_read, fd, buf, sizeof(buf));
	check(n == 12 && buf[0] == 'h' && buf[10] == 'd', "read back");
	check(sys2(NR_fstat, fd, buf) == 0 &&
	      *(s64 *)(buf + STAT_SIZE_OFF) == 12, "fstat st_size");
	sys1(NR_close, fd);

	check(sys2(NR_mkdir, "/tmp/rawsys.dir", 0755) == 0, "mkdir");
	fd = sys3(NR_open, "/tmp", 0, 0);
	n = sys4(NR_getdents, fd, buf, sizeof(buf), 0);
	check(n > 0 && memfind(buf, n, "rawsys.dir") && memfind(buf, n, "rawsys.file"),
	      "getdents lists the new entries");
	sys1(NR_close, fd);
	check(sys1(NR_rmdir, "/tmp/rawsys.dir") == 0, "rmdir");
	check(sys1(NR_unlink, path) == 0, "unlink");

	{
		u64 p = sys6(NR_mmap, 0, 8192, 3, 2 | MAP_ANON_, -1, 0);
		check((s64)p > 0 && (*(volatile int *)p = 42) == 42 &&
		      sys2(NR_munmap, p, 8192) == 0, "anonymous mmap");
	}

#if defined(OS_ILLUMOS)
	fds[0] = sys0(NR_pipe);
	fds[1] = rdx_out;
	check(fds[0] >= 0, "pipe");
#else
	check(sys2(NR_pipe, fds, 0) == 0, "pipe2");
#endif
#if defined(OS_ILLUMOS)
	pid = sys2(P(forksys), 0, 0);
#else
	pid = sys0(NR_fork);
#endif
	if (pid >= 0 && rdx_out == 1) {			/* child */
		sys3(NR_write, fds[1], "child", 5);
		sys1(NR_exit, 7);
	}
	check(pid > 0, "fork");
	n = sys3(NR_read, fds[0], buf, 5);
	check(n == 5 && buf[0] == 'c', "data from the child through the pipe");
	check(wait_child(pid, &status) == pid && status == (7 << 8),
	      "wait for the child, exit status 7");

	check(install_handler(SIGUSR1_) == 0, "sigaction");
	sys2(NR_kill, sys0(NR_getpid), SIGUSR1_);
	check(got_signal == SIGUSR1_, "signal handler ran with the native number");

	{
		struct { s64 sec, nsec; } ts = { 0, 1000000 };
		check(sys2(NR_nanosleep, &ts, 0) == 0, "nanosleep");
	}

#if defined(OS_ILLUMOS)
	n = sys1(NR_uname, buf);	/* struct utsname: 5 x 257 bytes */
	check(n >= 0 && memfind(buf, 257, "SunOS"), "uname says SunOS");
#endif

	puts_(nfail ? "# FAILED " : "# all passed ");
	putnum(ntest - nfail);
	puts_("/");
	putnum(ntest);
	puts_("\n");
	return nfail != 0;
}

#define STR_(x)	#x
#define STR(x)	STR_(x)

__asm__(".text\n.globl _start\n_start:\n"
	"	andq $-16, %rsp\n"
	"	call main\n"
	"	movl %eax, %edi\n"
	"	movq $" STR(NR_exit) ", %rax\n"
	"	syscall\n"
	"	hlt\n");
