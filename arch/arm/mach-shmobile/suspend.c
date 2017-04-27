/*
 * Suspend-to-RAM support code for SH-Mobile ARM
 * arch/arm/mach-shmobile/suspend.c
 *
 * Copyright (C) 2011 Magnus Damm
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
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <mach/pm.h>
#include <asm/system.h>
#include <linux/delay.h>
#include <linux/hwspinlock.h>
/* #include <mach/sbsc.h> */
#include <linux/wakelock.h>
#ifndef CONFIG_PM_HAS_SECURE
#include "pm_ram0.h"
#else /*CONFIG_PM_HAS_SECURE*/
#include "pm_ram0_tz.h"
#endif /*CONFIG_PM_HAS_SECURE*/
#include "pmRegisterDef.h"

#ifdef CONFIG_SHMOBILE_RAM_DEFRAG
#include <mach/ram_defrag.h>
#endif /* CONFIG_SHMOBILE_RAM_DEFRAG */
#include <memlog/memlog.h>

#define pm_writeb(v, a)			__raw_writeb(v, a)
#define pm_writew(v, a)			__raw_writew(v, a)
#define pm_writel(v, a)			__raw_writel(v, a)
#define pm_readb(a)				__raw_readb(a)
#define pm_readw(a)				__raw_readw(a)
#define pm_readl(a)				__raw_readl(a)
#define DO_SAVE_REGS(array)		do_save_regs(array, ARRAY_SIZE(array))
#define DO_RESTORE_REGS(array)	do_restore_regs(array, ARRAY_SIZE(array))

#ifndef CONFIG_PM_HAS_SECURE
#define RAM_ARM_VECT                   secramBasePhys
#else /*CONFIG_PM_HAS_SECURE*/
#define RAM_ARM_VECT                   ram0ArmVectorPhys
#endif /*CONFIG_PM_HAS_SECURE*/

#define PMDBG_PRFX				"PM-DBG: "

#define CPG_SET_FREQ_MAX_RETRY (10000)	/* 10ms */

enum {
	IRQC_EVENTDETECTOR_BLK0 = 0,
	IRQC_EVENTDETECTOR_BLK1,
	IRQC_EVENTDETECTOR_BLK10,
	IRQC_EVENTDETECTOR_BLK11,
	IRQC_EVENTDETECTOR_BLK12,
	HSGPR,
	SYSGPR,
	HPB,
	SHWYSTATHS,
	SHWYSTATSY,
	SHWYSTATDM,
	SHBUF,
#ifdef CONFIG_PM_HAS_SECURE
	SBSC_SDCR0A_TZ,
	SBSC_SDWCRC0A_TZ,
	SBSC_SDWCRC1A_TZ,
	SBSC_SDWCR00A_TZ,
	SBSC_SDWCR01A_TZ,
	SBSC_SDWCR10A_TZ,
	SBSC_SDWCR11A_TZ,
	SBSC_SDWCRC2A_TZ,
#endif
};

static suspend_state_t shmobile_suspend_state;
static int not_core_shutdown;

static unsigned int save_sbar_val;

static int log_wakeupfactor;
module_param(log_wakeupfactor, int, S_IRUGO | S_IWUSR | S_IWGRP);

static int es;

static char xtal1_log_out;

/*Change clocks function*/
unsigned int frqcrA_save;
unsigned int frqcrB_save;
unsigned int frqcrD_save;
#define CLOCK_SUSPEND		0
#define CLOCK_RESTORE		1
#define ZB3_CLK_SUSPEND		0

#define I2C_ICCRDVM_DUMMY_READ_LOOP	0x1000


#ifdef CONFIG_PM_DEBUG
/*
 * Dynamic on/off for System Suspend
 *   0: System Suspend is disable
 *   1: System Suspend in enable
 */
static int enable_module = 1;
static DEFINE_SPINLOCK(systemsuspend_lock);
#endif	/* CONFIG_PM_DEBUG */
struct base_map {
	unsigned long phys;	/* phys base  */
	int size;			/* remap size */
	void __iomem *base;	/* virt base  */
};

struct reg_info {
	void __iomem **vbase;
	unsigned long offset;
	int size;
	int esrev;
	unsigned int val;
};

#define PM_SAVE_REG(type, of, sz, rev)	\
{								\
	.vbase  = &map[type].base,	\
	.offset = of,				\
	.size   = sz,				\
	.esrev  = rev,				\
}

static struct base_map map[] = {
	[IRQC_EVENTDETECTOR_BLK0] = {
		.size = SZ_4K,
		.base = IRQC_EVENTDETECTOR_BLK0_BASE,
	},
	[IRQC_EVENTDETECTOR_BLK1] = {
		.size = SZ_4K,
		.base = IRQC_EVENTDETECTOR_BLK1_BASE,
	},
	[IRQC_EVENTDETECTOR_BLK10] = {
		.size = SZ_4K,
		.base = IRQC_EVENTDETECTOR_BLK10_BASE,
	},
	[IRQC_EVENTDETECTOR_BLK11] = {
		.size = SZ_4K,
		.base = IRQC_EVENTDETECTOR_BLK11_BASE,
	},
	[IRQC_EVENTDETECTOR_BLK12] = {
		.size = SZ_4K,
		.base = IRQC_EVENTDETECTOR_BLK12_BASE,
	},
	[HSGPR] = {
		.phys = HSGPR_BASE_PHYS,
		.size = SZ_4K,
	},
	[SYSGPR] = {
		.phys = SYSGPR_BASE_PHYS,
		.size = SZ_4K,
	},
	[HPB] = {
		.size = SZ_8K,
		.base = HPB_BASE,
	},
	[SHWYSTATHS] = {
		.size = SZ_4K,
		.base = SHWYSTATHS_BASE,
	},
	[SHWYSTATSY] = {
		.size = SZ_4K,
		.base = SHWYSTATSY_BASE,
	},
	[SHWYSTATDM] = {
		.phys = SHWYSTATDM_BASE_PHYS,
		.size = SZ_4K,
	},
	[SHBUF] = {
		.size = SZ_4K,
		.base = SHBUF_BASE,
	},
#ifdef CONFIG_PM_HAS_SECURE
	[SBSC_SDCR0A_TZ] = {	/* for setclock */
		.phys = SBSC_SDCR0A_PHYS,
		.size = SZ_4,	/* 4 bytes */
	},

	[SBSC_SDWCRC0A_TZ] = {	/* for setclock */
		.phys = SBSC_SDWCRC0A_PHYS,
		.size = SZ_4,	/* 4 bytes */
	},
	[SBSC_SDWCRC1A_TZ] = {	/* for setclock */
		.phys = SBSC_SDWCRC1A_PHYS,
		.size = SZ_4,	/* 4 bytes */
	},
	[SBSC_SDWCR00A_TZ] = {	/* for setclock */
		.phys = SBSC_SDWCR00A_PHYS,
		.size = SZ_4,	/* 4 bytes */
	},
	[SBSC_SDWCR01A_TZ] = {	/* for setclock */
		.phys = SBSC_SDWCR01A_PHYS,
		.size = SZ_4,	/* 4 bytes */
	},
	[SBSC_SDWCR10A_TZ] = {	/* for setclock */
		.phys = SBSC_SDWCR10A_PHYS,
		.size = SZ_4,	/* 4 bytes */
	},
	[SBSC_SDWCR11A_TZ] = {	/* for setclock */
		.phys = SBSC_SDWCR11A_PHYS,
		.size = SZ_4,	/* 4 bytes */
	},
	[SBSC_SDWCRC2A_TZ] = {	/* for setclock */
		.phys = SBSC_SDWCRC2A_PHYS,
		.size = SZ_4,	/* 4 bytes */
	},
#endif
};

#if 0
static struct reg_info irqx_eventdetectors_regs[] = {
/* IRQC Event Detector Block_0  */
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_00,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_01,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_02,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_03,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_04,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_05,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_06,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_07,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_08,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_09,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_10,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_11,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_12,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_13,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_14,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_15,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_16,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_17,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_18,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_19,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_20,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_21,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_22,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_23,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_24,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_25,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_26,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_27,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_28,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_29,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_30,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK0, CONFIG_31,   32, ES_REV_ALL),
/* IRQC Event Detector Block_1  */
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_00,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_01,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_02,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_03,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_04,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_05,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_06,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_07,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_08,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_09,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_10,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_11,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_12,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_13,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_14,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_15,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_16,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_17,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_18,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_19,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_20,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_21,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_22,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_23,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_24,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_25,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_26,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_27,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_28,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_29,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_30,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK1, CONFIG_31,   32, ES_REV_ALL),
/* IRQC Event Detector Block_10  */
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_00,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_01,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_02,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_03,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_04,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_05,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_06,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_07,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_08,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_09,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_10,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_11,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_12,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_13,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_14,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_15,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_16,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_17,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_18,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_19,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_20,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_21,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_22,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_23,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_24,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_25,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_26,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_27,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_28,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_29,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_30,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK10, CONFIG_31,   32, ES_REV_ALL),
/* IRQC Event Detector Block_11  */
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_00,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_01,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_02,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_03,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_04,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_05,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_06,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_07,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_08,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_09,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_10,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_11,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_12,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_13,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_14,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_15,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_16,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_17,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_18,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_19,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_20,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_21,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_22,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_23,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_24,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_25,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_26,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_27,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_28,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_29,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_30,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK11, CONFIG_31,   32, ES_REV_ALL),
/* IRQC Event Detector Block_12  */
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_00,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_01,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_02,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_03,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_04,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_05,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_06,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_07,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_08,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_09,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_10,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_11,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_12,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_13,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_14,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_15,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_16,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_17,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_18,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_19,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_20,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_21,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_22,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_23,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_24,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_25,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_26,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_27,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_28,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_29,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_30,   32, ES_REV_ALL),
	PM_SAVE_REG(IRQC_EVENTDETECTOR_BLK12, CONFIG_31,   32, ES_REV_ALL),
};
#endif

static struct reg_info shwy_regs[] = {
/* SHBUF */
	PM_SAVE_REG(SHBUF, SHBMCTR,    32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBMAR,     32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBARCR11,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBARCR12,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBMCTR2,   32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBADDR00,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBMSKR00,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBCHCTR00, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBSIZER00, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBADDR01,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBMSKR01,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBCHCTR01, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBSIZER01, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBADDR02,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBMSKR02,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBCHCTR02, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBSIZER02, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBADDR03,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBMSKR03,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBCHCTR03, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBSIZER03, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBADDR04,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBMSKR04,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBCHCTR04, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBSIZER04, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBADDR05,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBMSKR05,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBCHCTR05, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBSIZER05, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBADDR06,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBMSKR06,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBCHCTR06, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBSIZER06, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBADDR07,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBMSKR07,  32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBCHCTR07, 32, ES_REV_ALL),
	PM_SAVE_REG(SHBUF, SHBSIZER07, 32, ES_REV_ALL),
/* HS GPR */
	PM_SAVE_REG(HSGPR, HSPRPRICR,    32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRARCR11,   32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRARCR12,   32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRARCR13,   32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRARCR14,   32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRARCR31,   32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRARCR32,   32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRARCR33,   32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRARCR34,   32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRERRMSK,   32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRPRICNT11, 32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRPRICNT12, 32, ES_REV_ALL),
	PM_SAVE_REG(HSGPR, HSPRPRICNT13, 32, ES_REV_ALL),
/* Sys GPR */
	PM_SAVE_REG(SYSGPR, SYPRPRICR,  32, ES_REV_ALL),
	PM_SAVE_REG(SYSGPR, SYPRARCR11, 32, ES_REV_ALL),
	PM_SAVE_REG(SYSGPR, SYPRARCR12, 32, ES_REV_ALL),
	PM_SAVE_REG(SYSGPR, SYPRARCR13, 32, ES_REV_ALL),
	PM_SAVE_REG(SYSGPR, SYPRERRMSK, 32, ES_REV_ALL),
/* HPB */
	PM_SAVE_REG(HPB, HPBCTRL1,      32, ES_REV_ALL),
	PM_SAVE_REG(HPB, HPBCTRL2,      32, ES_REV_ALL),
	PM_SAVE_REG(HPB, HPBCTRL4,      32, ES_REV_ALL),
	PM_SAVE_REG(HPB, HPBCTRL5,      32, ES_REV_ALL),
	PM_SAVE_REG(HPB, HPBCTRL7,      32, ES_REV_ALL),
	PM_SAVE_REG(HPB, OCPBRGWIN1,    32, ES_REV_ALL),
	PM_SAVE_REG(HPB, OCPBRGWIN2,    32, ES_REV_ALL),
	PM_SAVE_REG(HPB, OCPBRGWIN3,    32, ES_REV_ALL),
/* SHWYSTAT HS */
	PM_SAVE_REG(SHWYSTATHS, SHSTxCR,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxIR,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxDMR,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxCNT,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxTN,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxTR,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxAM11,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxAM12,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxTM1,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxAM21,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxAM22,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxTM2,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxATRM1, 32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATHS, SHSTxATRM2, 32, ES_REV_ALL),
/* SHWYSTAT SYS */
	PM_SAVE_REG(SHWYSTATSY, SHSTxCR,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxIR,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxDMR,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxCNT,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxTN,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxTR,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxAM11,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxAM12,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxTM1,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxAM21,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxAM22,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxTM2,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxATRM1, 32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATSY, SHSTxATRM2, 32, ES_REV_ALL),
};

struct reg_info shwystatdm_regs[] = {
/* SHWYSTAT DM */
	PM_SAVE_REG(SHWYSTATDM, SHSTxCR,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxIR,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxDMR,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxCNT,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxTN,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxTR,    32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxAM11,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxAM12,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxTM1,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxAM21,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxAM22,  32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxTM2,   32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxATRM1, 32, ES_REV_ALL),
	PM_SAVE_REG(SHWYSTATDM, SHSTxATRM2, 32, ES_REV_ALL),
};

/*
 * Helper functions for system suspend
 */
static void wakeups_factor(void)
{
	unsigned int dummy;

	/* clear */
	dummy = __raw_readl(WUPSFAC);

	if (log_wakeupfactor == 1) {
		pr_debug(PMDBG_PRFX "WakeUpFactor Value = 0x%08x\n", \
					dummy);
		pr_debug(PMDBG_PRFX "WakeUpS Mask Value = 0x%08x\n", \
					__raw_readl(WUPSMSK));

		/* For IRQ0,IRQ1 wakeup factors */
		if ((dummy & 0x40) != 0)
			pr_debug(PMDBG_PRFX " Wakeup by IRQ[31:0]: 0x%08x\n", \
					__raw_readl(ram0_ICSPISR0));
		else if ((dummy & 0x80) != 0)
			pr_debug(PMDBG_PRFX " Wakeup by IRQ[63:32]: 0x%08x\n",\
					__raw_readl(ram0_ICSPISR1));
		else
			pr_debug(PMDBG_PRFX "Not wakeup by IRQ wakeup factors.\n");
	}
}

/*
 * Helper functions for getting platform suspend state
 */
suspend_state_t get_shmobile_suspend_state(void)
{
	return shmobile_suspend_state;
}

/*
 * Helper functions for saving IP registers value
 */
static void do_save_regs(struct reg_info *regs, int count)
{
	struct reg_info *info = regs;
	int i;

	for (i = 0; i < count; i++, info++) {
		if (!*info->vbase)
			continue;
		if ((info->esrev & es) == es) {
			switch (info->size) {
			case 8:
				info->val = pm_readb(*info->vbase + \
							info->offset);
				break;
			case 16:
				info->val = pm_readw(*info->vbase + \
							info->offset);
				break;
			case 32:
				info->val = pm_readl(*info->vbase + \
							info->offset);
				break;
			default:
				break;
			}
		}
	}
}

/*
 * Helper functions for restoring IP registers value
 */
static void do_restore_regs(struct reg_info *regs, int count)
{
	struct reg_info *info;
	int i;

	info = regs + count;

	for (i = count; i > 0; i--) {
		info--;
		if (!*info->vbase)
			continue;
		if ((info->esrev & es) == es) {
			switch (info->size) {
			case 8:
				pm_writeb(info->val, (*info->vbase + \
						info->offset));
				break;
			case 16:
				pm_writew(info->val, (*info->vbase + \
						info->offset));
				break;
			case 32:
				pm_writel(info->val, (*info->vbase + \
						info->offset));
				break;
			default:
				break;
			}
		}
	}
}

#if 0
static void irqx_eventdetectors_regs_save(void)
{
	DO_SAVE_REGS(irqx_eventdetectors_regs);
}
#endif

static void shwy_regs_save(void)
{
	DO_SAVE_REGS(shwy_regs);
}

void shwystatdm_regs_save(void)
{
	DO_SAVE_REGS(shwystatdm_regs);
}

#if 0
static void irqx_eventdetectors_regs_restore(void)
{
	DO_RESTORE_REGS(irqx_eventdetectors_regs);
}
#endif

static void shwy_regs_restore(void)
{
	DO_RESTORE_REGS(shwy_regs);
}

void shwystatdm_regs_restore(void)
{
	DO_RESTORE_REGS(shwystatdm_regs);
}

/*
 * Helper functions for checking CPU status
 */
static int core_shutdown_status(unsigned int cpu)
{
	return (__raw_readl(SCPUSTR) >> (4 * cpu)) & 3;
}

/*
 * System suspend callback functions' implementation
 */
static int shmobile_suspend_begin(suspend_state_t state)
{
	int ret;

	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_BEGIN, 1);
	shmobile_suspend_state = state;
	if (get_shmobile_suspend_state() & PM_SUSPEND_MEM)
		is_suspend_request = 1;

	/* set DFS mode */
	ret = suspend_cpufreq();
	if (ret != 0) {
		pr_debug(PMDBG_PRFX "%s: suspend_cpufreq() returns %d.\n", \
				__func__, ret);
	}

	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_BEGIN, 0);
	return 0;
}

static void shmobile_suspend_end(void)
{
	int ret;

	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_END, 1);
	shmobile_suspend_state = PM_SUSPEND_ON;
	is_suspend_request = 0;

	if (not_core_shutdown) {
		pr_debug(PMDBG_PRFX "%s: CPU0 waited until the" \
				"CPU1 power down.\n", __func__);
		not_core_shutdown = 0;
	}

	ret = resume_cpufreq();
	if (ret != 0) {
		pr_debug(PMDBG_PRFX "%s: resume_cpufreq() returns %d.\n", \
				__func__, ret);
	}

	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_END, 0);
}

static void do_iicdvm_setting(void)
{
	pr_debug("Setting IICDVM\n");
	__raw_writeb(0x80, ICCRDVM);
	__raw_writeb(0x00, ICTMC1DVM1);
	__raw_writeb(0x00, ICTMC2DVM1);
	__raw_writeb(0x01, ICTMCWDVM1);
	__raw_writeb(0x03, ICICDVM1);
	__raw_writeb(0x90, ICACEDVM1);
	__raw_writeb(0x00, ICIMSKDVM1);
	__raw_writeb(0x00, ICATFRDVM1);
	__raw_writeb(0x10, ICVCONDVM1);
	__raw_writeb(0x02, ICATSET1DVM1);

	pr_debug("Set IICDVM communication data\n");
	__raw_writeb(0xFE, ICATD00DVM12);
	__raw_writeb(0xFE, ICATD00DVM13);
	__raw_writeb(0xFF, ICATD01DVM12);
	__raw_writeb(0xFF, ICATD01DVM13);
	__raw_writeb(0xFF, ICATD02DVM12);
	__raw_writeb(0xFF, ICATD02DVM13);

	pr_debug("Set IICDVM start transmission\n\n");
	__raw_writeb(0x80, ICASTARTDVM1);
}

/*
 *  setting for going to Low Power Mode without I2CDVM
 */
static void set_regs_for_LPM(void)
{
	unsigned int data;

    /* DVFSCR1 [30:29:28] <= 100 */
	data = __raw_readl(DVFSCR1);
	data = data & 0xCFFFFFFF;
	data = data | 0x40000000;
	__raw_writel(data, DVFSCR1);

	/* SYSC.SYCKENMSK LPMEN=1 */
	__raw_writel(0x402F0000, SYCKENMSK);

	/* SYSC.LPMWUCNT=0000007E */
	__raw_writel(0xA550007E, LPMWUCNT);

	/* EXMSKCNT1    =0000007E */
	__raw_writel(0xA550007E, EXMSKCNT1);

	/* LPMWUMSKCNT  =00000001 */
	__raw_writel(0xA5500001, LPMWUMSKCNT);

	/* LPMR to disable I2CDVM and CPG TIMEOUT ON */
	__raw_writel(0x00800102, LPMR);

	/* disable wakeup of I2CDVM timeout */
	data = __raw_readl(WUPSMSK);
	data = data | 0x01000000;
	__raw_writel(data, WUPSMSK);

	return;
}

/*
 *  setting for leaving from Low Power Mode without I2CDVM
 */
static void reset_regs_for_LPM(void)
{
	unsigned char cdata;
	int count;

	/* SYSC.SYCKENMSK LPMEN=0 */
	__raw_writel(0x002F0000, SYCKENMSK);

	/* reset ICCRDVM */
	cdata = __raw_readb(ICCRDVM);
	cdata = cdata & 0x7F;
	__raw_writeb(cdata, ICCRDVM);

	for (count = 0; count < I2C_ICCRDVM_DUMMY_READ_LOOP; count++) {
		cdata = __raw_readb(ICCRDVM);
		if ((cdata & 0x80) == 0)
			break;
	}
	if (count >= I2C_ICCRDVM_DUMMY_READ_LOOP)
		pr_debug(PMDBG_PRFX "I2C DVM reset error\n");

	cdata = __raw_readb(ICCRDVM);
	cdata = cdata | 0x80;
	__raw_writeb(cdata, ICCRDVM);

	return;
}


/*
 * suspend_set_clock
 *
 * Arguments:
 *		@is_restore:
 *			0: set clock when suspending
 *			1: restore clock when resuming
 * Return:
 *		0: successful
 */
int suspend_set_clock(unsigned int is_restore)
{
	int clocks_ret = 0;
	int cpg_clocks_ret = 0;
	unsigned int frqcrA_suspend_clock;
	unsigned int frqcrB_suspend_clock;
	unsigned int zb3_clock;
	unsigned int frqcrA_mask;
	unsigned int frqcrB_mask;
	int i;

	frqcrA_suspend_clock = POWERDOWN_FRQCRA_ES2;
	frqcrB_suspend_clock = POWERDOWN_FRQCRB_ES2;
	zb3_clock = ZB3_CLK_SUSPEND;
	frqcrA_mask = FRQCRA_MASK_ES2;
	frqcrB_mask = FRQCRB_MASK_ES2;

	if (!is_restore) {
		pr_info("[%s]: Suspend: Set clock for suspending\n",\
			__func__);
		memory_log_dump_int(
			PM_DUMP_ID_SUSPEND_SET_CLOCK_RETRY_1,
			PM_DUMP_START);
		for (i = 0; i < CPG_SET_FREQ_MAX_RETRY; i++) {
			/* Backup FRQCRA/B */
			frqcrA_save = __raw_readl(FRQCRA);
			frqcrB_save = __raw_readl(FRQCRB);

			cpg_clocks_ret = clock_update(
			frqcrA_suspend_clock, frqcrA_mask,
			frqcrB_suspend_clock, frqcrB_mask);
			if (cpg_clocks_ret < 0)
				udelay(1);
			else
				break;
		}
		if (cpg_clocks_ret < 0) {
			memory_log_dump_int(
			PM_DUMP_ID_SUSPEND_SET_CLOCK_RETRY_1,
			0xFFFF);
			pr_err("[%s]: Set clocks FAILED\n",\
				__func__);
		} else {
			memory_log_dump_int(
			PM_DUMP_ID_SUSPEND_SET_CLOCK_RETRY_1,
			PM_DUMP_END);
			pr_info("[%s]: Set clocks OK(retry %d)\n",
				__func__, i);
		}
#if (defined ZB3_CLK_SUSPEND_ENABLE) && (defined ZB3_CLK_DFS_ENABLE)
		if (es > ES_REV_2_1) {
			frqcrD_save = suspend_ZB3_backup();
			if (frqcrD_save > 0) {
				clocks_ret = cpg_set_sbsc_freq(zb3_clock);
				if (clocks_ret < 0) {
					pr_err("[%s]: Set ZB3 clocks" \
					"FAILED\n", __func__);
				} else {
					pr_info("[%s]: Set ZB3 " \
					"clocks OK\n", __func__);
				}
			} else {
				pr_err("[%s]: Backup ZB3 " \
						"clock FAILED\n", __func__);
					clocks_ret = frqcrD_save;
			}
		}
#endif
	} else {
		pr_info("[%s]: Restore clock for resuming\n",
			__func__);

		memory_log_dump_int(
			PM_DUMP_ID_SUSPEND_SET_CLOCK_RETRY_2,
			PM_DUMP_START);
		for (i = 0; i < CPG_SET_FREQ_MAX_RETRY; i++) {
			cpg_clocks_ret = clock_update(frqcrA_save, frqcrA_mask,
					frqcrB_save, frqcrB_mask);
			if (cpg_clocks_ret < 0)
				udelay(1);
			else
				break;
		}

		if (cpg_clocks_ret < 0) {
			memory_log_dump_int(
			PM_DUMP_ID_SUSPEND_SET_CLOCK_RETRY_2,
			0xFFFF);
			pr_err("[%s]: Restore clocks FAILED\n",
				__func__);
		} else {
			memory_log_dump_int(
			PM_DUMP_ID_SUSPEND_SET_CLOCK_RETRY_2,
			PM_DUMP_END);
			pr_info("[%s]: Restore clocks OK(retry %d)\n",
				__func__, i);
		}
#if (defined ZB3_CLK_SUSPEND_ENABLE) && \
		(defined ZB3_CLK_DFS_ENABLE)
		if (es > ES_REV_2_1) {
			clocks_ret = cpg_set_sbsc_freq(frqcrD_save);
			if (clocks_ret < 0) {
				pr_err("[%s]: Restore ZB3 " \
					"clocks FAILED\n", __func__);
			} else {
				pr_info("[%s]: Restore ZB3 clocks OK\n",
					__func__);
			}
		}
#endif
	}
	return clocks_ret | cpg_clocks_ret;
}

static int shmobile_suspend(void)
{
	int locked;
#ifdef CONFIG_PM_HAS_SECURE
	unsigned int sec_hal_ret_cpu0;
	unsigned int sec_hal_ret_cpu1;
#endif /* CONFIG_PM_HAS_SECURE*/
	unsigned int bankState;
	unsigned int workBankState2Area;
	unsigned int dramPasrSettingsArea0;
	unsigned int dramPasrSettingsArea1;

	/* check wakelock */
	locked = has_wake_lock_no_expire(WAKE_LOCK_SUSPEND);
	if (locked)
		goto Cancel;

	/* check cpu#1 power down */
	if (core_shutdown_status(1) != 3) {
		not_core_shutdown = 1;
		udelay(1000);
		barrier();
		if (core_shutdown_status(1) != 3)
			return -EBUSY;
	}

	/* Backup IP registers */
	/* irqx_eventdetectors_regs_save(); */
	shwy_regs_save();

#if ((defined CONFIG_SHMOBILE_PASR_SUPPORT) \
		&& (defined CONFIG_SHMOBILE_RAM_DEFRAG))
	/* Get ram bank status */
	bankState = get_ram_banks_status();
	if (bankState == -ENOTSUPP)		/* Ram Defrag is disabled */
		bankState = 0xFFFF;
#else
	bankState = 0xFFFF;
#endif
	/*
	 * Get OP of DRAM area 0 and area 1
	 * Bit[0->7] : OP of area 0
	 * Bit[8->15]: OP of area 1
	 */
	workBankState2Area = (SbscDramPasr2Area & ~bankState);
	if (workBankState2Area != 0) {
		/* Get setting OP, MA of DRAM area 0
		 *(OP = Bit[0->7] of workBankState2Area)
		 */
		dramPasrSettingsArea0 = ((workBankState2Area & 0x00FF) << 8) \
								| MRW_MA_PASR;
		dramPasrSettingsArea1 = (workBankState2Area & 0xFF00) \
								| MRW_MA_PASR;
	} else { /* workBankState2Area == 0 */
		dramPasrSettingsArea0 = 0;
		dramPasrSettingsArea1 = 0;
	}

	pr_debug(PMDBG_PRFX "%s: RAM bank status:\n", __func__);
	pr_debug("bankState: 0x%x workBankState2Area: 0x%08x\n" \
				, bankState, workBankState2Area);
	pr_debug("dramPasrSettingsArea0: 0x%08x ", dramPasrSettingsArea0);
	pr_debug("dramPasrSettingsArea1: 0x%08x\n", dramPasrSettingsArea1);

	/* Save setting value to ram0 */
	pm_writel(dramPasrSettingsArea0, ram0DramPasrSettingArea0);
	pm_writel(dramPasrSettingsArea1, ram0DramPasrSettingArea1);

#ifdef CONFIG_PM_HAS_SECURE
	/* For access SBSC registers */
	pm_writel((unsigned long)(map[SBSC_SDCR0A_TZ].base), \
				ram0SBSC_SDCR0AIOremap);
	pm_writel((unsigned long)(map[SBSC_SDWCRC0A_TZ].base), \
				ram0SBSC_SDWCRC0AIOremap);
	pm_writel((unsigned long)(map[SBSC_SDWCRC1A_TZ].base), \
				ram0SBSC_SDWCRC1AIOremap);
	pm_writel((unsigned long)(map[SBSC_SDWCR00A_TZ].base), \
				ram0SBSC_SDWCR00AIOremap);
	pm_writel((unsigned long)(map[SBSC_SDWCR01A_TZ].base), \
				ram0SBSC_SDWCR01AIOremap);
	pm_writel((unsigned long)(map[SBSC_SDWCR10A_TZ].base), \
				ram0SBSC_SDWCR10AIOremap);
	pm_writel((unsigned long)(map[SBSC_SDWCR11A_TZ].base), \
				ram0SBSC_SDWCR11AIOremap);
	pm_writel((unsigned long)(map[SBSC_SDWCRC2A_TZ].base), \
				ram0SBSC_SDWCRC2AIOremap);
#endif
	if (es <= ES_REV_2_1)
		do_iicdvm_setting();

	xtal1_log_out = 1;

	__raw_writel((__raw_readl(WUPSMSK) | (1 << 28)), WUPSMSK);

#ifndef CONFIG_PM_HAS_SECURE
	pm_writel(1, ram0ZQCalib);
#endif	/*CONFIG_PM_HAS_SECURE*/

	/* Update clocks setting */
	if (suspend_set_clock(CLOCK_SUSPEND) != 0) {
			pr_debug(PMDBG_PRFX "%s: Suspend without "\
			"updating clock setting\n", __func__);
	}

	set_regs_for_LPM();
#ifdef CONFIG_ARCH_R8A7373
	if (pmdbg_get_enable_dump_suspend())
		pmdbg_dump_suspend();
#endif /* CONFIG_ARCH_R8A7373 */
	/*
	 * do cpu suspend ...
	 */
	pr_err("[%s]: do cpu suspend ...\n\n", __func__);
	memory_log_func(PM_FUNC_ID_JUMP_SYSTEMSUSPEND, 1);
	jump_systemsuspend();
	memory_log_func(PM_FUNC_ID_JUMP_SYSTEMSUSPEND, 0);

	/* Update clocks setting */
	if (suspend_set_clock(CLOCK_RESTORE) != 0)
		pr_debug(PMDBG_PRFX "%s: Resume after "\
			"restoring clock setting\n", __func__);

#ifndef CONFIG_PM_HAS_SECURE
	pm_writel(0, ram0ZQCalib);
#endif	/*CONFIG_PM_HAS_SECURE*/

	wakeups_factor();
	reset_regs_for_LPM();

#ifdef CONFIG_PM_HAS_SECURE
	sec_hal_ret_cpu0 = __raw_readl(ram0SecHalReturnCpu0);
	pr_debug(PMDBG_PRFX "%s: SEC HAL return CPU0: 0x%08x\n", \
			__func__, sec_hal_ret_cpu0);
#ifdef CONFIG_PM_SMP
	sec_hal_ret_cpu1 = __raw_readl(ram0SecHalReturnCpu1);
	pr_debug(PMDBG_PRFX "%s: SEC HAL return CPU1: 0x%08x\n", \
			__func__, sec_hal_ret_cpu1);
#endif
#endif /*CONFIG_PM_HAS_SECURE*/

	/* - set Wake up factor unmask to GIC.CPU0,CPU1 */
	__raw_writel((__raw_readl(WUPSMSK) &  ~(1 << 28)), WUPSMSK);


	/* Restore IP registers */
	shwy_regs_restore();
	/* irqx_eventdetectors_regs_restore(); */

	return 0;

Cancel:
	return -EBUSY;
}

static int shmobile_suspend_enter(suspend_state_t unused)
{
	int ret = 0;

	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_ENTER, 1);
	switch (shmobile_suspend_state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		ret = shmobile_suspend();
		break;
	default:
		ret = -EINVAL;
	}
	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_ENTER, 0);
	return ret;
}

static int shmobile_suspend_valid(suspend_state_t state)
{
	return ((state > PM_SUSPEND_ON) && (state <= PM_SUSPEND_MAX));
}

static int shmobile_suspend_prepare(void)
{
	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_PREPARE, 1);
#ifdef CONFIG_SHMOBILE_RAM_DEFRAG
	int ret;

	/* Execute RAM Defragmentation */
	ret = defrag();

	if (0 != ret) {
		pr_debug(PMDBG_PRFX "%s: RAM defragment is not supported.\n", \
				__func__);
	}
#endif /* CONFIG_SHMOBILE_RAM_DEFRAG */

	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_PREPARE, 0);
	return 0;
}

static int shmobile_suspend_prepare_late(void)
{
	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_PREPARE_LATE, 1);
	disable_hlt();

	/* backup sys boot address */
	save_sbar_val = __raw_readl(SBAR);

	/* set RAM1 vector */
	__raw_writel(RAM_ARM_VECT, SBAR);

	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_PREPARE_LATE, 0);
	return 0;
}

static void shmobile_suspend_wake(void)
{
#ifdef __EXTAL1_INFO__
	unsigned int reg_val = 0;
#endif
	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_WAKE, 1);
	enable_hlt();

	/* restore sys boot address */
	__raw_writel(save_sbar_val, SBAR);

	/* Log information for disabling EXTAL1 */
#ifdef __EXTAL1_INFO__
	if (xtal1_log_out == 1) {
		pr_debug(PMDBG_PRFX "EXTAL1: Log information\n");
		pr_debug(PMDBG_PRFX "---[Before suspend]---\n");

		reg_val = pm_readl(ram0SaveEXMSKCNT1_suspend);
		pr_debug(PMDBG_PRFX "EXMSKCNT1: 0x%08x\n", reg_val);

		reg_val = pm_readl(ram0SaveAPSCSTP_suspend);
		pr_debug(PMDBG_PRFX "APSCSTP: 0x%08x\n", reg_val);

		reg_val = pm_readl(ram0SaveSYCKENMSK_suspend);
		pr_debug(PMDBG_PRFX "SYCKENMSK: 0x%08x\n", reg_val);

		reg_val = pm_readl(ram0SaveC4POWCR_suspend);
		pr_debug(PMDBG_PRFX "C4POWCR: 0x%08x\n", reg_val);

		reg_val = pm_readl(ram0SavePDNSEL_suspend);
		pr_debug(PMDBG_PRFX "PDNSEL: 0x%08x\n", reg_val);

		reg_val = pm_readl(ram0SavePSTR_suspend);
		pr_debug(PMDBG_PRFX "PSTR: 0x%08x\n", reg_val);

		pr_debug(PMDBG_PRFX "---[After suspend]---\n");

		reg_val = pm_readl(ram0SaveEXMSKCNT1_resume);
		pr_debug(PMDBG_PRFX "EXMSKCNT1: 0x%08x\n", reg_val);

		reg_val = pm_readl(ram0SaveAPSCSTP_resume);
		pr_debug(PMDBG_PRFX "APSCSTP: 0x%08x\n", reg_val);

		reg_val = pm_readl(ram0SaveSYCKENMSK_resume);
		pr_debug(PMDBG_PRFX "SYCKENMSK: 0x%08x\n", reg_val);

		reg_val = pm_readl(ram0SaveC4POWCR_resume);
		pr_debug(PMDBG_PRFX "C4POWCR: 0x%08x\n", reg_val);

		reg_val = pm_readl(ram0SavePDNSEL_resume);
		pr_debug(PMDBG_PRFX "PDNSEL: 0x%08x\n", reg_val);

		reg_val = pm_readl(ram0SavePSTR_resume);
		pr_debug(PMDBG_PRFX "PSTR: 0x%08x\n", reg_val);

		xtal1_log_out = 0;
	}
#endif
	memory_log_func(PM_FUNC_ID_SHMOBILE_SUSPEND_WAKE, 0);
}

#ifdef CONFIG_PM_DEBUG
int control_systemsuspend(int is_enabled)
{
	unsigned long irqflags;
	int cur_state = 0;
	spin_lock_irqsave(&systemsuspend_lock, irqflags);
	cur_state = get_suspend_state();

	if (cur_state == PM_SUSPEND_MEM)
		request_suspend_state(PM_SUSPEND_ON);

	enable_module = is_enabled;
	spin_unlock_irqrestore(&systemsuspend_lock, irqflags);
	return 0;
}
EXPORT_SYMBOL(control_systemsuspend);

int is_systemsuspend_enable(void)
{
	return enable_module;
}
EXPORT_SYMBOL(is_systemsuspend_enable);
#endif /* CONFIG_PM_DEBUG */

struct platform_suspend_ops shmobile_suspend_ops = {
	.begin			= shmobile_suspend_begin,
	.end			= shmobile_suspend_end,
	.enter			= shmobile_suspend_enter,
	.valid			= shmobile_suspend_valid,
	.prepare		= shmobile_suspend_prepare,
	.prepare_late	= shmobile_suspend_prepare_late,
	.wake			= shmobile_suspend_wake,
};

static int __init shmobile_suspend_init(void)
{
	int i;
	void __iomem *virt;
	struct base_map *tbl = map;
#ifdef CONFIG_PM_DEBUG
	log_wakeupfactor = 1;
#else /*CONFIG_PM_DEBUG*/
	log_wakeupfactor = 0;
#endif /*CONFIG_PM_DEBUG*/
	pr_debug(PMDBG_PRFX "%s: initialize\n", __func__);

#ifndef CONFIG_CPU_IDLE
	int ret;
	ret = shmobile_init_pm();
	if (ret != 0)
		return ret;
#endif
	/* Get chip revision */
	es = shmobile_chip_rev();

	/* create address table */
	for (i = 0; i < ARRAY_SIZE(map); i++) {
		if (tbl->base) {
			tbl++;
			continue;
		}
		virt = ioremap_nocache(tbl->phys, tbl->size);
		if (!virt) {
			pr_emerg(PMDBG_PRFX \
					"%s: ioremap failed. base 0x%lx\n", \
					__func__, tbl->phys);
			tbl++;
			continue;
		}
		tbl->base = virt;
		pr_debug(PMDBG_PRFX \
			"%s: ioremap phys 0x%lx, virt 0x%p, size %d\n", \
			__func__, tbl->phys, tbl->base, tbl->size);
		tbl++;
	}

	wakeups_factor();

	suspend_set_ops(&shmobile_suspend_ops);

	shmobile_suspend_state = PM_SUSPEND_ON;
#ifndef CONFIG_PM_HAS_SECURE
	pm_writel(0, ram0ZQCalib);
#endif	/*CONFIG_PM_HAS_SECURE*/

	return 0;
}
arch_initcall(shmobile_suspend_init);
