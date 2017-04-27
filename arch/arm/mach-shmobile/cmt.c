/*
 * R-Mobile CMT (Compare Match Timer) support
 *
 * Copyright (C) 2012  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * Features:
 * - both standard type CMT and new one (revised in R-Mobile U2) supported
 * - both One-shot and Free-running operations supported
 * - Compare Match Timer Counter Size (CMCSR.CMS) is supposed to be 2'b00;
 *   CMCNT/CMCOR must be used as a 32-bit counter
 * - RCLK-synchronous counter start/stop mode is not supported
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/clockchips.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/percpu.h>
#include <linux/spinlock.h>

#include <asm/localtimer.h>

#include <mach/cmt.h>

struct cmt_clock_event_device;

struct cmt_ops {
	int (*ack)(struct cmt_clock_event_device *cmt);
	int (*start)(struct cmt_clock_event_device *cmt, u16 cmcsr, u32 cmcor);
	void (*stop)(struct cmt_clock_event_device *cmt);
};

struct cmt_clock_event_device {
	struct clock_event_device *evt;
	void __iomem *base;
	struct cmt_ops *ops;

	struct clk *clk;
	struct clk *count_clk;
	struct irqaction irqaction;
	unsigned int irq;
	unsigned long rate;
	bool irq_setup;
	bool clk_enabled;

	struct cmt_timer_config *cfg;
};

DEFINE_SPINLOCK(cmt_lock);
static void __iomem *cmstr __read_mostly;
static void __iomem *cmclke __read_mostly;
static struct cmt_clock_event_device __percpu *percpu_evt;

/*
 * common helper routine
 */
static int cmt_clk_enable(struct cmt_clock_event_device *cmt)
{
	int ret;

	if (cmt->clk_enabled)
		return 0;

	ret = clk_enable(cmt->clk);
	if (ret) {
		pr_err("cannot enable clock\n");
		goto err_clk;
	}

	ret = clk_enable(cmt->count_clk);
	if (ret) {
		pr_err("cannot enable counting clock\n");
		goto err_count_clk;
	}

	cmt->clk_enabled = true;
	return ret;

 err_count_clk:
	clk_disable(cmt->clk);
 err_clk:
	return ret;
}

static void cmt_clk_disable(struct cmt_clock_event_device *cmt)
{
	if (!cmt->clk_enabled)
		return;

	clk_disable(cmt->count_clk);
	clk_disable(cmt->clk);
	cmt->clk_enabled = false;
}

static int cmt_counter_init(struct cmt_clock_event_device *cmt)
{
	int k;

	__raw_writel(0xffffffff, cmt->base + CMCOR);
	__raw_writel(0, cmt->base + CMCNT);

	/*
	 * According to the sh73a0 user's manual, as CMCNT can be operated
	 * only by the RCLK (Pseudo 32 KHz), there's one restriction on
	 * modifying CMCNT register; two RCLK cycles are necessary before
	 * this register is either read or any modification of the value
	 * it holds is reflected in the LSI's actual operation.
	 *
	 * While at it, we're supposed to clear out the CMCNT as of this
	 * moment, so make sure it's processed properly here.  This will
	 * take RCLKx2 at maximum.
	 */
	for (k = 0; k < 100; k++) {
		if (!__raw_readl(cmt->base + CMCNT))
			break;
		udelay(1);
	}

	if (__raw_readl(cmt->base + CMCNT)) {
		pr_err("cannot clear CMCNT\n");
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * low-level hardware functions - standard type with CMSTR/CMCSR 16-bit
 */
static int cmt_16bit_ack(struct cmt_clock_event_device *cmt)
{
	u16 cmcsr;

	cmcsr = __raw_readw(cmt->base + CMCSR);
	if (likely(cmcsr & CMF)) {
		__raw_writew(cmcsr & ~CMF, cmt->base + CMCSR);
		return 1;
	}
	return 0;
}

static int cmt_16bit_start(struct cmt_clock_event_device *cmt, u16 cmcsr, u32 cmcor)
{
	unsigned long flags;
	u16 value;
	int ret;

	spin_lock_irqsave(&cmt_lock, flags);
	value = __raw_readw(cmstr);
	__raw_writew(value & ~(1 << cmt->cfg->timer_bit), cmstr);
	spin_unlock_irqrestore(&cmt_lock, flags);

	ret = cmt_counter_init(cmt);
	if (ret)
		return ret;

	__raw_writew(cmcsr, cmt->base + CMCSR);
	__raw_writel(cmcor, cmt->base + CMCOR); /* 32-bit */

	spin_lock_irqsave(&cmt_lock, flags);
	value = __raw_readw(cmstr);
	__raw_writew(value | (1 << cmt->cfg->timer_bit), cmstr);
	spin_unlock_irqrestore(&cmt_lock, flags);

	return 0;
}

static void cmt_16bit_stop(struct cmt_clock_event_device *cmt)
{
	unsigned long flags;
	u16 value;

	spin_lock_irqsave(&cmt_lock, flags);
	value = __raw_readw(cmstr);
	__raw_writew(value & ~(1 << cmt->cfg->timer_bit), cmstr);
	spin_unlock_irqrestore(&cmt_lock, flags);
}

struct cmt_ops cmt_16bit_ops = {
	.ack	= cmt_16bit_ack,
	.start	= cmt_16bit_start,
	.stop	= cmt_16bit_stop,
};

/*
 * low-level hardware functions - new type with CMSTR/CMCSR 32-bit
 */
static int cmt_32bit_ack(struct cmt_clock_event_device *cmt)
{
	u32 cmcsr;

	cmcsr = __raw_readl(cmt->base + CMCSR);
	if (likely(cmcsr & CMF)) {
		__raw_writel(cmcsr & ~CMF, cmt->base + CMCSR);
		return 1;
	}
	return 0;
}

static int cmt_32bit_start(struct cmt_clock_event_device *cmt, u16 cmcsr, u32 cmcor)
{
	unsigned long flags;
	u32 value;
	int ret;

	spin_lock_irqsave(&cmt_lock, flags);
	value = __raw_readl(cmclke);
	__raw_writel(value | (1 << cmt->cfg->timer_bit), cmclke);
	spin_unlock_irqrestore(&cmt_lock, flags);

	__raw_writel(0, cmt->base + CMSTR);

	ret = cmt_counter_init(cmt);
	if (ret)
		return ret;

	__raw_writel(cmcsr, cmt->base + CMCSR);
	__raw_writel(cmcor, cmt->base + CMCOR);

	__raw_writel(1, cmt->base + CMSTR);

	return 0;
}

static void cmt_32bit_stop(struct cmt_clock_event_device *cmt)
{
	unsigned long flags;
	u32 value;

	__raw_writel(0, cmt->base + CMSTR);

	spin_lock_irqsave(&cmt_lock, flags);
	value = __raw_readl(cmclke);
	__raw_writel(value & ~(1 << cmt->cfg->timer_bit), cmclke);
	spin_unlock_irqrestore(&cmt_lock, flags);
}

struct cmt_ops cmt_32bit_ops = {
	.ack	= cmt_32bit_ack,
	.start	= cmt_32bit_start,
	.stop	= cmt_32bit_stop,
};

/*
 * top-level CMT maintenance operations
 */

#define cmt_ack(cmt)	(cmt->ops->ack(cmt))

static int cmt_start(struct cmt_clock_event_device *cmt,
		     enum clock_event_mode mode)
{
	int ret;
	u16 cmcsr;
	u32 cmcor;

	ret = cmt_clk_enable(cmt);
	if (ret)
		goto err0;

	cmcsr = cmt->cfg->cmcsr_init | cmt->cfg->cks;
	cmcsr &= ~(CMF | OVF);	/* clear pending flags */

	if (mode == CLOCK_EVT_MODE_PERIODIC) {
		pr_info("%s used for periodic clock events\n", cmt->cfg->name);

		cmcsr |= CMM;	/* Free-running operation */
		cmcor = ((cmt->rate + HZ/2) / HZ) - 1;
	} else {
		pr_info("%s used for oneshot clock events\n", cmt->cfg->name);

		cmcsr &= ~CMM;	/* One-shot operation */
		cmcor = 0xffffffff;
	}

	ret = cmt->ops->start(cmt, cmcsr, cmcor);
	if (ret)
		goto err1;

	return ret;

 err1:
	cmt_clk_disable(cmt);
 err0:
	return ret;
}

static void cmt_stop(struct cmt_clock_event_device *cmt)
{
	if (!cmt->clk_enabled)
		return;

	cmt->ops->stop(cmt);
	cmt_clk_disable(cmt);
}

/*
 * high-level linux-related functions
 */
static void cmt_timer_set_mode(enum clock_event_mode mode,
			       struct clock_event_device *evt)
{
	struct cmt_clock_event_device *cmt = __this_cpu_ptr(percpu_evt);

	cmt_stop(cmt);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
	case CLOCK_EVT_MODE_ONESHOT:
		cmt_start(cmt, mode);
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
}

static int cmt_timer_set_next_event(unsigned long delta,
				    struct clock_event_device *evt)
{
	struct cmt_clock_event_device *cmt = __this_cpu_ptr(percpu_evt);
	unsigned long flags;
	u32 cmcnt, next;

	local_irq_save(flags);

	next = __raw_readl(cmt->base + CMCNT) + delta;
	__raw_writel(next, cmt->base + CMCOR);

	/*
	 * After writing a value to CMCOR, two cycles of Counter input
	 * clock (RCLK or MAIN CLOCK 1/2) are necessary before the value
	 * gets reflected in the LSI's actual operation.
	 */
	cmcnt = __raw_readl(cmt->base + CMCNT); /* defeat write posting */

	local_irq_restore(flags);

	/*
	 * Make sure that a new value set-up into CMCOR register is ahead
	 * of current CMCNT value, at least by (2 + 1) ticks.
	 */
	return ((int)(next - (cmcnt + 2 + 1)) < 0) ? -ETIME : 0;
}

static irqreturn_t cmt_timer_interrupt(int irq, void *dev_id)
{
	struct cmt_clock_event_device *cmt = dev_id;
	struct clock_event_device *evt = cmt->evt;

	if (likely(cmt_ack(cmt))) {
		evt->event_handler(evt);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static void __cpuinit __cmt_timer_setup(struct clock_event_device *evt)
{
	struct cmt_clock_event_device *cmt = __this_cpu_ptr(percpu_evt);
	unsigned int cpu = smp_processor_id();
	unsigned long min_delta;
	int ret = 0;

	evt->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT;
	evt->set_mode = cmt_timer_set_mode;
	evt->set_next_event = cmt_timer_set_next_event;

	evt->name = cmt->cfg->name;
	evt->rating = 450;
	evt->irq = cmt->irq;
	evt->cpumask = cpumask_of(cpu);

	min_delta = cmt->cfg->cks_table[cmt->cfg->cks].min_delta_ticks;
	if (min_delta < 4) {
		pr_err("Error, min_delta_ticks is not right, load 0x1f...\n");
		min_delta = 0x1f;
	}

	clockevents_config_and_register(evt, cmt->rate, min_delta, 0x7fffffff);

	cmt->evt = evt;

	/* request irq using setup_irq() (too early for request_irq()) */
	cmt->irqaction.name = cmt->cfg->name;
	cmt->irqaction.handler = cmt_timer_interrupt;
	cmt->irqaction.dev_id = cmt;
	cmt->irqaction.flags = IRQF_TIMER | IRQF_NOBALANCING;
	if (!cmt->irq_setup) {
		ret = setup_irq(evt->irq, &cmt->irqaction);
		if (ret)
			pr_err("Error, irq setup failed with ret %d\n", ret);
		else
			cmt->irq_setup = 1;
	} else
		enable_irq(evt->irq);
	irq_set_affinity(evt->irq, cpumask_of(cpu));

	pr_info("%s used for clock events\n", cmt->cfg->name);
}

static int __cpuinit cmt_timer_setup(struct clock_event_device *evt)
{
	/* Use existing clock_event for cpu 0 */
	if (!smp_processor_id())
		return 0;

	__cmt_timer_setup(evt);
	return 0;
}

static void cmt_timer_stop(struct clock_event_device *evt)
{
	struct cmt_clock_event_device *cmt = __this_cpu_ptr(percpu_evt);

	cmt_stop(cmt);
	if (cmt->irq_setup)
		disable_irq(evt->irq);
}

static struct local_timer_ops cmt_timer_ops __cpuinitdata = {
	.setup	= cmt_timer_setup,
	.stop	= cmt_timer_stop,
};

static struct clock_event_device cmt_timer_global_evt;

static int __init cmt_timer_init_single(struct cmt_clock_event_device *cmt,
					struct cmt_timer_config *cfg)
{
	unsigned long remap_offset;
	int ret = -ENXIO;

	if (!cfg->cks_table || !cfg->cks_num) {
		pr_err("missing clock selection table\n");
		goto err0;
	}
	if ((cfg->cks >= cfg->cks_num) || !(cfg->cks_table[cfg->cks].name)) {
		pr_err("invalid clock selected\n");
		goto err0;
	}

	if (cmclke) {
		remap_offset = 0;
		cmt->ops = &cmt_32bit_ops;
	} else if (cmstr) {
		remap_offset = 0x10;
		cmt->ops = &cmt_16bit_ops;
	} else {
		pr_err("unknown CMT type, rejected\n");
		goto err0;
	}

	/* map memory, let mapbase point to our channel */
	cmt->base = ioremap(cfg->res[0].start - remap_offset,
			    resource_size(&cfg->res[0]) + remap_offset);
	if (!cmt->base) {
		pr_err("failed to remap I/O memory\n");
		goto err0;
	}
	cmt->irq = cfg->res[1].start;

	/* get hold of clock */
	cmt->clk = clk_get_sys(cfg->name, NULL);
	if (IS_ERR(cmt->clk)) {
		pr_err("cannot get clock\n");
		ret = PTR_ERR(cmt->clk);
		goto err1;
	}

	cmt->count_clk = clk_get(NULL, cfg->cks_table[cfg->cks].name);
	if (IS_ERR(cmt->count_clk)) {
		pr_err("cannot get counting clock\n");
		clk_put(cmt->clk);
		ret = PTR_ERR(cmt->count_clk);
		goto err1;
	}

	cmt->rate = clk_get_rate(cmt->count_clk);
	if (cmt->rate == 0) {
		pr_warn("counting clock frequency not available\n");
		ret = -EINVAL;
		goto err1;
	}

	cmt->cfg = cfg;

	pr_info("%s 0x%p probed with %s, cks=%d, min_delta_ticks=%ld\n",
		cfg->name, cmt->base, cmclke ? "CMCLKE" : "CMSTR",
		cfg->cks, cfg->cks_table[cfg->cks].min_delta_ticks);

	return 0;

 err1:
	iounmap(cmt->base);
 err0:
	return ret;
}

int __init cmt_clockevent_init(struct cmt_timer_config *cfg, int num,
			       unsigned long cmstr_base,
			       unsigned long cmclke_base)
{
	struct cmt_clock_event_device *cmt;
	unsigned int cpu;
	int ret = -ENXIO;

	if (percpu_evt)
		return -EBUSY;

	if (!cfg || !num)
		return -EINVAL;

	percpu_evt = alloc_percpu(struct cmt_clock_event_device);
	if (!percpu_evt)
		return -ENOMEM;

	/* remap I/O memory for a shared register, CMSTR or CMCLKE */
	if (cmclke_base) {
		cmclke = ioremap(cmclke_base, 4);
		pr_info("CMCLKE 0x%lx mapped to 0x%p\n", cmclke_base, cmclke);
	} else if (cmstr_base) {
		cmstr = ioremap(cmstr_base, 4);
		pr_info("CMSTR 0x%lx mapped to 0x%p\n", cmstr_base, cmstr);
	} else {
		pr_err("failed to get I/O memory to a shared register\n");
		goto out_free;
	}

	for_each_possible_cpu(cpu) {
		if (cpu >= num) {
			pr_err("run out of timer configs (%d/%d)\n", cpu, num);
			break;
		}

		cmt = per_cpu_ptr(percpu_evt, cpu);
		ret = cmt_timer_init_single(cmt, &cfg[cpu]);
		if (ret)
			break;
	}

	local_timer_register(&cmt_timer_ops);

	__cmt_timer_setup(&cmt_timer_global_evt);

	return 0;

out_free:
	free_percpu(percpu_evt);

	return ret;
}
