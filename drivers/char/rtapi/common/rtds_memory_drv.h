/*
 * rtds_memory_drv.h
 *	 RT domain shared memory device driver API function file.
 *
 * Copyright (C) 2012 Renesas Electronics Corporation
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
#ifndef __RTDS_MEMORY_DRV_H__
#define __RTDS_MEMORY_DRV_H__

/* ******************************* CONSTANTS ******************************** */

#define RTDS_MEMORY_DRV_WRITEBACK			0x00000000
#define RTDS_MEMORY_DRV_WRITETHROUGH		0x00000001
#define RTDS_MEMORY_DRV_NONCACHE			0x00000002
#define RTDS_MEMORY_DRV_BUFFER_NONCACHE		0x00000004

#define RTDS_MEM_DRV_APP_MEM				0	/* AppDomain */
#define RTDS_MEM_DRV_RT_MEM_NC				1	/* RTDomain(Non-cache)*/

/* ******************************* STRUCTURE ******************************** */
/* RTDS MEMORY handle info */
typedef struct {
	unsigned long		var_app_addr;		/* App Domain address */
	unsigned long		var_rt_addr_nc;		/* RT Domain address */
	unsigned int		var_addr_size;		/* Memory size */
	unsigned long		var_kernel_addr;	/* Variable area address(for kernel) */
	int					fd;					/* Device file info */
} rtds_memory_drv_handle;

/* RTDS MEMORY handle cleanup info */
typedef struct {
	void		*handle;		/* RTDS MEMORY handle */
} rtds_memory_drv_cleanup_param;

/* App shared memory create info */
typedef struct {
	unsigned int		mem_size;		/* memory size */
	unsigned int		app_cache;		/* Cache type of App Domain side */
	unsigned int		app_addr;		/* Logical address of App Domain side */
	struct page			**pages;		/* Page descriptor info */
} rtds_memory_drv_open_mem_param;

/* App shared memory destroy info */
typedef struct {
	unsigned int		app_addr;		/* Logical address of App Domain side */
	struct page			**pages;		/* Page descriptor info */
} rtds_memory_drv_close_mem_param;

/* Change logical address info */
typedef struct {
	void				*handle;		/* RTDS MEMORY handle */
	unsigned int		addr_type;		/* Address type(App/RT Domain) */
	unsigned int		org_addr;		/* Original address */
	unsigned int		chg_addr;		/* Changed address */
} rtds_memory_drv_change_addr_param;

/* RT Domain Map info */
typedef struct {
	unsigned long		rt_write_back_addr;	/* RT domain write-back address */
	unsigned long		rt_non_cache_addr;	/* RT domain non-cache address */
	unsigned long		apmem_id;			/* App shared memory ID */
} rtds_memory_drv_info;

/* App shared memory share info */
typedef struct {
	unsigned int			apmem_id;	/* App shared memory ID */
	unsigned long			app_addr;	/* Logical address of App Domain side */
	unsigned long			rt_addr;	/* RT domain write-back address */
	unsigned long			rt_addr_nc;	/* RT domain non-cache address */
	unsigned int			mem_size;	/* memory size */
	unsigned int			app_cache;	/* Cache type of App Domain side */
	unsigned int			rt_cache;	/* Cache type of RT Domain side */
	struct page				**pages;	/* Page descriptor info */
} rtds_memory_drv_share_mem_param;

/* Physical continuation domain map info */
typedef struct {
	void				*handle;		/* RTDS MEMORY handle */
	unsigned int		phy_addr;		/* Physical address */
	unsigned int		mem_size;		/* map size */
	unsigned int		app_addr;		/* Logical address of RT Domain side */
} rtds_memory_drv_map_param;

/* Physical continuation domain unmap info */
typedef struct {
	void				*handle;		/* RTDS MEMORY handle */
	unsigned int		mem_size;		/* unmap size */
	unsigned int		app_addr;		/* Logical address of RT Domain side */
} rtds_memory_drv_unmap_param;

/* Physical non-continuation domain map info */
typedef struct {
	void					*handle;		/* RTDS MEMORY handle */
	unsigned int			app_addr;		/* Logical address of APP Domain side */
	unsigned int			map_size;		/* map size */
	struct page				**pages;		/* Page descriptor info */
	unsigned int			rtcache_kind;	/* Cache type of RT Domain side */
	rtds_memory_drv_info	mem_info;		/* RT Domain Map info */
} rtds_memory_drv_map_pnc_param;

/* Physical non-continuation domain unmap info */
typedef struct {
	void				*handle;		/* RTDS MEMORY handle */
	unsigned long		app_addr;		/* Logical address of APP Domain side */
	unsigned int		apmem_id;		/* App shared memory ID */
} rtds_memory_drv_unmap_pnc_param;

/* Physical non-continuation domain map info */
typedef struct {
	void					*handle;		/* RTDS MEMORY handle */
	unsigned int			map_size;		/* map size */
	struct page				**pages;		/* Page descriptor info */
	unsigned int			rt_addr_wb;		/* Logical address of RT Domain side */
} rtds_memory_drv_map_pnc_nma_param;

/* ******************************* PROTOTYPE ******************************** */

void *rtds_memory_drv_init(
	void
);

void rtds_memory_drv_cleanup(
	rtds_memory_drv_cleanup_param	*rtds_memory_creanup
);

int rtds_memory_drv_open_apmem(
	rtds_memory_drv_open_mem_param	*rtds_memory_open_mem
);

int rtds_memory_drv_close_apmem(
	rtds_memory_drv_close_mem_param	*rtds_memory_close_mem
);

void rtds_memory_drv_flush_cache(
	 unsigned int   addr,
	 unsigned int   size
);

void rtds_memory_drv_inv_cache(
	 unsigned int   addr,
	 unsigned int   size
);

int rtds_memory_drv_change_address(
	rtds_memory_drv_change_addr_param	*rtds_memory_drv_change_addr
);

int rtds_memory_drv_share_apmem(
	rtds_memory_drv_share_mem_param	*rtds_memory_share_mem
);

rtds_memory_drv_handle *rtds_memory_create_handle(
	void
);

void rtds_memory_destroy_handle(
	rtds_memory_drv_handle			*handle
);

int rtds_memory_drv_map(
	rtds_memory_drv_map_param	*rtds_memory_map
);

int rtds_memory_drv_unmap(
	rtds_memory_drv_unmap_param	*rtds_memory_unmap
);

int rtds_memory_drv_map_pnc(
	 rtds_memory_drv_map_pnc_param *rtds_memory_map_pnc
);

int rtds_memory_drv_unmap_pnc(
	 rtds_memory_drv_unmap_pnc_param *rtds_memory_unmap_pnc
);

int rtds_memory_drv_map_pnc_nma(
	 rtds_memory_drv_map_pnc_nma_param *rtds_memory_map_pnc_nma
);

int rtds_memory_drv_reg_phymem(
	rtds_memory_drv_map_param *rtds_memory_map
);

int rtds_memory_drv_unreg_phymem(
	rtds_memory_drv_map_param *rtds_memory_map
);

int rtds_memory_drv_phy_change_address(
	rtds_memory_drv_change_addr_param *rtds_change_addr
);

int rtds_memory_drv_rtpmb_to_phy_address(
	rtds_memory_drv_change_addr_param *rtds_change_addr
);

int rtds_memory_drv_phy_to_rtpmb_address(
	rtds_memory_drv_change_addr_param *rtds_change_addr
);

int rtds_memory_drv_rtpmb_cache_address(
	rtds_memory_drv_change_addr_param *rtds_change_addr
);

void rtds_memory_drv_dump_mpro(
	void
);
#endif /* __RTDS_MEMORY_DRV_H__ */
