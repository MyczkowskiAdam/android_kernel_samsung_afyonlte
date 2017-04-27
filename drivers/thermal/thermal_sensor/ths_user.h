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
#ifndef __THERMAL_USER_H__
#define __THERMAL_USER_H__
  
#define THS 'T'
#define GET_CUR_TEMP	_IOWR(THS, 0x00, struct user_data)

/*Export a misc device driver as user interface */ 
extern struct miscdevice ths_user; 

struct user_data
{
	int temp; 			/* Current temperature */
	unsigned int id;	/* Thermal Sensor device id */
};


/* Define user interface. */

int ths_open(struct inode *node, struct file *fp);
int ths_close(struct inode *node, struct file *fp);
long ths_ioctl(struct file *fp, unsigned int cmd, unsigned long userdata);

#endif /* __THERMAL_USER_H__*/
