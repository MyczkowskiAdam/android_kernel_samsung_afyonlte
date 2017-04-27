/*
 * iccom_drv_recv.c
 *     Inter Core Communication driver function file for reception.
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
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/irqreturn.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include "log_kernel.h"
#include "iccom_hw.h"
#include "iccom_drv.h"
#include "iccom_drv_common.h"
#include "iccom_drv_private.h"
#ifdef ICCOM_ENABLE_STANDBYCONTROL
#include "iccom_drv_standby_private.h"
#endif

/******************************************************************************/
/* Function   : iccom_recv_command_sync                                       */
/* Description: receive synchronous command                                   */
/******************************************************************************/
int iccom_recv_command_sync(void *handle, iccom_cmd_recv_param *recv_param)
{
	int ret_code = SMAP_OK;

	iccom_drv_handle *drv_handle;
	iccom_recv_queue *recv_queue;
	iccom_recv_data *cmd_data;
	unsigned long recv_size = 0;
	unsigned long timeout = 0;

	if (NULL == recv_param) {
		MSG_ERROR("[ICCOMK]ERR|[%s] Parameter Error! recv_param is NULL.\n", __func__);
		ret_code					= SMAP_PARA_NG;
		goto out2;
	}

	if (NULL == handle) {
		MSG_ERROR("[ICCOMK]ERR|[%s] Parameter Error! handle is NULL.\n", __func__);
		recv_param->result_code		= 0;
		recv_param->recv_size		= recv_size;						/* 0 */
		recv_param->recv_data		= NULL;
		ret_code					= SMAP_PARA_NG;
		goto out2;
	}

	MSG_MED("[ICCOMK]INF|[%s] function_id [%d]\n", __func__, recv_param->function_id);

	drv_handle = (iccom_drv_handle *)handle;

	timeout = wait_for_completion_timeout(drv_handle->sync_completion, msecs_to_jiffies(ICCOM_SYNC_TIMEOUT));
	if (0 == timeout) {
		MSG_ERROR("[ICCOMK]ERR|[%s] wait_for_completion_timeout(timeout)(%d)\n", __func__, (unsigned int)timeout);
		recv_param->result_code		= 0;
		recv_param->recv_size		= recv_size;						/* 0 */
		recv_param->recv_data		= NULL;
		ret_code					= SMAP_SYNC_TIMEOUT;
		goto out2;
	} else {
		/* receive an interrupt */
		MSG_INFO("[ICCOMK]INF|[%s] wait_for_completion_timeout(interrupt)(%d)\n", __func__, (unsigned int)timeout);
	}

	/* get receive queue */
	ret_code = iccom_get_recv_queue(drv_handle->sync_completion, &recv_queue);
	if (SMAP_OK != ret_code) {
		MSG_ERROR("[ICCOMK]ERR|[%s] Queue did not exist.\n", __func__);
		/* queue is not found */
		recv_param->result_code		= 0;
		recv_param->recv_size		= recv_size;						/* 0 */
		recv_param->recv_data		= NULL;
		ret_code					= SMAP_NG;
		goto out2;
	}

	cmd_data = (iccom_recv_data *)recv_queue->recv_data;

	if (SMAP_OK != recv_queue->eicr_result) {
		MSG_ERROR("[ICCOMK]ERR|[%s] Error in the driver(%d).\n", __func__, (unsigned int)recv_queue->eicr_result);
		/* EICR result is NG */
		recv_param->result_code		= SMAP_NG;
		recv_param->recv_size		= recv_size;						/* 0 */
		recv_param->recv_data		= NULL;
		ret_code					= recv_queue->eicr_result;			/* error code */
		goto out1;
	}

	recv_size = recv_queue->recv_size - sizeof(iccom_recv_data);

	if (0 == recv_size) {
		MSG_MED("[ICCOMK]INF|[%s] There are no data.\n", __func__);
		/* receive data size is 0 */
		recv_param->result_code		= cmd_data->msg_header.ret_code;
		recv_param->recv_size		= recv_size;						/* 0 */
		recv_param->recv_data		= NULL;
		ret_code					= recv_queue->eicr_result;			/* SMAP_OK */

		if (ICCOM_MEM_DYNAMIC == cmd_data->mng_info) {					/* receive data use dynamic area */
			kfree(cmd_data);
		}
		goto out1;
	} else {
		MSG_MED("[ICCOMK]INF|[%s] Recieve Data Size=%d.\n", __func__, (unsigned int)recv_size);
		recv_param->result_code		= cmd_data->msg_header.ret_code;
		recv_param->recv_size		= recv_size;
		recv_param->recv_data		= (unsigned char *)cmd_data + sizeof(iccom_recv_data);
		ret_code					= recv_queue->eicr_result;			/* SMAP_OK */
		goto out1;
	}

out1:
	iccom_delete_recv_queue(recv_queue);
#ifdef ICCOM_ENABLE_STANDBYCONTROL
	/* decrement the internal standby control counter after a response to
	   synchronous command excluding EVENT_STATUS_STANDBYCONTROL is returned. */
	iccom_rtctl_after_send_cmd_check_standby(recv_param->function_id);
#endif

out2:
	return ret_code;
}

/******************************************************************************/
/* Function   : iccom_recv_command_async                                      */
/* Description: receive asynchronous command                                  */
/******************************************************************************/
int iccom_recv_command_async(void **handle, iccom_cmd_recv_async_param *recv_param)
{
	int ret_code = SMAP_OK;

	iccom_drv_handle *drv_handle;
	struct completion *async_completion;
	unsigned long *async_recv_status;
	iccom_recv_queue *recv_queue;
	iccom_recv_data *cmd_data;
	unsigned long recv_size = 0;
	int	error;

	if (NULL == recv_param) {
		MSG_ERROR("[ICCOMK]ERR|[%s] Parameter Error! recv_param is NULL.\n", __func__);
		ret_code					= SMAP_PARA_NG;
		goto out2;
	}

	if (NULL == handle) {
		MSG_ERROR("[ICCOMK]ERR|[%s] Parameter Error! handle is NULL.\n", __func__);
		recv_param->result_code		= 0;
		recv_param->function_id		= 0;
		recv_param->recv_size		= recv_size;						/* 0 */
		recv_param->recv_data		= NULL;
		ret_code					= SMAP_PARA_NG;
		goto out2;
	}

	drv_handle = (iccom_drv_handle *)*handle;

	/* set completion information */
	if (NULL == drv_handle) {
		MSG_MED("[ICCOMK]INF|[%s] Handle is NULL.\n", __func__);
		async_completion = &g_iccom_async_completion;
		async_recv_status = &g_iccom_async_recv_status;
	} else {
		async_completion = drv_handle->async_completion;
		async_recv_status = &drv_handle->async_recv_status;
	}

	error = wait_for_completion_interruptible(async_completion);
	MSG_MED("[ICCOMK]INF|[%s] async completion result[%d]\n", __func__, error);

	/* get receive queue */
	ret_code = iccom_get_recv_queue(async_completion, &recv_queue);
	if (SMAP_OK != ret_code) {
		/* queue is not found ---> cancel */
		recv_param->result_code		= 0;
		recv_param->function_id		= 0;
		recv_param->recv_size		= recv_size;						/* 0 */
		recv_param->recv_data		= NULL;

		if (0 == (*async_recv_status & ICCOM_ASYNC_RECV_CANCEL)) {
			MSG_MED("[ICCOMK]INF|[%s] Queue did not exist(Queue NG).\n", __func__);
			ret_code	= SMAP_QUEUE_NG;
		} else {
			MSG_MED("[ICCOMK]INF|[%s] Queue did not exist(Cancel).\n", __func__);
			ret_code	= SMAP_NG;
		}
		goto out2;
	}

	cmd_data = (iccom_recv_data *)recv_queue->recv_data;

	if (SMAP_OK != recv_queue->eicr_result) {
		MSG_ERROR("[ICCOMK]ERR|[%s] Error in the driver(%d).\n", __func__, (unsigned int)recv_queue->eicr_result);
		/* EICR result is NG */
		*handle						= cmd_data->msg_header.handle;
		recv_param->result_code		= SMAP_NG;
		recv_param->function_id		= cmd_data->msg_header.func_id;
		recv_param->recv_size		= recv_size;						/* 0 */
		recv_param->recv_data		= NULL;
		ret_code					= recv_queue->eicr_result;			/* error code */
		goto out1;
	}

	recv_size = recv_queue->recv_size - sizeof(iccom_recv_data);

	if (0 == recv_size) {
		MSG_MED("[ICCOMK]INF|[%s] There are no data.\n", __func__);
		/* receive data size is 0 */
		*handle						= cmd_data->msg_header.handle;
		recv_param->result_code		= cmd_data->msg_header.ret_code;
		recv_param->function_id		= cmd_data->msg_header.func_id;
		recv_param->recv_size		= recv_size;						/* 0 */
		recv_param->recv_data		= NULL;
		ret_code					= recv_queue->eicr_result;			/* SMAP_OK */

		if (ICCOM_MEM_DYNAMIC == cmd_data->mng_info) {					/* receive data use dynamic area */
			kfree(cmd_data);
		}
		goto out1;
	} else {
		MSG_MED("[ICCOMK]INF|[%s] Recieve Data Size=%d.\n", __func__, (unsigned int)recv_size);
		*handle						= cmd_data->msg_header.handle;
		recv_param->result_code		= cmd_data->msg_header.ret_code;
		recv_param->function_id		= cmd_data->msg_header.func_id;
		recv_param->recv_size		= recv_size;
		recv_param->recv_data		= (unsigned char *)cmd_data + sizeof(iccom_recv_data);
		ret_code					= recv_queue->eicr_result;			/* SMAP_OK */

		((iccom_drv_handle *)cmd_data->msg_header.handle)->recv_data = (void *)cmd_data;
		MSG_MED("[ICCOMK]INF|[%s] rcv_buf_addr[0x%08x].\n", __func__, (unsigned int)cmd_data);
		goto out1;
	}

out1:
	iccom_delete_recv_queue(recv_queue);

out2:
	return ret_code;
}

/******************************************************************************/
/* Function   : iccom_recv_complete                                           */
/* Description: release command data                                          */
/******************************************************************************/
int iccom_recv_complete(unsigned char *recv_data)
{
	iccom_recv_data *cmd_data;

	if (NULL != recv_data) {
		MSG_MED("[ICCOMK]INF|[%s] Recieve Data is not NULL.\n", __func__);
		cmd_data = (iccom_recv_data *)(recv_data - sizeof(iccom_recv_data));
		if (ICCOM_MEM_DYNAMIC == cmd_data->mng_info) {	/* receive data use dynamic area */
			MSG_MED("[ICCOMK]INF|[%s] Recieve Data is Dynamic Address.\n", __func__);
			kfree(cmd_data);
		}
	}
	return SMAP_OK;
}

/******************************************************************************/
/* Function   : iccom_recv_cancel                                             */
/* Description: cancel receive command                                        */
/******************************************************************************/
int iccom_recv_cancel(void *handle)
{
	iccom_drv_handle *drv_handle;

	/* release completion for receive */
	if (NULL == handle) {
		MSG_MED("[ICCOMK]INF|[%s] Handle is NULL.\n", __func__);
		g_iccom_async_recv_status |= ICCOM_ASYNC_RECV_CANCEL;
		complete(&g_iccom_async_completion);
	} else {
		drv_handle = (iccom_drv_handle *)handle;
		drv_handle->async_recv_status |= ICCOM_ASYNC_RECV_CANCEL;
		complete(drv_handle->async_completion);
	}

	return SMAP_OK;
}
