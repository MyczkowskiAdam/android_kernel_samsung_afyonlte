/*
 * drivers/sec_hal/rt/src/sec_hal_rt.c
 *
 * Copyright (c) 2010-2013, Renesas Mobile Corporation.
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


/* ************************ HEADER (INCLUDE) SECTION ********************* */

#define SEC_HAL_TRACE_LOCAL_ENABLE

#include "sec_hal_rt.h"
#include "sec_hal_rt_cmn.h"
#include "sec_hal_rt_trace.h"
#include "sec_serv_api.h"
#include "sec_msg.h"
#include "sec_dispatch.h"
#include "sec_hal_dev_ioctl.h"

#ifndef SEC_HAL_TEST_ISOLATION
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#else
#include <string.h>
#endif /* SEC_HAL_TEST_ISOLATION */


/* ***************** MACROS, CONSTANTS, COMPILATION FLAGS **************** */
#define LOCAL_DISP                         pub2sec_dispatcher
#define LOCAL_DEFAULT_DISP_FLAGS           0
#define LOCAL_DEFAULT_DISP_SPARE_PARAM     0
#define LOCAL_WMB()                        wmb()
#define LOCAL_RMB()                        rmb()
#define LOCAL_DEFAULT_PERIODIC_TIMER_VALUE 60000
#define LOCAL_DEFAULT_SIMLOCK_LEVEL_COUNT  5

#define LOCAL_CPY_TO_CLIENT_FUNC    copy_to_user
#define LOCAL_CPY_TO_CLIENT_FUNCPTR &copy_to_user



/* empty unlock code for simlock */
const char* k_sec_hal_simlock_empty_code = "EMPTY";



/* ****************************** CODE SECTION *************************** */
/* **************************************************************************
** Function name      : sec_hal_rt_init
** Description        : initialize global/local resources.
** Parameters         : OUT/--- sec_hal_init_info_t *runtime_int,
**                      version information and reset register values.
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_init(sec_hal_init_info_t *runtime_init)
{
    uint32_t sec_hal_st = SEC_HAL_RES_OK;
    uint32_t sec_msg_st = SEC_MSG_STATUS_OK;
    uint32_t ssa_disp_st = 0x00;
    uint32_t sec_serv_st = 0x00;
    uint16_t size;
    sec_msg_t *in_msg = NULL;
    sec_msg_t *out_msg = NULL;
    sec_msg_handle_t in_handle;
    sec_msg_handle_t out_handle;

    SEC_HAL_TRACE_ENTRY();

    /* allocate memory, from ICRAM, for msgs to be sent to TZ */
    size = sec_msg_param_size(sizeof(uint32_t))*2;
    in_msg = sec_msg_alloc(&in_handle, size, SEC_MSG_OBJECT_ID_NONE,
                0, SEC_HAL_MSG_BYTE_ORDER);
    size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
            sec_msg_param_size(sizeof(uint32_t))*2 +
            sec_msg_param_size(sizeof(uint64_t)) +
            sec_msg_param_size(sizeof(uint32_t)*SEC_HAL_RESCNT_COUNT);
    out_msg = sec_msg_alloc(&out_handle, size, SEC_MSG_OBJECT_ID_NONE,
                0, SEC_HAL_MSG_BYTE_ORDER);

    do {
        if (!in_msg || !out_msg) {
            SEC_HAL_TRACE("alloc failure, msgs not sent!");
            sec_hal_st = SEC_HAL_RES_FAIL;
            break;
        }

        /* write content to the input msg */
        if (sec_msg_param_write32(&in_handle,
               sizeof(uint64_t),
               SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
            SEC_HAL_TRACE("commit_id_length write error, aborting!");
            sec_hal_st = SEC_HAL_RES_FAIL;
            break;
        }
        if (sec_msg_param_write32(&in_handle,
               sizeof(uint32_t)*SEC_HAL_RESCNT_COUNT,
               SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
            SEC_HAL_TRACE("reset_info_length write error, aborting!");
            sec_hal_st = SEC_HAL_RES_FAIL;
            break;
        }
        LOCAL_WMB();


        /* call dispatcher */
        ssa_disp_st = LOCAL_DISP(SEC_SERV_RUNTIME_INIT,
                LOCAL_DEFAULT_DISP_FLAGS,
                LOCAL_DEFAULT_DISP_SPARE_PARAM,
                SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
                SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

        /* interpret the response(s) */
        sec_msg_st = sec_msg_param_read32(&out_handle, &sec_serv_st);
        LOCAL_RMB();
        if (SEC_ROM_RET_OK != ssa_disp_st ||
            SEC_MSG_STATUS_OK != sec_msg_st ||
            SEC_SERV_STATUS_OK != sec_serv_st) {
            SEC_HAL_TRACE("failure! disp==%d, msg==%d, serv==%d",
                    ssa_disp_st, sec_msg_st, sec_serv_st);
            sec_hal_st = SEC_HAL_RES_FAIL;
            break;
        }

        if (!runtime_init) {
            break; /* runtime_init arg not given, can exit now. */
        }

        sec_msg_st = sec_msg_param_read32(&out_handle, (uint32_t*)&size);
        LOCAL_RMB();
        if (SEC_MSG_STATUS_OK != sec_msg_st ||
            sec_msg_param_read(&out_handle,
                &(runtime_init->commit_id),
                size < sizeof(uint64_t) ?
                size : sizeof(uint64_t)) != SEC_MSG_STATUS_OK) {
            SEC_HAL_TRACE("commit_id read error, aborting!");
            sec_hal_st = SEC_HAL_RES_FAIL;
            break;
        }
        sec_msg_st = sec_msg_param_read32(&out_handle, (uint32_t*)&size);
        LOCAL_RMB();
        if (SEC_MSG_STATUS_OK != sec_msg_st ||
            sec_msg_param_read(&out_handle,
                &(runtime_init->reset_info[0]),
                size < sizeof(uint32_t)*SEC_HAL_RESCNT_COUNT ?
                size : sizeof(uint32_t)*SEC_HAL_RESCNT_COUNT)
                    != SEC_MSG_STATUS_OK) {
            SEC_HAL_TRACE("reset_info read error, aborting!");
            sec_hal_st = SEC_HAL_RES_FAIL;
            break;

        }
        LOCAL_RMB(); /* to ensure last read being completed */
    } while(0);

    /* de-allocate msgs */
    sec_msg_free(out_msg);
    sec_msg_free(in_msg);


    SEC_HAL_TRACE_EXIT();
    return sec_hal_st;
}


/* **************************************************************************
** Function name      : sec_hal_rt_key_info_get
** Description        : request key information from the TZ side.
** Parameters         : OUT/--- sec_hal_key_info_t *key_info
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_key_info_get(sec_hal_key_info_t *user_key_info_out)
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

    SEC_HAL_TRACE_ENTRY();

    if (NULL == user_key_info_out) {
        SEC_HAL_TRACE_EXIT_INFO("!!null user_key_info_out, aborting!!");
        return SEC_HAL_RES_PARAM_ERROR;
    }

    /* allocate memory, from ICRAM, for msgs to be sent to TZ */
    msg_data_size = sec_msg_param_size(sizeof(uint32_t));
    in_msg = sec_msg_alloc(&in_handle, msg_data_size,
            SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);
    msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
            sec_msg_param_size(SEC_HAL_KEY_INFO_SIZE);
    out_msg = sec_msg_alloc(&out_handle, msg_data_size,
            SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);

    if (NULL == in_msg || NULL == out_msg) {
        SEC_HAL_TRACE("alloc failure, msgs not sent!");
        sec_hal_status = SEC_HAL_RES_FAIL;
    }
    else {
        /* write content to the input msg */
        sec_msg_param_write32(
                &in_handle,
                SEC_HAL_KEY_INFO_SIZE,
                SEC_MSG_PARAM_ID_NONE);

        /* call dispatcher */
        ssa_disp_status = LOCAL_DISP(
                SEC_SERV_KEY_INFO_REQUEST,
                LOCAL_DEFAULT_DISP_FLAGS,
                LOCAL_DEFAULT_DISP_SPARE_PARAM,
                SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
                SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

        /* interpret the response */
        sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
        if (SEC_ROM_RET_OK != ssa_disp_status ||
            SEC_MSG_STATUS_OK != sec_msg_status ||
            SEC_SERV_STATUS_OK != sec_serv_status) {
            SEC_HAL_TRACE("op failed, KEY INFO not available!");
            SEC_HAL_TRACE_INT("ssa_disp_status", ssa_disp_status);
            SEC_HAL_TRACE_INT("sec_msg_status ", sec_msg_status);
            SEC_HAL_TRACE_INT("sec_serv_status", sec_serv_status);
            sec_hal_status = SEC_HAL_RES_FAIL;
        }
        else {
            sec_msg_status = _sec_msg_param_read(LOCAL_CPY_TO_CLIENT_FUNCPTR,
                    &out_handle, user_key_info_out, SEC_HAL_KEY_INFO_SIZE);
            if (SEC_MSG_STATUS_OK != sec_msg_status) {
                SEC_HAL_TRACE_INT("read failed! sec_msg_status", sec_msg_status);
                sec_hal_status = SEC_HAL_RES_FAIL;
            }
        }
    }

    /* de-allocate msgs */
    sec_msg_free(out_msg);
    sec_msg_free(in_msg);

    SEC_HAL_TRACE_EXIT();
    return sec_hal_status;
}

/* **************************************************************************
** Function name      : sec_hal_rt_cert_register
** Description        : register given certificate on TZ side.
** Parameters         : IN/--- void *cert
**                      IN/--- uint32_t cert_size
**                      OUT/ --- uint32_t *obj_id
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_cert_register(
	void *cert,
	uint32_t cert_size,
	uint32_t *obj_id)
{
	uint32_t ret = SEC_HAL_RES_OK;
	uint32_t msg_st = SEC_MSG_STATUS_OK;
	uint32_t disp_st = 0x00;
	uint32_t serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;
	void *kcert = NULL;

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
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		kcert = kmalloc(cert_size, GFP_KERNEL);
		if (kcert == NULL) {
			SEC_HAL_TRACE("kmalloc failed, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (copy_from_user(kcert, cert, cert_size)) {
			SEC_HAL_TRACE("copy failed, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				(uint32_t)virt_to_phys(kcert),
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("cert write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* call dispatcher */
		disp_st = LOCAL_DISP(SEC_SERV_CERTIFICATE_REGISTER,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		msg_st = sec_msg_param_read32(&out_handle, &serv_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != disp_st
				|| SEC_MSG_STATUS_OK != msg_st
				|| SEC_SERV_STATUS_OK != serv_st) {
			SEC_HAL_TRACE("failed! disp==%d, msg==%d, serv==%d",
				disp_st, msg_st, serv_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);

	kfree(kcert);
	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return ret;
}

/* **************************************************************************
** Function name      : sec_hal_rt_data_cert_register
** Description        : register given data certificate on TZ side,
**                      temporary data certificate.
** Parameters         : IN/--- void *cert
**                      IN/--- uint32_t cert_size
**                      IN/--- void *data
**                      IN/--- uint32_t data_size
**                      OUT/ --- uint32_t *id_ptr, ptr where to copy obj_id.
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_data_cert_register(
	void *cert,
	uint32_t cert_size,
	void *data,
	uint32_t data_size,
	uint32_t *id_ptr)
{
	uint32_t ret = SEC_HAL_RES_OK;
	uint32_t msg_st = SEC_MSG_STATUS_OK;
	uint32_t disp_st = 0x00;
	uint32_t serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;
	void *kcert = NULL;
	void *kdata = NULL;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*3;
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (NULL == in_msg || NULL == out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		kcert = kmalloc(cert_size, GFP_KERNEL);
		if (kcert == NULL) {
			SEC_HAL_TRACE("kmalloc failed, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (copy_from_user(kcert, cert, cert_size)) {
			SEC_HAL_TRACE("copy failed, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		kdata = kmalloc(data_size, GFP_KERNEL);
		if (kdata == NULL) {
			SEC_HAL_TRACE("kmalloc(2) failed, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (copy_from_user(kdata, data, data_size)) {
			SEC_HAL_TRACE("copy(2) failed, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				(uint32_t)virt_to_phys(kcert),
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("cert write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				(uint32_t)virt_to_phys(kdata),
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("data write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				data_size,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("dsize write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		disp_st = LOCAL_DISP(SEC_SERV_PROT_DATA_REGISTER,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		msg_st = sec_msg_param_read32(&out_handle, &serv_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != disp_st
				|| SEC_MSG_STATUS_OK != msg_st
				|| SEC_SERV_STATUS_OK != serv_st) {
			SEC_HAL_TRACE("failed! disp==%d, msg==%d, serv==%d",
				disp_st, msg_st, serv_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		if (id_ptr == NULL)
			break; /* can exit now, no more output wanted. */
		msg_st = sec_msg_param_read32(&out_handle, id_ptr);
		if (msg_st != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("read failed! msg == %d", msg_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB();

	kfree(kdata);
	kfree(kcert);
	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return ret;
}

/* **************************************************************************
** Function name      : sec_hal_rt_mac_address_get
** Description        : store requested MAC address on given userspace addr.
** Parameters         : IN/--- sec_hal_mac_address_t *mac_addr
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t
sec_hal_rt_mac_address_get(uint32_t user_index_in,
                           sec_hal_mac_address_t *user_mac_addr_out) {

    uint32_t sec_hal_status = SEC_HAL_RES_OK;
    uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
    uint32_t ssa_disp_status = 0x00;
    uint32_t sec_serv_status = 0x00;
    uint16_t msg_data_size;
    sec_msg_t *in_msg = NULL;
    sec_msg_t *out_msg = NULL;
    sec_msg_handle_t in_handle;
    sec_msg_handle_t out_handle;

    SEC_HAL_TRACE_ENTRY();

    if (NULL == user_mac_addr_out) {
        SEC_HAL_TRACE_EXIT_INFO("!!null user_mac_addr_out ptr, aborting!!");
        return SEC_HAL_RES_PARAM_ERROR;
    }

    /* allocate memory, from RAM, for msgs to be sent to TZ */
    msg_data_size = sec_msg_param_size(sizeof(uint32_t))*2;
    in_msg = sec_msg_alloc(&in_handle, msg_data_size,
            SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);
    msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
            sec_msg_param_size(SEC_HAL_MAC_SIZE);
    out_msg = sec_msg_alloc(&out_handle, msg_data_size,
            SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);

    if (NULL == in_msg || NULL == out_msg) {
        SEC_HAL_TRACE("alloc failure, msg not sent!");
        sec_hal_status = SEC_HAL_RES_FAIL;
    }
    else {
        /* write content to the input msg */
        sec_msg_param_write32(
                &in_handle,
                SEC_HAL_MAC_SIZE,
                SEC_MSG_PARAM_ID_NONE);
        sec_msg_param_write32(
                &in_handle,
                user_index_in,
                SEC_MSG_PARAM_ID_NONE);

        /* call dispatcher */
        ssa_disp_status = LOCAL_DISP(
                SEC_SERV_MAC_ADDRESS_REQUEST,
                LOCAL_DEFAULT_DISP_FLAGS,
                LOCAL_DEFAULT_DISP_SPARE_PARAM,
                SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
                SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

        /* interpret the response */
        sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
        if (SEC_ROM_RET_OK != ssa_disp_status ||
            SEC_MSG_STATUS_OK != sec_msg_status ||
            SEC_SERV_STATUS_OK != sec_serv_status) {
            SEC_HAL_TRACE("op failed, MAC not available!");
            SEC_HAL_TRACE_INT("ssa_disp_status", ssa_disp_status);
            SEC_HAL_TRACE_INT("sec_msg_status ", sec_msg_status);
            SEC_HAL_TRACE_INT("sec_serv_status", sec_serv_status);
            sec_hal_status = SEC_HAL_RES_FAIL;
        }
        else {
            sec_msg_status = _sec_msg_param_read(LOCAL_CPY_TO_CLIENT_FUNCPTR,
                        &out_handle,
                        user_mac_addr_out,
                        SEC_HAL_MAC_SIZE);
            if (SEC_MSG_STATUS_OK != sec_msg_status) {
                SEC_HAL_TRACE_INT("read failed! sec_msg_status", sec_msg_status);
                sec_hal_status = SEC_HAL_RES_FAIL;
            }
        }
    }

    /* de-allocate msg */
    sec_msg_free(out_msg);
    sec_msg_free(in_msg);

    SEC_HAL_TRACE_EXIT();
    return sec_hal_status;
}


/* **************************************************************************
** Function name      : sec_hal_rt_imei_get
** Description        : stores device's IMEI code to the given address.
** Parameters         : IN/--- sec_hal_imei_t *imei
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t
sec_hal_rt_imei_get(sec_hal_imei_t *user_imei_out){

    uint32_t sec_hal_status = SEC_HAL_RES_OK;
    uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
    uint32_t ssa_disp_status = 0x00;
    uint32_t sec_serv_status = 0x00;
    uint16_t msg_data_size;
    sec_msg_t *in_msg = NULL;
    sec_msg_t *out_msg = NULL;
    sec_msg_handle_t in_handle;
    sec_msg_handle_t out_handle;

    SEC_HAL_TRACE_ENTRY();

    if (NULL == user_imei_out) {
        SEC_HAL_TRACE_EXIT_INFO("!!null user_imei_out ptr, aborting!!");
        return SEC_HAL_RES_PARAM_ERROR;
    }

    /* allocate memory, from ICRAM, for msgs to be sent to TZ */
    msg_data_size = sec_msg_param_size(sizeof(uint32_t));
    in_msg = sec_msg_alloc(&in_handle, msg_data_size,
            SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);
    msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
            sec_msg_param_size(SEC_HAL_MAX_IMEI_SIZE);
    out_msg = sec_msg_alloc(&out_handle, msg_data_size,
            SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);

    if (NULL == in_msg || NULL == out_msg) {
        SEC_HAL_TRACE("alloc failure, msg not sent!");
        sec_hal_status = SEC_HAL_RES_FAIL;
    }
    else {
        /* write content to the input msg */
        sec_msg_param_write32(
                &in_handle,
                SEC_HAL_MAX_IMEI_SIZE,
                SEC_MSG_PARAM_ID_NONE);

        /* call dispatcher */
        ssa_disp_status = LOCAL_DISP(
                SEC_SERV_IMEI_REQUEST,
                LOCAL_DEFAULT_DISP_FLAGS,
                LOCAL_DEFAULT_DISP_SPARE_PARAM,
                SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
                SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

        /* interpret the response */
        sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
        if (SEC_ROM_RET_OK != ssa_disp_status ||
            SEC_MSG_STATUS_OK != sec_msg_status ||
            SEC_SERV_STATUS_OK != sec_serv_status) {
            SEC_HAL_TRACE("op failed, IMEI not available!");
            SEC_HAL_TRACE_INT("ssa_disp_status", ssa_disp_status);
            SEC_HAL_TRACE_INT("sec_msg_status ", sec_msg_status);
            SEC_HAL_TRACE_INT("sec_serv_status", sec_serv_status);
            sec_hal_status = SEC_HAL_RES_FAIL;
        }
        else {
            sec_msg_status = _sec_msg_param_read(
                    LOCAL_CPY_TO_CLIENT_FUNCPTR,
                    &out_handle,
                    user_imei_out,
                    SEC_HAL_MAX_IMEI_SIZE);
            if (SEC_MSG_STATUS_OK != sec_msg_status) {
                SEC_HAL_TRACE_INT("read failed! sec_msg_status", sec_msg_status);
                sec_hal_status = SEC_HAL_RES_FAIL;
            }
        }
    }

    /* de-allocate msg */
    sec_msg_free(out_msg);
    sec_msg_free(in_msg);

    SEC_HAL_TRACE_EXIT();
    return sec_hal_status;
}

/* **************************************************************************
** Function name      : sec_hal_rt_simlock_levels_open
** Description        : opens all simlock levels with given codes.
** Parameters         : IN/--- char levels_mask
**                    : IN/--- void *unlock_codes
**                    : OUT/--- uint32_t *post_lock_level_status
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_simlock_levels_open(
		uint32_t user_levels_mask_in,
		void* user_unlock_codes_in,
		uint32_t* user_post_lock_level_status_out)
{
	uint32_t sec_hal_status = SEC_HAL_RES_OK;
	uint32_t sec_msg_status = SEC_MSG_STATUS_OK;
	uint32_t ssa_disp_status = 0x00;
	uint32_t sec_serv_status = 0x00;
	uint16_t siml_levels;
	uint16_t msg_data_size;
	char* string_ptr = NULL;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;

	SEC_HAL_TRACE_ENTRY();

	if (0 == user_levels_mask_in)
	{
		SEC_HAL_TRACE_EXIT_INFO("!!no levels to-be-unlocked, aborting!!");
		return SEC_HAL_RES_PARAM_ERROR;
	}
	if (NULL == user_unlock_codes_in)
	{
		SEC_HAL_TRACE_EXIT_INFO("!!null user_unlock_codes_in ptr, aborting!!");
		return SEC_HAL_RES_PARAM_ERROR;
	}
	if (NULL == user_post_lock_level_status_out)
	{
		SEC_HAL_TRACE_EXIT_INFO("!!null user_post_lock_level_status_out ptr, aborting!!");
		return SEC_HAL_RES_PARAM_ERROR;
	}

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(SEC_HAL_MAX_SIMLOCK_CODE_LENGTH)*LOCAL_DEFAULT_SIMLOCK_LEVEL_COUNT;
	in_msg = sec_msg_alloc(
				&in_handle,
				msg_data_size,
				SEC_MSG_OBJECT_ID_NONE,
				0,
				SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
				sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(
				&out_handle,
				msg_data_size,
				SEC_MSG_OBJECT_ID_NONE,
				0,
				SEC_HAL_MSG_BYTE_ORDER);

	if (NULL == in_msg || NULL == out_msg)
	{
		SEC_HAL_TRACE("alloc failure, msg not sent!");
		sec_hal_status = SEC_HAL_RES_FAIL;
	}
	else
	{
		/* write content to the input msg */
		siml_levels = 0;
		msg_data_size = 0;
		string_ptr = user_unlock_codes_in;
		while (siml_levels < LOCAL_DEFAULT_SIMLOCK_LEVEL_COUNT &&
				SEC_MSG_STATUS_OK == sec_msg_status)
		{
			string_ptr = string_ptr + msg_data_size;
			msg_data_size = strlen(string_ptr) + 1;
			if (msg_data_size > SEC_HAL_MAX_SIMLOCK_CODE_LENGTH)
			{
				sec_msg_status = SEC_MSG_STATUS_PARAM_OUT_OF_RANGE;
			}
			else if ((user_levels_mask_in>>siml_levels)&0x1)
			{
				sec_msg_status = sec_msg_param_write(
									&in_handle,
									string_ptr,
									msg_data_size,
									SEC_MSG_PARAM_ID_NONE);
			}
			else
			{
				sec_msg_status = sec_msg_param_write(
									&in_handle,
									k_sec_hal_simlock_empty_code,
									strlen(k_sec_hal_simlock_empty_code)+1,
									SEC_MSG_PARAM_ID_NONE);
			}
			siml_levels++;
		}

		if (SEC_MSG_STATUS_OK == sec_msg_status)
		{
			/* call dispatcher */
			ssa_disp_status = LOCAL_DISP(
								SEC_SERV_SIMLOCK_CHECK_LOCKS,
								LOCAL_DEFAULT_DISP_FLAGS,
								LOCAL_DEFAULT_DISP_SPARE_PARAM,
								SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
								SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

			/* interpret the response */
			sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
		}

		if (SEC_MSG_STATUS_PARAM_OUT_OF_RANGE == sec_msg_status)
		{
			SEC_HAL_TRACE("param error, SIMLOCK post_status not given!");
			sec_hal_status = SEC_HAL_RES_PARAM_ERROR;
		}
		else if (SEC_ROM_RET_OK != ssa_disp_status ||
				SEC_MSG_STATUS_OK != sec_msg_status ||
				SEC_SERV_STATUS_OK != sec_serv_status)
		{
			SEC_HAL_TRACE("op failed, SIMLOCK post_status not given!");
			SEC_HAL_TRACE_INT("ssa_disp_status", ssa_disp_status);
			SEC_HAL_TRACE_INT("sec_msg_status ", sec_msg_status);
			SEC_HAL_TRACE_INT("sec_serv_status", sec_serv_status);
			sec_hal_status = SEC_HAL_RES_FAIL;
		}
		else
		{
			sec_msg_status = _sec_msg_param_read(
								LOCAL_CPY_TO_CLIENT_FUNCPTR,
								&out_handle,
								user_post_lock_level_status_out,
								sizeof(uint32_t));
			if (SEC_MSG_STATUS_OK != sec_msg_status)
			{
				SEC_HAL_TRACE_INT("read failed! sec_msg_status", sec_msg_status);
			sec_hal_status = SEC_HAL_RES_FAIL;
			}
		}
	}

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}

/* **************************************************************************
** Function name      : sec_hal_rt_simlock_level_open
** Description        : opens specific simlock level with the given code.
** Parameters         : IN/--- char *unlock_code
**                    : IN/--- uint8_t lock_level
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_simlock_level_open(
		char *user_unlock_code_in,
		uint8_t user_lock_level_in)
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

	SEC_HAL_TRACE_ENTRY();

	if (NULL == user_unlock_code_in)
	{
		SEC_HAL_TRACE_EXIT_INFO("!!no user_unlock_code_in, aborting!!");
		return SEC_HAL_RES_PARAM_ERROR;
	}
	if (0 == user_lock_level_in)
	{
		SEC_HAL_TRACE_EXIT_INFO("!!no user_lock_level_in, aborting!!");
		return SEC_HAL_RES_PARAM_ERROR;
	}

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint8_t)) +
				sec_msg_param_size(SEC_HAL_MAX_SIMLOCK_CODE_LENGTH);
	in_msg = sec_msg_alloc(
				&in_handle,
				msg_data_size,
				SEC_MSG_OBJECT_ID_NONE,
				0,
				SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t));
	out_msg = sec_msg_alloc(
				&out_handle,
				msg_data_size,
				SEC_MSG_OBJECT_ID_NONE,
				0,
				SEC_HAL_MSG_BYTE_ORDER);

	if (NULL == in_msg || NULL == out_msg)
	{
		SEC_HAL_TRACE("alloc failure, msg not sent!");
		sec_hal_status = SEC_HAL_RES_FAIL;
	}
	else
	{
		/* write content to the input msg */
		sec_msg_param_write8(
				&in_handle,
				user_lock_level_in,
				SEC_MSG_PARAM_ID_NONE);
		sec_msg_param_write(
				&in_handle,
				user_unlock_code_in,
				SEC_HAL_MAX_SIMLOCK_CODE_LENGTH,
				SEC_MSG_PARAM_ID_NONE);

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(
							SEC_SERV_SIMLOCK_CHECK_ONE_LOCK,
							LOCAL_DEFAULT_DISP_FLAGS,
							LOCAL_DEFAULT_DISP_SPARE_PARAM,
							SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
							SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
		if (SEC_ROM_RET_OK != ssa_disp_status ||
			SEC_MSG_STATUS_OK != sec_msg_status ||
			SEC_SERV_STATUS_OK != sec_serv_status)
		{
			SEC_HAL_TRACE("op failed, SIMLOCK open failed!");
			SEC_HAL_TRACE_INT("ssa_disp_status", ssa_disp_status);
			SEC_HAL_TRACE_INT("sec_msg_status ", sec_msg_status);
			SEC_HAL_TRACE_INT("sec_serv_status", sec_serv_status);
			sec_hal_status = SEC_HAL_RES_FAIL;
		}
	}

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}

/* **************************************************************************
** Function name      : sec_hal_rt_simlock_level_status_get
** Description        : retrieves device simlock levels state from TZ.
** Parameters         : OUT/--- uint32_t *lock_level_status
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_simlock_level_status_get(
		uint32_t *lvl_status,
		uint32_t *max_attempts,
		uint32_t *failed_attempts)
{
	uint32_t ret = SEC_HAL_RES_OK;
	uint32_t msg_st = SEC_MSG_STATUS_OK;
	uint32_t disp_st = 0x00;
	uint32_t serv_st = 0x00;
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
		+ sec_msg_param_size(sizeof(uint32_t))*3;
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (NULL == in_msg || NULL == out_msg) {
			SEC_HAL_TRACE("alloc failure, msg not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				sizeof(uint32_t),
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("dummy write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		disp_st = LOCAL_DISP(SEC_SERV_SIMLOCK_GET_STATE,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		msg_st = sec_msg_param_read32(&out_handle, &serv_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != disp_st
				|| SEC_MSG_STATUS_OK != msg_st
				|| SEC_SERV_STATUS_OK != serv_st) {
			SEC_HAL_TRACE("failed! disp==%d, msg==%d, serv==%d",
				disp_st, msg_st, serv_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		msg_st = sec_msg_param_read32(&out_handle, lvl_status);
		if (msg_st != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("lvl st read failed! msg == %d", msg_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (max_attempts == NULL && failed_attempts == NULL)
			break; /* can exit now, no more output wanted. */
		msg_st = sec_msg_param_read32(&out_handle, max_attempts);
		if (msg_st != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("max att read failed! msg == %d", msg_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (failed_attempts == NULL)
			break; /* can exit now, no more output wanted. */
		msg_st = sec_msg_param_read32(&out_handle, failed_attempts);
		if (msg_st != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("failed att read failed! msg == %d", msg_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB();

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return ret;
}

/* **************************************************************************
** Function name      : sec_hal_rt_auth_data_size_get
** Description        : retrieves device authentication data size from TZ.
** Parameters         : IN/--- uint32_t input_data_size
**                    : IN/--- uint32_t *input_data
**                    : OUT/--- uint32_t *auth_data_size
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_auth_data_size_get(
		void* user_input_data_in,
		uint32_t user_input_data_size_in,
		uint32_t* user_auth_data_size_out)
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

	SEC_HAL_TRACE_ENTRY();

	if (NULL == user_input_data_in)
	{
		SEC_HAL_TRACE_EXIT_INFO("!!null user_input_data_in, aborting!!");
		return SEC_HAL_RES_PARAM_ERROR;
	}
	if (0 == user_input_data_size_in)
	{
		SEC_HAL_TRACE_EXIT_INFO("!!zero user_input_data_size_in, aborting!!");
		return SEC_HAL_RES_PARAM_ERROR;
	}
	if (NULL == user_auth_data_size_out)
	{
		SEC_HAL_TRACE_EXIT_INFO("!!null user_auth_data_size_out, aborting!!");
		return SEC_HAL_RES_PARAM_ERROR;
	}

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t)) +
				sec_msg_param_size(user_input_data_size_in);
	in_msg = sec_msg_alloc(
				&in_handle,
				msg_data_size,
				SEC_MSG_OBJECT_ID_NONE,
				0,
				SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
				sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(
				&out_handle,
				msg_data_size,
				SEC_MSG_OBJECT_ID_NONE,
				0,
				SEC_HAL_MSG_BYTE_ORDER);

	if (NULL == in_msg || NULL == out_msg)
	{
		SEC_HAL_TRACE("alloc failure, msg not sent!");
		sec_hal_status = SEC_HAL_RES_FAIL;
	}
	else
	{
		/* write content to the input msg */
		sec_msg_param_write32(
				&in_handle,
				user_input_data_size_in,
				SEC_MSG_PARAM_ID_NONE);
		sec_msg_param_write(
				&in_handle,
				user_input_data_in,
				user_input_data_size_in,
				SEC_MSG_PARAM_ID_NONE);

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(
							SEC_SERV_DEVICE_AUTH_DATA_SIZE_REQUEST,
							LOCAL_DEFAULT_DISP_FLAGS,
							LOCAL_DEFAULT_DISP_SPARE_PARAM,
							SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
							SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
		if (SEC_ROM_RET_OK != ssa_disp_status ||
			SEC_MSG_STATUS_OK != sec_msg_status ||
			SEC_SERV_STATUS_OK != sec_serv_status)
		{
			SEC_HAL_TRACE("op failed, AUTH DATA SIZE not given!");
			SEC_HAL_TRACE_INT("ssa_disp_status", ssa_disp_status);
			SEC_HAL_TRACE_INT("sec_msg_status ", sec_msg_status);
			SEC_HAL_TRACE_INT("sec_serv_status", sec_serv_status);
			sec_hal_status = SEC_HAL_RES_FAIL;
		}
		else
		{
			sec_msg_status = _sec_msg_param_read(
								LOCAL_CPY_TO_CLIENT_FUNCPTR,
								&out_handle,
								user_auth_data_size_out,
								sizeof(uint32_t));
			if (SEC_MSG_STATUS_OK != sec_msg_status)
			{
				SEC_HAL_TRACE_INT("read failed! sec_msg_status", sec_msg_status);
				sec_hal_status = SEC_HAL_RES_FAIL;
			}
		}
	}

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}

/* **************************************************************************
** Function name      : sec_hal_rt_auth_data_get
** Description        : retrieves device authentication data from TZ side.
** Parameters         : IN/--- uint32_t input_data_size
**                    : IN/--- uint32_t *input_data
**                    : IN/--- uint32_t auth_data_size
**                    : OUT/--- uint32_t *auth_data
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_auth_data_get(
	void *user_input_data_in,
	uint32_t user_input_data_size_in,
	void *user_auth_data_out,
	uint32_t user_auth_data_size_in)
{
	uint32_t ret = SEC_HAL_RES_OK;
	uint32_t msg_st = SEC_MSG_STATUS_OK;
	uint32_t disp_st = 0x00;
	uint32_t serv_st = 0x00;
	uint16_t msg_data_size;
	sec_msg_t *in_msg = NULL;
	sec_msg_t *out_msg = NULL;
	sec_msg_handle_t in_handle;
	sec_msg_handle_t out_handle;
	void *kdata = NULL;

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*2
		+ sec_msg_param_size(user_input_data_size_in);
	in_msg = sec_msg_alloc(&in_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(user_auth_data_size_in);
	out_msg = sec_msg_alloc(&out_handle,
		msg_data_size, SEC_MSG_OBJECT_ID_NONE,
		0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (NULL == in_msg || NULL == out_msg) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		kdata = kmalloc(user_input_data_size_in, GFP_KERNEL);
		if (kdata == NULL) {
			SEC_HAL_TRACE("kmalloc failed, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (copy_from_user(kdata, user_input_data_in, user_input_data_size_in)) {
			SEC_HAL_TRACE("kmalloc failed, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(
				&in_handle,
				user_input_data_size_in,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("size write failed, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(
				&in_handle,
				kdata,
				user_input_data_size_in,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("data write failed, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(
				&in_handle,
				user_auth_data_size_in,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("authd size write failed, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		disp_st = LOCAL_DISP(SEC_SERV_DEVICE_AUTH_DATA_REQUEST,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		msg_st = sec_msg_param_read32(&out_handle, &serv_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != disp_st
				|| SEC_MSG_STATUS_OK != msg_st
				|| SEC_SERV_STATUS_OK != serv_st) {
			SEC_HAL_TRACE("failed! disp==%d, msg==%d, serv==%d",
				disp_st, msg_st, serv_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		msg_st = sec_msg_param_read(&out_handle,
			user_auth_data_out,
			user_auth_data_size_in);
		if (SEC_MSG_STATUS_OK != msg_st) {
			SEC_HAL_TRACE("read failed!msg == %d", msg_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);

	kfree(kdata);
	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return ret;
}

/* **************************************************************************
** Function name      : sec_hal_rt_periodic_integrity_check
** Description        : makes a periodic check of SW cert protected files.
** Parameters         : OUT/--- uint32_t *sec_exp_time
**                      timeout for next period
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_periodic_integrity_check(uint32_t *sec_exp_time)
{
	uint32_t ret = SEC_HAL_RES_OK;
	uint32_t msg_st = SEC_MSG_STATUS_OK;
	uint32_t disp_st = 0x00;
	uint32_t serv_st = 0x00;
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
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				sizeof(uint32_t),
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("dummy write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		disp_st = raw_pub2sec_dispatcher(SEC_SERV_INTEGRITY_CHECK,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		msg_st = sec_msg_param_read32(&out_handle, &serv_st);
		LOCAL_RMB();
		if (SEC_ROM_RET_OK != disp_st
				|| SEC_MSG_STATUS_OK != msg_st
				|| SEC_SERV_STATUS_OK != serv_st) {
			SEC_HAL_TRACE("warn! disp==%d, msg==%d, serv==%d",
				disp_st, msg_st, serv_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		if (sec_exp_time == NULL)
			break; /* can exit now, no more output wanted. */
		msg_st = sec_msg_param_read32(&out_handle, sec_exp_time);
		if (SEC_MSG_STATUS_OK != msg_st) {
			SEC_HAL_TRACE("time read failed! msg == %d", msg_st);
			ret = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB(); /* to ensure that out - param reads have completed */

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return ret;
}

/* **************************************************************************
** Function name      : sec_hal_rt_selftest
** Description        : check if selftest was a success, or not.
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_selftest(void)
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

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(
				&in_handle,
				msg_data_size,
				SEC_MSG_OBJECT_ID_NONE,
				0,
				SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
				sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(
				&out_handle,
				msg_data_size,
				SEC_MSG_OBJECT_ID_NONE,
				0,
				SEC_HAL_MSG_BYTE_ORDER);

	if (NULL == in_msg || NULL == out_msg)
	{
		SEC_HAL_TRACE("alloc failure, msgs not sent!");
		sec_hal_status = SEC_HAL_RES_FAIL;
	}
	else
	{
		/* write content to the input msg */
		sec_msg_param_write32(
				&in_handle,
				sizeof(uint32_t),
				SEC_MSG_PARAM_ID_NONE);

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_SELFTEST,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
		if (SEC_ROM_RET_OK != ssa_disp_status ||
			SEC_MSG_STATUS_OK != sec_msg_status ||
			SEC_SERV_STATUS_OK != sec_serv_status)
		{
			SEC_HAL_TRACE("SELFTEST failed!");
			SEC_HAL_TRACE_INT("ssa_disp_status", ssa_disp_status);
			SEC_HAL_TRACE_INT("sec_msg_status ", sec_msg_status);
			SEC_HAL_TRACE_INT("sec_serv_status", sec_serv_status);
			sec_hal_status = SEC_HAL_RES_FAIL;
		}
	}

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}


/* **************************************************************************
** Function name      : sec_hal_rt_public_cc42_key_init
** Description        : transmit public cc42 key init event to TZ.
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
uint32_t sec_hal_rt_public_cc42_key_init(void)
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

	SEC_HAL_TRACE_ENTRY();

	/* allocate memory, from ICRAM, for msgs to be sent to TZ */
	msg_data_size = sec_msg_param_size(sizeof(uint32_t));
	in_msg = sec_msg_alloc(
				&in_handle,
				msg_data_size,
				SEC_MSG_OBJECT_ID_NONE,
				0,
				SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t)) +
				sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(
				&out_handle,
				msg_data_size,
				SEC_MSG_OBJECT_ID_NONE,
				0,
				SEC_HAL_MSG_BYTE_ORDER);

	if (NULL == in_msg || NULL == out_msg)
	{
		SEC_HAL_TRACE("alloc failure, msgs not sent!");
		sec_hal_status = SEC_HAL_RES_FAIL;
	}
	else
	{
		/* write content to the input msg */
		sec_msg_param_write32(
				&in_handle,
				sizeof(uint32_t),
				SEC_MSG_PARAM_ID_NONE);

		/* call dispatcher */
		ssa_disp_status = LOCAL_DISP(SEC_SERV_PUBLIC_CC42_KEY_INIT,
			LOCAL_DEFAULT_DISP_FLAGS,
			LOCAL_DEFAULT_DISP_SPARE_PARAM,
			SEC_HAL_MEM_VIR2PHY_FUNC(out_msg),
			SEC_HAL_MEM_VIR2PHY_FUNC(in_msg));

		/* interpret the response */
		sec_msg_status = sec_msg_param_read32(&out_handle, &sec_serv_status);
		if (SEC_ROM_RET_OK != ssa_disp_status ||
			SEC_MSG_STATUS_OK != sec_msg_status ||
			SEC_SERV_STATUS_OK != sec_serv_status)
		{
			SEC_HAL_TRACE("CC42 KEY INIT failed!");
			SEC_HAL_TRACE_INT("ssa_disp_status", ssa_disp_status);
			SEC_HAL_TRACE_INT("sec_msg_status ", sec_msg_status);
			SEC_HAL_TRACE_INT("sec_serv_status", sec_serv_status);
			sec_hal_status = SEC_HAL_RES_FAIL;
		}
	}

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_status;
}


static long _sl_status_get(sd_ioctl_params_t *p)
{
	uint8_t max, fail;
	uint32_t rv, sl_lvl = 0x00, max_att = 0x00, fail_att = 0x00;

	rv = sec_hal_rt_simlock_level_status_get(&sl_lvl,
		p->param1 ? &max_att : NULL,
		p->param2 ? &fail_att : NULL);
	max = (max_att & 0xFF);
	fail = (fail_att & 0xFF);

	if (rv == SEC_HAL_RES_OK) {
		if (put_user(sl_lvl, (uint32_t __user *)p->param0))
			rv = SEC_HAL_RES_FAIL;
		if (rv == SEC_HAL_RES_OK && p->param1
			&& put_user(max,(uint8_t __user *)p->param1))
			rv = SEC_HAL_RES_FAIL;
		if (rv == SEC_HAL_RES_OK && p->param2
			&& put_user(fail, (uint8_t __user *)p->param2))
			rv = SEC_HAL_RES_FAIL;
	}

	return rv;
}


/* **************************************************************************
** Function name      : sec_hal_rt_ioctl
** Description        : entry from USR mode.
** Return value       : long
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
long sec_hal_rt_ioctl(unsigned int cmd, void **data, sd_ioctl_params_t *param)
{
	long rv = -EPERM;

	switch (cmd){
	case SD_KEY_INFO:
		rv = sec_hal_rt_key_info_get(
			(sec_hal_key_info_t *)param->param0);
		break;
	case SD_CERT_REGISTER:
		rv = sec_hal_rt_cert_register((void*)param->param0 /*IN*/,
			param->param1 /*IN*/,
			(uint32_t *)param->param2 /*OUT*/);
		break;
	case SD_DATA_CERT_REGISTER:
		rv = sec_hal_rt_data_cert_register(
			(void *)param->param0 /*IN*/,
			param->param1 /*IN*/,
			(void *)param->param2 /*IN*/,
			param->param3 /*IN*/,
			(uint32_t *)param->param4);
		break;
	case SD_MAC_ADDRESS_GET:
		rv = sec_hal_rt_mac_address_get(
			param->param0 /*IN*/,
			(sec_hal_mac_address_t *)param->param1 /*OUT*/);
		break;
	case SD_IMEI_GET:
		rv = sec_hal_rt_imei_get((sec_hal_imei_t *)param->param0 /*OUT*/);
		break;
	case SD_RAT_BAND_GET:
		SEC_HAL_TRACE("!!RAT_BAND - NOT IMPL.!!");
		break;
	case SD_PP_FLAGS_COUNT_GET:
		SEC_HAL_TRACE("!!PP_FLAGS_COUNT - NOT IMPL.!!");
		break;
	case SD_PP_FLAGS_GET:
		SEC_HAL_TRACE("!!PP_FLAGS - NOT IMPL.!!");
		break;
	case SD_SL_LEVELS_OPEN:
		rv = sec_hal_rt_simlock_levels_open(
			(uint32_t)param->param0 /*IN*/,
			(void *)param->param1 /*IN*/,
			(uint32_t *)param->param2 /*OUT*/);
		break;
	case SD_SL_LEVEL_OPEN:
		rv = sec_hal_rt_simlock_level_open((char *)param->param0 /*IN*/,
			(uint8_t)param->param2 /*IN*/);
		break;
	case SD_SL_LEVEL_STATUS_GET:
		rv = _sl_status_get(param);
		break;
	case SD_AUTH_DATA_SIZE_GET:
		rv = sec_hal_rt_auth_data_size_get((void *)param->param0 /*IN*/,
			param->param1 /*IN*/,
			(uint32_t *)param->param2 /*OUT*/);
		break;
	case SD_AUTH_DATA_GET:
		rv = sec_hal_rt_auth_data_get((void *)param->param0 /*IN*/,
			param->param1 /*IN*/,
			(void *)param->param2 /*OUT*/,
			param->param3 /*IN*/);
		break;
	case SD_SELFTEST:
		rv = sec_hal_rt_selftest();
		break;
	default:
		rv = -EPERM;
		break;
	}

	return rv;
}

/* ******************************** END ********************************** */

