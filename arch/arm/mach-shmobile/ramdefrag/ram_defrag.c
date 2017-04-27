/*
 * arch/arm/mach-shmobile/ramdefrag/ram_defrag.c
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

/* Main processing of RAM defragmentation (Core part) */
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/syscalls.h>
#include <linux/memory.h>
#include <linux/compaction.h>
#include <linux/page-flags.h>
#include <linux/export.h>
#include <linux/module.h>
#include <mach/memory-r8a7373.h>

#include "ram_defrag_internal.h"

/* #define DEBUG_RAMDEFRAG */

#ifdef DEBUG_RAMDEFRAG
	#define DEFRAG_PRINTK(fmt, arg...)  printk(fmt, ##arg)
#else
	#define DEFRAG_PRINTK(fmt, arg...)
#endif

#define SDRAM_START_ADDR		0x40000000
#define SDRAM_END_ADDR			0x7FFFFFFF
#define BANK_SIZE			0x04000000

#define INUSED_RANGE	(SDRAM_KERNEL_START_ADDR - SDRAM_START_ADDR)

#define USED_BANKS_ABOVE	(((INUSED_RANGE % BANK_SIZE) != 0) \
				? ((INUSED_RANGE / BANK_SIZE) + 1) \
				: (INUSED_RANGE / BANK_SIZE))

#define UNUSED_BANKS_START	(SDRAM_START_ADDR + \
				(USED_BANKS_ABOVE * BANK_SIZE))

#define RANGE_SKIP		(((INUSED_RANGE % BANK_SIZE) != 0) \
		? (UNUSED_BANKS_START - SDRAM_KERNEL_START_ADDR) : 0)

#define USED_RANGE_BELOW	\
				(SDRAM_END_ADDR - SDRAM_KERNEL_END_ADDR)

#define USED_BANKS_BELOW		(((USED_RANGE_BELOW % BANK_SIZE) != 0) \
				? ((USED_RANGE_BELOW / BANK_SIZE) + 1) : \
					(USED_RANGE_BELOW / BANK_SIZE))

#define SDRAM_SIZE		((SDRAM_END_ADDR - SDRAM_START_ADDR) + 1)

#define MAX_BANKS			(SDRAM_SIZE / BANK_SIZE)
#define UNUSED_BANK_IN_KERNEL	(MAX_BANKS - \
			USED_BANKS_ABOVE - USED_BANKS_BELOW)

#define MAX_PAGES_IN_BANK	(BANK_SIZE / PAGE_SIZE)

#define USED_BANKS_MASK(nr_above, nr_below)	\
		(~(0xFFFF0000 | ((0xFFFF << (nr_above)) \
				& (0xFFFF >> (nr_below)))))

const unsigned int max_unused_banks = UNUSED_BANK_IN_KERNEL;
const unsigned int max_pages_in_bank = MAX_PAGES_IN_BANK;
const unsigned int range_skip = RANGE_SKIP;
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
unsigned int get_ram_banks_status(void)
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
	start_page_check = mem_map + range_skip/PAGE_SIZE;
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
EXPORT_SYMBOL_GPL(get_ram_banks_status);

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

static void drop_pagecache_sb(struct super_block *sb, void *unused)
{
	struct inode *inode;
	struct inode *toput_inode;
	toput_inode = NULL;
	DEFRAG_PRINTK("%s\n", __func__);
	spin_lock(&inode_sb_list_lock);
	list_for_each_entry(inode, &sb->s_inodes, i_sb_list) {
		spin_lock(&inode->i_lock);
		if ((inode->i_state & (I_FREEING|I_WILL_FREE|I_NEW)) ||
		    (inode->i_mapping->nrpages == 0)) {
			spin_unlock(&inode->i_lock);
			continue;
		}
		__iget(inode);
		spin_unlock(&inode->i_lock);
		spin_unlock(&inode_sb_list_lock);
		invalidate_mapping_pages(inode->i_mapping, 0, -1);
		iput(toput_inode);
		toput_inode = inode;
		spin_lock(&inode_sb_list_lock);
	}
	spin_unlock(&inode_sb_list_lock);
	iput(toput_inode);
}

static void drop_slab(void)
{
	int nr_objects;

	struct shrink_control shrink = {
		.gfp_mask = GFP_KERNEL,
	};

	DEFRAG_PRINTK("%s\n", __func__);
	do {
		nr_objects = shrink_slab(&shrink, 1000, 1000);
	} while (nr_objects > 10);
}

#ifdef CONFIG_COMPACTION

/*
 * defrag: Execute RAM defragmentation
 * return:
 *		0: Normal operation.
 */
int defrag()
{
	int ret;
	ret = 0;
	DEFRAG_PRINTK("%s\n", __func__);

	/* Flush caches */
	iterate_supers(drop_pagecache_sb, NULL);
	drop_slab();

	/* Do defragmentation on all nodes */
	ret = compact_nodes();
	if (ret == 3) { /* If compaction is finished */
		ret = 0;
	}
	/* Flush caches again*/
	iterate_supers(drop_pagecache_sb, NULL);
	drop_slab();
	return ret;
}
EXPORT_SYMBOL_GPL(defrag);

#endif /*CONFIG_COMPACTION*/


