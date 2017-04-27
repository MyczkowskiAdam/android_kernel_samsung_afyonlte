/*
 * Function        : Composer driver for R Mobile
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
 * Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */
#ifndef _SH_MOBILE_COMPOSER_H
#define _SH_MOBILE_COMPOSER_H
#if defined(CONFIG_MISC_R_MOBILE_COMPOSER) || \
	defined(CONFIG_MISC_R_MOBILE_COMPOSER_MODULE)

#include <rtapi/screen_graphics.h>

/* common kernel and user */

/*******************/
/* define constant */
/*******************/
#define CMP_DATA_NUM_GRAP_LAYER    (1+4)
#define CMP_DATA_CHANNEL           2

/******************************/
/* define structure for ioctl */
/******************************/
struct cmp_blend_data {
	int                num;
	int                index[CMP_DATA_NUM_GRAP_LAYER];
	screen_grap_layer  buffer[CMP_DATA_NUM_GRAP_LAYER];

	/* buffer index 0 is for output */
	/* buffer index 1 is for layer0 */
	/* buffer index 2 is for layer1 */
	/* buffer index 3 is for layer2 */
	/* buffer index 4 is for layer3 */

	unsigned int       bgcolor;
};

struct cmp_postdata {
	/* information graphic buffer */
	int num_graphic_buffer;
	unsigned int rtAddress[CMP_DATA_NUM_GRAP_LAYER*CMP_DATA_CHANNEL+1];
	int graphic_buffer_fd[CMP_DATA_NUM_GRAP_LAYER*CMP_DATA_CHANNEL+1];

	/* information used buffer */
	int num_buffer;
	int acqure_fd[CMP_DATA_NUM_GRAP_LAYER*CMP_DATA_CHANNEL];
	int lookup_table[CMP_DATA_NUM_GRAP_LAYER*CMP_DATA_CHANNEL];

	/* information use to compose */
	struct cmp_blend_data *data[CMP_DATA_CHANNEL];

	struct cmp_blend_data blenddata[CMP_DATA_CHANNEL];
};

struct cmp_getfence {
	int release_lcd_fd;
	int release_hdmi_fd;
};

struct cmp_hdmimem {
	int size;
#if 0 /* not supported. */
	int hdmi_width;
	int hdmi_height;
#endif
};

/************************/
/* define cmd for ioctl */
/************************/

#define IOC_SH_MOBILE_COMP_MAGIC 'S'

#define CMP_IOC_ISSUSPEND \
	_IO(IOC_SH_MOBILE_COMP_MAGIC, 0x1a)
#define CMP_IOC_WAITCOMP \
	_IO(IOC_SH_MOBILE_COMP_MAGIC, 0x1b)
#define CMP_IOCS_FBADDR \
	_IOW(IOC_SH_MOBILE_COMP_MAGIC, 0x1d, unsigned long [2])
#define CMP_IOCS_BUSYLOCK \
	_IOW(IOC_SH_MOBILE_COMP_MAGIC, 0x1e, unsigned long)
#define CMP_IOC_POST \
	_IOW(IOC_SH_MOBILE_COMP_MAGIC, 0x21, struct cmp_postdata)
#define CMP_IOCG_GETFENCE \
	_IOR(IOC_SH_MOBILE_COMP_MAGIC, 0x22, struct cmp_getfence)
#define CMP_IOCS_HDMIMEM \
	_IOW(IOC_SH_MOBILE_COMP_MAGIC, 0x23, struct cmp_hdmimem)



/*******************/
/* define constant */
/*******************/

#define CMP_OK	0
#define CMP_NG	-1

/********************/
/* define busylock  */
/********************/
#define CMP_BUSYLOCK_SET     1
#define CMP_BUSYLOCK_CLEAR   4

#ifdef __KERNEL__
/* please not define __KERNEL__ when build application. */

#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <media/sh_mobile_appmem.h>

/********************/
/* define constant  */
#define CMP_HDMISET_STOP              0
#define CMP_HDMISET_STOP_CANCEL       1

#define COMPOSER_NUM_INPUT_GRAP_LAYER 4

struct appshare_list;
struct composer_fh;

/* for internal use for kernel */

struct composer_blendwait {
	wait_queue_head_t        wait_notify;
	int                      status;
};

struct localworkqueue {
	struct   list_head  top;
	spinlock_t          lock;
	wait_queue_head_t   wait;
	wait_queue_head_t   finish;
	struct task_struct  *task;
	int                 priority;
};

struct localwork;

struct localwork {
	struct list_head  link;
	void              (*func)(struct localwork *work);
	int               status;
};

/* file handle */
struct composer_fh {
	struct semaphore         fh_sem;
	void                     *ioctl_args;

	struct cmp_getfence      sync_fence;
};

#ifdef CONFIG_MACH_KOTA2
/* support HDMI for KOTA2 */
#define SH_MOBILE_COMPOSER_SUPPORT_HDMI    1
#else
/* support HDMI for other */
#define SH_MOBILE_COMPOSER_SUPPORT_HDMI    3
#endif

#define SH_MOBILE_COMPOSER_USE_SW_SYNC_API   0


struct cmp_request_queuedata {
	screen_grap_image_blend blend;
	screen_grap_layer       layer[4];
	int                     num_layers;
	int                     extlayer_index;
	struct {
		struct {
			unsigned short  format;
			unsigned short  yuv_format;
			unsigned short  yuv_range;
			unsigned short  reserved_for_alignment;
		} image;
		int                 rotate;
	} extlayer;
	int                     use_gpu_composition;
	int                     output_image_offset;
};

/* compose parameter for LCD */
struct cmp_data_compose_lcd {
	int                     valid;
	int                     display;
	screen_grap_image_blend blend;
	screen_grap_layer       layer[COMPOSER_NUM_INPUT_GRAP_LAYER];
	int                     need_blend;
};

/* compose parameter for HDMI */
struct cmp_data_compose_hdmi {
	int                      valid;
	int                      display;
	screen_grap_image_output output;
	screen_grap_image_blend  blend;
	screen_grap_layer        layer[COMPOSER_NUM_INPUT_GRAP_LAYER];
	int                      need_blend;
};

/* request queue handle */
struct composer_rh {
	struct localwork             rh_wqtask;
	struct composer_blendwait    rh_wqwait;
	struct localwork             rh_wqtask_disp;
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	struct localwork             rh_wqtask_hdmi_blend;
	struct localwork             rh_wqtask_hdmi;
	struct localwork             rh_wqtask_hdmi_comp;
	struct composer_blendwait    rh_wqwait_hdmi;
#endif

	struct localwork             rh_wqtask_schedule;

	struct cmp_data_compose_lcd  lcd_data;
	struct cmp_data_compose_hdmi hdmi_data;

	int                          num_buffer;
	unsigned long                buffer_usage[\
		CMP_DATA_NUM_GRAP_LAYER*CMP_DATA_CHANNEL];
	unsigned long                buffer_address[\
		CMP_DATA_NUM_GRAP_LAYER*CMP_DATA_CHANNEL];
#ifdef CONFIG_SYNC
	struct sync_fence            *buffer_sync[\
		CMP_DATA_NUM_GRAP_LAYER*CMP_DATA_CHANNEL];
#endif
	struct file                  *buffer_handle[\
		CMP_DATA_NUM_GRAP_LAYER*CMP_DATA_CHANNEL];

/* CONFIG_SYNC */
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	union {
		int                          fence_signal_flag;
		char                         fence_signal_flag_c[4];
	};
#endif
/* SH_MOBILE_COMPOSER_USE_SW_SYNC_API */

	int                          active;
	void                        (*user_callback)\
		(void *user_data, int result);
	void                         *user_data;
	int                          refcount;

	int                          refmask_disp;

	struct   list_head           lcd_list;
	struct   list_head           hdmi_list;
	struct   list_head           hdmi_wait_list;

	void                         *timerecord;
};

#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
extern int sh_mobile_composer_queue(
	void *data,
	int   data_size,
	void  (*callback)(void *user_data, int result),
	void   *user_data);

extern unsigned char *sh_mobile_composer_phy_change_rtaddr(
	unsigned long p_adr);
extern int sh_mobile_composer_register_gpu_buffer(
	unsigned long address, unsigned long size);
#endif
/* end CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE */
static inline __deprecated __maybe_unused void sh_mobile_composer_blendoverlay(
	unsigned long arg)
{
}
#ifdef SH_MOBILE_COMPOSER_SUPPORT_HDMI
extern int sh_mobile_composer_hdmiset(int mode);
#endif
/* end SH_MOBILE_COMPOSER_SUPPORT_HDMI */

#endif
/* end __KERNEL__ */
#endif
/* end  defined(CONFIG_MISC_R_MOBILE_COMPOSER) || \
	defined(CONFIG_MISC_R_MOBILE_COMPOSER_MODULE) */
#endif
/* end _SH_MOBILE_COMPOSER_H */

