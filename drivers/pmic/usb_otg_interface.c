/*
 * drivers/pmic/usb_otg_interface.c
 *
 * USB OTG interface
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
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

#include <linux/slab.h>
#include <linux/pmic/pmic.h>

/*
 * usb_otg device struct
 */
struct usb_otg_pmic_device {
	struct device dev;
	struct usb_otg_pmic_device_ops *ops;
};

static struct usb_otg_pmic_device *usb_otg_pmic_dev;

/*
 * pmic_set_vbus: enable or disable 5V USB OTG VBUS power supply
 * @enable:
 *          enable = 1: enable 5V VBUS power supply
 *          enable = 0: disable 5V VBUS power supply
 * return:
 *        = 0: Successful
 *        < 0: Failed
 */
int pmic_set_vbus(int enable)
{
	int ret;
	if (!(usb_otg_pmic_dev && usb_otg_pmic_dev->ops
		&& usb_otg_pmic_dev->ops->set_vbus))
		return -ENODEV;

	ret = usb_otg_pmic_dev->ops->set_vbus(usb_otg_pmic_dev->dev.parent,
					enable);

	return ret;
}
EXPORT_SYMBOL_GPL(pmic_set_vbus);

/*
 * usb_otg_pmic_device_register: register a usb_otg device into this interface
 * @dev: The struct which handles the usb_otg driver.
 * @ops: struct usb_otg_pmic_device_ops
 * return:
 *        = 0: Registration is successful
 *        < 0: Registration is failed
 */
int usb_otg_pmic_device_register(struct device *dev,
			struct usb_otg_pmic_device_ops *ops)
{
	int ret;

	if (usb_otg_pmic_dev) {
		ret = -EALREADY;
		goto err_device_register_already;
	}

	usb_otg_pmic_dev = kzalloc(sizeof(*usb_otg_pmic_dev), GFP_KERNEL);
	if (!usb_otg_pmic_dev) {
		ret = -ENOMEM;
		goto err_device_alloc_failed;
	}

	dev_set_name(&usb_otg_pmic_dev->dev, "usb_otg_pmic_interface%d", 0);
	usb_otg_pmic_dev->ops = ops;
	usb_otg_pmic_dev->dev.parent = dev;

	ret = device_register(&usb_otg_pmic_dev->dev);
	if (ret < 0)
		goto err_device_register_failed;

	return ret;

err_device_register_failed:
	kfree(usb_otg_pmic_dev);
err_device_alloc_failed:
err_device_register_already:
	return ret;
}
EXPORT_SYMBOL_GPL(usb_otg_pmic_device_register);

/*
 * usb_otg_pmic_device_unregister: unregister a usb_otg device
 * from this interface
 * @dev: The struct which handles the device is registered to usb_otg interface
 * return:
 *        = 0: Unregistration is successful
 */
int usb_otg_pmic_device_unregister(struct device *dev)
{
	device_unregister(&usb_otg_pmic_dev->dev);
	kfree(usb_otg_pmic_dev);
	return 0;
}
EXPORT_SYMBOL_GPL(usb_otg_pmic_device_unregister);

MODULE_DESCRIPTION("USB OTG PMIC interface functions");
MODULE_LICENSE("GPL");

