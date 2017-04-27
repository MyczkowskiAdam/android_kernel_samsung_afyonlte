/*
 * drivers/sec_hal/sec_hal_dev.c
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

#include "sec_hal_rt.h"
#include "sec_hal_rt_cmn.h"
#include "sec_hal_rt_trace.h"
#include "sec_hal_dev_ioctl.h"
#include "sec_hal_dev_info.h"
#include "sec_hal_cmn.h"
#include "sec_hal_mdm.h"
#include "sec_hal_toc.h"
#include "sec_hal_tee.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/wait.h>
#include <linux/ioport.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/timer.h>
#include <linux/io.h>


/* ********************************************************************
 * COMMON MACRO(s)
 * *******************************************************************/
#define ICRAM0_ADDRESS    0xE63A0000
#define ICRAM0_SIZE       4096
#define DEFAULT_WDT_VALUE 60000
#define SDTOC_SIZE        0x4000
#define SDTOC_ADDRESS     0x47FE0000
#define PUBLIC_TOC_SIZE   4096

#ifndef gic_spi
#define gic_spi(param)    (32 + param)
#endif


/* **********************************************************************
 * STRUCT(s)
 * *********************************************************************/
typedef struct {
	struct list_head list;
	void *virt_addr; /* virtual address */
	TEEC_SharedMemory *shmem;
} shared_memory_node;

typedef struct {
	struct list_head head;
} shared_memory_list;

struct device_data {
	struct class* class;
	struct cdev cdev;
	struct semaphore sem;
	struct platform_device *pdev;
};

struct client_data {
	struct device_data *device;
	void *drm_data; /* if non NULL then owned. */
	void *tee_data; /* if non NULL then owned. */
	TEEC_Context *teec_context;
	TEEC_SharedMemory *next_teec_shmem;
	void * next_teec_shmem_buffer;
	shared_memory_list shmem_list;
};


/* RunTime functions decl to higher level.(legacy funcs) */
long sec_hal_rt_ioctl(unsigned int cmd, void **data, sd_ioctl_params_t *param);


/* RPC functions decl to higher level */
ssize_t sec_hal_rpc_read(struct file *filp, char __user* buf,
		size_t count, loff_t *ppos);
ssize_t sec_hal_rpc_write(struct file *filp, const char *buf,
		size_t count, loff_t *ppos);
long sec_hal_rpc_ioctl(unsigned int cmd, void **data, sd_ioctl_params_t *param);


/* DRM functions decl to higher level */
void sec_hal_drm_usr_exit(void **drm_data);
long sec_hal_drm_ioctl(unsigned int cmd, void **data, sd_ioctl_params_t *param,
		struct platform_device *pdev);


/* CNF functions decl to higher level */
long sec_hal_cnf_ioctl(unsigned int cmd, void **data, sd_ioctl_params_t *param,
		struct platform_device *pdev);


/* TEE functions decl to higher level */
void sec_hal_tee_usr_exit(void **tee_data);
long sec_hal_tee_ioctl(unsigned int cmd, void **data, sd_ioctl_params_t *param,
		struct platform_device *pdev);


/* **********************************************************************
 * W/A(s)
 * *********************************************************************/
static inline int is_recovery(void)
{
	int i;
	char *c = &boot_command_line[0];

	if (c[0] && c[1] && c[2] && c[3]) {
		for (i=3; c[i]; i++)
			if (c[i-3] == 'a'
					&& c[i-2] == 'n'
					&& c[i-1] == 'd'
					&& c[i]   == 'r')
				return 0;
	} 

	return 1;
}


/* **********************************************************************
 * STATIC writable data
 * *********************************************************************/
static struct device_data g_device_data;
static sec_hal_public_id_t g_pub_id;
/* initial DBGREG values, for Rnd.
 * Eventually should be read from general purpose register
 * which are accessible by hw debuggers. */
#define TDBG_SPI 81U
static uint32_t g_dbgreg1 = 0x20000000;
static uint32_t g_dbgreg2 = 0x00;
static uint32_t g_dbgreg3 = 0x00078077;
DATA_TOC_ENTRY *sdtoc_root;
DATA_TOC_ENTRY *public_toc_root;
uint8_t *public_id;
uint8_t *prcf;
uint32_t public_id_size;
#define INIT_TIMER_MSECS 90000
static struct timer_list g_integ_timer;
static uint32_t g_banked_timeout;


/* **********************************************************************
 * USR space access
 * *********************************************************************/
/* **********************************************************************
 * Function name : sec_hal_usr_open
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
static int sec_hal_usr_open(struct inode *inode, struct file *filp)
{
	struct device_data *device;
	struct client_data *client;

	SEC_HAL_TRACE_ENTRY();

	if (nonseekable_open(inode, filp))
		return -ENODEV;

	device = container_of(inode->i_cdev, struct device_data, cdev);
	client = kmalloc(sizeof(struct client_data), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->device = device;
	client->drm_data = NULL;
	client->tee_data = NULL;

	/* Can not be removed until tee_data is fully in use */
	client->teec_context = NULL;
	client->next_teec_shmem = NULL;
	client->next_teec_shmem_buffer = NULL;
	INIT_LIST_HEAD(&(client->shmem_list.head));

	filp->private_data = client;

	SEC_HAL_TRACE_EXIT();
	return 0;
}


/* **********************************************************************
 * Function name : sec_hal_usr_release
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
static int sec_hal_usr_release(struct inode *inode, struct file *filp)
{
	struct client_data *client = filp->private_data;

	SEC_HAL_TRACE_ENTRY();

#ifdef CONFIG_ARM_SEC_HAL_TEE
	/* In case of crash client mem_nodes need to be freed here */
#if 0
	sec_hal_tee_usr_exit(&client->tee_data); /* abnormal exit usecase */
#endif
#endif /* CONFIG_ARM_SEC_HAL_TEE */
#ifdef CONFIG_ARM_SEC_HAL_DRM_WVN
	sec_hal_drm_usr_exit(&client->drm_data); /* abnormal exit usecase */
#endif /* CONFIG_ARM_SEC_HAL_DRM_WVN */
	kfree(client);

	SEC_HAL_TRACE_EXIT();
	return 0;
}


/* **********************************************************************
 * Function name : sec_hal_usr_ioctl
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
static long sec_hal_usr_ioctl(
	struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	long ret = (long) -EPERM;
	struct client_data *client = filp->private_data;
	struct device_data *device = client->device;
	sd_ioctl_params_t input;

	if (filp->f_flags & O_NONBLOCK)
		return -EPERM;

	if (copy_from_user(&input, (void __user*)arg, SD_IOCTL_PARAMS_SZ))
		return -EFAULT;

	if (down_interruptible(&device->sem))
		return -ERESTARTSYS;

	switch (cmd) {
	case SD_INIT:
		ret = 0;
		break;
	case SD_KEY_INFO:
	case SD_CERT_REGISTER:
	case SD_DATA_CERT_REGISTER:
	case SD_MAC_ADDRESS_GET:
	case SD_IMEI_GET:
	case SD_RAT_BAND_GET:
	case SD_PP_FLAGS_COUNT_GET:
	case SD_PP_FLAGS_GET:
	case SD_SL_LEVELS_OPEN:
	case SD_SL_LEVEL_OPEN:
	case SD_SL_LEVEL_STATUS_GET:
	case SD_AUTH_DATA_SIZE_GET:
	case SD_AUTH_DATA_GET:
	case SD_SELFTEST:
		ret = sec_hal_rt_ioctl(cmd, NULL, &input);
		break;
	case SD_PUBLIC_ID:
		ret = copy_to_user((void *)input.param0,
			g_pub_id.public_id,
			SEC_HAL_MAX_PUBLIC_ID_LENGTH);
		break;
#ifdef CONFIG_ARM_SEC_HAL_DRM_WVN
	case SD_DRM_ENTER_PLAY:
	case SD_DRM_EXIT_PLAY:
	case SD_DRM_SET_ENTIT_KEY:
	case SD_DRM_DER_CTL_WORD:
	case SD_DRM_DECRYPT_AUDIO:
	case SD_DRM_DECRYPT_VIDEO:
	case SD_DRM_VALID_KEYBOX:
	case SD_DRM_DEVICE_ID_GET:
	case SD_DRM_KEYDATA_GET:
	case SD_RANDOM_NUMBER_GET:
	case SD_DRM_WRAP_KEYBOX:
	case SD_DRM_INSTALL_KEYBOX:
		ret = sec_hal_drm_ioctl(cmd, &client->drm_data, &input, device->pdev);
		break;
#endif /* CONFIG_ARM_SEC_HAL_DRM_WVN */
	case SD_SEC_STORAGE_SELFTEST:
	case SD_SIMU_DS0_TEST:
	case SD_SIMU_DS1_TEST:
	case SD_SECURE_STORAGE_DAEMON_PID_REGISTER:
		ret = sec_hal_rpc_ioctl(cmd, NULL, &input);
		break;
	case SD_CNF_SIML:
	case SD_CNF_SIML_VALIDATE:
	case SD_CNF_SIML_VERIFY_DATA_GET:
	case SD_CNF_IMEI:
	case SD_CNF_MAC:
		ret = sec_hal_cnf_ioctl(cmd, NULL, &input, device->pdev);
		break;
#ifdef CONFIG_ARM_SEC_HAL_TEE
	case SD_TEE_INIT_CONTEXT: {
		uint32_t* name = (uint32_t *)input.param0;
		uint32_t* context = (uint32_t *)input.param1;

		if (client->teec_context) {
			SEC_HAL_TRACE("Context already open");
			ret = TEEC_ERROR_GENERIC;
		} else {
			client->teec_context = kmalloc(sizeof(TEEC_Context), GFP_KERNEL);
			if (copy_from_user(client->teec_context, context, sizeof(TEEC_Context)))
				SEC_HAL_TRACE("copy_from_user failed in line: %d", __LINE__);

			ret = sec_hal_tee_initialize_context((const char*)name, client->teec_context);
			if(copy_to_user(context, client->teec_context, sizeof(TEEC_Context)))
				SEC_HAL_TRACE("copy_to_user failed in line: %d", __LINE__);

			SEC_HAL_TRACE("client->teec_context->imp.tag 0x%x",client->teec_context->imp.tag);
			SEC_HAL_TRACE("client->teec_context->imp.hal_connection 0x%x",client->teec_context->imp.hal_connection);
                }
	} break;
	case SD_TEE_FINALIZE_CONTEXT:
		ret = sec_hal_tee_finalize_context(client->teec_context);
		client->teec_context = NULL;
		break;
	case SD_TEE_OPEN_SESSION: {
		uint32_t *session = (uint32_t *)input.param1;
		uint32_t *destination = (uint32_t *)input.param2;
		uint32_t connectionMethod = (uint32_t)input.param3;
		uint32_t *connectionData = (uint32_t *)input.param4;
		uint32_t *operation = (uint32_t *)input.param5;
		uint32_t *returnOrigin = (uint32_t *)input.reserved1;
		TEEC_Session *kernel_session;

		kernel_session = kmalloc(sizeof(TEEC_Session),GFP_KERNEL);
		if (copy_from_user(kernel_session,session,sizeof(TEEC_Session)))
			SEC_HAL_TRACE("copy_from_user failed in line: %d", __LINE__);

		ret = sec_hal_tee_open_session(client->teec_context,
			kernel_session,
			(const TEEC_UUID *)destination,
			connectionMethod,
			(const void *)connectionData,
			(TEEC_Operation *)operation,
			returnOrigin);

		SEC_HAL_TRACE("client->teec_context->imp.tag 0x%x",client->teec_context->imp.tag);
		SEC_HAL_TRACE("client->teec_context->imp.hal_connection 0x%x",client->teec_context->imp.hal_connection);
		SEC_HAL_TRACE("kernel_session->imp.tag 0x%x",kernel_session->imp.tag);

		if (copy_to_user(session, kernel_session, sizeof(TEEC_Session)))
			SEC_HAL_TRACE("copy_to_user failed in line: %d", __LINE__);

		kfree(kernel_session);
	} break;
	case SD_TEE_CLOSE_SESSION: {
		uint32_t *session = (uint32_t *)input.param0;
		TEEC_Session *kernel_session;

		kernel_session = kmalloc(sizeof(TEEC_Session),GFP_KERNEL);
		if (copy_from_user(kernel_session, session, sizeof(TEEC_Session)))
			SEC_HAL_TRACE("copy_from_user failed in line: %d", __LINE__);

		ret = sec_hal_tee_close_session(kernel_session);
		kfree(kernel_session);
	} break;
	case SD_TEE_INVOKE_COMMAND: {
		uint32_t* session = (uint32_t *)input.param0;
		uint32_t commandID = (uint32_t)input.param1;
		uint32_t* operation = (uint32_t *)input.param2;
		uint32_t* returnOrigin = (uint32_t *)input.param3;
		TEEC_Session *kernel_session;

		kernel_session = kmalloc(sizeof(TEEC_Session),GFP_KERNEL);
		if (copy_from_user(kernel_session,session,sizeof(TEEC_Session)))
			SEC_HAL_TRACE("copy_from_user failed in line: %d", __LINE__);

		ret = sec_hal_tee_invoke_command(
			kernel_session,
			commandID,
			(TEEC_Operation *)operation,
			returnOrigin);
		kfree(kernel_session);
	} break;
	case SD_TEE_PRE_MMAP: {
		uint32_t* shmem = (uint32_t *)input.param0;
		size_t size_rounded_to_pages;
		client->next_teec_shmem = kmalloc(sizeof(TEEC_SharedMemory), GFP_KERNEL);

		if (copy_from_user(client->next_teec_shmem,shmem,sizeof(TEEC_SharedMemory)))
			SEC_HAL_TRACE("copy_from_user failed in line: %d", __LINE__);

		SEC_HAL_TRACE("client->next_teec_shmem 0x%x", client->next_teec_shmem);
		SEC_HAL_TRACE("client->next_teec_shmem->size %d", client->next_teec_shmem->size);
		SEC_HAL_TRACE("client->next_teec_shmem->flags 0x%x", client->next_teec_shmem->flags);

		size_rounded_to_pages = ((client->next_teec_shmem->size / PAGE_SIZE) + 1) * PAGE_SIZE;
		client->next_teec_shmem_buffer = kmalloc(size_rounded_to_pages, GFP_KERNEL);
		client->next_teec_shmem->buffer = (void *)virt_to_phys(client->next_teec_shmem_buffer);

		SEC_HAL_TRACE("size_rounded_to_pages 0x%x", size_rounded_to_pages);
		SEC_HAL_TRACE("client->next_teec_shmem->buffer 0x%x", client->next_teec_shmem->buffer);

		if (TEEC_SUCCESS != sec_hal_tee_register_shared_memory_area(client->teec_context,client->next_teec_shmem)) {
			SEC_HAL_TRACE("registering shared memory failed\n");
			kfree(client->next_teec_shmem->buffer);
		}

		if (copy_to_user(shmem, client->next_teec_shmem, sizeof(TEEC_SharedMemory)))
			SEC_HAL_TRACE("copy_to_user failed in line: %d", __LINE__);

		ret = TEEC_SUCCESS;
	} break;
#endif /* CONFIG_ARM_SEC_HAL_TEE */
        default:
		SEC_HAL_TRACE("DEFAULT!");
		break;
	}

	up(&device->sem);

	return ret; /* directly return API definitions */
}



#ifdef CONFIG_ARM_SEC_HAL_TEE
/* **********************************************************************
 * Function name : search_mem_node
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
static int search_mem_node(struct list_head *lst, void *virt_addr)
{
	struct list_head *pos;
	shared_memory_node *item;
	shared_memory_node *node_to_return = NULL;

	list_for_each(pos, lst) {
		item = list_entry(pos, shared_memory_node, list);
		if (item->virt_addr == virt_addr) {
			node_to_return = item;
			break;
		}
	}

	return (int) node_to_return;
}


/* **********************************************************************
 * Function    : sec_hal_memory_tablewalk
 * Description : translates virt addr into phys addr.
 * Parameters  : vaddr - virtual address
 * Returns     : paddr - physical address
 * *********************************************************************/
unsigned long sec_hal_memory_tablewalk(void *vaddr)
{
	unsigned long paddr = 0x00;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	SEC_HAL_TRACE_ENTRY();

	if (PAGE_OFFSET <= (unsigned long)vaddr) {
		paddr = virt_to_phys((void *)vaddr);
	} else {
		pgd = pgd_offset(current->mm, (unsigned long)vaddr);
		pmd = pmd_offset((pud_t *)pgd, (unsigned long)vaddr);
		pte = pte_offset_map(pmd, (unsigned long)vaddr);
		paddr = (0xFFFFF000 & (*pte)) | (0x00000FFF & (unsigned long)vaddr);
	}

	SEC_HAL_TRACE_EXIT_INFO("paddr: 0x%08x", paddr);
	return paddr;
}


/* **********************************************************************
 * Function name : sec_hal_vma_open
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
void sec_hal_vma_open(struct vm_area_struct *vma)
{
	SEC_HAL_TRACE("Simple VMA open, virt %lx, phys %lx\n",
		vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}


/* **********************************************************************
 * Function name : sec_hal_vma_close
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
void sec_hal_vma_close(struct vm_area_struct *vma)
{
	shared_memory_node *mem_node_to_remove;
	unsigned long off = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long vaddr;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	TEEC_SharedMemory *shmem_to_release;
	struct client_data *client = vma->vm_private_data;
	int *kmalloc_area = phys_to_virt(off);

	SEC_HAL_TRACE("Simple VMA close.\n");
	SEC_HAL_TRACE("vma->vm_pgoff: 0x%08x",vma->vm_pgoff);
	SEC_HAL_TRACE("off: 0x%08x",off);
	SEC_HAL_TRACE("vma->vm_start: 0x%08x",vma->vm_start);
	SEC_HAL_TRACE("vma->vm_end: 0x%08x",vma->vm_end);
	SEC_HAL_TRACE("phys_to_virt(off): 0x%08x",phys_to_virt(off));

	/* Free the allocated memory */
	for (vaddr = (unsigned long)kmalloc_area;
		vaddr < (unsigned long)kmalloc_area + vsize;
		vaddr += PAGE_SIZE) {
		/* clear all pages to make them usable */
		ClearPageReserved(virt_to_page(vaddr));
	}
	mem_node_to_remove = (shared_memory_node *) search_mem_node(
		&(client->shmem_list.head), (void *)vma->vm_start);
	shmem_to_release = mem_node_to_remove->shmem;
	sec_hal_tee_release_shared_memory_area(shmem_to_release);
	kfree(shmem_to_release);

	list_del(&(mem_node_to_remove->list));
	kfree(mem_node_to_remove);

	kfree(kmalloc_area);
}


static struct vm_operations_struct sec_hal_remap_vm_ops = {
	.open = sec_hal_vma_open,
	.close = sec_hal_vma_close,
};


/* **********************************************************************
 * Function name : sec_hal_mmap
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
int sec_hal_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long vaddr;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	struct client_data *client = file->private_data;
	shared_memory_node *new_mem_node = NULL;
	int *kmalloc_area = NULL;
	int i;

	SEC_HAL_TRACE_ENTRY();

	new_mem_node = kmalloc(sizeof(shared_memory_node), GFP_KERNEL);
	vma->vm_private_data = file->private_data;
	kmalloc_area = phys_to_virt((phys_addr_t)client->next_teec_shmem->buffer);

	SEC_HAL_TRACE("client->next_teec_shmem 0x%x", client->next_teec_shmem);
	SEC_HAL_TRACE("vma->vm_pgoff: 0x%08x",vma->vm_pgoff);
	SEC_HAL_TRACE("vma->vm_start: 0x%08x",vma->vm_start);
	SEC_HAL_TRACE("vma->vm_end: 0x%08x",vma->vm_end);
	SEC_HAL_TRACE("PAGE_SHIFT: 0x%08x",PAGE_SHIFT);
	SEC_HAL_TRACE("vsize: %d",vsize);
	SEC_HAL_TRACE("vma->vm_flags: 0x%08x",vma->vm_flags);
	SEC_HAL_TRACE("kmalloc_area: 0x%08x",kmalloc_area);

	for (vaddr = (unsigned long)kmalloc_area;
		vaddr < (unsigned long)kmalloc_area + vsize;
		vaddr += PAGE_SIZE) {
		SetPageReserved(virt_to_page(vaddr));
	}
	SEC_HAL_TRACE("setpagereserved done");

	for (i = 0; i < (vsize/sizeof(int)); i += 2) {
		kmalloc_area[i]=(0xdead<<16) +i;
		kmalloc_area[i+1]=(0xbeef<<16) + i;
	}
	SEC_HAL_TRACE("deadbeef done");

	client->next_teec_shmem->size = vsize;
	if (remap_pfn_range(vma, vma->vm_start,
			(uint32_t)(client->next_teec_shmem->buffer) >> PAGE_SHIFT,
			vsize, vma->vm_page_prot))
		goto kfree_out;

	vma->vm_ops = &sec_hal_remap_vm_ops;
	new_mem_node->shmem = client->next_teec_shmem;
	new_mem_node->virt_addr = (void *)vma->vm_start;
	list_add(&new_mem_node->list, &(client->shmem_list.head));
	client->next_teec_shmem = NULL;

	SEC_HAL_TRACE("new_mem_node 0x%x", new_mem_node);
	SEC_HAL_TRACE("new_mem_node->shmem 0x%x", new_mem_node->shmem);
	SEC_HAL_TRACE("new_mem_node->virt_addr 0x%x", new_mem_node->virt_addr);

	SEC_HAL_TRACE_EXIT();
	return 0;
kfree_out:
	kfree(new_mem_node);
	SEC_HAL_TRACE_EXIT_INFO("remap failed, aborting");
	return -ENXIO;
}
#endif /* CONFIG_ARM_SEC_HAL_TEE */


static struct file_operations k_sec_hal_fops = {
    .owner = THIS_MODULE,
    .open = &sec_hal_usr_open,
    .release = &sec_hal_usr_release,
    .unlocked_ioctl = &sec_hal_usr_ioctl,
    .read = &sec_hal_rpc_read,
    .write = &sec_hal_rpc_write,
    .llseek = &no_llseek,
#ifdef CONFIG_ARM_SEC_HAL_TEE
    .mmap = &sec_hal_mmap,
#endif /* CONFIG_ARM_SEC_HAL_TEE */
};


/* **********************************************************************
 * Function name : add_attach_cdev
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
static int add_attach_cdev(
	struct cdev* dev,
	struct file_operations* fops,
	struct class* cls,
	dev_t devno,
	const char* devname)
{
	SEC_HAL_TRACE_ENTRY();

	cdev_init(dev, fops);
	dev->owner = THIS_MODULE;
	if (cdev_add(dev, devno, 1))
		return -ENODEV;

	device_create(cls, NULL, devno, NULL, devname);

	SEC_HAL_TRACE_EXIT();
	return 0;
}


/* **********************************************************************
 * Function name : detach_del_cdev
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
static void detach_del_cdev(struct cdev* dev, struct class* cls)
{
	SEC_HAL_TRACE_ENTRY();

	device_destroy(cls, dev->dev);
	cdev_del(dev);

	SEC_HAL_TRACE_EXIT();
}


/* **********************************************************************
 * Function name : sec_hal_cdev_init
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
static int sec_hal_cdev_init(struct device_data *device_data)
{
	int ret = 0;
	dev_t devno = 0;
	struct class* cls;

	SEC_HAL_TRACE_ENTRY();

	if (0 > alloc_chrdev_region(&devno, 0, 1, DEVNAME)) {
		SEC_HAL_TRACE("cannot register device, aborting!");
		ret = -EIO;
		goto e1;
	}

	cls = class_create(THIS_MODULE, DEVNAME);
	if (IS_ERR(cls)) {
		SEC_HAL_TRACE("failed to class_create, aborting!");
		ret = PTR_ERR(cls);
		goto e2;
	}

	if (add_attach_cdev(&device_data->cdev,
			&k_sec_hal_fops, cls, devno, DEVNAME)) {
		SEC_HAL_TRACE("failed to add and attach cdev, aborting!");
		ret = -ENODEV;
		goto e2;
	}

	/* all ok, store heap data to argument struct. */
	sema_init(&device_data->sem, CONFIG_NR_CPUS);
	device_data->class = cls;

	SEC_HAL_TRACE_EXIT();
	return ret;

e2:	class_destroy(cls);
e1:	unregister_chrdev_region(MAJOR(devno), 1);

	SEC_HAL_TRACE_EXIT_INFO("failed to create /dev - nodes.");
	return ret;
}


/* **********************************************************************
 * Function name : sec_hal_cdev_exit
 * Description   :
 * Parameters    :
 * Returns       :
 * *********************************************************************/
static void sec_hal_cdev_exit(struct device_data *device_data)
{
	dev_t devno = device_data->cdev.dev;

	SEC_HAL_TRACE_ENTRY();

	detach_del_cdev(&device_data->cdev, device_data->class);
	class_destroy(device_data->class);
	unregister_chrdev_region(MAJOR(devno), 1);

	SEC_HAL_TRACE_EXIT();
}


/* **********************************************************************
 * Function name : sec_hal_dbg_irq_hdr
 * Description   : IRQ handler for JTAG/hw debugger attach event.
 *                 Secure JTAG is not supported by this function.
 * Return        : IRQ_HANDLED
 * *********************************************************************/
static irqreturn_t sec_hal_dbg_irq_hdr(int irq, void *dev_id)
{
	(void) dev_id;
	(void) sec_hal_dbg_reg_set(&g_dbgreg1, &g_dbgreg2, &g_dbgreg3);

	/* Disable interrupt of TDBG because it requires only ONCE. */
	disable_irq_nosync(irq);
	return IRQ_HANDLED;
}


/* **********************************************************************
 * Function name : sec_hal_dbg_irq_init
 * Description   : initializes IRQ handler for TDBG, IRQ should be
 *                 triggered by JTAG/hw debugger attach event.
 *                 Secure JTAG is not supported by this function.
 * *********************************************************************/
static void sec_hal_dbg_irq_init(void)
{
	int rv;
	uint32_t irq, flags = IRQF_DISABLED;

	SEC_HAL_TRACE_ENTRY();

	irq = gic_spi(TDBG_SPI);
	set_irq_flags(irq, IRQF_VALID|IRQF_NOAUTOEN);
	rv = request_irq(irq, &sec_hal_dbg_irq_hdr, flags,
			"DBG_LINKUP_REQ", (void*)irq);
	if (rv < 0)
		free_irq(irq, (void*)irq);

	enable_irq(irq);

	SEC_HAL_TRACE_EXIT_INFO("rv: %d", rv);
}


/* **********************************************************************
 * Function name : sec_hal_timeout
 * Description   : handler for timeout IRQ, used for per. icheck.
 *                 new period is only set if per. check succeeds.
 * *********************************************************************/
static void sec_hal_timeout(unsigned long arg)
{
	uint32_t rv, next = 0;
	struct timer_list *timer = (struct timer_list *) arg;

	rv = sec_hal_rt_periodic_integrity_check(&next);
	if (rv == SEC_HAL_RES_OK && next) {
		timer->expires = jiffies + msecs_to_jiffies(next);
		add_timer(timer);
	}
}


uint32_t sec_hal_reset_info_addr_register(void);
int sec_hal_icram0_init(void);
int sec_hal_rpc_init(void);
/* **********************************************************************
 * PLATFORM DEVICE FRAMEWORK RELATED FUNCTIONS.
 * *********************************************************************/
/* **********************************************************************
 * Function name : sec_hal_pdev_probe
 * Description   :
 * *********************************************************************/
static int sec_hal_pdev_probe(struct platform_device *pdev)
{
	int ret, i = 0;
	sec_hal_init_info_t rt_init;

	(void) pdev;

	SEC_HAL_TRACE_ENTRY();

	ret = sec_hal_icram0_init();
	if (ret)
		goto e0;

	ret = sec_hal_rpc_init();
	if (ret)
		goto e0;

	ret = sec_hal_rt_init(&rt_init);
	if (ret)
		goto e1;

	printk(KERN_INFO "TRUSTZONE VERSION: 0x%08x%08x [0x%X, 0x%X, 0x%X]\n",
			(uint32_t)(rt_init.commit_id >> 32),
			(uint32_t)(rt_init.commit_id),
			rt_init.reset_info[0], rt_init.reset_info[1], rt_init.reset_info[2]);

	if (!is_recovery()) {
		ret = sec_hal_memfw_attr_reserve(0, 0x00UL, 0x00UL);
		if (ret)
			goto e1;
	}
	sec_hal_dbg_irq_init();

	ret = sec_hal_cdev_init(&g_device_data);
	if (ret)
		goto e2;

	ret = sec_hal_reset_info_addr_register();
	if (ret)
		goto e2;

	ret = sec_hal_public_id_get(&g_pub_id);
	if (ret)
		goto e2;

	printk("DEVICE PUBLIC ID: ");
	for (i=0; i < SEC_HAL_MAX_PUBLIC_ID_LENGTH; i++) {
		printk("%02x", g_pub_id.public_id[i]);
	}
	printk("\n");

	init_timer(&g_integ_timer);
	g_integ_timer.data = (unsigned long)&g_integ_timer;
	g_integ_timer.function = sec_hal_timeout;
	g_integ_timer.expires = jiffies + msecs_to_jiffies(INIT_TIMER_MSECS);
	add_timer(&g_integ_timer);

	SEC_HAL_TRACE_EXIT();
	return ret;

e2:	sec_hal_cdev_exit(&g_device_data);
e1:
e0:
	SEC_HAL_TRACE_EXIT_INFO("failed in device init");
	return ret;
}


/* **********************************************************************
 * Function name : sec_hal_pdev_suspend
 * Description   :
 * *********************************************************************/
static int sec_hal_pdev_suspend(struct platform_device *pdev, pm_message_t state)
{
	int rv = 0;

	(void)pdev;

	SEC_HAL_TRACE_ENTRY();
	SEC_HAL_TRACE("state.event: 0x%X", state.event);

	if ((PM_EVENT_SUSPEND & state.event)
		&& timer_pending(&g_integ_timer)) {
		rv = del_timer_sync(&g_integ_timer);
		(void)sec_hal_rt_periodic_integrity_check(&g_banked_timeout);
	}

	SEC_HAL_TRACE_EXIT_INFO("rv: %d", rv);
	return (rv > 0 ? 0 : rv);
}


/* **********************************************************************
 * Function name : sec_hal_pdev_resume
 * Description   :
 * *********************************************************************/
static int sec_hal_pdev_resume(struct platform_device *pdev)
{
	int rv = 0;

	(void)pdev;

	SEC_HAL_TRACE_ENTRY();

	if (g_banked_timeout) {
		g_integ_timer.expires = jiffies
			+ msecs_to_jiffies(g_banked_timeout);
		add_timer(&g_integ_timer);
		g_banked_timeout = 0;
	}

	SEC_HAL_TRACE_EXIT();
	return rv;
}


/* **********************************************************************
 * Function name : sec_hal_pdev_remove
 * Description   :
 * *********************************************************************/
static int sec_hal_pdev_remove(struct platform_device *pdev)
{
	(void) pdev;

	SEC_HAL_TRACE_ENTRY();

	sec_hal_cdev_exit(&g_device_data);

	SEC_HAL_TRACE_EXIT();
	return 0;
}


static struct platform_driver k_sec_hal_platform_device_driver =
{
	.probe = sec_hal_pdev_probe,
	.suspend = sec_hal_pdev_suspend,
	.resume = sec_hal_pdev_resume,
	.remove = sec_hal_pdev_remove,
	.driver = {.name  = DEVNAME, .owner = THIS_MODULE},
};


/* **********************************************************************
 * MDM Boot
 * *********************************************************************/
/* **********************************************************************
 * Function name : sec_hal_mdm_memcpy
 * Description   : copy data to a certain kind of protected memory.
 * *********************************************************************/
uint32_t sec_hal_mdm_memcpy(uint32_t pdst, uint32_t psrc, uint32_t sz)
{
	return sec_hal_memcpy(pdst, psrc, sz);
}


/* **********************************************************************
 * Function name : sec_hal_mdm_authenticate
 * Description   : authenticate memory content with SW cert.
 * *********************************************************************/
uint32_t sec_hal_mdm_authenticate(
	uint32_t c_paddr,
	uint32_t c_sz,
	uint32_t *objid)
{
	return sec_hal_authenticate(c_paddr, c_sz, objid);
}


/* **********************************************************************
 * MODULE init/exit routines
 * *********************************************************************/
static struct resource k_sec_hal_resources[] =
{
	[0] =
	{ /* ICRAM0 */
		.start = UL(ICRAM0_ADDRESS),
		.end = UL(ICRAM0_ADDRESS) + UL(ICRAM0_SIZE) - 1,/* 4kb from start addr */
		.flags = IORESOURCE_MEM,
	}
};
static u64 k_dma_mask = 0xffffffff;
static struct platform_device k_sec_hal_chardevice =
{
	.name =  DEVNAME,
	.id = 0,
	.dev = {
		.coherent_dma_mask = 0xffffffff,
		.dma_mask = &k_dma_mask
	},
	.resource = k_sec_hal_resources,
	.num_resources = ARRAY_SIZE(k_sec_hal_resources),
};
static struct platform_device *k_sec_hal_local_devs[] __initdata =
{
	&k_sec_hal_chardevice,
};

/* **********************************************************************
 * Function name : sec_hal_driver_init
 * Description   :
 * *********************************************************************/
static int __init sec_hal_driver_init(void)
{
	int ret;

	SEC_HAL_TRACE_INIT();
	SEC_HAL_TRACE_ENTRY();

	ret = platform_driver_register(&k_sec_hal_platform_device_driver);
	platform_add_devices(k_sec_hal_local_devs,
		ARRAY_SIZE(k_sec_hal_local_devs));

	SEC_HAL_TRACE_EXIT();
	return ret;
}


/* **********************************************************************
 * Function name : sec_hal_driver_exit
 * Description   :
 * *********************************************************************/
static void __exit sec_hal_driver_exit(void)
{
	SEC_HAL_TRACE_ENTRY();

	platform_driver_unregister(&k_sec_hal_platform_device_driver);

	SEC_HAL_TRACE_EXIT();
}


/* module_init(sec_hal_driver_init); */
module_exit(sec_hal_driver_exit);
pure_initcall(sec_hal_driver_init);
MODULE_AUTHOR("Renesas Mobile Corporation");
MODULE_DESCRIPTION("Device driver for ARM TRUSTZONE access");
MODULE_LICENSE("GPL");

