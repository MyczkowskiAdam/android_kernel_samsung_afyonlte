/*
 * system_standby_private.h
 *  standby control private header file.
 *
 * Copyright (C) 2012 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef __SYSTEM_STANDBY_PRIVATE_H__
#define __SYSTEM_STANDBY_PRIVATE_H__


#define	RT_STATUS_ACTIVE		(1)
#define	RT_STATUS_STANDBY		(2)
#define SMAP_LIB_STANDBY_INUSE	(-5)

typedef struct {
	void	*handle;
	int		status;
} system_standby_control;

typedef struct {
	void	*handle;
} system_standby_delete;

typedef struct {
	void	*handle;
} standby_handle;

#endif	/* __SYSTEM_STANDBY_PRIVATE_H__ */

