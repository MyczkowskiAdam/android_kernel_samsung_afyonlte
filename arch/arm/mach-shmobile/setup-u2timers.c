/**
 * @note To define ARCH_HAS_READ_CURRENT_TIMER,
 *       it will include arch/arm/mach-shmobile/include/mach/timex.h
 */
#include <linux/timex.h>

#include <mach/cmt.h>
#include <mach/r8a7373.h>
#include <mach/common.h>
#include <mach/irqs.h>
#include <linux/sh_timer.h>
#include <linux/spinlock_types.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/rmu2_rwdt.h>
#include <linux/clocksource.h>
#include <asm/io.h>
#include <asm/sched_clock.h>


#ifdef ARCH_HAS_READ_CURRENT_TIMER
struct delay_timer cmt_delay_timer __read_mostly;

int cmt_read_current_timer(unsigned long *timer_val)
{
	*timer_val = __raw_readl(CMCNT3);
	return 0;
}

int __init setup_current_timer(void)
{
	struct clk *clk = NULL;
	unsigned long lpj = 0, flags = 0;
	int ret = 0;

	clk = clk_get_sys("currtimer", NULL);
	if (IS_ERR(clk))
		return PTR_ERR(clk);
	ret = clk_enable(clk);
	if (ret < 0)

	{
		pr_err("%s:couldn't enable current timer clock\n", __func__);
		clk_put(clk);
		return ret;
	}

	lpj = clk_get_rate(clk) + HZ/2;
	do_div(lpj, HZ);
	lpj_fine = lpj;

	spin_lock_irqsave(&cmt_lock, flags);
	__raw_writel(__raw_readl(CMCLKE) | (1 << 3), CMCLKE);
	spin_unlock_irqrestore(&cmt_lock, flags);

	__raw_writel(0, CMSTR3);
	__raw_writel(0x103, CMCSR3); /* Free-running, DBGIVD, CKS=3 */
	__raw_writel(0xffffffff, CMCOR3);
	__raw_writel(0, CMCNT3);
	while (__raw_readl(CMCNT3) != 0)
		cpu_relax();
	__raw_writel(1, CMSTR3);

	pr_info("Current timer started (lpj=%lu)\n", lpj);

	/*
	 * TODO: Current timer (CMT1) MSTP bit vs. Suspend-to-RAM
	 *
	 * We don't have proper suspend/resume operations implemented yet
	 * for the current timer (CMT1), so there is no guarantee that CMT1
	 * module is functional when timer-based udelay() is used.  Thus
	 * we need to enable CMT1 MSTP clock here, and if possible, would
	 * like to leave it enabled forever.
	 *
	 * On the other hand, CMT1 should be halted during Suspend-to-RAM
	 * state to minimize power consumption.
	 *
	 * To solve the problem, we make the following assumptions:
	 *
	 * 1) udelay() is not used from now (time_init()) until
	 *    late_time_init() or calibrate_delay() completes
	 *
	 * 2) timer-based udelay() is functional as long as clocksource is
	 *    available
	 *
	 * and disable CMT1 MSTP clock here not to increment CMT1 usecount.
	 */
	cmt_delay_timer.read_current_timer = cmt_read_current_timer;
	cmt_delay_timer.freq = clk_get_rate(clk);
	register_current_timer_delay(&cmt_delay_timer);
	clk_disable(clk);
	clk_put(clk);
	return 0;
}

#endif /* ARCH_HAS_READ_CURRENT_TIMER */


static void cmt10_start(bool clear)
{
	unsigned long flags;

	spin_lock_irqsave(&cmt_lock, flags);
	__raw_writel(__raw_readl(CMCLKE) | (1 << 0), CMCLKE);
	spin_unlock_irqrestore(&cmt_lock, flags);

	/* stop */
	__raw_writel(0, CMSTR0);

	/* setup */
	if (clear)
		__raw_writel(0, CMCNT0);
	__raw_writel(0x103, CMCSR0); /* Free-running, DBGIVD, cp_clk/1 */
	__raw_writel(0xffffffff, CMCOR0);
	while (__raw_readl(CMCSR0) & (1<<13))
		cpu_relax();

	/* start */
	__raw_writel(1, CMSTR0);
}

static void cmt10_stop(void)
{
	unsigned long flags;

	__raw_writel(0, CMSTR0);

	spin_lock_irqsave(&cmt_lock, flags);
	__raw_writel(__raw_readl(CMCLKE) & ~(1 << 0), CMCLKE);
	spin_unlock_irqrestore(&cmt_lock, flags);
}

void clocksource_mmio_suspend(struct clocksource *cs)
{
	if (strcmp("cmt10",cs->name) == 0)
		cmt10_stop();
}

void clocksource_mmio_resume(struct clocksource *cs)
{
	if (strcmp("cmt10",cs->name) == 0)
		cmt10_start(false);
}

/* do nothing for !CONFIG_SMP or !CONFIG_HAVE_TWD */
void __init __weak r8a7373_register_twd(void) { }


static u32 notrace cmt_read_sched_clock(void)
{
        return __raw_readl(CMCNT0);
}

/**
 * read_persistent_clock -  Return time from a persistent clock.
 *
 * Reads the time from a source which isn't disabled during PM,
 * CMCNT4.  Convert the cycles elapsed since last read into
 * nsecs and adds to a monotonically increasing timespec.
 *
 * Copied from plat-omap. overrides weak definition in timekeeping.c
 */
static struct timespec persistent_ts;
static cycles_t cycles, last_cycles;
static unsigned int persistent_mult, persistent_shift;
void read_persistent_clock(struct timespec *ts)
{
	unsigned long long nsecs;
	cycles_t delta;
	struct timespec *tsp = &persistent_ts;

	last_cycles = cycles;
	cycles = __raw_readl(CMCNT4);
	delta = cycles - last_cycles;

	nsecs = clocksource_cyc2ns(delta, persistent_mult, persistent_shift);

	timespec_add_ns(tsp, nsecs);
	*ts = *tsp;
}

void __init cmt_clocksource_init(void)
{
	struct clk *cp_clk, *r_clk;
	unsigned long flags, rate;


	clk_enable(clk_get_sys("sh_cmt.10", NULL));
	cp_clk = clk_get(NULL, "cp_clk");
	rate = clk_get_rate(cp_clk);
	clk_enable(cp_clk);

	cmt10_start(true);

	clocksource_mmio_init(CMCNT0, "cmt10", rate, 125, 32,
				clocksource_mmio_readl_up);

	setup_sched_clock_needs_suspend(cmt_read_sched_clock, 32, rate);

	clk_enable(clk_get_sys("sh_cmt.14", NULL));
	r_clk = clk_get(NULL, "r_clk");
	clk_enable(r_clk);
	rate = clk_get_rate(r_clk);

	spin_lock_irqsave(&cmt_lock, flags);
	__raw_writel(__raw_readl(CMCLKE) | (1 << 4), CMCLKE);
	spin_unlock_irqrestore(&cmt_lock, flags);

	/* stop */
	__raw_writel(0, CMSTR4);

	/* setup */
	__raw_writel(0, CMCNT4);
	__raw_writel(0x107, CMCSR4); /* Free-running, debug, RCLK/1 */
	__raw_writel(0xffffffff, CMCOR4);
	while (__raw_readl(CMCNT4) != 0)
		;

	/* start */
	__raw_writel(1, CMSTR4);

	/*
	 * 120000 rough estimate from the calculations in
	 * __clocksource_updatefreq_scale.
	 */
	clocks_calc_mult_shift(&persistent_mult, &persistent_shift,
			32768, NSEC_PER_SEC, 120000);

	clocksource_mmio_init(CMCNT4, "cmt14", rate, 50, 32,
					clocksource_mmio_readl_up);
}


static struct cmt_timer_clock cmt1_cks_table[] = {
	[0] = CKS("cp_clk", 8, 512),
	[1] = CKS("cp_clk", 32, 128),
	[2] = CKS("cp_clk", 128, 32),
	[3] = CKS("cp_clk", 1, 4096), /* 0x1000 <=> 315 usecs */
	[4] = CKS("r_clk", 8, 8),
	[5] = CKS("r_clk", 32, 8),
	[6] = CKS("r_clk", 128, 8),
	[7] = CKS("r_clk", 1, 8), /* 0x8 <=> 244 usecs */
	/* Pseudo 32KHz/1 is omitted */
};


/* CMT11, CMT12 clockevent */
static struct cmt_timer_config cmt1_timers[2] = {
	[0] = {
		.res = {
			DEFINE_RES_MEM(0xe6130100, 0x100),
			DEFINE_RES_IRQ(gic_spi(94)),
		},
		.name		= "sh_cmt.11",
		.timer_bit	= 1,
		.cks_table	= cmt1_cks_table,
		.cks_num	= ARRAY_SIZE(cmt1_cks_table),
		.cks		= 3,
		.cmcsr_init	= 0x120, /* Free-run, request interrupt, debug */
	},
	[1] = {
		.res = {
			DEFINE_RES_MEM(0xe6130200, 0x100),
			DEFINE_RES_IRQ(gic_spi(95)),
		},
		.name		= "sh_cmt.12",
		.timer_bit	= 2,
		.cks_table	= cmt1_cks_table,
		.cks_num	= ARRAY_SIZE(cmt1_cks_table),
		.cks		= 3,
		.cmcsr_init	= 0x120, /* Free-run, request interrupt, debug */
	},
};

void __init u2_timers_init(void)
{
	r8a7373_clock_init();
	shmobile_calibrate_delay_early();
	cmt_clocksource_init();
	cmt_clockevent_init(cmt1_timers, 2, 0, CMCLKE_PHYS);
	setup_current_timer();
}
