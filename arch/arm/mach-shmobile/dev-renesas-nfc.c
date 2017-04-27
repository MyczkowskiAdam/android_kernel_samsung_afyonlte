#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#ifdef CONFIG_PN544_NFC
#include <linux/i2c-gpio.h>
#include <linux/nfc/pn544.h>
#include <mach/board-u2evm-renesas-nfc.h>
#endif


static struct i2c_gpio_platform_data pn544_i2c_gpio_data = {
	.sda_pin = NFC_I2C_SDA_GPIO,
	.scl_pin =  NFC_I2C_SCL_GPIO,
	.udelay = 1,
};

struct platform_device pn544_i2c_gpio_device = {
	.name = "i2c-gpio",
	.id = NFC_I2C_BUS_ID,
	.dev = {
	.platform_data  = &pn544_i2c_gpio_data,
	},
};

static struct pn544_i2c_platform_data pn544_pdata = {
	.irq_gpio = NFC_IRQ_GPIO,
	.ven_gpio = NFC_EN_GPIO,
	.firm_gpio = NFC_FIRM_GPIO,
};

static struct i2c_board_info pn544_info[] __initdata = {
	{
		I2C_BOARD_INFO("pn544", 0x2b),
		.irq = irqpin2irq(NFC_IRQ_GPIO),
		.platform_data = &pn544_pdata,
	},
};

void pn544_i2c_register_board_info(void)
{
	i2c_register_board_info(8, pn544_info, ARRAY_SIZE(pn544_info));
}

void nfc_gpio_init(void)
{
	/*  NFC Enable */
	/*
	gpio_request(GPIO_PORT12, NULL);
	gpio_direction_output(GPIO_PORT12, 0);
	*/

	/* NFC Firmware */
	gpio_request(GPIO_PORT101, NULL);
	gpio_direction_output(GPIO_PORT101, 0);
}

