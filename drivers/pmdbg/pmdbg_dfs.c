/*
 * pmdbg_dfs.c
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

#include "pmdbg_dfs.h"
#include <mach/r8a7373.h>
#ifndef IOMEM
#define IOMEM __io
#endif /*IOMEM*/

static char monitor_buf[NO_MON_ITEM][64] = { {0} };
static int curr_idx;
static char buf_reg[1024];
static int mon_state;

static struct clk_hw_info *__clk_hw_info;

static struct clk_hw_info __clk_hw_info_es1_x[] = {
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRB)
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
		.addr = IOMEM(FRQCRB)
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
		.addr = IOMEM(FRQCRB)
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
		.addr = IOMEM(FRQCRB)
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
		.addr = IOMEM(FRQCRB)
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
		.addr = IOMEM(FRQCRB)
	},
	[ZB_CLK] = { /* 1/2*(setting + 1) ~ 1/2, 1/4, 1/6, 1/8 */
		.mask_bit	= 0x1bf,
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
		.addr = IOMEM(ZBCKCR)
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
		.addr = IOMEM(FRQCRD)
	}
};
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRA)
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
		.addr = IOMEM(FRQCRB)
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
		.addr = IOMEM(FRQCRB)
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
		.addr = IOMEM(FRQCRB)
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
		.addr = IOMEM(FRQCRB)
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
		.addr = IOMEM(FRQCRB)
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
		.addr = IOMEM(FRQCRB)
	},
	[ZB_CLK] = { /* 1/2*(setting + 1) ~ 1/2, 1/4, 1/6, 1/8 */
		.mask_bit	= 0x1bf,
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
		.addr = IOMEM(ZBCKCR)
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
		.addr = IOMEM(FRQCRD)
	}
};

LOCAL_DECLARE_MOD_SHOW(dfs, dfs_init, dfs_exit, dfs_show);

DECLARE_CMD(start, start_dfs_cmd);
DECLARE_CMD(stop, stop_dfs_cmd);
DECLARE_CMD(en, enable_dfs_cmd);
DECLARE_CMD(dis, disable_dfs_cmd);
DECLARE_CMD(sup, suppress_cmd);
DECLARE_CMD(clk_get, clk_get_cmd);
DECLARE_CMD(mon, monitor_cmd);


struct notifier_block notifier = {
	.notifier_call = &transition_notifier_cb
};

#ifndef CONFIG_CPU_FREQ


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
static int cpg_get_pll(int pll)
{
	unsigned int stc_val = 0;
	unsigned int pllcr = 0;
	unsigned int addr = PLL0CR;

	switch (pll) {
	case PLL0:
		addr = PLL0CR;
		break;
	case PLL1:
		addr = PLL1CR;
		break;
	case PLL2:
		addr = CPG_PLL2CR;
		break;
	case PLL3:
		addr = PLL3CR;
		break;
	default:
		return -EINVAL;
	}

	pllcr = __raw_readl(IOMEM(IO_ADDRESS(addr)));
	stc_val = ((pllcr & PLLCR_STC_MASK) >> PLLCR_BIT24_SHIFT) + 1;
	return (int)stc_val;
}


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
static int cpg_get_freqval(int clk, int *div)
{
	int div_rate = 0;

	if (!div)
		return -EINVAL;
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

		return -EINVAL;
	}

	/* ZB3 == x1/2? */
	if ((clk == ZB3_CLK) && ((div_rate & 0x1f) == 0))
		div_rate = DIV1_2;
	else
		div_rate =
		(int)__match_div_rate((enum clk_type)clk, div_rate);
	if (div_rate < 0)
		return -EINVAL;

	*div = div_rate;

	return 0;
}

static unsigned int pm_get_syscpu_frequency(void)
{
	int ret = 0;
	int div = 0;
	int pll0 = 0;

	ret = cpg_get_freqval(Z_CLK, &div);
	if (!ret) {
		pll0 = cpg_get_pll(PLL0);
		if (pll0 > 0) {
			switch (div) {
			case DIV1_1: return (pll0 * 26000);
			case DIV1_2: return (pll0 * 13000);
			case DIV1_4: return (pll0 * 6500);
			}
		}
	}
	/* error case */
	return 0;
}
#endif /*CONFIG_CPU_FREQ*/

static inline int __match_div_rate(int clk, int val)
{
	const enum clk_div div_table[] = {
		DIV1_1, DIV1_2, DIV1_3, DIV1_4, DIV1_5,
		DIV1_6, DIV1_7, DIV1_8, DIV1_12, DIV1_16,
		DIV1_18, DIV1_24, DIV1_32, DIV1_36,
		DIV1_48, DIV1_96
	};
	int i = 0;
	int len = (int)ARRAY_SIZE(div_table);

	for (i = 0; i < len; i++) {
		if (__clk_hw_info[clk].div_val[i] == val)
			return div_table[i];
	}

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
static inline int __div(enum clk_div c_div)
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
		return -EINVAL;
	}
}


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
 #if 0
static int cpg_get_freq(struct clk_rate *rates)
{
	struct clk_rate get_rate;
	unsigned int frqcra = __raw_readl(FRQCRA);
	unsigned int frqcrb = __raw_readl(FRQCRB);
	unsigned int frqcrd = __raw_readl(FRQCRD);
	unsigned int zbckcr = __raw_readl(ZBCKCR);

	if (!rates)
		return -EINVAL;

	/* get the clock setting
	 * must execute in spin lock context
	 */
	get_rate.i_clk = __match_div_rate(I_CLK,
		HW_TO_DIV(frqcra, I_CLK));
	get_rate.zg_clk = __match_div_rate(ZG_CLK,
		HW_TO_DIV(frqcra, ZG_CLK));
	get_rate.b_clk = __match_div_rate(B_CLK,
		HW_TO_DIV(frqcra, B_CLK));
	get_rate.m1_clk = __match_div_rate(M1_CLK,
		HW_TO_DIV(frqcra, M1_CLK));
	get_rate.m3_clk = __match_div_rate(M3_CLK,
		HW_TO_DIV(frqcra, M3_CLK));
	get_rate.z_clk = __match_div_rate(Z_CLK,
		HW_TO_DIV(frqcrb, Z_CLK));
	get_rate.ztr_clk = __match_div_rate(ZTR_CLK,
		HW_TO_DIV(frqcrb, ZTR_CLK));
	get_rate.zt_clk = __match_div_rate(ZT_CLK,
		HW_TO_DIV(frqcrb, ZT_CLK));
	get_rate.zx_clk = __match_div_rate(ZX_CLK,
		HW_TO_DIV(frqcrb, ZX_CLK));
	get_rate.hp_clk = __match_div_rate(HP_CLK,
		HW_TO_DIV(frqcrb, HP_CLK));
	get_rate.zs_clk = __match_div_rate(ZS_CLK,
		HW_TO_DIV(frqcrb, ZS_CLK));
	get_rate.zb_clk = __match_div_rate(ZB_CLK,
		HW_TO_DIV(zbckcr, ZB_CLK));

	if (shmobile_chip_rev() >= ES_REV_2_0)
		get_rate.m5_clk = __match_div_rate(M5_CLK,
			HW_TO_DIV(frqcra, M5_CLK));
	else
		get_rate.m5_clk = DIV1_1; /* dummy one */

	if ((frqcrd & FRQCRD_ZB30SEL) != 0)
		get_rate.zb3_clk = __match_div_rate(ZB3_CLK,
			HW_TO_DIV(frqcrd, ZB3_CLK));
	else
		get_rate.zb3_clk = DIV1_2;
	/* verify again */
	if ((get_rate.i_clk < 0) || (get_rate.zg_clk < 0)
		|| (get_rate.b_clk < 0) || (get_rate.m1_clk < 0)
		|| (get_rate.m3_clk < 0) || (get_rate.z_clk < 0)
		|| (get_rate.ztr_clk < 0) || (get_rate.zt_clk < 0)
		|| (get_rate.zx_clk < 0) || (get_rate.hp_clk < 0)
		|| (get_rate.zs_clk < 0) || (get_rate.zb_clk < 0)
		|| (get_rate.zb3_clk < 0)) {
		return -EINVAL;
	}

	if (shmobile_chip_rev() >= ES_REV_2_0) {
		if (get_rate.m5_clk < 0)
			return -EINVAL;
		get_rate.pll0 = pm_get_pll_ratio(PLL0);
	} else
		get_rate.pll0 = PLLx38;

	(void)memcpy(rates, &get_rate, sizeof(struct clk_rate));

	return 0;
}
#endif
static int start_dfs_cmd(char *para, int size)
{
	struct timeval time;
	int freq = 0;
	char *s = buf_reg;
	(void)para;
	(void)size;
	FUNC_MSG_IN;
#ifdef CONFIG_SHMOBILE_CPUFREQ
	start_cpufreq();
	jiffies_to_timeval(jiffies, &time);
	s += sprintf(s, "DFS: Started at: %u s, %u us\n",
		(u32)time.tv_sec, (u32)time.tv_usec);
#else /*!CONFIG_SHMOBILE_CPUFREQ*/
	(void)time;
	s += sprintf(s,
		"DFS: Not support (macro CONFIG_SHMOBILE_CPUFREQ enable?)\n");
#endif /*CONFIG_SHMOBILE_CPUFREQ*/
	freq = pm_get_syscpu_frequency();
	s += sprintf(s, "DFS: CPU Freq: %u KHz\n", freq);
	MSG_INFO("%s", buf_reg);
	FUNC_MSG_RET(0);
}

static int stop_dfs_cmd(char *para, int size)
{
	struct timeval time;
	int freq = 0;
	char *s = buf_reg;
	(void)para;
	(void)size;
	FUNC_MSG_IN;
#ifdef CONFIG_SHMOBILE_CPUFREQ
	(void)stop_cpufreq();
	jiffies_to_timeval(jiffies, &time);
	s += sprintf(s, "DFS: Stopped at: %u s, %u us\n",
		(u32)time.tv_sec, (u32)time.tv_usec);
#else /*!CONFIG_SHMOBILE_CPUFREQ*/
	(void)time;
	s += sprintf(s,
		"DFS: Not support (macro CONFIG_SHMOBILE_CPUFREQ enable?)\n");
#endif /*CONFIG_SHMOBILE_CPUFREQ*/
	freq = pm_get_syscpu_frequency();
	s += sprintf(s, "DFS: CPU Freq: %u KHz\n", freq);
	MSG_INFO("%s", buf_reg);
	FUNC_MSG_RET(0);
}

static int enable_dfs_cmd(char *para, int size)
{
	struct timeval time;
	int freq = 0;
	char *s = buf_reg;
	(void)para;
	(void)size;
	FUNC_MSG_IN;
#ifdef CONFIG_SHMOBILE_CPUFREQ
	(void)control_dfs_scaling(true);
	jiffies_to_timeval(jiffies, &time);
	s += sprintf(s, "DFS: Enable at: %u s, %u us\n",
		(u32)time.tv_sec, (u32)time.tv_usec);
#else /*!CONFIG_SHMOBILE_CPUFREQ*/
	(void)time;
	s += sprintf(s,
		"DFS: Not support (macro CONFIG_SHMOBILE_CPUFREQ enable?)\n");
#endif /*CONFIG_SHMOBILE_CPUFREQ*/
	freq = pm_get_syscpu_frequency();
	s += sprintf(s, "DFS: CPU Freq: %u KHz", freq);
	MSG_INFO("%s", buf_reg);
	FUNC_MSG_RET(0);
}

static int disable_dfs_cmd(char *para, int size)
{
	struct timeval time;
	int freq = 0;
	char *s = buf_reg;
	(void)para;
	(void)size;
	FUNC_MSG_IN;
#ifdef CONFIG_SHMOBILE_CPUFREQ
	(void)control_dfs_scaling(false);
	jiffies_to_timeval(jiffies, &time);
	s += sprintf(s, "DFS: Disable at: %u s, %u ms\n",
		(u32)time.tv_sec, (u32)time.tv_usec);
#else /*!CONFIG_SHMOBILE_CPUFREQ*/
	(void)time;
	s += sprintf(s,
		"DFS: Not support (macro CONFIG_SHMOBILE_CPUFREQ enable?)\n");
#endif /*CONFIG_SHMOBILE_CPUFREQ*/
	freq = pm_get_syscpu_frequency();
	s += sprintf(s, "DFS: CPU Freq: %u KHz\n", freq);
	MSG_INFO("%s", buf_reg);
	FUNC_MSG_RET(0);
}

static int suppress_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	int pos = 0;
	int freq = 0;
	char *s = buf_reg;
	FUNC_MSG_IN;
#ifndef CONFIG_SHMOBILE_CPUFREQ
	s += sprintf(s,
		"DFS: Not support \(macro CONFIG_SHMOBILE_CPUFREQ enable?)\n");
	ret =  -ENOTSUPP;
	goto end;
#endif /*CONFIG_SHMOBILE_CPUFREQ*/

	para = strim(para);
	ret = get_word(para, size, 0, item, &para_sz);
	if (ret <= 0) {
		ret =  -EINVAL;
		goto fail;
	}

	pos = ret;
	ret = strncmp(item, "min", sizeof("min"));
	if (0 == ret) {
		disable_dfs_mode_min();
		s += sprintf(s, "DFS: Disable MIN level\n");
		goto end;
	}

	ret = strncmp(item, "no", sizeof("no"));
	if (0 == ret) {
		enable_dfs_mode_min();
		s += sprintf(s, "DFS: Allow all levels\n");
		goto end;
	}
	ret =  -EINVAL;
fail:
	s += sprintf(s, "FAILED");
end:
	freq = pm_get_syscpu_frequency();
	s += sprintf(s, "DFS: CPU Freq: %u KHz\n", freq);
	MSG_INFO("%s", buf_reg);
	FUNC_MSG_RET(ret);
}


static int clk_get_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	int pos = 0;
	char *s = buf_reg;
	struct clk_rate curr_rates;

	FUNC_MSG_IN;

	memset(&curr_rates, 0, sizeof(struct clk_rate));
	para = strim(para);

	ret = get_word(para, size, 0, item, &para_sz);
	if (ret <= 0) {
		ret = -EINVAL;
		goto fail;
	}

	pos = ret;
	ret = strncmp(item, "all", sizeof("all"));
	if (0 == ret) {
		ret = cpg_get_freq(&curr_rates);
		if (ret < 0)
			goto fail;
		s += sprintf(s, "I_CLK: 1:%d\n",
			__div(curr_rates.i_clk));
		s += sprintf(s, "ZG_CLK: 1:%d\n",
			__div(curr_rates.zg_clk));
		s += sprintf(s, "B_CLK: 1:%d\n",
			__div(curr_rates.b_clk));
		s += sprintf(s, "M1_CLK: 1:%d\n",
			__div(curr_rates.m1_clk));
		s += sprintf(s, "M3_CLK: 1:%d\n",
			__div(curr_rates.m3_clk));
		if (shmobile_chip_rev() >= ES_REV_2_0) {
			s += sprintf(s, "M5_CLK: 1:%d\n",
				__div(curr_rates.m5_clk));
		}
		s += sprintf(s, "Z_CLK: 1:%d\n",
			__div(curr_rates.z_clk));
		s += sprintf(s, "ZTR_CLK: 1:%d\n",
			__div(curr_rates.ztr_clk));
		s += sprintf(s, "ZT_CLK: 1:%d\n",
			__div(curr_rates.zt_clk));
		s += sprintf(s, "ZX_CLK: 1:%d\n",
			__div(curr_rates.zx_clk));
		s += sprintf(s, "HP_CLK: 1:%d\n",
			__div(curr_rates.hp_clk));
		s += sprintf(s, "ZS_CLK: 1:%d\n",
			__div(curr_rates.zs_clk));
		s += sprintf(s, "ZB_CLK: 1:%d\n",
			__div(curr_rates.zb_clk));
		s += sprintf(s, "ZB3_CLK: 1:%d\n",
			__div(curr_rates.zb3_clk));

		goto end;
	}

	ret = strncmp(item, "cpu", sizeof("cpu"));
	if (0 == ret) {
		ret = pm_get_syscpu_frequency();
		s += sprintf(s, "SYS-CPU Freq: %d KHz\n", ret);
		ret = 0;
		goto end;
	}

	ret =  -EINVAL;
fail:
	s += sprintf(s, "FAILED");
end:
	MSG_INFO("%s", buf_reg);
	FUNC_MSG_RET(ret);
}

static int start_monitor(void)
{
	int ret = 0;
	FUNC_MSG_IN;
	ret = cpufreq_register_notifier(&notifier,
			CPUFREQ_TRANSITION_NOTIFIER);
	if (!ret)
		mon_state = 1;
	FUNC_MSG_RET(ret);
}

static int stop_monitor(void)
{
	int ret = 0;
	FUNC_MSG_IN;
	ret = cpufreq_unregister_notifier(&notifier,
			CPUFREQ_TRANSITION_NOTIFIER);
	if (!ret)
		mon_state = 0;
	FUNC_MSG_RET(ret);
}

static int transition_notifier_cb(struct notifier_block *nb,
			unsigned long phase, void *data)
{
	struct cpufreq_freqs *freqs = (struct cpufreq_freqs *)data;
	static struct timeval time;
	(void)nb;
	jiffies_to_timeval(jiffies, &time);
	if (phase == CPUFREQ_POSTCHANGE) {
		curr_idx++;
		if (curr_idx >= NO_MON_ITEM)
			curr_idx = 0;

		sprintf(monitor_buf[curr_idx],
				"[%12u us] CPU %d: from %u KHz -> to: %u KHz",
				(u32)(time.tv_sec * 1000000 + time.tv_usec),
				(u32)freqs->cpu, (u32)freqs->old,
				(u32)freqs->new);
		MSG_INFO("%s", monitor_buf[curr_idx]);
	}
	return NOTIFY_OK;
}

static int monitor_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	int pos = 0;
	char *s = buf_reg;
	int cur = 0;
	int i = 0;
	FUNC_MSG_IN;
#ifdef CONFIG_CPU_FREQ
	para = strim(para);

	ret = get_word(para, size, 0, item, &para_sz);
	if (ret <= 0) {
		ret = -EINVAL;
		goto fail;
	}
	pos = ret;

	ret = strncmp(item, "start", sizeof("start"));
	if (0 == ret) {
		if (mon_state) {
			MSG_INFO("Monitor has been started");
			goto end;
		}
		ret = start_monitor();
		if (ret)
			goto fail;
		s += sprintf(s, "Started monitor Freq change\n");
		goto end;
	}
	ret = strncmp(item, "get", sizeof("get"));
	if (0 == ret) {
		if (!mon_state) {
			s += sprintf(s, "Monitor has not been started yet\n");
			ret = -1;
			goto end;
		}

		cur = curr_idx;
		cur++;
		for (i = 0; i < NO_MON_ITEM; i++, cur++) {
			if (cur >= NO_MON_ITEM)
				cur = 0;

			s += sprintf(s, "%s\n", monitor_buf[cur]);
		}
		goto end;
	}
	ret = strncmp(item, "stop", sizeof("stop"));
	if (0 == ret) {
		if (!mon_state) {
			s += sprintf(s, "Monitor has not been started yet\n");
			ret = -1;
			goto end;
		}
		ret = stop_monitor();
		if (ret)
			goto fail;
		s += sprintf(s, "Monitor has been stopped");
		goto end;
	}
#else /*!CONFIG_CPU_FREQ*/
	s += sprintf(s, "CONFIG_CPU_FREQ is disable\n");
#endif /*CONFIG_CPU_FREQ*/
	ret =  -EINVAL;
fail:
	s += sprintf(s, "FAILED\n");
end:
	MSG_INFO("%s", buf_reg);
	FUNC_MSG_RET(ret);
}

static void dfs_show(char **buf)
{
	FUNC_MSG_IN;
	*buf = buf_reg;
	FUNC_MSG_OUT;
}

static int dfs_init(void)
{
	FUNC_MSG_IN;
	ADD_CMD(dfs, start);
	ADD_CMD(dfs, stop);
	ADD_CMD(dfs, en);
	ADD_CMD(dfs, dis);
	ADD_CMD(dfs, sup);
	ADD_CMD(dfs, clk_get);
	ADD_CMD(dfs, mon);

	if (shmobile_chip_rev() >= ES_REV_2_0)
		__clk_hw_info = __clk_hw_info_es2_x;
	else
		__clk_hw_info = __clk_hw_info_es1_x;

	curr_idx = 0;
	mon_state = 0;

	FUNC_MSG_RET(0);
}

static void dfs_exit(void)
{
	FUNC_MSG_IN;
	DEL_CMD(dfs, start);
	DEL_CMD(dfs, stop);
	DEL_CMD(dfs, en);
	DEL_CMD(dfs, dis);
	DEL_CMD(dfs, sup);
	DEL_CMD(dfs, clk_get);
	DEL_CMD(dfs, mon);

	FUNC_MSG_OUT;
}



