/**
 * Low memory sniper.
 *
 * @draft kumhyun.cho@samsung.com
 * @since 2013.06.10
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/swap.h>

#include "xprint.h"

struct lowmemsnpr_default {
	struct list_head list;
} lowmemsnpr_default;

struct lowmemsnpr_target*
lowmemsnpr_find_target(struct task_struct* tmm) {
	struct lowmemsnpr_target* result = NULL;
	struct lowmemsnpr_target* target = NULL;
	struct lowmemsnpr_target* next = NULL;

	vstart(!tmm, NULL, "0x%p,EINVAL");

	list_for_each_entry_safe(target, next, &lowmemsnpr_default.list, list) {
		if (!target || !target->tmm)
			continue;

		if (target->tmm->pid == tmm->pid) {
			result = target;
			break;
		}
	}

	vreturn(result, "0x%p,result={%s,%d},tmm={%s,%d}",
			result ? result->tmm->comm : "(nil)", result ? result->tmm->pid : -1,
			tmm->comm, tmm->pid);
}

struct lowmemsnpr_target*
lowmemsnpr_add_task(struct task_struct* task, const char* req) {
	struct lowmemsnpr_target* target = NULL;
	struct task_struct* tmm = NULL;

	vstart(!task, NULL, "0x%p,EINVAL,req=%s", req);

	spin_lock(&lowmemsnpr.lock);

	if (!(tmm = find_lock_task_mm(task)))
		goto out;

	if (lowmemsnpr_find_target(tmm))
		goto out;

	if (!(target = kmalloc(sizeof(struct lowmemsnpr_target), GFP_KERNEL)))
		goto out;

	target->tmm = tmm;

	list_add_tail(&target->list, &lowmemsnpr_default.list);

out:
	task_unlock(tmm);

	spin_unlock(&lowmemsnpr.lock);

	vreturn(target, "0x%p,task={%s,%d},tmm={%s,%d},req=%s",
			task->comm, task->pid,
			tmm ? tmm->comm : "(nil)", tmm ? tmm->pid : -1,
			req);
}
EXPORT_SYMBOL(lowmemsnpr_add_task);

bool
lowmemsnpr_del_task(struct task_struct* task, const char* req) {
	struct lowmemsnpr_target* target = NULL;
	struct task_struct* tmm = NULL;
	bool result = false;

	vstart(!task, result, "%d,EINVAL,req=%s", req);

	spin_lock(&lowmemsnpr.lock);

	if (!(tmm = find_lock_task_mm(task)))
		goto out;

	if ((target = lowmemsnpr_find_target(task))) {
		list_del(&target->list);

		xfree(target);

		result = true;
	}

	task_unlock(tmm);

out:
	spin_unlock(&lowmemsnpr.lock);

	vreturn(result, "%d,task={%s,%d},tmm={%s,%d},req=%s",
			task->comm, task->pid,
			tmm ? tmm->comm : "(nil)", tmm ? tmm->pid : -1,
			req);

}
EXPORT_SYMBOL(lowmemsnpr_del_task);

struct task_struct*
lowmemsnpr_select(void) {
	struct lowmemsnpr_target* target = NULL;
	struct lowmemsnpr_target* next = NULL;

	struct task_struct* tmm = NULL;
	struct signal_struct* signal = NULL; 

	struct task_struct* s_tmm = NULL;
	int s_size = 0;

	spin_lock(&lowmemsnpr.lock);

	list_for_each_entry_safe(target, next, &lowmemsnpr_default.list, list) {
		if (!target || !(tmm = find_lock_task_mm(target->tmm)))
			continue;

		if (tmm->flags & (PF_KTHREAD | PF_EXITING | PF_EXITPIDONE | PF_SIGNALED))
			goto next;

		if (test_tsk_thread_flag(tmm, TIF_MEMDIE)
				&& time_before_eq(jiffies, lowmemsnpr.timeout)) {
			goto out;
		}

		if (!(signal = tmm->signal))
			goto next;

		if (!lowmemsnpr_is_hidden(signal->oom_score_adj))
			goto next;

		if (s_tmm) {
			if (signal->oom_score_adj < s_tmm->signal->oom_score_adj) {
				goto next;
			} else if (s_tmm->signal->oom_score_adj == signal->oom_score_adj) {
				if (!tmm->mm)
					goto next;

				if (get_mm_rss(tmm->mm) < s_size)
					goto next;
			}
		}

		s_tmm = tmm;
		s_size = get_mm_rss(s_tmm->mm);

		lowmemsnpr_print(2, "%s select %d (%s), adj %d, size %d, to kill\n",
				__func__, s_tmm->pid, s_tmm->comm,
				s_tmm->signal->oom_score_adj, s_size);

next:
		task_unlock(tmm);
		continue;
out:
		task_unlock(tmm);
		break;
	}

	spin_unlock(&lowmemsnpr.lock);

	return s_tmm;
}

bool
lowmemsnpr_setup(void) {
	INIT_LIST_HEAD(&lowmemsnpr_default.list);

	return true;
}
