// SPDX-License-Identifier: GPL-2.0
/*
 * abi_plan9 - run Plan 9 (9front) amd64 binaries on Linux
 *
 * In the spirit of Glendix (https://github.com/anantn/glendix), Plan 9
 * a.out executables are loaded directly and their system calls are
 * translated to Linux.  Plan 9 passes the call number in RARG (%rbp)
 * and the arguments in 8 byte slots on the stack above the return
 * address; errors are reported as -1 plus a per process error string.
 *
 * Name spaces map onto Linux mount name spaces, mount(2) onto the 9P
 * file system client (trans=fd) and notes onto signals.  /net and
 * /dev/draw are provided by the abi_p9net and abi_p9draw modules.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/binfmts.h>
#include <linux/elf.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/mm.h>
#include <asm/processor.h>

#include "plan9.h"

struct p9_task *p9_task(void)
{
	return abi_task_priv(sizeof(struct p9_task));
}

/* Report an error by string; the system call then returns -1. */
long p9_error(struct abi_ctx *ctx, const char *msg)
{
	struct p9_task *t = p9_task();

	if (t)
		strscpy(t->errstr, msg, sizeof(t->errstr));
	ctx->flags |= P9_F_ERRSET;
	return -EINVAL;
}

const char *p9_errno_str(long err, char *buf)
{
	switch (-err) {
	case ENOENT:		return "file does not exist";
	case EEXIST:		return "file already exists";
	case ENOTDIR:		return "not a directory";
	case EISDIR:		return "file is a directory";
	case EPERM:
	case EACCES:		return "permission denied";
	case EINVAL:		return "bad arg in system call";
	case EBUSY:		return "device or object already in use";
	case EIO:		return "i/o error";
	case EPIPE:		return "i/o on hungup channel";
	case ECHILD:		return "no living children";
	case ENOMEM:		return "virtual memory allocation failed";
	case EBADF:		return "fd out of range or not open";
	case EMFILE:
	case ENFILE:		return "no free file descriptors";
	case ESPIPE:		return "seek on a stream";
	case ENOEXEC:		return "exec header invalid";
	case ETIMEDOUT:		return "connection timed out";
	case ECONNREFUSED:	return "connection refused";
	case EADDRINUSE:	return "network port not available";
	case EINTR:		return "interrupted";
	case ENOTEMPTY:		return "directory not empty";
	case EROFS:		return "file system is read only";
	case ENOSPC:		return "file system full";
	case ENAMETOOLONG:	return "file name too long";
	case EFAULT:		return "bad address in syscall";
	case E2BIG:		return "arg list too long";
	case ENOSYS:		return "not implemented";
	case EAGAIN:		return "resource temporarily unavailable";
	case EXDEV:		return "cross-device link";
	case ELOOP:		return "too many levels of symbolic links";
	case ENODEV:		return "no free devices";
	case ENOTTY:		return "inappropriate use of fd";
	case EHOSTUNREACH:
	case ENETUNREACH:	return "no route";
	}
	snprintf(buf, P9_ERRMAX, "linux error %ld", -err);
	return buf;
}

/*
 * Exit messages.  Linux only passes a number to the parent, so the
 * string given to exits() is kept here until the parent's await()
 * collects it.
 */
struct p9_exitmsg {
	struct list_head	list;
	pid_t			pid;
	char			msg[P9_ERRMAX];
};

static LIST_HEAD(p9_exitmsgs);
static DEFINE_SPINLOCK(p9_exitmsg_lock);
static unsigned int p9_nexitmsg;

void p9_exitmsg_put(pid_t pid, const char *msg)
{
	struct p9_exitmsg *e = kmalloc(sizeof(*e), GFP_KERNEL), *old = NULL;

	if (!e)
		return;
	e->pid = pid;
	strscpy(e->msg, msg, sizeof(e->msg));
	spin_lock(&p9_exitmsg_lock);
	list_add_tail(&e->list, &p9_exitmsgs);
	if (++p9_nexitmsg > 4096) {	/* nobody waited for the oldest */
		old = list_first_entry(&p9_exitmsgs, struct p9_exitmsg, list);
		list_del(&old->list);
		p9_nexitmsg--;
	}
	spin_unlock(&p9_exitmsg_lock);
	kfree(old);
}

bool p9_exitmsg_take(pid_t pid, char *buf)
{
	struct p9_exitmsg *e, *found = NULL;

	spin_lock(&p9_exitmsg_lock);
	list_for_each_entry(e, &p9_exitmsgs, list) {
		if (e->pid == pid) {
			list_del(&e->list);
			p9_nexitmsg--;
			found = e;
			break;
		}
	}
	spin_unlock(&p9_exitmsg_lock);
	if (!found)
		return false;
	strscpy(buf, found->msg, P9_ERRMAX);
	kfree(found);
	return true;
}

static void p9_exitmsg_flush(void)
{
	struct p9_exitmsg *e, *n;

	list_for_each_entry_safe(e, n, &p9_exitmsgs, list)
		kfree(e);
}

/*
 * Calling convention
 */
static int p9_decode(struct abi_ctx *ctx)
{
	unsigned long __user *ap = (unsigned long __user *)(ctx->regs->sp + 8);
	int i;

	ctx->nr = ctx->regs->bp;
	for (i = 0; i < 6; i++)
		if (get_user(ctx->args[i], ap + i))
			ctx->args[i] = 0;
	return 0;
}

static void p9_set_result(struct abi_ctx *ctx, long ret)
{
	char buf[P9_ERRMAX];

	if (abi_is_error(ret)) {
		if (!(ctx->flags & P9_F_ERRSET)) {
			struct p9_task *t = p9_task();

			if (t)
				strscpy(t->errstr, p9_errno_str(ret, buf),
					sizeof(t->errstr));
		}
		ret = -1;
	}
	ctx->regs->ax = ret;
}

/* rfork() child: returns 0 */
static void p9_fork_child(struct abi_ctx *parent, struct pt_regs *child)
{
	child->ax = 0;
}

/* The child's Tos still holds the parent's pid. */
static void p9_fork_return(struct pt_regs *regs)
{
	u32 pid = task_tgid_vnr(current);

	put_user(pid, (u32 __user *)(P9_TOS + P9_TOS_PID));
}

/*
 * The a.out loader
 */
static u32 be32(const u8 *p)
{
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static int p9_copy_from_file(struct file *file, loff_t pos,
			     unsigned long uaddr, size_t len)
{
	char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	int err = 0;

	if (!buf)
		return -ENOMEM;
	while (len && !err) {
		size_t n = min_t(size_t, len, PAGE_SIZE);
		ssize_t r = kernel_read(file, buf, n, &pos);

		if (r != n)
			err = r < 0 ? r : -EIO;
		else if (copy_to_user((void __user *)uaddr, buf, n))
			err = -EFAULT;
		uaddr += n;
		len -= n;
	}
	kfree(buf);
	return err;
}

/* sp -> argc, argv[0..argc-1], nil; no environment (that is /env) */
static int p9_create_tables(struct linux_binprm *bprm, unsigned long *spp)
{
	struct mm_struct *mm = current->mm;
	unsigned long p = mm->arg_start, sp;
	unsigned long __user *ap;
	int i, argc = bprm->argc;

	sp = (bprm->p & ~7UL) - (argc + 2) * sizeof(long);
	ap = (unsigned long __user *)sp;
	if (put_user((unsigned long)argc, ap++))
		return -EFAULT;
	for (i = 0; i < argc; i++) {
		long len = strnlen_user((const char __user *)p, MAX_ARG_STRLEN);

		if (!len || len > MAX_ARG_STRLEN)
			return -EINVAL;
		if (put_user(p, ap++))
			return -EFAULT;
		p += len;
	}
	if (put_user(0UL, ap))
		return -EFAULT;
	mm->arg_end = mm->env_start = mm->env_end = p;
	*spp = sp;
	return 0;
}

static int p9_load_binary(struct linux_binprm *bprm)
{
	const u8 *h = bprm->buf;
	unsigned long text, data, bss, entry, textend, adata, abss, ebss, sp;
	unsigned long addr;
	int err;

	if (be32(h) != P9_S_MAGIC)
		return -ENOEXEC;
	text = be32(h + 4);
	data = be32(h + 8);
	bss = be32(h + 12);
	entry = ((u64)be32(h + 32) << 32) | be32(h + 36);
	textend = P9_UTZERO + P9_HDRSZ + text;
	if (entry < P9_UTZERO + P9_HDRSZ || entry >= textend)
		return -ENOEXEC;
	adata = ALIGN(textend, 0x200000);
	abss = PAGE_ALIGN(adata + data);
	ebss = PAGE_ALIGN(adata + data + bss);
	if (i_size_read(file_inode(bprm->file)) < P9_HDRSZ + text + data)
		return -ENOEXEC;

	err = begin_new_exec(bprm);
	if (err)
		return err;
	setup_new_exec(bprm);

	/* the text segment starts with the header, at file offset 0 */
	addr = vm_mmap(bprm->file, P9_UTZERO, P9_HDRSZ + text,
		       PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_FIXED, 0);
	if (IS_ERR_VALUE(addr))
		return addr;
	/* data is not page aligned in the file: copy it */
	if (ebss > adata) {
		addr = vm_mmap(NULL, adata, ebss - adata, PROT_READ | PROT_WRITE,
			       MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, 0);
		if (IS_ERR_VALUE(addr))
			return addr;
		err = p9_copy_from_file(bprm->file, P9_HDRSZ + text, adata, data);
		if (err)
			return err;
	}
	addr = vm_mmap(NULL, P9_TOS_PAGE, PAGE_SIZE, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, 0);
	if (IS_ERR_VALUE(addr))
		return addr;

	err = setup_arg_pages(bprm, STACK_TOP, EXSTACK_DISABLE_X);
	if (err)
		return err;

	current->mm->start_code = P9_UTZERO;
	current->mm->end_code = textend;
	current->mm->start_data = adata;
	current->mm->end_data = adata + data;
	current->mm->start_brk = current->mm->brk = ebss;
	current->mm->arg_start = bprm->p;
	current->mm->start_stack = bprm->p;

	err = p9_create_tables(bprm, &sp);
	if (err)
		return err;
	finalize_exec(bprm);
	start_thread(current_pt_regs(), entry, sp);
	return 0;
}

/* Runs on the way out of exec(): _main expects the Tos in AX. */
static void p9_exec_setup(struct pt_regs *regs)
{
	u32 pid = task_tgid_vnr(current);

	put_user(pid, (u32 __user *)(P9_TOS + P9_TOS_PID));
	regs->ax = P9_TOS;
	regs->bx = regs->cx = regs->dx = regs->si = regs->di = regs->bp = 0;
}

static struct abi_personality plan9_personality = {
	.name		= "plan9",
	.per		= PER_ABI_PLAN9,
	.owner		= THIS_MODULE,
	.sysent		= plan9_sysent,
	.decode		= p9_decode,
	.set_result	= p9_set_result,
	.fork_child	= p9_fork_child,
	.fork_return	= p9_fork_return,
	.load_binary	= p9_load_binary,
	.exec_setup	= p9_exec_setup,
	.sig_deliver	= p9_sig_deliver,
};

static int __init abi_plan9_init(void)
{
	plan9_personality.nsysent = plan9_nsysent;
	return abi_register_personality(&plan9_personality);
}

static void __exit abi_plan9_exit(void)
{
	abi_unregister_personality(&plan9_personality);
	p9_exitmsg_flush();
}

module_init(abi_plan9_init);
module_exit(abi_plan9_exit);

MODULE_DESCRIPTION("abiemu: Plan 9 (9front) amd64 emulation");
MODULE_LICENSE("GPL");
