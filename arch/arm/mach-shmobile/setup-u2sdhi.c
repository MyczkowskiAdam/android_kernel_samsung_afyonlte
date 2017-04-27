#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mmc/host.h>
#include <linux/mmc/renesas_sdhi.h>
#include <linux/pmic/pmic.h>
#include <linux/regulator/consumer.h>

#include <mach/setup-u2sdhi.h>
#include <mach/common.h>
#include <mach/r8a7373.h>
#include <mach/irqs.h>

#define WLAN_GPIO_EN	GPIO_PORT260
#define WLAN_IRQ	GPIO_PORT98
#define VSD_VDCORE_DELAY 50
#define E3_3_V 3300000
#define E1_8_V 1800000

static void sdhi0_set_pwr(struct platform_device *pdev, int state)
{
	struct regulator *regulator;
	int ret = 0;
	int regulator_voltage = 0;

	switch (state) {
	case RENESAS_SDHI_POWER_ON:
		printk(KERN_INFO"RENESAS_SDHI_POWER_ON:%s\n", __func__);
		regulator = regulator_get(NULL, "vsd");
		if (IS_ERR(regulator))
			return;

		ret = regulator_force_disable(regulator);
		if (ret)
			printk(KERN_INFO "%s:err regulator_force_disable ret = %d\n",
							__func__ , ret);

		ret = regulator_enable(regulator);
		if (ret)
			printk(KERN_INFO "%s:err regulator_enable ret = %d\n",
						__func__ , ret);

		regulator_put(regulator);

		regulator = regulator_get(NULL, "vio_sd");
		if (IS_ERR(regulator))
			return;

		ret = regulator_force_disable(regulator);
		if (ret)
			printk(KERN_INFO "%s:err regulator_force_disable ret = %d\n",
							__func__ , ret);

		ret = regulator_enable(regulator);
		if (ret)
			printk(KERN_INFO "%s:err regulator_enable ret = %d\n",
						__func__ , ret);

		regulator_put(regulator);

		__raw_writel(__raw_readl(MSEL3CR) | (1<<28), MSEL3CR);
		break;

	case RENESAS_SDHI_POWER_OFF:
			printk(KERN_INFO"RENESAS_SDHI_POWER_OFF:%s\n", __func__);
		__raw_writel(__raw_readl(MSEL3CR) & ~(1<<28), MSEL3CR);

		regulator = regulator_get(NULL, "vio_sd");
		if (IS_ERR(regulator))
			return;

		ret = regulator_force_disable(regulator);
		if (ret)
			printk(KERN_INFO "%s:err regulator_force_disable ret = %d\n",
							__func__ , ret);

		regulator_put(regulator);

		regulator = regulator_get(NULL, "vsd");
		if (IS_ERR(regulator))
			return;

		ret = regulator_force_disable(regulator);
		if (ret)
			printk(KERN_INFO "%s:err regulator_force_disable ret = %d\n",
							__func__ , ret);

		regulator_put(regulator);

		/* Delay of 50ms added between VSD off and VCORE
		off as per SSG specification */
		mdelay(VSD_VDCORE_DELAY);
		break;

		case RENESAS_SDHI_SIGNAL_V330:
			printk(KERN_INFO"RENESAS_SDHI_SIGNAL_V330:%s\n", __func__);

		regulator = regulator_get(NULL, "vsd");
		if (IS_ERR(regulator))
			return;

		regulator_voltage = regulator_get_voltage(regulator);
		printk(KERN_INFO"vsd voltage = %d\n", regulator_voltage);
		if (regulator_voltage != E3_3_V) {
			printk(KERN_INFO"vsd change as %duV\n", E3_3_V);

		if (regulator_is_enabled(regulator)) {
			ret = regulator_force_disable(regulator);
			if (ret)
				printk(KERN_INFO "%s:err regulator_force_disable ret = %d\n",
							__func__ , ret);
		}

		ret = regulator_set_voltage(regulator, E3_3_V, E3_3_V);
		if (ret)
			printk(KERN_INFO"%s: err vsd set voltage ret=%d\n",
							__func__, ret);

			ret = regulator_enable(regulator);
			if (ret)
				printk(KERN_INFO"%s: err regulator_enable ret=%d\n",
								__func__, ret);
		}

		regulator_put(regulator);

		regulator = regulator_get(NULL, "vio_sd");
		if (IS_ERR(regulator))
			return;

		regulator_voltage = regulator_get_voltage(regulator);
		printk(KERN_INFO"vio_sd voltage= %d\n", regulator_voltage);
		if (regulator_voltage != E3_3_V) {
			printk(KERN_INFO"vio_sd change as %duV\n", E3_3_V);


		if (regulator_is_enabled(regulator)) {
			ret = regulator_force_disable(regulator);
			if (ret)
				printk(KERN_INFO "%s:err regulator_force_disable ret = %d\n",
							__func__ , ret);
		}

		ret = regulator_set_voltage(regulator, E3_3_V, E3_3_V);
		if (ret)
			printk(KERN_INFO"%s: err vio_sd set voltage ret=%d\n",
							__func__, ret);

			ret = regulator_enable(regulator);
			if (ret)
				printk(KERN_INFO"%s: err regulator_enable ret=%d\n",
								__func__, ret);
		}
		regulator_put(regulator);
		break;
		case RENESAS_SDHI_SIGNAL_V180:
			printk(KERN_INFO"RENESAS_SDHI_SIGNAL_V180:%s\n", __func__);

		regulator = regulator_get(NULL, "vsd");
		if (IS_ERR(regulator))
			return;

		regulator_voltage = regulator_get_voltage(regulator);
		printk(KERN_INFO"vsd voltage = %d\n", regulator_voltage);
		if (regulator_voltage != E1_8_V) {
			printk(KERN_INFO"vsd change as %duV\n", E1_8_V);

		if (regulator_is_enabled(regulator)) {
			ret = regulator_force_disable(regulator);
			if (ret)
				printk(KERN_INFO "%s:err regulator_force_disable ret = %d\n",
							__func__ , ret);
		}

		ret = regulator_set_voltage(regulator, E1_8_V, E1_8_V);
		if (ret)
			printk(KERN_INFO "%s: err vsd set voltage ret=%d\n",
							__func__, ret);

			ret = regulator_enable(regulator);
			if (ret)
				printk(KERN_INFO"%s: err regulator_enable ret=%d\n",
								__func__, ret);
		}

		regulator_put(regulator);

		regulator = regulator_get(NULL, "vio_sd");
		if (IS_ERR(regulator))
			return;

		regulator_voltage = regulator_get_voltage(regulator);
		printk(KERN_INFO"vio_sd voltage = %d\n", regulator_voltage);
		if (regulator_voltage != E1_8_V) {
			printk(KERN_INFO"vio_sd change as %duV\n", E1_8_V);

		if (regulator_is_enabled(regulator)) {
			ret = regulator_force_disable(regulator);
			if (ret)
				printk(KERN_INFO "%s:err regulator_force_disable ret = %d\n",
							__func__ , ret);
		}

		ret = regulator_set_voltage(regulator, E1_8_V, E1_8_V);
		if (ret)
			printk(KERN_INFO"%s: err vio_sd set voltage ret=%d\n",
							__func__, ret);

			ret = regulator_enable(regulator);
			if (ret)
				printk(KERN_INFO"%s: err regulator_enable ret=%d\n",
								__func__, ret);
		}

		regulator_put(regulator);
		break;
		default:
			printk(KERN_INFO"default:%s\n", __func__);
			break;
	}
}

static int sdhi0_get_cd(struct platform_device *pdev)
{
	return gpio_get_value(GPIO_PORT327) ? 0 : 1;
}

#define SDHI0_EXT_ACC_PHYS	0xEE1000E4
#define SDHI0_DMACR_PHYS	0xEE108000

static void sdhi0_set_dma(struct platform_device *pdev, int size)
{
	static void __iomem *dmacr, *ext_acc;
	u32 val, val2;

	if (!dmacr)
		dmacr = ioremap_nocache(SDHI0_DMACR_PHYS, 4);
	if (!ext_acc)
		ext_acc = ioremap_nocache(SDHI0_EXT_ACC_PHYS, 4);

	switch (size) {
	case 32:
		val = 0x30;
		val2 = 1;
		break;
	case 16:
		val = 0x03;
		val2 = 1;
		break;
	default:
		val = 0x00;
		val2 = 0;
		break;
	}
	__raw_writew(val, dmacr);
	__raw_writew(val2, ext_acc);
}

static struct portn_gpio_setting_info sdhi0_gpio_setting_info[] = {
	[0] = {
		.flag = 0,
		.port = GPIO_PORT327,
		.active = {
			.port_fn	= GPIO_FN_SDHICD0,
			.pull		= PORTn_CR_PULL_NOT_SET,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn	= GPIO_FN_SDHICD0,
			.pull		= PORTn_CR_PULL_NOT_SET,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
};

struct renesas_sdhi_platdata sdhi0_info = {
	.caps			= 0,
	.flags			= RENESAS_SDHI_SDCLK_OFFEN |
					RENESAS_SDHI_WP_DISABLE |
					RENESAS_SDHI_DMA_SLAVE_CONFIG,
	.slave_id_tx		= SHDMA_SLAVE_SDHI0_TX,
	.slave_id_rx		= SHDMA_SLAVE_SDHI0_RX,
	.set_pwr		= sdhi0_set_pwr,
	.detect_irq		= R8A7373_IRQC_IRQ(50),
	.detect_msec		= 0,
	.get_cd			= sdhi0_get_cd,
	.set_dma		= sdhi0_set_dma,
	.port_cnt		= ARRAY_SIZE(sdhi0_gpio_setting_info),
	.gpio_setting_info	= sdhi0_gpio_setting_info,
};

static struct resource sdhi0_resources[] = {
	[0] = {
		.name	= "SDHI0",
		.start	= 0xee100000,
		.end	= 0xee1000ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(118),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device sdhi0_device = {
	.name		= "renesas_sdhi",
	.id		= 0,
	.dev		= {
		.platform_data	= &sdhi0_info,
	},
	.num_resources	= ARRAY_SIZE(sdhi0_resources),
	.resource	= sdhi0_resources,
};

static int sdhi1_get_pwr(struct platform_device *pdev)
{
	return gpio_get_value(GPIO_PORT260);
}

static void sdhi1_set_pwr(struct platform_device *pdev, int state)
{
	static int power_state;

	printk(KERN_ALERT "%s: %s\n", __func__, (state ? "on" : "off"));

	if (state != power_state) {
		power_state = state;
		gpio_set_value(GPIO_PORT260, state);
	}
}

static int sdhi1_get_cd(struct platform_device *pdev)
{
	/*
	In SSG , SDHI1 channel is using wlan .
	For wlan case its non removable card
	thats we dont have external interrupt.
	For that reason we returning  1. */
	return 1;/*return gpio_get_value(GPIO_PORT327) ? 0 : 1;*/
}

#define SDHI1_VOLTAGE (MMC_VDD_165_195 | MMC_VDD_20_21 | MMC_VDD_21_22 \
			| MMC_VDD_22_23 | MMC_VDD_23_24 | MMC_VDD_24_25 \
			| MMC_VDD_25_26 | MMC_VDD_26_27 | MMC_VDD_27_28 \
			| MMC_VDD_28_29 | MMC_VDD_29_30 | MMC_VDD_30_31 \
			| MMC_VDD_31_32 | MMC_VDD_32_33 | MMC_VDD_33_34 \
			| MMC_VDD_34_35 | MMC_VDD_35_36)

static struct renesas_sdhi_platdata sdhi1_info = {
	.caps		= MMC_CAP_SDIO_IRQ | MMC_CAP_NONREMOVABLE | MMC_CAP_4_BIT_DATA |
			  MMC_CAP_POWER_OFF_CARD | MMC_CAP_DISABLE,
	.pm_caps	= MMC_PM_KEEP_POWER | MMC_PM_IGNORE_PM_NOTIFY,
	.flags		= RENESAS_SDHI_SDCLK_OFFEN,
	.slave_id_tx	= SHDMA_SLAVE_SDHI1_TX,
	.slave_id_rx	= SHDMA_SLAVE_SDHI1_RX,
	.set_pwr	= sdhi1_set_pwr,
	.get_pwr	= sdhi1_get_pwr,
	.detect_irq	= 0,
	.detect_msec	= 0,
	.get_cd		= sdhi1_get_cd,
	.ocr		= MMC_VDD_165_195 | MMC_VDD_32_33 | MMC_VDD_33_34,
};

static struct resource sdhi1_resources[] = {
	[0] = {
		.name	= "SDHI1",
		.start	= 0xee120000,
		.end	= 0xee1200ff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(119),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device sdhi1_device = {
	.name		= "renesas_sdhi",
	.id		= 1,
	.dev		= {
		.platform_data	= &sdhi1_info,
	},
	.num_resources	= ARRAY_SIZE(sdhi1_resources),
	.resource	= sdhi1_resources,
};
