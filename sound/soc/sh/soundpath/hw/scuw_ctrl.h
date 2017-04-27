/* scuw_ctrl.h
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

#ifndef __SCUW_CTRL_H__
#define __SCUW_CTRL_H__

#include <linux/kernel.h>

/*
 * SCUW Registers
 */
/* MIX0 Register */
#define SCUW_SWRSR_MIX0			(0x00200)
#define SCUW_MIXIR_MIX0			(0x00204)
#define SCUW_ADINR_MIX0			(0x00208)
#define SCUW_MIXBR_MIX0			(0x0020C)
#define SCUW_MIXMR_MIX0			(0x00210)
#define SCUW_MVPDR_MIX0			(0x00214)
#define SCUW_MDBAR_MIX0			(0x00218)
#define SCUW_MDBBR_MIX0			(0x0021C)
#define SCUW_MDBCR_MIX0			(0x00220)
#define SCUW_MDBDR_MIX0			(0x00224)
#define SCUW_MDBER_MIX0			(0x00228)
#define SCUW_MIXSR_MIX0			(0x0022C)

/* MIX1 Register */
#define SCUW_SWRSR_MIX1			(0x00240)
#define SCUW_MIXIR_MIX1			(0x00244)
#define SCUW_ADINR_MIX1			(0x00248)
#define SCUW_MIXBR_MIX1			(0x0024C)
#define SCUW_MIXMR_MIX1			(0x00250)
#define SCUW_MVPDR_MIX1			(0x00254)
#define SCUW_MDBAR_MIX1			(0x00258)
#define SCUW_MDBBR_MIX1			(0x0025C)
#define SCUW_MDBCR_MIX1			(0x00260)
#define SCUW_MDBDR_MIX1			(0x00264)
#define SCUW_MDBER_MIX1			(0x00268)
#define SCUW_MIXSR_MIX1			(0x0026C)

/* IIR0 Register */
#define SCUW_SWRSR_IIR0			(0x00300)
#define SCUW_IIRIR_IIR0			(0x00304)
#define SCUW_ADINR_IIR0			(0x00308)
#define SCUW_IIRBR_IIR0			(0x0030C)
#define SCUW_IIRCR_IIR0			(0x00310)
#define SCUW_FILTR_IIR0			(0x00314)
#define SCUW_BDCTR_IIR0			(0x00318)
#define SCUW_DEMCR_IIR0			(0x0031C)
#define SCUW_SCLCR_IIR0			(0x00320)
#define SCUW_PGVSR_IIR0			(0x00324)
#define SCUW_S0VSR_IIR0			(0x00328)
#define SCUW_B0BD0_IIR0			(0x0032C)
#define SCUW_B1BD0_IIR0			(0x00330)
#define SCUW_B2BD0_IIR0			(0x00334)
#define SCUW_A1BD0_IIR0			(0x00338)
#define SCUW_A2BD0_IIR0			(0x0033C)
#define SCUW_B0BD1_IIR0			(0x00340)
#define SCUW_B1BD1_IIR0			(0x00344)
#define SCUW_B2BD1_IIR0			(0x00348)
#define SCUW_A1BD1_IIR0			(0x0034C)
#define SCUW_A2BD1_IIR0			(0x00350)
#define SCUW_B0BD2_IIR0			(0x00354)
#define SCUW_B1BD2_IIR0			(0x00358)
#define SCUW_B2BD2_IIR0			(0x0035C)
#define SCUW_A1BD2_IIR0			(0x00360)
#define SCUW_A2BD2_IIR0			(0x00364)
#define SCUW_S1VSR_IIR0			(0x00368)
#define SCUW_IIRSR_IIR0			(0x0036C)

/* VD Register */
/* VDSET_VOC0     : Voice Data setting register */
#define SCUW_VD_VDSET			(0x00500)

/* SEL Register */
/* SELCR_SEL0     : Selector control register(SEL0) */
#define SCUW_SEL_SELCR0			(0x00504)
/* SELCR_SEL1     : Selector control register(SEL1) */
#define SCUW_SEL_SELCR1			(0x00508)
#define SCUW_SEL_SELCR2			(0x0050C)
/* SELCR_SEL4     : Selector control register(SEL4) */
#define SCUW_SEL_SELCR4			(0x00514)
/* SELCR_SEL5     : Selector control register(SEL5) */
#define SCUW_SEL_SELCR5			(0x00518)
/* SELCR_SEL6     : Selector control register(SEL6) */
#define SCUW_SEL_SELCR6			(0x0051C)
/* SELCR_SEL7     : Selector control register(SEL7) */
#define SCUW_SEL_SELCR7			(0x00520)
/* SELCR_SEL8     : Selector control register(SEL8) */
#define SCUW_SEL_SELCR8			(0x00524)
/* SELCR_SEL9     : Selector control register(SEL9) */
#define SCUW_SEL_SELCR9			(0x00528)
/* SELCR_SEL10    : Selector control register(SEL10) */
#define SCUW_SEL_SELCR10		(0x0052C)
/* SELCR_SEL12    : Selector control register(SEL12) */
#define SCUW_SEL_SELCR12		(0x00534)
/* SELCR_SEL13    : Selector control register(SEL13) */
#define SCUW_SEL_SELCR13		(0x00538)
/* SELCR_SEL15    : Selector control register(SEL15) */
#define	SCUW_SEL_SELCR15		(0x00540)
/* SELCR_SEL21    : Selector control register(SEL21) */
#define	SCUW_SEL_SELCR21		(0x00558)

/* FSIIF Registers */
/* SWRSR_FSIIF    : FSI IF Software reset register(FSI IF0) */
#define SCUW_FSIIF_SWRSR		(0x00600)
/* FSIIR_FSIIF    : FSI IF initialization register(FSI IF0) */
#define SCUW_FSIIF_FSIIR		(0x00604)
/* ADINR_FSIIF_W0 : Audio information register for write port 0(FSI IF0) */
#define SCUW_FSIIF_ADINRW0		(0x00608)
/* ADINR_FSIIF_W1 : Audio information register for write port 1(FSI IF0) */
#define SCUW_FSIIF_ADINRW1		(0x0060C)
/* ADINR_FSIIF_R0 : Audio information register for read port 0(FSI IF0) */
#define SCUW_FSIIF_ADINRR0		(0x00610)
/* ADINR_FSIIF_R1 : Audio information register for read port 1(FSI IF0) */
#define SCUW_FSIIF_ADINRR1		(0x00614)
/* WADCR_FSIIF_0  : FSI Write Address control register for port 0(FSI IF0) */
#define SCUW_FSIIF_WADCR0		(0x00618)
/* WADCR_FSIIF_1  : FSI Write Address control register for port 1(FSI IF0) */
#define SCUW_FSIIF_WADCR1		(0x0061C)
/* RADCR_FSIIF_0  : FSI Read Address control register for port 0(FSI IF0) */
#define SCUW_FSIIF_RADCR0		(0x00620)
/* RADCR_FSIIF_1  : FSI Read Address control register for port 1(FSI IF0) */
#define SCUW_FSIIF_RADCR1		(0x00624)

/* SW_SRC_0  : Output switching register for SRC0 */
#define SCUW_SW_SRC0			(0x00648)
#define SCUW_SW_SRC1			(0x0064C)

/* SRC0 Register */
#define SCUW_SWRSR_SRC0			(0x00000)
#define SCUW_SRCIR_SRC0			(0x00004)
#define SCUW_EVMSR_SRC0			(0x00008)
#define SCUW_EVSTR_SRC0			(0x0000C)
#define SCUW_EVCLR_SRC0			(0x00010)
#define SCUW_ADINR_SRC0			(0x00014)
#define SCUW_SRCBR_SRC0			(0x00018)
#define SCUW_IFSCR_SRC0			(0x0001C)
#define SCUW_IFSVR_SRC0			(0x00020)
#define SCUW_SRCCR_SRC0			(0x00024)
#define SCUW_MNFSR_SRC0			(0x00028)
#define SCUW_BFSSR_SRC0			(0x0002C)
#define SCUW_SC2SR_SRC0			(0x00030)
#define SCUW_WATSR_SRC0			(0x00034)

#define SCUW_SWRSR_SRC1			(0x00040)
#define SCUW_SRCIR_SRC1			(0x00044)
#define SCUW_ADINR_SRC1			(0x00054)
#define SCUW_IFSCR_SRC1			(0x0005C)
#define SCUW_IFSVR_SRC1			(0x00060)
#define SCUW_SRCCR_SRC1			(0x00064)
#define SCUW_MNFSR_SRC1			(0x00068)
#define SCUW_BFSSR_SRC1			(0x0006C)

/* MSTP Register */
/* MSTP1_FSIIF    : FSIIF Module control register */
#define SCUW_MSTP0			(0x00658)
#define SCUW_MSTP1			(0x0065C)
#define SCUW_MSTP2			(0x00660)
#define SCUW_MSTP3			(0x00664)

/* FFD Register */
#define SCUW_SWRSR_FFD		(0x00700)
#define SCUW_DMACR_FFD		(0x00704)
#define SCUW_FDAIR_FFD		(0xEC748008)
#define SCUW_DRQSR_FFD		(0xEC74800C)
#define SCUW_FFDPR_FFD		(0xEC748010)
#define SCUW_FFDBR_FFD		(0xEC748014)
#define SCUW_DEVMR_FFD		(0xEC748018)
#define SCUW_DEVSR_FFD		(0xEC74801C)

/* CPUFIFO2 Register */
#define SCUW_SWRSR_CF2		(0x00730)
#define SCUW_DMACR_CF2		(0x00734)
#define SCUW_CF2AIR			(0xEC768008)
#define SCUW_CF2RQSR		(0xEC76800C)
#define SCUW_CF2PR			(0xEC768010)
#define SCUW_CF2EVMR		(0xEC768014)
#define SCUW_CF2EVSR		(0xEC768018)

#endif	/* __SCUW_CTRL_H__ */

