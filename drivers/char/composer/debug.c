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

#include <linux/kernel.h>
#include <linux/spinlock.h>

#include "linux/sh_mobile_composer.h"
#include "sh_mobile_debug.h"

#if _TIM_DBG
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched.h>
#endif

#if INTERNAL_DEBUG
#if INTERNAL_DEBUG_USE_DEBUGFS
#include <linux/debugfs.h>
#endif
#include <linux/seq_file.h>
#endif

/******************************************************/
/* define prototype                                   */
/******************************************************/

/******************************************************/
/* define local define                                */
/******************************************************/
#if INTERNAL_DEBUG
#define TRACELOG_SIZE         128
#endif

/* define for tracelog_record function. */
#define TRACELOG_RECORD_VALUE0_SHIFT_TO_LOGCLASS  24

#define DBGMSG_APPEND(FMT, ARG...) \
{ c = snprintf(p, n, FMT, ##ARG); if (c < n) { p += c; n -= c; } }

/******************************************************/
/* define local variables                             */
/******************************************************/
#if INTERNAL_DEBUG
static int         init_flag;
static int         dump_information_flag;
static char        dump_information_buf[6 * 1024];

static int         log_tracebuf[TRACELOG_SIZE][3];
static spinlock_t  log_irqlock;
static int         log_tracebuf_wp;
#endif

#if _TIM_DBG
struct record_time_info {
	int use_slot;
	ktime_t time[16];
};

#ifdef CONFIG_DEBUG_FS
#define MAX_TIMERECORD_DATA   8
static DECLARE_WAIT_QUEUE_HEAD(timerecord_waitdata);
static int                     timerecord_rp;
static int                     timerecord_wp;
static int                     timerecord_count;
static struct record_time_info timerecord_data[MAX_TIMERECORD_DATA];
static int                     timerecord_opencount;
static spinlock_t              timerecord_lock;
#endif
#endif

/******************************************************/
/* define global variables                            */
/******************************************************/

/******************************************************/
/* local functions                                    */
/******************************************************/
#if INTERNAL_DEBUG
/********************
 initialize
********************/
static void sh_mobile_composer_tracelog_init(void)
{
	/* reset debug level */
	log_tracebuf_wp = 0;
	spin_lock_init(&log_irqlock);

	init_flag = 1;
}

/********************
 record
********************/
void sh_mobile_composer_tracelog_record(int logclass, int line, int ID, int val)
{
	unsigned long flags;

	/* check initialized */
	if (!init_flag)
		return;

	/* record information */
	spin_lock_irqsave(&log_irqlock, flags);
	log_tracebuf[log_tracebuf_wp][0] =
		(logclass<<TRACELOG_RECORD_VALUE0_SHIFT_TO_LOGCLASS) |
		(line);
	log_tracebuf[log_tracebuf_wp][1] = ID;
	log_tracebuf[log_tracebuf_wp][2] = val;
	log_tracebuf_wp = (log_tracebuf_wp+1) & (TRACELOG_SIZE-1);
	spin_unlock_irqrestore(&log_irqlock, flags);
}
#endif

/*******************************
 RT-API debug log for graphics
*******************************/
static void dump_screen_grap_initialize(screen_grap_initialize *arg)
{
	printk_lowdbg("  handle:%p mode:%ld\n", arg->handle, arg->mode);
}

/* maximum message length is 210 character */
static int raw_dump_screen_grap_image_param(
	char *p, int n, screen_grap_image_param *arg, char *name)
{
	int c;
	char *org_p = p;

	DBGMSG_APPEND("[%6s]", name);
	DBGMSG_APPEND(" width:%4d height:%4d stride:%4d stride_c:%4d " \
		"format:%2d yuv_format:%1d yuv_range:%1d " \
		"address:(%p, %p) address_c0(%p, %p) address_c1(%p, %p)",
		arg->width, arg->height, arg->stride, arg->stride_c,
		arg->format, arg->yuv_format, arg->yuv_range,
		arg->address, arg->apmem_handle,
		arg->address_c0, arg->apmem_handle_c0,
		arg->address_c1, arg->apmem_handle_c1);
	return p - org_p;
}

/* maximum message length is 350 character */
static int raw_dump_screen_grap_layer(
	char *p, int n, screen_grap_layer *arg, char *name)
{
	int c;
	char *org_p = p;

	if (arg == NULL) {
		/* layer not opened */
		return 0;
	}

	/* maximum message length is 210 character */
	c = raw_dump_screen_grap_image_param(p, n, &arg->image, name);
	if (c >= 0) {
		p += c;
		n -= c;
	}
	/* maximum message length is 140 character */
	DBGMSG_APPEND(" rect(x:%4d y:%4d width:%4d height:%4d) " \
		"alpha:%3d rotate:%1d mirror:%1d key_color:0x%8lx " \
		"premultiplied:%1d alpha_coef:%1d",
		arg->rect.x, arg->rect.y, arg->rect.width, arg->rect.height,
		arg->alpha, arg->rotate, arg->mirror, arg->key_color,
		arg->premultiplied, arg->alpha_coef);
	return p - org_p;
}

/* maximum message length is 370 character */
static int raw_dump_screen_grap_image_blend(char *p, int n,
	screen_grap_image_blend *arg)
{
	int c;
	char *org_p = p;
	/* maximum message length is 160 character */
	DBGMSG_APPEND("handle:%p input_layer:(%p %p %p %p) " \
		"background_color:0x%08lx user_data:0x%08lx\n",
		arg->handle, arg->input_layer[0], arg->input_layer[1],
		arg->input_layer[2], arg->input_layer[3],
		arg->background_color, arg->user_data);

	/* maximum message length is 210 character */
	DBGMSG_APPEND("    ");
	c = raw_dump_screen_grap_image_param(p, n,
		&arg->output_image, "output");
	if (c > 0) {
		p += c;
		n -= c;
	}
	return p - org_p;
}

/* maximum message length is 80 character */
static int raw_dump_screen_grap_set_blend_size(
	char *p, int n, screen_grap_set_blend_size *arg)
{
	int c;
	char *org_p = p;

	DBGMSG_APPEND(" handle:%p lcd width:%4d height:%4d " \
		"hdmi width:%4d height:%4d",
		arg->handle, arg->lcd_width, arg->lcd_height,
		arg->hdmi_width, arg->hdmi_height);
	return p - org_p;
}

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI || INTERNAL_DEBUG
	/* maximum message length is 270 character */
static int raw_dump_screen_grap_image_output(char *p, int n,
	screen_grap_image_output *arg)
{
	int c;
	char *org_p = p;

	/* maximum message length is 60 character */
	DBGMSG_APPEND("handle:%p rotate:%d user_data:0x%08lx\n",
		arg->handle, arg->rotate, arg->user_data);

	/* maximum message length is 210 character */
	DBGMSG_APPEND("    ");
	c = raw_dump_screen_grap_image_param(p, n,
		&arg->output_image, "output");
	if (c > 0) {
		p += c;
		n -= c;
	}
	return p - org_p;
}
#endif

static void dump_screen_grap_image_blend(screen_grap_image_blend *arg)
{
	static char msg[370];

	raw_dump_screen_grap_image_blend(&msg[0], sizeof(msg), arg);
	printk_lowdbg("    %s\n", &msg[0]);

	if (raw_dump_screen_grap_layer(&msg[0], sizeof(msg),
		arg->input_layer[0], "layer0")) {
		/* report log */
		printk_lowdbg("    %s\n", &msg[0]);
	}
	if (raw_dump_screen_grap_layer(&msg[0], sizeof(msg),
		arg->input_layer[1], "layer1")) {
		/* report log */
		printk_lowdbg("    %s\n", &msg[0]);
	}
	if (raw_dump_screen_grap_layer(&msg[0], sizeof(msg),
		arg->input_layer[2], "layer2")) {
		/* report log */
		printk_lowdbg("    %s\n", &msg[0]);
	}
	if (raw_dump_screen_grap_layer(&msg[0], sizeof(msg),
		arg->input_layer[3], "layer3")) {
		/* report log */
		printk_lowdbg("    %s\n", &msg[0]);
	}
}

static void dump_screen_grap_set_blend_size(screen_grap_set_blend_size *arg)
{
	static char msg[80];

	raw_dump_screen_grap_set_blend_size(&msg[0], sizeof(msg), arg);

	printk_lowdbg("    %s\n", &msg[0]);
}

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
static void dump_screen_grap_image_output(screen_grap_image_output *arg)
{
	static char msg[270];

	raw_dump_screen_grap_image_output(&msg[0], sizeof(msg), arg);
	printk_lowdbg("    %s\n", &msg[0]);
}
#endif

static void dump_screen_grap_quit(screen_grap_quit *arg)
{
	printk_lowdbg("  handle:%p mode:%ld\n", arg->handle, arg->mode);
}

static void dump_screen_grap_delete(screen_grap_delete *arg)
{
	printk_lowdbg("  handle:%p\n", arg->handle);
}

static const char *get_RTAPImsg_graphics(int rc)
{
	const char *msg = "unknown RT-API error";
	switch (rc) {
	case SMAP_LIB_GRAPHICS_OK:
		msg = "SMAP_LIB_GRAPHICS_OK";
		break;
	case SMAP_LIB_GRAPHICS_NG:
		msg = "SMAP_LIB_GRAPHICS_NG";
		break;
	case SMAP_LIB_GRAPHICS_PARAERR:
		msg = "SMAP_LIB_GRAPHICS_PARAERR";
		break;
	case SMAP_LIB_GRAPHICS_SEQERR:
		msg = "SMAP_LIB_GRAPHICS_SEQERR";
		break;
	case SMAP_LIB_GRAPHICS_MEMERR:
		msg = "SMAP_LIB_GRAPHICS_MEMERR";
		break;
	case SMAP_LIB_GRAPHICS_INUSE:
		msg = "SMAP_LIB_GRAPHICS_INUSE";
		break;
	}
	return msg;
}

#if INTERNAL_DEBUG
#if INTERNAL_DEBUG_USE_DEBUGFS
static int sh_mobile_composer_dump_rhandle(char *p, int n,
	struct composer_rh *rh)
{
	char *p_org = p;
	int           i, c;

	if (!rh->rh_wqwait.status &&
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
		!rh->rh_wqwait_hdmi.status &&
#endif
		!rh->active) {
		DBGMSG_APPEND("  not used\n");
		goto finish;
	}

	/* variable */
	DBGMSG_APPEND("  rh_wqwait.status:%d\n",
		rh->rh_wqwait.status)
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	DBGMSG_APPEND("  rh_wqwait_hdmi.status:%d\n",
		rh->rh_wqwait_hdmi.status)
#endif
	DBGMSG_APPEND("  active:%d\n", rh->active)
	DBGMSG_APPEND("  ref_count:%d\n", rh->refcount)
	DBGMSG_APPEND("  refmask_disp:%d\n", rh->refmask_disp)
	DBGMSG_APPEND("  user_data:%p user_callback:%p\n",
		rh->user_data, rh->user_callback)
	DBGMSG_APPEND("  num_buffer:%d\n", rh->num_buffer);
	for (i = 0; i < CMP_DATA_NUM_GRAP_LAYER * \
		CMP_DATA_CHANNEL; i++) {
		DBGMSG_APPEND("  buf%d buffer_address:0x%lx",
			i, rh->buffer_address[i]);
#ifdef CONFIG_SYNC
		DBGMSG_APPEND(" buffer_sync:%p", rh->buffer_sync[i]);
#endif
		DBGMSG_APPEND("\n");
	}
#if SH_MOBILE_COMPOSER_USE_SW_SYNC_API
	DBGMSG_APPEND("  fence_signal_flag:0x%x\n", rh->fence_signal_flag);
#endif
	DBGMSG_APPEND("  lcd_list:%d\n", list_empty(&rh->lcd_list));
	DBGMSG_APPEND("  hdmi_list:%d\n", list_empty(&rh->hdmi_list));

	/* blend parameter */
	DBGMSG_APPEND("  lcd_data\n")
	{
		struct cmp_data_compose_lcd *lcd_data;

		lcd_data = &rh->lcd_data;
		DBGMSG_APPEND("    valid:%d display:%d need_blend:%d\n",
			lcd_data->valid, lcd_data->display,
			lcd_data->need_blend);
		DBGMSG_APPEND("    ");
		c = raw_dump_screen_grap_image_blend(p, n, &lcd_data->blend);
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n    ");
		c = raw_dump_screen_grap_layer(p, n,
			&lcd_data->layer[0], "layer0");
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n    ");
		c = raw_dump_screen_grap_layer(p, n,
			&lcd_data->layer[1], "layer1");
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n    ");
		c = raw_dump_screen_grap_layer(p, n,
			&lcd_data->layer[2], "layer2");
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n    ");
		c = raw_dump_screen_grap_layer(p, n,
			&lcd_data->layer[3], "layer3");
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n");
	}

	/* output parameter */
	DBGMSG_APPEND("  hdmi_data\n")
	{
		struct cmp_data_compose_hdmi *hdmi_data;

		hdmi_data = &rh->hdmi_data;
		DBGMSG_APPEND("    valid:%d display:%d need_blend:%d\n",
			hdmi_data->valid, hdmi_data->display,
			hdmi_data->display);
		DBGMSG_APPEND("    ");
		c = raw_dump_screen_grap_image_output(p, n, &hdmi_data->output);
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n    ");
		c = raw_dump_screen_grap_image_blend(p, n, &hdmi_data->blend);
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n    ");
		c = raw_dump_screen_grap_layer(p, n,
			&hdmi_data->layer[0], "layer0");
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n    ");
		c = raw_dump_screen_grap_layer(p, n,
			&hdmi_data->layer[1], "layer1");
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n    ");
		c = raw_dump_screen_grap_layer(p, n,
			&hdmi_data->layer[2], "layer2");
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n    ");
		c = raw_dump_screen_grap_layer(p, n,
			&hdmi_data->layer[3], "layer3");
		if (c > 0) {
			p += c;
			n -= c;
		}
		DBGMSG_APPEND("\n");
	}
finish:;
	return p - p_org;
}
#endif
#endif

/*******************************
 RT-API debug log for display
*******************************/
static void dump_screen_disp_set_address(screen_disp_set_address *arg)
{
	printk_lowdbg("  handle:%p output_mode:%d address:0x%x size:0x%x\n",
		arg->handle,  arg->output_mode, arg->address, arg->size);
}

static const char *get_RTAPImsg_display(int rc)
{
	const char *msg = "unknown RT-API error";
	switch (rc) {
	case SMAP_LIB_DISPLAY_OK:
		msg = "SMAP_LIB_DISPLAY_OK";
		break;
	case SMAP_LIB_DISPLAY_NG:
		msg = "SMAP_LIB_DISPLAY_NG";
		break;
	case SMAP_LIB_DISPLAY_PARAERR:
		msg = "SMAP_LIB_DISPLAY_PARAERR";
		break;
	case SMAP_LIB_DISPLAY_SEQERR:
		msg = "SMAP_LIB_DISPLAY_SEQERR";
		break;
	}
	return msg;
}

/*******************************
 RT-API debug log for memory
*******************************/
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
static void dump_system_mem_phy_change_rtaddr(
	system_mem_phy_change_rtaddr *arg)
{
	printk_lowdbg("  handle:%p phys_addr:0x%x\n",
		arg->handle, arg->phys_addr);
}

static const char *get_RTAPImsg_memory(int rc)
{
	const char *msg = "unknown RT-API error";
	switch (rc) {
	case SMAP_LIB_MEMORY_OK:
		msg = "SMAP_LIB_MEMORY_OK";
		break;
	case SMAP_LIB_MEMORY_NG:
		msg = "SMAP_LIB_MEMORY_NG";
		break;
	case SMAP_LIB_MEMORY_PARA_NG:
		msg = "SMAP_LIB_MEMORY_PARA_NG";
		break;
	case SMAP_LIB_MEMORY_NO_MEMORY:
		msg = "SMAP_LIB_MEMORY_NO_MEMORY";
		break;
	}
	return msg;
}
#endif

#if _TIM_DBG
static void *timerecord_createhandle(void)
{
	struct record_time_info *handle;

	handle = kmalloc(sizeof(struct record_time_info), GFP_KERNEL);
	if (handle) {
		/* initialize memory */
		memset(handle, 0, sizeof(*handle));
	}
	return handle;
}

static void timerecord_reset(void *arg)
{
	if (arg) {
		struct record_time_info *handle;

		handle = (struct record_time_info *)arg;
		handle->use_slot = 0;
	}
}

static unsigned int timerecord_record(void *arg, int id)
{
	unsigned int rc = 0;

	if (id < 0 || id >= 16) {
		printk_err("slot number %d invalid\n", id);
		return 0;
	}

	if (arg) {
		struct record_time_info *handle;

		handle = (struct record_time_info *)arg;

#ifdef CONFIG_DEBUG_FS
		spin_lock(&timerecord_lock);
#endif
		if (handle->use_slot & (1<<id)) {
			/* error report */
			printk_err("slot number %d overwrite\n", id);
		}
		handle->use_slot |= (1<<id);
#ifdef CONFIG_DEBUG_FS
		spin_unlock(&timerecord_lock);
#endif

		handle->time[id] = ktime_get();

		rc = ktime_to_ns(handle->time[id]);
	}
	return rc;
}

static void timerecord_set(void *arg, int id, ktime_t time)
{
	if (id < 0 || id >= 16) {
		printk_err("slot number %d invalid\n", id);
		return;
	}

	if (arg) {
		struct record_time_info *handle;

		handle = (struct record_time_info *)arg;

#ifdef CONFIG_DEBUG_FS
		spin_lock(&timerecord_lock);
#endif
		if (handle->use_slot & (1<<id)) {
			/* error report */
			printk_err("slot number %d overwrite\n", id);
		}
		handle->use_slot |= (1<<id);
#ifdef CONFIG_DEBUG_FS
		spin_unlock(&timerecord_lock);
#endif

		handle->time[id] = time;
	}
	return;
}

static int timerecord_createmessage(struct record_time_info *handle,
	char *p, int n)
{
	int i;
	int c;
	char *org_p = p;

	unsigned long long start_time;

	for (i = 0; i < 16; i++) {
		if (handle->use_slot & (1<<i)) {
			/* get time */
			start_time = ktime_to_ns(handle->time[i]);
			break;
		}
	}

	if (i == 16) {
		c = snprintf(p, n, "no record\n");
		return c;
	}

	c = snprintf(p, n, "%d:%lld ", i, start_time);
	if (c < n) {
		p += c;
		n -= c;
	}
	i++;

	for (; i < 16; i++) {
		if (handle->use_slot & (1<<i)) {
			c = snprintf(p, n, "%d:%lld ", i,
				ktime_to_ns(handle->time[i]) - start_time);
			if (c < n) {
				p += c;
				n -= c;
			} else {
				printk_err("no space\n");
				break;
			}
		}
	}
	c = snprintf(p, n, "\n");
	if (c < n) {
		p += c;
		n -= c;
	}
	return p - org_p;
}

static void timerecord_print(void *arg)
{
	struct record_time_info *handle = (struct record_time_info *)arg;
	if (arg) {
#ifdef CONFIG_DEBUG_FS
		if (!timerecord_opencount) {
			/* nothing to do */
			/* reader of timerecord is not opened. */
		} else if (timerecord_count >= MAX_TIMERECORD_DATA) {
			/* error report */
			printk_err("no space to recordtime\n");
		} else {
			/* record information */
			timerecord_data[timerecord_wp] = *handle;
			timerecord_wp = (timerecord_wp + 1) &
				(MAX_TIMERECORD_DATA - 1);
			timerecord_count++;
			wake_up_interruptible(&timerecord_waitdata);
		}
#else
		char msg[512];
		timerecord_createmessage(handle, &msg[0], sizeof(msg)-1);
		printk(KERN_INFO "%s\n" , &msg[0]);
#endif
	}
}

#ifdef CONFIG_DEBUG_FS
static ssize_t timerecord_read(struct file *filp, char __user *buf, \
		size_t sz, loff_t *off)
{
	char msg[512];
	int c = 0;

	if (timerecord_count == 0) {
		wait_event_interruptible(timerecord_waitdata,
			timerecord_rp != timerecord_wp || \
			timerecord_count == MAX_TIMERECORD_DATA);
		if (signal_pending(current))
			flush_signals(current);
	}

	if (timerecord_count) {
		struct record_time_info *handle;

		handle = &timerecord_data[timerecord_rp];
		timerecord_rp = (timerecord_rp + 1) &
			(MAX_TIMERECORD_DATA - 1);

		c = timerecord_createmessage(handle, &msg[0], sizeof(msg)-1);

		timerecord_count--;

		if (copy_to_user(buf, &msg[0], c)) {
			/* error in copy to user space. */
			c = 0;
		}
	}
	return c;
}

static int timerecord_open(struct inode *inode, struct file *filep)
{
	/* nothing to do */
	timerecord_opencount++;
	return 0;
}
static int timerecord_release(struct inode *inode, struct file *filep)
{
	/* nothing to do */
	timerecord_opencount--;
	return 0;
}

static const struct file_operations timerecord_debugfs_fops = {
	.open           = timerecord_open,
	.read           = timerecord_read,
	.release        = timerecord_release,
};

static __init int timerecord_debugfs_init(void)
{
	spin_lock_init(&timerecord_lock);
	debugfs_create_file("composer_processtime", S_IRUGO,
		NULL, NULL, &timerecord_debugfs_fops);
	return 0;
}
late_initcall(timerecord_debugfs_init);
#endif
#endif


#if INTERNAL_DEBUG
static int internal_debug_static_show(struct seq_file *s, void *unused)
{
	sh_mobile_composer_debug_info_static(s);
	return 0;
}

#if INTERNAL_DEBUG_USE_DEBUGFS
static int internal_debug_static_open(struct inode *inode, struct file *file)
{
	return single_open(file, internal_debug_static_show, inode->i_private);
}

static int internal_debug_queue_show(struct seq_file *s, void *unused)
{
	sh_mobile_composer_debug_info_queue(s);
	return 0;
}

static int internal_debug_queue_open(struct inode *inode, struct file *file)
{
	return single_open(file, internal_debug_queue_show, inode->i_private);
}
#endif

static int internal_debug_trace_show(struct seq_file *s, void *unused)
{
	int i, rp;
	unsigned long flags;
	int    (*tracelog)[TRACELOG_SIZE][3];

	/* check initialized */
	if (!init_flag)
		return 0;

	tracelog = (int (*)[TRACELOG_SIZE][3]) kmalloc(sizeof(log_tracebuf),
		GFP_KERNEL);
	if (!tracelog) {
		printk_err("error at kmalloc\n");
		return 0;
	}

	/* create copy of traces */
	spin_lock_irqsave(&log_irqlock, flags);

	memcpy(tracelog, &log_tracebuf[0][0], sizeof(*tracelog));
	rp = log_tracebuf_wp & (TRACELOG_SIZE-1);

	spin_unlock_irqrestore(&log_irqlock, flags);

	/* create tracelog message */
	for (i = 0; i < TRACELOG_SIZE; i++) {
		int logclass = (*tracelog)[rp][0]>>
			TRACELOG_RECORD_VALUE0_SHIFT_TO_LOGCLASS;
		int logline  = (*tracelog)[rp][0] & 0xffffff;

		if (i != 0 && (i & 0x1f) == 0) {
			/* insert separater */
			seq_printf(s, "\n");
		}

		switch (logclass) {
		case ID_TRACE_ENTER:
			seq_printf(s, "[0x%03x:ent:%d]",
				(*tracelog)[rp][1], logline);
			break;
		case ID_TRACE_LEAVE:
			seq_printf(s, "[0x%03x:lev:%d]",
				(*tracelog)[rp][1], logline);
			break;
		case ID_TRACE_LOG:
			seq_printf(s, "[0x%03x:%d]",
				(*tracelog)[rp][1], logline);
			break;
		case ID_TRACE_LOG1:
			seq_printf(s, "[0x%03x:%d:%d]",
				(*tracelog)[rp][1], logline,
				(*tracelog)[rp][2]);
			break;
		default:
			printk_err2("logclass not defined.");
			/* no log message */
			break;
		}
		rp = (rp+1) & (TRACELOG_SIZE-1);
	}

	kfree(tracelog);
	return 0;
}

#if INTERNAL_DEBUG_USE_DEBUGFS
static int internal_debug_trace_open(struct inode *inode, struct file *file)
{
	return single_open(file, internal_debug_trace_show, inode->i_private);
}

static const struct file_operations internal_debug_static_debugfs_fops = {
	.open           = internal_debug_static_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static const struct file_operations internal_debug_queue_debugfs_fops = {
	.open           = internal_debug_queue_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static const struct file_operations internal_debug_trace_debugfs_fops = {
	.open           = internal_debug_trace_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static __init int internal_debug_init(void)
{
	debugfs_create_file("composer_static", S_IRUGO, NULL, NULL,
		&internal_debug_static_debugfs_fops);
	debugfs_create_file("composer_queue", S_IRUGO, NULL, NULL,
		&internal_debug_queue_debugfs_fops);
	debugfs_create_file("composer_trace", S_IRUGO, NULL, NULL,
		&internal_debug_trace_debugfs_fops);
	return 0;
}
late_initcall(internal_debug_init);
#endif

static void sh_mobile_composer_dump_information(int flag)
{
	if (flag && dump_information_flag) {
		/* avoid multiply generate debug log. */
		printk_err2("already output ignored.");
	} else if (!flag) {
		/* clear output flag. */
		dump_information_flag = false;
	} else if (flag) {
		struct seq_file s = {
			.buf = dump_information_buf,
			.size = sizeof(dump_information_buf) - 1,
		};
		char *p, *next_p;
		int n;

		dump_information_flag = true;

		internal_debug_static_show(&s, NULL);
		internal_debug_trace_show(&s, NULL);

		p = s.buf;
		n = s.count;

		while (p && n > 0) {
			next_p = strnchr(p, n, '\n');
			if (next_p == NULL) {
				/* last log */
				printk(KERN_INFO DEV_NAME ": %s\n", p);
				break;
			} else {
				int c = next_p - p;

				*next_p = '\0';
				printk(KERN_INFO DEV_NAME ": %s\n", p);
				*next_p = '\n';

				p += c + 1;
				n -= c + 1;
			}
		}
	}
}

#endif
