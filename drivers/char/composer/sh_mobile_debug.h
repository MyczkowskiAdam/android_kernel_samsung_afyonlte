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
#ifndef _SH_MOBILE_COMPOSER_DEBUG_H
#define _SH_MOBILE_COMPOSER_DEBUG_H

#define INTERNAL_DEBUG  1
#define _TIM_DBG   0		/* record time log.                 */
#define _LOG_DBG   1		/* generate debug log.              */
#define _ERR_DBG   2		/* generate error log.              */
#define _ATR_DBG   0		/* use atrace log experimentally    */
#define DEV_NAME   "composer"

#define _EXTEND_TIMEOUT  1

#define INTERNAL_DEBUG_USE_DEBUGFS  (0 && defined(CONFIG_DEBUG_FS))

#include <rtapi/screen_display.h>
#include <rtapi/screen_graphics.h>
#include <rtapi/system_memory.h>
#if INTERNAL_DEBUG
#include <linux/sh_mobile_composer.h>
#if INTERNAL_DEBUG_USE_DEBUGFS
#include <linux/debugfs.h>
#endif
#include <linux/seq_file.h>
#endif

#if _ATR_DBG
#define CREATE_TRACE_POINTS
#include "../composer/sh_mobile_atrace.h"
#undef CREATE_TRACE_POINTS

#define ATRACE_BEGIN(X)  trace_tracing_mark_write(1, X, 0);
#define ATRACE_END(X)    trace_tracing_mark_write(0, X, 0);
#define ATRACE_INT(X, Y) trace_tracing_mark_write(2, X, Y);
#else
#define ATRACE_BEGIN(X)
#define ATRACE_END(X)
#define ATRACE_INT(X, Y)
#endif

/******************************/
/* define structure           */
/******************************/

/******************************/
/* define                     */
/******************************/

/* macros for general error message */
#define printk_lowerr(fmt, arg...) \
	printk(KERN_ERR DEV_NAME ":E %s: " fmt, \
				__func__, ## arg);

/* macros for general log message */
#define printk_lowdbg(fmt, arg...) \
	printk(KERN_INFO DEV_NAME ": %s: " fmt, \
				__func__, ## arg);

/* macros for normal-usecase error message */
#if _ERR_DBG >= 2
#define printk_err2(fmt, arg...) \
	do { \
		TRACE_LOG(FUNC_NONE); \
		if (debug > 1) \
			printk_lowerr(fmt, ## arg); \
	} while (0)
#else
#define printk_err2(fmt, arg...)
#endif

/* macros for RT-API related error message */
#if _ERR_DBG >= 1
#define printk_err1(fmt, arg...) \
	do { \
		TRACE_LOG(FUNC_NONE); \
		if (debug > 0) \
			printk_lowerr(fmt, ## arg); \
	} while (0)
#else
#define printk_err1(fmt, arg...)
#endif

/* macros for unexpected-error message */
#define printk_err(fmt, arg...) \
	do { \
		TRACE_LOG(FUNC_NONE); \
		printk_lowerr(fmt, ## arg); \
	} while (0)

/* macros for normal-usecase log message */
#if _LOG_DBG >= 2
#define printk_dbg2(level, fmt, arg...) \
	do { \
		if ((level)+2 <= debug) \
			printk_lowdbg(fmt, ## arg); \
	} while (0)
#else
#define printk_dbg2(level, fmt, arg...)
#endif

/* macros for RT-API log message */
#if _LOG_DBG >= 1
#define printk_dbg1(level, fmt, arg...) \
	do { \
		if ((level)+2 <= debug) \
			printk_lowdbg(fmt, ## arg); \
	} while (0)
#else
#define printk_dbg1(level, fmt, arg...)
#endif

#define printk_dbg(level, fmt, arg...) \
	do { \
		if ((level)+2 <= debug) \
			printk_lowdbg(fmt, ## arg); \
	} while (0)

#define DBGENTER(fmt, arg...) printk_dbg2(2, "in  "  fmt, ## arg)
#define DBGLEAVE(fmt, arg...) printk_dbg2(2, "out "  fmt, ## arg)


#if INTERNAL_DEBUG == 0
/* do not record tracelog */
#define TRACE_ENTER(ID)
#define TRACE_LEAVE(ID)
#define TRACE_LOG(ID)
#define TRACE_LOG1(ID, VAL1)

#else

#define ID_TRACE_ENTER        1
#define ID_TRACE_LEAVE        2
#define ID_TRACE_LOG          3
#define ID_TRACE_LOG1         4
#define FUNC_NONE             0x000
#define FUNC_OPEN             0x010
#define FUNC_CLOSE            0x011
#define FUNC_QUEUE            0x012
#define FUNC_CALLBACK         0x014
#define FUNC_HDMISET          0x015
#define FUNC_NOTIFY           0x016
#define FUNC_WQ_CREATE        0x020
#define FUNC_WQ_DELETE        0x021
#define FUNC_WQ_BLEND         0x022
#define FUNC_WQ_DISP          0x023
#define FUNC_WQ_CREATE_HDMI   0x025
#define FUNC_WQ_DELETE_HDMI   0x026
#define FUNC_WQ_BLEND_HDMI    0x027
#define FUNC_WQ_DISP_HDMI     0x028
#define FUNC_WQ_DISP_HDMICOMP 0x029

#define TRACE_ENTER(ID) \
	sh_mobile_composer_tracelog_record(ID_TRACE_ENTER, __LINE__, ID, 0);
#define TRACE_LEAVE(ID) \
	sh_mobile_composer_tracelog_record(ID_TRACE_LEAVE, __LINE__, ID, 0);
#define TRACE_LOG(ID) \
	sh_mobile_composer_tracelog_record(ID_TRACE_LOG,   __LINE__, ID, 0);
#define TRACE_LOG1(ID, VAL1) \
	sh_mobile_composer_tracelog_record(ID_TRACE_LOG1, __LINE__, ID, VAL1);

#endif

/******************************/
/* define external function   */
/******************************/
#if INTERNAL_DEBUG
extern void sh_mobile_composer_tracelog_record(
	int logclass, int line, int ID, int val);

static void sh_mobile_composer_tracelog_init(void);

#if INTERNAL_DEBUG_USE_DEBUGFS
static int sh_mobile_composer_dump_rhandle(char *p, int n,
	struct composer_rh *rh);
#endif

static void sh_mobile_composer_dump_information(int flag);
static void sh_mobile_composer_debug_info_static(struct seq_file *s);
#if INTERNAL_DEBUG_USE_DEBUGFS
static void sh_mobile_composer_debug_info_queue(struct seq_file *s);
#endif
#endif

/* graphics */
static void dump_screen_grap_initialize(screen_grap_initialize *arg);
static void dump_screen_grap_image_blend(screen_grap_image_blend *arg);
static void dump_screen_grap_set_blend_size(screen_grap_set_blend_size *arg);
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
static void dump_screen_grap_image_output(screen_grap_image_output *arg);
#endif
static void dump_screen_grap_quit(screen_grap_quit *arg);
static void dump_screen_grap_delete(screen_grap_delete *arg);
static const char *get_RTAPImsg_graphics(int rc);

/* display */
static void dump_screen_disp_set_address(screen_disp_set_address *arg);
static const char *get_RTAPImsg_display(int rc);

/* memory */
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
static void dump_system_mem_phy_change_rtaddr( \
	system_mem_phy_change_rtaddr *arg);
static const char *get_RTAPImsg_memory(int rc);
#endif

#if _TIM_DBG
/* timerecord */
static void *timerecord_createhandle(void);
static void timerecord_reset(void *);
static void timerecord_set(void *, int id, ktime_t time);
static unsigned int timerecord_record(void *, int id);
static void timerecord_print(void *);
#endif

#endif
