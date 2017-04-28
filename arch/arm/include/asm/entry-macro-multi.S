#include <asm/assembler.h>
#include <asm/unwind.h>

/*
 * Interrupt handling.  Preserves r7, r8, r9
 */
	.macro	arch_irq_handler_default, stack_offset=0
	get_irqnr_preamble r6, lr
1:	get_irqnr_and_base r0, r2, r6, lr
	addne	r1, sp, #\stack_offset
	@
	@ routine called with r0 = irq number, r1 = struct pt_regs *
	@
	adrne	lr, BSYM(1b)
	bne	asm_do_IRQ

#ifdef CONFIG_SMP
	/*
	 * XXX
	 *
	 * this macro assumes that irqstat (r2) and base (r6) are
	 * preserved from get_irqnr_and_base above
	 */
	ALT_SMP(test_for_ipi r0, r2, r6, lr)
	ALT_UP_B(9997f)
	addne	r1, sp, #\stack_offset
	adrne	lr, BSYM(1b)
	bne	do_IPI

#ifdef CONFIG_LOCAL_TIMERS
	test_for_ltirq r0, r6, r5, lr
	addne	r0, sp, #\stack_offset
	adrne	lr, BSYM(1b)
	bne	do_local_timer
#endif
#endif
9997:
	.endm

#ifdef CONFIG_ARM_UNWIND
/* Previous version transferred LR to R4. This can't be unwound by
 * unwind_backtrace, as it can't be expressed in the ARM unwind table.
 * So backtraces from interrupts, including ipi_cpu_stop, weren't useful.
 *
 * So instead we now push LR (and IP to keep stack multiple of 8). That can be
 * unwound.
 *
 * Seems silly to waste CPU cycles just for the crash case, and odd that
 * the ARM EABI unwind spec doesn't specify register-to-register moves (it
 * has an unspecified reserved slot for them though).
 *
 * This push then creates an new problem - dump_backtrace_entry tries to
 * print the struct pt_regs on the stack for exception routines like asm_do_IRQ.
 * But we've just pushed an extra 2 words, so on entry to those routines the
 * struct pt_regs is no longer at the top of the stack, so it doesn't print the
 * right 18 words. We put this routine in the exception area, so the right
 * 18 words are printed for this frame, at least.
 */
	.macro	arch_irq_handler, symbol_name
	.pushsection .exception.text,"ax",%progbits
	.align	5
	.global \symbol_name
\symbol_name:
	.fnstart
	stmfd	sp!, {ip, lr}
	.save	{ip, lr}
	arch_irq_handler_default 8
	ldmfd	sp!, {ip, pc}
	.fnend
	.popsection
	.endm
#else
	.macro	arch_irq_handler, symbol_name
	.align	5
	.global \symbol_name
\symbol_name:
	mov	r8, lr
	arch_irq_handler_default
	mov     pc, r8
	.endm
#endif
