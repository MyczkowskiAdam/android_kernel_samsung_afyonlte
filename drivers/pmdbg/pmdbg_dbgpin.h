/*
 * pmdbg_dbgpin.h
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
#ifndef __TST_PM_DBGPIN__
#define __TST_PM_DBGPIN__

#include "pmdbg_api.h"
#include <linux/errno.h>
#include <mach/pm.h>
#include "pmdbg_hw.h"
#ifdef CONFIG_ARM_TZ
#include <sec_hal_cmn.h>
#endif /*CONFIG_ARM_TZ*/

#define DBGREG1_PTR			((void __iomem *)IO_ADDRESS(0xE6100020))
#define DBGREG3_PTR			((void __iomem *)IO_ADDRESS(0xE6100028))
#define DBGREG9_PTR			((void __iomem *)IO_ADDRESS(0xE6100040))
#define DBGREG11_PTR		((void __iomem *)IO_ADDRESS(0xE6100048))
#define SWUCR_PTR			((void __iomem *)SWUCR)
#define PSTR_PTR			((void __iomem *)PSTR)
#define GPIO_MSEL03CR_PTR	((void __iomem *)MSEL3CR)

#define DBGREG9_AID		0xA5
#define DBGREG9_AID_SHIFT	8
#define DBGREG9_AID_MASK	(0xFF << DBGREG9_AID_SHIFT)
#define DBGREG9_KEY_SHIFT	0x0
#define DBGREG9_KEY_MASK	0x1

#define DBGREG1_TRC_MASK	0x0000E000
#define DBGREG1_TRC_SHIFT	13
#define TRC_MON				0x4

#define DBGREG1_MDA_MASK	0x000000FF
#define DBGREG1_MDA_SHIFT	0
#define MDA_PA				0x06
#define MDA_CPU				0x05
#define MDA_CLK				0x03

#define MONITOR_NUM			0x23

#define GPIO_MSEL03CR_MSEL15_SHIFT	15
#define GPIO_MSEL03CR_MSEL15_MASK	\
		(1 << GPIO_MSEL03CR_MSEL15_SHIFT)
#define GPIO_MSEL03CR_MSEL15_KEY	0
#define GPIO_MSEL03CR_MSEL15_BSC	1

extern int arch_hw_breakpoint_init_late(void);
extern int init_hw_perf_events_late(void);

#endif /*__TST_PM_DBGPIN__*/
