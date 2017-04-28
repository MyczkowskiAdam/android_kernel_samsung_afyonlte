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
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <asm-generic/uaccess.h>

#include "ths_main.h"
#include "ths_user.h"

/*
 * Define the operation of a misc device driver
 */
 
static const struct file_operations user_fops = {
	.owner   		= THIS_MODULE,
	.open    		= ths_open,
	.release 		= ths_close,
	.unlocked_ioctl = ths_ioctl,
};

/*
 * Define a misc device driver as user interface
 */

struct miscdevice ths_user = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "ths_user",
	.fops  = &user_fops,
};

/* Implement user interface */

/*
 * ths_open: open Thermal Sensor driver
 *  @node: represents a file in the system
 *  @fp  : created every time a file is opened
 * return: 
 *		0           : Open successfully
 */
 
int ths_open(struct inode *node, struct file *fp)
{
	THS_DEBUG_MSG(">>> %s start\n", __func__);
	
	THS_DEBUG_MSG("%s end <<<\n", __func__);
	
	return 0;
}

/*
 * ths_close: close Thermal Sensor driver
 *  @node: represents a file in the system
 *  @fp  : created every time a file is opened.
 * return: 
 *		0 : Close successfully
 */
 
int ths_close(struct inode *node, struct file *fp)
{
	THS_DEBUG_MSG(">>> %s start\n", __func__);
	
	THS_DEBUG_MSG("%s end <<<\n", __func__);
	
	return 0;
}

/*
 * ths_ioctl: get the current temperature of LSI from user-space
 *  @node  	  : represents a file in the system
 *  @fp    	  : created every time a file is opened.
 *  @cmd   	  : command to get current temperature of LSI
 *  @userdata : store address of user data (temperature and Thermal Sensor device id)
 * return: 
 *		-EINVAL (-22): invalid argument
 *		-ENXIO   (-6): Thermal Sensor device is IDLE state
 *		-EFAULT (-14): Can't exchange data between kernel and user side
 *		-EACCES (-13): Permission denied due to driver in suspend state
 *		0			 : Get current temperature successfully
 */
 
long ths_ioctl(struct file *fp, unsigned int cmd, unsigned long userdata)
{
	int ret = 0;
	struct user_data data;
	void __user *p = (void __user*)userdata;
	
	THS_DEBUG_MSG(">>> %s start\n", __func__);
	
	if (GET_CUR_TEMP != cmd) {
		THS_ERROR_MSG("%s: invalid command\n",__func__);
		ret = -EINVAL;
		goto ths_error;
	}
	
	if (copy_from_user(&data, p, sizeof(data))) {
		THS_ERROR_MSG("%s: can't copy data from user to kernel side\n",__func__);
		ret = -EFAULT;
		goto ths_error;
	}
	
	ret = __ths_get_cur_temp(data.id, &data.temp);
	
	if (0 == ret) {
		if (copy_to_user(p, &data, sizeof(data))) {
			THS_ERROR_MSG("%s: can't copy data from kernel to user side\n",__func__);
			ret = -EFAULT;
			goto ths_error;
		}
	}

ths_error:	

	THS_DEBUG_MSG("%s end <<<\n", __func__);	

	return ret;
}

