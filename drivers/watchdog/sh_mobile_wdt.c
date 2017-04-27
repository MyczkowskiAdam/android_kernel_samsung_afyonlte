/*
 * RCLK Watchdog Timer (RWDT) driver for Renesas SH-Mobile processors
 *
 * Copyright (C) 2011  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/moduleparam.h>
#include <linux/clk.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/sh_mobile_wdt.h>

/* register offset */
#define RWDTCNT		0x00
#define RWDTCSRA	0x04
#define RWDTCSRB	0x08

/* parameters */
#define WATCHDOG_TIMEOUT 30		/* 30 sec default timeout */
static int timeout = WATCHDOG_TIMEOUT;
module_param(timeout, int, 0);
MODULE_PARM_DESC(timeout,
	"Watchdog timeout in seconds. (1 <= timeout <= 1800, default="
				__MODULE_STRING(WATCHDOG_TIMEOUT) ")");

static int nowayout = WATCHDOG_NOWAYOUT;
module_param(nowayout, int, 0);
MODULE_PARM_DESC(nowayout,
	"Watchdog cannot be stopped once started (default="
				__MODULE_STRING(WATCHDOG_NOWAYOUT) ")");

static void __iomem	*wdt_base;
static int		wdt_width;
static int		wdt_set_count;
static struct clk	*wdt_clk;
static int		wdt_clk_rate;
static int		wdt_enable;
static unsigned long	wdt_users;
static void		(*wdt_irq_handler)(void);
static DEFINE_SPINLOCK(wdt_lock);

static struct workqueue_struct	*wdt_work;
static struct delayed_work	wdt_delayed_wq;

static u32 mult_table8[] = {8, 60, 600, 1800};
static u8 cks30_table8[] = {0x08, 0x0c, 0x0a, 0x06};
static u8 cks54_table8[] = {0x00, 0x01, 0x02, 0x03};
static u32 mult_table16[] = {256, 1024, 4096};
static u8 cks30_table16[] = {0x01, 0x03, 0x04};
static u8 cks54_table16[] = {0x00, 0x00, 0x00};

static u32 wdt_read(int offset)
{
	switch (offset) {
	case RWDTCNT:
		switch (wdt_width) {
		case 8:
			return __raw_readb(wdt_base + offset);
		default:
			return __raw_readw(wdt_base + offset);
		}
		break;
	default:
		return __raw_readb(wdt_base + offset);
	}
}

static void wdt_write(int offset, u32 val)
{
	if (wdt_width == 8) {
		switch (offset) {
		case RWDTCNT:
			__raw_writew(val | 0x5a00, wdt_base + offset);
			break;
		case RWDTCSRA:
		case RWDTCSRB:
			__raw_writew(val | 0xa500, wdt_base + offset);
			break;
		}
	} else {
		switch (offset) {
		case RWDTCNT:
			__raw_writel(val | 0x5a5a0000, wdt_base + offset);
			break;
		case RWDTCSRA:
		case RWDTCSRB:
			__raw_writel(val | 0xa5a5a500, wdt_base + offset);
			break;
		}
	}
}

static irqreturn_t sh_mobile_wdt_irq(int irq, void *arg)
{
	if (wdt_irq_handler)
		wdt_irq_handler();
	else
		printk(KERN_CRIT "Watchdog Timer expired. Reboot ignored.\n");
	return IRQ_HANDLED;
}

static int sh_mobile_wdt_ping(void)
{
	int ret = 0;

	spin_lock(&wdt_lock);
	if (!(wdt_read(RWDTCSRA) & 0x20))
		wdt_write(RWDTCNT, wdt_set_count);
	else
		ret = -EAGAIN;
	spin_unlock(&wdt_lock);

	return ret;
}

static void sh_mobile_wdt_enable(void)
{
	u32 base_count, time_count, max_count;
	u32 *mult_table, mult;
	u8 *cks30_table, *cks54_table;
	int i, loop_count;
	unsigned long flags;

	spin_lock_irqsave(&wdt_lock, flags);
	if (!wdt_enable) {
		clk_enable(wdt_clk);
		wdt_enable = 1;
	}

	/* Stop */
	wdt_write(RWDTCSRA, 0);

	if (timeout == 0)
		timeout = 1;

	max_count = 1 << wdt_width;

	if (wdt_width == 8) {
		base_count = wdt_clk_rate / 128;
		loop_count = ARRAY_SIZE(mult_table8);
		mult_table = mult_table8;
		cks30_table = cks30_table8;
		cks54_table = cks54_table8;
	} else {
		base_count = wdt_clk_rate * 256 / 128;
		loop_count = ARRAY_SIZE(mult_table16);
		mult_table = mult_table16;
		cks30_table = cks30_table16;
		cks54_table = cks54_table16;
	}

	/* Setting timeout */
	for (i = 0; i < loop_count; i++) {
		mult = mult_table[i];
		time_count = (timeout * base_count) / mult;
		if (time_count <= max_count) {
			wdt_set_count = max_count - time_count;
			timeout = (time_count * mult) / base_count;
			break;
		}
	}
	if (i == loop_count) {
		i--;
		wdt_set_count = 0;
		timeout = (max_count * mult_table[i]) / base_count;
	}

	/* Enable timer */
	wdt_write(RWDTCNT, wdt_set_count);
	wdt_write(RWDTCSRB, (cks54_table[i] << 4) | cks30_table[i]);
	wdt_write(RWDTCSRA, 0x88 | 0x07);

	spin_unlock_irqrestore(&wdt_lock, flags);
}

static void sh_mobile_wdt_disable(void)
{
	unsigned long flags;

	spin_lock_irqsave(&wdt_lock, flags);
	if (!wdt_enable) {
		clk_enable(wdt_clk);
		wdt_enable = 1;
	}

	wdt_write(RWDTCSRA, 0);
	wdt_write(RWDTCNT, 0);

	if (wdt_enable) {
		clk_disable(wdt_clk);
		wdt_enable = 0;
	}
	spin_unlock_irqrestore(&wdt_lock, flags);
}

static void sh_mobile_wdt_work(struct work_struct *work)
{
	unsigned long flags;

	while (wdt_enable && sh_mobile_wdt_ping())
		msleep(1);

	spin_lock_irqsave(&wdt_lock, flags);

	if (wdt_enable)
		queue_delayed_work(wdt_work, &wdt_delayed_wq,
			msecs_to_jiffies(timeout * 1000 / 2));

	spin_unlock_irqrestore(&wdt_lock, flags);
}

static int sh_mobile_wdt_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(1, &wdt_users))
		return -EBUSY;

	sh_mobile_wdt_enable();

	return nonseekable_open(inode, file);
}

static int sh_mobile_wdt_release(struct inode *inode, struct file *file)
{
#ifndef CONFIG_WATCHDOG_NOWAYOUT
	sh_mobile_wdt_disable();
#endif
	wdt_users = 0;
	return 0;
}

static ssize_t sh_mobile_wdt_write(struct file *file, const char __user *data,
		size_t len, loff_t *ppos)
{
	if (len)
		sh_mobile_wdt_ping();
	return len;
}

static const struct watchdog_info ident = {
	.options	= WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING,
	.identity	= "SH-Mobile Watchdog",
};

static long sh_mobile_wdt_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	switch (cmd) {
	case WDIOC_GETSUPPORT:
		return copy_to_user((struct watchdog_info __user *)arg, &ident,
				sizeof(ident));
	case WDIOC_GETSTATUS:
	case WDIOC_GETBOOTSTATUS:
		return put_user(0, (int *)arg);
	case WDIOC_KEEPALIVE:
		sh_mobile_wdt_ping();
		return 0;
	case WDIOC_SETTIMEOUT:
		spin_lock(&wdt_lock);
		if (get_user(timeout, (int __user *)arg)) {
			spin_unlock(&wdt_lock);
			return -EFAULT;
		}
		spin_unlock(&wdt_lock);
		sh_mobile_wdt_disable();
		sh_mobile_wdt_enable();
		/* Fall through */
	case WDIOC_GETTIMEOUT:
		return put_user(timeout, (int __user *)arg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations sh_mobile_wdt_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.write		= sh_mobile_wdt_write,
	.unlocked_ioctl	= sh_mobile_wdt_ioctl,
	.open		= sh_mobile_wdt_open,
	.release	= sh_mobile_wdt_release,
};

static struct miscdevice sh_mobile_wdt_miscdev = {
	.minor = WATCHDOG_MINOR,
	.name = "watchdog",
	.fops = &sh_mobile_wdt_fops,
};

static int __devinit sh_mobile_wdt_probe(struct platform_device *pdev)
{
	struct sh_mobile_wdt_pdata *wdt_pdata = pdev->dev.platform_data;
	struct resource *res, *mem, *wdt_irq;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		return -ENOENT;
	}

	mem = request_mem_region(res->start, resource_size(res), pdev->name);
	if (!mem) {
		dev_err(&pdev->dev, "failed to get memory region\n");
		return -ENOENT;
	}

	wdt_clk = clk_get(&pdev->dev, "rwdt0");
	if (IS_ERR(wdt_clk)) {
		ret = PTR_ERR(wdt_clk);
		goto err1;
	}
	wdt_clk_rate = clk_get_rate(wdt_clk);

	wdt_base = ioremap(res->start, resource_size(res));
	if (!wdt_base) {
		ret = -ENOMEM;
		goto err2;
	}

	wdt_users = 0;
	wdt_enable = 0;

	if (wdt_pdata) {
		switch (wdt_pdata->count_width) {
		case 8:
		case 16:
			wdt_width = wdt_pdata->count_width;
			break;
		default:
			ret = -EINVAL;
			goto err3;
		}
		if (wdt_pdata->timeout)
			timeout = wdt_pdata->timeout;
		wdt_irq_handler = wdt_pdata->wdt_irq_handler;
	} else
		wdt_width = 8;

	sh_mobile_wdt_disable();

	wdt_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (wdt_irq) {
		ret = request_irq(wdt_irq->start, sh_mobile_wdt_irq, 0,
				dev_name(&pdev->dev), NULL);
		if (ret)
			goto err3;
	}

	if (wdt_pdata && wdt_pdata->use_internal) {
		wdt_work = create_workqueue("rwdt");
		if (!wdt_work) {
			ret = -ENOMEM;
			goto err4;
		}
		INIT_DELAYED_WORK(&wdt_delayed_wq, sh_mobile_wdt_work);
		set_bit(1, &wdt_users);
		sh_mobile_wdt_enable();
		queue_delayed_work(wdt_work, &wdt_delayed_wq,
				msecs_to_jiffies(timeout * 1000 / 2));
		dev_info(&pdev->dev,
			"clear internal, timeout=%d sec\n", timeout);
	} else {
		ret = misc_register(&sh_mobile_wdt_miscdev);
		if (ret)
			goto err4;
		dev_info(&pdev->dev, "clear user space (/dev/watchdog)\n");
	}

	return 0;

err4:
	if (wdt_irq)
		free_irq(wdt_irq->start, NULL);
err3:
	iounmap(wdt_base);
err2:
	clk_put(wdt_clk);
err1:
	release_mem_region(res->start, resource_size(res));
	return ret;
}

static int __devexit sh_mobile_wdt_remove(struct platform_device *pdev)
{
	struct resource *res, *wdt_irq;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	wdt_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);

	if (!res)
		return -ENOENT;

	sh_mobile_wdt_disable();

	if (wdt_irq)
		free_irq(wdt_irq->start, NULL);

	if (wdt_work) {
		flush_workqueue(wdt_work);
		destroy_workqueue(wdt_work);
	} else
		misc_deregister(&sh_mobile_wdt_miscdev);

	release_mem_region(res->start, resource_size(res));

	clk_put(wdt_clk);
	iounmap(wdt_base);

	return 0;
}

static void sh_mobile_wdt_shutdown(struct platform_device *pdev)
{
	if (wdt_users)
		sh_mobile_wdt_disable();
}

static int
sh_mobile_wdt_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (wdt_work) {
		sh_mobile_wdt_disable();

		cancel_delayed_work_sync(&wdt_delayed_wq);
		flush_workqueue(wdt_work);
	}

	return 0;
}

static int sh_mobile_wdt_resume(struct platform_device *pdev)
{
	if (wdt_users)
		sh_mobile_wdt_enable();

	if (wdt_work)
		queue_delayed_work(wdt_work, &wdt_delayed_wq,
				msecs_to_jiffies(timeout * 1000 / 2));

	return 0;
}
static struct platform_driver sh_mobile_wdt_driver = {
	.probe		= sh_mobile_wdt_probe,
	.remove		= __devexit_p(sh_mobile_wdt_remove),
	.shutdown	= sh_mobile_wdt_shutdown,
	.suspend	= sh_mobile_wdt_suspend,
	.resume		= sh_mobile_wdt_resume,
	.driver = {
		.name = "sh_mobile_wdt",
		.owner	= THIS_MODULE,
	},
};

static int __init sh_mobile_wdt_init(void)
{
	return platform_driver_register(&sh_mobile_wdt_driver);
}

static void __exit sh_mobile_wdt_exit(void)
{
	platform_driver_unregister(&sh_mobile_wdt_driver);
}

module_init(sh_mobile_wdt_init);
module_exit(sh_mobile_wdt_exit);

MODULE_AUTHOR("Renesas");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(WATCHDOG_MINOR);
MODULE_ALIAS("platform:watchdog");
