/* drivers/mfd/richtek/rt9450.h
 * Driver to Richtek RT9450 switch mode charger
 *
 * Copyright (C) 2012
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __RICHTEK_RT9450_H
#define __RICHTEK_RT9450_H
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include "rt_comm_defs.h"

#define RT9450_REG_STATUS       0x00
#define RT9450_REG_CTRL         0x01
#define RT9450_REG_VOLTAGE      0x02
#define RT9450_REG_CHIP_ID      0x03
#define RT9450_REG_CURRENT      0x04
#define RT9450_REG_CMFS         0x05
#define RT9450_REG_BMFS         0x06

#define RT9450A_CHIP_ID  0x81
#define RT9450B_CHIP_ID  0x89
#define RT9450C_CHIP_ID  0x91

struct rt_charge_params {
	uint32_t    currentmA;
	uint32_t    voltagemV;
	uint32_t    term_currentmA;
	uint32_t    enable_iterm;
	bool        enable;
};


struct rt9450_data
{
    struct i2c_client* client;
    struct rt_charge_params charge_params;
    struct delayed_work rt9450_charger_work;
    int32_t status_reg;
	int32_t control_reg;
	int32_t voltage_reg;
	int32_t chipid_reg;
	int32_t current_reg;
	int32_t cmfs_reg;
	int32_t bmfs_reg;
	int32_t chip_model;
	uint32_t active;
	bool charge_in_progress;
};

#endif
