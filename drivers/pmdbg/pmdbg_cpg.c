/*
 * pmdbg_cpg.c
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

#include "pmdbg_api.h"
#include "pmdbg_hw.h"
#include <asm/page.h>
#include <linux/io.h>
#include <mach/r8a7373.h>

#define CPG_SIZE			0xFFFF
/*Below macros are offsets from the CPG_BASE_ADDR. These are offsets used only
  for PMDBG utility */
#define FRQCRA_OFFSET		0x0000
#define FRQCRB_OFFSET		0x0004
#define FRQCRD_OFFSET		0x00E4
#define PLLECR_OFFSET		0x00D0
#define PLL0CR_OFFSET		0x00D8
#define PLL1CR_OFFSET		0x0028
#define PLL2CR_OFFSET		0x002C
#define PLL3CR_OFFSET		0x00DC
#define PLL0STPCR_OFFSET	0x00F0
#define PLL1STPCR_OFFSET	0x00C8
#define PLL2STPCR_OFFSET	0x00F8
#define PLL3STPCR_OFFSET	0x00FC
#define MSTPSR0_OFFSET		0x0030
#define MSTPSR1_OFFSET		0x0038
#define MSTPSR2_OFFSET		0x0040
#define MSTPSR3_OFFSET		0x0048
#define MSTPSR4_OFFSET		0x004C
#define MSTPSR5_OFFSET		0x003C

static int cpg_init(void);
static void cpg_exit(void);
static void cpg_show(char **);

static char buf_reg[1024];

LOCAL_DECLARE_MOD_SHOW(cpg, cpg_init, cpg_exit, cpg_show);

#define sprint_reg(s, r) (s += sprintf(s, #r " (0x%x): 0x%x\n", \
					CPG_BASE_PHYS + r##_OFFSET, \
					rreg(vir_addr + r##_OFFSET)))

void cpg_show(char **buf)
{
	void __iomem *vir_addr = NULL;
	char *s = buf_reg;
	FUNC_MSG_IN;
	vir_addr = ioremap_nocache(CPG_BASE_PHYS, PAGE_SIZE);

	if (!vir_addr) {
		s += sprintf(s, "Failed: No memory\n");
		goto end;
	}

	memset(buf_reg, 0, sizeof(buf_reg));

	sprint_reg(s, FRQCRA);
	sprint_reg(s, FRQCRB);
	sprint_reg(s, FRQCRD);
	sprint_reg(s, PLLECR);
	sprint_reg(s, PLL0CR);
	sprint_reg(s, PLL1CR);
	sprint_reg(s, PLL2CR);
	sprint_reg(s, PLL3CR);
	sprint_reg(s, PLL0STPCR);
	sprint_reg(s, PLL1STPCR);
	sprint_reg(s, PLL2STPCR);
	sprint_reg(s, PLL3STPCR);
	sprint_reg(s, MSTPSR0);
	sprint_reg(s, MSTPSR1);
	sprint_reg(s, MSTPSR2);
	sprint_reg(s, MSTPSR3);
	sprint_reg(s, MSTPSR4);
	sprint_reg(s, MSTPSR5);

	iounmap(vir_addr);
	vir_addr = NULL;
end:
	*buf = buf_reg;
	FUNC_MSG_OUT;
}


int cpg_init(void)
{
	FUNC_MSG_IN;
	FUNC_MSG_RET(0);
}

void cpg_exit(void)
{
	FUNC_MSG_IN;
	FUNC_MSG_OUT;
}



