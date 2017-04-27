/*
 * system_rtload_private.h
 *	 RT domain boot function private header file.
 *
 * Copyright (C) 2012-2013 Renesas Electronics Corporation
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

#ifndef __SYSTEM_RTLOAD_PRIVATE_H__
#define __SYSTEM_RTLOAD_PRIVATE_H__

#include "system_rtload_internal.h"
#include <mach/r8a7373.h>

#define RT_FILENAME "/boot/RTFM_SH4AL_DSP_MU200.bin"

#define SYSTEM_RTLOAD_LEVEL2			(2)
#define RT_BOOT_SIZE					0xb00

#define REG_RT_BOOT_ADDR	RBARPhys
#define RD_BOOT_ADDR(x)		(readl((x)) & ~0xf)

int system_sub_load_rtimage(void);
int system_sub_get_section_header(system_rt_section_header *section_header);

#endif	/* __SYSTEM_RTLOAD_PRIVATE_H__ */

