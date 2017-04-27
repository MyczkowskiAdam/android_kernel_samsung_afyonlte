/*
 * Function        : Composer driver for SH Mobile
 *
 * Copyright (C) 2013-2013 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/kthread.h>

#include "../composer/sh_mobile_debug.h"
#include "../composer/sh_mobile_work.h"

/******************************************************/
/* define prototype                                   */
/******************************************************/


/******************************************************/
/* define local define                                */
/******************************************************/
#define USE_FIFO_SCHDULE    1

#if USE_FIFO_SCHDULE
#define THREAD_PRIORITY  (1)
#else
#define THREAD_NICE      (-15)
#endif

/******************************************************/
/* define local variables                             */
/******************************************************/


/******************************************************/
/* local functions                                    */
/******************************************************/

static void localwork_init(
	struct localwork *work, void (*func)(struct localwork *))
{
	INIT_LIST_HEAD(&work->link);
	work->func = func;
	work->status = false;
}


static void localworkqueue_flush(struct localworkqueue *wq)
{
	if (wq == NULL) {
		/* report error */
		printk_err("invalid argument.\n");
	} else if (!list_empty(&wq->top)) {
		/* wait all task complete */
		printk_dbg2(3, "wait localworkqueue complete\n");

		wait_event( \
			wq->finish, list_empty(&wq->top));
	}
}

static void localworkqueue_destroy(struct localworkqueue *wq)
{
	unsigned long flags;

	if (wq == NULL) {
		/* report error */
		printk_err("invalid argument.\n");
	} else {
		/* request task stop */
		if (wq->task)
			kthread_stop(wq->task);

		/* wakeup pending thread */
		printk_dbg2(3, "spinlock\n");
		spin_lock_irqsave(&wq->lock, flags);

		while (!list_empty(&wq->top)) {
			struct list_head *list;
			struct localwork *work = NULL;

			printk_dbg2(3, "localwork not empty\n");

			list_for_each(list, &wq->top)
			{
				work = list_entry(list,
					struct localwork, link);
				break;
			}
			if (work) {
				printk_dbg2(3, "localwork pending: %p\n", work);
				work->status = true;
				list_del_init(&work->link);
			}
		}
		spin_unlock_irqrestore(&wq->lock, flags);

		wake_up_interruptible_all(&wq->wait);

		kfree(wq);
	}
}


static inline int localworkqueue_thread(void *arg)
{
	struct localworkqueue *wq = (struct localworkqueue *)arg;
	unsigned long flags;

#if USE_FIFO_SCHDULE
	struct sched_param param = {.sched_priority = THREAD_PRIORITY};
	param.sched_priority += wq->priority;
	sched_setscheduler(current, SCHED_FIFO, &param);
#else
	set_user_nice(current, THREAD_NICE - wq->priority);
#endif

	DBGENTER("\n");

	/* dev->th_events already initialized 0. */
	while (!kthread_should_stop()) {
		struct localwork *work = NULL;
		void   (*func)(struct localwork *);

		wait_event_interruptible(wq->wait, !list_empty(&wq->top));

		/* ignore all signal */
		if (signal_pending(current))
			flush_signals(current);

		if (kthread_should_stop())
			break;

		printk_dbg2(3, "spinlock\n");
		spin_lock_irqsave(&wq->lock, flags);
		while (!list_empty(&wq->top)) {
			work = list_first_entry(&wq->top,
				struct localwork, link);

			printk_dbg2(3, "work:%p\n", work);

			func = work->func;
			spin_unlock_irqrestore(&wq->lock, flags);

			(*func)(work);

			spin_lock_irqsave(&wq->lock, flags);
			work->status = true;
			list_del_init(&work->link);
			wake_up_all(&wq->finish);
		}
		spin_unlock_irqrestore(&wq->lock, flags);
	}

	DBGLEAVE("\n");
	return 0;
}


static struct localworkqueue *localworkqueue_create(char *taskname,
	int priority)
{
	struct localworkqueue *wq;

	wq = kmalloc(sizeof(*wq), GFP_KERNEL);
	if (wq == NULL) {
		/* report error */
		printk_err("can not create localwork.\n");
	} else {
		memset(wq, 0, sizeof(*wq));

		INIT_LIST_HEAD(&wq->top);
		spin_lock_init(&wq->lock);
		init_waitqueue_head(&wq->wait);
		init_waitqueue_head(&wq->finish);
		wq->priority = priority;

		wq->task = kthread_run(localworkqueue_thread,
				     wq,
				     taskname);
		if (IS_ERR(wq->task)) {
			printk_err("could not create kernel thread\n");
			kfree(wq);
			wq = NULL;
		}
	}
	return wq;
}


static int localwork_queue(
	struct localworkqueue *wq, struct localwork *work)
{
	unsigned long flags;
	int rc;
	DBGENTER("wq:%p work:%p\n", wq, work);
	if (wq && work) {
		rc = true;

		spin_lock_irqsave(&wq->lock, flags);
		if (list_empty(&work->link)) {
			list_add_tail(&work->link, &wq->top);
			work->status = false;
		} else {
			printk_err2("work %p alredy queued.\n", work);
			rc = false;
		}
		spin_unlock_irqrestore(&wq->lock, flags);

		if (rc)
			wake_up_interruptible(&wq->wait);
	} else {
		/* set error code */
		printk_err("invalid argument.\n");
		rc = false;
	}
	DBGLEAVE("%d\n", rc);
	return rc;
}

static void localwork_flush(
	struct localworkqueue *wq, struct localwork *work)
{
	unsigned long flags;
	int rc = 0;
	DBGENTER("wq:%p work:%p\n", wq, work);
	if (wq && work) {
		int wait = false;
		spin_lock_irqsave(&wq->lock, flags);
		if (work->status) {
			/* wait is not necessary. */
			printk_dbg2(3, "work %p finished.\n", work);
		} else if (list_empty(&work->link)) {
			/* report error */
			printk_dbg2(3, "work %p not queued\n", work);
			rc = -EINVAL;
		} else if (current == wq->task) {
			/* report error */
			printk_err2("can not wait in sameworkqueue\n");
			rc = -EINVAL;
		} else
			wait = true;
		spin_unlock_irqrestore(&wq->lock, flags);

		if (wait) {
			printk_dbg2(3, "wait complete of work %p\n", work);
			wait_event( \
				wq->finish, work->status != false);
		}
	} else {
		/* set error code */
		printk_err("invalid argument.\n");
		rc = -EINVAL;
	}
	DBGLEAVE("%d\n", rc);
	return;
}

