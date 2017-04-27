/*
 * camera.c
 *  Camera function file.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
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
#include <linux/module.h>
#include <iccom_drv.h>
#include <iccom_drv_standby.h>
#include <log_kernel.h>
#include <camera.h>
#include "camera_private.h"

void *camera_new(void)
{
	int ret_code;
	struct camera_handle *cam_handle;
	iccom_drv_init_param iccom_init;

	ret_code = 0;
	cam_handle = NULL;
	memset(&iccom_init, 0, sizeof(iccom_init));

	MSG_HIGH("[RTAPIK] IN  |[%s]\n", __func__);

	cam_handle = kmalloc(sizeof(struct camera_handle), GFP_KERNEL);
	if (NULL == cam_handle) {
		MSG_ERROR(
			"[RTAPIK] ERR|[%s][%d] kmalloc() Handle NULL error\n",
			__func__,
			__LINE__);
		return NULL ;
	}

	iccom_init.user_data = cam_handle;
	iccom_init.comp_notice = NULL;
	cam_handle->handle = iccom_drv_init(&iccom_init);
	if (NULL == cam_handle->handle) {
		kfree(cam_handle);
		MSG_ERROR(
			"[RTAPIK] ERR|[%s][%d] iccom_drv_init()\n",
			__func__,
			__LINE__);
		return NULL ;
	}

	MSG_HIGH("[RTAPIK] OUT |[%s]\n", __func__);
	return cam_handle;

}
EXPORT_SYMBOL(camera_new);

int camera_start_notify(struct camera_prm_param *camera_param)
{
	int ret_code;
	iccom_drv_disable_standby_param iccom_disable_standby;
	ret_code = SMAP_LIB_CAMERA_NG;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (NULL == camera_param) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] pwmng_param NULL error\n",
		__func__,
		__LINE__);
		return SMAP_LIB_CAMERA_PARA_NG;
	}

	if (NULL == camera_param->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] handle NULL error\n",
		__func__,
		__LINE__);
		return SMAP_LIB_CAMERA_PARA_NG;
	}

	iccom_disable_standby.handle =
		((struct camera_handle *)(camera_param->handle))->handle;
	ret_code = iccom_drv_disable_standby(&iccom_disable_standby);
	if (SMAP_OK != ret_code) {
		MSG_ERROR(
			"[RTAPIK] ERR|[%d] iccom_drv_disable_standby() "
			"ret = [%d]\n",
			__LINE__,
			ret_code);

		return ret_code;
	}

	MSG_HIGH("[RTAPIK] OUT|[%s]\n", __func__);

	return SMAP_LIB_CAMERA_OK;
}
EXPORT_SYMBOL(camera_start_notify);

int camera_end_notify(struct camera_prm_param *camera_param)
{
	int ret_code;
	iccom_drv_enable_standby_param  iccom_standby_eneble;
	ret_code = SMAP_LIB_CAMERA_NG;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (NULL == camera_param) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] pwmng_param NULL error\n",
		__func__,
		__LINE__);
		return SMAP_LIB_CAMERA_PARA_NG;
	}

	if (NULL == camera_param->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] handle NULL error\n",
		__func__,
		__LINE__);
		return SMAP_LIB_CAMERA_PARA_NG;
	}

	iccom_standby_eneble.handle =
		((struct camera_handle *)
			(camera_param->handle))->handle;
	ret_code = iccom_drv_enable_standby(&iccom_standby_eneble);
	if (SMAP_OK != ret_code) {
		MSG_ERROR(
			"[RTAPIK] ERR|[%s] iccom_drv_enable_standby() "
			"ret = [%d]\n",
			__func__,
			ret_code);
		return ret_code;
	}

	MSG_HIGH("[RTAPIK] OUT|[%s]\n", __func__);

	return SMAP_LIB_CAMERA_OK;
}
EXPORT_SYMBOL(camera_end_notify);

void camera_delete(struct camera_prm_delete *camera_delete)
{
	iccom_drv_cleanup_param iccom_cleanup;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	memset(&iccom_cleanup, 0, sizeof(iccom_drv_cleanup_param));

	if (NULL == camera_delete) {
		MSG_ERROR(
			"[RTAPIK] ERR|[%s][%d] camera_delete NULL error\n",
			__func__,
			__LINE__);
		return;
	}

	if (NULL == camera_delete->handle) {
		MSG_ERROR(
			"[RTAPIK] ERR|[%s][%d] camera_delete->handle "
			"NULL error\n",
			__func__,
			__LINE__);
		return;
	} else {
		iccom_cleanup.handle =
			((struct camera_handle *)
				(camera_delete->handle))->handle;
		iccom_drv_cleanup(&iccom_cleanup);
		kfree(camera_delete->handle);
	}

	MSG_HIGH("[RTAPIK] OUT|[%s]\n", __func__);

	return;
}
EXPORT_SYMBOL(camera_delete);
