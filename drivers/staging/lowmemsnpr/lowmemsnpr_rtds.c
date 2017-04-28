/**
 * Low memory sniper to select RTDS process.
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

#include "lowmemsnpr.h"

#include "xprint.h"

#include "rtds_memory_drv.h"
#include "rtds_memory_drv_main.h"
#include "rtds_memory_drv_sub.h"

#define OOM_SCORE_ADJ_INVAL (OOM_SCORE_ADJ_MIN - 999)

void
lowmemsnpr_rtds_dump(const char* prefix, rtds_pid_table* entry) {
#if defined(DEBUG)
	struct task_struct* task = NULL;
	struct signal_struct* signal = NULL;

	pid_t tgid = -1;
	unsigned int size = 0;
	pid_t task_pid = -1;
	pid_t task_tgid = -1;
	const char* task_comm = "(nil)";
	int signal_oom_score_adj = OOM_SCORE_ADJ_INVAL;

	if (entry) {
		tgid = entry->tgid;
		size = entry->size;

		if ((task = entry->task_info)) {
			task_pid = task->pid;
			task_tgid = task->tgid;
			task_comm = task->comm;

			if ((signal = task->signal)) {
				signal_oom_score_adj = signal->oom_score_adj;
			}
		}
	}

	xprint("%s: 0x%p={tgid=%5d,size=%8u,task={pid=%5d,tgid=%5d,comm=%16s},signal={oom_score_adj=%5d}",
			prefix ? prefix : __func__,
			entry,
			tgid,
			size,
			task_pid,
			task_tgid,
			task_comm,
			signal_oom_score_adj);
#endif
}

struct task_struct*
lowmemsnpr_select(void) {
	struct task_struct* victim_task = NULL;
	int victim_tgid = 0;
	unsigned int victim_size = 0;
	int victim_oom_score_adj = OOM_SCORE_ADJ_INVAL;
	char victim_comm[TASK_COMM_LEN] = "(nil)";

	struct task_struct* task = NULL;
	struct signal_struct* signal = NULL;
	int oom_score_adj = OOM_SCORE_ADJ_INVAL;

	rtds_pid_table* entry = NULL;
	rtds_pid_table* next = NULL;

	unsigned long timeout = jiffies + (HZ / 20);

	RTDS_MEM_DOWN_TIMEOUT(&g_rtds_memory_shared_mem);

	read_lock(&tasklist_lock);

	list_for_each_entry_safe(entry, next, &g_rtds_process_list, head) {
		if (!entry) continue;

		if (!(task = entry->task_info)) goto invalid;

		rcu_read_lock();

		if (!find_task_by_vpid(task->pid)) {
			rcu_read_unlock();

			goto invalid;
		}

		rcu_read_unlock();

		task_lock(task);

		if (entry->tgid != task->tgid) goto invalid;

		if (!(signal = task->signal)) goto invalid;

		oom_score_adj = signal->oom_score_adj;

		if ((oom_score_adj < OOM_SCORE_ADJ_MIN)
				|| (OOM_SCORE_ADJ_MAX < signal->oom_score_adj))
			goto invalid;

		if (!lowmemsnpr_is_hidden(oom_score_adj)) goto next;

		if (test_tsk_thread_flag(task, TIF_MEMDIE)) goto invalid;

		lowmemsnpr_rtds_dump(__func__, entry);

		if (victim_task) {
#if defined(CONFIG_LOW_MEM_SNPR_SIZE)
			if (entry->size < victim_size)
				goto next;
#else
			if (oom_score_adj < victim_oom_score_adj) {
				goto next;
			} else if (oom_score_adj == victim_oom_score_adj) {
				if (entry->size < victim_size)
					goto next;
			}
#endif
		}

		victim_task = task;
		victim_tgid = task->tgid;
		victim_size = entry->size;
		strncpy(victim_comm, task->comm, TASK_COMM_LEN);

		goto next;

invalid:
#if 0
		list_del(&entry->head);
#endif

next:
		if (task)
			task_unlock(task);

		if (time_after(jiffies, timeout))
			sbreak("timeout");
	}

	read_unlock(&tasklist_lock);

	up(&g_rtds_memory_shared_mem);

	vreturn(victim_task,
			"0x%p,victim={comm=%s,tgid=%d,size=%u,oom_score_adj=%d}",
			victim_comm,
			victim_tgid,
			victim_size,
			victim_oom_score_adj);
}

struct lowmemsnpr_target*
lowmemsnpr_add_task(struct task_struct* task, const char* req) {
	/**
	 * Not support.
	 */
	return NULL;
}
EXPORT_SYMBOL(lowmemsnpr_add_task);

bool
lowmemsnpr_del_task(struct task_struct* task, const char* req) {
	/**
	 * Not support.
	 */
	return false;
}
EXPORT_SYMBOL(lowmemsnpr_del_task);

bool
lowmemsnpr_setup(void) {
	/**
	 * Empty.
	 */
	return true;
}
