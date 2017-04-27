/*
 * drivers/sec_hal/toc/inc/sec_hal_toc.h
 *
 * Copyright (c) 2013, Renesas Mobile Corporation.
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

#ifndef SEC_HAL_TOC_H
#define SEC_HAL_TOC_H


/* ************************ HEADER (INCLUDE) SECTION ********************* */
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <inttypes.h>
#endif

#include "toc_data.h"

#define SECURED_DATA_IMEI                               0x69656D69 ///< \brief "imei"
#define SECURED_DATA_MAC_BASE                           0x3063616D ///< \brief "mac0"
#define SECURED_DATA_MAC(n)                             (((n)*0x01000000) + SECURED_DATA_MAC_BASE)
#define SECURED_DATA_RATS_BANDS                         0x6F636272 ///< \brief "rbco"
#define SECURED_DATA_PP_FLAGS                           0x6C667070 ///< \brief "ppfl"
#define SECURED_DATA_PRODUCT_CONFIGURATION              0x66637270 ///< \brief "prcf"
#define SECURED_DATA_PUBLIC_DEBUG                       0x62656470 ///< \brief "pdeb"
#define SECURED_DATA_GLOBAL_DEBUG                       0x62656467 ///< \brief "gdeb"
#define SECURED_DATA_SIMLOCK_STATUS                     0x74736C73 ///< \brief "slst"
#define SECURED_DATA_DEFAULT_CERT                       0x61666564 ///< \brief "defa"
#define SECURED_DATA_DRM_BUFFER_STATE                   0x73627264 ///< \brief "drbs"
#define SECURED_DATA_RANDOM                             0x646E6172 ///< \brief "rand"
#define SECURED_DATA_PUBLIC_ID                          0x64697570 ///< \brief "puid"
#define SECURED_DATA_PRCF                               0x66637270 ///< \brief "prcf"

void * toc_get_payload(DATA_TOC_ENTRY * toc_root,const uint32_t object_id, uint32_t * const p_length);

const void * toc_put_payload(DATA_TOC_ENTRY * toc_root,
                               const uint32_t     object_id,
                               const void * const data,
                               const uint32_t     length,
                               const void ** const p_handle,
                               const uint32_t     reserve,
                               const uint32_t     offset);


DATA_TOC_ENTRY * get_new_toc_entry(DATA_TOC_ENTRY * toc_root, uint32_t object_id);

void * toc_get_free_slot(DATA_TOC_ENTRY * toc_root, uint32_t size);

int toc_initialize(DATA_TOC_ENTRY * toc_root, uint32_t size);
/* ******************************** END ********************************** */

#endif /* SEC_HAL_TOC_H */
