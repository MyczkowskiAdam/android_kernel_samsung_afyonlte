/*
 * rtds_memory_drv_main.c
 *	 RT domain shared memory driver API function file.
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
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>

#include <system_memory.h>
#include "log_kernel.h"
#include "rtds_memory_drv.h"
#include "rtds_memory_drv_main.h"
#include "rtds_memory_drv_common.h"
#include "rtds_memory_drv_sub.h"
#include "iccom_drv.h"
#include "system_rtload_internal.h"

/* file operation */
static const struct file_operations	g_rtds_memory_fops = {
	.owner			= THIS_MODULE,
	.open			= rtds_memory_drv_open,
	.release		= rtds_memory_drv_close,
	.mmap			= rtds_memory_drv_mapping,
	.unlocked_ioctl		= rtds_memory_drv_ioctl
};

/* miscdevice info */
static struct miscdevice	g_rtds_memory_device;

/* Asynchronous thread info */
static struct task_struct	*g_rtds_memory_thread_info;

/* Section info */
rtds_memory_section_info	g_rtds_memory_section_info;

/* ICCOM driver handle */
void				*g_rtds_memory_iccom_handle;

struct completion		g_rtds_memory_completion;
struct semaphore		g_rtds_memory_apmem_rttrig_sem;
spinlock_t			g_rtds_memory_lock_recv_queue;
spinlock_t			g_rtds_memory_lock_cache_all;
struct list_head		g_rtds_memory_list_rcv_event;
static unsigned int		g_rtds_memory_mpro_startup_flag;
spinlock_t			g_rtds_memory_lock_create_mem;
struct list_head		g_rtds_memory_list_create_mem;
struct list_head		g_rtds_memory_list_mpro;
struct semaphore		g_rtds_memory_mpro_sem;
spinlock_t			g_rtds_memory_lock_mpro;
struct list_head		g_rtds_memory_list_shared_mem;
struct list_head		g_rtds_process_list;
struct list_head		g_rtds_memory_list_leak_mpro;
struct semaphore		g_rtds_memory_shared_mem;
struct list_head		g_rtds_memory_list_map_rtmem;
spinlock_t			g_rtds_memory_lock_map_rtmem;
struct list_head		g_rtds_memory_list_reg_phymem;
struct semaphore		g_rtds_memory_phy_mem;
struct semaphore		g_rtds_memory_send_sem;
long				g_rtds_memory_sem_jiffies;
static struct proc_dir_entry	*g_rtds_memory_proc_entry;
static struct proc_dir_entry	*g_rtds_memory_proc_mem_info;
static struct proc_dir_entry	*g_rtds_memory_proc_mpro;

#ifdef RTDS_SUPPORT_CMA
spinlock_t			g_rtds_memory_lock_cma;
struct list_head		g_rtds_memory_list_cma;
#endif

static int rtds_mem_procfile_read(char *page, char **start, off_t off,
				int count, int *eof, void *data);
static int rtds_get_cmdline(pid_t pid, char *buf, int len);
/*****************************************************************************
 * Function   : rtds_memory_drv_open
 * Description: This function open RTDS MEMORY driver.
 * Parameters : inode   -   inode information
 *		fp	-   file descriptor
 * Returns    : SMAP_OK		-   Success
 *		SMAP_MEMORY	-   No memory
 *		SMAP_NG		-   FatalError
 *****************************************************************************/
int rtds_memory_drv_open(
		struct inode	*inode,
		struct file	*fp
)
{
	rtds_memory_drv_data		*rtds_memory_data_p;
	rtds_memory_mpro_control	*mpro_control_p;

	MSG_MED("[RTDSK]IN |[%s]\n", __func__);
	MSG_MED("[RTDSK]   |inode_p[0x%08X]\n", (u32)inode);
	MSG_MED("[RTDSK]   |fp[0x%08X]\n", (u32)fp);
	MSG_LOW("[RTDSK]   |major[%d]\n", imajor(inode));
	MSG_LOW("[RTDSK]   |minor[%d]\n", iminor(inode));

	/* Allocate memory for rtds_memory_mpro_control */
	mpro_control_p = kmalloc(sizeof(*mpro_control_p), GFP_KERNEL);
	if (NULL == mpro_control_p) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_mpro_control NotMemory\n");
		MSG_MED("[RTDSK]OUT|[%s] ret = SMAP_MEMORY\n", __func__);
		return SMAP_MEMORY;
	}

	/* Allocate memory for rtds_memory_drv_data */
	rtds_memory_data_p = kmalloc(sizeof(*rtds_memory_data_p), GFP_KERNEL);
	if (NULL == rtds_memory_data_p) {
		MSG_ERROR("[RTDSK]ERR| rtds_memory_drv_data NotMemory\n");
		MSG_MED("[RTDSK]OUT|[%s] ret = SMAP_MEMORY\n", __func__);
		kfree(mpro_control_p);
		return SMAP_MEMORY;
	}

	memset(mpro_control_p, 0, sizeof(*mpro_control_p));
	memset(rtds_memory_data_p, 0, sizeof(*rtds_memory_data_p));

	fp->private_data = rtds_memory_data_p;
	rtds_memory_data_p->mpro_control = mpro_control_p;

	MSG_MED("[RTDSK]OUT|[%s]ret = SMAP_OK\n", __func__);

	return SMAP_OK;
}

/*****************************************************************************
 * Function   : rtds_memory_drv_close
 * Description: This function close RTDS MEMORY driver.
 * Parameters : inode   -   inode information
 *		fp	-   file descriptor
 * Returns    : SMAP_OK	-   Success
 *
 *****************************************************************************/
int rtds_memory_drv_close(
		struct inode	*inode,
		struct file	*fp
)
{
	rtds_memory_drv_data	*rtds_memory_data_p = fp->private_data;

	MSG_MED("[RTDSK]IN |[%s]\n", __func__);
	MSG_MED("[RTDSK]   |inode_p[0x%08X]\n", (u32)inode);
	MSG_MED("[RTDSK]   |fp[0x%08X]\n", (u32)fp);

	/* Release memory for rtds_memory_drv_data */
	if (rtds_memory_data_p) {
		kfree(rtds_memory_data_p->mpro_control);
		kfree(rtds_memory_data_p);
		fp->private_data = NULL;
	}

	MSG_MED("[RTDSK]OUT|[%s]ret = SMAP_OK\n", __func__);

	return SMAP_OK;
}

/*****************************************************************************
 * Function   : rtds_memory_drv_ioctl
 * Description: This function controls I/O for RTDS MEMORY driver.
 * Parameters : inode   -   inode information
 *		fp	-   file descriptor
 * Returns    : SMAP_OK		-   Success
 *		SMAP_NG		-   Fatal error
 *		SMAP_MEMORY	-   No memory
 *
 *****************************************************************************/
long rtds_memory_drv_ioctl(
		struct file	*fp,
		unsigned int	cmd,
		unsigned long	arg
)
{
	int			ret = SMAP_OK;
	rtds_memory_drv_data	*rtds_memory_data_p = fp->private_data;
	rtds_memory_ioctl_data	ioctl_data;
	struct task_struct	*task = current;

	MSG_MED("[RTDSK]IN |[%s]\n", __func__);
	MSG_MED("[RTDSK]   |fp[0x%08X]\n", (u32)fp);
	MSG_MED("[RTDSK]   |command[0x%08X]\n", (u32)cmd);
	MSG_MED("[RTDSK]   |argument[0x%08X]\n", (u32)arg);

	task->flags |= PF_FREEZER_SKIP;
	memset(&ioctl_data, 0, sizeof(ioctl_data));

	if (0 != arg) {
		/* Check validation of user space pointer */
		if (!access_ok(VERIFY_READ, (void __user *)arg,
				_IOC_SIZE(cmd))) {
			ret = SMAP_NG;
			MSG_ERROR("[RTDSK]ERR| parameter error\n");
			MSG_MED("[RTDSK]OUT|[%s]ret = %d\n", __func__, ret);
			return ret;
		}

		/* Copy data from user space to kernel space */
		if (copy_from_user(&ioctl_data, (int __user *)arg,
			sizeof(ioctl_data))) {
			ret = SMAP_NG;
			MSG_ERROR("[RTDSK]ERR| copy_from_user error\n");
			MSG_MED("[RTDSK]OUT|[%s]ret = %d\n", __func__, ret);
			return ret;
		}
		MSG_LOW("[RTDSK]   |ioctl_data->size[%d]\n",
			ioctl_data.data_size);
	}

	switch (cmd) {
	case IOCTL_MEM_CMD_WR_SET_MPRO:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_SET_MPRO\n");

		if (RTDS_MEM_MPRO_ACTIVE == g_rtds_memory_mpro_startup_flag) {
			MSG_MED("[RTDSK]   |MPRO is already activated.\n");
			break;
		}

		/* Start Mpro */
		ret = rtds_memory_drv_init_mpro(fp);
		if (SMAP_OK == ret) {
			g_rtds_memory_mpro_startup_flag = RTDS_MEM_MPRO_ACTIVE;
			MSG_MED("[RTDSK]   |Start MPRO.\n");
			while (1) {
				rtds_memory_check_shared_apmem(fp,
					&(rtds_memory_data_p->mem_map));
			}
		} else {
			MSG_ERROR("[RTDSK]ERR|" \
				  " rtds_memory_drv_init_mpro failed ret[%d]\n",
				  ret);
			panic("[RTDSK]ERR|" \
			      "[%s][%d]rtds_memory_drv_init_mpro failed[%d]\n",
			      __func__, __LINE__, ret);
		}

		break;

	case IOCTL_MEM_CMD_WR_INIT:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_INIT\n");

		/* Initialise RTDS memory handle parameter */
		ret = rtds_memory_ioctl_init_data(fp,
			(void __user *)ioctl_data.data, ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_EXIT:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_EXIT\n");

		/* Unmap shared RT memory */
		if (0 != rtds_memory_data_p->var_app_addr) {
			(void)rtds_memory_close_shared_rtmem(
					rtds_memory_data_p->var_app_addr,
					rtds_memory_data_p->var_area_len);
		}
		break;

	case IOCTL_MEM_CMD_WR_OPEN_MEMORY:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_OPEN_MEMORY\n");

		/* Create App shared memory */
		ret = rtds_memory_ioctl_open_apmem(fp,
						(void __user *)ioctl_data.data,
						ioctl_data.data_size,
						&(rtds_memory_data_p->mem_map)
						);
		break;

	case IOCTL_MEM_CMD_WR_CLOSE_MEMORY:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_CLOSE_MEMORY\n");

		/* Close App shared memory */
		ret = rtds_memory_ioctl_close_apmem(
			(void __user *)ioctl_data.data, ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_SHARE_MEMORY:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_SHARE_MEMORY\n");

		/* Share App shared memory */
		ret = rtds_memory_ioctl_share_apmem(fp,
						(void __user *)ioctl_data.data,
						ioctl_data.data_size,
						&(rtds_memory_data_p->mem_map)
						);
		break;

	case IOCTL_MEM_CMD_WR_FLUSH_CACHE:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_FLUSH_CACHE\n");

		ret = rtds_memory_ioctl_cache_flush(
						(void __user *)ioctl_data.data,
						ioctl_data.data_size
						);
		break;

	case IOCTL_MEM_CMD_WR_CLEAR_CACHE:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_CLEAR_CACHE\n");

		ret = rtds_memory_ioctl_cache_clear(
						(void __user *)ioctl_data.data,
						ioctl_data.data_size
						);
		break;

	case IOCTL_MEM_CMD_WR_RTMAP:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_RTMAP\n");

		ret = rtds_memory_ioctl_map_mpro((void __user *)ioctl_data.data,
						ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_RTUNMAP:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_RTUNMAP\n");

		ret = rtds_memory_ioctl_unmap_mpro(
						(void __user *)ioctl_data.data,
						ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_RTMAP_PNC:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_RTMAP_PNC\n");

		ret = rtds_memory_ioctl_map_pnc_mpro(
						(void __user *)ioctl_data.data,
						ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_RTUNMAP_PNC:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_RTUNMAP_PNC\n");

		ret = rtds_memory_ioctl_unmap_pnc_mpro(
						(void __user *)ioctl_data.data,
						ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_RTMAP_PNC_NMA:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_RTMAP_PNC_NMA\n");

		ret = rtds_memory_ioctl_map_pnc_nma_mpro(
						(void __user *)ioctl_data.data,
						ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_GET_MEMSIZE:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_GET_MEMSIZE\n");

		/* Get App shared memory size */
		ret = rtds_memory_ioctl_get_memsize(
						(void __user *)ioctl_data.data,
						ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_GET_PAGESINFO:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_GET_PAGESINFO\n");

		/* Get memory of page descriptor */
		ret = rtds_memory_ioctl_get_pagesinfo(
						(void __user *)ioctl_data.data,
						ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_PHYMEM:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_PHYMEM\n");
		ret = rtds_memory_ioctl_change_phymem_address(
						(void __user *)ioctl_data.data,
						ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_CHANGE_PHY_PMB:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_CHANGE_PHY_PMB\n");
		{
			system_mem_phy_change_rtpmbaddr phy_change_rtaddr;
			if (copy_from_user(&phy_change_rtaddr,
				(int __user *)ioctl_data.data,
				sizeof(phy_change_rtaddr))) {
				MSG_ERROR("[RTDSK]ERR| copy_from_user (L:%d)\n",
					 __LINE__);
				ret = SMAP_NG;
				break;
			}
			ret = system_memory_phy_change_rtpmbaddr(
							&phy_change_rtaddr);
			if (copy_to_user((int __user *)ioctl_data.data,
				&phy_change_rtaddr, ioctl_data.data_size)) {
				MSG_ERROR("[RTDSK]ERR| copy_to_user (L:%d)\n",
					__LINE__);
			}
		}
		break;

	case IOCTL_MEM_CMD_WR_CHANGE_PMB_PHY:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_CHANGE_PMB_PHY\n");
		{
			system_mem_rtpmb_change_phyaddr rt_change_phyaddr;
			if (copy_from_user(&rt_change_phyaddr,
					   (int __user *)ioctl_data.data,
					   sizeof(rt_change_phyaddr))) {
				MSG_ERROR("[RTDSK]ERR| copy_from_user (L:%d)\n",
					__LINE__);
				ret = SMAP_NG;
				break;
			}
			ret = system_memory_rtpmb_change_phyaddr(
							&rt_change_phyaddr);
			if (copy_to_user((int __user *)ioctl_data.data,
				&rt_change_phyaddr, ioctl_data.data_size)) {
				MSG_ERROR("[RTDSK]ERR| copy_to_user (L:%d)\n",
					__LINE__);
			}
		}
		break;
#ifdef RTDS_SUPPORT_CMA
	case IOCTL_MEM_CMD_WR_ALLOC_CMA:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_ALLOC_CMA\n");

		ret = rtds_memory_ioctl_alloc_cma(fp,
					(void __user *)ioctl_data.data,
					ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_FREE_CMA:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_FREE_CMA\n");

		ret = rtds_memory_ioctl_free_cma(
					(void __user *)ioctl_data.data,
					ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_RTMAP_MA:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_RTMAP_MA\n");

		ret = rtds_memory_ioctl_map_mpro_ma(
					(void __user *)ioctl_data.data,
					ioctl_data.data_size);
		break;

	case IOCTL_MEM_CMD_WR_RTUNMAP_MA:
		MSG_MED("[RTDSK]   |IOCTL_MEM_CMD_WR_RTUNMAP_MA\n");

		ret = rtds_memory_ioctl_unmap_mpro_ma(
					(void __user *)ioctl_data.data,
					ioctl_data.data_size);
		break;
#endif
	default:
		MSG_ERROR("[RTDSK]ERR| No command\n");

		ret = SMAP_NG;
		break;
	}

	if (SMAP_OK != ret)
		MSG_ERROR("[RTDSK]ERR| ioctl function error\n");

	MSG_MED("[RTDSK]OUT|[%s]ret = %d\n", __func__, ret);
	return ret;
}

/*****************************************************************************
 * Function   : rtds_memory_drv_mapping
 * Description: This function does mapping.
 * Parameters : inode	-   inode information
 *		fp	-   file descriptor
 * Returns    : 0	-   Success(always)
 *
 *****************************************************************************/
int rtds_memory_drv_mapping(
		struct file		*fp,
		struct vm_area_struct	*vm_area
)
{
	int			ret;
	rtds_memory_drv_data	*rtds_memory_data_p = fp->private_data;
	MSG_MED("[RTDSK]IN |[%s]\n", __func__);
	MSG_MED("[RTDSK]   |fp[0x%08X]\n", (unsigned int)fp);
	MSG_MED("[RTDSK]   |vm_pgoff[0x%08X]\n",
		(unsigned int)vm_area->vm_pgoff);

	/* Mapping memory */
	ret = rtds_memory_map_shared_memory(&(rtds_memory_data_p->mem_map),
						vm_area);
	/*
	 * Always return 0, because this function is called by OS.
	 */
	MSG_MED("[RTDSK]OUT|[%s]ret = SMAP_OK\n", __func__);
	return 0;
}

/*****************************************************************************
 * Function   : rtds_memory_thread_apmem_rttrig
 * Description: This function is asynchronous receive thread from RT domain.
 * Parameters : none
 * Returns    : SMAP_OK -   Success
 *****************************************************************************/
int rtds_memory_thread_apmem_rttrig(
		void	*vp
)
{
	int				ret = SMAP_OK;
	rtds_memory_rcv_event_queue	*recv_queue;

	MSG_MED("[RTDSK]IN |[%s]\n", __func__);

	while (1) {
		/* Release semaphore */
		up(&g_rtds_memory_apmem_rttrig_sem);

		/* Wait for completion from RT Domain */
		wait_for_completion(&g_rtds_memory_completion);

		/* Get receive event */
		recv_queue = rtds_memory_get_recv_queue();
		if (NULL == recv_queue) {
			MSG_ERROR("[RTDSK]ERR| receive queue is NULL\n");
			panic("[RTDSK]ERR|[%s][%d]receive queue is NULL\n",
				__func__, __LINE__);
		} else {
			MSG_LOW("[RTDSK]   |event     [0x%08X]\n",
				recv_queue->event);
			MSG_LOW("[RTDSK]   |mem_size  [0x%08X]\n",
				recv_queue->mem_size);
			MSG_LOW("[RTDSK]   |rt_cache  [0x%08X]\n",
				recv_queue->rt_cache);
			MSG_LOW("[RTDSK]   |rt_trigger[0x%08X]\n",
				recv_queue->rt_trigger);
			MSG_LOW("[RTDSK]   |apmem_id  [0x%08X]\n",
				recv_queue->apmem_id);
			MSG_LOW("[RTDSK]   |leak_data [0x%08X]\n",
				(u32)recv_queue->leak_data);
			MSG_LOW("[RTDSK]   |leak_size [0x%08X]\n",
				recv_queue->leak_size);
#ifdef RTDS_SUPPORT_CMA
			MSG_LOW("[RTDSK]   |phys_addr [0x%08X]\n",
				recv_queue->phys_addr);
			MSG_LOW("[RTDSK]   |mem_attr  [0x%08X]\n",
				recv_queue->mem_attr);
#endif

			switch (recv_queue->event) {
			case RTDS_MEM_DRV_EVENT_APMEM_OPEN:
				MSG_MED("[RTDSK]   |" \
					"RTDS_MEM_DRV_EVENT_APMEM_OPEN\n");
				/* Open App shared memory */
				ret = rtds_memory_open_rttrig_shared_apmem(
					recv_queue->mem_size,
					recv_queue->rt_cache,
					recv_queue->rt_trigger);
				if (SMAP_OK != ret) {
					MSG_ERROR("[RTDSK]ERR| rtds_memory_" \
						"open_rttrig_shared_apmem " \
						"failed ret[%d]\n", ret);
					panic("[RTDSK]ERR|[%s][%d]rtds_memory" \
						"_open_rttrig_shared_apmem" \
						" failed[%d]\n",
						__func__, __LINE__, ret);
				}
				break;

			case RTDS_MEM_DRV_EVENT_APMEM_CLOSE:
				MSG_MED("[RTDSK]   |" \
					"RTDS_MEM_DRV_EVENT_APMEM_CLOSE\n");
				/* Close App shared memory */
				ret = rtds_memory_close_rttrig_shared_apmem(
					recv_queue->apmem_id);
				if (SMAP_OK != ret) {
					MSG_ERROR("[RTDSK]ERR| rtds_memory_" \
						"close_rttrig_shared_apmem " \
						"failed ret[%d]\n", ret);
					panic("[RTDSK]ERR|[%s][%d]rtds_memory" \
						"_close_rttrig_shared_apmem" \
						" failed[%d]\n",
						 __func__, __LINE__, ret);
				}
				break;

			case RTDS_MEM_DRV_EVENT_APMEM_DELETE:
				MSG_MED("[RTDSK]   |" \
					"RTDS_MEM_DRV_EVENT_APMEM_DELETE\n");

				break;

			case RTDS_MEM_DRV_EVENT_FATAL:
				MSG_MED("[RTDSK]   |" \
					"RTDS_MEM_DRV_EVENT_FATAL\n");
				rtds_memory_dump_mpro();
				break;

			case RTDS_MEM_DRV_EVENT_DELETE_LEAK_MEM:
				MSG_MED("[RTDSK]   |" \
					"RTDS_MEM_DRV_EVENT_DELETE_LEAK_MEM\n");
				/* Delete leak memory */
				(void)rtds_memory_delete_rttrig_leak_memory(
					recv_queue->leak_data,
					recv_queue->leak_size);
				break;


#ifdef RTDS_SUPPORT_CMA
			case RTDS_MEM_DRV_EVENT_CMA_OPEN:
				MSG_MED("[RTDSK]   |" \
					"RTDS_MEM_DRV_EVENT_CMA_OPEN\n");
				ret = rtds_memory_open_rttrig_cma(
					recv_queue->mem_size,
					recv_queue->map_id,
					recv_queue->mem_attr,
					recv_queue->rt_trigger);
				if (SMAP_OK != ret) {
					MSG_ERROR("[RTDSK]ERR| rtds_memory" \
						"_open_rttrig_cma[%d]\n", ret);
					panic("[RTDSK]ERR|[%s][%d]rtds_memory" \
						"_open_rttrig_cma failed[%d]\n",
						__func__, __LINE__, ret);
				}

				break;

			case RTDS_MEM_DRV_EVENT_CMA_CLOSE:
				MSG_MED("[RTDSK]   |" \
					"RTDS_MEM_DRV_EVENT_CMA_CLOSE\n");
				ret = rtds_memory_close_rttrig_cma(
					recv_queue->apmem_id);
				if (SMAP_OK != ret) {
					MSG_ERROR("[RTDSK]ERR| rtds_memory" \
						"_close_rttrig_cma[%d]\n", ret);
					panic("[RTDSK]ERR|[%s][%d]rtds_memory" \
						"_close_rttrig_cma failed[%d]\n"
						, __func__, __LINE__, ret);
				}
				break;

			case RTDS_MEM_DRV_EVENT_APMEM_MAP:
				MSG_MED("[RTDSK]   |" \
					"RTDS_MEM_DRV_EVENT_APMEM_MAP\n");
				ret = rtds_memory_map_rttrig_shared_apmem(
					recv_queue->phys_addr,
					recv_queue->mem_size,
					recv_queue->rt_cache,
					recv_queue->rt_trigger);
				if (SMAP_OK != ret) {
					MSG_ERROR("[RTDSK]ERR| rtds_memory" \
						"_map_rttrig_shared_apmem[%d]\n"
						, ret);
					panic("[RTDSK]ERR|[%s][%d]rtds_memory" \
						"_map_rttrig_shared_apmem" \
						" failed[%d]\n",
						__func__, __LINE__, ret);
				}
				break;

			case RTDS_MEM_DRV_EVENT_APMEM_UNMAP:
				MSG_MED("[RTDSK]   |" \
					"RTDS_MEM_DRV_EVENT_APMEM_UNMAP\n");
				ret = rtds_memory_unmap_rttrig_shared_apmem(
					recv_queue->apmem_id);
				if (SMAP_OK != ret) {
					MSG_ERROR("[RTDSK]ERR|" \
						" rtds_memory_unmap_rttrig" \
						"_shared_apmem failed ret[%d]\n"
						, ret);
					panic("[RTDSK]ERR|[%s][%d]rtds_memory" \
						"_unmap_rttrig_shared_apmem" \
						" failed[%d]\n",
						__func__, __LINE__, ret);
				}
				break;
#endif
			default:
				MSG_ERROR("[RTDSK]ERR| No EVENT[0x%08X]\n",
					recv_queue->event);
				panic("[RTDSK]ERR|" \
					" thread_apmem illegal event\n");
				break;
			}
			/* Delete receive event */
			rtds_memory_delete_recv_queue(recv_queue);
		}
	}

	MSG_MED("[RTDSK]OUT|[%s]ret = SMAP_OK\n", __func__);
	return ret;
}

static int rtds_mem_procfile_read(
	char *page, char **start, off_t off, int count, int *eof, void *data)
{
	rtds_memory_app_memory_table *m_table = NULL;
	struct list_head l_list;
	struct rtds_mem_dump_info *dump_list;
	struct rtds_mem_dump_info *next;
	bool find;
	unsigned int sum = 0;
	char comm[256];
	char *cmdline = comm;
	int n;

	INIT_LIST_HEAD(&l_list);
	RTDS_MEM_DOWN_TIMEOUT(&g_rtds_memory_shared_mem);

	list_for_each_entry(m_table,
				&g_rtds_memory_list_shared_mem, list_head) {
		find = false;
		list_for_each_entry(dump_list, &l_list, list) {
			if (m_table->task_info->tgid == dump_list->tgid) {
				dump_list->size += m_table->memory_size;
				find = true;
				break;
			}
		}
#if 1 /* VSS */
		if (!find) {
#else /* RSS */
		if ((!find) &&
		    ((m_table->event == RTDS_MEM_RT_CREATE_EVENT) ||
		    (m_table->event == RTDS_MEM_MAP_PNC_EVENT) ||
		    (m_table->event == RTDS_MEM_MAP_PNC_NMA_EVENT))) {
#endif
			dump_list = kmalloc(sizeof(struct rtds_mem_dump_info),
					    GFP_KERNEL);
			if (NULL == dump_list)
				goto error;
			dump_list->size = m_table->memory_size;
			dump_list->tgid = m_table->task_info->tgid;
			dump_list->task_info = m_table->task_info;
			if ((m_table->rt_nc_addr) &&
			    (m_table->event != RTDS_MEM_MAP_EVENT))
				dump_list->size += m_table->memory_size;
			list_add_tail(&(dump_list->list), &l_list);
		}
	}

	up(&g_rtds_memory_shared_mem);

	/* show dump info */
	count = snprintf(page, PAGE_SIZE, "%-4s %7s %s\n",
			"TGID", "SIZE", "NAME");
	if (count < 0)
		goto error;
	list_for_each_entry_safe(dump_list, next, &l_list, list) {

		if (rtds_get_cmdline(dump_list->tgid, comm, sizeof(comm)))
			cmdline = comm;
		else
			cmdline = dump_list->task_info->comm;

		n = snprintf(page + count,
			     PAGE_SIZE - count,
			     "%4d %6dK %s\n",
			     (u32)dump_list->tgid,
			     (dump_list->size / SZ_1K),
			     cmdline);
		if (n < 0)
			goto error;
		count += n;
		sum += dump_list->size;
		list_del(&(dump_list->list));
		kfree(dump_list);
	}
	n = snprintf(page + count, PAGE_SIZE - count, "%-4s-%-8s-%s\n",
			"----", "--------", "----");
	if (n < 0)
		goto error;
	count += n;
	n = snprintf(page + count, PAGE_SIZE - count, "%-4s %6dK %s\n",
			"", (sum / SZ_1K), "TOTAL");
	if (n < 0)
		goto error;
	count += n;

	return count;
error:
	list_for_each_entry_safe(dump_list, next, &l_list, list) {
		list_del(&(dump_list->list));
		kfree(dump_list);
	}
	return count;
}

#if defined(CONFIG_RTDS_LMK)

static unsigned long rtds_mem_check_timeout;

int rtds_mem_check_to_lmk(int minfree, int adj)
{
	rtds_pid_table*	pid_table = NULL;
	int task_num = 0;
	int current_adj = 0;
	int current_size = 0;
	struct task_struct* task_info;
	/*int i = 0;*/

	if (time_before_eq(jiffies, rtds_mem_check_timeout))
	{
		return 0;
	}

	rtds_mem_check_timeout = jiffies + (HZ*4);

	if(down_trylock(&g_rtds_memory_shared_mem))
	{
		printk(KERN_EMERG"nus: failed get sema \n");
		return 0;
	}
	/*RTDS_MEM_DOWN_TIMEOUT(&g_rtds_memory_shared_mem);*/

	list_for_each_entry(pid_table, &g_rtds_process_list, head) {
		/* defense code */
		task_info = NULL;
		task_info = find_task_by_vpid(pid_table->tgid);
		if(!task_info){
			printk(KERN_ERR "%s : task is not exist !!\n", __func__);
			continue;
		}

		if((pid_table->size / SZ_1K) >= minfree && pid_table->size >= current_size && 
	    	task_info->signal->oom_score_adj >= adj &&
			task_info->signal->oom_score_adj >= current_adj)
		{
			task_num = (int)(pid_table->tgid);
			current_adj = task_info->signal->oom_score_adj;
			current_size = pid_table->size;
		}
		/*printk(KERN_EMERG"nus:[%d] %d : %s : %d\n", ++i, pid_table->tgid, task_info->comm, (pid_table->size)/1024 );*/
	}
	up(&g_rtds_memory_shared_mem);

	return task_num;
	
}

#endif

static int rtds_mem_procinfo_mpro_open(
	struct inode *inode, struct file *file);
static void *mproinfo_s_start(
	struct seq_file *m, loff_t *pos);
static void *mproinfo_s_next(struct seq_file *m, void *p, loff_t *pos);
static void mproinfo_s_stop(struct seq_file *m, void *p);
static int mproinfo_s_show(struct seq_file *m, void *p);


static const struct file_operations g_procinfo_mpro_op = {
	.open		= rtds_mem_procinfo_mpro_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static const struct seq_operations g_rtds_mem_mpro_seq_op = {
	.start	= mproinfo_s_start,
	.next	= mproinfo_s_next,
	.stop	= mproinfo_s_stop,
	.show	= mproinfo_s_show,
};

static int rtds_mem_procinfo_mpro_open(
	struct inode *inode, struct file *file)
{
	return seq_open(file, &g_rtds_mem_mpro_seq_op);
}

static void *mproinfo_s_start(
	struct seq_file *m, loff_t *pos)
{
	rtds_memory_app_memory_table *m_table;
	struct list_head *list;
	int i;

	MSG_MED("[%s]%d\n", __func__, (u32)*pos);

	if (*pos == 0)
		seq_printf(m, "%-4s %-4s %3s %-10s %-10s %-10s %7s %s\n",
			 "PID", "TGID", "ID", "WB_ADDR", "NC_ADDR",
			 "PHY_ADDR", "SIZE", "NAME");

	RTDS_MEM_DOWN_TIMEOUT(&g_rtds_memory_shared_mem);

	if (list_empty(&g_rtds_memory_list_shared_mem))
		return NULL;

	list = g_rtds_memory_list_shared_mem.next;

	m_table = list_entry(list,
			     rtds_memory_app_memory_table,
			     list_head);
	for (i = 0; i < *pos; i++) {
		m_table = list_entry(list,
				     rtds_memory_app_memory_table,
				     list_head);
		if ((u32)m_table == (u32)&g_rtds_memory_list_shared_mem)
			return NULL;
		list = m_table->list_head.next;
	}

	return m_table;
}

static void *mproinfo_s_next(struct seq_file *m, void *p, loff_t *pos)
{
	rtds_memory_app_memory_table *m_table;
	struct list_head *list;

	++*pos;
	m_table = (rtds_memory_app_memory_table *)p;
	list = m_table->list_head.next;

	if ((u32)list == (u32)&g_rtds_memory_list_shared_mem) {
		MSG_MED("[%s]END!!\n", __func__);
		return NULL;
	}
	m_table = list_entry(list,
			     rtds_memory_app_memory_table,
			     list_head);

	return m_table;
}

static void mproinfo_s_stop(struct seq_file *m, void *p)
{
	MSG_MED("[%s]\n", __func__);
	up(&g_rtds_memory_shared_mem);
}

static int mproinfo_s_show(struct seq_file *m, void *p)
{
	rtds_memory_app_memory_table *m_table;
	char comm[256];
	char *cmdline = comm;
	unsigned int phys_addr;
	unsigned int nc_addr;
	unsigned int nc_mem_size = 0;

	m_table = (rtds_memory_app_memory_table *)p;
	if (rtds_get_cmdline(m_table->task_info->pid, comm, sizeof(comm)))
		cmdline = comm;
	else
		cmdline = m_table->task_info->comm;

	if (m_table->event == RTDS_MEM_MAP_EVENT) {
		nc_addr = 0;
		phys_addr = m_table->rt_nc_addr;
	} else {
		nc_addr = m_table->rt_nc_addr;
		phys_addr = m_table->phys_addr;
	}

	if ((m_table->rt_nc_addr) &&
	    (m_table->event != RTDS_MEM_MAP_EVENT))
		nc_mem_size = m_table->memory_size;

	seq_printf(m, "%4d %4d %3d 0x%08x 0x%08x 0x%08x %6dK %s\n",
			(u32)m_table->task_info->pid,
			(u32)m_table->task_info->tgid,
			m_table->apmem_id,
			(u32)m_table->rt_wb_addr,
			(u32)nc_addr,
			(u32)phys_addr,
			(u32)((m_table->memory_size + nc_mem_size) / SZ_1K),
			cmdline);
	return 0;
}

static int rtds_get_cmdline(
	pid_t pid, char *buf, int len
)
{
	char cmdline_name[255];
	struct file *fp = NULL;
	unsigned int size;

	if (len <= 0)
		goto error;

	if (sprintf(cmdline_name, "/proc/%d/cmdline", pid) < 0)
		goto error;

	fp = filp_open(cmdline_name, O_RDONLY, 0);
	if (IS_ERR(fp))
		goto error;

	size = kernel_read(fp, 0, buf, len);
	filp_close(fp, NULL);
	return size;

error:
	size = sprintf(buf, "unknown");
	if (fp && !IS_ERR(fp))
		filp_close(fp, NULL);
	return size;
}


/*****************************************************************************
 * Function   : rtds_memory_init_module
 * Description: This function initialise RTDS MEMORY driver.
 * Parameters : none
 * Returns    : SMAP_OK -   Success
 *		SMAP_NG -   Fatal error
 *****************************************************************************/
int rtds_memory_init_module(
		void
)
{
	int				ret;
	iccom_drv_init_param		iccom_init;
	iccom_drv_cleanup_param		iccom_cleanup;
	get_section_header_param	section;
	system_rt_section_header	section_header;
	iccom_drv_send_cmd_param	send_cmd;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	/* Register device driver */
	g_rtds_memory_device.name	= RTDS_MEMORY_DRIVER_NAME;
	g_rtds_memory_device.fops	= &g_rtds_memory_fops;
	g_rtds_memory_device.minor	= MISC_DYNAMIC_MINOR;
	ret = misc_register(&g_rtds_memory_device);
	if (0 != ret) {
		MSG_ERROR("[RTDSK]ERR| misc_register failed ret[%d]\n", ret);
		MSG_HIGH("[RTDSK]OUT|[%s]ret = SMAP_NG\n", __func__);
		return SMAP_NG;
	}

	memset(&iccom_init, 0, sizeof(iccom_init));
	/* Set callback for asynchronous response */
	iccom_init.comp_notice = rtds_memory_rcv_comp_notice;
	g_rtds_memory_mpro_startup_flag = RTDS_MEM_MPRO_INACTIVE;

	/* Create ICCOM driver handle */
	g_rtds_memory_iccom_handle = iccom_drv_init(&iccom_init);
	if (NULL == g_rtds_memory_iccom_handle) {
		MSG_ERROR("[RTDSK]ERR| iccom_drv_init failed\n");
		ret = misc_deregister(&g_rtds_memory_device);
		if (0 != ret)
			MSG_ERROR("[RTDSK]ERR|" \
				  " misc_deregister failed ret[%d]\n", ret);

		MSG_HIGH("[RTDSK]OUT|[%s]ret = SMAP_NG\n", __func__);
		return SMAP_NG;
	}

	/* Initialise section info */
	memset(&g_rtds_memory_section_info, 0,
		sizeof(g_rtds_memory_section_info));

	/* Initialise LIST head */
	memset(&g_rtds_memory_list_rcv_event, 0,
		sizeof(g_rtds_memory_list_rcv_event));
	memset(&g_rtds_memory_list_create_mem, 0,
		sizeof(g_rtds_memory_list_create_mem));
	memset(&g_rtds_memory_list_mpro, 0,
		sizeof(g_rtds_memory_list_mpro));
	memset(&g_rtds_memory_list_shared_mem, 0,
		sizeof(g_rtds_memory_list_shared_mem));
	memset(&g_rtds_process_list, 0,
		sizeof(g_rtds_process_list));
	memset(&g_rtds_memory_list_leak_mpro, 0,
		sizeof(g_rtds_memory_list_leak_mpro));
	memset(&g_rtds_memory_list_map_rtmem, 0,
		sizeof(g_rtds_memory_list_map_rtmem));
	memset(&g_rtds_memory_list_reg_phymem, 0,
		sizeof(g_rtds_memory_list_reg_phymem));

	INIT_LIST_HEAD(&g_rtds_memory_list_rcv_event);
	INIT_LIST_HEAD(&g_rtds_memory_list_create_mem);
	INIT_LIST_HEAD(&g_rtds_memory_list_mpro);
	INIT_LIST_HEAD(&g_rtds_memory_list_shared_mem);
	INIT_LIST_HEAD(&g_rtds_process_list);
	INIT_LIST_HEAD(&g_rtds_memory_list_leak_mpro);
	INIT_LIST_HEAD(&g_rtds_memory_list_map_rtmem);
	INIT_LIST_HEAD(&g_rtds_memory_list_reg_phymem);
#ifdef RTDS_SUPPORT_CMA
	INIT_LIST_HEAD(&g_rtds_memory_list_cma);
#endif

	/* Initialise spin_lock */
	spin_lock_init(&g_rtds_memory_lock_recv_queue);
	spin_lock_init(&g_rtds_memory_lock_cache_all);
	spin_lock_init(&g_rtds_memory_lock_create_mem);
	spin_lock_init(&g_rtds_memory_lock_mpro);
	spin_lock_init(&g_rtds_memory_lock_map_rtmem);
#ifdef RTDS_SUPPORT_CMA
	spin_lock_init(&g_rtds_memory_lock_cma);
#endif

	/* Initialise completion */
	init_completion(&g_rtds_memory_completion);

	/* Initialise semapore */
	init_MUTEX(&g_rtds_memory_apmem_rttrig_sem);
	init_MUTEX_LOCKED(&g_rtds_memory_mpro_sem);
	init_MUTEX(&g_rtds_memory_shared_mem);
	init_MUTEX(&g_rtds_memory_phy_mem);
	init_MUTEX(&g_rtds_memory_send_sem);

	/* Convert timeout value */
	g_rtds_memory_sem_jiffies = msecs_to_jiffies(RTDS_MEM_WAIT_TIMEOUT);

	/* Get section info */
	section.section_header = &section_header;
	ret = sys_get_section_header(&section);
	if (SMAP_OK != ret) {
		MSG_ERROR("[RTDSK]ERR| sys_get_section_header failed[%d]\n",
				ret);
		goto out;
	}

	g_rtds_memory_section_info.var_address	= section_header.memmpl_address;
	g_rtds_memory_section_info.var_length	= section_header.memmpl_size;
	g_rtds_memory_section_info.sh_pmb_offset = section_header.sh_pmb_offset;
	g_rtds_memory_section_info.sh_pmb_nc_offset =
						section_header.sh_pmb_nc_offset;
	g_rtds_memory_section_info.mfi_pmb_offset =
						section_header.mfi_pmb_offset;

	/* I/O remap */
	g_rtds_memory_section_info.kernel_var_addr = ioremap_nocache(
					g_rtds_memory_section_info.var_address,
					g_rtds_memory_section_info.var_length);
	if (NULL == g_rtds_memory_section_info.kernel_var_addr) {
		MSG_ERROR("[RTDSK]ERR| ioremap_nocache failed\n");
		goto out;
	}

	MSG_MED("[RTDSK]   |addr(section)[0x%08X]\n",
		(u32)g_rtds_memory_section_info.var_address);
	MSG_MED("[RTDSK]   |length(section)[0x%08X]\n",
		(u32)g_rtds_memory_section_info.var_length);
	MSG_MED("[RTDSK]   |addr(kernel)[0x%08X]\n",
		(u32)g_rtds_memory_section_info.kernel_var_addr);

	/*
	 *   Register asynchronous event to RT domain
	 */

	/* send EVENT_MEMORY_OPENAPPMEMORY */
	send_cmd.handle		= g_rtds_memory_iccom_handle;
	send_cmd.task_id	= TASK_MEMORY;
	send_cmd.function_id	= EVENT_MEMORY_OPENAPPMEMORY;
	send_cmd.send_mode	= ICCOM_DRV_ASYNC;
	send_cmd.send_size	= 0;
	send_cmd.send_data	= NULL;
	send_cmd.recv_size	= 0;
	send_cmd.recv_data	= NULL;

	ret = iccom_drv_send_command(&send_cmd);
	if (SMAP_OK != ret) {
		MSG_ERROR("[RTDSK]ERR|send error" \
			  "[EVENT_MEMORY_OPENAPPMEMORY]\n");
		goto out;
	}

	MSG_LOW("[RTDSK]   |Send [EVENT_MEMORY_OPENAPPMEMORY]\n");


	/* send EVENT_MEMORY_CLOSEAPPMEMORY
	 * It does not change send_command parameter as follows.
	 *  send_cmd.handle      = g_rtds_memory_iccom_handle;
	 *  send_cmd.task_id     = TASK_MEMORY;
	 *  send_cmd.send_mode   = ICCOM_DRV_ASYNC;
	 *  send_cmd.send_size   = 0;
	 *  send_cmd.send_data   = NULL;
	 *  send_cmd.recv_size   = 0;
	 *  send_cmd.recv_data   = NULL;
	 */
	send_cmd.function_id = EVENT_MEMORY_CLOSEAPPMEMORY;
	ret = iccom_drv_send_command(&send_cmd);
	if (SMAP_OK != ret) {
		MSG_ERROR("[RTDSK]ERR|send error" \
			  "[EVENT_MEMORY_CLOSEAPPMEMORY]\n");
		goto out;
	}
	MSG_LOW("[RTDSK]   |Send [EVENT_MEMORY_CLOSEAPPMEMORY]\n");

#ifdef RTDS_SUPPORT_CMA
	/* send EVENT_MEMORY_OPERATECTGMEMORY
	 * It does not change send_command parameter as follows.
	 *  send_cmd.handle      = g_rtds_memory_iccom_handle;
	 *  send_cmd.task_id     = TASK_MEMORY;
	 *  send_cmd.send_mode   = ICCOM_DRV_ASYNC;
	 *  send_cmd.send_size   = 0;
	 *  send_cmd.send_data   = NULL;
	 *  send_cmd.recv_size   = 0;
	 *  send_cmd.recv_data   = NULL;
	 */
	send_cmd.function_id = EVENT_MEMORY_OPERATECTGMEMORY;
	ret = iccom_drv_send_command(&send_cmd);
	if (SMAP_OK != ret) {
		MSG_ERROR("[RTDSK]ERR|send error" \
			  "[EVENT_MEMORY_OPERATECTGMEMORY]\n");
		goto out;
	}
	MSG_LOW("[RTDSK]   |Send [EVENT_MEMORY_OPERATECTGMEMORY]\n");
#endif

	/* Run asynchronous thread */
	g_rtds_memory_thread_info = kthread_run(rtds_memory_thread_apmem_rttrig,
						NULL,
						"rtds_memory_th_async");
	if (NULL == g_rtds_memory_thread_info) {
		MSG_ERROR("[RTDSK]ERR|kthread_run failed[NULL]\n");
		goto out;
	}

	g_rtds_memory_proc_entry = NULL;
	g_rtds_memory_proc_mem_info = NULL;
	g_rtds_memory_proc_mpro = NULL;
	/* 0644: S_IRUSR | S_IWUSR| S_IRGRP| S_IROTH */
	g_rtds_memory_proc_entry =
		proc_mkdir(RTDS_MEMORY_DRIVER_NAME, NULL);
	if (!g_rtds_memory_proc_entry) {
		MSG_ERROR("[RTDSK]ERR|L.%d Error creating proc entry\n",
			  __LINE__);
		goto out;
	}

	g_rtds_memory_proc_mem_info =
		create_proc_entry("mem_info",
				  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
				  g_rtds_memory_proc_entry);
	if (!g_rtds_memory_proc_mem_info) {
		MSG_ERROR("[RTDSK]ERR|L.%d Error creating proc entry\n",
			  __LINE__);
		goto out;
	}

	g_rtds_memory_proc_mem_info->read_proc = rtds_mem_procfile_read;
	g_rtds_memory_proc_mem_info->write_proc = NULL;

	g_rtds_memory_proc_mpro =
		create_proc_entry("maps",
				  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH,
				  g_rtds_memory_proc_entry);
	if (!g_rtds_memory_proc_mpro) {
		MSG_ERROR("[RTDSK]ERR|L.%d Error creating proc entry\n",
			  __LINE__);
		goto out;
	}

	g_rtds_memory_proc_mpro->proc_fops = &g_procinfo_mpro_op;

	MSG_HIGH("[RTDSK]OUT|[%s]ret = SMAP_OK\n", __func__);

	return SMAP_OK;
out:
	g_rtds_memory_section_info.var_address = 0;
	g_rtds_memory_section_info.var_length  = 0;
	g_rtds_memory_section_info.kernel_var_addr = NULL;
	iccom_cleanup.handle = g_rtds_memory_iccom_handle;
	iccom_drv_cleanup(&iccom_cleanup);
	ret = misc_deregister(&g_rtds_memory_device);
	if (0 != ret)
		MSG_ERROR("[RTDSK]ERR| misc_deregister failed ret[%d]\n", ret);
	if (g_rtds_memory_proc_mem_info)
		remove_proc_entry("mem_info", g_rtds_memory_proc_entry);
	if (g_rtds_memory_proc_entry)
		remove_proc_entry(RTDS_MEMORY_DRIVER_NAME, NULL);

	MSG_HIGH("[RTDSK]OUT|[%s]ret = SMAP_NG\n", __func__);

	return SMAP_NG;
}

/*****************************************************************************
 * Function   : rtds_memory_exit_module
 * Description: This function exits RTDS MEMORY driver.
 * Parameters : none
 * Returns    : none
 *
 *****************************************************************************/
void rtds_memory_exit_module(
		void
)
{
	int			ret;
	iccom_drv_cleanup_param	iccom_cleanup;

	MSG_HIGH("[RTDSK]IN |[%s]\n", __func__);

	/* Stop asynchronous thread */
	ret = kthread_stop(g_rtds_memory_thread_info);
	if (0 != ret)
		MSG_ERROR("[RTDSK]ERR| kthread_stop failed ret[%d]\n", ret);

	/* Cleanup iccom driver handle */
	iccom_cleanup.handle = g_rtds_memory_iccom_handle;
	iccom_drv_cleanup(&iccom_cleanup);

	/* Unregister device driver */
	ret = misc_deregister(&g_rtds_memory_device);
	if (0 != ret)
		MSG_ERROR("[RTDSK]ERR| misc_deregister failed ret[%d]\n", ret);

	remove_proc_entry("mem_info", g_rtds_memory_proc_entry);
	remove_proc_entry("maps", g_rtds_memory_proc_entry);
	remove_proc_entry(RTDS_MEMORY_DRIVER_NAME, NULL);

	MSG_HIGH("[RTDSK]OUT|[%s]\n", __func__);

	return;
}
module_init(rtds_memory_init_module);
module_exit(rtds_memory_exit_module);
