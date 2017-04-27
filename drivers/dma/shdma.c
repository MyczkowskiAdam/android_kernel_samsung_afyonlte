/*
 * Renesas SuperH DMA Engine support
 *
 * base is drivers/dma/flsdma.c
 *
 * Copyright (C) 2009 Nobuhiro Iwamatsu <iwamatsu.nobuhiro@renesas.com>
 * Copyright (C) 2009 Renesas Solutions, Inc. All rights reserved.
 * Copyright (C) 2007 Freescale Semiconductor, Inc. All rights reserved.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * - DMA of SuperH does not have Hardware DMA chain mode.
 * - MAX DMA size is 16MB.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/dmaengine.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/sh_dma.h>
#include <linux/notifier.h>
#include <linux/kdebug.h>
#include <linux/spinlock.h>
#include <linux/rculist.h>
#include <memlog/memlog.h>

#include "dmaengine.h"
#include "shdma.h"

/* DMA descriptor control */
enum sh_dmae_desc_status {
	DESC_IDLE,
	DESC_PREPARED,
	DESC_SUBMITTED,
	DESC_COMPLETED,	/* completed, have to call callback */
	DESC_WAITING,	/* callback called, waiting for ack / re-submit */
};

#define NR_DESCS_PER_CHANNEL 128

/* If using Hardware DescriptorMEM, allocate 6 descriptors per channel */
#define NR_RPT_HW_DESCS_PER_CH  6

/* Default MEMCPY transfer size = 2^2 = 4 bytes */
#define LOG2_DEFAULT_XFER_SIZE	2

/*
 * Used for write-side mutual exclusion for the global device list,
 * read-side synchronization by way of RCU, and per-controller data.
 */
static DEFINE_SPINLOCK(sh_dmae_lock);
static LIST_HEAD(sh_dmae_devices);

/* A bitmask with bits enough for enum sh_dmae_slave_chan_id */
static unsigned long sh_dmae_slave_used[BITS_TO_LONGS(SH_DMA_SLAVE_NUMBER)];

static void sh_dmae_chan_ld_cleanup(struct sh_dmae_chan *sh_chan, bool all);

static void chclr_write(struct sh_dmae_chan *sh_dc, u32 data)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_dc);

	__raw_writel(data, shdev->chan_reg +
		shdev->pdata->channel[sh_dc->id].chclr_offset / sizeof(u32));
}

static void sh_dmae_writel(struct sh_dmae_chan *sh_dc, u32 data, u32 reg)
{
	__raw_writel(data, sh_dc->base + reg / sizeof(u32));
}

static u32 sh_dmae_readl(struct sh_dmae_chan *sh_dc, u32 reg)
{
	return __raw_readl(sh_dc->base + reg / sizeof(u32));
}

static u16 dmaor_read(struct sh_dmae_device *shdev)
{
	u32 __iomem *addr = shdev->chan_reg + DMAOR / sizeof(u32);

	if (shdev->pdata->dmaor_is_32bit)
		return __raw_readl(addr);
	else
		return __raw_readw(addr);
}

static void dmaor_write(struct sh_dmae_device *shdev, u16 data)
{
	u32 __iomem *addr = shdev->chan_reg + DMAOR / sizeof(u32);

	if (shdev->pdata->dmaor_is_32bit)
		__raw_writel(data, addr);
	else
		__raw_writew(data, addr);
}

static void chcr_write(struct sh_dmae_chan *sh_dc, u32 data)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_dc);

	__raw_writel(data, sh_dc->base + shdev->chcr_offset / sizeof(u32));
}

static u32 chcr_read(struct sh_dmae_chan *sh_dc)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_dc);

	return __raw_readl(sh_dc->base + shdev->chcr_offset / sizeof(u32));
}

/*
 * Reset DMA controller
 *
 * SH7780 has two DMAOR register
 */
static void sh_dmae_ctl_stop(struct sh_dmae_device *shdev)
{
	unsigned short dmaor;
	unsigned long flags;

	spin_lock_irqsave(&sh_dmae_lock, flags);

	dmaor = dmaor_read(shdev);
	dmaor_write(shdev, dmaor & ~(DMAOR_NMIF | DMAOR_AE | DMAOR_DME));

	spin_unlock_irqrestore(&sh_dmae_lock, flags);
}

static int sh_dmae_rst(struct sh_dmae_device *shdev)
{
	unsigned short dmaor;
	unsigned long flags;

	spin_lock_irqsave(&sh_dmae_lock, flags);

	dmaor = dmaor_read(shdev) & ~(DMAOR_NMIF | DMAOR_AE | DMAOR_DME);

	if (shdev->pdata->chclr_present) {
		int i;
		for (i = 0; i < shdev->pdata->channel_num; i++) {
			struct sh_dmae_chan *sh_chan = shdev->chan[i];
			if (sh_chan)
				chclr_write(sh_chan, 0);
		}
	}

	dmaor_write(shdev, dmaor | shdev->pdata->dmaor_init);

	dmaor = dmaor_read(shdev);

	spin_unlock_irqrestore(&sh_dmae_lock, flags);

	if (dmaor & (DMAOR_AE | DMAOR_NMIF)) {
		dev_warn(shdev->common.dev, "Can't initialize DMAOR.\n");
		return -EIO;
	}
	if (shdev->pdata->dmaor_init & ~dmaor)
		dev_warn(shdev->common.dev,
			 "DMAOR=0x%x hasn't latched the initial value 0x%x.\n",
			 dmaor, shdev->pdata->dmaor_init);
	return 0;
}

static bool dmae_is_busy(struct sh_dmae_chan *sh_chan)
{
	u32 chcr = chcr_read(sh_chan);

	if ((chcr & (CHCR_DE | CHCR_TE)) == CHCR_DE)
		return true; /* working */

	return false; /* waiting */
}

static unsigned int calc_xmit_shift(struct sh_dmae_chan *sh_chan, u32 chcr)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_chan);
	struct sh_dmae_pdata *pdata = shdev->pdata;
	int cnt = ((chcr & pdata->ts_low_mask) >> pdata->ts_low_shift) |
		((chcr & pdata->ts_high_mask) >> pdata->ts_high_shift);

	if (cnt >= pdata->ts_shift_num)
		cnt = 0;

	return pdata->ts_shift[cnt];
}

static u32 log2size_to_chcr(struct sh_dmae_chan *sh_chan, int l2size)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_chan);
	struct sh_dmae_pdata *pdata = shdev->pdata;
	int i;

	for (i = 0; i < pdata->ts_shift_num; i++)
		if (pdata->ts_shift[i] == l2size)
			break;

	if (i == pdata->ts_shift_num)
		i = 0;

	return ((i << pdata->ts_low_shift) & pdata->ts_low_mask) |
		((i << pdata->ts_high_shift) & pdata->ts_high_mask);
}

static void dmae_set_reg(struct sh_dmae_chan *sh_chan, struct sh_dmae_regs *hw)
{
	sh_dmae_writel(sh_chan, hw->sar, SAR);
	sh_dmae_writel(sh_chan, hw->dar, DAR);
	sh_dmae_writel(sh_chan, hw->tcr >> sh_chan->xmit_shift, TCR);
}

static void dmae_start(struct sh_dmae_chan *sh_chan)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_chan);

	u32 chcr = chcr_read(sh_chan);

	if (shdev->pdata->needs_tend_set)
		sh_dmae_writel(sh_chan, 0xFFFFFFFF, TEND);

	chcr |= CHCR_DE | shdev->chcr_ie_bit;

	chcr_write(sh_chan, chcr & ~CHCR_TE);
}

static void dmae_halt(struct sh_dmae_chan *sh_chan)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_chan);
	u32 chcr = chcr_read(sh_chan);

	chcr &= ~(CHCR_DE | CHCR_TE | shdev->chcr_ie_bit);
	chcr_write(sh_chan, chcr);

}

static void dmae_init(struct sh_dmae_chan *sh_chan)
{
	/*
	 * Default configuration for dual address memory-memory transfer.
	 * 0x400 represents auto-request.
	 */
	u32 chcr = DM_INC | SM_INC | 0x400 | log2size_to_chcr(sh_chan,
						   LOG2_DEFAULT_XFER_SIZE);
#ifdef CONFIG_ARCH_SHMOBILE
	chcr |= CHCR_CAIE;
#endif
	sh_chan->xmit_shift = calc_xmit_shift(sh_chan, chcr);
	chcr_write(sh_chan, chcr);
}

static int dmae_set_chcr(struct sh_dmae_chan *sh_chan, u32 val)
{
	/* If DMA is active, cannot set CHCR. TODO: remove this superfluous
	 * check */
	if (dmae_is_busy(sh_chan))
		return -EBUSY;

	sh_chan->xmit_shift = calc_xmit_shift(sh_chan, val);
#ifdef CONFIG_ARCH_SHMOBILE
	val |= CHCR_CAIE;
#endif
	chcr_write(sh_chan, val);

	return 0;
}

static int dmae_set_dmars(struct sh_dmae_chan *sh_chan, u16 val)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_chan);
	struct sh_dmae_pdata *pdata = shdev->pdata;
	const struct sh_dmae_channel *chan_pdata = &pdata->channel[sh_chan->id];
	u16 __iomem *addr = shdev->dmars;
	unsigned int shift = chan_pdata->dmars_bit;


	if (dmae_is_busy(sh_chan))
		return -EBUSY;

	if (pdata->no_dmars)
		return 0;

	/* in the case of a missing DMARS resource use first memory window */
	if (!addr)
		addr = (u16 __iomem *)shdev->chan_reg;
	addr += chan_pdata->dmars / sizeof(u16);

	__raw_writew((__raw_readw(addr) & (0xff00 >> shift)) | (val << shift),
		     addr);

	return 0;
}

int sh_dmae_set_rpt_mode(struct dma_chan *chan)
{
	struct sh_dmae_chan *sh_chan = to_sh_chan(chan);
	struct sh_dmae_device *shdev = to_sh_dev(sh_chan);
	unsigned long flags;

	spin_lock_irqsave(&sh_chan->desc_lock, flags);
	/* Check if dma is in progress, this mode is not allowed
	 * once DMA transfer is initiated */
	if (dmae_is_busy(sh_chan)) {
		spin_unlock_irqrestore(&sh_chan->desc_lock, flags);
		dev_err(sh_chan->dev, "DMA is active, command not allowed\n");
		return -ENXIO;
	}
	sh_chan->desc_mode = 1;

	sh_chan->desc_mem = shdev->desc_mem + DPBASE_SHIFT * sh_chan->id;
	sh_chan->desc_pmem = shdev->desc_pmem + DPBASE_SHIFT * sh_chan->id;
	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);

	return 0;
}
EXPORT_SYMBOL(sh_dmae_set_rpt_mode);

int sh_dmae_clear_rpt_mode(struct dma_chan *chan)
{
	struct sh_dmae_chan *sh_chan = to_sh_chan(chan);
	unsigned long flags;
	u32 chcr = 0;

	spin_lock_irqsave(&sh_chan->desc_lock, flags);
	/* Revert to descriptor normal mode */
	chcr = chcr_read(sh_chan);
	chcr = (chcr & ~CHCR_DPM_MASK) | CHCR_DPM_DNM;
	chcr_write(sh_chan, chcr);

	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);

	return 0;
}
EXPORT_SYMBOL(sh_dmae_clear_rpt_mode);

void sh_dmae_aquire_desc_config(struct dma_chan *chan, struct sh_dmae_regs *hw)
{
	struct sh_dmae_chan *sh_chan = to_sh_chan(chan);
	unsigned long flags;

	if (!hw)
		return;
	spin_lock_irqsave(&sh_chan->desc_lock, flags);

	hw->sar = sh_dmae_readl(sh_chan, SAR);
	hw->dar = sh_dmae_readl(sh_chan, DAR);
	hw->tcr = sh_dmae_readl(sh_chan, TCR);

	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);
}
EXPORT_SYMBOL(sh_dmae_aquire_desc_config);

static void dmae_set_desc_mem(struct sh_dmae_chan *sh_chan,
	struct sh_dmae_regs *hw, int fdesc)
{
	u32 __iomem *desc_mem = sh_chan->desc_mem;

	iowrite32(hw->sar, desc_mem);
	desc_mem++;
	iowrite32(hw->dar, desc_mem);
	desc_mem++;
	iowrite32(hw->tcr >> sh_chan->xmit_shift, desc_mem);
	desc_mem++;
	desc_mem++; /* Reserved Byte */

	if (fdesc) {
		/* Write the first configuration to DMACH registers */
		sh_dmae_writel(sh_chan, hw->sar, SAR);
		sh_dmae_writel(sh_chan, hw->dar, DAR);
		sh_dmae_writel(sh_chan, hw->tcr >> sh_chan->xmit_shift, TCR);
	}
	sh_chan->desc_mem = desc_mem;
}

static void dmae_rpt_start(struct sh_dmae_chan *sh_chan)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_chan);
	u32 chcr = chcr_read(sh_chan);

	/* Start descriptor repeat mode with DPB = 1 */
	chcr |= CHCR_DE | (shdev->chcr_ie_bit) | CHCR_DSIE | CHCR_DPB;
	chcr_write(sh_chan, chcr);
}

static void dmae_rpt_halt(struct sh_dmae_chan *sh_chan)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_chan);
	u32 chcr = chcr_read(sh_chan);

	chcr &= ~(CHCR_DE | CHCR_TE | (shdev->chcr_ie_bit)
			  | CHCR_DSIE | CHCR_DSE);
	chcr_write(sh_chan, chcr);

}

static int dmae_rpt_init_reg(struct sh_dmae_chan *sh_chan,
	struct sh_dmae_slave *param)
{
	int i, load_first_desc = 1;
	u32 val = 0;
	u32 chcr, chcrb;
	void __iomem *desc_mem;
	size_t copy_size;
	struct scatterlist *sg;
	struct sh_dmae_regs hw;

	if (param) {
		const struct sh_dmae_slave_config *cfg = param->config;
		if (0 != dmae_set_dmars(sh_chan, cfg->mid_rid))
			goto error;
		sh_chan->chcr |= cfg->chcr;
		if (0 != dmae_set_chcr(sh_chan, sh_chan->chcr ? : cfg->chcr))
			goto error;
	} else {
		dmae_init(sh_chan);
	}

	val |= (sh_chan->desc_pmem & DPBASE_BASE);
	sh_dmae_writel(sh_chan, val, DPBASE);

	/* Reset the descriptor */
	chcrb = sh_dmae_readl(sh_chan, CHCRB) | CHCRB_DRST;
	sh_dmae_writel(sh_chan, chcrb, CHCRB);

	/* Update the descriptor count DCNT */
	chcrb &= ~CHCRB_DCNT_MASK;
	chcrb |= (sh_chan->no_of_descs - 1) << CHCRB_DCNT_SHIFT;
	sh_dmae_writel(sh_chan, chcrb, CHCRB);

	/* Update the RPT */
	chcr = chcr_read(sh_chan) | CHCR_RPT;
	chcr_write(sh_chan, chcr);

	/* Enable the descriptor repeat mode */
	chcr = (chcr & ~CHCR_DPM_MASK) | CHCR_DPM_DRM;
	chcr_write(sh_chan, chcr);

	/* Write the first configuration to DMACH registers */
	desc_mem = sh_chan->desc_mem;
	for_each_sg(sh_chan->sgl, sg, sh_chan->sg_len, i) {
		dma_addr_t sg_addr = sg_dma_address(sg);
		size_t len = sg_dma_len(sg);
		if (!len)
			printk(KERN_ERR "Error: sg_dma_len==0!\n");

		do {
			copy_size = min(len, (size_t)SH_DMA_TCR_MAX + 1);
			hw.tcr = copy_size;
			if (sh_chan->direction == DMA_DEV_TO_MEM) {
				hw.sar = sh_chan->addr;
				hw.dar = sg_addr;
			} else {
				hw.sar = sg_addr;
				hw.dar = sh_chan->addr;
			}

			dmae_set_desc_mem(sh_chan, &hw, load_first_desc);
			load_first_desc = 0;

			dev_dbg(sh_chan->dev, "Add SG #%d@%p[%d], dma %llx\n",
			    i, sg, copy_size, (unsigned long long)sg_addr);

			len -= copy_size;
			sg_addr += copy_size;
		} while (len);
	}

	/* Restore descriptor memory base */
	sh_chan->desc_mem = desc_mem;

	/* Updating the channel control register */
	sh_chan->chcr |= chcr;

	return 0;

error:
	return -1;
}

static dma_cookie_t sh_dmae_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct sh_desc *desc = tx_to_sh_desc(tx), *chunk, *last = desc, *c;
	struct sh_dmae_chan *sh_chan = to_sh_chan(tx->chan);
	struct sh_dmae_slave *param = tx->chan->private;
	dma_async_tx_callback callback = tx->callback;
	dma_cookie_t cookie = -EINVAL;
	unsigned long flags = 0;
	bool power_up = false;

	spin_lock_irqsave(&sh_chan->desc_lock, flags);

	if (list_empty(&sh_chan->ld_queue))
		power_up = true;
	else
		power_up = false;

	cookie = dma_cookie_assign(tx);
	if (power_up) {

		dev_dbg(sh_chan->dev, "Bring up channel %d\n", sh_chan->id);

		if ((param) && (sh_chan->desc_mode)) {
			if (0 != dmae_rpt_init_reg(sh_chan, param)) {
				cookie = -EINVAL;
				goto error;
			}
		}

		if (param) {
			const struct sh_dmae_slave_config *cfg = param->config;

			if (0 != dmae_set_dmars(sh_chan, cfg->mid_rid)) {
				cookie = -EINVAL;
				goto error;
			}
			if (0 != dmae_set_chcr(sh_chan,
						sh_chan->chcr ? : cfg->chcr)) {
				cookie = -EINVAL;
				goto error;
			}
		} else {
			dmae_init(sh_chan);
		}
	}
	if (sh_chan->desc_mode) {
		desc->mark = DESC_SUBMITTED;
		desc->cookie = cookie;
		/* Move the only descriptor to the queue */
		list_add_tail(&desc->node, &sh_chan->ld_queue);
		spin_unlock_irqrestore(&sh_chan->desc_lock, flags);
		return cookie;
	}

	/* Mark all chunks of this descriptor as submitted,move to the queue */
	list_for_each_entry_safe(chunk, c, desc->node.prev, node) {
		/*
		 * All chunks are on the global ld_free, so, we have to find
		 * the end of the chain ourselves
		 */
		if (chunk != desc && (chunk->mark == DESC_IDLE ||
				      chunk->async_tx.cookie > 0 ||
				      chunk->async_tx.cookie == -EBUSY ||
				      &chunk->node == &sh_chan->ld_free))
			break;
		chunk->mark = DESC_SUBMITTED;
		/* Callback goes to the last chunk */
		chunk->async_tx.callback = NULL;
		chunk->cookie = cookie;
		list_move_tail(&chunk->node, &sh_chan->ld_queue);
		last = chunk;
	}

	last->async_tx.callback = callback;
	last->async_tx.callback_param = tx->callback_param;

error:
	dev_dbg(sh_chan->dev, "submit #%d@%p on %d: %x[%d] -> %x\n",
		tx->cookie, &last->async_tx, sh_chan->id,
		desc->hw.sar, desc->hw.tcr, desc->hw.dar);

	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);

	return cookie;
}

/* Called with desc_lock held */
static struct sh_desc *sh_dmae_get_desc(struct sh_dmae_chan *sh_chan)
{
	struct sh_desc *desc;

	list_for_each_entry(desc, &sh_chan->ld_free, node)
		if (desc->mark != DESC_PREPARED) {
			BUG_ON(desc->mark != DESC_IDLE);
			list_del(&desc->node);
			return desc;
		}

	return NULL;
}

static const struct sh_dmae_slave_config *sh_dmae_find_slave(
	struct sh_dmae_chan *sh_chan, struct sh_dmae_slave *param)
{
	struct sh_dmae_device *shdev = to_sh_dev(sh_chan);
	struct sh_dmae_pdata *pdata = shdev->pdata;
	int i;

	if (param->slave_id >= SH_DMA_SLAVE_NUMBER)
		return NULL;

	for (i = 0; i < pdata->slave_num; i++)
		if (pdata->slave[i].slave_id == param->slave_id)
			return pdata->slave + i;

	return NULL;
}

static int sh_dmae_alloc_chan_resources(struct dma_chan *chan)
{
	struct sh_dmae_chan *sh_chan = to_sh_chan(chan);
	struct sh_desc *desc;
	struct sh_dmae_slave *param = chan->private;
	int ret;
	pm_runtime_get_sync(sh_chan->dev);
	/*
	 * This relies on the guarantee from dmaengine that
	 * alloc_chan_resources never runs concurrently with itself
	 * or free_chan_resources.
	 */
	if (param) {
		const struct sh_dmae_slave_config *cfg;

		cfg = sh_dmae_find_slave(sh_chan, param);
		if (!cfg) {
			pm_runtime_put_sync(sh_chan->dev);
			ret = -EINVAL;
			goto efindslave;
		}

		if (test_and_set_bit(param->slave_id, sh_dmae_slave_used)) {
			pm_runtime_put_sync(sh_chan->dev);
			ret = -EBUSY;
			goto etestused;
		}

		param->config = cfg;
	}

	while (sh_chan->descs_allocated < NR_DESCS_PER_CHANNEL) {
		desc = kzalloc(sizeof(struct sh_desc), GFP_KERNEL);
		if (!desc)
			break;
		dma_async_tx_descriptor_init(&desc->async_tx,
					&sh_chan->common);
		desc->async_tx.tx_submit = sh_dmae_tx_submit;
		desc->mark = DESC_IDLE;

		list_add(&desc->node, &sh_chan->ld_free);
		sh_chan->descs_allocated++;
	}

	if (!sh_chan->descs_allocated) {
		pm_runtime_put_sync(sh_chan->dev);
		ret = -ENOMEM;
		goto edescalloc;
	}
	/* Reset descripotr repeat mode flag */
	sh_chan->desc_mode = 0;

	return sh_chan->descs_allocated;

edescalloc:
	if (param)
		clear_bit(param->slave_id, sh_dmae_slave_used);
etestused:
efindslave:
	chan->private = NULL;
	return ret;
}

/*
 * sh_dma_free_chan_resources - Free all resources of the channel.
 */
static void sh_dmae_free_chan_resources(struct dma_chan *chan)
{
	struct sh_dmae_chan *sh_chan = to_sh_chan(chan);
	struct sh_desc *desc, *_desc;
	LIST_HEAD(list);
	unsigned long flags;

	/* Protect against ISR */
	spin_lock_irqsave(&sh_chan->desc_lock, flags);
	if (!sh_chan->desc_mode)
		dmae_halt(sh_chan);
	else
		dmae_rpt_halt(sh_chan);
	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);

	/* Now no new interrupts will occur */

	/* Prepared and not submitted descriptors can still be on the queue */
	if (!list_empty(&sh_chan->ld_queue))
		sh_dmae_chan_ld_cleanup(sh_chan, true);

	if (chan->private) {
		/* The caller is holding dma_list_mutex */
		struct sh_dmae_slave *param = chan->private;
		clear_bit(param->slave_id, sh_dmae_slave_used);
		chan->private = NULL;
	}

	spin_lock_irqsave(&sh_chan->desc_lock, flags);

	list_splice_init(&sh_chan->ld_free, &list);
	sh_chan->descs_allocated = 0;
	sh_chan->chcr = 0;
	sh_chan->desc_mode = 0;

	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);

	pm_runtime_put_sync(sh_chan->dev);
	list_for_each_entry_safe(desc, _desc, &list, node)
		kfree(desc);
}

/**
 * sh_dmae_add_desc - get, set up and return one transfer descriptor
 * @sh_chan:	DMA channel
 * @flags:	DMA transfer flags
 * @dest:	destination DMA address, incremented when direction equals
 *		DMA_DEV_TO_MEM
 * @src:	source DMA address, incremented when direction equals
 *		DMA_MEM_TO_DEV
 * @len:	DMA transfer length
 * @first:	if NULL, set to the current descriptor and cookie set to -EBUSY
 * @direction:	needed for slave DMA to decide which address to keep constant,
 *		equals DMA_MEM_TO_MEM for MEMCPY
 * Returns 0 or an error
 * Locks: called with desc_lock held
 */
static struct sh_desc *sh_dmae_add_desc(struct sh_dmae_chan *sh_chan,
	unsigned long flags, dma_addr_t *dest, dma_addr_t *src, size_t *len,
	struct sh_desc **first, enum dma_transfer_direction direction)
{
	struct sh_desc *new;
	size_t copy_size;

	if (!*len)
		return NULL;

	/* Allocate the link descriptor from the free list */
	new = sh_dmae_get_desc(sh_chan);
	if (!new) {
		dev_err(sh_chan->dev, "No free link descriptor available\n");
		return NULL;
	}

	copy_size = min(*len, (size_t)SH_DMA_TCR_MAX + 1);

	new->hw.sar = *src;
	new->hw.dar = *dest;
	new->hw.tcr = copy_size;

	if (!*first) {
		/* First desc */
		new->async_tx.cookie = -EBUSY;
		*first = new;
	} else {
		/* Other desc - invisible to the user */
		new->async_tx.cookie = -EINVAL;
	}

	dev_dbg(sh_chan->dev,
		"chaining (%u/%u)@%x -> %x with %p, cookie %d, shift %d\n",
		copy_size, *len, *src, *dest, &new->async_tx,
		new->async_tx.cookie, sh_chan->xmit_shift);

	new->mark = DESC_PREPARED;
	new->async_tx.flags = flags;
	new->direction = direction;

	*len -= copy_size;
	if (direction == DMA_MEM_TO_MEM || direction == DMA_MEM_TO_DEV)
		*src += copy_size;
	if (direction == DMA_MEM_TO_MEM || direction == DMA_DEV_TO_MEM)
		*dest += copy_size;

	return new;
}

/*
 * sh_dmae_rpt_prep_sg - prepare transfer descriptors from an SG list
 * for repeat mode.
 */
static struct dma_async_tx_descriptor *sh_dmae_rpt_prep_sg(
	struct sh_dmae_chan *sh_chan,
	struct scatterlist *sgl, unsigned int sg_len, dma_addr_t *addr,
	enum dma_transfer_direction direction, unsigned long flags)
{
	struct scatterlist *sg = NULL;
	struct sh_desc *first = NULL;
	int i = 0;
	int chunks = 0;
	unsigned long irq_flags;

	if (!sh_chan->desc_mode) {
		dev_err(sh_chan->dev, "not in repeat mode\n");
		return NULL;
	}
	if (!sg_len) {
		dev_err(sh_chan->dev, "sg_len = 0, returning\n");
		return NULL;
	}

	for_each_sg(sgl, sg, sg_len, i)
		chunks += (sg_dma_len(sg) + SH_DMA_TCR_MAX) /
			(SH_DMA_TCR_MAX + 1);

	if (chunks > NR_RPT_HW_DESCS_PER_CH) {
		/* As the DescriptorMEM is shared by all channels,
		 * only 6 descriptors are reserved per channel */
		dev_err(sh_chan->dev,
			"Not enough descs available, needed = %d\n", chunks);
		return NULL;
	}
	sh_chan->no_of_descs = chunks;

	spin_lock_irqsave(&sh_chan->desc_lock, irq_flags);
	/*
	 * In repeat only one software descriptor is used to maintain
	 * the interface consistency. The HW descriptor memory is filled
	 * with the sglist.
	 */

	first = sh_dmae_get_desc(sh_chan);
	if (!first) {
		dev_err(sh_chan->dev,
			"No free link descriptor available for rpt mode\n");
		goto err_get_desc;
	}
	first->mark = DESC_PREPARED;
	first->async_tx.cookie = -EBUSY;
	first->async_tx.flags = flags;
	first->direction = direction;
	first->chunks = chunks;

	sh_chan->sgl = sgl;
	sh_chan->sg_len = sg_len;
	sh_chan->addr = *addr;
	sh_chan->direction = direction;

	spin_unlock_irqrestore(&sh_chan->desc_lock, irq_flags);

	dev_dbg(sh_chan->dev, "&first->async_tx = 0x%p\n", &first->async_tx);
	return &first->async_tx;

err_get_desc:
	spin_unlock_irqrestore(&sh_chan->desc_lock, irq_flags);
	return NULL;
}

/*
 * sh_dmae_prep_sg - prepare transfer descriptors from an SG list
 *
 * Common routine for public (MEMCPY) and slave DMA. The MEMCPY case is also
 * converted to scatter-gather to guarantee consistent locking and a correct
 * list manipulation. For slave DMA direction carries the usual meaning, and,
 * logically, the SG list is RAM and the addr variable contains slave address,
 * e.g., the FIFO I/O register. For MEMCPY direction equals DMA_MEM_TO_MEM
 * and the SG list contains only one element and points at the source buffer.
 */
static struct dma_async_tx_descriptor *sh_dmae_prep_sg(
	struct sh_dmae_chan *sh_chan,
	struct scatterlist *sgl, unsigned int sg_len, dma_addr_t *addr,
	enum dma_transfer_direction direction, unsigned long flags)
{
	struct scatterlist *sg;
	struct sh_desc *first = NULL, *new = NULL /* compiler... */;
	LIST_HEAD(tx_list);
	int chunks = 0;
	unsigned long irq_flags;
	int i;

	if (!sg_len)
		return NULL;

	for_each_sg(sgl, sg, sg_len, i)
		chunks += (sg_dma_len(sg) + SH_DMA_TCR_MAX) /
			(SH_DMA_TCR_MAX + 1);

	/* Have to lock the whole loop to protect against concurrent release */
	spin_lock_irqsave(&sh_chan->desc_lock, irq_flags);

	/*
	 * Chaining:
	 * first descriptor is what user is dealing with in all API calls, its
	 *	cookie is at first set to -EBUSY, at tx-submit to a positive
	 *	number
	 * if more than one chunk is needed further chunks have cookie=-EINVAL
	 * the last chunk, if not equal to the first, has cookie = -ENOSPC
	 * all chunks are linked onto the tx_list head with their .node heads
	 *	only during this function, then they are immediately spliced
	 *	back onto the free list in form of a chain
	 */
	for_each_sg(sgl, sg, sg_len, i) {
		dma_addr_t sg_addr = sg_dma_address(sg);
		size_t len = sg_dma_len(sg);

		if (!len)
			goto err_get_desc;

		do {
			dev_dbg(sh_chan->dev, "Add SG #%d@%p[%d], dma %llx\n",
				i, sg, len, (unsigned long long)sg_addr);

			if (direction == DMA_DEV_TO_MEM)
				new = sh_dmae_add_desc(sh_chan, flags,
						&sg_addr, addr, &len, &first,
						direction);
			else
				new = sh_dmae_add_desc(sh_chan, flags,
						addr, &sg_addr, &len, &first,
						direction);
			if (!new)
				goto err_get_desc;

			new->chunks = chunks--;
			list_add_tail(&new->node, &tx_list);
		} while (len);
	}

	if (new != first)
		new->async_tx.cookie = -ENOSPC;

	/* Put them back on the free list, so, they don't get lost */
	list_splice_tail(&tx_list, &sh_chan->ld_free);

	spin_unlock_irqrestore(&sh_chan->desc_lock, irq_flags);

	return &first->async_tx;

err_get_desc:
	list_for_each_entry(new, &tx_list, node)
		new->mark = DESC_IDLE;
	list_splice(&tx_list, &sh_chan->ld_free);

	spin_unlock_irqrestore(&sh_chan->desc_lock, irq_flags);

	return NULL;
}

static struct dma_async_tx_descriptor *sh_dmae_prep_memcpy(
	struct dma_chan *chan, dma_addr_t dma_dest, dma_addr_t dma_src,
	size_t len, unsigned long flags)
{
	struct sh_dmae_chan *sh_chan;
	struct scatterlist sg;

	if (!chan || !len)
		return NULL;

	sh_chan = to_sh_chan(chan);

	sg_init_table(&sg, 1);
	sg_set_page(&sg, pfn_to_page(PFN_DOWN(dma_src)), len,
		    offset_in_page(dma_src));
	sg_dma_address(&sg) = dma_src;
	sg_dma_len(&sg) = len;

	return sh_dmae_prep_sg(sh_chan, &sg, 1, &dma_dest, DMA_MEM_TO_MEM,
			       flags);
}

static struct dma_async_tx_descriptor *sh_dmae_prep_slave_sg(
	struct dma_chan *chan, struct scatterlist *sgl, unsigned int sg_len,
	enum dma_transfer_direction direction, unsigned long flags,
	void *context)
{
	struct sh_dmae_slave *param;
	struct sh_dmae_chan *sh_chan;
	dma_addr_t slave_addr;

	if (!chan)
		return NULL;

	sh_chan = to_sh_chan(chan);
	param = chan->private;

	/* Someone calling slave DMA on a public channel? */
	if (!param || !sg_len) {
		dev_warn(sh_chan->dev, "%s: bad parameter: %p, %d, %d\n",
		 __func__, param, sg_len, param ? param->slave_id : -1);
		return NULL;
	}

	slave_addr = param->config->addr;

	/*
	 * if (param != NULL), this is a successfully requested slave channel,
	 * therefore param->config != NULL too.
	 */

	 if (!sh_chan->desc_mode)
		return sh_dmae_prep_sg(sh_chan, sgl, sg_len, &slave_addr,
			       direction, flags);
	 else
		return sh_dmae_rpt_prep_sg(sh_chan, sgl, sg_len, &slave_addr,
			       direction, flags);
}

static int dma_set_runtime_config(struct dma_chan *chan,
				  struct dma_slave_config *config)
{
	struct sh_dmae_chan *sh_chan = to_sh_chan(chan);
	struct sh_dmae_device *shdev = to_sh_dev(sh_chan);
	struct sh_dmae_pdata *pdata = shdev->pdata;
	struct sh_dmae_slave *param = sh_chan->common.private;
	const struct sh_dmae_slave_config *cfg;
	enum dma_slave_buswidth addr_width;
	u32 maxburst;
	u32 chcr;
	int l2size;

	if (!config || !param)
		return -EINVAL;

	/* Transfer direction */
	if (config->direction == DMA_MEM_TO_DEV) {
		addr_width = config->dst_addr_width;
		maxburst = config->dst_maxburst;
	} else if (config->direction == DMA_DEV_TO_MEM) {
		addr_width = config->src_addr_width;
		maxburst = config->src_maxburst;
	} else {
		dev_err(sh_chan->dev,
			"bad runtime_config: alien transfer direction\n");
		return -EINVAL;
	}

	switch (addr_width) {
	case DMA_SLAVE_BUSWIDTH_1_BYTE:
	case DMA_SLAVE_BUSWIDTH_2_BYTES:
	case DMA_SLAVE_BUSWIDTH_4_BYTES:
	case DMA_SLAVE_BUSWIDTH_8_BYTES:
		break;
	default:
		dev_err(sh_chan->dev,
			"bad runtime_config: alien address width\n");
		return -EINVAL;
	}

	/* Select one element if no maxburst is specified */
	if (maxburst == 0)
		maxburst = 1;

	/*
	 * Make sure that the burst_size passed in is available on this
	 * channel.  If it's not, leave current CHCR setting unchanged.
	 */
	l2size = __fls(addr_width * maxburst);
	if (((1 << l2size) & param->config->burst_sizes) == 0) {
		dev_err(sh_chan->dev,
			"bad runtime_config: illegal burst_size, rejected\n");
		return -EINVAL;
	}

	cfg = param->config;
	chcr = cfg->chcr;
	chcr &= ~(pdata->ts_high_mask | pdata->ts_low_mask);
	chcr |= log2size_to_chcr(sh_chan, l2size);
	sh_chan->chcr = chcr;

	dev_dbg(sh_chan->dev,
		"configured channel %s for %s, data width %d, "
		"maxburst %d words (burst_size %d bytes), CHCR=0x%08x\n",
		dma_chan_name(chan),
		(config->direction == DMA_DEV_TO_MEM) ? "RX" : "TX",
		addr_width, maxburst, 1 << l2size, chcr);

	return 0;
}

static int sh_dmae_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
			   unsigned long arg)
{
	struct sh_dmae_chan *sh_chan = to_sh_chan(chan);
	unsigned long flags;

	if (!chan)
		return -EINVAL;

	/* Controls applicable to inactive channels */
	if (cmd == DMA_SLAVE_CONFIG)
		return dma_set_runtime_config(chan,
					      (struct dma_slave_config *)arg);

	/* Only supports DMA_TERMINATE_ALL */
	if (cmd != DMA_TERMINATE_ALL)
		return -ENXIO;

	spin_lock_irqsave(&sh_chan->desc_lock, flags);
	if (!sh_chan->desc_mode) {
		dmae_halt(sh_chan);
	} else {
		dmae_rpt_halt(sh_chan);
		spin_unlock_irqrestore(&sh_chan->desc_lock, flags);
		return 0;
	}

	if (!list_empty(&sh_chan->ld_queue)) {
		/* Record partial transfer */
		struct sh_desc *desc = list_entry(sh_chan->ld_queue.next,
						  struct sh_desc, node);
		desc->partial = (desc->hw.tcr - sh_dmae_readl(sh_chan, TCR)) <<
			sh_chan->xmit_shift;
	}
	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);

	sh_dmae_chan_ld_cleanup(sh_chan, true);

	return 0;
}

static dma_async_tx_callback __ld_cleanup(struct sh_dmae_chan *sh_chan,
	bool all)
{
	struct sh_desc *desc, *_desc;
	/* Is the "exposed" head of a chain acked? */
	bool head_acked = false;
	dma_cookie_t cookie = 0;
	dma_async_tx_callback callback = NULL;
	void *param = NULL;
	unsigned long flags;
	
	memory_log_dump_int(DMA_FUNC_ID_LD_CLEANUP, DMA_START);
	spin_lock_irqsave(&sh_chan->desc_lock, flags);
	list_for_each_entry_safe(desc, _desc, &sh_chan->ld_queue, node) {
		struct dma_async_tx_descriptor *tx = &desc->async_tx;

		BUG_ON(tx->cookie > 0 && tx->cookie != desc->cookie);
		BUG_ON(desc->mark != DESC_SUBMITTED &&
		       desc->mark != DESC_COMPLETED &&
		       desc->mark != DESC_WAITING);

		/*
		 * queue is ordered, and we use this loop to (1) clean up all
		 * completed descriptors, and to (2) update descriptor flags of
		 * any chunks in a (partially) completed chain
		 */
		if (!all && desc->mark == DESC_SUBMITTED &&
		    desc->cookie != cookie)
			break;

		if (tx->cookie > 0)
			cookie = tx->cookie;

		if (desc->mark == DESC_COMPLETED && desc->chunks == 1) {
			if (sh_chan->common.completed_cookie != desc->cookie - 1)
				dev_dbg(sh_chan->dev,
					"Completing cookie %d, expected %d\n",
					desc->cookie,
					sh_chan->common.completed_cookie + 1);
			sh_chan->common.completed_cookie = desc->cookie;
		}

		/* Call callback on the last chunk */
		if (desc->mark == DESC_COMPLETED && tx->callback) {
			desc->mark = DESC_WAITING;
			callback = tx->callback;
			param = tx->callback_param;
			dev_dbg(sh_chan->dev,
				"descriptor #%d@%p on %d callback\n",
				tx->cookie, tx, sh_chan->id);
			BUG_ON(desc->chunks != 1);
			break;
		}

		if (tx->cookie > 0 || tx->cookie == -EBUSY) {
			if (desc->mark == DESC_COMPLETED) {
				BUG_ON(tx->cookie < 0);
				desc->mark = DESC_WAITING;
			}
			head_acked = async_tx_test_ack(tx);
		} else {
			switch (desc->mark) {
			case DESC_COMPLETED:
				desc->mark = DESC_WAITING;
				/* Fall through */
			case DESC_WAITING:
				if (head_acked)
					async_tx_ack(&desc->async_tx);
			}
		}

		dev_dbg(sh_chan->dev, "descriptor %p #%d completed.\n",
			tx, tx->cookie);

		if (((desc->mark == DESC_COMPLETED ||
		      desc->mark == DESC_WAITING) &&
		     async_tx_test_ack(&desc->async_tx)) || all) {
			/* Remove from ld_queue list */
			desc->mark = DESC_IDLE;

			list_move(&desc->node, &sh_chan->ld_free);

		}
	}

	if (all && !callback)
		/*
		 * Terminating and the loop completed normally: forgive
		 * uncompleted cookies
		 */
		sh_chan->common.completed_cookie = sh_chan->common.cookie;

	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);
	memory_log_dump_int(DMA_FUNC_ID_LD_CLEANUP, DMA_END);
	if (callback)
		callback(param);

	return callback;
}

/*
 * sh_chan_ld_cleanup - Clean up link descriptors
 *
 * This function cleans up the ld_queue of DMA channel.
 */
static void sh_dmae_chan_ld_cleanup(struct sh_dmae_chan *sh_chan, bool all)
{
	while (__ld_cleanup(sh_chan, all))
		;
}

/* Called under spin_lock_irqsave(&sh_chan->desc_lock) */
static void sh_chan_xfer_ld_queue(struct sh_dmae_chan *sh_chan)
{
	struct sh_desc *desc;

	/* DMA work check */
	if (dmae_is_busy(sh_chan))
		return;

	/* Find the first not transferred descriptor */
	list_for_each_entry(desc, &sh_chan->ld_queue, node)
		if (desc->mark == DESC_SUBMITTED) {
			dev_dbg(sh_chan->dev, "Queue #%d to %d: %u@%x -> %x\n",
				desc->async_tx.cookie, sh_chan->id,
				desc->hw.tcr, desc->hw.sar, desc->hw.dar);
			/* Get the ld start address from ld_queue */
			memory_log_dump_int(DMA_FUNC_LD_QUEUE,DMA_START);
			dmae_set_reg(sh_chan, &desc->hw);
			dmae_start(sh_chan);
			memory_log_dump_int(DMA_FUNC_LD_QUEUE,DMA_END);
			break;
		}
}

static void sh_dmae_memcpy_issue_pending(struct dma_chan *chan)
{
	struct sh_dmae_chan *sh_chan = to_sh_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&sh_chan->desc_lock, flags);
	memory_log_dump_int(DMA_FUNC_ID_PENDING, DMA_START);
	if (!sh_chan->desc_mode)
		sh_chan_xfer_ld_queue(sh_chan);
	else
		/* Descriptor memory is already loaded, start the transfer */
		dmae_rpt_start(sh_chan);
	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);
	memory_log_dump_int(DMA_FUNC_ID_PENDING, DMA_END);
}

static enum dma_status sh_dmae_tx_status(struct dma_chan *chan,
					dma_cookie_t cookie,
					struct dma_tx_state *txstate)
{
	struct sh_dmae_chan *sh_chan = to_sh_chan(chan);
	enum dma_status status;
	unsigned long flags;

	if (sh_chan->desc_mode)
		return DMA_SUCCESS;

	sh_dmae_chan_ld_cleanup(sh_chan, false);

	spin_lock_irqsave(&sh_chan->desc_lock, flags);

	status = dma_cookie_status(chan, cookie, txstate);

	/*
	 * If we don't find cookie on the queue, it has been aborted
	 * and we have to report error
	 */
	if (status != DMA_SUCCESS) {
		struct sh_desc *desc;
		status = DMA_ERROR;
		list_for_each_entry(desc, &sh_chan->ld_queue, node)
			if (desc->cookie == cookie) {
				status = DMA_IN_PROGRESS;
				break;
			}
	}

	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);

	return status;
}

static irqreturn_t sh_dmae_interrupt(int irq, void *data)
{
	irqreturn_t ret = IRQ_NONE;
	struct sh_dmae_chan *sh_chan = data;
	u32 chcr;

	spin_lock(&sh_chan->desc_lock);

	chcr = chcr_read(sh_chan);

	if (sh_chan->desc_mode) {
		if ((chcr & CHCR_DSE) || (chcr & CHCR_TE)) {
			chcr &= ~(CHCR_TE | CHCR_DSE);
			chcr_write(sh_chan, chcr);
			ret = IRQ_HANDLED;
			tasklet_schedule(&sh_chan->tasklet);
		}
	} else if (chcr & CHCR_TE) {
		/* DMA stop */
		dmae_halt(sh_chan);

		ret = IRQ_HANDLED;
		tasklet_schedule(&sh_chan->tasklet);
	}
#ifdef CONFIG_ARCH_SHMOBILE
	if (chcr & CHCR_CAE) {
		/* DMA stop */
		dmae_halt(sh_chan);

		ret = IRQ_HANDLED;
		chcr = chcr_read(sh_chan);
		chcr_write(sh_chan, chcr & ~CHCR_CAE);
		sh_chan->addr_error = 1;
		tasklet_schedule(&sh_chan->tasklet);
	}
#endif

	spin_unlock(&sh_chan->desc_lock);

	return ret;
}

/* Called from error IRQ or NMI */
static bool sh_dmae_reset(struct sh_dmae_device *shdev)
{
	unsigned int handled = 0;
	int i;

	/* halt the dma controller */
	sh_dmae_ctl_stop(shdev);

	/* We cannot detect, which channel caused the error,
	 * have to reset all */
	for (i = 0; i < SH_DMAC_MAX_CHANNELS; i++) {
		struct sh_dmae_chan *sh_chan = shdev->chan[i];
		struct sh_desc *desc;
		LIST_HEAD(dl);

		if (!sh_chan)
			continue;

		spin_lock(&sh_chan->desc_lock);

		/* Stop the channel */
		dmae_halt(sh_chan);

		list_splice_init(&sh_chan->ld_queue, &dl);


		spin_unlock(&sh_chan->desc_lock);

		/* Complete all  */
		list_for_each_entry(desc, &dl, node) {
			struct dma_async_tx_descriptor *tx = &desc->async_tx;
			desc->mark = DESC_IDLE;
			if (tx->callback)
				tx->callback(tx->callback_param);
		}

		spin_lock(&sh_chan->desc_lock);
		list_splice(&dl, &sh_chan->ld_free);
		spin_unlock(&sh_chan->desc_lock);

		handled++;
	}

	sh_dmae_rst(shdev);

	return !!handled;
}

#ifdef CONFIG_CPU_SH4
static irqreturn_t sh_dmae_err(int irq, void *data)
{
	struct sh_dmae_device *shdev = data;

	if (!(dmaor_read(shdev) & DMAOR_AE))
		return IRQ_NONE;

	sh_dmae_reset(data);
	return IRQ_HANDLED;
}
#endif

static void dmae_do_tasklet(unsigned long data)
{
	struct sh_dmae_chan *sh_chan = (struct sh_dmae_chan *)data;
	struct sh_desc *desc;
	u32 sar_buf = sh_dmae_readl(sh_chan, SAR);
	u32 dar_buf = sh_dmae_readl(sh_chan, DAR);
	int desc_mode;
	unsigned long flags;

	spin_lock_irqsave(&sh_chan->desc_lock, flags);
	memory_log_dump_int(DMA_FUNC_ID_DO_TASKLET, DMA_START);
	desc_mode = sh_chan->desc_mode;
	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);
	if (desc_mode) {
		/* We just want to get the only descriptor used */
		list_for_each_entry(desc, &sh_chan->ld_queue, node) {
			desc->async_tx.callback(desc->async_tx.callback_param);
			break;
		}
		return;
	}

	spin_lock_irqsave(&sh_chan->desc_lock, flags);

	if (sh_chan->addr_error) {
		dev_err(sh_chan->dev, "Address error source:%08x dest:%08x",
			sar_buf, dar_buf);
		sh_chan->addr_error = 0;
	}
	list_for_each_entry(desc, &sh_chan->ld_queue, node) {
		if (desc->mark == DESC_SUBMITTED &&
		    ((desc->direction == DMA_DEV_TO_MEM &&
		      (desc->hw.dar + desc->hw.tcr) == dar_buf) ||
		     (desc->hw.sar + desc->hw.tcr) == sar_buf)) {
			dev_dbg(sh_chan->dev, "done #%d@%p dst %u\n",
				desc->async_tx.cookie, &desc->async_tx,
				desc->hw.dar);
			desc->mark = DESC_COMPLETED;
			break;
		}
	}

	/* Next desc */
	sh_chan_xfer_ld_queue(sh_chan);

	spin_unlock_irqrestore(&sh_chan->desc_lock, flags);
	memory_log_dump_int(DMA_FUNC_ID_DO_TASKLET, DMA_END);
	sh_dmae_chan_ld_cleanup(sh_chan, false);
}

static bool sh_dmae_nmi_notify(struct sh_dmae_device *shdev)
{
	/* Fast path out if NMIF is not asserted for this controller */
	if ((dmaor_read(shdev) & DMAOR_NMIF) == 0)
		return false;

	return sh_dmae_reset(shdev);
}

static int sh_dmae_nmi_handler(struct notifier_block *self,
			       unsigned long cmd, void *data)
{
	struct sh_dmae_device *shdev;
	int ret = NOTIFY_DONE;
	bool triggered;

	/*
	 * Only concern ourselves with NMI events.
	 *
	 * Normally we would check the die chain value, but as this needs
	 * to be architecture independent, check for NMI context instead.
	 */
	if (!in_nmi())
		return NOTIFY_DONE;

	rcu_read_lock();
	list_for_each_entry_rcu(shdev, &sh_dmae_devices, node) {
		/*
		 * Only stop if one of the controllers has NMIF asserted,
		 * we do not want to interfere with regular address error
		 * handling or NMI events that don't concern the DMACs.
		 */
		triggered = sh_dmae_nmi_notify(shdev);
		if (triggered == true)
			ret = NOTIFY_OK;
	}
	rcu_read_unlock();

	return ret;
}

static struct notifier_block sh_dmae_nmi_notifier __read_mostly = {
	.notifier_call	= sh_dmae_nmi_handler,

	/* Run before NMI debug handler and KGDB */
	.priority	= 1,
};

static int __devinit sh_dmae_chan_probe(struct sh_dmae_device *shdev, int id,
					int irq, unsigned long flags)
{
	int err;
	const struct sh_dmae_channel *chan_pdata = &shdev->pdata->channel[id];
	struct platform_device *pdev = to_platform_device(shdev->common.dev);
	struct sh_dmae_chan *new_sh_chan;

	/* alloc channel */
	new_sh_chan = kzalloc(sizeof(struct sh_dmae_chan), GFP_KERNEL);
	if (!new_sh_chan) {
		dev_err(shdev->common.dev,
			"No free memory for allocating dma channels!\n");
		return -ENOMEM;
	}

	/* reference struct dma_device */
	new_sh_chan->common.device = &shdev->common;
	dma_cookie_init(&new_sh_chan->common);

	new_sh_chan->dev = shdev->common.dev;
	new_sh_chan->id = id;
	new_sh_chan->irq = irq;
	new_sh_chan->base = shdev->chan_reg + chan_pdata->offset / sizeof(u32);

	/* Init DMA tasklet */
	tasklet_init(&new_sh_chan->tasklet, dmae_do_tasklet,
			(unsigned long)new_sh_chan);

	spin_lock_init(&new_sh_chan->desc_lock);

	/* Init descripter manage list */
	INIT_LIST_HEAD(&new_sh_chan->ld_queue);
	INIT_LIST_HEAD(&new_sh_chan->ld_free);

	/* Add the channel to DMA device channel list */
	list_add_tail(&new_sh_chan->common.device_node,
			&shdev->common.channels);
	shdev->common.chancnt++;

	if (pdev->id >= 0)
		snprintf(new_sh_chan->dev_id, sizeof(new_sh_chan->dev_id),
			 "sh-dmae%d.%d", pdev->id, new_sh_chan->id);
	else
		snprintf(new_sh_chan->dev_id, sizeof(new_sh_chan->dev_id),
			 "sh-dma%d", new_sh_chan->id);

	/* set up channel irq */
	err = request_irq(irq, &sh_dmae_interrupt, flags,
			  new_sh_chan->dev_id, new_sh_chan);
	if (err) {
		dev_err(shdev->common.dev, "DMA channel %d request_irq error "
			"with return %d\n", id, err);
		goto err_no_irq;
	}

	shdev->chan[id] = new_sh_chan;
	return 0;

err_no_irq:
	/* remove from dmaengine device node */
	list_del(&new_sh_chan->common.device_node);
	kfree(new_sh_chan);
	return err;
}

static void sh_dmae_chan_remove(struct sh_dmae_device *shdev)
{
	int i;

	for (i = shdev->common.chancnt - 1 ; i >= 0 ; i--) {
		if (shdev->chan[i]) {
			struct sh_dmae_chan *sh_chan = shdev->chan[i];

			free_irq(sh_chan->irq, sh_chan);

			list_del(&sh_chan->common.device_node);
			kfree(sh_chan);
			shdev->chan[i] = NULL;
		}
	}
	shdev->common.chancnt = 0;
}

static int __init sh_dmae_probe(struct platform_device *pdev)
{
	struct sh_dmae_pdata *pdata = pdev->dev.platform_data;
	unsigned long irqflags = IRQF_DISABLED,
		chan_flag[SH_DMAC_MAX_CHANNELS] = {};
	int chan_irq[SH_DMAC_MAX_CHANNELS];
	int err, i, irq_cnt = 0, irqres = 0, irq_cap = 0;
	struct sh_dmae_device *shdev;
	struct resource *chan, *dmars, *desc_mem, *errirq_res, *chanirq_res;

	/* get platform data */
	if (!pdata || !pdata->channel_num)
		return -ENODEV;

	desc_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	chan = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	/* DMARS area is optional */
	dmars = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	/*
	 * IRQ resources:
	 * 1. there always must be at least one IRQ IO-resource. On SH4 it is
	 *    the error IRQ, in which case it is the only IRQ in this resource:
	 *    start == end. If it is the only IRQ resource, all channels also
	 *    use the same IRQ.
	 * 2. DMA channel IRQ resources can be specified one per resource or in
	 *    ranges (start != end)
	 * 3. iff all events (channels and, optionally, error) on this
	 *    controller use the same IRQ, only one IRQ resource can be
	 *    specified, otherwise there must be one IRQ per channel, even if
	 *    some of them are equal
	 * 4. if all IRQs on this controller are equal or if some specific IRQs
	 *    specify IORESOURCE_IRQ_SHAREABLE in their resources, they will be
	 *    requested with the IRQF_SHARED flag
	 */
	errirq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!desc_mem || !chan || !errirq_res)
		return -ENODEV;

	if (!request_mem_region(desc_mem->start,
				resource_size(desc_mem), pdev->name)) {
		dev_err(&pdev->dev,
			"DMAC DescriptorMEM region already claimed\n");
		return -EBUSY;
	}

	if (!request_mem_region(chan->start,
				resource_size(chan), pdev->name)) {
		dev_err(&pdev->dev, "DMAC register region already claimed\n");
		err = -EBUSY;
		goto ermrchan;
	}

	if (dmars && !request_mem_region(dmars->start,
					 resource_size(dmars), pdev->name)) {
		dev_err(&pdev->dev, "DMAC DMARS region already claimed\n");
		err = -EBUSY;
		goto ermrdmars;
	}

	err = -ENOMEM;
	shdev = kzalloc(sizeof(struct sh_dmae_device), GFP_KERNEL);
	if (!shdev) {
		dev_err(&pdev->dev, "Not enough memory\n");
		goto ealloc;
	}

	shdev->desc_mem = ioremap(desc_mem->start, resource_size(desc_mem));
	if (!shdev->desc_mem)
		goto emapdescmem;
	shdev->desc_pmem = desc_mem->start;

	shdev->chan_reg = ioremap(chan->start, resource_size(chan));
	if (!shdev->chan_reg)
		goto emapchan;

	if (dmars) {
		shdev->dmars = ioremap(dmars->start, resource_size(dmars));
		if (!shdev->dmars)
			goto emapdmars;
	}


	/* platform data */
	shdev->pdata = pdata;

	if (pdata->chcr_offset)
		shdev->chcr_offset = pdata->chcr_offset;
	else
		shdev->chcr_offset = CHCR;

	if (pdata->chcr_ie_bit)
		shdev->chcr_ie_bit = pdata->chcr_ie_bit;
	else
		shdev->chcr_ie_bit = CHCR_IE;

	platform_set_drvdata(pdev, shdev);

	shdev->common.dev = &pdev->dev;
	shdev->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(shdev->clk)) {
		dev_err(&pdev->dev,
			"cannot get clock \"%s\"\n", dev_name(&pdev->dev));
		err = PTR_ERR(shdev->clk);
		goto clk_err;
	}

	pm_runtime_enable(&pdev->dev);
	/* Since PM get and put sync is used , we need to add irq_safe to use in interrupt context */
	pm_runtime_irq_safe(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	spin_lock_irq(&sh_dmae_lock);
	list_add_tail_rcu(&shdev->node, &sh_dmae_devices);
	spin_unlock_irq(&sh_dmae_lock);

	/* reset dma controller - only needed as a test */
	err = sh_dmae_rst(shdev);
	if (err)
		goto rst_err;

	INIT_LIST_HEAD(&shdev->common.channels);

	if (!pdata->slave_only)
		dma_cap_set(DMA_MEMCPY, shdev->common.cap_mask);
	if (pdata->slave && pdata->slave_num)
		dma_cap_set(DMA_SLAVE, shdev->common.cap_mask);

	shdev->common.device_alloc_chan_resources
		= sh_dmae_alloc_chan_resources;
	shdev->common.device_free_chan_resources = sh_dmae_free_chan_resources;
	shdev->common.device_prep_dma_memcpy = sh_dmae_prep_memcpy;
	shdev->common.device_tx_status = sh_dmae_tx_status;
	shdev->common.device_issue_pending = sh_dmae_memcpy_issue_pending;

	/* Compulsory for DMA_SLAVE fields */
	shdev->common.device_prep_slave_sg = sh_dmae_prep_slave_sg;
	shdev->common.device_control = sh_dmae_control;

	/* Default transfer size of 32 bytes requires 32-byte alignment */
	shdev->common.copy_align = LOG2_DEFAULT_XFER_SIZE;

#if defined(CONFIG_CPU_SH4) || defined(CONFIG_ARCH_SHMOBILE)
	chanirq_res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);

	if (!chanirq_res)
		chanirq_res = errirq_res;
	else
		irqres++;

	if (chanirq_res == errirq_res ||
	    (errirq_res->flags & IORESOURCE_BITS) == IORESOURCE_IRQ_SHAREABLE)
		irqflags = IRQF_SHARED;

#ifdef CONFIG_CPU_SH4
	err = request_irq(errirq_res->start, sh_dmae_err, irqflags,
			  "DMAC Address Error", shdev);
	if (err) {
		dev_err(&pdev->dev,
			"DMA failed requesting irq #%d, error %d\n",
			errirq_res->start, err);
		goto eirq_err;
	}
#else
	/* DMA address error is processed channel by channel */
#endif

#else
	chanirq_res = errirq_res;
#endif /* CONFIG_CPU_SH4 || CONFIG_ARCH_SHMOBILE */

	if (chanirq_res->start == chanirq_res->end &&
	    !platform_get_resource(pdev, IORESOURCE_IRQ, 1)) {
		/* Special case - all multiplexed */
		for (; irq_cnt < pdata->channel_num; irq_cnt++) {
			if (irq_cnt < SH_DMAC_MAX_CHANNELS) {
				chan_irq[irq_cnt] = chanirq_res->start;
				chan_flag[irq_cnt] = IRQF_SHARED;
			} else {
				irq_cap = 1;
				break;
			}
		}
	} else {
		do {
			for (i = chanirq_res->start;
			     i <= chanirq_res->end; i++) {
				if (irq_cnt >= SH_DMAC_MAX_CHANNELS) {
					irq_cap = 1;
					break;
				}

				if ((errirq_res->flags & IORESOURCE_BITS) ==
				    IORESOURCE_IRQ_SHAREABLE)
					chan_flag[irq_cnt] = IRQF_SHARED;
				else
					chan_flag[irq_cnt] = IRQF_DISABLED;
				dev_dbg(&pdev->dev,
					"Found IRQ %d for channel %d\n",
					i, irq_cnt);
				chan_irq[irq_cnt++] = i;
			}

			if (irq_cnt >= SH_DMAC_MAX_CHANNELS)
				break;

			chanirq_res = platform_get_resource(pdev,
						IORESOURCE_IRQ, ++irqres);
		} while (irq_cnt < pdata->channel_num && chanirq_res);
	}

	/* Create DMA Channel */
	for (i = 0; i < irq_cnt; i++) {
		err = sh_dmae_chan_probe(shdev, i, chan_irq[i], chan_flag[i]);
		if (err)
			goto chan_probe_err;
	}

	if (irq_cap)
		dev_notice(&pdev->dev, "Attempting to register %d DMA "
			   "channels when a maximum of %d are supported.\n",
			   pdata->channel_num, SH_DMAC_MAX_CHANNELS);


	pm_runtime_put_sync(&pdev->dev);

	dma_async_device_register(&shdev->common);
	return err;

chan_probe_err:
	sh_dmae_chan_remove(shdev);

#if defined(CONFIG_CPU_SH4)
	free_irq(errirq_res->start, shdev);
eirq_err:
#endif
rst_err:
	spin_lock_irq(&sh_dmae_lock);
	list_del_rcu(&shdev->node);
	spin_unlock_irq(&sh_dmae_lock);

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	clk_put(shdev->clk);

clk_err:
	if (dmars)
		iounmap(shdev->dmars);

	platform_set_drvdata(pdev, NULL);
emapdmars:
	iounmap(shdev->chan_reg);
	synchronize_rcu();
emapchan:
	iounmap(shdev->desc_mem);
emapdescmem:
	kfree(shdev);
ealloc:
	if (dmars)
		release_mem_region(dmars->start, resource_size(dmars));
ermrdmars:
	release_mem_region(chan->start, resource_size(chan));
ermrchan:
	release_mem_region(desc_mem->start, resource_size(chan));

	return err;
}

static int __exit sh_dmae_remove(struct platform_device *pdev)
{
	struct sh_dmae_device *shdev = platform_get_drvdata(pdev);
	struct resource *res;

	dma_async_device_unregister(&shdev->common);

#ifdef CONFIG_CPU_SH4
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res)
		free_irq(res->start, shdev);
#endif

	spin_lock_irq(&sh_dmae_lock);
	list_del_rcu(&shdev->node);
	spin_unlock_irq(&sh_dmae_lock);

	/* channel data remove */
	sh_dmae_chan_remove(shdev);

	pm_runtime_disable(&pdev->dev);
	clk_put(shdev->clk);

	if (shdev->desc_mem)
		iounmap(shdev->desc_mem);
	if (shdev->dmars)
		iounmap(shdev->dmars);
	if (shdev->chan_reg)
		iounmap(shdev->chan_reg);

	platform_set_drvdata(pdev, NULL);

	synchronize_rcu();
	kfree(shdev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res)
		release_mem_region(res->start, resource_size(res));

	return 0;
}

static void sh_dmae_shutdown(struct platform_device *pdev)
{
	struct sh_dmae_device *shdev = platform_get_drvdata(pdev);
	sh_dmae_ctl_stop(shdev);
}

static int sh_dmae_runtime_suspend(struct device *dev)
{
	struct sh_dmae_device *shdev = dev_get_drvdata(dev);
	if (shdev && shdev->clk)
		clk_disable(shdev->clk);
	return 0;
}

static int sh_dmae_runtime_resume(struct device *dev)
{
	struct sh_dmae_device *shdev = dev_get_drvdata(dev);
	if (shdev == NULL || shdev->clk == NULL) {
		return -ENODEV;
	}
	else {
		clk_enable(shdev->clk);
		return sh_dmae_rst(shdev);
	}
}

#ifdef CONFIG_PM
static int sh_dmae_suspend(struct device *dev)
{
	return 0;
}

static int sh_dmae_resume(struct device *dev)
{
	struct sh_dmae_device *shdev = dev_get_drvdata(dev);
	int i;


	for (i = 0; i < shdev->pdata->channel_num; i++) {
		struct sh_dmae_chan *sh_chan = shdev->chan[i];
		struct sh_dmae_slave *param = sh_chan->common.private;

		if (!sh_chan->descs_allocated)
			continue;

		if (param) {
			const struct sh_dmae_slave_config *cfg = param->config;

			dmae_set_dmars(sh_chan, cfg->mid_rid);
			dmae_set_chcr(sh_chan, sh_chan->chcr ? : cfg->chcr);
		} else {
			dmae_init(sh_chan);
		}
	}


	return 0;
}
#else
#define sh_dmae_suspend NULL
#define sh_dmae_resume NULL
#endif

const struct dev_pm_ops sh_dmae_pm = {
	.suspend		= sh_dmae_suspend,
	.resume			= sh_dmae_resume,
	.runtime_suspend	= sh_dmae_runtime_suspend,
	.runtime_resume		= sh_dmae_runtime_resume,
};

static struct platform_driver sh_dmae_driver = {
	.remove		= __exit_p(sh_dmae_remove),
	.shutdown	= sh_dmae_shutdown,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sh-dma-engine",
		.pm	= &sh_dmae_pm,
	},
};

static int __init sh_dmae_init(void)
{
	/* Wire up NMI handling */
	int err = register_die_notifier(&sh_dmae_nmi_notifier);
	if (err)
		return err;

	return platform_driver_probe(&sh_dmae_driver, sh_dmae_probe);
}
module_init(sh_dmae_init);

static void __exit sh_dmae_exit(void)
{
	platform_driver_unregister(&sh_dmae_driver);

	unregister_die_notifier(&sh_dmae_nmi_notifier);
}
module_exit(sh_dmae_exit);

MODULE_AUTHOR("Nobuhiro Iwamatsu <iwamatsu.nobuhiro@renesas.com>");
MODULE_DESCRIPTION("Renesas SH DMA Engine driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sh-dma-engine");
