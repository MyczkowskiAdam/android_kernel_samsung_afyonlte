/*
 * system_memory.c
 *	 System memory manager device driver API function file.
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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

#include "iccom_drv.h"
#include "system_memory.h"
#include "system_memory_private.h"
#include "rtds_memory_drv_common.h"
#include "rtds_memory_drv.h"
#include "log_kernel.h"

/*******************************************************************************
 * Function   : system_memory_rt_alloc
 * Description: This function allocates RT shared memory.
 * Parameters : rt_alloc					-   Memory information.
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				SMAP_LIB_MEMORY_NO_MEMORY	-   No memory
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_rt_alloc(
		system_mem_rt_alloc *rt_alloc
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	unsigned int rtaddr = 0;
	system_mem_rt_change_apaddr rt_change_apaddr;
	iccom_drv_send_cmd_param iccom_send_cmd;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == rt_alloc) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_alloc = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == rt_alloc->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_alloc->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (0 == rt_alloc->alloc_size) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_alloc->alloc_size = 0\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Message parameter settings. */
	iccom_send_cmd.handle		= ((mem_rtapi_info_handle *)rt_alloc->handle)->iccom_handle;
	iccom_send_cmd.task_id		= TASK_MEMORY;
	iccom_send_cmd.function_id  = EVENT_MEMORY_GLOBALALLOCATERT;
	iccom_send_cmd.send_mode	= ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size	= sizeof(rt_alloc->alloc_size);
	iccom_send_cmd.send_data	= (unsigned char *)&rt_alloc->alloc_size;
	iccom_send_cmd.recv_size	= sizeof(rtaddr);
	iccom_send_cmd.recv_data	= (unsigned char *)&rtaddr;

	/* The message is sent. */
	ret_code = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != ret_code) {
		if (ICCOM_DRV_SYSTEM_ERR < ret_code) {
			ret_code = SMAP_LIB_MEMORY_NO_MEMORY;
		}
		MSG_ERROR("[RTAPIK]ERR|[%s] iccom_drv_send_command() failed[%d].\n", __func__, ret_code);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Translation parameter settings. */
	rt_change_apaddr.handle	   = rt_alloc->handle;
	rt_change_apaddr.rtmem_rtaddr = rtaddr;
	rt_change_apaddr.rtmem_apaddr = 0;

	/* The address of the RTDomain side translate into the address of the AppDomain side. */
	ret_code = system_memory_rt_change_apaddr(&rt_change_apaddr);
	if (SMAP_LIB_MEMORY_OK != ret_code) {
		/* This route is assumption that doesn't pass. */
		MSG_ERROR("[RTAPIK]ERR|[%s] system_memory_rt_change_apaddr() failed.\n", __func__);
	} else {
		rt_alloc->rtmem_apaddr = rt_change_apaddr.rtmem_apaddr;
		MSG_MED("[RTAPIK]INF|[%s] rt_alloc->rtmem_apaddr = 0x%08x\n", __func__, rt_alloc->rtmem_apaddr);
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_rt_alloc);

/*******************************************************************************
 * Function   : system_memory_rt_free
 * Description: This function frees RT shared memory.
 * Parameters : rt_free						-   Memory information.
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_rt_free(
		system_mem_rt_free *rt_free
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	system_mem_rt_change_rtaddr rt_change_rtaddr;
	iccom_drv_send_cmd_param iccom_send_cmd;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == rt_free) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_free = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == rt_free->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_free->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Translation parameter settings. */
	rt_change_rtaddr.handle	   = rt_free->handle;
	rt_change_rtaddr.rtmem_apaddr = rt_free->rtmem_apaddr;
	rt_change_rtaddr.rtmem_rtaddr = 0;

	/* The address of the AppDomain side translate into the address of the RTDomain side. */
	ret_code = system_memory_rt_change_rtaddr(&rt_change_rtaddr);
	if (SMAP_LIB_MEMORY_OK != ret_code) {
		MSG_ERROR("[RTAPIK]ERR|[%s] system_memory_rt_change_rtaddr() failed.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Message parameter settings. */
	iccom_send_cmd.handle		= ((mem_rtapi_info_handle *)rt_free->handle)->iccom_handle;
	iccom_send_cmd.task_id		= TASK_MEMORY;
	iccom_send_cmd.function_id  = EVENT_MEMORY_GLOBALFREERT;
	iccom_send_cmd.send_mode	= ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size	= sizeof(rt_change_rtaddr.rtmem_rtaddr);
	iccom_send_cmd.send_data	= (unsigned char *)&rt_change_rtaddr.rtmem_rtaddr;
	iccom_send_cmd.recv_size	= 0;
	iccom_send_cmd.recv_data	= NULL;

	/* The message is sent. */
	ret_code = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != ret_code) {
		if (ICCOM_DRV_SYSTEM_ERR < ret_code) {
			ret_code = SMAP_LIB_MEMORY_PARA_NG;
		}
		MSG_ERROR("[RTAPIK]ERR|[%s] iccom_drv_send_command() failed[%d].\n", __func__, ret_code);
	} else {
		ret_code = SMAP_LIB_MEMORY_OK;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_rt_free);

/*******************************************************************************
 * Function   : system_memory_rt_change_rtaddr
 * Description: This function translates the address of the AppDomain into the address of the RTDomain side.
 * Parameters : rt_change_rtaddr		-   Logical addresses
 * Returns	  : SMAP_LIB_MEMORY_OK		-   Success
 *				SMAP_LIB_MEMORY_PARA_NG -   Parameter error
 *******************************************************************************/
int system_memory_rt_change_rtaddr(
		system_mem_rt_change_rtaddr *rt_change_rtaddr
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_drv_change_addr_param rtds_change_addr;
	void	*handle = NULL;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == rt_change_rtaddr) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_change_rtaddr = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == rt_change_rtaddr->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_change_rtaddr->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (0 == rt_change_rtaddr->rtmem_apaddr) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_change_rtaddr->rtmem_apaddr = 0\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	handle = ((mem_rtapi_info_handle *)rt_change_rtaddr->handle)->rtds_mem_handle;
	if (NULL == handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rtds_memory_handle = NULL\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Translation parameter settings. */
	rtds_change_addr.handle		= handle;
	rtds_change_addr.addr_type  = RTDS_MEM_DRV_APP_MEM;
	rtds_change_addr.org_addr   = rt_change_rtaddr->rtmem_apaddr;
	rtds_change_addr.chg_addr   = 0;

	/* Logical address is translated. */
	ret_code =  rtds_memory_drv_change_address(&rtds_change_addr);
	if (SMAP_OK != ret_code) {
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		MSG_ERROR("[RTAPIK]ERR|[%s] rtds_memory_drv_change_address() failed.\n", __func__);
	} else {
		rt_change_rtaddr->rtmem_rtaddr = rtds_change_addr.chg_addr;
		ret_code = SMAP_LIB_MEMORY_OK;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_rt_change_rtaddr);

/*******************************************************************************
 * Function   : system_memory_rt_change_apaddr
 * Description: This function translates the address of the RTDomain into the address of the AppDomain side.
 * Parameters : rt_change_apaddr		-   Logical addresses
 * Returns	  : SMAP_LIB_MEMORY_OK		-   Success
 *				SMAP_LIB_MEMORY_PARA_NG -   Parameter error
 *******************************************************************************/
int system_memory_rt_change_apaddr(
		system_mem_rt_change_apaddr *rt_change_apaddr
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_drv_change_addr_param rtds_change_addr;
	void	*handle = NULL;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == rt_change_apaddr) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_change_apaddr = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == rt_change_apaddr->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_change_apaddr->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (0 == rt_change_apaddr->rtmem_rtaddr) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rt_change_apaddr->rtmem_rtaddr = 0\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	handle = ((mem_rtapi_info_handle *)rt_change_apaddr->handle)->rtds_mem_handle;
	if (NULL == handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rtds_memory_handle = NULL\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Translation parameter settings. */
	rtds_change_addr.handle		= handle;
	rtds_change_addr.addr_type  = RTDS_MEM_DRV_RT_MEM_NC;
	rtds_change_addr.org_addr   = rt_change_apaddr->rtmem_rtaddr;
	rtds_change_addr.chg_addr   = 0;

	/* Logical address is translated. */
	ret_code =  rtds_memory_drv_change_address(&rtds_change_addr);
	if (SMAP_OK != ret_code) {
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		MSG_ERROR("[RTAPIK]ERR|[%s] rtds_memory_drv_change_address() failed.\n", __func__);
	} else {
		rt_change_apaddr->rtmem_apaddr = rtds_change_addr.chg_addr;
		ret_code = SMAP_LIB_MEMORY_OK;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_rt_change_apaddr);

/*******************************************************************************
 * Function   : system_memory_ap_open
 * Description: This function creates App shared memory.
 * Parameters : ap_open						-   Memory information.
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				SMAP_LIB_MEMORY_NO_MEMORY	-   No memory
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_ap_open(
		system_mem_ap_open   *ap_open
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_drv_open_mem_param rtds_memory_open_mem;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == ap_open) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_open = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_open->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_open->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if ((0 == ap_open->aparea_size) ||
		(0 != (ap_open->aparea_size & (RT_MEMORY_PAGESIZE-1)))) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_open->aparea_size(%d) is invalid parameter.\n", __func__, ap_open->aparea_size);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	switch (ap_open->cache_kind) {
	case RT_MEMORY_WRITEBACK:
	case RT_MEMORY_WRITETHROUGH:
	case RT_MEMORY_NONCACHE:
	case RT_MEMORY_BUFFER_NONCACHE:
		break;
	default:
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_open->cache_kind(%d) is invalid parameter.\n", __func__, ap_open->cache_kind);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	memset(&rtds_memory_open_mem, 0, sizeof(rtds_memory_open_mem));
	rtds_memory_open_mem.mem_size	 = ap_open->aparea_size;
	rtds_memory_open_mem.app_cache	= ap_open->cache_kind;

	/* Creating App shared memory. */
	ret_code = rtds_memory_drv_open_apmem(&rtds_memory_open_mem);
	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	case SMAP_MEMORY:
		ret_code = SMAP_LIB_MEMORY_NO_MEMORY;
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		break;
	}

	if (SMAP_LIB_MEMORY_OK != ret_code) {
		MSG_ERROR("[RTAPIK]ERR|[%s] rtds_memory_drv_open_app_mem() failed.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	ap_open->apaddr		= rtds_memory_open_mem.app_addr;
	ap_open->pages		= rtds_memory_open_mem.pages;

	MSG_MED("[RTAPIK]INF|[%s] ap_open->apaddr = 0x%08x\n", __func__, ap_open->apaddr);
	MSG_MED("[RTAPIK]INF|[%s] ap_open->pages = 0x%08x\n", __func__, (unsigned int)ap_open->pages);
	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_open);

/*******************************************************************************
 * Function   : system_memory_ap_close
 * Description: This function destroys App shared memory.
 * Parameters : ap_close					-   Memory information.
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_ap_close(
		system_mem_ap_close  *ap_close
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_drv_close_mem_param rtds_memory_close_mem;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == ap_close) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_close = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_close->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_close->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_close->pages) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_close->pages = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (RT_MEMORY_APP_ADDR_START > ap_close->apaddr) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_close->apaddr(0x%08x) is invalid parameter.\n", __func__, ap_close->apaddr);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	rtds_memory_close_mem.app_addr = ap_close->apaddr;
	rtds_memory_close_mem.pages = ap_close->pages;

	/* Destroying App shared memory. */
	ret_code = rtds_memory_drv_close_apmem(&rtds_memory_close_mem);
	if (SMAP_OK != ret_code) {
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s] rtds_memory_drv_close_app_mem() failed.\n", __func__);
	} else {
		ret_code = SMAP_LIB_MEMORY_OK;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_close);

/*******************************************************************************
 * Function   : system_memory_ap_alloc
 * Description: This function allocates App shared memory.
 * Parameters : rt_alloc					-   Memory information.
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				SMAP_LIB_MEMORY_NO_MEMORY	-   No memory
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_ap_alloc(
		system_mem_ap_alloc  *ap_alloc
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	iccom_drv_send_cmd_param iccom_send_cmd;
	system_mem_ap_change_apaddr ap_change_apaddr;
	unsigned int rtaddr = 0;

	struct {
		unsigned int apmem_id;
		unsigned int obj_id;
		unsigned int alloc_size;
	} send_data;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == ap_alloc) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_alloc = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}
	if (NULL == ap_alloc->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_alloc->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_alloc->apmem_handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_alloc->apmem_handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (0 == ap_alloc->alloc_size) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_alloc->alloc_size = 0.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Message parameter settings. */
	send_data.apmem_id   = ((rtds_memory_shared_info *)ap_alloc->apmem_handle)->apmem_id;
	send_data.obj_id	 = 0;
	send_data.alloc_size = ap_alloc->alloc_size;

	iccom_send_cmd.handle		= ((mem_rtapi_info_handle *)ap_alloc->handle)->iccom_handle;
	iccom_send_cmd.task_id		= TASK_MEMORY_SUB;
	iccom_send_cmd.function_id  = EVENT_MEMORYSUB_GLOBALALLOCATEAPP;
	iccom_send_cmd.send_mode    = ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size    = sizeof(send_data);
	iccom_send_cmd.send_data    = (unsigned char *)&send_data;
	iccom_send_cmd.recv_size    = sizeof(rtaddr);
	iccom_send_cmd.recv_data    = (unsigned char *)&rtaddr;

	/* The message is sent. */
	ret_code = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != ret_code) {
		if (SMAP_NG == ret_code) {
			ret_code = SMAP_LIB_MEMORY_NO_MEMORY;
		} else if (SMAP_PARA_NG == ret_code) {
			ret_code = SMAP_LIB_MEMORY_PARA_NG;
		}
		MSG_ERROR("[RTAPIK]ERR|[%s] iccom_drv_send_command() failed[%d].\n", __func__, ret_code);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Translation parameter settings. */
	ap_change_apaddr.handle	   = ap_alloc->handle;
	ap_change_apaddr.apmem_handle = ap_alloc->apmem_handle;
	ap_change_apaddr.apmem_rtaddr = rtaddr;
	ap_change_apaddr.apmem_apaddr = 0;

	/* The address of the RTDomain side translate into the address of the AppDomain side. */
	ret_code = system_memory_ap_change_apaddr(&ap_change_apaddr);
	if (SMAP_LIB_MEMORY_OK != ret_code) {
		/* This route is assumption that doesn't pass. */
		MSG_ERROR("[RTAPIK]ERR|[%s] system_memory_ap_change_apaddr() failed.\n", __func__);
	} else {
		ap_alloc->apmem_apaddr = ap_change_apaddr.apmem_apaddr;
		MSG_MED("[RTAPIK]INF|[%s] ap_alloc->apmem_apaddr = 0x%08x\n", __func__, ap_alloc->apmem_apaddr);
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_alloc);

/*******************************************************************************
 * Function   : system_memory_ap_free
 * Description: This function frees App shared memory.
 * Parameters : rt_free						-   Memory information.
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_ap_free(
		system_mem_ap_free *ap_free
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_shared_info *apmem_handle;
	system_mem_ap_cache_flush ap_cache_flush;
	system_mem_ap_change_rtaddr ap_change_rtaddr;
	iccom_drv_send_cmd_param iccom_send_cmd;

	struct {
		unsigned int apmem_id;
		unsigned int rtaddr;
	} send_data;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == ap_free) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_free = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_free->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_free->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_free->apmem_handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_free->apmem_handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}
	apmem_handle = (rtds_memory_shared_info *)ap_free->apmem_handle;

	if (0 == ap_free->apmem_apaddr) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_free->apmem_apaddr = 0.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Parameter settings for flush. */
	ap_cache_flush.handle	    = ap_free->handle;
	ap_cache_flush.apmem_handle = ap_free->apmem_handle;
	ap_cache_flush.apmem_apaddr = ap_free->apmem_apaddr;
	ap_cache_flush.flush_size   = apmem_handle->mem_size
					- (ap_free->apmem_apaddr - apmem_handle->app_addr);

	/* Flushing the cache of App shared memory. */
	(void)system_memory_ap_cache_flush(&ap_cache_flush);

	/* Translation parameter settings. */
	ap_change_rtaddr.handle       = ap_free->handle;
	ap_change_rtaddr.cache_kind   = RT_MEMORY_WRITEBACK;
	ap_change_rtaddr.apmem_handle = ap_free->apmem_handle;
	ap_change_rtaddr.apmem_apaddr = ap_free->apmem_apaddr;
	ap_change_rtaddr.apmem_rtaddr = 0;

	/* The address of the AppDomain side translate into the address of the RTDomain side. */
	ret_code = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
	if (SMAP_LIB_MEMORY_OK != ret_code) {
		MSG_ERROR("[RTAPIK]ERR|[%s] system_memory_ap_change_rtaddr() failed.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Message parameter settings. */
	send_data.apmem_id = apmem_handle->apmem_id;
	send_data.rtaddr   = ap_change_rtaddr.apmem_rtaddr;

	iccom_send_cmd.handle      = ((mem_rtapi_info_handle *)ap_free->handle)->iccom_handle;
	iccom_send_cmd.task_id     = TASK_MEMORY_SUB;
	iccom_send_cmd.function_id = EVENT_MEMORYSUB_GLOBALFREEAPP;
	iccom_send_cmd.send_mode   = ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size   = sizeof(send_data);
	iccom_send_cmd.send_data   = (unsigned char *)&send_data;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = NULL;

	/* The message is sent. */
	ret_code = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != ret_code) {
		if (ICCOM_DRV_SYSTEM_ERR < ret_code) {
			ret_code = SMAP_LIB_MEMORY_PARA_NG;
		}
		MSG_ERROR("[RTAPIK]ERR|[%s] iccom_drv_send_command() failed[%d].\n", __func__, ret_code);
	} else {
		ret_code = SMAP_LIB_MEMORY_OK;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_free);

/*******************************************************************************
 * Function   : system_memory_ap_change_rtaddr
 * Description: This function translates the address of the AppDomain into the address of the RTDomain side.
 * Parameters : ap_change_rtaddr		-   Logical addresses
 * Returns	  : SMAP_LIB_MEMORY_OK		-   Success
 *				SMAP_LIB_MEMORY_PARA_NG -   Parameter error
 *******************************************************************************/
int system_memory_ap_change_rtaddr(
		system_mem_ap_change_rtaddr *ap_change_rtaddr
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_shared_info *apmem_handle;

	MSG_HIGH("[RTAPIK]IN|[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == ap_change_rtaddr) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_change_rtaddr = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_change_rtaddr->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_change_rtaddr->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_change_rtaddr->apmem_handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_change_rtaddr->apmem_handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}
	apmem_handle = (rtds_memory_shared_info *)ap_change_rtaddr->apmem_handle;

	if ((ap_change_rtaddr->apmem_apaddr < apmem_handle->app_addr) ||
		(ap_change_rtaddr->apmem_apaddr >= (apmem_handle->app_addr + apmem_handle->mem_size))) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_change_rtaddr->apmem_apaddr(0x%08x) is invalid parameter.\n", __func__, ap_change_rtaddr->apmem_apaddr);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* The address of the AppDomain side translate into the address of the RTDomain side. */
	switch (ap_change_rtaddr->cache_kind) {
	case RT_MEMORY_WRITEBACK:
		ap_change_rtaddr->apmem_rtaddr = apmem_handle->rt_addr;
		ap_change_rtaddr->apmem_rtaddr += (ap_change_rtaddr->apmem_apaddr - apmem_handle->app_addr);
		break;
	case RT_MEMORY_NONCACHE:
		if (RT_MEMORY_RTMAP_WBNC != apmem_handle->rt_cache) {
			MSG_ERROR("[RTAPIK]ERR|[%s] App shared area is not mapped by non-cache.\n", __func__);
			MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
			return ret_code;
		}
		ap_change_rtaddr->apmem_rtaddr = apmem_handle->rt_addr_nc;
		ap_change_rtaddr->apmem_rtaddr += (ap_change_rtaddr->apmem_apaddr - apmem_handle->app_addr);
		break;
	default:
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_change_rtaddr->cache_kind(%d) is invalid parameter.\n", __func__, ap_change_rtaddr->cache_kind);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	MSG_MED("[RTAPIK]INF|[%s] ap_change_rtaddr->cache_kind = %d\n", __func__, ap_change_rtaddr->cache_kind);
	MSG_MED("[RTAPIK]INF|[%s] ap_change_rtaddr->apmem_apaddr = 0x%08x\n", __func__, ap_change_rtaddr->apmem_apaddr);
	MSG_MED("[RTAPIK]INF|[%s] ap_change_rtaddr->apmem_rtaddr = 0x%08x\n", __func__, ap_change_rtaddr->apmem_rtaddr);
	ret_code = SMAP_LIB_MEMORY_OK;

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_change_rtaddr);

/*******************************************************************************
 * Function   : system_memory_ap_change_apaddr
 * Description: This function translates the address of the RTDomain into the address of the AppDomain side.
 * Parameters : ap_change_apaddr		-   Logical addresses
 * Returns	  : SMAP_LIB_MEMORY_OK		-   Success
 *				SMAP_LIB_MEMORY_PARA_NG -   Parameter error
 *******************************************************************************/
int system_memory_ap_change_apaddr(
		system_mem_ap_change_apaddr *ap_change_apaddr
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_shared_info *apmem_handle;
	unsigned long rtaddr = 0;

	MSG_HIGH("[RTAPIK]IN|[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == ap_change_apaddr) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_change_apaddr = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_change_apaddr->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_change_apaddr->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_change_apaddr->apmem_handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_change_apaddr->apmem_handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}
	apmem_handle = (rtds_memory_shared_info *)ap_change_apaddr->apmem_handle;

	if ((0 != apmem_handle->rt_addr) &&
		(ap_change_apaddr->apmem_rtaddr >= apmem_handle->rt_addr) &&
		(ap_change_apaddr->apmem_rtaddr < (apmem_handle->rt_addr + apmem_handle->mem_size))) {
		rtaddr = apmem_handle->rt_addr;
		MSG_MED("[RTAPIK]INF|[%s] cache kind = WRITEBACK.\n", __func__);
	} else if ((0 != apmem_handle->rt_addr_nc) &&
		(ap_change_apaddr->apmem_rtaddr >= apmem_handle->rt_addr_nc) &&
		(ap_change_apaddr->apmem_rtaddr < (apmem_handle->rt_addr_nc + apmem_handle->mem_size))) {
		rtaddr = apmem_handle->rt_addr_nc;
		MSG_MED("[RTAPIK]INF|[%s] cache_kind = NONCACHE.\n", __func__);
	} else {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* The address of the RTDomain side translate into the address of the AppDomain side. */
	ap_change_apaddr->apmem_apaddr = apmem_handle->app_addr;
	ap_change_apaddr->apmem_apaddr += (ap_change_apaddr->apmem_rtaddr - rtaddr);

	MSG_MED("[RTAPIK]INF|[%s] ap_change_apaddr->apmem_rtaddr = 0x%08x\n", __func__, ap_change_apaddr->apmem_rtaddr);
	MSG_MED("[RTAPIK]INF|[%s] ap_change_apaddr->apmem_apaddr = 0x%08x\n", __func__, ap_change_apaddr->apmem_apaddr);
	ret_code = SMAP_LIB_MEMORY_OK;

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_change_apaddr);

/*******************************************************************************
 * Function   : system_memory_ap_cache_flush
 * Description: This function flushes the cache of App shared memory.
 * Parameters : ap_cache_flush				-   Flush information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_ap_cache_flush(
		system_mem_ap_cache_flush *ap_cache_flush
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_shared_info *apmem_handle;

	MSG_HIGH("[RTAPIK]IN|[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == ap_cache_flush) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_cache_flush = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_cache_flush->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_cache_flush->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_cache_flush->apmem_handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_cache_flush->apmem_handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}
	apmem_handle = (rtds_memory_shared_info *)ap_cache_flush->apmem_handle;

	if ((ap_cache_flush->apmem_apaddr < apmem_handle->app_addr) ||
		(ap_cache_flush->apmem_apaddr >= (apmem_handle->app_addr + apmem_handle->mem_size))) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_cache_flush->apmem_apaddr(0x%08X) is invalid parameter.\n", __func__, ap_cache_flush->apmem_apaddr);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if ((ap_cache_flush->flush_size > apmem_handle->mem_size) ||
		((ap_cache_flush->apmem_apaddr + ap_cache_flush->flush_size) > (apmem_handle->app_addr + apmem_handle->mem_size))) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_cache_flush->flush_size(%d) is invalid parameter.\n", __func__, ap_cache_flush->flush_size);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Flushing the cache of App shared memory. */
	rtds_memory_drv_flush_cache(ap_cache_flush->apmem_apaddr, ap_cache_flush->flush_size);
	ret_code = SMAP_LIB_MEMORY_OK;

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_cache_flush);

/*******************************************************************************
 * Function   : system_memory_ap_cache_clear
 * Description: This function clears the cache of App shared memory.
 * Parameters : ap_cache_clear				-   clear information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_ap_cache_clear(
	system_mem_ap_cache_clear *ap_cache_clear
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_shared_info *apmem_handle;

	MSG_HIGH("[RTAPIK]IN|[%s]\n", __func__);

	if (NULL == ap_cache_clear) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_cache_clear = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_cache_clear->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_cache_clear->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == ap_cache_clear->apmem_handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_cache_clear->apmem_handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}
	apmem_handle = (rtds_memory_shared_info *)ap_cache_clear->apmem_handle;

	if ((ap_cache_clear->apmem_apaddr < apmem_handle->app_addr) ||
		(ap_cache_clear->apmem_apaddr >= (apmem_handle->app_addr + apmem_handle->mem_size))) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_cache_clear->apmem_apaddr(0x%08X) is invalid parameter.\n", __func__, ap_cache_clear->apmem_apaddr);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if ((ap_cache_clear->clear_size > apmem_handle->mem_size) ||
		((ap_cache_clear->apmem_apaddr + ap_cache_clear->clear_size) > (apmem_handle->app_addr + apmem_handle->mem_size))) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_cache_clear->clear_size(%d) is invalid parameter.\n", __func__, ap_cache_clear->clear_size);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	/* Clearing the cache of App shared memory. */
	rtds_memory_drv_inv_cache(ap_cache_clear->apmem_apaddr, ap_cache_clear->clear_size);
	ret_code = SMAP_LIB_MEMORY_OK;

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_cache_clear);

/*******************************************************************************
 * Function   : system_memory_ap_buffer_flush
 * Description: This function flushes the buffer of App shared memory.
 * Parameters : ap_buffer_flush			 -   Flush information
 * Returns	  : None
 *******************************************************************************/
void system_memory_ap_buffer_flush(
		system_mem_ap_buffer_flush *ap_buffer_flush
)
{
	MSG_HIGH("[RTAPIK]IN|[%s]\n", __func__);

	if (NULL == ap_buffer_flush) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_buffer_flush = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s]\n", __func__);
		return;
	}
	if (NULL == ap_buffer_flush->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] ap_buffer_flush->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s]\n", __func__);
		return;
	}

	/* Flushing the buffer of App shared memory. */
	asm("MOV	r0, #0");
	asm("MCR	p15, 0, r0, c7, c10, 5");

	MSG_HIGH("[RTAPIK]OUT|[%s]\n", __func__);
	return;
}
EXPORT_SYMBOL(system_memory_ap_buffer_flush);

/*******************************************************************************
 * Function   : system_memory_ap_get_apmem_id
 * Description: This function gets a ID for shared area between processes.
 * Parameters : ap_get_apmem_id			-   get shared area ID information
 * Returns	  : apmem_id				-   shared area ID
 *******************************************************************************/
unsigned int system_memory_ap_get_apmem_id(
	system_mem_ap_get_apmem_id *ap_get_apmem_id
)
{
	rtds_memory_shared_info *apmem_handle;
	unsigned int apmem_id = 0;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == ap_get_apmem_id) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]apmem_id[%d]\n", __func__, apmem_id);
		return apmem_id;
	}
	if ((NULL == ap_get_apmem_id->handle) ||
		(NULL == ap_get_apmem_id->apmem_handle)) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]apmem_id[%d]\n", __func__, apmem_id);
		return apmem_id;
	}

	apmem_handle = (rtds_memory_shared_info  *)ap_get_apmem_id->apmem_handle;
	apmem_id = apmem_handle->apmem_id;

	MSG_HIGH("[RTAPIK]OUT|[%s]apmem_id[%d]\n", __func__, apmem_id);
	return apmem_id;
}
EXPORT_SYMBOL(system_memory_ap_get_apmem_id);

/*******************************************************************************
 * Function   : system_memory_ap_share_area
 * Description: This function shares AppDomain control shared area between processes.
 * Parameters : ap_share_area				-   shared area information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_ap_share_area(
	system_mem_ap_share_area *ap_share_area
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_shared_info			*apmem_handle = NULL;
	rtds_memory_drv_share_mem_param	rtds_memory_share_mem;
	int								pages_size = 0;
	struct page						**kernel_pages = NULL;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == ap_share_area) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}
	if ((NULL == ap_share_area->handle) ||
		(0 == ap_share_area->apmem_id)) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	apmem_handle = kmalloc(sizeof(*apmem_handle), GFP_KERNEL);
	if (NULL == apmem_handle) {
		ret_code = SMAP_LIB_MEMORY_NO_MEMORY + RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	memset(&rtds_memory_share_mem, 0, sizeof(rtds_memory_share_mem));
	rtds_memory_share_mem.apmem_id = ap_share_area->apmem_id;

	/* Creating App shared memory. */
	ret_code = rtds_memory_drv_share_apmem(&rtds_memory_share_mem);
	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	case SMAP_MEMORY:
		ret_code = SMAP_LIB_MEMORY_NO_MEMORY + RT_MEMORY_SYSTEM_ERR;
		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		break;
	}

	if (SMAP_LIB_MEMORY_OK != ret_code) {
		kfree(apmem_handle);
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	apmem_handle->app_addr   = rtds_memory_share_mem.app_addr;
	apmem_handle->rt_addr    = rtds_memory_share_mem.rt_addr;
	apmem_handle->rt_addr_nc = rtds_memory_share_mem.rt_addr_nc;
	apmem_handle->apmem_id   = rtds_memory_share_mem.apmem_id;
	apmem_handle->mem_size   = rtds_memory_share_mem.mem_size;
	apmem_handle->app_cache  = rtds_memory_share_mem.app_cache;
	apmem_handle->rt_cache   = rtds_memory_share_mem.rt_cache;
	apmem_handle->pages	     = rtds_memory_share_mem.pages;
	MSG_LOW("[RTAPIK]   |apmem_handle->app_addr[0x%08X]\n", (unsigned int)apmem_handle->app_addr);
	MSG_LOW("[RTAPIK]   |apmem_handle->rt_addr[0x%08X]\n", (unsigned int)apmem_handle->rt_addr);
	MSG_LOW("[RTAPIK]   |apmem_handle->rt_addr_nc[0x%08X]\n", (unsigned int)apmem_handle->rt_addr_nc);
	MSG_LOW("[RTAPIK]   |apmem_handle->apmem_id[%d]\n", apmem_handle->apmem_id);
	MSG_LOW("[RTAPIK]   |apmem_handle->mem_size[%d]\n", apmem_handle->mem_size);
	MSG_LOW("[RTAPIK]   |apmem_handle->app_cache[%d]\n", apmem_handle->app_cache);
	MSG_LOW("[RTAPIK]   |apmem_handle->rt_cache[%d]\n", apmem_handle->rt_cache);
	MSG_LOW("[RTAPIK]   |apmem_handle->pages[0x%08X]\n", (unsigned int)apmem_handle->pages);

	/* Page descriptor information storage area */
	pages_size = (rtds_memory_share_mem.mem_size / PAGE_SIZE) * sizeof(struct page *);
	kernel_pages = kmalloc(pages_size, GFP_KERNEL);
	if (NULL == kernel_pages) {
		kfree(apmem_handle);
		ret_code = SMAP_LIB_MEMORY_NO_MEMORY + RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	memcpy(kernel_pages, rtds_memory_share_mem.pages, pages_size);
	MSG_LOW("[RTAPIK]   |kernel_pages[0x%08X]\n", (unsigned int)kernel_pages);

	ap_share_area->apmem_handle = apmem_handle;
	ap_share_area->apaddr		= apmem_handle->app_addr;
	ap_share_area->pages		= kernel_pages;
	ret_code = SMAP_LIB_MEMORY_OK;
	MSG_MED("[RTAPIK]   |ap_share_area->apmem_handle[0x%08X]\n", (unsigned int)ap_share_area->apmem_handle);
	MSG_MED("[RTAPIK]   |ap_share_area->apaddr[0x%08X]\n", (unsigned int)ap_share_area->apaddr);
	MSG_MED("[RTAPIK]   |ap_share_area->pages[0x%08X]\n", (unsigned int)ap_share_area->pages);
	MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_share_area);

/*******************************************************************************
 * Function   : system_memory_ap_share_mem_offset
 * Description: This function gets an offset for shared memory between processes.
 * Parameters : ap_share_mem_offset			-   get offset information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *******************************************************************************/
int system_memory_ap_share_mem_offset(
	system_mem_ap_share_mem_offset *ap_share_mem_offset
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_shared_info *apmem_handle;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == ap_share_mem_offset) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}
	if ((NULL == ap_share_mem_offset->handle) ||
		(NULL == ap_share_mem_offset->apmem_handle)) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	apmem_handle = (rtds_memory_shared_info  *)ap_share_mem_offset->apmem_handle;

	if ((ap_share_mem_offset->apmem_apaddr < apmem_handle->app_addr) ||
		(ap_share_mem_offset->apmem_apaddr >= (apmem_handle->app_addr + apmem_handle->mem_size))) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	ap_share_mem_offset->apmem_offset = ap_share_mem_offset->apmem_apaddr - apmem_handle->app_addr;
	ret_code = SMAP_LIB_MEMORY_OK;
	MSG_LOW("[RTAPIK]   |*offset[%d]\n", ap_share_mem_offset->apmem_offset);
	MSG_HIGH("[RTAPIK]OUT|[%s]\n", __func__);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_share_mem_offset);

/*******************************************************************************
 * Function   : system_memory_ap_share_mem
 * Description: This function shares AppDomain control shared memory between processes.
 * Parameters : ap_share_mem				-   shared memory information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *******************************************************************************/
int system_memory_ap_share_mem(
	system_mem_ap_share_mem *ap_share_mem
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_shared_info *apmem_handle;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == ap_share_mem) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}
	if ((NULL == ap_share_mem->handle) ||
		(NULL == ap_share_mem->apmem_handle)) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	apmem_handle = (rtds_memory_shared_info  *)ap_share_mem->apmem_handle;

	if (ap_share_mem->apmem_offset >= apmem_handle->mem_size) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	ap_share_mem->apmem_apaddr = apmem_handle->app_addr + ap_share_mem->apmem_offset;
	ret_code = SMAP_LIB_MEMORY_OK;

	MSG_MED("[RTAPIK]   |apmem_apaddr[0x%08X]\n", ap_share_mem->apmem_apaddr);
	MSG_HIGH("[RTAPIK]OUT|[%s]\n", __func__);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_ap_share_mem);


/*******************************************************************************
 * Function   : system_memory_rt_map
 * Description: This function map of the physical continuation domain.
 * Parameters : rt_map						-   Physical continuation domain map information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_rt_map(
	system_mem_rt_map   *rt_map
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_drv_map_param  rtds_drv_map;
	iccom_drv_send_cmd_param iccom_send_cmd;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Paremeter check */
	if (NULL == rt_map) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}
	if ((NULL == rt_map->handle) ||
		(0 != (rt_map->phys_addr & (RT_MEMORY_PAGESIZE-1))) ||
		(0 != (rt_map->map_size & (RT_MEMORY_PAGESIZE-1)))) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	/* Set rtds_drv_map info */
	rtds_drv_map.handle   = rt_map->handle;
	rtds_drv_map.phy_addr = rt_map->phys_addr;
	rtds_drv_map.mem_size = rt_map->map_size;
	rtds_drv_map.app_addr = 0;

	/* Map of the physical continuation domain */
	ret_code = rtds_memory_drv_map(&rtds_drv_map);

	switch (ret_code) {
	case SMAP_OK:
		rt_map->rtaddr = rtds_drv_map.app_addr;
		MSG_MED("[RTAPIK]INF|[%s] rt_map->rtaddr = 0x%08x\n", __func__, rt_map->rtaddr);

		/* Check of the logical address mapped RTDomain */
		if (0 == rt_map->rtaddr) {
			ret_code += RT_MEMORY_SYSTEM_ERR;
			MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
			MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
			return ret_code;
		}
		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	/* Message parameter settings. */
	iccom_send_cmd.handle		= ((mem_rtapi_info_handle *)rt_map->handle)->iccom_handle;
	iccom_send_cmd.task_id		= TASK_MEMORY;
	iccom_send_cmd.function_id  = EVENT_MEMORY_FLUSHTLB;
	iccom_send_cmd.send_mode	= ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size	= 0;
	iccom_send_cmd.send_data	= NULL;
	iccom_send_cmd.recv_size	= 0;
	iccom_send_cmd.recv_data	= NULL;

	/* The message is sent. */
	ret_code = iccom_drv_send_command(&iccom_send_cmd);

	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]ret_code[%d]\n", __func__, __LINE__, ret_code);
		break;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_rt_map);


/*******************************************************************************
 * Function   : system_memory_rt_unmap
 * Description: This function unmap of the physical continuation domain.
 * Parameters : rt_unmap					-   Physical continuation domain unmap information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_rt_unmap(
	system_mem_rt_unmap  *rt_unmap
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_drv_unmap_param  rtds_drv_unmap;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Paremeter check */
	if (NULL == rt_unmap) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}
	if ((NULL == rt_unmap->handle) ||
		(0 != (rt_unmap->rtaddr & (RT_MEMORY_PAGESIZE-1))) ||
		(0 != (rt_unmap->map_size & (RT_MEMORY_PAGESIZE-1)))) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	/* Set rtds_drv_unmap info */
	rtds_drv_unmap.handle   = rt_unmap->handle;
	rtds_drv_unmap.app_addr = rt_unmap->rtaddr;
	rtds_drv_unmap.mem_size = rt_unmap->map_size;

	/* Unmap of the physical continuation domain */
	ret_code = rtds_memory_drv_unmap(&rtds_drv_unmap);

	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		break;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_rt_unmap);


/*******************************************************************************
 * Function   : system_memory_rt_map_pnc
 * Description: This function map of the Physical non-continuation domain.
 * Parameters : rt_map_pnc					-   Physical non-continuation domain map information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_rt_map_pnc(
	system_mem_rt_map_pnc   *rt_map_pnc
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_drv_map_pnc_param	rtds_drv_map_pnc;
	rtds_memory_shared_info			*apmem_handle = NULL;
	unsigned int					cache_kind = 0;
	int								pages_size = 0;
	struct page						**kernel_pages = NULL;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Paremeter check */
	if (NULL == rt_map_pnc) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	cache_kind = (RT_MEMORY_RTMAP_WB | RT_MEMORY_RTMAP_WBNC);
	if ((NULL == rt_map_pnc->handle) ||
		(0 == rt_map_pnc->apaddr) ||
		(0 != (rt_map_pnc->map_size & (RT_MEMORY_PAGESIZE-1)))  ||
		(0 == rt_map_pnc->map_size) ||
		(NULL == rt_map_pnc->pages) ||
		(0 != (~cache_kind & rt_map_pnc->rtcache_kind))
		) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	/* Page descriptor information storage area */
	pages_size = (rt_map_pnc->map_size / PAGE_SIZE) * sizeof(struct page *);
	kernel_pages = kmalloc(pages_size, GFP_KERNEL);
	if (NULL == kernel_pages) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return RT_MEMORY_SYSTEM_ERR;
	}

	memcpy(kernel_pages, rt_map_pnc->pages, pages_size);
	MSG_LOW("[RTAPIK]   |kernel_pages[0x%08X]\n", (unsigned int)kernel_pages);

	/* Set rtds_drv_map_pnc info */
	memset(&rtds_drv_map_pnc, 0, sizeof(rtds_drv_map_pnc));
	rtds_drv_map_pnc.handle		= rt_map_pnc->handle;
	rtds_drv_map_pnc.app_addr	= rt_map_pnc->apaddr;
	rtds_drv_map_pnc.map_size	= rt_map_pnc->map_size;
	rtds_drv_map_pnc.pages		= kernel_pages;
	rtds_drv_map_pnc.rtcache_kind = rt_map_pnc->rtcache_kind;

	/* Map of the Physical non-continuation domain */
	ret_code = rtds_memory_drv_map_pnc(&rtds_drv_map_pnc);

	switch (ret_code) {
	case SMAP_OK:
		apmem_handle = kmalloc(sizeof(*apmem_handle), GFP_KERNEL);

		apmem_handle->app_addr		= rtds_drv_map_pnc.app_addr;
		apmem_handle->rt_addr		= rtds_drv_map_pnc.mem_info.rt_write_back_addr;
		apmem_handle->rt_addr_nc	= rtds_drv_map_pnc.mem_info.rt_non_cache_addr;
		apmem_handle->apmem_id		= rtds_drv_map_pnc.mem_info.apmem_id;
		apmem_handle->mem_size		= rtds_drv_map_pnc.map_size;
		apmem_handle->app_cache		= 0;
		apmem_handle->rt_cache		= rtds_drv_map_pnc.rtcache_kind;
		apmem_handle->pages			= rtds_drv_map_pnc.pages;

		rt_map_pnc->apmem_handle = apmem_handle;
		ret_code = SMAP_LIB_MEMORY_OK;

		MSG_LOW("[RTAPIK]   |apmem_handle->app_addr[0x%08X]\n", (unsigned int)apmem_handle->app_addr);
		MSG_LOW("[RTAPIK]   |apmem_handle->rt_addr[0x%08X]\n", (unsigned int)apmem_handle->rt_addr);
		MSG_LOW("[RTAPIK]   |apmem_handle->rt_addr_nc[0x%08X]\n", (unsigned int)apmem_handle->rt_addr_nc);
		MSG_LOW("[RTAPIK]   |apmem_handle->apmem_id[%d]\n", apmem_handle->apmem_id);
		MSG_LOW("[RTAPIK]   |apmem_handle->mem_size[%d]\n", apmem_handle->mem_size);
		MSG_LOW("[RTAPIK]   |apmem_handle->rt_cache[%d]\n", apmem_handle->rt_cache);
		MSG_LOW("[RTAPIK]   |apmem_handle->pages[0x%08X]\n", (unsigned int)apmem_handle->pages);
		MSG_MED("[RTAPIK]   |apmem_handle[0x%08X]\n", (unsigned int)rt_map_pnc->apmem_handle);

		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		break;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_rt_map_pnc);


/*******************************************************************************
 * Function   : system_memory_rt_unmap_pnc
 * Description: This function unmap of the Physical non-continuation domain.
 * Parameters : rt_unmap_pnc				-   Physical non-continuation domain unmap information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_rt_unmap_pnc(
	system_mem_rt_unmap_pnc   *rt_unmap_pnc
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_drv_unmap_pnc_param  rtds_drv_unmap_pnc;
	rtds_memory_shared_info *apmem_handle = NULL;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Paremeter check */
	if (NULL == rt_unmap_pnc) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	if ((NULL == rt_unmap_pnc->handle) ||
		(NULL == rt_unmap_pnc->apmem_handle)) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	apmem_handle = (rtds_memory_shared_info *)rt_unmap_pnc->apmem_handle;

	/* Set rtds_drv_unmap_pnc info */
	rtds_drv_unmap_pnc.handle	 = rt_unmap_pnc->handle;
	rtds_drv_unmap_pnc.app_addr	 = apmem_handle->app_addr;
	rtds_drv_unmap_pnc.apmem_id	 = apmem_handle->apmem_id;

	/* UNMap of the Physical non-continuation domain */
	ret_code = rtds_memory_drv_unmap_pnc(&rtds_drv_unmap_pnc);

	switch (ret_code) {
	case SMAP_OK:
		kfree(apmem_handle);
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		break;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_rt_unmap_pnc);


/*******************************************************************************
 * Function   : system_memory_rt_map_pnc_nma
 * Description: This function map of the Physical non-continuation domain.
 * Parameters : rt_map_pnc_nma				-   Physical non-continuation domain map information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_rt_map_pnc_nma(
	system_mem_rt_map_pnc_nma   *rt_map_pnc_nma
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_drv_map_pnc_nma_param   rtds_drv_map_pnc_nma;
	iccom_drv_send_cmd_param iccom_send_cmd;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Paremeter check */
	if (NULL == rt_map_pnc_nma) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}
	if ((NULL == rt_map_pnc_nma->handle) ||
		(0 != (rt_map_pnc_nma->map_size & (RT_MEMORY_PAGESIZE-1))) ||
		(0 == rt_map_pnc_nma->map_size) ||
		(NULL == rt_map_pnc_nma->pages)
		) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	/* Set rtds_drv_map_pnc_nma info */
	memset(&rtds_drv_map_pnc_nma, 0, sizeof(rtds_drv_map_pnc_nma));
	rtds_drv_map_pnc_nma.handle     = rt_map_pnc_nma->handle;
	rtds_drv_map_pnc_nma.map_size   = rt_map_pnc_nma->map_size;
	rtds_drv_map_pnc_nma.pages      = rt_map_pnc_nma->pages;

	/* Map of the physical non-continuation domain */
	ret_code = rtds_memory_drv_map_pnc_nma(&rtds_drv_map_pnc_nma);

	switch (ret_code) {
	case SMAP_OK:
		rt_map_pnc_nma->rt_addr_wb = rtds_drv_map_pnc_nma.rt_addr_wb;
		MSG_MED("[RTAPIK]INF|[%s] rt_map_pnc_nma->rt_addr_wb = 0x%08x\n", __func__, rt_map_pnc_nma->rt_addr_wb);

		/* Check of the logical address mapped RTDomain */
		if (0 == rt_map_pnc_nma->rt_addr_wb) {
			ret_code += RT_MEMORY_SYSTEM_ERR;
			MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
			MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
			return ret_code;
		}
		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	/* Message parameter settings. */
	iccom_send_cmd.handle		= ((mem_rtapi_info_handle *)rt_map_pnc_nma->handle)->iccom_handle;
	iccom_send_cmd.task_id		= TASK_MEMORY;
	iccom_send_cmd.function_id  = EVENT_MEMORY_FLUSHTLB;
	iccom_send_cmd.send_mode	= ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size	= 0;
	iccom_send_cmd.send_data	= NULL;
	iccom_send_cmd.recv_size	= 0;
	iccom_send_cmd.recv_data	= NULL;

	/* The message is sent. */
	ret_code = iccom_drv_send_command(&iccom_send_cmd);

	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]ret_code[%d]\n", __func__, __LINE__, ret_code);
		break;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_rt_map_pnc_nma);


/*******************************************************************************
 * Function   : system_memory_rt_unmap_pnc_nma
 * Description: This function unmap of the Physical non-continuation domain.
 * Parameters : rt_unmap_pnc_nma			-   Physical non-continuation domain unmap information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int system_memory_rt_unmap_pnc_nma(
	system_mem_rt_unmap_pnc_nma  *rt_unmap_pnc_nma
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	rtds_memory_drv_unmap_param  rtds_drv_unmap;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Paremeter check */
	if (NULL == rt_unmap_pnc_nma) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}
	if ((NULL == rt_unmap_pnc_nma->handle) ||
		(0 != (rt_unmap_pnc_nma->rt_addr_wb & (RT_MEMORY_PAGESIZE-1))) ||
		(0 == rt_unmap_pnc_nma->rt_addr_wb) ||
		(0 != (rt_unmap_pnc_nma->map_size & (RT_MEMORY_PAGESIZE-1))) ||
		(0 == rt_unmap_pnc_nma->map_size)
		) {
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
		return ret_code;
	}

	/* Set rtds_drv_unmap_pnc_nma info */
	rtds_drv_unmap.handle   = rt_unmap_pnc_nma->handle;
	rtds_drv_unmap.app_addr = rt_unmap_pnc_nma->rt_addr_wb;
	rtds_drv_unmap.mem_size = rt_unmap_pnc_nma->map_size;

	/* Unmap of the physical continuation domain */
	ret_code = rtds_memory_drv_unmap(&rtds_drv_unmap);

	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		break;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s]ret_code[%d]\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_rt_unmap_pnc_nma);


/*******************************************************************************
 * Function   : system_memory_info_new
 * Description: This function create handle getting the memory information.
 * Parameters : None
 * Returns	  : Handle getting the memory information(Returns NULL on failure)
 *******************************************************************************/
void *system_memory_info_new(
	void
)
{
	mem_info_handle *mem_info = NULL;
	iccom_drv_init_param   iccom_init;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Allocating memory for handle. */
	mem_info = kmalloc(sizeof(*mem_info), GFP_KERNEL);
	if (NULL == mem_info) {
		MSG_ERROR("[RTAPIK]ERR|[%s] mem_info[NULL].\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s]\n", __func__);
		return NULL;
	}

	/* Creating a ICCOM handle. */
	iccom_init.user_data = NULL;
	iccom_init.comp_notice = NULL;

	mem_info->handle = iccom_drv_init(&iccom_init);
	if (NULL == mem_info->handle) {
		kfree(mem_info);
		MSG_ERROR("[RTAPIK]ERR|[%s] mem_info->handle[NULL].\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s]\n", __func__);
		return NULL;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] mem_info[0x%08x].\n", __func__, (unsigned int)mem_info);
	return (void *)mem_info;
}
EXPORT_SYMBOL(system_memory_info_new);

/*******************************************************************************
 * Function   : system_memory_get_rtinfo
 * Description: This function gets RT shared memory information.
 * Parameters : get_rtinfo			  -   Memory type to get memory information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-   System Error
 *******************************************************************************/
int  system_memory_get_rtinfo(
	system_mem_get_rtinfo  *get_rtinfo
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	iccom_drv_send_cmd_param	iccom_send_cmd;
	mem_info_handle				*mem_info;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == get_rtinfo) {
		MSG_ERROR("[RTAPIK]ERR|[%s] get_rtinfo = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == get_rtinfo->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] get_rtinfo->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if ((RT_MEMORY_WORK != get_rtinfo->memory_type) &&
		(RT_MEMORY_WORKSMALL != get_rtinfo->memory_type)) {
		MSG_ERROR("[RTAPIK]ERR|[%s] get_rtinfo->memory_type(%d) is invalid parameter.\n", __func__, get_rtinfo->memory_type);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	mem_info = (mem_info_handle *)get_rtinfo->handle;

	/* Message parameter settings. */
	iccom_send_cmd.handle	   = mem_info->handle;
	iccom_send_cmd.task_id	   = TASK_MEMORY;
	iccom_send_cmd.function_id = EVENT_MEMORY_GETMEMINFORT;
	iccom_send_cmd.send_mode   = ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size   = sizeof(*get_rtinfo);
	iccom_send_cmd.send_data   = (unsigned char *)get_rtinfo;
	iccom_send_cmd.recv_size   = sizeof(get_rtinfo->mem_info);
	iccom_send_cmd.recv_data   = (unsigned char *)&get_rtinfo->mem_info;

	/* The message is sent. */
	ret_code = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != ret_code) {
		if (ICCOM_DRV_SYSTEM_ERR < ret_code) {
			ret_code = SMAP_LIB_MEMORY_PARA_NG;
		}
		MSG_ERROR("[RTAPIK]ERR|[%s] iccom_drv_send_command() failed.\n", __func__);
	} else {
		MSG_MED("[RTAPIK]INF|[%s] mem_size = %d\n", __func__, get_rtinfo->mem_info.mem_size);
		MSG_MED("[RTAPIK]INF|[%s] free_size = %d\n", __func__, get_rtinfo->mem_info.free_size);
		MSG_MED("[RTAPIK]INF|[%s] max_free_size = %d\n", __func__, get_rtinfo->mem_info.max_free_size);
		ret_code = SMAP_LIB_MEMORY_OK;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_get_rtinfo);

/*******************************************************************************
 * Function   : system_memory_get_apinfo
 * Description: This function gets APP shared memory information.
 * Parameters : get_apinfo				-   Memory type to get memory information
 * Returns	  : SMAP_LIB_MEMORY_OK		-   Success
 *				SMAP_LIB_MEMORY_PARA_NG -   Parameter error
 *				Others					-   System Error
 *******************************************************************************/
int system_memory_get_apinfo(
	system_mem_get_apinfo  *get_apinfo
)
{
	int ret_code = SMAP_LIB_MEMORY_PARA_NG;
	iccom_drv_send_cmd_param	iccom_send_cmd;
	mem_info_handle				*mem_info;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == get_apinfo) {
		MSG_ERROR("[RTAPIK]ERR|[%s] get_apinfo = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (NULL == get_apinfo->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] get_apinfo->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	mem_info = (mem_info_handle *)get_apinfo->handle;

	/* Message parameter settings. */
	iccom_send_cmd.handle	   = mem_info->handle;
	iccom_send_cmd.task_id	   = TASK_MEMORY_SUB;
	iccom_send_cmd.function_id = EVENT_MEMORYSUB_GETMEMINFOAPP;
	iccom_send_cmd.send_mode   = ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size   = sizeof(get_apinfo->apmem_id);
	iccom_send_cmd.send_data   = (unsigned char *)&get_apinfo->apmem_id;
	iccom_send_cmd.recv_size   = sizeof(get_apinfo->mem_info);
	iccom_send_cmd.recv_data   = (unsigned char *)&get_apinfo->mem_info;

	MSG_MED("[RTAPIK]INF|[%s] get_apinfo->apmem_id = %d\n", __func__, get_apinfo->apmem_id);

	/* The message is sent. */
	ret_code = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != ret_code) {
		if (ICCOM_DRV_SYSTEM_ERR < ret_code) {
			ret_code = SMAP_LIB_MEMORY_PARA_NG;
		}
		MSG_ERROR("[RTAPIK]ERR|[%s] iccom_drv_send_command() failed.\n", __func__);
	} else {
		MSG_MED("[RTAPIK]INF|[%s] mem_size = %d\n", __func__, get_apinfo->mem_info.mem_size);
		MSG_MED("[RTAPIK]INF|[%s] free_size = %d\n", __func__, get_apinfo->mem_info.free_size);
		MSG_MED("[RTAPIK]INF|[%s] max_free_size = %d\n", __func__, get_apinfo->mem_info.max_free_size);
		ret_code = SMAP_LIB_MEMORY_OK;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_get_apinfo);

/*******************************************************************************
 * Function   : system_memory_info_delete
 * Description: This function delete handle getting the memory information.
 * Parameters : info_delete   -   Handle getting the memory information
 * Returns	  : None
 *******************************************************************************/
void system_memory_info_delete(
	system_mem_info_delete  *info_delete
)
{
	iccom_drv_cleanup_param	iccom_cleanup;
	mem_info_handle			*mem_info;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	/* Parameter check. */
	if (NULL == info_delete) {
		MSG_ERROR("[RTAPIK]ERR|[%s] info_delete = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s]\n", __func__);
		return;
	}

	if (NULL == info_delete->handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] info_delete->handle = NULL.\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s]\n", __func__);
		return;
	}

	mem_info = (mem_info_handle *)info_delete->handle;

	/* Deleting a ICCOM handle. */
	iccom_cleanup.handle = mem_info->handle;
	iccom_drv_cleanup(&iccom_cleanup);
	kfree(info_delete->handle);

	MSG_HIGH("[RTAPIK]OUT|[%s]\n", __func__);
	return;
}
EXPORT_SYMBOL(system_memory_info_delete);

/*******************************************************************************
 * Function   : system_memory_reg_phymem
 * Description: This function register address translation information.
 * Parameters : reg_phymem   -   Address translation information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *******************************************************************************/
int system_memory_reg_phymem(
	system_mem_reg_phymem *reg_phymem
)
{
	int ret_code = SMAP_OK;
	rtds_memory_drv_map_param  rtds_drv_map;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == reg_phymem) {
		MSG_ERROR("[RTAPIK]ERR|[%s]reg_phymem NULL\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}
	if ((NULL == reg_phymem->handle) ||
		(0 == reg_phymem->phys_addr) ||
		(0 == reg_phymem->map_size)) {
		MSG_ERROR("[RTAPIK]ERR|[%s]reg_phymem->handle    = 0x%08X\n", __func__, (u32)reg_phymem->handle);
		MSG_ERROR("[RTAPIK]ERR|[%s]reg_phymem->phys_addr = 0x%08X\n", __func__, reg_phymem->phys_addr);
		MSG_ERROR("[RTAPIK]ERR|[%s]reg_phymem->map_size  = 0x%08X\n", __func__, reg_phymem->map_size);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}

	rtds_drv_map.handle   = reg_phymem->handle;
	rtds_drv_map.phy_addr = reg_phymem->phys_addr;
	rtds_drv_map.mem_size = reg_phymem->map_size;
	rtds_drv_map.app_addr = reg_phymem->rtaddr;

	MSG_LOW("[RTAPIK]   |rtds_drv_map.handle[0x%08X]\n", (u32)rtds_drv_map.handle);
	MSG_LOW("[RTAPIK]   |rtds_drv_map.phy_addr[0x%08X]\n", rtds_drv_map.phy_addr);
	MSG_LOW("[RTAPIK]   |rtds_drv_map.mem_size[0x%08X]\n", rtds_drv_map.mem_size);
	MSG_LOW("[RTAPIK]   |rtds_drv_map.app_addr[0x%08X]\n", rtds_drv_map.app_addr);

	ret_code = rtds_memory_drv_reg_phymem(&rtds_drv_map);

	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s]rtds_memory_drv_reg_phymem ret_code = %d\n", __func__, ret_code);
		break;
	}
	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_reg_phymem);

/*******************************************************************************
 * Function   : system_memory_unreg_phymem
 * Description: This function unregister address translation information.
 * Parameters : unreg_phymem   -   Address translation information
 * Returns	  : SMAP_LIB_MEMORY_OK			 -   Success
 *				SMAP_LIB_MEMORY_PARA_NG		 -   Parameter error
 *******************************************************************************/
int system_memory_unreg_phymem(
	system_mem_unreg_phymem *unreg_phymem
)
{
	int ret_code = SMAP_OK;
	rtds_memory_drv_map_param  rtds_drv_map;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == unreg_phymem) {
		MSG_ERROR("[RTAPIK]ERR|[%s]unreg_phymem NULL\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}
	if ((NULL == unreg_phymem->handle) ||
		(0 == unreg_phymem->phys_addr) ||
		(0 == unreg_phymem->map_size)) {
		MSG_ERROR("[RTAPIK]ERR|[%s]unreg_phymem->handle    = 0x%08X\n", __func__, (u32)unreg_phymem->handle);
		MSG_ERROR("[RTAPIK]ERR|[%s]unreg_phymem->phys_addr = 0x%08X\n", __func__, unreg_phymem->phys_addr);
		MSG_ERROR("[RTAPIK]ERR|[%s]unreg_phymem->map_size  = 0x%08X\n", __func__, unreg_phymem->map_size);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}
	rtds_drv_map.handle   = unreg_phymem->handle;
	rtds_drv_map.phy_addr = unreg_phymem->phys_addr;
	rtds_drv_map.mem_size = unreg_phymem->map_size;
	rtds_drv_map.app_addr = unreg_phymem->rtaddr;

	MSG_LOW("[RTAPIK]   |rtds_drv_map.handle[0x%08X]\n", (u32)rtds_drv_map.handle);
	MSG_LOW("[RTAPIK]   |rtds_drv_map.phy_addr[0x%08X]\n", rtds_drv_map.phy_addr);
	MSG_LOW("[RTAPIK]   |rtds_drv_map.mem_size[0x%08X]\n", rtds_drv_map.mem_size);
	MSG_LOW("[RTAPIK]   |rtds_drv_map.app_addr[0x%08X]\n", rtds_drv_map.app_addr);

	ret_code = rtds_memory_drv_unreg_phymem(&rtds_drv_map);

	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	default:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		MSG_ERROR("[RTAPIK]ERR|[%s]rtds_memory_drv_unreg_phymem ret_code = %d\n", __func__, ret_code);
		break;
	}
	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_unreg_phymem);

/*******************************************************************************
 * Function   : system_memory_phy_change_rtaddr
 * Description: This function change physical address to logical address
 * Parameters : phy_change_rtaddr   -   Address change information
 * Returns	  : SMAP_LIB_MEMORY_OK			-   Success
 *				SMAP_LIB_MEMORY_PARA_NG		-   Parameter error
 *				Others						-	System Error
 *******************************************************************************/
int system_memory_phy_change_rtaddr(
	system_mem_phy_change_rtaddr *phy_change_rtaddr
)
{
	int ret_code = SMAP_OK;
	rtds_memory_drv_change_addr_param rtds_change_addr;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == phy_change_rtaddr) {
		MSG_ERROR("[RTAPIK]ERR|[%s]phy_change_rtaddr NULL\n", __func__);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}
	if ((NULL == phy_change_rtaddr->handle) ||
		(0 == phy_change_rtaddr->phys_addr)) {
		MSG_ERROR("[RTAPIK]ERR|[%s]phy_change_rtaddr->handle    = 0x%08X\n", __func__, (u32)phy_change_rtaddr->handle);
		MSG_ERROR("[RTAPIK]ERR|[%s]phy_change_rtaddr->phys_addr = 0x%08X\n", __func__, phy_change_rtaddr->phys_addr);
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}

	memset(&rtds_change_addr, 0, sizeof(rtds_change_addr));
	rtds_change_addr.handle   = phy_change_rtaddr->handle;
	rtds_change_addr.org_addr = phy_change_rtaddr->phys_addr;
	rtds_change_addr.chg_addr = 0;

	MSG_LOW("[RTAPIK]   |rtds_change_addr.handle[0x%08X]\n", (u32)rtds_change_addr.handle);
	MSG_LOW("[RTAPIK]   |rtds_change_addr.org_addr[0x%08X]\n", rtds_change_addr.org_addr);
	MSG_LOW("[RTAPIK]   |rtds_change_addr.chg_addr[0x%08X]\n", rtds_change_addr.chg_addr);

	ret_code = rtds_memory_drv_phy_change_address(&rtds_change_addr);

	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		phy_change_rtaddr->rtaddr = rtds_change_addr.chg_addr;
		MSG_LOW("[RTAPIK]   |*rt_addr[0x%08X]\n", phy_change_rtaddr->rtaddr);
		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		MSG_ERROR("[RTAPIK]ERR|[%s][%d]\n", __func__, __LINE__);
		break;
	}

	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_phy_change_rtaddr);

int system_memory_meram_alloc(
	system_mem_meram_alloc  *meram_alloc
)
{
	int ret_code = SMAP_OK;
	iccom_drv_send_cmd_param send_cmd;
	struct {
		u32 offset;
		u32 ch_num;
	} recv_data;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == meram_alloc) {
		MSG_HIGH("[RTAPIK]>%s ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}

	if ((NULL == meram_alloc->handle) ||
		(0 == meram_alloc->alloc_size)) {
		MSG_HIGH("[RTAPIK]>%s ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}

	meram_alloc->meram_offset = 0;
	meram_alloc->ch_num = 0;

	send_cmd.handle	  = ((mem_rtapi_info_handle *)meram_alloc->handle)->iccom_handle;
	send_cmd.task_id	 = TASK_MEMORY;
	send_cmd.function_id = EVENT_MEMORY_ALLOCATEMERAM;
	send_cmd.send_mode   = ICCOM_DRV_SYNC;
	send_cmd.send_size   = sizeof(meram_alloc->alloc_size);
	send_cmd.send_data   = (unsigned char *)&meram_alloc->alloc_size;
	send_cmd.recv_size   = sizeof(recv_data);
	send_cmd.recv_data   = (unsigned char *)&recv_data;

	ret_code = iccom_drv_send_command(&send_cmd);
	if (SMAP_OK == ret_code) {
		meram_alloc->meram_offset = recv_data.offset;
		meram_alloc->ch_num       = recv_data.ch_num;
	}

	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		break;
	case SMAP_MEMORY:
		ret_code = SMAP_LIB_MEMORY_NO_MEMORY;
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		break;
	}
	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_meram_alloc);

int system_memory_meram_free(
	system_mem_meram_free *meram_free
)
{
	int ret_code = SMAP_OK;
	iccom_drv_send_cmd_param send_cmd;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == meram_free) {
		MSG_HIGH("[RTAPIK]>%s ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}

	if (NULL == meram_free->handle) {
		MSG_HIGH("[RTAPIK]>%s ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}
	send_cmd.handle	  = ((mem_rtapi_info_handle *)meram_free->handle)->iccom_handle;
	send_cmd.task_id	 = TASK_MEMORY;
	send_cmd.function_id = EVENT_MEMORY_FREEMERAM;
	send_cmd.send_mode   = ICCOM_DRV_SYNC;
	send_cmd.send_size   = sizeof(meram_free->ch_num);
	send_cmd.send_data   = (unsigned char *)&meram_free->ch_num;
	send_cmd.recv_size   = 0;
	send_cmd.recv_data   = NULL;

	ret_code = iccom_drv_send_command(&send_cmd);

	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_MEMORY_OK;
		break;
	case SMAP_PARA_NG:
		ret_code = SMAP_LIB_MEMORY_PARA_NG;
		break;
	case SMAP_MEMORY:
		ret_code = SMAP_LIB_MEMORY_NO_MEMORY;
		break;
	default:
		ret_code += RT_MEMORY_SYSTEM_ERR;
		break;
	}
	MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(system_memory_meram_free);

int system_memory_phy_change_rtpmbaddr(
	system_mem_phy_change_rtpmbaddr *phy_change_rtaddr
)
{
	rtds_memory_drv_change_addr_param rtds_change_addr;
	int ret_code = SMAP_OK;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == phy_change_rtaddr) {
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}

	/* change address physical to logical */
	memset(&rtds_change_addr, 0, sizeof(rtds_change_addr));
	rtds_change_addr.org_addr = phy_change_rtaddr->phys_addr;
	rtds_change_addr.chg_addr = 0;
	ret_code = rtds_memory_drv_phy_to_rtpmb_address(&rtds_change_addr);
	if (ret_code != SMAP_OK) {
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}

	phy_change_rtaddr->rtmem_rtpmbaddr = rtds_change_addr.chg_addr;

	MSG_HIGH("[RTAPIK]OUT|[%s] 0x%08x -> 0x%08x\n", __func__, phy_change_rtaddr->phys_addr, phy_change_rtaddr->rtmem_rtpmbaddr);
	return SMAP_LIB_MEMORY_OK;
}
EXPORT_SYMBOL(system_memory_phy_change_rtpmbaddr);

int system_memory_rtpmb_change_phyaddr(
	system_mem_rtpmb_change_phyaddr *rt_change_phyaddr
)
{
	rtds_memory_drv_change_addr_param rtds_change_addr;
	int ret_code = SMAP_OK;

	MSG_HIGH("[RTAPIK]IN |[%s]\n", __func__);

	if (NULL == rt_change_phyaddr) {
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}
	/* change address logical to physical */
	memset(&rtds_change_addr, 0, sizeof(rtds_change_addr));
	rtds_change_addr.org_addr = rt_change_phyaddr->rtmem_rtpmbaddr;
	rtds_change_addr.chg_addr = 0;
	ret_code = rtds_memory_drv_rtpmb_to_phy_address(&rtds_change_addr);
	if (ret_code != SMAP_OK) {
		MSG_HIGH("[RTAPIK]OUT|[%s] ret_code = %d\n",
			__func__, SMAP_LIB_MEMORY_PARA_NG);
		return SMAP_LIB_MEMORY_PARA_NG;
	}

	rt_change_phyaddr->phys_addr = rtds_change_addr.chg_addr;

	MSG_HIGH("[RTAPIK]OUT|[%s] 0x%08x -> 0x%08x\n", __func__, rt_change_phyaddr->rtmem_rtpmbaddr , rt_change_phyaddr->phys_addr);
	return SMAP_LIB_MEMORY_OK;
}
EXPORT_SYMBOL(system_memory_rtpmb_change_phyaddr);
