/*
 * pmdbg_sysc.c
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

#define SYSC_SIZE			0xFFFF
#define SBAR_OFFSET			0x0020
#define PSTR_OFFSET			0x0080
#define WUPSFAC_OFFSET		0x0098
#define WUPRFAC_OFFSET		0x0094
#define SRSTFR_OFFSET		0x00B4
#define WUPSMSK_OFFSET		0x002C
#define WUPRMSK_OFFSET		0x0028
#define SWUCR_OFFSET		0x0014
#define RWUCR_OFFSET		0x0010
#define SWBCR_OFFSET		0x0204
#define RWBCR_OFFSET		0x0200
#define SYCKENMSK_OFFSET	0x024C
#define APSCSTP_OFFSET		0x0234
#define EXMSKCNT1_OFFSET	0x0214
#define EXMSKCNT2_OFFSET	0x0210
#define WUPRCR_OFFSET		0x0260
#define WUPSCR_OFFSET		0x0264

static int sysc_init(void);
static void sysc_exit(void);
static void sysc_show(char **);

static char buf_reg[1024];

LOCAL_DECLARE_MOD_SHOW(sysc, sysc_init, sysc_exit, sysc_show);

#define sprint_reg(s, r) (s += sprintf(s, #r " (0x%x): 0x%x\n", \
					SYSC_BASE_PHYS + r##_OFFSET, \
					rreg(vir_addr + r##_OFFSET)))

void sysc_show(char **buf)
{
	void __iomem *vir_addr = NULL;
	char *s = buf_reg;
	FUNC_MSG_IN;
	vir_addr = ioremap_nocache(SYSC_BASE_PHYS, PAGE_SIZE);
	if (!vir_addr) {
		s += sprintf(s, "Failed: No memory\n");
		goto end;
	}
	memset(buf_reg, 0, sizeof(buf_reg));

	sprint_reg(s, SBAR);
	sprint_reg(s, PSTR);
	sprint_reg(s, WUPSFAC);
	sprint_reg(s, WUPRFAC);
	sprint_reg(s, SRSTFR);
	sprint_reg(s, WUPSMSK);
	sprint_reg(s, WUPRMSK);
	sprint_reg(s, SWUCR);
	sprint_reg(s, RWUCR);
	sprint_reg(s, SWBCR);
	sprint_reg(s, RWBCR);
	sprint_reg(s, SYCKENMSK);
	sprint_reg(s, APSCSTP);
	sprint_reg(s, EXMSKCNT1);
	sprint_reg(s, EXMSKCNT2);
	sprint_reg(s, WUPRCR);
	sprint_reg(s, WUPSCR);

	iounmap(vir_addr);
	vir_addr = NULL;
end:
	*buf = buf_reg;
	FUNC_MSG_OUT;
}


int sysc_init(void)
{
	FUNC_MSG_IN;

	FUNC_MSG_RET(0);
}

void sysc_exit(void)
{
	FUNC_MSG_IN;
	FUNC_MSG_OUT;
}



