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

Version:       10   09-Apr-2013     Heikki Siikaluoma
Status:        draft
Description :  New SHM SDRAM memory mapping with flagging added

Version:       1    03-Feb-2012     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMC_INSTANCE_CONFIG_R8A73734_WGE31_H
#define SMC_INSTANCE_CONFIG_R8A73734_WGE31_H


#ifdef SMECO_MODEM

  #define SMC_CPU_NAME          "Modem"

  #if (defined(NEW_MEMORY_MAP_13W14) && NEW_MEMORY_MAP_13W14==TRUE )

      #define SDRAM_SMC_START_ADDR  0x45001000
      #define SDRAM_SMC_END_ADDR    0x457FFBFF

  #else
        /* Old memory map, to be cleaned when all releases have the new SHM mapping (after 13w14)*/
      #define SDRAM_SMC_START_ADDR  0x44001000
      #define SDRAM_SMC_END_ADDR    0x44800FFF

  #endif    /* #if (defined(NEW_MEMORY_MAP_13W14) && NEW_MEMORY_MAP_13W14==TRUE ) */

#elif( defined(SMECO_LINUX_KERNEL) )

  #ifdef SMC_CONFIG_ARCH_R8A7373
      #include <mach/memory-r8a7373.h>

  #elif (defined ( SMC_CONFIG_ARCH_R8A73734 ) )
      /*#include <mach/memory-r8a73734.h>*/

      #define SDRAM_SMC_START_ADDR  0x44001000
      #define SDRAM_SMC_END_ADDR    0x44800FFF

      #define SMC_LINUX_USE_KMAP_OLD
  #else
      #warning "No target product defined, using default memory configuration"
      #define SDRAM_SMC_START_ADDR  0x44001000
      #define SDRAM_SMC_END_ADDR    0x44800FFF
  #endif

  /*
   * Other common Linux Kernel configurations
   */

  #define SMC_CPU_NAME    "APE"

#else
  #error "Error in EOS2 configuration: no valid compile option"
#endif

#define SMC_EOS_ASIC_ES20   0x20

    /* =============================================================================
     * SMC specific configurations for EOS2
     */

#define SMC_CONFIG_MASTER_NAME_SH_MOBILE_R8A7373_EOS2_ES20  "SH-Mobile-R8A7373-EOS2-ES20"
#define SMC_CONFIG_SLAVE_NAME_MODEM_WGEM31_EOS2_ES20         "WGEModem-3.1-EOS2-ES20"

#define SMC_CONF_GLOBAL_SHM_START_ES20                       SDRAM_SMC_START_ADDR
#define SMC_CONF_GLOBAL_SHM_END_ES20                         SDRAM_SMC_END_ADDR      /* 7FFFFF -> 8192 kB -> 8 MB, NOTE: Use always the last address */

    /* SHM area for SMC Control Instance */
#define SMC_CONF_CONTROL_SHM_START_OFFSET_ES20               (0)
#define SMC_CONF_CONTROL_SHM_START_ES20                      (SMC_CONF_GLOBAL_SHM_START_ES20 + SMC_CONF_CONTROL_SHM_START_OFFSET_ES20)
#define SMC_CONF_CONTROL_SHM_SIZE_ES20                       (1024*200)                  /* 200kB */

    /* SHM Area for L2MUX */
#define SMC_CONF_L2MUX_SHM_START_OFFSET_ES20                 (SMC_CONF_CONTROL_SHM_SIZE_ES20 + 64)
#define SMC_CONF_L2MUX_SHM_START_ES20                        (SMC_CONF_GLOBAL_SHM_START_ES20 + SMC_CONF_L2MUX_SHM_START_OFFSET_ES20)
#define SMC_CONF_L2MUX_SHM_SIZE_ES20                         (1024*1856*2 + 1024*256*4 + 1024*30)    /* 4.5MB */


#define SMC_CHANNEL_ID_FOR_CONFIGURATION                     0   /* Channel to used for configure others (FIFO, SHM), if not defined, no config.
                                                                    FIFO+SHM configuration of this channel is not allowed to change without parallel changes to the other device
                                                                    Currently only first channel (0) is supported.
                                                                    The master (APE) only initiate the runtime configuration and its config is used
                                                                  */

    /* =============================================================================
     * COMMON Configuration for EOS2 ES1.0/ES2.0
     */

#define SMC_CONF_PM_APE_HOST_ACCESS_REQ_ENABLED     TRUE

    /* Modem side offset for the shared memory */
#define SMC_CONF_SHM_OFFSET_TO_MODEM                (-1*0x38000000)


/* =================================================
 * Peripheral addresses and configurations
 *
 */

    /*
     * Modem side peripherals
     */
#define SMC_MODEM_INTGEN_L2_OFFSET                   16
#define SMC_MODEM_INTGEN_L2_FIRST                    35          /* C2_L2_CPU_Int_Gen_Ch0 */
#define SMC_PERIPHERAL_ADDRESS_MODEM_GOP_INTGEN_1    0x07C00040

    /*
     * APE side peripherals
     */
#define SMC_ADDRESS_APE_OFFSET_TO_MODEM              0xDC000000

#define SMC_APE_IRQ_OFFSET_INTCSYS_SPI               32         /* INTC-SYS SPI ID starts from 32 -> 255 */
#define SMC_APE_IRQ_OFFSET_INTCSYS_TO_WGM            193        /* TODO Valid only with SPI 193-198 (NOT 221 and 222)*/

#define SMC_APE_IRQ_OFFSET_IRQ_SPI                   512        /* APE IRQ offset (#define IRQPIN_IRQ_BASE 512 in irqs.h )*/


#define SMC_WPMCIF_EPMU_BASE                         IO_ADDRESS(0xE6190000)
#define SMC_WPMCIF_EPMU_ACC_CR                       (SMC_WPMCIF_EPMU_BASE + 0x0004)    /* Used for checking that modem is up */
    /* Register values of the EPMU ACC CR */
#define SMC_WPMCIF_EPMU_ACC_CR_MODEM_SLEEP_REQ       0x00000000
#define SMC_WPMCIF_EPMU_ACC_CR_MODEM_ACCESS_REQ      0x00000002
#define SMC_WPMCIF_EPMU_ACC_CR_MODEM_ACCESS_OK       0x00000003

#define SMC_HPB_BASE                                 IO_ADDRESS(0xE6000000)                         /* Used to retrieve EOS2 ASIC version */
#define SMC_CCCR                                     (SMC_HPB_BASE + 0x101C)

#if( SMC_RUNTIME_TRACES_ENABLED == TRUE )
#define SMC_APE_RDTRACE_ENABLED         /* If defined, APE handles runtime trace activation for modem stype rdtraces */
#else
#undef SMC_APE_RDTRACE_ENABLED
#endif

#define SMC_WAKEUP_USE_EXTERNAL_IRQ_MODEM                        /* If defined uses ext irq in modem side (modem wakes APE)*/

#define SMC_APE_WAKEUP_WAKELOCK_TIMEOUT_MSEC              400    /* Default wakelock timeout in milliseconds in APE IRQ (old configs: 2000/200/400<=), if 0, lock is released immediately */

/* #define SMC_WAKEUP_USE_EXTERNAL_IRQ_APE */                    /* If defined the APE SMC initializes the WUP signal (modem wakes APE)*/

#if( defined( SMC_WAKEUP_USE_EXTERNAL_IRQ_APE ) || defined( SMC_WAKEUP_USE_EXTERNAL_IRQ_MODEM ) )

        /* Configuration for Modem and APE*/
    #define SMC_APE_WAKEUP_EXTERNAL_IRQ_ID                65                                /* IRQ ID in the APE side */
    #define SMC_APE_WAKEUP_EXTERNAL_IRQ_TYPE              SMC_SIGNAL_TYPE_INT_WGM_GENOUT
    #define SMC_APE_WAKEUP_EXTERNAL_IRQ_SENSE_DEFAULT     SMC_SIGNAL_SENSE_RISING_EDGE      /* Default value if the configuration negotiation fails */
    #define SMC_APE_WAKEUP_EXTERNAL_IRQ_SENSE             SMC_SIGNAL_SENSE_FALLING_EDGE     /* SMC_SIGNAL_SENSE_FALLING_EDGE / SMC_SIGNAL_SENSE_RISING_EDGE */
    #define SMC_APE_WAKEUP_EXTERNAL_IRQ_ID_FROM_MODEM     0                                 /* Modem side interrupt genio to wakeup APE */
    #define SMC_APE_WAKEUP_EXTERNAL_IRQ_REGISTER_HANDLER  FALSE


    #define SMC_VERSION_REQUIREMENT_EXTERNAL_IRQ_POLARITY  { SMC_VERSION_TO_INT(0,0,36), SMC_VERSION_REQUIREMENT_LEVEL_WARNING, "Modem->APE wakeup signal polarity change" }

#else
    #define SMC_VERSION_REQUIREMENT_EXTERNAL_IRQ_POLARITY  {0,0,""}

#endif /* #if( defined( SMC_WAKEUP_USE_EXTERNAL_IRQ_APE ) || defined( SMC_WAKEUP_USE_EXTERNAL_IRQ_MODEM ) ) */

    /* Version requirements */
#define SMC_VERSION_REQUIREMENT_MHDP_SHM_CHANGE_0_0_37 { SMC_VERSION_TO_INT(0,0,37), SMC_VERSION_REQUIREMENT_LEVEL_ASSERT, "MHDP Channel Shared Memory size change" }

    /*
     * SMC Version requirements
     */
#define SMC_VERSION_REQUIREMENT_ARRAY  SMC_VERSION_REQUIREMENT_EXTERNAL_IRQ_POLARITY, SMC_VERSION_REQUIREMENT_MHDP_SHM_CHANGE_0_0_37
#define SMC_VERSION_REQUIREMENT_COUNT  2


/* #define SMC_CHANNEL_SYNC_WAIT_ALL */                   /* If defined the SMC instance waits for all channels to be sync'd before any communication */

/* ======================================
 * Target specific product configurations
 */
#ifdef SMECO_MODEM
    /*
     * MODEM specific configuration
     */

  /* #define SMC_BUFFER_MESSAGE_OUT_OF_FIFO_ITEMS */            /* If defined, the message is buffered when FIFO is full */
  #define SMC_BUFFER_MESSAGE_OUT_OF_MDB_MEM               /* If defined, the message is buffered when out of MDB memory */
  #define SMC_HISTORY_DATA_COLLECTION_ENABLED                   /* If defined, the SMC data transfer collection is enabled in the modem side. This has some impact to throughput performance */
  /* #define SMC_DUMP_ON_CLOSE_ENABLED */                       /* If defined, the SMC Dump is printed during SMC driver close */

      /* Definition for Modem to wakeup APE when the channel interrupt does not do that */

  #define SMC_APE_WAKEUP_WAIT_LOOP_COUNT          20     /* Wait loop timer, if not defined wakeup loop delay uses busy loop */

  #define SYSC_PSTR_REG (*(volatile uint32_t *)0x20180080)
  #define SYSC_PSTR_C4_A3SM_A2SL ((1 << 16) | (1 << 20) | (1 << 19))

  #define SMC_MODEM_WAKEUP_APE()           {                                                                                                            \
                                                volatile uint32_t sysc_pstr_data;                                                                       \
                                                volatile uint32_t sysc_poll_delay = 0;                                                                  \
                                                uint32_t sysc_poll_delay_counter = 0;                                                          \
                                                while (SYSC_PSTR_C4_A3SM_A2SL != ((sysc_pstr_data = SYSC_PSTR_REG) & SYSC_PSTR_C4_A3SM_A2SL))           \
                                                {                                                                                                       \
                                                    SMC_TRACE_PRINTF_INFO("smc_signal_raise: SYSC.PSTR C4/A3SM/A2SL is off (0x%08X)", sysc_pstr_data);  \
                                                    for (sysc_poll_delay = 0xFFF; sysc_poll_delay; sysc_poll_delay--);                                  \
                                                    sysc_poll_delay_counter++;                                                                          \
                                                    if(sysc_poll_delay_counter >= SMC_APE_WAKEUP_WAIT_LOOP_COUNT) {                                     \
                                                        SMC_TRACE_PRINTF_ERROR("smc_signal_raise: APE not detected to woken UP, counter %d", sysc_poll_delay_counter);  \
                                                        RD_TRACE_SEND1(TRA_SMC_SIGNAL_WAKEUP_APE_NOT_WOKEN_UP, 32, &sysc_poll_delay_counter);           \
                                                        break;                                                                                          \
                                                    }                                                                                                   \
                                                }                                                                                                       \
                                                SMC_TRACE_PRINTF_SIGNAL("smc_signal_raise: SYSC.PSTR C4ST C4+A3SM+A2SL is ON (0x%08X)", sysc_pstr_data);\
                                            }


  #define SMC_MODEM_GENIO_GOP001_REG    &a_genio_gop001
  #define SMC_MODEM_INTGEN_IN_USE                           /* If defined, the intgen interrupts are enabled */

#elif( defined(SMECO_LINUX_KERNEL) )
    /*
     * APE Linux Kernel specific configuration
     */

  #define SMC_BUFFER_MESSAGE_OUT_OF_FIFO_ITEMS                  /* If defined, the message is buffered when FIFO is full */
  /* #define SMC_BUFFER_MESSAGE_OUT_OF_MDB_MEM */                    /* If defined, the message is buffered when out of MDB memory */

  #define SMC_SUPPORT_SKB_FRAGMENT_UL                           /* If defined, the SKB fragments are supported in uplink. NOTE: enable also the SMC_DMA_ENABLED with this for highmem config */
  #define SMC_DMA_ENABLED                                       /* If defined, the DMA initialization is enabled, this is required by the SKB fragment handling */
  /*#define SMC_DMA_TRANSFER_ENABLED*/                          /* If defined, the DMA transfer feature is enabled for channel configuration, requires SMC_DMA_ENABLED to be defined */

  #define SMC_DMA_TRANSFER_TIMEOUT_MS            500            /* TODO Optimize */

  #define SMC_MODEM_WAKEUP_WAIT_TIMEOUT_MS       0 /* 5 */      /* Timeout in milliseconds to wait the modem to wake up. If 0, waits forever, if not defined no wakeup request from APE */

  #define SMC_HISTORY_DATA_COLLECTION_ENABLED                   /* If defined, the SMC data transfer collection is enabled in the modem side. This has some impact to throughput performance */
  #define SMC_DUMP_ON_CLOSE_ENABLED                             /* If defined, the SMC Dump is printed during SMC driver close */

      /*
       * Device driver suspend and resume macros
       */
  #define SMC_PLATFORM_DEVICE_DRIVER_SUSPEND( device, wakeup_irq_sense )    {                                                                               \
                                                                                uint32_t signal_event_sense = 0x00000002;                                   \
                                                                                if( wakeup_irq_sense == SMC_SIGNAL_SENSE_RISING_EDGE ) {                    \
                                                                                    signal_event_sense = 0x00000002; }                                      \
                                                                                else if( wakeup_irq_sense == SMC_SIGNAL_SENSE_FALLING_EDGE ) {              \
                                                                                    signal_event_sense = 0x00000001; }                                      \
                                                                                else {                                                                      \
                                                                                    SMC_TRACE_PRINTF_ASSERT("SMC_PLATFORM_DEVICE_DRIVER_SUSPEND: invalid wakeup IRQ sense value"); \
                                                                                    assert(0);                                                              \
                                                                                }                                                                           \
                                                                                __raw_writel(0x00000001, IO_ADDRESS(0xe61c2414));           /* PORT_SET */              \
                                                                                __raw_writel(signal_event_sense, IO_ADDRESS(0xe61c1980));   /* CONFIG_0 - 1 = low level detect, 2 = high level detect */ \
                                                                                __raw_writel(0x00000001, IO_ADDRESS(0xe61c1888));           /* WAKEN_SET0 */            \
                                                                            }

      /* Called from the smc_platform_device_driver_resume() */
  #define SMC_PLATFORM_DEVICE_DRIVER_RESUME( device )                       {                                                                               \
                                                                                __raw_writel(0x00000000, IO_ADDRESS(0xe61c1980)); /* CONFIG_02 - Disable Interrupt */               \
                                                                                __raw_writel(0x00000001, IO_ADDRESS(0xe61c1884)); /* WAKEN_STS0 - Disable WakeUp Request Enable */  \
                                                                            }

  #define SMC_APE_WAKEUP_WAKELOCK_USE                             /* If defined, the APE uses wakelock, otherwise there is no wakelocks at all */
  #define SMC_APE_WAKEUP_WAKELOCK_USE_TIMER                       /* If defined, the APE uses timer in wakelock while receiving packets from modem */


  #define SMC_NETDEV_WAKELOCK_IN_TX                               /* If defined, the wakelock is used in the net device TX */

  /* #define SMC_RX_USE_HIGHMEM */                                /* If defined, SMC can allocate High memory if low memory is not available */

  #define SMC_RX_MEMORY_REALLOC_TIMER_ENABLED                     /* If defined, the RX uses timer to check if there is memory available */

  #define SMC_APE_USE_THREADED_IRQ                          /* If defined, the threaded IRQ is in use in SMC RX */

  /* #define SMC_APE_USE_TASKLET_IN_IRQ */                        /* If defined, tasklet usage for IRQs is enabled */

#endif  /* End of target specific configuration */

#endif  /* #ifndef SMC_INSTANCE_CONFIG_R8A73734_WGE31_H */

/* EOF */
