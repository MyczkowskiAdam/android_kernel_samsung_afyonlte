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

#include <linux/sh_mobile_composer.h>
#include <media/sh_mobile_appmem.h>
#include "../composer/sh_mobile_debug.h"
#include "../composer/sh_mobile_work.h"
#include "../composer/sh_mobile_lcd_compose.h"

#include <rtapi/screen_graphics.h>
#include <rtapi/screen_display.h>
#include <rtapi/system_memory.h>
#include <linux/fb.h>

#include <video/sh_mobile_lcdc.h>

/******************************************************/
/* define prototype                                   */
/******************************************************/

/******************************************************/
/* define local define                                */
/******************************************************/
#define FEATURE_IGNORE_SET_SIZE_ERROR    1

/* define for error threshold */
#define RTAPI_FATAL_ERROR_THRESHOLD  (-256)

/* define for notify */
#define RTAPI_NOTIFY_RESULT_ERROR     3
#define RTAPI_NOTIFY_RESULT_NORMAL    1
#define RTAPI_NOTIFY_RESULT_UNDEFINED 0

/* define for time-out */
#if _EXTEND_TIMEOUT
#define WORK_RUNBLEND_WAITTIME         10       /* sec  */
#else
#define WORK_RUNBLEND_WAITTIME         1        /* sec  */
#endif

/* define for get_fb_info */
#define USE_FB_INDEX    0

/* define for work_dispdraw function. */
#define WORK_DISPDRAW_INVALID_YOFFSET  -1

/* define for index of fb_offset_info. */
#define FB_OFFSET_INFO_OFFSET    0
#define FB_OFFSET_INFO_YLINE     1

/* define for index of reserve */
#define FB_RESERVE_OFFSET        0
#define FB_RESERVE_BUFFERID      1
#define BUFFERID_A               0
#define BUFFERID_B               0x12345678
#define BUFFERID_BLANK           0x55555555

/******************************************************/
/* define local variables                             */
/******************************************************/
#if _LOG_DBG >= 1
static int __lcd_compose_pid_create;
static int __lcd_compose_pid_delete;
static int __lcd_compose_pid_blend;
#endif

/******************************************************/
/* local functions                                    */
/******************************************************/
static void notify_graphics_image_output_dummy(
	int result, unsigned long user_data)
{
	/* currently not implemented. */
	printk_err1("callback.");
}
static void notify_graphics_image_edit_dummy(
	int result, unsigned long user_data)
{
	/* currently not implemented. */
	printk_err1("callback.");
}
static void notify_graphics_image_conv_dummy(
	int result, unsigned long user_data)
{
	/* currently not implemented. */
	printk_err1("callback.");
}

static void notify_graphics_image_blend(int result, unsigned long user_data)
{
	struct composer_rh *rh;

	DBGENTER("result:%d user_data:0x%lx\n", result, user_data);

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
			printk_err("notify_graphics_image_blend result:%d %s\n",
				result, get_RTAPImsg_graphics(result));
			rh->rh_wqwait.status = RTAPI_NOTIFY_RESULT_ERROR;
		} else {
			rh->rh_wqwait.status = RTAPI_NOTIFY_RESULT_NORMAL;
		}
		/* wakeup waiting task */
		wake_up_all(&rh->rh_wqwait.wait_notify);
	}

	DBGLEAVE("\n");
}



static void *lcd_rtapi_create(int lcd_width, int lcd_height,
	int hdmi_width, int hdmi_height)
{
	void *graphic_handle = NULL;
	screen_grap_new _new;
	screen_grap_set_blend_size _size;
	screen_grap_initialize  _ini;
	int  rc;

	/* update for screen_grap_new */
	_new.notify_graphics_image_conv   = notify_graphics_image_conv_dummy;
	_new.notify_graphics_image_blend  = notify_graphics_image_blend;
	_new.notify_graphics_image_output = notify_graphics_image_output_dummy;
	_new.notify_graphics_image_edit   = notify_graphics_image_edit_dummy;

	graphic_handle = screen_graphics_new(&_new);

#if _LOG_DBG >= 1
	if (__lcd_compose_pid_create == 0) {
		/* record info */
		__lcd_compose_pid_create = current->pid;
	} else if (__lcd_compose_pid_create != current->pid) {
		/* error report */
		printk_err("pid is different from previous\n");
	}
#endif
	printk_dbg1(1, "screen_graphics_new result:%p in PID:%d TGID:%d\n",
		graphic_handle, current->pid, current->tgid);

	if (graphic_handle && lcd_width && lcd_height) {
		_size.handle = graphic_handle;
		_size.lcd_width = lcd_width;
		_size.lcd_height = lcd_height;
		_size.hdmi_width = ((hdmi_width == 0) ? 0 : 1920);
		_size.hdmi_height = ((hdmi_height == 0) ? 0 : 1088);

#if _LOG_DBG >= 1
		if (5 <= debug)
			dump_screen_grap_set_blend_size(&_size);
#endif

		rc = screen_graphics_set_blend_size(&_size);
		if (rc != SMAP_LIB_GRAPHICS_OK) {
			printk_err("screen_graphics_set_blend_size " \
				"return by %d %s.\n", rc,
				get_RTAPImsg_graphics(rc));
#if _ERR_DBG >= 1
			dump_screen_grap_set_blend_size(&_size);
#endif

#if FEATURE_IGNORE_SET_SIZE_ERROR
			/* ignore error */
			rc = SMAP_LIB_GRAPHICS_OK;
#else
			lcd_rtapi_delete(graphic_handle);
			graphic_handle = NULL;
#endif
		}
		if (rc < RTAPI_FATAL_ERROR_THRESHOLD) {
			/* notify hung-up */
			sh_mobile_composer_notifyfatalerror();
		}
	} else if (graphic_handle) {
		/* skip set_blend_size */
		printk_dbg1(2, "skip screen_graphics_set_blend_size\n");
	} else {
		/* error report */
		printk_dbg1(1, "graphic_handle is NULL\n");
	}

	if (graphic_handle) {
		_ini.handle   = graphic_handle;
		_ini.mode = RT_GRAPHICS_MODE_IMAGE_BLEND;
#if _LOG_DBG >= 1
		if (5 <= debug)
			dump_screen_grap_initialize(&_ini);
#endif
		rc = screen_graphics_initialize(&_ini);
		if (rc != SMAP_LIB_GRAPHICS_OK) {
			printk_err("screen_graphics_initialize " \
				"return by %d %s.\n", rc,
				get_RTAPImsg_graphics(rc));
#if _ERR_DBG >= 1
			dump_screen_grap_initialize(&_ini);
#endif
			lcd_rtapi_delete(graphic_handle);
			graphic_handle = NULL;
		}
		if (rc < RTAPI_FATAL_ERROR_THRESHOLD) {
			/* notify hung-up */
			sh_mobile_composer_notifyfatalerror();
		}
	} else {
		/* error report */
		printk_dbg1(1, "graphic_handle is NULL\n");
	}
	return graphic_handle;
}

static int  lcd_rtapi_delete(void *graphic_handle)
{
	screen_grap_delete _del;
	screen_grap_quit   _quit;
	int                rc = CMP_NG;

#if _LOG_DBG >= 1
	if (__lcd_compose_pid_delete == 0) {
		/* record info */
		__lcd_compose_pid_delete = current->pid;
	} else if (__lcd_compose_pid_delete != current->pid) {
		/* error report */
		printk_err("pid is different from previous\n");
	} else if (__lcd_compose_pid_delete != __lcd_compose_pid_create) {
		/* error report */
		printk_err("pid is different from create\n");
	}
#endif
	printk_dbg1(1, "delete_handle %p in PID:%d TGID:%d\n",
		graphic_handle, current->pid, current->tgid);

	if (graphic_handle) {
		_del.handle   = graphic_handle;
		_quit.handle  = graphic_handle;
		_quit.mode = RT_GRAPHICS_MODE_IMAGE_BLEND;
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

	return rc;
}

static int  lcd_rtapi_blend(void *graphic_handle, struct composer_rh *rh)
{
	struct composer_blendwait *wait = &rh->rh_wqwait;
	screen_grap_image_blend *_blend     = &rh->lcd_data.blend;
	int  rc = CMP_OK;

#if _LOG_DBG >= 1
	if (__lcd_compose_pid_blend == 0) {
		/* record info */
		__lcd_compose_pid_blend = current->pid;
	} else if (__lcd_compose_pid_blend != current->pid) {
		/* error report */
		printk_err("pid is different from previous\n");
	} else if (__lcd_compose_pid_blend != __lcd_compose_pid_create) {
		/* error report */
		printk_err("pid is different from create\n");
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

	rc = wait_event_timeout(
		wait->wait_notify,
		wait->status != RTAPI_NOTIFY_RESULT_UNDEFINED,
		WORK_RUNBLEND_WAITTIME * HZ);
	if (rc < 0) {
		/* report error */
		printk_err("unexpectly wait_event interrupted by %d .\n", rc);
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

static int config_update_address(screen_grap_image_param *image,
	unsigned long rtaddr)
{
	int num_layer;
	int rc = CMP_OK;

	switch (image->format) {
	case RT_GRAPHICS_COLOR_YUV420PL:
		num_layer = 0b111;
		break;
	case RT_GRAPHICS_COLOR_YUV422SP:
	case RT_GRAPHICS_COLOR_YUV420SP:
		num_layer = 0b011;
		break;
	case RT_GRAPHICS_COLOR_RGB565:
	case RT_GRAPHICS_COLOR_RGB888:
	case RT_GRAPHICS_COLOR_ARGB8888:
	case RT_GRAPHICS_COLOR_XRGB8888:
	case RT_GRAPHICS_COLOR_ABGR8888:
	case RT_GRAPHICS_COLOR_XBGR8888:
		num_layer = 0b001;
		break;
	default:
		printk_err("not supported format");
		num_layer = 0b000;
		rc = CMP_NG;
		break;
	}
	if (num_layer & 0b001)
		image->address += rtaddr;

	if (num_layer & 0b010)
		image->address_c0 += rtaddr;

	if (num_layer & 0b100)
		image->address_c1 += rtaddr;

	if (rc) {
		/* address set to NULL to be able to detect error */
		image->address = NULL;
	}
	return rc;
}


static int  lcd_config(struct composer_rh *rh, struct cmp_postdata *post)
{
	struct cmp_data_compose_lcd *data = &rh->lcd_data;
	struct cmp_blend_data       *user_data = post->data[0];
	int                         rc = CMP_NG;

	DBGENTER("rh:%p post:%p\n", rh, post);

	data->valid = false;
	data->display = false;
	data->need_blend = false;

	if (user_data == NULL) {
		/* no need fb draw */
		rc = CMP_OK;
	} else if (user_data->num <= 1) {
		/* turn off display (draw blank) */
		screen_grap_image_blend *blend = &data->blend;

		/* clear information */
		memset(data, 0, sizeof(*data));

		/* create blend parameter */
		blend->output_image     = user_data->buffer[0].image;
		blend->background_color = user_data->bgcolor;

		data->valid = true;
		data->display = true;
		data->need_blend = true;
		rc = CMP_OK;
	} else {
		int i;
		screen_grap_image_blend *blend = &data->blend;
		screen_grap_layer       *layer;
		unsigned long rtaddr;

		/* handle blend information */
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
			int index;

			layer = &data->layer[i-1];
			blend->input_layer[i-1] = layer;

			*layer = user_data->buffer[i];

			index = user_data->index[i];
			if (index < 0) {
				printk_err2("buffer index %d not correct.\n",
					i);
				goto finish;
			}
			rh->buffer_usage[index] |= BUFFER_USAGE_BLEND;
			rtaddr = rh->buffer_address[index];
			if (rtaddr == 0 || rtaddr > 0xffff0000u ||
				config_update_address(&layer->image, rtaddr)) {
				printk_err2("address invalid.\n");
				goto finish;
			}
		}

		data->valid = true;
		data->display = true;
		data->need_blend = true;
		rc = CMP_OK;
	}
finish:

	DBGLEAVE("%d\n", rc);
	return rc;
}


static void get_fb_info(struct cmp_information_fb *info)
{
	struct fb_info *fb_info;
	int    res = 0;

	fb_info = registered_fb[USE_FB_INDEX];

	if (fb_info == NULL) {
		/* error report */
		printk_err("FB device %d not found.\n", USE_FB_INDEX);
		goto err;
	}

	if (!lock_fb_info(fb_info)) {
		res = -ENODEV;
	} else {
		if (!try_module_get(fb_info->fbops->owner)) {
			res = -ENODEV;
		} else {
			if (fb_info->fbops->fb_open) {
				res = fb_info->fbops->fb_open(fb_info, 0);
				if (res)
					module_put(fb_info->fbops->owner);
			}
		}
		unlock_fb_info(fb_info);
	}

	if (res == 0) {
		unsigned long offset;
		int           linelength;
		int           quotient, remainder;

		info->fb_info = fb_info;

		linelength = fb_info->fix.line_length;

		/* set page 0 offset */
		info->fb_offset_info[0][FB_OFFSET_INFO_OFFSET] = 0;
		info->fb_offset_info[0][FB_OFFSET_INFO_YLINE] = \
			0 / linelength;

		/* set page 1 offset */
		offset = fb_info->var.reserved[FB_RESERVE_OFFSET];
		if (offset == 0) {
			/* assume half of smem_len */
			offset = fb_info->fix.smem_len / 2;
		}

		info->fb_offset_info[1][FB_OFFSET_INFO_OFFSET] = offset;
		info->fb_offset_info[1][FB_OFFSET_INFO_YLINE] = \
			offset / linelength;

		if (info->fb_offset_info[1][FB_OFFSET_INFO_YLINE] >=
			fb_info->var.yres) {
			/* frame buffer is double_buffer */
			info->fb_double_buffer = 1;
		} else {
			/* frame buffer is not double_buffer */
			info->fb_double_buffer = 0;
		}

		quotient  = offset / linelength;
		remainder = offset % linelength;

		if (offset & 0xF)
			printk_err("framebuffer not aligned 16.\n");

		if (remainder)
			printk_err("xoffset is not 0.\n");

		printk_dbg2(3, "lane0 offset:%d yline:%d\n",
			info->fb_offset_info[0][FB_OFFSET_INFO_OFFSET],
			info->fb_offset_info[0][FB_OFFSET_INFO_YLINE]);
		printk_dbg2(3, "lane1 offset:%d yline:%d\n",
			info->fb_offset_info[1][FB_OFFSET_INFO_OFFSET],
			info->fb_offset_info[1][FB_OFFSET_INFO_YLINE]);
	}
err:
	return;
}

static void release_fb_info(struct cmp_information_fb *info)
{
	struct fb_info   *fb_info = info->fb_info;

	if (fb_info) {
		if (!lock_fb_info(fb_info)) {
			/* error report */
			printk_err("ignore module_put of FB.\n");
		} else {
			if (fb_info->fbops->fb_release != NULL)
				fb_info->fbops->fb_release(fb_info, 0);

			module_put(fb_info->fbops->owner);
			unlock_fb_info(fb_info);
		}

		info->fb_info = NULL;
	}
}

static int first_fb_register_rtmemory(unsigned long *args)
{
	struct fb_info            *fb_info;
	unsigned long             fb_addr;
	unsigned long             fb_size;
	struct cmp_information_fb *info = (struct cmp_information_fb *)args[2];

	get_fb_info(info);

	fb_info = info->fb_info;
	if (fb_info == NULL) {
		printk_err("can not open FB driver\n");
		goto err_exit;
	}

	fb_addr = fb_info->fix.smem_start;
	fb_size = fb_info->fix.smem_len;

	info->queue_fb_map_handle = sh_mobile_rtmem_physarea_register(
		fb_size, fb_addr);

	if (info->queue_fb_map_handle) {
		printk_dbg1(2, "framebuffer 0x%lx-0x%lx " \
			"map success.\n",                 \
			fb_addr, fb_addr + fb_size - 1);
	} else {
		printk_err("can not map framebuffer 0x%lx-0x%lx.\n",\
			fb_addr, fb_addr + fb_size - 1);

		/* release fb_info */
		release_fb_info(info);
	}

err_exit:
	return 0;
}

static int second_fb_register_rtmemory(unsigned long *args)
{
	unsigned long fb_addr = args[0];
	unsigned long fb_size = args[1];
	struct cmp_information_fb *info = (struct cmp_information_fb *)args[2];

	/* register address */
	{
		void                    *handle;
		screen_disp_set_address addr;
		screen_disp_delete      del;
		int                     rc;

		handle = screen_display_new();
		if (handle == NULL) {
			/* report error */
			printk_err("display_handle is NULL\n");
		} else {
			addr.handle      = handle;
			addr.output_mode = RT_DISPLAY_LCD1;
			addr.buffer_id   = RT_DISPLAY_BUFFER_B;
			addr.address     = fb_addr;
			addr.size        = fb_size;

#if _LOG_DBG >= 1
			if (5 <= debug)
				dump_screen_disp_set_address(&addr);
#endif

			rc = screen_display_set_address(&addr);

			del.handle = handle;
			screen_display_delete(&del);

			if (rc != SMAP_LIB_DISPLAY_OK) {
				/* report error */
				printk_err("screen_display_set_address " \
					"return by %d %s.\n", rc,
					get_RTAPImsg_display(rc));
#if _ERR_DBG >= 1
				dump_screen_disp_set_address(&addr);
#endif
				goto err_exit;
			}
		}
	}

	/* ignore ioctl() parameters */
	printk_dbg1(3, "framebuffer 0x%lx-0x%lx is used.\n",
		fb_addr, fb_addr + fb_size - 1);

	info->queue_fb_map_handle2 = sh_mobile_rtmem_physarea_register(
		fb_size, fb_addr);

	if (info->queue_fb_map_handle2) {
		printk_dbg1(2, "framebuffer 0x%lx-0x%lx " \
			"map success.\n",                 \
			fb_addr, fb_addr + fb_size - 1);
	} else {
		/* there is no API to unregister the address of
		   screen_display_set_address. */
		printk_err("can not map framebuffer 0x%lx-0x%lx.\n",\
			fb_addr, fb_addr + fb_size - 1);
	}
err_exit:;

	return 0;
}


static int  lcd_set_address(int id, unsigned long addr, unsigned long size,
	struct localworkqueue  *workqueue, struct cmp_information_fb *info)
{
	int rc = 0;
	unsigned long args[3];

	DBGENTER("id:%d addr:0x%lx, size:0x%lx, workqueue:%p info:%p\n",
		id, addr, size, workqueue, info);

	/* confirm already configured. */
	if (id == FB_SCREEN_BUFFERID0) {
		if (info->queue_fb_map_handle) {
			printk_err2("id 0 already configure.\n");
			rc = -EBUSY;
			/* free resource performed,
			   when all composer handle closed. */
			goto err_exit;
		}
	} else if (id == FB_SCREEN_BUFFERID1) {
		if (info->queue_fb_map_handle2) {
			printk_err2("id 1 already configure.\n");
			rc = -EBUSY;
			/* currently, free resource is not safely. */
			goto err_exit;
		}
	} else {
		printk_err2("argument id:%d not valid.\n", id);
		rc = -EINVAL;
		goto err_exit;
	}


	args[0] = addr;
	args[1] = size;
	args[2] = (unsigned long)info;

	if (id == FB_SCREEN_BUFFERID0) {
		/* register function for id = 0 */
		rc = indirect_call(workqueue,
			first_fb_register_rtmemory, 3, &args[0]);
	} else {
		/* register function for id = 1 */
		rc = indirect_call(workqueue,
			second_fb_register_rtmemory, 3, &args[0]);
	}

	if (rc == 0) {
		if (id == FB_SCREEN_BUFFERID0) {
			if (info->queue_fb_map_handle == NULL) {
				/* set error flag */
				rc = -EINVAL;
			} else {
				/* set no error flag and record address. */
				info->queue_fb_map_address    = addr;
				info->queue_fb_map_endaddress = addr + size;
				rc = 0;
			}
		} else {
			if (info->queue_fb_map_handle2 == NULL) {
				/* set error flag */
				rc = -EINVAL;
			} else {
				/* set no error flag and record address. */
				info->queue_fb_map_address2    = addr;
				info->queue_fb_map_endaddress2 = addr + size;
				rc = 0;
			}
		}
	} else {
		printk_err("failed to request remote function call\n");
		rc = -EINVAL;
	}
err_exit:

	DBGLEAVE("%d\n", rc);
	return rc;
}

static int fb_unregister_rtmemory(unsigned long *args)
{
	int id = args[0];
	struct cmp_information_fb *info = (struct cmp_information_fb *)args[1];

	switch (id) {
	case FB_SCREEN_BUFFERID0:
		if (info->queue_fb_map_handle) {
			sh_mobile_rtmem_physarea_unregister(
				info->queue_fb_map_handle);
			info->queue_fb_map_handle = NULL;
		}
		if (info->fb_info) {
			/* if all handle is closed,
			   then release fb_handle. */
			release_fb_info(info);
		}
		break;
	case FB_SCREEN_BUFFERID1:
		if (info->queue_fb_map_handle2) {
			sh_mobile_rtmem_physarea_unregister(
				info->queue_fb_map_handle2);
			info->queue_fb_map_handle2 = NULL;
		}
		break;
	}
	return 0;
}

static int  lcd_unset_address(int id, struct localworkqueue  *workqueue,
	struct cmp_information_fb *info)
{
	int rc = 0;
	unsigned long args[2];

	DBGENTER("id:%d workqueue:%p info:%p\n", id, workqueue, info);

	/* confirm already configured. */
	switch (id) {
	case FB_SCREEN_BUFFERID0:
	case FB_SCREEN_BUFFERID1:
		break;
	default:
		/*error report */
		printk_err2("argument id:%d not valid.\n", id);
		rc = -EINVAL;
		goto err_exit;
	}

	args[0] = id;
	args[1] = (unsigned long)info;

	rc = indirect_call(workqueue,
		fb_unregister_rtmemory, 2, &args[0]);

	if (rc == 0) {
		if (id == FB_SCREEN_BUFFERID0) {
			if (info->queue_fb_map_handle != NULL) {
				/* set error flag */
				rc = -EINVAL;
			} else if (info->fb_info != NULL) {
				/* set error flag */
				rc = -EINVAL;
			} else {
				/* set no error flag and reset address. */
				info->queue_fb_map_address    = 0;
				info->queue_fb_map_endaddress = 0;
				rc = 0;
			}
		} else {
			if (info->queue_fb_map_handle2 != NULL) {
				/* set error flag */
				rc = -EINVAL;
			} else {
				/* set no error flag and record address. */
				info->queue_fb_map_address2    = 0;
				info->queue_fb_map_endaddress2 = 0;
				rc = 0;
			}
		}
	} else {
		printk_err("failed to request remote function call\n");
		rc = -EINVAL;
	}
err_exit:

	DBGLEAVE("%d\n", rc);
	return rc;
}

static int  lcd_config_output(struct composer_rh *blend_req,
	struct cmp_information_fb *info)
{
	int              res = CMP_NG;
	int              lane;
	int              offset;
	int              y_offset = WORK_DISPDRAW_INVALID_YOFFSET;
	int              bufferid = FB_SCREEN_BUFFERID0;
	unsigned long    fb_rtstart;
	unsigned long    fb_rtend;
	unsigned long    fb_size;
	struct cmp_data_compose_lcd *data;
	struct rtmem_phys_handle *queue_fb_map_handle;
	struct rtmem_phys_handle *queue_fb_map_handle2;

	DBGENTER("rh:%p info:%p\n", blend_req, info);

	queue_fb_map_handle  = info->queue_fb_map_handle;
	queue_fb_map_handle2 = info->queue_fb_map_handle2;

	/* confirm condition. */
	if (info->fb_info == NULL ||
		blend_req == NULL ||
		queue_fb_map_handle == NULL) {
		printk_dbg1(1, "invalid condition: blend_req:%p " \
			"queue_fb_map_handle:%p fb_info: %p.\n",
			blend_req, queue_fb_map_handle, info->fb_info);
		goto draw_skip;
	}

	data = &blend_req->lcd_data;

	/* assume need blending. */
	data->need_blend = true;
	/* clear blend_buffer id */
	info->blend_bufferid = -1;

	if (data->blend.input_layer[0] == NULL) {
		/* display blank image. */
		data->need_blend = false;

		y_offset = 0;
		bufferid = FB_SCREEN_BLANKBUFFER;
	} else if (data->blend.input_layer[1] == NULL &&
		data->blend.input_layer[0] != NULL &&
		queue_fb_map_handle2 != NULL) {
		unsigned long    rt_addr;

		/* if use layer is one, blending may not necessary. */

		if (data->layer[0].image.format !=
			data->blend.output_image.format) {
			printk_dbg2(3, "detect format mismatch.\n");
			goto next_step;
		} else if (data->layer[0].image.stride !=
			data->blend.output_image.stride) {
			printk_dbg2(3, "detect stride mismatch.\n");
			goto next_step;
		}

		fb_rtstart = queue_fb_map_handle2->rt_addr;
		fb_size    = queue_fb_map_handle2->size;
		fb_rtend   = fb_rtstart + fb_size;

		rt_addr   = (unsigned long) data->layer[0].image.address;

		if (fb_rtstart <= rt_addr && rt_addr < fb_rtend) {
			/* set output_image address is not necessary. */
			data->need_blend = false;

			offset = rt_addr - fb_rtstart;

			/* select lane 0 of second frame buffer */
			lane = 0;
			if (info->fb_double_buffer) {
				if (offset >= fb_size / 2) {
					/* select lane 1 of
					   second frame buffer */
					lane = 1;
				}
			}

			y_offset = info->fb_offset_info[lane]\
[FB_OFFSET_INFO_YLINE];
			bufferid = FB_SCREEN_BUFFERID1;
		}
	}

next_step:
	printk_dbg2(3, "need_blend:%d bufferid:%d y_offset:%d\n",
		data->need_blend, bufferid, y_offset);

	if (data->need_blend) {
		/* set output_image address. */

		fb_rtstart = queue_fb_map_handle->rt_addr;
		fb_size    = queue_fb_map_handle->size;

		if (info->fb_double_buffer) {
			/* select lane 0 of frame buffer */
			lane = info->fb_count_display & 1;
			fb_size /= 2;
		} else {
			lane = 0;
		}

		y_offset = info->fb_offset_info[lane]\
[FB_OFFSET_INFO_YLINE];

		offset = (unsigned long)data->blend.output_image.address;
		if (offset > fb_size) {
			offset = 0;
			printk_err("parameter of output_image ignored.");
		}

		printk_dbg2(3, "output offset:%d\n", offset);

		offset += info->fb_offset_info[lane]\
[FB_OFFSET_INFO_OFFSET];

		data->blend.output_image.address =
			(unsigned char *) (fb_rtstart + offset);

		printk_dbg2(3, "y_offset:%d\n", y_offset);

		/* record blend_buffer id */
		info->blend_bufferid = lane;
	}

	/* confirm valid condition. */
	if (y_offset == WORK_DISPDRAW_INVALID_YOFFSET) {
		printk_dbg1(1, "y_offset invalid\n");
		res = CMP_NG;
	} else {
		info->fb_select_bufferid = bufferid;
		info->fb_select_yoffset  = y_offset;
		res = CMP_OK;
	}

draw_skip:;

	DBGLEAVE("%d\n", res);
	return res;
}

static int lcd_fb_pan_display(struct composer_rh *blend_req,
	struct cmp_information_fb *info)
{
	struct fb_var_screeninfo vInfo;
	int              res      = CMP_OK;
	int              y_offset = info->fb_select_yoffset;
	int              bufferid = info->fb_select_bufferid;
	struct fb_info   *fb_info = info->fb_info;

	DBGENTER("rh:%p info:%p\n", blend_req, info);

	/* no need drawing */
	if (fb_info == NULL || blend_req == NULL) {
		printk_dbg1(1, "invalid condition blend_req:%p fb_info: %p.\n",
			blend_req, fb_info);
		res = CMP_NG;
		goto draw_skip;
	}

	/* confirm valid condition. */
	if (y_offset == WORK_DISPDRAW_INVALID_YOFFSET) {
		printk_dbg1(1, "y_offset invalid\n");
		res = CMP_NG;
	} else if (!lock_fb_info(fb_info)) {
		printk_dbg2(3, "lock_fb_info failed.\n");
		res = CMP_NG;
	} else {
		vInfo = fb_info->var;

		vInfo.xoffset = 0;
		vInfo.yoffset = y_offset;

		/* set current display mode */
		if (bufferid == FB_SCREEN_BUFFERID0) {
			/* display FB's buffer*/
			vInfo.reserved[FB_RESERVE_BUFFERID] = BUFFERID_A;
		} else if (bufferid == FB_SCREEN_BUFFERID1) {
			/* display gpu buffer */
			vInfo.reserved[FB_RESERVE_BUFFERID] = BUFFERID_B;
		} else {
			/* display blank buffer */
			vInfo.reserved[FB_RESERVE_BUFFERID] = BUFFERID_BLANK;
		}

		res = fb_pan_display(fb_info, &vInfo);
		unlock_fb_info(fb_info);
		if (res != 0) {
			printk_err("fb_pan_display failed (Y Offset: %d," \
				" Error: %d)\n", y_offset, res);
		} else {
			res = CMP_OK;
		}

		info->fb_count_display++;
	}
draw_skip:

	DBGLEAVE("%d\n", res);
	return res;
}

static int  lcd_get_resolution(int *width, int *height,
		struct cmp_information_fb *info)
{
	struct fb_info   *fb_info;

	DBGENTER("width:%p height:%p info:%p\n",
		width, height, info);

	fb_info = info->fb_info;

	*width = SH_MLCD_WIDTH;
	*height = SH_MLCD_HEIGHT;
	if (fb_info == NULL) {
		/* use default   */
		printk_dbg2(3, "fb_info is NULL.\n");
	} else if (!lock_fb_info(fb_info)) {
		/* use default   */
		printk_dbg2(3, "lock_fb_info failed.\n");
	} else {
		*width  = fb_info->var.xres;
		*height = fb_info->var.yres;

		unlock_fb_info(fb_info);
	}

	DBGLEAVE("\n");
	return CMP_OK;
}
