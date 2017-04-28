/*
 * include/linux/pmic/pmic-tps80032.h
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <linux/rtc.h>
#include <mach/gpio.h>

#ifndef _LINUX_PMIC_TPS80032_H
#define _LINUX_PMIC_TPS80032_H

/* #define PMIC_DEBUG_ENABLE */
/* Define for use Non_volatile value */
/* #define PMIC_NON_VOLATILE_ENABLE */
#define PMIC_CHARGE_ENABLE
#define PMIC_FUELGAUGE_ENABLE

#define PINT_IRQ_BASE		512
#define pint2irq(bit)		(PINT_IRQ_BASE + (bit))

#define IRQPIN_IRQ_BASE		512

#define TPS80032_IRQ(_reg, _mask)\
	{\
		.mask_reg = (MSK_INT_LINE_##_reg) -\
				MSK_INT_LINE_A,	\
		.mask_mask = (_mask),\
	}

#define TPS80032_IRQ_SEC(_reg, _mask, _pint, _sint_mask_bit, _sint_sts_bit) \
	{								\
		.mask_reg = (MSK_INT_LINE_##_reg) -		\
				MSK_INT_LINE_A,		\
		.mask_mask = (_mask),					\
		.is_sec_int = true,					\
		.parent_int = TPS80032_INT_##_pint,			\
		.mask_sec_int_reg = HW_REG_CONTROLLER_INT_MASK,	\
		.int_mask_bit = MSK_CONTROLLER_STAT_##_sint_mask_bit,	\
		.int_sec_sts_reg = HW_REG_CONTROLLER_STAT1,		\
		.int_sts_bit = BIT_CONTROLLER_STAT1_##_sint_sts_bit	\
	}

struct tps80032_subdev_info {
	int		id;
	const char	*name;
	void		*platform_data;
};

#define NUM_PORT	5
#define GPIO_LOW	0
#define GPIO_HIGH	1
struct tps80032_platform_data {
	unsigned pin_gpio[NUM_PORT];
	unsigned pin_gpio_fn[NUM_PORT];
	unsigned portcr[NUM_PORT];
	u8 (*get_portcr_value)(u32 addr);
	void (*set_portcr_value)(u8 value, u32 addr);
	int num_subdevs;
	struct tps80032_subdev_info *subdevs;
	int irq_base;
};

struct tps80032_rtc_platform_data {
	int irq;
	struct rtc_time time;
};

enum {
	TPS80032_INT_PWRON,
	TPS80032_INT_RPWRON,
	TPS80032_INT_SYS_VLOW,
	TPS80032_INT_RTC_ALARM,
	TPS80032_INT_RTC_PERIOD,
	TPS80032_INT_HOT_DIE,
	TPS80032_INT_VXX_SHORT,
	TPS80032_INT_SPDURATION,
	TPS80032_INT_WATCHDOG,
	TPS80032_INT_BAT,
	TPS80032_INT_SIM,
	TPS80032_INT_MMC,
	TPS80032_INT_RES,
	TPS80032_INT_GPADC_RT,
	TPS80032_INT_GPADC_SW2_EOC,
	TPS80032_INT_CC_AUTOCAL,
	TPS80032_INT_ID_WKUP,
	TPS80032_INT_VBUSS_WKUP,
	TPS80032_INT_ID,
	TPS80032_INT_VBUS,
	TPS80032_INT_CHRG_CTRL,
	TPS80032_INT_EXT_CHRG,
	TPS80032_INT_INT_CHRG,
	TPS80032_INT_RES2,
	TPS80032_INT_BAT_TEMP_OVRANGE,
	TPS80032_INT_BAT_REMOVED,
	TPS80032_INT_VBUS_DET,
	TPS80032_INT_VAC_DET,
	TPS80032_INT_FAULT_WDG,
	TPS80032_INT_LINCH_GATED,

	/* Last interrupt id to get the end number */
	TPS80032_INT_NR,
};

enum {
	USB_SDP_FULL_SPEED = 0,
	USB_SDP_HIGH_SPEED,
	USB_CDP_FULL_SPEED,
	USB_CDP_HIGH_SPEED,
	USB_DCP,
};

struct tps80032_irq_data {
	u8 mask_reg;
	u8 mask_mask;
	u8 is_sec_int;
	u8 parent_int;
	u8 mask_sec_int_reg;
	u8 int_mask_bit;
	u8 int_sec_sts_reg;
	u8 int_sts_bit;
};

union power_ctrl_propval {
	int intval;
	const char *strval;
};

enum power_ctrl_property {
	POWER_CTRL_PROP_STATE = 0,
	POWER_CTRL_PROP_STATE_SLEEP,
	POWER_CTRL_PROP_VOLTAGE,
	POWER_CTRL_PROP_VOLTAGE_SLEEP,
};

struct power_ctrl {
	const char *name;
	enum power_ctrl_property *properties;
	size_t num_properties;

	int (*get_property)(struct power_ctrl *pctl,
				enum power_ctrl_property pcp,
				union power_ctrl_propval *val);
	int (*set_property)(struct power_ctrl *pctl,
				enum power_ctrl_property pcp,
				const union power_ctrl_propval *val);
	int (*property_is_writeable)(struct power_ctrl *pctl,
				enum power_ctrl_property pcp);

	struct device *dev;
};

/*
 * Define the address of the bin file which contain non-volatile value
 */
#ifdef PMIC_NON_VOLATILE_ENABLE
static void __iomem *virt_addr;

#define MAP_BASE_NV				0x48000000
#define MAP_BASE_PMIC_NV			0x0010800
#define MAP_SIZE_PMIC_NV			1280
#define get_map_data(base, offset)		__raw_readb(base + offset)
#endif

/*
 * Define the contanst value of HW register address
 */

#define HW_REG_PREQ1_RES_ASS_A				0xD7
#define HW_REG_PREQ1_RES_ASS_B				0xD8
#define HW_REG_PREQ1_RES_ASS_C				0xD9

#define HW_REG_PREQ2_RES_ASS_A				0xDA
#define HW_REG_PREQ2_RES_ASS_B				0xDB
#define HW_REG_PREQ2_RES_ASS_C				0xDC

#define HW_REG_PREQ3_RES_ASS_A				0xDD
#define HW_REG_PREQ3_RES_ASS_B				0xDE
#define HW_REG_PREQ3_RES_ASS_C				0xDF

#define HW_REG_PHOENIX_MSK_TRANSITION			0x20

#define HW_REG_CHARGEUSB_CTRL1				0xE8
#define HW_REG_CHARGEUSB_VSYSREG			0xDC
#define HW_REG_INT_STS_C				0xD2
#define HW_REG_INT_STS_B				0xD1
#define HW_REG_INT_STS_A				0xD0
#define HW_REG_INT_MSK_LINE_STS_A			0xD3
#define HW_REG_INT_MSK_LINE_STS_B			0xD4
#define HW_REG_INT_MSK_LINE_STS_C			0xD5
#define HW_REG_INT_MSK_STS_A				0xD6
#define HW_REG_INT_MSK_STS_B				0xD7
#define HW_REG_INT_MSK_STS_C				0xD8
#define HW_REG_CONTROLLER_STAT1				0xE3
#define HW_REG_CONTROLLER_CTRL1				0xE1
#define HW_REG_CONTROLLER_CTRL2				0xDA
#define HW_REG_CONTROLLER_INT_MASK			0xE0
#define HW_REG_CONTROLLER_VSEL_COMP			0xDB
#define HW_REG_CHARGERUSB_INT_MASK			0xE5
#define HW_REG_CHARGERUSB_STATUS_INT1			0xE6
#define HW_REG_CHARGERUSB_STATUS_INT2			0xE7
#define HW_REG_CHARGERUSB_CTRL1				0xE8
#define HW_REG_CHARGERUSB_CTRL2				0xE9
#define HW_REG_CHARGERUSB_CTRL3				0xEA
#define HW_REG_CHARGERUSB_CINLIMIT			0xEE
#define HW_REG_CHARGERUSB_CTRLLIMIT1			0xEF
#define HW_REG_CHARGERUSB_CTRLLIMIT2			0xF0
#define HW_REG_CHARGERUSB_VOREG				0xEC
#define HW_REG_CHARGERUSB_VICHRG			0xED
#define HW_REG_CHARGERUSB_VICHRG_PC			0xDE
#define HW_REG_CHARGERUSB_VSYSREG			0xDC
#define HW_REG_USB_ID_INT_SRC				0x0F
#define HW_REG_USB_ID_CTRL_SET				0x06
#define HW_REG_USB_ID_INT_LATCH_CLR			0x11
#define HW_REG_USB_ID_EN_LO_SET				0x12
#define HW_REG_USB_ID_EN_HI_SET				0x14
#define HW_REG_MISC1					0xE4
#define HW_REG_MISC2					0xE5
#define HW_REG_TOGGLE1					0x90
#define HW_REG_LINEAR_CHRG_STS				0xDE

#define HW_REG_GPSELECT_ISB				0x35
#define HW_REG_CTRL_P1					0x36
#define HW_REG_GPADC_CTRL				0x2E
#define HW_REG_GPADC_CTRL2				0x2F
#define HW_REG_GPCH0_MSB				0x3C
#define HW_REG_GPCH0_LSB				0x3B

#define HW_REG_PHOENIX_DEV_ON				0x25
#define HW_REG_VSYSMIN_HI_THRESHOLD			0x24
#define HW_REG_VBATMIN_HI_THRESHOLD			0x26

#define HW_REG_SMPS4_CFG_STATE				0x42
#define HW_REG_LDO1_CFG_STATE				0x9E
#define HW_REG_LDO2_CFG_STATE				0x86
#define HW_REG_LDO3_CFG_STATE				0x8E
#define HW_REG_LDO4_CFG_STATE				0x8A
#define HW_REG_LDO5_CFG_STATE				0x9A
#define HW_REG_LDO6_CFG_STATE				0x92
#define HW_REG_LDO7_CFG_STATE				0xA6
#define HW_REG_LDOUSB_CFG_STATE				0xA2

#define HW_REG_CLK32KAO_CFG_TRANS			0xBA
#define HW_REG_CLK32KAO_CFG_STATE			0xBB
#define HW_REG_CLK32KG_CFG_TRANS			0xBD
#define HW_REG_CLK32KG_CFG_STATE			0xBE
#define HW_REG_CLK32KAUDIO_CFG_TRANS			0xC0
#define HW_REG_CLK32KAUDIO_CFG_STATE			0xC1
#define HW_REG_BBSPOR_CFG				0xE6

#define HW_REG_GPADC_TRIM1				0xCD
#define HW_REG_GPADC_TRIM2				0xCE
#define HW_REG_GPADC_TRIM3				0xCF
#define HW_REG_GPADC_TRIM4				0xD0
#define HW_REG_GPADC_TRIM5				0xD1
#define HW_REG_GPADC_TRIM6				0xD2

#define HW_REG_LDO1_CFG_VOLTAGE				0x9F
#define HW_REG_LDO2_CFG_VOLTAGE				0x87
#define HW_REG_LDO3_CFG_VOLTAGE				0x8F
#define HW_REG_LDO4_CFG_VOLTAGE				0x8B
#define HW_REG_LDO5_CFG_VOLTAGE				0x9B
#define HW_REG_LDO6_CFG_VOLTAGE				0x93
#define HW_REG_LDO7_CFG_VOLTAGE				0xA7

#define HW_REG_SMPS1_CFG_TRANS				0x53
#define HW_REG_SMPS2_CFG_TRANS				0x59
#define HW_REG_SMPS3_CFG_TRANS				0x65
#define HW_REG_SMPS4_CFG_TRANS				0x41
#define HW_REG_SMPS5_CFG_TRANS				0x47
#define HW_REG_LDO1_CFG_TRANS				0x9D
#define HW_REG_LDO2_CFG_TRANS				0x85
#define HW_REG_LDO3_CFG_TRANS				0x8D
#define HW_REG_LDO4_CFG_TRANS				0x89
#define HW_REG_LDO5_CFG_TRANS				0x99
#define HW_REG_LDO6_CFG_TRANS				0x91
#define HW_REG_LDO7_CFG_TRANS				0xA5
#define HW_REG_LDOLN_CFG_TRANS				0x95

#define HW_REG_FG_REG_00				0xC0
#define HW_REG_FG_REG_01				0xC1
#define HW_REG_FG_REG_02				0xC2
#define HW_REG_FG_REG_03				0xC3
#define HW_REG_FG_REG_04				0xC4
#define HW_REG_FG_REG_05				0xC5
#define HW_REG_FG_REG_06				0xC6
#define HW_REG_FG_REG_07				0xC7
#define HW_REG_FG_REG_08				0xC8
#define HW_REG_FG_REG_09				0xC9

/*
 * Define the mask value from bit 0 to bit 7
 */

#define MSK_BIT_0					0x01
#define MSK_BIT_1					0x02
#define MSK_BIT_2					0x04
#define MSK_BIT_3					0x08
#define MSK_BIT_4					0x10
#define MSK_BIT_5					0x20
#define MSK_BIT_6					0x40
#define MSK_BIT_7					0x80

#define MSK_POWER_STATE					0x03
#define MSK_POWER_VOLTAGE				0x1F

#define TPS80032_STATE_ON				(1)
#define TPS80032_STATE_OFF				(0)

#define CLK32KAO					(0)
#define CLK32KG						(1)
#define CLK32KAUDIO					(2)

#define RT_CPU_SIDE					(0x01)
#define SYS_CPU_SIDE					(0x40)
#define BB_CPU_SIDE					(0x93)

/*
 * Define the contain non-volatile value
 */


#ifdef PMIC_NON_VOLATILE_ENABLE
/* If use BIN */
#define CONST_TIMER_UPDATE		(get_map_data(virt_addr, 0x00)\
					| (get_map_data(virt_addr, 0x01) << 8))

#define CONST_WAIT_TIME			get_map_data(virt_addr, 0x02)
#define CONST_WAIT_TIME_CURRENT		get_map_data(virt_addr, 0x03)
#define CONST_0C_DEGREE			(get_map_data(virt_addr, 0x04)\
					| (get_map_data(virt_addr, 0x05) << 8))

#define CONST_CONVERT_VOLT		(get_map_data(virt_addr, 0x06)\
					| (get_map_data(virt_addr, 0x07) << 8))

#define CONST_BAT_MIN			get_map_data(virt_addr, 0x08)
#define THR_BAT_FULL			get_map_data(virt_addr, 0x09)
#define THR_BAT_HIGH			get_map_data(virt_addr, 0x0A)
#define THR_BAT_NORMAL			get_map_data(virt_addr, 0x0B)
#define THR_BAT_LOW			get_map_data(virt_addr, 0x0C)
#define MSK_GET_EXT_DEVICE		get_map_data(virt_addr, 0x0D)
#define MSK_GET_INT_SRC_A		get_map_data(virt_addr, 0x0E)
#define MSK_GET_INT_SRC_C		get_map_data(virt_addr, 0x0F)
#define MSK_INT_LINE_A			get_map_data(virt_addr, 0x10)
#define MSK_INT_LINE_B			get_map_data(virt_addr, 0x11)
#define MSK_INT_LINE_C			get_map_data(virt_addr, 0x12)
#define MSK_INT_SRC_A			get_map_data(virt_addr, 0x13)
#define MSK_INT_SRC_B			get_map_data(virt_addr, 0x14)
#define MSK_INT_SRC_C			get_map_data(virt_addr, 0x15)
#define MSK_DISABLE			get_map_data(virt_addr, 0x16)
#define MSK_CONTROLLER_INT		get_map_data(virt_addr, 0x17)
#define MSK_CHARGERUSB_INT		get_map_data(virt_addr, 0x18)
#define MSK_PREQ1_ASS_A			get_map_data(virt_addr, 0x19)
#define MSK_PREQ1_ASS_B			get_map_data(virt_addr, 0x1A)
#define MSK_PREQ2_ASS_A			get_map_data(virt_addr, 0x1B)
#define MSK_PREQ3_ASS_B			get_map_data(virt_addr, 0x1C)
#define MSK_TRANSITION			get_map_data(virt_addr, 0x1D)
#define MSK_GPADC			get_map_data(virt_addr, 0x1E)
#define MSK_GG_ENABLE			get_map_data(virt_addr, 0x1F)
#define MSK_GG_DISABLE			get_map_data(virt_addr, 0x20)
#define CONST_INT_ID			get_map_data(virt_addr, 0x21)
#define CONST_X1			(get_map_data(virt_addr, 0x22)\
					| (get_map_data(virt_addr, 0x23) << 8))

#define CONST_X2			(get_map_data(virt_addr, 0x24)\
					| (get_map_data(virt_addr, 0x25) << 8))

#define CONST_LDOLN_CFG_TRANS		get_map_data(virt_addr, 0x26)
#define CONST_LDO6_CFG_TRANS		get_map_data(virt_addr, 0x27)
#define CONST_VAC_CURRENT_LIMIT		get_map_data(virt_addr, 0x28)
#define CONST_DEF_CURRENT_LIMIT		get_map_data(virt_addr, 0x29)
#define CONST_VBATMIN_HI		get_map_data(virt_addr, 0x2A)
#define CONST_VSYSMIN_HI		get_map_data(virt_addr, 0x2B)
#define CONST_VOREG			get_map_data(virt_addr, 0x2C)
#define CONST_VICHRG			get_map_data(virt_addr, 0x2D)
#define CONST_VICHRG_PC			get_map_data(virt_addr, 0x2E)
#define CONST_VSEL_COMP			get_map_data(virt_addr, 0x2F)
#define CONST_HPB_WAIT			get_map_data(virt_addr, 0x30)
#define CONST_BATTERY_CURRENT_UPDATE	get_map_data(virt_addr, 0x31)
#define CONST_CHRG_CTRL2		get_map_data(virt_addr, 0x32)
#define CONST_CTRLLIMIT1		get_map_data(virt_addr, 0x33)
#define CONST_CTRLLIMIT2		get_map_data(virt_addr, 0x34)
#define CONST_I2C_RETRY			get_map_data(virt_addr, 0x35)

#else
/* If no use BIN */
#define CONST_TIMER_UPDATE			0x1388
#define CONST_BATTERY_CURRENT_UPDATE		250
#define CONST_WAIT_TIME				0x05
#define CONST_WAIT_TIME_CURRENT			0xFF
#define CONST_0C_DEGREE				2566	/* 0 degree */
#define CONST_CONVERT_VOLT			0x03E8
#define CONST_BAT_MIN				0x01
#define THR_BAT_FULL				0x64
#define THR_BAT_HIGH				0x46
#define THR_BAT_NORMAL				0x0F
#define THR_BAT_LOW				0x05
#define MSK_GET_EXT_DEVICE			0x0E
#define MSK_GET_INT_SRC_A			0xFF
#define MSK_GET_INT_SRC_C			0x7F
#define MSK_INT_LINE_A				0x44
#define MSK_INT_LINE_B				0xFF
#define MSK_INT_LINE_C				0x8A
#define MSK_INT_SRC_A				0x44
#define MSK_INT_SRC_B				0xFF
#define MSK_INT_SRC_C				0x8A
#define MSK_DISABLE				0x00
#define MSK_CONTROLLER_INT			0x2C
#define MSK_CHARGERUSB_INT			0x16
#define MSK_PREQ1_ASS_A				0x11
#define MSK_PREQ1_ASS_B				0x80
#define MSK_PREQ2_ASS_A				0x08
#define MSK_PREQ3_ASS_B				0x80
#define MSK_TRANSITION				0x00
#define MSK_GPADC				0x0E
#define MSK_GG_ENABLE				0xA0
#define MSK_GG_DISABLE				0x50
#define CONST_CTRLLIMIT1			0x23
#define CONST_CTRLLIMIT2			0x0E
#define CONST_INT_ID				0x1C
#define CONST_X1				0x05A1
#define CONST_X2				0x0CCC
#define CONST_LDOLN_CFG_TRANS			0x01
#define CONST_LDO6_CFG_TRANS			0x01
#define CONST_VAC_CURRENT_LIMIT			0x01	/* 100mA */
#define CONST_DEF_CURRENT_LIMIT			0x09	/* 500mA */
#define CONST_VBATMIN_HI			0x1C	/* 3.4 V */
#define CONST_VSYSMIN_HI			0x17	/* 3.15 V */
#define CONST_VOREG				0x23	/* 4.20V */
#define CONST_VICHRG				0x02	/* 300mA with POP = 1*/
#define CONST_VICHRG_PC				0x00	/* 100mA */
#define CONST_VSEL_COMP				0x58
#define CONST_CHRG_CTRL2			0x01
#define CONST_HPB_WAIT				25
#define CONST_I2C_RETRY				3
#endif

/* Define interrupt bit for interrupt register */
#define MSK_CONTROLLER_STAT_MVAC_DET		0
#define MSK_CONTROLLER_STAT_MVBUS_DET		1
#define MSK_CONTROLLER_STAT_MBAT_TEMP		2
#define MSK_CONTROLLER_STAT_MFAULT_WDG		3
#define MSK_CONTROLLER_STAT_MBAT_REMOVED	4
#define MSK_CONTROLLER_STAT_MLINCH_GATED	5

#define BIT_CONTROLLER_STAT1_BAT_TEMP		0
#define BIT_CONTROLLER_STAT1_BAT_REMOVED	1
#define BIT_CONTROLLER_STAT1_VBUS_DET		2
#define BIT_CONTROLLER_STAT1_VAC_DET		3
#define BIT_CONTROLLER_STAT1_FAULT_WDG		4
#define BIT_CONTROLLER_STAT1_LINCH_GATED	6


#define RESOURCE_COUNTER_MAX			13
#define RESOURCE_POWER_CTRL_PROP_MAX		7

#endif
