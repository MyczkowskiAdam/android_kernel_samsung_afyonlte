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
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#include "../composer/sh_mobile_remote.h"
#include "../composer/sh_mobile_debug.h"

/******************************************************/
/* define prototype                                   */
/******************************************************/
static void work_indirectcall(struct localwork *work);

/******************************************************/
/* define local define                                */
/******************************************************/
/* this module uses follow process */
/* - alloc/ free memory for HDMI   */
/* - alloc/ free memory for LCD    */
/* - create blend handle           */
/* - create output handle          */

#define MAX_REMOTE_TASK  8

/******************************************************/
/* define local variables                             */
/******************************************************/
static struct composer_indirectcall __request_indirect_call[MAX_REMOTE_TASK];
static LIST_HEAD(__free_entry_indirect_call);
static spinlock_t __lock_indirect_call;

/******************************************************/
/* local functions                                    */
/******************************************************/

static void indirect_call_init(void)
{
	int i;
	struct composer_indirectcall *rr;
	DBGENTER("\n");

	spin_lock_init(&__lock_indirect_call);
	INIT_LIST_HEAD(&__free_entry_indirect_call);

	for (i = 0; i < MAX_REMOTE_TASK; i++) {
		rr = &__request_indirect_call[i];
		localwork_init(&rr->wqtask, work_indirectcall);
		INIT_LIST_HEAD(&rr->list);

		list_add_tail(&rr->list,
			&__free_entry_indirect_call);
	}
	DBGLEAVE("\n");
}

static void work_indirectcall(struct localwork *work)
{
	struct composer_indirectcall *rr;

	DBGENTER("work:%p\n", work);

	rr = container_of(work, struct composer_indirectcall, wqtask);

	printk_dbg2(3, "rr->function:%p rr->args:0x%lx,0x%lx\n",
		rr->remote, rr->args[0], rr->args[1]);

	if (rr->remote) {
		/* do remote function call */
		rr->args[0] = rr->remote(&rr->args[0]);
	}
	DBGLEAVE("\n");
}

static int indirect_call(struct localworkqueue *queue,
	int (*func)(unsigned long *args),
	int num_arg,
	unsigned long *args)
{
	struct composer_indirectcall *rr = NULL;
	int rc = -1;
	int i;

	DBGENTER("queue:%p func:%p num:%d args:%p\n",
		queue, func, num_arg, args);

	if (num_arg > 4) {
		printk_err("invalid argument\n");
		goto err;
	}

	/* get free entry */
	spin_lock(&__lock_indirect_call);
	if (!list_empty(&__free_entry_indirect_call)) {
		rr = list_first_entry(&__free_entry_indirect_call,
			struct composer_indirectcall, list);
		list_del_init(&rr->list);
	}
	spin_unlock(&__lock_indirect_call);

	if (!rr) {
		printk_err("no available entry\n");
		goto err;
	}

	rr->remote = func;
	for (i = 0; i < num_arg; i++)
		rr->args[i] = args[i];

	rc = localwork_queue(queue, &rr->wqtask);
	if (rc) {
		/* wait work compete. */
		localwork_flush(queue, &rr->wqtask);

		rc = rr->args[0];
	} else {
		printk_err("failed to call function\n");
		rc = -1;
	}

	/* add free entry */
	spin_lock(&__lock_indirect_call);
	list_add_tail(&rr->list,
		&__free_entry_indirect_call);
	spin_unlock(&__lock_indirect_call);
err:
	DBGLEAVE("rc:%d\n", rc);
	return rc;
}
