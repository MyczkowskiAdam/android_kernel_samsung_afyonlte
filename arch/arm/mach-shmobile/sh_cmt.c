/*
 * SuperH Timer Support - CMT
 *
 *  Copyright (C) 2008 Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License
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

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/clockchips.h>
#include <linux/sh_timer.h>
#include <linux/slab.h>
#include <linux/module.h>

struct sh_cmt_priv {
	void __iomem *mapbase;
	struct clk *clk;
	struct clk *count_clk;
	unsigned long overflow_bit;
	unsigned long clear_bits;
	struct irqaction irqaction;
	int irq;
	struct platform_device *pdev;

	unsigned long rate;
	spinlock_t lock;
	struct clock_event_device *ced;

	unsigned clk_enabled:1;
};

static struct clock_event_device shmobile_ced;
static struct platform_device *sh_cmt_devices[NR_CPUS];

DEFINE_SPINLOCK(sh_cmt_lock);

#define CMCLKE	-2 /* shared register */
#define CMSTR	0x00 /* channel register */
#define CMCSR	0x10 /* channel register */
#define CMCNT	0x14 /* channel register */
#define CMCOR	0x18 /* channel register */

static inline unsigned long sh_cmt_read(struct sh_cmt_priv *p, int reg_nr)
{
	struct sh_timer_config *cfg = p->pdev->dev.platform_data;
	void __iomem *base = p->mapbase;
	unsigned long offs;

	if (reg_nr == CMCLKE)
		offs = cfg->channel_offset;
	else
		offs = reg_nr;

	return __raw_readl(base + offs);
}

static inline void sh_cmt_write(struct sh_cmt_priv *p, int reg_nr,
				unsigned long value)
{
	struct sh_timer_config *cfg = p->pdev->dev.platform_data;
	void __iomem *base = p->mapbase;
	unsigned long offs;

	if (reg_nr == CMCLKE)
		offs = cfg->channel_offset;
	else
		offs = reg_nr;

	__raw_writel(value, base + offs);
	return;
}

static int sh_cmt_clk_enable(struct sh_cmt_priv *p)
{
	struct sh_timer_config *cfg = p->pdev->dev.platform_data;
	unsigned long flags;
	int ret;

	if (p->clk_enabled)
		return 0;

	ret = clk_enable(p->clk);
	if (ret) {
		dev_err(&p->pdev->dev, "cannot enable clock\n");
		return ret;
	}
	ret = clk_enable(p->count_clk);
	if (ret) {
		dev_err(&p->pdev->dev, "cannot enable counting clock\n");
		clk_disable(p->clk);
		return ret;
	}

	spin_lock_irqsave(&sh_cmt_lock, flags);
	sh_cmt_write(p, CMCLKE, sh_cmt_read(p, CMCLKE) | (1 << cfg->timer_bit));
	spin_unlock_irqrestore(&sh_cmt_lock, flags);

	p->clk_enabled = 1;
	return 0;
}

static void sh_cmt_clk_disable(struct sh_cmt_priv *p)
{
	struct sh_timer_config *cfg = p->pdev->dev.platform_data;
	unsigned long flags;

	if (!p->clk_enabled)
		return;

	spin_lock_irqsave(&sh_cmt_lock, flags);
	sh_cmt_write(p, CMCLKE, sh_cmt_read(p, CMCLKE) & ~(1 << cfg->timer_bit));
	spin_unlock_irqrestore(&sh_cmt_lock, flags);

	clk_disable(p->count_clk);
	clk_disable(p->clk);
	p->clk_enabled = 0;
}

#ifdef CONFIG_ARCH_R8A7373
/*
 * In R-Mobile U2 CMT hardware,
 * 1. CMSTR is not a shared register any more
 * 2. No need to shift the start bit (cfg->timer_bit), always use bit[0]
 *
 * That is, sh_cmt_start_stop_ch() could be replaced by sh_cmt_write()
 */
#define sh_cmt_start_stop_ch(p, start)	sh_cmt_write(p, CMSTR, start)
#else
static void sh_cmt_start_stop_ch(struct sh_cmt_priv *p, int start)
{
	struct sh_timer_config *cfg = p->pdev->dev.platform_data;
	unsigned long flags, value;

	/* start stop register shared by multiple timer channels */
	spin_lock_irqsave(&sh_cmt_lock, flags);
	value = sh_cmt_read(p, CMSTR);

	if (start)
		value |= 1 << cfg->timer_bit;
	else
		value &= ~(1 << cfg->timer_bit);

	sh_cmt_write(p, CMSTR, value);
	spin_unlock_irqrestore(&sh_cmt_lock, flags);
}
#endif

static int sh_cmt_enable(struct sh_cmt_priv *p, unsigned long *rate)
{
	struct sh_timer_config *cfg = p->pdev->dev.platform_data;
	int k, ret;

	/* enable clock */
	ret = sh_cmt_clk_enable(p);
	if (ret)
		goto err0;

	/* make sure channel is disabled */
	sh_cmt_start_stop_ch(p, 0);

	/* configure channel, periodic mode and maximum timeout */
	*rate = clk_get_rate(p->count_clk) / cfg->cks_table[cfg->cks].divisor;
	sh_cmt_write(p, CMCSR, cfg->cmcsr_init | cfg->cks);

	sh_cmt_write(p, CMCOR, 0xffffffff);
	sh_cmt_write(p, CMCNT, 0);

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
		if (!sh_cmt_read(p, CMCNT))
			break;
		udelay(1);
	}

	if (sh_cmt_read(p, CMCNT)) {
		dev_err(&p->pdev->dev, "cannot clear CMCNT\n");
		ret = -ETIMEDOUT;
		goto err1;
	}

	/* enable channel */
	sh_cmt_start_stop_ch(p, 1);
	return 0;
 err1:
	/* stop clock */
	sh_cmt_clk_disable(p);

 err0:
	return ret;
}

static void sh_cmt_disable(struct sh_cmt_priv *p)
{
	/* disable channel */
	sh_cmt_start_stop_ch(p, 0);

	/* disable interrupts in CMT block */
	sh_cmt_write(p, CMCSR, 0);

	/* stop clock */
	sh_cmt_clk_disable(p);
}

static int sh_cmt_set_next(struct sh_cmt_priv *p, unsigned long delta)
{
	unsigned long curr, next;
	unsigned long flags;
	int ret;

	local_irq_save(flags);

	next = sh_cmt_read(p, CMCNT) + delta;
	sh_cmt_write(p, CMCOR, next);
	sh_cmt_read(p, CMCOR); /* defeat write posting */

	curr = sh_cmt_read(p, CMCNT);

	local_irq_restore(flags);

	ret = (int)(next - curr) < 0 ? -ETIME : 0;
	if (ret == -ETIME)
		pr_warn("*** sh_cmt_set_next returning -ETIME...  next=0x%08lx "
			"curr=0x%08lx delta=0x%08lx\n", next, curr, delta);
	return ret;
}

static irqreturn_t sh_cmt_interrupt(int irq, void *dev_id)
{
	struct sh_cmt_priv *p = dev_id;
	struct clock_event_device *ced = p->ced;

	/* clear flags */
	sh_cmt_write(p, CMCSR, sh_cmt_read(p, CMCSR) & p->clear_bits);

	/*
	 * TODO: We have to clear CMF bit and resume CMCNT operation first,
	 * otherwise there is a possibility that CMCNT operation will stop
	 * Clockevent operates with CMT one-shot operating mode, though.
	 */

	ced->event_handler(ced);

	return IRQ_HANDLED;
}

static int sh_cmt_start(struct sh_cmt_priv *p)
{
	return sh_cmt_enable(p, &p->rate);
}

static void sh_cmt_stop(struct sh_cmt_priv *p)
{
	sh_cmt_disable(p);
}

static struct sh_cmt_priv *ced_to_sh_cmt(struct clock_event_device *ced)
{
	return (struct sh_cmt_priv *)ced->priv;
}

#define MIN_DELTA_TICKS		0x1000
#define MAX_DELTA_TICKS		0x3fffffff

static void sh_cmt_clock_event_start(struct sh_cmt_priv *p, int periodic)
{
	struct clock_event_device *ced = p->ced;
	struct sh_timer_config *cfg = p->pdev->dev.platform_data;

	if (periodic)
		cfg->cmcsr_init |= 1 << 8; /* Free-running operation */
	else
		cfg->cmcsr_init &= ~(1 << 8); /* One-shot operation */

	sh_cmt_start(p);

	clockevents_calc_mult_shift(ced, p->rate, 5);
	ced->max_delta_ns = clockevent_delta2ns(MAX_DELTA_TICKS, ced);
	ced->min_delta_ns = clockevent_delta2ns(MIN_DELTA_TICKS, ced);

	if (periodic)
		sh_cmt_write(p, CMCOR, ((p->rate + HZ/2) / HZ) - 1);
	else
		sh_cmt_write(p, CMCOR, MAX_DELTA_TICKS);
}

static void sh_cmt_clock_event_mode(enum clock_event_mode mode,
				    struct clock_event_device *ced)
{
	struct sh_cmt_priv *p = ced_to_sh_cmt(ced);

	/* deal with old setting first */
	switch (ced->mode) {
	case CLOCK_EVT_MODE_PERIODIC:
	case CLOCK_EVT_MODE_ONESHOT:
		sh_cmt_stop(p);
		break;
	default:
		break;
	}

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		dev_info(&p->pdev->dev, "used for periodic clock events\n");
		sh_cmt_clock_event_start(p, 1);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		dev_info(&p->pdev->dev, "used for oneshot clock events\n");
		sh_cmt_clock_event_start(p, 0);
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
		sh_cmt_stop(p);
		break;
	default:
		break;
	}
}

static int sh_cmt_clock_event_next(unsigned long delta,
				   struct clock_event_device *ced)
{
	struct sh_cmt_priv *p = ced_to_sh_cmt(ced);

	return sh_cmt_set_next(p, delta);
}

static void sh_cmt_register_clockevent(struct sh_cmt_priv *p,
				       const char *name, unsigned long rating,
				       struct clock_event_device *ced)
{
	int ret;

	ced->name = name;
	ced->priv = p;
	ced->features = CLOCK_EVT_FEAT_PERIODIC;
	ced->features |= CLOCK_EVT_FEAT_ONESHOT;
	ced->rating = rating;
	ced->irq = p->irq;
	ced->cpumask = cpumask_of(0);
	ced->set_next_event = sh_cmt_clock_event_next;
	ced->set_mode = sh_cmt_clock_event_mode;

	p->ced = ced;

	/* request irq using setup_irq() (too early for request_irq()) */
	p->irqaction.name = name;
	p->irqaction.handler = sh_cmt_interrupt;
	p->irqaction.dev_id = p;
	p->irqaction.flags = IRQF_TIMER | IRQF_NOBALANCING;

	ret = setup_irq(p->irq, &p->irqaction);
	if (ret) {
		dev_err(&p->pdev->dev, "failed to request irq %d\n", p->irq);
		return;
	}

	dev_info(&p->pdev->dev, "used for clock events\n");
	clockevents_register_device(ced);
}

static int sh_cmt_setup(struct sh_cmt_priv *p, struct platform_device *pdev)
{
	struct sh_timer_config *cfg = pdev->dev.platform_data;
	struct resource *res;
	int ret;
	ret = -ENXIO;

	memset(p, 0, sizeof(*p));
	p->pdev = pdev;

	if (!cfg) {
		dev_err(&p->pdev->dev, "missing platform data\n");
		goto err0;
	}
	if (!cfg->cks_table || !cfg->cks_num) {
		dev_err(&p->pdev->dev, "missing clock selection table\n");
		goto err0;
	}
	if ((cfg->cks >= cfg->cks_num) || !(cfg->cks_table[cfg->cks].name)) {
		dev_err(&p->pdev->dev, "invalid clock selected\n");
		goto err0;
	}

	res = platform_get_resource(p->pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&p->pdev->dev, "failed to get I/O memory\n");
		goto err0;
	}

	p->irq = platform_get_irq(p->pdev, 0);
	if (p->irq < 0) {
		dev_err(&p->pdev->dev, "failed to get irq\n");
		goto err0;
	}

	/* map memory, let mapbase point to our channel */
	p->mapbase = ioremap_nocache(res->start, resource_size(res));
	if (p->mapbase == NULL) {
		dev_err(&p->pdev->dev, "failed to remap I/O memory\n");
		goto err0;
	}

	/* get hold of clock */
	p->clk = clk_get(&p->pdev->dev, "cmt_fck");
	if (IS_ERR(p->clk)) {
		dev_err(&p->pdev->dev, "cannot get clock\n");
		ret = PTR_ERR(p->clk);
		goto err1;
	}
	p->count_clk = clk_get(NULL, cfg->cks_table[cfg->cks].name);
	if (IS_ERR(p->count_clk)) {
		dev_err(&p->pdev->dev, "cannot get counting clock\n");
		clk_put(p->clk);
		ret = PTR_ERR(p->count_clk);
		goto err1;
	}

	p->overflow_bit = 0x8000;
	p->clear_bits = ~0xc000;

	spin_lock_init(&p->lock);

	platform_set_drvdata(pdev, p);

	return 0;

err1:
	iounmap(p->mapbase);
err0:
	return ret;
}

static int __devinit sh_cmt_probe(struct platform_device *pdev)
{
	struct sh_cmt_priv *p = platform_get_drvdata(pdev);
	int ret;

	if (p) {
		dev_info(&pdev->dev, "kept as earlytimer\n");
		return 0;
	}

	p = kmalloc(sizeof(*p), GFP_KERNEL);
	if (p == NULL) {
		dev_err(&pdev->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	ret = sh_cmt_setup(p, pdev);
	if (ret)
		kfree(p);
	return ret;
}

static int __devexit sh_cmt_remove(struct platform_device *pdev)
{
	return -EBUSY; /* cannot unregister clockevent */
}

static struct platform_driver sh_cmt_device_driver = {
	.probe		= sh_cmt_probe,
	.remove		= __devexit_p(sh_cmt_remove),
	.driver		= {
		.name	= "sh_cmt",
	}
};

static int __init sh_cmt_init(void)
{
	return platform_driver_register(&sh_cmt_device_driver);
}

static void __exit sh_cmt_exit(void)
{
	platform_driver_unregister(&sh_cmt_device_driver);
}

early_platform_init("earlytimer", &sh_cmt_device_driver);
module_init(sh_cmt_init);
module_exit(sh_cmt_exit);

MODULE_AUTHOR("Magnus Damm");
MODULE_DESCRIPTION("SuperH CMT Timer Driver");
MODULE_LICENSE("GPL v2");

void sh_cmt_register_devices(struct platform_device **devs, int num)
{
	int i;

	if (num > NR_CPUS)
		return;

	for (i = 0; i < num; i++)
		sh_cmt_devices[i] = devs[i];
}

void shmobile_clockevent_init(void)
{
	struct platform_device *pdev;
	struct sh_cmt_priv *p;
	struct sh_timer_config *cfg;

	pdev = sh_cmt_devices[0];
	p = platform_get_drvdata(pdev);
	cfg = pdev->dev.platform_data;

	sh_cmt_register_clockevent(p, dev_name(&pdev->dev),
				   cfg->clockevent_rating, &shmobile_ced);
}

#ifdef CONFIG_SMP

int __cpuinit cmt_timer_setup(struct clock_event_device *clk)
{
	struct platform_device *pdev;
	struct sh_cmt_priv *p;
	unsigned int cpu = smp_processor_id();

	if (cpu < 1)
		return 0;

	pdev = sh_cmt_devices[cpu];
	p = platform_get_drvdata(pdev);

	clk->name = dev_name(&p->pdev->dev);
	clk->priv = p;
	clk->features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT;
	clk->rating = 350;
	clk->irq = p->irq;
	clk->set_next_event = sh_cmt_clock_event_next;
	clk->set_mode = sh_cmt_clock_event_mode;

	p->ced = clk;

	/* request irq using setup_irq() (too early for request_irq()) */
	p->irqaction.name = dev_name(&p->pdev->dev);
	p->irqaction.handler = sh_cmt_interrupt;
	p->irqaction.dev_id = p;
	p->irqaction.flags = IRQF_TIMER | IRQF_NOBALANCING;

	setup_irq(p->irq, &p->irqaction);
	irq_set_affinity(p->irq, cpumask_of(cpu));

	dev_info(&p->pdev->dev, "used for clock events\n");
	clockevents_register_device(clk);
	return 0;
}

#endif /* CONFIG_SMP */
