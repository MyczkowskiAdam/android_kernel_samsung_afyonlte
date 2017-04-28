/*
 * sh_fsi_wireless_transciever.c --  ASoC sh-fsi Bluetooth and
 *                                                   Fm dummy codec driver
 *
 * Copyright (C) 2013 Renesas Mobile Corp.
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

#include <linux/module.h>
#include <sound/soc.h>

static struct snd_soc_dai_driver sh_fsi_wireless_transciever_dai[] = {
	{
		.name = "sh_fsi_wireless_transciever",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = NULL,
	},
};

static int sh_fsi_wireless_transciever_probe(struct snd_soc_codec *codec)
{
	/* Nothing to do*/
	return 0;
}

static int sh_fsi_wireless_transciever_remove(struct snd_soc_codec *codec)
{
	/* Nothing to do*/
	return 0;
}

struct snd_soc_codec_driver soc_codec_dev_sh_fsi_wireless_transciever = {
	.probe = sh_fsi_wireless_transciever_probe,
	.remove = sh_fsi_wireless_transciever_remove,
	.reg_cache_size = 0,
	.reg_word_size = sizeof(u8),
	.reg_cache_default = NULL,
};

static int sh_fsi_wireless_transciever_driver_probe(
						struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev,
			&soc_codec_dev_sh_fsi_wireless_transciever,
			&sh_fsi_wireless_transciever_dai[0],
			ARRAY_SIZE(sh_fsi_wireless_transciever_dai));
}

static int sh_fsi_wireless_transciever_driver_remove(
						struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver sh_fsi_wireless_transciever_driver = {
	.driver = {
		.name = "sh_fsi_wireless_transciever",
		.owner = THIS_MODULE,
	},
	.probe = sh_fsi_wireless_transciever_driver_probe,
	.remove = sh_fsi_wireless_transciever_driver_remove,
};

static int __init sh_fsi_wireless_transciever_init(void)
{
	return platform_driver_register(&sh_fsi_wireless_transciever_driver);
}
module_init(sh_fsi_wireless_transciever_init);

static void __exit sh_fsi_wireless_transciever_exit(void)
{
	platform_driver_unregister(&sh_fsi_wireless_transciever_driver);
}
module_exit(sh_fsi_wireless_transciever_exit);

MODULE_DESCRIPTION("ASoC sh-fsi Bluetooth and Fm dummy codec driver");
MODULE_AUTHOR("RMC");
MODULE_LICENSE("GPL");
