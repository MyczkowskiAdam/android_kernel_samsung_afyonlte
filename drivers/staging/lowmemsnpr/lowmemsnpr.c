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

#include "lowmemsnpr.h"

#include "xprint.h"

#define lowmemsnpr_print(level, x...) \
	do { \
		if (lowmemsnpr.debug_level >= (level)) \
			printk(x); \
	} while (0)

#define LMSF_START 0x00000001

struct lowmemsnpr_target {
	struct list_head list;
	struct task_struct* tmm;
};

struct lowmemsnpr {
	unsigned long jiffies;
	unsigned long timeout;

	/**
	 * hidden app oom score adj.
	 */
	int oom_score_adj_min;

	/**
	 * minfree table of lowmemorykiller.
	 */
	int* minfree;
	int* minfree_size;

	/**
	 * minimum number of pages to shrink.
	 */
	int minfree_offset;

	uint32_t debug_level;

	unsigned int flag;

	spinlock_t lock;
} lowmemsnpr = {
	.jiffies = 0,
	.timeout = CONFIG_LOW_MEM_SNPR_TIMEOUT * HZ,

	.oom_score_adj_min = OOM_SCORE_ADJ_MAX,

	.minfree = NULL,
	.minfree_size = NULL,

	.minfree_offset = CONFIG_LOW_MEM_SNPR_MIN_FREE_OFFSET,

	.debug_level = 2,

	.flag = 0x00000000,

	__SPIN_LOCK_UNLOCKED(lock),
};

int
lowmemsnpr_dump(struct lowmemsnpr* l) {
	vstart(!l, -1, "%d,EINVAL");

	vreturn(0, "%d,jiffies=%lu,timeout=%lu,"
			"oom_score_adj_min=%d,"
			"minfree=0x%p,minfree_size=%d,"
			"minfree_offset=%d,"
			"debug_level=%u,lock=%d\n",
			l->jiffies, l->timeout,
			l->oom_score_adj_min,
			l->minfree, l->minfree_size ? *l->minfree_size : -1,
			l->minfree_offset, 
			l->debug_level, spin_is_locked(&l->lock));
}

int
lowmemsnpr_config(int adj, int* minfree, int* minfree_size) {
	lowmemsnpr.oom_score_adj_min = adj;

#if !defined(CONFIG_LOW_MEM_SNPR_MIN_FREE_ZERO_BASE)
	lowmemsnpr.minfree = minfree;

	/**
	 * @todo fix minfree_size missmatch issue.
	 */
	lowmemsnpr.minfree_size = minfree_size;
#endif

	lowmemsnpr_dump(&lowmemsnpr);

	return 0;
}

int
lowmemsnpr_is_hidden(int oom_score_adj) {
	return lowmemsnpr.oom_score_adj_min <= oom_score_adj;
}

int
lowmemsnpr_min(void) {
#if defined(CONFIG_LOW_MEM_SNPR_MIN_FREE_ZERO_BASE)
	int base = 0;
#else
	int base = lowmemsnpr.minfree[*lowmemsnpr.minfree_size - 1];
#endif

	return base + lowmemsnpr.minfree_offset;
}

int
lowmemsnpr_log_sigkill(
		const char* prefix, struct task_struct* victim,
		int victim_oom_score_adj, int victim_size) {
	int other_free = 0;
	int other_file = 0;
	int min = 0;

	other_file = global_page_state(NR_FILE_PAGES) - global_page_state(NR_SHMEM);
	other_free = global_page_state(NR_FREE_PAGES) - totalreserve_pages;

	min = lowmemsnpr_min();

	lowmemsnpr_print(1, "%s: send %s to %d (%s), adj %d, size %d, "
			"ofree %dMB ofile %dMB min %dMB\n",
			prefix,
			CONFIG_LOW_MEM_SNPR_LOG_SIGKILL,
			victim->pid, victim->comm,
			victim_oom_score_adj, victim_size,
			MB(other_free),
			MB(other_file),
			MB(min));

	return 0;
}

bool
lowmemsnpr_shrinkable(struct shrinker* s, struct shrink_control* sc) {
	bool result = false;
	int other_free = 0;
	int other_file = 0;
	int min = 0;

	if (!s || !sc)
		goto out;

	if (sc->nr_to_scan <= 0)
		goto out;

	if (time_before_eq(jiffies, lowmemsnpr.jiffies + lowmemsnpr.timeout))
		goto out;

	other_free = global_page_state(NR_FREE_PAGES) - totalreserve_pages;
	other_file = global_page_state(NR_FILE_PAGES) - global_page_state(NR_SHMEM);

	min = lowmemsnpr_min();

	if (other_free < min && other_file < min) {
		result = true;
	}

out:
	return result;
}

int
lowmemsnpr_start(void) {
	if (lowmemsnpr.flag & LMSF_START)
		return 0;

	if (spin_trylock(&lowmemsnpr.lock)) {
		lowmemsnpr.flag |= LMSF_START;

		spin_unlock(&lowmemsnpr.lock);
	}

	return lowmemsnpr.flag & LMSF_START;
}

int
lowmemsnpr_end(void) {
	spin_lock(&lowmemsnpr.lock);

	lowmemsnpr.flag &= ~LMSF_START;

	spin_unlock(&lowmemsnpr.lock);

	return lowmemsnpr.flag & LMSF_START;
}

int 
lowmemsnpr_shrink(struct shrinker *s, struct shrink_control *sc) {
	struct task_struct* victim = NULL;
	struct task_struct* p = NULL;
	int victim_size = -1;
	int victim_oom_score_adj = -1;

	int rem = global_page_state(NR_ACTIVE_ANON) +
		global_page_state(NR_ACTIVE_FILE) +
		global_page_state(NR_INACTIVE_ANON) +
		global_page_state(NR_INACTIVE_FILE);

	if (!lowmemsnpr_start())
		goto rtn;

	if (!lowmemsnpr_shrinkable(s, sc))
		goto end;

	if (!(p = lowmemsnpr_select()))
		goto end;

	task_lock(victim = p);

	victim_size = victim->mm ? get_mm_rss(victim->mm) : 0;
	victim_oom_score_adj = victim->signal->oom_score_adj;

	lowmemsnpr_del_task(victim, __func__);

	lowmemsnpr_log_sigkill(__func__, victim, victim_oom_score_adj, victim_size);

	lowmemsnpr.jiffies = jiffies;

	task_unlock(victim);

	send_sig(SIGKILL, victim, 0);

	set_tsk_thread_flag(victim, TIF_MEMDIE);

	rem -= victim_size;

end:
	lowmemsnpr_end();

rtn:
	return rem;
}

static struct shrinker
lowmemsnpr_shrinker = {
	.shrink = lowmemsnpr_shrink,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init 
lowmemsnpr_init(void) {
	lowmemsnpr_setup();

	register_shrinker(&lowmemsnpr_shrinker);

	return 0;
}

static void __exit 
lowmemsnpr_exit(void) {
	unregister_shrinker(&lowmemsnpr_shrinker);
}

module_param_named(oom_score_adj_min, lowmemsnpr.oom_score_adj_min, int, S_IRUGO | S_IWUSR);

module_param_named(minfree_offset, lowmemsnpr.minfree_offset, uint, S_IRUGO | S_IWUSR);

module_init(lowmemsnpr_init);
module_exit(lowmemsnpr_exit);

MODULE_LICENSE("GPL");
