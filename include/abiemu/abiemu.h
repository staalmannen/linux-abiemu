/* SPDX-License-Identifier: GPL-2.0 */
/*
 * abiemu - foreign system call ABI emulation for Linux
 *
 * Interface between the core module (abiemu.ko) and the personality
 * modules (abi_freebsd.ko, abi_plan9.ko, ...).
 */
#ifndef _ABIEMU_ABIEMU_H
#define _ABIEMU_ABIEMU_H

#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/list.h>
#include <linux/personality.h>
#include <linux/sched.h>
#include <linux/elf.h>
#include <linux/signal.h>
#include <asm/ucontext.h>
#include <asm/page.h>

#ifndef CONFIG_X86_64
#error "abiemu currently only supports x86_64"
#endif

/*
 * Personality identifiers.  They live in the low byte of
 * current->personality, are inherited over fork() and are what marks a
 * task as "foreign".  The values are chosen to stay clear of the PER_*
 * constants the kernel itself knows about.
 */
#define PER_ABI_FREEBSD		0x41
#define PER_ABI_NETBSD		0x42
#define PER_ABI_OPENBSD		0x43
#define PER_ABI_ILLUMOS		0x44
#define PER_ABI_PLAN9		0x45

struct abi_ctx;
struct abi_task;
struct linux_binprm;

typedef long (*abi_handler_t)(struct abi_ctx *ctx);
typedef long (*abi_post_t)(struct abi_ctx *ctx, long ret);

/**
 * struct abi_sysent - one entry of a foreign system call table
 * @name:    name of the foreign call, NULL for an unassigned slot
 * @handler: emulation routine, or NULL for a 1:1 pass-through to @native
 * @native:  Linux system call number used for pass-through entries
 * @nargs:   number of arguments (used for tracing only)
 */
struct abi_sysent {
	const char	*name;
	abi_handler_t	handler;
	short		native;
	unsigned char	nargs;
};

#define ABI_PASS(_name, _nargs, _native) \
	{ .name = _name, .nargs = _nargs, .native = _native }
#define ABI_EMUL(_name, _nargs, _handler) \
	{ .name = _name, .nargs = _nargs, .handler = _handler, .native = -1 }

enum abi_state {
	ABI_ST_IDLE = 0,	/* not inside a foreign system call */
	ABI_ST_EMUL,		/* handler running, result produced by us */
	ABI_ST_NATIVE,		/* registers rewritten, Linux runs the call */
};

/* abi_ctx.flags */
#define ABI_F_NORESULT	0x01	/* handler loaded the registers itself */
#define ABI_F_NORESTART	0x02	/* never restart, always report EINTR */

#define ABI_MAXARGS	8

/**
 * struct abi_ctx - state of the foreign system call in progress
 *
 * A handler runs at system call entry.  It either returns the final
 * result (a value or a negative Linux errno), or it calls abi_native()
 * which rewrites the user registers so that Linux executes a native
 * system call.  In the latter case @post, if set, is called at system
 * call exit to convert the native result.
 */
struct abi_ctx {
	struct pt_regs			*regs;
	const struct abi_personality	*pers;
	const struct abi_sysent		*ent;
	unsigned int			nr;
	unsigned long			args[ABI_MAXARGS];
	abi_post_t			post;
	unsigned long			priv[6];  /* handler -> post scratch */
	unsigned int			flags;
	enum abi_state			state;
	unsigned long			gap;	  /* stack gap cursor */
	/* user registers saved at entry */
	unsigned long			sv_nr, sv_di, sv_si, sv_dx,
					sv_r10, sv_r8, sv_r9;
};

/**
 * struct abi_personality - description of a foreign ABI
 * @decode:      extract system call number and arguments into the ctx
 * @set_result:  store a Linux style result using the foreign convention
 * @fork_child:  fix up the registers of a freshly forked child (atomic)
 * @probe_elf:   return > 0 if the ELF image belongs to this personality;
 *               the generic Linux ELF loader then maps it
 * @load_binary: loader for non-ELF formats (-ENOEXEC if not recognised)
 * @exec_setup:  called in the new image just before it first runs, to
 *               adjust the initial registers and stack (may sleep)
 */
struct abi_personality {
	const char		*name;
	unsigned int		per;
	struct module		*owner;
	const struct abi_sysent	*sysent;
	unsigned int		nsysent;
	const unsigned short	*errno_map;	/* Linux errno -> foreign */
	unsigned int		nerrno;
	const void		*data;		/* for helper libraries */

	int  (*decode)(struct abi_ctx *ctx);
	void (*set_result)(struct abi_ctx *ctx, long ret);
	void (*fork_child)(struct abi_ctx *parent, struct pt_regs *child);
	int  (*probe_elf)(struct linux_binprm *bprm, const struct elfhdr *eh);
	int  (*load_binary)(struct linux_binprm *bprm);
	void (*exec_setup)(struct pt_regs *regs);
	/* signal delivery, see core/signal.c */
	int  (*sig_deliver)(struct abi_ctx *ctx, int lsig,
			    unsigned long handler, unsigned long lframe);
	long (*sig_return)(struct abi_ctx *ctx);

	struct list_head	list;
};

/**
 * struct abi_task - per thread emulation state
 * @priv: personality private data, kfree()d with the task
 */
struct abi_task {
	struct hlist_node		node;
	struct rcu_head			rcu;
	struct task_struct		*task;
	const struct abi_personality	*pers;
	struct abi_ctx			ctx;
	void				*priv;
	unsigned int			priv_per;
	/* exec in progress */
	const struct abi_personality	*exec_pers;
	struct linux_binprm		*exec_bprm;
	struct mm_struct		*exec_mm;
	bool				exec_setup;
};

int abi_register_personality(struct abi_personality *p);
void abi_unregister_personality(struct abi_personality *p);

/* Make sure foreign system calls are intercepted from now on. */
int abi_enable(void);

struct abi_task *abi_task_current(bool create);
void *abi_task_priv(size_t size);

/* Run a native Linux system call in place of the foreign one. */
long abi_native(struct abi_ctx *ctx, int nr, unsigned long a0,
		unsigned long a1, unsigned long a2, unsigned long a3,
		unsigned long a4, unsigned long a5);

#define abi_native0(c, nr)		abi_native(c, nr, 0, 0, 0, 0, 0, 0)
#define abi_native1(c, nr, a)		abi_native(c, nr, a, 0, 0, 0, 0, 0)
#define abi_native2(c, nr, a, b)	abi_native(c, nr, a, b, 0, 0, 0, 0)
#define abi_native3(c, nr, a, b, d)	abi_native(c, nr, a, b, d, 0, 0, 0)
#define abi_native4(c, nr, a, b, d, e)	abi_native(c, nr, a, b, d, e, 0, 0)
#define abi_native5(c, nr, a, b, d, e, f) \
	abi_native(c, nr, a, b, d, e, f, 0)

/*
 * Reserve scratch space in user memory, below the user stack pointer
 * (and its red zone).  Used to hand converted structures to native
 * system calls, which only accept user pointers.
 */
void __user *abi_gap(struct abi_ctx *ctx, size_t size);

/* Copy a kernel string into the stack gap; returns NULL on fault. */
char __user *abi_gap_str(struct abi_ctx *ctx, const char *s);

/* Generic helpers */
int abi_errno(const struct abi_personality *p, long err);

struct kstat;
int abi_stat_fd(int fd, struct kstat *st);
int abi_stat_path(int dfd, const char __user *path, bool follow,
		  bool empty_ok, struct kstat *st);

/*
 * Directory reading.  @emit is called for every entry and returns the
 * number of bytes it stored at @buf, or 0 if the entry does not fit in
 * @room bytes.  Returns the number of bytes produced or an error.
 */
typedef size_t (*abi_dirent_fn)(void *buf, size_t room, const char *name,
				int namlen, u64 ino, unsigned int type,
				loff_t next);
long abi_getdents(int fd, void __user *ubuf, size_t count, loff_t *basep,
		  abi_dirent_fn emit);

/* Map a trampoline/scratch area into the current process. */
unsigned long abi_map_anon(unsigned long addr, unsigned long len,
			   unsigned long prot, unsigned long flags);
int abi_elf_has_note(struct linux_binprm *bprm, const struct elfhdr *eh,
		     const char *owner, u32 type);
int abi_elf_interp(struct linux_binprm *bprm, const struct elfhdr *eh,
		   char *buf, size_t len);

/*
 * Signals.  Foreign handlers are never installed directly: Linux calls
 * a stub in a per process trampoline page which traps back into the
 * personality, which then builds the signal frame the foreign handler
 * expects.  All signal numbers, flags and masks here are Linux ones.
 */
#define ABI_TRAMP_ADDR		0x100000UL
#define ABI_TRAMP_ENTRY		(ABI_TRAMP_ADDR + 0)
#define ABI_TRAMP_RETURN	(ABI_TRAMP_ADDR + 4)
#define ABI_TRAMP_DATA		(ABI_TRAMP_ADDR + PAGE_SIZE)

struct abi_sigact {
	unsigned long	handler;	/* SIG_DFL, SIG_IGN or user function */
	unsigned long	flags;		/* SA_* */
	u64		mask;
};

/* Layout of the frame Linux pushes for a signal handler (x86_64). */
struct abi_linux_sigframe {
	u64			pretcode;
	struct ucontext		uc;
	struct siginfo		info;
};

long abi_sigaction(struct abi_ctx *ctx, int lsig,
		   const struct abi_sigact *act, bool want_old);
int abi_sigaction_old(struct abi_ctx *ctx, struct abi_sigact *old);
/* Leave a signal handler: unwind the Linux frame at @lframe. */
long abi_sigreturn(struct abi_ctx *ctx, unsigned long lframe);

struct abi_auxv { unsigned long type, val; };
/*
 * Walk the ELF auxiliary vector of the fresh image at @sp and let @fn
 * rewrite each entry in place; @uaddr is the user address of the entry.
 */
int abi_auxv_rewrite(unsigned long sp,
		     void (*fn)(struct abi_auxv *av, unsigned long uaddr,
				void *arg), void *arg);

extern int abi_trace;
#define ABI_TRACE_UNIMPL	1
#define ABI_TRACE_CALLS		2

static inline bool abi_is_error(long ret)
{
	return (unsigned long)ret >= (unsigned long)-MAX_ERRNO;
}

/* x86_64 SysV style calling convention shared by the BSDs and illumos */
static inline void abi_decode_sysv(struct abi_ctx *ctx)
{
	ctx->nr = ctx->sv_nr;
	ctx->args[0] = ctx->sv_di;
	ctx->args[1] = ctx->sv_si;
	ctx->args[2] = ctx->sv_dx;
	ctx->args[3] = ctx->sv_r10;
	ctx->args[4] = ctx->sv_r8;
	ctx->args[5] = ctx->sv_r9;
}

#endif /* _ABIEMU_ABIEMU_H */
