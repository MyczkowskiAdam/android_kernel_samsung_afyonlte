/* clkgen_ctrl.c
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


#define __CLKGEN_CTRL_NO_EXTERN__

#include <sound/soundpath/soundpath.h>
#include <sound/soundpath/common_extern.h>
#include <sound/soundpath/clkgen_extern.h>
#include "clkgen_ctrl.h"
#include <mach/common.h>


/*
 * GLOBAL DATA Definitions
 */

/* FSI format */
static u_int g_clkgen_rate = 0x0;

/* BT Sampling rate */
static u_int g_clkgen_btscorate = 0x0;

/* CLKGEN Control functions table */
static struct ctrl_func_tbl g_clkgen_ctrl_func_tbl[] = {
	{ SNDP_PLAYBACK_EARPIECE_NORMAL,                    clkgen_playback   },
	{ SNDP_PLAYBACK_EARPIECE_RINGTONE,                  clkgen_playback   },
	{ SNDP_PLAYBACK_EARPIECE_INCALL,                    clkgen_voicecall  },
	{ SNDP_PLAYBACK_EARPIECE_INCOMMUNICATION,           clkgen_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_NORMAL,                     clkgen_playback   },
	{ SNDP_PLAYBACK_SPEAKER_RINGTONE,                   clkgen_playback   },
	{ SNDP_PLAYBACK_SPEAKER_INCALL,                     clkgen_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_INCOMMUNICATION,            clkgen_voicecall  },
	{ SNDP_PLAYBACK_BLUETOOTH_NORMAL,                   clkgen_playback   },
	{ SNDP_PLAYBACK_BLUETOOTH_RINGTONE,                 clkgen_playback   },
	{ SNDP_PLAYBACK_BLUETOOTH_INCALL,                   clkgen_voicecall  },
	{ SNDP_PLAYBACK_BLUETOOTH_INCOMMUNICATION,          clkgen_voicecall  },
	{ SNDP_PLAYBACK_HEADSET_NORMAL,                     clkgen_playback   },
	{ SNDP_PLAYBACK_HEADSET_RINGTONE,                   clkgen_playback   },
	{ SNDP_PLAYBACK_HEADSET_INCALL,                     clkgen_voicecall  },
	{ SNDP_PLAYBACK_HEADSET_INCOMMUNICATION,            clkgen_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_HEADSET_NORMAL,             clkgen_playback   },
	{ SNDP_PLAYBACK_SPEAKER_HEADSET_RINGTONE,           clkgen_playback   },
	{ SNDP_PLAYBACK_SPEAKER_HEADSET_INCALL,             clkgen_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_HEADSET_INCOMMUNICATION,    clkgen_voicecall  },
	{ SNDP_PLAYBACK_HEADPHONE_NORMAL,                   clkgen_playback   },
	{ SNDP_PLAYBACK_HEADPHONE_RINGTONE,                 clkgen_playback   },
	{ SNDP_PLAYBACK_HEADPHONE_INCALL,                   clkgen_voicecall  },
	{ SNDP_PLAYBACK_HEADPHONE_INCOMMUNICATION,          clkgen_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_HEADPHONE_NORMAL,           clkgen_playback   },
	{ SNDP_PLAYBACK_SPEAKER_HEADPHONE_RINGTONE,         clkgen_playback   },
	{ SNDP_PLAYBACK_SPEAKER_HEADPHONE_INCALL,           clkgen_voicecall  },
	{ SNDP_PLAYBACK_SPEAKER_HEADPHONE_INCOMMUNICATION,  clkgen_voicecall  },
	{ SNDP_PLAYBACK_AUXDIGITAL_NORMAL,                  clkgen_playback   },
	{ SNDP_PLAYBACK_AUXDIGITAL_RINGTONE,                NULL              },
	{ SNDP_PLAYBACK_AUXDIGITAL_INCALL,                  NULL              },
	{ SNDP_PLAYBACK_AUXDIGITAL_INCOMMUNICATION,         NULL              },
	{ SNDP_PLAYBACK_FMTX_NORMAL,                        clkgen_playback   },
	{ SNDP_PLAYBACK_FMTX_RINGTONE,                      NULL              },
	{ SNDP_PLAYBACK_FMTX_INCALL,                        NULL              },
	{ SNDP_PLAYBACK_FMTX_INCOMMUNICATION,               NULL              },
	{ SNDP_PLAYBACK_FMRX_SPEAKER_NORMAL,                clkgen_playback   },
	{ SNDP_PLAYBACK_FMRX_SPEAKER_RINGTONE,              NULL              },
	{ SNDP_PLAYBACK_FMRX_SPEAKER_INCALL,                NULL              },
	{ SNDP_PLAYBACK_FMRX_SPEAKER_INCOMMUNICATION,       NULL              },
	{ SNDP_PLAYBACK_FMRX_HEADSET_NORMAL,                clkgen_playback   },
	{ SNDP_PLAYBACK_FMRX_HEADSET_RINGTONE,              NULL              },
	{ SNDP_PLAYBACK_FMRX_HEADSET_INCALL,                NULL              },
	{ SNDP_PLAYBACK_FMRX_HEADSET_INCOMMUNICATION,       NULL              },
	{ SNDP_PLAYBACK_FMRX_HEADPHONE_NORMAL,              clkgen_playback   },
	{ SNDP_PLAYBACK_FMRX_HEADPHONE_RINGTONE,            NULL              },
	{ SNDP_PLAYBACK_FMRX_HEADPHONE_INCALL,              NULL              },
	{ SNDP_PLAYBACK_FMRX_HEADPHONE_INCOMMUNICATION,     NULL              },
	{ SNDP_CAPTURE_MIC_NORMAL,                          clkgen_capture    },
	{ SNDP_CAPTURE_MIC_RINGTONE,                        clkgen_capture    },
	{ SNDP_CAPTURE_MIC_INCALL,                          NULL              },
	{ SNDP_CAPTURE_MIC_INCOMMUNICATION,                 clkgen_voicecall  },
	{ SNDP_CAPTURE_HEADSET_NORMAL,                      clkgen_capture    },
	{ SNDP_CAPTURE_HEADSET_RINGTONE,                    clkgen_capture    },
	{ SNDP_CAPTURE_HEADSET_INCALL,                      NULL              },
	{ SNDP_CAPTURE_HEADSET_INCOMMUNICATION,             clkgen_voicecall  },
	{ SNDP_CAPTURE_BLUETOOTH_NORMAL,                    clkgen_capture    },
	{ SNDP_CAPTURE_BLUETOOTH_RINGTONE,                  clkgen_capture    },
	{ SNDP_CAPTURE_BLUETOOTH_INCALL,                    clkgen_voicecall  },
	{ SNDP_CAPTURE_BLUETOOTH_INCOMMUNICATION,           clkgen_voicecall  },
	{ SNDP_CAPTURE_FMRX_NORMAL,                         clkgen_playback   },
	{ SNDP_CAPTURE_FMRX_RINGTONE,                       NULL              },
	{ SNDP_CAPTURE_FMRX_INCALL,                         NULL              },
	{ SNDP_CAPTURE_FMRX_INCOMMUNICATION,                NULL              },
};


/*
 * Register Table Definisions
 */

/* [Table summary] Reg=Register, Val=Value, D=Delay time, C=Clear bit */

/* Table for Playback(PortA, CLKGEN master, 48kHz) */
static struct common_reg_table clkgen_reg_tbl_playA_M_48[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_FSIACOM,	 0x00212901, 0, 0 }, /* 2ch, 64fs, 48kHz,
					      * CLKGEN master,
					      * Non - continuos mode */
	{ CLKG_PULSECTL, 0x00000001, 0, 0 }, /* PortA Enable */
};

/* Table for Playback(PortA, CLKGEN master, 16kHz) */
static struct common_reg_table clkgen_reg_tbl_playA_M_16[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_FSIACOM,	 0x00212401, 0, 0 }, /* 2ch, 64fs, 16kHz,
					      * CLKGEN master,
					      * Non - continuos mode */
	{ CLKG_PULSECTL, 0x00000001, 0, 0 }, /* PortA Enable */
};

/* Table for Playback(PortB, CLKGEN slave) */
static struct common_reg_table clkgen_reg_tbl_playB_S[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_FSISEL,	 0x00000002, 0, 0 }, /* 1:Select FSIBOBT/FSIBOLR */
};

/* Table for Playback(from PortB to PortA(FM Playback), CLKGEN slave) */
static struct common_reg_table clkgen_reg_tbl_playBA_S[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_TIMSEL1,	 0x04000200, 0, 0x04000200 }, /* REC TIM1(PortA) */
	{ CLKG_FSISEL,	 0x00200003, 0, 0 }, /* 1:Select FSIAOBT/FSIAOLR
					      * 1:Select FSIBOBT/FSIBOLR */
};

/* Table for Capture(PortA, CLKGEN master) */
static struct common_reg_table clkgen_reg_tbl_captureA_M[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_TIMSEL1,	 0x00000200, 0, 0x00000200 }, /* REC TIM1(PortA) */
	{ CLKG_FSIACOM,	 0x00212901, 0, 0 }, /* 2ch, 64fs, 48.1kHz,
					      * CLKGEN master,
					      * Non - continuos mode */
	{ CLKG_PULSECTL, 0x00000001, 0, 0 }, /* PortA Enable */
};

/* Table for Capture(PortA, CLKGEN slave) */
static struct common_reg_table clkgen_reg_tbl_captureA_S[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_TIMSEL1,	 0x00000200, 0, 0x00000200 }, /* REC TIM1(PortA) */
	{ CLKG_FSISEL,	 0x00000001, 0, 0x00000001 }, /* 1:Select FSIAOBT/FSIAOLR */
};

/* Table for Capture(PortB, CLKGEN slave) */
static struct common_reg_table clkgen_reg_tbl_captureB_S[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_TIMSEL1,	 0x00000000, 0, 0 }, /* REC TIM1(PortB) */
	{ CLKG_FSISEL,	 0x00000002, 0, 0x00000002 }, /* 1:Select FSIBOBT/FSIBOLR */
};

#ifdef __SNDP_INCALL_CLKGEN_MASTER
/* Table for Voicecall(PortA, CLKGEN master) */
static struct common_reg_table clkgen_reg_tbl_voicecallA_M[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_SPUVCOM,	 0x00213401, 0, 0 }, /* 2ch, 128fs, 16kHz,
					      * CLKGEN master,
					      * Non - continuos mode */
	{ CLKG_TIMSEL0,	 0x00000002, 0, 0 }, /* VOTIM(PortA) */
	{ CLKG_TIMSEL1,	 0x00000200, 0, 0 }, /* REC TIM1(PortA) */
	{ CLKG_FSIACOM,	 0x00213401, 0, 0 }, /* 2ch, 128fs, 16kHz,
					      * CLKGEN master,
					      * Non - continuos mode */
	{ CLKG_PULSECTL, 0x00000011, 0, 0 }, /* SPUV / PortA Enable */
};

/* Table for Playback(PortB, CLKGEN master, 16kHz) */
static struct common_reg_table clkgen_reg_tbl_playB_M_16[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_FSIBCOM,	 0x00212401, 0, 0 }, /* 2ch, 64fs, 44.1kHz,
					      * CLKGEN master,
					      * Non - continuos mode */
	{ CLKG_PULSECTL, 0x00000002, 0, 0 }, /* PortB Enable */
};

#else /* !__SNDP_INCALL_CLKGEN_MASTER */
/* Table for Voicecall(PortA, CLKGEN slave) */
static struct common_reg_table clkgen_reg_tbl_voicecallA_S[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_TIMSEL0,	 0x00000002, 0, 0 }, /* VOTIM(PortA) */
	{ CLKG_TIMSEL1,	 0x00000200, 0, 0 }, /* REC TIM1(PortA) */
	{ CLKG_FSISEL,	 0x00000011, 0, 0 }, /* CSELSPV 01:Sel FSIAOBT/FSIAOLR
					      * CSELA 1:Sel FSIAOBT/FSIAOLR */
};
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

/* Table for Voicecall(PortB, CLKGEN master, 16kHz) */
static struct common_reg_table clkgen_reg_tbl_voicecallB_M_16000[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_SPUVCOM,	 0x00202401, 0, 0 }, /* 1ch, 64fs, 16kHz,
					      * CLKGEN master,
					      * Non - continuos mode */
	{ CLKG_TIMSEL0,	 0x00000004, 0, 0 }, /* VOTIM(PortB) */
	{ CLKG_TIMSEL1,	 0x00000000, 0, 0 }, /* REC TIM1(PortB) */
	{ CLKG_FSIBCOM,	 0x00202401, 0, 0 }, /* 1ch, 64fs, 16kHz,
					      * CLKGEN master,
					      * Non - continuos mode */
	{ CLKG_PULSECTL, 0x00000012, 0, 0 }, /* SPUV / PortB Enable */
};

/* Table for Voicecall(PortB, CLKGEN master, 8kHz) */
static struct common_reg_table clkgen_reg_tbl_voicecallB_M_8000[] = {
/*        Reg		 Val	     D  C */
	{ CLKG_SYSCTL,	 0x00000000, 0, 0 }, /* EXTAL1 clock supply */
	{ CLKG_SPUVCOM,	 0x00202401, 0, 0 }, /* 1ch, 64fs, 16kHz,
					      * CLKGEN master,
					      * Non - continuos mode */
	{ CLKG_TIMSEL0,	 0x00000000, 0, 0 }, /* VOTIM(PortB) */
	{ CLKG_TIMSEL1,	 0x00000000, 0, 0 }, /* REC TIM1(PortB) */
	{ CLKG_FSIBCOM,	 0x00202101, 0, 0 }, /* 1ch, 64fs, 8kHz,
					      * CLKGEN master,
					      * Non - continuos mode */
	{ CLKG_PULSECTL, 0x00000012, 0, 0 }, /* SPUV / PortB Enable */
};


/*!
   @brief CLKGEN start function

   @param[in]	uiValue		PCM type
   @param[in]	iRate		sampling rate
   @param[out]	none

   @retval	0		Successful
   @retval	-EINVAL		Invalid argument
 */
int clkgen_start(const u_int uiValue, const int iRate, const u_int btscorate)
{
	/* Local variable declaration */
	int iCnt;

	sndp_log_debug_func("start\n");
	sndp_log_info("[0x%08x]\n", uiValue);

	/* Call of function of each PATH */
	for (iCnt = 0; ARRAY_SIZE(g_clkgen_ctrl_func_tbl) > iCnt; iCnt++) {
		/* uiValue check */
		if (uiValue == g_clkgen_ctrl_func_tbl[iCnt].uiValue) {
			/* Function pointer check */
			if (NULL != g_clkgen_ctrl_func_tbl[iCnt].func) {
				/* Set sampling rate */
				g_clkgen_rate = iRate;
				g_clkgen_btscorate = btscorate;

				/* Clock framework API, Status ON */
				audio_ctrl_func(SNDP_HW_CLKGEN, STAT_ON, 1);
				/* Path setting API call */
				g_clkgen_ctrl_func_tbl[iCnt].func(uiValue);
			}
			clkgen_reg_dump();
			sndp_log_debug_func("end\n");
			return ERROR_NONE;
		}
	}

	sndp_log_err("UnSupported value error end\n");
	return -EINVAL;
}


/*!
   @brief CLKGEN stop function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void clkgen_stop(void)
{
	sndp_log_debug_func("start\n");

	clkgen_reg_dump();

	/* clear sampling rate */
	g_clkgen_rate = 0;

	/* Clock framework API, Status OFF */
	audio_ctrl_func(SNDP_HW_CLKGEN, STAT_OFF, 1);

	sndp_log_debug_func("end\n");
}


/*!
   @brief CLKGEN registers setting (Voice call)

   @param[in]	uiValue		PCM type
   @param[out]	none

   @retval	none
 */
static void clkgen_voicecall(const u_int uiValue)
{
	/**********************************************
	 * PATH : SNDP_PLAYBACK_EARPIECE_INCALL
	 *        SNDP_PLAYBACK_SPEAKER_INCALL
	 *        SNDP_PLAYBACK_BLUETOOTH_INCALL
	 *        SNDP_PLAYBACK_HEADSET_INCALL
	 *        SNDP_PLAYBACK_SPEAKER_HEADSET_INCALL
	 *        SNDP_PLAYBACK_HEADPHONE_INCALL
	 *        SNDP_PLAYBACK_SPEAKER_HEADPHONE_INCALL
	 * FSI           : Slave
	 * MAXIM         : Slave
	 * CLKGEN        : Master
	 * Port          : PortA or B
	 * Ch            : 2
	 * Sampling rate : 16000
	 * Adjust mode   : OFF
	 **********************************************/

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
		/* PortA, CLKGEN master, 16kHz */
		reg_tbl  = clkgen_reg_tbl_voicecallA_M;
		tbl_size = ARRAY_SIZE(clkgen_reg_tbl_voicecallA_M);
#else /* !SNDP_INCALL_CLKGEN_MASTER */
		/* PortA, FSI master */
		reg_tbl  = clkgen_reg_tbl_voicecallA_S;
		tbl_size = ARRAY_SIZE(clkgen_reg_tbl_voicecallA_S);
#endif /* SNDP_INCALL_CLKGEN_MASTER */
	/* BLUETOOTHSCO */
	} else {
		/* PortB, CLKGEN master */
		if (g_clkgen_btscorate == 16000) {
			sndp_log_info("rate=16000..\n");
			reg_tbl  = clkgen_reg_tbl_voicecallB_M_16000;
			tbl_size = ARRAY_SIZE(clkgen_reg_tbl_voicecallB_M_16000);
		} else {
			sndp_log_info("rate=8000..\n");
			reg_tbl  = clkgen_reg_tbl_voicecallB_M_8000;
			tbl_size = ARRAY_SIZE(clkgen_reg_tbl_voicecallB_M_8000);
		}
	}

	/* Register setting function call */
	common_set_register(SNDP_HW_CLKGEN, reg_tbl, tbl_size);

	sndp_log_debug_func("end\n");
}


/*!
   @brief CLKGEN registers setting (Playback)

   @param[in]	uiValue		PCM type
   @param[out]	none

   @retval	none
 */
static void clkgen_playback(const u_int uiValue)
{
	/*********************************************
	 * PATH : SNDP_PLAYBACK_XXXXX_NORMAL
	 *        SNDP_PLAYBACK_XXXXX_RINGTONE
	 *        SNDP_PLAYBACK_XXXXX_INCOMMUNICATION
	 * FSI           : Slave
	 * MAXIM         : Slave
	 * CLKGEN        : Master
	 * Port          : PortA or B
	 * Ch            : 2
	 * Sampling rate : 44100 or 16000
	 *********************************************/

	/* Local variable declaration */
	u_int			dev		= 0;
	struct common_reg_table	*reg_tbl	= NULL;
	u_int			tbl_size	= 0;

	sndp_log_debug_func("start\n");

	/* Device check */
	dev = SNDP_GET_DEVICE_VAL(uiValue);
	/*
	 * SPEAKER, EARPIECE, WIREDHEADSET, WIREDHEADPHONE,
	 * AUXDIGITAL(HDMI)
	 */
	if ((false == (dev & SNDP_BLUETOOTHSCO)) &&
	    (false == (dev & SNDP_FM_RADIO_TX)) &&
	    (false == (dev & SNDP_FM_RADIO_RX))) {
		if (!SNDP_IS_FSI_MASTER_DEVICE(dev)) {
			/* CLKGEN master */
			if (SNDP_NORMAL_RATE == g_clkgen_rate) {
				/* PortA, CLKGEN master, 48kHz */
				reg_tbl  = clkgen_reg_tbl_playA_M_48;
				tbl_size = ARRAY_SIZE(clkgen_reg_tbl_playA_M_48);
			} else {
				/* PortA, CLKGEN master, 16kHz */
				reg_tbl  = clkgen_reg_tbl_playA_M_16;
				tbl_size = ARRAY_SIZE(clkgen_reg_tbl_playA_M_16);
			}
		} else {
			/* FSI master */
			/* nothing process */
			return;
		}
	/* FM_RADIO_RX */
	} else if (false != (dev & SNDP_FM_RADIO_RX)) {
		/* FSI master */
		reg_tbl  = clkgen_reg_tbl_playBA_S;
		tbl_size = ARRAY_SIZE(clkgen_reg_tbl_playBA_S);
	/* BLUETOOTHSCO, FM_RADIO_TX */
	} else {
#ifdef __SNDP_INCALL_CLKGEN_MASTER
		if (SNDP_NORMAL_RATE == g_clkgen_rate) {
			/* PortB, FSI master */
			reg_tbl  = clkgen_reg_tbl_playB_S;
			tbl_size = ARRAY_SIZE(clkgen_reg_tbl_playB_S);
		} else {
			/* PortB, CLKGEN master, 16kHz */
			reg_tbl  = clkgen_reg_tbl_playB_M_16;
			tbl_size = ARRAY_SIZE(clkgen_reg_tbl_playB_M_16);
		}
#else /* !__SNDP_INCALL_CLKGEN_MASTER */
		/* FSI master */
		reg_tbl  = clkgen_reg_tbl_playB_S;
		tbl_size = ARRAY_SIZE(clkgen_reg_tbl_playB_S);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */
	}

	/* Register setting function call */
	common_set_register(SNDP_HW_CLKGEN, reg_tbl, tbl_size);

	sndp_log_debug_func("end\n");
}


/*!
   @brief CLKGEN registers setting (Capture)

   @param[in]	uiValue		PCM type
   @param[out]	none

   @retval	none
 */
static void clkgen_capture(const u_int uiValue)
{
	/*********************************************
	 * PATH : SNDP_CAPTURE_MIC_NORMAL
	 *        SNDP_CAPTURE_MIC_RINGTONE
	 *        SNDP_CAPTURE_MIC_INCOMMUNICATION
	 *        SNDP_CAPTURE_HEADSET_NORMAL
	 *        SNDP_CAPTURE_HEADSET_RINGTONE
	 *        SNDP_CAPTURE_HEADSET_INCOMMUNICATION
	 *        SNDP_CAPTURE_FMRX_NORMAL
	 * FSI            : Slave
	 * MAXIM          : Slave
	 * CLKGEN         : Master
	 * Port           : PortA or B
	 * Ch             : 2
	 * Sampling rate  : 44100
	 *********************************************/

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
		if (!SNDP_IS_FSI_MASTER_DEVICE(dev)) {
			/* PortA, CLKGEN master, 48kHz */
			reg_tbl  = clkgen_reg_tbl_captureA_M;
			tbl_size = ARRAY_SIZE(clkgen_reg_tbl_captureA_M);
		} else {
			/* PortA, FSI master */
			reg_tbl  = clkgen_reg_tbl_captureA_S;
			tbl_size = ARRAY_SIZE(clkgen_reg_tbl_captureA_S);
		}
	/* FM_RADIO_RX */
	} else {
		/* FSI master */
		reg_tbl = clkgen_reg_tbl_captureB_S;
		tbl_size = ARRAY_SIZE(clkgen_reg_tbl_captureB_S);
	}

	/* Register setting function call */
	common_set_register(SNDP_HW_CLKGEN, reg_tbl, tbl_size);

	sndp_log_debug_func("end\n");
}


/*!
   @brief CLKGEN registers dump

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void clkgen_reg_dump(void)
{
	sndp_log_reg_dump("===== CLKGEN Registers Dump Start =====\n");

	sndp_log_reg_dump("CLKG_SYSCTL   [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_SYSCTL)),
			(g_clkgen_Base + CLKG_SYSCTL));
	sndp_log_reg_dump("CLKG_PULSECTL [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_PULSECTL)),
			(g_clkgen_Base + CLKG_PULSECTL));
	sndp_log_reg_dump("CLKG_TIMSEL0  [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_TIMSEL0)),
			(g_clkgen_Base + CLKG_TIMSEL0));
	sndp_log_reg_dump("CLKG_TIMSEL1  [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_TIMSEL1)),
			(g_clkgen_Base + CLKG_TIMSEL1));
	sndp_log_reg_dump("CLKG_FSISEL   [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_FSISEL)),
			(g_clkgen_Base + CLKG_FSISEL));
	sndp_log_reg_dump("CLKG_FSIACOM  [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_FSIACOM)),
			(g_clkgen_Base + CLKG_FSIACOM));
	sndp_log_reg_dump("CLKG_FSIBCOM  [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_FSIBCOM)),
			(g_clkgen_Base + CLKG_FSIBCOM));
	sndp_log_reg_dump("CLKG_CPF0COM  [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_CPF0COM)),
			(g_clkgen_Base + CLKG_CPF0COM));
	sndp_log_reg_dump("CLKG_CPF1COM  [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_CPF1COM)),
			(g_clkgen_Base + CLKG_CPF1COM));
	sndp_log_reg_dump("CLKG_SPUVCOM  [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_SPUVCOM)),
			(g_clkgen_Base + CLKG_SPUVCOM));
	sndp_log_reg_dump("CLKG_AURCOM   [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_AURCOM)),
			(g_clkgen_Base + CLKG_AURCOM));
	sndp_log_reg_dump("CLKG_FFDCOM   [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_FFDCOM)),
			(g_clkgen_Base + CLKG_FFDCOM));
	sndp_log_reg_dump("CLKG_SLIMCOM  [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_SLIMCOM)),
			(g_clkgen_Base + CLKG_SLIMCOM));
	sndp_log_reg_dump("CLKG_FSIAAD   [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_FSIAAD)),
			(g_clkgen_Base + CLKG_FSIAAD));
	sndp_log_reg_dump("CLKG_FSIBAD   [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_FSIBAD)),
			(g_clkgen_Base + CLKG_FSIBAD));
	sndp_log_reg_dump("CLKG_CPF0AD   [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_CPF0AD)),
			(g_clkgen_Base + CLKG_CPF0AD));
	sndp_log_reg_dump("CLKG_CPF1AD   [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_CPF1AD)),
			(g_clkgen_Base + CLKG_CPF1AD));
	sndp_log_reg_dump("CLKG_SPUVAD   [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_SPUVAD)),
			(g_clkgen_Base + CLKG_SPUVAD));
	sndp_log_reg_dump("CLKG_AURAD    [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_AURAD)),
			(g_clkgen_Base + CLKG_AURAD));
	sndp_log_reg_dump("CLKG_FFDAD    [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_FFDAD)),
			(g_clkgen_Base + CLKG_FFDAD));
	sndp_log_reg_dump("CLKG_CLKADIV  [%08X][%p]\n",
			ioread32((g_clkgen_Base + CLKG_CLKADIV)),
			(g_clkgen_Base + CLKG_CLKADIV));

	sndp_log_reg_dump("\n===== CLKGEN Registers Dump End =====\n");
}

#ifdef SOUND_TEST


void clkgen_play_test_start_a(void)
{
	/* Clock framework API, Status ON */
	audio_ctrl_func(SNDP_HW_CLKGEN, STAT_ON, 1);

	iowrite32(0x00000000, (g_clkgen_Base + CLKG_SYSCTL));
	iowrite32(0x00212901, (g_clkgen_Base + CLKG_FSIACOM));
	iowrite32(0x00000001, (g_clkgen_Base + CLKG_PULSECTL));

	clkgen_reg_dump();
}

void clkgen_rec_test_start_a(void)
{
	/* Clock framework API, Status ON */
	audio_ctrl_func(SNDP_HW_CLKGEN, STAT_ON, 1);

	iowrite32(0x00000000, (g_clkgen_Base + CLKG_SYSCTL));
	iowrite32(0x00000000, (g_clkgen_Base + CLKG_TIMSEL1));
	iowrite32(0x00212901, (g_clkgen_Base + CLKG_FSIACOM));
	iowrite32(0x00000001, (g_clkgen_Base + CLKG_PULSECTL));

	clkgen_reg_dump();
}

void clkgen_voice_test_start_a(void)
{
	/* Clock framework API, Status ON */
	audio_ctrl_func(SNDP_HW_CLKGEN, STAT_ON, 1);

	iowrite32(0x00000000, (g_clkgen_Base + CLKG_SYSCTL));
	iowrite32(0x00212401, (g_clkgen_Base + CLKG_SPUVCOM));
	iowrite32(0x00000000, (g_clkgen_Base + CLKG_TIMSEL1));
	iowrite32(0x00212001, (g_clkgen_Base + CLKG_FSIACOM));
	iowrite32(0x00000011, (g_clkgen_Base + CLKG_PULSECTL));

	clkgen_reg_dump();
}

void clkgen_play_test_stop_a(void)
{
	clkgen_reg_dump();

	/* Clock framework API, Status OFF */
	audio_ctrl_func(SNDP_HW_CLKGEN, STAT_OFF, 1);
}

void clkgen_rec_test_stop_a(void)
{
	clkgen_reg_dump();

	/* Clock framework API, Status OFF */
	audio_ctrl_func(SNDP_HW_CLKGEN, STAT_OFF, 1);
}

void clkgen_voice_test_stop_a(void)
{
	clkgen_reg_dump();

	/* Clock framework API, Status OFF */
	audio_ctrl_func(SNDP_HW_CLKGEN, STAT_OFF, 1);
}

#endif /* SOUND_TEST */

