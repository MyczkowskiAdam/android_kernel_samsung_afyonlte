/*
 * arch/arm/mach-shmobile/pm_ram0_tz.h
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef __PM_RAM0_H__
#define __PM_RAM0_H__

#include <mach/r8a7373.h>

#define		__EXTAL1_INFO__

#define		VMALLOC_EXPAND /* vmalloc area is expanded or not */
#undef CONFIG_PM_SMP
#if (defined(CONFIG_SMP) && (CONFIG_NR_CPUS > 1))
#define CONFIG_PM_SMP
#endif

#ifndef CONFIG_PM_SMP
#define		CORESTANDBY_A2SL /* CORESTANDBY_A2SL */
#endif

/* #define DISPLAY_LOG_DEBUG */


/*
 * Used Inter Connect RAM0 (hereafter, Internal RAM0 or RAM0) for
 *  - Function area
 *  - Backup area
 *  - SBSC clock change area
 */

/****************************************************************************/
/* RAM0 MAPPING */
/****************************************************************************/
#define ram0BasePhys	(RAM0_BASE_PHYS+0x2000)
#define ram0Base	IO_ADDRESS(ram0BasePhys)


/* Size of backup area	*/
#define	saveCommonSettingSize				0x90
#define	savePl30GlobalSettingSize			0x14
#define	saveArmMmuSettingSize				0x28

/* Size of CPU Register backup area	*/
#define	saveCpuRegisterAreaSize				0x660


/*--------------------------------------------------------------------------*/
/* Address of RAM0 area */
/* function area (RAM0  :0xE63A2000-0xE63A2FFF  MT_MEMORY_NONCACHED ) */
/* backup area   (RAM0  :0xE63A3000-0xE63A3FFF  MT_DEVICE ) */
/*--------------------------------------------------------------------------*/
/* Address of function (Virt)	*/
/* Start/end address of function area (must be page-aligned) */
#define	 ram0StartAddressOfFunctionArea	ram0Base
#define	 ram0EndAddressOfFunctionArea	ram0Backup

/* Address of function (Phys)	*/
#define	 ram0StartAddressOfFunctionAreaPhys	ram0BasePhys
#define	 ram0EndAddressOfFunctionAreaPhys	ram0BackupPhys

#define PM_FUNCTION_START	ram0StartAddressOfFunctionAreaPhys
#define PM_FUNCTION_END		ram0EndAddressOfFunctionAreaPhys

/* backup area */
#define	hoBackup		0x1000 /* Offset to the Area for backup */

/* Address of backup area top */
/* Address of backup(L) */
#define ram0Backup							\
(ram0Base		+ hoBackup)
/* Address of backup(P) */
#define ram0BackupPhys						\
(ram0BasePhys	+ hoBackup)

/* Backup area */
#define	ram0CommonSetting	ram0Backup
#define	ram0Pl310GlobalSetting				\
(ram0CommonSetting				+ saveCommonSettingSize)
#define	ram0MmuSetting0						\
(ram0Pl310GlobalSetting			+ savePl30GlobalSettingSize)
#define	ram0MmuSetting1						\
(ram0MmuSetting0				+ saveArmMmuSettingSize)
#define	ram0WakeupCodeAddr0					\
(ram0MmuSetting1				+ saveArmMmuSettingSize)
#define	ram0WakeupCodeAddr1					\
(ram0WakeupCodeAddr0			+ 0x4)
#define	ram0Cpu0RegisterArea				\
(ram0WakeupCodeAddr1			+ 0x4)
#define	ram0Cpu1RegisterArea				\
(ram0Cpu0RegisterArea			+ 0x4)
#define	ram0SpinLockVA						\
(ram0Cpu1RegisterArea			+ 0x4)
#define	ram0SpinLockPA						\
(ram0SpinLockVA					+ 0x4)
#define	ram0Cpu0Status						\
(ram0SpinLockPA					+ 0x4)
#define	ram0Cpu1Status						\
(ram0Cpu0Status					+ 0x4)
#define	ram0CpuClock						\
(ram0Cpu1Status					+ 0x4)
#define	ram0SetClockWork					\
(ram0CpuClock					+ 0x4)
#define	ram0SetClockFrqcra					\
(ram0SetClockWork				+ 0x4)
#define	ram0SetClockFrqcrb					\
(ram0SetClockFrqcra				+ 0x4)
#define	ram0SetClockFrqcrd					\
(ram0SetClockFrqcrb				+ 0x4)
#define	ram0SecHalCommaEntry				\
(ram0SetClockFrqcrd				+ 0x4)
#define ram0SecHalReturnCpu0						\
(ram0SecHalCommaEntry						+ 0x4)
#define ram0SecHalReturnCpu1						\
(ram0SecHalReturnCpu0						+ 0x4)
#define ram0ZClockFlag						\
(ram0SecHalReturnCpu1						+ 0x4)

/* Errata(ECR0285) */
#define ram0ES_2_2_AndAfter				\
(ram0ZClockFlag + 0x4)
#define ram0CPU0SpinLock					\
(ram0ES_2_2_AndAfter + 0x4)

#define ram0CPU1SpinLock					\
(ram0CPU0SpinLock + 0x4)
#define	ram0DramPasrSettingArea0			\
(ram0CPU1SpinLock				+ 0x4)
#define	ram0DramPasrSettingArea1			\
(ram0DramPasrSettingArea0		+ 0x4)
#define	ram0SaveSdmracr0a					\
(ram0DramPasrSettingArea1		+ 0x4)
#define	ram0SaveSdmracr1a					\
(ram0SaveSdmracr0a				+ 0x4)
#define	ram0SystemSuspendRestoreCPU0	\
(ram0SaveSdmracr1a + 0x4)
/*Restore point after enable MMU for System Suspend with CPU1*/
#define	ram0SystemSuspendRestoreCPU1		\
(ram0SystemSuspendRestoreCPU0	+ 0x4)
/*Restore point after enable MMU for CoreStandby with CPU0*/
#define	ram0CoreStandbyRestoreCPU0			\
(ram0SystemSuspendRestoreCPU1	+ 0x4)
/*Restore point after enable MMU for CoreStandby with CPU1*/
#define	ram0CoreStandbyRestoreCPU1			\
(ram0CoreStandbyRestoreCPU0 + 0x4)
/*Restore point after enable MMU for CoreStandby2 with CPU0*/
#define	ram0CoreStandby2RestoreCPU0			\
(ram0CoreStandbyRestoreCPU1	+ 0x4)
/*Restore point after enable MMU for CoreStandby2 with CPU1*/
#define	ram0CoreStandby2RestoreCPU1			\
(ram0CoreStandby2RestoreCPU0 + 0x4)
/*Restore point error after enable MMU for CoreStandby with CPU0*/
#define	ram0CoreStandbyRestoreCPU0_error			\
(ram0CoreStandby2RestoreCPU1	+ 0x4)
/*Restore point error after enable MMU for CoreStandby with CPU1*/
#define	ram0CoreStandbyRestoreCPU1_error			\
(ram0CoreStandbyRestoreCPU0_error + 0x4)

/*FRQCRA mask*/
#define	ram0FRQCRAMask			\
(ram0CoreStandbyRestoreCPU1_error + 0x4)

#define	ram0FRQCRADown			\
(ram0FRQCRAMask + 0x4)
#define	ram0FRQCRBDown			\
(ram0FRQCRADown + 0x4)

/* SPI Status Registers */
#define	ram0_ICSPISR0		\
(ram0FRQCRBDown + 0x4)
#define	 ram0_ICSPISR1						\
(ram0_ICSPISR0 + 0x4)

/* SBSC ioremap address */
#define ram0SBSC_SDCR0AIOremap	\
(ram0_ICSPISR1 + 0x4)
#define ram0SBSC_SDWCRC0AIOremap	\
(ram0SBSC_SDCR0AIOremap + 0x4)
#define ram0SBSC_SDWCRC1AIOremap	\
(ram0SBSC_SDWCRC0AIOremap + 0x4)
#define ram0SBSC_SDWCR00AIOremap	\
(ram0SBSC_SDWCRC1AIOremap + 0x4)
#define ram0SBSC_SDWCR01AIOremap	\
(ram0SBSC_SDWCR00AIOremap + 0x4)
#define ram0SBSC_SDWCR10AIOremap	\
(ram0SBSC_SDWCR01AIOremap + 0x4)
#define ram0SBSC_SDWCR11AIOremap	\
(ram0SBSC_SDWCR10AIOremap + 0x4)
#define ram0SBSC_SDWCRC2AIOremap	\
(ram0SBSC_SDWCR11AIOremap + 0x4)

#define ram0MmuTable	\
(ram0SBSC_SDWCRC2AIOremap + 0x4)
/* Watchdog status in suspend */
#define	ram0RwdtStatus	\
(ram0MmuTable + 0x4)
#define	ram0CMT15Status	\
(ram0RwdtStatus + 0x4)
#define	ram0SaveEXMSKCNT1_suspend	\
(ram0CMT15Status + 0x4)
#define	ram0SaveAPSCSTP_suspend		\
(ram0SaveEXMSKCNT1_suspend + 0x4)
#define	ram0SaveSYCKENMSK_suspend	\
(ram0SaveAPSCSTP_suspend + 0x4)
#define	ram0SaveC4POWCR_suspend	\
(ram0SaveSYCKENMSK_suspend + 0x4)
#define	ram0SavePDNSEL_suspend		\
(ram0SaveC4POWCR_suspend + 0x4)
#define	ram0SavePSTR_suspend		\
(ram0SavePDNSEL_suspend + 0x4)

#define	ram0SaveEXMSKCNT1_resume	\
(ram0SavePSTR_suspend + 0x4)
#define	ram0SaveAPSCSTP_resume		\
(ram0SaveEXMSKCNT1_resume + 0x4)
#define	ram0SaveSYCKENMSK_resume	\
(ram0SaveAPSCSTP_resume + 0x4)
#define	ram0SaveC4POWCR_resume		\
(ram0SaveSYCKENMSK_resume + 0x4)
#define	ram0SavePDNSEL_resume		\
(ram0SaveC4POWCR_resume + 0x4)
#define	ram0SavePSTR_resume			\
(ram0SavePDNSEL_resume + 0x4)

/* memlog for PM */
#define	ram0MemlogPmAddressPA				\
(ram0SavePSTR_resume + 0x4)
#define	ram0MemlogPmAddressVA				\
(ram0MemlogPmAddressPA + 0x4)

/* Address of ARM Vector function */
#define	ram0ArmVector					\
(ram0MemlogPmAddressVA + 0x4)
/* Address of Core Standby function */
#define	ram0CoreStandby					\
(ram0ArmVector + 0x4)
/* Address of Core Standby2 function */
#define	ram0CoreStandby_2				\
(ram0CoreStandby + 0x4)
/* Address of System Suspend function */
#define	ram0SystemSuspend				\
(ram0CoreStandby_2 + 0x4)
/* Address of Save ARM register function */
#define	ram0SaveArmRegister				\
(ram0SystemSuspend + 0x4)
/* Address of Restore ARM register function(PA) */
#define	ram0RestoreArmRegisterPA		\
(ram0SaveArmRegister + 0x4)
/* Address of Restore ARM register function(VA) */
#define	ram0RestoreArmRegisterVA		\
(ram0RestoreArmRegisterPA + 0x4)
/* Address of Save ARM common register function */
#define	ram0SaveArmCommonRegister		\
(ram0RestoreArmRegisterVA + 0x4)
/* Address of Restore ARM common register function */
#define	ram0RestoreArmCommonRegister	\
(ram0SaveArmCommonRegister + 0x4)
/* Address of PM spin lock */
#define	ram0PM_Spin_Lock				\
(ram0RestoreArmCommonRegister + 0x4)
/* Address of PM spin unlock */
#define	ram0PM_Spin_Unlock				\
(ram0PM_Spin_Lock + 0x4)
/* Address of System power down function */
#define	ram0SysPowerDown				\
(ram0PM_Spin_Unlock + 0x4)
/* Address of System power up function */
#define	ram0SysPowerUp					\
(ram0SysPowerDown + 0x4)
/* Address of memory log pm function */
#define	 ram0MemoryLogPm				\
(ram0SysPowerUp + 0x4)

/* Backup area Phys			*/
#define	ram0CommonSettingPhys				ram0BackupPhys
#define	ram0Pl310GlobalSettingPhys			\
(ram0CommonSettingPhys			+ saveCommonSettingSize)
#define	ram0MmuSetting0Phys					\
(ram0Pl310GlobalSettingPhys		+ savePl30GlobalSettingSize)
#define	ram0MmuSetting1Phys					\
(ram0MmuSetting0Phys			+ saveArmMmuSettingSize)
#define	ram0WakeupCodeAddr0Phys				\
(ram0MmuSetting1Phys			+ saveArmMmuSettingSize)
#define	ram0WakeupCodeAddr1Phys				\
(ram0WakeupCodeAddr0Phys		+ 0x4)
#define	ram0Cpu0RegisterAreaPhys			\
(ram0WakeupCodeAddr1Phys		+ 0x4)
#define	ram0Cpu1RegisterAreaPhys			\
(ram0Cpu0RegisterAreaPhys		+ 0x4)
#define	ram0SpinLockVAPhys					\
(ram0Cpu1RegisterAreaPhys		+ 0x4)
#define	ram0SpinLockPAPhys					\
(ram0SpinLockVAPhys				+ 0x4)
#define	ram0Cpu0StatusPhys					\
(ram0SpinLockPAPhys				+ 0x4)
#define	ram0Cpu1StatusPhys					\
(ram0Cpu0StatusPhys				+ 0x4)
#define	ram0CpuClockPhys					\
(ram0Cpu1StatusPhys				+ 0x4)
#define	ram0SetClockWorkPhys				\
(ram0CpuClockPhys				+ 0x4)
#define	ram0SetClockFrqcraPhys				\
(ram0SetClockWorkPhys			+ 0x4)
#define	ram0SetClockFrqcrbPhys				\
(ram0SetClockFrqcraPhys			+ 0x4)
#define	ram0SetClockFrqcrdPhys				\
(ram0SetClockFrqcrbPhys			+ 0x4)
#define	ram0SecHalCommaEntryPhys			\
(ram0SetClockFrqcrdPhys			+ 0x4)
#define ram0SecHalReturnCpu0Phys			\
(ram0SecHalCommaEntryPhys						+ 0x4)
#define ram0SecHalReturnCpu1Phys			\
(ram0SecHalReturnCpu0Phys					+ 0x4)
#define ram0ZClockFlagPhys						\
(ram0SecHalReturnCpu1Phys						+ 0x4)

/* Errata(ECR0285) */
#define ram0ES_2_2_AndAfterPhys				\
(ram0ZClockFlagPhys + 0x4)
#define ram0CPU0SpinLockPhys					\
(ram0ES_2_2_AndAfterPhys + 0x4)

#define ram0CPU1SpinLockPhys					\
(ram0CPU0SpinLockPhys + 0x4)
#define	ram0DramPasrSettingArea0Phys		\
(ram0CPU1SpinLockPhys		+ 0x4)
#define	ram0DramPasrSettingArea1Phys		\
(ram0DramPasrSettingArea0Phys	+ 0x4)
#define	ram0SaveSdmracr0aPhys				\
(ram0DramPasrSettingArea1Phys	+ 0x4)
#define	ram0SaveSdmracr1aPhys				\
(ram0SaveSdmracr0aPhys			+ 0x4)
#define	ram0SystemSuspendRestoreCPU0Phys	\
(ram0SaveSdmracr1aPhys + 0x4)
/* Restore point after enable MMU for System Suspend with CPU1 */
#define	ram0SystemSuspendRestoreCPU1Phys	\
(ram0SystemSuspendRestoreCPU0Phys	+ 0x4)
/* Restore point after enable MMU for CoreStandby with CPU0 */
#define	ram0CoreStandbyRestoreCPU0Phys		\
(ram0SystemSuspendRestoreCPU1Phys	+ 0x4)
/* Restore point after enable MMU for CoreStandby with CPU1 */
#define	ram0CoreStandbyRestoreCPU1Phys		\
(ram0CoreStandbyRestoreCPU0Phys + 0x4)
/* Restore point after enable MMU for CoreStandby2 with CPU0 */
#define	ram0CoreStandby2RestoreCPU0Phys		\
(ram0CoreStandbyRestoreCPU1Phys	+ 0x4)
/* Restore point after enable MMU for CoreStandby2 with CPU1 */
#define	ram0CoreStandby2RestoreCPU1Phys		\
(ram0CoreStandby2RestoreCPU0Phys + 0x4)
/*Restore point error after enable MMU for CoreStandby with CPU0*/
#define	ram0CoreStandbyRestoreCPU0_errorPhys			\
(ram0CoreStandby2RestoreCPU1Phys	+ 0x4)
/*Restore point error after enable MMU for CoreStandby with CPU1*/
#define	ram0CoreStandbyRestoreCPU1_errorPhys			\
(ram0CoreStandbyRestoreCPU0_errorPhys + 0x4)

/*FRQCRA mask*/
#define	ram0FRQCRAMaskPhys			\
(ram0CoreStandbyRestoreCPU1_errorPhys + 0x4)
/*FRQCRA mask*/
#define	ram0FRQCRADownPhys			\
(ram0FRQCRAMaskPhys + 0x4)
#define	ram0FRQCRBDownPhys			\
(ram0FRQCRADownPhys + 0x4)

/* SPI Status Registers */
#define	ram0_ICSPISR0Phys		\
(ram0FRQCRBDownPhys + 0x4)
#define	 ram0_ICSPISR1Phys					\
(ram0_ICSPISR0Phys + 0x4)

/* SBSC ioremap address */
#define ram0SBSC_SDCR0AIOremapPhys	\
(ram0_ICSPISR1Phys + 0x4)
#define ram0SBSC_SDWCRC0AIOremapPhys	\
(ram0SBSC_SDCR0AIOremapPhys + 0x4)
#define ram0SBSC_SDWCRC1AIOremapPhys	\
(ram0SBSC_SDWCRC0AIOremapPhys + 0x4)
#define ram0SBSC_SDWCR00AIOremapPhys	\
(ram0SBSC_SDWCRC1AIOremapPhys + 0x4)
#define ram0SBSC_SDWCR01AIOremapPhys	\
(ram0SBSC_SDWCR00AIOremapPhys + 0x4)
#define ram0SBSC_SDWCR10AIOremapPhys	\
(ram0SBSC_SDWCR01AIOremapPhys + 0x4)
#define ram0SBSC_SDWCR11AIOremapPhys	\
(ram0SBSC_SDWCR10AIOremapPhys + 0x4)
#define ram0SBSC_SDWCRC2AIOremapPhys	\
(ram0SBSC_SDWCR11AIOremapPhys + 0x4)

#define ram0MmuTablePhys	\
(ram0SBSC_SDWCRC2AIOremapPhys + 0x4)

#define ram0RwdtStatusPhys	\
(ram0MmuTablePhys + 0x4)
#define ram0CMT15StatusPhys	\
(ram0RwdtStatusPhys + 0x4)
#define	ram0SaveEXMSKCNT1Phys_suspend	\
(ram0CMT15StatusPhys + 0x4)
#define	ram0SaveAPSCSTPPhys_suspend	\
(ram0SaveEXMSKCNT1Phys_suspend + 0x4)
#define	ram0SaveSYCKENMSKPhys_suspend	\
(ram0SaveAPSCSTPPhys_suspend + 0x4)
#define	ram0SaveC4POWCRPhys_suspend	\
(ram0SaveSYCKENMSKPhys_suspend + 0x4)
#define	ram0SavePDNSELPhys_suspend		\
(ram0SaveC4POWCRPhys_suspend + 0x4)
#define	ram0SavePSTRPhys_suspend		\
(ram0SavePDNSELPhys_suspend + 0x4)

#define	ram0SaveEXMSKCNT1Phys_resume	\
(ram0SavePSTRPhys_suspend + 0x4)
#define	ram0SaveAPSCSTPPhys_resume		\
(ram0SaveEXMSKCNT1Phys_resume + 0x4)
#define	ram0SaveSYCKENMSKPhys_resume	\
(ram0SaveAPSCSTPPhys_resume + 0x4)
#define	ram0SaveC4POWCRPhys_resume		\
(ram0SaveSYCKENMSKPhys_resume + 0x4)
#define	ram0SavePDNSELPhys_resume		\
(ram0SaveC4POWCRPhys_resume + 0x4)
#define	ram0SavePSTRPhys_resume		\
(ram0SavePDNSELPhys_resume + 0x4)

/* memlog for PM */
#define	ram0MemlogPmAddressPAPhys			\
(ram0SavePSTRPhys_resume + 0x4)
#define	ram0MemlogPmAddressVAPhys			\
(ram0MemlogPmAddressPAPhys + 0x4)

/* Address of ARM Vector function */
#define	ram0ArmVectorPhys					\
(ram0MemlogPmAddressVAPhys + 0x4)
/* Address of Core Standby function */
#define	ram0CoreStandbyPhys					\
(ram0ArmVectorPhys + 0x4)
/* Address of Core Standby2 function */
#define	ram0CoreStandby_2Phys				\
(ram0CoreStandbyPhys + 0x4)
/* Address of System Suspend function */
#define	ram0SystemSuspendPhys				\
(ram0CoreStandby_2Phys + 0x4)
/* Address of Save ARM register function */
#define	ram0SaveArmRegisterPhys				\
(ram0SystemSuspendPhys + 0x4)
/* Address of Restore ARM register function(PA) */
#define	ram0RestoreArmRegisterPAPhys		\
(ram0SaveArmRegisterPhys + 0x4)
/* Address of Restore ARM register function(VA) */
#define	ram0RestoreArmRegisterVAPhys		\
(ram0RestoreArmRegisterPAPhys + 0x4)
/* Address of Save ARM common register function */
#define	ram0SaveArmCommonRegisterPhys		\
(ram0RestoreArmRegisterVAPhys + 0x4)
/* Address of Restore ARM common register function */
#define	ram0RestoreArmCommonRegisterPhys	\
(ram0SaveArmCommonRegisterPhys + 0x4)
/* Address of PM spin lock */
#define	ram0PM_Spin_LockPhys				\
(ram0RestoreArmCommonRegisterPhys + 0x4)
/* Address of PM spin unlock */
#define	ram0PM_Spin_UnlockPhys				\
(ram0PM_Spin_LockPhys + 0x4)
/* Address of System power down function */
#define	ram0SysPowerDownPhys				\
(ram0PM_Spin_UnlockPhys + 0x4)
/* Address of System power up function */
#define	ram0SysPowerUpPhys					\
(ram0SysPowerDownPhys + 0x4)
/* Address of memory log pm function */
#define	 ram0MemoryLogPmPhys				\
(ram0SysPowerUpPhys + 0x4)

/*------------------------------------------------*/
/* Offset of CPU register buckup area */
/*	Defining the offset of allocated memory area. */
/*	Subject to the offset address is stored */
/*	in ram0Cpu0RegisterArea/ram0Cpu1RegisterArea. */
/*------------------------------------------------*/
/* Backup setting(CPU register)	*/
#define	hoSaveArmSvc			0
#define	hoSaveArmExceptSvc		(hoSaveArmSvc + 0x4)
#define	hoSaveArmVfp			(hoSaveArmExceptSvc + 0x4)
#define	hoSaveArmGic			(hoSaveArmVfp + 0x4)
#define	hoSaveArmTimer			(hoSaveArmGic + 0x4)
#define	hoSaveArmSystem			(hoSaveArmTimer + 0x4)
#define	hoSaveArmPerformanceMonitor	(hoSaveArmSystem + 0x4)
#define	hoSaveArmCti			(hoSaveArmPerformanceMonitor + 0x4)
#define	hoSaveArmPtm			(hoSaveArmCti + 0x4)
#define	hoSaveArmDebug			(hoSaveArmPtm + 0x4)
#define	hoBackupAddr			(hoSaveArmDebug	+ 0x4)
#define	hoDataArea				(hoBackupAddr + 0x4)

/*-----------------------------------------*/
/* Definition of CPU status	*/
/*----------------------------------------*/
#define CPUSTATUS_RUN				0x0
#define CPUSTATUS_WFI				0x1
#define CPUSTATUS_SHUTDOWN			0x3
#define CPUSTATUS_WFI2				0x4
#define CPUSTATUS_HOTPLUG			0x5
#define CPUSTATUS_SHUTDOWN2			0x6
/*----------------------------------------------*/
/* Definition parameters of sec_hal_pm_coma_entry()*/
/*----------------------------------------------*/
/* Mode */
#define COMA_MODE_INITIAL_STATE			0x0
#define COMA_MODE_SUSPEND				0x1
#define COMA_MODE_CORE_STANDBY			0x2
#define COMA_MODE_SLEEP					0x3
#define COMA_MODE_HOTPLUG				0x4
#define COMA_MODE_SLEEP_2				0x5
#define COMA_MODE_CORE_STANDBY_2		0x6

/* Frequency */
#define CLK_NOCHANGED				0x00
#define PLL0_OFF					0x01
#define EXTAL2_OFF					0x02
#define Z_CLK_CHANGED				0x04
#define ZG_CLK_CHANGED				0x08
#define ZTR_CLK_CHANGED				0x10
#define ZT_CLK_CHANGED				0x20
#define ZX_CLK_CHANGED				0x40
#define ZS_CLK_CHANGED				0x80
#define HP_CLK_CHANGED				0x100
#define I_CLK_CHANGED				0x200
#define B_CLK_CHANGED				0x400
#define M1_CLK_CHANGED				0x800
#define M3_CLK_CHANGED				0x1000
#define M5_CLK_CHANGED				0x2000
#define ZB3_CLK_CHANGED				0x4000

#define CORESTANDBY_CLK_CHANGED	(PLL0_OFF | Z_CLK_CHANGED | CLK_NOCHANGED)

#define SUSPEND_CLK_CHANGED	(PLL0_OFF | EXTAL2_OFF | Z_CLK_CHANGED | \
ZG_CLK_CHANGED | ZTR_CLK_CHANGED | ZT_CLK_CHANGED | ZX_CLK_CHANGED | \
ZS_CLK_CHANGED | HP_CLK_CHANGED | I_CLK_CHANGED | B_CLK_CHANGED | \
M1_CLK_CHANGED | M3_CLK_CHANGED | M5_CLK_CHANGED | ZB3_CLK_CHANGED)
/*#define SUSPEND_CLK_CHANGED					0x03FFB */

/* Return value */
#define SEC_HAL_RES_OK				0x00000000
#define SEC_HAL_RES_FAIL			0x00000010
#define SEC_HAL_FREQ_FAIL			0x00000020

#endif /* __PM_RAM0_H__ */
