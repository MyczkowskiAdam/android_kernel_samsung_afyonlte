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

Version:       3    23-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  Linux Net Device driver implementation

Version:       1    08-Nov-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMC_LINUX_H
#define SMC_LINUX_H

#define SMC_DRIVER_OK     0
#define SMC_DRIVER_ERROR  (-EINVAL)


#define SMC_PLATFORM_DRIVER_NAME    "smc_net_device"
    /*
     * The  device name format Smeco uses.
     * The postfix '%d' of the name is the interface number kernel creates for the driver, e.g "smc0", "smc1"
     */
#define SMC_NET_DEVICE_NAME_FORMAT   "smc%d"

    /*
     * Upper layer TX/RX callback function prototypes
     */
typedef int ( *upper_layer_skb_tx )( struct sk_buff *skb, struct net_device *device );
typedef int ( *upper_layer_skb_rx )( struct sk_buff *skb, struct net_device *device );

    /* IOCTL function for upper layers */
typedef int ( *upper_layer_net_device_driver_ioctl )( struct net_device* device, struct ifreq* ifr, int cmd );

    /* Queue selection handler function for upper layers */
typedef uint16_t ( *upper_layer_net_device_driver_select_queue )( struct net_device *device, struct sk_buff *skb );

    /*
     * SKB packet and SMC user data modification function for upper layer
     * This is called right before sending data to other CPU
     */

typedef int (* upper_layer_modify_send_data )( struct sk_buff *skb, smc_user_data_t* smc_user_data );

typedef void (* upper_layer_device_driver_setup )(struct net_device* dev);

    /* SMC Configuration creation function prototype */
typedef smc_conf_t* ( *smc_device_create_conf )( char* device_name );

typedef void (* upper_layer_device_driver_close )(struct net_device* dev);

    /*
     * Net Device specific configuration
     */
typedef struct _smc_device_driver_config
{
    char*              name;
    int                queue_count;

        /* SMC Configuration function */
    smc_device_create_conf smc_conf;

        /* TX and RX functions */
    upper_layer_skb_tx skb_tx_function;
    upper_layer_skb_rx skb_rx_function;

    upper_layer_net_device_driver_ioctl driver_ioctl;

    upper_layer_net_device_driver_select_queue driver_select_queue;

    upper_layer_modify_send_data driver_modify_send_data;

    upper_layer_device_driver_setup driver_setup;

        /* Link to the device */
    struct _smc_device_driver_priv_t* device_driver_priv;

    upper_layer_device_driver_close device_driver_close;

} smc_device_driver_config;


    /* SMC Network Device Driver object*/
typedef struct _smc_device_driver_priv_t
{
    struct net_device*        net_dev;
    struct platform_device*   platform_device;

        /* Single SMC instance in every device */
    smc_t*                    smc_instance;

    smc_device_driver_config* smc_dev_config;

} smc_device_driver_priv_t;


void smc_register_wakeup_irq( smc_t* smc_instance, uint32_t signal_id, uint32_t signal_type );

    /* Wakeup IRQ sense setup, function implemented in net device module */
void smc_set_ape_wakeup_irq_sense( uint8_t irq_sense );

#include "smc_linux_ioctl.h"

/**
 * DMA transfer function prototypes
 */
#ifdef SMC_DMA_ENABLED
    uint8_t smc_dma_init(struct device *device);
    uint8_t smc_dma_uninit(struct device *device);
#endif

#endif
