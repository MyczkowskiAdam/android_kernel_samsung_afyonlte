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

Version:       5    19-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  Code clean up, Linux Kernel compile warnings fixed

Version:       2    08-Nov-2011     Heikki Siikaluoma
Status:        draft
Description :  Platform independent code implemented

Version:       1    19-Oct-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#include "smc_common_includes.h"

#include "smc_conf.h"
#include "smc.h"
#include "smc_trace.h"

/*
 * Creates empty SMC configuration for one SMC instance.
 */
smc_conf_t* smc_conf_create( void )
{
    smc_conf_t* conf = (smc_conf_t*)SMC_MALLOC( sizeof(smc_conf_t) );

        /*
         * Initialize empty values
         */
    conf->cpu_id_remote              = 0;
    conf->cpu_id_local               = 0;
    conf->is_master                  = FALSE;
    conf->smc_channel_conf_count     = 0;
    conf->smc_channel_conf_ptr_array = NULL;
    conf->smc_shm_conf               = NULL;
    conf->initialization_flags       = SMC_INIT_FLAGS_NONE;

    return conf;
}

/*
 * Creates empty SMC Channel configuration.
 */
smc_channel_conf_t* smc_channel_conf_create( void )
{
    smc_channel_conf_t* conf = (smc_channel_conf_t*)SMC_MALLOC( sizeof(smc_channel_conf_t) );

    assert( conf != NULL );

        /*
         * Initialize empty values
         */
    conf->channel_id                    = 0;
    conf->priority                      = SMC_CHANNEL_PRIORITY_DEFAULT;
    conf->copy_scheme                   = SMC_COPY_SCHEME_COPY_IN_SEND + SMC_COPY_SCHEME_COPY_IN_RECEIVE;
    conf->wake_lock_flags               = SMC_CHANNEL_WAKELOCK_NONE;
    conf->protocol                      = 0;

    conf->fifo_size_in                  = 0;
    conf->fifo_size_out                 = 0;
    conf->mdb_size_in                   = 0;
    conf->mdb_size_out                  = 0;

    conf->signal_remote                 = NULL;
    conf->signal_local                  = NULL;

    conf->smc_receive_data_cb           = NULL;
    conf->smc_receive_data_allocator_cb = NULL;
    conf->smc_send_data_deallocator_cb  = NULL;
    conf->smc_event_cb                  = NULL;

    conf->fifo_full_check_timeout_usec  = 0;
    conf->rx_mem_realloc_check_timeout_usec = 0;

    conf->trace_features                = 0x00;
    conf->wakelock_timeout_ms           = 0;
    conf->history_data_max              = 0;

    return conf;
}

smc_channel_runtime_fixed_conf_t*  smc_channel_runtime_fixes_conf_create( void )
{
    smc_channel_runtime_fixed_conf_t* conf = (smc_channel_runtime_fixed_conf_t*)SMC_MALLOC_IRQ( sizeof(smc_channel_runtime_fixed_conf_t) );

    assert( conf != NULL );

    conf->channel_id        = 0;
    conf->fifo_in_size      = 0;
    conf->fifo_out_size     = 0;
    conf->shm_start_address = 0;
    conf->shm_size          = 0;
    conf->mdb_in_size       = 0;
    conf->mdb_out_size      = 0;

    return conf;
}

/*
 * Adds new channel configuration to the SMC instance configuration.
 */
void smc_conf_add_channel_conf(smc_conf_t* smc_conf, smc_channel_conf_t* smc_channel_conf)
{
    smc_channel_conf_t** old_conf_array = NULL;

    smc_conf->smc_channel_conf_count++;

    if( smc_conf->smc_channel_conf_ptr_array )
    {
        SMC_TRACE_PRINTF_INFO("smc_conf_add_channel_conf: Store old array pointer 0x%08X (array len %d) from conf 0x%08X...",
                (uint32_t)smc_conf->smc_channel_conf_ptr_array, (smc_conf->smc_channel_conf_count-1), (uint32_t)smc_conf);
        old_conf_array = smc_conf->smc_channel_conf_ptr_array;
    }

    smc_conf->smc_channel_conf_ptr_array = (smc_channel_conf_t**)SMC_MALLOC( sizeof(smc_channel_conf_t*)*smc_conf->smc_channel_conf_count );

    assert( smc_conf->smc_channel_conf_ptr_array != NULL );

    SMC_TRACE_PRINTF_INFO("smc_conf_add_channel_conf: New conf ptr in 0x%08X", (uint32_t)smc_conf->smc_channel_conf_ptr_array);

    if( old_conf_array )
    {
        SMC_TRACE_PRINTF_INFO("smc_conf_add_channel_conf: Restoring old array pointer 0x%08X data...", (uint32_t)old_conf_array);

        for(int i = 0; i < smc_conf->smc_channel_conf_count-1; i++)
        {
            smc_conf->smc_channel_conf_ptr_array[i] = old_conf_array[i];
        }

        SMC_TRACE_PRINTF_INFO("smc_conf_add_channel_conf: Put new conf into index %d", (smc_conf->smc_channel_conf_count-1));

        smc_conf->smc_channel_conf_ptr_array[smc_conf->smc_channel_conf_count-1] = smc_channel_conf;

        SMC_TRACE_PRINTF_INFO("smc_conf_add_channel_conf: free old conf array 0x%08X...", (uint32_t)old_conf_array);
        SMC_FREE( old_conf_array );
        old_conf_array = NULL;
    }
    else
    {
        SMC_TRACE_PRINTF_INFO("smc_conf_add_channel_conf: First channel conf ptr 0x%08X added to array pointer 0x%08X",
                (uint32_t)smc_channel_conf, (uint32_t)smc_conf->smc_channel_conf_ptr_array);

        smc_conf->smc_channel_conf_ptr_array[0] = smc_channel_conf;
    }

    SMC_TRACE_PRINTF_INFO("smc_conf_add_channel_conf: completed, channel count is %d", smc_conf->smc_channel_conf_count);
}

void smc_conf_destroy( smc_conf_t* smc_conf )
{
    if( smc_conf != NULL )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_conf_destroy: 0x%08X", (uint32_t)smc_conf );

        for( int i = 0; i < smc_conf->smc_channel_conf_count; i++ )
        {
            smc_channel_conf_destroy( smc_conf->smc_channel_conf_ptr_array[i] );
        }

        SMC_FREE( smc_conf->smc_channel_conf_ptr_array );

        smc_conf->smc_channel_conf_ptr_array = NULL;

        smc_shm_conf_destroy( smc_conf->smc_shm_conf );
        smc_conf->smc_shm_conf = NULL;

        SMC_FREE( smc_conf ) ;
        smc_conf = NULL;
    }
}

void smc_channel_conf_destroy( smc_channel_conf_t* smc_channel_conf)
{
    if( smc_channel_conf != NULL )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_channel_conf_destroy: 0x%08X", (uint32_t)smc_channel_conf );

        smc_signal_destroy( smc_channel_conf->signal_remote );
        smc_channel_conf->signal_remote = NULL;

        smc_signal_destroy( smc_channel_conf->signal_local );
        smc_channel_conf->signal_local = NULL;

        SMC_FREE( smc_channel_conf ) ;
        smc_channel_conf = NULL;
    }
}

/**
 * Creates SMC configuration from the common SMC instance configuration.
 * The parameter smc_cpu_name is either master or slave name in the instance config.
 */
smc_conf_t* smc_conf_create_from_instance_conf( char* smc_cpu_name, smc_instance_conf_t* smc_instance_conf )
{
    smc_conf_t*       smc_conf     = NULL;
    smc_shm_config_t* smc_shm_conf = NULL;


    SMC_TRACE_PRINTF_DEBUG("smc_conf_create_from_instance_conf: Creating for %s from 0x%08X: %s->%s: %s <-> %s", smc_cpu_name,
            (uint32_t)smc_instance_conf, smc_instance_conf->user_name,
            smc_instance_conf->name, smc_instance_conf->master_name, smc_instance_conf->slave_name );

    smc_conf = smc_conf_create();

        /* Solve the master/slave by the CPU name */
    smc_conf->is_master = (strcmp( smc_instance_conf->master_name , smc_cpu_name ) == 0);

    SMC_TRACE_PRINTF_DEBUG("smc_conf_create_from_instance_conf: CPU is %s",  (smc_conf->is_master?"MASTER":"SLAVE"));

        /* Create the SHM config */
    smc_shm_conf = smc_shm_conf_create();

    smc_shm_conf->shm_area_start_address = (uint8_t*)smc_instance_conf->shm_start_address;
    smc_shm_conf->size                   = smc_instance_conf->shm_size;
    smc_conf->name                       = smc_instance_conf->user_name;

    if( smc_conf->is_master )
    {
        smc_conf->initialization_flags              = smc_instance_conf->initialization_flags_master;

        smc_shm_conf->use_cache_control             = smc_instance_conf->shm_use_cache_control_master;
        smc_shm_conf->cache_line_len                = smc_instance_conf->shm_cache_line_len_master;
        smc_shm_conf->remote_cpu_memory_offset_type = smc_instance_conf->shm_memory_offset_type_master_to_slave;
    }
    else
    {
        smc_conf->initialization_flags              = smc_instance_conf->initialization_flags_slave;

        smc_shm_conf->use_cache_control             = smc_instance_conf->shm_use_cache_control_slave;
        smc_shm_conf->cache_line_len                = smc_instance_conf->shm_cache_line_len_slave;

            /* The remote side offset type is opposite or equal */
        if( smc_instance_conf->shm_memory_offset_type_master_to_slave == SMC_SHM_OFFSET_MDB_OFFSET )
        {
            smc_shm_conf->remote_cpu_memory_offset_type = SMC_SHM_OFFSET_MDB_OFFSET;
        }
        else if( smc_instance_conf->shm_memory_offset_type_master_to_slave == SMC_SHM_OFFSET_EQUAL )
        {
            smc_shm_conf->remote_cpu_memory_offset_type = SMC_SHM_OFFSET_EQUAL;
        }
        else if( smc_instance_conf->shm_memory_offset_type_master_to_slave == SMC_SHM_OFFSET_ADD )
        {
            smc_shm_conf->remote_cpu_memory_offset_type = SMC_SHM_OFFSET_SUBTRACT;
        }
        else if( smc_instance_conf->shm_memory_offset_type_master_to_slave == SMC_SHM_OFFSET_SUBTRACT )
        {
            smc_shm_conf->remote_cpu_memory_offset_type = SMC_SHM_OFFSET_ADD;
        }
        else
        {
            SMC_TRACE_PRINTF_WARNING("smc_conf_create_from_instance_conf: unsupported offset type 0x%02X in master to slave configuration", smc_instance_conf->shm_memory_offset_type_master_to_slave);
        }
    }

    smc_shm_conf->remote_cpu_memory_offset = smc_instance_conf->shm_cpu_memory_offset;

    smc_conf->smc_shm_conf = smc_shm_conf;

        /* Create the channel config */
    if( smc_instance_conf->channel_config_count > 0 )
    {
        int i = 0;

        SMC_TRACE_PRINTF_DEBUG("smc_conf_create_from_instance_conf: Creating %d channel configs...", smc_instance_conf->channel_config_count);

        for( i = 0; i < smc_instance_conf->channel_config_count; i++ )
        {
            smc_channel_conf_t* smc_channel_conf = smc_channel_conf_create_from_instance_conf( &smc_instance_conf->channel_config_array[i], smc_conf->is_master );

            if( smc_channel_conf != NULL )
            {
                smc_conf_add_channel_conf( smc_conf, smc_channel_conf );
            }
            else
            {
                SMC_TRACE_PRINTF_WARNING("smc_conf_create_from_instance_conf: SMC channel instance conf in index %d creation failed", i);
            }
        }
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("smc_conf_create_from_instance_conf: SMC instance conf does not contain channels");
    }

    SMC_TRACE_PRINTF_DEBUG("smc_conf_create_from_instance_conf: returning SMC conf 0x%08X", (uint32_t)smc_conf);

    return smc_conf;
}

smc_channel_conf_t* smc_channel_conf_create_from_instance_conf( smc_instance_conf_channel_t* smc_instance_conf_channel, uint8_t is_master )
{
    smc_channel_conf_t* smc_channel_conf = NULL;

    if( smc_instance_conf_channel != NULL )
    {
        smc_channel_conf = smc_channel_conf_create();

        if( is_master )
        {
            SMC_TRACE_PRINTF_DEBUG("smc_channel_conf_create_from_instance_conf: MASTER configuration used");

            smc_channel_conf->copy_scheme   = smc_instance_conf_channel->copy_scheme_master;

            smc_channel_conf->fifo_size_in  = smc_instance_conf_channel->fifo_size_slave;
            smc_channel_conf->fifo_size_out = smc_instance_conf_channel->fifo_size_master;

            smc_channel_conf->mdb_size_in   = smc_instance_conf_channel->mdb_size_slave;
            smc_channel_conf->mdb_size_out  = smc_instance_conf_channel->mdb_size_master;

            smc_channel_conf->signal_remote = smc_signal_create( smc_instance_conf_channel->signal_id_master_to_slave, smc_instance_conf_channel->signal_type_master_to_slave );
            smc_channel_conf->signal_local  = smc_signal_create( smc_instance_conf_channel->signal_id_master_from_slave, smc_instance_conf_channel->signal_type_master_from_slave );

            smc_channel_conf->fifo_full_check_timeout_usec = smc_instance_conf_channel->fifo_full_check_timeout_usec_master;

            smc_channel_conf->rx_mem_realloc_check_timeout_usec = smc_instance_conf_channel->rx_mem_realloc_check_timeout_usec_master;

            smc_channel_conf->trace_features = smc_instance_conf_channel->trace_features_master;

            smc_channel_conf->wake_lock_flags = smc_instance_conf_channel->wake_lock_flags_master;

            smc_channel_conf->history_data_max = smc_instance_conf_channel->history_data_max_master;
        }
        else
        {
            SMC_TRACE_PRINTF_DEBUG("smc_channel_conf_create_from_instance_conf: SLAVE configuration used");

            smc_channel_conf->copy_scheme   = smc_instance_conf_channel->copy_scheme_slave;

            smc_channel_conf->fifo_size_in   = smc_instance_conf_channel->fifo_size_master;
            smc_channel_conf->fifo_size_out  = smc_instance_conf_channel->fifo_size_slave;

            smc_channel_conf->mdb_size_in    = smc_instance_conf_channel->mdb_size_master;
            smc_channel_conf->mdb_size_out   = smc_instance_conf_channel->mdb_size_slave;

            smc_channel_conf->signal_remote  = smc_signal_create( smc_instance_conf_channel->signal_id_slave_to_master, smc_instance_conf_channel->signal_type_slave_to_master );
            smc_channel_conf->signal_local   = smc_signal_create( smc_instance_conf_channel->signal_id_slave_from_master, smc_instance_conf_channel->signal_type_slave_from_master);

            smc_channel_conf->fifo_full_check_timeout_usec = smc_instance_conf_channel->fifo_full_check_timeout_usec_slave;

            smc_channel_conf->rx_mem_realloc_check_timeout_usec = smc_instance_conf_channel->rx_mem_realloc_check_timeout_usec_slave;

            smc_channel_conf->trace_features = smc_instance_conf_channel->trace_features_slave;

            smc_channel_conf->wake_lock_flags = smc_instance_conf_channel->wake_lock_flags_slave;

            smc_channel_conf->history_data_max = smc_instance_conf_channel->history_data_max_slave;
        }

        smc_channel_conf->priority            = smc_instance_conf_channel->priority;
        smc_channel_conf->protocol            = smc_instance_conf_channel->protocol;
        smc_channel_conf->wakelock_timeout_ms = smc_instance_conf_channel->wakelock_timeout_ms;
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("smc_channel_conf_create_from_instance_conf: SMC channel instance conf is NULL");
    }

    SMC_TRACE_PRINTF_DEBUG("smc_channel_conf_create_from_instance_conf: returning SMC channel conf 0x%08X", (uint32_t)smc_channel_conf);

    return smc_channel_conf;
}

smc_instance_conf_t* smc_instance_conf_get_from_list(smc_instance_conf_t* smc_instance_conf_array, int config_count, char* smc_user_name, char* smc_name)
{
    smc_instance_conf_t* config    = NULL;
    int                  i         = 0;

    SMC_TRACE_PRINTF_DEBUG("smc_instance_conf_get_from_list: Searching array with %d items...", config_count);

    for(i = 0; i < config_count; i++)
    {
        SMC_TRACE_PRINTF_DEBUG("smc_instance_conf_get_from_list: Index %d: Check %s->%s: %s <-> %s...", i,
                smc_instance_conf_array[i].user_name, smc_instance_conf_array[i].name,
                smc_instance_conf_array[i].master_name, smc_instance_conf_array[i].slave_name);

        if( strcmp(smc_instance_conf_array[i].user_name , smc_user_name ) == 0 &&
            (strcmp(smc_instance_conf_array[i].master_name , smc_name ) == 0 ||
             strcmp(smc_instance_conf_array[i].slave_name , smc_name ) == 0))
        {
            SMC_TRACE_PRINTF_DEBUG("smc_instance_conf_get_from_list: found item from array index %d", i);
            config = &smc_instance_conf_array[i];

            break;
        }
    }

    SMC_TRACE_PRINTF_DEBUG("smc_instance_conf_get_from_list: returning config 0x%08X", (uint32_t)config);

    return config;
}

char* smc_instance_conf_name_get_from_list(smc_instance_conf_t* smc_instance_conf_array, int config_count, char* smc_user_name, uint8_t is_master, uint8_t version_major, uint8_t version_minor)
{
    char* name = NULL;
    int   i    = 0;
    smc_instance_conf_t* current_config = NULL;

    SMC_TRACE_PRINTF_DEBUG("smc_instance_conf_name_get_from_list: Searching array with %d items...", config_count);

    for(i = 0; i < config_count; i++)
    {
        SMC_TRACE_PRINTF_DEBUG("smc_instance_conf_name_get_from_list: Index %d: Check %s->%s: %s <-> %s...", i,
                smc_instance_conf_array[i].user_name, smc_instance_conf_array[i].name,
                smc_instance_conf_array[i].master_name, smc_instance_conf_array[i].slave_name);

        if( strcmp(smc_instance_conf_array[i].user_name , smc_user_name ) == 0 )
        {
            if( is_master )
            {
                if( version_major > smc_instance_conf_array[i].master_cpu_version_major ||
                    (version_major == smc_instance_conf_array[i].master_cpu_version_major &&
                     version_minor >= smc_instance_conf_array[i].master_cpu_version_minor))
                {
                    /**/
                    if( current_config==NULL || (current_config != NULL &&
                        (smc_instance_conf_array[i].master_cpu_version_major > current_config->master_cpu_version_major ||
                         (smc_instance_conf_array[i].master_cpu_version_major == current_config->master_cpu_version_major &&
                         smc_instance_conf_array[i].master_cpu_version_minor >= current_config->master_cpu_version_minor)) ))
                    {
                        current_config = &smc_instance_conf_array[i];
                        SMC_TRACE_PRINTF_DEBUG("smc_instance_conf_name_get_from_list: found master item from array index %d", i);
                    }
                }
            }
            else
            {
                if( version_major >= smc_instance_conf_array[i].slave_cpu_version_major ||
                    (version_major == smc_instance_conf_array[i].slave_cpu_version_major &&
                    version_minor >= smc_instance_conf_array[i].slave_cpu_version_minor ))
                {
                    /**/
                    if( current_config==NULL || (current_config != NULL &&
                        (smc_instance_conf_array[i].slave_cpu_version_major >= current_config->slave_cpu_version_major ||
                        (smc_instance_conf_array[i].slave_cpu_version_major == current_config->slave_cpu_version_major &&
                        smc_instance_conf_array[i].slave_cpu_version_minor >= current_config->slave_cpu_version_minor) ) ))
                    {
                        current_config = &smc_instance_conf_array[i];
                        SMC_TRACE_PRINTF_DEBUG("smc_instance_conf_name_get_from_list: found slave item from array index %d", i);
                    }
                }
            }
        }
    }

    if( current_config != NULL )
    {
        if( is_master )
        {
            name = current_config->master_name;
        }
        else
        {
            name = current_config->slave_name;
        }
    }

    if( name != NULL )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_instance_conf_name_get_from_list: returning config name %s", name);
    }
    else
    {
        SMC_TRACE_PRINTF_ASSERT("smc_instance_conf_name_get_from_list: No proper SMC '%s' configuration found for version %d.%d", smc_user_name, version_major, version_minor);
        assert(0);
    }

    return name;
}

uint8_t smc_shm_conf_request_received( smc_channel_t* channel, int32_t fifo_out_size, int32_t fifo_in_size, uint32_t mdb_start_address, uint32_t mdb_size )
{
    uint8_t ret_val = SMC_OK;

    int32_t offset =  (int32_t)channel->smc_instance->smc_shm_conf->remote_cpu_memory_offset;

    offset = offset;

    SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_shm_conf_request_received: channel %d: Remote requests: fifo_out_size: %d, fifo_in_size: %d, mdb_start_address 0x%08X, mdb_size: %d",
            channel->id, fifo_out_size, fifo_in_size, mdb_start_address, mdb_size);

    SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_shm_conf_request_received: channel %d: Own data:        fifo_out_size: %d, fifo_in_size: %d, mdb_start_address 0x%08X, mdb_size: %d",
                channel->id, channel->fifo_out->length, channel->fifo_in->length, (uint32_t)(channel->smc_shm_conf_channel->shm_area_start_address-offset), channel->smc_shm_conf_channel->size);

    SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_shm_conf_request_received: channel %d: Own config data:   fifo_out_size: %d, fifo_in_size: %d, mdb_size_out %d, mdb_size_in: %d (total %d)",
                channel->id, channel->fifo_size_out, channel->fifo_size_in, channel->mdb_size_out, channel->mdb_size_in, channel->mdb_size_out+channel->mdb_size_in);


    return ret_val;
}

void smc_shm_conf_response_received( smc_channel_t* channel, int32_t fifo_out_size, int32_t fifo_in_size, uint32_t mdb_start_address, uint32_t mdb_size )
{
    int32_t offset =  (int32_t)channel->smc_instance->smc_shm_conf->remote_cpu_memory_offset;

    offset = offset;

    SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_shm_conf_response_received: channel %d: Remote responses: fifo_out_size: %d, fifo_in_size: %d, mdb_start_address 0x%08X, mdb_size: %d",
            channel->id, fifo_out_size, fifo_in_size, mdb_start_address, mdb_size);

    SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_shm_conf_request_received: channel %d: Own data:          fifo_out_size: %d, fifo_in_size: %d, mdb_start_address 0x%08X, mdb_size: %d",
                channel->id, channel->fifo_out->length, channel->fifo_in->length, (uint32_t)(channel->smc_shm_conf_channel->shm_area_start_address-offset), channel->smc_shm_conf_channel->size);

    SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_shm_conf_request_received: channel %d: Own config data:   fifo_out_size: %d, fifo_in_size: %d, mdb_size_out %d, mdb_size_in: %d (total %d)",
                channel->id, channel->fifo_size_out, channel->fifo_size_in, channel->mdb_size_out, channel->mdb_size_in, channel->mdb_size_out+channel->mdb_size_in);



}



/* EOF */
