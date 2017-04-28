/*
 * Driver for NCP6914
 *
 * Copyright (C) 2012 Renesas Mobile Corp.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_PMIC_NCP6914_H
#define _LINUX_PMIC_NCP6914_H

struct NCP6914_platform_data {
	unsigned int subpmu_pwron_gpio;
};


int subPMIC_PowerOn(int opt);
int subPMIC_PowerOff(int opt);
int subPMIC_PinOnOff(int pin, int on_off);

#endif
