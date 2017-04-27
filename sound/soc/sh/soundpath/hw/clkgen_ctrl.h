/* clkgen_ctrl.h
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

#ifndef __CLKGEN_CTRL_H__
#define __CLKGEN_CTRL_H__

#include <linux/kernel.h>

/*
 * CLKGEN Registers
 */

#define CLKG_SYSCTL	(0x0000)	/* System control register    */
#define CLKG_PULSECTL	(0x0004)	/* PULSE control register     */
#define CLKG_TIMSEL0	(0x0054)	/* TIM select register 0      */
#define CLKG_TIMSEL1	(0x0058)	/* TIM select register 1      */
#define CLKG_FSISEL	(0x000C)	/* FSI select register        */
#define CLKG_FSIACOM	(0x0010)	/* FSI Port A common register */
#define CLKG_FSIBCOM	(0x0014)	/* FSI Port B common register */
#define CLKG_CPF0COM	(0x0018)	/* CPF0 common register       */
#define CLKG_CPF1COM	(0x001C)	/* CPF1 common register       */
#define CLKG_SPUVCOM	(0x0020)	/* SPUV common register       */
#define CLKG_AURCOM	(0x0024)	/* AURAM common register      */
#define CLKG_FFDCOM	(0x002C)	/* FFD common register        */
#define CLKG_SLIMCOM	(0x002C)	/* SLIM common register       */
#define CLKG_FSIAAD	(0x0030)	/* FSI Port A adjust register */
#define CLKG_FSIBAD	(0x0034)	/* FSI Port B adjust register */
#define CLKG_CPF0AD	(0x0038)	/* CPF0 adjust register       */
#define CLKG_CPF1AD	(0x003C)	/* CPF1 adjust register       */
#define CLKG_SPUVAD	(0x0040)	/* SPUV adjust register       */
#define CLKG_AURAD	(0x0044)	/* AURAM adjust register      */
#define CLKG_FFDAD	(0x0048)	/* FFD adjust register        */
#define CLKG_CLKADIV	(0x0090)	/* CLK audio divider register */


/*
 * PROTOTYPE Declarations
 */

/* Voice call setting function */
static void clkgen_voicecall(const u_int uiValue);
/* Playback setting function */
static void clkgen_playback(const u_int uiValue);
/* Capture setting function */
static void clkgen_capture(const u_int uiValue);


#endif /* __CLKGEN_CTRL_H__ */

