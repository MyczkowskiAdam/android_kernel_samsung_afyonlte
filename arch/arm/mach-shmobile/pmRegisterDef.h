/*
 * arch/arm/mach-shmobile/pmRegisterDef.h
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/****************************************************************/
/* IMPLEMENTATION DEFINED - definitions relating to Cortex-A9	*/
/****************************************************************/

#ifndef CONFIG_PM_HAS_SECURE
#include "pm_ram0.h"
#else /*CONFIG_PM_HAS_SECURE*/
#include "pm_ram0_tz.h"
#endif /*CONFIG_PM_HAS_SECURE*/

#include <mach/r8a7373.h>

#define CA9_SCU_ICD	0x1000	/* GIC Distributor offset from SCU_BASE */
#define CA9_SCU_ICC	0x100	/* GIC CPU Interface offset from SCU_BASE */
#define CA9_SCU_GLTIM	0x200	/* Global timer block offset from SCU_BASE */
#define CA9_SCU_PRTIM	0x600	/* Private timer block offset from SCU_BASE */

/********************************************************/
/* General Interupt Controller (GIC) Register offset	*/
/********************************************************/

/************************************/
/* Interrupt Interface Register		*/
/************************************/
#define BaseInterruptIFPhys	0xF0000100
#define BaseInterruptIF		IO_ADDRESS(BaseInterruptIFPhys)
#define ICC_ICR		0x00	/* CPU Interface Control Register	*/
#define ICC_PMR		0x04	/* Interrupt Priority Mask Register	*/
#define ICC_BPR		0x08	/* Binary Point Register	*/
#define ICC_HPIR    0x18 /* Highest Priority Pending Interrupt Register */


/************************************/
/* Distrbutor Register				*/
/************************************/
#define BaseDistrbutorPhys	0xF0001000
#define BaseDistrbutor		IO_ADDRESS(BaseDistrbutorPhys)
#define ICD_DCR		0x00	/* Distrbutor Control Register(Only CPU0)*/
#define ICD_ICTR	0x04	/* Interrupt Control Type Register(R0)	*/
#define ICD_ISR		0x80	/* Interrupt Security Register */
#define ICD_ISER	0x100	/* Interrupt Set-Enable Register */
#define ICD_IPR		0x400	/* Interrupt Priority Register */
#define ICD_IPTR	0x800	/* Interrupt Priority Targets Register	*/
#define ICD_ICFR	0xC00	/* Interrupt Configurantion Register	*/

/************************************/
/* SCU Register offset				*/
/***********************************/
#define BaseSCUPhys				0xF0000000
#define BaseSCU					IO_ADDRESS(BaseSCUPhys)
#define SCU_CTL			0x00	/* SCU Control Register(RW)*/
#define SCU_CONFIG		0x04	/* SCU Configuration Register(RO)*/
#define SCU_PWRST		0x08	/* SCU CPU Power Status Register(RW)*/
/* SCU Invalidate All Register in Secure State Register(WO)*/
#define SCU_INVALL	0x0C
#define SCU_DIAG	0x30
#define SCU_FILTER_START_ADDR 0x40 /* Filtering Start Address Register(RW)*/
#define SCU_FILTER_END_ADDR	0x44 /* Filtering End Address Register(RW)*/
#define SCU_SAC		0x50	/* SCU Access Control Register(RW)*/
#define SCU_SNSAC	0x54	/* SCU Non-Secure Access Control Register(RW)*/

#define SCU_PWRST_ADDR		(BaseSCU + SCU_PWRST)

/************************************************************/
/* Timer Register offset									*/
/************************************************************/

/********************************************/
/* Private timer Register					*/
/********************************************/
#define BasePrivateAndWDTPhys		0xF0000600
#define BasePrivateAndWDT		IO_ADDRESS(BasePrivateAndWDTPhys)
#define PRTIM_LOAD		0x00	/* Private Timer Load Register(RW)*/
#define PRTIM_COUNTER	0x04	/* Private Timer Counter Register(RW)*/
#define PRTIM_CONTROL	0x08	/* Private Timer Control Register(RW)*/
#define PRTIM_INTSTATE	0x0C	/* Private Timer Interrupt Register(RW)*/

/********************************************/
/* Watchdog Register						*/
/********************************************/
#define WDOG_LOAD		0x20	/* Watchdog Load Register(RW)*/
#define WDOG_COUNTER	0x24	/* Watchdog Counter Register(RW)*/
#define WDOG_CONTROL	0x28	/* Watchdog Control Register(RW)*/
#define WDOG_INTSTATE	0x2C	/* Watchdog Interrupt Register(RW)*/
#define WDOG_RESETSTATE	0x30	/* Watchdog Reset Status Register(RW)*/
#define WDOG_DISABLE	0x34	/* Watchdog Disable Register(RW)	*/

/********************************************/
/* Global timer Register					*/
/********************************************/
#define BaseGlobalTimerPhys		0xF0000200
#define BaseGlobalTimer			IO_ADDRESS(BaseGlobalTimerPhys)
#define GLTIM_CNTLOW	0x00	/* Global Timer Counter Register(Lower)(RW)*/
#define GLTIM_CNTHI	0x04	/* Global Timer Counter Register(Upper)(RW)*/
#define GLTIM_CONTROL	0x08	/* Global Timer Control Register (RW)*/
#define GLTIM_INTSTATE	0x0C	/* Global Timer Interrupt Status Register(RW)*/
#define GLTIM_CMPLOW	0x10	/* Comparator Value Register(Lower)(RW)*/
#define GLTIM_CMPHI	0x14	/* Comparator Value Register(Upper)(RW)*/
#define GLTIM_AUTO		0x18	/* Auto-increment Register(RW)*/

/********************************************/
/* PL310 Register							*/
/********************************************/
#define L2_CONTROL			0x100	/* Control Register*/
#define L2_AUX_CONTROL		0x104	/* Auxiliary Control Register*/
#define L2_LATENCY_CONTROL	0x108	/* Tag RAM Latency Control Register*/
#define L2_RAM_LATENCY_CONTROL	0x10C	/* Data RAM Latency Control Register*/
#define L2_EV_CNT_CONTROL	0x200	/* Event Counter Control Register*/
#define L2_EV_CNT_CONF1	0x204	/* Event Counter Configuration 1 Register*/
#define L2_EV_CNT_CONF0	0x208	/* Event Counter Configuration 0 Register*/
#define L2_EV_CNT_VAL1	0x20C	/* Event counter value 1 register*/
#define L2_EV_CNT_VAL0	0x210	/* Event counter value 0 register*/
#define L2_INIT_MASK	0x214	/* Interrupt Mask Register*/
#define L2_CHACH_SYNC	0x730	/* Cache Sync*/
#define L2_CLEAN_BY_PA	0x7B0	/* Clean Line by PA*/
#define L2_CLEAN_BY_WAY				0x7BC	/* Clean by Way*/
#define L2_CLEAN_AND_INVALIDATE		0x7FC	/* Clean and Invalidate by Way*/
#define L2_INVALIDATW_BY_WAY		0x77C	/* Invalidate by Way*/
#define L2_DATA_LOCKDOWN0			0x900	/* Data Lockdown 0*/
#define L2_LOCKDOWN_BY_LINE_EN	0x950	/* Lockdown by Line Enable Register*/
#define L2_UNLOCK_ALL_LINES		0x954	/* Unlock All Lines Register*/
#define L2_ADDR_FILTER_START	0xC00	/* Address Filtering Start Register*/
#define L2_ADDR_FILTER_END	0xC04	/* Address Filtering End Register*/
#define L2_DEBUG_CONTROL	0xF40	/* Debug Register*/
#define L2_PREFETCH_CONTROL	0xF60	/* Prefetch Control Register*/
#define L2_POWER_CONTROL	0xF80	/* Power Control Register*/

#define L2_LOCKDOWN_BASE	(PL310_BASE + 0x900)
#define L2_LOCKDOWN_BASE_PHYS	(PL310_BASE_PHYS + 0x900)


/******************************************/
/* XTAL though mode				*/
/*****************************************/
#define LPCKCR_LEGACY		0x00000000
#define LPCKCR_26MHz		0x00000002
#define LPCKCR_PLLOFF		0x00000004


#define A2SLSTP			(1 << 20)
#define A2SLSTP_MASK		(~A2SLSTP)

#define D4STP			(1 << 1)
#define D4STP_MASK		(~D4STP)
#define A4MPSTP			(1 << 8)
#define A4MPSTP_MASK		(~A4MPSTP)

#define A3RSTP			(1 << 13)
#define A3RSTP_MASK		(~A3RSTP)

#define C4STP			(1 << 16)
#define C4STP_MASK		(~C4STP)

#define PLL1STPCR_DEFALT	(A2SLSTP | A3RSTP | C4STP | A4MPSTP)
#define PLL1STPCR_DEFALT_MASK	(~PLL1STPCR_DEFALT)

/* IIC0(MSTPST116) halted */
#define MSTPST116 (1 << 16)
#define MSTPST1_PLL1 MSTPST116
#define MSTPST1_PLL1_MASK (~MSTPST1_PLL1)

/* SPU2V(MSTPST220) halted */
#define MSTPST220 (1 << 20)
/* SY-DMAC(MSTPST218) halted */
#define MSTPST218 (1 << 18)
/* USB-DMAC0(MSTPST214) halted */
#define MSTPST214 (1 << 14)
#define MSTPST2_PLL1 (MSTPST220 | MSTPST214)
#define MSTPST2_PLL1_MASK (~MSTPST2_PLL1)

/*IIC1(MSTPST323) halted */
#define MSTPST323 (1 << 23)
/* USB(MSTPST322) halted */
#define MSTPST322 (1 << 22)
#define MSTPST3_PLL1 (MSTPST323 | MSTPST322)
#define MSTPST3_PLL1_MASK (~MSTPST3_PLL1)

/* IICDVM(MSTPST428) halted */
#define MSTPST428 (1 << 28)
/* IIC3H(MSTPST427) halted */
#define MSTPST427 (1 << 27)
/* IIC2H(MSTPST426) halted */
#define MSTPST426 (1 << 26)
/* IIC1H(MSTPST425) halted */
#define MSTPST425 (1 << 25)
/* IIC0H(MSTPST424) halted */
#define MSTPST424 (1 << 24)
/* IICM(MSTPST412) halted */
#define MSTPST412 (1 << 12)
/* IIC3(MSTPST411) halted */
#define MSTPST411 (1 << 11)
/* USB-DMAC1(MSTPST407) */
#define MSTPST407 (1 << 7)
#define MSTPST4_PLL1 (MSTPST428 | MSTPST427 | MSTPST426 | MSTPST425	\
			| MSTPST424 | MSTPST412 | MSTPST411 | MSTPST407)
#define MSTPST4_PLL1_MASK (~MSTPST4_PLL1)

#define MSTP402		0x00000004	/* Module Stop bit 402(RWDT0) */

#define MSTPST527 0x08000000 /* Module Stop Status 527(Internal RAM0)*/
#define MSTPST528 0x10000000 /* Module Stop Status 528(Internal RAM1)*/
#define MSTPST529 0x20000000 /* Module Stop Status 529(Secure RAM) */

#define MSTP527 0x08000000 /* Module Stop bit 527(Internal RAM0)*/
#define MSTP528 0x10000000 /* Module Stop bit 528(Internal RAM1)*/
#define MSTP529 0x20000000 /* Module Stop bit 529(Secure RAM) */


#define FRQCRA_ES2_MASK		0x00000F00 /* 0x00FFFFFF */
#define FRQCRB_MASK			0x0000FFF0 /* 0x1FFFFFF0 */
#define FRQCRB_ZSEL_BIT		0x10000000
#define FRQCRB_ZFC_MASK		0x0F000000
#define FRQCRB_ZCLK_MASK	0x1F000000
#define FRQCRB_KICK		0x80000000 /* FRQCRB KICK bit*/
#define FRQCRD_MASK		0x0000001F
#define FRQCRD_ZB30SEL_BIT	0x00000010
#define FRQCRD_ZB30FC_MASK	0x0000000F
#define FRQCRD_ZB3CHG		0x80000000 /* FRQCRD ZB3CHG bit */
#define KICK_WAIT		1000
#define FRRCRB_ZCLK_1_48	0x1B000000 /* Z clk div = 1/48 */

/* For ES 2.0 and later*/
#define FRQCRA_MASK_ES2		0x00FFFFFF
#define FRQCRB_MASK_ES2		0x00FFFFF0
/* I:1/6, ZG:1/4, M3: 1/8, B:1/48, M1:1/6, M5: 1/8 */
#define	POWERDOWN_FRQCRA_ES2	0x00324B34

/* Z: Not change, ZTR: 1/4, ZT: 1/6, ZX:1/24, ZS:1/24, HP:1/24 */
#define	POWERDOWN_FRQCRB_ES2    0x00238880

/********************************************/
/* RWDT Register			*/
/********************************************/
#define RWTCNT_CLEAR	0x5A5A0000	/* RWDTCNT clear value	*/

#define A3SM_PD		0x00080000
#define A2SLPD		0x00100000
#define A2SLST		0x00100000
#define A3SP_ON		0x00020000

/********************************************/
/* HPBC Register							*/
/********************************************/
#define ESREVISION_MASK		0x0000FFFF

#define SDCR0A_DUMMY_READ	0x80000000	/* SDCR0A dummy read bit */
#define ZB3_HIGHSPEED		0x00000000	/* (1/2)		*/
#define ZB3_MIDSPEED		0x00000010	/* (1/4)		*/
#define ZB3_LOWSPEED		0x00000014	/* (1/16)		*/


/****************************************/
/* PASR SETTINGS						*/
/****************************************/
/* Physical address of MRW area 0 (SDMRACR0)*/
#define	SdramMrwCmd0Phys 0xFE520000
/* Physical address of MRW area 1 (SDMRACR1)*/
#define	SdramMrwCmd1Phys 0xFE538000
/* Physical address of MRW area 1 (SDMRACR0)*/
#define	SdramMrwCmd1SDMRACR0Phys 0xFE530000
/* SDRAM mode register address command register 0A*/
#define	SDMRACR0A	        0xFE400084
/* SDRAM mode register address command register 1A*/
#define	SDMRACR1A	        0xFE400088
/* Bit mask to acquire target bank for PASR on DRAM0 area 0, area 1 (Bank0-15)*/
#define	SbscDramPasr2Area	0x0000FFFF
/* #define	SdmracrOp		8 */
#define	OP_MASK		0xFF00
#define	MA_MASK		0x00FF
#define	OPMA_MASK	(OP_MASK | MA_MASK)

/* Register address of mode register access(PASR)*/
#define	MRW_MA_PASR	0x10

/*SDRAM Self-refresh Setting*/
#define	SDCR0A		0xFE400008	/*SDRAM Control Register 0 A*/
#define	SDCR1A		0xFE40000C	/*SDRAM Control Register 1 A*/
#define	SDCR0SA		0xFE400018	/*SDRAM Control Register 0S A*/
#define	SDCR1SA		0xFE40001C	/*SDRAM Control Register 1S A*/
#define	RMODE		0x400		/* Bit SDCRxxA.RMODE */
#define	SDPDCR0A	0xFE400058	/*Power-Down Control RegisterA*/
#define	RMODESEL0	0x2000		/* Bit SDPDCR0A.RMODESEL0 */
#define	RMODESEL1	0x1000		/* Bit SDPDCR0A.RMODESEL1 */

/************************************************/
/* MACROS										*/
/************************************************/
/* for change processer mode */
#define POWER_MODEUSR				0x10
#define POWER_MODEFIQ				0x11
#define POWER_MODEIRQ				0x12
#define POWER_MODESVC				0x13
#define POWER_MODEABT				0x17
#define POWER_MODEUND				0x1B
#define POWER_MODESYS				0x1F

/* Conditions */
#define POWER_CC_EQ					0x0
#define POWER_CC_NE					0x1
#define POWER_CC_CS					0x2
#define POWER_CC_CC					0x3
#define POWER_CC_MI					0x4
#define POWER_CC_PL					0x5
#define POWER_CC_VS					0x6
#define POWER_CC_VC					0x7
#define POWER_CC_HI					0x8
#define POWER_CC_LS					0x9
#define POWER_CC_GE					0xa
#define POWER_CC_LT					0xb
#define POWER_CC_GT					0xc
#define POWER_CC_LE					0xd
#define POWER_CC_AL					0xe

/* Only use of VFP-D32 */
#define POWER_DN(Dn)((Dn) - 16)
#define POWER_D(Dn)(1 << 22)
/* Only use of Save VFP-D32 */
#define POWER_VSTMIA(cc, Rn, Dd, N)		\
	((cc << 28) | (Rn << 16) | (POWER_DN(Dd) << 12) |	\
		 ((N) * 2) | (POWER_D(Dd)) | (0x0CA00B00))

/* Only use of Restore VFP-D32 */
#define POWER_VLDMIA(cc, Rn, Dd, N)		\
	((cc << 28) | (Rn << 16) | (POWER_DN(Dd) << 12) |	\
		 ((N) * 2) | (POWER_D(Dd)) | (0x0CB00B00))

/* SPI Status Registers	*/
#define ICSPISR0Phys	0xF0001D04
/*SPI Status Registers */
 #define ICSPISR0		IO_ADDRESS(0xF0001D04)
/*SPI Status Registers */
#define ICSPISR1Phys	0xF0001D08
/*SPI Status Registers */
#define ICSPISR1		IO_ADDRESS(0xF0001D08)

/* Register information of IRQC Event Detectors */
/* IRQx Configuration register */
#define CONFIG_00		0x0180
#define CONFIG_01		0x0184
#define CONFIG_02		0x0188
#define CONFIG_03		0x018C
#define CONFIG_04		0x0190
#define CONFIG_05		0x0194
#define CONFIG_06		0x0198
#define CONFIG_07		0x019C
#define CONFIG_08		0x01A0
#define CONFIG_09		0x01A4
#define CONFIG_10		0x01A8
#define CONFIG_11		0x01AC
#define CONFIG_12		0x01B0
#define CONFIG_13		0x01B4
#define CONFIG_14		0x01B8
#define CONFIG_15		0x01BC
#define CONFIG_16		0x01C0
#define CONFIG_17		0x01C4
#define CONFIG_18		0x01C8
#define CONFIG_19		0x01CC
#define CONFIG_20		0x01D0
#define CONFIG_21		0x01D4
#define CONFIG_22		0x01D8
#define CONFIG_23		0x01DC
#define CONFIG_24		0x01E0
#define CONFIG_25		0x01E4
#define CONFIG_26		0x01E8
#define CONFIG_27		0x01EC
#define CONFIG_28		0x01F0
#define CONFIG_29		0x01F4
#define CONFIG_30		0x01F8
#define CONFIG_31		0x01FC

/* HS GPR */
#define HSPRPRICR		0x0018
#define HSPRARCR11		0x0030
#define HSPRARCR12		0x0038
#define HSPRARCR13		0x0040
#define HSPRARCR14		0x0048
#define HSPRARCR31		0x0070
#define HSPRARCR32		0x0078
#define HSPRARCR33		0x0080
#define HSPRARCR34		0x0088
#define HSPRERRMSK		0x0090
#define HSPRPRICNT11	0x00A0
#define HSPRPRICNT12	0x00A8
#define HSPRPRICNT13	0x00B0
/* Sys GPR */
#define SYPRPRICR		0x0018
#define SYPRARCR11		0x0030
#define SYPRARCR12		0x0038
#define SYPRARCR13		0x0040
#define SYPRERRMSK		0x0090
/* HPB */
#define HPBCTRL1		0x1014
#define HPBCTRL2		0x1018
#define HPBCTRL4		0x1024
#define HPBCTRL5		0x1028
#define HPBCTRL7		0x1034
#define OCPBRGWIN1		0x1200
#define OCPBRGWIN2		0x1204
#define OCPBRGWIN3		0x1208
/* SHWYSTAT */
#define SHSTxCR			0x0000
#define SHSTxIR			0x0004
#define SHSTxDMR		0x0008
#define SHSTxCNT		0x000C
#define SHSTxTN			0x0010
#define SHSTxTR			0x0014
#define SHSTxAM11		0x0018
#define SHSTxAM12		0x001C
#define SHSTxTM1		0x0020
#define SHSTxAM21		0x0024
#define SHSTxAM22		0x0028
#define SHSTxTM2		0x002C
#define SHSTxATRM1		0x00B0
#define SHSTxATRM2		0x00B4

/* SHBUF */
#define SHBMCTR			0x4000
#define SHBMAR			0x400C
#define SHBMCTR2		0x4010
#define SHBARCR11		0x4100
#define SHBARCR12		0x4104

#define SHBCHCTR00		0x0000
#define SHBADDR00		0x0004
#define SHBMSKR00		0x0008
#define SHBSIZER00		0x000C

#define SHBCHCTR01		0x0100
#define SHBADDR01		0x0104
#define SHBMSKR01		0x0108
#define SHBSIZER01		0x010C

#define SHBCHCTR02		0x0200
#define SHBADDR02		0x0204
#define SHBMSKR02		0x0208
#define SHBSIZER02		0x020C

#define SHBCHCTR03		0x0300
#define SHBADDR03		0x0304
#define SHBMSKR03		0x0308
#define SHBSIZER03		0x030C

#define SHBCHCTR04		0x0400
#define SHBADDR04		0x0404
#define SHBMSKR04		0x0408
#define SHBSIZER04		0x040C

#define SHBCHCTR05		0x0500
#define SHBADDR05		0x0504
#define SHBMSKR05		0x0508
#define SHBSIZER05		0x050C

#define SHBCHCTR06		0x0600
#define SHBADDR06		0x0604
#define SHBMSKR06		0x0608
#define SHBSIZER06		0x060C

#define SHBCHCTR07		0x0700
#define SHBADDR07		0x0704
#define SHBMSKR07		0x0708
#define SHBSIZER07		0x070C

#define CMCLKE_PHYS     0xe6131000
#define CMCLKE IO_ADDRESS(CMCLKE_PHYS)
/*
 * ********************************************************************
 * LPDDR2 ZQ Calibration Issue WA
 * ********************************************************************
 */
#define STBCHRB3_bit7		0x80
