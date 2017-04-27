/* drivers/mfd/richtek/rt9450.c
 * Driver to Richtek RT9450 switch mode charger
 *
 * Copyright (C) 2012
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/platform_data/rtsmc.h>
#include "rt9450.h"

#define DEVICE_NAME "rt9450"
#define RT9450_DRV_NAME "rt9450"
#define RT9450_TIMER_TIMEOUT 25

#define ID_RT9450A 0x01
#define ID_RT9450B 0x02
#define ID_RT9450C 0x03
#define ID_RT9450_UNKNOW 0x0A
#define DELAY0 0

#define I2CRByte(x) i2c_smbus_read_byte_data(pClient,x)
#define I2CWByte(x,y) i2c_smbus_write_byte_data(pClient,x,y)

static const char * chips_name[] = {"RT9450","RT9450A",
    "RT9450B","RT9450C","Unkonw"};

struct rt9450_data* pDrvData = NULL;
struct rtsmc_platform_data platform_data;

static const char* getModelName(int model)
{
    switch(model)
    {
        case 0x01:
            return chips_name[1];
        case 0x02:
            return chips_name[2];
        case 0x03:
            return chips_name[3];
        default:
            return chips_name[4]; // UNKNOW
    }
    return chips_name[4]; // UNKNOW
}
static int getModel(int32_t chip_id)
{
    switch(chip_id)
    {
        case RT9450A_CHIP_ID:
        return ID_RT9450A;
        case RT9450B_CHIP_ID:
        return ID_RT9450B;
        case RT9450C_CHIP_ID:
        return ID_RT9450C;
        default:
        return ID_RT9450_UNKNOW;
    }
    return ID_RT9450_UNKNOW;
}

static int rt9450_charger_event(unsigned long event);

void RT945x_enable_charge(int32_t en)
{
    rt9450_charger_event(en?eRT945x_START_CHARGE:eRT945x_STOP_CHARGE);
}
EXPORT_SYMBOL_GPL(RT945x_enable_charge);

void RT945x_enable_otg(int32_t en)
{
    rt9450_charger_event((en>0)?eRT_ATTACH_OTG:eRT_DETACH_OTG);
}
EXPORT_SYMBOL_GPL(RT945x_enable_otg);

#ifdef CONFIG_RT_SYSFS_DBG

static ssize_t reg_addr_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf,int32_t nRegAddr)
{
    struct i2c_client* pClient = pDrvData->client;
    return sprintf(buf,"%d\n",I2CRByte(nRegAddr));
}
static ssize_t reg_addr_store(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               const char *buf, size_t len,int32_t nRegAddr)
{
    uint32_t value = simple_strtoul(buf, NULL, 10);
    struct i2c_client* pClient = pDrvData->client;
    I2CWByte(nRegAddr,value);
    return len;
}
#define regRO(addr) \
    static ssize_t reg##addr##_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf) \
    { return reg_addr_show(kobj, attr, buf, addr); }

#define regRW(addr) \
    static ssize_t reg##addr##_store(struct kobject * kobj,struct kobj_attribute * attr,const char * buf, size_t len) \
    { return reg_addr_store(kobj,attr,buf,len,addr);} \
    regRO(addr)

regRW(0x00)
regRW(0x01)
regRW(0x02)
regRO(0x03)
regRW(0x04)
regRO(0x05)
regRO(0x06)

static ssize_t model_name_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"%s\n",getModelName(pDrvData->chip_model));
}

static ssize_t status_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"0x%x\n",pDrvData->status_reg);
}

static ssize_t chip_id_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"0x%x\n",pDrvData->chipid_reg);
}

static ssize_t bmfs_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"0x%x\n",pDrvData->bmfs_reg);
}

static ssize_t cmfs_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"0x%x\n",pDrvData->cmfs_reg);
}

static ssize_t enable_charge_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"%d\n",pDrvData->charge_in_progress?1:0);
}

static ssize_t enable_charge_store(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               const char *buf, size_t len)
{
    uint32_t value = simple_strtoul(buf, NULL, 10);
    RT945x_enable_charge(value);
    return len;
}

static ssize_t enable_otg_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"%d\n",(pDrvData->active&0x2)?1:0);
}

static ssize_t enable_otg_store(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               const char *buf, size_t len)
{
    uint32_t value = simple_strtoul(buf, NULL, 10);
    RT945x_enable_otg(value);
    return len;
}


static struct kobj_attribute reg0x00_attribute = (struct kobj_attribute)__ATTR_RW(reg0x00);
static struct kobj_attribute reg0x01_attribute = (struct kobj_attribute)__ATTR_RW(reg0x01);
static struct kobj_attribute reg0x02_attribute = (struct kobj_attribute)__ATTR_RW(reg0x02);
static struct kobj_attribute reg0x03_attribute = (struct kobj_attribute)__ATTR_RO(reg0x03);
static struct kobj_attribute reg0x04_attribute = (struct kobj_attribute)__ATTR_RW(reg0x04);
static struct kobj_attribute reg0x05_attribute = (struct kobj_attribute)__ATTR_RO(reg0x05);
static struct kobj_attribute reg0x06_attribute = (struct kobj_attribute)__ATTR_RO(reg0x06);

static struct kobj_attribute model_name_attribute = (struct kobj_attribute)__ATTR_RO(model_name);
static struct kobj_attribute status_attribute = (struct kobj_attribute)__ATTR_RO(status);
static struct kobj_attribute chip_id_attribute = (struct kobj_attribute)__ATTR_RO(chip_id);
static struct kobj_attribute bmfs_attribute = (struct kobj_attribute)__ATTR_RO(bmfs);
static struct kobj_attribute cmfs_attribute = (struct kobj_attribute)__ATTR_RO(cmfs);
static struct kobj_attribute enable_charge_attribute = (struct kobj_attribute)__ATTR_RW(enable_charge);
static struct kobj_attribute enable_otg_attribute = (struct kobj_attribute)__ATTR_RW(enable_otg);
static struct attribute* rt9450_dbg_attrs [] =
{
    &reg0x00_attribute.attr,
    &reg0x01_attribute.attr,
    &reg0x02_attribute.attr,
    &reg0x03_attribute.attr,
    &reg0x04_attribute.attr,
    &reg0x05_attribute.attr,
    &reg0x06_attribute.attr,
    &model_name_attribute.attr,
    &status_attribute.attr,
    &chip_id_attribute.attr,
    &bmfs_attribute.attr,
    &cmfs_attribute.attr,
    &enable_charge_attribute.attr,
    &enable_otg_attribute.attr,
    NULL,
};
static struct attribute_group rt9450_dbg_attrs_group =
{
    .name = "dbg",
    .attrs = rt9450_dbg_attrs,
};
#endif //CONFIG_RT_SYSFS_DBG


static void rt9450ac_charger_work(struct work_struct *work)
{
	struct rt9450_data *drv_data = container_of(work,
		struct rt9450_data, rt9450_charger_work.work);
    struct i2c_client * pClient = drv_data->client;
    int32_t bmfs_reg,cmfs_reg,status;
    bool charge_in_progress;
    INFO("Charger Work is triggered\n");
    status = I2CRByte(RT9450_REG_STATUS);
    if (status<0)
        WARNING("I2C Read error (%d)\r\n",status);

    charge_in_progress = ((status&0x38) == 0x10)?true:false;
    INFO("Status reg = 0x%x, charge_in_progress = %s\r\n",status,charge_in_progress?"Yes":"No");

    I2CWByte(RT9450_REG_STATUS,status|0x80);

    bmfs_reg = I2CRByte(RT9450_REG_BMFS);
    cmfs_reg = I2CRByte(RT9450_REG_CMFS);

    switch(status&0x30)
    {
        case 0x20:
        //Charge Done
        if (platform_data.rtsmc_feedback)
        platform_data.rtsmc_feedback(eRT945x_CHARGE_DONE);
        break;

        case 0x30:
        //Fault
        if (platform_data.rtsmc_feedback)
        {
            if (cmfs_reg&0x80)
                platform_data.rtsmc_feedback(eRT945x_FAULT_CHGVINUV);
            if (cmfs_reg&0x40)
                platform_data.rtsmc_feedback(eRT945x_FALUT_CHGSLP);
            if (cmfs_reg&0x10)
                platform_data.rtsmc_feedback(eRT945x_FALUT_CHGOT);
            if (cmfs_reg&0x08)
                platform_data.rtsmc_feedback(eRT945x_FALUT_CHGBATAB);
            if (cmfs_reg&0x02)
                platform_data.rtsmc_feedback(eRT945x_FALUT_CHGBATOV);
            if (cmfs_reg&0x01)
                platform_data.rtsmc_feedback(eRT945x_FALUT_CHGVINOV);
            if (bmfs_reg&0x40)
                platform_data.rtsmc_feedback(eRT945x_FALUT_CHGDPM);
            if (bmfs_reg&0x10)
                platform_data.rtsmc_feedback(eRT945x_FALUT_BSTOT);
            if (bmfs_reg&0x08)
                platform_data.rtsmc_feedback(eRT945x_FALUT_BSTBATOV);
            if (bmfs_reg&0x04)
                platform_data.rtsmc_feedback(eRT945x_FALUT_BSTBATUV);
            if (bmfs_reg&0x02)
                platform_data.rtsmc_feedback(eRT945x_FALUT_BSTOLP);
            if (bmfs_reg&0x01)
                platform_data.rtsmc_feedback(eRT945x_FALUT_BSTVINOV);
        }
        break;
    }
    if (charge_in_progress!=pDrvData->charge_in_progress)
    {
        pDrvData->charge_in_progress = charge_in_progress;
        platform_data.rtsmc_feedback(charge_in_progress?eRT945x_START_CHARGE:eRT945x_STOP_CHARGE);
    }
    if (charge_in_progress)
        drv_data->active |= 0x01;
    else
        drv_data->active &= (~0x01);
    if (status&0x08)
        drv_data->active |=0x02;
    else
        drv_data->active &= (~0x02);
    pDrvData->bmfs_reg = bmfs_reg;
    pDrvData->cmfs_reg = cmfs_reg;
    cancel_delayed_work(&drv_data->rt9450_charger_work);
    if (drv_data->active!=0)
    {
         schedule_delayed_work(&drv_data->rt9450_charger_work,
						RT9450_TIMER_TIMEOUT * HZ);
        INFO("Schedule next delay work for WDT reset\r\n");
    }
}

static void rt9450b_charger_work(struct work_struct *work)
{
	struct rt9450_data *drv_data = container_of(work,
		struct rt9450_data, rt9450_charger_work.work);
    struct i2c_client * pClient = drv_data->client;
    int32_t status;
    bool charge_in_progress;
    INFO("Charger Work is triggered\n");
    status = I2CRByte(RT9450_REG_STATUS);
    if (status<0)
        WARNING("I2C Read error (%d)\r\n",status);

    charge_in_progress = ((status&0x38) == 0x10)?true:false;
    INFO("Status reg = 0x%x, charge_in_progress = %s\r\n",status,charge_in_progress?"Yes":"No");

    I2CWByte(RT9450_REG_STATUS,status|0x80);

    switch(status&0x30)
    {
        case 0x20:
        //Charge Done
        if (platform_data.rtsmc_feedback)
        platform_data.rtsmc_feedback(eRT945x_CHARGE_DONE);
        break;

        case 0x30:
        //Fault
        if (platform_data.rtsmc_feedback)
        {
            if (status&0x08) // in boost mode
            {
                switch(status&0x07)
                {
                    case 1: // OVP
                    platform_data.rtsmc_feedback(eRT945x_FALUT_BSTVINOV);
                    break;
                    case 2:
                    platform_data.rtsmc_feedback(eRT945x_FALUT_BSTOLP);
                    break;
                    case 3:
                    platform_data.rtsmc_feedback(eRT945x_FALUT_BSTBATUV);
                    break;
                    case 4:
                    platform_data.rtsmc_feedback(eRT945x_FALUT_BSTBATOV);
                    break;
                    case 5:
                    platform_data.rtsmc_feedback(eRT945x_FALUT_BSTOT);
                    break;
                    default:
                    WARNING("No implement of CODE = 0x%x\r\n",(int32_t)status&0x07);
                }

            }
            else
            {
                switch(status&0x07)
                {
                    case 1: // OVP
                    platform_data.rtsmc_feedback(eRT945x_FALUT_CHGVINOV);
                    break;
                    case 2: //charge slp
                    platform_data.rtsmc_feedback(eRT945x_FALUT_CHGSLP);
                    break;
                    case 3: //VBUS UVLO
                    platform_data.rtsmc_feedback(eRT945x_FAULT_CHGVINUV);
                    break;
                    case 4: // BATT OVP
                    platform_data.rtsmc_feedback(eRT945x_FALUT_CHGBATOV);
                    break;
                    case 5: // OTP
                    platform_data.rtsmc_feedback(eRT945x_FALUT_CHGOT);
                    break;
                    case 7: // OVP
                    platform_data.rtsmc_feedback(eRT945x_FALUT_CHGBATAB);
                    break;
                    default:
                    WARNING("No implement of CODE = 0x%x\r\n",(int32_t)status&0x07);
                }
            }
        }
        break;
    }
    if (charge_in_progress!=pDrvData->charge_in_progress)
    {
        pDrvData->charge_in_progress = charge_in_progress;
        platform_data.rtsmc_feedback(charge_in_progress?eRT945x_START_CHARGE:eRT945x_STOP_CHARGE);
    }
    if (charge_in_progress)
        drv_data->active |= 0x01;
    else
        drv_data->active &= (~0x01);
    if (status&0x08)
        drv_data->active |=0x02;
    else
        drv_data->active &= (~0x02);
    cancel_delayed_work(&drv_data->rt9450_charger_work);
    if (drv_data->active!=0)
    {
         schedule_delayed_work(&drv_data->rt9450_charger_work,
						RT9450_TIMER_TIMEOUT * HZ);
        INFO("Schedule next delay work for WDT reset\r\n");
    }
}

static void rt9450_charger_work(struct work_struct *work)
{
	struct rt9450_data *drv_data = container_of(work,
		struct rt9450_data, rt9450_charger_work.work);

    switch (drv_data->chip_model)
    {
        case ID_RT9450A:
        case ID_RT9450C:
        INFO("RT9450A/C charger work\r\n");
        rt9450ac_charger_work(work);
        break;
        case ID_RT9450B:
        INFO("RT9450B charger work\r\n");
        rt9450b_charger_work(work);
        break;
        default:
        WARNING("Error chip id\r\n");

    }
}


static bool init_reg_setting(void)
{
    struct i2c_client* pClient = pDrvData->client;
    I2CWByte(0x04,0x80); // reset
    pDrvData->chipid_reg = I2CRByte(RT9450_REG_CHIP_ID);
    if (pDrvData->chipid_reg<0)
    {
        ERR("Error : i2c i/o error (%d)\n",pDrvData->chipid_reg);
        return false;
    }
    if ((pDrvData->chipid_reg&0xe0)!= 0x80)
    {
        ERR("Error : Not RT9450x(0x%x)\n",pDrvData->chipid_reg);
        return false;
    }
    pDrvData->chip_model = getModel(pDrvData->chipid_reg);
    if (pDrvData->chip_model==ID_RT9450_UNKNOW)
        WARNING("Warning : unknow 9450 id (0x%x)\n",pDrvData->chipid_reg);
    INFO("Chip Model = %s\n",getModelName(pDrvData->chip_model));
    pDrvData->control_reg = CONFIG_RT9450_CTRL;
    pDrvData->voltage_reg = CONFIG_RT9450_VOLTAGE;
    pDrvData->current_reg = CONFIG_RT9450_CURRENT;
    I2CWByte(RT9450_REG_CTRL,pDrvData->control_reg);
    I2CWByte(RT9450_REG_VOLTAGE,pDrvData->voltage_reg);
    I2CWByte(RT9450_REG_CURRENT,pDrvData->current_reg);
    pDrvData->status_reg = I2CRByte(RT9450_REG_STATUS);
    if (pDrvData->chip_model == ID_RT9450A || pDrvData->chip_model == ID_RT9450C)
    {
        pDrvData->bmfs_reg = I2CRByte(RT9450_REG_BMFS);
        pDrvData->cmfs_reg = I2CRByte(RT9450_REG_CMFS);
    }
    if ((pDrvData->status_reg&0x30)==0x10)
    {
        INFO("Already started charaing\n");
        pDrvData->active =0x01;
        schedule_delayed_work(&pDrvData->rt9450_charger_work,DELAY0);
    }
    else
    {
        pDrvData->active =0x00;
    }
    return true;
}
static int rt9450_charger_event(unsigned long event)
{
    int ret = 0;
    struct rt9450_data* drv_data = pDrvData;
    struct i2c_client* pClient;
    pClient = drv_data->client;
    I2CWByte(RT9450_REG_STATUS,pDrvData->status_reg|0x80);
    switch(event)
    {
        case eRT_START_CHARING:
        drv_data->active |= 0x01;
        schedule_delayed_work(&drv_data->rt9450_charger_work,HZ);
        break;
        case eRT_STOP_CHARING:
        drv_data->active &= 0x02;
        if (drv_data->active==0x00)
        {
            cancel_delayed_work(&drv_data->rt9450_charger_work);
        }
        break;
        case eRT_ATTACH_OTG:
        drv_data->control_reg |= 0x01;
        INFO("Ctrl Reg = 0x%x\r\n",(int32_t)(drv_data->control_reg)&(~0x08));
        I2CWByte(RT9450_REG_CTRL,(drv_data->control_reg)&(~0x08));
        drv_data->active |= 0x02;
        schedule_delayed_work(&drv_data->rt9450_charger_work,HZ);
        break;
        case eRT_DETACH_OTG:
        drv_data->control_reg &= (~0x01);
        INFO("Ctrl Reg = 0x%x\r\n",(int32_t)drv_data->control_reg);
        I2CWByte(RT9450_REG_CTRL,drv_data->control_reg);
        drv_data->active &= 0x01;
        if (drv_data->active==0x00)
        {
            cancel_delayed_work(&drv_data->rt9450_charger_work);
        }
        break;
    }
    return ret;
}

static irqreturn_t rt9450smc_irq_handler(int irq, void *data)
{
    INFO("Int triggered\r\n");
    cancel_delayed_work(&pDrvData->rt9450_charger_work);
    schedule_delayed_work(&pDrvData->rt9450_charger_work,DELAY0);
    return IRQ_HANDLED;
}

static int rt9450_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
    int err;
    struct rt9450_data* drv_data;

    INFO("RT9450 irq # = %d\n",client->irq);
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
    {
        ERR("No Support for I2C_FUNC_SMBUS_BYTE_DATA\n");
        err = -ENODEV;
        goto i2c_check_functionality_fail;
    }
    drv_data = kzalloc(sizeof(struct rt9450_data),GFP_KERNEL);
    drv_data->client = client;
    pDrvData = drv_data;
    i2c_set_clientdata(client,drv_data);
    if (client->dev.platform_data)
        memcpy(&platform_data,client->dev.platform_data,sizeof(struct rtsmc_platform_data));
    else
        memset(&platform_data,0,sizeof(struct rtsmc_platform_data));

    INIT_DELAYED_WORK_DEFERRABLE(&drv_data->rt9450_charger_work,rt9450_charger_work);

#if CONFIG_RTSMC_IRQ_NUMBER<0
    client->irq = gpio_to_irq(CONFIG_RTSMC_INT_GPIO_NUMBER);
#endif

#ifdef CONFIG_RTSMC_INT_CONFIG
    INFO("gpio pin # = %d\n",(int)CONFIG_RTSMC_INT_GPIO_NUMBER);
    err = gpio_request(CONFIG_RTSMC_INT_GPIO_NUMBER,"RT9450_EINT");         // Request for gpio pin
    if (err<0)
        WARNING("Request GPIO %d failed\n",(int)CONFIG_RTSMC_INT_GPIO_NUMBER);
    err = gpio_direction_input(CONFIG_RTSMC_INT_GPIO_NUMBER);
    if (err<0)
        WARNING("Set GPIO Direction to input : failed\n");
#endif // CONFIG_RTMUSC_INT_CONFIG


    if (!init_reg_setting())
    {
        err = -EINVAL;
        goto init_fail;
    }

#ifdef CONFIG_RT_SYSFS_DBG
    err = sysfs_create_group(&client->dev.kobj, &rt9450_dbg_attrs_group);
   if (err<0)
    {
        WARNING("Can't create engineer debug node), error code = %d\r\n",err);
    }
#endif //CONFIG_RT_SYSFS_DBG

    err = request_irq(client->irq, rt9450smc_irq_handler, IRQF_TRIGGER_FALLING, DEVICE_NAME, drv_data);
    if (err<0)
    {
        WARNING("Can't use INT (use polling only), error code = %d\r\n",err);
    }
    return 0;
    init_fail:
    cancel_delayed_work(&drv_data->rt9450_charger_work);
    kfree(pDrvData);
    pDrvData = NULL;
    i2c_check_functionality_fail:
    return err;
}
static int rt9450_remove(struct i2c_client *client)
{
#ifdef CONFIG_RT_SYSFS_DBG
    sysfs_remove_group(&client->dev.kobj, &rt9450_dbg_attrs_group);
#endif //CONFIG_RT_SYSFS_DBG
    if (pDrvData)
    {
        cancel_delayed_work(&pDrvData->rt9450_charger_work);
        flush_scheduled_work();
        kfree(pDrvData);
        pDrvData = NULL;
    }
    return 0;
}

static const struct i2c_device_id rt9450_id[] =
{
    {"rt9450", 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, rt9450_id);

static struct i2c_driver rt9450_i2c_driver =
{
    .driver = {
        .name = RT9450_DRV_NAME,
    },
    .probe = rt9450_probe,
    .remove = rt9450_remove,
    .id_table = rt9450_id,
};

static int __init rt9450_init(void)
{
	int ret;
    ret = i2c_add_driver(&rt9450_i2c_driver);
    if (ret)
    {
        WARNING("i2c_add_driver fail\n");
        return ret;
    }

    if (pDrvData == NULL)
    {
        WARNING("pDrvData = NULL\n");
        ret = -EINVAL;
        goto alloc_device_fail;
    }
	INFO("RT9450 Module initialized.\n");
    return 0;

alloc_device_fail:
    i2c_del_driver(&rt9450_i2c_driver);
    ERR("RT9450 Module initialization failed.\n");
    return ret;
}

static void __exit rt9450_exit(void)
{

    i2c_del_driver(&rt9450_i2c_driver);
    INFO("RT9450 Module deinitialized.\n");
}
MODULE_AUTHOR("Patrick Chang <weichung.chang@gmail.com>");
MODULE_DESCRIPTION("Richtek RT9450 switch mode charger device driver");
MODULE_LICENSE("GPL");
module_init(rt9450_init);
module_exit(rt9450_exit);
