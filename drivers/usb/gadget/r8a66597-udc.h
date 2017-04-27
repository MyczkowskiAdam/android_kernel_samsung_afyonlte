/*
 * R8A66597 UDC
 *
 * Copyright (C) 2007-2009 Renesas Solutions Corp.
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * Author : Yoshihiro Shimoda <yoshihiro.shimoda.uh@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef __R8A66597_H__
#define __R8A66597_H__

#ifdef CONFIG_HAVE_CLK
#include <linux/clk.h>
#endif

#include <linux/usb/r8a66597.h>
#include <linux/usb/r8a66597_dmac.h>
#include <linux/wakelock.h>

#define R8A66597_MAX_SAMPLING	10
#define R8A66597_MAX_PACKET_SIZE	512

#ifdef CONFIG_USB_R8A66597_TYPE_BULK_PIPES_12
#define R8A66597_MAX_NUM_PIPE	16
#define R8A66597_MAX_NUM_BULK	10
#define R8A66597_MAX_NUM_ISOC	2
#define R8A66597_MAX_NUM_INT	3
#else
#define R8A66597_MAX_NUM_PIPE	10
#define R8A66597_MAX_NUM_BULK	3
#define R8A66597_MAX_NUM_ISOC	2
#define R8A66597_MAX_NUM_INT	4
#endif

#define R8A66597_BASE_PIPENUM_BULK	1
#define R8A66597_BASE_PIPENUM_ISOC	1
#define R8A66597_BASE_PIPENUM_INT	6

#define R8A66597_BASE_BUFNUM	6
#define R8A66597_MAX_BUFNUM	0x4F
#define R8A66597_MAX_DMA_CHANNELS	2

#define r8a66597_has_dmac(r8a66597)	(r8a66597->pdata->dmac)

struct r8a66597_pipe_info {
	u16	pipe;
	u16	epnum;
	u16	maxpacket;
	u16	type;
	u16	interval;
	u16	dir_in;
};

struct r8a66597_request {
	struct usb_request	req;
	struct list_head	queue;
	unsigned		mapped:1;
};

struct r8a66597_ep {
	struct usb_ep		ep;
	struct r8a66597		*r8a66597;
	struct r8a66597_dma	*dma;

	struct list_head	queue;
	unsigned		busy:1;
	unsigned		wedge:1;
	unsigned		internal_ccpl:1;	/* use only control */

	/* this member can able to after r8a66597_enable */
	unsigned		use_dma:1;
	u16			pipenum;
	u16			type;

	/* register address */
	unsigned char		fifoaddr;
	unsigned char		fifosel;
	unsigned char		fifoctr;
	unsigned char		pipectr;
	unsigned char		pipetre;
	unsigned char		pipetrn;
};

/*
 * Use CH0 and CH1 with their transfer direction fixed.  Please refer
 * to [Restrictions] 4) IN/OUT switching after NULLL packet reception,
 * at the end of "DMA Transfer Function, (3) DMA transfer flow" in the
 * datasheet.
 */
#define USBHS_DMAC_OUT_CHANNEL	0
#define USBHS_DMAC_IN_CHANNEL	1

struct r8a66597_dma {
	struct r8a66597_ep	*ep;
	unsigned long		expect_dmicr;
	unsigned long		chcr_ts;
	int			channel;
	int			tx_size;

	unsigned		initialized:1;
	unsigned		used:1;
	unsigned		dir:1;	/* 1 = IN(write), 0 = OUT(read) */
};

struct r8a66597 {
	spinlock_t		lock;
	void __iomem		*reg;
	void __iomem		*dmac_reg;

#ifdef CONFIG_HAVE_CLK
	struct clk *clk;
	struct clk *clk_dmac;
#endif
	struct r8a66597_platdata	*pdata;

	struct usb_gadget		gadget;
	struct usb_gadget_driver	*driver;

	struct r8a66597_ep	ep[R8A66597_MAX_NUM_PIPE];
	struct r8a66597_ep	*pipenum2ep[R8A66597_MAX_NUM_PIPE];
	struct r8a66597_ep	*epaddr2ep[16];
	struct r8a66597_dma	dma[R8A66597_MAX_DMA_CHANNELS];

	struct timer_list	timer;
	struct usb_request	*ep0_req;	/* for internal request */
	u16			ep0_data;	/* for internal request */
	u16			old_vbus;
	u16			scount;
	u16			old_dvsq;
	u16			device_status;	/* for GET_STATUS */

	/* pipe config */
	unsigned char num_dma;

	unsigned irq_sense_low:1;
	unsigned vbus_active:1;
	unsigned softconnect:1;
	unsigned is_active:1;
	unsigned phy_active:1;
	unsigned phy_active_sav:1; /*save the state of the phy before resume*/

	struct usb_phy		*transceiver;
	struct delayed_work	vbus_work;
	struct wake_lock	wake_lock;
#ifdef CONFIG_USB_OTG
	struct delayed_work hnp_work;
	struct timer_list   hnp_timer_fail;
	unsigned host_request_flag:1;
	unsigned role;
#endif
};

#define gadget_to_r8a66597(_gadget)	\
		container_of(_gadget, struct r8a66597, gadget)
#define r8a66597_to_gadget(r8a66597) (&r8a66597->gadget)
#define r8a66597_to_dev(r8a66597)	(r8a66597->gadget.dev.parent)

static inline u16 r8a66597_read(struct r8a66597 *r8a66597, unsigned long offset)
{
	return ioread16(r8a66597->reg + offset);
}

static inline void r8a66597_read_fifo(struct r8a66597 *r8a66597,
				      unsigned long offset,
				      unsigned char *buf,
				      int len)
{
	void __iomem *fifoaddr = r8a66597->reg + offset;
	unsigned int data = 0;
	int i;

	if (r8a66597->pdata->on_chip) {
		/* 32-bit accesses for on_chip controllers */

		/* aligned buf case */
		if (len >= 4 && !((unsigned long)buf & 0x03)) {
			ioread32_rep(fifoaddr, buf, len / 4);
			buf += len & ~0x03;
			len &= 0x03;
		}

		/* unaligned buf case */
		for (i = 0; i < len; i++) {
			if (!(i & 0x03))
				data = ioread32(fifoaddr);

			buf[i] = (data >> ((i & 0x03) * 8)) & 0xff;
		}
	} else {
		/* 16-bit accesses for external controllers */

		/* aligned buf case */
		if (len >= 2 && !((unsigned long)buf & 0x01)) {
			ioread16_rep(fifoaddr, buf, len / 2);
			buf += len & ~0x01;
			len &= 0x01;
		}

		/* unaligned buf case */
		for (i = 0; i < len; i++) {
			if (!(i & 0x01))
				data = ioread16(fifoaddr);

			buf[i] = (data >> ((i & 0x01) * 8)) & 0xff;
		}
	}
}

static inline void r8a66597_write(struct r8a66597 *r8a66597, u16 val,
				  unsigned long offset)
{
	iowrite16(val, r8a66597->reg + offset);
}

static inline void r8a66597_mdfy(struct r8a66597 *r8a66597,
				 u16 val, u16 pat, unsigned long offset)
{
	u16 tmp;
	tmp = r8a66597_read(r8a66597, offset);
	tmp = tmp & (~pat);
	tmp = tmp | val;
	r8a66597_write(r8a66597, tmp, offset);
}

/* USBHS-DMAC read/write */
static inline u32 r8a66597_dma_read(struct r8a66597 *r8a66597,
				unsigned long offset)
{
	return ioread32(r8a66597->dmac_reg + offset);
}

static inline void r8a66597_dma_write(struct r8a66597 *r8a66597, u32 val,
				unsigned long offset)
{
	iowrite32(val, r8a66597->dmac_reg + offset);
}

static inline void r8a66597_dma_mdfy(struct r8a66597 *r8a66597,
				 u32 val, u32 pat, unsigned long offset)
{
	u32 tmp;
	tmp = r8a66597_dma_read(r8a66597, offset);
	tmp = tmp & (~pat);
	tmp = tmp | val;
	r8a66597_dma_write(r8a66597, tmp, offset);
}

#define r8a66597_bclr(r8a66597, val, offset)	\
			r8a66597_mdfy(r8a66597, 0, val, offset)
#define r8a66597_bset(r8a66597, val, offset)	\
			r8a66597_mdfy(r8a66597, val, 0, offset)

#define r8a66597_dma_bclr(r8a66597, val, offset)	\
			r8a66597_dma_mdfy(r8a66597, 0, val, offset)
#define r8a66597_dma_bset(r8a66597, val, offset)	\
			r8a66597_dma_mdfy(r8a66597, val, 0, offset)

static inline void r8a66597_write_fifo(struct r8a66597 *r8a66597,
				       struct r8a66597_ep *ep,
				       unsigned char *buf,
				       int len)
{
	void __iomem *fifoaddr = r8a66597->reg + ep->fifoaddr;
	int adj = 0;
	int i;

	if (r8a66597->pdata->on_chip) {
		/* 32-bit access only if buf is 32-bit aligned */
		if (len >= 4 && !((unsigned long)buf & 0x03)) {
			iowrite32_rep(fifoaddr, buf, len / 4);
			buf += len & ~0x03;
			len &= 0x03;
		}
	} else {
		/* 16-bit access only if buf is 16-bit aligned */
		if (len >= 2 && !((unsigned long)buf & 0x01)) {
			iowrite16_rep(fifoaddr, buf, len / 2);
			buf += len & ~0x01;
			len &= 0x01;
		}
	}

	/* adjust fifo address in the little endian case */
	if (!(r8a66597_read(r8a66597, CFIFOSEL) & BIGEND)) {
		if (r8a66597->pdata->on_chip)
			adj = 0x03; /* 32-bit wide */
		else
			adj = 0x01; /* 16-bit wide */
	}

	if (r8a66597->pdata->wr0_shorted_to_wr1)
		r8a66597_bclr(r8a66597, MBW_16, ep->fifosel);
	for (i = 0; i < len; i++)
		iowrite8(buf[i], fifoaddr + adj - (i & adj));
	if (r8a66597->pdata->wr0_shorted_to_wr1)
		r8a66597_bclr(r8a66597, MBW_16, ep->fifosel);
}

static inline u16 get_xtal_from_pdata(struct r8a66597_platdata *pdata)
{
	u16 clock = 0;

	switch (pdata->xtal) {
	case R8A66597_PLATDATA_XTAL_12MHZ:
		clock = XTAL12;
		break;
	case R8A66597_PLATDATA_XTAL_24MHZ:
		clock = XTAL24;
		break;
	case R8A66597_PLATDATA_XTAL_48MHZ:
		clock = XTAL48;
		break;
	default:
		printk(KERN_ERR "r8a66597: platdata clock is wrong.\n");
		break;
	}

	return clock;
}

#define get_pipectr_addr(pipenum)	(PIPE1CTR + (pipenum - 1) * 2)
#ifdef CONFIG_USB_R8A66597_TYPE_BULK_PIPES_12
static inline unsigned long get_pipetre_addr(u16 pipenum)
{
	const unsigned long offset[] = {
		0,		PIPE1TRE,	PIPE2TRE,	PIPE3TRE,
		PIPE4TRE,	PIPE5TRE,	0,		0,
		0,		PIPE9TRE,	PIPEATRE,	PIPEBTRE,
		PIPECTRE,	PIPEDTRE,	PIPEETRE,	PIPEFTRE,
	};

	if (offset[pipenum] == 0) {
		printk(KERN_ERR "no PIPEnTRE (%d)\n", pipenum);
		return 0;
	}

	return offset[pipenum];
}

static inline unsigned long get_pipetrn_addr(u16 pipenum)
{
	const unsigned long offset[] = {
		0,		PIPE1TRN,	PIPE2TRN,	PIPE3TRN,
		PIPE4TRN,	PIPE5TRN,	0,		0,
		0,		PIPE9TRN,	PIPEATRN,	PIPEBTRN,
		PIPECTRN,	PIPEDTRN,	PIPEETRN,	PIPEFTRN,
	};

	if (offset[pipenum] == 0) {
		printk(KERN_ERR "no PIPEnTRN (%d)\n", pipenum);
		return 0;
	}

	return offset[pipenum];
}

#else
#define get_pipetre_addr(pipenum)	(PIPE1TRE + (pipenum - 1) * 4)
#define get_pipetrn_addr(pipenum)	(PIPE1TRN + (pipenum - 1) * 4)
#endif

#define enable_irq_ready(r8a66597, pipenum)	\
	enable_pipe_irq(r8a66597, pipenum, BRDYENB)
#define disable_irq_ready(r8a66597, pipenum)	\
	disable_pipe_irq(r8a66597, pipenum, BRDYENB)
#define enable_irq_empty(r8a66597, pipenum)	\
	enable_pipe_irq(r8a66597, pipenum, BEMPENB)
#define disable_irq_empty(r8a66597, pipenum)	\
	disable_pipe_irq(r8a66597, pipenum, BEMPENB)
#define enable_irq_nrdy(r8a66597, pipenum)	\
	enable_pipe_irq(r8a66597, pipenum, NRDYENB)
#define disable_irq_nrdy(r8a66597, pipenum)	\
	disable_pipe_irq(r8a66597, pipenum, NRDYENB)

#endif	/* __R8A66597_H__ */
