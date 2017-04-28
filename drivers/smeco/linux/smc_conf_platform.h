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

Version:       1    21-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif


#ifndef SMC_CONF_PLATFORM_H
#define SMC_CONF_PLATFORM_H

#define SMC_SEND_USE_SEMAPHORE

/* #define SMC_XMIT_BUFFER_FAIL_SEND */
#define SMC_XMIT_BUFFER_SIZE                  15

#define SMC_LOCK_TX_BUFFER                    SMC_LOCK_IRQ
#define SMC_UNLOCK_TX_BUFFER                  SMC_UNLOCK_IRQ

#define SMC_LOCK_RX_THREADED_IRQ              SMC_LOCK_IRQ
#define SMC_UNLOCK_RX_THREADED_IRQ            SMC_UNLOCK_IRQ


#define SMC_CHANNEL_FIFO_BUFFER_SIZE_MAX      10

#define SMC_FREE_LOCAL_PTR_OS_NOT_NEEDED


    /* ===============================================
     * Define Linux Kernel Specific data types for SMC
     */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <asm/signal.h>


#ifdef SMC_CONF_PM_APE_HOST_ACCESS_REQ_ENABLED
  #if(SMC_CONF_PM_APE_HOST_ACCESS_REQ_ENABLED==TRUE)

    #define SMC_HOST_ACCESS_WAKEUP(spinlock, timeout)  { uint32_t timer = 0;                                          \
                                                         uint32_t timeout_val = timeout;                              \
                                                         if( spinlock != NULL ) SMC_LOCK_IRQ( spinlock );             \
                                                         __raw_writel(SMC_WPMCIF_EPMU_ACC_CR_MODEM_ACCESS_REQ, SMC_WPMCIF_EPMU_ACC_CR);           \
                                                         (void)__raw_readl(SMC_WPMCIF_EPMU_ACC_CR);                   \
                                                         while (SMC_WPMCIF_EPMU_ACC_CR_MODEM_ACCESS_OK != __raw_readl(SMC_WPMCIF_EPMU_ACC_CR)) {  \
                                                             if(timeout_val != 0 && ++timer >= timeout_val ) {                 \
                                                                 SMC_TRACE_PRINTF_ERROR("SMC_HOST_ACCESS_WAKEUP: modem not woken up in %d ms", timeout_val); \
                                                                 break;                                               \
                                                             }                                                        \
                                                             else if( timeout_val != 0 ) {                                 \
                                                                 SMC_SLEEP_MS(1);                                     \
                                                             }                                                        \
                                                         }                                                            \
                                                       }


    #define SMC_HOST_ACCESS_SLEEP(spinlock)            { __raw_writel(SMC_WPMCIF_EPMU_ACC_CR_MODEM_SLEEP_REQ, SMC_WPMCIF_EPMU_ACC_CR);  \
                                                        (void)__raw_readl(SMC_WPMCIF_EPMU_ACC_CR);          \
                                                        if( spinlock != NULL ) SMC_UNLOCK_IRQ( spinlock );  \
                                                       }

  #else
    #define SMC_HOST_ACCESS_WAKEUP(spinlock)
    #define SMC_HOST_ACCESS_SLEEP(spinlock)
  #endif
#else
  #error "SMC_CONF_PM_APE_HOST_ACCESS_REQ_ENABLED --- NOT DEFINED"
#endif

#define SMC_SIGNAL_TYPE_IRQ_NONE       (SMC_SIGNAL_TYPE_INTERRUPT + SMC_SIGNAL_TYPE_PRIVATE_START + 0x00)  /* 0x03000000 */
#define SMC_SIGNAL_TYPE_INTGEN         (SMC_SIGNAL_TYPE_INTERRUPT + SMC_SIGNAL_TYPE_PRIVATE_START + 0x01)  /* 0x03000001 */
#define SMC_SIGNAL_TYPE_INTCBB         (SMC_SIGNAL_TYPE_INTERRUPT + SMC_SIGNAL_TYPE_PRIVATE_START + 0x02)  /* 0x03000002 */
#define SMC_SIGNAL_TYPE_INT_WGM_GENOUT (SMC_SIGNAL_TYPE_INTERRUPT + SMC_SIGNAL_TYPE_PRIVATE_START + 0x04)  /* 0x03000004 */
#define SMC_SIGNAL_TYPE_INT_RESOURCE   (SMC_SIGNAL_TYPE_INTERRUPT + SMC_SIGNAL_TYPE_PRIVATE_START + 0x06)  /* 0x03000006 */
#define SMC_SIGNAL_TYPE_INT_IRQC       (SMC_SIGNAL_TYPE_INTERRUPT + SMC_SIGNAL_TYPE_PRIVATE_START + 0x07)  /* 0x03000007 */



    /*
     * Data type for SMC signals
     */
typedef struct _smc_signal_t
{
    uint32_t interrupt_id;
    uint32_t signal_type;
    uint32_t peripheral_address;
    uint8_t  address_remapped;
    uint8_t  event_sense;
    uint8_t  fill2;
    uint8_t  fill1;

} smc_signal_t;


typedef struct
{
  volatile uint32_t output;        /* output := <>              input := output */
  volatile uint32_t set;           /* output := output OR <>    input := None   */
  volatile uint32_t clear;         /* output := output AND ~<>  input := None   */
  volatile uint32_t toggle;        /* output := output XOR <>   input := None   */
} smc_gop001_t;


/*
 * Shared memory remapping from physical to virtual address
 */

#include <asm/irq.h>

#define SMC_SHM_IOREMAP( address, size )                        ioremap( (long unsigned int)address, size )
#define SMC_SHM_IOUNMAP( address )                              iounmap( (void __iomem *)address )

#define SMC_HW_ARM_MEMORY_SYNC(startaddress)

#define SMC_SHM_WRITE32( target_address, value )                 __raw_writel( value, ((void __iomem *)(target_address)) )
#define SMC_SHM_READ32( source_address )                         __raw_readl( ((void __iomem *)source_address) )
#define SMC_SHM_READ8( source_address )                         (__raw_readl( ((void __iomem *)source_address) )&0xFF)

#define SMC_SHM_CACHE_INVALIDATE( start_address, end_address )
#define SMC_SHM_CACHE_CLEAN( start_address, end_address )        { __raw_readl( ((void __iomem *)(start_address)) ); __raw_readl( ((void __iomem *)(end_address)) );  }


#define  SMC_LOCK( lock )        { SMC_TRACE_PRINTF_LOCK("lock: 0x%08X...", (uint32_t)lock); spin_lock( &(lock->mr_lock)); }
#define  SMC_UNLOCK( lock )      { spin_unlock( &(lock->mr_lock)); SMC_TRACE_PRINTF_LOCK("unlock: 0x%08X", (uint32_t)lock); }
#define  SMC_LOCK_IRQ( lock )    { SMC_TRACE_PRINTF_LOCK("lock irq save: 0x%08X...", (uint32_t)lock); spin_lock_irqsave( &lock->mr_lock, lock->flags); }
#define  SMC_UNLOCK_IRQ( lock )  { spin_unlock_irqrestore( &lock->mr_lock, lock->flags); SMC_TRACE_PRINTF_LOCK("unlock irq save: 0x%08X...", (uint32_t)lock); }

#define  SMC_LOCK_BH( lock )     { SMC_TRACE_PRINTF_LOCK("lock bh: 0x%08X...", (uint32_t)lock); spin_lock_bh( &(lock->mr_lock)); }
#define  SMC_UNLOCK_BH( lock )   { spin_unlock_bh( &(lock->mr_lock)); SMC_TRACE_PRINTF_LOCK("unlock bh: 0x%08X", (uint32_t)lock); }


#define  SMC_LOCK_MUTEX( smc_semaphore )        { SMC_TRACE_PRINTF_LOCK_MUTEX("mutex lock: 0x%08X...", (uint32_t)smc_semaphore); mutex_lock( &(smc_semaphore->smc_mutex) ); }
#define  SMC_UNLOCK_MUTEX( smc_semaphore )      { mutex_unlock( &(smc_semaphore->smc_mutex) ); SMC_TRACE_PRINTF_LOCK_MUTEX("mutex unlock: 0x%08X...", (uint32_t)smc_semaphore);}

    /*
     * Data type for SMC locking
     */
#include <linux/spinlock.h>

typedef struct _smc_lock_t
{
    spinlock_t    mr_lock;
    unsigned long flags;   /* For IRQ lock */

} smc_lock_t;

    /* Lockdep friendly macro for spinlock */
#define smc_lock_create()                                           \
    ({                                                              \
        smc_lock_t *lock = SMC_MALLOC_IRQ( sizeof(smc_lock_t) );    \
        assert( lock != NULL );                                     \
        spin_lock_init(&lock->mr_lock);                             \
        lock->flags = 0x00000000;                                   \
        lock;                                                       \
    })



    /*
     * Data type for SMC semaphore
     */
typedef struct _smc_semaphore_t
{
    struct mutex smc_mutex;

} smc_semaphore_t;


    /*
     * Data type for SMC timer
     */
typedef struct _smc_timer_t
{
    uint32_t           period_us;
    struct timer_list* smc_timer_list;
    uint32_t           timer_data;

    unsigned long      prev_jiffies;

    struct hrtimer*    hr_timer;            /* Highrate timer */
} smc_timer_t;




#ifdef SMC_DMA_TRANSFER_ENABLED
    /*
     * DMA structures
     */

typedef void ( *dma_transfer_cb )( void* );


typedef struct _smc_dma_t
{
    /*struct device*      device;*/

    dma_cap_mask_t      dma_cap_mask;
    struct dma_chan*    dma_channel;
    enum dma_ctrl_flags dma_ctrl_flags;

    int                 dma_transfer_timeout_ms;

    struct completion   completion_conf;

    dma_transfer_cb     completion_func;

} smc_dma_t;

int smc_dma_transfer_mdb(smc_dma_t* dma, void* target_address, void* source_address, uint32_t length, uint8_t from_mdb);

#endif

uint16_t smc_asic_version_get(void);

void smc_wake_lock( uint32_t data );
void smc_wake_unlock( uint32_t data );

#endif

/* EOF */
