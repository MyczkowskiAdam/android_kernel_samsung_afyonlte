/*
 * system_memory_private.c
 *	 System memory manager device driver API private header file.
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
#ifndef __SYSTEM_MEMORY_PRIVATE_H__
#define __SYSTEM_MEMORY_PRIVATE_H__

#define RT_MEMORY_SYSTEM_ERR	(-256)

#define RT_MEMORY_PAGESIZE		(4096)
#define RT_MEMORY_PAGEINFO		(128)

#define RT_MEMORY_APP_ADDR_START	(0xC0000000)

typedef struct {
	void	*handle;
} mem_info_handle;

typedef struct {
	void	*iccom_handle;
	void	*rtds_mem_handle;
} mem_rtapi_info_handle;

typedef struct {
	unsigned long	app_addr;
	unsigned long	rt_addr;
	unsigned long	rt_addr_nc;
	unsigned int	apmem_id;
	unsigned int	mem_size;
	unsigned int	app_cache;
	unsigned int	rt_cache;
	struct page		**pages;
} rtds_memory_shared_info;

#endif	/* __SYSTEM_MEMORY_PRIVATE_H__ */

