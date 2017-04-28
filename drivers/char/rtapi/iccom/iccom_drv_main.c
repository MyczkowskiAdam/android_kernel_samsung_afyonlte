/*
 * iccom_drv_main.c
 *     Inter Core Communication driver function file.
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
#include <linux/interrupt.h>
#include <linux/ioctl.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include "log_kernel.h"
#include "iccom_hw.h"
#include "iccom_drv.h"
#include "iccom_drv_common.h"
#include "iccom_drv_private.h"
#include "system_rtload_internal.h"
#ifdef ICCOM_ENABLE_STANDBYCONTROL
#include <linux/mfis.h>
#include "iccom_drv_standby_private.h"
#endif

/*** define ***/
#define ICCOM_DRIVER_NAME	"iccom"
#define ICCOM_DEVICE_ID		"iccom_int"
#define ICCOM_MAX_LOOP_CNT  6000
#define ICCOM_SLEEP_TIME	1

static struct miscdevice g_iccom_device;					/* device driver information */
static struct task_struct *g_iccom_async_resp_task;			/* task information */
spinlock_t				g_iccom_lock_handle_list;
struct list_head		g_iccom_list_handle;
#ifdef ICCOM_ENABLE_STANDBYCONTROL
struct semaphore		g_iccom_sem_fp_list;
struct list_head		g_iccom_list_fp;
#endif

/**** prototype ****/
static int  iccom_open(struct inode*, struct file*);
static int  iccom_close(struct inode*, struct file*);
static long iccom_ioctl(struct file*, unsigned int, unsigned long);
static int  iccom_thread_async_resp(void *);
static int  iccom_init_module(void);
static void iccom_cleanup_module(void);

/******************************************************************************/
/* Function   : iccom_open                                                    */
/* Description: open ICCOM driver                                             */
/******************************************************************************/
static
int iccom_open(
	struct inode	*inode,
	struct file		*fp
)
{
	int				ret;
	iccom_drv_handle   *handle;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	ret = SMAP_NG;
	/* create a ICCOM handle */
	handle = iccom_create_handle(ICCOM_TYPE_USER);
	if (handle) {
		fp->private_data = handle;
		ret = SMAP_OK;
	}
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return ret;
}

/******************************************************************************/
/* Function   : iccom_close                                                   */
/* Description: close ICCOM driver                                            */
/******************************************************************************/
static
int iccom_close(
	struct inode	*inode,
	struct file		*fp
)
{
	iccom_drv_handle	*drv_handle;
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	drv_handle = (iccom_drv_handle *)(fp->private_data);
	iccom_leak_check(drv_handle);

#ifdef ICCOM_ENABLE_STANDBYCONTROL
	{
		iccom_fp_list	*fp_list;
		iccom_fp_list	*fp_list_next;
		unsigned int	ng_cancel_cnt = 0;

		ICCOM_DOWN_TIMEOUT(&g_iccom_sem_fp_list);
		list_for_each_entry_safe(fp_list, fp_list_next, &g_iccom_list_fp, list) {
			if (fp == fp_list->fp) {
				list_del(&fp_list->list);
				kfree(fp_list);
				ng_cancel_cnt++;
			}
		}
		up(&g_iccom_sem_fp_list);
		for (; 0 < ng_cancel_cnt; ng_cancel_cnt--) {
			/* decrement internal standby control counter */
			iccom_rtctl_ioctl_standby_ng_cancel();
		}
	}
#endif

	/* release a ICCOM handle */
	iccom_destroy_handle(fp->private_data);

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return SMAP_OK;
}

/******************************************************************************/
/* Function   : iccom_ioctl                                                   */
/* Description: ioctl                                                         */
/******************************************************************************/
static
long iccom_ioctl(
	struct file		*fp,
	unsigned int	cmd,
	unsigned long	arg
)
{
	int				ret;
	iccom_cmd_param	ioctl_param;
	struct task_struct  *task = current;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	ret = SMAP_OK;

	task->flags |= PF_FREEZER_SKIP;
	memset(&ioctl_param, 0, sizeof(ioctl_param));

	if (0 != arg) {
		if (!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd))) {
			MSG_ERROR("[ICCOMK]ERR| parameter error.\n");
			ret = SMAP_PARA_NG;
			goto out;
		}
	}

#ifdef ICCOM_ENABLE_STANDBYCONTROL
	ret = mfis_drv_resume();
#endif

	if (SMAP_OK == ret) {
		MSG_MED("[ICCOMK]INF| cmd = 0x%08x\n", cmd);
		switch (cmd) {
		/* send a command to RT Domain */
		case ICCOM_CMD_SEND:
			{
				unsigned int		send_size = 0;
				iccom_drv_cmd_data *send_data = NULL;

				/* set parameters of iccom_send_command() */
				if (copy_from_user(&ioctl_param.send, (void __user *) arg, sizeof(ioctl_param.send))) {
					MSG_ERROR("[ICCOMK]ERR| ioctl copy error.\n");
					ret = SMAP_NG;
					break;
				}
				if (0 != ioctl_param.send.send_num) { /* transmission data is set */
					send_size = sizeof(*send_data) * ioctl_param.send.send_num;
					send_data = kmalloc(send_size, GFP_KERNEL);
					if (NULL == send_data) {
						MSG_ERROR("[ICCOMK]ERR| memory allocate error.\n");
						ret = SMAP_MEMORY;
						break;
					}
					/* set transmission data */
					if (copy_from_user(send_data, (void __user *) ioctl_param.send.send_data, send_size)) {
						MSG_ERROR("[ICCOMK]ERR| send data copy.\n");
						kfree(send_data);
						ret = SMAP_NG;
						break;
					}
					ioctl_param.send.send_data = send_data;
				}
				/* send a command */
				ret = iccom_send_command(fp->private_data,
										 ICCOM_TYPE_USER,
										 &ioctl_param.send);
				if (NULL != send_data) {
					kfree(send_data);
				}
			}
			break;
		/* receive a synchronous command from RT Domain */
		case ICCOM_CMD_RECV:
			{
				iccom_cmd_recv_param	recv_param;
				unsigned int			recv_size = 0;

				if (copy_from_user(&ioctl_param.recv, (void __user *) arg, sizeof(ioctl_param.recv))) {
					MSG_ERROR("[ICCOMK]ERR| ioctl copy error.\n");
					ret = SMAP_NG;
					break;
				}

				/* set parameters of iccom_recv_command_sync() */
				memcpy(&recv_param, &ioctl_param.recv, sizeof(recv_param));
				recv_param.result_code = SMAP_OK;
				recv_param.recv_data   = NULL;

				/* receive a synchronous command */
				ret = iccom_recv_command_sync(fp->private_data,
											   &recv_param);
				if (SMAP_OK == ret) {
					/* check receive data size */
					recv_size = ioctl_param.recv.recv_size;
					if (recv_size > recv_param.recv_size) {
						recv_size = recv_param.recv_size;
					}
					/* check receive data */
					if ((0 != recv_size) &&
						 (NULL != recv_param.recv_data) && (NULL != ioctl_param.recv.recv_data)) {
						/* copy data to local area */
						if (copy_to_user((void __user *)ioctl_param.recv.recv_data,
										   recv_param.recv_data, recv_size)) {
							MSG_ERROR("[ICCOMK]ERR| receive data copy.\n");
							ret = SMAP_NG;
						}
					}
					ioctl_param.recv.result_code = recv_param.result_code;

					/* release command data */
					iccom_recv_complete(recv_param.recv_data);
					if (SMAP_OK != ret) {
						break;
					}
					/* copy data to user area */
					if (copy_to_user((void __user *)arg, &ioctl_param.recv, sizeof(ioctl_param.recv))) {
						MSG_ERROR("[ICCOMK]ERR| output data copy.\n");
						ret = SMAP_NG;
						break;
					}
				}
			}
			break;
		/* receive an asynchronous command from RT Domain */
		case ICCOM_CMD_RECV_ASYNC:
			{
				void				   *handle;

				handle = fp->private_data;
				memset(&ioctl_param.recv_async, 0, sizeof(ioctl_param.recv_async));
				/* receive an asynchronous command */
				ret = iccom_recv_command_async(&handle,
												&ioctl_param.recv_async);
				if (SMAP_OK == ret) {
					MSG_MED("[ICCOMK]INF| handle->recv_data[0x%08x].\n",
							(unsigned int)((iccom_drv_handle *)handle)->recv_data);
					/* copy receive data information to user area */
					if (copy_to_user((void __user *)arg, &ioctl_param.recv_async, sizeof(ioctl_param.recv_async))) {
						MSG_ERROR("[ICCOMK]ERR| output data copy.\n");
						ret = SMAP_NG;
					}
				}
			}
			break;
		/* cancel to receive asynchronous command */
		case ICCOM_CMD_CANCEL_RECV_ASYNC:
			{
				ret = iccom_recv_cancel(fp->private_data);
			}
			break;
		/* copy receive data */
		case ICCOM_CMD_GET_RECV_DATA:
			{
				iccom_drv_handle	*handle;

				handle = fp->private_data;
				/* set receive data information */
				if (copy_from_user(&ioctl_param.get_data, (void __user *) arg, sizeof(ioctl_param.get_data))) {
					MSG_ERROR("[ICCOMK]ERR| ioctl copy error.\n");
					ret = SMAP_NG;
					break;
				}
				if ((NULL != ioctl_param.get_data.recv_data) && (NULL != ioctl_param.get_data.user_recv_data)) {
					/* copy receive data to user area */
					if (copy_to_user((void __user *)ioctl_param.get_data.user_recv_data,
									   ioctl_param.get_data.recv_data, ioctl_param.get_data.recv_size)) {
						MSG_ERROR("[ICCOMK]ERR| output data copy.\n");
						ret = SMAP_NG;
					}
				}
				/* release command data */
				iccom_recv_complete(ioctl_param.get_data.recv_data);
				MSG_MED("[ICCOMK]INF| handle->recv_data[0x%08x].\n", (unsigned int)handle->recv_data);
				handle->recv_data = NULL;
			}
			break;
		/* get process ID of asynchronous thread */
		case ICCOM_CMD_GET_PID:
			{
				ioctl_param.get_pid.pid = current->pid;
				if (copy_to_user((void __user *)arg, &ioctl_param.get_pid, sizeof(ioctl_param.get_pid))) {
					MSG_ERROR("[ICCOMK]ERR| output data copy.\n");
					ret = SMAP_NG;
				}
			}
			break;
		/* set fatal information */
		case ICCOM_CMD_SET_FATAL:
			{
				iccom_fatal_info	fatal_info;
				iccom_fatal_info   *p_get_fatal;

				if (copy_from_user(&ioctl_param.set_fatal, (void __user *) arg, sizeof(ioctl_param.set_fatal))) {
					MSG_ERROR("[ICCOMK]ERR| ioctl copy error.\n");
					ret = SMAP_NG;
					break;
				}

				memset(&fatal_info, 0, sizeof(fatal_info));

				/* fatal information entry */
				if (ICCOM_DRV_FATAL_ENTRY == ioctl_param.set_fatal.kind) {
					fatal_info.handle = fp->private_data;
				}
				/* fatal information delete */
				else{
					/* get fatal information */
					ret = iccom_get_fatal(&p_get_fatal);
					if (SMAP_OK != ret) {
						MSG_ERROR("[ICCOMK]ERR| handle is already deleted.\n");
						ret = SMAP_PARA_NG;
						break;
					} else{
						if (p_get_fatal->handle != fp->private_data) {
							MSG_ERROR("[ICCOMK]ERR| handle is not in agreement.\n");
							ret = SMAP_PARA_NG;
							break;
						}
					}
				}
				ret = iccom_put_fatal(&fatal_info);   /* put fatal information */
			}
			break;
		/* set the priority of the thread */
		case ICCOM_CMD_SET_THREAD:
			{
				int sch_ret;
				struct sched_param param;

				if (copy_from_user(&ioctl_param.set_thread, (void __user *) arg, sizeof(ioctl_param.set_thread))) {
					MSG_ERROR("[ICCOMK]ERR| ioctl copy error.\n");
					ret = SMAP_NG;
					break;
				}
				/* set a priority */
				param.sched_priority = ioctl_param.set_thread.async_priority;
				sch_ret = sched_setscheduler(g_iccom_async_resp_task, SCHED_FIFO, &param);
				if (0 != sch_ret) {
					MSG_ERROR("[ICCOMK]ERR| sched_setscheduler failed [%d]\n", sch_ret);
					ret = SMAP_NG;
				}
			}
			break;

#ifdef ICCOM_ENABLE_STANDBYCONTROL
		/* change RT state to active */
		case ICCOM_CMD_ACTIVE:
			{
				ret = iccom_rtctl_change_rt_state_active();
			}
			break;
		/* change internal standby control state to disable standby */
		case ICCOM_CMD_STANDBY_NG:
			{
				iccom_fp_list *fp_list;

				fp_list = kmalloc(sizeof(*fp_list), GFP_KERNEL);
				if (NULL == fp_list) {
					MSG_ERROR("[ICCOMK]ERR| fp list allocate error.\n");
					ret = SMAP_MEMORY;
				} else {
					fp_list->fp = fp;
					ICCOM_DOWN_TIMEOUT(&g_iccom_sem_fp_list);
					list_add_tail(&fp_list->list, &g_iccom_list_fp);
					up(&g_iccom_sem_fp_list);

					/* increment internal standby control counter */
					ret = iccom_rtctl_ioctl_standby_ng();
				}
			}
			break;
		/* change internal standby control state to enable standby */
		case ICCOM_CMD_STANDBY_NG_CANCEL:
			{
				iccom_fp_list	*fp_list;
				iccom_fp_list	*fp_list_next;
				unsigned int	ng_cancel_cnt = 0;

				ICCOM_DOWN_TIMEOUT(&g_iccom_sem_fp_list);
				list_for_each_entry_safe(fp_list, fp_list_next, &g_iccom_list_fp, list) {
					if (fp == fp_list->fp) {
						list_del(&fp_list->list);
						kfree(fp_list);
						ng_cancel_cnt++;
						break;
					}
				}
				up(&g_iccom_sem_fp_list);
				if (0 < ng_cancel_cnt) {
					/* decrement internal standby control counter */
					ret = iccom_rtctl_ioctl_standby_ng_cancel();
				}
			}
			break;
#endif

		default:
			MSG_ERROR("[ICCOMK]ERR| command error.\n");
			ret = SMAP_NG;
			break;
		}
	}
out:
	MSG_MED("[ICCOMK]OUT|[%s] ret = %d\n", __func__, ret);
	return ret;
}

/******************************************************************************/
/* Function   : iccom_thread_async_resp                                       */
/* Description: asynchronous thread                                           */
/******************************************************************************/
static
int iccom_thread_async_resp(
	void   *ptr
)
{
	int						ret;
	int						recv_async_ret;
	unsigned long			loop = ICCOM_MAX_LOOP_CNT;

	iccom_drv_handle			*handle;
	iccom_cmd_recv_async_param	recv_param;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	do {
		handle = NULL;
		memset(&recv_param, 0, sizeof(iccom_cmd_recv_async_param));

		/* receive an asynchronous command */
		recv_async_ret = iccom_recv_command_async((void **)&handle, &recv_param);
		MSG_LOW("[ICCOMK]INF| iccom_recv_command_async recv_async_ret[%d]\n", recv_async_ret);

		if ((SMAP_NG != recv_async_ret) &&		 /* receiving the asynchronous command was not canceled */
			(SMAP_QUEUE_NG != recv_async_ret)) {   /* receive queue was found */
			MSG_INFO("[ICCOMK]INF| handle[0x%08x]\n", (unsigned int)handle);
			MSG_INFO("[ICCOMK]INF| result_code[%d]\n", recv_param.result_code);
			MSG_INFO("[ICCOMK]INF| function_id[%d]\n", recv_param.function_id);
			MSG_INFO("[ICCOMK]INF| recv_size[%d]\n", recv_param.recv_size);
			MSG_INFO("[ICCOMK]INF| recv_data[0x%08x]\n", (unsigned int)recv_param.recv_data);

			if (NULL != handle) {
				if (NULL != handle->kernel_cb_info.module) {
					/* execute callback function */
					handle->kernel_cb_info.module(handle->kernel_cb_info.user_data,
												recv_param.result_code,
												recv_param.function_id,
												recv_param.recv_data,
												recv_param.recv_size);
				}
			} else {
				MSG_ERROR("[ICCOMK]ERR| handle[NULL]\n");
			}

			/* release command data */
			ret = iccom_recv_complete(recv_param.recv_data);
			if (SMAP_OK != ret) {
				MSG_ERROR("[ICCOMK]ERR| iccom_recv_complete failed ret[%d]\n", ret);
			}
		}
	} while (SMAP_NG != recv_async_ret);

	do {
		MSG_INFO("[ICCOMK]INF| kthread_should_stop() waiting...\n");
		if (0 != kthread_should_stop()) {
			break;
		}
		msleep(ICCOM_SLEEP_TIME);
		loop--;
	} while (loop > 0);
	MSG_INFO("[ICCOMK]INF| kthread_should_stop() loop=%d\n", (unsigned int)loop);

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return SMAP_OK;
}

/* initialize file_operations */
static const struct file_operations g_iccom_fops = {
	.owner			= THIS_MODULE,
	.open			= iccom_open,
	.release		= iccom_close,
	.unlocked_ioctl	= iccom_ioctl
};

/******************************************************************************/
/* Function   : iccom_init_module                                             */
/* Description: ICCOM driver initialize module                                */
/******************************************************************************/
static
int iccom_init_module(
	void
)
{
	int				ret;
	unsigned long	loop = ICCOM_MAX_LOOP_CNT;
	unsigned long	reg_eicr = 0;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	g_iccom_device.name  = ICCOM_DRIVER_NAME;
	g_iccom_device.fops  = &g_iccom_fops;
	g_iccom_device.minor = MISC_DYNAMIC_MINOR;

	/* register device driver */
	ret = misc_register(&g_iccom_device);
	if (0 != ret) {
		MSG_ERROR("[ICCOMK]ERR| misc_register failed ret[%d]\n", ret);
		return SMAP_NG;
	}

	spin_lock_init(&g_iccom_lock_handle_list);

	memset(&g_iccom_list_handle, 0, sizeof(g_iccom_list_handle));
	INIT_LIST_HEAD(&g_iccom_list_handle);

#ifdef ICCOM_ENABLE_STANDBYCONTROL
	init_MUTEX(&g_iccom_sem_fp_list);
	memset(&g_iccom_list_fp, 0, sizeof(g_iccom_list_fp));
	INIT_LIST_HEAD(&g_iccom_list_fp);

	/* initialize standby function */
	ret = iccom_rtctl_initilize();
	if (0 != ret) {
		MSG_ERROR("[ICCOMK]ERR| iccom_rtctl_initialize failed ret[%d]\n", ret);
		ret = misc_deregister(&g_iccom_device);
		if (0 != ret) {
			MSG_ERROR("[ICCOMK]ERR| misc_deregister failed ret[%d]\n", ret);
		}
		return SMAP_NG;
	}
#endif

	/* wait for RT Domain boot */
	do {
		reg_eicr = RD_ICCOMEICR();
		MSG_INFO("[ICCOMK]INF| reg_eicr[0x%08x]\n", (unsigned int)reg_eicr);
		if (ICCOMEICR_INIT_COMP == (reg_eicr & ICCOMEICR_INIT_COMP)) {
			break;
		}

		current->state = TASK_INTERRUPTIBLE;
		msleep(ICCOM_SLEEP_TIME);
		loop--;
	} while (loop > 0);

	if (0 == loop) {
		MSG_ERROR("[ICCOMK]ERR| RTDomain Boot NG : Time Out Error\n");
		{
			get_section_header_param	get_section;
			system_rt_section_header    section;

			get_section.section_header = &section;
			ret = sys_get_section_header(&get_section);
			if (SMAP_OK == ret) {
				unsigned long __iomem *addr_status;
				addr_status = ioremap_nocache(section.command_area_address, sizeof(unsigned long));
				MSG_ERROR("[ICCOMK]ERR| RTDomain Boot Status [%d]\n", __raw_readl(addr_status));
				iounmap(addr_status);
			}
		}
		/* unregister device driver */
		ret = misc_deregister(&g_iccom_device);
		if (0 != ret) {
			MSG_ERROR("[ICCOMK]ERR| misc_deregister failed ret[%d]\n", ret);
		}
		return SMAP_NG;
	}

	WT_ICCOMEICR(ICCOMEICR_INIT);

#ifdef ICCOM_ENABLE_STANDBYCONTROL
	/* set RT state to active */
	iccom_rtctl_set_rt_state();
#endif

	/* initialize fatal information */
	iccom_init_fatal();

	/* initialize communication function */
	ret = iccom_comm_func_init();
	if (SMAP_OK != ret) {
		MSG_ERROR("[ICCOMK]ERR| iccom_comm_func_init failed ret[%d]\n", ret);
		/* unregister device driver */
		ret = misc_deregister(&g_iccom_device);
		if (0 != ret) {
			MSG_ERROR("[ICCOMK]ERR| misc_deregister failed ret[%d]\n", ret);
		}
		return SMAP_NG;
	}

	/* run the asynchronous thread */
	g_iccom_async_resp_task = kthread_run(iccom_thread_async_resp, NULL, "iccom_th_async");
	if (NULL == g_iccom_async_resp_task) {
		MSG_ERROR("[ICCOMK]ERR| kthread_run failed g_iccom_async_resp_task[NULL]\n");
		/* quit communication function */
		iccom_comm_func_quit();
		/* unregister device driver */
		ret = misc_deregister(&g_iccom_device);
		if (0 != ret) {
			MSG_ERROR("[ICCOMK]ERR| misc_deregister failed ret[%d]\n", ret);
		}
		return SMAP_NG;
	}

	/* register IRQ handler */
#ifdef ICCOM_ENABLE_STANDBYCONTROL
	ret = request_irq(INT_ICCOM, iccom_iccomeicr_int, (IRQF_SHARED|IRQF_NO_SUSPEND), ICCOM_DRIVER_NAME, (void *)ICCOM_DEVICE_ID);
#else
	ret = request_irq(INT_ICCOM, iccom_iccomeicr_int, IRQF_SHARED, ICCOM_DRIVER_NAME, (void *)ICCOM_DEVICE_ID);
#endif
	if (0 != ret) {
		MSG_ERROR("[ICCOMK]ERR| request_irq failed ret[%d]\n", ret);
		/* cancel to receive asynchronous command */
		ret = iccom_recv_cancel(NULL);
		if (SMAP_OK != ret) {
			MSG_ERROR("[ICCOMK]ERR| iccom_recv_cancel failed ret[%d]\n", ret);
		}

		/* stop the asynchronous thread */
		ret = kthread_stop(g_iccom_async_resp_task);
		if (0 != ret) {
			MSG_ERROR("[ICCOMK]ERR| kthread_stop failed ret[%d]\n", ret);
		}

		/* quit communication function */
		iccom_comm_func_quit();

		/* unregister device driver */
		ret = misc_deregister(&g_iccom_device);
		if (0 != ret) {
			MSG_ERROR("[ICCOMK]ERR| misc_deregister failed ret[%d]\n", ret);
		}
		return SMAP_NG;
	}

	/* start log */
	iccom_log_start();

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return SMAP_OK;
}

/******************************************************************************/
/* Function   : iccom_cleanup_module                                          */
/* Description: ICCOM driver exit module                                      */
/******************************************************************************/
static
void iccom_cleanup_module(
	void
)
{
	int ret;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	/* stop log */
	iccom_log_stop();

	/* free IRQ handler */
	free_irq(INT_ICCOM, (void *)ICCOM_DEVICE_ID);

	/* cancel to receive asynchronous command */
	ret = iccom_recv_cancel(NULL);
	if (SMAP_OK != ret) {
		MSG_ERROR("[ICCOMK]ERR| iccom_recv_cancel failed ret[%d]\n", ret);
	}

	/* stop the asynchronous thread */
	ret = kthread_stop(g_iccom_async_resp_task);
	if (0 != ret) {
		MSG_ERROR("[ICCOMK]ERR| kthread_stop failed ret[%d]\n", ret);
	}

	/* quit communication function */
	iccom_comm_func_quit();

#ifdef ICCOM_ENABLE_STANDBYCONTROL
	/* finalize standby function */
	iccom_rtctl_finalize();
#endif

	/* unregister device driver */
	ret = misc_deregister(&g_iccom_device);
	if (0 != ret) {
		MSG_ERROR("[ICCOMK]ERR| misc_deregister failed ret[%d]\n", ret);
	}

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}
module_init(iccom_init_module);
module_exit(iccom_cleanup_module);
