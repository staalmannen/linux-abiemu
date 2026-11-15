// SPDX-License-Identifier: GPL-2.0
/*
 * abiemu core - recognising and starting foreign binaries
 *
 * Our binfmt handler sits in front of all other loaders.  It never
 * loads an ELF image itself: if a personality claims the file, we note
 * that in the task state and return -ENOEXEC so that the kernel's own
 * ELF loader does the mapping.  Once the exec has succeeded the
 * sched_process_exec tracepoint switches the personality, and the
 * personality gets to fix up the start registers and the auxiliary
 * vector on the way out of execve() (see abi_sys_exit()).
 *
 * Non-ELF formats (Plan 9 a.out) are loaded by the personality.
 */
#define pr_fmt(fmt) "abiemu: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/binfmts.h>
#include <linux/elf.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#include <abiemu/abiemu.h>
#include "core.h"

static const struct abi_personality *abi_pers_get(unsigned int per)
{
	const struct abi_personality *p;

	rcu_read_lock();
	p = rcu_dereference(abi_pers[per]);
	if (p && !try_module_get(p->owner))
		p = NULL;
	rcu_read_unlock();
	return p;
}

static bool abi_elf_ok(const struct elfhdr *eh)
{
	return !memcmp(eh->e_ident, ELFMAG, SELFMAG) &&
	       eh->e_ident[EI_CLASS] == ELFCLASS64 &&
	       eh->e_ident[EI_DATA] == ELFDATA2LSB &&
	       eh->e_machine == EM_X86_64 &&
	       (eh->e_type == ET_EXEC || eh->e_type == ET_DYN) &&
	       eh->e_phentsize == sizeof(struct elf_phdr) &&
	       eh->e_phnum > 0 && eh->e_phnum <= 64;
}

static struct elf_phdr *abi_elf_phdrs(struct linux_binprm *bprm,
				      const struct elfhdr *eh)
{
	size_t size = eh->e_phnum * sizeof(struct elf_phdr);
	struct elf_phdr *ph;
	loff_t pos = eh->e_phoff;

	ph = kmalloc(size, GFP_KERNEL);
	if (!ph)
		return NULL;
	if (kernel_read(bprm->file, ph, size, &pos) != size) {
		kfree(ph);
		return NULL;
	}
	return ph;
}

/* Does the image carry an ELF note with the given owner and type? */
int abi_elf_has_note(struct linux_binprm *bprm, const struct elfhdr *eh,
		     const char *owner, u32 type)
{
	size_t olen = strlen(owner) + 1;
	struct elf_phdr *ph;
	char *buf = NULL;
	int i, found = 0;

	ph = abi_elf_phdrs(bprm, eh);
	if (!ph)
		return 0;
	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	for (i = 0; buf && i < eh->e_phnum && !found; i++) {
		loff_t pos = ph[i].p_offset;
		size_t len = min_t(size_t, ph[i].p_filesz, PAGE_SIZE);
		size_t off = 0;

		if (ph[i].p_type != PT_NOTE)
			continue;
		if (kernel_read(bprm->file, buf, len, &pos) != len)
			continue;
		while (off + sizeof(struct elf_note) <= len) {
			struct elf_note *n = (void *)(buf + off);
			size_t name = off + sizeof(*n);
			size_t desc = name + ALIGN(n->n_namesz, 4);

			if (desc > len || name + n->n_namesz > len)
				break;
			if (n->n_type == type && n->n_namesz == olen &&
			    !memcmp(buf + name, owner, olen)) {
				found = 1;
				break;
			}
			off = desc + ALIGN(n->n_descsz, 4);
		}
	}
	kfree(buf);
	kfree(ph);
	return found;
}
EXPORT_SYMBOL_GPL(abi_elf_has_note);

/* Copy the PT_INTERP path into @buf; returns 1 if there is one. */
int abi_elf_interp(struct linux_binprm *bprm, const struct elfhdr *eh,
		   char *buf, size_t len)
{
	struct elf_phdr *ph;
	int i, found = 0;

	ph = abi_elf_phdrs(bprm, eh);
	if (!ph)
		return 0;
	for (i = 0; i < eh->e_phnum; i++) {
		loff_t pos = ph[i].p_offset;
		size_t n = min_t(size_t, ph[i].p_filesz, len - 1);

		if (ph[i].p_type != PT_INTERP)
			continue;
		if (kernel_read(bprm->file, buf, n, &pos) == n) {
			buf[n] = 0;
			found = 1;
		}
		break;
	}
	kfree(ph);
	return found;
}
EXPORT_SYMBOL_GPL(abi_elf_interp);

int abi_auxv_rewrite(unsigned long sp,
		     void (*fn)(struct abi_auxv *av, unsigned long uaddr,
				void *arg), void *arg)
{
	unsigned long __user *p = (unsigned long __user *)sp;
	unsigned long v, argc;
	int i;

	if (get_user(argc, p))
		return -EFAULT;
	p += 1 + argc + 1;			/* argc, argv[], NULL */
	for (i = 0; i < 0x100000; i++) {	/* envp[], NULL */
		if (get_user(v, p++))
			return -EFAULT;
		if (!v)
			break;
	}
	for (i = 0; i < 64; i++, p += 2) {
		struct abi_auxv av;

		if (copy_from_user(&av, p, sizeof(av)))
			return -EFAULT;
		if (av.type == AT_NULL)
			break;
		fn(&av, (unsigned long)p, arg);
		if (copy_to_user(p, &av, sizeof(av)))
			return -EFAULT;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(abi_auxv_rewrite);

static int abi_load_binary(struct linux_binprm *bprm)
{
	const struct elfhdr *eh = (const struct elfhdr *)bprm->buf;
	bool elf = abi_elf_ok(eh);
	const struct abi_personality *p;
	struct abi_task *t;
	unsigned int per;
	int ret;

	t = abi_task_current(false);
	if (t) {
		t->exec_pers = NULL;
		t->exec_bprm = NULL;
		t->exec_mm = NULL;
		t->exec_setup = false;
	}

	for (per = ABI_PER_MIN; per <= ABI_PER_MAX; per++) {
		p = abi_pers_get(per);
		if (!p)
			continue;

		if (!elf && p->load_binary) {
			struct mm_struct *mm = bprm->mm;

			ret = p->load_binary(bprm);
			if (!ret) {
				abi_enable();
				if (t)
					t->exec_mm = mm;
			}
			if (ret != -ENOEXEC) {
				module_put(p->owner);
				return ret;
			}
		}
		if (elf && p->probe_elf && p->probe_elf(bprm, eh) > 0) {
			ret = abi_enable();
			if (!ret && !t)
				t = abi_task_current(true);
			if (!ret && !t)
				ret = -ENOMEM;
			if (!ret) {
				t->exec_pers = p;
				t->exec_bprm = bprm;
				t->exec_mm = bprm->mm;
				ret = -ENOEXEC;	/* let binfmt_elf map it */
			}
			module_put(p->owner);
			return ret;
		}
		module_put(p->owner);
	}

	/* A foreign task executing a native binary becomes native. */
	if (abi_per_foreign(current->personality & PER_MASK))
		bprm->per_clear |= PER_MASK;
	return -ENOEXEC;
}

/* The exec can no longer fail; called in atomic context. */
void abi_tp_exec(void *data, struct task_struct *tsk, pid_t old_pid,
		 struct linux_binprm *bprm)
{
	const struct abi_personality *p;
	struct abi_task *t;

	rcu_read_lock();
	t = abi_task_current(false);
	if (!t)
		goto out;
	p = t->exec_pers;
	if (p && t->exec_bprm == bprm) {
		unsigned int per;

		/* Careful: p is only valid if it is still registered. */
		for (per = ABI_PER_MIN; per <= ABI_PER_MAX; per++)
			if (rcu_access_pointer(abi_pers[per]) == p)
				break;
		if (per <= ABI_PER_MAX) {
			current->personality =
				(current->personality & ~PER_MASK) | per;
			t->exec_setup = true;
		}
	}
	t->exec_pers = NULL;
	t->exec_bprm = NULL;
	if (!abi_per_foreign(current->personality & PER_MASK))
		abi_task_release(t);
out:
	rcu_read_unlock();
}

static struct linux_binfmt abi_binfmt = {
	.module		= THIS_MODULE,
	.load_binary	= abi_load_binary,
};

void abi_binfmt_init(void)
{
	insert_binfmt(&abi_binfmt);
}

void abi_binfmt_exit(void)
{
	unregister_binfmt(&abi_binfmt);
}
