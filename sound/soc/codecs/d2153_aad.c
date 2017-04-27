/*
 * D2153 ALSA SoC Audio Accessory Detection driver
 *
 * Copyright (c) 2012 Dialog Semiconductor
 *
 * Written by Adam Thomson <Adam.Thomson.Opensource@diasemi.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <mach/common.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/jack.h>
#include <sound/soundpath/soundpath.h>
#include <linux/gpio.h>
#include <mach/r8a7373.h>

//#define D2153_READ_PMIC_BUTTON_REG

/* PMIC related includes */
#include <linux/d2153/core.h>
#include <linux/d2153/d2153_reg.h>

#include <linux/d2153/d2153_codec.h>
#include <linux/d2153/d2153_aad.h>

#define D2153_AAD_DETECT_JACK_ADC 91
#define D2153_AAD_CONNER_CASE_ADC 239
#define D2153_GPIO_DEBOUNCE_TIME 4000 /* 4ms*/


struct d2153_aad_priv *d2153_aad_ex;
EXPORT_SYMBOL(d2153_aad_ex);

static int d2153_aad_read(struct i2c_client *client, u8 reg);
struct i2c_client *add_client = NULL;


/*
 * Samsung defined resistances for 4-pole key presses:
 *
 * 0 - 108 Ohms:	Send Button
 * 139 - 270 Ohms:	Vol+ Button
 * 330 - 680 Ohms:	Vol- Button
 */

/* Button resistance lookup table */
/* DLG - ADC values need to be correctly set as they're currently not known */
static const struct button_resistance button_res_2V6_tbl[MAX_BUTTONS] = {
	[SEND_BUTTON] = {
		.min_val = 0,
		.max_val = 15,
	},
	[VOL_UP_BUTTON] = {
		.min_val = 16,
		.max_val = 31,
	},
	[VOL_DN_BUTTON] = {
		.min_val = 32,
		.max_val = 86,
	},
};

static const struct button_resistance button_res_2V5_tbl[MAX_BUTTONS] = {
	[SEND_BUTTON] = {
		.min_val = 0,
		.max_val = 14,
	},
	[VOL_UP_BUTTON] = {
		.min_val = 15,
		.max_val = 31,
	},
	[VOL_DN_BUTTON] = {
		.min_val = 32,
		.max_val = 95,
	},
};

static const struct button_resistance *button_res_tbl;
static ssize_t show_headset(struct device *dev, struct device_attribute *attr, char *buf);

#define HEADSET_ATTR(_name)													\
{																				\
	.attr = { .name = #_name, .mode = 0644, },					\
	.show = show_headset,														\
}

enum {
	STATE = 0,
	KEY_STATE,
	ADC,
};

static struct device_attribute headset_Attrs[] = {
	HEADSET_ATTR(state),
	HEADSET_ATTR(key_state),
	HEADSET_ATTR(adc),
};

static ssize_t show_headset(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	const ptrdiff_t off = attr - headset_Attrs;
	u8 adc_det_status = 0;
	struct i2c_client *client = add_client;

	switch (off)
	{
	case STATE:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n",
			(((d2153_aad_ex->switch_data.state == D2153_HEADPHONE)
			|| (d2153_aad_ex->switch_data.state == D2153_HEADSET))
				? 1 : 0));
		printk("[%s]:kwanjin : state : %d\n", __func__,
			d2153_aad_ex->switch_data.state);
		break;
	case KEY_STATE:
		printk("[%s]:kwanjin : button_status : %d\n",__func__,
			d2153_aad_ex->button.status);
		ret = scnprintf(buf, PAGE_SIZE, "%d\n",
			d2153_aad_ex->button.status);
		break;
	case ADC:
		adc_det_status = d2153_aad_read(client, D2153_ACCDET_STATUS);
		printk("[%s]:kwanjin : earjack adc : %d\n", __func__,
			adc_det_status);
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", adc_det_status);
		break;
	default :
		break;
	}

	return ret;
}


/* Following register access methods based on soc-cache code */
static int d2153_aad_read(struct i2c_client *client, u8 reg)
{
	struct d2153_aad_priv *d2153_aad = i2c_get_clientdata(client);

	struct i2c_msg xfer[2];
	u8 data;
	int ret;

	mutex_lock(&d2153_aad->d2153_codec->d2153_pmic->d2153_io_mutex);

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = &reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 1;
	xfer[1].buf = &data;

	ret = i2c_transfer(client->adapter, xfer, 2);

	mutex_unlock(&d2153_aad->d2153_codec->d2153_pmic->d2153_io_mutex);

	if (ret == 2)
		return data;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

int d2153_aad_write(struct i2c_client *client, u8 reg, u8 value)
{
	struct d2153_aad_priv *d2153_aad = i2c_get_clientdata(client);
	u8 data[2];
	int ret;

	mutex_lock(&d2153_aad->d2153_codec->d2153_pmic->d2153_io_mutex);

    reg &= 0xff;
    data[0] = reg;
    data[1] = value & 0xff;

	ret = i2c_master_send(client, data, 2);

	mutex_unlock(&d2153_aad->d2153_codec->d2153_pmic->d2153_io_mutex);

	if (ret == 2)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}
EXPORT_SYMBOL(d2153_aad_write);

int d2153_aad_update_bits(struct i2c_client *client, u8 reg, u8 mask,
				 u8 value)
{
	int change;
	unsigned int old, new;
	int ret;

	ret = d2153_aad_read(client, reg);
	if (ret < 0)
		return ret;

	old = ret;
	new = (old & ~mask) | value;
	change = old != new;
	if (change) {
		ret = d2153_aad_write(client, reg, new);
		if (ret < 0)
			return ret;
	}

	return change;
}
EXPORT_SYMBOL(d2153_aad_update_bits);

int d2153_aad_enable(struct snd_soc_codec *codec)
{
	struct d2153_codec_priv *d2153_codec =
		snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = d2153_codec->aad_i2c_client;
	struct d2153_aad_priv *d2153_aad = i2c_get_clientdata(client);

	d2153_aad_write(client, D2153_ACCDET_UNLOCK_AO, 0x4a);

	d2153_aad_write(client, D2153_ACCDET_TST2, 0x10);
	d2153_aad_write(client, D2153_ACCDET_THRESH1, 0x1a);
	d2153_aad_write(client, D2153_ACCDET_THRESH2, 0x62);
	d2153_aad_write(client, D2153_ACCDET_THRESH3, 0x08);
	d2153_aad_write(client, D2153_ACCDET_THRESH4, 0x44);

	snd_soc_write(codec, D2153_REFERENCES, 0x88);

	d2153_aad_write(client, D2153_ACCDET_CFG1, 0x5f);
	d2153_aad_write(client, D2153_ACCDET_CFG2, 0x00);
	d2153_aad_write(client, D2153_ACCDET_CFG3, 0x03);

	if (D2153_AA_Silicon == d2153_aad->chip_rev)
		d2153_aad_write(client, D2153_ACCDET_CFG4, 0x07);

	snd_soc_write(codec, D2153_UNLOCK, 0x8b);

#ifndef D2153_DEFAULT_SET_MICBIAS
	snd_soc_update_bits(codec, D2153_MICBIAS1_CTRL,
		D2153_MICBIAS_LEVEL_MASK, d2153_codec->micbias1_level);
#else
	snd_soc_update_bits(codec, D2153_MICBIAS1_CTRL,
		D2153_MICBIAS_LEVEL_MASK, D2153_MICBIAS_LEVEL_2_6V);
#endif	/* D2153_DEFAULT_SET_MICBIAS */

	if (d2153_aad->codec_detect_enable)
		d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x88);
	else
		d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x80);

	if (D2153_AA_Silicon == d2153_aad->chip_rev)
		/* ignore the fist interrupt */
		d2153_aad->button.status = D2153_BUTTON_PRESS;
	return 0;
}
EXPORT_SYMBOL(d2153_aad_enable);

static int d2153_init_setup_register(struct snd_soc_codec *codec)
{

	struct d2153_codec_priv *d2153_codec =
			snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = d2153_codec->aad_i2c_client;
	struct d2153_aad_priv *d2153_aad = i2c_get_clientdata(client);

	/* Default to using ALC auto offset calibration mode. */
	snd_soc_update_bits(codec, D2153_ALC_CTRL1,
			D2153_ALC_CALIB_MODE_MAN, 0);
	d2153_codec->alc_calib_auto = true;

	/* Default LDO settings */
	snd_soc_update_bits(codec, D2153_LDO_CTRL,
			D2153_LDO_LEVEL_SELECT_MASK,
			D2153_LDO_LEVEL_SELECT_1_05V);
	snd_soc_update_bits(codec, D2153_LDO_CTRL,
			D2153_LDO_EN_MASK,
			D2153_LDO_EN_MASK);

#ifndef D2153_DEFAULT_SET_MICBIAS
	snd_soc_update_bits(d2153_aad->d2153_codec->codec,
			D2153_MICBIAS1_CTRL, D2153_MICBIAS_LEVEL_MASK,
			d2153_codec->micbias1_level);
#else
	snd_soc_update_bits(d2153_aad->d2153_codec->codec,
			D2153_MICBIAS1_CTRL, D2153_MICBIAS_LEVEL_MASK,
			D2153_MICBIAS_LEVEL_2_6V);
#endif	/* D2153_DEFAULT_SET_MICBIAS */

	if (d2153_aad->switch_data.state == D2153_HEADSET)
		snd_soc_update_bits(d2153_aad->d2153_codec->codec,
				D2153_MICBIAS1_CTRL,
				D2153_MICBIAS_EN, D2153_MICBIAS_EN);
	return 0;
}

int d2153_codec_power(struct snd_soc_codec *codec, int on)
{
	struct d2153_codec_priv *d2153_codec =
			snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = d2153_codec->aad_i2c_client;
	struct d2153_aad_priv *d2153_aad = i2c_get_clientdata(client);
	struct regulator *regulator;
	#define D2153_LDO_AUD_RECHECK
#ifdef D2153_LDO_AUD_RECHECK
	unsigned char regval = 0;
#endif

	if (on == 1 && d2153_codec->power_mode == 0) {
		dlg_info("%s() Start power = %d\n", __func__, on);

		regulator = regulator_get(NULL, "aud1");
		if (IS_ERR(regulator))
			return -1;
		regulator_set_voltage(regulator, 1800000, 1800000);
		regulator_enable(regulator);
#ifdef D2153_LDO_AUD_RECHECK 
		d2153_reg_read(d2153_codec->d2153_pmic, 0x5E, &regval);
		if(regval == 0x00){
			dlg_err("%s() LDO_AUD1 on regval[0x%x] \n", __func__, regval);
			d2153_reg_write(d2153_codec->d2153_pmic, 0x5E, 0x66);
		}
#endif
		regulator_put(regulator);

		regulator = regulator_get(NULL, "aud2");
		if (IS_ERR(regulator))
			return -1;
		regulator_set_voltage(regulator, 2700000, 2700000);
		regulator_enable(regulator);
		regulator_put(regulator);

		msleep(10);
		snd_soc_write(codec, D2153_UNLOCK, 0x8b);
		snd_soc_write(codec, D2153_REFERENCES, 0x88);
		d2153_init_setup_register(codec);
		d2153_codec->power_mode = 1;
	} else if ((0 == on) && (1 == d2153_codec->power_mode)) {
		dlg_info("%s() Start power = %d\n", __func__, on);

		if ((D2153_HEADSET == d2153_codec->switch_state) &&
				(D2153_AC_Silicon > d2153_aad->chip_rev))
			return 0;

		snd_soc_write(codec, D2153_SYSTEM_MODES_CFG3, 0x01);
		snd_soc_write(codec, D2153_CIF_CTRL, 0x80);
		if (d2153_codec->switch_state == D2153_HEADSET)
			d2153_aad_enable(codec);
		snd_soc_write(codec, D2153_MICBIAS1_CTRL, 0x00);

		regulator = regulator_get(NULL, "aud1");
		if (IS_ERR(regulator))
			return -1;

    	d2153_get_i2c_hwsem();
		regulator_disable(regulator);
#ifdef D2153_LDO_AUD_RECHECK 
		d2153_reg_read(d2153_codec->d2153_pmic, 0x5E, &regval);
		if(regval != 0x00){
			dlg_err("%s() LDO_AUD1 off regval[0x%x] \n", __func__, regval);
			d2153_reg_write(d2153_codec->d2153_pmic, 0x5E, 0x00);
		}
#endif
		d2153_put_i2c_hwsem();

		regulator_put(regulator);

		regulator = regulator_get(NULL, "aud2");
		if (IS_ERR(regulator))
			return -1;

		regulator_disable(regulator);
		regulator_put(regulator);

		d2153_codec->power_mode = 0;
	}

	return 0;
}
EXPORT_SYMBOL(d2153_codec_power);

static irqreturn_t d2153_g_det_handler(int irq, void *data)
{
	struct d2153_aad_priv *d2153_aad = data;
	struct i2c_client *client = d2153_aad->i2c_client;
	dlg_info("[%s] start!\n", __func__);

	if ( d2153_aad->switch_data.state != D2153_NO_JACK)
	{	
		cancel_delayed_work_sync(&d2153_aad->jack_monitor_work);
 		snd_soc_update_bits(d2153_aad->d2153_codec->codec,
			D2153_MICBIAS1_CTRL, D2153_MICBIAS_EN,0);	
		
		dlg_info("[%s] BTN Disable !\n",__func__);
		d2153_aad_write(client,D2153_ACCDET_CONFIG,0x08);
		schedule_delayed_work(&d2153_aad->jack_monitor_work, msecs_to_jiffies(D2153_AAD_JACKOUT_DEBOUNCE_MS)); 	
	} else if(d2153_aad->l_det_status) {
		dlg_info("[%s] d2153_aad->l_det_status [%d]\n",__func__,d2153_aad->l_det_status);
		schedule_delayed_work(&d2153_aad->jack_monitor_work, msecs_to_jiffies(D2153_AAD_JACK_DEBOUNCE_MS)); 	
	}
	return IRQ_HANDLED;
}


/* IRQ handler for HW jack detection */
static irqreturn_t d2153_jack_handler(int irq, void *data)
{
	struct d2153_aad_priv *d2153_aad = data;

	wake_lock_timeout(&d2153_aad->wakeup, HZ * 10);
	dlg_info("[%s] start!\n", __func__);

	if (D2153_NO_JACK == d2153_aad->switch_data.state) {
		schedule_delayed_work(&d2153_aad->jack_monitor_work,
			msecs_to_jiffies(d2153_aad->jack_debounce_ms));
	} else {
		schedule_delayed_work(&d2153_aad->jack_monitor_work,
			msecs_to_jiffies(d2153_aad->jackout_debounce_ms));
	}

	d2153_aad->switch_data.state = D2153_NO_JACK;

	return IRQ_HANDLED;
}

static irqreturn_t d2153_button_handler(int irq, void *data)
{
	struct d2153_aad_priv *d2153_aad = data;

	wake_lock_timeout(&d2153_aad->wakeup, HZ * 10);

	if (D2153_AA_Silicon == d2153_aad->chip_rev) {
		if (d2153_aad->button.status == D2153_BUTTON_IGNORE) {
			d2153_aad->button.status = D2153_BUTTON_PRESS;
			return IRQ_HANDLED;
		}

		if (d2153_aad->button.status == D2153_BUTTON_PRESS) {
			d2153_aad->button.status = D2153_BUTTON_RELEASE;
			return IRQ_HANDLED;
		}
	}

	if (d2153_aad->d2153_codec->power_mode == 0)
		schedule_delayed_work(&d2153_aad->button_monitor_work,
			msecs_to_jiffies(d2153_aad->button_sleep_debounce_ms));
	else
	schedule_delayed_work(&d2153_aad->button_monitor_work,
			msecs_to_jiffies(d2153_aad->button_debounce_ms));

	return IRQ_HANDLED;
}

ssize_t d2153_aad_print_state(struct switch_dev *sdev, char *buf)
{
	struct d2153_switch_data *switch_data =
		container_of(sdev, struct d2153_switch_data, sdev);
	struct d2153_aad_priv *d2153_aad =
		container_of(switch_data, struct d2153_aad_priv, switch_data);
	wake_unlock(&d2153_aad->wakeup);
	return sprintf(buf, "%d\n", sdev->state);
}

static int d2153_switch_dev_register(struct d2153_aad_priv *d2153_aad)
{
	int ret = 0;
	dlg_info("[%s] start!\n", __func__);

	d2153_aad->switch_data.sdev.name = "h2w";
	d2153_aad->switch_data.sdev.state = 0;
	d2153_aad->switch_data.state = 0;
	d2153_aad->button.status=D2153_BUTTON_RELEASE;
	d2153_aad->switch_data.sdev.print_state = d2153_aad_print_state;
	ret = switch_dev_register(&d2153_aad->switch_data.sdev);

	return ret;
}

static int d2153_hooksw_dev_register(struct i2c_client *client,
				struct d2153_aad_priv *d2153_aad)
{
	int ret = 0;
	dlg_info("[%s] start!\n", __func__);
	d2153_aad->input_dev = input_allocate_device();
	d2153_aad->input_dev->name = "d2153-aad";
	d2153_aad->input_dev->id.bustype = BUS_I2C;
	d2153_aad->input_dev->dev.parent = &client->dev;

	__set_bit(EV_KEY, d2153_aad->input_dev->evbit);
	__set_bit(KEY_VOLUMEUP, d2153_aad->input_dev->keybit);
	__set_bit(KEY_VOLUMEDOWN, d2153_aad->input_dev->keybit);
	__set_bit(KEY_MEDIA, d2153_aad->input_dev->keybit);
	ret = input_register_device(d2153_aad->input_dev);

	if (0 != ret) {
		input_free_device(d2153_aad->input_dev);
	}

	return ret;
}

static void d2153_aad_jackdet_monitor_timer_work(struct work_struct *work)
{
	struct d2153_aad_priv *d2153_aad =
		container_of(work, struct d2153_aad_priv,
		jack_monitor_work.work);

	struct i2c_client *client = d2153_aad->i2c_client;
	u8 jack_mode,btn_status;
	int state = d2153_aad->switch_data.state;
	int state_gpio;
	struct snd_soc_codec *codec;

	dlg_info("[%s] start!\n", __func__);

	if (d2153_aad->d2153_codec == NULL ||
		d2153_aad->d2153_codec->codec_init == 0) {
		schedule_delayed_work(&d2153_aad->jack_monitor_work,
			msecs_to_jiffies(300));
		return;
	}

	codec = d2153_aad->d2153_codec->codec;
	snd_soc_update_bits(d2153_aad->d2153_codec->codec,
			D2153_MICBIAS1_CTRL,
			D2153_MICBIAS_EN, D2153_MICBIAS_EN);

	msleep(D2153_AAD_MICBIAS_SETUP_TIME_MS);

	jack_mode = d2153_aad_read(client, D2153_ACCDET_CFG3);
	dlg_info(" %s, JACK MODE = 0x%x\n", __func__, jack_mode);

	if (jack_mode & D2153_ACCDET_JACK_MODE_JACK) {
		d2153_aad->l_det_status = true;

		if (d2153_aad->gpio_detect_enable) {
			state_gpio = gpio_get_value(d2153_aad->gpio_port);
			if (state_gpio == 1) {
				dlg_info(" %s, state_gpio = 0x%x\n",
						__func__, state_gpio);
				snd_soc_update_bits(
						d2153_aad->d2153_codec->codec,
						D2153_MICBIAS1_CTRL,
						D2153_MICBIAS_EN, 0);
				state = D2153_NO_JACK;

				if (d2153_aad->switch_data.state != state) {
					d2153_aad->button.status =
							D2153_BUTTON_RELEASE;
					dlg_info("%s Jack state = %d\n",
							__func__, state);
					d2153_aad->d2153_codec->switch_state =
							state;
					d2153_aad->switch_data.state =
							state;
					switch_set_state(
						&d2153_aad->switch_data.sdev,
						state);
				}
				return;
			}
		}
		if (jack_mode & D2153_ACCDET_JACK_MODE_MIC) {
			d2153_unmask_irq(d2153_aad->d2153_codec->d2153_pmic,
				D2153_IRQ_EACCDET);
			dlg_info("%s d2153_unmask_irq - D2153_IRQ_EACCDET\n",
				__func__);
			dlg_info("%s JACK MODE! 4 Pole Heaset set\n",
				__func__);
			state = D2153_HEADSET;
			d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x88);
		} else {
			d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x88);
			if (D2153_AA_Silicon == d2153_aad->chip_rev)
				d2153_aad_write(client,
					D2153_ACCDET_CFG4, 0x17);

			msleep(d2153_aad->button_detect_rate);

			btn_status = d2153_aad_read(client,
				D2153_ACCDET_STATUS);
			dlg_info("%s Heaset set ADC= %d\n",
				__func__, btn_status);

			if (D2153_AA_Silicon == d2153_aad->chip_rev)
				d2153_aad_update_bits(client,
					D2153_ACCDET_CFG4,
					D2153_ACCDET_ADC_COMP_OUT_INV,
					0);

			if (btn_status < D2153_AAD_DETECT_JACK_ADC) {
				dlg_info("%s ADC CHECK!! 3 Pole Heaset set2\n",
					__func__);
				d2153_aad_write(client,
					D2153_ACCDET_CONFIG, 0x08);
				state = D2153_HEADPHONE;
				snd_soc_update_bits(
					d2153_aad->d2153_codec->codec,
					D2153_MICBIAS1_CTRL,
					D2153_MICBIAS_EN, 0);
			} else {
				d2153_unmask_irq(
					d2153_aad->d2153_codec->d2153_pmic,
					D2153_IRQ_EACCDET);
				dlg_info("%s d2153_unmask_irq " \
					"- D2153_IRQ_EACCDET\n", __func__);
				dlg_info("%s ADC CHECK!! 4 Pole Heaset set2\n",
					__func__);
				state = D2153_HEADSET;
				d2153_aad_write(client,
					D2153_ACCDET_CONFIG, 0x88);
			}
	}
	} else {
		d2153_mask_irq(d2153_aad->d2153_codec->d2153_pmic,
			D2153_IRQ_EACCDET);
		dlg_info("%s d2153_unmask_irq - D2153_IRQ_EACCDET\n",
			__func__);
		dlg_info("%s Jack Pull Out !\n", __func__);
		state=D2153_NO_JACK;
		snd_soc_update_bits(d2153_aad->d2153_codec->codec,
			D2153_MICBIAS1_CTRL, D2153_MICBIAS_EN, 0);
		d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x08);
		d2153_aad->l_det_status = false;
	}

	d2153_aad->button.status = D2153_BUTTON_RELEASE;

	if (d2153_aad->switch_data.state != state ||
			d2153_aad->switch_data.state == D2153_NO_JACK) {
		dlg_info("%s Jack state = %d\n", __func__, state);
		d2153_aad->d2153_codec->switch_state=state;
		d2153_aad->switch_data.state = state;
		switch_set_state(&d2153_aad->switch_data.sdev, state);
	}

	mutex_lock_nested(&codec->card->dapm_mutex,
		SND_SOC_DAPM_CLASS_PCM);
	if (state == D2153_NO_JACK)
		snd_soc_dapm_disable_pin(&d2153_aad->d2153_codec->codec->dapm,
			"Headphone Enable");
	else
		snd_soc_dapm_enable_pin(&d2153_aad->d2153_codec->codec->dapm,
			"Headphone Enable");
	mutex_unlock(&codec->card->dapm_mutex);
}

static void d2153_aad_gpio_monitor_timer_work(struct work_struct *work)
{
	struct d2153_aad_priv *d2153_aad =
		container_of(work, struct d2153_aad_priv,
		jack_monitor_work.work);

	struct i2c_client *client = d2153_aad->i2c_client;
	u8 jack_mode,btn_status;
	int state = d2153_aad->switch_data.state,state_gpio;
	struct snd_soc_codec *codec;
	
	dlg_info("[%s] start!\n", __func__);

	if (d2153_aad->d2153_codec == NULL ||
		d2153_aad->d2153_codec->codec_init == 0) {
		schedule_delayed_work(&d2153_aad->jack_monitor_work,
			msecs_to_jiffies(300));
		return;
	}

	codec = d2153_aad->d2153_codec->codec;
	state_gpio = gpio_get_value(d2153_aad->gpio_port);
	if (state_gpio == 0 && state != D2153_NO_JACK)
		return;

	if (state_gpio == 0) {

		d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x80);

		snd_soc_update_bits(d2153_aad->d2153_codec->codec,
				D2153_MICBIAS1_CTRL,
				D2153_MICBIAS_EN, D2153_MICBIAS_EN);
		msleep(D2153_AAD_MICBIAS_SETUP_TIME_MS);

		jack_mode = d2153_aad_read(client, D2153_ACCDET_CFG3);

		if (jack_mode & D2153_ACCDET_JACK_MODE_MIC) {
			dlg_info("[%s] 4 Pole Heaset set\n", __func__);
			state = D2153_HEADSET;
			d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x80);

			d2153_unmask_irq(d2153_aad->d2153_codec->d2153_pmic,
				D2153_IRQ_EACCDET);
			dlg_info("%s d2153_unmask_irq - D2153_IRQ_EACCDET\n",
				__func__);
		} else {
			d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x80);


			if (D2153_AA_Silicon == d2153_aad->chip_rev)
				d2153_aad_write(client,
					D2153_ACCDET_CFG4, 0x17);


			btn_status = d2153_aad_read(client,
					D2153_ACCDET_STATUS);
			dlg_info("%s Heaset set ADC= %d\n",
					__func__, btn_status);

			if (D2153_AA_Silicon == d2153_aad->chip_rev)
				d2153_aad_update_bits(client,
					D2153_ACCDET_CFG4,
					D2153_ACCDET_ADC_COMP_OUT_INV,
					0);

			if (btn_status < D2153_AAD_DETECT_JACK_ADC) {
				dlg_info("[%s] First 3 Pole Heaset set2\n",
					__func__);

			snd_soc_update_bits(d2153_aad->d2153_codec->codec,
				D2153_MICBIAS1_CTRL, D2153_MICBIAS_EN, 0);

				d2153_aad_write(client,
					D2153_ACCDET_CONFIG, 0x00);
				state = D2153_HEADPHONE;
			} else {
				d2153_unmask_irq(
					d2153_aad->d2153_codec->d2153_pmic,
					D2153_IRQ_EACCDET);
				dlg_info("%s d2153_unmask_irq " \
					"- D2153_IRQ_EACCDET\n", __func__);

				dlg_info("[%s] First 4 Pole Heaset set2\n",
					__func__);
				state = D2153_HEADSET;
				d2153_aad_write(client,
					D2153_ACCDET_CONFIG, 0x80);
			}
		}
	} else {
		d2153_mask_irq(d2153_aad->d2153_codec->d2153_pmic,
			D2153_IRQ_EACCDET);
		dlg_info("%s d2153_unmask_irq - D2153_IRQ_EACCDET\n",
			__func__);

		dlg_info("%s Jack Pull Out !\n", __func__);
		state = D2153_NO_JACK;
		snd_soc_update_bits(d2153_aad->d2153_codec->codec,
			D2153_MICBIAS1_CTRL, D2153_MICBIAS_EN, 0);
		d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x00);
	}

	d2153_aad->button.status = D2153_BUTTON_RELEASE;

	if (d2153_aad->switch_data.state != state ||
		d2153_aad->switch_data.state == D2153_NO_JACK) {
		d2153_aad->d2153_codec->switch_state=state;
		d2153_aad->switch_data.state = state;
		switch_set_state(&d2153_aad->switch_data.sdev, state);
	}

	mutex_lock_nested(&codec->card->dapm_mutex,
		SND_SOC_DAPM_CLASS_PCM);
	if (state == D2153_NO_JACK)
		snd_soc_dapm_disable_pin(&d2153_aad->d2153_codec->codec->dapm,
			"Headphone Enable");
	else
		snd_soc_dapm_enable_pin(&d2153_aad->d2153_codec->codec->dapm,
			"Headphone Enable");
	mutex_unlock(&codec->card->dapm_mutex);
}

static void d2153_aad_button_monitor_timer_work(struct work_struct *work)
{
	struct d2153_aad_priv *d2153_aad =
		container_of(work, struct d2153_aad_priv,
		button_monitor_work.work);
	struct i2c_client *client = d2153_aad->i2c_client;
	u8 btn_status;
	u8 jack_mode;
	int state_gpio;
	u8 no_jack = 0;

	dlg_info("[%s] start!!\n", __func__);

	if (d2153_aad->d2153_codec == NULL ||
		d2153_aad->d2153_codec->codec_init == 0) {
		schedule_delayed_work(&d2153_aad->button_monitor_work,
			msecs_to_jiffies(300));
		return;
	}
	if (d2153_aad->switch_data.state != D2153_HEADSET)
		return;

	if (D2153_AC_Silicon <= d2153_aad->chip_rev)
		d2153_codec_power(d2153_aad->d2153_codec->codec, 1);

	if (D2153_AA_Silicon == d2153_aad->chip_rev) {
		d2153_aad_write(client, D2153_ACCDET_CFG4, 0x17);
		d2153_aad->button.status = D2153_BUTTON_IGNORE;
	}

	msleep(d2153_aad->button_detect_rate);
	if (d2153_aad->codec_detect_enable) {
		jack_mode = d2153_aad_read(client, D2153_ACCDET_CFG3);
		if (0 == (jack_mode & D2153_ACCDET_JACK_MODE_JACK))
			no_jack = 1;
	} else {
		state_gpio = gpio_get_value(d2153_aad->gpio_port);
		if (state_gpio == 1)
			no_jack = 1;
	}

	if (no_jack) {
		d2153_aad->switch_data.state = D2153_NO_JACK;
		snd_soc_update_bits(d2153_aad->d2153_codec->codec,
			D2153_MICBIAS1_CTRL, D2153_MICBIAS_EN, 0);
		switch_set_state(&d2153_aad->switch_data.sdev,
			d2153_aad->switch_data.state);
		dlg_info("%s Jack Pull Out !\n", __func__);
		return;
	}

	btn_status = d2153_aad_read(client, D2153_ACCDET_STATUS);
	dlg_info("%s btn = %d !\n", __func__, btn_status);

	if (D2153_AA_Silicon == d2153_aad->chip_rev)
		d2153_aad_update_bits(client, D2153_ACCDET_CFG4,
			D2153_ACCDET_ADC_COMP_OUT_INV, 0);

	if ((btn_status >= button_res_tbl[SEND_BUTTON].min_val) &&
		(btn_status <= button_res_tbl[SEND_BUTTON].max_val)) {

		d2153_aad->button.key = KEY_MEDIA;
		d2153_aad->button.status = D2153_BUTTON_PRESS;
		input_event(d2153_aad->input_dev, EV_KEY,
			d2153_aad->button.key, 1);
		input_sync(d2153_aad->input_dev);
		dlg_info("%s event Send Press !\n", __func__);
	} else if ((btn_status >= button_res_tbl[VOL_UP_BUTTON].min_val) &&
		(btn_status <= button_res_tbl[VOL_UP_BUTTON].max_val)) {

		d2153_aad->button.key = KEY_VOLUMEUP;
		d2153_aad->button.status = D2153_BUTTON_PRESS;
		input_event(d2153_aad->input_dev, EV_KEY,
			d2153_aad->button.key, 1);
		input_sync(d2153_aad->input_dev);
		dlg_info("%s event VOL UP Press !\n", __func__);
	} else if ((btn_status >= button_res_tbl[VOL_DN_BUTTON].min_val) &&
		(btn_status <= button_res_tbl[VOL_DN_BUTTON].max_val)) {

		d2153_aad->button.key = KEY_VOLUMEDOWN;
		d2153_aad->button.status = D2153_BUTTON_PRESS;
		input_event(d2153_aad->input_dev, EV_KEY,
			d2153_aad->button.key, 1);
		input_sync(d2153_aad->input_dev);
		dlg_info("%s event VOL DOWN Press !\n", __func__);
	} else {
		if (d2153_aad->button.status == D2153_BUTTON_PRESS) {
			input_event(d2153_aad->input_dev, EV_KEY,
				d2153_aad->button.key, 0);
			input_sync(d2153_aad->input_dev);
			d2153_aad->button.status = D2153_BUTTON_RELEASE;
			dlg_info("%s event Rel key=%d!\n", __func__,
				d2153_aad->button.key);
		}


	}
}

static int __devinit d2153_aad_i2c_probe(struct i2c_client *client,
					 const struct i2c_device_id *id)
{
	struct d2153_aad_priv *d2153_aad;
	int ret;
	void *jackdet_work_func;
	void *d2153_handler_func;
	void *gpio_handler_func;
	u8 regval;
#ifndef D2153_DEFAULT_SET_MICBIAS
	struct d2153 *pmic;
#endif	/* D2153_DEFAULT_SET_MICBIAS */
	struct d2153_audio *audio;

	dlg_info("[%s] start!\n", __func__);

	d2153_aad = devm_kzalloc(&client->dev, sizeof(struct d2153_aad_priv),
				 GFP_KERNEL);
	if (!d2153_aad)
		return -ENOMEM;

	d2153_aad->i2c_client = client;
	d2153_aad->d2153_codec = client->dev.platform_data;
	add_client = client;

	wake_lock_init(&d2153_aad->wakeup, WAKE_LOCK_SUSPEND, "jack_monitor");

	i2c_set_clientdata(client, d2153_aad);

	d2153_switch_dev_register(d2153_aad);

	d2153_hooksw_dev_register(client, d2153_aad);

#ifndef D2153_DEFAULT_SET_MICBIAS
	pmic = d2153_aad->d2153_codec->d2153_pmic;
	if (D2153_MICBIAS_LEVEL_2_5V == pmic->pdata->audio.micbias1_level)
		button_res_tbl = button_res_2V5_tbl;
	else
#endif	/* D2153_DEFAULT_SET_MICBIAS */
		button_res_tbl = button_res_2V6_tbl;

	d2153_aad_ex = d2153_aad;

	audio = &(d2153_aad->d2153_codec->d2153_pmic->pdata->audio);
	d2153_aad->button_debounce_ms = audio->aad_button_debounce_ms;
	d2153_aad->button_sleep_debounce_ms =
			audio->aad_button_sleep_debounce_ms;
	d2153_aad->jack_debounce_ms = audio->aad_jack_debounce_ms;
	d2153_aad->jackout_debounce_ms = audio->aad_jackout_debounce_ms;
	d2153_aad->codec_detect_enable = audio->aad_codec_detect_enable;
	d2153_aad->gpio_detect_enable = audio->aad_gpio_detect_enable;
	d2153_aad->gpio_port = audio->aad_gpio_port;
	d2153_aad->button_detect_rate = 20;

	/* Ensure jack & button detection is disabled, default to auto power */
	d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x00);

	if (d2153_aad->codec_detect_enable) {
		d2153_handler_func = d2153_jack_handler;
		gpio_handler_func = d2153_g_det_handler;
		jackdet_work_func = d2153_aad_jackdet_monitor_timer_work;
	} else {
		d2153_handler_func = NULL;
		gpio_handler_func = d2153_jack_handler;
		jackdet_work_func = d2153_aad_gpio_monitor_timer_work;
	}

	INIT_DELAYED_WORK(&d2153_aad->jack_monitor_work,
			jackdet_work_func);
	INIT_DELAYED_WORK(&d2153_aad->button_monitor_work,
			d2153_aad_button_monitor_timer_work);

	if (d2153_handler_func) {
		/* Register virtual IRQs with PMIC */
		/* for jack & button detection */
		d2153_register_irq(d2153_aad->d2153_codec->d2153_pmic,
				   D2153_IRQ_EJACKDET, d2153_jack_handler, 0,
				   "Jack detect", d2153_aad);
	}

	d2153_aad->g_det_irq = gpio_to_irq(d2153_aad->gpio_port);
	dlg_info("[%s] : qpio_to_irq= %d\n", __func__, d2153_aad->g_det_irq);
	ret = request_threaded_irq(d2153_aad->g_det_irq, NULL,
				gpio_handler_func,
				IRQF_TRIGGER_FALLING |
				IRQF_TRIGGER_RISING |
				IRQF_DISABLED,
				(d2153_aad->codec_detect_enable ?
					"GPIO detect" : "Jack detect"),
				d2153_aad);
	dlg_info("[%s] : request_threaded_irq= %d\n", __func__, ret);
	enable_irq_wake(d2153_aad->g_det_irq);	
	/* Generate node for DFMS  */
	dlg_info("--------------------------------------------\n");
	d2153_aad_ex->audio_class = class_create(THIS_MODULE, "audio");
	d2153_aad_ex->headset_dev = device_create(d2153_aad_ex->audio_class,
						NULL, client->dev.devt,
						NULL, "earjack");
	ret = device_create_file(d2153_aad_ex->headset_dev,
			&headset_Attrs[STATE]);
	if (0 != ret)
		dlg_err("[%s] : device_create_file(STATE)= %d\n",
			__func__, ret);
	ret = device_create_file(d2153_aad_ex->headset_dev,
			&headset_Attrs[KEY_STATE]);
	if (0 != ret)
		dlg_err("[%s] : device_create_file(KEY_STATE)= %d\n",
			__func__, ret);
	ret = device_create_file(d2153_aad_ex->headset_dev,
			&headset_Attrs[ADC]);
	if (0 != ret)
		dlg_err("[%s] : device_create_file(ADC)= %d\n",
			__func__, ret);
	dlg_info("--------------------------------------------+++++++\n");
	d2153_register_irq(d2153_aad->d2153_codec->d2153_pmic,
			D2153_IRQ_EACCDET, d2153_button_handler, 0,
			"Button detect", d2153_aad);


	d2153_reg_read(d2153_aad->d2153_codec->d2153_pmic, 0x96, &regval);
	d2153_aad->chip_rev = regval;
	d2153_aad->l_det_status = false;

	dlg_info("%s chip_rev [0x%x]\n", __func__, d2153_aad->chip_rev);

	schedule_delayed_work(&d2153_aad->jack_monitor_work,
			msecs_to_jiffies(300));

	return 0;
}

static int __devexit d2153_aad_i2c_remove(struct i2c_client *client)
{
	struct d2153_aad_priv *d2153_aad = i2c_get_clientdata(client);
	struct d2153 *d2153_pmic = d2153_aad->d2153_codec->d2153_pmic;

	/* Disable jack & button detection, default to auto power */
	d2153_aad_write(client, D2153_ACCDET_CONFIG, 0);

	/* Free up virtual IRQs from PMIC */
	d2153_free_irq(d2153_pmic, D2153_IRQ_EJACKDET);
	d2153_free_irq(d2153_pmic, D2153_IRQ_EACCDET);

	return 0;
}

static const struct i2c_device_id d2153_aad_i2c_id[] = {
	{ "d2153-aad", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, d2153_aad_i2c_id);

/* I2C AAD control layer */
static struct i2c_driver d2153_aad_i2c_driver = {
	.driver = {
		.name = "d2153-aad",
		.owner = THIS_MODULE,
	},
	.probe		= d2153_aad_i2c_probe,
	.remove		= __devexit_p(d2153_aad_i2c_remove),
	.id_table	= d2153_aad_i2c_id,
};

static int __init d2153_aad_init(void)
{
	int ret;

	ret = i2c_add_driver(&d2153_aad_i2c_driver);
	if (ret)
		pr_err("D2153 AAD I2C registration failed %d\n", ret);

	return ret;
}
module_init(d2153_aad_init);

static void __exit d2153_aad_exit(void)
{
	i2c_del_driver(&d2153_aad_i2c_driver);
}
module_exit(d2153_aad_exit);

MODULE_DESCRIPTION("ASoC D2153 AAD Driver");
MODULE_AUTHOR("Adam Thomson <Adam.Thomson.Opensource@diasemi.com>");
MODULE_LICENSE("GPL");
