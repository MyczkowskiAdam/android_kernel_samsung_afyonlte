/*
 * arch/arm/mach-shmobile/setup-u2touchkey.c
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
#include <linux/kernel.h>
#include <linux/i2c/touchkey_i2c.h>

#include <mach/r8a7373.h>
#include <mach/irqs.h>
static struct i2c_board_info i2c_touchkey[];

void touchkey_init_hw(void)
{

}

static int touchkey_suspend(void)
{
#if 0
	struct regulator *regulator;
	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator))
		return 0;
	if (regulator_is_enabled(regulator))
		regulator_force_disable(regulator);

	regulator_put(regulator);
#endif
	return 1;
}

static int touchkey_resume(void)
{
#if 0
	struct regulator *regulator;
	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator))
		return 0;
	regulator_enable(regulator);
	regulator_put(regulator);
#endif
	return 1;
}

static int touchkey_power_on(bool on)
{
	int ret;
	if (on)
		ret = touchkey_resume();
	else
		ret = touchkey_suspend();

	return ret;
}

static int touchkey_led_power_on(bool on)
{
	return 1;
}
#define TCKEY_SDA 27
#define TCKEY_SCL 26
static struct touchkey_platform_data touchkey_pdata = {
	.gpio_sda = TCKEY_SDA,	/* To do to set gpio */
	.gpio_scl = TCKEY_SCL,	/* To do to set gpio */
	.gpio_int = (int)NULL,	/* To do to set gpio */
	.init_platform_hw = touchkey_init_hw,
	.suspend = touchkey_suspend,
	.resume = touchkey_resume,
	.power_on = touchkey_power_on,
	.led_power_on = touchkey_led_power_on,
};


static struct i2c_board_info i2c_touchkey[] = {
	{
		I2C_BOARD_INFO("sec_touchkey", 0x20),
		.platform_data = &touchkey_pdata,
		.irq = R8A7373_IRQC_IRQ(43),
	},

};
int touchkey_i2c_register_board_info(int busnum) {
	return i2c_register_board_info(busnum, i2c_touchkey, ARRAY_SIZE(i2c_touchkey));
}

