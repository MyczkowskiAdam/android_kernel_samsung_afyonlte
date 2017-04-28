/*
 * iccom_drv_send.c
 *     Inter Core Communication driver function file for send.
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
#include <linux/slab.h>
#include <linux/irqreturn.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include "log_kernel.h"
#include "system_rtload_internal.h"
#include "iccom_hw.h"
#include "iccom_drv.h"
#include "iccom_drv_common.h"
#include "iccom_drv_private.h"
#ifdef ICCOM_ENABLE_STANDBYCONTROL
#include "iccom_drv_id.h"
#include "iccom_drv_standby_private.h"
#endif

iccom_recv_data_info	g_iccom_recv_info;			/* receive data information */
unsigned char __iomem		*g_iccom_command_area;		/* command transfer area address */
static unsigned long	g_iccom_send_command_area;	/* command transfer area offset */
static unsigned char	*g_iccom_send_buf_addr;		/* send buffer */
struct semaphore		g_iccom_send_sem;			/* send semaphore */
spinlock_t				g_iccom_lock_iicr;			/* IICR spinlock */
struct completion		g_iccom_async_completion;	/* completion information */
unsigned long			g_iccom_async_recv_status;	/* asynchronous receive status */

/******************************************************************************/
/* Function   : iccom_comm_func_init                                          */
/* Description: initialize communication function                             */
/******************************************************************************/
int iccom_comm_func_init(void)
{
	unsigned long	addr = 0;
	int				ret;
	get_section_header_param	get_section;
	system_rt_section_header    section;

	memset(&g_iccom_recv_info, 0x00, sizeof(iccom_recv_data_info));	/* initialize receive data information */

	/* map command transfer area */
	get_section.section_header = &section;
	ret = sys_get_section_header(&get_section);
	if (SMAP_OK != ret) {
		MSG_ERROR("[ICCOMK]ERR|[%s] sys_get_section_header() Failed.\n", __func__);
		return SMAP_NG;
	}
	addr = section.command_area_address;

	g_iccom_command_area = ioremap_nocache(addr
										   , (ICCOM_CMD_AREA_SIZE*ICCOM_CMD_AREA_NUM)
										  );
	if (NULL == g_iccom_command_area) {
		MSG_ERROR("[ICCOMK]ERR|[%s] ioremap_nocache() Failed(Command Transfer Area).\n", __func__);
		return SMAP_NG;
	}

	g_iccom_send_command_area = ICCOM_CMD_AREA_X1;

	/* allocate memory of send buffer */
	g_iccom_send_buf_addr = kmalloc(ICCOM_CMD_AREA_SIZE , GFP_KERNEL);
	if (NULL == g_iccom_send_buf_addr) {
		MSG_ERROR("[ICCOMK]ERR|[%s] kmalloc() Failed(Send Buffer).\n", __func__);
		iounmap(g_iccom_command_area);			/* unmap command transfer area */
		return SMAP_MEMORY;
	}

	iccom_init_recv_queue();						/* initialize receive data queue */


	init_MUTEX(&g_iccom_send_sem);					/* initialize send semaphore */

	/* initialize IICR spinlock */
	spin_lock_init(&g_iccom_lock_iicr);

	init_completion(&g_iccom_async_completion);	/* initialize completion information */

	g_iccom_async_recv_status = 0;					/* initialize asynchronous receive status */

	return SMAP_OK;
}

/******************************************************************************/
/* Function   : iccom_comm_func_quit                                          */
/* Description: quit communication function                                   */
/******************************************************************************/
void iccom_comm_func_quit(void)
{
	int ret_code = SMAP_OK;
	iccom_recv_queue *recv_queue;
	iccom_recv_data *cmd_data;

	kfree(g_iccom_send_buf_addr);		/* release memory of send buffer */

	/* delete all receive data queue */
	for ( ;; ) {
		ret_code = iccom_get_recv_queue(NULL, &recv_queue);
		if (SMAP_OK != ret_code) {
			/* queue does not exist */
			MSG_MED("[ICCOMK]INF|[%s] iccom_get_recv_queue() Queue did not exist.\n", __func__);
			break;
		}

		MSG_MED("[ICCOMK]INF|[%s] iccom_get_recv_queue() Queue exist.\n", __func__);

		if (NULL != recv_queue->recv_data) {
			cmd_data = (iccom_recv_data *)recv_queue->recv_data;
			if (ICCOM_MEM_DYNAMIC == cmd_data->mng_info) {	/* receive data use dynamic area */
				kfree(recv_queue->recv_data);				/* release receive buffer */
			}
		}
		iccom_delete_recv_queue(recv_queue);
	}

	/* release receive data information */
	if (NULL != g_iccom_recv_info.recv_data) {
		MSG_MED("[ICCOMK]INF|[%s] Release Receive Data Information\n", __func__);
		kfree(g_iccom_recv_info.recv_data);
	}

	iounmap(g_iccom_command_area);	/* unmap command transfer area */
}

/******************************************************************************/
/* Function   : iccom_send_command                                            */
/* Description: send message                                                  */
/******************************************************************************/
int iccom_send_command(void *handle, int type, iccom_cmd_send_param *send_param)
{
	int ret_code = SMAP_OK;

	if ((NULL == handle) || (NULL == send_param)) {
		MSG_ERROR("[ICCOMK]ERR|[%s] Parameter Error! handle(0x%08X), send_param(0x%08X)\n", __func__, (unsigned int)handle, (unsigned int)send_param);
		return SMAP_PARA_NG;
	}

	MSG_MED("[ICCOMK]INF|[%s] type        [%d]\n", __func__, type);
	MSG_MED("[ICCOMK]INF|[%s] task_id     [%d]\n", __func__, send_param->task_id);
	MSG_MED("[ICCOMK]INF|[%s] function_id [%d]\n", __func__, send_param->function_id);
	MSG_MED("[ICCOMK]INF|[%s] send_mode   [%d]\n", __func__, send_param->send_mode);
	MSG_MED("[ICCOMK]INF|[%s] send_num    [%d]\n", __func__, send_param->send_num);

	ICCOM_DOWN_TIMEOUT(&g_iccom_send_sem);	/* lock a semaphore */

	ret_code = iccom_write_command(handle, type, send_param);		/* write message parameters */

	up(&g_iccom_send_sem);	/* unlock a semaphore */

	return ret_code;
}

/******************************************************************************/
/* Function   : iccom_write_command                                           */
/* Description: write message to RT                                           */
/******************************************************************************/
int iccom_write_command(void *handle, int type, iccom_cmd_send_param *send_param)
{
	int ret_code = SMAP_OK;

	iccom_cmd_info	 cmd_info;
	iccom_msg_header header_info;

	unsigned long	i = 0;
	unsigned long	cnt = 0;
	unsigned long	area_num = 0;
	unsigned long	data_size = 0;
	unsigned long	out_size = 0;

	void __iomem *output_addr = NULL;
	void __iomem *output_top_addr = NULL;

	unsigned long	dummy_read;

	iccom_drv_cmd_io_data	output_area;
	iccom_write_data_info	write_data;

	memset(&cmd_info, 0x00, sizeof(iccom_cmd_info));
	memset(&header_info, 0x00, sizeof(iccom_msg_header));

#ifdef ICCOM_ENABLE_STANDBYCONTROL
	/* increment the internal standby control counter
	   before sending a synchronous command. */
	if (ICCOM_DRV_SYNC == send_param->send_mode) {
		ret_code = iccom_rtctl_before_send_cmd_check_standby(send_param->function_id);
		if (SMAP_OK != ret_code) {
			return ret_code;
		}
	}
#endif

	/* calculate total size of the data */
	for (i = 0; i < send_param->send_num; i++) {
		data_size += send_param->send_data[i].size;
	}

	/* calculate total size of the transmission data */
	if (data_size <= ICCOM_CMD_DATA_SIZE) {		/* independent send ? */
		MSG_MED("[ICCOMK]INF|[%s]total send size(%d) <= Command area size(%d)\n", __func__, (unsigned int)data_size, ICCOM_CMD_DATA_SIZE);
		area_num = 1;
		cmd_info.total_size = data_size + ICCOM_CMD_INFO_SIZE + ICCOM_CMD_HEADER_SIZE;
	} else {
		MSG_MED("[ICCOMK]INF|[%s]total send size(%d) > Command area size(%d)\n", __func__, (unsigned int)data_size, ICCOM_CMD_DATA_SIZE);
		if (0 == ((data_size - ICCOM_CMD_DATA_SIZE) % ICCOM_CMD_BLOCK_SIZE)) {
			cnt = (data_size - ICCOM_CMD_DATA_SIZE) / ICCOM_CMD_BLOCK_SIZE;
		} else {
			cnt = ((data_size - ICCOM_CMD_DATA_SIZE) / ICCOM_CMD_BLOCK_SIZE) + 1;
		}
		area_num = cnt + 1;
		cmd_info.total_size = data_size + ICCOM_CMD_HEADER_SIZE + (area_num * ICCOM_CMD_INFO_SIZE);
	}

	MSG_MED("[ICCOMK]INF|[%s] Total Size     [%d]\n", __func__, (unsigned int)cmd_info.total_size);
	MSG_MED("[ICCOMK]INF|[%s] Total Area Num [%d]\n", __func__, (unsigned int)area_num);

	/* set message header */
	header_info.task_id		= send_param->task_id;
	header_info.func_id		= send_param->function_id;
	header_info.send_mode	= send_param->send_mode;
	if (ICCOM_DRV_ASYNC_NO_RET == send_param->send_mode) {
		header_info.resp_kind	= ICCOM_RESP_KIND_ON;
	}
	header_info.handle		= handle;

	/* set write data information */
	write_data.send_num		= send_param->send_num;
	write_data.send_data	= send_param->send_data;
	write_data.e_send_num	= 0;
	write_data.e_send_posi	= 0;

	for (i = 0; i < area_num; i++) {
		unsigned int buff_kind;
		unsigned int buff_position;

#ifdef ICCOM_ENABLE_STANDBYCONTROL
		/* wait until RT state turns active. */
		ret_code = iccom_rtctl_wait_rt_state_active();
		if (SMAP_OK != ret_code) {
			break;
		}
#endif

		output_top_addr = iccom_get_send_command_area();
		output_addr = output_top_addr + ICCOM_CMD_INFO_SIZE;

		if (0 == i) {
			/* write message header */
			ret_code = iccom_copy_to_command_area(output_addr, &header_info, ICCOM_CMD_HEADER_SIZE, 0);
			if (ret_code != SMAP_OK) {
				MSG_ERROR("[ICCOMK]ERR|[%s] iccom_copy_to_command_area() Failed(header).cnt=%d\n", __func__, (unsigned int)i);
				ret_code = SMAP_NG;
				break;
			}
			cmd_info.block_size = ICCOM_CMD_HEADER_SIZE;
			output_addr += ICCOM_CMD_HEADER_SIZE;
			output_area.size = ICCOM_CMD_DATA_SIZE;
		} else {
			cmd_info.block_size = 0;
			output_area.size = ICCOM_CMD_BLOCK_SIZE;
		}

		output_area.data = output_addr;

		/* write data */
		ret_code = iccom_write_command_data(&output_area, &write_data, &out_size, type);
		if (SMAP_OK == ret_code) {
			cmd_info.block_size += out_size;
		} else {
			MSG_ERROR("[ICCOMK]ERR|[%s] iccom_copy_to_command_area() Failed(data).cnt=%d\n", __func__, (unsigned int)i);
			ret_code = SMAP_NG;
			break;
		}

		MSG_MED("[ICCOMK]INF|[%s] Block Size     [%d]\n", __func__, (unsigned int)cmd_info.block_size);

		/* write command information */
		ret_code = iccom_copy_to_command_area(output_top_addr, &cmd_info, ICCOM_CMD_INFO_SIZE, 0);
		if (ret_code != SMAP_OK) {
			MSG_ERROR("[ICCOMK]ERR|[%s] iccom_copy_to_command_area() Failed(info).cnt=%d\n", __func__, (unsigned int)i);
			ret_code = SMAP_NG;
			break;
		}

		/* synchronize APP and RT */
		asm("DMB");
		outer_sync();
		dummy_read = ioread32(output_top_addr+4);

		MSG_MED("[ICCOMK]INF|[%s] Command area address[0x%8X]=\n", __func__, (unsigned int)output_top_addr);
		MSG_MED("[ICCOMK]INF|[%s] Dummy Read[0x%8X]=%d\n", __func__, (unsigned int)(output_top_addr+4), (unsigned int)dummy_read);

		/* determination of a data position */
		if (1 == area_num) {
			buff_kind = ICCOMIICR_DATA_ONLY;
		} else {
			if (0 == i) {
				buff_kind = ICCOMIICR_DATA_TOP;
			} else if (i == (area_num-1)) {
				buff_kind = ICCOMIICR_DATA_BTM;
			} else {
				buff_kind = ICCOMIICR_DATA_MID;
			}
		}

		/* determination of a buffer position */
		if (ICCOM_CMD_AREA_X1 == g_iccom_send_command_area) {
			buff_position = ICCOMIICR_CMD1;
		} else {
			buff_position = ICCOMIICR_CMD2;
		}

		MSG_MED("[ICCOMK]INF|[%s] buff_kind     [%d]\n", __func__, buff_kind);
		MSG_MED("[ICCOMK]INF|[%s] buff_position [%d]\n", __func__, buff_position);

		iccom_iccomiicr_int(buff_kind, buff_position);	/* internal interrupt (APP--->RT) */
	}

#ifdef ICCOM_ENABLE_STANDBYCONTROL
	/* decrement the internal standby control counter
	   if any error occurs after sending a synchronous command. */
	if (SMAP_OK != ret_code) {
		if (ICCOM_DRV_SYNC == send_param->send_mode) {
			iccom_rtctl_after_send_cmd_check_standby(send_param->function_id);
		}
		return ret_code;
	}
	/* wait until RT state turns standby if the command is EVENT_STATUS_STANDBYCONTROL. */
	if (EVENT_STATUS_STANDBYCONTROL == send_param->function_id) {
		ret_code = iccom_rtctl_watch_rt_state();
	}
#endif

	return ret_code;
}

/******************************************************************************/
/* Function   : iccom_get_send_command_area                                   */
/* Description: get address of command transfer area for send                 */
/******************************************************************************/
unsigned char __iomem *iccom_get_send_command_area(void)
{
	unsigned long iicr;
	unsigned long status;
	void __iomem *send_command_area = NULL;

	MSG_MED("[ICCOMK]INF|[%s] RD_ICCOMIICR()\n", __func__);

	/* wait for initialization of ICCOMIICR */
	do {
		iicr = RD_ICCOMIICR();
		if (ICCOMIICR_INIT == iicr) {
			break;
		}
	} while (1);

	MSG_MED("[ICCOMK]INF|[%s] RD_ICCOMCSR()\n", __func__);

	/* wait to become free command transfer area */
	do {
		status = RD_ICCOMCSR();
		if ((0 == (status & ICCOMCSR_CMD1))
			|| (0 == (status & ICCOMCSR_CMD2))) {
			break;
		}
	} while (1);

	/* determination of a buffer offset */
	if (ICCOMCSR_CMD1 == (status & ICCOMCSR_CMD1)) {
		MSG_MED("[ICCOMK]INF|[%s] ICCOMCSR_CMD1 is 1.\n", __func__);
		g_iccom_send_command_area = ICCOM_CMD_AREA_X2;
	} else if (ICCOMCSR_CMD2 == (status & ICCOMCSR_CMD2)) {
		MSG_MED("[ICCOMK]INF|[%s] ICCOMCSR_CMD2 is 1.\n", __func__);
		g_iccom_send_command_area = ICCOM_CMD_AREA_X1;
	} else {
		MSG_MED("[ICCOMK]INF|[%s] ICCOMCSR_CMD1 and ICCOMCSR_CMD2 are 0.\n", __func__);
		g_iccom_send_command_area = (g_iccom_send_command_area == ICCOM_CMD_AREA_X1) ? ICCOM_CMD_AREA_X2 : ICCOM_CMD_AREA_X1;
	}

	send_command_area = g_iccom_command_area + g_iccom_send_command_area;

	return send_command_area;
}

/******************************************************************************/
/* Function   : iccom_write_command_data                                      */
/* Description: write data to command transfer area                           */
/******************************************************************************/
int iccom_write_command_data(iccom_drv_cmd_io_data *output_area, iccom_write_data_info *write_data, unsigned long *out_size, int type)
{
	int ret_code = SMAP_OK;

	unsigned int	cmd_area_rsize;
	unsigned char __iomem *cmd_area;
	unsigned int	send_rsize;
	unsigned char	*send_data;
	unsigned int	write_size;

	/* initialize command area size and address */
	cmd_area_rsize = output_area->size;
	cmd_area       = output_area->data;

	*out_size = 0;

	for ( ;; ) {
		MSG_MED("[ICCOMK]INF|[%s] cmd_area_rsize=%d, cmd_area=0x%8X\n", __func__, (unsigned int)cmd_area_rsize, (unsigned int)cmd_area);
		MSG_MED("[ICCOMK]INF|[%s] e_send_num=%d, e_send_posi=%d\n", __func__, (unsigned int)write_data->e_send_num, (unsigned int)write_data->e_send_posi);

		if (write_data->e_send_num  >= write_data->send_num) {
			MSG_MED("[ICCOMK]INF|[%s] There is no send data.\n", __func__);
			break;
		}

		send_rsize = write_data->send_data[write_data->e_send_num].size - write_data->e_send_posi;
		send_data  = (unsigned char *)write_data->send_data[write_data->e_send_num].data + write_data->e_send_posi;

		MSG_MED("[ICCOMK]INF|[%s] send_rsize=%d, send_data=0x%8X\n", __func__, (unsigned int)send_rsize, (unsigned int)send_data);
		MSG_MED("[ICCOMK]INF|[%s] cmd_area_rsize=%d\n", __func__, (unsigned int)cmd_area_rsize);

		/* set write size */
		if (send_rsize <= cmd_area_rsize) {
			write_size = send_rsize;
		} else {
			write_size = cmd_area_rsize;
		}

		MSG_MED("[ICCOMK]INF|[%s] write_size=%d\n", __func__, (unsigned int)write_size);

		/* write data to command transfer area */
		ret_code = iccom_copy_to_command_area(cmd_area, send_data, write_size, type);
		if (SMAP_OK != ret_code) {
			MSG_ERROR("[ICCOMK]ERR|[%s] iccom_copy_to_command_area() Failed.\n", __func__);
			ret_code = SMAP_NG;
			break;
		}

		cmd_area_rsize -= write_size;
		write_data->e_send_posi += write_size;
		*out_size += write_size;

		/* command transfer area is full */
		if (0 == cmd_area_rsize) {
			/* send data is not remaining */
			if (0 == (send_rsize - write_size)) {
				/* use next command transfer area */
				write_data->e_send_num++;
				write_data->e_send_posi = 0;
			}
			break;
		}

		cmd_area += write_size;

		/* send next data */
		write_data->e_send_num++;
		write_data->e_send_posi = 0;
	}

	return ret_code;
}


/******************************************************************************/
/* Function   : iccom_iccomiicr_int                                           */
/* Description: internal interrupt (APP--->RT)                                */
/******************************************************************************/
void iccom_iccomiicr_int(unsigned long buff_kind, unsigned long buff_position)
{
	unsigned long data = ICCOMIICR_INT;
	unsigned long	   flag;
	unsigned long iicr;

	spin_lock_irqsave(&g_iccom_lock_iicr, flag);

	/* wait for initialization of ICCOMIICR */
	do {
		iicr = RD_ICCOMIICR();
		if (ICCOMIICR_INIT == iicr) {
			break;
		}
	} while (1);

	/* set ICCOMIICR parameters */
	data |= ICCOMIICR_READ;
	data |= buff_kind;
	data |= buff_position;

	asm("DMB");
	WT_ICCOMIICR(data);	/* internal interrupt (APP--->RT) */

	spin_unlock_irqrestore(&g_iccom_lock_iicr, flag);
}

/******************************************************************************/
/* Function   : iccom_copy_to_command_area                                    */
/* Description: copy data to command transfer area                            */
/******************************************************************************/
int iccom_copy_to_command_area(void __iomem *cmd_area, void *from_addr, unsigned long size, int type)
{
	unsigned long ret_code;

	if (ICCOM_TYPE_USER == type) {
		/* copy from User to Kernel */
		ret_code = copy_from_user(g_iccom_send_buf_addr,
				(void __force __user *) from_addr, size);
		if (0 != ret_code) {
			return SMAP_NG;
		}
		memcpy_toio(cmd_area, g_iccom_send_buf_addr, size);	/* copy from Kernel to Physical Memory */
	} else {
		memcpy_toio(cmd_area, from_addr, size);				/* copy from Kernel to Physical Memory */
	}
	return SMAP_OK;
}
