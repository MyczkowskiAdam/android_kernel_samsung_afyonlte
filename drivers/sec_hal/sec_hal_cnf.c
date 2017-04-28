/*
 * drivers/sec_hal/sec_hal_cnf.c
 *
 * Copyright (c) 2013, Renesas Mobile Corporation.
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
/* #define SEC_HAL_TRACE_LOCAL_DISABLE */
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
#define LOCAL_MAX_CNF_DATA             PAGE_SIZE


/* **************************************************************************
 * Function name      : sec_hal_cnf_siml
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_cnf_siml(uint32_t cnfd_paddr,
	uint32_t cnfd_size,
	siml_unlock_codes_t *ul_codes,
	uint32_t emask,
	uint32_t max_attempts,
	cnf_code_t *master_code,
	cnf_code_t *reprog_code)
{
	int i = 0;
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
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*4
		+ sec_msg_param_size(CNF_CODE_MAX_LEN)*7;
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				cnfd_size,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("cnfd_size write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				cnfd_paddr,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("cnfd_paddr write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				emask,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("emask write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		for (i = 0; i < SIML_LVL_CNT && ret == SEC_MSG_STATUS_OK; i++) {
			msg_st = sec_msg_param_write(&in_handle,
				ul_codes->codes[i].code, CNF_CODE_MAX_LEN,
				SEC_MSG_PARAM_ID_NONE);
		}
		if (msg_st != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("ul_codes write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				max_attempts,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("max att write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(&in_handle,
				master_code->code,
				CNF_CODE_MAX_LEN,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("master_code write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(&in_handle,
				reprog_code->code,
				CNF_CODE_MAX_LEN,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("reprog_code write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		disp_st = LOCAL_DISP(SEC_SERV_SIMLOCK_CONFIGURE,
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

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return ret;
}


/* **************************************************************************
 * Function name      : sec_hal_cnf_siml_validate
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_cnf_siml_validate(uint32_t cnfd_paddr,
	uint32_t cnfd_size)
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
	msg_data_size = sec_msg_param_size(sizeof(uint32_t))*2;
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				cnfd_size,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("cnfd_size write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				cnfd_paddr,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("cnfd_paddr write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		disp_st = LOCAL_DISP(SEC_SERV_SIMLOCK_CONFIGURATION_REGISTER,
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

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return ret;
}


/* **************************************************************************
 * Function name      : sec_hal_cnf_siml_verify_data_get
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_cnf_siml_verify_data_get(uint32_t req_hash_len,
	uint8_t hash[SIML_LVL_CNT+1][CNF_HASH_MAX_LEN])
{
	uint32_t i, len = 0;
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
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t))*6
		+ sec_msg_param_size(req_hash_len)*6;
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(&in_handle,
				req_hash_len,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("req_hash_len write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		disp_st = LOCAL_DISP(SEC_SERV_SIMLOCK_VERIFICATION_DATA_READ,
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
		for (i = 0, len = req_hash_len; i < SIML_LVL_CNT+1
			&& msg_st == SEC_MSG_STATUS_OK
			&& serv_st == SEC_MSG_STATUS_OK
			&& (!len || req_hash_len == len); i++) {
			msg_st = sec_msg_param_read32(&out_handle, &len);
			serv_st = sec_msg_param_read(&out_handle,
				hash[i], req_hash_len);
		}
		if (SEC_MSG_STATUS_OK != msg_st
			|| SEC_MSG_STATUS_OK != serv_st
			|| (len && req_hash_len != len)) {
			SEC_HAL_TRACE("failed! msg==%d, serv==%d, len==%d",
				msg_st, serv_st, len);
			ret = SEC_HAL_RES_FAIL;
			break;
		}
	} while (0);
	LOCAL_RMB(); /* ensure that read ops completed */

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return ret;
}


/* **************************************************************************
 * Function name      : sec_hal_cnf_imei
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_cnf_imei(imei_t *imei, cnf_code_t *reprog_code)
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
	msg_data_size = sec_msg_param_size(IMEI_MAX_LEN)
		+ sec_msg_param_size(CNF_CODE_MAX_LEN);
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write(&in_handle,
				imei->imei,
				IMEI_MAX_LEN,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("imei write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(&in_handle,
				reprog_code->code,
				CNF_CODE_MAX_LEN,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("reprog_code write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		disp_st = LOCAL_DISP(SEC_SERV_IMEI_WRITE,
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

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return ret;
}


/* **************************************************************************
 * Function name      : sec_hal_cnf_mac
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_cnf_mac(mac_t *mac,
	uint32_t idx,
	cnf_code_t *reprog_code)
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
	msg_data_size = sec_msg_param_size(MAC_MAX_LEN)
		+ sec_msg_param_size(sizeof(uint32_t))
		+ sec_msg_param_size(CNF_CODE_MAX_LEN);
	in_msg = sec_msg_alloc(&in_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write(&in_handle,
				mac->mac,
				MAC_MAX_LEN,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("mac write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write32(&in_handle,
				idx,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("idx write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		if (sec_msg_param_write(&in_handle,
				reprog_code->code,
				CNF_CODE_MAX_LEN,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("reprog_code write error, aborting!");
			ret = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		disp_st = LOCAL_DISP(SEC_SERV_MAC_WRITE,
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

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return ret;
}


/* **************************************************************************
 * Function name      :
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t _write_siml(sd_ioctl_params_t *p, struct platform_device *pdev)
{
	int i;
	uint32_t rv, sz, emask = 0x00, paddr = 0x00;
	siml_unlock_codes_t cds;
	cnf_code_t mcode, rcode;
	void *cnfd = NULL;

	sz = p->param1;
	if (sz > LOCAL_MAX_CNF_DATA)
		return -E2BIG;

	cnfd = kmalloc(sz, GFP_KERNEL);
	if (cnfd == NULL)
		return -ENOMEM;

	if (copy_from_user(cnfd, (const void __user *)p->param0, sz)) {
		rv = -EIO;
		goto out;
	}

	paddr = dma_map_single(&pdev->dev, cnfd, sz, DMA_TO_DEVICE);
	if (!paddr) {
		rv = -EIO;
		goto out;
	}

	if (copy_from_user(&mcode, (const void __user *)p->param2,
			CNF_CODE_MAX_LEN)) {
		rv = -EIO;
		goto dmaout;
	}

	if (copy_from_user(&cds, (const void __user *)p->param3,
			SIML_UNLOCK_CODES_SZ)) {
		rv = -EIO;
		goto dmaout;
	}

	if (copy_from_user(&rcode, (const void __user *)p->param5,
			CNF_CODE_MAX_LEN)) {
		rv = -EIO;
		goto dmaout;
	}

	for (i = 0; i < SIML_LVL_CNT; i++)
		emask |= (cds.codes[i].code[0] ? 0x01 : 0x00) << i;

	rv = sec_hal_cnf_siml(paddr, sz, &cds, emask, p->param4,
		&mcode, &rcode);
dmaout:
	dma_unmap_single(&pdev->dev, paddr, sz, DMA_TO_DEVICE);
out:
	kfree(cnfd);
	return rv;
}


/* **************************************************************************
 * Function name      :
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t _validate_siml(
	sd_ioctl_params_t *p,
	struct platform_device *pdev)
{
	uint32_t rv, sz, paddr = 0x00;
	void *cnfd = NULL;

	sz = p->param1;
	if (sz > LOCAL_MAX_CNF_DATA)
		return -E2BIG;

	cnfd = kmalloc(sz, GFP_KERNEL);
	if (cnfd == NULL)
		return -ENOMEM;

	if (copy_from_user(cnfd, (const void __user *)p->param0, sz)) {
		rv = -EIO;
		goto out;
	}

	paddr = dma_map_single(&pdev->dev, cnfd, sz, DMA_TO_DEVICE);
	if (!paddr) {
		rv = -EIO;
		goto out;
	}

	rv = sec_hal_cnf_siml_validate(paddr, sz);
	dma_unmap_single(&pdev->dev, paddr, sz, DMA_TO_DEVICE);
out:
	kfree(cnfd);
	return rv;
}


/* **************************************************************************
 * Function name      :
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t _siml_verify_data_get(sd_ioctl_params_t *p)
{
	uint32_t rv, len;
	uint8_t hashes[SIML_LVL_CNT+1][CNF_HASH_MAX_LEN];

	len = p->param0;
	if (len > CNF_HASH_MAX_LEN)
		return -E2BIG;

	rv = sec_hal_cnf_siml_verify_data_get(len, hashes);

	if (SEC_HAL_RES_OK == rv && p->param2
		&& copy_to_user((void *)p->param2, hashes[0], len))
		rv = SEC_HAL_RES_FAIL;
	if (SEC_HAL_RES_OK == rv && p->param3
		&& copy_to_user((void *)p->param3, hashes[1], len))
		rv = SEC_HAL_RES_FAIL;
	if (SEC_HAL_RES_OK == rv && p->param4
		&& copy_to_user((void *)p->param4, hashes[2], len))
		rv = SEC_HAL_RES_FAIL;
	if (SEC_HAL_RES_OK == rv && p->param5
		&& copy_to_user((void *)p->param5, hashes[3], len))
		rv = SEC_HAL_RES_FAIL;
	if (SEC_HAL_RES_OK == rv && p->reserved1
		&& copy_to_user((void *)p->reserved1, hashes[4], len))
		rv = SEC_HAL_RES_FAIL;
	if (SEC_HAL_RES_OK == rv && p->param1 /* master_code_data */
		&& copy_to_user((void *)p->param1, hashes[5], len))
		rv = SEC_HAL_RES_FAIL;

	return rv;
}


/* **************************************************************************
 * Function name      :
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t _write_imei(sd_ioctl_params_t *p)
{
	imei_t imei = IMEI_ZI;
	cnf_code_t rcode = CNF_CODE_ZI;

	if (copy_from_user(&imei, (const void __user *)p->param0, IMEI_SZ))
		return -EINVAL;

	if (copy_from_user(&rcode, (const void __user *)p->param1, CNF_CODE_SZ))
		return -EINVAL;

	return sec_hal_cnf_imei(&imei, &rcode);
}


/* **************************************************************************
 * Function name      :
 * Description        :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t _write_mac(sd_ioctl_params_t *p)
{
	mac_t mac = MAC_ZI;
	cnf_code_t rcode = CNF_CODE_ZI;

	if (copy_from_user(&mac, (const void __user *)p->param0, MAC_SZ))
		return -EINVAL;

	if (copy_from_user(&rcode, (const void __user *)p->param2, CNF_CODE_SZ))
		return -EINVAL;

	return sec_hal_cnf_mac(&mac, p->param1, &rcode);
}




/* **************************************************************************
 * Function name      : sec_hal_cnf_ioctl
 * Description        : entry from USR mode.
 * Return value       : long
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
long sec_hal_cnf_ioctl(unsigned int cmd,
	void **data,
	sd_ioctl_params_t *param,
	struct platform_device *pdev)
{
	long rv;

	switch (cmd) {
	case SD_CNF_SIML:
		rv = _write_siml(param, pdev);
		break;
	case SD_CNF_SIML_VALIDATE:
		rv = _validate_siml(param, pdev);
		break;
	case SD_CNF_SIML_VERIFY_DATA_GET:
		rv = _siml_verify_data_get(param);
		break;
	case SD_CNF_IMEI:
		rv = _write_imei(param);
		break;
	case SD_CNF_MAC:
		rv = _write_mac(param);
		break;
	default:
		rv = -EPERM;
	}

	return rv;
}



