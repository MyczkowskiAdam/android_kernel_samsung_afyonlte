/*
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

#if 0
/*
Change history:

Version:       6    12-Feb-2012     Heikki Siikaluoma
Status:        draft
Description :  MDB info container changed to SMC channel structure.
               Channel ID changed to pointer to valid SMC channel.

Version:       3    27-Dec-2011     Janne Mahosenaho
Status:        draft
Description :  Alternative TLSF implementation added.
-------------------------------------------------------------------------------
Version:       1    01-Dec-2011     Jussi Pellinen
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif


#if !defined(__GNUC__)
#ifndef __inline__
#define __inline__ inline
#endif
#endif

#ifndef SMC_MDB_H
#define SMC_MDB_H

#include "smc.h"

#define SMC_MDB_IN  0
#define SMC_MDB_OUT 1

typedef struct _smc_mdb_channel_info_t
{
    void*    pool_in;
    void*    pool_out;
    uint32_t total_size_in;
    uint32_t total_size_out;

} smc_mdb_channel_info_t;

smc_mdb_channel_info_t* smc_mdb_channel_info_create( void );

uint32_t smc_mdb_calculate_required_shared_mem( uint32_t mdb_size );
uint8_t  smc_mdb_create_pool_out( void* pool_address, uint32_t pool_size );
void     smc_mdb_info_destroy( smc_mdb_channel_info_t* smc_mdb_info );
void     smc_mdb_all_destroy( void );
void*    smc_mdb_alloc        ( smc_channel_t* smc_channel, uint32_t size );
void     smc_mdb_free         ( const smc_channel_t* smc_channel, void* ptr );
uint8_t  smc_mdb_address_check( const smc_channel_t* smc_channel, void* ptr, uint8_t direction );
void     smc_mdb_copy( void* target_ptr, void* source_ptr, uint32_t length );
uint32_t smc_mdb_channel_frag_get( smc_channel_t* smc_channel );
uint32_t smc_mdb_channel_free_space_get( smc_channel_t* smc_channel );
uint32_t smc_mdb_channel_free_space_min_get( smc_channel_t* smc_channel );
uint32_t smc_mdb_channel_largest_free_block_get( smc_channel_t* smc_channel );


#define SMC_MDB_ADDRESS_IN_POOL_IN(address, smc_mdb_info_ptr)   SMC_MDB_ADDRESS_IN_POOL( address, smc_mdb_info_ptr->pool_in,  smc_mdb_info_ptr->total_size_in )
#define SMC_MDB_ADDRESS_IN_POOL_OUT(address, smc_mdb_info_ptr)  SMC_MDB_ADDRESS_IN_POOL( address, smc_mdb_info_ptr->pool_out, smc_mdb_info_ptr->total_size_out )

#define SMC_MDB_ADDRESS_IN_POOL(address, pool, pool_size)   (((uint32_t)address >= (uint32_t)((uint8_t*)pool)) && ((uint32_t)address < ((uint32_t)((uint8_t*)pool)+pool_size)))

typedef void* tlsf_pool;

    /* Debugging. */
typedef void (*tlsf_walker)(void* ptr, size_t size, int used, void* user);
void tlsf_walk_heap(tlsf_pool pool, tlsf_walker walker, void* user);

    /* Returns nonzero if heap check fails. */
int tlsf_check_heap(tlsf_pool pool);

    /* Returns internal block size, not original request size */
size_t tlsf_block_size(void* ptr);

    /* Returns external block size, should be equal or greater
     * than the original request size */
size_t tlsf_ptr_size(void * ptr);

    /* Overhead of per-pool internal structures. */
size_t tlsf_overhead( void );

#endif

/* EOF */
