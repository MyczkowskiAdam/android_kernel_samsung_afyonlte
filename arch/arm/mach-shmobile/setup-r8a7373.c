/*
 * r8a7373 processor support
 *
 * Copyright (C) 2012  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_data/rmobile_hwsem.h>
#include <linux/platform_device.h>
#include <linux/i2c/i2c-sh_mobile.h>
#include <linux/i2c-gpio.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/system_info.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <mach/common.h>
#include <mach/irqs.h>
#include <mach/r8a7373.h>
#include <mach/serial.h>
#include <mach/memory-r8a7373.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <mach/setup-u2audio.h>
#include <mach/setup-u2ion.h>
#include <mach/setup-u2timers.h>
#ifdef CONFIG_SH_RAMDUMP
#include <mach/ramdump.h>
#endif
#if defined CONFIG_CPU_IDLE || defined CONFIG_SUSPEND
#ifndef CONFIG_PM_HAS_SECURE
#include "pm_ram0.h"
#else /*CONFIG_PM_HAS_SECURE*/
#include "pm_ram0_tz.h"
#endif /*CONFIG_PM_HAS_SECURE*/
#endif

#ifdef CONFIG_RENESAS_BT
#include <mach/dev-renesas-bt.h>
#endif

#include <mach/board-bcm4334-bt.h>

#include <mach/sec_debug.h>
#if defined(CONFIG_SEC_DEBUG_INFORM_IOTABLE)
#include <mach/sec_debug_inform.h>
#endif

#ifdef CONFIG_MFD_D2153
#include <linux/d2153/core.h>
#endif
#include <mach/setup-u2sci.h>

#ifdef CONFIG_ARCH_SHMOBILE
void __iomem *dummy_write_mem;
#endif

static struct map_desc r8a7373_io_desc[] __initdata = {
#ifdef PM_FUNCTION_START
/* We arrange for some of ICRAM 0 to be MT_MEMORY_NONCACHED, so
 * it can be executed from, for the PM code; it is then Normal Uncached memory,
 * with the XN (eXecute Never) bit clear. However, the data area of the ICRAM
 * has to be MT_DEVICE, to satisfy data access size requirements of the ICRAM.
 */
	{
		.virtual	= __IO_ADDRESS(0xe6000000),
		.pfn		= __phys_to_pfn(0xe6000000),
		.length		= PM_FUNCTION_START-0xe6000000,
		.type		= MT_DEVICE
	},
	{
		.virtual	= __IO_ADDRESS(PM_FUNCTION_START),
		.pfn		= __phys_to_pfn(PM_FUNCTION_START),
		.length		= PM_FUNCTION_END-PM_FUNCTION_START,
		.type		= MT_MEMORY_NONCACHED
	},
	{
		.virtual	= __IO_ADDRESS(PM_FUNCTION_END),
		.pfn		= __phys_to_pfn(PM_FUNCTION_END),
		.length		= 0xe7000000-PM_FUNCTION_END,
		.type		= MT_DEVICE
	},
#else
	{
		.virtual	= __IO_ADDRESS(0xe6000000),
		.pfn		= __phys_to_pfn(0xe6000000),
		.length		= SZ_16M,
		.type		= MT_DEVICE
	},
#endif
	{
		.virtual	= __IO_ADDRESS(0xf0000000),
		.pfn		= __phys_to_pfn(0xf0000000),
		.length		= SZ_2M,
		.type		= MT_DEVICE
	},
#if defined(CONFIG_SEC_DEBUG_INFORM_IOTABLE)
	{
		.virtual	= SEC_DEBUG_INFORM_VIRT,
		.pfn		= __phys_to_pfn(SEC_DEBUG_INFORM_PHYS),
		.length		= SZ_4K,
		.type		= MT_UNCACHED,
	},
#endif
};

void __init r8a7373_map_io(void)
{
	iotable_init(r8a7373_io_desc, ARRAY_SIZE(r8a7373_io_desc));
	init_consistent_dma_size(8 << 20);
#if defined(CONFIG_SEC_DEBUG)
	sec_debug_init();
#endif
}


/* IIC0 */
static struct i2c_sh_mobile_platform_data i2c0_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= false,
	.bus_data_delay	= MIN_SDA_DELAY ,
	.clks_per_count = 2,
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

static struct platform_device i2c0_device = {
	.name		= "i2c-sh_mobile",
	.id		= 0,
	.resource	= i2c0_resources,
	.num_resources	= ARRAY_SIZE(i2c0_resources),
	.dev		= {
		.platform_data	= &i2c0_platform_data,
	},
};

/* IIC1 */
static struct i2c_sh_mobile_platform_data i2c1_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= false,
	.bus_data_delay	= MIN_SDA_DELAY,
	.clks_per_count = 2,
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

static struct platform_device i2c1_device = {
	.name		= "i2c-sh_mobile",
	.id		= 1,
	.resource	= i2c1_resources,
	.num_resources	= ARRAY_SIZE(i2c1_resources),
	.dev		= {
		.platform_data	= &i2c1_platform_data,
	},
};

/* IIC2 */
static struct i2c_sh_mobile_platform_data i2c2_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= false,
	.bus_data_delay	= MIN_SDA_DELAY,
	.clks_per_count = 2,
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

static struct platform_device i2c2_device = {
	.name		= "i2c-sh_mobile",
	.id		= 2,
	.resource	= i2c2_resources,
	.num_resources	= ARRAY_SIZE(i2c2_resources),
	.dev		= {
		.platform_data	= &i2c2_platform_data,
	},
};

/* IIC3 */
static struct i2c_sh_mobile_platform_data i2c3_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= false,
	.bus_data_delay	= MIN_SDA_DELAY,
	.clks_per_count = 2,
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

static struct platform_device i2c3_device = {
	.name		= "i2c-sh_mobile",
	.id		= 3,
	.resource	= i2c3_resources,
	.num_resources	= ARRAY_SIZE(i2c3_resources),
	.dev		= {
		.platform_data	= &i2c3_platform_data,
	},
};

/* IIC0H */
static struct i2c_sh_mobile_platform_data i2c4_platform_data = {
	.bus_speed	= 400000,
        .pin_multi	= false,
#if defined(CONFIG_MACH_AFYONLTE) || defined(CONFIG_MACH_HEATLTE)
		.bus_data_delay = MIN_SDA_DELAY,
#else
		.bus_data_delay = I2C_SDA_163NS_DELAY,
#endif
	.scl_info	= {
		.port_num	= GPIO_PORT84,
		.port_func	= GPIO_FN_I2C_SCL0H,
	},
	.sda_info	= {
		.port_num	= GPIO_PORT85,
		.port_func	= GPIO_FN_I2C_SDA0H,
	},
	.clks_per_count = 2,
};

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

static struct platform_device i2c4_device = {
	.name		= "i2c-sh_mobile",
	.id		= 4,
	.resource	= i2c4_resources,
	.num_resources	= ARRAY_SIZE(i2c4_resources),
	.dev		= {
		.platform_data	= &i2c4_platform_data,
	},
};

/* IIC1H */
#ifdef CONFIG_FB_R_MOBILE_VX5B3D
static struct i2c_sh_mobile_platform_data i2c5_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= true,
	.bus_data_delay = MIN_SDA_DELAY,
	.scl_info	= {
		.port_num	= GPIO_PORT86,
		.port_func	= GPIO_FN_I2C_SCL1H,
	},
	.sda_info	= {
		.port_num	= GPIO_PORT87,
		.port_func	= GPIO_FN_I2C_SDA1H,
	},
	.clks_per_count = 2,
};

static struct resource i2c5_resources[] = {
	[0] = {
		.name	= "IIC5",
		.start	= 0xe682a000,
		.end	= 0xe682a425 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
	/* In ES2, 189 is for I2C5 and 190 for I2CB. */
		.start	= gic_spi(189),
		.end	= gic_spi(189),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device i2c5_device = {
	.name		= "i2c-sh_mobile",
	.id		= 5,
	.resource	= i2c5_resources,
	.num_resources	= ARRAY_SIZE(i2c5_resources),
	.dev = {
		.platform_data	= &i2c5_platform_data,
	},
};
#endif

/* IIC2H */
#ifndef CONFIG_SPI_SH_MSIOF
static struct i2c_sh_mobile_platform_data i2c6_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= true,
	.bus_data_delay = MIN_SDA_DELAY,
	.scl_info	= {
		.port_num	= GPIO_PORT82,
		.port_func	= GPIO_FN_I2C_SCL2H,
	},
	.sda_info	= {
		.port_num	= GPIO_PORT83,
		.port_func	= GPIO_FN_I2C_SDA2H,
	},
	.clks_per_count = 2,
};
static struct resource i2c6_resources[] = {
	[0] = {
		.name	= "IIC6",
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

static struct platform_device i2c6_device = {
	.name		= "i2c-sh_mobile",
	.id             = 6,
	.resource       = i2c6_resources,
	.num_resources  = ARRAY_SIZE(i2c6_resources),
	.dev            = {
		.platform_data  = &i2c6_platform_data,
	},
};
#endif
/* IIC3H */
static struct i2c_sh_mobile_platform_data i2c7_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= true,
	.bus_data_delay = MIN_SDA_DELAY,
	.scl_info	= {
		.port_num	= GPIO_PORT273,
		.port_func	= GPIO_FN_I2C_SCL3H,
	},
	.sda_info	= {
		.port_num	= GPIO_PORT274,
		.port_func	= GPIO_FN_I2C_SDA3H,
	},
	.clks_per_count = 2,
};

static struct resource i2c7_resources[] = {
	[0] = {
		.name	= "IIC7",
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
static struct platform_device i2c7_device = {
	.name		= "i2c-sh_mobile",
	.id		= 7,
	.resource	= i2c7_resources,
	.num_resources	= ARRAY_SIZE(i2c7_resources),
	.dev		= {
		.platform_data	= &i2c7_platform_data,
	},
};

/* IICM */
static struct i2c_sh_mobile_platform_data i2c8_platform_data = {
	.bus_speed	= 400000,
	.pin_multi	= false,
	.bus_data_delay = MIN_SDA_DELAY,
};

static struct resource i2c8_resources[] = {
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

static struct platform_device i2c8_device = {
	.name		= "i2c-sh7730",
	.id		= 8,
	.resource	= i2c8_resources,
	.num_resources	= ARRAY_SIZE(i2c8_resources),
	.dev		= {
		.platform_data	= &i2c8_platform_data,
	},
};

/* SYS-DMAC */
/* GPIO Port number needs to be modified by the respective driver module
Udealy=5 will set I2C bus speed to 100k HZ */

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

static const struct sh_dmae_slave_config r8a7373_dmae_slaves[] = {
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
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF7_TX,
		.addr		= 0xe6470040,
		.chcr		= CHCR_TX(XMIT_SZ_8BIT),
		.mid_rid	= 0x35,
	}, {
		.slave_id	= SHDMA_SLAVE_SCIF7_RX,
		.addr		= 0xe6470060,
		.chcr		= CHCR_RX(XMIT_SZ_8BIT),
		.mid_rid	= 0x36,
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI0_TX,
		.addr		= 0xee100030,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc1,
		.burst_sizes	= (1 << 1) | (1 << 4) | (1 << 5),
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI0_RX,
		.addr		= 0xee100030,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc2,
		.burst_sizes	= (1 << 1) | (1 << 4) | (1 << 5),
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI1_TX,
		.addr		= 0xee120030,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc5,
		.burst_sizes	= (1 << 1) | (1 << 4) | (1 << 5),
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI1_RX,
		.addr		= 0xee120030,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc6,
		.burst_sizes	= (1 << 1) | (1 << 4) | (1 << 5),
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI2_TX,
		.addr		= 0xee140030,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0xc9,
		.burst_sizes	= (1 << 1) | (1 << 4) | (1 << 5),
	}, {
		.slave_id	= SHDMA_SLAVE_SDHI2_RX,
		.addr		= 0xee140030,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xca,
		.burst_sizes	= (1 << 1) | (1 << 4) | (1 << 5),
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
		.slave_id	= SHDMA_SLAVE_FSI2A_TX,
		.addr		= 0xec230024,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0xd5,
	}, {
		.slave_id	= SHDMA_SLAVE_FSI2A_RX,
		.addr		= 0xec230020,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xd6,
	}, {
		.slave_id	= SHDMA_SLAVE_FSI2B_TX,
		.addr		= 0xec230064,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0xd9,
	}, {
		.slave_id	= SHDMA_SLAVE_FSI2B_RX,
		.addr		= 0xec230060,
		.chcr		= CHCR_RX(XMIT_SZ_16BIT),
		.mid_rid	= 0xda,
	}, {
		.slave_id	= SHDMA_SLAVE_SCUW_FFD_TX,
		.addr		= 0xec700708,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0x79,
	}, {
		.slave_id	= SHDMA_SLAVE_SCUW_FFU_RX,
		.addr		= 0xec700714,
		.chcr		= CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid	= 0x7a,
	}, {
		.slave_id	= SHDMA_SLAVE_SCUW_CPUFIFO_0_TX,
		.addr		= 0xec700720,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0x7d,
	}, {
		.slave_id	= SHDMA_SLAVE_SCUW_CPUFIFO_2_RX,
		.addr		= 0xec700738,
		.chcr		= CHCR_RX(XMIT_SZ_32BIT),
		.mid_rid	= 0x7e,
	}, {
		.slave_id	= SHDMA_SLAVE_SCUW_CPUFIFO_1_TX,
		.addr		= 0xec70072c,
		.chcr		= CHCR_TX(XMIT_SZ_32BIT),
		.mid_rid	= 0x81,
	}, {
		.slave_id	= SHDMA_SLAVE_PCM2PWM_TX,
		.addr		= 0xec380080,
		.chcr		= CHCR_TX(XMIT_SZ_16BIT),
		.mid_rid	= 0x91,
	},
};

#define DMAE_CHANNEL(_offset)					\
	{							\
		.offset		= _offset - 0x20,		\
		.dmars		= _offset - 0x20 + 0x40,	\
	}

static const struct sh_dmae_channel r8a7373_dmae_channels[] = {
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
};

static const unsigned int ts_shift[] = TS_SHIFT;

static struct sh_dmae_pdata r8a7373_dmae_platform_data = {
	.slave		= r8a7373_dmae_slaves,
	.slave_num	= ARRAY_SIZE(r8a7373_dmae_slaves),
	.channel	= r8a7373_dmae_channels,
	.channel_num	= ARRAY_SIZE(r8a7373_dmae_channels),
	.ts_low_shift	= 3,
	.ts_low_mask	= 0x18,
	.ts_high_shift	= (20 - 2),	/* 2 bits for shifted low TS */
	.ts_high_mask	= 0x00300000,
	.ts_shift	= ts_shift,
	.ts_shift_num	= ARRAY_SIZE(ts_shift),
	.dmaor_init	= DMAOR_DME,
};

static struct resource r8a7373_dmae_resources[] = {
	{
		/* DescriptorMEM */
		.start	= 0xFE00A000,
		.end	= 0xFE00A7FC,
		.flags	= IORESOURCE_MEM,
	},
	{
		/* Registers including DMAOR and channels including DMARSx */
		.start	= 0xfe000020,
		.end	= 0xfe008a00 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		/* DMA error IRQ */
		.start	= gic_spi(167),
		.end	= gic_spi(167),
		.flags	= IORESOURCE_IRQ,
	},
	{
		/* IRQ for channels 0-17 */
		.start	= gic_spi(147),
		.end	= gic_spi(164),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device dma0_device = {
	.name		= "sh-dma-engine",
	.id		= 0,
	.resource	= r8a7373_dmae_resources,
	.num_resources	= ARRAY_SIZE(r8a7373_dmae_resources),
	.dev		= {
		.platform_data	= &r8a7373_dmae_platform_data,
	},
};

/*
 * These three HPB semaphores will be requested at board-init timing,
 * and globally available (even for out-of-tree loadable modules).
 */
struct hwspinlock *r8a7373_hwlock_gpio;
EXPORT_SYMBOL(r8a7373_hwlock_gpio);
struct hwspinlock *r8a7373_hwlock_cpg;
EXPORT_SYMBOL(r8a7373_hwlock_cpg);
struct hwspinlock *r8a7373_hwlock_sysc;
EXPORT_SYMBOL(r8a7373_hwlock_sysc);

static struct resource pmu_resources[] = {
	[0] = {
		.name	= "cpu0",
		.start	= gic_spi(77),
		.end	= gic_spi(77),
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.name	= "cpu1",
		.start	= gic_spi(78),
		.end	= gic_spi(78),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device pmu_device = {
	.name		= "arm-pmu",
	.resource	= pmu_resources,
	.num_resources	= ARRAY_SIZE(pmu_resources),
};

#ifdef CONFIG_SMECO
static struct resource smc_resources[] = {
	[0] = {
		.start	= gic_spi(193),
		.end	= gic_spi(193),
		.flags	= IORESOURCE_IRQ,
	},
	[1] = {
		.start	= gic_spi(194),
		.end	= gic_spi(194),
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= gic_spi(195),
		.end	= gic_spi(195),
		.flags	= IORESOURCE_IRQ,
	},
	[3] = {
		.start	= gic_spi(196),
		.end	= gic_spi(196),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device smc_netdevice0 = {
	.name		= "smc_net_device",
	.id		= 0,
	.resource	= smc_resources,
	.num_resources	= ARRAY_SIZE(smc_resources),
};

static struct platform_device smc_netdevice1 = {
	.name		= "smc_net_device",
	.id		= 1,
	.resource	= smc_resources,
	.num_resources	= ARRAY_SIZE(smc_resources),
};
#endif /* CONFIG_SMECO */

/* Bus Semaphores 0 */
static struct hwsem_desc r8a7373_hwsem0_descs[] = {
	HWSEM(SMGPIO, 0x20),
	HWSEM(SMCPG, 0x50),
	HWSEM(SMSYSC, 0x70),
};

static struct hwsem_pdata r8a7373_hwsem0_platform_data = {
	.base_id	= SMGPIO,
	.descs		= r8a7373_hwsem0_descs,
	.nr_descs	= ARRAY_SIZE(r8a7373_hwsem0_descs),
};

static struct resource r8a7373_hwsem0_resources[] = {
	{
		.start	= 0xe6001800,
		.end	= 0xe600187f,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device hwsem0_device = {
	.name		= "rmobile_hwsem",
	.id		= 0,
	.resource	= r8a7373_hwsem0_resources,
	.num_resources	= ARRAY_SIZE(r8a7373_hwsem0_resources),
	.dev = {
		.platform_data = &r8a7373_hwsem0_platform_data,
	},
};

/* Bus Semaphores 1 */
static struct hwsem_desc r8a7373_hwsem1_descs[] = {
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

static struct hwsem_pdata r8a7373_hwsem1_platform_data = {
	.base_id	= SMGP000,
	.descs		= r8a7373_hwsem1_descs,
	.nr_descs	= ARRAY_SIZE(r8a7373_hwsem1_descs),
};

static struct resource r8a7373_hwsem1_resources[] = {
	{
		.start	= 0xe6001800,
		.end	= 0xe600187f,
		.flags	= IORESOURCE_MEM,
	},
	{
		/* software extension base */
		.start	= SDRAM_SOFT_SEMAPHORE_TVRF_START_ADDR,
		.end	= SDRAM_SOFT_SEMAPHORE_TVRF_END_ADDR,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device hwsem1_device = {
	.name		= "rmobile_hwsem",
	.id		= 1,
	.resource	= r8a7373_hwsem1_resources,
	.num_resources	= ARRAY_SIZE(r8a7373_hwsem1_resources),
	.dev		= {
		.platform_data	= &r8a7373_hwsem1_platform_data,
	},
};

/* Bus Semaphores 2 */
static struct hwsem_desc r8a7373_hwsem2_descs[] = {
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

static struct hwsem_pdata r8a7373_hwsem2_platform_data = {
	.base_id	= SMGP100,
	.descs		= r8a7373_hwsem2_descs,
	.nr_descs	= ARRAY_SIZE(r8a7373_hwsem2_descs),
};

static struct resource r8a7373_hwsem2_resources[] = {
	{
		.start	= 0xe6001800,
		.end	= 0xe600187f,
		.flags	= IORESOURCE_MEM,
	},
	{
		/* software bit extension */
		.start	= SDRAM_SOFT_SEMAPHORE_E20_START_ADDR,
		.end	= SDRAM_SOFT_SEMAPHORE_E20_END_ADDR,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device hwsem2_device = {
	.name		= "rmobile_hwsem",
	.id			= 2,
	.resource	= r8a7373_hwsem2_resources,
	.num_resources	= ARRAY_SIZE(r8a7373_hwsem2_resources),
	.dev = {
		.platform_data = &r8a7373_hwsem2_platform_data,
	},
};


static struct resource sgx_resources[] = {
	{
		.start	= 0xfd000000,
		.end	= 0xfd00bfff,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= gic_spi(92),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device sgx_device = {
	.name		= "pvrsrvkm",
	.id		= -1,
	.resource	= sgx_resources,
	.num_resources	= ARRAY_SIZE(sgx_resources),
};

#ifdef CONFIG_SH_RAMDUMP
static struct hw_register_range ramdump_res[] __initdata = {
	{
		.start	= 0xFE400000,	/*SBSC_SETTING_00_S*/
		.end	= 0xFE40011C,	/*SBSC_SETTING_00_E*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xFE400200,	/*SBSC_SETTING_01_S*/
		.end	= 0xFE400240,	/*SBSC_SETTING_01_E*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xFE400358,	/*SBSC_MON_SETTING*/
		.end	= 0xFE400358,	/*SBSC_MON_SETTING*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xFE401000,	/*SBSC_PHY_SETTING_00_S*/
		.end	= 0xFE401004,	/*SBSC_PHY_SETTING_00_E*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xFE4011F4,	/*SBSC_PHY_SETTING_01*/
		.end	= 0xFE4011F4,	/*SBSC_PHY_SETTING_01*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xFE401050,	/*SBSC_PHY_SETTING_02_S*/
		.end	= 0xFE4010BC,	/*SBSC_PHY_SETTING_02_E*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6150000,	/*CPG_SETTING_00_S*/
		.end	= 0xE6150200,	/*CPG_SETTING_00_E*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6151000,	/*CPG_SETTING_01_S*/
		.end	= 0xE6151180,	/*CPG_SETTING_01_E*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6020000,	/*RWDT_CONDITION_00*/
		.end	= 0xE6020000,	/*RWDT_CONDITION_00*/
		.width	= HW_REG_16BIT,
	},
	{
		.start	= 0xE6020004,	/*RWDT_CONDITION_01*/
		.end	= 0xE6020004,	/*RWDT_CONDITION_01*/
		.width	= HW_REG_8BIT,
	},
	{
		.start	= 0xE6020008,	/*RWDT_CONDITION_02*/
		.end	= 0xE6020008,	/*RWDT_CONDITION_02*/
		.width	= HW_REG_8BIT,
	},
	{
		.start	= 0xE6030000,	/*SWDT_CONDITION_00*/
		.end	= 0xE6030000,	/*SWDT_CONDITION_00*/
		.width	= HW_REG_16BIT,
	},
	{
		.start	= 0xE6030004,	/*SWDT_CONDITION_01*/
		.end	= 0xE6030004,	/*SWDT_CONDITION_01*/
		.width	= HW_REG_8BIT,
	},
	{
		.start	= 0xE6030008,	/*SWDT_CONDITION_02*/
		.end	= 0xE6030008,	/*SWDT_CONDITION_02*/
		.width	= HW_REG_8BIT,
	},
	{
		.start	= 0xE61D0000,	/*SUTC_CONDITION_00*/
		.end	= 0xE61D0000,	/*SUTC_CONDITION_00*/
		.width	= HW_REG_16BIT,
	},
	{
		.start	= 0xE61D0040,	/*SUTC_CONDITION_01*/
		.end	= 0xE61D0040,	/*SUTC_CONDITION_01*/
		.width	= HW_REG_16BIT,
	},
	{
		.start	= 0xE61D0044,	/*SUTC_CONDITION_02*/
		.end	= 0xE61D0048,	/*SUTC_CONDITION_03*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6130500,	/*CMT15_CONDITION_00*/
		.end	= 0xE6130500,	/*CMT15_CONDITION_00*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6130510,	/*CMT15_CONDITION_01*/
		.end	= 0xE6130510,	/*CMT15_CONDITION_01*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6130514,	/*CMT15_CONDITION_02*/
		.end	= 0xE6130514,	/*CMT15_CONDITION_02*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6130518,	/*CMT15_CONDITION_03*/
		.end	= 0xE6130518,	/*CMT15_CONDITION_03*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6180000,	/*SYSC_SETTING_00_S*/
		.end	= 0xE61800FC,	/*SYSC_SETTING_00_E*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6180200,	/*SYSC_SETTING_01_S*/
		.end	= 0xE618027C,	/*SYSC_SETTING_01_E*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE618801C,	/*SYSC_RESCNT_00*/
		.end	= 0xE6188024,	/*SYSC_RESCNT_02*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6100020,	/*DBG_SETTING_00*/
		.end	= 0xE6100020,	/*DBG_SETTING_00*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE6100028,	/*DBG_SETTING_01*/
		.end	= 0xE610002c,	/*DBG_SETTING_02*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xF000010C,	/*GIC_SETTING_00*/
		.end	= 0xF0000110,	/*GIC_SETTING_01*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xF0100100,	/*PL310_SETTING_00*/
		.end	= 0xF0100104,	/*PL310_SETTING_01*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE61C0100,	/*INTC_SYS_INFO_00*/
		.end	= 0xE61C0104,	/*INTC_SYS_INFO_01*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE61C0300,	/*INTC_SYS_INFO_02*/
		.end	= 0xE61C0304,	/*INTC_SYS_INFO_03*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE623000C,	/*INTC_BB_INFO_00*/
		.end	= 0xE623000C,	/*INTC_BB_INFO_00*/
		.width	= HW_REG_32BIT,
	},
	{
		.start	= 0xE623200C,	/*INTC_BB_INFO_01*/
		.end	= 0xE623200C,	/*INTC_BB_INFO_01*/
		.width	= HW_REG_32BIT,
	},
	{
		/*NOTE: at the moment address increment is done by 4 byte steps
		 * so this will read one byte from 004 and one byte form 008 */
		.start	= 0xE6820004,	/*IIC0_SETTING_00*/
		.end	= 0xE6820008,	/*IIC0_SETTING_01*/
		.width	= HW_REG_8BIT,
		.pa		= POWER_A3SP,
		.msr	= MSTPSR1,
		.msb	= MSTPST116,
	},
	{
		.start	= 0xE682002C,	/*IIC0_SETTING_02*/
		.end	= 0xE682002C,	/*IIC0_SETTING_02*/
		.width	= HW_REG_8BIT,
		.pa	= POWER_A3SP,
		.msr	= MSTPSR1,
		.msb	= MSTPST116,
	},
	{
		.start	= 0xE62A0004,	/*IICB_SETTING_00*/
		.end	= 0xE62A0008,	/*IICB_SETTING_01*/
		.width	= HW_REG_8BIT,
		.msr	= MSTPSR5,
		.msb	= MSTPST525,
	},
	{
		.start	= 0xE62A002C,	/*IICB_SETTING_02*/
		.end	= 0xE62A002C,	/*IICB_SETTING_02*/
		.width	= HW_REG_8BIT,
		.msr	= MSTPSR5,
		.msb	= MSTPST525,
	},
	{
		.start	= 0xFE951000,	/*IPMMU_SETTING_S*/
		.end	= 0xFE9510FC,	/*IPMMU_SETTING_E*/
		.width	= HW_REG_32BIT,
		.pa	= POWER_A3R,
		.msr	= SMSTPCR0,
		.msb	= MSTO007,
	},
};

struct ramdump_plat_data ramdump_pdata __initdata = {
	.reg_dump_base = SDRAM_REGISTER_DUMP_AREA_START_ADDR,
	.reg_dump_size = SDRAM_REGISTER_DUMP_AREA_END_ADDR -
			SDRAM_REGISTER_DUMP_AREA_START_ADDR + 1,
	/* size of reg dump of each core */
	.core_reg_dump_size = SZ_1K,
	.num_resources = ARRAY_SIZE(ramdump_res),
	.hw_register_range = ramdump_res,
	.io_desc = r8a7373_io_desc,
	.io_desc_size = ARRAY_SIZE(r8a7373_io_desc),
};

/* platform_device structure can not be marked as __initdata as
 * it is used by platform_uevent etc. That is why __refdata needs
 * to be used. platform_data pointer is nulled in probe */
static struct platform_device ramdump_device __refdata = {
	.name = "ramdump",
	.dev.platform_data = &ramdump_pdata,
};
#endif

/* Removed unused SCIF Ports getting initialized
 * to reduce BOOT UP time "JIRAID 1382" */
static struct platform_device *r8a7373_early_devices[] __initdata = {
	&scif0_device,
	&scif4_device,
	&scif5_device,
	&pmu_device,
#ifdef CONFIG_SH_RAMDUMP
	&ramdump_device,
#endif
};
static struct resource mtd_res[] = {
                [0] = {
                                .name   = "mtd_trace",
                                .start     = SDRAM_STM_TRACE_BUFFER_START_ADDR,
                                .end       = SDRAM_STM_TRACE_BUFFER_END_ADDR,
                                .flags     = IORESOURCE_MEM,
                },
};
static struct platform_device mtd_device = {
                .name = "mtd_trace",
                .num_resources               = ARRAY_SIZE(mtd_res),
                .resource             = mtd_res,
};


/* HS-- ES20 Specific late devices for Dialog */
static struct platform_device *r8a7373_late_devices_es20_d2153[] __initdata = {
	&i2c0_device, /* IIC0  */
	&i2c1_device, /* IIC1  */
	&i2c2_device, /* IIC2  */
	&i2c3_device, /* IIC3  */
	&i2c4_device, /* IIC0H */
#ifdef CONFIG_FB_R_MOBILE_VX5B3D
	&i2c5_device, /* IIC1H*/
#endif
#ifndef CONFIG_SPI_SH_MSIOF
	&i2c6_device, /* IIC2H */
#endif
	&i2c7_device, /* IIC3H */
	&i2c8_device, /* IICM  */
	&dma0_device,
#ifdef CONFIG_SMECO
	&smc_netdevice0,
	&smc_netdevice1,
#endif
	&hwsem0_device,
	&hwsem1_device,
	&hwsem2_device,
	&sgx_device,
	&mtd_device,
};

void __init r8a7373_add_standard_devices(void)
{

	platform_add_devices(r8a7373_early_devices,
			ARRAY_SIZE(r8a7373_early_devices));

	if (((system_rev & 0xFFFF) >> 4) >= 0x3E1) {
		platform_add_devices(r8a7373_late_devices_es20_d2153,
			ARRAY_SIZE(r8a7373_late_devices_es20_d2153));
	}
/* ES2.0 change end */
}
/*Do Dummy write in L2 cache to avoid A2SL turned-off
	just after L2-sync operation */
#ifdef CONFIG_ARCH_SHMOBILE
void __init r8a7373_avoid_a2slpowerdown_afterL2sync(void)
{
	dummy_write_mem = __arm_ioremap(
	(unsigned long)(SDRAM_NON_SECURE_SPINLOCK_START_ADDR + 0x00000400),
	0x00000400/*1k*/, MT_UNCACHED);

	if (dummy_write_mem == NULL)
		printk(KERN_ERR "97373_a2slpowerdown_workaround Failed\n");
}
#endif
/* do nothing for !CONFIG_SMP or !CONFIG_HAVE_TWD */


#if defined(CONFIG_MFD_D2153)
struct regulator *emmc_regulator;

void d2153_mmcif_pwr_control(int onoff)
{
	int ret;

	printk(KERN_EMERG "%s %s\n", __func__, (onoff) ? "on" : "off");
	if (emmc_regulator == NULL) {
		printk(KERN_INFO " %s, %d\n", __func__, __LINE__);
		emmc_regulator = regulator_get(NULL, "vmmc");
		if (IS_ERR(emmc_regulator)) {
			printk(KERN_INFO "can not get vmmc regulator\n");
			return;
		}
	}

	if (onoff == 1) {
#if 1 /* always enabling the vmmc */
		if (!regulator_is_enabled(emmc_regulator)) {
			printk(KERN_INFO " %s, %d vmmc On\n", __func__,
				__LINE__);
			ret = regulator_enable(emmc_regulator);
			printk(KERN_INFO "regulator_enable ret = %d\n", ret);
		}
#else
		printk(KERN_INFO " %s, %d vmmc On\n", __func__, __LINE__);
		ret = regulator_enable(emmc_regulator);
		printk(KERN_INFO "regulator_enable ret = %d\n", ret);
#endif
	} else {
#if 0 /* always enabling the vmmc */
		printk(KERN_INFO "%s, %d vmmc Off\n", __func__, __LINE__);
		ret = regulator_disable(emmc_regulator);
		printk(KERN_INFO "regulator_disable ret = %d\n", ret);
#endif
	}
}
#endif

void mmcif_set_pwr(struct platform_device *pdev, int state)
{
#if defined(CONFIG_MFD_D2153)
	d2153_mmcif_pwr_control(1);
#endif /* CONFIG_MFD_D2153 */
}

void mmcif_down_pwr(struct platform_device *pdev)
{
#if defined(CONFIG_MFD_D2153)
	d2153_mmcif_pwr_control(0);
#endif /* CONFIG_MFD_D2153 */
}

/* Lock used while modifying register */
static DEFINE_SPINLOCK(io_lock);

void sh_modify_register8(void __iomem *addr, u8 clear, u8 set)
{
	unsigned long flags;
	u8 val;
	spin_lock_irqsave(&io_lock, flags);
	val = __raw_readb(addr);
	val &= ~clear;
	val |= set;
	__raw_writeb(val, addr);
	spin_unlock_irqrestore(&io_lock, flags);
}
EXPORT_SYMBOL_GPL(sh_modify_register8);

void sh_modify_register16(void __iomem *addr, u16 clear, u16 set)
{
	unsigned long flags;
	u16 val;
	spin_lock_irqsave(&io_lock, flags);
	val = __raw_readw(addr);
	val &= ~clear;
	val |= set;
	__raw_writew(val, addr);
	spin_unlock_irqrestore(&io_lock, flags);
}
EXPORT_SYMBOL_GPL(sh_modify_register16);

void sh_modify_register32(void __iomem *addr, u32 clear, u32 set)
{
	unsigned long flags;
	u32 val;
	spin_lock_irqsave(&io_lock, flags);
	val = __raw_readl(addr);
	val &= ~clear;
	val |= set;
	__raw_writel(val, addr);
	spin_unlock_irqrestore(&io_lock, flags);
}
EXPORT_SYMBOL_GPL(sh_modify_register32);

void __iomem *sbsc_sdmracr1a;

void SBSC_Init_520Mhz(void)
{
	unsigned long work;

	printk(KERN_ALERT "START < %s >\n", __func__);

	/* Check PLL3 status */
	work = __raw_readl(PLLECR);
	if (!(work & PLLECR_PLL3ST)) {
		printk(KERN_ALERT "PLLECR_PLL3ST is 0\n");
		return;
	}

	/* Set PLL3 = 1040 Mhz*/
	__raw_writel(PLL3CR_1040MHZ, PLL3CR);

	/* Wait PLL3 status on */
	while (1) {
		work = __raw_readl(PLLECR);
		work &= PLLECR_PLL3ST;
		if (work == PLLECR_PLL3ST)
			break;
	}

	/* Dummy read */
	__raw_readl(sbsc_sdmracr1a);
}
EXPORT_SYMBOL_GPL(SBSC_Init_520Mhz);

static int read_board_rev(void)
{
	int rev0, rev1, rev2, rev3, ret;
	int error;
	error = gpio_request(GPIO_PORT72, "HW_REV0");
	if (error < 0)
		goto ret_err;
	error = gpio_direction_input(GPIO_PORT72);
	if (error < 0)
		goto ret_err1;
	gpio_pull_down_port(GPIO_PORT72);

	rev0 = gpio_get_value(GPIO_PORT72);
	if (rev0 < 0) {
		error = rev0;
		goto ret_err1;
	}

	error = gpio_request(GPIO_PORT73, "HW_REV1");
	if (error < 0)
		goto ret_err1;
	error = gpio_direction_input(GPIO_PORT73);
	if (error < 0)
		goto ret_err2;
	gpio_pull_down_port(GPIO_PORT73);

	rev1 = gpio_get_value(GPIO_PORT73);
	if (rev1 < 0) {
		error = rev1;
		goto ret_err2;
	}

	error = gpio_request(GPIO_PORT74, "HW_REV2");
	if (error < 0)
		goto ret_err2;
	error = gpio_direction_input(GPIO_PORT74);
	if (error < 0)
		goto ret_err3;
	gpio_pull_down_port(GPIO_PORT74);

	rev2 = gpio_get_value(GPIO_PORT74);
	if (rev2 < 0) {
		error = rev2;
		goto ret_err3;
	}

	error = gpio_request(GPIO_PORT75, "HW_REV3");
	if (error < 0)
		goto ret_err3;
	error = gpio_direction_input(GPIO_PORT75);
	if (error < 0)
		goto ret_err4;
	gpio_pull_down_port(GPIO_PORT75);

	rev3 = gpio_get_value(GPIO_PORT75);
	if (rev3 < 0) {
		error = rev3;
		goto ret_err4;
	}

	ret =  rev3 << 3 | rev2 << 2 | rev1 << 1 | rev0;
	return ret;
ret_err4:
	 gpio_free(GPIO_PORT75);
ret_err3:
	 gpio_free(GPIO_PORT74);
ret_err2:
	 gpio_free(GPIO_PORT73);
ret_err1:
	 gpio_free(GPIO_PORT72);
ret_err:
	return error;
}
unsigned int u2_get_board_rev(void)
{
	static int board_rev_val = -1;
	unsigned int loop = 0;

	/*if Revision read is faild for 3 times genarate panic*/
	if (unlikely(board_rev_val < 0)) {
		for (loop = 0; loop < 3; loop++) {
			board_rev_val = read_board_rev();
			if (board_rev_val >= 0)
				break;
		}
		if (unlikely(loop == 3))
			panic("Board revision not found\n");
	}
	/*board revision is always be a +value*/

	return (unsigned int) board_rev_val;
}
EXPORT_SYMBOL_GPL(u2_get_board_rev);

void r8a7373_l2cache_init(void)
{
#ifdef CONFIG_CACHE_L2X0
	/*
	 * [30] Early BRESP enable
	 * [27] Non-secure interrupt access control
	 * [26] Non-secure lockdown enable
	 * [22] Shared attribute override enable
	 * [19:17] Way-size: b010 = 32KB
	 * [16] Accosiativity: 0 = 8-way
	 */
	if(((system_rev & 0xFFFF)>>4) >= 0x3E1)
	{
		/*The L2Cache is resized to 512 KB*/
		l2x0_init(IO_ADDRESS(0xf0100000), 0x4c460000, 0x820f0fff);
	}
#endif
}


void __init r8a7373_init_early(void)
{
	system_rev = __raw_readl(CCCR);

#ifdef CONFIG_ARM_TZ
	r8a7373_l2cache_init();
#endif
	/* override timer setup with soc-specific code */
	shmobile_timer.init = u2_timers_init;
}

/*
 * Common reserve for R8 A7373 - for memory carveouts
 */
void __init r8a7373_reserve(void)
{
	u2evm_ion_adjust();
	u2vcd_reserve();

#if defined(CONFIG_SEC_DEBUG)
	sec_debug_magic_init();
#endif
}
