/*
* d2153-regulator.c: Regulator driver for Dialog D2153
*
* Copyright(c) 2011 Dialog Semiconductor Ltd.
*
* Author: Dialog Semiconductor Ltd. D. Chen
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <linux/d2153/pmic.h>
#include <linux/d2153/d2153_reg.h>
#include <linux/d2153/hwmon.h>
#include <linux/d2153/core.h>

#include <mach/common.h>

#define DRIVER_NAME                 "d2153-regulator"

struct regl_register_map {
	u8 v_reg;
	u8 v_bit;
	u8 v_len;
	u8 v_mask;
	u8 en_reg;
	u8 en_mask;
	u8 mctl_reg;
	u8 dsm_opmode;
	u8 ramp_reg;
	u8 ramp_bit;
};

struct d2153_reg_info {
	u32 min_uVolts;
	u32 uVolt_step;
	u32 max_uVolts;
};

extern struct d2153 *d2153_regl_info;

#define D2153_NO_V_CONTROL_REG 0
#define D2153_NO_V_CONTROL_BIT 0
#define D2153_NO_V_CONTROL_LEN 0
#define D2153_NO_V_CONTROL_MASK 0
#define D2153_DEFINE_REGL(_name, _v_reg_, _vn_, _e_reg_, _en_, _m_reg_, _ramp_reg, _ramp_bit) \
	[D2153_##_name] = \
		{ \
			.v_reg = D2153_##_v_reg_##_REG, \
			.v_bit = D2153_##_vn_##_BIT, \
			.v_len =  D2153_##_vn_##_LEN, \
			.v_mask =  D2153_##_vn_##_MASK, \
			.en_reg = D2153_##_e_reg_##_REG, \
			.en_mask = D2153_##_en_##_MASK, \
			.mctl_reg = D2153_##_m_reg_##_REG, \
			.ramp_reg = _ramp_reg, \
			.ramp_bit = _ramp_bit, \
		}

static struct regl_register_map regulator_register_map[] = {
	D2153_DEFINE_REGL(BUCK_1,   BUCK2PH_BUCK1,    VB2PH_VBUCK_DP,    BUCK2PH_BUCK1,    B2PH_EN_BUCK1_EN,       BUCK1_MCTL,    0,                0),
	D2153_DEFINE_REGL(BUCK_2,   BUCKCORE_BUCK2,   VBCORE_VBUCK2,     BUCKCORE_BUCK2,   BCORE_EN_BUCK2_EN,      BUCK2_MCTL,    D2153_SUPPLY_REG, D2153_VBUCK2_GO_MASK),
	D2153_DEFINE_REGL(BUCK_3,   BUCKPRO_BUCK3,    VBPRO_VBUCK3,      BUCKPRO_BUCK3,    BPRO_EN_BUCK3_EN,       BUCK3_MCTL,    D2153_SUPPLY_REG, D2153_VBUCK3_GO_MASK),
	D2153_DEFINE_REGL(BUCK_4,   BUCKMEM_BUCK4,    VBMEM_VBUCK4,      BUCKMEM_BUCK4,    BMEM_EN_BUCK4_EN,       BUCK4_MCTL,    D2153_SUPPLY_REG, D2153_VBUCK4_GO_MASK),
	D2153_DEFINE_REGL(BUCK_5,   BUCKPERI_BUCK5,   VBPERI_VBUCK5,     BUCKPERI_BUCK5,   BPERI_EN_BUCK5_EN,      BUCK5_MCTL,    D2153_SUPPLY_REG, D2153_VBUCK5_GO_MASK),
	D2153_DEFINE_REGL(BUCK_6,   NO_V_CONTROL,     NO_V_CONTROL,      BUCKRF_CONF,      RFBUCK_EN,              BUCK_RF_MCTL,  0, 0),
	D2153_DEFINE_REGL(LDO_1,    LDO1,             VLDO1,             LDO1,             LDO1_EN,                LDO1_MCTL,     0, 0),
	D2153_DEFINE_REGL(LDO_2,    LDO2,             VLDO2,             LDO2,             LDO2_EN,                LDO2_MCTL,     0, 0),
	D2153_DEFINE_REGL(LDO_3,    LDO3,             VLDO3,             LDO3,             LDO3_EN,                LDO3_MCTL,     0, 0),
	D2153_DEFINE_REGL(LDO_4,    LDO4,             VLDO4,             LDO4,             LDO4_EN,                LDO4_MCTL,     0, 0),
	D2153_DEFINE_REGL(LDO_5,    LDO5,             VLDO5,             LDO5,             LDO5_EN,                LDO5_MCTL,     0, 0),
	D2153_DEFINE_REGL(LDO_6,    LDO6,             VLDO6,             LDO6,             LDO6_EN,                LDO6_MCTL,     0, 0),
	D2153_DEFINE_REGL(LDO_7,    LDO7,             VLDO7,             LDO7,             LDO7_EN,                LDO7_MCTL,     0, 0),
	D2153_DEFINE_REGL(LDO_8,    LDO8,             VLDO8,             LDO8,             LDO8_EN,                LDO8_MCTL,     0, 0),
	D2153_DEFINE_REGL(LDO_9,    LDO9,             VLDO9,             LDO9,             LDO9_EN,                LDO9_MCTL,     0, 0),
	D2153_DEFINE_REGL(LDO_10,   LDO10,            VLDO10,            LDO10,            LDO10_EN,               LDO10_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_11,   LDO11,            VLDO11,            LDO11,            LDO11_EN,               LDO11_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_12,   LDO12,            VLDO12,            LDO12,            LDO12_EN,               LDO12_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_13,   LDO13,            VLDO13,            LDO13,            LDO13_EN,               LDO13_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_14,   LDO14,            VLDO14,            LDO14,            LDO14_EN,               LDO14_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_15,   LDO15,            VLDO15,            LDO15,            LDO15_EN,               LDO15_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_16,   LDO16,            VLDO16,            LDO16,            LDO16_EN,               LDO16_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_17,   LDO17,            VLDO17,            LDO17,            LDO17_EN,               LDO17_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_18,   LDO18_LDO_VRFANA, VLDO18_VLDO_VRFANA,LDO18_LDO_VRFANA, LDO18_EN_LDO_VRFANA_EN, LDO18_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_19,   LDO19_LDO_19,     VLDO19,            LDO19_LDO_19,     LDO19_EN,               LDO19_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_20,   LDO20_LDO_20,     VLDO20,            LDO20_LDO_20,     LDO20_EN,               LDO20_MCTL,    0, 0),
	D2153_DEFINE_REGL(LDO_AUD1, LDO21_LDO_AUD1,   VLDO21_VLDO_AUD1,  LDO21_LDO_AUD1,   LDO21_EN_LDO_AUD1_EN,   LDO21_MCTL, 0, 0),
	D2153_DEFINE_REGL(LDO_AUD2, LDO22_LDO_AUD2,   VLDO22_VLDO_AUD2,  LDO22_LDO_AUD2,   LDO22_EN_LDO_AUD2_EN,   LDO22_MCTL, 0, 0),
};

#define D2153_DEFINE_INFO(_name, _vn_) \
	[D2153_##_name] = \
		{ \
		.min_uVolts = D2153_##_vn_##_VOLT_LOWER, \
		.max_uVolts = D2153_##_vn_##_VOLT_UPPER, \
		.uVolt_step = D2153_##_vn_##_VOLT_WIDTH, \
		}

static struct d2153_reg_info d2153_regulator_info[D2153_NUMBER_OF_REGULATORS] = {
	D2153_DEFINE_INFO(BUCK_1,BUCK1),
	D2153_DEFINE_INFO(BUCK_2,BUCK2),
	D2153_DEFINE_INFO(BUCK_3,BUCK3),
	D2153_DEFINE_INFO(BUCK_4,BUCK4),
	D2153_DEFINE_INFO(BUCK_5,BUCK5),
	D2153_DEFINE_INFO(BUCK_6,BUCK6),
	D2153_DEFINE_INFO(LDO_1,LDO1),
	D2153_DEFINE_INFO(LDO_2,LDO2),
	D2153_DEFINE_INFO(LDO_3,LDO3),
	D2153_DEFINE_INFO(LDO_4,LDO4),
	D2153_DEFINE_INFO(LDO_5,LDO5),
	D2153_DEFINE_INFO(LDO_6,LDO6),
	D2153_DEFINE_INFO(LDO_7,LDO7),
	D2153_DEFINE_INFO(LDO_8,LDO8),
	D2153_DEFINE_INFO(LDO_9,LDO9),
	D2153_DEFINE_INFO(LDO_10,LDO10),
	D2153_DEFINE_INFO(LDO_11,LDO11),
	D2153_DEFINE_INFO(LDO_12,LDO12),
	D2153_DEFINE_INFO(LDO_13,LDO13),
	D2153_DEFINE_INFO(LDO_14,LDO14),
	D2153_DEFINE_INFO(LDO_15,LDO15),
	D2153_DEFINE_INFO(LDO_16,LDO16),
	D2153_DEFINE_INFO(LDO_17,LDO17),
	D2153_DEFINE_INFO(LDO_18,LDO18),
	D2153_DEFINE_INFO(LDO_19,LDO19),
	D2153_DEFINE_INFO(LDO_20,LDO20),
	D2153_DEFINE_INFO(LDO_AUD1,LDOAUD1),
	D2153_DEFINE_INFO(LDO_AUD2,LDOAUD2),
};

static int mctl_status;   /* default is disable */
static struct regulator_dev *d2153_rdev[D2153_NUMBER_OF_REGULATORS];

static int d2153_register_regulator(struct d2153 *d2153, int reg, struct regulator_init_data *initdata);

static int is_mode_control_enabled(void)
{
	/* 0 : mctl disable, 1 : mctl enable */
	return mctl_status;
}

/*
 * get_global_mctl_mode
 */
static inline int get_global_mctl_mode(struct d2153 *d2153)
{
	u8 reg_val;

	d2153_reg_read(d2153, D2153_STATUS_A_REG, &reg_val);

	/* Remove "NOT" operation */
	return ((reg_val & D2153_M_CTL_MASK) >> D2153_M_CTL_BIT);
}

/*
 * get_regulator_mctl_mode
 */
static unsigned int get_regulator_mctl_mode(struct d2153 *d2153, int regulator_id)
{
	u8 reg_val, mctl_reg;
	int ret = 0;

	if (regulator_id < 0 || regulator_id >= D2153_NUMBER_OF_REGULATORS)
		return -EINVAL;
	mctl_reg = regulator_register_map[regulator_id].mctl_reg;

	ret = d2153_reg_read(d2153, mctl_reg, &reg_val);
	reg_val &= D2153_REGULATOR_MCTL0;
	reg_val >>= D2153_REG_MCTL0_SHIFT;

	return reg_val;
}

/*
 * set_regulator_mctl_mode
 */
static int set_regulator_mctl_mode(struct d2153 *d2153, int regulator_id, u8 mode)
{
	u8 reg_val, mctl_reg;
	int ret = 0;

	if (regulator_id < 0 || regulator_id >= D2153_NUMBER_OF_REGULATORS)
		return -EINVAL;
	if (mode > REGULATOR_MCTL_TURBO)
		return -EINVAL;

	mctl_reg = regulator_register_map[regulator_id].mctl_reg;
	ret = d2153_reg_read(d2153, mctl_reg, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= ~(D2153_REGULATOR_MCTL0 | D2153_REGULATOR_MCTL2);
	reg_val |= ((mode << D2153_REG_MCTL0_SHIFT) | ( mode << D2153_REG_MCTL2_SHIFT));
	ret = d2153_reg_write(d2153, mctl_reg, reg_val);
	dlg_info("[REGULATOR] %s. reg_val = 0x%X\n", __func__, reg_val);

	return ret;
}

/*
 * d2153_set_mctl_enable
 */
void d2153_set_mctl_enable(void)
{
	u8 reg_val;

	if (d2153_regl_info) {
		d2153_reg_read(d2153_regl_info, D2153_POWER_CONT_REG, &reg_val);
		reg_val |= D2153_MCTRL_EN_MASK;
		d2153_reg_write(d2153_regl_info, D2153_POWER_CONT_REG, reg_val);
		mctl_status = 1;
	}
	else {
		dlg_err("MCTRL_EN bit is not set\n");
		return;
	}
}
EXPORT_SYMBOL(d2153_set_mctl_enable);


/*
 * d2153_set_mctl_enable
 */
void d2153_lcd_power_on(void)
{
	u8 reg_val;

	if (d2153_regl_info) {
		reg_val = 0x56;
		d2153_reg_write(d2153_regl_info,D2153_LDO8_MCTL_REG,reg_val);
		d2153_reg_write(d2153_regl_info,D2153_LDO9_MCTL_REG,reg_val);
	}
	else {
		dlg_err("d2153_lcd_power_on bit is not set\n");
		return;
	}
}
EXPORT_SYMBOL(d2153_lcd_power_on);


/*
 * d2153_clk32k_enable
 */
void d2153_clk32k_enable(int onoff)
{
	if (d2153_regl_info) {
		dlg_info("[%s] OUT1_32K onoff ->[%d]\n", __func__, onoff);

		if (onoff == 1)
			d2153_set_bits(d2153_regl_info,  D2153_OUT2_32K_ONKEY_CONT_REG, D2153_OUT2_32K_EN_MASK);
		else
			d2153_clear_bits(d2153_regl_info,  D2153_OUT2_32K_ONKEY_CONT_REG, D2153_OUT2_32K_EN_MASK);
	}
	else {
		dlg_err("Failed OUT1_32K on/off\n");
		return;
	}
}
EXPORT_SYMBOL(d2153_clk32k_enable);


/*
 * d2153_platform_regulator_init
 */
int d2153_platform_regulator_init(struct d2153 *d2153)
{
	int i;
	u8 reg_val=0;
	struct d2153_regl_init_data *regl_data = d2153->pdata->regulator_data;

	if (regl_data == NULL)
		return -1;

	for(i = D2153_BUCK_1; i < D2153_NUMBER_OF_REGULATORS; i++) {
		d2153_register_regulator(d2153, i, (regl_data + i)->initdata);
		regulator_register_map[i].dsm_opmode = d2153->pdata->regl_map[i].default_pm_mode;
		d2153_reg_write(d2153, regulator_register_map[i].mctl_reg, d2153->pdata->regl_map[i].dsm_opmode);
	}

	/* set MISC_MCTL */
	reg_val = 0x0F;
	d2153_reg_write(d2153_regl_info, D2153_MISC_MCTL_REG, reg_val);

#if 0	// 20130720 remove
	d2153_reg_write(d2153_regl_info,D2153_BUCK_D_REG,0x67);
#endif
	return 0;
}
EXPORT_SYMBOL(d2153_platform_regulator_init);

/*
 * d2153_regulator_val_to_uVolts
 */
static int d2153_regulator_val_to_uVolts(unsigned int val, int regulator_id)
{
	u32 min_uVolts = d2153_regulator_info[regulator_id].min_uVolts;
	u32 uVolt_step = d2153_regulator_info[regulator_id].uVolt_step;
	u32 max_uVolts = d2153_regulator_info[regulator_id].max_uVolts;
	u32 val_uVolts = min_uVolts + val * uVolt_step;

	if (val_uVolts > max_uVolts)
		return max_uVolts;
	else
		return val_uVolts;
}

/*
 * d2153_regulator_uvolts_to_val
 */
static unsigned int d2153_regulator_uvolts_to_val(int uV, int regulator_id)
{
	u32 min_uVolts = d2153_regulator_info[regulator_id].min_uVolts;
	u32 uVolt_step = d2153_regulator_info[regulator_id].uVolt_step;

	if (uVolt_step)
		return (uV - min_uVolts) / uVolt_step;
	else
		return 0;
}

/*
 * d2153_regulator_set_voltage
 */
static int d2153_regulator_set_voltage(struct regulator_dev *rdev, int min_uV, int max_uV,unsigned *selector)
{
	struct d2153 *d2153 = rdev_get_drvdata(rdev);
	int value, val_uV;
	unsigned int reg_num, regulator_id = rdev_get_id(rdev);
	u8 supply, control;
	int ret = 0;
	*selector = -1;

	/* before we do anything check the lock bit */
	ret = d2153_reg_read(d2153, D2153_SUPPLY_REG, &supply);
	if (supply & D2153_V_LOCK_MASK)
		d2153_clear_bits(d2153, D2153_SUPPLY_REG, D2153_V_LOCK_MASK);

	value =  d2153_regulator_uvolts_to_val(min_uV, regulator_id);

	reg_num = regulator_register_map[regulator_id].v_reg;
	val_uV = d2153_regulator_val_to_uVolts(value, regulator_id);

	dlg_info("%s(), reg[%d] = register %02X min_uV=%d max_uV=%d value=%02X val_uV=%d\n", __func__, regulator_id, reg_num, min_uV, max_uV, value, val_uV);

	/* Sanity check for maximum value */
	if (val_uV > max_uV)
		return -EINVAL;

	ret = d2153_reg_read(d2153, reg_num, &control);
	control &= ~regulator_register_map[regulator_id].v_mask;
	control |= value << regulator_register_map[regulator_id].v_bit;

	d2153_reg_write(d2153, reg_num, control);

	dlg_info("%s(), regl_id=%d, reg_num=%02X, value = %02X\n", __func__, regulator_id, reg_num, value);

	if (regulator_register_map[regulator_id].ramp_reg) /* For BUCKs enable the ramp */
		d2153_set_bits(d2153,regulator_register_map[regulator_id].ramp_reg, regulator_register_map[regulator_id].ramp_bit);

	*selector = regulator_id;

	return ret;
}

/*
 * d2153_regulator_get_voltage
 */
static int d2153_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct d2153 *d2153 = rdev_get_drvdata(rdev);
	unsigned int reg_num, regulator_id = rdev_get_id(rdev);
	int ret;
	u8 val;

	reg_num = regulator_register_map[regulator_id].v_reg;

	dlg_info("%s(), regulator_id[%d] => register %d\n", __func__, regulator_id, reg_num);

	ret = d2153_reg_read(d2153, reg_num, &val);
	val &= regulator_register_map[regulator_id].v_mask;
	val >>= regulator_register_map[regulator_id].v_bit;
	ret = d2153_regulator_val_to_uVolts(val, regulator_id);

	return ret;
}

/*
 * d2153_regulator_enable
 */
static int d2153_regulator_enable(struct regulator_dev *rdev)
{
	struct d2153 *d2153 = rdev_get_drvdata(rdev);
	u8 reg_val;
	int ret = 0;
	unsigned int regulator_id = rdev_get_id(rdev);
	unsigned int reg_num;
	int i;

	if ((rdev_get_id(rdev) == 20) || (rdev_get_id(rdev) == 15)){
		printk(KERN_INFO "%s:>> rdev_get_id %d\n", 
				__func__, rdev_get_id(rdev));
	}

	if (regulator_id >= D2153_NUMBER_OF_REGULATORS)
		return -EINVAL;

#if 1	// 20130720 add
	if((regulator_id == D2153_BUCK_5) || (regulator_id == D2153_BUCK_6)) {
		dlg_info("	 => Not used, regulator_id[%d]\n", regulator_id);
		return 0;
	}
#endif

	if (!is_mode_control_enabled()) {
		reg_num = regulator_register_map[regulator_id].en_reg;

		d2153_reg_read(d2153, reg_num, &reg_val);
		reg_val |= regulator_register_map[regulator_id].en_mask;
		ret = d2153_reg_write(d2153, reg_num,reg_val);
		dlg_info("%s : regulator[%d] register %02x ==> %02x\n", __func__,regulator_id,reg_num,reg_val);
	}else {
		reg_num = regulator_register_map[regulator_id].mctl_reg;

		ret = d2153_reg_read(d2153, reg_num, &reg_val);

		if (ret < 0) {
			dlg_err("I2C read error\n");
			for(i=0; i<3 ; i++) {
				ret = d2153_reg_read(d2153, reg_num, &reg_val);
				if(ret == 0) {
					break;
				}
			}
			if(ret < 0) {
				reg_val = 0x56;
			}
		}

		reg_val &= ~(D2153_REGULATOR_MCTL1 | D2153_REGULATOR_MCTL3);   /* Clear MCTL11 and MCTL01 */
		reg_val |= (D2153_REGULATOR_MCTL1_ON | D2153_REGULATOR_MCTL3_ON);

		switch(regulator_register_map[regulator_id].dsm_opmode) {
			case D2153_REGULATOR_LPM_IN_DSM :
				reg_val &= ~(D2153_REGULATOR_MCTL0 | D2153_REGULATOR_MCTL2);
				reg_val |= (D2153_REGULATOR_MCTL0_SLEEP | D2153_REGULATOR_MCTL2_SLEEP);
				break;
			case D2153_REGULATOR_OFF_IN_DSM :
				reg_val &= ~(D2153_REGULATOR_MCTL0 | D2153_REGULATOR_MCTL2);
				break;
			case D2153_REGULATOR_ON_IN_DSM :
				reg_val &= ~(D2153_REGULATOR_MCTL0 | D2153_REGULATOR_MCTL2);
				reg_val |= (D2153_REGULATOR_MCTL0_ON | D2153_REGULATOR_MCTL2_ON);
				break;
		}
		ret = d2153_reg_write(d2153, reg_num, reg_val);

		if (ret < 0) {
			dlg_err("I2C write error\n");
			for(i=0; i<3 ; i++) {
				ret = d2153_reg_write(d2153, reg_num, reg_val);
				if(ret == 0) {
					break;
				}
			}
		}
		printk(KERN_ERR "[WS] regl_id[%d], reg_num[0x%x], reg_val[0x%x]\n", regulator_id, reg_num, reg_val);
	}

	if ((rdev_get_id(rdev) == 20) || (rdev_get_id(rdev) == 15)){
		printk(KERN_INFO "%s:<< rdev_get_id %d\n", 
				__func__, rdev_get_id(rdev));
	}

	return ret;
}

/*
 * d2153_regulator_disable
 */
static int d2153_regulator_disable(struct regulator_dev *rdev)
{
	struct d2153 *d2153 = rdev_get_drvdata(rdev);
	unsigned int regulator_id = rdev_get_id(rdev);
	unsigned int reg_num = 0;
	int ret = 0;
	u8 reg_val;
	int i;

	if ((rdev_get_id(rdev) == 20) || (rdev_get_id(rdev) == 15)){
		printk(KERN_INFO "%s:>> rdev_get_id %d\n", 
				__func__, rdev_get_id(rdev));
	}

	if (regulator_id >= D2153_NUMBER_OF_REGULATORS)
		return -EINVAL;

#if 1	// 20130720 add
	if((regulator_id == D2153_BUCK_5) || (regulator_id == D2153_BUCK_6)) {
		dlg_info("	 => Not used, regulator_id[%d]\n", regulator_id);
		return 0;
	}
#endif

	if (!is_mode_control_enabled()) {
		reg_num = regulator_register_map[regulator_id].en_reg;

		d2153_reg_read(d2153, reg_num, &reg_val);
		reg_val &= ~regulator_register_map[regulator_id].en_mask;
		d2153_reg_write(d2153, reg_num, reg_val);
	} else {
		reg_num = regulator_register_map[regulator_id].mctl_reg;

		ret = d2153_reg_write(d2153, reg_num, 0x00);
		if (ret < 0) {
			dlg_err("I2C write error\n");
			for(i=0; i<3 ; i++) {
				ret = d2153_reg_write(d2153, reg_num, 0x00);	
				if(ret ==0) {
					break;
				}
			}
		}
	}

	if ((rdev_get_id(rdev) == 20) || (rdev_get_id(rdev) == 15)){
		printk(KERN_INFO "%s:<< rdev_get_id %d\n", 
				__func__, rdev_get_id(rdev));
	}

	return ret;
}

/*
 * d2153_regulator_get_mode
 */
static unsigned int d2153_regulator_get_mode(struct regulator_dev *rdev)
{
	struct d2153 *d2153 = rdev_get_drvdata(rdev);
	unsigned int regulator_id = rdev_get_id(rdev);
	unsigned int mode = 0;

	if (regulator_id >= D2153_NUMBER_OF_REGULATORS)
		return -EINVAL;

	mode = get_regulator_mctl_mode(d2153, regulator_id);

	/* Map d2153 regulator mode to Linux framework mode */
	switch(mode) {
		case REGULATOR_MCTL_TURBO:
			mode = REGULATOR_MODE_FAST;
			break;
		case REGULATOR_MCTL_ON:
			mode = REGULATOR_MODE_NORMAL;
			break;
		case REGULATOR_MCTL_SLEEP:
			mode = REGULATOR_MODE_IDLE;
			break;
		case REGULATOR_MCTL_OFF:
			mode = REGULATOR_MODE_STANDBY;
			break;
		default:
			/* unsupported or unknown mode */
			break;
	}

	dlg_info("[REGULATOR] : [%s] >> MODE(%d)\n", __func__, mode);

	return mode;
}

/*
 * d2153_regulator_set_mode
 */
static int d2153_regulator_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct d2153 *d2153 = rdev_get_drvdata(rdev);
	unsigned int regulator_id = rdev_get_id(rdev);
	int ret;
	u8 mctl_mode;

	dlg_info("[REGULATOR] : regulator_set_mode. mode is %d\n", mode);

	switch(mode) {
		case REGULATOR_MODE_FAST :
			mctl_mode = REGULATOR_MCTL_TURBO;
			break;
		case REGULATOR_MODE_NORMAL :
			mctl_mode = REGULATOR_MCTL_ON;
			break;
		case REGULATOR_MODE_IDLE :
			mctl_mode = REGULATOR_MCTL_SLEEP;
			break;
		case REGULATOR_MODE_STANDBY:
			mctl_mode = REGULATOR_MCTL_OFF;
			break;
		default:
			return -EINVAL;
	}

	ret = set_regulator_mctl_mode(d2153, regulator_id, mode);

	return ret;
}

/*
 * d2153_regulator_list_voltage
 */
static int d2153_regulator_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	unsigned int regulator_id = rdev_get_id(rdev);

	if (regulator_id >= D2153_NUMBER_OF_REGULATORS)
		return -EINVAL;

	if (d2153_regulator_info[regulator_id].uVolt_step == 0) {
		return d2153_regulator_info[regulator_id].min_uVolts;
	} else {
		u32 selVolt = d2153_regulator_info[regulator_id].min_uVolts
			+ selector * d2153_regulator_info[regulator_id].uVolt_step;

		if (selVolt > d2153_regulator_info[regulator_id].max_uVolts)
			return -EINVAL;
		else
			return selVolt;
	}
}

/*
 * d2153_regulator_is_enabled
 */
static int d2153_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct d2153 *d2153 = rdev_get_drvdata(rdev);
	unsigned int reg_num, regulator_id = rdev_get_id(rdev);
	int ret = -EINVAL;
	u8 reg_val = 0;

	if (regulator_id >= D2153_NUMBER_OF_REGULATORS)
		return -EINVAL;

	if (!is_mode_control_enabled()) {

		reg_num = regulator_register_map[regulator_id].en_reg;

		ret = d2153_reg_read(d2153, reg_num, &reg_val);
		if (ret < 0) {
			dlg_err("I2C read error.\n");
			return ret;
		}

		if (reg_val & regulator_register_map[regulator_id].en_mask)
			return 1;
		else
			return 0;
	} else {
		reg_num = regulator_register_map[regulator_id].mctl_reg;
		ret = d2153_reg_read(d2153, reg_num, &reg_val);
		if (ret < 0) {
			dlg_err("I2C read error.\n");
			return ret;
		}

		/* 0x0 : Off    * 0x1 : On    * 0x2 : Sleep    * 0x3 : n/a */
		ret = ((reg_val & (D2153_REGULATOR_MCTL1|D2153_REGULATOR_MCTL3)) >= 1) ? 1 : 0;
		return ret;
	}

}

static struct regulator_ops d2153_ldo_ops = {
	.set_voltage = d2153_regulator_set_voltage,
	.get_voltage = d2153_regulator_get_voltage,
	.enable = d2153_regulator_enable,
	.disable = d2153_regulator_disable,
	.list_voltage = d2153_regulator_list_voltage,
	.is_enabled = d2153_regulator_is_enabled,
	.get_mode = d2153_regulator_get_mode,
	.set_mode = d2153_regulator_set_mode,
};

static struct regulator_desc d2153_reg[D2153_NUMBER_OF_REGULATORS] = {
	{
		.name = "D2153_BUCK_1",
		.id = D2153_BUCK_1,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_BUCK_2",
		.id = D2153_BUCK_2,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_BUCK_3",
		.id = D2153_BUCK_3,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_BUCK_4",
		.id = D2153_BUCK_4,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_BUCK_5",
		.id = D2153_BUCK_5,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_BUCK_6",
		.id = D2153_BUCK_6,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_1",
		.id = D2153_LDO_1,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_2",
		.id = D2153_LDO_2,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_3",
		.id = D2153_LDO_3,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_4",
		.id = D2153_LDO_4,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_5",
		.id = D2153_LDO_5,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_6",
		.id = D2153_LDO_6,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_7",
		.id = D2153_LDO_7,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_8",
		.id = D2153_LDO_8,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_9",
		.id = D2153_LDO_9,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_10",
		.id = D2153_LDO_10,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_11",
		.id = D2153_LDO_11,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_12",
		.id = D2153_LDO_12,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_13",
		.id = D2153_LDO_13,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_14",
		.id = D2153_LDO_14,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_15",
		.id = D2153_LDO_15,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_16",
		.id = D2153_LDO_16,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_17",
		.id = D2153_LDO_17,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_18",
		.id = D2153_LDO_18,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_19",
		.id = D2153_LDO_19,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_20",
		.id = D2153_LDO_20,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_AUD1",
		.id = D2153_LDO_AUD1,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "D2153_LDO_AUD2",
		.id = D2153_LDO_AUD2,
		.ops = &d2153_ldo_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	}
};


/*
 * d2153_regulator_is_enabled
 */
static int d2153_regulator_probe(struct platform_device *pdev)
{
	struct regulator_dev *rdev;
	u32 min_uVolts,uVolt_step,max_uVolts;

	dev_info(&pdev->dev, "Starting Regulator\n");

	if (pdev->id < D2153_BUCK_1 || pdev->id >= D2153_NUMBER_OF_REGULATORS)
		return -ENODEV;

	min_uVolts = d2153_regulator_info[pdev->id].min_uVolts;
	uVolt_step = d2153_regulator_info[pdev->id].uVolt_step;
	max_uVolts = d2153_regulator_info[pdev->id].max_uVolts;

	if (0 == uVolt_step) {
		d2153_reg[pdev->id].n_voltages = 1;
	} else {
		u32 uVolt_value = min_uVolts;
		int num_voltages = 1;

		while(uVolt_value < max_uVolts) {
			num_voltages += 1;
			uVolt_value += uVolt_step;
		}
		d2153_reg[pdev->id].n_voltages = num_voltages;
	}

	/* register regulator */
	rdev = regulator_register(&d2153_reg[pdev->id], &pdev->dev,
				pdev->dev.platform_data,
				dev_get_drvdata(&pdev->dev),
				NULL);

	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "failed to register %s\n",
			d2153_reg[pdev->id].name);
		return PTR_ERR(rdev);
	}

	d2153_rdev[pdev->id] = rdev;		/* rdev required for IOCTL support */

	dev_info(&pdev->dev, "Regulator started.\n");

	return 0;
}

/*
 * d2153_regulator_remove
 */
static int d2153_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);
	struct d2153 *d2153 = rdev_get_drvdata(rdev);
	int i;

	for (i = 0; i < ARRAY_SIZE(d2153->pmic.pdev); i++)
		platform_device_unregister(d2153->pmic.pdev[i]);

	regulator_unregister(rdev);

	return 0;
}

/*
 * d2153_register_regulator
 */
static int d2153_register_regulator(struct d2153 *d2153, int reg,
					struct regulator_init_data *initdata)
{
	struct platform_device *pdev;
	int ret;
	if (reg < D2153_BUCK_1 || reg >= D2153_NUMBER_OF_REGULATORS)
		return -EINVAL;

	if (d2153->pmic.pdev[reg])
		return -EBUSY;

	pdev = platform_device_alloc(DRIVER_NAME, reg);
	if (!pdev)
		return -ENOMEM;

	d2153->pmic.pdev[reg] = pdev;

	initdata->driver_data = d2153;

	pdev->dev.platform_data = initdata;
	pdev->dev.parent = d2153->dev;
	platform_set_drvdata(pdev, d2153);

	ret = platform_device_add(pdev);

	if (ret != 0) {
		dev_err(d2153->dev, "Failed to register regulator %d: %d\n", reg, ret);
		platform_device_del(pdev);
		d2153->pmic.pdev[reg] = NULL;
	}
	return ret;
}


static struct platform_driver d2153_regulator_driver = {
	.probe  = d2153_regulator_probe,
	.remove = d2153_regulator_remove,
	.driver = {
		.name = DRIVER_NAME,
	},
};

static int __init d2153_regulator_init(void)
{
	return platform_driver_register(&d2153_regulator_driver);
}
subsys_initcall(d2153_regulator_init);

static void __exit d2153_regulator_exit(void)
{
	platform_driver_unregister(&d2153_regulator_driver);
}
module_exit(d2153_regulator_exit);

/* Module information */
MODULE_AUTHOR("Dialog Semiconductor Ltd < william.seo@diasemi.com >");
MODULE_DESCRIPTION("D2153 voltage and current regulator driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRIVER_NAME);
