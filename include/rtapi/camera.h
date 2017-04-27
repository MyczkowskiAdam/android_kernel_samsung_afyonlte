/*
 * camera.h
 *     This file is camera function.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
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

#ifndef __CAMERA_H__
#define __CAMERA_H__

/********************************/
/* Return code			*/
/********************************/
#define SMAP_LIB_CAMERA_OK		(0)
#define SMAP_LIB_CAMERA_NG		(-1)
#define SMAP_LIB_CAMERA_PARA_NG		(-2)

/********************************/
/* Struction			*/
/********************************/
struct camera_prm_param {
	void			*handle;
};

struct camera_prm_delete {
	void			*handle;
};

/********************************/
/* Prototype Declaration	*/
/********************************/
extern void *camera_new(void);
extern int camera_start_notify(struct camera_prm_param *camera_param);
extern int camera_end_notify(struct camera_prm_param *camera_param);
extern void camera_delete(struct camera_prm_delete *camera_delete);

#endif  /* __CAMERA_H__ */

