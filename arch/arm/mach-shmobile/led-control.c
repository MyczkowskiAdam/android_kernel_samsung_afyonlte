/*
 * arch/arm/mach-shmobile/led-control.c
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

#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <mach/r8a7373.h>

#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>

#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <linux/tpu_pwm.h>

/* Functions prototype */
static void set_backlight_brightness(struct led_classdev *led_cdev,
					enum led_brightness value);
static void set_keylight_brightness(struct led_classdev *led_cdev,
					enum led_brightness value);
static void set_red_brightness(struct led_classdev *led_cdev,
					enum led_brightness value);
static void set_green_brightness(struct led_classdev *led_cdev,
					enum led_brightness value);
static void set_blue_brightness(struct led_classdev *led_cdev,
					enum led_brightness value);
static int control_led_backlight(enum led_brightness value);
static void set_color_brightness(int value, unsigned char brightness_reg);
static ssize_t led_colorlight_onoff_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count);
static void led_colorlight_on_off(unsigned char leds_set_bit,
					unsigned char value);
static ssize_t led_hard_blink_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static void vout_control(int onoff);
static s32 wrap_led_i2c_write_data(u8 command, u8 value);
static s32 wrap_led_i2c_read_data(u8 command);
static int led_probe(struct i2c_client *client, const struct i2c_device_id *id);
static __init int led_dev_init(void);

/* Macro */
/* Registers to control 3-color LED */
#define LEDS_RED_BRIGHTNESS_REG 0x06
#define LEDS_GREEN_BRIGHTNESS_REG 0x07
#define LEDS_BLUE_BRIGHTNESS_REG 0x08

#define LEDS_GRADATION_CTRL_REG 0x0D
#define LEDS_RED_AUTO_OFF_REG 0x0E
#define LEDS_RED_AUTO_ON_REG 0x0F
#define LEDS_GREEN_AUTO_OFF_REG 0x10
#define LEDS_GREEN_AUTO_ON_REG 0x11
#define LEDS_BLUE_AUTO_OFF_REG 0x12
#define LEDS_BLUE_AUTO_ON_REG 0x13

#define LEDS_REDLIGHT_ON 0x01
#define LEDS_GREENLIGHT_ON 0x02
#define LEDS_BLUELIGHT_ON 0x04

#define CHARGE_OFF 0x00
#define CHARGE_ON 0x01

#define TPU_CHANNEL "TPU0TO3"
#define GPIO_PORT GPIO_PORT39

#define CLK_SOURCE_CP 0 /* CP clock Common Peripheral*/
#define CLK_SOURCE_CP_DIV4 1 /* CP/4 */
#define CLK_SOURCE_CP_DIV16 2 /* CP/16 */
#define CLK_SOURCE_CP_DIV64 3 /* CP/64 */

/* Enum for led type */
enum led_type {
	LED_BACKLIGHT, /* 0: Lcd-backlight */
#if 0
	LED_REDLIGHT, /* 1: 3-color red light */
	LED_GREENLIGHT, /* 2: 3-color green light */
	LED_BLUELIGHT, /* 3: 3-color blue light */
	LED_KEYLIGHT, /* 4: Key backlight */
#endif
	LEDS_NUM, /* 5: led_type max num */
};

/* Global variables */
static struct i2c_client *led_client;
static struct wake_lock wakelock;

static const int periodical_cycle = 43000; /* PWM periodic cycle */
static const int backlight_brightness = 192; /* Initial value
						for LCD backlight */
static unsigned char i2c_reg[40]; /* Array storing current value of register */

struct i2c_def_reg {
	u8 add;
	u8 val;
};

static const struct i2c_def_reg i2c_val[] = {
	{0x09, 0x00}, /* Turn OFF CLED 2 time current mode */
	{0x14, 0x07}, /* Turn OFF sensor */
	{0x27, 0x38}, /* Set MLED current */
	{0xFF, 0xFF}
};

DEFINE_MUTEX(led_mutex);  /* Initialize mutex */

/* Device attribute */
static DEVICE_ATTR(onoff, 0644, NULL, led_colorlight_onoff_store);
static DEVICE_ATTR(blink, 0644, NULL, led_hard_blink_store);

/*
 * set_backlight_brightness: Control LCD backlight
 * @led_cdev: Pointer to LCD backlight device
 * @value: Brightness value
 * return: None
 */
static void set_backlight_brightness(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	control_led_backlight(value);
}

/*
 * set_keylight_brightness: Control key backlight
 * @led_cdev: Pointer to key backlight device
 * @value: Brightness value
 * return: None
 */
static void set_keylight_brightness(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	int ret;
	int hw_val;
	ret = 0;
	hw_val = value >> 3; /* Calculate value set for hardware */

	mutex_lock(&led_mutex);
	/* Set brightness to register */
	ret = wrap_led_i2c_write_data(0x03, hw_val);
	if (ret) {
		printk(KERN_ERR "%s[%d] i2c write err\n", __func__, __LINE__);
	} else {
		if (LED_OFF == value) {
			/* Set status of LED is OFF */
			ret = wrap_led_i2c_write_data(0x01, 0x00);
			if (ret) {
				printk(KERN_ERR "%s[%d] i2c write err\n",
					__func__, __LINE__);
			}
			/* Turn OFF ChargePump and standby */
			vout_control(CHARGE_OFF);
		} else {
			/* Turn ON ChargePump and active */
			vout_control(CHARGE_ON);
			/* Set status of LED is ON */
			ret = wrap_led_i2c_write_data(0x01, 0x1E);
			if (ret) {
				printk(KERN_ERR "%s[%d] i2c write err\n",
					__func__, __LINE__);
			}
		}
	}
	mutex_unlock(&led_mutex);
}

/*
 * set_red_brightness: Set brightness for red led
 * @led_cdev: Pointer to red light device
 * @value: Brightness value
 * return: None
 */
static void set_red_brightness(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	set_color_brightness(value, LEDS_RED_BRIGHTNESS_REG);
}

/*
 * set_green_brightness: Set brightness for green led
 * @led_cdev: Pointer to green light device
 * @value: Brightness value
 * return: None
 */
static void set_green_brightness(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	set_color_brightness(value, LEDS_GREEN_BRIGHTNESS_REG);
}

/*
 * set_blue_brightness: Set brightness for blue led
 * @led_cdev: Pointer to blue light device
 * @value: Brightness value
 * return: None
 */
static void set_blue_brightness(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	set_color_brightness(value, LEDS_BLUE_BRIGHTNESS_REG);
}

/*
 * control_led_backlight: Control LCD backlight using PWM signal
 * @value: Brightness value
 * return:
 *   + 0: Success
 *   + Others: Errors of TPU functions
 */
static int control_led_backlight(enum led_brightness value)
{
	int ret = 0;
	int duty_cycle;
	static void *handle;

	/* Calculate duty cycle basing on brightness value */
	duty_cycle = (periodical_cycle + 1) -
			(value * (periodical_cycle + 1) / 255);

	/* Enable PWM signal */
	if (value) {
		/* Open TPU channel to use, clock source is CP/4 */
		if (!handle) {
			gpio_free(GPIO_PORT);
			ret = tpu_pwm_open(TPU_CHANNEL, CLK_SOURCE_CP, &handle);
			if (ret) {
				printk(KERN_ERR "tpu_pwm_open() %d\n", ret);
				return ret;
			}
		}
	/* Enable PWM signal with specific duty cycle and periodic cycle */
		ret = tpu_pwm_enable(handle, TPU_PWM_START,
					duty_cycle, periodical_cycle);
		if (ret) {
			printk(KERN_ERR "Enable PWM signal %d\n", ret);
			return ret;
		}
		return ret;
	} else {
		if (handle) {
			/* Disable PWM signal */
			ret = tpu_pwm_enable(handle, TPU_PWM_STOP,
						duty_cycle, periodical_cycle);
			if (ret) {
				printk(KERN_ERR "Disable PWM signal %d\n", ret);
				return ret;
			}
			/* Close TPU channel */
			ret = tpu_pwm_close(handle);
			if (ret) {
				printk(KERN_ERR "tpu_pwm_close() %d\n", ret);
				return ret;
			}
			handle = NULL;
		}
		gpio_request(GPIO_PORT, NULL);
		gpio_direction_output(GPIO_PORT, 0);
		return ret;
	}
}

/*
 * set_color_brightness: Set brightness for 3-color LED
 * @value: Brightness value
 * @brightness_reg: Brightness register of 3-color LED
 * return: None
 */
static void set_color_brightness(int value, unsigned char brightness_reg)
{
	int hw_val = value >> 3; /* Hardware suports
				only 5 bits for brightness */
	int ret = 0;

	mutex_lock(&led_mutex);
	/* Set LED brightness */
	ret = wrap_led_i2c_write_data(brightness_reg, (hw_val & 0xff));
	/*if (ret)
		printk(KERN_ERR "Set brightness failed %d\n", ret); */
	mutex_unlock(&led_mutex);
}

/*
 * led_colorlight_onoff_store: Turn ON/OFF for 3-color LEDs
 * @dev: Device
 * @attr: Device attribute
 * @buf: Buffer containing data data
 * @count: Size of buffer
 * return:
 *   + Size of buffer: Success
 *   + -EINVAL: Error of reading buffer
 */
static ssize_t led_colorlight_onoff_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	int value = 0;
	int scan_result = 0;
	int err = count;

	unsigned char red = 0;
	unsigned char green = 0;
	unsigned char blue = 0;
	unsigned char set_val = 0;
	unsigned char set_bit = 0;

	mutex_lock(&led_mutex);
	scan_result = sscanf(buf, "%d", &value);
	if (1 != scan_result) {
		printk(KERN_ERR "failed to read value\n");
		mutex_unlock(&led_mutex);
		return -EINVAL;
	}

	if (value < 0) {
		printk(KERN_ERR "invalid input\n");
		mutex_unlock(&led_mutex);
		return -EINVAL;
	}

	red = (value & 0x04) ? LEDS_REDLIGHT_ON : 0;
	green = (value & 0x02) ? LEDS_GREENLIGHT_ON : 0;
	blue = (value & 0x01) ? LEDS_BLUELIGHT_ON : 0;

	set_val = (red | green | blue);
	set_bit = (LEDS_REDLIGHT_ON | LEDS_GREENLIGHT_ON | LEDS_BLUELIGHT_ON);

	if (!set_val) {
		/* LED OFF */
		led_colorlight_on_off(set_bit, set_val);
		vout_control(CHARGE_OFF);
	} else {
		/* LED ON */
		vout_control(CHARGE_ON);
		led_colorlight_on_off(set_bit, set_val);
	}

	mutex_unlock(&led_mutex);
	return err;
}

/*
 * led_colorlight_on_off: Set ON/OFF bits of 3-color LEDs
 * @leds_set_bit: Bit position sample for setting
 * @value: Bit value sample
 * return: None
 */
static void led_colorlight_on_off(unsigned char leds_set_bit,
					unsigned char value)
{
	unsigned char get_reg_val = 0;
	unsigned char set_reg_val = 0;
	int ret = 0;

	/* Get the value of register */
	get_reg_val = i2c_reg[0x02];
	set_reg_val = (get_reg_val & ~leds_set_bit) | (value & leds_set_bit);

	ret = wrap_led_i2c_write_data(0x02, set_reg_val);
	/*if (ret)
		printk(KERN_ERR "Set register failed\n");*/
}

/*
 * led_hard_blink_store: Set hardware blink for 3-color LEDs
 * @dev: Device
 * @attr: Device attribute
 * @buf: Buffer containing data data
 * @count: Size of buffer
 * return:
 *   + Size of buffer: Success
 *   + -EINVAL: Error of reading buffer
 */
static ssize_t led_hard_blink_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int value = 0;
	int scan_result = 0;
	int i2c_err = 0;
	int err = count;

	unsigned int set_grfreq_val = 0; /* Gradation and Freq value */
	unsigned int set_raoff_val = 0; /* Red auto OFF value */
	unsigned int set_gaoff_val = 0; /* Green auto OFF value */
	unsigned int set_baoff_val = 0; /* Blue auto OFF value */

	mutex_lock(&led_mutex);
	scan_result = sscanf(buf, "%ul", &value);
	if (1 != scan_result) {
		printk(KERN_ERR "Failed to read value\n");
		mutex_unlock(&led_mutex);
		return -EINVAL;
	}

	if (value < 0) {
		printk(KERN_ERR "Invalid input\n");
		mutex_unlock(&led_mutex);
		return -EINVAL;
	}

	set_grfreq_val = (value & 0xFF000000) >> 24;
	set_raoff_val = (value & 0x00FF0000) >> 16;
	set_gaoff_val = (value & 0x0000FF00) >> 8;
	set_baoff_val = (value & 0x000000FF);

	i2c_err = wrap_led_i2c_write_data(LEDS_RED_AUTO_ON_REG, 0);
	/*if (i2c_err)
		printk(KERN_ERR "%s[%d]: I2C error\n", __func__, __LINE__);*/

	i2c_err = wrap_led_i2c_write_data(LEDS_GREEN_AUTO_ON_REG, 0);
	/*if (i2c_err)
		printk(KERN_ERR "%s[%d]: I2C error\n", __func__, __LINE__);*/

	i2c_err = wrap_led_i2c_write_data(LEDS_BLUE_AUTO_ON_REG, 0);
	/*if (i2c_err)
		printk(KERN_ERR "%s[%d]: I2C error\n", __func__, __LINE__);*/

	i2c_err = wrap_led_i2c_write_data(LEDS_RED_AUTO_OFF_REG, set_raoff_val);
	/*if (i2c_err)
		printk(KERN_ERR "%s[%d]: I2C error\n", __func__, __LINE__);*/

	i2c_err = wrap_led_i2c_write_data(LEDS_GREEN_AUTO_OFF_REG,
						set_gaoff_val);
	/*if (i2c_err)
		printk(KERN_ERR "%s[%d]: I2C error\n", __func__, __LINE__);*/

	i2c_err = wrap_led_i2c_write_data(LEDS_BLUE_AUTO_OFF_REG,
						set_baoff_val);
	/*if (i2c_err)
		printk(KERN_ERR "%s[%d]: I2C error\n", __func__, __LINE__);*/

	if (!set_grfreq_val) {
		i2c_err = wrap_led_i2c_write_data(LEDS_GRADATION_CTRL_REG, 0);
		vout_control(CHARGE_OFF);
	} else {
		vout_control(CHARGE_ON);
		i2c_err = wrap_led_i2c_write_data(LEDS_GRADATION_CTRL_REG,
							set_grfreq_val);
	}
	/*if (i2c_err)
		printk(KERN_ERR "%s[%d]: I2C error\n", __func__, __LINE__);*/

	mutex_unlock(&led_mutex);
	return err;
}

/*
 * vout_control: Turn ON/OFF charge pump
 * @value: Brightness value (If value = 0, turn OFF otherwise, turn ON)
 * return: None
 */
static void vout_control(int onoff)
{
	int ret = 0;
	unsigned char set_val = 0;
	int wait = 0;

	if (onoff == CHARGE_ON) {
		/* Turn ON charge pump */
		set_val = 0x81;
	} else {
		/* Turn OFF charge pump */
		set_val = 0x00;
	}

	if (!set_val) {
		if (!(i2c_reg[0x01] & 0x1E) &&
			!(i2c_reg[0x02] & 0x07) &&
			!(i2c_reg[0x0d] & 0x18)) {
			ret = wrap_led_i2c_write_data(0x00, set_val);
		}
	} else {
		if (!(i2c_reg[0x00] & 0x80))
			wait = 1;
		ret = wrap_led_i2c_write_data(0x00, set_val);
	}

	if (wait == 1)
		/* Wait until charge pump is stable */
		msleep(20);

}

/*
 * wrap_led_i2c_write_data: Write value to LED hardware register
 * @command: Address of LED hardware register
 * @value: Value to set to register
 * return:
 *   0: Success
 *   Others: Errors of I2C write function
 */
static s32 wrap_led_i2c_write_data(u8 command, u8 value)
{
	int err = 0;
	int retry = 0;
	i2c_reg[command] = value;

	for (retry = 0; retry < 5; retry++) {
		wake_lock(&wakelock);
		err = i2c_smbus_write_byte_data(led_client, command, value);
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
 * wrap_led_i2c_read_data: Read value from LED hardware register
 * @command: Address of LED hardware register
 * return:
 *   >= 0: Success
 *   <0: Error of I2C read function
 */
static s32 wrap_led_i2c_read_data(u8 command)
{
	int ret_val = 0;
	int retry = 0;

	for (retry = 0; retry < 5; retry++) {
		wake_lock(&wakelock);
		ret_val = i2c_smbus_read_byte_data(led_client, command);
		i2c_reg[command] = ret_val;
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

/* List of LED devices */
static struct led_classdev led_lights[] = {
	[LED_BACKLIGHT] = {
		.name = "lcd-backlight",
		.brightness = LED_FULL,
		.brightness_set = set_backlight_brightness,
	},
#if 0
	[LED_REDLIGHT] = {
		.name = "red",
		.brightness_set = set_red_brightness,
	},
	[LED_GREENLIGHT] = {
		.name = "green",
		.brightness_set = set_green_brightness,
	},
	[LED_BLUELIGHT] = {
		.name = "blue",
		.brightness_set = set_blue_brightness,
	},
	[LED_KEYLIGHT] = {
		.name = "button-backlight",
		.max_brightness = 0xFFFFFF,
		.brightness_set = set_keylight_brightness,
	},
#endif
};

/*
 * led_probe: Register LED devices
 * @client: Pointer to i2c_client, represent LED device
 * @id: LED device id
 * return:
 *   + 0: Success
 *   + Others: Error to register LED device
 */
static int led_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0; /* Function return value */
	int ret_createfile = 0; /* Function return value */
	int led_err = 0; /* Register failed led num */
	/* Loop counters */
	int i;
	int j;

	led_client = client;
	memset(i2c_reg, 0, sizeof(i2c_reg));

	/* Save initial value for some functions */
	i2c_reg[0] = 0x81; /* Turn charge pump ON, set actively */
	i2c_reg[1] = 0x1E; /* Turn ON MLED4, 5, 6 & set nomal mode,
				turn OFF mini light mode */
	i2c_reg[2] = 0x0F; /* Turn ON BLED, GLED, RLED */
	i2c_reg[3] = 0x10; /* Set current to KEY LED at 10.2mA */
	i2c_reg[6] = 0x00; /* Set current of RLED 0.6mA */
	i2c_reg[7] = 0x02; /* Set current of GLED 1.8mA */
	i2c_reg[8] = 0x03; /* Set current of BLED 2.4mA */

	gpio_request(GPIO_PORT13, NULL);
	gpio_direction_output(GPIO_PORT13, 1);
	udelay(30);
#if 0
	/* Turn OFF camera and sensor */
	for (i = 0; i2c_val[i].add != 0xFF; i++)
		wrap_led_i2c_write_data(i2c_val[i].add, i2c_val[i].val);
#endif

	for (i = 0; i < LEDS_NUM; i++) {
		ret = led_classdev_register(&client->dev, &led_lights[i]);
		if (ret) {
			printk(KERN_ERR "led_classdev_register() failed\n");
			led_err = i;
			break;
		}
	}
	/* Unregister all registered LEDs */
	if (ret) {
		for (j = 0; j <= led_err; j++)
			led_classdev_unregister(&led_lights[j]);
	}

	/* Create device file for LEDs ON/OFF */
	ret_createfile = device_create_file(&led_client->dev, &dev_attr_onoff);
	if (ret_createfile)
		printk(KERN_ERR "device_create_file() - onoff\n");

	/* Create device file for LEDs hardware blink */
	ret_createfile = device_create_file(&led_client->dev, &dev_attr_blink);
	if (ret_createfile)
		printk(KERN_ERR "device_create_file() - blink\n");

	/* Turn ON LCD backlight */
	control_led_backlight(backlight_brightness);
	return ret;
}

/* Structure of LED driver */
static struct i2c_device_id led_idtable[] = {
	{"led", 0},
};

static struct i2c_driver led_drv = {
	.driver = {
		.name = "led driver",
	},
	.probe = led_probe,
	.id_table = led_idtable,
};

/*
 * led_dev_init: Init LED common driver, add driver to I2C core.
 * Parameter: None
 * return:
 *   + 0: Success
 *   + Others: Error of i2c_add_driver() function
 */
static __init int led_dev_init(void)
{
	int ret = 0;
	wake_lock_init(&wakelock, WAKE_LOCK_SUSPEND, "led_wakelock");
	ret = i2c_add_driver(&led_drv);
	return ret;
}

module_init(led_dev_init);
MODULE_DESCRIPTION("led device");
MODULE_LICENSE("GPL");
