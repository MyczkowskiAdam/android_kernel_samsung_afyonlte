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

Version:       1    23-Aug-2012     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMC_LINUX_IOCTL_H
#define SMC_LINUX_IOCTL_H


/*
 * Net device driver interface request structure for commands
 * Do not include when SMC application uses this header.
 */

#ifndef SMC_TEST_APP_COMPILATION

#include "linux/if.h"

#endif

/*
 * Private IO Control commands
 * #define SIOCDEVPRIVATE   0x89F0 to 89FF (Max +15)
 * #define SIOCPROTOPRIVATE 0x89E0 to 89EF
 */
/* #define SIOCPNGETOBJECT     (SIOCPROTOPRIVATE + 0) */

/* SIOCDEVPRIVATE = 0x89F0 */

#define SIOCDEV_SEND_DATA     (SIOCDEVPRIVATE + 0x04)
#define SIOCDEV_RUN_TEST      (SIOCDEVPRIVATE + 0x05)       /* Run SMC tests (valid only when test module is built in)*/
#define SIOCDEV_STATUS        (SIOCDEVPRIVATE + 0x06)       /* Prints the status of the specified device (val 0x89F6) to kernel log */
#define SIOCDEV_TRACE         (SIOCDEVPRIVATE + 0x07)       /* Activate / Deactivate runtime traces in the SMC */
#define SIOCDEV_HISTORY       (SIOCDEVPRIVATE + 0x08)       /* Retrieve the SMC history data */
#define SIOCDEV_INFO          (SIOCDEVPRIVATE + 0x09)
#define SIOCDEV_MSG_INTERNAL  (SIOCDEVPRIVATE + 0x0A)
#define SIOCDEV_MSG_LOOPBACK  (SIOCDEVPRIVATE + 0x0B)       /* Runs loopback test */
#define SIOCDEV_STATUS_SMC    (SIOCDEVPRIVATE + 0x0C)       /* Returns SMC status to IOCTL client */

/*
 * Internal message return values
 */

#define MSG_RESP_OK      0x01
#define MSG_RESP_FAIL    0x00

#define SMC_VERSION_SELECTION_LOCAL   0x00
#define SMC_VERSION_SELECTION_REMOTE  0x01

struct ifreq_smc
{
    union
    {
      char ifrn_name[IFNAMSIZ];        /* if name, e.g. "smc0" */
    } ifr_ifrn;

    uint32_t if_data_len;
    uint8_t* if_data;
};

struct ifreq_smc_info
{
    union
    {
      char ifrn_name[IFNAMSIZ];
    } ifr_ifrn;

    uint32_t smc_version_selection;
    char smc_version[16];
};

struct ifreq_smc_test
{
    union
    {
      char ifrn_name[IFNAMSIZ];
    } ifr_ifrn;

    uint32_t if_test_case;
    uint32_t if_test_data_len;
    uint8_t* if_test_data;
    uint32_t if_test_result;
};

struct ifreq_smc_trace
{
    union
    {
      char ifrn_name[IFNAMSIZ];
    } ifr_ifrn;

    uint32_t if_trace_group_id;
    uint8_t  if_trace_group_activate;
    uint32_t if_traces_activated;
};

struct ifreq_smc_history_item
{
    uint8_t channel_id;
    uint8_t history_item_type;
    uint8_t fill2;
    uint8_t fill1;

    uint32_t length;
};

struct ifreq_smc_history
{
    union
    {
      char ifrn_name[IFNAMSIZ];
    } ifr_ifrn;

    uint32_t                        requested_history_item_count;
    uint32_t                        history_item_count;
    struct ifreq_smc_history_item*  history_item_array;
};

struct ifreq_smc_msg_internal
{
    union
    {
      char ifrn_name[IFNAMSIZ];
    } ifr_ifrn;

    uint32_t if_channel_id;
    uint32_t if_msg_id;
    uint32_t if_msg_parameter;
    uint32_t if_msg_response;
    uint32_t if_msg_data_len;
    uint8_t* if_msg_data;

};

struct ifreq_smc_loopback
{
    union
    {
      char ifrn_name[IFNAMSIZ];
    } ifr_ifrn;

    uint32_t  if_channel_id;
    uint32_t  if_loopback_payload_length;
    uint32_t  if_loopback_rounds;
};

struct ifreq_smc_status
{
    union
    {
      char ifrn_name[IFNAMSIZ];
    } ifr_ifrn;

    uint32_t  state;
    uint32_t  if_loopback_payload_length;
    uint32_t  if_loopback_rounds;
};


#endif
