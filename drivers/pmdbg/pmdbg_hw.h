/*
 * pmdbg_hw.h
 *
 * Copyright (C) 2011-2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __TST_PM_HW___
#define __TST_PM_HW___

#include <linux/io.h>

#define rreg8(addr)	ioread8(addr)
#define rreg16(addr)	ioread16(addr)
#define rreg32(addr)	ioread32(addr)
#define rreg(addr)		rreg32(addr)


#define wreg8(addr, val)	iowrite8(val, addr)
#define wreg16(addr, val)	iowrite16(val, addr)
#define wreg32(addr, val)	iowrite32(val, addr)
#define wreg(addr, val)		wreg32(val, addr)


extern void mreg8(void __iomem *addr, u8 set, u8 clear);
extern void mreg16(void __iomem *addr, u16 set, u16 clear);
extern void mreg32(void __iomem *addr, u32 set, u32 clear);

#define mreg(addr, set, clear) mreg32(addr, set, clear)

#ifndef CHIP_VERSION_MASK
#define CHIP_VERSION_MASK	0x0000FFFF
#endif /*CHIP_VERSION_MASK*/

#ifndef CHIP_VERSION_ES2_0
#define CHIP_VERSION_ES2_0	0x00003E10
#endif /*CHIP_VERSION_ES2_0*/

#ifndef CHIP_VERSION_ES2_1
#define CHIP_VERSION_ES2_1	0x00003E11
#endif /*CHIP_VERSION_ES2_1*/

#ifndef CHIP_VERSION_ES2_2
#define CHIP_VERSION_ES2_2	0x00003E12
#endif /*CHIP_VERSION_ES2_2*/

#ifndef CHIP_VERSION_ES2_3
#define CHIP_VERSION_ES2_3	0x00003E13
#endif /*CHIP_VERSION_ES2_3*/

#endif /*__TST_PM_HW___*/
