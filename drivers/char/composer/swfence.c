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
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/file.h>
#include <linux/module.h>

#if FEATURE_HDMI_WORKQUEUE
#include <linux/sem.h>
#endif

#include "../composer/sh_mobile_debug.h"
#include "../composer/sh_mobile_swfence.h"

/******************************************************/
/* define prototype                                   */
/******************************************************/
#define MAX_TIMELINE_COUNT  4

/******************************************************/
/* define local define                                */
/******************************************************/

/******************************************************/
/* define local variables                             */
/******************************************************/
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
static struct module *sw_sync_api_owner;
static struct sw_sync_timeline *(*do_sw_sync_timeline_create)(const char *name);
static void (*do_sw_sync_timeline_inc)(struct sw_sync_timeline *obj, u32 inc);
static struct sync_pt *(*do_sw_sync_pt_create)(
	struct sw_sync_timeline *obj, u32 value);
#endif

/******************************************************/
/* local functions                                    */
/******************************************************/
#ifdef CONFIG_SYNC
static int fence_config(struct composer_rh *rh, struct cmp_postdata *post)
{
	int rc = CMP_OK;
	int i;
	int fd;
	struct sync_fence *fence;

	DBGENTER("rh:%p post:%p\n", rh, post);

	/* set all handle NULL */
	memset(&rh->buffer_sync,    0, sizeof(rh->buffer_sync));

	/* set information */
	for (i = 0; i < post->num_buffer; i++) {
		fd = post->acqure_fd[i];

		if (fd >= 0)
			fence = sync_fence_fdget(fd);
		else
			fence = NULL;

		rh->buffer_sync[i] = fence;
	}

	DBGLEAVE("%d\n", rc);
	return rc;
}

static int fence_expire(struct composer_rh *rh)
{
	int i;
	struct sync_fence *fence;

	DBGENTER("rh:%p\n", rh);

	/* all sync object released without wait. */
	printk_dbg2(3, "all sync handle released.\n");

	for (i = 0; i < rh->num_buffer; i++) {
		fence = rh->buffer_sync[i];
		if (fence) {
			printk_err("close without fence_wait.");
			sync_fence_put(fence);
			rh->buffer_sync[i] = NULL;
		}
	}
	return CMP_OK;
}

static int fence_wait(struct composer_rh *rh, int timeout, int usage)
{
	int rc = CMP_OK;
	int i;
	struct sync_fence *fence;

	DBGENTER("rh:%p timeout:%d usage:%d\n", rh, timeout, usage);

	/* wait sync_fence signaled. */
	for (i = 0; i < rh->num_buffer; i++) {
		if (rh->buffer_usage[i] & usage) {
			fence = rh->buffer_sync[i];
			if (fence) {
				if (sync_fence_wait(fence, timeout) < 0) {
					printk_err("sync_wait error.");
					break;
				}
			}
		}
	}

	/* close sync_fence. */
	for (i = 0; i < rh->num_buffer; i++) {
		if (rh->buffer_usage[i] & usage) {
			fence = rh->buffer_sync[i];
			if (fence) {
				sync_fence_put(fence);
				rh->buffer_sync[i] = NULL;
			}
		}
	}
	DBGLEAVE("%d\n", rc);
	return rc;
}
#endif

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API

/* interface of get and release API entry point */
static void   fence_get_api(void)
{
	struct module *owner[3] = { NULL, NULL, NULL };
	const struct kernel_symbol *sym;
	int rc = 0;

	do_sw_sync_timeline_create = NULL;
	do_sw_sync_timeline_inc = NULL;
	do_sw_sync_pt_create = NULL;

	/* disable preempt */
	preempt_disable();

	sym = find_symbol("sw_sync_timeline_create", &owner[0],
		NULL, true, false);
	if (sym) {
		do_sw_sync_timeline_create = \
			(struct sw_sync_timeline *(*)(const char *name)) \
			sym->value;

		sw_sync_api_owner = owner[0];
	} else {
		printk_err2("symbol sw_sync_timeline_create not found.\n");
		rc = -1;
	}

	sym = find_symbol("sw_sync_timeline_inc", &owner[1],
		NULL, true, false);
	if (sym) {
		do_sw_sync_timeline_inc = \
			(void (*)(struct sw_sync_timeline *obj, u32 inc)) \
			sym->value;
		if (owner[0] != owner[1]) {
			/* report error */
			printk_err("symbol owner different.\n");
			rc = -1;
		}
	} else {
		printk_err2("symbol sw_sync_timeline_inc not found.\n");
		rc = -1;
	}

	sym = find_symbol("sw_sync_pt_create", &owner[1],
		NULL, true, false);
	if (sym) {
		do_sw_sync_pt_create = \
			(struct sync_pt *(*)(struct sw_sync_timeline *obj, \
			u32 value)) sym->value;
		if (owner[0] != owner[1]) {
			/* report error */
			printk_err("symbol owner different.\n");
			rc = -1;
		}
	} else {
		printk_err2("symbol sw_sync_pt_create not found.\n");
		rc = -1;
	}

	/* enable preempt */
	preempt_enable();

	if (rc) {
		/* handle error case */
		fence_release_api();
	}
}
static void   fence_release_api(void)
{
	if (do_sw_sync_timeline_create) {
		do_sw_sync_timeline_create = NULL;
		module_put(sw_sync_api_owner);
	}
	if (do_sw_sync_timeline_inc) {
		do_sw_sync_timeline_inc = NULL;
		module_put(sw_sync_api_owner);
	}
	if (do_sw_sync_pt_create) {
		do_sw_sync_pt_create = NULL;
		module_put(sw_sync_api_owner);
	}
	sw_sync_api_owner = NULL;
}



static struct sw_fence_handle *fence_get_handle(void)
{
	struct sw_fence_handle *handle;

	DBGENTER("\n");

	handle = kmalloc(sizeof(struct sw_fence_handle), GFP_KERNEL);
	if (handle == NULL) {
		/* no memory */
		printk_err2("memory allocation failed.\n");
	} else if (do_sw_sync_timeline_create == NULL) {
		/* no API available */
		printk_err2("sw_sync_timeline_create not available.\n");

		kfree(handle);
		handle = NULL;
	} else {
		handle->timeline = do_sw_sync_timeline_create("composer_sync");
		handle->timeline_count = 0; /* count of increment */
		handle->timeline_inc   = 0; /* count of signal    */
	}

	DBGLEAVE("%p\n", handle);
	return handle;
}

static void  fence_put_handle(struct sw_fence_handle *handle)
{
	DBGENTER("handle:%p\n", handle);

	if (handle) {
#ifdef CONFIG_SYNC
		if (handle->timeline) {
			sync_timeline_destroy(&handle->timeline->obj);
			handle->timeline = NULL;
		}
#endif
		kfree(handle);
	}
	DBGLEAVE("\n");
}

static int fence_get_syncfd(struct sw_fence_handle *handle, int type)
{
	int sync_fd = -1;
	struct sw_sync_timeline  *timeline;
	int                      timeline_count;

	DBGENTER("handle:%p type:%d\n", handle, type);

	if (handle == NULL) {
		printk_dbg1(3, "handle is NULL.\n");
		goto err;
	}
#if _LOG_DBG > 1
	if (type != SYNC_FD_TYPE_BLIT &&
		type != SYNC_FD_TYPE_V4L2) {
		printk_dbg2(3, "invalid type.\n");
		goto err;
	}
#endif
#if _LOG_DBG > 1
	if (do_sw_sync_pt_create == NULL) {
		printk_err("fence_get_handle should return error.\n");
		goto err;
	}
#endif
	timeline       = handle->timeline;
	timeline_count = handle->timeline_count + type;

	if ((timeline_count - handle->timeline_inc) > MAX_TIMELINE_COUNT) {
		/* previous fence has not signaled. */
		/* abort to create fence fd         */
		printk_err2("abort create sync fd.\n");
	} else if (timeline) {
		struct sync_pt *pt = NULL;
		int rc = -EINVAL;
		int fd = -1;

#ifdef CONFIG_SYNC
		do {
			struct sync_fence *fence = NULL;

			fd = get_unused_fd();
			if (fd < 0) {
				printk_dbg2(3, "get_unused_fd failed\n");
				break;
			}

			pt = do_sw_sync_pt_create(timeline, timeline_count);
			if (pt == NULL) {
				printk_dbg2(3, "sw_sync_pt_create failed\n");
				rc = -ENOMEM;
				break;
			}

			fence = sync_fence_create("composer_fence", pt);
			if (fence == NULL) {
				printk_dbg2(3, "sync_fence_create failed\n");
				rc = -ENOMEM;
				break;
			}

			sync_fence_install(fence, fd);

			rc = 0;
			sync_fd = fd;
		} while (0);
#endif

		if (rc) {
			/* free_resource */
			printk_dbg1(3, "iocg_sw_sync failed\n");

			if (pt) {
				/* free sw_sync_pt_create */
				sync_pt_free(pt);
			}
			if (fd >= 0) {
				/* free get_unused_fd */
				put_unused_fd(fd);
			}
		}
	}
err:

	DBGLEAVE("%d\n", sync_fd);
	return sync_fd;
}

static int fence_inc_timeline(struct sw_fence_handle *handle)
{
	if (handle) {
		struct sw_sync_timeline  *timeline = handle->timeline;
		unsigned long flags;

		spin_lock_irqsave(&timeline->obj.child_list_lock, flags);
		handle->timeline_count++;
		spin_unlock_irqrestore(&timeline->obj.child_list_lock, flags);
	}

	return CMP_OK;
}

static int fence_signal(struct sw_fence_handle *handle)
{
	struct sw_sync_timeline  *timeline;
	int timeline_count;
	int timeline_inc;

	DBGENTER("handle:%p\n", handle);

	if (handle == NULL) {
		printk_dbg1(3, "handle is NULL.\n");
		goto err;
	}

	timeline       = handle->timeline;
	timeline_count = handle->timeline_count;
	timeline_inc   = handle->timeline_inc;

	if (timeline) {
#if _LOG_DBG > 1
		if (do_sw_sync_timeline_inc == NULL) {
			printk_err("fence_get_handle should return error.\n");
			goto err;
		}
#endif
		do_sw_sync_timeline_inc(timeline, 1);

		handle->timeline_inc++;
		timeline_inc   = handle->timeline_inc;
	}

	printk_dbg1(3, "sw_sync pt_value:%d, value:%d\n",
		timeline_count, timeline_inc);
err:

	DBGLEAVE("\n");
	return CMP_OK;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
/* reset timeline */
static int fence_reset_timeline(struct sw_fence_handle *handle)
{
	DBGENTER("handle:%p\n", handle);

	if (handle == NULL) {
		printk_dbg1(3, "handle is NULL.\n");
		goto err;
	}

	if (handle->timeline_inc != handle->timeline_count) {
		/* report error */
		printk_err("invalid condition\n");
		printk_dbg1(3, "sw_sync pt_value:%d, value:%dms\n",
			handle->timeline_count, handle->timeline_inc);
	} else {
		/* force signalled for SYNC_FD_TYPE_V4L2 */
		fence_inc_timeline(handle);
		fence_signal(handle);
	}
err:

	DBGLEAVE("\n");
	return CMP_OK;
}
#endif

#endif

/******************************************************/
/* global functions                                   */
/******************************************************/

