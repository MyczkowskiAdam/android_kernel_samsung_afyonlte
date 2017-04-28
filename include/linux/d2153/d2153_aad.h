/*
 * d2153_aad.h  --  D2153 ASoC AAD driver
 *
 * Copyright (c) 2012 Dialog Semiconductor
 *
 * Author: Adam Thomson <Adam.Thomson@diasemi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __D2153_AAD_H
#define __D2153_AAD_H

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/switch.h>
#include <sound/soc.h>
#include <sound/jack.h>

/* PMIC related includes */
#include <linux/d2153/core.h>

/*
 * D2153 AAD register space
 */

/* Accessory Detect Registers */
#define D2153_ACCDET_CONFIG			0x6C
#define D2153_ACCDET_STATUS			0x6D
#define D2153_ACCDET_CFG1			0x6E
#define D2153_ACCDET_CFG2			0x6F
#define D2153_ACCDET_CFG3			0x70
#define D2153_ACCDET_CFG4			0x71
#define D2153_ACCDET_CFG5			0x72
#define D2153_ACCDET_TST1			0x73
#define D2153_ACCDET_TST2			0x74
#define D2153_ACCDET_TST3			0x75
#define D2153_ACCDET_THRESH1			0x76
#define D2153_ACCDET_THRESH2			0x77
#define D2153_ACCDET_THRESH3			0x78
#define D2153_ACCDET_THRESH4			0x79
#define D2153_ACCDET_UNLOCK_AO			0x7a


/*
 * Bit fields
 */

/* D2153_ACCDET_CONFIG = 0x6C */
#define D2153_ACCDET_MODE_AUTO				(0 << 0)
#define D2153_ACCDET_JACK_EN				(1 << 3)
#define D2153_ACCDET_BTN_EN				(1 << 7)

/* D2153_ACCDET_CFG1 = 0x6E */
#define D2153_ACCDET_NO_JACK_RATE_8MS			(5 << 4)
#define D2153_ACCDET_NO_JACK_RATE_MASK			(0xF << 4)
#define D2153_ACCDET_NO_JACK_RATE_130MS			(9 << 4)
#define D2153_ACCDET_JACK_DET_DEBOUNCE_4SAMPLE		(3 << 2)
#define D2153_ACCDET_JACK_DET_DEBOUNCE_MASK		(0x3 << 2)

/* D2153_ACCDET_CFG2 = 0x6F */
#define D2153_ACCDET_THREE_POLE_JACK_RATE_8MS		(5 << 0)
#define D2153_ACCDET_THREE_POLE_JACK_RATE_MASK		(0xF << 0)
#define D2153_ACCDET_THREE_POLE_JACK_RATE_130MS		(9 << 0)

#define D2153_ACCDET_FOUR_POLE_JACK_RATE_8MS		(5 << 4)
#define D2153_ACCDET_FOUR_POLE_JACK_RATE_MASK		(0xF << 4)
#define D2153_ACCDET_FOUR_POLE_JACK_RATE_130MS		(9 << 4)


/* D2153_ACCDET_CFG3 = 0x70 */
#define D2153_ACCDET_JACK_MODE_JACK			(1 << 6)
#define D2153_ACCDET_JACK_MODE_MIC			(1 << 7)

/* D2153_ACCDET_CFG3 = 0x71 */
#define D2153_ACCDET_ADC_COMP_OUT_INV			(1 << 4)

/* D2153_ACCDET_CFG5 = 0x72 */
#define ACCDET_ISOURCE_MIC_FRC_EN			(1 << 3)

/* Virtual IRQs */
#define D2153_PMIC_IRQ_EACCDET		LEOPARD_IRQ_EACCDET
#define D2153_PMIC_IRQ_EJACKDET		LEOPARD_IRQ_EACCDET

/* Jack & button report types */
#define D2153_JACK_REPORT_TYPE		SND_JACK_HEADSET
#define D2153_BUTTON_REPORT_TYPE	(SND_JACK_BTN_0 | SND_JACK_BTN_1 |\
					 SND_JACK_BTN_2 | SND_JACK_BTN_3)

#define D2153_GPIO_DEBOUNCE_TIME_LONG (4000)  /*4ms*/
#define D2153_GPIO_DEBOUNCE_TIME_SHORT (1000)  /*1ms*/

#define D2153_AAD_BUTTON_DEBOUNCE_MS 10
#define D2153_AAD_BUTTON_SLEEP_DEBOUNCE_MS 10
#define D2153_AAD_JACKOUT_DEBOUNCE_MS 100
#define D2153_AAD_JACK_DEBOUNCE_MS 400

#define D2153_AAD_MICBIAS_SETUP_TIME
#ifdef D2153_AAD_MICBIAS_SETUP_TIME
#define D2153_AAD_MICBIAS_SETUP_TIME_MS 50
#else
#define D2153_AAD_MICBIAS_SETUP_TIME_MS 0
#endif

/* Headset buttons */
#define SEND_BUTTON		0
#define VOL_UP_BUTTON		1
#define VOL_DN_BUTTON		2
#define MAX_BUTTONS		3

#define D2153_BUTTON_RELEASE	0
#define D2153_BUTTON_PRESS	1
#define D2153_BUTTON_START	2
#define D2153_BUTTON_IGNORE	3

#define D2153_NO_JACK		0x0
#define D2153_HEADSET		0x1
#define D2153_HEADPHONE		0x2

/* Structure to encapsulate button press ADC value ranges */
struct button_resistance {
	u8 min_val;
	u8 max_val;
};

struct d2153_jack_info {
	struct snd_soc_jack *jack;
	int report_mask;	
	int status;
	int key;
};

struct d2153_switch_data {
	struct switch_dev sdev; /**< switch dev. */
	int state;              /**< switch state. */
	int key_press;          /**< hook button pressed/released. */
};

/* AAD private data */
struct d2153_aad_priv {
	struct i2c_client *i2c_client;
	struct d2153_codec_priv *d2153_codec;
	struct d2153_jack_info jack;
	struct d2153_jack_info button;
	unsigned int button_detect_rate;
	struct wake_lock wakeup;
	struct class *audio_class;
	struct device *headset_dev;
	unsigned int button_state;
	struct input_dev *input_dev;        
	struct d2153_switch_data switch_data;
	struct delayed_work	jack_monitor_work;
	struct delayed_work	button_monitor_work;
	unsigned int g_det_irq;
	u8 chip_rev;
	bool l_det_status;
	int jack_debounce_ms;
	int jackout_debounce_ms;
	int button_debounce_ms;
	int button_sleep_debounce_ms;
	bool codec_detect_enable;	/* d2153 detect configuration */
	bool gpio_detect_enable;	/* gpio detect configuration */
	int gpio_port;			/* gpio port for jack detect */
};

/* HACK to deal with issue of ADC 1/8 bit reads for button detect */
int d2153_aad_update_bits(struct i2c_client *client, u8 reg, u8 mask,
			  u8 value);

int d2153_aad_write(struct i2c_client *client, u8 reg, u8 value);

/* Jack/Button detection control methods */
int d2153_jack_detect(struct snd_soc_jack *jack, bool enable);
int d2153_button_detect(struct snd_soc_jack *jack, bool enable);
int d2153_aad_read_ex(u8 reg);
int d2153_aad_write_ex(u8 reg, u8 value);
int d2153_pmic_read_ex(u8 reg, u8 regval);

#endif /* __D2153_AAD_H */

