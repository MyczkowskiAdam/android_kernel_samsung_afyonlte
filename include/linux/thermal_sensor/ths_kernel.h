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
#ifndef __THS_KERNEL_H__
#define __THS_KERNEL_H__

struct thermal_sensor_data {
	int last_mode;		/* The last operation mode of Thermal Sensor device */
	int current_mode;	/* The current operation mode of Thermal Sensor device */
};

enum mode {
	E_NORMAL_1 = 0,
	E_NORMAL_2,
	E_IDLE
};

/* Declaration of export functions. */
extern int ths_get_cur_temp(unsigned int ths_id, int *cur_temp);
extern int ths_set_op_mode(enum mode ths_mode, unsigned int ths_id);
extern int ths_get_op_mode(unsigned int ths_id);

#endif /* __THS_KERNEL_H__*/
