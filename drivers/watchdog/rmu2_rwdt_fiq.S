/*
 * rmu2_rwdt_fiq.S
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/asm-offsets.h>

/* If fiq_flue is available, don't need anything from this file */
#ifndef CONFIG_FIQ_GLUE

/* These are in <asm/io.h>, but can't include it from assembler */
#define	STBCHR2		IO_ADDRESS(0xE6180002)
#define APE_RESETLOG_RWDT_CMT_FIQ        (0x40)

#if defined(CONFIG_AEABI) && (__LINUX_ARM_ARCH__ >= 5)
#define SPFIX(code...) code
#else
#define SPFIX(code...)
#endif

	.text
	.global rmu2_cmt_fiq_handler_end

/*
 * FIQs are only used for the watchdog - we don't bother to check source.
 * This means that all cores will enter process the FIQ; if we checked
 * source in the GIC, then only one core would handle it. It also gives
 * more flexibility for the secure code, if present - we can assume any
 * FIQ it passes through to the non-secure side is a watchdog death.
 *
 * As FIQs aren't used elsewhere, we can trample registers - their
 * contents at crash time are not interesting, except R13 which should
 * be our per-CPU register dump area.
 */
ENTRY(rmu2_cmt_fiq_handler)
/* This is SMP-safe, at least for the FIQ handler being entered
 * simultaneously - the bit will end up set, maybe more than once.
 */
	LDR	r8, =STBCHR2
	LDRB	r9, [r8]
	ORR	r9, r9, #APE_RESETLOG_RWDT_CMT_FIQ
	STRB	r9, [r8]

	SUB	lr, lr, #4
	STR	lr, [r13, #S_PC]		// Save R14_fiq-4 (FIQ return address) as "PC"
	MVN	lr, #0
	STR	lr, [r13, #S_OLD_R0]		// Dummy value in "old_R0"
	MRS	lr, spsr
	STR	lr, [r13, #S_PSR]		// Save SPSR_fiq as "PSR"
	STMIA	r13, {r0-r14}^			// Initially store User R0-R14
	MOV	r0, r13
	TST	lr, #(MODE_MASK &~ MODE32_BIT)
	MRSNE	r7, cpsr
	BFINE	r7, lr, #0, #5			// If saved mode wasn't User, then switch
	MSRNE	cpsr_c, r7			// to it, and save its R13, R14 instead
	STRNE	r13, [r0, #S_SP]
	STRNE	r14, [r0, #S_LR]
	CPS	#SVC_MODE
	/* The hope now is that we do have some SVC stack to execute on.
	 * It would be nice to have an emergency stack to use instead,
	 * although it would mean the standard SVC stack backtrace would break,
	 * and we'd want to dump it separately.
	 *
	 * Entry conditions: R0 -> struct pt_regs
	 */
 SPFIX(	BIC	sp, sp, #7	)
	LDR	pc, =rmu2_cmt_fiq		// This mustn't return
ENDPROC(rmu2_cmt_fiq_handler)
	.ltorg
rmu2_cmt_fiq_handler_end:

#endif // CONFIG_FIQ_GLUE
