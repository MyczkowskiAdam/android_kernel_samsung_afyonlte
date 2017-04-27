/*
 * GPS GPIO	control
 *
 *	Copyright (C) 2012 Samsung,	Inc.
 *	Copyright (C) 2012 Google, Inc.
 *
 *	This program is	free software; you can redistribute	it and/or modify
 *	it under the terms of the GNU General Public License as	published by
 *	the	Free Software Foundation; either version 2 of the License, or
 *	(at	your option) any later version.
 *
 *	This program is	distributed	in the hope	that it	will be	useful,
 *	but	WITHOUT	ANY	WARRANTY; without even the implied warranty	of
 *	MERCHANTABILITY	or FITNESS FOR A PARTICULAR	PURPOSE.  See the
 *	GNU	General	Public License for more	details.
 *
 *	You	should have	received a copy	of the GNU General Public License
 *	along with this	program; if	not, write to the Free Software
 *	Foundation,	Inc., 59 Temple	Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/r8a7373.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <linux/bug.h>

#include <linux/sysfs.h>
#include <linux/proc_fs.h>


struct class *gps_class;

void gps_gpio_init(void)
{
	struct device *gps_dev;

	gps_class =	class_create(THIS_MODULE, "gps");
	if (IS_ERR(gps_class)) {
		pr_err("Failed to create class(sec)!\n");
		return;
	}
	BUG_ON(!gps_class);

	gps_dev	= device_create(gps_class, NULL, 0,	NULL, "bcm4752");
	BUG_ON(!gps_dev);

	printk("gps_gpio_init!");

	/* GPS UART settings (ttySC5) */
	gpio_request(GPIO_FN_SCIFB1_RXD, NULL);
	gpio_pull_up_port(GPIO_PORT79);
	gpio_request(GPIO_FN_SCIFB1_TXD, NULL);
	gpio_pull_off_port(GPIO_PORT78);
	gpio_request(GPIO_FN_SCIFB1_CTS, NULL);
	gpio_pull_off_port(GPIO_PORT77);
	gpio_request(GPIO_FN_SCIFB1_RTS, NULL);
	gpio_pull_off_port(GPIO_PORT76);

	/* GPS Settings	*/
	gpio_request(GPIO_PORT11, "GPS_PWR_EN");
	gpio_pull_off_port(GPIO_PORT11);
	gpio_direction_output(GPIO_PORT11, 0);

	gpio_request(GPIO_PORT48, "GPS_ECLK_26M");
	gpio_pull_off_port(GPIO_PORT48);
	gpio_direction_output(GPIO_PORT48, 0);

	gpio_export(GPIO_PORT11, 1);
	gpio_export(GPIO_PORT48, 1);

	gpio_export_link(gps_dev, "GPS_PWR_EN",	GPIO_PORT11);
	gpio_export_link(gps_dev, "GPS_ECLK_26M", GPIO_PORT48);

	printk("gps_gpio_init done!");
}
