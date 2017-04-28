/*
 * Function        : share memory management for SH Mobile
 *
 * Copyright (C) 2011-2013 Renesas Electronics Corporation
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
 * Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>

#include <media/sh_mobile_appmem.h>
#include <rtapi/system_memory.h>

#define DEV_NAME      "composer"

#define DEBUG_RTAPI_ARGUMENT 1

/******************************************************/
/* define local variables for App Memory handling     */
/******************************************************/
static int         app_share_initialize;
static spinlock_t  app_share_lock;
static LIST_HEAD(app_share_top);
static int         debug;
static LIST_HEAD(rt_physaddr_top);

#define printk_lowdbg(fmt, arg...) \
	printk(KERN_INFO DEV_NAME ": %s: " fmt, \
				__func__, ## arg);

#define printk_dbg(level, fmt, arg...) \
	do { \
		if (level <= debug) \
			printk_lowdbg(fmt, ## arg); \
	} while (0)

#define printk_err(fmt, arg...) \
	do { \
		printk(KERN_ERR DEV_NAME ":E %s: " fmt, __func__, ## arg); \
	} while (0)

/**************************************************/
/* implementation for dump arguments of RT-API.   */
/**************************************************/
#if DEBUG_RTAPI_ARGUMENT
static const char *get_RTAPImsg_memory(int rc)
{
	const char *msg = "unknown RT-API error";
	switch (rc) {
	case SMAP_LIB_MEMORY_OK:
		msg = "SMAP_LIB_MEMORY_OK";
		break;
	case SMAP_LIB_MEMORY_NG:
		msg = "SMAP_LIB_MEMORY_NG";
		break;
	case SMAP_LIB_MEMORY_PARA_NG:
		msg = "SMAP_LIB_MEMORY_PARA_NG";
		break;
	case SMAP_LIB_MEMORY_NO_MEMORY:
		msg = "SMAP_LIB_MEMORY_NO_MEMORY";
		break;
	}
	return msg;
}
#else
#define get_RTAPImsg_memory(X) ""
#endif

#if DEBUG_RTAPI_ARGUMENT
static void dump_system_memory_ap_open(system_mem_ap_open *op)
{
	printk_lowdbg("system_memory_ap_open " \
		"handle:%p aparea_size:0%x cache_kind:%d\n",
		op->handle, op->aparea_size, op->cache_kind);
}

static void dump_system_memory_rt_map_pnc(system_mem_rt_map_pnc *mpnc)
{
	printk_lowdbg("system_memory_rt_map_pnc " \
		"handle:%p apaddr:0%x map_size:0x%x " \
		"pages:%p rtcache_kind:%d\n",
		mpnc->handle, mpnc->apaddr, mpnc->map_size,
		mpnc->pages, mpnc->rtcache_kind);
}

static void dump_system_memory_ap_close(system_mem_ap_close *clo)
{
	printk_lowdbg("system_memory_ap_close " \
		"handle:%p apaddr:0%x pages:%p\n",
		clo->handle, clo->apaddr, clo->pages);
}

static void dump_system_memory_ap_alloc(system_mem_ap_alloc *aloc)
{
	printk_lowdbg("system_memory_ap_alloc " \
		"handle:%p apaddr:0%x apmem_handle:%p\n",
		aloc->handle, aloc->alloc_size, aloc->apmem_handle);
}

static void dump_system_memory_ap_share_mem_offset(
	system_mem_ap_share_mem_offset * ofs)
{
	printk_lowdbg("system_memory_ap_share_mem_offset " \
		"handle:%p apmem_handle:%p apmem_apaddr:0x%x\n",
		ofs->handle, ofs->apmem_handle, ofs->apmem_apaddr);
}

static void dump_system_memory_ap_change_rtaddr(
	system_mem_ap_change_rtaddr * adr)
{
	printk_lowdbg("system_memory_ap_change_rtaddr " \
		"handle:%p cache_kind:%d, " \
		"apmem_handle:%p apmem_apaddr:0x%x\n",
		adr->handle, adr->cache_kind,
		adr->apmem_handle, adr->apmem_apaddr);
}

static void dump_system_memory_ap_free(system_mem_ap_free *fre)
{
	printk_lowdbg("system_memory_ap_free " \
		"handle:%p apmem_handle:%p apmem_apaddr:0x%x\n",
		fre->handle, fre->apmem_handle, fre->apmem_apaddr);
}

static void dump_system_memory_rt_unmap_pnc(system_mem_rt_unmap_pnc *upnc)
{
	printk_lowdbg("system_memory_rt_unmap_pnc " \
		"handle:%p apmem_handle:%p\n",
		upnc->handle, upnc->apmem_handle);
}

static void dump_system_memory_ap_share_area(system_mem_ap_share_area *area)
{
	printk_lowdbg("system_memory_ap_share_area " \
		"handle:%p apmem_id:%d\n",
		area->handle, area->apmem_id);
}

static void dump_system_memory_ap_share_mem(system_mem_ap_share_mem *mem)
{
	printk_lowdbg("system_memory_ap_share_mem " \
		"handle:%p apmem_handle:%p\n",
		mem->handle, mem->apmem_handle);
}

static void dump_system_memory_rt_map(system_mem_rt_map *map)
{
	printk_lowdbg("system_memory_rt_map " \
		"handle:%p phys_addr:0x%x map_size:0x%x\n",
		map->handle, map->phys_addr, map->map_size);
}

static void dump_system_memory_rt_unmap(system_mem_rt_unmap *umap)
{
	printk_lowdbg("system_memory_rt_unmap " \
		"handle:%p rtaddr:0x%x map_size:0x%x\n",
		umap->handle, umap->rtaddr, umap->map_size);
}
#endif /* DEBUG_RTAPI_ARGUMENT */

/**************************************************/
/* implementation for app memory handling         */
/**************************************************/

/**************************************************/
/* name : sh_mobile_appmem_dump_appshare_list     */
/* function: dump list of app share.              */
/**************************************************/
static void sh_mobile_appmem_dump_appshare_list(void)
{
	unsigned long flags;
	struct list_head *list;

	printk_dbg(1, "list currently registered appshare.\n");
	spin_lock_irqsave(&app_share_lock, flags);

	list_for_each(list, &app_share_top)
	{
		struct appmem_handle *mem = NULL;

		mem = list_entry((void *)list, struct appmem_handle, list);

		printk_dbg(1, "  memhandle:%p app_id:0x%x offset:0x%x key:%s" \
			"size:0x%x vaddr:%p rtaddr:0x%lx ref_count:%d " \
			"apaddr:0x%x pages:%p\n",
			mem->memhandle, mem->app_id, mem->offset, mem->key,
			mem->size, mem->vaddr, mem->rtaddr, mem->ref_count,
			mem->op_apaddr, mem->op_pages);
	}

	spin_unlock_irqrestore(&app_share_lock, flags);
}

/**************************************************/
/* name : sh_mobile_appmem__init                  */
/* function: initialize local variable            */
/**************************************************/
static void sh_mobile_appmem__init(void)
{
	if (app_share_initialize == false) {
		app_share_initialize = true;
		spin_lock_init(&app_share_lock);
	}
}

/***************************************************/
/* name : sh_mobile_appmem_alloc                   */
/* function: allocate app share memory in kernel   */
/* return: handle of app share memory information. */
/*         NULL if found error.                    */
/***************************************************/
struct appmem_handle *sh_mobile_appmem_alloc(int size, char *key)
{
	struct appmem_handle *mem = NULL;
	void *handle = NULL;
	void *memhandle = NULL;
	unsigned char *vadr = NULL;
	unsigned int offset, rt_adr;
	int  rc;
	unsigned int op_apaddr = 0;
	struct page **op_pages = NULL;

	sh_mobile_appmem__init();

	printk_dbg(1, "size:0x%x key:%s\n", size, key);

	handle = system_memory_info_new();
	if (handle == NULL) {
		printk_err("system_memory_info_new error\n");
		goto err_exit;
	}

	{
		system_mem_ap_open  op;
		system_mem_rt_map_pnc  mpnc;

		op.handle      = handle;
		op.aparea_size = RT_MEMORY_APAREA_SIZE(
			RT_MEMORY_APMEM_SIZE(size));
		op.cache_kind  = RT_MEMORY_NONCACHE;
		rc = system_memory_ap_open(&op);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_open " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_open(&op);
#endif
			goto err_exit;
		}

		mpnc.handle       = handle;
		mpnc.apaddr       = op.apaddr;
		mpnc.map_size     = op.aparea_size;
		mpnc.pages        = op.pages;
		mpnc.rtcache_kind = RT_MEMORY_RTMAP_WBNC;

		rc = system_memory_rt_map_pnc(&mpnc);
		if (rc != SMAP_LIB_MEMORY_OK) {
			system_mem_ap_close clo;

			printk_err("system_memory_rt_map_pnc " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_rt_map_pnc(&mpnc);
#endif

			clo.handle = handle;
			clo.apaddr = op.apaddr;
			clo.pages  = op.pages;
			rc = system_memory_ap_close(&clo);
			if (rc != SMAP_LIB_MEMORY_OK) {
				printk_err("system_memory_ap_close " \
					"error: return by %d %s\n", rc,
					get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
				dump_system_memory_ap_close(&clo);
#endif
			}
			goto err_exit;
		}

		op_apaddr = op.apaddr;
		op_pages  = op.pages;
		memhandle = mpnc.apmem_handle;
	}

	{
		system_mem_ap_alloc  aloc;
		aloc.handle       = handle;
		aloc.alloc_size   = size;
		aloc.apmem_handle = memhandle;
		rc = system_memory_ap_alloc(&aloc);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_alloc " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_alloc(&aloc);
#endif
			goto err_exit;
		}
		vadr = (unsigned char *)aloc.apmem_apaddr;
	}

	{
		system_mem_ap_share_mem_offset apmem_ofs;

		apmem_ofs.handle       = handle;
		apmem_ofs.apmem_handle = memhandle;
		apmem_ofs.apmem_apaddr = (unsigned int)vadr;
		rc = system_memory_ap_share_mem_offset(&apmem_ofs);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_share_mem_offset " \
			"error: return by %d %s\n", rc,
			get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_share_mem_offset(&apmem_ofs);
#endif
			goto err_exit;
		}
		offset = apmem_ofs.apmem_offset;
	}

	{
		system_mem_ap_change_rtaddr adr;

		adr.handle       = handle;
		adr.cache_kind   = RT_MEMORY_WRITEBACK;
		adr.apmem_handle = memhandle;
		adr.apmem_apaddr = (unsigned int)vadr;
		rc = system_memory_ap_change_rtaddr(&adr);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_change_rtaddr " \
			"error: return by %d %s\n", rc,
			get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_change_rtaddr(&adr);
#endif
			goto err_exit;
		}
		rt_adr = adr.apmem_rtaddr;
	}

	mem = kmalloc(sizeof(*mem), GFP_KERNEL);

	if (mem == NULL) {
		printk_err("kmalloc error\n");
		goto err_exit;
	} else {
		system_mem_ap_get_apmem_id  apmem_id;
		unsigned long flags;

		apmem_id.handle       = handle;
		apmem_id.apmem_handle = memhandle;

		mem->memhandle = memhandle;
		mem->app_id    = system_memory_ap_get_apmem_id(&apmem_id);
		mem->offset    = offset;
		mem->key       = key;

		/* following information is reserved */
		INIT_LIST_HEAD(&mem->list);
		mem->size      = size;
		mem->vaddr     = vadr   - offset;
		mem->rtaddr    = rt_adr - offset;
		mem->ref_count = 1;

		mem->op_apaddr = op_apaddr;
		mem->op_pages  = op_pages;

		printk_dbg(2, "open appshare memory.\n");
		printk_dbg(2, "  memhandle:%p app_id:0x%x offset:0x%x key:%s" \
			"size:0x%x vaddr:%p rtaddr:0x%lx ref_count:%d " \
			"apaddr:0x%x pages:%p\n",
			mem->memhandle, mem->app_id, mem->offset, mem->key,
			mem->size, mem->vaddr, mem->rtaddr, mem->ref_count,
			mem->op_apaddr, mem->op_pages);

		spin_lock_irqsave(&app_share_lock, flags);
		list_add_tail(&mem->list, &app_share_top);
		spin_unlock_irqrestore(&app_share_lock, flags);
	}

err_exit:
	if (mem == NULL) {
		/* free resources */
		if (vadr) {
			system_mem_ap_free fre;

			fre.handle       = handle;
			fre.apmem_handle = memhandle;
			fre.apmem_apaddr = (unsigned int)vadr;
			rc = system_memory_ap_free(&fre);
			if (rc != SMAP_LIB_MEMORY_OK) {
				printk_err("system_memory_ap_free " \
					"error: return by %d %s\n", rc,
					get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
				dump_system_memory_ap_free(&fre);
#endif
			}
			vadr = NULL;
		}
		if (memhandle) {
			system_mem_rt_unmap_pnc upnc;
			system_mem_ap_close clo;

			upnc.handle       = handle;
			upnc.apmem_handle = memhandle;
			memhandle = NULL;
			rc = system_memory_rt_unmap_pnc(&upnc);
			if (rc != SMAP_LIB_MEMORY_OK) {
				printk_err("system_memory_rt_unmap_pnc " \
					"error: return by %d %s\n", rc,
					get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
				dump_system_memory_rt_unmap_pnc(&upnc);
#endif
			}

			clo.handle = handle;
			clo.apaddr = op_apaddr;
			clo.pages  = op_pages;
			op_apaddr = 0;
			op_pages  = NULL;
			rc = system_memory_ap_close(&clo);
			if (rc != SMAP_LIB_MEMORY_OK) {
				printk_err("system_memory_ap_close " \
					"error: return by %d %s\n", rc,
					get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
				dump_system_memory_ap_close(&clo);
#endif
			}
		}
	}
	if (handle) {
		system_mem_info_delete del;
		del.handle = handle;

		system_memory_info_delete(&del);
	}
	if (debug) {
		/* display debug information. */
		sh_mobile_appmem_dump_appshare_list();
	}

	return mem;
}
EXPORT_SYMBOL(sh_mobile_appmem_alloc);


/***************************************************/
/* name : sh_mobile_appmem_share                   */
/* function: mapping app share memory in kernel    */
/* return: handle of app share memory information. */
/*         NULL if found error.                    */
/***************************************************/
struct appmem_handle *sh_mobile_appmem_share(int appid, char *key)
{
	struct appmem_handle *mem = NULL;
	void *handle        = NULL;
	void *memhandle     = NULL;
	unsigned char *vadr = NULL;
	unsigned int rt_adr;
	int  rc;
	unsigned int op_apaddr = 0;
	struct page **op_pages = NULL;

	sh_mobile_appmem__init();

	printk_dbg(1, "appid:%d key:%s\n", appid, key);
	/* search already created. */

	{
		unsigned long flags;
		struct list_head *list;

		spin_lock_irqsave(&app_share_lock, flags);

		list_for_each(list, &app_share_top)
		{
			struct appmem_handle *tmp = NULL;

			tmp = list_entry((void *)list,
				struct appmem_handle, list);
			if (appid == tmp->app_id) {
				/* already handle created. */

				tmp->ref_count++;

				mem = tmp;
				break;
			}
		}
		spin_unlock_irqrestore(&app_share_lock, flags);

		if (mem != NULL) {
			/* app share already opend */
			printk_dbg(2, "app_id:0x%x already opend. mem:%p\n",
				appid, mem);
			goto normal_exit;
		}
	}

	handle = system_memory_info_new();
	if (handle == NULL) {
		printk_err("system_memory_info_new error\n");
		goto err_exit;
	}

	{
		system_mem_ap_share_area area;

		area.handle = handle;
		area.apmem_id = appid;
		rc = system_memory_ap_share_area(&area);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_share_area " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_share_area(&area);
#endif
			goto err_exit;
		}
		memhandle = area.apmem_handle;
		op_apaddr = area.apaddr;
		op_pages  = area.pages;
	}

	{
		system_mem_ap_share_mem  mem;

		mem.handle = handle;
		mem.apmem_handle = memhandle;
		mem.apmem_offset = 0;
		rc = system_memory_ap_share_mem(&mem);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_share_mem " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_share_mem(&mem);
#endif
			goto err_exit;
		}
		vadr = (unsigned char *)mem.apmem_apaddr;
	}

	{
		system_mem_ap_change_rtaddr adr;

		adr.handle = handle;
		adr.cache_kind = RT_MEMORY_NONCACHE;
		adr.apmem_handle = memhandle;
		adr.apmem_apaddr = (unsigned int)vadr;
		rc = system_memory_ap_change_rtaddr(&adr);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_change_rtaddr " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_change_rtaddr(&adr);
#endif
			goto err_exit;
		}
		rt_adr = adr.apmem_rtaddr;
	}


	mem = kmalloc(sizeof(*mem), GFP_KERNEL);

	if (mem == NULL) {
		printk_err("kmalloc error\n");
		goto err_exit;
	} else {
		unsigned long flags;

		mem->memhandle = memhandle;
		mem->app_id    = appid;
		mem->offset    = 0;
		mem->key       = key;

		/* following information is reserved */
		INIT_LIST_HEAD(&mem->list);
		mem->size      = 0;
		mem->vaddr     = vadr;
		mem->rtaddr    = rt_adr;
		mem->ref_count = 1;

		mem->op_apaddr = op_apaddr;
		mem->op_pages  = op_pages;

		printk_dbg(2, "share appshare memory.\n");
		printk_dbg(2, "  memhandle:%p app_id:0x%x offset:0x%x key:%s" \
			"size:0x%x vaddr:%p rtaddr:0x%lx ref_count:%d " \
			"apaddr:0x%x pages:%p\n",
			mem->memhandle, mem->app_id, mem->offset, mem->key,
			mem->size, mem->vaddr, mem->rtaddr, mem->ref_count,
			mem->op_apaddr, mem->op_pages);

		spin_lock_irqsave(&app_share_lock, flags);
		list_add_tail(&mem->list, &app_share_top);
		spin_unlock_irqrestore(&app_share_lock, flags);
	}

err_exit:
	if (mem == NULL) {
		/* free resources */
		if (memhandle) {
			system_mem_rt_unmap_pnc upnc;
			system_mem_ap_close clo;

			upnc.handle       = handle;
			upnc.apmem_handle = memhandle;
			memhandle = NULL;
			rc = system_memory_rt_unmap_pnc(&upnc);
			if (rc != SMAP_LIB_MEMORY_OK) {
				printk_err("system_memory_rt_unmap_pnc " \
					"error: return by %d %s\n", rc,
					get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
				dump_system_memory_rt_unmap_pnc(&upnc);
#endif
			}

			clo.handle = handle;
			clo.apaddr = op_apaddr;
			clo.pages  = op_pages;
			op_apaddr = 0;
			op_pages  = NULL;
			rc = system_memory_ap_close(&clo);
			if (rc != SMAP_LIB_MEMORY_OK) {
				printk_err("system_memory_ap_close " \
					"error: return by %d %s\n", rc,
					get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
				dump_system_memory_ap_close(&clo);
#endif
			}
		}
	}
	if (handle) {
		system_mem_info_delete     del;
		del.handle = handle;

		system_memory_info_delete(&del);
	}
normal_exit:
	if (debug) {
		/* display debug information. */
		sh_mobile_appmem_dump_appshare_list();
	}
	return mem;
}
EXPORT_SYMBOL(sh_mobile_appmem_share);


/***************************************************/
/* name : sh_mobile_appmem_getRTaddress            */
/* function: get RT address of offset.             */
/* return: virtual address to access app share.    */
/*         NULL if found error.                    */
/***************************************************/
unsigned long sh_mobile_appmem_getRTaddress(\
	struct appmem_handle *appmem, unsigned char *vadr)
{
	int rc = -1;
	system_mem_ap_change_rtaddr rtaddr;

	printk_dbg(1, "appmem:%p vadr:%p\n", appmem, vadr);

	if (appmem == NULL) {
		printk_err("appmem handle passed NULL.\n");
		goto err_exit;
	}

	if (vadr == NULL) {
		printk_err("appmem handle passed NULL.\n");
		goto err_exit;
	}

	if (appmem->size) {
		int offset = vadr - appmem->vaddr;
		int min = appmem->offset;
		int max = appmem->offset + appmem->size;
		/* appmem handle created by sh_mobile_appmem_alloc */
		if (offset < min || offset >= max) {
			printk_err("vadr %p out of valid range" \
				"(%p - %p)\n", vadr, \
				appmem->vaddr+min, appmem->vaddr+max);
			goto err_exit;
		}
		rtaddr.apmem_rtaddr = appmem->rtaddr + offset;
		rc = 0;
	} else {
		void *handle;
		system_mem_info_delete   del;

		handle = system_memory_info_new();
		if (handle == NULL) {
			printk_err("system_memory_info_new error\n");
			goto err_exit;
		}

		/* set handle */
		rtaddr.handle = handle;
		del.handle    = handle;

		/* set ap_change_rtaddr parameter */
		rtaddr.cache_kind   = RT_MEMORY_NONCACHE;
		rtaddr.apmem_handle = appmem->memhandle;
		rtaddr.apmem_apaddr = (unsigned long)vadr;
		rc = system_memory_ap_change_rtaddr(&rtaddr);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_change_rtaddr " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_change_rtaddr(&rtaddr);
#endif
#if SMAP_LIB_MEMORY_OK != 0
			/* set error flag */
			rc = -1;
		} else {
			/* clear error flag */
			rc = 0;
#endif
		}
		system_memory_info_delete(&del);
	}
err_exit:
	if (rc == 0)
		return rtaddr.apmem_rtaddr;
	else
		return 0;
}
EXPORT_SYMBOL(sh_mobile_appmem_getRTaddress);


/***************************************************/
/* name : sh_mobile_appmem_getaddress              */
/* function: get virtual address of offset.        */
/* return: virtual address to access app share.    */
/*         NULL if found error.                    */
/***************************************************/
unsigned char *sh_mobile_appmem_getaddress(\
	struct appmem_handle *appmem, int offset)
{
	int rc;
	unsigned char *vadr = NULL;

	printk_dbg(1, "appmem:%p offset:0x%x\n", appmem, offset);

	if (appmem == NULL) {
		printk_err("appmem handle passed NULL.\n");
		goto err_exit;
	}

	if (appmem->size) {
		int min = appmem->offset;
		int max = appmem->offset + appmem->size;
		/* appmem handle created by sh_mobile_appmem_alloc */
		if (offset < min || offset >= max) {
			printk_err("offset 0x%x out of valid range" \
				"(0x%x - 0x%x)\n", offset, min, max);
			goto err_exit;
		}
		vadr = appmem->vaddr + offset;
	} else {
		void *handle;
		system_mem_ap_share_mem  mem;
		system_mem_info_delete   del;

		handle = system_memory_info_new();
		if (handle == NULL) {
			printk_err("system_memory_info_new error\n");
			goto err_exit;
		}

		/* set handle */
		mem.handle = handle;
		del.handle = handle;

		/* set share_mem parameter */
		mem.apmem_handle = appmem->memhandle;
		mem.apmem_offset = offset;
		rc = system_memory_ap_share_mem(&mem);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_share_mem " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_share_mem(&mem);
#endif
		} else {
			/* update vadr */
			vadr = (unsigned char *)mem.apmem_apaddr;
		}

		system_memory_info_delete(&del);
	}
err_exit:
	return vadr;
}
EXPORT_SYMBOL(sh_mobile_appmem_getaddress);


/***************************************************/
/* name : sh_mobile_appmem_free                    */
/* function: release app share handle.             */
/* return: 0  if success.                          */
/*         -EINVAL if found error.                 */
/***************************************************/
int sh_mobile_appmem_free(struct appmem_handle *appmem)
{
	/* remove list */
	unsigned long flags;
	int      rc = -EINVAL;
	void     *handle = NULL;

	printk_dbg(1, "appmem:%p\n", appmem);

	if (appmem == NULL) {
		printk_err("appmem handle passed NULL.\n");
		goto err_exit;
	}

	spin_lock_irqsave(&app_share_lock, flags);
	if (appmem->ref_count > 0) {
		/* decrement reference count */
		appmem->ref_count--;
	}
	if (appmem->ref_count != 0) {
		/* set return code */
		rc = 0;
	}
	spin_unlock_irqrestore(&app_share_lock, flags);

	printk_dbg(2, "appmem:%p ref_count:%d\n", appmem, appmem->ref_count);
	if (rc == 0) {
		/* reference count not equal 0 */
		printk_dbg(2, "not free appmem:%p (appid:%d).\n",
			appmem, appmem->app_id);
		goto err_exit;
	}

	list_del_init(&appmem->list);

	handle = system_memory_info_new();
	if (handle == NULL) {
		printk_err("system_memory_info_new error\n");
		goto err_exit;
	}

	if (appmem->size) {
		/* need free */
		system_mem_ap_free fre;

		fre.handle       = handle;
		fre.apmem_handle = appmem->memhandle;
		fre.apmem_apaddr = (unsigned int)appmem->vaddr + appmem->offset;
		rc = system_memory_ap_free(&fre);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_free " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_free(&fre);
#endif
			rc = -EINVAL;
			/* fall through, assume memory free */
		}
		appmem->vaddr = NULL;
		appmem->size  = 0;
	}
	{
		system_mem_rt_unmap_pnc upnc;
		system_mem_ap_close clo;

		upnc.handle       = handle;
		upnc.apmem_handle = appmem->memhandle;
		appmem->memhandle = NULL;
		rc = system_memory_rt_unmap_pnc(&upnc);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_rt_unmap_pnc " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_rt_unmap_pnc(&upnc);
#endif
		}

		clo.handle = handle;
		clo.apaddr = appmem->op_apaddr;
		clo.pages  = appmem->op_pages;
		appmem->op_apaddr = 0;
		appmem->op_pages  = NULL;
		rc = system_memory_ap_close(&clo);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_ap_close " \
				"error: return by %d %s\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_ap_close(&clo);
#endif
			rc = -EINVAL;
			/* fall through, assume close success */
		}
	}

	printk_dbg(2, "free appshare memory.\n");
	printk_dbg(2, "  app_id:0x%x key:%s\n", appmem->app_id, appmem->key);
	kfree(appmem);
err_exit:
	if (handle) {
		system_mem_info_delete   del;
		del.handle = handle;
		system_memory_info_delete(&del);
	}
	if (debug) {
		/* display debug information. */
		sh_mobile_appmem_dump_appshare_list();
	}
	return rc;
}
EXPORT_SYMBOL(sh_mobile_appmem_free);


/***************************************************/
/* name : sh_mobile_appmem_debugmode               */
/* function: set debug mode flag of app share.     */
/* return: -                                       */
/***************************************************/
void sh_mobile_appmem_debugmode(int mode)
{
	/* not implemented. */
	debug = mode;
}
EXPORT_SYMBOL(sh_mobile_appmem_debugmode);

/**************************************************/
/* implementation for rt addr/ physical handling  */
/**************************************************/
/**************************************************/
/* name : sh_mobile_appmem_dump_appshare_list     */
/* function: dump list of app share.              */
/**************************************************/
static void sh_mobile_appmem_dump_rt_phys_list(void)
{
	unsigned long flags;
	struct list_head *list;

	printk_dbg(1, "list currently registered physical address.\n");
	spin_lock_irqsave(&app_share_lock, flags);

	list_for_each(list, &rt_physaddr_top)
	{
		struct rtmem_phys_handle *mem = NULL;

		mem = list_entry((void *)list, struct rtmem_phys_handle, list);

		printk_dbg(1, "  phys_addr:0x%lx size:0x%x rtaddr:0x%lx\n",
			mem->phys_addr, mem->size, mem->rt_addr);
	}

	spin_unlock_irqrestore(&app_share_lock, flags);
}


/**************************************************/
/* name : sh_mobile_rtmem_physarea_register       */
/* function: map physical memory                  */
/**************************************************/
struct rtmem_phys_handle *sh_mobile_rtmem_physarea_register(
	int size,
	unsigned long phys_addr)
{
	void *handle = NULL;
	struct rtmem_phys_handle *mem = NULL;
	unsigned long rtaddr = 0;
	int  rc;

	sh_mobile_appmem__init();

	printk_dbg(1, "size:0x%x phys:0x%lx\n", size, phys_addr);

/* confirm already mapped. */
	{
		unsigned long flags;
		struct list_head *list;

		rc = false;
		spin_lock_irqsave(&app_share_lock, flags);
		list_for_each(list, &rt_physaddr_top)
		{
			unsigned long adr_low, adr_high;
			struct rtmem_phys_handle *info;

			info = list_entry((void *)list,
				struct rtmem_phys_handle, list);

			adr_low  =           info->phys_addr;
			adr_high = adr_low + info->size;

			if ((adr_low < phys_addr + size) &&
				(adr_high > phys_addr)) {
				/* this memory block has collision. */
				printk_dbg(1, "found collision: 0x%lx-0x%lx\n",
					adr_low, adr_high);
				rc = true;
				break;
			}
		}
		spin_unlock_irqrestore(&app_share_lock, flags);

		if (rc) {
			printk_err("register 0x%lx has collision not mapped.\n",
				phys_addr);
			goto err_exit;
		}
	}

/* map rt-physical address */

	handle = system_memory_info_new();
	if (handle == NULL) {
		printk_err("system_memory_info_new error\n");
		goto err_exit;
	}

	{
		system_mem_rt_map  map;
		map.handle    = handle;
		map.phys_addr = phys_addr;
		map.map_size  = size;
		map.rtaddr    = 0;

		printk_dbg(1, "system_memory_rt_map " \
			"handle:%p phys_addr:0x%x map_size:0x%x\n",
				map.handle, map.phys_addr, map.map_size);

		rc = system_memory_rt_map(&map);
		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_rt_map return by %d %s.\n",
				rc, get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_rt_map(&map);
#endif
			goto err_exit;
		}
		rtaddr = map.rtaddr;
	}

/* allocate object */

	mem = kmalloc(sizeof(*mem), GFP_KERNEL);
	if (mem == NULL) {
		printk_err("kmalloc error\n");
		goto err_exit;
	} else {
		unsigned long flags;

		mem->size      = size;
		mem->rt_addr   = rtaddr;
		mem->phys_addr = phys_addr;

		INIT_LIST_HEAD(&mem->list);

		printk_dbg(2, "physical address registered.\n");
		printk_dbg(2, "  phys_addr:0x%lx size:0x%x rtaddr:0x%lx\n",
			mem->phys_addr, mem->size, mem->rt_addr);

		spin_lock_irqsave(&app_share_lock, flags);
		list_add_tail(&mem->list, &rt_physaddr_top);
		spin_unlock_irqrestore(&app_share_lock, flags);
	}

err_exit:
	if (mem == NULL) {
		/* free resources */
		if (rtaddr) {
			system_mem_rt_unmap unmap;

			unmap.handle   = handle;
			unmap.rtaddr   = rtaddr;
			unmap.map_size = size;

			printk_dbg(1, "system_memory_rt_unmap " \
				"handle:%p rtaddr:0x%x map_size:0x%x\n",
				unmap.handle, unmap.rtaddr, unmap.map_size);

			rc = system_memory_rt_unmap(&unmap);

			if (rc != SMAP_LIB_MEMORY_OK) {
				printk_err("system_memory_rt_unmap " \
					"return by %d %s.\n", rc,
					get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
				dump_system_memory_rt_unmap(&unmap);
#endif
			}
			rtaddr = 0;
		}
	}
	if (handle) {
		system_mem_info_delete del;
		del.handle = handle;

		system_memory_info_delete(&del);
	}
	if (debug) {
		/* display debug information. */
		sh_mobile_appmem_dump_rt_phys_list();
	}
	return mem;
}
EXPORT_SYMBOL(sh_mobile_rtmem_physarea_register);


/**************************************************/
/* name : sh_mobile_rtmem_physarea_unregister     */
/* function: unmap physical memory                */
/**************************************************/
void sh_mobile_rtmem_physarea_unregister(
	struct rtmem_phys_handle *_mem)
{
	struct rtmem_phys_handle *mem = NULL;

	void *handle = NULL;
	int  rc;

	printk_dbg(1, "mam:%p\n", _mem);

	sh_mobile_appmem__init();

	mem = NULL;

	/* confirm handle registered */
	if (_mem) {
		unsigned long flags;
		struct list_head *list;

		spin_lock_irqsave(&app_share_lock, flags);
		list_for_each(list, &rt_physaddr_top)
		{
			struct rtmem_phys_handle *tmp = NULL;

			tmp = list_entry((void *)list,
				struct rtmem_phys_handle, list);

			if (tmp == _mem) {
				/* handle registered. */
				mem = tmp;
				break;
			}
		}
		spin_unlock_irqrestore(&app_share_lock, flags);
	}

	if (mem == NULL) {
		printk_err("handle not registered.\n");
		goto err_exit;
	}

	handle = system_memory_info_new();
	if (handle == NULL) {
		printk_err("system_memory_info_new error\n");
		goto err_exit;
	}

	list_del_init(&mem->list);

	{
		system_mem_rt_unmap unmap;

		unmap.handle   = handle;
		unmap.rtaddr   = mem->rt_addr;
		unmap.map_size = mem->size;

		printk_dbg(1, "system_memory_rt_unmap " \
			"handle:%p rtaddr:0x%x map_size:0x%x\n",
			unmap.handle, unmap.rtaddr, unmap.map_size);

		rc = system_memory_rt_unmap(&unmap);

		if (rc != SMAP_LIB_MEMORY_OK) {
			printk_err("system_memory_rt_unmap " \
				"return by %d %s.\n", rc,
				get_RTAPImsg_memory(rc));
#if DEBUG_RTAPI_ARGUMENT
			dump_system_memory_rt_unmap(&unmap);
#endif
		}
	}

	printk_dbg(2, "physical address unregistered.\n");
	kfree(mem);

err_exit:
	if (handle) {
		system_mem_info_delete del;
		del.handle = handle;

		system_memory_info_delete(&del);
	}
	if (debug) {
		/* display debug information. */
		sh_mobile_appmem_dump_rt_phys_list();
	}
	return;
}
EXPORT_SYMBOL(sh_mobile_rtmem_physarea_unregister);


/**************************************************/
/* name : sh_mobile_rtmem_conv_phys2rtmem         */
/* function: convert physical to RT address       */
/**************************************************/
unsigned long sh_mobile_rtmem_conv_phys2rtmem(
	unsigned long phys_addr)
{
	struct rtmem_phys_handle *mem;
	unsigned long rt_addr;
	unsigned long flags;
	struct list_head *list;

	sh_mobile_appmem__init();

	rt_addr = 0;
	spin_lock_irqsave(&app_share_lock, flags);
	list_for_each(list, &rt_physaddr_top)
	{
		unsigned long adr_low, adr_high;
		int  offset;

		mem = list_entry((void *)list,
			struct rtmem_phys_handle, list);

		adr_low  =           mem->phys_addr;
		adr_high = adr_low + mem->size;
		if (adr_low <= phys_addr && phys_addr < adr_high) {
			offset = phys_addr - adr_low;

			rt_addr = mem->rt_addr + offset;
			break;
		}
	}
	spin_unlock_irqrestore(&app_share_lock, flags);

	if (debug) {
		/* display debug information. */
		sh_mobile_appmem_dump_rt_phys_list();
	}
	if (rt_addr == 0) {
		printk_dbg(1, "physical address:0x%lx not registered.\n",
			phys_addr);
	} else {
		printk_dbg(1, "physical address 0x%lx convert to " \
			"RT address 0x%lx\n",
			phys_addr, rt_addr);
	}

	return rt_addr;
}
EXPORT_SYMBOL(sh_mobile_rtmem_conv_phys2rtmem);


/**************************************************/
/* name : sh_mobile_rtmem_conv_rt2physmem         */
/* function: convert RT to physical address       */
/**************************************************/
unsigned long sh_mobile_rtmem_conv_rt2physmem(
	unsigned long rt_addr)
{
	struct rtmem_phys_handle *mem;
	unsigned long phys_addr;
	unsigned long flags;
	struct list_head *list;

	sh_mobile_appmem__init();

	phys_addr = 0;
	spin_lock_irqsave(&app_share_lock, flags);
	list_for_each(list, &rt_physaddr_top)
	{
		unsigned long adr_low, adr_high;
		int  offset;

		mem = list_entry((void *)list,
			struct rtmem_phys_handle, list);

		adr_low  =           mem->rt_addr;
		adr_high = adr_low + mem->size;
		if (adr_low <= rt_addr && rt_addr < adr_high) {
			offset = rt_addr - adr_low;

			phys_addr = mem->phys_addr + offset;
			break;
		}
	}
	spin_unlock_irqrestore(&app_share_lock, flags);

	if (debug) {
		/* display debug information. */
		sh_mobile_appmem_dump_rt_phys_list();
	}
	if (phys_addr == 0) {
		printk_dbg(1, "RT address:0x%lx not registered.\n",
			phys_addr);
	} else {
		printk_dbg(1, "RT address 0x%lx convert to " \
			"physical address 0x%lx\n",
			rt_addr, phys_addr);
	}

	return phys_addr;
}
EXPORT_SYMBOL(sh_mobile_rtmem_conv_rt2physmem);
