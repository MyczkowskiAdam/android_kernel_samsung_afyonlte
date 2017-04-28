/*
 * drivers/char/rtapi/include/system_standby.h
 *     This file is RT-domain standby function.
 *
 * Copyright (C) 2011-2012 Renesas Electronics Corporation
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

#ifndef __SYSTEM_STANDBY_H__
#define __SYSTEM_STANDBY_H__


/* return code */
#define SMAP_LIB_STANDBY_OK		(0)
#define SMAP_LIB_STANDBY_NG		(-1)


extern int system_rt_standby
(
	void
);


extern int system_rt_active
(
	void
);


#endif  /* __SYSTEM_STANDBY_H__ */

