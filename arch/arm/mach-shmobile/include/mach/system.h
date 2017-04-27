#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

#include <asm/system_misc.h>

extern void (*shmobile_arch_reset)(char mode, const char *cmd);

static inline void arch_idle(void)
{
	cpu_do_idle();
}

static inline void arch_reset(char mode, const char *cmd)
{
        /* call the CPU-specific reset function */
        if (shmobile_arch_reset)
                shmobile_arch_reset(mode, cmd);
        else
 		soft_restart(0);
}

#endif
