/*
 * Function        : Composer driver for SH Mobile
 *
 * Copyright (C) 2013-2013 Renesas Electronics Corporation
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
 * Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */
#ifndef _SH_MOBILE_COMPOSER_FBCOMPOSE_H
#define _SH_MOBILE_COMPOSER_FBCOMPOSE_H

#include <linux/sh_mobile_composer.h>
#include <media/sh_mobile_appmem.h>
#include <linux/fb.h>

#include "../composer/sh_mobile_lcd_compose.h"
#include "../composer/sh_mobile_work.h"

/******************************/
/* define for function        */
/******************************/

/******************************/
/* define for set_buffer      */
/******************************/
#define FB_SCREEN_BUFFERID0      0 /* normal frame buffer */
#define FB_SCREEN_BUFFERID1      1 /* gpu    frame buffer */
#define FB_SCREEN_BLANKBUFFER    2 /* blank  frame buffer */

/******************************/
/* define structure           */
/******************************/

struct cmp_information_fb {
	int                    fb_select_bufferid;
	int                    fb_select_yoffset;
	int                    fb_count_display;
	int                    fb_double_buffer;
	int                    fb_offset_info[2][2];

	int                    direct_display[2];
	int                    blend_bufferid;

	struct fb_info         *fb_info;
	unsigned long          queue_fb_map_address;
	unsigned long          queue_fb_map_endaddress;
	struct rtmem_phys_handle *queue_fb_map_handle;
	unsigned long          queue_fb_map_address2;
	unsigned long          queue_fb_map_endaddress2;
	struct rtmem_phys_handle *queue_fb_map_handle2;
};

/******************************/
/* define external function   */
/******************************/

/* interface of execute RT-API.  */
static void *lcd_rtapi_create(int lcd_width, int lcd_height,
	int hdmi_width, int hdmi_height);
static int  lcd_rtapi_delete(void *handle);
static int  lcd_rtapi_blend(void *handle, struct composer_rh *rh);

/* configure blending parameters (input layer) */
static int  lcd_config(struct composer_rh *rh, struct cmp_postdata *post);

/* configure blending parameters (output) and display area. */
static int  lcd_config_output(struct composer_rh *rh,
		struct cmp_information_fb *info);

/* set buffer */
static int  lcd_set_address(int id, unsigned long addr, unsigned long size,
		struct localworkqueue  *workqueue,
		struct cmp_information_fb *info);
static int  lcd_unset_address(int id, struct localworkqueue  *workqueue,
		struct cmp_information_fb *info);

/* fb driver */
static int  lcd_fb_pan_display(struct composer_rh *rh,
		struct cmp_information_fb *info);
static int  lcd_get_resolution(int *width, int *height,
		struct cmp_information_fb *info);

#endif
