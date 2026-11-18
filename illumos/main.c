// SPDX-License-Identifier: GPL-2.0
/*
 * abi_illumos - run illumos (OpenSolaris, OmniOS, SmartOS...) amd64
 * binaries on Linux
 *
 * illumos binaries are mostly branded ELFOSABI_SYSV; they are recognised
 * by ELFOSABI_SOLARIS, by the illumos run-time linker as interpreter, or
 * by the Sun specific program headers the illumos link-editor emits.
 *
 * The system call convention is the BSD one (carry flag plus positive
 * errno), but the numbering of errno values and signals is SVR4.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/binfmts.h>
#include <linux/elf.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <asm/processor-flags.h>

#include "illumos.h"

static const unsigned short illumos_errno[] = {
	[ENOMSG] = 35, [EIDRM] = 36, [ECHRNG] = 37, [EL2NSYNC] = 38,
	[EL3HLT] = 39, [EL3RST] = 40, [ELNRNG] = 41, [EUNATCH] = 42,
	[ENOCSI] = 43, [EL2HLT] = 44, [EDEADLK] = 45, [ENOLCK] = 46,
	[ECANCELED] = 47, [EDQUOT] = 49, [EBADE] = 50, [EBADR] = 51,
	[EXFULL] = 52, [ENOANO] = 53, [EBADRQC] = 54, [EBADSLT] = 55,
	[EBFONT] = 57, [EOWNERDEAD] = 58, [ENOTRECOVERABLE] = 59,
	[ENOSTR] = 60, [ENODATA] = 61, [ETIME] = 62, [ENOSR] = 63,
	[ENONET] = 64, [ENOPKG] = 65, [EREMOTE] = 66, [ENOLINK] = 67,
	[EADV] = 68, [ESRMNT] = 69, [ECOMM] = 70, [EPROTO] = 71,
	[EMULTIHOP] = 74, [EBADMSG] = 77, [ENAMETOOLONG] = 78,
	[EOVERFLOW] = 79, [ENOTUNIQ] = 80, [EBADFD] = 81, [EREMCHG] = 82,
	[ELIBACC] = 83, [ELIBBAD] = 84, [ELIBSCN] = 85, [ELIBMAX] = 86,
	[ELIBEXEC] = 87, [EILSEQ] = 88, [ENOSYS] = 89, [ELOOP] = 90,
	[ERESTART] = 91, [ESTRPIPE] = 92, [ENOTEMPTY] = 93, [EUSERS] = 94,
	[ENOTSOCK] = 95, [EDESTADDRREQ] = 96, [EMSGSIZE] = 97,
	[EPROTOTYPE] = 98, [ENOPROTOOPT] = 99, [EPROTONOSUPPORT] = 120,
	[ESOCKTNOSUPPORT] = 121, [EOPNOTSUPP] = 122, [EPFNOSUPPORT] = 123,
	[EAFNOSUPPORT] = 124, [EADDRINUSE] = 125, [EADDRNOTAVAIL] = 126,
	[ENETDOWN] = 127, [ENETUNREACH] = 128, [ENETRESET] = 129,
	[ECONNABORTED] = 130, [ECONNRESET] = 131, [ENOBUFS] = 132,
	[EISCONN] = 133, [ENOTCONN] = 134, [ESHUTDOWN] = 143,
	[ETOOMANYREFS] = 144, [ETIMEDOUT] = 145, [ECONNREFUSED] = 146,
	[EHOSTDOWN] = 147, [EHOSTUNREACH] = 148, [EALREADY] = 149,
	[EINPROGRESS] = 150, [ESTALE] = 151, [ENOMEDIUM] = 6,
	[ENOTSUPP] = 48, [EUCLEAN] = 5, [ENOTNAM] = 22, [EREMOTEIO] = 5,
};

static int illumos_decode(struct abi_ctx *ctx)
{
	abi_decode_sysv(ctx);
	if (ctx->nr == 0) {		/* indirect: syscall(nr, ...) */
		unsigned long a6 = 0;

		if (get_user(a6, (unsigned long __user *)(ctx->regs->sp + 8)))
			a6 = 0;
		ctx->nr = ctx->args[0];
		memmove(&ctx->args[0], &ctx->args[1], 5 * sizeof(long));
		ctx->args[5] = a6;
	}
	return 0;
}

static void illumos_set_result(struct abi_ctx *ctx, long ret)
{
	struct pt_regs *regs = ctx->regs;

	if (abi_is_error(ret)) {
		int err = abi_errno(ctx->pers, ret);

		if (err == -ret && err > 34)	/* no SVR4 equivalent */
			err = EIO;
		regs->ax = err;
		regs->flags |= X86_EFLAGS_CF;
	} else {
		regs->ax = ret;
		regs->flags &= ~X86_EFLAGS_CF;
	}
	regs->r11 = regs->flags;
}

/* forksys(): the child sees rval1 = parent pid, rval2 = 1 */
static void illumos_fork_child(struct abi_ctx *parent, struct pt_regs *child)
{
	child->ax = task_tgid_vnr(current);
	child->dx = 1;
	child->flags &= ~X86_EFLAGS_CF;
}

#define PT_SUNWBSS	0x6ffffffa
#define PT_SUNWSTACK	0x6ffffffb

static int illumos_probe_elf(struct linux_binprm *bprm, const struct elfhdr *eh)
{
	char interp[64];

	if (eh->e_ident[EI_OSABI] == ELFOSABI_SOLARIS)
		return 1;
	if (eh->e_ident[EI_OSABI] != ELFOSABI_NONE)
		return 0;
	if (abi_elf_interp(bprm, eh, interp, sizeof(interp)) &&
	    (!strcmp(interp, "/usr/lib/amd64/ld.so.1") ||
	     !strcmp(interp, "/lib/amd64/ld.so.1")))
		return 1;
	return 0;
}

/*
 * The auxiliary vector: AT_SUN_* entries replace the Linux specific
 * ones.  AT_SUN_PLATFORM must point to "i86pc", which we place in a
 * page of its own.
 */
#define AT_SUN_UID	2000
#define AT_SUN_RUID	2001
#define AT_SUN_GID	2002
#define AT_SUN_RGID	2003
#define AT_SUN_PLATFORM	2008
#define AT_SUN_HWCAP	2009
#define AT_SUN_EXECNAME	2014

static void illumos_auxv(struct abi_auxv *av, unsigned long uaddr, void *arg)
{
	unsigned long platform = *(unsigned long *)arg;

	switch (av->type) {
	case AT_PHDR: case AT_PHENT: case AT_PHNUM: case AT_PAGESZ:
	case AT_BASE: case AT_FLAGS: case AT_ENTRY:
		break;
	case AT_UID:	av->type = AT_SUN_RUID; break;
	case AT_EUID:	av->type = AT_SUN_UID; break;
	case AT_GID:	av->type = AT_SUN_RGID; break;
	case AT_EGID:	av->type = AT_SUN_GID; break;
	case AT_EXECFN:	av->type = AT_SUN_EXECNAME; break;
	case AT_HWCAP:
		av->type = AT_SUN_HWCAP;
		av->val = 0;
		break;
	case AT_PLATFORM:
		if (platform) {
			av->type = AT_SUN_PLATFORM;
			av->val = platform;
			break;
		}
		fallthrough;
	default:
		av->type = AT_IGNORE;
	}
}

static void illumos_exec_setup(struct pt_regs *regs)
{
	unsigned long page;

	page = abi_map_anon(0, PAGE_SIZE, PROT_READ | PROT_WRITE, 0);
	if (IS_ERR_VALUE(page) ||
	    copy_to_user((void __user *)page, "i86pc", 6))
		page = 0;
	abi_auxv_rewrite(regs->sp, illumos_auxv, &page);
	regs->dx = 0;		/* no atexit() handler from a loader */
}

static struct abi_personality illumos_personality = {
	.name		= "illumos",
	.per		= PER_ABI_ILLUMOS,
	.owner		= THIS_MODULE,
	.sysent		= illumos_sysent,
	.errno_map	= illumos_errno,
	.nerrno		= ARRAY_SIZE(illumos_errno),
	.decode		= illumos_decode,
	.set_result	= illumos_set_result,
	.fork_child	= illumos_fork_child,
	.probe_elf	= illumos_probe_elf,
	.exec_setup	= illumos_exec_setup,
	.sig_deliver	= illumos_sig_deliver,
	.sig_return	= illumos_sig_return,
};

static int __init abi_illumos_init(void)
{
	illumos_personality.nsysent = illumos_nsysent;
	return abi_register_personality(&illumos_personality);
}

static void __exit abi_illumos_exit(void)
{
	abi_unregister_personality(&illumos_personality);
}

module_init(abi_illumos_init);
module_exit(abi_illumos_exit);

MODULE_DESCRIPTION("abiemu: illumos/amd64 system call emulation");
MODULE_LICENSE("GPL");
