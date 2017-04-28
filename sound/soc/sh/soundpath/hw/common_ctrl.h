/* common_ctrl.h
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __COMMON_CTRL_H__
#define __COMMON_CTRL_H__

#include <linux/kernel.h>

/*
 * COMMON Registers
 */
#define CPG_REG_MAX		(0x01FF)

#define CPG_REG_MAX_SRST	(0x01CC)

#define GPIO_PHY_BASE_AUDIO_STATUS_PHYS		(0xE6058000)
#define GPIO_REG_MAX		(0x0150)

#define CPG_SMSTPCR2		(g_common_ulClkRstRegBase + 0x00000138)
#define CPG_SMSTPCR3		(g_common_ulClkRstRegBase + 0x0000013C)
#define CPG_SMSTPCR6		(g_common_ulClkRstRegBase + 0x00000148)
#define CPG_SRCR2		(g_common_ulSrstRegBase + 0x000000B0)
#define CPG_SRCR3		(g_common_ulSrstRegBase + 0x000000B8)
#define CPG_SRCR6		(g_common_ulSrstRegBase + 0x000001C8)
#define CPG_PLLECR		(g_common_ulClkRstRegBase + 0x000000D0)
#define CPG_PLL22CR		(g_common_ulClkRstRegBase + 0x000001F4)
#define CPG_FSIACKCR		(g_common_ulClkRstRegBase + 0x00000018)
#define CPG_FSIBCKCR		(g_common_ulClkRstRegBase + 0x00000090)

#define GPIO_FSI2CR		(g_common_ulClkGpioRegBase + 0x0144)

#ifdef SOUND_TEST
#define	CPG_SUBCKCR		(g_common_ulSrstRegBase + 0x00000080)
#endif

#define FSI_REG_MAX		(0x040C)
#define FSI_MAP_LEN		(FSI_REG_MAX + 4)

/* SCUW physical address top. */
/* SCUW physical address offset size. */
#define SCUW_REG_MAX		(0x00738)
/* SCUW physical address mapped size. */
#define SUCW_MAP_LEN		(SCUW_REG_MAX + 4)
/* SCUW FFD physical address offset size. */
#define SCUW_REG_MAX_FFD	(0x00020)
/* SCUW FFD physical address mapped size. */
#define SUCW_MAP_LEN_FFD	(SCUW_REG_MAX_FFD + 4)
/* SCUW FFD physical address offset size. */
#define SCUW_REG_MAX_CPUFIFO2	(0x00020)
/* SCUW FFD physical address mapped size. */
#define SUCW_MAP_LEN_CPUFIFO2	(SCUW_REG_MAX_CPUFIFO2 + 4)

#define CLKGEN_REG_MAX		(0x0098)
#define CLKGEN_MAP_LEN		(CLKGEN_REG_MAX + 4)

#define SNDP_CLK_FSI		(0x01)
#define SNDP_CLK_SCUW		(0x02)
#define SNDP_CLK_CLKGEN		(0x04)

#define COMMON_UDELAY_MAX	(1000)

/*
 * PROTOTYPE Declarations
 */

/* ioremap function */
static int fsi_ioremap(void);
static int scuw_ioremap(void);
static int clkgen_ioremap(void);
static int common_audio_status_ioremap(void);

/* iounmap function */
static void fsi_iounmap(void);
static void scuw_iounmap(void);
static void clkgen_iounmap(void);
static void common_audio_status_iounmap(void);


#endif /* __COMMON_CTRL_H__ */

