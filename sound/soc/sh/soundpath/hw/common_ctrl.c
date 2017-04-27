/* common_ctrl.c
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


#define __COMMON_CTRL_NO_EXTERN__

#include <linux/clk.h>
#include <sound/soundpath/common_extern.h>
#include <sound/soundpath/scuw_extern.h>
#include "common_ctrl.h"
#include <mach/common.h>
#include <mach/r8a7373.h>
#include <linux/hwspinlock.h>



/*
 * GLOBAL DATA Definitions
 */

/* CPGA register base address */
static u_char __iomem *g_common_ulClkRstRegBase;
/* CPGA register(soft reset) base address */
static u_char __iomem *g_common_ulSrstRegBase;

/* GPIO register base address */
static u_char __iomem *g_common_ulClkGpioRegBase;

/* Clock status flag */
static u_int g_clock_flag;


/*!
   @brief Register value modify

   @param[in]	uiClr	Clear bit
   @param[in]	uiSet	Set bit
   @param[in]	uiReg	Register address
   @param[out]	none

   @retval	none
 */
#if 0
void iomodify32(u_int uiClr, u_int uiSet, u_int uiReg)
{
	/* Local variable declaration */
	u_int uiTmp;

	uiTmp = ioread32((void __iomem *)uiReg);
	uiTmp &= (~uiClr);
	uiTmp |= uiSet;
	iowrite32(uiTmp, (void __iomem *)uiReg);
}
#endif

/*!
   @brief Registers ioremap for Common/CLKGEN/FSI/SCUW

   @param[in]	none
   @param[out]	none

   @retval	0		Successfull
   @retval	-ENOMEM		Resource error
 */
int common_ioremap(void)
{
	/* Local variable declaration */
	int iRet = ERROR_NONE;

	iRet = common_audio_status_ioremap();
	if (ERROR_NONE != iRet)
		goto ioremap_audio_err;

	iRet = clkgen_ioremap();
	if (ERROR_NONE != iRet)
		goto ioremap_clkgen_err;

	iRet = fsi_ioremap();
	if (ERROR_NONE != iRet)
		goto ioremap_fsi_err;

	iRet = scuw_ioremap();
	if (ERROR_NONE != iRet)
		goto ioremap_scuw_err;

	/* Success of all */
	return ERROR_NONE;

ioremap_scuw_err:
	fsi_iounmap();
ioremap_fsi_err:
	clkgen_iounmap();
ioremap_clkgen_err:
	common_audio_status_iounmap();
ioremap_audio_err:
	return iRet;
}


/*!
   @brief Registers iounmap for Common/CLKGEN/FSI/SCUW

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void common_iounmap(void)
{
	scuw_iounmap();
	fsi_iounmap();
	clkgen_iounmap();
	common_audio_status_iounmap();
}


/*!
   @brief FSI registers ioremap

   @param[in]	none
   @param[out]	none

   @retval	0		Successful
   @retval	-ENOMEM		Resource error
 */
static int fsi_ioremap(void)
{
	/* Get FSI Logical Address */
	g_fsi_Base = ioremap_nocache(FSI_BASE_PHYS, FSI_MAP_LEN);
	if (!g_fsi_Base) {
		sndp_log_err("error fsi ioremap failed\n");
		return -ENOMEM;
	}

	/* Successfull all */
	return ERROR_NONE;
}


/*!
   @brief SCUW ioremap function

   @param[in]	none
   @param[out]	none

   @retval	ERROR_NONE	successful
   @retval	-ENOMEM		resource error
 */
static int scuw_ioremap(void)
{
	/* Get SCUW Logical Address */
	g_scuw_Base = ioremap_nocache(SCUW_BASE_PHYS, SUCW_MAP_LEN);
	if (!g_scuw_Base) {
		sndp_log_err("error scuw ioremap failed\n");
		return -ENOMEM;
	}

	/* Get SCUW(FFD) Logical Address */
	g_scuw_Base_FFD =
		ioremap_nocache(SCUW_BASE_FFD_PHYS, SUCW_MAP_LEN_FFD);
	if (!g_scuw_Base_FFD) {
		sndp_log_err("error scuw(FFD) ioremap failed\n");
		/* Release SCUW Logical Address */
		iounmap(g_scuw_Base);
		g_scuw_Base = NULL;
		return -ENOMEM;
	}

	/* Get SCUW(CPUFIFO2) Logical Address */
	g_scuw_Base_CPUFIFO2 =
		ioremap_nocache(SCUW_BASE_CPUFIFO2_PHYS,
				SUCW_MAP_LEN_CPUFIFO2);
	if (!g_scuw_Base_CPUFIFO2) {
		sndp_log_err("error scuw(CPUFIFO2) ioremap failed\n");
		/* Release SCUW Logical Address */
		iounmap(g_scuw_Base);
		g_scuw_Base = NULL;
		iounmap(g_scuw_Base_FFD);
		g_scuw_Base_FFD = NULL;
		return -ENOMEM;
	}

	/* Successfull all */
	return ERROR_NONE;
}


/*!
   @brief CLKGEN registers ioremap

   @param[in]	none
   @param[out]	none

   @retval	0		Successful
   @retval	-ENOMEM		Resource error
 */
int clkgen_ioremap(void)
{
	/* Get CLKGEN Logical Address */
	g_clkgen_Base = ioremap_nocache(CLKGEN_BASE_PHYS, CLKGEN_MAP_LEN);
	if (!g_clkgen_Base) {
		sndp_log_err("error clkgen ioremap failed\n");
		return -ENOMEM;
	}

	return ERROR_NONE;
}


/*!
   @brief Registers ioremap for use audio_ctrl_func()

   @param[in]	none
   @param[out]	none

   @retval	0		Succcessfull
   @retval	-ENOMEM		Resource error
 */
static int common_audio_status_ioremap(void)
{
	/* Get CPGA Logical Address */
	g_common_ulClkRstRegBase = ioremap_nocache(CPG_BASE_PHYS, CPG_REG_MAX);
	if (!g_common_ulClkRstRegBase) {
		sndp_log_err("error CPGA register ioremap failed\n");
		goto error;
	}

	/* Get CPGA(soft reset) Logical Address */
	g_common_ulSrstRegBase =
		ioremap_nocache(CPG_SEMCTRL_BASE_PHYS, CPG_REG_MAX_SRST);
	if (!g_common_ulSrstRegBase) {
		sndp_log_err("error Software Reset register ioremap failed\n");
		goto error2;
	}

	/* Get GPIO Logical Address */
	g_common_ulClkGpioRegBase =
		ioremap_nocache(GPIO_PHY_BASE_AUDIO_STATUS_PHYS, GPIO_REG_MAX);
	if (!g_common_ulClkGpioRegBase) {
		sndp_log_err("error GPIO register ioremap failed\n");
		goto error3;
	}

	/* Successfull all */
	return ERROR_NONE;

error3:
	iounmap(g_common_ulSrstRegBase);
	g_common_ulSrstRegBase = NULL;
error2:
	iounmap(g_common_ulClkRstRegBase);
	g_common_ulClkRstRegBase = NULL;
error:
	return -ENOMEM;
}


/*!
   @brief FSI registers iounmap

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void fsi_iounmap(void)
{
	/* Release FSI Logical Address */
	if (g_fsi_Base) {
		iounmap(g_fsi_Base);
		g_fsi_Base = NULL;
	}
}


/*!
   @brief SCUW iounmap function

   @param[in]	none
   @param[out]	none
   @retval	none
 */
static void scuw_iounmap(void)
{
	/* Release SCUW Logical Address */
	if (g_scuw_Base) {
		iounmap(g_scuw_Base);
		g_scuw_Base = NULL;
	}
	/* Release SCUW Logical Address to FFD */
	if (g_scuw_Base_FFD) {
		iounmap(g_scuw_Base_FFD);
		g_scuw_Base_FFD = NULL;
	}
	/* Release SCUW Logical Address to CPUFIFO2 */
	if (g_scuw_Base_CPUFIFO2) {
		iounmap(g_scuw_Base_CPUFIFO2);
		g_scuw_Base_CPUFIFO2 = NULL;
	}
}


/*!
   @brief CLKGEN registers iounmap

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void clkgen_iounmap(void)
{
	/* Release CLKGEN Logical Address */
	if (g_clkgen_Base) {
		iounmap(g_clkgen_Base);
		g_clkgen_Base = NULL;
	}
}


/*!
   @brief Registers iounmap for use audio_ctrl_func()

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void common_audio_status_iounmap(void)
{
	/* Release CPGA Logical Address */
	if (g_common_ulClkRstRegBase) {
		iounmap(g_common_ulClkRstRegBase);
		g_common_ulClkRstRegBase = NULL;
	}

	/* Release CPGA(soft reset) Logical Address */
	if (g_common_ulSrstRegBase) {
		iounmap(g_common_ulSrstRegBase);
		g_common_ulSrstRegBase = NULL;
	}

	/* Release GPIO Logical Address */
	if (g_common_ulClkGpioRegBase) {
		iounmap(g_common_ulClkGpioRegBase);
		g_common_ulClkGpioRegBase = NULL;
	}
}


/*!
   @brief Power management for CLKGEN/FSI/SCUW

   @param[in]	drv	H/W type
   @param[in]	stat	On/Off
   @param[out]	none

   @retval	none
 */
void audio_ctrl_func(enum sndp_hw_audio drv, int stat, const u_int regclr)
{
	/* Local variable declaration */
	struct clk *clk;
	unsigned long flags;
	int ret;
#ifdef SOUND_TEST
	u_int reg;
#endif

	switch (drv) {
	case SNDP_HW_CLKGEN:
		/* Status ON */
		if (STAT_ON == stat) {
			if (!(SNDP_CLK_CLKGEN & g_clock_flag)) {
				clk = clk_get(NULL, "clkgen");
				if (IS_ERR(clk)) {
					sndp_log_err("clkget(clkgen) error\n");
				} else {
					clk_enable(clk);
					clk_put(clk);
				}

				if (regclr) {
					ret = hwspin_lock_timeout_irqsave(r8a7373_hwlock_cpg, 10, &flags);
					if (0 > ret)
						sndp_log_err("Can't lock cpg\n");

					/* Soft Reset */
					sh_modify_register32(CPG_SRCR2, 0, 0x01000000);
					udelay(62);
					/* CLKGEN operates */
					sh_modify_register32(CPG_SRCR2, 0x01000000, 0);

					if (0 <= ret)
						hwspin_unlock_irqrestore(r8a7373_hwlock_cpg, &flags);
				}
				g_clock_flag |= SNDP_CLK_CLKGEN;
			}
		/* Status OFF */
		} else {
			if ((SNDP_CLK_CLKGEN & g_clock_flag)) {
				clk = clk_get(NULL, "clkgen");
				if (IS_ERR(clk)) {
					sndp_log_err("clkget(clkgen) error\n");
				} else {
					clk_disable(clk);
					clk_put(clk);
				}
				g_clock_flag &= ~SNDP_CLK_CLKGEN;
			}
		}
		break;

	case SNDP_HW_FSI:
		/* Status ON */
		if (STAT_ON == stat) {
			if (!(SNDP_CLK_FSI & g_clock_flag)) {
				clk = clk_get(NULL, "fsi");
				if (IS_ERR(clk)) {
					sndp_log_err("clkget(fsi) error\n");
				} else {
					clk_enable(clk);
					clk_put(clk);
				}

				if (regclr) {
					ret = hwspin_lock_timeout_irqsave(r8a7373_hwlock_cpg, 10, &flags);
					if (0 > ret)
						sndp_log_err("Can't lock cpg\n");

					/* Soft Reset */
					sh_modify_register32(CPG_SRCR3, 0, 0x10000000);
					udelay(62);
					/* FSI operates */
					sh_modify_register32(CPG_SRCR3, 0x10000000, 0);

					if (0 <= ret)
						hwspin_unlock_irqrestore(r8a7373_hwlock_cpg, &flags);
				}
				g_clock_flag |= SNDP_CLK_FSI;
#ifdef SOUND_TEST
				reg = ioread32(CPG_SUBCKCR);
				if (reg & (1 << 11)) {
					sh_modify_register32(CPG_SUBCKCR, 0x00000800, 0);
				reg = ioread32(CPG_SUBCKCR);
				}
#endif /* SOUND_TEST */
			}
		/* Status OFF */
		} else {
			if ((SNDP_CLK_FSI & g_clock_flag)) {
				clk = clk_get(NULL, "fsi");
				if (IS_ERR(clk)) {
					sndp_log_err("clkget(fsi) error\n");
				} else {
					clk_disable(clk);
					clk_put(clk);
				}

				g_clock_flag &= ~SNDP_CLK_FSI;
			}
		}
		break;

	case SNDP_HW_SCUW:
		/* Status ON */
		if (STAT_ON == stat) {
			if (!(SNDP_CLK_SCUW & g_clock_flag)) {
				clk = clk_get(NULL, "scuw");
				if (IS_ERR(clk)) {
					sndp_log_err("clkget(scuw) error\n");
				} else {
					clk_enable(clk);
					clk_put(clk);
				}

				if (regclr) {
					ret = hwspin_lock_timeout_irqsave(r8a7373_hwlock_cpg, 10, &flags);
					if (0 > ret)
						sndp_log_err("Can't lock cpg\n");

					/* Soft Reset */
					sh_modify_register32(CPG_SRCR3, 0, 0x04000000);
					udelay(62);
					/* SCUW operates */
					sh_modify_register32(CPG_SRCR3, 0x04000000, 0);

					if (0 <= ret)
						hwspin_unlock_irqrestore(r8a7373_hwlock_cpg, &flags);
				}
				g_clock_flag |= SNDP_CLK_SCUW;
			}
		/* Status OFF */
		} else {
			if ((SNDP_CLK_SCUW & g_clock_flag)) {
				clk = clk_get(NULL, "scuw");
				if (IS_ERR(clk)) {
					sndp_log_err("clkget(scuw) error\n");
				} else {
					clk_disable(clk);
					clk_put(clk);
				}
				g_clock_flag &= ~SNDP_CLK_SCUW;
			}
		}
		break;

	default:
		break;
	}
}


/*!
   @brief Common registers setting function

   @param[in]	H/W type
   @param[in]	Register table
   @param[in]	Register table size
   @param[out]	none

   @retval	none
 */
void common_set_register(
	enum sndp_hw_audio drv,
	struct common_reg_table *reg_tbl,
	u_int size)
{
	/* Local variable declaration */
	int	i;
	int addr;

	sndp_log_debug_func("start\n");

	for (i = 0; size > i; i++) {
		/* Delay */
		if (0 != reg_tbl[i].uiDelay) {
			/* 1000 micro over */
			if (COMMON_UDELAY_MAX <= reg_tbl[i].uiDelay)
				mdelay((reg_tbl[i].uiDelay /
					COMMON_UDELAY_MAX));
			else
				udelay(reg_tbl[i].uiDelay);
		/* Modify */
		} else if (0 != reg_tbl[i].uiClrbit) {
			/* CLKGEN */
			if (SNDP_HW_CLKGEN == drv)
				sh_modify_register32(
					(g_clkgen_Base + reg_tbl[i].uiReg),
					reg_tbl[i].uiClrbit,
					reg_tbl[i].uiValue);
			/* FSI */
			else if (SNDP_HW_FSI == drv)
				sh_modify_register32(
					(g_fsi_Base + reg_tbl[i].uiReg),
					reg_tbl[i].uiClrbit,
					reg_tbl[i].uiValue);
			/* SCUW */
			else
				sh_modify_register32(
					(g_scuw_Base + reg_tbl[i].uiReg),
					reg_tbl[i].uiClrbit,
					reg_tbl[i].uiValue);
		/* Register setting */
		} else {
			/* CLKGEN */
			if (SNDP_HW_CLKGEN == drv) {
				iowrite32(reg_tbl[i].uiValue,
					  (g_clkgen_Base + reg_tbl[i].uiReg));
			/* FSI */
			} else if (SNDP_HW_FSI == drv) {
				iowrite32(reg_tbl[i].uiValue,
					  (g_fsi_Base + reg_tbl[i].uiReg));
			/* SCUW */
			} else {
				if ((SCUW_FFDIR_FFD <= reg_tbl[i].uiReg) &&
				    (reg_tbl[i].uiReg <= SCUW_DEVCR_FFD)) {
					addr = reg_tbl[i].uiReg -
							SCUW_BASE_FFD_PHYS;
					iowrite32(reg_tbl[i].uiValue,
						  (g_scuw_Base_FFD + addr));
				} else if ((SCUW_CF2IR <= reg_tbl[i].uiReg) &&
					   (reg_tbl[i].uiReg <= SCUW_CF2EVCR)) {
					addr = reg_tbl[i].uiReg -
							SCUW_BASE_CPUFIFO2_PHYS;
					iowrite32(reg_tbl[i].uiValue,
						(g_scuw_Base_CPUFIFO2 + addr));
				} else {
					iowrite32(reg_tbl[i].uiValue,
						(g_scuw_Base + reg_tbl[i].uiReg));
				}
			}
		}
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief PLL22 and FSIACKCR/FSIBCKCR setting function

   @param[in]	uiValue		PCM type
   @param[in]	stat		On/Off
   @param[in]	rate		Sampling Rate
   @param[out]	none

   @retval	none
 */
void common_set_pll22(const u_int uiValue, int stat, u_int rate)
{
	/* Local variable declaration */
	u_int dev;
	void __iomem *fsickcr;
	int cnt, pll22val, fsival, ret;
	unsigned long flags;

	sndp_log_debug_func("start\n");

	/* Device check */
	dev = SNDP_GET_DEVICE_VAL(uiValue);
	/* PortA */
	if ((false == (dev & SNDP_BLUETOOTHSCO)) &&
	    (false == (dev & SNDP_FM_RADIO_TX)) &&
	    (false == (dev & SNDP_FM_RADIO_RX))) {
		fsickcr = CPG_FSIACKCR;
	/* PortB */
	} else {
		fsickcr = CPG_FSIBCKCR;
	}

	/* Status ON */
	if (STAT_ON == stat) {
		/* mode check */
		if ((SNDP_MODE_INCALL != SNDP_GET_MODE_VAL(uiValue)) &&
		    (SNDP_MODE_INCOMM != SNDP_GET_MODE_VAL(uiValue))) {
			if (false == (dev & SNDP_BLUETOOTHSCO)) {
				pll22val = 0x44000000;
				fsival = 0x00001047;
			} else {
				if (rate == 16000) {
					sndp_log_info("rate=16000..\n");
					pll22val = 0x44000000;
					fsival = 0x0000104B;
				} else {
					sndp_log_info("rate=8000..]\n");
					pll22val = 0x3F000000;
					fsival = 0x0000104C;
				}
			}
#ifndef __SNDP_INCALL_CLKGEN_MASTER
		} else {
			if (false == (dev & SNDP_BLUETOOTHSCO)) {
				pll22val = 0x3F000000;
				fsival = 0x0000104C;
			} else {
				if (rate == 16000) {
					sndp_log_debug("rate=16000..\n");
					pll22val = 0x3F000000;
					fsival = 0x0000104C;
				} else {
					sndp_log_info("rate=8000..]\n");
					pll22val = 0x3F000000;
					fsival = 0x0000104C;
				}
			}
		}
#endif /* __SNDP_INCALL_CLKGEN_MASTER */
			ret = hwspin_lock_timeout_irqsave(r8a7373_hwlock_cpg, 10, &flags);
			if (0 > ret)
				sndp_log_err("Can't lock cpg\n");

			/* set Pll22 enable */
			iowrite32(pll22val, CPG_PLL22CR);
			sh_modify_register32(CPG_PLLECR, 0, 0x00000010);

			for (cnt = 0; cnt < 10; cnt++) {
				if (!(0x1000 & ioread32(CPG_PLLECR)))
					udelay(100);
				else
					break;
			}
			if (10 == cnt)
				sndp_log_err("CPG_PLLECR is not available.\n");

			/* FSICKCR enable 38 divide */
			iowrite32(fsival, fsickcr);

			/* FM Radio */
			if (false != (dev & SNDP_FM_RADIO_RX)) {
				/* set FSIACKCR */
				fsickcr = CPG_FSIACKCR;
				iowrite32(fsival, fsickcr);
			}

			if (0 <= ret)
				hwspin_unlock_irqrestore(r8a7373_hwlock_cpg, &flags);
#ifdef __SNDP_INCALL_CLKGEN_MASTER
		}
#endif /* __SNDP_INCALL_CLKGEN_MASTER */
	/* Status OFF */
	} else {
#ifdef __SNDP_INCALL_CLKGEN_MASTER
		/* mode check */
		if (SNDP_MODE_INCALL != SNDP_GET_MODE_VAL(uiValue)) {
#endif /* __SNDP_INCALL_CLKGEN_MASTER */
			ret = hwspin_lock_timeout_irqsave(r8a7373_hwlock_cpg, 10, &flags);
			if (0 > ret)
				sndp_log_err("Can't lock cpg\n");

			/* FSICKCR disable */
			iowrite32(0x00000100, fsickcr);

			/* FM Radio */
			if (false != (dev & SNDP_FM_RADIO_RX)) {
				/* FSIACKCR disable */
				fsickcr = CPG_FSIACKCR;
				iowrite32(0x00000100, fsickcr);
			}

			/* Pll22 disable */
			sh_modify_register32(CPG_PLLECR, 0x00000010, 0);

			if (0 <= ret)
				hwspin_unlock_irqrestore(r8a7373_hwlock_cpg, &flags);
#ifdef __SNDP_INCALL_CLKGEN_MASTER
		}
#endif /* __SNDP_INCALL_CLKGEN_MASTER */
	}

	sndp_log_debug("CPG_PLL22CR[0x%08x]\n", ioread32(CPG_PLL22CR));
	sndp_log_debug("CPG_FSIACKCR[0x%08x]\n", ioread32(CPG_FSIACKCR));
	sndp_log_debug("CPG_FSIBCKCR[0x%08x]\n", ioread32(CPG_FSIBCKCR));

	sndp_log_debug_func("end\n");
}


/*!
   @brief FSI2CR(GPIO) setting function for FSI master

   @param[in]	stat		On/Off
   @param[out]	none

   @retval	none
 */
void common_set_fsi2cr(u_int dev, int stat)
{
	/* Local variable declaration */
	int ret;
	unsigned long flags;
	u_int clrbit = 0;
	u_int setbit = 0;

	sndp_log_debug_func("start\n");

	if (STAT_ON == stat) {
		if (SNDP_NO_DEVICE == dev)
			setbit = 0x0300;
		else if (SNDP_BLUETOOTHSCO & dev)
			setbit = 0x0200;
		else
			setbit = 0x0100;
	} else {
		if (SNDP_NO_DEVICE == dev)
			clrbit = 0x0300;
		else if (SNDP_BLUETOOTHSCO & dev)
			clrbit = 0x0200;
		else
			clrbit = 0x0100;
	}

	ret = hwspin_lock_timeout_irqsave(r8a7373_hwlock_gpio, 10, &flags);
	if (0 > ret)
		sndp_log_err("Can't lock cpg\n");

	sh_modify_register16(GPIO_FSI2CR, clrbit, setbit);

	if (0 <= ret)
		hwspin_unlock_irqrestore(r8a7373_hwlock_gpio, &flags);

	sndp_log_debug("FSI2CR[0x%04x]\n", ioread16(GPIO_FSI2CR));

	sndp_log_debug_func("end\n");
}

