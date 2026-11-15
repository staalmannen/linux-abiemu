// SPDX-License-Identifier: GPL-2.0
/*
 * abiemu core - signal delivery to foreign handlers
 *
 * The kernel only knows how to build its own signal frame, and it
 * passes Linux signal numbers and structures.  So a foreign handler is
 * never registered with the kernel directly.  Instead:
 *
 *  - Every foreign process that installs a handler gets two pages at
 *    ABI_TRAMP_ADDR: a code page with two "syscall" instructions and a
 *    data page holding the real handler of each signal.  Living in the
 *    address space, the table is inherited by fork(), shared between
 *    threads and discarded by exec(), just like the kernel's own.
 *
 *  - The kernel is told that the handler is ABI_TRAMP_ENTRY.  When a
 *    signal arrives, Linux pushes its frame and "calls" the stub, which
 *    immediately traps back to us.  The personality's sig_deliver()
 *    hook builds the frame the foreign handler expects below the Linux
 *    one and redirects the task into the real handler.
 *
 *  - The handler returns to ABI_TRAMP_RETURN (or calls the foreign
 *    sigreturn), which traps again; sig_return() copies the possibly
 *    modified context back and we finish with a native rt_sigreturn.
 */
#define pr_fmt(fmt) "abiemu: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/fs.h>
#include <linux/shmem_fs.h>
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#include <asm/unistd.h>

#include <abiemu/abiemu.h>
#include "core.h"

static const u8 abi_tramp_code[] = {
	0x0f, 0x05,		/* +0: syscall	(signal entry) */
	0x0f, 0x0b,		/*     ud2 */
	0x0f, 0x05,		/* +4: syscall	(signal return) */
	0x0f, 0x0b,		/*     ud2 */
};

/* contents of the data page */
struct abi_tramp_data {
	u64	handler[_NSIG + 1];
	u64	flags[_NSIG + 1];	/* SA_* as requested by the program */
};

/* what the kernel's rt_sigaction() takes on x86_64 */
struct abi_ksigaction {
	u64	handler;
	u64	flags;
	u64	restorer;
	u64	mask;
};

static struct file *abi_tramp_file;

static int abi_tramp_map(void)
{
	unsigned long addr;

	addr = vm_mmap(abi_tramp_file, ABI_TRAMP_ADDR, PAGE_SIZE,
		       PROT_READ | PROT_EXEC,
		       MAP_PRIVATE | MAP_FIXED_NOREPLACE, 0);
	if (addr == -EEXIST)
		return 0;		/* already there */
	if (IS_ERR_VALUE(addr))
		return addr;
	addr = vm_mmap(NULL, ABI_TRAMP_DATA, PAGE_SIZE, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, 0);
	if (IS_ERR_VALUE(addr) && addr != -EEXIST)
		return addr;
	return 0;
}

#define TRAMP_SLOT(field, sig) \
	((u64 __user *)(ABI_TRAMP_DATA + \
			offsetof(struct abi_tramp_data, field[sig])))

/*
 * Change a signal action on behalf of a foreign program.  Must be the
 * last thing a system call handler does: the kernel part is done by a
 * native rt_sigaction().  If @want_old, the post routine can fetch the
 * previous action with abi_sigaction_old().
 */
long abi_sigaction(struct abi_ctx *ctx, int lsig,
		   const struct abi_sigact *act, bool want_old)
{
	struct abi_ksigaction __user *gact = NULL, *goact = NULL;
	u64 ohandler = 0, oflags = 0;
	int err;

	if (lsig < 1 || lsig > _NSIG)
		return -EINVAL;
	if (act && (lsig == SIGKILL || lsig == SIGSTOP))
		return -EINVAL;

	/* The old values may be garbage if the pages are not there yet. */
	if (get_user(ohandler, TRAMP_SLOT(handler, lsig)) ||
	    get_user(oflags, TRAMP_SLOT(flags, lsig)))
		ohandler = oflags = 0;
	ctx->priv[4] = ohandler;
	ctx->priv[5] = oflags;

	if (act) {
		struct abi_ksigaction ka = {
			.handler = act->handler,
			.flags = act->flags | SA_RESTORER,
			.restorer = ABI_TRAMP_RETURN,
			.mask = act->mask,
		};

		if (act->handler > (unsigned long)SIG_IGN) {
			err = abi_tramp_map();
			if (err)
				return err;
			if (put_user(act->handler, TRAMP_SLOT(handler, lsig)) ||
			    put_user(act->flags, TRAMP_SLOT(flags, lsig)))
				return -EFAULT;
			ka.handler = ABI_TRAMP_ENTRY;
			ka.flags |= SA_SIGINFO;
		}
		gact = abi_gap(ctx, sizeof(ka));
		if (copy_to_user(gact, &ka, sizeof(ka)))
			return -EFAULT;
	}
	if (want_old)
		goact = abi_gap(ctx, sizeof(*goact));
	ctx->priv[3] = (unsigned long)goact;
	return abi_native4(ctx, __NR_rt_sigaction, lsig, (unsigned long)gact,
			   (unsigned long)goact, sizeof(u64));
}
EXPORT_SYMBOL_GPL(abi_sigaction);

int abi_sigaction_old(struct abi_ctx *ctx, struct abi_sigact *old)
{
	struct abi_ksigaction ka;

	if (!ctx->priv[3] ||
	    copy_from_user(&ka, (void __user *)ctx->priv[3], sizeof(ka)))
		return -EFAULT;
	old->handler = ka.handler;
	old->flags = ka.flags & ~SA_RESTORER;
	old->mask = ka.mask;
	if (ka.handler == ABI_TRAMP_ENTRY) {
		old->handler = ctx->priv[4];
		old->flags = ctx->priv[5];
	}
	return 0;
}
EXPORT_SYMBOL_GPL(abi_sigaction_old);

long abi_sigreturn(struct abi_ctx *ctx, unsigned long lframe)
{
	/* rt_sigreturn expects the stack as it is after the handler's ret */
	ctx->regs->sp = lframe + sizeof(u64);
	ctx->flags |= ABI_F_NORESULT;
	return abi_native0(ctx, __NR_rt_sigreturn);
}
EXPORT_SYMBOL_GPL(abi_sigreturn);

/* One of the two system calls in the trampoline page was executed. */
long abi_sig_trap(struct abi_ctx *ctx)
{
	const struct abi_personality *p = ctx->pers;
	struct pt_regs *regs = ctx->regs;
	long ret = -ENOSYS;

	if (ctx->nr == ABI_TRAMP_ENTRY - ABI_TRAMP_ADDR && p->sig_deliver) {
		/* entered like a Linux handler: (sig, info, uc), sp = frame */
		int lsig = regs->di;
		u64 handler;

		ret = -EFAULT;
		if (lsig >= 1 && lsig <= _NSIG &&
		    !get_user(handler, TRAMP_SLOT(handler, lsig)))
			ret = p->sig_deliver(ctx, lsig, handler, regs->sp);
		ctx->flags |= ABI_F_NORESULT;
	} else if (ctx->nr == ABI_TRAMP_RETURN - ABI_TRAMP_ADDR &&
		   p->sig_return) {
		ret = p->sig_return(ctx);
	}

	if (ret < 0) {
		pr_info_ratelimited("%s[%d:%s]: bad signal frame (%ld)\n",
				    p->name, task_pid_nr(current),
				    current->comm, ret);
		force_sig(SIGSEGV);
		ctx->flags |= ABI_F_NORESULT;
	}
	return ret;
}

int __init abi_tramp_init(void)
{
	loff_t pos = 0;
	ssize_t n;

	abi_tramp_file = shmem_kernel_file_setup("abiemu-tramp", PAGE_SIZE, 0);
	if (IS_ERR(abi_tramp_file))
		return PTR_ERR(abi_tramp_file);
	n = kernel_write(abi_tramp_file, abi_tramp_code,
			 sizeof(abi_tramp_code), &pos);
	if (n != sizeof(abi_tramp_code)) {
		fput(abi_tramp_file);
		return n < 0 ? n : -EIO;
	}
	return 0;
}

void abi_tramp_exit(void)
{
	fput(abi_tramp_file);
}
