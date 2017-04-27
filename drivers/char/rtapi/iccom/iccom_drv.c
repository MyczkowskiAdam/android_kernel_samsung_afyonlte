/*
 * iccom_drv.c
 *	 Inter Core Communication device driver API function file.
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
#include <linux/types.h>
#include <linux/module.h>
#include <linux/interrupt.h>

#include "log_kernel.h"
#include "iccom_drv.h"
#include "iccom_drv_common.h"
#include "iccom_drv_private.h"
#ifdef ICCOM_ENABLE_STANDBYCONTROL
#include <linux/mfis.h>
#include "iccom_drv_id.h"
#include "screen_id.h"
#include "iccom_drv_standby_private.h"
#endif

/******************************************************************************/
/* Function   : iccom_drv_init                                                */
/* Description: create a ICCOM handle                                         */
/******************************************************************************/
void *iccom_drv_init(iccom_drv_init_param *iccom_init)
{
	iccom_drv_handle *handle;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	/* create a ICCOM handle */
	handle = iccom_create_handle(ICCOM_TYPE_KERNEL);
	if (NULL == handle) {
		MSG_ERROR("[ICCOMK]ERR| iccom_create_handle failed handle[NULL]\n");
		return NULL;
	}

	if (NULL != iccom_init) {
		/* set callback information */
		handle->kernel_cb_info.user_data = iccom_init->user_data;
		handle->kernel_cb_info.module	= iccom_init->comp_notice;
	}

	MSG_MED("[ICCOMK]OUT|[%s] handle[0x%08x]\n", __func__, (unsigned int)handle);
	return (void *)handle;
}
EXPORT_SYMBOL(iccom_drv_init);

/******************************************************************************/
/* Function   : iccom_drv_cleanup                                             */
/* Description: release a ICCOM handle                                        */
/******************************************************************************/
void iccom_drv_cleanup(iccom_drv_cleanup_param *iccom_cleanup)
{
	iccom_drv_handle *handle;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	if (NULL == iccom_cleanup) {
		MSG_ERROR("[ICCOMK]ERR| iccom_cleanup[NULL]\n");
		return;
	}

	if (NULL == iccom_cleanup->handle) {
		MSG_ERROR("[ICCOMK]ERR| iccom_cleanup->handle[NULL]\n");
		return;
	}

	/* release a ICCOM handle */
	handle = (iccom_drv_handle *)iccom_cleanup->handle;
	iccom_destroy_handle(handle);

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return;
}
EXPORT_SYMBOL(iccom_drv_cleanup);

/******************************************************************************/
/* Function   : iccom_drv_send_command                                        */
/* Description: send a command to RT Domain                                   */
/******************************************************************************/
int iccom_drv_send_command(iccom_drv_send_cmd_param	*iccom_send_cmd)
{
	int ret_code = SMAP_OK;
	int ret = SMAP_OK;
	void *handle;
	iccom_drv_cmd_data send_data;
	iccom_cmd_send_param send_param;
	iccom_cmd_recv_param recv_param;
	unsigned int recv_size;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	if (NULL == iccom_send_cmd) {
		ret_code = SMAP_PARA_NG;
		MSG_ERROR("[ICCOMK]ERR| iccom_send_cmd[NULL]\n");
		return ret_code;
	}

	MSG_INFO("[ICCOMK]INF| handle[0x%08X]\n", (unsigned int)iccom_send_cmd->handle);
	MSG_INFO("[ICCOMK]INF| task_id[%d]\n", iccom_send_cmd->task_id);
	MSG_INFO("[ICCOMK]INF| function_id[%d]\n", iccom_send_cmd->function_id);
	MSG_INFO("[ICCOMK]INF| send_mode[%d]\n", iccom_send_cmd->send_mode);
	MSG_INFO("[ICCOMK]INF| send_size[%d]\n", iccom_send_cmd->send_size);
	MSG_INFO("[ICCOMK]INF| send_data[0x%08X]\n", (unsigned int)iccom_send_cmd->send_data);
	MSG_INFO("[ICCOMK]INF| recv_size[%d]\n", iccom_send_cmd->recv_size);
	MSG_INFO("[ICCOMK]INF| recv_data[0x%08X]\n", (unsigned int)iccom_send_cmd->recv_data);

	if (NULL == iccom_send_cmd->handle) {
		ret_code = SMAP_PARA_NG;
		MSG_ERROR("[ICCOMK]ERR| iccom_send_cmd->handle[NULL]\n");
		return ret_code;
	}
	handle = iccom_send_cmd->handle;

#ifdef ICCOM_ENABLE_STANDBYCONTROL
	if ((EVENT_STATUS_STANDBYCONTROL != iccom_send_cmd->function_id) &&
		(EVENT_DISPLAY_SETLCDREFRESH != iccom_send_cmd->function_id)) {
		ret = mfis_drv_resume();
		if (SMAP_OK != ret) {
			MSG_ERROR("[ICCOMK]ERR| mfis_drv_resume failed ret[%d]\n", ret);
			return ret + ICCOM_DRV_SYSTEM_ERR;
		}
	}
#endif

	/* set parameters of iccom_send_command() */
	memset(&send_param, 0, sizeof(iccom_cmd_send_param));
	send_param.task_id     = iccom_send_cmd->task_id;
	send_param.function_id = iccom_send_cmd->function_id;
	send_param.send_mode   = iccom_send_cmd->send_mode;
	if (0 < iccom_send_cmd->send_size) {
		memset(&send_data, 0, sizeof(iccom_drv_cmd_data));
		send_data.size		 = iccom_send_cmd->send_size;
		send_data.data		 = (unsigned int *)iccom_send_cmd->send_data;
		send_param.send_num  = 1;
		send_param.send_data = &send_data;
	} else {
		send_param.send_num  = 0;
		send_param.send_data = NULL;
	}

	/* send a command */
	ret = iccom_send_command(handle, ICCOM_TYPE_KERNEL, &send_param);
	if (SMAP_OK != ret) {
		MSG_ERROR("[ICCOMK]ERR| iccom_send_command failed ret[%d]\n", ret);

#ifdef ICCOM_ENABLE_STANDBYCONTROL
		if (SMAP_ALREADY_STANDBY != ret) {
			ret_code = (ret + ICCOM_DRV_SYSTEM_ERR);
		} else {
			ret_code = ret;
		}
#else
		ret_code = (ret + ICCOM_DRV_SYSTEM_ERR);
#endif

	} else {
		/* synchronous communication */
		if (ICCOM_DRV_SYNC == iccom_send_cmd->send_mode) {

#ifdef ICCOM_ENABLE_STANDBYCONTROL
			/* Response from RT to EVENT_STATUS_STANDBYCONTROL command is not to be returned */
			if (EVENT_STATUS_STANDBYCONTROL != iccom_send_cmd->function_id) {
#endif
				/* set parameters of iccom_recv_command_sync() */
				memset(&recv_param, 0, sizeof(iccom_cmd_recv_param));
				recv_param.function_id = iccom_send_cmd->function_id;

				/* receive a synchronous command */
				ret = iccom_recv_command_sync(handle, &recv_param);
				if (SMAP_OK == ret) {
					/* check receive data size */
					recv_size = iccom_send_cmd->recv_size;
					if (recv_size > recv_param.recv_size) {
						recv_size = recv_param.recv_size;
					}

					/* check receive data */
					if ((0 != recv_size) &&
						(NULL != iccom_send_cmd->recv_data) && (NULL != recv_param.recv_data)) {
						memcpy(iccom_send_cmd->recv_data, recv_param.recv_data, recv_size);
					}

					ret_code = recv_param.result_code;

					/* release command data */
					ret = iccom_recv_complete(recv_param.recv_data);
					if (SMAP_OK != ret) {
						MSG_ERROR("[ICCOMK]ERR| iccom_recv_complete failed ret[%d]\n", ret);
					}
				} else {
					MSG_ERROR("[ICCOMK]ERR| iccom_recv_command_sync failed ret[%d]\n", ret);
					ret_code = (ret + ICCOM_DRV_SYSTEM_ERR);
				}
#ifdef ICCOM_ENABLE_STANDBYCONTROL
			}
#endif
		}
	}

	MSG_MED("[ICCOMK]OUT|[%s] ret_code[%d]\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(iccom_drv_send_command);

/******************************************************************************/
/* Function   : iccom_drv_send_command_array                                  */
/* Description: send a command to RT Domain (multiple data)                   */
/******************************************************************************/
int iccom_drv_send_command_array(iccom_drv_send_cmd_array_param *iccom_send_cmd_array)
{
	int ret_code = SMAP_OK;
	int ret = SMAP_OK;
	void *handle;
	iccom_cmd_send_param send_param;
	iccom_cmd_recv_param recv_param;
	unsigned int recv_size;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	if (NULL == iccom_send_cmd_array) {
		ret_code = SMAP_PARA_NG;
		MSG_ERROR("[ICCOMK]ERR| iccom_send_cmd_array[NULL]\n");
		return ret_code;
	}

	MSG_INFO("[ICCOMK]INF| handle[0x%08X]\n", (unsigned int)iccom_send_cmd_array->handle);
	MSG_INFO("[ICCOMK]INF| task_id[%d]\n", iccom_send_cmd_array->task_id);
	MSG_INFO("[ICCOMK]INF| function_id[%d]\n", iccom_send_cmd_array->function_id);
	MSG_INFO("[ICCOMK]INF| send_mode[%d]\n", (unsigned int)iccom_send_cmd_array->send_mode);
	MSG_INFO("[ICCOMK]INF| send_num[%d]\n", iccom_send_cmd_array->send_num);
	MSG_INFO("[ICCOMK]INF| send_data[%d]\n", (unsigned int)iccom_send_cmd_array->send_data);
	MSG_INFO("[ICCOMK]INF| recv_size[%d]\n", iccom_send_cmd_array->recv_size);
	MSG_INFO("[ICCOMK]INF| recv_data[0x%08X]\n", (unsigned int)iccom_send_cmd_array->recv_data);

	if (NULL == iccom_send_cmd_array->handle) {
		ret_code = SMAP_PARA_NG;
		MSG_ERROR("[ICCOMK]ERR| iccom_send_cmd_array->handle[NULL]\n");
		return ret_code;
	}
	handle = iccom_send_cmd_array->handle;

#ifdef ICCOM_ENABLE_STANDBYCONTROL
	ret = mfis_drv_resume();
	if (SMAP_OK != ret) {
		MSG_ERROR("[ICCOMK]ERR| mfis_drv_resume failed ret[%d]\n", ret);
		return ret + ICCOM_DRV_SYSTEM_ERR;
	}
#endif

	/* set parameters of iccom_send_command() */
	memset(&send_param, 0, sizeof(iccom_cmd_send_param));
	send_param.task_id		= iccom_send_cmd_array->task_id;
	send_param.function_id	= iccom_send_cmd_array->function_id;
	send_param.send_mode	= iccom_send_cmd_array->send_mode;
	send_param.send_num		= iccom_send_cmd_array->send_num;
	send_param.send_data	= iccom_send_cmd_array->send_data;

	/* send a command */
	ret = iccom_send_command(handle, ICCOM_TYPE_KERNEL, &send_param);
	if (SMAP_OK != ret) {
		MSG_ERROR("[ICCOMK]ERR| iccom_send_command failed ret[%d]\n", ret);
		ret_code = (ret + ICCOM_DRV_SYSTEM_ERR);
	} else {
		/* synchronous communication */
		if (ICCOM_DRV_SYNC == iccom_send_cmd_array->send_mode) {
			/* set parameters of iccom_recv_command_sync() */
			memset(&recv_param, 0, sizeof(iccom_cmd_recv_param));
			recv_param.function_id = iccom_send_cmd_array->function_id;

			/* receive a synchronous command */
			ret = iccom_recv_command_sync(handle, &recv_param);
			if (SMAP_OK == ret) {
				/* check receive data size */
				recv_size = iccom_send_cmd_array->recv_size;
				if (recv_size > recv_param.recv_size) {
					recv_size = recv_param.recv_size;
				}

				/* check receive data */
				if ((0 != recv_size) &&
					(NULL != iccom_send_cmd_array->recv_data) && (NULL != recv_param.recv_data)) {
					memcpy(iccom_send_cmd_array->recv_data, recv_param.recv_data, recv_size);
				}

				ret_code = recv_param.result_code;

				/* release command data */
				ret = iccom_recv_complete(recv_param.recv_data);
				if (SMAP_OK != ret) {
					MSG_ERROR("[ICCOMK]ERR| iccom_recv_complete failed ret[%d]\n", ret);
				}
			} else {
				MSG_ERROR("[ICCOMK]ERR| iccom_recv_command_sync failed ret[%d]\n", ret);
				ret_code = (ret + ICCOM_DRV_SYSTEM_ERR);
			}
		}
	}

	MSG_MED("[ICCOMK]OUT|[%s] ret_code[%d]\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(iccom_drv_send_command_array);
