/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>

#include "ist30xx.h"
#include "ist30xx_update.h"

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
#include <linux/input/mt.h>
#endif

#if IST30XX_DEBUG
#include "ist30xx_misc.h"
#endif

#define MAX_ERR_CNT             (2)

#if IST30XX_USE_KEY
int ist30xx_key_code[] = { 0, KEY_MENU, KEY_BACK };
#endif

#if IST30XX_DETECT_TA
static int ist30xx_ta_status = -1;
#endif

DEFINE_MUTEX(ist30xx_mutex);

extern struct class *sec_class;
struct device *sec_touchkey_dev;

struct ist30xx_data *ts_data;
static struct delayed_work work_reset_check;

static struct regulator *keyled_regulator;
static int prev_value;



void ist30xx_disable_irq(struct ist30xx_data *data)
{
	if (data->irq_enabled) {
		disable_irq(data->client->irq);
		data->irq_enabled = 0;
	}
}

void ist30xx_enable_irq(struct ist30xx_data *data)
{
	if (!data->irq_enabled) {
		enable_irq(data->client->irq);
		msleep(50);
		data->irq_enabled = 1;
	}
}


int ist30xx_error_cnt = 0;
static void ist30xx_request_reset(void)
{
	ist30xx_error_cnt++;

	if (ist30xx_error_cnt >= MAX_ERR_CNT) {
		schedule_delayed_work(&work_reset_check, 0);
		DMSG("[ TSP ] ist30xx_request_reset!\n");
		ist30xx_error_cnt = 0;
	}
}


void ist30xx_start(struct ist30xx_data *data)
{
#if IST30XX_DETECT_TA
	if (ist30xx_ta_status > -1) {
		ist30xx_write_cmd(data->client, CMD_SET_TA_MODE, ist30xx_ta_status);

		DMSG("[ TSP ] ist30xx_start, ta_mode : %d\n",
		     ist30xx_ta_status);
	}
#endif

	ist30xx_cmd_start_scan(data->client);
}


static int ist30xx_get_ver_info(struct ist30xx_data *data)
{
	int ret;

	data->fw.pre_ver = data->fw.ver;
	data->fw.ver = 0;

	ret = ist30xx_read_cmd(data->client, CMD_GET_CHIP_ID, &data->chip_id);
	if (ret)
		return -EIO;

	ret = ist30xx_read_cmd(data->client, CMD_GET_FW_VER, &data->fw.ver);
	if (ret)
		return -EIO;

	ret = ist30xx_read_cmd(data->client, CMD_GET_PARAM_VER, &data->param_ver);
	if (ret)
		return -EIO;

	DMSG("[ TSP ] Chip ID : %x F/W: %x Param: %x\n",
	     data->chip_id, data->fw.ver, data->param_ver);

	return 0;
}


int ist30xx_init_touch_driver(struct ist30xx_data *data)
{
	int ret;

	ist30xx_disable_irq(data);

	ret = ist30xx_cmd_run_device(data->client);

	ist30xx_get_ver_info(data);

	ist30xx_start(data);

	ist30xx_enable_irq(data);

	return 0;
}


#define PRESS_MSG_MASK          (0x01)
#define MULTI_MSG_MASK          (0x02)
#define PRESS_MSG_KEY           (0x6)
#define CALIB_MSG_MASK          (0xF0000FFF)
#define CALIB_MSG_VALID         (0x80000CAB)
static void clear_input_data(struct ist30xx_data *data)
{
	int i, pressure, count;

	for (i = 0, count = 0; i < IST30XX_MAX_MT_FINGERS; i++) {
		if (data->fingers[i].bit_field.id == 0)
			continue;

		pressure = (data->fingers[i].bit_field.udmg & PRESS_MSG_MASK);
		if (pressure) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
			input_mt_slot(data->input_dev, data->fingers[i].bit_field.id - 1);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
#else
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					 data->fingers[i].bit_field.x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					 data->fingers[i].bit_field.y);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
					 0);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR,
					 0);
			input_mt_sync(data->input_dev);
#endif

			data->fingers[i].bit_field.id = 0;

			count++;
		}
	}

	if (count > 0)
		input_sync(data->input_dev);
}


static void report_input_data(struct ist30xx_data *data, int finger_counts, int key_counts)
{
	int i, pressure, count;

	for (i = 0, count = 0; i < finger_counts; i++) {
		if ((data->fingers[i].bit_field.id == 0) ||
		    (data->fingers[i].bit_field.id > IST30XX_MAX_MT_FINGERS) ||
		    (data->fingers[i].bit_field.x > IST30XX_MAX_X) ||
		    (data->fingers[i].bit_field.y > IST30XX_MAX_Y)) {
			pr_err("[ TSP ] Error, [%d][%d] - [%d][%d]\n", i,
			       data->fingers[i].bit_field.id,
			       data->fingers[i].bit_field.x,
			       data->fingers[i].bit_field.y);

			data->fingers[i].bit_field.id = 0;
			ist30xx_request_reset();
			continue;
		}

		pressure = data->fingers[i].bit_field.udmg & PRESS_MSG_MASK;

#if 0
		DMSG("[ TSP ] [%d][%d][%d] x, y, z = %03d, %03d, %04d\n", i,
		     data->fingers[i].bit_field.id, pressure,
		     data->fingers[i].bit_field.x,
		     data->fingers[i].bit_field.y,
		     data->fingers[i].bit_field.w << 5);
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
		input_mt_slot(data->input_dev, data->fingers[i].bit_field.id - 1);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER,
					   (pressure ? true : false));
		if (pressure) {
#endif
		input_report_abs(data->input_dev, ABS_MT_POSITION_X,
				 data->fingers[i].bit_field.x);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
				 data->fingers[i].bit_field.y);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
				 data->fingers[i].bit_field.w);
	}
#else
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
				 pressure);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR,
				 data->fingers[i].bit_field.w);
		input_mt_sync(data->input_dev);
#endif

	count++;

	ist30xx_error_cnt = 0;
}

#if IST30XX_USE_KEY
for (i = finger_counts; i < finger_counts + key_counts; i++) {
	int press, id;
	press = (data->fingers[i].bit_field.w == PRESS_MSG_KEY) ? 1 : 0;
	id = data->fingers[i].bit_field.id;
	input_report_key(data->input_dev, ist30xx_key_code[id], press);
	count++;
}
#endif

if (count > 0)
	input_sync(data->input_dev);
}

/*
 * CMD : CMD_GET_COORD
 *               [31:30]  [29:26]  [25:16]  [15:10]  [9:0]
 *   Multi(1st)  UDMG     Rsvd.    NumOfKey Rsvd.    NumOfFinger
 *    Single &   UDMG     ID       X        Area     Y
 *   Multi(2nd)
 *
 *   UDMG [31] 0/1 : single/multi
 *   UDMG [30] 0/1 : unpress/press
 */
static irqreturn_t ist30xx_irq_thread(int irq, void *ptr)
{
	int i, ret;
	int key_cnt, finger_cnt, read_cnt;
	struct ist30xx_data *data = ptr;
	u32 msg[IST30XX_MAX_MT_FINGERS];

	if (!data->irq_enabled)
		return IRQ_HANDLED;

	memset(msg, 0, IST30XX_MAX_MT_FINGERS);
	ret = ist30xx_get_position(data->client, msg, 1);
	if (ret)
		goto irq_err;

	//DMSG("[ TSP ] intr thread msg: 0x%08x\n", *msg);

	if (msg[0] == 0)
		return IRQ_HANDLED;

	if ((msg[0] & CALIB_MSG_MASK) == CALIB_MSG_VALID) {
		data->status.calib = msg[0];
		return IRQ_HANDLED;
	}

	for (i = 0; i < IST30XX_MAX_MT_FINGERS; i++)
		data->fingers[i].full_field = 0;

	key_cnt = 0;
	finger_cnt = 1;
	read_cnt = 1;
	data->fingers[0].full_field = msg[0];

	if (data->fingers[0].bit_field.udmg & MULTI_MSG_MASK) {
		key_cnt = data->fingers[0].bit_field.x;
		finger_cnt = data->fingers[0].bit_field.y;
		read_cnt = finger_cnt + key_cnt;

		if (read_cnt > IST30XX_MAX_MT_FINGERS)
			goto irq_err;

#if I2C_BURST_MODE
		ret = ist30xx_get_position(data->client, msg, read_cnt);
		if (ret)
			goto irq_err;

		for (i = 0; i < read_cnt; i++)
			data->fingers[i].full_field = msg[i];
#else
		for (i = 0; i < read_cnt; i++) {
			ret = ist30xx_get_position(data->client, &msg[i], 1);
			if (ret)
				goto irq_err;

			data->fingers[i].full_field = msg[i];
		}
#endif
	}

	if (read_cnt > 0)
		report_input_data(data, finger_cnt, key_cnt);

	return IRQ_HANDLED;

irq_err:
	pr_err("[ TSP ] intr msg[0]: 0x%08x, ret: %d\n", msg[0], ret);
	ist30xx_request_reset();
	return IRQ_HANDLED;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
#define ist30xx_suspend NULL
#define ist30xx_resume  NULL
static void ist30xx_early_suspend(struct early_suspend *h)
{
	struct ist30xx_data *data = container_of(h, struct ist30xx_data,
						 early_suspend);

	mutex_lock(&ist30xx_mutex);
//	touchkey_led_on(data,false); // temp code
	ist30xx_disable_irq(data);
	ist30xx_internal_suspend(data);
	mutex_unlock(&ist30xx_mutex);
}
static void ist30xx_late_resume(struct early_suspend *h)
{
	struct ist30xx_data *data = container_of(h, struct ist30xx_data,
						 early_suspend);

	mutex_lock(&ist30xx_mutex);
//	touchkey_led_on(data,true);  // temp code
	ist30xx_internal_resume(data);
	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

	ist30xx_start(data);
}
#else
static int ist30xx_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	return ist30xx_internal_suspend(data);
}
static int ist30xx_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	return ist30xx_internal_resume(data);
}
#endif


void ist30xx_set_ta_mode(bool charging)
{
#if IST30XX_DETECT_TA
	if ((ist30xx_ta_status == -1) || (charging == ist30xx_ta_status))
		return;

	ist30xx_ta_status = charging ? 1 : 0;
	schedule_delayed_work(&work_reset_check, 0);
#endif
}
EXPORT_SYMBOL(ist30xx_set_ta_mode);


static void reset_work_func(struct work_struct *work)
{
	if ((ts_data == NULL) || (ts_data->client == NULL))
		return;

	DMSG("[ TSP ] Request reset function\n");

	if ((ts_data->status.power == 1) && (ts_data->status.fw_update != 1)) {
		mutex_lock(&ist30xx_mutex);
		ist30xx_disable_irq(ts_data);

		clear_input_data(ts_data);

		ist30xx_cmd_run_device(ts_data->client);

		ist30xx_start(ts_data);

		ist30xx_enable_irq(ts_data);
		mutex_unlock(&ist30xx_mutex);
	}
}



void touchkey_led_on(struct ist30xx_data *data, bool on)
{
	int ret;
	printk("touchkey_led_on = %d\n", on);

	if(keyled_regulator == NULL)
	{
		printk(" %s, %d \n", __func__, __LINE__ );			
		keyled_regulator = regulator_get(NULL, "key_led"); 
		if(IS_ERR(keyled_regulator)){
			printk("can not get KEY_LED_3.3V\n");
			return;
		}
		ret = regulator_set_voltage(keyled_regulator,3300000,3300000);
		printk("regulator_set_voltage ret = %d \n", ret);
		
	}

	if(on)
	{
		printk(" %s, %d Touchkey On\n", __func__, __LINE__ );	

		ret = regulator_enable(keyled_regulator);
		printk("regulator_enable ret = %d \n", ret);
	}
	else
	{
		printk("%s, %d Touchkey Off\n", __func__, __LINE__ );	

		ret = regulator_disable(keyled_regulator);
		printk("regulator_disable ret = %d \n", ret);	

	}


}


static void key_led_set(struct led_classdev *led_cdev,
			      enum led_brightness value)
{
	struct ist30xx_data *data = container_of(led_cdev, struct ist30xx_data, led);
	struct i2c_client *client = data->client;


	data->led_brightness = value;

	printk("[TouchKey] data->led_brightness=%d, prev_value=%d\n",data->led_brightness, prev_value);

	if( value >= 1 && prev_value == 0)
	{
		touchkey_led_on(data, 1);
	}
	else if( value==0 && prev_value != 0)
	{
		touchkey_led_on(data, 0);
	}
	prev_value=value;

		
}


static ssize_t touchkey_led_control(struct device *dev,
				 struct device_attribute *attr, const char *buf, size_t size)
{
	int data;
	int ret;

	
	ret = sscanf(buf, "%d", &data);
	
	if (ret != 1) {
		printk(KERN_EMERG "%s, %d err\n",__func__, __LINE__);
		return size;
	}
		
	if (keyled_regulator == NULL) {
		printk(" %s, %d \n", __func__, __LINE__ );			
		keyled_regulator = regulator_get(NULL, "key_led"); 
		if(IS_ERR(keyled_regulator)){
			printk("can not get KEY_LED_3.3V\n");
			return -1;
		}
		ret = regulator_set_voltage(keyled_regulator,3300000,3300000);
		printk("regulator_set_voltage ret = %d \n", ret);
	}

	if (data) {
		printk(" %s, %d Touchkey LED On\n", __func__, __LINE__ );	

		ret = regulator_enable(keyled_regulator);
		printk("regulator_enable ret = %d \n", ret);
	} else {
		printk("%s, %d Touchkey LED Off\n", __func__, __LINE__ );	

		ret = regulator_disable(keyled_regulator);
		printk("regulator_disable ret = %d \n", ret);	
	}

	return size;
}



static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, touchkey_led_control,  touchkey_led_control);


static struct attribute *fac_attributes[] = {
	&dev_attr_brightness.attr,
	NULL,
};


static struct attribute_group fac_attr_group = {
	.attrs = fac_attributes,
};


static int __devinit ist30xx_probe(struct i2c_client *		client,
				   const struct i2c_device_id * id)
{
	int ret;
	struct ist30xx_data *data;
	struct input_dev *input_dev;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	input_dev = input_allocate_device();
	if (!input_dev) {
		ret = -ENOMEM;
		pr_err("%s: input_allocate_device failed (%d)\n", __func__, ret);
		goto err_alloc_dev;
	}

	data->num_fingers = IST30XX_MAX_MT_FINGERS;
	data->irq_enabled = 1;
	data->client = client;
	data->input_dev = input_dev;
	i2c_set_clientdata(client, data);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
	input_mt_init_slots(input_dev, IST30XX_MAX_MT_FINGERS);
#endif

	input_dev->name = "ist30xx_ts_input";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, IST30XX_MAX_X, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, IST30XX_MAX_Y, 0, 0);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 0, 0))
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, IST30XX_MAX_W, 0, 0);
#else
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, IST30XX_MAX_Z, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, IST30XX_MAX_W, 0, 0);
#endif

#if IST30XX_USE_KEY
	{
		int i;
		set_bit(EV_KEY, input_dev->evbit);
		set_bit(EV_SYN, input_dev->evbit);
		for (i = 1; i < ARRAY_SIZE(ist30xx_key_code); i++)
			set_bit(ist30xx_key_code[i], input_dev->keybit);
	}
#endif

	input_set_drvdata(input_dev, data);
	ret = input_register_device(input_dev);
	if (ret) {
		input_free_device(input_dev);
		goto err_reg_dev;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = ist30xx_early_suspend;
	data->early_suspend.resume = ist30xx_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	ts_data = data;

	ret = ist30xx_init_system();
	if (ret) {
		dev_err(&client->dev, "chip initialization failed\n");
		goto err_init_drv;
	}

	ret = request_threaded_irq(client->irq, NULL, ist30xx_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ist30xx_ts", data);
	if (ret)
		goto err_irq;

	ret = ist30xx_init_update_sysfs();
	if (ret)
		goto err_init_drv;

#if IST30XX_DEBUG
	ret = ist30xx_init_misc_sysfs();
	if (ret)
		goto err_init_drv;
#endif

	ist30xx_init_touch_driver(data);

# if IST30XX_INTERNAL_BIN
	ist30xx_auto_fw_update(data);
	ist30xx_auto_param_update(data);
# endif

	INIT_DELAYED_WORK(&work_reset_check, reset_work_func);

#if IST30XX_DETECT_TA
	ist30xx_ta_status = 0;
#endif

	/*key led ++*/
	data->led.name = "button-backlight";
	data->led.brightness = LED_OFF;
	data->led.max_brightness = LED_FULL;
	data->led.brightness_set = key_led_set;

	ret = led_classdev_register(&client->dev, &data->led);
	if (ret) {
		dev_err(&client->dev, "fail to register led_classdev (%d).\n", ret);
	}
	/*key led --*/	


#if defined(SEC_FAC_TK)
	sec_touchkey_dev = device_create(sec_class, NULL, 0, data, "sec_touchkey");
	if (IS_ERR(sec_touchkey_dev))
		dev_err(&client->dev, "Failed to create fac tsp temp dev\n");

	ret = sysfs_create_group(&sec_touchkey_dev->kobj, &fac_attr_group);
	if (ret)
		dev_err(&client->dev, "%s: failed to create fac_attr_group (%d)\n", __func__, ret);

#endif


	return 0;

err_irq:
err_init_drv:
	pr_err("[ TSP ] Error, ist30xx init driver\n");
	ist30xx_power_off();
	input_unregister_device(input_dev);

err_reg_dev:
err_alloc_dev:
	pr_err("[ TSP ] Error, ist30xx mem free\n");
	kfree(data);
	return ret;
	return 0;
}


static int __devexit ist30xx_remove(struct i2c_client *client)
{
	struct ist30xx_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

	free_irq(client->irq, data);
	ist30xx_power_off();

	input_unregister_device(data->input_dev);
	kfree(data);

	return 0;
}


static struct i2c_device_id ist30xx_idtable[] = {
	{ IST30XX_DEV_NAME, 0 },
	{},
};


MODULE_DEVICE_TABLE(i2c, ist30xx_idtable);

#ifdef CONFIG_HAS_EARLYSUSPEND
static const struct dev_pm_ops ist30xx_pm_ops = {
	.suspend	= ist30xx_suspend,
	.resume		= ist30xx_resume,
};
#endif


static struct i2c_driver ist30xx_i2c_driver = {
	.id_table	= ist30xx_idtable,
	.probe		= ist30xx_probe,
	.remove		= __devexit_p(ist30xx_remove),
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= IST30XX_DEV_NAME,
#ifdef CONFIG_HAS_EARLYSUSPEND
		.pm	= &ist30xx_pm_ops,
#endif
	},
};


static int __init ist30xx_init(void)
{
	return i2c_add_driver(&ist30xx_i2c_driver);
}


static void __exit ist30xx_exit(void)
{
	i2c_del_driver(&ist30xx_i2c_driver);
}

module_init(ist30xx_init);
module_exit(ist30xx_exit);

MODULE_DESCRIPTION("Imagis IST30XX touch driver");
MODULE_LICENSE("GPL");
