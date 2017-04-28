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

Version:       31   24-May-2013     Heikki Siikaluoma
Status:        draft
Description :  TX Wakelock

Version:       14   07-Jun-2012     Heikki Siikaluoma
Status:        draft
Description :  Resource IRQ handler changed: First clears the interrupt then handles the request

Version:       8    29-Feb-2012     Heikki Siikaluoma
Status:        draft
Description :  Linux Kernel SMC timer functions added

Version:       4    19-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  Linux Kernel module features implemented

Version:       1    17-Nov-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif


#include "smc_common_includes.h"

#include "smc.h"
#include "smc_trace.h"
#include "smc_linux.h"
#include <linux/random.h>
#include <linux/wakelock.h>
#include <asm/io.h>
#include <mach/r8a7373.h>
#ifdef SMC_DMA_ENABLED
  #include <linux/dma-mapping.h>
#endif


    /*
     * HW semaphore address get
     */
#include <linux/hwspinlock.h>
#include <linux/platform_data/rmobile_hwsem.h>

    /* TODO make better implementation */
#include "../hwspinlock/hwspinlock_internal.h"

struct hwspinlock_private
{
    void __iomem        *sm_base;
    void __iomem        *ext_base;
};

static irqreturn_t smc_linux_interrupt_handler_intcbb(int irq, void *dev_id );          /* INTC-BB interrupt handler */
static irqreturn_t smc_linux_interrupt_handler_int_genout(int irq, void *dev_id );      /* GENIO interrupt handler */
static irqreturn_t smc_linux_interrupt_handler_int_resource(int irq, void *dev_id );    /* SPI resource interrupt handler */

#ifdef SMC_APE_USE_THREADED_IRQ
  static irqreturn_t smc_linux_interrupt_handler_int_resource_threaded(int irq, void *dev_id );
#endif

static smc_lock_t* g_local_lock_sleep_control = NULL;

//static struct wake_lock* wakelock    = NULL;

static inline smc_lock_t* get_local_lock_sleep_control(void)
{
    if( g_local_lock_sleep_control == NULL ) g_local_lock_sleep_control = smc_lock_create();
    return g_local_lock_sleep_control;
}

/*
static inline struct wake_lock* get_wake_lock(void)
{
    if( wakelock==NULL )
    {
        SMC_TRACE_PRINTF_DEBUG("get_wake_lock: initialize...");
        wakelock = (struct wake_lock*)SMC_MALLOC_IRQ( sizeof( struct wake_lock ) );

        wake_lock_init(wakelock, WAKE_LOCK_SUSPEND, "smc_wakelock");
     }

    return wakelock;
}
*/

static smc_lock_t* g_local_lock_signal_intgen_set      = NULL;

static inline smc_lock_t* get_local_lock_signal_intgen_set(void)
{
    if( g_local_lock_signal_intgen_set == NULL ) g_local_lock_signal_intgen_set = smc_lock_create();
    return g_local_lock_signal_intgen_set;
}

static smc_lock_t* g_local_lock_signal_intgen_clear      = NULL;

static inline smc_lock_t* get_local_lock_signal_intgen_clear(void)
{
    if( g_local_lock_signal_intgen_clear == NULL ) g_local_lock_signal_intgen_clear = smc_lock_create();
    return g_local_lock_signal_intgen_clear;
}


#ifdef SMC_APE_USE_TASKLET_IN_IRQ

    /* ==========================
     * Tasklet function declarations
     */
    struct tasklet_struct* smc_create_tasklet(smc_signal_handler_t* signal_handler, void (*func)(unsigned long) );

    void smc_signal_handler_tasklet_mhdp(unsigned long data);    /* Tasklet for channel delivering MHDP protocol */

    void smc_do_tasklet_test(unsigned long unused);

    DECLARE_TASKLET(smc_tasklet_test, smc_do_tasklet_test, 0);
#endif

/* =============================================================
 * SMC Interrupt platform specific implementations
 */

#ifdef SMC_WAKEUP_USE_EXTERNAL_IRQ_APE

void smc_init_external_wakeup_irqc( smc_t* smc_instance );

static irqreturn_t smc_linux_interrupt_handler_int_genout_wakeup(int irq, void *dev_id );

static irqreturn_t smc_linux_interrupt_handler_int_genout_wakeup(int irq, void *dev_id )
{
    smc_signal_handler_t* signal_handler = NULL;
    uint32_t              signal_type    = SMC_SIGNAL_TYPE_INT_WGM_GENOUT;

#ifdef SMC_APE_WAKEUP_WAKELOCK_USE
    //wake_lock( get_wake_lock() );
    SMC_TRACE_PRINTF_SIGNAL("smc_linux_interrupt_handler_int_genout_wakeup: IRQ: 0x%02X (%d), Device 0x%08X (use wakelock)", (uint32_t)irq, irq, (uint32_t)dev_id);
#else
    SMC_TRACE_PRINTF_SIGNAL("smc_linux_interrupt_handler_int_genout_wakeup: IRQ: 0x%02X (%d), Device 0x%08X (no wakelock)", (uint32_t)irq, irq, (uint32_t)dev_id);
#endif


    RD_TRACE_SEND2(TRA_SMC_IRQ_START, 4, &irq,
                                      4, &signal_type );

    /* Nothing to do here currently */

    RD_TRACE_SEND2(TRA_SMC_IRQ_END, 4, &irq,
                                    4, &signal_type );

#ifdef SMC_APE_WAKEUP_WAKELOCK_USE
    //wake_unlock( get_wake_lock() );
#endif

    return IRQ_HANDLED;
}

void smc_register_wakeup_irq( smc_t* smc_instance, uint32_t signal_id, uint32_t signal_type)
{
#if(SMC_APE_WAKEUP_EXTERNAL_IRQ_REGISTER_HANDLER==TRUE)

    int           result       = 0;
    const char*   device_name  = NULL;
    void*         dev_id       = NULL;
    unsigned long flags        = 0x00;
    int           irq_offset   = SMC_APE_IRQ_OFFSET_INTCSYS_SPI;

    struct resource*        res             = NULL;
    struct platform_device* platform_device = NULL;

    smc_signal_t* signal = smc_signal_create( signal_id, signal_type );

    smc_init_external_wakeup_irqc( smc_instance );

    if( signal_type == SMC_SIGNAL_TYPE_INT_IRQC )
    {
        irq_offset = SMC_APE_IRQ_OFFSET_IRQ_SPI;
    }

    SMC_TRACE_PRINTF_SIGNAL("smc_register_wakeup_irq: signal: 0x%08X", (uint32_t)signal);

    if( smc_instance->smc_parent_ptr != NULL )
    {
        SMC_TRACE_PRINTF_SIGNAL("smc_register_wakeup_irq: parent object 0x%08X found, extract the platform device", smc_instance->smc_parent_ptr);
        platform_device = (struct platform_device*)smc_instance->smc_parent_ptr;
    }

    if( platform_device )
    {
        int     irq_spi = (int)signal->interrupt_id;
        int     res_id  = 0;
        uint8_t found   = FALSE;

        while( (res = platform_get_resource(platform_device, IORESOURCE_IRQ, res_id)) )
        {
            SMC_TRACE_PRINTF_SIGNAL("smc_register_wakeup_irq: check res id %d where IRQ ID %d...", res_id, res->start);

            if( (res->start-irq_offset) == irq_spi )
            {
                SMC_TRACE_PRINTF_SIGNAL("smc_register_wakeup_irq: res id %d: IRQ ID %d match to SPI %d...", res_id, res->start,irq_spi);
                found = TRUE;
                break;
            }

            res_id++;
        }

        if( found )
        {
            SMC_TRACE_PRINTF_SIGNAL("smc_register_wakeup_irq: signal: 0x%08X: found res 0x%08X, IRQ start is %d",
                    (uint32_t)signal, res, res->start );

            flags = IRQF_DISABLED;

            device_name  = dev_name(&platform_device->dev);
            dev_id    = platform_device;

            result = request_irq( res->start, smc_linux_interrupt_handler_int_genout_wakeup, flags, device_name, dev_id );

            if( result )
            {
                SMC_TRACE_PRINTF_SIGNAL("smc_register_wakeup_irq: signal: 0x%08X: request_irq FAILED, result %d",
                        (uint32_t)signal, result);
            }
            else
            {
                SMC_TRACE_PRINTF_SIGNAL("smc_register_wakeup_irq: signal: 0x%08X: request_irq SUCCESS", (uint32_t)signal);
            }

            enable_irq_wake(res->start);
        }
        else
        {
            SMC_TRACE_PRINTF_SIGNAL("smc_register_wakeup_irq: signal: SMC_SIGNAL_TYPE_INT_WGM_GENOUT: NO Resource found");
        }
    }
    else
    {
        SMC_TRACE_PRINTF_SIGNAL("smc_register_wakeup_irq: signal: SMC_SIGNAL_TYPE_INT_WGM_GENOUT: NO Platform device");
    }
#else

    smc_init_external_wakeup_irqc( smc_instance );

#endif
}

void smc_init_external_wakeup_irqc( smc_t* smc_instance )
{
    #define SMC_PRI_STS0         IO_ADDRESS(0xE61C2410)     /* Status i Event Generator block 2 */

    #define SMC_PRI_SET0         IO_ADDRESS(0xE61C2414)     /* Event Generator block 2 */
    #define SMC_PRI_SET0_VALUE   0x00000001

    #define SMC_CONFIG0          IO_ADDRESS(0xE61C1980)     /**/
    #define SMC_CONFIG0_VALUE    0x00000008     /* Rising edge*/

    #define SMC_WAKEN_STS0       IO_ADDRESS(0xE61C1884)
    #define SMC_WAKEN_SET0       IO_ADDRESS(0xE61C1888)
    #define SMC_WAKEN_SET0_VALUE 0x00000001

    #define SMC_WUPMMSK          IO_ADDRESS(0xE6180030)

    SMC_TRACE_PRINTF_SIGNAL("smc_init_external_wakeup_irqc: initialize...");

    uint32_t reg_val  = SMC_SHM_READ32(SMC_PRI_STS0);
    uint32_t new_val = reg_val | SMC_PRI_SET0_VALUE;

    SMC_TRACE_PRINTF_SIGNAL("smc_init_external_wakeup_irqc: SMC_PRI_SET0 = 0x%08X, write 0x%08X", reg_val, new_val);

    SMC_SHM_WRITE32(SMC_PRI_SET0, new_val);
    SMC_SHM_READ32(SMC_PRI_SET0);
    reg_val = SMC_SHM_READ32(SMC_PRI_STS0);

    SMC_TRACE_PRINTF_SIGNAL("smc_init_external_wakeup_irqc: SMC_PRI_SET0 is now 0x%08X", reg_val);

    // -------

    reg_val  = SMC_SHM_READ32(SMC_CONFIG0);
    new_val = reg_val | SMC_CONFIG0_VALUE;

    SMC_TRACE_PRINTF_SIGNAL("smc_init_external_wakeup_irqc: SMC_CONFIG0 = 0x%08X, write 0x%08X", reg_val, new_val);

    SMC_SHM_WRITE32(SMC_CONFIG0, new_val);
    reg_val = SMC_SHM_READ32(SMC_CONFIG0);

    SMC_TRACE_PRINTF_SIGNAL("smc_init_external_wakeup_irqc: SMC_CONFIG0 is now 0x%08X", reg_val);

    // -------

    reg_val  = SMC_SHM_READ32(SMC_WAKEN_STS0);
    new_val = reg_val | SMC_WAKEN_SET0_VALUE;

    SMC_TRACE_PRINTF_SIGNAL("smc_init_external_wakeup_irqc: SMC_WAKEN_SET0 = 0x%08X, write 0x%08X", reg_val, new_val);

    SMC_SHM_WRITE32(SMC_WAKEN_SET0, new_val);
    SMC_SHM_READ32(SMC_WAKEN_SET0);
    reg_val = SMC_SHM_READ32(SMC_WAKEN_STS0);

    // -------

    reg_val = SMC_SHM_READ32(SMC_WUPMMSK);
    SMC_TRACE_PRINTF_SIGNAL("smc_init_external_wakeup_irqc: SMC_WUPMMSK is 0x%08X", reg_val);

    SMC_TRACE_PRINTF_SIGNAL("smc_init_external_wakeup_irqc: completed");

}

#endif


/*
 * Interrupt handler
 */
static irqreturn_t smc_linux_interrupt_handler_intcbb(int irq, void *dev_id )
{
    smc_signal_handler_t* signal_handler = NULL;
    smc_channel_t*        smc_channel    = NULL;
    uint32_t              signal_type    = SMC_SIGNAL_TYPE_INTCBB;

    signal_type = signal_type;  /* Suppress warning */

    RD_TRACE_SEND2(TRA_SMC_IRQ_START, 4, &irq,
                                      4, &signal_type );

        /* Get the appropriate signal handler from array */
    signal_handler = smc_signal_handler_get( (uint32_t)irq, SMC_SIGNAL_TYPE_INTCBB );

    assert( signal_handler != NULL );

    smc_channel = signal_handler->smc_channel;

    if( smc_channel != NULL )
    {
        if( smc_channel->smc_rx_wakelock!=NULL &&
            ((smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_TIMER)   == SMC_CHANNEL_WAKELOCK_TIMER ||
            (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_MESSAGE) == SMC_CHANNEL_WAKELOCK_MESSAGE ))
        {
            wake_lock( (struct wake_lock*)smc_channel->smc_rx_wakelock );

            //wake_lock( get_wake_lock() );

            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_intcbb: IRQ: 0x%02X (%d), Device 0x%08X (use wakelock)", (uint32_t)irq, irq, (uint32_t)dev_id);
        }
        else
        {
            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_intcbb: IRQ: 0x%02X (%d), Device 0x%08X (no wakelock)", (uint32_t)irq, irq, (uint32_t)dev_id);
        }

        smc_channel_interrupt_handler( smc_channel );

        SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_intcbb: IRQ: 0x%02X (%d) handled", (uint32_t)irq, irq);
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("smc_linux_interrupt_handler_intcbb: No channel initialized for IRQ: 0x%02X (%d)", (uint32_t)irq, irq);
    }

    RD_TRACE_SEND2(TRA_SMC_IRQ_END, 4, &irq,
                                    4, &signal_type );

    if( smc_channel != NULL )
    {
        if( smc_channel->smc_rx_wakelock != NULL &&
            (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_TIMER) == SMC_CHANNEL_WAKELOCK_TIMER )
        {
            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_intcbb: IRQ: 0x%02X (%d), Device 0x%08X wake_lock_timeout (wakelock timeout %d ms)", (uint32_t)irq, irq, (uint32_t)dev_id, smc_channel->wakelock_timeout_ms);

            if( smc_channel->wakelock_timeout_ms > 0 )
            {
                wake_lock_timeout( (struct wake_lock*)smc_channel->smc_rx_wakelock, msecs_to_jiffies(smc_channel->wakelock_timeout_ms) );
            }
            else
            {
                wake_unlock( (struct wake_lock*)smc_channel->smc_rx_wakelock );
            }
        }
        else if( smc_channel->smc_rx_wakelock != NULL && (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_MESSAGE) == SMC_CHANNEL_WAKELOCK_MESSAGE )
        {
            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_intcbb: IRQ: 0x%02X (%d), Device 0x%08X wake_unlock", (uint32_t)irq, irq, (uint32_t)dev_id);
            wake_unlock( (struct wake_lock*)smc_channel->smc_rx_wakelock );
            //wake_unlock( get_wake_lock() );
        }
    }

    return IRQ_HANDLED;
}

static irqreturn_t smc_linux_interrupt_handler_int_genout(int irq, void *dev_id )
{
    smc_signal_handler_t* signal_handler = NULL;
    smc_channel_t*        smc_channel    = NULL;
    uint32_t              signal_type = SMC_SIGNAL_TYPE_INT_WGM_GENOUT;

    signal_type = signal_type;  /* Suppress warning */

    RD_TRACE_SEND2(TRA_SMC_IRQ_START, 4, &irq,
                                      4, &signal_type );

        /* Get the appropriate signal from array */
    signal_handler = smc_signal_handler_get( (uint32_t)irq, SMC_SIGNAL_TYPE_INT_WGM_GENOUT );

    assert( signal_handler != NULL );

    smc_channel = signal_handler->smc_channel;

    if( smc_channel != NULL )
    {
        if( smc_channel->smc_rx_wakelock != NULL &&
            ((smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_TIMER)  == SMC_CHANNEL_WAKELOCK_TIMER ||
            (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_MESSAGE) == SMC_CHANNEL_WAKELOCK_MESSAGE ))
        {
            wake_lock( (struct wake_lock*)smc_channel->smc_rx_wakelock );
            //wake_lock( get_wake_lock() );

            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_genout: IRQ: 0x%02X (%d), Device 0x%08X (use wakelock)", (uint32_t)irq, irq, (uint32_t)dev_id);
        }
        else
        {
            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_genout: IRQ: 0x%02X (%d), Device 0x%08X (no wakelock)", (uint32_t)irq, irq, (uint32_t)dev_id);
        }

        smc_channel_interrupt_handler( smc_channel );

        SMC_TRACE_PRINTF_SIGNAL("smc_linux_interrupt_handler_int_genout: IRQ: 0x%02X (%d) handled", (uint32_t)irq, irq);
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("smc_linux_interrupt_handler_int_genout: No channel initialized for IRQ: 0x%02X (%d)", (uint32_t)irq, irq);
    }

    RD_TRACE_SEND2(TRA_SMC_IRQ_END, 4, &irq,
                                    4, &signal_type );

    if( smc_channel != NULL )
    {
        if( smc_channel->smc_rx_wakelock != NULL &&
           (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_TIMER) == SMC_CHANNEL_WAKELOCK_TIMER )
        {
            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_genout: IRQ: 0x%02X (%d), Device 0x%08X wake_lock_timeout (wakelock timeout %d ms)", (uint32_t)irq, irq, (uint32_t)dev_id, smc_channel->wakelock_timeout_ms);

            if( smc_channel->wakelock_timeout_ms > 0 )
            {
                wake_lock_timeout( (struct wake_lock*)smc_channel->smc_rx_wakelock, msecs_to_jiffies(smc_channel->wakelock_timeout_ms) );
                //wake_lock_timeout( get_wake_lock(), msecs_to_jiffies(smc_channel->wakelock_timeout_ms) );
            }
            else
            {
                wake_unlock( (struct wake_lock*)smc_channel->smc_rx_wakelock );
                //wake_unlock( get_wake_lock() );
            }
        }
        else if( smc_channel->smc_rx_wakelock != NULL && (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_MESSAGE) == SMC_CHANNEL_WAKELOCK_MESSAGE )
        {
            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_genout: IRQ: 0x%02X (%d), Device 0x%08X wake_unlock", (uint32_t)irq, irq, (uint32_t)dev_id );
            wake_unlock( (struct wake_lock*)smc_channel->smc_rx_wakelock );
        }
    }

    return IRQ_HANDLED;
}


static inline irqreturn_t smc_linux_interrupt_handler_int_resource_exec(smc_signal_handler_t* signal_handler)
{
    if( signal_handler->smc_channel != NULL )
    {
        smc_channel_interrupt_handler( signal_handler->smc_channel );

        SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_resource_exec: signal handler 0x%08X executed", (uint32_t)signal_handler);
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("smc_linux_interrupt_handler_int_resource_exec: No channel initialized for signal handler 0x%08X", (uint32_t)signal_handler);
    }

    return IRQ_HANDLED;
}


#ifdef SMC_APE_USE_THREADED_IRQ

/**
 * Threaded IRQ handler.
 *
 */
static irqreturn_t smc_linux_interrupt_handler_int_resource_threaded(int irq, void *dev_id )
{
    smc_signal_handler_t* signal_handler = NULL;
    uint32_t              signal_type    = SMC_SIGNAL_TYPE_INT_RESOURCE;
    int                   irq_spi        = irq-SMC_APE_IRQ_OFFSET_INTCSYS_SPI;

    signal_handler = smc_signal_handler_get( (uint32_t)irq_spi, signal_type );

    if( signal_handler != NULL && signal_handler->smc_channel != NULL )
    {
        smc_channel_t* smc_channel = signal_handler->smc_channel;

        smc_channel_interrupt_handler( smc_channel );

        SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_resource_threaded: signal handler 0x%08X executed", (uint32_t)signal_handler);

        if( smc_channel->smc_rx_wakelock != NULL &&
            (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_TIMER) == SMC_CHANNEL_WAKELOCK_TIMER )
        {
            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_resource_threaded: IRQ: 0x%02X (%d), Device 0x%08X wake_lock_timeout (wakelock timeout %d ms)", (uint32_t)irq, irq, (uint32_t)dev_id, smc_channel->wakelock_timeout_ms);

            if( smc_channel->wakelock_timeout_ms > 0 )
            {
                wake_lock_timeout( (struct wake_lock*)smc_channel->smc_rx_wakelock, msecs_to_jiffies(smc_channel->wakelock_timeout_ms) );
            }
            else
            {
                wake_unlock( (struct wake_lock*)smc_channel->smc_rx_wakelock );
            }
        }
        else if( smc_channel->smc_rx_wakelock != NULL &&
                (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_MESSAGE) == SMC_CHANNEL_WAKELOCK_MESSAGE )
        {
            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_resource_threaded: IRQ: 0x%02X (%d), Device 0x%08X wake_unlock", (uint32_t)irq, irq, (uint32_t)dev_id );

            wake_unlock( (struct wake_lock*)smc_channel->smc_rx_wakelock );
        }
    }
    else
    {
        SMC_TRACE_PRINTF_WARNING("smc_linux_interrupt_handler_int_resource_threaded: No channel initialized for signal handler 0x%08X", (uint32_t)signal_handler);
    }

    return IRQ_HANDLED;
}

#endif

static irqreturn_t smc_linux_interrupt_handler_int_resource(int irq, void *dev_id )
{
    smc_signal_handler_t* signal_handler = NULL;
    smc_signal_t*         signal         = NULL;
    smc_channel_t*        smc_channel    = NULL;
    uint32_t              signal_type    = SMC_SIGNAL_TYPE_INT_RESOURCE;
    int                   irq_spi        = irq-SMC_APE_IRQ_OFFSET_INTCSYS_SPI;
    irqreturn_t           retval;

    RD_TRACE_SEND2(TRA_SMC_IRQ_START, 4, &irq,
                                      4, &signal_type );

        /* Get the appropriate signal from array */
    signal_handler = smc_signal_handler_get( (uint32_t)irq_spi, signal_type );

    assert( signal_handler != NULL );

    smc_channel = signal_handler->smc_channel;
    signal      = signal_handler->signal;

    if( smc_channel != NULL &&
        smc_channel->smc_rx_wakelock != NULL &&
      ((smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_TIMER)   == SMC_CHANNEL_WAKELOCK_TIMER ||
       (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_MESSAGE) == SMC_CHANNEL_WAKELOCK_MESSAGE) )
    {
        wake_lock( (struct wake_lock*)smc_channel->smc_rx_wakelock );

        SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_resource: channel %d (use wakelock)", smc_channel->id);
    }
    else
    {
        SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_resource: channel %d (no wakelock)", smc_channel->id);
    }

    /* Clear IRQ first the handle the request to avoid missing irqs */
    /* Pull down the IRQ since there is GOP001 used in Modem Side (the peripheral address is given) */
    if( signal != NULL && signal->peripheral_address != 0 )
    {
        /* TODO FIX: The SMC_APE_IRQ_OFFSET_INTCSYS_TO_WGM only valid for SPI 193 - 198 */
        uint32_t      genios = (1UL << ( signal->interrupt_id - SMC_APE_IRQ_OFFSET_INTCSYS_TO_WGM) );
        smc_gop001_t* gop001 = (smc_gop001_t*)signal->peripheral_address;

        SMC_TRACE_PRINTF_SIGNAL("smc_linux_interrupt_handler_int_resource: Clear signal %d with gop001 CLEAR value 0x%08X", signal->interrupt_id, genios);

#ifdef SMC_MODEM_WAKEUP_WAIT_TIMEOUT_MS
        SMC_HOST_ACCESS_WAKEUP( get_local_lock_sleep_control(), SMC_MODEM_WAKEUP_WAIT_TIMEOUT_MS );
#endif

        SMC_LOCK_IRQ( get_local_lock_signal_intgen_clear() );

        /*genios |= SMC_SHM_READ32( &gop001->clear );*/
        SMC_SHM_WRITE32( &gop001->clear, genios );
        SMC_SHM_READ32( &gop001->clear );

        SMC_UNLOCK_IRQ( get_local_lock_signal_intgen_clear() );

#ifdef SMC_MODEM_WAKEUP_WAIT_TIMEOUT_MS
        SMC_HOST_ACCESS_SLEEP( get_local_lock_sleep_control() );
#endif
    }

#ifdef SMC_APE_USE_TASKLET_IN_IRQ
    if( signal_handler->userdata != 0x00000000 )
    {
        SMC_TRACE_PRINTF_SIGNAL("smc_linux_interrupt_handler_int_resource: signal handler 0x%08X uses tasklet 0x%08X...", (uint32_t)signal_handler, signal_handler->userdata);

        tasklet_schedule((struct tasklet_struct*)signal_handler->userdata);

        retval = IRQ_HANDLED;
    }
    else
#endif
    {

#ifdef SMC_APE_USE_THREADED_IRQ
        /* NOTE: The Wakelock is released in the threaded IRQ handler */

        retval = IRQ_WAKE_THREAD;
    }
#else

        retval = smc_linux_interrupt_handler_int_resource_exec( signal_handler );
    }

    RD_TRACE_SEND2(TRA_SMC_IRQ_END, 4, &irq,
                                    4, &signal_type );

    if( smc_channel != NULL)
    {
        if( smc_channel->smc_rx_wakelock != NULL &&
           (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_TIMER) == SMC_CHANNEL_WAKELOCK_TIMER )
        {
            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_resource: IRQ: 0x%02X (%d), Device 0x%08X wake_lock_timeout (wakelock timeout %d ms)", (uint32_t)irq, irq, (uint32_t)dev_id, smc_channel->wakelock_timeout_ms);

            if( smc_channel->wakelock_timeout_ms > 0 )
            {
                wake_lock_timeout( (struct wake_lock*)smc_channel->smc_rx_wakelock, msecs_to_jiffies(smc_channel->wakelock_timeout_ms) );
            }
            else
            {
                wake_unlock( (struct wake_lock*)smc_channel->smc_rx_wakelock );
            }
        }
        else if( smc_channel->smc_rx_wakelock != NULL &&
                (smc_channel->wake_lock_flags&SMC_CHANNEL_WAKELOCK_MESSAGE) == SMC_CHANNEL_WAKELOCK_MESSAGE )
        {
            SMC_TRACE_PRINTF_SIGNAL_RECEIVE("smc_linux_interrupt_handler_int_resource: IRQ: 0x%02X (%d), Device 0x%08X wake_unlock", (uint32_t)irq, irq, (uint32_t)dev_id );

            wake_unlock( (struct wake_lock*)smc_channel->smc_rx_wakelock );
        }
    }
#endif

    return retval;
}

smc_signal_t* smc_signal_create( uint32_t signal_id, uint32_t signal_type )
{
    smc_signal_t* signal = (smc_signal_t*)SMC_MALLOC( sizeof( smc_signal_t ) );

    assert(signal!=NULL);

    signal->interrupt_id       = signal_id;
    signal->signal_type        = signal_type;
    signal->event_sense        = SMC_SIGNAL_SENSE_RISING_EDGE;
    signal->peripheral_address = 0;
    signal->address_remapped   = FALSE;

    if( signal->signal_type == SMC_SIGNAL_TYPE_INTGEN ||
        signal->signal_type == SMC_SIGNAL_TYPE_INT_RESOURCE )
    {
        uint32_t p_address = SMC_PERIPHERAL_ADDRESS_MODEM_GOP_INTGEN_1+SMC_ADDRESS_APE_OFFSET_TO_MODEM ;

        signal->peripheral_address = (uint32_t)ioremap(p_address, sizeof(smc_gop001_t));
        signal->address_remapped   = TRUE;

        SMC_TRACE_PRINTF_SIGNAL("smc_signal_create: signal: 0x%08X, ID: (%d) 0x%08X type 0x%08X, remapped peripheral address from 0x%08X is 0x%08X",
                    (uint32_t)signal, signal->interrupt_id, signal->interrupt_id, signal->signal_type,
                    p_address, (uint32_t)signal->peripheral_address);
    }

    SMC_TRACE_PRINTF_SIGNAL("smc_signal_create: signal: 0x%08X, ID: (%d) 0x%08X, type 0x%08X",
            (uint32_t)signal, signal->interrupt_id, signal->interrupt_id, signal->signal_type);

    return signal;
}

smc_signal_t*  smc_signal_copy( smc_signal_t* signal )
{
    smc_signal_t* copy_signal = NULL;

    SMC_TRACE_PRINTF_SIGNAL("smc_signal_copy: copy from 0x%08X", (uint32_t)signal);

    if( signal )
    {
        copy_signal = smc_signal_create( signal->interrupt_id, signal->signal_type );

        copy_signal->peripheral_address = signal->peripheral_address;
        copy_signal->address_remapped   = signal->address_remapped;
    }

    return copy_signal;
}

void smc_signal_destroy( smc_signal_t* signal )
{
    SMC_TRACE_PRINTF_SIGNAL("smc_signal_destroy: signal: 0x%08X", (uint32_t)signal);

    if( signal != NULL )
    {
        /* Unregister */

        if( signal->peripheral_address != 0 && signal->address_remapped==TRUE )
        {
            iounmap( (void*)signal->peripheral_address );
            signal->peripheral_address = 0;
        }
    }

    SMC_FREE( signal );
    signal = NULL;

    /* Free the signal handler */
    /*void free_irq(unsigned int irq, void *dev_id);*/
}

uint8_t smc_signal_raise( smc_signal_t* signal )
{
    SMC_TRACE_PRINTF_SIGNAL("smc_signal_raise: signal: 0x%08X", (uint32_t)signal);

    if( signal->signal_type == SMC_SIGNAL_TYPE_IRQ_NONE )
    {
        SMC_TRACE_PRINTF_SIGNAL_RAISE("smc_signal_raise: SMC_SIGNAL_TYPE_IRQ_NONE: No signal generated");
        return SMC_OK;
    }

    if( signal->signal_type == SMC_SIGNAL_TYPE_INTGEN )
    {
        if( signal->peripheral_address != 0 )
        {
            uint8_t ret_value = SMC_OK;
            uint32_t genios = (1UL << ((signal->interrupt_id-SMC_MODEM_INTGEN_L2_FIRST) + SMC_MODEM_INTGEN_L2_OFFSET));
            smc_gop001_t* gop001 = (smc_gop001_t*)signal->peripheral_address;

            SMC_TRACE_PRINTF_SIGNAL_RAISE("smc_signal_raise: SMC_SIGNAL_TYPE_INTGEN: Raise signal %d with gop001 (0x%08X) set value 0x%08X",
            signal->interrupt_id, (uint32_t)signal->peripheral_address, genios);

            RD_TRACE_SEND2(TRA_SMC_SIGNAL_INTGEN, 4, &signal,
                                                  4, &signal->interrupt_id );

#ifdef SMC_MODEM_WAKEUP_WAIT_TIMEOUT_MS
            SMC_HOST_ACCESS_WAKEUP( get_local_lock_sleep_control(), SMC_MODEM_WAKEUP_WAIT_TIMEOUT_MS );
#endif

            SMC_LOCK_IRQ( get_local_lock_signal_intgen_set() );

            /*genios |= SMC_SHM_READ32( &gop001->set );*/
            SMC_SHM_WRITE32( &gop001->set, genios );
            SMC_SHM_READ32( &gop001->set );

            SMC_UNLOCK_IRQ( get_local_lock_signal_intgen_set() );

#ifdef SMC_MODEM_WAKEUP_WAIT_TIMEOUT_MS
            SMC_HOST_ACCESS_SLEEP( get_local_lock_sleep_control() );
#endif

            SMC_TRACE_PRINTF_SIGNAL_RAISE("smc_signal_raise: SMC_SIGNAL_TYPE_INTGEN: Signal %d raise with gop001 completed, genio value 0x%08X",
                        signal->interrupt_id, genios);

            return ret_value;
        }
    }

    SMC_TRACE_PRINTF_ERROR("smc_signal_raise: Signal type 0x%08X FAILED", signal->signal_type);

    return SMC_ERROR;
}

uint8_t smc_signal_acknowledge( smc_signal_t* signal )
{
    uint8_t ret_value = SMC_OK;

    SMC_TRACE_PRINTF_SIGNAL("smc_signal_acknowledge: signal: 0x%08X", (uint32_t)signal);

    return ret_value;
}

uint8_t smc_signal_handler_register( smc_t* smc_instance, smc_signal_t* signal, smc_channel_t* smc_channel )
{
    uint8_t       ret_value           = SMC_OK;
    int           result              = 0;
    const char*   device_name         = NULL;
    void*         dev_id              = NULL;
    unsigned long flags               = 0x00; /* 0x00, IRQF_SHARED, IRQ_TYPE_PRIO, IRQF_SAMPLE_RANDOM | IRQF_DISABLED */
    smc_signal_handler_t* signal_hdlr = NULL;

    SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: signal 0x%08X: 0x%02X (%d), type 0x%08X for channel: 0x%08X", (uint32_t)signal, signal->interrupt_id, signal->interrupt_id, signal->signal_type, (uint32_t)smc_channel);

    signal_hdlr = smc_signal_handler_create_and_add( smc_instance, signal, smc_channel, 0 );

    assert(signal_hdlr != NULL);

#ifdef SMC_APE_USE_TASKLET_IN_IRQ
    /* Initialize tasklet for IRQ if configured */

    if( smc_channel->protocol == 2 /*SMC_L2MUX_QUEUE_3_MHDP*/)
    {
        SMC_TRACE_PRINTF_TASKLET("smc_signal_handler_register: create tasklet for channel %d...", smc_channel->id);

        signal_hdlr->userdata = (uint32_t)smc_create_tasklet(signal_hdlr, smc_signal_handler_tasklet_mhdp );

        SMC_TRACE_PRINTF_TASKLET("smc_signal_handler_register: tasklet 0x%08X created for channel %d", signal_hdlr->userdata, smc_channel->id);
    }
    else
    {
        SMC_TRACE_PRINTF_TASKLET("smc_signal_handler_register: no tasklet for channel %d", smc_channel->id);
    }
#endif

    if( signal->signal_type == SMC_SIGNAL_TYPE_IRQ_NONE )
    {
        SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: signal: 0x%08X is SMC_SIGNAL_TYPE_IRQ_NONE", (uint32_t)signal);
        result = 0;
    }
    else if( signal->signal_type == SMC_SIGNAL_TYPE_INTCBB )
    {
        SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: signal: 0x%08X is SMC_SIGNAL_TYPE_INTCBB", (uint32_t)signal);
        result = request_irq( (unsigned int)signal->interrupt_id, smc_linux_interrupt_handler_intcbb, flags, device_name, dev_id );

        if( result )
        {
            SMC_TRACE_PRINTF_ERROR("smc_signal_handler_register: signal: SMC_SIGNAL_TYPE_INTCBB 0x%08X: request_irq FAILED, result %d", (uint32_t)signal, result);
        }
        else
        {
            SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: signal: SMC_SIGNAL_TYPE_INTCBB 0x%08X: request_irq SUCCESS", (uint32_t)signal);
        }

#if(SMCTEST==TRUE)
        if( result )
        {
            /* Ignore in test mode, linux kernel perhaps not fully initialized */
            result = 0;
        }
#endif
    }
    else if( signal->signal_type == SMC_SIGNAL_TYPE_INT_WGM_GENOUT )
    {
        SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: signal: 0x%08X: is SMC_SIGNAL_TYPE_INT_WGM_GENOUT", (uint32_t)signal);

        result = request_irq( (unsigned int)signal->interrupt_id, smc_linux_interrupt_handler_int_genout, flags, device_name, dev_id );

        if( result )
        {
            SMC_TRACE_PRINTF_ERROR("smc_signal_handler_register: signal: SMC_SIGNAL_TYPE_INT_WGM_GENOUT 0x%08X: request_irq FAILED, result %d", (uint32_t)signal, result);
        }
        else
        {
            SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: signal: SMC_SIGNAL_TYPE_INT_WGM_GENOUT 0x%08X: request_irq SUCCESS", (uint32_t)signal);
        }

#if(SMCTEST==TRUE)
        if( result )
        {
            /* Ignore in test mode, linux kernel perhaps not fully initialized */
            result = 0;
        }
#endif
    }
    else if( signal->signal_type == SMC_SIGNAL_TYPE_INT_RESOURCE )
    {
        struct resource*        res             = NULL;
        struct platform_device* platform_device = NULL;

        SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: signal: 0x%08X: is SMC_SIGNAL_TYPE_INT_RESOURCE", (uint32_t)signal);

        if( smc_instance->smc_parent_ptr != NULL )
        {
            SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: parent object 0x%08X found, extract the platform device", smc_instance->smc_parent_ptr);
            platform_device = (struct platform_device*)smc_instance->smc_parent_ptr;
        }

        if( platform_device )
        {
            int     irq_spi = (int)signal->interrupt_id;
            int     res_id  = 0;
            uint8_t found   = FALSE;
			unsigned long irqflags = 0;

            /* TODO Check if change to: platform_get_resource_byname(pdev, IORESOURCE_IRQ, "cmd_irq"); */

            while( (res = platform_get_resource(platform_device, IORESOURCE_IRQ, res_id)) )
            {
                SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: check res id %d where IRQ ID %d...", res_id, res->start);

                if( (res->start-SMC_APE_IRQ_OFFSET_INTCSYS_SPI) == irq_spi )
                {
                    SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: res id %d: IRQ ID %d match to SPI %d...", res_id, res->start,irq_spi);
                    found = TRUE;
                    break;
                }

                res_id++;
            }

            if( found )
            {
                SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: signal: SMC_SIGNAL_TYPE_INT_RESOURCE 0x%08X: found res 0x%08X, IRQ start is %d",
                        (uint32_t)signal, res, res->start );

                /*flags = IRQF_SHARED | IRQF_DISABLED;*/
                flags = IRQF_DISABLED;

                device_name  = dev_name(&platform_device->dev);
                dev_id       = platform_device;

#ifdef SMC_APE_USE_THREADED_IRQ
                result = request_threaded_irq(res->start, smc_linux_interrupt_handler_int_resource, smc_linux_interrupt_handler_int_resource_threaded, irqflags, device_name, dev_id);
#else
                result = request_irq( res->start, smc_linux_interrupt_handler_int_resource, flags, device_name, dev_id );
#endif

                if( result )
                {
                    SMC_TRACE_PRINTF_ERROR("smc_signal_handler_register: signal: SMC_SIGNAL_TYPE_INT_RESOURCE 0x%08X: request_irq FAILED, result %d", (uint32_t)signal, result);
                }
                else
                {
                    SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: signal: SMC_SIGNAL_TYPE_INT_RESOURCE 0x%08X: request_irq SUCCESS", (uint32_t)signal);
                }

                enable_irq_wake(res->start);
            }
            else
            {
                SMC_TRACE_PRINTF_ERROR("smc_signal_handler_register: signal: SMC_SIGNAL_TYPE_INT_RESOURCE 0x%08X: platform_get_resource with irq id %d FAILED", (uint32_t)signal, signal->interrupt_id);
            }
        }
        else
        {
            SMC_TRACE_PRINTF_ERROR("smc_signal_handler_register: signal: SMC_SIGNAL_TYPE_INT_RESOURCE 0x%08X: No device, unable to get IRQ %d", (uint32_t)signal, signal->interrupt_id);
        }

#if(SMCTEST==TRUE)
        if( result )
        {
            /* Ignore in test mode, linux kernel perhaps not fully initialized */
            result = 0;
        }
#endif
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_signal_handler_register: signal: 0x%08X type 0x%08X not supported, no handler initialized", (uint32_t)signal, signal->signal_type);
        result = -1;
    }

    if( result )
    {
        SMC_TRACE_PRINTF_ERROR("smc_signal_handler_register: signal: 0x%08X: 0x%02X (%d) request for channel: 0x%08X FAILED", (uint32_t)signal, signal->interrupt_id, signal->interrupt_id, (uint32_t)smc_channel);
        ret_value = SMC_ERROR;
    }
    else
    {
        SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_register: signal: 0x%08X: 0x%02X (%d) for channel: 0x%08X successfully registered", (uint32_t)signal, signal->interrupt_id, signal->interrupt_id, (uint32_t)smc_channel);
        ret_value = SMC_OK;
    }

    return ret_value;
}

/**
 * Unregister signal and remove signal handler.
 */
uint8_t smc_signal_handler_unregister( smc_t* smc_instance, smc_signal_t* signal, smc_channel_t* smc_channel )
{
    uint8_t               ret_value      = SMC_OK;
    smc_signal_handler_t* signal_handler = NULL;

    if( signal != NULL )
    {
        void* dev_id = NULL;

        signal_handler = smc_signal_handler_get(signal->interrupt_id, signal->signal_type);


        if( signal->signal_type == SMC_SIGNAL_TYPE_IRQ_NONE )
        {
            SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: signal: 0x%08X is SMC_SIGNAL_TYPE_IRQ_NONE", (uint32_t)signal);
        }
        else if( signal->signal_type == SMC_SIGNAL_TYPE_INTCBB )
        {
            SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: signal: 0x%08X: is SMC_SIGNAL_TYPE_INTCBB", (uint32_t)signal);

            free_irq((int)signal->interrupt_id, dev_id);
        }
        else if( signal->signal_type == SMC_SIGNAL_TYPE_INT_WGM_GENOUT )
        {
            SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: signal: 0x%08X: is SMC_SIGNAL_TYPE_INT_WGM_GENOUT", (uint32_t)signal);

            free_irq((int)signal->interrupt_id, dev_id);
        }
        else if( signal->signal_type == SMC_SIGNAL_TYPE_INT_RESOURCE )
        {
            struct resource*        res             = NULL;
            struct platform_device* platform_device = NULL;

            SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: signal: 0x%08X: is SMC_SIGNAL_TYPE_INT_RESOURCE", (uint32_t)signal);

            if( smc_instance->smc_parent_ptr != NULL )
            {
                SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: parent object 0x%08X found, extract the platform device", smc_instance->smc_parent_ptr);
                platform_device = (struct platform_device*)smc_instance->smc_parent_ptr;
            }

            if( platform_device )
            {
                int     irq_spi = (int)signal->interrupt_id;
                int     res_id  = 0;
                uint8_t found   = FALSE;

                while( (res = platform_get_resource(platform_device, IORESOURCE_IRQ, res_id)) )
                {
                    SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: check res id %d where IRQ ID %d...", res_id, res->start);

                    if( (res->start-SMC_APE_IRQ_OFFSET_INTCSYS_SPI) == irq_spi )
                    {
                        SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: res id %d: IRQ ID %d match to SPI %d...", res_id, res->start,irq_spi);
                        found = TRUE;
                        break;
                    }

                    res_id++;
                }

                if( found )
                {
                    dev_id    = platform_device;

                    SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: signal: SMC_SIGNAL_TYPE_INT_RESOURCE 0x%08X: found res 0x%08X, IRQ start is %d",
                            (uint32_t)signal, res, res->start );

                    free_irq( res->start, dev_id );
                }
                else
                {
                    SMC_TRACE_PRINTF_ERROR("smc_signal_handler_unregister: signal: SMC_SIGNAL_TYPE_INT_RESOURCE 0x%08X: platform_get_resource with irq id %d FAILED", (uint32_t)signal, signal->interrupt_id);
                }
            }
            else
            {
                SMC_TRACE_PRINTF_ERROR("smc_signal_handler_unregister: signal: SMC_SIGNAL_TYPE_INT_RESOURCE 0x%08X: No device, unable to get IRQ %d", (uint32_t)signal, signal->interrupt_id);
            }
        }
        else
        {
            SMC_TRACE_PRINTF_ERROR("smc_signal_handler_unregister: signal: 0x%08X type 0x%08X not supported", (uint32_t)signal, signal->signal_type);
            ret_value      = SMC_ERROR;
        }

        if( signal_handler != NULL )
        {
#ifdef SMC_APE_USE_TASKLET_IN_IRQ
            if( signal_handler->userdata != NULL )
            {
                struct tasklet_struct* task = NULL;

                SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: kill and remove signal handler's tasklet...");

                task = (struct tasklet_struct*)signal_handler->userdata;

                tasklet_kill( task );

                SMC_FREE( task );

                signal_handler->userdata = NULL;
            }
#endif
            SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: remove signal handler...");
            smc_signal_handler_remove_and_destroy( signal_handler );
        }
    }

    SMC_TRACE_PRINTF_SIGNAL("smc_signal_handler_unregister: completed");

    return ret_value;
}


void smc_signal_dump( char* indent, smc_signal_t* signal )
{
    if( signal != NULL )
    {
        SMC_TRACE_PRINTF_ALWAYS("%sSignal 0x%08X: ID %d, type 0x%08X, address: 0x%08X (%s)", indent, (uint32_t)signal,
                                                                        signal->interrupt_id,
                                                                        signal->signal_type,
                                                                        (uint32_t)signal->peripheral_address,
                                                                        signal->address_remapped?"remapped":"not remapped");
    }
    else
    {
        SMC_TRACE_PRINTF_ALWAYS("%s<Signal not initialized>", indent);
    }
}


/* =============================================================
 * SMC locking function platform specific implementation
 * The lock struct defined in smc_conf_platform.h
 *
 */

/* Lock creation changed as macro to satisfy lockdep
smc_lock_t* smc_lock_create( void )
{
    smc_lock_t* lock = (smc_lock_t*)SMC_MALLOC( sizeof( smc_lock_t ) );

    spin_lock_init(&lock->mr_lock);

    lock->flags = 0x00000000;

    SMC_TRACE_PRINTF_INFO("smc_lock_create: lock 0x%08X...", (uint32_t)lock);

    return lock;
}
*/

void smc_lock_destroy( smc_lock_t* lock )
{
    SMC_TRACE_PRINTF_INFO("smc_lock_destroy: lock 0x%08X...", (uint32_t)lock);

    if( lock != NULL )
    {
        SMC_FREE( lock );
        lock = NULL;
    }
}


/* =============================================================
 * SMC semaphore function platform specific implementations
 * The semaphore struct is defined in smc_conf_platform.h
 *
 */

smc_semaphore_t* smc_semaphore_create( void )
{
    smc_semaphore_t* semaphore = (smc_semaphore_t*)SMC_MALLOC( sizeof( smc_semaphore_t ) );

    assert( semaphore != NULL );

    mutex_init( &(semaphore->smc_mutex) );

    SMC_TRACE_PRINTF_INFO("smc_semaphore_create: semaphore 0x%08X...", (uint32_t)semaphore);

    return semaphore;
}

void smc_semaphore_destroy( smc_semaphore_t* semaphore )
{
    SMC_TRACE_PRINTF_INFO("smc_semaphore_destroy: semaphore 0x%08X...", (uint32_t)semaphore);

    if( semaphore != NULL )
    {
        mutex_destroy( &(semaphore->smc_mutex) );

        SMC_FREE( semaphore );
    }
}


/* =============================================================
 * SMC timer functions platform specific implementations
 * The timer struct is defined in smc_conf_platform.h
 *
 */

smc_timer_t* smc_timer_create( uint32_t timer_usec )
{
    smc_timer_t* timer = (smc_timer_t*)SMC_MALLOC( sizeof( smc_timer_t ) );

    assert(timer != NULL );

    timer->period_us      = timer_usec;
    timer->timer_data     = 0;
    timer->smc_timer_list = NULL;
    timer->prev_jiffies   = 0;

    timer->hr_timer       = NULL;

    SMC_TRACE_PRINTF_TIMER("smc_timer_create: timer 0x%08X created, timeout is %u usec", (uint32_t)timer, timer_usec);

    return timer;
}

uint8_t smc_timer_start( smc_timer_t* timer, smc_timer_callback* timer_cb, uint32_t timer_data )
{
    uint8_t       ret_val = SMC_OK;
    unsigned long period  = (timer->period_us * HZ)/(1000*1000);

    timer->timer_data = timer_data;


    if( 1 )
    {
        if( timer->smc_timer_list == NULL )
        {
            SMC_TRACE_PRINTF_TIMER("smc_timer_start: timer 0x%08X CB 0x%08X create new timer list...", (uint32_t)timer, (uint32_t)timer_cb);

            timer->smc_timer_list = (struct timer_list*)SMC_MALLOC_IRQ( sizeof(struct timer_list) );

            assert( timer->smc_timer_list != NULL );

            timer->smc_timer_list->data     = (unsigned long)timer;
            timer->smc_timer_list->function = (void*)timer_cb;
            timer->prev_jiffies             = jiffies;
            timer->smc_timer_list->expires  = timer->prev_jiffies + period;

            init_timer( timer->smc_timer_list );

            add_timer( timer->smc_timer_list );
        }
        else
        {
            SMC_TRACE_PRINTF_TIMER("smc_timer_start: timer 0x%08X CB 0x%08X: timer list created, modify it...", (uint32_t)timer, (uint32_t)timer_cb);

            timer->prev_jiffies = jiffies;
            mod_timer( timer->smc_timer_list, (timer->prev_jiffies + period) );
        }

        SMC_TRACE_PRINTF_TIMER("smc_timer_start: timer 0x%08X CB 0x%08X period %d us, expires: %lu , HZ: %d...",
                    (uint32_t)timer, (uint32_t)timer_cb, timer->period_us, timer->smc_timer_list->expires, HZ);
    }
    else
    {
        if(timer->hr_timer == NULL )
        {
                /* High rate timer */
            SMC_TRACE_PRINTF_TIMER("smc_timer_start: timer 0x%08X create HR timer, CB: 0x%08X...", (uint32_t)timer, (uint32_t)timer_cb);

            timer->hr_timer = (struct hrtimer *)SMC_MALLOC_IRQ( sizeof(struct hrtimer *) );

            hrtimer_init(timer->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

            /* TODO Setup the callbacks
            timer->hr_timer.function = &tx_timer_timeout;
            tasklet_init(&tunnel->taskl, tx_timer_timeout_tasklet, (unsigned long) tunnel);
            */
            SMC_TRACE_PRINTF_ASSERT("smc_timer_start: Not implemented for high rate timer");
            assert(0);
        }

        SMC_TRACE_PRINTF_TIMER("smc_timer_start: timer 0x%08X CB 0x%08X period %d us", (uint32_t)timer, (uint32_t)timer_cb, timer->period_us);

        hrtimer_start(timer->hr_timer, ktime_set(0, 1000*timer->period_us), HRTIMER_MODE_REL);
    }

    return ret_val;
}

uint8_t smc_timer_stop( smc_timer_t* timer )
{
    uint8_t ret_val = SMC_OK;

    SMC_TRACE_PRINTF_TIMER("smc_timer_stop: timer 0x%08X...", (uint32_t)timer);

    if( timer!= NULL )
    {
        if( timer->smc_timer_list != NULL )
        {
            SMC_TRACE_PRINTF_TIMER("smc_timer_stop: delete timer list 0x%08X...", (uint32_t)timer->smc_timer_list);
            del_timer( timer->smc_timer_list );
            SMC_TRACE_PRINTF_TIMER("smc_timer_stop: free timer list ptr 0x%08X...", (uint32_t)timer->smc_timer_list);
            SMC_FREE( timer->smc_timer_list );
            timer->smc_timer_list = NULL;
        }
        else if( timer->hr_timer != NULL )
        {
            if (hrtimer_active(timer->hr_timer))
            {
                SMC_TRACE_PRINTF_TIMER("smc_timer_stop: stop HR timer 0x%08X...", (uint32_t)timer->hr_timer);
                hrtimer_cancel(timer->hr_timer);
            }
        }
    }
    else
    {
        SMC_TRACE_PRINTF_TIMER("smc_timer_stop: invalid timer parameter NULL");
        ret_val = SMC_ERROR;
    }

    return ret_val;
}

uint8_t smc_timer_destroy( smc_timer_t* timer )
{
    uint8_t ret_val = SMC_OK;

    SMC_TRACE_PRINTF_TIMER("smc_timer_destroy: timer 0x%08X...", (uint32_t)timer);

    if( timer!= NULL )
    {
        if( timer->smc_timer_list != NULL )
        {
            SMC_TRACE_PRINTF_TIMER("smc_timer_destroy: delete timer list 0x%08X...", (uint32_t)timer->smc_timer_list);
            del_timer( timer->smc_timer_list );
            SMC_TRACE_PRINTF_TIMER("smc_timer_destroy: free timer list ptr 0x%08X...", (uint32_t)timer->smc_timer_list);
            SMC_FREE( timer->smc_timer_list );
            timer->smc_timer_list = NULL;
        }

        if( timer->hr_timer != NULL )
        {
            if (hrtimer_active(timer->hr_timer))
            {
                SMC_TRACE_PRINTF_TIMER("smc_timer_destroy: stop HR timer 0x%08X...", (uint32_t)timer->hr_timer);
                hrtimer_cancel(timer->hr_timer);
            }

            SMC_TRACE_PRINTF_TIMER("smc_timer_destroy: free HR timer 0x%08X...", (uint32_t)timer->hr_timer);

            SMC_FREE( timer->hr_timer );
            timer->hr_timer = NULL;
        }


        SMC_TRACE_PRINTF_TIMER("smc_timer_destroy: free timer 0x%08X...", (uint32_t)timer);
        SMC_FREE( timer );
        timer = NULL;
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_timer_destroy: invalid timer parameter NULL");
        ret_val = SMC_ERROR;
    }

    return ret_val;
}



/* =============================================================
 * Miscellaneous Linux Kernel implementations.
 */
uint32_t rand(void)
{
    uint32_t random;

    get_random_bytes(&random, sizeof(random));

    return random;
}

/*
 * Linux Kernel data trace function to print data array contents.
 */

void smc_printf_data_linux_kernel(int length, uint8_t* data)
{
    int i = 0;
    int row_len = 16;

    printk(KERNEL_DEBUG_LEVEL "\n");

    for( i = 0; i < length; i++ )
    {
        if(i%row_len == 0)
        {
            printk("0x%02X", data[i]);
        }
        else
        {
            printk(" 0x%02X", data[i]);
        }

        if( i > 0 && ( i%(row_len) == (row_len-1) || i >= length-1 ))
        {
            printk("\n");
        }
    }
}

#ifdef SMC_APE_LINUX_KERNEL_STM

void smc_printf_data_linux_kernel_stm(int length, uint8_t* data)
{
    int i = 0;
    int row_len = 16;

    /* TODO Fix the line break between bytes seen in Ntrace*/
    /*smc_printk("\n");*/

    for( i = 0; i < length; i++ )
    {
        if(i%row_len == 0)
        {
            smc_printk("0x%02X", data[i]);
        }
        else
        {
            smc_printk(" 0x%02X", data[i]);
        }

        /*
        if( i > 0 && ( i%(row_len) == (row_len-1) || i >= length-1 ))
        {
            smc_printk("\n");
        }
        */
    }
}


#endif /* #ifdef SMC_APE_LINUX_KERNEL_STM */


/*
 * SMC initialization function if called.
 */
uint8_t smc_module_initialize( smc_conf_t* smc_instance_conf )
{
    uint8_t ret_value = SMC_OK;

    SMC_TRACE_PRINTF_DEBUG("smc_module_initialize: Initialize Linux Kernel implementation module...");

    /*
     * Initialize locks
     */

    get_local_lock_sleep_control();
    get_local_lock_signal_intgen_set();
    get_local_lock_signal_intgen_clear();

    SMC_TRACE_PRINTF_DEBUG("smc_module_initialize: Linux Kernel implementation module initialization completed by return value 0x%02X", ret_value);

    return ret_value;
}

/*
 * Reading ASIC version from the EOS2 ES10/ES20
 * TODO Own module required
 */
uint16_t smc_asic_version_get(void)
{
    uint8_t cpu_version = 0x00;
    unsigned int cccr, major, minor;

    cccr = readl(SMC_CCCR);

    major = ((cccr & 0xF0) >> 4) + 1;
    minor = cccr & 0x0F;

    cpu_version = ((major&0xFF)<<4) + (minor&0xFF);

    SMC_TRACE_PRINTF_DEBUG("ASIC version Renesas R-Mobile U2 ES%d.%d (0x%08X / 0x%04X)\n", major, minor, cccr, cpu_version);

    return cpu_version;
}

/* ================================================================
 * Runtime  configuration handling
 */

// TODO clean-up static uint8_t g_smc_instance_config_send = FALSE;

uint8_t smc_conf_request_initiate( smc_channel_t* channel )
{
    uint8_t ret_val = SMC_ERROR;

    SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_conf_request_initiate: channel %d...",  channel->id);

    //if( !g_smc_instance_config_send )
    if( !SMC_INSTANCE_FIXED_CONFIG_SENT( channel->smc_instance->init_status ) )
    {
        //SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_conf_request_initiate: send SMC instance config using channel %d...",  channel->id);

        SMC_TRACE_PRINTF_STARTUP("Channel %d negotiating SMC instance config...",  channel->id);

#if( defined( SMC_WAKEUP_USE_EXTERNAL_IRQ_APE ) || defined( SMC_WAKEUP_USE_EXTERNAL_IRQ_MODEM ) )
        /* Send the APE wakeup sense request to have falling edge event */

        smc_channel_send_config(channel, SMC_RUNTIME_CONFIG_ID_APE_WAKEUP_EVENT_SENSE, SMC_APE_WAKEUP_EXTERNAL_IRQ_SENSE, FALSE );
#endif

        //g_smc_instance_config_send = TRUE;
        SMC_INSTANCE_STATUS_SET_FIXED_CONFIG_SENT( channel->smc_instance->init_status );
    }
    else
    {
        SMC_TRACE_PRINTF_DEBUG("smc_conf_request_initiate: send SMC instance config already initialized");
    }

#ifdef SMC_CHANNEL_ID_FOR_CONFIGURATION

    if( channel->id == SMC_CHANNEL_ID_FOR_CONFIGURATION )
    {
        smc_t* smc_instance = channel->smc_instance;

        SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_conf_request_initiate: channel %d is used to configure others (count %d)", channel->id, smc_instance->smc_channel_list_count-1);

        ret_val = SMC_OK;

        for(int i = 0; i < smc_instance->smc_channel_list_count; i++ )
        {
            if( i != SMC_CHANNEL_ID_FOR_CONFIGURATION )
            {
                smc_channel_t* channel_to_config = SMC_CHANNEL_GET(smc_instance, i);

                SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_conf_request_initiate: channel %d configures channel %d...", channel->id, channel_to_config->id);

                if( smc_channel_send_fixed_config(channel, channel_to_config) != SMC_OK )
                {
                    ret_val = SMC_ERROR;
                    SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_conf_request_initiate: channel %d configuration to channel %d FAILED", channel->id, channel_to_config->id);
                }
            }
        }
    }
    else
    {
        SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_conf_request_initiate: channel %d, no configuration channel defined", channel->id);
        ret_val = SMC_OK;
    }
#else
    SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_conf_request_initiate: channel %d, there is no configuration channel defined", channel->id);
#endif

    /* Last parameters that are not depended on any fixed value*/


    /* This function is deprecated: smc_channel_send_config_shm( channel, FALSE ); */


    SMC_TRACE_PRINTF_DEBUG("smc_conf_request_initiate: channel %d completed",  channel->id);

    return ret_val;
}

void smc_channel_fixed_config_response( smc_channel_t* smc_channel, smc_channel_t* smc_channel_target, smc_user_data_t* userdata_resp )
{
    SMC_TRACE_PRINTF_RUNTIME_CONF_SHM("smc_channel_fixed_config_response: channel %d configuration response received, configuration %s and status 0x%08X",  smc_channel_target->id, (userdata_resp->userdata4==SMC_OK)?"OK":"FAILED", userdata_resp->userdata3);

    if( userdata_resp->userdata4 == SMC_OK )
    {
        if( userdata_resp->userdata3 == SMC_FIXED_CONFIG_NO_CHANGES )
        {
            SMC_TRACE_PRINTF_STARTUP("Channel %d fixed configuration negotiated, no changes in remote host", smc_channel_target->id);
        }
        else
        {
            SMC_TRACE_PRINTF_STARTUP("Channel %d fixed configuration negotiated, remote host changed:%s%s%s%s%s%s", smc_channel_target->id,
                    ((userdata_resp->userdata3&SMC_FIXED_CONFIG_CHANGE_FIFO_IN)==SMC_FIXED_CONFIG_CHANGE_FIFO_IN)?" [FIFO in]":"",
                    ((userdata_resp->userdata3&SMC_FIXED_CONFIG_CHANGE_FIFO_OUT)==SMC_FIXED_CONFIG_CHANGE_FIFO_OUT)?" [FIFO out]":"",
                    ((userdata_resp->userdata3&SMC_FIXED_CONFIG_CHANGE_SHM_START)==SMC_FIXED_CONFIG_CHANGE_SHM_START)?" [SHM start]":"",
                    ((userdata_resp->userdata3&SMC_FIXED_CONFIG_CHANGE_SHM_SIZE)==SMC_FIXED_CONFIG_CHANGE_SHM_SIZE)?" [SHM size]":"",
                    ((userdata_resp->userdata3&SMC_FIXED_CONFIG_CHANGE_MDB_IN)==SMC_FIXED_CONFIG_CHANGE_MDB_IN)?" [MDB in]":"",
                    ((userdata_resp->userdata3&SMC_FIXED_CONFIG_CHANGE_MDB_OUT)==SMC_FIXED_CONFIG_CHANGE_MDB_OUT)?" [MDB out]":"");
        }
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("Channel %d fixed configuration negotiation failed", smc_channel_target->id );
    }
}


/*
 * Configuration request received from the remote CPU.
 * Only one configuration is received.
 */
uint8_t smc_conf_request_received( smc_channel_t* channel, uint32_t configuration_id, uint32_t configuration_value)
{
    uint8_t ret_val = SMC_ERROR;

    SMC_TRACE_PRINTF_DEBUG("smc_conf_request_received: channel %d, config ID = 0x%08X, value = 0x%08X",  channel->id, configuration_id, configuration_value);

    return ret_val;
}

void smc_conf_response_received( smc_channel_t* channel, uint32_t configuration_id, uint32_t configuration_value_requested, uint32_t configuration_response_value )
{
    SMC_TRACE_PRINTF_DEBUG("smc_conf_response_received: channel %d, requesete config ID = 0x%08X to 0x%08X, response = 0x%08X",  channel->id, configuration_id, configuration_value_requested, configuration_response_value);


    if( configuration_id == SMC_RUNTIME_CONFIG_ID_APE_WAKEUP_EVENT_SENSE )
    {
        SMC_TRACE_PRINTF_DEBUG("smc_conf_response_received: channel %d, config 0x%08X: SMC_RUNTIME_CONFIG_ID_APE_WAKEUP_EVENT_SENSE %s by remote",
                channel->id, configuration_id, (configuration_response_value==SMC_OK)?"ACCEPTED":"REJECTED");

        if( configuration_response_value==SMC_OK )
        {
            smc_set_ape_wakeup_irq_sense( (uint8_t)(configuration_value_requested&0xFF) );
        }

    }

}


/*
 * Shared variable function to get local addresses.
 */
void* smc_shared_variable_address_get_local( char* shared_variable_name )
{
    void*  shared_var_address1    = NULL;

    /*
    void*  shared_var_address2    = NULL;
    void*  test_address1          = NULL;
    void*  test_address2          = NULL;

    struct hwspinlock_private* p = r8a73734_hwlock_gpio->priv;

    SMC_TRACE_PRINTF_SHM_VAR("smc_shared_variable_address_get_local: shared var name '%s'...", shared_variable_name );

    SMC_TRACE_PRINTF_ERROR("smc_shared_variable_address_get_local: *** NOT IMPLEMENTED ***");
    shared_var_address1 = (void*)p->ext_base;
    test_address1       = (void*)&p->ext_base;

    shared_var_address2 = (void*)p->sm_base;
    test_address2       = (void*)&p->sm_base;

    SMC_TRACE_PRINTF_SHM_VAR("smc_shared_variable_address_get_local: return shared var name '%s' shared_var_address1 = 0x%08X (0x%08X), shared_var_address2 = 0x%08X (0x%08X)...",
            shared_variable_name, shared_var_address1, test_address1, shared_var_address2, test_address2 );

    */
    return shared_var_address1;
}


#ifdef SMC_APE_LINUX_KERNEL_STM

static smc_lock_t* g_local_lock_smc_trace = NULL;

static inline smc_lock_t* get_local_lock_smc_trace(void)
{
    if( g_local_lock_smc_trace == NULL ) g_local_lock_smc_trace = smc_lock_create();
    return g_local_lock_smc_trace;
}

static uint32_t smc_stm_ch_ptr = 0x00000000;

static inline uint32_t get_smc_stm_ch77(void)
{
    if( smc_stm_ch_ptr == 0x00000000 )
    {
        #define SYS_STM_STIMULUS_BASE_CH(_X_)   (0xE9000000 + (0x100*_X_))
        #define SYS_STM_STIMULUS_SIZE           256

        smc_stm_ch_ptr = (uint32_t)ioremap((uint32_t)SYS_STM_STIMULUS_BASE_CH(0x77), SYS_STM_STIMULUS_SIZE);
    }

    return smc_stm_ch_ptr;
}

void smc_vprintk(const char *fmt, va_list args)
{
    uint32_t stm_channel = get_smc_stm_ch77();

    if( stm_channel )
    {
        char printk_buf[1166];
        int printed_len = 0;
        char *bptr = NULL;
        volatile uint32_t tsfreq = 0;

        smc_lock_t* local_lock = get_local_lock_smc_trace();

        spin_lock( &(local_lock->mr_lock));

        printed_len += vscnprintf(printk_buf + printed_len, sizeof(printk_buf) - printed_len, fmt, args);

        *(volatile char *)(stm_channel + 0x18) = 0x20; /* ASCII Printf Identifier */

        bptr = printk_buf;

        while(printed_len >= 4)
        {
            *(volatile long *)(stm_channel + 0x18) = *(volatile long *)bptr;
            bptr += 4;
            printed_len -= 4;
        }

        while(printed_len > 0)
        {
            *(volatile char *)(stm_channel + 0x18) = *(volatile char *)bptr;
            bptr++;
            printed_len--;
        }

        *(volatile char *)(stm_channel + 0x00) = 0x00; /* timestamp and closure */

        #define SYS_STM_TSFREQR    IO_ADDRESS(0xE6F89E8C)

        tsfreq = __raw_readl(SYS_STM_TSFREQR /* sys_stm_conf.TSFREQR */);
        __raw_writel(tsfreq, SYS_STM_TSFREQR /* sys_stm_conf.TSFREQR */);
        tsfreq = __raw_readl(SYS_STM_TSFREQR /* sys_stm_conf.TSFREQR */);

        spin_unlock( &(local_lock->mr_lock));
    }
}

void smc_printk(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    smc_vprintk(fmt, args);
    va_end(args);
}

void smc_printk_data(const char* prefix_text, const uint8_t* data, int data_len, int max_print_len)
{
    if( data != NULL )
    {
        int data_index = 0;

        while( (data_len - data_index) >= 10 )
        {
            smc_printk("%s 0x%08X + %04d:  0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X",
                    prefix_text,
                    (uint32_t)data,
                    data_index,
                    data[data_index],
                    data[data_index+1],
                    data[data_index+2],
                    data[data_index+3],
                    data[data_index+4],
                    data[data_index+5],
                    data[data_index+6],
                    data[data_index+7],
                    data[data_index+8],
                    data[data_index+9]
                    );

            data_index += 10;

            if( data_index >= max_print_len ) break;
        }
    }
}

#endif  /* #ifdef SMC_APE_LINUX_KERNEL_STM */


/* =======================================================
 * DMA Implementations
 */
#ifdef SMC_DMA_ENABLED

uint8_t smc_dma_init( struct device *device )
{
    uint8_t  ret_val = SMC_ERROR;
    int      ret_drv = 0;

    SMC_TRACE_PRINTF_DMA("smc_dma_init: device 0x%08X", (uint32_t)device);

    if( device->dma_mask == NULL )
    {
            // Enable both
        device->dma_mask          = (u64*)SMC_MALLOC(sizeof(u64));
    }

    SMC_TRACE_PRINTF_DMA("smc_dma_init: device 0x%08X try to set DMA mask ptr 0x%08X to 0x%08X...", (uint32_t)device, (uint32_t)device->dma_mask, DMA_BIT_MASK(32));

    ret_drv = dma_set_mask(device, DMA_BIT_MASK(32));
    ret_drv = dma_set_coherent_mask(device, DMA_BIT_MASK(32));

    if( ret_drv == 0 )
    {
        SMC_TRACE_PRINTF_DMA("smc_dma_init: device 0x%08X DMA 32 bit mask 0x%08X set, drv returned %d", (uint32_t)device, device->dma_mask, ret_drv);

        ret_val = SMC_OK;
    }
    else
    {
        *device->dma_mask = DMA_BIT_MASK(32);
        SMC_TRACE_PRINTF_ERROR("smc_dma_init: device 0x%08X DMA 32bit mask failed, drv returned %d", (uint32_t)device, ret_drv);
    }

    return ret_val;
}

uint8_t smc_dma_uninit(struct device *device)
{
    uint8_t  ret_val = SMC_OK;

    SMC_TRACE_PRINTF_DMA("smc_dma_uninit: device 0x%08X", (uint32_t)device);

    if( device->dma_mask != NULL )
    {
        SMC_FREE( device->dma_mask );
        device->dma_mask = NULL;
    }

    return ret_val;
}


#ifdef SMC_DMA_TRANSFER_ENABLED

/* DMA completion function */

static void smc_dma_transfer_complete_cb(void *param)
{
    SMC_TRACE_PRINTF_DMA("smc_dma_transfer_complete_cb: param = 0x%08X -> complete", (uint32_t)param);

    complete(param);
}

smc_dma_t* smc_dma_create( void )
{
    smc_dma_t* dma = (smc_dma_t*)SMC_MALLOC( sizeof( smc_dma_t ) );

    assert( dma != NULL );
    //dma->device      = NULL;
    //dma->dma_channel = NULL;

    dma_cap_zero(dma->dma_cap_mask);
    dma_cap_set(DMA_SLAVE, dma->dma_cap_mask);

    dma->dma_channel = dma_request_channel(dma->dma_cap_mask, NULL, NULL);

        /*
         * DMA transfer configuration
         */
    dma->dma_ctrl_flags          = DMA_CTRL_ACK | DMA_PREP_INTERRUPT | DMA_COMPL_SKIP_DEST_UNMAP | DMA_COMPL_SKIP_SRC_UNMAP;
    dma->dma_transfer_timeout_ms = SMC_DMA_TRANSFER_TIMEOUT_MS;
    dma->completion_func         = smc_dma_transfer_complete_cb;

    init_completion(&dma->completion_conf);


    SMC_TRACE_PRINTF_DMA("smc_dma_create: dma 0x%08X initialized", dma);

    return dma;
}

int smc_dma_transfer_mdb(smc_dma_t* dma, void* target_address, void* source_address, uint32_t length, uint8_t from_mdb)
{
    int ret_value = SMC_DRIVER_OK;

    SMC_TRACE_PRINTF_DMA("smc_dma_transfer_mdb: dma 0x%08X %d bytes from 0x%08X to 0x%08X (%s MDB SHM)",
            (uint32_t)dma, length, (uint32_t)source_address, (uint32_t)target_address, from_mdb?"FROM":"TO");

    if( dma->dma_channel != NULL )
    {
        struct dma_device*              device            = dma->dma_channel->device;
        struct dma_async_tx_descriptor* dma_tx_descriptor = NULL;
        dma_addr_t dma_address_user;

        if( device == NULL )
        {
            SMC_TRACE_PRINTF_ERROR("smc_dma_transfer_mdb: invalid DMA device NULL");
            return SMC_DRIVER_ERROR;
        }

        if( from_mdb )
        {
            SMC_TRACE_PRINTF_DMA("smc_dma_transfer_mdb: prepare DMA, copy from MDB...");

            dma_address_user = dma_map_single(device->dev, target_address, length, DMA_FROM_DEVICE);

            dma_tx_descriptor = device->device_prep_dma_memcpy(dma->dma_channel, dma_address_user, source_address, length, dma->dma_ctrl_flags);
        }
        else
        {
            dma_address_user = dma_map_single(device->dev, source_address, length, DMA_TO_DEVICE);

            dma_tx_descriptor = device->device_prep_dma_memcpy(dma->dma_channel, target_address, dma_address_user, length, dma->dma_ctrl_flags);
        }

        if( dma_tx_descriptor )
        {
            dma_cookie_t      cookie;
            enum dma_status   status;
            unsigned long     tmo = msecs_to_jiffies( dma->dma_transfer_timeout_ms );

            dma_tx_descriptor->callback       = dma->completion_func;
            dma_tx_descriptor->callback_param = &dma->completion_conf;

            cookie = dma_tx_descriptor->tx_submit(dma_tx_descriptor);

            if( dma_submit_error(cookie) )
            {
                SMC_TRACE_PRINTF_ERROR("smc_dma_transfer_mdb: DMA submit error cookie");
            }

            dma_async_issue_pending(dma->dma_channel);

            tmo    = wait_for_completion_timeout(&dma->completion_conf, tmo);
            status = dma_async_is_tx_complete(dma->dma_channel, cookie, NULL, NULL);

            if (tmo == 0)
            {
                SMC_TRACE_PRINTF_ERROR("smc_dma_transfer_mdb: DMA transfer failed by timeout");
                ret_value = SMC_DRIVER_ERROR;
            }
            else if (status != DMA_SUCCESS)
            {
                SMC_TRACE_PRINTF_ERROR("smc_dma_transfer_mdb: DMA transfer failed by error %d", status);
                ret_value = SMC_DRIVER_ERROR;
            }
            else
            {
                SMC_TRACE_PRINTF_DMA("smc_dma_transfer_mdb: DMA transfer OK");
            }

            if( from_mdb )
            {
                dma_unmap_single(device->dev, dma_address_user, length, DMA_FROM_DEVICE);
            }
            else
            {
                dma_unmap_single(device->dev, dma_address_user, length, DMA_TO_DEVICE);
            }
        }
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_dma_transfer_mdb: invalid DMA channel NULL");
        ret_value = SMC_DRIVER_ERROR;
    }

    return ret_value;
}

/**
 * Initializes the device for DMA in Linux platform.
 *
 */

#if 0
void smc_dma_set_device(smc_dma_t* dma, void* parent_object)
{
    struct platform_device* platform_device = NULL;
    struct net_device*      net_device      = NULL;

    SMC_TRACE_PRINTF_DMA("smc_dma_set_device: dma 0x%08X with parent 0x%08X...", (uint32_t)dma, (uint32_t)parent_object);

        /* The parent is platform device */
    platform_device = (struct platform_device*)parent_object;

    net_device = platform_device->dev;

    SMC_TRACE_PRINTF_DMA("smc_dma_set_device: dma 0x%08X: found net device 0x%08X", (uint32_t)dma, (uint32_t)net_device);

    dma->device = net_device;
}
#endif

void smc_dma_destroy( smc_dma_t* dma )
{
    SMC_TRACE_PRINTF_DMA("smc_dma_destroy: dma 0x%08X...", (uint32_t)dma);

    /* Destroy the structure items */

    if( dma->dma_channel != NULL )
    {
        dma_release_channel(dma->dma_channel);
    }

    if( dma != NULL )
    {
        SMC_FREE( dma );
    }
}

#endif      /* SMC_DMA_TRANSFER_ENABLED */

#endif      /* SMC_DMA_ENABLED */

/* =======================================================
 * Tasklet Implementations
 */
#ifdef SMC_APE_USE_TASKLET_IN_IRQ

void smc_do_tasklet_test(unsigned long data)
{
    SMC_TRACE_PRINTF_TASKLET("smc_do_tasklet_test: tasklet scheduled -> execute");

}


/*
 * Tasklet for MHDP channel.
 */
void smc_signal_handler_tasklet_mhdp(unsigned long data)
{
    SMC_TRACE_PRINTF_TASKLET("smc_signal_handler_tasklet_mhdp: tasklet scheduled -> execute, data = 0x%08X", data);

    if( data != NULL )
    {
            /* Get the signal handler */
        smc_signal_handler_t* signal_handler = (smc_signal_handler_t*)data;

        SMC_TRACE_PRINTF_TASKLET("smc_signal_handler_tasklet_mhdp: tasklet has signal handler 0x%08X, channel ID %d", (uint32_t)signal_handler, signal_handler->smc_channel->id);

        smc_linux_interrupt_handler_int_resource_exec( signal_handler );
    }
    else
    {
        SMC_TRACE_PRINTF_ERROR("smc_signal_handler_tasklet_mhdp: no signal handler configured to tasklet");
    }
}

/*
 * Tasklet initialization for SMC IRQ.
 */
struct tasklet_struct* smc_create_tasklet(smc_signal_handler_t* signal_handler, void (*func)(unsigned long) )
{
    struct tasklet_struct* task = NULL;

    SMC_TRACE_PRINTF_TASKLET("smc_create_tasklet: create task for signal handler 0x%08X...", (uint32_t)signal_handler);

    task = kmalloc(sizeof(struct tasklet_struct),GFP_KERNEL);

    assert( task != NULL );

    tasklet_init(task, func, (unsigned long)signal_handler);

    SMC_TRACE_PRINTF_TASKLET("smc_create_tasklet: task 0x%08X created and initialized", (uint32_t)task);

    return task;
}

#endif

/*
 * SMC Wake lock implementation.
 */


#ifdef SMC_NETDEV_WAKELOCK_IN_TX

void* smc_wakelock_create( char* wakelock_name )
{
    struct wake_lock* wakelock = (struct wake_lock*)SMC_MALLOC_IRQ( sizeof( struct wake_lock ) );

    assert( wakelock != NULL );

    wake_lock_init(wakelock, WAKE_LOCK_SUSPEND, wakelock_name);

    SMC_TRACE_PRINTF_APE_WAKELOCK_TX("smc_wakelock_create: Created 0x%08X, name '%s' size %d", (uint32_t)wakelock, wakelock_name, sizeof( struct wake_lock ));

    return (void*)wakelock;
}

void smc_wakelock_destroy( void* wakelock_item, uint8_t destroy_ptr )
{
    if( wakelock_item != NULL )
    {
        struct wake_lock* wlock = (struct wake_lock*)wakelock_item;

        SMC_TRACE_PRINTF_APE_WAKELOCK_TX("smc_wakelock_destroy: destroy 0x%08X", (uint32_t)wakelock );

        wake_unlock( wlock );

        if( destroy_ptr )
        {
            SMC_TRACE_PRINTF_APE_WAKELOCK_TX("smc_wakelock_destroy: remove lock 0x%08X from wakelock list...", (uint32_t)wakelock_item);
            wake_lock_destroy( wlock );

            SMC_FREE( wakelock_item );
        }
        else
        {
            SMC_TRACE_PRINTF_APE_WAKELOCK_TX("smc_wakelock_destroy: not destroyed ptr 0x%08X size %d", (uint32_t)wakelock_item, sizeof( struct wake_lock ) );
        }
    }
}

#endif

void smc_wake_lock( uint32_t data )
{
    SMC_TRACE_PRINTF_SLEEP_CONTROL("smc_wake_lock: data: 0x%08X", data);
}

void smc_wake_unlock( uint32_t data )
{
    SMC_TRACE_PRINTF_SLEEP_CONTROL("smc_wake_unlock: data: 0x%08X", data);

}





/* EOF */
