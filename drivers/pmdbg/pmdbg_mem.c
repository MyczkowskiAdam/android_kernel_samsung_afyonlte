/*
 * pmdbg_mem.c
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
#include "pmdbg_core.h"
#include <linux/errno.h>
#include <mach/pm.h>
#include <linux/string.h>
#include <linux/parser.h>
#include <linux/io.h>
#include <mach/memory-r8a7373.h>

#ifdef CONFIG_SHMOBILE_RAM_DEFRAG
#include <mach/ram_defrag.h>
#endif

static int bank_status_cmd(char *, int);
static int defrag_cmd(char *, int);
static int dump_cmd(char *, int);

static int pmdgb_mem_init(void);
static void pmdgb_mem_exit(void);

LOCAL_DECLARE_MOD(mem, pmdgb_mem_init, pmdgb_mem_exit);

DECLARE_CMD(bank, bank_status_cmd);
DECLARE_CMD(defrag, defrag_cmd);
DECLARE_CMD(dump, dump_cmd);

#ifndef CONFIG_SHMOBILE_RAM_DEFRAG

#include <linux/compaction.h>
#include <linux/mm.h>
#include <linux/memory.h>
#include <linux/page-flags.h>

static unsigned int get_ram_banks_status(void);
static int page_check(struct page *page);

/* #define DEBUG_RAMDEFRAG */

#ifdef DEBUG_RAMDEFRAG
	#define DEFRAG_PRINTK(fmt, arg...)  printk(fmt, ##arg)
#else
	#define DEFRAG_PRINTK(fmt, arg...)
#endif

#define SDRAM_START			0x40000000
#define SDRAM_END			0x7FFFFFFF
#define BANK_SIZE			0x04000000

#define INUSED_RANGE		(SDRAM_KERNEL_START_ADDR - SDRAM_START)
#define USED_BANKS_ABOVE	(((INUSED_RANGE % BANK_SIZE) != 0) \
				? ((INUSED_RANGE / BANK_SIZE) + 1) \
				: (INUSED_RANGE / BANK_SIZE))

#define UNUSED_BANKS_START	(SDRAM_START + (USED_BANKS_ABOVE * BANK_SIZE))
#define NUMBER_PAGES_SKIP	(((INUSED_RANGE % BANK_SIZE) != 0) \
		? (UNUSED_BANKS_START - SDRAM_KERNEL_START_ADDR) : 0)

#define SDRAM_SIZE			((SDRAM_END - SDRAM_START) + 1)
#define MAX_BANKS			(SDRAM_SIZE / BANK_SIZE)
#define MAX_UNUSED_BANKS	(MAX_BANKS - USED_BANKS_ABOVE)
#define MAX_PAGES_IN_BANK	(BANK_SIZE / PAGE_SIZE)

#define USED_RANGE_BELOW	(SDRAM_END - SDRAM_KERNEL_END_ADDR)

#define USED_BANKS_BELOW	(((USED_RANGE_BELOW % BANK_SIZE) != 0) \
				? ((USED_RANGE_BELOW / BANK_SIZE) + 1) : \
					(USED_RANGE_BELOW / BANK_SIZE))

#define USED_BANKS_MASK(nr_above, nr_below)	\
		(~(0xFFFF0000 | ((0xFFFF << (nr_above)) \
				& (0xFFFF >> (nr_below)))))

const unsigned int max_unused_banks = MAX_UNUSED_BANKS;
const unsigned int max_pages_in_bank = MAX_PAGES_IN_BANK;
const unsigned int number_pages_skip = NUMBER_PAGES_SKIP;
const unsigned int used_banks_above = USED_BANKS_ABOVE;
const unsigned int used_banks_below = USED_BANKS_BELOW;

/*
 * get_ram_banks_status: Get status of RAM banks
 * return: A bit mask which corresponds to status of RAM banks
 *		- If certain bank is used, correlative bit status in
 *		 return value will be set to 1
 *		- If certain bank is freed, correlative bit status in
 *		 return value will be cleared to 0
 *		- Bit 16 to bit 31 are "Don't Care" which
 *		correspond to non-existent banks, will be set to 1
 *		- Bit status is least significant
 *		(bit 0 corresponds to bank 0, bit 1 corresponds to bank 1, etc.)
 */
static unsigned int get_ram_banks_status(void)
{
	unsigned int i;
	unsigned int j;
	unsigned int begin;
	unsigned int end;
	unsigned int status;
	struct page *start_page_check;
	i = 0;
	j = 0;
	begin = 0;
	end = 0;
	status = 0xFFFFFFFF;
	DEFRAG_PRINTK("%s\n", __func__);
	/* start checking */
	start_page_check = mem_map + number_pages_skip/PAGE_SIZE;
	for (i = 0; i < max_unused_banks; i++) {
		begin = i * max_pages_in_bank;
		end = ((i + 1) * max_pages_in_bank) - 1;
		for (j = begin; j <= end; j++) {
			if (page_check(start_page_check + j) == 1)
				break;
		}
		if (j > end)
			status &= ~(1 << i);
	}

	status = (status << used_banks_above) | \
		USED_BANKS_MASK(used_banks_above, used_banks_below);
	return status;
}

/*
 * page_check: Check whether page is "use" or "free"
 * @page: Page needs to check status
 * return:
 *	0: Page is "freed"
 *	1: Page is "used"
 *	EINVAL: invalid argument
 */
static int page_check(struct page *page)
{
	int mapcount;
	int pagecount;
	mapcount = 0;
	pagecount = 0;
	if (page != NULL) {
		mapcount = atomic_read(&page->_mapcount);
		pagecount = atomic_read(&page->_count);
		if ((mapcount >= 0) || (page->mapping != NULL)
			|| (pagecount > 0)
			|| (page->flags & PAGE_FLAGS_CHECK_AT_FREE)) {
			return 1;	/* Page is "used" */
		}
		return 0;		/* Page is "freed" */
	} else {
		return -EINVAL;
	}
}
#endif /* CONFIG_SHMOBILE_RAM_DEFRAG */

static int bank_status_cmd(char *para, int size)
{
	int ret = 0;
	char *s = bufres;
	FUNC_MSG_IN;
#ifdef CONFIG_COMPACTION
	ret = get_ram_banks_status();
	s += sprintf(s, "0x%x", ret);
	ret = 0;
#else /*!CONFIG_COMPACTION*/
	s += sprintf(s, "Not supported\n");
	ret = -ENOTSUPP;
#endif /*CONFIG_COMPACTION*/
	MSG_INFO("%s", bufres);
	FUNC_MSG_RET(ret);
}

static int defrag_cmd(char *para, int size)
{
	int ret = 0;
	char *s = bufres;
	struct timeval beforetime;
	struct timeval aftertime;
	FUNC_MSG_IN;
#ifdef CONFIG_COMPACTION
	ret = get_ram_banks_status();
	s += sprintf(s, "Bank status before 0x%x\n", ret);
	MSG("Defraging...");
	do_gettimeofday(&beforetime);
#ifndef CONFIG_SHMOBILE_RAM_DEFRAG
	ret = compact_nodes();
#else
	ret = defrag();
#endif /* CONFIG_SHMOBILE_RAM_DEFRAG */
	do_gettimeofday(&aftertime);
	MSG("Defrag done");
	s += sprintf(s, "Defraged in %12uus\n",
				(u32)((aftertime.tv_sec -
				beforetime.tv_sec) * 1000000
				+ (aftertime.tv_usec - beforetime.tv_usec)));
	ret = get_ram_banks_status();
	s += sprintf(s, "Bank status after 0x%x\n", ret);
	ret = 0;
#else /*!CONFIG_COMPACTION*/
	s += sprintf(s, "Not supported\n");
	ret = -ENOTSUPP;
#endif /*CONFIG_COMPACTION*/
	MSG_INFO("%s", bufres);
	FUNC_MSG_RET(ret);
}

static void dump_memory(u32 address, int length)
{
	u32 start = (int)address;
	u32 end = (int)address + length;
	u32 i = 0;
	u32 j = 0;
	char data[40];
	char str[40];
	char *data_tmp;
	char *str_tmp;
	char c = '.';
	for (i = start; i < end; i += 16) {
		data_tmp = data;
		str_tmp = str;
		for (j = 0; j < 16 && i+j < end; j++) {
			c = *((char *)(i+j));
			data_tmp += sprintf(data_tmp, "%02x ",
						(unsigned char)c);
			if (IS_CHAR(c))
				str_tmp += sprintf(str_tmp, "%c", c);
			else
				str_tmp += sprintf(str_tmp, ".");
		}
		*data_tmp = 0;
		*str_tmp = 0;
		MSG_INFO("<0x%x> %s%s", i, data, str);
	}
}

static int dump_cmd(char *para, int size)
{
	int ret = 0;
	char item[PAR_SIZE];
	int para_sz = 0;
	int length = 0;
	int addr = 0;
	int pos = 0;
	int is_addr_vir = 0;
	void __iomem *vir_addr = NULL;
	u32 base_addr = 0;
	void __iomem *map_addr = NULL;
	u32 map_size = 0;
	substring_t args;
	FUNC_MSG_IN;
	para = strim(para);
	/* Address type*/
	ret = get_word(para, size, 0, item, &para_sz);
	pos = ret;
	if (ret <= 0) {
		ret =  -EINVAL;
		goto fail;
	}
	ret = strncmp(item, "v", sizeof("v"));
	if (0 == ret) {
		is_addr_vir = 1;
	} else {
		ret = strncmp(item, "p", sizeof("p"));
		if (0 == ret) {
			is_addr_vir = 0;
		} else {
			ret =  -EINVAL;
			goto fail;
		}
	}

	/* Address*/
	ret = get_word(para, size, pos, item, &para_sz);
	if (ret <= 0) {
		ret = -EINVAL;
		goto fail;
	}
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &addr);
	if (ret)
		goto fail;

	/* Length*/
	ret = get_word(para, size, pos, item, &para_sz);
	if (ret <= pos) {
		ret =  -EINVAL;
		goto fail;
	}
	pos = ret;
	args.from = item;
	args.to = item + para_sz;
	ret = match_int(&args, &length);
	if (ret)
		goto fail;

	if (length < 0) {
		ret =  -EINVAL;
		goto fail;
	}
	if (!is_addr_vir) {
		base_addr = addr & 0xFFFFF000;
		map_size = (addr - base_addr)+length;
		map_addr = ioremap_nocache(base_addr, map_size);
		vir_addr = map_addr + (addr - base_addr);
	} else {
		vir_addr = (void __iomem *)addr;
	}

	if (!vir_addr) {
		ret =  -ENOMEM;
		goto fail;
	}
	if (is_addr_vir) {
		MSG_INFO("Dump memory at vir address 0x%08x, length %d",
					(u32)vir_addr, length);
	} else {
		MSG_INFO(
			"Dump memory at phys address 0x%08x (vir: 0x%08x), length %d",
			(u32)addr, (u32)vir_addr, length);
	}
	dump_memory((int)vir_addr, length);
	if (map_addr) {
		iounmap(map_addr);
		map_addr = NULL;
	}

	sprintf(bufres, "Show done");
	goto end;
fail:
	sprintf(bufres, "FAILED");
end:
	MSG_INFO("%s", bufres);
	FUNC_MSG_RET(ret);
}


static int pmdgb_mem_init(void)
{
	FUNC_MSG_IN;
	ADD_CMD(mem, bank);
	ADD_CMD(mem, defrag);
	ADD_CMD(mem, dump);

	FUNC_MSG_RET(0);
}

static void pmdgb_mem_exit(void)
{
	FUNC_MSG_IN;
	DEL_CMD(mem, bank);
	DEL_CMD(mem, defrag);
	DEL_CMD(mem, dump);

	FUNC_MSG_OUT;
}



