/*
 * SMP support for R-Mobile / SH-Mobile
 *
 * Copyright (C) 2010  Magnus Damm
 * Copyright (C) 2010  Takashi Yoshii
 *
 * Based on vexpress, Copyright (c) 2003 ARM Limited, All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/memory.h>

#include "pm-macro.S"

	__CPUINIT

ENTRY(shmobile_invalidate_start)
	v7_invalidate_l1
	b	secondary_startup
ENDPROC(shmobile_invalidate_start)
	.pool
/*
 * Reset vector for secondary CPUs.
 * This will be mapped at address 0 by SBAR register.
 * We need _long_ jump to the physical address.
 */
	.align  12
ENTRY(shmobile_secondary_vector)
	ldr     pc, 1f
1:	.long   shmobile_invalidate_start - PAGE_OFFSET + PLAT_PHYS_OFFSET
ENDPROC(shmobile_secondary_vector)

#ifdef CONFIG_ARCH_R8A7373
ENTRY(r8a7373_secondary_vector)
	ldr	r0, r8a7373_secondary_vector_addr
	mov	pc, r0
ENTRY(r8a7373_secondary_vector_addr)
	.word	0
ENTRY(r8a7373_secondary_vector_sz)
	.word	. - r8a7373_secondary_vector
#endif
