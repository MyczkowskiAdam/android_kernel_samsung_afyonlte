/*
 * r8a7373 processor support - SCIF related configuration
 *
 * Copyright (C) 2012  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 */
#ifndef __SETUP_U2_SCI_H__
#define __SETUP_U2_SCI_H__

#include <linux/serial_sci.h>
#include <mach/gpio.h>


/*GPIO Settings for SCIFA0 port */
static struct portn_gpio_setting_info scif0_gpio_setting_info[] = {
	[0] = { /* SCIFA0_TXD */
		.flag	= 1,
		.port	= GPIO_PORT128,
		/* GPIO settings to be retained at resume state */
		.active = {
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFA0_TXD,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_OUTPUT,
			/* PCP# VR13042588231. Set output level to low
			to retain initial Data Register value.
			This will maintain the current after deep sleep */
			.output_level	= PORTn_OUTPUT_LEVEL_LOW,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 0 */
			.port_fn	= GPIO_PORT128,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NONE,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[1] = { /* SCIFA0_RXD */
		.flag = 1,
		.port = GPIO_PORT129,
		/* GPIO settings to be retained at resume state */
		.active = {
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFA0_RXD,
			.pull		= PORTn_CR_PULL_UP,
			.direction	= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 0 */
			.port_fn	= GPIO_PORT129,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NONE,
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
	.ops		= &shmobile_sci_port_ops,
	/* GPIO settings */
	.port_count = ARRAY_SIZE(scif0_gpio_setting_info),
	.scif_gpio_setting_info = scif0_gpio_setting_info,
};

static struct platform_device scif0_device = {
	.name		= "sh-sci",
	.id		= 0,
	.dev		= {
		.platform_data	= &scif0_platform_data,
	},
};

/* ENABLE_UNUSED_SCIF_PORTS should be defined to include
unused SCIF ports (SCIF1, SCIF2, SCIF3, SCIF6 and SCIF7) */

#ifdef ENABLE_UNUSED_SCIF_PORTS

/* SCIFA1 */
static struct plat_sci_port scif1_platform_data = {
	.mapbase	= 0xe6c50000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFA,
	.irqs		= { gic_spi(103), gic_spi(103),
			gic_spi(103), gic_spi(103) },
	.ops		= &shmobile_sci_port_ops,
	/* GPIO settings */
	.port_count = 0,
	.scif_gpio_setting_info = NULL,
};

static struct platform_device scif1_device = {
	.name		= "sh-sci",
	.id		= 1,
	.dev		= {
		.platform_data	= &scif1_platform_data,
	},
};

/* SCIFA2 */
static struct plat_sci_port scif2_platform_data = {
	.mapbase	= 0xe6c60000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFA,
	.irqs		= { gic_spi(104), gic_spi(104),
			gic_spi(104), gic_spi(104) },
	.ops		= &shmobile_sci_port_ops,
	/* GPIO settings */
	.port_count = 0,
	.scif_gpio_setting_info = NULL,
};

static struct platform_device scif2_device = {
	.name		= "sh-sci",
	.id		= 2,
	.dev		= {
		.platform_data	= &scif2_platform_data,
	},
};

/* SCIFA3 */
static struct plat_sci_port scif3_platform_data = {
	.mapbase	= 0xe6c70000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFA,
	.irqs		= { gic_spi(105), gic_spi(105),
			gic_spi(105), gic_spi(105) },
	.ops		= &shmobile_sci_port_ops,
	/* GPIO settings */
	.port_count = 0,
	.scif_gpio_setting_info = NULL,
};

static struct platform_device scif3_device = {
	.name		= "sh-sci",
	.id		= 3,
	.dev		= {
		.platform_data	= &scif3_platform_data,
	},
};
#endif /* ENABLE_UNUSED_SCIF_PORTS */

/*GPIO Settings for SCIFB0 port */
static struct portn_gpio_setting_info scif4_gpio_setting_info[] = {
	[0] = { /* SCIFB0_TXD */
		.flag	= 1,
		.port	= GPIO_PORT137,
		/* GPIO settings to be retained at resume state */
		.active = {
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFB0_TXD,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFB0_TXD,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[1] = { /* SCIFB0_RXD */
		.flag = 0,
		.port = GPIO_PORT138,
		/* GPIO settings to be retained at resume state */
		.active = {
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFB0_RXD,
			.pull		= PORTn_CR_PULL_UP,
			.direction	= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFB0_RXD,
			.pull		= PORTn_CR_PULL_UP,
			.direction	= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[2] = { /* SCIFB0_CTS_ */
		.flag = 0,
		.port = GPIO_PORT38,
		/* GPIO settings to be retained at resume state */
		.active = {
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFB0_CTS_,
			.pull		= PORTn_CR_PULL_UP,
			.direction	= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
	},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFB0_CTS_,
			.pull		= PORTn_CR_PULL_UP,
			.direction	= PORTn_CR_DIRECTION_INPUT,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[3] = { /* SCIFB0_RTS_ */
		.flag = 1,
		.port = GPIO_PORT37,
		/* GPIO settings to be retained at resume state */
		.active = {
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFB0_RTS_,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 1 */
			.port_fn	= GPIO_FN_SCIFB0_RTS_,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
};

/* SCIFB0 */
static struct plat_sci_port scif4_platform_data = {
	.mapbase	= 0xe6c20000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFB,
	.irqs		= { gic_spi(107), gic_spi(107),
			gic_spi(107), gic_spi(107) },
	.ops		= &shmobile_sci_port_ops,
	.capabilities = SCIx_HAVE_RTSCTS,
	.rts_ctrl	= 0,
#if defined(CONFIG_RENESAS_BT)|| defined(CONFIG_BCM4330_BT) || defined(CONFIG_BCM4334_BT)
	.exit_lpm_cb	= bcm_bt_lpm_exit_lpm_locked,
#endif
	/* GPIO settings */
	.port_count = ARRAY_SIZE(scif4_gpio_setting_info),
	.scif_gpio_setting_info = scif4_gpio_setting_info,
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
			/* Function 2 - Direction not set as
			same as Deep sleep value */
			.port_fn	= GPIO_FN_SCIFB1_RTS,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 2 - Retain same initial value
			in deep sleep to avoid leakage or Hi-Z state */
			.port_fn	= GPIO_FN_SCIFB1_RTS,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[1] = { /* SCIFB1_CTS */
		.flag = 1,
		.port = GPIO_PORT77,
		/* GPIO settings to be retained at resume state */
		.active = {
			/* Function 2 - Direction not set as
			same as Deep sleep value */
			.port_fn	= GPIO_FN_SCIFB1_CTS,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 2 - Retain same initial value
			in deep sleep to avoid leakage or Hi-Z state */
			.port_fn	= GPIO_FN_SCIFB1_CTS,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[2] = { /* SCIFB1_TXD */
		.flag = 1,
		.port = GPIO_PORT78,
		/* GPIO settings to be retained at resume state */
		.active = {
			/* Function 2 - Direction not set as
			same as Deep sleep value */
			.port_fn	= GPIO_FN_SCIFB1_TXD,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 2 - Retain same initial value
			in deep sleep to avoid leakage or Hi-Z state */
			.port_fn	= GPIO_FN_SCIFB1_TXD,
			.pull		= PORTn_CR_PULL_OFF,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
	[3] = { /* SCIFB1_RXD */
		.flag = 1,
		.port = GPIO_PORT79,
		/* GPIO settings to be retained at resume state */
		.active = {
			/* Function 2 - Direction not set as
			same as Deep sleep value */
			.port_fn	= GPIO_FN_SCIFB1_RXD,
			.pull		= PORTn_CR_PULL_UP,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		},
		/* GPIO settings to be set at suspend state */
		.inactive = {
			/* Function 2 - Retain same initial value
			in deep sleep to avoid leakage or Hi-Z state */
			.port_fn	= GPIO_FN_SCIFB1_RXD,
			.pull		= PORTn_CR_PULL_UP,
			.direction	= PORTn_CR_DIRECTION_NOT_SET,
			.output_level	= PORTn_OUTPUT_LEVEL_NOT_SET,
		}
	},
};

/* SCIFB1 */
static struct plat_sci_port scif5_platform_data = {
	.mapbase	= 0xe6c30000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4_BIS,
	.type		= PORT_SCIFB,
	.irqs		= { gic_spi(108), gic_spi(108),
			gic_spi(108), gic_spi(108) },
	.ops		= &shmobile_sci_port_ops,
	.capabilities = SCIx_HAVE_RTSCTS,
	/* GPIO settings */
	.port_count = ARRAY_SIZE(scif5_gpio_setting_info),
	.scif_gpio_setting_info = scif5_gpio_setting_info,
};

static struct platform_device scif5_device = {
	.name		= "sh-sci",
	.id		= 5,
	.dev		= {
		.platform_data	= &scif5_platform_data,
	},
};

#ifdef ENABLE_UNUSED_SCIF_PORTS
/* SCIFB2 */
static struct plat_sci_port scif6_platform_data = {
	.mapbase	= 0xe6ce0000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFB,
	.irqs		= { gic_spi(116), gic_spi(116),
			gic_spi(116), gic_spi(116) },
	.ops		= &shmobile_sci_port_ops,
	/* GPIO settings */
	.port_count = 0,
	.scif_gpio_setting_info = NULL,
};

static struct platform_device scif6_device = {
	.name		= "sh-sci",
	.id		= 6,
	.dev		= {
		.platform_data	= &scif6_platform_data,
	},
};

/* SCIFB3 */
static struct plat_sci_port scif7_platform_data = {
	.mapbase	= 0xe6470000,
	.flags		= UPF_BOOT_AUTOCONF | UPF_IOREMAP,
	.scscr		= SCSCR_RE | SCSCR_TE,
	.scbrr_algo_id	= SCBRR_ALGO_4,
	.type		= PORT_SCIFB,
	.irqs		= { gic_spi(117), gic_spi(117),
			gic_spi(117), gic_spi(117) },
	.ops		= &shmobile_sci_port_ops,
	/* GPIO settings */
	.port_count = 0,
	.scif_gpio_setting_info = NULL,
};

static struct platform_device scif7_device = {
	.name		= "sh-sci",
	.id		= 7,
	.dev		= {
		.platform_data	= &scif7_platform_data,
	},
};
#endif /* ENABLE_UNUSED_SCIF_PORTS */

#endif /* __SETUP_U2_SCI_H__ */
