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

Version:       17   13-Aug-2012     Heikki Siikaluoma
Status:        draft
Description :  Power manager features merged to one baseline

Version:       15   19-Jun-2012     Heikki Siikaluoma
Status:        draft
Description :  Code cleanup

Version:       14   13-Jun-2012     Heikki Siikaluoma
Status:        draft
Description :  Code cleanup, SMC status interface function, XMIT function queue mapping fix (MHDP channel)

Version:       3    17-Jan-2012     Heikki Siikaluoma
Status:        draft
Description :  Improvements, test features added

Version:       1    23-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif


#include "smc_common_includes.h"

#include "smc.h"
#include "smc_trace.h"

#include <linux/wakelock.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/if_phonet.h>
#include <linux/skbuff.h>
#include <mach/r8a7373.h>
#include <net/sch_generic.h>

#include "smc_linux.h"
#include "smc_linux_ioctl.h"
#include "smc_mdb.h"


#if( SMCTEST == TRUE )
  #include "smc_test.h"
#endif

#if(SMC_CONTROL==TRUE)
  #define SMC_CONF_COUNT_CONTROL  1

  extern smc_device_driver_config dev_config_control;

#if( SMC_LINUX_ENABLE_SELF == TRUE )
  static int smc_initialize_instance_control( void );
#endif

#else
  #define SMC_CONF_COUNT_CONTROL  0
#endif

#if(SMC_L2MUX_IF==TRUE)
  #include "smc_conf_l2mux_linux.h"

  #define SMC_CONF_COUNT_L2MUX  1

    /*
     * The Device Configuration variables for L2MUX usage
     */
  extern smc_device_driver_config dev_config_l2mux;
#else
      /* No L2MUX included */
  #define SMC_CONF_COUNT_L2MUX  0
#endif

MODULE_AUTHOR("Renesas Mobile Europe / Heikki Siikaluoma <heikki.siikaluoma@renesasmobile.com>");
MODULE_DESCRIPTION("SMeCo - SMC Network Device Driver");
MODULE_LICENSE("Dual BSD/GPL");

#if( SMCTEST == TRUE )
#if( SMCTEST_OWN_INIT == TRUE )
        /**
         * For test purposes the smeco.ko can be instantiated in the driver
         * NOTE: In this case there should be no instantiation in the kernel startup
         */
    static int       instantiate  = 0;

    module_param(instantiate, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    MODULE_PARM_DESC(instantiate, "Instantiate SMC Platform Driver");

    /**
     * IRQ Resources from the R-Mobile HW Manual
     * Index of array is the bit in the Modem
     */
    static struct resource smc_resources[] =
    {
        [0] = {
                  .start = gic_spi(193),
                  .end   = gic_spi(193),
                  .flags = IORESOURCE_IRQ, /* | IRQ_TYPE_LEVEL_LOW,*/
              },

        [1] = {
                  .start = gic_spi(194),
                  .end   = gic_spi(194),
                  .flags = IORESOURCE_IRQ,
              },
        [2] = {
                  .start = gic_spi(195),
                  .end   = gic_spi(195),
                  .flags = IORESOURCE_IRQ,
              },
        [3] = {
                  .start = gic_spi(196),
                  .end   = gic_spi(196),
                  .flags = IORESOURCE_IRQ,
              },
    };


    static struct platform_device smc_netdevice0 =
    {
      .name          = "smc_net_device",
      .id            = 0,
      .resource      = smc_resources,
      .num_resources = ARRAY_SIZE(smc_resources),
    };

    static struct platform_device smc_ctrldevice1 =
    {
      .name          = "smc_ctrl_device",
      .id            = 1,
      .resource      = smc_resources,
      .num_resources = ARRAY_SIZE(smc_resources),
    };

    static struct platform_device *devices[] __initdata =
    {
        &smc_netdevice0,
        &smc_ctrldevice1,
    };
#endif
#endif


    /*
     * Net Device configuration array of variables defined above.
     * The first device has id 0, the second has id 1, etc.
     *
     */

#define SMC_DEVICE_DRIVER_CONF_COUNT    (SMC_CONF_COUNT_CONTROL + SMC_CONF_COUNT_L2MUX)

smc_device_driver_config* smc_device_conf[SMC_DEVICE_DRIVER_CONF_COUNT] =
{
#if(SMC_L2MUX_IF==TRUE)
    &dev_config_l2mux,
#endif

#if(SMC_CONTROL==TRUE)
    &dev_config_control,
#endif

};

    /*
     * Device methods implemented by SMC
     */
static int smc_net_device_driver_open(struct net_device* device);
static int smc_net_device_driver_close(struct net_device* device);
static int smc_net_device_driver_xmit(struct sk_buff *skb, struct net_device *device);
uint16_t   smc_net_device_driver_select_queue(struct net_device *device, struct sk_buff *skb);
static int smc_net_device_driver_ioctl(struct net_device *device, struct ifreq *ifr, int cmd);
static int smc_net_device_driver_set_mtu(struct net_device *device, int new_mtu);

    /*
     * Opens channels and start to communicate with remote.
     * NOTE: The network device should be open.
     */
static int smc_net_device_driver_open_channels(struct net_device* device);

    /*
     * Net Device function implementation declarations.
     */
static const struct net_device_ops smc_net_device_ops =
{
    .ndo_open           = smc_net_device_driver_open,
    .ndo_stop           = smc_net_device_driver_close,
    .ndo_select_queue   = smc_net_device_driver_select_queue,
    .ndo_start_xmit     = smc_net_device_driver_xmit,
    .ndo_do_ioctl       = smc_net_device_driver_ioctl,
    .ndo_change_mtu     = smc_net_device_driver_set_mtu,
};


#ifdef SMC_XMIT_BUFFER_FAIL_SEND

/* Buffer skb:s
static sk_buff* skb_wait_buffer[10];
static int      skb_wait_buffer_ind = 0;
*/

#endif

    /*
     * Prints the current status of the SMC.
     * Function is invoked from the IOCTL function.
     */
static int smc_net_device_print_status( struct net_device* device, smc_device_driver_priv_t* smc_net_dev );

/*
 * Network device configuration getter.
 */
static inline smc_device_driver_config* smc_get_device_driver_config(int platform_device_id)
{
    SMC_TRACE_PRINTF_INFO("smc_get_device_driver_config: search configuration for device id %d...", platform_device_id);

    if( platform_device_id < SMC_DEVICE_DRIVER_CONF_COUNT )
    {
        SMC_TRACE_PRINTF_INFO("smc_get_device_driver_config: configuration for device id %d found", platform_device_id);

        return smc_device_conf[platform_device_id];
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_get_device_driver_config: configuration for device id %d not found", platform_device_id);
        return NULL;
    }
}


/*
 * Functions for the SKB fragments
 */
static inline void* smc_kmap_skb_fragment(const skb_frag_t *frag)
{
#ifdef CONFIG_HIGHMEM
    BUG_ON(in_irq());

    local_bh_disable();
#endif

#ifdef SMC_LINUX_USE_KMAP_OLD
    return kmap_atomic(frag->page, KM_SKB_DATA_SOFTIRQ);
#else

    return kmap_atomic(skb_frag_page(frag));
#endif
}

static inline void smc_kunmap_skb_fragment(void *vaddr)
{
#ifdef SMC_LINUX_USE_KMAP_OLD
    kunmap_atomic(vaddr, KM_SKB_DATA_SOFTIRQ);
#else
    kunmap_atomic(vaddr);
#endif

#ifdef CONFIG_HIGHMEM
    local_bh_enable();
#endif
}

#if(SMC_CONTROL==TRUE && SMC_LINUX_ENABLE_SELF == TRUE)
static int smc_initialize_instance_control( void )
{
    int ret_val = SMC_DRIVER_OK;
    smc_conf_t* smc_instance_conf = NULL;
    smc_t*      smc_instance      = NULL;

    SMC_TRACE_PRINTF_STARTUP("smc_initialize_instance_control: retrieving control configuration...");

    smc_instance_conf = dev_config_control.smc_conf(NULL);

    if( smc_instance_conf != NULL )
    {
        SMC_TRACE_PRINTF_STARTUP("smc_initialize_instance_control: creating SMC instance...");
        smc_instance = smc_instance_create_ext(smc_instance_conf, NULL);

        if( smc_instance != NULL )
        {
            SMC_TRACE_PRINTF_STARTUP("smc_initialize_instance_control: SMC instance successfully created");
            ret_val = SMC_DRIVER_OK;
        }
        else
        {
            SMC_TRACE_PRINTF_ERROR("smc_initialize_instance_control: SMC instance creation failed");
            ret_val = SMC_DRIVER_ERROR;
        }
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_initialize_instance_control: SMC control configuration creation failed");
        ret_val = SMC_DRIVER_ERROR;
    }

    return ret_val;
}
#endif

/*
 * Opens channels and start to communicate with remote.
 * NOTE: The network device should be open.
 */
static int smc_net_device_driver_open_channels(struct net_device* device)
{
    int                       ret_val  = SMC_DRIVER_OK;
    smc_device_driver_priv_t* smc_priv = NULL;

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_open_channels: Device '%s' 0x%08X...", device->name, (uint32_t)device);

    smc_priv = netdev_priv(device);
    
    if( smc_priv != NULL && smc_priv->smc_dev_config != NULL )
    {
        smc_conf_t* smc_instance_conf = smc_priv->smc_dev_config->smc_conf( device->name );

        if( smc_instance_conf != NULL )
        {
            SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_open_channels: SMC priv 0x%08X: read SMC configuration 0x%08X and create SMC instance, use parent 0x%08X...",
                    (uint32_t)smc_priv, (uint32_t)smc_instance_conf, (uint32_t)smc_priv->platform_device);

#ifdef SMC_DMA_ENABLED

            SMC_TRACE_PRINTF_DMA("smc_net_device_driver_open_channels: check DMA usage of device '%s'...", device->name);

                /* Check if any channels use DMA */
            if( smc_instance_uses_dma( smc_instance_conf ) )
            {
                SMC_TRACE_PRINTF_DMA("smc_net_device_driver_open_channels: device '%s' (0x%08X) uses DMA (parent device 0x%08X), platform device 0x%08X...", device->name, (uint32_t)device, (uint32_t)device->dev.parent, (uint32_t)smc_priv->platform_device);

                if( smc_dma_init( device->dev.parent ) == SMC_OK )
                {
                    SMC_TRACE_PRINTF_DMA("smc_net_device_driver_open_channels: device '%s': DMA initialization ok", device->name);
                }
                else
                {
                    SMC_TRACE_PRINTF_ERROR("smc_net_device_driver_open_channels: device '%s': DMA initialization failed", device->name);
                }
            }
            else
            {
                SMC_TRACE_PRINTF_DMA("smc_net_device_driver_open_channels: Device '%s' does not use DMA in any channels", device->name);
            }
#endif
            smc_priv->smc_instance = smc_instance_create_ext(smc_instance_conf, smc_priv->platform_device);

            if( smc_priv->smc_instance != NULL )
            {
#ifdef SMC_WAKEUP_USE_EXTERNAL_IRQ_APE
                smc_register_wakeup_irq( smc_priv->smc_instance, SMC_APE_WAKEUP_EXTERNAL_IRQ_ID, SMC_APE_WAKEUP_EXTERNAL_IRQ_TYPE );
#endif
                SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_open_channels: SMC priv 0x%08X: SMC instance 0x%08X created", (uint32_t)smc_priv, (uint32_t)smc_priv->smc_instance);

                SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_open_channels: netif_tx_start_all_queues...");
                netif_tx_start_all_queues(device);

                ret_val = SMC_DRIVER_OK;
            }
            else
            {
                SMC_TRACE_PRINTF_WARNING("smc_net_device_driver_open_channels: SMC instance creation failed");
                ret_val = SMC_DRIVER_ERROR;
            }
         }
         else
         {
             SMC_TRACE_PRINTF_WARNING("smc_net_device_driver_open_channels: SMC configuration not initialized");
             ret_val = SMC_DRIVER_ERROR;
         }
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("smc_net_device_driver_open_channels: invalid SMC device configuration");
        ret_val = SMC_DRIVER_ERROR;
    }

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_open_channels: Completed by return value %d", ret_val);

    return ret_val;
}

static int smc_net_device_driver_open(struct net_device* device)
{
    int                       ret_val  = SMC_DRIVER_OK;
    //smc_device_driver_priv_t* smc_priv = NULL;

    if( device != NULL )
    {
        SMC_TRACE_PRINTF_STARTUP("Device '%s': network device is started", device->name);

        ret_val = SMC_DRIVER_OK;
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_net_device_driver_open: SMC device is NULL");
        ret_val = SMC_DRIVER_ERROR;
    }

    return ret_val;
}

static int smc_net_device_driver_close(struct net_device* device)
{
    int ret_val = SMC_DRIVER_OK;
    smc_device_driver_priv_t* smc_priv = NULL;

    SMC_TRACE_PRINTF_STARTUP("Device '%s': network device closing...", device->name);

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_close: Device 0x%08X...", (uint32_t)device);

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_close: net device 0x%08X: stop all queues...", (uint32_t)device);
    netif_tx_stop_all_queues(device);

    smc_priv = netdev_priv(device);

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_close: SMC priv: 0x%08X...", (uint32_t)smc_priv);

    if( smc_priv->smc_instance != NULL )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_close: SMC priv: 0x%08X: Destroying SMC instance 0x%08X...",
                (uint32_t)smc_priv, (uint32_t)smc_priv->smc_instance);

#ifdef SMC_DMA_ENABLED
        /* Unintialize the DMA */

        SMC_TRACE_PRINTF_DMA("smc_net_device_driver_close: device '%s' (0x%08X) uninitialize DMA (parent device 0x%08X), platform device 0x%08X...", device->name, (uint32_t)device, (uint32_t)device->dev.parent, (uint32_t)smc_priv->platform_device);
        smc_dma_uninit(device->dev.parent);

#endif

        if( smc_priv->smc_dev_config != NULL && smc_priv->smc_dev_config->device_driver_close != NULL )
        {
            smc_priv->smc_dev_config->device_driver_close( device );
        }

        smc_instance_destroy( smc_priv->smc_instance );
        smc_priv->smc_instance = NULL;

        SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_close: SMC priv: 0x%08X: SMC instance destroyed", (uint32_t)smc_priv);
    }
    else
    {
        SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_close: SMC priv: 0x%08X: SMC instance not initialized", (uint32_t)smc_priv);
    }


    SMC_TRACE_PRINTF_STARTUP("Device '%s': network device is closed", device->name);

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_close: Completed by return value %d", ret_val);

    return ret_val;
}

static int smc_net_device_driver_xmit(struct sk_buff* skb, struct net_device* device)
{
    int                       ret_val           = SMC_DRIVER_ERROR;
    smc_device_driver_priv_t* smc_net_dev       = NULL;
    smc_t*                    smc_instance      = NULL;
    uint8_t                   drop_packet       = 0;
    smc_channel_t*            smc_channel       = NULL;
    uint16_t                  skb_queue_mapping = 0xFF;

#ifdef SMC_NETDEV_WAKELOCK_IN_TX
    struct netdev_queue*      tx_queue     = NULL;
    int    tx_queue_len                    = 0;
#endif

    if( skb==NULL)
    {
        SMC_TRACE_PRINTF_TRANSMIT("smc_net_device_driver_xmit: SKB Data NULL");
        return SMC_DRIVER_ERROR;
    }

    SMC_TRACE_PRINTF_INFO("smc_net_device_driver_xmit: device 0x%08X, protocol 0x%04X, queue %d...", (uint32_t)device, skb->protocol, skb->queue_mapping );
    SMC_TRACE_PRINTF_TRANSMIT("smc_net_device_driver_xmit: SKB Data (len %d, queue):", skb->len, skb->queue_mapping);
    SMC_TRACE_PRINTF_TRANSMIT_DATA( skb->len , skb->data );

    if( device == NULL )
    {
        SMC_TRACE_PRINTF_ASSERT("smc_net_device_driver_xmit: net device in parameter is NULL");
        assert(0);
    }

    smc_net_dev  = netdev_priv(device);

    if( smc_net_dev != NULL )
    {
        smc_instance = smc_net_dev->smc_instance;
    }

    if (smc_instance != NULL)
    {
        if (skb->queue_mapping < smc_instance->smc_channel_list_count)
        {
            skb_queue_mapping = skb->queue_mapping;

            smc_channel = SMC_CHANNEL_GET(smc_instance, skb_queue_mapping);

            if( smc_channel == NULL )
            {
                SMC_TRACE_PRINTF_ERROR("smc_net_device_driver_xmit: no SMC channel found for the subqueue %d)", skb_queue_mapping);
                drop_packet = 8;
                goto DROP_PACKET;
            }

#ifdef SMC_NETDEV_WAKELOCK_IN_TX
            if( smc_channel->smc_tx_wakelock != NULL )
            {
                SMC_TRACE_PRINTF_APE_WAKELOCK_TX("smc_net_device_driver_xmit: wake_lock 0x%08X", (uint32_t)smc_channel->smc_tx_wakelock );
                wake_lock( (struct wake_lock*)smc_channel->smc_tx_wakelock );
            }
#endif

            if (skb->len < PHONET_MIN_MTU)
            {
                drop_packet = 6;
                goto DROP_PACKET;
            }

                /* 32-bit alignment check */
            if ((skb->len & 3) && skb_pad(skb, 4 - (skb->len & 3)))
            {
                drop_packet = 5;
                goto DROP_PACKET;
            }

                /* Prevent the remote side to wake up the queue during the send */
            SMC_LOCK_TX_BUFFER( smc_channel->lock_tx_queue );

#ifdef SMC_BUFFER_MESSAGE_OUT_OF_MDB_MEM
            if( TRUE )
#else
            if (!SMC_CHANNEL_STATE_SEND_IS_DISABLED( smc_channel->state ))
#endif
            {
                SMC_CHANNEL_STATE_SET_SEND_DISABLED_XMIT( smc_channel->state );
                SMC_CHANNEL_STATE_SET_SEND_IS_DISABLED( smc_channel->state );

                SMC_TRACE_PRINTF_TRANSMIT("smc_net_device_driver_xmit: stop the subqueue %d to allow queue in upper layer", skb_queue_mapping);

                netif_stop_subqueue(device, skb_queue_mapping);

                SMC_UNLOCK_TX_BUFFER( smc_channel->lock_tx_queue );

                if (smc_net_dev->smc_dev_config != NULL)
                {
                    SMC_TRACE_PRINTF_INFO("smc_net_device_driver_xmit: deliver to upper layer TX function...");
                    ret_val = smc_net_dev->smc_dev_config->skb_tx_function(skb, device);
                }
                else
                {
                    SMC_TRACE_PRINTF_WARNING("smc_net_device_driver_xmit: upper layer TX function not defined");
                    ret_val = SMC_DRIVER_OK;
                }

                if (unlikely(ret_val))
                {
                    SMC_TRACE_PRINTF_ERROR("smc_net_device_driver_xmit: protocol %d, SKB TX failed (wakeup the subqueue %d)", skb->protocol, skb_queue_mapping);
                    drop_packet = 3;
                }
                else
                {
                    smc_user_data_t userdata;
                    void*           data_to_send     = NULL;
                    uint32_t        data_to_send_len = 0;

                    SMC_TRACE_PRINTF_TRANSMIT("smc_net_device_driver_xmit: send data using SMC 0x%08X subqueue %d...", (uint32_t)smc_instance, skb_queue_mapping);

                    userdata.flags     = 0x00000000;
                    userdata.userdata1 = 0x00000000;
                    userdata.userdata2 = 0x00000000;
                    userdata.userdata3 = 0x00000000;
                    userdata.userdata4 = 0x00000000;
                    userdata.userdata5 = 0x00000000;

#ifndef SMC_SUPPORT_SKB_FRAGMENT_UL
                    if(skb_shinfo(skb)->nr_frags != 0)
                    {
                        SMC_TRACE_PRINTF_ASSERT("smc_net_device_driver_xmit: SKB fragments not supported (fragment count %d)", skb_shinfo(skb)->nr_frags);
                        assert(0);
                    }
#endif
                    if (smc_net_dev->smc_dev_config != NULL)
                    {
                        if (smc_net_dev->smc_dev_config->driver_modify_send_data)
                        {
                            SMC_TRACE_PRINTF_INFO("smc_net_device_driver_xmit: upper layer wants to modify send packet");
                            smc_net_dev->smc_dev_config->driver_modify_send_data(skb, &userdata);
                        }
                    }

#ifdef SMC_XMIT_BUFFER_FAIL_SEND
                    // Check if buffered messages

#endif

#if( defined(SMC_APE_LINUX_KERNEL_STM) && defined(SMC_TRACE_TRANSMIT_ENABLED) )
                    {
                        SMC_TRACE_PRINTF_STM("smc_net_device_driver_xmit: channel %d: send %d bytes from 0x%08X:",
                                smc_channel->id,
                                skb->len,
                                (uint32_t)skb->data);

                        SMC_TRACE_PRINTF_DATA_STM("SMC: smc_net_device_driver_xmit:", skb->data, skb->len, 120);
                    }
#endif

#ifdef SMC_SUPPORT_SKB_FRAGMENT_UL

                        /* Try to allocate the memory from the SMC MDB */
                    data_to_send_len = skb->len;
                    SMC_LOCK_IRQ( smc_channel->lock_mdb );
                    data_to_send = smc_mdb_alloc(smc_channel, data_to_send_len);
                    SMC_UNLOCK_IRQ( smc_channel->lock_mdb );

                    if( data_to_send != NULL )
                    {
                        int nr_fragments = skb_shinfo(skb)->nr_frags;

                        if (nr_fragments > 0)
                        {
                            skb_frag_t *frag        = NULL;
                            uint32_t    data_copied = 0;

                            SMC_TRACE_PRINTF_TRANSMIT("smc_net_device_driver_xmit: channel %d: send %d bytes from MDB address 0x%08X (skb=0x%08X) (fragments %d)...",
                                                                                        smc_channel->id,
                                                                                        data_to_send_len,
                                                                                        (uint32_t)data_to_send,
                                                                                        (uint32_t)skb,
                                                                                        nr_fragments);
                                /* Copy the MHDP header */
                            memcpy(data_to_send, skb->data, skb_headlen(skb));
                            data_copied += skb_headlen(skb);

                                /* Copy the data fragments */
                            for (int i = 0; i < nr_fragments; i++)
                            {
                                uint8_t* vaddr = NULL;

                                frag  = &skb_shinfo(skb)->frags[i];
                                vaddr = smc_kmap_skb_fragment(&skb_shinfo(skb)->frags[i]);


                                memcpy((void *)((uint8_t*)data_to_send+data_copied), (void *)(uint8_t*)(vaddr+frag->page_offset), frag->size);

                                smc_kunmap_skb_fragment(vaddr);
                                data_copied += frag->size;
                            }

                            if(data_copied != skb->len)
                            {
                                SMC_TRACE_PRINTF_ASSERT("smc_net_device_driver_xmit: SKB fragment copy failed (copied %d bytes, original length %d bytes, fragment count %d)", data_copied, skb->len, nr_fragments);
                                assert(0);
                            }
                        }
                        else
                        {
                            SMC_TRACE_PRINTF_TRANSMIT("smc_net_device_driver_xmit: channel %d: send %d bytes from MDB 0x%08X (skb=0x%08X) (no fragments)...",
                                                            smc_channel->id,
                                                            data_to_send_len,
                                                            (uint32_t)data_to_send,
                                                            (uint32_t)skb);

                            memcpy( data_to_send, skb->data, skb->len );
                        }

                        if( smc_channel->smc_shm_conf_channel->use_cache_control )
                        {
                            SMC_SHM_CACHE_CLEAN( data_to_send, ((void*)(((uint32_t)data_to_send)+data_to_send_len)) );
                        }
                        else
                        {
                            SMC_HW_ARM_MEMORY_SYNC(NULL);
                        }
                    }
                    else
                    {
                        // TODO Buffer the packet if supported
                        ret_val = SMC_DRIVER_ERROR;
                        drop_packet = 7;
                    }
#else
                    data_to_send     = (void*)skb->data;
                    data_to_send_len = skb->len;
#endif
                    if( data_to_send != NULL )
                    {
                        if ( smc_send_ext(smc_channel, data_to_send, data_to_send_len, &userdata) != SMC_OK )
                        {
                            SMC_TRACE_PRINTF_WARNING("smc_net_device_driver_xmit: channel %d protocol %d, smc_send failed", smc_channel->id, skb->protocol);

#ifdef SMC_SUPPORT_SKB_FRAGMENT_UL
                            /* TODO Free the data ptr if allocated from the MDB */
                            smc_channel_free_ptr_local( smc_channel, data_to_send, &userdata);
                            data_to_send = NULL;
#endif

#ifdef SMC_XMIT_BUFFER_FAIL_SEND
                            if( TRUE )
                            {
                                    /* Try to buffer the message */
                                SMC_TRACE_PRINTF_STM("smc_net_device_driver_xmit: channel %d: try to buffer SKB 0x%08X (%d bytes) --- NOT IMPLEMENTED ---",
                                    smc_channel->id,
                                    (uint32_t)skb->data,
                                    skb->len);

                                    //--- Currently: No buffering -> drop the packet
                                ret_val = SMC_DRIVER_ERROR;
                                drop_packet = 1;
                            }
                            else
#endif
                            {
                                ret_val = SMC_DRIVER_ERROR;
                                drop_packet = 1;
                            }
                        }
                        else
                        {
                                /* Update statistics */
                            device->stats.tx_packets++;
                            device->stats.tx_bytes += data_to_send_len;

                            SMC_TRACE_PRINTF_INFO("smc_net_device_driver_xmit: Free the SKB ANY 0x%08X...", (uint32_t)skb);

                                /* Free the message data */
                            dev_kfree_skb_any(skb);

                            drop_packet = 0;
                            ret_val = SMC_DRIVER_OK;
                            SMC_TRACE_PRINTF_TRANSMIT("smc_net_device_driver_xmit: Completed by return value %d", ret_val);
                        }
                    }
                }

                SMC_LOCK_TX_BUFFER( smc_channel->lock_tx_queue );

                SMC_CHANNEL_STATE_CLEAR_SEND_DISABLED_XMIT( smc_channel->state );

                    /* Wake up the queue only if it is not stopped by remote */
                if( SMC_CHANNEL_STATE_ALLOW_RESUME_SEND( smc_channel->state ) )
                {
                    SMC_TRACE_PRINTF_TRANSMIT("smc_net_device_driver_xmit: wake up the subqueue %d to allow message sending", skb_queue_mapping);
                    netif_wake_subqueue(device, skb_queue_mapping);
                    SMC_CHANNEL_STATE_CLEAR_SEND_IS_DISABLED( smc_channel->state );
                }

                SMC_UNLOCK_TX_BUFFER( smc_channel->lock_tx_queue );

                if( drop_packet > 0 )
                {
                    goto DROP_PACKET;
                }
            }
            else
            {
                SMC_UNLOCK_TX_BUFFER( smc_channel->lock_tx_queue );

                SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_xmit: channel %d: TX queue %d is disabled", smc_channel->id, skb_queue_mapping);
                drop_packet = 2;
                goto DROP_PACKET;
            }
        }
        else
        {
            SMC_TRACE_PRINTF_WARNING("smc_net_device_driver_xmit: SMC instance has no channel for queue %d", skb->queue_mapping);
            drop_packet = 2;
            goto DROP_PACKET;
        }

#ifdef SMC_NETDEV_WAKELOCK_IN_TX

        if( smc_channel != NULL )
        {
                /* Check if the TX queue is empty */
            tx_queue = netdev_get_tx_queue(device, 0);
            tx_queue_len = qdisc_qlen(tx_queue->qdisc);

            if( (tx_queue_len+1) > smc_channel->tx_queue_peak )
            {
                smc_channel->tx_queue_peak = tx_queue_len+1;
            }

            SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_xmit: channel %d: wake unlock device TX queue len %d", skb_queue_mapping, tx_queue_len);

            if( tx_queue_len == 0 )
            {
                if( smc_channel != NULL && smc_channel->smc_tx_wakelock != NULL )
                {
                    SMC_TRACE_PRINTF_APE_WAKELOCK_TX("smc_net_device_driver_xmit: channel %d: wake_unlock 0x%08X", skb_queue_mapping, (uint32_t)smc_channel->smc_tx_wakelock );
                    wake_unlock( (struct wake_lock*)smc_channel->smc_tx_wakelock );
                }
            }
            else
            {
                SMC_TRACE_PRINTF_APE_WAKELOCK_TX("smc_net_device_driver_xmit: channel %d: wake_lock not unlocked: device TX queue len %d", skb_queue_mapping, tx_queue_len);
            }
        }
#endif
        return ret_val;
     }
     else
     {
         SMC_TRACE_PRINTF_ERROR("smc_net_device_driver_xmit: SMC instance not initialized for the device");
         drop_packet = 9;
         goto DROP_PACKET;
     }

    /* --- DROP the PACKET ---- */
DROP_PACKET:

    if( device != NULL )
    {
        device->stats.tx_dropped++;

        if( drop_packet == 1 )
        {
            SMC_TRACE_PRINTF_WARNING("SMC TX Packet 0x%08X, len %d dropped (total %ld): unable to send to remote", (uint32_t)skb->data, skb->len, device->stats.tx_dropped);
        }
        else if( drop_packet == 2 )
        {
            SMC_TRACE_PRINTF_WARNING("SMC TX Packet 0x%08X, len %d dropped (total %ld): TX Queue locked", (uint32_t)skb->data, skb->len, device->stats.tx_dropped);
        }
        else if( drop_packet == 3 )
        {
            SMC_TRACE_PRINTF_WARNING("SMC TX Packet 0x%08X, len %d dropped (total %ld): SKB TX failed", (uint32_t)skb->data, skb->len, device->stats.tx_dropped);
        }
        else if( drop_packet == 4 )
        {
            SMC_TRACE_PRINTF_WARNING("SMC TX Packet 0x%08X, len %d dropped (total %ld): No channel for queue", (uint32_t)skb->data, skb->len, device->stats.tx_dropped);
        }
        else if( drop_packet == 5 )
        {
            SMC_TRACE_PRINTF_WARNING("SMC TX Packet 0x%08X, len %d dropped (total %ld): data not 32-bit aligned", (uint32_t)skb->data, skb->len, device->stats.tx_dropped);
        }
        else if( drop_packet == 6 )
        {
            SMC_TRACE_PRINTF_WARNING("SMC TX Packet 0x%08X, len %d dropped (total %ld): packet too short, required %d", (uint32_t)skb->data, skb->len, device->stats.tx_dropped, PHONET_MIN_MTU);
        }
        else if( drop_packet == 7 )
        {
#if 1
            /* Take out the warning trace because that might cause a rcu_preepmt stall when printing all the time... Testing this. */
            SMC_TRACE_PRINTF_DEBUG("SMC TX Packet 0x%08X, len %d dropped (total %ld): no shared memory available", (uint32_t)skb->data, skb->len, device->stats.tx_dropped);
#else
            SMC_TRACE_PRINTF_WARNING("SMC TX Packet 0x%08X, len %d dropped (total %ld): no shared memory available", (uint32_t)skb->data, skb->len, device->stats.tx_dropped);
#endif
        }
        else if( drop_packet == 8 )
        {
            SMC_TRACE_PRINTF_WARNING("SMC TX Packet 0x%08X, len %d dropped (total %ld): no SMC channel found", (uint32_t)skb->data, skb->len, device->stats.tx_dropped);
        }
        else if( drop_packet == 8 /* Should this be 9 ??? */ )
        {
            SMC_TRACE_PRINTF_WARNING("SMC TX Packet 0x%08X, len %d dropped (total %ld): no SMC instance found", (uint32_t)skb->data, skb->len, device->stats.tx_dropped);
        }
        else
        {
            SMC_TRACE_PRINTF_WARNING("SMC: TX Packet 0x%08X, len %d dropped (total %ld)", (uint32_t)skb->data, skb->len, device->stats.tx_dropped);
        }
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("smc_net_device_driver_xmit: packet 0x%08X, len %d (reason %d)", (uint32_t)skb->data, skb->len, drop_packet);
    }

#ifdef SMC_NETDEV_WAKELOCK_IN_TX

    if( smc_channel != NULL )
    {
            /* Check if the TX queue is empty */
        tx_queue = netdev_get_tx_queue(device, 0);
        tx_queue_len = qdisc_qlen(tx_queue->qdisc);

        if( (tx_queue_len+1) > smc_channel->tx_queue_peak )
        {
            smc_channel->tx_queue_peak = tx_queue_len+1;
        }

        SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_xmit: channel %d: wake unlock device TX queue len %d", skb_queue_mapping, tx_queue_len);

        if( tx_queue_len == 0 )
        {
            if( smc_channel->smc_tx_wakelock != NULL )
            {
                SMC_TRACE_PRINTF_APE_WAKELOCK_TX("smc_net_device_driver_xmit: channel %d: wake_unlock 0x%08X", skb_queue_mapping, (uint32_t)smc_channel->smc_tx_wakelock );
                wake_unlock( (struct wake_lock*)smc_channel->smc_tx_wakelock );
            }
        }
    }

#endif

    ret_val = SMC_DRIVER_ERROR;

        /* If skb_pad fails, it frees the packet, so not freeing it here */
    if( drop_packet != 5 )
    {
        dev_kfree_skb_any(skb);
    }

    return ret_val;
}

uint16_t smc_net_device_driver_select_queue(struct net_device *device, struct sk_buff *skb)
{
    smc_device_driver_priv_t* smc_net_dev  = NULL;

    SMC_TRACE_PRINTF_TRANSMIT("smc_net_device_driver_select_queue: device 0x%08X, protocol 0x%04X...", (uint32_t)device, skb->protocol);

    smc_net_dev = netdev_priv(device);

    if( smc_net_dev && smc_net_dev->smc_dev_config && smc_net_dev->smc_dev_config->driver_select_queue)
    {
        SMC_TRACE_PRINTF_TRANSMIT("smc_net_device_driver_select_queue: delivering to upper layer queue selection handler...");
        return smc_net_dev->smc_dev_config->driver_select_queue( device, skb );
    }
    else
    {
        SMC_TRACE_PRINTF_ASSERT("smc_net_device_driver_select_queue: no queue selection handler in upper layer");
        assert(0);
    }

    return 0;
}

static int smc_net_device_driver_ioctl(struct net_device* device, struct ifreq* ifr, int cmd)
{
    int                       ret_val      = SMC_DRIVER_ERROR;
    smc_device_driver_priv_t* smc_net_dev  = NULL;

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: starts device 0x%08X, command %d (0x%08X)...", (uint32_t)device, cmd, cmd);

    smc_net_dev = netdev_priv(device);

    /* First handle the common commands */

    if( cmd == SIOCDEV_STATUS )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: SIOCDEV_STATUS");
        ret_val = smc_net_device_print_status(device, smc_net_dev);
    }
    else if( cmd == SIOCDEV_MSG_LOOPBACK )
    {
        /* Valid only if test mode enabled */
#if(SMCTEST==TRUE)
        struct ifreq_smc_loopback* if_req_smc = (struct ifreq_smc_loopback *)ifr;

        smc_t*          smc_instance = NULL;
        smc_channel_t*  smc_channel  = NULL;
        uint32_t        lb_data_len  = 0;
        uint32_t        lb_rounds    = 0;

        smc_instance = smc_net_dev->smc_instance;
        smc_channel  = SMC_CHANNEL_GET(smc_instance, if_req_smc->if_channel_id);

        lb_data_len = if_req_smc->if_loopback_payload_length;
        lb_rounds   = if_req_smc->if_loopback_rounds;

        SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: SIOCDEV_MSG_LOOPBACK for channel %d, data len %d, %d loopback roundtrips",
        if_req_smc->if_channel_id, lb_data_len, lb_rounds);

        if( smc_channel != NULL )
        {
            if( smc_send_loopback_data_message( smc_channel, lb_data_len, lb_rounds, FALSE ) )
            {
                ret_val = SMC_DRIVER_OK;
            }
            else
            {
                ret_val = SMC_DRIVER_ERROR;
            }
        }
        else
        {
            ret_val = SMC_DRIVER_ERROR;
        }
#else
        SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: SIOCDEV_MSG_LOOPBACK: not permitted");
        ret_val = SMC_DRIVER_ERROR;
#endif
    }
    else if( cmd == SIOCDEV_MSG_INTERNAL )
    {
        struct ifreq_smc_msg_internal* if_req_smc_msg = (struct ifreq_smc_msg_internal *)ifr;

        smc_t*          smc_instance = NULL;
        smc_channel_t*  smc_channel  = NULL;

        SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: SIOCDEV_MSG_INTERNAL, message 0x%08X, param 0x%08X", if_req_smc_msg->if_msg_id, if_req_smc_msg->if_msg_parameter);

        smc_instance = smc_net_dev->smc_instance;

        if( smc_instance != NULL )
        {
            smc_channel  = SMC_CHANNEL_GET(smc_instance, if_req_smc_msg->if_channel_id);

            if( if_req_smc_msg->if_msg_id == SMC_MSG_FLAG_PING_REQ )
            {
                if( smc_channel != NULL )
                {
                    uint8_t ping_reply = smc_channel_send_ping( smc_channel, TRUE );

                    if(ping_reply == SMC_OK)
                    {
                        SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: ping ok, reply value = 0x%02X", ping_reply);
                        if_req_smc_msg->if_msg_response = MSG_RESP_OK;
                    }
                    else
                    {
                        SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: ping failed, reply value = 0x%02X", ping_reply);
                        if_req_smc_msg->if_msg_response = MSG_RESP_FAIL;
                    }

                    ret_val = SMC_DRIVER_OK;
                }
                else
                {
                    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: SMC channel %d not found", if_req_smc_msg->if_channel_id);
                }
            }
            else if( if_req_smc_msg->if_msg_id == SMC_MSG_FLAG_CONFIG_REQ )
            {
                if( smc_channel != NULL )
                {
                    if( if_req_smc_msg->if_msg_data_len > 7 && if_req_smc_msg->if_msg_data != 0x00000000 )
                    {
                        uint32_t configuration_id    = *(uint32_t*)if_req_smc_msg->if_msg_data;
                        uint32_t configuration_value = *(uint32_t*)(if_req_smc_msg->if_msg_data+4);

                        uint8_t  reply               = smc_channel_send_config( smc_channel, configuration_id, configuration_value, TRUE );

                        if(reply == SMC_OK)
                        {
                            SMC_TRACE_PRINTF_ALWAYS("smc_net_device_driver_ioctl: config 0x%08X set ok to 0x%08X, reply value = 0x%02X", configuration_id, configuration_value, reply);
                            if_req_smc_msg->if_msg_response = MSG_RESP_OK;
                        }
                        else
                        {
                            SMC_TRACE_PRINTF_ALWAYS("smc_net_device_driver_ioctl: config 0x%08X set to value 0x%08X failed, reply value = 0x%02X", configuration_id, configuration_value, reply);
                            if_req_smc_msg->if_msg_response = MSG_RESP_FAIL;
                        }

                        ret_val = SMC_DRIVER_OK;
                    }
                    else
                    {
                        SMC_TRACE_PRINTF_ERROR("smc_net_device_driver_ioctl: not enough parameters for config: Data length %d, data ptr 0x%08X", if_req_smc_msg->if_msg_data_len, (uint32_t)if_req_smc_msg->if_msg_data);
                        ret_val = SMC_DRIVER_ERROR;
                    }
                }
                else
                {
                    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: SMC channel %d not found", if_req_smc_msg->if_channel_id);
                }
            }
            else
            {
                SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: unsupported message 0x%08X", if_req_smc_msg->if_msg_id );
            }
        }
        else
        {
            SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: SMC instance not found");
        }
    }
    else if( cmd == SIOCDEV_HISTORY )
    {
        struct ifreq_smc_history* if_req_smc_history = (struct ifreq_smc_history *)ifr;
        uint32_t history_count_available = 3;

        SMC_TRACE_PRINTF_ALWAYS("smc_net_device_driver_ioctl: SIOCDEV_HISTORY, max returned %d", if_req_smc_history->requested_history_item_count);

        // Just dummy history for testing IOCTL

        if( history_count_available > if_req_smc_history->requested_history_item_count )
        {
            history_count_available = if_req_smc_history->requested_history_item_count;
        }

        if_req_smc_history->history_item_count = history_count_available;

        for(int i = 0; i < if_req_smc_history->history_item_count; i++ )
        {
            if_req_smc_history->history_item_array[i].channel_id        = (i+1);
            if_req_smc_history->history_item_array[i].history_item_type = 0xAB;
            if_req_smc_history->history_item_array[i].length            = (i+1)*10;
        }

        ret_val = SMC_DRIVER_OK;
    }
    else if( cmd==SIOCDEV_INFO )
    {
        struct ifreq_smc_info* if_req_smc_info = (struct ifreq_smc_info *)ifr;


        SMC_TRACE_PRINTF_STM("smc_net_device_driver_ioctl: device '%s': SIOCDEV_INFO: select version type 0x%02X", device->name, if_req_smc_info->smc_version_selection);

        if( if_req_smc_info->smc_version_selection == SMC_VERSION_SELECTION_REMOTE )
        {
            smc_t*         smc_instance   = NULL;
            smc_channel_t* smc_channel    = NULL;
            uint32_t       version_remote = 0x00000000;
            char*          temp_str       = NULL;

                /* Get the remote version from 0 channel */
            smc_instance = smc_net_dev->smc_instance;

            if( smc_instance != NULL )
            {
                smc_channel = SMC_CHANNEL_GET(smc_instance, 0);

                if( smc_channel != NULL )
                {
                    version_remote = smc_channel->version_remote;
                }
            }

            temp_str = smc_version_to_str(version_remote);

            strcpy( if_req_smc_info->smc_version, temp_str );

            SMC_FREE( temp_str );
        }
        else
        {
                /* Set the local version info */
            strcpy( if_req_smc_info->smc_version, SMC_SW_VERSION );
        }

        SMC_TRACE_PRINTF_STM("smc_net_device_driver_ioctl: device '%s': SIOCDEV_INFO: return version '%s' for version type %d", device->name, if_req_smc_info->smc_version, if_req_smc_info->smc_version_selection );

        ret_val = SMC_DRIVER_OK;
    }
    else if( cmd==SIOCDEV_TRACE )
    {
#ifdef SMC_APE_RDTRACE_ENABLED
        struct ifreq_smc_trace* if_req_smc_trace = (struct ifreq_smc_trace *)ifr;

        SMC_TRACE_PRINTF_ALWAYS("smc_net_device_driver_ioctl: Trace command invoked: Trace group id 0x%08X, action %d",
                if_req_smc_trace->if_trace_group_id, if_req_smc_trace->if_trace_group_activate);

        if_req_smc_trace->if_traces_activated = smc_rd_trace_group_activate(if_req_smc_trace->if_trace_group_id, if_req_smc_trace->if_trace_group_activate);

        ret_val = SMC_DRIVER_OK;
#else
        SMC_TRACE_PRINTF_ERROR("smc_net_device_driver_ioctl: Runtime trace activation not supported (cmd: SIOCDEV_TRACE 0x%04X)", cmd);
        ret_val = SMC_DRIVER_ERROR;
#endif
    }
    else
    {
        if( smc_net_dev && smc_net_dev->smc_dev_config && smc_net_dev->smc_dev_config->driver_ioctl)
        {
            SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: delivering to upper layer IOCTL handler...");
            ret_val = smc_net_dev->smc_dev_config->driver_ioctl(device, ifr, cmd );
        }
        else
        {
            SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: upper layer does not have IOCTL handler");

            switch(cmd)
            {
                default:
                {
                    SMC_TRACE_PRINTF_WARNING("smc_net_device_driver_ioctl: Unsupported command %d", cmd);
                    break;
                }
            }
        }
    }

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_ioctl: Completed by return value %d", ret_val);

    return ret_val;
}

static int smc_net_device_driver_set_mtu(struct net_device *device, int new_mtu)
{
    int ret_val = SMC_DRIVER_OK;

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_set_mtu: device 0x%08X, old MTU %d new MTU %d...", (uint32_t)device, device->mtu, new_mtu);

    /*if ((new_mtu < MHI_MIN_MTU) || (new_mtu > MHI_MAX_MTU))*/
    if( FALSE )
    {
        SMC_TRACE_PRINTF_WARNING("smc_net_device_driver_set_mtu: device 0x%08X, invalid MTU %d", (uint32_t)device, new_mtu);

        ret_val =  SMC_DRIVER_ERROR;
    }
    else
    {
        device->mtu = new_mtu;
        ret_val = SMC_DRIVER_OK;
    }

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_set_mtu: Completed by return value %d", ret_val);

    return ret_val;
}

static void smc_net_device_driver_setup(struct net_device* device)
{
    smc_device_driver_priv_t* smc_net_dev  = NULL;

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_setup: device 0x%08X starts...", (uint32_t)device);

    smc_net_dev = netdev_priv(device);

    if( smc_net_dev && smc_net_dev->smc_dev_config && smc_net_dev->smc_dev_config->driver_setup)
    {
        SMC_TRACE_PRINTF_STM("smc_net_device_driver_setup: device '%s'  (0x%08X) invoke device specific setup...", device->name, (uint32_t)device);
        smc_net_dev->smc_dev_config->driver_setup( device );
    }
    else
    {
        SMC_TRACE_PRINTF_STM("smc_net_device_driver_setup: device '%s' (0x%08X) creating default configuration...", device->name, (uint32_t)device);

        device->features        = NETIF_F_SG | NETIF_F_FRAGLIST /* Frags to be tested by MHDP team  | NETIF_F_HW_CSUM | NETIF_F_FRAGLIST*/;
        device->type            = 0;
        device->flags           = IFF_POINTOPOINT | IFF_NOARP;
        device->mtu             = 1024;
        device->hard_header_len = 4;
        device->dev_addr[0]     = 0x00;
        device->addr_len        = 1;
        device->tx_queue_len    = 500;
    }

        /* These are the mandatory values */
    device->netdev_ops      = &smc_net_device_ops;
    device->destructor      = free_netdev;

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_driver_setup: completed");
}

static int smc_device_notify(struct notifier_block *me, unsigned long event, void *arg)
{
	struct net_device *dev = arg;
	struct wake_lock   smc_wakelock_conf;

	SMC_TRACE_PRINTF_INFO("smc_device_notify: device '%s' notifies 0x%08X", dev!=NULL?dev->name:"<NO NAME>", (uint32_t)event);

	wake_lock_init(&smc_wakelock_conf, WAKE_LOCK_SUSPEND, "smc_wakelock_conf");

	switch(event) 
	{
		case NETDEV_REGISTER:	/* 0x05 */
		{
			SMC_TRACE_PRINTF_INFO("smc_device_notify: device '%s' NETDEV_REGISTER", dev!=NULL?dev->name:"<NO NAME>");
			break;
		}
		case NETDEV_UP:         /* 0x01 */
		{
			SMC_TRACE_PRINTF_DEBUG("smc_device_notify: device '%s' NETDEV_UP", dev!=NULL?dev->name:"<NO NAME>");

			if( TRUE )
			{
				SMC_TRACE_PRINTF_DEBUG("smc_device_notify: device '%s' NETDEV_UP, check name...",
					dev!=NULL?dev->name:"<NO NAME>");

				if( strncmp( dev->name, "smc", 3 ) == 0 )
				{
				    wake_lock( &smc_wakelock_conf );

				    SMC_TRACE_PRINTF_STARTUP("Device '%s': v.%s is starting up, preparing channels", dev->name, SMC_SW_VERSION);

					if( smc_net_device_driver_open_channels( dev ) == SMC_DRIVER_OK )
					{
					    SMC_TRACE_PRINTF_STARTUP("Device '%s': device is up and channels are started", dev->name);
					}
					else
					{
					    SMC_TRACE_PRINTF_ERROR(" ** ERROR: Network device '%s' is up but startup of channels failed", dev->name);
					}

					wake_unlock( &smc_wakelock_conf );
				}
				else
				{
					SMC_TRACE_PRINTF_DEBUG("smc_device_notify: device '%s' NETDEV_UP, not smc", 
						dev!=NULL?dev->name:"<NO NAME>");
				}
			}
			else
			{
			    SMC_TRACE_PRINTF_DEBUG("smc_device_notify: device '%s' NETDEV_UP, unknown device, no action",
			                        dev!=NULL?dev->name:"<NO NAME>");
			}

			break;
		}
		case NETDEV_DOWN:           /* 0x02 */
		{
			SMC_TRACE_PRINTF_INFO("smc_device_notify: device '%s' NETDEV_DOWN", dev!=NULL?dev->name:"<NO NAME>");

			if( dev != NULL )
			{
			    if( strncmp( dev->name, "smc", 3 ) == 0 )
			    {
			        wake_lock( &smc_wakelock_conf );

			        SMC_TRACE_PRINTF_STARTUP("Device '%s': v.%s is closing...", dev->name, SMC_SW_VERSION);
			        smc_net_device_driver_close( dev );

			        wake_unlock( &smc_wakelock_conf );
			    }
                else
                {
                    SMC_TRACE_PRINTF_DEBUG("smc_device_notify: device '%s' NETDEV_DOWN, unknown device, no action",
                                        dev!=NULL?dev->name:"<NO NAME>");
                }
			}

			break;
		}
		case NETDEV_UNREGISTER:
		{
			SMC_TRACE_PRINTF_INFO("smc_device_notify: device '%s' NETDEV_UNREGISTER", dev!=NULL?dev->name:"<NO NAME>");
			break;
		}
	}

	wake_lock_destroy(&smc_wakelock_conf);

	return 0;
}

static struct notifier_block smc_device_notifier = {
	.notifier_call = smc_device_notify,
	.priority = 0,
};


/*
 * SMC Platform driver implementation.
 *
 */

static int __devinit smc_net_platform_device_probe(struct platform_device* platform_device)
{
    int                       ret_val  = SMC_DRIVER_OK;
    struct net_device*        ndevice  = NULL;
    smc_device_driver_config* dev_conf = NULL;

    SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_probe: SMC v.%s: platform device 0x%08X: id %d, name %s num resources %d...",
            SMC_SW_VERSION, (uint32_t)platform_device, platform_device->id, platform_device->name, platform_device->num_resources);

    dev_conf = smc_get_device_driver_config(platform_device->id);

    if( !dev_conf )
    {
        ret_val = SMC_DRIVER_ERROR;
    }
    else
    {
        SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_probe: platform device 0x%08X: allocate net device using '%s' configuration...",
                (uint32_t)platform_device, dev_conf->name);

        ndevice = alloc_netdev_mq( sizeof(smc_device_driver_priv_t), SMC_NET_DEVICE_NAME_FORMAT, smc_net_device_driver_setup, dev_conf->queue_count );

        SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_probe: net device is 0x%08X, queue count %d", (uint32_t)ndevice, dev_conf->queue_count);

        if( ndevice )
        {
            smc_device_driver_priv_t* smc_net_dev = NULL;

            SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_probe: net device 0x%08X: get SMC priv...", (uint32_t)ndevice);

            smc_net_dev = netdev_priv( ndevice );

            SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_probe: net device 0x%08X: SMC priv 0x%08X", (uint32_t)ndevice, (uint32_t)smc_net_dev);

                /* Initialize SMC Net Device */
            smc_net_dev->net_dev         = ndevice;
            smc_net_dev->platform_device = platform_device;
                /* SMC instance is created while the device is opened */
            smc_net_dev->smc_instance    = NULL;
            smc_net_dev->smc_dev_config  = dev_conf;

                /* Link the config to priv */
            dev_conf->device_driver_priv = smc_net_dev;

            SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_probe: net device 0x%08X: stop all queues...", (uint32_t)ndevice);
            netif_tx_stop_all_queues(ndevice);

            SET_NETDEV_DEV(smc_net_dev->net_dev, &platform_device->dev);

            if( dev_conf->driver_setup )
            {
                SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_probe: initialize net device '%s' (0x%08X) specific features...", ndevice->name, (uint32_t)ndevice);
                dev_conf->driver_setup( ndevice );
            }
            else
            {
                SMC_TRACE_PRINTF_WARNING("smc_net_platform_device_probe: net device '%s' (0x%08X) specific features not defined, using default settings", ndevice->name, (uint32_t)ndevice);
            }

            SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_probe: register net device 0x%08X (0x%08X)...", (uint32_t)ndevice, (uint32_t)smc_net_dev->net_dev);
            ret_val = register_netdev(smc_net_dev->net_dev);

            if( ret_val >= 0 )
            {
                ret_val = SMC_DRIVER_OK;

                SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_probe: net device 0x%08X OK", (uint32_t)ndevice);
            }
            else
            {
                dev_err(&platform_device->dev, "netdev register failed (%d)\n", ret_val);
                free_netdev(smc_net_dev->net_dev);
                SMC_TRACE_PRINTF_ERROR("smc_net_platform_device_probe: net device register failed (%d)", ret_val);
                ret_val = SMC_DRIVER_ERROR;
            }
        }
        else
        {
            dev_err(&platform_device->dev, "No memory for netdev\n");
            SMC_TRACE_PRINTF_ERROR("smc_net_platform_device_probe: no memory for net device");
            ret_val = SMC_DRIVER_ERROR;
        }
    }

    SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_probe: completed by return value 0x%02X", ret_val);

    return ret_val;
}

static int smc_net_device_remove( smc_device_driver_priv_t* smc_device_priv )
{
    int                ret_val = SMC_DRIVER_OK;
    struct net_device* device  = smc_device_priv->net_dev;

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_remove: unregister net device 0x%08X...", (uint32_t)device);
    unregister_netdev( device );

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_remove: Destroy SMC instance 0x%08X in priv 0x%08X...", (uint32_t)smc_device_priv->smc_instance, (uint32_t)smc_device_priv);

    smc_instance_destroy( smc_device_priv->smc_instance );
    smc_device_priv->smc_instance = NULL;

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_remove: free net device 0x%08X...", (uint32_t)device);
    free_netdev(device);

    SMC_TRACE_PRINTF_DEBUG("smc_net_device_remove: completed by return value 0x%02X", ret_val);

    return ret_val;
}

static int smc_net_platform_device_remove(struct platform_device* platform_device)
{
    int ret_val = SMC_DRIVER_OK;
    int i       = 0;

    SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_remove: platform device: 0x%08X id %d...", (uint32_t)platform_device, platform_device->id);

    SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_remove: unregister notifier...");
    unregister_netdevice_notifier(&smc_device_notifier);

    for( i = 0; i < SMC_DEVICE_DRIVER_CONF_COUNT; i++ )
    {
        smc_device_driver_config* dev_config = smc_get_device_driver_config( platform_device->id );

        if( dev_config != NULL )
        {
            SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_remove: platform device 0x%08X unregister net device in config 0x%08X...", (uint32_t)platform_device, (uint32_t)dev_config);

            smc_net_device_remove( dev_config->device_driver_priv );

                /* Unlink the priv from config */
            dev_config->device_driver_priv = NULL;

            /* Do no destroy the device driver config since it is from the stack */
        }
    }


    SMC_TRACE_PRINTF_DEBUG("smc_net_platform_device_remove: completed by return value 0x%02X", ret_val);

    return ret_val;
}

#if( defined( SMC_WAKEUP_USE_EXTERNAL_IRQ_APE ) || defined( SMC_WAKEUP_USE_EXTERNAL_IRQ_MODEM ) )

static uint8_t g_smc_ape_wakeup_irq_sense = SMC_APE_WAKEUP_EXTERNAL_IRQ_SENSE_DEFAULT;

void smc_set_ape_wakeup_irq_sense( uint8_t irq_sense )
{
    g_smc_ape_wakeup_irq_sense = irq_sense;
}

#endif

#ifdef CONFIG_PM

static int smc_platform_device_driver_suspend(struct device *dev)
{
        /* This macro is defined in the target specific configuration header file (smc instance config) */
    SMC_PLATFORM_DEVICE_DRIVER_SUSPEND( dev, g_smc_ape_wakeup_irq_sense );

    /* TODO Cleanup
    uint32_t signal_event_sense = 0x00000002;

#if( defined( SMC_WAKEUP_USE_EXTERNAL_IRQ_APE ) || defined( SMC_WAKEUP_USE_EXTERNAL_IRQ_MODEM ) )
    if( g_smc_ape_wakeup_irq_sense == SMC_SIGNAL_SENSE_RISING_EDGE )
    {
        signal_event_sense = 0x00000002;
    }
    else if( g_smc_ape_wakeup_irq_sense == SMC_SIGNAL_SENSE_FALLING_EDGE )
    {
        signal_event_sense = 0x00000001;
    }
    else
    {
        SMC_TRACE_PRINTF_ASSERT("smc_platform_device_driver_suspend: invalid wakeup IRQ sense value");
        assert(0);
    }
#else
    #error "Invalid PM configuration"
#endif

    __raw_writel(0x00000001, 0xe61c2414);           // PORT_SET
    __raw_writel(signal_event_sense, 0xe61c1980);   // CONFIG_0 - 1 = low level detect, 2 = high level detect
    __raw_writel(0x00000001, 0xe61c1888);           // WAKEN_SET0
    */


    return 0;
}

static int smc_platform_device_driver_resume(struct device *dev)
{
        /* This macro is defined in the target specific configuration header file (smc instance config) */
    SMC_PLATFORM_DEVICE_DRIVER_RESUME( dev );

    /* TODO Cleanup
    __raw_writel(0x00000000, 0xe61c1980); // CONFIG_02 - Disable Interrupt
    __raw_writel(0x00000001, 0xe61c1884); // WAKEN_STS0 - Disable WakeUp Request Enable
    */

    return 0;
}

static const struct dev_pm_ops smc_platform_device_driver_pm_ops = {
    .suspend    = smc_platform_device_driver_suspend,
    .resume     = smc_platform_device_driver_resume,
};
#endif


static struct platform_driver smc_platform_device_driver = {
    .probe    = smc_net_platform_device_probe,
    .remove   = smc_net_platform_device_remove,
    .driver   = {
        .name = SMC_PLATFORM_DRIVER_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_PM
        .pm = &smc_platform_device_driver_pm_ops,
#endif
    },

};

static int __init smc_platform_device_driver_init(void)
{
    int ret_val = SMC_DRIVER_OK;

    SMC_TRACE_PRINTF_STARTUP("Platform device driver version %s", SMC_SW_VERSION);
    SMC_TRACE_PRINTF_DEBUG("smc_platform_device_driver_init: starts...");

    smc_initialize(NULL);

    SMC_TRACE_PRINTF_DEBUG("smc_platform_device_driver_init: register driver...");
    ret_val = platform_driver_register(&smc_platform_device_driver);

    if( ret_val != SMC_DRIVER_OK )
    {
        SMC_TRACE_PRINTF_STARTUP("Registration of the platform device driver version %s failed by return value %d", SMC_SW_VERSION, ret_val);
    }

    SMC_TRACE_PRINTF_DEBUG("smc_platform_device_driver_init: register device notifier...");
    register_netdevice_notifier(&smc_device_notifier);

#if( SMCTEST == TRUE )
#if( SMCTEST_OWN_INIT == TRUE && SMC_LINUX_INSTANTIATE_SELF == TRUE )

/*
    if( instantiate == 1 )
*/
    {
        if( ret_val == SMC_DRIVER_OK )
        {
            SMC_TRACE_PRINTF_STARTUP("Instantiate platform driver version %s...", SMC_SW_VERSION);
            platform_add_devices(devices, ARRAY_SIZE(devices));

            SMC_TRACE_PRINTF_STARTUP("Platform driver version %s instantiated", SMC_SW_VERSION);
        }
        else
        {
            SMC_TRACE_PRINTF_DEBUG("smc_platform_device_driver_init: SMC platform driver register failed, unable to instantiate");
        }
    }
#endif

#if( SMC_LINUX_ENABLE_SELF == TRUE )
            SMC_TRACE_PRINTF_STARTUP("Enabling SMC platform driver version %s...", SMC_SW_VERSION);

#if(SMC_CONTROL==TRUE)
            SMC_TRACE_PRINTF_STARTUP("Enabling SMC platform driver version %s, creating control instance...", SMC_SW_VERSION);

            ret_val = smc_initialize_instance_control();

            SMC_TRACE_PRINTF_STARTUP("SMC platform driver version %s control instance %s (%d)", SMC_SW_VERSION,(ret_val==SMC_DRIVER_OK)?"created successfully":"creation failed", ret_val);
#else
            SMC_TRACE_PRINTF_STARTUP("unable to enable SMC platform driver version %s: no proper SMC instance available", SMC_SW_VERSION);
            ret_val==SMC_DRIVER_ERROR;

#endif  /* #if(SMC_CONTROL==TRUE) */
#endif  /*#if( SMC_LINUX_ENABLE_SELF == TRUE )*/

#endif

    SMC_TRACE_PRINTF_STARTUP("Platform device driver version %s %s (%d)", SMC_SW_VERSION, (ret_val==SMC_DRIVER_OK)?"initialized successfully":"initialization failed", ret_val);

    return ret_val;
}

static void __exit smc_platform_device_driver_exit(void)
{
    SMC_TRACE_PRINTF_DEBUG("smc_platform_device_driver_exit: starts...");

    platform_driver_unregister(&smc_platform_device_driver);

    SMC_TRACE_PRINTF_DEBUG("smc_platform_device_driver_exit: completed");
}

/*
 * Prints the current status of the SMC.
 * Function is invoked from the IOCTL function.
 */
static int smc_net_device_print_status( struct net_device* device, smc_device_driver_priv_t* smc_net_dev )
{
    int ret_val = SMC_DRIVER_OK;

    SMC_TRACE_PRINTF_ALWAYS("SMC: v.%s: Net Device '%s':", SMC_SW_VERSION, device->name);

    SMC_TRACE_PRINTF_ALWAYS("SMC:   - TX packets delivered: %8d", (int)device->stats.tx_packets);
    SMC_TRACE_PRINTF_ALWAYS("SMC:   - TX packets dropped:   %8d", (int)device->stats.tx_dropped);
    SMC_TRACE_PRINTF_ALWAYS("SMC:   - RX packets received:  %8d", (int)device->stats.rx_packets);
    SMC_TRACE_PRINTF_ALWAYS("SMC:   - RX packets dropped:   %8d", (int)device->stats.rx_dropped);


    /* smc_net_dev->platform_device->name,  */

    if( smc_net_dev->smc_instance != NULL )
    {
        smc_instance_dump( smc_net_dev->smc_instance );
    }
    else
    {
        SMC_TRACE_PRINTF_ALWAYS("  SMC instance is NULL");
    }

    return ret_val;
}


module_init(smc_platform_device_driver_init);
module_exit(smc_platform_device_driver_exit);


#if( SMCTEST == TRUE )

//#include "smc_test.h"

/*
 * SMC Test Interface functions for Linux Kernel
 */
uint8_t smc_test_linux_start(uint16_t test_case_id, uint16_t test_data_input_len, uint8_t* test_data_input)
{
    SMC_TRACE_PRINTF_INFO("smc_test_linux_start: Test case id 0x%04X, test data len %d...", test_case_id, test_data_input_len);

    return smc_test_handler_start(test_case_id, test_data_input_len, test_data_input);
}

EXPORT_SYMBOL(smc_test_linux_start);

#endif

/* EOF */
