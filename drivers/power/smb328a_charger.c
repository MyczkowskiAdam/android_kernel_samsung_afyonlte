/*
*  SMB328A-charger.c
*  SMB328A charger interface driver
*
*  Copyright (C) 2012 Samsung Electronics
*
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/regulator/machine.h>
#include <linux/bq27425.h>
#include <linux/tsu6712.h>
#include <linux/pmic/pmic.h>
#include <linux/spa_power.h>
#include <linux/spa_agent.h>
#include <linux/wakelock.h>
#ifdef CONFIG_BATTERY_D2153
#include <linux/d2153/d2153_battery.h>
#ifdef CONFIG_D2153_EOC_CTRL
#define NO_USE_TERMINATION_CURRENT

#endif
#endif

/* Register define */
#define SMB328A_INPUT_AND_CHARGE_CURRENTS	0x00
#define SMB328A_CURRENT_TERMINATION			0x01
#define SMB328A_FLOAT_VOLTAGE				0x02
#define SMB328A_FUNCTION_CONTROL_A1			0x03
#define SMB328A_FUNCTION_CONTROL_A2			0x04
#define SMB328A_FUNCTION_CONTROL_B			0x05
#define SMB328A_OTG_PWR_AND_LDO_CONTROL	0x06
#define SMB328A_VARIOUS_CONTROL_FUNCTION_A	0x07
#define SMB328A_CELL_TEMPERATURE_MONITOR	0x08
#define SMB328A_INTERRUPT_SIGNAL_SELECTION	0x09
#define SMB328A_I2C_BUS_SLAVE_ADDRESS		0x0A

#define SMB328A_CLEAR_IRQ					0x30
#define SMB328A_COMMAND						0x31
#define SMB328A_INTERRUPT_STATUS_A			0x32
#define SMB328A_BATTERY_CHARGING_STATUS_A	0x33
#define SMB328A_INTERRUPT_STATUS_B			0x34
#define SMB328A_BATTERY_CHARGING_STATUS_B	0x35
#define SMB328A_BATTERY_CHARGING_STATUS_C	0x36
#define SMB328A_INTERRUPT_STATUS_C			0x37
#define SMB328A_BATTERY_CHARGING_STATUS_D	0x38
#define SMB328A_AUTOMATIC_INPUT_CURRENT_LIMMIT_STATUS	0x39

#define STATUS_A_CURRENT_TERMINATION	(0x01 << 3)
#define STATUS_A_TAPER_CHARGING			(0x01 << 2)
#define STATUS_A_INPUT_VALID 			(0x01 << 1)
#define STATUS_A_AICL_COMPLETE 			(0x01 << 0)

#define STATUS_C_TERMINATED_ONE_CYCLED	(0x01 << 7)
#define STATUS_C_TERMINATED_LOW_CURRENT	(0x01 << 6)
#define STATUS_C_SAFETY_TIMER_STATUS	(0x03 << 4)	// 4,5 two bit. 
#define STATUS_C_CHARGER_ERROR			(0x01 << 3)


enum {
	BAT_NOT_DETECTED,
	BAT_DETECTED
};

enum {
	CHG_MODE_NONE,
	CHG_MODE_AC,
	CHG_MODE_USB
};

struct smb328a_chip {
	struct i2c_client		*client;
	struct wake_lock    i2c_lock;
	struct mutex    i2c_mutex_lock;
	struct work_struct      work;
	struct smb328a_platform_data	*pdata;
	int chg_mode;
	int charger_status;
};

static struct smb328a_chip *smb_charger = NULL;

struct i2c_client *smb328a_chip_client(void)
{
	return smb_charger->client;
}

int smb328a_chip_status(void)
{
	return smb_charger->charger_status;
}

static bool FullChargeSend;

#ifdef CONFIG_PMIC_INTERFACE
extern int pmic_get_temp_status(void);
extern int pmic_read_battery_status(int property);
#endif

#ifdef CONFIG_BATTERY_D2153
extern void d2153_battery_start(void);
extern int d2153_battery_read_status(int type);
extern int d2153_battery_set_status(int type, int status);
#endif

static int smb328a_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;
	wake_lock(&smb_charger->i2c_lock);
	mutex_lock(&smb_charger->i2c_mutex_lock);
	ret = i2c_smbus_write_byte_data(client, reg, value);
	mutex_unlock(&smb_charger->i2c_mutex_lock);
	wake_unlock(&smb_charger->i2c_lock);
	pr_info("%s : REG(0x%x) = 0x%x\n", __func__, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static int smb328a_read_reg(struct i2c_client *client, int reg)
{
	int ret;
	wake_lock(&smb_charger->i2c_lock);
	mutex_lock(&smb_charger->i2c_mutex_lock);
	ret = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&smb_charger->i2c_mutex_lock);
	wake_unlock(&smb_charger->i2c_lock);
	pr_info("%s : REG(0x%x) = 0x%x\n", __func__, reg, ret);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static void smb328a_allow_volatile_writes(struct i2c_client *client)
{
	int val;
	u8 data;

	pr_info("%s\n", __func__);
	val = smb328a_read_reg(client, SMB328A_COMMAND);
	if ((val >= 0) && !(val&0x80)) {
		data = (u8)val;
		data |= (0x1 << 7) | (0x1<<3) |(0x1<<2);
		if (smb328a_write_reg(client, SMB328A_COMMAND, data) < 0)
			pr_err("%s : error!\n", __func__);
	}
}

static void smb328a_set_command_reg(struct i2c_client *client)
{
	int val;
	u8 data;

	pr_info("%s\n", __func__);
	val = smb328a_read_reg(client, SMB328A_COMMAND);
	if (val >= 0) {
		//data = (u8)val;
		data = 0xad;
		if (smb328a_write_reg(client, SMB328A_COMMAND, data) < 0)
			pr_err("%s : error!\n", __func__);
	}
}

static void smb328a_charger_function_conrol(struct i2c_client *client, int chg_current)
{
	int val;
	u8 data;

	pr_info("%s\n", __func__);

	smb328a_allow_volatile_writes(client);

	val = smb328a_read_reg(client, SMB328A_INPUT_AND_CHARGE_CURRENTS);
	if (val >= 0) {
		data = (u8)val;
		data &= 0x1F;
		data |= (((chg_current / 100) - 5) << 5);
		if (smb328a_write_reg(client, SMB328A_INPUT_AND_CHARGE_CURRENTS, data) < 0)
			pr_err("%s : error!\n", __func__);
	}

	val = smb328a_read_reg(client, SMB328A_CURRENT_TERMINATION);
	if (val >= 0) {
//		if(chg_current > 500)
			data = 0xb0;	//input currnet limmit value 1000mA
//		else
//			data = 0x10;	//input currnet limmit value 450mA for SMB327 sillicon bug.

		if (smb328a_write_reg(client, SMB328A_CURRENT_TERMINATION, data) < 0)
			pr_err("%s : error!\n", __func__);
	}

	val = smb328a_read_reg(client, SMB328A_FLOAT_VOLTAGE);
	if (val >= 0) {
		data = (u8)val;
		if (data != 0xda) {
			data = 0xda; /* 4.35V float voltage . only for smb327 support*/
			if (smb328a_write_reg(client, SMB328A_FLOAT_VOLTAGE, data) < 0)
				pr_err("%s : error!\n", __func__);
		}
	}

	val = smb328a_read_reg(client, SMB328A_FUNCTION_CONTROL_A1);
	if (val >= 0) {
		data = (u8)val;
		if (data != 0xc2) {
			data = 0xc2; // changed pre-charge to Fast < charge Voltage Threshold 2.6V->2.2V
			if (smb328a_write_reg(client, SMB328A_FUNCTION_CONTROL_A1, data) < 0)
				pr_err("%s : error!\n", __func__);
		}
	}

	val = smb328a_read_reg(client, SMB328A_FUNCTION_CONTROL_A2);
	if (val >= 0) {
		data = (u8)val;
		if (data != 0x4D) {
			data = 0x4D;
			if (smb328a_write_reg(client, SMB328A_FUNCTION_CONTROL_A2, data) < 0)
				pr_err("%s : error!\n", __func__);
		}
	}

	val = smb328a_read_reg(client, SMB328A_FUNCTION_CONTROL_B);
	if (val >= 0) {
		data = (u8)val;
		if (data != 0x0) {
			data = 0x0;
			if (smb328a_write_reg(client, SMB328A_FUNCTION_CONTROL_B, data) < 0)
				pr_err("%s : error!\n", __func__);
		}
	}

	val = smb328a_read_reg(client, SMB328A_OTG_PWR_AND_LDO_CONTROL);
	if (val >= 0) {
		data = (u8)val;
		if (data != 0xc5) {
			data = 0xc5;
			if (smb328a_write_reg(client, SMB328A_OTG_PWR_AND_LDO_CONTROL, data) < 0)
				pr_err("%s : error!\n", __func__);
		}
	}

	val = smb328a_read_reg(client, SMB328A_VARIOUS_CONTROL_FUNCTION_A);
	if (val >= 0) {
		data = (u8)val;
		if (data != 0x96) { /* this can be changed with top-off setting */
			data = 0x96;
			if (smb328a_write_reg(client, SMB328A_VARIOUS_CONTROL_FUNCTION_A, data) < 0)
				pr_err("%s : error!\n", __func__);
		}
	}

	val = smb328a_read_reg(client, SMB328A_CELL_TEMPERATURE_MONITOR);
	if (val >= 0) {
		data = (u8)val;
		if (data != 0x0) {
			data = 0x0;
			if (smb328a_write_reg(client, SMB328A_CELL_TEMPERATURE_MONITOR, data) < 0)
				pr_err("%s : error!\n", __func__);
		}
	}

	val = smb328a_read_reg(client, SMB328A_INTERRUPT_SIGNAL_SELECTION);
	if (val >= 0) {
		data = (u8)val;
#ifdef NO_USE_TERMINATION_CURRENT
		if (data != 0x01) {
			data = 0x01;
			if (smb328a_write_reg(client, SMB328A_INTERRUPT_SIGNAL_SELECTION, data) < 0)
				pr_err("%s : error!\n", __func__);
		}
#else
		if (data != 0x11) {
			data = 0x11;
			if (smb328a_write_reg(client, SMB328A_INTERRUPT_SIGNAL_SELECTION, data) < 0)
				pr_err("%s : error!\n", __func__);
		}
#endif
	}
#if 0
	val = smb328a_read_reg(client, SMB328A_I2C_BUS_SLAVE_ADDRESS);
	if (val >= 0) {
		data = (u8)val;
		if (data != 0x69) {
			data = 0x69;
			if (smb328a_write_reg(client, SMB328A_I2C_BUS_SLAVE_ADDRESS, data) < 0)
				pr_err("%s : error!\n", __func__);
		}
	}
#endif
}

#if 0
/**
 * not used function.
 */
static bool smb328a_check_bat_full(struct i2c_client *client)
{
	int val;
	u8 data = 0;
	bool ret = false;

	pr_info("%s\n", __func__);

	val = smb328a_read_reg(client, SMB328A_BATTERY_CHARGING_STATUS_C);
	if (val >= 0) {
		data = (u8)val;
		if (data&(0x1<<6))
			ret = true; /* full */
	}

	return ret;
}
#endif

/* vf check */
static bool smb328a_check_bat_missing(struct i2c_client *client)
{
	int val;
	u8 data = 0;
	bool ret = false;

	pr_info("%s\n", __func__);

	val = smb328a_read_reg(client, SMB328A_BATTERY_CHARGING_STATUS_B);
	if (val >= 0) {
		data = (u8)val;
		if (data&0x1) {
			ret = true; /* missing battery */
		}
	}

	return ret;
}

#if 0
/**
 * not used function.
 */
/* whether valid dcin or not */
static bool smb328a_check_vdcin(struct i2c_client *client)
{
	int val;
	u8 data = 0;
	bool ret = false;

	pr_info("%s\n", __func__);

	val = smb328a_read_reg(client, SMB328A_BATTERY_CHARGING_STATUS_A);
	if (val >= 0) {
		data = (u8)val;
		if (data&(0x1<<1))
			ret = true;
	}

	return ret;
}
#endif

#if 0
/**
 * not used function.
 */
static bool smb328a_check_bmd_disabled(struct i2c_client *client)
{
	int val;
	u8 data = 0;
	bool ret = false;

	pr_info("%s\n", __func__);

	val = smb328a_read_reg(client, SMB328A_FUNCTION_CONTROL_B);
	if (val >= 0) {
		data = (u8)val;
		if (data&(0x1<<7)) {
			ret = true;
		}
	}

	val = smb328a_read_reg(client, SMB328A_OTG_PWR_AND_LDO_CONTROL);
	if (val >= 0) {
		data = (u8)val;
		if ((data&(0x1<<7))==0) {
			ret = true;
		}
	}

	return ret;
}
#endif

static int smb328a_set_top_off(struct i2c_client *client, int set_val)
{
	int val;
	u8 data;

	pr_info("%s\n", __func__);

	smb328a_allow_volatile_writes(client);

	val = smb328a_read_reg(client, SMB328A_INPUT_AND_CHARGE_CURRENTS);
	if (val >= 0) {
		data = (u8)val;
		data &= 0xF8;
		data |= ((100 / 25) - 1);		// don't use termination current
		if (smb328a_write_reg(client, SMB328A_INPUT_AND_CHARGE_CURRENTS, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -1;
		}
	}

	val = smb328a_read_reg(client, SMB328A_VARIOUS_CONTROL_FUNCTION_A);
	if (val >= 0) {
		data = (u8)val;
		data &= 0x1F;
		data |= (((set_val / 25) - 1) << 5);
		if (smb328a_write_reg(client, SMB328A_VARIOUS_CONTROL_FUNCTION_A, data) < 0) {
			pr_err("%s : error!\n", __func__);

			return -1;
		}
	}

	return 0;
}

static int smb328a_set_charging_current(struct i2c_client *client, int chg_current)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
    int cable_type = get_cable_type();

	pr_info("%s\n", __func__);

	if (cable_type == CABLE_TYPE_USB)
		chip->chg_mode = CHG_MODE_USB;
	else if(cable_type == CABLE_TYPE_AC)
		chip->chg_mode = CHG_MODE_AC;
	else
	    chip->chg_mode = CHG_MODE_NONE;

	return 0;
}

static int smb328a_enable_otg(struct i2c_client *client)
{
	int val;
	u8 data;

	pr_info("%s\n", __func__);

	val = smb328a_read_reg(client, SMB328A_COMMAND);
	if (val >= 0) {
		//data = (u8)val;
		data = 0x80;
		if (smb328a_write_reg(client, SMB328A_COMMAND, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -1;
		}
		msleep(100);
	}

	val = smb328a_read_reg(client, SMB328A_FUNCTION_CONTROL_B);
	if (val >= 0) {
		//data = (u8)val;
		data = 0x0C;
		if (smb328a_write_reg(client, SMB328A_FUNCTION_CONTROL_B, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -1;
		}
		msleep(100);
	}

	val = smb328a_read_reg(client, SMB328A_COMMAND);
	if (val >= 0) {
		data = (u8)val;
		if (data != 0x9a)
		{
			data = 0x9a;
			if (smb328a_write_reg(client, SMB328A_COMMAND, data) < 0) {
				pr_err("%s : error!\n", __func__);
				return -1;
			}
			msleep(100);
		}
	}

	return 0;
}

static int smb328a_disable_otg(struct i2c_client *client)
{
	int val;
	u8 data;

	pr_info("%s\n", __func__);

	//	fsa9480_otg_detach();

	val = smb328a_read_reg(client, SMB328A_FUNCTION_CONTROL_B);
	if (val >= 0) {
		data = (u8)val;
		data = 0x0C;
		if (smb328a_write_reg(client, SMB328A_FUNCTION_CONTROL_B, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -1;
		}
		msleep(100);
	}

	val = smb328a_read_reg(client, SMB328A_COMMAND);
	if (val >= 0) {
		//data = (u8)val;
		data = 0x98;
		if (smb328a_write_reg(client, SMB328A_COMMAND, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -1;
		}
		msleep(100);
	}

	return 0;
}

void smb328a_otg_enable_disable(int onoff, int cable)
{
	struct i2c_client *client = smb_charger->client;

	pr_info("%s\n", __func__);

	if (onoff)
	{
		// smb328a_charger_function_conrol(client, cable);  kbj temp
		smb328a_enable_otg(client);
	}
	else
		smb328a_disable_otg(client);
}
EXPORT_SYMBOL(smb328a_otg_enable_disable);


int smb328a_check_charging_status(void)
{
	int val;
	u8 data = 0;
	bool ret = false;
	struct i2c_client *client = smb_charger->client;

	pr_info("%s\n", __func__);

	val = smb328a_read_reg(client, SMB328A_BATTERY_CHARGING_STATUS_C);
	if (val >= 0) {
		data = (u8)val;
		if (data & 0x3)
			ret = 1; /* Charging */
		else
			ret = 0; /* No charging */
	}

	return ret;
}
EXPORT_SYMBOL(smb328a_check_charging_status);

#if 0
static void smb328a_ldo_disable(struct i2c_client *client)
{
	int val;
	u8 data;

	pr_info("%s\n", __func__);

	smb328a_allow_volatile_writes(client);

	val = smb328a_read_reg(client, SMB328A_OTG_PWR_AND_LDO_CONTROL);
	if (val >= 0) {
		data = (u8)val;
		data |= (0x1 << 5);
		if (smb328a_write_reg(client, SMB328A_OTG_PWR_AND_LDO_CONTROL, data) < 0)
			pr_err("%s : error!\n", __func__);
	}
}
#endif
int smb328a_enable_charging(struct i2c_client *client)
{
	int val;
	u8 data;
	struct smb328a_chip *chip = i2c_get_clientdata(client);

	pr_info("%s\n", __func__);

	val = smb328a_read_reg(client, SMB328A_COMMAND);
	if (val >= 0) {
		data = (u8)val;
		if (chip->chg_mode == CHG_MODE_AC)
			data = 0x8C;
		else if (chip->chg_mode == CHG_MODE_USB)
			data = 0x88;
		else
			data = 0x98;

		if (smb328a_write_reg(client, SMB328A_COMMAND, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -1;
		}
	}

	return 0;
}

int smb328a_disable_charging(struct i2c_client *client)
{
	int val;
	u8 data;

	pr_info("%s\n", __func__);

	val = smb328a_read_reg(client, SMB328A_COMMAND);
	if (val >= 0) {
		//data = (u8)val;
		data = 0x98;
		if (smb328a_write_reg(client, SMB328A_COMMAND, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -1;
		}
	}

	return 0;
}

static int smb328a_get_charger_type (void)
{
	int type = get_cable_type();
	pr_info("%s, %d\n", __func__, type);
	switch(type)
	{
	case CABLE_TYPE_USB:
		return POWER_SUPPLY_TYPE_USB;
	case CABLE_TYPE_AC:
		return POWER_SUPPLY_TYPE_USB_DCP;
	default :
		return POWER_SUPPLY_TYPE_BATTERY;
	}
}

static int smb328a_set_charge(unsigned int en)
{
	int ret = 0;
	if (en) {
//		if (smb_charger->chg_mode == CHG_MODE_AC)
//			smb328a_ldo_disable(smb_charger->client);
	    //smb328a_disable_charging(smb_charger->client);
		ret = smb328a_enable_charging(smb_charger->client);
	} else {
		ret = smb328a_disable_charging(smb_charger->client);
	}

#ifdef CONFIG_BATTERY_D2153
	d2153_battery_set_status(D2153_STATUS_CHARGING, en);
#endif
	return ret;
}

static int smb328a_set_charge_current (unsigned int curr)
{
	int ret = 0;
	int validval = curr;

	pr_info("%s : current =%d\n", __func__, curr);

	if (curr < 500 || curr > 1200) {
		validval = 500; //min current
	}
	smb328a_set_command_reg(smb_charger->client);
	smb328a_charger_function_conrol(smb_charger->client, validval);
	ret = smb328a_set_charging_current(smb_charger->client, validval);

	return ret;
}

static int smb328a_set_full_charge (unsigned int eoc)
{
	int ret = 0;
	int validval = eoc;

	pr_info("%s : eoc =%d\n", __func__, eoc);

	if (eoc < 25 || eoc > 200) {
		validval = 200; //max top-off
	}
#ifdef NO_USE_TERMINATION_CURRENT
	validval = 25;	//don't use charger eoc.
#endif	
	ret = smb328a_set_top_off(smb_charger->client, validval);

	return ret;
}

static int smb328a_get_capacity (void)
{
	unsigned int bat_per;

#ifdef CONFIG_BATTERY_D2153
	bat_per = d2153_battery_read_status(D2153_BATTERY_SOC);
#else

#ifdef CONFIG_BATTERY_BQ27425
	get_bq27425_battery_data(BQ27425_REG_SOC, &bat_per);
#else
	bat_per = 50;
#endif
#endif
	return bat_per;
}

static int smb328a_get_temp (unsigned int opt)
{
	int temp;

#ifdef CONFIG_BATTERY_D2153
	temp = d2153_battery_read_status(D2153_BATTERY_TEMP_ADC);
#else

#ifdef CONFIG_PMIC_INTERFACE
	temp = pmic_get_temp_status();
#else
	temp = 30;
#endif
#endif
	return temp;
}

static int smb328a_get_voltage (unsigned char opt)
{
#ifdef CONFIG_BATTERY_D2153
	int volt;
	volt = d2153_battery_read_status(D2153_BATTERY_AVG_VOLTAGE);
#else
	int volt=3800;
#endif
	return volt;
}

static int smb328a_get_batt_presence (unsigned int opt)
{
	if (smb328a_check_bat_missing(smb_charger->client))
		return BAT_NOT_DETECTED;
	else
		return BAT_DETECTED;
}

static int smb328a_ctrl_fg (void *data)
{
	int ret = 0;

#ifdef CONFIG_BATTERY_D2153
	d2153_battery_set_status(D2153_RESET_SW_FG, 0);
#endif
	
	return ret;
}

#ifdef CONFIG_USE_MUIC
extern void muic_set_vbus(int vbus);
#endif

static void smb328a_work_func(struct work_struct *work)
{
    struct smb328a_chip *p = container_of(work, struct smb328a_chip, work);
	int val;
	int i;
#ifdef CONFIG_USE_MUIC
	static bool pre_vbus = false;
	bool vbus;
#endif
    pr_info("%s\n", __func__);

    if(!p)
    {
        pr_err("%s: smb328a_chip is NULL\n", __func__);
		return ;
    }
	msleep(110);
	val = smb328a_read_reg(p->client, SMB328A_BATTERY_CHARGING_STATUS_A);
#ifdef CONFIG_USE_MUIC
	vbus = (bool)((u8)val & STATUS_A_INPUT_VALID);
	if(pre_vbus != vbus)
	{
		pre_vbus = vbus;
		muic_set_vbus(vbus == true ? 1 : 0);
		FullChargeSend = 0;
	}
#endif
#ifndef NO_USE_TERMINATION_CURRENT
	if(val & (STATUS_A_CURRENT_TERMINATION|STATUS_A_TAPER_CHARGING))
	{
		if(FullChargeSend==0 || (val & STATUS_A_CURRENT_TERMINATION)) {
	        pr_info("%s: EOC\n", __func__);
	        if(spa_event_handler(SPA_EVT_EOC, 0) < 0)
				pr_info("%s: EOC is not ready\n", __func__);
			else
				FullChargeSend = 1;
		}
    }
#endif

	val = smb328a_read_reg(p->client, SMB328A_BATTERY_CHARGING_STATUS_C);

	if((val & (STATUS_C_TERMINATED_ONE_CYCLED|STATUS_C_TERMINATED_LOW_CURRENT
		| STATUS_C_CHARGER_ERROR | STATUS_C_SAFETY_TIMER_STATUS))		// 01 --> pre charger timer // 10 --> complete charger
		&& ((val&STATUS_C_SAFETY_TIMER_STATUS) != STATUS_C_SAFETY_TIMER_STATUS)) {	// 11 --> waiting charger
		smb328a_disable_charging(p->client);
		if(smb_charger->chg_mode == CHG_MODE_AC)
			smb328a_charger_function_conrol(p->client, 1200);
		else
			smb328a_charger_function_conrol(p->client, 500);

		smb328a_enable_charging(p->client);
		pr_info("%s charger is unexpected error.enable again.\n", __func__);
	}


	for(i = 0; i < 0xb; i++){
	   val = smb328a_read_reg(p->client, i);
   }

	for(i = 0; i < 7; i++){
		val = smb328a_read_reg(p->client, SMB328A_INTERRUPT_STATUS_A + i);
	}

}

static irqreturn_t smb328a_irq_handler(int irq, void *data)
{
    struct smb328a_chip *p = (struct smb328a_chip *)data;

	pr_info("%s\n", __func__);
   schedule_work(&(p->work));
	smb328a_write_reg(p->client, SMB328A_CLEAR_IRQ, 1)  ;

	return IRQ_HANDLED;
}

static int smb328a_irq_init(struct i2c_client *client)
{
	int ret = 0;

	smb328a_irq_handler(client->irq, smb_charger);

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL, smb328a_irq_handler,
            (IRQF_TRIGGER_FALLING | IRQF_ONESHOT| IRQF_NO_SUSPEND), "smb328a_charger", smb_charger);

		if (ret) {
			pr_err("%s: failed to reqeust IRQ\n", __func__);
			return ret;
		}

		ret = enable_irq_wake(client->irq);
		if (ret < 0)
			dev_err(&client->dev,"failed to enable wakeup src %d\n", ret);
	}
    else
        pr_err("%s: SMB328A IRQ is NULL\n", __func__);

	return ret;
}

static int __devinit smb328a_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct smb328a_chip *chip;
	int val;

	pr_info("%s\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	smb_charger = chip;
	chip->client = client;
	i2c_set_clientdata(client, chip);

	mutex_init(&smb_charger->i2c_mutex_lock);
	wake_lock_init(&smb_charger->i2c_lock, WAKE_LOCK_SUSPEND, "smb328a_i2c");

   INIT_WORK(&(chip->work), smb328a_work_func);

	chip->chg_mode = CHG_MODE_NONE;

	spa_agent_register(SPA_AGENT_SET_CHARGE, (void*)smb328a_set_charge, "smb328a-charger");
	spa_agent_register(SPA_AGENT_SET_CHARGE_CURRENT, (void*)smb328a_set_charge_current, "smb328a-charger");
	spa_agent_register(SPA_AGENT_SET_FULL_CHARGE, (void*)smb328a_set_full_charge, "smb328a-charger");
	spa_agent_register(SPA_AGENT_GET_CAPACITY, (void*)smb328a_get_capacity, "smb328a-charger");
	spa_agent_register(SPA_AGENT_GET_TEMP, (void*)smb328a_get_temp, "smb328a-charger");
	spa_agent_register(SPA_AGENT_GET_VOLTAGE, (void*)smb328a_get_voltage, "smb328a-charger");
	spa_agent_register(SPA_AGENT_GET_BATT_PRESENCE, (void*)smb328a_get_batt_presence, "smb328a-charger");
	spa_agent_register(SPA_AGENT_GET_CHARGER_TYPE, (void*)smb328a_get_charger_type, "smb328a-charger");
	spa_agent_register(SPA_AGENT_CTRL_FG, (void*)smb328a_ctrl_fg, "smb328a-charger");

	val = smb328a_read_reg(client, SMB328A_BATTERY_CHARGING_STATUS_C);

	if(val & (STATUS_C_TERMINATED_ONE_CYCLED | STATUS_C_TERMINATED_LOW_CURRENT |
		STATUS_C_CHARGER_ERROR | STATUS_C_SAFETY_TIMER_STATUS)) {
		smb328a_disable_charging(client);
		smb328a_enable_charging(client);
		pr_info("%s charger is unexpected error.enable again.\n", __func__);
	}

	smb328a_charger_function_conrol(client, 500);

	smb328a_irq_init(client);

#ifdef CONFIG_BATTERY_D2153
	d2153_battery_start();
#endif

	return 0;
}

static int __devexit smb328a_remove(struct i2c_client *client)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	mutex_destroy(&smb_charger->i2c_mutex_lock);
	kfree(chip);
	return 0;
}

static int smb328a_suspend(struct i2c_client *client,
	pm_message_t state)
{
	return 0;
}

static int smb328a_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id smb328a_id[] = {
	{ "smb328a", 0 },
	{ }
};

static struct i2c_driver smb328a_i2c_driver = {
	.driver	= {
		.name	= "smb328a",
	},
	.probe		= smb328a_probe,
	.remove		= __devexit_p(smb328a_remove),
	.suspend	= smb328a_suspend,
	.resume		= smb328a_resume,
	.id_table	= smb328a_id,
};

static int __init smb328a_init(void)
{
	return i2c_add_driver(&smb328a_i2c_driver);
}

static void __exit smb328a_exit(void)
{
	i2c_del_driver(&smb328a_i2c_driver);
}

//module_init(smb328a_init);
subsys_initcall_sync(smb328a_init);
module_exit(smb328a_exit);

MODULE_DESCRIPTION("SMB328A charger control driver");
MODULE_AUTHOR("SAMSUNG");
MODULE_LICENSE("GPL");
