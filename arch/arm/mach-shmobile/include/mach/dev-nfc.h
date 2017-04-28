#ifndef __ASM_BOARD_NFC_H
#define __ASM_BOARD_NFC_H

#if defined(CONFIG_PN547_NFC)
extern struct i2c_board_info PN547_info[];

extern struct platform_device PN547_i2c_gpio_device;

int pn547_info_size(void);

void pn547_device_i2c_register(void);
#endif

#endif 
