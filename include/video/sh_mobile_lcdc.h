#ifndef __ASM_SH_MOBILE_LCDC_H__
#define __ASM_SH_MOBILE_LCDC_H__

#include <linux/fb.h>

/* Register definitions */
#define _LDDCKR			0x410
#define LDDCKR_ICKSEL_BUS	(0 << 16)
#define LDDCKR_ICKSEL_MIPI	(1 << 16)
#define LDDCKR_ICKSEL_HDMI	(2 << 16)
#define LDDCKR_ICKSEL_EXT	(3 << 16)
#define LDDCKR_ICKSEL_MASK	(7 << 16)
#define LDDCKR_MOSEL		(1 << 6)
#define _LDDCKSTPR		0x414
#define _LDINTR			0x468
#define LDINTR_FE		(1 << 10)
#define LDINTR_VSE		(1 << 9)
#define LDINTR_VEE		(1 << 8)
#define LDINTR_FS		(1 << 2)
#define LDINTR_VSS		(1 << 1)
#define LDINTR_VES		(1 << 0)
#define LDINTR_STATUS_MASK	(0xff << 0)
#define _LDSR			0x46c
#define LDSR_MSS		(1 << 10)
#define LDSR_MRS		(1 << 8)
#define LDSR_AS			(1 << 1)
#define _LDCNT1R		0x470
#define LDCNT1R_DE		(1 << 0)
#define _LDCNT2R		0x474
#define LDCNT2R_BR		(1 << 8)
#define LDCNT2R_MD		(1 << 3)
#define LDCNT2R_SE		(1 << 2)
#define LDCNT2R_ME		(1 << 1)
#define LDCNT2R_DO		(1 << 0)
#define _LDRCNTR		0x478
#define LDRCNTR_SRS		(1 << 17)
#define LDRCNTR_SRC		(1 << 16)
#define LDRCNTR_MRS		(1 << 1)
#define LDRCNTR_MRC		(1 << 0)
#define _LDDDSR			0x47c
#define LDDDSR_LS		(1 << 2)
#define LDDDSR_WS		(1 << 1)
#define LDDDSR_BS		(1 << 0)

#define LDMT1R_VPOL		(1 << 28)
#define LDMT1R_HPOL		(1 << 27)
#define LDMT1R_DWPOL		(1 << 26)
#define LDMT1R_DIPOL		(1 << 25)
#define LDMT1R_DAPOL		(1 << 24)
#define LDMT1R_HSCNT		(1 << 17)
#define LDMT1R_DWCNT		(1 << 16)
#define LDMT1R_IFM		(1 << 12)
#define LDMT1R_MIFTYP_RGB8	(0x0 << 0)
#define LDMT1R_MIFTYP_RGB9	(0x4 << 0)
#define LDMT1R_MIFTYP_RGB12A	(0x5 << 0)
#define LDMT1R_MIFTYP_RGB12B	(0x6 << 0)
#define LDMT1R_MIFTYP_RGB16	(0x7 << 0)
#define LDMT1R_MIFTYP_RGB18	(0xa << 0)
#define LDMT1R_MIFTYP_RGB24	(0xb << 0)
#define LDMT1R_MIFTYP_YCBCR	(0xf << 0)
#define LDMT1R_MIFTYP_SYS8A	(0x0 << 0)
#define LDMT1R_MIFTYP_SYS8B	(0x1 << 0)
#define LDMT1R_MIFTYP_SYS8C	(0x2 << 0)
#define LDMT1R_MIFTYP_SYS8D	(0x3 << 0)
#define LDMT1R_MIFTYP_SYS9	(0x4 << 0)
#define LDMT1R_MIFTYP_SYS12	(0x5 << 0)
#define LDMT1R_MIFTYP_SYS16A	(0x7 << 0)
#define LDMT1R_MIFTYP_SYS16B	(0x8 << 0)
#define LDMT1R_MIFTYP_SYS16C	(0x9 << 0)
#define LDMT1R_MIFTYP_SYS18	(0xa << 0)
#define LDMT1R_MIFTYP_SYS24	(0xb << 0)
#define LDMT1R_MIFTYP_MASK	(0xf << 0)

#define LDDFR_CF1		(1 << 18)
#define LDDFR_CF0		(1 << 17)
#define LDDFR_CC		(1 << 16)
#define LDDFR_YF_420		(0 << 8)
#define LDDFR_YF_422		(1 << 8)
#define LDDFR_YF_444		(2 << 8)
#define LDDFR_YF_MASK		(3 << 8)
#define LDDFR_PKF_ARGB32	(0x00 << 0)
#define LDDFR_PKF_RGB16		(0x03 << 0)
#define LDDFR_PKF_RGB24		(0x0b << 0)
#define LDDFR_PKF_MASK		(0x1f << 0)

#define LDSM1R_OS		(1 << 0)

#define LDSM2R_OSTRG		(1 << 0)

#define LDPMR_LPS		(3 << 0)

#define _LDDWD0R		0x800
#define LDDWDxR_WDACT		(1 << 28)
#define LDDWDxR_RSW		(1 << 24)
#define _LDDRDR			0x840
#define LDDRDR_RSR		(1 << 24)
#define LDDRDR_DRD_MASK		(0x3ffff << 0)
#define _LDDWAR			0x900
#define LDDWAR_WA		(1 << 0)
#define _LDDRAR			0x904
#define LDDRAR_RA		(1 << 0)

enum {
	RGB8	= LDMT1R_MIFTYP_RGB8,	/* 24bpp, 8:8:8 */
	RGB9	= LDMT1R_MIFTYP_RGB9,	/* 18bpp, 9:9 */
	RGB12A	= LDMT1R_MIFTYP_RGB12A,	/* 24bpp, 12:12 */
	RGB12B	= LDMT1R_MIFTYP_RGB12B,	/* 12bpp */
	RGB16	= LDMT1R_MIFTYP_RGB16,	/* 16bpp */
	RGB18	= LDMT1R_MIFTYP_RGB18,	/* 18bpp */
	RGB24	= LDMT1R_MIFTYP_RGB24,	/* 24bpp */
	YUV422	= LDMT1R_MIFTYP_YCBCR,	/* 16bpp */
	SYS8A	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS8A,	/* 24bpp, 8:8:8 */
	SYS8B	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS8B,	/* 18bpp, 8:8:2 */
	SYS8C	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS8C,	/* 18bpp, 2:8:8 */
	SYS8D	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS8D,	/* 16bpp, 8:8 */
	SYS9	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS9,	/* 18bpp, 9:9 */
	SYS12	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS12,	/* 24bpp, 12:12 */
	SYS16A	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS16A,	/* 16bpp */
	SYS16B	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS16B,	/* 18bpp, 16:2 */
	SYS16C	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS16C,	/* 18bpp, 2:16 */
	SYS18	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS18,	/* 18bpp */
	SYS24	= LDMT1R_IFM | LDMT1R_MIFTYP_SYS24,	/* 24bpp */
};

/* Header Section */
struct lcdrt_sectioninfo {
	u32 variableareaaddr;	/* Address of Variable Area */
	u32 variableareasize;	/* Size of Variable Area */
	u32 fixedareaaddr;	/* Address of Fixed Area */
	u32 fixedareasize;	/* Size of Fixed Area */
	u32 commandareaaddr;	/* Address of Command Transfer Area */
	u32 commandareasize;	/* Size of Command Transfer Area */
	u32 onscrnaddr;		/* Address of OnScreen Buffer */
	u32 onscrnsize;		/* Size of OnScreen Buffer */
};

enum {	LCDC_CHAN_DISABLED = 0,
	LCDC_CHAN_MAINLCD,
	LCDC_CHAN_SUBLCD };

enum { LCDC_CLK_BUS, LCDC_CLK_PERIPHERAL, LCDC_CLK_EXTERNAL };

#define LCDC_FLAGS_DWPOL (1 << 0) /* Rising edge dot clock data latch */
#define LCDC_FLAGS_DIPOL (1 << 1) /* Active low display enable polarity */
#define LCDC_FLAGS_DAPOL (1 << 2) /* Active low display data polarity */
#define LCDC_FLAGS_HSCNT (1 << 3) /* Disable HSYNC during VBLANK */
#define LCDC_FLAGS_DWCNT (1 << 4) /* Disable dotclock during blanking */

/*Main display*/
#define SH_MLCD_WIDTH		480
#define SH_MLCD_HEIGHT		800

#define SH_MLCD_TRCOLOR		0
#define SH_MLCD_REPLACECOLOR	0
#define SH_MLCD_RECTX		0
#define SH_MLCD_RECTY		0

/*Sub display*/
#define SH_SLCD_WIDTH		480
#define SH_SLCD_HEIGHT		864

#define SH_SLCD_TRCOLOR		0
#define SH_SLCD_REPLACECOLOR	0
#define SH_SLCD_RECTX		0
#define SH_SLCD_RECTY		0

#define SH_FB_HDMI_START	1
#define SH_FB_HDMI_STOP		2

#define SH_FB_VSYNC_OFF         0
#define SH_FB_VSYNC_ON          1

enum {
	SH_FB_HDMI_480P60,
	SH_FB_HDMI_720P60,
	SH_FB_HDMI_1080I60,
	SH_FB_HDMI_1080P24,
	SH_FB_HDMI_576P50,
	SH_FB_HDMI_720P50,
	SH_FB_HDMI_1080P60,
	SH_FB_HDMI_1080P50,
	SH_FB_HDMI_480P60A43,
	SH_FB_HDMI_576P50A43,
};

#define FBIO_WAITFORVSYNC _IOW('F', 0x20, __u32)

struct sh_mobile_lcdc_sys_bus_ops {
	void (*write_index)(void *handle, unsigned long data);
	void (*write_data)(void *handle, unsigned long data);
	unsigned long (*read_data)(void *handle);
};

struct sh_mobile_lcdc_panel_cfg {
	unsigned long width;		/* Panel width in mm */
	unsigned long height;		/* Panel height in mm */
	int (*setup_sys)(void *sys_ops_handle,
			 struct sh_mobile_lcdc_sys_bus_ops *sys_ops);
	void (*start_transfer)(void *sys_ops_handle,
			       struct sh_mobile_lcdc_sys_bus_ops *sys_ops);
	void (*display_on)(void);
	void (*display_off)(void);
};

struct sh_mobile_lcdc_chan_cfg {
	int chan;
	int fourcc;
	int bpp;
	int interface_type; /* selects RGBn or SYSn I/F, see above */
	int clock_divider;
	unsigned long flags; /* LCDC_FLAGS_... */
	struct fb_videomode *lcd_modes;
	int num_modes;
	struct sh_mobile_lcdc_panel_cfg panel_cfg;
	unsigned long panelreset_gpio;
	unsigned long paneldsi_irq;

	struct platform_device *tx_dev;	/* HDMI/DSI transmitter device */
};

struct sh_mobile_lcdc_info {
	int clock_source;
	struct sh_mobile_lcdc_chan_cfg ch[2];
};

struct fb_hdmi_set_mode {
	unsigned int start;
	unsigned int format;
};

struct fb_panel_hw_info {
	unsigned int gpio_reg;
	unsigned int dsi_irq;
};

struct fb_panel_func {
	int (*panel_init)(unsigned int mem_size);
	int (*panel_suspend)(void);
	int (*panel_resume)(void);
	int (*panel_probe)(struct fb_info *info,
			struct fb_panel_hw_info hw_info);
	int (*panel_remove)(struct fb_info *info);
	struct fb_panel_info (*panel_info)(void);
};

struct fb_panel_info {
	unsigned int pixel_width;
	unsigned int pixel_height;
	unsigned int size_width;
	unsigned int size_height;
	unsigned int buff_address;
	unsigned int pixclock;
	unsigned int left_margin;
	unsigned int right_margin;
	unsigned int upper_margin;
	unsigned int lower_margin;
	unsigned int hsync_len;
	unsigned int vsync_len;
};

struct fb_hdmi_func {
	int (*hdmi_set)(unsigned int format);
	int (*hdmi_suspend)(void);
	int (*hdmi_resume)(void);
};


#define IOC_SH_MOBILE_FB_MAGIC 'S'

#define SH_MOBILE_FB_HDMI_SET \
	_IOW(IOC_SH_MOBILE_FB_MAGIC, 0x00, struct fb_hdmi_set_mode)

#define SH_MOBILE_FB_ENABLEVSYNC \
	_IOW(IOC_SH_MOBILE_FB_MAGIC, 0x01, __u32)

extern int sh_mobile_lcdc_keyclr_set(unsigned short s_key_clr,
				unsigned short output_mode);
extern int sh_mobile_lcdc_alpha_set(unsigned short s_alpha,
				unsigned short output_mode);
extern int sh_mobile_lcdc_refresh(unsigned short set_state,
				unsigned short output_mode);

extern struct fb_panel_func r_mobile_panel_func(int panel);
extern struct fb_hdmi_func r_mobile_hdmi_func(void);

extern void r_mobile_fb_err_msg(int value, char *func_name);

/*extern struct semaphore   sh_mobile_sem_hdmi;*/

#endif /* __ASM_SH_MOBILE_LCDC_H__ */
