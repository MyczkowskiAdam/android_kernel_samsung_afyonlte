/*
 * arch/arm/mach-shmobile/include/mach/setup-u2csi2.h
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
#ifndef __ASM_ARCH_CSI2_H
#define __ASM_ARCH_CSI2_H

/**
 * Camera Serial Interface.
 */

extern struct platform_device csi20_device;
extern struct platform_device csi21_device;

extern struct sh_csi2_pdata csi20_info;
extern struct sh_csi2_pdata csi21_info;

extern struct resource csi20_resources[2];
extern struct resource csi21_resources[2];

#endif /* __ASM_ARCH_CSI2_H */
