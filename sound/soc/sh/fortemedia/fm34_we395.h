/* drivers/misc/2mic/fm34_we395.h
 * sound/soc/sh/fortmedia/fm34_we395.c - fm34_we395 voice processor driver
 *
 * Copyright (C) 2012 Samsung Corporation.
 * Copyright (C) 2013 Renesas Mobile Corp.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __FEM34_WE395_H__
#define __FEM34_WE395_H__

#include <linux/i2c/fm34_we395.h>

#include "./fm34_cmd/fm34_we395_cmd.h"

#define FM34_WAIT_TIME 0
#define FM34_BYPASS_ONLY 0

#if FM34_WAIT_TIME
/* Detasheet wait time */
#define FM34_TRST_US		120
#define FM34_TSU_RST2PP		2000
#define FM34_TSU_PP2PD		2000
#else
/* Sample code wait time */
#define FM34_TRST_US		5000
#define FM34_TSU_RST2PP		5000
#define FM34_TSU_PP2PD		20000
#endif

/* vclk4 */
#define FM34_VCLKCR4_REGSIZE	0x4

/* GPIO */
#define FM34_PWDN_ENABLE	0
#define FM34_PWDN_DISABLE	1
#define FM34_BP_ENABLE		0
#define FM34_BP_DISABLE		1
#define FM34_RST_ENABLE		0
#define FM34_RST_DISABLE	1
#define FM34_AVDD_ENABLE	1
#define FM34_AVDD_DISABLE	0


#define __FM34_PRINT__	1
extern unsigned int g_fm34_log_level;

#define FM34_LOG_NONE	0x0
#define FM34_LOG_ERR	0x1
#define FM34_LOG_IF	0x2
#define FM34_LOG_DEBUG	0x4
#define FM34_LOG_INFO	0x8
#define FM34_LOG_FUNC	0x10
#define FM34_LOG_ALL	0x1F
#define FM34_LOG_LEVEL_CHECK	0xffff0000

#ifdef __FM34_PRINT__
#define fm34_pr_err(fmt, ...) { \
	if (g_fm34_log_level & FM34_LOG_ERR) { \
		pr_alert( \
			FM34_MODULE_NAME \
			" : [error] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#define fm34_pr_if_start(fmt, ...) { \
	if ((g_fm34_log_level & FM34_LOG_IF) || \
		(g_fm34_log_level & FM34_LOG_FUNC)) { \
		pr_alert( \
			FM34_MODULE_NAME \
			" : [start] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#define fm34_pr_if_end(fmt, ...) { \
	if ((g_fm34_log_level & FM34_LOG_IF) || \
		(g_fm34_log_level & FM34_LOG_FUNC)) { \
		pr_alert( \
			FM34_MODULE_NAME \
			" : [end] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#define fm34_pr_debug(fmt, ...) { \
	if (g_fm34_log_level & FM34_LOG_DEBUG) { \
		pr_alert( \
			FM34_MODULE_NAME \
			" : [debug] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#define fm34_pr_info(fmt, ...) { \
	if (g_fm34_log_level & FM34_LOG_INFO) { \
		pr_alert( \
			FM34_MODULE_NAME \
			" : [info] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#define fm34_pr_func_start(fmt, ...) { \
	if (g_fm34_log_level & FM34_LOG_FUNC) { \
		pr_alert( \
			FM34_MODULE_NAME \
			" : [start] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#define fm34_pr_func_end(fmt, ...) { \
	if (g_fm34_log_level & FM34_LOG_FUNC) { \
		pr_alert( \
			FM34_MODULE_NAME \
			" : [end] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#else

#define fm34_pr_err(fmt, ...)		{}
#define fm34_pr_if_start(fmt, ...)	{}
#define fm34_pr_if_end(fmt, ...)	{}
#define fm34_pr_debug(fmt, ...)		{}
#define fm34_pr_info(fmt, ...)		{}
#define fm34_pr_func_start(fmt, ...)	{}
#define fm34_pr_func_end(fmt, ...)	{}

#endif /* __FM34_PRINT__ */

#endif
