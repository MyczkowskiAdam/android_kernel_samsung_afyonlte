/*
 * system_rtload.c
 *     RT domain boot function file.
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
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/module.h>
#include "iccom_drv.h"
#include "iccom_drv_id.h"
#include "system_rtload.h"
#include "log_kernel.h"
#include "system_rtload_private.h"
#include "rt_boot_drv.h"


/************************************/
/*			API						*/
/************************************/
int system_rt_load_level2task(void)
{
	int ret_code;
	int load_level = SYSTEM_RTLOAD_LEVEL2;

	iccom_drv_send_cmd_param		iccom_send_cmd;
	iccom_drv_init_param			iccom_init;
	iccom_drv_cleanup_param			iccom_cleanup;
	void							*rtload_handle;

	MSG_MED("[RTAPIK]IN |[%s]\n", __func__);

	ret_code = SMAP_LIB_LOAD_NG;
	memset(&iccom_init, 0, sizeof(iccom_init));
	memset(&iccom_cleanup, 0, sizeof(iccom_cleanup));
	memset(&iccom_send_cmd, 0, sizeof(iccom_send_cmd));

	rtload_handle = iccom_drv_init(&iccom_init);
	if (NULL == rtload_handle) {
		MSG_ERROR("[RTAPIK]ERR|[%s] iccom_drv_init Error = %d\n",
					__func__ , ret_code);
		return SMAP_LIB_LOAD_NG;
	}

	iccom_send_cmd.handle		= rtload_handle;
	iccom_send_cmd.task_id		= TASK_STATUS_CONTROL;
	iccom_send_cmd.function_id	= EVENT_STATUS_INITTSKLVL;
	iccom_send_cmd.send_mode	= (short)ICCOM_DRV_SYNC;
	iccom_send_cmd.send_size	= sizeof(load_level);
	iccom_send_cmd.send_data	= (void *)&load_level;
	iccom_send_cmd.recv_size	= 0;
	iccom_send_cmd.recv_data	= NULL;

	ret_code = system_sub_load_rtimage();
	if (SMAP_OK == ret_code) {
		MSG_LOW("[RTAPIK]   |[%s] iccom_drv_send_command START\n", __func__);
		ret_code = iccom_drv_send_command(&iccom_send_cmd);
	}

	MSG_HIGH("[RTAPIK]   |[%s] err_code = %d\n", __func__, ret_code);
	switch (ret_code) {
	case SMAP_OK:
		ret_code = SMAP_LIB_LOAD_OK;
		break;

	case SMAP_NG:
		ret_code = SMAP_LIB_LOAD_NG;
		break;

	default:
		break;
	}

	iccom_cleanup.handle = rtload_handle;
	iccom_drv_cleanup(&iccom_cleanup);

	MSG_MED("[RTAPIK]OUT|[%s] : Return[%d]\n", __func__, ret_code);

	return ret_code;
}
EXPORT_SYMBOL(system_rt_load_level2task);


/************************************/
/*			internal function		*/
/************************************/
int sys_get_section_header(get_section_header_param *param)
{
	int ret_code;

	MSG_MED("[RTAPIK]IN |[%s] :\n", __func__);

	/* param check */
	if (NULL == param) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : param error :%x\n",
														__func__,
														(unsigned int)param);
		return SMAP_PARA_NG;
	}
	if (NULL == param->section_header) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : param error section_header:%x\n",
					__func__, (unsigned int)param->section_header);
		return SMAP_PARA_NG;
	}


	/* load RT Section header */
	ret_code = system_sub_get_section_header(param->section_header);

	if (SMAP_OK != ret_code) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : system_sub_get_section_header() Error\n",
					__func__);
	}

	MSG_MED("[RTAPIK]OUT|[%s] :\n", __func__);

	return ret_code;
}
EXPORT_SYMBOL(sys_get_section_header);


/************************************/
/*			sub function			*/
/************************************/
int system_sub_load_rtimage(void)
{
	struct file *fp = NULL;
	void __iomem *addr = NULL;
	void __iomem *load_addr = NULL;
	int data_size;
	system_rt_section_header sc_header;

	int ret_code = SMAP_NG;

	MSG_MED("[RTAPIK]IN |[%s] :\n", __func__);
	MSG_LOW("[RTAPIK]   |filename = %s", RT_FILENAME);

	fp = filp_open(RT_FILENAME, O_RDONLY, 0);
	if ((NULL == fp) || (IS_ERR(fp))) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : filp_open(%s,RD,0)error\n",
					__func__, RT_FILENAME);
		return SMAP_LIB_LOAD_NG;
	}

	/* Read RT Section header from RT Image in boot address */
	ret_code = system_sub_get_section_header(&sc_header);
	if (SMAP_OK != ret_code) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : system_sub_get_section_header() Error\n", __func__);
		filp_close(fp, NULL);
		return ret_code;
	}

	/* Read RT Section header from RT Image in boot address */
	addr = ioremap(sc_header.boot_address+RT_BOOT_SIZE, sizeof(sc_header));
	if (NULL == addr) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : ioremap ERROR section_info\n", __func__);
		filp_close(fp, NULL);
		return SMAP_LIB_LOAD_NG;
	}

	if (0x000000FF == sc_header.load_flag) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : Lvl2 Task is already copied",
					__func__);
		iounmap(addr);
		filp_close(fp, NULL);
		return SMAP_LIB_LOAD_SEQ_NG;
	}

	if (0 == sc_header.img[RT_LEVEL_2].size) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : Lvl2 Task is empty",
					__func__);
		iounmap(addr);
		filp_close(fp, NULL);
		return SMAP_LIB_LOAD_NG;
	}

	load_addr = ioremap(sc_header.boot_address+sc_header.img[RT_LEVEL_2].start,
						sc_header.img[RT_LEVEL_2].size);
	if (NULL == load_addr) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : ioremap(Lvl2LoadAdr) error\n", __func__);
		iounmap(addr);
		filp_close(fp, NULL);
		return SMAP_LIB_LOAD_NG;
	}
	/* load RT firm level2 */
	data_size = kernel_read(fp, sc_header.img[RT_LEVEL_2].start, load_addr,
							sc_header.img[RT_LEVEL_2].size);
	if (data_size != sc_header.img[RT_LEVEL_2].size) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : data_size[0x%x] error\n", __func__,
					(unsigned int)data_size);
		iounmap(addr);
		iounmap(load_addr);
		filp_close(fp, NULL);
		return SMAP_LIB_LOAD_NG;
	}

	filp_close(fp, NULL);
	iounmap(load_addr);

	sc_header.load_flag = 0x000000FF;
	memcpy_toio(addr, &sc_header, sizeof(sc_header));
	iounmap(addr);

	MSG_MED("[RTAPIK]OUT|[%s] :\n", __func__);

	return SMAP_OK;
}

int system_sub_get_section_header(system_rt_section_header *section_header)
{
	struct	rt_boot_info rt_info;
	int		ret;

	MSG_MED("[RTAPIK]IN |[%s] :\n", __func__);

	if (NULL == section_header) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : param error section_header=0x%08x\n",
					__func__, (unsigned int)section_header);
		return SMAP_NG;
	}

	memset(&rt_info, 0, sizeof(rt_info));

	ret = rtboot_get_section_header(&rt_info);
	if (ret != 0) {
		MSG_ERROR("[RTAPIK]ERR|[%s] : rtboot_get_section_header ERROR\n", __func__);
		return SMAP_NG;
	}

	MSG_LOW("[RTAPIK]   |Read RT Section header from RT boot address\n");
	memcpy(section_header, (void *)&rt_info, sizeof(system_rt_section_header));

	MSG_LOW("[RTAPIK]   |version               = %c%c%c%c%c%c%c%c%c%c%c%c\n",
															section_header->version[0],
															section_header->version[1],
															section_header->version[2],
															section_header->version[3],
															section_header->version[4],
															section_header->version[5],
															section_header->version[6],
															section_header->version[7],
															section_header->version[8],
															section_header->version[9],
															section_header->version[10],
															section_header->version[11]);
	MSG_LOW("[RTAPIK]   |boot_address          = 0x%08x\n",	(unsigned int)section_header->boot_address);
	MSG_LOW("[RTAPIK]   |image_size            = %08d\n",	(int)section_header->image_size);
	MSG_LOW("[RTAPIK]   |memmpl_address        = 0x%08x\n",	(int)section_header->memmpl_address);
	MSG_LOW("[RTAPIK]   |memmpl_size           = %08d\n",	(int)section_header->memmpl_size);
	MSG_LOW("[RTAPIK]   |command_area_address  = 0x%08x\n",	(int)section_header->command_area_address);
	MSG_LOW("[RTAPIK]   |command_area_size     = %08d\n",	(int)section_header->command_area_size);
	MSG_LOW("[RTAPIK]   |load_flg              = %08d\n",	(int)section_header->load_flag);
	MSG_LOW("[RTAPIK]   |img[RT_LEVEL_1].start = 0x%08x\n",	(unsigned int)section_header->img[RT_LEVEL_1].start);
	MSG_LOW("[RTAPIK]   |img[RT_LEVEL_1].size  = %08d\n",	(int)section_header->img[RT_LEVEL_1].size);
	MSG_LOW("[RTAPIK]   |img[RT_LEVEL_2].start = 0x%08x\n",	(unsigned int)section_header->img[RT_LEVEL_2].start);
	MSG_LOW("[RTAPIK]   |img[RT_LEVEL_2].size  = %08d\n",	(int)section_header->img[RT_LEVEL_2].size);
	MSG_LOW("[RTAPIK]   |sh_pmb_offset         = 0x%08x\n",	(int)section_header->sh_pmb_offset);
	MSG_LOW("[RTAPIK]   |sh_pmb_nc_offset      = 0x%08x\n",	(int)section_header->sh_pmb_nc_offset);
	MSG_LOW("[RTAPIK]   |mfi_pmb_offset        = 0x%08x\n",	(int)section_header->mfi_pmb_offset);

	MSG_MED("[RTAPIK]OUT|[%s] :\n", __func__);
	return SMAP_OK;
}
