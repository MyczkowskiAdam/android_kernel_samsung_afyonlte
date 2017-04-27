/*
 * Function        : Composer driver for SH Mobile
 *
 * Copyright (C) 2013-2013 Renesas Electronics Corporation
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
 * Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */

#include <linux/kernel.h>
#include <linux/sh_mobile_composer.h>
#include <linux/module.h>

#include "../composer/sh_mobile_module.h"

/******************************************************/
/* define global variables                            */
/******************************************************/
struct sh_mobile_composer_entry_t sh_mobile_composer_entry = {
	.composer_phy_change_rtaddr = NULL,
	.composer_register_gpu_buffer = NULL,
	.composer_queue = NULL,
	.composer_hdmiset = NULL,
};
EXPORT_SYMBOL(sh_mobile_composer_entry);


/******************************************************/
/* global functions                                    */
/******************************************************/
unsigned char *sh_mobile_composer_phy_change_rtaddr(
	unsigned long p_adr)
{
	unsigned char *ret = NULL;
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
	if (!sh_mobile_composer_entry.composer_phy_change_rtaddr)
		goto err;
	ret = sh_mobile_composer_entry.composer_phy_change_rtaddr(p_adr);
err:
#endif
	return ret;
}
EXPORT_SYMBOL(sh_mobile_composer_phy_change_rtaddr);

int sh_mobile_composer_register_gpu_buffer(
	unsigned long address, unsigned long size)
{
	int ret = -1;
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
	if (!sh_mobile_composer_entry.composer_register_gpu_buffer)
		goto err;
	ret = sh_mobile_composer_entry.composer_register_gpu_buffer(
		address, size);
err:
#endif
	return ret;
}
EXPORT_SYMBOL(sh_mobile_composer_register_gpu_buffer);

int sh_mobile_composer_queue(
	void *data,
	int   data_size,
	void  (*callback)(void *user_data, int result),
	void   *user_data)
{
	int ret = -1;
#ifdef CONFIG_MISC_R_MOBILE_COMPOSER_REQUEST_QUEUE
	if (!sh_mobile_composer_entry.composer_queue)
		goto err;
	ret = sh_mobile_composer_entry.composer_queue(
		data, data_size, callback, user_data);
err:
#endif
	return ret;
}
EXPORT_SYMBOL(sh_mobile_composer_queue);

int sh_mobile_composer_hdmiset(int mode)
{
	int ret = -1;
#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
	if (!sh_mobile_composer_entry.composer_hdmiset)
		goto err;
	ret = sh_mobile_composer_entry.composer_hdmiset(mode);
err:
#endif
	return ret;
}
EXPORT_SYMBOL(sh_mobile_composer_hdmiset);
