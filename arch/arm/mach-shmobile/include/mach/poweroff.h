/*
 * arch/arm/mach-shmobile/include/mach/poweroff.h
 *
 * Copyright 2013 Renesas Mobile Corporation
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef __ASM_ARCH_POWEROFF_H
#define __ASM_ARCH_POWEROFF_H

void shmobile_do_restart(char mode, const char *cmd, u32 debug_mode);

#endif /* __ASM_ARCH_POWEROFF_H */
