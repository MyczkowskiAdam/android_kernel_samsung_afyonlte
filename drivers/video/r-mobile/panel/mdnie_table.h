#ifndef __MDNIE_TABLE_H__                                                          
#define __MDNIE_TABLE_H__                                                          
                                                                                   
#include "mdnie.h"  


#if 1 /* HX8389B */
static u8 set_cabc[] = { /* 0xc9 */
0xC9,
0x0F,
0x02,
0x1E,
0x1D,
0x00,
0x80
};
#else /* HX8369B */
static u8 set_cabc[] = { /*0xC9 */
0xC9,
0x0F, /*PWM CLK DIV*/
0xFF, /*PWM period*/
};
#endif


static u8 seteco[] = { /*0xc6 */
0xC6,
0xEC
};

/* + CABC Mode Sequence */
static u8 cabc_ui[] = { /* 0x55 */
0x55,
0x01,
};

#if 0
static u8 cabc_still[] = { /* 0x55 */
0x55,	
0x02,
};

static u8 cabc_moving[] = { /* 0x55 */
0x55, 
0x03,
};
#endif

/* - CABC Mode Sequence */

/* + CABC off Sequence */
static u8 cabc_off[] = { /* 0x55 */
0x55,	
0x00,
};
/* - CABC off Sequence */


static u8 negative_tuning[] = {  
0xCD,	
0x5A, //password 5A
0x00, //mask 000
0x00, //data_width
0x30, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
0x00, //roi_ctrl
0x00, //roi1 y end
0x00, 
0x00, //roi1 y start
0x00,
0x00, //roi1 x end
0x00,
0x00, //roi1 x start
0x00,
0x00, //roi0 y end
0x00,
0x00, //roi0 y start
0x00,
0x00, //roi0 x end
0x00,
0x00, //roi0 x start
0x00,
0xff, //scr Kb
0x00, //scr Wb
0xff, //scr Kg
0x00, //scr Wg
0xff, //scr Kr
0x00, //scr Wr
0x00, //scr Bb
0xff, //scr Yb
0xff, //scr Bg
0x00, //scr Yg
0xff, //scr Br
0x00, //scr Yr
0xff, //scr Gb
0x00, //scr Mb
0x00, //scr Gg
0xff, //scr Mg
0xff, //scr Gr
0x00, //scr Mr
0xff, //scr Rb
0x00, //scr Cb
0xff, //scr Rg
0x00, //scr Cg
0x00, //scr Rr
0xff, //scr Cr
0x00, //sharpen_set cc_en gamma_en 00 0 0
0x20, //curve24 a
0x00, //curve24 b
0x20, //curve23 a
0x00, //curve23 b
0x20, //curve22 a
0x00, //curve22 b
0x20, //curve21 a
0x00, //curve21 b
0x20, //curve20 a
0x00, //curve20 b
0x20, //curve19 a
0x00, //curve19 b
0x20, //curve18 a
0x00, //curve18 b
0x20, //curve17 a
0x00, //curve17 b
0x20, //curve16 a
0x00, //curve16 b
0x20, //curve15 a
0x00, //curve15 b
0x20, //curve14 a
0x00, //curve14 b
0x20, //curve13 a
0x00, //curve13 b
0x20, //curve12 a
0x00, //curve12 b
0x20, //curve11 a
0x00, //curve11 b
0x20, //curve10 a
0x00, //curve10 b
0x20, //curve 9 a
0x00, //curve 9 b
0x20, //curve 8 a
0x00, //curve 8 b
0x20, //curve 7 a
0x00, //curve 7 b
0x20, //curve 6 a
0x00, //curve 6 b
0x20, //curve 5 a
0x00, //curve 5 b
0x20, //curve 4 a
0x00, //curve 4 b
0x20, //curve 3 a
0x00, //curve 3 b
0x20, //curve 2 a
0x00, //curve 2 b
0x20, //curve 1 a
0x00, //curve 1 b
0x04, //cc b3
0x00,
0x00, //cc b2
0x00,
0x00, //cc b1
0x00,
0x00, //cc g3
0x00,
0x04, //cc g2
0x00,
0x00, //cc g1
0x00,
0x00, //cc r3
0x00,
0x00, //cc r2
0x00,
0x04, //cc r1
0x00,
};

static u8 video_tuning[] = {   
0xCD,	                                                               
0x5A, //password 5A
0x00, //mask 000
0x00, //data_width
0x03, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
0x00, //roi_ctrl
0x00, //roi1 y end
0x00, 
0x00, //roi1 y start
0x00,
0x00, //roi1 x end
0x00,
0x00, //roi1 x start
0x00,
0x00, //roi0 y end
0x00,
0x00, //roi0 y start
0x00,
0x00, //roi0 x end
0x00,
0x00, //roi0 x start
0x00,
0x00, //scr Kb
0xFF, //scr Wb
0x00, //scr Kg
0xFF, //scr Wg
0x00, //scr Kr
0xFF, //scr Wr
0xFF, //scr Bb
0x00, //scr Yb
0x00, //scr Bg
0xFF, //scr Yg
0x00, //scr Br
0xFF, //scr Yr
0x00, //scr Gb
0xFF, //scr Mb
0xFF, //scr Gg
0x00, //scr Mg
0x00, //scr Gr
0xFF, //scr Mr
0x00, //scr Rb
0xFF, //scr Cb
0x00, //scr Rg
0xFF, //scr Cg
0xFF, //scr Rr
0x00, //scr Cr
0x06, //sharpen_set cc_en gamma_en 00 0 0
0xff, //curve24 a
0x00, //curve24 b
0x20, //curve23 a
0x00, //curve23 b
0x20, //curve22 a
0x00, //curve22 b
0x20, //curve21 a
0x00, //curve21 b
0xa1, //curve20 a
0x05, //curve20 b
0xa2, //curve19 a
0x09, //curve19 b
0xa0, //curve18 a
0x04, //curve18 b
0xa0, //curve17 a
0x04, //curve17 b
0xac, //curve16 a
0x1c, //curve16 b
0xac, //curve15 a
0x1c, //curve15 b
0xa8, //curve14 a
0x15, //curve14 b
0xa8, //curve13 a
0x15, //curve13 b
0xa8, //curve12 a
0x15, //curve12 b
0xa0, //curve11 a
0x0a, //curve11 b
0xa0, //curve10 a
0x0a, //curve10 b
0x98, //curve 9 a
0x01, //curve 9 b
0x98, //curve 8 a
0x01, //curve 8 b
0x98, //curve 7 a
0x01, //curve 7 b
0x98, //curve 6 a
0x01, //curve 6 b
0x98, //curve 5 a
0x01, //curve 5 b
0x98, //curve 4 a
0x01, //curve 4 b
0x98, //curve 3 a
0x01, //curve 3 b
0x98, //curve 2 a
0x01, //curve 2 b
0x08, //curve 1 a
0x00, //curve 1 b
0x04, //cc b3 0.1
0x5b,
0x1f, //cc b2
0xc4,
0x1f, //cc b1
0xe1,
0x1f, //cc g3
0xf4,
0x04, //cc g2
0x2b,
0x1f, //cc g1
0xe1,
0x1f, //cc r3
0xf4,
0x1f, //cc r2
0xc4,
0x04, //cc r1
0x48
 };

static u8 ui_tuning[] = {
0xCD,	                                                                
0x5A, //password 5A
0x00, //mask 000
0x00, //data_width
0x03, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
0x00, //roi_ctrl
0x00, //roi1 y end
0x00, 
0x00, //roi1 y start
0x00,
0x00, //roi1 x end
0x00,
0x00, //roi1 x start
0x00,
0x00, //roi0 y end
0x00,
0x00, //roi0 y start
0x00,
0x00, //roi0 x end
0x00,
0x00, //roi0 x start
0x00,
0x00, //scr Kb
0xFF, //scr Wb
0x00, //scr Kg
0xFF, //scr Wg
0x00, //scr Kr
0xFF, //scr Wr
0xFF, //scr Bb
0x00, //scr Yb
0x00, //scr Bg
0xFF, //scr Yg
0x00, //scr Br
0xFF, //scr Yr
0x00, //scr Gb
0xFF, //scr Mb
0xFF, //scr Gg
0x00, //scr Mg
0x00, //scr Gr
0xFF, //scr Mr
0x00, //scr Rb
0xFF, //scr Cb
0x00, //scr Rg
0xFF, //scr Cg
0xFF, //scr Rr
0x00, //scr Cr
0x02, //sharpen_set cc_en gamma_en 00 0 0
0xff, //curve24 a
0x00, //curve24 b
0x20, //curve23 a
0x00, //curve23 b
0x20, //curve22 a
0x00, //curve22 b
0x20, //curve21 a
0x00, //curve21 b
0xa1, //curve20 a
0x05, //curve20 b
0xa2, //curve19 a
0x09, //curve19 b
0xa0, //curve18 a
0x04, //curve18 b
0xa0, //curve17 a
0x04, //curve17 b
0xac, //curve16 a
0x1c, //curve16 b
0xac, //curve15 a
0x1c, //curve15 b
0xa8, //curve14 a
0x15, //curve14 b
0xa8, //curve13 a
0x15, //curve13 b
0xa8, //curve12 a
0x15, //curve12 b
0xa0, //curve11 a
0x0a, //curve11 b
0xa0, //curve10 a
0x0a, //curve10 b
0x98, //curve 9 a
0x01, //curve 9 b
0x98, //curve 8 a
0x01, //curve 8 b
0x98, //curve 7 a
0x01, //curve 7 b
0x98, //curve 6 a
0x01, //curve 6 b
0x98, //curve 5 a
0x01, //curve 5 b
0x98, //curve 4 a
0x01, //curve 4 b
0x98, //curve 3 a
0x01, //curve 3 b
0x98, //curve 2 a
0x01, //curve 2 b
0x08, //curve 1 a
0x00, //curve 1 b
0x04, //cc b3 0.1
0x5b,
0x1f, //cc b2
0xc4,
0x1f, //cc b1
0xe1,
0x1f, //cc g3
0xf4,
0x04, //cc g2
0x2b,
0x1f, //cc g1
0xe1,
0x1f, //cc r3
0xf4,
0x1f, //cc r2
0xc4,
0x04, //cc r1
0x48
};

static u8 gallery_tuning[] = {
0xCD,	  
0x5A, //password 5A
0x00, //mask 000
0x00, //data_width
0x03, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
0x00, //roi_ctrl
0x00, //roi1 y end
0x00, 
0x00, //roi1 y start
0x00,
0x00, //roi1 x end
0x00,
0x00, //roi1 x start
0x00,
0x00, //roi0 y end
0x00,
0x00, //roi0 y start
0x00,
0x00, //roi0 x end
0x00,
0x00, //roi0 x start
0x00,
0x00, //scr Kb
0xFF, //scr Wb
0x00, //scr Kg
0xFF, //scr Wg
0x00, //scr Kr
0xFF, //scr Wr
0xFF, //scr Bb
0x00, //scr Yb
0x00, //scr Bg
0xFF, //scr Yg
0x00, //scr Br
0xFF, //scr Yr
0x00, //scr Gb
0xFF, //scr Mb
0xFF, //scr Gg
0x00, //scr Mg
0x00, //scr Gr
0xFF, //scr Mr
0x00, //scr Rb
0xFF, //scr Cb
0x00, //scr Rg
0xFF, //scr Cg
0xFF, //scr Rr
0x00, //scr Cr
0x02, //sharpen_set cc_en gamma_en 00 0 0
0xff, //curve24 a
0x00, //curve24 b
0x20, //curve23 a
0x00, //curve23 b
0x20, //curve22 a
0x00, //curve22 b
0x20, //curve21 a
0x00, //curve21 b
0xa1, //curve20 a
0x05, //curve20 b
0xa2, //curve19 a
0x09, //curve19 b
0xa0, //curve18 a
0x04, //curve18 b
0xa0, //curve17 a
0x04, //curve17 b
0xac, //curve16 a
0x1c, //curve16 b
0xac, //curve15 a
0x1c, //curve15 b
0xa8, //curve14 a
0x15, //curve14 b
0xa8, //curve13 a
0x15, //curve13 b
0xa8, //curve12 a
0x15, //curve12 b
0xa0, //curve11 a
0x0a, //curve11 b
0xa0, //curve10 a
0x0a, //curve10 b
0x98, //curve 9 a
0x01, //curve 9 b
0x98, //curve 8 a
0x01, //curve 8 b
0x98, //curve 7 a
0x01, //curve 7 b
0x98, //curve 6 a
0x01, //curve 6 b
0x98, //curve 5 a
0x01, //curve 5 b
0x98, //curve 4 a
0x01, //curve 4 b
0x98, //curve 3 a
0x01, //curve 3 b
0x98, //curve 2 a
0x01, //curve 2 b
0x08, //curve 1 a
0x00, //curve 1 b
0x04, //cc b3 0.1
0x5b,
0x1f, //cc b2
0xc4,
0x1f, //cc b1
0xe1,
0x1f, //cc g3
0xf4,
0x04, //cc g2
0x2b,
0x1f, //cc g1
0xe1,
0x1f, //cc r3
0xf4,
0x1f, //cc r2
0xc4,
0x04, //cc r1
0x48
 };

static u8 camera_tuning[] = {
0xCD,	                                                                  
0x5A, //password 5A
0x00, //mask 000
0x00, //data_width
0x03, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
0x00, //roi_ctrl
0x00, //roi1 y end
0x00, 
0x00, //roi1 y start
0x00,
0x00, //roi1 x end
0x00,
0x00, //roi1 x start
0x00,
0x00, //roi0 y end
0x00,
0x00, //roi0 y start
0x00,
0x00, //roi0 x end
0x00,
0x00, //roi0 x start
0x00,
0x00, //scr Kb
0xFF, //scr Wb
0x00, //scr Kg
0xFF, //scr Wg
0x00, //scr Kr
0xFF, //scr Wr
0xFF, //scr Bb
0x00, //scr Yb
0x00, //scr Bg
0xFF, //scr Yg
0x00, //scr Br
0xFF, //scr Yr
0x00, //scr Gb
0xFF, //scr Mb
0xFF, //scr Gg
0x00, //scr Mg
0x00, //scr Gr
0xFF, //scr Mr
0x00, //scr Rb
0xFF, //scr Cb
0x00, //scr Rg
0xFF, //scr Cg
0xFF, //scr Rr
0x00, //scr Cr
0x06, //sharpen_set cc_en gamma_en 00 0 0
0xff, //curve24 a
0x00, //curve24 b
0x20, //curve23 a
0x00, //curve23 b
0x20, //curve22 a
0x00, //curve22 b
0x20, //curve21 a
0x00, //curve21 b
0xa1, //curve20 a
0x05, //curve20 b
0xa2, //curve19 a
0x09, //curve19 b
0xa0, //curve18 a
0x04, //curve18 b
0xa0, //curve17 a
0x04, //curve17 b
0xac, //curve16 a
0x1c, //curve16 b
0xac, //curve15 a
0x1c, //curve15 b
0xa8, //curve14 a
0x15, //curve14 b
0xa8, //curve13 a
0x15, //curve13 b
0xa8, //curve12 a
0x15, //curve12 b
0xa0, //curve11 a
0x0a, //curve11 b
0xa0, //curve10 a
0x0a, //curve10 b
0x98, //curve 9 a
0x01, //curve 9 b
0x98, //curve 8 a
0x01, //curve 8 b
0x98, //curve 7 a
0x01, //curve 7 b
0x98, //curve 6 a
0x01, //curve 6 b
0x98, //curve 5 a
0x01, //curve 5 b
0x98, //curve 4 a
0x01, //curve 4 b
0x98, //curve 3 a
0x01, //curve 3 b
0x98, //curve 2 a
0x01, //curve 2 b
0x08, //curve 1 a
0x00, //curve 1 b
0x04, //cc b3 0.1
0x5b,
0x1f, //cc b2
0xc4,
0x1f, //cc b1
0xe1,
0x1f, //cc g3
0xf4,
0x04, //cc g2
0x2b,
0x1f, //cc g1
0xe1,
0x1f, //cc r3
0xf4,
0x1f, //cc r2
0xc4,
0x04, //cc r1
0x48
 };

static u8 bypass_tuning[] = {
0xCD,	                                                                  
0x5A, //password 5A
0x00, //mask 000
0x00, //data_width
0x00, //scr_roi 1 scr algo_roi 1 algo 00 1 0 00 1 0
0x00, //roi_ctrl
0x00, //roi1 y end
0x00, 
0x00, //roi1 y start
0x00,
0x00, //roi1 x end
0x00,
0x00, //roi1 x start
0x00,
0x00, //roi0 y end
0x00,
0x00, //roi0 y start
0x00,
0x00, //roi0 x end
0x00,
0x00, //roi0 x start
0x00,
0x00, //scr Kb
0xFF, //scr Wb
0x00, //scr Kg
0xFF, //scr Wg
0x00, //scr Kr
0xFF, //scr Wr
0xFF, //scr Bb
0x00, //scr Yb
0x00, //scr Bg
0xFF, //scr Yg
0x00, //scr Br
0xFF, //scr Yr
0x00, //scr Gb
0xFF, //scr Mb
0xFF, //scr Gg
0x00, //scr Mg
0x00, //scr Gr
0xFF, //scr Mr
0x00, //scr Rb
0xFF, //scr Cb
0x00, //scr Rg
0xFF, //scr Cg
0xFF, //scr Rr
0x00, //scr Cr
0x00, //sharpen_set cc_en gamma_en 00 0 0
0x20, //curve24 a
0x00, //curve24 b
0x20, //curve23 a
0x00, //curve23 b
0x20, //curve22 a
0x00, //curve22 b
0x20, //curve21 a
0x00, //curve21 b
0x20, //curve20 a
0x00, //curve20 b
0x20, //curve19 a
0x00, //curve19 b
0x20, //curve18 a
0x00, //curve18 b
0x20, //curve17 a
0x00, //curve17 b
0x20, //curve16 a
0x00, //curve16 b
0x20, //curve15 a
0x00, //curve15 b
0x20, //curve14 a
0x00, //curve14 b
0x20, //curve13 a
0x00, //curve13 b
0x20, //curve12 a
0x00, //curve12 b
0x20, //curve11 a
0x00, //curve11 b
0x20, //curve10 a
0x00, //curve10 b
0x20, //curve 9 a
0x00, //curve 9 b
0x20, //curve 8 a
0x00, //curve 8 b
0x20, //curve 7 a
0x00, //curve 7 b
0x20, //curve 6 a
0x00, //curve 6 b
0x20, //curve 5 a
0x00, //curve 5 b
0x20, //curve 4 a
0x00, //curve 4 b
0x20, //curve 3 a
0x00, //curve 3 b
0x20, //curve 2 a
0x00, //curve 2 b
0x20, //curve 1 a
0x00, //curve 1 b
0x04, //cc b3
0x00, 
0x00, //cc b2
0x00,
0x00, //cc b1
0x00,
0x00, //cc g3
0x00,
0x04, //cc g2
0x00,
0x00, //cc g1
0x00,
0x00, //cc r3
0x00,
0x00, //cc r2
0x00,
0x04, //cc r1
0x00
};                                                                          

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


static const struct specific_cmdset cabc_on[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  seteco,  sizeof(seteco) },
	{ MIPI_DSI_DCS_LONG_WRITE,  set_cabc,  sizeof(set_cabc) },
	{ MIPI_DSI_DCS_LONG_WRITE,  cabc_ui,  sizeof(cabc_ui) },
	{ MIPI_DSI_END,             NULL,      0                }
};

static const struct specific_cmdset cabc_ui_cmdset[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  cabc_ui,  sizeof(cabc_ui) },
	{ MIPI_DSI_END,             NULL,      0                }
};

static const struct specific_cmdset cabc_off_cmdset[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  cabc_off,  sizeof(cabc_off) },
	{ MIPI_DSI_END,             NULL,      0                }
};

static const struct specific_cmdset camera_tuning_cmdset[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  camera_tuning,  sizeof(camera_tuning) },
	{ MIPI_DSI_END,             NULL,      0                }
};

static const struct specific_cmdset gallery_tuning_cmdset[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  gallery_tuning,  sizeof(gallery_tuning) },
	{ MIPI_DSI_END,             NULL,      0                }
};

static const struct specific_cmdset ui_tuning_cmdset[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  ui_tuning,  sizeof(ui_tuning) },
	{ MIPI_DSI_END,             NULL,      0                }
};

static const struct specific_cmdset video_tuning_cmdset[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  video_tuning,  sizeof(video_tuning) },
	{ MIPI_DSI_END,             NULL,      0                }
};

static const struct specific_cmdset bypass_tuning_cmdset[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  bypass_tuning,  sizeof(bypass_tuning) },
	{ MIPI_DSI_END,             NULL,      0                }
};


static const struct specific_cmdset negative_tuning_cmdset[] = {
		{ MIPI_DSI_DCS_LONG_WRITE,  negative_tuning,  sizeof(negative_tuning) },
	{ MIPI_DSI_END,             NULL,      0                }
};


#endif /* __MDNIE_TABLE_H__ */                                                     
    
