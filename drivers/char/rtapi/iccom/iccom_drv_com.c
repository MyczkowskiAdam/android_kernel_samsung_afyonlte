/*
 * iccom_drv_com.c
 *	 Inter Core Communication driver common function file.
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
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include "log_kernel.h"
#include "iccom_drv.h"
#include "iccom_drv_common.h"
#include "iccom_drv_private.h"
#include "iccom_drv_id.h"


static iccom_fatal_info g_iccom_fatal;			  /* fatal information */
static struct list_head g_iccom_list_recv;		  /* queue header */
static spinlock_t g_iccom_lock_recv_list;		  /* spinlock for receive */
static spinlock_t g_iccom_lock_fatal;			  /* spinlock for fatal */

static void				*iccom_handle;			/* iccomhandle for log */

/******************************************************************************/
/* Function   : iccom_create_handle											  */
/* Description: create a ICCOM handle										  */
/******************************************************************************/
iccom_drv_handle *iccom_create_handle(
	int				 type
) {
	iccom_drv_handle   *handle;
	struct completion  *completion;
	unsigned int		alloc_size;
	iccom_handle_list	*handle_list;
	unsigned long		flag;
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	/* calculate allocation size */
	alloc_size = sizeof(*handle) + sizeof(*completion);
	if (ICCOM_TYPE_USER == type) {
		alloc_size += sizeof(*completion);
	}

	/* allocate memory of ICCOM handle */
	handle = kmalloc(alloc_size, GFP_KERNEL);
	if (NULL != handle) {
		memset(handle, 0, alloc_size);
		completion = (struct completion *)(handle + 1);
		if (ICCOM_TYPE_KERNEL == type) {
			handle->sync_completion = completion;
			handle->async_completion = &g_iccom_async_completion;

			init_completion(handle->sync_completion);
		} else {
			handle->sync_completion = completion;
			handle->async_completion = (completion + 1);

			init_completion(handle->sync_completion);
			init_completion(handle->async_completion);
		}
	} else {
		MSG_ERROR("[ICCOMK]ERR| handle allocate error.\n");
		MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
		return (iccom_drv_handle *)NULL;
	}

	handle_list = kmalloc(sizeof(*handle_list), GFP_KERNEL);
	if (NULL == handle_list) {
		MSG_ERROR("[ICCOMK]ERR| handle list allocate error.\n");
		memset(handle, 0, alloc_size);
		kfree(handle);
		MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
		return (iccom_drv_handle *)NULL;
	} else {
		handle_list->handle = handle;

		spin_lock_irqsave(&g_iccom_lock_handle_list, flag);
		list_add_tail(&handle_list->list, &g_iccom_list_handle);
		spin_unlock_irqrestore(&g_iccom_lock_handle_list, flag);
	}

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return handle;
}

/******************************************************************************/
/* Function   : iccom_destroy_handle										  */
/* Description: destroy a ICCOM handle										  */
/******************************************************************************/
void iccom_destroy_handle(
	iccom_drv_handle   *handle
)
{
	iccom_handle_list	*handle_list;
	unsigned long		flag;
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	spin_lock_irqsave(&g_iccom_lock_handle_list, flag);
	list_for_each_entry(handle_list, &g_iccom_list_handle, list) {
		if (handle_list->handle == handle) {
			MSG_MED("[ICCOMK]INF|list[0x%08x] handle[0x%08x]\n",
			(unsigned int)handle_list->handle, (unsigned int)handle);
			list_del(&handle_list->list);
			kfree(handle_list);
			memset(handle, 0, sizeof(*handle));
			kfree(handle);
			break;
		}
	}
	spin_unlock_irqrestore(&g_iccom_lock_handle_list, flag);

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_init_recv_queue										  */
/* Description: initialize receive queue									  */
/******************************************************************************/
void iccom_init_recv_queue(
	void
)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	memset(&g_iccom_list_recv, 0, sizeof(g_iccom_list_recv));

	INIT_LIST_HEAD(&g_iccom_list_recv);

	spin_lock_init(&g_iccom_lock_recv_list);
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_put_recv_queue										  */
/* Description: put data to receive queue									  */
/******************************************************************************/
int iccom_put_recv_queue(
	struct completion	*completion,
	int					eicr_result,
	void				*recv_data,
	unsigned long		recv_size
)
{
	int				 ret;
	iccom_recv_queue   *p_queue_data;
	unsigned long	   flag;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	ret = SMAP_OK;

	/* allocate memory of receive queue data */
	p_queue_data = kmalloc(sizeof(*p_queue_data), GFP_ATOMIC);
	if (NULL == p_queue_data) {
		ret = SMAP_MEMORY;
	} else {
		/* set receive data information */
		p_queue_data->recv_data	 = recv_data;
		p_queue_data->recv_size	 = recv_size;
		p_queue_data->completion	= completion;
		p_queue_data->eicr_result   = eicr_result;
		spin_lock_irqsave(&g_iccom_lock_recv_list, flag);

		list_add_tail(&p_queue_data->queue_header, &g_iccom_list_recv);

		spin_unlock_irqrestore(&g_iccom_lock_recv_list, flag);
	}

	MSG_MED("[ICCOMK]OUT|[%s] ret = %d\n", __func__, ret);
	return ret;
}

/******************************************************************************/
/* Function   : iccom_get_recv_queue										  */
/* Description: get data from receive queue									  */
/******************************************************************************/
int iccom_get_recv_queue(
	struct completion  *completion,
	iccom_recv_queue  **queue
) {
	int ret;
	iccom_recv_queue   *p_queue_data;
	unsigned long	   flag;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	ret = SMAP_NG;
	if (NULL != queue) {
		*queue = NULL;
		spin_lock_irqsave(&g_iccom_lock_recv_list, flag);
		/* search the list */
		list_for_each_entry(p_queue_data, &g_iccom_list_recv, queue_header) {
			if ((NULL == completion) ||   /* target is not specified */
			   (completion == p_queue_data->completion)) {	/* match */
				*queue = p_queue_data;
				break;
			}
		}
		spin_unlock_irqrestore(&g_iccom_lock_recv_list, flag);
		if (NULL != *queue) {
			ret = SMAP_OK;
		}
	} else {
		MSG_ERROR("[ICCOMK]ERR| parameter error.\n");
	}
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return ret;
}

/******************************************************************************/
/* Function   : iccom_delete_recv_queue										  */
/* Description: delete data from receive queue								  */
/******************************************************************************/
void iccom_delete_recv_queue(
	iccom_recv_queue   *queue
) {
	unsigned long	   flag;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	spin_lock_irqsave(&g_iccom_lock_recv_list, flag);
	list_del(&queue->queue_header);
	spin_unlock_irqrestore(&g_iccom_lock_recv_list, flag);
	kfree(queue);
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_init_fatal											  */
/* Description: initialize fatal information								  */
/******************************************************************************/
void iccom_init_fatal(
	void
)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	memset(&g_iccom_fatal, 0, sizeof(g_iccom_fatal));

	spin_lock_init(&g_iccom_lock_fatal);
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_put_fatal												  */
/* Description: put fatal information										  */
/******************************************************************************/
int iccom_put_fatal(
	iccom_fatal_info   *fatal_info
) {
	int				 ret;
	unsigned long	 flag;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	ret = SMAP_OK;
	spin_lock_irqsave(&g_iccom_lock_fatal, flag);
	/* mismatch fatal information */
	if (fatal_info->handle != g_iccom_fatal.handle) {
		if ((NULL == fatal_info->handle) ||	/* delete fatal information */
			(NULL == g_iccom_fatal.handle)) {  /* fatal information none */
			memcpy(&g_iccom_fatal,
					fatal_info,
					sizeof(g_iccom_fatal));
		} else {
			MSG_ERROR("[ICCOMK]ERR| already entry.\n");
			ret = SMAP_ALREADY_EXIST;
		}
	}
	spin_unlock_irqrestore(&g_iccom_lock_fatal, flag);
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return ret;
}

/******************************************************************************/
/* Function   : iccom_get_fatal												  */
/* Description: get fatal information										  */
/******************************************************************************/
int iccom_get_fatal(
	iccom_fatal_info  **fatal_info
)
{
	int				 ret;
	unsigned long	 flag;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	ret = SMAP_OK;
	if (NULL != fatal_info) {
		spin_lock_irqsave(&g_iccom_lock_fatal, flag);
		if (NULL != g_iccom_fatal.handle) {   /* fatal information is set */
			*fatal_info = &g_iccom_fatal;
			ret = SMAP_OK;
		} else {
			MSG_ERROR("[ICCOMK]ERR| no entry.\n");
			ret = SMAP_NG;
		}
		spin_unlock_irqrestore(&g_iccom_lock_fatal, flag);
	} else {
		MSG_ERROR("[ICCOMK]ERR| parameter error.\n");
		ret = SMAP_NG;
	}
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return ret;
}

/******************************************************************************/
/* Function   : iccom_leak_check												*/
/* Description: check memory leak												*/
/******************************************************************************/
void iccom_leak_check(
	iccom_drv_handle	*handle
)
{
	iccom_recv_queue	*recv_queue;
	iccom_recv_queue	*next_queue;
	unsigned long		flag;

	if (NULL == handle) {
		MSG_MED("[ICCOMK]INF|handle is NULL\n");
		return;
	}

	MSG_MED("[ICCOMK]INF|handle->recv_data[0x%08x]\n", (unsigned int)handle->recv_data);
	if (NULL != handle->recv_data) {
		kfree(handle->recv_data);
		handle->recv_data = NULL;
	}

	MSG_MED("[ICCOMK]INF|handle->async_recv_status[0x%08x]\n", (unsigned int)handle->async_recv_status);
	if (0 == (handle->async_recv_status & ICCOM_ASYNC_RECV_CANCEL))
		handle->async_recv_status |= ICCOM_ASYNC_RECV_CANCEL;

	spin_lock_irqsave(&g_iccom_lock_recv_list, flag);
	/* search the list */
	list_for_each_entry_safe(recv_queue, next_queue, &g_iccom_list_recv, queue_header) {
		MSG_MED("[ICCOMK]INF|queue[0x%08x]queue->comp[0x%08x]comp[0x%08x]\n",
			(unsigned int)recv_queue,
			(unsigned int)recv_queue->completion,
			(unsigned int)handle->async_completion);
		MSG_MED("[ICCOMK]INF|rcv_data[0x%08x]rcv_size[0x%08x]\n",
			(unsigned int)recv_queue->recv_data,
			(unsigned int)recv_queue->recv_size);
		if (handle->async_completion == recv_queue->completion) {
			kfree(recv_queue->recv_data);
			list_del(&recv_queue->queue_header);
			kfree(recv_queue);
		}
	}
	spin_unlock_irqrestore(&g_iccom_lock_recv_list, flag);

	return;
}

/******************************************************************************/
/* Function   : iccom_log_request											  */
/* Description: log output													  */
/******************************************************************************/
static void iccom_log_request(void *user_data, int result, int func_id,
							  unsigned char *addr, int length)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	MSG_LOW("[ICCOMK]    |info   [0x%08X]\n", (unsigned int)user_data);
	MSG_LOW("[ICCOMK]    |result [%d]\n", result);
	MSG_LOW("[ICCOMK]    |func_id[%d]\n", func_id);
	MSG_LOW("[ICCOMK]    |addr   [0x%08X]\n", (unsigned int)addr);
	MSG_LOW("[ICCOMK]    |length [%d]\n", length);

	switch (func_id) {
	case EVENT_DEBUG_STARTOUTPUTLOG:
		if ((0 < length) && (NULL != addr)) {
			printk(KERN_ALERT "[RTDomain]%s", addr);
		}
		break;
	default:
		break;
	}
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return;
}

/******************************************************************************/
/* Function   : iccom_log_start												  */
/* Description: log start													  */
/******************************************************************************/
void iccom_log_start(void)
{
	iccom_drv_init_param		iccom_init;
	iccom_drv_send_cmd_param	iccom_send_cmd;
	int							result;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	iccom_init.user_data	= (void *)NULL;
	iccom_init.comp_notice	= &iccom_log_request;

	iccom_handle = iccom_drv_init(&iccom_init);

	if (NULL == iccom_handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] iccom_log_start() aInfo NULL error\n",
		__func__,
		__LINE__);
		return;
	}

	iccom_send_cmd.handle      = iccom_handle;
	iccom_send_cmd.task_id     = TASK_DEBUG;
	iccom_send_cmd.function_id = EVENT_DEBUG_STARTOUTPUTLOG;
	iccom_send_cmd.send_mode   = ICCOM_DRV_ASYNC;
	iccom_send_cmd.send_size   = 0;
	iccom_send_cmd.send_data   = NULL;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = NULL;

	result = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != result) {
		MSG_ERROR(
		"[ICCOMK] ERR|[%d] iccom_drv_send_command() ret = [%d]\n",
		__LINE__,
		result);
		return;
	}

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return;
}

/******************************************************************************/
/* Function   : iccom_log_stop												  */
/* Description: log stop													  */
/******************************************************************************/
void iccom_log_stop(void)
{
	iccom_drv_cleanup_param  iccom_cleanup;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	iccom_cleanup.handle = iccom_handle;
	iccom_drv_cleanup(&iccom_cleanup);
	iccom_handle = NULL;

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return;
}

