/*
 * rmu2_rwdt.h
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

#ifndef _LINUX_RWDT_H
#define _LINUX_RWDT_H

#include <linux/ioctl.h>

/* define macro declaration for IOCTL command numbers */
#define RWDT_MAGIC	'r'
#define IOCTL_RWDT_SOFT_RESET	_IO(RWDT_MAGIC, 1)

#ifdef CONFIG_RMU2_RWDT
int rmu2_rwdt_cntclear(void);
int rmu2_rwdt_stop(void);
#else
static inline int rmu2_rwdt_cntclear(void)
{
	return 0;
}
static inline int rmu2_rwdt_stop(void)
{
	return 0;
}
#endif
void rmu2_rwdt_software_reset(void);

#endif  /* _LINUX_RWDT_H */

/* End of File */

