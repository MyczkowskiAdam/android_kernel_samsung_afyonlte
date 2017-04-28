#if defined(CONFIG_SEC_CHARGING_FEATURE)

#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <linux/spa_power.h>
#include <linux/module.h>

#include <mach/common.h>

/* Samsung charging feature
 +++ for board files, it may contain changeable values */
struct spa_temp_tb batt_temp_tb_d2153[]=
{
	{2300, -300},            /* -30 */
	{2144, -200},	 	 	 /* -20 */
	{1341, -100},            /* -10 */
	{1072,  -50},			 /* -5 */
	{865,     0},            /* 0   */
	{577,   100},            /* 10  */
	{400,   200},            /* 20  */
	{334,   250},            /* 25  */
	{285,   300},            /* 30  */
	{199,   400},            /* 40  */
	{143,   500},            /* 50  */
	{106,   600},            /* 60  */
	{ 93,   650},            /* 65  */
	{ 83,   700},            /* 70  */
	{ 75 ,  800},            /* 80  */
};

struct spa_temp_tb batt_temp_tb[] = {
	{869, -300},			/* -30 */
	{769, -200},			/* -20 */
	{643, -100},                    /* -10 */
	{568, -50},			/* -5  */
	{509,   0},                     /* 0   */
	{382,  100},                    /* 10  */
	{275,  200},                    /* 20  */
	{231,  250},                    /* 25  */
	{196,  300},                    /* 30  */
	{138,  400},                    /* 40  */
	{95 ,  500},                    /* 50  */
	{68 ,  600},                    /* 60  */
	{54 ,  650},                    /* 65  */
	{46 ,  700},			/* 70  */
	{34 ,  800},			/* 80  */
};

struct spa_power_data spa_power_pdata_d2153 = {
	.charger_name = "smb328a-charger",
	.eoc_current = 100,
	.recharge_voltage = 4150,
	.charging_cur_usb = 500,
	.charging_cur_wall = 800,
	.suspend_temp_hot = 600,
	.recovery_temp_hot = 400,
	.suspend_temp_cold = -50,
	.recovery_temp_cold = 0,
	.charge_timer_limit = CHARGE_TIMER_6HOUR,
	.batt_temp_tb = &batt_temp_tb_d2153[0],
	.batt_temp_tb_len = ARRAY_SIZE(batt_temp_tb_d2153),
};
/*EXPORT_SYMBOL(spa_power_pdata_d2153);*/
struct spa_power_data spa_power_pdata = {
	.charger_name = "smb328a-charger",
	.eoc_current = 100,
	.recharge_voltage = 4150,
	.charging_cur_usb = 500,
	.charging_cur_wall = 800,
	.suspend_temp_hot = 600,
	.recovery_temp_hot = 400,
	.suspend_temp_cold = -50,
	.recovery_temp_cold = 0,
	.charge_timer_limit = CHARGE_TIMER_6HOUR,
	.batt_temp_tb = &batt_temp_tb[0],
	.batt_temp_tb_len = ARRAY_SIZE(batt_temp_tb),
};
/*EXPORT_SYMBOL(spa_power_pdata);*/

static struct platform_device spa_power_device_d2153 = {
	.name = "spa_power",
	.id = -1,
	.dev.platform_data = &spa_power_pdata_d2153,
};

int spa_power_init(void)
{
	return platform_device_register(&spa_power_device_d2153);
}
#endif
