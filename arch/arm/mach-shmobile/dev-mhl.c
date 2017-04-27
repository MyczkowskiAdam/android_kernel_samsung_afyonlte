#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/sii8332_platform.h>
/*#include "midas.h"*/

#include <rtapi/screen_display.h>

#define GPIO_MHL_SDA_1_8V 15
#define GPIO_MHL_SCL_1_8V 14
#define GPIO_MHL_INT      109
#define GPIO_MHL_EN      102
#define GPIO_MHL_RST      39

#define I2C_BUS_ID_MHL	  15
#define GPIO_VBUS_EN 25

static struct i2c_gpio_platform_data mhl_i2c_gpio_data = {
	.sda_pin    = GPIO_MHL_SDA_1_8V,
	.scl_pin    = GPIO_MHL_SCL_1_8V,
	.udelay  = 1,
};


static struct mhl_platform_data sii9234_pdata = {
	.mhl_rst = GPIO_MHL_RST,
	.mhl_int = GPIO_MHL_INT,
	.mhl_en = GPIO_MHL_EN,
	.irq = 0,
	.hdcp_support = false,
	.status.op_status = 0,
	.status.intr1_mask_value = 0,
	.status.intr2_mask_value = 0,
	.status.intr3_mask_value = 0,
	.status.intr4_mask_value = 0,
	.status.intr5_mask_value = 0,
	.status.intr7_mask_value = 0,
	.status.intr8_mask_value = 0,
	.status.intr_cbus0_mask_value = 0,
	.status.intr_cbus1_mask_value = 0,
	.status.intr_tpi_mask_value = 0,
	.status.mhl_rgnd = false,
	.status.cbus_connected = false,
	.status.linkmode = false,
	.status.connected_ready = false,

	.rx_cap.mhl_ver = 0,
	.rx_cap.dev_type = 0,
	.rx_cap.adopter_id = 0,
	.rx_cap.vid_link_mode = 0,
	.rx_cap.aud_link_mode = 0,
	.rx_cap.video_type = 0,
	.rx_cap.log_dev_map = 0,
	.rx_cap.bandwidth = 0,
	.rx_cap.feature_flag = 0,
	.rx_cap.device_id = 0,
	.rx_cap.scratchpad_size = 0,
	.rx_cap.int_stat_size = 0,
	.rx_cap.rcp_support = 0,
	.rx_cap.rap_support = 0,
	.rx_cap.sp_support = 0,

	.simg72_tx_client = NULL,
	.simg7A_tx_client = NULL,
	.simg92_tx_client = NULL,
	.simg9A_tx_client = NULL,
	.simgC8_tx_client = NULL,
};

static struct i2c_board_info __initdata i2c_devs_sii9234[] = {
	{
		I2C_BOARD_INFO("SIMG72", 0x72>>1),
		.platform_data = &sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("SIMG7A", 0x7A>>1),
		.platform_data = &sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("SIMG92", 0x92>>1),
		.platform_data = &sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("SIMG9A", 0x9A>>1),
		.platform_data = &sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("SIMGC8", 0xC8>>1),
		.platform_data = &sii9234_pdata,
	},
};

struct platform_device mhl_i2c_gpio_device = {
	.name = "i2c-gpio",
	.id = I2C_BUS_ID_MHL,
	.dev = {
		.platform_data = &mhl_i2c_gpio_data,
	},
};

static void sii8332_cfg_gpio(void)
{
	int gpio, rc;

	printk(KERN_INFO "sii8332: %s()\n", __func__);

	gpio = GPIO_MHL_INT;
	rc = gpio_request(gpio, "MHL_INT");
	if (rc < 0) {
		printk(KERN_ERR "unable to request GPIO pin %d\n", gpio);
		return;
	}
	gpio_direction_input(gpio);

	gpio = GPIO_MHL_EN;
	rc = gpio_request(gpio, "HDMI_EN");
	if (rc < 0) {
		printk(KERN_ERR "unable to request GPIO pin %d\n", gpio);
		return;
	}
	gpio_direction_output(gpio, 0);

	gpio = GPIO_MHL_RST;
	rc = gpio_request(gpio, "MHL_RST");
	if (rc < 0) {
		printk(KERN_ERR "unable to request GPIO pin %d\n", gpio);
		return;
	}
	gpio_direction_output(gpio, 1);

}

static void mhl_usb_switch_control(bool on)
{
	pr_info("sii8332: %s() [MHL] USB path change : %s\n", __func__, on ? "MHL" : "USB");
}


void __init board_mhl_init(void)
{
	int ret;

	printk("sii8332: %s : START", __func__);

	ret = i2c_register_board_info(I2C_BUS_ID_MHL, i2c_devs_sii9234,
			ARRAY_SIZE(i2c_devs_sii9234));

	if (ret < 0) {
		printk(KERN_ERR "[MHL] adding i2c fail - nodevice\n");
		return;
	}

	sii8332_cfg_gpio();

	return;
}
