/*
 *  smb358_charger.c
 *  Samsung SMB358 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define DEBUG
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/input.h>
#include <linux/smb358_charger.h>
#include <linux/power_supply.h>
#include <linux/regulator/machine.h>
#include <linux/tsu6712.h>
#include <linux/pmic/pmic.h>
#include <linux/spa_power.h>
#include <linux/spa_agent.h>
#include <linux/wakelock.h>
#ifdef CONFIG_BATTERY_D2153
#include <linux/d2153/d2153_battery.h>
#endif
#ifdef CONFIG_STC3115_FUELGAUGE
#include <linux/stc3115_battery.h>
#endif

#ifdef CONFIG_BATTERY_D2153
extern void d2153_battery_start(void);
extern int d2153_battery_read_status(int type);
extern int d2153_battery_set_status(int type, int status);
#endif

enum {
	BAT_NOT_DETECTED,
	BAT_DETECTED
};

enum {
	CHG_MODE_NONE,
	CHG_MODE_AC,
	CHG_MODE_USB
};

#define TA_CHG_CURRENT	FAST_CHG_1800mA
#define MAX_INPUT_CURRENT	LIMIT_1800mA

struct i2c_client * global_client;

static struct sec_charger_info *smb_charger = NULL;


static int smb358_write_reg(struct i2c_client *client, u8 reg, u8 data)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msg[1];

	buf[0] = reg;
	buf[1] = data;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret != 1) {
		pr_err("\n [smb358] i2c Write Failed (ret=%d)\n", ret);
		return -1;
	}
	pr_info("%s:reg[0x%x]= 0x%x\n", __func__, reg, data);
	return ret;
}
static int smb358_read_reg(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret;
	u8 buf[1];
	struct i2c_msg msg[2];

	buf[0] = reg;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2) {
		pr_err("\n [smb358] i2c Read Failed (ret=%d)\n", ret);
		return -1;
	}
	*data = buf[0];

	pr_info("%s:reg[0x%x]= 0x%x\n", __func__, reg, *data);

	return 0;
}

void smb358_stop_chg(void)
{
	struct i2c_client * client;
	u8 data=0;
	pr_info("%s\n",__func__);
	client = global_client;
	if( !client) return;

	smb358_read_reg(client,SMB358_COMMAND_A,&data);


	data &= ~CHG_ENABLE;

	smb358_write_reg(client,SMB358_COMMAND_A,data);

	
	smb358_read_reg(client,SMB358_INPUT_CURRENTLIMIT,&data);

	data &= ~(0xf0);

	data |= LIMIT_500mA;

	smb358_write_reg(client, SMB358_INPUT_CURRENTLIMIT, data);

	smb358_read_reg(client,SMB358_CHARGE_CURRENT,&data);
	
	data &= FAST_CHG_MASK;
	data |= FAST_CHG_450mA;
	smb358_write_reg(client,SMB358_CHARGE_CURRENT,data);

}

void smb358_start_chg(void)
{
	struct i2c_client * client;
	u8 data=0;

	pr_info("%s\n",__func__);
	client = global_client;
	if( !client) return;

	smb358_read_reg(client,SMB358_COMMAND_A,&data);

	//data |= ALLOW_VOLATILE_WRITE | CHG_ENABLE;
	data |= CHG_ENABLE;

	smb358_write_reg(client,SMB358_COMMAND_A,data);

	smb358_read_reg(client,SMB358_COMMAND_B,&data);

	if (smb_charger->chg_mode == CHG_MODE_USB)
		data &= ~(0x01<<0);
	else
		data |= (0x01<<0);

	data |= USB_5_MODE;	//smb 358 chagne this bit automatically.(by hw schematic)

	smb358_write_reg(client,SMB358_COMMAND_B,data);

}
//EXPORT_SYMBOL(smb358_start_chg);

int smb358_set_chg_current(int chg_current)
{
	u8 ret = -1;
	struct i2c_client * client;
	u8 data=0,set_current=0;
	int cable_type = get_cable_type();
	client = global_client;

	if( !client) return ret;

	pr_info("%s selected chg_current=%d\n",__func__,chg_current);

	// to set system power
	if (cable_type == CABLE_TYPE_USB)
		smb_charger->chg_mode = CHG_MODE_USB;
	else if(cable_type == CABLE_TYPE_AC)
		smb_charger->chg_mode = CHG_MODE_AC;
	else
	    smb_charger->chg_mode = CHG_MODE_NONE;

	if (smb_charger->chg_mode == CHG_MODE_USB)
		data &= ~(0x01<<0);
	else
		data |= (0x01<<0);

	data |= USB_5_MODE;	//smb 358 chagne this bit automatically.(by hw schematic)

	smb358_write_reg(client,SMB358_COMMAND_B,data);

	smb358_read_reg(client,SMB358_INPUT_CURRENTLIMIT,&data);

	data &= ~(0xf0);

	if(cable_type == CABLE_TYPE_AC)
		data |= MAX_INPUT_CURRENT;
	else
		data |= LIMIT_500mA;

	smb358_write_reg(client, SMB358_INPUT_CURRENTLIMIT, data);

	smb358_read_reg(client,SMB358_CHARGE_CURRENT,&data);

	switch( chg_current)
	{
	case 100:
	case 200:	set_current = FAST_CHG_200mA;	break;
	case 450:	set_current = FAST_CHG_450mA;	break;
	case 1300:	set_current = FAST_CHG_1300mA;	break;
	case 1500:	set_current = FAST_CHG_1500mA;	break;
	case 1800:	set_current = FAST_CHG_1800mA;	break;
	case 2000:	set_current = FAST_CHG_2000mA;	break;
	default :	set_current = FAST_CHG_1500mA;	break;

	}

	data &= FAST_CHG_MASK;
	data |= set_current;
	ret = smb358_write_reg(client,SMB358_CHARGE_CURRENT,data);
	return ret;

}
//EXPORT_SYMBOL(smb358_set_chg_current);

int smb358_set_eoc(int eoc_value)
{
	u8 ret = -1;
	struct i2c_client * client;
	u8 data=0,set_eoc=0;
	client = global_client;

	if( !client) return ret;

	pr_info("%s eoc_value=%d\n",__func__,eoc_value);

	ret = smb358_read_reg(client,SMB358_CHARGE_CURRENT,&data);

	switch( eoc_value)
	{
	case 30:		set_eoc = EOC_30mA;		break;
	case 40:		set_eoc = EOC_40mA;		break;
	case 60:		set_eoc = EOC_60mA;		break;
	case 80:		set_eoc = EOC_80mA;		break;
	case 100:	set_eoc = EOC_100mA;		break;
	case 125:	set_eoc = EOC_125mA;		break;
	case 150:	set_eoc = EOC_150mA;		break;
	case 200:	set_eoc = EOC_200mA;		break;
	default :	set_eoc = EOC_40mA;		break;
	}

	data &= EOC_MASK;
	data |= set_eoc;

	ret = smb358_write_reg(client,SMB358_CHARGE_CURRENT,data);

	return ret;

}
EXPORT_SYMBOL(smb358_set_eoc);

bool smb358_chg_init(struct i2c_client *client)
{
	u8 data;
	pr_info("%s\n",__func__);

	/* command A is setted @ bootload */
	smb358_read_reg(client,SMB358_COMMAND_A,&data);
	data |= ALLOW_VOLATILE_WRITE|FAST_CHARGE;
	smb358_write_reg(client, SMB358_COMMAND_A, data);

	/* Command B : USB1 mode, USB mode */
	data = USB_5_MODE | HC_MODE;		//fast charge mode & usb charge mode is 500 mA
	smb358_write_reg(client, SMB358_COMMAND_B, data);

	/* Allow volatile writes to CONFIG registers */
	data = TA_CHG_CURRENT|PRE_CHG_450mA|EOC_200mA;
	smb358_write_reg(client, SMB358_CHARGE_CURRENT, data); //0

	data = MAX_INPUT_CURRENT|CHG_INHIBIT_THR_100mV;
	smb358_write_reg(client, SMB358_INPUT_CURRENTLIMIT, data); // 1

	data = 0xD7;
	smb358_write_reg(client, SMB358_VARIOUS_FUNCTIONS, data); // 2

	data = VOLTAGE_4_20V;
	smb358_write_reg(client, SMB358_FLOAT_VOLTAGE, data); // 3

	data = AUTO_RECHG_DISABLE | AUTO_EOC_DISABLE;
	smb358_write_reg(client, SMB358_CHARGE_CONTROL, data); // 4

	data = 0x0F;
	smb358_write_reg(client, SMB358_STAT_TIMERS_CONTROL, data);	// 5

	data = 0x09;
	smb358_write_reg(client, SMB358_PIN_ENABLE_CONTROL, data); // 6

	data = 0xF0;
	smb358_write_reg(client, SMB358_THERM_CONTROL_A, data); //7

	data = 0x09;
	smb358_write_reg(client, SMB358_SYSOK_USB30_SELECTION, data); //8

	data = 0x00;
	smb358_write_reg(client, SMB358_OTHER_CONTROL_A, data); // 9

	data = 0xF6;
	smb358_write_reg(client, SMB358_OTG_TLIM_THERM_CONTROL, data); // A

	data = 0xA5;
	smb358_write_reg(client, SMB358_LIMIT_CELL_TEMPERATURE_MONITOR, data);	// B

	data = 0x00;
	smb358_write_reg(client, SMB358_FAULT_INTERRUPT, data); // C

	data = 0x14;	//Fullcharging & INOK enable
	smb358_write_reg(client, SMB358_STATUS_INTERRUPT, data); // D

	return true;
}

static int smb358_get_batt_presence (unsigned int opt)
{
//	if (smb328a_check_bat_missing(smb_charger->client))
//		return BAT_NOT_DETECTED;
//	else
		return BAT_DETECTED;
}

extern int read_voltage(int *vbat);
static int smb358_get_voltage (unsigned char opt)
{
	int volt;
#ifdef CONFIG_STC3115_FUELGAUGE
	read_voltage(&volt);
#elif CONFIG_BATTERY_D2153
	volt = d2153_battery_read_status(D2153_BATTERY_AVG_VOLTAGE);
#else
	volt = 4000;
#endif
	return volt;
}

extern int read_soc(int *soc);
static int smb358_get_capacity (void)
{
	unsigned int bat_per;
#ifdef CONFIG_STC3115_FUELGAUGE
	read_soc(&bat_per);
#elif CONFIG_BATTERY_D2153
	bat_per = d2153_battery_read_status(D2153_BATTERY_SOC);
#else
	bat_per = 50;
#endif
	return bat_per;
}

void smb358_set_charge(unsigned int en)
{
	pr_info("%s : en =%d\n", __func__, en);

	if (en) {
		smb358_start_chg();
	} else {
		smb358_stop_chg();
	}

#ifdef CONFIG_BATTERY_D2153
	d2153_battery_set_status(D2153_STATUS_CHARGING, en);
#endif

}

static int smb358_set_charge_current (unsigned int curr)
{
	int ret = 0;

	pr_info("%s : current =%d\n", __func__, curr);

	//if(curr < 450)
	//	curr = 200;
	if(curr  < 600)
		curr = 450;
	else if(curr < 900)
		curr = 600;
	else if(curr < 1300)
		curr = 900;
	else if(curr < 1500)
		curr = 1300;
	else if(curr < 1800)
		curr = 1500;
	else if(curr < 2000)
		curr = 1800;
	else
		curr = 2000;

	ret = smb358_set_chg_current(curr);
	return ret;
}

static int smb358_set_full_charge (unsigned int eoc)
{
	int ret = 0;

	pr_info("%s : eoc =%d\n", __func__, eoc);

	if(eoc < 40)
		eoc = 30;
	else if(eoc < 60)
		eoc =40;
	else if(eoc < 80)
		eoc =60;
	else if(eoc < 100)
		eoc =80;
	else if(eoc < 125)
		eoc =100;
	else if(eoc < 150)
		eoc =125;
	else if(eoc < 200)
		eoc =150;
	else
		eoc = 200;

	ret = smb358_set_eoc(eoc);

	return ret;
}

static int smb358_get_temp (unsigned int opt)
{
#ifdef CONFIG_BATTERY_D2153
	int temp;
	temp = d2153_battery_read_status(D2153_BATTERY_TEMP_ADC);
#else
	int temp = 270;
#endif
	return temp;
}

static int smb358_get_charger_type (void)
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

static int smb358a_enable_otg(struct i2c_client *client)
{
	u8 val = 0;
	u8 data = 0;
	int ret = 0;

	pr_info("%s:\n", __func__);

	/* Allow Volatile writes */
	ret = smb358_read_reg(client, SMB358_COMMAND_A, &val);
	if (ret >= 0) {
		data = (u8)val;

		pr_info("%s : reg (0x%x) = 0x%x\n",
			__func__, SMB358_COMMAND_A, data);
		data |= ALLOW_VOLATILE_WRITE;
		if (smb358_write_reg(client, SMB358_COMMAND_A, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -1;
		}
		msleep(20);

		smb358_read_reg(client, SMB358_COMMAND_A, &data);
		pr_info("%s : => reg (0x%x) = 0x%x\n",
			__func__, SMB358_COMMAND_A, data);

	}

	/* Disable Charger */
	smb358_set_charge(0);

	/* Enable OTG Mode */
	ret = smb358_read_reg(client, SMB358_COMMAND_A, &val);
	if (ret >= 0) {
		data = (u8)val;
		pr_info("%s : reg (0x%x) = 0x%x\n",
			__func__, SMB358_COMMAND_A, data);
		data |= OTG_EN;
		if (smb358_write_reg(client, SMB358_COMMAND_A, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -1;
		}
		msleep(20);

		smb358_read_reg(client, SMB358_COMMAND_A, &data);
		pr_info("%s : => reg (0x%x) = 0x%x\n",
			__func__, SMB358_COMMAND_A, data);
	}

	return 0;
}


static int smb358a_disable_otg(struct i2c_client *client)
{
	u8 val = 0;
	u8 data = 0;
	int ret = 0;

	pr_info("%s :\n", __func__);

	/*Disable OTG Mode */
	ret = smb358_read_reg(client, SMB358_COMMAND_A, &val);
	if (ret >= 0) {
		data = (u8)val;
		pr_info("%s : reg (0x%x) = 0x%x\n",
			__func__, SMB358_COMMAND_A, data);
		data &= ~OTG_EN;
		if (smb358_write_reg(client, SMB358_COMMAND_A, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -1;
		}
		msleep(20);
		smb358_read_reg(client, SMB358_COMMAND_A, &data);
		pr_info("%s : => reg (0x%x) = 0x%x\n",
			__func__, SMB358_COMMAND_A, data);
	}
	/* Enable Charger */
	smb358_set_charge(1);
	return 0;
}

void smb358a_otg_enable_disable(int en)
{
	struct i2c_client *client;
	pr_info("%s\n", __func__);
	client = global_client;
	if (!client)
		return;

	if (en)
		smb358a_enable_otg(client);
	else
		smb358a_disable_otg(client);

}
EXPORT_SYMBOL(smb358a_otg_enable_disable);

static int smb358_ctrl_fg (void *data)
{
	int ret = 0;

	pr_info("%s %d\n", __func__, (int)data);
#ifdef CONFIG_STC3115_FUELGAUGE
{
	u32 temp;
	STC311x_Reset();
	read_voltage(&temp);	
	read_soc(&temp);
}
#endif
	
	return ret;
}

static bool smb358_check_vdcin(struct i2c_client *client)
{
	int val;
	u8 data = 0;
	bool ret = false;

	pr_info("%s\n", __func__);

	val = smb358_read_reg(client, SMB358_INTERRUPT_STATUS_F, &data);
	if (val >= 0) {
		if (data&(0x1<<2))
			ret = true;
	}

	return ret;
}

static bool smb358_check_bat_full(struct i2c_client *client)
{
	int val;
	u8 data = 0;
	bool ret = false;

	pr_info("%s\n", __func__);

	val = smb358_read_reg(client, SMB358_INTERRUPT_STATUS_C, &data);
	if (val >= 0) {
		if (data&(0x1<<1)) {
			ret = true; /* full */
			pr_info("%s: IRQ reason is full charging \n", __func__);
		}
	}

	return ret;
}


#ifdef CONFIG_USE_MUIC
extern void muic_set_vbus(int vbus);
#endif

/*
	check two type intrrupt.
	1) full charging.
	2) VBUS ok.

*/
static void smb358_work_func(struct work_struct *work)
{
    struct sec_charger_info *p = container_of(work, struct sec_charger_info, isr_work.work);
#ifdef CONFIG_USE_MUIC
	static bool pre_vbus = false;
	bool vbus;
#endif
    pr_info("%s\n", __func__);

    if(!p)
    {
        pr_err("%s: smb358_chip is NULL\n", __func__);
		return ;
    }

#ifdef CONFIG_USE_MUIC
	vbus = smb358_check_vdcin(p->client);
	if(pre_vbus != vbus)
	{
		pre_vbus = vbus;
		muic_set_vbus(vbus == true ? 1 : 0);
	}
#endif

    if(smb358_check_bat_full(p->client))
    {
        pr_info("%s: EOC\n", __func__);
        spa_event_handler(SPA_EVT_EOC, 0);
    }
}


static irqreturn_t smb358_irq_handler(int irq, void *data)
{
    struct sec_charger_info *p = (struct sec_charger_info *)data;

	pr_info("%s\n", __func__);
	schedule_delayed_work(&p->isr_work, msecs_to_jiffies(400));
	//smb328a_write_reg(p->client, SMB328A_CLEAR_IRQ, 1)  ;

	return IRQ_HANDLED;
}


static int smb358_irq_init(struct i2c_client *client)
{
	int ret = 0;

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL, smb358_irq_handler,
            (IRQF_TRIGGER_FALLING | IRQF_ONESHOT| IRQF_NO_SUSPEND), "smb358_charger", smb_charger);

		if (ret) {
			pr_err("%s: failed to reqeust IRQ\n", __func__);
			return ret;
		}

		ret = enable_irq_wake(client->irq);
		if (ret < 0)
			dev_err(&client->dev,"failed to enable wakeup src %d\n", ret);
	}
    else
        pr_err("%s: SMB358 IRQ is NULL\n", __func__);

	smb358_irq_handler(client->irq, smb_charger);

	return ret;
}



static int __devinit smb358_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct sec_charger_info *charger;
	int ret=0;

	pr_info("smb358_probe\n");
	global_client = client;

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	smb_charger = charger;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	charger->client = client;
	charger->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, charger);

	charger->chg_mode = CHG_MODE_NONE;

	INIT_DELAYED_WORK(&charger->isr_work, smb358_work_func);

	//smb358_test_read(charger->client);
	smb358_chg_init(charger->client);
	smb358_start_chg();
	//smb358_test_read(charger->client);


	spa_agent_register(SPA_AGENT_SET_CHARGE, (void*)smb358_set_charge, "smb358-charger");
	spa_agent_register(SPA_AGENT_SET_CHARGE_CURRENT, (void*)smb358_set_charge_current, "smb358-charger");
	spa_agent_register(SPA_AGENT_SET_FULL_CHARGE, (void*)smb358_set_full_charge, "smb358-charger");
	spa_agent_register(SPA_AGENT_GET_CAPACITY, (void*)smb358_get_capacity, "smb358-charger");
	spa_agent_register(SPA_AGENT_GET_TEMP, (void*)smb358_get_temp, "smb358-charger");
	spa_agent_register(SPA_AGENT_GET_VOLTAGE, (void*)smb358_get_voltage, "smb358-charger");
	spa_agent_register(SPA_AGENT_GET_BATT_PRESENCE, (void*)smb358_get_batt_presence, "smb358-charger");
	spa_agent_register(SPA_AGENT_GET_CHARGER_TYPE, (void*)smb358_get_charger_type, "smb358-charger");
	spa_agent_register(SPA_AGENT_CTRL_FG, (void*)smb358_ctrl_fg, "smb358-charger");

//	smb358_charger_function_conrol(client, 500);

	smb358_irq_init(client);

#if !(defined(CONFIG_STC3115_FUELGAUGE))
#ifdef CONFIG_BATTERY_D2153
		d2153_battery_start();
#endif
#endif

	return ret;
}

static int __devexit smb358_remove(struct i2c_client *client)
{

	return 0;
}

static int smb358_resume(struct i2c_client *client)
{
	return 0;
}


static const struct i2c_device_id smb358_id[] = {
	{"smb358", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, smb358_id);

static struct i2c_driver smb358_i2c_driver = {
	.driver = {
		.name = "smb358",
	},
	.probe = smb358_probe,
	.remove = __devexit_p(smb358_remove),
	.resume = smb358_resume,
	.id_table = smb358_id,
};

static int __init smb358_init(void)
{
	return i2c_add_driver(&smb358_i2c_driver);
}
//module_init(smb358_init);
subsys_initcall_sync(smb358_init);


static void __exit smb358_exit(void)
{
	i2c_del_driver(&smb358_i2c_driver);
}
module_exit(smb358_exit);

MODULE_AUTHOR("Sehyoung Park <sh16.park@samsung.com>");
MODULE_DESCRIPTION("SMB358 USB Switch driver");
MODULE_LICENSE("GPL");


