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
#ifndef _SH_MOBILE_COMPOSER_HDMICOMPOSE_H
#define _SH_MOBILE_COMPOSER_HDMICOMPOSE_H

#include <linux/sh_mobile_composer.h>
#include <media/sh_mobile_appmem.h>

#include "../composer/sh_mobile_work.h"

/******************************/
/* define structure           */
/******************************/
struct cmp_information_hdmi {
	unsigned long          allocatesize;
	struct appmem_handle   *hdmi_map_handle;
	int                    hdmi_count_display;
	int                    max_width;
	int                    max_height;

	int                    direct_display[2];
	int                    blend_bufferid;
};

/******************************/
/* define external function   */
/******************************/

#if SH_MOBILE_COMPOSER_SUPPORT_HDMI
/* interface of execute RT-API.  */
static void *hdmi_rtapi_create(void) __maybe_unused;
static int hdmi_rtapi_delete(void *handle) __maybe_unused;
static int hdmi_rtapi_output(void *handle, struct composer_rh *rh) \
	__maybe_unused;
static int hdmi_rtapi_blend(void *handle, struct composer_rh *rh) \
	__maybe_unused;

/* work memory */
static int hdmi_memory_allocate(struct cmp_information_hdmi *info,
	unsigned long size);
static int hdmi_memory_free(struct cmp_information_hdmi *info);

/* configure blending parameters (input layer) */
static int hdmi_config(void *handle,
	struct composer_rh *rh, struct cmp_postdata *data);

/* configure blending parameters (output) */
static int hdmi_config_output(struct composer_rh *rh,
	struct cmp_information_hdmi *info);
#endif

#endif
