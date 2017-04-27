/*
 * arch/arm/mach-shmobile/include/mach/sh_cmt.h
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
#ifndef __ASM_ARCH_SH_CMT_H
#define __ASM_ARCH_SH_CMT_H

#ifdef ARCH_HAS_READ_CURRENT_TIMER
extern spinlock_t	sh_cmt_lock; /* arch/arm/mach-shmobile/sh_cmt.c */
#endif

#endif /* __ASM_ARCH_SH_CMT_H*/
