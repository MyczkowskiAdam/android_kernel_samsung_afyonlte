/*
 * iccom_drv_standby.h
 *   Inter Core Communication Standby API header file.
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

#ifndef __ICCOM_DRV_STANDBY_H__
#define __ICCOM_DRV_STANDBY_H__

typedef struct {
	void				*handle;
} iccom_drv_disable_standby_param;

typedef struct {
	void				*handle;
} iccom_drv_enable_standby_param;

typedef struct {
	void				*handle;
	int					lcd_state;
} iccom_drv_lcd_state_param;


int iccom_drv_disable_standby(iccom_drv_disable_standby_param *iccom_disable_standby);
int iccom_drv_enable_standby(iccom_drv_enable_standby_param *iccom_enable_standby);
int iccom_drv_set_lcd_state(iccom_drv_lcd_state_param *iccom_lcd_state);
int iccom_drv_change_active(void);

bool iccom_drv_check_standby_enable(
	void
);


#endif /* __ICCOM_DRV_STANDBY_H__ */
