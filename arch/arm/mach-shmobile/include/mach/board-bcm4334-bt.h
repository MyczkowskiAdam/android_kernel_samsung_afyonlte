#ifndef __ASM_MACH_BOARD_U2EVM_RENESAS_BT_H
#define __ASM_MACH_BOARD_U2EVM_RENESAS_BT_H

extern struct platform_device bcm4334_bluetooth_device;

extern void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

int __init bcm4334_bluetooth_init(void);

#endif // __ASM_MACH_BOARD_U2EVM_RENESAS_BT_H
