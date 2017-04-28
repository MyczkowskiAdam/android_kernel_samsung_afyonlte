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

Version:       1    09-Jan-2012     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif


#include "smc_common_includes.h"

#include <linux/phonet.h>
#include <linux/l2mux.h>
#include <net/phonet/pn_dev.h>

#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/if_phonet.h>

#include "smc.h"
#include "smc_trace.h"
#include "smc_conf_l2mux_linux.h"
#include "smc_config_l2mux.h"

#if( SMCTEST == TRUE )
  #include "smc_test.h"
#endif



typedef struct _smc_skb_receive_data_t
{
    struct  net_device* device;
    struct  _smc_channel_t* channel;
    struct _smc_user_data_t userdata;
    struct  sk_buff *skb;
    void*   data;
    int32_t data_length;

} smc_skb_receive_data_t;

#ifdef SMC_RX_MEMORY_REALLOC_TIMER_ENABLED

#define SMC_SKB_DATA_ITEM_BUFFER_SIZE   15

static smc_lock_t* g_local_lock_smc_skb_data_buffer = NULL;

static inline smc_lock_t* get_local_lock_smc_skb_data_buffer(void)
{
    if( g_local_lock_smc_skb_data_buffer == NULL ) g_local_lock_smc_skb_data_buffer = smc_lock_create();
    return g_local_lock_smc_skb_data_buffer;
}

static smc_skb_receive_data_t* g_smc_skb_receive_data_buffer[SMC_SKB_DATA_ITEM_BUFFER_SIZE];

static inline void smc_create_skb_receive_data_item_buffer( uint8_t from_irq )
{
    SMC_LOCK_IRQ( get_local_lock_smc_skb_data_buffer() );

    for(int i = 0; i < SMC_SKB_DATA_ITEM_BUFFER_SIZE; i++)
    {
        smc_skb_receive_data_t* item = NULL;

        if( from_irq )
        {
            item = (smc_skb_receive_data_t*)SMC_MALLOC_IRQ( sizeof( smc_skb_receive_data_t ) );
        }
        else
        {
            item = (smc_skb_receive_data_t*)SMC_MALLOC( sizeof( smc_skb_receive_data_t ) );
        }

        if( item != NULL )
        {
            item->device      = NULL;
            item->channel     = NULL;

            item->userdata.flags = 0x00;
            item->userdata.userdata1 = 0x00;
            item->userdata.userdata2 = 0x00;
            item->userdata.userdata3 = 0x00;
            item->userdata.userdata4 = 0x00;
            item->userdata.userdata5 = 0x00;

            item->skb         = NULL;
            item->data        = NULL;
            item->data_length = 0;

            g_smc_skb_receive_data_buffer[i] = item;
        }
        else
        {
            SMC_TRACE_PRINTF_ASSERT("smc_create_skb_receive_data_item_buffer: no memory for the buffer");
            assert(0);
        }
   }

    SMC_UNLOCK_IRQ( get_local_lock_smc_skb_data_buffer() );
}

static inline void smc_destroy_skb_receive_data_item_buffer( void )
{
    smc_skb_receive_data_t* item = NULL;

    SMC_LOCK_IRQ( get_local_lock_smc_skb_data_buffer() );

    for(int i = 0; i < SMC_SKB_DATA_ITEM_BUFFER_SIZE; i++)
    {
        item = g_smc_skb_receive_data_buffer[i];

        item->device             = NULL;
        item->channel            = NULL;

        item->userdata.flags     = 0x00;
        item->userdata.userdata1 = 0x00;
        item->userdata.userdata2 = 0x00;
        item->userdata.userdata3 = 0x00;
        item->userdata.userdata4 = 0x00;
        item->userdata.userdata5 = 0x00;

        item->skb                = NULL;
        item->data               = NULL;
        item->data_length        = 0;

        SMC_FREE( item );

        g_smc_skb_receive_data_buffer[i] = NULL;
    }

    SMC_UNLOCK_IRQ( get_local_lock_smc_skb_data_buffer() );
}


static inline smc_skb_receive_data_t* smc_alloc_skb_receive_data_item( void )
{
    smc_skb_receive_data_t* item = NULL;

    SMC_LOCK_IRQ( get_local_lock_smc_skb_data_buffer() );

    for(int i = 0; i < SMC_SKB_DATA_ITEM_BUFFER_SIZE; i++)
    {
        item = g_smc_skb_receive_data_buffer[i];

        if( item              != NULL &&
            item->device      == NULL &&
            item->channel     == NULL &&
            item->skb         == NULL &&
            item->data        == NULL &&
            item->data_length == 0)
        {
            break;
        }
    }

    SMC_UNLOCK_IRQ( get_local_lock_smc_skb_data_buffer() );

    return item;
}

static inline void smc_free_skb_receive_data_item( smc_skb_receive_data_t* item )
{
    SMC_LOCK_IRQ( get_local_lock_smc_skb_data_buffer() );

    if( item != NULL )
    {
        item->device      = NULL;
        item->channel     = NULL;

        item->userdata.flags = 0x00;
        item->userdata.userdata1 = 0x00;
        item->userdata.userdata2 = 0x00;
        item->userdata.userdata3 = 0x00;
        item->userdata.userdata4 = 0x00;
        item->userdata.userdata5 = 0x00;

        item->skb         = NULL;
        item->data        = NULL;
        item->data_length = 0;
    }

    SMC_UNLOCK_IRQ( get_local_lock_smc_skb_data_buffer() );
}

#endif

/*
 * Start / stop lock mechanism
 */
static smc_lock_t* g_local_lock_smc_start_stop = NULL;

static inline smc_lock_t* get_local_lock_smc_start_stop(void)
{
    if( g_local_lock_smc_start_stop == NULL ) g_local_lock_smc_start_stop = smc_lock_create();
    return g_local_lock_smc_start_stop;
}

#ifdef SMC_RX_MEMORY_REALLOC_TIMER_ENABLED
    /**
     * Memory reallocation timer callback function.
     */
  static void smc_rx_mem_realloc_timer_expired(uint32_t timer_data);

#endif

/*
 * SKB RX handler that delivers the SKB to upper layers (L2MUX)
 */
static inline uint8_t smc_receive_skb_l2mux(smc_skb_receive_data_t* skb_receive_data);
static inline uint8_t smc_receive_skb_l2mux_free_mem(smc_skb_receive_data_t* skb_receive_data);

/*
 * Callback functions for SMC
 */
static void  smc_receive_data_callback_channel_l2mux(void*   data,
                                                     int32_t data_length,
                                                     const struct _smc_user_data_t* userdata,
                                                     const struct _smc_channel_t* channel);

static void  smc_event_callback_l2mux(smc_channel_t* smc_channel, SMC_CHANNEL_EVENT event, void* event_data);

/*
static void  smc_deallocator_callback_l2mux(smc_channel_t* smc_channel, void* ptr, struct _smc_user_data_t* userdata);
*/

static smc_conf_t* smc_device_create_conf_l2mux(char* device_name);

static int      l2mux_net_device_driver_ioctl(struct net_device* device, struct ifreq* ifr, int cmd);
static uint16_t l2mux_net_device_driver_select_queue( struct net_device *device, struct sk_buff *skb );
static int      l2mux_modify_send_data( struct sk_buff *skb, smc_user_data_t* smc_user_data );
static void     l2mux_layer_device_driver_setup(struct net_device* dev);

static void     l2mux_device_device_driver_close(struct net_device* dev);

/*
 * The one and only L2MUX device configuration
 */
smc_device_driver_config dev_config_l2mux =
{
    .name                    = "L2MUX",
    .queue_count             = SMC_L2MUX_QUEUE_COUNT,
    .smc_conf                = smc_device_create_conf_l2mux,
    .skb_tx_function         = l2mux_skb_tx,
    .skb_rx_function         = l2mux_skb_rx,
    .driver_ioctl            = l2mux_net_device_driver_ioctl,
    .driver_select_queue     = l2mux_net_device_driver_select_queue,
    .driver_modify_send_data = l2mux_modify_send_data,
    .driver_setup            = l2mux_layer_device_driver_setup,
    .device_driver_priv      = NULL,
    .device_driver_close     = l2mux_device_device_driver_close,
};

#if 0
/* If L2MUX is not available use these dummy functions */
/* TODO FLAG OUT */
int l2mux_skb_tx( struct sk_buff *skb, struct net_device *device )
{
    SMC_TRACE_PRINTF_DEBUG("l2mux_skb_tx: DUMMY L2MUX function invoked");
    return 0;
}

int l2mux_skb_rx( struct sk_buff *skb, struct net_device *device )
{
    SMC_TRACE_PRINTF_DEBUG("l2mux_skb_rx: DUMMY L2MUX function invoked");
    return 0;
}

#endif

/*
 * Returns the specified L2MUX Configuration initialized in the smc_instance_config_l2mux.h.
 */
smc_instance_conf_t* smc_instance_conf_get_l2mux( char* smc_user_name, char* config_name )
{
    return smc_instance_conf_get_from_list(smc_instance_conf_l2mux, SMC_CONF_COUNT_L2MUX, smc_user_name, config_name);
}

smc_t* get_smc_instance_l2mux( void )
{
    smc_t*  smc_instance = NULL;

    if( dev_config_l2mux.device_driver_priv != NULL )
    {
        SMC_TRACE_PRINTF_INFO("get_smc_instance_l2mux: SMC Net Device is configured properly, retrieving SMC instance...");
        smc_instance = dev_config_l2mux.device_driver_priv->smc_instance;
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("get_smc_instance_l2mux: SMC Net Device is not configured properly. SMC instance is not available");
    }

    SMC_TRACE_PRINTF_DEBUG("get_smc_instance_l2mux: Return the SMC Instance 0x%08X...", (uint32_t)smc_instance);
    return smc_instance;
}


/*
 * Creates configuration for SMC between CA9 and MODEM
 */
static smc_conf_t* smc_device_create_conf_l2mux(char* device_name)
{
    smc_conf_t*          smc_conf                = NULL;
    smc_channel_conf_t*  smc_channel_conf        = NULL;
    smc_instance_conf_t* smc_instance_conf       = NULL;
    int                  i                       = 0;

        /* Select the SMC configuration */

        /* TODO THE EOS2 related things must be removed */

    char* smc_cpu_name = NULL;
    uint8_t asic_version = smc_asic_version_get();
    uint8_t version_major = (( (asic_version&0xF0)>>4 )&0xFF);
    uint8_t version_minor = (asic_version&0x0F);

        /* Use EOS2 ES2.0 configuration */
    smc_cpu_name = smc_instance_conf_name_get_from_list( smc_instance_conf_l2mux, SMC_CONF_COUNT_L2MUX, SMC_CONFIG_USER_L2MUX, TRUE, version_major, version_minor);


    SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX configuration '%s' for ASIC version %d.%d (0x%02X)", device_name, smc_cpu_name, version_major, version_minor, asic_version );
    SMC_TRACE_PRINTF_STARTUP("Device '%s': PM Host Access Req %s", device_name, SMC_CONF_PM_APE_HOST_ACCESS_REQ_ENABLED?"enabled":"disabled");
    SMC_TRACE_PRINTF_STARTUP("Device '%s': APE Wakeup interrupt sense 0x%02X", device_name, SMC_APE_WAKEUP_EXTERNAL_IRQ_SENSE);

#ifdef SMC_SUPPORT_SKB_FRAGMENT_UL
    SMC_TRACE_PRINTF_STARTUP("Device '%s': UL SKB fragments supported", device_name);
#else
    SMC_TRACE_PRINTF_STARTUP("Device '%s': UL SKB fragments not supported", device_name);
#endif

#ifdef SMC_NETDEV_WAKELOCK_IN_TX
    SMC_TRACE_PRINTF_STARTUP("Device '%s': TX wakelock enabled", device_name );
#else
    SMC_TRACE_PRINTF_STARTUP("Device '%s': TX wakelock not enabled", device_name );
#endif

#ifdef SMC_APE_USE_THREADED_IRQ
    SMC_TRACE_PRINTF_STARTUP("Device '%s': RX threaded IRQ in use", device_name );
#else
    SMC_TRACE_PRINTF_STARTUP("Device '%s': RX threaded IRQ not in use", device_name );
#endif

#ifdef SMC_CHANNEL_SYNC_WAIT_ALL
    SMC_TRACE_PRINTF_STARTUP("Device '%s': Channel synchronization mode: Wait all", device_name);
#else
    SMC_TRACE_PRINTF_STARTUP("Device '%s': Channel synchronization mode: Independent", device_name);
#endif

    SMC_TRACE_PRINTF_DEBUG("smc_device_create_conf_l2mux: start...");

    smc_instance_conf = smc_instance_conf_get_l2mux( SMC_CONFIG_USER_L2MUX, smc_cpu_name );

    smc_conf = smc_conf_create_from_instance_conf( smc_cpu_name, smc_instance_conf );

    SMC_TRACE_PRINTF_DEBUG("smc_device_create_conf_l2mux: initializing callback functions for %d channels...", smc_conf->smc_channel_conf_count);

    for( i = 0; i < smc_conf->smc_channel_conf_count; i++ )
    {
        smc_channel_conf = smc_conf->smc_channel_conf_ptr_array[i];

        smc_channel_conf->smc_receive_data_cb           = (void*)smc_receive_data_callback_channel_l2mux;
        smc_channel_conf->smc_send_data_deallocator_cb  = NULL;
        smc_channel_conf->smc_receive_data_allocator_cb = NULL;
        smc_channel_conf->smc_event_cb                  = (void*)smc_event_callback_l2mux;

        if( (smc_channel_conf->wake_lock_flags&SMC_CHANNEL_WAKELOCK_TIMER) == SMC_CHANNEL_WAKELOCK_TIMER)
        {
            SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX channel %d: wakelock policy is timer, timeout %d ms", device_name, i, smc_channel_conf->wakelock_timeout_ms );
        }
        else
        {
            SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX channel %d: wakelock policy 0x%02X", device_name, i, smc_channel_conf->wake_lock_flags );
        }

#ifdef SMC_APE_USE_TASKLET_IN_IRQ
        if( smc_channel_conf->protocol == SMC_L2MUX_QUEUE_3_MHDP )
        {
            SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX channel %d: IRQ uses task", device_name, i);
        }
        else
        {
            SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX channel %d: IRQ calls CB", device_name, i);
        }
#else
        SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX channel %d: IRQ calls CB", device_name, i);
#endif
    }

#ifdef SMC_RX_USE_HIGHMEM
    SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX highmem option in use", device_name);
#else
    SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX highmem option not in use", device_name);
#endif

#ifdef SMC_RX_MEMORY_REALLOC_TIMER_ENABLED
    SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX uses memory reallocation timer", device_name);
    smc_create_skb_receive_data_item_buffer( TRUE );
#else
    SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX memory reallocation timer not used", device_name);
#endif

    SMC_TRACE_PRINTF_DEBUG("smc_device_create_conf_l2mux: completed, return SMC instance configuration 0x%08X", (uint32_t)smc_conf);

    return smc_conf;
}


#ifdef SMC_RX_MEMORY_REALLOC_TIMER_ENABLED
/**
 * Callback for timer allocating memory.
 */
static void smc_rx_mem_realloc_timer_expired(uint32_t data)
{
    if( data != 0x00000000 )
    {
        smc_timer_t*            timer            = (smc_timer_t*)data;
        smc_skb_receive_data_t* skb_receive_data = NULL;

        if( timer->timer_data == 0x00000000 )
        {
            SMC_TRACE_PRINTF_ERROR("smc_rx_mem_realloc_timer_expired: invalid timer_data pointer <NULL> inside timer");
            return;
        }

        skb_receive_data = (smc_skb_receive_data_t*)timer->timer_data;

        if( skb_receive_data != NULL )
        {
            struct net_device*        device             = NULL;
            smc_channel_t*            smc_channel        = NULL;
            struct _smc_user_data_t*  userdata           = NULL;
            struct  sk_buff*          skb                = NULL;
            void*                     data               = NULL;
            int32_t                   data_length        = 0;

            device      = skb_receive_data->device;
            smc_channel = skb_receive_data->channel;
            data        = skb_receive_data->data;
            data_length = skb_receive_data->data_length;
            userdata    = &skb_receive_data->userdata;

            if( device==NULL || smc_channel==NULL || data==NULL || userdata==NULL)
            {
                SMC_TRACE_PRINTF_ERROR("smc_rx_mem_realloc_timer_expired: skb_receive_data contains NULL data, unable to continue: device==0x%08X, smc_channel=0x%08X, data==0x%08X",
                (uint32_t)device, (uint32_t)smc_channel, (uint32_t)data);
                return;
            }

            SMC_TRACE_PRINTF_TIMER("smc_rx_mem_realloc_timer_expired: channel %d: allocate memory for data 0x%08X, len %d, userdata=0x%08X",
                    smc_channel->id, (uint32_t)data, data_length, (uint32_t)userdata);

            SMC_LOCK_IRQ( smc_channel->lock_read );

                /* First try to allocate from LOW MEM */
            skb = netdev_alloc_skb( device, data_length + SMC_L2MUX_HEADER_SIZE );

#ifdef SMC_RX_USE_HIGHMEM
            if( unlikely(!skb) )
            {
                SMC_TRACE_PRINTF_TIMER("smc_rx_mem_realloc_timer_expired: No low memory available, trying high memory...");

                skb = __netdev_alloc_skb( device, data_length + SMC_L2MUX_HEADER_SIZE, GFP_ATOMIC| __GFP_HIGHMEM );
            }
#endif
                /* Update the SKB */
            skb_receive_data->skb = skb;

            if( unlikely(!skb) )
            {
                SMC_TRACE_PRINTF_ERROR("smc_rx_mem_realloc_timer_expired: No low or high memory for RX SKB");

                if( smc_timer_start( smc_channel->rx_mem_alloc_timer,
                                     (void*)smc_rx_mem_realloc_timer_expired,
                                     (uint32_t)skb_receive_data ) != SMC_OK )
                {
                    SMC_TRACE_PRINTF_ERROR("smc_rx_mem_realloc_timer_expired: mem reallocation timer restart failed, packet dropped");
                    device->stats.rx_dropped++;
                }

                SMC_UNLOCK_IRQ( smc_channel->lock_read );
                /*
                 * Critical section ends
                 * ========================================
                 */
            }
            else
            {
                /* We have memory for SKB -> send data and open lockings */

               SMC_TRACE_PRINTF_TIMER("smc_rx_mem_realloc_timer_expired: channel %d: SKB 0x%08X created, send data (userdata: 0x%08X,0x%08X)...",
               smc_channel->id, (uint32_t)skb, userdata->flags, userdata->userdata1);

               if( smc_receive_skb_l2mux( skb_receive_data ) == SMC_OK )
               {
                   smc_device_driver_priv_t* smc_net_dev = NULL;

                   SMC_UNLOCK_IRQ( smc_channel->lock_read );
                   /*
                    * Critical section ends
                    * ========================================
                    */

                   /* SKB is modified in receive function*/
                   SMC_TRACE_PRINTF_INFO("smc_rx_mem_realloc_timer_expired: Deliver SKB (length %d) to upper layer RX ...", skb->len);
                   SMC_TRACE_PRINTF_INFO_DATA( skb->len , skb->data );

                   smc_net_dev = netdev_priv( device );
                   smc_net_dev->smc_dev_config->skb_rx_function( skb, device );
               }
               else
               {
                   SMC_UNLOCK_IRQ( smc_channel->lock_read );
                   /*
                    * Critical section ends
                    * ========================================
                    */
                   SMC_TRACE_PRINTF_ERROR("smc_rx_mem_realloc_timer_expired: smc_receive_skb_l2mux() failed");
               }

               SMC_TRACE_PRINTF_TIMER("smc_rx_mem_realloc_timer_expired: channel %d: free the skb item data 0x%08X...", smc_channel->id, (uint32_t)skb_receive_data );

               smc_free_skb_receive_data_item( skb_receive_data );

               SMC_TRACE_PRINTF_TIMER("smc_rx_mem_realloc_timer_expired: channel %d: enable receive mode", smc_channel->id );

               if( smc_channel_enable_receive_mode( smc_channel, TRUE ) == SMC_OK )
               {
                   SMC_TRACE_PRINTF_TIMER("smc_rx_mem_realloc_timer_expired: channel %d: flush channel...", smc_channel->id );
                   smc_channel_interrupt_handler(smc_channel);

                   /* --- TODO Prevent remote to send data --- */
               }
               else
               {
                   SMC_TRACE_PRINTF_ERROR("smc_rx_mem_realloc_timer_expired: channel %d: enabling receive state failed", smc_channel->id );
               }
            }
        }
        else
        {
            SMC_TRACE_PRINTF_ERROR("smc_rx_mem_realloc_timer_expired: invalid receive data pointer <NULL>");
        }
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_rx_mem_realloc_timer_expired: invalid timer data pointer <NULL>");
    }

    SMC_TRACE_PRINTF_TIMER("smc_rx_mem_realloc_timer_expired: completed");
}

#endif /* #ifdef SMC_RX_MEMORY_REALLOC_TIMER_ENABLED */

/**
 * Function to free memory of the data sent to remote host.
 */
static inline uint8_t smc_receive_skb_l2mux_free_mem(smc_skb_receive_data_t* skb_receive_data)
{
    uint8_t                   ret_val            = SMC_OK;
    struct  _smc_channel_t*   channel            = NULL;
    struct _smc_user_data_t*  userdata           = NULL;
    //struct  sk_buff*          skb                = NULL;
    void*                     data               = NULL;
    int32_t                   data_length        = 0;

    if( skb_receive_data == NULL )
    {
        SMC_TRACE_PRINTF_ERROR("smc_receive_skb_l2mux_free_mem: skb_receive_data is NULL");
        return SMC_ERROR;
    }

    /*device      = skb_receive_data->device;*/
    channel     = skb_receive_data->channel;
    /*skb         = skb_receive_data->skb;*/
    data        = skb_receive_data->data;
    data_length = skb_receive_data->data_length;
    userdata    = &skb_receive_data->userdata;

    if( channel==NULL || data==NULL || userdata==NULL)
    {
        SMC_TRACE_PRINTF_ERROR("smc_receive_skb_l2mux_free_mem: skb_receive_data contains NULL data, channel=0x%08X, data=0x%08X, userdata=0x%08X",
        (uint32_t)channel, (uint32_t)data, (uint32_t)userdata);
        return SMC_ERROR;
    }

    if( SMC_COPY_SCHEME_RECEIVE_IS_COPY( channel->copy_scheme ) )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_receive_skb_l2mux_free_mem: free the original data 0x%08X from local mem", (uint32_t)data);
        smc_channel_free_ptr_local( channel, data, userdata );
    }
    else
    {
        smc_user_data_t userdata_free;

        userdata_free.flags     = SMC_MSG_FLAG_FREE_MEM_MDB;
        userdata_free.userdata1 = userdata->userdata1;
        userdata_free.userdata2 = userdata->userdata2;
        userdata_free.userdata3 = userdata->userdata3;
        userdata_free.userdata4 = userdata->userdata4;
        userdata_free.userdata5 = userdata->userdata5;

        SMC_TRACE_PRINTF_DEBUG("smc_receive_skb_l2mux_free_mem: free the original data 0x%08X from SHM...", (uint32_t)data);

            /* Free the MDB SHM data PTR from remote */
        if( smc_send_ext( channel, data, 0, &userdata_free) != SMC_OK )
        {
            SMC_TRACE_PRINTF_ERROR("smc_receive_skb_l2mux_free_mem: channel %d: MDB memory 0x%08X free from remote failed",
                    channel->id, (uint32_t)data);
        }
    }

    return ret_val;
}


/**
 * SKB RX handler.
 */
static inline uint8_t smc_receive_skb_l2mux( smc_skb_receive_data_t* skb_receive_data )
{
    uint8_t                   ret_val            = SMC_OK;
    char*                     skb_data_buffer    = NULL;
    struct                    net_device* device = NULL;
    struct  _smc_channel_t*   channel            = NULL;
    struct _smc_user_data_t*  userdata           = NULL;
    struct  sk_buff*          skb                = NULL;
    void*                     data               = NULL;
    int32_t                   data_length        = 0;

    if( skb_receive_data == NULL )
    {
        SMC_TRACE_PRINTF_ERROR("smc_receive_skb_l2mux: skb_receive_data is NULL");
        return SMC_ERROR;
    }

    device      = skb_receive_data->device;
    channel     = skb_receive_data->channel;
    skb         = skb_receive_data->skb;
    data        = skb_receive_data->data;
    data_length = skb_receive_data->data_length;
    userdata    = &skb_receive_data->userdata;

    if( device==NULL || channel==NULL || data==NULL || userdata==NULL || skb==NULL)
    {
        SMC_TRACE_PRINTF_ERROR("smc_receive_skb_l2mux: skb_receive_data contains NULL data: device==0x%08X, channel==0x%08X, data==0x%08X, userdata==0x%08X, skb==0x%08X",
            (uint32_t)device, (uint32_t)channel, (uint32_t)data, (uint32_t)userdata, (uint32_t)skb);
        return SMC_ERROR;
    }

    skb_data_buffer = skb_put(skb, data_length);

#ifdef SMC_DMA_TRANSFER_ENABLED
    if( SMC_COPY_SCHEME_RECEIVE_USE_DMA( channel->copy_scheme ) )
    {
#if 0   /* Not in use yet */
        SMC_TRACE_PRINTF_DMA("smc_receive_skb_l2mux: DMA copy message data 0x%08X to SKB buffer 0x%08X (0x%08X)...",
                                                    (uint32_t)data, (uint32_t)skb->data, (uint32_t)skb_data_buffer);

        if( channel->smc_dma != NULL )
        {
            /*
             * Start the DMA transfer
             * NOTE: Currently NO DMA copy implementation, just use memcpy
             */
            SMC_TRACE_PRINTF_DMA("smc_receive_skb_l2mux: DMA copy by SMC DMA transfer API...");

            int dma_ret_val = smc_dma_transfer_mdb(channel->smc_dma, skb_data_buffer, SMC_MEMORY_VIRTUAL_TO_PHYSICAL( channel->smc_instance, data ), data_length, 0x01);

            if( dma_ret_val != SMC_DRIVER_OK )
            {
                SMC_TRACE_PRINTF_ERROR("smc_receive_skb_l2mux: DMA copy from 0x%08X to SKB buffer 0x%08X (0x%08X) FAILED",
                                                                                (uint32_t)data, (uint32_t)skb->data, (uint32_t)skb_data_buffer);
            }
        }
        else
#endif
        {
            memcpy( skb_data_buffer, data, data_length);
        }
    }
    else
#endif
    {
        SMC_TRACE_PRINTF_DEBUG("smc_receive_skb_l2mux: memcpy message data 0x%08X to SKB buffer 0x%08X (0x%08X)...",
                                (uint32_t)data, (uint32_t)skb->data, (uint32_t)skb_data_buffer);
        memcpy( skb_data_buffer, data, data_length);
    }

        /* Ensure the cache cleanup */
    SMC_TRACE_PRINTF_DEBUG("smc_receive_skb_l2mux clean 0x%08X->0x%08X (len %d)",
            (uint32_t)skb_data_buffer, (uint32_t)(skb_data_buffer+data_length), data_length );

    __raw_readl( ((void __iomem *)(skb_data_buffer+data_length)) );

    SMC_TRACE_PRINTF_DEBUG("smc_receive_skb_l2mux: free the original data 0x%08X", (uint32_t)data);

    if( smc_receive_skb_l2mux_free_mem( skb_receive_data ) != SMC_OK )
    {
        SMC_TRACE_PRINTF_ASSERT("smc_receive_skb_l2mux: smc_receive_skb_l2mux_free_mem failed");
        assert(0);
    }

    skb_push(skb, SMC_L2MUX_HEADER_SIZE);

    SMC_TRACE_PRINTF_INFO("smc_receive_skb_l2mux: Push L2MUX header 0x%08X into the SKB 0x%08X",
            userdata->userdata1, (uint32_t)skb->data);

    *(uint32_t*)(skb->data) = userdata->userdata1;

        /* Put the metadata */
    skb->dev        = device;
    skb->ip_summed  = CHECKSUM_UNNECESSARY;

    device->stats.rx_packets++;

    SMC_TRACE_PRINTF_INFO("smc_receive_skb_l2mux: completed by return value %d", ret_val);
    return ret_val;
}

/**
 * Callback function for data receiving.
 */
static void  smc_receive_data_callback_channel_l2mux(void*   data,
                                                     int32_t data_length,
                                                     const struct _smc_user_data_t* userdata,
                                                     const struct _smc_channel_t* channel)
{
    struct net_device* device = NULL;

    assert( channel!=NULL );

    SMC_TRACE_PRINTF_INFO("smc_receive_data_callback_channel_l2mux: Data 0x%08X from SMC 0x%08X: channel %d, len %d",
                            (uint32_t)data, (uint32_t)channel->smc_instance, channel->id, data_length);
    SMC_TRACE_PRINTF_INFO_DATA(data_length, data);

    device = dev_config_l2mux.device_driver_priv->net_dev;

    if( device != NULL )
    {
        SMC_TRACE_PRINTF_INFO("smc_receive_data_callback_channel_l2mux: net device is 0x%08X", (uint32_t)device);

        if( unlikely( !netif_running(device) ) )
        {
            SMC_TRACE_PRINTF_WARNING("smc_receive_data_callback_channel_l2mux: Device 0x%08X not running, drop RX packet", (uint32_t)device);
            device->stats.rx_dropped++;
        }
        else
        {
            struct sk_buff         *skb = NULL;

#ifndef SMC_APE_USE_THREADED_IRQ
            /* ========================================
             * Critical section begins
             *
             */
            SMC_LOCK( channel->lock_read );
            skb = netdev_alloc_skb( device, data_length + SMC_L2MUX_HEADER_SIZE );
#else
            skb = __netdev_alloc_skb( device, data_length + SMC_L2MUX_HEADER_SIZE, GFP_KERNEL | __GFP_HIGH );
#endif



#ifdef SMC_RX_USE_HIGHMEM
            if( unlikely(!skb) )
            {
                SMC_TRACE_PRINTF_DEBUG("smc_receive_data_callback_channel_l2mux: No low memory available, trying high memory...");

                    /* Try to allocate from HIGHMEM MEM */
  #ifndef SMC_APE_USE_THREADED_IRQ
                skb = __netdev_alloc_skb( device, data_length + SMC_L2MUX_HEADER_SIZE, GFP_ATOMIC | __GFP_HIGHMEM );
  #else
                skb = __netdev_alloc_skb( device, data_length + SMC_L2MUX_HEADER_SIZE, GFP_KERNEL | __GFP_HIGH | __GFP_HIGHMEM );
  #endif
            }
#endif  /* #ifdef SMC_RX_USE_HIGHMEM */


#ifdef SMC_APE_USE_THREADED_IRQ
            /* ========================================================
             * Critical section begins
             * If threaded IRQ -> the lock is here after the mem alloc
             */
            SMC_LOCK_RX_THREADED_IRQ( channel->lock_read );
#endif

            if( unlikely(!skb) )
            {
#ifdef SMC_RX_MEMORY_REALLOC_TIMER_ENABLED
                    /* TODO Check that this is locked */
                smc_skb_receive_data_t* skb_rcv_ptr = smc_alloc_skb_receive_data_item();

                if( skb_rcv_ptr != NULL )
                {
                    skb_rcv_ptr->device      = device;
                    skb_rcv_ptr->channel     = (smc_channel_t*)channel;
                    skb_rcv_ptr->userdata    = *(smc_user_data_t*)userdata;
                    skb_rcv_ptr->skb         = skb;
                    skb_rcv_ptr->data        = data;
                    skb_rcv_ptr->data_length = data_length;
                }
                else
                {
                    SMC_TRACE_PRINTF_ERROR("smc_receive_data_callback_channel_l2mux: No FREE BLOCKS for RX SKB");
                }

                SMC_TRACE_PRINTF_TIMER("smc_receive_data_callback_channel_l2mux: start memory reallocation timer, data=0x%08X len %d, userdata 0x%08X: 0x%08X,0x%08X...",
                (uint32_t)data, data_length, (uint32_t)userdata, userdata->flags, userdata->userdata1);

                if( skb_rcv_ptr == NULL || channel->rx_mem_alloc_timer == NULL ||
                    smc_timer_start( channel->rx_mem_alloc_timer,
                                    (void*)smc_rx_mem_realloc_timer_expired,
                                    (uint32_t)skb_rcv_ptr ) != SMC_OK )
                {
                    SMC_TRACE_PRINTF_TIMER("smc_receive_data_callback_channel_l2mux: memory reallocation timer not started");

                    if( smc_receive_skb_l2mux_free_mem( skb_rcv_ptr ) != SMC_OK )
                    {
                        SMC_TRACE_PRINTF_ASSERT("smc_receive_data_callback_channel_l2mux: smc_receive_skb_l2mux_free_mem failed");
                        assert(0);
                    }

                    smc_free_skb_receive_data_item( skb_rcv_ptr );

                    SMC_TRACE_PRINTF_ERROR("smc_receive_data_callback_channel_l2mux: No low or high memory for RX SKB");
                    device->stats.rx_dropped++;
                }
                else
                {
                    smc_channel_enable_receive_mode( (smc_channel_t*)channel, FALSE );

                    SMC_TRACE_PRINTF_TIMER("smc_receive_data_callback_channel_l2mux: memory reallocation timer started, receive locked");

                    /* --- TODO Prevent remote to send data --- */
                }
#else   /* #ifdef SMC_RX_MEMORY_REALLOC_TIMER_ENABLED */
                smc_skb_receive_data_t skb_receive_data;

                skb_receive_data.device      = device;
                skb_receive_data.channel     = (smc_channel_t*)channel;
                skb_receive_data.userdata    = *(smc_user_data_t*)userdata;
                skb_receive_data.skb         = NULL;
                skb_receive_data.data        = data;
                skb_receive_data.data_length = data_length;

                smc_receive_skb_l2mux_free_mem( &skb_receive_data );

                SMC_TRACE_PRINTF_ERROR("smc_receive_data_callback_channel_l2mux: No low or high memory for RX SKB");
                device->stats.rx_dropped++;
#endif  /* #ifdef SMC_RX_MEMORY_REALLOC_TIMER_ENABLED */

#ifdef SMC_APE_USE_THREADED_IRQ
                SMC_UNLOCK_RX_THREADED_IRQ( channel->lock_read );
#else
                SMC_UNLOCK( channel->lock_read );
#endif
                /*
                 * Critical section ends
                 * ========================================
                 */
            }
            else
            {
                smc_skb_receive_data_t skb_receive_data;

                skb_receive_data.device      = device;
                skb_receive_data.channel     = (smc_channel_t*)channel;
                skb_receive_data.userdata    = *(smc_user_data_t*)userdata;
                skb_receive_data.skb         = skb;
                skb_receive_data.data        = data;
                skb_receive_data.data_length = data_length;

                if( smc_receive_skb_l2mux( &skb_receive_data ) == SMC_OK )
                {
                    smc_device_driver_priv_t* smc_net_dev        = NULL;

#ifdef SMC_APE_USE_THREADED_IRQ
                    SMC_UNLOCK_RX_THREADED_IRQ( channel->lock_read );
#else

                    SMC_UNLOCK( channel->lock_read );
#endif
                    /*
                     * Critical section ends
                     * ========================================
                     */

                    /* SKB is modified in receive function*/
                    SMC_TRACE_PRINTF_INFO("smc_receive_data_callback_channel_l2mux: Deliver SKB (length %d) to upper layer RX ...", skb->len);
                    SMC_TRACE_PRINTF_INFO_DATA( skb->len , skb->data );

                    smc_net_dev = netdev_priv( device );
                    smc_net_dev->smc_dev_config->skb_rx_function( skb, device );
                }
                else
                {
#ifdef SMC_APE_USE_THREADED_IRQ
                    SMC_UNLOCK_RX_THREADED_IRQ( channel->lock_read );
#else
                    SMC_UNLOCK( channel->lock_read );
#endif
                    /*
                     * Critical section ends
                     * ========================================
                     */
                    SMC_TRACE_PRINTF_ERROR("smc_receive_data_callback_channel_l2mux: smc_receive_skb_l2mux() failed");
                }
            }
        }
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_receive_data_callback_channel_l2mux: unable to get proper net device");
    }

    SMC_TRACE_PRINTF_DEBUG("smc_receive_data_callback_channel_l2mux: completed");
}

/**
 * SMC event callback handler function
 */
static void smc_event_callback_l2mux(smc_channel_t* smc_channel, SMC_CHANNEL_EVENT event, void* event_data)
{
    assert(smc_channel != NULL );

    switch(event)
    {
        case SMC_CHANNEL_READY_TO_SEND:
        {
            SMC_TRACE_PRINTF_EVENT_RECEIVED("smc_event_callback_l2mux: channel id %d: SMC_CHANNEL_READY_TO_SEND", smc_channel->id);

            SMC_TRACE_PRINTF_STARTUP("Channel %d is connected with the remote", smc_channel->id);

            break;
        }
        case SMC_STOP_SEND:
        case SMC_STOP_SEND_LOCAL:
        {
            struct net_device* device     = NULL;

            SMC_TRACE_PRINTF_EVENT_RECEIVED("smc_event_callback_l2mux: channel id %d: SMC_STOP_SEND: queue protocol %d", smc_channel->id, smc_channel->protocol);

                /* Get the net device and close */
            device = dev_config_l2mux.device_driver_priv->net_dev;

            if( device != NULL )
            {
                struct netdev_queue* queue     = NULL;
                uint16_t             queue_ind = smc_channel->protocol;

                if( event == SMC_STOP_SEND )
                {
                    SMC_CHANNEL_STATE_SET_STOP_SEND_FROM_REMOTE( smc_channel->state );
                }

                SMC_CHANNEL_STATE_SET_SEND_IS_DISABLED( smc_channel->state );

                SMC_TRACE_PRINTF_DEBUG("smc_event_callback_l2mux: channel id %d: netif_tx_stop_queue %d...", smc_channel->id, queue_ind);

                /* Get the QUEUE and stop it */
                queue = netdev_get_tx_queue(device, queue_ind);

                netif_stop_subqueue(device, queue_ind);

                //SMC_TRACE_PRINTF_ALWAYS("Channel %d: TX is disabled (from %s)", smc_channel->id, (event == SMC_STOP_SEND)?"remote":"local");
            }
            else
            {
                SMC_TRACE_PRINTF_ASSERT("mc_event_callback_l2mux: channel id %d: Net device not found", smc_channel->id);
                assert(0);
            }

            break;
        }
        case SMC_RESUME_SEND:
        case SMC_RESUME_SEND_LOCAL:
        {
            struct net_device* device     = NULL;

            SMC_TRACE_PRINTF_EVENT_RECEIVED("smc_event_callback_l2mux: channel id %d: SMC_RESUME_SEND: queue protocol %d", smc_channel->id, smc_channel->protocol);

            if( event == SMC_RESUME_SEND )
            {
                SMC_CHANNEL_STATE_CLEAR_STOP_SEND_FROM_REMOTE( smc_channel->state );
            }

            /* Check that the channel state allows the TX opening */

            if( SMC_CHANNEL_STATE_ALLOW_RESUME_SEND( smc_channel->state )  )
            {
                    /* Get the net device and close */
                device = dev_config_l2mux.device_driver_priv->net_dev;

                if( device != NULL )
                {
                    struct netdev_queue* queue     = NULL;
                    uint16_t             queue_ind = smc_channel->protocol;

                    SMC_TRACE_PRINTF_DEBUG("smc_event_callback_l2mux: channel id %d: netif_tx_start_queue %d...", smc_channel->id, queue_ind);

                    queue = netdev_get_tx_queue(device, queue_ind);

                    netif_wake_subqueue(device, queue_ind);

                    SMC_CHANNEL_STATE_CLEAR_SEND_IS_DISABLED( smc_channel->state );
                }
                else
                {
                    SMC_TRACE_PRINTF_ASSERT("mc_event_callback_l2mux: channel id %d: Net device not found", smc_channel->id);
                    assert(0);
                }
            }
            else
            {
                SMC_TRACE_PRINTF_EVENT_RECEIVED("smc_event_callback_l2mux: channel %d: not starting queue %d (denied)", smc_channel->id, smc_channel->protocol);
            }

            break;
        }
        case SMC_VERSION_INFO_REMOTE:
        {
            uint32_t version           = *(uint32_t*)event_data;
            char*    str_version       = smc_version_to_str(version);
            char*    str_version_local = smc_get_version();
            uint32_t version_local     = smc_version_to_int( str_version_local );

            if( version_local <= 0 )
            {
                version_local = version_local;  /* Remove warning */
            }

            SMC_TRACE_PRINTF_EVENT_RECEIVED("smc_l2mux_event_handler: channel id %d: SMC_VERSION_INFO_REMOTE, version is 0x%08X -> v.%s, local version: 0x%08X -> v.%s",
                    smc_channel->id, version, str_version, version_local, str_version_local);

            SMC_TRACE_PRINTF_STARTUP("Channel %d version info: local v.%s, remote v.%s", smc_channel->id,str_version_local, str_version);

            /* TODO Implement version compare and decide if it is possible to communicate */

            SMC_FREE( str_version );

            break;
        }
        default:
        {
            SMC_TRACE_PRINTF_EVENT_RECEIVED("smc_event_callback_l2mux: channel id %d: event %d, no actions", smc_channel->id, event);
        }
    }
}

static int l2mux_net_device_driver_ioctl(struct net_device* device, struct ifreq* ifr, int cmd)
{
    int ret_val = SMC_DRIVER_OK;

    SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: starts device 0x%08X, command %d (0x%08X)...", (uint32_t)device, cmd, cmd);

    switch(cmd)
    {
        case SIOCPNGAUTOCONF:
        {
            struct if_phonet_req *req = (struct if_phonet_req *)ifr;
            uint8_t address = 0x00;

            SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCPNGAUTOCONF");

            req->ifr_phonet_autoconf.device = PN_DEV_HOST;

            address = 0x60;
            SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCPNGAUTOCONF: Add route 0x%02X...", address);
            phonet_route_add(device, address);

            address = 0x44;
            SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCPNGAUTOCONF: Add route 0x%02X...", address);
            phonet_route_add(device, address);

            address = 0x64;
            SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCPNGAUTOCONF: Add route 0x%02X...", address);
            phonet_route_add(device, address);

            break;
        }
        case SIOCCONFIGTYPE:
        {
            SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCCONFIGTYPE");
            break;
        }
        case SIOCCONFIGSUBTYPE:
        {
            SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCCONFIGSUBTYPE");
            break;
        }
        case SIOCPNGETOBJECT:
        {
            SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCPNGETOBJECT");
            break;
        }
#if(SMCTEST==TRUE)
        case SIOCDEV_RUN_TEST:
        {
            extern void set_smc_device_driver_priv(smc_device_driver_priv_t* smc_net_dev_priv);

            struct ifreq_smc_test* if_req_smc_test = (struct ifreq_smc_test *)ifr;
            smc_device_driver_priv_t* smc_net_dev  = NULL;

            SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCDEV_RUN_TEST: test case 0x%02X, test data length = %d, test data 0x%08X",
                    if_req_smc_test->if_test_case ,if_req_smc_test->if_test_data_len, (uint32_t)if_req_smc_test->if_test_data);

            if(if_req_smc_test->if_test_data_len > 0 && if_req_smc_test->if_test_data != NULL )
            {
                SMC_TRACE_PRINTF_DEBUG_DATA( if_req_smc_test->if_test_data_len, if_req_smc_test->if_test_data );
            }
            else
            {
                SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCDEV_RUN_TEST: No test input data found");
            }

            smc_net_dev = netdev_priv(device);

            set_smc_device_driver_priv( smc_net_dev );

            if( smc_test_handler_start(if_req_smc_test->if_test_case, if_req_smc_test->if_test_data_len, if_req_smc_test->if_test_data) == SMC_OK )
            {
                SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCDEV_RUN_TEST: test 0x%02X OK", if_req_smc_test->if_test_case);

                if_req_smc_test->if_test_result = 0xABCD;
                ret_val = SMC_DRIVER_OK;
            }
            else
            {
                SMC_TRACE_PRINTF_ERROR("l2mux_net_device_driver_ioctl: SIOCDEV_RUN_TEST: test 0x%02X FAILED", if_req_smc_test->if_test_case);
                if_req_smc_test->if_test_result = 0xCDEF;
                ret_val = SMC_DRIVER_OK;
            }

            break;
        }
#endif
        default:
        {
            SMC_TRACE_PRINTF_WARNING("l2mux_net_device_driver_ioctl: unsupported ioctl command 0x%04X", cmd);
            break;
        }
    }

    SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: completed by return value %d", ret_val);

    return ret_val;
}

static uint16_t l2mux_net_device_driver_select_queue( struct net_device *device, struct sk_buff *skb )
{
    uint16_t subqueue = 0;

    SMC_TRACE_PRINTF_TRANSMIT("l2mux_net_device_driver_select_queue: device 0x%08X, protocol 0x%04X...", (uint32_t)device, skb->protocol);

    if (skb->protocol == htons(ETH_P_PHONET))
    {
        SMC_TRACE_PRINTF_TRANSMIT("l2mux_net_device_driver_select_queue: protocol ETH_P_PHONET");
        subqueue = SMC_L2MUX_QUEUE_1_PHONET;
    }
    else if (skb->protocol == htons(ETH_P_MHI))
    {
        SMC_TRACE_PRINTF_TRANSMIT("l2mux_net_device_driver_select_queue: protocol ETH_P_MHI");
        subqueue = SMC_L2MUX_QUEUE_2_MHI;
    }
    else if (skb->protocol == htons(ETH_P_MHDP))
    {
        SMC_TRACE_PRINTF_TRANSMIT("l2mux_net_device_driver_select_queue: protocol ETH_P_MHDP");
        subqueue = SMC_L2MUX_QUEUE_3_MHDP;
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("l2mux_net_device_driver_select_queue: unsupported protocol device 0x%08X, 0x%04X", (uint32_t)device, skb->protocol);
    }

    SMC_TRACE_PRINTF_TRANSMIT("l2mux_net_device_driver_select_queue: completed: subqueue 0x%04X", subqueue);

    return subqueue;
}

static int l2mux_modify_send_data( struct sk_buff *skb, smc_user_data_t* smc_user_data )
{
    SMC_TRACE_PRINTF_TRANSMIT("l2mux_modify_send_data: get L2MUX header from SKB and put it into SMC user data...");

    smc_user_data->userdata1 = *(uint32_t*)skb->data;

    skb_pull(skb, SMC_L2MUX_HEADER_SIZE);

    return SMC_DRIVER_OK;
}

static void l2mux_layer_device_driver_setup(struct net_device* device)
{
    SMC_TRACE_PRINTF_STM("l2mux_layer_device_driver_setup: modify net device for L2MUX usage...");

#ifdef SMC_SUPPORT_SKB_FRAGMENT_UL
    device->features        = NETIF_F_SG | NETIF_F_FRAGLIST | NETIF_F_GEN_CSUM /* | NETIF_F_HW_CSUM */ ;
#else
    device->features        = NETIF_F_SG;
#endif

    device->type            = ARPHRD_MHI;
    device->flags           = IFF_POINTOPOINT | IFF_NOARP;
    device->mtu             = MHI_MAX_MTU;
    device->hard_header_len = 4;
    device->dev_addr[0]     = PN_MEDIA_MODEM_HOST_IF;
    device->addr_len        = 1;
    device->tx_queue_len    = 500;

}

static void l2mux_device_device_driver_close(struct net_device* device)
{
    SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX device driver is closing", device->name);

#ifdef SMC_RX_MEMORY_REALLOC_TIMER_ENABLED

    smc_destroy_skb_receive_data_item_buffer();
#endif


    SMC_TRACE_PRINTF_STARTUP("Device '%s': L2MUX device driver is closed", device->name);
}


/* EOF */
