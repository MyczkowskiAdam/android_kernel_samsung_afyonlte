/*
 * drivers/sec_hal/rt/src/sec_hal_cmn.c
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

#include "sec_hal_cmn.h"
#include "sec_msg.h"
#include "sec_serv_api.h"
#include "sec_dispatch.h"
#include "sec_hal_rt_cmn.h"
#include "sec_hal_rt_trace.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/system.h>
#include <linux/slab.h>


#define LOCAL_DISP                        pub2sec_dispatcher
#define LOCAL_MSG_BYTE_ORDER              SEC_MSG_BYTE_ORDER_LE
#define LOCAL_DEFAULT_DISP_FLAGS          0
#define LOCAL_DEFAULT_DISP_SPARE_PARAM    0
#define LOCAL_WMB()                       wmb()
#define LOCAL_RMB()                       rmb()


static sec_reset_info *sec_hal_reset_info;


/* **********************************************************************
 * Function name      : sec_hal_memcpy
 * Description        : request memcpy to TZ protected memory areas.
 *                      Physical buffers should be cache cleaned before this
 *                      operation, since RAM is directly read. Otherwise
 *                      unwanted failures can occur.
 * Parameters         : IN/--- uint32_t phys_dst
 *                      IN/--- uint32_t phys_src
 *                      IN/--- uint32_t size
 * Return value       : uint32
 *                      ==SEC_HAL_CMN_RES_OK operation successful
 *                      failure otherwise.
 * *********************************************************************/
uint32_t sec_hal_memcpy(
	uint32_t phys_dst,
	uint32_t phys_src,
	uint32_t size)
{
	uint32_t sec_api_st = SEC_HAL_CMN_RES_OK;
	uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*3;
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
		sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, abort!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle, phys_dst,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("phys_dst write error, abort!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle, phys_src,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("phys_src write error, abort!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle, size,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("size write error, abort!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(SEC_SERV_MEMCPY_REQUEST,
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
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_api_st;
}
/* made available for other kernel entities */
EXPORT_SYMBOL(sec_hal_memcpy);


/* **********************************************************************
 * Function name      : sec_hal_authenticate
 * Description        : Authenticate memory area with given SWCERT.
 *                      Physical buffers should be cache cleaned before this
 *                      operation, since RAM is directly read. Otherwise
 *                      unwanted failures can occur.
 * Parameters         : IN/--- uint32_t phys_cert_addr
 *                      IN/--- uint32_t cert_size
 *                      OUT/--- uint32_t *obj_id, object id if successful
 * Return value       : uint32
 *                      ==SEC_HAL_CMN_RES_OK operation successful
 *                      failure otherwise.
 * *********************************************************************/
uint32_t sec_hal_authenticate(
	uint32_t phys_cert_addr,
	uint32_t cert_size,
	uint32_t *obj_id)
{
	uint32_t sec_api_st = SEC_HAL_CMN_RES_OK;
	uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
		sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, abort!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle, phys_cert_addr,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("phys_cert_addr write error, abort!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(SEC_SERV_CERTIFICATE_REGISTER,
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
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}

		if (obj_id == NULL)
			break; /* objid arg not given, can exit now. */
		sec_msg_st = sec_msg_param_read32(&out_handle, obj_id);
		if (SEC_MSG_STATUS_OK != sec_msg_st) {
			SEC_HAL_TRACE("failed to read objid! msg==%d",
				sec_msg_st);
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB();

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_api_st;
}
/* made available for other kernel entities */
EXPORT_SYMBOL(sec_hal_authenticate);


/* **********************************************************************
 * Function name      : sec_hal_memfw_attr_reserve
 * Description        : Reserve memory firewalling attributes from secenv
 * Parameters         : IN/--- uint32_t area_id, HDMI/OMX identifier.
 *                      IN/--- uint32_t phys_start_addr, start of protm.
 *                      OUT/--- uint32_t phys_end_addr, end of protm.
 * Return value       : uint32
 *                      ==SEC_HAL_CMN_RES_OK operation successful
 *                      failure otherwise.
 * *********************************************************************/
uint32_t sec_hal_memfw_attr_reserve(
	uint32_t area_id,
	uint32_t phys_start_addr,
	uint32_t phys_end_addr)
{
	uint32_t sec_api_st = SEC_HAL_CMN_RES_OK;
	uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*3;
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				area_id,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("area_id write error, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				phys_start_addr,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("phys_start_addr write error, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				phys_end_addr,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("phys_end_addr write error, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(SEC_SERV_RESERVE_MEDIA_AREA,
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
			SEC_HAL_TRACE("failure! disp==%d, msg==%d, serv==%d",
				ssa_disp_st, sec_msg_st, sec_serv_st);
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_api_st;
}
/* made available for other kernel entities */
EXPORT_SYMBOL(sec_hal_memfw_attr_reserve);


/* **********************************************************************
 * Function name      : sec_hal_memfw_attr_free
 * Description        : Free resources previously reserved for memory fw.
 * Parameters         : IN/--- uint32_t area_id, HDMI/OMX identifier.
 * Return value       : uint32
 *                      ==SEC_HAL_CMN_RES_OK operation successful
 *                      failure otherwise.
 * *********************************************************************/
uint32_t sec_hal_memfw_attr_free(uint32_t area_id)
{
	uint32_t sec_api_st = SEC_HAL_CMN_RES_OK;
	uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				area_id,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("area_id write error, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(SEC_SERV_FREE_MEDIA_AREA,
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
			SEC_HAL_TRACE("failure! disp==%d, msg==%d, serv==%d",
				ssa_disp_st, sec_msg_st, sec_serv_st);
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_api_st;
}
/* made available for other kernel entities */
EXPORT_SYMBOL(sec_hal_memfw_attr_free);


/* ****************************************************************************
 * Function name      : sec_hal_reset_info_addr_register
 * Description        : Register address for secure reset information to be
 *                      read later.
 * Parameters         : IN/--- None
 * Return value       : uint32
 *                      ==SEC_HAL_CMN_RES_OK operation successful
 *                      failure otherwise.
 * ***************************************************************************/
uint32_t sec_hal_reset_info_addr_register(void)
{
	uint32_t sec_api_st = SEC_HAL_CMN_RES_OK;
	uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	sec_hal_reset_info = kmalloc(sizeof(sec_reset_info), GFP_KERNEL);
	memset(sec_hal_reset_info, 0x00, sizeof(sec_reset_info));

	/* allocate memory for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*2;
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}

		/* write the reset info address to the input msg */
		if (sec_msg_param_write32(&in_handle,
				virt_to_phys(sec_hal_reset_info),
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("sec_reset_info werr, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		/* write size of the info struct to the input msg */
		if (sec_msg_param_write32(&in_handle,
				sizeof(sec_reset_info),
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("sec_reset_info sz werr, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(SEC_SERV_RESET_INFO_ADDR_REGISTER,
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
			SEC_HAL_TRACE("failure! disp==%d, msg==%d, serv==%d",
				ssa_disp_st, sec_msg_st, sec_serv_st);
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_api_st;
}
/* made available for other kernel entities */
EXPORT_SYMBOL(sec_hal_reset_info_get);


/* ****************************************************************************
 * Function name      : sec_hal_reset_info_get
 * Description        : Get secure reset reason
 * Parameters         : IN/--- sec_reset_info * reset_info, pointer where reset
 *                      information will be updated.
 * Return value       : uint32
 *                      ==SEC_HAL_CMN_RES_OK operation successful
 *                      failure otherwise.
 * ***************************************************************************/
uint32_t sec_hal_reset_info_get(sec_reset_info *reset_info)
{
	SEC_HAL_TRACE_ENTRY();

	if (sec_hal_reset_info == NULL) {
		SEC_HAL_TRACE_EXIT();
		return SEC_HAL_CMN_RES_FAIL;
	} else {
		memcpy(reset_info, sec_hal_reset_info, sizeof(sec_reset_info));
		SEC_HAL_TRACE_EXIT();
		return SEC_HAL_CMN_RES_OK;
	}
}

/* ****************************************************************************
 * Function name      : sec_hal_dbg_reg_set
 * Description        : Set DBGREG values to TZ.
 *                      Will write registers back for assertion.
 *                      Some regs/bit configuration are not allowed, thus
 *                      assertion required by requestor.
 * Parameters         : IN/OUT--- uint32_t *dbgreg1, pointer to in/out value.
 *                      IN/OUT--- uint32_t *dbgreg2, pointer to in/out value.
 *                      IN/OUT--- uint32_t *dbgreg3, pointer to in/out value.
 * Return value       : uint32
 *                      ==SEC_HAL_CMN_RES_OK operation successful
 *                      failure otherwise.
 * ***************************************************************************/
uint32_t sec_hal_dbg_reg_set(
	uint32_t *dbgreg1,
	uint32_t *dbgreg2,
	uint32_t *dbgreg3)
{
	uint32_t sec_api_st = SEC_HAL_CMN_RES_OK;
	uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*3;
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
		sec_msg_param_size(sizeof(uint32_t))*3;
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				dbgreg1 ? *dbgreg1 : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("dbgreg1 write error, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				dbgreg2 ? *dbgreg2 : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("dbgreg2 write error, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				dbgreg3 ? *dbgreg3 : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("dbgreg3 write error, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = raw_pub2sec_dispatcher(SEC_SERV_DEBUG_CONTROL_DATA_SET,
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
			SEC_HAL_TRACE("failure! disp==%d, msg==%d, serv==%d",
				ssa_disp_st, sec_msg_st, sec_serv_st);
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		if (dbgreg1) {
			sec_msg_st = sec_msg_param_read32(&out_handle, dbgreg1);
			if (SEC_MSG_STATUS_OK != sec_msg_st) {
				SEC_HAL_TRACE("failed to read reg1! msg==%d",
					sec_msg_st);
				sec_api_st = SEC_HAL_CMN_RES_FAIL;
				break;
			}
		}
		if (dbgreg2) {
			sec_msg_st = sec_msg_param_read32(&out_handle, dbgreg2);
			if (SEC_MSG_STATUS_OK != sec_msg_st) {
				SEC_HAL_TRACE("failed to read reg2! msg==%d",
					sec_msg_st);
				sec_api_st = SEC_HAL_CMN_RES_FAIL;
				break;
			}
		}
		if (dbgreg3) {
			sec_msg_st = sec_msg_param_read32(&out_handle, dbgreg3);
			if (SEC_MSG_STATUS_OK != sec_msg_st) {
				SEC_HAL_TRACE("failed to read reg3! msg==%d",
					sec_msg_st);
				sec_api_st = SEC_HAL_CMN_RES_FAIL;
				break;
			}
		}
	} while (0);
	LOCAL_RMB();

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_api_st;
}


/* ****************************************************************************
 * Function name      : sec_hal_public_id_get
 * Description        : stores device's Public ID to the given address.
 * Parameters         : IN/--- sec_hal_public_id_t *pub_id
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * ***************************************************************************/
uint32_t sec_hal_public_id_get(sec_hal_public_id_t *pub_id)
{
	uint32_t sec_hal_st = SEC_HAL_CMN_RES_OK;
	uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(SEC_HAL_MAX_PUBLIC_ID_LENGTH);
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, aborting!");
			sec_hal_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}

		if (sec_msg_param_write32(&in_handle,
				SEC_HAL_MAX_PUBLIC_ID_LENGTH,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("alloc failure, aborting!");
			sec_hal_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher(the interrupter) */
		ssa_disp_st = LOCAL_DISP(SEC_SERV_PUBLIC_ID_REQUEST,
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
			SEC_HAL_TRACE("failure! disp==%d, msg==%d, serv==%d",
				ssa_disp_st, sec_msg_st, sec_serv_st);
			sec_hal_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		sec_msg_st = sec_msg_param_read(&out_handle,
			pub_id->public_id,
			SEC_HAL_MAX_PUBLIC_ID_LENGTH);
		if (SEC_MSG_STATUS_OK != sec_msg_st) {
			SEC_HAL_TRACE("failed to read pub_id! msg==%d",
				sec_msg_st);
			sec_hal_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB();

	/* de-allocate msg */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_st;
}

/* ****************************************************************************
 * Function name      : sec_hal_dbg_reg_set
 * Description        : Set DBGREG values to TZ.
 *                      Will write registers back for assertion.
 *                      Some regs/bit configuration are not allowed, thus
 *                      assertion required by requestor.
 * Parameters         : IN/OUT--- uint32_t *dbgreg1, pointer to in/out value.
 *                      IN/OUT--- uint32_t *dbgreg2, pointer to in/out value.
 *                      IN/OUT--- uint32_t *dbgreg3, pointer to in/out value.
 * Return value       : uint32
 *                      ==SEC_HAL_CMN_RES_OK operation successful
 *                      failure otherwise.
 * ***************************************************************************/
uint32_t sec_hal_arbmode_set(uint32_t *arbmode)
{
	uint32_t sec_api_st = SEC_HAL_CMN_RES_OK;
	uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_st = 0x00;
	uint32_t sec_serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0,
		LOCAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				arbmode ? *arbmode : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("arbmode write error, aborting!");
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = raw_pub2sec_dispatcher(SEC_SERV_ARBMODE_SET,
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
			SEC_HAL_TRACE("failure! disp==%d, msg==%d, serv==%d",
				ssa_disp_st, sec_msg_st, sec_serv_st);
			sec_api_st = SEC_HAL_CMN_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB();

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_api_st;
}


