#ifndef __ASM_ARCH_GPIO_KEY_H
#define __ASM_ARCH_GPIO_KEY_H

#ifdef CONFIG_KEYBOARD_SH_KEYSC
extern struct platform_device keysc_device;
#endif
extern struct platform_device gpio_key_device;
int gpio_key_init(int stm_select,
		unsigned int u2_board_rev,
		struct platform_device **u2evm_devices_stm_sdhi0,
		int u2evm_devices_stm_sdhi0_size,
		struct platform_device **u2evm_devices_stm_sdhi1,
		int u2evm_devices_stm_sdhi1_size,
		struct platform_device **u2evm_devices_stm_none,
		int u2evm_devices_stm_none_size);
#endif /* __ASM_ARCH_GPIO_KEY_H */
