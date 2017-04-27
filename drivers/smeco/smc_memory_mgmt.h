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

Version:       6    03-Feb-2012     Jussi Pellinen
Status:        draft
Description:   Offset type SMC_SHM_OFFSET_MDB_OFFSET added

Version:       5    02-Dec-2011     Heikki Siikaluoma
Status:        draft
Description:   Code clean up

Version:       1    19-Oct-2011     Heikki Siikaluoma
Status:        draft
Description:   File created
-------------------------------------------------------------------------------
*/
#endif


#ifndef SMC_MEMORY_MGMT_H
#define SMC_MEMORY_MGMT_H

    /* Remote SHM memory offset policy */
typedef enum
{
    SMC_SHM_OFFSET_EQUAL = 0x00,
    SMC_SHM_OFFSET_ADD,
    SMC_SHM_OFFSET_SUBTRACT,
    SMC_SHM_OFFSET_MDB_OFFSET
} SMC_SHM_OFFSET_TYPE;

typedef struct
{
    uint8_t*            shm_area_start_address;            /* Start address of the SHM */
    uint32_t            size;                              /* Size of SHM in bytes */

    uint8_t             use_cache_control;                 /* If TRUE the user is responsible of cache clean/invalidate */
    uint8_t             cache_line_len;                    /* Length of cache line in bytes */
    uint8_t             reserved;                          /* Fill */
    SMC_SHM_OFFSET_TYPE remote_cpu_memory_offset_type;     /* SHM memory offset policy of the remote CPU. Uses constants SMC_SHM_OFFSET_xxx */

    uint32_t            remote_cpu_memory_offset;          /* Offset to remote CPU SHM physical address */

} smc_shm_config_t;


    /*
     * Shared memory API functions
     */

smc_shm_config_t* smc_shm_conf_create ( void );
void              smc_shm_conf_destroy( smc_shm_config_t* smc_shm_conf);

smc_shm_config_t* smc_shm_config_copy ( smc_shm_config_t* source_shm_config );

    /*
     * Platform specific shared memory API functions
     * Implementation is in the directory
     * - /modem
     * - /linux
     */
/* Cache controls are changed to macros
 * TODO Cleanup
void smc_shm_cache_invalidate(void* start_address, void* end_address);
void smc_shm_cache_clean     (void* start_address, void* end_address);
*/

/* Remove these */
#define smc_shm_cache_invalidate  SMC_SHM_CACHE_INVALIDATE
#define smc_shm_cache_clean  SMC_SHM_CACHE_CLEAN

#endif /* EOF */

