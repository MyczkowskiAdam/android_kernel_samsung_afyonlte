/*
 * Renesas SH-/R-Mobile bus semaphore driver
 *
 * Copyright (C) 2012  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/platform_device.h>
#include <linux/platform_data/rmobile_hwsem.h>
#include <linux/lockdep.h>
#include "hwspinlock_internal.h"

/*
 * Implementation of SH-/R-Mobile bus semaphore without interrupts.
 * The only masterID we allow is '0x40' to force people to use HPB
 * bus semaphore for synchronisation between processors rather than
 * processes on the ARM core.
 */

/*
 * Known/available HPB bus semaphores on SH-/R-Mobile SoCs
 *
 *               SH-Mobile  SH-Mobile  R-Mobile   R-Mobile   R-Mobile
 * H'E600_xxxx   G4         AP4        APE5R      A1         U2
 * -------------------------------------------------------------------
 *      0x1600   MPSRC      MPSRC      MPSRC      MPSRC      MPSRC
 *      0x1604   MPACCTL    MPACCTL    MPACCTL    MPACCTL    MPACCTL
 *
 *      0x1820   SMGPIOxxx  SMGPIOxxx  SMGPIOxxx  SMGPIOxxx  SMGPIOxxx
 *      0x1830   SMDBGxxx   SMDBGxxx   -          SMDBGxxx   SMGP0xxx(*)
 *      0x1840   SMCMT2xxx  SMCMT2xxx  SMCMT2xxx  SMCMT2xxx  SMGP1xxx(*)
 *      0x1850   SMCPGAxxx  SMCPGxxx   SMCPGxxx   SMCPGxxx   SMCPGxxx
 *      0x1860   SMCPGBxxx  -          -          -          -
 *      0x1870   SMSYSCxxx  SMSYSCxxx  SMSYSCxxx  SMSYSCxxx  SMSYSCxxx
 *
 * (*) General-purpose bus semaphore
 */
#define NR_HPB_SEMAPHORES	6
#define NR_SMGP_SEMAPHORES	32

/*
 * SrcID of the AP-System CPU on the SHwy bus
 *
 * Bus semaphore should only be used to synchronise operations between
 * the Cortex-A9 core(s) and the other CPUs.  Hence forcing the masterID
 * to a preset value.
 */
#define HWSEM_MASTER_ID		0x40

#define MPSRC			0x00
#define MPACCTL			0x04

#define SMxxSRC			0x00
#define SMxxERR			0x04
#define SMxxTIME		0x08
#define SMxxCNT			0x0C

/*
 * private data structure for a 'struct hwspinlock' instance
 */
struct hwspinlock_private {
	void __iomem		*sm_base;
	void __iomem		*ext_base;
};

/*
 * We don't get SMP/IRQ protection from the core framework for the
 * HW semaphores backing our extension semaphores, so provide our
 * own lock here. Formally we need one spinlock per underlying
 * HW semaphore, but as we only hold it very briefly, one single
 * lock for the entire extension framework should be good enough.
 */
static DEFINE_SPINLOCK(ext_spinlock);

static int hwsem_trylock(struct hwspinlock *lock)
{
	struct hwspinlock_private *p = lock->priv;
	u32 smsrc;

	/*
	 * Taking a HW lock with IRQs enabled could mean that we hold it
	 * long enough to break real-time parts of the system.
	 */
	WARN_ON_ONCE(!irqs_disabled());

	/*Check if the semaphore is open*/
	smsrc = __raw_readl(p->sm_base + SMxxSRC) >> 24;

	if (smsrc == 0) {
		__raw_writel(1, p->sm_base + SMxxSRC); /* SMGET */

	/*
	 * Get upper 8 bits and compare to master ID.
	 * If equal, we have the semaphore, otherwise someone else has it.
	 *
	 * For ARM MPcore systems after R-Mobile U2, each CPU core may be
	 * given a distinct SrcID of the SHwy bus, so master ID matching
	 * condition needs to be relaxed; ignore lower 2 bits of SMSRC.
	 *
	 * Note that this is only safe if we know another APE core cannot be
	 * holding the semaphore - this guarantee is provided by the spinlock
	 * in the hwspinlock object for the main semaphores, or by the
	 * static ext_spinlock for the extension semaphores.
	 */
	smsrc = (__raw_readl(p->sm_base + SMxxSRC) >> 24) & 0xfc;

	mb();
	return smsrc == HWSEM_MASTER_ID;
	} else
		return 0;
}

static void hwsem_unlock(struct hwspinlock *lock)
{
	struct hwspinlock_private *p = lock->priv;

	mb();
	__raw_writel(0, p->sm_base + SMxxSRC);
	__raw_readl(p->sm_base + SMxxSRC);
}

/*
 *  hwsem_get_lock_id: Get HW semaphore ID
 *  return:
 *      0x00: Open state
 *		0x01: AP Realtime side
 *		0x40: AP System side
 *		0x93: Baseband side
 */
static u32 hwsem_get_lock_id(struct hwspinlock *lock)
{
	struct hwspinlock_private *p = lock->priv;
	return (__raw_readl(p->sm_base + SMxxSRC) >> 24);
}

static void hwsem_relax(struct hwspinlock *lock)
{
	ndelay(50);
}

/*
 * General-purpose bus semaphore extension
 */
static int hwsem_ext_trylock(struct hwspinlock *lock)
{
	struct hwspinlock_private *p = lock->priv;
	unsigned long value, flags;
	int ret = 0;

	spin_lock_irqsave(&ext_spinlock, flags);

	/* check to see if software semaphore bit is already set to be done
	BEFORE getting the HW semaphore */
	value = __raw_readl(p->ext_base);
	if (value & 0xff)
		goto out_unlocked; /* no need to get HW sem, failure case */

	if (!hwsem_trylock(lock))
		goto out_unlocked;

	/* check to see if software semaphore bit is already set */
	value = __raw_readl(p->ext_base);

	if (value & 0xff)
		goto out;

	value |= HWSEM_MASTER_ID;

	__raw_writel(value, p->ext_base);
	__raw_readl(p->ext_base); /* defeat write posting */
	ret = 1;

out:
	hwsem_unlock(lock);
out_unlocked:
	spin_unlock_irqrestore(&ext_spinlock, flags);
	return ret;
}

static void hwsem_ext_unlock(struct hwspinlock *lock)
{
	struct hwspinlock_private *p = lock->priv;
	unsigned long expire, mask, value, flags;

	/* try to lock hwspinlock with timeout limit */
	expire = msecs_to_jiffies(100) + jiffies;
	for (;;) {
		spin_lock_irqsave(&ext_spinlock, flags);
		if (hwsem_trylock(lock))
			break;

		spin_unlock_irqrestore(&ext_spinlock, flags);
		if (time_is_before_eq_jiffies(expire)) {
			dev_err(lock->bank->dev, "Timeout to lock hwspinlock\n");
			return;
		}

		hwsem_relax(lock);
	}

	mask = 0xff;

	value = __raw_readl(p->ext_base);
	if (unlikely((value & mask) == 0)) {
		dev_warn(lock->bank->dev,
			 "Trying to unlock hwspinlock %d without lock\n",
			 hwlock_to_id(lock));
		goto out;
	}

	if (unlikely((value & mask) != HWSEM_MASTER_ID)) {
		dev_err(lock->bank->dev,
			 "Trying to unlock hwspinlock %d not for ARM (%08lx)\n",
			 hwlock_to_id(lock), value);
		/*
		 * Even if it's not for ARM, a general purpose semaphore
		 * has been taken successfully, so we're going to unlock
		 * this software semaphore anyway.
		 */
	}

	value &= ~mask;
	__raw_writel(value, p->ext_base);
	__raw_readl(p->ext_base); /* defeat write posting */

 out:
	hwsem_unlock(lock);
	spin_unlock_irqrestore(&ext_spinlock, flags);
}

/*
 *  hwsem_ext_get_lock_id: Get SW semaphore ID
 *  return:
 *      0x00: Open state
 *		0x01: AP Realtime side
 *		0x40: AP System side
 *		0x93: Baseband side
 */
static u32 hwsem_ext_get_lock_id(struct hwspinlock *lock)
{
	struct hwspinlock_private *p = lock->priv;
	unsigned long value;

	/* check to see if software semaphore bit is already set */
	value = __raw_readl(p->ext_base);

	value = value & 0xff;

	return value;
}

static const struct hwspinlock_ops rmobile_hwspinlock_ops = {
	.trylock		= hwsem_trylock,
	.unlock			= hwsem_unlock,
	.relax			= hwsem_relax,
	.get_lock_id	= hwsem_get_lock_id,
};

static const struct hwspinlock_ops rmobile_hwspinlock_ext_ops = {
	.trylock		= hwsem_ext_trylock,
	.unlock			= hwsem_ext_unlock,
	.relax			= hwsem_relax,
	.get_lock_id	= hwsem_ext_get_lock_id,
};

static int __devinit rmobile_hwsem_probe(struct platform_device *pdev)
{
	struct hwsem_pdata *pdata = pdev->dev.platform_data;
	struct hwspinlock_device *bank;
	const struct hwspinlock_ops *ops;
	struct hwspinlock *hwlock;
	struct hwspinlock_private *priv;
	struct resource *res;
	void __iomem *io_base, *ext_base;
	int i, ret, num_locks;

	if (!pdata || !pdata->descs)
		return -EINVAL;

	if ((pdata->nr_descs == 0) || (pdata->nr_descs > NR_SMGP_SEMAPHORES))
		return -EINVAL;

	num_locks = pdata->nr_descs;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	io_base = ioremap(res->start, resource_size(res));
	if (!io_base)
		return -ENOMEM;

	ops = &rmobile_hwspinlock_ops;
	ext_base = NULL;

	/* check to see if general-purpose bus semaphore extension is used */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		ext_base = ioremap(res->start, resource_size(res));
		if (!ext_base) {
			ret = -ENXIO;
			goto iounmap_base;
		}

		ops = &rmobile_hwspinlock_ext_ops;
	}

	/* allocate hwspinlock_device + (hwspinlock * num_locks) */
	bank = kzalloc(sizeof(*bank) + num_locks * sizeof(*hwlock), GFP_KERNEL);
	if (!bank) {
		ret = -ENOMEM;
		goto iounmap_base;
	}

	bank->bank_data = io_base;

	/* allocate (hwspinlock_private * num_locks) */
	priv = kzalloc(num_locks * sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto kfree_bank;
	}

	for (i = 0, hwlock = &bank->lock[0]; i < num_locks; i++, hwlock++) {
		priv[i].sm_base = io_base + pdata->descs[i].offset;
		priv[i].ext_base = ext_base + sizeof(u32) * i;
		hwlock->priv = &priv[i];
	}

	ret = hwspin_lock_register(bank, &pdev->dev, ops,
				   pdata->base_id, num_locks);
	if (ret)
		goto reg_fail;

	platform_set_drvdata(pdev, bank);

	/* set lockdep class */
	for (i = 0; i < num_locks; i++) {
		hwlock = &bank->lock[i];
		lockdep_set_class(&hwlock->lock, &pdata->descs[i].key);
	}
	return 0;

reg_fail:
	kfree(priv);
kfree_bank:
	kfree(bank);
iounmap_base:
	if (ext_base)
		iounmap(ext_base);
	iounmap(io_base);
	return ret;
}

static int __devexit rmobile_hwsem_remove(struct platform_device *pdev)
{
	struct hwspinlock_device *bank = platform_get_drvdata(pdev);
	struct hwspinlock_private *priv = bank->lock[0].priv;
	void __iomem *io_base = bank->bank_data;
	int ret;

	ret = hwspin_lock_unregister(bank);
	if (ret) {
		dev_err(&pdev->dev, "%s failed: %d\n", __func__, ret);
		return ret;
	}

	if (priv->ext_base)
		iounmap(priv->ext_base);
	iounmap(io_base);
	kfree(priv);
	kfree(bank);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver rmobile_hwsem_driver = {
	.probe		= rmobile_hwsem_probe,
	.remove		= __devexit_p(rmobile_hwsem_remove),
	.driver		= {
		.name	= "rmobile_hwsem",
		.owner	= THIS_MODULE,
	},
};

static int __init rmobile_hwsem_init(void)
{
	return platform_driver_register(&rmobile_hwsem_driver);
}

/* board init code might need to reserve hwspinlocks for predefined purposes */
postcore_initcall(rmobile_hwsem_init);

static void __exit rmobile_hwsem_exit(void)
{
	platform_driver_unregister(&rmobile_hwsem_driver);
}
module_exit(rmobile_hwsem_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hardware spinlock driver for SH-/R-Mobile");
MODULE_AUTHOR("Renesas Electronics Corporation");
