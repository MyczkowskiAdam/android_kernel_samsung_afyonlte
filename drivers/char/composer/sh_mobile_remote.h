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
#ifndef _SH_MOBILE_COMPOSER_REMOTE_H
#define _SH_MOBILE_COMPOSER_REMOTE_H

#include "../composer/sh_mobile_work.h"
#include <linux/list.h>

/******************************/
/* define structure           */
/******************************/

/* request remote function handle. */
struct composer_indirectcall {
	struct localwork               wqtask;
	int    (*remote)(unsigned long *args);
	unsigned long                  args[4];
	struct list_head               list;
};

/******************************/
/* define                     */
/******************************/

/******************************/
/* define external function   */
/******************************/
static void indirect_call_init(void);

static int indirect_call(struct localworkqueue *queue,
	int (*func)(unsigned long *args),
	int num_arg,
	unsigned long *args);

#endif
