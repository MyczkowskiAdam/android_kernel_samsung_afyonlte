/* drivers/mfd/richtek/rt9524.c
 * Driver to Richtek RT9524 linear charger
 *
 * Copyright (C) 2012
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/pm_runtime.h>
#include <linux/platform_data/rt_charger.h>
#include "rt_comm_defs.h"

#define DEVICE_NAME "rt9524"
#define RT9524_DRV_NAME "rt9524"

#define PLUSE_H_TIME 500
#define PLUSE_L_TIME 500
#define RESET_TIME 1600

void rt9524_setmode(int nMode);
int nRT9542Mode = 0;
static char* mode_name_str[] = { "USB500","ISET","USB100","FACTORY","USE100"};

static ssize_t mode_name_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"%s\n",mode_name_str[(nRT9542Mode>4)?4:nRT9542Mode]);
}

static ssize_t mode_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"%d\n",nRT9542Mode);
}

static ssize_t mode_store(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               const char *buf, size_t len)
{
    uint32_t value = simple_strtoul(buf, NULL, 10);
    rt9524_setmode(value);
    return len;
}

static struct kobj_attribute mode_name_attribute = (struct kobj_attribute)__ATTR_RO(mode_name);
static struct kobj_attribute mode_attribute = (struct kobj_attribute)__ATTR_RW(mode);
static struct attribute* rt9524_attrs [] =
{
    &mode_name_attribute.attr,
    &mode_attribute.attr,
    NULL,
};



inline void rt9524_reset(void)
{
	gpio_set_value(CONFIG_RT9524_USE_GPIO_NUMBER,1);
	udelay(RESET_TIME);
}
void rt9524_setmode(int nMode)
{
	int nCount;
	if (nMode>4)
		nMode = 4;
	nRT9542Mode = nMode;
	rt9524_reset();
	gpio_set_value(CONFIG_RT9524_USE_GPIO_NUMBER,0);
	udelay(80);
	for (nCount=0;nCount <nMode;nCount++)
	{
		gpio_set_value(CONFIG_RT9524_USE_GPIO_NUMBER,1);
		udelay(PLUSE_H_TIME);
		gpio_set_value(CONFIG_RT9524_USE_GPIO_NUMBER,0);
		udelay(PLUSE_L_TIME);		
	}
}
EXPORT_SYMBOL(rt9524_setmode);
void rt9524_disable(void)
{
	rt9524_reset();
}
EXPORT_SYMBOL(rt9524_disable);


static struct platform_device rt9524_chg_device = {
	.name	= RT9524_DRV_NAME,
	.id	= -1,
};

static void rt9524_dev_register(void)
{
	platform_device_register(&rt9524_chg_device);
}
static int rt_sysfs_create_files(struct kobject *kobj,struct attribute** attrs)
{
    int err;
    while(*attrs!=NULL)
    {
        err = sysfs_create_file(kobj,*attrs);
        if (err)
            return err;
        attrs++;
    }
    return 0;
}

static int rt9524_probe(struct platform_device *pdev)
{
	int err;
	err = gpio_request(CONFIG_RT9524_USE_GPIO_NUMBER,"RT9524_SET_ENABLE");
	if (err<0)
	{
		ERR("Can't get GPIO %d\r\n",CONFIG_RT9524_USE_GPIO_NUMBER);
		return err;
	}
	err = gpio_direction_output(CONFIG_RT9524_USE_GPIO_NUMBER,1);
    	if (err<0)
	{
        	ERR("Set GPIO Direction to output : failed\n");
		return err;
	}
	err = rt_sysfs_create_files(&(rt9524_chg_device.dev.kobj),rt9524_attrs);

	rt9524_setmode(0);
	INFO("RT9524 module initializd -- OK.\n");
	return 0;
}
void rt9524_usb500(struct platform_device *pdev)
{
	rt9524_setmode(0);
}

static struct platform_driver rt9524_driver = {
	.driver = { .name = RT9524_DRV_NAME, },
	.probe = rt9524_probe,
	.shutdown = rt9524_usb500,
	
};

static int __init rt9524_init(void)
{
	rt9524_dev_register();
	INFO("RT9524 module start initializing.\n");
	return platform_driver_register(&rt9524_driver);
}

static void __exit rt9524_exit(void)
{
	platform_driver_unregister(&rt9524_driver);
	INFO("RT9524 module deinitialized.\n");
}

MODULE_AUTHOR("Patrick Chang <patrick_chang@richtek.com>");
MODULE_DESCRIPTION("Richtek RT9524 linear charger device driver");
MODULE_LICENSE("GPL");
module_init(rt9524_init);
module_exit(rt9524_exit);
