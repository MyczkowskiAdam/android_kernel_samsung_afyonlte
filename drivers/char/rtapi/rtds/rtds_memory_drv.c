/*
 * rtds_memory_drv.c
 *	 RT domain shared memory device driver API function file.
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

#include <linux/types.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/semaphore.h>
#include <asm/cacheflush.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

#include "log_kernel.h"
#include "system_memory.h"
#include "rtds_memory_drv.h"
#include "rtds_memory_drv_main.h"
#include "rtds_memory_drv_common.h"
#include "rtds_memory_drv_sub.h"
#include "rtds_memory_drv_private.h"
#include "iccom_drv.h"

#define RTDS_MEMORY_DRV_CACHE_MASK	(0xFFFFFFF8)


/*****************************************************************************
 * Function   : rtds_memory_drv_init
 * Description: This function creates RTDS MEMORY driver handle.
 * Parameters : rtds_memory_drv_init	- handle informaion
 * Returns	  : RTDS MEMORY driver handle address
 *
 *****************************************************************************/
void *rtds_memory_drv_init(
		void
)
{
	rtds_memory_drv_handle	 *handle = NULL; /* RTDS MEMORY handle */

	MSG_MED("[RTDSK]IN |[%s]\n", __func__);

	/* Create RTDS MEMORY handle */
	handle = rtds_memory_create_handle();

	if (NULL == handle) {
		MSG_ERROR("[RTDSK]ERR|" \
			" rtds_memory_create_handle failed handle[NULL]\n");
	} else {
		/* Initialise handle info */
		rtds_memory_init_data(handle);
	}

	MSG_MED("[RTDSK]OUT|[%s] handle[0x%08x]\n",
		__func__, (unsigned int)handle);

	return (void *)handle;
}
EXPORT_SYMBOL(rtds_memory_drv_init);

/*****************************************************************************
 * Function   : rtds_memory_drv_cleanup
 * Description: This function cleanups RTDS MEMORY driver handle.
 * Parameters : rtds_memory_cleanup - handle informaion
 * Returns	  : none
 *
 ****************************************************************************/
void rtds_memory_drv_cleanup(
		rtds_memory_drv_cleanup_param	*rtds_memory_cleanup
)
{
	rtds_memory_drv_handle *handle;

	MSG_MED("[RTDSK]IN |[%s]\n", __func__);

	/* NULL check */
	if (NULL == rtds_memory_cleanup) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_creanup[NULL]\n");
	} else if (NULL == rtds_memory_cleanup->handle) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_creanup->handle[NULL]\n");
	} else {
		/* Release RTDS MEMORY driver handle region */
		handle = (rtds_memory_drv_handle *)rtds_memory_cleanup->handle;
		rtds_memory_destroy_handle(handle);
	}

	MSG_MED("[RTDSK]OUT|[%s]\n", __func__);

	return;
}
EXPORT_SYMBOL(rtds_memory_drv_cleanup);

/*****************************************************************************
 * Function   : rtds_memory_drv_open_apmem
 * Description: This function creates App shared memory.
 * Parameters : rtds_memory_open_mem - App shared memory create info
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 *				SMAP_MEMORY		- No memory
 *
 ****************************************************************************/
int rtds_memory_drv_open_apmem(
	rtds_memory_drv_open_mem_param	 *rtds_memory_open_mem
)
{
	int ret = SMAP_PARA_NG;
	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);
	MSG_MED("[RTDSK]   |mem_size[0x%08X]\n",
		rtds_memory_open_mem->mem_size);
	MSG_MED("[RTDSK]   |app_cache[0x%08X]\n",
		rtds_memory_open_mem->app_cache);
	MSG_MED("[RTDSK]   |pages[0x%08X]\n",
		(unsigned int)rtds_memory_open_mem->pages);

	/* Paremeter check */
	if (NULL == rtds_memory_open_mem) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_open_mem is NULL\n");
		MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);
		return ret;
	}

	if (0 == rtds_memory_open_mem->mem_size) {
		MSG_ERROR("[RTDSK]ERR| mem_size is 0\n");
		MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);
		return ret;
	}

	if (0 != (rtds_memory_open_mem->app_cache & RTDS_MEMORY_DRV_CACHE_MASK)) {
		MSG_ERROR("[RTDSK]ERR| app_cache is 0x%08x\n",
			rtds_memory_open_mem->app_cache);
		MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);
		return ret;
	}

	/* Create App shared memory in kernel space */
	ret = rtds_memory_open_kernel_shared_apmem(rtds_memory_open_mem);
	if (SMAP_OK != ret) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_open_kernel" \
			"_shared_apmem failed ret[%d]\n", ret);
	} else {
		MSG_MED("[RTDSK]   |app_addr[0x%08X]\n",
			rtds_memory_open_mem->app_addr);
		MSG_MED("[RTDSK]   |pages[0x%08X]\n",
			(unsigned int)rtds_memory_open_mem->pages);
	}

	MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(rtds_memory_drv_open_apmem);

/*****************************************************************************
 * Function   : rtds_memory_drv_close_apmem
 * Description: This function destroys App shared memory.
 * Parameters : rtds_memory_close_mem - App shared memory destroy info
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 *				SMAP_MEMORY		- No memory
 *
 ****************************************************************************/
int rtds_memory_drv_close_apmem(
	rtds_memory_drv_close_mem_param	*rtds_memory_close_mem
)
{
	int ret = SMAP_PARA_NG;
	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);
	MSG_MED("[RTDSK]   |app_addr[0x%08X]\n",
		rtds_memory_close_mem->app_addr);
	MSG_MED("[RTDSK]   |pages[0x%08X]\n",
		(unsigned int)rtds_memory_close_mem->pages);

	/* Paremeter check */
	if (NULL == rtds_memory_close_mem) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_close_mem is NULL\n");
		MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);
		return ret;
	}

	if (0 == rtds_memory_close_mem->app_addr) {
		MSG_ERROR("[RTDSK]ERR| app_addr is 0\n");
		MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);
		return ret;
	}

	if (NULL == rtds_memory_close_mem->pages) {
		MSG_ERROR("[RTDSK]ERR| pages is NULL\n");
		MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);
		return ret;
	}

	/* Destroy App shared memory in kernel space */
	ret = rtds_memory_close_kernel_shared_apmem(rtds_memory_close_mem);
	if (SMAP_OK != ret) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_close_kernel_" \
			"shared_apmem failed ret[%d]\n", ret);
	}

	MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(rtds_memory_drv_close_apmem);

/*****************************************************************************
 * Function   : rtds_memory_drv_flush_cache
 * Description: This function flushes the cache.
 * Parameters : addr			- Flush address
 *				size			- Flush size
 * Returns	  : None
 ****************************************************************************/
void rtds_memory_drv_flush_cache(
	 unsigned int	addr,
	 unsigned int	size
)
{
	MSG_MED("[RTDSK]IN |[%s]\n", __func__);
	MSG_MED("[RTDSK]   |addr[0x%08X]\n", addr);
	MSG_MED("[RTDSK]   |size[%d]\n", size);

	/* Paremeter check */
	if (0 == addr) {
		MSG_ERROR("[RTDSK]ERR| addr is 0\n");
		MSG_MED("[RTDSK]OUT|[%s]\n", __func__);
		return;
	}

	if (0 == size) {
		MSG_ERROR("[RTDSK]ERR| size is 0\n");
		MSG_MED("[RTDSK]OUT|[%s]\n", __func__);
		return;
	}

	/* Flushing a L1 cache in the specified range. */
	dmac_flush_range((const void *)addr,
			(const void *)(addr + size));

	if (RTDS_MEM_FLUSH_CACHE_SIZE < size) {
		/* Flushing all the cache. */
		rtds_memory_flush_cache_all();
	} else {
		/* Flushing a L2 cache in the specified range. */
		rtds_memory_flush_l2cache(addr, addr + size);
	}

	MSG_MED("[RTDSK]OUT|[%s]\n", __func__);
	return;
}
EXPORT_SYMBOL(rtds_memory_drv_flush_cache);

/*****************************************************************************
 * Function   : rtds_memory_drv_inv_cache
 * Description: This function clears the cache.
 * Parameters : addr			- Clear address
 *				size			- Clear size
 * Returns	  : None
 ****************************************************************************/
void rtds_memory_drv_inv_cache(
	 unsigned int	addr,
	 unsigned int	size
)
{
	MSG_MED("[RTDSK]IN |[%s]\n", __func__);
	MSG_MED("[RTDSK]   |addr[0x%08X]\n", addr);
	MSG_MED("[RTDSK]   |size[%d]\n", size);

	/* Paremeter check */
	if (0 == addr) {
		MSG_ERROR("[RTDSK]ERR| addr is 0\n");
		MSG_MED("[RTDSK]OUT|[%s]\n", __func__);
		return;
	}

	if (0 == size) {
		MSG_ERROR("[RTDSK]ERR| size is 0\n");
		MSG_MED("[RTDSK]OUT|[%s]\n", __func__);
		return;
	}

	/* Clearing a L1 cache in the specified range. */
	dmac_map_area((const void *)addr, size, DMA_FROM_DEVICE);

	if (RTDS_MEM_FLUSH_CACHE_SIZE < size) {
		/* Clearing all the cache. */
		rtds_memory_flush_cache_all();
	} else {
		/* Clearing a L2 cache in the specified range. */
		rtds_memory_inv_l2cache(addr, addr + size);
	}

	MSG_MED("[RTDSK]OUT|[%s]\n", __func__);
	return;
}
EXPORT_SYMBOL(rtds_memory_drv_inv_cache);

/*****************************************************************************
 * Function   : rtds_memory_drv_change_address
 * Description: This function changes logical address
 *				between App doamin and RT domain.
 * Parameters : rtds_memory_drv_change_addr - logical address change informaion
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 *
 ****************************************************************************/
int rtds_memory_drv_change_address(
	rtds_memory_drv_change_addr_param	*rtds_memory_drv_change_addr
)
{
	int			ret = SMAP_PARA_NG;
	unsigned int		top_addr;
	unsigned int		bottom_addr;
	rtds_memory_drv_handle	*handle;
	unsigned int		org_addr;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);
	MSG_MED("[RTDSK]   |handle [0x%08X]\n",
		(unsigned int)rtds_memory_drv_change_addr->handle);
	MSG_MED("[RTDSK]   |addr_type [0x%08X]\n",
		rtds_memory_drv_change_addr->addr_type);

	/* Parameter check */
	if (NULL == rtds_memory_drv_change_addr) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_drv_change_addr is NULL\n");
		MSG_HIGH("[RTDSK]OUT|[%s]ret = %d\n", __func__, ret);
		return ret;
	}

	if (NULL == rtds_memory_drv_change_addr->handle) {
		MSG_ERROR("[RTDSK]ERR|" \
			" rtds_memory_drv_change_addr->handle is NULL\n");
		MSG_HIGH("[RTDSK]OUT|[%s]ret = %d\n", __func__, ret);
		return ret;
	}

	org_addr	= rtds_memory_drv_change_addr->org_addr;
	handle		= rtds_memory_drv_change_addr->handle;
	MSG_MED("[RTDSK]   |org_addr   [0x%08X]\n",
		rtds_memory_drv_change_addr->org_addr);
	MSG_LOW("[RTDSK]   |handle->var_kernel_addr [0x%08X]\n",
		(unsigned int)handle->var_kernel_addr);
	MSG_LOW("[RTDSK]   |handle->var_rt_addr_nc [0x%08X]\n",
		(unsigned int)handle->var_rt_addr_nc);


	switch (rtds_memory_drv_change_addr->addr_type) {
	case RTDS_MEM_DRV_APP_MEM:

		MSG_LOW("[RTDSK]   |RTDS_MEM_DRV_APP_MEM\n");

		/* Check address range */
		top_addr = (unsigned int)handle->var_kernel_addr;
		bottom_addr = (unsigned int)handle->var_kernel_addr
				+ handle->var_addr_size - 1;

		if ((org_addr < top_addr) || (org_addr > bottom_addr)) {
			MSG_ERROR("[RTDSK]ERR| Address is out of range.\n");
		} else {

		/* Change logical address from App domain to RT domain */
			rtds_memory_drv_change_addr->chg_addr = (unsigned int)handle->var_rt_addr_nc + (org_addr - top_addr);
			MSG_MED("[RTDSK]   |[App->RT]chg_addr[0x%08X]\n",
				rtds_memory_drv_change_addr->chg_addr);
			ret = SMAP_OK;
		}
		break;

	case RTDS_MEM_DRV_RT_MEM_NC:

		MSG_LOW("[RTDSK]   |RTDS_MEM_DRV_RT_MEM_NC\n");

		/* Check address range */
		top_addr	= (unsigned int)handle->var_rt_addr_nc;
		bottom_addr	= (unsigned int)handle->var_rt_addr_nc +
					handle->var_addr_size - 1;

		if ((org_addr < top_addr) || (org_addr > bottom_addr)) {
			MSG_ERROR("[RTDSK]ERR| Address is out of range.\n");
		} else {
		/* Change logical address from RT domain tp App domain */
			rtds_memory_drv_change_addr->chg_addr = (unsigned int)handle->var_kernel_addr + (org_addr - top_addr);
			MSG_MED("[RTDSK]   |[RT->App]chg_addr[0x%08X]\n",
					 rtds_memory_drv_change_addr->chg_addr);
			ret = SMAP_OK;
		}
		break;

	default:
		MSG_ERROR("[RTDSK]ERR| address type is illegal.\n");
		break;
	}

	MSG_HIGH("[RTDSK]OUT|[%s]ret = %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(rtds_memory_drv_change_address);

/*****************************************************************************
 * Function   : rtds_memory_drv_share_apmem
 * Description: This function shares App shared memory.
 * Parameters : rtds_memory_share_mem - App shared memory share info
 * Returns	: SMAP_OK		- Success
 *			  SMAP_PARA_NG	- Parameter Error
 *			  SMAP_MEMORY	- No memory
 *
 ****************************************************************************/
int rtds_memory_drv_share_apmem(
	rtds_memory_drv_share_mem_param	 *rtds_memory_share_mem
)
{
	int ret = SMAP_PARA_NG;
	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);
	MSG_MED("[RTDSK]   |apmem_id[0x%08X]\n",
		rtds_memory_share_mem->apmem_id);

	/* Paremeter check */
	if (NULL == rtds_memory_share_mem) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_share_mem is NULL\n");
		MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);
		return ret;
	}

	if (0 == rtds_memory_share_mem->apmem_id) {
		MSG_ERROR("[RTDSK]ERR| apmem_id is 0\n");
		MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);
		return ret;
	}

	/* Share App shared memory in kernel space */
	ret = rtds_memory_share_kernel_shared_apmem(rtds_memory_share_mem);
	if (SMAP_OK != ret) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_share_" \
			"kernel_shared_apmem failed ret[%d]\n", ret);
	}

	MSG_HIGH("[RTDSK]OUT|[%s] ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(rtds_memory_drv_share_apmem);

/*****************************************************************************
 * Function   : rtds_memory_create_handle
 * Description: This function allocates RTDS MEMORY driver handle region.
 * Parameters : None
 * Returns	  : RTDS MEMORY driver handle address
 *
 ****************************************************************************/
rtds_memory_drv_handle *rtds_memory_create_handle(
		void
)
{
	rtds_memory_drv_handle *handle = NULL; /* RTDS MEMORY handle */

	MSG_MED("[RTDSK]IN |[%s]\n", __func__);

	/* Allocate handle region from kernel memory */
	handle = kmalloc(sizeof(*handle), GFP_KERNEL);

	if (NULL == handle) {
		MSG_ERROR("[RTDSK]ERR| kmalloc() failed\n");
	}

	MSG_MED("[RTDSK]OUT|[%s] handle[0x%08x]\n",
		__func__, (unsigned int)handle);

	return handle;
}
EXPORT_SYMBOL(rtds_memory_create_handle);

/*****************************************************************************
 * Function   : rtds_memory_destroy_handle
 * Description: This function releases RTDS MEMORY driver handle region.
 * Parameters : handle  - RTDS MEMORY driver handle informaion
 * Returns	  : none
 *
 ****************************************************************************/
void rtds_memory_destroy_handle(
		rtds_memory_drv_handle	 *handle
)
{
	MSG_MED("[RTDSK]IN |[%s]\n", __func__);

	if (NULL == handle) {
		MSG_ERROR("[RTDSK]handle is NULL.\n");
	} else {
		/* Release the allocated handle region from kernel memory */
		kfree(handle);
	}
	MSG_MED("[RTDSK]OUT|[%s]\n", __func__);

	return;
}
EXPORT_SYMBOL(rtds_memory_destroy_handle);



/*****************************************************************************
 * Function   : rtds_memory_drv_map
 * Description: This function gives a map demand to Mpro.
 * Parameters : rtds_memory_map  - Physical continuation domain map information
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 *				SMAP_MEMORY		- No memory
 ****************************************************************************/
int rtds_memory_drv_map(
	rtds_memory_drv_map_param	*rtds_memory_map
)
{
	int ret_code = SMAP_PARA_NG;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	/* Parameter check */
	if (NULL == rtds_memory_map) {
		MSG_ERROR("[RTDSK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	if ((NULL == rtds_memory_map->handle) ||
		(0 == rtds_memory_map->phy_addr) ||
		(0 == rtds_memory_map->mem_size)) {
		MSG_ERROR("[RTDSK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	/* Map to RTDomain logic space */
	ret_code = rtds_memory_map_mpro(rtds_memory_map->phy_addr,
					rtds_memory_map->mem_size,
			(unsigned long *)&rtds_memory_map->app_addr);

	MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_map);



/*****************************************************************************
 * Function   : rtds_memory_drv_unmap
 * Description: This function gives a unmap demand to Mpro.
 * Parameters : rtds_memory_unmap  - Physical continuation domain unmap
 *				information
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 *				SMAP_MEMORY		- No memory
 ****************************************************************************/
int rtds_memory_drv_unmap(
	rtds_memory_drv_unmap_param	*rtds_memory_unmap
)
{
	int ret_code = SMAP_PARA_NG;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	/* Parameter check */
	if (NULL == rtds_memory_unmap) {
		MSG_ERROR("[RTDSK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	if ((NULL == rtds_memory_unmap->handle) ||
		(0 == rtds_memory_unmap->mem_size) ||
		(0 == rtds_memory_unmap->app_addr)) {
		MSG_ERROR("[RTDSK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	/* UNMap to RTDomain logic space */
	ret_code = rtds_memory_unmap_mpro(rtds_memory_unmap->app_addr,
					rtds_memory_unmap->mem_size);

	MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_unmap);



/*****************************************************************************
 * Function   : rtds_memory_drv_map_pnc
 * Description: This function gives a map demand to Mpro.
 * Parameters : rtds_memory_map_pnc  - Physical non-continuation
 *				domain map info
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 *				SMAP_MEMORY		- No memory
 *				Others			- System error/RT result
 ****************************************************************************/
int rtds_memory_drv_map_pnc(
	 rtds_memory_drv_map_pnc_param	*rtds_memory_map_pnc
)
{
	int		ret_code = SMAP_PARA_NG;
	unsigned int	cache_check;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	/* Parameter check */
	if (NULL == rtds_memory_map_pnc) {
		MSG_ERROR("[RTDSK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	cache_check = (RT_MEMORY_RTMAP_WB | RT_MEMORY_RTMAP_WBNC);
	if ((NULL == rtds_memory_map_pnc->handle) ||
		(0 == rtds_memory_map_pnc->app_addr) ||
		(0 == rtds_memory_map_pnc->map_size) ||
		(NULL == rtds_memory_map_pnc->pages) ||
		(0 != (~cache_check & rtds_memory_map_pnc->rtcache_kind))
	) {
		MSG_ERROR("[RTDSK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	/* Map to RTDomain logic space */
	ret_code = rtds_memory_map_pnc_mpro(rtds_memory_map_pnc->app_addr,
					    rtds_memory_map_pnc->map_size,
					    rtds_memory_map_pnc->pages,
					    rtds_memory_map_pnc->rtcache_kind,
		(rtds_memory_drv_app_mem_info *)&rtds_memory_map_pnc->mem_info);

	MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_map_pnc);



/*****************************************************************************
 * Function   : rtds_memory_drv_unmap_pnc
 * Description: This function gives a unmap demand to Mpro.
 * Parameters : rtds_memory_unmap_pnc  - Physical non-continuation
 *				domain unmap info
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 *				SMAP_MEMORY		- No memory
 *				Others			- System error/RT result
 ****************************************************************************/
int rtds_memory_drv_unmap_pnc(
	 rtds_memory_drv_unmap_pnc_param	*rtds_memory_unmap_pnc
)
{
	int ret_code = SMAP_PARA_NG;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	/* Parameter check */
	if (NULL == rtds_memory_unmap_pnc) {
		MSG_ERROR("[RTDSK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	if ((NULL == rtds_memory_unmap_pnc->handle) ||
		(0 == rtds_memory_unmap_pnc->app_addr) ||
		(0 == rtds_memory_unmap_pnc->apmem_id)
	) {
		MSG_ERROR("[RTDSK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	/* UNMap to RTDomain logic space */
	ret_code = rtds_memory_unmap_pnc_mpro(rtds_memory_unmap_pnc->app_addr,
					      rtds_memory_unmap_pnc->apmem_id);

	MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_unmap_pnc);

/*****************************************************************************
 * Function   : rtds_memory_drv_reg_phymem
 * Description: This function give a translation information.
 * Parameters : rtds_memory_map   -   Physical continuation
 *				domain map information
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 ****************************************************************************/
int rtds_memory_drv_reg_phymem(
	rtds_memory_drv_map_param *rtds_memory_map
)
{
	int ret_code = SMAP_PARA_NG;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	if (NULL == rtds_memory_map) {
		MSG_ERROR("[RTDSK]ERR|[%s]rtds_memory_map NULL\n", __func__);
		MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if ((NULL == rtds_memory_map->handle) ||
		(0 == rtds_memory_map->phy_addr) ||
		(0 == rtds_memory_map->mem_size)) {
		MSG_ERROR("[RTDSK]ERR|[%s]rtds_memory_map->handle   = 0x%08X\n",
			__func__, (u32)rtds_memory_map->handle);
		MSG_ERROR("[RTDSK]ERR|[%s]rtds_memory_map->phy_addr = 0x%08X\n",
			__func__, rtds_memory_map->phy_addr);
		MSG_ERROR("[RTDSK]ERR|[%s]rtds_memory_map->mem_size = 0x%08X\n",
			__func__, rtds_memory_map->mem_size);
		MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	ret_code = rtds_memory_reg_kernel_phymem(
						rtds_memory_map->phy_addr,
						rtds_memory_map->mem_size,
						rtds_memory_map->app_addr);

	MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_reg_phymem);

/*****************************************************************************
 * Function   : rtds_memory_drv_unreg_phymem
 * Description: This function clear translation information.
 * Parameters : rtds_memory_map   -   Physical continuation
 *				domain map information
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 ****************************************************************************/
int rtds_memory_drv_unreg_phymem(
	rtds_memory_drv_map_param *rtds_memory_map
)
{
	int ret_code = SMAP_PARA_NG;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	if (NULL == rtds_memory_map) {
		MSG_ERROR("[RTDSK]ERR|[%s]rtds_memory_map NULL\n", __func__);
		MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if ((NULL == rtds_memory_map->handle) ||
		(0 == rtds_memory_map->phy_addr) ||
		(0 == rtds_memory_map->mem_size)) {
		MSG_ERROR("[RTDSK]ERR|[%s]rtds_memory_map->handle   = 0x%08X\n",
			__func__, (u32)rtds_memory_map->handle);
		MSG_ERROR("[RTDSK]ERR|[%s]rtds_memory_map->phy_addr = 0x%08X\n",
			__func__, rtds_memory_map->phy_addr);
		MSG_ERROR("[RTDSK]ERR|[%s]rtds_memory_map->mem_size = 0x%08X\n",
			__func__, rtds_memory_map->mem_size);
		MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	ret_code = rtds_memory_unreg_kernel_phymem(
						rtds_memory_map->phy_addr,
						rtds_memory_map->mem_size,
						rtds_memory_map->app_addr);

	MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_unreg_phymem);

/*****************************************************************************
 * Function   : rtds_memory_drv_phy_change_address
 * Description: This function give logical address
 * Parameters : phy_change_rtaddr   -   Address change information
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 ****************************************************************************/
int rtds_memory_drv_phy_change_address(
	rtds_memory_drv_change_addr_param *phy_change_rtaddr
)
{
	int ret_code = SMAP_PARA_NG;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	if (NULL == phy_change_rtaddr) {
		MSG_ERROR("[RTDSK]ERR|[%s]phy_change_rtaddr NULL\n", __func__);
		MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if ((NULL == phy_change_rtaddr->handle) ||
		(0 == phy_change_rtaddr->org_addr)) {
		MSG_ERROR("[RTDSK]ERR|[%s]phy_change_rtaddr->handle = 0x%08X\n",
			__func__, (u32)phy_change_rtaddr->handle);
		MSG_ERROR("[RTDSK]ERR|" \
			"[%s]phy_change_rtaddr->org_addr = 0x%08X\n",
			__func__, phy_change_rtaddr->org_addr);
		MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	ret_code = rtds_memory_change_kernel_phymem_address(
						phy_change_rtaddr->org_addr,
				(unsigned long *)&phy_change_rtaddr->chg_addr);

	MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_phy_change_address);

/*****************************************************************************
 * Function   : rtds_memory_drv_rtpmb_to_phy_address
 * Description: This function give physical address
 * Parameters : phy_change_rtaddr   -   Address change information
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 ****************************************************************************/
int rtds_memory_drv_rtpmb_to_phy_address(
	rtds_memory_drv_change_addr_param *phy_change_rtaddr
)
{
	int ret_code = SMAP_PARA_NG;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	if (NULL == phy_change_rtaddr) {
		MSG_ERROR("[RTDSK]ERR|[%s]phy_change_rtaddr NULL\n", __func__);
		MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (0 == phy_change_rtaddr->org_addr) {
		MSG_ERROR("[RTDSK]ERR|[%s]" \
			"phy_change_rtaddr->org_addr = 0x%08X\n",
			__func__, phy_change_rtaddr->org_addr);
		MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	ret_code = rtds_memory_change_rtpmb_to_phy_address(
						phy_change_rtaddr->org_addr,
				(unsigned long *)&phy_change_rtaddr->chg_addr);

	MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_rtpmb_to_phy_address);

/*****************************************************************************
 * Function   : rtds_memory_drv_phy_to_rtpmb_address
 * Description: This function give RT-logical(PMB) address
 * Parameters : phy_change_rtaddr   -   Address change information
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 ****************************************************************************/
int rtds_memory_drv_phy_to_rtpmb_address(
	rtds_memory_drv_change_addr_param *phy_change_rtaddr
)
{
	int ret_code = SMAP_PARA_NG;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	if (NULL == phy_change_rtaddr) {
		MSG_ERROR("[RTDSK]ERR|[%s]phy_change_rtaddr NULL\n", __func__);
		MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (0 == phy_change_rtaddr->org_addr) {
		MSG_ERROR("[RTDSK]ERR|[%s]" \
			"phy_change_rtaddr->org_addr = 0x%08X\n",
			__func__, phy_change_rtaddr->org_addr);
		MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	ret_code = rtds_memory_change_phy_to_rtpmb_address(
						phy_change_rtaddr->org_addr,
				(unsigned long *)&phy_change_rtaddr->chg_addr);

	MSG_HIGH("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_phy_to_rtpmb_address);

/*****************************************************************************
 * Function   : rtds_memory_drv_rtpmb_cache_address
 * Description: This function give rtpmb cache address
 * Parameters : phy_change_rtaddr   -   Address change information
 * Returns	  : SMAP_OK			- Success
 *				SMAP_PARA_NG	- Parameter Error
 ****************************************************************************/
int rtds_memory_drv_rtpmb_cache_address(
	rtds_memory_drv_change_addr_param *phy_change_rtaddr
)
{
	int ret_code = SMAP_PARA_NG;

	MSG_MED("[RTDSK]IN |[%s]\n", __func__);

	if (NULL == phy_change_rtaddr) {
		MSG_ERROR("[RTDSK]ERR|[%s]phy_change_rtaddr NULL\n", __func__);
		MSG_MED("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	if (0 == phy_change_rtaddr->org_addr) {
		MSG_ERROR("[RTDSK]ERR|[%s]" \
			"phy_change_rtaddr->org_addr = 0x%08X\n",
			__func__, phy_change_rtaddr->org_addr);
		MSG_MED("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
		return ret_code;
	}

	ret_code = rtds_memory_change_rtpmb_cache_address(
						phy_change_rtaddr->org_addr,
				(unsigned long *)&phy_change_rtaddr->chg_addr);

	MSG_MED("[RTDSK]OUT|[%s] ret_code = %d\n", __func__, ret_code);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_rtpmb_cache_address);

/*****************************************************************************
 * Function   : rtds_memory_drv_map_pnc_nma
 * Description: This function gives a map demand to Mpro.
 * Parameters : rtds_memory_map_pnc_nma  - Physical non-continuation
 *				domain map info
 * Returns    : SMAP_OK		- Success
 *		SMAP_PARA_NG	- Parameter Error
 *		SMAP_MEMORY	- No memory
 ****************************************************************************/
int rtds_memory_drv_map_pnc_nma(
	 rtds_memory_drv_map_pnc_nma_param	*rtds_memory_map_pnc_nma
)
{
	int		ret_code = SMAP_PARA_NG;
	struct page	**k_pages;
	unsigned long	size;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	/* Parameter check */
	if (NULL == rtds_memory_map_pnc_nma) {
		MSG_ERROR("[RTDSK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	if ((NULL == rtds_memory_map_pnc_nma->handle) ||
		(0 == rtds_memory_map_pnc_nma->map_size)  ||
		(NULL == rtds_memory_map_pnc_nma->pages)
	) {
		MSG_ERROR("[RTDSK]ERR|[%s][%d]\n", __func__, __LINE__);
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	size = sizeof(struct page *)
		* (rtds_memory_map_pnc_nma->map_size / PAGE_SIZE);
	k_pages = vmalloc(size);
	if (NULL == k_pages) {
		MSG_ERROR("[RTDSK]ERR| vmalloc failed.\n");
		ret_code = SMAP_MEMORY;
		MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
		return ret_code;
	}

	memcpy(k_pages, rtds_memory_map_pnc_nma->pages, size);

	/* Map to RTDomain logic space */
	ret_code = rtds_memory_map_pnc_nma_mpro(
					rtds_memory_map_pnc_nma->map_size,
					k_pages,
			(unsigned int *)&rtds_memory_map_pnc_nma->rt_addr_wb);

	MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
	return ret_code;
}
EXPORT_SYMBOL(rtds_memory_drv_map_pnc_nma);

void rtds_memory_drv_dump_mpro(
	void
)
{
	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);
	rtds_memory_dump_notify();
	MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);
}
EXPORT_SYMBOL(rtds_memory_drv_dump_mpro);
