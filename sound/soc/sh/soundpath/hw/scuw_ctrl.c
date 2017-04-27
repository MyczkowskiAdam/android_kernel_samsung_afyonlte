/* scuw_ctrl.c
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

#define __SCUW_NO_EXTERN__

#include <sound/soundpath/soundpath.h>
#include <sound/soundpath/common_extern.h>
#include <sound/soundpath/scuw_extern.h>
#include "scuw_ctrl.h"
/*#include "common_ctrl.h"*/
#include <mach/common.h>
#include <mach/r8a7373.h>
/*
 * Register Table Definisions
 */

/* [Table summary] Reg=Register, Val=Value, D=Delay time, C=Clear bit */

/* Table for Voicecall(PortA) */
static struct common_reg_table scuw_reg_tbl_voicecallA[] = {
/*        Reg			Val		D  C */
	/*   1 : SPU2V output data */
	{ SCUW_SEL_SELCR21,	0x00000001,	0, 0 },
	/*   1 : Voice data(from VOIP) */
	{ SCUW_SEL_SELCR15,	0x00000001,	0, 0 },
	/* 011 : FSI-IF read port 1 data (from FSI2) */
	{ SCUW_SEL_SELCR12,	0x00000003,	0, 0 },
	/*   1 : FSI-IF operates. */
	{ SCUW_MSTP1,		0x00000001,	0, 0 },
	/*   0 : Reset the FSI IF. */
	{ SCUW_FSIIF_SWRSR,	0x00000000,	0, 0 },
	/*   1 : FSI IF enters the operating state. */
	{ SCUW_FSIIF_SWRSR,	0x00000001,	0, 0 },
	/*   1 : Initialization */
/*	{ SCUW_FSIIF_FSIIR,	0x00000001,	0, 0 }, */
	/* 010 : 2 channel */
	{ SCUW_FSIIF_ADINRW0,	0x00000002,	0, 0 },
	/* 010 : 2 channel */
	{ SCUW_FSIIF_ADINRR1,	0x00000002,	0, 0 },
	/* target module : FSI2(0x00), Write address : FSI2 port A(0x09) */
	{ SCUW_FSIIF_WADCR0,	0x00000009,	0, 0 },
	/* target module : FSI2(0x00), Read address : FSI2 port A(0x08) */
	{ SCUW_FSIIF_RADCR1,	0x00000008,	0, 0 },
	/*   0 : Processing State */
/*	{ SCUW_FSIIF_FSIIR,	0x00000000,	0, 0 }, */
	/* 010 : Channel 1 to 7 are copied Channel 0 */
	{ SCUW_VD_VDSET,	0x00000002,	0, 0 },
};

/* Table for Voicecall(PortB) BT 8kHz */
static struct common_reg_table scuw_reg_tbl_voicecallB_8000[] = {
/*        Reg			Val		D  C */
	/*   1 : SPU2V output data */
	{ SCUW_SEL_SELCR21,	0x00000001,	0, 0 },
	/* 010: CPU-FIFO0 data */
	{ SCUW_SEL_SELCR0,	0x00000006,	0, 0 },
	/* 000: Voice data (from VOIP or SPU2V) */
	{ SCUW_SEL_SELCR1,	0x00000000,	0, 0 },
	/* 010: MIX0 system D and SEL5 */
	{ SCUW_SW_SRC0,		0x00000002,	0, 0 },
	/*  10: SW output data, clock supply to MIX0 halted */
	{ SCUW_SEL_SELCR5,	0x00000002,	0, 0 },
	/*   0: SEL5 output data, clock supply to IIR0 halted */
	{ SCUW_SEL_SELCR6,	0x00000000,	0, 0 },
	/*   0: SEL6 output data, clock supply to DVU0 halted */
	{ SCUW_SEL_SELCR7,	0x00000000,	0, 0 },
	/*   0: SEL7 output data */
	{ SCUW_SEL_SELCR15,	0x00000000,	0, 0 },
	/* 110: FSI-IF read port 1 data (from FSI2) */
	{ SCUW_SEL_SELCR2,	0x00000006,	0, 0 },
	/* 011: CPU-FIFO1 data */
	{ SCUW_SEL_SELCR4,	0x00000000,	0, 0 },
	/* 110: MIX1 system D and SEL8 */
	{ SCUW_SW_SRC1,		0x00000006,	0, 0 },
	/*   0: SEL8 output data, clock supply to IIR1 halted */
	{ SCUW_SEL_SELCR8,	0x00000002,	0, 0 },
	/*   0: SEL8 output data, clock supply to IIR1 halted */
	{ SCUW_SEL_SELCR9,	0x00000000,	0, 0 },
	/*   0: SEL9 output data, clock supply to DVU1 halted */
	{ SCUW_SEL_SELCR10,	0x00000000,	0, 0 },
	/* 001: SEL10 output data */
	{ SCUW_SEL_SELCR12,	0x00000001,	0, 0 },
	/* SRC0 / SRC1 setting */
	/* 011: SRC0/SRC1 Module operates */
	{ SCUW_MSTP0,		0x00000003,	0, 0 },
	/*   0 : Resets the SRC0. */
	{ SCUW_SWRSR_SRC0,	0x00000000,	0, 0 },
	/*   1 : SRC0 operating state. */
	{ SCUW_SWRSR_SRC0,	0x00000001,	0, 0 },
	/*   1: SRC0 Initialization */
	{ SCUW_SRCIR_SRC0,	0x00000001,	0, 0 },
	/*   Output Audio Data 16bit, 1ch  */
	{ SCUW_ADINR_SRC0,	0x00080001,	0, 0 },
	/*   Enable IFSVR  */
	{ SCUW_IFSCR_SRC0,	0x00000001,	0, 0 },
	/*  Init. FS; 2^22 * 16k / 8k  */
	{ SCUW_IFSVR_SRC0,	0x00800000,	0, 0 },
	/*  IDATMD, BUFMD, IJECMD, TAPMD, async. SRC */
	{ SCUW_SRCCR_SRC0,	0x00011110,	0, 0 },
	/*  Min. FS; IFSVR * 98 % */
	{ SCUW_MNFSR_SRC0,	0x007D70A3,	0, 0 },
	/*  Buffer size; fixed value */
	{ SCUW_BFSSR_SRC0,	0x00800005,	0, 0 },
	/*  Negate initializing */
	{ SCUW_SRCIR_SRC0,	0x00000000,	0, 0 },
	/*   0 : Resets the SRC1. */
	{ SCUW_SWRSR_SRC1,	0x00000000,	0, 0 },
	/*   1 : SRC1 operating state. */
	{ SCUW_SWRSR_SRC1,	0x00000001,	0, 0 },
	/*   1: SRC1 Initialization */
	{ SCUW_SRCIR_SRC1,	0x00000001,	0, 0 },
	/*   Output Audio Data 16bit, 1ch  */
	{ SCUW_ADINR_SRC1,	0x00080001,	0, 0 },
	/*   Enable IFSVR  */
	{ SCUW_IFSCR_SRC1,	0x00000001,	0, 0 },
	/*  Init. FS; 2^22 * 8k / 16k  */
	{ SCUW_IFSVR_SRC1,	0x00200000,	0, 0 },
	/*  IDATMD, BUFMD, IJECMD, TAPMD, async. SRC */
	{ SCUW_SRCCR_SRC1,	0x00011110,	0, 0 },
	/*  Min. FS; IFSVR * 98 % */
	{ SCUW_MNFSR_SRC1,	0x001F5C28,	0, 0 },
	/*  Buffer size; fixed value */
	{ SCUW_BFSSR_SRC1,	0x00800005,	0, 0 },
	/*  Negate initializing */
	{ SCUW_SRCIR_SRC1,	0x00000000,	0, 0 },
	/*   1 : FSI-IF operates. */
	{ SCUW_MSTP1,		0x00000001,	0, 0 },
	/*   0 : Reset the FSI IF. */
	{ SCUW_FSIIF_SWRSR,	0x00000000,	0, 0 },
	/*   1 : FSI IF enters the operating state. */
	{ SCUW_FSIIF_SWRSR,	0x00000001,	0, 0 },
	/*   1 : Initialization */
/*	{ SCUW_FSIIF_FSIIR,	0x00000001,	0, 0 }, */
	/* 001 : 1 channel */
	{ SCUW_FSIIF_ADINRW0,	0x00000001,	0, 0 },
	/* 001 : 1 channel */
	{ SCUW_FSIIF_ADINRR1,	0x00000001,	0, 0 },
	/* target module : FSI2(0x00), Write address : FSI2 port B(0x19) */
	{ SCUW_FSIIF_WADCR0,	0x00000019,	0, 0 },
	/* target module : FSI2(0x00), Read address : FSI2 port B(0x18) */
	{ SCUW_FSIIF_RADCR1,	0x00000018,	0, 0 },
	/*   0 : Processing State */
/*	{ SCUW_FSIIF_FSIIR,	0x00000000,	0, 0 }, */
	/* 010 : Channel 1 to 7 are copied Channel 0 */
	{ SCUW_VD_VDSET,	0x00000002,	0, 0 },
};

/* Table for Voicecall(PortB) BT 16kHz */
static struct common_reg_table scuw_reg_tbl_voicecallB_16000[] = {
/*        Reg			Val		D  C */
	/*   1 : SPU2V output data */
	{ SCUW_SEL_SELCR21,	0x00000001,	0, 0 },
	/*   1 : Voice data (from VOIP) */
	{ SCUW_SEL_SELCR15,	0x00000001,	0, 0 },
	/* 011 : FSI-IF read port 1 data (from FSI2) */
	{ SCUW_SEL_SELCR12,	0x00000003,	0, 0 },
	/*   1 : FSI-IF operates. */
	{ SCUW_MSTP1,		0x00000001,	0, 0 },
	/*   0 : Reset the FSI IF. */
	{ SCUW_FSIIF_SWRSR,	0x00000000,	0, 0 },
	/*   1 : FSI IF enters the operating state. */
	{ SCUW_FSIIF_SWRSR,	0x00000001,	0, 0 },
	/*   1 : Initialization */
/*	{ SCUW_FSIIF_FSIIR,	0x00000001,	0, 0 }, */
	/* 010 : 1 channel */
	{ SCUW_FSIIF_ADINRW0,	0x00000001,	0, 0 },
	/* 010 : 1 channel */
	{ SCUW_FSIIF_ADINRR1,	0x00000001,	0, 0 },
	/* target module : FSI2(0x00), Write address : FSI2 port B(0x19) */
	{ SCUW_FSIIF_WADCR0,	0x00000019,	0, 0 },
	/* target module : FSI2(0x00), Read address : FSI2 port B(0x18) */
	{ SCUW_FSIIF_RADCR1,	0x00000018,	0, 0 },
	/*   0 : Processing State */
/*	{ SCUW_FSIIF_FSIIR,	0x00000000,	0, 0 }, */
	/* 010 : Channel 1 to 7 are copied Channel 0 */
	{ SCUW_VD_VDSET,	0x00000002,	0, 0 },
};

/* Table for FM(loopback, from FSIB to FSIA) */
static struct common_reg_table scuw_reg_tbl_loopbackBA[] = {
/*        Reg			Val		D  C */
	/* 110: FSI-IF read port 1 data (from FSI2) */
	{ SCUW_SEL_SELCR0,	0x00000006,	0, 0 },
	/* 100: FFD data */
	{ SCUW_SEL_SELCR1,	0x00000004,	0, 0 },
	{ SCUW_SEL_SELCR4,	0x00000006,	0, 0 },
	/*  01: MIX0 output data, clock supply to MIX0 operated */
	{ SCUW_SEL_SELCR5,	0x00000001,	0, 0 },
	/*   1: IIR0 output data, clock supply to IIR0 operated */
	{ SCUW_SEL_SELCR6,	0x00000001,	0, 0 },
	/*   0: SEL6 output data, clock supply to DVU0 halted */
	{ SCUW_SEL_SELCR7,	0x00000000,	0, 0 },
	{ SCUW_SEL_SELCR8,	0x00000001,	0, 0 },
	{ SCUW_SEL_SELCR9,	0x00000000,	0, 0 },
	{ SCUW_SEL_SELCR10,	0x00000000,	0, 0 },
	/*   001: SEL10 output data */
	{ SCUW_SEL_SELCR13,	0x00000001,	0, 0 },
	/*   0: SEL7 output data */
	{ SCUW_SEL_SELCR15,	0x00000000,	0, 0 },

	/*  010: MIX0 system D and SEL5 */
	{ SCUW_SW_SRC0,	0x00000002,	0, 0 },

	{ SCUW_MSTP0,		0x00000001,	0, 0 },
	{ SCUW_SWRSR_SRC0,	0x00000000,	0, 0 },
	{ SCUW_SWRSR_SRC0,	0x00000001,	0, 0 },
	{ SCUW_SRCIR_SRC0,	0x00000001,	0, 0 },
	{ SCUW_SRCBR_SRC0,	0x00000001,	0, 0 },
	{ SCUW_SRCIR_SRC0,	0x00000000,	0, 0 },

	/*   1 : FSI-IF operates. */
	{ SCUW_MSTP1,		0x00000001,	0, 0 },
	/*   0 : Reset the FSI IF. */
	{ SCUW_FSIIF_SWRSR,	0x00000000,	0, 0 },
	/*   1 : FSI IF enters the operating state. */
	{ SCUW_FSIIF_SWRSR,	0x00000001,	0, 0 },
	/*   1 : Initialization */
	{ SCUW_FSIIF_FSIIR,	0x00000001,	0, 0 },
	/* 010 : 2 channel */
	{ SCUW_FSIIF_ADINRW0,	0x00000002,	0, 0 },
	/* 010 : 2 channel */
	{ SCUW_FSIIF_ADINRR1,	0x00000002,	0, 0 },
	/* target module : FSI2(0x00), Write address : FSI2 port A(0x09) */
	{ SCUW_FSIIF_WADCR0,	0x00000009,	0, 0 },
	/* target module : FSI2(0x00), Read address : FSI2 port B(0x18) */
	{ SCUW_FSIIF_RADCR1,	0x00000018,	0, 0 },
	/*   0 : Processing State */
	{ SCUW_FSIIF_FSIIR,	0x00000000,	0, 0 },

	/*   1 : FFD operates. */
	{ SCUW_MSTP2,		0x00000001,	0, 0 },
	/*   0 : Reset the FIFO. */
	{ SCUW_SWRSR_FFD,	0x00000000,	0, 0 },
	/*   1 : FIFO enters the operating state. */
	{ SCUW_SWRSR_FFD,	0x00000001,	0, 0 },
	/*   1 : Initialization (sets the initial setting of other registers) */
	{ SCUW_FFDIR_FFD,	0x00000001,	0, 0 },
	/*   0010 : 2 channel */
	{ SCUW_FDAIR_FFD,	0x00000002,	0, 0 },
	/*   01: Using sampling rate timing (Async Mode) */
	{ SCUW_FFDPR_FFD,	0x00000001,	0, 0 },
	/* 0002 : 64 data. */
	{ SCUW_DRQSR_FFD,	0x00000002,	0, 0 },
	/*   1 : Enables interrupt. */
	{ SCUW_DEVMR_FFD,	0x00000000,	0, 0 },
	/*   1 : Enable DMA request */
	{ SCUW_DMACR_FFD,	0x00000001,	0, 0 },
	/*   0 : Processing State */
	{ SCUW_FFDIR_FFD,	0x00000000,	0, 0 },
	/*   1 : Boot the data request */
	{ SCUW_FFDBR_FFD,	0x00000001,	0, 0 },

	/*   1 : FIFO2 operates. */
	{ SCUW_MSTP3,		0x00000004,	0, 0 },
	/*   0 : Reset FIFO. */
	{ SCUW_SWRSR_CF2,	0x00000000,	0, 0 },
	/*   1 : FIFO enters the operating state. */
	{ SCUW_SWRSR_CF2,	0x00000001,	0, 0 },
	/*   1 : Initialization (sets the initial setting of other registers) */
	{ SCUW_CF2IR,		0x00000001,	0, 0 },
	/*   0010 : 2 channel */
	{ SCUW_CF2AIR,		0x00000002,	0, 0 },
	/*   01: Using sampling rate timing (Async Mode) */
	{ SCUW_CF2PR,		0x00000001,	0, 0 },
	/* 0001 : 128 data. */
	{ SCUW_CF2RQSR,		0x00000001,	0, 0 },
	/*   1 : Enables interrupt. */
	{ SCUW_CF2EVMR,		0x00000000,	0, 0 },
	/*   1 : Enable DMA request */
	{ SCUW_DMACR_CF2,	0x00000001,	0, 0 },
	/*   0 : Processing State */
	{ SCUW_CF2IR,		0x00000000,	0, 0 },

	/*   0: Resets the MIX. */
	{ SCUW_SWRSR_MIX0,	0x00000000,	0, 0 },
	/*   1: Operating state. */
	{ SCUW_SWRSR_MIX0,	0x00000001,	0, 0 },
	/*   1 : Initialization (sets the initial setting of other registers) */
	{ SCUW_MIXIR_MIX0,	0x00000001,	0, 0 },
	{ SCUW_ADINR_MIX0,	0x00000002,	0, 0 },
	{ SCUW_MIXMR_MIX0,	0x00000000,	0, 0 },
	{ SCUW_MVPDR_MIX0,	0x00000000,	0, 0 },
	{ SCUW_MDBER_MIX0,	0x00000000,	0, 0 },
	/* 182:0.0038681205 times */
	{ SCUW_MDBAR_MIX0,	0x00000182,	0, 0 },
	/* Mute */
	{ SCUW_MDBBR_MIX0,	0x000003FF,	0, 0 },
	/* Mute */
	{ SCUW_MDBCR_MIX0,	0x000003FF,	0, 0 },
	/* 1 times */
	{ SCUW_MDBDR_MIX0,	0x00000000,	0, 0 },
	{ SCUW_MDBER_MIX0,	0x00000001,	0, 0 },
	{ SCUW_MIXIR_MIX0,	0x00000000,	0, 0 },

	/*   0: Resets the MIX. */
	{ SCUW_SWRSR_MIX1,	0x00000000,	0, 0 },
	/*   1: Operating state. */
	{ SCUW_SWRSR_MIX1,	0x00000001,	0, 0 },
	/*   1 : Initialization (sets the initial setting of other registers) */
	{ SCUW_MIXIR_MIX1,	0x00000001,	0, 0 },
	{ SCUW_ADINR_MIX1,	0x00000002,	0, 0 },
	{ SCUW_MIXMR_MIX1,	0x00000000,	0, 0 },
	{ SCUW_MVPDR_MIX1,	0x00000000,	0, 0 },
	/* 1 times */
	{ SCUW_MDBER_MIX1,	0x00000000,	0, 0 },
	/* 0.0038681205 times */
	{ SCUW_MDBAR_MIX1,	0x00000182,	0, 0 },
	/* Mute */
	{ SCUW_MDBBR_MIX1,	0x000003FF,	0, 0 },
	/* Mute */
	{ SCUW_MDBCR_MIX1,	0x000003FF,	0, 0 },
	/* Mute */
	{ SCUW_MDBDR_MIX1,	0x000003FF,	0, 0 },
	/* 1 times */
	{ SCUW_MDBER_MIX1,	0x00000001,	0, 0 },
	{ SCUW_MIXIR_MIX1,	0x00000000,	0, 0 },

	/* Set state to reset */
	{ SCUW_SWRSR_IIR0,	0x00000000,	0, 0 },
	/* Set state from reset to operation */
	{ SCUW_SWRSR_IIR0,	0x00000001,	0, 0 },
	/* Set state to initialization */
	{ SCUW_IIRIR_IIR0,	0x00000001,	0, 0 },
	/* Set channel to 2ch */
	{ SCUW_ADINR_IIR0,	0x00000002,	0, 0 },
	/* Enable channel 0 and 1 */
	{ SCUW_IIRCR_IIR0,	0x00000003,	0, 0 },
	/* 1/2/3 bands are not used */
	{ SCUW_FILTR_IIR0,	0x00000000,	0, 0 },
	/* Enable pre-scale and post-scale */
	{ SCUW_SCLCR_IIR0,	0x00000001,	0, 0 },
	/* Set pre-Gain value to 1 times(0dB) */
	{ SCUW_PGVSR_IIR0,	0x00100000,	0, 0 },
	/* Set pre-scale value to 1 times(0dB) */
	{ SCUW_S0VSR_IIR0,	0x00100000,	0, 0 },
	/* Set post-scale value to 256 times */
	{ SCUW_S1VSR_IIR0,	0x00000400,	0, 0 },
	/* Set state from initialization to processing */
	{ SCUW_IIRIR_IIR0,	0x00000000,	0, 0 }

};


/*!
   @brief SCUW start function

   @param[in]	uiValue		PCM type
   @param[in]   rate            Sampling rate
   @param[out]	none

   @retval	ERROR_NONE	successful
 */
int scuw_start(const u_int uiValue, const u_int rate)
{
	/* Local variable declaration */
	u_int			dev		= 0;
	struct common_reg_table	*reg_tbl	= NULL;
	u_int			tbl_size	= 0;

	sndp_log_debug_func("start\n");

	/* Start SCUW Clock Supply */
	audio_ctrl_func(SNDP_HW_SCUW, STAT_ON, 1);

	/* Device check */
	dev = SNDP_GET_DEVICE_VAL(uiValue);
	/* SPEAKER, EARPIECE, WIREDHEADSET, WIREDHEADPHONE, MIC */
	if ((false == (dev & SNDP_BLUETOOTHSCO)) &&
	    (false == (dev & SNDP_FM_RADIO_TX)) &&
	    (false == (dev & SNDP_FM_RADIO_RX))) {
		reg_tbl  = scuw_reg_tbl_voicecallA;
		tbl_size = ARRAY_SIZE(scuw_reg_tbl_voicecallA);
	/* BLUETOOTHSCO */
	} else if (false != (dev & SNDP_BLUETOOTHSCO)) {
		if (rate == 16000) {
			sndp_log_info("rate=16000..\n");
			reg_tbl  = scuw_reg_tbl_voicecallB_16000;
			tbl_size = ARRAY_SIZE(scuw_reg_tbl_voicecallB_16000);
		} else {
			sndp_log_info("rate=8000..\n");
			reg_tbl  = scuw_reg_tbl_voicecallB_8000;
			tbl_size = ARRAY_SIZE(scuw_reg_tbl_voicecallB_8000);
		}
	/* FM_RADIO_RX */
	} else {
		reg_tbl  = scuw_reg_tbl_loopbackBA;
		tbl_size = ARRAY_SIZE(scuw_reg_tbl_loopbackBA);
	}

	/* Start SCUW Settings */
	common_set_register(SNDP_HW_SCUW, reg_tbl, tbl_size);

	/* SCUW Registers Dump */
	scuw_reg_dump();

	sndp_log_debug_func("end\n");

	return ERROR_NONE;
}


/*!
   @brief SCUW stop function(all path)

   @param[in]	none
   @param[out]	none

   @retval	ERROR_NONE	successful
 */
int scuw_stop(void)
{
	sndp_log_debug_func("start\n");

	/* SCUW Registers Dump */
	scuw_reg_dump();

	/* Stop SCUW Clock Supply */
	audio_ctrl_func(SNDP_HW_SCUW, STAT_OFF, 1);

	sndp_log_debug_func("end\n");

	return ERROR_NONE;
}


/*!
   @brief SCUW set FSIIR_FSIF

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void scuw_set_fsiir(void)
{
	u_long r_dummy = 0x0;


	/*   1 : Initialization */
	iowrite32(0x00000001, g_scuw_Base + SCUW_FSIIF_FSIIR);
	r_dummy = ioread32(g_scuw_Base + SCUW_FSIIF_FSIIR);

	/*   0 : Processing State */
	iowrite32(0x00000000, g_scuw_Base + SCUW_FSIIF_FSIIR);
	r_dummy = ioread32(g_scuw_Base + SCUW_FSIIF_FSIIR);
}


/*!
   @brief Dump registers function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void scuw_reg_dump(void)
{
	/* Register Dump Start */
	sndp_log_reg_dump("=== SCUW REGISTER DUMP START ===\n");

	sndp_log_reg_dump("SCUW_VD_VDSET       [%p] = %08X\n",
			(g_scuw_Base + SCUW_VD_VDSET),
			ioread32((g_scuw_Base + SCUW_VD_VDSET)));
	sndp_log_reg_dump("SCUW_SEL_SELCR0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR0),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR0)));
	sndp_log_reg_dump("SCUW_SEL_SELCR1     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR1),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR1)));
	sndp_log_reg_dump("SCUW_SEL_SELCR2     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR2),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR2)));
	sndp_log_reg_dump("SCUW_SEL_SELCR4     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR4),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR4)));
	sndp_log_reg_dump("SCUW_SEL_SELCR5     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR5),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR5)));
	sndp_log_reg_dump("SCUW_SEL_SELCR6     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR6),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR6)));
	sndp_log_reg_dump("SCUW_SEL_SELCR7     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR7),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR7)));
	sndp_log_reg_dump("SCUW_SEL_SELCR8     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR8),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR8)));
	sndp_log_reg_dump("SCUW_SEL_SELCR9     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR9),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR9)));
	sndp_log_reg_dump("SCUW_SEL_SELCR10    [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR10),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR10)));
	sndp_log_reg_dump("SCUW_SEL_SELCR12    [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR12),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR12)));
	sndp_log_reg_dump("SCUW_SEL_SELCR13    [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR13),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR13)));
	sndp_log_reg_dump("SCUW_SEL_SELCR15    [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR15),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR15)));
	sndp_log_reg_dump("SCUW_SEL_SELCR21    [%p] = %08X\n",
			(g_scuw_Base + SCUW_SEL_SELCR21),
			ioread32((g_scuw_Base + SCUW_SEL_SELCR21)));
	sndp_log_reg_dump("SCUW_FSIIF_SWRSR    [%p] = %08X\n",
			(g_scuw_Base + SCUW_FSIIF_SWRSR),
			ioread32((g_scuw_Base + SCUW_FSIIF_SWRSR)));
	sndp_log_reg_dump("SCUW_FSIIF_FSIIR    [%p] = %08X\n",
			(g_scuw_Base + SCUW_FSIIF_FSIIR),
			ioread32((g_scuw_Base + SCUW_FSIIF_FSIIR)));
	sndp_log_reg_dump("SCUW_FSIIF_ADINRW0  [%p] = %08X\n",
			(g_scuw_Base + SCUW_FSIIF_ADINRW0),
			ioread32((g_scuw_Base + SCUW_FSIIF_ADINRW0)));
	sndp_log_reg_dump("SCUW_FSIIF_ADINRW1  [%p] = %08X\n",
			(g_scuw_Base + SCUW_FSIIF_ADINRW1),
			ioread32((g_scuw_Base + SCUW_FSIIF_ADINRW1)));
	sndp_log_reg_dump("SCUW_FSIIF_ADINRR0  [%p] = %08X\n",
			(g_scuw_Base + SCUW_FSIIF_ADINRR0),
			ioread32((g_scuw_Base + SCUW_FSIIF_ADINRR0)));
	sndp_log_reg_dump("SCUW_FSIIF_ADINRR1  [%p] = %08X\n",
			(g_scuw_Base + SCUW_FSIIF_ADINRR1),
			ioread32((g_scuw_Base + SCUW_FSIIF_ADINRR1)));
	sndp_log_reg_dump("SCUW_FSIIF_WADCR0   [%p] = %08X\n",
			(g_scuw_Base + SCUW_FSIIF_WADCR0),
			ioread32((g_scuw_Base + SCUW_FSIIF_WADCR0)));
	sndp_log_reg_dump("SCUW_FSIIF_WADCR1   [%p] = %08X\n",
			(g_scuw_Base + SCUW_FSIIF_WADCR1),
			ioread32((g_scuw_Base + SCUW_FSIIF_WADCR1)));
	sndp_log_reg_dump("SCUW_FSIIF_RADCR0   [%p] = %08X\n",
			(g_scuw_Base + SCUW_FSIIF_RADCR0),
			ioread32((g_scuw_Base + SCUW_FSIIF_RADCR0)));
	sndp_log_reg_dump("SCUW_FSIIF_RADCR1   [%p] = %08X\n",
			(g_scuw_Base + SCUW_FSIIF_RADCR1),
			ioread32((g_scuw_Base + SCUW_FSIIF_RADCR1)));
	sndp_log_reg_dump("SCUW_SW_SRC0        [%p] = %08X\n",
			(g_scuw_Base + SCUW_SW_SRC0),
			ioread32((g_scuw_Base + SCUW_SW_SRC0)));
	sndp_log_reg_dump("SCUW_SW_SRC1        [%p] = %08X\n",
			(g_scuw_Base + SCUW_SW_SRC1),
			ioread32((g_scuw_Base + SCUW_SW_SRC1)));
	sndp_log_reg_dump("SCUW_SWRSR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SWRSR_SRC0),
			ioread32((g_scuw_Base + SCUW_SWRSR_SRC0)));
	sndp_log_reg_dump("SCUW_SRCIR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SRCIR_SRC0),
			ioread32((g_scuw_Base + SCUW_SRCIR_SRC0)));
	sndp_log_reg_dump("SCUW_EVMSR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_EVMSR_SRC0),
			ioread32((g_scuw_Base + SCUW_EVMSR_SRC0)));
	sndp_log_reg_dump("SCUW_EVSTR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_EVSTR_SRC0),
			ioread32((g_scuw_Base + SCUW_EVSTR_SRC0)));
	sndp_log_reg_dump("SCUW_EVCLR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_EVCLR_SRC0),
			ioread32((g_scuw_Base + SCUW_EVCLR_SRC0)));
	sndp_log_reg_dump("SCUW_ADINR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_ADINR_SRC0),
			ioread32((g_scuw_Base + SCUW_ADINR_SRC0)));
	sndp_log_reg_dump("SCUW_SRCBR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SRCBR_SRC0),
			ioread32((g_scuw_Base + SCUW_SRCBR_SRC0)));
	sndp_log_reg_dump("SCUW_IFSCR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_IFSCR_SRC0),
			ioread32((g_scuw_Base + SCUW_IFSCR_SRC0)));
	sndp_log_reg_dump("SCUW_IFSVR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_IFSVR_SRC0),
			ioread32((g_scuw_Base + SCUW_IFSVR_SRC0)));
	sndp_log_reg_dump("SCUW_SRCCR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SRCCR_SRC0),
			ioread32((g_scuw_Base + SCUW_SRCCR_SRC0)));
	sndp_log_reg_dump("SCUW_MNFSR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_MNFSR_SRC0),
			ioread32((g_scuw_Base + SCUW_MNFSR_SRC0)));
	sndp_log_reg_dump("SCUW_BFSSR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_BFSSR_SRC0),
			ioread32((g_scuw_Base + SCUW_BFSSR_SRC0)));
	sndp_log_reg_dump("SCUW_SC2SR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SC2SR_SRC0),
			ioread32((g_scuw_Base + SCUW_SC2SR_SRC0)));
	sndp_log_reg_dump("SCUW_WATSR_SRC0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_WATSR_SRC0),
			ioread32((g_scuw_Base + SCUW_WATSR_SRC0)));

	sndp_log_reg_dump("SCUW_SWRSR_SRC1     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SWRSR_SRC1),
			ioread32((g_scuw_Base + SCUW_SWRSR_SRC1)));
	sndp_log_reg_dump("SCUW_SRCIR_SRC1     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SRCIR_SRC1),
			ioread32((g_scuw_Base + SCUW_SRCIR_SRC1)));
	sndp_log_reg_dump("SCUW_ADINR_SRC1     [%p] = %08X\n",
			(g_scuw_Base + SCUW_ADINR_SRC1),
			ioread32((g_scuw_Base + SCUW_ADINR_SRC1)));
	sndp_log_reg_dump("SCUW_IFSCR_SRC1     [%p] = %08X\n",
			(g_scuw_Base + SCUW_IFSCR_SRC1),
			ioread32((g_scuw_Base + SCUW_IFSCR_SRC1)));
	sndp_log_reg_dump("SCUW_IFSVR_SRC1     [%p] = %08X\n",
			(g_scuw_Base + SCUW_IFSVR_SRC1),
			ioread32((g_scuw_Base + SCUW_IFSVR_SRC1)));
	sndp_log_reg_dump("SCUW_SRCCR_SRC1     [%p] = %08X\n",
			(g_scuw_Base + SCUW_SRCCR_SRC1),
			ioread32((g_scuw_Base + SCUW_SRCCR_SRC1)));
	sndp_log_reg_dump("SCUW_MNFSR_SRC1     [%p] = %08X\n",
			(g_scuw_Base + SCUW_MNFSR_SRC1),
			ioread32((g_scuw_Base + SCUW_MNFSR_SRC1)));
	sndp_log_reg_dump("SCUW_BFSSR_SRC1     [%p] = %08X\n",
			(g_scuw_Base + SCUW_BFSSR_SRC1),
			ioread32((g_scuw_Base + SCUW_BFSSR_SRC1)));

	sndp_log_reg_dump("SCUW_MSTP0          [%p] = %08X\n",
			(g_scuw_Base + SCUW_MSTP0),
			ioread32((g_scuw_Base + SCUW_MSTP0)));
	sndp_log_reg_dump("SCUW_MSTP1          [%p] = %08X\n",
			(g_scuw_Base + SCUW_MSTP1),
			ioread32((g_scuw_Base + SCUW_MSTP1)));
	sndp_log_reg_dump("SCUW_MSTP2          [%p] = %08X\n",
			(g_scuw_Base + SCUW_MSTP2),
			ioread32((g_scuw_Base + SCUW_MSTP2)));
	sndp_log_reg_dump("SCUW_MSTP3          [%p] = %08X\n",
			(g_scuw_Base + SCUW_MSTP3),
			ioread32((g_scuw_Base + SCUW_MSTP3)));

	sndp_log_reg_dump("SCUW_MIXIR_MIX0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_MIXIR_MIX0),
			ioread32((g_scuw_Base + SCUW_MIXIR_MIX0)));
	sndp_log_reg_dump("SCUW_ADINR_MIX0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_ADINR_MIX0),
			ioread32((g_scuw_Base + SCUW_ADINR_MIX0)));
	sndp_log_reg_dump("SCUW_MIXBR_MIX0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_MIXBR_MIX0),
			ioread32((g_scuw_Base + SCUW_MIXBR_MIX0)));
	sndp_log_reg_dump("SCUW_MIXMR_MIX0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_MIXMR_MIX0),
			ioread32((g_scuw_Base + SCUW_MIXMR_MIX0)));
	sndp_log_reg_dump("SCUW_MVPDR_MIX0     [%p] = %08X\n",
			(g_scuw_Base + SCUW_MVPDR_MIX0),
			ioread32((g_scuw_Base + SCUW_MVPDR_MIX0)));

	sndp_log_reg_dump("SCUW_DMACR_FFD      [%p] = %08X\n",
			(g_scuw_Base + SCUW_DMACR_FFD),
			ioread32((g_scuw_Base + SCUW_DMACR_FFD)));
	sndp_log_reg_dump("SCUW_FFDIR_FFD      [%p] = %08X\n",
			(g_scuw_Base_FFD + SCUW_FFDIR_FFD - SCUW_BASE_FFD_PHYS),
			ioread32((g_scuw_Base_FFD +
				SCUW_FFDIR_FFD -
				SCUW_BASE_FFD_PHYS)));
	sndp_log_reg_dump("SCUW_FDAIR_FFD      [%p] = %08X\n",
			(g_scuw_Base_FFD + SCUW_FDAIR_FFD - SCUW_BASE_FFD_PHYS),
			ioread32((g_scuw_Base_FFD +
				SCUW_FDAIR_FFD -
				SCUW_BASE_FFD_PHYS)));
	sndp_log_reg_dump("SCUW_DRQSR_FFD      [%p] = %08X\n",
			(g_scuw_Base_FFD + SCUW_DRQSR_FFD - SCUW_BASE_FFD_PHYS),
			ioread32((g_scuw_Base_FFD +
				SCUW_DRQSR_FFD -
				SCUW_BASE_FFD_PHYS)));
	sndp_log_reg_dump("SCUW_FFDPR_FFD      [%p] = %08X\n",
			(g_scuw_Base_FFD + SCUW_FFDPR_FFD - SCUW_BASE_FFD_PHYS),
			ioread32((g_scuw_Base_FFD +
				SCUW_FFDPR_FFD -
				SCUW_BASE_FFD_PHYS)));
	sndp_log_reg_dump("SCUW_FFDBR_FFD      [%p] = %08X\n",
			(g_scuw_Base_FFD + SCUW_FFDBR_FFD - SCUW_BASE_FFD_PHYS),
			ioread32((g_scuw_Base_FFD +
				SCUW_FFDBR_FFD -
				SCUW_BASE_FFD_PHYS)));
	sndp_log_reg_dump("SCUW_DEVMR_FFD      [%p] = %08X\n",
			(g_scuw_Base_FFD + SCUW_DEVMR_FFD - SCUW_BASE_FFD_PHYS),
			ioread32((g_scuw_Base_FFD +
				SCUW_DEVMR_FFD -
				SCUW_BASE_FFD_PHYS)));

	sndp_log_reg_dump("SCUW_DMACR_CF2      [%p] = %08X\n",
			(g_scuw_Base + SCUW_DMACR_CF2),
			ioread32((g_scuw_Base + SCUW_DMACR_CF2)));
	sndp_log_reg_dump("SCUW_CF2IR          [%p] = %08X\n",
			(g_scuw_Base_CPUFIFO2 + SCUW_CF2IR -
					SCUW_BASE_CPUFIFO2_PHYS),
			ioread32((g_scuw_Base_CPUFIFO2 +
				SCUW_CF2IR -
				SCUW_BASE_CPUFIFO2_PHYS)));
	sndp_log_reg_dump("SCUW_CF2AIR         [%p] = %08X\n",
			(g_scuw_Base_CPUFIFO2 + SCUW_CF2AIR -
						SCUW_BASE_CPUFIFO2_PHYS),
			ioread32((g_scuw_Base_CPUFIFO2 +
				SCUW_CF2AIR -
				SCUW_BASE_CPUFIFO2_PHYS)));
	sndp_log_reg_dump("SCUW_CF2RQSR        [%p] = %08X\n",
			(g_scuw_Base_CPUFIFO2 + SCUW_CF2RQSR -
						SCUW_BASE_CPUFIFO2_PHYS),
			ioread32((g_scuw_Base_CPUFIFO2 +
				SCUW_CF2RQSR -
				SCUW_BASE_CPUFIFO2_PHYS)));
	sndp_log_reg_dump("SCUW_CF2PR          [%p] = %08X\n",
			(g_scuw_Base_CPUFIFO2 + SCUW_CF2PR -
						SCUW_BASE_CPUFIFO2_PHYS),
			ioread32((g_scuw_Base_CPUFIFO2 +
				SCUW_CF2PR -
				SCUW_BASE_CPUFIFO2_PHYS)));
	sndp_log_reg_dump("SCUW_CF2EVMR        [%p] = %08X\n",
			(g_scuw_Base_CPUFIFO2 + SCUW_CF2EVMR -
						SCUW_BASE_CPUFIFO2_PHYS),
			ioread32((g_scuw_Base_CPUFIFO2 +
				SCUW_CF2EVMR -
				SCUW_BASE_CPUFIFO2_PHYS)));

	sndp_log_reg_dump("==== SCUW REGISTER DUMP END ====\n");
}

#ifdef SOUND_TEST
void scuw_voice_test_start_a(void)
{
	/* Clock framework API, Status ON */
	audio_ctrl_func(SNDP_HW_SCUW, STAT_ON, 1);

	iowrite32(0x00000001, (g_scuw_Base + SCUW_SEL_SELCR21));
	iowrite32(0x00000001, (g_scuw_Base + SCUW_SEL_SELCR15));
	iowrite32(0x00000003, (g_scuw_Base + SCUW_SEL_SELCR12));
	iowrite32(0x00000001, (g_scuw_Base + SCUW_MSTP1));
	iowrite32(0x00000000, (g_scuw_Base + SCUW_FSIIF_SWRSR));
	iowrite32(0x00000001, (g_scuw_Base + SCUW_FSIIF_SWRSR));
	iowrite32(0x00000001, (g_scuw_Base + SCUW_FSIIF_FSIIR));
	iowrite32(0x00000002, (g_scuw_Base + SCUW_FSIIF_ADINRW0));
	iowrite32(0x00000002, (g_scuw_Base + SCUW_FSIIF_ADINRR1));
	iowrite32(0x00000009, (g_scuw_Base + SCUW_FSIIF_WADCR0));
	iowrite32(0x00000008, (g_scuw_Base + SCUW_FSIIF_RADCR1));
	iowrite32(0x00000000, (g_scuw_Base + SCUW_FSIIF_FSIIR));
	iowrite32(0x00000002, (g_scuw_Base + SCUW_VD_VDSET));

	scuw_reg_dump();
}

void scuw_voice_test_stop_a(void)
{
	scuw_reg_dump();

	/* Clock framework API, Status OFF */
	audio_ctrl_func(SNDP_HW_SCUW, STAT_OFF);
}
#endif /* SOUND_TEST */

