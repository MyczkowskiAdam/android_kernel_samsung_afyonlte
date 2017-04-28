/* common_extern.h
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


#ifndef __COMMON_EXTERN_H__
#define __COMMON_EXTERN_H__

#include <linux/kernel.h>
#include "soundpath.h"

#ifdef __COMMON_CTRL_NO_EXTERN__
#define COMMON_CTRL_NO_EXTERN
#else
#define COMMON_CTRL_NO_EXTERN	extern
#endif

/* Struct declaration */
/* Register setting table */
struct common_reg_table {
	u_int	uiReg;
	u_int	uiValue;
	u_int	uiDelay;
	u_int	uiClrbit;
};

/* FSI base address (PortA) */
COMMON_CTRL_NO_EXTERN u_char __iomem *g_fsi_Base;
/* SCUW base address */
COMMON_CTRL_NO_EXTERN u_char __iomem *g_scuw_Base;
/* SCUW base address to FFD */
COMMON_CTRL_NO_EXTERN u_char __iomem *g_scuw_Base_FFD;
/* CLKGEN base address */
COMMON_CTRL_NO_EXTERN u_char __iomem *g_clkgen_Base;
/* SCUW base address to CPUFIFO2 */
COMMON_CTRL_NO_EXTERN u_char __iomem *g_scuw_Base_CPUFIFO2;

COMMON_CTRL_NO_EXTERN void iomodify32(u_int uiClr, u_int uiSet, u_int uiReg);
COMMON_CTRL_NO_EXTERN int common_ioremap(void);
COMMON_CTRL_NO_EXTERN void common_iounmap(void);
COMMON_CTRL_NO_EXTERN void audio_ctrl_func(
	enum sndp_hw_audio drv,
	int stat,
	const u_int regclr);
COMMON_CTRL_NO_EXTERN void common_set_register(
	enum sndp_hw_audio drv,
	struct common_reg_table *reg_tbl,
	u_int size);
COMMON_CTRL_NO_EXTERN void common_set_pll22(const u_int uiValue, int stat, u_int rate);
COMMON_CTRL_NO_EXTERN void common_set_fsi2cr(u_int dev, int stat);

#endif /* __COMMON_EXTERN_H__ */



