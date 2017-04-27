/*
 * Thermal Sensor Driver
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

#include <linux/io.h>

#include "ths_main.h"
#include "ths_hardware.h"


/* Implement Hardware control part */

/*
 * modify_register_32: modify value of a 32-bit register
 *  @offset      : the offset address of a 32-bit register
 *  @set_value   : value is set to a 32-bit register
 *  @clear_mask  : clear the value of some bits in a 32-bit register
 * return: 
 *		None
 */
 
void modify_register_32(int offset, u32 set_value, u32 clear_mask)
{
	u32 value;
	value = ioread32(ths->iomem_base + offset);
	value &= ~clear_mask;
	value |= set_value;
	iowrite32(value, ths->iomem_base + offset);
}
 
/*
 * set_register_32: set value to a 32-bit register
 *  @offset : the offset address of a 32-bit register
 *  @value: value is set to a 32-bit register
 * return: 
 *		None
 */
 
void set_register_32(int offset, u32 value)
{
	iowrite32(value, ths->iomem_base + offset);
}

/*
 * get_register_32: get value from a register
 *  @offset: the offset address of a register
 * return: 
 *		the value of a 32-bit register
 */
 
u32 get_register_32(int offset)
{
	return  ioread32(ths->iomem_base + offset);
}
