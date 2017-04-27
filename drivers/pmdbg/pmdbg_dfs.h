/*
 * pmdbg_dfs.h
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
#ifndef __TST_PM_DFS__
#define __TST_PM_DFS__

#include "pmdbg_api.h"
#include <linux/string.h>
#include <linux/parser.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <mach/pm.h>
#include <linux/jiffies.h>
#include <linux/cpufreq.h>

#define NO_MON_ITEM		10


#define PLLCR_STC_MASK			0x3F000000
#define PLLCR_BIT24_SHIFT		24

#define FRQCRD_ZB30SEL			BIT(4)
#define KICK_WAIT_INTERVAL_US	500

#define HW_TO_DIV(reg, clk)	\
	((reg >> __clk_hw_info[clk].shift_bit) & \
	__clk_hw_info[clk].mask_bit)


struct clk_hw_info {
	unsigned int	mask_bit;
	unsigned int	shift_bit;
	int				div_val[16];
	void __iomem	*addr;
};

#ifdef CONFIG_CPU_FREQ
extern unsigned int pm_get_syscpu_frequency(void);
#else /*!CONFIG_CPU_FREQ*/
static unsigned int pm_get_syscpu_frequency(void);
static int cpg_get_pll(int pll);
static int cpg_get_freqval(int clk, int *div);
#endif /*CONFIG_CPU_FREQ*/
static inline int __div(enum clk_div c_div);
/* static int cpg_get_freq(struct clk_rate *rates); */
static inline int __match_div_rate(int clk, int val);
static int transition_notifier_cb(struct notifier_block *,
		unsigned long, void *);
static int start_monitor(void);
static int stop_monitor(void);
static int start_dfs_cmd(char *, int);
static int stop_dfs_cmd(char *, int);
static int enable_dfs_cmd(char *, int);
static int disable_dfs_cmd(char *, int);
static int suppress_cmd(char *, int);
static int clk_get_cmd(char *, int);
static int monitor_cmd(char *, int);

static void dfs_show(char **);
static int dfs_init(void);
static void dfs_exit(void);


#endif /*__TST_PM_DFS__*/
