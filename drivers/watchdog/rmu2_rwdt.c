/*
 * rmu2_rwdt.c
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
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

#include <linux/rmu2_rwdt.h>
#include <linux/rmu2_cmt15.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/hwspinlock.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <mach/irqs.h>
#include <mach/r8a7373.h>
#include <mach/sbsc.h>

#include <memlog/memlog.h>

static struct delayed_work *dwork;
static struct delayed_work *dwork_wa_zq;
static struct workqueue_struct *wq;
static struct workqueue_struct *wq_wa_zq;
static struct clk *rmu2_rwdt_clk;
static unsigned long cntclear_time;
static unsigned long cntclear_time_wa_zq;
static unsigned long next_touch_at;
static int stop_func_flg;
static int wa_zq_flg;
static bool running;
static bool startup;

#ifdef CONFIG_RWDT_DEBUG
#define RWDT_DEBUG(fmt, ...)	printk(KERN_DEBUG "" fmt, ##__VA_ARGS__)
#else /* CONFIG_RWDT_DEBUG */
#define RWDT_DEBUG(fmt, ...)
#endif /* CONFIG_RWDT_DEBUG */

/* register address define */
#define REG_SIZE		0xCU
#define RWTCNT_OFFSET           0x0U
#define RWTCSRA_OFFSET		0x4U
#define RWTCSRB_OFFSET		0x8U

/* register mask define */
#define RESCSR_HEADER		0xA5A5A500U
#define RESCNT_INIT_VAL		0xFF00
#define RESCNT_LOW_VAL		0xFF20
#define RESCNT_CLEAR_DATA	(0x5A5A0000U | RESCNT_INIT_VAL)
#define RESCNT2_RWD0A_MASK	0x00003000U
#define RESCNT2_PRES_MASK	0x80000000U
#define RWTCSRA_TME_MASK	0x80U
#define RWTCSRA_WOVF_MASK	0x10U
#define RWTCSRA_WOVFE_MASK	0x08U
#define RWTCSRA_CSK0_MASK	0x07U
#define RWDT_SPI		141U

/* wait time define */
#define WRFLG_WAITTIME		214000	/* [nsec] 7RCLK */

static void __iomem *rwdt_base;
/* SBSC register address */
static void __iomem *sbsc_sdmra_28200;
static void __iomem *sbsc_sdmra_38200;

#define CONFIG_RMU2_RWDT_ZQ_CALIB	(500)

/*
 * Modify register
 */
static DEFINE_SPINLOCK(io_lock);

static void rmu2_modify_register32(void __iomem *addr, u32 clear, u32 set)
{
	unsigned long flags;
	u32 val;
	spin_lock_irqsave(&io_lock, flags);
	val = ioread32(addr);
	val &= ~clear;
	val |= set;
	iowrite32(val, addr);
	spin_unlock_irqrestore(&io_lock, flags);
}

static struct resource rmu2_rwdt_resources[] = {
	[0] = DEFINE_RES_MEM(RWDT_BASE_PHYS, REG_SIZE),
	[1] = DEFINE_RES_IRQ(gic_spi(RWDT_SPI))
};

static struct platform_device rmu2_rwdt_dev = {
	.name = "rwdt",
	.id = -1,
	.num_resources = ARRAY_SIZE(rmu2_rwdt_resources),
	.resource = rmu2_rwdt_resources
};

/*
 * open API
 */
static int rmu2_rwdt_open(struct inode *inode, struct file *filep)
{
	return 0;
}

/*
 * release API
 */
static int rmu2_rwdt_release(struct inode *inode, struct file *filep)
{
	return 0;
}

/*
 * rmu2_ioctl_rwdt: ioctl API
 * input:
 *		@*filep: struct file
 *		@cmd: command
 *		@arg: other argument
 * output:none
 * return:
 *		0: sucessful
 *		-EINVAL: Invalid argument
 */
static long rmu2_ioctl_rwdt(struct file *filep,
				unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	RWDT_DEBUG("ioctl_called!!");
	if (cmd == IOCTL_RWDT_SOFT_RESET)
		rmu2_rwdt_software_reset();
	else
		ret = -EINVAL;

	return ret;
}

/*
 * Kernel Interfaces
 */
static const struct file_operations rwdt_fops = {
	.owner = THIS_MODULE,
	.open = rmu2_rwdt_open,
	.release = rmu2_rwdt_release,
	.unlocked_ioctl = rmu2_ioctl_rwdt,
};

static struct miscdevice rwdt_mdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "rmu2_rwdt",
	.fops = &rwdt_fops,
};

/*
 * rmu2_rwdt_cntclear: RWDT counter clear
 * input: none
 * output: none
 * return:
 *			0: sucessful
 *			-EAGAIN: try again
 */
int rmu2_rwdt_cntclear(void)
{
	static DEFINE_SPINLOCK(clear_lock);
	unsigned long flags;
	int ret;
	unsigned char reg8;
	unsigned int wrflg;

	if (!running)
		return -ENODEV;

#ifdef CONFIG_RWDT_CMT15_TEST
	if (test_mode == TEST_NO_KICK)
		return 0;
#endif

	/* If we collide with the other core, just leave it to them */
	if (!spin_trylock_irqsave(&clear_lock, flags)) {
		return 0;
	}

	/* check RWTCSRA wrflg */
	reg8 = __raw_readb(rwdt_base + RWTCSRA_OFFSET);
	wrflg = ((u32)reg8 >> 5) & 0x01U;
	if (0U == wrflg) {
		/*RWDT_DEBUG(KERN_DEBUG "Clear the watchdog counter!!\n");*/
		__raw_writel(RESCNT_CLEAR_DATA, rwdt_base + RWTCNT_OFFSET);
		memory_log_timestamp(RWDT_TIMESTAMP);
		ret = 0;
	} else {
		ret = -EAGAIN; /* try again */
	}
	spin_unlock_irqrestore(&clear_lock, flags);
	return ret;
}

/*
 * rmu2_rwdt_stop: stop counter
 * input: none
 * output: none
 * return: none
 */
int rmu2_rwdt_stop(void)
{
	int ret = 0;
	u8 reg8;
	u32 reg32;

	RWDT_DEBUG("START < %s >\n", __func__);

	stop_func_flg = 1;

	running = false;

	cancel_delayed_work_sync(dwork);
	flush_workqueue(wq);
	if (wa_zq_flg) {
		cancel_delayed_work_sync(dwork_wa_zq);
		flush_workqueue(wq_wa_zq);
	}

	/* set 0 to RWTCSRA TME for stopping timer */
	reg8 = __raw_readb(rwdt_base + RWTCSRA_OFFSET);
	reg32 = (u32)reg8;
	reg32 &= ~(RWTCSRA_TME_MASK);
	reg32 |= RESCSR_HEADER;
	__raw_writel(reg32, rwdt_base + RWTCSRA_OFFSET);

	ndelay(WRFLG_WAITTIME); /* wait 7RCLK for module stop */

	/* module stop */
	clk_disable(rmu2_rwdt_clk);

	stop_func_flg = 0;

	return ret;
}

/* ES2.02 / LPDDR2 ZQ Calibration Issue WA */
static void rmu2_rwdt_workfn_zq_wa(struct work_struct *work)
{
	__raw_writel(SBSC_SDMRA_DONE, sbsc_sdmra_28200);
	__raw_writel(SBSC_SDMRA_DONE, sbsc_sdmra_38200);
	RWDT_DEBUG("< %s > CPG_PLL3CR_WDT 0x%8x\n",
			__func__, __raw_readl(CPG_PLL3CR_WDT));

	if (0 == stop_func_flg)	/* do not execute while stop()*/
		queue_delayed_work(wq_wa_zq, dwork_wa_zq, cntclear_time_wa_zq);
}

/*
 * rmu2_rwdt_workfn: work function of RWDT
 * input:
 *		@work: work struct
 * output: none
 * return: none
 */
static void rmu2_rwdt_workfn(struct work_struct *work)
{
	int ret;

#ifdef CONFIG_RWDT_CMT15_TEST
	switch (test_mode) {
	case TEST_NO_KICK:
		printk(KERN_DEBUG "Skip clearing RWDT for debug!\n");
		return;
	case TEST_WORKQUEUE_LOOP:
		rmu2_cmt_loop((void *) 0);
		break;
	}
#endif
	/* Printk() was removed from here intentionally - watchdog
	*  timers have no business printing anything because the
	*  printk() can take so long in high load cases that we
	*  get a watchdog reset due to that!
	*/

#ifdef CONFIG_IRQ_TRACE
	{
		unsigned int val;
		/* Get Time Stamp value */
		val = __raw_readl(CMCNT3);
		printk(KERN_DEBUG "< %s > CMTCNT3=%08x\n", __func__, val);

		if (smp_processor_id())
			/* Store Time Stamp value in CPU1 Time Stamp location
				in History Information Area */
			__raw_writel(val, (tmplog_nocache_address +
				TMPLOG_SIZE_PERCPU + TMPLOG_TIME_OFFSET));
		else
			/* Store Time Stamp value in CPU0 Time Stamp location
				in History Information Area */
			__raw_writel(val, (tmplog_nocache_address +
				TMPLOG_TIME_OFFSET));
	}
#endif /* CONFIG_IRQ_TRACE */
	cpg_check_check();

	preempt_disable();
	ret = rmu2_rwdt_cntclear();
	if (0 > ret) {
		ret = -EBUSY;			/* cannot write RWTCNT  */
		printk(KERN_ERR
		"< %s > rmu2_rwdt_cntclear() = %d ->HARDWARE ERROR\n",
		__func__, ret);
	} else {
		next_touch_at = jiffies + cntclear_time / 4;
	}
	rmu2_cmt_clear();

	if (0 == stop_func_flg)	/* do not execute while stop() */
		queue_delayed_work(wq, dwork, cntclear_time);
	preempt_enable();
}

#ifndef CONFIG_RMU2_RWDT_REBOOT_ENABLE
/*
 * rmu2_rwdt_irq: implement for RWDT's IRQ
 * input:
 *		@irq: interrupt number
 *		@dev_id: device ID
 * output: none
 * return:
 *			IRQ_HANDLED: irq handled
 */
static irqreturn_t rmu2_rwdt_irq(int irq, void *dev_id)
{
	printk(KERN_ERR "RWDT Counter Overflow Occur!! Start Crash Dump\n");
	/* __raw_readl(IO_MEM(0x00000000)); */

	return IRQ_HANDLED;
}

/*
 * rmu2_rwdt_init_irq: initialization handler for RWDT's IRQ
 * input: none
 * output: none
 * return:
 *			0: sucessful
 *			-EINVAL: Invalid argument
 *			-ENOMEM: Out of memory
 *			-ENOSYS: Function not implemented
 */
static int rmu2_rwdt_init_irq(void)
{
	int ret = 0;
	unsigned int irq;
	struct resource *r;

	RWDT_DEBUG("START < %s >\n", __func__);

	r = platform_get_resource(&rmu2_rwdt_dev, IORESOURCE_IRQ, 0);
	if (NULL == r) {
		ret = -ENOMEM;
		printk(KERN_ERR
		"%s:%d platform_get_resource failed err=%d\n",
		__func__, __LINE__, ret);
		return ret;
	}
	irq = r->start;

	set_irq_flags(irq, IRQF_VALID | IRQF_NOAUTOEN);

	ret = request_irq(irq, rmu2_rwdt_irq, IRQF_DISABLED,
						"RWDT0", (void *)irq);
	if (0 > ret) {
		printk(KERN_ERR
		"%s:%d request_irq failed err=%d\n",
		__func__, __LINE__, ret);
		free_irq(irq, (void *)irq);

		return ret;
	}

	enable_irq(irq);

	return ret;
}
#endif /* CONFIG_RMU2_RWDT_REBOOT_ENABLE */

/*
 * rmu2_rwdt_start: start RWDT
 * input: none
 * output: none
 * return:
 *			0: sucessful
 *			-ENOMEM: Out of memory
 *			-EBUSY: Device or resource busy
 */
static int rmu2_rwdt_start(void)
{
	int ret = 0;
	u8 reg8;
	u16 clockSelect;
	u32 reg32;
	int hwlock;

	RWDT_DEBUG("START < %s >\n", __func__);

	for (;;) {
		hwlock = hwspin_lock_timeout_irq(r8a7373_hwlock_sysc, 1);
		if (0 == hwlock)
			break;
	}
	/* set 11 to SYSC RESCNT2 RWD0A for selecting soft power on reset */
#ifdef CONFIG_RMU2_RWDT_REBOOT_ENABLE
	rmu2_modify_register32(RESCNT2, 0x00000000, RESCNT2_RWD0A_MASK);
#else	/* CONFIG_RMU2_RWDT_REBOOT_ENABLE */
	rmu2_modify_register32(RESCNT2, RESCNT2_RWD0A_MASK, 0x00000000);
#endif	/* CONFIG_RMU2_RWDT_REBOOT_ENABLE */

	hwspin_unlock_irq(r8a7373_hwlock_sysc);

	/* module stop release */
	clk_enable(rmu2_rwdt_clk);

	reg8 = __raw_readb(rwdt_base + RWTCSRA_OFFSET);
	reg8 &= ~(RWTCSRA_TME_MASK);
	reg32 = RESCSR_HEADER + (u32)reg8;
	__raw_writel(reg32, rwdt_base + RWTCSRA_OFFSET);

	reg8 = __raw_readb(rwdt_base + RWTCSRA_OFFSET);
	reg8 &= ~(RWTCSRA_WOVF_MASK);
	reg32 = RESCSR_HEADER + (u32)reg8;
	__raw_writel(reg32, rwdt_base + RWTCSRA_OFFSET);

#ifndef CONFIG_RMU2_RWDT_REBOOT_ENABLE
	/* set 1 to WTCSRA WOVFE for overflow interrupt enable */
	reg8 = __raw_readb(rwdt_base + RWTCSRA_OFFSET);
	reg8 |= RWTCSRA_WOVFE_MASK;
	reg32 = RESCSR_HEADER + (u32)reg8;
	__raw_writel(reg32, rwdt_base + RWTCSRA_OFFSET);
#endif	/* CONFIG_RMU2_RWDT_REBOOT_ENABLE */

	clockSelect = CONFIG_RMU2_RWDT_OVF;

	reg8 = __raw_readb(rwdt_base + RWTCSRA_OFFSET);
	reg8 &= ~RWTCSRA_CSK0_MASK;
	reg8 |= ((clockSelect >> 8) & 0x00FFU);
	reg32 = RESCSR_HEADER + (u32)reg8;
	__raw_writel(reg32, rwdt_base + RWTCSRA_OFFSET);

	reg8 = (clockSelect & 0x00FFU);
	reg32 = RESCSR_HEADER + (u32)reg8;
	__raw_writel(reg32, rwdt_base + RWTCSRB_OFFSET);

	running = true;
	startup = true;

	/* clear RWDT counter */
	ret = rmu2_rwdt_cntclear();

	if (0 > ret) {
		/* retry */
		printk(KERN_ERR
		"< %s > rmu2_rwdt_cntclear() = %d ->Try again\n",
		__func__, ret);
		ndelay(WRFLG_WAITTIME); /* 7RCLK wait for counter clear */
		ret = rmu2_rwdt_cntclear();

		if (0 > ret) {
			ret = -EBUSY; /* cannot write RWTCNT  */
			printk(KERN_ERR
			"< %s > rmu2_rwdt_cntclear() = %d ->HARDWARE ERROR\n",
			__func__, ret);

			/* module stop */
			clk_disable(rmu2_rwdt_clk);

			return ret;
		}
	}

	startup = false;

	/* start soft timer */
	queue_delayed_work(wq, dwork, cntclear_time);
	if (wa_zq_flg)
		queue_delayed_work(wq_wa_zq, dwork_wa_zq, cntclear_time_wa_zq);

	/* set 1 to RWTCSRA TME for starting timer */
	reg8 = __raw_readb(rwdt_base + RWTCSRA_OFFSET);
	reg8 |= RWTCSRA_TME_MASK;
	reg32 = RESCSR_HEADER + (u32)reg8;
	__raw_writel(reg32, rwdt_base + RWTCSRA_OFFSET);

	return ret;
}

/*
 * rmu2_rwdt_probe:
 * input:
 *		@pdev: platform device
 * output:
 * return:
 */
static int __devinit rmu2_rwdt_probe(struct platform_device *pdev)
{
	int ret = 0;
	u8 reg8;
	u32 reg32;
	struct resource *r;
	void __iomem *sbsc_sdmracr1a = NULL;

	RWDT_DEBUG("START < %s >\n", __func__);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == r) {
		ret = -ENOMEM;
		printk(KERN_ERR
		"%s:%d platform_get_resource failed err=%d\n",
		__func__, __LINE__, ret);
		goto clk_get_err;
	}
	rwdt_base = devm_request_and_ioremap(&pdev->dev, r);
	if (NULL == rwdt_base) {
		ret = -ENOMEM;
		printk(KERN_ERR
		"%s:%d devm_request_and_ioremap failed err=%d\n",
		__func__, __LINE__, ret);
		goto clk_get_err;
	}

	rmu2_rwdt_clk = clk_get(NULL, "rwdt0");
	if (true == IS_ERR(rmu2_rwdt_clk)) {
		ret = PTR_ERR(rmu2_rwdt_clk);
		printk(KERN_ERR "< %s > ret = %d clk_get err\n", __func__, ret);
		goto clk_get_err;
	}

	/* module stop release */
	clk_enable(rmu2_rwdt_clk);

	/* set 0 to WTCSRA WOVF for clearing WOVF */
	reg8 = __raw_readb(rwdt_base + RWTCSRA_OFFSET);
	reg32 = (u32)reg8;
	reg32 &= ~(RWTCSRA_WOVF_MASK);
	reg32 |= RESCSR_HEADER;
	__raw_writel(reg32, rwdt_base + RWTCSRA_OFFSET);

	/* module stop */
	clk_disable(rmu2_rwdt_clk);

	stop_func_flg = 0;

	/* ES2.02 / LPDDR2 ZQ Calibration Issue WA */
	wa_zq_flg = 0;
	reg8 = __raw_readb(STBCHRB3);
	if ((reg8 & 0x80) && ((system_rev & 0xFFFF) >= 0x3E12)) {
		RWDT_DEBUG("< %s > Apply for ZQ calibration\n", __func__);

		sbsc_sdmracr1a   = ioremap(SBSC_BASE + 0x000088, 0x4);
		sbsc_sdmra_28200 = ioremap(SBSC_BASE + 0x128200, 0x4);
		sbsc_sdmra_38200 = ioremap(SBSC_BASE + 0x138200, 0x4);
		if (sbsc_sdmracr1a && sbsc_sdmra_28200 && sbsc_sdmra_38200) {
			wa_zq_flg = 1;
			__raw_writel(SBSC_SDMRACR1A_ZQ, sbsc_sdmracr1a);
			__raw_writel(SBSC_SDMRA_DONE, sbsc_sdmra_28200);
			__raw_writel(SBSC_SDMRA_DONE, sbsc_sdmra_38200);

			if (sbsc_sdmracr1a) {
				iounmap(sbsc_sdmracr1a);
				sbsc_sdmracr1a = NULL;
			}

			cntclear_time_wa_zq =
					msecs_to_jiffies(CONFIG_RMU2_RWDT_ZQ_CALIB);
			wq_wa_zq = alloc_workqueue("rmu2_rwdt_queue_wa_zq",
						WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
			if (NULL == wq_wa_zq) {
				ret = -ENOMEM;
				printk(KERN_ERR
				"< %s > ret = %d alloc_workqueue err\n", __func__, ret);
				goto create_workqueue_err;
			}

			dwork_wa_zq = kzalloc(sizeof(*dwork_wa_zq), GFP_KERNEL);
			if (NULL == dwork_wa_zq) {
				ret = -ENOMEM;
				printk(KERN_ERR
					"< %s > ret = %d dwork_wa_zq kzalloc err\n",
					__func__, ret);
				goto dwork_err;
			}
			INIT_DELAYED_WORK(dwork_wa_zq, rmu2_rwdt_workfn_zq_wa);
		} else {
			RWDT_DEBUG("%s: ioremap failed.\n", __func__);
			goto ioremap_err;
		}
	}

	if (1U > num_online_cpus()) {
		ret = -ENXIO;
		printk(KERN_ERR
		"< %s > ret = %d num_online_cpus err\n", __func__, ret);
		goto num_online_cpus_err;
	}
	/* set counter clear time */
	cntclear_time = msecs_to_jiffies(CONFIG_RMU2_RWDT_CLEARTIME);
	next_touch_at = jiffies + cntclear_time / 4;

	wq = alloc_workqueue("rmu2_rwdt_queue",
				WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (NULL == wq) {
		ret = -ENOMEM;
		printk(KERN_ERR
		"< %s > ret = %d alloc_workqueue err\n", __func__, ret);
		goto create_workqueue_err;
	}

	dwork = kzalloc(sizeof(*dwork), GFP_KERNEL);
	if (NULL == dwork) {
		ret = -ENOMEM;
		printk(KERN_ERR
		"< %s > ret = %d dwork kzalloc err\n", __func__, ret);
		goto dwork_err;
	}

	INIT_DELAYED_WORK(dwork, rmu2_rwdt_workfn);

	/* Device Registration */
	ret = misc_register(&rwdt_mdev);
	if (0 > ret)
		return ret;

#ifndef CONFIG_RMU2_RWDT_REBOOT_ENABLE
	ret = rmu2_rwdt_init_irq();
#endif /* CONFIG_RMU2_RWDT_REBOOT_ENABLE */

	if (0 <= ret) {
		ret = rmu2_rwdt_start();
		if (0 <= ret)
			return ret;
	}

	kfree(dwork);
	if (wa_zq_flg)
		kfree(dwork_wa_zq);
dwork_err:
	flush_workqueue(wq);
	destroy_workqueue(wq);
	if (wa_zq_flg) {
		flush_workqueue(wq_wa_zq);
		destroy_workqueue(wq_wa_zq);
	}
create_workqueue_err:
num_online_cpus_err:
	clk_put(rmu2_rwdt_clk);
clk_get_err:
ioremap_err:
	if (sbsc_sdmra_28200) {
		iounmap(sbsc_sdmra_28200);
		sbsc_sdmra_28200 = NULL;
	}
	if (sbsc_sdmra_38200) {
		iounmap(sbsc_sdmra_38200);
		sbsc_sdmra_38200 = NULL;
	}

	return ret;
}

/*
 * rmu2_rwdt_remove:
 * input:
 *		@pdev: platform device
 * output:none
 * return:
 *		0:sucessful
 */
static int __devexit rmu2_rwdt_remove(struct platform_device *pdev)
{
	RWDT_DEBUG("START < %s >\n", __func__);

	rmu2_rwdt_stop();
	kfree(dwork);
	destroy_workqueue(wq);
	if (wa_zq_flg) {
		kfree(dwork_wa_zq);
		destroy_workqueue(wq_wa_zq);
		if (sbsc_sdmra_28200) {
			iounmap(sbsc_sdmra_28200);
			sbsc_sdmra_28200 = NULL;
		}
		if (sbsc_sdmra_38200) {
			iounmap(sbsc_sdmra_38200);
			sbsc_sdmra_38200 = NULL;
		}
	}

	return 0;
}

/*
 * rmu2_rwdt_suspend:
 * input:
 *		@pdev: platform device
 *		@state: power management
 * output:none
 * return:
 *		0:sucessful
 *		-EAGAIN: try again
 */
static int rmu2_rwdt_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret;
	RWDT_DEBUG("START < %s >\n", __func__);

	rmu2_cmt_clear();

	/* clear RWDT counter */
	ret = rmu2_rwdt_cntclear();

	if (0 > ret) {
		/* retry */
		printk(KERN_ERR
		"< %s > rmu2_rwdt_cntclear() =  %d ->Try again\n",
		__func__, ret);
		ndelay(WRFLG_WAITTIME); /* 7RCLK wait for counter clear */
		ret = rmu2_rwdt_cntclear();

		if (0 > ret) {
			ret = -EBUSY; /* cannot write RWTCNT */
			printk(KERN_ERR
			"< %s > rmu2_rwdt_cntclear() = %d ->HARDWARE ERROR\n",
			__func__, ret);
			return ret;
		}
	}

	cancel_delayed_work_sync(dwork);
	flush_workqueue(wq);
	if (wa_zq_flg) {
		cancel_delayed_work_sync(dwork_wa_zq);
		flush_workqueue(wq_wa_zq);
	}

	return 0;
}

/*
 * rmu2_rwdt_resume:
 * input:
 *		@pdev: platform device
 * output:none
 * return:
 *		0:sucessful
 *		-EAGAIN: try again
 */
static int rmu2_rwdt_resume(struct platform_device *pdev)
{
	int ret;
	RWDT_DEBUG("START < %s >\n", __func__);

	rmu2_cmt_clear();

	/* clear RWDT counter */
	ret = rmu2_rwdt_cntclear();

	if (0 > ret) {
		/* retry */
		printk(KERN_ERR
		"< %s > rmu2_rwdt_cntclear() = %d ->Try again\n",
		__func__, ret);
		ndelay(WRFLG_WAITTIME); /* 7RCLK wait for counter clear */
		ret = rmu2_rwdt_cntclear();

		if (0 > ret) {
			ret = -EBUSY; /* cannot write RWTCNT */
			printk(KERN_ERR
			"< %s > rmu2_rwdt_cntclear() = %d ->HARDWARE ERROR\n",
			__func__, ret);
			return ret;
		}
	}

	/* start soft timer */
	queue_delayed_work(wq, dwork, cntclear_time);
	if (wa_zq_flg)
		queue_delayed_work(wq_wa_zq, dwork_wa_zq, cntclear_time_wa_zq);

	return 0;
}

static struct platform_driver rmu2_rwdt_driver = {
	.probe		= rmu2_rwdt_probe,
	.remove		= rmu2_rwdt_remove,
	.suspend	= rmu2_rwdt_suspend,
	.resume		= rmu2_rwdt_resume,

	.driver		= {
		.name	= "rwdt",
	}

};

/*
 * init routines
 */
static int __init rmu2_rwdt_init(void)
{
	int ret;

	RWDT_DEBUG("START < %s >\n", __func__);

	ret = platform_device_register(&rmu2_rwdt_dev);
	if (0 > ret) {
		printk(KERN_ERR "< %s > platform_device_register() = %d\n",
						__func__, ret);
		return ret;
	}

	ret = platform_driver_register(&rmu2_rwdt_driver);
	if (0 > ret) {
		printk(KERN_ERR "< %s > platform_driver_register() = %d\n",
						__func__, ret);
		return ret;
	}

	return ret;
}

/*
 * exit routines
 */
static void __exit rmu2_rwdt_exit(void)
{
	RWDT_DEBUG("START < %s >\n", __func__);

	platform_driver_unregister(&rmu2_rwdt_driver);
	platform_device_unregister(&rmu2_rwdt_dev);
}

/*
 * rmu2_rwdt_software_reset: software reset
 * input:none
 * output:none
 * return:none
 */
void rmu2_rwdt_software_reset(void)
{
	u8 reg = 0;
	int hwlock;
	unsigned long flags;
	/* set 0x22 to STBCHRB1(0xE6180041) */
	/* __raw_writeb(0x22, STBCHRB1); */

	reg = __raw_readb(STBCHR2); /* read STBCHR2 for debug */
	__raw_writeb((reg | APE_RESETLOG_RWDT_SOFTWARE_RESET),
					STBCHR2); /* write STBCHR2 for debug */
	/* execute software reset by setting 0x80000000 to RESCNT2 */

	for (;;) {
		hwlock = hwspin_lock_timeout_irqsave(r8a7373_hwlock_sysc, 1,
							&flags);
		if (0 == hwlock) {
			RWDT_DEBUG(">>> %s Get lock in loop successfully\n",
							__func__);
			break;
		}
	}
	rmu2_modify_register32(RESCNT2, RESCNT2_PRES_MASK,
							RESCNT2_PRES_MASK);

	hwspin_unlock_irqrestore(r8a7373_hwlock_sysc, &flags);
}

#ifndef CONFIG_LOCKUP_DETECTOR
void touch_softlockup_watchdog(void)
{
	if (time_is_after_jiffies(next_touch_at))
		return;

	rmu2_cmt_clear();
	if (rmu2_rwdt_cntclear() >= 0)
		next_touch_at = jiffies + cntclear_time / 4;
}

void touch_all_softlockup_watchdogs(void)
{
	touch_softlockup_watchdog();
}

void lockup_detector_init(void)
{
}
#endif

subsys_initcall(rmu2_rwdt_init);
module_exit(rmu2_rwdt_exit);

MODULE_AUTHOR("Renesas Mobile Corp.");
MODULE_DESCRIPTION("RWDT driver for R-Mobile-U2");
MODULE_LICENSE("GPL");
