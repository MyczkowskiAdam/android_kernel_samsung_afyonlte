/*
 * screen_graphics.c
 *  screen graphics function file.
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
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <system_memory.h>
#include <log_kernel.h>
#include <screen_graphics.h>
#include <iccom_drv_id.h>
#include <iccom_drv.h>
#include <screen_id.h>
#include <iccom_drv_standby.h>
#include <rtds_memory_drv.h>
#include "screen_graphics_private.h"


static void screen_graphics_request(void *handle_data, int result,
				int function_id, unsigned char *resp_data, int resp_size)
{
	int								drv_result;
	unsigned long					user_data;
	screen_grap_handle				*grap_handle = handle_data;
	iccom_drv_enable_standby_param	ena_standby;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);
	MSG_MED("[RTAPIK]    |handle     [0x%08X]\n",	(unsigned int)handle_data);
	MSG_MED("[RTAPIK]    |result     [%d]\n",		result);
	MSG_MED("[RTAPIK]    |function_id[%d]\n",		function_id);
	MSG_MED("[RTAPIK]    |resp_data  [0x%08X]\n",	(unsigned int)resp_data);
	MSG_MED("[RTAPIK]    |resp_size  [%d]\n",		resp_size);

	if (handle_data == NULL) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s] handle NULL error\n",
		__func__);
		return;
	}

	switch (function_id) {
	case EVENT_GRAPHICS_IMAGECONVERSION:
		ena_standby.handle = grap_handle->handle;
		drv_result = iccom_drv_enable_standby(&ena_standby);
		if (drv_result != SMAP_OK) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%s] iccom_drv_enable_standby() result = [%d]\n",
			__func__,
			drv_result);
		}

		if ((resp_data == NULL) ||
			(resp_size < (int)sizeof(unsigned long))) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%s] resp_data [0x%08X] or resp_size[%d] error\n",
			__func__,
			(unsigned int)resp_data,
			resp_size);
			return;
		}

		user_data = *((unsigned long *)resp_data);

		if (grap_handle->notify_graphics_image_conv != NULL) {
			grap_handle->notify_graphics_image_conv(result, user_data);
		}
		break;

	case EVENT_GRAPHICS_IMAGEBLEND:
		ena_standby.handle = grap_handle->handle;
		drv_result = iccom_drv_enable_standby(&ena_standby);
		if (drv_result != SMAP_OK) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%s] iccom_drv_enable_standby() result = [%d]\n",
			__func__,
			drv_result);
		}

		if ((resp_data == NULL) ||
			(resp_size < (int)sizeof(unsigned long))) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%s] resp_data [0x%08X] or resp_size[%d] error\n",
			__func__,
			(unsigned int)resp_data,
			resp_size);
			return;
		}

		user_data = *((unsigned long *)resp_data);

		if (grap_handle->notify_graphics_image_blend != NULL) {
			grap_handle->notify_graphics_image_blend(result, user_data);
		}
		break;

	case EVENT_GRAPHICS_IMAGEOUTPUT:
		ena_standby.handle = grap_handle->handle;
		drv_result = iccom_drv_enable_standby(&ena_standby);
		if (drv_result != SMAP_OK) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%s] iccom_drv_enable_standby() result = [%d]\n",
			__func__,
			drv_result);
		}

		if ((resp_data == NULL) ||
			(resp_size < (int)sizeof(unsigned long))) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%s] resp_data [0x%08X] or resp_size[%d] error\n",
			__func__,
			(unsigned int)resp_data,
			resp_size);
			return;
		}

		user_data = *((unsigned long *)resp_data);

		if (grap_handle->notify_graphics_image_output != NULL) {
			grap_handle->notify_graphics_image_output(result, user_data);
		}
		break;

	case EVENT_GRAPHICS_IMAGEEDIT:

		ena_standby.handle = grap_handle->handle;
		drv_result = iccom_drv_enable_standby(&ena_standby);
		if (drv_result != SMAP_OK) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%s] iccom_drv_enable_standby() result = [%d]\n",
			__func__,
			drv_result);
		}

		if ((resp_data == NULL) ||
			(resp_size < (int)sizeof(unsigned long))) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%s] resp_data [0x%08X] or resp_size[%d] error\n",
			__func__,
			(unsigned int)resp_data,
			resp_size);
			return;
		}

		user_data = *((unsigned long *)resp_data);

		if (grap_handle->notify_graphics_image_edit != NULL) {
			grap_handle->notify_graphics_image_edit(result, user_data);
		}
		break;

	default:
		MSG_ERROR(
		"[RTAPIK] ERR|[%s] function_id[%d] error\n",
		__func__,
		function_id);
		break;
	}

	MSG_HIGH("[RTAPIK] OUT|[%s] (line = %d)\n", __func__, __LINE__);
	return;
}
EXPORT_SYMBOL(screen_graphics_request);

static int screen_graphics_change_rtaddr(
	void			*handle,
	unsigned char	*address,
	void			*apmem_handle,
	unsigned char	**change_address,
	void			**change_apmem_handle)
{
	int result;
	system_mem_ap_get_apmem_id		ap_get_apmem_id;
	system_mem_ap_change_rtaddr		ap_change_rtaddr;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (NULL == handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	if ((NULL == address) || (NULL == apmem_handle)) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	if ((NULL == change_address) || (NULL == change_apmem_handle)) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	ap_get_apmem_id.handle = handle;
	ap_get_apmem_id.apmem_handle = apmem_handle;

	*change_apmem_handle = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
	if (0 == *change_apmem_handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	ap_change_rtaddr.handle         = handle;
	ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
	ap_change_rtaddr.apmem_handle   = apmem_handle;
	ap_change_rtaddr.apmem_apaddr   = (unsigned int)(address);

	result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
	if (result != SMAP_LIB_MEMORY_OK) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}
	*change_address = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;

	MSG_HIGH(
	"[RTAPIK] OUT|[%s][%d] ret = [%d]\n",
	__func__,
	__LINE__,
	SMAP_LIB_GRAPHICS_OK);
	return SMAP_LIB_GRAPHICS_OK;
}

void *screen_graphics_new(screen_grap_new *grap_new)
{
	screen_grap_handle		*grap_handle;
	iccom_drv_init_param	iccom_init;
	iccom_drv_cleanup_param	iccom_cleanup;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (grap_new == NULL) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d] parameter NULL\n",
		__LINE__);
		return NULL;
	}
	if (grap_new->notify_graphics_image_conv == NULL) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d] callback func NULL\n",
		__LINE__);
		return NULL;
	}

	MSG_MED("[RTAPIK]    |notify_graphics_image_conv[0x%08X]\n",
	(unsigned int)grap_new->notify_graphics_image_conv);

	if (grap_new->notify_graphics_image_blend == NULL) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d] callback func NULL\n",
		__LINE__);
		return NULL;
	}

	MSG_MED("[RTAPIK]    |notify_graphics_image_blend[0x%08X]\n",
	(unsigned int)grap_new->notify_graphics_image_blend);

	if (grap_new->notify_graphics_image_output == NULL) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d] callback func NULL\n",
		__LINE__);
		return NULL;
	}

	MSG_MED("[RTAPIK]    |notify_graphics_image_output[0x%08X]\n",
	(unsigned int)grap_new->notify_graphics_image_output);

	if (grap_new->notify_graphics_image_edit == NULL) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d] callback func NULL\n",
		__LINE__);
		return NULL;
	}

	MSG_MED("[RTAPIK]    |notify_graphics_image_edit[0x%08X]\n",
	(unsigned int)grap_new->notify_graphics_image_edit);

	grap_handle = kmalloc(
			sizeof(screen_grap_handle), GFP_KERNEL);
	if (grap_handle == NULL) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] kmalloc() grap_handle NULL error\n",
		__func__,
		__LINE__);
		return NULL;
	}

	grap_handle->notify_graphics_image_conv =
		grap_new->notify_graphics_image_conv;

	grap_handle->notify_graphics_image_blend =
		grap_new->notify_graphics_image_blend;

	grap_handle->notify_graphics_image_output =
		grap_new->notify_graphics_image_output;

	grap_handle->notify_graphics_image_edit =
		grap_new->notify_graphics_image_edit;

	iccom_init.user_data = (void *)grap_handle;
	iccom_init.comp_notice = screen_graphics_request;
	grap_handle->handle = iccom_drv_init(&iccom_init);
	if (NULL == grap_handle->handle) {
		kfree(grap_handle);
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] iccom_drv_init() aInfo NULL error\n",
		__func__,
		__LINE__);
		return NULL;
	}

	grap_handle->rtds_mem_handle = rtds_memory_drv_init();
	if (NULL == grap_handle->rtds_mem_handle) {
		iccom_cleanup.handle = grap_handle->handle;
		iccom_drv_cleanup(&iccom_cleanup);
		kfree(grap_handle);
		MSG_ERROR(
		"[RTAPIK] ERR|[%s][%d] rtds_memory_drv_init() aInfo NULL error\n",
		__func__,
		__LINE__);
		return NULL;
	}

	MSG_HIGH(
	"[RTAPIK] OUT|[%s][%d] grap_handle[0x%08X]\n",
	__func__,
	__LINE__,
	(unsigned int)grap_handle);
	return (void *)grap_handle;
}
EXPORT_SYMBOL(screen_graphics_new);


int screen_graphics_set_blend_size(
		screen_grap_set_blend_size *grap_set_blend_size)
{
	int result;
	iccom_drv_send_cmd_param        iccom_send_cmd;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (NULL == grap_set_blend_size) {
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	MSG_MED("[RTAPIK]    |handle       [0x%08X]\n",
	(unsigned int)grap_set_blend_size->handle);

	if (NULL == grap_set_blend_size->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	iccom_send_cmd.handle      =
		((screen_grap_handle *)grap_set_blend_size->handle)->handle;
	iccom_send_cmd.task_id     = TASK_GRAPHICS;
	iccom_send_cmd.function_id = EVENT_GRAPHICS_SETBLENDSIZE;
	iccom_send_cmd.send_mode   = ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size   = sizeof(screen_grap_set_blend_size);
	iccom_send_cmd.send_data   = (unsigned char *)grap_set_blend_size;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = NULL;

	result = iccom_drv_send_command(&iccom_send_cmd);
	if (result != SMAP_LIB_GRAPHICS_OK) {
		MSG_ERROR("[RTAPIK] OUT|%s ret = %d(line = %d)\n",
		__func__, result, __LINE__);
		return result;
	}

	MSG_HIGH("[RTAPIK] OUT|[%s][%d] ret = [%d]\n",
	__func__, __LINE__, SMAP_LIB_GRAPHICS_OK);
	return SMAP_LIB_GRAPHICS_OK;
}
EXPORT_SYMBOL(screen_graphics_set_blend_size);


int screen_graphics_initialize(screen_grap_initialize *grap_initialize)
{
	int result;
	iccom_drv_send_cmd_param        iccom_send_cmd;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (NULL == grap_initialize) {
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	MSG_MED("[RTAPIK]    |handle       [0x%08X]\n", (unsigned int)grap_initialize->handle);

	if (NULL == grap_initialize->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	MSG_MED("[RTAPIK]    |handle       [0x%08X]\n", (unsigned int)grap_initialize->mode);

	iccom_send_cmd.handle      = ((screen_grap_handle *)grap_initialize->handle)->handle;
	iccom_send_cmd.task_id     = TASK_GRAPHICS;
	iccom_send_cmd.function_id = EVENT_GRAPHICS_INITIALIZE;
	iccom_send_cmd.send_mode   = ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size   = sizeof(screen_grap_initialize);
	iccom_send_cmd.send_data   = (unsigned char *)grap_initialize;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = NULL;

	result = iccom_drv_send_command(&iccom_send_cmd);
	if (result != SMAP_LIB_GRAPHICS_OK) {
		MSG_ERROR("[RTAPIK] OUT|%s ret = %d(line = %d)\n",
		__func__, result, __LINE__);
		return result;
	}

	MSG_HIGH(
	"[RTAPIK] OUT|[%s][%d] ret = [%d]\n",
	__func__,
	__LINE__,
	SMAP_LIB_GRAPHICS_OK);
	return SMAP_LIB_GRAPHICS_OK;
}
EXPORT_SYMBOL(screen_graphics_initialize);

int screen_graphics_quit(screen_grap_quit	*grap_quit)
{
	int result;
	iccom_drv_send_cmd_param       iccom_send_cmd;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (NULL == grap_quit) {
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	MSG_MED("[RTAPIK]    |handle       [0x%08X]\n", (unsigned int)grap_quit->handle);

	if (NULL == grap_quit->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	MSG_MED("[RTAPIK]    |handle       [0x%08X]\n", (unsigned int)grap_quit->mode);

	iccom_send_cmd.handle      = ((screen_grap_handle *)grap_quit->handle)->handle;
	iccom_send_cmd.task_id     = TASK_GRAPHICS;
	iccom_send_cmd.function_id = EVENT_GRAPHICS_QUIT;
	iccom_send_cmd.send_mode   = ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size   = sizeof(screen_grap_quit);
	iccom_send_cmd.send_data   = (unsigned char *)grap_quit;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = NULL;

	result = iccom_drv_send_command(&iccom_send_cmd);
	if (result != SMAP_LIB_GRAPHICS_OK) {
		MSG_ERROR("[RTAPIK] OUT|%s ret = %d(line = %d)\n",
		__func__, result, __LINE__);
		return result;
	}

	MSG_HIGH(
	"[RTAPIK] OUT|[%s][%d] ret = [%d]\n",
	__func__,
	__LINE__,
	SMAP_LIB_GRAPHICS_OK);
	return SMAP_LIB_GRAPHICS_OK;
}
EXPORT_SYMBOL(screen_graphics_quit);

int screen_graphics_image_conversion(screen_grap_image_conv *grap_image_conv)
{
	int result;
	screen_grap_image_conv      image_conv;
	iccom_drv_send_cmd_param      iccom_send_cmd;
	system_mem_ap_get_apmem_id  ap_get_apmem_id;
	system_mem_ap_change_rtaddr ap_change_rtaddr;
	iccom_drv_disable_standby_param   dis_standby;
	iccom_drv_enable_standby_param    ena_standby;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (grap_image_conv == NULL) {
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	memcpy(&image_conv, grap_image_conv, sizeof(screen_grap_image_conv));

	MSG_MED("[RTAPIK]    |handle       [0x%08X]\n", (unsigned int)grap_image_conv->handle);

	if (NULL == grap_image_conv->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	/* Input(Y) */
	if (NULL != grap_image_conv->input_image.apmem_handle) {
		ap_get_apmem_id.handle       = grap_image_conv->handle;
		ap_get_apmem_id.apmem_handle = grap_image_conv->input_image.apmem_handle;

		image_conv.input_image.apmem_handle = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
		if (image_conv.input_image.apmem_handle == 0) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%d]\n", __LINE__);
			return SMAP_LIB_GRAPHICS_PARAERR;
		}

		ap_change_rtaddr.handle         = grap_image_conv->handle;
		ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
		ap_change_rtaddr.apmem_handle   = grap_image_conv->input_image.apmem_handle;
		ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_image_conv->input_image.address);

		result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
		if (result != SMAP_LIB_MEMORY_OK) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%d]\n", __LINE__);
			return SMAP_LIB_GRAPHICS_PARAERR;
		}
		image_conv.input_image.address = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
	} else {
		image_conv.input_image.address = GET_RT_CACHE_ADDRESS(grap_image_conv->input_image.address);
	}

	/* Input(C0) */
	if (NULL != grap_image_conv->input_image.address_c0) {
		if (NULL != grap_image_conv->input_image.apmem_handle_c0) {
			ap_get_apmem_id.handle       = grap_image_conv->handle;
			ap_get_apmem_id.apmem_handle = grap_image_conv->input_image.apmem_handle_c0;
			image_conv.input_image.apmem_handle_c0 = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
			if (image_conv.input_image.apmem_handle_c0 == 0) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}

			ap_change_rtaddr.handle         = grap_image_conv->handle;
			ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
			ap_change_rtaddr.apmem_handle   = grap_image_conv->input_image.apmem_handle_c0;
			ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_image_conv->input_image.address_c0);

			result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
			if (result != SMAP_LIB_MEMORY_OK) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}
			image_conv.input_image.address_c0 = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
		} else {
			image_conv.input_image.address_c0 = GET_RT_CACHE_ADDRESS(grap_image_conv->input_image.address_c0);
		}
	}

	/* Output(Y) */
	if (NULL != grap_image_conv->output_image.apmem_handle) {
		ap_get_apmem_id.handle       = grap_image_conv->handle;
		ap_get_apmem_id.apmem_handle = grap_image_conv->output_image.apmem_handle;
		image_conv.output_image.apmem_handle = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
		if (image_conv.output_image.apmem_handle == 0) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%d]\n", __LINE__);
			return SMAP_LIB_GRAPHICS_PARAERR;
		}

		ap_change_rtaddr.handle         = grap_image_conv->handle;
		ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
		ap_change_rtaddr.apmem_handle   = grap_image_conv->output_image.apmem_handle;
		ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_image_conv->output_image.address);

		result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
		if (result != SMAP_LIB_MEMORY_OK) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%d]\n", __LINE__);
			return SMAP_LIB_GRAPHICS_PARAERR;
		}
		image_conv.output_image.address = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
	} else {
		image_conv.output_image.address = GET_RT_CACHE_ADDRESS(grap_image_conv->output_image.address);
	}


	/* Output(C0) */
	if (NULL != grap_image_conv->output_image.address_c0) {
		if (NULL != grap_image_conv->output_image.apmem_handle_c0) {
			ap_get_apmem_id.handle       = grap_image_conv->handle;
			ap_get_apmem_id.apmem_handle = grap_image_conv->output_image.apmem_handle_c0;
			image_conv.output_image.apmem_handle_c0 = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
			if (image_conv.output_image.apmem_handle_c0 == 0) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}

			ap_change_rtaddr.handle         = grap_image_conv->handle;
			ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
			ap_change_rtaddr.apmem_handle   = grap_image_conv->output_image.apmem_handle_c0;
			ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_image_conv->output_image.address_c0);

			result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
			if (result != SMAP_LIB_MEMORY_OK) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}
			image_conv.output_image.address_c0 = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
		} else {
			image_conv.output_image.address_c0 = GET_RT_CACHE_ADDRESS(grap_image_conv->output_image.address_c0);
		}
	}

	/* Output(C1) */
	if (NULL != grap_image_conv->output_image.address_c1) {
		if (NULL != grap_image_conv->output_image.apmem_handle_c1) {
			ap_get_apmem_id.handle       = grap_image_conv->handle;
			ap_get_apmem_id.apmem_handle = grap_image_conv->output_image.apmem_handle_c1;
			image_conv.output_image.apmem_handle_c1 = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
			if (image_conv.output_image.apmem_handle_c1 == 0) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}

			ap_change_rtaddr.handle         = grap_image_conv->handle;
			ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
			ap_change_rtaddr.apmem_handle   = grap_image_conv->output_image.apmem_handle_c1;
			ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_image_conv->output_image.address_c1);

			result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
			if (result != SMAP_LIB_MEMORY_OK) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}
			image_conv.output_image.address_c1 = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
		} else {
			image_conv.output_image.address_c1 = GET_RT_CACHE_ADDRESS(grap_image_conv->output_image.address_c1);
		}
	}

	iccom_send_cmd.handle      = ((screen_grap_handle *)grap_image_conv->handle)->handle;
	iccom_send_cmd.task_id     = TASK_GRAPHICS;
	iccom_send_cmd.function_id = EVENT_GRAPHICS_IMAGECONVERSION;
	iccom_send_cmd.send_mode   = ICCOM_DRV_ASYNC;
	iccom_send_cmd.send_size   = sizeof(screen_grap_image_conv);
	iccom_send_cmd.send_data   = (unsigned char *)&image_conv;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = NULL;

	dis_standby.handle = ((screen_grap_handle *)grap_image_conv->handle)->handle;
	result = iccom_drv_disable_standby(&dis_standby);
	if (result != SMAP_OK) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d] iccom_drv_disable_standby() ret = [%d]\n",
		__LINE__,
		result);
		return result;
	}

	result = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != result) {
		ena_standby.handle = ((screen_grap_handle *)grap_image_conv->handle)->handle;
		(void)iccom_drv_enable_standby(&ena_standby);

		MSG_ERROR(
		"[RTAPIK] ERR|[%d] iccom_drv_send_command() ret = [%d]\n",
		__LINE__,
		result);
		return result;
	}

	MSG_HIGH(
	"[RTAPIK] OUT|[%s][%d] ret = [%d]\n",
	__func__,
	__LINE__,
	SMAP_LIB_GRAPHICS_OK);
	return SMAP_LIB_GRAPHICS_OK;
}
EXPORT_SYMBOL(screen_graphics_image_conversion);

int screen_graphics_image_blend(screen_grap_image_blend *grap_blend)
{
	int result;
	int layer;

	struct {
		screen_grap_image_blend		t_grap_blend;
		screen_grap_layer_local		t_grap_layer[RT_GRAPHICS_BLEND_LAYER];
	} send_data;

	iccom_drv_send_cmd_param      iccom_send_cmd;
	system_mem_ap_get_apmem_id  ap_get_apmem_id;
	system_mem_ap_change_rtaddr ap_change_rtaddr;
	iccom_drv_disable_standby_param   dis_standby;
	iccom_drv_enable_standby_param    ena_standby;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (grap_blend == NULL) {
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	MSG_MED("[RTAPIK]    |handle       [0x%08X]\n", (unsigned int)grap_blend->handle);

	if (NULL == grap_blend->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	memcpy(&send_data.t_grap_blend, grap_blend, sizeof(screen_grap_image_blend));

	for (layer = 0; layer < RT_GRAPHICS_BLEND_LAYER; layer++) {
		if (NULL == grap_blend->input_layer[layer]) {
			break;
		}

		memcpy(&send_data.t_grap_layer[layer], grap_blend->input_layer[layer], sizeof(screen_grap_layer_local));

		/* Input(Y/RGB) */
		if (NULL != grap_blend->input_layer[layer]->image.apmem_handle) {
			ap_get_apmem_id.handle       = grap_blend->handle;
			ap_get_apmem_id.apmem_handle = grap_blend->input_layer[layer]->image.apmem_handle;

			send_data.t_grap_layer[layer].image.apmem_handle = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
			if (send_data.t_grap_layer[layer].image.apmem_handle == 0) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}

			ap_change_rtaddr.handle         = grap_blend->handle;
			ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
			ap_change_rtaddr.apmem_handle   = grap_blend->input_layer[layer]->image.apmem_handle;
			ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_blend->input_layer[layer]->image.address);

			result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
			if (result != SMAP_LIB_MEMORY_OK) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}
			send_data.t_grap_layer[layer].image.address = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;

			MSG_HIGH("[RTAPIK] inputlayer[%d] use ap_change_rtaddr\n", layer);
		} else {
			send_data.t_grap_layer[layer].image.address = GET_RT_CACHE_ADDRESS(grap_blend->input_layer[layer]->image.address);
		}

		/* Input(C0) */
		if (NULL != grap_blend->input_layer[layer]->image.address_c0) {
			if (NULL != grap_blend->input_layer[layer]->image.apmem_handle_c0) {
				ap_get_apmem_id.handle       = grap_blend->handle;
				ap_get_apmem_id.apmem_handle = grap_blend->input_layer[layer]->image.apmem_handle_c0;

				send_data.t_grap_layer[layer].image.apmem_handle_c0 = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
				if (send_data.t_grap_layer[layer].image.apmem_handle_c0 == 0) {
					MSG_ERROR(
					"[RTAPIK] ERR|[%d]\n", __LINE__);
					return SMAP_LIB_GRAPHICS_PARAERR;
				}

				ap_change_rtaddr.handle         = grap_blend->handle;
				ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
				ap_change_rtaddr.apmem_handle   = grap_blend->input_layer[layer]->image.apmem_handle_c0;
				ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_blend->input_layer[layer]->image.address_c0);

				result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
				if (result != SMAP_LIB_MEMORY_OK) {
					MSG_ERROR(
					"[RTAPIK] ERR|[%d]\n", __LINE__);
					return SMAP_LIB_GRAPHICS_PARAERR;
				}
				send_data.t_grap_layer[layer].image.address_c0 = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
				MSG_HIGH("[RTAPIK] inputlayer_C0[%d] use ap_change_rtaddr\n", layer);
			} else {
				send_data.t_grap_layer[layer].image.address_c0 = GET_RT_CACHE_ADDRESS(grap_blend->input_layer[layer]->image.address_c0);
			}
		}

		/* Input(C1) */
		if (NULL != grap_blend->input_layer[layer]->image.address_c1) {
			if (NULL != grap_blend->input_layer[layer]->image.apmem_handle_c1) {
				ap_get_apmem_id.handle       = grap_blend->handle;
				ap_get_apmem_id.apmem_handle = grap_blend->input_layer[layer]->image.apmem_handle_c1;

				send_data.t_grap_layer[layer].image.apmem_handle_c1 = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
				if (send_data.t_grap_layer[layer].image.apmem_handle_c1 == 0) {
					MSG_ERROR(
					"[RTAPIK] ERR|[%d]\n", __LINE__);
					return SMAP_LIB_GRAPHICS_PARAERR;
				}

				ap_change_rtaddr.handle         = grap_blend->handle;
				ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
				ap_change_rtaddr.apmem_handle   = grap_blend->input_layer[layer]->image.apmem_handle_c1;
				ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_blend->input_layer[layer]->image.address_c1);

				result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
				if (result != SMAP_LIB_MEMORY_OK) {
					MSG_ERROR(
					"[RTAPIK] ERR|[%d]\n", __LINE__);
					return SMAP_LIB_GRAPHICS_PARAERR;
				}
				send_data.t_grap_layer[layer].image.address_c1 = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
				MSG_HIGH("[RTAPIK] inputlayer_C1[%d] use ap_change_rtaddr\n", layer);
			} else {
				send_data.t_grap_layer[layer].image.address_c1 = GET_RT_CACHE_ADDRESS(grap_blend->input_layer[layer]->image.address_c1);
			}
		}
	}


	/* Output(Y) */
	if (NULL != grap_blend->output_image.apmem_handle) {
		ap_get_apmem_id.handle       = grap_blend->handle;
		ap_get_apmem_id.apmem_handle = grap_blend->output_image.apmem_handle;

		send_data.t_grap_blend.output_image.apmem_handle = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
		if (send_data.t_grap_blend.output_image.apmem_handle == 0) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%d]\n", __LINE__);
			return SMAP_LIB_GRAPHICS_PARAERR;
		}

		ap_change_rtaddr.handle         = grap_blend->handle;
		ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
		ap_change_rtaddr.apmem_handle   = grap_blend->output_image.apmem_handle;
		ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_blend->output_image.address);

		result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
		if (result != SMAP_LIB_MEMORY_OK) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%d]\n", __LINE__);
			return SMAP_LIB_GRAPHICS_PARAERR;
		}
		send_data.t_grap_blend.output_image.address = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
		MSG_HIGH("[RTAPIK] outputlayer use ap_change_rtaddr\n");
	} else {
		send_data.t_grap_blend.output_image.address = GET_RT_CACHE_ADDRESS(grap_blend->output_image.address);
	}

	/* Output(C0) */
	if (NULL != grap_blend->output_image.address_c0) {
		if (NULL != grap_blend->output_image.apmem_handle_c0) {
			ap_get_apmem_id.handle       = grap_blend->handle;
			ap_get_apmem_id.apmem_handle = grap_blend->output_image.apmem_handle_c0;

			send_data.t_grap_blend.output_image.apmem_handle_c0 = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
			if (send_data.t_grap_blend.output_image.apmem_handle_c0 == 0) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}

			ap_change_rtaddr.handle         = grap_blend->handle;
			ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
			ap_change_rtaddr.apmem_handle   = grap_blend->output_image.apmem_handle_c0;
			ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_blend->output_image.address_c0);

			result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
			if (result != SMAP_LIB_MEMORY_OK) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}
			send_data.t_grap_blend.output_image.address_c0 = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
			MSG_HIGH("[RTAPIK] outputlayer_C0 use ap_change_rtaddr\n");
		} else {
			send_data.t_grap_blend.output_image.address_c0 = GET_RT_CACHE_ADDRESS(grap_blend->output_image.address_c0);
		}
	}

	/* Output(C1) */
	if (NULL != grap_blend->output_image.address_c1) {
		if (NULL != grap_blend->output_image.apmem_handle_c1) {
			ap_get_apmem_id.handle       = grap_blend->handle;
			ap_get_apmem_id.apmem_handle = grap_blend->output_image.apmem_handle_c1;

			send_data.t_grap_blend.output_image.apmem_handle_c1 = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
			if (send_data.t_grap_blend.output_image.apmem_handle_c1 == 0) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}

			ap_change_rtaddr.handle         = grap_blend->handle;
			ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
			ap_change_rtaddr.apmem_handle   = grap_blend->output_image.apmem_handle_c1;

			ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_blend->output_image.address_c1);

			result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
			if (result != SMAP_LIB_MEMORY_OK) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}
			send_data.t_grap_blend.output_image.address_c1 = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
			MSG_HIGH("[RTAPIK] outputlayer_C1 use ap_change_rtaddr\n");
		} else {
			send_data.t_grap_blend.output_image.address_c1 = GET_RT_CACHE_ADDRESS(grap_blend->output_image.address_c1);
		}
	}

	iccom_send_cmd.handle      = ((screen_grap_handle *)grap_blend->handle)->handle;
	iccom_send_cmd.task_id     = TASK_GRAPHICS;
	iccom_send_cmd.function_id = EVENT_GRAPHICS_IMAGEBLEND;
	iccom_send_cmd.send_mode   = ICCOM_DRV_ASYNC;
	iccom_send_cmd.send_size   = sizeof(send_data);
	iccom_send_cmd.send_data   = (unsigned char *)&send_data;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = NULL;

	dis_standby.handle = ((screen_grap_handle *)grap_blend->handle)->handle;
	result = iccom_drv_disable_standby(&dis_standby);
	if (result != SMAP_OK) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d] iccom_drv_disable_standby() ret = [%d]\n",
		__LINE__,
		result);
		return result;
	}

	result = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != result) {
		ena_standby.handle = ((screen_grap_handle *)grap_blend->handle)->handle;
		(void)iccom_drv_enable_standby(&ena_standby);

		MSG_ERROR(
		"[RTAPIK] ERR|[%d] iccom_drv_send_command() ret = [%d]\n",
		__LINE__,
		result);
		return result;
	}

	MSG_HIGH(
	"[RTAPIK] OUT|[%s][%d] ret = [%d]\n",
	__func__,
	__LINE__,
	SMAP_LIB_GRAPHICS_OK);
	return SMAP_LIB_GRAPHICS_OK;
}
EXPORT_SYMBOL(screen_graphics_image_blend);

int screen_graphics_image_output(screen_grap_image_output *grap_output)
{
	int result;

	screen_grap_image_output	send_data;

	iccom_drv_send_cmd_param      iccom_send_cmd;
	system_mem_ap_get_apmem_id  ap_get_apmem_id;
	system_mem_ap_change_rtaddr ap_change_rtaddr;
	iccom_drv_disable_standby_param   dis_standby;
	iccom_drv_enable_standby_param    ena_standby;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (grap_output == NULL) {
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	MSG_MED("[RTAPIK]    |handle       [0x%08X]\n", (unsigned int)grap_output->handle);

	if (NULL == grap_output->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	memcpy(&send_data, grap_output, sizeof(screen_grap_image_output));

	/* Output(Y/RGB) */
	if (NULL != grap_output->output_image.apmem_handle) {
		ap_get_apmem_id.handle       = grap_output->handle;
		ap_get_apmem_id.apmem_handle = grap_output->output_image.apmem_handle;

		send_data.output_image.apmem_handle = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
		if (send_data.output_image.apmem_handle == 0) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%d]\n", __LINE__);
			return SMAP_LIB_GRAPHICS_PARAERR;
		}

		ap_change_rtaddr.handle         = grap_output->handle;
		ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
		ap_change_rtaddr.apmem_handle   = grap_output->output_image.apmem_handle;
		ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_output->output_image.address);

		result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
		if (SMAP_OK != result) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%d]\n", __LINE__);
			return SMAP_LIB_GRAPHICS_PARAERR;
		}
		send_data.output_image.address = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;

		MSG_HIGH("[RTAPIK] use ap_change_rtaddr address=%x\n", (unsigned int)send_data.output_image.address);
	} else {
		send_data.output_image.address = GET_RT_CACHE_ADDRESS(grap_output->output_image.address);
	}

	/* Output(C0) */
	if (NULL != grap_output->output_image.address_c0) {
		if (NULL != grap_output->output_image.apmem_handle_c0) {
			ap_get_apmem_id.handle       = grap_output->handle;
			ap_get_apmem_id.apmem_handle = grap_output->output_image.apmem_handle_c0;

			send_data.output_image.apmem_handle_c0 = (void *)system_memory_ap_get_apmem_id(&ap_get_apmem_id);
			if (send_data.output_image.apmem_handle_c0 == 0) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}

			ap_change_rtaddr.handle         = grap_output->handle;
			ap_change_rtaddr.cache_kind     = RT_MEMORY_NONCACHE;
			ap_change_rtaddr.apmem_handle   = grap_output->output_image.apmem_handle_c0;
			ap_change_rtaddr.apmem_apaddr   = (unsigned int)(grap_output->output_image.address_c0);

			result = system_memory_ap_change_rtaddr(&ap_change_rtaddr);
			if (SMAP_OK != result) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}
			send_data.output_image.address_c0 = (unsigned char *)ap_change_rtaddr.apmem_rtaddr;
			MSG_HIGH("[RTAPIK] use ap_change_rtaddr address_c0=%x\n", (unsigned int)send_data.output_image.address_c0);
		} else {
			send_data.output_image.address_c0 = GET_RT_CACHE_ADDRESS(grap_output->output_image.address_c0);
		}
	}

	iccom_send_cmd.handle      = ((screen_grap_handle *)grap_output->handle)->handle;
	iccom_send_cmd.task_id     = TASK_GRAPHICS;
	iccom_send_cmd.function_id = EVENT_GRAPHICS_IMAGEOUTPUT;
	iccom_send_cmd.send_mode   = ICCOM_DRV_ASYNC;
	iccom_send_cmd.send_size   = sizeof(send_data);
	iccom_send_cmd.send_data   = (unsigned char *)&send_data;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = NULL;

	dis_standby.handle = ((screen_grap_handle *)grap_output->handle)->handle;
	result = iccom_drv_disable_standby(&dis_standby);
	if (SMAP_OK != result) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d] iccom_drv_disable_standby() ret = [%d]\n",
		__LINE__,
		result);
		return result;
	}

	result = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != result) {
		ena_standby.handle = ((screen_grap_handle *)grap_output->handle)->handle;
		(void)iccom_drv_enable_standby(&ena_standby);

		MSG_ERROR(
		"[RTAPIK] ERR|[%d] iccom_drv_send_command() ret = [%d]\n",
		__LINE__,
		result);
		return result;
	}

	MSG_HIGH(
	"[RTAPIK] OUT|[%s][%d] ret = [%d]\n",
	__func__,
	__LINE__,
	SMAP_LIB_GRAPHICS_OK);
	return SMAP_LIB_GRAPHICS_OK;
}
EXPORT_SYMBOL(screen_graphics_image_output);

int screen_graphics_image_edit(screen_grap_image_edit *grap_edit)
{
	int result;
	screen_grap_image_edit			send_data;
	iccom_drv_send_cmd_param		iccom_send_cmd;
	iccom_drv_disable_standby_param	dis_standby;
	iccom_drv_enable_standby_param  ena_standby;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);

	if (grap_edit == NULL) {
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	MSG_MED("[RTAPIK]    |handle       [0x%08X]\n", (unsigned int)grap_edit->handle);

	if (NULL == grap_edit->handle) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d]\n", __LINE__);
		return SMAP_LIB_GRAPHICS_PARAERR;
	}

	memcpy(&send_data, grap_edit, sizeof(screen_grap_image_edit));

	/* Input(Y/RGB) */
	if (NULL != grap_edit->input_param.input_image.apmem_handle) {
		/* change appaddr to rtaddr */
		result = screen_graphics_change_rtaddr(
							grap_edit->handle,
							grap_edit->input_param.input_image.address,
							grap_edit->input_param.input_image.apmem_handle,
							&send_data.input_param.input_image.address,
							&send_data.input_param.input_image.apmem_handle);

		if (SMAP_LIB_GRAPHICS_OK != result) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%d]\n", __LINE__);
			return SMAP_LIB_GRAPHICS_PARAERR;
		}
		MSG_HIGH("[RTAPIK] input_image.address use ap_change_rtaddr\n");
	} else {
		send_data.input_param.input_image.address = GET_RT_CACHE_ADDRESS(grap_edit->input_param.input_image.address);
	}

	/* Input(C0) */
	if (NULL != grap_edit->input_param.input_image.address_c0) {
		if (NULL != grap_edit->input_param.input_image.apmem_handle_c0) {
			/* change appaddr to rtaddr */
			result = screen_graphics_change_rtaddr(
								grap_edit->handle,
								grap_edit->input_param.input_image.address_c0,
								grap_edit->input_param.input_image.apmem_handle_c0,
								&send_data.input_param.input_image.address_c0,
								&send_data.input_param.input_image.apmem_handle_c0);

			if (SMAP_LIB_GRAPHICS_OK != result) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}
			MSG_HIGH("[RTAPIK] input_image.address_c0 use ap_change_rtaddr\n");
		} else {
			send_data.input_param.input_image.address_c0 = GET_RT_CACHE_ADDRESS(grap_edit->input_param.input_image.address_c0);
		}
	}
	/* Input(C1) Don't care */

	/* Output(Y) */
	if (NULL != grap_edit->output_image.apmem_handle) {
		/* change appaddr to rtaddr */
		result = screen_graphics_change_rtaddr(
							grap_edit->handle,
							grap_edit->output_image.address,
							grap_edit->output_image.apmem_handle,
							&send_data.output_image.address,
							&send_data.output_image.apmem_handle);

		if (SMAP_LIB_GRAPHICS_OK != result) {
			MSG_ERROR(
			"[RTAPIK] ERR|[%d]\n", __LINE__);
			return SMAP_LIB_GRAPHICS_PARAERR;
		}
		MSG_HIGH("[RTAPIK] output_image.address use ap_change_rtaddr\n");
	} else {
		send_data.output_image.address = GET_RT_CACHE_ADDRESS(grap_edit->output_image.address);
	}

	/* Output(C0) */
	if (NULL != grap_edit->output_image.address_c0) {
		if (NULL != grap_edit->output_image.apmem_handle_c0) {
			/* change appaddr to rtaddr */
			result = screen_graphics_change_rtaddr(
								grap_edit->handle,
								grap_edit->output_image.address_c0,
								grap_edit->output_image.apmem_handle_c0,
								&send_data.output_image.address_c0,
								&send_data.output_image.apmem_handle_c0);

			if (SMAP_LIB_GRAPHICS_OK != result) {
				MSG_ERROR(
				"[RTAPIK] ERR|[%d]\n", __LINE__);
				return SMAP_LIB_GRAPHICS_PARAERR;
			}
			MSG_HIGH("[RTAPIK] output_image.address_c0 use ap_change_rtaddr\n");
		} else {
			send_data.output_image.address_c0 = GET_RT_CACHE_ADDRESS(grap_edit->output_image.address_c0);
		}
	}
	/* Output(C1) Don't care */

	/* send_command param setting */
	iccom_send_cmd.handle      = ((screen_grap_handle *)grap_edit->handle)->handle;
	iccom_send_cmd.task_id     = TASK_GRAPHICS;
	iccom_send_cmd.function_id = EVENT_GRAPHICS_IMAGEEDIT;
	iccom_send_cmd.send_mode   = ICCOM_DRV_ASYNC;
	iccom_send_cmd.send_size   = sizeof(send_data);
	iccom_send_cmd.send_data   = (unsigned char *)&send_data;
	iccom_send_cmd.recv_size   = 0;
	iccom_send_cmd.recv_data   = NULL;

	dis_standby.handle = ((screen_grap_handle *)grap_edit->handle)->handle;
	result = iccom_drv_disable_standby(&dis_standby);
	if (result != SMAP_OK) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d] iccom_drv_disable_standby() ret = [%d]\n",
		__LINE__,
		result);
		return result;
	}

	result = iccom_drv_send_command(&iccom_send_cmd);
	if (SMAP_OK != result) {
		ena_standby.handle = ((screen_grap_handle *)grap_edit->handle)->handle;
		(void)iccom_drv_enable_standby(&ena_standby);

		MSG_ERROR(
		"[RTAPIK] ERR|[%d] iccom_drv_send_command() ret = [%d]\n",
		__LINE__,
		result);
		return result;
	}

	MSG_HIGH(
	"[RTAPIK] OUT|[%s][%d] ret = [%d]\n",
	__func__,
	__LINE__,
	SMAP_LIB_GRAPHICS_OK);
	return SMAP_LIB_GRAPHICS_OK;
}
EXPORT_SYMBOL(screen_graphics_image_edit);

void screen_graphics_delete(screen_grap_delete *grap_delete)
{
	iccom_drv_cleanup_param  iccom_cleanup;
	rtds_memory_drv_cleanup_param mem_cleanup;
	screen_grap_handle	*grap_handle;

	MSG_HIGH("[RTAPIK] IN |[%s]\n", __func__);
	if (NULL == grap_delete) {
		return;
	}
	MSG_MED("[RTAPIK]    |handle[0x%08X]\n", (unsigned int)grap_delete->handle);

	if (grap_delete->handle ==  NULL) {
		MSG_ERROR(
		"[RTAPIK] ERR|[%d] handle NULL error\n",
		__LINE__);
	} else {
		grap_handle = grap_delete->handle;
		mem_cleanup.handle = grap_handle->rtds_mem_handle;
		rtds_memory_drv_cleanup(&mem_cleanup);
		iccom_cleanup.handle = grap_handle->handle;
		iccom_drv_cleanup(&iccom_cleanup);
		kfree(grap_handle);
	}

	MSG_HIGH("[RTAPIK] OUT|[%s]\n", __func__);
}
EXPORT_SYMBOL(screen_graphics_delete);
