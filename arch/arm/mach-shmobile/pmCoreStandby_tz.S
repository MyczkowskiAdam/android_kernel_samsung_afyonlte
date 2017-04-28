/*
 * arch/arm/mach-shmobile/pmCoreStandby_tz.S
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
#include <linux/linkage.h>
#include <linux/rmu2_cmt15.h>
#include <asm/assembler.h>
#include <memlog/memlog.h>
#include <mach/cmt.h>

#include "pm_ram0_tz.h"
#include "pmRegisterDef.h"
#include "pmMacro.S"
#include "pmCachemaintenance.S"

	.align	5
/********************************************************/
/*	memory log for PM */
/* This function is running on icram0 */
/* Enter with: */
/* 	r0 = identifier */
/* 	lr = Address of return point */
/********************************************************/
ENTRY(memory_log_pm)
#ifdef CONFIG_MEMLOG
	MRC		p15, 0, r1, c1, c0, 0	/* System Control Register */
	TST		r1, #0x1
	LDRNE	r1, =ram0MemlogPmAddressVA
	LDRNE	r3, =CMCNT0
	LDREQ	r1, =ram0MemlogPmAddressPAPhys
	LDREQ	r3, =IO_TO_PHYS(CMCNT0)
	LDR		r1, [r1]

	MRC	p15, 0, r2, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r2, r2, #0, #2 /* Extract the CPUID Field */
	CMP		r2, #0x0
	ADDNE	r1, r1, #CPU0_PM_SIZE

	/* store time stamp */
	LDR		r2, [r3]
	STR		r2, [r1]

	/* store identifier */
	STR		r0, [r1, #0x4]
	DSB

#endif
	MOV		pc, lr

.pool
ENDPROC(memory_log_pm)
	.align	5
MEMORY_LOG_PM_END:

/*==========================================*/
/*	Name: PM_Spin_Lock						*/
/*	Overview: Get PM SpinLock 				*/
/*==========================================*/
ENTRY(PM_Spin_Lock)
#ifdef CONFIG_PM_SMP
	MRC		p15, 0, r7, c1, c0, 0	/* System Control Register */
	TST		r7, #0x1
	LDRNE	r9, =ram0ES_2_2_AndAfter
	LDREQ	r9, =ram0ES_2_2_AndAfterPhys
	LDR 	r9, [r9]

	MOV		r4, #0x1

	TST		r7, #0x1
	LDRNE	r5, =ram0SpinLockVA
	LDREQ	r5, =ram0SpinLockPAPhys
	LDR		r5, [r5]

	CMP		r9, #0x1
	BEQ		2f	/* es = 2.0.2 */

3: /* es > 2.0.2 */
	LDREX	r6, [r5]
	CMP		r6, #0x0
	/* WFENE */
	STREXEQ	r6, r4, [r5]
	CMPEQ	r6, #0x0
	BNE		3b
	B	SPIN_LOCK_GET1_END

2:
	LDR	r6, [r5]
	CMP	r6, #0x0
	BNE	2b
	DMB

	MRC	p15, 0, r8, c0, c0, 5	/* Read MPIDR */
	ADD		r4, r4, r8 /* Different value for each CPU */

1:
	LDREX	r6, [r5]
	CMP		r6, #0x0
	/* WFENE */
	STREXEQ	r6, r4, [r5]
	LDR		r6, [r5]
	CMPEQ	r6, r4
	BNE		1b

SPIN_LOCK_GET1_END:
	DMB
	mov pc, lr
#endif

.pool
ENDPROC(PM_Spin_Lock)
	.align	5
PM_SPIN_LOCK_END:

/*==========================================*/
/*	Name: PM_Spin_Unlock					*/
/*	Overview: Release PM SpinLock 			*/
/*==========================================*/
ENTRY(PM_Spin_Unlock)
#ifdef CONFIG_PM_SMP
	DMB
	MOV		r4, #0x0
	MRC		p15, 0, r7, c1, c0, 0	/* System Control Register */
	TST		r7, #0x1
	LDRNE	r5, =ram0SpinLockVA
	LDREQ	r5, =ram0SpinLockPAPhys
	LDR		r5, [r5]
	STR		r4, [r5]
	DSB
	/* SEV */
	MOV		pc, lr
#endif

.pool
ENDPROC(PM_Spin_Unlock)
	.align	5
PM_SPIN_UNLOCK_END:

/********************************************************/
/*	Transition to coreStandby(A1SL0 or A1Sl1 Power Off) */
/* This function is running on SDRAM */
/* Enter with: */
/* 	r0 = this pointer */
/* 	lr = Address of return point */
/********************************************************/
ENTRY(start_corestandby)
	STMFD	sp!, {r4-r11, lr}

	MRC	p15, 0, r11, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r11, r11, #0, #2 /* Extract the CPUID Field */

	/*--------------------------------------------------------------*/
	/* Initialize the management information of backup area */
	/*--------------------------------------------------------------*/
	CMP		r11, #0 /* if(CPUID == 0) */
	LDREQ	r4, =ram0Cpu0RegisterArea
	LDRNE	r4, =ram0Cpu1RegisterArea

	LDR		r4, [r4]
	LDR		r5, =hoDataArea
	MOV		r0, r4
	LDR 	r1, =0x0
	MOV		r2, r5
	BL		memset
	ADD		r6, r4, r5
	LDR		r7, =hoBackupAddr
	STR		r6, [r4, r7]

	/*--------------------------------------*/
	/* Jump to CoreStandby() on RAM0		*/
	/*--------------------------------------*/
	LDR		r4, =ram0CoreStandby
	LDR		r4, [r4]
	MOV		lr, pc			/* lr: Return address */
	MOV		pc, r4			/* Call corestandby() */

	LDMFD	sp!, {r4-r11, pc}

.pool
ENDPROC(start_corestandby)
	.align	5

/************************************************************/
/*	Transition to System Suspend(A3SM Power Off)			*/
/* This function is running on SDRAM */
/* Enter with: */
/* r0 = this pointer */
/* lr = Address of return point */
/************************************************************/
ENTRY(jump_systemsuspend)
	STMFD	sp!, {r4-r11, lr}

	MRC	p15, 0, r11, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX r11, r11, #0, #2 /* Extract the CPUID Field */

	/*------------------------------*/
	/* Initialize the backup area	*/
	/*------------------------------*/
	CMP		r11, #0 /* if(CPUID == 0) */
	LDREQ	r4, =ram0Cpu0RegisterArea
	LDRNE	r4, =ram0Cpu1RegisterArea
	LDR		r4, [r4]
	LDR		r5, =hoDataArea
	MOV		r0, r4
	LDR 	r1, =0x0
	MOV		r2, r5
	BL		memset
	ADD		r6, r4, r5
	LDR		r7, =hoBackupAddr
	STR		r6, [r4, r7]

	/*------------------------------------------*/
	/* Initialize the return value of SEC_HAL	*/
	/*------------------------------------------*/
	MRC	p15, 0, r11, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r11, r11, #0, #2 /* Extract the CPUID Field */
	CMP		r11, #0 /* if(CPUID == 0) */
	LDREQ	r4, =ram0SecHalReturnCpu0
	LDRNE	r4, =ram0SecHalReturnCpu1
	LDR 	r5, =0x0
	STR		r5, [r4]

	/*------------------------------------------*/
	/* Jump to systemsuspend() on RAM0			*/
	/* r4: systemsuspend() address on RAM0		*/
	/*------------------------------------------*/
	LDR		r4, =ram0SystemSuspend
	LDR		r4, [r4]
	MOV		lr, pc
	MOV		pc, r4	/* Call systemsuspend()	*/

	LDMFD	sp!, {r4-r11, pc}

.pool
ENDPROC(jump_systemsuspend)
	.align	5

/********************************************************************/
/* Vector table */
/* This function is running on RAM0 (Inter connect RAM0) */
/* After modification of this function, please check the offset. */
/* Enter with: */
/********************************************************************/
ENTRY(ArmVector)
	B		WAKEUP_ENTRY	/* RESET */
	NOP						/* UNDEF */
	NOP						/* SVC */
	NOP						/* PABT	*/
	NOP						/* DABT	*/
	NOP						/* RESERVED */
	NOP						/* IRQ */
	NOP						/* FIQ */
WAKEUP_EXCEPTLOOP:
	B		WAKEUP_EXCEPTLOOP
WAKEUP_ENTRY:		/* Entry point for wake up core	*/

	LDR		r0, =PM_PM_ID_ARMVECTOR
	LDR		r4, =ram0MemoryLogPmPhys
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	ADD		pc, r4, #ram0BasePhys - ram0Base

	/* Enable the I-Cache and branch prediction */
	MCR		p15, 0, r0, c7, c5, 0 /* Invalidate I-Cache to PoC */
	MCR		p15, 0, r0, c7, c5, 6 /* Invalidate branch predictor */
	MOV		r0, #0x1800
	MCR	p15, 0, r0, c1, c0, 0 /* Enable I-Cache and branch prediction */
	ISB
	MCR	p15, 0, r0, c8, c7, 0 /* Invalidate entire unified TLB */
	ISB

	/* Jump to wakeup code MMU off */
	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r4, r4, #0, #2 /* Extract the CPUID Field */
	CMP		r4, #0x0
	LDREQ	r3, =ram0WakeupCodeAddr0Phys
	LDRNE	r3, =ram0WakeupCodeAddr1Phys
	LDR		r1, [r3]
	MOV		pc,	r1 /* Jump to WakeUp Code */

.pool
ENDPROC(ArmVector)
	.align	5
ARM_VECTOR_END:

/*****************************************************************/
/*	Transition to coreStandby(A1SL0 or A1Sl1 Power Off) */
/* This function is running on RAM0 */
/* After modification of this function, please check the offset. */
/* Enter with: */
/* 	r0 = this pointer */
/* 	lr = Address of return point */
/****************************************************************/
ENTRY(corestandby)
	STMFD	sp!, {r4-r11, lr}

	LDR		r0, =PM_PM_ID_CORESTANDBY_IN
	LDR		r4, =ram0MemoryLogPm
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call memory_log_pm */
	NOP

	/*-----------------------------------*/
	/* Clear WUPSFAC */
	/*---------------------------------*/
	LDR		r4, =WUPSFAC
	LDR		r4, [r4] /* Clear WUPSFAC */

	/*------------------------------------------*/
	/* Save ARM registers						*/
	/*------------------------------------------*/
	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r4, r4, #0, #2 /* Extract the CPUID Field */
	CMP		r4, #0x0
	LDREQ	r0, =ram0Cpu0RegisterArea /* r0: Backup area for CPU0 */
	LDRNE	r0, =ram0Cpu1RegisterArea /* r0: Backup area for CPU1 */
	LDR		r0, [r0]
	LDR		r4, =ram0SaveArmRegister
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call save_arm_register() */
	NOP

#ifdef CONFIG_PM_SMP
	/* Get PM SpinLock				*/
	LDR		r4, =ram0PM_Spin_Lock
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc,r4 /* Call PM_Spin_Lock() */
	NOP

	/* Update current cpu status			*/
	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r0, r4, #0x0, #0x2 /* Extract the CPUID Field */
	CMP		r0, #0x0
	LDREQ	r5, =ram0Cpu0Status
	LDRNE	r5, =ram0Cpu1Status
	LDR		r6, =CPUSTATUS_SHUTDOWN
	STR		r6, [r5]
	DSB

	/* Release PM SpinLock				*/
	LDR		r4, =ram0PM_Spin_Unlock
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc,r4 /* Call PM_Spin_UnLock() */
	NOP
#endif

	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r4, r4, #0, #2 /* Extract the CPUID Field */
	CMP		r4, #0x0
	LDREQ	r0, =ram0CoreStandbyRestoreCPU0
	LDRNE	r0, =ram0CoreStandbyRestoreCPU1
		/* r0: Backup area of resume point after MMU is enable */
	ADR		r4, CORESTANDBY_RESTORE
	STR		r4, [r0]
	DSB

	/* Check IRQ */
	LDR		r6, =1023
	LDR		r5, =BaseInterruptIF
	LDR		r3, [r5, #ICC_HPIR]
	AND		r3, r3, r6
	CMP		r3, r6
	BNE		CORESTANDBY_RESTORE

	/*-----------------------------------*/
	/* CPU power down preparation for wakeup code MMU off */
	/*----------------------------------*/
	ADR		r5, CORESTANDBY_WAKEUP
	ADD		r5, r5, #ram0BasePhys - ram0Base
	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r4, r4, #0x0, #0x2 /* Extract the CPUID Field */
	CMP		r4, #0x0
	LDREQ	r6, =ram0WakeupCodeAddr0
	LDRNE	r6, =ram0WakeupCodeAddr1
	STR		r5, [r6] /* Set the CPU wakeup code address	*/
	DSB

	LDR		r0, =COMA_MODE_CORE_STANDBY /* mode */
	LDR		r1, =ram0ArmVector
	LDR		r1, [r1]
	LDR		r2, =0x0
	LDR		r3, =0x0

	LDR		r5, =ram0SecHalCommaEntry
	LDR		r5, [r5]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r5 /* Call sec_hal_pm_coma_entry() */
	NOP

	/* Save return value of sec_hal_pm_coma_entry() for error case only */
	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r4, r4, #0, #2 /* Extract the CPUID Field */
	CMP		r4, #0x0
	LDREQ	r5, =ram0SecHalReturnCpu0
	LDRNE	r5, =ram0SecHalReturnCpu1
	STR		r0, [r5] /* Save return value */
	DSB
	B		CORESTANDBY_RESTORE

CORESTANDBY_WAKEUP:
	NOP

	/*------------------------------------------*/
	/* Restore ARM registers					*/
	/*------------------------------------------*/
	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r4, r4, #0, #2 /* Extract the CPUID Field */
	CMP		r4, #0x0
	LDREQ	r0, =ram0MmuSetting0Phys
	LDRNE	r0, =ram0MmuSetting1Phys

	LDREQ	r1, =ram0CoreStandbyRestoreCPU0Phys
	LDRNE	r1, =ram0CoreStandbyRestoreCPU1Phys
	LDR		r1, [r1]

	LDR		r4, =ram0RestoreArmRegisterPAPhys
	LDR		r4, [r4]
	MOV		pc,r4 /* Call restore_arm_register_pa() */

CORESTANDBY_RESTORE:

#ifdef CONFIG_PM_SMP
	/* Get PM SpinLock				*/
	LDR		r4, =ram0PM_Spin_Lock
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc,r4 /* Call PM_Spin_Lock() */

	/* Update current cpu status			*/
	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r4, r4, #0, #2 /* Extract the CPUID Field */
	CMP		r4, #0x0
	LDREQ	r5, =ram0Cpu0Status
	LDRNE	r5, =ram0Cpu1Status
	LDR		r6, =CPUSTATUS_RUN
	STR		r6, [r5]
	DSB

	/* Release PM SpinLock				*/
	LDR		r4, =ram0PM_Spin_Unlock
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc,r4 /* Call PM_Spin_UnLock() */
	NOP
#endif

	/*-----------------------------------*/
	/* Restore ARM registers 			*/
	/*---------------------------------*/
	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r4, r4, #0, #2 /* Extract the CPUID Field */
	CMP		r4, #0x0
	LDREQ	r0, =ram0Cpu0RegisterArea
	LDRNE	r0, =ram0Cpu1RegisterArea
	LDR		r0, [r0]
	LDR		r4, =ram0RestoreArmRegisterVA
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call restore_arm_register_va */
	NOP

	LDR		r0, =PM_PM_ID_CORESTANDBY_OUT
	LDR		r4, =ram0MemoryLogPm
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call memory_log_pm */
	NOP

	LDMFD	sp!, {r4-r11,pc}

.pool
ENDPROC(corestandby)
	.align	5
CORESTANDBY_END:

/************************************************/
/* Transition to System Suspend(A3SM Power Off) */
/* This function is running on RAM0 */
/* Enter with: */
/* 	r0 = this pointer */
/* 	lr = Address of return point */
/***********************************************/
ENTRY(systemsuspend)
	STMFD	sp!, {r4-r11, lr}

	/* Check CPU ID	*/
	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX r4, r4, #0, #2		/* Extract the CPUID Field */
	CMP		r4, #0			/* if(CPUID == 0) */
	BNE		CPU1_SYSTEMSUSPEND

	/*==================================*/
	/* CPU 0 System Suspend				*/
	/*==================================*/
CPU0_SYSTEMSUSPEND:

	LDR		r0, =PM_PM_ID_SUSPEND_IN
	LDR		r4, =ram0MemoryLogPm
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call memory_log_pm */
	NOP

	/*--------------------------------------*/
	/* Clear WUPSFAC						*/
	/*--------------------------------------*/
	LDR		r6, =WUPSFAC
	LDR		r6, [r6]		/* Clear WUPSFAC */
	/*--------------------------------------*/
	/* Save ARM common registers			*/
	/*--------------------------------------*/
	LDR		r0, =ram0Cpu0RegisterArea /* r0: backup area */
	LDR		r0, [r0]
	LDR		r4, =ram0SaveArmCommonRegister
	LDR		r4, [r4]
	MOV		lr, pc
	MOV		pc, r4	/* Call save_arm_common_register()	*/

	/*--------------------------------------*/
	/* Save ARM registers					*/
	/*--------------------------------------*/
	LDR		r0, =ram0Cpu0RegisterArea /* r0: backup area */
	LDR		r0, [r0]
	LDR		r4, =ram0SaveArmRegister
	LDR		r4, [r4]
	MOV		lr, pc
	MOV		pc, r4	/* Call save_arm_register()	*/

	ADR		r4, SYSTEMSUSPEND_RESTORE_CPU0
	LDR		r0, =ram0SystemSuspendRestoreCPU0
	STR		r4, [r0]

	/*------------------*/
	/* Sys power down	*/
	/*------------------*/
	LDR		r4, =ram0SysPowerDown
	LDR		r4, [r4]
	/* NB sys_powerdown returns to LR with MMU off if successful, or */
	/* to ram0SystemSuspendRestoreCPU0 with MMU on if unsuccessful */
	MOV		lr, pc
	MOV		pc, r4		/* Call sys_powerdown	*/

	/*------------------*/
	/* Sys power up		*/
	/*------------------*/
	LDR		r4, =ram0SysPowerUpPhys
	LDR		r4, [r4]
	MOV		lr, pc
	MOV		pc, r4		/* Call sys_powerup		*/

	/*--------------------------------------------------------------*/
	/* Restore ARM registers */
	/* r1: Virtual address of next instruction after enabling MMU	*/
	/*--------------------------------------------------------------*/
	LDR		r1, =ram0SystemSuspendRestoreCPU0Phys
	LDR		r1, [r1]

	LDR		r0, =ram0MmuSetting0Phys
	LDR		r4, =ram0RestoreArmRegisterPAPhys
	LDR		r4, [r4]
	MOV		pc, r4	/* Call restore_arm_register_pa()	*/

SYSTEMSUSPEND_RESTORE_CPU0:
	/* Deal with error case
	 * SEC_HAL_RES_FAIL (0x10)
	 */
	LDR		r9, =ram0SecHalReturnCpu0
	LDR		r0, [r9]

	/*----------------------------------*/
	/* Restore ARM registers			*/
	/*----------------------------------*/
	LDR		r0, =ram0Cpu0RegisterArea
	LDR		r0, [r0]
	LDR		r4, =ram0RestoreArmRegisterVA
	LDR		r4, [r4]
	MOV		lr, pc		/* lr: Return address */
	MOV		pc, r4		/* Call restore_arm_register_va */
	NOP

	/*------------------------------*/
	/* Restore ARM common registers	*/
	/*------------------------------*/
	LDR		r0, =ram0Cpu0RegisterArea
	LDR		r0, [r0]
	LDR		r4, =ram0RestoreArmCommonRegister
	LDR		r4, [r4]
	MOV		lr, pc	/* lr: Return address	*/
	MOV		pc, r4	/* Call restore_arm_common_register	*/
	NOP

	LDR		r0, =PM_PM_ID_SUSPEND_OUT
	LDR		r4, =ram0MemoryLogPm
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call memory_log_pm */
	NOP

	B		SYSTEMSUSPEND_EXIT

	/*======================*/
	/* CPU 1 System Suspend	*/
	/*======================*/
CPU1_SYSTEMSUSPEND:

	LDR		r0, =PM_PM_ID_HOTPLUG_IN
	LDR		r4, =ram0MemoryLogPm
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call memory_log_pm */
	NOP

	/*----------------------*/
	/* Save ARM registers	*/
	/*----------------------*/
	LDR		r0, =ram0Cpu1RegisterArea	/* r0: backup area */
	LDR		r0, [r0]
	LDR		r4, =ram0SaveArmRegister
	LDR		r4, [r4]
	MOV		lr, pc		/* lr: Return address */
	MOV		pc, r4		/* Call save_arm_register()	*/
	NOP

	ADR		r4, SYSTEMSUSPEND_RESTORE_CPU1
	LDR		r0, =ram0SystemSuspendRestoreCPU1
	STR		r4, [r0]

	/* Get PM SpinLock				*/
	LDR		r4, =ram0PM_Spin_Lock
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call PM_Spin_Lock() */
	NOP

	/* Update current cpu status			*/

	LDR		r5, =ram0Cpu1Status
	LDR		r6, =CPUSTATUS_HOTPLUG
	STR		r6, [r5]
	DSB

/* Release PM SpinLock				*/
	LDR		r4, =ram0PM_Spin_Unlock
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc,r4 /* Call PM_Spin_UnLock() */
	NOP


	/*------------------------------------------*/
	/* Call sec_hal_pm_coma_entry					*/
	/*------------------------------------------*/
	LDR		r4, =is_suspend_request
	LDR		r5, [r4]
	CMP		r5, #0x1
	LDREQ	r0, =COMA_MODE_SUSPEND
	LDRNE	r0, =COMA_MODE_HOTPLUG
	LDR		r1, =ram0ArmVector
	LDR		r1, [r1]
	LDR 	r2, =0x0
	LDR 	r3, =0x0

	LDR		r5, =ram0SecHalCommaEntry
	LDR		r4, [r5]

	ADR		r5, SECURE_SUSPEND_WAKEUP_CPU1
	ADD		r5, r5, #ram0BasePhys - ram0Base
	LDR		r7, =ram0WakeupCodeAddr1
	STR 	r5, [r7]
	MOV		lr, pc
	MOV		pc, r4		/* CPU1 down */

	/* Deal with error case
	 * SEC_HAL_RES_FAIL (0x10)
	 */
	LDR		r9, =ram0SecHalReturnCpu1
	STR		r0, [r9]
	B		SYSTEMSUSPEND_RESTORE_CPU1

SECURE_SUSPEND_WAKEUP_CPU1:

	/*--------------------------------------------------------------*/
	/* Restore ARM registers (enable MMU) */
	/* r1: Virtual address of next instruction after enabling MMU	*/
	/*--------------------------------------------------------------*/
	LDR		r1, =ram0SystemSuspendRestoreCPU1Phys
	LDR		r1, [r1]

	LDR		r0, =ram0MmuSetting1Phys
	LDR		r4, =ram0RestoreArmRegisterPAPhys
	LDR		r4, [r4]
	MOV		pc, r4	/* Call restore_arm_register_pa()	*/

SYSTEMSUSPEND_RESTORE_CPU1:

	/* Get PM SpinLock				*/
	LDR		r4, =ram0PM_Spin_Lock
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call PM_Spin_Lock() */

	/* Update current cpu status			*/
	LDR		r5, =ram0Cpu1Status
	LDR		r6, =CPUSTATUS_RUN
	STR		r6, [r5]
	DSB

/* Release PM SpinLock				*/
	LDR		r4, =ram0PM_Spin_Unlock
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call PM_Spin_UnLock() */

	/*------------------------------------------*/
	/* Restore ARM registers */
	/*------------------------------------------*/
	LDR		r0, =ram0Cpu1RegisterArea
	LDR		r0, [r0]
	LDR		r4, =ram0RestoreArmRegisterVA
	LDR		r4, [r4]
	MOV		lr, pc		/* lr: Return address */
	MOV		pc, r4		/* Call restore_arm_register_va */
	NOP

	LDR		r0, =PM_PM_ID_HOTPLUG_OUT
	LDR		r4, =ram0MemoryLogPm
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call memory_log_pm */
	NOP

SYSTEMSUSPEND_EXIT:

	LDMFD	sp!, {r4-r11,pc}

.pool
ENDPROC(systemsuspend)
	.align	5
SYSTEMSUSPEND_END:

/****************************************/
/* Save ARM register					*/
/* This function is running on RAM0		*/
/* Enter with: 							*/
/* 	r0:backup area						*/
/* 	lr:return address					*/
/****************************************/
ENTRY(save_arm_register)

	/*------------------------------*/
	/* Read backup start address	*/
	/*------------------------------*/
	MOV		r12, r0 /* r12 = backup area(manage area base address) */
	LDR		r0, [r12, #hoBackupAddr] /* Read backup start address */

	/*------------------------------*/
	/* Save SVC mode context		*/
	/*------------------------------*/
	STR		r0, [r12, #hoSaveArmSvc]
	MRS		r4, CPSR /* Get CPSR */
	/* Ensure all exceptions disabled now. In secure world, watchdog FIQs
	 * could still happen until R13_fiq is modified */
	CPSID	aif
	MRS		r5, SPSR /* Get SPSR */
	STMIA	r0!, {r4-r5, r13} /* Save CPSR,SPSR,sp */

	/*----------------------------------------------*/
	/* Save SYS,ABT,UND,IRQ,FIQ mode context		*/
	/*----------------------------------------------*/
	STR		r0, [r12, #hoSaveArmExceptSvc]
	CPS		#POWER_MODESYS /* Switch to SYS mode */
	STMIA	r0!, {r13-r14} /* Save sp,lr */
	CPS		#POWER_MODEIRQ /* Switch to IRQ mode */
	MRS		r4, SPSR /* Get SPSR */
	STMIA	r0!, {r4, r13-r14} /* Save SPSR,sp,lr */
	CPS		#POWER_MODEABT /* Switch to ABT mode */
	MRS		r4, SPSR /* Get SPSR */
	STMIA	r0!, {r4, r13-r14} /* Save SPSR,sp,lr */
	CPS		#POWER_MODEUND /* Switch to UND mode */
	MRS		r4, SPSR /* Get SPSR */
	STMIA	r0!, {r4, r13-r14} /* Save SPSR,sp,lr */
	CPS		#POWER_MODEFIQ /* Switch to FIQ mode */
	MRS		r4, SPSR /* Get SPSR */
	STMIA	r0!, {r4, r8-r14} /* Save SPSR,r8-r12,sp,lr */
	LDR		r13, =0xDEADDEAD /* Magic word for secure code */
	CPS		#POWER_MODESVC /* switch to SVC mode */

	/*-----------------------------------*/
	/* Save VFP	*/
	/*---------------------------------*/
	MRC	p15, 0, r4, c1, c1, 2
	/* Read Non-secure Access Control Register */
	UBFX	r5, r4, #0xA, #0x2 /* Extract the CP10 and CP11 Fields */
	CMP		r5, #0x3
	BNE		SAVE_VFP_SKIP

	LDR		r4, =hoSaveArmVfp
	STR		r0, [r12, r4]
	MRC	p15, 0, r3, c1, c0, 2
	/* Read Coprocessor Access Control register */
	ORR		r4, r3, #0xF00000 /* Allows CP10 and CP11 access */
	MCR	p15, 0, r4, c1, c0, 2
	/* Write Coprocessor Access Control register */
	ISB		/* Insrtuction Synchronization Barrier */
	MRS		r4, FPEXC /* Read FPEXC register(VMRS -> MRS) */
	STR		r4, [r0], #0x4 /* Save FPEXC register */
	MOV		r5, #0x1
	MOV		r6, r5, LSL #0x1E /* Set EN bit to 1. */
	MSR		FPEXC, r6
	/* Write FPEXC (Set global enable bit to 1) (VMSR -> MSR)*/
	MRS		r7, FPSCR /* Read FPSCR register(VMRS -> MRS) */
	STR		r7, [r0], #0x4 /* Save FPSCR register */
	VSTM	r0!, {D0-D15} /* Save VFP-D16 register */
	MRS		r5, MVFR0 /* Read MVFR0 register(VMRS -> MRS) */
	AND		r6, r5, #0xF /* Extract the RB field */
	TEQ	r6, #0x2 /* Check for Advanced SIMD/VFP-D32 support	*/
	.word(POWER_VSTMIA(POWER_CC_EQ, 0, 16, 16)) /* Save VFP-D32 */
	MSR		FPEXC, r4 /* Restore FPEXC register(VMSR -> MSR) */
	MCR	p15, 0, r3, c1, c0, 2
	/* Restore Coprocessor Access Control register */

SAVE_VFP_SKIP:
	NOP

	/*----------------------------------------------*/
	/* Save System(MMU) */
	/*----------------------------------------------*/
	LDR		r1, =ram0MmuSetting0
	MRC		p15, 0, r4, c0, c0, 5
	UBFX	r5, r4, #0x0, #0x2
	LDR		r6, =saveArmMmuSettingSize
	MUL		r7, r5, r6
	ADD		r1, r1, r7
	MRC	p15, 0, r3, c10, c2, 0 /* Primary Region Remap Register */
	MRC	p15, 0, r4, c10, c2, 1 /* Normal Memory Remap Register */
	MRC	p15, 0, r5, c2, c0, 0 /* Translation Table Base 0 Register */
	MRC	p15, 0, r6, c2, c0, 1 /* Translation Table Base 1 Register */
	/* Translation Table Base Control Register */
	MRC	p15, 0, r7, c2, c0, 2
	MRC	p15, 0, r8, c3, c0, 0 /* Domain Access Control Register */
	MRC	p15, 0, r9, c13, c0, 1 /* Context ID Register */
	MRC	p15, 0, r10, c1, c0, 0 /* System Control Register */
	STMIA	r1!, {r3-r10}

	/*----------------------------------------------*/
	/* Save System */
	/*----------------------------------------------*/
	LDR		r4,  =hoSaveArmSystem
	STR		r0, [r12, r4]
	MRC	p15, 0, r4, c12, c0, 0 /* Vector Base Address Register */
	MRC	p15, 0, r5, c7, c4, 0 /* Physical Address Register */
	MRC	p15, 0, r6, c13, c0, 2 /* User Read/Write Thread ID Register */
	MRC	p15, 0, r7, c13, c0, 3
			/* User Read-only Thread ID Register	*/
	MRC	p15, 0, r8, c13, c0, 4
			/* Privileged Only ThreadID Register	*/
	MRC	p15, 2, r9, c0, c0, 0 /* Cache Size Selection Register */

	STMIA	r0!, {r4-r9}

	/*----------------------------------------------*/
	/* Update backup start address					*/
	/*----------------------------------------------*/
	LDR		r4, =hoBackupAddr
	STR		r0, [r12, r4]

	MOV		pc, lr

.pool
ENDPROC(save_arm_register)
	.align	5
SAVE_ARM_REGISTER_END:

/*****************************************************************/
/* Restore ARM register(running in MMU disable) */
/* This function is running on RAM0	*/
/* After modification of this function, please check the offset. */
/* Enter with: */
/*	r0:backup area */
/*	lr:return address */
/*****************************************************************/
ENTRY(restore_arm_register_pa)
	/*----------------------------------------------*/
	/* System(MMU) */
	/*----------------------------------------------*/
	LDR	lr, =ram0MmuTablePhys
	LDR	lr, [lr]
	LDMIA	r0!, {r3-r10}
	MCR		p15, 0, r9, c13, c0, 1 /* Context ID Register */
	ISB
	MCR	p15, 0, r8, c3, c0, 0 /* Domain Access Control Register */
	/* Translation Table Base Control Register */
	MCR	p15, 0, r7, c2, c0, 2
	MCR	p15, 0, r6, c2, c0, 1 /* Translation Table Base 1 Register */
	BFI	lr, r5, #0, #7		/* Copy flags from normal TTBR0 */
	MCR	p15, 0, lr, c2, c0, 0	/* Translation Table Base 0 (idmap) */
	MCR	p15, 0, r4, c10, c2, 1 /* Normal Memory Remap Register */
	MCR	p15, 0, r3, c10, c2, 0 /* Primary Region Remap Register */
	DSB

	/* Enable MMU */
	ISB
    LDR		r0, =0x1801             /* set I,Z,M bit */
    MCR		p15, 0, r0, c1, c0, 0   /* MMU Enable */
    MCR		p15, 0, r0, c7, c5, 6   /* invalidate branch predictor */
	DSB
    ISB
    MCR     p15,0,r0,c8,c7,0        /* invalidate all TLB entries on this cpu */
    ISB

	/* Jump to from physical identity mapping to normal virtual address */
	ADD	pc, pc, #ram0Base - ram0BasePhys
	NOP

	/* Switch back to standard MMU table */
	ISB
	MCR	p15, 0, r5, c2, c0, 0	/* Translation Table Base 0 Register */
	ISB
	MCR	p15, 0, r0, c7, c5, 6	/* invalidate branch predictor */
	MCR	p15, 0, r0, c8, c7,0	/* invalidate whole TLB on this cpu */
	DSB
	ISB

	/* Enable D-Cache */
	MCR	p15, 0, r10, c1, c0, 0
	DSB
	ISB
	MOV		pc, r1

.pool
ENDPROC(restore_arm_register_pa)
	.align	5
RESTORE_ARM_REGISTER_PA_END:

/****************************************************************/
/* Restore ARM register(running in MMU enable) */
/* This function is running on RAM0 */
/* After modification of this function, please check the offset.*/
/* Enter with: */
/*	r0:backup area  */
/*	lr:return address  */
/****************************************************************/
ENTRY(restore_arm_register_va)
	/*----------------------------------------------*/
	/* Copy backup area address */
	/*----------------------------------------------*/
	MOV		r12, r0 /* r12 = r0(backup area) */

	/*-----------------------------------*/
	/* System */
	/*---------------------------------*/
	LDR		r4,  =hoSaveArmSystem
	LDR		r0, [r12, r4]
	LDMIA	r0!, {r4-r9}

	MCR	p15, 0, r4, c12, c0, 0 /* Vector Base Address Register */
	MCR	p15, 0, r5, c7, c4, 0 /* Physical Address Register */
	MCR	p15, 0, r6, c13, c0, 2 /* User Read/Write Thread ID Register */
	MCR	p15, 0, r7, c13, c0, 3 /* User Read-only Thread ID Register */
	MCR	p15, 0, r8, c13, c0, 4 /* Privileged Only ThreadID Register */
	MCR	p15, 2, r9, c0, c0, 0 /* Cache Size Selection Register */

	/*----------------------------------------------*/
	/* VFP */
	/*----------------------------------------------*/
	MRC	p15, 0, r4, c1, c1, 2
	/* Read Non-secure Access Control Register */
	UBFX r5, r4, #0xA, #0x2 /* Extract the CP10 and CP11 Fields */
	CMP		r5, #0x3
	BNE		RESTORE_VFP_SKIP

	LDR		r4, =hoSaveArmVfp
	LDR		r0, [r12, r4]
	MRC	p15, 0, r3, c1, c0, 2
			/* Read Coprocessor Access Control register */
	ORR		r4, r3, #0xF00000 /* Allows CP10 and CP11 access */
	MCR	p15, 0, r4, c1, c0, 2
			/* Write Coprocessor Access Control register */
	ISB		/* Insrtuction Synchronization Barrier */
	MOV		r5, #0x1
	MOV		r6, r5, LSL #0x1E /* Set EN bit to 1. */
	MSR		FPEXC, r6
			/* Write FPEXC register (Set global enable bit to 1) */
	LDMIA	r0!, {r4-r5} /* Load r4:FPEXC, r5:FPSCR	*/
	VLDM	r0!, {D0-D15} /* Restore VFP-D16 register */
	MRS		r6, MVFR0 /* Read MVFR0 register */
	AND		r7, r6, #0xF /* Extract the RB field */
	TEQ	r7, #0x2 /* Check for Advanced SIMD/VFP-D32 support	*/
	.word(POWER_VLDMIA(POWER_CC_EQ, 0, 16, 16)) /* Restore VFP-D32 */
	MSR		FPSCR, r5 /* Restore FPSCR */
	MSR		FPEXC, r4 /* Restore FPEXC */
	MCR		p15, 0, r3, c1, c0, 2
			/* Restore Coprocessor Access Control register */

RESTORE_VFP_SKIP:
	NOP

	/* SVC mode context */
	LDR		r0, [r12, #hoSaveArmSvc]

	LDMIA	r0!, {r4-r5, r13} /* Load CPSR,SPSR & restore sp */
	MSR		SPSR_fsxc, r5 /* Restore SPSR */

	/*----------------------------------------------*/
	/* SYS, ABT, UND, IRQ, FIQ mode context		*/
	/*----------------------------------------------*/
	LDR		r0, [r12, #hoSaveArmExceptSvc]
	CPS		#POWER_MODESYS
	LDMIA	r0!, {r13-r14} /* Restore sp_usr,lr_usr */
	CPS		#POWER_MODEIRQ
	LDMIA	r0!, {r7, r13-r14} /* Load SPSR, restore sp_irq,lr_irq */
	MSR		SPSR_fsxc, r7 /* Restore SPSR_irq */
	CPS		#POWER_MODEABT
	LDMIA	r0!, {r7, r13-r14} /* Load SPSR, restore sp_abt,lr_abt */
	MSR		SPSR_fsxc, r7 /* Restore SPSR_abt */
	CPS		#POWER_MODEUND
	LDMIA	r0!, {r7, r13-r14} /* Load SPSR, restore sp_und,lr_und */
	MSR		SPSR_fsxc, r7 /* Restore SPSR_und */
	CPS		#POWER_MODEFIQ
	LDMIA	r0!, {r7, r8-r14} /* Load SPSR, restore r8_fiq-r13_fiq,lr_fiq */
	/* In secure world, a watchdog FIQ could hit from this point -
	 * secure code checks r13_fiq to see whether public side is ready
	 * for FIQs. So it's basically last, after sp_svc etc. */
	MSR		SPSR_fsxc, r7 /* Restore SPSR_fiq */

	MSR		CPSR_fsxc, r4 /* Restore CPSR (so back to SVC mode) */

	MOV		pc, lr /* return */

.pool
ENDPROC(restore_arm_register_va)
	.align	5
RESTORE_ARM_REGISTER_VA_END:

/********************************************************************/
/* Save arm common register											*/
/* This function is running on RAM0 */
/* After modification of this function, please check the offset.	*/
/* Enter with: */
/*	lr:return address */
/********************************************************************/
ENTRY(save_arm_common_register)

	/*--------------------------------------*/
	/* Save manage area base address		*/
	/*--------------------------------------*/
	MOV		r12, r0
	LDR		r4, =hoBackupAddr
	LDR		r0, [r12, r4] /* Read backup start address */

	/*---------------*/
	/* Save GIC		*/
	/*-------------*/
	LDR		r4, =hoSaveArmGic
	STR		r0, [r12, r4]

	/* Interrupt interface register	*/
	LDR		r1, =BaseInterruptIF
	LDR		r4, [r1, #ICC_PMR]
	LDR		r5, [r1, #ICC_BPR]
	LDR		r6, [r1, #ICC_ICR]
	STMIA	r0!, {r4-r6}

	LDR		r1, =BaseDistrbutor
	/* Distributor Control Register (ICDDCR) */
	LDR		r6, [r1, #ICD_DCR]
	STR		r6, [r0], #0x4

	/*----------------------------------------------*/
	/* Update backup start address					*/
	/*----------------------------------------------*/
	LDR		r4, =hoBackupAddr
	STR		r0, [r12, r4]

	MOV		pc, lr	/* return	*/

.pool
ENDPROC(save_arm_common_register)
	.align	5
SAVE_ARM_COMMON_REGISTER_END:

/********************************************************************/
/* Restore ARM common register	*/
/* This function is running on RAM0 */
/* After modification of this function, please check the offset.	*/
/* Enter with: 						*/
/*	r0:backup area					*/
/*	lr:return address				*/
/********************************************************************/
ENTRY(restore_arm_common_register)

	/*--------------------------------------*/
	/* Restore manage area base address		*/
	/*--------------------------------------*/
	MOV		r12, r0

	/*----------------------------------------------*/
	/* GIC */
	/*----------------------------------------------*/
	LDR		r4, =hoSaveArmGic
	LDR		r0, [r12, r4]

	/* Interrupt interface register */
	LDR		r1, =BaseInterruptIF
	LDMIA	r0!, {r4-r6}
	STR		r4, [r1, #ICC_PMR]
	STR		r5, [r1, #ICC_BPR]
	STR		r6, [r1, #ICC_ICR]

	/* Distributor Control rergister	*/
	LDR	r1, =BaseDistrbutor
	LDR	r6, [r0], #0x4
	STR	r6, [r1, #ICD_DCR]

	MOV		pc, lr	/* Return	*/

.pool
ENDPROC(restore_arm_common_register)
	.align	5
RESTORE_ARM_COMMON_REGISTER_END:

/************************************************************************/
/* system(A3SM) power down	*/
/* This function is running on RAM0 */
/* After modification of this function, please check the offset. */
/* Enter with: 						*/
/* 	lr = Address of return point 	*/
/************************************************************************/
ENTRY(sys_powerdown)

	/*------------------------------*/
	/* Save wakeup code address		*/
	/*------------------------------*/
	LDR		r4, =ram0WakeupCodeAddr0
	ADD		lr, lr, #ram0BasePhys - ram0Base
	STR		lr, [r4]	/* Save wakeup code address	*/

#ifdef CONFIG_SHMOBILE_PASR_SUPPORT
/* PASR Setting */
	/* PASR setting for DRAM area 0*/
	LDR	r4, =ram0DramPasrSettingArea0
	LDR	r5, [r4]	/* r5 = PASR setting area 0*/
	CMP	r5, #0x0	/* if there is no self block bit then*/
	BEQ	SKIP_PASR_DRAM_AREA0	/* skip*/
	LDR	r6, =SDMRACR0A 		/* XXX physical address! */
	LDR	r7, [r6]				/* r7 = SDMRACR0A*/
	LDR	r8, =OPMA_MASK
	AND	r9, r7, r8		/* r9 = SDMRACR0A.OP[15:8], MA[7:0]*/
	LDR	r4, =ram0SaveSdmracr0a
	STR	r9, [r4]		/* Save SDMRACR0A.OP[15:8], MA[7:0]*/
	BIC	r7, r7, r8		/* Clear SDMRACR0A.OP[15:8], MA[7:0]*/
	ORR	r7, r7, r5		/* Set SDMRACR0A.OP[15:8], MA[7:0]*/
	STR	r7, [r6]		/* Write SDMRACR0A*/
	LDR	r4, =SdramMrwCmd0	/* Load address of MRW area 0*/
	MOV	r6, #0x0
	STR	r6, [r4]		/* MRW(PASR) for area 0*/
SKIP_PASR_DRAM_AREA0:

	/* PASR setting for DRAM area 1*/
	LDR	r4, =ram0DramPasrSettingArea1
	LDR	r5, [r4]		/* r5 = PASR setting area 1*/
	CMP	r5, #0x0		/* if there is no self block bit then*/
	BEQ	SKIP_PASR_DRAM_AREA1	/* skip*/
	LDR	r6, =SDMRACR1A
	LDR	r7, [r6]		/* r7 = SDMRACR1A*/
	LDR	r8, =OPMA_MASK
	AND	r9, r7, r8		/* r9 = SDMRACR1A.OP[15:8], MA[7:0]*/
	LDR	r4, =ram0SaveSdmracr1a
	STR	r9, [r4]		/* Save SDMRACR1A.OP[15:8], MA[7:0]*/
	BIC	r7, r7, r8		/* Clear SDMRACR1A.OP[15:8], MA[7:0]*/
	ORR	r7, r7, r5		/* Set SDMRACR1A.OP[15:8], MA[7:0]*/
	STR	r7, [r6]		/* Write SDMRACR1A*/
	LDR	r4, =SdramMrwCmd1	/* Load address of MRW area 1*/
	MOV	r6, #0x0
	STR	r6, [r4]		/* MRW(PASR) for area 1*/
SKIP_PASR_DRAM_AREA1:

#endif /* CONFIG_SHMOBILE_PASR_SUPPORT */

#ifdef CONFIG_RMU2_CMT15
	/*--------------------------------------*/
	/* Turn off CMT1_5, Watchdog FIQ timer	*/
	/*--------------------------------------*/
	LDR		r6, =CMSTR15		/* Use as base of CMT15 */
	LDR		r5, =ram0CMT15Status
	LDR		r7, [r6, #CMSTR]
	ANDS	r7, r7, #1			/* Count start */
	STR		r7, [r5]
	BEQ		SKIP_TURN_OFF_CMT15

	LDR		r6, =CMCLKE
	LDR		r4, [r6]
	BIC		r4, r4, #1<<5 		/* Channel 5 clk disable */
	STR		r4, [r6]
SKIP_TURN_OFF_CMT15:
#endif
	/*--------------------------------------*/
	/* Turn off Watchdog timer				*/
	/*--------------------------------------*/
	/* Check status before turn it off */
	LDR		r6, =MSTPSR4
	LDR		r7, [r6]
	AND		r4, r7, #MSTP402
	CMP		r4, #MSTP402		/* 0: ON, 1: OFF */
	LDR		r6, =ram0RwdtStatus
	LDREQ	r7, =0x1
	LDRNE	r7, =0x0
	STR		r7, [r6]
	BEQ		SKIP_TURN_OFF_RWDT

	LDR		r6, =RWTCNT
	LDR		r7, =RWTCNT_CLEAR
	STRH	r7, [r6]

	LDR		r6, =RWDTCSRA
SYSPD_LOOP_RWDT:
	LDRB	r7, [r6]
	TST		r7, #0x20
	BNE		SYSPD_LOOP_RWDT

	LDR		r6, =RMSTPCR4
	LDR		r7, =SMSTPCR4
	LDR		r4, [r6]
	LDR		r5, [r7]
	ORR		r4, r4, #MSTP402
	ORR		r5, r5, #MSTP402
	STR		r4, [r6]
	STR		r5, [r7]
SKIP_TURN_OFF_RWDT:


#ifdef __EXTAL1_INFO__
	/*------------------------------------------*/
	/* Log information for disabling EXTAL1	*/
	/*------------------------------------------*/

	/* Read EXTAL1 Mask Count Register (EXMSKCNT1)*/
	LDR 	r4, =EXMSKCNT1
	LDR		r5, [r4]
	LDR		r6, =ram0SaveEXMSKCNT1_suspend
	STR		r5, [r6]

	/* Read EXTAL1 Clock Stop Control Register (APSCSTP)*/
	LDR 	r4, =APSCSTP
	LDR		r5, [r4]
	LDR		r6, =ram0SaveAPSCSTP_suspend
	STR		r5, [r6]

	/* Read EXTAL1 Control Register (SYCKENMSK)*/
	LDR 	r4, =SYCKENMSK
	LDR		r5, [r4]
	LDR		r6, =ram0SaveSYCKENMSK_suspend
	STR		r5, [r6]

	/* Read C4 Area Power Control Register (C4POWCR)*/
	LDR 	r4, =C4POWCR
	LDR		r5, [r4]
	LDR		r6, =ram0SaveC4POWCR_suspend
	STR		r5, [r6]

	/* Read	C4 Area Power Control Register2 (PDNSEL)*/
	LDR 	r4, =PDNSEL
	LDR		r5, [r4]
	LDR		r6, =ram0SavePDNSEL_suspend
	STR		r5, [r6]

	/* Read	Power Status Register (PSTR)*/
	LDR 	r4, =PSTR
	LDR		r5, [r4]
	LDR		r6, =ram0SavePSTR_suspend
	STR		r5, [r6]
#endif /*__EXTAL1_INFO__*/

	/*--------------------------------------*/
	/* Power Down							*/
	/*--------------------------------------*/

	/*------------------------------------------*/
	/* Call sec_hal_pm_coma_entry				*/
	/*------------------------------------------*/
	LDR		r0, =COMA_MODE_SUSPEND
	LDR		r1, =ram0ArmVector
	LDR		r1, [r1]
	LDR 	r2, =0x0
	LDR 	r3, =0x0

	LDR		r5, =ram0SecHalCommaEntry
	LDR		r4, [r5]

	MOV		lr, pc
	MOV		pc, r4		/* A3SM down */

	/* Deal with error case
	 * SEC_HAL_RES_FAIL (0x10)
	 */
	LDR		r9, =ram0SecHalReturnCpu0
	STR		r0, [r9]

	/* Turn on Watchdog timer */
	LDR		r6, =ram0RwdtStatus
	LDR		r7, [r6]
	CMP		r7, #0x1		/* 0: enable RWDT, 1: skip */
	BEQ		SKIP_TURN_ON_RWDT3
	LDR		r6, =RMSTPCR4
	LDR		r7, =SMSTPCR4
	LDR		r4, [r6]
	LDR		r5, [r7]
	BIC		r4, r4, #MSTP402
	BIC		r5, r5, #MSTP402
	STR		r4, [r6]
	STR		r5, [r7]
SKIP_TURN_ON_RWDT3:

#ifdef CONFIG_RMU2_CMT15
	/* Turn on CMT1_5, Watchdog FIQ timer */
	LDR		r5, =ram0CMT15Status
	LDR		r7, [r5]
	CMP		r7, #0
	BEQ		SKIP_TURN_ON_CMT15

	LDR		r6, =CMCLKE
	LDR		r4, [r6]
	ORR		r4, r4, #1<<5 		/* Channel 5 clk enable */
	STR		r4, [r6]
SKIP_TURN_ON_CMT15:
#endif

	LDR		r0, =ram0SystemSuspendRestoreCPU0
	LDR		r4, [r0]

	MOV		pc, r4

.pool
ENDPROC(sys_powerdown)
	.align	5
SYS_POWERDOWN_END:

/********************************************/
/*	system(A3SM) power up					*/
/* This function is running on RAM0			*/
/* Enter with: 								*/
/* 	lr = Address of return point 			*/
/********************************************/
ENTRY(sys_powerup)


	/*------------------------------------------*/
	/* Log information of SPI Status Registers	*/
	/*------------------------------------------*/

	/*Read SPI Status Registers 0 */
	LDR		r10, =ICSPISR0Phys
	LDR		r11, [r10]

	LDR		r12, =ram0_ICSPISR0Phys
	STR		r11, [r12]

	/*Read SPI Status Registers 1 */
	LDR		r10, =ICSPISR1Phys
	LDR		r11, [r10]

	LDR		r12, =ram0_ICSPISR1Phys
	STR		r11, [r12]

#ifdef __EXTAL1_INFO__
	/*------------------------------------------*/
	/* Log information for disabling EXTAL1	*/
	/*------------------------------------------*/

	/* Read EXTAL1 Mask Count Register (EXMSKCNT1)*/
	LDR 	r4, =IO_TO_PHYS(EXMSKCNT1)
	LDR		r5, [r4]
	LDR		r6, =ram0SaveEXMSKCNT1Phys_resume
	STR		r5, [r6]

	/* Read EXTAL1 Clock Stop Control Register (APSCSTP)*/
	LDR 	r4, =IO_TO_PHYS(APSCSTP)
	LDR		r5, [r4]
	LDR		r6, =ram0SaveAPSCSTPPhys_resume
	STR		r5, [r6]

	/* Read EXTAL1 Control Register (SYCKENMSK)*/
	LDR 	r4, =IO_TO_PHYS(SYCKENMSK)
	LDR		r5, [r4]
	LDR		r6, =ram0SaveSYCKENMSKPhys_resume
	STR		r5, [r6]

	/* Read C4 Area Power Control Register (C4POWCR)*/
	LDR 	r4, =IO_TO_PHYS(C4POWCR)
	LDR		r5, [r4]
	LDR		r6, =ram0SaveC4POWCRPhys_resume
	STR		r5, [r6]

	/* Read	C4 Area Power Control Register2 (PDNSEL)*/
	LDR 	r4, =IO_TO_PHYS(PDNSEL)
	LDR		r5, [r4]
	LDR		r6, =ram0SavePDNSELPhys_resume
	STR		r5, [r6]

	/* Read	Power Status Register (PSTR)*/
	LDR 	r4, =IO_TO_PHYS(PSTR)
	LDR		r5, [r4]
	LDR		r6, =ram0SavePSTRPhys_resume
	STR		r5, [r6]
#endif /*__EXTAL1_INFO__*/

	/*----------------------------------*/
	/* Turn on Watchdog timer			*/
	/*----------------------------------*/
	LDR		r6, =ram0RwdtStatusPhys
	LDR		r7, [r6]
	CMP		r7, #0x1		/* 0: enable RWDT, 1: skip */
	BEQ		SKIP_TURN_ON_RWDT2

	LDR		r6, =IO_TO_PHYS(RMSTPCR4)
	LDR		r7, =IO_TO_PHYS(SMSTPCR4)
	LDR		r4, [r6]
	LDR		r5, [r7]
	BIC		r4, r4, #MSTP402
	BIC		r5, r5, #MSTP402
	STR		r4, [r6]
	STR		r5, [r7]
SKIP_TURN_ON_RWDT2:

#ifdef CONFIG_RMU2_CMT15
	/* Turn on CMT1_5, Watchdog FIQ timer */
	LDR		r5, =ram0CMT15StatusPhys
	LDR		r7, [r5]
	CMP		r7, #0
	BEQ		SKIP_TURN_ON_CMT15_1

	LDR		r6, =CMCLKE_PHYS
	LDR		r4, [r6]
	ORR		r4, r4, #1<<5 		/* Channel 5 clk enable */
	STR		r4, [r6]
SKIP_TURN_ON_CMT15_1:
#endif

#ifdef CONFIG_SHMOBILE_PASR_SUPPORT
/* PASR Setting*/
	/*Release PASR setting for DRAM area 0*/
	LDR	r4, =ram0DramPasrSettingArea0Phys
	LDR	r5, [r4]	/* r5 = PASR setting*/
	CMP	r5, #0x0	/* if there is no self block bit then*/
	BEQ	SKIP_RELEASE_PASR_DRAM_AREA0 /* skip*/
	LDR	r6, =SDMRACR0A
	LDR	r7, [r6]			/* r7 = SDMRACR0A*/
	MOV	r8, #OP_MASK
	BIC	r7, r7, r8			/* Clear SDMRACR0A.OP[15:8]*/
	STR r7, [r6]			/* Write SDMRACR0A*/
	LDR	r8, =SdramMrwCmd0Phys	/* Load address of MRW area 0*/
	MOV	r9, #0x0
	STR	r9, [r8]			/* MRW (release PASR)*/
	LDR	r4, =ram0SaveSdmracr0aPhys
	LDR	r5, [r4]
	MOV	r8, #MA_MASK
	BIC	r7, r7, r8			/* Clear SDMRACR0A.MA[7:0]*/
	ORR	r7, r7, r5
	STR	r7, [r6]			/* Restore SDMRACR0A	*/
SKIP_RELEASE_PASR_DRAM_AREA0:

	/*Release PASR setting for DRAM area 1*/
	LDR	r4, =ram0DramPasrSettingArea1Phys
	LDR	r5, [r4]			/* r5 = PASR setting*/
	CMP	r5, #0x0	/* if there is no self block bit then*/
	BEQ	SKIP_RELEASE_PASR_DRAM_AREA1 /* skip*/
	LDR	r6, =SDMRACR1A
	LDR	r7, [r6]			/* r7 = SDMRACR1A*/
	MOV	r8, #OP_MASK
	BIC	r7, r7, r8			/* Clear SDMRACR1A.OP[15:8]*/
	STR r7, [r6]			/* Write SDMRACR1A*/
	LDR	r8, =SdramMrwCmd1Phys	/* Load address of MRW area 1*/
	MOV	r9, #0x0
	STR	r9, [r8]	/* MRW (release PASR) for area 1 */
	LDR	r4, =ram0SaveSdmracr1aPhys
	LDR	r5, [r4]
	MOV	r8, #MA_MASK
	BIC	r7, r7, r8	/* Clear SDMRACR1A.MA[7:0]*/
	ORR	r7, r7, r5
	STR	r7, [r6]	/* Restore SDMRACR1A	*/
SKIP_RELEASE_PASR_DRAM_AREA1:
#endif /* CONFIG_SHMOBILE_PASR_SUPPORT */

	MOV		pc, lr		/* return	*/

.pool
ENDPROC(sys_powerup)
	.align	5
SYS_POWERUP_END:

/****************************************/
/* Transition to WFI standby.			*/
/* This function is running on SDRAM	*/
/* Enter with: 							*/
/* 	lr = Address of return point 		*/
/****************************************/
ENTRY(start_wfi)
	STMFD	sp!, {r4-r11, lr}

#ifdef CONFIG_PM_SMP
	/* Get PM SpinLock */
	BL PM_Spin_Lock
	NOP

	/* Update current cpu status			*/
	MRC	p15, 0, r4, c0, c0, 5 /* Read MPIDR, MP affinty register */
	UBFX	r0, r4, #0x0, #0x2 /* Extract the CPUID Field */
	CMP		r0, #0x0
	LDREQ	r5, =ram0Cpu0Status
	LDRNE	r5, =ram0Cpu1Status
	LDR		r6, =CPUSTATUS_WFI
	STR		r6, [r5]
	DSB

	/* Release PM SpinLock					*/
	BL PM_Spin_Unlock
	NOP
#endif

	WFI		/* Transition to the WFI standby state */

#ifdef CONFIG_PM_SMP
	/* Get PM SpinLock						*/
	BL PM_Spin_Lock
	NOP

	/* Update current cpu status			*/
	/* r0: CPU number						*/
	CMP		r0, #0x0
	LDREQ	r5, =ram0Cpu0Status
	LDRNE	r5, =ram0Cpu1Status
	LDR		r6, =CPUSTATUS_RUN
	STR		r6, [r5]
	DSB

	/* Release PM SpinLock					*/
	BL PM_Spin_Unlock
	NOP
#endif

	LDMFD	sp!, {r4-r11, pc}

.pool
ENDPROC(start_wfi)
	.align	5

/********************************************************/
/*	Transition to coreStandby(A1SL0 or A1Sl1 Power Off) */
/* This function is running on SDRAM */
/* Enter with: */
/* 	r0 = this pointer */
/* 	lr = Address of return point */
/********************************************************/
ENTRY(start_corestandby_2)
	STMFD	sp!, {r4-r11, lr}

	/*--------------------------------------------------------------*/
	/* Initialize the management information of backup area */
	/*--------------------------------------------------------------*/
	LDR	r4, =ram0Cpu0RegisterArea
	LDR		r4, [r4]
	LDR		r5, =hoDataArea
	MOV		r0, r4
	LDR 	r1, =0x0
	MOV		r2, r5
	BL		memset
	ADD		r6, r4, r5
	LDR		r7, =hoBackupAddr
	STR		r6, [r4, r7]

	/*--------------------------------------*/
	/* Jump to CoreStandby_2() on SDRAM		*/
	/*--------------------------------------*/

	LDR		r4, =ram0CoreStandby_2
	LDR		r4, [r4]
	MOV		lr, pc			/* lr: Return address */
	MOV		pc, r4			/* Call corestandby_2() */

	LDMFD	sp!, {r4-r11, pc}

.pool
ENDPROC(start_corestandby_2)
	.align	5

/*****************************************************************/
/*	Transition to coreStandby(A1SL0 or A1Sl1 Power Off) */
/* This function is running on SDRAM */
/* After modification of this function, please check the offset. */
/* Enter with: */
/* 	r0 = this pointer */
/* 	lr = Address of return point */
/******************************************************************/
ENTRY(corestandby_2)
	STMFD	sp!, {r4-r11, lr}

	LDR		r0, =PM_PM_ID_CORESTANDBY2_IN
	LDR		r4, =ram0MemoryLogPm
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call memory_log_pm */
	NOP

	/*------------------------------------------*/
	/* Clear WUPSFAC */
	/*------------------------------------------*/
	LDR		r4, =WUPSFAC
	LDR		r4, [r4] /* Clear WUPSFAC */

	/*------------------------------------------*/
	/* Save ARM registers						*/
	/*------------------------------------------*/
	LDR	r0, =ram0Cpu0RegisterArea /* r0: Backup area for CPU0 */
	LDR		r0, [r0]
	LDR		r4, =ram0SaveArmRegister
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call save_arm_register() */
	NOP

#ifdef CONFIG_PM_SMP
	/* Update current cpu status			*/
	LDR	r5, =ram0Cpu0Status
	LDR		r6, =CPUSTATUS_SHUTDOWN2
	STR		r6, [r5]
	DSB
#endif

	LDR	r0, =ram0CoreStandby2RestoreCPU0
		/* r0: Backup area of resume point after MMU is enable */
	ADR		r4, CORESTANDBY2_RESTORE
	STR		r4, [r0]
	DSB

	/* Check IRQ */
	LDR		r6, =1023
	LDR		r5, =BaseInterruptIF
	LDR		r3, [r5, #ICC_HPIR]
	AND		r3, r3, r6
	CMP		r3, r6
	BNE		CORESTANDBY2_RESTORE

	/*-----------------------------------*/
	/* CPU power down preparation for wakeup code MMU off */
	/*----------------------------------*/
	ADR		r5, CORESTANDBY2_WAKEUP
	ADD		r5, r5, #ram0BasePhys - ram0Base
	LDR	r6, =ram0WakeupCodeAddr0
	STR		r5, [r6] /* Set the CPU wakeup code address	*/
	DSB

	LDR		r0, =COMA_MODE_CORE_STANDBY_2 /* mode */
	LDR		r1, =ram0ArmVector
	LDR		r1, [r1]
	LDR		r2, =0x0
	LDR		r3, =0x0

	LDR		r5, =ram0SecHalCommaEntry
	LDR		r5, [r5]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r5 /* Call sec_hal_pm_coma_entry() */
	NOP

	/* Save return value of sec_hal_pm_coma_entry() for error case only */
	LDR	r5, =ram0SecHalReturnCpu0
	STR		r0, [r5] /* Save return value */
	DSB
	B		CORESTANDBY2_RESTORE

CORESTANDBY2_WAKEUP:
	NOP

	/*------------------------------------------*/
	/* Restore ARM registers					*/
	/*------------------------------------------*/
	LDR	r0, =ram0MmuSetting0Phys

	LDR	r1, =ram0CoreStandby2RestoreCPU0Phys
	LDR		r1, [r1]

	LDR		r4, =ram0RestoreArmRegisterPAPhys
	LDR		r4, [r4]
	MOV		pc,r4 /* Call restore_arm_register_pa() */

CORESTANDBY2_RESTORE:

#ifdef CONFIG_PM_SMP
	/* Update current cpu status			*/
	LDR		r5, =ram0Cpu0Status
	LDR		r6, =CPUSTATUS_RUN
	STR		r6, [r5]
	DSB
#endif

	/*-----------------------------------*/
	/* Restore ARM registers 			*/
	/*---------------------------------*/
	LDR	r0, =ram0Cpu0RegisterArea
	LDR		r0, [r0]
	LDR		r4, =ram0RestoreArmRegisterVA
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call restore_arm_register_va */
	NOP

	LDR		r0, =PM_PM_ID_CORESTANDBY2_OUT
	LDR		r4, =ram0MemoryLogPm
	LDR		r4, [r4]
	MOV		lr, pc /* lr: Return address */
	MOV		pc, r4 /* Call memory_log_pm */
	NOP

	LDMFD	sp!, {r4-r11,pc}

.pool
ENDPROC(corestandby_2)
	.align	5
CORESTANDBY_2_END:

/********************************************************/
/*copy_functions */
/* This function is running on SDRAM */
/* Enter with: */
/* lr = Address of return point */
/********************************************************/
ENTRY(copy_functions)
	STMFD	sp!, {r4-r11, lr}

	LDR		r4, =ram0StartAddressOfFunctionArea
	LDR		r1, =ram0ArmVector
	ADD		r0, r4, #ram0BasePhys - ram0Base /* store PA, not VA */
	STR		r0, [r1]
	MOV		r0, r4
	LDR		r1, =ArmVector
	LDR		r5, =ARM_VECTOR_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0CoreStandby
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =corestandby
	LDR		r5, =CORESTANDBY_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0CoreStandby_2
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =corestandby_2
	LDR		r5, =CORESTANDBY_2_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0SystemSuspend
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =systemsuspend
	LDR		r5, =SYSTEMSUSPEND_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0SaveArmRegister
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =save_arm_register
	LDR		r5, =SAVE_ARM_REGISTER_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0RestoreArmRegisterPA
	ADD		r0, r4, #ram0BasePhys - ram0Base /* store PA, not VA */
	STR		r0, [r1]
	MOV		r0, r4
	LDR		r1, =restore_arm_register_pa
	LDR		r5, =RESTORE_ARM_REGISTER_PA_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0RestoreArmRegisterVA
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =restore_arm_register_va
	LDR		r5, =RESTORE_ARM_REGISTER_VA_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0SaveArmCommonRegister
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =save_arm_common_register
	LDR		r5, =SAVE_ARM_COMMON_REGISTER_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0RestoreArmCommonRegister
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =restore_arm_common_register
	LDR		r5, =RESTORE_ARM_COMMON_REGISTER_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0PM_Spin_Lock
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =PM_Spin_Lock
	LDR		r5, =PM_SPIN_LOCK_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0PM_Spin_Unlock
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =PM_Spin_Unlock
	LDR		r5, =PM_SPIN_UNLOCK_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0SysPowerDown
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =sys_powerdown
	LDR		r5, =SYS_POWERDOWN_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0SysPowerUp
	ADD		r0, r4, #ram0BasePhys - ram0Base /* store PA, not VA */
	STR		r0, [r1]
	MOV		r0, r4
	LDR		r1, =sys_powerup
	LDR		r5, =SYS_POWERUP_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0MemoryLogPm
	STR		r4, [r1]
	MOV		r0, r4
	LDR		r1, =memory_log_pm
	LDR		r5, =MEMORY_LOG_PM_END
	SUB		r5, r5, r1
	MOV		r2, r5
	BL		memcpy

	ADD		r4, r4, r5
	LDR		r1, =ram0EndAddressOfFunctionArea
	CMP		r4, r1
	MOVLS		r0, #0
	MOVHI		r0, #-1

	LDMFD	sp!, {r4-r11, pc}

.pool
ENDPROC(copy_functions)
