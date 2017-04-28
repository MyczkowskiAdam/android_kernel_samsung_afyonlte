/*
 * drivers/video/r-mobile/r-mobile_lcdcfb.c
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
/*
 * SuperH Mobile LCDC Framebuffer
 *
 * Copyright (c) 2008 Magnus Damm
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */


#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/ioctl.h>
#include <linux/slab.h>
#include <video/sh_mobile_lcdc.h>
/*#include <media/sh_mobile_overlay.h>*/
#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kthread.h>

#if defined (CONFIG_SEC_DEBUG)
#include <mach/sec_debug.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */

#include <rtapi/screen_display.h>

#define FB_SH_MOBILE_REFRESH 0

#define REFRESH_TIME_MSEC 100

#if FB_SH_MOBILE_REFRESH
#include <linux/mfis.h>
#define COUNT_MFIS_SUSPEND 10
#endif

#ifdef CONFIG_SAMSUNG_MHL 
#define FB_SH_MOBILE_HDMI 1
#else
#define FB_SH_MOBILE_HDMI 0
#endif

#define CHAN_NUM 2

#define FB_HDMI_STOP  0
#define FB_HDMI_START 1

#define PALETTE_NR 16
#define SIDE_B_OFFSET 0x1000
#define MIRROR_OFFSET 0x2000

#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
#include <linux/sh_mobile_composer.h>
#endif

#define FB_DRM_NUMBER		0x12345678
#define FB_BLACK_NUMBER		0x55555555

#define IRQ_BIT_NUM		0

#define IRQC10_BASE		0x1400
#define IRQC10_INTREQ_STS0	(IRQC10_BASE + 0x000)
#define IRQC10_INTEN_STS0	(IRQC10_BASE + 0x004)
#define IRQC10_INTEN_SET0	(IRQC10_BASE + 0x008)
#define IRQC10_WAKEN_STS0	(IRQC10_BASE + 0x080)
#define IRQC10_WAKEN_SET0	(IRQC10_BASE + 0x088)
#define IRQC10_CONFIG		(IRQC10_BASE + 0x180 + (0x04 * IRQ_BIT_NUM))

#define IRQC_CPORT0_BASE	 0x2000
#define IRQC_CPORT0_STS0	(IRQC_CPORT0_BASE + 0x000)

static char __iomem *irqc_baseaddr;

struct sh_mobile_lcdc_priv;
struct sh_mobile_lcdc_chan {
	struct sh_mobile_lcdc_priv *lcdc;
	struct sh_mobile_lcdc_chan_cfg cfg;
	u32 pseudo_palette[PALETTE_NR];
	struct fb_info *info;
	dma_addr_t dma_handle;
	unsigned long pan_offset;
};

struct sh_mobile_lcdc_priv {
	struct device *dev;
	struct sh_mobile_lcdc_chan ch[CHAN_NUM];
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend    early_suspend;
#endif /* CONFIG_HAS_EARLYSUSPEND */
	int irq;
};

struct sh_mobile_lcdc_ext_param {
	struct semaphore sem_lcd;
	int lcd_type;
	void *aInfo;
	unsigned short o_mode;
	unsigned short draw_mode;
	unsigned int phy_addr;
	unsigned int vir_addr;
	unsigned short rect_x;
	unsigned short rect_y;
	unsigned short rect_width;
	unsigned short rect_height;
	unsigned short alpha;
	unsigned short key_clr;
	unsigned short v4l2_state;
	unsigned short draw_bpp;
	unsigned int mem_size;
	unsigned short delay_flag;
	unsigned short refresh_on;
	struct delayed_work ref_work;
	unsigned short mfis_err_flag;
	unsigned short rotate;
	struct fb_panel_func panel_func;
	struct fb_hdmi_func hdmi_func;
	unsigned short hdmi_flag;
};

struct sh_mobile_lcdc_ext_param lcd_ext_param[CHAN_NUM];

struct semaphore   sh_mobile_sem_hdmi;

#if FB_SH_MOBILE_REFRESH
struct workqueue_struct *sh_mobile_wq;
#endif

struct sh_mobile_lcdc_vsync {
	struct task_struct *vsync_thread;
	wait_queue_head_t vsync_wait;
	ktime_t vsync_time;
	int vsync_flag;
};

static struct sh_mobile_lcdc_vsync sh_lcd_vsync;

static u32 fb_debug;
#define DBG_PRINT(FMT, ARGS...)	      \
	do { \
		if (fb_debug)						\
			printk(KERN_INFO "%s(): " FMT, __func__, ##ARGS); \
	} while (0)

module_param(fb_debug, int, 0644);
MODULE_PARM_DESC(fb_debug, "SH LCD debug level");

static unsigned long RoundUpToMultiple(unsigned long x, unsigned long y);
static unsigned long GCD(unsigned long x, unsigned long y);
static unsigned long LCM(unsigned long x, unsigned long y);

void r_mobile_fb_err_msg(int value, char *func_name)
{

	switch (value) {
	case -1:
		printk(KERN_INFO "FB %s ret = -1 SMAP_LIB_DISPLAY_NG\n",
		       func_name);
		break;
	case -2:
		printk(KERN_INFO "FB %s ret = -2 SMAP_LIB_DISPLAY_PARAERR\n",
		       func_name);
		break;
	case -3:
		printk(KERN_INFO "FB %s ret = -3 SMAP_LIB_DISPLAY_SEQERR\n",
		       func_name);
		break;
	default:
		printk(KERN_INFO "FB %s ret = %d unknown RT-API error\n",
		       func_name, value);
		break;
	}

	return;
}

static int vsync_recv_thread(void *data)
{
	int ret;
	struct sh_mobile_lcdc_priv *priv = data;

	while (!kthread_should_stop()) {
		sh_lcd_vsync.vsync_flag = 0;
		ret = wait_event_interruptible(sh_lcd_vsync.vsync_wait,
					       sh_lcd_vsync.vsync_flag);
		if (!ret)
			sysfs_notify(&priv->dev->kobj, NULL, "vsync");
	}

	return 0;
}

static irqreturn_t sh_mobile_lcdc_irq(int irq, void *data)
{

	int intreq_sts;
	int loop_num = 10000;

	intreq_sts = __raw_readl(irqc_baseaddr + IRQC10_INTREQ_STS0)
		& (0x1 << IRQ_BIT_NUM);

	if (intreq_sts) {
		sh_lcd_vsync.vsync_time = ktime_get();
		sh_lcd_vsync.vsync_flag = 1;
		wake_up_interruptible(&sh_lcd_vsync.vsync_wait);

		__raw_writel(0x1 << IRQ_BIT_NUM,
			     irqc_baseaddr + IRQC_CPORT0_STS0);
		do {
			intreq_sts = __raw_readl(irqc_baseaddr
						 + IRQC10_INTREQ_STS0)
				& (0x1 << IRQ_BIT_NUM);
		} while (intreq_sts && loop_num--);
	}

	return IRQ_HANDLED;
}

static ssize_t sh_fb_vsync_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%llu\n",
			 ktime_to_ns(sh_lcd_vsync.vsync_time));
}

static DEVICE_ATTR(vsync, S_IRUGO, sh_fb_vsync_show, NULL);

static int sh_mobile_lcdc_setcolreg(u_int regno,
				    u_int red, u_int green, u_int blue,
				    u_int transp, struct fb_info *info)
{
	/* No. of hw registers */
	if (regno >= 256)
		return 1;

	/* grayscale works only partially under directcolor */
	if (info->var.grayscale) {
		/* grayscale = 0.30*R + 0.59*G + 0.11*B */
		red = green = blue = (red * 77 + green * 151 + blue * 28) >> 8;
	}

#define CNVT_TOHW(val, width) ((((val)<<(width))+0x7FFF-(val))>>16)
	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:	/* FALL THROUGH */
	case FB_VISUAL_PSEUDOCOLOR:
	{
		red = CNVT_TOHW(red, info->var.red.length);
		green = CNVT_TOHW(green, info->var.green.length);
		blue = CNVT_TOHW(blue, info->var.blue.length);
		transp = CNVT_TOHW(transp, info->var.transp.length);
		break;
	}
	case FB_VISUAL_DIRECTCOLOR:
	{
		red = CNVT_TOHW(red, 8);	/* expect 8 bit DAC */
		green = CNVT_TOHW(green, 8);
		blue = CNVT_TOHW(blue, 8);
		/* hey, there is bug in transp handling... */
		transp = CNVT_TOHW(transp, 8);
		break;
	}
	}
#undef CNVT_TOHW
	/* Truecolor has hardware independent palette */
	if (info->fix.visual == FB_VISUAL_TRUECOLOR) {
		u32 v;

		if (regno >= 16)
			return 1;

		v = (red << info->var.red.offset) |
		    (green << info->var.green.offset) |
		    (blue << info->var.blue.offset) |
		    (transp << info->var.transp.offset);
		switch (info->var.bits_per_pixel) {
		case 16:	/* FALL THROUGH */
		case 24:	/* FALL THROUGH */
		case 32:
			((u32 *) (info->pseudo_palette))[regno] = v;
			break;
		case 8:		/* FALL THROUGH */
		default:
			break;
		}
	}

	return 0;
}

static struct fb_fix_screeninfo sh_mobile_lcdc_fix  = {
	.id		= "SH Mobile LCDC",
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_TRUECOLOR,
	.accel		= FB_ACCEL_NONE,
	.xpanstep	= 0,
	.ypanstep	= 1,
	.ywrapstep	= 0,
};

static int display_initialize(int lcd_num)
{
	screen_disp_delete disp_delete;

	int ret = 0;

	printk(KERN_INFO "enter display_initialize\n");

	lcd_ext_param[lcd_num].aInfo = screen_display_new();

	printk(KERN_INFO "%s, lcd_num=%d", __func__, lcd_num);

	if (lcd_ext_param[lcd_num].aInfo == NULL)
		return -2;

	if (lcd_ext_param[lcd_num].panel_func.panel_init) {
		ret = lcd_ext_param[lcd_num].panel_func.panel_init(
			lcd_ext_param[lcd_num].mem_size);
		if (ret != 0) {
			printk(KERN_ALERT "r_mobile_panel_init error\n");
			disp_delete.handle = lcd_ext_param[lcd_num].aInfo;
			screen_display_delete(&disp_delete);
			return -1;
		}
	}

	disp_delete.handle = lcd_ext_param[lcd_num].aInfo;
	screen_display_delete(&disp_delete);

	printk(KERN_INFO "exit display_initialize\n");

	return 0;

}

int sh_mobile_lcdc_keyclr_set(unsigned short s_key_clr,
			      unsigned short output_mode)
{

	return 0;

}
EXPORT_SYMBOL(sh_mobile_lcdc_keyclr_set);

int sh_mobile_lcdc_alpha_set(unsigned short s_alpha,
			      unsigned short output_mode)
{

	return 0;

}
EXPORT_SYMBOL(sh_mobile_lcdc_alpha_set);

#if FB_SH_MOBILE_REFRESH
static void refresh_work(struct work_struct *work)
{

	int ret;
	screen_disp_set_lcd_refresh disp_refresh;
	screen_disp_delete disp_delete;
	void *screen_handle;
	int loop_count = COUNT_MFIS_SUSPEND;

	struct sh_mobile_lcdc_ext_param *extp =
		container_of(work, struct sh_mobile_lcdc_ext_param,
			     ref_work.work);

	if (extp->aInfo != NULL) {
		screen_handle = screen_display_new();
		disp_refresh.handle = screen_handle;
		disp_refresh.output_mode = extp->o_mode;
		disp_refresh.refresh_mode = RT_DISPLAY_REFRESH_ON;
		ret = screen_display_set_lcd_refresh(&disp_refresh);
		if (ret != SMAP_LIB_DISPLAY_OK)
			r_mobile_fb_err_msg(ret,
					    "screen_display_set_lcd_refresh");
		DBG_PRINT("screen_display_set_lcd_refresh ON\n");
		disp_delete.handle = screen_handle;
		screen_display_delete(&disp_delete);
		ret = mfis_drv_suspend();
		extp->refresh_on = 1;

		while ((ret != 0 && extp->mfis_err_flag == 0) && loop_count) {
			DBG_PRINT("mfis_drv_suspend err :sleep 100mS\n");
			msleep(100);
			ret = mfis_drv_suspend();
			loop_count--;
		}
		if (!loop_count)
			printk(KERN_ALERT "##mfis_drv_suspend ERR%d\n", ret);
	}

	extp->delay_flag = 0;
	return;
}
#endif


int sh_mobile_lcdc_refresh(unsigned short set_state,
			   unsigned short output_mode)
{

#if FB_SH_MOBILE_REFRESH
	int i, ret;
	screen_disp_set_lcd_refresh disp_refresh;
	screen_disp_delete disp_delete;
	void *screen_handle;
	for (i = 0 ; i < CHAN_NUM ; i++) {
		if (output_mode == lcd_ext_param[i].o_mode)
			break;
	}
	if (i >= CHAN_NUM) {
		printk(KERN_ALERT "lcdc_key_clr_set param ERR\n");
		return -1;
	}
	if (down_interruptible(&lcd_ext_param[i].sem_lcd)) {
		printk(KERN_ALERT "down_interruptible failed\n");
		return -1;
	}

	lcd_ext_param[i].v4l2_state = set_state;
	if (lcd_ext_param[i].v4l2_state == RT_DISPLAY_REFRESH_OFF) {
		if (lcd_ext_param[i].delay_flag == 1) {
			lcd_ext_param[i].mfis_err_flag = 1;
			cancel_delayed_work_sync(&lcd_ext_param[i].ref_work);
			lcd_ext_param[i].delay_flag = 0;
		}
		if (lcd_ext_param[i].refresh_on == 1) {
			if (lcd_ext_param[i].aInfo != NULL) {
				ret = mfis_drv_resume();
				if (ret != 0) {
					printk(KERN_ALERT
					       "##mfis_drv_resume ERR%d\n",
					       ret);
				}
				screen_handle = screen_display_new();
				disp_refresh.handle = screen_handle;
				disp_refresh.output_mode
					= lcd_ext_param[i].o_mode;
				disp_refresh.refresh_mode
					= RT_DISPLAY_REFRESH_OFF;
				ret = screen_display_set_lcd_refresh(
					&disp_refresh);
				if (ret != SMAP_LIB_DISPLAY_OK) {
					r_mobile_fb_err_msg(
						ret,
						"screen_display_set_lcd_refresh");
					disp_delete.handle = screen_handle;
					screen_display_delete(&disp_delete);
					up(&lcd_ext_param[i].sem_lcd);
					return -1;
				}
				lcd_ext_param[i].refresh_on = 0;
				disp_delete.handle = screen_handle;
				screen_display_delete(&disp_delete);
			}
		}
	} else {
		queue_delayed_work(
			sh_mobile_wq, &lcd_ext_param[i].ref_work, 0);
		lcd_ext_param[i].delay_flag = 1;
		lcd_ext_param[i].mfis_err_flag = 0;
	}
	up(&lcd_ext_param[i].sem_lcd);
#endif
	return 0;

}
EXPORT_SYMBOL(sh_mobile_lcdc_refresh);

#if FB_SH_MOBILE_HDMI
int sh_mobile_fb_hdmi_set(struct fb_hdmi_set_mode *set_mode)
{

	void *hdmi_handle;
	screen_disp_stop_hdmi disp_stop_hdmi;
	screen_disp_delete disp_delete;
	int ret;

	if (set_mode->start != SH_FB_HDMI_START &&
	    set_mode->start != SH_FB_HDMI_STOP) {
		DBG_PRINT("set_mode->start param\n");
		return -1;
	}
	if (set_mode->start == SH_FB_HDMI_START &&
	    (set_mode->format < SH_FB_HDMI_480P60 ||
	     set_mode->format > SH_FB_HDMI_576P50A43)) {
		DBG_PRINT("set_mode->format param\n");
		return -1;
	}

	if (set_mode->start == SH_FB_HDMI_STOP) {
		if (down_interruptible(&lcd_ext_param[0].sem_lcd)) {
			printk(KERN_ALERT "down_interruptible failed\n");
			return -1;
		}
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI

		sh_mobile_composer_hdmiset(0);
#endif
#endif
		hdmi_handle = screen_display_new();
		disp_stop_hdmi.handle = hdmi_handle;
		ret = screen_display_stop_hdmi(&disp_stop_hdmi);
		if (ret != SMAP_LIB_DISPLAY_OK)
			r_mobile_fb_err_msg(ret, "screen_display_stop_hdmi");
		DBG_PRINT("screen_display_stop_hdmi ret = %d\n", ret);
		disp_delete.handle = hdmi_handle;
		screen_display_delete(&disp_delete);
		lcd_ext_param[0].hdmi_flag = FB_HDMI_STOP;
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI

		sh_mobile_composer_hdmiset(1);
#endif
#endif
		up(&lcd_ext_param[0].sem_lcd);
		sh_mobile_lcdc_refresh(
			RT_DISPLAY_REFRESH_ON, RT_DISPLAY_LCD1);
	} else {
		sh_mobile_lcdc_refresh(
			RT_DISPLAY_REFRESH_OFF, RT_DISPLAY_LCD1);
		if (lcd_ext_param[0].hdmi_func.hdmi_set) {
			ret = lcd_ext_param[0].hdmi_func.hdmi_set(
				set_mode->format);
			if (ret) {
				printk(KERN_ALERT " error\n");
				return -1;
			}
			lcd_ext_param[0].hdmi_flag = FB_HDMI_START;
		}
	}

	return 0;
}
#endif

static int sh_mobile_fb_sync_set(int vsyncval)
{

	int ret;

	if (vsyncval != SH_FB_VSYNC_OFF && vsyncval != SH_FB_VSYNC_ON)
		return -1;

	if (vsyncval == SH_FB_VSYNC_ON) {
		if (lcd_ext_param[0].aInfo == NULL) {
			ret = display_initialize(0);
			if (ret == -1) {
				printk(KERN_ALERT
				       "err sync display_initialize\n");
				return -1;
			} else if (ret == -2) {
				printk(KERN_ALERT "nothing MFI driver\n");
				return -1;
			}
		}

		__raw_writel(0x1 << IRQ_BIT_NUM,
			     irqc_baseaddr + IRQC_CPORT0_STS0);
		__raw_writel(0x1 << IRQ_BIT_NUM,
			     irqc_baseaddr + IRQC10_INTEN_SET0);
		__raw_writel(0x1 << IRQ_BIT_NUM,
			     irqc_baseaddr + IRQC10_WAKEN_SET0);

		__raw_writel(__raw_readl(irqc_baseaddr + IRQC10_CONFIG)
			     | 0x00000002, irqc_baseaddr + IRQC10_CONFIG);
	} else {
		__raw_writel(0x1 << IRQ_BIT_NUM,
			     irqc_baseaddr + IRQC10_INTEN_STS0);
		__raw_writel(0x1 << IRQ_BIT_NUM,
			     irqc_baseaddr + IRQC10_WAKEN_STS0);
		__raw_writel(0x1 << IRQ_BIT_NUM,
			     irqc_baseaddr + IRQC_CPORT0_STS0);
	}

	return 0;

}
static int sh_mobile_fb_pan_display(struct fb_var_screeninfo *var,
				     struct fb_info *info)
{
	struct sh_mobile_lcdc_chan *ch = info->par;
	unsigned long new_pan_offset;

	int ret = 0;

/* onscreen buffer 2 */
	unsigned int i;
	unsigned short set_format;
	unsigned char  lcd_num;
	screen_disp_draw disp_draw;
	screen_disp_delete disp_delete;
	void *screen_handle;
	unsigned short set_buff_id;
#if FB_SH_MOBILE_REFRESH
	screen_disp_set_lcd_refresh disp_refresh;
#endif
	new_pan_offset = (var->yoffset * info->fix.line_length) +
		(var->xoffset * (info->var.bits_per_pixel / 8));

	for (i = 0 ; i < CHAN_NUM ; i++) {
		if (ch->cfg.chan == lcd_ext_param[i].lcd_type)
			break;
	}
	lcd_num = i;
	if (lcd_num >= CHAN_NUM)
		return -EINVAL;

	if (down_interruptible(&lcd_ext_param[lcd_num].sem_lcd)) {
		printk(KERN_ALERT "down_interruptible failed\n");
		return -ERESTARTSYS;
	}

	/* DRM */
	if (var->reserved[1] == FB_DRM_NUMBER)
		set_buff_id = RT_DISPLAY_BUFFER_B;
	else if (var->reserved[1] == FB_BLACK_NUMBER)
		set_buff_id = RT_DISPLAY_DRAW_BLACK;
	else
		set_buff_id = RT_DISPLAY_BUFFER_A;

	/* Set the source address for the next refresh */

	if (lcd_ext_param[lcd_num].aInfo == NULL) {
		ret = display_initialize(lcd_num);
		if (ret == -1) {
			up(&lcd_ext_param[lcd_num].sem_lcd);
			return -EIO;
		} else if (ret == -2)
			printk(KERN_ALERT "nothing MFI driver\n");
	}
	if (lcd_ext_param[lcd_num].aInfo != NULL) {
		screen_handle = screen_display_new();
#if FB_SH_MOBILE_REFRESH
		if (lcd_ext_param[lcd_num].v4l2_state
		    == RT_DISPLAY_REFRESH_ON) {
			if (lcd_ext_param[lcd_num].delay_flag == 1) {
				lcd_ext_param[lcd_num].mfis_err_flag = 1;
				cancel_delayed_work_sync(
					&lcd_ext_param[lcd_num].ref_work);
				lcd_ext_param[lcd_num].delay_flag = 0;
			}
			if (lcd_ext_param[lcd_num].refresh_on == 1) {
				ret = mfis_drv_resume();
				if (ret != 0) {
					printk(KERN_ALERT
					       "##mfis_drv_resume "
					       "ERR%d\n", ret);
				}
				disp_refresh.handle = screen_handle;
				disp_refresh.output_mode
					= lcd_ext_param[lcd_num].o_mode;
				disp_refresh.refresh_mode
					= RT_DISPLAY_REFRESH_OFF;
				ret = screen_display_set_lcd_refresh(
					&disp_refresh);
				if (ret != SMAP_LIB_DISPLAY_OK) {
					r_mobile_fb_err_msg(
						ret,
						"screen_display_set_lcd_refresh");
					up(&lcd_ext_param[lcd_num].sem_lcd);
					disp_delete.handle = screen_handle;
					screen_display_delete(&disp_delete);
					return -EIO;
				}
				lcd_ext_param[lcd_num].refresh_on = 0;
			}
		}
#endif
		if (var->bits_per_pixel == 16)
			set_format = RT_DISPLAY_FORMAT_RGB565;
		else if (var->bits_per_pixel == 24)
			set_format = RT_DISPLAY_FORMAT_RGB888;
		else
			set_format = RT_DISPLAY_FORMAT_ARGB8888;

#ifdef CONFIG_FB_SH_MOBILE_DOUBLE_BUF
		disp_draw.handle = screen_handle;
		if (var->reserved[1] == FB_BLACK_NUMBER)
			disp_draw.output_mode = RT_DISPLAY_LCD1;
		else
			disp_draw.output_mode =
				lcd_ext_param[lcd_num].draw_mode;

		disp_draw.draw_rect.x = lcd_ext_param[lcd_num].rect_x;
		disp_draw.draw_rect.y = lcd_ext_param[lcd_num].rect_y;
		disp_draw.draw_rect.width =
			lcd_ext_param[lcd_num].rect_width;
		disp_draw.draw_rect.height =
			lcd_ext_param[lcd_num].rect_height;
		disp_draw.format = set_format;
		disp_draw.buffer_id = set_buff_id;
		disp_draw.buffer_offset = new_pan_offset;
		disp_draw.rotate = lcd_ext_param[lcd_num].rotate;
		DBG_PRINT("screen_display_draw handle %x\n",
			  (unsigned int)disp_draw.handle);
		DBG_PRINT("screen_display_draw output_mode %d\n",
			  disp_draw.output_mode);
		DBG_PRINT("screen_display_draw draw_rect.x %d\n",
			  disp_draw.draw_rect.x);
		DBG_PRINT("screen_display_draw draw_rect.y %d\n",
			  disp_draw.draw_rect.y);
		DBG_PRINT("screen_display_draw draw_rect.width %d\n",
			  disp_draw.draw_rect.width);
		DBG_PRINT("screen_display_draw draw_rect.height %d\n",
			  disp_draw.draw_rect.height);
		DBG_PRINT("screen_display_draw format %d\n", disp_draw.format);
		DBG_PRINT("screen_display_draw buffer_id %d\n",
			  disp_draw.buffer_id);
		DBG_PRINT("screen_display_draw buffer_offset %d\n",
			  disp_draw.buffer_offset);
		DBG_PRINT("screen_display_draw rotate %d\n", disp_draw.rotate);
		ret = screen_display_draw(&disp_draw);
		DBG_PRINT("screen_display_draw return %d\n", ret);
		if (ret != SMAP_LIB_DISPLAY_OK) {
			r_mobile_fb_err_msg(ret, "screen_display_draw");
			up(&lcd_ext_param[lcd_num].sem_lcd);
			disp_delete.handle = screen_handle;
			screen_display_delete(&disp_delete);
			return -EIO;
		}
#else

		memcpy((void *)lcd_ext_param[lcd_num].vir_addr,
		       (void *)(info->screen_base + new_pan_offset),
		       (lcd_ext_param[lcd_num].rect_width *
			lcd_ext_param[lcd_num].rect_height *
			var->bits_per_pixel / 8));

		disp_draw.handle = screen_handle;
		disp_draw.output_mode = lcd_ext_param[lcd_num].draw_mode;
		disp_draw.draw_rect.x = lcd_ext_param[lcd_num].rect_x;
		disp_draw.draw_rect.y = lcd_ext_param[lcd_num].rect_y;
		disp_draw.draw_rect.width =
			lcd_ext_param[lcd_num].rect_width;
		disp_draw.draw_rect.height =
			lcd_ext_param[lcd_num].rect_height;
		disp_draw.format = set_format;
		disp_draw.buffer_id = set_buff_id;
		disp_draw.buffer_offset = 0;
		disp_draw.rotate = lcd_ext_param[lcd_num].rotate;
		ret = screen_display_draw(&disp_draw);
		if (ret != SMAP_LIB_DISPLAY_OK) {
			r_mobile_fb_err_msg(ret, "screen_display_draw");
			up(&lcd_ext_param[lcd_num].sem_lcd);
			disp_delete.handle = screen_handle;
			screen_display_delete(&disp_delete);
			return -EIO;
		}
#endif

#if FB_SH_MOBILE_REFRESH
		if (lcd_ext_param[lcd_num].v4l2_state
		    == RT_DISPLAY_REFRESH_ON) {
			queue_delayed_work(
				sh_mobile_wq, &lcd_ext_param[lcd_num].ref_work,
				msecs_to_jiffies(REFRESH_TIME_MSEC));
			lcd_ext_param[lcd_num].delay_flag = 1;
			lcd_ext_param[lcd_num].mfis_err_flag = 0;
		}
#endif
		disp_delete.handle = screen_handle;
		screen_display_delete(&disp_delete);
	}

	ch->pan_offset = new_pan_offset;

	up(&lcd_ext_param[lcd_num].sem_lcd);

	return 0;
}

static int sh_mobile_ioctl(struct fb_info *info, unsigned int cmd,
		       unsigned long arg)
{
	int retval;
#if FB_SH_MOBILE_HDMI
	struct fb_hdmi_set_mode hdmi_set;
#endif
	int vsyncval;

	switch (cmd) {
	case FBIO_WAITFORVSYNC:
		retval = 0;
		break;

#if FB_SH_MOBILE_HDMI
	case SH_MOBILE_FB_HDMI_SET:
		if (arg == 0) {
			retval = -EINVAL;
			break;
		}
		if (copy_from_user(&hdmi_set, (void *)arg,
				   sizeof(struct fb_hdmi_set_mode))) {
			printk(KERN_ALERT "copy_from_user failed\n");
			retval = -EFAULT;
			break;
		}
		if (sh_mobile_fb_hdmi_set(&hdmi_set)) {
			retval = -EINVAL;
			break;
		} else {
			retval = 0;
			break;
		}
#endif
	case SH_MOBILE_FB_ENABLEVSYNC:
		if (arg == 0) {
			retval = -EINVAL;
			break;
		}
		if (get_user(vsyncval, (int __user *)arg)) {
			printk(KERN_ALERT "get_user failed\n");
			retval = -EFAULT;
			break;
		}
		if (sh_mobile_fb_sync_set(vsyncval)) {
			retval = -EINVAL;
			break;
		} else {
			retval = 0;
			break;
		}
	default:
		retval = -ENOIOCTLCMD;
		break;
	}
	return retval;
}

static int sh_mobile_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	unsigned long start;
	unsigned long off;
	u32 len;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;

	off = vma->vm_pgoff << PAGE_SHIFT;
	start = info->fix.smem_start;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.smem_len);

	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;

	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;

	/* Accessing memory will be done non-cached. */
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* To stop the swapper from even considering these pages */
	vma->vm_flags |= (VM_IO | VM_RESERVED);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}
static int sh_mobile_fb_check_var(struct fb_var_screeninfo *var,
				  struct fb_info *info)
{
	switch (var->bits_per_pixel) {
	case 16: /* RGB 565 */
		var->red.offset    = 11;
		var->red.length    = 5;
		var->green.offset  = 5;
		var->green.length  = 6;
		var->blue.offset   = 0;
		var->blue.length   = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 24: /* RGB 888 */
		var->red.offset    = 0;
		var->red.length    = 8;
		var->green.offset  = 8;
		var->green.length  = 8;
		var->blue.offset   = 16;
		var->blue.length   = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32: /* ARGB 8888*/
		var->red.offset    = 16;
		var->red.length    = 8;
		var->green.offset  = 8;
		var->green.length  = 8;
		var->blue.offset   = 0;
		var->blue.length   = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	default:
		return -EINVAL;

	}
	return 0;
}

static int sh_mobile_fb_set_par(struct fb_info *info)
{
	unsigned long ulLCM;
	int bpp = info->var.bits_per_pixel;

#ifdef CONFIG_FB_SH_MOBILE_RGB888
	/* calculate info->fix.smem_len by its maximum size */
	if (bpp == 24)
		bpp = 32;
#endif

	info->fix.line_length = RoundUpToMultiple(info->var.xres, 32)
		* (bpp / 8);

	/* 4kbyte align */
	ulLCM = LCM(info->fix.line_length, 0x1000);
	info->fix.smem_len = RoundUpToMultiple(
			info->fix.line_length*info->var.yres, ulLCM);

	info->var.reserved[0] = info->fix.smem_len;
	info->fix.smem_len *= 2;

	info->var.yres_virtual = info->fix.smem_len
		/ info->fix.line_length;

#ifdef CONFIG_FB_SH_MOBILE_RGB888
	/* calculate other values again using actual bpp */
	bpp = info->var.bits_per_pixel;
	if (bpp == 24) {
		int smem_len;

		info->fix.line_length = RoundUpToMultiple(info->var.xres, 32)
			* (bpp / 8);
		smem_len = info->fix.line_length * info->var.yres;

		info->var.reserved[0] = smem_len;
		smem_len *= 2;

		/* 4kbyte align */
		smem_len = RoundUpToMultiple(smem_len, 0x1000);

		info->var.yres_virtual = smem_len
			/ info->fix.line_length;
	}
#endif
	return 0;
}

static struct fb_ops sh_mobile_lcdc_ops = {
	.owner          = THIS_MODULE,
	.fb_setcolreg	= sh_mobile_lcdc_setcolreg,
	.fb_check_var	= sh_mobile_fb_check_var,
	.fb_set_par	= sh_mobile_fb_set_par,
	.fb_read        = fb_sys_read,
	.fb_write       = fb_sys_write,
	.fb_fillrect	= sys_fillrect,
	.fb_copyarea	= sys_copyarea,
	.fb_imageblit	= sys_imageblit,
	.fb_pan_display = sh_mobile_fb_pan_display,
	.fb_ioctl       = sh_mobile_ioctl,
	.fb_mmap	= sh_mobile_mmap,
};

static int sh_mobile_lcdc_set_bpp(struct fb_var_screeninfo *var, int bpp)
{
	switch (bpp) {
	case 16: /* RGB 565 */
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 24: /* RGB 888 */
		var->red.offset    = 0;
		var->red.length    = 8;
		var->green.offset  = 8;
		var->green.length  = 8;
		var->blue.offset   = 16;
		var->blue.length   = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32: /* ARGB 8888 */
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	default:
		return -EINVAL;
	}
	var->bits_per_pixel = bpp;
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;
	return 0;
}


static int sh_mobile_lcdc_suspend(struct device *dev)
{
	int lcd_num;
	screen_disp_delete disp_delete;
	void *suspend_handle;
#if FB_SH_MOBILE_REFRESH
	screen_disp_set_lcd_refresh disp_refresh;
#endif

	suspend_handle = screen_display_new();

	down(&sh_mobile_sem_hdmi);

	for (lcd_num = 0; lcd_num < CHAN_NUM; lcd_num++) {
		if (lcd_ext_param[lcd_num].aInfo != NULL) {
			down(&lcd_ext_param[lcd_num].sem_lcd);
#if FB_SH_MOBILE_REFRESH
			if (lcd_ext_param[lcd_num].v4l2_state
			    == RT_DISPLAY_REFRESH_ON) {
				if (lcd_ext_param[lcd_num].delay_flag == 1) {
					lcd_ext_param[lcd_num].mfis_err_flag
						= 1;
					cancel_delayed_work_sync(
						&lcd_ext_param[lcd_num].
						ref_work);
					lcd_ext_param[lcd_num].delay_flag = 0;
				}
				if (lcd_ext_param[lcd_num].refresh_on == 1) {
					mfis_drv_resume();
					disp_refresh.handle = suspend_handle;
					disp_refresh.output_mode
						= lcd_ext_param[lcd_num].
						o_mode;
					disp_refresh.refresh_mode
						= RT_DISPLAY_REFRESH_OFF;
					screen_display_set_lcd_refresh(
						&disp_refresh);
					lcd_ext_param[lcd_num].refresh_on = 0;
				}
			}
#endif

			if (lcd_ext_param[lcd_num].panel_func.panel_suspend) {
				lcd_ext_param[lcd_num].
					panel_func.panel_suspend();
			}

			up(&lcd_ext_param[lcd_num].sem_lcd);
		}
	}

#if FB_SH_MOBILE_HDMI
	if (lcd_ext_param[0].hdmi_flag == FB_HDMI_START) {
		if (lcd_ext_param[0].hdmi_func.hdmi_suspend)
			lcd_ext_param[0].hdmi_func.hdmi_suspend();
	}
#endif
	up(&sh_mobile_sem_hdmi);

	disp_delete.handle = suspend_handle;
	screen_display_delete(&disp_delete);

	return 0;
}

static int sh_mobile_lcdc_resume(struct device *dev)
{
	unsigned int lcd_num;

	down(&sh_mobile_sem_hdmi);

	for (lcd_num = 0; lcd_num < CHAN_NUM; lcd_num++) {
		if (lcd_ext_param[lcd_num].aInfo != NULL) {
			if (lcd_ext_param[lcd_num].panel_func.panel_resume) {
				lcd_ext_param[lcd_num].
					panel_func.panel_resume();
			}

		}
	}

#if FB_SH_MOBILE_HDMI
	if (lcd_ext_param[0].hdmi_flag == FB_HDMI_START) {
		if (lcd_ext_param[0].hdmi_func.hdmi_resume)
			lcd_ext_param[0].hdmi_func.hdmi_resume();
	}
#endif

	up(&sh_mobile_sem_hdmi);

	return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void sh_mobile_fb_early_suspend(struct early_suspend *h)
{
	struct sh_mobile_lcdc_priv *priv;

	priv = container_of(h, struct sh_mobile_lcdc_priv, early_suspend);

	sh_mobile_lcdc_suspend(priv->dev);

}

static void sh_mobile_fb_late_resume(struct early_suspend *h)
{
	struct sh_mobile_lcdc_priv *priv;

	priv = container_of(h, struct sh_mobile_lcdc_priv, early_suspend);

	sh_mobile_lcdc_resume(priv->dev);

}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static int sh_mobile_lcdc_remove(struct platform_device *pdev);

static unsigned long RoundUpToMultiple(unsigned long x, unsigned long y)
{
	unsigned long div = x / y;
	unsigned long rem = x % y;

	return (div + ((rem == 0) ? 0 : 1)) * y;
}

static unsigned long GCD(unsigned long x, unsigned long y)
{
	while (y != 0) {
		unsigned long r = x % y;
		x = y;
		y = r;
	}
	return x;
}

static unsigned long LCM(unsigned long x, unsigned long y)
{
	unsigned long gcd = GCD(x, y);

	return (gcd == 0) ? 0 : ((x / gcd) * y);
}

static int __devinit sh_mobile_lcdc_probe(struct platform_device *pdev)
{
	struct fb_info *info = NULL;
	struct sh_mobile_lcdc_priv *priv;
	struct sh_mobile_lcdc_info *pdata;
	struct sh_mobile_lcdc_chan_cfg *cfg;
	struct resource *res;
	int error = 0;
	int i, j;
	void __iomem *temp = NULL;
	struct fb_panel_info panel_info;

#ifndef CONFIG_FB_SH_MOBILE_DOUBLE_BUF
	void *buf = NULL;
#endif

	printk(KERN_ALERT "sh_mobile_lcdc_probe\n");

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "no platform data defined\n");
		error = -EINVAL;
		goto err0;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	i = platform_get_irq(pdev, 0);
	if (!res || i < 0) {
		dev_err(&pdev->dev, "cannot get platform resources\n");
		return -ENOENT;
	}
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&pdev->dev, "cannot allocate device data\n");
		error = -ENOMEM;
		goto err0;
	}

	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);
	pdata = pdev->dev.platform_data;

	error = request_irq(i, sh_mobile_lcdc_irq, 0,
			    dev_name(&pdev->dev), priv);
	if (error) {
		dev_err(&pdev->dev, "unable to request irq\n");
		goto err0;
	}

	priv->irq = i;

	/* irq base address */
	irqc_baseaddr = ioremap(res->start, resource_size(res));

#if FB_SH_MOBILE_REFRESH
	sh_mobile_wq = create_singlethread_workqueue("sh_mobile_lcd");
#endif

	j = 0;
	for (i = 0; i < ARRAY_SIZE(pdata->ch); i++) {
		priv->ch[j].lcdc = priv;
		memcpy(&priv->ch[j].cfg, &pdata->ch[i], sizeof(pdata->ch[i]));

		priv->ch[j].pan_offset = 0;
		switch (pdata->ch[i].chan) {
		case LCDC_CHAN_MAINLCD:
			lcd_ext_param[i].panel_func
				= r_mobile_panel_func(RT_DISPLAY_LCD1);
			if (!lcd_ext_param[i].panel_func.panel_info) {
				j++;
				break;
			}
			panel_info =
				lcd_ext_param[i].panel_func.panel_info();

			lcd_ext_param[i].o_mode = RT_DISPLAY_LCD1;
#if FB_SH_MOBILE_HDMI
			lcd_ext_param[i].draw_mode = RT_DISPLAY_LCDHDMI;
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI > 1
			/* disable mirror to HDMI */
			lcd_ext_param[i].draw_mode = RT_DISPLAY_LCD1;
#endif
#endif
#else
			lcd_ext_param[i].draw_mode = RT_DISPLAY_LCD1;
#endif

			lcd_ext_param[i].rect_x = SH_MLCD_RECTX;
			lcd_ext_param[i].rect_y = SH_MLCD_RECTY;
			lcd_ext_param[i].rect_width = panel_info.pixel_width;
			lcd_ext_param[i].rect_height = panel_info.pixel_height;
			lcd_ext_param[i].alpha = 0xFF;
			lcd_ext_param[i].key_clr = SH_MLCD_TRCOLOR;
			lcd_ext_param[i].v4l2_state = RT_DISPLAY_REFRESH_ON;
			lcd_ext_param[i].delay_flag = 0;
			lcd_ext_param[i].refresh_on = 0;
			lcd_ext_param[i].phy_addr = panel_info.buff_address;
			if (lcd_ext_param[i].rect_height
			    > lcd_ext_param[i].rect_width) {
				lcd_ext_param[i].rotate
					= RT_DISPLAY_ROTATE_270;
			} else {
				lcd_ext_param[i].rotate
					= RT_DISPLAY_ROTATE_0;
			}
#if FB_SH_MOBILE_REFRESH
			INIT_DELAYED_WORK(&lcd_ext_param[i].ref_work,
					  refresh_work);
#endif
			j++;
			break;
		case LCDC_CHAN_SUBLCD:
			lcd_ext_param[i].panel_func
				= r_mobile_panel_func(RT_DISPLAY_LCD2);
			if (!lcd_ext_param[i].panel_func.panel_info) {
				j++;
				break;
			}
			panel_info =
				lcd_ext_param[i].panel_func.panel_info();

			lcd_ext_param[i].o_mode = RT_DISPLAY_LCD2;
			lcd_ext_param[i].draw_mode = RT_DISPLAY_LCD2;
			lcd_ext_param[i].rect_x = SH_SLCD_RECTX;
			lcd_ext_param[i].rect_y = SH_SLCD_RECTY;
			lcd_ext_param[i].rect_width = panel_info.pixel_width;
			lcd_ext_param[i].rect_height = panel_info.pixel_height;
			lcd_ext_param[i].alpha = 0xFF;
			lcd_ext_param[i].key_clr = SH_SLCD_TRCOLOR;
			lcd_ext_param[i].v4l2_state = RT_DISPLAY_REFRESH_ON;
			lcd_ext_param[i].delay_flag = 0;
			lcd_ext_param[i].refresh_on = 0;
			/* SUBLCD undefined */
			lcd_ext_param[i].phy_addr = panel_info.buff_address;
			lcd_ext_param[i].rotate = RT_DISPLAY_ROTATE_0;
#if FB_SH_MOBILE_REFRESH
			INIT_DELAYED_WORK(&lcd_ext_param[i].ref_work,
					  refresh_work);
#endif
			j++;
			break;
		}
	}

	if (!j) {
		dev_err(&pdev->dev, "no channels defined\n");
		error = -EINVAL;
		goto err1;
	}

	for (i = 0; i < j; i++) {
		cfg = &priv->ch[i].cfg;

		priv->ch[i].info = framebuffer_alloc(0, &pdev->dev);
		if (!priv->ch[i].info) {
			dev_err(&pdev->dev, "unable to allocate fb_info\n");
			error = -ENOMEM;
			break;
		}

		if (!lcd_ext_param[i].panel_func.panel_info)
			continue;
		panel_info = lcd_ext_param[i].panel_func.panel_info();

		info = priv->ch[i].info;
		info->fbops = &sh_mobile_lcdc_ops;
		info->var.xres = info->var.xres_virtual
			= panel_info.pixel_width;
		info->var.yres = panel_info.pixel_height;
		/* Default Y virtual resolution is 2x panel size */
		info->var.width = panel_info.size_width;
		info->var.height = panel_info.size_height;
		info->var.pixclock = panel_info.pixclock;
		info->var.left_margin = panel_info.left_margin;
		info->var.right_margin = panel_info.right_margin;
		info->var.upper_margin = panel_info.upper_margin;
		info->var.lower_margin = panel_info.lower_margin;
		info->var.hsync_len = panel_info.hsync_len;
		info->var.vsync_len = panel_info.vsync_len;

		info->var.activate = FB_ACTIVATE_NOW;
		error = sh_mobile_lcdc_set_bpp(&info->var, cfg->bpp);
		if (error)
			break;
		info->fix = sh_mobile_lcdc_fix;
		sh_mobile_fb_set_par(info);
		lcd_ext_param[i].mem_size = info->fix.smem_len;

#ifdef CONFIG_FB_SH_MOBILE_DOUBLE_BUF
/* onscreen buffer 2 */
		temp = ioremap(lcd_ext_param[i].phy_addr,
			       info->fix.smem_len);
		if (NULL == temp) {
			error = -ENOMEM;
			break;
		} else {
			lcd_ext_param[i].vir_addr = (unsigned int)temp;
		}
#else
		buf = dma_alloc_coherent(&pdev->dev, info->fix.smem_len,
					 &priv->ch[i].dma_handle, GFP_KERNEL);
		if (!buf) {
			dev_err(&pdev->dev, "unable to allocate buffer\n");
			error = -ENOMEM;
			break;
		}
		temp = ioremap(lcd_ext_param[i].phy_addr,
			       info->fix.smem_len);
		if (NULL == temp) {
			error = -ENOMEM;
			break;
		} else {
			lcd_ext_param[i].vir_addr = (unsigned int)temp;
		}
#endif
		info->pseudo_palette = &priv->ch[i].pseudo_palette;
		info->flags = FBINFO_FLAG_DEFAULT;

		error = fb_alloc_cmap(&info->cmap, PALETTE_NR, 0);
		if (error < 0) {
			dev_err(&pdev->dev, "unable to allocate cmap\n");
			break;
		}
		fb_set_cmap(&info->cmap, info);

#ifdef CONFIG_FB_SH_MOBILE_DOUBLE_BUF
/* onscreen buffer 2 */
		info->fix.smem_start = lcd_ext_param[i].phy_addr;
		info->screen_base = (char __iomem *)lcd_ext_param[i].vir_addr;
		memset(info->screen_base, 0, info->fix.smem_len);
#else
		memset(buf, 0, info->fix.smem_len);
		info->fix.smem_start = priv->ch[i].dma_handle;
		info->screen_base = buf;
#endif
		info->device = &pdev->dev;
		info->par = &priv->ch[i];

	}

	if (error)
		goto err1;

	for (i = 0; i < j; i++) {
		struct sh_mobile_lcdc_chan *ch = priv->ch + i;
		struct fb_panel_hw_info hw_info;

		info = ch->info;

		error = register_framebuffer(info);
		if (error < 0)
			goto err1;

		dev_info(info->dev,
			 "registered %s/%s as %dx%d %dbpp.\n",
			 pdev->name,
			 (ch->cfg.chan == LCDC_CHAN_MAINLCD) ?
			 "mainlcd" : "sublcd",
			 (int) info->var.xres,
			 (int) info->var.yres,
			 ch->cfg.bpp);

		if (lcd_ext_param[i].panel_func.panel_probe) {
			hw_info.gpio_reg = ch->cfg.panelreset_gpio;
			hw_info.dsi_irq  = ch->cfg.paneldsi_irq;
			lcd_ext_param[i].panel_func.
				panel_probe(info, hw_info);
		}

		lcd_ext_param[i].aInfo = NULL;
		lcd_ext_param[i].lcd_type = ch->cfg.chan;
		lcd_ext_param[i].draw_bpp = ch->cfg.bpp;
		sema_init(&lcd_ext_param[i].sem_lcd, 1);
	}

	sema_init(&sh_mobile_sem_hdmi, 1);

#if FB_SH_MOBILE_HDMI

	lcd_ext_param[0].hdmi_func = r_mobile_hdmi_func();
	lcd_ext_param[0].hdmi_flag = FB_HDMI_STOP;
#endif
	error = device_create_file(priv->ch[0].lcdc->dev, &dev_attr_vsync);

	if (error) {
		printk(KERN_ALERT "device_create_file error\n");
		goto err1;
	}

	init_waitqueue_head(&sh_lcd_vsync.vsync_wait);

	sh_lcd_vsync.vsync_thread = kthread_run(vsync_recv_thread,
						priv->ch[0].lcdc,
						"sh-fb-vsync");
	if (IS_ERR(sh_lcd_vsync.vsync_thread)) {
		error = PTR_ERR(sh_lcd_vsync.vsync_thread);
		printk(KERN_ALERT "kthread_run error\n");
		goto err1;
	}

	fb_debug = 0;


#ifdef CONFIG_HAS_EARLYSUSPEND
	priv->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	priv->early_suspend.suspend = sh_mobile_fb_early_suspend;
	priv->early_suspend.resume = sh_mobile_fb_late_resume;
	register_early_suspend(&priv->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */

#if defined(CONFIG_SEC_DEBUG)
	// Mark for GetLog
	sec_getlog_supply_fbinfo(info->screen_base, info->var.xres, 
			info->var.yres, info->var.bits_per_pixel,2);
#endif

	return 0;
err1:
	sh_mobile_lcdc_remove(pdev);
err0:
	return error;
}

static int sh_mobile_lcdc_remove(struct platform_device *pdev)
{
	struct sh_mobile_lcdc_priv *priv = platform_get_drvdata(pdev);
	struct fb_info *info;
	int i;


#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&priv->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */

#if FB_SH_MOBILE_REFRESH
	destroy_workqueue(sh_mobile_wq);
#endif

	for (i = 0; i < ARRAY_SIZE(priv->ch); i++)
		if (priv->ch[i].info->dev)
			unregister_framebuffer(priv->ch[i].info);

	for (i = 0; i < ARRAY_SIZE(priv->ch); i++) {
		info = priv->ch[i].info;

		if (!info || !info->device)
			continue;

		if (lcd_ext_param[i].panel_func.panel_remove) {
			lcd_ext_param[i].
				panel_func.panel_remove(info);
		}


#ifdef CONFIG_FB_SH_MOBILE_DOUBLE_BUF
		if (lcd_ext_param[i].vir_addr != 0)
			iounmap((void __iomem *)lcd_ext_param[i].vir_addr);
#else
		if (info->screen_base != NULL)
			dma_free_coherent(&pdev->dev, info->fix.smem_len,
					  info->screen_base,
					  priv->ch[i].dma_handle);

		if (lcd_ext_param[i].vir_addr != 0)
			iounmap((void __iomem *)lcd_ext_param[i].vir_addr);

#endif
		fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);
	}

	kthread_stop(sh_lcd_vsync.vsync_thread);

	if (priv->irq)
		free_irq(priv->irq, priv);

	kfree(priv);
	return 0;
}

static struct platform_driver sh_mobile_lcdc_driver = {
	.driver		= {
		.name		= "sh_mobile_lcdc_fb",
		.owner		= THIS_MODULE,
#if 0
		.pm		= &sh_mobile_lcdc_dev_pm_ops,
#endif
	},
	.probe		= sh_mobile_lcdc_probe,
	.remove		= sh_mobile_lcdc_remove,
};

static int __init sh_mobile_lcdc_init(void)
{
	return platform_driver_register(&sh_mobile_lcdc_driver);
}

static void __exit sh_mobile_lcdc_exit(void)
{
	platform_driver_unregister(&sh_mobile_lcdc_driver);
}

module_init(sh_mobile_lcdc_init);
module_exit(sh_mobile_lcdc_exit);

MODULE_DESCRIPTION("SuperH Mobile LCDC Framebuffer driver");
MODULE_AUTHOR("Renesas Electronics");
MODULE_LICENSE("GPL v2");
