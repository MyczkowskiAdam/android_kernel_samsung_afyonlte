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

Version:       1    18-Jul-2012     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMC_TRACE_RDTRACE_H
#define SMC_TRACE_RDTRACE_H


    /* Single trace line */
typedef struct _smc_rdtrace_t
{
    uint32_t           trace_id;
    char*              trace_text;
    char*              trace_var_format_str;
    char**             trace_var_name_array;
    uint32_t*          trace_var_size_array;

} smc_rdtrace_t;

typedef struct _smc_rdtrace_group_t
{
    uint32_t          trace_group_id;
    char*             trace_group_text;
    smc_rdtrace_t*    trace_item_array;


} smc_rdtrace_group_t;

#define SMC_TRACE_GROUP_MASK          0xF0000000
#define SMC_TRACE_ID_MASK             0x0FFFFFFF

    /* Trace groups available */
#define SMC_TRACE_GROUP               0x00000001    /*  0 */
#define SMC_TRACE_GROUP_MSG_SEND      0x00000002    /*  1 */
#define SMC_TRACE_GROUP_MSG_RECEIVE   0x00000004    /*  2 */
#define SMC_TRACE_GROUP_EVENT         0x00000008    /*  3 */
#define SMC_TRACE_GROUP_IRQ           0x00000010    /*  4 */
#define SMC_TRACE_GROUP_FIFO          0x00000020    /*  5 */
#define SMC_TRACE_GROUP_FIFO_STAT     0x00000040    /*  6 */
#define SMC_TRACE_GROUP_LOCK          0x00000080    /*  7 */
#define SMC_TRACE_GROUP_L2MUX         0x00000100    /*  8 */
#define SMC_TRACE_GROUP_L2MUX_DL      0x00000200    /*  9 0x9 */
#define SMC_TRACE_GROUP_L2MUX_UL      0x00000400    /* 10 0xA */
#define SMC_TRACE_GROUP_PING          0x00000800    /* 11 0xB */
#define SMC_TRACE_GROUP_LOOPBACK      0x00001000    /* 12 0xC */
#define SMC_TRACE_GROUP_COUNT         13            /* <== Remember to update when adding new groups */

/* Trace IDs (same as used in modem) */

#define TRA_SMC_INIT                  0x00000000  /* Group 0 (MSB) in group index 0 */
#define TRA_SMC_CHANNEL_IN_SYNC       0x00000001
#define TRA_SMC_CHANNEL_VERSION_INFO  0x00000002

#define TRA_SMC_MESSAGE_SEND          0x10000000
#define TRA_SMC_MESSAGE_SEND_END      0x10000001

#define TRA_SMC_MESSAGE_RECV          0x20000000
#define TRA_SMC_MESSAGE_RECV_TO_CB    0x20000001
#define TRA_SMC_MESSAGE_RECV_END      0x20000002
#define TRA_SMC_CONFIG_REQ_RECV       0x20000003
#define TRA_SMC_CONFIG_RESP_RECV      0x20000004

#define TRA_SMC_EVENT_SEND            0x30000000
#define TRA_SMC_EVENT_RECV            0x30000001

#define TRA_SMC_IRQ_START             0x40000000
#define TRA_SMC_IRQ_END               0x40000001
#define TRA_SMC_SIGNAL_INTGEN         0x40000002
#define TRA_SMC_SIGNAL_INTGEN_OUT     0x40000003
#define TRA_SMC_SIGNAL_GOP001         0x40000004
#define TRA_SMC_SIGNAL_WAKEUP         0x40000005
#define TRA_SMC_SIGNAL_WAKEUP_CLEAR   0x40000006

#define TRA_SMC_FIFO_INIT             0x50000000
#define TRA_SMC_FIFO_PUT              0x50000001
#define TRA_SMC_FIFO_GET              0x50000002
#define TRA_SMC_FIFO_GET_EMPTY        0x50000003

#define TRA_SMC_FIFO_PUT_STATISTICS   0x60000000
#define TRA_SMC_FIFO_GET_STATISTICS   0x60000001

// L2MUX traces not in APE

#define TRA_SMC_PING_SEND_REQ         0xB0000000
#define TRA_SMC_PING_RECV_REQ         0xB0000001
#define TRA_SMC_PING_SEND_RESP        0xB0000002
#define TRA_SMC_PING_RECV_RESP        0xB0000003


#define TRA_SMC_LOOPBACK_START        0xC0000000
#define TRA_SMC_LOOPBACK_SEND_RESP    0xC0000001
#define TRA_SMC_LOOPBACK_SEND_REQ     0xC0000002


// 0x00000000 + (0x01<<group_id)

#define SMC_TRACE_ID_TO_GROUP_INDEX( trace_id )          ( ((uint8_t)(trace_id>>28))&0xF )
#define SMC_TRACE_ID_TO_GROUP_ID( trace_id )             (  0x00000000 + ( 0x01 << SMC_TRACE_ID_TO_GROUP_INDEX(trace_id) ) )
#define SMC_TRACE_GROUP_IS_ACTIVATED( trace_group_id )   ( (get_smc_trace_activation_group(0)&trace_group_id)==trace_group_id )


#define RD_TRACE_SEND1(trace_id, sz1, ptr1)              if( SMC_TRACE_GROUP_IS_ACTIVATED( SMC_TRACE_ID_TO_GROUP_ID(trace_id)) ) { smc_rd_trace_send1(trace_id, ptr1); }
#define RD_TRACE_SEND2(trace_id, sz1, ptr1, sz2, ptr2)   if( SMC_TRACE_GROUP_IS_ACTIVATED( SMC_TRACE_ID_TO_GROUP_ID(trace_id)) ) { smc_rd_trace_send2(trace_id, ptr1, ptr2); }
#define RD_TRACE_SEND3(trace_id, sz1, ptr1, sz2, ptr2, sz3, ptr3)  if( SMC_TRACE_GROUP_IS_ACTIVATED( SMC_TRACE_ID_TO_GROUP_ID(trace_id)) ) { smc_rd_trace_send3(trace_id, ptr1, ptr2, ptr3); }
#define RD_TRACE_SEND4(trace_id, sz1, ptr1, sz2, ptr2, sz3, ptr3, sz4, ptr4)   if( SMC_TRACE_GROUP_IS_ACTIVATED( SMC_TRACE_ID_TO_GROUP_ID( trace_id )) ) { smc_rd_trace_send4(trace_id, ptr1, ptr2, ptr3, ptr4); }
#define RD_TRACE_SEND5(trace_id, sz1, ptr1, sz2, ptr2, sz3, ptr3, sz4, ptr4, sz5, ptr5) if( SMC_TRACE_GROUP_IS_ACTIVATED( SMC_TRACE_ID_TO_GROUP_ID( trace_id )) ) { smc_rd_trace_send5(trace_id, ptr1, ptr2, ptr3, ptr4, ptr5); }

uint32_t get_smc_trace_activation_group(uint8_t bit_group_id);
uint32_t smc_rd_trace_group_activate(uint32_t trace_group_id, uint8_t activate);

void smc_rd_trace_send1(uint32_t trace_id, uint32_t* ptr1);
void smc_rd_trace_send2(uint32_t trace_id, uint32_t* ptr1, uint32_t* ptr2);
void smc_rd_trace_send3(uint32_t trace_id, uint32_t* ptr1, uint32_t* ptr2, uint32_t* ptr3);
void smc_rd_trace_send4(uint32_t trace_id, uint32_t* ptr1, uint32_t* ptr2, uint32_t* ptr3, uint32_t* ptr4);
void smc_rd_trace_send5(uint32_t trace_id, uint32_t* ptr1, uint32_t* ptr2, uint32_t* ptr3, uint32_t* ptr4, uint32_t* ptr5);

#ifdef SMC_APE_LINUX_KERNEL_STM
  #define SMC_RD_TRACE_PRINTK smc_printk
#else
  #define SMC_RD_TRACE_PRINTK printk
#endif

#endif
/* EOF */

