/* Copyright (C) 2013 Renesas Electronics Corporation
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
#include <linux/delay.h>

#include <linux/gpio.h>

#include <video/sh_mobile_lcdc.h>

#include <rtapi/screen_display.h>
#include <linux/backlight.h>


#include <linux/platform_device.h>
#include <linux/fb.h>

#include <linux/regulator/consumer.h>
#include <linux/lcd.h>
#include <mach/r8a7373.h>
#include <mach/memory-r8a7373.h>
#include <mach/common.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/clk.h>

#include "panel_common.h"

/* #define VX5B3D_DRAW_BLACK_KERNEL */
#define CABC_FUNCTION_ENABLE
#define BL_TUNE_WITH_TABLE
#define VX5B3D_POWAREA_MNG_ENABLE

#ifdef VX5B3D_POWAREA_MNG_ENABLE
#include <rtapi/system_pwmng.h>
#endif

#define VX5B3D_VEE_ENABLE
/* #define VX5B3D_STANDBY_CMD_ENABLE */

#define VX5B3D_USE_I2C


/* framebuffer address and size */
#define R_MOBILE_M_BUFF_ADDR		SDRAM_FRAME_BUFFER_START_ADDR
#define R_MOBILE_M_BUFF_SIZE		(SDRAM_FRAME_BUFFER_END_ADDR - \
					 SDRAM_FRAME_BUFFER_START_ADDR + 1)

/* panel size (mm) */
#define R_MOBILE_M_PANEL_SIZE_WIDTH	154
#define R_MOBILE_M_PANEL_SIZE_HEIGHT	90

/* panel pixel */
#define R_MOBILE_M_PANEL_PIXEL_WIDTH	 1024
#define R_MOBILE_M_PANEL_PIXEL_HEIGHT	 600

/* pixclock = 1000000 / DCF */
#define R_MOBILE_M_PANEL_PIXCLOCK	 13209

/* timing */
#define R_MOBILE_M_PANEL_LEFT_MARGIN	 225
#define R_MOBILE_M_PANEL_RIGHT_MARGIN	 737
#define R_MOBILE_M_PANEL_HSYNC_LEN	 30
#define R_MOBILE_M_PANEL_UPPER_MARGIN	 16
#define R_MOBILE_M_PANEL_LOWER_MARGIN	 16
#define R_MOBILE_M_PANEL_VSYNC_LEN	 3
#define R_MOBILE_M_PANEL_H_TOTAL	 2016
#define R_MOBILE_M_PANEL_V_TOTAL	 635

#define LCD_DSITCKCR		0x00000007
#define LCD_DSI0PCKCR		0x00000021
#define LCD_DSI0PHYCR		0x2A800020
#define LCD_SYSCONF		0x00000703
#define LCD_TIMSET0		0x4C2C6453
#define LCD_TIMSET1		0x000D00C2
#define LCD_DSICTRL		0x00000001
#define LCD_VMCTR1		0x0001003E
#define LCD_VMCTR2		0x00020700
#define LCD_VMLEN1		0x0C000000
#define LCD_VMLEN2		0x00000000
#define LCD_VMLEN3		0x00000000
#define LCD_VMLEN4		0x00000000
#define LCD_DTCTR		0x00000007
#define LCD_MLDHCNR		0x008000FC
#define LCD_MLDHSYNR		0x000300DC
#define LCD_MLDHAJR		0x00000601
#define LCD_MLDVLNR		0x0258027B
#define LCD_MLDVSYNR		0x00030269
#define LCD_MLDMT1R		0x0400000B
#define LCD_LDDCKR		0x00010040
#define LCD_MLDDCKPAT1R		0x00000000
#define LCD_MLDDCKPAT2R		0x00000000
#define LCD_PHYTEST		0x0000038C

#define LCD_MASK_DSITCKCR	0x000000BF
#define LCD_MASK_DSI0PCKCR	0x0000703F
#define LCD_MASK_DSI0PHYCR	0x000000FF
#define LCD_MASK_SYSCONF	0x00000F0F
#define LCD_MASK_TIMSET0	0x7FFFF7F7
#define LCD_MASK_TIMSET1	0x003F03FF
#define LCD_MASK_DSICTRL	0x00000601
#define LCD_MASK_VMCTR1		0x00F3F03F
#define LCD_MASK_VMCTR2		0x07E2073B
#define LCD_MASK_VMLEN1		0xFFFFFFFF
#define LCD_MASK_VMLEN2		0xFFFFFFFF
#define LCD_MASK_VMLEN3		0xFFFFFFFF
#define LCD_MASK_VMLEN4		0xFFFF0000
#define LCD_MASK_DTCTR		0x00000002
#define LCD_MASK_MLDHCNR	0x07FF07FF
#define LCD_MASK_MLDHSYNR	0x001F07FF
#define LCD_MASK_MLDHAJR	0x07070707
#define LCD_MASK_MLDVLNR	0x1FFF1FFF
#define LCD_MASK_MLDVSYNR	0x001F1FFF
#define LCD_MASK_MLDMT1R	0x1F03FCCF
#define LCD_MASK_LDDCKR		0x0007007F
#define LCD_MASK_MLDDCKPAT1R	0x0FFFFFFF
#define LCD_MASK_MLDDCKPAT2R	0xFFFFFFFF
#define LCD_MASK_PHYTEST	0x000003CC

#define LCD_DSI0PCKCR_40HZ	0x0000001F
#define LCD_DSI0PHYCR_40HZ	0x2A80000D

#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)
static int vx5b3d_panel_suspend(void);
static int vx5b3d_panel_resume(void);
/* i2c */
#if 0
static int ql_i2c_read(u32 addr, u32 *val, u32 data_size);
#endif
static int ql_i2c_write_wrapper(u8 cmd, int size, const u8 *data);


#define MIN_BRIGHTNESS	(0)
#define MAX_BRIGHTNESS	(255)
#define INIT_BRIGHTNESS	MAX_BRIGHTNESS

static struct fb_panel_info r_mobile_info = {
	.pixel_width	= R_MOBILE_M_PANEL_PIXEL_WIDTH,
	.pixel_height	= R_MOBILE_M_PANEL_PIXEL_HEIGHT,
	.size_width	= R_MOBILE_M_PANEL_SIZE_WIDTH,
	.size_height	= R_MOBILE_M_PANEL_SIZE_HEIGHT,
	.buff_address	= R_MOBILE_M_BUFF_ADDR,
	.pixclock      = R_MOBILE_M_PANEL_PIXCLOCK,
	.left_margin   = R_MOBILE_M_PANEL_LEFT_MARGIN,
	.right_margin  = R_MOBILE_M_PANEL_RIGHT_MARGIN,
	.upper_margin  = R_MOBILE_M_PANEL_UPPER_MARGIN,
	.lower_margin  = R_MOBILE_M_PANEL_LOWER_MARGIN,
	.hsync_len     = R_MOBILE_M_PANEL_HSYNC_LEN,
	.vsync_len     = R_MOBILE_M_PANEL_VSYNC_LEN,
};

static screen_disp_lcd_if r_mobile_lcd_if_param = {
	.dsitckcr    = LCD_DSITCKCR,
	.dsi0pckcr   = LCD_DSI0PCKCR,
	.dsi0phycr   = LCD_DSI0PHYCR,
	.sysconf     = LCD_SYSCONF,
	.timset0     = LCD_TIMSET0,
	.timset1     = LCD_TIMSET1,
	.dsictrl     = LCD_DSICTRL,
	.vmctr1      = LCD_VMCTR1,
	.vmctr2      = LCD_VMCTR2,
	.vmlen1      = LCD_VMLEN1,
	.vmlen2      = LCD_VMLEN2,
	.vmlen3      = LCD_VMLEN3,
	.vmlen4      = LCD_VMLEN4,
	.dtctr       = LCD_DTCTR,
	.mldhcnr     = LCD_MLDHCNR,
	.mldhsynr    = LCD_MLDHSYNR,
	.mldhajr     = LCD_MLDHAJR,
	.mldvlnr     = LCD_MLDVLNR,
	.mldvsynr    = LCD_MLDVSYNR,
	.mldmt1r     = LCD_MLDMT1R,
	.lddckr      = LCD_LDDCKR,
	.mlddckpat1r = LCD_MLDDCKPAT1R,
	.mlddckpat2r = LCD_MLDDCKPAT2R,
	.phytest     = LCD_PHYTEST,
};

static screen_disp_lcd_if r_mobile_lcd_if_param_mask = {
	LCD_MASK_DSITCKCR,
	LCD_MASK_DSI0PCKCR,
	LCD_MASK_DSI0PHYCR,
	LCD_MASK_SYSCONF,
	LCD_MASK_TIMSET0,
	LCD_MASK_TIMSET1,
	LCD_MASK_DSICTRL,
	LCD_MASK_VMCTR1,
	LCD_MASK_VMCTR2,
	LCD_MASK_VMLEN1,
	LCD_MASK_VMLEN2,
	LCD_MASK_VMLEN3,
	LCD_MASK_VMLEN4,
	LCD_MASK_DTCTR,
	LCD_MASK_MLDHCNR,
	LCD_MASK_MLDHSYNR,
	LCD_MASK_MLDHAJR,
	LCD_MASK_MLDVLNR,
	LCD_MASK_MLDVSYNR,
	LCD_MASK_MLDMT1R,
	LCD_MASK_LDDCKR,
	LCD_MASK_MLDDCKPAT1R,
	LCD_MASK_MLDDCKPAT2R,
	LCD_MASK_PHYTEST,
};

static unsigned int reset_gpio;
static unsigned int irq_portno;
static struct regulator *power_ldo_3v;
static struct regulator *power_ldo_1v8;
static struct regulator *power_ldo_1v2;
static struct clk *lvdsclk;

struct specific_cmdset {
	unsigned char cmd;
	unsigned char *data;
	int size;
};

#ifdef BL_TUNE_WITH_TABLE
struct brt_value {
	int level;		/* Platform setting values */
	int tune_level;		/* Chip Setting values */
};

/* TOTAL 95 STEPS */
struct brt_value bl_table[] = {
	{ 20,  1},	/* MIN 20 */
	{ 23,  5 },
	{ 25,  8 },
	{ 27, 10 },
	{ 30, 12 },
	{ 32, 13 },
	{ 35, 15 },
	{ 38, 17 },
	{ 40, 19 },
	{ 42, 21 },
	{ 45, 23 },
	{ 48, 25 },
	{ 50, 27 },
	{ 52, 28 },
	{ 55, 30 },
	{ 57, 32 },
	{ 60, 34 },
	{ 63, 36 },
	{ 65, 37 },
	{ 67, 39 },
	{ 70, 41 },
	{ 72, 43 },
	{ 75, 45 },
	{ 77, 47 },
	{ 80, 49 },
	{ 82, 51 },
	{ 85, 53 },
	{ 87, 55 },
	{ 90, 56 },
	{ 93, 57 },
	{ 95, 58 },
	{ 97, 60 },
	{ 100, 61 },
	{ 102, 62 },
	{ 105, 64 },
	{ 107, 66 },
	{ 110, 67 },
	{ 112, 68 },
	{ 115, 70 },
	{ 117, 71 },
	{ 120, 73 },
	{ 122, 74 },
	{ 125, 76 },
	{ 127, 78 },
	{ 130, 79 },
	{ 133, 82 },
	{ 135, 84 },
	{ 137, 88 },
	{ 140, 91 },
	{ 143, 94 },
	{ 145, 97 },
	{ 147, 100 },
	{ 150, 103 },
	{ 152, 106 },
	{ 155, 109 },
	{ 157, 112 },
	{ 160, 115 },
	{ 162, 118 },
	{ 165, 121 },
	{ 167, 124 },
	{ 170, 127 },
	{ 173, 130 },
	{ 175, 133 },	/* DEFAULT 175 */
	{ 177, 136 },
	{ 180, 139 },
	{ 182, 142 },
	{ 185, 145 },
	{ 187, 148 },
	{ 190, 151 },
	{ 192, 154 },
	{ 195, 157 },
	{ 197, 160 },
	{ 200, 163 },
	{ 203, 166 },
	{ 205, 169 },
	{ 207, 172 },
	{ 210, 175 },
	{ 213, 178 },
	{ 215, 180 },
	{ 217, 183 },
	{ 220, 186 },
	{ 223, 189 },
	{ 225, 192 },
	{ 227, 195 },
	{ 230, 197 },
	{ 233, 200 },
	{ 235, 203 },
	{ 237, 206 },
	{ 240, 209 },
	{ 242, 211 },
	{ 245, 214 },
	{ 247, 217 },
	{ 250, 219 },
	{ 252, 222 },
	{ 255, 225 }, /* MAX 255 */
};

#define MAX_BRT_STAGE (int)(sizeof(bl_table)/sizeof(struct brt_value))

int current_tune_level;

#endif


#define MIPI_DSI_DCS_LONG_WRITE		(0x39)
#define MIPI_DSI_GEN_LONG_WRITE		(0x29)
#define MIPI_DSI_DCS_SHORT_WRITE_PARAM	(0x15)
#define MIPI_DSI_GEN_SHORT_WRITE_PARAM	(0x23)
#define MIPI_DSI_DCS_SHORT_WRITE	(0x05)
#define MIPI_DSI_GEN_SHORT_WRITE	(0x03)
#define MIPI_DSI_SET_MAX_RETURN_PACKET	(0x37)
#define MIPI_DSI_DCS_READ		(0x06)
#define MIPI_DSI_DELAY			(0x00)
#define MIPI_DSI_BLACK			(0x01)
#define MIPI_DSI_END			(0xFF)


/* don't remove this. Need to restart the host after the init....... */
static unsigned char snd_data_100[] = { 0x05, 0x01, 0x40, 0x00, 0x03,
		0x00, 0x00, 0x00, 0x00 };
static unsigned char snd_data_101[] = { 0x05, 0x01, 0x40, 0x00, 0x03,
		0x01, 0x00, 0x00, 0x00 };
static unsigned char snd_data_102[] = { 0x05, 0x01, 0x40, 0x04, 0x03,
		0xff, 0xff, 0xff, 0xff };
static unsigned char snd_data_103[] = { 0x05, 0x01, 0x40, 0x04, 0x02,
		0xff, 0xff, 0xff, 0xff };
static unsigned char snd_data_104[] = { 0x05, 0x01, 0x40, 0x48, 0x01,
		0xff, 0xff, 0xff, 0xff };

/* for backlight */
static unsigned char snd_data_200[] = { 0x05, 0x01, 0x40, 0x14, 0x01,
		0x02, 0x63, 0x0C, 0x00 };
static unsigned char snd_data_201[] = { 0x05, 0x01, 0x40, 0x60, 0x01,
		0xFF, 0x00, 0x00, 0x00 };
static unsigned char snd_data_202[] = { 0x05, 0x01, 0x40, 0x64, 0x01,
		0x7F, 0x00, 0x00, 0x00 };
static unsigned char snd_data_203[] = { 0x05, 0x01, 0x40, 0x38, 0x01,
		0x00, 0x00, 0xFF, 0x3F };
static unsigned char snd_data_204[] = { 0x05, 0x01, 0x40, 0x5C, 0x01,
		0x05, 0x00, 0x00, 0x00 };

/* initialize sequence */
static unsigned char snd_data_001[] = { 0x05, 0x01, 0x40, 0x00, 0x07,
		0x40, 0x00, 0x90, 0x6C };
// plin@ql: HBP 130
#if 0  //0x704	0x302DB
static unsigned char snd_data_002[] = { 0x05, 0x01, 0x40, 0x04, 0x07,
		0xDB, 0x02, 0x03, 0x00 };
#else  //0x704	0x302D0
static unsigned char snd_data_002[] = { 0x05, 0x01, 0x40, 0x04, 0x07,
		0xD0, 0x02, 0x03, 0x00 };
#endif
static unsigned char snd_data_003[] = { 0x05, 0x01, 0x40, 0x0C, 0x07,
		0x04, 0x46, 0x00, 0x00 };
#ifdef VX5B3D_VEE_ENABLE
static unsigned char snd_data_004[] = { 0x05, 0x01, 0x40, 0x10, 0x07,
		0x4B, 0x00, 0x4D, 0x05 };
#else
static unsigned char snd_data_004[] = { 0x05, 0x01, 0x40, 0x10, 0x07,
		0x0B, 0x00, 0x4D, 0x05 };
#endif /* VX5B3D_VEE_ENABLE */
static unsigned char snd_data_005[] = { 0x05, 0x01, 0x40, 0x14, 0x07,
		0x20, 0x00, 0x00, 0x00 };
static unsigned char snd_data_006[] = { 0x05, 0x01, 0x40, 0x18, 0x07,
		0x02, 0x01, 0x00, 0x00 };
static unsigned char snd_data_007[] = { 0x05, 0x01, 0x40, 0x1C, 0x07,
		0x2F, 0x00, 0xA8, 0x00 };
static unsigned char snd_data_008[] = { 0x05, 0x01, 0x40, 0x20, 0x07,
		0x00, 0x00, 0x00, 0x00 };
static unsigned char snd_data_009[] = { 0x05, 0x01, 0x40, 0x54, 0x01,
		0x00, 0x00, 0x00, 0x00 };
static unsigned char snd_data_010[] = { 0x05, 0x01, 0x40, 0x54, 0x01,
		0x00, 0x00, 0x00, 0x80 };
static unsigned char snd_data_011[] = { 0x05, 0x01, 0x40, 0x00, 0x07,
		0x40, 0x08, 0x90, 0x6C };
static unsigned char snd_data_012[] = { 0x05, 0x01, 0x40, 0x0C, 0x07,
		0x56, 0x5E, 0x00, 0x00 };
static unsigned char snd_data_013[] = { 0x05, 0x01, 0x40, 0x18, 0x07,
		0x02, 0x02, 0x00, 0x00 };
static unsigned char snd_data_014[] = { 0x05, 0x01, 0x40, 0x54, 0x01,
		0x00, 0x00, 0x00, 0x00 };
static unsigned char snd_data_015[] = { 0x05, 0x01, 0x40, 0x54, 0x01,
		0x00, 0x00, 0x00, 0x80 };
static unsigned char snd_data_016[] = { 0x05, 0x01, 0x40, 0x20, 0x01,
		0x05, 0x00, 0x00, 0x00 };
static unsigned char snd_data_017[] = { 0x05, 0x01, 0x40, 0x24, 0x01,
		0x00, 0xC4, 0x12, 0x05 };
static unsigned char snd_data_018[] = { 0x05, 0x01, 0x40, 0x28, 0x01,
		0x10, 0x40, 0x10, 0x00 };
// plin@ql: HBP 130
#if 0  // 0x12C	0x93
static unsigned char snd_data_019[] = { 0x05, 0x01, 0x40, 0x2C, 0x01,
		0x93, 0x00, 0x00, 0x00 };
#else  // 0x12C	0x7F
static unsigned char snd_data_019[] = { 0x05, 0x01, 0x40, 0x2C, 0x01,
		0x7F, 0x00, 0x00, 0x00 };
#endif
static unsigned char snd_data_020[] = { 0x05, 0x01, 0x40, 0x30, 0x01,
		0x18, 0x3C, 0x00, 0x00 };
static unsigned char snd_data_021[] = { 0x05, 0x01, 0x40, 0x34, 0x01,
		0x15, 0x00, 0x00, 0x00 };
static unsigned char snd_data_022[] = { 0x05, 0x01, 0x40, 0x38, 0x01,
		0x00, 0x00, 0xFF, 0x00 };
static unsigned char snd_data_023[] = { 0x05, 0x01, 0x40, 0x3C, 0x01,
		0x00, 0x00, 0x00, 0x00 };
static unsigned char snd_data_024[] = { 0x05, 0x01, 0x40, 0x40, 0x01,
		0x00, 0x00, 0x01, 0x00 };
#ifdef VX5B3D_VEE_ENABLE
static unsigned char snd_data_025[] = { 0x05, 0x01, 0x40, 0x74, 0x01,
		0xFF, 0x00, 0x00, 0x00 };
#endif /* VX5B3D_VEE_ENABLE */

static unsigned char snd_data_026[] = { 0x05, 0x01, 0x40, 0x0C, 0x02,
		0x32, 0x01, 0x00, 0x00 };
static unsigned char snd_data_027[] = { 0x05, 0x01, 0x40, 0x1C, 0x02,
		0x9E, 0x07, 0x00, 0x00 };
static unsigned char snd_data_028[] = { 0x05, 0x01, 0x40, 0x24, 0x02,
		0x00, 0x00, 0x00, 0x00 };
static unsigned char snd_data_029[] = { 0x05, 0x01, 0x40, 0x28, 0x02,
		0x01, 0x00, 0x05, 0x00 };
static unsigned char snd_data_030[] = { 0x05, 0x01, 0x40, 0x2C, 0x02,
		0x03, 0xFF, 0x00, 0x00 };
static unsigned char snd_data_031[] = { 0x05, 0x01, 0x40, 0x30, 0x02,
		0x01, 0x00, 0x00, 0x00 };
static unsigned char snd_data_032[] = { 0x05, 0x01, 0x40, 0x34, 0x02,
		0x10, 0x3E, 0x03, 0xCA };
static unsigned char snd_data_033[] = { 0x05, 0x01, 0x40, 0x38, 0x02,
		0x60, 0x00, 0x00, 0x00 };
static unsigned char snd_data_034[] = { 0x05, 0x01, 0x40, 0x3C, 0x02,
		0x30, 0x60, 0xE8, 0x82 };
static unsigned char snd_data_035[] = { 0x05, 0x01, 0x40, 0x44, 0x02,
		0x85, 0x02, 0x1E, 0x00 };
static unsigned char snd_data_036[] = { 0x05, 0x01, 0x40, 0x58, 0x02,
		0x14, 0x00, 0x03, 0x00 };
static unsigned char snd_data_037[] = { 0x05, 0x01, 0x40, 0x58, 0x01,
		0x00, 0x00, 0x00, 0x00 };
static unsigned char snd_data_038[] = { 0x05, 0x01, 0x40, 0x58, 0x01,
		0x01, 0x00, 0x00, 0x00 };
static unsigned char snd_data_039[] = { 0x05, 0x01, 0x40, 0x7C, 0x03,
		0x63, 0x10, 0x00, 0x00 };
static unsigned char snd_data_040[] = { 0x05, 0x01, 0x40, 0x80, 0x03,
		0x30, 0x60, 0xA8, 0x82 };
static unsigned char snd_data_041[] = { 0x05, 0x01, 0x40, 0x84, 0x03,
		0x8B, 0x40, 0x61, 0x28 };
static unsigned char snd_data_042[] = { 0x05, 0x01, 0x40, 0x88, 0x03,
		0x85, 0x02, 0x13, 0x00 };
static unsigned char snd_data_043[] = { 0x05, 0x01, 0x40, 0x8C, 0x03,
		0x09, 0x00, 0x63, 0x10 };
static unsigned char snd_data_044[] = { 0x05, 0x01, 0x40, 0x94, 0x03,
		0xA8, 0x82, 0x0B, 0x40 };
static unsigned char snd_data_045[] = { 0x05, 0x01, 0x40, 0x00, 0x06,
		0x8C, 0xC7, 0x6C, 0x01 };
static unsigned char snd_data_046[] = { 0x05, 0x01, 0x40, 0x04, 0x06,
		0xE0, 0xFF, 0xFF, 0x3F };
static unsigned char snd_data_047[] = { 0x05, 0x01, 0x40, 0x08, 0x06,
		0x8C, 0x0D, 0x00, 0x00 };
static unsigned char snd_data_048[] = { 0x05, 0x01, 0x40, 0x54, 0x01,
		0x00, 0x00, 0x00, 0x00 };
static unsigned char snd_data_049[] = { 0x05, 0x01, 0x40, 0x54, 0x01,
		0x00, 0x00, 0x00, 0x80 };

static const struct specific_cmdset initialize_cmdset[] = {
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_001, sizeof(snd_data_001) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_002, sizeof(snd_data_002) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_003, sizeof(snd_data_003) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_004, sizeof(snd_data_004) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_005, sizeof(snd_data_005) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_006, sizeof(snd_data_006) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_007, sizeof(snd_data_007) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_008, sizeof(snd_data_008) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_009, sizeof(snd_data_009) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_010, sizeof(snd_data_010) },
	{ MIPI_DSI_DELAY,          NULL,         1                    },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_011, sizeof(snd_data_011) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_012, sizeof(snd_data_012) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_013, sizeof(snd_data_013) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_014, sizeof(snd_data_014) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_015, sizeof(snd_data_015) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_016, sizeof(snd_data_016) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_017, sizeof(snd_data_017) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_018, sizeof(snd_data_018) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_019, sizeof(snd_data_019) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_020, sizeof(snd_data_020) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_021, sizeof(snd_data_021) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_022, sizeof(snd_data_022) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_023, sizeof(snd_data_023) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_024, sizeof(snd_data_024) },
#ifdef VX5B3D_VEE_ENABLE
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_025, sizeof(snd_data_025) },
#endif /* VX5B3D_VEE_ENABLE */
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_026, sizeof(snd_data_026) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_027, sizeof(snd_data_027) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_028, sizeof(snd_data_028) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_029, sizeof(snd_data_029) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_030, sizeof(snd_data_030) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_031, sizeof(snd_data_031) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_032, sizeof(snd_data_032) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_033, sizeof(snd_data_033) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_034, sizeof(snd_data_034) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_035, sizeof(snd_data_035) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_036, sizeof(snd_data_036) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_037, sizeof(snd_data_037) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_038, sizeof(snd_data_038) },
	{ MIPI_DSI_DELAY,          NULL,         1                    },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_039, sizeof(snd_data_039) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_040, sizeof(snd_data_040) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_041, sizeof(snd_data_041) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_042, sizeof(snd_data_042) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_043, sizeof(snd_data_043) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_044, sizeof(snd_data_044) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_045, sizeof(snd_data_045) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_046, sizeof(snd_data_046) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_047, sizeof(snd_data_047) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_048, sizeof(snd_data_048) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_049, sizeof(snd_data_049) },
	{ MIPI_DSI_DELAY,          NULL,         1                    },

	/* don't remove this. Need to restart the host after the init....... */
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_100, sizeof(snd_data_100) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_101, sizeof(snd_data_101) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_102, sizeof(snd_data_102) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_103, sizeof(snd_data_103) },
	{ MIPI_DSI_GEN_LONG_WRITE, snd_data_104, sizeof(snd_data_104) },

	{ MIPI_DSI_DELAY,           NULL,          10                   },
	{ MIPI_DSI_BLACK,           NULL,          0                    },

	/* for backlight */
	{ MIPI_DSI_DELAY,           NULL,          10                   },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_data_200,  sizeof(snd_data_200) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_data_201,  sizeof(snd_data_201) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_data_202,  sizeof(snd_data_202) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_data_203,  sizeof(snd_data_203) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_data_204,  sizeof(snd_data_204) },

	{ MIPI_DSI_END,             NULL,          0                    }
};

#ifdef VX5B3D_STANDBY_CMD_ENABLE
/* for standby register setting */
static unsigned char snd_stby_001[] = { 0x05, 0x01, 0x40, 0x00, 0x07,
		0x40, 0x00, 0x00, 0x24 };
static unsigned char snd_stby_002[] = { 0x05, 0x01, 0x40, 0x04, 0x07,
		0x09, 0x00, 0x9F, 0x00 };
static unsigned char snd_stby_003[] = { 0x05, 0x01, 0x40, 0x08, 0x07,
		0x00, 0x00, 0x00, 0x00 };
static unsigned char snd_stby_004[] = { 0x05, 0x01, 0x40, 0x0C, 0x07,
		0x00, 0x06, 0x00, 0x00 };
static unsigned char snd_stby_005[] = { 0x05, 0x01, 0x40, 0x10, 0x07,
		0x02, 0x00, 0x0C, 0x00 };
static unsigned char snd_stby_006[] = { 0x05, 0x01, 0x40, 0x14, 0x07,
		0x20, 0x00, 0x00, 0x00 };
static unsigned char snd_stby_007[] = { 0x05, 0x01, 0x40, 0x18, 0x07,
		0x09, 0x03, 0x00, 0x00 };
static unsigned char snd_stby_008[] = { 0x05, 0x01, 0x40, 0x54, 0x01,
		0x00, 0x00, 0x00, 0x00 };
static unsigned char snd_stby_009[] = { 0x05, 0x01, 0x40, 0x54, 0x01,
		0x00, 0x00, 0x00, 0x80 };
static unsigned char snd_stby_010[] = { 0x05, 0x01, 0x40, 0x48, 0x02,
		0x09, 0x00, 0x63, 0x10 };
static unsigned char snd_stby_011[] = { 0x05, 0x01, 0x40, 0x78, 0x03,
		0x90, 0x3C, 0x0F, 0xCA };
static unsigned char snd_stby_012[] = { 0x05, 0x01, 0x40, 0x7C, 0x03,
		0x60, 0x00, 0x00, 0x00 };
static unsigned char snd_stby_013[] = { 0x05, 0x01, 0x40, 0x8C, 0x03,
		0x09, 0x00, 0x63, 0x10 };
static unsigned char snd_stby_014[] = { 0x05, 0x01, 0x40, 0x08, 0x06,
		0x0F, 0x05, 0x00, 0x00 };

static unsigned char snd_data_300[] = { 0x05, 0x01, 0x40, 0x5C, 0x01,
		0x01, 0x00, 0x00, 0x00 };

static const struct specific_cmdset standby_cmdset[] = {
/* for backlight */
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_data_300,  sizeof(snd_data_300) },
	{ MIPI_DSI_DELAY,           NULL,          10                   },

/* for standby register setting */
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_001,  sizeof(snd_stby_001) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_002,  sizeof(snd_stby_002) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_003,  sizeof(snd_stby_003) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_004,  sizeof(snd_stby_004) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_005,  sizeof(snd_stby_005) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_006,  sizeof(snd_stby_006) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_007,  sizeof(snd_stby_007) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_008,  sizeof(snd_stby_008) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_009,  sizeof(snd_stby_009) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_010,  sizeof(snd_stby_010) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_011,  sizeof(snd_stby_011) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_012,  sizeof(snd_stby_012) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_013,  sizeof(snd_stby_013) },
	{ MIPI_DSI_GEN_LONG_WRITE,  snd_stby_014,  sizeof(snd_stby_014) },
	{ MIPI_DSI_END,             NULL,          0                    }
};
#endif /* VX5B3D_STANDBY_CMD_ENABLE */

static int is_dsi_read_enabled;
static struct backlight_device *registed_bd;
static int is_panel_initialized;
static int is_backlight_called;

struct specific_cmdset const *init_cmdset = initialize_cmdset;

static struct fb_info *common_fb_info;
static int panel_specific_cmdset(void *lcd_handle,
				   const struct specific_cmdset *cmdset);
enum lcdfreq_level_idx {
	LEVEL_NORMAL,		/* 60Hz */
	LEVEL_LOW,		/* 40Hz */
	LCDFREQ_LEVEL_END
};

struct lcd_info {
	enum lcdfreq_level_idx	level;	/* Current level */
	struct mutex		lock;	/* Lock for change frequency */
	struct device		*dev;	/* Hold device of LCD */
	struct device_attribute	*attr;	/* Hold attribute info */
	struct lcd_device	*ld;	/* LCD device info */

	unsigned int			ldi_enable;
	unsigned int			power;
};

static struct lcd_info lcd_info_data;

#ifdef CABC_FUNCTION_ENABLE
enum OUTDOOR {
	OUTDOOR_OFF,
	OUTDOOR_ON,
	OUTDOOR_MAX,
};

enum CABC {
	CABC_OFF,
	CABC_ON,
	CABC_MAX,
};

enum POWER_LUT_LEVEL {
	LUT_LEVEL_MANUAL_AND_INDOOR,
	LUT_LEVEL_OUTDOOR_1,
	LUT_LEVEL_OUTDOOR_2,
	LUT_LEVEL_MAX,
};

struct Vx5b3d_backlight_value {
	const unsigned int max;
	const unsigned int mid;
	const unsigned char low;
	const unsigned char dim;
};

struct Vx5d3b_cabc_info {
	enum OUTDOOR			outdoor;
	enum CABC			cabc;
	enum POWER_LUT_LEVEL		powerLut;

	struct backlight_device		*bd;
	struct lcd_device		*lcd;
	struct Vx5b3d_backlight_value	*vee_lightValue;
	struct device			*dev;
	struct mutex			lock;

	unsigned int			auto_brightness;
	unsigned int			power_lut_idx;
	unsigned int			vee_strenght;
};

/** Unused currently.
static struct Vx5b3d_backlight_value backlight_table[1] = {
	{
		.max = 236,
		.mid = 140,
		.low = 10,
		.dim = 10,
	}
};

struct Vx5b3d_backlight_value *pwm;
**/
struct class *mdnie_class;
struct Vx5d3b_cabc_info *g_vx5d3b;

#define V5D3BX_VEESTRENGHT		0x00001f07
#define V5D3BX_VEEDEFAULTVAL		7		/* 0x38 */
#define V5D3BX_DEFAULT_STRENGHT		10		/* 0x50 */
#define V5D3BX_DEFAULT_LOW_STRENGHT	11		/* 0x58 */
#define V5D3BX_DEFAULT_HIGH_STRENGHT	12		/* 0x60 */
#define V5D3BX_MAX_STRENGHT		15		/* 0x78 */

#define V5D3BX_CABCBRIGHTNESSRATIO	815

static unsigned char snd_cabc_on[] = { 0x05, 0x01, 0x40, 0x00, 0x04,
		0x07, 0x1f, 0x00, 0x60 };

static unsigned char snd_cabc_off[] = { 0x05, 0x01, 0x40, 0x00, 0x04,
		0x07, 0x1f, 0x00, 0x00 };

static int vx5b3d_update_cabc_ctrl(int onoff)
{
#ifndef VX5B3D_USE_I2C
	screen_disp_delete disp_delete;
	screen_disp_write_dsi_long  write_dsi_l;
	void *screen_handle;
#endif
	unsigned short data_count;
	unsigned char *write_data;

	int ret;

	if (!is_panel_initialized) {
		printk(KERN_DEBUG
			"vx5b3d_update_cabc_ctrl rejected. "
			"just a warning, not a real error.\n");
		return 0;
	}

	printk("%s on_off:%d\n", __func__, onoff);

	if (onoff) {
		data_count = sizeof(snd_cabc_on);
		write_data = snd_cabc_on;
	} else {
		data_count = sizeof(snd_cabc_off);
		write_data = snd_cabc_off;
	}

#ifdef VX5B3D_USE_I2C
	ret = ql_i2c_write_wrapper(
		MIPI_DSI_GEN_LONG_WRITE,
		data_count,
		write_data);
	if (ret)
		printk(KERN_ERR "ql_i2c_write_wrapper err:%d\n", ret);

#else
	screen_handle = screen_display_new();

	/* set brightness */
	write_dsi_l.handle	= screen_handle;
	write_dsi_l.output_mode = RT_DISPLAY_LCD1;
	write_dsi_l.data_id     = MIPI_DSI_GEN_LONG_WRITE;
	write_dsi_l.data_count  = data_count;
	write_dsi_l.write_data  = write_data;
	write_dsi_l.send_mode   = RT_DISPLAY_SEND_MODE_HS;
	write_dsi_l.reception_mode = RT_DISPLAY_RECEPTION_OFF;
	ret = screen_display_write_dsi_long_packet(&write_dsi_l);
	if (ret != SMAP_LIB_DISPLAY_OK)
		printk(KERN_ERR "write_dsi_long_packet err:%d\n", ret);

	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);
#endif

	return ret;
}

static ssize_t cabc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;

	return sprintf(buf, "%d\n", Vee_cabc->cabc);
}

static ssize_t cabc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
	unsigned int value = 0;
	int ret = 0;

	ret = kstrtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (value >= CABC_MAX)
		value = CABC_OFF;

	value = (value) ? CABC_ON : CABC_OFF;

	mutex_lock(&Vee_cabc->lock);
	Vee_cabc->cabc = value;
	vx5b3d_update_cabc_ctrl(Vee_cabc->cabc);
	mutex_unlock(&Vee_cabc->lock);

	return count;
}

static struct device_attribute mdnie_attributes[] = {
	__ATTR(cabc, 0664, cabc_show, cabc_store),
	__ATTR_NULL,
};
#endif


static int lcdfreq_lock_free(struct device *dev)
{
	void *screen_handle;
	screen_disp_set_lcd_if_param set_lcd_if_param;
	screen_disp_delete disp_delete;
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);

	screen_handle =  screen_display_new();

	/* Setting peculiar to panel */
	set_lcd_if_param.handle			= screen_handle;
	set_lcd_if_param.port_no		= irq_portno;
	set_lcd_if_param.lcd_if_param		= &r_mobile_lcd_if_param;
	set_lcd_if_param.lcd_if_param_mask	= &r_mobile_lcd_if_param_mask;
	ret = screen_display_set_lcd_if_parameters(&set_lcd_if_param);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_set_lcd_if_parameters err!\n");
		goto out;
	}

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	return ret;
}


static ssize_t level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	printk(KERN_DEBUG "%s\n", __func__);
	return sprintf(buf, "%d\n", lcd_info_data.level);
}

static ssize_t level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);

	mutex_lock(&lcd_info_data.lock);

	ret = kstrtoul(buf, 0, (unsigned long *)&value);

	printk(KERN_DEBUG "\t%s :: value=%d\n", __func__, value);

	if (value >= LCDFREQ_LEVEL_END) {
		count = -EINVAL;
		goto out;
	}

	if (value) {
		/* set freq 40Hz */
		printk(KERN_ALERT "set low freq(40Hz)\n");

		r_mobile_lcd_if_param.DSI0PCKCR = LCD_DSI0PCKCR_40HZ;
		r_mobile_lcd_if_param.DSI0PHYCR = LCD_DSI0PHYCR_40HZ;
	} else {
		/* set freq 60Hz */
		printk(KERN_ALERT "set normal freq(60Hz)\n");

		r_mobile_lcd_if_param.DSI0PCKCR = LCD_DSI0PCKCR;
		r_mobile_lcd_if_param.DSI0PHYCR = LCD_DSI0PHYCR;
	}

	ret = lcdfreq_lock_free(dev);
	if (ret) {
		printk(KERN_ALERT "%s fail\n", __func__);
		count = -EINVAL;
		goto out;
	}

	lcd_info_data.level	= value;
	lcd_info_data.dev	= dev;
	lcd_info_data.attr	= attr;
out:
	mutex_unlock(&lcd_info_data.lock);

	return count;
}

static DEVICE_ATTR(level, S_IRUGO|S_IWUSR, level_show, level_store);

static struct attribute *lcdfreq_attributes[] = {
	&dev_attr_level.attr,
	NULL,
};

static struct attribute_group lcdfreq_attr_group = {
	.name = "lcdfreq",
	.attrs = lcdfreq_attributes,
};

static int vx5b3d_lcd_frequency_register(struct device *dev)
{
	struct lcd_info *lcdfreq = NULL;
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);

	lcdfreq = &lcd_info_data;
	lcdfreq->dev = dev;
	lcdfreq->level = LEVEL_NORMAL;

	mutex_init(&lcdfreq->lock);

	ret = sysfs_create_group(&lcdfreq->dev->kobj, &lcdfreq_attr_group);
	if (ret < 0) {
		printk(KERN_ALERT "fail to add sysfs entries, %d\n", __LINE__);

		return ret;
	}

	printk(KERN_DEBUG "%s is done\n", __func__);

	return 0;
}

static void vx5b3d_lcd_frequency_unregister(void)
{
	printk(KERN_DEBUG "%s\n", __func__);

	sysfs_remove_group(&lcd_info_data.dev->kobj,
					&lcdfreq_attr_group);
	mutex_destroy(&lcd_info_data.lock);

	printk(KERN_DEBUG "%s is done\n", __func__);

}


static int vx5b3d_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ret = vx5b3d_panel_resume();

	return ret;
}

static int vx5b3d_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ret = vx5b3d_panel_suspend();

	msleep(135);

	return ret;
}

static int vx5b3d_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = vx5b3d_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = vx5b3d_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int vx5b3d_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return vx5b3d_power(lcd, power);
}

static int vx5b3d_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static struct lcd_ops vx5b3d_lcd_ops = {
	.set_power = vx5b3d_set_power,
	.get_power = vx5b3d_get_power,
};


static int vx5b3d_update_brightness_ctrl(int brightness)
{
#ifndef VX5B3D_USE_I2C
	screen_disp_delete disp_delete;
	screen_disp_write_dsi_long  write_dsi_l;
	void *screen_handle;
#endif
	int ret;

	#ifdef BL_TUNE_WITH_TABLE
	int tune_level = 0, i;
	#endif

	unsigned char brightness_cmd[] = { 0x05, 0x01, 0x40, 0x64, 0x01,
		0x00, 0x00, 0x00, 0x00 };


	if (!is_panel_initialized) {
		printk(KERN_DEBUG
			"vx5b3d_update_brightness_ctrl rejected. "
			"just a warning, not a real error.\n");
		return 0;
	}

#ifdef VX5B3D_USE_I2C
	printk(KERN_DEBUG "%s brightness(i2c):%d\n", __func__, brightness);
#else
	printk(KERN_DEBUG "%s brightness(dsi):%d\n", __func__, brightness);
#endif


#ifdef BL_TUNE_WITH_TABLE
	for (i = 0; i < MAX_BRT_STAGE; i++) {
		if (brightness <= bl_table[i].level) {
			tune_level = bl_table[i].tune_level;
			break;
		}
	}
	/**
	printk(KERN_DEBUG "bl = %d , tune_level = %d\n", bl, tune_level);

	if (current_tune_level == tune_level && tune_level != 3)
		return 0;
	**/

	printk(KERN_INFO "brightness = %d , tune_level = %d\n",
					brightness, tune_level);

	brightness_cmd[5] = (char)tune_level;

	current_tune_level = tune_level;
#else

	brightness_cmd[5] = brightness;
#endif


#ifdef VX5B3D_USE_I2C
	ret = ql_i2c_write_wrapper(
		MIPI_DSI_GEN_LONG_WRITE,
		sizeof(brightness_cmd),
		brightness_cmd);
	if (ret)
		printk(KERN_ERR "ql_i2c_write_wrapper err:%d\n", ret);
#else
	screen_handle = screen_display_new();

	/* set brightness */
	write_dsi_l.handle	= screen_handle;
	write_dsi_l.output_mode = RT_DISPLAY_LCD1;
	write_dsi_l.data_id     = MIPI_DSI_GEN_LONG_WRITE;
	write_dsi_l.data_count  = sizeof(brightness_cmd);
	write_dsi_l.write_data  = brightness_cmd;
	write_dsi_l.send_mode   = RT_DISPLAY_SEND_MODE_HS;
	write_dsi_l.reception_mode = RT_DISPLAY_RECEPTION_OFF;
	ret = screen_display_write_dsi_long_packet(&write_dsi_l);
	if (ret != SMAP_LIB_DISPLAY_OK)
		printk(KERN_ERR "write_dsi_long_packet err:%d\n", ret);

	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);
#endif

	return ret;
}

static int get_brightness(struct backlight_device *bd)
{
	printk(KERN_DEBUG "brightness:%d\n", bd->props.brightness);
	return bd->props.brightness;
}

static int set_brightness(struct backlight_device *bd)
{
	int ret = 0, brightness = bd->props.brightness;

	is_backlight_called = 1;

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		printk(KERN_WARNING "lcd brightness should be %d to %d.\n",
		MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		return -EINVAL;
	}
	ret = vx5b3d_update_brightness_ctrl(bd->props.brightness);
	if (ret) {
		printk(KERN_ERR "lcd brightness setting failed.\n");
		return -EIO;
	}

	return ret;
}

static const struct backlight_ops vx5b3d_backlight_ops  = {
	.get_brightness = get_brightness,
	.update_status = set_brightness,
};

#ifdef CABC_FUNCTION_ENABLE
static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;

	return sprintf(buf, "%d\n", Vee_cabc->auto_brightness);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
	unsigned int value = 0;
	int ret = 0;

	ret = kstrtoul(buf, 0, (unsigned long *)&value);

	if (ret < 0)
		return ret;
	else {

		if (Vee_cabc->auto_brightness != value) {
			dev_info(dev, "%s - %d, %d\n", __func__,
				Vee_cabc->auto_brightness, value);
			Vee_cabc->auto_brightness = value;
			if (Vee_cabc->auto_brightness == 0) {
				Vee_cabc->cabc = 0;
				vx5b3d_update_cabc_ctrl(0);
			} else if ((Vee_cabc->auto_brightness >= 1) &&
					(Vee_cabc->auto_brightness < 5)) {
				Vee_cabc->cabc = 1;
				vx5b3d_update_cabc_ctrl(1);
			} else if (Vee_cabc->auto_brightness >= 5) {
				Vee_cabc->cabc = 0;
				vx5b3d_update_cabc_ctrl(0);
			}
		}
	}

	return size;

}

static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show,
						auto_brightness_store);

#endif

static int vx5b3d_backlight_device_register(struct device *dev)
{
	struct backlight_device *bd;
#ifdef CABC_FUNCTION_ENABLE
	int ret;
#endif

	is_backlight_called = 0;

	bd = backlight_device_register("panel",
		dev, NULL, &vx5b3d_backlight_ops, NULL);
	if (IS_ERR(bd)) {
		printk(KERN_ERR "backlight_device_register err\n");
		return PTR_ERR(bd);
	}

#ifdef CABC_FUNCTION_ENABLE
	ret = device_create_file(&bd->dev, &dev_attr_auto_brightness);

	if (ret < 0)
		dev_err(dev, "failed to add sysfs entries\n");
#endif

	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = INIT_BRIGHTNESS;
	registed_bd = bd;
	printk(KERN_INFO "vx5b3d Backlight Driver Initialized\n");

	return 0;
}

static void vx5b3d_backlight_device_unregister(void)
{
	if (registed_bd)
		backlight_device_unregister(registed_bd);

	printk(KERN_INFO "vx5b3d Backlight Driver Terminated\n");
}


int panel_dsi_read(int id, int reg, int len, char *buf)
{
	void *screen_handle;
	screen_disp_write_dsi_short write_dsi_s;
	screen_disp_read_dsi_short read_dsi_s;
	screen_disp_delete disp_delete;
	int ret = 0;

	printk(KERN_INFO "%s\n", __func__);

	if (!is_dsi_read_enabled) {
		printk(KERN_ALERT "sequence error!!\n");
		return -EINVAL;
	}

	if ((len <= 0) || (len > 60) || (buf == NULL)) {
		printk(KERN_ALERT "argument error!!\n");
		return -EINVAL;
	}

	screen_handle =  screen_display_new();

	/* Set maximum return packet size  */
	write_dsi_s.handle		= screen_handle;
	write_dsi_s.output_mode	= RT_DISPLAY_LCD1;
	write_dsi_s.data_id		= MIPI_DSI_SET_MAX_RETURN_PACKET;
	write_dsi_s.reg_address	= len;
	write_dsi_s.write_data		= 0x00;
	write_dsi_s.reception_mode      = RT_DISPLAY_RECEPTION_ON;
	ret = screen_display_write_dsi_short_packet(&write_dsi_s);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_write_dsi_short err!\n");
		goto out;
	}

	/* DSI read */
	read_dsi_s.handle		= screen_handle;
	read_dsi_s.output_mode		= RT_DISPLAY_LCD1;
	read_dsi_s.data_id		= id;
	read_dsi_s.reg_address		= reg;
	read_dsi_s.write_data		= 0;
	read_dsi_s.data_count		= len;
	read_dsi_s.read_data		= &buf[0];
	ret = screen_display_read_dsi_short_packet(&read_dsi_s);
	if (ret != SMAP_LIB_DISPLAY_OK)
		printk(KERN_ALERT "disp_dsi_read err! ret = %d\n", ret);

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	return ret;
}

static int vx5b3d_panel_draw_black(void *screen_handle)
{
	u32 panel_width  = R_MOBILE_M_PANEL_PIXEL_WIDTH;
	u32 panel_height = R_MOBILE_M_PANEL_PIXEL_HEIGHT;
	screen_disp_draw disp_draw;
	int ret;

#ifdef VX5B3D_DRAW_BLACK_KERNEL
	printk(KERN_ALERT
		"num_registered_fb = %d\n", num_registered_fb);

	if (!num_registered_fb) {
		printk(KERN_ALERT
			"num_registered_fb err!\n");
		return -1;
	}
	if (!registered_fb[0]->fix.smem_start) {
		printk(KERN_ALERT
			"registered_fb[0]->fix.smem_start"
			" is NULL err!\n");
		return -1;
	}
	printk(KERN_INFO
	       "registerd_fb[0]-> fix.smem_start: %08x\n"
	       "screen_base :%08x\n"
	       "fix.smem_len :%08x\n",
	       (unsigned)(registered_fb[0]->fix.smem_start),
	       (unsigned)(registered_fb[0]->screen_base),
	       (unsigned)(registered_fb[0]->fix.smem_len));
	memset(registered_fb[0]->screen_base, 0x0,
			registered_fb[0]->fix.smem_len);
#endif

	/* Memory clean */
	disp_draw.handle = screen_handle;
#ifdef VX5B3D_DRAW_BLACK_KERNEL
	disp_draw.output_mode = RT_DISPLAY_LCD1;
	disp_draw.buffer_id   = RT_DISPLAY_BUFFER_A;
#else
	disp_draw.output_mode = RT_DISPLAY_LCD1_ASYNC;
	disp_draw.buffer_id   = RT_DISPLAY_DRAW_BLACK;
#endif
	disp_draw.draw_rect.x = 0;
	disp_draw.draw_rect.y = 0;
	disp_draw.draw_rect.width  = panel_width;
	disp_draw.draw_rect.height = panel_height;
#ifdef CONFIG_FB_SH_MOBILE_RGB888
	disp_draw.format = RT_DISPLAY_FORMAT_RGB888;
#else
	disp_draw.format = RT_DISPLAY_FORMAT_ARGB8888;
#endif
	disp_draw.buffer_offset = 0;
	disp_draw.rotate = RT_DISPLAY_ROTATE_270;
	ret = screen_display_draw(&disp_draw);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "screen_display_draw err!\n");
		return -1;
	}

	return 0;
}

static int panel_specific_cmdset(void *lcd_handle,
				   const struct specific_cmdset *cmdset)
{
	int ret;
	int loop = 0;
	screen_disp_write_dsi_short write_dsi_s;
	screen_disp_write_dsi_long  write_dsi_l;

	printk(KERN_INFO "%s\n", __func__);

	while (0 <= loop) {
		switch (cmdset[loop].cmd) {
		case MIPI_DSI_DCS_LONG_WRITE:
		case MIPI_DSI_GEN_LONG_WRITE:
#ifdef CONFIG_RENESAS
			printk(KERN_INFO "panel_cmdset LONG Write\n");
#endif
			write_dsi_l.handle         = lcd_handle;
			write_dsi_l.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_l.data_id        = cmdset[loop].cmd;
			write_dsi_l.data_count     = cmdset[loop].size;
			write_dsi_l.write_data     = cmdset[loop].data;

			/* when done series registers are reception off. */
			if (((cmdset[loop].data[3] == 0x54) &&
					(cmdset[loop].data[4] == 0x01)) ||
			    ((cmdset[loop].data[3] == 0x58) &&
					(cmdset[loop].data[4] == 0x01)))
				write_dsi_l.reception_mode
						= RT_DISPLAY_RECEPTION_OFF;
			else
				write_dsi_l.reception_mode
						= RT_DISPLAY_RECEPTION_ON;

			/* send hs mode in operation */
			if (is_panel_initialized)
				write_dsi_l.send_mode = RT_DISPLAY_SEND_MODE_HS;
			else
				write_dsi_l.send_mode = RT_DISPLAY_SEND_MODE_LP;
			ret = screen_display_write_dsi_long_packet(
					&write_dsi_l);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT
				       "display_write_dsi_long err %d!\n", ret);
				return -1;
			}
			break;
		case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
		case MIPI_DSI_GEN_SHORT_WRITE_PARAM:
			printk(KERN_INFO
			       "panel_cmdset SHORT Write with param\n");
			write_dsi_s.handle         = lcd_handle;
			write_dsi_s.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_s.data_id        = cmdset[loop].cmd;
			write_dsi_s.reg_address    = cmdset[loop].data[0];
			write_dsi_s.write_data     = cmdset[loop].data[1];
			write_dsi_s.reception_mode = RT_DISPLAY_RECEPTION_ON;
			ret = screen_display_write_dsi_short_packet(
					&write_dsi_s);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT
				       "disp_write_dsi_short err %d!\n", ret);
				return -1;
			}
			break;
		case MIPI_DSI_DCS_SHORT_WRITE:
		case MIPI_DSI_GEN_SHORT_WRITE:
			printk(KERN_INFO "panel_cmdset SHORT Write\n");
			write_dsi_s.handle         = lcd_handle;
			write_dsi_s.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_s.data_id        = cmdset[loop].cmd;
			write_dsi_s.reg_address    = cmdset[loop].data[0];
			write_dsi_s.write_data     = 0x00;
			write_dsi_s.reception_mode = RT_DISPLAY_RECEPTION_ON;
			ret = screen_display_write_dsi_short_packet(
					&write_dsi_s);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT
				       "disp_write_dsi_short err %d!\n", ret);
				return -1;
			}
			break;
		case MIPI_DSI_BLACK:
		{
			ret = vx5b3d_panel_draw_black(lcd_handle);
			if (ret != 0)
				return -1;
			break;
		}

		case MIPI_DSI_DELAY:
			msleep(cmdset[loop].size);
			break;

		case MIPI_DSI_END:
			loop = -2;
			break;
		default:
			printk(KERN_ALERT "Undefine command err!\n");
			return -1;
		}
		loop++;
	}

	return 0;
}

static void mipi_display_reset(void)
{
	int ret;

	printk(KERN_INFO "%s\n", __func__);

	/* GPIO control */
	gpio_request(reset_gpio, NULL);

	/* LCD_BLIC_ON GPIO control */
	gpio_request(GPIO_PORT47, NULL);
	gpio_direction_output(GPIO_PORT47, 1);

	/* LVDS clock */
	ret = gpio_request(GPIO_FN_VIO_CKO5, NULL);
	if (ret == -EBUSY) {
		gpio_free(GPIO_FN_VIO_CKO5);
		ret = gpio_request(GPIO_FN_VIO_CKO5, NULL);
	}
	if (ret) {
		printk(KERN_ALERT "Request CKO5 fail(%d)\n", ret);
		return;
	}
	if (lvdsclk)
		clk_put(lvdsclk);
	lvdsclk = clk_get(NULL, "vclk5_clk");
	if (IS_ERR(lvdsclk)) {
		printk(KERN_ALERT "cannot get vclk5(%ld)\n", PTR_ERR(lvdsclk));
		return;
	}
	ret = clk_set_rate(lvdsclk, 19500000);
	if (ret) {
		printk(KERN_ALERT "vclk5 set rate fail(%d)\n", ret);
		return;
	}
	ret = clk_enable(lvdsclk);
	if (ret) {
		printk(KERN_ALERT "vclk5 fail to enable(%d)\n", ret);
		return;
	}

	regulator_enable(power_ldo_1v8);
	regulator_enable(power_ldo_3v);
	regulator_enable(power_ldo_1v2);

	gpio_direction_output(reset_gpio, 0);
	msleep(100);
	gpio_direction_output(reset_gpio, 1);
	msleep(100);
}


static int vx5b3d_panel_init(unsigned int mem_size)
{
	void *screen_handle;
	screen_disp_start_lcd start_lcd;
	screen_disp_set_lcd_if_param set_lcd_if_param;
	screen_disp_set_address set_address;
	screen_disp_delete disp_delete;
	int ret = 0;
	int retry_count = 10;

#ifdef VX5B3D_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif
#ifdef CABC_FUNCTION_ENABLE
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
#endif

	printk(KERN_INFO "%s\n", __func__);


#ifdef VX5B3D_POWAREA_MNG_ENABLE
	printk(KERN_INFO "Start A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		printk(KERN_ALERT "system_pwmng_powerarea_start_notify err!\n");
	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

retry:
	screen_handle =  screen_display_new();

	/* Setting peculiar to panel */
	set_lcd_if_param.handle			= screen_handle;
	set_lcd_if_param.port_no		= irq_portno;
	set_lcd_if_param.lcd_if_param		= &r_mobile_lcd_if_param;
	set_lcd_if_param.lcd_if_param_mask	= &r_mobile_lcd_if_param_mask;
	ret = screen_display_set_lcd_if_parameters(&set_lcd_if_param);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_set_lcd_if_parameters err!\n");
		goto out;
	}

	/* Setting FB address */
	set_address.handle	= screen_handle;
	set_address.output_mode	= RT_DISPLAY_LCD1;
	set_address.buffer_id	= RT_DISPLAY_BUFFER_A;
	set_address.address	= R_MOBILE_M_BUFF_ADDR;
	set_address.size	= R_MOBILE_M_BUFF_SIZE;
	ret = screen_display_set_address(&set_address);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_set_address err!\n");
		goto out;
	}

	/* Start a display to LCD */
	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_start_lcd err!\n");
		goto out;
	}

	/* LCD panel reset */
	mipi_display_reset();

	is_dsi_read_enabled = 1;

	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, init_cmdset);
	if (ret != 0) {
		printk(KERN_ALERT "panel_specific_cmdset err!\n");
		is_dsi_read_enabled = 0;
		if (retry_count == 0) {
			printk(KERN_ALERT "retry count 0!!!!\n");
			vx5b3d_panel_draw_black(screen_handle);
			ret = -ENODEV;
			goto out;
		} else {
			screen_disp_stop_lcd disp_stop_lcd;
			retry_count--;

			/* Stop a display to LCD */
			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT "display_stop_lcd err!\n");
				goto out;
			}

			disp_delete.handle = screen_handle;
			screen_display_delete(&disp_delete);

			goto retry;
		}
	}

	printk(KERN_DEBUG "Panel initialized with Video mode\n");

	/* update brightness at last changed */
	is_panel_initialized = 1;
	if (registed_bd && is_backlight_called)
		registed_bd->ops->update_status(registed_bd);

#ifdef CABC_FUNCTION_ENABLE
	vx5b3d_update_cabc_ctrl(Vee_cabc->cabc);
#endif

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	return ret;
}

static int vx5b3d_panel_suspend(void)
{
	void *screen_handle;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_delete disp_delete;
	int ret;

#ifdef VX5B3D_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_end_notify;
	system_pmg_delete pmg_delete;
#endif
	printk(KERN_INFO "%s\n", __func__);
	is_panel_initialized = 0;


	screen_handle =  screen_display_new();

#ifdef VX5B3D_STANDBY_CMD_ENABLE
	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, standby_cmdset);
	if (ret != 0) {
		printk(KERN_ALERT "panel_specific_cmdset err!\n");
		/* continue */
	}
#endif /* VX5B3D_STANDBY_CMD_ENABLE */

	is_dsi_read_enabled = 0;

	/* Stop a display to LCD */
	disp_stop_lcd.handle		= screen_handle;
	disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_stop_lcd(&disp_stop_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		printk(KERN_ALERT "display_stop_lcd err!\n");

	/* GPIO control */
	gpio_direction_output(reset_gpio, 0);
	msleep(20);
	regulator_disable(power_ldo_3v);
	msleep(50);
	regulator_disable(power_ldo_1v2);
	regulator_disable(power_ldo_1v8);
	msleep(25);

	/* LCD_BLIC_ON GPIO control */
	gpio_direction_output(GPIO_PORT47, 0);

	/* Release VIO_CKO5 */
	if (lvdsclk) {
		clk_disable(lvdsclk);
		clk_put(lvdsclk);
		lvdsclk = NULL;
		gpio_free(GPIO_FN_VIO_CKO5);
	}

	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

#ifdef VX5B3D_POWAREA_MNG_ENABLE
	printk(KERN_INFO "End A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_end_notify.handle		= system_handle;
	powarea_end_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_end_notify(&powarea_end_notify);
	if (ret != SMAP_LIB_PWMNG_OK) {
		printk(KERN_ALERT "system_pwmng_powerarea_end_notify err!\n");
		pmg_delete.handle = system_handle;
		system_pwmng_delete(&pmg_delete);
		return -1;
	}

	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

	return 0;
}

static int vx5b3d_panel_resume(void)
{
	void *screen_handle;
	screen_disp_start_lcd start_lcd;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_delete disp_delete;
	int ret = 0;
	int retry_count = 10;

#ifdef VX5B3D_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif
#ifdef CABC_FUNCTION_ENABLE
	struct Vx5d3b_cabc_info *Vee_cabc = g_vx5d3b;
#endif
	printk(KERN_INFO "%s\n", __func__);

#ifdef VX5B3D_POWAREA_MNG_ENABLE
	printk(KERN_INFO "Start A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		printk(KERN_ALERT "system_pwmng_powerarea_start_notify err!\n");
	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

retry:
	screen_handle =  screen_display_new();

	/* Start a display to LCD */
	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_start_lcd err!\n");
		goto out;
	}
	is_dsi_read_enabled = 1;

	/* LCD panel reset */
	mipi_display_reset();

	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, init_cmdset);
	if (ret != 0) {
		printk(KERN_ALERT "panel_specific_cmdset err!\n");
		is_dsi_read_enabled = 0;
		if (retry_count == 0) {
			printk(KERN_ALERT "retry count 0!!!!\n");
			vx5b3d_panel_draw_black(screen_handle);
			ret = -ENODEV;
			goto out;
		} else {
			/* Stop a display to LCD */
			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT "display_stop_lcd err!\n");
				goto out;
			}

			retry_count--;
			disp_delete.handle = screen_handle;
			screen_display_delete(&disp_delete);
			goto retry;
		}
	}

	/* update brightness at last changed */
	is_panel_initialized = 1;
	if (registed_bd)
		registed_bd->ops->update_status(registed_bd);

#ifdef CABC_FUNCTION_ENABLE
	vx5b3d_update_cabc_ctrl(Vee_cabc->cabc);
#endif
out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	return ret;
}

static int vx5b3d_panel_probe(struct fb_info *info,
			    struct fb_panel_hw_info hw_info)
{
	struct platform_device *pdev;
	int ret;
#ifdef CABC_FUNCTION_ENABLE
	struct Vx5d3b_cabc_info *vx5d3bInfo;
#endif
	printk(KERN_INFO "%s\n", __func__);

	reset_gpio = hw_info.gpio_reg;

	/* fb parent device info to platform_device */
	pdev = to_platform_device(info->device);

	power_ldo_3v = regulator_get(NULL, "vlcd_3v");
	power_ldo_1v8 = regulator_get(NULL, "vlcd_1v8");
	power_ldo_1v2 = regulator_get(NULL, "vlcd_1v2");

	if (IS_ERR(power_ldo_3v) ||
	    IS_ERR(power_ldo_1v8) ||
	    IS_ERR(power_ldo_1v2)) {
		printk(KERN_ERR "regulator_get failed\n");
		printk(KERN_ERR "power_ldo_3v ret=%ld\n",
						PTR_ERR(power_ldo_3v));
		printk(KERN_ERR "power_ldo_1v8 ret=%ld\n",
						PTR_ERR(power_ldo_1v8));
		printk(KERN_ERR "power_ldo_1v2 ret=%ld\n",
						PTR_ERR(power_ldo_1v2));
		return -ENODEV;
	}
	printk(KERN_INFO "PMIC        : for panel power\n");
	printk(KERN_INFO "GPIO_PORT%d : for panel reset\n", reset_gpio);
	printk(KERN_INFO "IRQ%d       : for panel te\n", irq_portno);

	ret = regulator_set_voltage(power_ldo_1v2, 1200000, 1200000);
	if (ret) {
		printk(KERN_ERR "regulator_set_voltage ldo_1v2 failed\n");
		return ret;
	}

	ret = regulator_set_voltage(power_ldo_3v, 3300000, 3300000);
	if (ret) {
		printk(KERN_ERR "regulator_set_voltage ldo_3v failed\n");
		return ret;
	}

	common_fb_info = info;
	is_dsi_read_enabled = 0;
	is_panel_initialized = 0;

	/* clear internal info */
	memset(&lcd_info_data, 0, sizeof(lcd_info_data));

	/* register sysfs for LCD */
	lcd_info_data.ld = lcd_device_register("panel",
						&pdev->dev,
						&lcd_info_data,
						&vx5b3d_lcd_ops);
	if (IS_ERR(lcd_info_data.ld))
		return PTR_ERR(lcd_info_data.ld);

	/* register sysfs for LCD frequency control */
	ret = vx5b3d_lcd_frequency_register(info->device);
	if (ret < 0)
		goto out;

	/* register device for backlight control */
	ret = vx5b3d_backlight_device_register(info->dev);

	lcd_info_data.power = FB_BLANK_UNBLANK;

#ifdef CABC_FUNCTION_ENABLE
	vx5d3bInfo = kzalloc(sizeof(struct Vx5d3b_cabc_info), GFP_KERNEL);

	if (!vx5d3bInfo)
		pr_err("failed to allocate vx5d3bInfo\n");

	mutex_init(&vx5d3bInfo->lock);
	vx5d3bInfo->cabc = CABC_OFF;
	vx5d3bInfo->auto_brightness = false;

	g_vx5d3b = vx5d3bInfo;

	mdnie_class = class_create(THIS_MODULE, "mdnie");

	if (IS_ERR_OR_NULL(mdnie_class))
		pr_err("failed to create mdnie class\n");

	mdnie_class->dev_attrs = mdnie_attributes;

	vx5d3bInfo = kzalloc(sizeof(struct Vx5d3b_cabc_info), GFP_KERNEL);

	if (!vx5d3bInfo) {
		pr_err("failed to allocate vx5d3bInfo\n");
		ret = -ENOMEM;
		goto out;
	}

	vx5d3bInfo->dev = device_create(mdnie_class, NULL, 0,
						&vx5d3bInfo, "mdnie");

	if (IS_ERR_OR_NULL(vx5d3bInfo->dev)) {
		pr_err("failed to create mdnie device\n");
		ret = -EINVAL;
		goto out;
	}
#endif

	return 0;

out:
	/* unregister sysfs for LCD */
	lcd_device_unregister(lcd_info_data.ld);

	return ret;
}

static int vx5b3d_panel_remove(struct fb_info *info)
{
	printk(KERN_INFO "%s\n", __func__);

	/* unregister device for backlight control */
	vx5b3d_backlight_device_unregister();

	/* unregister sysfs for LCD frequency control */
	vx5b3d_lcd_frequency_unregister();

	/* unregister sysfs for LCD */
	lcd_device_unregister(lcd_info_data.ld);

	return 0;
}

static struct fb_panel_info vx5b3d_panel_info(void)
{
	printk(KERN_INFO "%s\n", __func__);

	return r_mobile_info;
}

struct fb_panel_func r_mobile_panel_func(int panel)
{

	struct fb_panel_func panel_func;

	printk(KERN_INFO "%s\n", __func__);

	memset(&panel_func, 0, sizeof(struct fb_panel_func));

/* e.g. support (panel=RT_DISPLAY_LCD1) */

	if (panel == RT_DISPLAY_LCD1) {
		panel_func.panel_init    = vx5b3d_panel_init;
		panel_func.panel_suspend = vx5b3d_panel_suspend;
		panel_func.panel_resume  = vx5b3d_panel_resume;
		panel_func.panel_probe   = vx5b3d_panel_probe;
		panel_func.panel_remove  = vx5b3d_panel_remove;
		panel_func.panel_info    = vx5b3d_panel_info;
	}

	return panel_func;
}

/* For I2C */

/* defs */
#define QL_ID 0x2300

#define CONTROL_BYTE_DCS       (0x08u)
#define CONTROL_BYTE_GEN       (0x09u)

#define GEN_QL_CSR_OFFSET_LENGTH  {\
		CONTROL_BYTE_GEN, \
	0x29,  /* Data ID */\
	0x05,  /* Vendor Id 1 */\
	0x01,  /* Vendor Id 2 */\
	0x41,  /* Vendor Unique Command */\
	0x00,  /* Address LS */\
	0x00,  /* Address MS */\
	0x00,  /* Length LS */\
	0x00,  /* Length MS */\
	}

#define GEN_QL_CSR_WRITE  {\
		CONTROL_BYTE_GEN, \
	0x29,  /* Data ID */\
	0x05,  /* Vendor Id 1 */\
	0x01,  /* Vendor Id 2 */\
	0x40,  /* Vendor Unique Command */\
	0x00,  /* Address LS */\
	0x00,  /* Address MS */\
	0x00,  /* data LS */\
	0x00, \
	0x00, \
	0x00,  /* data MS */\
	}

static struct i2c_client *i2c_quick_client;

static int i2c_quickvx_probe(struct i2c_client *client,
				const struct i2c_device_id *idp)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "error : not compatible i2c function \r\n");
		return -ENODEV;
	}
	i2c_quick_client = client;
	return 0;
}

static int __devexit i2c_quickvx_remove(struct i2c_client *client)
{
	i2c_set_clientdata(client, NULL);
	return 0;
}

static struct i2c_device_id i2c_quickvx_idtable[] = {
	{ "panel_vx5b3d", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, i2c_quickvx_idtable);
static struct i2c_driver i2c_quickvx_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "panel_vx5b3d",
	},
	.id_table = i2c_quickvx_idtable,
	.probe = i2c_quickvx_probe,
	.remove		= __devexit_p(i2c_quickvx_remove),
};


static int __init mipi_quickvx_lcd_init(void)
{
	return i2c_add_driver(&i2c_quickvx_driver);
}
static void mipi_quickvx_lcd_exit(void)
{
	i2c_del_driver(&i2c_quickvx_driver);
}

#if 0
static int ql_i2c_read(u32 addr, u32 *val, u32 data_size)
{
	u32 data;
	char buf[] = GEN_QL_CSR_OFFSET_LENGTH;
	char rx[10];
	int ret = -1;
	int write_size;

	buf[5] = addr & 0xff;
	buf[6] = (addr >> 8) & 0xff;
	buf[7] = data_size & 0xff;
	buf[8] = (data_size >> 8) & 0xff;

	write_size = 9;

	ret = i2c_master_send(i2c_quick_client, &buf[0], write_size);
	if (ret != write_size) {
		printk(KERN_ERR "%s: i2c_master_send failed (%d)!\n",
							__func__, ret);
		return -1;
	}

	/* generic read request 0x24 to send generic read command */
	write_size = 4;

	buf[0] = CONTROL_BYTE_GEN;
	buf[1] =    0x24;  /* Data ID */
	buf[2] =    0x05;  /* Vendor Id 1 */
	buf[3] =    0x01;  /* Vendor Id 2 */

	ret = i2c_master_send(i2c_quick_client, &buf[0], write_size);
	if (ret != write_size) {
		printk(KERN_ERR "%s: i2c_master_send failed (%d)!\n",
							__func__, ret);
		return -1;
	}
	/* return number of bytes or error */
	ret = i2c_master_recv(i2c_quick_client, &rx[0], data_size);
	if (ret != data_size) {
		printk(KERN_ERR "%s: i2c_master_recv failed (%d)!\n",
							__func__, ret);
		return -1;
	}

	data = rx[0];
	if (data_size > 1)
		data |= (rx[1] << 8);
	if (data_size > 2)
		data |= (rx[2] << 16) | (rx[3] << 24);

	*val = data;

	printk(KERN_DEBUG "r0x%x=0x%x\n", addr, data);

	return 0;

}
#endif

/* i2c header + data type */
#define I2C_DATA_HEADER_SIZE 2

static int ql_i2c_write_wrapper(u8 cmd, int size, const u8 *data)
{
	int ret;
	char buf[] = GEN_QL_CSR_WRITE;

	buf[1] = cmd;
	if (size > sizeof(buf) - I2C_DATA_HEADER_SIZE)
		printk(KERN_ERR "size over %d > %d\n",
				size, sizeof(buf) - I2C_DATA_HEADER_SIZE);
	memcpy(&buf[2], data, size);

	ret = i2c_master_send(i2c_quick_client, buf,
						size + I2C_DATA_HEADER_SIZE);

	if (ret != size + I2C_DATA_HEADER_SIZE) {
		printk(KERN_ERR
			"%s: i2c_master_send failed (%d)!\n", __func__, ret);
		return -1;
	}
#if 0 /* for debug */
	printk(KERN_DEBUG
		"send %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
				buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
				buf[6], buf[7], buf[8], buf[9], buf[10]);
#endif
	return 0;
}

module_init(mipi_quickvx_lcd_init);
module_exit(mipi_quickvx_lcd_exit);

MODULE_LICENSE("GPL");
