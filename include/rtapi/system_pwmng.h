/*
 * drivers/char/rtapi/include/system_pwmng.h
 *     This file is power management function.
 *
 * Copyright (C) 2011-2013 Renesas Electronics Corporation
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

#ifndef __SYSTEM_PWMNG_H__
#define __SYSTEM_PWMNG_H__

/****************************/
/* Return code				*/
/****************************/
#define SMAP_LIB_PWMNG_OK		(0)
#define SMAP_LIB_PWMNG_NG		(-1)
#define SMAP_LIB_PWMNG_PARA_NG	(-2)

/****************************/
/* Power area type			*/
/****************************/
#define	RT_PWMNG_POWERAREA_A4LC	(1)

/****************************/
/* Struction				*/
/****************************/
typedef struct {
	void			*handle;
	unsigned long	powerarea_name;
} system_pmg_param;

typedef struct {
	void			*handle;
} system_pmg_delete;

/****************************/
/* Prototype Declaration	*/
/****************************/
extern void *system_pwmng_new(void);
extern int system_pwmng_powerarea_start_notify(system_pmg_param *pwmng_param);
extern int system_pwmng_powerarea_end_notify(system_pmg_param *pwmng_param);
extern void system_pwmng_delete(system_pmg_delete *pwmng_delete);

#endif  /* __SYSTEM_PWMNG_H__ */

