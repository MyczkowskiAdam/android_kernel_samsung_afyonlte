/*
 * r8a7373 Power management support
 *
 *  Copyright (C) 2011 Magnus Damm
 *  Copyright (C) 2012  Renesas Electronics Corporation
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/cpuidle.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/suspend.h>
#include <asm/smp_plat.h>
#include <mach/common.h>
#include <mach/r8a7373.h>

#include "pm-r8a7373.h"

/* #define DEBUG_A3SG */
/* #define DEBUG_A3SP */
/* #define DEBUG_A3R */
/* #define DEBUG_A4RM */
/* #define DEBUG_A4MP */

#define PSTR_RETRIES 100
#define PSTR_DELAY_US 10

#ifndef CONFIG_PDC
#ifdef CONFIG_PM

static DEFINE_SPINLOCK(r8a7373_sysc_lock); /* SMP CPUs + I/O devices */

static int pd_power_set(int shift, bool on)
{
	int i = 0;
	unsigned int mask = 1 << shift;
	void __iomem *reg = on ? SWUCR : SPDCR;
	unsigned long flags;

	spin_lock_irqsave(&r8a7373_sysc_lock, flags);

	__raw_writel(mask, reg);

	for (i = 0; i < PSTR_RETRIES ; i++) {
		if (!__raw_readl(reg)) {
			spin_unlock_irqrestore(&r8a7373_sysc_lock, flags);
			return 0;
		}
		udelay(PSTR_DELAY_US);
	}

	spin_unlock_irqrestore(&r8a7373_sysc_lock, flags);

	panic("power status error (bit:%d on:%d PSTR:0x%08x)",
				shift, on, __raw_readl(PSTR)));
	return 0;
}

static int pd_power_down(struct generic_pm_domain *genpd)
{
	struct r8a7373_pm_domain *r8a7373_pd = to_r8a7373_pd(genpd);

	if (r8a7373_pd->debug)
		pr_info("%s: power off\n", genpd->name);
	return pd_power_set(r8a7373_pd->bit_shift, false);
}

static int pd_power_up(struct generic_pm_domain *genpd)
{
	struct r8a7373_pm_domain *r8a7373_pd = to_r8a7373_pd(genpd);

	if (r8a7373_pd->debug)
		pr_info("%s: power on\n", genpd->name);
	return pd_power_set(r8a7373_pd->bit_shift, true);
}

static bool pd_active_wakeup(struct device *dev)
{
	bool (*active_wakeup)(struct device *dev);

	active_wakeup = dev_gpd_data(dev)->ops.active_wakeup;
	return active_wakeup ? active_wakeup(dev) : false;
}

static int pd_stop_dev(struct device *dev)
{
	int (*stop)(struct device *dev);
	int ret;

	stop = dev_gpd_data(dev)->ops.stop;
	if (stop) {
		ret = stop(dev);
		if (ret)
			return ret;
	}
	return pm_clk_suspend(dev);
}

static int pd_start_dev(struct device *dev)
{
	int (*start)(struct device *dev);
	int ret;

	ret = pm_clk_resume(dev);
	if (ret)
		return ret;

	start = dev_gpd_data(dev)->ops.start;
	if (start)
		ret = start(dev);
	return ret;
}

void r8a7373_init_pm_domain(struct r8a7373_pm_domain *r8a7373_pd)
{
	struct generic_pm_domain *genpd = &r8a7373_pd->genpd;
	struct dev_power_governor *gov = r8a7373_pd->gov;

	pm_genpd_init(genpd, gov ? : NULL, false);
	genpd->dev_ops.stop = pd_stop_dev;
	genpd->dev_ops.start = pd_start_dev;
	genpd->dev_ops.active_wakeup = pd_active_wakeup;
	genpd->dev_irq_safe = true;
	genpd->power_off = pd_power_down;
	genpd->power_on = pd_power_up;
	genpd->power_on(genpd);
}

void r8a7373_add_device_to_domain(struct r8a7373_pm_domain *r8a7373_pd,
				struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	pm_genpd_add_device(&r8a7373_pd->genpd, dev);
	if (pm_clk_no_clocks(dev))
		pm_clk_add(dev, NULL);
}

void r8a7373_pm_add_subdomain(struct r8a7373_pm_domain *r8a7373_pd,
				struct r8a7373_pm_domain *r8a7373_sd)
{
	pm_genpd_add_subdomain(&r8a7373_pd->genpd, &r8a7373_sd->genpd);
}

struct r8a7373_pm_domain r8a7373_a3sg = {
	.genpd.name = "A3SG",
	.bit_shift = 18,
#ifdef DEBUG_A3SG
	.debug = true,
#endif
};

struct r8a7373_pm_domain r8a7373_a3sp = {
	.genpd.name = "A3SP",
	.bit_shift = 17,
#ifdef DEBUG_A3SP
	.debug = true,
#endif
};

struct r8a7373_pm_domain r8a7373_a3r = {
	.genpd.name = "A3R",
	.bit_shift = 13,
	.gov = &pm_domain_always_on_gov,
#ifdef DEBUG_A3R
	.debug = true,
#endif
};

struct r8a7373_pm_domain r8a7373_a4rm = {
	.genpd.name = "A4RM",
	.bit_shift = 12,
#ifdef DEBUG_A4RM
	.debug = true,
#endif
};

struct r8a7373_pm_domain r8a7373_a4mp = {
	.genpd.name = "A4MP",
	.bit_shift = 8,
#ifdef DEBUG_A4MP
	.debug = true,
#endif
};

#endif /* CONFIG_PM */
#endif /* CONFIG_PDC */

#if 0 /* #ifdef CONFIG_SUSPEND */
static int r8a7373_enter_suspend(suspend_state_t state)
{
	cpu_do_idle();
	return 0;
}

static void r8a7373_suspend_init(void)
{
	shmobile_suspend_ops.enter = r8a7373_enter_suspend;
}
#else
static void r8a7373_suspend_init(void) {}
#endif /* CONFIG_SUSPEND */

#if defined(CONFIG_SUSPEND) || defined(CONFIG_CPU_IDLE)
void r8a7373_enter_core_standby(void)
{
	unsigned int cpu;
	void __iomem *addr;

	cpu = cpu_logical_map(smp_processor_id());
	addr = cpu ? RAM0_WAKEUP_ADDR1 : RAM0_WAKEUP_ADDR0;
	__raw_writel(virt_to_phys(r8a7373_resume_core_standby), addr);

	cpu_suspend(0, r8a7373_do_idle_core_standby);
}
#endif

/* #ifdef CONFIG_CPU_IDLE */
#if 0
static void r8a7373_cpuidle_setup(struct cpuidle_driver *drv)
{
	struct cpuidle_state *state;
	int i = drv->state_count;

	state = &drv->states[i];
	snprintf(state->name, CPUIDLE_NAME_LEN, "C2");
	strncpy(state->desc, "Core Standby Mode", CPUIDLE_DESC_LEN);
	state->exit_latency = 10;
	state->target_residency = 20 + 10;
	state->flags = CPUIDLE_FLAG_TIME_VALID;
	shmobile_cpuidle_modes[i] = r8a7373_enter_core_standby;

	drv->state_count++;
}

static void  r8a7373_cpuidle_init(void)
{
	shmobile_cpuidle_setup = r8a7373_cpuidle_setup;
}
#else
static void r8a7373_cpuidle_init(void) {}
#endif

void __init r8a7373_pm_init(void)
{
#if defined(CONFIG_SUSPEND) || defined(CONFIG_CPU_IDLE)
	memcpy(RAM0_VECTOR_ADDR,
			r8a7373_common_vector, r8a7373_common_vector_size);
	__raw_writel(0, SBAR2);
	__raw_writel(0, APARMBAREA);
	__raw_writel(RAM0_VECTOR_ADDR_PHYS, SBAR);
#endif

	r8a7373_suspend_init();
	r8a7373_cpuidle_init();
}
