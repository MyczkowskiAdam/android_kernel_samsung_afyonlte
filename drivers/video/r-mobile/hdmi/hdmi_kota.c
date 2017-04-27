/*
 * drivers/video/r-mobile/hdmi/hdmi_kota.c
 *
 * Copyright (C) 2012 Renesas Electronics Corporation
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

#include <linux/kernel.h>

#include <video/sh_mobile_lcdc.h>

#include <rtapi/screen_display.h>

/*edid*/
#include "edid2.h"
/*edid*/

extern struct mutex format_update_lock;

#define LCD_BG_BLACK 0x0

static screen_disp_aspect aspect[10] = {
	{ 16, 9, 32, 27 }, /* 720x480p60 */
	{ 16, 9, 1, 1 },   /* 1280x720p60 */
	{ 16, 9, 1, 1 },   /* 1920x1080p60 */
	{ 16, 9, 64, 45 }, /* 720x576p50 */
	{ 16, 9, 1, 1 },   /* 1280x720p50 */
	{ 16, 9, 1, 1 },   /* 1920x1080p50 */
	{ 4, 3, 8, 9 },    /* 720x480p60 4:3 */
	{ 4, 3, 16, 15 },  /* 720x576p50 4:3 */
	{ 16, 9, 1, 1 },   /* 1920x1080i60 */
	{ 16, 9, 1, 1 },   /* 1920x1080P24 */
};

static screen_disp_hdmi_if hdmi_if[10] = {
	{/* 720x480p60 */
		0x005A006B, 0x0007005C, 0x00020600,
		0x01E0020D, 0x000601EA,
		0, 0,
		0, 0xB, 0xB,
		0, 0, 0x00010040,
		0x00003F07, 0x14003222, 0x00040091, 0x00000001,
		0x0061003E, 0x00C00038, 0x08700000, 0x01600024, 0, 0,
		0x00000006, 0x1A100000, 0x00001033, 0x2A83800F
	},/* 720x480p60 */
	{/* 1280x720p60 */
		0x00A000CE, 0x000500AD, 0x00020006,
		0x02D002EE, 0x000502D6,
		0, 0,
		0, 0xB, 0xB,
		0, 0, 0x00010040,
		0x00003F07, 0x14006453, 0x000900C1, 0x00000001,
		0x0051003E, 0x00C00038, 0x0F000000, 0x02FE013E, 0, 0,
		0x00000006, 0x19100000, 0x00001011, 0x2A83800F
	},/* 1280x720p60 */
	{/* 1920x1080p60 */
		0x00F00113, 0x000500FB, 0x00000400,
		0x04380465, 0x0005043D,
		0, 0,
		0, 0xB, 0xB,
		0, 0, 0x00060040,
		0x00003F0F, 0x14009797, 0x00120121, 0x00000001,
		0x0053100E, 0x00C00038, 0x0F000000, 0x017200A4, 0, 0,
		0x00000006, 0x18100000, 0x00001011, 0x2A83800F
	},/* 1920x1080p60 */
	{/* 720x576p50 */
		0x005A006C, 0x0008005B, 0x00000004,
		0x02400271, 0x00050246,
		0, 0,
		0, 0xB, 0xB,
		0, 0, 0x00010040,
		0x00003F07, 0x14003222, 0x00040091, 0x00000001,
		0x0081003E, 0x00C00038, 0x08700000, 0x017E0018, 0, 0,
		0x00000006, 0x1A100000, 0x00001033, 0x2A83800F
	},/* 720x576p50 */
	{/* 1280x720p50 */
		0x00A000F7, 0x000500D7, 0x00040000,
		0x02D002EE, 0x000502D6,
		0, 0,
		0, 0xB, 0xB,
		0, 0, 0x00010040,
		0x00003F07, 0x14006453, 0x000900C1, 0x00000001,
		0x0051003E, 0x00C00038, 0x0F000000, 0x02FE051C, 0, 0,
		0x00000006, 0x19100000, 0x00001011, 0x2A83800F
	},/* 1280x720p50 */
	{/* 1920x1080p50 */
		0x00F0014A, 0x00050132, 0x00000400,
		0x04380465, 0x0005043D,
		0, 0,
		0, 0xB, 0xB,
		0, 0, 0x00060040,
		0x00003F0F, 0x14009797, 0x00120121, 0x00000001,
		0x0053100E, 0x00C00038, 0x0F000000, 0x01720414, 0, 0,
		0x00000006, 0x18100000, 0x00001011, 0x2A83800F
	},/* 1920x1080p50 */
	{/* 720x480p60 4:3 */
		0x005A006B, 0x0007005C, 0x00020600,
		0x01E0020D, 0x000601EA,
		0, 0,
		0, 0xB, 0xB,
		0, 0, 0x00010040,
		0x00003F07, 0x14003222, 0x00040091, 0x00000001,
		0x0061003E, 0x00C00038, 0x08700000, 0x01600024, 0, 0,
		0x00000006, 0x1A100000, 0x00001033, 0x2A83800F
	},/* 720x480p60 4:3 */
	{/* 720x576p50 4:3 */
		0x005A006C, 0x0008005B, 0x00000004,
		0x02400271, 0x00050246,
		0, 0,
		0, 0xB, 0xB,
		0, 0, 0x00010040,
		0x00003F07, 0x14003222, 0x00040091, 0x00000001,
		0x0081003E, 0x00C00038, 0x08700000, 0x017E0018, 0, 0,
		0x00000006, 0x1A100000, 0x00001033, 0x2A83800F
	},/* 720x576p50 4:3 */
	{/* 1920x1080i60 */
		0x00F00113, 0x000500FB, 0x00000400,
		0x021C0233, 0x00050220,
		0x021C0232, 0x0005021E,
		0, 0xB, 0xB,
		0, 0, 0x00010040,
		0x00003F07, 0x14006453, 0x000900C1, 0x00000001,
		0x1051003E, 0x00C00038, 0x16800000, 0x023200FC, 0, 0,
		0x00000006, 0x19100000, 0x00001011, 0x2A83800F
	},/* 1920x1080i60 */
	{/* 1920x1080p24 */
		0x00F00157, 0x00050135, 0x00060406,
		0x04380465, 0x00050220,
		0x021C0232, 0x0005043C,
		0, 0x0400008B, 0x0400008B,
		0, 0, 0x00010040,
		0x00003F07, 0x50006454, 0x000B0113, 0x00000001,
		0x0002003E, 0x00C00018, 0x16800000, 0x0143058F, 0, 0,
		0x00000006, 0x19100000, 0x00001006, 0x2A838004
	},/* 1920x1080p24 */

};


static int kota_hdmi_set(unsigned int format)
{

	void *hdmi_handle;
	screen_disp_start_hdmi disp_start_hdmi;
	screen_disp_delete disp_delete;
	screen_disp_set_hdmi_if_param hdmi_if_param;
	int ret;
	edid_read(); /*edid*/
	format = edid_set_output_format();
	hdmi_handle = screen_display_new();

	switch (format) {

	case SH_FB_HDMI_480P60:
	{
		disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
		hdmi_if_param.aspect = &aspect[0];
		hdmi_if_param.hdmi_if_param = &hdmi_if[0];
		break;
	}
	case SH_FB_HDMI_720P60:
	{
		disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
		hdmi_if_param.aspect = &aspect[1];
		hdmi_if_param.hdmi_if_param = &hdmi_if[1];
		break;
	}
	case SH_FB_HDMI_1080P60:
	{
		disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
		hdmi_if_param.aspect = &aspect[2];
		hdmi_if_param.hdmi_if_param = &hdmi_if[2];
		break;
	}
	case SH_FB_HDMI_576P50:
	{
		disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
		hdmi_if_param.aspect = &aspect[3];
		hdmi_if_param.hdmi_if_param = &hdmi_if[3];
		break;
	}
	case SH_FB_HDMI_720P50:
	{
		disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
		hdmi_if_param.aspect = &aspect[4];
		hdmi_if_param.hdmi_if_param = &hdmi_if[4];
		break;
	}
	case SH_FB_HDMI_1080P50:
	{
		disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
		hdmi_if_param.aspect = &aspect[5];
		hdmi_if_param.hdmi_if_param = &hdmi_if[5];
		break;
	}
	case SH_FB_HDMI_480P60A43:
	{
		disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
		hdmi_if_param.aspect = &aspect[6];
		hdmi_if_param.hdmi_if_param = &hdmi_if[6];
		break;
	}
	case SH_FB_HDMI_576P50A43:
	{
		disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
		hdmi_if_param.aspect = &aspect[7];
		hdmi_if_param.hdmi_if_param = &hdmi_if[7];
		break;
	}
	case SH_FB_HDMI_1080I60:
	{
		disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
		hdmi_if_param.aspect = &aspect[8];
		hdmi_if_param.hdmi_if_param = &hdmi_if[8];
		break;
	}
	case SH_FB_HDMI_1080P24:
	{
		printk("%s resolution 1080P 24Hz \n",__func__);
		disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
		hdmi_if_param.aspect = &aspect[9];
		hdmi_if_param.hdmi_if_param = &hdmi_if[9];
		break;
	}
	}

	hdmi_if_param.handle = hdmi_handle;
	hdmi_if_param.ipmode = RT_DISPLAY_PROGRESSIVE;
	ret = screen_display_set_hdmi_if_parameters(&hdmi_if_param);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		r_mobile_fb_err_msg(ret,
				    "screen_display_set_hdmi_if_parameters");
		disp_delete.handle = hdmi_handle;
		screen_display_delete(&disp_delete);
		mutex_unlock(&format_update_lock);
		return -1;

	}

	disp_start_hdmi.handle = hdmi_handle;
	disp_start_hdmi.background_color = LCD_BG_BLACK;
	ret = screen_display_start_hdmi(&disp_start_hdmi);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		r_mobile_fb_err_msg(ret, "screen_display_start_hdmi");
		disp_delete.handle = hdmi_handle;
		screen_display_delete(&disp_delete);
		mutex_unlock(&format_update_lock);
		return -1;
	}

	disp_delete.handle = hdmi_handle;
	screen_display_delete(&disp_delete);
	mutex_unlock(&format_update_lock);
	return 0;

}

static int kota_hdmi_suspend(void)
{
	void *hdmi_handle;
	screen_disp_stop_hdmi disp_stop_hdmi;
	screen_disp_delete disp_delete;
	int ret;

	hdmi_handle = screen_display_new();
	disp_stop_hdmi.handle = hdmi_handle;
	ret = screen_display_stop_hdmi(&disp_stop_hdmi);
	if (ret != SMAP_LIB_DISPLAY_OK)
		r_mobile_fb_err_msg(ret, "screen_display_stop_hdmi");

	disp_delete.handle = hdmi_handle;
	screen_display_delete(&disp_delete);

	return 0;

}

static int kota_hdmi_resume(void)
{
	void *hdmi_handle;
	screen_disp_start_hdmi disp_start_hdmi;
	screen_disp_delete disp_delete;
	int ret;

	hdmi_handle = screen_display_new();
	disp_start_hdmi.handle = hdmi_handle;
	disp_start_hdmi.format = RT_DISPLAY_USE_IF_PARAM;
	disp_start_hdmi.background_color = LCD_BG_BLACK;
	ret = screen_display_start_hdmi(&disp_start_hdmi);
	if (ret != SMAP_LIB_DISPLAY_OK)
		r_mobile_fb_err_msg(ret, "screen_display_start_hdmi");

	disp_delete.handle = hdmi_handle;
	screen_display_delete(&disp_delete);

	return 0;

}
struct fb_hdmi_func r_mobile_hdmi_func(void)
{
	struct fb_hdmi_func hdmi_func;

	memset(&hdmi_func, 0, sizeof(struct fb_hdmi_func));

	hdmi_func.hdmi_set     = kota_hdmi_set;
	hdmi_func.hdmi_suspend = kota_hdmi_suspend;
	hdmi_func.hdmi_resume  = kota_hdmi_resume;

	return hdmi_func;
}
