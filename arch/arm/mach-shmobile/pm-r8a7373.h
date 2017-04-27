#ifndef __ARCH_ARM_MACH_SHMOBILE_PM_R8A7373_H
#define __ARCH_ARM_MACH_SHMOBILE_PM_R8A7373_H
/*
 * arch/arm/mach-shmobile/pm-r8a7373.h
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
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

#include <mach/r8a7373.h>
/* SCU */
#define SCU_PWRST      (SCU_BASE + 0x08)
#define SCU_INVALL     (SCU_BASE + 0x0c)

/* CPG */
#define WUPCR		(CPG_BASE + 0x1010)
#define SRESCR		(CPG_BASE + 0x1018)
#define CPU0RFR		(CPG_BASE + 0x1104)
#define CPU1RFR		(CPG_BASE + 0x1114)
#define SPCTR		(CPG_BASE + 0x01a4)
#define SPCMMR		(CPG_BASE + 0x01ac)
#define SPCDMR		(CPG_BASE + 0x01b0)

/* size */
#define RAM0_VECTOR_SIZE	0x80

#define RAM0_SAVE_OFFSET	0x1e00

/* code area */
#define RAM0_VECTOR_ADDR	(RAM0_BASE + 0x00)
#define RAM0_VECTOR_ADDR_PHYS	(RAM0_BASE_PHYS + 0x00)

/* save area */
#define RAM0_SAVE_BASE		(RAM0_BASE + RAM0_SAVE_OFFSET)
#define RAM0_WAKEUP_ADDR0	(RAM0_SAVE_BASE + 0x00)
#define RAM0_WAKEUP_ADDR1	(RAM0_WAKEUP_ADDR0 + 0x04)

#ifndef __ASSEMBLY__
extern void r8a7373_common_vector(void);
extern unsigned int r8a7373_common_vector_size;

extern void r8a7373_resume_core_standby(void);
extern int r8a7373_do_idle_core_standby(unsigned long unused);
#endif

#endif
