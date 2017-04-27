/*
 * r8a7373 clock framework support
 *
 * Copyright (C) 2012  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * TODO, known issues:
 * - DDR clocks are not supported (as they won't be used in the kernel)
 * - DSI0P, DSI1P clocks are not fully integrated
 * - VCLKCR4.PDIV[1:0] is not supported
 * - MPCKCR.EX2MPDIV[1:0] EXTAL2 prescaler is not supported
 * - FSIACKCR/FSIBCKCR/SLIMBCKCR: reparent is not fully supported (just probed)
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/sh_clk.h>
#include <asm/clkdev.h>
#include <mach/common.h>
#include <mach/r8a7373.h>


/* 26 MHz default rate for the EXTAL1 root input clock */
static struct clk extal1_clk = {
	.rate	= 26000000,
};

/* 48 MHz default rate for the EXTAL2 root input clock */
static struct clk extal2_clk = {
	.rate	= 48000000,
};

/* Fixed 32 KHz root clock from EXTALR pin */
static struct clk extalr_clk = {
	.rate	= 32768,
};

static unsigned long fixed_div_recalc(struct clk *clk)
{
	return clk->parent->rate / (int)(clk->priv);
}

static struct sh_clk_ops fixed_div_clk_ops = {
	.recalc	= fixed_div_recalc,
};

/* EXTAL1 x1/2 */
static struct clk extal1_div2_clk = {
	.parent	= &extal1_clk,
	.ops	= &fixed_div_clk_ops,
	.priv	= (void *)2,
};

/* EXTAL2 x1/2 */
static struct clk extal2_div2_clk = {
	.parent	= &extal2_clk,
	.ops	= &fixed_div_clk_ops,
	.priv	= (void *)2,
};

/* EXTAL2 x1/4 */
static struct clk extal2_div4_clk = {
	.parent	= &extal2_clk,
	.ops	= &fixed_div_clk_ops,
	.priv	= (void *)4,
};

static struct sh_clk_ops followparent_clk_ops = {
	.recalc	= followparent_recalc,
};

/* Main clock */
static struct clk main_clk = {
	.ops	= &followparent_clk_ops,
};

/* Main clock x1/2 */
static struct clk main_div2_clk = {
	.parent	= &main_clk,
	.ops	= &fixed_div_clk_ops,
	.priv	= (void *)2,
};

static struct clk cp_clk = {
	.ops		= &followparent_clk_ops,
	.parent		= &main_div2_clk,
};

/* PLL0, PLL1, PLL2, PLL3, PLL22 */
static unsigned long pll_recalc(struct clk *clk)
{
	unsigned long mult = 1;
	unsigned long stc_mask;

	switch (clk->enable_bit) {
	case 0 ... 3:
		stc_mask = 0x3f;
		break;
	case 4:
		stc_mask = 0x7f;
		break;
	default:
		stc_mask = 0;
	}

	if (__raw_readl(PLLECR) & (1 << clk->enable_bit)) {
		mult = (((__raw_readl(clk->enable_reg) >> 24) & stc_mask) + 1);
		/* handle CFG bit for PLL1, PLL2 and PLL22 */
		switch (clk->enable_bit) {
		case 1:
		case 2:
		case 4:
			if (__raw_readl(clk->enable_reg) & (1 << 20))
				mult *= 2;
		}
	}

	return clk->parent->rate * mult;
}

static struct sh_clk_ops pll_clk_ops = {
	.recalc		= pll_recalc,
};

/* PLL2, PLL22 clock source selection */
static struct clk *pll2_parent[] = {
	[0] = &main_div2_clk,
	[1] = &extal2_div2_clk,
	[2] = NULL,
	[3] = &extal2_div4_clk,
	[4] = &main_clk,
	[5] = &extal2_clk,
};

enum { CKSEL_PLL2, CKSEL_PLL22, CKSEL_PLL_NR };

static struct clk cksel_pll_clks[CKSEL_PLL_NR] = {
	[CKSEL_PLL2] = SH_CLK_CKSEL(PLL2CR, 0, 0,
				pll2_parent, ARRAY_SIZE(pll2_parent), 5, 3),
	[CKSEL_PLL22] = SH_CLK_CKSEL(PLL22CR, 0, 0,
				pll2_parent, ARRAY_SIZE(pll2_parent), 5, 3),
};

/* PLL0 */
static struct clk pll0_clk = {
	.parent		= &main_clk,
	.ops		= &pll_clk_ops,
	.enable_reg	= PLL0CR,
	.enable_bit	= 0,
};

/* PLL1 */
static struct clk pll1_clk = {
	.parent		= NULL, /* set up later */
	.ops		= &pll_clk_ops,
	.enable_reg	= PLL1CR,
	.enable_bit	= 1,
};

/* PLL2 */
static struct clk pll2_clk = {
	.parent		= &cksel_pll_clks[CKSEL_PLL2],
	.ops		= &pll_clk_ops,
	.enable_reg	= PLL2CR,
	.enable_bit	= 2,
};

/* PLL3 */
static struct clk pll3_clk = {
	.parent		= &main_clk,
	.ops		= &pll_clk_ops,
	.enable_reg	= PLL3CR,
	.enable_bit	= 3,
};

/* PLL22 */
static struct clk pll22_clk = {
	.parent		= &cksel_pll_clks[CKSEL_PLL22],
	.ops		= &pll_clk_ops,
	.enable_reg	= PLL22CR,
	.enable_bit	= 4,
};

/* PLL1 x1/2 */
static struct clk pll1_div2_clk = {
	.parent		= &pll1_clk,
	.ops		= &fixed_div_clk_ops,
	.priv		= (void *)2,
};

/* PLL1 x1/7 */
static struct clk pll1_div7_clk = {
	.parent		= &pll1_clk,
	.ops		= &fixed_div_clk_ops,
	.priv		= (void *)7,
};

/* PLL1 x1/13 */
static struct clk pll1_div13_clk = {
	.parent		= &pll1_clk,
	.ops		= &fixed_div_clk_ops,
	.priv		= (void *)13,
};

/* External input clocks for FSI port A/B */
static struct clk fsiack_clk = {};
static struct clk fsibck_clk = {};

static struct clk *main_clks[] = {
	&extal1_clk,
	&extal2_clk,
	&extalr_clk,
	&extal1_div2_clk,
	&extal2_div2_clk,
	&extal2_div4_clk,
	&main_clk,
	&main_div2_clk,
	&cp_clk,
	&fsiack_clk,
	&fsibck_clk,
};

static struct clk *pll_clks[] = {
	&pll0_clk,
	&pll1_clk,
	&pll2_clk,
	&pll3_clk,
	&pll22_clk,
	&pll1_div2_clk,
	&pll1_div7_clk,
	&pll1_div13_clk,
};

static void div4_kick(struct clk *clk)
{
	unsigned long value;

	/* set KICK bit in FRQCRB to update hardware setting */
	value = __raw_readl(FRQCRB);
	value |= 1 << 31;
	__raw_writel(value, FRQCRB);
}

static unsigned int divisors[] = { 2, 3, 4, 6, 8, 12, 16, 18, 24, 0, 36, 48, 7 };

static struct clk_div_mult_table div4_div_mult_table = {
	.divisors	= divisors,
	.nr_divisors	= ARRAY_SIZE(divisors),
};

static struct clk_div4_table div4_table = {
	.div_mult_table	= &div4_div_mult_table,
	.kick		= div4_kick,
};

enum {
	DIV4_I, DIV4_ZG, DIV4_M3, DIV4_B, DIV4_M1, DIV4_M5,
	DIV4_Z, DIV4_ZTR, DIV4_ZT, DIV4_ZX, DIV4_ZS, DIV4_HP,
	DIV4_DDR,
	DIV4_NR
};

static struct clk div4_clks[DIV4_NR] = {
	[DIV4_I]   = SH_CLK_DIV4(&pll1_clk, FRQCRA, 20, 0xdff, 0),
	[DIV4_ZG]  = SH_CLK_DIV4(&pll1_clk, FRQCRA, 16, 0x97f, 0),
	[DIV4_M3]  = SH_CLK_DIV4(&pll1_clk, FRQCRA, 12, 0x1dff, 0),
	[DIV4_B]   = SH_CLK_DIV4(&pll1_clk, FRQCRA, 8, 0xdff, 0),
	[DIV4_M1]  = SH_CLK_DIV4(&pll1_clk, FRQCRA, 4, 0x1dff, 0),
	[DIV4_M5]  = SH_CLK_DIV4(&pll1_clk, FRQCRA, 0, 0x1dff, 0),
	[DIV4_Z]   = SH_CLK_DIV4(NULL,      FRQCRB, 24, 0x097f, 0),
	[DIV4_ZTR] = SH_CLK_DIV4(&pll1_clk, FRQCRB, 20, 0x0dff, 0),
	[DIV4_ZT]  = SH_CLK_DIV4(&pll1_clk, FRQCRB, 16, 0xdff, 0),
	[DIV4_ZX]  = SH_CLK_DIV4(&pll1_clk, FRQCRB, 12, 0xdff, 0),
	[DIV4_ZS]  = SH_CLK_DIV4(&pll1_clk, FRQCRB, 8, 0xdff, 0),
	[DIV4_HP]  = SH_CLK_DIV4(&pll1_clk, FRQCRB, 4, 0xdff, 0),
	[DIV4_DDR] = SH_CLK_DIV4(&pll3_clk, FRQCRD, 0, 0x97f, 0),
};

enum {
	DIV6_ZB, DIV6_SD0, DIV6_SD1, DIV6_SD2,
	DIV6_VCK1, DIV6_VCK2, DIV6_VCK3, DIV6_VCK4, DIV6_VCK5,
	DIV6_FSIA, DIV6_FSIB, DIV6_MP, DIV6_SPUA, DIV6_SPUV,
	DIV6_SLIMB, DIV6_HSI, /* DIV6_M4, DIV6_UF, */
	DIV6_DSIT, DIV6_DSI0P, DIV6_DSI1P,
	DIV6_NR
};

static struct clk *exsrc_parent[] = {
	[0] = &pll1_div2_clk,
	[1] = &pll2_clk,
};

static struct clk *sd_parent[] = {
	[0] = &pll1_div2_clk,
	[1] = &pll2_clk,
	[2] = &pll1_div13_clk,
};

static struct clk *vclk_parent[] = {
	[0] = &pll1_div2_clk,
	[1] = &pll2_clk,
	[2] = NULL,
	[3] = &extal2_clk,
	[4] = &main_div2_clk,
	[5] = &extalr_clk,
	[6] = &main_clk,
};

static struct clk *hsi_parent[] = {
	[0] = &pll1_div2_clk,
	[1] = &pll2_clk,
	[2] = &pll1_div7_clk,
};

static struct clk *ddr_parent[] = {
	[0] = &pll3_clk,
	[1] = &div4_clks[DIV4_DDR],
};

static struct clk ddr_clk = SH_CLK_CKSEL(FRQCRD, 0, 0,
					 ddr_parent, ARRAY_SIZE(ddr_parent), 4,
					 1);

static struct clk zb30_clk = {
	.parent = &ddr_clk,
	.ops = &fixed_div_clk_ops,
	.priv = (void *)2,
};

static struct clk zb30d2_clk = {
	.parent = &ddr_clk,
	.ops = &fixed_div_clk_ops,
	.priv = (void *)4,
};

static struct clk ddr_div2_clk = {
	.parent = &ddr_clk,
	.ops = &fixed_div_clk_ops,
	.priv = (void *)2,
};

static struct clk ddr_div4_clk = {
	.parent = &ddr_clk,
	.ops = &fixed_div_clk_ops,
	.priv = (void *)4,
};

static struct clk *zb30sl_parent[] = {
	[0] = &ddr_div2_clk,
	[1] = &ddr_div4_clk,
};

static struct clk zb30sl_clk = SH_CLK_CKSEL(FRQCRD, 0, 0,
					    zb30sl_parent,
					    ARRAY_SIZE(zb30sl_parent), 15, 1);


static struct clk div6_clks[DIV6_NR] = {
	[DIV6_ZB] = SH_CLK_DIV6_EXT(ZBCKCR, 8, 0,
			exsrc_parent, ARRAY_SIZE(exsrc_parent), 7, 1),
	[DIV6_SD0] = SH_CLK_DIV6_EXT(SD0CKCR, 8, 0,
			sd_parent, ARRAY_SIZE(sd_parent), 6, 2),
	[DIV6_SD1] = SH_CLK_DIV6_EXT(SD1CKCR, 8, 0,
			sd_parent, ARRAY_SIZE(sd_parent), 6, 2),
	[DIV6_SD2] = SH_CLK_DIV6_EXT(SD2CKCR, 8, 0,
			sd_parent, ARRAY_SIZE(sd_parent), 6, 2),
	[DIV6_VCK1] = SH_CLK_DIV6_EXT(VCLKCR1, 8, 0,
			vclk_parent, ARRAY_SIZE(vclk_parent), 12, 3),
	[DIV6_VCK2] = SH_CLK_DIV6_EXT(VCLKCR2, 8, 0,
			vclk_parent, ARRAY_SIZE(vclk_parent), 12, 3),
	[DIV6_VCK3] = SH_CLK_DIV6_EXT(VCLKCR3, 8, 0,
			vclk_parent, ARRAY_SIZE(vclk_parent), 12, 3),
	[DIV6_VCK4] = SH_CLK_DIV6_EXT(VCLKCR4, 8, 0,
			vclk_parent, ARRAY_SIZE(vclk_parent), 12, 3),
	[DIV6_VCK5] = SH_CLK_DIV6_EXT(VCLKCR5, 8, 0,
			vclk_parent, ARRAY_SIZE(vclk_parent), 12, 3),
	[DIV6_FSIA] = SH_CLK_DIV6_EXT(FSIACKCR, 8, 0,
			exsrc_parent, ARRAY_SIZE(exsrc_parent), 6, 1),
	[DIV6_FSIB] = SH_CLK_DIV6_EXT(FSIBCKCR, 8, 0,
			exsrc_parent, ARRAY_SIZE(exsrc_parent), 6, 1),
	[DIV6_MP] = SH_CLK_DIV6_EXT(MPCKCR, 9, CLK_DIV_SHARED,
			exsrc_parent, ARRAY_SIZE(exsrc_parent), 6, 1),
	[DIV6_SPUA] = SH_CLK_DIV6_EXT(SPUACKCR, 8, 0,
			exsrc_parent, ARRAY_SIZE(exsrc_parent), 6, 1),
	[DIV6_SPUV] = SH_CLK_DIV6_EXT(SPUVCKCR, 8, 0,
			exsrc_parent, ARRAY_SIZE(exsrc_parent), 6, 1),
	[DIV6_SLIMB] = SH_CLK_DIV6_EXT(SLIMBCKCR, 8, 0,
			exsrc_parent, ARRAY_SIZE(exsrc_parent), 7, 1),
	[DIV6_HSI] = SH_CLK_DIV6_EXT(HSICKCR, 8, 0,
			hsi_parent, ARRAY_SIZE(hsi_parent), 6, 2),
	[DIV6_DSIT] = SH_CLK_DIV6_EXT(DSITCKCR, 8, 0,
			exsrc_parent, ARRAY_SIZE(exsrc_parent), 7, 1),
	[DIV6_DSI0P] = SH_CLK_DIV6(&pll1_div2_clk, DSI0PCKCR, 0),
	[DIV6_DSI1P] = SH_CLK_DIV6(&pll2_clk, DSI1PCKCR, 0),
};

enum {
	CKSEL_FSIA, CKSEL_FSIB, CKSEL_MP, CKSEL_MPC, CKSEL_MPMP,
	CKSEL_SPUA, CKSEL_SPUV, CKSEL_NR
};

static struct clk *fsia_parent[] = {
	[0] = &div6_clks[DIV6_FSIA],
	[1] = &fsiack_clk,
};

static struct clk *fsib_parent[] = {
	[0] = &div6_clks[DIV6_FSIB],
	[1] = &fsibck_clk,
};

static struct clk *mp_parent[] = {
	[0] = &div6_clks[DIV6_MP],
	[1] = &extal2_clk,
};

static struct clk *spua_parent[] = {
	[0] = &div6_clks[DIV6_SPUA],
	[1] = &extal2_clk,
};

static struct clk *spuv_parent[] = {
	[0] = &div6_clks[DIV6_SPUV],
	[1] = &extal2_clk,
};

static struct clk cksel_clks[CKSEL_NR] = {
	[CKSEL_FSIA] = SH_CLK_CKSEL(FSIACKCR, 0, 0,
				fsia_parent, ARRAY_SIZE(fsia_parent), 7, 1),
	[CKSEL_FSIB] = SH_CLK_CKSEL(FSIBCKCR, 0, 0,
				fsib_parent, ARRAY_SIZE(fsib_parent), 7, 1),
	[CKSEL_MP] = SH_CLK_CKSEL(MPCKCR, 9, 0,
				mp_parent, ARRAY_SIZE(mp_parent), 7, 1),
		/* Do not control CKSTP bit for MP_C Clock */
	[CKSEL_MPC] = SH_CLK_CKSEL(MPCKCR, 10, 0,
				mp_parent, ARRAY_SIZE(mp_parent), 7, 1),
	[CKSEL_MPMP] = SH_CLK_CKSEL(MPCKCR, 11, CLK_CKSEL_CKSTP,
				mp_parent, ARRAY_SIZE(mp_parent), 7, 1),
	[CKSEL_SPUA] = SH_CLK_CKSEL(SPUACKCR, 0, 0,
				spua_parent, ARRAY_SIZE(spua_parent), 7, 1),
	[CKSEL_SPUV] = SH_CLK_CKSEL(SPUVCKCR, 0, 0,
				spuv_parent, ARRAY_SIZE(spuv_parent), 7, 1),
};

static struct clk z_clk = {
	.ops	= &followparent_clk_ops,
};

struct clk *late_main_clks[] = {
	&z_clk,
};
/* Dynamic divider. Only for recalc. No setting capable. */
static unsigned long div_recalc(struct clk *clk)
{
	return clk->parent->rate /
	    ((__raw_readl(clk->enable_reg) >> clk->src_shift
	      & ((1 << clk->src_width) - 1))
	     + 1);
}

static struct sh_clk_ops div_clk_ops = {

	.recalc = div_recalc,
};

static struct clk extcki_clk = { };


static struct clk *dsi0p_parent[] = {
	[0] = &pll1_div2_clk,
	[1] = &pll2_clk,
	[2] = &main_clk,
	[3] = &extal2_clk,
	[4] = &extcki_clk,
};

static struct clk dsi0p0_cksel_clk = SH_CLK_CKSEL(DSI0PCKCR,
						  8, CLK_CKSEL_CKSTP,
						  dsi0p_parent,
						  ARRAY_SIZE(dsi0p_parent), 24,
						  3);

static struct clk dsi0p0_clk = {
	.parent = &dsi0p0_cksel_clk,
	.ops = &div_clk_ops,
	.enable_reg = DSI0PCKCR,
	.src_shift = 16,
	.src_width = 6,
};

static struct clk dsi0p1_cksel_clk = SH_CLK_CKSEL(DSI0PCKCR,
						  8, CLK_CKSEL_CKSTP,
						  dsi0p_parent,
						  ARRAY_SIZE(dsi0p_parent), 12,
						  3);

static struct clk dsi0p1_clk = {
	.parent = &dsi0p1_cksel_clk,
	.ops = &div_clk_ops,
	.enable_reg = DSI0PCKCR,
	.src_shift = 0,
	.src_width = 6,
};


static struct clk *dsi1p_parent[] = {
	[0] = &pll2_clk,
	[1] = &pll1_div2_clk,
	[2] = &main_clk,
	[3] = &extal2_clk,
	[4] = &extcki_clk,
};

static struct clk dsi1p0_cksel_clk = SH_CLK_CKSEL(DSI1PCKCR,
						  8, CLK_CKSEL_CKSTP,
						  dsi1p_parent,
						  ARRAY_SIZE(dsi1p_parent), 24,
						  3);
static struct clk dsi1p0_clk = {
	.parent = &dsi1p0_cksel_clk,
	.ops = &div_clk_ops,
	.enable_reg = DSI1PCKCR,
	.src_shift = 16,
	.src_width = 6,
};

static struct clk dsi1p1_cksel_clk = SH_CLK_CKSEL(DSI1PCKCR,
						  8, CLK_CKSEL_CKSTP,
						  dsi1p_parent,
						  ARRAY_SIZE(dsi1p_parent), 12,
						  3);

static struct clk dsi1p1_clk = {
	.parent = &dsi1p1_cksel_clk,
	.ops = &div_clk_ops,
	.enable_reg = DSI1PCKCR,
	.src_shift = 0,
	.src_width = 6,
};

static int selmon_reparent(struct clk *clk, struct clk *parent)
{
	int sel = (__raw_readl(clk->enable_reg) >> clk->src_shift)
	    & ((1 << clk->src_width) - 1);
	return clk_reparent(clk, clk->parent_table[sel]);
}

static int selmon_enable(struct clk *clk)
{
	__raw_writel(__raw_readl(clk->enable_reg) & ~(1 << clk->enable_bit),
		     clk->enable_reg);
	return 0;
}

static void selmon_disable(struct clk *clk)
{
	__raw_writel(__raw_readl(clk->enable_reg) | (1 << clk->enable_bit),
		     clk->enable_reg);
}

static struct sh_clk_ops selmon_clk_ops = {
	.recalc = followparent_recalc,
	.set_parent = selmon_reparent,
	.enable = selmon_enable,
	.disable = selmon_disable,
};

static struct clk *selmon0_parent[2] = {
	[0] = &dsi0p0_clk,
	[1] = &dsi0p1_clk,
};

static struct clk *selmon1_parent[2] = {
	[0] = &dsi1p0_clk,
	[1] = &dsi1p1_clk,
};

static struct clk dsi0p_clk = {
	.ops = &selmon_clk_ops,
	.enable_reg = DSI0PCKCR,
	.enable_bit = 8,
	.parent_table = selmon0_parent,
	.src_shift = 27,
	.src_width = 1,
};

static struct clk dsi1p_clk = {
	.enable_reg = DSI1PCKCR,
	.enable_bit = 8,
	.parent_table = selmon1_parent,
	.src_shift = 27,
	.src_width = 1,
};


enum {
	MSTP031, MSTP030, MSTP029, MSTP026,
	MSTP022, MSTP021, MSTP019, MSTP018, MSTP017, MSTP016,
	MSTP015, MSTP007, MSTP001, MSTP000,

	MSTP130, MSTP128, MSTP126, MSTP125, MSTP124,
	MSTP122, MSTP121, MSTP119, MSTP118, MSTP117, MSTP116,
	MSTP115, MSTP113, MSTP112, MSTP111, MSTP108,
	MSTP107, MSTP106, MSTP105, MSTP104, MSTP101, MSTP100,

	MSTP229, MSTP228, MSTP224,
	MSTP223, MSTP220, MSTP218, MSTP217, MSTP216,
	MSTP215, MSTP214, MSTP213, MSTP209, MSTP208,
	MSTP207, MSTP206, MSTP205, MSTP204, MSTP203, MSTP202, MSTP201,

	MSTP330, MSTP329, MSTP328, MSTP326, MSTP325, MSTP324,
	MSTP323, MSTP322, MSTP321, MSTP319, MSTP316,
	MSTP315, MSTP314, MSTP313, MSTP312, MSTP309,
	MSTP307, MSTP306, MSTP305, MSTP304,

	MSTP431, MSTP428, MSTP427, MSTP426, MSTP425, MSTP424,
	MSTP423, MSTP413, MSTP412, MSTP411,
	MSTP407, MSTP406, MSTP403, MSTP402,

	MSTP530, MSTP529, MSTP528, MSTP527, MSTP526, MSTP525, MSTP524,
	MSTP523, MSTP522, MSTP519, MSTP518, MSTP517, MSTP516,
	MSTP508, MSTP507, MSTP501, MSTP500,

	MSTP631, MSTP627, MSTP623, MSTP622, MSTP618,
	MSTP615, MSTP614, MSTP613, MSTP612, MSTP610,
	MSTP604, MSTP601,

	MSTP_NR
};

#define MSTP(_reg, _bit, _parent, _flags) \
	SH_CLK_MSTP32_EXT(_parent, SMSTPCR##_reg, MSTPSR##_reg, _bit, _flags)

static struct clk mstp_clks[MSTP_NR] = {
	[MSTP031] = MSTP(0, 31, &div4_clks[DIV4_I], 0), /* RT-CPU TLB */
	[MSTP030] = MSTP(0, 30, &div4_clks[DIV4_I], 0), /* RT-CPU Instruction Cache (IC) */
	[MSTP029] = MSTP(0, 29, &div4_clks[DIV4_I], 0), /* RT-CPU Operand Cache (OC) */
	[MSTP026] = MSTP(0, 26, &div4_clks[DIV4_I], 0), /* RT-CPU X/Y memory */
	[MSTP022] = MSTP(0, 22, &div4_clks[DIV4_HP], 0), /* INTC-RT */
	[MSTP021] = MSTP(0, 21, &div4_clks[DIV4_ZS], 0), /* RT-DMAC */
	[MSTP019] = MSTP(0, 19, &div4_clks[DIV4_HP], 0), /* H-UDI */
	[MSTP018] = MSTP(0, 18, &div4_clks[DIV4_HP], 0), /* RT-CPU debug module 1 */
	[MSTP017] = MSTP(0, 17, &div4_clks[DIV4_ZS], 0), /* UBC */
	[MSTP016] = MSTP(0, 16, &div4_clks[DIV4_HP], 0), /* RT-CPU debug module 2 */
	[MSTP015] = MSTP(0, 15, &div4_clks[DIV4_ZS], 0), /* ILRAM */
	[MSTP007] = MSTP(0,  7, &div4_clks[DIV4_B], 0), /* ICB */
	[MSTP001] = MSTP(0,  1, &div4_clks[DIV4_HP], 0), /* IIC2 */
	[MSTP000] = MSTP(0,  0, &cksel_clks[CKSEL_MP], 0), /* MSIOF0 */

	[MSTP130] = MSTP(1, 30, &div4_clks[DIV4_ZS], 0), /* VSP */
	[MSTP128] = MSTP(1, 28, &div4_clks[DIV4_B], 0), /* CSI2-RX1 */
	[MSTP126] = MSTP(1, 26, &div4_clks[DIV4_B], 0), /* CSI2-RX0 */
	[MSTP125] = MSTP(1, 25, &cp_clk, 0), /* TMU (secure) */
	[MSTP124] = MSTP(1, 24, &div4_clks[DIV4_B], 0), /* CMT0 */
	[MSTP122] = MSTP(1, 22, &cksel_clks[CKSEL_MP], 0), /* TMU2 */
	[MSTP121] = MSTP(1, 21, &cksel_clks[CKSEL_MP], 0), /* TMU3 */
	[MSTP119] = MSTP(1, 19, &div4_clks[DIV4_ZS], 0), /* ISP */
	[MSTP118] = MSTP(1, 18, &div4_clks[DIV4_B], 0), /* DSI-TX0 */
	[MSTP117] = MSTP(1, 17, &div4_clks[DIV4_M3], 0), /* LCDC1 */
	[MSTP116] = MSTP(1, 16, &div4_clks[DIV4_HP], 0), /* IIC0 */
	[MSTP115] = MSTP(1, 15, &div4_clks[DIV4_B], 0), /* 2D-DMAC */
	[MSTP113] = MSTP(1, 13, &div4_clks[DIV4_ZS], 0), /* S3$ */
	[MSTP112] = MSTP(1, 12, &div4_clks[DIV4_ZG], 0), /* 3DG */
	[MSTP111] = MSTP(1, 11, &cksel_clks[CKSEL_MP], 0), /* TMU1 */
	[MSTP108] = MSTP(1,  8, &div4_clks[DIV4_HP], 0), /* TSIF */
	[MSTP107] = MSTP(1,  7, NULL, 0), /* RCUA0 */
	[MSTP106] = MSTP(1,  6, &div4_clks[DIV4_M5], 0), /* JPU */
	[MSTP105] = MSTP(1,  5, NULL, 0), /* RCUA1 */
	[MSTP104] = MSTP(1,  4, &div4_clks[DIV4_M5], 0), /* JPU6E */
	[MSTP101] = MSTP(1,  1, &div4_clks[DIV4_M1], 0), /* VCP */
	[MSTP100] = MSTP(1,  0, &div4_clks[DIV4_M3], 0), /* LCDC0 */

	[MSTP229] = MSTP(2, 29, &div4_clks[DIV4_HP], 0), /* CC4.2(public) */
	[MSTP228] = MSTP(2, 28, &div4_clks[DIV4_HP], 0), /* CC4.2(secure) */
	[MSTP224] = MSTP(2, 24, &cksel_clks[CKSEL_MPMP], 0), /* CLKGEN */
	[MSTP223] = MSTP(2, 23, &cksel_clks[CKSEL_SPUA], 0), /* SPU2A */
	[MSTP220] = MSTP(2, 20, &cksel_clks[CKSEL_SPUV], 0), /* SPU2V */
	[MSTP218] = MSTP(2, 18, &div4_clks[DIV4_HP], 0), /* SY-DMAC */
	[MSTP217] = MSTP(2, 17, &cksel_clks[CKSEL_MP], 0), /* SCIFB3 */
	[MSTP216] = MSTP(2, 16, &cksel_clks[CKSEL_MP], 0), /* SCIFB2 */
	[MSTP215] = MSTP(2, 15, &cksel_clks[CKSEL_MP], 0), /* MSIOF3 */
	[MSTP214] = MSTP(2, 14, &cksel_clks[CKSEL_MP], 0), /* USB-DMAC0 */
	[MSTP213] = MSTP(2, 13, &div4_clks[DIV4_HP], 0), /* MFI */
	[MSTP209] = MSTP(2,  9, &cksel_clks[CKSEL_MP], 0), /* MSIOF4 */
	[MSTP208] = MSTP(2,  8, &cksel_clks[CKSEL_MP], 0), /* MSIOF1 */
	[MSTP207] = MSTP(2,  7, &cksel_clks[CKSEL_MP], 0), /* SCIFB1 */
	[MSTP206] = MSTP(2,  6, &cksel_clks[CKSEL_MP], 0), /* SCIFB0 */
	[MSTP205] = MSTP(2,  5, &cksel_clks[CKSEL_MP], 0), /* MSIOF2 */
	[MSTP204] = MSTP(2,  4, &cksel_clks[CKSEL_MPC], 0), /* SCIFA0 */
	[MSTP203] = MSTP(2,  3, &cksel_clks[CKSEL_MP], 0), /* SCIFA1 */
	[MSTP202] = MSTP(2,  2, &cksel_clks[CKSEL_MP], 0), /* SCIFA2 */
	[MSTP201] = MSTP(2,  1, &cksel_clks[CKSEL_MP], 0), /* SCIFA3 */

	[MSTP330] = MSTP(3, 30, NULL, 0), /* HSI-DMAC */
	[MSTP329] = MSTP(3, 29, &cp_clk, 0), /* CMT1 */
	[MSTP328] = MSTP(3, 28, &cksel_clks[CKSEL_MPMP], 0), /* FSI */
	[MSTP326] = MSTP(3, 26, &cksel_clks[CKSEL_MPMP], 0), /* SCUW */
	[MSTP325] = MSTP(3, 25, &div6_clks[DIV6_HSI], 0), /* HSI1 */
	[MSTP324] = MSTP(3, 24, &div6_clks[DIV6_HSI], 0), /* HSI0 */
	[MSTP323] = MSTP(3, 23, &div4_clks[DIV4_HP], 0), /* IIC1 */
	[MSTP322] = MSTP(3, 22, &cksel_clks[CKSEL_MP], 0), /* USB */
	[MSTP321] = MSTP(3, 21, NULL, 0), /* SBSC performance monitor */
	[MSTP319] = MSTP(3, 19, NULL, 0), /* RT-DMAC scheduler */
	[MSTP316] = MSTP(3, 16, &div4_clks[DIV4_ZS], 0), /* SHWYSTAT */
	[MSTP315] = MSTP(3, 15, &div4_clks[DIV4_HP], 0), /* MMCIF0 */
	[MSTP314] = MSTP(3, 14, &div6_clks[DIV6_SD0], 0), /* SDHI0 */
	[MSTP313] = MSTP(3, 13, &div6_clks[DIV6_SD1], 0), /* SDHI1 */
	[MSTP312] = MSTP(3, 12, &div6_clks[DIV6_SD2], 0), /* SDHI2 */
	[MSTP309] = MSTP(3,  9, &cksel_clks[CKSEL_MP], 0), /* ICUSB */
	[MSTP307] = MSTP(3,  7, &cksel_clks[CKSEL_MPC], 0), /* ICUSB-DMAC2 */
	[MSTP306] = MSTP(3,  6, &cksel_clks[CKSEL_MPC], 0), /* ICUSB-DMAC1 */
	[MSTP305] = MSTP(3,  5, &div4_clks[DIV4_HP], 0), /* MMCIF1 */
	[MSTP304] = MSTP(3,  4, &cp_clk, 0), /* TPU0 */

	[MSTP431] = MSTP(4, 31, &cp_clk, 0), /* Secure Up-Time Clock */
	[MSTP428] = MSTP(4, 28, &cp_clk, 0), /* IICDVM */
	[MSTP427] = MSTP(4, 27, &div4_clks[DIV4_HP], 0), /* IIC3H */
	[MSTP426] = MSTP(4, 26, &div4_clks[DIV4_HP], 0), /* IIC2H */
	[MSTP425] = MSTP(4, 25, &div4_clks[DIV4_HP], 0), /* IIC1H */
	[MSTP424] = MSTP(4, 24, &div4_clks[DIV4_HP], 0), /* IIC0H */
	[MSTP423] = MSTP(4, 23, &div4_clks[DIV4_B], 0), /* DSI-TX1 */
	[MSTP413] = MSTP(4, 13, NULL, 0), /* MTB-S */
	[MSTP412] = MSTP(4, 12, &cksel_clks[CKSEL_MP], 0), /* IICM */
	[MSTP411] = MSTP(4, 11, &div4_clks[DIV4_HP], 0), /* IIC3 */
	[MSTP407] = MSTP(4,  7, &cksel_clks[CKSEL_MP], 0), /* USB-DMAC1 */
	[MSTP406] = MSTP(4,  6, NULL, 0), /* DDM */
	[MSTP403] = MSTP(4,  3, &cp_clk, 0), /* KEYSC */
	[MSTP402] = MSTP(4,  2, &cp_clk, 0), /* RWDT0 */

	[MSTP530] = MSTP(5, 30, &div4_clks[DIV4_HP], 0), /* Secure boot ROM */
	[MSTP529] = MSTP(5, 29, &div4_clks[DIV4_HP], CLK_ENABLE_ON_INIT), /* Secure RAM */
	[MSTP528] = MSTP(5, 28, &div4_clks[DIV4_HP], 0), /* Inter connect RAM1 */
	[MSTP527] = MSTP(5, 27, &div4_clks[DIV4_HP], CLK_ENABLE_ON_INIT), /* Inter connect RAM0 */
	[MSTP526] = MSTP(5, 26, &div4_clks[DIV4_HP], 0), /* Public boot ROM */
	[MSTP525] = MSTP(5, 25, &div4_clks[DIV4_HP], 0), /* IICB0 */
	[MSTP524] = MSTP(5, 24, NULL, 0), /* SLIMBUS */
	[MSTP523] = MSTP(5, 23, &cksel_clks[CKSEL_MP], 0), /* PCM2PWM */
	[MSTP522] = MSTP(5, 22, NULL, 0), /* Thermal Sensor */
	[MSTP519] = MSTP(5, 19, NULL, 0), /* OCP2SHWY */
	[MSTP518] = MSTP(5, 18, NULL, 0), /* O2M */
	[MSTP517] = MSTP(5, 17, NULL, 0), /* S2O1 */
	[MSTP516] = MSTP(5, 16, NULL, 0), /* S2O0 */
	[MSTP508] = MSTP(5,  8, &div4_clks[DIV4_HP], 0), /* INTC-BB */
	[MSTP507] = MSTP(5,  7, &div4_clks[DIV4_HP], 0), /* IRQC(INTC-SYS) */
	[MSTP501] = MSTP(5,  1, &div4_clks[DIV4_HP], 0), /* SPU2A Core1 */
	[MSTP500] = MSTP(5,  0, &div4_clks[DIV4_HP], 0), /* SPU2A Core0 */

	[MSTP631] = MSTP(6, 31, NULL, 0), /* M4 */
	[MSTP627] = MSTP(6, 27, NULL, 0), /* SF-DMAC */
	[MSTP623] = MSTP(6, 23, NULL, 0), /* MSIOF6 */
	[MSTP622] = MSTP(6, 22, NULL, 0), /* MSIOF5 */
	[MSTP618] = MSTP(6, 18, NULL, 0), /* COMREG */
	[MSTP615] = MSTP(6, 15, NULL, 0), /* SF-IIC3 */
	[MSTP614] = MSTP(6, 14, NULL, 0), /* SF-IIC2 */
	[MSTP613] = MSTP(6, 13, NULL, 0), /* SF-IIC1 */
	[MSTP612] = MSTP(6, 12, NULL, 0), /* SF-IIC0 */
	[MSTP610] = MSTP(6, 10, NULL, 0), /* SF-CMT */
	[MSTP604] = MSTP(6,  4, NULL, 0), /* EVDT */
	[MSTP601] = MSTP(6,  1, NULL, 0), /* TPU1 */
};

static struct clk_lookup lookups[] = {
	/* main clocks */
	CLKDEV_CON_ID("extal1_clk", &extal1_clk),
	CLKDEV_CON_ID("extal2_clk", &extal2_clk),
	CLKDEV_CON_ID("r_clk", &extalr_clk),
	CLKDEV_CON_ID("main_clk", &main_clk),
	CLKDEV_CON_ID("cp_clk", &cp_clk),
	CLKDEV_CON_ID("fsiack_clk", &fsiack_clk),
	CLKDEV_CON_ID("fsibck_clk", &fsibck_clk),

	/* PLL clocks */
	CLKDEV_CON_ID("pll1_div2_clk", &pll1_div2_clk),
	CLKDEV_CON_ID("pll1_clk", &pll1_clk),
	CLKDEV_CON_ID("pll2_clk", &pll2_clk),
	CLKDEV_CON_ID("pll3_clk", &pll3_clk),
	CLKDEV_CON_ID("pll22_clk", &pll22_clk),

	/* DIV4 clocks */
	CLKDEV_CON_ID("i_clk", &div4_clks[DIV4_I]),
	CLKDEV_CON_ID("zg_clk", &div4_clks[DIV4_ZG]),
	CLKDEV_CON_ID("m3_clk", &div4_clks[DIV4_M3]),
	CLKDEV_CON_ID("b_clk", &div4_clks[DIV4_B]),
	CLKDEV_CON_ID("m1_clk", &div4_clks[DIV4_M1]),
	CLKDEV_CON_ID("m5_clk", &div4_clks[DIV4_M5]),
	CLKDEV_CON_ID("z_clk", &z_clk),
	CLKDEV_CON_ID("ztr_clk", &div4_clks[DIV4_ZTR]),
	CLKDEV_CON_ID("zt_clk", &div4_clks[DIV4_ZT]),
	CLKDEV_CON_ID("zx_clk", &div4_clks[DIV4_ZX]),
	CLKDEV_CON_ID("zs_clk", &div4_clks[DIV4_ZS]),
	CLKDEV_CON_ID("hp_clk", &div4_clks[DIV4_HP]),

	CLKDEV_CON_ID("ddr_clk", &div4_clks[DIV4_DDR]),
	CLKDEV_CON_ID("ddr_div2_clk", &ddr_div2_clk),
	CLKDEV_CON_ID("zb30_clk", &zb30_clk),

	/* DIV6 clocks */
	CLKDEV_CON_ID("zb_clk", &div6_clks[DIV6_ZB]),
	CLKDEV_CON_ID("vclk1_clk", &div6_clks[DIV6_VCK1]),
	CLKDEV_CON_ID("vclk2_clk", &div6_clks[DIV6_VCK2]),
	CLKDEV_CON_ID("vclk3_clk", &div6_clks[DIV6_VCK3]),
	CLKDEV_CON_ID("vclk4_clk", &div6_clks[DIV6_VCK4]),
	CLKDEV_CON_ID("vclk5_clk", &div6_clks[DIV6_VCK5]),
	CLKDEV_CON_ID("fsia_clk", &cksel_clks[CKSEL_FSIA]),
	CLKDEV_CON_ID("fsib_clk", &cksel_clks[CKSEL_FSIB]),
	CLKDEV_CON_ID("mp_clk", &cksel_clks[CKSEL_MP]),
	CLKDEV_CON_ID("mpc_clk", &cksel_clks[CKSEL_MPC]),
	CLKDEV_CON_ID("mpmp_clk", &cksel_clks[CKSEL_MPMP]),
	CLKDEV_CON_ID("spua_clk", &cksel_clks[CKSEL_SPUA]),
	CLKDEV_CON_ID("spuv_clk", &cksel_clks[CKSEL_SPUV]),
	CLKDEV_CON_ID("slimb_clk", &div6_clks[DIV6_SLIMB]),
	CLKDEV_ICK_ID("dsit_clk", "sh-mipi-dsi.0", &div6_clks[DIV6_DSIT]),
	CLKDEV_ICK_ID("dsip_clk", "sh-mipi-dsi.0", &div6_clks[DIV6_DSI0P]),

	CLKDEV_CON_ID("icb", &mstp_clks[MSTP007]), /* ICB */
	CLKDEV_CON_ID("meram", &mstp_clks[MSTP113]), /* S3$ */
	CLKDEV_CON_ID("clkgen", &mstp_clks[MSTP224]), /* CLKGEN */
	CLKDEV_CON_ID("spuv", &mstp_clks[MSTP220]), /* SPU2V */
	CLKDEV_CON_ID("usb0_dmac", &mstp_clks[MSTP214]), /* USBHS-DMAC */
	CLKDEV_CON_ID("mfis", &mstp_clks[MSTP213]), /* MFI */
	CLKDEV_CON_ID("fsi", &mstp_clks[MSTP328]), /* FSI */
	CLKDEV_CON_ID("scuw", &mstp_clks[MSTP326]), /* SCUW */
	CLKDEV_CON_ID("rwdt0", &mstp_clks[MSTP402]), /* RWDT0 */
	CLKDEV_CON_ID("Crypt1", &mstp_clks[MSTP229]),/* Crypt1 */

	CLKDEV_DEV_ID("i2c-sh_mobile.2", &mstp_clks[MSTP001]), /* IIC2 */
	CLKDEV_DEV_ID("spi_sh_msiof.0", &mstp_clks[MSTP000]), /* MSIOF0 */
	CLKDEV_DEV_ID("spi_sh_msiof.1", &mstp_clks[MSTP208]),   /* MSIOF1 */
	CLKDEV_DEV_ID("spi_sh_msiof.2", &mstp_clks[MSTP205]),   /* MSIOF2 */
	CLKDEV_DEV_ID("spi_sh_msiof.3", &mstp_clks[MSTP215]),   /* MSIOF3 */
	CLKDEV_DEV_ID("spi_sh_msiof.4", &mstp_clks[MSTP209]),   /* MSIOF4 */
	CLKDEV_DEV_ID("sh-mobile-csi2.1", &mstp_clks[MSTP128]), /* CSI2-RX1 */
	CLKDEV_DEV_ID("sh-mobile-csi2.0", &mstp_clks[MSTP126]), /* CSI2-RX0 */
	CLKDEV_DEV_ID("sh-mipi-dsi.0", &mstp_clks[MSTP118]), /* DSI-TX0 */
	CLKDEV_DEV_ID("i2c-sh_mobile.0", &mstp_clks[MSTP116]), /* IIC0 */
	CLKDEV_DEV_ID("pvrsrvkm", &mstp_clks[MSTP112]), /* SGX544 */
	CLKDEV_DEV_ID("sh_mobile_rcu.0", &mstp_clks[MSTP107]), /* RCUA0 */
	CLKDEV_DEV_ID("sh_mobile_rcu.1", &mstp_clks[MSTP105]), /* RCUA1 */
	CLKDEV_DEV_ID("sh_mobile_lcdc_fb.0", &mstp_clks[MSTP100]), /* LCDC0 */
	CLKDEV_DEV_ID("sh-dma-engine.0", &mstp_clks[MSTP218]), /* DMAC */
	CLKDEV_DEV_ID("sh-sci.7", &mstp_clks[MSTP217]), /* SCIFB3 */
	CLKDEV_DEV_ID("sh-sci.6", &mstp_clks[MSTP216]), /* SCIFB2 */
	CLKDEV_DEV_ID("sh-sci.5", &mstp_clks[MSTP207]), /* SCIFB1 */
	CLKDEV_DEV_ID("sh-sci.4", &mstp_clks[MSTP206]), /* SCIFB0 */
	CLKDEV_DEV_ID("sh-sci.0", &mstp_clks[MSTP204]), /* SCIFA0 */
	CLKDEV_DEV_ID("sh-sci.1", &mstp_clks[MSTP203]), /* SCIFA1 */
	CLKDEV_DEV_ID("sh-sci.2", &mstp_clks[MSTP202]), /* SCIFA2 */
	CLKDEV_DEV_ID("sh-sci.3", &mstp_clks[MSTP201]), /* SCIFA3 */
	CLKDEV_DEV_ID("currtimer", &mstp_clks[MSTP329]), /* CMT1 */
	CLKDEV_DEV_ID("sh_cmt.10", &mstp_clks[MSTP329]), /* CMT1 */
	CLKDEV_DEV_ID("sh_cmt.11", &mstp_clks[MSTP329]), /* CMT1 */
	CLKDEV_DEV_ID("sh_cmt.12", &mstp_clks[MSTP329]), /* CMT1 */
	CLKDEV_DEV_ID("sh_cmt.13", &mstp_clks[MSTP329]), /* CMT1 */
	CLKDEV_DEV_ID("sh_cmt.14", &mstp_clks[MSTP329]), /* CMT1 */
	CLKDEV_DEV_ID("sh_cmt.15", &mstp_clks[MSTP329]), /* CMT1 */
	CLKDEV_DEV_ID("sh_cmt.16", &mstp_clks[MSTP329]), /* CMT1 */
	CLKDEV_DEV_ID("sh_cmt.17", &mstp_clks[MSTP329]), /* CMT1 */
	CLKDEV_DEV_ID("sh_fsi2.0", &mstp_clks[MSTP328]), /* FSI */
	CLKDEV_DEV_ID("sh_fsi2.1", &mstp_clks[MSTP328]), /* FSI */
	CLKDEV_DEV_ID("i2c-sh_mobile.1", &mstp_clks[MSTP323]), /* IIC1 */
	CLKDEV_DEV_ID("r8a66597_udc.0", &mstp_clks[MSTP322]), /* USBHS */
	CLKDEV_DEV_ID("renesas_mmcif.0", &mstp_clks[MSTP315]), /* MMCIF0 */
	CLKDEV_DEV_ID("renesas_sdhi.0", &mstp_clks[MSTP314]), /* SDHI0 */
	CLKDEV_DEV_ID("renesas_sdhi.1", &mstp_clks[MSTP313]), /* SDHI1 */
	CLKDEV_DEV_ID("renesas_sdhi.2", &mstp_clks[MSTP312]), /* SDHI2 */
	CLKDEV_DEV_ID("tpu-renesas-sh_mobile.0", &mstp_clks[MSTP304]), /* TPU0 */
	CLKDEV_DEV_ID("i2c-sh_mobile.7", &mstp_clks[MSTP427]), /* IIC3H */
	CLKDEV_DEV_ID("i2c-sh_mobile.6", &mstp_clks[MSTP426]), /* IIC2H */
	CLKDEV_DEV_ID("i2c-sh_mobile.5", &mstp_clks[MSTP425]), /* IIC1H */
	CLKDEV_DEV_ID("i2c-sh_mobile.4", &mstp_clks[MSTP424]), /* IIC0H */
	CLKDEV_DEV_ID("i2c-sh7730.8", &mstp_clks[MSTP412]), /* IICM */
	CLKDEV_DEV_ID("i2c-sh_mobile.3", &mstp_clks[MSTP411]), /* IIC3 */
	CLKDEV_DEV_ID("sh_keysc.0", &mstp_clks[MSTP403]), /* KEYSC */
	CLKDEV_DEV_ID("pcm2pwm-renesas-sh_mobile.1", &mstp_clks[MSTP523]), /* PCM2PWM */
	CLKDEV_DEV_ID("thermal_sensor.0", &mstp_clks[MSTP522]), /* Thermal Sensor */

	/* DSI0P clocks */
	CLKDEV_CON_ID("dsi0p_clk", &div6_clks[DIV6_DSI0P]),
	CLKDEV_CON_ID("dsi0p0_clk", &dsi0p0_clk),
	CLKDEV_CON_ID("dsi0p1_clk", &dsi0p1_clk),
	/* DSI1P clocks */
	CLKDEV_CON_ID("dsi1p_clk", &div6_clks[DIV6_DSI1P]),
	CLKDEV_CON_ID("dsi1p0_clk", &dsi1p0_clk),
	CLKDEV_CON_ID("dsi1p1_clk", &dsi1p1_clk),

	CLKDEV_CON_ID("csi21", &mstp_clks[MSTP128]), /* CSI2-RX1 */
	CLKDEV_CON_ID("csi20", &mstp_clks[MSTP126]), /* CSI2-RX0 */
#ifdef CONFIG_USB_R8A66597_HCD
	CLKDEV_DEV_ID("r8a66597_hcd.0", &mstp_clks[MSTP322]),/* USBHS */
#endif
#ifdef CONFIG_USB_OTG
	CLKDEV_DEV_ID("tusb1211_driver.0",\
					&mstp_clks[MSTP322]),/*USBHS*/
#endif


};

void __init r8a7373_clock_init(void)
{
	int k, ret = 0;

	/* detect main clock parent */
	switch ((__raw_readl(CKSCR) >> 28) & 0x03) {
	case 0:
		main_clk.parent = &extal1_clk;
		break;
	case 1:
		main_clk.parent = &extal1_div2_clk;
		break;
	case 2:
		main_clk.parent = &extal2_clk;
		break;
	case 3:
		main_clk.parent = &extal2_div2_clk;
		break;
	}

	/* detect PLL1 clock parent */
	if (__raw_readl(PLL1CR) & (1 << 7))
		pll1_clk.parent = &main_div2_clk;
	else
		pll1_clk.parent = &main_clk;

	/* detect System-CPU clock parent */
	if (__raw_readl(PLLECR) & (1 << 8)) { /* PLL0ST */
		div4_clks[DIV4_Z].parent = &pll0_clk;

		if (__raw_readl(FRQCRB) & (1 << 28)) /* ZSEL */
			z_clk.parent = &div4_clks[DIV4_Z];
		else
			z_clk.parent = &pll0_clk;
	} else {
		div4_clks[DIV4_Z].parent = &main_clk;
		z_clk.parent = &main_clk;
	}

	/*
	 * adjust DIV6 clock parent - check to see if PLL22 is used or not
	 *
	 * FSIACKCR, FSIBCKCR, SLIMBCKCR registers are capable of selecting
	 * PLL22 output as their clock source, and equipped with a dedicated
	 * clock source selection bit, called EXSRC2.
	 *
	 * However, EXSRC2 is assigned at a short distance away from EXSRC1
	 * like this:
	 *
	 *           EXSRC2 EXSRC1      EXSRC[1:0]
	 * ------------------------       00: PLL circuit 1 output x1/2
	 * FSIACKCR    [12] [6]           01: PLL circuit 2 output
	 * FSIBCKCR    [12] [6]           10: Setting prohibited
	 * SLIMBCKCR   [12] [7]           10: PLL circuit 22 output
	 *
	 * Unfortunately, SH_CLK_DIV6_EXT() can not handle such fragmented
	 * EXSRC[1:0] properly, and sh_clk_div6_reparent_register() can not
	 * set up an appropriate parent, either.
	 *
	 * As a temporary solution, check EXSRC2 bit here before installing
	 * DIV6 clocks.  If it's set to 1, fill in div6 clock parent field
	 * manually, that will prevent sh_clk_init_parent() in sh/clk/cpg.c
	 * from being processed.  If EXSRC2 is not set, SH_CLK_DIV6_EXT()
	 * should work as usual, as if nothing happened.
	 */
	if (__raw_readl(FSIACKCR) & (1 << 12)) /* EXSRC2 */
		div6_clks[DIV6_FSIA].parent = &pll22_clk;
	if (__raw_readl(FSIBCKCR) & (1 << 12)) /* EXSRC2 */
		div6_clks[DIV6_FSIB].parent = &pll22_clk;
	if (__raw_readl(SLIMBCKCR) & (1 << 12)) /* EXSRC2 */
		div6_clks[DIV6_SLIMB].parent = &pll22_clk;

	for (k = 0; !ret && (k < ARRAY_SIZE(main_clks)); k++)
		ret = clk_register(main_clks[k]);

	if (!ret)
		ret = sh_clk_cksel_register(cksel_pll_clks, CKSEL_PLL_NR);

	for (k = 0; !ret && (k < ARRAY_SIZE(pll_clks)); k++)
		ret = clk_register(pll_clks[k]);

	if (!ret)
		ret = sh_clk_div4_register(div4_clks, DIV4_NR, &div4_table);
	if (!ret)
		ret = sh_clk_div6_reparent_register(div6_clks, DIV6_NR);
	if (!ret)
		ret = sh_clk_cksel_register(cksel_clks, CKSEL_NR);
	if (!ret)
		ret = sh_clk_mstp32_register(mstp_clks, MSTP_NR);

/* DDR clock registration */
	sh_clk_cksel_register(&ddr_clk, 1);
	clk_register(&zb30_clk);
	clk_register(&zb30d2_clk);
	clk_register(&ddr_div2_clk);
		clk_register(&ddr_div4_clk);
	sh_clk_cksel_register(&zb30sl_clk, 1);
	sh_clk_cksel_register(&dsi0p0_cksel_clk, 1);
	sh_clk_cksel_register(&dsi0p1_cksel_clk, 1);
	clk_register(&dsi0p0_clk);
	clk_register(&dsi0p1_clk);
	clk_register(&dsi0p_clk);

	sh_clk_cksel_register(&dsi1p0_cksel_clk, 1);
	sh_clk_cksel_register(&dsi1p1_cksel_clk, 1);
	clk_register(&dsi1p0_clk);
	clk_register(&dsi1p1_clk);
	clk_register(&dsi1p_clk);

	for (k = 0; !ret && (k < ARRAY_SIZE(late_main_clks)); k++)
		ret = clk_register(late_main_clks[k]);

	clkdev_add_table(lookups, ARRAY_SIZE(lookups));

	if (!ret)
		shmobile_clk_init();
	else
		panic("failed to setup r8a7373 clocks\n");
};
