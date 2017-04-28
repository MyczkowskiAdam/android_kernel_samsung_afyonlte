/*
 * r8a7373 lowlevel sleep code support
 *
 * Copyright (C) 2012  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 */

#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/memory.h>
#include <asm/assembler.h>

#include "pm-r8a7373.h"
#include "pm-macro.S"

#if defined(CONFIG_SUSPEND) || defined(CONFIG_CPU_IDLE)
	.align	2
ENTRY(r8a7373_common_vector)
	b	init
	nop
	nop
	nop
	nop
	nop
	nop
	nop
exception_loop:
	b	exception_loop

init:
	/* Setup L1 I-cache */
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 0	@ invalidate I cache
	mcr	p15, 0, r0, c7, c5, 6	@ invalidate branch predictor
	mrc	p15, 0, r1, c1, c0, 0
	orr	r1, r1, #0x1800
	mcr	p15, 0, r1, c1, c0, 0
	isb

	/* Jump to wakeup code */
	mrc	p15, 0, r1, c0, c0, 5
	ands	r1, r1, #0x0f
	ldreq	r2, =RAM0_WAKEUP_ADDR0
	ldrne	r2, =RAM0_WAKEUP_ADDR1
	ldr	r1, [r2]
	mov	pc, r1
ENDPROC(r8a7373_common_vector)
	.pool
ENTRY(r8a7373_common_vector_size)
	.word	. - r8a7373_common_vector

L1_dcache_flush:
	.word	v7_flush_dcache_all

	.align	2
ENTRY(r8a7373_do_idle_core_standby)
	stmfd	sp!, {r0-r12, lr}	@ save registers on stack

	/* Disable D-cache */
	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #(1 << 2)
	mcr	p15, 0, r0, c1, c0, 0
	isb

	/* Flush D-cache */
	ldr	r1, L1_dcache_flush
	blx	r1
	dsb
	dmb

#ifdef CONFIG_SMP
	/* Clear SMP bit */
	mrc	p15, 0, r0, c1, c0, 1
	bic	r0, r0, #0x40
	mcr	p15, 0, r0, c1, c0, 1
	isb
#endif

	/* Set the CPU Powered-off mode */
	mrc	p15, 0, r1, c0, c0, 5
	and	r1, r1, #0x0f
	mov	r2, #0x03
	ldr	r3, =SCU_PWRST
	add	r3, r3, r1
	ldrb	r4, [r3]
	orr	r4, r4, r2
	strb	r4, [r3]
	dsb

	wfi
	nop

	/* Set the CPU Normal mode */
	mrc	p15, 0, r1, c0, c0, 5
	and	r1, r1, #0x0f
	mov	r2, #0x03
	ldr	r3, =SCU_PWRST
	add	r3, r3, r1
	ldrb	r4, [r3]
	bic	r4, r4, r2
	strb	r4, [r3]
	dsb

#ifdef CONFIG_SMP
	/* Set SMP bit */
	mrc	p15, 0, r0, c1, c0, 1
	orr	r0, r0, #0x40
	mcr	p15, 0, r0, c1, c0, 1
	isb
#endif

	mrc	p15, 0, r0, c1, c0, 0
	tst	r0, #(1 << 2)		@ Check C bit enabled?
	orreq	r0, r0, #(1 << 2)	@ Enable the C bit if cleared
	mcreq	p15, 0, r0, c1, c0, 0
	isb

	ldmfd	sp!, {r0-r12, pc}	@ restore regs and return
ENDPROC(r8a7373_do_idle_core_standby)
	.pool

	.align	2
ENTRY(r8a7373_resume_core_standby)
	/* Set the CPU Normal mode */
	mrc	p15, 0, r1, c0, c0, 5
	and	r1, r1, #0x0f
	mov	r2, #0x03
	ldr	r3, =SCU_PWRST
	add	r3, r3, r1
	ldrb	r4, [r3]
	bic	r4, r4, r2
	strb	r4, [r3]
	dsb

	/* L1 D-cache invalidate */
	v7_invalidate_l1

	/* Invalidate duplicate tag store that associated with current cpu. */
	mrc	p15, 0, r1, c0, c0, 5
	ands	r1, r1, #0x0f
	ldreq	r2, =0x0f	@ Set CPU0 ways
	ldrne	r2, =0xf0	@ Set CPU1 ways
	ldr	r3, =SCU_INVALL
	str	r2, [r3]
	dsb

	ldr     pc, 1f
1:	.long   cpu_resume - PAGE_OFFSET + PLAT_PHYS_OFFSET
ENDPROC(r8a7373_resume_core_standby)
	.pool
#endif
