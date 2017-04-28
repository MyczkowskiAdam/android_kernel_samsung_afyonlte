/*
 * pmdbg_hw.c
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
#include <linux/string.h>
#include <linux/parser.h>
#include <linux/errno.h>
#include <linux/io.h>
#include "pmdbg_hw.h"
#include <mach/pm.h>

static int rreg_cmd(char *, int);
static int wreg_cmd(char *, int);
static int mreg_cmd(char *, int);
static int rev_cmd(char *, int);
static int hw_init(void);
static void hw_exit(void);

LOCAL_DECLARE_MOD(hw, hw_init, hw_exit);

DECLARE_CMD(rreg, rreg_cmd);
DECLARE_CMD(wreg, wreg_cmd);
DECLARE_CMD(mreg, mreg_cmd);
DECLARE_CMD(rev, rev_cmd);


void mreg32(void __iomem *addr, u32 set, u32 clear)
{
	u32 value;
	value = ioread32(addr);
	value &= ~clear;
	value |= set;
	iowrite32(value, addr);
}


void mreg16(void __iomem *addr, u16 set, u16 clear)
{
	u16 value;
	value = ioread16(addr);
	value &= ~clear;
	value |= set;
	iowrite16(value, addr);
}


void mreg8(void __iomem *addr, u8 set, u8 clear)
{
	u8 value;
	value = ioread8(addr);
	value &= ~clear;
	value |= set;
	iowrite8(value, addr);
}



/*<size> <address (hexa)>
 * ex: rreg 32 0x123456
 * */
int rreg_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	int reg_size = 0;
	int reg_addr = 0;
	int pos = 0;
	int base_addr = 0;
	void __iomem *vir_addr = NULL;
	u32 offset = 0;
	int val;
	substring_t args;
	FUNC_MSG_IN;
	para = strim(para);
	/* Get size of register*/
	/* MSG("Get size of register...");*/
	ret = get_word(para, size, 0, item, &para_sz);
	if (ret <= 0) {
		ret =  -EINVAL;
		goto fail;
	}
	/* MSG("Size in string: %s", item);*/
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &reg_size);
	if (ret)
		goto fail;

	/* MSG("Size in dec: %d", reg_size);*/

	if (reg_size != 32 && reg_size != 16 && reg_size != 8) {
		ret = -ENOTSUPP;
		goto end;
	}

	/* Get phys address*/
	/* MSG("Get phys address...");*/
	ret = get_word(para, size, pos, item, &para_sz);
	if (ret <= pos) {
		ret =  -EINVAL;
		goto fail;
	}
	/*MSG("Address in string: %s", item);*/
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &reg_addr);
	if (ret)
		goto fail;

	/*MSG("Address in hex: 0x%x", reg_addr);*/
	/* Mapping*/
	base_addr = reg_addr & 0xFFFFF000;
	offset = reg_addr & 0x00000FFF;

	/*MSG("Map IO address with base: 0x%x", base_addr);*/
	vir_addr = ioremap_nocache(base_addr, PAGE_SIZE);
	if (!vir_addr) {
		ret = -EINVAL;
		goto fail;
	}
	/*MSG("Virtual address: 0x%x", vir_addr);*/

	switch (reg_size) {
	case 32:
		val = rreg32(vir_addr+offset);
		break;
	case 16:
		val = (unsigned long)rreg16(vir_addr+offset);
		break;
	case 8:
		val = (unsigned long)rreg8(vir_addr+offset);
		break;
	default:
		ret = -EINVAL;
		iounmap(vir_addr);
		vir_addr = NULL;
		goto fail;
		break;
	};
	iounmap(vir_addr);
	vir_addr = NULL;
	ret = 0;
	sprintf(bufres, "0x%x: 0x%x", reg_addr, val);
	/*MSG("Value: 0x%x", val);*/
	goto end;

fail:
	sprintf(bufres, "FAILED");
end:
	MSG_INFO("%s", bufres);
	FUNC_MSG_RET(ret);
}


/*<size> <address (hexa)> <value>
 * ex: wreg 32 0x123456 0xABC
 * */
int wreg_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	int reg_size = 0;
	int reg_addr = 0;
	int pos = 0;
	int base_addr = 0;
	void __iomem *vir_addr = NULL;
	u32 offset = 0;
	u32 set_value = 0;
	/* u32 val = 0;*/
	substring_t args;

	FUNC_MSG_IN;
	para = strim(para);
	/* Get size of register*/
	/* MSG("Get size of register...");*/
	ret = get_word(para, size, 0, item, &para_sz);
	if (ret <= 0) {
		ret =  -EINVAL;
		goto fail;
	}
	/* MSG("Size in string: %s", item);*/
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &reg_size);
	if (ret)
		goto fail;

	/* MSG("Size in dec: %d", reg_size);*/

	if (reg_size != 32 && reg_size != 16 && reg_size != 8) {
		ret = -ENOTSUPP;
		goto end;
	}

	/* Get phys address*/
	/* MSG("Get phys address...");*/
	ret = get_word(para, size, pos, item, &para_sz);
	if (ret <= pos) {
		ret =  -EINVAL;
		goto fail;
	}
	/*MSG("Address in string: %s", item);*/
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &reg_addr);
	if (ret)
		goto fail;


	/* Get value*/
	/* MSG("Get phys address...");*/
	ret = get_word(para, size, pos, item, &para_sz);
	if (ret <= pos) {
		ret =  -EINVAL;
		goto fail;
	}
	/*MSG("value in string: %s", item);*/
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &set_value);
	if (ret)
		goto fail;


	/*MSG("set_value in hex: 0x%x", set_value);*/

	/* Mapping*/
	base_addr = reg_addr & 0xFFFFF000;
	offset = reg_addr & 0x00000FFF;

	/*MSG("Map IO address with base: 0x%x", base_addr);*/
	vir_addr = ioremap_nocache(base_addr, PAGE_SIZE);
	if (!vir_addr) {
		ret = -EINVAL;
		goto fail;
	}
	/*MSG("Virtual address: 0x%x", vir_addr);*/

	switch (reg_size) {
	case 32:
		wreg32(vir_addr+offset, set_value);
		/* val = rreg32(vir_addr+offset);*/
		break;
	case 16:
		wreg16(vir_addr+offset, (u16)set_value);
		/* val = (u32)rreg16(vir_addr+offset);*/
		break;
	case 8:
		wreg8(vir_addr+offset, (u8)set_value);
		/* val = (u32)rreg8(vir_addr+offset);*/
		break;
	default:
		ret = -EINVAL;
		iounmap(vir_addr);
		vir_addr = NULL;
		goto fail;
		break;
	};
	iounmap(vir_addr);
	vir_addr = NULL;
	ret = 0;
	/* sprintf(bufres, "0x%x:\nSet: 0x%x\nActual: 0x%x",
			reg_addr, set_value, val);*/
	sprintf(bufres, "0x%x:\nSet: 0x%x\n",
			reg_addr, set_value);
	/*MSG("Value: 0x%x", val);*/
	goto end;

fail:
	sprintf(bufres, "FAILED");
end:
	MSG_INFO("%s", bufres);
	FUNC_MSG_RET(ret);
}

/*<size> <address (hexa)> <mask set> <mask clear>
 * ex: wreg 32 0x123456 0xABC
 * */
int mreg_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	int reg_size = 0;
	int reg_addr = 0;
	int pos = 0;
	int base_addr = 0;
	void __iomem *vir_addr = NULL;
	u32 offset = 0;
	u32 mask_set = 0;
	u32 mask_clr = 0;
	/* u32 val = 0;*/
	substring_t args;
	char *s = bufres;

	FUNC_MSG_IN;
	para = strim(para);
	/* Get size of register*/
	/* MSG("Get size of register...");*/
	ret = get_word(para, size, 0, item, &para_sz);
	if (ret <= 0) {
		ret =  -EINVAL;
		goto fail;
	}
	/* MSG("Size in string: %s", item);*/
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &reg_size);
	if (ret)
		goto fail;

	/* MSG("Size in dec: %d", reg_size);*/

	if (reg_size != 32 && reg_size != 16 && reg_size != 8) {
		ret = -ENOTSUPP;
		goto end;
	}

	/* Get phys address*/
	/* MSG("Get phys address...");*/
	ret = get_word(para, size, pos, item, &para_sz);
	if (ret <= pos) {
		ret =  -EINVAL;
		goto fail;
	}
	/*MSG("Address in string: %s", item);*/
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &reg_addr);
	if (ret)
		goto fail;


	/* Get mask set*/
	/* MSG("Get phys address...");*/
	ret = get_word(para, size, pos, item, &para_sz);
	if (ret <= pos) {
		ret =  -EINVAL;
		goto fail;
	}
	/*MSG("value in string: %s", item);*/
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &mask_set);
	if (ret)
		goto fail;


	/*MSG("mask_set in hex: 0x%x", set_value);*/

	/* Get mask clear*/
	/* MSG("Get phys address...");*/
	ret = get_word(para, size, pos, item, &para_sz);
	if (ret <= pos) {
		ret =  -EINVAL;
		goto fail;
	}
	/*MSG("value in string: %s", item);*/
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &mask_clr);
	if (ret)
		goto fail;


	/*MSG("mask_clr in hex: 0x%x", mask_clr);*/

	/* Mapping*/
	base_addr = reg_addr & 0xFFFFF000;
	offset = reg_addr & 0x00000FFF;

	/*MSG("Map IO address with base: 0x%x", base_addr);*/
	vir_addr = ioremap_nocache(base_addr, PAGE_SIZE);
	if (!vir_addr) {
		ret = -EINVAL;
		goto fail;
	}
	/*MSG("Virtual address: 0x%x", vir_addr);*/

	switch (reg_size) {
	case 32:
		mreg32(vir_addr+offset, mask_set, mask_clr);
		/* val = rreg32(vir_addr+offset);*/
		break;
	case 16:
		mreg16(vir_addr+offset, (u16)mask_set, (u16)mask_clr);
		/* val = (u32)rreg16(vir_addr+offset);*/
		break;
	case 8:
		mreg8(vir_addr+offset, (u8)mask_set, (u8)mask_clr);
		/* val = (u32)rreg8(vir_addr+offset);*/
		break;
	default:
		ret = -EINVAL;
		iounmap(vir_addr);
		vir_addr = NULL;
		goto fail;
		break;
	};
	iounmap(vir_addr);
	vir_addr = NULL;
	ret = 0;
	s += sprintf(s, "0x%x:\nMask set: 0x%x\n",
			reg_addr, mask_set);
	s += sprintf(s, "Mask clr: 0x%x\n", mask_clr);
	/* bufres += sprintf(bufres, "Actual: 0x%x\n", val);*/

	goto end;

fail:
	sprintf(bufres, "FAILED");
end:
	MSG_INFO("%s", bufres);
	FUNC_MSG_RET(ret);
}

int rev_cmd(char *para, int size)
{
	unsigned int ret = system_rev;
	FUNC_MSG_IN;

	switch (ret) {
	case CHIP_VERSION_ES2_0:
		sprintf(bufres, "R-MobileU2 ES2.0");
		break;
	case CHIP_VERSION_ES2_1:
		sprintf(bufres, "R-MobileU2 ES2.1");
		break;
	case CHIP_VERSION_ES2_2:
		sprintf(bufres, "R-MobileU2 ES2.2");
		break;
	case CHIP_VERSION_ES2_3:
		sprintf(bufres, "R-MobileU2 ES2.3");
		break;
	default:
		sprintf(bufres, "R-MobileU2 ES2.x");
		break;
	}
	ret = 0;
	MSG_INFO("%s", bufres);
	FUNC_MSG_RET(ret);
}

int hw_init(void)
{
	FUNC_MSG_IN;
	ADD_CMD(hw, rreg);
	ADD_CMD(hw, wreg);
	ADD_CMD(hw, mreg);
	ADD_CMD(hw, rev);

	FUNC_MSG_RET(0);
}

void hw_exit(void)
{
	FUNC_MSG_IN;
	DEL_CMD(hw, rreg);
	DEL_CMD(hw, wreg);
	DEL_CMD(hw, mreg);
	DEL_CMD(hw, rev);
	FUNC_MSG_OUT;
}

