/*
 * rt_boot_drv.h
 *     RT Boot driver API function header file.
 *
 * Copyright (C) 2012 Renesas Electronics Corporation
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
#ifndef __RTBOOT_DRV_H__
#define __RTBOOT_DRV_H__

enum {
	RT_LVL_1 = 0,
	RT_LVL_2,
	RT_LVL_MAX
};

struct rt_section_img_info {
	unsigned int	section_start;		/* start address of section */
	unsigned int	section_size;		/* size of section	*/
};

struct rt_boot_info {
	unsigned char	version[12];
	unsigned int	table_size;						/* unused */
	unsigned int	boot_addr;
	unsigned int	image_size;
	unsigned int	memmpl_address;					/* unused */
	unsigned int	memmpl_size;					/* unused */
	unsigned int	command_area_address;
	unsigned int	command_area_size;
	unsigned int	load_flg;
	struct rt_section_img_info img[RT_LVL_MAX];	/* section image information */
	unsigned int	sh_pmb_offset;
	unsigned int	sh_pmb_nc_offset;
	unsigned int	mfi_pmb_offset;
};


/*** prototype ***/
int rtboot_get_section_header(struct rt_boot_info *info);


#endif /* __RTBOOT_DRV_H__ */
