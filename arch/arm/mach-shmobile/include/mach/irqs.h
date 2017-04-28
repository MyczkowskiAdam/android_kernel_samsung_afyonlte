#ifndef __ASM_MACH_IRQS_H
#define __ASM_MACH_IRQS_H

#include <linux/sh_intc.h>

/* GIC */
#define gic_spi(nr)		((nr) + 32)

/* INTCS */
#define INTCS_VECT_BASE		0x2200
#define INTCS_VECT(n, vect)	INTC_VECT((n), INTCS_VECT_BASE + (vect))
#define intcs_evt2irq(evt)	evt2irq(INTCS_VECT_BASE + (evt))

#ifdef CONFIG_ARCH_R8A7373
/* IRQ pin */
#define IRQPIN_IRQ_BASE		512
#define irqpin2irq(nr)		(int)(IRQPIN_IRQ_BASE + (nr))
#endif

#define FIQ_START	0

#endif /* __ASM_MACH_IRQS_H */
