/*
 * system_standby.c
 *  RT domain standby function file.
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

#include <linux/types.h>
#include <asm/cacheflush.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <iccom_drv_id.h>
#include <iccom_drv.h>
#include <iccom_drv_standby.h>
#include <system_memory.h>
#include <log_kernel.h>
#include <system_standby.h>
#include "system_standby_private.h"

static void *p_standby_handle;

static void system_sub_rt_standby_delete(system_standby_delete *standby_delete)
{
	iccom_drv_cleanup_param			iccom_cleanup;

	MSG_MED("[RTAPIK] IN |[%s]\n", __func__);

	memset(&iccom_cleanup, 0, sizeof(iccom_cleanup));

	iccom_cleanup.handle = ((standby_handle *)standby_delete->handle)->handle;
	iccom_drv_cleanup(&iccom_cleanup);
	kfree(standby_delete->handle);
	p_standby_handle = NULL;

	MSG_MED("[RTAPIK]OUT |[%s]\n", __func__);

	return;
}


static int system_sub_rt_standby_control(system_standby_control *standby_control)
{
	int	ret_code;
	iccom_drv_send_cmd_param	iccom_send_cmd;

	ret_code = SMAP_LIB_STANDBY_NG;
	memset(&iccom_send_cmd , 0 , sizeof(iccom_send_cmd));

	MSG_MED("[RTAPIK] IN |[%s]\n", __func__);

	/* Standby Transition Process */
	if (RT_STATUS_STANDBY == standby_control->status) {
		iccom_send_cmd.handle      = ((standby_handle *)standby_control->handle)->handle;
		iccom_send_cmd.task_id     = TASK_STATUS_CONTROL;
		iccom_send_cmd.function_id = EVENT_STATUS_STANDBYCONTROL;
		iccom_send_cmd.send_mode   = ICCOM_DRV_SYNC;
		iccom_send_cmd.send_size   = sizeof(unsigned int);
		iccom_send_cmd.send_data   = (unsigned char *) &standby_control->status;
		iccom_send_cmd.recv_size   = 0;
		iccom_send_cmd.recv_data   = 0;

		ret_code = iccom_drv_send_command(&iccom_send_cmd);
		MSG_MED("[RTAPIK]%s iccom_drv_send_command=%d\n", __func__, ret_code);
	} else {	/* Active Transition Process */
		/* This function isn't refer to a argument */
		ret_code = iccom_drv_change_active();
		MSG_MED("[RTAPIK]%s iccom_rtctl_change_rt_state_active=%d\n", __func__, ret_code);
	}

	switch (ret_code) {
	case SMAP_OK:
	case SMAP_ALREADY_STANDBY:
		ret_code = SMAP_LIB_STANDBY_OK;
		break;

	case SMAP_LIB_STCON_INUSE:
		ret_code = SMAP_LIB_STANDBY_INUSE;
		break;

	default:
		ret_code = SMAP_LIB_STANDBY_NG;
		break;
	}

	MSG_MED("[RTAPIK]OUT |[%s]\n", __func__);
	return ret_code;
}

static void *system_sub_rt_standby_new(void)
{
	int						ret_code;
	standby_handle			*standby_handle;
	iccom_drv_init_param	iccom_init;

	ret_code	= 0;
	standby_handle	= NULL;
	memset(&iccom_init, 0, sizeof(iccom_init));

	MSG_MED("[RTAPIK] IN |[%s]\n", __func__);

	standby_handle = kmalloc(sizeof(*standby_handle), GFP_KERNEL);
	if (NULL == standby_handle) {
		MSG_ERROR("[RTAPIK] ERR |[%s] Standby Handle Alloc Fail\n" , __func__);
		return NULL;
	}

	iccom_init.user_data    = standby_handle;
	iccom_init.comp_notice  = NULL;

	standby_handle->handle = iccom_drv_init(&iccom_init);
	if (NULL == standby_handle->handle) {
		kfree(standby_handle);
		MSG_ERROR("[RTAPIK] ERR |[%s] Standby Control iccom_drv_init Error = %d\n" , __func__ , ret_code);
		return NULL;
	}

	MSG_MED("[RTAPIK]OUT |[%s]\n", __func__);
	return  (void *)standby_handle;
}

static int system_rt_standby_standby(void)
{
	int	ret_code;
	bool ret;
	system_standby_control t_standby_cotrol;
	system_standby_delete	t_standby_delete;

	ret_code = SMAP_LIB_STANDBY_NG;
	ret = false;
	memset(&t_standby_cotrol, 0, sizeof(t_standby_cotrol));
	memset(&t_standby_delete, 0, sizeof(t_standby_delete));

	MSG_MED("[RTAPIK] IN |[%s]\n", __func__);

	/* Check Standby Counter */
	ret = iccom_drv_check_standby_enable();
	if (false == ret) {
		MSG_ERROR("[RTAPIK] |[%s] SYS-Domain isn't ready\n" , __func__);
		return SMAP_LIB_STANDBY_NG;
	}

	/* Create the handle for standby control */
	p_standby_handle = system_sub_rt_standby_new();
	if (NULL == p_standby_handle) {
		MSG_ERROR("[RTAPIK] ERR |[%s] handle NULL error\n" , __func__);
		return SMAP_LIB_STANDBY_NG;
	}

	/* Standby Transition Process */
	t_standby_cotrol.status = RT_STATUS_STANDBY;
	t_standby_cotrol.handle = p_standby_handle;

	ret_code = system_sub_rt_standby_control(&t_standby_cotrol);
	if (SMAP_LIB_STANDBY_OK != ret_code) {
		t_standby_delete.handle = p_standby_handle;
		system_sub_rt_standby_delete(&t_standby_delete);
		MSG_ERROR("[RTAPIK] ERR |[%s] Standby Transition Fail\n" , __func__);
		return SMAP_LIB_STANDBY_NG;
	}

	MSG_MED("[RTAPIK]OUT |[%s]\n", __func__);

	return SMAP_LIB_STANDBY_OK;
}

static int system_rt_standby_active(void)
{
	int	ret_code;
	system_standby_control	t_standby_cotrol;
	system_standby_delete	t_standby_delete;

	MSG_MED("[RTAPIK] IN |[%s]\n", __func__);

	ret_code = SMAP_LIB_STANDBY_NG;
	memset(&t_standby_cotrol, 0, sizeof(t_standby_cotrol));
	memset(&t_standby_delete, 0, sizeof(t_standby_delete));

	if (NULL == p_standby_handle) {
		MSG_ERROR("[RTAPIK] ERR |[%s] Standby Handle is NULL\n" , __func__);
		return SMAP_LIB_STANDBY_NG;
	}

	t_standby_cotrol.handle = p_standby_handle;
	t_standby_cotrol.status = RT_STATUS_ACTIVE;

	ret_code = system_sub_rt_standby_control(&t_standby_cotrol);
	if (SMAP_LIB_STANDBY_OK != ret_code) {
		t_standby_delete.handle = p_standby_handle;
		system_sub_rt_standby_delete(&t_standby_delete);
		MSG_ERROR("[RTAPIK] ERR |[%s] Standby Transition Fail\n" , __func__);
		return SMAP_LIB_STANDBY_NG;
	}

	t_standby_delete.handle = p_standby_handle;
	system_sub_rt_standby_delete(&t_standby_delete);

	MSG_MED("[RTAPIK]OUT |[%s]\n", __func__);

	return ret_code;
}

int system_rt_standby(void)
{
	int ret = -1;

	/* Execute rt standby functions. */
	ret = system_rt_standby_standby();

	return ret;

}
EXPORT_SYMBOL(system_rt_standby);

int system_rt_active(void)
{
	int ret = -1;

	/* Execute rt standby functions. */
	ret = system_rt_standby_active();

	return ret;

}
EXPORT_SYMBOL(system_rt_active);
