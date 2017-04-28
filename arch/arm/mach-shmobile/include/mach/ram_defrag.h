/*
 * arch/arm/mach-shmobile/include/mach/ram_defrag.h
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

/* API of RAM defragmentation (offers to user) */
#ifndef __ARCH_MACH_RAM_DEFRAG_H__
#define __ARCH_MACH_RAM_DEFRAG_H__

#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>

extern struct page *mem_map;

#ifdef CONFIG_SHMOBILE_RAM_DEFRAG

int defrag(void);
unsigned int get_ram_banks_status(void);

#else /* !CONFIG_SHMOBILE_RAM_DEFRAG */

static inline int defrag(void)
{
	return -ENOTSUPP;
}

static inline unsigned int get_ram_banks_status(void)
{
	return -ENOTSUPP;
}

#endif /*CONFIG_SHMOBILE_RAM_DEFRAG*/

#endif /*__ARCH_MACH_RAM_DEFRAG_H__*/
