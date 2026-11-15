// SPDX-License-Identifier: GPL-2.0
/*
 * abiemu core - personality registry and per task state
 *
 * How it works on a current kernel
 * --------------------------------
 * The old iBCS/linux-abi code hooked into exec domains and the lcall7
 * gate, both of which are long gone.  This implementation only uses
 * interfaces that are available to modules today:
 *
 *  - A task is marked foreign through the low byte of
 *    current->personality (PER_ABI_*), which is inherited over fork().
 *
 *  - The sys_enter/sys_exit tracepoints are used to intercept every
 *    system call of a foreign task.  The generic entry code explicitly
 *    allows a sys_enter probe to change the system call number and
 *    arguments, or to skip the call altogether (see syscall.c).
 *
 *  - A binfmt handler placed in front of the regular loaders recognises
 *    foreign binaries.  Foreign ELF images are still mapped by the
 *    kernel's own ELF loader; we only brand the resulting process.
 */
#define pr_fmt(fmt) "abiemu: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/tracepoint.h>
#include <linux/sched/task_stack.h>

#include <abiemu/abiemu.h>
#include "core.h"

int abi_trace;
module_param_named(trace, abi_trace, int, 0644);
MODULE_PARM_DESC(trace, "1: log unimplemented calls, 2: log every foreign call");
EXPORT_SYMBOL_GPL(abi_trace);

static bool abi_eager;
module_param_named(eager, abi_eager, bool, 0444);
MODULE_PARM_DESC(eager,
	"intercept system calls right away instead of at the first foreign exec");

const struct abi_personality __rcu *abi_pers[ABI_PER_MAX + 1];
static DEFINE_MUTEX(abi_pers_mutex);

/*
 * Per task state
 */
static DEFINE_HASHTABLE(abi_tasks, 8);
static DEFINE_SPINLOCK(abi_tasks_lock);

static struct abi_task *abi_task_find(struct task_struct *tsk)
{
	struct abi_task *t;

	hash_for_each_possible_rcu(abi_tasks, t, node, (unsigned long)tsk)
		if (t->task == tsk)
			return t;
	return NULL;
}

/*
 * The entry for a task is only removed once the task is gone, so the
 * result stays valid for current without further protection.
 */
struct abi_task *abi_task_current(bool create)
{
	struct abi_task *t;
	unsigned long flags;

	rcu_read_lock();
	t = abi_task_find(current);
	rcu_read_unlock();
	if (t || !create)
		return t;

	t = kzalloc(sizeof(*t), GFP_KERNEL);
	if (!t)
		return NULL;
	t->task = current;
	spin_lock_irqsave(&abi_tasks_lock, flags);
	hash_add_rcu(abi_tasks, &t->node, (unsigned long)current);
	spin_unlock_irqrestore(&abi_tasks_lock, flags);
	return t;
}
EXPORT_SYMBOL_GPL(abi_task_current);

/* Personality private per task data, zeroed on first use. */
void *abi_task_priv(size_t size)
{
	struct abi_task *t = abi_task_current(true);
	unsigned int per = current->personality & PER_MASK;

	if (!t)
		return NULL;
	if (t->priv && t->priv_per != per) {
		kfree(t->priv);
		t->priv = NULL;
	}
	if (!t->priv) {
		t->priv = kzalloc(size, GFP_KERNEL);
		t->priv_per = per;
	}
	return t->priv;
}
EXPORT_SYMBOL_GPL(abi_task_priv);

static void abi_task_unbind(struct abi_task *t)
{
	if (t->pers) {
		module_put(t->pers->owner);
		t->pers = NULL;
	}
}

/*
 * Return the personality that handles the calling task, or NULL if its
 * personality byte does not belong to a registered module.  The task
 * holds a reference on the module for as long as it is bound.
 */
const struct abi_personality *abi_task_bind(struct abi_task *t,
					    unsigned int per)
{
	const struct abi_personality *p;

	if (likely(t->pers && t->pers->per == per))
		return t->pers;

	abi_task_unbind(t);
	rcu_read_lock();
	p = rcu_dereference(abi_pers[per]);
	if (p && !try_module_get(p->owner))
		p = NULL;
	rcu_read_unlock();
	t->pers = p;
	return p;
}

void abi_task_release(struct abi_task *t)
{
	abi_task_unbind(t);
}

/* Called when the last reference to a task_struct is dropped (atomic). */
static void abi_tp_free(void *data, struct task_struct *tsk)
{
	struct abi_task *t;
	unsigned long flags;

	spin_lock_irqsave(&abi_tasks_lock, flags);
	t = abi_task_find(tsk);
	if (t)
		hash_del_rcu(&t->node);
	spin_unlock_irqrestore(&abi_tasks_lock, flags);
	if (!t)
		return;
	abi_task_unbind(t);
	kfree(t->priv);
	kfree_rcu(t, rcu);
}

static void abi_tasks_destroy(void)
{
	struct abi_task *t;
	struct hlist_node *tmp;
	int bkt;

	hash_for_each_safe(abi_tasks, bkt, tmp, t, node) {
		hash_del(&t->node);
		abi_task_unbind(t);
		kfree(t->priv);
		kfree(t);
	}
}

/*
 * Tracepoints.  The ones we need are not exported by name, so look
 * them up the same way out of tree tracers do.
 */
static struct tracepoint *tp_sys_enter, *tp_sys_exit;
static struct tracepoint *tp_fork, *tp_exec, *tp_free;

static void __init abi_tp_lookup(struct tracepoint *tp, void *priv)
{
	if (!strcmp(tp->name, "sys_enter"))
		tp_sys_enter = tp;
	else if (!strcmp(tp->name, "sys_exit"))
		tp_sys_exit = tp;
	else if (!strcmp(tp->name, "sched_process_fork"))
		tp_fork = tp;
	else if (!strcmp(tp->name, "sched_process_exec"))
		tp_exec = tp;
	else if (!strcmp(tp->name, "sched_process_free"))
		tp_free = tp;
}

static DEFINE_MUTEX(abi_enable_mutex);
static bool abi_enabled;

/*
 * Hooking the system call tracepoints sends every task in the system
 * through the slow system call path, so this is only done once there
 * is something to emulate.
 */
int abi_enable(void)
{
	int err = 0;

	if (READ_ONCE(abi_enabled))
		return 0;

	mutex_lock(&abi_enable_mutex);
	if (abi_enabled)
		goto out;
	err = tracepoint_probe_register(tp_sys_exit, abi_sys_exit, NULL);
	if (err)
		goto out;
	err = tracepoint_probe_register(tp_sys_enter, abi_sys_enter, NULL);
	if (err) {
		tracepoint_probe_unregister(tp_sys_exit, abi_sys_exit, NULL);
		goto out;
	}
	WRITE_ONCE(abi_enabled, true);
	pr_info("system call interception enabled\n");
out:
	mutex_unlock(&abi_enable_mutex);
	return err;
}
EXPORT_SYMBOL_GPL(abi_enable);

int abi_register_personality(struct abi_personality *p)
{
	int err = 0;

	if (p->per < ABI_PER_MIN || p->per > ABI_PER_MAX ||
	    !p->decode || !p->set_result || !p->sysent)
		return -EINVAL;

	mutex_lock(&abi_pers_mutex);
	if (rcu_access_pointer(abi_pers[p->per]))
		err = -EBUSY;
	else
		rcu_assign_pointer(abi_pers[p->per], p);
	mutex_unlock(&abi_pers_mutex);
	if (err)
		return err;

	if (abi_eager)
		abi_enable();
	pr_info("registered personality %s (0x%02x)\n", p->name, p->per);
	return 0;
}
EXPORT_SYMBOL_GPL(abi_register_personality);

void abi_unregister_personality(struct abi_personality *p)
{
	mutex_lock(&abi_pers_mutex);
	if (rcu_access_pointer(abi_pers[p->per]) == p)
		RCU_INIT_POINTER(abi_pers[p->per], NULL);
	mutex_unlock(&abi_pers_mutex);
	synchronize_rcu();
	pr_info("unregistered personality %s\n", p->name);
}
EXPORT_SYMBOL_GPL(abi_unregister_personality);

static int __init abiemu_init(void)
{
	int err;

	for_each_kernel_tracepoint(abi_tp_lookup, NULL);
	if (!tp_sys_enter || !tp_sys_exit || !tp_fork || !tp_exec || !tp_free) {
		pr_err("required tracepoints not found (CONFIG_HAVE_SYSCALL_TRACEPOINTS?)\n");
		return -ENODEV;
	}

	err = abi_tramp_init();
	if (err)
		return err;
	err = tracepoint_probe_register(tp_free, abi_tp_free, NULL);
	if (err)
		goto err_tramp;
	err = tracepoint_probe_register(tp_fork, abi_tp_fork, NULL);
	if (err)
		goto err_free;
	err = tracepoint_probe_register(tp_exec, abi_tp_exec, NULL);
	if (err)
		goto err_fork;

	abi_binfmt_init();
	pr_info("foreign ABI emulation core loaded\n");
	return 0;

err_fork:
	tracepoint_probe_unregister(tp_fork, abi_tp_fork, NULL);
err_free:
	tracepoint_probe_unregister(tp_free, abi_tp_free, NULL);
	tracepoint_synchronize_unregister();
err_tramp:
	abi_tramp_exit();
	return err;
}

static void __exit abiemu_exit(void)
{
	abi_binfmt_exit();
	if (abi_enabled) {
		tracepoint_probe_unregister(tp_sys_enter, abi_sys_enter, NULL);
		tracepoint_probe_unregister(tp_sys_exit, abi_sys_exit, NULL);
	}
	tracepoint_probe_unregister(tp_exec, abi_tp_exec, NULL);
	tracepoint_probe_unregister(tp_fork, abi_tp_fork, NULL);
	tracepoint_probe_unregister(tp_free, abi_tp_free, NULL);
	tracepoint_synchronize_unregister();
	rcu_barrier();
	abi_tasks_destroy();
	abi_tramp_exit();
}

module_init(abiemu_init);
module_exit(abiemu_exit);

MODULE_DESCRIPTION("Foreign system call ABI emulation core");
MODULE_AUTHOR("linux-abiemu contributors");
MODULE_LICENSE("GPL");
