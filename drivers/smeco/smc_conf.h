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

Version:       10   31-Jan-2012     Heikki Siikaluoma
Status:        draft
Description :  Signal type configuration added.

Version:       9    11-Jan-2012     Heikki Siikaluoma
Status:        draft
Description :  Structure for the SMC instance configuration.

Version:       8    21-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  Change common name for platform specific configuration header

Version:       7    14-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  Linux Kernel object target included

Version:       3    08-Nov-2011     Heikki Siikaluoma
Status:        draft
Description :  Platform independent code implemented

Version:       1    19-Oct-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif


#ifndef SMC_CONF_H
#define SMC_CONF_H

    /* Product configurations */

#include "smc_instance_config.h"
#include "smc_conf_platform.h"    /* Platform specific configuration header */
#include "smc_memory_mgmt.h"


struct _smc_channel_t;

/**
 * Configuration structure for one logical SMC Channel.
 */
typedef struct
{
    uint8_t           channel_id;
    uint8_t           priority;                         /* The priority of the channel, the highest priority is 0 */
    uint8_t           copy_scheme;                      /* Copy scheme bits: SMC_COPY_SCHEME_COPY_IN_SEND | SMC_COPY_SCHEME_COPY_IN_RECEIVE*/
    uint8_t           wake_lock_flags;                  /* Channel specific Wakeup lock policy */

    uint32_t          fifo_size_in;                     /* The size of the FIFO receiving data (remote) */
    uint32_t          fifo_size_out;                    /* The size of the FIFO sending data (local) */
    uint32_t          mdb_size_in;                      /* The size of the MDB for receiving data (remote) */
    uint32_t          mdb_size_out;                     /* The size of the MDB for sending data (local) */
    smc_signal_t*     signal_remote;                    /* The signal used to indicate remote that data has been sent */
    smc_signal_t*     signal_local;                     /* The signal that indicates that data is received from remote side */

    void*             smc_receive_data_cb;              /* Function pointer for data receiving      */
    void*             smc_receive_data_allocator_cb;    /* Function pointer for memory allocation   */
    void*             smc_send_data_deallocator_cb;     /* Function pointer for memory deallocation */
    void*             smc_event_cb;                     /* Function pointer for receiving events    */

    uint8_t           protocol;
    uint16_t          fifo_full_check_timeout_usec;     /* Timeout in microseconds for timer to check if FIFO full situation is over */
    uint8_t           trace_features;                   /* Slave  Runtime trace features: SMC_TRACE_HISTORY_MESSAGE_SEND, SMC_TRACE_HISTORY_MESSAGE_RECEIVE */

    uint32_t          wakelock_timeout_ms;              /* Timeout in milliseconds to keep UE wake after handling a message */

    uint16_t          history_data_max;                 /* Maximum amount of history data items collected in master */
    uint16_t          fill1;

    uint16_t          rx_mem_realloc_check_timeout_usec;     /* Timeout in microseconds for timer to check if there is mem available */

} smc_channel_conf_t;


/**
 * Configuration structure for one SMC instance.
 */
typedef struct
{
    uint8_t              cpu_id_remote;                 /* ID of the remote CPU */
    uint8_t              cpu_id_local;                  /* ID of the CPU used locally */
    uint8_t              is_master;                     /* If set the SMC instance is master */
    uint8_t              smc_channel_conf_count;        /* Count of SMC channels in smc_channel_conf_ptr_array */
    smc_channel_conf_t** smc_channel_conf_ptr_array;

    smc_shm_config_t*    smc_shm_conf;                  /* Global shared memory configuration (in SMC instance level) */
    char*                name;
    uint32_t             initialization_flags;          /* Various instance initialization flags */

} smc_conf_t;

/**
 * Common configuration structures for SMC instance.
 * Used in the product configuration.
 */

typedef struct _smc_instance_conf_channel_t
{
    char*    name;
    uint32_t protocol;

    uint32_t fifo_size_master;
    uint32_t fifo_size_slave;

    uint32_t mdb_size_master;         /* Size of the MDB for data from master to slave */
    uint32_t mdb_size_slave;          /* Size of the MDB for data from slave to master */

        /*
         * Signal configuration for master side
         */
    uint32_t signal_id_master_to_slave;             /* Master is raising the signal */
    uint32_t signal_type_master_to_slave;

    uint32_t signal_id_master_from_slave;           /* Master is waiting the signal */
    uint32_t signal_type_master_from_slave;

        /*
         * Signal configuration in slave side
         */
    uint32_t signal_id_slave_to_master;             /* Slave is raising the signal */
    uint32_t signal_type_slave_to_master;

    uint32_t signal_id_slave_from_master;           /* Slave is waiting the signal */
    uint32_t signal_type_slave_from_master;

    uint16_t fifo_full_check_timeout_usec_master;
    uint16_t fifo_full_check_timeout_usec_slave;

    uint16_t rx_mem_realloc_check_timeout_usec_master;
    uint16_t rx_mem_realloc_check_timeout_usec_slave;

    uint8_t  priority;
    uint8_t  copy_scheme_master;                    /* Copy scheme used in the master */
    uint8_t  copy_scheme_slave;                     /* Copy scheme used in the slave */
    uint8_t  trace_features_master;                 /* Master Runtime trace features: SMC_TRACE_HISTORY_MESSAGE_SEND, SMC_TRACE_HISTORY_MESSAGE_RECEIVE */

    uint8_t  trace_features_slave;                  /* Slave  Runtime trace features: SMC_TRACE_HISTORY_MESSAGE_SEND, SMC_TRACE_HISTORY_MESSAGE_RECEIVE */
    uint8_t  wake_lock_flags_master;                /* Wakelock policy in master side */
    uint8_t  wake_lock_flags_slave;                 /* Wakelock policy in slave side  */
    uint8_t  fill1;                                 /*  */

    uint32_t wakelock_timeout_ms;                   /* Timeout in milliseconds to keep UE wake after handling a message */

    uint16_t history_data_max_master;               /* Maximum amount of history data items collected in master */
    uint16_t history_data_max_slave;                /* Maximum amount of history data items collected in slave */

} smc_instance_conf_channel_t;


typedef struct _smc_instance_conf_t
{
    char*    name;                      /* Unique name of the configuration */
    char*    user_name;                 /* Name of the SMC user (e.g. L2MUX) */
    char*    master_name;               /* Master CPU name */
    char*    slave_name;                /* Slave CPU name */

    uint16_t master_cpu_version_major;
    uint16_t master_cpu_version_minor;
    uint16_t slave_cpu_version_major;
    uint16_t slave_cpu_version_minor;

    uint32_t shm_start_address;
    uint32_t shm_start_address_offset;
    uint32_t shm_size;

        /* Share memory configuration */
    uint8_t  shm_use_cache_control_master;  /* Master cache control */
    uint8_t  shm_use_cache_control_slave;   /* Slave cache control */
    uint8_t  shm_cache_line_len_master;     /* Master cache line length */
    uint8_t  shm_cache_line_len_slave;      /* Slave cache line length */

    SMC_SHM_OFFSET_TYPE shm_memory_offset_type_master_to_slave; /* Offset type in master point of view */
    uint32_t shm_cpu_memory_offset;                             /* Offset between master and slave */

    smc_instance_conf_channel_t* channel_config_array;
    uint8_t  channel_config_count;
    uint8_t  fill3;
    uint8_t  fill2;
    uint8_t  fill1;

    uint32_t initialization_flags_master;
    uint32_t initialization_flags_slave;

} smc_instance_conf_t;


/*
 * SMC channel runtime configuration for fixed configuration
 * This can be used only by using other channel.
 */
typedef struct _smc_channel_runtime_fixed_conf_t
{
    uint32_t  channel_id;        /* Target channel */

    int32_t   fifo_in_size;      /* Sender takes care that this means target channels IN FIFO */
    int32_t   fifo_out_size;     /* Sender takes care that this means target channels OUT FIFO */

    uint32_t  shm_start_address; /* Shared memory start address for the channel */
    uint32_t  shm_size;          /* Max SHM size for the channel*/

    uint32_t  mdb_in_size;       /* Sender takes care that this means target channels IN MDB */
    uint32_t  mdb_out_size;      /* Sender takes care that this means target channels OUT MDB */

} smc_channel_runtime_fixed_conf_t;

/*
 * SMC instance configuration API functions.
 */
smc_conf_t*          smc_conf_create( void );
void                 smc_conf_destroy( smc_conf_t* smc_conf );

/*
 * SMC Channel configuration API functions.
 */
smc_channel_conf_t*  smc_channel_conf_create( void );

void                 smc_channel_conf_destroy( smc_channel_conf_t* smc_channel_conf);
void                 smc_conf_add_channel_conf(smc_conf_t* smc_conf, smc_channel_conf_t* smc_channel_conf);

smc_conf_t*          smc_conf_create_from_instance_conf( char* smc_cpu_name, smc_instance_conf_t* smc_instance_conf);
smc_channel_conf_t*  smc_channel_conf_create_from_instance_conf( smc_instance_conf_channel_t* smc_instance_conf_channel, uint8_t is_master );

smc_instance_conf_t* smc_instance_conf_get_from_list(smc_instance_conf_t* smc_instance_conf_array, int config_count, char* smc_user_name, char* smc_name);
char*                smc_instance_conf_name_get_from_list(smc_instance_conf_t* smc_instance_conf_array, int config_count, char* smc_user_name, uint8_t is_master, uint8_t version_major, uint8_t version_minor);

smc_channel_runtime_fixed_conf_t*  smc_channel_runtime_fixes_conf_create( void );

/* =====================================================
 * Runtime configuration functions.
 *
 * These functions have device specific implementations (android/linux kernel/modem)
 */
uint8_t              smc_conf_request_initiate ( struct _smc_channel_t* channel );
uint8_t              smc_conf_request_received ( struct _smc_channel_t* channel, uint32_t configuration_id, uint32_t configuration_value );
void                 smc_conf_response_received( struct _smc_channel_t* channel, uint32_t configuration_id, uint32_t configuration_value_requested, uint32_t configuration_response_value );

/*
 * These configuration functions have common implementation in the smc_conf.c
 */
uint8_t              smc_shm_conf_request_received ( struct _smc_channel_t* channel, int32_t fifo_out_size, int32_t fifo_in_size, uint32_t mdb_start_address, uint32_t mdb_size );
void                 smc_shm_conf_response_received( struct _smc_channel_t* channel, int32_t fifo_out_size, int32_t fifo_in_size, uint32_t mdb_start_address, uint32_t mdb_size );


/*
 * Runtime configuration features
 */

#define SMC_RUNTIME_CONFIG_ID_APE_WAKEUP_EVENT_SENSE   0x00000001
#define SMC_RUNTIME_CONFIG_ID_APE_WAKEUP_LOCK_TIME_MS  0x00000002



#endif /* EOF */

