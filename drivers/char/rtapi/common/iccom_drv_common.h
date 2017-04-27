/*
 * iccom_drv_common.h
 *     Inter Core Communication common definition header file.
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
#ifndef __ICCOM_DRV_COMMON_H__
#define __ICCOM_DRV_COMMON_H__

/*** struct ***/
/* ICCOM_CMD_SEND parameter */
typedef struct {
	int					task_id;		/* task ID */
	int					function_id;	/* function ID */
	short				send_mode;		/* send mode */
	unsigned int		send_num;		/* number of send data */
	iccom_drv_cmd_data	*send_data;		/* send data */
} iccom_cmd_send_param;

/* ICCOM_CMD_RECV parameter */
typedef struct {
	int					result_code;	/* RT Domain result */
	int					function_id;	/* function ID */
	unsigned int		recv_size;		/* receive data size */
	unsigned char		*recv_data;		/* area of receive data */
} iccom_cmd_recv_param;

/* ICCOM_CMD_RECV_ASYNC parameter */
typedef struct {
	int					result_code;	/* RT Domain result */
	int					function_id;	/* function ID */
	unsigned int		recv_size;		/* receive data size */
	unsigned char		*recv_data;		/* area of receive data (kernel area) */
} iccom_cmd_recv_async_param;

/* ICCOM_CMD_GET_RECV_DATA parameter */
typedef struct {
	unsigned char		*recv_data;		/* area of receive data (kernel area) */
	unsigned int		recv_size;		/* receive data size */
	unsigned char		*user_recv_data;/* area of receive data (user area) */
} iccom_cmd_get_recv_data_param;

/* ICCOM_CMD_GET_PID parameter */
typedef struct {
	pid_t				pid;			/* process ID */
} iccom_cmd_get_pid_param;

/* ICCOM_CMD_SET_FATAL parameter */
typedef struct {
	unsigned int		kind;		   /* kind of fatal infomation registration */
} iccom_cmd_set_fatal_param;

typedef struct {
	int			async_priority; /* priority */
} iccom_cmd_set_thread_param;

/*** union ***/
/* ioctl command parameter */
typedef union {
	iccom_cmd_send_param			send;		/* ICCOM_CMD_SEND parameter */
	iccom_cmd_recv_param			recv;		/* ICCOM_CMD_RECV parameter */
	iccom_cmd_recv_async_param		recv_async;	/* ICCOM_CMD_RECV_ASYNC parameter */
	iccom_cmd_get_recv_data_param	get_data;	/* ICCOM_CMD_GET_RECV_DATA parameter */
	iccom_cmd_get_pid_param			get_pid;	/* ICCOM_CMD_GET_PID parameter */
	iccom_cmd_set_fatal_param		set_fatal;	/* ICCOM_CMD_SET_FATAL parameter */
	iccom_cmd_set_thread_param		set_thread;	/* ICCOM_CMD_SET_THREAD parameter */
} iccom_cmd_param;

/*** enum ***/
/* ioctl command number */
enum {
	ICCOM_CMD_NUM_SEND = 1,
	ICCOM_CMD_NUM_RECV,
	ICCOM_CMD_NUM_RECV_ASYNC,
	ICCOM_CMD_NUM_CANCEL_RECV_ASYNC,
	ICCOM_CMD_NUM_GET_RECV_DATA,
	ICCOM_CMD_NUM_GET_PID,
	ICCOM_CMD_NUM_SET_FATAL,
	ICCOM_CMD_NUM_SET_THREAD,
	ICCOM_CMD_NUM_ACTIVE,
	ICCOM_CMD_NUM_STANDBY_NG,
	ICCOM_CMD_NUM_STANDBY_NG_CANCEL,

	ICCOM_CMD_NUM_END
};

/*** define ***/
/* ioctl magic number */
#define ICCOM_CMD_MAGIC 'i'

/* ioctl command */
#define ICCOM_CMD_SEND                      _IOW(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_SEND, iccom_cmd_send_param)
#define ICCOM_CMD_RECV                      _IOR(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_RECV, iccom_cmd_recv_param)
#define ICCOM_CMD_RECV_ASYNC                _IOR(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_RECV_ASYNC, iccom_cmd_recv_async_param)
#define ICCOM_CMD_CANCEL_RECV_ASYNC         _IO(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_CANCEL_RECV_ASYNC)
#define ICCOM_CMD_GET_RECV_DATA             _IOR(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_GET_RECV_DATA, iccom_cmd_get_recv_data_param)
#define ICCOM_CMD_GET_PID                   _IOR(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_GET_PID, iccom_cmd_get_pid_param)
#define ICCOM_CMD_SET_FATAL                 _IOW(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_SET_FATAL, iccom_cmd_set_fatal_param)
#define ICCOM_CMD_SET_THREAD                _IOW(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_SET_THREAD, iccom_cmd_set_thread_param)
#define ICCOM_CMD_ACTIVE                    _IO(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_ACTIVE)
#define ICCOM_CMD_STANDBY_NG                _IO(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_STANDBY_NG)
#define ICCOM_CMD_STANDBY_NG_CANCEL         _IO(ICCOM_CMD_MAGIC, ICCOM_CMD_NUM_STANDBY_NG_CANCEL)

#endif /* __ICCOM_DRV_COMMON_H__ */
