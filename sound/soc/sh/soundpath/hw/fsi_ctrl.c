/* fsi_ctrl.c
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

#define __FSI_CTRL_NO_EXTERN__

#include <sound/soundpath/soundpath.h>
#include <sound/soundpath/common_extern.h>
#include <sound/soundpath/fsi_extern.h>
#include "fsi_ctrl.h"
#include <mach/common.h>

/*
 * GLOBAL DATA Definitions
 */

/* 1st path value */
static u_int g_fsi_ui1stPathValue = SNDP_VALUE_INIT;

/* FSI Control functions table */
static struct ctrl_func_tbl g_fsi_func_tbl[] = {
	{ SNDP_PLAYBACK_EARPIECE_NORMAL,                    fsi_playback   },
	{ SNDP_PLAYBACK_EARPIECE_RINGTONE,                  fsi_playback   },
	{ SNDP_PLAYBACK_EARPIECE_INCALL,                    fsi_voicecall  },
	{ SNDP_PLAYBACK_EARPIECE_INCOMMUNICATION,           fsi_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_NORMAL,                     fsi_playback   },
	{ SNDP_PLAYBACK_SPEAKER_RINGTONE,                   fsi_playback   },
	{ SNDP_PLAYBACK_SPEAKER_INCALL,                     fsi_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_INCOMMUNICATION,            fsi_voicecall  },
	{ SNDP_PLAYBACK_BLUETOOTH_NORMAL,                   fsi_playback   },
	{ SNDP_PLAYBACK_BLUETOOTH_RINGTONE,                 fsi_playback   },
	{ SNDP_PLAYBACK_BLUETOOTH_INCALL,                   fsi_voicecall  },
	{ SNDP_PLAYBACK_BLUETOOTH_INCOMMUNICATION,          fsi_voicecall  },
	{ SNDP_PLAYBACK_HEADSET_NORMAL,                     fsi_playback   },
	{ SNDP_PLAYBACK_HEADSET_RINGTONE,                   fsi_playback   },
	{ SNDP_PLAYBACK_HEADSET_INCALL,                     fsi_voicecall  },
	{ SNDP_PLAYBACK_HEADSET_INCOMMUNICATION,            fsi_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_HEADSET_NORMAL,             fsi_playback   },
	{ SNDP_PLAYBACK_SPEAKER_HEADSET_RINGTONE,           fsi_playback   },
	{ SNDP_PLAYBACK_SPEAKER_HEADSET_INCALL,             fsi_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_HEADSET_INCOMMUNICATION,    fsi_voicecall  },
	{ SNDP_PLAYBACK_HEADPHONE_NORMAL,                   fsi_playback   },
	{ SNDP_PLAYBACK_HEADPHONE_RINGTONE,                 fsi_playback   },
	{ SNDP_PLAYBACK_HEADPHONE_INCALL,                   fsi_voicecall  },
	{ SNDP_PLAYBACK_HEADPHONE_INCOMMUNICATION,          fsi_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_HEADPHONE_NORMAL,           fsi_playback   },
	{ SNDP_PLAYBACK_SPEAKER_HEADPHONE_RINGTONE,         fsi_playback   },
	{ SNDP_PLAYBACK_SPEAKER_HEADPHONE_INCALL,           fsi_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_HEADPHONE_INCOMMUNICATION,  fsi_voicecall  },
	{ SNDP_PLAYBACK_AUXDIGITAL_NORMAL,                  fsi_playback   },
	{ SNDP_PLAYBACK_AUXDIGITAL_RINGTONE,                NULL           },
	{ SNDP_PLAYBACK_AUXDIGITAL_INCALL,                  NULL           },
	{ SNDP_PLAYBACK_AUXDIGITAL_INCOMMUNICATION,         NULL           },
	{ SNDP_PLAYBACK_FMTX_NORMAL,                        fsi_playback   },
	{ SNDP_PLAYBACK_FMTX_RINGTONE,                      NULL           },
	{ SNDP_PLAYBACK_FMTX_INCALL,                        NULL           },
	{ SNDP_PLAYBACK_FMTX_INCOMMUNICATION,               NULL           },
	{ SNDP_PLAYBACK_FMRX_SPEAKER_NORMAL,                fsi_playback   },
	{ SNDP_PLAYBACK_FMRX_SPEAKER_RINGTONE,              NULL           },
	{ SNDP_PLAYBACK_FMRX_SPEAKER_INCALL,                NULL           },
	{ SNDP_PLAYBACK_FMRX_SPEAKER_INCOMMUNICATION,       NULL           },
	{ SNDP_PLAYBACK_FMRX_HEADSET_NORMAL,                fsi_playback   },
	{ SNDP_PLAYBACK_FMRX_HEADSET_RINGTONE,              NULL           },
	{ SNDP_PLAYBACK_FMRX_HEADSET_INCALL,                NULL           },
	{ SNDP_PLAYBACK_FMRX_HEADSET_INCOMMUNICATION,       NULL           },
	{ SNDP_PLAYBACK_FMRX_HEADPHONE_NORMAL,              fsi_playback   },
	{ SNDP_PLAYBACK_FMRX_HEADPHONE_RINGTONE,            NULL           },
	{ SNDP_PLAYBACK_FMRX_HEADPHONE_INCALL,              NULL           },
	{ SNDP_PLAYBACK_FMRX_HEADPHONE_INCOMMUNICATION,     NULL           },
	{ SNDP_CAPTURE_MIC_NORMAL,                          fsi_capture    },
	{ SNDP_CAPTURE_MIC_RINGTONE,                        fsi_capture    },
	{ SNDP_CAPTURE_MIC_INCALL,                          NULL           },
	{ SNDP_CAPTURE_MIC_INCOMMUNICATION,                 fsi_voicecall  },
	{ SNDP_CAPTURE_HEADSET_NORMAL,                      fsi_capture    },
	{ SNDP_CAPTURE_HEADSET_RINGTONE,                    fsi_capture    },
	{ SNDP_CAPTURE_HEADSET_INCALL,                      NULL           },
	{ SNDP_CAPTURE_HEADSET_INCOMMUNICATION,             fsi_voicecall  },
	{ SNDP_CAPTURE_BLUETOOTH_NORMAL,                    NULL           },
	{ SNDP_CAPTURE_BLUETOOTH_RINGTONE,                  NULL           },
	{ SNDP_CAPTURE_BLUETOOTH_INCALL,                    NULL           },
	{ SNDP_CAPTURE_BLUETOOTH_INCOMMUNICATION,           fsi_voicecall  },
	{ SNDP_CAPTURE_FMRX_NORMAL,                         fsi_playback   },
	{ SNDP_CAPTURE_FMRX_RINGTONE,                       NULL           },
	{ SNDP_CAPTURE_FMRX_INCALL,                         NULL           },
	{ SNDP_CAPTURE_FMRX_INCOMMUNICATION,                NULL           },
};


/*
 * Register Table Definisions
 */

/* [Table summary] Reg=Register, Val=Value, D=Delay time, C=Clear bit */

#ifdef __SNDP_INCALL_CLKGEN_MASTER
/* Table for Voice call(PortA, FSI slave) */
static struct common_reg_table fsi_reg_tbl_voicecallA_S[] = {
/*	  Reg		Value		D  C */
	/* Bus clock(MP clock) */
	{ FSI_CLK_SEL,	0x00000001,	0, 0 },
	/* 512 fs, 64bit/fs, DIIS:Slave, DOIS:Slave */
	{ FSI_ACK_MD,	0x00000100,	0, 0 },
	/* LRS:Clock not inverted, BRS:Clock inverted */
	{ FSI_ACK_RV,	0x00000001,	0, 0 },
	/* 24bits,	PCM format,	I2S */
	{ FSI_DO_FMT,	0x00000030,	0, 0 },
	/* 24bits,	PCM format,	I2S */
	{ FSI_DI_FMT,	0x00000030,	0, 0 },
	/* MUTE OFF */
	{ FSI_MUTE,	0x00001111,	0, 0 },
};
#else /* !__SNDP_INCALL_CLKGEN_MASTER */

/* Table for Voice call(PortA, FSI master) */
static struct common_reg_table fsi_reg_tbl_voicecallA_M[] = {
/*	  Reg		Value		D  C */
	/* Bus clock(MP clock) */
	{ FSI_CLK_SEL,	0x00000001,	0, 0 },
	/* Divider clock 1 / 12 (49.174 / 12 = 4.098MHz) */
	{ FSI_FSIDIVA,	0x007D0003,	0, 0 },
	/* 256 fs, 64bit/fs, DIIS:Master, DOIS:Master, 16kHz */
	{ FSI_ACK_MD,	0x00004011,	0, 0 },
	/* LRM:Clock not inverted, BRM:Clock inverted */
	{ FSI_ACK_RV,	0x00000100,	0, 0 },
	/* 24bits,	PCM format,	I2S */
	{ FSI_DO_FMT,	0x00100030,	0, 0 },
	/* 24bits,	PCM format,	I2S */
	{ FSI_DI_FMT,	0x00100030,	0, 0 },
	/* MUTE OFF */
	{ FSI_MUTE,	0x00001111,	0, 0 },
	/* Clears the reset(PortA) */
	{ FSI_ACK_RST,	0x00000001,	0, 0x00000001 },
};
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

/* Table for Voice call(PortB, FSI slave) */
static struct common_reg_table fsi_reg_tbl_voicecallB_S[] = {
/*	  Reg					Value		D  C */
	/* Bus clock(MP clock) */
	{ FSI_CLK_SEL,				0x00000001,	0, 0 },
	/* 512 fs, 64bit/fs, DIIS:Slave, DOIS:Slave */
	{ (FSI_ACK_MD + FSI_PORTB_OFFSET),	0x00000100,	0, 0 },
	/* LRS:Clock inverted, BRS:Clock not inverted */
	{ (FSI_ACK_RV + FSI_PORTB_OFFSET),	0x00000000,	0, 0 },
	/* 24bits, PCM format, MONO */
	{ (FSI_DO_FMT + FSI_PORTB_OFFSET),	0x00100010,	0, 0 },
	/* 24bits, PCM format, MONO */
	{ (FSI_DI_FMT + FSI_PORTB_OFFSET),	0x00100010,	0, 0 },
	/* MUTE OFF */
	{ FSI_MUTE,				0x00001111,	0, 0 },
};

/* Table for Playback(PortA, FSI master) */
static struct common_reg_table fsi_reg_tbl_playA_M[] = {
/*	  Reg		Value		D  C */
	/* Bus clock(MP clock) */
	{ FSI_CLK_SEL,	0x00000001,	0, 0 },
	/* Divider clock 1 / 73 (112.125 / 73 = 1.53MHz) */
	{ FSI_FSIDIVA,	0x00490003,	0, 0 },
	/* 32 fs, 16bit/fs, DIIS:Slave, DOIS:Master, 47.998kHz(1.53M/32) */
	{ FSI_ACK_MD,	0x00004001,	0, 0x00004101 },
	/* LRM:Clock not inverted, BRM:Clock inverted */
	{ FSI_ACK_RV,	0x00000100,	0, 0 },
	/* 16bits, PCM format, I2S */
	{ FSI_DO_FMT,	0x00100030,	0, 0 },
	/* MUTE OFF */
	{ FSI_MUTE,	0x00001111,	0, 0 },
	/* Clears the reset(PortA) */
	{ FSI_ACK_RST,	0x00000001,	0, 0x00000001 },
};

/* Table for Playback(PortB, FSI master) */
static struct common_reg_table fsi_reg_tbl_playB_M[] = {
/*	  Reg					Value		D  C */
	/* Bus clock(MP clock) */
	{ FSI_CLK_SEL,				0x00000001,	0, 0 },
	/* Divider clock 1 / 73 (112.125 / 73 = 1.53MHz) */
	{ FSI_FSIDIVB,				0x00490003,	0, 0 },
	/* 32 fs, 16bit/fs, DIIS:Slave, DOIS:Master, 47.998kHz(1.53M/32) */
	{ (FSI_ACK_MD + FSI_PORTB_OFFSET),	0x00004001,	0, 0 },
	/* LRM:Clock not inverted, BRM:Clock inverted */
	{ (FSI_ACK_RV + FSI_PORTB_OFFSET),	0x00000100,	0, 0 },
	/* 16bits, PCM format, I2S */
	{ (FSI_DO_FMT + FSI_PORTB_OFFSET),	0x00100030,	0, 0 },
	/* MUTE OFF */
	{ FSI_MUTE,				0x00001111,	0, 0 },
	/* Clears the reset(PortB) */
	{ FSI_ACK_RST,				0x00000010,	0, 0x00000010 },
};

/* Table for Capture(PortA, FSI master) */
static struct common_reg_table fsi_reg_tbl_captureA_M[] = {
/*	  Reg		Value		D  C */
	/* Bus clock(MP clock) */
	{ FSI_CLK_SEL,	0x00000001,	0, 0 },
	/* Divider clock 1 / 73 (112.125 / 73 = 1.53MHz) */
	{ FSI_FSIDIVA,	0x00490003,	0, 0 },
	/* 32 fs, 16bit/fs, DIIS:Master, DOIS:Slave, 47.998kHz(1.53M/32) */
	{ FSI_ACK_MD,	0x00004010,	0, 0 },
	/* LRM:Clock not inverted, BRM:Clock inverted */
	{ FSI_ACK_RV,	0x00000100,	0, 0 },
	/* 32bits, PCM format, I2S */
	{ FSI_DI_FMT,	0x00100030,	0, 0 },
	/* MUTE OFF */
	{ FSI_MUTE,	0x00001111,	0, 0 },
	/* Clears the reset(PortA) */
	{ FSI_ACK_RST,	0x00000001,	0, 0x00000001 },
};

/* Table for Capture(PortB, FSI master) */
static struct common_reg_table fsi_reg_tbl_captureB_M[] = {
/*	  Reg					Value		D  C */
	/* Bus clock(MP clock) */
	{ FSI_CLK_SEL,				0x00000001,	0, 0 },
	/* Divider clock 1 / 73 (112.125 / 73 = 1.53MHz) */
	{ FSI_FSIDIVB,				0x00490003,	0, 0 },
	/* 32 fs, 16bit/fs, DIIS:Master, DOIS:Slave, 47.998kHz(1.53/32) */
	{ (FSI_ACK_MD + FSI_PORTB_OFFSET),	0x00004010,	0, 0 },
	/* LRM:Clock not inverted, BRM:Clock inverted */
	{ (FSI_ACK_RV + FSI_PORTB_OFFSET),	0x00000100,	0, 0 },
	/* 16bits, PCM format, I2S */
	{ (FSI_DI_FMT + FSI_PORTB_OFFSET),	0x00100030,	0, 0 },
	/* MUTE OFF */
	{ FSI_MUTE,				0x00001111,	0, 0 },
	/* Clears the reset(PortB) */
	{ FSI_ACK_RST,				0x00000010,	0, 0x00000010 },
};

/* Table for DownLink Mute ON */
static struct common_reg_table fsi_reg_tbl_dl_mute_on[] = {
/*	  Reg					Value		D  C */
	/* MUTE OFF */
	{ FSI_MUTE,				0x00001313,	0, 0 },
};

/* Table for DownLink Mute OFF */
static struct common_reg_table fsi_reg_tbl_dl_mute_off[] = {
/*	  Reg					Value		D  C */
	/* MUTE OFF */
	{ FSI_MUTE,				0x00001111,	0, 0 },
};

/*!
   @brief FSI start function

   @param[in]   uiValue     PCM type
   @param[out]  none

   @retval  0           Successful
   @retval  -EINVAL     Invalid argument
 */
int fsi_start(const u_int uiValue, const u_int uiRegClr)
{
	/* Local variable declaration */
	int iCnt;

	sndp_log_debug_func("start\n");
	sndp_log_info("[0x%08x]\n", uiValue);

	/* Call of function of each PATH */
	for (iCnt = 0; ARRAY_SIZE(g_fsi_func_tbl) > iCnt; iCnt++) {
		/* uiValue check */
		if (uiValue == g_fsi_func_tbl[iCnt].uiValue) {
			/* Function pointer check */
			if (NULL != g_fsi_func_tbl[iCnt].func) {
				g_fsi_ui1stPathValue = uiValue;
				/* Clock framework API, Status ON */
				audio_ctrl_func(SNDP_HW_FSI, STAT_ON, uiRegClr);
				/* Path setting API call */
				g_fsi_func_tbl[iCnt].func(uiValue);
			}
			fsi_reg_dump(g_fsi_ui1stPathValue);
			sndp_log_debug_func("end\n");
			return ERROR_NONE;
		}
	}

	sndp_log_err("UnSupported value error end\n");
	return -EINVAL;
}


/*!
   @brief FSI stop function

   @param[in]	regclr	Regisetr clear bit
   @param[out]	none

   @retval	none
 */
void fsi_stop(const u_int regclr)
{
	sndp_log_debug_func("start\n");

	fsi_reg_dump(g_fsi_ui1stPathValue);

	/* Clock framework API, Status OFF */
	audio_ctrl_func(SNDP_HW_FSI, STAT_OFF, regclr);

	/* PCM clear */
	g_fsi_ui1stPathValue = SNDP_VALUE_INIT;

	sndp_log_debug_func("end\n");
}


/*!
   @brief FSI registers setting (Voice call)

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void fsi_voicecall(const u_int uiValue)
{
	/*************************************************
	 * PATH : SNDP_PLAYBACK_EARPIECE_INCALL,
	 *        SNDP_PLAYBACK_SPEAKER_INCALL,
	 *        SNDP_PLAYBACK_BLUETOOTH_INCALL,
	 *        SNDP_PLAYBACK_HEADSET_INCALL,
	 *        SNDP_PLAYBACK_SPEAKER_HEADSET_INCALL,
	 *        SNDP_PLAYBACK_HEADPHONE_INCALL,
	 *        SNDP_PLAYBACK_SPEAKER_HEADPHONE_INCALL,
	 * FSI           : Slave
	 * MAXIM         : Slave
	 * CLKGEN        : Master
	 * Port          : PortA or B
	 * Ch            : 2
	 * Sampling rate : 16000
	 * Adjust mode   : OFF
	 *************************************************/

	/* Local variable declaration */
	u_int			dev		= 0;
	struct common_reg_table	*reg_tbl	= NULL;
	u_int			tbl_size	= 0;

	sndp_log_debug_func("start\n");

	/* Device check */
	dev = SNDP_GET_DEVICE_VAL(uiValue);
	/* SPEAKER, EARPIECE, WIREDHEADSET, WIREDHEADPHONE */
	if ((false == (dev & SNDP_BLUETOOTHSCO)) &&
	    (false == (dev & SNDP_FM_RADIO_TX)) &&
	    (false == (dev & SNDP_FM_RADIO_RX))) {
#ifdef __SNDP_INCALL_CLKGEN_MASTER
		/* CLKGEN master */
		reg_tbl  = fsi_reg_tbl_voicecallA_S;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_voicecallA_S);
#else /* !__SNDP_INCALL_CLKGEN_MASTER */
		/* FSI master */
		reg_tbl  = fsi_reg_tbl_voicecallA_M;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_voicecallA_M);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */
	/* BLUETOOTHSCO */
	} else {
		/* CLKGEN master */
		reg_tbl  = fsi_reg_tbl_voicecallB_S;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_voicecallB_S);
	}

	/* Register setting function call */
	common_set_register(SNDP_HW_FSI, reg_tbl, tbl_size);

	sndp_log_debug_func("end\n");
}


/*!
   @brief FSI registers setting (Playback)

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void fsi_playback(const u_int uiValue)
{
	/********************************************
	 * PATH : SNDP_PLAYBACK_XXXXX_NORMAL
	 *        SNDP_PLAYBACK_XXXXX_RINGTONE
	 *        SNDP_PLAYBACK_XXXXX_INCOMMUNICATION
	 * FSI           : Slave
	 * MAXIM         : Slave
	 * CLKGEN        : Master
	 * Port          : PortA or B
	 * Ch            : 2
	 * Sampling rate : 44100
	 ********************************************/

	/* Local variable declaration */
	u_int			dev		= 0;
	struct common_reg_table	*reg_tbl	= NULL;
	u_int			tbl_size	= 0;
	void __iomem *diff_st_reg = g_fsi_Base + FSI_PORTB_OFFSET + FSI_DIFF_ST;
	u_int	wait_cnt = 0;

	sndp_log_debug_func("start\n");

	/* Device check */
	dev = SNDP_GET_DEVICE_VAL(uiValue);
	/* SPEAKER, EARPIECE, WIREDHEADSET, WIREDHEADPHONE, AUXDIGITAL(HDMI) */
	if ((false == (dev & SNDP_BLUETOOTHSCO)) &&
	    (false == (dev & SNDP_FM_RADIO_TX)) &&
	    (false == (dev & SNDP_FM_RADIO_RX))) {
		/* FSI master */
		reg_tbl  = fsi_reg_tbl_playA_M;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_playA_M);
	/* FM_RADIO_RX */
	} else if (false != (dev & SNDP_FM_RADIO_RX)) {
		/* FSI master */
		reg_tbl  = fsi_reg_tbl_captureB_M;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_captureB_M);
	/* FM_RADIO_TX */
	} else {
		/* FSI master */
		reg_tbl  = fsi_reg_tbl_playB_M;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_playB_M);
	}

	/* Register setting function call */
	common_set_register(SNDP_HW_FSI, reg_tbl, tbl_size);

	/* Add setting for FM_RADIO_RX */
	if (false != (dev & SNDP_FM_RADIO_RX)) {
		/* FSI master */
		reg_tbl  = fsi_reg_tbl_playA_M;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_playA_M);

		wait_cnt = FSI_DIFF_ST_WAIT_COUNT;
		while (wait_cnt--) {
			if (FSI_DIFF_ST_WAIT_SIZE <
			((0x0000ff00 & ioread32(diff_st_reg)) >> 8))
				break;

			udelay(FSI_DIFF_ST_WAIT_TIME);
		}

		sndp_log_info("OUT FSI_DIFF_ST[0x%08x] wait_cnt[%d]\n",
				ioread32(diff_st_reg), wait_cnt);

		/* Register setting function call */
		common_set_register(SNDP_HW_FSI, reg_tbl, tbl_size);
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief FSI registers setting (Capture)

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void fsi_capture(const u_int uiValue)
{
	/********************************************
	 * PATH : SNDP_CAPTURE_MIC_NORMAL,
	 *        SNDP_CAPTURE_MIC_RINGTONE,
	 *        SNDP_CAPTURE_MIC_INCOMMUNICATION,
	 *        SNDP_CAPTURE_HEADSET_NORMAL,
	 *        SNDP_CAPTURE_HEADSET_RINGTONE,
	 *        SNDP_CAPTURE_HEADSET_INCOMMUNICATION,
	 *        SNDP_CAPTURE_FMRX_NORMAL,
	 * FSI           : Slave
	 * MAXIM         : Slave
	 * CLKGEN        : Master
	 * Port          : PortA or B
	 * Ch            : 2
	 * Sampling rate : 44100
	 ********************************************/

	/* Local variable declaration */
	u_int			dev		= 0;
	struct common_reg_table	*reg_tbl	= NULL;
	u_int			tbl_size	= 0;

	sndp_log_debug_func("start\n");

	/* Device check */
	dev = SNDP_GET_DEVICE_VAL(uiValue);
	/* MIC, WIREDHEADSET */
	if ((false == (dev & SNDP_BLUETOOTHSCO)) &&
	    (false == (dev & SNDP_FM_RADIO_TX)) &&
	    (false == (dev & SNDP_FM_RADIO_RX))) {
		/* FSI master */
		reg_tbl  = fsi_reg_tbl_captureA_M;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_captureA_M);
	/* FM_RADIO_RX */
	} else {
		/* FSI master */
		reg_tbl  = fsi_reg_tbl_captureB_M;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_captureB_M);
	}

	/* Register setting function call */
	common_set_register(SNDP_HW_FSI, reg_tbl, tbl_size);

	sndp_log_debug_func("end\n");
}


/*!
   @brief DMA process start function

   @param[in]
   @param[out]

   @retval
 */
void fsi_dma_start(void)
{
	sndp_log_debug_func("start\n");

	/* T.B.D. */

	sndp_log_debug_func("end\n");
}


/*!
   @brief DMA process stop function

   @param[in]
   @param[out]

   @retval
 */
void fsi_dma_stop(void)
{
	sndp_log_debug_func("start\n");

	/* T.B.D. */

	sndp_log_debug_func("end\n");
}


/*!
   @brief FSI registers dump

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void fsi_reg_dump(const u_int uiValue)
{
	u_char __iomem *portb_base;
	sndp_log_reg_dump("===== FSI Registers Dump Start =====\n");

	sndp_log_reg_dump("\n<PortA>\n");
	sndp_log_reg_dump("DO_FMT     [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DO_FMT),
			(g_fsi_Base + FSI_DO_FMT));
	sndp_log_reg_dump("DOFF_CTL   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DOFF_CTL),
			(g_fsi_Base + FSI_DOFF_CTL));
	sndp_log_reg_dump("DOFF_ST    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DOFF_ST),
			(g_fsi_Base + FSI_DOFF_ST));
	sndp_log_reg_dump("DI_FMT     [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DI_FMT),
			(g_fsi_Base + FSI_DI_FMT));
	sndp_log_reg_dump("DIFF_CTL   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DIFF_CTL),
			(g_fsi_Base + FSI_DIFF_CTL));
	sndp_log_reg_dump("DIFF_ST    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DIFF_ST),
			(g_fsi_Base + FSI_DIFF_ST));
	sndp_log_reg_dump("ACK_MD     [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_ACK_MD),
			(g_fsi_Base + FSI_ACK_MD));
	sndp_log_reg_dump("ACK_RV     [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_ACK_RV),
			(g_fsi_Base + FSI_ACK_RV));
/*		sndp_log_reg_dump("DIDT       [%08X][%p]\n",
				ioread32(g_fsi_Base + FSI_DIDT),
				(g_fsi_Base + FSI_DIDT)); */
/*		sndp_log_reg_dump("DODT       [%08X][%p]\n",
				ioread32(g_fsi_Base + FSI_DODT),
				(g_fsi_Base + FSI_DODT)); */
	sndp_log_reg_dump("MUTE_ST    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_MUTE_ST),
			(g_fsi_Base + FSI_MUTE_ST));
	sndp_log_reg_dump("OUT_DMAC   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_OUT_DMAC),
			(g_fsi_Base + FSI_OUT_DMAC));
	sndp_log_reg_dump("OUT_SEL    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_OUT_SEL),
			(g_fsi_Base + FSI_OUT_SEL));
	sndp_log_reg_dump("OUT_SPST   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_OUT_SPST),
			(g_fsi_Base + FSI_OUT_SPST));
	sndp_log_reg_dump("IN_DMAC    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_IN_DMAC),
			(g_fsi_Base + FSI_IN_DMAC));
	sndp_log_reg_dump("IN_SEL     [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_IN_SEL),
			(g_fsi_Base + FSI_IN_SEL));

	portb_base = g_fsi_Base + FSI_PORTB_OFFSET;

	sndp_log_reg_dump("\n<PortB>\n");
	sndp_log_reg_dump("DO_FMT     [%08X][%p]\n",
			ioread32(portb_base + FSI_DO_FMT),
			(portb_base + FSI_DO_FMT));
	sndp_log_reg_dump("DOFF_CTL   [%08X][%p]\n",
			ioread32(portb_base + FSI_DOFF_CTL),
			(portb_base + FSI_DOFF_CTL));
	sndp_log_reg_dump("DOFF_ST    [%08X][%p]\n",
			ioread32(portb_base + FSI_DOFF_ST),
			(portb_base + FSI_DOFF_ST));
	sndp_log_reg_dump("DI_FMT     [%08X][%p]\n",
			ioread32(portb_base + FSI_DI_FMT),
			(portb_base + FSI_DI_FMT));
	sndp_log_reg_dump("DIFF_CTL   [%08X][%p]\n",
			ioread32(portb_base + FSI_DIFF_CTL),
			(portb_base + FSI_DIFF_CTL));
	sndp_log_reg_dump("DIFF_ST    [%08X][%p]\n",
			ioread32(portb_base + FSI_DIFF_ST),
			(portb_base + FSI_DIFF_ST));
	sndp_log_reg_dump("ACK_MD     [%08X][%p]\n",
			ioread32(portb_base + FSI_ACK_MD),
			(portb_base + FSI_ACK_MD));
	sndp_log_reg_dump("ACK_RV     [%08X][%p]\n",
			ioread32(portb_base + FSI_ACK_RV),
			(portb_base + FSI_ACK_RV));
/*	sndp_log_reg_dump("DIDT       [%08X][%p]\n",
			ioread32(portb_base + FSI_DIDT),
			(portb_base + FSI_DIDT)); */
/*	sndp_log_reg_dump("DODT       [%08X][%p]\n",
			ioread32(portb_base + FSI_DODT),
			(portb_base + FSI_DODT)); */
	sndp_log_reg_dump("MUTE_ST    [%08X][%p]\n",
			ioread32(portb_base + FSI_MUTE_ST),
			(portb_base + FSI_MUTE_ST));
	sndp_log_reg_dump("OUT_DMAC   [%08X][%p]\n",
			ioread32(portb_base + FSI_OUT_DMAC),
			(portb_base + FSI_OUT_DMAC));
	sndp_log_reg_dump("OUT_SEL    [%08X][%p]\n",
			ioread32(portb_base + FSI_OUT_SEL),
			(portb_base + FSI_OUT_SEL));
	sndp_log_reg_dump("OUT_SPST   [%08X][%p]\n",
			ioread32(portb_base + FSI_OUT_SPST),
			(portb_base + FSI_OUT_SPST));
	sndp_log_reg_dump("IN_DMAC    [%08X][%p]\n",
			ioread32(portb_base + FSI_IN_DMAC),
			(portb_base + FSI_IN_DMAC));
	sndp_log_reg_dump("IN_SEL     [%08X][%p]\n",
			ioread32(portb_base + FSI_IN_SEL),
			(portb_base + FSI_IN_SEL));

	sndp_log_reg_dump("\n<Common>\n");
	sndp_log_reg_dump("TMR_CTL    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_TMR_CTL),
			(g_fsi_Base + FSI_TMR_CTL));
	sndp_log_reg_dump("TMR_CLR    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_TMR_CLR),
			(g_fsi_Base + FSI_TMR_CLR));
	sndp_log_reg_dump("INT_SEL    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_INT_SEL),
			(g_fsi_Base + FSI_INT_SEL));
	sndp_log_reg_dump("INT_CLR    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_INT_CLR),
			(g_fsi_Base + FSI_INT_CLR));
	sndp_log_reg_dump("CPU_INT_ST [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_CPU_INT_ST),
			(g_fsi_Base + FSI_CPU_INT_ST));
	sndp_log_reg_dump("CPU_IEMSK  [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_CPU_IEMSK),
			(g_fsi_Base + FSI_CPU_IEMSK));
	sndp_log_reg_dump("CPU_IMSK   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_CPU_IMSK),
			(g_fsi_Base + FSI_CPU_IMSK));
	sndp_log_reg_dump("DSP_INT_ST [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DSP_INT_ST),
			(g_fsi_Base + FSI_DSP_INT_ST));
	sndp_log_reg_dump("DSP_IEMSK  [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DSP_IEMSK),
			(g_fsi_Base + FSI_DSP_IEMSK));
	sndp_log_reg_dump("DSP_IMSK   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DSP_IMSK),
			(g_fsi_Base + FSI_DSP_IMSK));
	sndp_log_reg_dump("MUTE       [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_MUTE),
			(g_fsi_Base + FSI_MUTE));
	sndp_log_reg_dump("ACK_RST    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_ACK_RST),
			(g_fsi_Base + FSI_ACK_RST));
	sndp_log_reg_dump("SOFT_RST   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_SOFT_RST),
			(g_fsi_Base + FSI_SOFT_RST));
	sndp_log_reg_dump("FIFO_SZ    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_FIFO_SZ),
			(g_fsi_Base + FSI_FIFO_SZ));
	sndp_log_reg_dump("CLK_SEL    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_CLK_SEL),
			(g_fsi_Base + FSI_CLK_SEL));
	sndp_log_reg_dump("SWAP_SEL   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_SWAP_SEL),
			(g_fsi_Base + FSI_SWAP_SEL));
	sndp_log_reg_dump("HPB_SRST   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_HPB_SRST),
			(g_fsi_Base + FSI_HPB_SRST));
	sndp_log_reg_dump("FSIDIVA    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_FSIDIVA),
			(g_fsi_Base + FSI_FSIDIVA));
	sndp_log_reg_dump("FSIDIVB    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_FSIDIVB),
			(g_fsi_Base + FSI_FSIDIVB));

	sndp_log_reg_dump("\n===== FSI Registers Dump End =====\n");
}


/*!
   @brief FSI soft reset function(CPG)

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void fsi_soft_reset(void)
{
	sndp_log_debug_func("start\n");

	audio_ctrl_func(SNDP_HW_FSI, STAT_ON, 1);

	audio_ctrl_func(SNDP_HW_FSI, STAT_OFF, 1);

	sndp_log_debug_func("end\n");
}


/*!
   @brief All down link mute control

   @param[in]	mute	true / false
   @param[out]	none

   @retval	none
 */
void fsi_all_dl_mute_ctrl(bool mute)
{
	struct common_reg_table	*reg_tbl	= NULL;
	u_int			tbl_size	= 0;

	sndp_log_debug_func("start\n");
	sndp_log_info("mute=%s\n", (false == mute) ? "false" : "true");


	if (false == mute) {
		/* Mute Off */
		reg_tbl  = fsi_reg_tbl_dl_mute_off;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_dl_mute_off);
	} else {
		/* Mute On */
		reg_tbl  = fsi_reg_tbl_dl_mute_on;
		tbl_size = ARRAY_SIZE(fsi_reg_tbl_dl_mute_on);
	}

	common_set_register(SNDP_HW_FSI, reg_tbl, tbl_size);

	sndp_log_debug_func("end\n");
}

void fsi_fifo_reset(int port)
{
	void __iomem *doff = NULL;
	void __iomem *diff = NULL;
	u_long r_doff = 0x0;
	u_long r_diff = 0x0;
	int iCnt = 0;


	if (SNDP_PCM_PORTA == port) {
		doff = g_fsi_Base + FSI_DOFF_CTL;
		diff = g_fsi_Base + FSI_DIFF_CTL;
	} else {
		doff = g_fsi_Base + FSI_PORTB_OFFSET + FSI_DOFF_CTL;
		diff = g_fsi_Base + FSI_PORTB_OFFSET + FSI_DIFF_CTL;
	}

	/* Reg write */
	sh_modify_register32(doff, 0, 0x00000001);
	sh_modify_register32(diff, 0, 0x00000001);

	for (iCnt = 0; iCnt < FSI_RESTET_RETRY_MAX; iCnt++) {
		/* FIFO reset check */
		r_doff = ioread32(doff);
		r_diff = ioread32(diff);

		if ((!(0x00000001 & r_doff)) && (!(0x00000001 & r_diff)))
			break;

		udelay(100);
	}
}

#ifdef SOUND_TEST

static int fsi_test_status_a = TEST_NONE;
static char *fsi_play_test_buf;
static u_int fsi_play_test_buf_len;
static u_int fsi_play_test_byte_offset;

/* fsi play test function */
int fsi_play_test_start_a(char *buf, u_int size)
{
	/* Local variable declaration */
	int ret;
	u32 reg;

	/* FSI initialization */
	ret = fsi_test_init_a();
	if (RET_NONE != ret)
		return RET_NG;

	/* Set play information */
	fsi_play_test_buf = buf;
	fsi_play_test_buf_len = size;
	fsi_play_test_byte_offset = 0;

	/* FIFO Clear */
	iowrite32(0x00100001, (g_fsi_Base + FSI_DOFF_CTL));

	/* Setting valid data, Package in the back */
	iowrite32(0x00000010, (g_fsi_Base + FSI_OUT_DMAC));

	iowrite32(0x00000100, (g_fsi_Base + FSI_ACK_MD));
	iowrite32(0x00000001, (g_fsi_Base + FSI_ACK_RV));
	iowrite32(0x00000030, (g_fsi_Base + FSI_DO_FMT));
	iowrite32(0x00001111, (g_fsi_Base + FSI_MUTE));

	iowrite32(0x00000001, (g_fsi_Base + FSI_ACK_RST));

	/* IR reset */
	reg = ioread32(g_fsi_Base + FSI_SOFT_RST);
	reg &= ~(1 << 4);
	iowrite32(reg, (g_fsi_Base + FSI_SOFT_RST));

	reg = ioread32(g_fsi_Base + FSI_SOFT_RST);
	reg |= (1 << 4);
	iowrite32(reg, (g_fsi_Base + FSI_SOFT_RST));

#ifndef NO_INTURRUPT
	/* Fifo write */
	fsi_test_fifo_write_a();
#endif /* NO_INTURRUPT */

	reg = ioread32(g_fsi_Base + FSI_CPU_INT_ST);
	reg &= ~(1 << 0);
	reg |= 0 & (1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_INT_ST));

	/* FSI start */
	fsi_test_status_a = TEST_PLAY;

	fsi_test_reg_dump();

#ifdef NO_INTURRUPT
	udelay(500);
#endif /* NO_INTURRUPT */

	return RET_NONE;
}

/* fsi play test stop function */
void fsi_play_test_stop_a(void)
{
	fsi_test_reg_dump();

	/* FSI stop */
	fsi_test_stop_a();
}


static char *fsi_rec_test_buf;
static u_int fsi_rec_test_buf_len;
static u_int fsi_rec_test_byte_offset;

/* fsi rec test function */
int fsi_rec_test_start_a(char *buf, u_int size)
{
	/* Local variable declaration */
	int ret;
	u32 reg;

	/* FSI initialization */
	ret = fsi_test_init_a();
	if (RET_NONE != ret)
		return RET_NG;

	/* Set rec information */
	fsi_rec_test_buf = buf;
	fsi_rec_test_buf_len = size;
	fsi_rec_test_byte_offset = 0;

	/* FIFO Clear */
	iowrite32(0x00100001, (g_fsi_Base + FSI_DIFF_CTL));

	/* Setting valid data, Package in the back */
	iowrite32(0x00000010, (g_fsi_Base + FSI_IN_DMAC));

	iowrite32(0x00000100, (g_fsi_Base + FSI_ACK_MD));
	iowrite32(0x00000001, (g_fsi_Base + FSI_ACK_RV));
	iowrite32(0x00000030, (g_fsi_Base + FSI_DI_FMT));
	iowrite32(0x00001111, (g_fsi_Base + FSI_MUTE))

	iowrite32(0x00000001, (g_fsi_Base + FSI_ACK_RST));

	/* IR reset */
	reg = ioread32(g_fsi_Base + FSI_SOFT_RST);
	reg &= ~(1 << 4);
	iowrite32(reg, (g_fsi_Base + FSI_SOFT_RST));

	reg = ioread32(g_fsi_Base + FSI_SOFT_RST);
	reg |= (1 << 4);
	iowrite32(reg, (g_fsi_Base + FSI_SOFT_RST));

#ifndef NO_INTURRUPT
	/* Fifo read */
	fsi_test_fifo_read_a();
#endif /* NO_INTURRUPT */

	reg = ioread32(g_fsi_Base + FSI_CPU_INT_ST);
	reg &= ~(1 << 0);
	reg |= 0 & (1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_INT_ST));

	/* FSI start */
	fsi_test_status_a = TEST_REC;

	fsi_test_reg_dump();

#ifdef NO_INTURRUPT
	udelay(500);
#endif /* NO_INTURRUPT */

	return RET_NONE;
}

/* fsi rec test stop function */
int fsi_rec_test_stop_a(void)
{
	int size;

	fsi_test_reg_dump();

	/* FSI stop */
	fsi_test_stop_a();

	size = fsi_rec_test_byte_offset;

	return size;
}

/* fsi test initialization function */
int fsi_test_init_a()
{
	int reg;

	/* Already FSI started */
	if (TEST_NONE != fsi_test_status_a) {
		/* Simultaneous operation is to expand later. */
		return RET_NG;
	}

	/* Clock framework API, Status ON */
	audio_ctrl_func(SNDP_HW_FSI, STAT_ON, 1);

	/* IRQ Disable */
	reg = ioread32(g_fsi_Base + FSI_CPU_IMSK);
	reg &= ~(1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_IMSK));

	reg = ioread32(g_fsi_Base + FSI_CPU_IEMSK);
	reg &= ~(1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_IEMSK));

	reg = ioread32(g_fsi_Base + FSI_CPU_INT_ST);
	reg &= ~(1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_INT_ST));

	return RET_NONE;
}

/* fsi test fifo write function */
void fsi_test_fifo_write_a(void)
{
	/* Local variable declaration */
	int i;
	int reg;
	int send;
	int residue;
	int fifo_free;
	int fifo_max = 128;
	u8 *start;

#ifdef NO_INTURRUPT
while (1) {
#endif /* NO_INTURRUPT */

	/* Get send num */
	send = (fsi_play_test_buf_len - fsi_play_test_byte_offset) / 2;

	/* Get FIFO free size */
	reg = ioread32(g_fsi_Base + FSI_DOFF_ST);
	residue = 0x1ff & (reg >> 8);
	fifo_free = fifo_max - residue;

	/* Size check */
	if ((fifo_free * 4 / 2) < send)
		send = (fifo_free * 4 / 2);

	start = fsi_play_test_buf;
	start += fsi_play_test_byte_offset;

	for (i = 0; i < send; i++)
		iowrite32(((u32)*((u16 *)start + i) << 8),
			  (g_fsi_Base + FSI_DODT));

	fsi_play_test_byte_offset += (send * 2);

	/* All data complete */
	if (0 >= (fsi_play_test_buf_len - fsi_play_test_byte_offset)) {
		fsi_play_test_stop_a();
		return;
	}

#ifdef NO_INTURRUPT
	udelay(500);
}
#endif /* NO_INTURRUPT */

	/* irq_enable */
	reg = ioread32(g_fsi_Base + FSI_CPU_IMSK);
	reg |= (1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_IMSK));

	reg = ioread32(g_fsi_Base + FSI_CPU_IEMSK);
	reg |= (1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_IEMSK));

}

/* fsi test fifo read function */
void fsi_test_fifo_read_a(void)
{
	/* Local variable declaration */
	int i;
	int reg;
	int read;
	int fifo_fill;
	u8 *start;

#ifdef NO_INTURRUPT
	int count = 0;
while (1) {
#endif /* NO_INTURRUPT */

	/* Get read num */
	read = (fsi_rec_test_buf_len - fsi_rec_test_byte_offset) / 2;

	/* Get FIFO free size */
	reg = ioread32(g_fsi_Base + FSI_DIFF_ST);
	fifo_fill = 0x1ff & (reg >> 8);

	/* Size check */
	if ((fifo_fill * 4 / 2) < read)
		read = (fifo_fill * 4 / 2);

	start = fsi_rec_test_buf;
	start += fsi_rec_test_byte_offset;

	for (i = 0; i < read; i++) {
		*((u16 *)start + i) =
			(u16)(ioread32(g_fsi_Base + FSI_DIDT) >> 8);
		/*printk("%x\n", *((u16 *)start + i)); */
	}
	fsi_rec_test_byte_offset += (read * 2);

	/* All data complete */
	if (0 >= (fsi_rec_test_buf_len - fsi_rec_test_byte_offset)) {
		fsi_rec_test_stop_a();
		return;
	}

#ifdef NO_INTURRUPT
	udelay(500);
	count++;
}
#endif /* NO_INTURRUPT */

	/* irq_enable */
	reg = ioread32(g_fsi_Base + FSI_CPU_IMSK);
	reg |= (1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_IMSK));

	reg = ioread32(g_fsi_Base + FSI_CPU_IEMSK);
	reg |= (1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_IEMSK));

}

/* fsi test interrupt function */
void fsi_test_interrupt_a(void)
{
	/* Local variable declaration */
	u_int reg;

	reg = ioread32(g_fsi_Base + FSI_SOFT_RST);
	reg &= ~(1 << 4);
	iowrite32(reg, (g_fsi_Base + FSI_SOFT_RST));

	reg = ioread32(g_fsi_Base + FSI_SOFT_RST);
	reg &= ~(1 << 4);
	reg |= (1 << 4);
	iowrite32(reg, (g_fsi_Base + FSI_SOFT_RST));

	if (TEST_PLAY == fsi_test_status_a)
		fsi_test_fifo_write_a();
	else
		fsi_test_fifo_read_a();

	reg = ioread32(g_fsi_Base + FSI_CPU_INT_ST);
	reg &= ~(1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_INT_ST));
}

/* fsi test stop function */
void fsi_test_stop_a(void)
{
	/* Local variable declaration */
	u_int reg;

	/* Already FSI stoped */
	if (TEST_NONE == fsi_test_status_a) {
		/* Simultaneous operation with
		 * the Capture is to expand later. */
		return;
	}

	/* IRQ Disable */
	reg = ioread32(g_fsi_Base + FSI_CPU_IMSK);
	reg &= ~(1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_IMSK));

	reg = ioread32(g_fsi_Base + FSI_CPU_IEMSK);
	reg &= ~(1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_IEMSK));

	reg = ioread32(g_fsi_Base + FSI_CPU_INT_ST);
	reg &= ~(1 << 0);
	iowrite32(reg, (g_fsi_Base + FSI_CPU_INT_ST));

	/* 3msec wait */
	mdelay(3);

	/* Clock framework API, Status OFF */
	audio_ctrl_func(SNDP_HW_FSI, STAT_OFF, 1);

	/* FSI stop */
	fsi_test_status_a = TEST_NONE;
}

void fsi_test_reg_dump(void)
{
	printk(KERN_INFO "===== FSI Registers Dump Start =====\n");

	printk(KERN_INFO "\n<PortA>\n");
	printk(KERN_INFO "DO_FMT     [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DO_FMT),
			(g_fsi_Base + FSI_DO_FMT));
	printk(KERN_INFO "DOFF_CTL   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DOFF_CTL),
			(g_fsi_Base + FSI_DOFF_CTL));
	printk(KERN_INFO "DOFF_ST    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DOFF_ST),
			(g_fsi_Base + FSI_DOFF_ST));
	printk(KERN_INFO "DI_FMT     [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DI_FMT),
			(g_fsi_Base + FSI_DI_FMT));
	printk(KERN_INFO "DIFF_CTL   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DIFF_CTL),
			(g_fsi_Base + FSI_DIFF_CTL));
	printk(KERN_INFO "DIFF_ST    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DIFF_ST),
			(g_fsi_Base + FSI_DIFF_ST));
	printk(KERN_INFO "ACK_MD     [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_ACK_MD),
			(g_fsi_Base + FSI_ACK_MD));
	printk(KERN_INFO "ACK_RV     [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_ACK_RV),
			(g_fsi_Base + FSI_ACK_RV));
/*	printk(KERN_INFO "DIDT       [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DIDT),
			(g_fsi_Base + FSI_DIDT)); */
/*	printk(KERN_INFO "DODT       [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DODT),
			(g_fsi_Base + FSI_DODT)); */
	printk(KERN_INFO "MUTE_ST    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_MUTE_ST),
			(g_fsi_Base + FSI_MUTE_ST));
	printk(KERN_INFO "OUT_DMAC   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_OUT_DMAC),
			(g_fsi_Base + FSI_OUT_DMAC));
	printk(KERN_INFO "OUT_SEL    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_OUT_SEL),
			(g_fsi_Base + FSI_OUT_SEL));
	printk(KERN_INFO "OUT_SPST   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_OUT_SPST),
			(g_fsi_Base + FSI_OUT_SPST));
	printk(KERN_INFO "IN_DMAC    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_IN_DMAC),
			(g_fsi_Base + FSI_IN_DMAC));
	printk(KERN_INFO "IN_SEL     [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_IN_SEL),
			(g_fsi_Base + FSI_IN_SEL));
	printk(KERN_INFO "TMR_CTL    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_TMR_CTL),
			(g_fsi_Base + FSI_TMR_CTL));
	printk(KERN_INFO "TMR_CLR    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_TMR_CLR),
			(g_fsi_Base + FSI_TMR_CLR));
	printk(KERN_INFO "INT_SEL    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_INT_SEL),
			(g_fsi_Base + FSI_INT_SEL));
	printk(KERN_INFO "INT_CLR    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_INT_CLR),
			(g_fsi_Base + FSI_INT_CLR));
	printk(KERN_INFO "CPU_INT_ST [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_CPU_INT_ST),
			(g_fsi_Base + FSI_CPU_INT_ST));
	printk(KERN_INFO "CPU_IEMSK  [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_CPU_IEMSK),
			(g_fsi_Base + FSI_CPU_IEMSK));
	printk(KERN_INFO "CPU_IMSK   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_CPU_IMSK),
			(g_fsi_Base + FSI_CPU_IMSK));
	printk(KERN_INFO "DSP_INT_ST [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DSP_INT_ST),
			(g_fsi_Base + FSI_DSP_INT_ST));
	printk(KERN_INFO "DSP_IEMSK  [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DSP_IEMSK),
			(g_fsi_Base + FSI_DSP_IEMSK));
	printk(KERN_INFO "DSP_IMSK   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_DSP_IMSK),
			(g_fsi_Base + FSI_DSP_IMSK));
	printk(KERN_INFO "MUTE       [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_MUTE),
			(g_fsi_Base + FSI_MUTE));
	printk(KERN_INFO "ACK_RST    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_ACK_RST),
			(g_fsi_Base + FSI_ACK_RST));
	printk(KERN_INFO "SOFT_RST   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_SOFT_RST),
			(g_fsi_Base + FSI_SOFT_RST));
	printk(KERN_INFO "FIFO_SZ    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_FIFO_SZ),
			(g_fsi_Base + FSI_FIFO_SZ));
	printk(KERN_INFO "CLK_SEL    [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_CLK_SEL),
			(g_fsi_Base + FSI_CLK_SEL));
	printk(KERN_INFO "SWAP_SEL   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_SWAP_SEL),
			(g_fsi_Base + FSI_SWAP_SEL));
	printk(KERN_INFO "HPB_SRST   [%08X][%p]\n",
			ioread32(g_fsi_Base + FSI_HPB_SRST),
			(g_fsi_Base + FSI_HPB_SRST));

	printk(KERN_INFO "\n===== FSI Registers Dump End =====\n");
}

int fsi_voice_test_start_a(void)
{
	/* Already FSI started */
	if (TEST_NONE != fsi_test_status_a) {
		/* Simultaneous operation is to expand later. */
		return RET_NG;
	}

	/* Clock framework API, Status ON */
	audio_ctrl_func(SNDP_HW_FSI, STAT_ON, 1);

	iowrite32(0x00000001, (g_fsi_Base + FSI_CLK_SEL));
	iowrite32(0x00212901, (g_fsi_Base + FSI_ACK_MD));
	iowrite32(0x00000001, (g_fsi_Base + FSI_ACK_RV));
	iowrite32(0x00000001, (g_fsi_Base + FSI_DO_FMT));
	iowrite32(0x00000001, (g_fsi_Base + FSI_DI_FMT));
	iowrite32(0x00000001, (g_fsi_Base + FSI_MUTE));

	/* FSI start */
	fsi_test_status_a = TEST_VOICE;

	fsi_test_reg_dump();

	return RET_NONE;
}

void fsi_voice_test_stop_a(void)
{
	/* Clock framework API, Status OFF */
	audio_ctrl_func(SNDP_HW_FSI, STAT_OFF, 1);

	/* FSI stop */
	fsi_test_status_a = TEST_NONE;
}

#endif /* SOUND_TEST */

