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
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/list.h>

#include "../composer/sh_mobile_debug.h"
#include "../composer/sh_mobile_hdmi_compose.h"

#include <rtapi/screen_graphics.h>

/******************************************************/
/* define prototype                                   */
/******************************************************/

/******************************************************/
/* define local define                                */
/******************************************************/
#define FEATURE_HDMI_CONFIG_BLENDONLY    0

/* define for error threshold */
#define RTAPI_FATAL_ERROR_THRESHOLD  (-256)

/* define for notify */
#define RTAPI_NOTIFY_RESULT_ERROR     3
#define RTAPI_NOTIFY_RESULT_NORMAL    1
#define RTAPI_NOTIFY_RESULT_UNDEFINED 0

/* define for time-out */
#if _EXTEND_TIMEOUT
#define WORK_OUTPUT_WAITTIME          (10*1000) /* 10 sec */
#else
#define WORK_OUTPUT_WAITTIME          500      /* msec */
#endif

/******************************************************/
/* define local variables                             */
/******************************************************/
#if _LOG_DBG >= 1
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
static int __hdmi_compose_pid_create;
static int __hdmi_compose_pid_delete;
static int __hdmi_compose_pid_outut;
static int __hdmi_compose_pid_blend;
#endif
#endif
static DEFINE_SPINLOCK(irqlock_hdmi_wait_list);
static LIST_HEAD(top_hdmi_wait_list);

/******************************************************/
/* local functions                                    */
/******************************************************/
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI

static void hdmi_waitlist_insert(struct composer_rh *rh)
{
	unsigned long flags;

	printk_dbg2(3, "spinlock\n");
	spin_lock_irqsave(&irqlock_hdmi_wait_list, flags);

	list_add_tail(&rh->hdmi_wait_list, &top_hdmi_wait_list);

	spin_unlock_irqrestore(&irqlock_hdmi_wait_list, flags);
}

static struct composer_rh *hdmi_waitlist_delete(struct composer_rh *rh)
{
	unsigned long flags;

	printk_dbg2(3, "spinlock\n");
	spin_lock_irqsave(&irqlock_hdmi_wait_list, flags);

	if (rh == NULL) {
		/* lookup head of wait list. */
		if (!list_empty(&top_hdmi_wait_list)) {
			rh = list_first_entry(&top_hdmi_wait_list,
				struct composer_rh, hdmi_wait_list);
		}
	}

	if (rh) {
		/* remove list */
		list_del_init(&rh->hdmi_wait_list);
	}

	spin_unlock_irqrestore(&irqlock_hdmi_wait_list, flags);

	return rh;
}

static void notify_graphics_image_output(int result, unsigned long user_data)
{
	struct composer_rh *rh;

	DBGENTER("result:%d user_data:0x%lx\n", result, user_data);

	TRACE_LOG(FUNC_WQ_DISP_HDMI);
	/* confirm result code. */
	if (result < RTAPI_FATAL_ERROR_THRESHOLD) {
		/* user_data not reliable. */
		printk_err1("callback ignored. result:%d %s\n",
			result, get_RTAPImsg_graphics(result));

		/* notify hung-up */
		sh_mobile_composer_notifyfatalerror();
	} else {
		rh = (struct composer_rh *) user_data;

		if (result != SMAP_LIB_GRAPHICS_OK) {
			/* report error */
			printk_err("notify_graphics_image_output " \
				"result:%d %s\n",
				result, get_RTAPImsg_graphics(result));
			rh->rh_wqwait_hdmi.status = RTAPI_NOTIFY_RESULT_ERROR;
		} else {
			rh->rh_wqwait_hdmi.status = RTAPI_NOTIFY_RESULT_NORMAL;
		}

		hdmi_waitlist_delete(rh);

		/* schedule to complete. */
		localwork_queue(VAR_WORKQUEUE_HDMI,
			&rh->rh_wqtask_hdmi_comp);
	}

	DBGLEAVE("\n");
}
static void notify_graphics_image_blend_dummy(
	int result, unsigned long user_data)
{
	/* currently not implemented. */
	printk_err1("callback.");
}

/* notify_graphics_image_conv_dummy is declared in lcd_compose.c */
/* notify_graphics_image_edit_dummy is declared in lcd_compose.c */

static void *hdmi_rtapi_create(void)
{
	void *graphic_handle_hdmi = NULL;

	screen_grap_new _new;
	screen_grap_initialize  _ini;
	int  rc;

	/* update for screen_grap_new */
	_new.notify_graphics_image_conv  = notify_graphics_image_conv_dummy;
	_new.notify_graphics_image_blend = notify_graphics_image_blend_dummy;
	_new.notify_graphics_image_output = notify_graphics_image_output;
	_new.notify_graphics_image_edit = notify_graphics_image_edit_dummy;

	graphic_handle_hdmi = screen_graphics_new(&_new);

#if _LOG_DBG >= 1
	if (__hdmi_compose_pid_create == 0) {
		/* record info */
		__hdmi_compose_pid_create = current->pid;
	} else if (__hdmi_compose_pid_create != current->pid) {
		/* error report */
		printk_err("pid is different from previous\n");
	}
#endif
	printk_dbg1(1, "screen_graphics_new result:%p " \
		"in PID:%d TGID:%d\n",
		graphic_handle_hdmi, current->pid, current->tgid);

	if (graphic_handle_hdmi) {
		_ini.handle   = graphic_handle_hdmi;
		_ini.mode = RT_GRAPHICS_MODE_IMAGE_OUTPUT;
#if _LOG_DBG >= 1
		if (5 <= debug)
			dump_screen_grap_initialize(&_ini);
#endif
		rc = screen_graphics_initialize(&_ini);
		if (rc != SMAP_LIB_GRAPHICS_OK) {
			screen_grap_delete _del;
			_del.handle   = graphic_handle_hdmi;

			printk_err1("screen_graphics_initialize " \
				"return by %d %s.\n", rc,
				get_RTAPImsg_graphics(rc));
#if _ERR_DBG >= 1
			if (1 <= debug)
				dump_screen_grap_initialize(&_ini);
#endif

#if _LOG_DBG >= 1
			if (5 <= debug)
				dump_screen_grap_delete(&_del);
#endif
			screen_graphics_delete(&_del);
			graphic_handle_hdmi = NULL;
		}
		if (rc < RTAPI_FATAL_ERROR_THRESHOLD) {
			/* notify hung-up */
			sh_mobile_composer_notifyfatalerror();
		}
	} else {
		/* error report */
		printk_err("graphic_handle_hdmi is NULL\n");
	}
	return graphic_handle_hdmi;
}

static int  hdmi_rtapi_delete(void *graphic_handle_hdmi)
{
	int rc = CMP_OK;

	screen_grap_delete _del;
	screen_grap_quit   _quit;

#if _LOG_DBG >= 1
	if (__hdmi_compose_pid_delete == 0) {
		/* record info */
		__hdmi_compose_pid_delete = current->pid;
	} else if (__hdmi_compose_pid_delete != current->pid) {
		/* error report */
		printk_err("pid is different from previous\n");
	} else if (__hdmi_compose_pid_delete != __hdmi_compose_pid_create) {
		/* error report */
		printk_err("pid is different from create\n");
	}
#endif
	printk_dbg1(1, "delete_handle %p in PID:%d TGID:%d\n",
		graphic_handle_hdmi, current->pid, current->tgid);

	if (graphic_handle_hdmi) {
		_del.handle   = graphic_handle_hdmi;
		_quit.handle  = graphic_handle_hdmi;
		_quit.mode = RT_GRAPHICS_MODE_IMAGE_OUTPUT;
#if _LOG_DBG >= 1
		if (5 <= debug)
			dump_screen_grap_quit(&_quit);
#endif
		rc = screen_graphics_quit(&_quit);
		if (rc != SMAP_LIB_GRAPHICS_OK) {
			/* error report */
			printk_err("screen_graphics_quit " \
				"return by %d %s.\n", rc,
				get_RTAPImsg_graphics(rc));
#if _ERR_DBG >= 1
			dump_screen_grap_quit(&_quit);
#endif
			if (rc < RTAPI_FATAL_ERROR_THRESHOLD) {
				/* notify hung-up */
				sh_mobile_composer_notifyfatalerror();
			}
			rc = CMP_NG;
		} else {
			/* no error */
			rc = CMP_OK;
		}

#if _LOG_DBG >= 1
		if (5 <= debug)
			dump_screen_grap_delete(&_del);
#endif
		screen_graphics_delete(&_del);
	}

	{
		struct composer_rh *rh;
		/* issue fake-hdmi output complete if list is found. */
		while ((rh = hdmi_waitlist_delete(NULL)) != NULL) {
			/* schedule to complete. */
			rh->rh_wqwait_hdmi.status = RTAPI_NOTIFY_RESULT_ERROR;

			localwork_queue(VAR_WORKQUEUE_HDMI,
				&rh->rh_wqtask_hdmi_comp);
		}
	}

	return rc;
}

static int  hdmi_rtapi_output(void *graphic_handle_hdmi, struct composer_rh *rh)
{
	int  rc = CMP_OK;
	screen_grap_image_output *_out;
	struct composer_blendwait *wait;

#if _LOG_DBG >= 1
	if (__hdmi_compose_pid_outut == 0) {
		/* record info */
		__hdmi_compose_pid_outut = current->pid;
	} else if (__hdmi_compose_pid_outut != current->pid) {
		/* error report */
		printk_err("pid is different from previous\n");
	} else if (__hdmi_compose_pid_outut != __hdmi_compose_pid_create) {
		/* error report */
		printk_err("pid is different from create\n");
	}
#endif
	printk_dbg1(1, "output handle:%p in PID:%d TGID:%d\n",
		graphic_handle_hdmi, current->pid, current->tgid);

	/* wait object */
	wait = &rh->rh_wqwait_hdmi;

	_out = &rh->hdmi_data.output;

	_out->handle = graphic_handle_hdmi;
	_out->user_data = (unsigned long)rh;

#if _LOG_DBG >= 1
	if (5 <= debug)
		dump_screen_grap_image_output(_out);
#endif

	/* append wait lists */
	hdmi_waitlist_insert(rh);

	wait->status  = RTAPI_NOTIFY_RESULT_UNDEFINED;
	rc = screen_graphics_image_output(_out);

	if (rc != SMAP_LIB_GRAPHICS_OK) {
		printk_err("screen_graphics_image_output " \
			"return by %d %s.\n", rc,
			get_RTAPImsg_graphics(rc));
#if _ERR_DBG >= 1
		dump_screen_grap_image_output(_out);
#endif

		/* remove wait list */
		hdmi_waitlist_delete(rh);

		if (rc < RTAPI_FATAL_ERROR_THRESHOLD) {
			/* notify hung-up */
			sh_mobile_composer_notifyfatalerror();
		}
		rc = CMP_NG;
		goto finish;
	}

	/* do not wait complete of operation.    */
	/* therefore, assume the normal results. */
	rc = CMP_OK;

	_out->handle = NULL;

finish:
	return rc;
}

/* function hdmi_rtapi_blend should execute in same thread. */

static int  hdmi_rtapi_blend(void *graphic_handle, struct composer_rh *rh)
{
	struct composer_blendwait *wait = &rh->rh_wqwait;
	screen_grap_image_blend *_blend     = &rh->hdmi_data.blend;
	int  rc = CMP_OK;

#if _LOG_DBG >= 1
	if (__hdmi_compose_pid_blend == 0) {
		/* record info */
		__hdmi_compose_pid_blend = current->pid;
	} else if (__hdmi_compose_pid_blend != current->pid) {
		/* error report */
		printk_err("pid is different from previous\n");
	} else if (__hdmi_compose_pid_blend != __lcd_compose_pid_create) {
		/* error report. */
		printk_err("pid is different from create for lcd\n");
	}
#endif
	printk_dbg1(1, "blending handle:%p in PID:%d TGID:%d\n",
		graphic_handle, current->pid, current->tgid);

	wait->status  = RTAPI_NOTIFY_RESULT_UNDEFINED;

	_blend->handle    = graphic_handle;
	_blend->user_data = (unsigned long)rh;

#if _LOG_DBG >= 1
	if (5 <= debug)
		dump_screen_grap_image_blend(_blend);
#endif

	rc = screen_graphics_image_blend(_blend);
	if (rc != SMAP_LIB_GRAPHICS_OK) {
		printk_err("screen_graphics_image_blend return by %d %s.\n",
			rc, get_RTAPImsg_graphics(rc));
#if _ERR_DBG >= 1
		dump_screen_grap_image_blend(_blend);
#endif
		if (rc < RTAPI_FATAL_ERROR_THRESHOLD) {
			/* notify hung-up */
			sh_mobile_composer_notifyfatalerror();
		}
		rc = CMP_NG;
		goto finish3;
	}

	/* WORK_RUNBLEND_WAITTIME is defined in lcd_compose.c */
	rc = wait_event_timeout(
		wait->wait_notify,
		wait->status != RTAPI_NOTIFY_RESULT_UNDEFINED,
		WORK_RUNBLEND_WAITTIME * HZ);
	if (rc < 0) {
		/* report error */
		printk_err("unexpectedly wait_event interrupted by %d .\n", rc);
	} else if (rc == 0) {
		/* report error */
		printk_err1("not detect notify of blending.\n");
	}

	if (wait->status == RTAPI_NOTIFY_RESULT_NORMAL) {
		rc = CMP_OK;
	} else {
		printk_err1("callback result is error.\n");
#if _ERR_DBG >= 1
		dump_screen_grap_image_blend(_blend);
#endif
		rc = CMP_NG;
	}
finish3:
	_blend->handle = NULL;

	return rc;
}

/* config_update_address is declared in lcd_compose.c */

static int  hdmi_config(void *handle,
	struct composer_rh *rh, struct cmp_postdata *post)
{
	struct cmp_data_compose_hdmi *data = &rh->hdmi_data;
	struct cmp_blend_data       *user_data = post->data[1];
	int                         rc = CMP_NG;

	DBGENTER("handle:%p rh:%p post:%p\n", handle, rh, post);

	data->valid = false;
	data->display = false;

	if (user_data == NULL) {
		/* no need hdmi draw */
		rc = CMP_OK;
	} else if (user_data->num <= 1) {
		/* turn off display (draw black) */
		if (handle) {
			/* handle need close */
			data->valid = true;
			rc = CMP_OK;
		}
		rc = CMP_OK;
	} else {
		screen_grap_image_blend  *blend  = &data->blend;
		screen_grap_layer        *layer;
		unsigned long rtaddr;
		int index;
		int i;

		/* parameter check */
		if (user_data->num > CMP_DATA_NUM_GRAP_LAYER) {
			printk_err2("buffer num %d invalid.\n", user_data->num);
			goto finish;
		}

		/* clear information */
		memset(data, 0, sizeof(*data));

		/* create blend parameter */
		blend->output_image     = user_data->buffer[0].image;
		blend->background_color = user_data->bgcolor;

		/* buffer_index for output_image not necessary */

		for (i = 1; i < user_data->num; i++) {
			layer = &data->layer[i-1];
			blend->input_layer[i-1] = layer;

			*layer = user_data->buffer[i];

			index = user_data->index[i];
			if (index < 0) {
				printk_err2("buffer index %d not correct.\n",
					i);
				goto finish;
			}
			rh->buffer_usage[index] |= BUFFER_USAGE_OUTPUT;
			rtaddr = rh->buffer_address[index];
			if (rtaddr == 0 || rtaddr > 0xffff0000u ||
				config_update_address(&layer->image, rtaddr)) {
				printk_err2("address invalid.\n");
				goto finish;
			}
		}

		/* use hdmi output */
		data->display = true;

		if (user_data->num == 2) {
			/* may be not necessary blending. */
			data->need_blend = false;
		} else {
			/* need blending. */
			data->need_blend = true;
		}

		data->valid = true;
		rc = CMP_OK;
	}
finish:

	DBGLEAVE("%d\n", rc);
	return rc;
}

/* work memory */
static int hdmi_memory_allocate(struct cmp_information_hdmi *info,
	unsigned long size)
{
	int rc = CMP_OK;
	DBGENTER("info:%p size:%ld\n", info, size);
	if (info->hdmi_map_handle) {
		if (info->allocatesize < size) {
			/* allocated size is small */
			printk_err2("hdmi memory already allocated " \
				"%ld.\n", info->allocatesize);
			rc = CMP_NG;
		} else {
			/* allocated size is large or equal */
			printk_dbg2(2, "hdmi memory already allocated " \
				"by %ld.\n", info->allocatesize);
		}
	} else {
		/* allocate memory */
		info->hdmi_map_handle = sh_mobile_appmem_alloc(
			size, "composer");
		if (!info->hdmi_map_handle) {
			/* report error */
			printk_err("failed to allocate hdmi memory %ld.",
				size);
		} else {
			info->allocatesize = size;
			printk_dbg2(2, "hdmi memory allocated " \
				"by %ld.\n", info->allocatesize);
		}
	}
	DBGLEAVE("%d\n", rc);
	return rc;
}

static int hdmi_memory_free(struct cmp_information_hdmi *info)
{
	int rc = CMP_OK;
	DBGENTER("info:%p\n", info);
	if (info->hdmi_map_handle) {
		rc = sh_mobile_appmem_free(info->hdmi_map_handle);
		if (rc) {
			/* report error */
			printk_err("failed to free hdmi memory.\n");
			rc = CMP_NG;
		}

		/* no method to recover from error.ignore it. */
		info->hdmi_map_handle = NULL;
		info->allocatesize = 0;
	}
	DBGLEAVE("%d\n", rc);
	return rc;
}

/* configure blending parameters (output) */
static int hdmi_config_output(struct composer_rh *rh,
	struct cmp_information_hdmi *info)
{
	int rc = CMP_OK;
	int need_blend;
	struct cmp_data_compose_hdmi *data = &rh->hdmi_data;

	DBGENTER("info:%p\n", info);

	need_blend = data->need_blend;
#if FEATURE_HDMI_CONFIG_BLENDONLY
	/* always perform blending */
	need_blend = true;
#endif
	if (!need_blend) {
		screen_grap_layer   *layer = &data->layer[0];

		if (layer->rotate != RT_GRAPHICS_ROTATE_0) {
			printk_dbg2(3, "rotation not supported\n");
			need_blend = true;
		}

		if (layer->mirror != RT_GRAPHICS_MIRROR_N) {
			printk_dbg2(3, "mirror not supported\n");
			need_blend = true;
		}

		if (layer->image.format != RT_GRAPHICS_COLOR_YUV420SP &&
			layer->image.format != RT_GRAPHICS_COLOR_YUV422SP &&
			layer->image.format != RT_GRAPHICS_COLOR_ARGB8888) {
			printk_dbg2(3, "format not supported\n");
			need_blend = true;
		}

		if (!need_blend) {
			/* image output without blending. */
			data->output.output_image = layer->image;
			data->output.rotate       = layer->rotate;
		}
	}
	data->need_blend = need_blend;
	/* clear blend_buffer id */
	info->blend_bufferid = -1;

	printk_dbg2(2, "need_blend:%d\n", need_blend);

	if (!need_blend)
		goto finish;

	if (info->hdmi_map_handle == NULL) {
		printk_dbg1(1, "workmemory not allocated.\n");
		rc = CMP_NG;
		goto finish;
	} else {
		unsigned long rtaddr;
		int           offset;
		int           lane;

		lane = info->hdmi_count_display & 1;

		if (lane)
			offset = 0;
		else
			offset = info->allocatesize/2;

		rtaddr = info->hdmi_map_handle->rtaddr +
			info->hdmi_map_handle->offset;

		/* update output address */
		if (config_update_address(&data->blend.output_image, rtaddr)) {
			/* failed to set output address. */
			printk_err("hdmi output invalid.\n");
			rc = CMP_NG;
			goto finish;
		}

		/* image output without blending. */
		data->output.output_image = data->blend.output_image;
		data->output.rotate       = RT_GRAPHICS_ROTATE_0;
		info->hdmi_count_display++;

		/* record blend_buffer id */
		info->blend_bufferid = lane;
	}

finish:
	DBGLEAVE("%d\n", rc);
	return rc;
}

#endif

/******************************************************/
/* global functions                                   */
/******************************************************/

