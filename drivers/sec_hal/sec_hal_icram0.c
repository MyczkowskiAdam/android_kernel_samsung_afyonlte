/*
 * drivers/sec_hal/sec_hal_icram0.c
 *
 * Copyright (c) 2013, Renesas Mobile Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/hardirq.h>
#include <linux/errno.h>

#include <linux/io.h>


#define FALSE 0
#define TRUE  1
#define BLOCKCOUNT 128
#define ICRAM1_SIZE 4096
#define ICRAM1_ADDRESS 0xE63A0000
#define MEM_MSG_AREA_CLEAR(ptr) mem_msg_area_clear(ptr)


struct mem_alloc_info {
	void *virt_addr; /* the start address of the message area */
	uint8_t size;/* the size of allocation */
	uint8_t alloc; /* status of the block */
};

struct mem_msg_area {
	void *virt_baseptr; /* stores ioremap output */
	unsigned long phys_start; /* phys start addr */
	unsigned long phys_size; /* phys size */
	unsigned long offset; /* offset between phys and virt addrs */
	/* blocks to-be-allocated for out & in msgs */
	struct mem_alloc_info msg_blocks[BLOCKCOUNT];
};


static struct mem_msg_area g_smc0;
static DEFINE_SPINLOCK(g_spinlock_smc0);


static void mem_msg_area_clear(struct mem_msg_area *ptr)
{
	int i = 0;
	if (ptr) {
		ptr->virt_baseptr = NULL;
		ptr->phys_start = 0;
		ptr->phys_size = 0;
		for (; i < BLOCKCOUNT; i++) {
			ptr->msg_blocks[i].virt_addr = NULL;
			ptr->msg_blocks[i].size = 0;
			ptr->msg_blocks[i].alloc = FALSE;
		}
	}
}


unsigned long sec_hal_mem_msg_area_memcpy(
	void *dst,
	const void *src,
	unsigned long sz)
{
	__u8 *dst8 = (__u8 *)dst;
	__u8 *src8 = (__u8 *)src;

	while (sz--)
		*dst8++ = *src8++;

	return (unsigned long)dst;
}


unsigned long sec_hal_mem_msg_area_write(
	void *dst,
	const void *src,
	unsigned long sz)
{
	__u8 *dst8 = (__u8 *)dst;
	__u8 *src8 = (__u8 *)src;

	while (sz--)
		*dst8++ = *src8++;

	return (unsigned long)dst;
}


unsigned long sec_hal_mem_msg_area_read(
	void *dst,
	const void *src,
	unsigned long sz)
{
	__u8* dst8 = (__u8 *)dst;
	__u8* src8 = (__u8 *)src;

	while (sz--)
		*dst8++ =  *src8++;

	return (unsigned long)dst;
}


static void sec_hal_mem_msg_area_memset(
	void *buff,
	unsigned char data,
	unsigned int cnt)
{
	__u8 * ptr = (__u8 *)buff;

	while (cnt > 0) {
		*ptr++ = data;
		cnt--;
	}
}


#if (!defined(BLOCKCOUNT) && !BLOCKCOUNT)
#error !!local macro not defined, can cause div by zero exception!!
#endif
/* **********************************************************************
 * Function name      : sec_hal_mem_msg_area_calloc
 * Description        :
 * Return             : virtual address if success, NULL otherwise.
 * *********************************************************************/
void *sec_hal_mem_msg_area_calloc(unsigned int n, unsigned int sz)
{
	unsigned long flags = 0;
	unsigned int blk_sz, blk_cnt, blk_idx, idx = 0;
	int found = FALSE;
	void *vaddr = NULL;

	if (0 == (n*sz))
		return NULL;

	blk_sz = (g_smc0.phys_size)/BLOCKCOUNT;
	blk_cnt = ((n*sz)+blk_sz-1)/blk_sz;

	if (blk_cnt > BLOCKCOUNT)
		return NULL;

	spin_lock_irqsave(&g_spinlock_smc0, flags);
	/* critical section starting, do not 'call' anything that may sleep.*/

	/* seek big enough unallocated mem area */
	while (idx < BLOCKCOUNT) {
		if (FALSE == g_smc0.msg_blocks[idx].alloc) {
			found = TRUE;
			blk_idx = blk_cnt - 1;

			while (blk_idx > 0
				&& (idx+blk_idx) < BLOCKCOUNT) {
				found = (found && FALSE ==
					g_smc0.msg_blocks[idx+blk_idx].alloc);
				blk_idx--;
			}
			if (TRUE == found)
				break; /* check if the loop can be ended */
		}
		idx++;
	}

	/* return ptr to first block, update alloc info & ZI */
	if (TRUE == found && idx < BLOCKCOUNT) {
		/* alloc found message area */
		vaddr = g_smc0.msg_blocks[idx].virt_addr;
		g_smc0.msg_blocks[idx].size = blk_cnt;
		g_smc0.msg_blocks[idx].alloc = TRUE;
		sec_hal_mem_msg_area_memset(vaddr, 0, blk_sz);
		blk_cnt--;
	}

	/* also update allocation info for rest of the blocks & ZI */
	while (TRUE == found && blk_cnt > 0
		&& (idx+blk_cnt) < BLOCKCOUNT) {
		g_smc0.msg_blocks[idx+blk_cnt].alloc = TRUE;
		sec_hal_mem_msg_area_memset(
			g_smc0.msg_blocks[idx+blk_cnt].virt_addr,
			0, blk_sz);
		blk_cnt--;
	}

	/* critical section ending. */
	spin_unlock_irqrestore(&g_spinlock_smc0, flags);

	return vaddr; /* return allocated(or not) memory address */
}


/* **********************************************************************
 * Function name      : sec_hal_mem_msg_area_free
 * Description        :
 * Return             :
 * *********************************************************************/
void sec_hal_mem_msg_area_free(void *vaddr)
{
	unsigned long flags = 0;
	unsigned int blk_idx, idx = 0;

	if (NULL == vaddr)
		return;

	spin_lock_irqsave(&g_spinlock_smc0, flags);
	/* critical section starting, do not 'call' anything that may sleep.*/

	while (idx < BLOCKCOUNT) {
		if (g_smc0.msg_blocks[idx].virt_addr == vaddr) {
			blk_idx = g_smc0.msg_blocks[idx].size - 1;

			/* free allocated message area */
			g_smc0.msg_blocks[idx].size = 0;
			g_smc0.msg_blocks[idx].alloc = FALSE;

			/* free rest of the blocks */
			while (0 < blk_idx
				&& (idx+blk_idx) < BLOCKCOUNT) {
				g_smc0.msg_blocks[idx+blk_idx].size = 0;
				g_smc0.msg_blocks[idx+blk_idx].alloc = FALSE;
				blk_idx--;
			}
			break; /* terminate the seek-loop */
		}
		idx++; /* seek next mem block */
	}

	/* critical section ending. */
	spin_unlock_irqrestore(&g_spinlock_smc0, flags);
}


/* **********************************************************************
 * Function name      : sec_hal_virt_to_icram_phys
 * Description        :
 * Return             :
 * *********************************************************************/
unsigned long sec_hal_virt_to_icram_phys(unsigned long vaddr)
{
	unsigned long paddr;
#ifdef CONFIG_ARM_SEC_HAL_TEST_DISPATCHER
	paddr = virt_to_phys((void *)vaddr);
#else
	paddr = vaddr + g_smc0.offset;
#endif
	return paddr;
}


/* **********************************************************************
 * Function name      : sec_hal_icram_phys_to_virt
 * Description        :
 * Return             :
 * *********************************************************************/
unsigned long sec_hal_icram_phys_to_virt(unsigned long paddr)
{
	unsigned long vaddr;
#ifdef CONFIG_ARM_SEC_HAL_TEST_DISPATCHER
	vaddr = (unsigned long) phys_to_virt((phys_addr_t) paddr);
#else
	vaddr = paddr - g_smc0.offset;
#endif
	return vaddr;
}


/* **********************************************************************
 * Function name      : sec_hal_icram0_init
 * Description        :
 * Return             :
 * *********************************************************************/
int sec_hal_icram0_init(void)
{
	int ret = 0;
	unsigned int blk_sz, idx = 0;

	if (g_smc0.virt_baseptr) {
		ret = -EINVAL;
		goto e1;
	}

#ifdef CONFIG_ARM_SEC_HAL_TEST_DISPATCHER
	g_smc0.virt_baseptr = kmalloc(UL(ICRAM1_SIZE), GFP_KERNEL);
#else
	g_smc0.virt_baseptr = ioremap_nocache(UL(ICRAM1_ADDRESS),
		UL(ICRAM1_SIZE));
#endif /* CONFIG_ARM_SEC_HAL_TEST_DISPATCHER */
	g_smc0.phys_start = UL(ICRAM1_ADDRESS);
	g_smc0.phys_size = UL(ICRAM1_SIZE);
	g_smc0.offset = g_smc0.phys_start - (unsigned long)g_smc0.virt_baseptr;

	if (g_smc0.virt_baseptr == NULL) {
		ret = -EINVAL;
		goto e2;
	}

	blk_sz = (g_smc0.phys_size)/BLOCKCOUNT;
	/* initialize msg area alloc blocks */
	for (; idx < BLOCKCOUNT; idx++) {
		g_smc0.msg_blocks[idx].virt_addr =
			((__u8 *)g_smc0.virt_baseptr + blk_sz*idx);
		g_smc0.msg_blocks[idx].size = 0;
		g_smc0.msg_blocks[idx].alloc = FALSE;
	}

	return ret;

e2:	/*release_mem_region(UL(ICRAM1_ADDRESS), UL(ICRAM1_SIZE));*/
e1:	MEM_MSG_AREA_CLEAR(&g_smc0); /* leave mem struct as 'unallocated' */
	return ret;
}


