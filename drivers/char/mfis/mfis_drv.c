/*
 * mfis_drv.c
 *	 This file is MFIS driver function.
 *
 * Copyright (C) 2012-2013 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <rtapi/system_standby.h>
#include "mfis_private.h"
#include <mach/r8a7373.h>

#if RTPM_PF_CUSTOM
#include <mach/pm.h>
#endif

static struct clk *mfis_clk_data;
static struct clk *clk_data;
static struct semaphore a3r_power_sem;
#define	MFIS_SEM_WAIT_TIMEOUT 40000

#if EARLYSUSPEND_STANDBY
#include <linux/earlysuspend.h>
#include <mach/irqs.h>
#define	GIC_MFIS			(126)
#define	IRQ_MFIS			(gic_spi(GIC_MFIS))
#define	CLOCK_TLB_IC_OC	((unsigned long)0xE0000000)

struct mfis_early_suspend_tbl {
	struct device *dev;
	struct early_suspend early_suspend;
};

static struct platform_device *pdev_tbl;
static unsigned long early_suspend_phase_flag;
static unsigned long late_resume_phase_flag;
static struct semaphore mfis_sem;
static unsigned long eco_mode_flag;
static unsigned long sh_early_suspend_finish_flag;

#define CYCLE_STANDBY 1
#else
#define CYCLE_STANDBY 0
#endif /* EARLYSUSPEND_STANDBY */

#if CYCLE_STANDBY
#include <linux/mfis.h>
#define STANDBY_RETRY_INTERVAL 100

static struct workqueue_struct	*mfis_wq;
static struct delayed_work	standby_work;
static unsigned long			standby_work_on;
static unsigned long			standby_work_stop_flag;
#endif /* CYCLE_STANDBY */


static void mfis_standby_work_entry(int reset_flag)
{
#if CYCLE_STANDBY
	long jiffies;

	if (reset_flag) {
		standby_work_stop_flag = 0;
	}

	if (!standby_work_stop_flag) {
		jiffies = msecs_to_jiffies(MFIS_SEM_WAIT_TIMEOUT);
		if (0 != down_timeout(&mfis_sem, jiffies))
			panic("[%s] : down_timeout TIMEOUT Error!\n", __func__);

		standby_work_on = 1;

		queue_delayed_work(
			mfis_wq,
			&standby_work,
			msecs_to_jiffies(STANDBY_RETRY_INTERVAL));

		up(&mfis_sem);
	}
#endif /* CYCLE_STANDBY */

	return;
}

static void mfis_standby_work_cancel(void)
{
#if CYCLE_STANDBY

	standby_work_stop_flag = 1;

	if (standby_work_on) {
		cancel_delayed_work_sync(&standby_work);
		standby_work_on = 0;
	}
#endif /* CYCLE_STANDBY */

	return;
}

#if CYCLE_STANDBY
static void mfis_standby_work(struct work_struct *work)
{
	int ret;

	standby_work_on = 0;

	ret = mfis_drv_suspend();
	if (ret != 0) {
		mfis_standby_work_entry(0);
	} else {
		eco_mode_flag = 1;
	}

	return;
}
#endif /* CYCLE_STANDBY */

static int mfis_suspend_noirq(struct device *dev)
{

	int ret = -1;
	long jiffies;
#if RTPM_PF_CUSTOM
	char domain_name[] = "av-domain";
	char *dev_name;
	struct device *dev_img[POWER_DOMAIN_COUNT_MAX];
	size_t dev_cnt;
	unsigned int i;
#endif

#if EARLYSUSPEND_STANDBY
	if (!early_suspend_phase_flag) {
		mfis_standby_work_cancel();

		eco_mode_flag = 0;
	}
#endif /* EARLYSUSPEND_STANDBY */

	jiffies = msecs_to_jiffies(MFIS_SEM_WAIT_TIMEOUT);
	if (0 != down_timeout(&a3r_power_sem, jiffies))
		panic("[%s] : down_timeout TIMEOUT Error!\n", __func__);

#if (EARLYSUSPEND_STANDBY == 1) && (RTPM_PF_CUSTOM == 1)
	if (POWER_A3R & readl(PSTR)) {
		dev_name = domain_name;

		ret = power_domain_devices(dev_name, dev_img, &dev_cnt);
		if (!ret) {
		} else {
			up(&a3r_power_sem);
			return -1;
		}

		for (i = 0; i < dev_cnt; i++) {
			if (ID_A3R == to_platform_device(dev_img[i])->id) {
				break;
			}
		}

		if (i >= dev_cnt) {
			up(&a3r_power_sem);
			return -1;
		}

		if (((early_suspend_phase_flag) && (1 == atomic_read(&dev_img[i]->power.usage_count))) || (!early_suspend_phase_flag)) {
#endif /* EARLYSUSPEND_STANDBY */
			ret = system_rt_standby();
			if (ret == SMAP_LIB_STANDBY_OK) {
			} else {
				up(&a3r_power_sem);
				return -1;
			}
#if RTPM_PF_CUSTOM
			dev_name = domain_name;
			ret = power_domain_devices(dev_name, dev_img, &dev_cnt);
			if (!ret) {
				for (i = 0; i < dev_cnt; i++) {
					ret = pm_runtime_put_sync(dev_img[i]);
					if (!ret) {
					} else {
						up(&a3r_power_sem);
						return -1;
					}
				}
			} else {
				up(&a3r_power_sem);
				return -1;
			}
#endif

			ret = pm_runtime_put_sync(dev);
			if (!ret) {
			} else {
				up(&a3r_power_sem);
				return -1;
			}
			clk_disable(mfis_clk_data);
			clk_disable(clk_data);
#if (EARLYSUSPEND_STANDBY == 1) && (RTPM_PF_CUSTOM == 1)
		}
	}
#endif /* EARLYSUSPEND_STANDBY */

	up(&a3r_power_sem);

	return 0;
}

static int mfis_resume_noirq(struct device *dev)
{

	int ret = -1;
	long jiffies;
#if RTPM_PF_CUSTOM

	char domain_name[] = "av-domain";
	char *dev_name;
	struct device *dev_img[POWER_DOMAIN_COUNT_MAX];
	size_t dev_cnt;
	unsigned int i;
#endif

	jiffies = msecs_to_jiffies(MFIS_SEM_WAIT_TIMEOUT);
	if (0 != down_timeout(&a3r_power_sem, jiffies))
		panic("[%s] : down_timeout TIMEOUT Error!\n", __func__);

#if EARLYSUSPEND_STANDBY
	if (CLOCK_TLB_IC_OC == (readl(MSTPSR0) & CLOCK_TLB_IC_OC)) {
#endif /* EARLYSUSPEND_STANDBY */
		clk_enable(clk_data);

		ret = pm_runtime_get_sync(dev);
		if (0 > ret) {
			up(&a3r_power_sem);
			return -1;
		}
		clk_enable(mfis_clk_data);

#if RTPM_PF_CUSTOM
		dev_name = domain_name;
		ret = power_domain_devices(dev_name, dev_img, &dev_cnt);
		if (!ret) {
			for (i = 0; i < dev_cnt; i++) {
				ret = pm_runtime_get_sync(dev_img[i]);
				if (0 > ret) {
					up(&a3r_power_sem);
					return -1;
				}
			}
		} else {
			up(&a3r_power_sem);
			return -1;
		}
#endif

		if (!(POWER_A3R & readl(PSTR)) &&
			(atomic_read(&dev_img[i]->power.usage_count) >= 1))
			power_domain_set_status(POWER_A3R, 1);


		ret = system_rt_active();
		if (ret == SMAP_LIB_STANDBY_OK) {
		} else {
			up(&a3r_power_sem);
			return -1;
		}
#if EARLYSUSPEND_STANDBY
	}
#endif /* EARLYSUSPEND_STANDBY */

	up(&a3r_power_sem);

	return 0;
}

#if EARLYSUSPEND_STANDBY
static void mfis_drv_early_suspend(struct early_suspend *h)
{
	struct mfis_early_suspend_tbl *p_tbl;
	int ret;

	p_tbl = container_of(h, struct mfis_early_suspend_tbl, early_suspend);

	early_suspend_phase_flag = 1;

	ret = mfis_suspend_noirq(p_tbl->dev);
	if (ret != 0) {
		mfis_standby_work_entry(1);
	} else {
		eco_mode_flag = 1;
	}

	early_suspend_phase_flag = 0;
	sh_early_suspend_finish_flag = 1;

	return;
}

static void mfis_drv_late_resume(struct early_suspend *h)
{
	struct mfis_early_suspend_tbl *p_tbl;

	sh_early_suspend_finish_flag = 0;
	mfis_standby_work_cancel();

	eco_mode_flag = 0;

	p_tbl = container_of(h, struct mfis_early_suspend_tbl, early_suspend);

	late_resume_phase_flag = 1;

	mfis_resume_noirq(p_tbl->dev);

	late_resume_phase_flag = 0;

	return;
}
#endif /* EARLYSUSPEND_STANDBY */

static int mfis_drv_probe(struct platform_device *pdev)
{

	int ret = -1;
#if RTPM_PF_CUSTOM
	char domain_name[] = "av-domain";
	char *dev_name;
	struct device *dev_img[POWER_DOMAIN_COUNT_MAX];
	size_t dev_cnt;
	unsigned int i;
#endif
#if EARLYSUSPEND_STANDBY
	struct mfis_early_suspend_tbl *p_tbl;
	early_suspend_phase_flag = 0;
	late_resume_phase_flag = 0;
	eco_mode_flag = 0;
	sh_early_suspend_finish_flag = 0;
#endif /* EARLYSUSPEND_STANDBY */

	clk_data = clk_get(NULL, "mp_clk");
	if (IS_ERR(clk_data)) {
		pr_err("cannot get clock \"%s\"\n", "mp_clk");
		return PTR_ERR(clk_data);
	}
	clk_enable(clk_data);

	pm_runtime_enable(&pdev->dev);

	mfis_clk_data = clk_get(NULL, "mfis");
	if (IS_ERR(mfis_clk_data)) {
		pr_err("cannot get clock \"%s\"\n", "mfis");
		return PTR_ERR(mfis_clk_data);
	}
	clk_enable(mfis_clk_data);

#if RTPM_PF_CUSTOM
	dev_name = domain_name;
	ret = power_domain_devices(dev_name, dev_img, &dev_cnt);
	if (!ret) {
		for (i = 0; i < dev_cnt; i++) {
			ret = pm_runtime_get_sync(dev_img[i]);
			if (0 > ret) {
				return -1;
			}
		}
	} else {
		return -1;
	}

#endif

	ret = pm_runtime_get_sync(&pdev->dev);
	if (0 > ret) {
		return -1;
	}

	sema_init(&a3r_power_sem, 1);

#if CYCLE_STANDBY
	mfis_wq = create_singlethread_workqueue("mfis_wq");

	INIT_DELAYED_WORK(&standby_work, mfis_standby_work);

	standby_work_on = 0;
	standby_work_stop_flag = 0;
#endif /* CYCLE_STANDBY */

#if EARLYSUSPEND_STANDBY
	p_tbl = kzalloc(sizeof(*p_tbl), GFP_KERNEL);
	if (p_tbl == NULL) {
		pr_err("cannot alloc area\n");
		return -1;
	}
	platform_set_drvdata(pdev, p_tbl);

	sema_init(&mfis_sem, 1);
	p_tbl->dev						= &pdev->dev;
	p_tbl->early_suspend.level		= (EARLY_SUSPEND_LEVEL_DISABLE_FB + 1);
	p_tbl->early_suspend.suspend	= mfis_drv_early_suspend;
	p_tbl->early_suspend.resume		= mfis_drv_late_resume;
	register_early_suspend(&p_tbl->early_suspend);
	pdev_tbl = pdev;
#endif /* EARLYSUSPEND_STANDBY */

	return 0;
}

static int mfis_drv_remove(struct platform_device *pdev)
{

#if EARLYSUSPEND_STANDBY
	struct mfis_early_suspend_tbl *p_tbl;
#endif /* EARLYSUSPEND_STANDBY */
#if RTPM_PF_CUSTOM
	int ret = -1;
	char domain_name[] = "av-domain";
	char *dev_name;
	struct device *dev_img[POWER_DOMAIN_COUNT_MAX];
	size_t dev_cnt;
	unsigned int i;
#endif

#if EARLYSUSPEND_STANDBY
	p_tbl = platform_get_drvdata(pdev);
	unregister_early_suspend(&p_tbl->early_suspend);
	kfree(p_tbl);
#endif /* EARLYSUSPEND_STANDBY */

#if RTPM_PF_CUSTOM
	dev_name = domain_name;
	ret = power_domain_devices(dev_name, dev_img, &dev_cnt);
	if (!ret) {
		for (i = 0; i < dev_cnt; i++) {
			ret = pm_runtime_put_sync(dev_img[i]);
			if (!ret) {
			} else {
				return -1;
			}
		}
	}
#endif

	pm_runtime_disable(&pdev->dev);

	clk_disable(mfis_clk_data);
	clk_put(mfis_clk_data);

	clk_disable(clk_data);
	clk_put(clk_data);

#if CYCLE_STANDBY
	destroy_workqueue(mfis_wq);
#endif /* CYCLE_STANDBY */

#if RTPM_PF_CUSTOM
	return ret;
#else
	return 0;
#endif

}

#if EARLYSUSPEND_STANDBY
int mfis_drv_suspend(void)
{
	struct mfis_early_suspend_tbl *p_tbl;
	int ret = 0;
	long jiffies;

	jiffies = msecs_to_jiffies(MFIS_SEM_WAIT_TIMEOUT);
	if (0 != down_timeout(&mfis_sem, jiffies))
		panic("[%s] : down_timeout TIMEOUT Error!\n", __func__);

	if (POWER_A3R & readl(PSTR)) {
		p_tbl = platform_get_drvdata(pdev_tbl);

		early_suspend_phase_flag = 1;

		ret = mfis_suspend_noirq(p_tbl->dev);

		early_suspend_phase_flag = 0;
	}

	up(&mfis_sem);

	return ret;
}
EXPORT_SYMBOL(mfis_drv_suspend);

int mfis_drv_resume(void)
{
	struct mfis_early_suspend_tbl *p_tbl;
	int ret = 0;
	long jiffies;

	jiffies = msecs_to_jiffies(MFIS_SEM_WAIT_TIMEOUT);
	if (0 != down_timeout(&mfis_sem, jiffies))
		panic("[%s] : down_timeout TIMEOUT Error!\n", __func__);

	if (CLOCK_TLB_IC_OC == (readl(MSTPSR0) & CLOCK_TLB_IC_OC)) {
		p_tbl = platform_get_drvdata(pdev_tbl);

		late_resume_phase_flag = 1;

		ret = mfis_resume_noirq(p_tbl->dev);

		late_resume_phase_flag = 0;
	}

	if ((sh_early_suspend_finish_flag) && (!standby_work_on)) {
		up(&mfis_sem);
		mfis_standby_work_entry(1);
	} else {
		up(&mfis_sem);
	}

	return ret;
}
EXPORT_SYMBOL(mfis_drv_resume);

#else /* EARLYSUSPEND_STANDBY */
int mfis_drv_suspend(void)
{
	return 0;
}
EXPORT_SYMBOL(mfis_drv_suspend);

int mfis_drv_resume(void)
{
	return 0;
}
EXPORT_SYMBOL(mfis_drv_resume);
#endif /* EARLYSUSPEND_STANDBY */

void mfis_drv_eco_suspend(void)
{
#if EARLYSUSPEND_STANDBY
	if (eco_mode_flag) {
		mfis_drv_suspend();
	}
#endif /* EARLYSUSPEND_STANDBY */
}
EXPORT_SYMBOL(mfis_drv_eco_suspend);


int mfis_drv_use_a4rm(void)
{
#if RTPM_PF_CUSTOM
	int		ret;
	struct device	*dev_img[POWER_DOMAIN_COUNT_MAX];
	size_t		dev_cnt	= 0;
	char domain_name[] = "meram-domain";
	char *dev_name;
	unsigned int	i;

	dev_name = domain_name;
	ret	= power_domain_devices(dev_name, dev_img, &dev_cnt);
	if (!ret) {
		for (i = 0; i < dev_cnt; i++) {
				ret = pm_runtime_get_sync(dev_img[i]);
				if (0 > ret) {
					return -1;
				}
		}
	} else {
		return -1;
	}
#endif

	return 0 ;
}
EXPORT_SYMBOL(mfis_drv_use_a4rm);


int mfis_drv_rel_a4rm(void)
{
#if RTPM_PF_CUSTOM
	int		ret;
	struct device	*dev_img[POWER_DOMAIN_COUNT_MAX];
	size_t		dev_cnt	= 0;
	char domain_name[] = "meram-domain";
	char *dev_name;
	unsigned int	i;

	dev_name = domain_name;
	ret	= power_domain_devices(dev_name, dev_img, &dev_cnt);
	if (!ret) {
		for (i = 0; i < dev_cnt; i++) {
				ret = pm_runtime_put_sync(dev_img[i]);
				if (0 > ret) {
					return -1;
				}
		}
	} else {
		return -1;
	}
#endif

	return 0 ;
}
EXPORT_SYMBOL(mfis_drv_rel_a4rm);

static int mfis_runtime_nop(struct device *dev)
{
	/* nop */
	return 0;
}


static const struct dev_pm_ops mfis_dev_pm_ops = {
	.suspend_noirq   = mfis_suspend_noirq,
	.runtime_suspend = mfis_runtime_nop,
	.runtime_resume  = mfis_runtime_nop,
};

static struct platform_driver mfis_driver = {
	.driver		= {
		.name		= "mfis",
		.owner		= THIS_MODULE,
		.pm		= &mfis_dev_pm_ops,
	},
	.probe		= mfis_drv_probe,
	.remove		= mfis_drv_remove,
};

static int __init mfis_drv_adap_init(void)
{
	return platform_driver_register(&mfis_driver);
}

static void __exit mfis_drv_adap_exit(void)
{
	platform_driver_unregister(&mfis_driver);
}


/* module_init(mfis_drv_adap_init); */
arch_initcall(mfis_drv_adap_init);
module_exit(mfis_drv_adap_exit);



MODULE_DESCRIPTION("SuperH Mobile MFIS driver");
MODULE_AUTHOR("Renesas Electronics Corporation.");
MODULE_LICENSE("GPL v2");


