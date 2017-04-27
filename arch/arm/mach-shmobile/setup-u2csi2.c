/*
 * arch/arm/mach-shmobile/setup-u2csi2.c
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

#include <media/sh_mobile_csi2.h>

#include <mach/irqs.h>
#include <mach/setup-u2camera.h>
#include <mach/setup-u2csi2.h>

struct sh_csi2_client_config csi20_clients[] = {
	{
		.phy		= SH_CSI2_PHY_MAIN,
		.lanes		= 0xF,
		.channel	= 0,
		.pdev		= &camera_devices[0],
	},
};

struct sh_csi2_pdata csi20_info = {
	.type		= SH_CSI2C,
	.clients	= csi20_clients,
	.num_clients	= ARRAY_SIZE(csi20_clients),
	.flags		= SH_CSI2_ECC | SH_CSI2_CRC,
	.ipr		= 0x24,
	.ipr_set	= (0x0001 << 8),
	.imcr		= 0x1D0,
	.imcr_set	= (0x01 << 2),
	.priv		= NULL,
	.cmod_name	= "csi20",
};

struct resource csi20_resources[] = {
	[0] = {
		.name	= "CSI20",
		.start	= 0xfeaa0000,
		.end	= 0xfeaa0fff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= intcs_evt2irq(0x17a0),
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device csi20_device = {
	.name   = "sh-mobile-csi2",
	.id     = 0,
	.num_resources	= ARRAY_SIZE(csi20_resources),
	.resource	= csi20_resources,
	.dev    = {
		.platform_data = &csi20_info,
	},
};

struct sh_csi2_client_config csi21_clients[] = {
	{
		.phy		= SH_CSI2_PHY_SUB,
		.lanes		= 1,
		.channel	= 0,
		.pdev		= &camera_devices[1],
	},
};

struct sh_csi2_pdata csi21_info = {
	.type		= SH_CSI2C,
	.clients	= csi21_clients,
	.num_clients	= ARRAY_SIZE(csi21_clients),
	.flags		= SH_CSI2_ECC | SH_CSI2_CRC,
	.ipr		= 0x44,
	.ipr_set	= (0x0001 << 0),
	.imcr		= 0x1E0,
	.imcr_set	= (0x01 << 0),
	.priv		= NULL,
	.cmod_name	= "csi21",
};

struct resource csi21_resources[] = {
	[0] = {
		.name	= "CSI21",
		.start	= 0xfeaa8000,
		.end	= 0xfeaa8fff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= intcs_evt2irq(0x1be0),
		.flags  = IORESOURCE_IRQ,
	},
};

struct platform_device csi21_device = {
	.name   = "sh-mobile-csi2",
	.id     = 1,
	.num_resources	= ARRAY_SIZE(csi21_resources),
	.resource	= csi21_resources,
	.dev    = {
		.platform_data = &csi21_info,
	},
};

