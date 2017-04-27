/*
 * Thermal Sensor Driver
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */
 
#ifndef __THS_MAIN_H__
#define __THS_MAIN_H__

#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/thermal_sensor/ths_kernel.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
/* #define CONFIG_THS_DEBUG_ENABLE */
#ifdef CONFIG_THS_DEBUG_ENABLE
#define THS_DEBUG_MSG(...) printk(KERN_INFO __VA_ARGS__)
#define THS_ERROR_MSG(...) printk(KERN_ERR __VA_ARGS__)
#else
#define THS_DEBUG_MSG(...) while(0)
#define THS_ERROR_MSG(...) while(0)
#endif

enum mode;

/* Define structure, enum */
struct thermal_sensor {
	void __iomem 			*iomem_base;	/* The base address of Thermal Sensor module */
	struct platform_device  	*pdev;
	struct mutex 			sensor_mutex;
	struct device 			*dev;
	struct work_struct 		tj0_work;
	struct work_struct 		tj1_work;
	struct work_struct 		tj2_work;
	struct work_struct      tj3_work;
	struct delayed_work		work;
	struct workqueue_struct 	*queue;
	struct thermal_sensor_data  	pdata[2];
	int 				ths_irq;
	struct clk 			*clk;
	struct early_suspend 		early_suspend;
};

/* Common functions is used by kernel and user interface */
extern int __ths_get_cur_temp(unsigned int ths_id, int *cur_temp);
extern int __ths_set_op_mode(enum mode ths_mode, unsigned int ths_id);
extern int __ths_get_op_mode(unsigned int ths_id);

#endif /* __THS_MAIN_H__ */
