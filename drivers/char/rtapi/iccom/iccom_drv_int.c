/*
 * iccom_drv_int.c
 *     Inter Core Communication driver function file for interrupts.
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
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include "log_kernel.h"
#include "iccom_hw.h"
#include "iccom_drv.h"
#include "iccom_drv_common.h"
#include "iccom_drv_private.h"
#include "rtds_memory_drv.h"
#ifdef ICCOM_ENABLE_STANDBYCONTROL
#include "iccom_drv_standby_private.h"
#endif

static iccom_recv_data_err  g_iccom_recv_data_err;  /* receive data (memory allocate error) */

/* MACRO */
#define GET_STATIC_RECV_AREA(x)														\
	do {																			\
		if (g_iccom_recv_data_err.index >= ICCOM_ERROR_DATA_AREA_MAX) {				\
			g_iccom_recv_data_err.index = 0;										\
		}																			\
		x = (void *)&g_iccom_recv_data_err.recv_data[g_iccom_recv_data_err.index];	\
		g_iccom_recv_data_err.index++;												\
	} while (0)

/******************************************************************************/
/* Function   : iccom_read_comp                                               */
/* Description: read complete notify                                          */
/******************************************************************************/
static
void iccom_read_comp(
	unsigned long	   cmd_posi
)
{
	unsigned long	   iicr;
	unsigned long	   flag;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	spin_lock_irqsave(&g_iccom_lock_iicr, flag);

	/* wait for initialization of ICCOMIICR */
	do {
		iicr = RD_ICCOMIICR();
		if (ICCOMIICR_INIT == iicr) {
			break;
		}
	} while (1);

	/* set IICR parameters */
	iicr = (cmd_posi | ICCOMIICR_INT);
	WT_ICCOMIICR(iicr);

	spin_unlock_irqrestore(&g_iccom_lock_iicr, flag);

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_read_command                                            */
/* Description: read from command area (responce data)                        */
/******************************************************************************/
static
void iccom_read_command(
	unsigned long	  cmd_posi,
	unsigned long	  data_posi
) {
	int				  ret = SMAP_OK;
	unsigned char __iomem		  *p_cmd_addr;
	unsigned char	  *p_recv_data;
	unsigned long	  data_size;
	iccom_cmd_info	  cmd_info;
	unsigned long	  alloc_size;
	iccom_recv_data	  *p_recv_head;
	struct completion *p_completion;
	int				  result_code = SMAP_OK;
	iccom_handle_list	*tmp_p = NULL;
	iccom_handle_list	*handle_list = NULL;
	unsigned long		flag;

	MSG_MED("[ICCOMK]IN |[%s] cmd_posi = 0x%08x,data_posi = 0x%08x\n", __func__, (unsigned int)cmd_posi, (unsigned int)data_posi);
	/* calculate address of command transfer area */
	p_cmd_addr =
		(g_iccom_command_area + ICCOM_CMD_AREA_Y1) +
			((cmd_posi >> ICCOMEICR_BIT_SHIFT_CMD) * ICCOM_CMD_AREA_SIZE);

	memcpy_fromio(&cmd_info, p_cmd_addr, sizeof(cmd_info));	/* copy from physical memory to kernel (command information) */
	do {
		if (ICCOMEICR_DATA_TOP == (ICCOMEICR_DATA_TOP & data_posi)) {
			/* calculate allocation size */
			alloc_size = (cmd_info.total_size + sizeof(unsigned long)) -
				((cmd_info.total_size-1) / ICCOM_CMD_AREA_SIZE * sizeof(cmd_info));

			g_iccom_recv_info.recv_data = kmalloc(alloc_size, GFP_ATOMIC);	/* allocate memory of receive data */
			if (NULL != g_iccom_recv_info.recv_data) {
				g_iccom_recv_info.recv_size  = alloc_size;
				g_iccom_recv_info.write_size = 0;

				data_size = cmd_info.block_size;	/* set message header size and data unit size */

				p_recv_head = g_iccom_recv_info.recv_data;
				p_recv_head->mng_info = ICCOM_MEM_DYNAMIC;
			} else{
				MSG_ERROR("[ICCOMK]ERR| receive data area was not obtained.\n");
				GET_STATIC_RECV_AREA(g_iccom_recv_info.recv_data);
				g_iccom_recv_info.recv_size  = sizeof(iccom_recv_data);
				g_iccom_recv_info.write_size = 0;

				data_size = sizeof(iccom_msg_header);   /* set message header size */

				p_recv_head = g_iccom_recv_info.recv_data;
				p_recv_head->mng_info = ICCOM_MEM_STATIC;

				data_posi |= ICCOMEICR_DATA_BTM;

				result_code = SMAP_MEMORY;  /* set internal result code */
			}

			memcpy(&p_recv_head->cmd_info, &cmd_info, sizeof(iccom_cmd_info));  /* copy command information */

			p_recv_data = (unsigned char *)&p_recv_head->msg_header;
			g_iccom_recv_info.write_size += (sizeof(iccom_recv_data) - sizeof(iccom_msg_header));
		} else {
			if (NULL == g_iccom_recv_info.recv_data) {
				MSG_ERROR("[ICCOMK]ERR| received data does not exist.\n");
				break;
			}
			p_recv_data = (unsigned char *)(g_iccom_recv_info.recv_data + g_iccom_recv_info.write_size);

			data_size = cmd_info.block_size;	/* set data unit size */
		}
		if (0 != data_size) {
			memcpy_fromio(p_recv_data, (p_cmd_addr + sizeof(iccom_cmd_info)), data_size); /* copy from physical memory to kernel (data unit) */
			g_iccom_recv_info.write_size += data_size;
		}
		if (ICCOMEICR_DATA_BTM == (ICCOMEICR_DATA_BTM & data_posi)) {
			p_recv_head = (iccom_recv_data *)g_iccom_recv_info.recv_data;

			spin_lock_irqsave(&g_iccom_lock_handle_list, flag);
			list_for_each_entry(handle_list, &g_iccom_list_handle, list) {
				if (handle_list->handle == p_recv_head->msg_header.handle) {
					tmp_p = handle_list;
					break;
				}
			}
			spin_unlock_irqrestore(&g_iccom_lock_handle_list, flag);

			if (NULL == tmp_p) {
				MSG_ERROR("[ICCOMK]ERR| ICCOM handle does not exist.\n");
				/* handle error */
				ret = SMAP_NG;
				break;
			}

			/* set completion information */
			if (ICCOM_DRV_SYNC == p_recv_head->msg_header.send_mode) {
				p_completion = ((iccom_drv_handle *)p_recv_head->msg_header.handle)->sync_completion;
			} else {
				p_completion = ((iccom_drv_handle *)p_recv_head->msg_header.handle)->async_completion;
			}

			if (NULL == p_completion) {
				MSG_ERROR("[ICCOMK]ERR| completion is illegal.\n");
				ret = SMAP_NG;
				break;
			}

			/* put data to receive queue */
			ret = iccom_put_recv_queue(p_completion,
									result_code,
									g_iccom_recv_info.recv_data,
									g_iccom_recv_info.recv_size);
			if (SMAP_OK != ret) {
				MSG_ERROR("[ICCOMK]ERR| iccom_put_recv_queue() Error(%d).\n", (unsigned int)ret);
			} else {
				memset(&g_iccom_recv_info, 0, sizeof(g_iccom_recv_info));
			}
			complete(p_completion);   /* release completion for receive */
		}
	} while (0);
	if (SMAP_OK != ret) {
		if (NULL != g_iccom_recv_info.recv_data) {
			if (ICCOM_MEM_DYNAMIC == ((iccom_recv_data *)g_iccom_recv_info.recv_data)->mng_info) {
				kfree(g_iccom_recv_info.recv_data);   /* release memory of receive data */
			}
			memset(&g_iccom_recv_info, 0, sizeof(g_iccom_recv_info));
		}
	}
	iccom_read_comp(cmd_posi);	/* read complete notify */
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_read_fatal                                              */
/* Description: read from command area (fatal information)                    */
/******************************************************************************/
static
void iccom_read_fatal(
	unsigned long	   cmd_posi
)
{
	int				   ret;
	unsigned char __iomem		   *p_cmd_addr;
	unsigned long	   data_size;
	iccom_fatal_info   *p_fatal_info;
	iccom_recv_data	   *p_recv_head;
	unsigned long	   recv_size;
	struct completion  *p_completion;
	int				   result_code = SMAP_OK;

	MSG_MED("[ICCOMK]IN |[%s] cmd_posi = 0x%08x\n", __func__, (unsigned int)cmd_posi);
	ret = iccom_get_fatal(&p_fatal_info); /* get fatal information */
	if (SMAP_OK == ret) {
		/* calculate command transfer area address */
		p_cmd_addr =
			(g_iccom_command_area + ICCOM_CMD_AREA_Y1) +
				((cmd_posi >> ICCOMEICR_BIT_SHIFT_CMD) * ICCOM_CMD_AREA_SIZE);
		recv_size =  (sizeof(iccom_recv_data) + ICCOM_CMD_AREA_SIZE);
		p_recv_head = kmalloc(recv_size, GFP_ATOMIC);  /* allocate memory of fatal information */
		if (NULL != p_recv_head) {
			data_size = ICCOM_CMD_AREA_SIZE;	/* set data unit size */

			p_recv_head->mng_info = ICCOM_MEM_DYNAMIC;
			p_recv_head->cmd_info.block_size = (sizeof(iccom_msg_header) + ICCOM_CMD_AREA_SIZE);
			p_recv_head->cmd_info.total_size = p_recv_head->cmd_info.block_size + sizeof(iccom_cmd_info);
		} else {
			GET_STATIC_RECV_AREA(p_recv_head);
			recv_size = sizeof(iccom_recv_data);

			data_size = sizeof(iccom_msg_header);   /* set message header size */

			p_recv_head->mng_info = ICCOM_MEM_STATIC;
			p_recv_head->cmd_info.block_size = sizeof(iccom_msg_header);
			p_recv_head->cmd_info.total_size = p_recv_head->cmd_info.block_size + sizeof(iccom_cmd_info);

			result_code = SMAP_MEMORY;  /* set internal result code */
		}
		memset(&p_recv_head->msg_header, 0, sizeof(p_recv_head->msg_header));
		p_recv_head->msg_header.handle   = p_fatal_info->handle;
		p_recv_head->msg_header.ret_code = SMAP_ERROR_DIED;
		memcpy_fromio((p_recv_head + 1), p_cmd_addr, data_size);
		iccom_debug_output_fatal_info(
			(unsigned char *)(p_recv_head + 1), data_size);
		p_completion = ((iccom_drv_handle *)p_recv_head->msg_header.handle)->async_completion;
		if (NULL == p_completion) {
			MSG_ERROR("[ICCOMK]ERR| ICCOM handle does not exist.\n");
			if (ICCOM_MEM_DYNAMIC == p_recv_head->mng_info) {
				kfree(p_recv_head);
			}
		} else {
			/* put data to receive queue */
			ret = iccom_put_recv_queue(p_completion,
									   result_code,
									   p_recv_head,
									   recv_size);
			if (SMAP_OK != ret) {
				MSG_ERROR("[ICCOMK]ERR| iccom_put_recv_queue() Error(%d).\n", (unsigned int)ret);
				if (ICCOM_MEM_DYNAMIC == p_recv_head->mng_info) {
					kfree(p_recv_head);
				}
			}
			complete(p_completion);   /* release completion for receive */
		}
	}
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_iccomeicr_int                                           */
/* Description: external interrupt processing                                 */
/******************************************************************************/
irqreturn_t iccom_iccomeicr_int(
	int				 irq_number,
	void			 *device_id
)
{
	unsigned long	   eicr;
	unsigned long	   gsr;
	unsigned long	   data_posi;
	unsigned long	   cmd_posi;
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	eicr = RD_ICCOMEICR();
	WT_ICCOMEICR(ICCOMEICR_INIT);

	if (eicr == ICCOMEICR_INIT) {
		MSG_MED("[ICCOMK]OUT|[%s] eicr = 0x%08x\n", __func__, (unsigned int)eicr);
		return IRQ_HANDLED;
	}
	if (ICCOMEICR_WRITE == (eicr & ICCOMEICR_WRITE)) {
		data_posi = (eicr & ICCOMEICR_DATA_BIT);
		cmd_posi  = (eicr & ICCOMEICR_CMD_BIT);

		/* fatal event notice  */
		if (ICCOMEICR_FATAL == (eicr & ICCOMEICR_FATAL)) {
			do {
				gsr = RD_ICCOMGSR();
				if (ICCOMGSR_FATAL_COMP == gsr) {
					break;
				}
			} while (1);
#ifdef ICCOM_ENABLE_STANDBYCONTROL
			iccom_rtctl_set_rt_fatal_error();
#endif
			rtds_memory_drv_dump_mpro();
			iccom_read_fatal(cmd_posi);
		} else {
			/* event responce */
#ifdef ICCOM_ENABLE_STANDBYCONTROL
			if (ICCOMEICR_INIT_COMP == (eicr & ICCOMEICR_INIT_COMP)) {
				MSG_LOW("[ICCOMK]   |active complete\n");
				iccom_rtctl_set_rt_state();
				iccom_read_comp(cmd_posi);    /* read complete notify */
			} else {
				iccom_read_command(cmd_posi, data_posi);
			}
#else
			iccom_read_command(cmd_posi, data_posi);
#endif
		}
	}
	MSG_MED("[ICCOMK]OUT|[%s] eicr = 0x%08x\n", __func__, (unsigned int)eicr);
	return IRQ_HANDLED;
}
