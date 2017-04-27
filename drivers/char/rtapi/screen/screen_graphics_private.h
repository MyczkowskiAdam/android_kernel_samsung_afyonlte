/*
 * screen_graphics_private.h
 *  screen graphics private header file.
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
#ifndef __SCREEN_GRAPHICS_PRIVATE_H__
#define __SCREEN_GRAPHICS_PRIVATE_H__

#define RT_GRAPHICS_BLEND_LAYER		4

inline void *GET_RT_CACHE_ADDRESS(void *addr)
{
	rtds_memory_drv_change_addr_param rtds_change_addr;
	rtds_change_addr.org_addr = (unsigned int)addr;
	rtds_change_addr.chg_addr = 0;
	(void)rtds_memory_drv_rtpmb_cache_address(&rtds_change_addr);
	return (void *)rtds_change_addr.chg_addr;
}

typedef struct {
	void *handle;
	void *rtds_mem_handle;
	void (*notify_graphics_image_conv)
		(
			int				result,
			unsigned long	user_data
		);
	void (*notify_graphics_image_blend)
		(
			int				result,
			unsigned long	user_data
		);
	void (*notify_graphics_image_output)
		(
			int				result,
			unsigned long	user_data
		);
	void (*notify_graphics_image_edit)
		(
			int				result,
			unsigned long	user_data
		);
} screen_grap_handle;

/* screen_graphics_image_blend API parameter(local) */
typedef struct {
	screen_grap_image_param		image;						/* input image parameters */
	screen_rect					rect;						/* rect */
	unsigned short				alpha;						/* alpha */
	unsigned short				rotate;						/* rotation type */
	unsigned short				mirror;						/* mirror type */
	unsigned short				dummy;						/* dummy */
	long						key_color;					/* key color */
	unsigned short				premultiplied;				/* premultiplied */
	unsigned short				alpha_coef;					/* alpha coefficient */
} screen_grap_layer_local;


#endif /* __SCREEN_GRAPHICS_PRIVATE_H__ */
