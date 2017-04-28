/*
 * drivers/sec_hal/rt/src/sec_hal_tee.c
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
#include "sec_hal_tee.h"
#define SEC_HAL_TRACE_LOCAL_ENABLE
#include "sec_hal_rt_trace.h"
#include "sec_serv_api.h"
#include "sec_dispatch.h"
#include "sec_msg.h"

#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/slab.h>


#define LOCAL_DISP                     pub2sec_dispatcher
#define LOCAL_DEFAULT_DISP_FLAGS       0
#define LOCAL_DEFAULT_DISP_SPARE_PARAM 0
#define LOCAL_WMB()                    wmb()
#define LOCAL_RMB()                    rmb()


unsigned long sec_hal_memory_tablewalk(void *virt_addr);

/* **************************************************************************
 * Function name      : sec_hal_tee_initialize_context
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
uint32_t sec_hal_tee_initialize_context(
	const char *name,
	TEEC_Context *kernel_context)
{
	uint32_t sec_hal_status = TEEC_SUCCESS;
	uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_status = 0x00;
	uint32_t sec_serv_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	uint32_t *kernel_name = NULL;
	uint32_t name_length = 0;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*3;
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (NULL == in_msg || NULL == out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}
		if (name) {
			name_length = strlen(name) + 1;
			kernel_name = kmalloc(name_length, GFP_KERNEL);
			if (copy_from_user(kernel_name, name, name_length)) {
				SEC_HAL_TRACE("name copy_from_user failed, aborting!");
				sec_hal_status = TEEC_ERROR_GENERIC;
				break;
			}
		}

		/* write content to the input msg */
		/* 1 ) string size */
		/* 2 ) name */
		/* 3 ) context */
		if (sec_msg_param_write32(&in_handle,
				name_length,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("name_len write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				kernel_name ? (uint32_t)virt_to_phys(kernel_name) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_name write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				kernel_context ? (uint32_t)virt_to_phys(kernel_context) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_context write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_TEEC_InitializeContext,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_status
			|| SEC_MSG_STATUS_OK != sec_msg_status
			|| TEEC_SUCCESS != sec_serv_status) {
			SEC_HAL_TRACE("failed! disp==%d, msg==%d, serv==%d",
				ssa_disp_status, sec_msg_status, sec_serv_status);
			sec_hal_status = sec_serv_status;
			break;
		}
	} while (0);

	kfree(kernel_name);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}


/* **************************************************************************
 * Function name      : sec_hal_tee_initialize_context
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
uint32_t sec_hal_tee_finalize_context(TEEC_Context *kernel_context)
{
	uint32_t sec_hal_status = TEEC_SUCCESS;
	uint32_t ssa_disp_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (NULL == in_msg || NULL == out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}

		/* write content to the input msg */
		/* 1 ) context */
		if (sec_msg_param_write32(&in_handle,
				kernel_context ? (uint32_t)virt_to_phys(kernel_context) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_context write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_TEEC_FinalizeContext,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		if (SEC_ROM_RET_OK != ssa_disp_status) {
			SEC_HAL_TRACE("failed! disp==%d", ssa_disp_status);
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}


/* **************************************************************************
 * Function name      : sec_hal_tee_open_session
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
uint32_t sec_hal_tee_open_session(
	TEEC_Context *kernel_context,
	TEEC_Session *kernel_session,
	const TEEC_UUID *destination,
	uint32_t connectionMethod,
	const void *connectionData,
	TEEC_Operation *operation,
	uint32_t *returnOrigin)
{
	uint32_t sec_hal_status = TEEC_SUCCESS;
	uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_status = 0x00;
	uint32_t sec_serv_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	TEEC_UUID *kernel_destination = NULL;
	uint32_t *kernel_connectionData = NULL;
	TEEC_Operation *kernel_operation = NULL;
	uint32_t *kernel_returnOrigin = NULL;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*7;
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (NULL == in_msg || NULL == out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}
		if (destination == NULL) {
			SEC_HAL_TRACE("destination is NULL, aborting!");
			sec_hal_status = TEEC_ERROR_BAD_PARAMETERS;
			break;
		}
		kernel_destination = kmalloc(sizeof(TEEC_UUID), GFP_KERNEL);
		if (copy_from_user(kernel_destination, destination, sizeof(TEEC_UUID))) {
			SEC_HAL_TRACE("dest copy_from_user failed, aborting!");
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}
		if (connectionMethod != TEEC_LOGIN_USER && connectionData) {
			kernel_connectionData = kmalloc(sizeof(uint32_t), GFP_KERNEL);
			if(copy_from_user(kernel_connectionData, connectionData, sizeof(uint32_t))) {
				SEC_HAL_TRACE("conn_data copy_from_user failed, aborting!");
				sec_hal_status = TEEC_ERROR_GENERIC;
				break;
			}
		}
		if (operation) {
			kernel_operation = kmalloc(sizeof(TEEC_Operation), GFP_KERNEL);
			if (copy_from_user(kernel_operation, operation, sizeof(TEEC_Operation))) {
				SEC_HAL_TRACE("operation copy_from_user failed, aborting!");
				sec_hal_status = TEEC_ERROR_GENERIC;
				break;
			}
			sec_hal_tee_convert_memrefs(NULL, kernel_operation, 1);
		}
		if (returnOrigin) {
			kernel_returnOrigin = kmalloc(sizeof(uint32_t), GFP_KERNEL);
			if (copy_from_user(kernel_returnOrigin, returnOrigin, sizeof(uint32_t))) {
				SEC_HAL_TRACE("ret_orig copy_from_user failed, aborting!");
				sec_hal_status = TEEC_ERROR_GENERIC;
				break;
			}
		}

		/* write content to the input msg */
		/* 1 ) context */
		/* 2 ) session */
		/* 3 ) destination */
		/* 4 ) connectionMethod */
		/* 5 ) connectionData (NULL) */
		/* 6 ) operation (NULL) */
		/* 7 ) returnOrigin (NULL) */
		if (sec_msg_param_write32(&in_handle,
				kernel_context ? (uint32_t)virt_to_phys(kernel_context) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_context write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				kernel_session ? (uint32_t)virt_to_phys(kernel_session) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_session write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				kernel_destination ? (uint32_t)virt_to_phys(kernel_destination) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_session write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				connectionMethod,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("conn_method write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				kernel_connectionData ? (uint32_t)virt_to_phys(kernel_connectionData) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("conn_data write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				kernel_operation ? (uint32_t)virt_to_phys(kernel_operation) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_op write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				kernel_returnOrigin ? (uint32_t)virt_to_phys(kernel_returnOrigin) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("ret_orig write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_TEEC_OpenSession,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_status
			|| SEC_MSG_STATUS_OK != sec_msg_status
			|| TEEC_SUCCESS != sec_serv_status) {
			SEC_HAL_TRACE("failed! disp==%d, msg==%d, serv==%d",
				ssa_disp_status, sec_msg_status, sec_serv_status);
			sec_hal_status = sec_serv_status;
			break;
		}
	} while (0);

	kfree(kernel_returnOrigin);
	if (kernel_operation) {
		sec_hal_tee_convert_memrefs(operation, kernel_operation, 0);
		kfree(kernel_operation);
	}
	kfree(kernel_connectionData);
	kfree(kernel_destination);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}


/* **************************************************************************
 * Function name      : sec_hal_tee_close_session
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
uint32_t sec_hal_tee_close_session(TEEC_Session *kernel_session)
{
	uint32_t sec_hal_status = TEEC_SUCCESS;
	uint32_t ssa_disp_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (NULL == in_msg || NULL == out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}

		/* write content to the input msg */
		/* 1 ) session */
		if (sec_msg_param_write32(&in_handle,
				kernel_session ? (uint32_t)virt_to_phys(kernel_session) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_session write error, aborting!");
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_TEEC_CloseSession,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		if (SEC_ROM_RET_OK != ssa_disp_status) {
			SEC_HAL_TRACE("failed! disp==%d", ssa_disp_status);
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}


/* **************************************************************************
 * Function name      : sec_hal_tee_invoke_command
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
uint32_t sec_hal_tee_invoke_command(
	TEEC_Session *kernel_session,
	uint32_t commandID,
	TEEC_Operation *operation,
	uint32_t *returnOrigin)
{
	uint32_t sec_hal_status = TEEC_SUCCESS;
	uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_status = 0x00;
	uint32_t sec_serv_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	TEEC_Operation *kernel_operation = NULL;
	uint32_t *kernel_returnOrigin = NULL;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*4;
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (NULL == in_msg || NULL == out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}
		if (operation) {
			kernel_operation = kmalloc(sizeof(TEEC_Operation), GFP_KERNEL);
			if(copy_from_user(kernel_operation, operation, sizeof(TEEC_Operation))) {
				SEC_HAL_TRACE("operation copy_from_user failed, aborting!");
				sec_hal_status = TEEC_ERROR_GENERIC;
				break;
			}
			sec_hal_tee_convert_memrefs(NULL, kernel_operation, 1);
		}
		if (returnOrigin) {
			kernel_returnOrigin = kmalloc(sizeof(uint32_t), GFP_KERNEL);
			if (copy_from_user(kernel_returnOrigin, returnOrigin, sizeof(uint32_t))) {
				SEC_HAL_TRACE("ret_orig copy_from_user failed, aborting!");
				sec_hal_status = TEEC_ERROR_GENERIC;
				break;
			}
		}

		/* write content to the input msg */
		/* 1 ) session */
		/* 2 ) commandID */
		/* 3 ) operation */
		/* 4 ) returnOrigin */
		if (sec_msg_param_write32(&in_handle,
				kernel_session ? (uint32_t)virt_to_phys(kernel_session) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_session write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				commandID,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("commandId write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				kernel_operation ? (uint32_t)virt_to_phys(kernel_operation) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_op write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				kernel_returnOrigin ? (uint32_t)virt_to_phys(kernel_returnOrigin) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("ret_orig write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_TEEC_InvokeCommand,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_status
			|| SEC_MSG_STATUS_OK != sec_msg_status
			|| TEEC_SUCCESS != sec_serv_status) {
			SEC_HAL_TRACE("failed! disp==%d, msg==%d, serv==%d",
				ssa_disp_status, sec_msg_status, sec_serv_status);
			sec_hal_status = sec_serv_status;
			break;
		}
		if (kernel_returnOrigin == NULL)
			break; /* no more args, can exit now. */
		if(copy_to_user(returnOrigin, kernel_returnOrigin, sizeof(uint32_t))) {
			SEC_HAL_TRACE("ret_orig copy_from_user failed, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);

	kfree(kernel_returnOrigin);
	if (kernel_operation) {
		sec_hal_tee_convert_memrefs(operation, kernel_operation, 0);
		kfree(kernel_operation);
	}

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}


/* **************************************************************************
 * Function name      : sec_hal_tee_register_shared_memory_area
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
uint32_t sec_hal_tee_register_shared_memory_area(
	TEEC_Context *kernel_context,
	TEEC_SharedMemory *kernel_shmem)
{
	uint32_t sec_hal_status = TEEC_SUCCESS;
	uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_status = 0x00;
	uint32_t sec_serv_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*2;
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (NULL == in_msg || NULL == out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}
		if (kernel_context == NULL) {
			SEC_HAL_TRACE("context is null");
			sec_hal_status = TEEC_ERROR_BAD_PARAMETERS;
			break;
		}
		if (kernel_shmem == NULL) {
			SEC_HAL_TRACE("kernel_shmem is null");
			sec_hal_status = TEEC_ERROR_BAD_PARAMETERS;
			break;
		}

		/* write content to the input msg */
		/* 1 ) context */
		/* 2 ) shared memory */
		if (sec_msg_param_write32(&in_handle,
				(uint32_t)virt_to_phys(kernel_context),
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_context write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				(uint32_t)virt_to_phys(kernel_shmem),
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_shmem write error, aborting!");
			sec_hal_status = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_TEEC_RegisterSharedMemory,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != ssa_disp_status
			|| SEC_MSG_STATUS_OK != sec_msg_status
			|| TEEC_SUCCESS != sec_serv_status) {
			SEC_HAL_TRACE("failed! disp==%d, msg==%d, serv==%d",
				ssa_disp_status, sec_msg_status, sec_serv_status);
			sec_hal_status = sec_serv_status;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}


/* **************************************************************************
 * Function name      : sec_hal_tee_release_shared_memory_area
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
uint32_t sec_hal_tee_release_shared_memory_area(TEEC_SharedMemory *kernel_shmem)
{
	uint32_t sec_hal_status = TEEC_SUCCESS;
	uint32_t ssa_disp_status = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (NULL == in_msg || NULL == out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}

		/* write content to the input msg */
		/* 1 ) shared memory pointer */
		if (sec_msg_param_write32(&in_handle,
				kernel_shmem ? (uint32_t)virt_to_phys(kernel_shmem) : 0x00,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("kernel_shmem write error, aborting!");
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_TEEC_ReleaseSharedMemory,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		if (SEC_ROM_RET_OK != ssa_disp_status) {
			SEC_HAL_TRACE("failed! disp==%d ", ssa_disp_status);
			sec_hal_status = TEEC_ERROR_GENERIC;
			break;
		}
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}


/* **************************************************************************
 * Function name      : sec_hal_tee_convert_memrefs
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
void sec_hal_tee_convert_memrefs(
	TEEC_Operation *user_operation,
	TEEC_Operation *operation,
	uint32_t direction)
{
	uint32_t i;

	SEC_HAL_TRACE_ENTRY();

	for (i = 0; i < 4; i++) {
		uint32_t param_type = TEEC_PARAM_TYPE_GET(operation->paramTypes, i);

		if(param_type == TEEC_MEMREF_WHOLE
			|| param_type == TEEC_MEMREF_PARTIAL_INPUT
			|| param_type == TEEC_MEMREF_PARTIAL_OUTPUT
			|| param_type == TEEC_MEMREF_PARTIAL_INOUT) {
			if (direction == 1) {
				TEEC_SharedMemory *kernelSM;

				kernelSM = kmalloc(sizeof(TEEC_SharedMemory), GFP_KERNEL);
				if (copy_from_user(kernelSM, operation->params[i].memref.parent, sizeof(TEEC_SharedMemory))) {
					SEC_HAL_TRACE("copy_from_user failed in line: %d", __LINE__);
				}

				operation->params[i].memref.parent = kernelSM;
				SEC_HAL_TRACE("after copy: operation->params[%d].memref.parent: 0x%x",  i, operation->params[i].memref.parent);
				operation->params[i].memref.parent->buffer = (void *)sec_hal_memory_tablewalk(operation->params[i].memref.parent->buffer);
				SEC_HAL_TRACE("after conversion: operation->params[%d].memref.parent->buffer: 0x%x", i,  operation->params[i].memref.parent->buffer);
				operation->params[i].memref.parent = (void *)sec_hal_memory_tablewalk(operation->params[i].memref.parent);
				SEC_HAL_TRACE("after conversion: operation->params[%d].memref.parent: 0x%x",  i, operation->params[i].memref.parent);
			} else {
				operation->params[i].memref.parent = (void *)phys_to_virt((phys_addr_t)operation->params[i].memref.parent);
				SEC_HAL_TRACE("kfree for 0x%x",  i, operation->params[i].memref.parent);
				kfree(operation->params[i].memref.parent);
			}
		} else if (param_type == TEEC_MEMREF_TEMP_INPUT
			|| param_type == TEEC_MEMREF_TEMP_OUTPUT
			|| param_type == TEEC_MEMREF_TEMP_INOUT) {
			if (direction == 1) {
				SEC_HAL_TRACE("operation->params[%d].tmpref.buffer: 0x%x",  i, operation->params[i].tmpref.buffer);
				operation->params[i].tmpref.buffer = (void *)virt_to_phys(operation->params[i].tmpref.buffer);
				SEC_HAL_TRACE("after conversion: operation->params[%d].tmpref.buffer: 0x%x",  i, operation->params[i].tmpref.buffer);
			} else {
				SEC_HAL_TRACE("PHYS:operation->params[%d].tmpref.buffer: 0x%x",  i, operation->params[i].tmpref.buffer);
				operation->params[i].tmpref.buffer = (void *)phys_to_virt((phys_addr_t)operation->params[i].tmpref.buffer);
				SEC_HAL_TRACE("VIRT:operation->params[%d].tmpref.buffer: 0x%x",  i, operation->params[i].tmpref.buffer);
			}
		} else if (param_type == TEEC_VALUE_INPUT
			|| param_type == TEEC_VALUE_OUTPUT
			|| param_type == TEEC_VALUE_INOUT) {
			if(direction != 1) {
				user_operation->params[i].value.a = operation->params[i].value.a;
				user_operation->params[i].value.b = operation->params[i].value.b;
			}
		}
	}

	SEC_HAL_TRACE_EXIT();
}



/* ******************************** END ********************************** */
