/*
 * fsi_d2153.h - FSI ASoC driver for boards using wm1811 codec.
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __FSI_D2153_H__
#define __FSI_D2153_H__

#include "soc.h"
#include "control.h"

#define FSI_SOC_SINGLE_EXT(xname, xdata, xhandler_info,\
	xhandler_get, xhandler_put) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,\
	.name = xname, \
	.info = xhandler_info, \
	.get = xhandler_get, \
	.put = xhandler_put, \
	.private_value = xdata \
}

#define FSI_SOC_SINGLE(xname, reg, shift, max, invert, xget, xput)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,				\
	.name = xname,							\
	.info = snd_soc_info_volsw,					\
	.get = xget,							\
	.put = xput,							\
	.private_value = SOC_SINGLE_VALUE(reg, shift, max, invert)	\
}

/* Loopback state */
enum fsi_d2153_loopback_state {
	FSI_D2153_LOOPBACK_START,	/* Loopback start */
	FSI_D2153_LOOPBACK_STOP,	/* Loopback stop */
	FSI_D2153_LOOPBACK_MAX
};

extern void fsi_d2153_set_dac_power(struct snd_kcontrol *kcontrol,
	int status);
extern void fsi_d2153_set_adc_power(struct snd_kcontrol *kcontrol,
	int status);
extern int fsi_d2153_enable_ignore_suspend(struct snd_soc_card *card,
	unsigned int dev_id);
extern int fsi_d2153_disable_ignore_suspend(struct snd_soc_card *card,
	unsigned int dev_id);

extern int fsi_d2153_set_sampling_rate(struct snd_pcm_hw_params *params);

extern int fsi_d2153_loopback_notify(int status);

/* temp process */
extern void fsi_d2153_soc_write(int dev);
/* temp process */

#endif
