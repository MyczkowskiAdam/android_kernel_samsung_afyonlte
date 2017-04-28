/*
 * iccom_drv_private.h
 *     Inter Core Communication driver private header file.
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
#ifndef __ICCOM_DRV_PRIVATE_H__
#define __ICCOM_DRV_PRIVATE_H__

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/completion.h>

/*** define ***/
#define ICCOM_CMD_AREA_SIZE         2048        /* size of command transfer area */
#define ICCOM_CMD_INFO_SIZE         8           /* size of information area */
#define ICCOM_CMD_HEADER_SIZE       20          /* size of message header */
#define ICCOM_CMD_DATA_SIZE         (ICCOM_CMD_AREA_SIZE-(ICCOM_CMD_INFO_SIZE+ICCOM_CMD_HEADER_SIZE))   /* data size of the first transmission */
#define ICCOM_CMD_BLOCK_SIZE        (ICCOM_CMD_AREA_SIZE-ICCOM_CMD_INFO_SIZE)   /* data size of the transmission after the second */
#define ICCOM_CMD_AREA_NUM          4           /* number of command transfer area */

/* return code */
#define SMAP_QUEUE_NG               -32
#define SMAP_SYNC_TIMEOUT           -33

/* command transfer area offset */
#define ICCOM_CMD_AREA_X1           0x0000
#define ICCOM_CMD_AREA_X2           0x0800
#define ICCOM_CMD_AREA_Y1           0x1000
#define ICCOM_CMD_AREA_Y2           0x1800

/* type of address space */
#define ICCOM_TYPE_KERNEL           0
#define ICCOM_TYPE_USER             1

#define ICCOM_ERROR_DATA_AREA_MAX   4           /* number of error data area */

/* type of memory */
#define ICCOM_MEM_DYNAMIC           1
#define ICCOM_MEM_STATIC            0

/* response kind */
#define ICCOM_RESP_KIND_ON          1
#define ICCOM_RESP_KIND_OFF         0

#define ICCOM_ASYNC_RECV_CANCEL     1           /* asynchronous receive status */

#define ICCOM_SYNC_TIMEOUT          40000       /* timeout for sync(msec) */

#define	ICCOM_DOWN_TIMEOUT(sem)						  \
{									  \
	if (0 != down_timeout(sem, msecs_to_jiffies(ICCOM_SYNC_TIMEOUT))) \
		panic("[%s][%d] : down_timeout TIMEOUT Error!\n",	  \
		__func__, __LINE__);					  \
}

/*** struct ***/
/* callback information */
typedef struct {
	void			   *user_data;			/* user data */
	void				(*module)(void		 *user_data,
								int                 result_code,
								int                 function_id,
								unsigned char      *data_addr,
								int                 data_len);	/* callback function */
} iccom_callback_info;

/* ICCOM handle */
typedef struct {
	struct completion	*sync_completion;	/* completion information for synchronous receive */
	struct completion	*async_completion;	/* completion information for asynchronous receive */
	iccom_callback_info	kernel_cb_info;		/* callback information */
	unsigned long		async_recv_status;	/* asynchronous receive status */
	void				*recv_data;			/* receive data */
} iccom_drv_handle;

/* command information */
typedef struct {
	unsigned long		total_size;			/* total data size */
	unsigned long		block_size;			/* block data size */
} iccom_cmd_info;

/* message header */
typedef struct {
	int					task_id;			/* task ID */
	int					func_id;			/* function ID */
	short				send_mode;			/* send mode */
	short				resp_kind;			/* response kind */
	void				*handle;			/* ICCOM handle */
	int					ret_code;			/* return code */
} iccom_msg_header;

/* write data information */
typedef struct {
	unsigned int		send_num;			/* number of send data */
	iccom_drv_cmd_data	*send_data;			/* send data */
	unsigned int		e_send_num;			/* number of transmitted data */
	unsigned int		e_send_posi;		/* position of transmitted data */
} iccom_write_data_info;

/* receive data information */
typedef struct {
	void			   *recv_data;			/* receive data */
	unsigned long	   recv_size;			/* receive data size */
	unsigned long	   write_size;			/* write data size */
} iccom_recv_data_info;


/* queue data */
typedef struct {
	struct list_head	queue_header;		/* queue header */
	void				*recv_data;			/* receive data */
	unsigned long		recv_size;			/* receive data size */
	struct completion	*completion;		/* completion information */
	int					eicr_result;		/* result of eicr */
} iccom_recv_queue;

/* fatal information */
typedef struct {
	void				*handle;			 /* ICCOM handle */
} iccom_fatal_info;

/* receive data information */
typedef struct {
	unsigned long		mng_info;		/* type of memory */
	iccom_cmd_info		cmd_info;		/* command information */
	iccom_msg_header	msg_header;		/* message header */
} iccom_recv_data;

/* receive data information (memory allocate error) */
typedef struct {
	unsigned long		index;			  /* index */
	iccom_recv_data		recv_data[ICCOM_ERROR_DATA_AREA_MAX];   /* receive data information */
} iccom_recv_data_err;

typedef struct {
	struct list_head	list;	/* queue header */
	void				*handle;
} iccom_handle_list;

typedef struct {
	struct list_head	list;	/* queue header */
	struct file			*fp;
} iccom_fp_list;

/**** prototype ****/
iccom_drv_handle *iccom_create_handle(int type);
void iccom_destroy_handle(iccom_drv_handle *handle);
void iccom_init_recv_queue(void);
int iccom_put_recv_queue(struct completion *completion, int eicr_result,
	void *recv_data, unsigned long recv_size);
int iccom_get_recv_queue(struct completion *completion, iccom_recv_queue **queue);
void iccom_delete_recv_queue(iccom_recv_queue *queue);
void iccom_init_fatal(void);
int iccom_put_fatal(iccom_fatal_info *fatal_info);
int iccom_get_fatal(iccom_fatal_info **fatal_info);
int iccom_comm_func_init(void);
void iccom_comm_func_quit(void);
int iccom_send_command(void *handle, int type, iccom_cmd_send_param *send_param);
int iccom_recv_command_sync(void *handle, iccom_cmd_recv_param *recv_param);
int iccom_recv_command_async(void **handle, iccom_cmd_recv_async_param *recv_param);
int iccom_recv_complete(unsigned char *recv_data);
int iccom_recv_cancel(void *handle);
irqreturn_t iccom_iccomeicr_int(int irq_number, void *device_id);
int iccom_write_command(void *handle, int type, iccom_cmd_send_param *send_param);
unsigned char __iomem *iccom_get_send_command_area(void);
int iccom_write_command_data(iccom_drv_cmd_io_data *output_area,
	iccom_write_data_info *write_data, unsigned long *out_size, int type);
void iccom_iccomiicr_int(unsigned long buff_kind,
	unsigned long buff_position);
int iccom_copy_to_command_area(void __iomem *cmd_area, void *from_addr,
	unsigned long size, int type);
void iccom_leak_check(iccom_drv_handle *handle);

void iccom_log_start(void);
void iccom_log_stop(void);

void iccom_debug_output_fatal_info(unsigned char *data_addr, int data_len);

extern struct completion    g_iccom_async_completion;
extern unsigned long        g_iccom_async_recv_status;

extern spinlock_t           g_iccom_lock_handle_list;
extern struct list_head     g_iccom_list_handle;

extern iccom_recv_data_info g_iccom_recv_info;
extern unsigned char __iomem *g_iccom_command_area;
extern spinlock_t           g_iccom_lock_iicr;

#endif /* __ICCOM_DRV_PRIVATE_H__ */
