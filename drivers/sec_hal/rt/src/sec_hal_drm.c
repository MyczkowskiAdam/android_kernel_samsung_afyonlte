/*
 * drivers/sec_hal/rt/src/sec_hal_drm.c
 *
 * Copyright (c) 2012, Renesas Mobile Corporation.
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

#include "sec_hal_res.h"
#include "sec_hal_rt_cmn.h"
#include "sec_hal_dev_ioctl.h"
#define SEC_HAL_TRACE_LOCAL_DISABLE
#include "sec_hal_rt_trace.h"
#include "sec_serv_api.h"
#include "sec_msg.h"
#include "sec_dispatch.h"

#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>


#define LOCAL_DISP                     pub2sec_dispatcher
#define LOCAL_DEFAULT_DISP_FLAGS       0
#define LOCAL_DEFAULT_DISP_SPARE_PARAM 0
#define LOCAL_WMB()                    wmb()
#define LOCAL_RMB()                    rmb()
#define LOCAL_IV_SIZE                  16
#define LOCAL_NOT_DEFINED_ID           0xFFFFFFFF


struct _drm_data {
	uint32_t session_id;
};


/* **************************************************************************
 * Function name      : sec_hal_drm_enter
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_drm_enter(uint32_t thr_id, uint32_t *session_id)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				thr_id,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("thr_id write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_SESSION_INIT,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st = sec_msg_param_read32(&out_handle, &sec_serv_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE("failed! disp==%d, msg==%d, serv==%d",
				ssa_disp_st, sec_msg_st, sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		sec_msg_st = sec_msg_param_read32(&out_handle, session_id);
		if (SEC_MSG_STATUS_OK != sec_msg_st) {
			SEC_HAL_TRACE("failed to read session_id! msg==%d",
				sec_msg_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB(); /* ensure that read ops completed */

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_st;
}


/* **************************************************************************
 * Function name      : sec_hal_drm_exit
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
uint32_t sec_hal_drm_exit(uint32_t id_cnt, uint32_t session_ids[])
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*(1+id_cnt);
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(
				&in_handle,
				id_cnt,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("id_cnt write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		while (SEC_MSG_STATUS_OK == sec_msg_st && id_cnt) {
			sec_msg_st = sec_msg_param_write32(
				&in_handle,
				session_ids[--id_cnt],
				SEC_MSG_PARAM_ID_NONE);
		}
		if (SEC_MSG_STATUS_OK != sec_msg_st) {
			SEC_HAL_TRACE("id write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_SESSION_TERMINATE,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st = sec_msg_param_read32(&out_handle, &sec_serv_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE("failed! disp==%d, msg==%d, serv==%d",
				ssa_disp_st, sec_msg_st, sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_st;
}


/* **************************************************************************
 * Function name      : sec_hal_drm_set_entit_key
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_drm_set_entit_key(
	uint32_t session_id,
	uint32_t key_len,
	uint8_t *key)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st[2] = {SEC_MSG_STATUS_OK};
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint32_t sec_drm_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*2
		+ sec_msg_param_size(key_len);
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				session_id,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("session_id write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				key_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("key_len write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(&in_handle,
				key, key_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("key write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_SET_ENTITLEMENT_KEY,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, &sec_serv_st);
		sec_msg_st[1] = sec_msg_param_read32(&out_handle, &sec_drm_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE(
				"failed! disp==%d, msg0==%d, msg1==%d, serv==%d",
				ssa_disp_st, sec_msg_st[0],
				sec_msg_st[1], sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return (SEC_HAL_RES_OK == sec_hal_st ? sec_drm_st : sec_hal_st);
}


/* **************************************************************************
 * Function name      : sec_hal_drm_derive_cw
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_drm_derive_cw(
	uint32_t session_id,
	uint32_t ecm_len,
	uint8_t *ecm,
	uint32_t *flags)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st[2] = {SEC_MSG_STATUS_OK};
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint32_t sec_drm_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*2
		+ sec_msg_param_size(ecm_len);
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t))*2;
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				session_id,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("session_id write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				ecm_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("ecm_len write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(&in_handle,
				ecm, ecm_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("ecm write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_DERIVE_CONTROL_WORD,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, &sec_serv_st);
		sec_msg_st[1] = sec_msg_param_read32(&out_handle, &sec_drm_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE(
				"failed! disp==%d, msg0==%d, msg1==%d, serv==%d",
				ssa_disp_st, sec_msg_st[0],
				sec_msg_st[1], sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
		}
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, flags);
		if (SEC_MSG_STATUS_OK != sec_msg_st[0]) {
			SEC_HAL_TRACE("failed to read flags! msg==%d",
				sec_msg_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB(); /* ensure that read ops completed */

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return (SEC_HAL_RES_OK == sec_hal_st ? sec_drm_st : sec_hal_st);
}


/* **************************************************************************
 * Function name      : sec_hal_drm_decrypt_video
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_drm_decrypt_video(
	uint32_t session_id,
	uint8_t *iv_in,
	uint32_t iv_len,
	uint32_t input_len,
	uint32_t input_phys_addr,
	uint32_t output_phys_addr,
	uint32_t output_offset,
	uint32_t *output_len,
	uint8_t *iv_out)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st[2] = {SEC_MSG_STATUS_OK};
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint32_t sec_drm_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*6
		+ sec_msg_param_size(LOCAL_IV_SIZE);
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t))*2
		+ sec_msg_param_size(LOCAL_IV_SIZE);
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				session_id,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("session_id write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				iv_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("iv_len write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(&in_handle,
				iv_in, LOCAL_IV_SIZE,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("iv_in write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				input_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("input_len write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				input_phys_addr,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("in phys_addr write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				output_phys_addr,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("out phys_addr write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				output_offset,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK){
			SEC_HAL_TRACE("out offset write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_DECRYPT_VIDEO,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, &sec_serv_st);
		sec_msg_st[1] = sec_msg_param_read32(&out_handle, &sec_drm_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE(
				"failed! disp==%d, msg0==%d, msg1==%d, serv==%d",
				ssa_disp_st, sec_msg_st[0],
				sec_msg_st[1], sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
		}
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, output_len);
		sec_msg_st[1] = sec_msg_param_read(&out_handle,
			iv_out, LOCAL_IV_SIZE);
		if (SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]) {
			SEC_HAL_TRACE("failed to read! msg0==%d, msg1==%d",
				sec_msg_st[0], sec_msg_st[1]);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB(); /* ensure that read ops completed */

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return (sec_hal_st == SEC_HAL_RES_OK ? sec_drm_st : sec_hal_st);
}


/* **************************************************************************
 * Function name      : sec_hal_drm_decrypt_audio
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_drm_decrypt_audio(
	uint32_t session_id,
	uint8_t *iv_in,
	uint32_t iv_len,
	uint32_t input_len,
	uint32_t input_phys_addr,
	uint32_t output_phys_addr,
	uint32_t *output_len,
	uint8_t *iv_out)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st[2] = {SEC_MSG_STATUS_OK};
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint32_t sec_drm_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*5
		+ sec_msg_param_size(LOCAL_IV_SIZE);
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t))*2
		+ sec_msg_param_size(LOCAL_IV_SIZE);
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (!in_msg || !out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				session_id,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("session_id write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				iv_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("iv_len write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(&in_handle,
				iv_in, LOCAL_IV_SIZE,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("iv write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				input_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("input_len error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				input_phys_addr,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("input_phys_addr error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				output_phys_addr,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("output_phys_addr error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_DECRYPT_AUDIO,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, &sec_serv_st);
		sec_msg_st[1] = sec_msg_param_read32(&out_handle, &sec_drm_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE(
				"failed! disp==%d, msg0==%d, msg1==%d, serv==%d",
				ssa_disp_st, sec_msg_st[0],
				sec_msg_st[1], sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, output_len);
		sec_msg_st[1] = sec_msg_param_read(&out_handle,
			iv_out, LOCAL_IV_SIZE);
		if (SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]) {
			SEC_HAL_TRACE("failed to read! msg0==%d, msg1==%d",
				sec_msg_st[0], sec_msg_st[1]);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB(); /* ensure that read ops completed */

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return (SEC_HAL_RES_OK == sec_hal_st ? sec_drm_st : sec_hal_st);
}


/* **************************************************************************
 * Function name      : sec_hal_drm_wrap_keybox
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_drm_wrap_keybox(
	uint8_t *keybox,
	uint32_t keybox_size,
	uint8_t *wrapped_keybox,
	uint32_t *wrapped_keybox_size,
	uint8_t *transport_key,
	uint32_t transport_key_size)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st[2] = {SEC_MSG_STATUS_OK};
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint32_t sec_drm_st = 0x00;
	uint16_t msg_data_size, buffer_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	buffer_size = wrapped_keybox ? sec_msg_param_size(keybox_size*2) : 0;
	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*3
		+ sec_msg_param_size(keybox_size)
		+ sec_msg_param_size(transport_key_size);
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t))*2
		+ buffer_size;
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				keybox_size,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("keybox_size write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(&in_handle,
				keybox, keybox_size,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("keybox write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				buffer_size,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("buffer_size write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		/* optional transport key */
		if (transport_key_size
			&& (sec_msg_param_write32(&in_handle,
					transport_key_size,
					SEC_MSG_PARAM_ID_NONE)
				!= SEC_MSG_STATUS_OK)) {
			SEC_HAL_TRACE("trs_key_size write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (transport_key && transport_key_size
			&& (sec_msg_param_write(&in_handle,
					transport_key,
					transport_key_size,
					SEC_MSG_PARAM_ID_NONE)
				!= SEC_MSG_STATUS_OK)) {
			SEC_HAL_TRACE("trs_key write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_WRAP_KEYBOX,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, &sec_serv_st);
		sec_msg_st[1] = sec_msg_param_read32(&out_handle, &sec_drm_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE(
				"failed! disp==%d, msg0==%d, msg1==%d, serv==%d",
				ssa_disp_st, sec_msg_st[0],
				sec_msg_st[1], sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		sec_msg_st[0] = sec_msg_param_read32(&out_handle,
			wrapped_keybox_size);
		if (wrapped_keybox && *wrapped_keybox_size) {
			sec_msg_st[1] = sec_msg_param_read(&out_handle,
				wrapped_keybox,
				(uint16_t)*wrapped_keybox_size);
		}
		if (SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]) {
			SEC_HAL_TRACE("wrap_keybox read failed!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB(); /* ensure that read ops completed */

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return (sec_hal_st == SEC_HAL_RES_OK ? sec_drm_st : sec_hal_st);
}


/* **************************************************************************
 * Function name      : sec_hal_drm_install_keybox
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_drm_install_keybox(
	uint8_t *keybox,
	uint32_t keybox_size)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st[2] = {SEC_MSG_STATUS_OK};
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint32_t sec_drm_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))
		+ sec_msg_param_size(keybox_size);
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				keybox_size,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("keybox_size write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(&in_handle,
				keybox, keybox_size,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("keybox write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_INSTALL_KEYBOX,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, &sec_serv_st);
		sec_msg_st[1] = sec_msg_param_read32(&out_handle, &sec_drm_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE(
				"failed! disp==%d, msg0==%d, msg1==%d, serv==%d",
				ssa_disp_st, sec_msg_st[0],
				sec_msg_st[1], sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return (sec_hal_st == SEC_HAL_RES_OK ? sec_drm_st : sec_hal_st);
}


/* **************************************************************************
 * Function name      : sec_hal_drm_is_keybox_valid
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
uint32_t sec_hal_drm_is_keybox_valid(void)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st[2] = {SEC_MSG_STATUS_OK};
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint32_t sec_drm_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				sizeof(uint32_t),
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("keybox_size write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_IS_KEYBOX_VALID,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, &sec_serv_st);
		sec_msg_st[1] = sec_msg_param_read32(&out_handle, &sec_drm_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE(
				"failed! disp==%d, msg0==%d, msg1==%d, serv==%d",
				ssa_disp_st, sec_msg_st[0],
				sec_msg_st[1], sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_INT("valid", !sec_drm_st);
	SEC_HAL_TRACE_EXIT();
	return (sec_hal_st == SEC_HAL_RES_OK ? sec_drm_st : sec_hal_st);
}


/* **************************************************************************
 * Function name      : sec_hal_drm_get_random
 * Description        : returns requested number of random number from TZ.
 * Parameters         : IN/--- uint32_t size
 *                      OUT/ --- uint8_t *random, ptr to array for random.
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_drm_get_random(uint32_t size, uint8_t *random)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st[2] = {SEC_MSG_STATUS_OK};
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint32_t sec_drm_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from RAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t))
		+ sec_msg_param_size(size);
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msg not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				size,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("size write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_GET_RANDOM,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, &sec_serv_st);
		sec_msg_st[1] = sec_msg_param_read32(&out_handle, &sec_drm_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE(
				"failed! disp==%d, msg0==%d, msg1==%d, serv==%d",
				ssa_disp_st, sec_msg_st[0],
				sec_msg_st[1], sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		sec_msg_st[0] = sec_msg_param_read(&out_handle, random, size);
		if (SEC_MSG_STATUS_OK != sec_msg_st[0]) {
			SEC_HAL_TRACE("read failed!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB(); /* ensure that read ops completed */

	/* de-allocate msg */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return (SEC_HAL_RES_OK == sec_hal_st ? sec_drm_st : sec_hal_st);
}


/* **************************************************************************
 * Function name      : sec_hal_drm_get_device_id
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_drm_get_device_id(
	uint32_t id_len,
	uint8_t *id,
	uint32_t *out_size)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st[2] = {SEC_MSG_STATUS_OK};
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint32_t sec_drm_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t))*2
		+ sec_msg_param_size(id_len);
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				id_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("id_len write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_GET_DEVICE_ID,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, &sec_serv_st);
		sec_msg_st[1] = sec_msg_param_read32(&out_handle, &sec_drm_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE(
				"failed! disp==%d, msg0==%d, msg1==%d, serv==%d",
				ssa_disp_st, sec_msg_st[0],
				sec_msg_st[1], sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, out_size);
		sec_msg_st[1] = sec_msg_param_read(&out_handle, id, *out_size);
		if (SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]) {
			SEC_HAL_TRACE("read failed!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB(); /* ensure that read ops completed */

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return (sec_hal_st == SEC_HAL_RES_OK ? sec_drm_st : sec_hal_st);
}


/* **************************************************************************
 * Function name      : sec_hal_drm_get_key_data
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_drm_get_key_data(
	uint32_t key_data_len,
	uint8_t *key_data,
	uint32_t *out_size)
{
	uint32_t sec_hal_st = SEC_HAL_RES_OK;
	uint32_t sec_msg_st[2] = {SEC_MSG_STATUS_OK};
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint32_t sec_drm_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t))*2
		+ sec_msg_param_size(key_data_len);
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (!in_msg || !out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				key_data_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("key_data_len write err, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(
			SEC_SERV_DRM_OEMCRYPTO_GET_KEYDATA,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, &sec_serv_st);
		sec_msg_st[1] = sec_msg_param_read32(&out_handle, &sec_drm_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_st
			|| SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]
			|| SEC_SERV_STATUS_OK != sec_serv_st) {
			SEC_HAL_TRACE(
				"failed! disp==%d, msg0==%d, msg1==%d, serv==%d",
				ssa_disp_st, sec_msg_st[0],
				sec_msg_st[1], sec_serv_st);
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		sec_msg_st[0] = sec_msg_param_read32(&out_handle, out_size);
		sec_msg_st[1] = sec_msg_param_read(&out_handle,
			key_data, *out_size);
		if (SEC_MSG_STATUS_OK != sec_msg_st[0]
			|| SEC_MSG_STATUS_OK != sec_msg_st[1]) {
			SEC_HAL_TRACE("read failed!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB(); /* ensure that read ops completed */

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return (sec_hal_st == SEC_HAL_RES_OK ? sec_drm_st : sec_hal_st);
}


static inline int has_drm_session(struct _drm_data *data, uint32_t id)
{
	return (data->session_id == id);
}


static uint32_t _drm_enter(
	struct _drm_data *data,
	sd_ioctl_params_t *params)
{
	uint32_t id, ret;

	if (!has_drm_session(data, id)) {
		ret = sec_hal_drm_enter(current->pid, &id);
		if (SEC_HAL_RES_OK == ret) {
			data->session_id = id;
			if (copy_to_user((void __user *)params->param0,
					&id, sizeof(uint32_t)))
				ret = SEC_HAL_RES_FAIL;
		} else
			ret = SEC_HAL_RES_FAIL;
	} else
		ret = SEC_HAL_RES_FAIL;

	return ret;
}


static uint32_t _drm_exit(
	struct _drm_data *data,
	sd_ioctl_params_t *p)
{
	uint32_t id, ret;

	id = p->param0;
	if (has_drm_session(data, id)) {
		ret = sec_hal_drm_exit(1, &id);
		data->session_id = 0;
	} else
		ret = SEC_HAL_RES_FAIL;

	return ret;
}


#define ENTIT_KEY_MAX_LEN 16
static uint32_t _drm_set_entit_key(
	struct _drm_data *data,
	sd_ioctl_params_t *p)
{
	uint32_t id, ret, keyl;
	uint8_t key[ENTIT_KEY_MAX_LEN] = {0};

	id = p->param0;
	keyl = p->param2;
	if (has_drm_session(data, id)
		&& ENTIT_KEY_MAX_LEN >= keyl) {
		if (copy_from_user(key, (const void *)p->param1, keyl))
			ret = SEC_HAL_RES_FAIL;
		else
			ret = sec_hal_drm_set_entit_key(id, keyl, key);
	} else
		ret = SEC_HAL_RES_FAIL;

	return ret;
}


#define ECM_KEY_MAX_LEN 32
static uint32_t _drm_derive_cw(
	struct _drm_data *data,
	sd_ioctl_params_t *p)
{
	uint32_t id, ret, ecm_len, flags = 0;
	uint8_t ecm[ECM_KEY_MAX_LEN] = {0};

	id = p->param0;
	ecm_len = p->param2;
	if (has_drm_session(data, id)
		&& ECM_KEY_MAX_LEN >= ecm_len) {
		if (copy_from_user(ecm, (const void *)p->param1,
				ecm_len))
			ret = SEC_HAL_RES_FAIL;
		else {
			ret = sec_hal_drm_derive_cw(id, ecm_len, ecm, &flags);
			put_user(flags, (uint32_t *)p->param3);
		}
	} else
		ret = SEC_HAL_RES_FAIL;

	return ret;
}


static inline uint32_t pad_size(uint32_t sz)
{
	return (sz+0xF) & ~0xF;
}


static inline uint32_t get_phys_addr(void *vaddr)
{
	uint32_t rv;

	if (PAGE_OFFSET <= (uint32_t)vaddr)
		rv = (uint32_t)virt_to_phys(vaddr);
	else {
		pgd_t *pgd = pgd_offset(current->mm, (uint32_t)vaddr);
		pmd_t *pmd = pmd_offset((pud_t *)pgd, (uint32_t)vaddr);
		pte_t *pte = pte_offset_map(pmd, (uint32_t)vaddr);
		rv = (0xFFFFF000 & (*pte)) | (0x00000FFF & (uint32_t)vaddr);
	}

	return rv;
}


#define IV_SZ 16
static uint32_t _drm_decrypt_video(
	struct _drm_data *data,
	sd_ioctl_params_t *p,
	struct platform_device *pdev)
{
	uint32_t id, ret, inl, outl = 0, phys_in = 0;
	uint8_t iv[IV_SZ*2] = {0};
	void *iv_usr = 0, *input = 0;

	id = p->param0;
	if (has_drm_session(data, id)) {
		iv_usr = (void *)p->param1;
		inl = p->param3;

		if (iv_usr && copy_from_user(iv, (const void *)iv_usr, IV_SZ))
			return SEC_HAL_RES_FAIL;

		input = kmalloc(pad_size(inl), GFP_KERNEL);
		if (!input)
			return -ENOMEM;

		if (copy_from_user(input, (const void *)p->param2, inl)) {
			kfree(input);
			return SEC_HAL_RES_FAIL;
		}

		phys_in = dma_map_single(&pdev->dev, input,
			pad_size(inl), DMA_TO_DEVICE);
		if (phys_in == 0) {
			SEC_HAL_TRACE("dma_map_single error: %d", phys_in);
			kfree(input);
			return -ENOMEM;
		}

		ret = sec_hal_drm_decrypt_video(id, iv, iv_usr ? IV_SZ : 0,
			inl, phys_in, p->param4,
			p->param5, &outl, iv+IV_SZ);
		dma_unmap_single(&pdev->dev, phys_in, pad_size(inl),
			DMA_TO_DEVICE);
		kfree(input);

		if (SEC_HAL_RES_OK == ret
			&& ((iv_usr && copy_to_user(iv_usr, iv+IV_SZ, IV_SZ))
				|| put_user(outl, (uint32_t *)p->reserved1)))
			ret = SEC_HAL_RES_FAIL;
	} else
		ret = SEC_HAL_RES_FAIL;

	return ret;
}


static uint32_t _drm_decrypt_audio(
	struct _drm_data *data,
	sd_ioctl_params_t *p,
	struct platform_device *pdev)
{
	uint32_t id, ret, inl, outl = 0, phys_in = 0, phys_out = 0;
	uint8_t iv[IV_SZ*2] = {0};
	void *iv_usr = 0, *input = 0, *output = 0;

	id = p->param0;
	if (has_drm_session(data, id)) {
		iv_usr = (void *)p->param1;
		inl = p->param3;

		if (iv_usr && copy_from_user(iv, iv_usr, IV_SZ))
			return SEC_HAL_RES_FAIL;

		input = kmalloc(pad_size(inl), GFP_KERNEL);
		output = kmalloc(pad_size(inl), GFP_KERNEL);
		if (!input || !output)
			return -ENOMEM;

		if (copy_from_user(input, (const void *)p->param2, inl)) {
			kfree(output);
			kfree(input);
			return SEC_HAL_RES_FAIL;
		}

		phys_in = dma_map_single(&pdev->dev, input, pad_size(inl),
			DMA_TO_DEVICE);
		if (phys_in == 0) {
			SEC_HAL_TRACE("dma_map_single error: %d", phys_in);
			kfree(output);
			kfree(input);
			return -ENOMEM;
		}

		phys_out = dma_map_single(&pdev->dev,
			output, pad_size(inl), DMA_FROM_DEVICE);
		if (phys_out == 0) {
			SEC_HAL_TRACE("dma_map_single error: %d", phys_out);
			kfree(output);
			kfree(input);
			return -ENOMEM;
		}

		ret = sec_hal_drm_decrypt_audio(id, iv, iv_usr ? IV_SZ : 0, inl,
			phys_in, phys_out, &outl, iv+IV_SZ);
		if (SEC_HAL_RES_OK == ret
			&& copy_to_user((void __user *)p->param4, output, outl))
			ret = SEC_HAL_RES_FAIL;

		dma_unmap_single(&pdev->dev, phys_in,
			pad_size(inl), DMA_TO_DEVICE);
		dma_unmap_single(&pdev->dev, phys_out,
			pad_size(inl), DMA_FROM_DEVICE);
		kfree(output);
		kfree(input);

		if (SEC_HAL_RES_OK == ret) {
			if ((iv_usr && copy_to_user(iv_usr, iv+IV_SZ, IV_SZ))
				|| put_user(outl, (uint32_t *)p->param5)) {
				SEC_HAL_TRACE("failed in iv/outl copy to usr");
				ret = SEC_HAL_RES_FAIL;
			}
		}
	} else
		ret = SEC_HAL_RES_FAIL;

	return ret;
}


static uint32_t _drm_wrap_keybox(sd_ioctl_params_t *p)
{
	uint32_t ret, kbox_size = 0, wr_kbox_size = 0, trs_key_size = 0;
	void *kbox = 0, *wr_kbox = 0, *trs_key = 0;
	void *kbox_usr = 0, *wr_kbox_usr = 0, *trs_key_usr = 0;

	kbox_usr = (void *)p->param1;
	kbox_size = p->param2;
	wr_kbox_usr = (void *)p->param3;
	trs_key_usr = (void *)p->param5;
	trs_key_size = p->reserved1;
	if (!kbox_usr || !kbox_size)
		return -ENOMEM;

	kbox = kmalloc(kbox_size, GFP_KERNEL);
	wr_kbox = wr_kbox_usr ? kmalloc(2*kbox_size, GFP_KERNEL) : 0;
	trs_key = trs_key_size ? kmalloc(trs_key_size, GFP_KERNEL) : 0;
	if (!kbox)
		return -ENOMEM;

	if (copy_from_user(kbox, (const void *)kbox_usr, kbox_size)) {
		kfree(trs_key);
		kfree(wr_kbox);
		kfree(kbox);
		return SEC_HAL_RES_FAIL;
	}
	if (copy_from_user(trs_key, (const void *)trs_key_usr, trs_key_size)) {
		kfree(trs_key);
		kfree(wr_kbox);
		kfree(kbox);
		return SEC_HAL_RES_FAIL;
	}

	ret = sec_hal_drm_wrap_keybox((uint8_t *)kbox,
		kbox_size, (uint8_t *)wr_kbox, &wr_kbox_size,
		(uint8_t *)trs_key, trs_key_size);

	if (SEC_HAL_RES_OK == ret
		&& (wr_kbox_usr && copy_to_user(wr_kbox_usr,
				wr_kbox, wr_kbox_size)))
		ret = SEC_HAL_RES_FAIL;

	if (put_user(wr_kbox_size, (uint32_t *)p->param4))
		ret = SEC_HAL_RES_FAIL;

	kfree(trs_key);
	kfree(wr_kbox);
	kfree(kbox);
	return ret;
}


static uint32_t _drm_install_keybox(sd_ioctl_params_t *p)
{
	uint32_t ret, kbox_size = 0;
	void *kbox = 0;

	kbox_size = p->param2;
	kbox = kmalloc(kbox_size, GFP_KERNEL);
	if (!kbox)
		return -ENOMEM;

	if (copy_from_user(kbox, (const void *)p->param1, kbox_size)) {
		kfree(kbox);
		return SEC_HAL_RES_FAIL;
	}

	ret = sec_hal_drm_install_keybox((uint8_t *)kbox, kbox_size);

	kfree(kbox);
	return ret;
}


static uint32_t _get_random(sd_ioctl_params_t *p)
{
	uint32_t ret, size;
	void *rand;

	size = p->param2;
	rand = kmalloc(size, GFP_KERNEL);
	if (!rand)
		return -ENOMEM;

	ret = sec_hal_drm_get_random(size, (uint8_t *)rand);
	if (SEC_HAL_RES_OK == ret
		&& copy_to_user((void __user *)p->param1, rand, size))
		ret = SEC_HAL_RES_FAIL;

	kfree(rand);
	return ret;
}


static uint32_t _drm_get_device_id(sd_ioctl_params_t *p)
{
	uint32_t ret, id_len, out_size = 0;
	void *id = 0;

	id_len = p->param1;
	if (!id_len)
		return -ENOMEM;

	id = kmalloc(id_len, GFP_KERNEL);
	if (!id)
		return -ENOMEM;

	ret = sec_hal_drm_get_device_id(id_len, (uint8_t *)id, &out_size);
	if (SEC_HAL_RES_OK == ret) {
		if (id_len < out_size)
			ret = SEC_HAL_RES_FAIL;
		else if (copy_to_user((void __user *)p->param2, id, out_size)
			|| put_user(out_size, (uint32_t __user *)p->param3))
			ret = SEC_HAL_RES_FAIL;
	}

	kfree(id);
	return ret;
}


static uint32_t _drm_get_keydata(sd_ioctl_params_t *p)
{
	uint32_t ret, keyd_len, out_size = 0;
	void *keyd = 0;

	keyd_len = p->param1;
	if (!keyd_len)
		return -ENOMEM;

	keyd = kmalloc(keyd_len, GFP_KERNEL);
	if (!keyd)
		return -ENOMEM;

	ret = sec_hal_drm_get_key_data(keyd_len, (uint8_t *)keyd, &out_size);
	if (SEC_HAL_RES_OK == ret) {
		if (keyd_len < out_size)
			ret = SEC_HAL_RES_FAIL;
		else if (copy_to_user((void *)p->param2, keyd, out_size)
			|| put_user(out_size, (uint32_t *)p->param3))
			ret = SEC_HAL_RES_FAIL;
	}

	kfree(keyd);
	return ret;
}


/* **************************************************************************
 * Function name      : sec_hal_drm_usr_init
 * Description        : DRM specific allocator. Data will be stored in
 *                      high level container, and it will be given as param
 *                      to DRM functions.
 * Return value       : long
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
long sec_hal_drm_usr_init(void **drm_data)
{
	long rv = 0;
	struct _drm_data *ptr;

	SEC_HAL_TRACE_ENTRY();

	do {
		if (*drm_data != NULL) { /* failsafe, double 'init' check */
			rv = EFAULT;
			break;
		}

		*drm_data = kmalloc(sizeof(struct _drm_data), GFP_KERNEL);
		if (*drm_data == NULL) {
			rv = ENOMEM;
			break;
		}

		ptr = (struct _drm_data *) *drm_data;
		ptr->session_id = LOCAL_NOT_DEFINED_ID;
	} while (0);

	SEC_HAL_TRACE_EXIT_INFO("rv = %d", rv);
	return rv;
}


/* **************************************************************************
 * Function name      : sec_hal_drm_usr_exit
 * Description        : DRM specific de-allocator. This should be called
 *                      also from high level close function impl. since
 *                      abnormal exit can cause mem leaks.
 * Return value       :
 * *************************************************************************/
void sec_hal_drm_usr_exit(void **drm_data)
{
	SEC_HAL_TRACE_ENTRY();

	if (drm_data && *drm_data) {
		struct _drm_data *ptr = (struct _drm_data *) *drm_data;
		if (ptr->session_id)
			sec_hal_drm_exit(1, &ptr->session_id);
		kfree(ptr);
		*drm_data = NULL;
	}

	SEC_HAL_TRACE_EXIT();
}


/* **************************************************************************
 * Function name      : sec_hal_drm_ioctl
 * Description        : entry from USR mode.
 * Return value       : long
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
long sec_hal_drm_ioctl(
	unsigned int cmd,
	void **data,
	sd_ioctl_params_t *param,
	struct platform_device *pdev)
{
	long rv = -EPERM;
	struct _drm_data *ptr = (struct _drm_data *)*data;

	switch (cmd) {
	case SD_DRM_ENTER_PLAY:
		if (ptr == NULL) {
			sec_hal_drm_usr_init(data);
			rv = _drm_enter((struct _drm_data *) *data, param);
		}
		break;
	case SD_DRM_EXIT_PLAY:
		rv = _drm_exit((struct _drm_data *) *data, param);
		sec_hal_drm_usr_exit(data);
		*data = NULL;
		break;
	case SD_DRM_SET_ENTIT_KEY:
		if (ptr)
			rv = _drm_set_entit_key(ptr, param);
		break;
	case SD_DRM_DER_CTL_WORD:
		if (ptr)
			rv = _drm_derive_cw(ptr, param);
		break;
	case SD_DRM_DECRYPT_AUDIO:
		if (ptr)
			rv = _drm_decrypt_audio(ptr, param, pdev);
		break;
	case SD_DRM_DECRYPT_VIDEO:
		if (ptr)
			rv = _drm_decrypt_video(ptr, param, pdev);
		break;
	case SD_DRM_VALID_KEYBOX:
		rv = sec_hal_drm_is_keybox_valid();
		break;
	case SD_DRM_DEVICE_ID_GET:
		rv = _drm_get_device_id(param);
		break;
	case SD_DRM_KEYDATA_GET:
		rv = _drm_get_keydata(param);
		break;
	case SD_RANDOM_NUMBER_GET:
		rv = _get_random(param);
		break;
	case SD_DRM_WRAP_KEYBOX:
		rv = _drm_wrap_keybox(param);
		break;
	case SD_DRM_INSTALL_KEYBOX:
		rv = _drm_install_keybox(param);
		break;
	default:
		rv = -EPERM;
		break;
	}

	return rv;
}


