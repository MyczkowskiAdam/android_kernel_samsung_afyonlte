/*
 * rt_boot_local.h
 *		booting rt_cpu.
 *
 * Copyright (C) 2012,2013 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __RTBOOT_LOCAL_H__
#define __RTBOOT_LOCAL_H__

#include <mach/memory-r8a7373.h>
#include "rt_boot_drv.h"

#define DEBUG (0)
#define SECURE_BOOT			(0)

#if defined(CONFIG_ARM_SEC_HAL) && SECURE_BOOT
#include <sec_hal_cmn.h>
#define SECURE_BOOT_ENABLE
#endif

#define MAX_POLLING_COUNT	(30000)
#define MSLEEP_WAIT_VALUE	(1)

/* RT boot_program size */
#define RT_BOOT_SIZE		(0xb00)

/* Screen information */
#ifdef CONFIG_FB_R_MOBILE_NT35510
#define SCREEN0_HEIGHT		(800)
#define SCREEN0_WIDTH		(480)
#define SCREEN0_STRIDE		(480)
#endif

#ifdef CONFIG_FB_R_MOBILE_HX8389B
#define SCREEN0_HEIGHT          (960)
#define SCREEN0_WIDTH           (540)
#define SCREEN0_STRIDE          (544)
#endif

#define SCREEN0_MODE		(0)
#define SCREEN1_HEIGHT		(0)
#define SCREEN1_WIDTH		(0)
#define SCREEN1_STRIDE		(0)
#define SCREEN1_MODE		(0)

#define PRIMARY_COPY_ADDR	(SDRAM_TUNE_UP_VALUE_START_ADDR)

struct screen_info {
	unsigned short	height;
	unsigned short	width;
	unsigned short	stride;
	unsigned short	mode;
};

void	do_ioremap_register(void);
void	do_iounmap_register(void);
int		read_rt_image(unsigned int *addr);
void	write_rt_imageaddr(unsigned int addr);
void	stop_rt_interrupt(void);
void	init_rt_register(void);
void	write_os_kind(unsigned int kind);
void	start_rt_cpu(void);
int		wait_rt_cpu(unsigned int check_num);
void	write_req_comp(void);
int		read_rt_cert(unsigned int addr);

extern struct rt_boot_info g_rtboot_info;

#endif

