/*
 * rtds_memory_drv_common.h
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
#ifndef __RTDS_MEMORY_DRV_COMMON_H__
#define __RTDS_MEMORY_DRV_COMMON_H__

/* ******************************* STRUCTURE ******************************** */
/* IOCTL data info */
typedef struct {
	void				*data;			/* user data */
	unsigned int		data_size;		/* user data size */
} rtds_memory_ioctl_data;

/* Handle data info */
typedef struct {
	unsigned long	app_address;
	unsigned long	rt_address;
	unsigned int	var_size;
} rtds_memory_init_info;

/* ******************************* CONSTANTS ******************************** */

#define RTDS_MEM_CMD_MAGIC 'r'

/* IOCTL command */
enum {
	MEM_CMD_WR_INIT = 0,
	MEM_CMD_WR_EXIT,
	MEM_CMD_WR_SET_MPRO,
	MEM_CMD_WR_OPEN_MEMORY,
	MEM_CMD_WR_CLOSE_MEMORY,
	MEM_CMD_WR_SHARE_MEMORY,
	MEM_CMD_WR_FLUSH_CACHE,
	MEM_CMD_WR_CLEAR_CACHE,
	MEM_CMD_WR_RTMAP,
	MEM_CMD_WR_RTUNMAP,
	MEM_CMD_WR_RTMAP_PNC,
	MEM_CMD_WR_RTUNMAP_PNC,
	MEM_CMD_WR_GET_MEMSIZE,
	MEM_CMD_WR_GET_PAGESINFO,
	MEM_CMD_WR_RTMAP_PNC_NMA,
	MEM_CMD_WR_PHYMEM,
	MEM_CMD_WR_CHANGE_PHY_PMB,
	MEM_CMD_WR_CHANGE_PMB_PHY,
#ifdef RTDS_SUPPORT_CMA
	MEM_CMD_WR_ALLOC_CMA,
	MEM_CMD_WR_FREE_CMA,
	MEM_CMD_WR_RTMAP_MA,
	MEM_CMD_WR_RTUNMAP_MA,
#endif
	MEM_COMMAND_END
};



/* IOCTL command identifier */
#define IOCTL_MEM_CMD_WR_SET_MPRO		_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_SET_MPRO, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_OPEN_MEMORY	_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_OPEN_MEMORY, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_CLOSE_MEMORY   _IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_CLOSE_MEMORY, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_SHARE_MEMORY   _IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_SHARE_MEMORY, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_FLUSH_CACHE	_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_FLUSH_CACHE, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_CLEAR_CACHE	_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_CLEAR_CACHE, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_RTMAP			_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_RTMAP, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_RTUNMAP		_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_RTUNMAP, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_RTMAP_PNC		_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_RTMAP_PNC, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_RTUNMAP_PNC	_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_RTUNMAP_PNC, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_INIT			_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_INIT, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_EXIT			_IO(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_EXIT)
#define IOCTL_MEM_CMD_WR_GET_MEMSIZE	_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_GET_MEMSIZE, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_GET_PAGESINFO	_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_GET_PAGESINFO, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_RTMAP_PNC_NMA	_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_RTMAP_PNC_NMA, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_PHYMEM			_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_PHYMEM, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_CHANGE_PHY_PMB	_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_CHANGE_PHY_PMB, system_mem_phy_change_rtpmbaddr)
#define IOCTL_MEM_CMD_WR_CHANGE_PMB_PHY	_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_CHANGE_PMB_PHY, system_mem_rtpmb_change_phyaddr)
#ifdef RTDS_SUPPORT_CMA
#define IOCTL_MEM_CMD_WR_ALLOC_CMA		_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_ALLOC_CMA, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_FREE_CMA		_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_FREE_CMA, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_RTMAP_MA		_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_RTMAP_MA, rtds_memory_ioctl_data)
#define IOCTL_MEM_CMD_WR_RTUNMAP_MA		_IOWR(RTDS_MEM_CMD_MAGIC, MEM_CMD_WR_RTUNMAP_MA, rtds_memory_ioctl_data)
#endif
/* Memory Manager */
/* Memory Manager Task */
/* Task ID */
#define TASK_MEMORY (6)
/* SMB ID */
#define SMB_MEMORY (TASK_MEMORY*2)
/* Function ID Base */
#define FUNCTIONID_MEMORY_BASE (TASK_MEMORY*256+3)

/* Memory Manager Sub Task */
/* Task ID */
#define TASK_MEMORY_SUB (13)
/* SMB ID */
#define SMB_MEMORY_SUB (TASK_MEMORY_SUB*2)
/* Function ID Base */
#define FUNCTIONID_MEMORY_SUB_BASE (TASK_MEMORY_SUB*256+3)

/* Memory Callback Task */
/* Task ID */
#define TASK_MEMORY_CB (14)
/* SMB ID */
#define SMB_MEMORY_CB (TASK_MEMORY_CB*2)
/* Function ID Base */
#define FUNCTIONID_MEMORY_CB_BASE (TASK_MEMORY_CB*256+3)


/* Function ID */
/* Memory Manager */
#define EVENT_MEMORY_GETMEMINFORT		(FUNCTIONID_MEMORY_BASE+1)
#define EVENT_MEMORY_GLOBALALLOCATERT	(FUNCTIONID_MEMORY_BASE+2)
#define EVENT_MEMORY_GLOBALFREERT		(FUNCTIONID_MEMORY_BASE+3)
#define EVENT_MEMORY_CREATEAPPMEMORY	(FUNCTIONID_MEMORY_BASE+4)
#define EVENT_MEMORY_OPENAPPMEMORY		(FUNCTIONID_MEMORY_BASE+5)
#define EVENT_MEMORY_CLOSEAPPMEMORY		(FUNCTIONID_MEMORY_BASE+6)
#define EVENT_MEMORY_FLUSHTLB			(FUNCTIONID_MEMORY_BASE+9)
#define EVENT_MEMORY_ALLOCATEMERAM		(FUNCTIONID_MEMORY_BASE+10)
#define EVENT_MEMORY_FREEMERAM			(FUNCTIONID_MEMORY_BASE+11)
#ifdef RTDS_SUPPORT_CMA
#define EVENT_MEMORY_OPERATECTGMEMORY	(FUNCTIONID_MEMORY_BASE+12)
#endif

/* Memory Manager Sub */
#define EVENT_MEMORYSUB_INITAPPSHAREDMEM	(FUNCTIONID_MEMORY_SUB_BASE+1)
#define EVENT_MEMORYSUB_DELETEAPPSHAREDMEM	(FUNCTIONID_MEMORY_SUB_BASE+2)
#define EVENT_MEMORYSUB_GLOBALALLOCATEAPP	(FUNCTIONID_MEMORY_SUB_BASE+3)
#define EVENT_MEMORYSUB_GLOBALFREEAPP		(FUNCTIONID_MEMORY_SUB_BASE+4)
#define EVENT_MEMORYSUB_GETMEMINFOAPP		(FUNCTIONID_MEMORY_SUB_BASE+5)

/* Memory Callback */
#define EVENT_MEMORYCB_CHECKUNMAP		(FUNCTIONID_MEMORY_CB_BASE+1)

/* ******************************* PROTOTYPE ******************************** */


#endif /* __RTDS_MEMORY_DRV_COMMON_H__ */
