/*
 * system_rtload_internal.h
 *     RT domain boot internal function header file.
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

#ifndef __SYSTEM_RTLOAD_INTERNAL_H__
#define __SYSTEM_RTLOAD_INTERNAL_H__

enum {
	RT_LEVEL_1,
	RT_LEVEL_2,
	RT_LEVEL_MAX
};

typedef struct {
	unsigned long		start;
	unsigned long		size;
} system_rt_img_info;

typedef struct {
	unsigned char		version[12];
	unsigned long		table_size;
	unsigned long		boot_address;
	unsigned long		image_size;
	unsigned long		memmpl_address;
	unsigned long		memmpl_size;
	unsigned long		command_area_address;
	unsigned long		command_area_size;
	unsigned long		load_flag;
	system_rt_img_info	img[RT_LEVEL_MAX];
	unsigned long		sh_pmb_offset;
	unsigned long		sh_pmb_nc_offset;
	unsigned long		mfi_pmb_offset;
} system_rt_section_header;

typedef struct {
	system_rt_section_header	*section_header;
} get_section_header_param;


int sys_get_section_header(get_section_header_param *param);

#endif /* __SYSTEM_RTLOAD_INTERNAL_H__ */
