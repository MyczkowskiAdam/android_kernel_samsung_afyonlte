#ifndef __ASM_MACH_TIMEX_H
#define __ASM_MACH_TIMEX_H

#define CLOCK_TICK_RATE		1193180 /* unused i8253 PIT value */
#ifdef CONFIG_SMP /* && CONFIG_CPU_FREQ */
#define ARCH_HAS_READ_CURRENT_TIMER
#endif

#endif /* __ASM_MACH_TIMEX_H */
