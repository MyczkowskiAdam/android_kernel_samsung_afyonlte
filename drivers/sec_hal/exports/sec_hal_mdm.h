/*
 * drivers/sec_hal/exports/sec_hal_mdm.h
 *
 * Copyright (c) 2012-2013, Renesas Mobile Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef SEC_HAL_MDM_H
#define SEC_HAL_MDM_H


/* **************************************************************************
** return values given by following functions.
** *************************************************************************/
#define SEC_HAL_MDM_RES_OK           0x00000000UL
#define SEC_HAL_MDM_RES_FAIL         0x00000010UL
#define SEC_HAL_MDM_RES_PARAM_ERROR  0x00000020UL


/* **************************************************************************
** Function name      : sec_hal_mdm_memcpy
** Description        : copy data to a protected memory.
** Parameters         : IN/--- uint32_t phys_dst, physical address where to
                        copy.
**                      IN/--- uint32_t phys_src, physical address of
                        to-be-copied data.
**                                             continuous mem block expected.
**                      IN/--- uint32_t size, size of the to-be-copied data.
** Return value       : uint32
**                      == SEC_HAL_MDM_RES_OK if operation successful
**                      == SEC_HAL_MDM_RES_FAIL general failure indication.
**                      == SEC_HAL_MDM_RES_PARAM_ERROR given inputs
                        were faulty.
** *************************************************************************/
uint32_t sec_hal_mdm_memcpy(uint32_t phys_dst, uint32_t phys_src,
                            uint32_t size);

/* **************************************************************************
** Function name      : sec_hal_mdm_authenticate
** Description        : authenticate memory content with given SW CERT.
** Parameters         : IN/--- uint32_t phys_addr, physical address of SW
                        CERT.
**                      IN/--- uint32_t size, size of the SW CERT.
** Return value       : uint32
**                      == SEC_HAL_MDM_RES_OK if operation successful
**                      == SEC_HAL_MDM_RES_FAIL general failure indication.
**                      == SEC_HAL_MDM_RES_PARAM_ERROR given inputs
                        were faulty.
** *************************************************************************/
uint32_t sec_hal_mdm_authenticate(uint32_t phys_addr, uint32_t size,
                                  uint32_t *objid);


#endif /* SEC_HAL_MDM_H */
