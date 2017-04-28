/* vcd_common.h
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
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
#ifndef __VCD_COMMON_H__
#define __VCD_COMMON_H__

#include "linux/vcd/vcd.h"
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/time.h>

/*
 * global variable declaration
 */
extern unsigned int g_vcd_log_level;


/*
 * constant definition
 */
/* driver name */
#define VCD_DRIVER_NAME			"vcd"
#define VCD_DEVICE_NAME			"vcd_mem"

/* return values */
#define VCD_ERR_NONE			0
#define VCD_ERR_FILE_NOT_FOUND		(-ENOENT)
#define VCD_ERR_NOMEMORY		(-ENOMEM)
#define VCD_ERR_BUSY			(-EBUSY)
#define VCD_ERR_PARAM			(-EINVAL)
#define VCD_ERR_FILE_TOO_BIG		(-EFBIG)
#define VCD_ERR_NOT_SUPPORT		(-ENOTSUPP)
#define VCD_ERR_NOT_ACTIVE		(-EWOULDBLOCK)
#define VCD_ERR_FW_TIME_OUT		(-ETIMEDOUT)
#define VCD_ERR_ALREADY_EXECUTION	(-EALREADY)
#define VCD_ERR_NOW_EXECUTION		(-EINPROGRESS)
#define VCD_ERR_SYSTEM			(-EFAULT)
#define VCD_ERR_POWER_DOMAIN		(-ENODEV)
#define VCD_ERR_SET_CLK			(-EIO)
#define VCD_ERR_HWSPINLOCK_TIME_OUT	(-ETIME)
#define VCD_ERR_FORCE_END_WAIT		(-ERESTARTSYS)


/* log level */
#define VCD_LOG_NONE			0x00000000
#define VCD_LOG_ERROR			0x00000001
#define VCD_LOG_IF			0x00000002
#define VCD_LOG_LEVEL_UPPER		0x00000003
#define VCD_LOG_STATUS_CHANGE		0x00000004
#define VCD_LOG_USER_IF			0x00000008
#define VCD_LOG_SPUV_IF			0x00000008
#define VCD_LOG_INTERFACE_INFO		0x00000010
#define VCD_LOG_CONTROL_INFO		0x00000020
#define VCD_LOG_SPUV_INFO		0x00000040
#define VCD_LOG_REGISTERS_DUMP		0x00000080
#define VCD_LOG_INTERFACE_DEBUG		0x00000100
#define VCD_LOG_CONTROL_DEBUG		0x00000200
#define VCD_LOG_SPUV_DEBUG		0x00000400
#define VCD_LOG_IRQ_DEBUG		0x00000800
#define VCD_LOG_INTERFACE_FUNCTION	0x00001000
#define VCD_LOG_CONTROL_FUNCTION	0x00002000
#define VCD_LOG_SPUV_FUNCTION		0x00004000
#define VCD_LOG_IRQ_FUNCTION		0x00008000
#define VCD_LOG_TRIGGER_COUNT		0x00010000
#define VCD_LOG_ON_SYSTEM_INFO_IND	0x00100000
#define VCD_LOG_ON_UDATA_IND		0x00200000
#define VCD_LOG_ON_TRIGGER_REC_IND	0x00400000
#define VCD_LOG_ON_TRIGGER_PLAY_IND	0x00800000
#define VCD_LOG_LEVEL_TIMESTAMP		0x20000000
#define VCD_LOG_LEVEL_CONDITION_LOCK	0x40000000
#define VCD_LOG_LEVEL_LOCK		0x80000000

/* trigger log type */
#define VCD_LOG_TRIGGER_REC		0x00000001
#define VCD_LOG_TRIGGER_PLAY		0x00000002

/*
 * define macro declaration
 */

/* panic macro */
#define vcd_kern_fault(fmt, ...)	panic(fmt "(%d)", ##__VA_ARGS__)

/* dynamic log level change */
#ifdef __PRINT_VCD__
	#define vcd_pr_always(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
			struct timeval tv; \
			do_gettimeofday(&tv); \
			pr_alert( \
				"[%5ld.%06ld] " \
				VCD_DRIVER_NAME \
				" : %s: " \
				fmt, \
				tv.tv_sec, \
				tv.tv_usec, \
				__func__, \
				##__VA_ARGS__); \
		} else { \
			pr_alert( \
				VCD_DRIVER_NAME \
				" : %s: " \
				fmt, \
				__func__, \
				##__VA_ARGS__); \
		} \
	}

	#define vcd_pr_always_err(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
			struct timeval tv; \
			do_gettimeofday(&tv); \
			pr_alert( \
				"[%5ld.%06ld] " \
				VCD_DRIVER_NAME \
				" : %s: " \
				fmt, \
				tv.tv_sec, \
				tv.tv_usec, \
				__func__, \
				##__VA_ARGS__); \
		} else { \
			pr_alert( \
				VCD_DRIVER_NAME \
				" : %s: " \
				fmt, \
				__func__, \
				##__VA_ARGS__); \
		} \
	}

	#define vcd_pr_err(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_ERROR) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert(  \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [error] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [error] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_if_pt(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_IF) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_if_sound(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_IF) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_if_amhal(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_IF) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_if_pm_runtime(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_IF) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_if_spuv(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_IF) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_if_spuv_system_info_ind(fmt, ...) { \
		if ((g_vcd_log_level & VCD_LOG_IF) && \
		(g_vcd_log_level & VCD_LOG_ON_SYSTEM_INFO_IND)) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_if_spuv_udata_ind(fmt, ...) { \
		if ((g_vcd_log_level & VCD_LOG_IF) && \
		(g_vcd_log_level & VCD_LOG_ON_UDATA_IND)) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_if_spuv_trigger_rec_ind(fmt, ...) { \
		if ((g_vcd_log_level & VCD_LOG_IF) && \
		(g_vcd_log_level & VCD_LOG_ON_TRIGGER_REC_IND)) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_if_spuv_trigger_play_ind(fmt, ...) { \
		if ((g_vcd_log_level & VCD_LOG_IF) && \
		(g_vcd_log_level & VCD_LOG_ON_TRIGGER_PLAY_IND)) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_start_if_user(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_USER_IF) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_end_if_user(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_USER_IF) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_start_if_spuv(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_SPUV_IF) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_end_if_spuv(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_SPUV_IF) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_status_change(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_STATUS_CHANGE) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [state] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [state] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_interface_info(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_INTERFACE_INFO) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [info ] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [info ] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_control_info(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_CONTROL_INFO) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [info ] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [info ] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_spuv_info(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_SPUV_INFO) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [info ] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [info ] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_registers_dump(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_REGISTERS_DUMP) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [ reg ] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [ reg ] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_interface_debug(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_INTERFACE_DEBUG) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [debug] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [debug] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_control_debug(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_CONTROL_DEBUG) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [debug] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [debug] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_spuv_debug(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_SPUV_DEBUG) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [debug] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [debug] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_irq_debug(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_IRQ_DEBUG) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [debug] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [debug] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_start_interface_function(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_INTERFACE_FUNCTION) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_end_interface_function(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_INTERFACE_FUNCTION) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_start_control_function(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_CONTROL_FUNCTION) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_end_control_function(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_CONTROL_FUNCTION) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_start_spuv_function(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_SPUV_FUNCTION) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_end_spuv_function(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_SPUV_FUNCTION) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_start_irq_function(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_IRQ_FUNCTION) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [start] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_end_irq_function(fmt, ...) { \
		if (g_vcd_log_level & VCD_LOG_IRQ_FUNCTION) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					__func__, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : [ end ] %s: " \
					fmt, \
					__func__, \
					##__VA_ARGS__); \
			} \
		} \
	}

	#define vcd_pr_trigger_count(fmt, ...) { \
		if ((g_vcd_log_level & VCD_LOG_IF) && \
		(g_vcd_log_level & VCD_LOG_TRIGGER_COUNT)) { \
			if (g_vcd_log_level & VCD_LOG_LEVEL_TIMESTAMP) { \
				struct timeval tv; \
				do_gettimeofday(&tv); \
				pr_alert( \
					"[%5ld.%06ld] " \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					tv.tv_sec, \
					tv.tv_usec, \
					##__VA_ARGS__); \
			} else { \
				pr_alert( \
					VCD_DRIVER_NAME \
					" : " \
					fmt, \
					##__VA_ARGS__); \
			} \
		} \
	}

#else
	#define vcd_pr_always(fmt, ...)				{}
	#define vcd_pr_always_err(fmt, ...)			{}
	#define vcd_pr_err(fmt, ...)				{}
	#define vcd_pr_if_pt(fmt, ...)				{}
	#define vcd_pr_if_sound(fmt, ...)			{}
	#define vcd_pr_if_amhal(fmt, ...)			{}
	#define vcd_pr_if_pm_runtime(fmt, ...)			{}
	#define vcd_pr_if_spuv(fmt, ...)			{}
	#define vcd_pr_if_spuv_system_info_ind(fmt, ...)	{}
	#define vcd_pr_if_spuv_udata_ind(fmt, ...)		{}
	#define vcd_pr_if_spuv_trigger_rec_ind(fmt, ...)	{}
	#define vcd_pr_if_spuv_trigger_play_ind(fmt, ...)	{}
	#define vcd_pr_start_if_user(fmt, ...)			{}
	#define vcd_pr_end_if_user(fmt, ...)			{}
	#define vcd_pr_start_if_spuv(fmt, ...)			{}
	#define vcd_pr_end_if_spuv(fmt, ...)			{}
	#define vcd_pr_status_change(fmt, ...)			{}
	#define vcd_pr_interface_info(fmt, ...)			{}
	#define vcd_pr_control_info(fmt, ...)			{}
	#define vcd_pr_spuv_info(fmt, ...)			{}
	#define vcd_pr_registers_dump(fmt, ...)			{}
	#define vcd_pr_interface_debug(fmt, ...)		{}
	#define vcd_pr_control_debug(fmt, ...)			{}
	#define vcd_pr_spuv_debug(fmt, ...)			{}
	#define vcd_pr_irq_debug(fmt, ...)			{}
	#define vcd_pr_start_interface_function(fmt, ...)	{}
	#define vcd_pr_end_interface_function(fmt, ...)		{}
	#define vcd_pr_start_control_function(fmt, ...)		{}
	#define vcd_pr_end_control_function(fmt, ...)		{}
	#define vcd_pr_start_spuv_function(fmt, ...)		{}
	#define vcd_pr_end_spuv_function(fmt, ...)		{}
	#define vcd_pr_start_irq_function(fmt, ...)		{}
	#define vcd_pr_end_irq_function(fmt, ...)		{}
	#define vcd_pr_trigger_count(fmt, ...)			{}
#endif  /* __PRINT_VCD__ */


/*
 * enum declaration
 */
enum VCD_CALL_TYPE {
	VCD_CALL_TYPE_CS = 0,
	VCD_CALL_TYPE_VOIP,
	VCD_CALL_TYPE_VOLTE,
	VCD_CALL_TYPE_VTCALL,
};

enum VCD_SEMAPHORE {
	VCD_SEMAPHORE_RELEASE = 0,
	VCD_SEMAPHORE_TAKE = 1,
};

enum VCD_VALIDITY {
	VCD_DISABLE = 0,
	VCD_ENABLE
};

enum VCD_SYSTEM_ERROR_TYPE {
	VCD_SYSTEM_ERROR = 0,
	VCD_TIMEOUT,
	VCD_CNF_ERROR,
	VCD_INVALID_REQ,
	VCD_WD_TIMEOUT,
};

enum VCD_BINARY_KIND {
	VCD_BINARY_SPUV = 0,
	VCD_BINARY_PCM,
	VCD_BINARY_DIAMOND,

};


/*
 * structure declaration
 */


/*
 * table declaration
 */


/*
 * prototype declaration
 */


#endif /* __VCD_COMMON_H__ */
