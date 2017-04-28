/*
 * drivers/sec_hal/rt/inc/sec_hal_tee.h
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
#ifndef SEC_HAL_TEE_H
#define SEC_HAL_TEE_H


/* ************************ HEADER (INCLUDE) SECTION ********************* */
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <inttypes.h>
#endif
#include "sec_hal_res.h"
#include "tee_defs.h"

/* ***************** MACROS, CONSTANTS, COMPILATION FLAGS **************** */

uint32_t sec_hal_tee_initialize_context( const char* name,
                                         TEEC_Context* context);

uint32_t sec_hal_tee_finalize_context( TEEC_Context* context);

uint32_t sec_hal_tee_open_session( TEEC_Context* context,
                          TEEC_Session* session,
                          const TEEC_UUID* destination,
                          uint32_t connectionMethod,
                          const void* connectionData,
                          TEEC_Operation* operation,
                          uint32_t* returnOrigin);

uint32_t  sec_hal_tee_close_session( TEEC_Session* session );

uint32_t sec_hal_tee_invoke_command( TEEC_Session* session,
                        uint32_t commandID,
                        TEEC_Operation* operation,
                        uint32_t* returnOrigin);

uint32_t sec_hal_tee_get_param_type(uint32_t index, uint32_t types);

void  sec_hal_tee_convert_memrefs( TEEC_Operation* user_operation,
                                       TEEC_Operation* operation,
                                       uint32_t direction );

uint32_t sec_hal_tee_register_shared_memory_area( TEEC_Context* kernel_context, TEEC_SharedMemory* kernel_shmem);

uint32_t sec_hal_tee_release_shared_memory_area(TEEC_SharedMemory* kernel_shmem);


/* ******************************** END ********************************** */

#endif /* SEC_HAL_TEE_H */

