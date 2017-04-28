#ifndef __ASM_BOARD_RENESAS_NFC_H
#define __ASM_BOARD_RENESAS_NFC_H

/* Define GPIO aliases */

#define NFC_EN_GPIO         GPIO_PORT12
#define NFC_IRQ_GPIO        GPIO_PORT13
#define NFC_FIRM_GPIO       GPIO_PORT101
#define NFC_I2C_SDA_GPIO    GPIO_PORT274
#define NFC_I2C_SCL_GPIO    GPIO_PORT273
#define NFC_I2C_BUS_ID      (8)

extern struct platform_device pn544_i2c_gpio_device;

extern void pn544_i2c_register_board_info(void);

extern void nfc_gpio_init(void);

#endif 
