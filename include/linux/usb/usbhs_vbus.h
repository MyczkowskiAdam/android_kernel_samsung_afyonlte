/*
 * VBUS sensing driver for Renesas SH/R-Mobile USBHS module,
 * especially with an internal transceiver.
 *
 * Copyright (C) 2012  Renesas Electronics Corporation
 *
 * Highly inspired by gpio_vbus driver by:
 *
 * A simple GPIO VBUS sensing driver for B peripheral only devices
 * with internal transceivers.
 * Optionally D+ pullup can be controlled by a second GPIO.
 *
 * Copyright (c) 2008 Philipp Zabel <philipp.zabel@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/**
 * struct usbhs_vbus_mach_info - configuration for usbhs_vbus
 * @is_vbus_powered: check VBUS power supply status
 * @set_vbus_draw: set current limit (if it's NULL, regulator framework is used)
 * @wakeup: configure vbus_irq as a wake-up source
 */
struct usbhs_vbus_mach_info {
	int (*is_vbus_powered)(void);
	void (*set_vbus_draw)(int mA, int speed);
	bool wakeup;
};
