/*
 * drivers/leds/leds-ktd253ehd.c
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
#include <linux/gpio.h>
#include <mach/r8a7373.h>
#include <linux/i2c.h>
#include <linux/module.h>

#include <linux/hrtimer.h>
#include <linux/kthread.h>

#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/earlysuspend.h>
#include <linux/leds-ktd253ehd.h>

/* Functions prototype */
static int get_backlight_brightness(struct backlight_device *bd);
static int set_backlight_brightness(struct backlight_device *bd);
static int control_led_backlight(int value);
static int led_remove(struct platform_device *dev);
static int led_probe(struct platform_device *pdev);
static int led_suspend(struct device *dev);
static int led_resume(struct device *dev);
#ifdef CONFIG_HAS_EARLYSUSPEND
static void led_early_suspend(struct early_suspend *h);
static void led_late_resume(struct early_suspend *h);
#endif

static int __init led_dev_init(void);
static void __exit led_dev_exit(void);

/* Macro */
#define MIN_BRIGHTNESS	(0)
#define MAX_BRIGHTNESS	(255)
#define INIT_BRIGHTNESS	MAX_BRIGHTNESS

/* Global variables */
static struct backlight_device *registed_bd;
static int g_current_data;
static spinlock_t g_light_lock;

/* List of LED devices */
static const struct backlight_ops ktd253ehd_backlight_ops  = {
	.get_brightness = get_backlight_brightness,
	.update_status = set_backlight_brightness,
};

/*
 * get_backlight_brightness: Control LCD backlight
 * @bd: Pointer to LCD backlight device
 * return: brightness value
 */
static int get_backlight_brightness(struct backlight_device *bd)
{
	printk(KERN_DEBUG "brightness:%d\n", bd->props.brightness);

	return bd->props.brightness;
}

/*
 * set_backlight_brightness: Control LCD backlight
 * @bd: Pointer to LCD backlight device
 * return: Return value of control_led_backlight
 */
static int set_backlight_brightness(struct backlight_device *bd)
{
	int value = bd->props.brightness;
	int ret = 0;

	ret = control_led_backlight(value);

	return ret;
}

/*
 * control_led_backlight: Control LCD backlight using PWM signal
 * @value: Brightness value
 * return:
 *   + 0: Success
 */
static int control_led_backlight(int value)
{
	/* convert value area of 0-255 into 0-32.*/
	int new_data = (int)((value + 7) / 8);
	int pulse_num = 0;
	int i = 0;
	unsigned long flag;

	if (new_data == 0) {
		/* extinction */
		__raw_writel(0x8000, GPIO_PORTL063_032DCR);

		mdelay(3);
		g_current_data = 0;
	} else {
		if (g_current_data == 0) {
			/* extinction -> lighting */
			__raw_writel(0x8000, GPIO_PORTL063_032DSR);

			g_current_data = 32;
		}

		/* calculate the number of pulses for intensity control. */
		pulse_num = g_current_data - new_data;

		if (pulse_num < 0)
			pulse_num += 32;

		/* produce a pulse */

		spin_lock_irqsave(&g_light_lock, flag);
		for (i = 0; i < pulse_num; i++) {
			__raw_writel(0x8000, GPIO_PORTL063_032DCR);

			__raw_writel(0x8000, GPIO_PORTL063_032DSR);
		}
		spin_unlock_irqrestore(&g_light_lock, flag);

		g_current_data = new_data;
	}
	return 0;
}

/*
 * led_remove: Unregister LED devices
 * @dev: Pointer to platform_device
 * return:
 *   + 0: Success
 */
static int led_remove(struct platform_device *dev)
{
	backlight_device_unregister(registed_bd);

	return 0;
}

/*
 * led_suspend: Suspend LED devices
 * @dev: Pointer to platform_device
 * @state: pm_message_t
 * return:
 *   + 0: Success
 *   + Others: Error to control_led_backlight()
 */
#ifdef CONFIG_PM
static int led_suspend(struct device *dev)
{
	return 0;
}

/*
 * led_resume: Resume LED devices
 * @dev: Pointer to platform_device
 * return:
 *   + 0: Success
 *   + Others: Error to control_led_backlight()
 */
static int led_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops led_pm_ops = {
	.suspend	= led_suspend,
	.resume		= led_resume,
};
#endif /*CONFIG_PM*/

#ifdef CONFIG_HAS_EARLYSUSPEND
/*
 * led_early_suspend: Early suspend LED devices
 * @dev: Pointer to early_suspend
 * return: None
 */
static void led_early_suspend(struct early_suspend *h)
{
	led_suspend(NULL);
}

/*
 * led_late_resume: Late resume LED devices
 * @dev: Pointer to early_suspend
 * return: None
 */
static void led_late_resume(struct early_suspend *h)
{
	led_resume(NULL);
}
#endif

/* Structure of LED driver */
#ifdef CONFIG_HAS_EARLYSUSPEND
struct early_suspend ktd253ehd_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend = led_early_suspend,
	.resume = led_late_resume,
};
#endif

static struct platform_driver ktd253ehd_led_driver = {
	.driver		= {
		.name	= "ktd253ehd_led",
		.owner	= THIS_MODULE,
#if (!defined(CONFIG_HAS_EARLYSUSPEND) && defined(CONFIG_PM))
		.pm	= &led_pm_ops,
#endif
	},
	.probe		= led_probe,
	.remove		= led_remove,
};

/*
 * led_probe: Register LED devices
 * @dev: Pointer to platform_device
 * return:
 *   + 0: Success
 *   + Others: Error to register LED device
 */
static int led_probe(struct platform_device *pdev)
{
	struct backlight_device *bd;
	struct backlight_properties props;
	struct ktd253ehd_led_platform_data *data = pdev->dev.platform_data;

	spin_lock_init(&g_light_lock);

	g_current_data = 0;

	/* setting gpio */
	gpio_request(data->gpio_port, NULL);
	gpio_direction_output(data->gpio_port, 1);

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = MAX_BRIGHTNESS;
	props.brightness = INIT_BRIGHTNESS;

	/* create and register a backlight_device class */
	bd = backlight_device_register("panel", &pdev->dev,
		NULL, &ktd253ehd_backlight_ops, &props);
	if (IS_ERR(bd)) {
		printk(KERN_ERR "%s: backlight_device_register fail.\n",
			__func__);
		return PTR_ERR(bd);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&ktd253ehd_early_suspend);
#endif
	bd->props.max_brightness = MAX_BRIGHTNESS;
	bd->props.brightness = INIT_BRIGHTNESS;
	platform_set_drvdata(pdev, bd);

	registed_bd = bd;
	/* initialize backlight */
	backlight_update_status(bd);

	return 0;
}

/*
 * led_dev_init: Init LED driver.
 * Parameter: None
 * return:
 *   + 0: Success
 *   + Others: Error of platform_driver_register() function
 */
static int __init led_dev_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&ktd253ehd_led_driver);

	return ret;
}

/*
 * led_dev_exit: Exit LED driver.
 * Parameter: None
 * return: None
 */
static void __exit led_dev_exit(void)
{
	platform_driver_unregister(&ktd253ehd_led_driver);
}

module_init(led_dev_init);
module_exit(led_dev_exit);
MODULE_DESCRIPTION("led device");
MODULE_LICENSE("GPL");
