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

Version:       1    18-Jul-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif


#include "smc_common_includes.h"

#include "smc.h"
#include "smc_trace.h"

#ifdef SMC_APE_RDTRACE_ENABLED

char* var_smc_init[1] = {"osTaskId"};
uint32_t var_size_smc_init[1] = {0xFFFF};

/* Trace group: SMC_TRACE_GROUP */
smc_rdtrace_t trace_smc[3] =
{
    { TRA_SMC_INIT, "initialize", "%s%s; %s:%d", &var_smc_init, &var_size_smc_init },
    { TRA_SMC_CHANNEL_IN_SYNC, "channel is synchronized with remote", "%s%s; ", NULL, NULL },
    { TRA_SMC_CHANNEL_VERSION_INFO, "channel version info", "%s%s; ", NULL, NULL }
};

char* var_smc_send[4] = {"channelId","msgPtr","msgLength","userdata"};
uint32_t var_size_smc_send[4] = {0xFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};
smc_rdtrace_t trace_smc_send[2] =
{
    { TRA_SMC_MESSAGE_SEND,     "Send",      "%s%s; %s:%d; %s:0x%08X; %s:%d; %s:0x%08X", &var_smc_send, &var_size_smc_send },
    { TRA_SMC_MESSAGE_SEND_END, "Send: end", "%s%s; %s:%d; %s:0x%08X; %s:%d; %s:0x%08X", &var_smc_send, &var_size_smc_send }
};


char* var_smc_receive[4]    = {"channelId","msgPtr","msgLength","userdata"};
char* var_smc_receive_cb[5] = {"channelId","msgPtr","translatedMsgPtr","msgLength", "userdata"};
char* var_smc_receive_conf_req[3]    = {"channelId","configurationId","configurationValue"};
char* var_smc_receive_conf_resp[3]    = {"channelId","configurationId","configurationResponse"};
uint32_t var_size_smc_receive[4] = {0xFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};
uint32_t var_size_smc_receive_cb[5] = {0xFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};

uint32_t var_size_smc_receive_conf[3] = {0xFF,0xFFFFFFFF,0xFFFFFFFF};


smc_rdtrace_t trace_smc_receive[3] =
{
    { TRA_SMC_MESSAGE_RECV,       "Recv",               "%s%s; %s:%d; %s:0x%08X; %s:%d; %s:0x%08X", &var_smc_receive, &var_size_smc_receive },
    { TRA_SMC_MESSAGE_RECV_TO_CB, "Recv: put callback", "%s%s; %s:%d; %s:0x%08X; %s:0x%08X; %s:%d; %s:0x%08X", &var_smc_receive_cb, &var_size_smc_receive_cb },
    { TRA_SMC_MESSAGE_RECV_END,   "Recv: end",          "%s%s; %s:%d; %s:0x%08X; %s:%d; %s:0x%08X", &var_smc_receive, &var_size_smc_receive },
    { TRA_SMC_CONFIG_REQ_RECV,    "Recv: configuration request",  "%s%s; %s:%d; %s:0x%08X; %s:0x%08X", &var_smc_receive_conf_req, &var_size_smc_receive_conf },
    { TRA_SMC_CONFIG_RESP_RECV,   "Recv: configuration response", "%s%s; %s:%d; %s:0x%08X; %s:0x%08X", &var_smc_receive_conf_resp, &var_size_smc_receive_conf }
};


char* var_smc_event[2] = {"channelId","eventId"};
uint32_t var_size_smc_event[2] = {0xFF,0xFFFFFFFF};
smc_rdtrace_t trace_smc_event[2] =
{
    { TRA_SMC_EVENT_SEND, "Event send", "%s%s; %s:%d; %s:0x%08X", &var_smc_event, &var_size_smc_event },
    { TRA_SMC_EVENT_RECV, "Event recv", "%s%s; %s:%d; %s:0x%08X", &var_smc_event, &var_size_smc_event }
};


char* var_smc_irq[2] = {"irqId","irqType"};
char* var_smc_signal[2] = {"signalPtr","signalId"};
uint32_t var_size_smc_irq[2] = {0xFFFFFFFF,0xFFFFFFFF};
smc_rdtrace_t trace_smc_irq[5] =
{
    { TRA_SMC_IRQ_START,         "IRQ:Start",        "%s%s; %s:%d; %s:0x%08X", &var_smc_irq,    &var_size_smc_irq },
    { TRA_SMC_IRQ_END,           "IRQ:End",          "%s%s; %s:%d; %s:0x%08X", &var_smc_irq,    &var_size_smc_irq },
    { TRA_SMC_SIGNAL_INTGEN,     "Signal:IntGen",    "%s%s; %s:0x%08X; %s:%d", &var_smc_signal, &var_size_smc_irq },
    { TRA_SMC_SIGNAL_INTGEN_OUT, "Signal:IntGenOut", "%s%s; %s:0x%08X; %s:%d", &var_smc_signal, &var_size_smc_irq },
    { TRA_SMC_SIGNAL_GOP001,     "Signal:GOP001",    "%s%s; %s:0x%08X; %s:%d", &var_smc_signal, &var_size_smc_irq }
};


char* var_smc_fifo_init[1] = {"fifoSize"};
uint32_t var_size_smc_fifo_init[1] = {0xFFFFFFFF};

char* var_smc_fifo_put[5]  = {"fifoPtr","data","length","flags","itemsInFifo"};
char* var_smc_fifo_get[5]  = {"fifoPtr","data","length","flags","itemsLeftInFifo"};
uint32_t var_size_fifo_put_get[5] = {0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};
char* var_smc_fifo_get_empty[5]  = {"fifoPtr","readIndex","writeIndex","readCount","writeCount"};

smc_rdtrace_t trace_smc_fifo[4] =
{
    { TRA_SMC_FIFO_INIT,      "initialize",     "%s%s; %s:%d;", &var_smc_fifo_init, &var_size_smc_fifo_init },
    { TRA_SMC_FIFO_PUT,       "fifo put",       "%s%s; %s:0x%08X; %s:0x%08X; %s:%d; %s:0x%08X; %s:%d", &var_smc_fifo_put, &var_size_fifo_put_get },
    { TRA_SMC_FIFO_GET,       "fifo get",       "%s%s; %s:0x%08X; %s:0x%08X; %s:%d; %s:0x%08X; %s:%d", &var_smc_fifo_get, &var_size_fifo_put_get },
    { TRA_SMC_FIFO_GET_EMPTY, "fifo get empty", "%s%s; %s:0x%08X; %s:%d; %s:%d; %s:%d; %s:%d", &var_smc_fifo_get_empty, &var_size_fifo_put_get }
};


char* var_smc_fifo_stat[4] = {"fifoPtr","writeCount","readCount","itemsInFifo"};
uint32_t var_size_smc_fifo_stat[4] = {0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF};

smc_rdtrace_t trace_smc_fifo_stat[2] =
{
    { TRA_SMC_FIFO_PUT_STATISTICS, "fifo put stat", "%s%s; %s:0x%08X; %s:%d; %s:%d; %s:%d", &var_smc_fifo_stat, &var_size_smc_fifo_stat },
    { TRA_SMC_FIFO_GET_STATISTICS, "fifo get stat", "%s%s; %s:0x%08X; %s:%d; %s:%d; %s:%d", &var_smc_fifo_stat, &var_size_smc_fifo_stat }
};


char* var_smc_ping[1] = {"channelId"};
char* var_smc_ping_req[2] = {"channelId", "replyVar"};
uint32_t var_size_smc_ping[1] = {0xFF};
uint32_t var_size_smc_ping_req[2] = {0xFF, 0xFFFFFFFF};

smc_rdtrace_t trace_smc_ping[4] =
{
    { TRA_SMC_PING_SEND_REQ,  "Ping send req",  "%s%s; %s:%d; %s:0x%08X;", &var_smc_ping_req, &var_size_smc_ping_req },
    { TRA_SMC_PING_RECV_REQ,  "Ping recv req",  "%s%s; %s:%d; %s:0x%08X;", &var_smc_ping_req, &var_size_smc_ping_req },
    { TRA_SMC_PING_SEND_RESP, "Ping send resp", "%s%s; %s:%d; %s:0x%08X;", &var_smc_ping_req, &var_size_smc_ping_req },
    { TRA_SMC_PING_RECV_RESP, "Ping recv resp", "%s%s; %s:%d; %s:0x%08X;", &var_smc_ping_req, &var_size_smc_ping_req }
};


char* var_smc_loopback[5] = {"channelId","dataPtr","dataLength","loopbackCnt","loopbackLeft"};
uint32_t var_size_smc_loopback[5] = {0xFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };

smc_rdtrace_t trace_smc_loopback[3] =
{
    { TRA_SMC_LOOPBACK_START,     "start",         "%s%s; %s:%d; %s:0x%08X; %s:%d; %s:%d; %s:%d;", &var_smc_loopback, &var_size_smc_loopback },
    { TRA_SMC_LOOPBACK_SEND_RESP, "send response", "%s%s; %s:%d; %s:0x%08X; %s:%d; %s:%d; %s:%d;", &var_smc_loopback, &var_size_smc_loopback },
    { TRA_SMC_LOOPBACK_SEND_REQ,  "send request",  "%s%s; %s:%d; %s:0x%08X; %s:%d; %s:%d; %s:%d;", &var_smc_loopback, &var_size_smc_loopback }
};


/*
 * Trace groups for SMC
 */
smc_rdtrace_group_t  trace_group_smc[SMC_TRACE_GROUP_COUNT] =
{
    {SMC_TRACE_GROUP,             "SMC:",           &trace_smc},
    {SMC_TRACE_GROUP_MSG_SEND,    "SMC:",           &trace_smc_send},
    {SMC_TRACE_GROUP_MSG_RECEIVE, "SMC:",           &trace_smc_receive},
    {SMC_TRACE_GROUP_EVENT,       "SMC:",           &trace_smc_event},
    {SMC_TRACE_GROUP_IRQ,         "SMC:",           &trace_smc_irq},
    {SMC_TRACE_GROUP_FIFO,        "SMC FIFO:",      &trace_smc_fifo},
    {SMC_TRACE_GROUP_FIFO_STAT,   "SMC FIFO STAT:", &trace_smc_fifo_stat},
    {SMC_TRACE_GROUP_LOCK,        "SMC:",           NULL},                  /* Not used in APE */
    {SMC_TRACE_GROUP_L2MUX,       "SMC:",           NULL},                  /* Not used in APE */
    {SMC_TRACE_GROUP_L2MUX_DL,    "SMC:",           NULL},                  /* Not used in APE */
    {SMC_TRACE_GROUP_L2MUX_UL,    "SMC:",           NULL},                  /* Not used in APE */
    {SMC_TRACE_GROUP_PING,        "SMC:",           &trace_smc_ping},
    {SMC_TRACE_GROUP_LOOPBACK,    "SMC LOOPBACK:",  &trace_smc_loopback}
};


    /* Trace group activation */
static uint32_t smc_trace_activation_group_0 = 0x00000000;

uint32_t get_smc_trace_activation_group(uint8_t bit_group_id)
{
    return smc_trace_activation_group_0;
}

uint32_t smc_rd_trace_group_activate(uint32_t trace_group_id, uint8_t activate)
{
    if( activate == 0x01 )
    {
            /* Activate traces */
        smc_trace_activation_group_0 |= trace_group_id;
    }
    else if( activate == 0x00 )
    {
            /* Deactivate traces */
        smc_trace_activation_group_0 &= ~trace_group_id;
    }
    else if( activate == 0x03 )
    {
            /* Deactivate traces */
        smc_trace_activation_group_0 = 0x00000000;
    }

    return smc_trace_activation_group_0;
}

void smc_rd_trace_send1(uint32_t trace_id, uint32_t* ptr1)
{
    smc_rdtrace_group_t group = trace_group_smc[SMC_TRACE_ID_TO_GROUP_INDEX(trace_id)];

    if( group.trace_item_array != NULL )
    {
        smc_rdtrace_t trace = group.trace_item_array[trace_id&SMC_TRACE_ID_MASK];

        SMC_RD_TRACE_PRINTK(trace.trace_var_format_str, group.trace_group_text, trace.trace_text,
               trace.trace_var_name_array[0], SMC_SHM_READ32(ptr1)&(trace.trace_var_size_array[0]) );

    }
}

void smc_rd_trace_send2(uint32_t trace_id, uint32_t* ptr1, uint32_t* ptr2)
{
    smc_rdtrace_group_t group = trace_group_smc[SMC_TRACE_ID_TO_GROUP_INDEX(trace_id)];

    if( group.trace_item_array != NULL )
    {
        smc_rdtrace_t trace = group.trace_item_array[trace_id&SMC_TRACE_ID_MASK];

        SMC_RD_TRACE_PRINTK(trace.trace_var_format_str, group.trace_group_text, trace.trace_text,
               trace.trace_var_name_array[0], SMC_SHM_READ32(ptr1)&(trace.trace_var_size_array[0]),
               trace.trace_var_name_array[1], SMC_SHM_READ32(ptr2)&(trace.trace_var_size_array[1]));
    }
}

void smc_rd_trace_send3(uint32_t trace_id, uint32_t* ptr1, uint32_t* ptr2, uint32_t* ptr3)
{
    smc_rdtrace_group_t group = trace_group_smc[SMC_TRACE_ID_TO_GROUP_INDEX(trace_id)];

    if( group.trace_item_array != NULL )
    {
        smc_rdtrace_t trace = group.trace_item_array[trace_id&SMC_TRACE_ID_MASK];

        SMC_RD_TRACE_PRINTK(trace.trace_var_format_str, group.trace_group_text, trace.trace_text,
               trace.trace_var_name_array[0], SMC_SHM_READ32(ptr1)&(trace.trace_var_size_array[0]),
               trace.trace_var_name_array[1], SMC_SHM_READ32(ptr2)&(trace.trace_var_size_array[1]),
               trace.trace_var_name_array[2], SMC_SHM_READ32(ptr3)&(trace.trace_var_size_array[2]));
    }
}

void smc_rd_trace_send4(uint32_t trace_id, uint32_t* ptr1, uint32_t* ptr2, uint32_t* ptr3, uint32_t* ptr4)
{
    smc_rdtrace_group_t group = trace_group_smc[SMC_TRACE_ID_TO_GROUP_INDEX(trace_id)];

    if( group.trace_item_array != NULL )
    {
        smc_rdtrace_t trace = group.trace_item_array[trace_id&SMC_TRACE_ID_MASK];

        SMC_RD_TRACE_PRINTK(trace.trace_var_format_str, group.trace_group_text, trace.trace_text,
               trace.trace_var_name_array[0], SMC_SHM_READ32(ptr1)&(trace.trace_var_size_array[0]),
               trace.trace_var_name_array[1], SMC_SHM_READ32(ptr2)&(trace.trace_var_size_array[1]),
               trace.trace_var_name_array[2], SMC_SHM_READ32(ptr3)&(trace.trace_var_size_array[2]),
               trace.trace_var_name_array[3], SMC_SHM_READ32(ptr4)&(trace.trace_var_size_array[3]));

    }
}

void smc_rd_trace_send5(uint32_t trace_id, uint32_t* ptr1, uint32_t* ptr2, uint32_t* ptr3, uint32_t* ptr4, uint32_t* ptr5)
{
            smc_rdtrace_group_t group = trace_group_smc[SMC_TRACE_ID_TO_GROUP_INDEX(trace_id)];

            if( group.trace_item_array != NULL )
            {
                smc_rdtrace_t trace = group.trace_item_array[trace_id&SMC_TRACE_ID_MASK];

                SMC_RD_TRACE_PRINTK(trace.trace_var_format_str, group.trace_group_text, trace.trace_text,
                       trace.trace_var_name_array[0], SMC_SHM_READ32(ptr1)&(trace.trace_var_size_array[0]),
                       trace.trace_var_name_array[1], SMC_SHM_READ32(ptr2)&(trace.trace_var_size_array[1]),
                       trace.trace_var_name_array[2], SMC_SHM_READ32(ptr3)&(trace.trace_var_size_array[2]),
                       trace.trace_var_name_array[3], SMC_SHM_READ32(ptr4)&(trace.trace_var_size_array[3]),
                       trace.trace_var_name_array[4], SMC_SHM_READ32(ptr5)&(trace.trace_var_size_array[4]));
            }
}


#endif
