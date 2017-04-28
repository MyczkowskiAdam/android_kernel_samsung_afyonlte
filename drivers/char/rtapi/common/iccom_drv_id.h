/*
 * iccom_drv_id.h
 *     function id definition header file.
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

#ifndef __ICCOM_DRV_ID_H__
#define __ICCOM_DRV_ID_H__

/*
 * Task ID
 */
/* Stcon Task */
/* Task ID */
#define TASK_STATUS_CONTROL             (1)
/* Function ID Base */
#define FUNCTIONID_STATUS_CONTROL_BASE  (TASK_STATUS_CONTROL*256+3)
/* Function ID */
#define EVENT_STATUS_INITTSKLVL				(FUNCTIONID_STATUS_CONTROL_BASE+2)
#define EVENT_STATUS_STANDBYCONTROL			(FUNCTIONID_STATUS_CONTROL_BASE+3)
#define EVENT_STATUS_STARTPOWERAREANOTIFY	(FUNCTIONID_STATUS_CONTROL_BASE+4)
#define EVENT_STATUS_ENDPOWERAREANOTIFY		(FUNCTIONID_STATUS_CONTROL_BASE+5)

/* Debug */
/* Debug Task */
/* Task ID */
#define TASK_DEBUG (5)
/* Function ID Base */
#define FUNCTIONID_DEBUG_BASE (TASK_DEBUG*256+3)

/* Debug */
#define EVENT_DEBUG_STARTOUTPUTLOG   (FUNCTIONID_DEBUG_BASE+11)

#endif /* __ICCOM_DRV_ID_H__ */
