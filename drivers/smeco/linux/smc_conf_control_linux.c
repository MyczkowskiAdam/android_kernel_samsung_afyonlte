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

Version:       1    25-Jan-2012     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif


#include "smc_common_includes.h"

#include "smc.h"
#include "smc_trace.h"

#include "smc_linux.h"
#include "smc_config_control.h"

#if( SMC_CONTROL_USE_PHONET == TRUE)
  #include <linux/phonet.h>
  #include <net/phonet/pn_dev.h>

  #define SMC_CONTROL_PHONET_ROUTE_DEVICE  0x77

  #ifndef PN_DEV_HOST
    #define PN_DEV_HOST 0x00
  #endif
#endif

#if( SMCTEST == TRUE )
  #include "smc_test.h"
#endif


#define SMC_CONTROL_QUEUE_COUNT 1

/*
 * Callback functions for SMC
 */
static void  smc_receive_data_callback_channel_control(void*   data,
                                                      int32_t data_length,
                                                      const struct _smc_user_data_t* userdata,
                                                      const struct _smc_channel_t* channel);

static void  smc_deallocator_callback_control(smc_channel_t* smc_channel, void* ptr, uint32_t userdata);
static void* smc_allocator_callback_control(smc_channel_t* smc_channel, uint32_t size, uint32_t userdata);
static void  smc_event_callback_control(smc_channel_t* smc_channel, SMC_CHANNEL_EVENT event, void* event_data);


static smc_conf_t* smc_device_create_conf_control(char* device_name);

static int      smc_control_net_device_driver_ioctl(struct net_device* device, struct ifreq* ifr, int cmd);
static uint16_t smc_control_net_device_driver_select_queue( struct net_device *device, struct sk_buff *skb );
static int      smc_control_modify_send_data( struct sk_buff *skb, smc_user_data_t* smc_user_data );
static void     smc_control_layer_device_driver_setup(struct net_device* dev);

/*
 * The one and only L2MUX device configuration
 */
smc_device_driver_config dev_config_control =
{
    .name                    = "SMC_CONTROL",
    .queue_count             = SMC_CONTROL_QUEUE_COUNT,
    .smc_conf                = smc_device_create_conf_control,
    .skb_tx_function         = NULL,
    .skb_rx_function         = NULL,
    .driver_ioctl            = smc_control_net_device_driver_ioctl,
    .driver_select_queue     = smc_control_net_device_driver_select_queue,
    .driver_modify_send_data = smc_control_modify_send_data,
    .driver_setup            = smc_control_layer_device_driver_setup,
    .device_driver_priv      = NULL,
    .device_driver_close     = NULL,
};



/*
 * Returns pointer to the SMC Control instance.
 */
smc_t* smc_instance_get_control( void )
{
    smc_t*  smc_instance_control = NULL;

    if( dev_config_control.device_driver_priv != NULL )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_instance_get_control: SMC Net Device is configured properly, retrieving SMC Control instance...");
        smc_instance_control = dev_config_control.device_driver_priv->smc_instance;
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_instance_get_control: SMC Net Device is not configured properly. SMC Control instance is not available");
    }

    SMC_TRACE_PRINTF_DEBUG("smc_instance_get_control: Return the SMC Control Instance 0x%08X...", (uint32_t)smc_instance_control);
    return smc_instance_control;
}


/*
 * Returns the specified SMC Control Configuration initialized in the smc_instance_config_control.h.
 */
smc_instance_conf_t* smc_instance_conf_get_control( char* smc_user_name, char* config_name )
{
    return smc_instance_conf_get_from_list(smc_instance_conf_control, SMC_CONF_COUNT_CONTROL, smc_user_name, config_name);
}

/*
 * Creates configuration for SMC between CA9 and MODEM
 */
static smc_conf_t* smc_device_create_conf_control(char* device_name)
{
    smc_conf_t*          smc_conf                = NULL;
    smc_channel_conf_t*  smc_channel_conf        = NULL;
    smc_instance_conf_t* smc_instance_conf       = NULL;
    int                  i                       = 0;

        /* Select the SMC configuration */
    char* smc_cpu_name = NULL;
    uint16_t asic_version = smc_asic_version_get();

    smc_cpu_name = smc_instance_conf_name_get_from_list( smc_instance_conf_control, SMC_CONF_COUNT_CONTROL, SMC_CONFIG_USER_CONTROL, TRUE, 2, 0);

    SMC_TRACE_PRINTF_STARTUP("Control configuration '%s' for ASIC version 0x%02X", smc_cpu_name, asic_version);

    smc_instance_conf = smc_instance_conf_get_control( SMC_CONFIG_USER_CONTROL, smc_cpu_name );

    smc_conf = smc_conf_create_from_instance_conf( smc_cpu_name, smc_instance_conf );

    SMC_TRACE_PRINTF_DEBUG("smc_device_create_conf_control: initializing callback functions for %d channels...", smc_conf->smc_channel_conf_count);

    for( i = 0; i < smc_conf->smc_channel_conf_count; i++ )
    {
        smc_channel_conf = smc_conf->smc_channel_conf_ptr_array[i];

        smc_channel_conf->smc_receive_data_cb           = (void*)smc_receive_data_callback_channel_control;
        smc_channel_conf->smc_send_data_deallocator_cb  = (void*)smc_deallocator_callback_control;
        smc_channel_conf->smc_receive_data_allocator_cb = (void*)smc_allocator_callback_control;
        smc_channel_conf->smc_event_cb                  = (void*)smc_event_callback_control;
    }

    SMC_TRACE_PRINTF_DEBUG("smc_device_create_conf_control: completed, return SMC instance configuration 0x%08X", (uint32_t)smc_conf);

    return smc_conf;
}

static void  smc_receive_data_callback_channel_control(void*   data,
                                                      int32_t data_length,
                                                      const struct _smc_user_data_t* userdata,
                                                      const struct _smc_channel_t* channel)
{
    struct net_device* device = NULL;

    assert( channel!=NULL );

    SMC_TRACE_PRINTF_DEBUG("smc_receive_data_callback_channel_control: Data 0x%08X from SMC 0x%08X: channel %d, len %d",
                            (uint32_t)data, (uint32_t)channel->smc_instance, channel->id, data_length);

    device = dev_config_control.device_driver_priv->net_dev;

    if( device != NULL )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_receive_data_callback_channel_control: net device is 0x%08X", (uint32_t)device);

        if (unlikely( !netif_running(device) ) )
        {
            SMC_TRACE_PRINTF_WARNING("smc_receive_data_callback_channel_control: Device 0x%08X not running, drop RX packet", (uint32_t)device);
            device->stats.rx_dropped++;
        }
        else
        {
            smc_device_driver_priv_t* smc_net_dev = NULL;

            smc_net_dev = netdev_priv( device );

            if( smc_net_dev->smc_dev_config->skb_rx_function != NULL )
            {
                struct sk_buff *skb = NULL;

                skb = netdev_alloc_skb( device, data_length );

                if( unlikely(!skb) )
                {
                    SMC_TRACE_PRINTF_ERROR("smc_receive_data_callback_channel_control: No memory for RX skb");
                    device->stats.rx_dropped++;
                }
                else
                {
                    skb->dev = device;

                    /* TODO Build the SKB based on device type */
                    SMC_TRACE_PRINTF_DEBUG("smc_receive_data_callback_channel_control: Push L2MUX header 0x%08X into the SKB 0x%08X",
                            userdata->userdata1, (uint32_t)skb->data);

                    *(uint32_t*)(uint32_t)(skb->data) = (uint32_t)userdata->userdata1;

                    SMC_TRACE_PRINTF_ERROR("smc_receive_data_callback_channel_control: Deliver SKB to upper layer RX ...");
                    SMC_TRACE_PRINTF_DEBUG_DATA( skb->len , skb->data );

                    smc_net_dev->smc_dev_config->skb_rx_function( skb, device );
                }
            }
            else
            {
                SMC_TRACE_PRINTF_DEBUG("smc_receive_data_callback_channel_control: no RX function");

                /* TODO Handle the control packet if in here */
            }
        }
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_receive_data_callback_channel_control: unable to get proper net device");
    }
}

static void  smc_deallocator_callback_control(smc_channel_t* smc_channel, void* ptr, uint32_t userdata)
{
    SMC_TRACE_PRINTF_DEBUG("smc_deallocator_callback_control: SMC channel 0x%08X, ptr 0x%08X (NOT IMPLEMENTED)",
    (uint32_t)smc_channel, (uint32_t)ptr);

}

static void* smc_allocator_callback_control(smc_channel_t* smc_channel, uint32_t size, uint32_t userdata)
{
    SMC_TRACE_PRINTF_DEBUG("smc_allocator_callback_control: SMC channel 0x%08X, size %d (NOT IMPLEMENTED)", (uint32_t)smc_channel, size);

    return NULL;
}

static void  smc_event_callback_control(smc_channel_t* smc_channel, SMC_CHANNEL_EVENT event, void* event_data)
{
    SMC_TRACE_PRINTF_DEBUG("smc_event_callback_control: SMC channel 0x%08X, event %d", (uint32_t)smc_channel, event);

}

static int smc_control_net_device_driver_ioctl(struct net_device* device, struct ifreq* ifr, int cmd)
{
    int ret_val = SMC_DRIVER_OK;

    SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_ioctl: starts device 0x%08X, command %d (0x%08X)...", (uint32_t)device, cmd, cmd);

    switch(cmd)
    {
#if( SMC_CONTROL_USE_PHONET == TRUE)
        case SIOCPNGAUTOCONF:
        {
            struct if_phonet_req *req = (struct if_phonet_req *)ifr;
            int iRet = 0;

            SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_ioctl: SIOCPNGAUTOCONF add route 0x%02X", SMC_CONTROL_PHONET_ROUTE_DEVICE);

            req->ifr_phonet_autoconf.device = PN_DEV_HOST;

            iRet = phonet_route_add(device, SMC_CONTROL_PHONET_ROUTE_DEVICE);

            if( iRet )
            {
                SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_ioctl: SIOCPNGAUTOCONF failed (%d)", iRet);
            }

            break;
        }
        /*
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
        */
        case SIOCPNGETOBJECT:
        {
            SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_ioctl: SIOCPNGETOBJECT");
            break;
        }
#endif
        case SIOCDEV_SEND_DATA:
        {
            struct ifreq_smc* if_req_smc = (struct ifreq_smc *)ifr;

            SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_ioctl: SIOCDEV_SEND_DATA: Data length = %d, data 0x%08X", if_req_smc->if_data_len, (uint32_t)if_req_smc->if_data);

            if(if_req_smc->if_data_len > 0 && if_req_smc->if_data != NULL )
            {
                SMC_TRACE_PRINTF_DEBUG_DATA(if_req_smc->if_data_len, if_req_smc->if_data);
            }
            else
            {
                SMC_TRACE_PRINTF_DEBUG("l2mux_net_device_driver_ioctl: SIOCDEV_SEND_DATA: NO Data available");
            }

            break;
        }
        case SIOCDEV_RUN_TEST:
        {
#if(SMCTEST==TRUE)
            struct ifreq_smc_test* if_req_smc_test = (struct ifreq_smc_test *)ifr;

            SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_ioctl: SIOCDEV_RUN_TEST: test case 0x%02X, test data length = %d, test data 0x%08X",
                    if_req_smc_test->if_test_case ,if_req_smc_test->if_test_data_len, (uint32_t)if_req_smc_test->if_test_data);

            if(if_req_smc_test->if_test_data_len > 0 && if_req_smc_test->if_test_data != NULL )
            {
                SMC_TRACE_PRINTF_DEBUG_DATA( if_req_smc_test->if_test_data_len, if_req_smc_test->if_test_data );
            }
            else
            {
                SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_ioctl: SIOCDEV_RUN_TEST: No test input data found");
            }

            if( smc_test_handler_start(if_req_smc_test->if_test_case, if_req_smc_test->if_test_data_len, if_req_smc_test->if_test_data) == SMC_OK )
            {
                SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_ioctl: SIOCDEV_RUN_TEST: test 0x%02X OK", if_req_smc_test->if_test_case);

                if_req_smc_test->if_test_result = 0xABCD;
                ret_val = SMC_DRIVER_OK;
            }
            else
            {
                SMC_TRACE_PRINTF_ERROR("smc_control_net_device_driver_ioctl: SIOCDEV_RUN_TEST: test 0x%02X FAILED", if_req_smc_test->if_test_case);
                if_req_smc_test->if_test_result = 0xCDEF;
                ret_val = SMC_DRIVER_ERROR;
            }
#else
            SMC_TRACE_PRINTF_ERROR("smc_control_net_device_driver_ioctl: SIOCDEV_RUN_TEST: Test interface not available");
            ret_val = SMC_DRIVER_ERROR;
#endif
            break;
        }
        default:
        {
            SMC_TRACE_PRINTF_WARNING("smc_control_net_device_driver_ioctl: unsupported ioctl command 0x%04X", cmd);
            break;
        }
    }

    SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_ioctl: completed by return value %d", ret_val);

    return ret_val;
}

static uint16_t smc_control_net_device_driver_select_queue( struct net_device *device, struct sk_buff *skb )
{
    uint16_t subqueue = 0;

    SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_select_queue: device 0x%08X, protocol 0x%04X...", (uint32_t)device, skb->protocol);

    /* TODO If more channels return those */


    subqueue = 0x0000;


    SMC_TRACE_PRINTF_DEBUG("smc_control_net_device_driver_select_queue: completed: subqueue 0x%04X", subqueue);

    return subqueue;
}

static int smc_control_modify_send_data( struct sk_buff *skb, smc_user_data_t* smc_user_data )
{
    SMC_TRACE_PRINTF_DEBUG("smc_control_modify_send_data: no mods");

    return SMC_DRIVER_OK;
}

static void smc_control_layer_device_driver_setup(struct net_device* device)
{
    SMC_TRACE_PRINTF_STM("smc_control_layer_device_driver_setup: modify net device for SMC control usage...");

#ifdef SMC_SUPPORT_SKB_FRAGMENT_UL
    device->features        = NETIF_F_SG | NETIF_F_FRAGLIST /* | NETIF_F_HW_CSUM */ ;
#else
    device->features        = NETIF_F_SG;
#endif

    device->type            = 0x00;
    device->flags           = IFF_POINTOPOINT | IFF_NOARP;
    device->mtu             = 2;
    device->hard_header_len = 4;
    device->dev_addr[0]     = 0x70;
    device->addr_len        = 1;
    device->tx_queue_len    = 500;
}

/* EOF */
