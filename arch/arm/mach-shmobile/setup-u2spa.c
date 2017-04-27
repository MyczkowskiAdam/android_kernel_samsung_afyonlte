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

#if defined(CONFIG_CHARGER_SMB358)
#include <linux/smb358_charger.h>

static struct smb358_platform_data smb358_info = {
	.irq = GPIO_SMB358_INT,
};

struct platform_device smb358_charger =  {
	.name		= "smb358",
	.id		= CHARGER_I2C_BUS_ID,
	.dev		= {
		.platform_data = &smb358_info,
	},
};
#endif

#if defined(CONFIG_STC3115_FUELGAUGE)
#include <linux/power_supply.h>
#include <linux/stc3115_battery.h>

#ifdef CONFIG_CHARGER_RT9532
extern int rt9532_get_charger_online(void);
#endif

static int Temperature_fn(void)
{
	return (25);
}

struct stc311x_platform_data stc3115_data = {
	.battery_online = NULL,
#ifdef CONFIG_CHARGER_RT9532
	.charger_online = rt9532_get_charger_online, 	// used in stc311x_get_status()
#else
	.charger_online = NULL, 	// used in stc311x_get_status()
#endif
	.charger_enable = NULL,		// used in stc311x_get_status()
	.power_supply_register = NULL,
	.power_supply_unregister = NULL,

	.Vmode = 0,		/*REG_MODE, BIT_VMODE 1=Voltage mode, 0=mixed mode */
	.Alm_SOC = 15,		/* SOC alm level % */
	.Alm_Vbat = 3400,	/* Vbat alm level mV */
	.CC_cnf = 241,      /* nominal CC_cnf, coming from battery characterisation*/
  	.VM_cnf = 295,      /* nominal VM cnf , coming from battery characterisation*/
	.Cnom = 1200,       /* nominal capacity in mAh, coming from battery characterisation*/
	.Rsense = 10,		/* sense resistor mOhms */
	.RelaxCurrent = 100, /* current for relaxation in mA (< C/20) */
	.Adaptive = 1,		/* 1=Adaptive mode enabled, 0=Adaptive mode disabled */

	.CapDerating[6] = 277,   /* capacity derating in 0.1%, for temp = -20C */
	.CapDerating[5] = 82,   /* capacity derating in 0.1%, for temp = -10C */
	.CapDerating[4] = 23,   /* capacity derating in 0.1%, for temp = 0C */
	.CapDerating[3] = 19,   /* capacity derating in 0.1%, for temp = 10C */
	.CapDerating[2] = 0,   /* capacity derating in 0.1%, for temp = 25C */
	.CapDerating[1] = 0,   /* capacity derating in 0.1%, for temp = 40C */
	.CapDerating[0] = 0,   /* capacity derating in 0.1%, for temp = 60C */

	.OCVOffset[15] = -22,    /* OCV curve adjustment */
	.OCVOffset[14] = -9,   /* OCV curve adjustment */
	.OCVOffset[13] = -15,    /* OCV curve adjustment */
	.OCVOffset[12] = -2,    /* OCV curve adjustment */
	.OCVOffset[11] = 0,	/* OCV curve adjustment */
	.OCVOffset[10] = -2,    /* OCV curve adjustment */
	.OCVOffset[9] = -26,     /* OCV curve adjustment */
	.OCVOffset[8] = -6,      /* OCV curve adjustment */
	.OCVOffset[7] = -7,      /* OCV curve adjustment */
	.OCVOffset[6] = -14,    /* OCV curve adjustment */
	.OCVOffset[5] = -23,    /* OCV curve adjustment */
	.OCVOffset[4] = -46,     /* OCV curve adjustment */
	.OCVOffset[3] = -27,    /* OCV curve adjustment */
	.OCVOffset[2] = -34,     /* OCV curve adjustment */
	.OCVOffset[1] = -125,    /* OCV curve adjustment */
	.OCVOffset[0] = -68,     /* OCV curve adjustment */

	.OCVOffset2[15] = -58,    /* OCV curve adjustment */
	.OCVOffset2[14] = -37,   /* OCV curve adjustment */
	.OCVOffset2[13] = -21,    /* OCV curve adjustment */
	.OCVOffset2[12] = -14,    /* OCV curve adjustment */
	.OCVOffset2[11] = -6,    /* OCV curve adjustment */
	.OCVOffset2[10] = -16,    /* OCV curve adjustment */
	.OCVOffset2[9] = -6,     /* OCV curve adjustment */
	.OCVOffset2[8] = 4,      /* OCV curve adjustment */
	.OCVOffset2[7] = 9,      /* OCV curve adjustment */
	.OCVOffset2[6] = 11,    /* OCV curve adjustment */
	.OCVOffset2[5] = 24,    /* OCV curve adjustment */
	.OCVOffset2[4] = 7,     /* OCV curve adjustment */
	.OCVOffset2[3] = 28,    /* OCV curve adjustment */
	.OCVOffset2[2] = 89,     /* OCV curve adjustment */
	.OCVOffset2[1] = 94,    /* OCV curve adjustment */
	.OCVOffset2[0] = 0,     /* OCV curve adjustment */

	/*if the application temperature data is preferred than the STC3115 temperature */
	.ExternalTemperature = Temperature_fn,	/*External temperature fonction, return C */
	.ForceExternalTemperature = 0,	/* 1=External temperature, 0=STC3115 temperature */

};
#endif

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
#else
struct spa_power_data spa_power_pdata = {
	.charger_name = "spa_agent_chrg",
	.eoc_current = 180,
	.recharge_voltage = 4280,
	.charging_cur_usb = 500,
	.charging_cur_wall = 1200,
	.suspend_temp_hot = 600,
	.recovery_temp_hot = 400,
	.suspend_temp_cold = -50,
	.recovery_temp_cold = 0,
	.charge_timer_limit = CHARGE_TIMER_6HOUR,
	.regulated_vol = 4350,
	.batt_temp_tb = &batt_temp_tb[0],
	.batt_temp_tb_len = ARRAY_SIZE(batt_temp_tb),
};
#endif


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
	int ret = 0;
	ret = platform_device_register(&spa_agent_device);
	if (ret < 0)
		return ret;

	ret = platform_device_register(&spa_power_device);
	if (ret < 0)
		return ret;

	return 0;
}

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
