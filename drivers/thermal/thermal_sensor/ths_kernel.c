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
 
#include <linux/module.h>
#include <linux/thermal_sensor/ths_kernel.h>

#include "ths_main.h"


/* Implement Kernel interface */

/*
 * ths_get_cur_temp: get the current temperature of LSI
 *  @ths_id  : index of Thermal Sensor device (THS0 or THS1)
 *  @cur_temp: This value shows that the actual LSI temperature is
			in range of [cur_temp-5, cur_temp]
			E.g: cur_temp is 45. It means the current temperature is
			in range from 40 to 45 degree.
 * return: 
 * 		-EINVAL (-22): invalid argument
 *		-ENXIO   (-6): Thermal Sensor device is IDLE state
 *		-EACCES (-13): Permission denied due to driver in suspend state
 *		0			 : Get current temperature successfully
 */
 
int ths_get_cur_temp(unsigned int ths_id, int *cur_temp)
{
	return __ths_get_cur_temp(ths_id, cur_temp);
}
EXPORT_SYMBOL_GPL(ths_get_cur_temp);

/*
 * ths_set_op_mode: set operation mode of Thermal Sensor device
 *  @ths_mode: operation mode of Thermal Sensor device
 *  @ths_id  : index of Thermal Sensor device (THS0 or THS1)
 * return: 
 *	0			  : set new operation mode successfully
 *	-EINVAL (-22) : invalid argument
 */
 
int ths_set_op_mode(enum mode ths_mode, unsigned int ths_id)
{
	return __ths_set_op_mode(ths_mode, ths_id);
}
EXPORT_SYMBOL_GPL(ths_set_op_mode);

/*
 * ths_get_op_mode: get the current operation mode of Thermal Sensor device.
 *  	@ths_id: index of Thermal Sensor device (THS0 or THS1)
 * return: 
 *		E_NORMAL_1 (0): is equivalent to NORMAL 1 operation.
 *		E_NORMAL_2 (1): is equivalent to NORMAL 2 operation.
 *		E_IDLE 	   (2): is equivalent to IDLE operation.
 *		-EINVAL  (-22): invalid argument (ths_id is different from 0 and 1)
 */
 
int ths_get_op_mode(unsigned int ths_id)
{
	return __ths_get_op_mode(ths_id);
}
EXPORT_SYMBOL_GPL(ths_get_op_mode);
