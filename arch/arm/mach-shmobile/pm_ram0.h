/*
 * arch/arm/mach-shmobile/pm_ram0.h
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

/* #define __EXTAL1_INFO__ */

#undef CONFIG_PM_SMP
#if (defined(CONFIG_SMP) && (CONFIG_NR_CPUS > 1))
#define CONFIG_PM_SMP
#endif

#ifndef CONFIG_PM_SMP
#define CORESTANDBY_A2SL /* CORESTANDBY_A2SL */
#endif


/*
 * Used Inter Connect RAM0/1 (hereafter, Internal RAM0/1 or RAM0/1) for
 *  - Function area
 *  - Backup area
 */

/****************************************************************************/
/* RAM0/RAM1/SECRAM MAPPING */
/****************************************************************************/
#define ram0BasePhys	(RAM0_BASE_PHYS+0x2000)
#define ram0Base	IO_ADDRESS(ram0BasePhys)

#define secramBasePhys	0xE6300000	/* Secure RAM Base physical address */
#define secramBase	IO_ADDRESS(secramBasePhys)

/* Size of backup area	*/
#define saveCommonSettingSize		0x90
#define savePl30GlobalSettingSize	0x14
#define saveArmMmuSettingSize		0x28

/* Size of CPU Register backup area	*/
#define saveCpuRegisterAreaSize		0x660

/*--------------------------------------------------------------------------*/
/* Address of RAM0/SECRAM area */
/* function area (SECRAM:0xE6300000-0xE6301FFF) */
/* backup area   (RAM0  :0xE63A2000-0xE63A3FFF) */
/*--------------------------------------------------------------------------*/
/* Address of function (Virt)	*/
/* Start/end address of function area (must be page-aligned) */
#define	 secramStartAddressOfFunctionArea	secramBase
#define	 secramEndAddressOfFunctionArea		(secramBase+0x2000)

/* Address of function (Phys)	*/
#define	 secramStartAddressOfFunctionAreaPhys	secramBasePhys
#define	 secramEndAddressOfFunctionArea		(secramBasePhys+0x2000)

#define PM_FUNCTION_START	secramStartAddressOfFunctionAreaPhys
#define PM_FUNCTION_END		secramEndAddressOfFunctionAreaPhys

/* backup area */
#define hoBackup	0x0000 /* Offset to the Area for backup */

/* Address of backup area top	*/
/* Address of backup(L) */
#define ram0Backup							\
(ram0Base		+ hoBackup)
/* Address of backup(P) */
#define ram0BackupPhys						\
(ram0BasePhys	+ hoBackup)

/* Backup area */
#define ram0CommonSetting ram0Backup
#define ram0Pl310GlobalSetting				\
(ram0CommonSetting + saveCommonSettingSize)
#define ram0MmuSetting0						\
(ram0Pl310GlobalSetting + savePl30GlobalSettingSize)
#define ram0MmuSetting1						\
(ram0MmuSetting0 + saveArmMmuSettingSize)
#define ram0WakeupCodeAddr0					\
(ram0MmuSetting1 + saveArmMmuSettingSize)
#define ram0WakeupCodeAddr1					\
(ram0WakeupCodeAddr0 + 0x4)
#define ram0Cpu0RegisterArea				\
(ram0WakeupCodeAddr1 + 0x4)
#define ram0Cpu1RegisterArea				\
(ram0Cpu0RegisterArea + 0x4)
#define ram0SpinLockVA						\
(ram0Cpu1RegisterArea + 0x4)
#define ram0SpinLockPA						\
(ram0SpinLockVA + 0x4)
#define ram0Cpu0Status						\
(ram0SpinLockPA + 0x4)
#define ram0Cpu1Status						\
(ram0Cpu0Status + 0x4)
#define ram0SetClockWork					\
(ram0Cpu1Status + 0x4)
#define ram0SetClockFrqcra					\
(ram0SetClockWork + 0x4)
#define ram0SetClockFrqcrb					\
(ram0SetClockFrqcra + 0x4)
#define ram0SetClockFrqcrd					\
(ram0SetClockFrqcrb + 0x4)

/* Errata(ECR0285) */
#define ram0ES_2_2_AndAfter				\
(ram0SetClockFrqcrd + 0x4)

#define ram0CPU0SpinLock					\
(ram0ES_2_2_AndAfter + 0x4)
#define ram0CPU1SpinLock					\
(ram0CPU0SpinLock + 0x4)

#define ram0DramPasrSettingArea0			\
(ram0CPU1SpinLock + 0x4)
#define ram0DramPasrSettingArea1			\
(ram0DramPasrSettingArea0 + 0x4)
#define ram0SaveSdmracr0a					\
(ram0DramPasrSettingArea1 + 0x4)
#define ram0SaveSdmracr1a					\
(ram0SaveSdmracr0a + 0x4)

/* SPI Status Registers */
#define ram0_ICSPISR0				\
(ram0SaveSdmracr1a + 0x4)
#define  ram0_ICSPISR1				\
(ram0_ICSPISR0 + 0x4)
/*FRQCRA mask*/
#define ram0FRQCRAMask				\
(ram0_ICSPISR1 + 0x4)
/*FRQCRA Down*/
#define ram0FRQCRADown				\
(ram0FRQCRAMask + 0x4)
#define	ram0FRQCRBDown			\
(ram0FRQCRADown + 0x4)

#define ram0MmuTable	\
(ram0FRQCRBDown + 0x4)
/* Watchdog status in suspend */
#define ram0RwdtStatus	\
(ram0MmuTable + 0x4)
#define	ram0SaveEXMSKCNT1_suspend 	\
(ram0RwdtStatus + 0x4)
#define ram0SaveAPSCSTP_suspend		\
(ram0SaveEXMSKCNT1_suspend + 0x4)
#define ram0SaveSYCKENMSK_suspend	\
(ram0SaveAPSCSTP_suspend + 0x4)
#define ram0SaveC4POWCR_suspend		\
(ram0SaveSYCKENMSK_suspend + 0x4)
#define ram0SavePDNSEL_suspend		\
(ram0SaveC4POWCR_suspend + 0x4)
#define ram0SavePSTR_suspend		\
(ram0SavePDNSEL_suspend + 0x4)

#define ram0SaveEXMSKCNT1_resume	\
(ram0SavePSTR_suspend + 0x4)
#define ram0SaveAPSCSTP_resume		\
(ram0SaveEXMSKCNT1_resume + 0x4)
#define ram0SaveSYCKENMSK_resume	\
(ram0SaveAPSCSTP_resume + 0x4)
#define ram0SaveC4POWCR_resume		\
(ram0SaveSYCKENMSK_resume + 0x4)
#define ram0SavePDNSEL_resume		\
(ram0SaveC4POWCR_resume + 0x4)
#define ram0SavePSTR_resume			\
(ram0SavePDNSEL_resume + 0x4)


/*Restore point after enable MMU for System Suspend with CPU0*/
#define	ram0SystemSuspendRestoreCPU0	\
(ram0SavePSTR_resume + 0x4)
/*Restore point after enable MMU for System Suspend with CPU1*/
#define ram0SystemSuspendRestoreCPU1		\
(ram0SystemSuspendRestoreCPU0 + 0x4)
/*Restore point after enable MMU for CoreStandby with CPU0*/
#define ram0CoreStandbyRestoreCPU0			\
(ram0SystemSuspendRestoreCPU1 + 0x4)
/*Restore point after enable MMU for CoreStandby with CPU1*/
#define ram0CoreStandbyRestoreCPU1			\
(ram0CoreStandbyRestoreCPU0 + 0x4)
/*Restore point after enable MMU for CoreStandby2 with CPU0*/
#define	ram0CoreStandby2RestoreCPU0			\
(ram0CoreStandbyRestoreCPU1	+ 0x4)
/*Restore point after enable MMU for CoreStandby2 with CPU1*/
#define	ram0CoreStandby2RestoreCPU1			\
(ram0CoreStandby2RestoreCPU0 + 0x4)
/* Do ZQ Calibration or not */
#define	ram0ZQCalib		\
(ram0CoreStandby2RestoreCPU1 + 0x4)

/* memlog for PM */
#define	ram0MemlogPmAddressPA				\
(ram0ZQCalib + 0x4)
#define	ram0MemlogPmAddressVA				\
(ram0MemlogPmAddressPA + 0x4)

/* Address of ARM Vector function */
#define	secramArmVector					\
(ram0MemlogPmAddressVA + 0x4)
/* Address of PM spin lock */
#define	secramPM_Spin_Lock				\
(secramArmVector + 0x4)
/* Address of PM spin unlock */
#define	secramPM_Spin_Unlock			\
(secramPM_Spin_Lock + 0x4)
/* Address of Disable MMU function */
#define	secramDisableMMU				\
(secramPM_Spin_Unlock + 0x4)
/* Address of Restore ARM register function(PA) */
#define	secramRestoreArmRegisterPA		\
(secramDisableMMU + 0x4)
/* Address of Restore common register function */
#define	secramRestoreCommonRegister		\
(secramRestoreArmRegisterPA + 0x4)
/* Address of System power down function */
#define	secramSysPowerDown				\
(secramRestoreCommonRegister + 0x4)
/* Address of System power up function */
#define	secramSysPowerUp				\
(secramSysPowerDown + 0x4)
/* Address of System suspend CPU0(PA) function */
#define	secramSystemSuspendCPU0PA		\
(secramSysPowerUp + 0x4)
/* Address of CoreStandby(PA) function */
#define	secramCoreStandbyPA				\
(secramSystemSuspendCPU0PA + 0x4)
/* Address of CoreStandby 2(PA) function */
#define	secramCoreStandbyPA2			\
(secramCoreStandbyPA + 0x4)
/* Address of System suspend CPU1(PA) function */
#define	secramSystemSuspendCPU1PA		\
(secramCoreStandbyPA2 + 0x4)
/* Address of CoreStandby down status function */
#define	secramCorestandbyDownStatus		\
(secramSystemSuspendCPU1PA + 0x4)
/* Address of CoreStandby up status function */
#define	secramCorestandbyUpStatus		\
(secramCorestandbyDownStatus + 0x4)
/* Address of memory log pm function */
#define	 secramMemoryLogPm				\
(secramCorestandbyUpStatus + 0x4)

/* Backup area Phys */
#define ram0CommonSettingPhys		ram0BackupPhys
#define ram0Pl310GlobalSettingPhys			\
(ram0CommonSettingPhys			+ saveCommonSettingSize)
#define ram0MmuSetting0Phys					\
(ram0Pl310GlobalSettingPhys		+ savePl30GlobalSettingSize)
#define ram0MmuSetting1Phys					\
(ram0MmuSetting0Phys			+ saveArmMmuSettingSize)
#define ram0WakeupCodeAddr0Phys				\
(ram0MmuSetting1Phys			+ saveArmMmuSettingSize)
#define ram0WakeupCodeAddr1Phys				\
(ram0WakeupCodeAddr0Phys	+ 0x4)
#define ram0Cpu0RegisterAreaPhys			\
(ram0WakeupCodeAddr1Phys	+ 0x4)
#define ram0Cpu1RegisterAreaPhys			\
(ram0Cpu0RegisterAreaPhys	+ 0x4)
#define ram0SpinLockVAPhys					\
(ram0Cpu1RegisterAreaPhys	+ 0x4)
#define ram0SpinLockPAPhys					\
(ram0SpinLockVAPhys			+ 0x4)
#define ram0Cpu0StatusPhys					\
(ram0SpinLockPAPhys			+ 0x4)
#define ram0Cpu1StatusPhys					\
(ram0Cpu0StatusPhys			+ 0x4)
#define ram0SetClockWorkPhys				\
(ram0Cpu1StatusPhys			+ 0x4)
#define ram0SetClockFrqcraPhys				\
(ram0SetClockWorkPhys		+ 0x4)
#define ram0SetClockFrqcrbPhys				\
(ram0SetClockFrqcraPhys		+ 0x4)
#define ram0SetClockFrqcrdPhys				\
(ram0SetClockFrqcrbPhys		+ 0x4)

/* Errata(ECR0285) */
#define ram0ES_2_2_AndAfterPhys				\
(ram0SetClockFrqcrdPhys + 0x4)

#define ram0CPU0SpinLockPhys					\
(ram0ES_2_2_AndAfterPhys + 0x4)

#define ram0CPU1SpinLockPhys					\
(ram0CPU0SpinLockPhys + 0x4)

#define ram0DramPasrSettingArea0Phys		\
(ram0CPU1SpinLockPhys			+ 0x4)
#define ram0DramPasrSettingArea1Phys		\
(ram0DramPasrSettingArea0Phys	+ 0x4)
#define ram0SaveSdmracr0aPhys				\
(ram0DramPasrSettingArea1Phys	+ 0x4)
#define ram0SaveSdmracr1aPhys				\
(ram0SaveSdmracr0aPhys			+ 0x4)

/* SPI Status Registers */
#define ram0_ICSPISR0Phys				\
(ram0SaveSdmracr1aPhys + 0x4)
#define  ram0_ICSPISR1Phys				\
(ram0_ICSPISR0Phys + 0x4)

/*FRQCRA mask*/
#define ram0FRQCRAMaskPhys				\
(ram0_ICSPISR1Phys + 0x4)
/*FRQCRA mask*/
#define ram0FRQCRADownPhys				\
(ram0FRQCRAMaskPhys + 0x4)
#define	ram0FRQCRBDownPhys			\
(ram0FRQCRADownPhys + 0x4)

#define ram0MmuTablePhys	\
(ram0FRQCRBDownPhys + 0x4)
/* Watchdog status in suspend */
#define ram0RwdtStatusPhys	\
(ram0MmuTablePhys + 0x4)
#define ram0SaveEXMSKCNT1Phys_suspend	\
(ram0RwdtStatusPhys + 0x4)
#define ram0SaveAPSCSTPPhys_suspend		\
(ram0SaveEXMSKCNT1Phys_suspend + 0x4)
#define ram0SaveSYCKENMSKPhys_suspend	\
(ram0SaveAPSCSTPPhys_suspend + 0x4)
#define ram0SaveC4POWCRPhys_suspend		\
(ram0SaveSYCKENMSKPhys_suspend + 0x4)
#define ram0SavePDNSELPhys_suspend		\
(ram0SaveC4POWCRPhys_suspend + 0x4)
#define ram0SavePSTRPhys_suspend		\
(ram0SavePDNSELPhys_suspend + 0x4)

#define ram0SaveEXMSKCNT1Phys_resume	\
(ram0SavePSTRPhys_suspend + 0x4)
#define ram0SaveAPSCSTPPhys_resume		\
(ram0SaveEXMSKCNT1Phys_resume + 0x4)
#define ram0SaveSYCKENMSKPhys_resume	\
(ram0SaveAPSCSTPPhys_resume + 0x4)
#define ram0SaveC4POWCRPhys_resume		\
(ram0SaveSYCKENMSKPhys_resume + 0x4)
#define ram0SavePDNSELPhys_resume		\
(ram0SaveC4POWCRPhys_resume + 0x4)
#define ram0SavePSTRPhys_resume			\
(ram0SavePDNSELPhys_resume + 0x4)


/* Restore point after enable MMU for System Sleep with CPU0 */
#define ram0SystemSuspendRestoreCPU0Phys	\
(ram0SavePSTRPhys_resume + 0x4)

/* Restore point after enable MMU for System Sleep with CPU1 */
#define ram0SystemSuspendRestoreCPU1Phys	\
(ram0SystemSuspendRestoreCPU0Phys + 0x4)
/* Restore point after enable MMU for CoreStandby with CPU0 */
#define ram0CoreStandbyRestoreCPU0Phys		\
(ram0SystemSuspendRestoreCPU1Phys + 0x4)
/* Restore point after enable MMU for CoreStandby with CPU1 */
#define ram0CoreStandbyRestoreCPU1Phys		\
(ram0CoreStandbyRestoreCPU0Phys + 0x4)
/* Restore point after enable MMU for CoreStandby2 with CPU0 */
#define	ram0CoreStandby2RestoreCPU0Phys		\
(ram0CoreStandbyRestoreCPU1Phys	+ 0x4)
/* Restore point after enable MMU for CoreStandby2 with CPU1 */
#define	ram0CoreStandby2RestoreCPU1Phys		\
(ram0CoreStandby2RestoreCPU0Phys + 0x4)
/* Do ZQ Calibration or not */
#define	ram0ZQCalibPhys		\
(ram0CoreStandby2RestoreCPU1Phys + 0x4)

/* memlog for PM */
#define	ram0MemlogPmAddressPAPhys			\
(ram0ZQCalibPhys + 0x4)
#define	ram0MemlogPmAddressVAPhys			\
(ram0MemlogPmAddressPAPhys + 0x4)

/* Address of ARM Vector function */
#define	secramArmVectorPhys					\
(ram0MemlogPmAddressVAPhys + 0x4)
/* Address of PM spin lock */
#define	secramPM_Spin_LockPhys				\
(secramArmVectorPhys + 0x4)
/* Address of PM spin unlock */
#define	secramPM_Spin_UnlockPhys			\
(secramPM_Spin_LockPhys + 0x4)
/* Address of Disable MMU function */
#define	secramDisableMMUPhys				\
(secramPM_Spin_UnlockPhys + 0x4)
/* Address of Restore ARM register function(PA) */
#define	secramRestoreArmRegisterPAPhys		\
(secramDisableMMUPhys + 0x4)
/* Address of Restore common register function */
#define	secramRestoreCommonRegisterPhys		\
(secramRestoreArmRegisterPAPhys + 0x4)
/* Address of System power down function */
#define	secramSysPowerDownPhys				\
(secramRestoreCommonRegisterPhys + 0x4)
/* Address of System power up function */
#define	secramSysPowerUpPhys				\
(secramSysPowerDownPhys + 0x4)
/* Address of System suspend CPU0(PA) function */
#define	secramSystemSuspendCPU0PAPhys		\
(secramSysPowerUpPhys + 0x4)
/* Address of CoreStandby(PA) function */
#define	secramCoreStandbyPAPhys				\
(secramSystemSuspendCPU0PAPhys + 0x4)
/* Address of CoreStandby 2(PA) function */
#define	secramCoreStandbyPA2Phys			\
(secramCoreStandbyPAPhys + 0x4)
/* Address of System suspend CPU1(PA) function */
#define	secramSystemSuspendCPU1PAPhys		\
(secramCoreStandbyPA2Phys + 0x4)
/* Address of CoreStandby down status function */
#define	secramCorestandbyDownStatusPhys		\
(secramSystemSuspendCPU1PAPhys + 0x4)
/* Address of CoreStandby up status function */
#define	secramCorestandbyUpStatusPhys		\
(secramCorestandbyDownStatusPhys + 0x4)
/* Address of memory log pm function */
#define	 ram0MemoryLogPmPhys				\
(secramCorestandbyUpStatusPhys + 0x4)

/*-----------------------------------------------*/
/* Offset of CPU register buckup area */
/* Defining the offset of allocated memory area. */
/* Subject to the offset address is stored */
/* in ram0Cpu0RegisterArea/ram0Cpu1RegisterArea. */
/*-----------------------------------------------*/
/* Backup setting(CPU register)	*/
#define hoSaveArmSvc				0
#define hoSaveArmExceptSvc			(hoSaveArmSvc + 0x4)
#define hoSaveArmVfp				(hoSaveArmExceptSvc + 0x4)
#define hoSaveArmGic				(hoSaveArmVfp + 0x4)
#define hoSaveArmTimer				(hoSaveArmGic + 0x4)
#define hoSaveArmSystem				(hoSaveArmTimer + 0x4)
#define hoSaveArmPerformanceMonitor	(hoSaveArmSystem + 0x4)
#define hoSaveArmCti		(hoSaveArmPerformanceMonitor + 0x4)
#define hoSaveArmPtm				(hoSaveArmCti + 0x4)
#define hoSaveArmDebug				(hoSaveArmPtm + 0x4)
#define hoBackupAddr				(hoSaveArmDebug + 0x4)
#define hoDataArea					(hoBackupAddr + 0x4)

/*-------------------------------------------------*/
/* Definition of CPU status */
/*-------------------------------------------------*/
#define CPUSTATUS_RUN				0x0
#define CPUSTATUS_WFI				0x1
#define CPUSTATUS_SHUTDOWN			0x3
#define CPUSTATUS_WFI2				0x4
#define CPUSTATUS_HOTPLUG			0x5
#define CPUSTATUS_SHUTDOWN2			0x6
#endif /* __PM_RAM0_H__ */
