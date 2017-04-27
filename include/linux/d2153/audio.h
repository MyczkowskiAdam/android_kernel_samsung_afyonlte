/*
 * audio.h: Audio amplifier driver for D2153
 *
 * Copyright(c) 2012 Dialog Semiconductor Ltd.
 * Author: Mariusz Wojtasik <mariusz.wojtasik@diasemi.com>
 *
 * This program is free software; you can redistribute  it and/or modify
 * it under  the terms of  the GNU General  Public License as published by
 * the Free Software Foundation;  either version 2 of the  License, or
 * (at your option) any later version.
 *
 */

#ifndef __LINUX_LEOPARD_AUDIO_H
#define __LINUX_LEOPARD_AUDIO_H

#include <linux/platform_device.h>

#define LEOPARD_MAX_GAIN_TABLE 24

enum d2153_audio_input_val {
	LEOPARD_IN_NONE   = 0x00,
	LEOPARD_IN_A1     = 0x01,
	LEOPARD_IN_A2     = 0x02,
	LEOPARD_IN_B1     = 0x04,
	LEOPARD_IN_B2     = 0x08
};

enum d2153_input_mode_val {
	LEOPARD_IM_TWO_SINGLE_ENDED   = 0x00,
	LEOPARD_IM_ONE_SINGLE_ENDED_1 = 0x01,
	LEOPARD_IM_ONE_SINGLE_ENDED_2 = 0x02,
	LEOPARD_IM_FULLY_DIFFERENTIAL = 0x03
};
enum d2153_input_path_sel {
	LEOPARD_INPUTA,
	LEOPARD_INPUTB
};

enum d2153_preamp_gain_val {
	LEOPARD_PREAMP_GAIN_NEG_6DB   = 0x07,
	LEOPARD_PREAMP_GAIN_NEG_5DB   = 0x08,
	LEOPARD_PREAMP_GAIN_NEG_4DB   = 0x09,
	LEOPARD_PREAMP_GAIN_NEG_3DB   = 0x0a,
	LEOPARD_PREAMP_GAIN_NEG_2DB   = 0x0b,
	LEOPARD_PREAMP_GAIN_NEG_1DB   = 0x0c,
	LEOPARD_PREAMP_GAIN_0DB       = 0x0d,
	LEOPARD_PREAMP_GAIN_1DB       = 0x0e,
	LEOPARD_PREAMP_GAIN_2DB       = 0x0f,
	LEOPARD_PREAMP_GAIN_3DB       = 0x10,
	LEOPARD_PREAMP_GAIN_4DB       = 0x11,
	LEOPARD_PREAMP_GAIN_5DB       = 0x12,
	LEOPARD_PREAMP_GAIN_6DB       = 0x13,
	LEOPARD_PREAMP_GAIN_7DB       = 0x14,
	LEOPARD_PREAMP_GAIN_8DB       = 0x15,
	LEOPARD_PREAMP_GAIN_9DB       = 0x16,
	LEOPARD_PREAMP_GAIN_10DB      = 0x17,
	LEOPARD_PREAMP_GAIN_11DB      = 0x18,
	LEOPARD_PREAMP_GAIN_12DB      = 0x19,
	LEOPARD_PREAMP_GAIN_13DB      = 0x1a,
	LEOPARD_PREAMP_GAIN_14DB      = 0x1b,
	LEOPARD_PREAMP_GAIN_15DB      = 0x1c,
	LEOPARD_PREAMP_GAIN_16DB      = 0x1d,
	LEOPARD_PREAMP_GAIN_17DB      = 0x1e,
	LEOPARD_PREAMP_GAIN_18DB      = 0x1f,
	LEOPARD_PREAMP_GAIN_MUTE
};

static inline int d2153_pagain_to_reg(enum d2153_preamp_gain_val gain_val)
{
	return (int)gain_val;
}

static inline enum d2153_preamp_gain_val d2153_reg_to_pagain(int reg)
{
	return ((reg < LEOPARD_PREAMP_GAIN_NEG_6DB) ? LEOPARD_PREAMP_GAIN_NEG_6DB :
			(enum d2153_preamp_gain_val)reg);
}

static inline int d2153_pagain_to_db(enum d2153_preamp_gain_val gain_val)
{
	return (int)gain_val - (int)LEOPARD_PREAMP_GAIN_0DB;
}

static inline enum d2153_preamp_gain_val d2153_db_to_pagain(int db)
{
	return (enum d2153_preamp_gain_val)(db + (int)LEOPARD_PREAMP_GAIN_0DB);
}

enum d2153_mixer_selector_val {
	LEOPARD_MSEL_A1   = 0x01, /* if A is fully diff. then selects A2 as the inverting input */
	LEOPARD_MSEL_B1   = 0x02, /* if B is fully diff. then selects B2 as the inverting input */
	LEOPARD_MSEL_A2   = 0x04, /* if A is fully diff. then selects A1 as the non-inverting input */
	LEOPARD_MSEL_B2   = 0x08  /* if B is fully diff. then selects B1 as the non-inverting input */
};

enum d2153_mixer_attenuation_val {
	LEOPARD_MIX_ATT_0DB       = 0x00, /* attenuation = 0dB */
	LEOPARD_MIX_ATT_NEG_6DB   = 0x01, /* attenuation = -6dB */
	LEOPARD_MIX_ATT_NEG_9DB5  = 0x02, /* attenuation = -9.5dB */
	LEOPARD_MIX_ATT_NEG_12DB  = 0x03  /* attenuation = -12dB */
};

enum d2153_hp_vol_val {
	LEOPARD_HPVOL_NEG_57DB,   LEOPARD_HPVOL_NEG_56DB,   LEOPARD_HPVOL_NEG_55DB,   LEOPARD_HPVOL_NEG_54DB,
	LEOPARD_HPVOL_NEG_53DB,   LEOPARD_HPVOL_NEG_52DB,   LEOPARD_HPVOL_NEG_51DB,   LEOPARD_HPVOL_NEG_50DB,
	LEOPARD_HPVOL_NEG_49DB,   LEOPARD_HPVOL_NEG_48DB,   LEOPARD_HPVOL_NEG_47DB,   LEOPARD_HPVOL_NEG_46DB,
	LEOPARD_HPVOL_NEG_45DB,   LEOPARD_HPVOL_NEG_44DB,   LEOPARD_HPVOL_NEG_43DB,   LEOPARD_HPVOL_NEG_42DB,
	LEOPARD_HPVOL_NEG_41DB,   LEOPARD_HPVOL_NEG_40DB,   LEOPARD_HPVOL_NEG_39DB,   LEOPARD_HPVOL_NEG_38DB,
	LEOPARD_HPVOL_NEG_37DB,   LEOPARD_HPVOL_NEG_36DB,   LEOPARD_HPVOL_NEG_35DB,   LEOPARD_HPVOL_NEG_34DB,
	LEOPARD_HPVOL_NEG_33DB,   LEOPARD_HPVOL_NEG_32DB,   LEOPARD_HPVOL_NEG_31DB,   LEOPARD_HPVOL_NEG_30DB,
	LEOPARD_HPVOL_NEG_29DB,   LEOPARD_HPVOL_NEG_28DB,   LEOPARD_HPVOL_NEG_27DB,   LEOPARD_HPVOL_NEG_26DB,
	LEOPARD_HPVOL_NEG_25DB,   LEOPARD_HPVOL_NEG_24DB,   LEOPARD_HPVOL_NEG_23DB,   LEOPARD_HPVOL_NEG_22DB,
	LEOPARD_HPVOL_NEG_21DB,   LEOPARD_HPVOL_NEG_20DB,   LEOPARD_HPVOL_NEG_19DB,   LEOPARD_HPVOL_NEG_18DB,
	LEOPARD_HPVOL_NEG_17DB,   LEOPARD_HPVOL_NEG_16DB,   LEOPARD_HPVOL_NEG_15DB,   LEOPARD_HPVOL_NEG_14DB,
	LEOPARD_HPVOL_NEG_13DB,   LEOPARD_HPVOL_NEG_12DB,   LEOPARD_HPVOL_NEG_11DB,   LEOPARD_HPVOL_NEG_10DB,
	LEOPARD_HPVOL_NEG_9DB,    LEOPARD_HPVOL_NEG_8DB,    LEOPARD_HPVOL_NEG_7DB,    LEOPARD_HPVOL_NEG_6DB,
	LEOPARD_HPVOL_NEG_5DB,    LEOPARD_HPVOL_NEG_4DB,    LEOPARD_HPVOL_NEG_3DB,    LEOPARD_HPVOL_NEG_2DB,
	LEOPARD_HPVOL_NEG_1DB,    LEOPARD_HPVOL_0DB,        LEOPARD_HPVOL_1DB,        LEOPARD_HPVOL_2DB,
	LEOPARD_HPVOL_3DB,        LEOPARD_HPVOL_4DB,        LEOPARD_HPVOL_5DB,        LEOPARD_HPVOL_6DB,
	LEOPARD_HPVOL_MUTE
};


#define PMU_HSGAIN_NUM 	LEOPARD_HPVOL_MUTE
#define PMU_IHFGAIN_NUM	LEOPARD_SPVOL_MUTE


static inline int d2153_hpvol_to_reg(enum d2153_hp_vol_val vol_val)
{
	return (int)vol_val;
}

static inline enum d2153_hp_vol_val d2153_reg_to_hpvol(int reg)
{
	return (enum d2153_hp_vol_val)reg;
}

static inline int d2153_hpvol_to_db(enum d2153_hp_vol_val vol_val)
{
	return (int)vol_val - (int)LEOPARD_HPVOL_0DB;
}

static inline enum d2153_hp_vol_val d2153_db_to_hpvol(int db)
{
	return (enum d2153_hp_vol_val)(db + (int)LEOPARD_HPVOL_0DB);
}

enum d2153_sp_vol_val {
	LEOPARD_SPVOL_NEG_24DB    = 0x1b, LEOPARD_SPVOL_NEG_23DB = 0x1c,
	LEOPARD_SPVOL_NEG_22DB    = 0x1d, LEOPARD_SPVOL_NEG_21DB = 0x1e,
	LEOPARD_SPVOL_NEG_20DB    = 0x1f, LEOPARD_SPVOL_NEG_19DB = 0x20,
	LEOPARD_SPVOL_NEG_18DB    = 0x21, LEOPARD_SPVOL_NEG_17DB = 0x22,
	LEOPARD_SPVOL_NEG_16DB    = 0x23, LEOPARD_SPVOL_NEG_15DB = 0x24,
	LEOPARD_SPVOL_NEG_14DB    = 0x25, LEOPARD_SPVOL_NEG_13DB = 0x26,
	LEOPARD_SPVOL_NEG_12DB    = 0x27, LEOPARD_SPVOL_NEG_11DB = 0x28,
	LEOPARD_SPVOL_NEG_10DB    = 0x29, LEOPARD_SPVOL_NEG_9DB = 0x2a,
	LEOPARD_SPVOL_NEG_8DB     = 0x2b, LEOPARD_SPVOL_NEG_7DB = 0x2c,
	LEOPARD_SPVOL_NEG_6DB     = 0x2d, LEOPARD_SPVOL_NEG_5DB = 0x2e,
	LEOPARD_SPVOL_NEG_4DB     = 0x2f, LEOPARD_SPVOL_NEG_3DB = 0x30,
	LEOPARD_SPVOL_NEG_2DB     = 0x31, LEOPARD_SPVOL_NEG_1DB = 0x32,
	LEOPARD_SPVOL_0DB         = 0x33,
	LEOPARD_SPVOL_1DB         = 0x34, LEOPARD_SPVOL_2DB = 0x35,
	LEOPARD_SPVOL_3DB         = 0x36, LEOPARD_SPVOL_4DB = 0x37,
	LEOPARD_SPVOL_5DB         = 0x38, LEOPARD_SPVOL_6DB = 0x39,
	LEOPARD_SPVOL_7DB         = 0x3a, LEOPARD_SPVOL_8DB = 0x3b,
	LEOPARD_SPVOL_9DB         = 0x3c, LEOPARD_SPVOL_10DB = 0x3d,
	LEOPARD_SPVOL_11DB        = 0x3e, LEOPARD_SPVOL_12DB = 0x3f,
	LEOPARD_SPVOL_MUTE
};

static inline int d2153_spvol_to_reg(enum d2153_sp_vol_val vol_val)
{
	return (int)vol_val;
}

static inline enum d2153_sp_vol_val d2153_reg_to_spvol(int reg)
{
	return ((reg < LEOPARD_SPVOL_NEG_24DB) ? LEOPARD_SPVOL_NEG_24DB :
			(enum d2153_sp_vol_val)reg);
}

static inline int d2153_spvol_to_db(enum d2153_sp_vol_val vol_val)
{
	return (int)vol_val - (int)LEOPARD_SPVOL_0DB;
}

static inline enum d2153_sp_vol_val d2153_db_to_spvol(int db)
{
	return (enum d2153_sp_vol_val)(db + (int)LEOPARD_SPVOL_0DB);
}


enum d2153_audio_output_sel {
	LEOPARD_OUT_NONE  = 0x00,
	LEOPARD_OUT_HPL   = 0x01,
	LEOPARD_OUT_HPR   = 0x02,
	LEOPARD_OUT_HPLR  = LEOPARD_OUT_HPL | LEOPARD_OUT_HPR,
	LEOPARD_OUT_SPKR  = 0x04,
};

struct d2153_audio_platform_data
{
	u8 ina_def_mode;
	u8 inb_def_mode;
	u8 ina_def_preampgain;
	u8 inb_def_preampgain;

	u8 lhs_def_mixer_in;
	u8 rhs_def_mixer_in;
	u8 ihf_def_mixer_in;

	int hs_input_path;
	int ihf_input_path;
};

struct d2153_audio {
	struct platform_device  *pdev;
	bool IHFenabled;
	bool HSenabled;
	u8 hs_pga_gain;
	u8 hs_pre_gain;
	struct timer_list timer;
	struct work_struct work;
	u8 AudioStart; //0=not started, 1=hs starting, 2=ihf starting, 3=started
	u8 micbias1_level;
	u8 micbias2_level;
	u8 micbias3_level;
	u8 ear_detect_enable;
	int aad_jack_debounce_ms;
	int aad_jackout_debounce_ms;
	int aad_button_debounce_ms;
	int aad_button_sleep_debounce_ms;
	bool aad_codec_detect_enable;	/* d2153 detect configuration */
	bool aad_gpio_detect_enable;	/* gpio detect configuration */
	int aad_gpio_port;		/* gpio port for jack detect */
};

int d2153_audio_hs_poweron(bool on);
int d2153_audio_hs_shortcircuit_enable(bool en);
int d2153_audio_hs_set_gain(enum d2153_audio_output_sel hs_path_sel, enum d2153_hp_vol_val hs_gain_val);
int d2153_audio_hs_ihf_poweron(void);
int d2153_audio_hs_ihf_poweroff(void);
int d2153_audio_hs_ihf_enable_bypass(bool en);
int d2153_audio_hs_ihf_set_gain(enum d2153_sp_vol_val ihfgain_val);
int d2153_audio_set_mixer_input(enum d2153_audio_output_sel path_sel, enum d2153_audio_input_val input_val);
int d2153_audio_set_input_mode(enum d2153_input_path_sel inpath_sel, enum d2153_input_mode_val mode_val);
int d2153_audio_set_input_preamp_gain(enum d2153_input_path_sel inpath_sel, enum d2153_preamp_gain_val pagain_val);
int d2153_audio_hs_preamp_gain(enum d2153_preamp_gain_val hsgain_val);
int d2153_audio_ihf_preamp_gain(enum d2153_preamp_gain_val ihfgain_val);
int d2153_audio_enable_zcd(int enable);
int d2153_audio_enable_vol_slew(int enable);
int d2153_set_hs_noise_gate(u16 regval);		
int d2153_set_ihf_noise_gate(u16 regval);
int d2153_set_ihf_none_clip(u16 regval);
int d2153_set_ihf_pwr(u8 regval);
int d2153_sp_set_hi_impedance(u8 set_last_bit);

#endif /* __LINUX_LEOPARD_AUDIO_H */
