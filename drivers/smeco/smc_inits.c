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

Version:       1    24-Oct-2012     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif


#include "smc_common_includes.h"

#include "smc.h"
#include "smc_trace.h"
#include "smc_mdb.h"

#if(SMCTEST==TRUE)
  #include "smc_test.h"
#endif

uint8_t                smc_initialized            = FALSE;

/*
 * Array of all SMC instances created
 */
static smc_t**         g_smc_instance_array       = NULL;
static uint8_t         g_smc_instance_array_count = 0;

    /* Mutexes for initialization function */
static smc_semaphore_t* g_local_mutex_smc_instance      = NULL;

static inline smc_semaphore_t* get_local_mutex_smc_instance(void)
{
    if( g_local_mutex_smc_instance == NULL ) g_local_mutex_smc_instance = smc_semaphore_create();
    return g_local_mutex_smc_instance;
}

/*
 * Static local functions
 */
static void smc_instance_add( smc_t* smc_instance );


/*
 * Extern functions
 */
extern uint8_t smc_module_initialize( smc_conf_t* smc_instance_conf );


char* smc_get_version( void )
{
    return SMC_SW_VERSION;
}

/*
 * Returns SMC Channel of specified SMC instance with specified SMC channel id.
 * If the SMC channel is not found, function returns NULL.
 */
smc_channel_t* smc_channel_get( const smc_t* smc_instance, uint8_t smc_channel_id )
{
    smc_channel_t* smc_channel = NULL;

    if( smc_instance != NULL && smc_instance->smc_channel_ptr_array && smc_channel_id < smc_instance->smc_channel_list_count )
    {
        smc_channel = smc_instance->smc_channel_ptr_array[smc_channel_id];
    }

    return smc_channel;
}



/* ========================================
 * SMC initialization functions
 *
 */



/*
 * Creates SMC instance based on given SMC configuration.
 *
 * NOTE: Every item value in configuration structure (except function ptrs)
 *       must be copied to SMC instance because the configuration
 *       might be destroyed after the configuration.
 */
smc_t* smc_instance_create(smc_conf_t* smc_instance_conf)
{
    return smc_instance_create_ext(smc_instance_conf, NULL);
}

/*
 * Creates SMC instance based on given SMC configuration.
 *
 * NOTE: Every item value in configuration structure (except function ptrs)
 *       must be copied to SMC instance because the configuration
 *       might be destroyed after the configuration.
 */

smc_t* smc_instance_create_ext(smc_conf_t* smc_instance_conf, void* parent_object)
{
    smc_t* smc = (smc_t*)SMC_MALLOC( sizeof(smc_t) );

    assert( smc != NULL );
    assert( smc_instance_conf != NULL );

    SMC_TRACE_PRINTF_DEBUG("smc_instance_create_ext: created SMC instance 0x%08X based on configuration in 0x%08X",
            (uint32_t)smc, (uint32_t)smc_instance_conf);

    smc->cpu_id_remote                = smc_instance_conf->cpu_id_remote;
    smc->cpu_id_local                 = smc_instance_conf->cpu_id_local;
    smc->is_master                    = smc_instance_conf->is_master;
    smc->smc_channel_count_configured = smc_instance_conf->smc_channel_conf_count;
    smc->smc_channel_list_count       = 0;
    smc->smc_channel_ptr_array        = NULL;
    smc->smc_instance_conf            = smc_instance_conf;
    smc->smc_parent_ptr               = parent_object;
    smc->init_status                  = SMC_INSTANCE_STATUS_INIT_NONE;
    smc->instance_name                = smc_instance_conf->name;
    smc->tx_wakelock_count            = 0;
    smc->rx_wakelock_count            = 0;

    smc->initialization_flags         = smc_instance_conf->initialization_flags;

    smc_instance_add( smc );

    if( smc_instance_conf->smc_shm_conf != NULL )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_instance_create_ext: SHM configuration found, copying to instance...");
        smc->smc_shm_conf = smc_shm_config_copy( smc_instance_conf->smc_shm_conf );

        if( smc->smc_shm_conf != NULL )
        {
            if( smc->smc_shm_conf->shm_area_start_address != NULL )
            {
                uint32_t physical_addr = (uint32_t)smc->smc_shm_conf->shm_area_start_address;

                smc->smc_shm_conf->shm_area_start_address = SMC_SHM_IOREMAP( smc->smc_shm_conf->shm_area_start_address, smc->smc_shm_conf->size );

                if( ((uint32_t)smc->smc_shm_conf->shm_area_start_address - physical_addr) != 0 )
                {
                        /* Store the offset for debugging purposes only if the shm is in virtual memory */
                    smc->smc_shm_conf->remote_cpu_memory_offset = ((uint32_t)smc->smc_shm_conf->shm_area_start_address - physical_addr);
                }

                SMC_TRACE_PRINTF_DEBUG("smc_instance_create_ext: Remapped SHM start address from 0x%08X is 0x%08X, size is %d, offset to physical is %d (0x%08X)",
                        (uint32_t)smc_instance_conf->smc_shm_conf->shm_area_start_address , (uint32_t)smc->smc_shm_conf->shm_area_start_address,
                        smc->smc_shm_conf->size, (int32_t)smc->smc_shm_conf->remote_cpu_memory_offset, (uint32_t)smc->smc_shm_conf->remote_cpu_memory_offset);

                SMC_TRACE_PRINTF_STARTUP("Shared memory size %d bytes, memory area: 0x%08X - 0x%08X (physical: 0x%08X - 0x%08X)", smc->smc_shm_conf->size,
                                        (uint32_t)smc->smc_shm_conf->shm_area_start_address, ((uint32_t)smc->smc_shm_conf->shm_area_start_address + smc->smc_shm_conf->size),
                                        ((uint32_t)smc->smc_shm_conf->shm_area_start_address-smc->smc_shm_conf->remote_cpu_memory_offset),
                                        (((uint32_t)smc->smc_shm_conf->shm_area_start_address + smc->smc_shm_conf->size)-smc->smc_shm_conf->remote_cpu_memory_offset) );
            }
            else
            {
                SMC_TRACE_PRINTF_WARNING("smc_instance_create_ext: SHM start address is NULL, SHM not available");
            }

            SMC_TRACE_PRINTF_DEBUG("smc_instance_create_ext: SHM configuration copied, modify first free address to 0x%08X...", (uint32_t)smc->smc_shm_conf->shm_area_start_address);
            smc->smc_memory_first_free = smc->smc_shm_conf->shm_area_start_address;
        }
        else
        {
            SMC_TRACE_PRINTF_WARNING("smc_instance_create_ext: SHM configuration copy failed");
            smc->smc_memory_first_free = NULL;
        }
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("smc_instance_create_ext: SHM configuration not set");
        smc->smc_shm_conf = NULL;
        smc->smc_memory_first_free = NULL;
    }

        /* Create the SMC channels */
    if( smc_instance_conf->smc_channel_conf_count > 0 )
    {
        int i = 0;

        SMC_TRACE_PRINTF_DEBUG("smc_instance_create_ext: found %d channel configurations, initializing SMC channels...", smc_instance_conf->smc_channel_conf_count);

        for(i = 0; i < smc_instance_conf->smc_channel_conf_count; i++ )
        {
            smc_channel_t* channel = smc_channel_create( smc, smc_instance_conf->smc_channel_conf_ptr_array[i] );

/*
#ifdef SMC_DMA_TRANSFER_ENABLED
            if( channel->smc_dma != NULL )
            {
                smc_dma_set_device( channel->smc_dma, parent_object);
            }
#endif
*/
            SMC_TRACE_PRINTF_INFO("smc_instance_create_ext: new SMC channel in index %d created (0x%08X), add to channel array...",
                        i, (uint32_t)channel);

            smc_add_channel(smc, channel, smc_instance_conf->smc_channel_conf_ptr_array[i]);
        }

        SMC_TRACE_PRINTF_DEBUG("smc_instance_create_ext: channels successfully configured");

        SMC_INSTANCE_STATUS_INIT_SET_LOCAL( smc->init_status );

            /* Send message to remote that channels are initialized */
        if( smc_send_channels_initialized_message( smc ) != SMC_OK )
        {
            SMC_TRACE_PRINTF_WARNING("smc_instance_create_ext: smc_send_channels_initialized_message failed");
        }

        if( SMC_INSTANCE_CAN_SEND_FIXED_CONFIG( smc->init_status ) )
        {
            SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_instance_create_ext: SMC_FIFO_IS_INTERNAL_MESSAGE_REMOTE_CHANNELS_INIT, send negotiable configurations...");

                /* Send negotiable parameters using all channels */
            if( smc_send_negotiable_configurations( smc ) != SMC_OK )
            {
                SMC_TRACE_PRINTF_WARNING("smc_instance_create_ext: smc_send_negotiable_configurations failed");
            }
        }
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("smc_instance_create_ext: no channels found from configuration");
    }

    SMC_TRACE_PRINTF_DEBUG("smc_instance_create_ext: new SMC instance 0x%08X created", (uint32_t)smc);

#ifdef SMC_TRACE_INFO_ENABLED

    smc_instance_dump(smc);

#endif

    return smc;
}


/*
 * Creates SMC channel based on configuration.
 *
 * NOTE: Every item value in configuration structure
 *       (except function ptrs and memory address references)
 *       must be copied to SMC channel because the configuration
 *       might be destroyed after the configuration.
 */
smc_channel_t* smc_channel_create( smc_t* smc_instance, smc_channel_conf_t* smc_channel_conf )
{
    smc_channel_t* channel = (smc_channel_t*)SMC_MALLOC( sizeof( smc_channel_t ) );

    assert( channel != NULL );

        /* Setup the configuration */
    channel->id                            = smc_channel_conf->channel_id;
    channel->priority                      = smc_channel_conf->priority;
    channel->smc_instance                  = smc_instance;
    channel->copy_scheme                   = smc_channel_conf->copy_scheme;
    channel->protocol                      = smc_channel_conf->protocol;
    channel->wake_lock_flags               = smc_channel_conf->wake_lock_flags;

    channel->smc_shm_conf_channel          = NULL;
    channel->trace_features                = smc_channel_conf->trace_features;
    channel->version_remote                = 0x00000000;

#ifdef SMC_HISTORY_DATA_COLLECTION_ENABLED
    channel->smc_history_items_max         = smc_channel_conf->history_data_max;
#endif

    channel->wakelock_timeout_ms           = smc_channel_conf->wakelock_timeout_ms;

        /* Initialize callback functions */
    channel->smc_receive_cb                = (smc_receive_data_callback)smc_channel_conf->smc_receive_data_cb;
    channel->smc_send_data_deallocator_cb  = (smc_send_data_deallocator_callback)smc_channel_conf->smc_send_data_deallocator_cb;

    channel->smc_receive_data_allocator_cb = (smc_receive_data_allocator_callback)smc_channel_conf->smc_receive_data_allocator_cb;
    channel->smc_event_cb                  = (smc_event_callback)smc_channel_conf->smc_event_cb;

        /*
         * FIFOs are created when channel is added to SMC instance and SHM is ready
         */
    channel->fifo_out    = NULL;
    channel->fifo_in     = NULL;

    if( smc_channel_conf->fifo_full_check_timeout_usec > 0 )
    {
        channel->fifo_timer = smc_timer_create( smc_channel_conf->fifo_full_check_timeout_usec );
    }
    else
    {
        channel->fifo_timer = NULL;
        SMC_TRACE_PRINTF_WARNING("smc_channel_create: timeout for FIFO full check not set");
    }

    if( smc_channel_conf->rx_mem_realloc_check_timeout_usec > 0 )
    {
        channel->rx_mem_alloc_timer = smc_timer_create( smc_channel_conf->rx_mem_realloc_check_timeout_usec );
    }
    else
    {
        channel->rx_mem_alloc_timer = NULL;
        SMC_TRACE_PRINTF_DEBUG("smc_channel_create: timeout for memory reallocation not set");
    }




    channel->fifo_size_in  = smc_channel_conf->fifo_size_in;
    channel->fifo_size_out = smc_channel_conf->fifo_size_out;

    channel->mdb_size_in   = smc_channel_conf->mdb_size_in;
    channel->mdb_size_out  = smc_channel_conf->mdb_size_out;

        /*
         * MDBs are created when channel is added to SMC instance and SHM is ready
         */
    channel->mdb_out      = NULL;
    channel->mdb_in       = NULL;
    channel->smc_mdb_info = NULL;

        /* Initialize locks */
    channel->lock_write    = smc_lock_create();
    channel->lock_read     = smc_lock_create();
    channel->lock_mdb      = smc_lock_create();
    channel->lock_tx_queue = smc_lock_create();

        /* Initialize Signals */
    channel->signal_remote = smc_signal_copy( smc_channel_conf->signal_remote );
    channel->signal_local  = smc_signal_copy( smc_channel_conf->signal_local );

        /*
         * Set the default values for non-configurable items
         */
    channel->state = 0x00000000;

        /* Set default flags */
    SMC_CHANNEL_STATE_CLEAR_SYNCHRONIZED(channel->state);

        /*
         * Initialize FIFO buffer
         */
    channel->fifo_buffer_current_index   = 0;
    channel->fifo_buffer_data_count      = 0;
    channel->fifo_buffer                 = NULL;

#ifdef SMC_SEND_USE_SEMAPHORE
    channel->send_semaphore              = smc_semaphore_create();
#else
    channel->send_semaphore              = NULL;
#endif

    channel->dropped_packets_mdb_out     = 0;
    channel->dropped_packets_fifo_buffer = 0;
    channel->send_packets_fifo_buffer    = 0;
    channel->fifo_buffer_copied_total    = 0;
    channel->tx_queue_peak               = 0;
    channel->rx_queue_peak               = 0;


#ifdef SMC_APE_WAKEUP_WAKELOCK_USE

    /* TX wakelock */
#ifdef SMC_NETDEV_WAKELOCK_IN_TX
    {
        //char* name        = NULL;
        char* name_prefix = "smc_wakelock_tx_";
        char* temp_str    = NULL;
        int   str_len     = 0;

        temp_str = smc_utoa( smc_instance->tx_wakelock_count++ );

        str_len = strlen(name_prefix) + strlen(temp_str) + 1;

        channel->smc_tx_wakelock_name = (char*)SMC_MALLOC_IRQ(str_len);

        if( channel->smc_tx_wakelock_name != NULL )
        {
            memset( channel->smc_tx_wakelock_name, 0, str_len );
            strcpy( channel->smc_tx_wakelock_name, name_prefix );
            strcpy( channel->smc_tx_wakelock_name+strlen(name_prefix), temp_str );

            channel->smc_tx_wakelock = smc_wakelock_create(channel->smc_tx_wakelock_name);
        }
        else
        {
            SMC_TRACE_PRINTF_ERROR("smc_channel_create: Unable to allocate memory for the TX wakelock name, TX wakelock is NULL");
        }

        SMC_FREE(temp_str);
    }
#endif

    /* RX wakelock */
    {
        //char* name        = NULL;
        char* name_prefix = "smc_wakelock_rx_";
        char* temp_str    = NULL;
        int   str_len     = 0;

        temp_str = smc_utoa( smc_instance->rx_wakelock_count++ );

        str_len = strlen(name_prefix) + strlen(temp_str) + 1;

        channel->smc_rx_wakelock_name = (char*)SMC_MALLOC_IRQ(str_len);

        if( channel->smc_rx_wakelock_name != NULL )
        {
            memset( channel->smc_rx_wakelock_name, 0, str_len );
            strcpy( channel->smc_rx_wakelock_name, name_prefix );
            strcpy( channel->smc_rx_wakelock_name+strlen(name_prefix), temp_str );

            channel->smc_rx_wakelock = smc_wakelock_create(channel->smc_rx_wakelock_name);
        }
        else
        {
            SMC_TRACE_PRINTF_ERROR("smc_channel_create: Unable to allocate memory for the RX wakelock name, RX wakelock is NULL");
        }

        SMC_FREE(temp_str);
    }
#endif /* #ifdef SMC_APE_WAKEUP_WAKELOCK_USE */


#ifdef SMC_DMA_TRANSFER_ENABLED
    channel->smc_dma = NULL;

    if( SMC_COPY_SCHEME_RECEIVE_USE_DMA(smc_channel_conf->copy_scheme) ||
        SMC_COPY_SCHEME_SEND_USE_DMA(smc_channel_conf->copy_scheme) )
    {
        channel->smc_dma = smc_dma_create();
    }
#endif

    SMC_TRACE_PRINTF_DEBUG("smc_channel_create: channel %d: 0x%08X created", channel->id, (uint32_t)channel);

    return channel;
}

static void smc_instance_add( smc_t* smc_instance )
{
    smc_t**     old_array  = NULL;
    smc_semaphore_t* local_mutex = NULL;

    local_mutex = get_local_mutex_smc_instance();
    SMC_LOCK_MUTEX( local_mutex );

    SMC_TRACE_PRINTF_DEBUG("smc_instance_add: add instance 0x%08X, count %d", (uint32_t)smc_instance, g_smc_instance_array_count);

    if( g_smc_instance_array )
    {
        old_array = g_smc_instance_array;
    }

    g_smc_instance_array_count++;

    g_smc_instance_array = (smc_t**)SMC_MALLOC( sizeof(smc_t*) * g_smc_instance_array_count );

    assert(g_smc_instance_array != NULL );

    if( old_array )
    {
        int i = 0;

        for( i = 0; i < g_smc_instance_array_count-1; i++ )
        {
            g_smc_instance_array[i] = old_array[i];
        }

        g_smc_instance_array[g_smc_instance_array_count-1] = smc_instance;

        SMC_FREE( old_array );
        old_array = NULL;
    }
    else
    {
        g_smc_instance_array[0] = smc_instance;
    }

    SMC_TRACE_PRINTF_DEBUG("smc_instance_add: instance 0x%08X added, count %d", (uint32_t)smc_instance, g_smc_instance_array_count);

    SMC_UNLOCK_MUTEX( local_mutex );
}

static void smc_instance_remove( smc_t* smc_instance )
{
    smc_semaphore_t* local_mutex = NULL;

    local_mutex = get_local_mutex_smc_instance();
    SMC_LOCK_MUTEX( local_mutex );

    SMC_TRACE_PRINTF_DEBUG("smc_instance_remove: remove instance 0x%08X, count %d", (uint32_t)smc_instance, g_smc_instance_array_count);

    if( g_smc_instance_array != NULL && g_smc_instance_array_count > 0 )
    {
        smc_t** old_array      = NULL;
        uint8_t instance_found = 0;
        uint8_t instance_index = 0;

        for( int i = 0; i < g_smc_instance_array_count; i++ )
        {
            if( g_smc_instance_array[i] == smc_instance )
            {
                instance_found = 1;
                instance_index = i;
                break;
            }
        }

        old_array = g_smc_instance_array;

        if( instance_found > 0 )
        {
            g_smc_instance_array_count--;

            if( g_smc_instance_array_count > 0 )
            {
                g_smc_instance_array = (smc_t**)SMC_MALLOC( sizeof(smc_t*) * g_smc_instance_array_count );

                assert( g_smc_instance_array != NULL );
            }
            else
            {
                SMC_FREE( g_smc_instance_array );
                g_smc_instance_array       = NULL;
                g_smc_instance_array_count = 0;
            }

            if( old_array && g_smc_instance_array)
            {
                uint8_t temp_index = 0;

                for(int i = 0; i < g_smc_instance_array_count+1; i++ )
                {
                    if( i != instance_index )
                    {
                        g_smc_instance_array[temp_index] = old_array[i];
                        temp_index++;
                    }
                }

                SMC_FREE(old_array);
                old_array = NULL;
            }
        }
    }

    SMC_TRACE_PRINTF_DEBUG("smc_instance_remove: instance 0x%08X removed, count %d", (uint32_t)smc_instance, g_smc_instance_array_count);

    SMC_UNLOCK_MUTEX( local_mutex );
}


smc_t** smc_instance_array_get( void )
{
    return g_smc_instance_array;
}

uint8_t smc_instance_array_count_get( void )
{
    return g_smc_instance_array_count;
}


/* ========================================
 * SMC destroy functions
 *
 */


/**
 * Destroys specified SMC instance and data related to it.
 */
void smc_instance_destroy( smc_t* smc_instance )
{
    if( smc_instance )
    {
#ifdef SMC_DUMP_ON_CLOSE_ENABLED
        smc_instance_dump(smc_instance);
        SMC_TRACE_PRINTF_STARTUP("");
#endif

        SMC_TRACE_PRINTF_STARTUP("Closing SMC instance with %d channels...", smc_instance->smc_channel_list_count);

        for(int i = 0; i < smc_instance->smc_channel_list_count; i++)
        {
            smc_channel_destroy( smc_instance->smc_channel_ptr_array[i] );
        }

        SMC_FREE( smc_instance->smc_channel_ptr_array );
        smc_instance->smc_channel_ptr_array = NULL;

        if( smc_instance->smc_shm_conf != NULL )
        {
            SMC_TRACE_PRINTF_STARTUP("Deallocating Shared memory area: size %d bytes, memory area: 0x%08X - 0x%08X (physical: 0x%08X - 0x%08X)", smc_instance->smc_shm_conf->size,
                                                    (uint32_t)smc_instance->smc_shm_conf->shm_area_start_address, ((uint32_t)smc_instance->smc_shm_conf->shm_area_start_address + smc_instance->smc_shm_conf->size),
                                                    ((uint32_t)smc_instance->smc_shm_conf->shm_area_start_address-smc_instance->smc_shm_conf->remote_cpu_memory_offset),
                                                    (((uint32_t)smc_instance->smc_shm_conf->shm_area_start_address + smc_instance->smc_shm_conf->size)-smc_instance->smc_shm_conf->remote_cpu_memory_offset) );

            SMC_SHM_IOUNMAP( smc_instance->smc_shm_conf->shm_area_start_address );

            SMC_FREE( smc_instance->smc_shm_conf );
            smc_instance->smc_shm_conf = NULL;
        }
        else
        {
            SMC_TRACE_PRINTF_STARTUP("Shared memory area was not configured");
        }

        smc_instance_remove( smc_instance );

        SMC_FREE( smc_instance );
        smc_instance = NULL;

        SMC_TRACE_PRINTF_STARTUP("SMC instance closed");
    }
}

void smc_channel_destroy( smc_channel_t* smc_channel )
{
    if( smc_channel )
    {
        SMC_TRACE_PRINTF_STARTUP("Closing SMC Channel %d...", smc_channel->id);

        if( smc_channel->mdb_out != NULL )
        {
            SMC_TRACE_PRINTF_DEBUG("smc_channel_destroy: channel 0x%08X: destroy MDB INFO 0x%08X...", (uint32_t)smc_channel, (uint32_t)smc_channel->smc_mdb_info);
            smc_mdb_info_destroy(smc_channel->smc_mdb_info);
        }

        if( smc_channel->fifo_timer != NULL )
        {
            smc_timer_destroy( smc_channel->fifo_timer );
        }

        if( smc_channel->rx_mem_alloc_timer != NULL )
        {
            smc_timer_destroy( smc_channel->rx_mem_alloc_timer );
        }

        if( smc_channel->lock_write != NULL )
        {
            smc_lock_destroy(smc_channel->lock_write);
        }

        if( smc_channel->lock_read != NULL )
        {
            smc_lock_destroy(smc_channel->lock_read);
        }

        if( smc_channel->lock_mdb != NULL )
        {
            smc_lock_destroy(smc_channel->lock_mdb);
        }

        if( smc_channel->lock_tx_queue != NULL )
        {
            smc_lock_destroy(smc_channel->lock_tx_queue);
        }

        if( smc_channel->signal_remote != NULL )
        {
            smc_signal_destroy(smc_channel->signal_remote);
        }

        if( smc_channel->signal_local != NULL )
        {
                /* Local interrupt has IRQ handler registered */
            smc_signal_handler_unregister( smc_channel->smc_instance, smc_channel->signal_local, smc_channel );
            smc_signal_destroy(smc_channel->signal_local);
        }

        if( smc_channel->smc_shm_conf_channel )
        {
            smc_shm_conf_destroy(smc_channel->smc_shm_conf_channel);
        }

        if( smc_channel->fifo_buffer != NULL )
        {
            SMC_TRACE_PRINTF_DEBUG("smc_channel_destroy: channel 0x%08X: destroy FIFO buffer 0x%08X...", (uint32_t)smc_channel, (uint32_t)smc_channel->fifo_buffer);
            SMC_FREE( smc_channel->fifo_buffer );
            smc_channel->fifo_buffer = NULL;
        }

        if( smc_channel->send_semaphore != NULL )
        {
            SMC_TRACE_PRINTF_DEBUG("smc_channel_destroy: channel 0x%08X: destroy send semaphore 0x%08X...", (uint32_t)smc_channel, (uint32_t)smc_channel->send_semaphore);
            SMC_FREE( smc_channel->send_semaphore );
            smc_channel->send_semaphore = NULL;
        }

#ifdef SMC_APE_WAKEUP_WAKELOCK_USE

        /* TX wakelock */
#ifdef SMC_NETDEV_WAKELOCK_IN_TX
        {
            uint8_t destroy_ptr = TRUE;

            smc_wakelock_destroy(smc_channel->smc_tx_wakelock, destroy_ptr);

            if( smc_channel->smc_tx_wakelock_name != NULL )
            {
                SMC_FREE(smc_channel->smc_tx_wakelock_name);
                smc_channel->smc_tx_wakelock_name = NULL;
            }

            if( destroy_ptr )
            {
                SMC_TRACE_PRINTF_DEBUG("smc_channel_destroy: SMC Channel %d: TX wakelock ptr freed", smc_channel->id);
                smc_channel->smc_tx_wakelock = NULL;
            }
        }
#endif

        {
            uint8_t destroy_ptr = TRUE;

            smc_wakelock_destroy(smc_channel->smc_rx_wakelock, destroy_ptr);

            if( smc_channel->smc_rx_wakelock_name != NULL )
            {
                SMC_FREE(smc_channel->smc_rx_wakelock_name);
                smc_channel->smc_rx_wakelock_name = NULL;
            }

            if( destroy_ptr )
            {
                SMC_TRACE_PRINTF_DEBUG("smc_channel_destroy: SMC Channel %d: RX wakelock ptr freed", smc_channel->id);
                smc_channel->smc_rx_wakelock = NULL;
            }
        }

#endif  /* #ifdef SMC_APE_WAKEUP_WAKELOCK_USE */

#ifdef SMC_DMA_TRANSFER_ENABLED
        if( smc_channel->smc_dma != NULL )
        {
            SMC_TRACE_PRINTF_DEBUG("smc_channel_destroy: channel 0x%08X: destroy dma 0x%08X...", (uint32_t)smc_channel, (uint32_t)smc_channel->smc_dma);
            smc_dma_destroy( smc_channel->smc_dma );
        }
#endif

#ifdef SMC_HISTORY_DATA_COLLECTION_ENABLED
        if( smc_channel->smc_history_data_sent != NULL )
        {
            SMC_TRACE_PRINTF_DEBUG("smc_channel_destroy: channel 0x%08X: destroy sent data history buffer 0x%08X...", (uint32_t)smc_channel, (uint32_t)smc_channel->smc_history_data_sent);
            SMC_FREE( smc_channel->smc_history_data_sent );
            smc_channel->smc_history_data_sent = NULL;
        }

        if( smc_channel->smc_history_data_received != NULL )
        {
            SMC_TRACE_PRINTF_DEBUG("smc_channel_destroy: channel 0x%08X: destroy received data history buffer 0x%08X...", (uint32_t)smc_channel, (uint32_t)smc_channel->smc_history_data_received);
            SMC_FREE( smc_channel->smc_history_data_received );
            smc_channel->smc_history_data_received = NULL;
        }
#endif
        /* Finally destroy the smc channel pointer */

        SMC_FREE( smc_channel );

        smc_channel = NULL;

        SMC_TRACE_PRINTF_DEBUG("smc_channel_destroy: channel 0x%08X completed", (uint32_t)smc_channel);
    }
}



/* ========================================
 * Various functions
 *
 */

/*
 * Change the SMC Channel priority.
 * Returns the old priority.
 */
uint8_t smc_channel_change_priority( smc_t* smc_instance, uint8_t smc_channel_id, uint8_t new_priority )
{
    uint8_t old_priority = SMC_CHANNEL_PRIORITY_LOWEST;

    smc_channel_t* channel = smc_channel_get( smc_instance, smc_channel_id );

    if( channel != NULL )
    {
        old_priority = channel->priority;
        channel->priority = new_priority;
    }

    return old_priority;
}

char* smc_version_to_str(uint32_t version)
{
    char* version_str = NULL;
    int   str_len     = 3+3+1;
    char* temp_str    = NULL;
    int   version_ind = 0;

    uint8_t  v1 = (version>>24)&0xFF;
    uint8_t  v2 = (version>>16)&0xFF;
    uint16_t v3 =  version&0xFFFF;

    SMC_TRACE_PRINTF_INFO("smc_version_to_str: convert version 0x%08X to string...", version);

    if( v1 > 9 )
    {
        str_len++;
    }

    if( v2 > 9 )
    {
        str_len++;
    }

    if( v3 > 9 )
    {
        str_len++;

        if( v3 > 99 )
        {
            str_len++;

            if( v3 > 999 )
            {
                str_len++;
            }
        }
    }

    version_str = (char*)SMC_MALLOC_IRQ(str_len);

    if( version_str != NULL )
    {
        temp_str = smc_utoa( v1 );
        strcpy( version_str, temp_str );
        version_ind += strlen(temp_str);
        SMC_FREE( temp_str );

        strcpy( (version_str+version_ind), "." );
        version_ind += 1;

        temp_str = smc_utoa( v2 );
        strcpy( (version_str + version_ind), temp_str );
        version_ind += strlen(temp_str);
        SMC_FREE( temp_str );

        strcpy( (version_str+version_ind), "." );
        version_ind += 1;

        temp_str = smc_utoa( v3 );
        strcpy( (version_str + version_ind), temp_str );
        version_ind += strlen(temp_str);
        SMC_FREE( temp_str );

        SMC_TRACE_PRINTF_INFO("smc_version_to_str: version 0x%08X converted to '%s'...", version, version_str);
    }

    return version_str;
}

uint32_t smc_version_to_int(char* version)
{
    uint32_t version_int        = 0x00000000;
    int      level              = 0;
    int      version_ind        = 0;
    int      str_len            = strlen( version );
    char     version_number[10];

    SMC_TRACE_PRINTF_INFO("smc_version_to_int: convert version '%s' to unsigned integer, len = %d..", version, str_len);

    memset(version_number, 0, 10);

    for(int i = 0; i < str_len; i++)
    {
        char ch = version[i];

        if( ch != '.' || ( level<=1 && version_ind>2) || ( level==2 && version_ind>4) )
        {
            if( ch >= 48 && ch <= 57 )
            {
                version_number[version_ind] = ch;
                version_ind++;
            }

            if(( level<=1 && version_ind>2) || ( level==2 && version_ind>4))
            {
                level++;

                if( level > 2 )
                {
                    break;
                }
            }
        }
        else
        {
            int vers = smc_atoi( version_number );

            if( level == 0 )
            {
                version_int += ( (vers&0xFF) << 24 );
            }
            else if( level == 1 )
            {
                version_int += ( (vers&0xFF) << 16 );
            }
            else
            {
                version_int += ( vers&0xFFFF );
            }

            memset(version_number, 0, 10);
            version_ind = 0;
            level++;
        }

        if( level > 2 )
        {
            break;
        }
    }

    if( strlen( version_number ) > 0 )
    {
        int vers = smc_atoi( version_number );

        if( level == 0 )
        {
            version_int += ( (vers&0xFF) << 24 );
        }
        else if( level == 1 )
        {
            version_int += ( (vers&0xFF) << 16 );
        }
        else
        {
            version_int += ( vers&0xFFFF );
        }
    }

    SMC_TRACE_PRINTF_INFO("smc_version_to_int: version '%s' converted to 0x%08X", version, version_int);

    return version_int;
}

int smc_atoi(char* a)
{
    int number  = 0;
    int str_len = strlen( a );
    int iPow    = 1;

    for(int i = str_len-1; i >= 0; i--)
    {
       char ch = a[i];

       if( ch >= 48 && ch <= 57 )
       {
           number += ( ch-48 ) * iPow;
           iPow *= 10;
       }
    }

    return number;
}

char* smc_utoa(uint32_t i)
{
    static char int_digits[]    = "0123456789";
    char*       int_str         = NULL;
    int         iNrIndx         = 0;
    char        temp[9];
    uint32_t    version_counter = i;
    int         divider_counter = 10;

    memset(temp, 0, 9);

    while( divider_counter >= 0 )
    {
        uint8_t mod = 0;
        uint32_t iPow = 1;

        for( int x = 0; x < divider_counter; x++)
        {
            iPow *= 10;
        }

        mod = (version_counter / iPow);

        if( !(mod==0 && divider_counter>0) )
        {
            temp[iNrIndx] = int_digits[mod];
            iNrIndx++;
            version_counter -= (mod*iPow);
        }

        divider_counter--;
    }

    int_str = (char*)SMC_MALLOC_IRQ(strlen(temp)+1);

    if( int_str != NULL )
    {
        strcpy(int_str, temp);
    }

    return int_str;
}


/*
 * General initialization function
 * This is not required to use.
 */
uint8_t smc_initialize( smc_conf_t* smc_instance_conf )
{
    if( !smc_initialized )
    {
        uint8_t return_value = SMC_ERROR;

        SMC_TRACE_PRINTF_VERSION("Version %s", SMC_SW_VERSION);

        RD_TRACE_SEND1(TRA_SMC_INIT, 2, &return_value);

        SMC_TRACE_PRINTF_DEBUG("smc_initialize: starting...");

#if(SMCTEST==TRUE)
        smc_test_handler_register();
#endif

        /* Instance configuration not required currently
        if( smc_instance_conf == NULL )
        {
            SMC_TRACE_PRINTF_WARNING("smc_initialize: SMC initialization is NULL");
        }
        */


        SMC_TRACE_PRINTF_DEBUG("smc_initialize: initializing locks...");
        get_local_mutex_smc_instance();

            /* Initialize SMC core features */
        smc_init_core();

        SMC_TRACE_PRINTF_DEBUG("smc_initialize: initializing plaform specific module...");
        return_value = smc_module_initialize( smc_instance_conf );

        smc_initialized = TRUE;

        SMC_TRACE_PRINTF_DEBUG("smc_initialize: completed by return value 0x%02X", return_value);
        return return_value;
    }
    else
    {
        return SMC_OK;
    }
}


smc_conf_t* smc_instance_get_conf( smc_t* smc_instance )
{
    SMC_TRACE_PRINTF_ASSERT("smc_instance_get_conf: NOT IMPLEMENTED");
    assert(0);
    return NULL;
}

smc_channel_conf_t* smc_channel_get_conf( smc_channel_t* smc_channel )
{
    SMC_TRACE_PRINTF_ASSERT("smc_channel_get_conf: NOT IMPLEMENTED");
    assert(0);
    return NULL;
}

/**
 * Returns TRUE if any of the channels of the specified SMC instance uses DMA transfer.
 */
uint8_t smc_instance_uses_dma( smc_conf_t* smc_instance_conf )
{
    uint8_t use_dma = FALSE;

    SMC_TRACE_PRINTF_DMA("smc_instance_uses_dma: check DMA usage of SMC instance configuration 0x%08X...", (uint32_t)smc_instance_conf);

    for(int i = 0; i < smc_instance_conf->smc_channel_conf_count; i++)
    {
        if( SMC_COPY_SCHEME_SEND_USE_DMA(smc_instance_conf->smc_channel_conf_ptr_array[i]->copy_scheme) ||
            SMC_COPY_SCHEME_RECEIVE_USE_DMA(smc_instance_conf->smc_channel_conf_ptr_array[i]->copy_scheme) )
        {
            SMC_TRACE_PRINTF_DMA("smc_instance_uses_dma: channel %d uses DMA, copy scheme = 0x%02X",
                    smc_instance_conf->smc_channel_conf_ptr_array[i]->channel_id,
                    smc_instance_conf->smc_channel_conf_ptr_array[i]->copy_scheme );

            use_dma = TRUE;
            break;
        }
    }

    SMC_TRACE_PRINTF_DMA("smc_instance_uses_dma: return DMA usage = %s", use_dma?"TRUE":"FALSE");

    return use_dma;
}


/* End of file */


