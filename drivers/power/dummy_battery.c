/*
 *  File Name       : drivers/power/dummy_battery.c
 *  Function        : dummy Battery
 *
 *  Copyright (C) Renesas Electronics Corporation 2010
 *
 *  This program is free software;you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by Free
 *  Softwere Foundation; either version 2 of License, or (at your option) any
 *  later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warrnty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; If not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/io.h>

typedef enum {
	CHARGER_BATTERY = 0,
	CHARGER_USB,
	CHARGER_AC
} charger_type_t;

struct dummy_battery_data {
	struct power_supply battery;
	struct power_supply usb;
	struct power_supply ac;

	charger_type_t charger;

	unsigned int battery_present;
	unsigned int level;
};

static struct dummy_battery_data *battery_data;

static enum power_supply_property dummy_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
};

static enum power_supply_property dummy_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int dummy_battery_get_status(void)
{
	int ret;

	switch (battery_data->charger) {
	case CHARGER_BATTERY:
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case CHARGER_USB:
	case CHARGER_AC:
		if (battery_data->level == 100)
			ret = POWER_SUPPLY_STATUS_FULL;
		else
			ret = POWER_SUPPLY_STATUS_CHARGING;
		break;
	default:
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
	}
	return ret;
}

static int dummy_battery_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:		/* 0 */
		val->intval = dummy_battery_get_status();
		break;
	case POWER_SUPPLY_PROP_HEALTH:		/* 1 */
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:		/* 2 */
		val->intval = battery_data->battery_present;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:	/* 4 */
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:	/* 26 */
		val->intval = battery_data->level;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int dummy_power_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	charger_type_t charger = battery_data->charger;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:	/* 3 */
		if (psy->type == POWER_SUPPLY_TYPE_MAINS)
			val->intval = (charger ==  CHARGER_AC ? 1 : 0);
		else if (psy->type == POWER_SUPPLY_TYPE_USB)
			val->intval = (charger ==  CHARGER_USB ? 1 : 0);
		else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int __init dummy_battery_init(void)
{
	int ret;
	struct dummy_battery_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err_data_alloc_failed;
	}

	/* Battey */
	data->battery.name = "battery";
	data->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	data->battery.properties = dummy_battery_props;
	data->battery.num_properties = ARRAY_SIZE(dummy_battery_props);
	data->battery.get_property = dummy_battery_get_property;

	/* USB */
	data->usb.name = "usb";
	data->usb.type = POWER_SUPPLY_TYPE_USB;
	data->usb.properties = dummy_power_props;
	data->usb.num_properties = ARRAY_SIZE(dummy_power_props);
	data->usb.get_property = dummy_power_get_property;

	/* AC */
	data->ac.name = "ac";
	data->ac.type = POWER_SUPPLY_TYPE_MAINS;
	data->ac.properties = dummy_power_props;
	data->ac.num_properties = ARRAY_SIZE(dummy_power_props);
	data->ac.get_property = dummy_power_get_property;

	battery_data = data;

	/* Dummy battery setting (always 100%) */
	battery_data->level = 100;
	battery_data->charger = CHARGER_AC;
	battery_data->battery_present = 1;

	ret = power_supply_register(NULL, &data->battery);
	if (ret)
		goto err_battery_failed;

	ret = power_supply_register(NULL, &data->usb);
	if (ret)
		goto err_usb_failed;

	ret = power_supply_register(NULL, &data->ac);
	if (ret)
		goto err_ac_failed;

	return 0;

err_ac_failed:
	power_supply_unregister(&data->usb);
err_usb_failed:
	power_supply_unregister(&data->battery);
err_battery_failed:
	kfree(data);
	battery_data = NULL;
err_data_alloc_failed:
	return ret;
}

static void __exit dummy_battery_exit(void)
{
	struct dummy_battery_data *data = battery_data;

	power_supply_unregister(&data->battery);
	power_supply_unregister(&data->usb);
	power_supply_unregister(&data->ac);

	kfree(data);
	battery_data = NULL;
}

module_init(dummy_battery_init);
module_exit(dummy_battery_exit);

MODULE_AUTHOR("Renesas");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dummy Battery driver");
