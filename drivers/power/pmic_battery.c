/*
 * drivers/power/pmic-battery.c
 *
 * Battery management
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


#include <linux/io.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/pmic/pmic.h>

static DEFINE_MUTEX(battery_mutex);


struct pmic_battery_device {
	int depth;
	struct device dev;
	struct device *pdev[E_BATTERY_TERMINATE];
	struct pmic_battery_ops *ops;
	struct power_supply usb;
	struct power_supply battery;
	struct battery_correct_ops *correct_ops;
};

static struct pmic_battery_device *battery_dev;

void pmic_power_supply_changed(int status)
{
	if (status & E_USB_STATUS_CHANGED)
		power_supply_changed(&battery_dev->usb);
	if (status & E_BATTERY_STATUS_CHANGED)
		power_supply_changed(&battery_dev->battery);
}
EXPORT_SYMBOL_GPL(pmic_power_supply_changed);

static int pmic_correct_status(int status)
{
	int retval = 0;

	if (battery_dev->correct_ops
		&& battery_dev->correct_ops->correct_status_func)
		retval = battery_dev->correct_ops->correct_status_func(status);
	else
		retval = status + 0;	 /* Nothing to do */

	return retval;
}

static int pmic_correct_capacity(int cap)
{
	int retval = 0;

	if (battery_dev->correct_ops
		&& battery_dev->correct_ops->correct_capacity_func)
		retval = battery_dev->correct_ops->correct_capacity_func(cap);
	else
		retval = cap + 0;	 /* Nothing to do */

	return retval;
}

static int pmic_correct_temperature(int temp)
{
	int retval = 0;

	if (battery_dev->correct_ops
		&& battery_dev->correct_ops->correct_temp_func)
		retval = battery_dev->correct_ops->correct_temp_func(temp);
	else
		retval = temp + 0;	 /* Nothing to do */

	return retval;
}

static int pmic_correct_voltage(int vol)
{
	int retval = 0;
	if (battery_dev->correct_ops
		&& battery_dev->correct_ops->correct_voltage_func)
		retval = battery_dev->correct_ops->correct_voltage_func(vol);
	else
		retval = vol + 0;	 /* Nothing to do */

	return retval;
}

static int pmic_usb_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	struct pmic_battery_device *battery_dev = container_of(psy,
		struct pmic_battery_device, usb);

	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (battery_dev->ops->get_usb_online)
			val->intval = battery_dev->ops->get_usb_online(
				battery_dev->pdev[E_BATTERY_USB_ONLINE]);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int pmic_battery_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	struct pmic_battery_device *battery_dev = container_of(psy,
		struct pmic_battery_device, battery);

	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (battery_dev->ops->get_bat_status)
			val->intval = pmic_correct_status(
				battery_dev->ops->get_bat_status(
					battery_dev->pdev[E_BATTERY_STATUS]));
		else
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (battery_dev->ops->get_bat_health)
			val->intval = battery_dev->ops->get_bat_health(
				battery_dev->pdev[E_BATTERY_HEALTH]);
		else
			val->intval = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (battery_dev->ops->get_bat_present)
			val->intval = battery_dev->ops->get_bat_present(
				battery_dev->pdev[E_BATTERY_PRESENT]);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		if (battery_dev->ops->get_bat_technology)
			val->intval = battery_dev->ops->get_bat_technology(
				battery_dev->pdev[E_BATTERY_TECHNOLOGY]);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (battery_dev->ops->get_bat_capacity)
			val->intval = pmic_correct_capacity(
				battery_dev->ops->get_bat_capacity(
					battery_dev->pdev[E_BATTERY_CAPACITY]));
		else
			val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		if (battery_dev->ops->get_bat_capacity_level)
			val->intval = battery_dev->ops->get_bat_capacity_level(
				battery_dev->pdev[E_BATTERY_CAPACITY_LEVEL]);
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		if (battery_dev->ops->get_bat_temperature)
			val->intval = pmic_correct_temperature(
				battery_dev->ops->get_bat_temperature(
				battery_dev->pdev[E_BATTERY_TEMPERATURE]));
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_TEMP_HPA:
		if (battery_dev->ops->get_hpa_temperature)
			val->intval = pmic_correct_temperature(
				battery_dev->ops->get_hpa_temperature(
				battery_dev->pdev[E_BATTERY_HPA_TEMPERATURE]));
		else
			val->intval = 0;
		break;


	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (battery_dev->ops->get_bat_voltage)
			val->intval =
			pmic_correct_voltage(battery_dev->ops->get_bat_voltage(
				battery_dev->pdev[E_BATTERY_VOLTAGE_NOW]));
		else
			val->intval = 0;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}


static int pmic_battery_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	int ret = 0;
	struct pmic_battery_device *battery_dev = container_of(psy,
		struct pmic_battery_device, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		switch (val->intval) {
		case 0:
		case 1:
			if (battery_dev->ops->stop_charging)
				ret = battery_dev->ops->stop_charging(
				battery_dev->pdev[E_BATTERY_STOP], val->intval);
			else
				ret = -ENODEV;
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;
	default:
		ret = -EPERM;
		break;
	}
	return ret;
}

static int pmic_battery_property_is_writeable(struct power_supply *psy,
				enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		return 1;
	default:
		break;
	}
	return 0;
}

static enum power_supply_property pmic_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_HPA,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static enum power_supply_property pmic_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

int pmic_battery_register_correct_func(struct battery_correct_ops *ops)
{
	int ret = 0;
	if (!battery_dev)
		ret = -ENODEV;
	else
		battery_dev->correct_ops = ops;

	return ret;
}
EXPORT_SYMBOL(pmic_battery_register_correct_func);

void pmic_battery_unregister_correct_func(void)
{
	if (battery_dev)
		battery_dev->correct_ops = NULL;
}
EXPORT_SYMBOL(pmic_battery_unregister_correct_func);

int pmic_battery_device_register(struct device *dev,
			struct pmic_battery_ops *ops)
{
	int ret = 0;
	mutex_lock(&battery_mutex);
	if (!battery_dev) {
		battery_dev = kzalloc(sizeof(*battery_dev), GFP_KERNEL);
		if (!battery_dev) {
			ret = -ENOMEM;
			goto err_device_alloc_failed;
		}
		dev_set_name(&battery_dev->dev, "pmic_battery%d", 0);

		if (ops->get_usb_online)
			battery_dev->pdev[E_BATTERY_USB_ONLINE] = dev;

		if (ops->get_bat_status)
			battery_dev->pdev[E_BATTERY_STATUS] = dev;

		if (ops->get_bat_health)
			battery_dev->pdev[E_BATTERY_HEALTH] = dev;

		if (ops->get_bat_present)
			battery_dev->pdev[E_BATTERY_PRESENT] = dev;

		if (ops->get_bat_technology)
			battery_dev->pdev[E_BATTERY_TECHNOLOGY] = dev;

		if (ops->get_bat_capacity)
			battery_dev->pdev[E_BATTERY_CAPACITY] = dev;

		if (ops->get_bat_capacity_level)
			battery_dev->pdev[E_BATTERY_CAPACITY_LEVEL] = dev;

		if (ops->get_bat_temperature)
			battery_dev->pdev[E_BATTERY_TEMPERATURE] = dev;

		if (ops->get_hpa_temperature)
			battery_dev->pdev[E_BATTERY_HPA_TEMPERATURE] = dev;

		if (ops->get_bat_voltage)
			battery_dev->pdev[E_BATTERY_VOLTAGE_NOW] = dev;

		if (ops->stop_charging)
			battery_dev->pdev[E_BATTERY_STOP] = dev;

		battery_dev->ops = ops;
		battery_dev->depth = 0;

		ret = device_register(&battery_dev->dev);
		if (ret)
			goto err_kalloc_bat;

		battery_dev->battery.properties = pmic_battery_props;
		battery_dev->battery.num_properties =
					ARRAY_SIZE(pmic_battery_props);
		battery_dev->battery.get_property = pmic_battery_get_property;
		battery_dev->battery.name = "battery";
		battery_dev->battery.type = POWER_SUPPLY_TYPE_BATTERY;
		battery_dev->battery.set_property	=
					pmic_battery_set_property;
		battery_dev->battery.property_is_writeable =
					pmic_battery_property_is_writeable;

		battery_dev->usb.properties = pmic_usb_props;
		battery_dev->usb.num_properties = ARRAY_SIZE(pmic_usb_props);
		battery_dev->usb.get_property = pmic_usb_get_property;
		battery_dev->usb.name = "usb";
		battery_dev->usb.type = POWER_SUPPLY_TYPE_USB;

		ret = power_supply_register(&battery_dev->dev,
						&battery_dev->usb);
		if (ret)
			goto err_usb_failed;

		ret = power_supply_register(&battery_dev->dev,
						&battery_dev->battery);
		if (ret)
			goto err_battery_failed;
	} else {
		if (ops->get_usb_online) {
			battery_dev->pdev[E_BATTERY_USB_ONLINE] = dev;
			battery_dev->ops->get_usb_online = ops->get_usb_online;
		}
		if (ops->get_bat_status) {
			battery_dev->pdev[E_BATTERY_STATUS] = dev;
			battery_dev->ops->get_bat_status = ops->get_bat_status;
		}
		if (ops->get_bat_health) {
			battery_dev->pdev[E_BATTERY_HEALTH] = dev;
			battery_dev->ops->get_bat_health = ops->get_bat_health;
		}
		if (ops->get_bat_present) {
			battery_dev->pdev[E_BATTERY_PRESENT] = dev;
			battery_dev->ops->get_bat_present =
						ops->get_bat_present;
		}
		if (ops->get_bat_technology) {
			battery_dev->pdev[E_BATTERY_TECHNOLOGY] = dev;
			battery_dev->ops->get_bat_technology =
						ops->get_bat_technology;
		}
		if (ops->get_bat_capacity) {
			battery_dev->pdev[E_BATTERY_CAPACITY] = dev;
			battery_dev->ops->get_bat_capacity =
						ops->get_bat_capacity;
		}
		if (ops->get_bat_capacity_level) {
			battery_dev->pdev[E_BATTERY_CAPACITY_LEVEL] = dev;
			battery_dev->ops->get_bat_capacity_level =
						ops->get_bat_capacity_level;
		}
		if (ops->get_bat_temperature) {
			battery_dev->pdev[E_BATTERY_TEMPERATURE] = dev;
			battery_dev->ops->get_bat_temperature =
						ops->get_bat_temperature;
		}
		if (ops->get_hpa_temperature) {
			battery_dev->pdev[E_BATTERY_HPA_TEMPERATURE] = dev;
			battery_dev->ops->get_hpa_temperature =
						ops->get_hpa_temperature;
		}
		if (ops->get_bat_voltage) {
			battery_dev->pdev[E_BATTERY_VOLTAGE_NOW] = dev;
			battery_dev->ops->get_bat_voltage =
						ops->get_bat_voltage;
		}
		if (ops->stop_charging) {
			battery_dev->pdev[E_BATTERY_STOP] = dev;
			battery_dev->ops->stop_charging = ops->stop_charging;
		}
	}
	battery_dev->depth++;
	mutex_unlock(&battery_mutex);
	return ret;

err_battery_failed:
	power_supply_unregister(&battery_dev->usb);
err_usb_failed:
#ifdef CONFIG_ENABLE_RUNNING_AVERAGE
	kfree(cap_list);
err_kalloc_list:
#endif
	device_unregister(&battery_dev->dev);
err_kalloc_bat:
	kfree(battery_dev);
	battery_dev = NULL;
err_device_alloc_failed:
	mutex_unlock(&battery_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(pmic_battery_device_register);

int pmic_battery_device_unregister(struct device *dev)
{
	mutex_lock(&battery_mutex);
	battery_dev->depth--;
	if (battery_dev->depth <= 0) {
		power_supply_unregister(&battery_dev->usb);
		power_supply_unregister(&battery_dev->battery);
		device_unregister(&battery_dev->dev);
		kfree(battery_dev);
		battery_dev = NULL;
	}
	mutex_unlock(&battery_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(pmic_battery_device_unregister);

MODULE_DESCRIPTION("PowerManagementIC battery interface functions");
MODULE_LICENSE("GPL");

