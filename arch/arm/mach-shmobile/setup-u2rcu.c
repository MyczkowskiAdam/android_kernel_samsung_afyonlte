/*
 * arch/arm/mach-shmobile/setup-u2rcu.c
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
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <mach/setup-u2camera.h>
#include <mach/setup-u2rcu.h>
#include <mach/irqs.h>

#include <mach/setup-u2csi2.h>
#include <media/sh_mobile_rcu.h>

struct sh_mobile_rcu_companion csi20 = {
	.id		= 0,
	.num_resources	= ARRAY_SIZE(csi20_resources),
	.resource	= csi20_resources,
	.platform_data	= &csi20_info,
};

struct sh_mobile_rcu_info sh_mobile_rcu0_info = {
	.flags		= 0,
	.csi2		= &csi20,
	.mod_name	= "sh_mobile_rcu.0",
	.led		= main_cam_led,
};

struct resource rcu0_resources[] = {
	[0] = {
		.name	= "RCU0",
		.start	= 0xfe910000,
		.end	= 0xfe91022b,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(83),
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		/* place holder for contiguous memory */
	},
};

struct platform_device rcu0_device = {
	.name			= "sh_mobile_rcu",
	.id				= 0, /* "rcu0" clock */
	.num_resources	= ARRAY_SIZE(rcu0_resources),
	.resource		= rcu0_resources,
	.dev = {
		.platform_data	= &sh_mobile_rcu0_info,
	},
};

struct sh_mobile_rcu_companion csi21 = {
	.id		= 1,
	.num_resources	= ARRAY_SIZE(csi21_resources),
	.resource	= csi21_resources,
	.platform_data	= &csi21_info,
};

struct sh_mobile_rcu_info sh_mobile_rcu1_info = {
	.flags		= 0,
	.csi2		= &csi21,
	.mod_name	= "sh_mobile_rcu.1",
};

struct resource rcu1_resources[] = {
	[0] = {
		.name	= "RCU1",
		.start	= 0xfe914000,
		.end	= 0xfe91422b,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= gic_spi(84),
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		/* place holder for contiguous memory */
	},
};

struct platform_device rcu1_device = {
	.name			= "sh_mobile_rcu",
	.id				= 1, /* "rcu1" clock */
	.num_resources	= ARRAY_SIZE(rcu1_resources),
	.resource		= rcu1_resources,
	.dev	= {
		.platform_data	= &sh_mobile_rcu1_info,
	},
};

