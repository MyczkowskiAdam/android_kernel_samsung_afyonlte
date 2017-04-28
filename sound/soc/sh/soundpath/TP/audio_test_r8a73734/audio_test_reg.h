/* audio_test_reg.h
 *
 * Copyright (C) 2012 Renesas Mobile Corp.
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

/*!
  @file		audio_test_reg.h

  @brief	Public definition Audio test register file.
*/

#ifndef __AUDIO_TEST_REG_H__
#define __AUDIO_TEST_REG_H__

/*---------------------------------------------------------------------------*/
/* include files                                                             */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* typedef declaration                                                       */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* define macro declaration                                                  */
/*---------------------------------------------------------------------------*/
/***********************************/
/* FSI Registers                   */
/***********************************/
/* Output serial format register */
#define AUDIO_TEST_FSI_DO_FMT		(0x0000)
/* Output FIFO control register */
#define AUDIO_TEST_FSI_DOFF_CTL		(0x0004)
/* Output FIFO status register */
#define AUDIO_TEST_FSI_DOFF_ST		(0x0008)
/* Input serial format register */
#define AUDIO_TEST_FSI_DI_FMT		(0x000C)
/* Input FIFO control register */
#define AUDIO_TEST_FSI_DIFF_CTL		(0x0010)
/* Iinput FIFO status register */
#define AUDIO_TEST_FSI_DIFF_ST		(0x0014)
/* Audio clock control register */
#define AUDIO_TEST_FSI_ACK_MD		(0x0018)
/* Clock invert control register */
#define AUDIO_TEST_FSI_ACK_RV		(0x001C)
/* Read data register */
#define AUDIO_TEST_FSI_DIDT		(0x0020)
/* Write data register */
#define AUDIO_TEST_FSI_DODT		(0x0024)
/* MUTE state register */
#define AUDIO_TEST_FSI_MUTE_ST		(0x0028)
/* DMA control register(Output) */
#define AUDIO_TEST_FSI_OUT_DMAC		(0x002C)
/* Output control register */
#define AUDIO_TEST_FSI_OUT_SEL		(0x0030)
/* Output SPDIF state register */
#define AUDIO_TEST_FSI_OUT_SPST		(0x0034)
/* DMA control register(Input) */
#define AUDIO_TEST_FSI_IN_DMAC		(0x0038)
/* Input control register */
#define AUDIO_TEST_FSI_IN_SEL		(0x003C)
/* Timer set register */
#define AUDIO_TEST_FSI_TMR_CTL		(0x01D8)
/* Timer clear register */
#define AUDIO_TEST_FSI_TMR_CLR		(0x01DC)
/* DSP interrupt select register */
#define AUDIO_TEST_FSI_INT_SEL		(0x01E0)
/* Interrupt clear register */
#define AUDIO_TEST_FSI_INT_CLR		(0x01F0)
/* CPU Interrupt State Register */
#define AUDIO_TEST_FSI_CPU_INT_ST	(0x01F4)
/* CPU Interrupt Source Mask Set Register */
#define AUDIO_TEST_FSI_CPU_IEMSK	(0x01F8)
/* CPU Interrupt Signal Mask Set Register */
#define AUDIO_TEST_FSI_CPU_IMSK		(0x01FC)
/* DSP Interrupt State Register */
#define AUDIO_TEST_FSI_DSP_INT_ST	(0x0200)
/* DSP Interrupt Source Mask Set Register */
#define AUDIO_TEST_FSI_DSP_IEMSK	(0x0204)
/* DSP Interrupt Signal Mask Set Register */
#define AUDIO_TEST_FSI_DSP_IMSK		(0x0208)
/* MUTE set register */
#define AUDIO_TEST_FSI_MUTE		(0x020C)
/* Clock reset register */
#define AUDIO_TEST_FSI_ACK_RST		(0x0210)
/* Software Reset Set Register */
#define AUDIO_TEST_FSI_SOFT_RST		(0x0214)
/* FIFO Size Register */
#define AUDIO_TEST_FSI_FIFO_SZ		(0x0218)
/* Clock select register */
#define AUDIO_TEST_FSI_CLK_SEL		(0x0220)
/* Swap select register */
#define AUDIO_TEST_FSI_SWAP_SEL		(0x0228)
/* HPB software reset register */
#define AUDIO_TEST_FSI_HPB_SRST		(0x022C)
/* FSIA divide control register */
#define AUDIO_TEST_FSI_FSIDIVA		(0x0400)
/* FSIB divide control register */
#define AUDIO_TEST_FSI_FSIDIVB		(0x0408)
/* PortA offset to PortB  */
#define AUDIO_TEST_FSI_PORTB_OFFSET	(0x0040)

/***********************************/
/* SCUW Registers                  */
/***********************************/
/*******************/
/* VD Register     */
/*******************/
/* VDSET_VOC0     : Voice Data setting register */
#define AUDIO_TEST_SCUW_VD_VDSET	(0x00500)
/*******************/
/* SEL Register    */
/*******************/
/* SELCR_SEL0     : Selector control register(SEL0) */
#define AUDIO_TEST_SCUW_SEL_SELCR0	(0x00504)
/* SELCR_SEL5     : Selector control register(SEL5) */
#define AUDIO_TEST_SCUW_SEL_SELCR5	(0x00518)
/* SELCR_SEL6     : Selector control register(SEL6) */
#define AUDIO_TEST_SCUW_SEL_SELCR6	(0x0051C)
/* SELCR_SEL7     : Selector control register(SEL7) */
#define AUDIO_TEST_SCUW_SEL_SELCR7	(0x00520)
/* SELCR_SEL12    : Selector control register(SEL12) */
#define AUDIO_TEST_SCUW_SEL_SELCR12	(0x00534)
/* SELCR_SEL15    : Selector control register(SEL15) */
#define	AUDIO_TEST_SCUW_SEL_SELCR15	(0x00540)
/* SELCR_SEL21    : Selector control register(SEL21) */
#define	AUDIO_TEST_SCUW_SEL_SELCR21	(0x00558)
/*******************/
/* FSIIF Registers */
/*******************/
/* SWRSR_FSIIF    : FSI IF Software reset register(FSI IF0) */
#define AUDIO_TEST_SCUW_FSIIF_SWRSR	(0x00600)
/* FSIIR_FSIIF    : FSI IF initialization register(FSI IF0) */
#define AUDIO_TEST_SCUW_FSIIF_FSIIR	(0x00604)
/* ADINR_FSIIF_W0 : Audio information register for write port 0(FSI IF0) */
#define AUDIO_TEST_SCUW_FSIIF_ADINRW0	(0x00608)
/* ADINR_FSIIF_W1 : Audio information register for write port 1(FSI IF0) */
#define AUDIO_TEST_SCUW_FSIIF_ADINRW1	(0x0060C)
/* ADINR_FSIIF_R0 : Audio information register for read port 0(FSI IF0) */
#define AUDIO_TEST_SCUW_FSIIF_ADINRR0	(0x00610)
/* ADINR_FSIIF_R1 : Audio information register for read port 1(FSI IF0) */
#define AUDIO_TEST_SCUW_FSIIF_ADINRR1	(0x00614)
/* WADCR_FSIIF_0  : FSI Write Address control register for port 0(FSI IF0) */
#define AUDIO_TEST_SCUW_FSIIF_WADCR0	(0x00618)
/* WADCR_FSIIF_1  : FSI Write Address control register for port 1(FSI IF0) */
#define AUDIO_TEST_SCUW_FSIIF_WADCR1	(0x0061C)
/* RADCR_FSIIF_0  : FSI Read Address control register for port 0(FSI IF0) */
#define AUDIO_TEST_SCUW_FSIIF_RADCR0	(0x00620)
/* RADCR_FSIIF_1  : FSI Read Address control register for port 1(FSI IF0) */
#define AUDIO_TEST_SCUW_FSIIF_RADCR1	(0x00624)
/*******************/
/* MSTP Register   */
/*******************/
/* MSTP1_FSIIF    : FSIIF Module control register */
#define AUDIO_TEST_SCUW_MSTP1		(0x0065C)

/***********************************/
/* CLKGEN Registers                */
/***********************************/
#define AUDIO_TEST_CLKG_SYSCTL		(0x0000)
/* System control register    */
#define AUDIO_TEST_CLKG_PULSECTL	(0x0004)
/* PULSE control register     */
#define AUDIO_TEST_CLKG_TIMSEL0		(0x0054)
/* TIM select register 0      */
#define AUDIO_TEST_CLKG_TIMSEL1		(0x0058)
/* TIM select register 1      */
#define AUDIO_TEST_CLKG_FSISEL		(0x000C)
/* FSI select register        */
#define AUDIO_TEST_CLKG_FSIACOM		(0x0010)
/* FSI Port A common register */
#define AUDIO_TEST_CLKG_FSIBCOM		(0x0014)
/* FSI Port B common register */
#define AUDIO_TEST_CLKG_CPF0COM		(0x0018)
/* CPF0 common register       */
#define AUDIO_TEST_CLKG_CPF1COM		(0x001C)
/* CPF1 common register       */
#define AUDIO_TEST_CLKG_SPUVCOM		(0x0020)
/* SPUV common register       */
#define AUDIO_TEST_CLKG_AURCOM		(0x0024)
/* AURAM common register      */
#define AUDIO_TEST_CLKG_FFDCOM		(0x002C)
/* FFD common register        */
#define AUDIO_TEST_CLKG_SLIMCOM		(0x002C)
/* SLIM common register       */
#define AUDIO_TEST_CLKG_FSIAAD		(0x0030)
/* FSI Port A adjust register */
#define AUDIO_TEST_CLKG_FSIBAD		(0x0034)
/* FSI Port B adjust register */
#define AUDIO_TEST_CLKG_CPF0AD		(0x0038)
/* CPF0 adjust register       */
#define AUDIO_TEST_CLKG_CPF1AD		(0x003C)
/* CPF1 adjust register       */
#define AUDIO_TEST_CLKG_SPUVAD		(0x0040)
/* SPUV adjust register       */
#define AUDIO_TEST_CLKG_AURAD		(0x0044)
/* AURAM adjust register      */
#define AUDIO_TEST_CLKG_FFDAD		(0x0048)
/* FFD adjust register        */
#define AUDIO_TEST_CLKG_CLKADIV		(0x0090)
/* CLK audio divider register */

/***********************************/
/* COMMON Registers                */
/***********************************/
/***************************/
/* CPG setting             */
/***************************/
#define AUDIO_TEST_CPG_REG_MAX_SRST	(0x01CC)
#define AUDIO_TEST_CPG_SRCR2	(g_audio_test_ulSrstRegBase + 0x000000B0)
#define AUDIO_TEST_CPG_SRCR3	(g_audio_test_ulSrstRegBase + 0x000000B8)
/***************************/
/* FSI physical address    */
/***************************/
#define AUDIO_TEST_FSI_REG_MAX		(0x040C)
#define AUDIO_TEST_FSI_MAP_LEN		(AUDIO_TEST_FSI_REG_MAX + 4)
/***************************/
/* SCUW physical address   */
/***************************/
/* SCUW physical address top. */
/* SCUW physical address offset size. */
#define AUDIO_TEST_SCUW_REG_MAX		(0x00738)
/* SCUW physical address mapped size. */
#define AUDIO_TEST_SUCW_MAP_LEN		(AUDIO_TEST_SCUW_REG_MAX + 4)
/***************************/
/* CLKGEN physical address */
/***************************/
#define AUDIO_TEST_CLKGEN_REG_MAX	(0x0098)
#define AUDIO_TEST_CLKGEN_MAP_LEN	(AUDIO_TEST_CLKGEN_REG_MAX + 4)
/***************************/
/* Clock flag bit          */
/***************************/
#define AUDIO_TEST_CLK_FSI		(0x01)
#define AUDIO_TEST_CLK_SCUW		(0x02)
#define AUDIO_TEST_CLK_CLKGEN		(0x04)
/***************************/
/* Max delay               */
/***************************/
#define AUDIO_TEST_COMMON_UDELAY_MAX	(1000)

/*---------------------------------------------------------------------------*/
/* define function macro declaration                                         */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* enum declaration                                                          */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* global variable declaration                                               */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* extern variable declaration                                               */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* extern function declaration                                               */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* prototype declaration                                                     */
/*---------------------------------------------------------------------------*/
/* none */

/*---------------------------------------------------------------------------*/
/* inline function implementation                                            */
/*---------------------------------------------------------------------------*/
/* none */

#endif  /* __AUDIO_TEST_REG_H__ */
