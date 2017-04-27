/*
 * d2153-i2c.c: I2C (Serial Communication) driver for D2153
 *
 * Copyright(c) 2012 Dialog Semiconductor Ltd.
 *
 * Author: Dialog Semiconductor Ltd. D. Chen, D. Patel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/d2153/pmic.h>
#include <linux/d2153/d2153_reg.h>
#include <linux/d2153/hwmon.h>
#include <linux/d2153/rtc.h>
#include <linux/d2153/core.h>

#include <mach/common.h>

static int d2153_i2c_read_device(struct d2153 *d2153, char reg,
					int bytes, void *dest)
{
	int ret;
	struct i2c_msg msgs[2];
	struct i2c_adapter *adap = d2153->pmic_i2c_client->adapter;

	msgs[0].addr = d2153->pmic_i2c_client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;

	msgs[1].addr = d2153->pmic_i2c_client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = bytes;
	msgs[1].buf = (char *)dest;

	mutex_lock(&d2153->i2c_mutex);

	ret = i2c_transfer(adap,msgs,ARRAY_SIZE(msgs));

	mutex_unlock(&d2153->i2c_mutex);

	if (ret < 0 )
		return ret;
	else if (ret == ARRAY_SIZE(msgs))
		return 0;
	else
		return -EFAULT;
}

static int d2153_i2c_write_device(struct d2153 *d2153, char reg,
				   int bytes, u8 *src )
{
	int ret;
	struct i2c_msg msgs[1];
	u8 data[12];
	u8 *buf = data;

	struct i2c_adapter *adap = d2153->pmic_i2c_client->adapter;

	if (bytes == 0)
		return -EINVAL;

	BUG_ON(bytes >= ARRAY_SIZE(data));

	msgs[0].addr = d2153->pmic_i2c_client->addr;
	msgs[0].flags = d2153->pmic_i2c_client->flags & I2C_M_TEN;
	msgs[0].len = 1+bytes;
	msgs[0].buf = data;

	*buf++ = reg;
	while(bytes--) *buf++ = *src++;

	mutex_lock(&d2153->i2c_mutex);

	ret = i2c_transfer(adap,msgs,ARRAY_SIZE(msgs));

	mutex_unlock(&d2153->i2c_mutex);

	if (ret < 0 )
		return ret;
	else if (ret == ARRAY_SIZE(msgs))
		return 0;
	else
		return -EFAULT;
}

static int d2153_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct d2153 *d2153;
	int ret = 0;

	dlg_info("%s() Starting I2C\n", __FUNCTION__);

	d2153 = kzalloc(sizeof(struct d2153), GFP_KERNEL);
	if (d2153 == NULL) {
		kfree(i2c);
		return -ENOMEM;
	}

	i2c_set_clientdata(i2c, d2153);
	d2153->dev = &i2c->dev;
	d2153->pmic_i2c_client = i2c;

	mutex_init(&d2153->i2c_mutex);

	d2153->read_dev = d2153_i2c_read_device;
	d2153->write_dev = d2153_i2c_write_device;

	ret = d2153_device_init(d2153, i2c->irq, i2c->dev.platform_data);
	dev_info(d2153->dev, "I2C initialized err=%d\n",ret);
	if (ret < 0)
		goto err;


	dlg_info("%s() Finished I2C setup\n",__FUNCTION__);
	return ret;

err:
	kfree(d2153);
	return ret;
}

static int d2153_i2c_remove(struct i2c_client *i2c)
{
	struct d2153 *d2153 = i2c_get_clientdata(i2c);

	d2153_device_exit(d2153);
	kfree(d2153);
	return 0;
}


static const struct i2c_device_id d2153_i2c_id[] = {
	{ D2153_I2C, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, d2153_i2c_id);


static struct i2c_driver d2153_i2c_driver = {
	.driver = {
		   .name = D2153_I2C,
		   .owner = THIS_MODULE,
	},
	.probe = d2153_i2c_probe,
	.remove = d2153_i2c_remove,
	.id_table = d2153_i2c_id,
};

static int __init d2153_i2c_init(void)
{
	return i2c_add_driver(&d2153_i2c_driver);
}

/* Initialised very early during bootup (in parallel with Subsystem init) */
subsys_initcall(d2153_i2c_init);

static void __exit d2153_i2c_exit(void)
{
	i2c_del_driver(&d2153_i2c_driver);
}
module_exit(d2153_i2c_exit);

MODULE_AUTHOR("Dialog Semiconductor Ltd < william.seo@diasemi.com >");
MODULE_DESCRIPTION("I2C MFD driver for Dialog D2153 PMIC plus Audio");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" D2153_I2C);
