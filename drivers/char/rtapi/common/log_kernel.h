/*
 * log_kernel.h
 *     message log header file.
 *
 * Copyright (C) 2012 Renesas Electronics Corporation
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/kernel.h>

#ifndef __LOG_KERNEL_H__
#define __LOG_KERNEL_H__

#define MSG_LVL_FATAL	(5)
#define MSG_LVL_ERROR	(4)
#define MSG_LVL_HIGH	(3)
#define MSG_LVL_MED	(2)
#define MSG_LVL_LOW	(1)
#define MSG_LVL_INFO	(0)
#define MSG_LVL_NONE	(255)

/* message level set*/
#ifndef MSG_LEVEL
#define MSG_LEVEL MSG_LVL_NONE
#endif

/* FATAL Error priority messages */
#if MSG_LEVEL < MSG_LVL_NONE
#define MSG_FATAL(args...)	printk(KERN_ALERT args)
#else
#define MSG_FATAL(args...)
#endif

/* Error priority messages */
#if MSG_LEVEL < MSG_LVL_FATAL
#define MSG_ERROR(args...)	printk(KERN_ALERT args)
#else
#define MSG_ERROR(args...)
#endif

/* High priority messages */
#if MSG_LEVEL < MSG_LVL_ERROR
#define MSG_HIGH(args...)	printk(KERN_ALERT args)
#else
#define MSG_HIGH(args...)
#endif

/* Medium priority messages */
#if MSG_LEVEL < MSG_LVL_HIGH
#define MSG_MED(args...)	printk(KERN_ALERT args)
#else
#define MSG_MED(args...)
#endif

/* Low priority messages */
#if MSG_LEVEL < MSG_LVL_MED
#define MSG_LOW(args...)	printk(KERN_ALERT args)
#else
#define MSG_LOW(args...)
#endif

/* INFO priority messages */
#if MSG_LEVEL < MSG_LVL_LOW
#define MSG_INFO(args...)	printk(KERN_ALERT args)
#else
#define MSG_INFO(args...)
#endif

#endif /* __LOG_KERNEL_H__ */
