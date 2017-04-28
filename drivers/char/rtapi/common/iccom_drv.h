/*
 * iccom_drv.h
 *     Inter Core Communication device driver API function header file.
 *
 * Copyright (C) 2012,2013 Renesas Electronics Corporation
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
#ifndef __ICCOM_DRV_H__
#define __ICCOM_DRV_H__

#define init_MUTEX(sem) sema_init(sem, 1)
#define init_MUTEX_LOCKED(sem) sema_init(sem, 0)

/*** define ***/
#define ICCOM_ENABLE_STANDBYCONTROL

/* return code */
#define SMAP_OK                     (0)
#define SMAP_NG                     (-1)
#define SMAP_PARA_NG                (-2)
#define SMAP_MEMORY                 (-3)
#define SMAP_ALREADY_EXIST          (-4)
#define SMAP_LIB_STCON_INUSE        (-5)
#define SMAP_LIB_STCON_BUSY         (-8)
#define SMAP_ALREADY_STANDBY        (-10)
#define SMAP_ERROR_DIED             (-13)

#define ICCOM_DRV_SYSTEM_ERR        -256

#define ICCOM_DRV_STATE_LCD_ON        (1)
#define ICCOM_DRV_STATE_LCD_OFF       (2)
#define ICCOM_DRV_STATE_LCD_REFRESH   (3)

/*** enum ***/
/* send mode */
typedef enum {
	ICCOM_DRV_SYNC = 0,				/* synchronous communication */
	ICCOM_DRV_ASYNC,				/* asynchronous communication */
	ICCOM_DRV_ASYNC_NO_RET			/* asynchronous communication (no return) */
} iccom_drv_send_mode;

/* kind of fatal information registration */
typedef enum {
	ICCOM_DRV_FATAL_DELETE = 0,		/* delete fatal information */
	ICCOM_DRV_FATAL_ENTRY			/* entry fatal information */
} iccom_drv_fatal_kind;

/*** struct ***/
/* transmission data information */
typedef struct {
	unsigned int		size;		/* data size */
	unsigned int		*data;		/* data address */
} iccom_drv_cmd_data;

typedef struct {
	unsigned int		size;		/* data size */
	void __iomem		*data;		/* data address */
} iccom_drv_cmd_io_data;

/* iccom_drv_init() parameter */
typedef struct {
	void			   *user_data;	/* user data */
	void			(*comp_notice)(void *user_data, /* callback function */
									int result_code,
									int function_id,
									unsigned char *data_addr,
									int data_len);
} iccom_drv_init_param;

/* iccom_drv_cleanup() parameter */
typedef struct {
	void			   *handle;		 /* ICCOM handle */
} iccom_drv_cleanup_param;

/* iccom_drv_send_command() parameter */
typedef struct {
	void				*handle;		/* ICCOM handle */
	int					task_id;		/* task ID */
	int					function_id;	/* function ID */
	short				send_mode;		/* send mode */
	short				dummy;			/* dummy (not use) */
	unsigned int		send_size;		/* send data size */
	unsigned char		*send_data;		/* send data */
	unsigned int		recv_size;		/* receive data size */
	unsigned char		*recv_data;		/* receive data */
} iccom_drv_send_cmd_param;

/* iccom_drv_send_command_array() parameter */
typedef struct {
	void				*handle;		/* ICCOM handle */
	int					task_id;		/* task ID */
	int					function_id;	/* function ID */
	short				send_mode;		/* send mode */
	short				dummy;			/* dummy (not use) */
	unsigned int		send_num;		/* number of send data */
	iccom_drv_cmd_data	*send_data;		/* send data */
	unsigned int		recv_size;		/* receive data size */
	unsigned char		*recv_data;		/* receive data */
} iccom_drv_send_cmd_array_param;

/*** prototype ***/
void *iccom_drv_init(iccom_drv_init_param *iccom_init);
void iccom_drv_cleanup(iccom_drv_cleanup_param *iccom_cleanup);
int iccom_drv_send_command(iccom_drv_send_cmd_param *iccom_send_cmd);
int iccom_drv_send_command_array(iccom_drv_send_cmd_array_param	*iccom_send_cmd_array);

#endif /* __ICCOM_DRV_H__ */
