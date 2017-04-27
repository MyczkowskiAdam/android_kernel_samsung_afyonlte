/*
 * drivers/char/rtapi/include/screen_display.h
 *     This file is screen display function.
 *
 * Copyright (C) 2011-2013 Renesas Electronics Corporation
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

#ifndef __SCREEN_DISPLAY_H__
#define __SCREEN_DISPLAY_H__

/****************************************/
/* Fixed Number				*/
/****************************************/
/* LCD Type */
#define RT_DISPLAY_LCD1 (1)
#define RT_DISPLAY_LCD2 (2)
#define RT_DISPLAY_HDMI (3)
#define RT_DISPLAY_LCDHDMI (7)
#define RT_DISPLAY_LCD1_ASYNC (257)

/** Image format */
#define RT_DISPLAY_FORMAT_RGB565		(1)
#define RT_DISPLAY_FORMAT_ARGB8888		(2)
#define RT_DISPLAY_FORMAT_RGB888		(3)
#define RT_DISPLAY_FORMAT_ABGR8888		(8)

/** Buffer ID */
#define RT_DISPLAY_DRAW_BLACK			(99)

/** Refreshment mode */
#define RT_DISPLAY_REFRESH_ON			(1)
#define RT_DISPLAY_REFRESH_OFF			(2)

/** Display Buffer ID **/
#define RT_DISPLAY_BUFFER_A				(0)
#define RT_DISPLAY_BUFFER_B				(1)

/** HDMI Output Format */
#define	RT_DISPLAY_720_480P60			(1)
#define	RT_DISPLAY_1280_720P60			(2)
#define	RT_DISPLAY_1920_1080I60			(3)
#define	RT_DISPLAY_1920_1080P24			(4)
#define	RT_DISPLAY_720_576P50			(5)
#define	RT_DISPLAY_1280_720P50			(6)
#define	RT_DISPLAY_1920_1080I50			(7)
#define	RT_DISPLAY_720_480I60A43		(9)
#define	RT_DISPLAY_720_576I50A43		(10)
#define	RT_DISPLAY_1920_1080P60			(11)
#define	RT_DISPLAY_1920_1080P50			(12)
#define	RT_DISPLAY_720_480P60A43		(13)
#define	RT_DISPLAY_720_576P50A43		(14)
#define	RT_DISPLAY_720_480P60FULL		(33)
#define	RT_DISPLAY_1280_720P60FULL		(34)
#define	RT_DISPLAY_1920_1080I60FULL		(35)
#define	RT_DISPLAY_720_576P50FULL		(37)
#define	RT_DISPLAY_1280_720P50FULL		(38)
#define	RT_DISPLAY_1920_1080I50FULL		(39)
#define	RT_DISPLAY_720_480I60			(41)
#define	RT_DISPLAY_720_576I50			(42)
#define RT_DISPLAY_USE_IF_PARAM			(99)

/* Rotation type */
#define RT_DISPLAY_ROTATE_0				(1)
#define RT_DISPLAY_ROTATE_90			(2)
#define RT_DISPLAY_ROTATE_180			(3)
#define RT_DISPLAY_ROTATE_270			(4)

/* Reseption mode */
#define RT_DISPLAY_RECEPTION_OFF		(1)
#define RT_DISPLAY_RECEPTION_ON			(2)

/* DSI mode */
#define RT_DISPLAY_SEND_MODE_LP			(1)
#define RT_DISPLAY_SEND_MODE_HS			(2)


#define RT_DISPLAY_PROGRESSIVE			(1)
#define RT_DISPLAY_INTERLACE			(2)

/** RT I/F Return code */
#define SMAP_LIB_DISPLAY_OK				(0)
#define SMAP_LIB_DISPLAY_NG				(-1)
#define SMAP_LIB_DISPLAY_PARAERR		(-2)
#define SMAP_LIB_DISPLAY_SEQERR			(-3)

/* LUT mode */
#define RT_DISPLAY_LUT_OFF				(1)
#define RT_DISPLAY_LUT_ON				(2)

/** Color Palette mode */
#define RT_DISPLAY_PALETTE_ON			(1)
#define RT_DISPLAY_PALETTE_OFF			(2)

/****************************************/
/* Structure				*/
/****************************************/
typedef struct {
	unsigned int dsitckcr;
	unsigned int dsi0pckcr;
	unsigned int dsi0phycr;
	unsigned int sysconf;
	unsigned int timset0;
	unsigned int timset1;
	unsigned int dsictrl;
	unsigned int vmctr1;
	unsigned int vmctr2;
	unsigned int vmlen1;
	unsigned int vmlen2;
	unsigned int vmlen3;
	unsigned int vmlen4;
	unsigned int dtctr;
	unsigned int mldhcnr;
	unsigned int mldhsynr;
	unsigned int mldhajr;
	unsigned int mldvlnr;
	unsigned int mldvsynr;
	unsigned int mldmt1r;
	unsigned int lddckr;
	unsigned int mlddckpat1r;
	unsigned int mlddckpat2r;
	unsigned int phytest;
} screen_disp_lcd_if;

#ifndef _SCREEN_RECT_TYPE_
#define _SCREEN_RECT_TYPE_
typedef struct {
	unsigned short x;
	unsigned short y;
	unsigned short width;
	unsigned short height;
} screen_rect;
#endif

/* DSI mode */
enum dsi_mode {
	DSI_FROMHS = 0,
	DSI_TOHS
};

typedef struct {
	void               *handle;
	unsigned short      dsimode;
} screen_disp_set_dsi_mode;


typedef struct {
	void            *handle;
	unsigned short   output_mode;
	unsigned short   format;
	screen_rect      draw_rect;
	unsigned int     buffer_offset;
	unsigned short   buffer_id;
	unsigned short   rotate;
} screen_disp_draw;

typedef struct {
	void          *handle;
	unsigned short output_mode;
} screen_disp_start_lcd;

typedef struct {
	void          *handle;
	unsigned short output_mode;
} screen_disp_stop_lcd;

typedef struct {
	void          *handle;
	unsigned short output_mode;
	unsigned short refresh_mode;
} screen_disp_set_lcd_refresh;

typedef struct {
	void        *handle;
	unsigned int format;
	unsigned int background_color;
} screen_disp_start_hdmi;

typedef struct {
	void *handle;
} screen_disp_stop_hdmi;

typedef struct {
	void         *handle;
	unsigned short output_mode;
	unsigned short dummy;
	unsigned char data_id;
	unsigned char reg_address;
	unsigned char	write_data;
	unsigned char	data_count;
	unsigned char	*read_data;
} screen_disp_read_dsi_short;

typedef struct {
	void         *handle;
	unsigned short output_mode;
	unsigned char data_id;
	unsigned char reg_address;
	unsigned char write_data;
	unsigned char reception_mode;
} screen_disp_write_dsi_short;

typedef struct {
	void          *handle;
	unsigned short output_mode;
	unsigned char  data_id;
	unsigned char  dummy;
	unsigned short data_count;
	unsigned short dummy2;
	unsigned char *write_data;
	unsigned char reception_mode;
	unsigned char send_mode;
} screen_disp_write_dsi_long;

typedef struct {
	void               *handle;
	unsigned int		port_no;
	screen_disp_lcd_if *lcd_if_param;
	screen_disp_lcd_if *lcd_if_param_mask;
} screen_disp_set_lcd_if_param;

typedef struct {
	void           *handle;
	unsigned short output_mode;
	unsigned short buffer_id;
	unsigned int   address;
	unsigned int   size;
} screen_disp_set_address;

typedef struct {
	unsigned short videoH;
	unsigned short videoV;
	unsigned short pixelH;
	unsigned short pixelV;
} screen_disp_aspect;

typedef struct {
	unsigned int mldhcnr;
	unsigned int mldhsynr;
	unsigned int mldhajr;
	unsigned int mldvlnr;
	unsigned int mldvsynr;
	unsigned int mldvlnr_b;
	unsigned int mldvsynr_b;
	unsigned int mldivsnr;
	unsigned int mldmt1r;
	unsigned int mldmt1r_b;
	unsigned int mlddckpat1r;
	unsigned int mlddckpat2r;
	unsigned int lddckr;
	unsigned int sysconf;
	unsigned int timset0;
	unsigned int timset1;
	unsigned int dsictrl;
	unsigned int vmctr1;
	unsigned int vmctr2;
	unsigned int vmlen1;
	unsigned int vmlen2;
	unsigned int vmlen3;
	unsigned int vmlen4;
	unsigned int dtctr;
	unsigned int pll2cr;
	unsigned int dsi1pckcr;
	unsigned int dsi1phycr;
	unsigned int phytest;
} screen_disp_hdmi_if;

typedef struct {
	void					*handle;
	unsigned int			ipmode;
	screen_disp_aspect		*aspect;
	screen_disp_hdmi_if		*hdmi_if_param;
} screen_disp_set_hdmi_if_param;

typedef struct {
	void					*handle;
	unsigned short			output_mode;
	unsigned short			lut_mode;
	unsigned long			*lut;
} screen_disp_set_lut;

typedef struct {
	void			*handle;
	unsigned short	output_mode;
	unsigned short	palette_mode;
	unsigned long	*data;
} screen_disp_lcd_color_palette;

typedef struct {
	void *handle;
} screen_disp_delete;

/****************************************/
/* function								*/
/****************************************/
/* Create handle */
extern void *screen_display_new(
void);

/* Draw */
extern int screen_display_draw(
screen_disp_draw *disp_draw);

/* Set DSI mode - FromHS, ToHS */
extern int screen_display_set_dsi_mode(
screen_disp_set_dsi_mode *set_dsi_mode);

/* Start LCD */
extern int screen_display_start_lcd(
screen_disp_start_lcd *start_lcd);

/* Stop LCD */
extern int screen_display_stop_lcd(
screen_disp_stop_lcd *stop_lcd);

/* Set LCD refresh mode */
extern int screen_display_set_lcd_refresh(
screen_disp_set_lcd_refresh *set_lcd_refresh);

/* Start HDMI */
extern int screen_display_start_hdmi(
screen_disp_start_hdmi *start_hdmi);

/* Stop HDMI */
extern int screen_display_stop_hdmi(
screen_disp_stop_hdmi *stop_hdmi);

/* Read LCD packet data */
extern int screen_display_read_dsi_short_packet(
screen_disp_read_dsi_short *read_dsi_s);

/* Write LCD packet data */
extern int screen_display_write_dsi_short_packet(
screen_disp_write_dsi_short *write_dsi_s);

/* Write LCD packet data */
extern int screen_display_write_dsi_long_packet(
screen_disp_write_dsi_long *write_dsi_l);

/* Set LCD parameters */
extern int screen_display_set_lcd_if_parameters(
screen_disp_set_lcd_if_param *set_lcd_if_param);

/* Set address  */
extern int screen_display_set_address(
screen_disp_set_address *address);

/* Set HDMI parameters */
extern int screen_display_set_hdmi_if_parameters(
screen_disp_set_hdmi_if_param *set_hdmi_if_param);

/* Set Lut parameters */
extern int screen_display_set_lut(
screen_disp_set_lut *disp_set_lut);

/* Set LCD color palette data */
extern int screen_display_set_lcd_color_palette(
screen_disp_lcd_color_palette *disp_lcd_color_plt);

/* Delete handle */
extern void screen_display_delete(
screen_disp_delete *disp_delete);

#endif	/* __SCREEN_DISPLAY_H__ */
