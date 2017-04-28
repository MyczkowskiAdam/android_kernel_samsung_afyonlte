/*
 * usbhs_vbus.c - VBUS sensing driver for Renesas SH/R-Mobile USBHS mobule,
 * especially with an internal transceiver.
 *
 * Copyright (C) 2012  Renesas Electronics Corporation
 *
 * Highly inspired by gpio_vbus driver by:
 *
 * A simple GPIO VBUS sensing driver for B peripheral only devices
 * with internal transceivers.
 * Optionally D+ pullup can be controlled by a second GPIO.
 *
 * Copyright (c) 2008 Philipp Zabel <philipp.zabel@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/usb.h>
#include <linux/workqueue.h>

#include <linux/regulator/consumer.h>

#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/usb/usbhs_vbus.h>

static void __iomem	*usbcr2;

struct usbhs_vbus_data {
	struct usb_phy		phy;
	struct device		*dev;
	struct regulator	*vbus_draw;
	int			vbus_draw_enabled;
	unsigned		mA;
	struct delayed_work	work;

	struct work_struct	phy_work;
	int			is_on;

	int			phy_irq;
	int			vbus_irq;
};

/* interface to regulator framework */
static void set_vbus_draw(struct usbhs_vbus_data *usbhs_vbus, unsigned mA)
{
	struct regulator *vbus_draw = usbhs_vbus->vbus_draw;
	struct usbhs_vbus_mach_info *pdata = usbhs_vbus->dev->platform_data;
	int enabled;

	if (!vbus_draw && !pdata->set_vbus_draw)
		return;

	if (pdata->set_vbus_draw) {
		pdata->set_vbus_draw(mA, usbhs_vbus->phy.otg->gadget->speed);
		goto out;
	}

	enabled = usbhs_vbus->vbus_draw_enabled;
	if (mA) {
		regulator_set_current_limit(vbus_draw, 0, 1000 * mA);
		if (!enabled) {
			regulator_enable(vbus_draw);
			usbhs_vbus->vbus_draw_enabled = 1;
		}
	} else {
		if (enabled) {
			regulator_disable(vbus_draw);
			usbhs_vbus->vbus_draw_enabled = 0;
		}
	}

out:
	if (mA)
		usbhs_vbus->mA = mA;

	dev_info(usbhs_vbus->dev, "Avail curr from USB = %u\n", mA);
}

static int is_usb_phy_on(void)
{
	return !(__raw_readw(usbcr2) & (1 << 7)); /* USB_OFF */
}

static void usb_phy_start(void)
{
	__raw_writew((1 << 15) | (0xA << 8), usbcr2); /* USB_START, USB_COR */
}

static void usb_phy_work(struct work_struct *work)
{
	struct usbhs_vbus_data *usbhs_vbus =
		container_of(work, struct usbhs_vbus_data, phy_work);
	int is_on, status;

	if (!usbhs_vbus->phy.otg->gadget)
		return;

	is_on = is_usb_phy_on();
	if ((is_on ^ usbhs_vbus->is_on) == 0) {
		return;
	}
	usbhs_vbus->is_on = is_on;

	if (is_on) {
		status = USB_EVENT_VBUS;
		usbhs_vbus->phy.state = OTG_STATE_B_PERIPHERAL;
		usbhs_vbus->phy.last_event = status;
		usb_gadget_vbus_connect(usbhs_vbus->phy.otg->gadget);

		/* drawing a "unit load" is *always* OK, except for OTG */
		set_vbus_draw(usbhs_vbus, 100);
	} else {
		set_vbus_draw(usbhs_vbus, 0);

		usb_gadget_vbus_disconnect(usbhs_vbus->phy.otg->gadget);
		status = USB_EVENT_NONE;
		usbhs_vbus->phy.state = OTG_STATE_B_IDLE;
		usbhs_vbus->phy.last_event = status;
	}

	atomic_notifier_call_chain(&usbhs_vbus->phy.notifier,
				   status, usbhs_vbus->phy.otg->gadget);
}

/* USB-PHY on/off IRQ handler */
static irqreturn_t usb_phy_irq(int irq, void *data)
{
	struct platform_device *pdev = data;
	struct usbhs_vbus_data *usbhs_vbus = platform_get_drvdata(pdev);
	struct usb_otg *otg = usbhs_vbus->phy.otg;

	dev_dbg(&pdev->dev, "USB-PHY %s (gadget: %s)\n",
		is_usb_phy_on() ? "on" : "off",
		otg->gadget ? otg->gadget->name : "none");

	if (otg->gadget)
		schedule_work(&usbhs_vbus->phy_work);

	return IRQ_HANDLED;
}

static int is_vbus_powered(struct usbhs_vbus_mach_info *pdata)
{
	return pdata->is_vbus_powered();
}

static void gpio_vbus_work(struct work_struct *work)
{
	struct usbhs_vbus_data *usbhs_vbus =
		container_of(work, struct usbhs_vbus_data, work.work);
	struct usbhs_vbus_mach_info *pdata = usbhs_vbus->dev->platform_data;

	if (!usbhs_vbus->phy.otg->gadget)
		return;

	if (is_vbus_powered(pdata)) {
		/*
		 * Even if USB-PHY is put into Power ON state (USB_OFF == 0),
		 * it will be reactivated by writing 1 to USB_START bit.
		 * That is, USB-PHY gets isolated first, the countdown starts
		 * again, then USB-PHY gets re-started.  In the process,
		 * USB-PHY off and on interrupts get triggered accordingly.
		 */
		usb_phy_start(); /* (re)start USB-PHY... */
	} else {
		/*
		 * If VBUS is removed, USB-PHY gets isolated automatically.
		 * Nothing to do here.
		 */
	}
}

/* VBUS change IRQ handler */
static irqreturn_t gpio_vbus_irq(int irq, void *data)
{
	struct platform_device *pdev = data;
	struct usbhs_vbus_mach_info *pdata = pdev->dev.platform_data;
	struct usbhs_vbus_data *usbhs_vbus = platform_get_drvdata(pdev);
	struct usb_otg *otg = usbhs_vbus->phy.otg;

	dev_dbg(&pdev->dev, "VBUS %s (gadget: %s)\n",
		is_vbus_powered(pdata) ? "supplied" : "inactive",
		otg->gadget ? otg->gadget->name : "none");

	if (otg->gadget)
		schedule_delayed_work(&usbhs_vbus->work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

/* OTG transceiver interface */

/* bind/unbind the peripheral controller */
static int usbhs_vbus_set_peripheral(struct usb_otg *otg,
				     struct usb_gadget *gadget)
{
	struct usbhs_vbus_data *usbhs_vbus;
	struct usbhs_vbus_mach_info *pdata;
	struct platform_device *pdev;

	usbhs_vbus = container_of(otg->phy, struct usbhs_vbus_data, phy);
	pdev = to_platform_device(usbhs_vbus->dev);
	pdata = usbhs_vbus->dev->platform_data;

	if (!gadget) {
		dev_info(&pdev->dev, "unregistering gadget '%s'\n",
			 otg->gadget->name);

		set_vbus_draw(usbhs_vbus, 0);

		usb_gadget_vbus_disconnect(otg->gadget);
		otg->phy->state = OTG_STATE_UNDEFINED;

		otg->gadget = NULL;
		return 0;
	}

	otg->gadget = gadget;
	dev_dbg(&pdev->dev, "registered gadget '%s'\n", gadget->name);

	/* initialize connection state */
	usbhs_vbus->is_on = 0; /* start with USB-PHY off */
	gpio_vbus_irq(usbhs_vbus->vbus_irq, pdev);
	return 0;
}

/* effective for B devices, ignored for A-peripheral */
static int usbhs_vbus_set_power(struct usb_phy *phy, unsigned mA)
{
	struct usbhs_vbus_data *usbhs_vbus;

	usbhs_vbus = container_of(phy, struct usbhs_vbus_data, phy);

	if (phy->state == OTG_STATE_B_PERIPHERAL)
		set_vbus_draw(usbhs_vbus, mA);
	return 0;
}

/* for non-OTG B devices: set/clear transceiver suspend mode */
static int usbhs_vbus_set_suspend(struct usb_phy *phy, int suspend)
{
	struct usbhs_vbus_data *usbhs_vbus;

	usbhs_vbus = container_of(phy, struct usbhs_vbus_data, phy);

	/* draw max 0 mA from vbus in suspend mode; or the previously
	 * recorded amount of current if not suspended
	 *
	 * NOTE: high powered configs (mA > 100) may draw up to 2.5 mA
	 * if they're wake-enabled ... we don't handle that yet.
	 */
	return usbhs_vbus_set_power(phy, suspend ? 0 : usbhs_vbus->mA);
}

/* platform driver interface */

static int __init usbhs_vbus_probe(struct platform_device *pdev)
{
	struct usbhs_vbus_mach_info *pdata = pdev->dev.platform_data;
	struct usbhs_vbus_data *usbhs_vbus;
	struct resource *res;
	int err, irq;
	unsigned long irqflags;

	if (!pdata)
		return -EINVAL;

	/* USB control register 2 (USBCR2) */
	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (!res) {
		dev_err(&pdev->dev, "can't get io resource\n");
		return -ENXIO;
	}
	usbcr2 = (void __iomem *)res->start;

	usbhs_vbus = kzalloc(sizeof(struct usbhs_vbus_data), GFP_KERNEL);
	if (!usbhs_vbus)
		return -ENOMEM;

	usbhs_vbus->phy.otg = kzalloc(sizeof(struct usb_otg), GFP_KERNEL);
	if (!usbhs_vbus->phy.otg) {
		kfree(usbhs_vbus);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, usbhs_vbus);
	usbhs_vbus->dev = &pdev->dev;
	usbhs_vbus->phy.dev = &pdev->dev;
	usbhs_vbus->phy.label = "usbhs_vbus";
	usbhs_vbus->phy.set_power = usbhs_vbus_set_power;
	usbhs_vbus->phy.set_suspend = usbhs_vbus_set_suspend;
	usbhs_vbus->phy.state = OTG_STATE_UNDEFINED;

	usbhs_vbus->phy.otg->phy = &usbhs_vbus->phy;
	usbhs_vbus->phy.otg->set_peripheral = usbhs_vbus_set_peripheral;

	/* USB-PHY on/off IRQ */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "can't get irq resource\n");
		err = -ENXIO;
		goto err_phy_irq;
	}
	irq = usbhs_vbus->phy_irq = res->start;
	irqflags = res->flags & IRQF_TRIGGER_MASK;

	err = request_irq(irq, usb_phy_irq, irqflags, "usb_phy", pdev);
	if (err) {
		dev_err(&pdev->dev, "can't request phy_irq %d, err: %d\n",
			irq, err);
		goto err_phy_irq;
	}
	INIT_WORK(&usbhs_vbus->phy_work, usb_phy_work);

	/* VBUS change IRQ */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if (!res) {
		dev_err(&pdev->dev, "can't get irq resource\n");
		err = -ENXIO;
		goto err_vbus_irq;
	}
	irq = usbhs_vbus->vbus_irq = res->start;
	irqflags = res->flags & IRQF_TRIGGER_MASK;
	irqflags |= IRQF_SHARED;

	err = request_irq(irq, gpio_vbus_irq, irqflags, "vbus_detect", pdev);
	if (err) {
		dev_err(&pdev->dev, "can't request vbus_irq %i, err: %d\n",
			irq, err);
		goto err_vbus_irq;
	}
	INIT_DELAYED_WORK(&usbhs_vbus->work, gpio_vbus_work);

	ATOMIC_INIT_NOTIFIER_HEAD(&usbhs_vbus->phy.notifier);

	usbhs_vbus->vbus_draw = regulator_get(&pdev->dev, "vbus_draw");
	if (IS_ERR(usbhs_vbus->vbus_draw)) {
		dev_dbg(&pdev->dev, "can't get vbus_draw regulator, err: %ld\n",
			PTR_ERR(usbhs_vbus->vbus_draw));
		usbhs_vbus->vbus_draw = NULL;
	}

	/* only active when a gadget is registered */
	err = usb_set_transceiver(&usbhs_vbus->phy);
	if (err) {
		dev_err(&pdev->dev, "can't register transceiver, err: %d\n", err);
		goto err_otg;
	}

	device_init_wakeup(&pdev->dev, pdata->wakeup);

	return 0;

err_otg:
	regulator_put(usbhs_vbus->vbus_draw);
	free_irq(usbhs_vbus->vbus_irq, pdev);
err_vbus_irq:
	free_irq(usbhs_vbus->phy_irq, pdev);
err_phy_irq:
	platform_set_drvdata(pdev, NULL);
	kfree(usbhs_vbus->phy.otg);
	kfree(usbhs_vbus);
	return err;
}

static int __exit usbhs_vbus_remove(struct platform_device *pdev)
{
	struct usbhs_vbus_data *usbhs_vbus = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);
	cancel_delayed_work_sync(&usbhs_vbus->work);
	cancel_work_sync(&usbhs_vbus->phy_work);

	regulator_put(usbhs_vbus->vbus_draw);

	usb_set_transceiver(NULL);

	free_irq(usbhs_vbus->phy_irq, pdev);
	free_irq(usbhs_vbus->vbus_irq, pdev);
	platform_set_drvdata(pdev, NULL);
	kfree(usbhs_vbus->phy.otg);
	kfree(usbhs_vbus);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int usbhs_vbus_pm_suspend(struct device *dev)
{
	struct usbhs_vbus_data *usbhs_vbus = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(usbhs_vbus->vbus_irq);

	return 0;
}

static int usbhs_vbus_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usbhs_vbus_data *usbhs_vbus = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(usbhs_vbus->vbus_irq);

	gpio_vbus_irq(usbhs_vbus->vbus_irq, pdev);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(usbhs_vbus_dev_pm_ops,
			 usbhs_vbus_pm_suspend, usbhs_vbus_pm_resume);

/* NOTE:  the gpio-vbus device may *NOT* be hotplugged */

MODULE_ALIAS("platform:usbhs_vbus");

static struct platform_driver usbhs_vbus_driver = {
	.driver = {
		.name  = "usbhs_vbus",
		.owner = THIS_MODULE,
		.pm = &usbhs_vbus_dev_pm_ops,
	},
	.remove  = __exit_p(usbhs_vbus_remove),
};

static int __init usbhs_vbus_init(void)
{
	return platform_driver_probe(&usbhs_vbus_driver, usbhs_vbus_probe);
}
module_init(usbhs_vbus_init);

static void __exit usbhs_vbus_exit(void)
{
	platform_driver_unregister(&usbhs_vbus_driver);
}
module_exit(usbhs_vbus_exit);

MODULE_DESCRIPTION("Renesas SH/R-Mobile USBHS internal transceiver driver");
MODULE_AUTHOR("Renesas Electronics Corporation");
MODULE_LICENSE("GPL v2");
