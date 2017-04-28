/*
 * pmdbg_suspend.c
 *
 * Copyright (C) 2011-2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/pm.h>
#include <linux/regulator/driver.h>

#define PMDBG_ENABLE_PMIC_D2153

#ifdef PMDBG_ENABLE_PMIC_D2153
#include <linux/d2153/pmic.h>
#include <linux/d2153/d2153_reg.h>
#include <linux/d2153/hwmon.h>
#include <linux/d2153/core.h>
#endif /* PMDBG_ENABLE_PMIC_D2153 */

#include <mach/gpio.h>

#include "pmdbg_suspend.h"
#include "pmdbg_hw.h"

static char pmdbg_enable_dump_suspend;
static char suspend_buf[1024];

LOCAL_DECLARE_MOD(suspend, suspend_init, suspend_exit);

DECLARE_CMD(suspend, suspend_cmd);
DECLARE_CMD(wakelock, wakelock_cmd);
DECLARE_CMD(enable_dump_suspend, enable_dump_suspend_cmd);
DECLARE_CMD(disable_dump_suspend, disable_dump_suspend_cmd);

#ifdef PMDBG_ENABLE_PMIC_D2153
struct pmic_table {
	const char	*name;
	unsigned char	reg;
	unsigned char	mctl_reg;
};

static struct pmic_table pmic_reg_tbl[D2153_NUMBER_OF_REGULATORS] = {
	{
		.name		= "D2153_BUCK_1",
		.reg		= D2153_BUCK2PH_BUCK1_REG,
		.mctl_reg	= D2153_BUCK1_MCTL_REG,
	},
	{
		.name		= "D2153_BUCK_2",
		.reg		= D2153_BUCKCORE_BUCK2_REG,
		.mctl_reg	= D2153_BUCK2_MCTL_REG,
	},
	{
		.name		= "D2153_BUCK_3",
		.reg		= D2153_BUCKPRO_BUCK3_REG,
		.mctl_reg	= D2153_BUCK3_MCTL_REG,
	},
	{
		.name		= "D2153_BUCK_4",
		.reg		= D2153_BUCKMEM_BUCK4_REG,
		.mctl_reg	= D2153_BUCK4_MCTL_REG,
	},
	{
		.name		= "D2153_BUCK_5",
		.reg		= D2153_BUCKPERI_BUCK5_REG,
		.mctl_reg	= D2153_BUCK5_MCTL_REG,
	},
	{
		.name		= "D2153_BUCK_6",
		.reg		= D2153_BUCKRF_CONF_REG,
		.mctl_reg	= D2153_BUCK_RF_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_1",
		.reg		= D2153_LDO1_REG,
		.mctl_reg	= D2153_LDO1_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_2",
		.reg		= D2153_LDO2_REG,
		.mctl_reg	= D2153_LDO2_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_3",
		.reg		= D2153_LDO3_REG,
		.mctl_reg	= D2153_LDO3_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_4",
		.reg		= D2153_LDO4_REG,
		.mctl_reg	= D2153_LDO4_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_5",
		.reg		= D2153_LDO5_REG,
		.mctl_reg	= D2153_LDO5_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_6",
		.reg		= D2153_LDO6_REG,
		.mctl_reg	= D2153_LDO6_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_7",
		.reg		= D2153_LDO7_REG,
		.mctl_reg	= D2153_LDO7_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_8",
		.reg		= D2153_LDO8_REG,
		.mctl_reg	= D2153_LDO8_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_9",
		.reg		= D2153_LDO9_REG,
		.mctl_reg	= D2153_LDO9_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_10",
		.reg		= D2153_LDO10_REG,
		.mctl_reg	= D2153_LDO10_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_11",
		.reg		= D2153_LDO11_REG,
		.mctl_reg	= D2153_LDO11_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_12",
		.reg		= D2153_LDO12_REG,
		.mctl_reg	= D2153_LDO12_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_13",
		.reg		= D2153_LDO13_REG,
		.mctl_reg	= D2153_LDO13_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_14",
		.reg		= D2153_LDO14_REG,
		.mctl_reg	= D2153_LDO14_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_15",
		.reg		= D2153_LDO15_REG,
		.mctl_reg	= D2153_LDO15_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_16",
		.reg		= D2153_LDO16_REG,
		.mctl_reg	= D2153_LDO16_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_17",
		.reg		= D2153_LDO17_REG,
		.mctl_reg	= D2153_LDO17_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_18",
		.reg		= D2153_LDO18_LDO_VRFANA_REG,
		.mctl_reg	= D2153_LDO18_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_19",
		.reg		= D2153_LDO19_LDO_19_REG,
		.mctl_reg	= D2153_LDO19_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_20",
		.reg		= D2153_LDO20_LDO_20_REG,
		.mctl_reg	= D2153_LDO20_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_AUD1(LDO_21)",
		.reg		= D2153_LDO21_LDO_AUD1_REG,
		.mctl_reg	= D2153_LDO21_MCTL_REG,
	},
	{
		.name		= "D2153_LDO_AUD2(LDO_22)",
		.reg		= D2153_LDO22_LDO_AUD2_REG,
		.mctl_reg	= D2153_LDO22_MCTL_REG,
	},
};
#endif /* PMDBG_ENABLE_PMIC_D2153 */

/* suspend [type]
 * type:
 * - force: force suspend (skip early suspend)
 * */
static int suspend_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	int pos = 0;
	struct timeval beforeTime;
	struct timeval afterTime;
	int suspend_time;
	FUNC_MSG_IN;
	para = strim(para);
	ret = get_word(para, size, 0, item, &para_sz);
	pos = ret;
	if (para_sz > 0) {
		ret = strncmp(item, "force", sizeof("force"));
		if (0 == ret) {
			do_gettimeofday(&beforeTime);
			ret = suspend_devices_and_enter(PM_SUSPEND_MEM);
		} else {
			ret = -ENOTSUPP;
			MSG_INFO("No supported");
			goto fail;
		}
	} else {
		do_gettimeofday(&beforeTime);
		ret = pm_suspend(PM_SUSPEND_MEM);
	}
	do_gettimeofday(&afterTime);
	suspend_time = (afterTime.tv_sec -
			beforeTime.tv_sec) * 1000000
			+ (afterTime.tv_usec - beforeTime.tv_usec);

	MSG_INFO("Suspended in %12uus", suspend_time);
fail:
	FUNC_MSG_RET(ret);
}


/* wakelock
 * - no parameter: Display list of active wakelock to console
 * */
static int wakelock_cmd(char *para, int size)
{
	int ret = 0;
	FUNC_MSG_IN;

#ifdef CONFIG_ARCH_R8A7373
	ret = has_wake_lock_no_expire(WAKE_LOCK_SUSPEND);
	if (ret == 0)
		MSG_INFO("No active suspend wakelock");
#else /*!CONFIG_ARCH_R8A7373*/
	ret = has_wake_lock(WAKE_LOCK_SUSPEND);
	if (ret == 0)
		MSG_INFO("No active suspend wakelock");
#endif /*CONFIG_ARCH_R8A7373*/

	FUNC_MSG_RET(0);
}

/* enable_dump_suspend
 * - no parameter:
 * */
static int enable_dump_suspend_cmd(char *para, int size)
{
	char *s = suspend_buf;

	FUNC_MSG_IN;

	pmdbg_enable_dump_suspend = 1;
	s += sprintf(s, "Suspended: dump suspend log output Enabled");
	MSG_INFO("%s", suspend_buf);

	FUNC_MSG_RET(0);
}

/* disable_dump_suspend
 * - no parameter:
 * */
static int disable_dump_suspend_cmd(char *para, int size)
{
	char *s = suspend_buf;

	FUNC_MSG_IN;

	pmdbg_enable_dump_suspend = 0;
	s += sprintf(s, "Suspended: dump suspend log output Disabled");
	MSG_INFO("%s", suspend_buf);

	FUNC_MSG_RET(0);
}

static int suspend_init(void)
{
	FUNC_MSG_IN;
	ADD_CMD(suspend, suspend);
	ADD_CMD(suspend, wakelock);
	ADD_CMD(suspend, enable_dump_suspend);
	ADD_CMD(suspend, disable_dump_suspend);
	pmdbg_enable_dump_suspend = 0;

	FUNC_MSG_RET(0);
}

static void suspend_exit(void)
{
	FUNC_MSG_IN;
	DEL_CMD(suspend, suspend);
	DEL_CMD(suspend, wakelock);
	DEL_CMD(suspend, enable_dump_suspend);
	DEL_CMD(suspend, disable_dump_suspend);

	FUNC_MSG_OUT;
}

void pmdbg_dump_suspend(void)
{
	CPG_dump_suspend();
	SYSC_dump_suspend();
	GPIO_dump_suspend();
	Other_dump_suspend();

	return;
}

/* Why not print the name, rather than the phys addr, like this? */
/*define sprint_regm(s, p, v, w) (s += sprintf(s, #v ": 0x%x\n", rreg##w(v)))*/
#define sprint_regm(s, p, v, w) (s += sprintf(s, "0x%x: 0x%x\n", p, rreg##w(v)))
#define sprint_reg(s, v) sprint_regm(s, IO_TO_PHYS(v), v, 32)
#define sprint_reg8(s, v) sprint_regm(s, IO_TO_PHYS(v), v, 8)

static void CPG_dump_suspend(void)
{
	void __iomem *vir_addr = NULL;
	char *s = suspend_buf;

	sprintf(suspend_buf, ">> %s", __func__);
	MSG_INFO("%s", suspend_buf);

	vir_addr = ioremap(SBSC_Freq_APE, 0x8);
	if (!vir_addr) {
		sprintf(suspend_buf, "FAILED");
		goto fail;
	}

	sprint_reg(s, FRQCRA);
	sprint_reg(s, FRQCRB);
	sprint_reg(s, FRQCRD);
	sprint_regm(s, SBSC_Freq_APE, vir_addr, 32);
	sprint_regm(s, SBSC_Freq_BB, vir_addr + 0x4, 32);
	sprint_reg(s, BBFRQCRD);
	sprint_reg(s, VCLKCR1);
	sprint_reg(s, VCLKCR2);
	sprint_reg(s, VCLKCR3);
	sprint_reg(s, VCLKCR4);
	sprint_reg(s, VCLKCR5);
	sprint_reg(s, ZBCKCR);
	sprint_reg(s, SD0CKCR);
	sprint_reg(s, SD1CKCR);
	sprint_reg(s, SD2CKCR);
	sprint_reg(s, FSIACKCR);
	sprint_reg(s, FSIBCKCR);
	sprint_reg(s, MPCKCR);
	sprint_reg(s, SPUACKCR);
	MSG_INFO("%s, s", suspend_buf);

	/* Done until here */
	s = suspend_buf;
	sprint_reg(s, SPUVCKCR);
	sprint_reg(s, SLIMBCKCR);
	sprint_reg(s, HSICKCR);
	sprint_reg(s, M4CKCR);
	sprint_reg(s, DSITCKCR);
	sprint_reg(s, DSI0PCKCR);
	sprint_reg(s, DSI1PCKCR);
	sprint_reg(s, DSI0PHYCR);
	sprint_reg(s, DSI1PHYCR);
	sprint_reg(s, MPMODE);
	sprint_reg(s, PLLECR);
	sprint_reg(s, PLL0CR);
	sprint_reg(s, PLL1CR);
	sprint_reg(s, PLL2CR);
	sprint_reg(s, PLL22CR);
	sprint_reg(s, PLL3CR);
	sprint_reg(s, PLL0STPCR);
	sprint_reg(s, PLL1STPCR);
	sprint_reg(s, PLL2STPCR);
	sprint_reg(s, PLL22STPCR);
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;
	sprint_reg(s, PLL3STPCR);
	sprint_reg(s, MSTPSR0);
	sprint_reg(s, MSTPSR1);
	sprint_reg(s, MSTPSR2);
	sprint_reg(s, MSTPSR3);
	sprint_reg(s, MSTPSR4);
	sprint_reg(s, MSTPSR5);
	sprint_reg(s, MSTPSR6);
	sprint_reg(s, RMSTPCR0);
	sprint_reg(s, RMSTPCR1);
	sprint_reg(s, RMSTPCR2);
	sprint_reg(s, RMSTPCR3);
	sprint_reg(s, RMSTPCR4);
	sprint_reg(s, RMSTPCR5);
	sprint_reg(s, RMSTPCR6);

	sprint_reg(s, SMSTPCR0);
	sprint_reg(s, SMSTPCR1);
	sprint_reg(s, SMSTPCR2);
	sprint_reg(s, SMSTPCR3);
	sprint_reg(s, SMSTPCR4);
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;
	sprint_reg(s, SMSTPCR5);
	sprint_reg(s, SMSTPCR6);
	sprint_reg(s, MMSTPCR0);
	sprint_reg(s, MMSTPCR1);
	sprint_reg(s, MMSTPCR2);
	sprint_reg(s, MMSTPCR3);
	sprint_reg(s, MMSTPCR4);
	sprint_reg(s, MMSTPCR5);
	sprint_reg(s, MMSTPCR6);
	sprint_reg(s, CKSCR);
	sprint_reg(s, PCLKCR);
	sprint_reg(s, SCGCR);
	MSG_INFO("%s", suspend_buf);

	iounmap(vir_addr);

	return;

fail:
	MSG_INFO("%s", suspend_buf);
	return;
}

static void SYSC_dump_suspend(void)
{
	char *s = suspend_buf;

	sprintf(suspend_buf, ">> %s", __func__);
	MSG_INFO("%s", suspend_buf);

	sprint_reg(s, WUPRMSK);
	sprint_reg(s, WUPSMSK);
	sprint_reg(s, WUPMMSK);
	sprint_reg(s, C4POWCR);
	sprint_reg(s, PSTR);
	sprint_reg(s, LPMWUMON);
	sprint_reg(s, EXSTMON2);
	sprint_reg(s, EXSTMON1);
	sprint_reg(s, LPMWUMSKMON);
	sprint_reg(s, WUPRFAC);
	sprint_reg(s, WUPSFAC);
	sprint_reg(s, WUPMFAC);
	sprint_reg(s, RWBCR);
	sprint_reg(s, SWBCR);
	sprint_reg(s, LPMWUCNT);
	sprint_reg(s, LPMWUMSKCNT);
	sprint_reg(s, APSCSTP);
	sprint_reg(s, SYCKENMSK);
	sprint_reg(s, PDNSEL);
	sprint_reg(s, WUPRCR);
	sprint_reg(s, WUPSCR);
	MSG_INFO("%s", suspend_buf);

	return;
}

static void GPIO_dump_suspend(void)
{
	char *s = suspend_buf;

	sprintf(suspend_buf, ">> %s", __func__);
	MSG_INFO("%s", suspend_buf);

	sprint_reg8(s, GPIO_PORTCR(0));
	sprint_reg8(s, GPIO_PORTCR(1));
	sprint_reg8(s, GPIO_PORTCR(2));
	sprint_reg8(s, GPIO_PORTCR(3));
	sprint_reg8(s, GPIO_PORTCR(4));
	sprint_reg8(s, GPIO_PORTCR(5));
	sprint_reg8(s, GPIO_PORTCR(6));
	sprint_reg8(s, GPIO_PORTCR(7));
	sprint_reg8(s, GPIO_PORTCR(8));
	sprint_reg8(s, GPIO_PORTCR(9));
	sprint_reg8(s, GPIO_PORTCR(10));
	sprint_reg8(s, GPIO_PORTCR(11));
	sprint_reg8(s, GPIO_PORTCR(12));
	sprint_reg8(s, GPIO_PORTCR(13));
	sprint_reg8(s, GPIO_PORTCR(14));
	sprint_reg8(s, GPIO_PORTCR(15));
	sprint_reg8(s, GPIO_PORTCR(16));
	sprint_reg8(s, GPIO_PORTCR(17));
	sprint_reg8(s, GPIO_PORTCR(18));
	sprint_reg8(s, GPIO_PORTCR(19));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;
	sprint_reg8(s, GPIO_PORTCR(20));
	sprint_reg8(s, GPIO_PORTCR(21));
	sprint_reg8(s, GPIO_PORTCR(22));
	sprint_reg8(s, GPIO_PORTCR(23));
	sprint_reg8(s, GPIO_PORTCR(24));
	sprint_reg8(s, GPIO_PORTCR(25));
	sprint_reg8(s, GPIO_PORTCR(26));
	sprint_reg8(s, GPIO_PORTCR(27));
	sprint_reg8(s, GPIO_PORTCR(28));
	sprint_reg8(s, GPIO_PORTCR(29));
	sprint_reg8(s, GPIO_PORTCR(30));
	sprint_reg8(s, GPIO_PORTCR(31));
	sprint_reg8(s, GPIO_PORTCR(32));
	sprint_reg8(s, GPIO_PORTCR(33));
	sprint_reg8(s, GPIO_PORTCR(34));
	sprint_reg8(s, GPIO_PORTCR(35));
	sprint_reg8(s, GPIO_PORTCR(36));
	sprint_reg8(s, GPIO_PORTCR(37));
	sprint_reg8(s, GPIO_PORTCR(38));
	sprint_reg8(s, GPIO_PORTCR(39));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(80));
	sprint_reg8(s, GPIO_PORTCR(81));
	sprint_reg8(s, GPIO_PORTCR(82));
	sprint_reg8(s, GPIO_PORTCR(83));
	sprint_reg8(s, GPIO_PORTCR(84));
	sprint_reg8(s, GPIO_PORTCR(85));
	sprint_reg8(s, GPIO_PORTCR(86));
	sprint_reg8(s, GPIO_PORTCR(87));
	sprint_reg8(s, GPIO_PORTCR(88));
	sprint_reg8(s, GPIO_PORTCR(89));
	sprint_reg8(s, GPIO_PORTCR(219));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(90));
	sprint_reg8(s, GPIO_PORTCR(91));
	sprint_reg8(s, GPIO_PORTCR(215));
	sprint_reg8(s, GPIO_PORTCR(216));
	sprint_reg8(s, GPIO_PORTCR(273));
	sprint_reg8(s, GPIO_PORTCR(274));
	sprint_reg8(s, GPIO_PORTCR(275));
	sprint_reg8(s, GPIO_PORTCR(276));
	sprint_reg8(s, GPIO_PORTCR(277));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(261));
	sprint_reg8(s, GPIO_PORTCR(263));
	sprint_reg8(s, GPIO_PORTCR(264));
	sprint_reg8(s, GPIO_PORTCR(265));
	sprint_reg8(s, GPIO_PORTCR(311));
	sprint_reg8(s, GPIO_PORTCR(312));
	sprint_reg8(s, GPIO_PORTCR(218));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(137));
	sprint_reg8(s, GPIO_PORTCR(138));
	sprint_reg8(s, GPIO_PORTCR(140));
	sprint_reg8(s, GPIO_PORTCR(198));
	sprint_reg8(s, GPIO_PORTCR(199));
	sprint_reg8(s, GPIO_PORTCR(200));
	sprint_reg8(s, GPIO_PORTCR(201));
	sprint_reg8(s, GPIO_PORTCR(260));
	sprint_reg8(s, GPIO_PORTCR(262));
	sprint_reg8(s, GPIO_PORTCR(266));
	sprint_reg8(s, GPIO_PORTCR(267));
	sprint_reg8(s, GPIO_PORTCR(268));
	sprint_reg8(s, GPIO_PORTCR(269));
	sprint_reg8(s, GPIO_PORTCR(270));
	sprint_reg8(s, GPIO_PORTCR(271));
	sprint_reg8(s, GPIO_PORTCR(272));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;
	sprint_reg8(s, GPIO_PORTCR(288));
	sprint_reg8(s, GPIO_PORTCR(289));
	sprint_reg8(s, GPIO_PORTCR(290));
	sprint_reg8(s, GPIO_PORTCR(291));
	sprint_reg8(s, GPIO_PORTCR(292));
	sprint_reg8(s, GPIO_PORTCR(293));
	sprint_reg8(s, GPIO_PORTCR(294));
	sprint_reg8(s, GPIO_PORTCR(295));
	sprint_reg8(s, GPIO_PORTCR(296));
	sprint_reg8(s, GPIO_PORTCR(297));
	sprint_reg8(s, GPIO_PORTCR(298));
	sprint_reg8(s, GPIO_PORTCR(299));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(300));
	sprint_reg8(s, GPIO_PORTCR(301));
	sprint_reg8(s, GPIO_PORTCR(302));
	sprint_reg8(s, GPIO_PORTCR(303));
	sprint_reg8(s, GPIO_PORTCR(304));
	sprint_reg8(s, GPIO_PORTCR(305));
	sprint_reg8(s, GPIO_PORTCR(306));
	sprint_reg8(s, GPIO_PORTCR(307));
	sprint_reg8(s, GPIO_PORTCR(308));
	sprint_reg8(s, GPIO_PORTCR(309));
	sprint_reg8(s, GPIO_PORTCR(310));

	sprint_reg8(s, GPIO_PORTCR(44));
	sprint_reg8(s, GPIO_PORTCR(45));
	sprint_reg8(s, GPIO_PORTCR(46));
	sprint_reg8(s, GPIO_PORTCR(47));
	sprint_reg8(s, GPIO_PORTCR(48));
	sprint_reg8(s, GPIO_PORTCR(96));
	sprint_reg8(s, GPIO_PORTCR(97));
	sprint_reg8(s, GPIO_PORTCR(98));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;
	sprint_reg8(s, GPIO_PORTCR(99));
	sprint_reg8(s, GPIO_PORTCR(100));
	sprint_reg8(s, GPIO_PORTCR(101));
	sprint_reg8(s, GPIO_PORTCR(102));
	sprint_reg8(s, GPIO_PORTCR(103));
	sprint_reg8(s, GPIO_PORTCR(104));
	sprint_reg8(s, GPIO_PORTCR(105));
	sprint_reg8(s, GPIO_PORTCR(106));
	sprint_reg8(s, GPIO_PORTCR(107));
	sprint_reg8(s, GPIO_PORTCR(108));
	sprint_reg8(s, GPIO_PORTCR(109));
	sprint_reg8(s, GPIO_PORTCR(110));

	sprint_reg8(s, GPIO_PORTCR(139));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(320));
	sprint_reg8(s, GPIO_PORTCR(321));
	sprint_reg8(s, GPIO_PORTCR(322));
	sprint_reg8(s, GPIO_PORTCR(323));
	sprint_reg8(s, GPIO_PORTCR(324));
	sprint_reg8(s, GPIO_PORTCR(325));
	sprint_reg8(s, GPIO_PORTCR(326));
	sprint_reg8(s, GPIO_PORTCR(327));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(64));
	sprint_reg8(s, GPIO_PORTCR(65));
	sprint_reg8(s, GPIO_PORTCR(66));
	sprint_reg8(s, GPIO_PORTCR(67));
	sprint_reg8(s, GPIO_PORTCR(68));
	sprint_reg8(s, GPIO_PORTCR(69));
	sprint_reg8(s, GPIO_PORTCR(70));
	sprint_reg8(s, GPIO_PORTCR(71));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(130));
	sprint_reg8(s, GPIO_PORTCR(131));
	sprint_reg8(s, GPIO_PORTCR(203));
	sprint_reg8(s, GPIO_PORTCR(204));
	sprint_reg8(s, GPIO_PORTCR(205));
	sprint_reg8(s, GPIO_PORTCR(206));
	sprint_reg8(s, GPIO_PORTCR(207));
	sprint_reg8(s, GPIO_PORTCR(208));
	sprint_reg8(s, GPIO_PORTCR(209));
	sprint_reg8(s, GPIO_PORTCR(210));
	sprint_reg8(s, GPIO_PORTCR(211));
	sprint_reg8(s, GPIO_PORTCR(212));
	sprint_reg8(s, GPIO_PORTCR(213));
	sprint_reg8(s, GPIO_PORTCR(214));
	sprint_reg8(s, GPIO_PORTCR(217));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(72));
	sprint_reg8(s, GPIO_PORTCR(73));
	sprint_reg8(s, GPIO_PORTCR(74));
	sprint_reg8(s, GPIO_PORTCR(75));
	sprint_reg8(s, GPIO_PORTCR(76));
	sprint_reg8(s, GPIO_PORTCR(77));
	sprint_reg8(s, GPIO_PORTCR(78));
	sprint_reg8(s, GPIO_PORTCR(79));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(40));
	sprint_reg8(s, GPIO_PORTCR(41));
	sprint_reg8(s, GPIO_PORTCR(42));
	sprint_reg8(s, GPIO_PORTCR(43));

	sprint_reg8(s, GPIO_PORTCR(202));
	sprint_reg8(s, GPIO_PORTCR(141));
	sprint_reg8(s, GPIO_PORTCR(142));

	sprint_reg8(s, GPIO_PORTCR(133));
	sprint_reg8(s, GPIO_PORTCR(134));
	sprint_reg8(s, GPIO_PORTCR(135));
	sprint_reg8(s, GPIO_PORTCR(136));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg8(s, GPIO_PORTCR(128));
	sprint_reg8(s, GPIO_PORTCR(129));
	sprint_reg8(s, GPIO_PORTCR(224));
	sprint_reg8(s, GPIO_PORTCR(225));
	sprint_reg8(s, GPIO_PORTCR(226));
	sprint_reg8(s, GPIO_PORTCR(227));
	sprint_reg8(s, GPIO_PORTCR(228));
	sprint_reg8(s, GPIO_PORTCR(229));
	sprint_reg8(s, GPIO_PORTCR(230));
	sprint_reg8(s, GPIO_PORTCR(231));
	sprint_reg8(s, GPIO_PORTCR(232));
	sprint_reg8(s, GPIO_PORTCR(233));
	sprint_reg8(s, GPIO_PORTCR(234));
	sprint_reg8(s, GPIO_PORTCR(235));
	sprint_reg8(s, GPIO_PORTCR(236));
	sprint_reg8(s, GPIO_PORTCR(237));
	sprint_reg8(s, GPIO_PORTCR(238));
	sprint_reg8(s, GPIO_PORTCR(239));
	sprint_reg8(s, GPIO_PORTCR(240));
	sprint_reg8(s, GPIO_PORTCR(241));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;
	sprint_reg8(s, GPIO_PORTCR(242));
	sprint_reg8(s, GPIO_PORTCR(243));
	sprint_reg8(s, GPIO_PORTCR(244));
	sprint_reg8(s, GPIO_PORTCR(245));
	sprint_reg8(s, GPIO_PORTCR(246));
	sprint_reg8(s, GPIO_PORTCR(247));
	sprint_reg8(s, GPIO_PORTCR(248));
	sprint_reg8(s, GPIO_PORTCR(249));
	sprint_reg8(s, GPIO_PORTCR(250));
	sprint_reg8(s, GPIO_PORTCR(251));
	sprint_reg8(s, GPIO_PORTCR(252));
	sprint_reg8(s, GPIO_PORTCR(253));
	sprint_reg8(s, GPIO_PORTCR(254));
	sprint_reg8(s, GPIO_PORTCR(255));
	sprint_reg8(s, GPIO_PORTCR(256));
	sprint_reg8(s, GPIO_PORTCR(257));
	sprint_reg8(s, GPIO_PORTCR(258));
	sprint_reg8(s, GPIO_PORTCR(259));
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	sprint_reg(s, GPIO_PORTL031_000DR);
	sprint_reg(s, GPIO_PORTL063_032DR);
	sprint_reg(s, GPIO_PORTL095_064DR);
	sprint_reg(s, GPIO_PORTL127_096DR);
	sprint_reg(s, GPIO_PORTD159_128DR);
	sprint_reg(s, GPIO_PORTR223_192DR);
	sprint_reg(s, GPIO_PORTR255_224DR);
	sprint_reg(s, GPIO_PORTR287_256DR);
	sprint_reg(s, GPIO_PORTR319_288DR);
	sprint_reg(s, GPIO_PORTR351_320DR);
	MSG_INFO("%s", suspend_buf);

	return;
}

static void Other_dump_suspend(void)
{
	char *s = suspend_buf;
	void __iomem *vir_addr = NULL;

	sprintf(suspend_buf, ">> %s", __func__);
	MSG_INFO("%s", suspend_buf);

	sprint_reg(s, MODEM_PLL5_CR);
	sprint_reg(s, MODEM_RFCLK_CR);
	sprint_reg(s, MODEM_HPSSCLK_CR);
	sprint_reg(s, MODEM_WPSSCLK_CR);
	sprint_reg(s, MODEM_FDICLK_CR);
	sprint_reg(s, MODEM_RETMEMPDRS_CR);
	sprint_reg(s, MODEM_PSTR);
	sprint_reg(s, MODEM_MONREG);
	sprint_reg(s, MODEM_MONREG1);
	MSG_INFO("%s", suspend_buf);

	s = suspend_buf;
	sprint_reg(s, THERMAL_THSCR0);
	sprint_reg(s, THERMAL_THSCR1);

	vir_addr = ioremap_nocache(SBSC_SDPDCR0A_PHYS, 0x4);
	if (vir_addr) {
		sprint_regm(s, SBSC_SDPDCR0A_PHYS, vir_addr, 32);
		iounmap(vir_addr);
	} else {
		sprintf(s, "Read 0x%x FAILED\n", SBSC_SDPDCR0A_PHYS);
	}

	MSG_INFO("%s", suspend_buf);

	return;
}

void pmdbg_pmic_dump_suspend(void *pmic_data)
{
#ifdef PMDBG_ENABLE_PMIC_D2153
	int i, ret;
	unsigned char val;
	char *s = suspend_buf;
	struct d2153 *d2153 = (struct d2153 *)pmic_data;

	sprintf(suspend_buf, ">> %s", __func__);
	MSG_INFO("%s", suspend_buf);

	if (NULL == d2153) {
		sprintf(suspend_buf, "pmic_data is NULL");
		goto fail;
	}

	/* Read BUCK, LDO  */
	for (i = D2153_BUCK_1; i < D2153_NUMBER_OF_REGULATORS; i++) {
		ret = d2153_reg_read(d2153, pmic_reg_tbl[i].reg, &val);
		if (0 > ret) {
			sprintf(suspend_buf, "Read 0x%04x FAILED",
						pmic_reg_tbl[i].reg);
			goto fail;
		}

		s += sprintf(s, "0x%04x: 0x%02x\n",
						pmic_reg_tbl[i].reg, val);
	}
	MSG_INFO("%s", suspend_buf);
	s = suspend_buf;

	/* Read MCTL settings */
	for (i = D2153_BUCK_1; i < D2153_NUMBER_OF_REGULATORS; i++) {
		ret = d2153_reg_read(d2153, pmic_reg_tbl[i].mctl_reg, &val);
		if (0 > ret) {
			sprintf(suspend_buf, "Read 0x%04x FAILED",
						pmic_reg_tbl[i].mctl_reg);
			goto fail;
		}

		s += sprintf(s, "0x%04x: 0x%02x\n",
						pmic_reg_tbl[i].mctl_reg, val);
	}
	MSG_INFO("%s", suspend_buf);

	return;

fail:
	MSG_INFO("%s", suspend_buf);
	return;

#else

	/* D2153 Excepting Not Support  */

	return;

#endif /* PMDBG_ENABLE_PMIC_D2153 */
}

char pmdbg_get_enable_dump_suspend(void)
{
	/* enable:1, disable:0 */
	return pmdbg_enable_dump_suspend;
}
