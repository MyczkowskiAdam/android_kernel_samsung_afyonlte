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

Version:       5    22-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  Common platform specific header file added

Version:       1    19-Oct-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMC_TRACE_H
#define SMC_TRACE_H

#include "smc_trace_platform.h"

#define SMC_RD_TRACE_PREFIX                   "SMC:"

/**
 * R&D common traces enabling
 */
#define SMC_TRACE_VERSION_INFO_ENABLED
#define SMC_TRACE_STARTUP_INFO_ENABLED
#define SMC_TRACE_ASSERT_ENABLED
#define SMC_TRACE_ERROR_ENABLED
#define SMC_TRACE_WARNING_ENABLED
/*#define SMC_TRACE_DEBUG_ENABLED*/
/*#define SMC_TRACE_INFO_ENABLED*/


/* -----------------------------------
 * Module specific traces.
 * These flags can be enabled also in the smc_trace_platform.h file
 */

/*
#define SMC_TRACE_INFO_DATA_ENABLED
#define SMC_TRACE_FIFO_ENABLED
#define SMC_TRACE_FIFO_GET_ENABLED
#define SMC_TRACE_FIFO_PUT_ENABLED
#define SMC_TRACE_MDB_ENABLED
#define SMC_TRACE_MDB_ALLOC_ENABLED
#define SMC_TRACE_SIGNALS_ENABLED
#define SMC_TRACE_SIGNAL_RAISE_ENABLED
#define SMC_TRACE_SIGNAL_RECEIVE_ENABLED
#define SMC_TRACE_EVENT_SEND_ENABLED
#define SMC_TRACE_EVENT_RECEIVED_ENABLED
#define SMC_TRACE_LOCK_ENABLED
#define SMC_TRACE_TRANSMIT_ENABLED
#define SMC_TRACE_TRANSMIT_DATA_ENABLED
#define SMC_TRACE_RECEIVE_ENABLED
#define SMC_TRACE_SEND_PACKET_ENABLED
#define SMC_TRACE_SEND_PACKET_DATA_ENABLED
#define SMC_TRACE_RECEIVE_PACKET_ENABLED
#define SMC_TRACE_RECEIVE_PACKET_DATA_ENABLED
#define SMC_TRACE_TIMER_ENABLED
#define SMC_TRACE_RUNTIME_CONF_SHM_ENABLED
#define SMC_TRACE_LOOPBACK_ENABLED
#define SMC_TRACE_LOOPBACK_DATA_ENABLED
#define SMC_TRACE_HISTORY_ENABLED
#define SMC_TRACE_FIFO_BUFFER_ENABLED
#define SMC_TRACE_DMA_ENABLED
#define SMC_TRACE_SHM_VARIABLE_ENABLED
#define SMC_TRACE_SLEEP_CONTROL_ENABLED
*/

/**
 * ----------- R&D Trace macros begin ---------------
 */

#ifdef SMC_TRACE_SLEEP_CONTROL_ENABLED
  #define SMC_TRACE_PRINTF_SLEEP_CONTROL(...)             SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"SLEEPCTRL: " __VA_ARGS__ )
#else
  #define SMC_TRACE_PRINTF_SLEEP_CONTROL(...)
#endif


#ifdef SMC_TRACE_SHM_VARIABLE_ENABLED
  #define SMC_TRACE_PRINTF_SHM_VAR(...)             SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"SHMVAR: " __VA_ARGS__ )
#else
  #define SMC_TRACE_PRINTF_SHM_VAR(...)
#endif

#ifdef SMC_TRACE_DMA_ENABLED
  #define SMC_TRACE_PRINTF_DMA(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"DMA: " __VA_ARGS__ )
#else
  #define SMC_TRACE_PRINTF_DMA(...)
#endif

#ifdef SMC_TRACE_LOOPBACK_ENABLED
  #define SMC_TRACE_PRINTF_LOOPBACK(...)               SMC_TRACE_PRINTF(SMC_RD_TRACE_PREFIX"LOOPBACK: " __VA_ARGS__)

  #ifdef SMC_TRACE_LOOPBACK_DATA_ENABLED
    #define SMC_TRACE_PRINTF_LOOPBACK_DATA(length, data) SMC_TRACE_PRINTF_DATA(length, data)
  #else
    #define SMC_TRACE_PRINTF_LOOPBACK_DATA(length, data)
  #endif
#else
  #define SMC_TRACE_PRINTF_LOOPBACK(...)
  #define SMC_TRACE_PRINTF_LOOPBACK_DATA(length, data)
#endif

#ifdef SMC_TRACE_VERSION_INFO_ENABLED
  #define SMC_TRACE_PRINTF_VERSION(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX" " __VA_ARGS__ )
#else
  #define SMC_TRACE_PRINTF_VERSION(...)
#endif

#ifdef SMC_TRACE_STARTUP_INFO_ENABLED
  #define SMC_TRACE_PRINTF_STARTUP(...)                 SMC_TRACE_PRINTF_UI( SMC_RD_TRACE_PREFIX" " __VA_ARGS__ )
#else
  #define SMC_TRACE_PRINTF_STARTUP(...)
#endif

#ifdef SMC_TRACE_FIFO_ENABLED
  #define SMC_TRACE_PRINTF_FIFO(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"FIFO: " __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_FIFO_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_FIFO(...)
  #define SMC_TRACE_PRINTF_FIFO_DATA( length, data )
#endif

#ifdef SMC_TRACE_FIFO_GET_ENABLED
  #define SMC_TRACE_PRINTF_FIFO_GET(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"FIFO: get:" __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_FIFO_GET_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_FIFO_GET(...)
  #define SMC_TRACE_PRINTF_FIFO_GET_DATA( length, data )
#endif

#ifdef SMC_TRACE_FIFO_PUT_ENABLED
  #define SMC_TRACE_PRINTF_FIFO_PUT(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"FIFO: put:" __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_FIFO_PUT_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_FIFO_PUT(...)
  #define SMC_TRACE_PRINTF_FIFO_PUT_DATA( length, data )
#endif

#ifdef SMC_TRACE_TIMER_ENABLED
  #define SMC_TRACE_PRINTF_TIMER(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"TIMER:" __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_TIMER_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_TIMER(...)
  #define SMC_TRACE_PRINTF_TIMER_DATA( length, data )
#endif

#ifdef SMC_TRACE_EVENT_SEND_ENABLED
  #define SMC_TRACE_PRINTF_EVENT_SEND(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"EVENT: send:" __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_EVENT_SEND_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_EVENT_SEND(...)
  #define SMC_TRACE_PRINTF_EVENT_SEND_DATA( length, data )
#endif

#ifdef SMC_TRACE_EVENT_RECEIVED_ENABLED
  #define SMC_TRACE_PRINTF_EVENT_RECEIVED(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"EVENT: recv:" __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_EVENT_RECEIVED_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_EVENT_RECEIVED(...)
  #define SMC_TRACE_PRINTF_EVENT_RECEIVED_DATA( length, data )
#endif


#ifdef SMC_TRACE_LOCK_ENABLED
  #define SMC_TRACE_PRINTF_LOCK(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"LOCK: " __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_LOCK_MUTEX(...)           SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"LOCK: " __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_LOCK_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_LOCK(...)
  #define SMC_TRACE_PRINTF_LOCK_MUTEX(...)
  #define SMC_TRACE_PRINTF_LOCK_DATA( length, data )
#endif

#ifdef SMC_TRACE_MDB_ENABLED
  #define SMC_TRACE_PRINTF_MDB(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"MDB:  " __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_MDB_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_MDB(...)
  #define SMC_TRACE_PRINTF_MDB_DATA( length, data )
#endif


#ifdef SMC_TRACE_MDB_ALLOC_ENABLED
  #define SMC_TRACE_PRINTF_MDB_ALLOC(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"MDB_ALLOC:  " __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_MDB_ALLOC_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_MDB_ALLOC(...)
  #define SMC_TRACE_PRINTF_MDB_ALLOC_DATA( length, data )
#endif


#ifdef SMC_TRACE_SIGNALS_ENABLED
  #define SMC_TRACE_PRINTF_SIGNAL(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"SIGNL:" __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_SIGNAL_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_SIGNAL(...)
  #define SMC_TRACE_PRINTF_SIGNAL_DATA( length, data )
#endif

#ifdef SMC_TRACE_SIGNAL_RECEIVE_ENABLED
  #define SMC_TRACE_PRINTF_SIGNAL_RECEIVE(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"SIGRCV:" __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_SIGNAL_RECEIVE_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_SIGNAL_RECEIVE(...)
  #define SMC_TRACE_PRINTF_SIGNAL_RECEIVE_DATA( length, data )
#endif

#ifdef SMC_TRACE_SIGNAL_RAISE_ENABLED
  #define SMC_TRACE_PRINTF_SIGNAL_RAISE(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"SIGRSE:" __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_SIGNAL_RAISE_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_SIGNAL_RAISE(...)
  #define SMC_TRACE_PRINTF_SIGNAL_RAISE_DATA( length, data )
#endif

#ifdef SMC_TRACE_TRANSMIT_ENABLED
  #define SMC_TRACE_PRINTF_TRANSMIT(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"XMIT: " __VA_ARGS__ )

  #ifdef SMC_TRACE_TRANSMIT_DATA_ENABLED
      #define SMC_TRACE_PRINTF_TRANSMIT_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
  #else
      #define SMC_TRACE_PRINTF_TRANSMIT_DATA( length, data )
  #endif
#else
  #define SMC_TRACE_PRINTF_TRANSMIT(...)
  #define SMC_TRACE_PRINTF_TRANSMIT_DATA( length, data )
#endif

#ifdef SMC_TRACE_RECEIVE_ENABLED
  #define SMC_TRACE_PRINTF_RECEIVE(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"RECV: " __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_RECEIVE_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_RECEIVE(...)
  #define SMC_TRACE_PRINTF_RECEIVE_DATA( length, data )
#endif

#ifdef SMC_TRACE_SEND_PACKET_ENABLED
  #define SMC_TRACE_PRINTF_SEND_PACKET(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"SEND: " __VA_ARGS__ )

  #ifdef SMC_TRACE_SEND_PACKET_DATA_ENABLED
    #define SMC_TRACE_PRINTF_SEND_PACKET_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
  #else
    #define SMC_TRACE_PRINTF_SEND_PACKET_DATA( length, data )
  #endif
#else
  #define SMC_TRACE_PRINTF_SEND_PACKET(...)
  #define SMC_TRACE_PRINTF_SEND_PACKET_DATA( length, data )
#endif

#ifdef SMC_TRACE_RECEIVE_PACKET_ENABLED
  #define SMC_TRACE_PRINTF_RECEIVE_PACKET(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"RECV: " __VA_ARGS__ )

  #ifdef SMC_TRACE_RECEIVE_PACKET_DATA_ENABLED
    #define SMC_TRACE_PRINTF_RECEIVE_PACKET_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
  #else
    #define SMC_TRACE_PRINTF_RECEIVE_PACKET_DATA( length, data )
  #endif
#else
  #define SMC_TRACE_PRINTF_RECEIVE_PACKET(...)
  #define SMC_TRACE_PRINTF_RECEIVE_PACKET_DATA( length, data )
#endif


#ifdef SMC_TRACE_RUNTIME_CONF_SHM_ENABLED
  #define SMC_TRACE_PRINTF_RUNTIME_CONF_SHM(...)       SMC_TRACE_PRINTF(SMC_RD_TRACE_PREFIX"CONF: " __VA_ARGS__)
#else
  #define SMC_TRACE_PRINTF_RUNTIME_CONF_SHM(...)
#endif

#ifdef SMC_TRACE_HISTORY_ENABLED
  #define SMC_TRACE_PRINTF_HISTORY(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"HISTORY: " __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_HISTORY_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_HISTORY(...)
  #define SMC_TRACE_PRINTF_HISTORY_DATA( length, data )
#endif

#ifdef SMC_TRACE_FIFO_BUFFER_ENABLED
  #define SMC_TRACE_PRINTF_FIFO_BUFFER(...)               SMC_TRACE_PRINTF(SMC_RD_TRACE_PREFIX"FIFO_BUFFER: " __VA_ARGS__)

  #ifdef SMC_TRACE_FIFO_BUFFER_ENABLED_ENABLED
    #define SMC_TRACE_PRINTF_FIFO_BUFFER_DATA(length, data) SMC_TRACE_PRINTF_DATA(length, data)
  #else
    #define SSMC_TRACE_PRINTF_FIFO_BUFFER_DATA(length, data)
  #endif
#else
  #define SMC_TRACE_PRINTF_FIFO_BUFFER(...)
  #define SMC_TRACE_PRINTF_FIFO_BUFFER_DATA(length, data)
#endif

#ifdef SMC_TRACE_ERROR_ENABLED
  #define SMC_TRACE_PRINTF_ERROR(...)                 SMC_TRACE_PRINTF_ALWAYS_ERROR( SMC_RD_TRACE_PREFIX"ERR:  " __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_ERROR_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_ERROR(...)
  #define SMC_TRACE_PRINTF_ERROR_DATA( length, data )
#endif

#ifdef SMC_TRACE_WARNING_ENABLED
  #define SMC_TRACE_PRINTF_WARNING(...)               SMC_TRACE_PRINTF_ALWAYS(SMC_RD_TRACE_PREFIX"WARN: " __VA_ARGS__)
#else
  #define SMC_TRACE_PRINTF_WARNING(...)
#endif

#ifdef SMC_TRACE_DEBUG_ENABLED
  #define SMC_TRACE_PRINTF_DEBUG(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"DEBUG:" __VA_ARGS__ )
  #define SMC_TRACE_PRINTF_DEBUG_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
#else
  #define SMC_TRACE_PRINTF_DEBUG(...)
  #define SMC_TRACE_PRINTF_DEBUG_DATA( length, data )
#endif

#ifdef SMC_TRACE_INFO_ENABLED
  #define SMC_TRACE_PRINTF_INFO(...)                 SMC_TRACE_PRINTF( SMC_RD_TRACE_PREFIX"INFO: " __VA_ARGS__ )

  #ifdef SMC_TRACE_INFO_DATA_ENABLED
    #define SMC_TRACE_PRINTF_INFO_DATA( length, data ) SMC_TRACE_PRINTF_DATA(length, data)
  #else
    #define SMC_TRACE_PRINTF_INFO_DATA( length, data )
  #endif
#else
  #define SMC_TRACE_PRINTF_INFO(...)
  #define SMC_TRACE_PRINTF_INFO_DATA( length, data )
#endif

#ifdef SMC_TRACE_ASSERT_ENABLED
  #define SMC_TRACE_PRINTF_ASSERT(...)                 SMC_TRACE_PRINTF_ALWAYS(SMC_RD_TRACE_PREFIX"ASSERT: *** " __VA_ARGS__)
#else
  #define SMC_TRACE_PRINTF_ASSERT(...)
#endif

#define SMC_TRACE_PRINTF_CRASH_INFO(...)                 SMC_TRACE_PRINTF_ALWAYS( __VA_ARGS__ )

#endif /* EOF */

