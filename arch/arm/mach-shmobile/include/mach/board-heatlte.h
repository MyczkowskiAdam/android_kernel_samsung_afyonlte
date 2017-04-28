/*
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef __ASM_ARCH_BOARD_H
#define __ASM_ARCH_BOARD_H

#if defined(CONFIG_BATTERY_BQ27425)
#define BQ27425_ADDRESS (0xAA >> 1)
#define GPIO_FG_INT 44
#endif

#define GPIO_CHG_INT 19

#if defined(CONFIG_CHARGER_SMB328A)
#define SMB327B_ADDRESS (0xA9 >> 1)     // SMB327B
#define SMB328A_ADDRESS (0x69 >> 1)     // SMB328A
#endif

#if defined(CONFIG_CHARGER_SMB358)
#define CHARGER_I2C_SLAVE_ADDRESS (0xD4 >> 1)		//for smb 358
#endif

#ifdef CONFIG_USE_MUIC
#ifdef CONFIG_USB_SWITCH_TSU6712
#define MUIC_I2C_ADDRESS (0x4A >> 1)
#define MUIC_NAME	"tsu6712"
#endif
#ifdef CONFIG_USB_SWITCH_RT8973
#define MUIC_I2C_ADDRESS (0x28 >> 1) //Only RT8973 is different with another MUIC
#define MUIC_NAME	"rt8973"
#endif
#define GPIO_MUS_INT 41
#endif

#ifdef CONFIG_RT8973
#define GPIO_MUS_INT 41
#endif

/**
 * BOARD_REV
 */
#define BOARD_REV_0_0	0
#define BOARD_REV_0_1	1
#define BOARD_REV_0_2	2
#define BOARD_REV_0_3	3
#define BOARD_REV_0_4	4

/**
 * ION
 */
#ifdef CONFIG_ION_R_MOBILE_USE_VIDEO_HEAP
#define ION_HEAP_VIDEO_SIZE	(SZ_16M + SZ_2M)
#define ION_HEAP_VIDEO_ADDR	0x4AE00000
#endif

#endif // __ASM_ARCH_BOARD_H
