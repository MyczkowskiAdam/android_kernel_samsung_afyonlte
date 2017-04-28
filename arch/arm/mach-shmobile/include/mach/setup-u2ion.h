/*
 * arch/arm/mach-shmobile/include/mach/setup-u2ion.h
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
#ifndef __ASM_ARCH_BOARD_U2EVM_ION_H
#define __ASM_ARCH_BOARD_U2EVM_ION_H

extern struct ion_platform_data u2evm_ion_data;
extern struct platform_device u2evm_ion_device;

int u2evm_ion_adjust(void);

#endif /* __ASM_ARCH_BOARD_U2EVM_ION_H */
