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

#ifndef _SH_MOBILE_COMPOSER_MODULE_H
#define _SH_MOBILE_COMPOSER_MODULE_H

#include <linux/sh_mobile_composer.h>

struct sh_mobile_composer_entry_t {
	/* convert physical to rt-address */
	unsigned char *(*composer_phy_change_rtaddr)(
		unsigned long p_adr);
	/* register gpu buffer to composer. unregister not supported. */
	int (*composer_register_gpu_buffer)(
		unsigned long address, unsigned long size);
	/* queue fro gpu */
	int (*composer_queue)(
		void *data,
		int   data_size,
		void  (*callback)(void *user_data, int result),
		void   *user_data);
	int (*composer_hdmiset)(int mode);
};

extern struct sh_mobile_composer_entry_t sh_mobile_composer_entry;

#endif
