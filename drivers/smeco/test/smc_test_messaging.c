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

Version:       1    11-Nov-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#include "smc_common_includes.h"

/*#include "smc_conf.h"*/
#include "smc_fifo.h"
#include "smc.h"
#include "smc_test.h"
#include "smc_test_fifo.h"
#include "smc_trace.h"

#ifdef SMECO_MODEM
    /*TODO Remove I2C tests */
  #define INCLUDE_I2C_TEST


#if( SMC_L2MUX_IF == TRUE )
    #include "../modem/smc_conf_l2mux_modem.h"
#endif

#endif

#ifdef INCLUDE_I2C_TEST
  #include "i2c_drv_if.h"
  #include "power_hal_modem_ext.h"

  static uint8_t smc_start_messaging_test_i2c(uint8_t* test_input_data, uint16_t test_input_data_len);

#endif

    /* Local messaging test case functions */
static uint8_t smc_start_messaging_test_single_cpu(uint8_t* test_input_data, uint16_t test_input_data_len);

static uint8_t smc_test_handler_send_phonet_message(uint8_t* test_input_data, uint32_t test_input_data_len );
static uint8_t smc_test_handler_send_event(uint8_t* test_input_data, uint32_t test_input_data_len);

uint8_t smc_test_case_function_messaging( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t  test_result = SMC_ERROR;
    uint16_t test_input_len_required = 2;

    SMC_TEST_TRACE_PRINTF_INFO("Messaging test case invoked, input data len %d", test_input_data_len);

    if( test_input_data_len >= test_input_len_required )
    {
        uint16_t data_index = 0;
        uint8_t  test_case  = test_input_data[data_index++];

        switch( test_case )
        {
            case 0x00:
            {
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: smc_start_messaging_test_single_cpu test case starting...");

                test_result = smc_start_messaging_test_single_cpu((test_input_data+data_index), test_input_data_len-data_index);

                break;
            }
            case 0x01:
            {
                smc_t* smc_instance_control = NULL;
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: Send using SMC control instance...");

                smc_instance_control = smc_instance_get_control();

                if( smc_instance_control != NULL )
                {
                    uint8_t         smc_channel_id = 0;
                    uint32_t        data_length    = (test_input_data_len-data_index);
                    uint8_t*        data           = (test_input_data + data_index);
                    smc_user_data_t userdata;

                    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_messaging (SMC Control): Sending data using channel %d...", smc_channel_id);

                    /* Put the length to the userdata in case of L2MUX channel
                     * TODO add own parameter ??
                     */
                    userdata.flags     = 0x00000000;
                    userdata.userdata1 = 0x00000000;
                    userdata.userdata2 = 0x00000000;
                    userdata.userdata3 = 0x00000000;
                    userdata.userdata4 = 0x00000000;
                    userdata.userdata5 = 0x00000000;

                    SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_messaging: sending %d bytes of data...", data_length);
                    SMC_TEST_TRACE_PRINTF_DEBUG_DATA(data_length, data);

                    test_result = smc_send_ext(SMC_CHANNEL_GET(smc_instance_control, smc_channel_id), (void*)data, data_length, &userdata);
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_messaging: SMC control channel is not available");
                    test_result = SMC_ERROR;
                }

                break;
            }
            case 0x02:
            {
                uint32_t data_length = (test_input_data_len-data_index);

                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: smc_test_handler_send_phonet_message...");

                test_result = smc_test_handler_send_phonet_message(&test_input_data[data_index], data_length );

                break;
            }
            case 0x03:
            {
                uint32_t data_length = (test_input_data_len-data_index);

                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: smc_test_handler_send_event...");

                test_result = smc_test_handler_send_event( &test_input_data[data_index], data_length );

                break;
            }
            case 0x04:
            {
                test_input_len_required = 4;

                if( test_input_data_len >= test_input_len_required )
                {
                    uint8_t smc_instance_id = test_input_data[data_index++];
                    uint8_t smc_channel_id  = test_input_data[data_index++];
                    uint8_t enable          = (test_input_data[data_index++]==0x01);
                    smc_t*  smc_instance    = NULL;

                    smc_instance = smc_test_get_instance_by_test_instance_id( smc_instance_id );

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: %s sending in SMC instance 0x%02X channel %d ...",
                                                (enable?"ENABLE":"DISABLE"), smc_instance_id, smc_channel_id);

                    if( smc_instance )
                    {
                        test_result = smc_channel_enable_receive_mode( SMC_CHANNEL_GET(smc_instance, smc_channel_id), enable);
                    }
                    else
                    {
                        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_messaging: Test case 0x%02X failed: No proper SMC instance found", test_case);
                        test_result = SMC_ERROR;
                    }
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: not enough test input data (received %d, expected %d)",
                                                                test_input_data_len, test_input_len_required);
                    test_result = SMC_ERROR;
                }

                break;
            }
            case 0x05:
            {
                test_input_len_required = 4;

                if( test_input_data_len >= test_input_len_required )
                {
                    uint8_t  smc_instance_id = test_input_data[data_index++];
                    uint8_t  smc_channel_id  = test_input_data[data_index++];
                    uint8_t* data            = &test_input_data[data_index];
                    uint8_t  data_length     = test_input_data_len-data_index;
                    smc_t*   smc_instance    = NULL;

                    smc_instance = smc_test_get_instance_by_test_instance_id( smc_instance_id );

                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: send data using SMC instance 0x%02X channel %d...",
                                                smc_instance_id, smc_channel_id );
                    if( smc_instance )
                    {
                        smc_user_data_t userdata;

                        /* Put the length to the userdata in case of L2MUX channel
                         * TODO add own parameter ??
                         */
                        userdata.flags     = 0x00000000;
                        userdata.userdata1 = data_length&0x00FFFFFF;
                        userdata.userdata2 = 0x00000000;
                        userdata.userdata3 = 0x00000000;
                        userdata.userdata4 = 0x00000000;
                        userdata.userdata5 = 0x00000000;

                        SMC_TEST_TRACE_PRINTF_DEBUG("smc_test_case_function_messaging: sending %d bytes of data, userdata1 0x%08X...", data_length, userdata.userdata1);
                        SMC_TEST_TRACE_PRINTF_DEBUG_DATA(data_length, data);

                        test_result = smc_send_ext(SMC_CHANNEL_GET(smc_instance, smc_channel_id), (void*)data, data_length, &userdata);
                    }
                    else
                    {
                        SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_messaging: Test case 0x%02X failed: No proper SMC instance found", test_case);
                        test_result = SMC_ERROR;
                    }
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: not enough test input data (received %d, expected %d)",
                                                                test_input_data_len, test_input_len_required);
                    test_result = SMC_ERROR;
                }
                break;
            }
            case 0xFE:
            {
#ifdef INCLUDE_I2C_TEST
                SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: smc_start_messaging_test_i2c dummy test case starting...");
                test_result = smc_start_messaging_test_i2c((test_input_data+data_index), test_input_data_len-data_index);
#else
                SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_messaging: SMC dummy test is not available");
                test_result = SMC_ERROR;
#endif
                break;
            }
            default:
            {
                SMC_TEST_TRACE_PRINTF_ERROR("smc_test_case_function_messaging: Invalid test case 0x%02X", test_case);
                test_result = SMC_ERROR;
                break;
            }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: not enough test input data (received %d, expected %d)",
                                            test_input_data_len, test_input_len_required);
        test_result = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("smc_test_case_function_messaging: Test completed with return value 0x%02X", test_result);

    return test_result;
}

static uint8_t smc_start_messaging_test_single_cpu(uint8_t* test_input_data, uint16_t test_input_data_len)
{
    uint8_t test_result = SMC_ERROR;
    
    SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_single_cpu: START Messaging test single CPU, (Input data len %d)...", test_input_data_len);
    SMC_TEST_TRACE_PRINTF_INFO_DATA(test_input_data_len, test_input_data);
    
    SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_single_cpu: NOT IMPLEMENTED");

    SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_single_cpu: Messaging test single CPU completed by return value 0x%02X", test_result);
    
    return test_result;
}

static uint8_t smc_test_handler_send_phonet_message(uint8_t* test_input_data, uint32_t test_input_data_len)
{
    uint8_t return_value = SMC_ERROR;

#ifdef SMECO_MODEM
#if(MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L2)

    if( test_input_data_len >= 10 )
    {

        pn_msg* pn_message = (pn_msg*)SMC_MALLOC(test_input_data_len);

        memcpy( pn_message, test_input_data, test_input_data_len);


        if( pn_msg_send(pn_message) == PN_OK )
        {
            SMC_TRACE_PRINTF_INFO( "smc_test_handler_send_phonet_message: Phonet message 0x%08X successfully sent", pn_message);
            return_value = SMC_OK;
        }
        else
        {
            SMC_TRACE_PRINTF_INFO( "smc_test_handler_send_phonet_message: Phonet message 0x%08X send failed", pn_message);
            /* Do not free the message */
            return_value = SMC_ERROR;
        }
    }
    else
    {
        SMC_TRACE_PRINTF_INFO( "smc_test_handler_send_phonet_message: not enough data for phonet message");
        return_value = SMC_ERROR;

    }

#else
    SMC_TRACE_PRINTF_ERROR( "smc_test_handler_send_phonet_message: IMPLEMENTED ONLY IN L2");
#endif
#else
    SMC_TRACE_PRINTF_ERROR( "smc_test_handler_send_phonet_message: IMPLEMENTED ONLY IN MODEM");
#endif

    return return_value;
}


static uint8_t smc_test_handler_send_event(uint8_t* test_input_data, uint32_t test_input_data_len)
{
    uint8_t return_value = SMC_ERROR;

#ifdef SMECO_MODEM
#if(MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L2)

    if( test_input_data_len >= 2 )
    {
        smc_t* smc_instance = NULL;
        uint8_t channel_id = test_input_data[0];
        uint8_t event      = test_input_data[1];

#if( SMC_L2MUX_IF == TRUE )
        SMC_TRACE_PRINTF_DEBUG( "smc_test_handler_send_event: get L2MUX instance...");
        smc_instance = get_smc_instance_l2mux();
#else
#error "L2MUX Flag not visible"
#endif

        if( smc_instance != NULL )
        {
            SMC_TRACE_PRINTF_DEBUG( "smc_test_handler_send_event: Send event %d to channel %d", event, channel_id);

            smc_channel_t* smc_channel = SMC_CHANNEL_GET( smc_instance, channel_id );

            return_value = smc_send_event(smc_channel, (SMC_CHANNEL_EVENT)event);

        }
        else
        {
             SMC_TRACE_PRINTF_ERROR( "smc_test_handler_send_event: no proper SMC instance available");
             return_value = SMC_ERROR;
         }
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR( "smc_test_handler_send_event: not enough data for event message");
        return_value = SMC_ERROR;
    }

#else
    SMC_TRACE_PRINTF_ERROR( "smc_test_handler_send_event: IMPLEMENTED ONLY IN L2");
#endif
#else
    SMC_TRACE_PRINTF_ERROR( "smc_test_handler_send_event: IMPLEMENTED ONLY IN MODEM");
#endif
    SMC_TRACE_PRINTF_INFO( "smc_test_handler_send_event: completed by return value %d", return_value);
    return return_value;

}



#ifdef INCLUDE_I2C_TEST

static uint8_t smc_start_messaging_test_i2c(uint8_t* test_input_data, uint16_t test_input_data_len)
{
    uint8_t  test_result             = SMC_ERROR;
    uint16_t test_input_len_required = 4;

    SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: START I2C test, (Input data len %d)...", test_input_data_len);
    SMC_TEST_TRACE_PRINTF_INFO_DATA(test_input_data_len, test_input_data);

    if( test_input_data_len >= test_input_len_required )
    {
        i2c_result_t ret_val        = I2C_DRV_ERROR;
        uint16_t     data_index     = 0;
        uint8_t      exec_write     = test_input_data[data_index++];
        uint8_t      address        = test_input_data[data_index++];
        uint8_t      slave_register = test_input_data[data_index++];
        uint8_t*     data           = &test_input_data[data_index];
        uint16_t     data_len       = test_input_data_len-data_index;
        uint16_t     loop_times     = 1;
        uint8_t      data_to_read[] = {0,0,0,0,0,0,0,0,0,0};



        SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: sysdrv_i2c_init...");

        sysdrv_i2c_init();

        SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: hw_resource_control power up...");
        hw_resource_control(HW_EXT_I2C, HW_RESOURCE_ON, NULL);

        if( exec_write == 0x02 )
        {
            uint8_t start_address = address;
            uint8_t end_address  = slave_register;
            int     devices_found = 0;
            slave_register = 0x00;
            data_len = 1;

            uint8_t   device_list[0xFF];

            memset( &device_list[0], 0, 0xFF);

            if( end_address< start_address ) end_address = start_address;

            SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: scanning address range 0x%02X-0x%02X...",
                    start_address, end_address);

            for(uint8_t i = start_address; i <= end_address; i++ )
            {
                data_to_read[0] = 0x00;

                SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: Write address 0x%02X, register 0x%02X...", i, slave_register);

                ret_val = sysdrv_i2c_write( i, slave_register, data_len, data );

                SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: write ret_val = 0x%02X (%s)", ret_val, (ret_val==I2C_DRV_SUCCESS)?"SUCCESS":"ERROR");

                if( ret_val == I2C_DRV_SUCCESS )
                {
                    // Read something from the device

                    if( i == 0x48 || i == 0x49 || i==0x4A )
                    {
                        SMC_TEST_TRACE_PRINTF_DEBUG( "smc_start_messaging_test_i2c: ============= Check TPS80032 Status...");
                        data_to_read[0] = 0x00;

                        DELAY_SMC_TEST(5000);

                        ret_val = sysdrv_i2c_read( 0x4A, 0x87, 1, &data_to_read[0]);

                        if( ret_val == I2C_DRV_SUCCESS )
                        {
                            SMC_TEST_TRACE_PRINTF_DEBUG( "smc_start_messaging_test_i2c: ============= Check TPS80032 Status. OK IC=0x%02X", data_to_read[0]);
                            device_list[devices_found++] = i;
                        }
                        else
                        {
                            SMC_TEST_TRACE_PRINTF_ERROR( "smc_start_messaging_test_i2c: ============= TPS80032 Status not found");
                        }

                    }
                    else
                    {
                        device_list[devices_found++] = i;
                    }
                }

                // Delay
                DELAY_SMC_TEST(5000);

                /*
                ret_val = sysdrv_i2c_read( i, slave_register, data_len, &data_to_read[0]);
                SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: read ret_val = 0x%02X (%s)", ret_val, (ret_val==I2C_DRV_SUCCESS)?"SUCCESS":"ERROR");
                */
            }

            if( devices_found > 0 )
            {
                SMC_TEST_TRACE_PRINTF_DEBUG( "smc_start_messaging_test_i2c: ============= FOLLOWING I2C DEVICES FOUND FROM BUS (%d):", devices_found);
                SMC_TEST_TRACE_PRINTF_DEBUG( "" );

                for(int i = 0; i < devices_found; i++ )
                {
                    SMC_TEST_TRACE_PRINTF_DEBUG( "smc_start_messaging_test_i2c: - 0x%02X", device_list[i] );
                }

                SMC_TEST_TRACE_PRINTF_DEBUG( "" );
                SMC_TEST_TRACE_PRINTF_DEBUG( "smc_start_messaging_test_i2c: ============= END OF I2C Device list");
            }
            else
            {
                SMC_TEST_TRACE_PRINTF_ERROR( "smc_start_messaging_test_i2c: ============= NO I2C DEVICES FOUND FROM BUS");
            }
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: %s address 0x%02X, register 0x%02X, data len %d, loop times %d",
                    (exec_write == 0x01)?"WRITE to":"READ from", address, slave_register, data_len, loop_times);

            SMC_TEST_TRACE_PRINTF_INFO_DATA(data_len, data);

            for(int i = 0; i < loop_times; i++)
            {
                if (exec_write == 0x01)
                {
                    ret_val = sysdrv_i2c_write( address, slave_register, data_len, data );
                }
                else
                {
                    if( data_len > 10 )
                    {
                        data_len = 10;
                    }

                    ret_val = sysdrv_i2c_read( address, slave_register, data_len, &data_to_read[0]);
                }

                if( ret_val == I2C_DRV_SUCCESS )
                {
                    if (exec_write == 0x01)
                    {
                        SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: write success in %d", i );
                    }
                    else
                    {
                        SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: read success in %d, len %d", i, data_len );
                        SMC_TEST_TRACE_PRINTF_INFO_DATA(data_len, data_to_read);
                    }

                    break;
                }

                if( (i&0x0A)==i )
                {
                    SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: loop %d", i );
                }
            }
         }

        SMC_TEST_TRACE_PRINTF_INFO( "smc_start_messaging_test_i2c: completed, ret_val = 0x%02X (%s)",
                ret_val, (ret_val==I2C_DRV_SUCCESS)?"SUCCESS":"ERROR");

        if( ret_val == I2C_DRV_SUCCESS )
        {
            test_result = SMC_OK;
        }
        else
        {
            test_result = SMC_ERROR;
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_INFO("smc_start_messaging_test_i2c: not enough test input data (received %d, expected %d)",
                                                    test_input_data_len, test_input_len_required);
    }


    return test_result;
}

#endif
