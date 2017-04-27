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

#ifndef SMC_TEST_H
#define SMC_TEST_H


/*#define SMC_INCLUDE_PERF_TEST*/

#if (defined SMECO_LINUX_ANDROID || defined SMECO_LINUX_KERNEL)

    /**
     * Wrap the Modem BFAT test handler to Android and Linux Kernel
     */

  typedef uint32_t  os_task_token;
  typedef uint8_t   os_prior;
  typedef uint32_t  os_entrypoint_t;
  typedef uint16_t  modem_test_group_id_t;
  typedef uint8_t   modem_test_status_t;
  typedef uint32_t* modem_test_handler_ptr_t;

  #define MODEM_TEST_FAILED SMC_ERROR
  #define MODEM_TEST_PASSED SMC_OK
  #define SMC_TEST_HDLR     0xF1

      /*
       * Struct for test register function
       */
  typedef struct
  {
      uint16_t case_ID;
      uint16_t inp_data_length;
      uint8_t* inp_data;
  } modem_test_req_str_t;

  static inline void modem_test_set_timeout(uint16_t timeout)
  {
      /* Nothing to set */
  }

  static inline void modem_test_handler_register(uint16_t id, uint16_t hdlr_id, uint32_t* init_func)
  {
      /* Nothing to register */
  }

#else
  #include "mexe_build_flags.h"
  #include "test_hdlr_id_mexe_int.h"
  #include "test_handler_modem_int.h"
  #include "modem_test_isi.h"

#endif

#include "smc_test_fifo.h"
#include "smc_test_messaging.h"
#include "smc_test_mdb.h"

#if (defined SMECO_LINUX_ANDROID || defined SMECO_LINUX_KERNEL)
      /* Android and Linux Kernel mappings */
    #define SMC_TEST_GROUP_ID        0x2F
    #define SMC_TEST_GROUP_ID_REMOTE 0x1F

    #define SMC_CPU_ID_LOCAL         0x10
    #define SMC_CPU_ID_REMOTE        0x02
#else

    /*
     * Cortex specific test variables
     */
  #if(MEXE_TARGET_IMAGE == MEXE_TARGET_IMAGE_L1)

    #define SMC_TEST_GROUP_ID        MEXE_L1_TEST_GROUP
    #define SMC_TEST_GROUP_ID_REMOTE MEXE_L23_TEST_GROUP

    #define SMC_CPU_ID_LOCAL         0x01
    #define SMC_CPU_ID_REMOTE        0x02

  #else

    #define SMC_TEST_GROUP_ID        MEXE_L23_TEST_GROUP
    #define SMC_TEST_GROUP_ID_REMOTE MEXE_L1_TEST_GROUP

    #define SMC_CPU_ID_LOCAL         0x02
    #define SMC_CPU_ID_REMOTE        0x01

  #endif

#endif

    /*
     * Configuration test case message ID:s
     */
#define SMC_TEST_CONFIG_SET_SHARED_SDRAM   0x00      // Configure Shared memory area


#define SMC_BYTES_TO_32BIT(data)    ((data[0]<<24) + (data[1]<<16) + (data[2]<<8) + (data[3]&0xFF))
#define SMC_BYTES_TO_16BIT(data)    ((data[0]<< 8) + (data[1]&0xFF))

#define DELAY_SMC_TEST(delayrnd)  {                                   \
                                  volatile uint32_t delay = delayrnd; \
                                  while(delay>0) delay--;             \
                                  }                                   \


    /*
     * Test Trace defines
     */
#define SMC_TEST_RD_TRACE_PREFIX                        "SMC_TEST:"

#define SMC_TEST_TRACE_PRINTF                           SMC_TRACE_PRINTF_ALWAYS
#define SMC_TEST_TRACE_PRINTF_DATA(length, data)        SMC_TRACE_PRINTF_ALWAYS_DATA(length, data)


#define SMC_TEST_TRACE_PRINTF_INFO(...)                 SMC_TEST_TRACE_PRINTF( SMC_TEST_RD_TRACE_PREFIX"INFO:" __VA_ARGS__ )
#define SMC_TEST_TRACE_PRINTF_INFO_DATA( length, data ) SMC_TEST_TRACE_PRINTF_DATA(length, data)

#define SMC_TEST_TRACE_PRINTF_ERROR(...)                 SMC_TEST_TRACE_PRINTF( SMC_TEST_RD_TRACE_PREFIX"ERROR:" __VA_ARGS__ )
#define SMC_TEST_TRACE_PRINTF_ERROR_DATA( length, data ) SMC_TEST_RACE_PRINTF_DATA(length, data)

#define SMC_TEST_TRACE_PRINTF_DEBUG(...)                 SMC_TEST_TRACE_PRINTF( SMC_TEST_RD_TRACE_PREFIX"INFO:" __VA_ARGS__ )
#define SMC_TEST_TRACE_PRINTF_DEBUG_DATA( length, data ) SMC_TEST_TRACE_PRINTF_DATA(length, data)


  /*
   * SMC Data structure for loopback messaging.
   */
typedef struct _smc_loopback_data_t
{
  uint32_t round_trip_counter;
  uint32_t loopback_data_length;
  uint32_t timestamp;
  uint32_t loopback_rounds_left;

  uint8_t  loopback_data[1];

} smc_loopback_data_t;




    /* SMC test case execution function prototype */
typedef uint8_t ( *smc_test_case_function )( uint8_t* test_input_data, uint16_t test_input_data_len );

void smc_test_handler_register( void );

uint8_t smc_test_handler_start(uint16_t test_case_id, uint16_t test_data_input_len, uint8_t* test_data_input);

os_task_token smc_os_task_create(const char* task_name, os_prior task_priority, os_entrypoint_t task_function);
uint8_t       smc_os_task_start (const char* task_name, os_prior task_priority, os_entrypoint_t task_function,
                                 uint8_t* test_input_data, uint16_t test_input_data_len, os_task_token* new_task_id);

smc_t* smc_test_get_instance_by_test_instance_id( uint8_t smc_instance_id);


/*
 * Loopback test functions
 */
smc_loopback_data_t*  smc_loopback_data_create( uint32_t size_of_message_payload, uint8_t from_irq );
uint8_t               smc_send_loopback_data_message( smc_channel_t* smc_channel, uint32_t loopback_data_len, uint32_t loopback_rounds, uint8_t from_irq );
uint8_t               smc_handle_loopback_data_message( smc_channel_t* smc_channel, smc_loopback_data_t* loopback_data, smc_user_data_t* userdata, uint8_t from_irq);

#endif /* EOF */


