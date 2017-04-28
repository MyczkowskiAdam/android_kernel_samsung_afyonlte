/*
 * drivers/sec_hal/exports/sec_hal_pm.h
 *
 * Copyright (c) 2012-2013, Renesas Mobile Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef SEC_HAL_PM_H
#define SEC_HAL_PM_H


/* **************************************************************************
 * return values given by following functions.
 * *************************************************************************/
#define SEC_HAL_PM_RES_OK           0x00000000UL
#define SEC_HAL_PM_RES_FAIL         0x00000010UL
#define SEC_HAL_PM_RES_PARAM_ERROR  0x00000020UL


/* **************************************************************************
 * sec_hal_coma_entry : initializes 'coma' entry's static data.
 *                      must be called before coma_entry calls.
 * *************************************************************************/
uint32_t sec_hal_pm_coma_entry_init(void);

/* **************************************************************************
 * sec_hal_coma_entry : sent indication of 'coma' to secure side.
 * *************************************************************************/
uint32_t sec_hal_pm_coma_entry(
	uint32_t mode,
	uint32_t wakeup_address,
	uint32_t pll0,
	uint32_t zclk);

/* **************************************************************************
 * sec_hal_pm_power_off : sent indication of 'power off' to secure side.
 * *************************************************************************/
uint32_t sec_hal_pm_power_off(void);

/* **************************************************************************
 * sec_hal_pm_public_cc42_key_init : sent indication of 'cc42 key init'
 *                                   to secure side.
 * *************************************************************************/
uint32_t sec_hal_pm_public_cc42_key_init(void);

/* **************************************************************************
 * sec_hal_pm_a3sp_state_request : sent indication of 'a3sp' state
 *                                 to secure side.
 * *************************************************************************/
uint32_t sec_hal_pm_a3sp_state_request(uint32_t state);


#endif /* SEC_HAL_PM_H */

