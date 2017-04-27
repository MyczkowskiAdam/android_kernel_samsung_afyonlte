/*
 * iccom_drv_standby.c
 *	 Inter Core Communication Standby API function file.
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
#include <linux/module.h>
#include <linux/interrupt.h>

#include "log_kernel.h"
#include "iccom_drv.h"
#include "iccom_drv_standby.h"
#include "iccom_drv_standby_private.h"

/******************************************************************************/
/* Function   : iccom_drv_disable_standby                                     */
/* Description:                                                               */
/******************************************************************************/
int iccom_drv_disable_standby(iccom_drv_disable_standby_param *iccom_disable_standby)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	if (NULL == iccom_disable_standby) {
		MSG_HIGH("[ICCOMK]OUT|[%s] ret_code = SMAP_PARA_NG\n", __func__);
		return SMAP_PARA_NG;
	}
	if (NULL == iccom_disable_standby->handle) {
		MSG_HIGH("[ICCOMK]OUT|[%s] ret_code = SMAP_PARA_NG\n", __func__);
		return SMAP_PARA_NG;
	}
	iccom_rtctl_standby_ng();
	MSG_MED("[ICCOMK]OUT|[%s] ret_code = SMAP_OK\n", __func__);
	return SMAP_OK;
}
EXPORT_SYMBOL(iccom_drv_disable_standby);

/******************************************************************************/
/* Function   : iccom_drv_enable_standby                                      */
/* Description:                                                               */
/******************************************************************************/
int iccom_drv_enable_standby(iccom_drv_enable_standby_param *iccom_standby_eneble)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	if (NULL == iccom_standby_eneble) {
		MSG_HIGH("[ICCOMK]OUT|[%s] ret_code = NULL(SMAP_PARA_NG)\n", __func__);
		return SMAP_PARA_NG;
	}
	if (NULL == iccom_standby_eneble->handle) {
		MSG_HIGH("[ICCOMK]OUT|[%s] ret_code = NULL(SMAP_PARA_NG)\n", __func__);
		return SMAP_PARA_NG;
	}
	iccom_rtctl_standby_ng_cancel();
	MSG_MED("[ICCOMK]OUT|[%s] ret_code = SMAP_OK\n", __func__);
	return SMAP_OK;
}
EXPORT_SYMBOL(iccom_drv_enable_standby);

/******************************************************************************/
/* Function   : iccom_drv_set_lcd_state                                       */
/* Description:                                                               */
/******************************************************************************/
int iccom_drv_set_lcd_state(iccom_drv_lcd_state_param *iccom_lcd_state)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	if (NULL == iccom_lcd_state) {
		MSG_HIGH("[ICCOMK]OUT|[%s] ret_code = NULL (SMAP_PARA_NG)\n", __func__);
		return SMAP_PARA_NG;
	}

	if ((NULL == iccom_lcd_state->handle) ||
		((ICCOM_DRV_STATE_LCD_ON != iccom_lcd_state->lcd_state) &&
		 (ICCOM_DRV_STATE_LCD_OFF != iccom_lcd_state->lcd_state) &&
		 (ICCOM_DRV_STATE_LCD_REFRESH != iccom_lcd_state->lcd_state))) {
		MSG_HIGH("[ICCOMK]OUT|[%s] lcd_state = %d(SMAP_PARA_NG)\n",
					__func__, iccom_lcd_state->lcd_state);
		return SMAP_PARA_NG;
	}
	iccom_rtctl_set_lcd_status(iccom_lcd_state->handle, iccom_lcd_state->lcd_state);
	MSG_MED("[ICCOMK]OUT|[%s] ret_code = SMAP_OK\n", __func__);
	return SMAP_OK;
}
EXPORT_SYMBOL(iccom_drv_set_lcd_state);

/******************************************************************************/
/* Function   : iccom_drv_change_active                                       */
/* Description:                                                               */
/******************************************************************************/
int iccom_drv_change_active(
	void
)
{
	int ret_code;

	ret_code = SMAP_NG;

	ret_code = iccom_rtctl_change_rt_state_active();

	return ret_code;
}
EXPORT_SYMBOL(iccom_drv_change_active);

/******************************************************************************/
/* Function   : iccom_drv_check_standby_enable                                */
/* Description:                                                               */
/******************************************************************************/
bool iccom_drv_check_standby_enable(
	void
)
{
	bool ret;

	ret = false;

	ret = rtctl_check_standby_enable();

	return ret;
}
EXPORT_SYMBOL(iccom_drv_check_standby_enable);

