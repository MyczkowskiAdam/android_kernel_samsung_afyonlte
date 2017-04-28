/* drivers/mfd/richtek/rt8969.h
 * Driver to Richtek RT8969 micro USB switch device
 *
 * Copyright (C) 2012
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RICHTEK_RT8969_H
#define __RICHTEK_RT8969_H
#include "rt_comm_defs.h"

#define RT8969_WAIT_DELAY   1

#define RT8969_REG_CHIP_ID         0x01
#define RT8969_REG_CONTROL_1       0x02
#define RT8969_REG_INT_FLAG        0x03
#define RT8969_REG_INTERRUPT_MASK  0x05
#define RT8969_REG_ADC             0x07
#define RT8969_REG_DEVICE_1        0x0A
#define RT8969_REG_DEVICE_2        0x0B
#define RT8969_REG_MANUAL_SW1      0x13
#define RT8969_REG_MANUAL_SW2      0x14
#define RT8969_REG_RESET           0x1B
#define RT8969_REG_CHG_CTRL1       0x20
#define RT8969_REG_CHG_CTRL2       0x21
#define RT8969_REG_CHG_CTRL3       0x22
#define RT8969_REG_CHG_INT_FLAG    0x23
#define RT8969_REG_CHG_STATUS      0x24

#define RT8969_INT_ATTACH_MASK     0x01
#define RT8969_INT_DETACH_MASK     0x02
#define RT8969_INT_CHGDET_MASK     0x04
#define RT8969_INT_DCDTIMEOUT_MASK 0x08
#define RT8969_INT_ADCCHG_MASK     0x40
#if 1 // modify for Samsung ivory (AP does not support TRIGGER_LOW)
#define RT8969_IRQF_MODE (IRQF_TRIGGER_FALLING)
#else
#define RT8969_IRQF_MODE (IRQF_TRIGGER_LOW)
#endif
#define RTMUSC_DRIVER_VER "1.1.2"

struct rt8969_data
{
    struct i2c_client *client;
    int32_t usbid_adc; // 5 bits
    int32_t factory_mode; // 0~3
    int32_t operating_mode;
    int32_t chip_id;
    int32_t accessory_id;
    struct work_struct work;
    int32_t irq;
    uint8_t prev_int_flag;
    uint8_t prev_chg_int_flag;
    struct wake_lock muic_wake_lock;
};


/* Initial Setting */

#define CHG_BIT  0x80
#define LDO_BIT  0x40
#define CHARGER_EN_SETTING (CHG_BIT|LDO_BIT)

#ifdef CONFIG_RT8969_IPRE_SET_MODE00
#define CONFIG_IPRE_SET 0x00
#elif defined(CONFIG_RT8969_IPRE_SET_MODE01)
#define CONFIG_IPRE_SET 0x04
#elif defined(CONFIG_RT8969_IPRE_SET_MODE10)
#define CONFIG_IPRE_SET 0x08
#elif defined(CONFIG_RT8969_IPRE_SET_MODE11)
#define CONFIG_IPRE_SET 0x0C
#endif
#define CHG_CTRL1_SETTING (CHARGER_EN_SETTING | CONFIG_IPRE_SET)

#ifdef CONFIG_RT8969_CM_AUTO
#define OPERATING_MODE 0
#elif defined(CONFIG_RT8969_CM_MANUAL)
#define OPERATING_MODE 1
#else
#error No operating mode defined
#endif

#ifdef CONFIG_RT8969_ICHG_SET_MODE0000
#define ICH_SET_BITS 0x00
#elif defined(CONFIG_RT8969_ICHG_SET_MODE0001)
#define ICH_SET_BITS 0x10
#elif defined(CONFIG_RT8969_ICHG_SET_MODE0010)
#define ICH_SET_BITS 0x20
#elif defined(CONFIG_RT8969_ICHG_SET_MODE0011)
#define ICH_SET_BITS 0x30
#elif defined(CONFIG_RT8969_ICHG_SET_MODE0100)
#define ICH_SET_BITS 0x40
#elif defined(CONFIG_RT8969_ICHG_SET_MODE0101)
#define ICH_SET_BITS 0x50
#elif defined(CONFIG_RT8969_ICHG_SET_MODE0110)
#define ICH_SET_BITS 0x60
#elif defined(CONFIG_RT8969_ICHG_SET_MODE0111)
#define ICH_SET_BITS 0x70
#elif defined(CONFIG_RT8969_ICHG_SET_MODE1000)
#define ICH_SET_BITS 0x80
#elif defined(CONFIG_RT8969_ICHG_SET_MODE1001)
#define ICH_SET_BITS 0x90
#else
#error Reversed ICH_SET
#endif

#ifdef CONFIG_RT8969_ECO_SET_MODE000
#define EOC_SET_BITS 0x00
#elif defined(CONFIG_RT8969_ECO_SET_MODE001)
#define EOC_SET_BITS 0x01
#elif defined(CONFIG_RT8969_ECO_SET_MODE010)
#define EOC_SET_BITS 0x02
#elif defined(CONFIG_RT8969_ECO_SET_MODE011)
#define EOC_SET_BITS 0x03
#elif defined(CONFIG_RT8969_ECO_SET_MODE100)
#define EOC_SET_BITS 0x04
#elif defined(CONFIG_RT8969_ECO_SET_MODE101)
#define EOC_SET_BITS 0x05
#elif defined(CONFIG_RT8969_ECO_SET_MODE110)
#define EOC_SET_BITS 0x06
#elif defined(CONFIG_RT8969_ECO_SET_MODE111)
#define EOC_SET_BITS 0x07
#else
#error Never select EOC_SET
#endif

#define CHG_CTRL2_SETTING (ICH_SET_BITS|EOC_SET_BITS)

#ifdef CONFIG_RT8969_VREG_MODE0000
#define CHG_CTRL3_SETTING 0x00
#elif defined(CONFIG_RT8969_VREG_MODE0001)
#define CHG_CTRL3_SETTING 0x10
#elif defined(CONFIG_RT8969_VREG_MODE0010)
#define CHG_CTRL3_SETTING 0x20
#elif defined(CONFIG_RT8969_VREG_MODE0011)
#define CHG_CTRL3_SETTING 0x30
#elif defined(CONFIG_RT8969_VREG_MODE0100)
#define CHG_CTRL3_SETTING 0x40
#elif defined(CONFIG_RT8969_VREG_MODE0101)
#define CHG_CTRL3_SETTING 0x50
#elif defined(CONFIG_RT8969_VREG_MODE0110)
#define CHG_CTRL3_SETTING 0x60
#elif defined(CONFIG_RT8969_VREG_MODE0111)
#define CHG_CTRL3_SETTING 0x70
#elif defined(CONFIG_RT8969_VREG_MODE1000)
#define CHG_CTRL3_SETTING 0x80
#elif defined(CONFIG_RT8969_VREG_MODE1001)
#define CHG_CTRL3_SETTING 0x90
#elif defined(CONFIG_RT8969_VREG_MODE1010)
#define CHG_CTRL3_SETTING 0xA0
#elif defined(CONFIG_RT8969_VREG_MODE1011)
#define CHG_CTRL3_SETTING 0xB0
#elif defined(CONFIG_RT8969_VREG_MODE1100)
#define CHG_CTRL3_SETTING 0xC0
#elif defined(CONFIG_RT8969_VREG_MODE1101)
#define CHG_CTRL3_SETTING 0xD0
#elif defined(CONFIG_RT8969_VREG_MODE1110)
#define CHG_CTRL3_SETTING 0xE0
#elif defined(CONFIG_RT8969_VREG_MODE1111)
#define CHG_CTRL3_SETTING 0xF0
#else
#error Never select RT8969_VREG_MODE
#endif


#endif
