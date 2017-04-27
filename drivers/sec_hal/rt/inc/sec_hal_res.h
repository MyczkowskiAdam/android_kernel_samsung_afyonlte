/*
 * drivers/sec_hal/rt/inc/sec_hal_res.h
 *
 * Copyright (c) 2012-2013, Renesas Mobile Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef SEC_HAL_RES_H
#define SEC_HAL_RES_H

/* Operation completed successfully */
#define SEC_HAL_RES_OK                        0x00000000

/* Operation failed */
#define SEC_HAL_RES_FAIL                      0x00000010

/* Incorrect parameters given to the function */
#define SEC_HAL_RES_PARAM_ERROR               0x00000020

/* Requested functionality is not, yet, implemented. */
#define SEC_HAL_RES_NOT_IMPL                  0x00000040

/* Requested service needs more data to complete the request */
#define SEC_HAL_RES_NEED_DATA                 0x00000080

/* ******************************** END ********************************** */

#endif /* SEC_HAL_RES_H */

