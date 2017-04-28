/*
 * mfis.h
 *	 This file is MFIS driver function.
 *
 * Copyright (C) 2012-2013 Renesas Electronics Corporation
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
#ifndef __MFIS_H__
#define __MFIS_H__

int mfis_drv_suspend(void);
int mfis_drv_resume(void);
void mfis_drv_eco_suspend(void);

int mfis_drv_rel_a4rm(void);
int mfis_drv_use_a4rm(void);

#endif /* __MFIS_H__ */
