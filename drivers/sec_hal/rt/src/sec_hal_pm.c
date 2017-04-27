/*
 * drivers/sec_hal/rt/src/sec_hal_pm.c
 *
 * Copyright (c) 2012-2013, Renesas Mobile Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "sec_hal_rt.h"
#include "sec_hal_rt_cmn.h"
#include "sec_serv_api.h"
#include "sec_msg.h"
#include "sec_dispatch.h"

#include <linux/module.h>
#include <linux/string.h>
#include <linux/uaccess.h>


#define LOCAL_DISP                     raw_pub2sec_dispatcher
#define LOCAL_DEFAULT_DISP_FLAGS       0
#define LOCAL_DEFAULT_DISP_SPARE_PARAM 0
#define LOCAL_WMB()
#define LOCAL_RMB()


static uint16_t msg_data_size[CONFIG_NR_CPUS] ;
static sec_msg_t *in_msg[CONFIG_NR_CPUS] = {NULL};
static sec_msg_t *out_msg[CONFIG_NR_CPUS] = {NULL};
static sec_msg_handle_t in_handle[CONFIG_NR_CPUS];
static sec_msg_handle_t out_handle[CONFIG_NR_CPUS];


/* **************************************************************************
** Function name      : sec_hal_pm_coma_entry_init
** Description        : initialize coma_entry routines static data.
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_pm_coma_entry_init(void)
{
	uint32_t ret = SEC_HAL_RES_OK;
	uint32_t cpu;

	for_each_possible_cpu(cpu) {
		msg_data_size[cpu] = sec_msg_param_size(sizeof(uint32_t))*4;
		in_msg[cpu] = sec_msg_alloc(&in_handle[cpu],
			msg_data_size[cpu],
			SEC_MSG_OBJECT_ID_NONE,
			0,
			SEC_HAL_MSG_BYTE_ORDER);
		msg_data_size[cpu] = sec_msg_param_size(sizeof(sec_serv_status_t))
			+ sec_msg_param_size(sizeof(uint32_t));
		out_msg[cpu] = sec_msg_alloc(&out_handle[cpu],
			msg_data_size[cpu],
			SEC_MSG_OBJECT_ID_NONE,
			0,
			SEC_HAL_MSG_BYTE_ORDER);

		if (NULL == in_msg[cpu] || NULL == out_msg[cpu]){
			ret = SEC_HAL_RES_FAIL;
		}
	}

	return ret;
}


/* **************************************************************************
** Function name      : sec_hal_pm_coma_entry
** Description        : transmit power control event and params to TZ.
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_pm_coma_entry(
	uint32_t mode,
	uint32_t wakeup_address,
	uint32_t pll0,
	uint32_t zclk)
{
	uint32_t sec_hal_status = SEC_HAL_RES_OK;
	uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_status = 0x00;
	uint32_t sec_serv_status = 0x00;
	uint32_t cpu = 0x0;

	cpu = smp_processor_id();
	sec_msg_open(&in_handle[cpu], in_msg[cpu]);
	sec_msg_open(&out_handle[cpu], out_msg[cpu]);

	/* write content to the input msg */
	sec_msg_param_write32(&in_handle[cpu],
		mode,
		SEC_MSG_PARAM_ID_NONE);
	sec_msg_param_write32(&in_handle[cpu],
		wakeup_address,
		SEC_MSG_PARAM_ID_NONE);
	sec_msg_param_write32(
		&in_handle[cpu],
		pll0,
		SEC_MSG_PARAM_ID_NONE);
	sec_msg_param_write32(&in_handle[cpu],
		zclk,
		SEC_MSG_PARAM_ID_NONE);
	LOCAL_WMB();

	/* call dispatcher */
	ssa_disp_status = LOCAL_DISP(SEC_SERV_COMA_ENTRY,
		LOCAL_DEFAULT_DISP_FLAGS,
		LOCAL_DEFAULT_DISP_SPARE_PARAM,
		SEC_HAL_MEM_VIR2PHY_FUNC(out_msg[cpu]),
		SEC_HAL_MEM_VIR2PHY_FUNC(in_msg[cpu]));

	/* interpret the response */
	sec_msg_status = sec_msg_param_read32(&out_handle[cpu],
		&sec_serv_status);
	LOCAL_RMB();
	if (SEC_ROM_RET_OK != ssa_disp_status
		|| SEC_MSG_STATUS_OK != sec_msg_status
		|| SEC_SERV_STATUS_OK != sec_serv_status) {
		printk(KERN_WARNING "[%s] returned a fail!! serv == %d", __func__, sec_serv_status);
		sec_hal_status = SEC_HAL_RES_FAIL;
	}

	return sec_hal_status;
}


/* **************************************************************************
** Function name      : sec_hal_pm_poweroff
** Description        : transmit poweroff event to TZ.
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_pm_poweroff(void)
{
	uint32_t sec_hal_status = SEC_HAL_RES_OK;
	uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_status = 0x00;
	uint32_t sec_serv_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size,
		SEC_MSG_OBJECT_ID_NONE,
		0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size,
		SEC_MSG_OBJECT_ID_NONE,
		0,
		SEC_HAL_MSG_BYTE_ORDER);

	if (NULL == in_msg || NULL == out_msg) {
		sec_hal_status = SEC_HAL_RES_FAIL;
	} else {
		/* write content to the input msg */
		sec_msg_param_write32(
				&in_handle,
				sizeof(uint32_t),
				SEC_MSG_PARAM_ID_NONE);
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_POWER_OFF,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle,
			&sec_serv_status);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_status ||
			SEC_MSG_STATUS_OK != sec_msg_status ||
			SEC_SERV_STATUS_OK != sec_serv_status) {
			sec_hal_status = SEC_HAL_RES_FAIL;
		}
	}

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	return sec_hal_status;
}


/* **************************************************************************
** Function name      : sec_hal_pm_public_cc42_key_init
** Description        : transmit public cc42 key init event to TZ.
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_pm_public_cc42_key_init(void)
{
	uint32_t sec_hal_status = SEC_HAL_RES_OK;
	uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_status = 0x00;
	uint32_t sec_serv_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size,
		SEC_MSG_OBJECT_ID_NONE,
		0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size,
		SEC_MSG_OBJECT_ID_NONE,
		0,
		SEC_HAL_MSG_BYTE_ORDER);

	if (NULL == in_msg || NULL == out_msg) {
		sec_hal_status = SEC_HAL_RES_FAIL;
	} else {
		/* write content to the input msg */
		sec_msg_param_write32(
				&in_handle,
				sizeof(uint32_t),
				SEC_MSG_PARAM_ID_NONE);
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_status = pub2sec_dispatcher(
			SEC_SERV_PUBLIC_CC42_KEY_INIT,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle,
			&sec_serv_status);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_status ||
			SEC_MSG_STATUS_OK != sec_msg_status ||
			SEC_SERV_STATUS_OK != sec_serv_status) {
			sec_hal_status = SEC_HAL_RES_FAIL;
		}
	}

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	return sec_hal_status;
}


/* **************************************************************************
** Function name      : sec_hal_pm_a3sp_state_request
** Description        : transmit&receives public cc42 key init event to TZ.
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_pm_a3sp_state_request(uint32_t state)
{
	uint32_t sec_hal_status = SEC_HAL_RES_OK;
	uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_status = 0x00;
	uint32_t sec_serv_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size,
		SEC_MSG_OBJECT_ID_NONE,
		0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size,
		SEC_MSG_OBJECT_ID_NONE,
		0,
		SEC_HAL_MSG_BYTE_ORDER);

	if (NULL == in_msg || NULL == out_msg) {
		sec_hal_status = SEC_HAL_RES_FAIL;
	} else {
		/* write content to the input msg */
		sec_msg_param_write32(&in_handle,
			state,
			SEC_MSG_PARAM_ID_NONE);
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(
			SEC_SERV_FAST_CALL_A3SP_STATE_REQUEST,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle,
			&sec_serv_status);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_status ||
			SEC_MSG_STATUS_OK != sec_msg_status ||
			SEC_SERV_STATUS_OK != sec_serv_status) {
			sec_hal_status = SEC_HAL_RES_FAIL;
		}
	}

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	return sec_hal_status;
}


/* ****************************************************************************
** Function name      : sec_hal_pm_l2_enable
** Description        : share l2cache control spinlock with secure side.
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** ***************************************************************************/
uint32_t sec_hal_pm_l2_enable(uint32_t spinlock_phys_addr)
{
	uint32_t sec_hal_status = SEC_HAL_RES_OK;
	uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_status = 0x00;
	uint32_t sec_serv_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*3;
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size,
		SEC_MSG_OBJECT_ID_NONE,
		0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size,
		SEC_MSG_OBJECT_ID_NONE,
		0,
		SEC_HAL_MSG_BYTE_ORDER);

	if (NULL == in_msg || NULL == out_msg) {
		sec_hal_status = SEC_HAL_RES_FAIL;
	} else {
		/* write content to the input msg */
		sec_msg_param_write32(&in_handle,
			1, /* nonzero == enable */
			SEC_MSG_PARAM_ID_NONE);
		sec_msg_param_write32(&in_handle,
			L2_SPINLOCK_TYPE_DEFAULT,
			SEC_MSG_PARAM_ID_NONE);
		sec_msg_param_write32(&in_handle,
			spinlock_phys_addr,
			SEC_MSG_PARAM_ID_NONE);
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_L2_CACHE_CONTROL,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle,
			&sec_serv_status);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_status
			|| SEC_MSG_STATUS_OK != sec_msg_status
			|| SEC_SERV_STATUS_OK != sec_serv_status) {
			sec_hal_status = SEC_HAL_RES_FAIL;
		}
	}

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	return sec_hal_status;
}


