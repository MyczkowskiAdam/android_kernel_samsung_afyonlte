/*
 * arch/arm/mach-shmobile/include/mach/setup-u2rcu.h
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
#ifndef __ASM_ARCH_RCU_H
#define __ASM_ARCH_RCU_H

extern struct platform_device rcu0_device;
extern struct platform_device rcu1_device;

extern struct sh_mobile_rcu_companion csi20;
extern struct sh_mobile_rcu_companion csi21;

extern struct resource rcu0_resources[];
extern struct resource rcu1_resources[];


extern struct sh_mobile_rcu_info sh_mobile_rcu0_info;
extern struct sh_mobile_rcu_info sh_mobile_rcu1_info;

#endif /* __ASM_ARCH_RCU_H */
