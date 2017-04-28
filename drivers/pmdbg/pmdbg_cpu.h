/*
 * pmdbg_cpu.h
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
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
#ifndef __TST_PM_CPU__
#define __TST_PM_CPU__

#include <linux/string.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/cpufreq.h>

#define MON_PERIOD		11
#define MON_DPERIOD		1
#define MON_WAIT_TIME	30000

#define CPG_BASE_VA	IO_ADDRESS(0xE6150000)
#define CPG_FRQCRB	(CPG_BASE_VA + 0x0004)
#define CPG_PLL0CR	(CPG_BASE_VA + 0x00D8)
#define CPG_SCGCR	(CPG_BASE_VA + 0x01b4)

#define FRQCRB_ZSEL_MASK		0x10000000
#define FRQCRB_ZFC_MASK			0x0F000000
#define FRQCRB_BIT24_SHIFT		24
#define ZFC_RATIO_1_1			0x00000000

#define PLLCR_STC_MASK			0x3F000000
#define PLLCR_BIT24_SHIFT		24

#define SCGCR_MANUAL_MASK		0xC0000000
#define SCGCR_CKMODE_MASK		0x000C0000
#define SCGCR_WAVEFORM_MASK		0x0000FFFF
#define SCGCR_ENABLE_MCLK		0xC0000000
#define SCGCR_GATING_MODE		0x00000000
#define SCGCR_GENERATE_MODE		0x00040000

static int percent100(cputime64_t val, cputime64_t period);
static int getfreq(void);

static int enable_cpu_profile_cmd(char *, int);
static int disable_cpu_profile_cmd(char *, int);

static int pmdbg_cpu_init(void);
static void pmdbg_cpu_exit(void);

#endif /*__TST_PM_CPU__*/
