/*
 * drivers/char/rtapi/include/screen_graphics.h
 *     This file is screen graphics function.
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

#ifndef __SCREEN_GRAPHICS_H__
#define __SCREEN_GRAPHICS_H__


/*
 * Definition
 */
/* graphics mode */
#define RT_GRAPHICS_MODE_IMAGE_CONVERSION	(0x00000001)	/* image conversion */
#define RT_GRAPHICS_MODE_IMAGE_BLEND		(0x00000002)	/* image blend */
#define RT_GRAPHICS_MODE_IMAGE_OUTPUT		(0x00000004)	/* image output */
#define RT_GRAPHICS_MODE_IMAGE_EDIT			(0x00000008)	/* image edit */

/* color_format */
#define RT_GRAPHICS_COLOR_YUV422SP			(1)	/* YUV422 SemiPlanar */
#define RT_GRAPHICS_COLOR_YUV420SP			(2)	/* YUV420 SemiPlanar */
#define RT_GRAPHICS_COLOR_NV21				(3)	/* NV21 */
#define RT_GRAPHICS_COLOR_YV12				(4)	/* YV12 */
#define RT_GRAPHICS_COLOR_RGB565			(5)	/* RGB565 */
#define RT_GRAPHICS_COLOR_RGB888			(6)		/* RGB888 */
#define RT_GRAPHICS_COLOR_ARGB8888			(7)		/* aRGB8888 */
#define RT_GRAPHICS_COLOR_ABGR8888			(8)		/* aBGR8888 */
#define RT_GRAPHICS_COLOR_YUV420PL			(12)	/* YUV420 Planar */
#define RT_GRAPHICS_COLOR_XRGB8888			(13)	/* xRGB8888 */
#define RT_GRAPHICS_COLOR_XBGR8888			(14)	/* xBGR8888 */
#define RT_GRAPHICS_COLOR_YUV422UYVY		(15)	/* YUV422UYVY */

/* yuv_format */
#define RT_GRAPHICS_COLOR_BT601				(1)	/* ITU-R BT.601 */
#define RT_GRAPHICS_COLOR_BT709				(2)	/* ITU-R BT.709 */

/* yuv_range */
#define RT_GRAPHICS_COLOR_COMPRESSED		(1)	/* Compressed-Range */
#define RT_GRAPHICS_COLOR_FULLSCALE			(2)	/* Fullscale-Range */

/* Rotation type */
#define RT_GRAPHICS_ROTATE_0				(1)		/* Non-rotation */
#define RT_GRAPHICS_ROTATE_90				(2)		/* Angle of 90 degrees */
#define RT_GRAPHICS_ROTATE_180				(3)		/* Angle of 180 degrees */
#define RT_GRAPHICS_ROTATE_270				(4)		/* Angle of 270 degrees */

/* Mirror type */
#define RT_GRAPHICS_MIRROR_N				(1)		/* Non-inversion */
#define RT_GRAPHICS_MIRROR_V				(2)		/* Up Down inversion */
#define RT_GRAPHICS_MIRROR_H				(4)		/* Right Left inversion */

/* key_color */
#define RT_GRAPHICS_KEY_COLOR_OFF			(-1)		/* key color off */

/* premultiplied */
#define RT_GRAPHICS_PREMULTI_OFF			(1)		/* premultiplied OFF */
#define RT_GRAPHICS_PREMULTI_ON				(2)		/* premultiplied ON */

/* alpha coefficient */
#define RT_GRAPHICS_COEFFICIENT_ALPHA1		(1)		/* alpha coefficient1 */
#define RT_GRAPHICS_COEFFICIENT_ALPHA2		(2)		/* alpha coefficient2 */

/* RT-API result code */
#define SMAP_LIB_GRAPHICS_OK				(0)
#define SMAP_LIB_GRAPHICS_NG				(-1)
#define SMAP_LIB_GRAPHICS_PARAERR			(-2)
#define SMAP_LIB_GRAPHICS_SEQERR			(-3)
#define SMAP_LIB_GRAPHICS_MEMERR			(-4)
#define SMAP_LIB_GRAPHICS_INUSE				(-5)

/*
 * Structure
 */
#ifndef _SCREEN_RECT_TYPE_
#define _SCREEN_RECT_TYPE_
typedef struct {
	unsigned short x;
	unsigned short y;
	unsigned short width;
	unsigned short height;
} screen_rect;
#endif

/* screen_graphics_new API parameter */
typedef struct {
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
} screen_grap_new;

/* screen_graphics_set_blend_size API parameter */
typedef struct {
	void					*handle;		/* graphics interface handle */
	unsigned short			lcd_width;		/* lcd width */
	unsigned short			lcd_height;		/* lcd height */
	unsigned short			hdmi_width;		/* hdmi width */
	unsigned short			hdmi_height;	/* hdmi height */
} screen_grap_set_blend_size;

/* screen_graphics_initialize API parameter */
typedef struct {
	void					*handle;		/* graphics interface handle */
	unsigned long			mode;			/* graphics mode */
} screen_grap_initialize;

/* screen_graphics_image_conversion API parameter */
typedef struct {
	unsigned short			width;				/* image width */
	unsigned short			height;				/* image height */
	unsigned short			stride;				/* image stride(Y) */
	unsigned short			stride_c;			/* image stride(C) */
	unsigned int			format;				/* image color format */
	unsigned short			yuv_format;			/* image yuv format */
	unsigned short			yuv_range;			/* image yuv range */
	unsigned char			*address;			/* image data address(Y) */
	void					*apmem_handle;		/* image data app-shared memory handle(Y) */
	unsigned char			*address_c0;		/* image data address(C0) */
	void					*apmem_handle_c0;	/* image data app-shared memory handle(C0) */
	unsigned char			*address_c1;		/* image data address(C1) */
	void					*apmem_handle_c1;	/* image data app-shared memory handle(C1) */
} screen_grap_image_param;

typedef struct {
	void					*handle;			/* graphics interface handle */
	screen_grap_image_param	input_image;		/* input image parameters */
	screen_grap_image_param	output_image;		/* output image parameters */
	unsigned long			user_data;			/* user data */
} screen_grap_image_conv;

/* screen_graphics_image_blend API parameter */
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
	unsigned char				*palette;					/* palette data address */
	void						*palette_apmem_handle;		/* palette data app-shared memory handle */
	unsigned long				palette_size;				/* palette data size */
	unsigned char				*alpha_plane;				/* alpha plane address */
	void						*alpha_plane_apmem_handle;	/* alpha plane app-shared memory handle */
} screen_grap_layer;

/* screen_graphics_image_blend API parameter */
typedef struct {
	void					*handle;			/* graphics interface handle */
	screen_grap_layer		*input_layer[4];	/* input image information */
	screen_grap_image_param	output_image;		/* output image parameters */
	unsigned long			background_color;	/* background_color */
	unsigned long			user_data;			/* user data */
} screen_grap_image_blend;

/* screen_graphics_image_output API parameter */
typedef struct {
	void					*handle;			/* graphics interface handle */
	screen_grap_image_param	output_image;		/* output image parameters */
	unsigned short			rotate;				/* rotation type */
	unsigned short			dummy;				/* dummy */
	unsigned long			user_data;			/* user data */
} screen_grap_image_output;

/* screen_graphics_quit API parameter */
typedef struct {
	void					*handle;			/* graphics interface handle */
	unsigned long			mode;				/* graphics mode */
} screen_grap_quit;

/* screen_graphics_edit API parameter */
typedef struct {
	screen_grap_image_param		input_image;	/* input image parameters */
	screen_rect					rect;			/* rect */
	unsigned short				rotate;			/* rotation type */
	unsigned short				mirror;			/* mirror type */
} screen_grap_edit_param;

typedef struct {
	void						*handle;		/* graphics interface handle */
	screen_grap_edit_param		input_param;	/* input image parameters */
	screen_grap_image_param		output_image;	/* output image parameters */
	unsigned long				user_data;		/* user data */
} screen_grap_image_edit;

/* screen_graphics_delete API parameter */
typedef struct {
	void					*handle;			/* graphics interface handle */
} screen_grap_delete;


/*
 * Function
 */

extern void *screen_graphics_new
(
	screen_grap_new	*grap_new
);

extern int screen_graphics_set_blend_size
(
	screen_grap_set_blend_size	*grap_set_blend_size
);

extern int screen_graphics_initialize
(
	screen_grap_initialize	*grap_initialize
);

extern int screen_graphics_image_conversion
(
	screen_grap_image_conv	*grap_image_conv
);

extern int screen_graphics_image_blend
(
	screen_grap_image_blend	*grap_blend
);

extern int screen_graphics_image_output
(
	screen_grap_image_output	*grap_output
);

extern int screen_graphics_quit
(
	screen_grap_quit	*grap_quit
);

extern int screen_graphics_image_edit
(
	screen_grap_image_edit	*grap_edit
);

extern void screen_graphics_delete
(
	screen_grap_delete	*grap_delete
);


#endif	/* __SCREEN_GRAPHICS_H__ */
