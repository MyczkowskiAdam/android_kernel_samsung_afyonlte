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
#ifndef _SH_MOBILE_COMPOSER_SWFENCE_H
#define _SH_MOBILE_COMPOSER_SWFENCE_H

#include <linux/sh_mobile_composer.h>

#ifdef CONFIG_SYNC
#include <linux/sync.h>
#endif
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
#include <linux/sw_sync.h>
#endif


/******************************/
/* define                     */
/******************************/
#define SYNC_FD_TYPE_BLIT  0
#define SYNC_FD_TYPE_V4L2  1

/******************************/
/* define structure           */
/******************************/
struct sw_fence_handle {
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	struct sw_sync_timeline  *timeline;
#endif
	int                      timeline_count;
	int                      timeline_inc;
};

/******************************/
/* define external function   */
/******************************/
#ifdef CONFIG_SYNC
/* make ready to use sync object */
static int fence_config(struct composer_rh *rh, struct cmp_postdata *post);

/* wait signal of sync object */
static int fence_wait(struct composer_rh *rh, int timeout, int usage);

/* release sync object */
static int fence_expire(struct composer_rh *rh);

#endif

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
/* interface of get and release API entry point */
static void   fence_get_api(void);
static void   fence_release_api(void);

/* interface of handle sw_fence.  */
static struct sw_fence_handle *fence_get_handle(void);

static void   fence_put_handle(struct sw_fence_handle *handle);

/* get sync fd */
static int fence_get_syncfd(struct sw_fence_handle *handle, int type);

/* increment timeline */
static int fence_inc_timeline(struct sw_fence_handle *handle);

#ifdef CONFIG_HAS_EARLYSUSPEND
/* reset timeline */
static int fence_reset_timeline(struct sw_fence_handle *handle);
#endif

/* assert signal */
static int fence_signal(struct sw_fence_handle *handle);
#endif

#endif
