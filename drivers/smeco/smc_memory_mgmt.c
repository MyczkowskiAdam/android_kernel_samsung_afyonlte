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

Version:       5    02-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  Code clean up

Version:       3    08-Nov-2011     Heikki Siikaluoma
Status:        draft
Description :  Platform independent code implemented

Version:       1    19-Oct-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#include "smc_common_includes.h"
#include "smc_trace.h"
#include "smc_fifo.h"
#include "smc.h"
#include "smc_memory_mgmt.h"

smc_shm_config_t* smc_shm_conf_create( void )
{
    smc_shm_config_t* config = (smc_shm_config_t*)SMC_MALLOC( sizeof(smc_shm_config_t) );

    assert( config != NULL );

    config->shm_area_start_address        = NULL;
    config->size                          = 0;
    config->use_cache_control             = FALSE;
    config->cache_line_len                = 0;
    config->remote_cpu_memory_offset_type = SMC_SHM_OFFSET_EQUAL;
    config->remote_cpu_memory_offset      = 0;

    SMC_TRACE_PRINTF_DEBUG("smc_shm_conf_create: 0x%08X", (uint32_t)config);

    return config;
}

void smc_shm_conf_destroy( smc_shm_config_t* smc_shm_conf)
{
    if( smc_shm_conf )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_shm_conf_destroy: 0x%08X", (uint32_t)smc_shm_conf);
        SMC_FREE( smc_shm_conf );
        smc_shm_conf = NULL;
    }
}

/*
 * Creates new SHM configuration with values copied from source SHM config.
 */
smc_shm_config_t* smc_shm_config_copy( smc_shm_config_t* source_shm_config )
{
    smc_shm_config_t* config = smc_shm_conf_create();

    config->shm_area_start_address        = source_shm_config->shm_area_start_address;
    config->size                          = source_shm_config->size;
    config->use_cache_control             = source_shm_config->use_cache_control;
    config->cache_line_len                = source_shm_config->cache_line_len;
    config->remote_cpu_memory_offset_type = source_shm_config->remote_cpu_memory_offset_type;
    config->remote_cpu_memory_offset      = source_shm_config->remote_cpu_memory_offset;


    return config;
}

/* EOF */
