// SPDX-License-Identifier: GPL-2.0
/*
 * abi_netbsd - run NetBSD/amd64 binaries on Linux
 *
 * NetBSD executables carry EI_OSABI == ELFOSABI_SYSV and are recognised
 * by their ".note.netbsd.ident" ELF note.  At start-up %rbx has to
 * point to a struct ps_strings, which crt0 uses to find argv and envp.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/binfmts.h>
#include <linux/elf.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/errno.h>

#include "netbsd.h"

static const unsigned short netbsd_errno[] = {
	BSD_ERRNO_COMMON,
	[EIDRM] = 82, [ENOMSG] = 83, [EOVERFLOW] = 84, [EILSEQ] = 85,
	[ECANCELED] = 87, [EBADMSG] = 88, [ENODATA] = 89, [ENOSR] = 90,
	[ENOSTR] = 91, [ETIME] = 92, [EMULTIHOP] = 94, [ENOLINK] = 95,
	[EPROTO] = 96, [EOWNERDEAD] = 97, [ENOTRECOVERABLE] = 98,
	[ENOMEDIUM] = 6, [EMEDIUMTYPE] = 79,
};

static const struct abi_flagmap netbsd_oflags[] = {
	{ 0x00000004, O_NONBLOCK }, { 0x00000008, O_APPEND },
	{ 0x00000040, FASYNC }, { 0x00000080, O_SYNC },
	{ 0x00000100, O_NOFOLLOW }, { 0x00000200, O_CREAT },
	{ 0x00000400, O_TRUNC }, { 0x00000800, O_EXCL },
	{ 0x00008000, O_NOCTTY }, { 0x00010000, O_DSYNC },
	{ 0x00080000, O_DIRECT }, { 0x00200000, O_DIRECTORY },
	{ 0x00400000, O_CLOEXEC }, { 0x00800000, O_PATH /* O_SEARCH */ },
	{ 0 }
};

static const struct abi_flagmap netbsd_atflags[] = {
	{ 0x0100, AT_EACCESS }, { 0x0200, AT_SYMLINK_NOFOLLOW },
	{ 0x0400, AT_SYMLINK_FOLLOW }, { 0x0800, AT_REMOVEDIR }, { 0 }
};

static const struct abi_bsd_ops netbsd_bsd_ops = {
	.oflags		= netbsd_oflags,
	.atflags	= netbsd_atflags,
	.wcontinued	= 0x10,
	.wcont_status	= 0xffff,
	.f_dupfd_cloexec = 12,
};

#define NT_NETBSD_IDENT	1

static int netbsd_probe_elf(struct linux_binprm *bprm, const struct elfhdr *eh)
{
	if (eh->e_ident[EI_OSABI] != ELFOSABI_NONE)
		return 0;
	return abi_elf_has_note(bprm, eh, "NetBSD", NT_NETBSD_IDENT);
}

/* NetBSD auxiliary vector types that differ from Linux */
#define NETBSD_AT_IGNORE	1
#define NETBSD_AT_STACKBASE	13
#define NETBSD_AT_EUID		2000
#define NETBSD_AT_RUID		2001
#define NETBSD_AT_EGID		2002
#define NETBSD_AT_RGID		2003
#define NETBSD_AT_SUN_EXECNAME	2014

static void netbsd_auxv(struct abi_auxv *av, unsigned long uaddr, void *arg)
{
	unsigned long stackbase = *(unsigned long *)arg;

	switch (av->type) {
	case AT_PHDR: case AT_PHENT: case AT_PHNUM: case AT_PAGESZ:
	case AT_BASE: case AT_FLAGS: case AT_ENTRY:
		break;
	case AT_UID:	av->type = NETBSD_AT_RUID; break;
	case AT_EUID:	av->type = NETBSD_AT_EUID; break;
	case AT_GID:	av->type = NETBSD_AT_RGID; break;
	case AT_EGID:	av->type = NETBSD_AT_EGID; break;
	case AT_EXECFN:	av->type = NETBSD_AT_SUN_EXECNAME; break;
	case AT_PLATFORM:
		av->type = NETBSD_AT_STACKBASE;
		av->val = stackbase;
		break;
	default:
		av->type = NETBSD_AT_IGNORE;
		break;
	}
}

struct netbsd_ps_strings {
	u64	ps_argvstr;
	s32	ps_nargvstr;
	u64	ps_envstr;
	s32	ps_nenvstr;
};

static void netbsd_exec_setup(struct pt_regs *regs)
{
	struct netbsd_ps_strings ps = { 0 };
	struct vm_area_struct *vma;
	unsigned long sp = regs->sp, page, stackbase = 0, v;
	unsigned long __user *p;
	long argc;

	mmap_read_lock(current->mm);
	vma = find_vma(current->mm, sp);
	if (vma)
		stackbase = vma->vm_end;
	mmap_read_unlock(current->mm);
	abi_auxv_rewrite(sp, netbsd_auxv, &stackbase);

	if (get_user(argc, (long __user *)sp))
		return;
	ps.ps_argvstr = sp + 8;
	ps.ps_nargvstr = argc;
	ps.ps_envstr = sp + 8 * (argc + 2);
	for (p = (unsigned long __user *)ps.ps_envstr;
	     !get_user(v, p) && v; p++)
		ps.ps_nenvstr++;

	page = abi_map_anon(0, PAGE_SIZE, PROT_READ | PROT_WRITE, 0);
	if (IS_ERR_VALUE(page) ||
	    copy_to_user((void __user *)page, &ps, sizeof(ps)))
		return;
	regs->bx = page;
	regs->dx = 0;		/* no cleanup function */
}

static struct abi_personality netbsd_personality = {
	.name		= "netbsd",
	.per		= PER_ABI_NETBSD,
	.owner		= THIS_MODULE,
	.sysent		= netbsd_sysent,
	.errno_map	= netbsd_errno,
	.nerrno		= ARRAY_SIZE(netbsd_errno),
	.data		= &netbsd_bsd_ops,
	.decode		= abi_bsd_decode,
	.set_result	= abi_bsd_set_result,
	.fork_child	= abi_bsd_fork_child,
	.probe_elf	= netbsd_probe_elf,
	.exec_setup	= netbsd_exec_setup,
	.sig_deliver	= netbsd_sig_deliver,
	.sig_return	= netbsd_sig_return,
};

static int __init abi_netbsd_init(void)
{
	netbsd_personality.nsysent = netbsd_nsysent;
	return abi_register_personality(&netbsd_personality);
}

static void __exit abi_netbsd_exit(void)
{
	abi_unregister_personality(&netbsd_personality);
}

module_init(abi_netbsd_init);
module_exit(abi_netbsd_exit);

MODULE_DESCRIPTION("abiemu: NetBSD/amd64 system call emulation");
MODULE_LICENSE("GPL");
