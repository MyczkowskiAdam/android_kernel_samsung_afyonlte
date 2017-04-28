/*
 * rtds_memory_drv_main.h
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

#ifndef __RTDS_MEMORY_DRV_MAIN_H__
#define __RTDS_MEMORY_DRV_MAIN_H__

/* ****************************** CONSTANTS ******************************* */

#define RTDS_MEMORY_DRIVER_NAME "rtds_mem"

#define RTDS_MEM_MPRO_ACTIVE	(1)
#define RTDS_MEM_MPRO_INACTIVE	(0)

#define RTDS_MEM_WAIT_TIMEOUT	(40000)	/* timeout value (msec) */

#define RTDS_MEM_DOWN_TIMEOUT(sem)					\
	do {								\
		if (0 != down_timeout(sem, g_rtds_memory_sem_jiffies))	\
			panic("[RTDSK]ERR|[%s]L.[%d]down time out!\n",	\
						__func__, __LINE__);	\
	} while (0)

/* ****************************** STRUCTURE ******************************* */

/* IOCTL memory info */
typedef struct {
	unsigned int		app_cache;	/* AppDomain cache type */
	unsigned int		rt_cache;
	unsigned int		mem_size;	/* Shared memory size */
	unsigned long		app_addr;	/* AppDomain logical address */
	struct page		**pages;	/* Page descriptor info */
	unsigned long		rt_addr;
	unsigned long		rt_addr_nc;
	unsigned int		apmem_id;
	int			err_code;	/* Error code */
} rtds_memory_drv_ioctl_mem;

/* IOCTL cache info */
typedef struct {
	unsigned long			app_addr;   /* Cache address */
	unsigned int			mem_size;   /* Cache size */
} rtds_memory_drv_ioctl_cache;

/* IOCTL map info */
typedef struct {
	unsigned int		phy_addr;
	unsigned int		mem_size;
	unsigned int		app_addr;
} rtds_memory_drv_ioctl_rtmap;

/* IOCTL unmap info */
typedef struct {
	unsigned int		mem_size;
	unsigned int		app_addr;
} rtds_memory_drv_ioctl_rtunmap;

/* IOCTL phymem info */
typedef struct {
	unsigned int		phy_addr;
	unsigned int		mem_size;
	unsigned int		app_addr;
} rtds_memory_drv_ioctl_phymem;

#ifdef RTDS_SUPPORT_CMA
typedef struct {
	unsigned int	phys_addr;
	unsigned int	mem_size;
	unsigned int	mem_attr;
	int		err_code;
	unsigned int	vir_addr;
	unsigned int	mapping;
} rtds_memory_drv_ioctl_cma;
#endif

/* IOCTL parameter info */
typedef struct {
	void		*handle;			/* RTDS MEMORY handle */
	unsigned int	command;			/* command */
	union {
		rtds_memory_drv_ioctl_mem   mem;	/* IOCTL memory info */
		rtds_memory_drv_ioctl_cache cache;	/* IOCTL cache info */
		rtds_memory_drv_ioctl_rtmap   rtmap;	/* IOCTL map info */
		rtds_memory_drv_ioctl_rtunmap rtunmap;  /* IOCTL unmap info */
		rtds_memory_drv_ioctl_phymem phymem;	/* IOCTL phymem info */
#ifdef RTDS_SUPPORT_CMA
		rtds_memory_drv_ioctl_cma	cma;
#endif
	};
} rtds_memory_drv_ioctl_param;

/* Memory info */
typedef struct {
	unsigned long		app_addr;		/* AppDomain logical address */
	unsigned long		app_cache;		/* Cache type of App domain side */
	unsigned long		mem_size;		/* Memory size */
	struct page		**pages;		/* Page descriptor info */
} rtds_memory_apmem_info;

/* AppDomain shared memory info table*/
typedef struct {
	struct list_head	list_head;		/* Manager list info */
	struct list_head	list_head_mpro;		/* Manager list info */
	struct list_head	list_head_leak;		/* Manager list info */
	unsigned long		event;			/* Request event to Mpro*/
	unsigned long		memory_size;		/* Memory size */
	unsigned long		app_cache;		/* Cache type of App domain side */
	unsigned long		rt_cache;		/* Cache type of RT domain side */
	unsigned long		app_addr;		/* AppDomain logical address */
	unsigned long		rt_wb_addr;		/* RT domain write-back address */
	unsigned long		rt_nc_addr;		/* RT domain non-cache address */
	struct page		**pages;		/* Page descriptor info */
	unsigned int		apmem_id;		/* AppDomain shared memory ID */
	struct semaphore	semaphore;		/* Mpro */
	int			error_code;		/* Mpro result */
	struct task_struct	*task_info;		/* Task info */
	int			trigger;		/* Memory operation trigger */
	unsigned int		phys_addr;
	unsigned int		mem_attr;
	pid_t			tgid;
	unsigned int		map_id;
} rtds_memory_app_memory_table;

typedef struct {
	struct list_head	head;		/* Manager list info */
 	pid_t				tgid;
	unsigned int		size;
} rtds_pid_table;


/* RTDS memory section info */
typedef struct {
	unsigned long		var_address;		/* Variable address */
	unsigned long		var_length;		/* Variable length */
	void __iomem		*kernel_var_addr;	/* Variable memory address(kernel) */
	unsigned long		sh_pmb_offset;
	unsigned long		sh_pmb_nc_offset;
	unsigned long		mfi_pmb_offset;
} rtds_memory_section_info;

/* Memory mapping info */
typedef struct {
	unsigned long		cache_kind;		/* Cache type */
	bool			data_ent;		/* Virtula area entry flag */
	unsigned long		mapping_flag;   /* Memory mapping flag */
	rtds_memory_app_memory_table   *mem_table; /* AppDomain shared memory info table*/
} rtds_memory_mapping_data;

/* Mpro setting data info */
typedef struct {
	unsigned long	pgd_phy_addr;	/* Page gloval directry physical address */
	unsigned long	context_id;		/* ContextID */
} rtds_memory_mpro_data;

/* Mpro info */
typedef struct {
	rtds_memory_mpro_data	mpro_data;	/* Mpro data info */
	unsigned long		apmem_id;	/* AppDomain shared memory ID */
} rtds_memory_mpro_control;

/* RTDS MEMORY data info */
typedef struct {
	unsigned long	var_app_addr;		/* Variable area address */
	unsigned long	var_rt_nc_addr;		/* Variable area non cache address(for RT Domain access) */
	unsigned long	var_kernel_addr;	/* Variable area address(for kernel) */
	unsigned long	var_area_len;		/* Variable area size */
	rtds_memory_app_memory_table	*mem_table;	/* AppDomain shared memory info table */
	rtds_memory_mapping_data	mem_map;	/* Memory mapping info */
	rtds_memory_mpro_control	*mpro_control;	/* Mpro info */
} rtds_memory_drv_data;

/* rtmem map table*/
typedef struct {
	struct list_head	list_head;		/* Manager list info */
	pid_t			tgid;			/* thread group leader process ID  */
	unsigned long		rt_addr;		/* rtmem logical address (AppDomain) */
	unsigned long		open_count;		/* rtmem open count */
} rtds_memory_rtmem_table;

/* ****************************** PROTOTYPE ******************************* */

int rtds_memory_drv_open(
		struct inode	*inode,
		struct file	*fp
);

int rtds_memory_drv_close(
		struct inode	*inode,
		struct file	*fp
);

long rtds_memory_drv_ioctl(
		struct file	*fp,
		unsigned int	cmd,
		unsigned long	arg
);

int rtds_memory_thread_apmem_rttrig(
		void *vp
);

int rtds_memory_drv_mapping(
		struct file		*fp,
		struct vm_area_struct	*vm_area
);

int rtds_memory_init_module(
		void
);

void rtds_memory_exit_module(
		void
);

#if defined(CONFIG_RTDS_LMK)
int rtds_mem_check_to_lmk(int minfree, int adj);
#endif

extern spinlock_t			g_rtds_memory_lock_recv_queue;
extern struct list_head			g_rtds_memory_list_rcv_event;
extern struct semaphore			g_rtds_memory_apmem_rttrig_sem;
extern struct completion		g_rtds_memory_completion;
extern void				*g_rtds_memory_iccom_handle;
extern spinlock_t			g_rtds_memory_lock_cache_all;
extern rtds_memory_section_info		g_rtds_memory_section_info;
extern spinlock_t			g_rtds_memory_lock_mpro;
extern struct list_head			g_rtds_memory_list_mpro;
extern struct semaphore			g_rtds_memory_mpro_sem;
extern struct list_head			g_rtds_memory_list_shared_mem;
extern struct list_head			g_rtds_process_list;
extern struct list_head			g_rtds_memory_list_leak_mpro;
extern struct semaphore			g_rtds_memory_shared_mem;
extern struct list_head			g_rtds_memory_list_reg_phymem;
extern struct semaphore			g_rtds_memory_phy_mem;
extern struct list_head			g_rtds_memory_list_create_mem;
extern spinlock_t			g_rtds_memory_lock_create_mem;
extern struct list_head			g_rtds_memory_list_map_rtmem;
extern spinlock_t			g_rtds_memory_lock_map_rtmem;
extern struct semaphore			g_rtds_memory_send_sem;
extern long				g_rtds_memory_sem_jiffies;

#ifdef RTDS_SUPPORT_CMA
extern spinlock_t			g_rtds_memory_lock_cma;
extern struct list_head			g_rtds_memory_list_cma;
#endif
#endif /* __RTDS_MEMORY_DRV_MAIN_H__ */
