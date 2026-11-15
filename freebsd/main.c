// SPDX-License-Identifier: GPL-2.0
/*
 * abi_freebsd - run FreeBSD/amd64 binaries on Linux
 *
 * FreeBSD executables are ELF files branded with EI_OSABI ==
 * ELFOSABI_FREEBSD.  The Linux ELF loader maps them (and their
 * /libexec/ld-elf.so.1) just fine; what is left to us is the start-up
 * convention, the auxiliary vector and of course the system calls.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/binfmts.h>
#include <linux/elf.h>
#include <linux/cpumask.h>
#include <linux/mman.h>
#include <linux/errno.h>

#include "freebsd.h"

static const unsigned short freebsd_errno[] = {
	BSD_ERRNO_COMMON,
	[EIDRM] = 82, [ENOMSG] = 83, [EOVERFLOW] = 84, [ECANCELED] = 85,
	[EILSEQ] = 86, [ENODATA] = 87 /* ENOATTR */, [EBADMSG] = 89,
	[EMULTIHOP] = 90, [ENOLINK] = 91, [EPROTO] = 92,
	[ENOTRECOVERABLE] = 95, [EOWNERDEAD] = 96, [ETIME] = 60,
	[ENOSR] = 55, [ENOSTR] = 22, [ENOMEDIUM] = 6, [EMEDIUMTYPE] = 79,
};

static const struct abi_flagmap freebsd_oflags[] = {
	{ 0x00000004, O_NONBLOCK }, { 0x00000008, O_APPEND },
	{ 0x00000040, FASYNC }, { 0x00000080, O_SYNC },
	{ 0x00000100, O_NOFOLLOW }, { 0x00000200, O_CREAT },
	{ 0x00000400, O_TRUNC }, { 0x00000800, O_EXCL },
	{ 0x00008000, O_NOCTTY }, { 0x00010000, O_DIRECT },
	{ 0x00020000, O_DIRECTORY }, { 0x00100000, O_CLOEXEC },
	{ 0x00400000, O_PATH }, { 0x01000000, O_DSYNC },
	{ 0 }
};

static const struct abi_flagmap freebsd_atflags[] = {
	{ 0x0100, AT_EACCESS }, { 0x0200, AT_SYMLINK_NOFOLLOW },
	{ 0x0400, AT_SYMLINK_FOLLOW }, { 0x0800, AT_REMOVEDIR },
	{ 0x4000, AT_EMPTY_PATH }, { 0 }
};

static const struct abi_bsd_ops freebsd_bsd_ops = {
	.oflags		= freebsd_oflags,
	.atflags	= freebsd_atflags,
	.wcontinued	= 4,
	.wcont_status	= 0x13,
	.f_dupfd_cloexec = 17,
};

static int freebsd_probe_elf(struct linux_binprm *bprm,
			     const struct elfhdr *eh)
{
	return eh->e_ident[EI_OSABI] == ELFOSABI_FREEBSD;
}

/* FreeBSD's auxiliary vector types where they differ from Linux */
#define FREEBSD_AT_IGNORE	1
#define FREEBSD_AT_EXECPATH	15
#define FREEBSD_AT_CANARY	16
#define FREEBSD_AT_CANARYLEN	17
#define FREEBSD_AT_OSRELDATE	18
#define FREEBSD_AT_NCPUS	19
#define FREEBSD_AT_PAGESIZES	20
#define FREEBSD_AT_PAGESIZESLEN	21
#define FREEBSD_AT_STACKPROT	23
#define FREEBSD_AT_BSDFLAGS	27

struct freebsd_auxv_state {
	unsigned long pagesz_ptr;	/* user address of the AT_PAGESZ value */
	bool rewrite;
};

/*
 * The Linux loader has already built an auxiliary vector.  It cannot
 * grow, so reuse the slots of entries FreeBSD has no use for.
 */
static void freebsd_auxv(struct abi_auxv *av, unsigned long uaddr, void *arg)
{
	struct freebsd_auxv_state *st = arg;

	if (!st->rewrite) {
		if (av->type == AT_PAGESZ)
			st->pagesz_ptr = uaddr + sizeof(long);
		return;
	}

	switch (av->type) {
	case AT_PHDR: case AT_PHENT: case AT_PHNUM: case AT_PAGESZ:
	case AT_BASE: case AT_FLAGS: case AT_ENTRY:
	case AT_UID: case AT_EUID: case AT_GID: case AT_EGID:
		break;			/* same meaning */
	case AT_EXECFN:
		av->type = FREEBSD_AT_EXECPATH;
		break;
	case AT_RANDOM:			/* 16 random bytes */
		av->type = FREEBSD_AT_CANARY;
		break;
	case AT_HWCAP2:
		av->type = FREEBSD_AT_CANARYLEN;
		av->val = 16;
		break;
	case AT_CLKTCK:
		av->type = FREEBSD_AT_OSRELDATE;
		av->val = FREEBSD_OSRELDATE;
		break;
	case AT_HWCAP:
		av->type = FREEBSD_AT_NCPUS;
		av->val = num_online_cpus();
		break;
	case AT_SECURE:
		av->type = FREEBSD_AT_STACKPROT;
		av->val = PROT_READ | PROT_WRITE;
		break;
	case AT_PLATFORM:
		/* a one element array: the AT_PAGESZ value itself */
		if (st->pagesz_ptr) {
			av->type = FREEBSD_AT_PAGESIZES;
			av->val = st->pagesz_ptr;
			break;
		}
		fallthrough;
	default:
		av->type = FREEBSD_AT_IGNORE;
		break;
	case AT_SYSINFO_EHDR:
		if (st->pagesz_ptr) {
			av->type = FREEBSD_AT_PAGESIZESLEN;
			av->val = sizeof(long);
		} else {
			av->type = FREEBSD_AT_IGNORE;
		}
		break;
	}
}

static void freebsd_exec_setup(struct pt_regs *regs)
{
	struct freebsd_auxv_state st = { 0 };

	abi_auxv_rewrite(regs->sp, freebsd_auxv, &st);
	st.rewrite = true;
	abi_auxv_rewrite(regs->sp, freebsd_auxv, &st);

	/*
	 * _start(char **ap, void (*cleanup)(void)) is entered like a C
	 * function: %rdi points at argc and the stack looks as if a
	 * return address had been pushed.
	 */
	regs->di = regs->sp;
	regs->si = 0;
	regs->sp = ((regs->sp - 8) & ~0xfUL) + 8;
}

static struct abi_personality freebsd_personality = {
	.name		= "freebsd",
	.per		= PER_ABI_FREEBSD,
	.owner		= THIS_MODULE,
	.sysent		= freebsd_sysent,
	.errno_map	= freebsd_errno,
	.nerrno		= ARRAY_SIZE(freebsd_errno),
	.data		= &freebsd_bsd_ops,
	.decode		= abi_bsd_decode,
	.set_result	= abi_bsd_set_result,
	.fork_child	= abi_bsd_fork_child,
	.probe_elf	= freebsd_probe_elf,
	.exec_setup	= freebsd_exec_setup,
	.sig_deliver	= freebsd_sig_deliver,
	.sig_return	= freebsd_sig_return,
};

static int __init abi_freebsd_init(void)
{
	freebsd_personality.nsysent = freebsd_nsysent;
	return abi_register_personality(&freebsd_personality);
}

static void __exit abi_freebsd_exit(void)
{
	abi_unregister_personality(&freebsd_personality);
}

module_init(abi_freebsd_init);
module_exit(abi_freebsd_exit);

MODULE_DESCRIPTION("abiemu: FreeBSD/amd64 system call emulation");
MODULE_LICENSE("GPL");
