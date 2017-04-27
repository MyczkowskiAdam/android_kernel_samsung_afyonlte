/*
 * Function        : Composer driver for SH Mobile
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

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/rwlock.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/file.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */

#include <rtapi/screen_graphics.h>
#include <rtapi/system_memory.h>
#include <rtapi/screen_display.h>

#include <linux/sh_mobile_composer.h>
#include "../composer/sh_mobile_module.h"
#include "../composer/sh_mobile_remote.h"
#include "../composer/sh_mobile_debug.h"
#include "../composer/sh_mobile_work.h"
#include "../composer/sh_mobile_lcd_compose.h"
#include "../composer/sh_mobile_hdmi_compose.h"
#include "../composer/sh_mobile_swfence.h"


static int debug;    /* default debug level */

#define FEATURE_FRAMEACCESS       3    /* bit0: LCD  bit1: HDMI */
#define FEATURE_SKIP_LCD          0    /* experimental */
#define FEATURE_SKIP_HDMI         0    /* experimental */
#define FEATURE_LCD_WORKQUEUE     1
#define FEATURE_HDMI_WORKQUEUE    1
#define FEATURE_HDMI_WAIT_TIMING  2    /* 0:before 1:after 2:no wait */
#define FEATURE_FENCE_QUEUEWAIT   1

/******************************************************/
/* define prototype                                   */
/******************************************************/
static long core_ioctl(struct file *filep, \
		unsigned int cmd, unsigned long arg);
static int core_open(struct inode *inode, struct file *filep);
static int core_release(struct inode *inode, struct file *filep);

static int waitcomp_composer(int mode);

/* module interface */
static int composer_queue(
	void *data,
	int   data_size,
	void  (*callback)(void *user_data, int result),
	void   *user_data,
	struct cmp_getfence *sync_fence);

/* buffer set/ unset */
static int composer_set_address(
	unsigned int id, unsigned long addr, unsigned long size);

/* hung-up notify */
static void sh_mobile_composer_notifyfatalerror(void);

/* callback */
static void process_composer_queue_callback(struct composer_rh *rh);

/* complete */
static void complete_work_blend(struct composer_rh *rh);
static void complete_work_dispdraw(struct composer_rh *rh);
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
static void complete_work_blend_hdmi(struct composer_rh *rh);
static void complete_work_dispdraw_hdmi(struct composer_rh *rh);
#endif

/* task processed in workqueue */
static void work_dispdraw(struct localwork *work);
static int work_create_handle(unsigned long *args __always_unused);
static int work_delete_handle(unsigned long *args __always_unused);
static int work_re_initialize_handle(unsigned long *args __always_unused);
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
static void work_dispdraw_hdmi(struct localwork *work);
static int work_alloc_memory_hdmi(unsigned long *args) __maybe_unused;
static int work_free_memory_hdmi(unsigned long *args);
static int work_create_handle_hdmi(unsigned long *args __always_unused) \
	__maybe_unused;
static int work_delete_handle_hdmi(unsigned long *args __always_unused);
#endif

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
/* create/delete sw_sync handle */
static void swsync_create_handle(void);
static void swsync_delete_handle(void);
#endif

#if FEATURE_FRAMEACCESS
/* frame buffer access control. */
static void increment_usable_framebuffer(int type);
static int  decrement_usable_framebuffer(int type);
static int  wait_ready_usable_framebuffer(int type);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void pm_early_suspend(struct early_suspend *h);
static void pm_late_resume(struct early_suspend *h);
#endif

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
/* output control. */
static int composer_enable_hdmioutput(int enable, int mode);
#endif

/******************************************************/
/* define local define                                */
/******************************************************/
/* define for error threshold */
#define RTAPI_FATAL_ERROR_THRESHOLD  (-256)

#if INTERNAL_DEBUG
#define INTERNAL_LOG_MSG_SIZE 16384
#endif

#define MAX_OPEN      32
#define MAX_KERNELREQ 8

#define DEV_NAME      "composer"

/* define for core_ioctl function. */
#define CORE_IOCTL_MAX_ARG_LENGTH     300

/* define for time-out */
#if _EXTEND_TIMEOUT
#define IOC_WAITCOMP_WAITTIME             (20*1000)  /* 20 sec */
#define WORK_DISPDRAW_SWFENCE_WAITTIME        100    /*   msec */
#define GETFENCE_WAITTIME                 (10*1000)  /* 20 sec */
#else
#define IOC_WAITCOMP_WAITTIME              300  /* msec */
#define WORK_DISPDRAW_SWFENCE_WAITTIME     200  /* msec */
#define GETFENCE_WAITTIME                  100  /* msec */
#endif

/* buffer use flag */
#define BUFFER_USAGE_BLEND   0x01   /* 0b001 */
#define BUFFER_USAGE_OUTPUT  0x02   /* 0b010 */

/* index of array of fence_signal_flag_c  */
#define FENCE_SIGNAL_BLEND   0
#define FENCE_SIGNAL_OUTPUT  1

#define REFFLAG_DISPLAY_LCD  0x01
#define REFFLAG_DISPLAY_HDMI 0x02

/* display flag */
#define DISPLAY_FLAG_BLANK        (false)
#define DISPLAY_FLAG_UNBLANK      (true)
#define DISPLAY_FLAG_UNBLANK_SKIP (2)

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
#define HDMI_OUTPUT_MASK_HDMISET  1
#define HDMI_OUTPUT_MASK_HDMIMEM  2
#define HDMI_OUTPUT_MASK_HDMIMEM2 4
#endif /* SH_MOBILE_COMPOSER_SUPPORT_HDMI */

#if _TIM_DBG
/* define slot ID for timerecord */
#define TIMID_BUSYLOCK       0
#define TIMID_QUEUE          1
#define TIMID_SWSYNC1        2
#define TIMID_SWSYNC2        3
#define TIMID_BLEND_HDMI_S   4
#define TIMID_BLEND_HDMI_E   5
#define TIMID_DISP_HDMI_S    6
#define TIMID_DISP_HDMI_O    7
#define TIMID_DISP_HDMI_E    8
#define TIMID_BLEND_S        9
#define TIMID_BLEND_E        10
#define TIMID_DISP_S         11
#define TIMID_DISP_E         12
#define TIMID_CALLBACK       13
#endif

/* variable of workqueue used to process LCD */
#if FEATURE_LCD_WORKQUEUE
#define VAR_WORKQUEUE_LCD    workqueue_lcd
#else
#define VAR_WORKQUEUE_LCD    workqueue
#endif

/* variable of workqueue used to process HDMI */
#if FEATURE_HDMI_WORKQUEUE
#define VAR_WORKQUEUE_HDMI   workqueue_hdmi
#else
#define VAR_WORKQUEUE_HDMI   workqueue
#endif

#if FEATURE_FRAMEACCESS
/* define for busylock */
#if _EXTEND_TIMEOUT
#define BUSYLOCK_WAITTIME           (10*1000) /* 10 sec */
#else
#define BUSYLOCK_WAITTIME            300 /* msec */
#endif

#if FEATURE_FRAMEACCESS & 1
#define FRAMEACESS_FB_FOR_LCD          1
#define NUM_OF_ANDROID_USABLE_FB       2
#endif

#if FEATURE_FRAMEACCESS & 2
#define FRAMEACESS_FB_FOR_HDMI         2
#define NUM_OF_ANDROID_USABLE_HDMI_FB  2
#endif

#endif

#define NUM_OF_FRAMEBUFFER_MAXIMUM        1
#define NUM_OF_HDMI_FRAMEBUFFER_MAXIMUM   2
#define BLEND_REQ_LCD                     0x01
#define BLEND_REQ_HDMI                    0x02

/* define for waitcomp_composer */
#define WAITCOMP_COMPOSER_QUEUE  1
#define WAITCOMP_COMPOSER_BLEND  2
#define WAITCOMP_COMPOSER_DISP   4

#if FEATURE_SKIP_HDMI || FEATURE_SKIP_LCD
#define SKIP_MODE_LCD   0
#define SKIP_MODE_HDMI  1

#define NUM_OF_CONTIG_SKIP_LCD   1
#define NUM_OF_CONTIG_SKIP_HDMI  2
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#define NUM_OF_COMPOSER_PROHIBITE_AT_START    0
#define NUM_OF_COMPOSER_PROHIBITE_AT_RESUME   0
#endif

#if !defined(CONFIG_FB_SH_MOBILE_DOUBLE_BUF)
#error	configuration of FB not supported.
#endif

/******************************************************/
/* define local variables                             */
/******************************************************/
static const struct file_operations composer_fops = {
	.owner		= THIS_MODULE,
	.write		= NULL,
	.unlocked_ioctl	= core_ioctl,
	.open		= core_open,
	.release	= core_release,
};

static struct miscdevice composer_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "composer",
	.fops = &composer_fops,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static  struct early_suspend    early_suspend = {
	.level		= EARLY_SUSPEND_LEVEL_STOP_DRAWING,
	.suspend	= pm_early_suspend,
	.resume		= pm_late_resume,
};
static int composer_prohibited_count = NUM_OF_COMPOSER_PROHIBITE_AT_START;
#endif /* CONFIG_HAS_EARLYSUSPEND */

static DEFINE_SPINLOCK(irqlock);
static DEFINE_SEMAPHORE(sem);
static int                    num_open;

/* workqueue to execute RT-API Graphics API, */
static struct localworkqueue  *workqueue;
static int                    rtapi_hungup;

/* workqueue to schedule request. */
static struct localworkqueue  *workqueue_schedule;

/* task for blending */
static void                   *graphic_handle;
static int                    set_blend_size_flag;

/* blend timing control */
static DEFINE_SPINLOCK(irqlock_list);
static struct   list_head     top_lcd_list;
static struct   list_head     top_hdmi_list;
static DEFINE_SEMAPHORE(sem_framebuf_useable);

/* task for HDMI support */
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
#if FEATURE_HDMI_WORKQUEUE
static struct localworkqueue  *workqueue_hdmi;
#endif
static int                    hdmi_output_enable;
static int                    hdmi_output_enable_mask;
static void                   *graphic_handle_hdmi;
static DEFINE_SEMAPHORE(sem_hdmi_framebuf_useable);

static struct cmp_information_hdmi info_hdmi;
static DEFINE_SEMAPHORE(sem_hdmimemory);
#endif /* SH_MOBILE_COMPOSER_SUPPORT_HDMI */

/* task for process queue data */
#if FEATURE_LCD_WORKQUEUE
static struct localworkqueue  *workqueue_lcd;
#endif
static struct cmp_information_fb info_fb;

/* support skip blending */
#if FEATURE_SKIP_HDMI || FEATURE_SKIP_LCD
static int                    skip_frame_count[2][2];
#endif

/* queue data */
static DECLARE_WAIT_QUEUE_HEAD(kernel_waitqueue_comp);
static DEFINE_SEMAPHORE(kernel_queue_sem);
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
static int                    queue_data_complete;
static DECLARE_WAIT_QUEUE_HEAD(kernel_waitqueue_queued);
#endif
static struct composer_rh     kernel_request[MAX_KERNELREQ];

#if FEATURE_FRAMEACCESS
/* confirm framebuffer is available. */
static DEFINE_SPINLOCK(irqlock_framebuffer);
static int                      available_num_framebuffer[2];
static DECLARE_WAIT_QUEUE_HEAD(wait_framebuffer_available);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static int                    in_early_suspend;
#endif /* CONFIG_HAS_EARLYSUSPEND */

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
/* timeline for sw_sync */
static DEFINE_SEMAPHORE(sem_get_syncfd);
struct sw_fence_handle         *swsync_handle;
struct sw_fence_handle         *swsync_handle_hdmi;
#endif

#if INTERNAL_DEBUG
static char        *internal_log_msg;
static int         internal_log_msgsize;
#endif

#if _TIM_DBG
static ktime_t     ktime_busylock;
#endif

#if _ATR_DBG
static int        atrace_hdmi_display_request;
static int        atrace_hdmi_display_complete;
static int        atrace_hdmi_display_used;
#endif

/******************************************************/
/* include other modules                              */
/******************************************************/
#include "../composer/debug.c"
#include "../composer/swfence.c"
#include "../composer/work.c"
#include "../composer/lcd_compose.c"
#include "../composer/hdmi_compose.c"
#include "../composer/remote.c"


/******************************************************/
/* local functions                                    */
/******************************************************/
static void initialize_blendwait_obj(struct composer_blendwait *obj)
{
	init_waitqueue_head(&obj->wait_notify);
}

static struct composer_fh *allocate_device(void)
{
	struct composer_fh *fh;

	DBGENTER("\n");

	fh = kmalloc(sizeof(*fh), GFP_KERNEL);
	if (NULL == fh)
		goto err_exit;

	/* initialize handle */
	memset(fh, 0, sizeof(*fh));

	/* semaphore */
	sema_init(&fh->fh_sem, 1);

	fh->ioctl_args = kmalloc(CORE_IOCTL_MAX_ARG_LENGTH * 4, GFP_KERNEL);
	if (NULL == fh->ioctl_args) {
		kfree(fh);
		fh = NULL;
		goto err_exit;
	}

	/* sync fence */
	fh->sync_fence.release_lcd_fd = -1;
	fh->sync_fence.release_hdmi_fd = -1;

err_exit:
	DBGLEAVE("%p\n", fh);

	return fh;
}

static void  free_device(struct composer_fh *fh)
{
	DBGENTER("fh:%p\n", fh);

	if (fh->sync_fence.release_lcd_fd != -1 ||
		fh->sync_fence.release_hdmi_fd != -1) {
		/* error report. */
		printk_err("fd used for sync is not closed.\n");
	}

	kfree(fh->ioctl_args);
	kfree(fh);

	DBGLEAVE("\n");
}

static int  ioc_issuspend(struct composer_fh *fh)
{
	int rc = -EBUSY;
	DBGENTER("fh:%p\n", fh);

	if (graphic_handle == NULL) {
		printk_dbg2(3, "handle is NULL\n");
		/* report log */
	} else {
		printk_dbg2(3, "handle is not NULL\n");
		/* report log */
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (in_early_suspend) {
		printk_dbg2(3, "now suspend state\n");
		rc = 0;
	}
	if (composer_prohibited_count) {
		printk_dbg2(3, "composition prohibited count(%d)\n", \
			composer_prohibited_count);
		rc = 0;
	}
#endif

	DBGLEAVE("%d\n", rc);
	return rc;
}

static int  ioc_waitcomp(struct composer_fh *fh)
{
	int rc = 0;
#if _LOG_DBG > 1
	unsigned long jiffies_s = jiffies;
	int prev_state = 0, new_state = 0;
	int i;
#endif /* _LOG_DBG > 1 */
	DBGENTER("fh:%p\n", fh);

#if _LOG_DBG > 1
	for (i = 0; i < MAX_KERNELREQ; i++) {
		if (kernel_request[i].active)
			prev_state |= (1 << i);
	}
#endif

	rc = waitcomp_composer(
		WAITCOMP_COMPOSER_QUEUE | WAITCOMP_COMPOSER_BLEND);

#if _LOG_DBG > 1
	for (i = 0; i < MAX_KERNELREQ; i++) {
		if (kernel_request[i].active)
			new_state |= (1 << i);
	}
	printk_dbg2(3, "queue state: 0x%x to 0x%x\n", prev_state, new_state);
#endif
	if (rc != CMP_OK) {
		printk_err("fail to wait task complete.\n");
		rc = -EBUSY;
	} else {
		/* wait success before timeout */
		rc = 0;
	}
#if _LOG_DBG > 1
	printk_dbg2(3, "actual waiting %d msec",
		jiffies_to_msecs(jiffies - jiffies_s));
#endif /* _LOG_DBG > 1 */

	DBGLEAVE("%d\n", rc);
	return rc;
}
static int  ioc_setfbaddr(struct composer_fh *fh, unsigned long *addr)
{
	int rc = 0;
	unsigned long fb_addr = *addr;
	unsigned long fb_size = *(addr+1);
	DBGENTER("fh:%p addr:%p\n", fh, addr);
	printk_dbg2(3, "arg addr:0x%lx size:0x%lx\n", fb_addr, fb_size);

	rc = composer_set_address(FB_SCREEN_BUFFERID0,
		fb_addr, fb_size);

	if (rc == 0) {
		printk_dbg2(3, "down\n");
		down(&sem);

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI > 1
		rc = composer_enable_hdmioutput(false,
						HDMI_OUTPUT_MASK_HDMIMEM2);

		if (rc) {
			/* nothing to do */
			printk_err1("failed to stop output\n");
			rc = -EINVAL;
		} else {
#endif
			/* update graphic handle. */
			rc = indirect_call(workqueue,
				work_re_initialize_handle, 0, NULL);

			if (rc || !graphic_handle) {
				/* report error */
				printk_err("lost graphic handle, need restart.\n");
				rc = -EINVAL;
			}
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI > 1
		}

		composer_enable_hdmioutput(true, HDMI_OUTPUT_MASK_HDMIMEM2);
#endif

		up(&sem);
	}

	DBGLEAVE("%d\n", rc);
	return rc;
}
static int  ioc_busylock(struct composer_fh *fh, unsigned long *data)
{
	int rc = -EINVAL;
	DBGENTER("fh:%p data:%p\n", fh, data);
	printk_dbg2(3, "arg data:0x%lx\n", *data);

	if (*data & CMP_BUSYLOCK_SET) {
#if _TIM_DBG
		ktime_busylock = ktime_get();
#endif
		/* nothing to do */
		rc = 0;
	}
	if (*data & CMP_BUSYLOCK_CLEAR) {
		/* nothing to do */
		rc = 0;
	}
	DBGLEAVE("%d\n", rc);
	return rc;
}
static void ioc_post_callback(void *user_data, int result)
{
	printk_dbg2(3, "callback user_data:%p result:%d\n", user_data, result);
}

static int  ioc_post(struct composer_fh *fh, struct cmp_postdata *data)
{
	int rc;

	DBGENTER("fh:%p data:%p\n", fh, data);
	printk_dbg2(3, "arg num_buffer:%d data[0]:%p data[1]:%p\n",
		data->num_buffer, data->data[0], data->data[1]);

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	if (swsync_handle == NULL && swsync_handle_hdmi == NULL) {
		printk_dbg2(3, "down\n");
		down(&sem_get_syncfd);

		/* create sw_sync handle if not created. */
		swsync_create_handle();

		up(&sem_get_syncfd);
	}
#endif

	if (data->num_buffer >= CMP_DATA_NUM_GRAP_LAYER*CMP_DATA_CHANNEL) {
		/* num of buffer too large */
		rc = -EINVAL;
		goto err;
	}

	rc = composer_queue(data, sizeof(*data),
		ioc_post_callback, NULL, &fh->sync_fence);

	if (rc == CMP_OK) {
		/* no error */
		rc = 0;
	} else {
		/* set error code */
		rc = -EINVAL;
	}
err:
	DBGLEAVE("%d\n", rc);
	return rc;
}

static int iocg_getfence(struct composer_fh *fh, struct cmp_getfence *get_fence)
{
	int    prev_queueflag = true;
#if FEATURE_FRAMEACCESS
	int    wait_mode;
#endif
	DBGENTER("fh:%p get_fence:%p\n", fh, get_fence);

	if (fh->sync_fence.release_lcd_fd == -1 &&
		fh->sync_fence.release_hdmi_fd == -1) {
		/* syncfd is not ready. */

		printk_dbg2(3, "sync fence is not created.\n");
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
#if FEATURE_FENCE_QUEUEWAIT
		if (!queue_data_complete) {
			int rc;
			rc = wait_event_timeout(kernel_waitqueue_queued,
				queue_data_complete,
				msecs_to_jiffies(GETFENCE_WAITTIME));
			if (rc <= 0)
				printk_err("waiting queue time-out or error.\n");
		}
#endif

		prev_queueflag = queue_data_complete;
#if FEATURE_FENCE_QUEUEWAIT
		ioc_waitcomp(fh);
#endif

#endif
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
		if (prev_queueflag &&
			swsync_handle && swsync_handle_hdmi) {
			get_fence->release_lcd_fd =
				fence_get_syncfd(swsync_handle,
					SYNC_FD_TYPE_BLIT);
			get_fence->release_hdmi_fd =
				fence_get_syncfd(swsync_handle_hdmi,
					SYNC_FD_TYPE_BLIT);
		} else {
			/* wait complete blending. */
			printk_dbg1(3, "sw_sync is not available.\n");
#endif
			get_fence->release_lcd_fd = -1;
			get_fence->release_hdmi_fd = -1;
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
		}
#endif
	} else {
		/* set sync fd */
		*get_fence = fh->sync_fence;

		/* reset sync fd */
		fh->sync_fence.release_lcd_fd = -1;
		fh->sync_fence.release_hdmi_fd = -1;
	}
#if _LOG_DBG >= 2
	printk_dbg2(3, "release_fd:%d,%d\n",
		get_fence->release_lcd_fd, get_fence->release_hdmi_fd);
#endif

#if FEATURE_FRAMEACCESS
	wait_mode = 0;
#if (FEATURE_FRAMEACCESS & 1) && (NUM_OF_ANDROID_USABLE_FB > 1)
	wait_mode |= FRAMEACESS_FB_FOR_LCD;
#endif
#if (FEATURE_FRAMEACCESS & 2) && (NUM_OF_ANDROID_USABLE_HDMI_FB > 1)
	wait_mode |= FRAMEACESS_FB_FOR_HDMI;
#endif
	/* wait to guarantee signal of previous queue. */
	if (wait_mode) {
		if (wait_ready_usable_framebuffer(wait_mode) == 0) {
			/* error report */
			printk_err("time out previous queue complete.\n");
		}
	}
#endif

	DBGLEAVE("\n");
	return 0;
}

static int iocs_hdmimem(struct composer_fh *fh, struct cmp_hdmimem *mem)
{
	int rc = -EINVAL;
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI > 1
	unsigned long args[1];
#endif
	DBGENTER("fh:%p mem:%p\n", fh, mem);
	printk_dbg2(3, "size:%d\n", mem->size);

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI > 1
	printk_dbg2(3, "down\n");
	down(&sem);

	rc = composer_enable_hdmioutput(false, HDMI_OUTPUT_MASK_HDMIMEM);

	if (rc) {
		/* nothing to do */
		printk_err1("failed to stop output\n");
	} else {
		/* success to stop output.       */
		/* update registerd information. */
		info_hdmi.max_width  = 1920;
		info_hdmi.max_height = 1088;

		/* update graphic handle. */
		rc = indirect_call(workqueue,
			work_re_initialize_handle, 0, NULL);

		if (rc || !graphic_handle) {
			/* report error */
			printk_err("lost graphic handle, need restart.\n");
			rc = CMP_NG;
		}
	}

	composer_enable_hdmioutput(true, HDMI_OUTPUT_MASK_HDMIMEM);

	up(&sem);

	args[0] = mem->size;

	if (rc) {
		/* previous operation failed. */
		rc = -EINVAL;
	} else if (mem->size == 0) {
		/* free reserved memory. */
		printk_dbg2(3, "down\n");
		down(&sem_hdmimemory);

		rc = indirect_call(VAR_WORKQUEUE_HDMI,
			work_free_memory_hdmi, 0, &args[0]);

		if (rc == 0 && !info_hdmi.hdmi_map_handle) {
			/* no error */
			rc = 0;
		} else {
			/* report error */
			printk_err1("failed to free HDMI memory\n");
			rc = -EINVAL;
		}

		up(&sem_hdmimemory);
	} else {
		/* reserve memory. */
		printk_dbg2(3, "down\n");
		down(&sem_hdmimemory);

		rc = indirect_call(VAR_WORKQUEUE_HDMI,
			work_alloc_memory_hdmi, 1, &args[0]);

		if (rc == 0 && info_hdmi.hdmi_map_handle) {
			/* no error */
			rc = 0;
		} else {
			/* report error */
			printk_err1("failed to alloc HDMI memory\n");
			rc = -EINVAL;
		}

		up(&sem_hdmimemory);
	}
#endif

	DBGLEAVE("\n");
	return rc;
}

static int  waitcomp_composer_complete_display(void)
{
	int i;
	int rc = false;

	for (i = 0; i < MAX_KERNELREQ; i++) {
		struct composer_rh *rh = &kernel_request[i];
		if (rh->active || rh->refmask_disp) {
			rc = true;
			break;
		}
	}
	return (rc == false);
}

static int  waitcomp_composer_complete_buffer(void)
{
	int i;
	int rc = false;

	for (i = 0; i < MAX_KERNELREQ; i++) {
		struct composer_rh *rh = &kernel_request[i];
		if (rh->active) {
			rc = true;
			break;
		}
	}
	return (rc == false);
}

static int waitcomp_composer(int mode)
{
	int rc;

	DBGENTER("mode:%d\n", mode);

	/* wait queue */
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
	if (mode & WAITCOMP_COMPOSER_QUEUE) {
		rc = wait_event_timeout(kernel_waitqueue_queued,
			queue_data_complete,
			msecs_to_jiffies(100));
		if (rc == 0) {
			printk_err("fail to wait queue.\n");
			/* ignore and continue; */
		} else {
			/* clear queue flag */
			queue_data_complete = false;
		}
	}
#endif /* CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE */

	rc = CMP_OK;

	if (mode & WAITCOMP_COMPOSER_DISP) {
		printk_dbg2(3, "wait complete display.\n");
		rc = wait_event_timeout(kernel_waitqueue_comp,
			waitcomp_composer_complete_display(),
			msecs_to_jiffies(IOC_WAITCOMP_WAITTIME));
		if (rc == 0) {
			/* timeout */
			rc = CMP_NG;
#if INTERNAL_DEBUG
			sh_mobile_composer_dump_information(1);
#endif
		} else {
			/* wait success before timeout */
			rc = CMP_OK;
#if INTERNAL_DEBUG
			sh_mobile_composer_dump_information(0);
#endif
		}
	} else if (mode & WAITCOMP_COMPOSER_BLEND) {
		printk_dbg2(3, "wait complete blending.\n");
		rc = wait_event_timeout(kernel_waitqueue_comp,
			waitcomp_composer_complete_buffer(),
			msecs_to_jiffies(IOC_WAITCOMP_WAITTIME));
		if (rc == 0) {
			/* timeout */
			rc = CMP_NG;
#if INTERNAL_DEBUG
			sh_mobile_composer_dump_information(1);
#endif
		} else {
			/* wait success before timeout */
			rc = CMP_OK;
#if INTERNAL_DEBUG
			sh_mobile_composer_dump_information(0);
#endif
		}
	} else {
		printk_dbg2(3, "nothing to do.\n");
		rc = CMP_OK;
	}

	DBGLEAVE("%d\n", rc);
	return rc;
}

#if FEATURE_FRAMEACCESS
static void increment_usable_framebuffer(int type)
{
	printk_dbg2(3, "spinlock\n");
	spin_lock(&irqlock_framebuffer);

#if FEATURE_FRAMEACCESS & 1
	if (type & FRAMEACESS_FB_FOR_LCD) {
		if (available_num_framebuffer[0] <
			NUM_OF_ANDROID_USABLE_FB)
			available_num_framebuffer[0]++;
		else
			printk_err("framebuffer access control failed.\n");
	}
#endif
#if FEATURE_FRAMEACCESS & 2
	if (type & FRAMEACESS_FB_FOR_HDMI) {
		if (available_num_framebuffer[1] <
			NUM_OF_ANDROID_USABLE_HDMI_FB)
			available_num_framebuffer[1]++;
		else
			printk_err("framebuffer access control failed.\n");
	}
#endif

	spin_unlock(&irqlock_framebuffer);

	wake_up_all(&wait_framebuffer_available);
}
static int  wait_ready_usable_framebuffer(int type)
{
	int      rc;

	switch (type) {
#if FEATURE_FRAMEACCESS == 1
	case FRAMEACESS_FB_FOR_LCD:
		rc = wait_event_timeout(
			wait_framebuffer_available,
			available_num_framebuffer[0] != 0,
			msecs_to_jiffies(BUSYLOCK_WAITTIME));
		break;
#endif
#if FEATURE_FRAMEACCESS == 2
	case FRAMEACESS_FB_FOR_HDMI:
		rc = wait_event_timeout(
			wait_framebuffer_available,
			available_num_framebuffer[1] != 0,
			msecs_to_jiffies(BUSYLOCK_WAITTIME));
		break;
#endif
	default:
		/* FRAMEACESS_FB_FOR_LCD | FRAMEACESS_FB_FOR_HDMI */
		rc = wait_event_timeout(
			wait_framebuffer_available,
			available_num_framebuffer[0] != 0 &&
			available_num_framebuffer[1] != 0,
			msecs_to_jiffies(BUSYLOCK_WAITTIME));
		break;
	}

	return rc;
}
static int decrement_usable_framebuffer(int type)
{
	int      rc;

	rc = wait_ready_usable_framebuffer(type);

	if (rc == 0) {
		/* report information */
		printk_dbg2(3, "detect timeout.\n");
		rc = -EBUSY;
	} else if (rc > 0) {
		/* set default code */
		rc = 0;
	} else {
		/* report error */
		printk_err2("error in wait_event_timeout\n");
		rc = -EINVAL;
	}

	printk_dbg2(3, "spinlock\n");
	spin_lock(&irqlock_framebuffer);

#if FEATURE_FRAMEACCESS & 1
	if (type & FRAMEACESS_FB_FOR_LCD) {
		if (available_num_framebuffer[0] > 0)
			available_num_framebuffer[0]--;
		else
			printk_err("framebuffer access control failed.\n");
	}
#endif
#if FEATURE_FRAMEACCESS & 2
	if (type & FRAMEACESS_FB_FOR_HDMI) {
		if (available_num_framebuffer[1] > 0)
			available_num_framebuffer[1]--;
		else
			printk_err("framebuffer access control failed.\n");
	}
#endif

	spin_unlock(&irqlock_framebuffer);

	return rc;
}
#endif

static void sh_mobile_composer_notifyfatalerror(void)
{
	rtapi_hungup = true;

	printk_err("set hang-up flag\n");
}

static void handle_list_blend_request(int mask)
{
	struct composer_rh *rh;

	/***********************************
	* handle blend request for LCD
	***********************************/
	if ((mask & BLEND_REQ_LCD) == 0) {
		/* do not handle blend request for LCD. */
		goto skip_lcd_blend;
	}

	printk_dbg2(3, "spinlock\n");
	spin_lock(&irqlock_list);

	rh = NULL;
	if (down_trylock(&sem_framebuf_useable)) {
		/* semaphore is not acquired. */
		printk_dbg2(3, "not ready to blend for LCD.\n");
	} else {
		if (!list_empty(&top_lcd_list)) {
			rh = list_first_entry(&top_lcd_list,
				struct composer_rh, lcd_list);

			/* remove list */
			list_del_init(&rh->lcd_list);
		} else {
			/* no blend request. release semaphore */
			up(&sem_framebuf_useable);
		}
	}
	spin_unlock(&irqlock_list);

	if (rh) {
		if ((rh->refmask_disp & REFFLAG_DISPLAY_LCD) == 0)
			printk_err("blend request list invalid.\n");

		/* request to start blending */
		printk_dbg2(3, "schedule to blend for lcd.\n");

#if FEATURE_LCD_WORKQUEUE
		/* queue tasks */
		localwork_flush(workqueue, &rh->rh_wqtask);
#endif
		if (!localwork_queue(workqueue, &rh->rh_wqtask)) {
			/* fatal error */
			printk_err("drop blend request.\n");
		}
	}
skip_lcd_blend:;

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	/***********************************
	* handle blend request for HDMI
	***********************************/
	if ((mask & BLEND_REQ_HDMI) == 0) {
		/* do not handle blend request for HDMI. */
		goto skip_hdmi_blend;
	}

	printk_dbg2(3, "spinlock\n");
	spin_lock(&irqlock_list);

	rh = NULL;
	if (down_trylock(&sem_hdmi_framebuf_useable)) {
		/* semaphore is not acquired. */
		printk_dbg2(3, "not ready to blend for HDMI.\n");
	} else {
		if (!list_empty(&top_hdmi_list)) {
			rh = list_first_entry(&top_hdmi_list,
				struct composer_rh, hdmi_list);

			/* remove list */
			list_del_init(&rh->hdmi_list);
		} else {
			/* no blend request. release semaphore */
			up(&sem_hdmi_framebuf_useable);
		}
	}
	spin_unlock(&irqlock_list);

	if (rh) {
		if ((rh->refmask_disp & REFFLAG_DISPLAY_HDMI) == 0)
			printk_err("blend request list invalid.\n");

		/* request to start blending */
		printk_dbg2(3, "schedule to blend for hdmi.\n");

		/* queue tasks */
#if FEATURE_HDMI_WORKQUEUE
		localwork_flush(workqueue,
			&rh->rh_wqtask_hdmi_blend);
#endif
		if (!localwork_queue(workqueue,
			&rh->rh_wqtask_hdmi_blend)) {
			/* fatal error */
			printk_err("drop blend request.\n");
		}
	}
skip_hdmi_blend:;
#endif
}

#if FEATURE_SKIP_HDMI || FEATURE_SKIP_LCD
static int skip_confirm_condition(struct composer_rh *rh, int mode)
{
	struct list_head *head;
	int    skip_limit;
	int    skip = false;

	printk_dbg2(2, "mode:%d current task %p.\n", mode, rh);

	printk_dbg2(3, "spinlock\n");
	spin_lock(&irqlock_list);

	if (mode == SKIP_MODE_LCD) {
		/* for LCD */
		head = &top_lcd_list;
		skip_limit = NUM_OF_CONTIG_SKIP_LCD;
	} else {
		/* for HDMI */
		head = &top_hdmi_list;
		skip_limit = NUM_OF_CONTIG_SKIP_HDMI;
	}
#if _LOG_DBG > 1
	{
		struct list_head *list;
		int num_request = 0;
		list_for_each(list, head)
		{
			/* count up list */
			num_request++;
		}
		printk_dbg2(3, "mode:%d num of request:%d\n",
			mode, num_request);
	}
#endif
	if (!list_empty(head)) {
		/* there is subsequence request. */
		skip = true;
	}

	spin_unlock(&irqlock_list);

	if (skip) {
		/* increment contig skip count. */
		skip_frame_count[mode][1]++;

		if (skip_frame_count[mode][1] > skip_limit) {
			/* skip is not allowed */
			skip_frame_count[mode][1] = 0;
			return false;
		}

		/* increment skip count. */
		skip_frame_count[mode][0]++;
		return true;
	}
	return  false;
}
#endif

static void work_schedule(struct localwork *work)
{
	struct composer_rh *rh;

	ATRACE_BEGIN("schedule");

	rh = container_of(work, struct composer_rh, rh_wqtask_schedule);

	DBGENTER("work:%p\n", work);

#if _LOG_DBG > 1
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	if (!list_empty(&rh->hdmi_list))
		printk_err("link list for hdmi_list invalid.\n");
#endif
	if (!list_empty(&rh->lcd_list))
		printk_err("link list for lcd_list invalid.\n");
#endif

	/* blend request add to control. */
	if (rh->lcd_data.valid) {
		/* handle sync_wait */
#ifdef CONFIG_SYNC
		printk_dbg2(3, "wait sync fence.\n");
		fence_wait(rh, WORK_DISPDRAW_SWFENCE_WAITTIME,
			BUFFER_USAGE_BLEND);
#if _TIM_DBG
		timerecord_record(rh->timerecord, TIMID_SWSYNC1);
#endif
#endif
		/* append lists */
		printk_dbg2(3, "spinlock\n");
		spin_lock(&irqlock_list);

		list_add_tail(&rh->lcd_list, &top_lcd_list);

		spin_unlock(&irqlock_list);

		/* issue blend request if available. */
		handle_list_blend_request(BLEND_REQ_LCD);
	}

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	/* blend request add to control. */
	if (rh->hdmi_data.valid) {
		/* handle sync_wait */
#ifdef CONFIG_SYNC
		printk_dbg2(3, "wait sync fence.\n");
		fence_wait(rh, WORK_DISPDRAW_SWFENCE_WAITTIME,
			BUFFER_USAGE_OUTPUT);
#if _TIM_DBG
		timerecord_record(rh->timerecord, TIMID_SWSYNC2);
#endif
#endif

		/* append lists */
		printk_dbg2(3, "spinlock\n");
		spin_lock(&irqlock_list);

		list_add_tail(&rh->hdmi_list, &top_hdmi_list);

		spin_unlock(&irqlock_list);

		/* issue blend request if available. */
		handle_list_blend_request(BLEND_REQ_HDMI);
	}
#endif

	ATRACE_END("schedule");
	DBGLEAVE("\n");
	return;
}

static int work_delete_handle(unsigned long *args __always_unused)
{
	TRACE_ENTER(FUNC_WQ_DELETE);
	DBGENTER("\n");

	ATRACE_BEGIN("del_handle");

	if (down_trylock(&sem) == 0) {
		/* error report */
		printk_err("acquire semaphore needs to exclusive-control\n");
		up(&sem);
	}

	if (graphic_handle) {
		/* ignore rtapi-error because of an unrecoverable. */
		lcd_rtapi_delete(graphic_handle);

		graphic_handle = NULL;
	}
#if FEATURE_FRAMEACCESS & 1
#if NUM_OF_ANDROID_USABLE_FB > 1
	if (info_fb.direct_display[0]) {
		info_fb.direct_display[0] = false;
		/* finish using framebuffer of current display */
		increment_usable_framebuffer(FRAMEACESS_FB_FOR_LCD);
	}
#endif
#endif

	ATRACE_END("del_handle");
	TRACE_LEAVE(FUNC_WQ_DELETE);
	DBGLEAVE("\n");
	return 0;
}

static int work_create_handle(unsigned long *args __always_unused)
{
	int lcd_width, lcd_height;
	int hdmi_width, hdmi_height;

	TRACE_ENTER(FUNC_WQ_CREATE);
	DBGENTER("\n");

	ATRACE_BEGIN("create_handle");

	if (down_trylock(&sem) == 0) {
		/* error report */
		printk_err("acquire semaphore needs to exclusive-control\n");
		up(&sem);
	}

	if (graphic_handle) {
		/* error report and free handle to re-create graphic handle */
		printk_err("graphic_handle is not NULL\n");
		lcd_rtapi_delete(graphic_handle);
		graphic_handle = NULL;
	}

	if (rtapi_hungup) {
		printk_err1("graphics system hungup.\n");
		goto finish;
	}

	/* create graphic handle */
	lcd_get_resolution(&lcd_width, &lcd_height, &info_fb);

	hdmi_width = 0;
	hdmi_height = 0;
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI > 1
	hdmi_width = info_hdmi.max_width;
	hdmi_height = info_hdmi.max_height;
#endif

	if (!set_blend_size_flag) {
		graphic_handle = lcd_rtapi_create(lcd_width, lcd_height,
			hdmi_width, hdmi_height);
		set_blend_size_flag = true;
	} else {
		/* skip configure blend image size */
		graphic_handle = lcd_rtapi_create(0, 0, 0, 0);
	}

finish:
	ATRACE_END("create_handle");
	TRACE_LEAVE(FUNC_WQ_CREATE);
	DBGLEAVE("\n");
	return 0;
}

static int work_re_initialize_handle(unsigned long *args __always_unused)
{
	TRACE_ENTER(FUNC_WQ_CREATE);
	DBGENTER("\n");

	work_delete_handle(NULL);
	set_blend_size_flag = false;
	work_create_handle(NULL);

	DBGLEAVE("\n");
	return 0;
}

static void complete_work_blend(struct composer_rh *rh)
{
	DBGENTER("rh:%p\n", rh);

	ATRACE_BEGIN("blend_lcd comp");

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
#if _LOG_DBG > 1
	/* detect un-expected condition. */
	WARN_ON(swsync_handle &&
		rh->fence_signal_flag_c[FENCE_SIGNAL_BLEND]);
#endif
	if (!rh->fence_signal_flag_c[FENCE_SIGNAL_BLEND]) {
		printk_dbg2(3, "generate signal for blend\n");
		fence_signal(swsync_handle);
		rh->fence_signal_flag_c[FENCE_SIGNAL_BLEND] = true;
	} else {
		/* report log. already signaled or not available. */
		printk_dbg2(3, "ignore generate signal for blend\n");
	}
#endif

	/* process callback */
	process_composer_queue_callback(rh);

#if FEATURE_FRAMEACCESS & 1

#if NUM_OF_ANDROID_USABLE_FB > 1
	{
		int direct = false;

		if (rh->lcd_data.display == DISPLAY_FLAG_UNBLANK &&
			!rh->lcd_data.need_blend) {
			/* this buffer is directly    */
			/* display after this event.  */
			direct = true;
		}

		/* backup previous display mode */
		info_fb.direct_display[1] = info_fb.direct_display[0];

		/* set direct display flag */
		info_fb.direct_display[0] = direct;

		printk_dbg2(2, "direct display %d %d\n",
			info_fb.direct_display[0],
			info_fb.direct_display[1]);

		if (info_fb.direct_display[1]) {
			/* finish using framebuffer of previous display */
			increment_usable_framebuffer(FRAMEACESS_FB_FOR_LCD);
		}
		if (!info_fb.direct_display[0]) {
			/* finish using framebuffer of current display */
			increment_usable_framebuffer(FRAMEACESS_FB_FOR_LCD);
		}
	}
#else
	/* finish using framebuffer */
	increment_usable_framebuffer(FRAMEACESS_FB_FOR_LCD);
#endif

#endif

	ATRACE_END("blend_lcd comp");
	DBGLEAVE("\n");
	return;
}

static void work_blend(struct localwork *work)
{
	struct composer_rh *rh;
	int  rc;
	int  blend_flag = 0;

	TRACE_ENTER(FUNC_WQ_BLEND);
	DBGENTER("work:%p\n", work);

	ATRACE_BEGIN("blend_lcd");

	rh = container_of(work, struct composer_rh, rh_wqtask);

#if _TIM_DBG
	timerecord_record(rh->timerecord, TIMID_BLEND_S);
#endif

	printk_dbg2(2, "lcd_data.valid:%d\n", rh->lcd_data.valid);

#if _LOG_DBG > 1
	/* detect un-expected condition. */
	WARN_ON(rh->lcd_data.valid == false || rh->active == false);
#endif

#if FEATURE_SKIP_LCD
	if (skip_confirm_condition(rh, SKIP_MODE_LCD)) {
		printk_dbg1(1, "task %p skipped.\n", rh);

		blend_flag = 2;
		rc = CMP_OK;
		goto finish;
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (in_early_suspend) {
		printk_err1("composition was skipped because " \
			"already in early suspend state.\n");
		rc = CMP_OK;
		goto finish;
	}
#endif
	if (graphic_handle == NULL) {
		printk_err1("handle not created.\n");
		rc = CMP_NG;
		goto finish;
	}
	if (rtapi_hungup) {
		printk_err1("graphics system hungup.\n");
		rc = CMP_NG;
		goto finish;
	}

	/* last configuration of blend output */
	rc = lcd_config_output(rh, &info_fb);
	if (rc != CMP_OK) {
		/* can not configure output */
		printk_err2("error in config_output.\n");
		goto finish;
	}

	blend_flag = 1;

	if (rh->lcd_data.need_blend) {
		/* run blending */
		rc = lcd_rtapi_blend(graphic_handle, rh);
	} else {
		printk_dbg2(3, "skip blend.\n");
		rc = CMP_OK;
	}

finish:

#if FEATURE_SKIP_LCD
	if (blend_flag == 2) {
		/* reserved for skip */
		if (rh->lcd_data.display) {
			/* change display type as skip */
			rh->lcd_data.display = DISPLAY_FLAG_UNBLANK_SKIP;
		}
	}
#endif
	if (!blend_flag || rc != CMP_OK) {
		/* disable display to avoid illegal display. */
		rh->lcd_data.display = DISPLAY_FLAG_BLANK;
	}

	printk_dbg1(2, "results rc:%d\n", rc);
	if (rc != CMP_OK) {
		const char *msg1 = "";
		const char *msg2 = "";
		const char *msg3 = "";

		/* error report */
#ifdef CONFIG_HAS_EARLYSUSPEND
		if (in_early_suspend) {
			/* set message */
			msg1 = "[in suspend]";
		}
#endif
		if (graphic_handle == NULL) {
			/* set message */
			msg2 = "[handle not create]";
		}
		if (rtapi_hungup) {
			/* set message */
			msg3 = "[rtapi fatal]";
		}
		printk_err("blend result is error %s, %s, %s\n",
			msg1, msg2, msg3);
	}

#if _TIM_DBG
	timerecord_record(rh->timerecord, TIMID_BLEND_E);
#endif

	if (rh->lcd_data.need_blend) {
		/* complete blend task. */
		complete_work_blend(rh);
	} else {
		/* blend task complete
		   processed in display task. */
	}

#if FEATURE_LCD_WORKQUEUE
	/* schedule to display. */
	localwork_flush(VAR_WORKQUEUE_LCD, &rh->rh_wqtask_disp);
	localwork_queue(VAR_WORKQUEUE_LCD, &rh->rh_wqtask_disp);
#else
	work_dispdraw(&rh->rh_wqtask_disp);
#endif

	ATRACE_END("blend_lcd");
	TRACE_LEAVE(FUNC_WQ_BLEND);
	DBGLEAVE("\n");
	return;
}

static void complete_work_dispdraw(struct composer_rh *rh)
{
	DBGENTER("rh:%p\n", rh);

	ATRACE_BEGIN("disp_lcd comp");

	up(&sem_framebuf_useable);
	if (sem_framebuf_useable.count > NUM_OF_FRAMEBUFFER_MAXIMUM)
		printk_err("semaphore sem_framebuf_usable invalid.\n");

#if _TIM_DBG
	timerecord_record(rh->timerecord, TIMID_DISP_E);
#endif

	/* clear display flag. */
	if (rh->refmask_disp & REFFLAG_DISPLAY_LCD) {
		printk_dbg2(3, "down\n");
		down(&kernel_queue_sem);

		rh->refmask_disp &= ~REFFLAG_DISPLAY_LCD;
#if _TIM_DBG
		if (rh->refmask_disp == 0)
			timerecord_print(rh->timerecord);
#endif

		up(&kernel_queue_sem);

		/* wake-up waiting thread */
		wake_up_all(&kernel_waitqueue_comp);
	}

	/* handle blend request */
	handle_list_blend_request(BLEND_REQ_LCD);

	ATRACE_END("disp_lcd comp");
	DBGLEAVE("\n");
	return;
}

static void work_dispdraw(struct localwork *work)
{
	struct composer_rh *rh;

	DBGENTER("work:%p\n", work);
	TRACE_ENTER(FUNC_WQ_DISP);

	ATRACE_BEGIN("disp_lcd");

	rh = container_of(work, struct composer_rh, rh_wqtask_disp);

#if _TIM_DBG
	timerecord_record(rh->timerecord, TIMID_DISP_S);
#endif

#if FEATURE_SKIP_LCD
	if (rh->lcd_data.display == DISPLAY_FLAG_UNBLANK_SKIP) {
		printk_dbg2(3, "skip display\n");
		goto finish;
	}
#endif
	if (!rh->lcd_data.display) {
		printk_dbg2(3, "disable display\n");
		goto finish;
	}

#if FEATURE_LCD_WORKQUEUE
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI && FEATURE_HDMI_WAIT_TIMING == 0
	/* wait complete output task */
	if (rh->hdmi_data.valid) {
		/* wait task complete to management buffer.*/
		printk_dbg2(3, "wait complete output\n");
		localwork_flush(VAR_WORKQUEUE_HDMI,
			&rh->rh_wqtask_hdmi);
	}
#endif
#endif

	/* display blend result */
	lcd_fb_pan_display(rh, &info_fb);

finish:
	if (!rh->lcd_data.need_blend) {
		/* complete blend task. */
		complete_work_blend(rh);
	}

#if FEATURE_LCD_WORKQUEUE
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI && FEATURE_HDMI_WAIT_TIMING == 1
	/* wait complete output task */
	if (rh->hdmi_data.valid) {
		/* wait task complete to management buffer.*/
		printk_dbg2(3, "wait complete output\n");
		localwork_flush(VAR_WORKQUEUE_HDMI,
			&rh->rh_wqtask_hdmi);
	}
#endif
#endif

	complete_work_dispdraw(rh);

	ATRACE_END("disp_lcd");
	TRACE_LEAVE(FUNC_WQ_DISP);
	DBGLEAVE("\n");
	return;
}

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
static int work_alloc_memory_hdmi(unsigned long *args)
{
	unsigned long size = args[0];

	DBGENTER("args:%p\n", args);
	printk_dbg1(3, "allocate %ld for hdmi.\n", size);

	hdmi_memory_allocate(&info_hdmi, size);

	DBGLEAVE("\n");
	return 0;
}
static int work_free_memory_hdmi(unsigned long *args)
{
	DBGENTER("args:%p\n", args);

	if (graphic_handle_hdmi) {
		/* destroy handle of HDMI. */
		work_delete_handle_hdmi(NULL);
	}
	hdmi_memory_free(&info_hdmi);

	DBGLEAVE("\n");
	return 0;
}

static int work_delete_handle_hdmi(unsigned long *args __always_unused)
{
	TRACE_ENTER(FUNC_WQ_DELETE_HDMI);
	DBGENTER("\n");

	ATRACE_BEGIN("del_handle hdmi");

	if (graphic_handle_hdmi) {
#ifdef CONFIG_MACH_KOTA2
		/* Kota2 not support graphic output. */
		graphic_handle_hdmi = NULL;
#elif SH_MOBILE_COMPOSER_SUPPORT_HDMI <= 1
		/* HDMI type is 1. */
		graphic_handle_hdmi = NULL;
#else
		/* ignore rtapi-error because of an unrecoverable. */
		hdmi_rtapi_delete(graphic_handle_hdmi);

		graphic_handle_hdmi = NULL;
#endif
	}
#if FEATURE_FRAMEACCESS & 2
#if NUM_OF_ANDROID_USABLE_HDMI_FB > 1
	if (info_hdmi.direct_display[0]) {
		info_hdmi.direct_display[0] = false;
		/* finish using framebuffer of previous display */
		increment_usable_framebuffer(FRAMEACESS_FB_FOR_HDMI);
	}
#endif
#endif

	ATRACE_END("del_handle hdmi");
	TRACE_LEAVE(FUNC_WQ_DELETE_HDMI);
	DBGLEAVE("\n");
	return 0;
}

static int work_create_handle_hdmi(unsigned long *args __always_unused)
{
	TRACE_ENTER(FUNC_WQ_CREATE_HDMI);
	DBGENTER("\n");

	ATRACE_BEGIN("create_handle hdmi");

	/* currently not implemented. */
	if (rtapi_hungup) {
		/* report error */
		printk_err1("graphics system hungup.\n");
	} else if (graphic_handle_hdmi == NULL) {
#ifdef CONFIG_MACH_KOTA2
		/* Kota2 not support graphic output. */
		graphic_handle_hdmi = NULL;
#elif SH_MOBILE_COMPOSER_SUPPORT_HDMI <= 1
		/* HDMI type is 1. or RT-API not available. */
		graphic_handle_hdmi = NULL;
#else
		/* screen_graphics_image_output available */
		graphic_handle_hdmi = hdmi_rtapi_create();
#endif
	}

	ATRACE_END("create_handle hdmi");
	TRACE_LEAVE(FUNC_WQ_CREATE_HDMI);
	DBGLEAVE("\n");
	return 0;
}
#endif

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
static void complete_work_blend_hdmi(struct composer_rh *rh)
{
	DBGENTER("rh:%p\n", rh);

	ATRACE_BEGIN("blend_hdmi comp");

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
#if _LOG_DBG > 1
	/* detect un-expected condition. */
	WARN_ON(swsync_handle_hdmi &&
		rh->fence_signal_flag_c[FENCE_SIGNAL_OUTPUT]);
#endif
	if (!rh->fence_signal_flag_c[FENCE_SIGNAL_OUTPUT]) {
		printk_dbg2(3, "generate signal for output\n");
		fence_signal(swsync_handle_hdmi);
		rh->fence_signal_flag_c[FENCE_SIGNAL_OUTPUT] = true;
	} else {
		/* report log. already signaled or not available. */
		printk_dbg2(3, "ignore generate signal for output\n");
	}
#endif

	/* process callback */
	process_composer_queue_callback(rh);

#if FEATURE_FRAMEACCESS & 2

#if NUM_OF_ANDROID_USABLE_HDMI_FB > 1
	{
		int direct = false;

		if (rh->hdmi_data.display == DISPLAY_FLAG_UNBLANK &&
			!rh->hdmi_data.need_blend) {
			switch (rh->hdmi_data.layer[0].image.format) {
			case RT_GRAPHICS_COLOR_ARGB8888:
			case RT_GRAPHICS_COLOR_YUV420SP:
			case RT_GRAPHICS_COLOR_YUV422SP:
				/* this buffer is not directly */
				/* display after this event.   */
				break;
			default:
				/* other buffer is directly    */
				/* display after this event.   */
				direct = true;
				break;
			}
		}

		/* backup previous display mode */
		info_hdmi.direct_display[1] = info_hdmi.direct_display[0];

		/* set direct display flag */
		info_hdmi.direct_display[0] = direct;

		printk_dbg2(2, "direct display %d %d\n",
			info_hdmi.direct_display[0],
			info_hdmi.direct_display[1]);

		if (info_hdmi.direct_display[1]) {
			/* finish using framebuffer of previous display */
			increment_usable_framebuffer(FRAMEACESS_FB_FOR_HDMI);
		}
		if (!info_hdmi.direct_display[0]) {
			/* finish using framebuffer of current display */
			increment_usable_framebuffer(FRAMEACESS_FB_FOR_HDMI);
		}
	}
#else
	/* finish using framebuffer */
	increment_usable_framebuffer(FRAMEACESS_FB_FOR_HDMI);
#endif

#endif

	ATRACE_END("blend_hdmi comp");
	DBGLEAVE("\n");
	return;
}

static void work_blend_hdmi(struct localwork *work)
{
	struct composer_rh *rh;
	int  rc;
	int  blend_flag = 0;

	TRACE_ENTER(FUNC_WQ_BLEND_HDMI);
	DBGENTER("work:%p\n", work);

	ATRACE_BEGIN("blend_hdmi");

	rh = container_of(work, struct composer_rh, rh_wqtask_hdmi_blend);

#if _TIM_DBG
	timerecord_record(rh->timerecord, TIMID_BLEND_HDMI_S);
#endif

	printk_dbg2(2, "hdmi_data.valid:%d hdmi_data.display:%d\n",
		rh->hdmi_data.valid, rh->hdmi_data.display);

	if (!rh->hdmi_data.display) {
		/* blending is not necessary. */
		rc = CMP_OK;
		goto finish;
	}

#if _LOG_DBG > 1
	/* detect un-expected condition. */
	WARN_ON(rh->hdmi_data.valid == false || rh->active == false);
#endif

#if FEATURE_SKIP_HDMI
	if (skip_confirm_condition(rh, SKIP_MODE_HDMI)) {
		printk_dbg1(1, "task %p skipped.\n", rh);

		blend_flag = 2;
		rc = CMP_OK;
		goto finish;
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	if (in_early_suspend) {
		printk_err1("composition was skipped because " \
			"already in early suspend state.\n");
		rc = CMP_OK;
		goto finish;
	}
#endif
	if (graphic_handle == NULL) {
		printk_err1("handle not created.\n");
		rc = CMP_NG;
		goto finish;
	}
	if (rtapi_hungup) {
		printk_err1("graphics system hungup.\n");
		rc = CMP_NG;
		goto finish;
	}

	/* acquire mutex to prevent free memory. */
	printk_dbg2(3, "down\n");
	down(&sem_hdmimemory);

	/* last configuration of blend output */
	rc = hdmi_config_output(rh, &info_hdmi);
	if (rc != CMP_OK) {
		/* can not configure output */
		printk_err2("error in config_output.\n");
		rc = CMP_OK;
		goto finish2;
	}

	blend_flag = 1;

	if (rh->hdmi_data.need_blend) {
		/* run blending */
		rc = hdmi_rtapi_blend(graphic_handle, rh);
	} else {
		printk_dbg2(3, "skip blend hdmi.\n");
		rc = CMP_OK;
	}

finish2:
	up(&sem_hdmimemory);

finish:

#if FEATURE_SKIP_HDMI
	if (blend_flag == 2) {
		/* reserved for skip */
		if (rh->hdmi_data.display) {
			/* change display type as skip */
			rh->hdmi_data.display = DISPLAY_FLAG_UNBLANK_SKIP;
		}
	}
#endif
	if (!blend_flag || rc != CMP_OK) {
		/* disable display to avoid illegal display. */
		rh->hdmi_data.display = DISPLAY_FLAG_BLANK;
	}

	printk_dbg1(2, "results rc:%d\n", rc);
	if (rc != CMP_OK) {
		const char *msg1 = "";
		const char *msg2 = "";
		const char *msg3 = "";

		/* error report */
#ifdef CONFIG_HAS_EARLYSUSPEND
		if (in_early_suspend) {
			/* set message */
			msg1 = "[in suspend]";
		}
#endif
		if (graphic_handle == NULL) {
			/* set message */
			msg2 = "[handle not create]";
		}
		if (rtapi_hungup) {
			/* set message */
			msg3 = "[rtapi fatal]";
		}
		printk_err("blend result is error %s, %s, %s\n",
			msg1, msg2, msg3);
	}

#if _TIM_DBG
	timerecord_record(rh->timerecord, TIMID_BLEND_HDMI_E);
#endif

	if (rh->hdmi_data.need_blend) {
		/* blend task complete. */
		complete_work_blend_hdmi(rh);
	} else {
		/* blend task complete
		   processed in output task. */
	}
#if FEATURE_HDMI_WORKQUEUE
	/* schedule to display. */
	localwork_flush(VAR_WORKQUEUE_HDMI, &rh->rh_wqtask_hdmi);
	localwork_queue(VAR_WORKQUEUE_HDMI, &rh->rh_wqtask_hdmi);
#else
	/* perform output */
	work_dispdraw_hdmi(&rh->rh_wqtask_hdmi);
#endif

	ATRACE_END("blend_hdmi");
	TRACE_LEAVE(FUNC_WQ_BLEND_HDMI);
	DBGLEAVE("\n");
	return;
}

static void complete_work_dispdraw_hdmi(struct composer_rh *rh)
{
	DBGENTER("rh:%p\n", rh);

	ATRACE_BEGIN("disp_hdmi comp");

	up(&sem_hdmi_framebuf_useable);
	if (sem_hdmi_framebuf_useable.count > NUM_OF_HDMI_FRAMEBUFFER_MAXIMUM)
		printk_err("semaphore sem_hdmi_framebuf_useable invalid.\n");

	/* clear display flag. */
	if (rh->refmask_disp & REFFLAG_DISPLAY_HDMI) {
		printk_dbg2(3, "down\n");
		down(&kernel_queue_sem);

		rh->refmask_disp &= ~REFFLAG_DISPLAY_HDMI;
#if _TIM_DBG
		if (rh->refmask_disp == 0)
			timerecord_print(rh->timerecord);
#endif

		up(&kernel_queue_sem);

		/* wake-up waiting thread */
		wake_up_all(&kernel_waitqueue_comp);
	}
	/* handle blend request */
	handle_list_blend_request(BLEND_REQ_HDMI);

	ATRACE_END("disp_hdmi comp");
	DBGLEAVE("\n");
	return;
}

static void work_dispdraw_hdmicomp(struct localwork *work)
{
	struct composer_rh *rh;

	TRACE_ENTER(FUNC_WQ_DISP_HDMICOMP);
	DBGENTER("work:%p\n", work);

	ATRACE_BEGIN("disp_hdmi2");

	rh = container_of(work, struct composer_rh, rh_wqtask_hdmi_comp);

	if ((rh->refmask_disp & REFFLAG_DISPLAY_HDMI) == 0) {
		printk_err("illegal scheduling\n");
		/* nothing to do */
	} else {
#if _ATR_DBG
		if (rh->hdmi_data.display) {
			int use_id = atrace_hdmi_display_complete & 1;

			if (!(atrace_hdmi_display_used & (1 << use_id)))
				printk_err("ATRACE_DEBUG not " \
					"propery implemented.\n");

			atrace_hdmi_display_used &= ~(1 << use_id);
			atrace_hdmi_display_complete++;
			if (use_id) {
				/* mark used buffer */
				ATRACE_INT("hdmi0", 0);
			} else {
				/* mark used buffer */
				ATRACE_INT("hdmi1", 0);
			}
		}
#endif

		printk_dbg2(2, "output state: %d\n",
			rh->rh_wqwait_hdmi.status);

		if (!rh->hdmi_data.need_blend) {
			/* complete blend task. */
			complete_work_blend_hdmi(rh);
		}

#if _TIM_DBG
		timerecord_record(rh->timerecord, TIMID_DISP_HDMI_E);
#endif

		/* complete output task. */
		complete_work_dispdraw_hdmi(rh);
	}

	ATRACE_END("disp_hdmi2");
	TRACE_LEAVE(FUNC_WQ_DISP_HDMICOMP);
	DBGLEAVE("\n");
	return;
}

static void work_dispdraw_hdmi(struct localwork *work)
{
	struct composer_rh *rh;
	int  output_flag = false;

	TRACE_ENTER(FUNC_WQ_DISP_HDMI);
	DBGENTER("work:%p\n", work);

	ATRACE_BEGIN("disp_hdmi1");

	rh = container_of(work, struct composer_rh, rh_wqtask_hdmi);

#if _TIM_DBG
	timerecord_record(rh->timerecord, TIMID_DISP_HDMI_S);
#endif

	printk_dbg2(2, "hdmi_data.valid:%d hdmi_data.display:%d\n",
		rh->hdmi_data.valid, rh->hdmi_data.display);

#ifdef CONFIG_MACH_KOTA2
	/* Kota2 not support graphic output. */
#elif SH_MOBILE_COMPOSER_SUPPORT_HDMI <= 1
	/* HDMI type is 1. or RT-API not available. */
#else
#if FEATURE_SKIP_HDMI
	if (rh->hdmi_data.display == DISPLAY_FLAG_UNBLANK_SKIP) {
		printk_dbg2(3, "skip output\n");
		goto finish2;
	}
#endif

	/*************************************/
	/* create/delete graphics handle     */
	/*************************************/
	if (graphic_handle_hdmi) {
		if (!rh->hdmi_data.display) {
			/* destroy handle of HDMI. */
			work_delete_handle_hdmi(NULL);
		}
	} else {
		if (rh->hdmi_data.display) {
			if (!hdmi_output_enable) {
				/* report logs */
				printk_dbg1(3, "hdmi is disabled.\n");
			} else {
				/* create handle og HDMI */
				work_create_handle_hdmi(NULL);
			}
		}
	}

	/*************************************/
	/* handle output                     */
	/*************************************/
	if (rh->hdmi_data.display) {
		int rc;
#ifdef CONFIG_HAS_EARLYSUSPEND
		if (in_early_suspend) {
			printk_dbg2(1, "suspend state.\n");
			rc = CMP_NG;
			goto finish;
		}
#endif
		if (graphic_handle_hdmi == NULL) {
			printk_err1("handle for HDMI not created.\n");
			rc = CMP_NG;
			goto finish;
		} else if (rtapi_hungup) {
			printk_err1("graphics system hungup.\n");
			rc = CMP_NG;
			goto finish;
		} else {
			/* output */
			rc = hdmi_rtapi_output(graphic_handle_hdmi, rh);
			if (rc == CMP_OK) {
				/* set output flag */
				output_flag = true;
			}
		}

finish:
		printk_dbg1(2, "results rc:%d\n", rc);
		if (rc != CMP_OK) {
			/* report error */
			printk_err1("output request result is error.\n");
		}
	}
#endif

#if FEATURE_SKIP_HDMI
finish2:
#endif
	if (output_flag) {
#if _ATR_DBG
		int use_id = atrace_hdmi_display_request & 1;

		if (atrace_hdmi_display_used & (1 << use_id))
			printk_err("ATRACE_DEBUG not propery implemented.\n");

		atrace_hdmi_display_used |= (1 << use_id);
		atrace_hdmi_display_request++;
		if (use_id) {
			/* mark used buffer */
			ATRACE_INT("hdmi0", 1);
		} else {
			/* mark used buffer */
			ATRACE_INT("hdmi1", 1);
		}
#endif
#if _TIM_DBG
		timerecord_record(rh->timerecord, TIMID_DISP_HDMI_O);
#endif
		/* task completion is delayed until call back occurs. */
	} else {
		/* forcely off display to return current display buffer */
		if (graphic_handle_hdmi) {
			if (!rh->hdmi_data.display) {
				/* destroy handle of HDMI. */
				work_delete_handle_hdmi(NULL);
			}
		}

		/* schedule to complete. */
		localwork_flush(VAR_WORKQUEUE_HDMI,
			&rh->rh_wqtask_hdmi_comp);
		localwork_queue(VAR_WORKQUEUE_HDMI,
			&rh->rh_wqtask_hdmi_comp);
	}

	ATRACE_END("disp_hdmi1");
	TRACE_LEAVE(FUNC_WQ_DISP_HDMI);
	DBGLEAVE("\n");
	return;
}
#endif /* SH_MOBILE_COMPOSER_SUPPORT_HDMI */

static int composer_set_address(
	unsigned int id, unsigned long addr, unsigned long size)
{
	int rc;

	printk_dbg2(3, "down\n");
	down(&sem);

	/* set buffer of framebuffer. */
	rc = lcd_set_address(id, addr, size, workqueue, &info_fb);

	up(&sem);
	return rc;
}

static int composer_unset_address(unsigned int id)
{
	int rc;

	/* currently not acquire semaphore.         */
	/* because core_release already acquire it. */

	rc = lcd_unset_address(id, workqueue, &info_fb);

	return rc;
}

#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
static int module_composer_register_gpu_buffer(
	unsigned long address, unsigned long size)
{
	int rc;

	DBGENTER("address:0x%lx size:0x%lx\n", address, size);

	rc = composer_set_address(FB_SCREEN_BUFFERID1,
		address, size);

	DBGLEAVE("%d\n", rc);

	return rc;
}

static unsigned char *composer_get_RT_address(unsigned char *address)
{
	unsigned long p_addr = (unsigned long)address;
	unsigned char *rt_addr = NULL;

	/* translate physical to RT address */
	rt_addr = (char *)sh_mobile_rtmem_conv_phys2rtmem(p_addr);

	if (rt_addr == NULL) {
		/* resolve conversion by RT-API */
		if (graphic_handle == NULL) {
			/* currently not open handle */
			printk_dbg2(3, "not open rt-api handle\n");
		} else {
			system_mem_phy_change_rtaddr adr;
			int                          rc;

			adr.handle    = graphic_handle;
			adr.phys_addr = p_addr;
			adr.rtaddr    = 0;

#if _LOG_DBG >= 1
			if (3 <= debug)
				dump_system_mem_phy_change_rtaddr(
					&adr);
#endif

			rc = system_memory_phy_change_rtaddr(&adr);
			if (rc != SMAP_LIB_MEMORY_OK) {
				/* report error */
				printk_err("system_memory_phy_change_rtaddr" \
					" return by %d %s.\n", rc,
					get_RTAPImsg_memory(rc));
#if _ERR_DBG >= 1
				dump_system_mem_phy_change_rtaddr(
					&adr);
#endif
				if (rc < RTAPI_FATAL_ERROR_THRESHOLD) {
					/* notify hung-up */
					sh_mobile_composer_notifyfatalerror();
				}
			} else {
				rt_addr = (unsigned char *)adr.rtaddr;
			}
		}
	}

	printk_dbg2(2, "convert result 0x%lx to %p\n", p_addr, rt_addr);
	return rt_addr;
}

static unsigned char *module_composer_phy_change_rtaddr(unsigned long p_adr)
{
	unsigned char *rt_adr;

	rt_adr = composer_get_RT_address((unsigned char *) p_adr);

	return rt_adr;
}
#endif


#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
static int composer_enable_hdmioutput(int enable, int mode)
{
	int mask;
	int rc = CMP_OK;

	if (down_trylock(&sem) == 0) {
		/* error report */
		printk_err("acquire semaphore needs to exclusive-control\n");
		up(&sem);
	}

	if (enable) {
		/* clear mask */
		hdmi_output_enable_mask &= ~mode;
		mask = hdmi_output_enable_mask;
		printk_dbg2(3, "hdmi_output_enable_mask:%d\n", mask);

		if (!hdmi_output_enable && !mask) {
			/* enable output. */
			hdmi_output_enable = true;
		}
	} else {
		/* set mask */
		hdmi_output_enable_mask |= mode;
		mask = hdmi_output_enable_mask;
		printk_dbg2(3, "hdmi_output_enable_mask:%d\n", mask);

		if (hdmi_output_enable) {
			/* disable output. */
			hdmi_output_enable = false;
		}

		/* confirm that the graphics handle for HDMI need release. */
		if (graphic_handle_hdmi) {
			/*************************************/
			/* delete graphics handle            */
			/*************************************/

			rc = indirect_call(VAR_WORKQUEUE_HDMI,
				work_delete_handle_hdmi, 0, NULL);

			if (rc) {
				printk_err("failed to release graphic " \
					"handle for hdmi\n");
				rc = CMP_NG;
			}
		}

		if (graphic_handle_hdmi) {
			printk_dbg2(3, "release graphic handle failed.\n");
			rc = CMP_NG;
		}
	}
	printk_dbg2(2, "hdmi_output_enable:%d\n", hdmi_output_enable);
	return rc;
}

static int module_composer_hdmiset(int mode)
{
	int rc = CMP_OK;
	TRACE_ENTER(FUNC_HDMISET);
	DBGENTER("mode:%d\n", mode);

	printk_dbg2(3, "down\n");
	down(&sem);

	if (mode == CMP_HDMISET_STOP) {
		/* disable output hdmi */

		rc = composer_enable_hdmioutput(false,
			HDMI_OUTPUT_MASK_HDMISET);

		TRACE_LOG1(FUNC_HDMISET, rc);
	} else if (mode == CMP_HDMISET_STOP_CANCEL) {
		/* enable output hdmi */

		rc = composer_enable_hdmioutput(true,
			HDMI_OUTPUT_MASK_HDMISET);

		TRACE_LOG1(FUNC_HDMISET, rc);
	}
	up(&sem);

	DBGLEAVE("rc:%d\n", rc);
	TRACE_LEAVE(FUNC_HDMISET);
	return rc;
}
#endif

static void get_buffer_handle(struct composer_rh  *rh,
	struct cmp_postdata *post)
{
	int i;

	for (i = 0; i < rh->num_buffer; i++) {
		int lookup_index;
		int fd = -1;

		/* get buffer information. */
		lookup_index = post->lookup_table[i];
		if (lookup_index < 0) {
			/* buffer is not used. */
			/* nothing to do */
		} else if (lookup_index >= post->num_graphic_buffer) {
			printk_err2("lookuptable invalid.");
			/* ignore handling buffer. */
		} else {
			/* get buffer address */
			fd = post->graphic_buffer_fd[lookup_index];
		}

		printk_dbg2(2, "lookup_index:%d fd:%d\n",
			lookup_index, fd);

		if (fd >= 0) {
			/* get file handle */
			rh->buffer_handle[i] = fget(fd);
		}
	}
}

static void free_buffer_handle(struct composer_rh  *rh)
{
	int i;

	for (i = 0; i < rh->num_buffer; i++) {
		if (rh->buffer_handle[i]) {
			fput(rh->buffer_handle[i]);
			rh->buffer_handle[i] = NULL;
		}
	}
}

static int handle_queue_data_type0(
	struct composer_rh  *rh,
	struct cmp_request_queuedata *post)
{
	int i;

	if (info_fb.queue_fb_map_handle == NULL) {
		printk_dbg2(2, "frame buffer not assigned.\n");
		/* nothing to do */
		goto err_exit;
	}

	if (post->use_gpu_composition) {
		printk_err1("use_gpu_composition not supported.\n");
		goto err_exit;
	}

	/**********************************
	 confirm graphic buffer
	**********************************/
	rh->lcd_data.valid = true;
	rh->lcd_data.display = true;
	rh->lcd_data.blend = post->blend;
	for (i = 0; i < 4; i++) {
		if (post->blend.input_layer[i]) {
			rh->lcd_data.layer[i] = post->layer[i];
			rh->lcd_data.blend.input_layer[i] = \
				&rh->lcd_data.layer[i];
			if (post->layer[i].image.address == 0) {
				/* report error */
				printk_err1("image address NULL.\n");
				goto err_exit;
			}
		}
	}

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	i = post->extlayer_index;
	rh->hdmi_data.valid = true;
	rh->hdmi_data.display = false;
	if (i >= 0) {
		rh->hdmi_data.need_blend = false;
		rh->hdmi_data.display = true;
		rh->hdmi_data.layer[0] = post->layer[i];
	} else {
		if (!graphic_handle_hdmi)
			rh->hdmi_data.valid = false;
	}
#endif

	rh->refcount = 0;
	if (rh->lcd_data.valid) {
		rh->refcount++;
		rh->refmask_disp |= REFFLAG_DISPLAY_LCD;
	}
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	if (rh->hdmi_data.valid) {
		rh->refcount++;
		rh->refmask_disp |= REFFLAG_DISPLAY_HDMI;
	}
#endif

	if (rh->refcount == 0) {
		printk_err("usable blend config not found.");
		goto err_exit;
	}

	memset(rh->buffer_address, 0, sizeof(rh->buffer_address));
#ifdef CONFIG_SYNC
	memset(rh->buffer_sync, 0, sizeof(rh->buffer_sync));
#endif
	memset(&rh->buffer_handle, 0, sizeof(rh->buffer_handle));

	return CMP_OK;

err_exit:
	return CMP_NG;
}

static int handle_queue_data_type1(
	struct composer_rh  *rh,
	struct cmp_postdata *post)
{
	int i;
#if _LOG_DBG >= 2
	{
		unsigned int info[CMP_DATA_NUM_GRAP_LAYER*CMP_DATA_CHANNEL];
		for (i = 0; i < ARRAY_SIZE(info); i++)
			info[i] = 0;

		for (i = 0; i < post->num_graphic_buffer; i++) {
			if (i >= ARRAY_SIZE(info))
				break;
			info[i] = post->rtAddress[i];
		}

		printk_dbg2(3, "num_graphic_buffer:%d\n",
			post->num_graphic_buffer);
		printk_dbg2(3, "0:0x%x 1:0x%x 2:0x%x 3:0x%x 4:0x%x 5:0x%x " \
			"6:0x%x 7:0x%x 8:0x%x 9:0x%x\n",
			info[0], info[1], info[2], info[3], info[4],
			info[5], info[6], info[7], info[8], info[9]);

		for (i = 0; i < post->num_graphic_buffer; i++) {
			if (i >= ARRAY_SIZE(info))
				break;
			info[i] = post->graphic_buffer_fd[i];
		}

		printk_dbg2(3, "0:%d 1:%d 2:%d 3:%d 4:%d 5:%d " \
			"6:%d 7:%d 8:%d 9:%d\n",
			info[0], info[1], info[2], info[3], info[4],
			info[5], info[6], info[7], info[8], info[9]);

		for (i = 0; i < post->num_buffer; i++)
			info[i] = post->lookup_table[i];

		printk_dbg2(3, "num_buffer:%d\n", post->num_buffer);
		printk_dbg2(3, "lookup: buf0:%d buf1:%d buf2:%d buf3:%d " \
			"buf4:%d buf5:%d buf6:%d buf7:%d buf8:%d buf9:%d\n",
			info[0], info[1], info[2], info[3], info[4],
			info[5], info[6], info[7], info[8], info[9]);

		for (i = 0; i < ARRAY_SIZE(info); i++)
			info[i] = -1;

		for (i = 0; i < post->num_buffer; i++)
			info[i] = post->acqure_fd[i];
		printk_dbg2(3, "syncfd: buf0:%d buf1:%d buf2:%d buf3:%d " \
			"buf4:%d buf5:%d buf6:%d buf7:%d buf8:%d buf9:%d\n",
			info[0], info[1], info[2], info[3], info[4],
			info[5], info[6], info[7], info[8], info[9]);

	}
#endif
	if (post->data[0] && info_fb.queue_fb_map_handle == NULL) {
		printk_dbg2(2, "frame buffer not assigned.\n");
		/* nothing to do */
		goto err_exit;
	}

	/**********************************
	 confirm graphic buffer
	**********************************/
	if (post->num_graphic_buffer < 0 ||
		post->num_graphic_buffer > ARRAY_SIZE(post->rtAddress)) {
		printk_err2("num of graphic_buffer invalid.");
		goto err_exit;
	}
	for (i = 0; i < post->num_graphic_buffer; i++) {
		if (post->rtAddress[i] == 0) {
			printk_err2("configuration of graphic buffer failed.");
			goto err_exit;
		}
	}

	/**********************************
	 clear buffer usage.
	**********************************/
	memset(&rh->buffer_usage, 0, sizeof(rh->buffer_usage));

	/**********************************
	 resolve buffer address.
	**********************************/
	memset(&rh->buffer_address, 0, sizeof(rh->buffer_address));

	rh->num_buffer = post->num_buffer;
	for (i = 0; i < rh->num_buffer; i++) {
		int lookup_index;
		unsigned long address = 0;

		/* get buffer information. */
		lookup_index = post->lookup_table[i];
		if (lookup_index < 0) {
			/* buffer is not used. */
			/* nothing to do */
		} else if (lookup_index >= post->num_graphic_buffer) {
			printk_err2("lookuptable invalid.");
			rh->active = false;
			goto err_exit;
		} else {
			/* get buffer address */
			address = post->rtAddress[lookup_index];
		}

		rh->buffer_address[i] = address;
		printk_dbg2(2, "lookup_index:%d rt-address:0x%lx\n",
			lookup_index, address);
	}

	/**********************************
	 setup parameters.
	**********************************/
	if (post->data[0])
		post->data[0] = &post->blenddata[0];
	if (post->data[1])
		post->data[1] = &post->blenddata[1];

	if (lcd_config(rh, post) == CMP_NG) {
		printk_err2("error in blend config.");
		goto err_exit;
	}
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	if (hdmi_config(graphic_handle_hdmi, rh, post) == CMP_NG) {
		printk_err2("error in hdmi config.");
		goto err_exit;
	}
#endif

	rh->refcount = 0;
	if (rh->lcd_data.valid) {
		rh->refcount++;
		rh->refmask_disp |= REFFLAG_DISPLAY_LCD;
	}
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	if (rh->hdmi_data.valid) {
		rh->refcount++;
		rh->refmask_disp |= REFFLAG_DISPLAY_HDMI;
	}
#endif

	if (rh->refcount == 0) {
		printk_err("usable blend config not found.");
		goto err_exit;
	}

#ifdef CONFIG_SYNC
	/* configure fence_sync */
	fence_config(rh, post);
#endif

	/* get buffer handle */
	memset(&rh->buffer_handle, 0, sizeof(rh->buffer_handle));

	get_buffer_handle(rh, post);

	return CMP_OK;

err_exit:
	return CMP_NG;
}


static int composer_queue(
	void *data,
	int   data_size,
	void  (*callback)(void *user_data, int result),
	void   *user_data,
	struct cmp_getfence *sync_fence)
{
	int i;
	int rc = -1;
	struct composer_rh  *rh   = NULL;
	TRACE_ENTER(FUNC_QUEUE);
	DBGENTER("data:%p data_size:%d callback:%p user_data:%p\n",
		data, data_size, callback, user_data);

	/* update debug level */

	if (rtapi_hungup) {
		printk_err1("graphic system hung-up.\n");
		goto err_exit;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	if (in_early_suspend) {
		printk_dbg2(2, "now early-suspending.\n");
		/* nothing to do */
		goto err_exit;
	}
	if (composer_prohibited_count > 0) {
		printk_dbg2(2, "now prohibited composer.\n");
		composer_prohibited_count--;
		goto err_exit;
	}
#endif

	printk_dbg2(3, "down\n");
	down(&kernel_queue_sem);
	for (i = 0; i < MAX_KERNELREQ; i++) {
		if (kernel_request[i].active == false &&
			kernel_request[i].refmask_disp == 0) {
			printk_dbg2(3, "use request buffer index:%d\n", i);
			rh = &kernel_request[i];
			rh->active = true;
			break;
		}
	}
	up(&kernel_queue_sem);

	if (rh == NULL) {
		printk_err2("no space left to request blending.");
		goto err_exit;
	}

#if _TIM_DBG
	timerecord_reset(rh->timerecord);
	timerecord_set(rh->timerecord, TIMID_BUSYLOCK, ktime_busylock);
	timerecord_record(rh->timerecord, TIMID_QUEUE);
#endif

	if (data_size == sizeof(struct cmp_postdata)) {
		/* handle queue using cmp_postdata*/
		if (handle_queue_data_type1(rh, (struct cmp_postdata *)data)) {
			printk_err1("queue data invalid.\n");
			goto err_exit2;
		}
	} else if (data_size == sizeof(struct cmp_request_queuedata)) {
		/* handle queue using cmp_postdata*/
		if (handle_queue_data_type0(rh,
			(struct cmp_request_queuedata *)data)) {
			printk_err1("queue data invalid.\n");
			goto err_exit2;
		}
	} else {
		printk_err("size of queue not match.\n");
		goto err_exit2;
	}


#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	printk_dbg2(3, "down\n");
	down(&sem_get_syncfd);

	/* clear flag of signal */
	rh->fence_signal_flag = 0;

	/* increase timeline signaled. */
	if (rh->hdmi_data.valid && swsync_handle_hdmi) {
		/* execute output. */
		fence_inc_timeline(swsync_handle_hdmi);
	} else {
		/* increment timeline not necessary. */
		rh->fence_signal_flag_c[FENCE_SIGNAL_OUTPUT] = true;
	}
	if (rh->lcd_data.valid && swsync_handle) {
		/* execute blending. */
		fence_inc_timeline(swsync_handle);
	} else {
		/* increment timeline not necessary. */
		rh->fence_signal_flag_c[FENCE_SIGNAL_BLEND] = true;
	}
#endif

	rh->user_callback  = callback;
	rh->user_data      = user_data;

	/* get resource to access control frame buffer. */
#if FEATURE_FRAMEACCESS
	{
		int fb_access_type = 0;
#if FEATURE_FRAMEACCESS & 2
		if (rh->hdmi_data.valid)
			fb_access_type |= FRAMEACESS_FB_FOR_HDMI;
#endif
#if FEATURE_FRAMEACCESS & 1
		if (rh->lcd_data.valid)
			fb_access_type |= FRAMEACESS_FB_FOR_LCD;
#endif
		decrement_usable_framebuffer(fb_access_type);
	}
#endif

	/**********************************
	 schedule to run.
	**********************************/
	/* queue tasks */
	localwork_flush(workqueue_schedule, &rh->rh_wqtask_schedule);
	localwork_queue(workqueue_schedule, &rh->rh_wqtask_schedule);

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	if (sync_fence) {
		if (sync_fence->release_lcd_fd != -1 ||
			sync_fence->release_hdmi_fd != -1) {
			/* sequence error. */
			printk_err("can not get sync fd.\n");
		} else {
			sync_fence->release_lcd_fd =
				fence_get_syncfd(swsync_handle,
					SYNC_FD_TYPE_BLIT);
			sync_fence->release_hdmi_fd =
				fence_get_syncfd(swsync_handle_hdmi,
					SYNC_FD_TYPE_BLIT);
		}
	}
	up(&sem_get_syncfd);
#endif

	rc = 0;

err_exit2:
	if (rc) {
		/**********************************
		 some error found
		**********************************/
		rh->active = false;
		rh->refmask_disp = 0;
	}
err_exit:
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
	{
		unsigned long flags;
		spin_lock_irqsave(&irqlock, flags);

		queue_data_complete = true;

		spin_unlock_irqrestore(&irqlock, flags);
	}
	wake_up_all(&kernel_waitqueue_queued);
#endif

	if (rc) {
		/* error detected. */
		TRACE_ENTER(FUNC_CALLBACK);
		callback(user_data, 1);
		TRACE_LEAVE(FUNC_CALLBACK);

		printk_err2("request failed.\n");

		/* wake-up waiting thread */
		wake_up_all(&kernel_waitqueue_comp);
	}

	TRACE_LEAVE(FUNC_QUEUE);
	DBGLEAVE("%d\n", rc);
	return rc;
}

static int module_composer_queue(
	void *data,
	int   data_size,
	void  (*callback)(void *user_data, int result),
	void   *user_data)
{
	/* queue without create sync fence. */
	return composer_queue(data, data_size, callback, user_data, NULL);
}

static void process_composer_queue_callback(struct composer_rh *rh)
{
	void   (*user_callback)(void*, int);
	void   *user_data;

	TRACE_ENTER(FUNC_CALLBACK);
	DBGENTER("rh:%p\n", rh);

	user_callback = NULL;
	user_data     = NULL;

	printk_dbg2(3, "down\n");
	down(&kernel_queue_sem);

	rh->refcount--;
	printk_dbg2(3, "refcount:%d\n", rh->refcount);
	if (rh->refcount <= 0) {
		user_callback = rh->user_callback;
		user_data     = rh->user_data;
		rh->user_callback = NULL;

#ifdef CONFIG_SYNC
		/* fail safe, expire all sync object here. */
		fence_expire(rh);
#endif
		/* release file handle */
		free_buffer_handle(rh);

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
		/* fail safe generate signal for sw_sync */
		if (rh->fence_signal_flag_c[FENCE_SIGNAL_BLEND] == 0) {
			printk_dbg2(3, "generate signal for blend\n");
			fence_signal(swsync_handle);
		}
		if (rh->fence_signal_flag_c[FENCE_SIGNAL_OUTPUT] == 0) {
			printk_dbg2(3, "generate signal for output\n");
			fence_signal(swsync_handle_hdmi);
		}
#endif

#if _TIM_DBG
		timerecord_record(rh->timerecord, TIMID_CALLBACK);
#endif

		rh->active = false;

		/* wake-up waiting thread */
		wake_up_all(&kernel_waitqueue_comp);
	}
	up(&kernel_queue_sem);

	if (user_callback) {
		TRACE_LOG(FUNC_CALLBACK);
		user_callback(user_data, 1);
		/* process callback */
	}
	TRACE_LEAVE(FUNC_CALLBACK);
	DBGLEAVE("\n");
}

#if INTERNAL_DEBUG
static void sh_mobile_composer_debug_info_static(struct seq_file *s)
{
	/* log of static variable */
	seq_printf(s, "[semaphore]\n");
	seq_printf(s, "  sem:%d\n", sem.count);
	seq_printf(s, "  kernel_queue_sem:%d\n",
		kernel_queue_sem.count);
	seq_printf(s, "  sem_framebuf_useable:%d\n",
		sem_framebuf_useable.count);
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	seq_printf(s, "  sem_hdmi_framebuf_useable:%d\n",
		sem_hdmi_framebuf_useable.count);
	seq_printf(s, "  sem_hdmimemory:%d\n",
		sem_hdmimemory.count);
#endif
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	seq_printf(s, "  sem_get_syncfd:%d\n",
		sem_get_syncfd.count);
#endif
	seq_printf(s, "[static]\n");
	seq_printf(s, "  num_open:%d\n", num_open);
	seq_printf(s, "  debug:%d\n", debug);
	seq_printf(s, "  rtapi_hungup:%d\n", rtapi_hungup);
	seq_printf(s, "  set_blend_size_flag:%d\n", set_blend_size_flag);
#if FEATURE_FRAMEACCESS
	seq_printf(s, "  available_num_framebuffer: " \
		"lcd[%d] hdmi[%d]\n",
		available_num_framebuffer[0],
		available_num_framebuffer[1]);
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	seq_printf(s, "  in_early_suspend:%d\n", in_early_suspend);
#endif
	seq_printf(s, "  graphic_handle:%p\n", graphic_handle);
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	seq_printf(s, "  graphic_handle_hdmi:%p\n",
		graphic_handle_hdmi);
	seq_printf(s, "  hdmi_output_enable:%d\n", hdmi_output_enable);
	seq_printf(s, "  hdmi_output_enable_mask:%d\n",
		hdmi_output_enable_mask);
#endif
	seq_printf(s, "  fb    addr:0x%lx-0x%lx map_handle:%p\n",
		info_fb.queue_fb_map_address,
		info_fb.queue_fb_map_endaddress,
		info_fb.queue_fb_map_handle);
	seq_printf(s, "        offset:0x%x 0x%x\n" \
		"        yline:0x%x 0x%x\n",
		info_fb.fb_offset_info[0][0],
		info_fb.fb_offset_info[1][0],
		info_fb.fb_offset_info[0][1],
		info_fb.fb_offset_info[1][1]);
	seq_printf(s, "        doublebuffer:%d display_count:%d\n",
		info_fb.fb_double_buffer,
		info_fb.fb_count_display);
	seq_printf(s, "  gpufb addr:0x%lx-0x%lx map_handle:%p\n",
		info_fb.queue_fb_map_address2,
		info_fb.queue_fb_map_endaddress2,
		info_fb.queue_fb_map_handle2);
	seq_printf(s, "        direct_display:%d\n",
		info_fb.direct_display[0]);
	seq_printf(s, "        blend_bufferid:%d\n",
		info_fb.blend_bufferid);
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	seq_printf(s, "  hdmi  memsize:0x%lx\n",
		info_hdmi.allocatesize);
	seq_printf(s, "        memhandle:%p\n",
		info_hdmi.hdmi_map_handle);
	seq_printf(s, "        max width:%d height:%d\n",
		info_hdmi.max_width, info_hdmi.max_height);
	seq_printf(s, "        display_count:%d\n",
		info_hdmi.hdmi_count_display);
	seq_printf(s, "        direct_display:%d\n",
		info_hdmi.direct_display[0]);
	seq_printf(s, "        blend_bufferid:%d\n",
		info_hdmi.blend_bufferid);
#endif
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	seq_printf(s, "  swsync_handle:%p\n", swsync_handle);
	if (swsync_handle) {
		seq_printf(s, "    timeline_count:%d\n",
			swsync_handle->timeline_count);
		seq_printf(s, "    timeline_inc:%d\n",
			swsync_handle->timeline_inc);
	}
	seq_printf(s, "  swsync_handle_hdmi:%p\n", swsync_handle_hdmi);
	if (swsync_handle_hdmi) {
		seq_printf(s, "    timeline_count:%d\n",
			swsync_handle_hdmi->timeline_count);
		seq_printf(s, "    timeline_inc:%d\n",
			swsync_handle_hdmi->timeline_inc);
	}
#endif
	seq_printf(s, "  top_lcd_list: %d top_hdmi_list: %d\n",
		list_empty(&top_lcd_list), list_empty(&top_hdmi_list));
#if FEATURE_SKIP_HDMI || FEATURE_SKIP_LCD
	seq_printf(s, "  display skip count:%d %d [%d %d]\n",
		skip_frame_count[0][0], skip_frame_count[1][0],
		skip_frame_count[0][1], skip_frame_count[1][1]);
#endif
}

#if INTERNAL_DEBUG_USE_DEBUGFS

#define DBGMSG_WORKQUEUE(ARG) { if (ARG) \
		seq_printf(s, "  " #ARG " run:%d priority:%d\n", \
			!list_empty(&ARG->top), ARG->priority); }

#define DBGMSG_WORKTASK(NAME, ARG) { seq_printf(s, "  " #NAME \
	" queue:%d status:%d\n", !list_empty(&ARG.link), ARG.status); }

static void sh_mobile_composer_debug_info_queue(struct seq_file *s)
{
	int i;
	struct composer_rh *rh;

	/* queue info */
	for (i = 0; i < MAX_KERNELREQ; i++) {
		rh = &kernel_request[i];

		seq_printf(s, "[queue-%d]\n", i);

		*internal_log_msg = 0;
		sh_mobile_composer_dump_rhandle(
			internal_log_msg, internal_log_msgsize, rh);

		seq_printf(s, "%s\n", internal_log_msg);
	}

	/* workqueue */

	seq_printf(s, "[workqueue]\n");

	DBGMSG_WORKQUEUE(workqueue);
	DBGMSG_WORKQUEUE(workqueue_schedule);
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI && FEATURE_HDMI_WORKQUEUE
	DBGMSG_WORKQUEUE(workqueue_hdmi);
#endif
#if FEATURE_LCD_WORKQUEUE
	DBGMSG_WORKQUEUE(workqueue_lcd);
#endif
	seq_printf(s, "[worktask]\n");

	for (i = 0; i < MAX_KERNELREQ; i++) {
		seq_printf(s, "  kernel_request[%d]\n", i);

		DBGMSG_WORKTASK(rh_wqtask,
			kernel_request[i].rh_wqtask);
		DBGMSG_WORKTASK(rh_wqtask_disp,
			kernel_request[i].rh_wqtask_disp);
		DBGMSG_WORKTASK(rh_wqtask_hdmi_blend,
			kernel_request[i].rh_wqtask_hdmi_blend);
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
		DBGMSG_WORKTASK(rh_wqtask_hdmi,
			kernel_request[i].rh_wqtask_hdmi);
#endif
		DBGMSG_WORKTASK(rh_wqtask_schedule,
			kernel_request[i].rh_wqtask_schedule);
	}
}
#undef DBGMSG_WORKQUEUE
#undef DBGMSG_WORKTASK

#endif
#endif

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
static void swsync_create_handle(void)
{
	/* get API */
	if (swsync_handle == NULL &&
		swsync_handle_hdmi == NULL) {
		printk_dbg2(3, "get api for sw_sync\n");
		fence_get_api();
	}

	/* create handle */
	if (swsync_handle == NULL) {
		printk_dbg2(3, "create sw_sync for lcd\n");
		swsync_handle = fence_get_handle();
		if (swsync_handle == NULL) {
			/* error report */
			printk_err2("not support sw_sync\n");
		}
	}

	if (swsync_handle_hdmi == NULL) {
		printk_dbg2(3, "create sw_sync for hdmi\n");
		swsync_handle_hdmi = fence_get_handle();
		if (swsync_handle_hdmi == NULL) {
			/* error report */
			printk_err2("not support sw_sync\n");
		}
	}
}
static void swsync_delete_handle(void)
{
	/* delete handle */
	if (swsync_handle) {
		fence_put_handle(swsync_handle);
		swsync_handle = NULL;
	}

	if (swsync_handle_hdmi) {
		fence_put_handle(swsync_handle_hdmi);
		swsync_handle_hdmi = NULL;
	}

	/* release API */
	if (swsync_handle == NULL &&
		swsync_handle_hdmi == NULL) {
		printk_dbg2(3, "release api for sw_sync\n");
		fence_release_api();
	}
}
#endif

/******************************************************/
/* file operation entry function                      */
/******************************************************/
static long core_ioctl(struct file *filep, \
		unsigned int cmd, unsigned long arg)
{
	int rc  = -EINVAL;
	int dir = _IOC_DIR(cmd);
	int sz  = _IOC_SIZE(cmd);
	struct composer_fh *fh;
	int    parg_size;
	void   *parg;

	DBGENTER("filep:%p cmd:0x%x arg:0x%lx\n", filep, cmd, arg);

	fh = (struct composer_fh *)filep->private_data;
	parg = fh->ioctl_args;
	parg_size = CORE_IOCTL_MAX_ARG_LENGTH * 4;

/*********************/
/* Prologue of IOCTL */
/*********************/
	if (sz != 0 && ((dir & (_IOC_WRITE|_IOC_READ)) != 0)) {
		if (sz >= parg_size) {
			printk_err2("ioctl argument size too large\n");
			goto err_exit;
		}
	}

	if (sz != 0 && (dir & _IOC_WRITE) != 0) {
		printk_dbg2(3, "copy_from_user\n");
		if (copy_from_user(parg, (void __user *)arg, sz)) {
			printk_err2("fail in copy_from_user\n");
			goto err_exit;
		}
	}

	printk_dbg2(3, "down\n");
	down(&fh->fh_sem);

	switch (cmd) {
	case CMP_IOC_ISSUSPEND:
		rc = ioc_issuspend(fh);
		break;
	case CMP_IOC_WAITCOMP:
		rc = ioc_waitcomp(fh);
		break;
	case CMP_IOCS_FBADDR:
		rc = ioc_setfbaddr(fh, parg);
		break;
	case CMP_IOCS_BUSYLOCK:
		rc = ioc_busylock(fh, parg);
		break;
	case CMP_IOC_POST:
		rc = ioc_post(fh, parg);
		break;
	case CMP_IOCG_GETFENCE:
		rc = iocg_getfence(fh, parg);
		break;
	case CMP_IOCS_HDMIMEM:
		rc = iocs_hdmimem(fh, parg);
		break;
	default:
		printk_err2("invalid cmd 0x%x\n", cmd);
	}

	up(&fh->fh_sem);

/*********************/
/* Epilogue of IOCTL */
/*********************/
	if (rc == 0 && sz != 0 && (dir & _IOC_READ) != 0) {
		printk_dbg2(3, "copy_to_user\n");
		if (copy_to_user((void __user *)arg, parg, sz)) {
			printk_err2("fail in copy_to_user\n");
			rc = -EINVAL;
		}
	}
err_exit:

	DBGLEAVE("%d\n", rc);
	return rc;
}

static int core_open(struct inode *inode, struct file *filep)
{
	int rc = 0;
	struct composer_fh *private_fh;

	TRACE_ENTER(FUNC_OPEN);
	DBGENTER("inode:%p filep:%p\n", inode, filep);
	printk_dbg2(3, "down\n");
	down(&sem);
	if (num_open >= MAX_OPEN) {
		/* set return code */
		printk_err("reach the upper limit of open\n");
		rc = -ENODEV;
	} else {
		if (num_open == 0) {
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
			/* create sw_sync handle */
			swsync_create_handle();
#endif
			rc = indirect_call(workqueue,
				work_create_handle, 0, NULL);

			if (rc == 0) {
				if (graphic_handle) {
					/* set no error */
					rc = 0;
				} else {
					/* set error */
					rc = -ENOMEM;
					printk_err2("can not create handle\n");
				}
			} else {
				/* set error */
				rc = -ENOMEM;
				printk_err2("can not create handle\n");
			}
		}

		if (rc == 0) {
			/* increment open count */
			num_open++;
		}
	}
	up(&sem);

	if (rc) {
		/* return by error */
		goto err_exit;
	}
	printk_dbg2(3, "current num of opens:%d\n", num_open);

	/* allocate per-filehandle data */
	private_fh = allocate_device();
	if (private_fh == NULL) {

		printk_dbg2(3, "down\n");
		down(&sem);

		num_open--;
		if (num_open == 0) {
			/*************************************/
			/* delete graphics handle            */
			/*************************************/
			rc = indirect_call(workqueue,
				work_delete_handle, 0, NULL);

			if (rc || graphic_handle) {
				/* report error */
				printk_err2("can not delete handle.\n");
			}

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
			/* delete sw_sync handle */
			swsync_delete_handle();
#endif
		}
		up(&sem);

		rc = -ENOMEM;
	} else {
		filep->private_data = private_fh;
	}
err_exit:
	TRACE_LEAVE(FUNC_OPEN);
	DBGLEAVE("%d\n", rc);
	return rc;
}

static int core_release(struct inode *inode, struct file *filep)
{
	struct composer_fh *fh;

	TRACE_ENTER(FUNC_CLOSE);
	DBGENTER("inode:%p filep:%p\n", inode, filep);

	fh = (struct composer_fh *)filep->private_data;

	free_device(fh);

	printk_dbg2(3, "down\n");
	down(&sem);

	num_open--;
	if (num_open == 0) {
		int    rc;

		/* wait display task complete. */
#if FEATURE_LCD_WORKQUEUE
		localworkqueue_flush(VAR_WORKQUEUE_LCD);
#endif
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI && FEATURE_HDMI_WORKQUEUE
		localworkqueue_flush(VAR_WORKQUEUE_HDMI);
#endif

		/*************************************/
		/* delete graphics handle            */
		/*************************************/
		rc = indirect_call(workqueue,
			work_delete_handle, 0, NULL);

		if (rc || graphic_handle) {
			/* report error */
			printk_err2("can not delete handle.\n");
		}
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
		/*************************************/
		/* delete graphics handle            */
		/*************************************/
		rc = indirect_call(VAR_WORKQUEUE_HDMI,
			work_delete_handle_hdmi, 0, NULL);

		if (rc || graphic_handle_hdmi) {
			/* report error */
			printk_err2("can not delete handle.\n");
		}

		/*************************************/
		/* free hdmi memory                  */
		/*************************************/
		rc = indirect_call(VAR_WORKQUEUE_HDMI,
			work_free_memory_hdmi, 0, NULL);

		if (rc || info_hdmi.hdmi_map_handle) {
			/* report error */
			printk_err2("can not free memory.\n");
		}
#endif

		printk_dbg2(3, "release FB related resource "     \
			"fb_info(%p) queue_fb_map_handle(%p)\n", \
			info_fb.fb_info, info_fb.queue_fb_map_handle);
		if (info_fb.queue_fb_map_handle) {
			/* unmap handle */
			if (composer_unset_address(FB_SCREEN_BUFFERID0)) {
				/* only report error */
				printk_err2("error in " \
					"composer_unset_address.\n");
			}
		}
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
		/* delete sw_sync handle */
		swsync_delete_handle();
#endif
	}
	up(&sem);
	printk_dbg2(3, "current num of opens:%d\n", num_open);

	TRACE_LEAVE(FUNC_CLOSE);
	DBGLEAVE("%d\n", 0);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void pm_early_suspend(struct early_suspend *h)
{
	DBGENTER("h:%p\n", h);
	in_early_suspend = true;
	composer_prohibited_count = NUM_OF_COMPOSER_PROHIBITE_AT_RESUME;

	if (rtapi_hungup && graphic_handle) {
		printk_err("not release graphic handle, " \
			"due to RTAPI hung-up\n");
	} else if (graphic_handle) {
		int rc;

		/***************************************
		* release RTAPI resource,
		***************************************/
		printk_dbg2(3, "down\n");
		down(&sem);

		rc = indirect_call(workqueue,
			work_delete_handle, 0, NULL);

		if (rc) {
			/* error */
			printk_err("failed to release graphic handle\n");
		}

		up(&sem);
	} else {
		printk_dbg2(3, "already release graphic handle\n");
		/* nothing to do */
	}

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	printk_dbg2(3, "down\n");
	down(&sem);

	if (rtapi_hungup && graphic_handle_hdmi) {
		printk_err("not release graphic handle, " \
			"due to RTAPI hung-up\n");
	} else if (graphic_handle_hdmi) {
		int rc;

		rc = indirect_call(VAR_WORKQUEUE_HDMI,
			work_delete_handle_hdmi, 0, NULL);

		if (rc) {
			printk_err("failed to release graphic " \
				"handle for hdmi\n");
		}
	}

	up(&sem);
#endif /* SH_MOBILE_COMPOSER_SUPPORT_HDMI*/

	/* confirm complete of request queue. */
	waitcomp_composer(WAITCOMP_COMPOSER_DISP);

#if FEATURE_LCD_WORKQUEUE
	printk_dbg2(3, "wait display task complete\n");
	localworkqueue_flush(VAR_WORKQUEUE_LCD);
#endif
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI && FEATURE_HDMI_WORKQUEUE
	printk_dbg2(3, "wait hdmi output task complete\n");
	localworkqueue_flush(VAR_WORKQUEUE_HDMI);
#endif

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	/* timeline increase to guarantee all sync object be signaled. */
	fence_reset_timeline(swsync_handle);
	fence_reset_timeline(swsync_handle_hdmi);
#endif

	printk_dbg2(3, "suspend state:%d graphic_handle:%p"
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
		"graphic_handle hdmi:%p"
#endif /* SH_MOBILE_COMPOSER_SUPPORT_HDMI*/
		"\n",
		in_early_suspend, graphic_handle
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
		, graphic_handle_hdmi
#endif /* SH_MOBILE_COMPOSER_SUPPORT_HDMI*/
		);
	/* nothing to do */
	DBGLEAVE("\n");
	return;
}

static void pm_late_resume(struct early_suspend *h)
{
	int need_graphic_handle;
	DBGENTER("h:%p\n", h);
	down(&sem);

	need_graphic_handle = 0;
	if (num_open > 0 && graphic_handle == NULL) {
		/* need_graphic_handle */
		need_graphic_handle = 1;
	}

	if (need_graphic_handle == 0) {
		/* not need create handle */
		/* nothing to do */
	} else if (rtapi_hungup) {
		printk_err("not create graphic handle, " \
			"due to RTAPI hung-up\n");
	} else if (graphic_handle == NULL) {
		int rc;

		rc = indirect_call(workqueue,
			work_create_handle, 0, NULL);

		if (rc) {
			/* report error */
			printk_err("failed to create graphic handle\n");
		}
	} else {
		printk_dbg2(3, "already create graphic handle\n");
		/* nothing to do */
	}
	in_early_suspend = false;
	up(&sem);
	printk_dbg2(3, "suspend state:%d graphic_handle:%p\n",
		in_early_suspend, graphic_handle);
	DBGLEAVE("\n");
	return;
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

/******************************************************/
/* module initialize function                         */
/******************************************************/
static int __init sh_mobile_composer_init(void)
{
	int ret = 0;
	int i;

	DBGENTER("\n");

	/* register entry */
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
	sh_mobile_composer_entry.composer_phy_change_rtaddr =
		module_composer_phy_change_rtaddr;
	sh_mobile_composer_entry.composer_register_gpu_buffer =
		module_composer_register_gpu_buffer;
	sh_mobile_composer_entry.composer_queue =
		module_composer_queue;
#endif
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	sh_mobile_composer_entry.composer_hdmiset =
		module_composer_hdmiset;
#endif

	/* initialize globals */
	if (debug >= 3) {
		/* set app share library. to debug mode */
		sh_mobile_appmem_debugmode(1);
	}

	/*****************************/
	/* initialize driver static  */
	/*****************************/
	spin_lock_init(&irqlock);
	sema_init(&sem, 1);
	num_open = 0;
	graphic_handle = NULL;
	set_blend_size_flag = false;

	spin_lock_init(&irqlock_list);

	/* clear information of fb mapping.  */
	memset(&info_fb, 0, sizeof(info_fb));

	/*****************************/
	/* initialize request queue. */
	/*****************************/

	memset(&kernel_request, 0, sizeof(kernel_request));
	for (i = 0; i < MAX_KERNELREQ; i++) {
		struct composer_rh *rh =  &kernel_request[i];
		localwork_init(&rh->rh_wqtask, work_blend);
		localwork_init(&rh->rh_wqtask_disp, work_dispdraw);

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
		localwork_init(&rh->rh_wqtask_hdmi_blend, work_blend_hdmi);
		localwork_init(&rh->rh_wqtask_hdmi, work_dispdraw_hdmi);
		localwork_init(&rh->rh_wqtask_hdmi_comp,
			work_dispdraw_hdmicomp);
#endif

		localwork_init(&rh->rh_wqtask_schedule, work_schedule);

		initialize_blendwait_obj(&rh->rh_wqwait);
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
		initialize_blendwait_obj(&rh->rh_wqwait_hdmi);
#endif
#if _TIM_DBG
		rh->timerecord = timerecord_createhandle();
#endif
		INIT_LIST_HEAD(&rh->lcd_list);
		INIT_LIST_HEAD(&rh->hdmi_list);
		INIT_LIST_HEAD(&rh->hdmi_wait_list);
	}
	INIT_LIST_HEAD(&top_lcd_list);
	INIT_LIST_HEAD(&top_hdmi_list);

	sema_init(&kernel_queue_sem, 1);
	init_waitqueue_head(&kernel_waitqueue_comp);

#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
	init_waitqueue_head(&kernel_waitqueue_queued);
#endif

#if FEATURE_FRAMEACCESS
	spin_lock_init(&irqlock_framebuffer);
#if FEATURE_FRAMEACCESS & 1
	available_num_framebuffer[0] = NUM_OF_ANDROID_USABLE_FB;
#endif
#if FEATURE_FRAMEACCESS & 2
	available_num_framebuffer[1] = NUM_OF_ANDROID_USABLE_HDMI_FB;
#endif
	init_waitqueue_head(&wait_framebuffer_available);
#endif

	sema_init(&sem_framebuf_useable, NUM_OF_FRAMEBUFFER_MAXIMUM);

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	sema_init(&sem_hdmi_framebuf_useable, NUM_OF_HDMI_FRAMEBUFFER_MAXIMUM);
	hdmi_output_enable = true;
	hdmi_output_enable_mask = 0;
	graphic_handle_hdmi = NULL;
#endif /* SH_MOBILE_COMPOSER_SUPPORT_HDMI*/

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	sema_init(&sem_get_syncfd, 1);
	swsync_handle = NULL;
	swsync_handle_hdmi = NULL;
#endif

	/* initialize function call */
	indirect_call_init();

#if INTERNAL_DEBUG
	sh_mobile_composer_tracelog_init();

	internal_log_msg = kmalloc(INTERNAL_LOG_MSG_SIZE, GFP_KERNEL);
	if (internal_log_msg) {
		/* record available memory size */
		internal_log_msgsize = INTERNAL_LOG_MSG_SIZE;
	}
#endif

	/* Linux standard workqueue can not be used,
	   because RT-API requires a single thread where PID never changed. */

	/* create workqueue */
	workqueue = localworkqueue_create("sh_mobile_cmp", 0);
	if (workqueue == NULL) {
		printk_err("fail to workqueue_create");
		ret = -ENOMEM;
		goto err_exit;
	}

	/* create workqueue */
	workqueue_schedule = localworkqueue_create("sh_mobile_cmpsc", 2);
	if (workqueue_schedule == NULL) {
		printk_err("fail to workqueue_create");
		ret = -ENOMEM;
		goto err_exit;
	}

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI && FEATURE_HDMI_WORKQUEUE
	/* create workqueue */
	workqueue_hdmi = localworkqueue_create("sh_mobile_cmphd", 1);
	if (workqueue_hdmi == NULL) {
		printk_err("fail to workqueue_create");
		ret = -ENOMEM;
		goto err_exit;
	}
#endif

#if FEATURE_LCD_WORKQUEUE
	/* create workqueue */
	workqueue_lcd = localworkqueue_create("sh_mobile_cmpfb", 1);
	if (workqueue_lcd == NULL) {
		printk_err("fail to workqueue_create");
		ret = -ENOMEM;
		goto err_exit;
	}
#endif

	/* register device */
	ret = misc_register(&composer_device);
	if (ret) {
		printk_err("fail to misc_register (MISC_DYNAMIC_MINOR)\n");
		goto err_exit;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	in_early_suspend = false;
	register_early_suspend(&early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */

	DBGLEAVE("%d\n", 0);
	return 0;

err_exit:
	if (workqueue) {
		localworkqueue_destroy(workqueue);
		workqueue = NULL;
	}
	if (workqueue_schedule) {
		localworkqueue_destroy(workqueue_schedule);
		workqueue_schedule = NULL;
	}
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI && FEATURE_HDMI_WORKQUEUE
	if (workqueue_hdmi) {
		localworkqueue_destroy(workqueue_hdmi);
		workqueue_hdmi = NULL;
	}
#endif
#if FEATURE_LCD_WORKQUEUE
	if (workqueue_lcd) {
		localworkqueue_destroy(workqueue_lcd);
		workqueue_lcd = NULL;
	}
#endif
	DBGLEAVE("%d\n", ret);
	return ret;
}

static void __exit sh_mobile_composer_release(void)
{
	DBGENTER("\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */

	if (num_open > 0)
		printk_err("there is 'not close device'.\n");

#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	/* delete sw_sync handle */
	swsync_delete_handle();
#endif
	/* unregist device */
	misc_deregister(&composer_device);

	/* release all resources */
	/* destroy workqueue */
	if (workqueue) {
		localworkqueue_destroy(workqueue);
		workqueue = NULL;
	}
	if (workqueue_schedule) {
		localworkqueue_destroy(workqueue_schedule);
		workqueue_schedule = NULL;
	}
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI && FEATURE_HDMI_WORKQUEUE
	if (workqueue_hdmi) {
		localworkqueue_destroy(workqueue_hdmi);
		workqueue_hdmi = NULL;
	}
#endif
#if FEATURE_LCD_WORKQUEUE
	if (workqueue_lcd) {
		localworkqueue_destroy(workqueue_lcd);
		workqueue_lcd = NULL;
	}
#endif

#if INTERNAL_DEBUG >= 1
	if (internal_log_msg) {
		/* free memory */
		kfree(internal_log_msg);
		internal_log_msgsize = 0;
	}
#endif

	if (info_fb.queue_fb_map_handle) {
		/* unmap handle */
		sh_mobile_rtmem_physarea_unregister(
			info_fb.queue_fb_map_handle);
		info_fb.queue_fb_map_handle = NULL;
	}
	if (info_fb.queue_fb_map_handle2) {
		/* unmap handle */
		sh_mobile_rtmem_physarea_unregister(
			info_fb.queue_fb_map_handle2);
		info_fb.queue_fb_map_handle2 = NULL;
	}

	/* unregister entry */
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
	sh_mobile_composer_entry.composer_phy_change_rtaddr = NULL;
	sh_mobile_composer_entry.composer_register_gpu_buffer = NULL;
	sh_mobile_composer_entry.composer_queue = NULL;
#endif
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	sh_mobile_composer_entry.composer_hdmiset = NULL;
#endif

	DBGLEAVE("\n");
	return;
}

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "SHMobile Composer debug level");
MODULE_LICENSE("GPL");
module_init(sh_mobile_composer_init);
module_exit(sh_mobile_composer_release);
