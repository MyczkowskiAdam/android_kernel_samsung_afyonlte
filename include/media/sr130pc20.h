/*
 * Driver for Samsung SR130PC20 1.3M Camera
 *
 * Copyright (C) 2013 Renesas Mobile Corp.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SR130PC20_H__
#define __SR130PC20_H__

#include <linux/device.h>

int SR130PC20_power(struct device *dev, int power_on);

#endif /* __SR130PC20_H__ */

