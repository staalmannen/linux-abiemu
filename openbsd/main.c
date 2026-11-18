// SPDX-License-Identifier: GPL-2.0
/*
 * abi_openbsd - run OpenBSD/amd64 binaries on Linux
 *
 * OpenBSD executables carry an ".note.openbsd.ident" ELF note.  Their
 * crt0 takes everything from the stack, so apart from the auxiliary
 * vector nothing needs to be set up at exec time.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/binfmts.h>
#include <linux/elf.h>
#include <linux/errno.h>

#include "openbsd.h"

static const unsigned short openbsd_errno[] = {
	BSD_ERRNO_COMMON,
	[ENODATA] = 83 /* ENOATTR */, [EILSEQ] = 84, [ENOMEDIUM] = 85,
	[EMEDIUMTYPE] = 86, [EOVERFLOW] = 87, [ECANCELED] = 88, [EIDRM] = 89,
	[ENOMSG] = 90, [EBADMSG] = 92, [ENOTRECOVERABLE] = 93,
	[EOWNERDEAD] = 94, [EPROTO] = 95,
	[EMULTIHOP] = 95, [ENOLINK] = 95, [ETIME] = 60, [ENOSR] = 55,
	[ENOSTR] = 22,
};

static const struct abi_flagmap openbsd_oflags[] = {
	{ 0x00004, O_NONBLOCK }, { 0x00008, O_APPEND }, { 0x00040, FASYNC },
	{ 0x00080, O_SYNC }, { 0x00100, O_NOFOLLOW }, { 0x00200, O_CREAT },
	{ 0x00400, O_TRUNC }, { 0x00800, O_EXCL }, { 0x08000, O_NOCTTY },
	{ 0x10000, O_CLOEXEC }, { 0x20000, O_DIRECTORY }, { 0 }
};

static const struct abi_flagmap openbsd_atflags[] = {
	{ 0x01, AT_EACCESS }, { 0x02, AT_SYMLINK_NOFOLLOW },
	{ 0x04, AT_SYMLINK_FOLLOW }, { 0x08, AT_REMOVEDIR }, { 0 }
};

static const struct abi_bsd_ops openbsd_bsd_ops = {
	.oflags		= openbsd_oflags,
	.atflags	= openbsd_atflags,
	.wcontinued	= 8,
	.wcont_status	= 0xffff,
	.f_dupfd_cloexec = 10,
};

#define NT_OPENBSD_IDENT	1

static int openbsd_probe_elf(struct linux_binprm *bprm, const struct elfhdr *eh)
{
	if (eh->e_ident[EI_OSABI] != ELFOSABI_NONE)
		return 0;
	return abi_elf_has_note(bprm, eh, "OpenBSD", NT_OPENBSD_IDENT);
}

/* Only the classic SVR4 entries mean the same thing on OpenBSD. */
static void openbsd_auxv(struct abi_auxv *av, unsigned long uaddr, void *arg)
{
	switch (av->type) {
	case AT_PHDR: case AT_PHENT: case AT_PHNUM: case AT_PAGESZ:
	case AT_BASE: case AT_FLAGS: case AT_ENTRY:
		break;
	default:
		av->type = AT_IGNORE;
	}
}

static void openbsd_exec_setup(struct pt_regs *regs)
{
	abi_auxv_rewrite(regs->sp, openbsd_auxv, NULL);
	regs->dx = 0;		/* no cleanup function */
}

static struct abi_personality openbsd_personality = {
	.name		= "openbsd",
	.per		= PER_ABI_OPENBSD,
	.owner		= THIS_MODULE,
	.sysent		= openbsd_sysent,
	.errno_map	= openbsd_errno,
	.nerrno		= ARRAY_SIZE(openbsd_errno),
	.data		= &openbsd_bsd_ops,
	.decode		= abi_bsd_decode,
	.set_result	= abi_bsd_set_result,
	.fork_child	= abi_bsd_fork_child,
	.probe_elf	= openbsd_probe_elf,
	.exec_setup	= openbsd_exec_setup,
	.sig_deliver	= openbsd_sig_deliver,
	.sig_return	= openbsd_sig_return,
};

static int __init abi_openbsd_init(void)
{
	openbsd_personality.nsysent = openbsd_nsysent;
	return abi_register_personality(&openbsd_personality);
}

static void __exit abi_openbsd_exit(void)
{
	abi_unregister_personality(&openbsd_personality);
}

module_init(abi_openbsd_init);
module_exit(abi_openbsd_exit);

MODULE_DESCRIPTION("abiemu: OpenBSD/amd64 system call emulation");
MODULE_LICENSE("GPL");
