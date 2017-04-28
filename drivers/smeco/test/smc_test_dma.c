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

Version:       1    04-Dec-2012     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#include "smc_common_includes.h"

#include "smc_conf.h"
#include "smc_trace.h"
#include "smc_fifo.h"
#include "smc.h"
#include "smc_memory_mgmt.h"
#include "smc_test.h"
#include "smc_mdb.h"

    /* ===============================
     * DMA test cases for Linux Kernel
     */
#ifdef SMECO_LINUX_KERNEL

#include "smc_linux.h"
#include <linux/sh_dma.h>
#include <linux/dmaengine.h>
#include <asm/memory.h>

smc_device_driver_priv_t* g_smc_net_dev_priv = NULL;


#ifdef SMC_DMA_TRANSFER_ENABLED

static int     smc_test_dma_transfer(struct dma_chan* dma_channel, void* target_address, void* source_address, uint32_t length, int32_t source_address_offset);
static int     smc_test_dma_transfer_shm(struct dma_chan* dma_channel, void* target_address, void* source_address, uint32_t length);
static int     smc_test_dma_request_start(smc_device_driver_priv_t* smc_net_dev_priv, const void *buf, unsigned int buf_size);
static uint8_t smc_test_dma_filter(struct dma_chan *chan, void *slave);
static void    smc_test_dma_transfer_complete_cb(void *param);
static void    smc_test_dma_request_release(void);

#endif

/**
 * Initializes the SMC device to used in DMA test cases.
 */
void set_smc_device_driver_priv(smc_device_driver_priv_t* smc_net_dev_priv)
{
    g_smc_net_dev_priv = smc_net_dev_priv;
}


uint8_t smc_test_case_dma( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t test_status = SMC_ERROR;

#ifdef SMC_DMA_TRANSFER_ENABLED

    if( test_input_data == NULL || test_input_data_len < 3 )
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: invalid test data");
        return SMC_ERROR;
    }

    if( g_smc_net_dev_priv != NULL )
    {

        /* Use SMC channel 2 */
        smc_channel_t*  smc_channel = SMC_CHANNEL_GET(g_smc_net_dev_priv->smc_instance, 2);
        uint8_t         use_highmem = test_input_data[0];
        uint8_t         from_mdb    = test_input_data[1];
        uint8_t         dma_ret_val = SMC_DRIVER_OK;
        smc_user_data_t userdata;
        void*           source_address = NULL;
        void*           target_address = NULL;

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: DMA test invoke from device '%s'", g_smc_net_dev_priv->net_dev->name);

        if( from_mdb==0x00 && use_highmem!=0x03 )
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: DMA copy TO MDB implemented only for test case 0x03");
            return SMC_ERROR;
        }


        test_input_data_len -= 2;
        test_input_data += 2;

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: allocate memory for DMA source data (use_highmem==0x%08X)...", use_highmem);


        if( from_mdb==0x01 )
        {
            if( use_highmem == 0x01 || use_highmem==0x02 || use_highmem==0x03)
            {
                source_address = smc_mdb_alloc(smc_channel, test_input_data_len);
            }
            else
            {
                source_address = kmalloc(test_input_data_len, GFP_KERNEL);
            }

            target_address = kmalloc(test_input_data_len, GFP_KERNEL);
        }
        else
        {
            if( use_highmem == 0x01 || use_highmem==0x02 || use_highmem==0x03)
            {
                target_address = smc_mdb_alloc(smc_channel, test_input_data_len);
            }
            else
            {
                target_address = kmalloc(test_input_data_len, GFP_KERNEL);
            }

            source_address = kmalloc(test_input_data_len, GFP_KERNEL);
        }

        memcpy(source_address, test_input_data, test_input_data_len);
        SMC_SHM_CACHE_CLEAN( source_address, ((void*)(((uint32_t)source_address)+test_input_data_len)) );

        memset(target_address, 0xFF, test_input_data_len);
        SMC_SHM_CACHE_CLEAN( target_address, ((void*)(((uint32_t)target_address)+test_input_data_len)) );

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: start %d bytes DMA transfer from 0x%08X to 0x%08X...", test_input_data_len, (uint32_t)source_address, (uint32_t)target_address);

        for(int i = 0; i < test_input_data_len; i++ )
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: target data[%d]: 0x%02X", i, *((uint8_t*)(target_address+i)) );
        }

        if( use_highmem==0x03 )
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: Use SMC DMA function, from_mdb=0x%02X...", from_mdb);

            if( from_mdb == 0x01 )
            {
                dma_ret_val = smc_dma_transfer_mdb(smc_channel->smc_dma, target_address, SMC_MEMORY_VIRTUAL_TO_PHYSICAL( smc_channel->smc_instance, source_address ), test_input_data_len, from_mdb);
            }
            else
            {
                dma_ret_val = smc_dma_transfer_mdb(smc_channel->smc_dma, SMC_MEMORY_VIRTUAL_TO_PHYSICAL( smc_channel->smc_instance, target_address ), source_address, test_input_data_len, from_mdb);
            }

            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: Use SMC DMA function completed");
        }
        else
        {
            dma_cap_mask_t     mask;
            struct dma_chan*   dma_channel = NULL;


            dma_cap_zero(mask);
            dma_cap_set(DMA_SLAVE, mask);

            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: request DMA channel...");

            dma_channel = dma_request_channel(mask, NULL /*(dma_filter_fn)smc_test_dma_filter*/, NULL);

            if( dma_channel )
            {
                if( use_highmem==0x02 )
                {
                       /* Use the physical address */
                   dma_ret_val = smc_test_dma_transfer_shm( dma_channel, target_address, SMC_MEMORY_VIRTUAL_TO_PHYSICAL( smc_channel->smc_instance, source_address ), test_input_data_len);
               }
               else
               {
                   int32_t         mem_offset = 0;

                   if( smc_channel->smc_instance->smc_shm_conf )
                   {
                       mem_offset = (int32_t)smc_channel->smc_instance->smc_shm_conf->remote_cpu_memory_offset;
                   }

                   dma_ret_val = smc_test_dma_transfer( dma_channel, target_address, source_address, test_input_data_len, mem_offset);
               }

               SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: release DMA channel...");
               dma_release_channel(dma_channel);
            }
            else
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: DMA channel request failed");
            }
        }

        if( dma_ret_val == SMC_DRIVER_OK )
        {
            uint8_t transfer_ok = 0x01;
            uint8_t source_val  = 0x00;
            uint8_t target_val  = 0x00;

            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: %d bytes DMA transfer succeeded, source (0x%08X)/mdb (0x%08X)/target (0x%08X) data:", test_input_data_len, test_input_data, source_address, target_address);

            /* Compare the data */

            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: Compare the data transferred...");

            for(int i = 0; i < test_input_data_len; i++ )
            {
                source_val = *(uint8_t*)(source_address+i);
                target_val = *(uint8_t*)(target_address+i);

                if( source_val == target_val )
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: data in %d OK      source_address: 0x%02X == 0x%02X target_address", i, source_val, target_val);
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: data in %d INVALID source_address: 0x%02X != 0x%02X target_address", i, source_val, target_val);
                    transfer_ok = 0x00;
                }
            }

            if( transfer_ok==0x01)
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: ++++ DMA Transfer OK, data is succesfully transferred +++");
            }
            else
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: *** DMA Transfer failed: Data does not match!! ***");
            }

            test_status = SMC_OK;
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: DMA transfer failed");
            test_status = SMC_ERROR;
        }


        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: release allocated memory...");


        if( from_mdb==0x01 )
        {
            kfree(target_address);

            if( use_highmem == 0x01 || use_highmem == 0x02 || use_highmem==0x03)
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: release MDB memory (source)...");
                smc_channel_free_ptr_local( smc_channel, source_address, &userdata);
            }
            else
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: release kernel memory (source)...");
                kfree( source_address );
            }
         }
         else
         {
             kfree( source_address );

             if( use_highmem == 0x01 || use_highmem == 0x02 || use_highmem==0x03)
             {
                 SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: release MDB memory (target)...");
                 smc_channel_free_ptr_local( smc_channel, target_address, &userdata);
             }
             else
             {
                 SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_dma: release kernel memory (target)...");
                 kfree( target_address );
             }
         }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_dma: The SMC device is not set, unable to run DMA test");
    }

#else

    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_dma: The DMA transfer is not enabled");

    test_status = SMC_ERROR;
#endif

    return test_status;
}


#ifdef SMC_DMA_TRANSFER_ENABLED

static int smc_test_dma_transfer_shm(struct dma_chan* dma_channel, void* target_address, void* source_address, uint32_t length)
{
    int                             ret_value         = SMC_DRIVER_OK;
    dma_addr_t                      dma_target_addr;
    enum dma_ctrl_flags             flags;
    struct dma_async_tx_descriptor *dma_tx_descriptor = NULL;
    struct dma_device*              device              = dma_channel->device;

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer_shm: Transfer %d bytes from 0x%08X to 0x%08X", length, source_address, target_address);

    if( device == NULL )
    {
        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer_shm: invalid DMA device NULL");
        return SMC_DRIVER_ERROR;
    }

    if( dma_channel == NULL )
    {
        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer_shm: invalid DMA channel NULL");
        return SMC_DRIVER_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer_shm: map DMA target from 0x%08X, len %d...", target_address, length);
    dma_target_addr = dma_map_single(device->dev, target_address, length, DMA_FROM_DEVICE);   /* Force invalidate*/

    flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SKIP_SRC_UNMAP;

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer_shm: prepare DMA memcpy...");
    dma_tx_descriptor = device->device_prep_dma_memcpy(dma_channel, dma_target_addr, source_address, length, flags);

    if( dma_tx_descriptor )
    {
        static int        timeout = 1000;      /* 1 sec timeout */
        struct completion cmp;
        dma_cookie_t      cookie;
        enum dma_status   status;
        unsigned long     tmo = msecs_to_jiffies(timeout);

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer_shm: DMA descriptor OK, initialize completion 0x%08X...", &cmp);

        init_completion(&cmp);
        dma_tx_descriptor->callback = smc_test_dma_transfer_complete_cb;
        dma_tx_descriptor->callback_param = &cmp;
        cookie = dma_tx_descriptor->tx_submit(dma_tx_descriptor);

        if( dma_submit_error(cookie) )
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer_shm: DMA submit error cookie");
        }

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer_shm: start DMA transfer...");
        dma_async_issue_pending(dma_channel);

        tmo    = wait_for_completion_timeout(&cmp, tmo);
        status = dma_async_is_tx_complete(dma_channel, cookie, NULL, NULL);

        if (tmo == 0)
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer_shm: DMA transfer failed by timeout");
            ret_value = SMC_DRIVER_ERROR;
        }
        else if (status != DMA_SUCCESS)
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer_shm: DMA transfer failed by error %d", status);
            ret_value = SMC_DRIVER_ERROR;
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer_shm: DMA transfer OK");
        }

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer_shm: unmap target DMA handle...");
        dma_unmap_single(device->dev, dma_target_addr, length, DMA_FROM_DEVICE);  /**/
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer_shm: DMA prepare failed: DMA descriptor NULL");
        ret_value = SMC_DRIVER_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer_shm: Transfer of %d bytes from 0x%08X to 0x%08X completed by ret_value %d", length, source_address, target_address, ret_value);
    return ret_value;
}

static int smc_test_dma_transfer(struct dma_chan* dma_channel, void* target_address, void* source_address, uint32_t length, int32_t source_address_offset)
{
    int                             ret_value         = SMC_DRIVER_OK;
    struct dma_async_tx_descriptor *dma_tx_descriptor = NULL;

    enum dma_ctrl_flags             flags;
    dma_addr_t                      dma_source_addr;
    dma_addr_t                      dma_target_addr;
    struct dma_device*              device              = dma_channel->device;
    struct page*                    page_source         = NULL;
    unsigned long                   source_address_high = 0;

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: Transfer %d bytes from 0x%08X to 0x%08X", length, source_address, target_address);

    if( device == NULL )
    {
        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer: invalid DMA device NULL");
        return SMC_DRIVER_ERROR;
    }

    if( dma_channel == NULL )
    {
        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer: invalid DMA channel NULL");
        return SMC_DRIVER_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: DMA map: PAGE_OFFSET=0x%08X, highmemory=0%08X", PAGE_OFFSET, high_memory);

        /* Map DMA source and target */
    if( source_address < high_memory )
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: map DMA SINGLE source from 0x%08X, len %d...", source_address, length);

            /* DMA_FROM_DEVICE == from SHM / DMA_TO_DEVICE == to SHM*/
        dma_source_addr = dma_map_single(device->dev, source_address, length, DMA_FROM_DEVICE);     /* DMA_TO_DEVICE */
    }
    else
    {
            /* SHM in highmem*/
        unsigned long                   offset_source       = 0;
        unsigned long                   page_source_address = 0;

        page_source = virt_to_page(source_address);

        //get_page(page_source);

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: highmem usage: Virtual address of page 0x%08X == 0x%08X, PAGE_MASK==0x%08X", (uint32_t)page_source, (uint32_t)page_address(page_source), PAGE_MASK);

        page_source_address = kmap_high( page_source );
        offset_source       = ((unsigned long)source_address) & ~PAGE_MASK;
        source_address_high = ((uint32_t)page_source_address) + offset_source;

        memset( source_address_high, 0xEE, length);

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: highmem usage: virtual address of page 0x%08X from kmap_high == 0x%08X", (uint32_t)page_source, page_source_address );
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: %d bytes from virtual address 0x%08X:", length, ((uint32_t)page_source_address)+offset_source);
        SMC_TEST_TRACE_PRINTF_INFO_DATA( length, source_address_high );
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: map DMA source from 0x%08X (highmem), len %d, page=0x%08X (address 0x%08X), offset 0x%08X, final address 0x%08X (HIGH)...",
                source_address, length, (uint32_t)page_source, page_source_address, offset_source, source_address_high);

        //dma_source_addr = dma_map_page(device->dev, page_source, offset_source, length, DMA_FROM_DEVICE);     /* DMA_TO_DEVICE / DMA_FROM_DEVICE */
        //dma_source_addr = dma_map_single(device->dev, source_address_high, length, DMA_FROM_DEVICE);


        dma_source_addr = source_address - source_address_offset;

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: Put SHM address directly as DMA address -> 0x%08X", dma_source_addr);
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: map DMA target from 0x%08X, len %d...", target_address, length);
    dma_target_addr = dma_map_single(device->dev, target_address, length, DMA_FROM_DEVICE);   /* Force invalidate*/

        /* Set control flags:
         * (src buffers are freed by the DMAEngine code with dma_unmap_single())
         * src buffers are freed by ourselves
         * dst buffers are freed by ourselves
         */
    //flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SRC_UNMAP_SINGLE;
    flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SKIP_SRC_UNMAP;

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: prepare DMA memcpy...");
    dma_tx_descriptor = device->device_prep_dma_memcpy(dma_channel, dma_target_addr, dma_source_addr, length, flags);

    if( dma_tx_descriptor )
    {
        static int        timeout = 1000;      /* 1 sec timeout */
        struct completion cmp;
        dma_cookie_t      cookie;
        enum dma_status   status;
        unsigned long     tmo = msecs_to_jiffies(timeout);

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: DMA descriptor OK, initialize completion 0x%08X...", &cmp);

        init_completion(&cmp);
        dma_tx_descriptor->callback = smc_test_dma_transfer_complete_cb;
        dma_tx_descriptor->callback_param = &cmp;
        cookie = dma_tx_descriptor->tx_submit(dma_tx_descriptor);

        if( dma_submit_error(cookie) )
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer: DMA submit error cookie");
        }

        for(int i = 0; i < length; i++ )
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: before DMA transfer: target data[%d]: 0x%02X", i, *((uint8_t*)(target_address+i)) );
        }

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: print kmapped before DMA transfer...");
        for(int i = 0; i < length; i++ )
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: before DMA transfer: source_address_high data[%d]: 0x%02X", i, *((uint8_t*)(source_address_high+i)) );
        }



        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: start DMA transfer...");
        dma_async_issue_pending(dma_channel);

        tmo    = wait_for_completion_timeout(&cmp, tmo);
        status = dma_async_is_tx_complete(dma_channel, cookie, NULL, NULL);

        if (tmo == 0)
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer: DMA transfer failed by timeout");
            ret_value = SMC_DRIVER_ERROR;
        }
        else if (status != DMA_SUCCESS)
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer: DMA transfer failed by error %d", status);
            ret_value = SMC_DRIVER_ERROR;
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer: DMA transfer OK");
        }

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: print kmapped after DMA transfer...");

        for(int i = 0; i < length; i++ )
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: after DMA transfer: source_address_high data[%d]: 0x%02X", i, *((uint8_t*)(source_address_high+i)) );
        }


        if( page_source != NULL && source_address >= high_memory )
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: unmap source DMA handle PAGE...");
            dma_unmap_page(device->dev, dma_source_addr, length, DMA_FROM_DEVICE);
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: unmap source DMA handle SINGLE...");
            dma_unmap_single(device->dev, dma_source_addr, length, DMA_FROM_DEVICE);  /**/
        }

        if( page_source != NULL && source_address >= high_memory )
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: unmap highmemory virtual address before page unmap...");
            kunmap_high( page_source );
        }

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: unmap target DMA handle...");
        dma_unmap_single(device->dev, dma_target_addr, length, DMA_FROM_DEVICE);  /**/

        page_source = NULL;
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_transfer: DMA descriptor NULL");
        ret_value = SMC_DRIVER_ERROR;
    }

    if( page_source != NULL && source_address >= high_memory )
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: unmap highmemory virtual address...");
        kunmap_high( page_source );
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer: Transfer of %d bytes from 0x%08X to 0x%08X completed by ret_value %d", length, source_address, target_address, ret_value);
    return ret_value;
}


/**
 * OBSOLETE FUNCTION CURRENTLY
 */
static int smc_test_dma_request_start(smc_device_driver_priv_t* smc_net_dev_priv, const void *buf, unsigned int buf_size)
{
    int ret_value = SMC_DRIVER_OK;

    struct dma_chan*      dma_channel = NULL;
    dma_cap_mask_t        mask;
    struct sh_dmae_slave* dma_user_fn_param = NULL;

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_request_start: Data 0x%08X, len %d", (uint32_t)buf, buf_size);

    /* Initialize the DMA transfer */

    //dma_user_fn_param = kzalloc( sizeof(struct sh_dmae_slave), GFP_KERNEL);
    //dma_user_fn_param->slave_id = ;

    dma_cap_zero(mask);
    dma_cap_set(DMA_SLAVE, mask);

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_request_start: dma_request_channel dma_user_fn_param=0x%08X", (uint32_t)dma_user_fn_param);

    dma_channel = dma_request_channel(mask, (dma_filter_fn)smc_test_dma_filter, dma_user_fn_param);

    if( dma_channel != NULL )
    {
        struct scatterlist *tx_sg = NULL;
        int nent = 0;
        struct dma_async_tx_descriptor* tx_desc = NULL;
        dma_cookie_t tx_cookie;

        SMC_TEST_TRACE_PRINTF_INFO( "smc_test_dma_request_start: DMA channel 0x%08X received", (uint32_t)dma_channel );

        tx_sg = kmalloc(sizeof(struct scatterlist),GFP_KERNEL);

        sg_init_table(tx_sg, 1);
        sg_set_page(tx_sg, virt_to_page(buf), PAGE_SIZE, offset_in_page(buf));

        SMC_TEST_TRACE_PRINTF_INFO( "smc_test_dma_request_start: dma_map_sg( tx_sq=0x%08X )", (uint32_t)tx_sg);

        nent = dma_map_sg(&smc_net_dev_priv->platform_device->dev, tx_sg, 1, DMA_TO_DEVICE);

        //sg_dma_address(tx_sg) = (sg_dma_address(tx_sg) & ~(PAGE_SIZE  - 1));

        SMC_TEST_TRACE_PRINTF_INFO( "smc_test_dma_request_start: call device_prep_slave_sg(tx_sg=0x%08X, nent=%d) ...", (uint32_t)tx_sg, nent );

        tx_desc = dma_channel->device->device_prep_slave_sg(dma_channel, tx_sg, nent, DMA_TO_DEVICE, DMA_PREP_INTERRUPT|DMA_CTRL_ACK);

        if( tx_desc != NULL )
        {
            tx_desc->callback = smc_test_dma_transfer_complete_cb;
            tx_desc->callback_param = dma_channel;

            SMC_TEST_TRACE_PRINTF_INFO( "smc_test_dma_request_start: call tx_desc->tx_submit()..." );

            tx_cookie = tx_desc->tx_submit(tx_desc);

            SMC_TEST_TRACE_PRINTF_INFO( "smc_test_dma_request_start: call dma_async_issue_pending()..." );

            dma_async_issue_pending(dma_channel);
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_request_start: failed in device_prep_slave_sg");
            ret_value = SMC_DRIVER_ERROR;
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_dma_request_start: failed to get DMA channel");
        ret_value = SMC_DRIVER_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_ERROR( "smc_test_dma_request_start: completed by ret val %d", ret_value);

    return ret_value;
}


static uint8_t smc_test_dma_filter(struct dma_chan *chan, void *slave)
{
    //struct sh_dmae_slave *param = slave;

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_filter: Channel 0x%08X, Slave 0x%08X", (uint32_t)chan, (uint32_t)slave);


    //chan->private = param;


    return TRUE;
}

static void smc_test_dma_transfer_complete_cb(void *param)
{
    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_transfer_complete_cb: param = 0x%08X -> complete", (uint32_t)param);

    complete(param);


}

static void smc_test_dma_request_release( void )
{
    SMC_TEST_TRACE_PRINTF_INFO("smc_test_dma_request_release: invoked");

}

#endif

#else   /* #ifdef SMECO_LINUX_KERNEL */

uint8_t smc_test_case_dma( uint8_t* test_input_data, uint32_t test_input_data_len )
{
    uint8_t test_status = SMC_ERROR;


    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_dma: Test not supported");

    return test_status;
}

#endif


/* EOF */

