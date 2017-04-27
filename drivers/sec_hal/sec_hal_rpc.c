/*
 * drivers/sec_hal/sec_hal_rpc.c
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

#include "sec_hal_cmn.h"
#include "sec_msg.h"
#include "sec_serv_api.h"
#include "sec_dispatch.h"
#include "sec_hal_rt.h"
#include "sec_hal_rt_cmn.h"
#include "sec_hal_rt_trace.h"
#include "sec_hal_dev_ioctl.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/system.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/sched.h>

#define LOCAL_DISP                        pub2sec_dispatcher
#define LOCAL_MSG_BYTE_ORDER              SEC_MSG_BYTE_ORDER_LE
#define LOCAL_DEFAULT_DISP_FLAGS          0
#define LOCAL_DEFAULT_DISP_SPARE_PARAM    0
#define LOCAL_WMB()                       wmb()
#define LOCAL_RMB()                       rmb()
#define RPC_SUCCESS                       0x00
#define RPC_FAILURE                       0x01


typedef struct {
	struct list_head list;
	sd_rpc_params_t param;
} rpc_queue_item_t;

typedef struct {
	struct list_head list_head;
	wait_queue_head_t waitq;
} rpc_queue_t;


static rpc_queue_t g_rpc_read_waitq;
static rpc_queue_t g_rpc_write_waitq;
static uint32_t g_secure_storage_pid;


static int rpcq_init(rpc_queue_t* q)
{
	INIT_LIST_HEAD(&q->list_head);
	init_waitqueue_head(&q->waitq);
	return 0;
}

static int _rpcq_add(struct list_head* lst, sd_rpc_params_t *param_in)
{
	rpc_queue_item_t* new = kmalloc(sizeof(rpc_queue_item_t), GFP_KERNEL);
	if (!new)
		return -ENOMEM;

	new->param = *param_in;

	list_add(&new->list, lst);
	return 0;
}

static int rpcq_add_wakeup(rpc_queue_t *q, sd_rpc_params_t *param_in)
{
	_rpcq_add(&q->list_head, param_in);
	wake_up_interruptible(&q->waitq);
	return 0;
}

static int _rpcq_get(struct list_head *lst, sd_rpc_params_t *param_out)
{
	unsigned int tgid = (unsigned int)current->tgid;
	struct list_head *pos;
	rpc_queue_item_t *item;

	list_for_each(pos, lst) {
		item = list_entry(pos, rpc_queue_item_t, list);
		if (item->param.reserved2 == tgid) {
			*param_out = item->param;
			list_del(&item->list);
			kfree(item);
			return 1;
		}
	}

	return 0;
}

static int rpcq_get_wait(rpc_queue_t *q, sd_rpc_params_t *item)
{
	return wait_event_interruptible(q->waitq, _rpcq_get(&q->list_head, item));
}


void* sec_hal_mem_msg_area_calloc(unsigned int n, unsigned int sz);
void  sec_hal_mem_msg_area_free(void *virt_addr);


/* **************************************************************************
 * Function name      : sec_hal_rpc_ins_hdr
 * Description        : install rpc function to TZ.
 *                      Notice the use of 'static' keyword, usage limited
 *                      only to this file. If overwrite supported by TZ then
 *                      remove 'static'.
 * Parameters         :
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
static uint32_t sec_hal_rpc_ins_hdr(sec_hal_rt_rpc_handler fptr)
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
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);
	msg_data_size = sec_msg_param_size(sizeof(sec_serv_status_t))
		+ sec_msg_param_size(sizeof(uint32_t));
	out_msg = sec_msg_alloc(&out_handle, msg_data_size,
		SEC_MSG_OBJECT_ID_NONE, 0, SEC_HAL_MSG_BYTE_ORDER);

	do {
		if (in_msg == NULL || out_msg == NULL) {
			SEC_HAL_TRACE("alloc failure, msgs not sent!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}

		/* write content to the input msg */
		if (sec_msg_param_write32(
				&in_handle,
				(uint32_t)fptr,
				SEC_MSG_PARAM_ID_NONE) != SEC_MSG_STATUS_OK) {
			SEC_HAL_TRACE("func ptr write error, aborting!");
			sec_hal_st = SEC_HAL_RES_FAIL;
			break;
		}
		LOCAL_WMB();

		/* call dispatcher */
		ssa_disp_st = LOCAL_DISP(SEC_SERV_RPC_ADDRESS,
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
	} while (0);

	/* de-allocate msgs */
	sec_msg_free(out_msg);
	sec_msg_free(in_msg);

	SEC_HAL_TRACE_EXIT();
	return sec_hal_st;
}


/* **************************************************************************
** Function name      : sec_hal_rpc_cb
** Description        : RPC handler function.
**                      This should be installed via previous function.
** Parameters         :
** Return value       : uint32
**                      ==0 operation successful
**                      failure otherwise.
** *************************************************************************/
static uint32_t sec_hal_rpc_cb(
	uint32_t id,
	uint32_t p1,
	uint32_t p2,
	uint32_t p3,
	uint32_t p4)
{
	uint32_t ret = 0x00, msg_st[3] = {0};
	sec_msg_handle_t in_handle, out_handle;
	uint64_t offset, filesize;
	sd_rpc_params_t params = {
		id, p1, p2, p3, p4, 0,
		(uint32_t)(current->tgid), (uint32_t)(current->tgid)
	};

	switch (id) { /* pre-ipc step for callbacks */
	case SEC_HAL_RPC_ALLOC:
		ret = (uint32_t)SEC_HAL_MEM_VIR2PHY_FUNC(sec_hal_mem_msg_area_calloc(1, p1));
		return ret;
	case SEC_HAL_RPC_FREE:
		sec_hal_mem_msg_area_free((void *)SEC_HAL_MEM_PHY2VIR_FUNC(p1));
		return RPC_SUCCESS;
	case SEC_HAL_RPC_PROT_DATA_ALLOC:
	case SEC_HAL_RPC_PROT_DATA_FREE:
		return RPC_FAILURE;
	case SEC_HAL_RPC_TRACE:
		SEC_HAL_TRACE_SECMSG((void*)p1, ret);
		return ret;
	case SEC_HAL_RPC_FS_LOOKUP:
		/* Secure Storage RPC's -- p1=*out_msg(to Secure), p2=*in_msg(from Secure), p3=0 */
		sec_msg_open(&in_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p2)));
		SEC_HAL_TRACE("SECHAL_RPC_FS_LOOKUP(1): p1=0x%08x, p2=0x%08x, in_handle=0x%08x",p1,p2,in_handle);
		/* Read in_msg from ICRAM (dir handle, namelen and filename) */
		/* Pass parameters (namelen and filename) to Sec Server */
		msg_st[0] = sec_msg_param_read32(&in_handle, &(params.param1)); /* dummy read: directory handle */
		msg_st[1] = sec_msg_param_read32(&in_handle, &(params.param1)); /* filename length */
		/* Filename length must be valid */
		if ((msg_st[1] != SEC_MSG_STATUS_OK) || params.param1 > SEC_STORAGE_FILENAME_MAXLEN) {
			sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
			sec_msg_param_write32(&out_handle, SFS_STATUS_INVALID_NAME, SEC_MSG_PARAM_ID_NONE); /* status */
			sec_msg_param_write32(&out_handle, 0, SEC_MSG_PARAM_ID_NONE); /* null handle */
			sec_msg_close(&out_handle);
			sec_msg_close(&in_handle);
			SEC_HAL_TRACE("Too long filename! (%d)", params.param1);
			return RPC_SUCCESS; /* Too long filename, return without calling Sec Server */
		}
		/* Read filename only if it was not too long */
		sec_msg_param_read(&in_handle, (void *)(params.fname), (uint16_t)params.param1); /* filename */
		SEC_HAL_TRACE("SECHAL_RPC_FS_LOOKUP(1): fnamelen: %d, fname: %s",params.param1, params.fname);
		sec_msg_close(&in_handle);
		params.reserved2 = g_secure_storage_pid; /* this is used to access sec storage at correct thread's context*/
		break;
	case SEC_HAL_RPC_FS_READ:
		sec_msg_open(&in_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p2)));
		SEC_HAL_TRACE("SECHAL_RPC_FS_READ(1): p1=0x%08x, p2=0x%08x, in_handle=0x%08x",p1,p2,in_handle);
		/* Read in_msg from ICRAM (file handle, offset(64b) and length) */
		/* Pass parameters (file handle and length) to Sec Server */
		msg_st[0] = sec_msg_param_read32(&in_handle, &params.param1); /* file handle */
		msg_st[1] = sec_msg_param_read64(&in_handle, &offset); /* offset */
		params.param2 = offset & 0xFFFFFFFF; /* low */
		params.param3 = (offset & 0xFFFFFFFF00000000ULL) >> 32; /* high */
		msg_st[2] = sec_msg_param_read32(&in_handle, &params.param4); /* length to-be-read */
		SEC_HAL_TRACE("SECHAL_RPC_FS_READ(1): handle=%d, offset=%llu, file_len=%d", params.param1, offset, params.param4);
		sec_msg_close(&in_handle);
		/* Filesize must be valid */
		if ((msg_st[0] != SEC_MSG_STATUS_OK) || (msg_st[1] != SEC_MSG_STATUS_OK)
			|| (msg_st[2] != SEC_MSG_STATUS_OK) || params.param4 > SEC_STORAGE_FILE_MAXLEN) {
			sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
			sec_msg_param_write32(&out_handle, SFS_STATUS_INVALID_CALL, SEC_MSG_PARAM_ID_NONE); /* status */
			sec_msg_param_write32(&out_handle, 0, SEC_MSG_PARAM_ID_NONE); /* length read 0 */
			sec_msg_close(&out_handle);
			SEC_HAL_TRACE("Illegal filesize!");
			return RPC_SUCCESS; /* Illegal filesize, return without calling Sec Server */
		}
		params.reserved2 = g_secure_storage_pid; /* this is used to access sec storage at correct thread's context*/
		break;
	case SEC_HAL_RPC_FS_WRITE:
		sec_msg_open(&in_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p2)));
		SEC_HAL_TRACE("SECHAL_RPC_FS_WRITE(1): p1=0x%08x, p2=0x%08x, in_handle=0x%08x",p1,p2,in_handle);
		/* Read in_msg from ICRAM (file handle, offset(64b), length and file contents) */
		/* Pass parameters (file handle, length and file contents) to Sec Server */
		msg_st[0] = sec_msg_param_read32(&in_handle, &params.param1); /* file handle */
		msg_st[1] = sec_msg_param_read64(&in_handle, &offset); /* offset */
		params.param2 = offset & 0xFFFFFFFF; /* low */
		params.param3 = (offset & 0xFFFFFFFF00000000ULL) >> 32; /* high */
		msg_st[2] = sec_msg_param_read32(&in_handle, &params.param4); /* length to-be-written */
		SEC_HAL_TRACE("SECHAL_RPC_FS_WRITE(1): handle=%d, offset=%llu, file_len=%d", params.param1, offset, params.param4);
		/* Filesize must be valid */
		if ((msg_st[0] != SEC_MSG_STATUS_OK) || (msg_st[1] != SEC_MSG_STATUS_OK)
			|| (msg_st[2] != SEC_MSG_STATUS_OK) || params.param4 > SEC_STORAGE_FILE_MAXLEN) {
			sec_msg_close(&in_handle);
			sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
			sec_msg_param_write32(&out_handle, SFS_STATUS_INVALID_CALL, SEC_MSG_PARAM_ID_NONE); /* status */
			sec_msg_close(&out_handle);
			SEC_HAL_TRACE("Illegal filesize!");
			return RPC_SUCCESS; /* Illegal filesize, return without calling Sec Server */
		}
		sec_msg_param_read(&in_handle, (void *)(params.data), (uint16_t)params.param4);
		sec_msg_close(&in_handle);
		params.reserved2 = g_secure_storage_pid; /* this is used to access sec storage at correct thread's context*/
		break;
	case SEC_HAL_RPC_FS_CREATE:
		sec_msg_open(&in_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p2)));
		SEC_HAL_TRACE("SECHAL_RPC_FS_CREATE(1): p1=0x%08x, p2=0x%08x, in_handle=0x%08x",p1,p2,in_handle);
		/* Read in_msg from ICRAM (dir handle, namelen, "filename") */
		/* Pass parameters (namelen and filename) to Sec Server     */
		msg_st[0] = sec_msg_param_read32(&in_handle, &(params.param1)); /* dummy read: directory handle */
		msg_st[1] = sec_msg_param_read32(&in_handle, &(params.param1)); /* filename length */
		/* Filename length must be valid */
		if ((msg_st[1] != SEC_MSG_STATUS_OK) || params.param1 > SEC_STORAGE_FILENAME_MAXLEN) {
			sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
			sec_msg_param_write32(&out_handle, SFS_STATUS_INVALID_NAME, SEC_MSG_PARAM_ID_NONE); /* status */
			sec_msg_close(&out_handle);
			sec_msg_close(&in_handle);
			SEC_HAL_TRACE("Too long filename! (%d)", params.param1);
			return RPC_SUCCESS; /* Too long filename, return without calling Sec Server */
		}
		/* Read filename only if it was not too long */
		sec_msg_param_read(&in_handle, (void *)(params.fname), (uint16_t)params.param1); /* filename */
		SEC_HAL_TRACE("SECHAL_RPC_FS_CREATE(1): fnamelen: %d, fname: %s",params.param1, params.fname);
		sec_msg_close(&in_handle);
		params.reserved2 = g_secure_storage_pid; /* this is used to access sec storage at correct thread's context*/
		break;
	case SEC_HAL_RPC_FS_SIZE:
		sec_msg_open(&in_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p2)));
		SEC_HAL_TRACE("SECHAL_RPC_FS_SIZE(1): p1=0x%08x, p2=0x%08x, in_handle=0x%08x",p1,p2,in_handle);
		/* Read in_msg from ICRAM (file handle) */
		/* Pass parameter (file handle) to Sec Server */
		msg_st[0] = sec_msg_param_read32(&in_handle, &(params.param1)); /* file handle */
		SEC_HAL_TRACE("SECHAL_RPC_FS_SIZE(1): f-handle=%d",params.param1);
		if (msg_st[0] != SEC_MSG_STATUS_OK) {
			sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
			sec_msg_param_write32(&out_handle, SFS_STATUS_FAIL, SEC_MSG_PARAM_ID_NONE); /* status */
			sec_msg_close(&out_handle);
			sec_msg_close(&in_handle);
			SEC_HAL_TRACE("failed to read, aborting!");
			return RPC_SUCCESS;
		}
		sec_msg_close(&in_handle);
		params.reserved2 = g_secure_storage_pid; /* this is used to access sec storage at correct thread's context*/
		break;
	case SEC_HAL_RPC_FS_ROOT:
		SEC_HAL_TRACE("SECHAL_RPC_FS_ROOT(x): p1=0x%08x",p1);
		sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
		sec_msg_param_write32(&out_handle, SFS_STATUS_NOT_SUPPORTED, SEC_MSG_PARAM_ID_NONE); /* status */
		sec_msg_param_write32(&out_handle, 0, SEC_MSG_PARAM_ID_NONE); /* null handle */
		sec_msg_close(&out_handle);
		SEC_HAL_TRACE("SECHAL_RPC_FS_ROOT(x): STATUS_NOT_SUPPORTED=0x%08x, handle=0x%08x",SFS_STATUS_NOT_SUPPORTED,NULL);
		return RPC_SUCCESS;
	case SEC_HAL_RPC_FS_MOVE:
	case SEC_HAL_RPC_FS_REMOVE:
		SEC_HAL_TRACE("SECHAL_RPC_FS_(RE)MOVE(x): p1=0x%08x, p2=0x%08x",p1,p2);
		sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
		sec_msg_param_write32(&out_handle, SFS_STATUS_NOT_SUPPORTED, SEC_MSG_PARAM_ID_NONE); /* status */
		sec_msg_close(&out_handle);
		SEC_HAL_TRACE("SECHAL_RPC_FS_(RE)MOVE(x): STATUS_NOT_SUPPORTED=0x%08x",SFS_STATUS_NOT_SUPPORTED);
		return RPC_SUCCESS;
	default:
		break;
	}

	rpcq_add_wakeup(&g_rpc_read_waitq, &params);
	if (rpcq_get_wait(&g_rpc_write_waitq, &params))
		return RPC_FAILURE;

	switch (id) {/* post-ipc step for params conversion and etc. */
	case SEC_HAL_RPC_FS_LOOKUP:
		sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
		SEC_HAL_TRACE("SECHAL_RPC_FS_LOOKUP(2): p1=0x%08x",p1);
		/* Write out_msg to ICRAM: status, file handle */
		sec_msg_param_write32(&out_handle, params.reserved1, SEC_MSG_PARAM_ID_NONE); /* status */
		sec_msg_param_write32(&out_handle, params.param4, SEC_MSG_PARAM_ID_NONE);    /* file handle */
		sec_msg_close(&out_handle);
		SEC_HAL_TRACE("SECHAL_RPC_FS_LOOKUP(2): status=%d, handle=%d", params.reserved1,params.param4);
		break;
	case SEC_HAL_RPC_FS_READ:
		SEC_HAL_TRACE("SECHAL_RPC_FS_READ(2): p1=0x%08x",p1);
		/* Write out_msg to ICRAM: status, length read, file */
		sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
		sec_msg_param_write32(&out_handle, params.reserved1, SEC_MSG_PARAM_ID_NONE); /* status */
		sec_msg_param_write32(&out_handle, params.param5, SEC_MSG_PARAM_ID_NONE); /* length read */
		/* Copy data only if length read > 0 */
		if (params.param5) {
			/* Write file to ICRAM from params.data */
			sec_msg_param_write(&out_handle, (void *)(params.data), params.param5, SEC_MSG_PARAM_ID_NONE);
			sec_msg_close(&out_handle);
		}
		SEC_HAL_TRACE("SECHAL_RPC_FS_READ(2): status=%d, l_read=%d",params.reserved1,params.param4);
		break;
	case SEC_HAL_RPC_FS_WRITE:
		SEC_HAL_TRACE("SECHAL_RPC_FS_WRITE(2): p1=0x%08x",p1);
		/* Write out_msg to ICRAM: status */
		sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
		sec_msg_param_write32(&out_handle, params.reserved1, SEC_MSG_PARAM_ID_NONE); /* status */
		sec_msg_close(&out_handle);
		SEC_HAL_TRACE("SECHAL_RPC_FS_WRITE(2): status=%d",params.reserved1);
		break;
	case SEC_HAL_RPC_FS_CREATE:
		SEC_HAL_TRACE("SECHAL_RPC_FS_CREATE(2): p1=0x%08x",p1);
		/* Write out_msg to ICRAM: status */
		sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
		sec_msg_param_write32(&out_handle, params.reserved1, SEC_MSG_PARAM_ID_NONE); /* status */
		sec_msg_close(&out_handle);
		SEC_HAL_TRACE("SECHAL_RPC_FS_CREATE(2): status=%d",params.reserved1);
		break;
	case SEC_HAL_RPC_FS_SIZE:
		SEC_HAL_TRACE("SECHAL_RPC_FS_SIZE(2): p1=0x%08x",p1);
		/* Write out_msg to ICRAM: status, size(64b) */
		filesize = params.param4;
		filesize <<= 32;
		filesize |= params.param5;
		sec_msg_open(&out_handle, (sec_msg_t *)(SEC_HAL_MEM_PHY2VIR_FUNC(p1)));
		sec_msg_param_write32(&out_handle, params.reserved1, SEC_MSG_PARAM_ID_NONE); /* status */
		sec_msg_param_write64(&out_handle, filesize, SEC_MSG_PARAM_ID_NONE);         /* size */
		sec_msg_close(&out_handle);
		SEC_HAL_TRACE("SECHAL_RPC_FS_SIZE(2): status=%d, size=%d",params.reserved1,filesize);
		break;
	default:
		break;
	}

	return RPC_SUCCESS;
}


/* **************************************************************************
 * Function name      : sec_hal_rpc_read
 * Description        : will read the content of rpc queque, if anything
 *                      available for for this particular thread. Blocks
 *                      until something is available, or interrupted.
 * Return value       : number of bytes left unread.
 * *************************************************************************/
size_t sec_hal_rpc_read(
	struct file *filp,
	char __user* buf,
	size_t count,
	loff_t *ppos)
{
	int cnt;
	sd_rpc_params_t param;

	SEC_HAL_TRACE_ENTRY();

	if (filp->f_flags & O_NONBLOCK)
		return -EPERM;

	if (rpcq_get_wait(&g_rpc_read_waitq, &param))
		return -ERESTARTSYS;

	cnt = copy_to_user(buf, &param, SD_RPC_PARAMS_SZ);

	SEC_HAL_TRACE_EXIT();
	return (SD_RPC_PARAMS_SZ-cnt);
}


/* **************************************************************************
 * Function name      : sec_hal_rpc_write
 * Description        : will receive the content of rpc action queque.
 * Return value       : number of bytes not received.
 * *************************************************************************/
size_t sec_hal_rpc_write(
	struct file *filp,
	const char *buf,
	size_t count,
	loff_t *ppos)
{
	int cnt;
	sd_rpc_params_t param;

	SEC_HAL_TRACE_ENTRY();

	if (filp->f_flags & O_NONBLOCK)
		return -EPERM;

	cnt = copy_from_user(&param, buf, SD_RPC_PARAMS_SZ);
	rpcq_add_wakeup(&g_rpc_write_waitq, &param);

	SEC_HAL_TRACE_EXIT();
	return (SD_RPC_PARAMS_SZ-cnt);
}


/* **************************************************************************
 * Function name      : sec_hal_rpc_init
 * Description        : initializes the rpc by installing callback to TZ,
 *                      and does the data queque init.
 * Return value       :
 * *************************************************************************/
int sec_hal_rpc_init(void)
{
	int rv = 0;
	u32 ret;

	SEC_HAL_TRACE_ENTRY();

	ret = sec_hal_rpc_ins_hdr(&sec_hal_rpc_cb);
	if (SEC_HAL_RES_OK == ret) {
		rpcq_init(&g_rpc_read_waitq);
		rpcq_init(&g_rpc_write_waitq);
	} else
		rv = -EPERM;

	SEC_HAL_TRACE_EXIT();
	return rv;
}


/* **************************************************************************
 * Function name      : sec_hal_rpc_ioctl
 * Description        : entry from USR mode.
 * Return value       : long
 *                      ==0 operation successful
 *                      failure otherwise.
 * *************************************************************************/
long sec_hal_rpc_ioctl(unsigned int cmd, void **data, sd_ioctl_params_t *param)
{
	long rv;

	switch (cmd) {
	case SD_SECURE_STORAGE_DAEMON_PID_REGISTER:
		g_secure_storage_pid = current->tgid;
		rv = SEC_HAL_RES_OK;
		break;
	default:
		rv = -EPERM;
	}

	return rv;
}


/* ****************************************************************************
 * Function name      : sec_trace_msg_to_string
 * Description        : convert sec_msg_t to a char buffer
 * Parameters         : IN/--- const void *msg, ptr to a sec_msg_t
 *                      OUT/--- byte buffer
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * ***************************************************************************/
uint32_t sec_trace_msg_to_string(const void *msg, char *out_str)
{
	char format[128] = {0};
	char param_str[128] = {0};

	uint32_t len = 0;
	uint16_t param_len = 0;

	uint32_t word = 0;
	uint8_t byte;
	char *string = 0;
	char char_store = 0;

	char *format_start = 0;
	uint32_t format_len = 0;

	char *fp = 0;
	char *outp = 0;
	bool state_format = false;

	sec_msg_status_t status = SEC_MSG_STATUS_OK;
	sec_msg_handle_t handle;

	if ((status = sec_msg_open(&handle, (sec_msg_t*)msg)) != SEC_MSG_STATUS_OK)
		return (uint32_t)status;
	if ((status = sec_msg_param_size_get(&handle, &param_len)) != SEC_MSG_STATUS_OK)
		return (uint32_t)status;
	if ((status = sec_msg_param_read(&handle, format, param_len)) != SEC_MSG_STATUS_OK)
		return (uint32_t)status;

	format_start = &format[0];
	format_len = 0;
	for (fp = &format[0], outp = &out_str[0]; *fp != 0; fp++) {
		if ('%' == *fp) {
			*param_str = 0;
			state_format = true;
			format_start = fp;
			format_len = 1;
			continue;
		} else if (state_format) {
			switch (*fp) {
			case '*':
			case 'd':
			case 'i':
			case 'o':
			case 'u':
			case 'x':
			case 'X':
			case 'n':
			case 'p':
				if ((status != sec_msg_param_read32(&handle, &word)) != SEC_MSG_STATUS_OK)
					return (uint32_t)status;
				format_len++;
				char_store = format_start[format_len];
				format_start[format_len] = 0;
				sprintf(param_str, format_start, word);
				format_start[format_len] = char_store;
				state_format = false;
				break;
			case 'c':
				if ((status != sec_msg_param_read8(&handle, &byte)) != SEC_MSG_STATUS_OK)
					return (uint32_t)status;
				format_len++;
				char_store = format_start[format_len];
				format_start[format_len] = 0;
				sprintf(param_str, format_start, byte);
				format_start[format_len] = char_store;
				state_format = false;
				break;
			case 's':
				if ((status = sec_msg_param_ptr_read(&handle, (void **)&string,
							&param_len)) != SEC_MSG_STATUS_OK)
					return (uint32_t)status;
				format_len++;
				char_store = format_start[format_len];
				format_start[format_len] = 0;
				sprintf(param_str, format_start, SEC_HAL_MEM_PHY2VIR_FUNC(string));
				format_start[format_len] = char_store;
				state_format = false;
				break;
			case '%':
				*param_str = 0;
				format_len = 0;
				state_format = false;
				*outp = *fp;
				outp++;
				break;
			default:
				format_len++;
				break;
			}

			if (*param_str) {
				len = strlen(param_str);
				strcpy(outp, param_str);
				outp += len;
				*param_str = 0;
				state_format = false;
			}
		} else {
			*outp = *fp;
			outp++;
		}
	}

	return 1;
}


