/*
 * D2153 ALSA SoC codec driver
 *
 * Copyright (c) 2012 Dialog Semiconductor
 *
 * Written by Adam Thomson <Adam.Thomson.Opensource@diasemi.com>
 * Based on DA9055 ALSA SoC codec driver.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/clk.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif
#include <mach/common.h>

//#define CONFIG_SND_SOC_USE_DA9055_HW

#ifdef CONFIG_SND_SOC_USE_DA9055_HW
#include "d2153_da9055.h"
#else
#include <linux/d2153/d2153_codec.h>
#endif /* CONFIG_SND_SOC_USE_DA9055_HW */

#ifdef CONFIG_SND_SOC_D2153_AAD
#include <linux/d2153/d2153_aad.h>
#endif /* CONFIG_SND_SOC_D2153_AAD */

/*
 * Controls section
 */

static const char *mixout_r_select_widget;

u8 d2153_aif_adjusted_val [] = {
	0x01,0x11,0x02,0x12,0x03,0x13,0x04,0x14,
	0x05,0x15,0x06,0x16,0x07,0x17,0x08,0x18,
	0x09,0x19,0x0a,0x1a,0x0b,0x1b,0x0c,0x1c,
	0x0d,0x1d,0x0e,0x1e,0x0f,0x1f,0x10,0x00,
	0x11,0x02,0x12,0x03,0x13,0x04,0x14,0x00,
};

/*
 * Gain and Volume
 */

static const DECLARE_TLV_DB_SCALE(dac_ng_threshold_tlv, -9000, 600, 0);

static const unsigned int aux_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x0, 0x10, TLV_DB_SCALE_ITEM(-5400, 0, 0),
	/* -54dB to 15dB */
	0x11, 0x3f, TLV_DB_SCALE_ITEM(-5400, 150, 0)
};

static const unsigned int digital_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x0, 0x07, TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
	/* -78dB to 12dB */
	0x08, 0x7f, TLV_DB_SCALE_ITEM(-7800, 75, 0)
};

static const unsigned int alc_analog_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	0x0, 0x0, TLV_DB_SCALE_ITEM(TLV_DB_GAIN_MUTE, 0, 1),
	/* 0dB to 36dB */
	0x01, 0x07, TLV_DB_SCALE_ITEM(0, 600, 0)
};

static const unsigned int hp_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	/* -56dB to 5.5dB */
	0x0, 0x7b, TLV_DB_SCALE_ITEM(-5600, 50, 0),
	/* 6dB */
	0x7c, 0x7f, TLV_DB_SCALE_ITEM(600, 0, 0)
};
static const unsigned int ep_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	/* -56dB to 5.5dB */
	0x0, 0x7b, TLV_DB_SCALE_ITEM(-5600, 50, 0),
	/* 6dB */
	0x7c, 0x7f, TLV_DB_SCALE_ITEM(600, 0, 0)
};
static const unsigned int sp_gain_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	/* -24dB */
	0x0, 0x1b, TLV_DB_SCALE_ITEM(-2400, 0, 0),
	/* -23dB to 12dB */
	0x1c, 0x3f, TLV_DB_SCALE_ITEM(-2300, 100, 0)
};
static const unsigned int sp_ng_att_tlv[] = {
	TLV_DB_RANGE_HEAD(2),
	/* -24dB */
	0x0, 0x2, TLV_DB_SCALE_ITEM(-2400, 0, 0),
	/* 12dB */
	0x3, 0x3, TLV_DB_SCALE_ITEM(-1200, 0, 0)
};

static const DECLARE_TLV_DB_SCALE(mic_gain_tlv, -600, 600, 0);
static const DECLARE_TLV_DB_SCALE(mixin_gain_tlv, -450, 150, 0);
static const DECLARE_TLV_DB_SCALE(eq_gain_tlv, -1050, 150, 0);
static const DECLARE_TLV_DB_SCALE(adc_eq_overall_gain_tlv, -1200, 600, 0);
static const DECLARE_TLV_DB_SCALE(alc_threshold_tlv, -9450, 150, 0);
static const DECLARE_TLV_DB_SCALE(alc_gain_tlv, 0, 600, 0);
static const DECLARE_TLV_DB_SCALE(sp_thd_lim_tlv, 0, 10, 0);

/*
 * Enums
 */

/* Gain ramping rate value */
static const char * const d2153_gain_ramping_txt[] = {
	"nominal rate", "nominal rate * 4", "nominal rate * 8",
	"nominal rate / 8"
};

static const struct soc_enum d2153_gain_ramping_rate =
	SOC_ENUM_SINGLE(D2153_GAIN_RAMP_CTRL, D2153_GAIN_RAMP_RATE_SHIFT,
			D2153_GAIN_RAMP_RATE_MAX, d2153_gain_ramping_txt);

/* ADC and DAC voice mode (8kHz) high pass cutoff value */
static const char * const d2153_vf_cutoff_txt[] = {
	"2.5Hz", "25Hz", "50Hz", "100Hz", "150Hz", "200Hz", "300Hz", "400Hz"
};

static const struct soc_enum d2153_dac_vf_cutoff =
	SOC_ENUM_SINGLE(D2153_DAC_FILTERS1, D2153_VOICE_HPF_CORNER_SHIFT,
			D2153_VOICE_HPF_CORNER_MAX, d2153_vf_cutoff_txt);

static const struct soc_enum d2153_adc_vf_cutoff =
	SOC_ENUM_SINGLE(D2153_ADC_FILTERS1, D2153_VOICE_HPF_CORNER_SHIFT,
			D2153_VOICE_HPF_CORNER_MAX, d2153_vf_cutoff_txt);

/* ADC and DAC high pass filter cutoff value */
static const char * const d2153_hpf_cutoff_txt[] = {
	"Fs/24000", "Fs/12000", "Fs/6000", "Fs/3000"
};

static const struct soc_enum d2153_dac_hpf_cutoff =
	SOC_ENUM_SINGLE(D2153_DAC_FILTERS1, D2153_AUDIO_HPF_CORNER_SHIFT,
			D2153_AUDIO_HPF_CORNER_MAX, d2153_hpf_cutoff_txt);

static const struct soc_enum d2153_adc_hpf_cutoff =
	SOC_ENUM_SINGLE(D2153_ADC_FILTERS1, D2153_AUDIO_HPF_CORNER_SHIFT,
			D2153_AUDIO_HPF_CORNER_MAX, d2153_hpf_cutoff_txt);

/* DAC soft mute rate value */
static const char * const d2153_dac_soft_mute_rate_txt[] = {
	"1", "2", "4", "8", "16", "32", "64"
};

static const struct soc_enum d2153_dac_soft_mute_rate =
	SOC_ENUM_SINGLE(D2153_DAC_FILTERS5, D2153_DAC_SOFTMUTE_RATE_SHIFT,
			D2153_DAC_SOFTMUTE_RATE_MAX,
			d2153_dac_soft_mute_rate_txt);

/* ALC Ext Mic Mode */
static const char * const d2153_alc_ext_mic_mode_txt[] = {
	"MIC1_L MIC2_R", "EXT_MIC_L_R"
};

static const struct soc_enum d2153_alc_ext_mic_mode =
	SOC_ENUM_SINGLE(D2153_ALC_CTRL1, D2153_ALC_EXT_MIC_MODE_SHIFT,
			D2153_ALC_EXT_MIC_MODE_MAX, d2153_alc_ext_mic_mode_txt);

/* ALC Attack Rate select */
static const char * const d2153_alc_attack_rate_txt[] = {
	"44/fs", "88/fs", "176/fs", "352/fs", "704/fs", "1408/fs", "2816/fs",
	"5632/fs", "11264/fs", "22528/fs", "45056/fs", "90112/fs", "180224/fs"
};

static const struct soc_enum d2153_alc_attack_rate =
	SOC_ENUM_SINGLE(D2153_ALC_CTRL2, D2153_ALC_ATTACK_SHIFT,
			D2153_ALC_ATTACK_MAX, d2153_alc_attack_rate_txt);

/* ALC Release Rate select */
static const char * const d2153_alc_release_rate_txt[] = {
	"172/fs", "344/fs", "688/fs", "1376/fs", "2752/fs", "5504/fs",
	"11008/fs", "22016/fs", "44032/fs", "88064/fs", "176128/fs"
};

static const struct soc_enum d2153_alc_release_rate =
	SOC_ENUM_SINGLE(D2153_ALC_CTRL2, D2153_ALC_RELEASE_SHIFT,
			D2153_ALC_RELEASE_MAX, d2153_alc_release_rate_txt);

/* ALC Hold Time period select */
static const char * const d2153_alc_hold_time_txt[] = {
	"62/fs", "124/fs", "248/fs", "496/fs", "992/fs", "1984/fs", "3968/fs",
	"7936/fs", "15872/fs", "31744/fs", "63488/fs", "126976/fs",
	"253952/fs", "507904/fs", "1015808/fs", "2031616/fs"
};

static const struct soc_enum d2153_alc_hold_time =
	SOC_ENUM_SINGLE(D2153_ALC_CTRL3, D2153_ALC_HOLD_SHIFT,
			D2153_ALC_HOLD_MAX, d2153_alc_hold_time_txt);

/* ALC Input Signal Tracking rate select */
static const char * const d2153_alc_signal_tracking_rate_txt[] = {
	"1/4", "1/16", "1/256", "1/65536"
};

static const struct soc_enum d2153_alc_integ_attack_rate =
	SOC_ENUM_SINGLE(D2153_ALC_CTRL3, D2153_ALC_INTEG_ATTACK_SHIFT,
			D2153_ALC_INTEG_ATTACK_MAX,
			d2153_alc_signal_tracking_rate_txt);

static const struct soc_enum d2153_alc_integ_release_rate =
	SOC_ENUM_SINGLE(D2153_ALC_CTRL3, D2153_ALC_INTEG_RELEASE_SHIFT,
			D2153_ALC_INTEG_RELEASE_MAX,
			d2153_alc_signal_tracking_rate_txt);

/* DAC noise gate setup time value */
static const char * const d2153_dac_ng_setup_time_txt[] = {
	"256 samples", "512 samples", "1024 samples", "2048 samples"
};

static const struct soc_enum d2153_dac_ng_setup_time =
	SOC_ENUM_SINGLE(D2153_DAC_NG_SETUP_TIME, D2153_DAC_NG_SETUP_TIME_SHIFT,
			D2153_DAC_NG_SETUP_TIME_MAX,
			d2153_dac_ng_setup_time_txt);

/* DAC noise gate rampup rate value */
static const char * const d2153_dac_ng_rampup_txt[] = {
	"20.4 us/dB", "1.28 ms/dB"
};

static const struct soc_enum d2153_dac_ng_rampup_rate =
	SOC_ENUM_SINGLE(D2153_DAC_NG_SETUP_TIME, D2153_DAC_NG_RAMPUP_RATE_SHIFT,
			D2153_DAC_NG_RAMPUP_RATE_MAX, d2153_dac_ng_rampup_txt);

/* DAC noise gate rampdown rate value */
static const char * const d2153_dac_ng_rampdown_txt[] = {
	"81.6 us/dB", "1.28 ms/dB"
};

static const struct soc_enum d2153_dac_ng_rampdown_rate =
	SOC_ENUM_SINGLE(D2153_DAC_NG_SETUP_TIME, D2153_DAC_NG_RAMPDN_RATE_SHIFT,
			D2153_DAC_NG_RAMPDN_RATE_MAX,
			d2153_dac_ng_rampdown_txt);

/* Speaker limiter attack rate */
static const char * const d2153_sp_atk_rate_txt[] = {
	"30 us/dB", "60 us/dB", "120 us/dB", "250 us/dB", "500 us/dB",
	"1 ms/dB", "2 ms/dB"
};

static const struct soc_enum d2153_sp_pwr_thd_atk_rate =
	SOC_ENUM_SINGLE(D2153_LIMITER_CTRL1, D2153_SP_ATK_RATE_SHIFT,
			D2153_SP_ATK_RATE_MAX, d2153_sp_atk_rate_txt);

static const struct soc_enum d2153_sp_atk_rate =
	SOC_ENUM_SINGLE(D2153_NG_CTRL2, D2153_SP_ATK_RATE_SHIFT,
			D2153_SP_ATK_RATE_MAX, d2153_sp_atk_rate_txt);

/* Speaker limiter release rate */
static const char * const d2153_sp_rel_rate_txt[] = {
	"20 ms/dB", "50 ms/dB", "100 ms/dB", "200 ms/dB", "400 ms/dB",
	"700 ms/dB", "1000 ms/dB"
};

static const struct soc_enum d2153_sp_pwr_thd_rel_rate =
	SOC_ENUM_SINGLE(D2153_LIMITER_CTRL1, D2153_SP_REL_RATE_SHIFT,
			D2153_SP_REL_RATE_MAX, d2153_sp_rel_rate_txt);

static const struct soc_enum d2153_sp_rel_rate =
	SOC_ENUM_SINGLE(D2153_NG_CTRL2, D2153_SP_REL_RATE_SHIFT,
			D2153_SP_REL_RATE_MAX, d2153_sp_rel_rate_txt);

/* Speaker limiter power/THD hold time */
static const char * const d2153_sp_pwr_thd_hold_time_txt[] = {
	"30 us", "500 ms", "1 s", "2 s"
};

static const struct soc_enum d2153_sp_pwr_thd_hold_time =
	SOC_ENUM_SINGLE(D2153_LIMITER_CTRL1, D2153_SP_PWR_THD_HOLD_TIME_SHIFT,
			D2153_SP_PWR_THD_HOLD_TIME_MAX,
			d2153_sp_pwr_thd_hold_time_txt);

/* Speaker limiter noise gate hold time */
static const char * const d2153_sp_hold_time_txt[] = {
	"10 ms", "50 ms", "200 ms"
};

static const struct soc_enum d2153_sp_hld_time =
	SOC_ENUM_SINGLE(D2153_NG_CTRL2, D2153_SP_HOLD_TIME_SHIFT,
			D2153_SP_HOLD_TIME_MAX, d2153_sp_hold_time_txt);


/*
 * Control functions
 */
int d2153_set_aif_adjust(struct snd_soc_codec *codec)
{
	int i;
	int loop_max = ARRAY_SIZE(d2153_aif_adjusted_val);
	u8 sr_status;

	dlg_info("%s()\n",__FUNCTION__);

	sr_status = snd_soc_read(codec, D2153_SR);
	if (sr_status > D2153_SR_16000)
	{
		msleep(300);
	}

	snd_soc_update_bits(codec, D2153_DAC_FILTERS5, D2153_DAC_SOFTMUTE_EN,
		 D2153_DAC_SOFTMUTE_EN);
	snd_soc_update_bits(codec, D2153_ADC_L_CTRL,
				D2153_ADC_MUTE_EN, D2153_ADC_MUTE_EN);
	msleep(30);		

	snd_soc_write(codec, D2153_PC_COUNT, 0x02);
	for (i = 0; i < loop_max; i++) {
		snd_soc_write(codec, D2153_AIF_OFFSET,
				d2153_aif_adjusted_val[i]);
		usleep_range(500, 500);
	}
	msleep(50);

	snd_soc_update_bits(codec, D2153_ADC_L_CTRL,
				D2153_ADC_MUTE_EN, 0);
	snd_soc_update_bits(codec, D2153_DAC_FILTERS5,
				D2153_DAC_SOFTMUTE_EN, 0);
	msleep(10);

	return 0;
}
EXPORT_SYMBOL(d2153_set_aif_adjust);

static int d2153_snd_soc_dapm_put_volsw(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	unsigned int val;
	int connect;

	val = (ucontrol->value.integer.value[0] & mask);

	if (invert)
		val = max - val;
	val = val << shift;

	if (val)
		/* new connection */
		connect = invert ? 0 : 1;
	else
		/* old connection must be powered down */
		connect = invert ? 1 : 0;

	if (connect)
		mixout_r_select_widget = widget->name;
	else {
		if (mixout_r_select_widget == NULL
		    || strcmp(mixout_r_select_widget, widget->name) != 0)
			return 0;
		mixout_r_select_widget = NULL;
	}
	return snd_soc_dapm_put_volsw(kcontrol, ucontrol);
}

static int d2153_get_alc_data(struct snd_soc_codec *codec, u8 reg_val)
{
	int mid_data, top_data;
	int sum = 0;
	u8 iteration;

	for (iteration = 0; iteration < D2153_ALC_AVG_ITERATIONS;
	     iteration++) {
		/* Select the left or right channel and capture data */
		snd_soc_write(codec, D2153_ALC_CIC_OP_LVL_CTRL, reg_val);

		/* Select middle 8 bits for read back from data register */
		snd_soc_write(codec, D2153_ALC_CIC_OP_LVL_CTRL,
			      reg_val | D2153_ALC_DATA_MIDDLE);
		mid_data = snd_soc_read(codec, D2153_ALC_CIC_OP_LVL_DATA);

		/* Select top 8 bits for read back from data register */
		snd_soc_write(codec, D2153_ALC_CIC_OP_LVL_CTRL,
			      reg_val | D2153_ALC_DATA_TOP);
		top_data = snd_soc_read(codec, D2153_ALC_CIC_OP_LVL_DATA);

		sum += ((mid_data << 8) | (top_data << 16));
	}

	return sum / D2153_ALC_AVG_ITERATIONS;
}

static void d2153_alc_calib_man(struct snd_soc_codec *codec)
{
	u8 reg_val;
	int avg_left_data, avg_right_data, offset_l, offset_r;

	/* Calculate average for Left and Right data */
	/* Left Data */
	avg_left_data = d2153_get_alc_data(codec,
			D2153_ALC_CIC_OP_CHANNEL_LEFT);
	/* Right Data */
	avg_right_data = d2153_get_alc_data(codec,
			 D2153_ALC_CIC_OP_CHANNEL_RIGHT);

	/* Calculate DC offset */
	offset_l = -avg_left_data;
	offset_r = -avg_right_data;

	reg_val = (offset_l & D2153_ALC_OFFSET_15_8) >> 8;
	snd_soc_write(codec, D2153_ALC_OFFSET_MAN_M_L, reg_val);
	reg_val = (offset_l & D2153_ALC_OFFSET_19_16) >> 16;
	snd_soc_write(codec, D2153_ALC_OFFSET_MAN_U_L, reg_val);

	reg_val = (offset_r & D2153_ALC_OFFSET_15_8) >> 8;
	snd_soc_write(codec, D2153_ALC_OFFSET_MAN_M_R, reg_val);
	reg_val = (offset_r & D2153_ALC_OFFSET_19_16) >> 16;
	snd_soc_write(codec, D2153_ALC_OFFSET_MAN_U_R, reg_val);

	/* Enable analog/digital gain mode & offset cancellation */
	snd_soc_update_bits(codec, D2153_ALC_CTRL1,
			    D2153_ALC_OFFSET_EN | D2153_ALC_SYNC_MODE,
			    D2153_ALC_OFFSET_EN | D2153_ALC_SYNC_MODE);
}

static void d2153_alc_calib_auto(struct snd_soc_codec *codec)
{
	u8 alc_ctrl1;

	/* DLG - Do we need a sleep? Timeout? */
	/* Begin auto calibration and wait for completion */
	snd_soc_update_bits(codec, D2153_ALC_CTRL1, D2153_ALC_AUTO_CALIB_EN,
			    D2153_ALC_AUTO_CALIB_EN);
	do {
		alc_ctrl1 = snd_soc_read(codec, D2153_ALC_CTRL1);
	} while (alc_ctrl1 & D2153_ALC_AUTO_CALIB_EN);

	/* If auto calibration fails, fall back to digital gain only mode */
	if (alc_ctrl1 & D2153_ALC_CALIB_OVERFLOW) {
		dev_warn(codec->dev,
			 "ALC auto calibration failed with overflow\n");
		snd_soc_update_bits(codec, D2153_ALC_CTRL1,
				    D2153_ALC_OFFSET_EN | D2153_ALC_SYNC_MODE,
				    0);
	} else {
		/* Enable analog/digital gain mode & offset cancellation */
		snd_soc_update_bits(codec, D2153_ALC_CTRL1,
				    D2153_ALC_OFFSET_EN | D2153_ALC_SYNC_MODE,
				    D2153_ALC_OFFSET_EN | D2153_ALC_SYNC_MODE);
	}

}

static void d2153_alc_calib(struct snd_soc_codec *codec)
{
	struct d2153_codec_priv *d2153_codec = snd_soc_codec_get_drvdata(codec);
	u8 adc_l_ctrl, adc_r_ctrl;
	u8 mixin_l_sel, mixin_r_sel;
	u8 mic_l_ctrl, mic_r_ctrl, mic_ext_ctrl;

	/* Save current values from ADC control registers */
	adc_l_ctrl = snd_soc_read(codec, D2153_ADC_L_CTRL);
	adc_r_ctrl = snd_soc_read(codec, D2153_ADC_R_CTRL);

	/* Save current values from MIXIN_L/R_SELECT registers */
	mixin_l_sel = snd_soc_read(codec, D2153_MIXIN_L_SELECT);
	mixin_r_sel = snd_soc_read(codec, D2153_MIXIN_R_SELECT);

	/* Save current values from MIC control registers */
	mic_l_ctrl = snd_soc_read(codec, D2153_MIC_L_CTRL);
	mic_r_ctrl = snd_soc_read(codec, D2153_MIC_R_CTRL);
	mic_ext_ctrl = snd_soc_read(codec, D2153_MIC_EXT_CTRL);

	/* Enable ADC Left and Right */
	snd_soc_update_bits(codec, D2153_ADC_L_CTRL, D2153_ADC_EN,
			    D2153_ADC_EN);
	snd_soc_update_bits(codec, D2153_ADC_R_CTRL, D2153_ADC_EN,
			    D2153_ADC_EN);

	/* Enable MIC paths */
	snd_soc_update_bits(codec, D2153_MIXIN_L_SELECT,
			    D2153_MIXIN_L_MIX_SELECT_MIC_L |
			    D2153_MIXIN_L_MIX_SELECT_MIC_R |
			    D2153_MIXIN_L_MIX_SELECT_MIC_EXT,
			    D2153_MIXIN_L_MIX_SELECT_MIC_L |
			    D2153_MIXIN_L_MIX_SELECT_MIC_R |
			    D2153_MIXIN_L_MIX_SELECT_MIC_EXT);
	snd_soc_update_bits(codec, D2153_MIXIN_R_SELECT,
			    D2153_MIXIN_R_MIX_SELECT_MIC_R |
			    D2153_MIXIN_R_MIX_SELECT_MIC_L |
			    D2153_MIXIN_R_MIX_SELECT_MIC_EXT,
			    D2153_MIXIN_R_MIX_SELECT_MIC_R |
			    D2153_MIXIN_R_MIX_SELECT_MIC_L |
			    D2153_MIXIN_R_MIX_SELECT_MIC_EXT);

	/* Mute MIC PGAs */
	snd_soc_update_bits(codec, D2153_MIC_L_CTRL, D2153_MIC_AMP_MUTE_EN,
			    D2153_MIC_AMP_MUTE_EN);
	snd_soc_update_bits(codec, D2153_MIC_R_CTRL, D2153_MIC_AMP_MUTE_EN,
			    D2153_MIC_AMP_MUTE_EN);
	snd_soc_update_bits(codec, D2153_MIC_EXT_CTRL, D2153_MIC_AMP_MUTE_EN,
			    D2153_MIC_AMP_MUTE_EN);

	/* Perform calibration */
	if (d2153_codec->alc_calib_auto)
		d2153_alc_calib_auto(codec);
	else
		d2153_alc_calib_man(codec);

	/* Restore MIXIN_L/R_SELECT registers to their original states */
	snd_soc_write(codec, D2153_MIXIN_L_SELECT, mixin_l_sel);
	snd_soc_write(codec, D2153_MIXIN_R_SELECT, mixin_r_sel);

	/* Restore ADC control registers to their original states */
	snd_soc_write(codec, D2153_ADC_L_CTRL, adc_l_ctrl);
	snd_soc_write(codec, D2153_ADC_R_CTRL, adc_r_ctrl);

	/* Restore original values of MIC control registers */
	snd_soc_write(codec, D2153_MIC_L_CTRL, mic_l_ctrl);
	snd_soc_write(codec, D2153_MIC_R_CTRL, mic_r_ctrl);
	snd_soc_write(codec, D2153_MIC_EXT_CTRL, mic_ext_ctrl);
}

static int d2153_put_mixin_gain(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2153_codec_priv *d2153_codec = snd_soc_codec_get_drvdata(codec);
	int ret;

	/*
	 * DLG - Does setting the register before calibration have an impact
	 * on recording quality? This may require some tweaking, or user
	 * nees to disable ALC first before changing this.
	 */
	ret = snd_soc_put_volsw_2r(kcontrol, ucontrol);

	/* If ALC in operation, make sure calibrated offsets are updated */
	if ((!ret) && (d2153_codec->alc_en))
		d2153_alc_calib(codec);

	return ret;
}

static int d2153_put_alc_sw(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2153_codec_priv *d2153_codec = snd_soc_codec_get_drvdata(codec);

	/* Force ALC offset calibration if enabling ALC */
	if (ucontrol->value.integer.value[0] ||
	    ucontrol->value.integer.value[1]) {
		if (!d2153_codec->alc_en) {
			d2153_alc_calib(codec);
			d2153_codec->alc_en = true;
		}
	} else {
		d2153_codec->alc_en = false;
	}

	return snd_soc_put_volsw(kcontrol, ucontrol);
}

static int d2153_put_alc_ext_mic_enum(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct d2153_codec_priv *d2153_codec = snd_soc_codec_get_drvdata(codec);
	int ret;

	ret = snd_soc_put_enum_double(kcontrol, ucontrol);

	/* If ALC in operation, make sure calibrated offsets are updated */
	if ((!ret) && (d2153_codec->alc_en))
		d2153_alc_calib(codec);

	return ret;
}

/*
 * Control list
 */

static const struct snd_kcontrol_new d2153_snd_controls[] = {
	SOC_SINGLE_TLV("Capture Main Mic Volume", D2153_MIC_R_GAIN, //main
			 D2153_MIC_AMP_GAIN_SHIFT, D2153_MIC_AMP_GAIN_MAX,
			 D2153_NO_INVERT, mic_gain_tlv),
	SOC_SINGLE_TLV("Capture Sub Mic Volume", D2153_MIC_L_GAIN, //sub
			 D2153_MIC_AMP_GAIN_SHIFT, D2153_MIC_AMP_GAIN_MAX,
			 D2153_NO_INVERT, mic_gain_tlv),
	SOC_SINGLE_TLV("Capture Headset Mic Volume", D2153_MIC_EXT_GAIN, //headset
		       D2153_MIC_AMP_GAIN_SHIFT, D2153_MIC_AMP_GAIN_MAX,
		       D2153_NO_INVERT, mic_gain_tlv),
	SOC_DOUBLE_R_TLV("Aux Volume", D2153_AUX_L_GAIN, D2153_AUX_R_GAIN,
			 D2153_AUX_AMP_GAIN_SHIFT, D2153_AUX_AMP_GAIN_MAX,
			 D2153_NO_INVERT, aux_gain_tlv),
	SOC_DOUBLE_R_EXT_TLV("In Mixer PGA Volume", D2153_MIXIN_L_GAIN,
			     D2153_MIXIN_R_GAIN, D2153_MIXIN_AMP_GAIN_SHIFT,
			     D2153_MIXIN_AMP_GAIN_MAX, D2153_NO_INVERT,
			     snd_soc_get_volsw_2r, d2153_put_mixin_gain,
			     mixin_gain_tlv),
	SOC_DOUBLE_R_TLV("ADC Volume", D2153_ADC_L_GAIN, D2153_ADC_R_GAIN,
			 D2153_ADC_DIGITAL_GAIN_SHIFT,
			 D2153_ADC_DIGITAL_GAIN_MAX,
			 D2153_NO_INVERT, digital_gain_tlv),
	SOC_DOUBLE_R_TLV("DAC Volume", D2153_DAC_L_GAIN, D2153_DAC_R_GAIN,
			 D2153_DAC_DIGITAL_GAIN_SHIFT,
			 D2153_DAC_DIGITAL_GAIN_MAX,
			 D2153_NO_INVERT, digital_gain_tlv),
	SOC_DOUBLE_R_TLV("Playback Headphone Volume", D2153_HP_L_GAIN, D2153_HP_R_GAIN,
			 D2153_HP_AMP_GAIN_SHIFT, D2153_HP_AMP_GAIN_MAX,
			 D2153_NO_INVERT, hp_gain_tlv),
	SOC_SINGLE_TLV("Playback Earpiece Volume", D2153_EP_GAIN,
		       D2153_EP_AMP_GAIN_SHIFT, D2153_EP_AMP_GAIN_MAX,
		       D2153_NO_INVERT, ep_gain_tlv),
	SOC_SINGLE_TLV("Playback Speaker Volume", D2153_SP_GAIN,
		       D2153_SP_AMP_GAIN_SHIFT, D2153_SP_AMP_GAIN_MAX,
		       D2153_NO_INVERT, sp_gain_tlv),
	/* ADC HPF Controls */
	SOC_ENUM("ADC Voice Cutoff", d2153_adc_vf_cutoff),
	SOC_SINGLE("ADC Voice Mode Switch", D2153_ADC_FILTERS1,
		   D2153_VOICE_EN_SHIFT, D2153_VOICE_EN_MAX, D2153_NO_INVERT),
	SOC_ENUM("ADC HPF Cutoff", d2153_adc_hpf_cutoff),
	SOC_SINGLE("ADC HPF Switch", D2153_ADC_FILTERS1,
		   D2153_HPF_EN_SHIFT, D2153_HPF_EN_MAX, D2153_NO_INVERT),

	/* ADC Equalizer controls */
	SOC_SINGLE_TLV("ADC EQ1 Volume", D2153_ADC_FILTERS2,
		       D2153_EQ_BAND1_SHIFT, D2153_EQ_BAND1_MAX,
		       D2153_NO_INVERT, eq_gain_tlv),
	SOC_SINGLE_TLV("ADC EQ2 Volume", D2153_ADC_FILTERS2,
		       D2153_EQ_BAND2_SHIFT, D2153_EQ_BAND2_MAX,
		       D2153_NO_INVERT, eq_gain_tlv),
	SOC_SINGLE_TLV("ADC EQ3 Volume", D2153_ADC_FILTERS3,
		       D2153_EQ_BAND3_SHIFT, D2153_EQ_BAND3_MAX,
		       D2153_NO_INVERT, eq_gain_tlv),
	SOC_SINGLE_TLV("ADC EQ4 Volume", D2153_ADC_FILTERS3,
		       D2153_EQ_BAND4_SHIFT, D2153_EQ_BAND4_MAX,
		       D2153_NO_INVERT, eq_gain_tlv),
	SOC_SINGLE_TLV("ADC EQ5 Volume", D2153_ADC_FILTERS4,
		       D2153_EQ_BAND5_SHIFT, D2153_EQ_BAND5_MAX,
		       D2153_NO_INVERT, eq_gain_tlv),
	SOC_SINGLE_TLV("ADC EQ Overall Volume", D2153_ADC_FILTERS4,
		       D2153_ADC_EQ_GAIN_SHIFT, D2153_ADC_EQ_GAIN_MAX,
		       D2153_INVERT, adc_eq_overall_gain_tlv),
	SOC_SINGLE("ADC EQ Switch", D2153_ADC_FILTERS4, D2153_EQ_EN_SHIFT,
		   D2153_EQ_EN_MAX, D2153_NO_INVERT),

	/* DAC HPF Controls */
	SOC_ENUM("DAC Voice Cutoff", d2153_dac_vf_cutoff),
	SOC_SINGLE("DAC Voice Mode Switch", D2153_DAC_FILTERS1,
		   D2153_VOICE_EN_SHIFT, D2153_VOICE_EN_MAX, D2153_NO_INVERT),
	SOC_ENUM("DAC HPF Cutoff", d2153_dac_hpf_cutoff),
	SOC_SINGLE("DAC HPF Switch", D2153_DAC_FILTERS1,
		   D2153_HPF_EN_SHIFT, D2153_HPF_EN_MAX, D2153_NO_INVERT),

	/* DAC Equalizer controls */
	SOC_SINGLE_TLV("DAC EQ1 Volume", D2153_DAC_FILTERS2,
		       D2153_EQ_BAND1_SHIFT, D2153_EQ_BAND1_MAX,
		       D2153_NO_INVERT, eq_gain_tlv),
	SOC_SINGLE_TLV("DAC EQ2 Volume", D2153_DAC_FILTERS2,
		       D2153_EQ_BAND2_SHIFT, D2153_EQ_BAND2_MAX,
		       D2153_NO_INVERT, eq_gain_tlv),
	SOC_SINGLE_TLV("DAC EQ3 Volume", D2153_DAC_FILTERS3,
		       D2153_EQ_BAND3_SHIFT, D2153_EQ_BAND3_MAX,
		       D2153_NO_INVERT, eq_gain_tlv),
	SOC_SINGLE_TLV("DAC EQ4 Volume", D2153_DAC_FILTERS3,
		       D2153_EQ_BAND4_SHIFT, D2153_EQ_BAND4_MAX,
		       D2153_NO_INVERT, eq_gain_tlv),
	SOC_SINGLE_TLV("DAC EQ5 Volume", D2153_DAC_FILTERS4,
		       D2153_EQ_BAND5_SHIFT, D2153_EQ_BAND5_MAX,
		       D2153_NO_INVERT, eq_gain_tlv),
	SOC_SINGLE("DAC EQ Switch", D2153_DAC_FILTERS4, D2153_EQ_EN_SHIFT,
		   D2153_EQ_EN_MAX, D2153_NO_INVERT),

	/* Mute controls */
	SOC_DOUBLE_R("Aux Switch", D2153_AUX_L_CTRL, D2153_AUX_R_CTRL,
		     D2153_AUX_AMP_MUTE_EN_SHIFT, D2153_AUX_AMP_MUTE_EN_MAX,
		     D2153_INVERT),
	SOC_DOUBLE_R("Mic Switch", D2153_MIC_L_CTRL, D2153_MIC_R_CTRL,
		     D2153_MIC_AMP_MUTE_EN_SHIFT, D2153_MIC_AMP_MUTE_EN_MAX,
		     D2153_INVERT),
	SOC_SINGLE("Mic Ext Switch", D2153_MIC_EXT_CTRL,
		   D2153_MIC_AMP_MUTE_EN_SHIFT, D2153_MIC_AMP_MUTE_EN_MAX,
		   D2153_INVERT),
	SOC_DOUBLE_R("In Mixer PGA Switch", D2153_MIXIN_L_CTRL,
		     D2153_MIXIN_R_CTRL, D2153_MIXIN_AMP_MUTE_EN_SHIFT,
		     D2153_MIXIN_AMP_MUTE_EN_MAX, D2153_INVERT),
	SOC_DOUBLE_R("ADC Switch", D2153_ADC_L_CTRL, D2153_ADC_R_CTRL,
		     D2153_ADC_MUTE_EN_SHIFT, D2153_ADC_MUTE_EN_MAX,
		     D2153_INVERT),
	SOC_DOUBLE_R("HP Switch", D2153_HP_L_CTRL, D2153_HP_R_CTRL,
		     D2153_HP_AMP_MUTE_EN_SHIFT, D2153_HP_AMP_MUTE_EN_MAX,
		     D2153_INVERT),
	SOC_SINGLE("EP Switch", D2153_EP_CTRL,
		   D2153_EP_AMP_MUTE_EN_SHIFT, D2153_EP_AMP_MUTE_EN_MAX,
		   D2153_INVERT),
	SOC_SINGLE("SP Switch", D2153_SP_CTRL,
		   D2153_SP_AMP_MUTE_EN_SHIFT, D2153_SP_AMP_MUTE_EN_MAX,
		   D2153_INVERT),
	SOC_ENUM("DAC Soft Mute Rate", d2153_dac_soft_mute_rate),
	SOC_SINGLE("DAC Soft Mute Switch", D2153_DAC_FILTERS5,
		   D2153_DAC_SOFTMUTE_EN_SHIFT, D2153_DAC_SOFTMUTE_EN_MAX,
		   D2153_NO_INVERT),

	/* Zero cross controls */
	SOC_DOUBLE_R("Aux ZC Switch", D2153_AUX_L_CTRL, D2153_AUX_R_CTRL,
		     D2153_AUX_AMP_ZC_EN_SHIFT, D2153_AUX_AMP_ZC_EN_MAX,
		     D2153_NO_INVERT),
	SOC_DOUBLE_R("In Mixer PGA ZC Switch", D2153_MIXIN_L_CTRL,
		     D2153_MIXIN_R_CTRL, D2153_MIXIN_AMP_ZC_EN_SHIFT,
		     D2153_MIXIN_AMP_ZC_EN_MAX, D2153_NO_INVERT),
	SOC_DOUBLE_R("Headphone ZC Switch", D2153_HP_L_CTRL, D2153_HP_R_CTRL,
		     D2153_HP_AMP_ZC_EN_SHIFT, D2153_HP_AMP_ZC_EN_MAX,
		     D2153_NO_INVERT),
	SOC_SINGLE("Earpiece ZC Switch", D2153_EP_CTRL,
		   D2153_EP_AMP_ZC_EN_SHIFT, D2153_EP_AMP_ZC_EN_MAX,
		   D2153_NO_INVERT),
	SOC_SINGLE("Speaker ZC Switch", D2153_SP_CTRL,
		   D2153_SP_AMP_ZC_EN_SHIFT, D2153_SP_AMP_ZC_EN_MAX,
		   D2153_NO_INVERT),

	/* Gain Ramping controls */
	SOC_DOUBLE_R("Aux Gain Ramping Switch", D2153_AUX_L_CTRL,
		     D2153_AUX_R_CTRL, D2153_AUX_AMP_RAMP_EN_SHIFT,
		     D2153_AUX_AMP_RAMP_EN_MAX, D2153_NO_INVERT),
	SOC_DOUBLE_R("In Mixer Gain Ramping Switch", D2153_MIXIN_L_CTRL,
		     D2153_MIXIN_R_CTRL, D2153_MIXIN_AMP_RAMP_EN_SHIFT,
		     D2153_MIXIN_AMP_RAMP_EN_MAX, D2153_NO_INVERT),
	SOC_DOUBLE_R("ADC Gain Ramping Switch", D2153_ADC_L_CTRL,
		     D2153_ADC_R_CTRL, D2153_ADC_RAMP_EN_SHIFT,
		     D2153_ADC_RAMP_EN_MAX, D2153_NO_INVERT),
	SOC_DOUBLE_R("DAC Gain Ramping Switch", D2153_DAC_L_CTRL,
		     D2153_DAC_R_CTRL, D2153_DAC_RAMP_EN_SHIFT,
		     D2153_DAC_RAMP_EN_MAX, D2153_NO_INVERT),
	SOC_DOUBLE_R("Headphone Gain Ramping Switch", D2153_HP_L_CTRL,
		     D2153_HP_R_CTRL, D2153_HP_AMP_RAMP_EN_SHIFT,
		     D2153_HP_AMP_RAMP_EN_MAX, D2153_NO_INVERT),
	SOC_SINGLE("Earpiece Gain Ramping Switch", D2153_EP_CTRL,
		   D2153_EP_AMP_RAMP_EN_SHIFT, D2153_EP_AMP_RAMP_EN_MAX,
		   D2153_NO_INVERT),
	SOC_SINGLE("Speaker Gain Ramping Switch", D2153_SP_CTRL,
		   D2153_SP_AMP_RAMP_EN_SHIFT, D2153_SP_AMP_RAMP_EN_MAX,
		   D2153_NO_INVERT),
	SOC_ENUM("Gain Ramping Rate", d2153_gain_ramping_rate),

	/* DAC Noise Gate controls */
	SOC_SINGLE("DAC NG Switch", D2153_DAC_NG_CTRL, D2153_DAC_NG_EN_SHIFT,
		   D2153_DAC_NG_EN_MAX, D2153_NO_INVERT),
	SOC_ENUM("DAC NG Setup Time", d2153_dac_ng_setup_time),
	SOC_ENUM("DAC NG Rampup Rate", d2153_dac_ng_rampup_rate),
	SOC_ENUM("DAC NG Rampdown Rate", d2153_dac_ng_rampdown_rate),
	SOC_SINGLE_TLV("DAC NG OFF Threshold", D2153_DAC_NG_OFF_THRESHOLD,
		       D2153_DAC_NG_THRESHOLD_SHIFT, D2153_DAC_NG_THRESHOLD_MAX,
		       D2153_NO_INVERT, dac_ng_threshold_tlv),
	SOC_SINGLE_TLV("DAC NG ON Threshold", D2153_DAC_NG_ON_THRESHOLD,
		       D2153_DAC_NG_THRESHOLD_SHIFT, D2153_DAC_NG_THRESHOLD_MAX,
		       D2153_NO_INVERT, dac_ng_threshold_tlv),

	/* DAC Mono control */
	SOC_DOUBLE("DAC Mono Switch", D2153_DIG_ROUTING_DAC,
		   D2153_DAC_L_MONO_SHIFT, D2153_DAC_R_MONO_SHIFT,
		   D2153_DAC_MONO_MAX, D2153_NO_INVERT),

	/* DAC Inversion control */
	SOC_DOUBLE("DAC Invert Switch", D2153_DIG_CTRL, D2153_DAC_L_INV_SHIFT,
		   D2153_DAC_R_INV_SHIFT, D2153_DAC_INV_MAX, D2153_NO_INVERT),

	/* DMIC controls */
	SOC_DOUBLE_R("DMIC Switch", D2153_MIXIN_L_SELECT, D2153_MIXIN_R_SELECT,
		     D2153_DMIC_EN_SHIFT, D2153_DMIC_EN_MAX, D2153_NO_INVERT),

	/* ALC Controls */
	SOC_DOUBLE_EXT("ALC Switch", D2153_ALC_CTRL1, D2153_ALC_L_EN_SHIFT,
		       D2153_ALC_R_EN_SHIFT, D2153_ALC_EN_MAX, D2153_NO_INVERT,
		       snd_soc_get_volsw, d2153_put_alc_sw),
	SOC_ENUM_EXT("ALC Ext Mic Mode", d2153_alc_ext_mic_mode,
		     snd_soc_get_enum_double, d2153_put_alc_ext_mic_enum),
	SOC_ENUM("ALC Attack Rate", d2153_alc_attack_rate),
	SOC_ENUM("ALC Release Rate", d2153_alc_release_rate),
	SOC_ENUM("ALC Hold Time", d2153_alc_hold_time),
	/*
	 * Rate at which input signal envelope is tracked as the signal gets
	 * larger
	 */
	SOC_ENUM("ALC Integ Attack Rate", d2153_alc_integ_attack_rate),
	/*
	 * Rate at which input signal envelope is tracked as the signal gets
	 * smaller
	 */
	SOC_ENUM("ALC Integ Release Rate", d2153_alc_integ_release_rate),
	SOC_SINGLE_TLV("ALC Noise Threshold Volume", D2153_ALC_NOISE,
		       D2153_ALC_NOISE_SHIFT, D2153_ALC_NOISE_MAX,
		       D2153_INVERT, alc_threshold_tlv),
	SOC_SINGLE_TLV("ALC Min Threshold Volume", D2153_ALC_TARGET_MIN,
		       D2153_ALC_THRESHOLD_MIN_SHIFT,
		       D2153_ALC_THRESHOLD_MIN_MAX,
		       D2153_INVERT, alc_threshold_tlv),
	SOC_SINGLE_TLV("ALC Max Threshold Volume", D2153_ALC_TARGET_MAX,
		       D2153_ALC_THRESHOLD_MAX_SHIFT,
		       D2153_ALC_THRESHOLD_MAX_MAX,
		       D2153_INVERT, alc_threshold_tlv),
	SOC_SINGLE_TLV("ALC Max Attenuation Volume", D2153_ALC_GAIN_LIMITS,
		       D2153_ALC_ATTEN_MAX_SHIFT, D2153_ALC_ATTEN_MAX_MAX,
		       D2153_NO_INVERT, alc_gain_tlv),
	SOC_SINGLE_TLV("ALC Max Gain Volume", D2153_ALC_GAIN_LIMITS,
		       D2153_ALC_GAIN_MAX_SHIFT, D2153_ALC_GAIN_MAX_MAX,
		       D2153_NO_INVERT, alc_gain_tlv),
	SOC_SINGLE_TLV("ALC Min Analog Gain Volume", D2153_ALC_ANA_GAIN_LIMITS,
		       D2153_ALC_ANA_GAIN_MIN_SHIFT, D2153_ALC_ANA_GAIN_MIN_MAX,
		       D2153_NO_INVERT, alc_analog_gain_tlv),
	SOC_SINGLE_TLV("ALC Max Analog Gain Volume", D2153_ALC_ANA_GAIN_LIMITS,
		       D2153_ALC_ANA_GAIN_MAX_SHIFT, D2153_ALC_ANA_GAIN_MAX_MAX,
		       D2153_NO_INVERT, alc_analog_gain_tlv),
	SOC_SINGLE("ALC Anticlip Mode Switch", D2153_ALC_ANTICLIP_CTRL,
		   D2153_ALC_ANTICLIP_EN_SHIFT, D2153_ALC_ANTICLIP_EN_MAX,
		   D2153_NO_INVERT),
	SOC_SINGLE("ALC Anticlip Level", D2153_ALC_ANTICLIP_LEVEL,
		   D2153_ALC_ANTICLIP_LEVEL_SHIFT, D2153_ALC_ANTICLIP_LEVEL_MAX,
		   D2153_NO_INVERT),

	/* Limiter controls */
	SOC_ENUM("Limiter Power/THD Attack Rate", d2153_sp_pwr_thd_atk_rate),
	SOC_ENUM("Limiter Power/THD Release Rate", d2153_sp_pwr_thd_rel_rate),
	SOC_ENUM("Limiter Power/THD Hold Time", d2153_sp_pwr_thd_hold_time),
	SOC_SINGLE("Limiter Power Switch", D2153_LIMITER_CTRL2,
		   D2153_SP_PWR_EN_SHIFT, D2153_SP_PWR_EN_MAX, D2153_NO_INVERT),
	SOC_SINGLE("Limiter THD Switch", D2153_LIMITER_CTRL2,
		   D2153_SP_THD_EN_SHIFT, D2153_SP_THD_EN_MAX, D2153_NO_INVERT),
	SOC_SINGLE("Limiter Power Hysteresis Switch", D2153_LIMITER_CTRL2,
		   D2153_SP_PWR_HYS_DIS_SHIFT, D2153_SP_PWR_HYS_DIS_MAX,
		   D2153_INVERT),
	SOC_SINGLE("Limiter THD Hysteresis Switch", D2153_LIMITER_CTRL2,
		   D2153_SP_THD_HYS_DIS_SHIFT, D2153_SP_THD_HYS_DIS_MAX,
		   D2153_INVERT),
	/*
	 * DLG - Following 2 controls are describing 0 - 20% clipping but
	 * there's no easy way to make this obvious in user space. Could use
	 * an enum but that would require 64 text entries.
	 */
	SOC_SINGLE_TLV("Limiter Power Limit", D2153_LIMITER_PWR_LIM,
		   D2153_SP_PWR_LIM_SHIFT, D2153_SP_PWR_LIM_MAX,
		   D2153_NO_INVERT, sp_thd_lim_tlv),
	SOC_SINGLE_TLV("Limiter THD Limit", D2153_LIMITER_THD_LIM,
		   D2153_SP_THD_LIM_SHIFT, D2153_SP_THD_LIM_MAX,
		   D2153_NO_INVERT, sp_thd_lim_tlv),
	SOC_SINGLE_TLV("Limiter NG Attenuation", D2153_NG_CTRL1,
		       D2153_SP_NG_ATT_SHIFT, D2153_SP_NG_ATT_MAX,
		       D2153_NO_INVERT, sp_ng_att_tlv),
	SOC_SINGLE("Limiter NG Switch", D2153_NG_CTRL1, D2153_SP_NG_EN_SHIFT,
		   D2153_SP_NG_EN_MAX, D2153_NO_INVERT),
	SOC_ENUM("Limiter NG Attack Rate", d2153_sp_atk_rate),
	SOC_ENUM("Limiter NG Release Rate", d2153_sp_rel_rate),
	SOC_ENUM("Limiter NG Hold Time", d2153_sp_hld_time),
};

/*
 * DAPM section
 */

/*
 * Enums
 */

/* AIF routing select */
static const char * const d2153_aif_src_txt[] = {
	"ADC Left", "ADC Right", "AIF Input Left", "AIF Input Right"
};

static const struct soc_enum d2153_aif_l_src =
	SOC_ENUM_SINGLE(D2153_DIG_ROUTING_AIF, D2153_AIF_L_SRC_SHIFT,
			D2153_AIF_L_SRC_MAX, d2153_aif_src_txt);

static const struct snd_kcontrol_new d2153_aif_l_src_mux =
	SOC_DAPM_ENUM("AIF Left Source MUX", d2153_aif_l_src);

static const struct soc_enum d2153_aif_r_src =
	SOC_ENUM_SINGLE(D2153_DIG_ROUTING_AIF, D2153_AIF_R_SRC_SHIFT,
			D2153_AIF_R_SRC_MAX, d2153_aif_src_txt);

static const struct snd_kcontrol_new d2153_aif_r_src_mux =
	SOC_DAPM_ENUM("AIF Right Source MUX", d2153_aif_r_src);

/* DAC routing select */
static const char * const d2153_dac_src_txt[] = {
	"ADC Left Output", "ADC Right Output", "AIF Input Left",
	"AIF Input Right"
};

static const struct soc_enum d2153_dac_l_src =
	SOC_ENUM_SINGLE(D2153_DIG_ROUTING_DAC, D2153_DAC_L_SRC_SHIFT,
			D2153_DAC_L_SRC_MAX, d2153_dac_src_txt);

static const struct snd_kcontrol_new d2153_dac_l_src_mux =
	SOC_DAPM_ENUM("DAC Left Source MUX", d2153_dac_l_src);

static const struct soc_enum d2153_dac_r_src =
	SOC_ENUM_SINGLE(D2153_DIG_ROUTING_DAC, D2153_DAC_R_SRC_SHIFT,
			D2153_DAC_R_SRC_MAX, d2153_dac_src_txt);

static const struct snd_kcontrol_new d2153_dac_r_src_mux =
	SOC_DAPM_ENUM("DAC Right Source MUX", d2153_dac_r_src);

/* MIC Left/Right/Ext Amp routing select */
static const char * const d2153_mic_amp_in_sel_txt[] = {
	"Differential", "MIC_P", "MIC_N"
};

/*
 * DLG - Have to map these to MIXIN_L/R_SELECT for DA9055 as the bit fields have
 * moved register in D2153. Unfortunately it requires a conditional compilation.
 */
#ifdef CONFIG_SND_SOC_USE_DA9055_HW
static const struct soc_enum d2153_mic_l_amp_in_sel =
	SOC_ENUM_SINGLE(D2153_MIXIN_L_SELECT, D2153_MIC_AMP_IN_SEL_SHIFT,
			D2153_MIC_AMP_IN_SEL_MAX, d2153_mic_amp_in_sel_txt);
static const struct snd_kcontrol_new d2153_mic_l_amp_in_sel_mux =
	SOC_DAPM_ENUM("Mic Left Amp Source MUX", d2153_mic_l_amp_in_sel);

static const struct soc_enum d2153_mic_r_amp_in_sel =
	SOC_ENUM_SINGLE(D2153_MIXIN_R_SELECT, D2153_MIC_AMP_IN_SEL_SHIFT,
			D2153_MIC_AMP_IN_SEL_MAX, d2153_mic_amp_in_sel_txt);
static const struct snd_kcontrol_new d2153_mic_r_amp_in_sel_mux =
	SOC_DAPM_ENUM("Mic Right Amp Source MUX", d2153_mic_r_amp_in_sel);

static const struct soc_enum d2153_mic_ext_amp_in_sel =
	SOC_ENUM_SINGLE(D2153_MIXIN_R_SELECT, D2153_MIC_AMP_IN_SEL_SHIFT,
			D2153_MIC_AMP_IN_SEL_MAX, d2153_mic_amp_in_sel_txt);
static const struct snd_kcontrol_new d2153_mic_ext_amp_in_sel_mux =
	SOC_DAPM_ENUM("Mic Ext Amp Source MUX", d2153_mic_ext_amp_in_sel);
#else
static const struct soc_enum d2153_mic_l_amp_in_sel =
	SOC_ENUM_SINGLE(D2153_MIC_L_CTRL, D2153_MIC_AMP_IN_SEL_SHIFT,
			D2153_MIC_AMP_IN_SEL_MAX, d2153_mic_amp_in_sel_txt);
static const struct snd_kcontrol_new d2153_mic_l_amp_in_sel_mux =
	SOC_DAPM_ENUM("Mic Left Amp Source MUX", d2153_mic_l_amp_in_sel);

static const struct soc_enum d2153_mic_r_amp_in_sel =
	SOC_ENUM_SINGLE(D2153_MIC_R_CTRL, D2153_MIC_AMP_IN_SEL_SHIFT,
			D2153_MIC_AMP_IN_SEL_MAX, d2153_mic_amp_in_sel_txt);
static const struct snd_kcontrol_new d2153_mic_r_amp_in_sel_mux =
	SOC_DAPM_ENUM("Mic Right Amp Source MUX", d2153_mic_r_amp_in_sel);

static const struct soc_enum d2153_mic_ext_amp_in_sel =
	SOC_ENUM_SINGLE(D2153_MIC_EXT_CTRL, D2153_MIC_AMP_IN_SEL_SHIFT,
			D2153_MIC_AMP_IN_SEL_MAX, d2153_mic_amp_in_sel_txt);
static const struct snd_kcontrol_new d2153_mic_ext_amp_in_sel_mux =
	SOC_DAPM_ENUM("Mic Ext Amp Source MUX", d2153_mic_ext_amp_in_sel);
#endif /* CONFIG_SND_SOC_USE_DA9055_HW */

/*
 * Mixer controls
 */

#define SOC_DAPM_SINGLE_D(xname, reg, shift, max, invert) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_volsw, \
	.get = snd_soc_dapm_get_volsw, \
	.put = d2153_snd_soc_dapm_put_volsw, \
	.private_value =  SOC_SINGLE_VALUE(reg, shift, max, invert) }

/* In Mixer Left */
static const struct snd_kcontrol_new d2153_dapm_mixinl_controls[] = {
	SOC_DAPM_SINGLE("Aux Left Switch", D2153_MIXIN_L_SELECT,
			D2153_MIXIN_L_MIX_SELECT_AUX_L_SHIFT,
			D2153_MIXIN_L_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mic Left Switch", D2153_MIXIN_L_SELECT,
			D2153_MIXIN_L_MIX_SELECT_MIC_L_SHIFT,
			D2153_MIXIN_L_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mic Right Switch", D2153_MIXIN_L_SELECT,
			D2153_MIXIN_L_MIX_SELECT_MIC_R_SHIFT,
			D2153_MIXIN_L_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mic Ext Switch", D2153_MIXIN_L_SELECT,
			D2153_MIXIN_L_MIX_SELECT_MIC_EXT_SHIFT,
			D2153_MIXIN_L_MIX_SELECT_MAX, D2153_NO_INVERT),
};

/* In Mixer Right */
static const struct snd_kcontrol_new d2153_dapm_mixinr_controls[] = {
	SOC_DAPM_SINGLE("Aux Right Switch", D2153_MIXIN_R_SELECT,
			D2153_MIXIN_R_MIX_SELECT_AUX_R_SHIFT,
			D2153_MIXIN_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mic Right Switch", D2153_MIXIN_R_SELECT,
			D2153_MIXIN_R_MIX_SELECT_MIC_R_SHIFT,
			D2153_MIXIN_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mic Left Switch", D2153_MIXIN_R_SELECT,
			D2153_MIXIN_R_MIX_SELECT_MIC_L_SHIFT,
			D2153_MIXIN_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mic Ext Switch", D2153_MIXIN_R_SELECT,
			D2153_MIXIN_R_MIX_SELECT_MIC_EXT_SHIFT,
			D2153_MIXIN_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Left Switch", D2153_MIXIN_R_SELECT,
			D2153_MIXIN_R_MIX_SELECT_MIXIN_L_SHIFT,
			D2153_MIXIN_R_MIX_SELECT_MAX, D2153_NO_INVERT),
};

/* Out Mixer Left */
static const struct snd_kcontrol_new d2153_dapm_mixoutl_controls[] = {
	SOC_DAPM_SINGLE("Aux Left Switch", D2153_MIXOUT_L_SELECT,
			D2153_MIXOUT_L_MIX_SELECT_AUX_L_SHIFT,
			D2153_MIXOUT_L_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Left Switch", D2153_MIXOUT_L_SELECT,
			D2153_MIXOUT_L_MIX_SELECT_MIXIN_L_SHIFT,
			D2153_MIXOUT_L_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Right Switch", D2153_MIXOUT_L_SELECT,
			D2153_MIXOUT_L_MIX_SELECT_MIXIN_R_SHIFT,
			D2153_MIXOUT_L_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("DAC Left Switch", D2153_MIXOUT_L_SELECT,
			D2153_MIXOUT_L_MIX_SELECT_DAC_L_SHIFT,
			D2153_MIXOUT_L_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Aux Left Invert Switch", D2153_MIXOUT_L_SELECT,
			D2153_MIXOUT_L_MIX_SELECT_AUX_L_INV_SHIFT,
			D2153_MIXOUT_L_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Left Invert Switch", D2153_MIXOUT_L_SELECT,
			D2153_MIXOUT_L_MIX_SELECT_MIXIN_L_INV_SHIFT,
			D2153_MIXOUT_L_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Right Invert Switch", D2153_MIXOUT_L_SELECT,
			D2153_MIXOUT_L_MIX_SELECT_MIXIN_R_INV_SHIFT,
			D2153_MIXOUT_L_MIX_SELECT_MAX, D2153_NO_INVERT),
};

/* Out Mixer Right */
static const struct snd_kcontrol_new d2153_dapm_mixoutr_controls[] = {
	SOC_DAPM_SINGLE("Aux Right Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_AUX_R_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Right Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_MIXIN_R_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Left Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_MIXIN_L_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE_D("DAC Right Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_DAC_R_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Aux Right Invert Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_AUX_R_INV_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Right Invert Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_MIXIN_R_INV_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Left Invert Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_MIXIN_L_INV_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
};

/* Out Mixer Earpiece */
static const struct snd_kcontrol_new d2153_dapm_mixoutep_controls[] = {
	SOC_DAPM_SINGLE("Aux Right Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_AUX_R_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Right Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_MIXIN_R_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Left Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_MIXIN_L_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE_D("DAC Right Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_DAC_R_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Aux Right Invert Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_AUX_R_INV_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Right Invert Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_MIXIN_R_INV_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Left Invert Switch", D2153_MIXOUT_R_SELECT,
			D2153_MIXOUT_R_MIX_SELECT_MIXIN_L_INV_SHIFT,
			D2153_MIXOUT_R_MIX_SELECT_MAX, D2153_NO_INVERT),
};

/* Out Mixer Speaker */
static const struct snd_kcontrol_new d2153_dapm_mixoutsp_controls[] = {
	SOC_DAPM_SINGLE("Aux Right Switch", D2153_MIXOUT_SP_SELECT,
			D2153_MIXOUT_SP_MIX_SELECT_AUX_R_SHIFT,
			D2153_MIXOUT_SP_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Right Switch", D2153_MIXOUT_SP_SELECT,
			D2153_MIXOUT_SP_MIX_SELECT_MIXIN_R_SHIFT,
			D2153_MIXOUT_SP_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Left Switch", D2153_MIXOUT_SP_SELECT,
			D2153_MIXOUT_SP_MIX_SELECT_MIXIN_L_SHIFT,
			D2153_MIXOUT_SP_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("DAC Right Switch", D2153_MIXOUT_SP_SELECT,
			D2153_MIXOUT_SP_MIX_SELECT_DAC_R_SHIFT,
			D2153_MIXOUT_SP_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Aux Right Invert Switch", D2153_MIXOUT_SP_SELECT,
			D2153_MIXOUT_SP_MIX_SELECT_AUX_R_INV_SHIFT,
			D2153_MIXOUT_SP_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Right Invert Switch", D2153_MIXOUT_SP_SELECT,
			D2153_MIXOUT_SP_MIX_SELECT_MIXIN_R_INV_SHIFT,
			D2153_MIXOUT_SP_MIX_SELECT_MAX, D2153_NO_INVERT),
	SOC_DAPM_SINGLE("Mixin Left Invert Switch", D2153_MIXOUT_SP_SELECT,
			D2153_MIXOUT_SP_MIX_SELECT_MIXIN_L_INV_SHIFT,
			D2153_MIXOUT_SP_MIX_SELECT_MAX, D2153_NO_INVERT),
};

/*
 * DAPM widgets
 */

#ifdef CONFIG_SND_SOC_D2153_AAD
/* HACK to deal with issue of ADC 1/8 bit reads for button detect */
static int d2153_micbias_event(struct snd_soc_dapm_widget *widget,
				   struct snd_kcontrol *kctl, int event)
{
	struct snd_soc_codec *codec = widget->codec;
	struct d2153_codec_priv *d2153_codec = snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = d2153_codec->aad_i2c_client;
	struct d2153_aad_priv *d2153_aad = i2c_get_clientdata(client);

	dlg_info("%s() event = %d  \n",__FUNCTION__,event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (D2153_AA_Silicon == d2153_aad->chip_rev) {
			d2153_aad->button.status = D2153_BUTTON_PRESS;
			d2153_aad_update_bits(d2153_codec->aad_i2c_client,
					D2153_ACCDET_CONFIG,
					D2153_ACCDET_BTN_EN,
					0);
		} else {
			d2153_aad_write(d2153_codec->aad_i2c_client,
					D2153_ACCDET_CFG2, 0x99);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if(d2153_aad->switch_data.state ==D2153_HEADSET)
			snd_soc_update_bits(d2153_aad->d2153_codec->codec,
				D2153_MICBIAS1_CTRL, D2153_MICBIAS_EN,D2153_MICBIAS_EN);
		if (D2153_AA_Silicon == d2153_aad->chip_rev) {
			d2153_aad->button.status = D2153_BUTTON_PRESS;
			d2153_aad_update_bits(d2153_codec->aad_i2c_client, D2153_ACCDET_CONFIG,
						  D2153_ACCDET_BTN_EN, D2153_ACCDET_BTN_EN);
		} else {
			d2153_aad_write(d2153_codec->aad_i2c_client,
					D2153_ACCDET_CFG2, 0x00);
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif /* CONFIG_SND_SOC_D2153_AAD */

static const struct snd_soc_dapm_widget d2153_dapm_widgets[] = {
	/* AIF */
	/* Use a supply here as this controls both input & output AIFs */
	SND_SOC_DAPM_SUPPLY("AIF", D2153_AIF_CTRL, D2153_AIF_EN_SHIFT,
			    D2153_NO_INVERT, NULL, 0),

	/* Input Side */
	/* Inputs */
	SND_SOC_DAPM_INPUT("AUXL"),
	SND_SOC_DAPM_INPUT("AUXR"),
	SND_SOC_DAPM_INPUT("MICL"),
	SND_SOC_DAPM_INPUT("MICR"),
	SND_SOC_DAPM_INPUT("MICEXT"),

	/* Mic Amp Source Selection MUXs */
	SND_SOC_DAPM_MUX("Mic Left Amp Source MUX", SND_SOC_NOPM, 0, 0,
			 &d2153_mic_l_amp_in_sel_mux),
	SND_SOC_DAPM_MUX("Mic Right Amp Source MUX", SND_SOC_NOPM, 0, 0,
			 &d2153_mic_r_amp_in_sel_mux),
	SND_SOC_DAPM_MUX("Mic Ext Amp Source MUX", SND_SOC_NOPM, 0, 0,
			 &d2153_mic_ext_amp_in_sel_mux),

	/* Mic Biases */
#ifdef CONFIG_SND_SOC_D2153_AAD
	/* HACK to deal with issue of ADC 1/8 bit reads for button detect */
	SND_SOC_DAPM_SUPPLY("Mic Bias 1", D2153_MICBIAS1_CTRL,
				D2153_MICBIAS_EN_SHIFT, D2153_NO_INVERT,
				d2153_micbias_event, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
#else
	SND_SOC_DAPM_SUPPLY("Mic Bias 1", D2153_MICBIAS1_CTRL,
			    D2153_MICBIAS_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
#endif /* CONFIG_SND_SOC_D2153_AAD */
	SND_SOC_DAPM_SUPPLY("Mic Bias 2", D2153_MICBIAS2_CTRL,
			    D2153_MICBIAS_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Mic Bias 3", D2153_MICBIAS3_CTRL,
			    D2153_MICBIAS_EN_SHIFT, D2153_NO_INVERT, NULL, 0),

	/* Input PGAs */
	SND_SOC_DAPM_PGA("Aux Left PGA", D2153_AUX_L_CTRL,
			 D2153_AUX_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Aux Right PGA", D2153_AUX_R_CTRL,
			 D2153_AUX_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Mic Left PGA", D2153_MIC_L_CTRL,
			 D2153_MIC_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Mic Right PGA", D2153_MIC_R_CTRL,
			 D2153_MIC_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Mic Ext PGA", D2153_MIC_EXT_CTRL,
			 D2153_MIC_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("In Mixer Left PGA", D2153_MIXIN_L_CTRL,
			 D2153_MIXIN_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("In Mixer Right PGA", D2153_MIXIN_R_CTRL,
			 D2153_MIXIN_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),

	/* Input Mixers */
	SND_SOC_DAPM_MIXER("In Mixer Left", SND_SOC_NOPM, 0, 0,
			   d2153_dapm_mixinl_controls,
			   ARRAY_SIZE(d2153_dapm_mixinl_controls)),

	SND_SOC_DAPM_MIXER("In Mixer Right", SND_SOC_NOPM, 0, 0,
			   d2153_dapm_mixinr_controls,
			   ARRAY_SIZE(d2153_dapm_mixinr_controls)),

	/* ADCs */
	SND_SOC_DAPM_ADC("ADC Left", NULL, D2153_ADC_L_CTRL,
			 D2153_ADC_EN_SHIFT, D2153_NO_INVERT),
	SND_SOC_DAPM_ADC("ADC Right", NULL, D2153_ADC_R_CTRL,
			 D2153_ADC_EN_SHIFT, D2153_NO_INVERT),

	/* AIF */
	SND_SOC_DAPM_MUX("AIF Left Source MUX", SND_SOC_NOPM, 0, 0,
			 &d2153_aif_l_src_mux),
	SND_SOC_DAPM_MUX("AIF Right Source MUX", SND_SOC_NOPM, 0, 0,
			 &d2153_aif_r_src_mux),
	SND_SOC_DAPM_AIF_OUT("AIFOUTL", "Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIFOUTR", "Capture", 1, SND_SOC_NOPM, 0, 0),

	/* Output Side */
	/* AIF */
	SND_SOC_DAPM_AIF_IN("AIFINL", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("AIFINR", "Playback", 1, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_MUX("DAC Left Source MUX", SND_SOC_NOPM, 0, 0,
			 &d2153_dac_l_src_mux),
	SND_SOC_DAPM_MUX("DAC Right Source MUX", SND_SOC_NOPM, 0, 0,
			 &d2153_dac_r_src_mux),

	/* DACs */
	SND_SOC_DAPM_DAC("DAC Left", NULL, D2153_DAC_L_CTRL,
			 D2153_DAC_EN_SHIFT, D2153_NO_INVERT),
	SND_SOC_DAPM_DAC("DAC Right", NULL, D2153_DAC_R_CTRL,
			 D2153_DAC_EN_SHIFT, D2153_NO_INVERT),

	/* Output Mixers */
	SND_SOC_DAPM_MIXER("Out Mixer Left", SND_SOC_NOPM, 0, 0,
			   d2153_dapm_mixoutl_controls,
			   ARRAY_SIZE(d2153_dapm_mixoutl_controls)),
	SND_SOC_DAPM_MIXER("Out Mixer Right", SND_SOC_NOPM, 0, 0,
			   d2153_dapm_mixoutr_controls,
			   ARRAY_SIZE(d2153_dapm_mixoutr_controls)),
	SND_SOC_DAPM_MIXER("Out Mixer Earpiece", SND_SOC_NOPM, 0, 0,
				   d2153_dapm_mixoutep_controls,
				   ARRAY_SIZE(d2153_dapm_mixoutep_controls)),
	SND_SOC_DAPM_MIXER("Out Mixer Speaker", SND_SOC_NOPM, 0, 0,
			   d2153_dapm_mixoutsp_controls,
			   ARRAY_SIZE(d2153_dapm_mixoutsp_controls)),

	/* Output PGAs */
	SND_SOC_DAPM_PGA("Out Mixer Left PGA", D2153_MIXOUT_L_CTRL,
			 D2153_MIXOUT_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Out Mixer Right PGA", D2153_MIXOUT_R_CTRL,
			 D2153_MIXOUT_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Out Mixer Earpiece PGA", D2153_MIXOUT_R_CTRL,
			 D2153_MIXOUT_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Out Mixer Speaker PGA", D2153_MIXOUT_SP_CTRL,
			 D2153_MIXOUT_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Headphone Left PGA", D2153_HP_L_CTRL,
			 D2153_HP_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Headphone Right PGA", D2153_HP_R_CTRL,
			 D2153_HP_AMP_EN_SHIFT, D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Earpiece PGA", D2153_EP_CTRL, D2153_EP_AMP_EN_SHIFT,
			 D2153_NO_INVERT, NULL, 0),
	SND_SOC_DAPM_PGA("Speaker PGA", D2153_SP_CTRL, D2153_SP_AMP_EN_SHIFT,
			 D2153_NO_INVERT, NULL, 0),

	/* Charge Pump */
	SND_SOC_DAPM_SUPPLY("Charge Pump", D2153_CP_CTRL, D2153_CP_EN_SHIFT,
			    D2153_NO_INVERT, NULL, 0),

	/* Outputs */
	SND_SOC_DAPM_OUTPUT("HPL"),
	SND_SOC_DAPM_OUTPUT("HPR"),
	SND_SOC_DAPM_OUTPUT("EP"),
	SND_SOC_DAPM_OUTPUT("SP"),
};

/*
 * DAPM audio route definition
 */

static const struct snd_soc_dapm_route d2153_audio_map[] = {
	/* Dest  |  Connecting Widget  |  Source */

	/* Input path */
	{"MICL", NULL, "Mic Bias 2"},
	{"MICR", NULL, "Mic Bias 3"},
	{"MICEXT", NULL, "Mic Bias 1"},

	{"Mic Left Amp Source MUX", "Differential", "MICL"},
	{"Mic Left Amp Source MUX", "MIC_P", "MICL"},
	{"Mic Left Amp Source MUX", "MIC_N", "MICL"},

	{"Mic Right Amp Source MUX", "Differential", "MICR"},
	{"Mic Right Amp Source MUX", "MIC_P", "MICR"},
	{"Mic Right Amp Source MUX", "MIC_N", "MICR"},

	{"Mic Ext Amp Source MUX", "Differential", "MICEXT"},
	{"Mic Ext Amp Source MUX", "MIC_P", "MICEXT"},
	{"Mic Ext Amp Source MUX", "MIC_N", "MICEXT"},

	{"Aux Left PGA", NULL, "AUXL"},
	{"Aux Right PGA", NULL, "AUXR"},
	/*
	 * DLG - for DA9055 the following are the mappings:
	 * MICL   -> MIC1
	 * MICR	  -> MIC2L
	 * MICEXT -> MIC2R
	 *
	 * As D2153 has a PGA for each mic. This means that for use with
	 * DA9055, the mapping of MIC2L to Mic Left PGA is lost.
	 */
	{"Mic Left PGA", NULL, "Mic Left Amp Source MUX"},
	{"Mic Right PGA", NULL, "Mic Right Amp Source MUX"},
	{"Mic Ext PGA", NULL, "Mic Ext Amp Source MUX"},

	{"In Mixer Left", "Aux Left Switch", "Aux Left PGA"},
	{"In Mixer Left", "Mic Left Switch", "Mic Left PGA"},
	{"In Mixer Left", "Mic Right Switch", "Mic Right PGA"},
	{"In Mixer Left", "Mic Ext Switch", "Mic Ext PGA"},

	{"In Mixer Right", "Aux Right Switch", "Aux Right PGA"},
	{"In Mixer Right", "Mic Right Switch", "Mic Right PGA"},
	{"In Mixer Right", "Mic Left Switch", "Mic Left PGA"},
	{"In Mixer Right", "Mic Ext Switch", "Mic Ext PGA"},
	{"In Mixer Right", "Mixin Left Switch", "In Mixer Left PGA"},

	{"In Mixer Left PGA", NULL, "In Mixer Left"},
	{"ADC Left", NULL, "In Mixer Left PGA"},

	{"In Mixer Right PGA", NULL, "In Mixer Right"},
	{"ADC Right", NULL, "In Mixer Right PGA"},

	{"AIF Left Source MUX", "ADC Left", "ADC Left"},
	{"AIF Left Source MUX", "ADC Right", "ADC Right"},
	{"AIF Left Source MUX", "AIF Input Left", "AIFINL"},
	{"AIF Left Source MUX", "AIF Input Right", "AIFINR"},

	{"AIF Right Source MUX", "ADC Left", "ADC Left"},
	{"AIF Right Source MUX", "ADC Right", "ADC Right"},
	{"AIF Right Source MUX", "AIF Input Left", "AIFINL"},
	{"AIF Right Source MUX", "AIF Input Right", "AIFINR"},

	{"AIFOUTL", NULL, "AIF Left Source MUX"},
	{"AIFOUTR", NULL, "AIF Right Source MUX"},

	{"AIFOUTL", NULL, "AIF"},
	{"AIFOUTR", NULL, "AIF"},

	/* Output path */
	{"AIFINL", NULL, "AIF"},
	{"AIFINR", NULL, "AIF"},

	{"DAC Left Source MUX", "ADC Left Output", "ADC Left"},
	{"DAC Left Source MUX", "ADC Right Output", "ADC Right"},
	{"DAC Left Source MUX", "AIF Input Left", "AIFINL"},
	{"DAC Left Source MUX", "AIF Input Right", "AIFINR"},

	{"DAC Right Source MUX", "ADC Left Output", "ADC Left"},
	{"DAC Right Source MUX", "ADC Right Output", "ADC Right"},
	{"DAC Right Source MUX", "AIF Input Left", "AIFINL"},
	{"DAC Right Source MUX", "AIF Input Right", "AIFINR"},

	{"DAC Left", NULL, "DAC Left Source MUX"},
	{"DAC Right", NULL, "DAC Right Source MUX"},

	{"Out Mixer Left", "Aux Left Switch", "Aux Left PGA"},
	{"Out Mixer Left", "Mixin Left Switch", "In Mixer Left PGA"},
	{"Out Mixer Left", "Mixin Right Switch", "In Mixer Right PGA"},
	{"Out Mixer Left", "DAC Left Switch", "DAC Left"},
	{"Out Mixer Left", "Aux Left Invert Switch", "Aux Left PGA"},
	{"Out Mixer Left", "Mixin Left Invert Switch", "In Mixer Left PGA"},
	{"Out Mixer Left", "Mixin Right Invert Switch", "In Mixer Right PGA"},

	{"Out Mixer Right", "Aux Right Switch", "Aux Right PGA"},
	{"Out Mixer Right", "Mixin Right Switch", "In Mixer Right PGA"},
	{"Out Mixer Right", "Mixin Left Switch", "In Mixer Left PGA"},
	{"Out Mixer Right", "DAC Right Switch", "DAC Right"},
	{"Out Mixer Right", "Aux Right Invert Switch", "Aux Right PGA"},
	{"Out Mixer Right", "Mixin Right Invert Switch", "In Mixer Right PGA"},
	{"Out Mixer Right", "Mixin Left Invert Switch", "In Mixer Left PGA"},

	{"Out Mixer Earpiece", "Aux Right Switch", "Aux Right PGA"},
	{"Out Mixer Earpiece", "Mixin Right Switch", "In Mixer Right PGA"},
	{"Out Mixer Earpiece", "Mixin Left Switch", "In Mixer Left PGA"},
	{"Out Mixer Earpiece", "DAC Right Switch", "DAC Right"},
	{"Out Mixer Earpiece", "Aux Right Invert Switch", "Aux Right PGA"},
	{"Out Mixer Earpiece", "Mixin Right Invert Switch", "In Mixer Right PGA"},
	{"Out Mixer Earpiece", "Mixin Left Invert Switch", "In Mixer Left PGA"},

	{"Out Mixer Speaker", "Aux Right Switch", "Aux Right PGA"},
	{"Out Mixer Speaker", "Mixin Right Switch", "In Mixer Right PGA"},
	{"Out Mixer Speaker", "Mixin Left Switch", "In Mixer Left PGA"},
	{"Out Mixer Speaker", "DAC Right Switch", "DAC Right"},
	{"Out Mixer Speaker", "Aux Right Invert Switch", "Aux Right PGA"},
	{"Out Mixer Speaker", "Mixin Right Invert Switch", "In Mixer Right PGA"},
	{"Out Mixer Speaker", "Mixin Left Invert Switch", "In Mixer Left PGA"},

	{"Out Mixer Left PGA", NULL, "Out Mixer Left"},
	{"Out Mixer Right PGA", NULL, "Out Mixer Right"},
	{"Out Mixer Speaker PGA", NULL, "Out Mixer Speaker"},
	{"Out Mixer Earpiece PGA", NULL, "Out Mixer Earpiece"},

	{"Headphone Left PGA", NULL, "Out Mixer Left PGA"},
	{"Headphone Left PGA", NULL, "Charge Pump"},
	{"HPL", NULL, "Headphone Left PGA"},

	{"Headphone Right PGA", NULL, "Out Mixer Right PGA"},
	{"Headphone Right PGA", NULL, "Charge Pump"},
	{"HPR", NULL, "Headphone Right PGA"},

	{"Earpiece PGA", NULL, "Out Mixer Earpiece PGA"},
	{"Earpiece PGA", NULL, "Charge Pump"},
	{"EP", NULL, "Earpiece PGA"},

	{"Speaker PGA", NULL, "Out Mixer Speaker PGA"},
	{"SP", NULL, "Speaker PGA"},
};

#ifdef CONFIG_SND_SOC_USE_DA9055_HW
static const u8 d2153_reg_defaults[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R0  - R7  */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R8  - RF  */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R10 - R17 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R18 - R1F */
	0x00, 0x10, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x0C,	/* R20 - R27 */
	0x01, 0x08, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R28 - R2F */
	0x35, 0x35, 0x00, 0x00, 0x03, 0x03, 0x6F, 0x6F,	/* R30 - R37 */
	0x80, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R38 - R3F */
	0x00, 0x88, 0x88, 0x08, 0x80, 0x6F, 0x6F, 0x61,	/* R40 - R47 */
	0x35, 0x35, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R48 - R4F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R50 - R57 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R58 - R5F */
	0x44, 0x44, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40,	/* R60 - R67 */
	0x40, 0x48, 0x40, 0x41, 0x40, 0x40, 0x10, 0x10,	/* R68 - R6F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R70 - R77 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R78 - R7F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R80 - R87 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R88 - R8F */
	0x80, 0x00, 0x02, 0x00, 0x00, 0x34, 0x95, 0x00,	/* R90 - R97 */
	0x00, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x3F, 0xFF,	/* R98 - R9F */
	0x71, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* RA0 - RA7 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,	/* RA8 - RAF */
	0x00, 0x00, 0x00,				/* RB0 - RB2 */
};
#else
static const u8 d2153_reg_defaults[] = {
	0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R0  - R7  */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R8  - RF  */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R10 - R17 */
	0x00, 0x00, 0x00, 0x00, 0x80, 0x88, 0x88, 0x08,	/* R18 - R1F */
	0x00, 0x00, 0x00, 0x00, 0x80, 0x88, 0x88, 0x08,	/* R20 - R27 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F,	/* R28 - R2F */
	0x3F, 0x00, 0xFF, 0x71, 0x00, 0x00, 0x00, 0x00,	/* R30 - R37 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R38 - R3F */
	0x00, 0x00, 0x00, 0x00, 0x10, 0x32, 0x00, 0x00,	/* R40 - R47 */
	0x08, 0x00, 0x01, 0x00, 0x0A, 0x00, 0x00, 0x20,	/* R48 - R4F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R50 - R57 */
	0x00, 0x00, 0x00, 0x00, 0x61, 0x95, 0x00, 0x32,	/* R58 - R5F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R60 - R67 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x48,	/* R68 - R6F */
	0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R70 - R77 */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R78 - R7F */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* R80 - R87 */
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,	/* R88 - R8F */
	0x44, 0x35, 0x00, 0x44, 0x35, 0x00, 0x00, 0x00,	/* R90 - R97 */
	0x40, 0x01, 0x01, 0x40, 0x01, 0x01, 0x40, 0x01,	/* R98 - R9F */
	0x01, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00,	/* RA0 - RA7 */
	0x40, 0x03, 0x00, 0x00, 0x40, 0x03, 0x00, 0x00,	/* RA8 - RAF */
	0x00, 0x00, 0x00, 0x00, 0x40, 0x6F, 0x00, 0x00,	/* RB0 - RB7 */
	0x40, 0x6F, 0x00, 0x00, 0x40, 0x6F, 0x00, 0x40,	/* RB8 - RBF */
	0x6F, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10, 0x00,	/* RC0 - RC7 */
	0x10, 0x00, 0x00, 0x00, 0x40, 0x72, 0x00, 0x40,	/* RC8 - RCF */
	0x72, 0x00, 0x00, 0x00, 0x40, 0x72, 0x00, 0x00,	/* RD0 - RD7 */
	0x40, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* RD8 - RDF */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* RE0 - RE6 */
};
#endif /* CONFIG_SND_SOC_USE_DA9055_HW */
#define D2153_SNDP_MCLK_RATE	13000000

static int d2153_volatile_register(struct snd_soc_codec *codec,
				     unsigned int reg)
{
	switch (reg) {
	case D2153_CIF_CTRL:
	case D2153_STATUS1:
	case D2153_SYSTEM_STATUS:
	case D2153_ALC_CTRL1:
	case D2153_ALC_OFFSET_AUTO_M_L:
	case D2153_ALC_OFFSET_AUTO_U_L:
	case D2153_ALC_OFFSET_AUTO_M_R:
	case D2153_ALC_OFFSET_AUTO_U_R:
	case D2153_ALC_CIC_OP_LVL_DATA:
	case D2153_PLL_STATUS:
	case D2153_AUX_L_GAIN_STATUS:
	case D2153_AUX_R_GAIN_STATUS:
	case D2153_MIC_L_GAIN_STATUS:
	case D2153_MIC_R_GAIN_STATUS:
	/*
	 * DLG - Need this conditional define as when building for DA9055
	 * the following maps to an already defined register number and
	 * it won't build without it (duplicate case statement).
	 */
#ifndef CONFIG_SND_SOC_USE_DA9055_HW
	case D2153_MIC_EXT_GAIN_STATUS:
#endif /* CONFIG_SND_SOC_USE_DA9055_HW */
	case D2153_MIXIN_L_GAIN_STATUS:
	case D2153_MIXIN_R_GAIN_STATUS:
	case D2153_ADC_L_GAIN_STATUS:
	case D2153_ADC_R_GAIN_STATUS:
	case D2153_DAC_L_GAIN_STATUS:
	case D2153_DAC_R_GAIN_STATUS:
	case D2153_HP_L_GAIN_STATUS:
	case D2153_HP_R_GAIN_STATUS:
	case D2153_EP_GAIN_STATUS:
	/* DLG - Following registers don't exist on DA9055. */
#ifndef CONFIG_SND_SOC_USE_DA9055_HW
	case D2153_SP_GAIN_STATUS:
	case D2153_SP_STATUS:
#endif /* CONFIG_SND_SOC_USE_DA9055_HW */
		return 1;
	default:
		return 0;
	}
}

/*
 * Need to make this method availabe outside of the file for Soundpath usage
 * as we don't want to assign the function to the snd_soc_dai_ops. If we do
 * assign it then the method is automatically called from ALSA framework and
 * this could interfere with Soundpath functionality.
 */
static int d2153_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 aif_ctrl = 0;
	u8 fs;

	/* Set AIF format */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		aif_ctrl |= D2153_AIF_WORD_LENGTH_S16_LE;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		aif_ctrl |= D2153_AIF_WORD_LENGTH_S20_LE;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		/* keep S16_LE for FM Sound */
		aif_ctrl |= D2153_AIF_WORD_LENGTH_S16_LE;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		aif_ctrl |= D2153_AIF_WORD_LENGTH_S32_LE;
		break;
	default:
		return -EINVAL;
	}

	/* Set sampling rate */
	switch (params_rate(params)) {
	case 8000:
		fs = D2153_SR_8000;
		break;
	case 11025:
		fs = D2153_SR_11025;
		break;
	case 12000:
		fs = D2153_SR_12000;
		break;
	case 16000:
		fs = D2153_SR_16000;
		break;
	case 22050:
		fs = D2153_SR_22050;
		break;
	case 32000:
		fs = D2153_SR_32000;
		break;
	case 44100:
		fs = D2153_SR_44100;
		break;
	case 48000:
		fs = D2153_SR_48000;
		break;
	case 88200:
		fs = D2153_SR_88200;
		break;
	case 96000:
		fs = D2153_SR_96000;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, D2153_AIF_CTRL, D2153_AIF_WORD_LENGTH_MASK,
			    aif_ctrl);
	snd_soc_write(codec, D2153_SR, fs);

	return 0;
}

static int d2153_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u8 aif_clk_mode = 0, aif_ctrl = 0;

	dlg_info("%s() format = %d  \n",__FUNCTION__,fmt);
	/* Set master/slave mode */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		aif_clk_mode |= D2153_AIF_CLK_EN_MASTER_MODE;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		aif_clk_mode |= D2153_AIF_CLK_EN_SLAVE_MODE;
		break;
	default:
		return -EINVAL;
	}

	/* Set clock normal/inverted */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		aif_clk_mode |= D2153_AIF_WCLK_POL_INV;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		aif_clk_mode |= D2153_AIF_CLK_POL_INV;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		aif_clk_mode |= D2153_AIF_WCLK_POL_INV | D2153_AIF_CLK_POL_INV;
		break;
	default:
		return -EINVAL;
	}

	/* Only I2S is supported */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		aif_ctrl |= D2153_AIF_FORMAT_I2S_MODE;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		aif_ctrl |= D2153_AIF_FORMAT_LEFT_J;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		aif_ctrl |= D2153_AIF_FORMAT_RIGHT_J;
		break;
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		aif_ctrl |= D2153_AIF_FORMAT_DSP;
		break;
	default:
		return -EINVAL;
	}

	/* By default only 32 BCLK per WCLK is supported */
	aif_clk_mode |= D2153_AIF_BCLKS_PER_WCLK_32;

	snd_soc_write(codec, D2153_AIF_CLK_MODE, aif_clk_mode);
	snd_soc_update_bits(codec, D2153_AIF_CTRL, D2153_AIF_FORMAT_MASK,
			    aif_ctrl);

	return 0;
}

static int d2153_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;


	dlg_info("%s() mute = %d  \n",__FUNCTION__,mute);

	if (mute) {
#if 0
		snd_soc_update_bits(codec, D2153_DAC_L_CTRL,
				   D2153_DAC_MUTE_EN, D2153_DAC_MUTE_EN);
		snd_soc_update_bits(codec, D2153_DAC_R_CTRL,
				   D2153_DAC_MUTE_EN, D2153_DAC_MUTE_EN);
#endif
	} else {
		snd_soc_update_bits(codec, D2153_DAC_L_CTRL,
				    D2153_DAC_MUTE_EN, 0);
		snd_soc_update_bits(codec, D2153_DAC_R_CTRL,
				    D2153_DAC_MUTE_EN, 0);
	}

	return 0;
}

static int d2153_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct d2153_codec_priv *d2153_codec = snd_soc_codec_get_drvdata(codec);

	dlg_info("%s() clk_id = %d  \n",__FUNCTION__,clk_id);

	switch (clk_id) {
	case D2153_CLKSRC_MCLK:
		if ((freq == 32768) ||
		    ((freq >= 5000000) && (freq <= 54000000))) {
			d2153_codec->mclk_rate = freq;
			return 0;
		} else {
			dev_err(codec_dai->dev, "Unsupported MCLK value %d\n",
				freq);
			return -EINVAL;
		}
		break;
	default:
		dev_err(codec_dai->dev, "Unknown clock source %d\n", clk_id);
		return -EINVAL;
	}
}

/* Note: Supported PLL input frequencies are 5MHz - 54MHz. */
static int d2153_set_dai_pll(struct snd_soc_dai *codec_dai, int pll_id,
			     int source, unsigned int fref, unsigned int fout)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct d2153_codec_priv *d2153_codec = snd_soc_codec_get_drvdata(codec);

	u8 pll_ctrl, indiv_bits, indiv;
	u8 pll_frac_top, pll_frac_bot, pll_integer;
	u32 freq_ref;
	u64 frac_div;

	dlg_info("%s() fout = %d\n", __func__, fout);
	pll_ctrl = 0;

	/* Workout input divider based on MCLK rate */
	if ((d2153_codec->mclk_rate == 32768) && (source == D2153_SYSCLK_PLL)) {
		/* 32KHz PLL Mode */
		indiv_bits = D2153_PLL_INDIV_10_20_MHZ;
		indiv = D2153_PLL_INDIV_10_20_MHZ_VAL;
		freq_ref = 3750000;
		pll_ctrl |= D2153_PLL_32K_MODE;
	} else {
		/* 5 - 54MHz MCLK */
		if (d2153_codec->mclk_rate < 5000000) {
			goto pll_err;
		} else if (d2153_codec->mclk_rate <= 10000000) {
			indiv_bits = D2153_PLL_INDIV_5_10_MHZ;
			indiv = D2153_PLL_INDIV_5_10_MHZ_VAL;
		} else if (d2153_codec->mclk_rate <= 20000000) {
			indiv_bits = D2153_PLL_INDIV_10_20_MHZ;
			indiv = D2153_PLL_INDIV_10_20_MHZ_VAL;
		} else if (d2153_codec->mclk_rate <= 40000000) {
			indiv_bits = D2153_PLL_INDIV_20_40_MHZ;
			indiv = D2153_PLL_INDIV_20_40_MHZ_VAL;
		} else if (d2153_codec->mclk_rate <= 54000000) {
			indiv_bits = D2153_PLL_INDIV_40_54_MHZ;
			indiv = D2153_PLL_INDIV_40_54_MHZ_VAL;
		} else {
			goto pll_err;
		}
		freq_ref = (d2153_codec->mclk_rate / indiv);
	}

	pll_ctrl |= indiv_bits;

	/* If SRM enabled, freq_out is (98304000 + 90316800)/2 = 94310400 */
	if (d2153_codec->srm_en) {
		fout = D2153_PLL_FREQ_OUT_94310400;
		pll_ctrl |= D2153_PLL_SRM_EN;
	}

	/* Are we changing anything? */
	if (d2153_codec->source == source &&
	    d2153_codec->freq_ref == freq_ref &&
		d2153_codec->fout == fout)
		return 0;

	/* Disable PLL before setting the divisors */
	snd_soc_update_bits(codec, D2153_PLL_CTRL, D2153_PLL_EN, 0);

	/* PLL Bypass mode */
	if (source == D2153_SYSCLK_MCLK) {
		snd_soc_write(codec, D2153_PLL_CTRL, pll_ctrl);
		return 0;
	}

	/* Calculate dividers for PLL */
	pll_integer = fout / freq_ref;
	frac_div = (u64)(fout % freq_ref) * 8192ULL;
	do_div(frac_div, freq_ref);
	pll_frac_top = (frac_div >> D2153_BYTE_SHIFT) & D2153_BYTE_MASK;
	pll_frac_bot = (frac_div) & D2153_BYTE_MASK;

	/* Write PLL dividers */
	snd_soc_write(codec, D2153_PLL_FRAC_TOP, pll_frac_top);
	snd_soc_write(codec, D2153_PLL_FRAC_BOT, pll_frac_bot);
	snd_soc_write(codec, D2153_PLL_INTEGER, pll_integer);
	snd_soc_write(codec, 0x52, 0x03);
	snd_soc_update_bits(codec, D2153_PC_COUNT, 0x02, 0x02);

	d2153_codec->source = source;
	d2153_codec->freq_ref = freq_ref;
	d2153_codec->fout = fout;

	/* Enable PLL */
	pll_ctrl |= D2153_PLL_EN;
	snd_soc_write(codec, D2153_PLL_CTRL, pll_ctrl);

	return 0;

pll_err:
	dev_err(codec_dai->dev, "Unsupported PLL input frequency %d\n",
		d2153_codec->mclk_rate);
	return -EINVAL;
}

#define D2153_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
		       SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops d2153_dai_ops = {
	.hw_params	= d2153_hw_params,
	.set_fmt	= d2153_set_dai_fmt,
	.set_sysclk	= d2153_set_dai_sysclk,
	.set_pll	= d2153_set_dai_pll,
	.digital_mute	= d2153_mute,
};


static struct snd_soc_dai_driver d2153_dai[] = {
	{
		.name = "d2153-aif1",
		.id = 1,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE },
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE },
		.ops =  &d2153_dai_ops,
	},
	{
		.name = "d2153-aif2",
		.id = 2,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE },
		.capture = {
			.stream_name = "Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_96000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE |
					SNDRV_PCM_FMTBIT_S24_LE },
		.ops =  &d2153_dai_ops,
	},
};

static int d2153_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	dlg_info("%s() level = %d  \n",__FUNCTION__,level);

	switch (level) {
	case SND_SOC_BIAS_ON:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
			snd_soc_update_bits(codec, D2153_REFERENCES,
					    D2153_VMID_EN | D2153_BIAS_EN |D2153_VMID_FAST_CHARGE |D2153_VMID_FAST_DISCHARGE,
					    D2153_VMID_EN | D2153_BIAS_EN);
			msleep(10);
			snd_soc_update_bits(codec, D2153_REFERENCES,
					    D2153_VMID_EN | D2153_BIAS_EN |D2153_VMID_FAST_CHARGE |D2153_VMID_FAST_DISCHARGE,
					    D2153_VMID_EN | D2153_BIAS_EN |D2153_VMID_FAST_CHARGE |D2153_VMID_FAST_DISCHARGE);
		}
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF) {
			/* Enable VMID reference & master bias */
			snd_soc_update_bits(codec, D2153_REFERENCES,
					    D2153_VMID_EN | D2153_BIAS_EN |D2153_VMID_FAST_CHARGE |D2153_VMID_FAST_DISCHARGE,
					    D2153_VMID_EN | D2153_BIAS_EN |D2153_VMID_FAST_CHARGE);
		}
		break;
	case SND_SOC_BIAS_OFF:
		/* Disable VMID reference & master bias */
		snd_soc_update_bits(codec, D2153_REFERENCES,
				    D2153_VMID_EN | D2153_BIAS_EN
				    |D2153_VMID_FAST_CHARGE | D2153_VMID_FAST_DISCHARGE
				    , 0);
		break;
	}
	codec->dapm.bias_level = level;
	return 0;
}

static void d2153_setup(struct snd_soc_codec *codec)
{
	struct d2153_codec_priv *d2153_codec = snd_soc_codec_get_drvdata(codec);

#ifdef CONFIG_SND_SOC_USE_DA9055_HW
	/*
	 * Default to using ALC manual offset calibration mode.
	 * Auto not supported on DA9055.
	 */
	snd_soc_update_bits(codec, D2153_ALC_CTRL1, D2153_ALC_CALIB_MODE_MAN,
			    D2153_ALC_CALIB_MODE_MAN);
	d2153_codec->alc_calib_auto = false;
#else
	/* Default to using ALC auto offset calibration mode. */
	snd_soc_update_bits(codec, D2153_ALC_CTRL1, D2153_ALC_CALIB_MODE_MAN, 0);
	d2153_codec->alc_calib_auto = true;
#endif /* CONFIG_SND_SOC_USE_DA9055_HW */

	/*
	 * DLG - The following defaults should probably be platform
	 * data, but for now they're hardcoded here as a reminder.
	 */

	/* Default to using SRM for slave mode */
	d2153_codec->srm_en = true;

	/* Default dmic settings */
	snd_soc_update_bits(codec, D2153_MIC_CONFIG, D2153_DMIC_DATA_SEL_MASK,
			    D2153_DMIC_DATA_SEL_RL_FR);
	snd_soc_update_bits(codec, D2153_MIC_CONFIG,
			    D2153_DMIC_SAMPLEPHASE_MASK,
			    D2153_DMIC_SAMPLEPHASE_ON_CLK_EDGE);
	snd_soc_update_bits(codec, D2153_MIC_CONFIG, D2153_DMIC_CLK_RATE_MASK,
			    D2153_DMIC_CLK_RATE_3MHZ);

	/* Default mic bias levels */
#ifdef CONFIG_SND_SOC_USE_DA9055_HW
	snd_soc_update_bits(codec, D2153_MIC_CONFIG, D2153_MICBIAS_LEVEL_MASK,
			    D2153_MICBIAS_LEVEL_1_5V);
#else
#ifndef D2153_DEFAULT_SET_MICBIAS
	snd_soc_update_bits(codec, D2153_MICBIAS1_CTRL,
			    D2153_MICBIAS_LEVEL_MASK,
			    d2153_codec->micbias1_level);
	snd_soc_update_bits(codec, D2153_MICBIAS2_CTRL,
			    D2153_MICBIAS_LEVEL_MASK,
			    d2153_codec->micbias2_level);
	snd_soc_update_bits(codec, D2153_MICBIAS3_CTRL,
			    D2153_MICBIAS_LEVEL_MASK,
			    d2153_codec->micbias3_level);
#else
	snd_soc_update_bits(codec, D2153_MICBIAS1_CTRL,
			    D2153_MICBIAS_LEVEL_MASK,
			    D2153_MICBIAS_LEVEL_2_6V);
	snd_soc_update_bits(codec, D2153_MICBIAS2_CTRL,
			    D2153_MICBIAS_LEVEL_MASK,
			    D2153_MICBIAS_LEVEL_2_1V);
	snd_soc_update_bits(codec, D2153_MICBIAS3_CTRL,
			    D2153_MICBIAS_LEVEL_MASK,
			    D2153_MICBIAS_LEVEL_2_1V);
#endif	/* D2153_DEFAULT_SET_MICBIAS */
#endif /* CONFIG_SND_SOC_USE_DA9055_HW */

	/*
	 * DLG - From the technical datasheet diagram for D2153 this should
	 * not be needed as there is a bias for each MIC input.
	 */
#ifdef CONFIG_SND_SOC_USE_DA9055_HW
	/* Default to MIC Bias 1 for right in mixer */
	snd_soc_update_bits(codec, D2153_MIXIN_R_SELECT,
			    D2153_MIC_BIAS_OUTPUT_SELECT_BIAS2, 0);
#else
	/* Default to MIC Bias 2 for right in mixer */
	snd_soc_update_bits(codec, D2153_MIXIN_R_SELECT,
			    D2153_MIC_BIAS_OUTPUT_SELECT_BIAS2,
			    D2153_MIC_BIAS_OUTPUT_SELECT_BIAS2);
#endif /* CONFIG_SND_SOC_USE_DA9055_HW */

	/* Speaker config defaults */
	snd_soc_write(codec, D2153_SP_CFG1, 0);
	snd_soc_write(codec, D2153_SP_CFG2, 0);

	/* Default IO settings */
	snd_soc_update_bits(codec, D2153_IO_CTRL,
			    D2153_IO_VOLTAGE_LEVEL_MASK,
			    D2153_IO_VOLTAGE_LEVEL_1_2V_2_8V);

	/* Default LDO settings */
	snd_soc_update_bits(codec, D2153_LDO_CTRL, D2153_LDO_LEVEL_SELECT_MASK,
			    D2153_LDO_LEVEL_SELECT_1_05V);
	snd_soc_update_bits(codec, D2153_LDO_CTRL, D2153_LDO_EN_MASK,
			    D2153_LDO_EN_MASK);

	/* Enable all Gain Ramping Controls */
	snd_soc_update_bits(codec, D2153_AUX_L_CTRL,
			    D2153_AUX_AMP_RAMP_EN, D2153_AUX_AMP_RAMP_EN);
	snd_soc_update_bits(codec, D2153_AUX_R_CTRL,
			    D2153_AUX_AMP_RAMP_EN, D2153_AUX_AMP_RAMP_EN);
	snd_soc_update_bits(codec, D2153_MIXIN_L_CTRL,
			    D2153_MIXIN_AMP_RAMP_EN, D2153_MIXIN_AMP_RAMP_EN);
	snd_soc_update_bits(codec, D2153_MIXIN_R_CTRL,
			    D2153_MIXIN_AMP_RAMP_EN, D2153_MIXIN_AMP_RAMP_EN);
	snd_soc_update_bits(codec, D2153_ADC_L_CTRL,
			    D2153_ADC_RAMP_EN, D2153_ADC_RAMP_EN);
	snd_soc_update_bits(codec, D2153_ADC_R_CTRL,
			    D2153_ADC_RAMP_EN, D2153_ADC_RAMP_EN);
	snd_soc_update_bits(codec, D2153_DAC_L_CTRL,
			    D2153_DAC_RAMP_EN, D2153_DAC_RAMP_EN);
	snd_soc_update_bits(codec, D2153_DAC_R_CTRL,
			    D2153_DAC_RAMP_EN, D2153_DAC_RAMP_EN);
	snd_soc_update_bits(codec, D2153_HP_L_CTRL,
			    D2153_HP_AMP_RAMP_EN, D2153_HP_AMP_RAMP_EN);
	snd_soc_update_bits(codec, D2153_HP_R_CTRL,
			    D2153_HP_AMP_RAMP_EN, D2153_HP_AMP_RAMP_EN);
	snd_soc_update_bits(codec, D2153_EP_CTRL,
			    D2153_EP_AMP_RAMP_EN, D2153_EP_AMP_RAMP_EN);
	snd_soc_update_bits(codec, D2153_SP_CTRL,
			    D2153_SP_AMP_RAMP_EN, D2153_SP_AMP_RAMP_EN);

	/*
	 * There are two separate control bits for input and output mixers as
	 * well as headphone and speaker outs.
	 * One to enable corresponding amplifier and other to enable its
	 * output. As amplifier bits are related to power control, they are
	 * being managed by DAPM while other (non power related) bits are
	 * enabled here
	 */

	/* Enable Left and Right input mixers */
	snd_soc_update_bits(codec, D2153_MIXIN_L_CTRL,
			    D2153_MIXIN_MIX_EN, D2153_MIXIN_MIX_EN);
	snd_soc_update_bits(codec, D2153_MIXIN_R_CTRL,
			    D2153_MIXIN_MIX_EN, D2153_MIXIN_MIX_EN);

	/* Enable Left and Right output mixers */
	snd_soc_update_bits(codec, D2153_MIXOUT_L_CTRL,
			    D2153_MIXOUT_MIX_EN, D2153_MIXOUT_MIX_EN);
	snd_soc_update_bits(codec, D2153_MIXOUT_R_CTRL,
			    D2153_MIXOUT_MIX_EN, D2153_MIXOUT_MIX_EN);

	/* Enable Speaker output mixer */
	snd_soc_update_bits(codec, D2153_MIXOUT_SP_CTRL,
			    D2153_MIXOUT_MIX_EN, D2153_MIXOUT_MIX_EN);

	/* Set charge pump mode */
	snd_soc_update_bits(codec, D2153_CP_CTRL, D2153_CP_MCHANGE_MASK,
			    D2153_CP_MCHANGE_SM_SIZE);
	//snd_soc_update_bits(codec, D2153_CP_CTRL, D2153_CP_MCHANGE_MASK,
			    //D2153_CP_MCHANGE_LARGEST_VOL);

	snd_soc_write(codec, D2153_CP_VOL_THRESHOLD1 ,0x34);

	/* Enable AIF output */
	snd_soc_update_bits(codec, D2153_AIF_CTRL, D2153_AIF_OE, D2153_AIF_OE);

	/* Enable Left and Right Headphone Output */
	snd_soc_update_bits(codec, D2153_HP_L_CTRL,
			    D2153_HP_AMP_OE, D2153_HP_AMP_OE);
	snd_soc_update_bits(codec, D2153_HP_R_CTRL,
			    D2153_HP_AMP_OE, D2153_HP_AMP_OE);

	/* Enable Earpiece Output Enable */
	snd_soc_update_bits(codec, D2153_EP_CTRL,
			    D2153_EP_AMP_OE, D2153_EP_AMP_OE);
}

static int d2153_codec_write(struct snd_soc_codec *codec,
		unsigned int reg, unsigned int value);

#ifdef CONFIG_PM
static int d2153_suspend(struct snd_soc_codec *codec)
{
	struct d2153_codec_priv *d2153_codec =
			snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = d2153_codec->aad_i2c_client;
	struct d2153_aad_priv *d2153_aad = i2c_get_clientdata(client);

	if ((d2153_codec->switch_state == D2153_HEADSET) &&
			(d2153_aad->chip_rev < D2153_AC_Silicon))
		return 0;

	d2153_codec_power(codec, 0);
	return 0;
}

static int d2153_resume(struct snd_soc_codec *codec)
{
	struct d2153_codec_priv *d2153_codec =
			snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = d2153_codec->aad_i2c_client;
	struct d2153_aad_priv *d2153_aad = i2c_get_clientdata(client);
	u8 *cache = codec->reg_cache;
	int i;

	if ((d2153_codec->switch_state == D2153_HEADSET) &&
			(d2153_aad->chip_rev < D2153_AC_Silicon))
		return 0;

	d2153_codec_power(codec,1);

	cache[D2153_SYSTEM_MODES_CFG3] = 0;
	for(i =0 ; i<ARRAY_SIZE(d2153_reg_defaults) ; i++) {
		if (0 == (d2153_volatile_register(codec, i)) &&
				(cache[i] != d2153_reg_defaults[i])) {
			d2153_codec_write(codec,i,cache[i]);
		}
	}

	d2153_aad_resume(codec);

	if (d2153_aad->chip_rev < D2153_AC_Silicon)
		schedule_delayed_work(&d2153_aad->jack_monitor_work,
				msecs_to_jiffies(100));
	return 0;
}
#else
#define d2153_suspend NULL
#define d2153_resume NULL
#endif

#ifdef CONFIG_SND_SOC_D2153_AAD
int d2153_aad_resume(struct snd_soc_codec *codec)
{
	struct d2153_codec_priv *d2153_codec =
		snd_soc_codec_get_drvdata(codec);
	struct i2c_client *client = d2153_codec->aad_i2c_client;
	struct d2153_aad_priv *d2153_aad = i2c_get_clientdata(client);

	d2153_aad_write(client,D2153_ACCDET_UNLOCK_AO,0x4a);

	d2153_aad_write(client,D2153_ACCDET_TST2,0x10);
	d2153_aad_write(client, D2153_ACCDET_THRESH1, 0x1a);
	d2153_aad_write(client,D2153_ACCDET_THRESH2,0x56);
	d2153_aad_write(client,D2153_ACCDET_THRESH3,0x0e);
	d2153_aad_write(client,D2153_ACCDET_THRESH4,0x44);

	d2153_aad_write(client,D2153_ACCDET_CFG1,0x5f);
	d2153_aad_write(client,D2153_ACCDET_CFG2,0x00);
	d2153_aad_write(client,D2153_ACCDET_CFG3,0x03);

	if (D2153_AA_Silicon == d2153_aad->chip_rev)
		d2153_aad_write(client, D2153_ACCDET_CFG4, 0x07);

	if (d2153_aad->chip_rev < D2153_AC_Silicon) {
		if (d2153_aad->codec_detect_enable)
			d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x88);
		else
			d2153_aad_write(client, D2153_ACCDET_CONFIG, 0x80);

		d2153_aad->switch_data.state = D2153_NO_JACK;
	} else {
		if (d2153_codec->switch_state == D2153_HEADSET) {
			if (d2153_aad->codec_detect_enable)
				d2153_aad_write(client,
					D2153_ACCDET_CONFIG, 0x88);
			else
				d2153_aad_write(client,
					D2153_ACCDET_CONFIG, 0x80);
		}
	}

	if (D2153_AA_Silicon == d2153_aad->chip_rev)
		/* ignore the fist interrupt */
		d2153_aad->button.status = D2153_BUTTON_PRESS;
	return 0;
}

#endif

static int d2153_probe(struct snd_soc_codec *codec)
{
	struct d2153_codec_priv *d2153_codec = snd_soc_codec_get_drvdata(codec);
	//struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;

	d2153_codec->codec = codec;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
	if (ret < 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

#ifdef CONFIG_SND_SOC_USE_DA9055_HW
	/*
	 * DLG - When using DA9055 for testing, some registers are shared due
	 * to new/modified registers for D2153. If we don't do cache bypass,
	 * then controls don't always operate correctly.
	 */
	codec->cache_bypass = 1;
#endif /* CONFIG_SND_SOC_USE_DA9055_HW */

		d2153_codec_power(d2153_codec->codec,1);
		d2153_setup(codec);
#ifdef CONFIG_SND_SOC_D2153_AAD
		d2153_aad_enable(codec);
		d2153_codec->codec_init =1;
#endif

	return 0;
}

static int d2153_remove(struct snd_soc_codec *codec)
{
	d2153_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int d2153_codec_write(struct snd_soc_codec *codec, unsigned int reg, unsigned int value)
{
	struct d2153_codec_priv *d2153_codec = snd_soc_codec_get_drvdata(codec);
	u8 data[2];
	int ret;

	mutex_lock(&d2153_codec->d2153_pmic->d2153_io_mutex);


	reg &= 0xff;
	data[0] = reg;
	data[1] = value & 0xff;

	ret = i2c_master_send(d2153_codec->i2c_client, data, 2);

	mutex_unlock(&d2153_codec->d2153_pmic->d2153_io_mutex);

	if (ret == 2)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;

}

static struct snd_soc_codec_driver soc_codec_dev_d2153 = {
	.probe			= d2153_probe,
	.remove			= d2153_remove,
	.suspend		= d2153_suspend,
	.resume			= d2153_resume,
	.reg_cache_size		= ARRAY_SIZE(d2153_reg_defaults),
	.reg_word_size		= sizeof(u8),
	.reg_cache_default	= d2153_reg_defaults,
	.volatile_register	= d2153_volatile_register,
	.set_bias_level		= d2153_set_bias_level,
	.controls		= d2153_snd_controls,
	.num_controls		= ARRAY_SIZE(d2153_snd_controls),
	.dapm_widgets		= d2153_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(d2153_dapm_widgets),
	.dapm_routes		= d2153_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(d2153_audio_map),
};

#ifdef CONFIG_SND_SOC_D2153_AAD
static struct i2c_board_info aad_i2c_info = {
	I2C_BOARD_INFO("d2153-aad", D2153_AAD_I2C_ADDR),
};
#endif /* CONFIG_SND_SOC_D2153_AAD */

#ifdef CONFIG_PROC_FS
static int d2153codec_i2c_read_device(struct d2153_codec_priv *d2153_codec,char reg,
					int bytes, void *dest)
{
	int ret;
	struct i2c_msg msgs[2];
	struct i2c_adapter *adap = d2153_codec->i2c_client->adapter;

	mutex_lock(&d2153_codec->d2153_pmic->d2153_io_mutex);
	
	if(reg > 0x6b && reg < 0x80){
		msgs[0].addr = D2153_AAD_I2C_ADDR;
	}
	else {
		msgs[0].addr = D2153_CODEC_I2C_ADDR;
	}
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;

	if(reg > 0x6b && reg < 0x80){
		msgs[1].addr = D2153_AAD_I2C_ADDR;
	}
	else {
		msgs[1].addr = D2153_CODEC_I2C_ADDR;
	}
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = bytes;
	msgs[1].buf = (char *)dest;

	ret = i2c_transfer(adap,msgs,ARRAY_SIZE(msgs));

	mutex_unlock(&d2153_codec->d2153_pmic->d2153_io_mutex);
	
	if (ret < 0 )
		return ret;
	else if (ret == ARRAY_SIZE(msgs))
		return 0;
	else
		return -EFAULT;
}

static int d2153codec_i2c_write_device(struct d2153_codec_priv *d2153_codec,char reg,
				   int bytes, u8 *src )
{
	int ret;
	struct i2c_msg msgs[1];
	u8 data[12];
	u8 *buf = data;

	struct i2c_adapter *adap = d2153_codec->i2c_client->adapter;

	if (bytes == 0)
		return -EINVAL;

	BUG_ON(bytes >= ARRAY_SIZE(data));

	mutex_lock(&d2153_codec->d2153_pmic->d2153_io_mutex);
	
	if(reg > 0x6b && reg < 0x80){
		msgs[0].addr = D2153_AAD_I2C_ADDR;
	}
	else {
		msgs[0].addr = D2153_CODEC_I2C_ADDR;
	}
	msgs[0].flags = d2153_codec->i2c_client->flags & I2C_M_TEN;
	msgs[0].len = 1+bytes;
	msgs[0].buf = data;

	*buf++ = reg;
	while(bytes--) *buf++ = *src++;

	ret = i2c_transfer(adap,msgs,ARRAY_SIZE(msgs));

	mutex_unlock(&d2153_codec->d2153_pmic->d2153_io_mutex);
	
	if (ret < 0 )
		return ret;
	else if (ret == ARRAY_SIZE(msgs))
		return 0;
	else
		return -EFAULT;
}


static int d2153_codec_ioctl_open(struct inode *inode, struct file *file)
{
	dlg_info("%s\n", __func__);
	file->private_data = PDE(inode)->data;
	return 0;
}

int d2153_codec_ioctl_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

/*
 *
 */
static long d2153_codec_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
		struct d2153_codec_priv *d2153_codec =  file->private_data;
		pmu_reg reg;
		int ret = 0;
		u8 reg_val;

		if (!d2153_codec)
			return -ENOTTY;

		switch (cmd) {

			case D2153_IOCTL_READ_REG:
				if (copy_from_user(&reg, (pmu_reg *)arg, sizeof(pmu_reg)) != 0)
					return -EFAULT;

				if(reg.reg > 0x6b && reg.reg < 0x80) {
					d2153codec_i2c_read_device(d2153_codec,(u8)reg.reg,1, &reg_val);
				}
				else {
					reg_val=snd_soc_read(d2153_codec->codec,reg.reg);
				}
				reg.val = (unsigned short)reg_val;
				if (copy_to_user((pmu_reg *)arg, &reg, sizeof(pmu_reg)) != 0)
					return -EFAULT;
			break;

			case D2153_IOCTL_WRITE_REG:
				if (copy_from_user(&reg, (pmu_reg *)arg, sizeof(pmu_reg)) != 0)
					return -EFAULT;
				if(reg.reg > 0x6b && reg.reg < 0x80) {
					d2153codec_i2c_write_device(d2153_codec,(u8)reg.reg,1, (u8 *)(&reg.val));
				}
				else {
					snd_soc_write(d2153_codec->codec, reg.reg, reg.val);
				}
			break;

		default:
			dlg_err("%s: unsupported cmd\n", __func__);
			ret = -ENOTTY;
		}

		return ret;
}

#define MAX_D2153_CODEC_USER_INPUT_LEN      100
#define MAX_D2153_CODEC_REGS_READ_WRITE     0xDF

enum codec_debug_ops {
	CODECDBG_READ_REG = 0UL,
	CODECDBG_WRITE_REG,
};

struct codec_debug {
	int read_write;
	int len;
	int addr;
	u8 val[MAX_D2153_CODEC_REGS_READ_WRITE];
};

/*
 *
 */
static void d2153_codec_dbg_usage(void)
{
	printk(KERN_INFO "Usage:\n");
	printk(KERN_INFO "Read a register: echo 0x0800 > /proc/codec0\n");
	printk(KERN_INFO
		"Read multiple regs: echo 0x0800 -c 10 > /proc/codec0\n");
	printk(KERN_INFO
		"Write multiple regs: echo 0x0800 0xFF 0xFF > /proc/codec0\n");
	printk(KERN_INFO
		"Write single reg: echo 0x0800 0xFF > /proc/codec0\n");
	printk(KERN_INFO "Max number of regs in single write is :%d\n",
		MAX_D2153_CODEC_REGS_READ_WRITE);
	printk(KERN_INFO "Register address is encoded as follows:\n");
	printk(KERN_INFO "0xSSRR, SS: i2c slave addr, RR: register addr\n");
}


/*
 *
 */
static int d2153_codec_dbg_parse_args(char *cmd, struct codec_debug *dbg)
{
	char *tok;                 /* used to separate tokens             */
	const char ct[] = " \t";   /* space or tab delimits the tokens    */
	bool count_flag = false;   /* whether -c option is present or not */
	int tok_count = 0;         /* total number of tokens parsed       */
	int i = 0;

	dbg->len        = 0;

	/* parse the input string */
	while ((tok = strsep(&cmd, ct)) != NULL) {
		dlg_info("token: %s\n", tok);

		/* first token is always address */
		if (tok_count == 0) {
			sscanf(tok, "%x", &dbg->addr);
		} else if (strnicmp(tok, "-c", 2) == 0) {
			/* the next token will be number of regs to read */
			tok = strsep(&cmd, ct);
			if (tok == NULL)
				return -EINVAL;

			tok_count++;
			sscanf(tok, "%d", &dbg->len);
			count_flag = true;
			break;
		} else {
			int val;

			/* this is a value to be written to the pmu register */
			sscanf(tok, "%x", &val);
			if (i < MAX_D2153_CODEC_REGS_READ_WRITE) {
				dbg->val[i] = val;
				i++;
			}
		}

		tok_count++;
	}

	/* decide whether it is a read or write operation based on the
	 * value of tok_count and count_flag.
	 * tok_count = 0: no inputs, invalid case.
	 * tok_count = 1: only reg address is given, so do a read.
	 * tok_count > 1, count_flag = false: reg address and atleast one
	 *     value is present, so do a write operation.
	 * tok_count > 1, count_flag = true: to a multiple reg read operation.
	 */
	switch (tok_count) {
	case 0:
		return -EINVAL;
	case 1:
		dbg->read_write = CODECDBG_READ_REG;
		dbg->len = 1;
		break;
	default:
		if (count_flag == true) {
			dbg->read_write = CODECDBG_READ_REG;
		} else {
			dbg->read_write = CODECDBG_WRITE_REG;
			dbg->len = i;
		}
	}

	return 0;
}

/*
 *
 */
static ssize_t d2153_codec_ioctl_write(struct file *file, const char __user *buffer,
	size_t len, loff_t *offset)
{
	struct d2153_codec_priv *d2153_codec = file->private_data;
	struct codec_debug dbg;
	char cmd[MAX_D2153_CODEC_USER_INPUT_LEN];
	int ret=0, i;

	dlg_info("%s\n", __func__);

	if (!d2153_codec) {
		dlg_err("%s: driver not initialized\n", __func__);
		return -EINVAL;
	}

	if (len > MAX_D2153_CODEC_USER_INPUT_LEN)
		len = MAX_D2153_CODEC_USER_INPUT_LEN;

	if (copy_from_user(cmd, buffer, len)) {
		dlg_err("%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	/* chop of '\n' introduced by echo at the end of the input */
	if (cmd[len - 1] == '\n')
		cmd[len - 1] = '\0';

	if (d2153_codec_dbg_parse_args(cmd, &dbg) < 0) {
		d2153_codec_dbg_usage();
		return -EINVAL;
	}

	dlg_info("operation: %s\n", (dbg.read_write == CODECDBG_READ_REG) ?
		"read" : "write");
	dlg_info("address  : 0x%x\n", dbg.addr);
	dlg_info("length   : %d\n", dbg.len);

	if (dbg.read_write == CODECDBG_READ_REG) {

		for (i = 0; i < dbg.len; i++, dbg.addr++)
		{
			//if(dbg.addr > 0x6b && dbg.addr < 0x80) {
				ret=d2153codec_i2c_read_device(d2153_codec,dbg.addr,1,&dbg.val[i]);
			//}
			//else
			//{
			//	dbg.val[i]=snd_soc_read(d2153_codec->codec,dbg.addr);
			//}
			if (ret < 0) {
				dlg_err("%s: codec reg read failed ret=%d\n", __func__, ret);
				return -EFAULT;
			}
			dlg_info("[%x] = 0x%02x\n", dbg.addr,
				dbg.val[i]);
		}
	} else {
		for (i = 0; i < dbg.len; i++, dbg.addr++)
		{
			//if(dbg.addr > 0x6b && dbg.addr < 0x80) {
				ret = d2153codec_i2c_write_device(d2153_codec,dbg.addr,1,&dbg.val[i]);
			//}
			//else {
			//	snd_soc_write(d2153_codec->codec, dbg.addr, dbg.val[i]);
			//}
			if (ret < 0) {
				dlg_err("%s: codec reg write failed ret=%d\n", __func__,ret);
				return -EFAULT;
			}
		}
	}

	*offset += len;

	return len;
}

static const struct file_operations d2153_codec_ops = {
	.open = d2153_codec_ioctl_open,
	.unlocked_ioctl = d2153_codec_ioctl,
	.write = d2153_codec_ioctl_write,
	.release = d2153_codec_ioctl_release,
	.owner = THIS_MODULE,
};

void d2153_codec_debug_proc_init(struct d2153_codec_priv *d2153_codec)
{
	struct proc_dir_entry *entry;

	entry = proc_create_data("codec0", S_IRWXUGO, NULL, &d2153_codec_ops, d2153_codec);
		dlg_crit("\nD2153.c: proc_create_data() = %p; name=\"%s\"\n", entry, (entry?entry->name:""));
}

void d2153_codec_debug_proc_exit(void)
{
	remove_proc_entry("codec0", NULL);
}
#endif /* CONFIG_PROC_FS */

static int __devinit d2153_i2c_probe(struct i2c_client *client,
				     const struct i2c_device_id *id)
{
	struct d2153_codec_priv *d2153_codec;
	int ret;

	d2153_codec = devm_kzalloc(&client->dev,
				   sizeof(struct d2153_codec_priv), GFP_KERNEL);
	if (!d2153_codec)
		return -ENOMEM;

#ifdef CONFIG_SND_SOC_D2153_AAD
	d2153_codec->d2153_pmic = client->dev.platform_data;
#endif /* CONFIG_SND_SOC_D2153_AAD */

#ifndef D2153_DEFAULT_SET_MICBIAS
	d2153_codec->micbias1_level =
			d2153_codec->d2153_pmic->pdata->audio.micbias1_level;
	d2153_codec->micbias2_level =
			d2153_codec->d2153_pmic->pdata->audio.micbias2_level;
	d2153_codec->micbias3_level =
			d2153_codec->d2153_pmic->pdata->audio.micbias3_level;
#endif	/* D2153_DEFAULT_SET_MICBIAS */

	i2c_set_clientdata(client, d2153_codec);

	d2153_codec->i2c_client=client;

	ret = snd_soc_register_codec(&client->dev, &soc_codec_dev_d2153,
				     d2153_dai, ARRAY_SIZE(d2153_dai));
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register d2153-codec: %d\n",
			ret);
		return ret;
	}

#ifdef CONFIG_SND_SOC_D2153_AAD
	/* Register AAD I2C client */
	aad_i2c_info.platform_data = d2153_codec;
	d2153_codec->aad_i2c_client =
		i2c_new_device(client->adapter, &aad_i2c_info);
	if (!d2153_codec->aad_i2c_client)
		dev_err(&client->dev, "Failed to register AAD I2C device: 0x%x\n",
			D2153_AAD_I2C_ADDR);
#endif /* CONFIG_SND_SOC_D2153_AAD */

	#ifdef CONFIG_PROC_FS
		d2153_codec_debug_proc_init(d2153_codec);
	#endif

	return ret;
}

static int __devexit d2153_i2c_remove(struct i2c_client *client)
{
#ifdef CONFIG_SND_SOC_D2153_AAD
	struct d2153_codec_priv *d2153_codec = i2c_get_clientdata(client);
#endif /* CONFIG_SND_SOC_D2153_AAD */
#ifdef CONFIG_PROC_FS
	d2153_codec_debug_proc_exit();
#endif
#ifdef CONFIG_SND_SOC_D2153_AAD
	/* Unregister AAD I2C client */
	if (d2153_codec->aad_i2c_client)
		i2c_unregister_device(d2153_codec->aad_i2c_client);
#endif /* CONFIG_SND_SOC_D2153_AAD */

	snd_soc_unregister_codec(&client->dev);
	return 0;
}

void d2153_i2c_shutdown(struct i2c_client *client)
{
    struct d2153_codec_priv *d2153_codec;

    d2153_codec = i2c_get_clientdata(client);

    snd_soc_write(d2153_codec->codec, D2153_SYSTEM_MODES_CFG3,0x01);
    snd_soc_write(d2153_codec->codec, D2153_CIF_CTRL,0x80);
}

static const struct i2c_device_id d2153_i2c_id[] = {
	{ "d2153-codec", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, d2153_i2c_id);

/* I2C codec control layer */
static struct i2c_driver d2153_i2c_driver = {
	.driver = {
		.name = "d2153-codec",
		.owner = THIS_MODULE,
	},
	.probe		= d2153_i2c_probe,
	.remove		= __devexit_p(d2153_i2c_remove),
	.shutdown       = d2153_i2c_shutdown,
	.id_table	= d2153_i2c_id,
};

static int __init d2153_modinit(void)
{
	int ret;

	ret = i2c_add_driver(&d2153_i2c_driver);
	if (ret)
		pr_err("D2153 I2C registration failed %d\n", ret);

	return ret;
}
module_init(d2153_modinit);

static void __exit d2153_exit(void)
{
	i2c_del_driver(&d2153_i2c_driver);
}
module_exit(d2153_exit);

MODULE_DESCRIPTION("ASoC D2153 Codec Driver");
MODULE_AUTHOR("Adam Thomson <Adam.Thomson.Opensource@diasemi.com>");
MODULE_LICENSE("GPL");
