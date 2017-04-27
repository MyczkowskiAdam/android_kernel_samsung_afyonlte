/*
 * system_pwmng.c
 *  Power Management function file.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

#include <iccom_drv_id.h>
#include <iccom_drv.h>
#include <log_kernel.h>
#include <system_pwmng.h>
#include "system_pwmng_private.h"

void *system_pwmng_new
(
	void
)
{
	int						ret_code;
	system_pwmng_handle		*pwmng_handle;
	iccom_drv_init_param	iccom_init;

	ret_code = 0;
	pwmng_handle = NULL;
	memset(&iccom_init, 0, sizeof(iccom_init));

	MSG_MED("[RTAPIK] IN  |[%s]\n", __func__);

	pwmng_handle = kmalloc(sizeof(system_pwmng_handle), GFP_KERNEL);
	if (NULL == pwmng_handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] kmalloc() Handle NULL error\n",
		__func__,
		__LINE__);
		return NULL;
	}

	iccom_init.user_data	= (void *)pwmng_handle;
	iccom_init.comp_notice	= NULL;
	pwmng_handle->handle = iccom_drv_init(&iccom_init);
	if (NULL == pwmng_handle->handle) {
		kfree(pwmng_handle);
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] iccom_drv_init()\n",
		__func__,
		__LINE__);
		return NULL;
	}

	MSG_MED("[RTAPIK] OUT |[%s]\n", __func__);
	return (void *)pwmng_handle;

}
EXPORT_SYMBOL(system_pwmng_new);

int system_pwmng_powerarea_start_notify(system_pmg_param *pwmng_param)
{
	int ret_code;
	iccom_drv_send_cmd_param iccom_send_cmd;

	ret_code = SMAP_LIB_PWMNG_NG;
	memset(&iccom_send_cmd , 0 , sizeof(iccom_drv_send_cmd_param));

	MSG_MED("[RTAPIK] IN |[%s]\n", __func__);

	if (NULL == pwmng_param) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] pwmng_param NULL error\n",
		__func__,
		__LINE__);
		return SMAP_LIB_PWMNG_PARA_NG;
	}

	if (NULL == pwmng_param->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] handle NULL error\n",
		__func__,
		__LINE__);
		return SMAP_LIB_PWMNG_PARA_NG;
	}

	if ((RT_PWMNG_POWERAREA_MAX_ID < pwmng_param->powerarea_name) ||
		 (RT_PWMNG_POWERAREA_MIN_ID > pwmng_param->powerarea_name)) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] powerarea type = %ld\n",
		__func__,
		__LINE__,
		pwmng_param->powerarea_name);
		return SMAP_LIB_PWMNG_PARA_NG;
	}

	iccom_send_cmd.handle      = ((system_pwmng_handle *)pwmng_param->handle)->handle;
	iccom_send_cmd.task_id     = TASK_STATUS_CONTROL;
	iccom_send_cmd.function_id = EVENT_STATUS_STARTPOWERAREANOTIFY;
	iccom_send_cmd.send_mode   = ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size   = sizeof(unsigned long);
	iccom_send_cmd.send_data   = (unsigned char *)&pwmng_param->powerarea_name;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = 0;

	ret_code = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != ret_code) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] iccom_drv_send_command() ret_code = %d\n",
		__func__,
		__LINE__,
		ret_code);
		return SMAP_LIB_PWMNG_NG;
	}

	MSG_MED("[RTAPIK] OUT|[%s]\n", __func__);

	return SMAP_LIB_PWMNG_OK;
}
EXPORT_SYMBOL(system_pwmng_powerarea_start_notify);

int system_pwmng_powerarea_end_notify(system_pmg_param *pwmng_param)
{
	int ret_code;
	iccom_drv_send_cmd_param iccom_send_cmd;

	ret_code = SMAP_LIB_PWMNG_NG;
	memset(&iccom_send_cmd , 0 , sizeof(iccom_drv_send_cmd_param));

	MSG_MED("[RTAPIK] IN |[%s]\n", __func__);

	if (NULL == pwmng_param) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] pwmng_param NULL error\n",
		__func__,
		__LINE__);
		return SMAP_LIB_PWMNG_PARA_NG;
	}

	if (NULL == pwmng_param->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] handle NULL error\n",
		__func__,
		__LINE__);
		return SMAP_LIB_PWMNG_PARA_NG;
	}

	if ((RT_PWMNG_POWERAREA_MAX_ID < pwmng_param->powerarea_name) ||
		 (RT_PWMNG_POWERAREA_MIN_ID > pwmng_param->powerarea_name)) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] powerarea type = %ld\n",
		__func__,
		__LINE__,
		pwmng_param->powerarea_name);
		return SMAP_LIB_PWMNG_PARA_NG;
	}

	iccom_send_cmd.handle      = ((system_pwmng_handle *)pwmng_param->handle)->handle;
	iccom_send_cmd.task_id     = TASK_STATUS_CONTROL;
	iccom_send_cmd.function_id = EVENT_STATUS_ENDPOWERAREANOTIFY;
	iccom_send_cmd.send_mode   = ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size   = sizeof(unsigned long);
	iccom_send_cmd.send_data   = (unsigned char *)&pwmng_param->powerarea_name;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = 0;

	ret_code = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != ret_code) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] iccom_drv_send_command() ret_code = %d\n",
		__func__,
		__LINE__,
		ret_code);
		return SMAP_LIB_PWMNG_NG;
	}

	MSG_MED("[RTAPIK] OUT|[%s]\n", __func__);

	return SMAP_LIB_PWMNG_OK;
}
EXPORT_SYMBOL(system_pwmng_powerarea_end_notify);

void system_pwmng_delete(system_pmg_delete *pwmng_delete)
{
	iccom_drv_cleanup_param			iccom_cleanup;

	MSG_MED("[RTAPIK] IN |[%s]\n", __func__);

	memset(&iccom_cleanup, 0, sizeof(iccom_drv_cleanup_param));

	if (NULL == pwmng_delete) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] pwmng_delete NULL error\n",
		__func__,
		__LINE__);
		return;
	}

	if (NULL == pwmng_delete->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] pwmng_delete->handle NULL error\n",
		__func__,
		__LINE__);
		return;
	} else {
		iccom_cleanup.handle = ((system_pwmng_handle *)pwmng_delete->handle)->handle;
		iccom_drv_cleanup(&iccom_cleanup);
		kfree(pwmng_delete->handle);
	}

	MSG_MED("[RTAPIK] OUT|[%s]\n", __func__);

	return;
}
EXPORT_SYMBOL(system_pwmng_delete);
