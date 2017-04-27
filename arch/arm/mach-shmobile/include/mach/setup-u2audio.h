/*
 * arch/arm/mach-shmobile/include/mach/setup-u2audio.h
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
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
#ifndef __ASM_ARCH_SETUP_U2AUDIO_H
#define __ASM_ARCH_SETUP_U2AUDIO_H

#define	DEVICE_NONE		0
#define	DEVICE_EXIST	1

void u2audio_init(unsigned int u2_board_rev);
void u2vcd_reserve(void);

#endif /* __ASM_ARCH_SETUP_U2AUDIO_H */
