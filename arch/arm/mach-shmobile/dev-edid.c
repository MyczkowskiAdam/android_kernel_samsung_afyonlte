#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <mach/r8a7373.h>
#include <rtapi/screen_display.h>

#include <linux/edid_platform.h>

#ifdef CONFIG_SAMSUNG_MHL

#define MHL_INT 48
#define I2C_BUS_ID_MHL	  15


static void edid_set_on(void)
{
	printk(KERN_INFO "%s()\n", __func__);
}


static struct edid_platform_data edid_pdata = {
};

static struct i2c_board_info __initdata i2c_devs_edid[] = {
	{
		I2C_BOARD_INFO("edidA0", 0xA0>>1),
		.platform_data = &edid_pdata,
	},
	{
		I2C_BOARD_INFO("edid60", 0x60>>1),
		.platform_data = &edid_pdata,
	},
};

void __init board_edid_init(void)
{
	int ret;
	printk("%s : START", __func__);

	ret = i2c_register_board_info(I2C_BUS_ID_MHL, i2c_devs_edid,
			ARRAY_SIZE(i2c_devs_edid));

	if (ret < 0) {
		printk(KERN_ERR "[edid] adding i2c fail - nodevice\n");
		return;
	}

	edid_set_on();

	return;
}

#endif
