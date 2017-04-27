/**
 * Low memory sniper.
 *
 * @draft kumhyun.cho@samsung.com
 * @since 2013.06.10
 */

#ifndef __LOWMEMORYKILLER_RTDS__
#define __LOWMEMORYKILLER_RTDS__

#define KB(x) ((x) << (PAGE_SHIFT - 10))
#define MB(x) (KB(x) / 1024)

/**
 * @todo reference to 
 *       /android/frameworks/base/services/java/com/android/server/am/ProcessList.java
 */
#define HIDDEN_APP_MIN_ADJ 9

int
lowmemsnpr_config(int adj, int* minfree, int* minfree_size);

extern int
lowmemsnpr_is_hidden(int oom_score_adj);

struct lowmemsnpr_target*
lowmemsnpr_add_task(struct task_struct* task, const char* req);

bool
lowmemsnpr_del_task(struct task_struct* task, const char* req);

struct task_struct* 
lowmemsnpr_select(void);

bool
lowmemsnpr_setup(void);

#endif // __LOWMEMORYKILLER_RTDS__
