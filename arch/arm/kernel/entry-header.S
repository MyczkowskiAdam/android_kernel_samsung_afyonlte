#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/errno.h>
#include <asm/thread_info.h>

@ Bad Abort numbers
@ -----------------
@
#define BAD_PREFETCH	0
#define BAD_DATA	1
#define BAD_ADDREXCPTN	2
#define BAD_IRQ		3
#define BAD_UNDEFINSTR	4

#ifdef CONFIG_IRQ_TRACE
/* TMPLOG_SIZE_PERCPU = (TMPLOG_TOTAL_SIZE / 2) = 128kB per CPU */
#define TMPLOG_SIZE_PERCPU 	0x00020000
/* TMPLOG_ENTRIES_PERCPU = (TMPLOG_TOTAL_SIZE / 32 bytes) = 4096 entries per CPU */
#define TMPLOG_ENTRIES_PERCPU	0x00001000
#define CMCNT3 			0xe6130314
#endif

@
@ Most of the stack format comes from struct pt_regs, but with
@ the addition of 8 bytes for storing syscall args 5 and 6.
@ This _must_ remain a multiple of 8 for EABI.
@
#define S_OFF		8

/* 
 * The SWI code relies on the fact that R0 is at the bottom of the stack
 * (due to slow/fast restore user regs).
 */
#if S_R0 != 0
#error "Please fix"
#endif

	.macro	zero_fp
#ifdef CONFIG_FRAME_POINTER
	mov	fp, #0
#endif
	.endm

	.macro	alignment_trap, rtemp
#ifdef CONFIG_ALIGNMENT_TRAP
	ldr	\rtemp, .LCcralign
	ldr	\rtemp, [\rtemp]
	mcr	p15, 0, \rtemp, c1, c0
#endif
	.endm

	@
	@ Store/load the USER SP and LR registers by switching to the SYS
	@ mode. Useful in Thumb-2 mode where "stm/ldm rd, {sp, lr}^" is not
	@ available. Should only be called from SVC mode
	@
	.macro	store_user_sp_lr, rd, rtemp, offset = 0
	mrs	\rtemp, cpsr
	eor	\rtemp, \rtemp, #(SVC_MODE ^ SYSTEM_MODE)
	msr	cpsr_c, \rtemp			@ switch to the SYS mode

	str	sp, [\rd, #\offset]		@ save sp_usr
	str	lr, [\rd, #\offset + 4]		@ save lr_usr

	eor	\rtemp, \rtemp, #(SVC_MODE ^ SYSTEM_MODE)
	msr	cpsr_c, \rtemp			@ switch back to the SVC mode
	.endm

	.macro	load_user_sp_lr, rd, rtemp, offset = 0
	mrs	\rtemp, cpsr
	eor	\rtemp, \rtemp, #(SVC_MODE ^ SYSTEM_MODE)
	msr	cpsr_c, \rtemp			@ switch to the SYS mode

	ldr	sp, [\rd, #\offset]		@ load sp_usr
	ldr	lr, [\rd, #\offset + 4]		@ load lr_usr

	eor	\rtemp, \rtemp, #(SVC_MODE ^ SYSTEM_MODE)
	msr	cpsr_c, \rtemp			@ switch back to the SVC mode
	.endm

#ifndef CONFIG_THUMB2_KERNEL
	.macro	svc_exit, rpsr
#ifdef	CONFIG_IRQ_TRACE
	mov	r7,\rpsr
	bl	save_data_svc
#endif
	msr	spsr_cxsf, \rpsr
#if defined(CONFIG_CPU_V6)
	ldr	r0, [sp]
	strex	r1, r2, [sp]			@ clear the exclusive monitor
	ldmib	sp, {r1 - pc}^			@ load r1 - pc, cpsr
#elif defined(CONFIG_CPU_32v6K)
	clrex					@ clear the exclusive monitor
	ldmia	sp, {r0 - pc}^			@ load r0 - pc, cpsr
#else
	ldmia	sp, {r0 - pc}^			@ load r0 - pc, cpsr
#endif
	.endm

	.macro	restore_user_regs, fast = 0, offset = 0
#ifdef	CONFIG_IRQ_TRACE
	MRC     p15, 0, r8, c0, c0, 5                   /* Read MPIDR, MP affinty register*/
	UBFX    r8, r8, #0x0, #0x2                      /* Extract the CPUID Field      */
/*	CMP     r8, #0x0 */
	LDR     r9, =tmplog_nocache_address
	LDR     r11, [r9]
	CMP     r11, #0x0
	BEQ	1f

	CMP     r8, #0x0
	MOVEQ   r8, r11
	ADDNE   r8, r11, #TMPLOG_SIZE_PERCPU

	LDR	r9, [r8]
	ADD	r9, r9, #1
	CMP	r9, #TMPLOG_ENTRIES_PERCPU

	MOVHS	r9, #1
	STR	r9, [r8]
	ADD	r8, r8, r9, lsl #5

	LDR	r9, = CMCNT3
	LDR	r9, [r9]
	MOV	r10, #0xffffffdd		@exit user IRQ
	ldr	r11, [sp, #\offset + S_LR]	@ lr
	ldr	r12, [sp, #\offset + S_SP]	@ sp
	STMIA	r8!, {r9,r10,r11,r12}		@save current_timer, mark, lr, sp
	mov	r12, r10
	ldr	r9,  [sp, #\offset + S_PC]	@ pc
	ldr	r10, [sp, #\offset + S_PSR]	@ psr
	mov	r11, r10
	STMIA	r8!, {r9,r10,r11,r12}		@pc, psr, psr,mark
1:
#endif
	ldr	r1, [sp, #\offset + S_PSR]	@ get calling cpsr
	ldr	lr, [sp, #\offset + S_PC]!	@ get pc
	msr	spsr_cxsf, r1			@ save in spsr_svc
#if defined(CONFIG_CPU_V6)
	strex	r1, r2, [sp]			@ clear the exclusive monitor
#elif defined(CONFIG_CPU_32v6K)
	clrex					@ clear the exclusive monitor
#endif
	.if	\fast
	ldmdb	sp, {r1 - lr}^			@ get calling r1 - lr
	.else
	ldmdb	sp, {r0 - lr}^			@ get calling r0 - lr
	.endif
	mov	r0, r0				@ ARMv5T and earlier require a nop
						@ after ldm {}^
	add	sp, sp, #S_FRAME_SIZE - S_PC
	movs	pc, lr				@ return & move spsr_svc into cpsr
	.endm

	.macro	get_thread_info, rd
	mov	\rd, sp, lsr #13
	mov	\rd, \rd, lsl #13
	.endm

	@
	@ 32-bit wide "mov pc, reg"
	@
	.macro	movw_pc, reg
	mov	pc, \reg
	.endm
#else	/* CONFIG_THUMB2_KERNEL */
	.macro	svc_exit, rpsr
	ldr	lr, [sp, #S_SP]			@ top of the stack
	ldrd	r0, r1, [sp, #S_LR]		@ calling lr and pc
	clrex					@ clear the exclusive monitor
	stmdb	lr!, {r0, r1, \rpsr}		@ calling lr and rfe context
	ldmia	sp, {r0 - r12}
	mov	sp, lr
	ldr	lr, [sp], #4
	rfeia	sp!
	.endm

	.macro	restore_user_regs, fast = 0, offset = 0
	clrex					@ clear the exclusive monitor
	mov	r2, sp
	load_user_sp_lr r2, r3, \offset + S_SP	@ calling sp, lr
	ldr	r1, [sp, #\offset + S_PSR]	@ get calling cpsr
	ldr	lr, [sp, #\offset + S_PC]	@ get pc
	add	sp, sp, #\offset + S_SP
	msr	spsr_cxsf, r1			@ save in spsr_svc
	.if	\fast
	ldmdb	sp, {r1 - r12}			@ get calling r1 - r12
	.else
	ldmdb	sp, {r0 - r12}			@ get calling r0 - r12
	.endif
	add	sp, sp, #S_FRAME_SIZE - S_SP
	movs	pc, lr				@ return & move spsr_svc into cpsr
	.endm

	.macro	get_thread_info, rd
	mov	\rd, sp
	lsr	\rd, \rd, #13
	mov	\rd, \rd, lsl #13
	.endm

	@
	@ 32-bit wide "mov pc, reg"
	@
	.macro	movw_pc, reg
	mov	pc, \reg
	nop
	.endm
#endif	/* !CONFIG_THUMB2_KERNEL */

/*
 * These are the registers used in the syscall handler, and allow us to
 * have in theory up to 7 arguments to a function - r0 to r6.
 *
 * r7 is reserved for the system call number for thumb mode.
 *
 * Note that tbl == why is intentional.
 *
 * We must set at least "tsk" and "why" when calling ret_with_reschedule.
 */
scno	.req	r7		@ syscall number
tbl	.req	r8		@ syscall table pointer
why	.req	r8		@ Linux syscall (!= 0)
tsk	.req	r9		@ current thread_info
