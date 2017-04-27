/*
 * rcvry_drv_private.h.h
 *	 Real Time Domain Recovery Driver API function file.
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

#ifndef __RCVRY_DRV_PRIVATE_H__
#define __RCVRY_DRV_PRIVATE_H__

/* Resource Release Notify */
#define	TASK_RCVRY01			(72)
/* Function ID Base */
#define	FUNCTIONID_RCVRY01_BASE	((TASK_RCVRY01*256) + 3)
/* Event ID */
#define	EVENT_TASK_RESOURCE_RELEASE	(FUNCTIONID_RCVRY01_BASE + 30)

/* Recovery Task01 ID */
#define	TASK_RCVRY01			(72)
/* Function ID Base */
#define	FUNCTIONID_RCVRY01_BASE	((TASK_RCVRY01*256) + 3)
/* Event ID */
#define	EVENT_TASK_RCVRY01_ID	(FUNCTIONID_RCVRY01_BASE + 2)

/* Recovery Task02 ID */
#define	TASK_RCVRY02			(88)
/* Function ID Base */
#define	FUNCTIONID_RCVRY02_BASE	((TASK_RCVRY02*256) + 3)
/* Event ID */
#define	EVENT_TASK_RCVRY02_ID	(FUNCTIONID_RCVRY02_BASE + 3)

/* Recovery Task03 ID */
#define	TASK_RCVRY03			(74)
/* Function ID Base */
#define	FUNCTIONID_RCVRY03_BASE	((TASK_RCVRY03*256) + 3)
/* Event ID */
#define	EVENT_TASK_RCVRY03_ID	(FUNCTIONID_RCVRY03_BASE + 3)

#define	RECOVERY_DRIVER_INFO_MAX	(32)

struct rcvry_delete {
	void	*handle;
} ;

struct rcvry_handle {
	void	*handle;
} ;

struct rcvry_info {
	pid_t				pid;
	long				standby_counter;
	unsigned long		killable_flag;
	struct completion	rcvry_completion;
	struct file			*rcvry_fp;
	unsigned long		cancel_flag;
} rcvry_info;

#endif /* __RCVRY_DRV_PRIVATE_H__ */

