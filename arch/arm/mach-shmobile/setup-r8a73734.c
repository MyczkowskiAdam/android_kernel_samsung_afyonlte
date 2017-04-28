#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clocksource.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/platform_data/rmobile_hwsem.h>
#include <linux/serial_sci.h>
#include <linux/sh_timer.h>
#include <linux/i2c/i2c-sh_mobile.h>
#include <asm/sched_clock.h>
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/r8a7373.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>
#include <mach/dev-renesas-bt.h>
#include <mach/sh_cmt.h>

#define CKS(_name, _divisor) { .name = _name, .divisor = _divisor }
#define I2C_SDA_NODELAY	0
#define I2C_SDA_163NS_DELAY	17

static bool is_es20(void);

static struct sh_timer_clock cmt1_cks_table[] = {
	[0] = CKS("cp_clk", 8),
	[1] = CKS("cp_clk", 32),
	[2] = CKS("cp_clk", 128),
	[3] = CKS("cp_clk", 1),
	[4] = CKS("r_clk", 8),
	[5] = CKS("r_clk", 32),
	[6] = CKS("r_clk", 128),
	[7] = CKS("r_clk", 1),
	/* Pseudo 32KHz/1 is omitted */
};

static struct sh_timer_config cmt11_platform_data = {
	.name			= "CMT11",
	.channel_offset		= 0x1000,
	.timer_bit		= 1,
	.clockevent_rating	= 125,
	.cks_table	= cmt1_cks_table,
	.cks_num	= ARRAY_SIZE(cmt1_cks_table),
	.cks		= 3,
	.cmcsr_init	= 0x120, /* Free-run, request interrupt, debug */
};

static struct resource cmt11_resources[] = {
	{
		.name	= "CMT11",
		.start	= 0xe6130100,
		.end	= 0xe6131003,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= gic_spi(94),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device cmt11_device = {
	.name		= "sh_cmt",
	.id		= 11,
	.dev		= {
			.platform_data	= &cmt11_platform_data,
	},
	.resource	= cmt11_resources,
	.num_resources	= ARRAY_SIZE(cmt11_resources),
};

static struct sh_timer_config cmt12_platform_data = {
	.name			= "CMT12",
	.channel_offset		= 0x1000 - 0x200,
	.timer_bit		= 2,
	.clockevent_rating	= 125,
	.cks_table	= cmt1_cks_table,
	.cks_num	= ARRAY_SIZE(cmt1_cks_table),
	.cks		= 3,
	.cmcsr_init	= 0x120, /* Free-run, request interrupt, debug */
};

static struct resource cmt12_resources[] = {
	{
		.name	= "CMT12",
		.start	= 0xe6130200,
		.end	= 0xe6131003,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= gic_spi(95),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device cmt12_device = {
	.name		= "sh_cmt",
	.id		= 12,
	.dev		= {
			.platform_data	= &cmt12_platform_data,
	},
	.resource	= cmt12_resources,
	.num_resources	= ARRAY_SIZE(cmt12_resources),
};

/*GPIO Settings for SCIFA0 port */
static struct portn_gpio_setting_info scif0_gpio_setting_info[] = {
	[0] = { /* SCIFA0_TXD */
		.flag = 1,
		.port = GPIO_PORT128,
		/* GPIO settings to be retained at resume state */
		.active = { 
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFA0_TXD, 
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_OUTPUT,
			.output_level = PORTn_OUTPUT_LEVEL_HIGH,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 0 */
			.port_fn 	= GPIO_PORT128, 
			.pull 	 	= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NONE,
			.output_level = PORTn_OUTPUT_LEVEL_NOT_SET,
		}                                   
	},
	[1] = { /* SCIFA0_RXD */
		.flag = 1,
		.port = GPIO_PORT129,
		/* GPIO settings to be retained at resume state */
		.active = { 
			 /* Function 1 */
			.port_fn	= GPIO_FN_SCIFA0_RXD, 
			.pull 		= PORTn_CR_PULL_UP,
			.direction	= PORTn_CR_DIRECTION_INPUT,
			.output_level = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 0 */
			.port_fn 		= GPIO_PORT129,
			.pull 			= PORTn_CR_PULL_OFF,
			.direction		= PORTn_CR_DIRECTION_NONE,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}                                   
	},
};

/* SCIFA0 */
static struct plat_sci_port scif0_platform_data = {
	.mapbase	= 0xe6450000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFA,
	.irqs		= { gic_spi(102), gic_spi(102),
			    gic_spi(102), gic_spi(102) },
	/* GPIO settings */
	.port_count = ARRAY_SIZE(scif0_gpio_setting_info),
	.scif_gpio_setting_info	= &scif0_gpio_setting_info,
};

static struct platform_device scif0_device = {
	.name		= "sh-sci",
	.id		= 0,
	.dev		= {
		.platform_data	= &scif0_platform_data,
	},
};

/* SCIFA1 */
static struct plat_sci_port scif1_platform_data = {
	.mapbase	= 0xe6c50000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFA,
	.irqs		= { gic_spi(103), gic_spi(103),
			    gic_spi(103), gic_spi(103) },
	/* GPIO settings */
	.port_count = 0,
	.scif_gpio_setting_info	= NULL,
};

static struct platform_device scif1_device = {
	.name		= "sh-sci",
	.id		= 1,
	.dev		= {
		.platform_data	= &scif1_platform_data,
	},
};

/*SCIFA2 */
static struct plat_sci_port scif2_platform_data = {
	.mapbase	= 0xe6c60000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFA,
	.irqs		= { gic_spi(104), gic_spi(104),
			    gic_spi(104), gic_spi(104) },
	/* GPIO settings */
	.port_count = 0,
	.scif_gpio_setting_info	= NULL,
};

static struct platform_device scif2_device = {
	.name		= "sh-sci",
	.id		= 2,
	.dev		= {
		.platform_data	= &scif2_platform_data,
	},
};

/*SCIFA3 */
static struct plat_sci_port scif3_platform_data = {
	.mapbase	= 0xe6c70000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFA,
	.irqs		= { gic_spi(105), gic_spi(105),
			    gic_spi(105), gic_spi(105) },
	/* GPIO settings */
	.port_count = 0,
	.scif_gpio_setting_info	= NULL,
};

static struct platform_device scif3_device = {
	.name		= "sh-sci",
	.id		= 3,
	.dev		= {
		.platform_data	= &scif3_platform_data,
	},
};

/*GPIO Settings for SCIFB0 port */
static struct portn_gpio_setting_info scif4_gpio_setting_info[] = {
	[0] = { /* SCIFB0_TXD */ 
		.flag = 1,
		.port = GPIO_PORT137,
		/* GPIO settings to be retained at resume state */
		.active = { 
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFB0_TXD, 
			.pull		= PORTn_CR_PULL_DOWN,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level = PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 1 */
			.port_fn 		= GPIO_FN_SCIFB0_TXD,
			.pull 			= PORTn_CR_PULL_OFF,
			.direction		= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}                                   
	},
	[1] = { /* SCIFB0_RXD */
		.flag = 0,
		.port = GPIO_PORT138,
		/* GPIO settings to be retained at resume state */
		.active = { 
			/* Function 1 */
			.port_fn 		= GPIO_FN_SCIFB0_RXD,
			.pull 			= PORTn_CR_PULL_UP,
			.direction		= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 1 */
			.port_fn 		= GPIO_FN_SCIFB0_RXD,
			.pull 			= PORTn_CR_PULL_UP,
			.direction		= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}                                   
	},
	[2] = { /* SCIFB0_CTS_ */
		.flag = 0,
		.port = GPIO_PORT38,
		/* GPIO settings to be retained at resume state */
		.active = { 
			/* Function 1 */
			.port_fn 		= GPIO_FN_SCIFB0_CTS_, 
			.pull 			= PORTn_CR_PULL_UP,
			.direction		= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 1 */
			.port_fn 		= GPIO_FN_SCIFB0_CTS_, 
			.pull 			= PORTn_CR_PULL_UP,
			.direction		= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}                                   
	},	 
	[3] = { /* SCIFB0_RTS_ */
		.flag = 1,
		.port = GPIO_PORT37,
		/* GPIO settings to be retained at resume state */
		.active = { 
			/* Function 1 */
			.port_fn 		= GPIO_FN_SCIFB0_RTS_, 
			.pull 			= PORTn_CR_PULL_DOWN,
			.direction		= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 1 */
			.port_fn 		= GPIO_FN_SCIFB0_RTS_, 
			.pull 			= PORTn_CR_PULL_OFF,
			.direction		= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}                                   
	},
};

/*SCIFB0 */
static struct plat_sci_port scif4_platform_data = {
	.mapbase	= 0xe6c20000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFB,
	.irqs		= { gic_spi(107), gic_spi(107),
			    gic_spi(107), gic_spi(107) },
	.rts_ctrl	= 0,
#if defined(CONFIG_RENESAS_BT)
	.exit_lpm_cb	= bcm_bt_lpm_exit_lpm_locked,
#endif
	/* GPIO settings */
	.port_count = ARRAY_SIZE(scif4_gpio_setting_info),
	.scif_gpio_setting_info	= &scif4_gpio_setting_info,
};

static struct platform_device scif4_device = {
	.name		= "sh-sci",
	.id		= 4,
	.dev		= {
		.platform_data	= &scif4_platform_data,
	},
};

/*GPIO Settings for SCIFB1 port */
static struct portn_gpio_setting_info scif5_gpio_setting_info[] = {
	[0] = { /* SCIFB1_RTS */ 
		.flag = 1,
		.port = GPIO_PORT76,
		/* GPIO settings to be retained at resume state */
		.active = { 
			/* Function 2 */
			.port_fn	= GPIO_FN_SCIFB1_RTS, 
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_OUTPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_LOW,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 0 */
			.port_fn 		= GPIO_PORT76,
			.pull 			= PORTn_CR_PULL_OFF,
			.direction		= PORTn_CR_DIRECTION_NONE,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}                                   
	},
	[1] = { /* SCIFB1_CTS */
		.flag = 1,
		.port = GPIO_PORT77,
		/* GPIO settings to be retained at resume state */
		.active = { 
			/* Function 2 */
			.port_fn 		= GPIO_FN_SCIFB1_CTS,
			.pull 			= PORTn_CR_PULL_UP,
			.direction		= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 0 */
			.port_fn 		= GPIO_PORT77,
			.pull 			= PORTn_CR_PULL_OFF,
			.direction		= PORTn_CR_DIRECTION_NONE,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}                                   
	},
	[2] = { /* SCIFB1_TXD */
		.flag = 1,
		.port = GPIO_PORT78,
		/* GPIO settings to be retained at resume state */
		.active = { 
			/* Function 2 */
			.port_fn 		= GPIO_FN_SCIFB1_TXD, 
			.pull 			= PORTn_CR_PULL_OFF,
			.direction		= PORTn_CR_DIRECTION_OUTPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_LOW,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			 /* Function 0 */
			.port_fn 		= GPIO_PORT78,
			.pull 			= PORTn_CR_PULL_OFF,
			.direction		= PORTn_CR_DIRECTION_NONE,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}                                   
	},	 
	[3] = { /* SCIFB1_RXD */
		.flag = 1,
		.port = GPIO_PORT79,
		/* GPIO settings to be retained at resume state */
		.active = { 
			/* Function 2 */
			.port_fn	= GPIO_FN_SCIFB1_RXD, 
			.pull		= PORTn_CR_PULL_UP,
			.direction	= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 0 */
			.port_fn 		= GPIO_PORT79, 
			.pull 			= PORTn_CR_PULL_DOWN,
			.direction		= PORTn_CR_DIRECTION_NONE,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}                                   
	},
};
/*SCIFB1 */
static struct plat_sci_port scif5_platform_data = {
	.mapbase	= 0xe6c30000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4_BIS,
	.type		= PORT_SCIFB,
	.irqs		= { gic_spi(108), gic_spi(108),
			    gic_spi(108), gic_spi(108) },
	/* GPIO settings */
	.port_count = ARRAY_SIZE(scif5_gpio_setting_info),
	.scif_gpio_setting_info	= &scif5_gpio_setting_info,
};

static struct platform_device scif5_device = {
	.name		= "sh-sci",
	.id		= 5,
	.dev		= {
		.platform_data	= &scif5_platform_data,
	},
};

/*SCIFB2 */
static struct plat_sci_port scif6_platform_data = {
	.mapbase	= 0xe6ce0000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFB,
	.irqs		= { gic_spi(116), gic_spi(116),
			    gic_spi(116), gic_spi(116) },
	/* GPIO settings */
	.port_count = 0,
	.scif_gpio_setting_info	= NULL,
};

static struct platform_device scif6_device = {
	.name		= "sh-sci",
	.id		= 6,
	.dev		= {
		.platform_data	= &scif6_platform_data,
	},
};

/* SCIFB3 - New channel Implementation for ES2.0*/
static struct plat_sci_port scif7_platform_data = {
        .mapbase        = 0xe6470000,
        .flags          = UPF_BOOT_AUTOCONF | UPF_IOREMAP,
        .scscr          = SCSCR_RE | SCSCR_TE,
        .scbrr_algo_id  = SCBRR_ALGO_4,
        .type           = PORT_SCIFB,
        .irqs           = { gic_spi(117), gic_spi(117),
                            gic_spi(117), gic_spi(117) },
		/* GPIO settings */
		.port_count = 0,
		.scif_gpio_setting_info	= NULL,
};

static struct platform_device scif7_device = {
        .name           = "sh-sci",
        .id             = 7,
        .dev            = {
        	.platform_data  = &scif7_platform_data,
        },
};

static struct i2c_sh_mobile_platform_data i2c0_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= false,
	.bus_data_delay	= I2C_SDA_NODELAY,
};

static struct resource i2c0_resources[] = {
	[0] = {
		.name	= "IIC0",
		.start	= 0xe6820000,
		.end	= 0xe6820425 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(184),
		.end	= gic_spi(184),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct i2c_sh_mobile_platform_data i2c1_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= false,
	.bus_data_delay	= I2C_SDA_NODELAY,
};

static struct resource i2c1_resources[] = {
	[0] = {
		.name	= "IIC1",
		.start	= 0xe6822000,
		.end	= 0xe6822425 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(185),
		.end	= gic_spi(185),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct i2c_sh_mobile_platform_data i2c2_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= false,
	.bus_data_delay	= I2C_SDA_NODELAY,
};

static struct resource i2c2_resources[] = {
	[0] = {
		.name	= "IIC2",
		.start	= 0xe6824000,
		.end	= 0xe6824425 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(186),
		.end	= gic_spi(186),
		.flags	= IORESOURCE_IRQ,
	},
};

//ES2.0 change start
static struct i2c_sh_mobile_platform_data i2c3_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= false,
	.bus_data_delay	= I2C_SDA_NODELAY,
};

static struct resource i2c3_resources[] = {
	[0] = {
		.name	= "IIC3",
		.start	= 0xe6826000,
		.end	= 0xe6826425 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(187),
		.end	= gic_spi(187),
		.flags	= IORESOURCE_IRQ,
	},
};
//ES2.0 change stop


static struct i2c_sh_mobile_platform_data i2c4_platform_data = {
	.bus_speed	= 400000,
        .pin_multi	= true,
	.bus_data_delay = I2C_SDA_163NS_DELAY,
	.scl_info	= {
		.port_num	= GPIO_PORT84,
		.port_func	= GPIO_FN_I2C_SCL0H,
	},
	.sda_info	= {
		.port_num	= GPIO_PORT85,
		.port_func	= GPIO_FN_I2C_SDA0H,
	},
};
//IIC0H
static struct resource i2c4_resources[] = {
	[0] = {
		.name	= "IIC4",
		.start	= 0xe6828000,
		.end	= 0xe6828425 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(188),
		.end	= gic_spi(188),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct i2c_sh_mobile_platform_data i2c5_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= true,
	.bus_data_delay = I2C_SDA_NODELAY,
	.scl_info	= {
		.port_num	= GPIO_PORT86,
		.port_func	= GPIO_FN_I2C_SCL1H,
	},
	.sda_info	= {
		.port_num	= GPIO_PORT87,
		.port_func	= GPIO_FN_I2C_SDA1H,
	},
};

static struct resource i2c5_resources_es20[] = {
	[0] = {
		.name	= "IIC5",
		.start	= 0xe682a000,
		.end	= 0xe682a425 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
	// In ES2, 189 is for I2C5 and 190 for I2CB.
		.start	= gic_spi(189),
		.end	= gic_spi(189),

		.flags	= IORESOURCE_IRQ,
	},
};

static struct i2c_sh_mobile_platform_data i2c6_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= false,
	.bus_data_delay = I2C_SDA_NODELAY,
};

static struct resource i2c6_resources[] = {
	[0] = {
		.name	= "IICM",
		.start	= 0xe6d20000,
		.end	= 0xe6d20009 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(191),
		.end	= gic_spi(191),
		.flags	= IORESOURCE_IRQ,
	},
};

#ifndef CONFIG_SPI_SH_MSIOF
//ES2.0 change start
static struct i2c_sh_mobile_platform_data i2c7_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= true,
	.bus_data_delay = I2C_SDA_NODELAY,
	.scl_info	= {
		.port_num	= GPIO_PORT82,
		.port_func	= GPIO_FN_I2C_SCL2H,
	},
	.sda_info	= {
		.port_num	= GPIO_PORT83,
		.port_func	= GPIO_FN_I2C_SDA2H,
	},
};
//IIC2H
static struct resource i2c7_resources[] = {
	[0] = {
		.name	= "IIC7",
		.start	= 0xe682c000,
		.end	= 0xe682c425 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(128),
		.end	= gic_spi(128),
		.flags	= IORESOURCE_IRQ,
	},
};
#endif

#ifndef CONFIG_PN544_NFC
static struct i2c_sh_mobile_platform_data i2c8_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= true,
	.bus_data_delay = I2C_SDA_NODELAY,
	.scl_info	= {
		.port_num	= GPIO_PORT273,
		.port_func	= GPIO_FN_I2C_SCL3H,
	},
	.sda_info	= {
		.port_num	= GPIO_PORT274,
		.port_func	= GPIO_FN_I2C_SDA3H,
	},
};
//IIC3H
static struct resource i2c8_resources[] = {
	[0] = {
		.name	= "IIC8",
		.start	= 0xe682e000,
		.end	= 0xe682e425 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(181),
		.end	= gic_spi(181),
		.flags	= IORESOURCE_IRQ,
	},
};
#endif
//ES2.0 change end

static struct platform_device i2c0_device = {
	.name		= "i2c-sh_mobile",
	.id		= 0,
	.resource	= i2c0_resources,
	.num_resources	= ARRAY_SIZE(i2c0_resources),
	.dev		= {
		.platform_data	= &i2c0_platform_data,
	},
};

static struct platform_device i2c1_device = {
	.name		= "i2c-sh_mobile",
	.id		= 1,
	.resource	= i2c1_resources,
	.num_resources	= ARRAY_SIZE(i2c1_resources),
	.dev		= {
		.platform_data	= &i2c1_platform_data,
	},
};

static struct platform_device i2c2_device = {
	.name		= "i2c-sh_mobile",
	.id		= 2,
	.resource	= i2c2_resources,
	.num_resources	= ARRAY_SIZE(i2c2_resources),
	.dev		= {
		.platform_data	= &i2c2_platform_data,
	},
};

//ES2.0 change start
static struct platform_device i2c3_device= {
	.name		= "i2c-sh_mobile",
	.id		= 3,
	.resource	= i2c3_resources,
	.num_resources	= ARRAY_SIZE(i2c3_resources),
	.dev		= {
		.platform_data	= &i2c3_platform_data,
	},
};
//ES2.0 change end

static struct platform_device i2c4_device = {
	.name		= "i2c-sh_mobile",
	.id		= 4,
	.resource	= i2c4_resources,
	.num_resources	= ARRAY_SIZE(i2c4_resources),
	.dev		= {
		.platform_data	= &i2c4_platform_data,
	},
};

static struct platform_device i2c5_device_es20 = {
	.name		= "i2c-sh_mobile",
	.id		= 5,
	.resource	= i2c5_resources_es20,
	.num_resources	= ARRAY_SIZE(i2c5_resources_es20),
	.dev		= {
		.platform_data	= &i2c5_platform_data,
	},
};

static struct platform_device i2c6_device = {
	.name		= "i2c-sh7730",
	.id		= 6,
	.resource	= i2c6_resources,
	.num_resources	= ARRAY_SIZE(i2c6_resources),
	.dev		= {
		.platform_data	= &i2c6_platform_data,
	},
};

#ifndef CONFIG_SPI_SH_MSIOF
// ES2.0 change start
static struct platform_device i2c7_device = {
	.name		= "i2c-sh_mobile",
	.id		= 7,
	.resource	= i2c7_resources,
	.num_resources	= ARRAY_SIZE(i2c7_resources),
	.dev		= {
		.platform_data	= &i2c7_platform_data,
	},
};
#endif

#ifndef CONFIG_PN544_NFC
static struct platform_device i2c8_device = {
	.name		= "i2c-sh_mobile",
	.id		= 8,
	.resource	= i2c8_resources,
	.num_resources	= ARRAY_SIZE(i2c8_resources),
	.dev		= {
		.platform_data	= &i2c8_platform_data,
	},
};
#endif
//ES2.0 change end

//GPIO Port number needs to be modified by the respective driver module
//Udealy=5 will set I2C bus speed to 100k HZ

static struct i2c_gpio_platform_data  i2c0gpio_platform_data = {
      .sda_pin        = GPIO_PORT5,
      .scl_pin        = GPIO_PORT4,
      .udelay         = 5,
};

static struct platform_device i2c0gpio_device = {
  .name          = "i2c-gpio",
  .id    = 9,
  .dev           = {
         .platform_data  = &i2c0gpio_platform_data,
  },
};

static struct i2c_gpio_platform_data  i2c1gpio_platform_data = {
      .sda_pin        = GPIO_PORT27,
      .scl_pin        = GPIO_PORT26,
      .udelay         = 5,
};

static struct platform_device i2c1gpio_device = {
  .name          = "i2c-gpio",
  .id    = 10,
  .dev           = {
         .platform_data  = &i2c1gpio_platform_data,
  },
};

/* Transmit sizes and respective CHCR register values */
enum {
	XMIT_SZ_8BIT		= 0,
	XMIT_SZ_16BIT		= 1,
	XMIT_SZ_32BIT		= 2,
	XMIT_SZ_64BIT		= 7,
	XMIT_SZ_128BIT		= 3,
	XMIT_SZ_256BIT		= 4,
	XMIT_SZ_512BIT		= 5,
};

/* log2(size / 8) - used to calculate number of transfers */
#define TS_SHIFT {			\
	[XMIT_SZ_8BIT]		= 0,	\
	[XMIT_SZ_16BIT]		= 1,	\
	[XMIT_SZ_32BIT]		= 2,	\
	[XMIT_SZ_64BIT]		= 3,	\
	[XMIT_SZ_128BIT]	= 4,	\
	[XMIT_SZ_256BIT]	= 5,	\
	[XMIT_SZ_512BIT]	= 6,	\
}

#define TS_INDEX2VAL(i) ((((i) & 3) << 3) | (((i) & 0xc) << (20 - 2)))
#define CHCR_TX(xmit_sz) (DM_FIX | SM_INC | 0x800 | TS_INDEX2VAL((xmit_sz)))
#define CHCR_RX(xmit_sz) (DM_INC | SM_FIX | 0x800 | TS_INDEX2VAL((xmit_sz)))

static const struct sh_dmae_slave_config r8a73734_dmae_slaves[] = {
	{
		.slave_id	= SHDMA_SLAVE_SCIF0_TX,
		.addr		= 0xe6450020,
		.chcr		= CHCR_TX(XMIT_SZ_8BIT),
		.mid_rid	= 0x21,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF0_RX,
		.addr		= 0xe6450024,
		.chcr		= CHCR_RX(XMIT_SZ_8BIT),
		.mid_rid	= 0x22,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF1_TX,
		.addr		= 0xe6c50020,
		.chcr		= CHCR_TX(XMIT_SZ_8BIT),
		.mid_rid	= 0x25,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF1_RX,
		.addr		= 0xe6c50024,
		.chcr		= CHCR_RX(XMIT_SZ_8BIT),
		.mid_rid	= 0x26,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF2_TX,
		.addr		= 0xe6c60020,
		.chcr		= CHCR_TX(XMIT_SZ_8BIT),
		.mid_rid	= 0x29,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF2_RX,
		.addr		= 0xe6c60024,
		.chcr		= CHCR_RX(XMIT_SZ_8BIT),
		.mid_rid	= 0x2a,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF3_TX,
		.addr		= 0xe6c70020,
		.chcr		= CHCR_TX(XMIT_SZ_8BIT),
		.mid_rid	= 0x2d,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF3_RX,
		.addr		= 0xe6c70024,
		.chcr		= CHCR_RX(XMIT_SZ_8BIT),
		.mid_rid	= 0x2e,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF4_TX,
		.addr		= 0xe6c20040,
		.chcr		= CHCR_TX(XMIT_SZ_8BIT),
		.mid_rid	= 0x3d,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF4_RX,
		.addr		= 0xe6c20060,
		.chcr		= CHCR_RX(XMIT_SZ_8BIT),
		.mid_rid	= 0x3e,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF5_TX,
		.addr		= 0xe6c30040,
		.chcr		= CHCR_TX(XMIT_SZ_8BIT),
		.mid_rid	= 0x19,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF5_RX,
		.addr		= 0xe6c30060,
		.chcr		= CHCR_RX(XMIT_SZ_8BIT),
		.mid_rid	= 0x1a,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF6_TX,
		.addr		= 0xe6ce0040,
		.chcr		= CHCR_TX(XMIT_SZ_8BIT),
		.mid_rid	= 0x1d,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF6_RX,
		.addr		= 0xe6ce0060,
		.chcr		= CHCR_RX(XMIT_SZ_8BIT),
		.mid_rid	= 0x1e,
	},{
		.slave_id	= SHDMA_SLAVE_SCIF7_TX,
		.addr		= 0xe6470040,
		.chcr		= CHCR_TX(XMIT_SZ_8BIT),
		.mid_rid	= 0x35,
	},{
		.slave_id	= SHDMA_SLAVE_SCIF7_RX,
		.addr		= 0xe6470060,
		.chcr		= CHCR_RX(XMIT_SZ_8BIT),
		.mid_rid	= 0x36,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI0_TX,
		.addr		= 0xee100030,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc1,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI0_RX,
		.addr		= 0xee100030,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc2,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI1_TX,
		.addr		= 0xee120030,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc5,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI1_RX,
		.addr		= 0xee120030,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc6,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI2_TX,
		.addr		= 0xee140030,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc9,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI2_RX,
		.addr		= 0xee140030,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xca,
	}, {
		.slave_id	= SHDMA_SLAVE_MMCIF0_TX,
		.addr		= 0xe6bd0034,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0xd1,
	}, {
		.slave_id	= SHDMA_SLAVE_MMCIF0_RX,
		.addr		= 0xe6bd0034,
		.chcr		= CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid	= 0xd2,
	}, {
		.slave_id	= SHDMA_SLAVE_MMCIF1_TX,
		.addr		= 0xe6be0034,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0xe1,
	}, {
		.slave_id	= SHDMA_SLAVE_MMCIF1_RX,
		.addr		= 0xe6be0034,
		.chcr		= CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid	= 0xe2,
	}, {
		.slave_id	= SHDMA_SLAVE_FSI2A_RX,
		.addr		= 0xec230020,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xd6,
	}, {
		.slave_id	= SHDMA_SLAVE_FSI2A_TX,
		.addr           = 0xec230024,
		.chcr           = CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid        = 0xd5,
	}, {
		.slave_id       = SHDMA_SLAVE_FSI2B_TX,
		.addr           = 0xec230064,
		.chcr           = CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid        = 0xd9,
	}, {
		.slave_id       = SHDMA_SLAVE_FSI2B_RX,
		.addr           = 0xec230060,
		.chcr           = CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid        = 0xda,
	}, {
		.slave_id       = SHDMA_SLAVE_SCUW_FFD_TX,
		.addr           = 0xEC700708,
		.chcr           = CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid        = 0x79,
	}, {
		.slave_id       = SHDMA_SLAVE_SCUW_FFU_RX,
		.addr           = 0xEC700714,
		.chcr           = CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid        = 0x7A,
	}, {
		.slave_id       = SHDMA_SLAVE_SCUW_CPUFIFO_0_TX,
		.addr           = 0xEC700720,
		.chcr           = CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid        = 0x7D,
	}, {
		.slave_id       = SHDMA_SLAVE_SCUW_CPUFIFO_2_RX,
		.addr           = 0xEC700738,
		.chcr           = CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid        = 0x7E,
	}, {
		.slave_id       = SHDMA_SLAVE_SCUW_CPUFIFO_1_TX,
		.addr           = 0xEC70072C,
		.chcr           = CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid        = 0x81,
	},{
		.slave_id       = SHDMA_SLAVE_PCM2PWM_TX,
		.addr           = 0xec380080,
		.chcr           = CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid        = 0x91,
	},
};

#define DMAE_CHANNEL(_offset)					\
	{							\
		.offset		= _offset - 0x20,		\
		.dmars		= _offset - 0x20 + 0x40,	\
	}

static const struct sh_dmae_channel r8a73734_dmae_channels[] = {
	DMAE_CHANNEL(0x8000),
	DMAE_CHANNEL(0x8080),
	DMAE_CHANNEL(0x8100),
	DMAE_CHANNEL(0x8180),
	DMAE_CHANNEL(0x8200),
	DMAE_CHANNEL(0x8280),
	DMAE_CHANNEL(0x8300),
	DMAE_CHANNEL(0x8380),
	DMAE_CHANNEL(0x8400),
	DMAE_CHANNEL(0x8480),
	DMAE_CHANNEL(0x8500),
	DMAE_CHANNEL(0x8580),
	DMAE_CHANNEL(0x8600),
	DMAE_CHANNEL(0x8680),
	DMAE_CHANNEL(0x8700),
	DMAE_CHANNEL(0x8780),
	DMAE_CHANNEL(0x8800),
	DMAE_CHANNEL(0x8880),
#if 0
	/*
	 * Tha last two channels, CH18 and CH19, are intended for debugging
	 * purposes and come with different functions (for instance, they're
	 * not capable of slave DMA feature).  Such channels should not be
	 * registered to the kernel and the shdma driver.
	 */
	DMAE_CHANNEL(0x8900),
	DMAE_CHANNEL(0x8980),
#endif
};

static const unsigned int ts_shift[] = TS_SHIFT;

static struct sh_dmae_pdata r8a73734_dmae_platform_data = {
	.slave          = r8a73734_dmae_slaves,
	.slave_num      = ARRAY_SIZE(r8a73734_dmae_slaves),
	.channel        = r8a73734_dmae_channels,
	.channel_num    = ARRAY_SIZE(r8a73734_dmae_channels),
	.ts_low_shift   = 3,
	.ts_low_mask    = 0x18,
	.ts_high_shift  = (20 - 2),     /* 2 bits for shifted low TS */
	.ts_high_mask   = 0x00300000,
	.ts_shift       = ts_shift,
	.ts_shift_num   = ARRAY_SIZE(ts_shift),
	.dmaor_init     = DMAOR_DME,
};

static struct resource r8a73734_dmae_resources[] = {
        {
		/* DescriptorMEM */
		.start  = 0xFE00A000,
		.end    = 0xFE00A7FC,
		.flags  = IORESOURCE_MEM,
	},
	{
		/* Registers including DMAOR and channels including DMARSx */
		.start  = 0xfe000020,
		.end    = 0xfe008a00 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		/* DMA error IRQ */
		.start  = gic_spi(167),
		.end    = gic_spi(167),
		.flags  = IORESOURCE_IRQ,
	},
	{
		/* IRQ for channels 0-17 */
		.start  = gic_spi(147),
		.end    = gic_spi(164),
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device dma0_device = {
	.name		= "sh-dma-engine",
	.id		= 0,
	.resource	= r8a73734_dmae_resources,
	.num_resources	= ARRAY_SIZE(r8a73734_dmae_resources),
	.dev		= {
		.platform_data	= &r8a73734_dmae_platform_data,
	},
};

static struct resource pmu_resources[] = {
  [0] = {
    .name= "cpu0",
    .start= gic_spi(77),
    .end= gic_spi(77),
    .flags= IORESOURCE_IRQ,
  },
  [1] = {
    .name= "cpu1",
    .start= gic_spi(78),
    .end= gic_spi(78),
    .flags= IORESOURCE_IRQ,
  },
};

static struct platform_device pmu_device = {
  .name= "arm-pmu",
  .resource= pmu_resources,
  .num_resources= ARRAY_SIZE(pmu_resources),
};


#ifdef CONFIG_SMECO
static struct resource smc_resources[] =
{
  [0] = {
          .start = gic_spi(193),
          .end   = gic_spi(193),
          .flags = IORESOURCE_IRQ,
        },
  [1] = {
          .start = gic_spi(194),
          .end   = gic_spi(194),
          .flags = IORESOURCE_IRQ,
        },
  [2] = {
          .start = gic_spi(195),
          .end   = gic_spi(195),
          .flags = IORESOURCE_IRQ,
        },
  [3] = {
          .start = gic_spi(196),
          .end   = gic_spi(196),
          .flags = IORESOURCE_IRQ,
        },
};

static struct platform_device smc_netdevice0 =
{
    .name          = "smc_net_device",
    .id            = 0,
    .resource      = smc_resources,
    .num_resources = ARRAY_SIZE(smc_resources),
};

static struct platform_device smc_netdevice1 =
{
    .name          = "smc_net_device",
    .id            = 1,
    .resource      = smc_resources,
    .num_resources = ARRAY_SIZE(smc_resources),
};
#endif // CONFIG_SMECO

/* Bus Semaphores 0 */
static struct hwsem_desc r8a73734_hwsem0_descs[] = {
	HWSEM(SMGPIO, 0x20),
	HWSEM(SMCPG, 0x50),
	HWSEM(SMSYSC, 0x70),
};

static struct hwsem_pdata r8a73734_hwsem0_platform_data = {
	.base_id	= SMGPIO,
	.descs		= r8a73734_hwsem0_descs,
	.nr_descs	= ARRAY_SIZE(r8a73734_hwsem0_descs),
};

static struct resource r8a73734_hwsem0_resources[] = {
	{
		.start  = 0xe6001800,
		.end    = 0xe600187f,
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device hwsem0_device = {
	.name		= "rmobile_hwsem",
	.id		= 0,
	.resource	= r8a73734_hwsem0_resources,
	.num_resources	= ARRAY_SIZE(r8a73734_hwsem0_resources),
	.dev		= {
		.platform_data	= &r8a73734_hwsem0_platform_data,
	},
};

/*
 * These three HPB semaphores will be requested at board-init timing,
 * and globally available (even for out-of-tree loadable modules).
 *
 * You don't have to call hwspin_lock_request_specific() each time
 * before acquiring these hwspinlock instances.
 */
struct hwspinlock *r8a73734_hwlock_gpio;
struct hwspinlock *r8a73734_hwlock_cpg;
struct hwspinlock *r8a73734_hwlock_sysc;
EXPORT_SYMBOL(r8a73734_hwlock_gpio);
EXPORT_SYMBOL(r8a73734_hwlock_cpg);
EXPORT_SYMBOL(r8a73734_hwlock_sysc);

/* Bus Semaphores 1 */
static struct hwsem_desc r8a73734_hwsem1_descs[] = {
	HWSEM(SMGP000, 0x30), HWSEM(SMGP001, 0x30),
	HWSEM(SMGP002, 0x30), HWSEM(SMGP003, 0x30),
	HWSEM(SMGP004, 0x30), HWSEM(SMGP005, 0x30),
	HWSEM(SMGP006, 0x30), HWSEM(SMGP007, 0x30),
	HWSEM(SMGP008, 0x30), HWSEM(SMGP009, 0x30),
	HWSEM(SMGP010, 0x30), HWSEM(SMGP011, 0x30),
	HWSEM(SMGP012, 0x30), HWSEM(SMGP013, 0x30),
	HWSEM(SMGP014, 0x30), HWSEM(SMGP015, 0x30),
	HWSEM(SMGP016, 0x30), HWSEM(SMGP017, 0x30),
	HWSEM(SMGP018, 0x30), HWSEM(SMGP019, 0x30),
	HWSEM(SMGP020, 0x30), HWSEM(SMGP021, 0x30),
	HWSEM(SMGP022, 0x30), HWSEM(SMGP023, 0x30),
	HWSEM(SMGP024, 0x30), HWSEM(SMGP025, 0x30),
	HWSEM(SMGP026, 0x30), HWSEM(SMGP027, 0x30),
	HWSEM(SMGP028, 0x30), HWSEM(SMGP029, 0x30),
	HWSEM(SMGP030, 0x30), HWSEM(SMGP031, 0x30),
};

static struct hwsem_pdata r8a73734_hwsem1_platform_data = {
	.base_id	= SMGP000,
	.descs		= r8a73734_hwsem1_descs,
	.nr_descs	= ARRAY_SIZE(r8a73734_hwsem1_descs),
};

static struct resource r8a73734_hwsem1_resources[] = {
	{
		.start  = 0xe6001800,
		.end    = 0xe600187f,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = 0x464ffc00,	/* software bit extension 464F FC00 */
		.end    = 0x464ffc7f,
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device hwsem1_device = {
	.name		= "rmobile_hwsem",
	.id		= 1,
	.resource	= r8a73734_hwsem1_resources,
	.num_resources	= ARRAY_SIZE(r8a73734_hwsem1_resources),
	.dev		= {
		.platform_data	= &r8a73734_hwsem1_platform_data,
	},
};

static struct hwsem_desc r8a73734_hwsem2_descs[] = {
	HWSEM(SMGP100, 0x40), HWSEM(SMGP101, 0x40),
	HWSEM(SMGP102, 0x40), HWSEM(SMGP103, 0x40),
	HWSEM(SMGP104, 0x40), HWSEM(SMGP105, 0x40),
	HWSEM(SMGP106, 0x40), HWSEM(SMGP107, 0x40),
	HWSEM(SMGP108, 0x40), HWSEM(SMGP109, 0x40),
	HWSEM(SMGP110, 0x40), HWSEM(SMGP111, 0x40),
	HWSEM(SMGP112, 0x40), HWSEM(SMGP113, 0x40),
	HWSEM(SMGP114, 0x40), HWSEM(SMGP115, 0x40),
	HWSEM(SMGP116, 0x40), HWSEM(SMGP117, 0x40),
	HWSEM(SMGP118, 0x40), HWSEM(SMGP119, 0x40),
	HWSEM(SMGP120, 0x40), HWSEM(SMGP121, 0x40),
	HWSEM(SMGP122, 0x40), HWSEM(SMGP123, 0x40),
	HWSEM(SMGP124, 0x40), HWSEM(SMGP125, 0x40),
	HWSEM(SMGP126, 0x40), HWSEM(SMGP127, 0x40),
	HWSEM(SMGP128, 0x40), HWSEM(SMGP129, 0x40),
	HWSEM(SMGP130, 0x40), HWSEM(SMGP131, 0x40),
};

static struct hwsem_pdata r8a73734_hwsem2_platform_data = {
	.base_id	= SMGP100,
	.descs		= r8a73734_hwsem2_descs,
	.nr_descs	= ARRAY_SIZE(r8a73734_hwsem2_descs),
};

static struct resource r8a73734_hwsem2_resources[] = {
	{
		.start  = 0xe6001800,
		.end    = 0xe600187f,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = 0x464ffe00,	/* software bit extension */
		.end    = 0x464ffe7f,
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device hwsem2_device = {
	.name		= "rmobile_hwsem",
	.id		= 2,
	.resource	= r8a73734_hwsem2_resources,
	.num_resources	= ARRAY_SIZE(r8a73734_hwsem2_resources),
	.dev		= {
		.platform_data	= &r8a73734_hwsem2_platform_data,
	},
};

static struct resource sgx_resources[] = {
	{
		.start = 0xfd000000,
		.end = 0xfd00bfff,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = gic_spi(92),
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device sgx_device = {
	.name = "pvrsrvkm",
	.id = -1,
	.resource = sgx_resources,
	.num_resources = ARRAY_SIZE(sgx_resources),
};

/* Removed unused SCIF Ports getting initialized
 * to reduce BOOT UP time "JIRAID 1382"  */
static struct platform_device *r8a73734_early_devices[] __initdata = {
	&cmt11_device,
	&cmt12_device,
	&scif0_device,
	&scif4_device,
	&scif5_device,
	&pmu_device,
};

#if 0
static struct platform_device *r8a73734_late_devices[] __initdata = {
	&i2c0_device,
	&i2c1_device,
	&i2c2_device,
	&i2c4_device,
	/*&i2c5_device,*/
	&i2c6_device,
	&i2c0gpio_device,
	&dma0_device,
#ifdef CONFIG_SMECO
	&smc_netdevice0,
	&smc_netdevice1,
#endif
};
#endif

// HS-- ES20 Specific late devices for Dialog
static struct platform_device *r8a73734_late_devices_es20_d2153[] __initdata = {
    &i2c0_device,
    &i2c1_device,
    &i2c2_device,
    &i2c3_device,
    &i2c4_device,
    &i2c5_device_es20,
    &i2c6_device,
#ifndef CONFIG_SPI_SH_MSIOF
    &i2c7_device,
#endif
#ifndef CONFIG_PN544_NFC
    &i2c8_device,
#endif
//    &i2c0gpio_device,
    &i2c1gpio_device,
    &dma0_device,
#ifdef CONFIG_SMECO
    &smc_netdevice0,
    &smc_netdevice1,
#endif
   &hwsem0_device,
   &hwsem1_device,
   &hwsem2_device,
   &sgx_device,
};

// HS-- ES20 Specific late devices
static struct platform_device *r8a73734_late_devices_es20[] __initdata = {
    &i2c0_device,
    &i2c1_device,
    &i2c2_device,
    &i2c3_device,
    &i2c4_device,
    &i2c5_device_es20,
    &i2c6_device,
#ifndef CONFIG_SPI_SH_MSIOF
    &i2c7_device,
#endif
#ifndef CONFIG_PN544_NFC
    &i2c8_device,
#endif
    &i2c0gpio_device,
    &i2c1gpio_device,
    &dma0_device,
#ifdef CONFIG_SMECO
    &smc_netdevice0,
    &smc_netdevice1,
#endif
   &hwsem0_device,
   &hwsem1_device,
   &hwsem2_device,
   &sgx_device,
};



extern spinlock_t sh_cmt_lock;	/* arch/arm/mach-shmobile/sh_cmt.c */

static struct clk *cmt10_clk;
static DEFINE_CLOCK_DATA(cd);

unsigned long long notrace sched_clock(void)
{
	u32 cyc = __raw_readl(CMCNT4);
	return cyc_to_sched_clock(&cd, cyc, (u32)~0);
}

static void notrace cmt_update_sched_clock(void)
{
	u32 cyc = __raw_readl(CMCNT4);
	update_sched_clock(&cd, cyc, (u32)~0);
}

static void cmt10_start(void)
{
	unsigned long flags;

	spin_lock_irqsave(&sh_cmt_lock, flags);
	__raw_writel(__raw_readl(CMCLKE) | (1 << 0), CMCLKE);
	spin_unlock_irqrestore(&sh_cmt_lock, flags);

	/* stop */
	__raw_writel(0, CMSTR0);

	/* setup */
	__raw_writel(0, CMCNT0);
	__raw_writel(0x103, CMCSR0); /* Free-running, DBGIVD, cp_clk/1 */
	__raw_writel(0xffffffff, CMCOR0);
	while (__raw_readl(CMCNT0) != 0)
		cpu_relax();

	/* start */
	__raw_writel(1, CMSTR0);
}

static void cmt10_stop(void)
{
	unsigned long flags;

	__raw_writel(0, CMSTR0);

	spin_lock_irqsave(&sh_cmt_lock, flags);
	__raw_writel(__raw_readl(CMCLKE) & ~(1 << 0), CMCLKE);
	spin_unlock_irqrestore(&sh_cmt_lock, flags);
}

void clocksource_mmio_suspend(struct clocksource *cs)
{
	cmt10_stop();
	clk_disable(cmt10_clk);
}

void clocksource_mmio_resume(struct clocksource *cs)
{
	clk_enable(cmt10_clk);
	cmt10_start();
}

static void r8a73734_clocksource_init(void)
{
	static struct clk *cp_clk, *r_clk;
	unsigned long flags, rate;

	/* Set up CMT14 for sched_clock - running forever */
	clk_enable(clk_get_sys("sh_cmt.14", NULL));
	r_clk = clk_get(NULL, "r_clk");
	clk_enable(r_clk);
	rate = clk_get_rate(r_clk);

	spin_lock_irqsave(&sh_cmt_lock, flags);
	__raw_writel(__raw_readl(CMCLKE) | (1 << 4), CMCLKE);
	spin_unlock_irqrestore(&sh_cmt_lock, flags);

	__raw_writel(0, CMSTR4);

	__raw_writel(0, CMCNT4);
	__raw_writel(0x107, CMCSR4); /* Free-running, DBGIVD, RCLK/1 */
	__raw_writel(0xffffffff, CMCOR4);
	while (__raw_readl(CMCNT4) != 0)
		cpu_relax();

	__raw_writel(1, CMSTR4);

	init_sched_clock(&cd, cmt_update_sched_clock, 32, rate);

	/* Set up CMT10 for clocksource - get halted during suspend */
	cmt10_clk = clk_get_sys("sh_cmt.10", NULL);
	clk_enable(cmt10_clk);
	cp_clk = clk_get(NULL, "cp_clk");
	clk_enable(cp_clk);
	rate = clk_get_rate(cp_clk);

	cmt10_start();

	clocksource_mmio_init(__io(CMCNT0), "cmt10", rate, 125, 32,
			      clocksource_mmio_readl_up);
}

void __init r8a73734_add_standard_devices(void)
{

	platform_add_devices(r8a73734_early_devices,
			ARRAY_SIZE(r8a73734_early_devices));
//ES2.0 change start

	if (u2_get_board_rev() >= BOARD_REV_0_5) {
		platform_add_devices(r8a73734_late_devices_es20_d2153,
				ARRAY_SIZE(r8a73734_late_devices_es20_d2153));
	} else {
		platform_add_devices(r8a73734_late_devices_es20,
				ARRAY_SIZE(r8a73734_late_devices_es20));
	}
//ES2.0 change end

/*	platform_add_devices(r8a73734_early_devices,
			ARRAY_SIZE(r8a73734_early_devices));
	platform_add_devices(r8a73734_late_devices,
			ARRAY_SIZE(r8a73734_late_devices));

	if( is_es20() )
	{
		printk("Loading ES20 late devices...\n");
		if (u2_get_board_rev() >= BOARD_REV_0_5) {
			platform_add_devices(r8a73734_late_devices_es20_d2153,
				ARRAY_SIZE(r8a73734_late_devices_es20_d2153));
		} else {
			platform_add_devices(r8a73734_late_devices_es20,
				ARRAY_SIZE(r8a73734_late_devices_es20));
		}
	 }
	*/
}

#define CCCR	IO_ADDRESS(0xe600101c)

static bool is_es20(void)
{
    unsigned int cccr = 0x00;
    unsigned int major = 0x00;
    unsigned int minor = 0x00;

    cccr = __raw_readl(__io(CCCR));

    major = ((cccr & 0xF0) >> 4) + 1;
    minor = cccr & 0x0F;

    return (((major&0xFF)<<4) + (minor&0xFF) == 0x20 );
}

extern void sh_cmt_register_devices(struct platform_device **devs, int num);

void __init r8a73734_add_early_devices(void)
{
	system_rev = __raw_readl(__io(CCCR));

	shmobile_clocksource_init = r8a73734_clocksource_init;

	early_platform_add_devices(r8a73734_early_devices,
			ARRAY_SIZE(r8a73734_early_devices));

#ifdef CONFIG_SH_TIMER_CMT_ARM
	sh_cmt_register_devices(r8a73734_early_devices, NR_CPUS);
#endif
}
