#ifndef __ASM_MACH_SETUP_U2STM_H
#define __ASM_MACH_SETUP_U2STM_H

static struct resource stm_res[] = {
        [0] = {
                .name   = "stm_ctrl",
                .start  = 0xe6f89000,
                .end    = 0xe6f89fff,
                .flags  = IORESOURCE_MEM,
        },
        [1] = {
                .name   = "stm_ports",
                .start  = 0xe9000000,
                .end    = 0xe90fffff,
                .flags  = IORESOURCE_MEM,
        },
        [2] = {
                .name   = "funnel",
                .start  = 0xe6f8b000,
                .end    = 0xe6f8bfff,
                .flags  = IORESOURCE_MEM,
        },
};

static struct platform_device stm_device = {
        .name = "stm",
        .num_resources  = ARRAY_SIZE(stm_res),
        .resource       = stm_res,
};

extern int u2evm_init_stm_select(void);
extern int u2evm_get_stm_select(void);

#endif // __ASM_MACH_SETUP_U2STM_H
