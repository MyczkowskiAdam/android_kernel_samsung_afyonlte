/*
 * arch/arm/mach-shmobile/clocksuspend.c
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
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/hwspinlock.h>
#include <linux/io.h>
#include <linux/export.h>
#include <memlog/memlog.h>

#include <mach/r8a7373.h>
#include <mach/pm.h>
#include <mach/sbsc.h>

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) "dvfs[clocksuspend.c<%4d>]:" fmt, __LINE__
#endif

#define PLLCR_STC_MASK		0x3F000000
#define PLLCR_BIT24_SHIFT	24
#define FRQCRD_ZB30SEL		BIT(4)
#define KICK_WAIT_INTERVAL_US	500

/* Modem/APE parameter sharing */
#define SEM_TIMEOUT_US	(10)	/* 10us */
#define SBSC_LOCK_TIMEOUT_US (10000)	/* 10ms */
#define FREQ_LOCK_TIMEOUT_US (10)	/* 10us */
#define CPG_SET_FREQ_MAX_RETRY (500)

/* #define DOWN_PLL1_ENABLE	1 */
static struct {
	unsigned int zs_disabled_cnt;
	unsigned int hp_disabled_cnt;
} the_clock;

/* #define SHM_CLK_TEST_MODE	1 */


/*******************************************************************************
 * PM APIs *********************************************************************
 ******************************************************************************/
struct clk_hw_info {
	unsigned int	mask_bit;
	unsigned int	shift_bit;
	int		div_val[16];
	void __iomem	*addr;
};

/*******************************************************************************
 * PM APIs *********************************************************************
 ******************************************************************************/
static DEFINE_SPINLOCK(freq_change_lock);
static DEFINE_SPINLOCK(zs_lock);
static struct clk_hw_info __clk_hw_info_es2_x[] = {
	[I_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 20,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = -1,
			[DIV1_8] = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = 0x7,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = 0xa,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRA
	},
	[ZG_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 16,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = -1,
			[DIV1_8] = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = -1,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = -1,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRA
	},
	[B_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 8,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = -1,
			[DIV1_8] = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = 0x7,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = 0xa,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRA
	},
	[M1_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 4,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = 0xc,
			[DIV1_8]  = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = 0x7,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = 0xa,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRA
	},
	[M3_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 12,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = 0xc,
			[DIV1_8]  = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = 0x7,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = 0xa,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRA
	},
	[M5_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 0,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = 0xc,
			[DIV1_8]  = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = 0x7,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = 0xa,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRA
	},
	[Z_CLK] = {
		.mask_bit	= 0x1f,
		.shift_bit	= 24,
		.div_val = {
			[DIV1_1] = 0x0,
			[DIV1_2] = 0x10,
			[DIV1_3] = 0x11,
			[DIV1_4] = 0x12,
			[DIV1_5] = -1,
			[DIV1_6] = 0x13,
			[DIV1_7] = -1,
			[DIV1_8] = 0x14,
			[DIV1_12] = 0x15,
			[DIV1_16] = 0x16,
			[DIV1_18] = -1,
			[DIV1_24] = 0x18,
			[DIV1_32] = -1,
			[DIV1_36] = -1,
			[DIV1_48] = 0x1b,
			[DIV1_96] = -1
		},
		.addr = FRQCRB
	},
	[ZTR_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 20,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = -1,
			[DIV1_8] = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = 0x7,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = 0xa,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRB
	},
	[ZT_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 16,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = -1,
			[DIV1_8] = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = 0x7,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = 0xa,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRB
	},
	[ZX_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 12,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = -1,
			[DIV1_8] = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = 0x7,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = 0xa,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRB
	},
	[HP_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 4,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = -1,
			[DIV1_8] = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = 0x7,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = 0xa,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRB
	},
	[ZS_CLK] = {
		.mask_bit	= 0xf,
		.shift_bit	= 8,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = 0x1,
			[DIV1_4] = 0x2,
			[DIV1_5] = -1,
			[DIV1_6] = 0x3,
			[DIV1_7] = -1,
			[DIV1_8] = 0x4,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x6,
			[DIV1_18] = 0x7,
			[DIV1_24] = 0x8,
			[DIV1_32] = -1,
			[DIV1_36] = 0xa,
			[DIV1_48] = 0xb,
			[DIV1_96] = -1
		},
		.addr = FRQCRB
	},
	[ZB_CLK] = { /* 1/2*(setting + 1) ~ 1/2, 1/4, 1/6, 1/8 */
		.mask_bit	= 0x3f,
		.shift_bit	= 0,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = -1,
			[DIV1_4] = 0x1,
			[DIV1_5] = -1,
			[DIV1_6] = 0x2,
			[DIV1_7] = -1,
			[DIV1_8] = 0x3,
			[DIV1_12] = 0x5,
			[DIV1_16] = 0x7,
			[DIV1_18] = 0x8,
			[DIV1_24] = 0xb,
			[DIV1_32] = 0xf,
			[DIV1_36] = 0x11,
			[DIV1_48] = 0x27,
			[DIV1_96] = 0x2f
		},
		.addr = ZBCKCR
	},
	[ZB3_CLK] = {
		.mask_bit	= 0x1f,
		.shift_bit	= 0,
		.div_val = {
			[DIV1_1] = -1,
			[DIV1_2] = 0x0,
			[DIV1_3] = -1,
			[DIV1_4] = 0x10,
			[DIV1_5] = -1,
			[DIV1_6] = 0x11,
			[DIV1_7] = -1,
			[DIV1_8] = 0x12,
			[DIV1_12] = 0x13,
			[DIV1_16] = 0x14,
			[DIV1_18] = -1,
			[DIV1_24] = 0x15,
			[DIV1_32] = 0x16,
			[DIV1_36] = -1,
			[DIV1_48] = 0x18,
			[DIV1_96] = 0x1b
		},
		.addr = FRQCRD
	}
};

#ifndef SHM_CLK_TEST_MODE
static
#endif
struct clk_hw_info *__clk_hw_info;

#ifndef SHM_CLK_TEST_MODE
static
#endif
struct clk_rate __shmobile_freq_modes_es2_x[] = {
	/* ES2.x */
	{
		/* Suspend */
		.i_clk = DIV1_6,
		.zg_clk = DIV1_4,
		.b_clk = DIV1_12,
		.m1_clk = DIV1_6,
		.m3_clk = DIV1_8,
		.m5_clk = DIV1_8,
		.z_clk = DIV1_2,
		.ztr_clk = DIV1_4,
		.zt_clk = DIV1_6,
		.zx_clk = DIV1_3,
		.hp_clk	= DIV1_12,
		.zs_clk = DIV1_12,
		.zb_clk = DIV1_6,
		.zb3_clk = DIV1_4,
		.zb3_freq = 260000,
		.pll0 = PLLx46
	},
	{
		/* Normal, CPU:MAX */
		.i_clk = DIV1_6,
		.zg_clk = DIV1_4,
		.b_clk = DIV1_12,
		.m1_clk = DIV1_6,
		.m3_clk = DIV1_8,
		.m5_clk = DIV1_8,
		.z_clk = DIV1_1,
		.ztr_clk = DIV1_4,
		.zt_clk = DIV1_6,
		.zx_clk = DIV1_3,
		.hp_clk	= DIV1_12,
		.zs_clk = DIV1_6,
		.zb_clk = DIV1_6,
		.zb3_clk = DIV1_2,
		.zb3_freq = 520000,
		.pll0 = PLLx56
	},
	{
		/* Normal, CPU:MID */
		.i_clk = DIV1_6,
		.zg_clk = DIV1_4,
		.b_clk = DIV1_12,
		.m1_clk = DIV1_6,
		.m3_clk = DIV1_8,
		.m5_clk = DIV1_8,
		.z_clk = DIV1_1,
		.ztr_clk = DIV1_4,
		.zt_clk = DIV1_6,
		.zx_clk = DIV1_3,
		.hp_clk	= DIV1_12,
		.zs_clk = DIV1_6,
		.zb_clk = DIV1_6,
		.zb3_clk = DIV1_2,
		.zb3_freq = 520000,
		.pll0 = PLLx56
	},
	{
		/* Normal, CPU:MIN */
		.i_clk = DIV1_6,
		.zg_clk = DIV1_4,
		.b_clk = DIV1_12,
		.m1_clk = DIV1_6,
		.m3_clk = DIV1_8,
		.m5_clk = DIV1_8,
		.z_clk = DIV1_1,
		.ztr_clk = DIV1_4,
		.zt_clk = DIV1_6,
		.zx_clk = DIV1_3,
		.hp_clk	= DIV1_12,
		.zs_clk = DIV1_6,
		.zb_clk = DIV1_6,
		.zb3_clk = DIV1_2,
		.zb3_freq = 520000,
		.pll0 = PLLx56
	},
	{
		/* Earlysuspend, CPU:HIGH */
		.i_clk = DIV1_6,
		.zg_clk = DIV1_4,
		.b_clk = DIV1_12,
		.m1_clk = DIV1_6,
		.m3_clk = DIV1_8,
		.m5_clk = DIV1_8,
		.z_clk = DIV1_1,
		.ztr_clk = DIV1_4,
		.zt_clk = DIV1_6,
		.zx_clk = DIV1_3,
		.hp_clk	= DIV1_12,
		.zs_clk = DIV1_6,
		.zb_clk = DIV1_6,
		.zb3_clk = DIV1_2,
		.zb3_freq = 520000,
		.pll0 = PLLx46
	},
	{
		/* Earlysuspend, CPU:MID */
		.i_clk = DIV1_6,
		.zg_clk = DIV1_4,
		.b_clk = DIV1_12,
		.m1_clk = DIV1_6,
		.m3_clk = DIV1_8,
		.m5_clk = DIV1_8,
		.z_clk = DIV1_2,
		.ztr_clk = DIV1_4,
		.zt_clk = DIV1_6,
		.zx_clk = DIV1_3,
		.hp_clk	= DIV1_12,
		.zs_clk = DIV1_12,
		.zb_clk = DIV1_6,
		.zb3_clk = DIV1_4,
		.zb3_freq = 260000,
		.pll0 = PLLx46
	},
	{
		/* Earlysuspend, CPU:MIN */
		.i_clk = DIV1_6,
		.zg_clk = DIV1_4,
		.b_clk = DIV1_24,
		.m1_clk = DIV1_6,
		.m3_clk = DIV1_8,
		.m5_clk = DIV1_8,
		.z_clk = DIV1_1,
		.ztr_clk = DIV1_4,
		.zt_clk = DIV1_6,
		.zx_clk = DIV1_24,
		.hp_clk	= DIV1_24,
		.zs_clk = DIV1_24,
		.zb_clk = DIV1_6,
		.zb3_clk = DIV1_16,
		.zb3_freq = 97500,
		.pll0 = PLLx46
	},
	{
		/* 720p, CPU:MAX */
		.i_clk = DIV1_6,
		.zg_clk = DIV1_4,
		.b_clk = DIV1_12,
		.m1_clk = DIV1_6,
		.m3_clk = DIV1_8,
		.m5_clk = DIV1_8,
		.z_clk = DIV1_1,
		.ztr_clk = DIV1_4,
		.zt_clk = DIV1_6,
		.zx_clk = DIV1_3,
		.hp_clk	= DIV1_12,
		.zs_clk = DIV1_6,
		.zb_clk = DIV1_6,
		.zb3_clk = DIV1_2,
		.zb3_freq = 520000,
		.pll0 = PLLx56
	},
	{
		/* 720p, CPU:MID */
		.i_clk = DIV1_6,
		.zg_clk = DIV1_4,
		.b_clk = DIV1_12,
		.m1_clk = DIV1_6,
		.m3_clk = DIV1_8,
		.m5_clk = DIV1_8,
		.z_clk = DIV1_1,
		.ztr_clk = DIV1_4,
		.zt_clk = DIV1_6,
		.zx_clk = DIV1_3,
		.hp_clk	= DIV1_12,
		.zs_clk = DIV1_6,
		.zb_clk = DIV1_6,
		.zb3_clk = DIV1_2,
		.zb3_freq = 520000,
		.pll0 = PLLx56
	},
	{
		/* 720p, CPU:MIN */
		.i_clk = DIV1_6,
		.zg_clk = DIV1_4,
		.b_clk = DIV1_12,
		.m1_clk = DIV1_6,
		.m3_clk = DIV1_8,
		.m5_clk = DIV1_8,
		.z_clk = DIV1_1,
		.ztr_clk = DIV1_4,
		.zt_clk = DIV1_6,
		.zx_clk = DIV1_3,
		.hp_clk	= DIV1_12,
		.zs_clk = DIV1_6,
		.zb_clk = DIV1_6,
		.zb3_clk = DIV1_2,
		.zb3_freq = 520000,
		.pll0 = PLLx56
	}
};

/* waveform mapping table */
static struct {
	int mode;
	unsigned int waveform;
} waveform_map[] = {
	{.mode = LLx16_16,	.waveform = 0xffff},
	{.mode = LLx15_16,	.waveform = 0xfffe},
	{.mode = LLx14_16,	.waveform = 0xfefe},
	{.mode = LLx13_16,	.waveform = 0xfeee},
	{.mode = LLx12_16,	.waveform = 0xeeee},
	{.mode = LLx11_16,	.waveform = 0xeeea},
	{.mode = LLx10_16,	.waveform = 0xeaea},
	{.mode = LLx9_16,	.waveform = 0xeaaa},
	{.mode = LLx8_16,	.waveform = 0xaaaa},
	{.mode = LLx7_16,	.waveform = 0xab56},
	{.mode = LLx6_16,	.waveform = 0xb6b6},
	{.mode = LLx5_16,	.waveform = 0xdb6c},
	{.mode = LLx4_16,	.waveform = 0xcccc},
	{.mode = LLx3_16,	.waveform = 0xe738},
	{.mode = LLx2_16,	.waveform = 0xf0f0},
	{.mode = LLx1_16,	.waveform = 0xff00}
};

#ifndef SHM_CLK_TEST_MODE
static
#endif
struct clk_rate *__shmobile_freq_modes;

struct hwspinlock *gen_sem1;
#define DIV_TO_HW(clk, div)	\
	(__clk_hw_info[clk].div_val[div] << __clk_hw_info[clk].shift_bit)

/*SBSC clock (ZB3) table parameter*/
struct sbsc_param zb3_lut[ZB3_FREQ_SIZE] = {
	[ZB3_FREQ_65] = {
		.pll3multiplier_1 = 30,
		.zb3divider_1 = 12,
 		.pll3multiplier_2 = 40,
 		.zb3divider_2 = 16,
		.SDWCRC0A = 0x70884111,
		.SDWCRC1A = 0x08043909,
		.SDWCRC2A = 0x00170503,
		.SDWCR00A = 0x10220202,
		.SDWCR01A = 0x00030109,
		.SDWCR10A = 0x10220202,
		.SDWCR11A = 0x00030109,
		.freq = 65000,
	},
	[ZB3_FREQ_87] = {
		.pll3multiplier_1 = 0,
		.zb3divider_1 = 0,
		.pll3multiplier_2 = 40,
		.zb3divider_2 = 12,
		.SDWCRC0A = 0x70884111,
		.SDWCRC1A = 0x0B05390C,
		.SDWCRC2A = 0x001F0704,
		.SDWCR00A = 0x10320202,
		.SDWCR01A = 0x00050109,
		.SDWCR10A = 0x10320202,
		.SDWCR11A = 0x00050109,
		.freq = 86667,
	},
	[ZB3_FREQ_97] = {
		.pll3multiplier_1 = 30,
		.zb3divider_1 = 8,
		.pll3multiplier_2 = 0,
		.zb3divider_2 = 0,
		.SDWCRC0A = 0x70884111,
		.SDWCRC1A = 0x0C06390D,
		.SDWCRC2A = 0x00230804,
		.SDWCR00A = 0x10420202,
		.SDWCR01A = 0x00050109,
		.SDWCR10A = 0x10420202,
		.SDWCR11A = 0x00050109,
		.freq = 97500,
	},
	[ZB3_FREQ_130] = {
		.pll3multiplier_1 = 30,
		.zb3divider_1 = 6,
		.pll3multiplier_2 = 40,
		.zb3divider_2 = 8,
		.SDWCRC0A = 0x70884111,
		.SDWCRC1A = 0x10083912,
		.SDWCRC2A = 0x002E0B06,
		.SDWCR00A = 0x10520202,
		.SDWCR01A = 0x00070109,
		.SDWCR10A = 0x10520202,
		.SDWCR11A = 0x00070109,
		.freq = 130000,
	},
	[ZB3_FREQ_173] = {
		.pll3multiplier_1 = 0,
		.zb3divider_1 = 0,
		.pll3multiplier_2 = 40,
		.zb3divider_2 = 6,
		/*195 MHz value*/
		.SDWCRC0A = 0x70884111,
		.SDWCRC1A = 0x190C491B,
		.SDWCRC2A = 0x00461109,
		.SDWCR00A = 0x10824303,
		.SDWCR01A = 0x000B0109,
		.SDWCR10A = 0x10824303,
		.SDWCR11A = 0x000B0109,
		.freq = 173333,
	},
	[ZB3_FREQ_260] = {
		.pll3multiplier_1 = 0,
		.zb3divider_1 = 0,
		.pll3multiplier_2 = 40,
		.zb3divider_2 = 4,
		.SDWCRC0A = 0x70884111,
		.SDWCRC1A = 0x21106224,
		.SDWCRC2A = 0x005D170C,
		.SDWCR00A = 0x20A24404,
		.SDWCR01A = 0x000F010A,
		.SDWCR10A = 0x20A24404,
		.SDWCR11A = 0x000F010A,
		.freq = 260000,
	},
	[ZB3_FREQ_520] = {
		.pll3multiplier_1 = 0,
		.zb3divider_1 = 0,
		.pll3multiplier_2 = 40,
		.zb3divider_2 = 2,
		.SDWCRC0A = 0x70A84115,
		.SDWCRC1A = 0x4320CD48,
		.SDWCRC2A = 0x00BB2E19,
		.SDWCR00A = 0x51528909,
		.SDWCR01A = 0x001F030E,
		.SDWCR10A = 0x51528909,
		.SDWCR11A = 0x001F030E,
		.freq = 520000,
	},
};

#define ZB3_HIGHEST_INDEX	ZB3_FREQ_520
#define LPCKCR_MODE0	0
#define LPCKCR_MODE1	1
#define LPCKCR_MODE2	2
#define LPCKCR_MODE3	4

void cpg_set_lpclkcr_mode(u32 mode)
{
	__raw_writel(mode, LPCKCR);
}
/*
 * __match_div_rate: get div-rate by hw setting value
 *
 * Arguments:
 *		@clk: wait time.
 *		@val: hw setting value.
 *
 * Return: divrate
 *
 */
inline int __match_div_rate(int clk, int val)
{
	const enum clk_div div_table[] = {
		DIV1_1, DIV1_2, DIV1_3, DIV1_4, DIV1_5,
		DIV1_6, DIV1_7, DIV1_8, DIV1_12, DIV1_16,
		DIV1_18, DIV1_24, DIV1_32, DIV1_36,
		DIV1_48, DIV1_96
	};
	int i = 0;
	int len = (int)ARRAY_SIZE(div_table);

	for (i = 0; i < len; i++)
		if (__clk_hw_info[clk].div_val[i] == val)
			return div_table[i];
	return -EINVAL;
}

/*
 * __div: convert the divrate value
 *
 * Arguments:
 *		@c_div: input divrate
 *
 * Return: div value
 *
 */
inline int __div(int c_div)
{
	switch (c_div) {
	case DIV1_1:	return 1;
	case DIV1_2:	return 2;
	case DIV1_3:	return 3;
	case DIV1_4:	return 4;
	case DIV1_5:	return 5;
	case DIV1_6:	return 6;
	case DIV1_7:	return 7;
	case DIV1_8:	return 8;
	case DIV1_12:	return 12;
	case DIV1_16:	return 16;
	case DIV1_18:	return 18;
	case DIV1_24:	return 24;
	case DIV1_32:	return 32;
	case DIV1_36:	return 36;
	case DIV1_48:	return 48;
	case DIV1_96:	return 96;
	default:
		pr_err("invalid div-rate<%d>\n", c_div);
	return -EINVAL;
	}
}

/*
 * check_restriction: validate the clock ratio
 * (return fail if violate restriction)
 *
 * Arguments:
 *		@rates: clocks div-rate.
 *
 * Return:
 *		0: normal
 *		-EINVAL: input clocks are violated restriction
 */
inline int check_restriction(const struct clk_rate rates)
{
	const int ratio[8] = {16, 12, 8, 6, 4, 3, 2, 1};
	int result[5] = {0, 0, 0, 0, 0};
	int i_div = 0, zs_div = 0, b_div = 0, hp_div = 0, zx_div = 0;
	int i = 0;

	/* get the div */
	i_div  = __div(rates.i_clk);
	zs_div = __div(rates.zs_clk);
	b_div  = __div(rates.b_clk);
	hp_div = __div(rates.hp_clk);
	zx_div = __div(rates.zx_clk);

	/* check for restriction */
	for (i = 0; i < (int)ARRAY_SIZE(ratio); i++) {
		/* I:ZS = N:1 * */
		if ((i_div * ratio[i]) == zs_div)
			result[0] = 1;
		/* ZS:B = N:1 * */
		if ((zs_div * ratio[i]) == b_div)
			result[1] = 1;
		/* I:HP = N:1 * */
		if ((i_div * ratio[i]) == hp_div)
			result[2] = 1;
		/* ZX:ZS = N:1 * */
		if ((zx_div * ratio[i]) == zs_div)
			result[3] = 1;
		/* ZS:HP = N:1 * (ZS:HP = 1:1 never happened???) */
		if ((zs_div * ratio[i]) == hp_div)
			result[4] = 1;
	}

	/* verify the result */
	for (i = 0; i < (int)ARRAY_SIZE(result); i++) {
		if (!result[i]) {
			pr_err("violate clock restriction, condition<%d>\n", i);
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * cpg_set_pll: set pll ratio
 *
 * Arguments:
 *		@pll: pll id
 *		@val: ratio
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int cpg_set_pll(int pll, unsigned int val)
{
	unsigned int stc_val = 0;
	unsigned int pllcr = 0;
	void __iomem *addr = PLL0CR;
	int timeout = 500;

	if (pll != PLL0) {
			/* support PLL0 only */
		pr_err("invalid parameter, PLL%d not supported\n", pll);
		return -EINVAL;
	}

	pllcr = __raw_readl(addr);
	stc_val = ((pllcr & PLLCR_STC_MASK) >> PLLCR_BIT24_SHIFT) + 1;
	if (val != stc_val) {
		pllcr &= ~PLLCR_STC_MASK;
		pllcr |= ((val - 1) << PLLCR_BIT24_SHIFT);
		__raw_writel(pllcr, addr);
		/* wait for status bit set */
		while (--timeout) {
			if (__raw_readl(PLLECR) & (1 << 8))
				break;
			udelay(1);
		}

		if (timeout <= 0)
			pr_err("wait PLL%d status timeout\n", pll);

		return (timeout <= 0) ? -ETIMEDOUT : 0;
	}

	return 0;
}
/*
 * cpg_get_pll: set pll ratio
 *
 * Arguments:
 *		@pll: pll id
 *
 * Return:
 *		pll0 mult-ratio
 *		negative: operation fail
 */
int cpg_get_pll(int pll)
{
	unsigned int stc_val = 0;
	unsigned int pllcr = 0;
	void __iomem *addr = PLL0CR;

	switch (pll) {
	case PLL0:
		addr = PLL0CR;
		break;
	case PLL1:
		addr = PLL1CR;
		break;
	case PLL2:
		addr = PLL2CR;
		break;
	case PLL3:
		addr = PLL3CR;
		break;
	default:
			return -EINVAL;
		}

	pllcr = __raw_readl(addr);
	stc_val = ((pllcr & PLLCR_STC_MASK) >> PLLCR_BIT24_SHIFT) + 1;

	return (int)stc_val;
}

/*
 * cpg_wait_kick: wait for KICK bit change
 *
 * Arguments:
 *		@time: wait time.
 *
 * Return:
 *		0: successful
 * -EBUSY: unsuccessful
 */
int cpg_wait_kick(unsigned int time)
{
	unsigned int wait_time = time;

	while (0 < wait_time--) {
		if ((__raw_readl(FRQCRB) >> 31) == 0)
			break;
		udelay(1);
	}

	return (wait_time <= 0) ? -EBUSY : 0;
}

/*
 * cpg_set_kick: set and wait for KICK bit cleared
 *
 * Arguments:
 *		@time: wait time.
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int cpg_set_kick(unsigned int time)
{
	unsigned int wait_time = time;

	if ((wait_time <= 0) || (wait_time > KICK_WAIT_INTERVAL_US))
		wait_time = KICK_WAIT_INTERVAL_US;
	__raw_writel(BIT(31) | __raw_readl(FRQCRB), FRQCRB);

	return cpg_wait_kick(wait_time);
}

#define HW_TO_DIV(reg, clk)	((reg >> __clk_hw_info[clk].shift_bit) & \
	__clk_hw_info[clk].mask_bit)
/*
 * cpg_get_freq: get div-rate of set of clock
 *
 * Arguments:
 *		@rates: return clocks div-rate.
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int cpg_get_freq(struct clk_rate *rates)
{
	unsigned int frqcra = __raw_readl(FRQCRA);
	unsigned int frqcrb = __raw_readl(FRQCRB);
	unsigned int frqcrd = __raw_readl(FRQCRD);

	if (!rates)
		return -EINVAL;

	/* get the clock setting
	 * must execute in spin lock context
	 */
	rates->i_clk = __match_div_rate(I_CLK,
		HW_TO_DIV(frqcra, I_CLK));
	rates->zg_clk = __match_div_rate(ZG_CLK,
		HW_TO_DIV(frqcra, ZG_CLK));
	rates->b_clk = __match_div_rate(B_CLK,
		HW_TO_DIV(frqcra, B_CLK));
	rates->m1_clk = __match_div_rate(M1_CLK,
		HW_TO_DIV(frqcra, M1_CLK));
	rates->m3_clk = __match_div_rate(M3_CLK,
		HW_TO_DIV(frqcra, M3_CLK));
	rates->z_clk = __match_div_rate(Z_CLK,
		HW_TO_DIV(frqcrb, Z_CLK));
	rates->ztr_clk = __match_div_rate(ZTR_CLK,
		HW_TO_DIV(frqcrb, ZTR_CLK));
	rates->zt_clk = __match_div_rate(ZT_CLK,
		HW_TO_DIV(frqcrb, ZT_CLK));
	rates->zx_clk = __match_div_rate(ZX_CLK,
		HW_TO_DIV(frqcrb, ZX_CLK));
	rates->hp_clk = __match_div_rate(HP_CLK,
		HW_TO_DIV(frqcrb, HP_CLK));
	rates->zs_clk = __match_div_rate(ZS_CLK,
		HW_TO_DIV(frqcrb, ZS_CLK));
		rates->m5_clk = __match_div_rate(M5_CLK,
			HW_TO_DIV(frqcra, M5_CLK));
	rates->zb_clk = DIV1_6; /* dummy one */

	if ((frqcrd & FRQCRD_ZB30SEL) != 0)
		rates->zb3_clk = __match_div_rate(ZB3_CLK,
			HW_TO_DIV(frqcrd, ZB3_CLK));
	else
		rates->zb3_clk = DIV1_2;

	/* verify again */
	if ((rates->i_clk < 0) || (rates->zg_clk < 0)
		|| (rates->b_clk < 0) || (rates->m1_clk < 0)
		|| (rates->m3_clk < 0) || (rates->z_clk < 0)
		|| (rates->ztr_clk < 0) || (rates->zt_clk < 0)
		|| (rates->zx_clk < 0) || (rates->hp_clk < 0)
		|| (rates->zs_clk < 0) || (rates->zb3_clk < 0)) {
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(cpg_get_freq);

/*
 * cpg_enable_sbsc_change_for_modem:
 *	Permits to change frequency from BBFRQCRD
 *
 * Arguments:
 *		void
 *
 * Return:
 *		void
 */
void cpg_enable_sbsc_change_for_modem(void)
{
	unsigned int ckscr = 0;

	/* unlock ZB30SEL */
	ckscr = __raw_readl(CKSCR);
	__raw_writel(BIT(14) | ckscr, CKSCR);
}

/*
 * cpg_init_bbfrqcrd: Init BBFRQCR to lowest value
 *
 * Arguments:
 *		void
 *
 * Return:
 *		void
 */
void cpg_init_bbfrqcrd(void)
{
	writel(0x1B, BBFRQCRD);
}

/*
 * cpg_sbsc_decide_clock: compute the new clock according to
 *			  modem and ape requests
 *
 * Arguments:
 *		@ape_freq_req: APE request.
 *
 * Return:
 *		void
 */
static unsigned int cpg_sbsc_decide_clock(unsigned int ape_freq_req)
{
	unsigned int system_freq_req = 0;
	unsigned int pll_change = shmobile_get_pll_reprogram();
	unsigned int bb_freq_req = shmobile_get_modem_req_freq();
	int i = 0, ret = 0;

	/* select the bigger requested value */
	system_freq_req = bb_freq_req + ape_freq_req;
	for (i = 0; i < ZB3_FREQ_SIZE; i++) {
		/* find the frequency just on top of requested,
		when modem requested some frequency, forbidden to use
		multiplier 1 only use multiplier 2 to avoid PLL3 change */
		if ((system_freq_req <= zb3_lut[i].freq)
			&& (((pll_change == 0) &&
			(zb3_lut[i].pll3multiplier_2 != 0))
			|| ((pll_change == 1) &&
			((zb3_lut[i].pll3multiplier_2 != 0) || (zb3_lut[i].pll3multiplier_1 != 0))))) {
			ret = i;
			goto exit;
		}
	}
	ret = ZB3_FREQ_SIZE - 1; /* ZB3 = 520Mhz */
exit:
	return ret;

}

/*
 * cpg_PLL3_change: set new value to PLL3
 *
 * Arguments:
 *		@pll_multiplier: PLL3 multiplier in decimal.
 *
 * Return:
 *		void
 */
/*need to be called under semaphore/spinlock*/
static void cpg_PLL3_change(unsigned int pll_multiplier)
{
	u32 work = 0;
	long timeout = 1500;

	/* Check PLL3 status */
	work = __raw_readl(PLLECR);
	if (!(work & PLLECR_PLL3ST))
		return;

	if (pll_multiplier == 40)
		__raw_writel(PLL3CR_1040MHZ, PLL3CR);
	else if (pll_multiplier == 30)
		__raw_writel(PLL3CR_X30, PLL3CR);

	/* Wait PLL3 status on */
	while (0 < timeout) {
		timeout--;
		work = __raw_readl(PLLECR);
		work &= PLLECR_PLL3ST;
		if (work == PLLECR_PLL3ST)
			break;
		udelay(1);
	}

	/* Dummy read */
	shmobile_sbsc_read_reg32(SBSC_SDMRACR1A_OFFSET);
}

/*
 * __cpg_set_sbsc_freq: set SBSC clock(ZB3) frequency without semaphore lock
 *			It is called by other APIs in semaphore lock!
 *
 * Arguments:
 *		@new_ape_freq: new frequency in kHz.
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
static int __cpg_set_sbsc_freq(unsigned int new_ape_freq)
{
	int ret = 0;
#ifdef ZB3_CLK_DFS_ENABLE
	u32 reg_zb3div = 0;
	unsigned int reg = 0;
	unsigned int ckscr = 0;
	unsigned int old_freq = 0;
	unsigned int idx_newfreq = 0;
	unsigned int actual_pllmult = 0;
	u8 zb3divider, pll3multiplier;

	if (shmobile_get_ape_req_freq() != new_ape_freq){
		memory_log_dump_int(PM_DUMP_ID_ZB3DFS_FREQ_REQ, (new_ape_freq / 1000) << 16 | (shmobile_get_modem_req_freq() / 1000));
	}

	/*first determine frequency*/
	idx_newfreq = cpg_sbsc_decide_clock(new_ape_freq);

	if((shmobile_get_pll_reprogram() == 1) && (zb3_lut[idx_newfreq].zb3divider_1 !=0)
		&& (zb3_lut[idx_newfreq].pll3multiplier_1 !=0)) {
		zb3divider = zb3_lut[idx_newfreq].zb3divider_1;
		pll3multiplier = zb3_lut[idx_newfreq].pll3multiplier_1;
	} else {
		zb3divider = zb3_lut[idx_newfreq].zb3divider_2;
		pll3multiplier = zb3_lut[idx_newfreq].pll3multiplier_2;
	}

	if ((zb3divider == 0) || (pll3multiplier == 0)) {
		ret = -EINVAL;
		goto exit;
	}
	switch (zb3divider) {
	case 2:
		reg_zb3div = 0x00;
		break;
	case 4:
		reg_zb3div = 0x10;
		break;
	case 6:
		reg_zb3div = 0x11;
		break;
	case 8:
		reg_zb3div = 0x12;
		break;
	case 12:
		reg_zb3div = 0x13;
		break;
	case 16:
		reg_zb3div = 0x14;
		break;
	case 24:
		reg_zb3div = 0x15;
		break;
	case 32:
		reg_zb3div = 0x16;
		break;
	case 48:
		reg_zb3div = 0x18;
		break;
	case 96:
		reg_zb3div = 0x1B;
		break;
	default:
		reg_zb3div = 0x00;
		ret = -EINVAL;
		goto exit;
	}
	if ((zb3_lut[idx_newfreq].freq > 266000)
		|| (zb3_lut[idx_newfreq].freq < 86000)
		|| (reg_zb3div == 0x00))
		reg_zb3div |= BIT(15);
	else
		reg_zb3div &= ~BIT(15);

	shmobile_set_ape_req_freq(new_ape_freq);

	ret = cpg_wait_kick(KICK_WAIT_INTERVAL_US);
	if (0 > ret)
		goto exit;

	actual_pllmult = cpg_get_pll(PLL3);
	reg = __raw_readl(FRQCRD);
	old_freq = (26 * actual_pllmult);

	switch (reg & (0x1F)) {
	case 0x0:
	case 0x4:
		old_freq /= 2;
		break;
	case 0x10:
		old_freq /= 4;
		break;
	case 0x11:
		old_freq /= 6;
		break;
	case 0x12:
		old_freq /= 8;
		break;
	case 0x13:
		old_freq /= 12;
		break;
	case 0x14:
		old_freq /= 16;
		break;
	case 0x15:
		old_freq /= 24;
		break;
	case 0x16:
		old_freq /= 32;
		break;
	case 0x18:
		old_freq /= 48;
		break;
	case 0x1B:
		old_freq /= 96;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}
	old_freq *= 1000;
	if (old_freq != zb3_lut[idx_newfreq].freq) {
		/*change SBSC param*/

		/*if needs a PLL change, first set SBSC param
		at worst case; because some memory operation can happens
		between PLL change and dividor change*/
		if ((actual_pllmult != pll3multiplier))
			shmobile_sbsc_update_param(&zb3_lut[ZB3_HIGHEST_INDEX]);

		if (actual_pllmult != pll3multiplier)
			cpg_PLL3_change(pll3multiplier);

		/*change SBSC parameters here*/
		if (zb3_lut[idx_newfreq].freq > old_freq)
			shmobile_sbsc_update_param(&zb3_lut[idx_newfreq]);

		if ((reg != reg_zb3div) && ((reg != 0x8004) ||
			(reg_zb3div != 0x8000))) {
			__raw_writel(reg_zb3div, FRQCRD);
			/* set and wait for KICK bit changed */
			if (cpg_set_kick(KICK_WAIT_INTERVAL_US)) {
				ret = -ETIMEDOUT;
				goto exit;
			}

			/*ERRATA*/
			if (zb3divider == 2) {
				ckscr = __raw_readl(CKSCR);
				ckscr &= ~BIT(14);
				__raw_writel(ckscr, CKSCR);

				__raw_writel(BIT(15) | 0x4, FRQCRD);

				if (cpg_set_kick(KICK_WAIT_INTERVAL_US)) {
					ret = -ETIMEDOUT;
					goto exit;
				}
				ckscr = __raw_readl(CKSCR);
				__raw_writel(ckscr | BIT(14), CKSCR);
			}
			/*end of ERRATA*/
		}
		if (zb3_lut[idx_newfreq].freq < old_freq)
			shmobile_sbsc_update_param(&zb3_lut[idx_newfreq]);

		memory_log_dump_int(PM_DUMP_ID_ZB3DFS_FREQ, (pll3multiplier << 26) | ((shmobile_get_pll_reprogram() & 1) << 25) | ((zb3_lut[idx_newfreq].freq / 1000) & 0x0000FFFF));
	}

exit:
#endif /* ZB3_CLK_DFS_ENABLE */
	return ret;

}
/*
 * cpg_set_sbsc_freq: set SBSC clock(ZB3) frequency
 *
 * Arguments:
 *		@new_ape_freq: new frequency in kHz.
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int cpg_set_sbsc_freq(unsigned int new_ape_freq)
{
	int ret = 0;
	unsigned long cpgflags = 0;

	/* limit */
	if (ES_REV_2_2 > shmobile_chip_rev())
		return 0;
	memory_log_dump_int(
		PM_DUMP_ID_SET_SBSC_FREQ_ZB3_LOCK,
		PM_DUMP_START);
	ret = shmobile_acquire_cpg_lock(&cpgflags, SBSC_LOCK_TIMEOUT_US);
	memory_log_dump_int(
		PM_DUMP_ID_SET_SBSC_FREQ_ZB3_LOCK,
		PM_DUMP_END);
	/*we are now in protected area*/
	if (ret) {
		memory_log_dump_int(
			PM_DUMP_ID_SET_SBSC_FREQ_ZB3_LOCK_ERR,
			shmobile_get_lock_cpg_nospin());
		pr_err("%s(): ret<%d>! Can't get HPB semaphore! SMSRC(0x%08x)",
			__func__, ret, shmobile_get_lock_cpg_nospin());
		return ret;
	}
	ret = __cpg_set_sbsc_freq(new_ape_freq);
	shmobile_release_cpg_lock(&cpgflags);
	memory_log_dump_int(
		PM_DUMP_ID_SET_SBSC_FREQ_ZB3_UNLOCK,
		0);
	pr_log("%s() end! ret<%d>", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(cpg_set_sbsc_freq);

/*
 * cpg_get_freqval: get div-rate of special clock
 *
 * Arguments:
 *		@clk: clock.
 *		@div: return div-rate
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int cpg_get_freqval(int clk, int *div)
{
	unsigned long flags;
	int div_rate = 0;

	spin_lock_irqsave(&freq_change_lock, flags);
	if (!div) {
		spin_unlock_irqrestore(&freq_change_lock, flags);
		pr_err("error! invalid parameter<NULL>\n");
		return -EINVAL;
	}

	/* get divrate */
	if (clk == ZB3_CLK) {
		div_rate = HW_TO_DIV(__raw_readl(FRQCRD), clk);
	} else if (clk == ZB_CLK) {
		div_rate = HW_TO_DIV(__raw_readl(ZBCKCR), clk);
	} else if ((clk == ZB_CLK) || (clk == ZG_CLK) || (clk == B_CLK)
		|| (clk == M1_CLK) || (clk == M3_CLK) || (clk == I_CLK)) {
		div_rate = HW_TO_DIV(__raw_readl(FRQCRA), clk);
	} else if ((clk == Z_CLK) || (clk == ZTR_CLK) || (clk == ZT_CLK)
		|| (clk == ZX_CLK) || (clk == HP_CLK) || (clk == ZS_CLK)) {
		div_rate = HW_TO_DIV(__raw_readl(FRQCRB), clk);
	} else if ((shmobile_chip_rev() >= ES_REV_2_0) &&
		(clk == M5_CLK)) {
		div_rate = HW_TO_DIV(__raw_readl(FRQCRA), clk);
	} else {
		spin_unlock_irqrestore(&freq_change_lock, flags);
		pr_err("error! invalid clock<%d>\n", clk);
		return -EINVAL;
	}

	/* ZB3 == x1/2? */
	if ((clk == ZB3_CLK) && ((div_rate & 0x1f) == 0))
		div_rate = DIV1_2;
	else
	div_rate = (int)__match_div_rate((enum clk_type)clk, div_rate);

	/* invalid H/W setting? */
	if (div_rate < 0) {
		spin_unlock_irqrestore(&freq_change_lock, flags);
		pr_err("error! invalid div-rate<%d>\n", div_rate);
		return -EINVAL;
	}

	*div = div_rate;
	spin_unlock_irqrestore(&freq_change_lock, flags);

	return 0;
}

/*
 * cpg_set_freq: set div-rate for set of clock
 *
 * Arguments:
 *		@rates: clocks div-rate.
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int cpg_set_freq(const struct clk_rate rates)
{
	struct clk_rate curr_rates;
	unsigned long cpgflags = 0;
	unsigned long zsflags = 0;
	unsigned long flags = 0;
	unsigned int reg = 0;
	int frq_change = 0;
	int zs_change = 0;
	int ret = 0;
	int i = 0;
	int ret_cpg = 0;
	int smsrc_cpg = 0;
	int ret_sem1 = 0;
	int smsrc_sem1 = 0;

	spin_lock_irqsave(&freq_change_lock, flags);
	/* violate restriction? */
	if (check_restriction(rates)) {
		pr_log("%s()[%d]: warning, violate restriction\n",
			__func__, __LINE__);
	}

	memory_log_dump_int(
		PM_DUMP_ID_SET_CPU_FREQ_RETRY,
		PM_DUMP_START);
	for (i = 0; i < CPG_SET_FREQ_MAX_RETRY; i++) {
		/* get CPG semaphore */
		memory_log_dump_int(
			PM_DUMP_ID_SET_CPU_FREQ_ZB3_LOCK,
			PM_DUMP_START);
		ret_cpg = shmobile_acquire_cpg_lock(&cpgflags,
			FREQ_LOCK_TIMEOUT_US);
		memory_log_dump_int(
			PM_DUMP_ID_SET_CPU_FREQ_ZB3_LOCK,
			PM_DUMP_END);
		if (ret_cpg) {
			smsrc_cpg = shmobile_get_lock_cpg_nospin();
			continue;
		}
		/* get current setting, if nor success try to apply new one */
		cpg_get_freq(&curr_rates);

		/* change FRQCR(A/B) ? */
		if ((curr_rates.i_clk != rates.i_clk)
			|| (curr_rates.zg_clk != rates.zg_clk)
			|| (curr_rates.b_clk != rates.b_clk)
			|| (curr_rates.m1_clk != rates.m1_clk)
			|| (curr_rates.m3_clk != rates.m3_clk)
			|| (curr_rates.m5_clk != rates.m5_clk)
			|| (curr_rates.ztr_clk != rates.ztr_clk)
			|| (curr_rates.zt_clk != rates.zt_clk)
			|| (curr_rates.zx_clk != rates.zx_clk)
			|| (curr_rates.hp_clk != rates.hp_clk)
			|| (curr_rates.zs_clk != rates.zs_clk)) {
			frq_change = 1;
		}

		/* enable HP change? */
		if ((!the_clock.hp_disabled_cnt) &&
			(curr_rates.hp_clk != rates.hp_clk))
			frq_change = 1;

		/* enable ZS change? */
		if ((!the_clock.zs_disabled_cnt) &&
			(curr_rates.zs_clk != rates.zs_clk)) {
			frq_change = 1;
			zs_change = 1;
		}
		/* wait for KICK bit change (if any) */
		ret = cpg_wait_kick(KICK_WAIT_INTERVAL_US);
		if (ret) {
			shmobile_release_cpg_lock(&cpgflags);
			memory_log_dump_int(
			PM_DUMP_ID_SET_CPU_FREQ_ZB3_UNLOCK_1,
			0);
			break;
		}

		/* change FRQCR(A/B) ? */
		if (frq_change) {
			/* not change Z-Phy, use the current one */
			reg = DIV_TO_HW(Z_CLK, curr_rates.z_clk);
			/* apply new setting */
			reg |= DIV_TO_HW(ZTR_CLK, rates.ztr_clk);
			reg |= DIV_TO_HW(ZT_CLK, rates.zt_clk);
			reg |= DIV_TO_HW(ZX_CLK, rates.zx_clk);

			/* change HP */
			if (!the_clock.hp_disabled_cnt)
				reg |= DIV_TO_HW(HP_CLK, rates.hp_clk);
			else /* keep current HP-Phy */
				reg |= DIV_TO_HW(HP_CLK, curr_rates.hp_clk);

			/* change ZS clock */
			if (zs_change) {
				spin_lock_irqsave(&zs_lock, zsflags);
				memory_log_dump_int(
					PM_DUMP_ID_SET_CPU_FREQ_ZS_LOCK,
					PM_DUMP_START);
				ret_sem1 = shmobile_hwspin_lock_timeout_nospin(
					gen_sem1, SEM_TIMEOUT_US);
				memory_log_dump_int(
					PM_DUMP_ID_SET_CPU_FREQ_ZS_LOCK,
					PM_DUMP_END);
				if (ret_sem1) {
					smsrc_sem1 = hwspin_get_lock_id_nospin(
						gen_sem1);
					spin_unlock_irqrestore(&zs_lock,
						zsflags);
					shmobile_release_cpg_lock(&cpgflags);
					memory_log_dump_int(
					PM_DUMP_ID_SET_CPU_FREQ_ZB3_UNLOCK_2,
						0);
					continue;
				}
				reg |= DIV_TO_HW(ZS_CLK, rates.zs_clk);
			} else {
				/* keep current ZS-Phy */
				reg |= DIV_TO_HW(ZS_CLK, curr_rates.zs_clk);
			}

			/* apply setting */
			__raw_writel(reg, FRQCRB);

			reg = DIV_TO_HW(I_CLK, rates.i_clk);
			reg |= DIV_TO_HW(ZG_CLK, rates.zg_clk);
			reg |= DIV_TO_HW(B_CLK, rates.b_clk);
			reg |= DIV_TO_HW(M1_CLK, rates.m1_clk);
			reg |= DIV_TO_HW(M3_CLK, rates.m3_clk);
			reg |= DIV_TO_HW(M5_CLK, rates.m5_clk);

			/* apply setting */
			__raw_writel(reg, FRQCRA);

			/* set and wait for KICK bit changed */
			ret = cpg_set_kick(KICK_WAIT_INTERVAL_US);
			if (ret) {
				if (zs_change) {
					hwspin_unlock_nospin(gen_sem1);
					memory_log_dump_int(
					PM_DUMP_ID_SET_CPU_FREQ_ZS_UNLOCK_1,
						0);
					spin_unlock_irqrestore(&zs_lock,
						zsflags);
				}

				shmobile_release_cpg_lock(&cpgflags);
				memory_log_dump_int(
				PM_DUMP_ID_SET_CPU_FREQ_ZB3_UNLOCK_3,
				0);
				break;
			}

			if (zs_change) {
				hwspin_unlock_nospin(gen_sem1);
				memory_log_dump_int(
					PM_DUMP_ID_SET_CPU_FREQ_ZS_UNLOCK_2,
					0);
				spin_unlock_irqrestore(&zs_lock, zsflags);
			}
		}
		/* change ZB3 clock ? */
		ret = __cpg_set_sbsc_freq(rates.zb3_freq);

		shmobile_release_cpg_lock(&cpgflags);
		memory_log_dump_int(
			PM_DUMP_ID_SET_CPU_FREQ_ZB3_UNLOCK_4,
			0);
		break;
	}

	memory_log_dump_int(
		PM_DUMP_ID_SET_CPU_FREQ_RETRY,
		PM_DUMP_END);

	spin_unlock_irqrestore(&freq_change_lock, flags);
	if ((ret_cpg < 0) && (smsrc_cpg != 0)) {
		memory_log_dump_int(
			PM_DUMP_ID_SET_CPU_FREQ_ZB3_LOCK_ERR,
			smsrc_cpg);
		pr_err("%s(): can't lock hwlock SMSRC(0x%08x) ret(%d)\n",
			 __func__, smsrc_cpg, ret_cpg);
	}
	if ((ret_sem1 < 0) && (smsrc_sem1 != 0)) {
		memory_log_dump_int(
			PM_DUMP_ID_SET_CPU_FREQ_ZS_LOCK_ERR,
			smsrc_sem1);
		pr_err("%s(): can't lock hwlock SMSRC(0x%08x) ret(%d)\n",
			 __func__, smsrc_sem1, ret_sem1);
	}
	if (ret != 0)
		pr_err("%s() error! ret<%d>", __func__, ret);

	ret |= ret_cpg | ret_sem1;

	pr_log("%s() end! ret<%d>", __func__, ret);
	return ret;
}

/*
 * cpg_set_zdiv: change waveform controlling Z-div clock
 *
 * Arguments:
 *		@mode: waveform level
 *
 * Return:
 *		0: operation success
 *		negative: operation fail
 */
static int cpg_set_zdiv(int mode)
{
	unsigned int zdivcr = 0xFFF00000;
	int i = 0;
	int ret = -EINVAL;

	/* Validate LSI revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	/* Validate waveform mode
	 * -> support LLx1_16 to LLx16_16
	 */
	if ((mode < LLx1_16) || (mode > LLx16_16)) {
		pr_err("invalid div mode<%d>\n", mode);
		return ret;
	}

	/* Change waveform setting
	 * -> setting indecates by 'mode' and define by 'waveform_map'
	 *    need to search
	 */
	for (i = 0; i < ARRAY_SIZE(waveform_map); i++) {
		if (waveform_map[i].mode == mode) {
			/* BIT(18) should be set when 'mode' from 1 -> 8
			 * else, pls clear
			 */
			if (mode <= 8)
				zdivcr |= BIT(18);
			else
				zdivcr &= ~BIT(18);

			zdivcr |= waveform_map[i].waveform;
			__raw_writel(zdivcr, SCGCR);

			/* BIT(29) should be clear when all done
			 */
			zdivcr &= ~BIT(29);
			__raw_writel(zdivcr, SCGCR);

			pr_log("ZDIVCR[0x%08x]\n", __raw_readl(SCGCR));
			return 0;
		}
	}

	return ret;
}
/******************************************************************************/
/* PM APIs ********************************************************************/
/******************************************************************************/
int pm_setup_clock(void)
{
	unsigned int zbckcr = 0;
	unsigned int cnt_zb = 1000;/*Wait 1ms*/

	if (shmobile_chip_rev() >= ES_REV_2_0) {
		__shmobile_freq_modes = __shmobile_freq_modes_es2_x;
		__clk_hw_info = __clk_hw_info_es2_x;
	}

	shmobile_sbsc_init();
	the_clock.zs_disabled_cnt = 0;
	the_clock.hp_disabled_cnt = 0;

	gen_sem1 = hwspin_lock_request_specific(SMGP100_DFS_ZS);
	if (!gen_sem1) {
		pr_err("error! fail to get semaphore<SMGP100_DFS_ZS>\n");
		the_clock.zs_disabled_cnt = ~0;
	}
	/* stop ZB-Phy */
	while (cnt_zb--) {
		zbckcr = __raw_readl(ZBCKCR);
		if (zbckcr & BIT(8)) {
			pr_info("ZB-Phy stopped, ZBCKCR<0x%08x>\n", zbckcr);
			break;
		}
		zbckcr |= BIT(8);
		__raw_writel(zbckcr, ZBCKCR);
		udelay(1);
	}
	if (!cnt_zb)
		pr_err("fail to stop ZB-Phy, ZBCKCR<0x%08x>\n", zbckcr);


	return 0;
}

/*
 * pm_set_clocks: set frequencies value
 *
 * Arguments:
 *		@rates: frequencies.
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int pm_set_clocks(const struct clk_rate rates)
{
	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (validate())
		return cpg_set_freq(rates);

	return 0;
}
EXPORT_SYMBOL(pm_set_clocks);

/*
 * pm_set_clock_mode: set frequencies value
 *
 * Arguments:
 *		@mode: frequency mode.
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int pm_set_clock_mode(int mode)
{
	struct clk_rate rates;
	int z_rate = 0;
	int size = 0;
	int ret = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	if (shmobile_chip_rev() >= ES_REV_2_0)
		size = (int)ARRAY_SIZE(__shmobile_freq_modes_es2_x);

	if ((mode >= size) || (mode < 0)) {
		pr_err("invalid clock mode<%d>\n", mode);
		return -EINVAL;
	}

	(void)memcpy(&rates, &__shmobile_freq_modes[mode],
		sizeof(struct clk_rate));

	/* not allow to change Z-Phy using this APIs */
	ret = cpg_get_freqval(Z_CLK, &z_rate);
	if (!ret) {
		rates.z_clk = z_rate;
		ret = cpg_set_freq(rates);
	}

	return ret;
}
EXPORT_SYMBOL(pm_set_clock_mode);

/*
 * pm_get_clock_mode: get frequencies value
 *
 * Arguments:
 *		@mode: frequency mode.
 *		@rate: frequencies value
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int pm_get_clock_mode(int mode, struct clk_rate *rate)
{
	int size = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return 0;

	if (shmobile_chip_rev() >= ES_REV_2_0)
		size = (int)ARRAY_SIZE(__shmobile_freq_modes_es2_x);

	/* invalid parameter? */
	if ((!rate) || (mode >= size) || (mode < 0)) {
		pr_err("invalid clock mode<%d>\n", mode);
		return -EINVAL;
	}

	(void)memcpy(rate, &__shmobile_freq_modes[mode],
		sizeof(struct clk_rate));

	return 0;
}
EXPORT_SYMBOL(pm_get_clock_mode);

/*
 * pm_set_syscpu_frequency: change SYS-CPU frequency
 *
 * Arguments:
 *		div-rate.
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int pm_set_syscpu_frequency(int div)
{
	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (validate())
		return cpg_set_zdiv(div);

	return 0;
}
EXPORT_SYMBOL(pm_set_syscpu_frequency);

/*
 * pm_get_syscpu_frequency: get SYS-CPU frequency
 *
 * Arguments:
 *		none
 *
 * Return:
 *    0: unsuccessful
 */
unsigned int pm_get_syscpu_frequency(void)
{
	int ret = 0;
	int div = 0;
	int pll0 = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return 0;

#ifdef ZFREQ_MODE
	pr_alert("should not use\n");
#endif /* ZFREQ_MODE */
	ret = cpg_get_freqval(Z_CLK, &div);
	if (!ret) {
		pll0 = cpg_get_pll(PLL0);
		if (pll0 > 0) {
			switch (div) {
			case DIV1_1: return (pll0 * 26000);
			case DIV1_2: return (pll0 * 13000);
			case DIV1_4: return (pll0 *  6500);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(pm_get_syscpu_frequency);

/*
 * pm_disable_clock_change: disable ZS/HP clock
 *
 * Arguments:
 *		@clk: clock
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int pm_disable_clock_change(int clk)
{
	unsigned long flags;
	int ret = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	spin_lock_irqsave(&freq_change_lock, flags);
	/* support ZS/HP only */
	if (!(clk & (ZSCLK | HPCLK))) {
		spin_unlock_irqrestore(&freq_change_lock, flags);
		pr_err("invalid parameter\n");
		return -EINVAL;
	}

	/* ZS clock is requested? */
	if ((clk & ZSCLK))
		the_clock.zs_disabled_cnt++;

	/* HP clock is requested? */
	if ((clk & HPCLK))
		the_clock.hp_disabled_cnt++;

	spin_unlock_irqrestore(&freq_change_lock, flags);

	return ret;
}
EXPORT_SYMBOL(pm_disable_clock_change);

/*
 * pm_enable_clock_change: enable ZS/HP clock
 *
 * Arguments:
 *		@clk: clock
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int pm_enable_clock_change(int clk)
{
	unsigned long flags;
	int ret = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (!validate())
		return ret;

	spin_lock_irqsave(&freq_change_lock, flags);

	/* validate parameter, support ZS/HP only */
	if (!(clk & (ZSCLK | HPCLK))) {
		spin_unlock_irqrestore(&freq_change_lock, flags);
		return -EINVAL;
	}

	/* ZS clock is enabled? */
	if ((clk & ZSCLK)) {
		if (the_clock.zs_disabled_cnt)
			the_clock.zs_disabled_cnt--;
	}

	/* HP clock is enabled? */
	if ((clk & HPCLK)) {
		if (the_clock.hp_disabled_cnt)
			the_clock.hp_disabled_cnt--;
	}

	spin_unlock_irqrestore(&freq_change_lock, flags);

	return ret;
}
EXPORT_SYMBOL(pm_enable_clock_change);

/*
 * pm_set_pll_ratio: change pll ratio
 *
 * Arguments:
 *		@pll: pll type
 *		@val: set value
 *
 * Return:
 *		0: successful
 *		negative: operation fail
 */
int pm_set_pll_ratio(int pll, unsigned int val)
{
	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (validate())
		return cpg_set_pll(pll, val);

	return 0;
}
EXPORT_SYMBOL(pm_set_pll_ratio);

/*
 * pm_get_pll_ratio: get pll ratio
 *
 * Arguments:
 *		@pll: pll type
 *
 * Return:
 *		pll0 mult-ratio
 *		negative: operation fail
 */
int pm_get_pll_ratio(int pll)
{
	return cpg_get_pll(pll);
}
EXPORT_SYMBOL(pm_get_pll_ratio);

/*
 * pm_spin_lock_irqrestore: spinlock(zs lock)
 *
 * Arguments:
 *		@flag: irq flag
 *
 * Return:
 *		None
 */
unsigned long pm_get_spinlock(void)
{
	unsigned long flag = 0;

	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (validate())
		spin_lock_irqsave(&zs_lock, flag);

	return flag;
}
EXPORT_SYMBOL(pm_get_spinlock);

/*
 * pm_spin_unlock_irqrestore: spinunlock(zs lock)
 *
 * Arguments:
 *		@flag: irq flag
 *
 * Return:
 *		None
 */
void pm_release_spinlock(unsigned long flag)
{
	/* validate chip revision
	 * -> support revision 2.x and later
	 */
	if (validate())
		spin_unlock_irqrestore(&zs_lock, flag);
}
EXPORT_SYMBOL(pm_release_spinlock);

/**** DebugFS Tuning****/
#if defined(CONFIG_DEBUG_FS)

#include <linux/debugfs.h>

static struct dentry *clk_debugfs_root;

struct dentry *array_dent[ARRAY_SIZE(__shmobile_freq_modes_es2_x)];

static int clk_debugfs_register_one(unsigned int index)
{
	int err;
	struct dentry *d;
	char s[255];
	char *p = s;

	p += sprintf(p, "opp-%d", index);
	d = debugfs_create_dir(s, clk_debugfs_root);
	if (!d)
		return -ENOMEM;
	array_dent[index] = d;

	d = debugfs_create_u8("i_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].i_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("zg_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].zg_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("b_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].b_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("m1_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].m1_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("m3_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].m3_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("m5_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].m5_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("z_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].z_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("ztr_clk", (S_IRUSR | S_IWUSR | S_IRGRP |
			S_IROTH), array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].ztr_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("zt_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].zt_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("zx_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].zx_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("hp_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].hp_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("zs_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].zs_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("zb_clk", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].zb_clk);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u32("zb3_freq", (S_IRUSR | S_IWUSR | S_IRGRP |
			S_IROTH), array_dent[index],
			(u32 *)&__shmobile_freq_modes_es2_x[index].zb3_freq);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	d = debugfs_create_u8("pll0", (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
			array_dent[index],
			(u8 *)&__shmobile_freq_modes_es2_x[index].pll0);
	if (!d) {
		err = -ENOMEM;
		goto err_out;
	}

	return 0;

err_out:
	d = array_dent[index];
	debugfs_remove_recursive(array_dent[index]);
	return err;
}

static int __init clk_debugfs_init(void)
{
	struct dentry *d;
	int err;
	unsigned int i;

	d = debugfs_create_dir("clk_dfs", NULL);
	if (!d)
		return -ENOMEM;

	clk_debugfs_root = d;

	for (i = 0; i < ARRAY_SIZE(__shmobile_freq_modes_es2_x); i++) {
		err = clk_debugfs_register_one(i);
		if (err)
			goto err_out;
	}

	return 0;
err_out:
	debugfs_remove_recursive(clk_debugfs_root);
	return err;
}
late_initcall(clk_debugfs_init);

#endif
