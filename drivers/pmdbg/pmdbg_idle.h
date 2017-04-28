/*
 * pmdbg_idle.h
 *
 * Copyright (C) 2011-2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __TST_PM_IDLE___
#define __TST_PM_IDLE___

#include "pmdbg_api.h"
#include <linux/string.h>
#include <linux/parser.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <mach/pm.h>
#include <linux/wakelock.h>


#define NO_LAST_ITEM	10

struct idle_info {
	int state;
	int start_us;
	int idle_time;
	int is_active;
};


static unsigned int idle_confirm_cb(void);
static unsigned int idle_notify_cb(int state);
static void get_info(char *buf);

static int monitor_cmd(char *, int);
static int suppress_cmd(char *, int);
static int wakelock_cmd(char *, int);


#ifdef CONFIG_ARCH_R8A7373
extern int has_wake_lock_no_expire(int type);
#endif /*CONFIG_ARCH_R8A7373*/

static int idle_init(void);
static void idle_exit(void);
static void idle_show(char **);

#endif /*__TST_PM_IDLE___*/
