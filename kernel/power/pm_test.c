/*
 * kernel/power/pm_test.c
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "power.h"
#include <linux/module.h>

/* 
 * Offer files to turn on/off below functions
 *   Serial console 
 *     0: no_console_suspend, 1:console_suspend
 *   Wakelock checking 
 *     0: don't ignore wakelocks, 1: ignore wakelocks
 *   Call late resume immediately after resume
 *     0: don't call late resume
 *     1: call late resume
 *     Set wait time before resuming (s)
 *   EXTAL 1 log 
 *     0: don't display log
 *     1: display log
 */
extern int console_suspend_enabled;
int ignore_wakelock = 0;
EXPORT_SYMBOL(ignore_wakelock);
int for_kernel_test = 0;
EXPORT_SYMBOL(for_kernel_test);
int wait_time = 5;		/* default is 5s */
EXPORT_SYMBOL(wait_time);
int xtal1_log = 0;
EXPORT_SYMBOL(xtal1_log);

module_param_named(console_suspend, console_suspend_enabled, int, \
					S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(ignore_wakelocks, ignore_wakelock, int, \
					S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(for_kernel_test, for_kernel_test, int, \
					S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(wait_time, wait_time, int, \
					S_IRUGO | S_IWUSR | S_IWGRP);
module_param_named(xtal1_log, xtal1_log, int, \
					S_IRUGO | S_IWUSR | S_IWGRP);

