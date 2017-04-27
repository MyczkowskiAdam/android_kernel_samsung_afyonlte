/*
 * arch/arm/mach-shmobile/include/mach/setup-u2touchkey.h
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */
#ifndef __ASM_ARCH_TOUCHKEY_H
#define __ASM_ARCH_TOUCHKEY_H

void touchkey_init_hw(void);

int touchkey_i2c_register_board_info(int busnum);

#endif // __ASM_ARCH_TOUCHKEY_H
