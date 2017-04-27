/*
 * arch/arm/mach-shmobile/flash-control.c
 *
 * Copyright (C) 2012 Renesas Mobile Corp.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/miscdevice.h>
#include <linux/fs.h>

#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/wakelock.h>

#include <mach/r8a7373.h>

/* Macro */
/* Define register */
#define BRIGHTNESS_REGISTER 0x02 /* Register 02h to set brightness
				* for both assist light and flash light
				*/
#define ONOFF_TIME_REGISTER 0x03 /* Register 03h set assist light
					 * or flash ON/OFF
					 * and set time for flash
					 */

/* Init value */
#define ASSISTCUR_DEFAULT 0x20 /* Default value 72mA */
#define ASSISTLIGHT_ON 0x20
#define ASSISTLIGHT_OFF 0x00

#define FLASHLIGHT_OFF 0x14 /* Flash light OFF
				* Flash time default value 150 ms
				*/
#define FLASHLIGHT_ON 0x30 /* Flash light ON */

#define FLASHLIGHT_BRIGHTNESS_LOW 0x00 /* 260mA */
#define FLASHLIGHT_BRIGHTNESS_MID 0x40 /* 280mA */
#define FLASHLIGHT_BRIGHTNESS_HIGH 0x80 /* 300mA */
#define FLASHLIGHT_BRIGHTNESS_MAX 0xC0 /* 320mA */

/* Enum for flash type */
enum flash_type {
	ASSISTLIGHT, /* 0: Set brightness in assist mode */
	FLASHLIGHT, /* 1: Set brightness in flash mode */
	FLASH_NUM, /* 2: flash_type Max num */
};

/* Variable */
static struct i2c_client *flash_client;
static struct wake_lock wakelock;
static int flash_onoff; /* 0: Flash OFF
							 * 1: Flash ON */
static const int time[] = {
			30, 60, 90, 120, 150, 180, 210, 240,
			270, 300, 330, 360, 390, 420, 450, 480};
DEFINE_MUTEX(flash_mutex); /* Initialize mutex */

/* Function prototype */
static void led_assistlight_set(struct led_classdev *led_cdev,
					enum led_brightness value);
static void led_flashlight_set(struct led_classdev *led_cdev,
					enum led_brightness value);
static ssize_t led_flashtime_set_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static s32 wrap_flash_i2c_write_data(u8 command, u8 value);
static s32 wrap_flash_i2c_read_data(u8 command);
static int flash_probe(struct i2c_client *client,
			const struct i2c_device_id *id);
static __init int flash_dev_init(void);
static DEVICE_ATTR(time, 0644, NULL, led_flashtime_set_store);

/*
 * led_assistlight_set: Turn ON/OFF torch light (assist mode)
 * @led_cdev: Pointer to flash device
 * @value: Brightness value.
 *         0: OFF
 *        >0: ON with brightness is 72 mA
 * return: None
 */
static void led_assistlight_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	int ret = 0;
	mutex_lock(&flash_mutex);

	if (value > 0) {
		/* Assist light ON */
		ret = wrap_flash_i2c_write_data(BRIGHTNESS_REGISTER,
						ASSISTCUR_DEFAULT);
		if (ret) {
			printk(KERN_ERR "(%s[%d]) i2c write err\n",
					__func__, __LINE__);
		} else {
			ret = wrap_flash_i2c_write_data(ONOFF_TIME_REGISTER,
							ASSISTLIGHT_ON);
			if (ret) {
				printk(KERN_ERR "(%s[%d]) i2c write err\n",
						__func__, __LINE__);
			}
		}
	} else {
		/* Assist light OFF */
		ret = wrap_flash_i2c_write_data(ONOFF_TIME_REGISTER,
						ASSISTLIGHT_OFF);
		if (ret) {
			printk(KERN_ERR "(%s[%d]) i2c write err\n",
					__func__, __LINE__);
		}
	}
	mutex_unlock(&flash_mutex);
}

/*
 * led_flashlight_set: Set brightness for flash in camera flash mode
 * @led_cdev: Pointer to flash device
 * @value: Brightness value
 * return: None
 */
static void led_flashlight_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	int ret = 0;
	mutex_lock(&flash_mutex);

	if (value <= 0) {
		/* Flash light OFF */
		flash_onoff = 0;
	} else if ((value >= 1) && (value <= 63)) {
		/* Flash light ON with 260mA */
		flash_onoff = 1;
		ret = wrap_flash_i2c_write_data(BRIGHTNESS_REGISTER,
					FLASHLIGHT_BRIGHTNESS_LOW);
		if (ret) {
			printk(KERN_ERR "(%s[%d]) i2c write err\n",
					__func__, __LINE__);
			flash_onoff = 0;
		}
	} else if ((value >= 64) && (value <= 127)) {
		/* Flash light ON with 280mA */
		flash_onoff = 1;
		ret = wrap_flash_i2c_write_data(BRIGHTNESS_REGISTER,
					FLASHLIGHT_BRIGHTNESS_MID);
		if (ret) {
			printk(KERN_ERR "(%s[%d]) i2c write err\n",
					__func__, __LINE__);
			flash_onoff = 0;
		}
	} else if ((value >= 128) && (value <= 191)) {
		/* Flash light ON with 300mA */
		flash_onoff = 1;
		ret = wrap_flash_i2c_write_data(BRIGHTNESS_REGISTER,
					FLASHLIGHT_BRIGHTNESS_HIGH);
		if (ret) {
			printk(KERN_ERR "(%s[%d]) i2c write err\n",
					__func__, __LINE__);
			flash_onoff = 0;
		}
	} else {
		/* Flash light ON with max current 320mA */
		flash_onoff = 1;
		ret = wrap_flash_i2c_write_data(BRIGHTNESS_REGISTER,
					FLASHLIGHT_BRIGHTNESS_MAX);
		if (ret) {
			printk(KERN_ERR "(%s[%d]) i2c write err\n",
					__func__, __LINE__);
			flash_onoff = 0;
		}
	}
	mutex_unlock(&flash_mutex);
}

/*
 * led_flashtime_set_store: Turn ON/OFF and set time for flash
 * @dev: Pointer to struct of device
 * @attr: Pointer to struct device_attribute
 * @buf: Buffer to contain data of device file
 * @count: Size of buffer
 * return:
 *   + Length of the buffer: Success
 *   + -EINVAL(-22): Invalid parameter
 *   + Other: Error returned by I2C function
 */
static ssize_t led_flashtime_set_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int value = 0; /* Value of time */
	int time_set = 0; /* Value of time, mode and ON/OFF
				to set to register */
	int scan_result = 0; /* Function return value */
	int err = count; /* Function return value */
	int ret = 0; /* Function return value */
	int i = 0;
	int input = 0;
	int tmp = 0;

	mutex_lock(&flash_mutex);

	if (!flash_onoff) {
		/* Turn OFF flash light*/
		ret = wrap_flash_i2c_write_data(ONOFF_TIME_REGISTER,
						FLASHLIGHT_OFF);
		if (ret) {
			err = ret;
			printk(KERN_ERR "(%s[%d]) i2c write err\n",
					__func__, __LINE__);
		}
	} else {
		scan_result = sscanf(buf, "%d", &input);
		if (1 != scan_result) {
			/* Failed */
			mutex_unlock(&flash_mutex);
			return -EINVAL;
		}

		printk(KERN_INFO "input = %x\n", input);
		if (input <= 0) {
			/* Invalid value */
			mutex_unlock(&flash_mutex);
			return -EINVAL;
		} else if (input >= time[15]) {
			value = 15;
		} else {
			for (i = 1; i < 15; i++) {
				if (input <= time[i]) {
					value = i;
					break;
				}
			}
			tmp = (time[value] + time[value - 1]) >> 1;
			if (tmp > input)
				value = i - 1;
		}

		time_set = FLASHLIGHT_ON | value;
		/* Turn ON flash light and set time*/
		ret = wrap_flash_i2c_write_data(ONOFF_TIME_REGISTER,
						time_set);
		if (ret) {
			err = ret;
			printk(KERN_ERR "(%s[%d]) i2c write err\n",
					__func__, __LINE__);
		}
	}

	mutex_unlock(&flash_mutex);
	return err;
}

/*
 * wrap_flash_i2c_write_data: Write value to FLASH hardware register
 * @command: Address of flash hardware register
 * @value: Value to set to register
 * return:
 *   0: Success
 *   Other: Error returned by i2c_smbus_write_byte_data() function
 */
static s32 wrap_flash_i2c_write_data(u8 command, u8 value)
{
	int err = 0;
	int retry = 0;

	for (retry = 0; retry < 5; retry++) {
		wake_lock(&wakelock);
		err = i2c_smbus_write_byte_data(flash_client, command, value);
		wake_unlock(&wakelock);
		if (!err) {
			/* Success */
			break;
		} else {
			/* Failed */
			break;
			/* printk(KERN_ERR "i2c write error %d\n", err); */
		}
	}
	return err;
}

/*
 * wrap_flash_i2c_read_data: Read value from FLASH hardware register
 * @command: Address of FLASH hardware register
 * return:
 *   >=0: Success
 *   Other: Error returned by i2c_smbus_read_byte_data() function
 */
static s32 wrap_flash_i2c_read_data(u8 command)
{
	int ret_val = 0;
	int retry = 0;

	for (retry = 0; retry < 5; retry++) {
		wake_lock(&wakelock);
		ret_val = i2c_smbus_read_byte_data(flash_client, command);
		wake_unlock(&wakelock);
		if (ret_val >= 0) {
			/* Success */
			break;
		} else {
			/* Failed */
			break;
			/* printk(KERN_ERR "i2c read error %d\n", ret_val); */
		}
	}
	return ret_val;
}

/* List of LED device */
static struct led_classdev flash_lights[] = {
	[ASSISTLIGHT] = {
		.name = "spotlight",
		.brightness_set = led_assistlight_set,
	},
	[FLASHLIGHT] = {
		.name = "flashlight",
		.brightness_set = led_flashlight_set,
	},
};

/*
 * flash_probe: Write init value to register, register device
 * @client: Pointer to i2c_client, represent FLASH device
 * @id: Flash id
 * return:
 *   0: Successful
 *   Others: Failed to register Flash devices
 */
static int flash_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int ret = 0; /* Function return value */
	int ret_createfile = 0; /* Function return value */
	int flash_err = 0; /* Register failed flash num */
	int i;  /* Loop counter */
	int j;  /* Loop counter */
	flash_client = client;

	/* Set flash OFF */
	flash_onoff = 0;

	for (i = 0; i < FLASH_NUM; i++) {
		ret = led_classdev_register(&client->dev, &flash_lights[i]);
		if (ret) {
			printk(KERN_ERR "led_classdev_register() failed\n");
			flash_err = i;
			break;
		}
	}
	/* Unregister all registered FLASHs */
	if (ret) {
		for (j = 0; j <= flash_err; j++)
			led_classdev_unregister(&flash_lights[j]);
	}

	/* Create device file for flash time */
	ret_createfile = device_create_file(&flash_client->dev, &dev_attr_time);
	if (ret_createfile)
		printk(KERN_ERR "device_create_file()\n");
	return ret;
}

/* id table */
static struct i2c_device_id flash_idtable[] = {
	{"flash", 0},
};

/* Flash driver */
static struct i2c_driver flash_drv = {
	.driver = {
		.name = "flash driver",
	},
	.probe = flash_probe,
	.id_table = flash_idtable,
};

/*
 * flash_dev_init: Init flash driver, add device to i2c driver
 * return:
 *   0: Success
 *   Others: Error of i2c_add_driver() function
 */
static __init int flash_dev_init(void)
{
	int ret = 0;
	wake_lock_init(&wakelock, WAKE_LOCK_SUSPEND, "flash_wakelock");
	ret = i2c_add_driver(&flash_drv);
	return ret;
}

device_initcall_sync(flash_dev_init);
MODULE_DESCRIPTION("flash device");
MODULE_LICENSE("GPL");

