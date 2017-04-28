#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/hwspinlock.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/r8a7373.h>
#include <mach/irqs.h>
#include <asm/hardware/cache-l2x0.h>
#include <linux/gpio_keys.h>
#include <linux/usb/r8a66597.h>
#include <mach/setup-u2usb.h>
#include <linux/mfd/tps80031.h>
#include <linux/dma-mapping.h>
#ifdef CONFIG_USB_OTG
#include <linux/usb/tusb1211.h>
#endif
#include <mach/r8a7373.h>
#define ENT_TPS80031_IRQ_BASE	(IRQPIN_IRQ_BASE + 64)
#define ENT_TPS80032_IRQ_BASE	(IRQPIN_IRQ_BASE + 64)
#define error_log(fmt, ...) printk(fmt, ##__VA_ARGS__)

#define TUSB_VENDOR_SPECIFIC1                           0x80

static int is_vbus_powered(void)
{
	int val = 0;
	int val1 = 0;
	int count = 10;

	/* Extract bit VBSTS in INTSTS0 register */
	val = __raw_readw(HSUSB_INTSTS0) & 0x80;

	while (--count) {
		msleep(1);
		val1 = __raw_readw(HSUSB_INTSTS0) & 0x80;
		if (val != val1) {
			count = 10;
			val = val1;
		}
	}

	printk(KERN_INFO "Value of Status register INTSTS0: %x\n",
			__raw_readw(HSUSB_INTSTS0));
	return val1>>7;
}

#define LOCK_TIME_OUT_MS 1000
static void usbhs_module_reset(void)
{
	unsigned long flags = 0;
	int ret = 0;

	static int ctr;

	ret = hwspin_lock_timeout_irqsave(r8a7373_hwlock_cpg,
		LOCK_TIME_OUT_MS, &flags);
	if (ret < 0)
		printk(KERN_INFO "Can't lock hwlock_cpg\n");
	else {
		__raw_writel(__raw_readl(SRCR2) |
				(1 << 14), SRCR2); /* USBHS-DMAC */
		__raw_writel(__raw_readl(SRCR3) | (1 << 22), SRCR3); /* USBHS */
		hwspin_unlock_irqrestore(r8a7373_hwlock_cpg, &flags);
	}
	udelay(50); /* wait for at least one EXTALR cycle */
	ret = hwspin_lock_timeout_irqsave(r8a7373_hwlock_cpg,
		LOCK_TIME_OUT_MS, &flags);
	if (ret < 0)
		printk(KERN_INFO "Can't lock hwlock_cpg\n");
	else {
		__raw_writel(__raw_readl(SRCR2) & ~(1 << 14), SRCR2);
		__raw_writel(__raw_readl(SRCR3) & ~(1 << 22), SRCR3);
		hwspin_unlock_irqrestore(r8a7373_hwlock_cpg, &flags);
	}
	/* wait for SuspendM bit being cleared by hardware */
	while (!(__raw_readw(PHYFUNCTR) & (1 << 14))) /* SUSMON */{
		mdelay(10);
		if (ctr++ > 100) {
			printk(KERN_INFO "FATAL ERROR: PHY Hang and NOT COMING out from LP mode\n");
			printk(KERN_INFO "Recover from FATAL ERROR\n");
			ctr = 0;
			break;
		}
	}
	__raw_writew(__raw_readw(PHYFUNCTR) |
		(1 << 13), PHYFUNCTR); /* PRESET */
	while (__raw_readw(PHYFUNCTR) & (1 << 13)) {
		mdelay(10);
		if (ctr++ > 100) {
			printk(KERN_INFO "FATAL ERROR: PHY Hang and NOT COMING out from LP mode\n");
			printk(KERN_INFO "Recover from FATAL ERROR\n");
			ctr = 0;
			break;
		}
	}



#ifdef CONFIG_USB_OTG
	__raw_writew(__raw_readw(PHYOTGCTR) |
		(1 << 8), PHYOTGCTR); /* IDPULLUP */
	msleep(50);
#endif
//Eye Diagram
		__raw_writew(0x0000, USB_SPADDR);       /* set HSUSB.SPADDR*/
                __raw_writew(0x0020, USB_SPEXADDR);     /* set HSUSB.SPEXADDR*/
                __raw_writew(0x004F, USB_SPWDAT);       /* set HSUSB.SPWDAT*/
                __raw_writew(USB_SPWR, USB_SPCTRL);     /* set HSUSB.SPCTRL*/
		mdelay(1);
//Eye Diagram
}

static void usb_sw_reset(void)
{
	unsigned long flags = 0;
	int ret = 0;

	ret = hwspin_lock_timeout_irqsave(r8a7373_hwlock_cpg,
		LOCK_TIME_OUT_MS, &flags);
	if (ret < 0)
		printk(KERN_INFO "Can't lock hwlock_cpg\n");
	else {
		__raw_writel(__raw_readl(SRCR2) |
			(1 << 14), SRCR2); /* USBHS-DMAC */
		__raw_writel(__raw_readl(SRCR3) | (1 << 22), SRCR3); /* USBHS */
		hwspin_unlock_irqrestore(r8a7373_hwlock_cpg, &flags);
	}
	udelay(50); /* wait for at least one EXTALR cycle */
	ret = hwspin_lock_timeout_irqsave(r8a7373_hwlock_cpg,
		LOCK_TIME_OUT_MS, &flags);
	if (ret < 0)
		printk(KERN_INFO "Can't lock hwlock_cpg\n");
	else {
		__raw_writel(__raw_readl(SRCR2) & ~(1 << 14), SRCR2);
		__raw_writel(__raw_readl(SRCR3) & ~(1 << 22), SRCR3);
		hwspin_unlock_irqrestore(r8a7373_hwlock_cpg, &flags);
	}
}

static struct portn_gpio_setting_info r8a66597_gpio_setting_info[] = {
	[0] = {
		.flag = 1,
		.port = GPIO_PORT203,
		.active = {
			.port_fn        = GPIO_FN_ULPI_DATA0,
			.pull           = PORTn_CR_PULL_DOWN,
			.direction      = PORTn_CR_DIRECTION_NOT_SET,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT203,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NONE,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[1] = {
		.flag = 1,
		.port = GPIO_PORT204,
		.active = {
			.port_fn        = GPIO_FN_ULPI_DATA1,
			.pull           = PORTn_CR_PULL_DOWN,
			.direction      = PORTn_CR_DIRECTION_NOT_SET,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT204,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NONE,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[2] = {
		.flag = 1,
		.port = GPIO_PORT205,
		.active = {
			.port_fn        = GPIO_FN_ULPI_DATA2,
			.pull           = PORTn_CR_PULL_DOWN,
			.direction      = PORTn_CR_DIRECTION_NOT_SET,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT205,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NONE,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[3] = {
		.flag = 1,
		.port = GPIO_PORT206,
		.active = {
			.port_fn	= GPIO_FN_ULPI_DATA3,
			.pull		= PORTn_CR_PULL_DOWN,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn	= GPIO_PORT206,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NONE,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[4] = {
		.flag = 1,
		.port = GPIO_PORT207,
		.active = {
			.port_fn        = GPIO_FN_ULPI_DATA4,
			.pull           = PORTn_CR_PULL_DOWN,
			.direction      = PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
	       .inactive = {
			.port_fn	= GPIO_PORT207,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NONE,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[5] = {
		.flag = 1,
		.port = GPIO_PORT208,
		.active = {
		       .port_fn		= GPIO_FN_ULPI_DATA5,
		       .pull		= PORTn_CR_PULL_DOWN,
		       .direction	= PORTn_CR_DIRECTION_NOT_SET,
		       .output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn	= GPIO_PORT208,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NONE,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[6] = {
		.flag = 1,
		.port = GPIO_PORT209,
		.active = {
			.port_fn	= GPIO_FN_ULPI_DATA6,
			.pull           = PORTn_CR_PULL_DOWN,
			.direction      = PORTn_CR_DIRECTION_NOT_SET,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT209,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NONE,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[7] = {
		.flag = 1,
		.port = GPIO_PORT210,
		.active = {
			.port_fn        = GPIO_FN_ULPI_DATA7,
			.pull           = PORTn_CR_PULL_DOWN,
			.direction      = PORTn_CR_DIRECTION_NOT_SET,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT210,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NONE,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[8] = {
		.flag = 1,
		.port = GPIO_PORT211,
		.active = {
			.port_fn        = GPIO_FN_ULPI_CLK,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NOT_SET,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT211,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NONE,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[9] = {
		.flag = 1,
		.port = GPIO_PORT212,
		.active = {
			.port_fn        = GPIO_FN_ULPI_STP,
			.pull           = PORTn_CR_PULL_UP,
			.direction      = PORTn_CR_DIRECTION_NOT_SET,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT212,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NONE,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[10] = {
		.flag = 1,
		.port = GPIO_PORT213,
		.active = {
			.port_fn        = GPIO_FN_ULPI_DIR,
			.pull           = PORTn_CR_PULL_DOWN,
			.direction      = PORTn_CR_DIRECTION_NOT_SET,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT213,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NONE,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[11] = {
		.flag = 1,
		.port = GPIO_PORT214,
		.active = {
			.port_fn        = GPIO_FN_ULPI_NXT,
			.pull           = PORTn_CR_PULL_DOWN,
			.direction      = PORTn_CR_DIRECTION_NOT_SET,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT214,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NONE,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[12] = {
		.flag = 1,
		.port = GPIO_PORT217,
		.active = {
			.port_fn        = GPIO_FN_VIO_CKO3,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_OUTPUT,
			.output_level   = PORTn_OUTPUT_LEVEL_LOW,
		},
		.inactive = {
			.port_fn        = GPIO_PORT217,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_NONE,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[13] = {
		.flag = 1,
		.port = GPIO_PORT130,
		.active = {
			.port_fn        = GPIO_PORT130,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_OUTPUT,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT130,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_OUTPUT,
			.output_level   = PORTn_OUTPUT_LEVEL_LOW,
		}
	},
	[14] = {
		.flag = 1,
		.port = GPIO_PORT131,
		.active = {
			.port_fn        = GPIO_PORT131,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_OUTPUT,
			.output_level   = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		.inactive = {
			.port_fn        = GPIO_PORT131,
			.pull           = PORTn_CR_PULL_OFF,
			.direction      = PORTn_CR_DIRECTION_OUTPUT,
			.output_level   = PORTn_OUTPUT_LEVEL_LOW,
		}
	},
};
static struct r8a66597_platdata usbhs_func_data_d2153 = {
	.is_vbus_powered = is_vbus_powered,
	.module_start	= usbhs_module_reset,
	.module_stop	= usb_sw_reset,
	.on_chip	= 1,
	.buswait	= 5,
	.max_bufnum	= 0xff,
	.vbus_irq	= ENT_TPS80031_IRQ_BASE + 19,
	.port_cnt		= ARRAY_SIZE(r8a66597_gpio_setting_info),
	.usb_gpio_setting_info  = r8a66597_gpio_setting_info,
	.dmac		= 1,
};

static struct r8a66597_platdata usbhs_func_data = {
	.is_vbus_powered = is_vbus_powered,
	.module_start	= usbhs_module_reset,
	.module_stop	= usb_sw_reset,
	.on_chip	= 1,
	.buswait	= 5,
	.max_bufnum	= 0xff,
#ifdef CONFIG_PMIC_INTERFACE
	.vbus_irq	= ENT_TPS80031_IRQ_BASE + TPS80031_INT_VBUS,
#else  /* CONFIG_PMIC_INTERFACE */
	.vbus_irq	= ENT_TPS80031_IRQ_BASE + TPS80031_INT_VBUS_DET,
#endif /* CONFIG_PMIC_INTERFACE */
	.port_cnt		= ARRAY_SIZE(r8a66597_gpio_setting_info),
	.usb_gpio_setting_info  = r8a66597_gpio_setting_info,
	.dmac		= 1,
};

static struct resource usbhs_resources[] = {
	[0] = {
		.name	= "USBHS",
		.start	= 0xe6890000,
		.end	= 0xe6890150 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(87) /* USBULPI */,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.name	= "USBHS-DMA",
		.start	= 0xe68a0000,
		.end	= 0xe68a0064 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[3] = {
		.start	= gic_spi(85) /* USBHSDMAC1 */,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device usbhs_func_device_d2153 = {
	.name	= "r8a66597_udc",
	.id	= 0,
	.dev = {
		.dma_mask		= NULL,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &usbhs_func_data_d2153,
	},
	.num_resources	= ARRAY_SIZE(usbhs_resources),
	.resource	= usbhs_resources,
};

struct platform_device usbhs_func_device = {
	.name	= "r8a66597_udc",
	.id	= 0,
	.dev = {
		.dma_mask		= NULL,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data		= &usbhs_func_data,
	},
	.num_resources	= ARRAY_SIZE(usbhs_resources),
	.resource	= usbhs_resources,
};
#ifdef CONFIG_USB_R8A66597_HCD
static void usb_host_port_power(int port, int power)
{
#ifdef CONFIG_PMIC_INTERFACE
	if (power)
		pmic_set_vbus(1);
	else
		pmic_set_vbus(0);
#endif
	return;
}
static struct r8a66597_platdata usb_host_data = {
	.module_start	= usbhs_module_reset,
	.on_chip = 1,
	.port_power = usb_host_port_power,
};

static struct resource usb_host_resources[] = {
	[0] = {
		.name	= "USBHS",
		.start	= 0xe6890000,
		.end	= 0xe689014b,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(87), /* USBULPI */
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device usb_host_device = {
	.name		= "r8a66597_hcd",
	.id		= 0,
	.dev = {
		.platform_data		= &usb_host_data,
		.dma_mask		= NULL,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(usb_host_resources),
	.resource	= usb_host_resources,
};
#endif /*CONFIG_USB_R8A66597_HCD*/
#ifdef CONFIG_USB_OTG
/*TUSB1211 OTG*/
static struct tusb1211_platform_data tusb1211_data = {
	.module_start = usbhs_module_reset,
};
static struct resource tusb1211_resource[] = {
	[0] = {
		.name	= "tusb1211_resource",
		.start	= 0xe6890000,
		.end	= 0xe689014b,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name	= "INT_ID",
		.start	= ENT_TPS80031_IRQ_BASE + TPS80031_INT_ID,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device tusb1211_device = {
	.name = "tusb1211_driver",
	.id = 0,
	.dev = {
		.platform_data = &tusb1211_data,
	},
	.num_resources = ARRAY_SIZE(tusb1211_resource),
	.resource = tusb1211_resource,
};
#endif /*CONFIG_USB_OTG*/

void __init USBGpio_init(void)
{
	int ret = 0;
	/* USBHS */
	ret = gpio_request(GPIO_FN_ULPI_DATA0, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_DATA0 failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_DATA1, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_DATA1 failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_DATA2, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_DATA2 failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_DATA3, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_DATA3 failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_DATA4, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_DATA4 failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_DATA5, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_DATA5 failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_DATA6, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_DATA6 failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_DATA7, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_DATA7 failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_CLK, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_CLK failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_STP, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_STP failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_DIR, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_DIR failed ! USB may not function\n");

	ret = gpio_request(GPIO_FN_ULPI_NXT, NULL);
	if (ret < 0)
		error_log("ERROR : ULPI_NXT failed ! USB may not function\n");

	ret = gpio_request(GPIO_PORT131, NULL);
	if (ret < 0)
		error_log("PORT131 failed!USB may not function\n");
	ret = gpio_direction_output(GPIO_PORT131, 0);
	if (ret < 0)
		error_log("PORT131 direction output(0) failed!\n");
	udelay(100); /* assert RESET_N (min pulse width 100 usecs) */
	ret = gpio_direction_output(GPIO_PORT131, 1);
	if (ret < 0)
		error_log("PORT131 direction output(1) failed!\n");

	ret = gpio_request(GPIO_PORT130, NULL);
	if (ret < 0)
		error_log("ERROR : PORT130 failed ! USB may not function\n");

	ret = gpio_direction_output(GPIO_PORT130, 1);
	if (ret < 0)
		error_log("ERROR : PORT130 direction output(1) failed !\n");

	/* start supplying VIO_CKO3@26MHz to REFCLK */
	ret = gpio_request(GPIO_FN_VIO_CKO3, NULL);
	if (ret < 0)
		error_log("ERROR : VIO_CKO3 failed ! USB may not function\n");
	clk_enable(clk_get(NULL, "vclk3_clk"));
}
