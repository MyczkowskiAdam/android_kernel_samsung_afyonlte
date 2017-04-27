/*
 * pmdbg_suspend.h
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

#ifndef __TST_PM_SUSPEND___
#define __TST_PM_SUSPEND___

#include "pmdbg_api.h"
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <mach/pm.h>
#include <mach/r8a7373.h>
#include <linux/wakelock.h>

#define SBSC_Freq_APE		SHARED_AREA_SBSC_START_PHY
#define SBSC_Freq_BB		(SHARED_AREA_SBSC_START_PHY + 0x4)

#ifdef CONFIG_ARCH_R8A7373
extern int has_wake_lock_no_expire(int type);
#endif /*CONFIG_ARCH_R8A7373*/

static int suspend_cmd(char *, int);
static int wakelock_cmd(char *, int);
static int enable_dump_suspend_cmd(char *, int);
static int disable_dump_suspend_cmd(char *, int);
static int suspend_init(void);
static void suspend_exit(void);

#ifdef CONFIG_SUSPEND
extern int suspend_devices_and_enter(suspend_state_t state);
#else /*!CONFIG_SUSPEND*/
static inline int suspend_devices_and_enter(suspend_state_t state)
{
	return 0;
}
#endif /*CONFIG_SUSPEND*/

static void CPG_dump_suspend(void);
static void SYSC_dump_suspend(void);
static void GPIO_dump_suspend(void);
static void Other_dump_suspend(void);

#endif /*__TST_PM_SUSPEND___*/
