/*
 * SuperH KEYSC Keypad Driver
 *
 * Copyright (C) 2008 Magnus Damm
 *
 * Based on gpio_keys.c, Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input/sh_keysc.h>
#include <linux/bitmap.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/slab.h>

static const struct {
	unsigned char kymd, keyout, keyin;
} sh_keysc_mode[] = {
	[SH_KEYSC_MODE_1] = { 0, 6, 5 },
	[SH_KEYSC_MODE_2] = { 1, 5, 6 },
	[SH_KEYSC_MODE_3] = { 2, 4, 7 },
	[SH_KEYSC_MODE_4] = { 3, 6, 6 },
	[SH_KEYSC_MODE_5] = { 4, 6, 7 },
	[SH_KEYSC_MODE_6] = { 5, 8, 8 },
};

struct sh_keysc_priv {
	void __iomem *iomem_base;
	struct clk *clk;
	DECLARE_BITMAP(last_keys, SH_KEYSC_MAXKEYS);
	struct input_dev *input;
	struct sh_keysc_info pdata;
};

#define KYCR1 0
#define KYCR2 1
#define KYINDR 2
#define KYOUTDR 3

#define KYCR2_IRQ_LEVEL    0x10
#define KYCR2_IRQ_DISABLED 0x00

#define A_KYCR1		4
#define A_KYCR2		5
#define A_KYCR4		7
#define A_KYCR6		9
#define A_KYIR		12
#define A_KYSR		13
#define A_KYINDR	14
#define A_KYOUTDR	15
#define A_KYSCDRL	16
#define A_KYSCDRH	17
#define A_KYSCDNUM	24

#define A_KYCR1_KEYEXT		(1 << 15)
#define A_KYCR1_MASKVALUE	(1 << 4)
#define A_KYCR1_MASKVALUE2	(1 << 3)

#define A_KYIR_STBCSL	(1 << 20)
#define A_KYIR_FIFONE	(1 << 4)
#define A_KYIR_KYALR	(1 << 3)
#define A_KYIR_KYCHG	(1 << 2)
#define A_KYIR_KYDET	(1 << 0)

#define A_KYCR4_AUTOLPC	(1 << 2)
#define A_KYCR4_AUTOKS	(1 << 0)

static unsigned long sh_keysc_read(struct sh_keysc_priv *p, int reg_nr)
{
	switch (reg_nr) {
	case A_KYIR:
	case A_KYSR:
	case A_KYSCDRL:
	case A_KYSCDRH:
	case A_KYSCDNUM:
		return readl_relaxed(p->iomem_base + (reg_nr << 2));
	}
	return readw_relaxed(p->iomem_base + (reg_nr << 2));
}

static void sh_keysc_write(struct sh_keysc_priv *p, int reg_nr,
			   unsigned long value)
{
	switch (reg_nr) {
	case A_KYIR:
	case A_KYSR:
	case A_KYSCDRL:
	case A_KYSCDRH:
	case A_KYSCDNUM:
		writel_relaxed(value, p->iomem_base + (reg_nr << 2));
		return;
	}
	writew_relaxed(value, p->iomem_base + (reg_nr << 2));
}

static void sh_keysc_level_mode(struct sh_keysc_priv *p,
				unsigned long keys_set)
{
	struct sh_keysc_info *pdata = &p->pdata;

	sh_keysc_write(p, KYOUTDR, 0);
	sh_keysc_write(p, KYCR2, KYCR2_IRQ_LEVEL | (keys_set << 8));
	sh_keysc_read(p, KYCR2); /* defeat write posting */

	if (pdata->kycr2_delay)
		udelay(pdata->kycr2_delay);
}

static void sh_keysc_map_dbg(struct device *dev, unsigned long *map,
			     const char *str)
{
	int k;

	for (k = 0; k < BITS_TO_LONGS(SH_KEYSC_MAXKEYS); k++)
		dev_dbg(dev, "%s[%d] 0x%lx\n", str, k, map[k]);
}

static irqreturn_t sh_keysc_isr(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct sh_keysc_priv *priv = platform_get_drvdata(pdev);
	struct sh_keysc_info *pdata = &priv->pdata;
	int keyout_nr = sh_keysc_mode[pdata->mode].keyout;
	int keyin_nr = sh_keysc_mode[pdata->mode].keyin;
	DECLARE_BITMAP(keys, SH_KEYSC_MAXKEYS);
	DECLARE_BITMAP(keys0, SH_KEYSC_MAXKEYS);
	DECLARE_BITMAP(keys1, SH_KEYSC_MAXKEYS);
	unsigned char keyin_set, tmp;
	int i, k, n;
	unsigned long drl, drh;

	unsigned long drl, drh, num;

	bitmap_fill(keys1, SH_KEYSC_MAXKEYS);
	bitmap_zero(keys0, SH_KEYSC_MAXKEYS);

	if (pdata->automode) {
		while ((num = sh_keysc_read(priv, A_KYSCDNUM))) {
			bitmap_zero(keys, SH_KEYSC_MAXKEYS);

			if ((pdata->flags & WA_EOS_E132_KEYSC) && num == 1) {
				int val = sh_keysc_read(priv, 11);
				if (val == 0x36 || val == 0x3b)
					usleep_range(40, 100);
			}
			drl = sh_keysc_read(priv, A_KYSCDRL);
			drh = sh_keysc_read(priv, A_KYSCDRH);

			for (i = 0; i < keyout_nr; i++) {
				n = keyin_nr * i;
				tmp = (i < 4) ? (drl >> (i * 8)) :
						(drh >> ((i-4) * 8));

				/* set bit if key press has been detected */
				for (k = 0; k < keyin_nr; k++) {
					if (tmp & (1 << k))
						__set_bit(n + k, keys);
				}
			}
			bitmap_and(keys1, keys1, keys, SH_KEYSC_MAXKEYS);
			bitmap_or(keys0, keys0, keys, SH_KEYSC_MAXKEYS);
		}

		}else{


		do {
			bitmap_zero(keys, SH_KEYSC_MAXKEYS);
			keyin_set = 0;

			sh_keysc_write(priv, KYCR2, KYCR2_IRQ_DISABLED);

			for (i = 0; i < keyout_nr; i++) {
				n = keyin_nr * i;

				/* drive one KEYOUT pin low, read KEYIN pins */
				sh_keysc_write(priv, KYOUTDR, 0xffff ^ (3 << (i * 2)));
				udelay(pdata->delay);
				tmp = sh_keysc_read(priv, KYINDR);

				/* set bit if key press has been detected */
				for (k = 0; k < keyin_nr; k++) {
					if (tmp & (1 << k))
						__set_bit(n + k, keys);
				}

				/* keep track of which KEYIN bits that have been set */
				keyin_set |= tmp ^ ((1 << keyin_nr) - 1);
			}
			sh_keysc_write(priv, KYOUTDR, 0);
			sh_keysc_level_mode(priv, keyin_set);

			sh_keysc_map_dbg(&pdev->dev, keys, "keys");

		bitmap_complement(keys, keys, SH_KEYSC_MAXKEYS);
		bitmap_and(keys1, keys1, keys, SH_KEYSC_MAXKEYS);
		bitmap_or(keys0, keys0, keys, SH_KEYSC_MAXKEYS);

		} while (sh_keysc_read(priv, KYCR2) & 0x01);
	}

	sh_keysc_map_dbg(&pdev->dev, priv->last_keys, "last_keys");
	sh_keysc_map_dbg(&pdev->dev, keys0, "keys0");
	sh_keysc_map_dbg(&pdev->dev, keys1, "keys1");

	for (i = 0; i < SH_KEYSC_MAXKEYS; i++) {
		k = pdata->keycodes[i];
		if (!k)
			continue;

		if (test_bit(i, keys0) == test_bit(i, priv->last_keys))
			continue;

		if (test_bit(i, keys1) || test_bit(i, keys0)) {
			input_event(priv->input, EV_KEY, k, 1);
			__set_bit(i, priv->last_keys);
		}

		if (!test_bit(i, keys1)) {
			input_event(priv->input, EV_KEY, k, 0);
			__clear_bit(i, priv->last_keys);
		}

	}
	input_sync(priv->input);

	return IRQ_HANDLED;
}

static int __devinit sh_keysc_probe(struct platform_device *pdev)
{
	struct sh_keysc_priv *priv;
	struct sh_keysc_info *pdata;
	struct resource *res;
	struct input_dev *input;
	int i;
	int irq, error;
	char clk_name[8];

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "no platform data defined\n");
		error = -EINVAL;
		goto err0;
	}

	error = -ENXIO;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get I/O memory\n");
		goto err0;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get irq\n");
		goto err0;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (priv == NULL) {
		dev_err(&pdev->dev, "failed to allocate driver data\n");
		error = -ENOMEM;
		goto err0;
	}

	platform_set_drvdata(pdev, priv);
	memcpy(&priv->pdata, pdev->dev.platform_data, sizeof(priv->pdata));
	pdata = &priv->pdata;

	priv->iomem_base = ioremap_nocache(res->start, resource_size(res));
	if (priv->iomem_base == NULL) {
		dev_err(&pdev->dev, "failed to remap I/O memory\n");
		error = -ENXIO;
		goto err1;
	}

	snprintf(clk_name, sizeof(clk_name), "keysc%d", pdev->id);
	priv->clk = clk_get(&pdev->dev, clk_name);
	if (IS_ERR(priv->clk)) {
		dev_err(&pdev->dev, "cannot get clock \"%s\"\n", clk_name);
		error = PTR_ERR(priv->clk);
		goto err5;
	}

	priv->input = input_allocate_device();
	if (!priv->input) {
		dev_err(&pdev->dev, "failed to allocate input device\n");
		error = -ENOMEM;
		goto err2;
	}

	input = priv->input;
	input->evbit[0] = BIT_MASK(EV_KEY);

	input->name = pdev->name;
	input->phys = "sh-keysc-keys/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	input->keycode = pdata->keycodes;
	input->keycodesize = sizeof(pdata->keycodes[0]);
	input->keycodemax = ARRAY_SIZE(pdata->keycodes);

	error = request_threaded_irq(irq, NULL, sh_keysc_isr, IRQF_ONESHOT,
				     dev_name(&pdev->dev), pdev);
	if (error) {
		dev_err(&pdev->dev, "failed to request IRQ\n");
		goto err3;
	}

	for (i = 0; i < SH_KEYSC_MAXKEYS; i++)
		__set_bit(pdata->keycodes[i], input->keybit);
	__clear_bit(KEY_RESERVED, input->keybit);

	error = input_register_device(input);
	if (error) {
		dev_err(&pdev->dev, "failed to register input device\n");
		goto err4;
	}
	
	clk_enable(priv->clk);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	if (pdata->automode) {
		sh_keysc_write(priv, A_KYCR1, A_KYCR1_KEYEXT |
				(sh_keysc_mode[pdata->mode].kymd << 8) |
				 A_KYCR1_MASKVALUE | pdata->scan_timing);
		if (!pdata->scan_timing2)
			pdata->scan_timing2 = 7;
		if (!pdata->scan_timing1)
			pdata->scan_timing1 = 2;
		sh_keysc_write(priv, A_KYCR2, (pdata->scan_timing2 << 12) |
				(pdata->scan_timing1 << 8) | 0x01);
		sh_keysc_write(priv, A_KYCR6, 0);
		sh_keysc_write(priv, A_KYOUTDR, 0);
		sh_keysc_write(priv, A_KYIR, A_KYIR_FIFONE);
		sh_keysc_write(priv, A_KYCR4, A_KYCR4_AUTOLPC);
	} else {
		sh_keysc_write(priv, KYCR1,
				(sh_keysc_mode[pdata->mode].kymd << 8) |
				pdata->scan_timing);
		sh_keysc_write(priv, KYOUTDR, 0);
		sh_keysc_level_mode(priv, 0);
	}

	if (pdata->wakeup)
		device_init_wakeup(&pdev->dev, 1);

	return 0;

 err4:
	free_irq(irq, pdev);
 err3:
	input_free_device(input);
err5:
	clk_put(priv->clk);	
 err2:
	iounmap(priv->iomem_base);
 err1:
	platform_set_drvdata(pdev, NULL);
	kfree(priv);
 err0:
	return error;
}

static int __devexit sh_keysc_remove(struct platform_device *pdev)
{
	struct sh_keysc_priv *priv = platform_get_drvdata(pdev);

	if (priv->pdata.automode) {
		sh_keysc_write(priv, A_KYCR4, 0);
		sh_keysc_write(priv, A_KYIR, 0);
	} else {
		sh_keysc_write(priv, KYCR2, KYCR2_IRQ_DISABLED);
	}

	input_unregister_device(priv->input);
	free_irq(platform_get_irq(pdev, 0), pdev);
	iounmap(priv->iomem_base);

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);
	
	clk_disable(priv->clk);
	clk_put(priv->clk);
	platform_set_drvdata(pdev, NULL);
	kfree(priv);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sh_keysc_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct sh_keysc_priv *priv = platform_get_drvdata(pdev);
	int irq = platform_get_irq(pdev, 0);
	unsigned long value;

	if (device_may_wakeup(dev)) {
		if (priv->pdata.automode) {
			value = sh_keysc_read(priv, A_KYIR);
			sh_keysc_write(priv, A_KYIR, value | A_KYIR_STBCSL);
		} else {
			value = sh_keysc_read(priv, KYCR1);
			sh_keysc_write(priv, KYCR1, value | 0x80);
		}
		enable_irq_wake(irq);
	} else {
		pm_runtime_put_sync(dev);
		clk_disable(priv->clk);
	}

	return 0;
}

static int sh_keysc_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
        struct sh_keysc_priv *priv = platform_get_drvdata(pdev);
	int irq = platform_get_irq(pdev, 0);

	if (device_may_wakeup(dev))
		disable_irq_wake(irq);
	else
		{
		clk_enable(priv->clk);
		pm_runtime_get_sync(dev);
		}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(sh_keysc_dev_pm_ops,
			 sh_keysc_suspend, sh_keysc_resume);

static struct platform_driver sh_keysc_device_driver = {
	.probe		= sh_keysc_probe,
	.remove		= __devexit_p(sh_keysc_remove),
	.driver		= {
		.name	= "sh_keysc",
		.pm	= &sh_keysc_dev_pm_ops,
	}
};
module_platform_driver(sh_keysc_device_driver);

MODULE_AUTHOR("Magnus Damm");
MODULE_DESCRIPTION("SuperH KEYSC Keypad Driver");
MODULE_LICENSE("GPL");
