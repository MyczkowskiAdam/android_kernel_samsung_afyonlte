/* tpa2026-i2c.h
 *
 * Copyright (C) 2013 Renesas Mobile Corp.
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

#ifndef __TPA2026_I2C_H__
#define __TPA2026_I2C_H__

/* variable of log level */
extern unsigned int g_tpa2026_i2c_log_level;

/* tpa2026 state */
#define TPA2026_I2C_DISABLE		0
#define TPA2026_I2C_ENABLE		1

/* output device */
#define TPA2026_I2C_INPUT_DEVICE	(SNDP_IN_COMMUNICATION | \
					SNDP_IN_AMBIENT | \
					SNDP_IN_BUILTIN_MIC | \
					SNDP_IN_BLUETOOTH_SCO_HEADSET | \
					SNDP_IN_WIRED_HEADSET | \
					SNDP_IN_AUX_DIGITAL | \
					SNDP_IN_VOICE_CALL | \
					SNDP_IN_BACK_MIC | \
					SNDP_IN_USB_HEADSET | \
					SNDP_IN_FM_RADIO_RX)

/* driver name */
#define TPA2026_I2C_DRIVER_NAME		"tpa2026"

/* log file name */
#define TPA2026_I2C_LOG_FILE_NAME	"log_level"

/* log level */
#define TPA2026_I2C_LOG_NONE		0x0
#define TPA2026_I2C_LOG_ERR		0x1
#define TPA2026_I2C_LOG_DEBUG		0x2
#define TPA2026_I2C_LOG_INFO		0x4
#define TPA2026_I2C_LOG_FUNC		0x8
#define TPA2026_I2C_LOG_ALL		0xF
#define TPA2026_I2C_LOG_LEVEL_CHECK	0xffffff00

/* log output macro */
#ifdef CONFIG_TPA2026_I2C_DBG
#define tpa2026_i2c_pr_err(fmt, ...) { \
	if (g_tpa2026_i2c_log_level & TPA2026_I2C_LOG_ERR) { \
		pr_alert( \
			TPA2026_I2C_DRIVER_NAME \
			" : [error] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#define tpa2026_i2c_pr_debug(fmt, ...) { \
	if (g_tpa2026_i2c_log_level & TPA2026_I2C_LOG_DEBUG) { \
		pr_alert( \
			TPA2026_I2C_DRIVER_NAME \
			" : [debug] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#define tpa2026_i2c_pr_info(fmt, ...) { \
	if (g_tpa2026_i2c_log_level & TPA2026_I2C_LOG_INFO) { \
		pr_alert( \
			TPA2026_I2C_DRIVER_NAME \
			" : [info] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#define tpa2026_i2c_pr_func_start(fmt, ...) { \
	if (g_tpa2026_i2c_log_level & TPA2026_I2C_LOG_FUNC) { \
		pr_alert( \
			TPA2026_I2C_DRIVER_NAME \
			" : [start] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#define tpa2026_i2c_pr_func_end(fmt, ...) { \
	if (g_tpa2026_i2c_log_level & TPA2026_I2C_LOG_FUNC) { \
		pr_alert( \
			TPA2026_I2C_DRIVER_NAME \
			" : [end] %s: " \
			fmt, \
			__func__, \
			##__VA_ARGS__); \
	} \
}
#else
#define tpa2026_i2c_pr_err(fmt, ...)		{}
#define tpa2026_i2c_pr_debug(fmt, ...)		{}
#define tpa2026_i2c_pr_info(fmt, ...)		{}
#define tpa2026_i2c_pr_func_start(fmt, ...)	{}
#define tpa2026_i2c_pr_func_end(fmt, ...)	{}
#endif /* CONFIG_TPA2026_I2C_DBG */


struct tpa2026_i2c_platform_data {
	int gpio_shdn;
};


struct tpa2026_i2c_data {
	struct i2c_client *i2c_client;
	struct tpa2026_i2c_platform_data *pdata;
	struct miscdevice device;
};


#endif /* __TPA2026_I2C_H__ */
