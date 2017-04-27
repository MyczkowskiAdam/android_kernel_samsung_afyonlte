/*
 * arch/arm/mach-shmobile/setup-u2spa.c
 *
 * Device initialization for spa_power and spa_agent.c
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <mach/setup-u2spa.h>
#include <linux/wakelock.h>
#include <mach/r8a7373.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#if defined(CONFIG_SEC_CHARGING_FEATURE)
#include <linux/spa_power.h>
#include <linux/spa_agent.h>

/* Samsung charging feature
 +++ for board files, it may contain changeable values */
static struct spa_temp_tb batt_temp_tb[] = {
	{3000, -250},		/* -25 */
	{2350, -200},		/* -20 */
	{1850, -150},		/* -15 */
	{1480, -100},		/* -10 */
	{1180, -50},		/* -5  */
	{945,  0},			/* 0    */
	{765,  50},			/* 5    */
	{620,  100},		/* 10  */
	{510,  150},		/* 15  */
	{420,  200},		/* 20  */
	{345,  250},		/* 25  */
	{285,  300},		/* 30  */
	{240,  350},		/* 35  */
	{200,  400},		/* 40  */
	{170,  450},		/* 45  */
	{143,  500},		/* 50  */
	{122,  550},		/* 55  */
	{104,  600},		/* 60  */
	{89,  650},			/* 65  */
	{77,  700},			/* 70  */
};

struct spa_power_data spa_power_pdata = {
	.charger_name = "spa_agent_chrg",
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	.eoc_current = 150,
	.recharging_eoc = 75,
#else
	.eoc_current = 75,
#endif
	.recharge_voltage = 4300,
	.charging_cur_usb = 500,
	.charging_cur_cdp_usb = 1000,
	.charging_cur_wall = 1200,
	.suspend_temp_hot = 600,
	.recovery_temp_hot = 400,
	.suspend_temp_cold = -50,
	.recovery_temp_cold = 0,
	.charge_timer_limit = CHARGE_TIMER_6HOUR,
	.backcharging_time = 30,
	.regulated_vol = 4350,
	.batt_temp_tb = &batt_temp_tb[0],
	.batt_temp_tb_len = ARRAY_SIZE(batt_temp_tb),
};


static struct platform_device spa_power_device = {
	.name = "spa_power",
	.id = -1,
	.dev.platform_data = &spa_power_pdata,
};

static struct platform_device spa_agent_device = {
	.name = "spa_agent",
	.id = -1,
};

static int spa_power_init(void)
{
	int ret;
	ret = platform_device_register(&spa_agent_device);
	if (ret < 0)
		return ret;
	ret = platform_device_register(&spa_power_device);
	if (ret < 0)
		return ret;

	return 0;
}
#endif

void spa_init(void)
{

#if defined(CONFIG_USE_MUIC)
	gpio_request(GPIO_PORT97, NULL);
	gpio_direction_input(GPIO_PORT97);
	gpio_pull_up_port(GPIO_PORT97);
#endif

#if defined(CONFIG_CHARGER_SMB328A)
	gpio_request(GPIO_PORT19, NULL);
	gpio_direction_input(GPIO_PORT19);
	gpio_pull_up_port(GPIO_PORT19);
#endif

#if defined(CONFIG_BATTERY_BQ27425)
	gpio_request(GPIO_PORT105, NULL);
	gpio_direction_input(GPIO_PORT105);
	gpio_pull_up_port(GPIO_PORT105);
#endif

#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_power_init();
#endif
}
