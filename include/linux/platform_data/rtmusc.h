/* include/linux/platform_data/rtmusc.h
 * Driver to Richtek RT8969 micro USB switch device
 *
 * Copyright (C) 2012
 * Author: Patrick Chang <weichung.chang@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RTMUSC_H_
#define __RTMUSC_H_
#include <linux/types.h>

#define RTMUSC_FM_NONE          0x00
#define RTMUSC_FM_BOOT_ON_UART  0x01
#define RTMUSC_FM_BOOT_OFF_UART 0x02
#define RTMUSC_FM_BOOT_ON_USB   0x03
#define RTMUSC_FM_BOOT_OFF_USB  0x04


struct rtmus_platform_data {
        void (*usb_callback) (uint8_t attached);
	void (*usb_cdp_callback) (uint8_t attached);
        void (*uart_callback) (uint8_t attached);
        void (*charger_callback) (uint8_t attached);
        void (*jig_callback) (uint8_t attached,uint8_t factory_mode);
        void (*over_temperature_callback)(uint8_t detected);
        void (*charging_complete_callback)(void);
        int (*ex_init)(void);
        void (*over_voltage_callback)(uint8_t detected);
	void (*usb_power) (uint8_t on); // 0 --> suspend  1 --> resume
	void (*otg_callback)(uint8_t attached);
};

#endif /* __RTMUSC_H_ */
