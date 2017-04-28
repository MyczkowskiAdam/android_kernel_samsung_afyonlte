/*
 * screen_display_private.h
 *  screen display private header file.
 *
 * Copyright (C) 2012-2013 Renesas Electronics Corporation
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
#ifndef __SCREEN_DISPLAY_PRIVATE_H__
#define __SCREEN_DISPLAY_PRIVATE_H__

#define RT_DISPLAY_LONGPACKETMEMSIZE	(65535)

#define RT_DISPLAY_LONGPACKETSIZE	(1996)

#define RT_DISPLAY_SCRNDATAINFO_SIZE	(16)
#define RT_DISPLAY_LCD1_OFFSET		(4)

#define RT_DISPLAY_WAIT_TIMEOUT		(40000)

#define	DISPLAY_DOWN_TIMEOUT(sem)					 \
{									 \
if (0 != down_timeout(sem, msecs_to_jiffies(RT_DISPLAY_WAIT_TIMEOUT))) \
		panic("[%s][%d] : down_timeout TIMEOUT Error!\n",	 \
		__func__, __LINE__);					 \
}

typedef struct {
	void *handle;
	void *rtds_mem_handle;
} screen_disp_handle;

typedef struct {
	unsigned short    height;
	unsigned short    width;
	unsigned short    stride;
	unsigned short    mode;
} screen_display_screen_data_info;

struct iccom_wq_system_mem_rt_map {
	struct semaphore	sem;
	system_mem_rt_map	*sys_rt_map;
	int					result;
};

int screen_display_get_screen_data_info(int, screen_display_screen_data_info *);

#endif /* __SCREEN_DISPLAY_PRIVATE_H__ */

