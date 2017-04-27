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

Version:       1    19-Oct-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#include "smc_common_includes.h"

/*#include "smc_conf.h"*/
#include "smc_trace.h"
#include "smc_fifo.h"
#include "smc.h"
#include "smc_memory_mgmt.h"
#include "smc_test.h"
#include "smc_mdb.h"


#if(SMC_L2MUX_IF==TRUE)
#include "smc_config_l2mux.h"

void smc_suppress_warn(void)
{
    (void)smc_instance_conf_l2mux;
}

#ifdef SMECO_MODEM
#include "smc_conf_l2mux_modem.h"
#else
#include "smc_conf_l2mux_linux.h"
#endif

#endif


#ifdef SMECO_LINUX_ANDROID
   /* User space specific includes */
#elif defined SMECO_LINUX_KERNEL
   /* Kernel space specific includes */
#else

#include "pn_const.h"

#if (MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L2)

#include "pn.h"
#include "pn_name.h"

#include "smc.h"

#elif (MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L1)

#include "mmsgr.h"

#endif

#endif

modem_test_group_id_t smc_test_handler_group_id        = SMC_TEST_GROUP_ID;
modem_test_group_id_t smc_test_handler_group_id_remote = SMC_TEST_GROUP_ID_REMOTE;



static uint8_t smc_test_case_function_configure_shared_mem( uint8_t* test_input_data, uint16_t test_input_data_len );


    /*
     * SMC instance creation for test cases
     */
static smc_t* g_smc_instance1 = NULL;

static smc_t* get_smc_instance_1( uint8_t is_master, uint32_t shm_start_address );

static smc_channel_conf_t* smc_test_case_create_channel_conf_1(uint8_t is_master);
static void  smc_receive_data_callback_channel_conf_1(void*   data,
                                                      int32_t data_length,
                                                      const struct _smc_user_data_t* userdata,
                                                      const struct _smc_channel_t* channel);

static void  smc_test_case_channel_1_deallocator_callback(smc_channel_t* smc_channel, void* ptr, struct _smc_user_data_t* userdata);
static void* smc_test_case_channel_1_allocator_callback(smc_channel_t* smc_channel, uint32_t size, struct _smc_user_data_t* userdata);
static void  smc_test_case_channel_1_event_callback(smc_channel_t* smc_channel, SMC_CHANNEL_EVENT event, void* event_data);

//static smc_channel_conf_t* smc_test_case_create_channel_conf_2(uint8_t is_master);
static uint8_t* smc_test_case_create_shm_conf_local( smc_conf_t* smc_conf, uint8_t* shm_mem_start_addr );


uint8_t smc_test_handler_send_message(uint16_t test_case_id, uint8_t* test_input_data, uint16_t test_input_data_len);



    /* Test configuration function used as testcase */
uint8_t smc_test_case_function_configure   ( uint8_t* test_input_data, uint16_t test_input_data_len );
uint8_t smc_test_case_function_smc_instance( uint8_t* test_input_data, uint16_t test_input_data_len );
uint8_t smc_test_case_function_remote_event( uint8_t* test_input_data, uint16_t test_input_data_len );

    /* Signal test case */
static uint8_t smc_test_case_function_signal( uint8_t* test_input_data, uint16_t test_input_data_len );

    /* Configuration instance test case */
static uint8_t smc_test_case_function_create_configuration( uint8_t* test_input_data, uint16_t test_input_data_len );

    /* Shared memory test case */
static uint8_t smc_test_case_function_shm( uint8_t* test_input_data, uint16_t test_input_data_len );

    /* Timer test case */
static uint8_t smc_test_case_function_timer( uint8_t* test_input_data, uint16_t test_input_data_len );

static uint8_t smc_test_case_function_misc( uint8_t* test_input_data, uint16_t test_input_data_len );

static uint8_t smc_test_case_function_ping( uint8_t* test_input_data, uint16_t test_input_data_len );

static uint8_t smc_test_case_function_loopback( uint8_t* test_input_data, uint16_t test_input_data_len );

static uint8_t smc_test_case_shm_variable( uint8_t* test_input_data, uint16_t test_input_data_len );

    /* DMA test case in smc_test_dma.c */
extern uint8_t smc_test_case_dma( uint8_t* test_input_data, uint16_t test_input_data_len );

    /* RPCL test */
static uint8_t smc_test_case_rpcl( uint8_t* test_input_data, uint16_t test_input_data_len );

    /* Test to set various channel states*/
static uint8_t smc_test_case_channel_state( uint8_t* test_input_data, uint16_t test_input_data_len );

    /* Special tests for FIFO in the channel */
static uint8_t smc_test_case_channel_fifo( uint8_t* test_input_data, uint16_t test_input_data_len );

static uint8_t smc_test_case_send_data_raw( uint8_t* test_input_data, uint16_t test_input_data_len );

    /* ========================================================
     * SMC Test Case functions
     * First index is 0x00
     * Index is the test case ID inside SMC test handler
     */
smc_test_case_function smc_test_cases[] =
{
    smc_test_case_function_configure,               /* 0x00 */
    smc_test_case_function_fifo,                    /* 0x01 in smc_test_fifo.c */
    smc_test_case_function_smc_instance,            /* 0x02 */
    smc_test_case_function_messaging,               /* 0x03 */
    smc_test_case_function_signal,                  /* 0x04 */
    smc_test_case_function_mdb,                     /* 0x05 in smc_test_mdb.c */
    smc_test_case_function_remote_event,            /* 0x06 */
    smc_test_case_function_create_configuration,    /* 0x07 */
    smc_test_case_function_shm,                     /* 0x08 */
    smc_test_case_function_timer,                   /* 0x09 */
    smc_test_case_function_misc,                    /* 0x0A */
    smc_test_case_function_ping,                    /* 0x0B */
    smc_test_case_function_loopback,                /* 0x0C */
    smc_test_case_dma,                              /* 0x0D */
    smc_test_case_shm_variable,                     /* 0x0E */
    smc_test_case_rpcl,                             /* 0x0F */
    smc_test_case_channel_state,                    /* 0x10 */
    smc_test_case_channel_fifo,                     /* 0x11 */
    smc_test_case_send_data_raw,                    /* 0x12 */
    0
};


/*
 * SMC Test Handler ID (in modem BFAT):
 * #define SMC_TEST_HDLR   31  (0x1F)
 */
modem_test_status_t smc_test_handler(modem_test_req_str_t *req)
{
    modem_test_status_t test_status = MODEM_TEST_FAILED;

    (void)modem_test_set_timeout(2000);

    SMC_TEST_TRACE_PRINTF_INFO("Testcase ID 0x%02X, INP Data len %d, data from 0x%08X", req->case_ID, req->inp_data_length, (uint32_t)req->inp_data);
    SMC_TEST_TRACE_PRINTF_INFO_DATA(req->inp_data_length, req->inp_data);

    if( req->case_ID < ((uint16_t)(sizeof(smc_test_cases)/sizeof(smc_test_case_function))) )
    {
        if( smc_test_cases[req->case_ID] != NULL )
        {
            int test_ret = smc_test_cases[req->case_ID]( req->inp_data, req->inp_data_length );

            if( test_ret == SMC_OK )
            {
                SMC_TEST_TRACE_PRINTF_INFO("Test Case 0x%02X passed", req->case_ID);
                test_status = MODEM_TEST_PASSED;
            }
            else
            {
                SMC_TEST_TRACE_PRINTF_INFO("Test Case 0x%02X FAILED!", req->case_ID);
                test_status = MODEM_TEST_FAILED;
            }
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_INFO("Test Case 0x%02X function invalid (NULL)", req->case_ID);
            test_status = MODEM_TEST_FAILED;
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("Test Case 0x%02X DOES NOT EXIST", req->case_ID);
        test_status = MODEM_TEST_FAILED;
    }

    return test_status;
}

uint8_t smc_test_handler_start(uint16_t test_case_id, uint16_t test_data_input_len, uint8_t* test_data_input)
{
    modem_test_status_t status;
    modem_test_req_str_t* req = (modem_test_req_str_t*)SMC_MALLOC(sizeof(modem_test_req_str_t));

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_handler_start: initializing...");

    req->case_ID = test_case_id;
    req->inp_data_length = test_data_input_len;
    req->inp_data = test_data_input;

    status = smc_test_handler(req);

    SMC_FREE(req);

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_handler_start: completed by status 0x%02X", status);

    if( status == MODEM_TEST_PASSED )
    {
        return SMC_OK;
    }
    else
    {
        return SMC_ERROR;
    }
}


/**
 * SMC test case for configuring the test environment.
 *
 * Test Input Data format header:
 *  byte[0]  == config message
 *
 * -> Message specific data might require more input data
 */
uint8_t smc_test_case_function_configure( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;
    uint16_t test_input_len_required = 1;

    if( test_input_data_len >= test_input_len_required )
    {
        uint8_t data_index = 0;

        uint8_t config_msg =  test_input_data[data_index];
        data_index++;
        test_input_data_len--;

        switch(config_msg)
        {
            case SMC_TEST_CONFIG_SET_SHARED_SDRAM:
            {
                test_status = smc_test_case_function_configure_shared_mem( (uint8_t*)(test_input_data+data_index), test_input_data_len);
                break;
            }
            default:
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_configure: unknown configure message 0x%02X", config_msg);
                test_status = SMC_ERROR;
            }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_configure: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_configure: completed with return value 0x%02X", test_status);

    return test_status;
}

static uint8_t smc_test_case_function_signal( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;

    uint16_t test_input_len_required = 9;

    if( test_input_data_len >= test_input_len_required )
    {
        uint8_t  data_index  = 0;
        uint32_t signal_id   = 0;
        uint32_t signal_type = 0;
        uint8_t  test_case   = test_input_data[data_index++];

        signal_id   = SMC_BYTES_TO_32BIT( (test_input_data+data_index) );
        data_index+= 4;
        signal_type = SMC_BYTES_TO_32BIT( (test_input_data+data_index) );

        switch(test_case)
        {
            case 0x00:
            case 0x02:
            {
                smc_signal_t* signal = NULL;
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_signal: Create signal 0x%02X (%d), type 0x%08X...", signal_id, signal_id, signal_type);

                signal = smc_signal_create( signal_id, signal_type );

                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_signal: Raising signal 0x%08X...", (uint32_t)signal);

                test_status = smc_signal_raise( signal );

                if( test_case == 0x02 )
                {
                    volatile uint32_t delay = 2000;

                    while(delay > 0 ) delay--;

                    smc_signal_acknowledge( signal );

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_signal: Acknowledged signal 0x%08X...", (uint32_t)signal);
                }

                smc_signal_destroy( signal );

                break;
             }
             case 0x01:
             {
                 smc_signal_t* signal = NULL;
                 SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_signal: Create signal 0x%02X (%d), type 0x%08X...", signal_id, signal_id, signal_type);

                 signal = smc_signal_create( signal_id, signal_type );

                 SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_signal: Registering handler for signal 0x%08X...", (uint32_t)signal);

                     /* Register without any SMC obejcts */
                 test_status = smc_signal_handler_register( NULL, signal, NULL);

                 SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_signal: Signal handler registered");

                 break;
             }
             default:
             {
                 SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_signal: Invalid test case 0x%02X", test_case);
                 test_status = SMC_ERROR;
             }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_signal: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_signal: completed with return value 0x%02X", test_status);

    return test_status;
}


static smc_t* get_smc_instance_1( uint8_t is_master, uint32_t shm_start_address )
{
    if( !g_smc_instance1 )
    {
            /* ====================================================
             * Create SMC configuration
             */
        smc_channel_conf_t* smc_channel_conf_1 = NULL;
        smc_conf_t*         smc_conf           = smc_conf_create();

        smc_conf->cpu_id_local  = SMC_CPU_ID_LOCAL;
        smc_conf->cpu_id_remote = SMC_CPU_ID_REMOTE;
        smc_conf->is_master     = is_master;

        SMC_TEST_TRACE_PRINTF_INFO("get_smc_instance_1: Creating channel configurations...");

        smc_channel_conf_1 = smc_test_case_create_channel_conf_1(is_master);

        smc_conf_add_channel_conf(smc_conf, smc_channel_conf_1);


            /* ====================================================
             * Create the SHM
             * NOTE: Must be after channel configuration
             */
        SMC_TEST_TRACE_PRINTF_INFO("get_smc_instance_1: Creating SHM configuration...");
        smc_test_case_create_shm_conf_local( smc_conf, (uint8_t*)shm_start_address );

        g_smc_instance1 = smc_instance_create(smc_conf);
    }

    return g_smc_instance1;
}

/*
 * Send message ptr deallocator callback
 */
static void smc_test_case_channel_1_deallocator_callback(smc_channel_t* smc_channel, void* ptr, struct _smc_user_data_t* userdata)
{
    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_1_deallocator_callback: Not deallocating message PTR 0x%08X from channel 0x%08X because test case inpout data used", (uint32_t)ptr, (uint32_t)smc_channel);
}

static void* smc_test_case_channel_1_allocator_callback(smc_channel_t* smc_channel, uint32_t size, struct _smc_user_data_t* userdata)
{
    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_1_allocator_callback: Allocating data size of %d for channel 0x%08X", size, (uint32_t)smc_channel);

    return (void*)SMC_MALLOC(size);
}

static void  smc_test_case_channel_1_event_callback(smc_channel_t* smc_channel, SMC_CHANNEL_EVENT event, void* event_data)
{
    SMC_TEST_TRACE_PRINTF_INFO("=====================================================================");
    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_1_event_callback: Event %d from channel 0x%08X", event, (uint32_t)smc_channel);
    SMC_TEST_TRACE_PRINTF_INFO("=====================================================================");
}

static smc_channel_conf_t* smc_test_case_create_channel_conf_1(uint8_t is_master)
{
    smc_channel_conf_t* smc_channel_conf_1 = smc_channel_conf_create();

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_create_channel_conf_1: Starting...");

        /* Set the channel configuration */
    smc_channel_conf_1->priority      = SMC_CHANNEL_PRIORITY_HIGHEST;

    if( is_master )
    {
        smc_channel_conf_1->fifo_size_in  = 25;
        smc_channel_conf_1->fifo_size_out = 20;
        smc_channel_conf_1->mdb_size_in   = 120*1024;   /* 120 kB */
        smc_channel_conf_1->mdb_size_out  = 100*1024;   /* 100 kB */
    }
    else
    {
        smc_channel_conf_1->fifo_size_in  = 20;
        smc_channel_conf_1->fifo_size_out = 25;
        smc_channel_conf_1->mdb_size_in   = 100*1024;   /* 100 kB */
        smc_channel_conf_1->mdb_size_out  = 120*1024;   /* 120 kB */
    }

        /* Configure the callbacks */
    smc_channel_conf_1->smc_receive_data_cb           = (void*)smc_receive_data_callback_channel_conf_1;
    smc_channel_conf_1->smc_send_data_deallocator_cb  = (void*)smc_test_case_channel_1_deallocator_callback;
    smc_channel_conf_1->smc_receive_data_allocator_cb = (void*)smc_test_case_channel_1_allocator_callback;
    smc_channel_conf_1->smc_event_cb                  = (void*)smc_test_case_channel_1_event_callback;

    /* Configure the signals */

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_create_channel_conf_1: Configuring signals...");
#ifdef SMECO_MODEM
#if (MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L2)

    smc_channel_conf_1->signal_remote = smc_signal_create(C1_L1_CPU_Int_Gen_Ch2, SMC_SIGNAL_TYPE_INTGEN);
    smc_channel_conf_1->signal_local  = smc_signal_create(C2_L2_CPU_Int_Gen_Ch2, SMC_SIGNAL_TYPE_INTGEN);

#elif (MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L1)

    smc_channel_conf_1->signal_remote = smc_signal_create(C2_L2_CPU_Int_Gen_Ch2, SMC_SIGNAL_TYPE_INTGEN);
    smc_channel_conf_1->signal_local  = smc_signal_create(C1_L1_CPU_Int_Gen_Ch2, SMC_SIGNAL_TYPE_INTGEN);

#endif
#else

#endif

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_create_channel_conf_1: Completed");

    return smc_channel_conf_1;
}

static uint8_t* smc_test_case_create_shm_conf_local( smc_conf_t* smc_conf, uint8_t* shm_mem_start_addr )
{
    uint8_t* shm_address = NULL;

        /* =========================================================
         * Create the Shared memory allocating it from dynamic pool
         */
    smc_conf->smc_shm_conf = smc_shm_conf_create();

    smc_conf->smc_shm_conf->size = 0;

    for( int i = 0; i < smc_conf->smc_channel_conf_count; i++)
    {
        smc_conf->smc_shm_conf->size += smc_channel_calculate_required_shared_mem(smc_conf->smc_channel_conf_ptr_array[i]);
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_create_shm_conf_local: %d bytes of SHM is needed", smc_conf->smc_shm_conf->size);

    if( shm_mem_start_addr > 0 )
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_create_shm_conf_local: Slave using SHM from 0x%08X created by master", (uint32_t)shm_mem_start_addr);
        shm_address = shm_mem_start_addr;
    }
    else
    {
        shm_address = (uint8_t*)SMC_MALLOC( smc_conf->smc_shm_conf->size );

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_create_shm_conf_local: Master allocated SHM from 0x%08X", (uint32_t)shm_address);
    }

    smc_conf->smc_shm_conf->shm_area_start_address = shm_address;

#ifdef SMECO_MODEM
    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_create_shm_conf_local: Modem uses cache control");
    smc_conf->smc_shm_conf->use_cache_control = TRUE;
#endif

    return shm_address;
}

uint8_t smc_test_case_function_remote_event( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status             = SMC_ERROR;
    uint16_t test_input_len_required = 2;

    if( test_input_data_len >= test_input_len_required )
    {
        uint8_t channel_id      = test_input_data[0];
        SMC_CHANNEL_EVENT event = (SMC_CHANNEL_EVENT)test_input_data[1];

        uint8_t smc_instance_id = 0x00;

            /* Expecting that the SMC instance is created */
        //smc_t* smc = get_smc_instance_1(TRUE, 0x00);

        smc_t* smc = smc_test_get_instance_by_test_instance_id( smc_instance_id );

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_remote_event: Sending event %d to remote using SMC instance %d channel %d", test_input_data[1], smc_instance_id, test_input_data[0]);

        test_status = smc_send_event( smc_channel_get(smc, channel_id), event);

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_remote_event: Sending event %d to remote using channel %d completed", test_input_data[1], test_input_data[0]);
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_remote_event: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_remote_event: completed with return value 0x%02X", test_status);

    return test_status;
}

static uint8_t smc_test_case_function_ping( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status             = SMC_ERROR;
    uint16_t test_input_len_required = 1;

    if( test_input_data_len >= test_input_len_required )
    {
        uint8_t channel_id      = test_input_data[0];
        uint8_t smc_instance_id = 0x00;                 /* TODO Take instance id into use */
        uint8_t wait_for_reply  = TRUE;

        smc_t* smc = smc_test_get_instance_by_test_instance_id( smc_instance_id );

        if( test_input_data_len > 1 )
        {
            if( test_input_data[1] == 0 )
            {
                wait_for_reply  = FALSE;
            }
        }

        if( smc != NULL )
        {
            smc_channel_t* smc_channel = smc_channel_get(smc, channel_id);

            if( smc_channel != NULL )
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_ping: Sending ping to channel %d using SMC instance %d ", channel_id, smc_instance_id );

                test_status = smc_channel_send_ping( smc_channel, wait_for_reply );

                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_ping: Sending ping to channel %d completed", channel_id);
            }
            else
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_ping: No SMC channel %d available (instance id 0x%02X)", channel_id, smc_instance_id);
                test_status = SMC_ERROR;
            }
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_ping: No SMC instance available (id 0x%02X)", smc_instance_id);
            test_status = SMC_ERROR;
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_ping: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_ping: completed with return value 0x%02X", test_status);

    return test_status;
}

static uint8_t smc_test_case_function_loopback( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status             = SMC_ERROR;
    uint16_t test_input_len_required = 10;

    if( test_input_data_len >= test_input_len_required )
    {
        uint8_t        data_index      = 0;
        uint8_t        smc_instance_id = test_input_data[data_index++];
        uint8_t        channel_id      = test_input_data[data_index++];

                         /* TODO Take instance id into use */
        smc_channel_t* smc_channel     = NULL;
        smc_t*         smc             = smc_test_get_instance_by_test_instance_id( smc_instance_id );
        uint32_t       lb_data_len     = 0;
        uint32_t       lb_rounds       = 0;


        if( smc != NULL )
        {
            lb_data_len = SMC_BYTES_TO_32BIT( (test_input_data+data_index) );
            data_index += 4;

            lb_rounds   = SMC_BYTES_TO_32BIT( (test_input_data+data_index) );
            data_index += 4;

            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_loopback: Starting loopback in channel %d using SMC instance %d: Message len %d, rounds %d", channel_id, smc_instance_id, lb_data_len, lb_rounds);

            smc_channel = smc_channel_get(smc, channel_id);

            test_status = smc_send_loopback_data_message( smc_channel, lb_data_len, lb_rounds, FALSE );

            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_loopback: Loopback test in channel %d completed", smc_channel->id);
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_loopback: Unable to start loopback test, no SMC instance available");
            test_status = SMC_ERROR;
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_loopback: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_loopback: completed with return value 0x%02X", test_status);

    return test_status;
}


smc_t* smc_test_get_instance_by_test_instance_id( uint8_t smc_instance_id)
{
    smc_t*  smc_instance    = NULL;

    if( smc_instance_id == 0x00 )
    {
#if( (SMC_L2MUX_IF==TRUE) )
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_get_instance_by_test_instance_id: get L2MUX SMC instance...");
        smc_instance = get_smc_instance_l2mux();
#else
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_get_instance_by_test_instance_id: L2MUX SMC instance not available");
#endif
    }
    else if( smc_instance_id == 0x01 )
    {
#if(SMC_CONTROL==TRUE)
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_get_instance_by_test_instance_id: get SMC control...");
        smc_instance = smc_instance_get_control();
#else
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_get_instance_by_test_instance_id: SMC control instance not available");
#endif
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_get_instance_by_test_instance_id: Unsupported SMC test instance id 0x%02X", smc_instance_id);
    }

    return smc_instance;
}



uint8_t smc_test_case_function_smc_instance( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;

    uint16_t test_input_len_required = 2;

    if( test_input_data_len >= test_input_len_required )
    {
        uint8_t data_index = 0;

        uint8_t test_case =  test_input_data[data_index];
        data_index++;

        switch(test_case)
        {
            case 0x00:
            {
                uint8_t  is_master          = TRUE;
                uint32_t shm_start_address  = 0x00000000;
                smc_t*   smc                = NULL;

#ifdef SMECO_MODEM
                uint8_t  test_case_data[6];
                uint8_t  start_remote_side  = TRUE;
                uint32_t shm_address        = 0x00000000;
#endif

                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: Test case 0x%02X, creating SMC instance...", test_case);

#ifdef SMECO_MODEM
#if (MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L1)
                is_master = FALSE;

                    /* We need the SHM address from test input data */
                if( test_input_data_len >= test_input_len_required+4 )
                {
                    shm_start_address = SMC_BYTES_TO_32BIT( (test_input_data+2) );

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: shm_start_address==0x%08X", (uint32_t)shm_start_address);
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance:not enough test input data, SHM pointer is missing for slave SMC");
                    return SMC_ERROR;
                }
#else
                /* L2 specific code */
                if( test_input_data[data_index] == 0x01 )
                {
                    start_remote_side = FALSE;
                }

                data_index++;

#endif
#endif
                smc = get_smc_instance_1(is_master, shm_start_address);

#ifdef SMECO_MODEM
#if (MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L2)

                if( start_remote_side )
                {
                    shm_address = (uint32_t)smc->smc_shm_conf->shm_area_start_address;

                        /* L2 sends message to L1 to create SMC instance */
                    test_case_data[0] = 0x00;
                    test_case_data[1] = 0x00;

                    test_case_data[2] = (shm_address>>24)&0xFF;
                    test_case_data[3] = (shm_address>>16)&0xFF;
                    test_case_data[4] = (shm_address>>8)&0xFF;
                    test_case_data[5] = shm_address&0xFF;

                    if( smc_test_handler_send_message(0x02, test_case_data, 6) == SMC_OK )
                    {
                        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: smc_test_handler_send_message to L1 OK");
                    }
                    else
                    {
                        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance: smc_test_handler_send_message to L1 FAILED");
                    }
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: The remote side SMC instance is not started, use separate test message");
                }
#endif
#endif
                smc_instance_dump( smc );

                test_status = SMC_OK;
                break;
            }
            case 0x01:
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: Test case 0x%02X, Sending message...", test_case);

                if( g_smc_instance1 )
                {
                    /* Use the default */

                    uint8_t  smc_channel_id = 0;
                    uint32_t data_length    = (test_input_data_len-data_index);
                    uint8_t* data           = (test_input_data + data_index);
                    smc_user_data_t userdata;

                    userdata.flags     = 0x00000000;
                    userdata.userdata1 = 0x00000000;
                    userdata.userdata2 = 0x00000000;
                    userdata.userdata3 = 0x00000000;
                    userdata.userdata4 = 0x00000000;
                    userdata.userdata5 = 0x00000000;

                    test_status = smc_send_ext(SMC_CHANNEL_GET(g_smc_instance1, smc_channel_id), (void*)data, data_length, &userdata);
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance: SMC instance not initialized, unable to send message");
                    test_status = SMC_ERROR;
                }

                break;
            }
            case 0x02:
            {
                uint8_t smc_instance_id = test_input_data[1];
                smc_t*  smc_instance    = NULL;

                smc_instance = smc_test_get_instance_by_test_instance_id( smc_instance_id );

                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: Test case 0x%02X, dumping SMC data (test id 0x%02X)...", test_case, smc_instance_id);

                if( smc_instance )
                {
                    smc_instance_dump( smc_instance );
                    test_status = SMC_OK;
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance: Test case 0x%02X failed: No proper SMC instance found", test_case);
                    test_status = SMC_ERROR;
                }

                break;
            }
            case 0x03:
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: Test case 0x%02X, Sending message with userdata...", test_case);

                if( g_smc_instance1 )
                {
                        /* Use the default smc channel*/
                    uint8_t  required_data_len = 7;
                    uint8_t  smc_channel_id    = 0;
                    uint32_t data_length       = (test_input_data_len-data_index);

                    if( data_length >= required_data_len )
                    {
                        uint8_t* data = NULL;
                        smc_user_data_t userdata;

                        userdata.flags     = test_input_data[data_index++];
                        userdata.userdata1 = test_input_data[data_index++];
                        userdata.userdata2 = test_input_data[data_index++];
                        userdata.userdata3 = test_input_data[data_index++];
                        userdata.userdata4 = test_input_data[data_index++];
                        userdata.userdata5 = test_input_data[data_index++];

                            /* Put the rest of the data to the message part */
                        data = (test_input_data + data_index);
                        data_length    = (test_input_data_len-data_index);

                        test_status = smc_send_ext(SMC_CHANNEL_GET(g_smc_instance1, smc_channel_id), (void*)data, data_length, &userdata);
                    }
                    else
                    {
                        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance: Not enough message data for test case 0x%02X, required %d, received %d",
                                test_case, (test_input_data_len-data_index+required_data_len), test_input_data_len);
                        test_status = SMC_ERROR;
                    }
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance: SMC instance not initialized, unable to send message");
                    test_status = SMC_ERROR;
                }

                break;
            }
            case 0x04:
            {
                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, Destroying SMC instance...", test_case);

                if( g_smc_instance1 )
                {
                    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, Destroying SMC instance 1: 0x%08X", test_case, (uint32_t)g_smc_instance1);

                    smc_instance_destroy( g_smc_instance1 );

                    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, SMC instance 1 destroyed", test_case);

                    g_smc_instance1 = NULL;

                    test_status = SMC_OK;
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance: SMC instance not initialized, unable to send message");
                    test_status = SMC_ERROR;
                }

                break;
            }
            case 0x05:
            {
                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, Start SMC instance in remote side...", test_case);

#ifdef SMECO_MODEM
#if (MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L2)
                if( g_smc_instance1 )
                {
                    uint8_t test_case_data[6];
                    uint32_t shm_address = 0x00000000;

                    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, Startimg SMC instance 1 0x%08X in remote side", test_case, (uint32_t)g_smc_instance1);

                        /* Get the SHM and send to the remote */
                    shm_address = (uint32_t)g_smc_instance1->smc_shm_conf->shm_area_start_address;

                        /* L2 sends message to L1 to create SMC instance */
                    test_case_data[0] = 0x00;
                    test_case_data[1] = 0x00;

                    test_case_data[2] = (shm_address>>24)&0xFF;
                    test_case_data[3] = (shm_address>>16)&0xFF;
                    test_case_data[4] = (shm_address>>8)&0xFF;
                    test_case_data[5] = shm_address&0xFF;

                    if( smc_test_handler_send_message(0x02, test_case_data, 6) == SMC_OK )
                    {
                        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: smc_test_handler_send_message to L1 OK");
                    }
                    else
                    {
                        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance: smc_test_handler_send_message to L1 FAILED");
                    }

                    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, SMC instance 1 started in remote", test_case);

                    test_status = SMC_OK;
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance: SMC instance not initialized, unable to send message");
                    test_status = SMC_ERROR;
                }
#else
                SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance: This test is supported only in L2");
                test_status = SMC_ERROR;
#endif
#endif

                break;
            }
            case 0x06:
            {
                smc_t*  smc_instance = NULL;
                uint8_t channel_id  = 0;

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, Send crash message to the remote CPU...", test_case);

#if( (SMC_L2MUX_IF==TRUE) )
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: get L2MUX SMC instance...");
                smc_instance = get_smc_instance_l2mux();
#else
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: L2MUX SMC instance not available");
#endif
                if( smc_instance != NULL )
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: send crash info...");

                    test_status = smc_send_crash_indication( SMC_CHANNEL_GET( smc_instance, channel_id ), "assertion failed: smc_test.c, 920" );

                    if( test_status == SMC_OK )
                    {
                        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: send NULL crash info...");
                        test_status = smc_send_crash_indication( SMC_CHANNEL_GET( smc_instance, channel_id ), NULL );
                    }
                }
                else
                {
                    test_status = SMC_ERROR;
                }

                break;
            }
            case 0xFD:
            {
                uint8_t test_input[4];

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, Sending PN message to other CPU...", test_case);

                test_input[0] = 0xFE;   /* Send the FIFO dump message */
                test_input[1] = 0x00;
                test_input[2] = 0xBB;
                test_input[3] = 0xCC;

                test_status = smc_test_handler_send_message(0x02, test_input, 4);

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, PN message sent to other CPU", test_case);
                break;
            }
            case 0xFE:
            {
                /* TODO Move this test to more propriate place */
                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, Dump FIFOs...", test_case);

                if( g_smc_instance1 )
                {
                    smc_fifo_t* p_fifo = SMC_CHANNEL_GET(g_smc_instance1, 0)->fifo_out;

                    smc_fifo_dump_data(p_fifo);

                    p_fifo = SMC_CHANNEL_GET(g_smc_instance1, 0)->fifo_in;

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: FIFO IN");
                    smc_fifo_dump_data(p_fifo);
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_smc_instance: SMC instance not initialized, unable to send message");
                    test_status = SMC_ERROR;
                }
                break;
            }
            case 0xFF:
            {
                uint8_t is_master = TRUE;
                smc_t* smc_2 = NULL;
                smc_channel_conf_t* smc_channel_conf_1_master = NULL;
                smc_t* smc_1 = NULL;
                smc_conf_t* smc_conf_2 = NULL;

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_smc_instance: Test case 0x%02X, creating SMC instance...", test_case);

#ifdef SMECO_MODEM
#if (MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L1)
                is_master = FALSE;
#endif
#endif
                /* Other SMC instance for Single CPU test case  */
                /* NOTE: If the instance is created the master/slave or SHM values are not modified */
                smc_1 = get_smc_instance_1(is_master, 0);

                    /* ====================================================
                     * Create seconc SMC configuration
                     */
                smc_conf_2 = smc_conf_create();
                smc_conf_2->is_master = !is_master;

                    // Use the same channel configuration but master
                smc_channel_conf_1_master = smc_test_case_create_channel_conf_1(is_master);
                smc_conf_add_channel_conf(smc_conf_2, smc_channel_conf_1_master);


                /* Just one channel
                smc_channel_conf_t* smc_channel_conf_2_master = smc_test_case_create_channel_conf_2(TRUE);
                smc_conf_add_channel_conf(smc_conf_2, smc_channel_conf_2_master);
                */

                    /* ====================================================
                     * Create the Shared memory
                     */

                    // Use the same shared memory area with SMC 1
                smc_conf_2->smc_shm_conf = smc_shm_conf_create();

                smc_conf_2->smc_shm_conf->shm_area_start_address = smc_1->smc_shm_conf->shm_area_start_address;
                smc_conf_2->smc_shm_conf->size                   = smc_1->smc_shm_conf->size;

                    // Create the second SMC instance
                smc_2 = smc_instance_create(smc_conf_2);

                smc_2->cpu_id_local  = SMC_CPU_ID_REMOTE;
                smc_2->cpu_id_remote = SMC_CPU_ID_LOCAL;

                smc_instance_dump( smc_2 );
                break;
            }
            default:
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: unknown test case 0x%02X", test_case);
                test_status = SMC_ERROR;
            }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_smc_instance: completed with return value 0x%02X", test_status);

    return test_status;
}

static uint8_t smc_test_case_function_configure_shared_mem( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t test_status = SMC_ERROR;
    uint8_t input_len_required = 10;

    if( test_input_data_len >= input_len_required )
    {
        smc_shm_config_t shm_config;

        uint32_t address = SMC_BYTES_TO_32BIT( test_input_data );
        shm_config.shm_area_start_address = (uint8_t*)address;
        shm_config.size                   = SMC_BYTES_TO_32BIT((test_input_data+4));
        shm_config.use_cache_control      = test_input_data[8];
        shm_config.cache_line_len         = test_input_data[9];

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_configure_shared_mem: SHM start: 0x%08X, length: %d",
                (uint32_t)shm_config.shm_area_start_address,
                shm_config.size);

        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_configure_shared_mem: Not implemented");
        test_status = SMC_ERROR;
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_configure_shared_mem: not enough input data %d, required %d", test_input_data_len, input_len_required);
        test_status = SMC_ERROR;
    }

    return test_status;
}

void smc_test_handler_register( void )
{
    SMC_TEST_TRACE_PRINTF_INFO("Register SMC test handler (Group ID 0x%04X)...", smc_test_handler_group_id);

    (void)modem_test_handler_register( smc_test_handler_group_id, (uint16_t)SMC_TEST_HDLR,
                                       (modem_test_handler_ptr_t)smc_test_handler );

    SMC_TEST_TRACE_PRINTF_INFO("SMC test handler successfully registered");
}

os_task_token smc_os_task_create(const char* task_name, os_prior task_priority, os_entrypoint_t task_function)
{
#ifdef SMECO_LINUX_ANDROID
    SMC_TEST_TRACE_PRINTF_ERROR( "smc_create_os_task: NOT IMPLEMENTED IN ANDROID");
    return 0;
#elif defined SMECO_LINUX_KERNEL
    SMC_TEST_TRACE_PRINTF_ERROR( "smc_create_os_task: NOT IMPLEMENTED IN LINUX KERNEL");
    return 0;
#else
    uint32_t stack_size = 512;

    if( os_task_id_get( task_name ) == OS_NO_TASK )
    {
        os_task_token task_id ;
        void*         stack_ptr  = os_aligned_block_alloc_no_wait(stack_size, 8);

        assert( stack_ptr != NULL );

        SMC_TEST_TRACE_PRINTF_INFO( "smc_create_os_task: Creating task '%s'...", task_name);

        task_id = os_task_create( task_name,
                                  task_priority,
                                  task_function,
                                  stack_ptr,
                                  stack_size,
                                  15 );
        return task_id;
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO( "smc_create_os_task: Task '%s' already exists", task_name);
        return OS_NO_TASK;
    }
#endif
}

uint8_t smc_os_task_start(const char* task_name, os_prior task_priority, os_entrypoint_t task_function, uint8_t* test_input_data, uint16_t test_input_data_len, os_task_token* new_task_id)
{
#ifdef SMECO_LINUX_ANDROID
    SMC_TEST_TRACE_PRINTF_INFO( "smc_os_task_start: NOT IMPLEMENTED IN ANDROID");
    return SMC_ERROR;
#elif defined SMECO_LINUX_KERNEL
    SMC_TEST_TRACE_PRINTF_INFO( "smc_os_task_start: NOT IMPLEMENTED IN LINUX KERNEL");
    return SMC_ERROR;
#else
    int task_id = OS_NO_TASK;

    SMC_TEST_TRACE_PRINTF_INFO( "smc_os_task_start: creating dynamic task with name '%s'...", task_name);

    task_id = smc_os_task_create( task_name, task_priority, task_function );

    if( task_id != OS_NO_TASK )
    {
        SMC_TEST_TRACE_PRINTF_INFO( "smc_os_task_start: Task '%s' with id %d created, starting task...", task_name, task_id);

        *new_task_id = task_id;

        os_task_resume( task_id );

        SMC_TEST_TRACE_PRINTF_INFO( "smc_os_task_start: Task '%s' with id %d successfully started", task_name, task_id);

        if( test_input_data != NULL && test_input_data_len > 0 )
        {
            SMC_TEST_TRACE_PRINTF_INFO( "smc_os_task_start: sending test input data to receiver task...");

                // Send configuration message to receiver task
            os_msg_send_no_wait( task_id, test_input_data );
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_INFO( "smc_os_task_start: test start message not send, msg NULL or empty");
        }

        SMC_TEST_TRACE_PRINTF_INFO( "smc_os_task_start: completed");

        return SMC_OK;
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO( "smc_os_task_start: failed: task with name '%s' already exists", task_name);
        return SMC_ERROR;
    }
#endif
}


/* ======================================================
 * SMC FIFO Callback functions
 */
static void smc_receive_data_callback_channel_conf_1( void*   data,
                                                      int32_t data_length,
                                                      const struct _smc_user_data_t* userdata,
                                                      const struct _smc_channel_t* channel)
{
    SMC_TEST_TRACE_PRINTF_INFO("==============================================================================================");
    SMC_TEST_TRACE_PRINTF_INFO( "smc_receive_data_callback_channel_0: Handling a message 0x%08X (len %d, flags 0x%08X) from SMC 0x%08X channel %d",
            (uint32_t)data, data_length, userdata->flags, (uint32_t)channel->smc_instance, channel->id);

    SMC_TRACE_PRINTF_INFO_DATA(data_length, data);

    SMC_TEST_TRACE_PRINTF_INFO( "smc_receive_data_callback_channel_0: Userdata of the message 0x%08X: 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X",
            (uint32_t)data, userdata->userdata1, userdata->userdata2, userdata->userdata3, userdata->userdata4, userdata->userdata5);

    SMC_TEST_TRACE_PRINTF_INFO("==============================================================================================");
}



#ifdef SMECO_LINUX_ANDROID

    /* ====================================================
     * Android Specific functions
     */
uint8_t smc_test_handler_send_message(uint16_t test_case_id, uint8_t* test_input_data, uint16_t test_input_data_len)
{
    uint8_t return_value = SMC_ERROR;

    SMC_TRACE_PRINTF_ERROR( "smc_test_handler_send_message: NOT IMPLEMENTED IN ANDROID");
    return return_value;
}

#elif defined SMECO_LINUX_KERNEL
    /* ====================================================
     * Linux Kernel Specific functions
     */
uint8_t smc_test_handler_send_message(uint16_t test_case_id, uint8_t* test_input_data, uint16_t test_input_data_len)
{
    uint8_t return_value = SMC_ERROR;

    SMC_TRACE_PRINTF_ERROR( "smc_test_handler_send_message: NOT IMPLEMENTED IN LINUX KERNEL");
    return return_value;
}

#else




/*
 * Sends message to other CPUs SMC test server
 */
uint8_t smc_test_handler_send_message(uint16_t test_case_id, uint8_t* test_input_data, uint16_t test_input_data_len)
{
    uint8_t  return_value = SMC_ERROR;
    uint16_t message_length = 0;
    //uint8_t  media = 0x00;
    uint8_t  receiver_device_id  = PN_DEV_MODEM_HOST_2;  /* Mexe Test Server is in L2*/
    //uint8_t  sender_device_id  = 0x00;


    //uint8_t  receiver_object = 0x00;
    //uint8_t  sender_object = 0x00;
    //uint8_t  tid = 0x00;

#if(MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L2)
    /* ====================================================
     * L2 sends to L1 using phonet
     */

    /*

Detailed PMD:
   0  (8) Media.......................: 0x00               => PN_MEDIA_ROUTING_REQ
   1  (6) Receiver Device.............: 0x60               => PN_DEV_MODEM_HOST_2
   2  (6) Sender Device...............: 0x10               => PN_DEV_PC
   3  (8) Resource....................: 0x91               => PN_MODEM_TEST
   4 (16) Length......................: 0x0012             => 18
   6 (10) Receiver Object.............: 0x000              => 0
   7 (10) Sender Object...............: 0x001              => 1
   8  (8) Transaction ID..............: 0x01               => 1
   9  (8) Message ID..................: 0x80               => MODEM_TEST_RUN_REQ
  10 (16) Test group ID...............: 0x8001             => MODEM_TEST_GRP_MEXE_L23
  12 (16) MeXe test handler ID........: 0x001F             => 0x001F
Warning: No constant is defined for this value
  14 (16) Test case ID................: 0x0002             => 0x0002
  16  (8) Attribute...................: 0x00               => 00000000  MODEM_TEST_ATTR_NONE
  17  (8) Filler......................: 0x00               => 8 bit padding
  18 (16) Input data length or zero...: 0x0002             => 2
  20  (8) Input data[1]...............: 0x01               => 0x01
  21  (8) Input data[2]...............: 0x02               => 0x02
  22 (16) Filler......................: 0x0000             => Alignment to 32 bit boundary

    */

    message_length = 12 + 2 + test_input_data_len;

    pn_msg* pn_message = (pn_msg*)SMC_MALLOC(message_length + 6);

    pn_message->media    = 0x00;
    pn_message->receiver = receiver_device_id;
    pn_message->sender   = PN_DEV_MODEM_HOST_2;
    pn_message->function = PN_MODEM_TEST;
    pn_message->length   = message_length;
    pn_message->data[0]  = 0x00;
    pn_message->data[1]  = 0x01;
    pn_message->data[2]  = 0x00;
    pn_message->data[3]  = 0x80;

    /* This is MODEM_TEST_GRP_MEXE_L23
    pn_message->data[4]  = 0x80;
    pn_message->data[5]  = 0x01;
    */

    /* This is MODEM_TEST_GRP_MEXE_L1 */
    pn_message->data[4]  = 0x00;
    pn_message->data[5]  = 0x01;

    pn_message->data[6]  = 0x00;    /* SMC test */
    pn_message->data[7]  = 0x1F;    /* SMC test */

    pn_message->data[8]  = ((test_case_id>>8)&0xFF);
    pn_message->data[9]  = test_case_id&0xFF;
    pn_message->data[10] = 0x00;
    pn_message->data[11] = 0x00;
    pn_message->data[12] = ((test_input_data_len>>8)&0xFF);
    pn_message->data[13] = test_input_data_len&0xFF;

    if( test_input_data_len > 0 )
    {
        for(int i = 0; i < test_input_data_len; i++)
        {
            pn_message->data[i+14] = test_input_data[i];
        }
    }

    SMC_TRACE_PRINTF_INFO( "smc_test_handler_send_message: Sending Phonet message 0x%08X...", (uint32_t)pn_message);
    SMC_TRACE_PRINTF_INFO_DATA(message_length+6, pn_message);

    if( pn_msg_send(pn_message) == PN_OK )
    {
        SMC_TRACE_PRINTF_INFO( "smc_test_handler_send_message: Phonet message 0x%08X successfully sent", pn_message);
        return_value = SMC_OK;
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR( "smc_test_handler_send_message: Phonet message 0x%08X send failed", pn_message);
        SMC_FREE( pn_message );
        return_value = SMC_ERROR;
    }

#elif(MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L1)

    /* ====================================================
     * L1 sends to L2 using MMSGR
     */

    uint8_t* message = NULL;
    uint8_t  resource_id = 0x00;        /* Mexe Test Server */

    //sender_device_id  = PN_DEV_MODEM_HOST_3;

    mmsgr_msg_send(resource_id, (MMSGR_ISI_MSG_STR*)message);
#endif

    return return_value;
}

#endif

/*
 * This function is deprecated
 */
static uint8_t smc_test_case_function_create_configuration( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t test_status      = SMC_OK;
    char*   smc_config_name  = NULL;   /* TODO create function to get proper name */
    char*   config_user_name = NULL;

    smc_instance_conf_t* smc_instance_configuration = NULL;

    if( smc_config_name == NULL )
    {
        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_create_configuration: SMC configuration name invalid (NULL)");
        return SMC_ERROR;
    }

#if(SMC_L2MUX_IF==TRUE)

    config_user_name = SMC_CONFIG_USER_L2MUX;   /* Get a configuration for the L2MUX */

    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_create_configuration: starting, Test input data (len %d):", test_input_data_len);
    SMC_TEST_TRACE_PRINTF_DEBUG_DATA(test_input_data_len, test_input_data);

        /* Get the configuration by the name */
    smc_instance_configuration = smc_instance_conf_get_l2mux( config_user_name, smc_config_name );

    if( smc_instance_configuration != NULL )
    {
        smc_conf_t* smc_conf     = NULL;
        smc_t*      smc_instance = NULL;

        SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_create_configuration: Configuration for config user %s by SMC name %s found",
                config_user_name, smc_config_name);

        /* Create SMC instance based on the configuration */

        SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_create_configuration: channel count is %d (array size %d)",
                smc_instance_configuration->channel_config_count, sizeof( smc_instance_configuration->channel_config_array ));

        smc_conf = smc_conf_create_from_instance_conf( smc_config_name, smc_instance_configuration );

        SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_create_configuration: SMC configuration 0x%08X created, creating instance...", (uint32_t)smc_conf );

        smc_instance = smc_instance_create(smc_conf);

        smc_instance = smc_instance;

        SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_create_configuration: SMC instance created");

        test_status = SMC_OK;
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_create_configuration: Configuration for config user %s by name %s not found", config_user_name, smc_config_name);
        test_status = SMC_ERROR;
    }
#else
    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_create_configuration: L2MUX is not built in");
    test_status = SMC_ERROR;
#endif

    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_create_configuration: completed with return value 0x%02X", test_status);
    return test_status;
}

static uint8_t smc_test_case_function_shm( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;

    uint16_t test_input_len_required = 5;

    if( test_input_data_len >= test_input_len_required )
    {
        uint8_t  remap_address = 0;      /* Valid only in Linux */
        uint32_t data_index   = 0;
        uint8_t  test_case    = test_input_data[data_index++];
        uint32_t shm_data_len = 0;
        uint32_t shm_address  = SMC_BYTES_TO_32BIT( (test_input_data+data_index) );

        data_index += 4;

        if( (test_case&0x10)==0x10 )
        {
            remap_address = 1;
            test_case     = test_case - 0x10;
        }
        else
        {
            remap_address = 0;
        }

        switch(test_case)
        {
            case 0x00:
            {
                int i = 0;
                shm_data_len = test_input_data_len - data_index;

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_shm: write %d bytes of data to SHM address 0x%08X...", shm_data_len, shm_address);
                SMC_TEST_TRACE_PRINTF_DEBUG_DATA(shm_data_len, (test_input_data+data_index) );

                if( remap_address == 1 )
                {
#if defined SMECO_LINUX_KERNEL

                    shm_address = (uint32_t)ioremap((long unsigned int)shm_address, shm_data_len);
                    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_shm: ioremapped address 0x%08X", shm_address);
#endif
                }

                /* Linux __raw_writel(2, ((void __iomem *)((uint32_t)remapped_mdm_io))); */

                for(i = 0; i < shm_data_len; i++ )
                {
                    *((uint8_t*)shm_address + i ) = test_input_data[data_index+i];
                }

                if( remap_address == 1 )
                {
#if defined SMECO_LINUX_KERNEL
                    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_shm: unmap address 0x%08X", shm_address);
                    iounmap((volatile void*)shm_address);
#endif
                }

                test_status = SMC_OK;

                break;
             }
             case 0x01:
             {
                 test_input_len_required = 8;

                 if( test_input_data_len >= test_input_len_required )
                 {
                     //uint8_t* data_read_from_shm = NULL;

                     shm_data_len = SMC_BYTES_TO_32BIT( (test_input_data + data_index) );
                     data_index += 4;

                     SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_shm: read %d bytes of data from SHM address 0x%08X...", shm_data_len, shm_address);

                     if( remap_address == 1 )
                     {
#if defined SMECO_LINUX_KERNEL
                         shm_address = (uint32_t)ioremap((long unsigned int)shm_address, shm_data_len);
                         SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_shm: ioremapped address 0x%08X", shm_address);
#endif
                     }

                     SMC_TEST_TRACE_PRINTF_DEBUG_DATA(shm_data_len, (uint8_t*)shm_address );

                     if( remap_address == 1 )
                     {
#if defined SMECO_LINUX_KERNEL
                         SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_shm: unmap address 0x%08X", shm_address);
                         iounmap((void*)shm_address);
#endif
                     }

                 }
                 else
                 {
                     SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_shm: not enough test input data for read (received %d, expected %d)",
                                                         test_input_data_len, test_input_len_required);
                 }

                 test_status = SMC_OK;

                 break;
             }
             default:
             {
                 SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_shm: Invalid test case 0x%02X", test_case);
                 test_status = SMC_ERROR;
             }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_shm: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_shm: completed with return value 0x%02X", test_status);

    return test_status;
}

static void smc_test_timer_expired(uint32_t timer_data)
{
    smc_timer_t* timer = NULL;
    uint32_t     timer_counter = 0;

    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_timer_expired: Test timer 0x%08X expired...", timer_data);

    timer = (smc_timer_t*)timer_data;

    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_timer_expired: previous jiffies: %ld", timer->prev_jiffies);

    if( timer->timer_data != 0 )
    {
        timer_counter = *((uint32_t*)timer->timer_data);
        timer_counter++;
    }

    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_timer_expired: timer object ptr 0x%08X, value %d",
            timer->timer_data, timer_counter);

    if( timer_counter > 0 && timer_counter <= 5)
    {
        *((uint32_t*)timer->timer_data) = timer_counter;

        SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_timer_expired: Start test timer 0x%08X again", timer_data);

        if( smc_timer_start( timer, (void*)smc_test_timer_expired, timer->timer_data ) != SMC_OK )
        {
            SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_timer_expired: Test timer 0x%08X restart failed", timer_data);
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_timer_expired: Destroy test timer 0x%08X...", timer_data);
        smc_timer_destroy( timer );
    }

    /* TODO Shut down the timer */

    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_timer_expired: Test timer 0x%08X handling completed", timer_data);
}

static uint8_t smc_test_case_function_timer( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;

    uint16_t test_input_len_required = 5;

    if( test_input_data_len >= test_input_len_required )
    {
        uint32_t data_index   = 0;
        uint8_t  test_case    = test_input_data[data_index++];

        switch(test_case)
        {
            case 0x00:
            {
                smc_timer_t* timer         = NULL;
                uint32_t     period_us     = SMC_BYTES_TO_32BIT( (test_input_data + data_index) );
                uint32_t*    timer_counter = (uint32_t*)SMC_MALLOC(sizeof(uint32_t));
                data_index+=4;

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_timer: Test case 0x%02X: start %u usec timer ...", test_case, period_us);

                timer = smc_timer_create( period_us );
                *timer_counter = 0;

                test_status = smc_timer_start( timer, (void*)smc_test_timer_expired, (uint32_t)timer_counter );

                break;
            }
            default:
            {
                SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_timer: Invalid test case 0x%02X", test_case);
                test_status = SMC_ERROR;
                break;
            }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_timer: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_timer: completed with return value 0x%02X", test_status);

    return test_status;
}


static uint8_t smc_test_case_function_misc( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;

    uint16_t test_input_len_required = 1;

    if( test_input_data_len >= test_input_len_required )
    {
        uint32_t data_index   = 0;
        uint8_t  test_case    = test_input_data[data_index++];

        switch(test_case)
        {
            case 0x00:
            {
                uint32_t version_int = 0;
                char version_str[15];
                char* version_from_int = NULL;

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_misc: Test case 0x%02X: get version'%s' in integer...", test_case, SMC_SW_VERSION);

                version_int = smc_version_to_int(SMC_SW_VERSION);

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_misc: Test case 0x%02X: version'%s' == 0x%08X", test_case, SMC_SW_VERSION, version_int);

                version_from_int = smc_version_to_str(version_int);

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_misc: Test case 0x%02X: version from 0x%08X is '%s'", test_case, version_int, version_from_int);

                    /* Remember to free */
                SMC_FREE( version_from_int );

                strcpy(version_str, "12.34.5678");

                version_int = smc_version_to_int(version_str);

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_misc: Test case 0x%02X: version'%s' == 0x%08X", test_case, version_str, version_int);

                version_from_int = smc_version_to_str(version_int);

                SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_misc: Test case 0x%02X: version from 0x%08X is '%s'", test_case, version_int, version_from_int);

                    /* Remember to free */
                SMC_FREE( version_from_int );

                test_status = SMC_OK;
                break;
            }
            default:
            {
                SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_misc: Invalid test case 0x%02X", test_case);
                test_status = SMC_ERROR;
                break;
            }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_misc: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }


    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_misc: completed with return value 0x%02X", test_status);

    return test_status;
}


void smc_shared_variable_address_cb( char* shared_variable_name, void* shared_variable_address )
{
    SMC_TEST_TRACE_PRINTF_INFO("smc_shared_variable_address_cb: variable's %s address == 0x%08X", shared_variable_name, (uint32_t)shared_variable_address);


}

static uint8_t smc_test_case_shm_variable( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;

    uint16_t test_input_len_required = 1;

    if( test_input_data_len >= test_input_len_required )
    {
        uint32_t data_index   = 0;
        uint8_t  test_case    = test_input_data[data_index++];

        switch(test_case)
        {
            case 0x00:
            {
                void* shm_var_address = NULL;
                char* shm_var_name    = "SMCTESTSHMVAR1";

                shm_var_address = smc_shared_variable_address_get(shm_var_name);


                if( shm_var_address != NULL )
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_shm_variable: SHM variable address 0x%08X received", (uint32_t)shm_var_address);
                    test_status = SMC_OK;
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_shm_variable: shared variable address is NULL");
                    test_status = SMC_ERROR;
                }

                break;
            }
            case 0x01:
            {
                //void* shm_var_address = NULL;
                char* shm_var_name         = "SMCTESTSHMVAR1";
                smc_t*  smc_instance       = NULL;
                uint8_t smc_instance_id    = 0;                //test_input_data[1];
                uint8_t smc_channel_id     = 0;
                smc_channel_t* smc_channel = NULL;

                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_shm_variable: send shared variable request using SMC instance %d and channel %d", smc_instance_id, smc_channel_id);

                smc_instance = smc_test_get_instance_by_test_instance_id( smc_instance_id );

                smc_channel = SMC_CHANNEL_GET( smc_instance, smc_channel_id);

                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_shm_variable: send shared variable request...");

                test_status = smc_shared_variable_address_request_send(smc_channel, shm_var_name, smc_shared_variable_address_cb);

                break;
            }
            default:
            {
                SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_shm_variable: Invalid test case 0x%02X", test_case);
                test_status = SMC_ERROR;
                break;
            }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_shm_variable: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }


    return test_status;
}


/** ===========================================
 *  Loopback test functions
 *
 *
 */

uint8_t smc_send_loopback_data_message( smc_channel_t* smc_channel, uint32_t loopback_data_len, uint32_t loopback_rounds, uint8_t from_irq )
{
    smc_user_data_t userdata;
    uint8_t         ret_val = SMC_OK;
    uint32_t        message_len = 0;
    smc_loopback_data_t* loopback_data = NULL;

    SMC_TRACE_PRINTF_LOOPBACK("smc_send_loopback_data_message: channel %d:", smc_channel->id);

    loopback_data = smc_loopback_data_create( loopback_data_len, from_irq );

    loopback_data->round_trip_counter   = 0;
    loopback_data->loopback_rounds_left = loopback_rounds;

    for(int i = 0; i < loopback_data->loopback_data_length; i++ )
    {
        loopback_data->loopback_data[i] = (uint8_t)(i&0xFF);
    }

    /* TODO Put the checksum in four last bytes */

    userdata.flags     = SMC_MSG_FLAG_LOOPBACK_DATA_REQ;
    userdata.userdata1 = 0x00000000;
    userdata.userdata2 = 0x00000000;
    userdata.userdata3 = 0x00000000;
    userdata.userdata4 = 0x00000000;
    userdata.userdata5 = 0x00000000;

    message_len = sizeof( smc_loopback_data_t ) + loopback_data->loopback_data_length - 1;

    RD_TRACE_SEND5(TRA_SMC_LOOPBACK_START, 1, &smc_channel->id,
                                           4, &loopback_data->loopback_data,
                                           4, &loopback_data->loopback_data_length,
                                           4, &loopback_data->round_trip_counter,
                                           4, &loopback_data->loopback_rounds_left);


    SMC_TRACE_PRINTF_LOOPBACK("smc_send_loopback_data_message: channel %d: send loopback data 0x%08X (len %d)", smc_channel->id, (uint32_t)loopback_data, message_len );
    SMC_TRACE_PRINTF_LOOPBACK("smc_send_loopback_data_message: channel %d: userdata: 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X", smc_channel->id,
            userdata.flags, userdata.userdata1, userdata.userdata2, userdata.userdata3, userdata.userdata4, userdata.userdata5);
    SMC_TRACE_PRINTF_LOOPBACK("smc_send_loopback_data_message: channel %d: loopback data: round trips = %d, lb rounds left %d, payload = 0x%08X, len %d", smc_channel->id,
            loopback_data->round_trip_counter, loopback_data->loopback_rounds_left, (uint32_t)loopback_data->loopback_data, loopback_data->loopback_data_length );

    SMC_TRACE_PRINTF_LOOPBACK_DATA(loopback_data->loopback_data_length, loopback_data->loopback_data);

    ret_val = smc_send_ext(smc_channel, (uint8_t*)loopback_data, message_len, &userdata);

    if( ret_val != SMC_OK )
    {
        SMC_TRACE_PRINTF_ERROR("smc_send_loopback_data_message: send failed");
    }

    SMC_TRACE_PRINTF_LOOPBACK("smc_send_loopback_data_message: channel %d completed", smc_channel->id);

    return ret_val;
}

uint8_t smc_handle_loopback_data_message( smc_channel_t* smc_channel, smc_loopback_data_t* loopback_data, smc_user_data_t* userdata, uint8_t from_irq)
{
    uint8_t ret_val    = SMC_OK;
    uint8_t is_request = SMC_FIFO_IS_INTERNAL_MESSAGE_LOOPBACK_DATA_REQ( userdata->flags );
    uint32_t  lb_data_len = 0;

    SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message (%s): channel %d: loopback data 0x%08X", is_request?"Request":"Response", smc_channel->id, (uint32_t)loopback_data);

    if( loopback_data != NULL )
    {
        lb_data_len = sizeof( smc_loopback_data_t ) + loopback_data->loopback_data_length - 1;

        SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message (%s): channel %d: send loopback data 0x%08X (len %d)", is_request?"Request":"Response", smc_channel->id, (uint32_t)loopback_data, lb_data_len);
        SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message (%s): channel %d: userdata: 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X, 0x%08X", is_request?"Request":"Response", smc_channel->id,
                userdata->flags, userdata->userdata1, userdata->userdata2, userdata->userdata3, userdata->userdata4, userdata->userdata5);
        SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message (%s): channel %d: loopback data: round_trip_counter = %d, lb rounds left %d, payload = 0x%08X, len %d", is_request?"Request":"Response", smc_channel->id,
                loopback_data->round_trip_counter, loopback_data->loopback_rounds_left, (uint32_t)loopback_data->loopback_data, loopback_data->loopback_data_length );

        SMC_TRACE_PRINTF_LOOPBACK_DATA(loopback_data->loopback_data_length, loopback_data->loopback_data);
    }
    else
    {
        SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message (%s): channel %d: loopback is null", is_request?"Request":"Response", smc_channel->id);
        ret_val = SMC_ERROR;
    }

    if( is_request || ( loopback_data!=NULL && loopback_data->loopback_rounds_left > 0) )
    {
        /* Send the same data back, put loopback roundtrip information to the userdata */
        smc_user_data_t userdata_resp;
        smc_loopback_data_t* loopback_data_send = NULL;

        userdata_resp.userdata1 = userdata->userdata1;
        userdata_resp.userdata2 = userdata->userdata2;
        userdata_resp.userdata3 = userdata->userdata3;
        userdata_resp.userdata4 = ret_val;
        userdata_resp.userdata5 = userdata->userdata5;

        if( is_request )
        {
            userdata_resp.flags     = SMC_MSG_FLAG_LOOPBACK_DATA_RESP;
        }
        else
        {
                /* Update the round trip information when response */
            loopback_data->round_trip_counter++;
            loopback_data->loopback_rounds_left--;

                /* Send as a request */
            userdata_resp.flags     = SMC_MSG_FLAG_LOOPBACK_DATA_REQ;
        }

        if( loopback_data != NULL )
        {
            if( SMC_MDB_ADDRESS_IN_POOL_IN( loopback_data, smc_channel->smc_mdb_info ) )
            {
                smc_user_data_t userdata_free;

                SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message (%s): channel %d: loopback ptr 0x%08X, in MDB IN pool, create a copy", is_request?"Request":"Response", smc_channel->id, (uint32_t)loopback_data);

                loopback_data_send = smc_loopback_data_create( lb_data_len, from_irq );

                loopback_data_send->round_trip_counter   = loopback_data->round_trip_counter;
                loopback_data_send->loopback_data_length = loopback_data->loopback_data_length;
                loopback_data_send->timestamp            = loopback_data->timestamp;
                loopback_data_send->loopback_rounds_left = loopback_data->loopback_rounds_left;

                for(int i = 0; i < loopback_data->loopback_data_length; i++ )
                {
                    loopback_data_send->loopback_data[i] = loopback_data->loopback_data[i];
                }

                /* The original pointer in sender's MDB must be freed */

                userdata_free.flags     = SMC_MSG_FLAG_FREE_MEM_MDB;
                userdata_free.userdata1 = userdata->userdata1;
                userdata_free.userdata2 = userdata->userdata2;
                userdata_free.userdata3 = userdata->userdata3;
                userdata_free.userdata4 = userdata->userdata4;
                userdata_free.userdata5 = userdata->userdata5;

                SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message: channel %d, free the original data 0x%08X from SHM...", smc_channel->id, (uint32_t)loopback_data);

                    /* Free the MDB SHM data PTR from remote */
                if( smc_send_ext( smc_channel, (void*)loopback_data, 0, &userdata_free) != SMC_OK )
                {
                    SMC_TRACE_PRINTF_ERROR("smc_handle_loopback_data_message: channel %d: MDB memory 0x%08X free from remote failed",
                            smc_channel->id, (uint32_t)loopback_data);
                }
            }
            else
            {
                loopback_data_send = loopback_data;
            }
        }

        SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message (%s): channel %d: send loopback response data 0x%08X, data len %d", is_request?"Request":"Response", smc_channel->id, (uint32_t)loopback_data_send, lb_data_len);

        if( userdata_resp.flags == SMC_MSG_FLAG_LOOPBACK_DATA_RESP )
        {
            RD_TRACE_SEND5(TRA_SMC_LOOPBACK_SEND_RESP, 1, &smc_channel->id,
                                                       4, &loopback_data_send->loopback_data,
                                                       4, &loopback_data_send->loopback_data_length,
                                                       4, &loopback_data_send->round_trip_counter,
                                                       4, &loopback_data_send->loopback_rounds_left);
        }
        else
        {
            RD_TRACE_SEND5(TRA_SMC_LOOPBACK_SEND_REQ, 1, &smc_channel->id,
                                                      4, &loopback_data_send->loopback_data,
                                                      4, &loopback_data_send->loopback_data_length,
                                                      4, &loopback_data_send->round_trip_counter,
                                                      4, &loopback_data_send->loopback_rounds_left);
        }

        if( smc_send_ext( smc_channel, (uint8_t*)loopback_data_send, lb_data_len, &userdata_resp) != SMC_OK )
        {
            SMC_TRACE_PRINTF_WARNING("smc_handle_loopback_data_message: SMC_MSG_FLAG_LOOPBACK_DATA_RESP send failed");
        }
    }
    else
    {
        if( loopback_data != NULL )
        {
            SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message (%s): channel %d: loopback completed, result %s, loopback rounds %d", is_request?"Request":"Response", smc_channel->id, userdata->userdata4?"OK":"FAILED", loopback_data->round_trip_counter);
        }
        else
        {
            SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message (%s): channel %d: loopback completed, result %s, loopback data was NULL (ERROR)", is_request?"Request":"Response", smc_channel->id, userdata->userdata4?"OK":"FAILED");
        }
    }

    SMC_TRACE_PRINTF_LOOPBACK("smc_handle_loopback_data_message (%s): channel %d: completed with return value 0x%02X", is_request?"Request":"Response", smc_channel->id, ret_val);

    return ret_val;
}

smc_loopback_data_t* smc_loopback_data_create( uint32_t size_of_message_payload, uint8_t from_irq )
{
    smc_loopback_data_t* data = NULL;

    if( from_irq )
    {
        data = (smc_loopback_data_t*)SMC_MALLOC_IRQ( sizeof( smc_loopback_data_t ) + ((size_of_message_payload-1)*sizeof(uint8_t)) );
    }
    else
    {
        data = (smc_loopback_data_t*)SMC_MALLOC( sizeof( smc_loopback_data_t ) + ((size_of_message_payload-1)*sizeof(uint8_t)) );
    }

    data->round_trip_counter   = 0;
    data->loopback_data_length = size_of_message_payload;
    data->timestamp            = 0;
    data->loopback_rounds_left = 0;

    memset( data->loopback_data, 0, size_of_message_payload );

    return data;
}

/*
 * End of loopback test functions
 *
 **/

static uint8_t smc_test_case_rpcl( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;

#ifdef SMECO_MODEM

    uint16_t test_input_len_required = 7;

    if( test_input_data_len >= test_input_len_required )
    {
        #define FILE_WRITE 0x00
        #define FILE_READ  0x01

        uint8_t data_index = 0;
        uint8_t test_case = test_input_data[data_index++];
        FILE * fp = NULL;
        char* fName = "CONF/SMC.DAT";

        uint32_t error_count = 0;
        uint32_t fSize       = 0;
        uint16_t rCount      = 0;

        rCount = SMC_BYTES_TO_16BIT( (test_input_data + data_index) );
        data_index += 2;

        fSize = SMC_BYTES_TO_32BIT( (test_input_data + data_index) );
        data_index += 4;

        if( test_case == FILE_WRITE )
        {
            if(test_input_data_len < test_input_len_required)
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: not enough test parameters, required %d", test_input_len_required);
                return SMC_ERROR;
            }
        }

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: test RPCL %d times...", rCount);

        for(int i = 0; i < rCount; i++)
        {
            if( test_case == FILE_WRITE )
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: opening file '%s' for writing... (%d)", fName, i+1);

                fp = fopen(fName, "w");

                if( fp )
                {
                    uint8_t* fData = NULL;
                    int x = 0;

                    if( fSize < 4 ) fSize = 4;

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: prepare file size %d bytes (%d)", fSize, i+1);

                    fData = (uint8_t*)SMC_MALLOC(fSize);

                    assert(fData != NULL);

                    memset(fData, 0x00, fSize);

                    x = 0;

                    while( x < fSize-4 )
                    {
                        fData[x++] = 0x53;
                        fData[x++] = 0x4D;
                        fData[x++] = 0x43;
                    }

                    fData[fSize-1] = 0x00;

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: file open ok (%d), write %d bytes file data", i+1, fSize);

                    if( fwrite(fData, fSize, 1, fp) == 1 )
                    {
                        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: file write ok (%d)", i+1);
                    }
                    else
                    {
                        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: file write failed (%d)", i+1);
                        error_count++;
                    }

                    SMC_FREE( fData );

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: closing file... (%d)", i+1);

                    fclose( fp );
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: file open for write failed (%d)", i+1);

                    error_count++;
                }
            }
            else
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: opening file '%s' for reading... (%d)", fName, i+1);

                fp = fopen(fName, "r");

                if( fp )
                {
                    uint8_t* fData = NULL;

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: prepare file size %d bytes (%d)", fSize, i+1);

                    fData = (uint8_t*)SMC_MALLOC(fSize);

                    assert(fData != NULL);

                    memset(fData, 0x00, fSize);

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: file open ok (%d), read %d bytes file data", i+1, fSize);

                    if( fread(fData, fSize, 1, fp) == 1 )
                    {
                        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: file read ok (%d)", i+1);
                    }
                    else
                    {
                        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: file read failed (%d)", i+1);
                        error_count++;
                    }

                    SMC_FREE( fData );

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: closing file... (%d)", i+1);

                    fclose( fp );
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: file open failed (%d)", i+1);
                    error_count++;
                }
            }
        }

        if( error_count > 0 )
        {
            SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: %d errors detected", error_count);
            test_status = SMC_ERROR;
        }
        else
        {
            test_status = SMC_OK;
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: completed with return value 0x%02X", test_status);

#else
    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_rpcl: NOT IMPLEMENTED");
#endif

    return test_status;
}

static uint8_t smc_test_case_channel_state( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;

    uint16_t test_input_len_required = 3;

    if( test_input_data_len >= test_input_len_required )
    {
        uint32_t       data_index      = 0;
        uint8_t        smc_instance_id = test_input_data[data_index++];
        uint8_t        smc_channel_id  = test_input_data[data_index++];
        smc_channel_t* smc_channel     = NULL;
        smc_t*         smc             = smc_test_get_instance_by_test_instance_id( smc_instance_id );
        uint8_t        test_case       = test_input_data[data_index++];


        if( smc == NULL )
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_channel_state: Unable to get SMC instance %d", smc_instance_id);
            return SMC_ERROR;
        }

        smc_channel = smc_channel_get(smc, smc_channel_id);

        if( smc_channel == NULL )
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_channel_state: Unable to get SMC channel %d from instance %d",
                                            smc_channel_id, smc_instance_id);
            return SMC_ERROR;
        }

        switch(test_case)
        {
            case 0x00:
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: Stop receiving data in channel %d", smc_instance_id, smc_channel->id);

                if( smc_channel_enable_receive_mode( smc_channel, FALSE ) == SMC_OK )
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: channel %d receive disable OK", smc_instance_id, smc_channel->id);
                    test_status = SMC_OK;
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_channel_state: SMC instance %d: channel %d receive disable FAILED", smc_instance_id, smc_channel->id);
                    test_status = SMC_ERROR;
                }

                break;
            }
            case 0x01:
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: Resume receiving data in channel %d", smc_instance_id, smc_channel->id);

                if( smc_channel_enable_receive_mode( smc_channel, TRUE ) == SMC_OK )
                {
                        /* Caller of the smc_channel_enable_receive_mode() must take care of reading FIFO */
                    smc_channel_interrupt_handler(smc_channel);

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: channel %d receive enable OK", smc_instance_id, smc_channel->id);
                    test_status = SMC_OK;
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_channel_state: SMC instance %d: channel %d receive enable FAILED", smc_instance_id, smc_channel->id);
                    test_status = SMC_ERROR;
                }

                break;
            }
            case 0x02:
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: Stop sending data in channel %d", smc_instance_id, smc_channel->id);

                SMC_LOCK_TX_BUFFER( smc_channel->lock_tx_queue );

                if( !SMC_CHANNEL_STATE_SEND_IS_DISABLED( smc_channel->state ) && smc_channel->smc_event_cb)
                {
                    smc_channel->smc_event_cb( smc_channel, SMC_STOP_SEND_LOCAL, NULL );

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: data sending stopped in channel %d", smc_instance_id, smc_channel->id);
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: data sending already stopped in channel %d", smc_instance_id, smc_channel->id);
                }

                SMC_UNLOCK_TX_BUFFER( smc_channel->lock_tx_queue );
                test_status = SMC_OK;

                break;
            }
            case 0x03:
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: Resume sending data in channel %d", smc_instance_id, smc_channel->id);

                SMC_LOCK_TX_BUFFER( smc_channel->lock_tx_queue );

                if( SMC_CHANNEL_STATE_SEND_IS_DISABLED( smc_channel->state ) && smc_channel->smc_event_cb)
                {
                    smc_channel->smc_event_cb( smc_channel, SMC_RESUME_SEND_LOCAL, NULL );

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: data sending resumed in channel %d", smc_instance_id, smc_channel->id);
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: data sending already enabled in channel %d", smc_instance_id, smc_channel->id);
                }

                SMC_UNLOCK_TX_BUFFER( smc_channel->lock_tx_queue );


                break;
            }
            default:
            {
                SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_channel_state: Invalid test case 0x%02X", test_case);
                test_status = SMC_ERROR;
                break;
            }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    return test_status;
}

static uint8_t smc_test_case_send_data_raw( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;

    uint16_t test_input_len_required = 7;

    if( test_input_data_len >= test_input_len_required )
    {
        uint32_t       data_index      = 0;
        uint8_t        smc_instance_id = test_input_data[data_index++];
        uint8_t        smc_channel_id  = test_input_data[data_index++];
        smc_channel_t* smc_channel     = NULL;
        smc_t*         smc             = smc_test_get_instance_by_test_instance_id( smc_instance_id );
        uint8_t        raw_data_send_count = test_input_data[data_index++];
        uint32_t       raw_data_len    = 0;
        uint8_t*       raw_data        = NULL;
        smc_user_data_t userdata;

        raw_data_len    = SMC_BYTES_TO_32BIT( (test_input_data + data_index) );
        data_index += 4;

        if( smc == NULL )
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_send_data_raw: Unable to get SMC instance %d", smc_instance_id);
            return SMC_ERROR;
        }

        smc_channel = smc_channel_get(smc, smc_channel_id);

        if( smc_channel == NULL )
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_send_data_raw: Unable to get SMC channel %d from instance %d",
                                            smc_channel_id, smc_instance_id);
            return SMC_ERROR;
        }

        if( raw_data_len <= 0 ) raw_data_len = 1;
        if( raw_data_send_count <= 0 ) raw_data_send_count = 1;

        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: channel %d: send %d bytes of data %d times",
                smc_instance_id, smc_channel->id, raw_data_len, raw_data_send_count);



        int iFailCount = 0;

        for(int i = 0; i < raw_data_send_count; i++ )
        {
            uint32_t x = 0;

            raw_data = (uint8_t*)SMC_MALLOC( raw_data_len );

            if( raw_data != NULL )
            {
                while( x < raw_data_len-3 )
                {
                    raw_data[x++] = 0x53;
                    raw_data[x++] = 0x4D;
                    raw_data[x++] = 0x43;
                }

                userdata.flags     = 0x00000000;
                userdata.userdata1 = raw_data_len;
                userdata.userdata2 = 0x00000000;
                userdata.userdata3 = 0x00000000;
                userdata.userdata4 = 0x00000000;
                userdata.userdata5 = 0x00000000;


                if( smc_send_ext(smc_channel, (void*)raw_data, raw_data_len, &userdata) != SMC_OK )
                {
                    iFailCount++;
                }
             }
             else
             {
                SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_send_data_raw: Unable to allocate %d bytes of memory ",
                        raw_data_len);
                iFailCount++;
             }
        }


        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: SMC instance %d: channel %d: send %d bytes of data %d times, errors %d",
                        smc_instance_id, smc_channel->id, raw_data_len, raw_data_send_count, iFailCount);

        if( iFailCount > 0 )
        {
            test_status = SMC_ERROR;
        }
        else
        {
            test_status = SMC_OK;
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_state: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    return test_status;
}


static uint8_t smc_test_case_channel_fifo( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_status = SMC_ERROR;

    uint16_t test_input_len_required = 3;

    if( test_input_data_len >= test_input_len_required )
    {
        uint32_t       data_index      = 0;
        uint8_t        smc_instance_id = test_input_data[data_index++];
        uint8_t        smc_channel_id  = test_input_data[data_index++];
        smc_channel_t* smc_channel     = NULL;
        smc_t*         smc             = smc_test_get_instance_by_test_instance_id( smc_instance_id );
        uint8_t        test_case       = test_input_data[data_index++];


        if( smc == NULL )
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_channel_fifo: Unable to get SMC instance %d", smc_instance_id);
            return SMC_ERROR;
        }

        smc_channel = smc_channel_get(smc, smc_channel_id);

        if( smc_channel == NULL )
        {
            SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_channel_fifo: Unable to get SMC channel %d from instance %d",
                                            smc_channel_id, smc_instance_id);
            return SMC_ERROR;
        }

        switch(test_case)
        {
            case 0x00:
            {
                uint8_t read_count = 1;

                if( test_input_data_len > 3 )
                {
                    read_count = test_input_data[data_index++];
                }

                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_fifo: SMC instance %d: Channel %d: Read %d FIFO items...", smc_instance_id, smc_channel->id, read_count);

                test_status = SMC_OK;

                for(int i = 0; i < read_count; i++ )
                {
                    int32_t fifo_count = SMC_FIFO_READ_TO_EMPTY;
                    smc_fifo_cell_t celldata;
                   // smc_user_data_t userdata;

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_fifo: SMC instance %d: channel %d: Read item %d...", smc_instance_id, smc_channel->id, (i+1));

                    SMC_LOCK_IRQ( smc_channel->lock_read );

                    fifo_count = smc_fifo_get_cell( smc_channel->fifo_in, &celldata, smc_channel->smc_shm_conf_channel->use_cache_control );

                    SMC_UNLOCK_IRQ( smc_channel->lock_read );

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_fifo: SMC instance %d: channel %d: Item %d read, fifo count value = %d...", smc_instance_id, smc_channel->id, (i+1), fifo_count);
                }

                /*
                if( smc_channel_enable_receive_mode( smc_channel, FALSE ) == SMC_OK )
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_fifo: SMC instance %d: channel %d receive disable OK", smc_instance_id, smc_channel->id);
                    test_status = SMC_OK;
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_channel_fifo: SMC instance %d: channel %d receive disable FAILED", smc_instance_id, smc_channel->id);
                    test_status = SMC_ERROR;
                }
                */
                break;
            }
            default:
            {
                SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_channel_state: Invalid test case 0x%02X", test_case);
                test_status = SMC_ERROR;
                break;
            }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_channel_fifo: not enough test input data (received %d, expected %d)",
                                    test_input_data_len, test_input_len_required);
        test_status = SMC_ERROR;
    }

    return test_status;
}






/* EOF */

