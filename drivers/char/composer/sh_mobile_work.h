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
#ifndef _SH_MOBILE_COMPOSER_WORK_H
#define _SH_MOBILE_COMPOSER_WORK_H

#include <linux/sh_mobile_composer.h>

/******************************/
/* define                     */
/******************************/

/******************************/
/* define structure           */
/******************************/

/******************************/
/* define external function   */
/******************************/
/* create workqueue with name */
static struct localworkqueue *localworkqueue_create(char *name, int priority);

/* flush workqueue */
static void localworkqueue_flush(struct localworkqueue *wq);

/* destroy workqueue */
static void localworkqueue_destroy(struct localworkqueue *wq);

/* add task */
static int  localwork_queue(struct localworkqueue *wq,
	struct localwork *work);

/* wait complete task */
static void localwork_flush(struct localworkqueue *wq,
	struct localwork *work);

/* task initialize */
static void localwork_init(struct localwork *work,
	void (*func)(struct localwork *));

#endif
