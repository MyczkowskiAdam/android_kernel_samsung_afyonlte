/* drivers/mfd/richtek/rt8973.c
 * Driver to Richtek RT8973 micro USB switch device
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
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include <linux/platform_data/rtmusc.h>
#include <linux/wakelock.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/switch.h>
#include <linux/power_supply.h>
#include "rt8973.h"

#define TSU6712_UART_AT_MODE           2
#define TSU6712_UART_INVALID_MODE      -1
#define TSU6712_UART_EMPTY_CR          3
#define TSU6712_UART_EMPTY_CRLF        4
#define TSU6712_UART_AT_MODE_MODECHAN  5

#ifdef CONFIG_SEC_CHARGING_FEATURE
#include <linux/spa_power.h>
#endif
#if defined(CONFIG_RT8969)||defined(CONFIG_RT8973)
#include <linux/platform_data/rtmusc.h>
#endif
#if defined(CONFIG_RT9450AC) || defined(CONFIG_RT9450B)
#include <linux/platform_data/rtsmc.h>
#endif

#define MUSB_IC_UART_AT_MODE           2
#define MUSB_IC_UART_INVALID_MODE      -1
#define MUSB_IC_UART_EMPTY_CR          3
#define MUSB_IC_UART_EMPTY_CRLF        4
#define MUSB_IC_UART_AT_MODE_MODECHAN  5

void send_usb_insert_event(int);

#define DEVICE_NAME "rt8973"
#define RT8973_DRV_NAME "rt8973"

#define EN_REV0_INT_BOUNCE_FIX 1

#if EN_REV0_INT_BOUNCE_FIX
#define EN_ADCCHG_MASK() en_adcchg_mask()
#define DIS_ADCCHG_MASK() dis_adcchg_mask()
#else
#define EN_ADCCHG_MASK()
#define DIS_ADCCHG_MASK()
#endif


static struct rt8973_data* pDrvData = NULL; /*driver data*/
static struct platform_device *rtmus_dev = NULL; /* Device structure */
static struct rtmus_platform_data platform_data;
static struct workqueue_struct *rtmus_work_queue = NULL; /* self-own work queue */
static enum cable_type_t set_cable_status;
static int usb_uart_switch_state;
char at_isi_switch_buf[400] = {0};
static struct switch_dev switch_usb_uart = {
        .name = "tsu6712",
};

// temporary avoid build error for logan
__weak int KERNEL_LOG;

#define ID_NONE 0
#define ID_USB  1
#define ID_UART 2
#define ID_CHARGER  3
#define ID_JIG  4
#define ID_UNKNOW 5
#define ID_OTG  6
#define ID_JIG_UART 7
#define ID_USB_CDP 8

#define MAX_DCDT_retry 2
static char * devices_name[] = { "NONE",
                        "USB",
                        "UART",
                        "CHARGER",
                        "JIG",
                        "UNKONW",
                        "OTG",
                        };
static int32_t DCDT_retry = 0;


#define I2C_RW_RETRY_MAX 2
#define I2C_RW_RETRY_DELAY 20

#if 1
#define I2CRByte(x) i2c_smbus_read_byte_data(pClient,x)
#define I2CWByte(x,y) i2c_smbus_write_byte_data(pClient,x,y)
#else
#define I2CRByte(x) retry_i2c_smbus_read_byte_data(pClient,x)
#define I2CWByte(x,y) retry_i2c_smbus_write_byte_data(pClient,x,y)

static s32 retry_i2c_smbus_write_byte_data(struct i2c_client *client,
				     u8 command, u8 value)
{
    s32 result=-1,i;
    for (i=0;i<I2C_RW_RETRY_MAX&&result<0;i++)
    {
        result = i2c_smbus_write_byte_data(client,command,value);
        if (unlikely(result<0))
            msleep(I2C_RW_RETRY_DELAY);
    }
    return result;
}

static s32 retry_i2c_smbus_read_byte_data(struct i2c_client *client, u8 command)
{
    s32 result=-1,i;
    for (i=0;i<I2C_RW_RETRY_MAX&&result<0;i++)
    {
        result = i2c_smbus_read_byte_data(client,command);
        if (unlikely(result<0))
            msleep(I2C_RW_RETRY_DELAY);
    }
    return result;
}
#endif

#define SWITCH_AT       103
#define SWITCH_ISI      104
extern int stop_isi;
static int isi_mode; /*initialized to 0 */
char at_isi_mode[100] = {0};

static ssize_t tsu6712_show_UUS_state(struct device *dev,
                                   struct device_attribute *attr,
                                   char *buf)
{
        return snprintf(buf, 4, "%d\n", usb_uart_switch_state);
}

ssize_t ld_set_manualsw(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count)
{
        printk(" ld_set_manualsw invoked\n");
        if (0 == strncmp(buf, "switch at", 9)) {
                printk(" ld_set_manualsw switch at\n");
                memset((char *)at_isi_mode, 0, 100);
                strcpy((char *)at_isi_mode, "at");
                switch_set_state(&switch_usb_uart, SWITCH_AT);

                stop_isi = 1;
		isi_mode = 0;
        }
        if (0 == strncmp(buf, "switch isi", 10)) {
                printk(" ld_set_manualsw switch isi\n");
                memset((char *)at_isi_mode, 0, 100);
                strcpy((char *)at_isi_mode, "isi");
                switch_set_state(&switch_usb_uart, SWITCH_ISI);
                stop_isi = 0;
		isi_mode = 1;
        }
        return count;
}
ssize_t ld_set_switch_buf(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int i;
	char temp[100];
	char *ptr = NULL;
	char *ptr2 = NULL;
	char atbuf[] = "AT+ATSTART\r";
	char atmodechanbuf[] = "AT+MODECHAN=0,2\r";
	char isi_cmd_buf[] = "switch isi";
	int error = 0;

	/* If UART is not connected ignore this sysfs access*/
	if (200 != usb_uart_switch_state)
		return 0;

	memset(temp, 0, 100);
	for (i = 0; i < count; i++)
		temp[i] = toupper(buf[i]);

	strncat((char *)at_isi_switch_buf, temp, count);

	if ((strncmp((char *)at_isi_switch_buf, "\n", 1) == 0) || \
	    (strncmp((char *)at_isi_switch_buf, "\r", 1) == 0) || \
	    (strncmp((char *)at_isi_switch_buf, "\r\n", 2) == 0)) {
		memset(at_isi_switch_buf, 0, 400);
		KERNEL_LOG = 0;
		return MUSB_IC_UART_EMPTY_CRLF;
	}

	if (strstr(at_isi_switch_buf, "\r\n"))
		printk(KERN_DEBUG"###TEST0### r n\n");
	else if (strstr(at_isi_switch_buf, "\t\n"))
		printk(KERN_DEBUG"###TEST1### t n\n");
	else if (strstr(at_isi_switch_buf, "\n"))
		printk(KERN_DEBUG"###TEST2### n\n");

	ptr = strstr(atbuf, at_isi_switch_buf);
	ptr2 = strstr(atmodechanbuf, at_isi_switch_buf);
	if (((NULL == ptr) || (ptr != atbuf)) &&
	     ((NULL == ptr2) || (ptr2 != atmodechanbuf))) {
		if (strstr("AT+ISISTART", at_isi_switch_buf) == NULL &&
		    strstr("AT+MODECHAN=0,0", at_isi_switch_buf) == NULL)
			error = 1;
	}

	if (strstr(at_isi_switch_buf, atbuf) != NULL) {
		KERNEL_LOG = 0;
		memset(at_isi_switch_buf, 0, 400);
		return MUSB_IC_UART_AT_MODE;
	} else if (strstr(at_isi_switch_buf, atmodechanbuf) != NULL) {
		KERNEL_LOG = 0;
		memset(at_isi_switch_buf, 0, 400);
		return MUSB_IC_UART_AT_MODE_MODECHAN;
	} else if (strstr(at_isi_switch_buf, "AT+ISISTART") != NULL ||
		   strstr(at_isi_switch_buf, "AT+MODECHAN=0,0") != NULL) {

		// do not switch to isi mode if isi mode already set
		if (isi_mode == 0) {
			KERNEL_LOG = 0;
			memset(at_isi_switch_buf, 0, 400);
			ld_set_manualsw(NULL, NULL, isi_cmd_buf,
				strlen(isi_cmd_buf));
			return count;
               }
       }

	/* this sends response if at+isistart is given in isi mode */
	if (strstr(at_isi_switch_buf, "AT+ISISTART\r") != NULL ||
		strstr(at_isi_switch_buf, "AT+MODECHAN=0,0\r") != NULL) {
		memset(at_isi_switch_buf, 0, 400);
		ld_set_manualsw(NULL, NULL, isi_cmd_buf, strlen(isi_cmd_buf));
		return TSU6712_UART_INVALID_MODE;
	}

	if (error != 0) {
		count = MUSB_IC_UART_INVALID_MODE;
		memset(at_isi_switch_buf, 0, 400);
	}

	return count;
}

static ssize_t ld_show_switch_buf(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        strcpy(buf, at_isi_switch_buf);
        printk("BUF from TSU %s\n", at_isi_switch_buf);
        return strlen(at_isi_switch_buf);
}


static ssize_t ld_show_mode(struct device *dev,
                                   struct device_attribute *attr,
                                   char *buf)
{
        strcpy(buf, at_isi_mode);
        printk("LD MODE from TSU %s\n", at_isi_mode);
        return 3;
}

static ssize_t ld_show_manualsw(struct device *dev,
                struct device_attribute *attr, char *buf)
{
        return 0;
}

/* JIRA ID 1362/1396
Sysfs interface to release and acquire
uart-wakelock from user space */
ssize_t ld_uart_wakelock(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count)
{
        int ret = 0;
        int buf_val = 0;

        if (pDrvData != NULL) {
                ret = sscanf(buf, "%d", &buf_val);
                if (1 != ret) {
                        printk(KERN_ERR \
                                "ld_uart_wakelock - Failed to read value\n");
                        return -EINVAL;
                }
                if (buf_val == 1) {
                        /* Release wakelock
                        to allow device to get into deep sleep
                        when UART JIG is disconnected */
                        wake_unlock(&pDrvData->uart_wakelock);
                        ret = count ;
                } else if (buf_val == 0) {
                        /* Acquire wakelock
                        to avoid device getting into deep sleep
                        when UART JIG is connected*/
                        wake_lock(&pDrvData->uart_wakelock);
                        ret = count ;
                }
        }
        return ret ;
}


static DEVICE_ATTR(at_isi_switch, S_IRUGO | S_IWUSR,
                ld_show_manualsw, ld_set_manualsw);
static DEVICE_ATTR(at_isi_mode, S_IRUGO | S_IWUSR,
                ld_show_mode, NULL);
static DEVICE_ATTR(at_isi_switch_buf, S_IRUGO | S_IWUSR,
                ld_show_switch_buf, ld_set_switch_buf);
static DEVICE_ATTR(UUS_state, S_IRUGO, tsu6712_show_UUS_state, NULL);
/* JIRA ID 1362/1396
Sysfs interface to release and acquire uart-wakelock from user space */
static DEVICE_ATTR(uart_wakelock, S_IRUGO | S_IWUSR,
                NULL, ld_uart_wakelock);

static struct attribute *tsu6712_attributes[] = {
  //        &dev_attr_switch.attr,
        &dev_attr_at_isi_switch.attr,   /* AT-ISI Separation */
        &dev_attr_at_isi_mode.attr,          /* AT-ISI Separation */
        &dev_attr_at_isi_switch_buf.attr,    /* AT-ISI Separation */
        &dev_attr_UUS_state.attr,
        /* JIRA ID 1362/1396
        uart-wakelock release */
        &dev_attr_uart_wakelock.attr,
        NULL
};

static struct kobject *usb_kobj;
#define USB_FS	"usb_atparser"
static const struct attribute_group tsu6712_group = {
        .attrs = tsu6712_attributes,
};

static int usb_sysfs_init(void)
{
	int ret;
	usb_kobj = kobject_create_and_add(USB_FS, kernel_kobj);
	if (!usb_kobj)
		return 0;
	ret = sysfs_create_group(usb_kobj, &tsu6712_group);
	if (ret)
		kobject_put(usb_kobj);
	return ret;
}

int get_cable_type(void)
{    
    return set_cable_status;
}
EXPORT_SYMBOL(get_cable_type);

int uart_connecting;
EXPORT_SYMBOL(uart_connecting);

static int rt_sysfs_create_files(struct kobject *kobj,struct attribute** attrs)
{
    int err;
    while(*attrs!=NULL)
    {
        err = sysfs_create_file(kobj,*attrs);
        if (err)
            return err;
        attrs++;
    }
    return 0;
}

static ssize_t accessory_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"%s\n",devices_name[pDrvData->accessory_id]);
}

static ssize_t chip_id_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"0x%x\n",pDrvData->chip_id);
}
static ssize_t driver_version_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"%s\n",RTMUSC_DRIVER_VER);
}
static ssize_t usbid_adc_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"0x%x\n",pDrvData->usbid_adc);
}

static ssize_t factory_mode_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"%d\n",pDrvData->factory_mode);
}

static ssize_t operating_mode_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf)
{
    return sprintf(buf,"%d\n",pDrvData->operating_mode);
}

static int rt8973_ex_init(void)
{
	int ret;
	/* for usb uart switch */
	ret = switch_dev_register(&switch_usb_uart);
	if (ret < 0) {
			ERR("Failed to register usb_uart switch. %d\n", ret);
			return ret;
	}

   return 0;
}
#if defined(CONFIG_RT8969)||defined(CONFIG_RT8973)
static void usb_attach(uint8_t attached)
{
	printk(attached ? "USB attached\n" : "USB detached\n");
	set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;
	if ( attached ) {
		usb_uart_switch_state = 100;
		send_usb_insert_event(1);
		spa_event_handler(SPA_EVT_CHARGER, (void *)POWER_SUPPLY_TYPE_USB);
		switch_set_state(&switch_usb_uart,100);
	}
	else {
		usb_uart_switch_state = 101;
		send_usb_insert_event(0);
		spa_event_handler(SPA_EVT_CHARGER, (void *)POWER_SUPPLY_TYPE_BATTERY);
		switch_set_state(&switch_usb_uart,101);
    }
}

static void usb_cdp_attach(uint8_t attached)
{
        printk(attached ? "USB CDP attached\n" : "USB CDP detached\n");
        set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;
        if ( attached ) {
		usb_uart_switch_state = 100;
                send_usb_insert_event(1);
                spa_event_handler(SPA_EVT_CHARGER, (void *)POWER_SUPPLY_TYPE_USB_CDP);
		switch_set_state(&switch_usb_uart,100);
        }
        else {
		usb_uart_switch_state = 101;
                send_usb_insert_event(0);
                spa_event_handler(SPA_EVT_CHARGER, (void *)POWER_SUPPLY_TYPE_BATTERY);
		switch_set_state(&switch_usb_uart,101);
    }
}

static void uart_attach(uint8_t attached)
{
	printk(attached?"UART attached\n":"UART detached\n");
	set_cable_status = CABLE_TYPE_NONE;
	if (attached) {	
		usb_uart_switch_state = 200;
		switch_set_state(&switch_usb_uart, 200);
		KERNEL_LOG = 1;
	}
	else {
		usb_uart_switch_state = 201;
		switch_set_state(&switch_usb_uart, 201);
		KERNEL_LOG = 0;
	}
}

static void charger_attach(uint8_t attached)
{
	printk(attached?"Charger attached\n":"Charger detached\n");
	set_cable_status = attached ? CABLE_TYPE_AC : CABLE_TYPE_NONE;
#ifdef CONFIG_SEC_CHARGING_FEATURE
	if (attached) {
		spa_event_handler(SPA_EVT_CHARGER, (void *)POWER_SUPPLY_TYPE_USB_DCP);
		send_usb_insert_event(0);
	}
	else {
		spa_event_handler(SPA_EVT_CHARGER, (void *)POWER_SUPPLY_TYPE_BATTERY);
	}
#endif

}

static void jig_attach(uint8_t attached,uint8_t factory_mode)
{
	set_cable_status = CABLE_TYPE_NONE;
	switch(factory_mode)
	{
        case RTMUSC_FM_BOOT_OFF_UART:
        INFO("JIG BOOT OFF UART\n");
	uart_attach(attached);
	break;
        case RTMUSC_FM_BOOT_OFF_USB:
        INFO("JIG BOOT OFF USB\n");
	usb_attach(attached);
	break;
        case RTMUSC_FM_BOOT_ON_UART:
        INFO("JIG BOOT ON UART\n");
	uart_attach(attached);
        break;
        case RTMUSC_FM_BOOT_ON_USB:
        INFO("JIG BOOT ON USB\n");
	usb_attach(attached);
	break;
        default:
        ;
	}
	printk(attached?"Jig attached\n":"Jig detached\n");
}

static void over_temperature(uint8_t detected)
{
    printk("over temperature detected = %d!\n",detected);
}
#ifdef CONFIG_RT8969
static void charging_complete(void)
{
    printk("charging complete\n");
}
#endif
static void over_voltage(uint8_t detected)
{
    printk("over voltage = %d\n",(int32_t)detected);
}
static void set_usb_power(uint8_t on)
{
    printk(on?"on resume() : Set USB on\n":"on suspend() : Set USB off\n");
}


static struct rtmus_platform_data __initdata rtmus_pdata = {
    .usb_callback = &usb_attach,
    .usb_cdp_callback = &usb_cdp_attach,
    .uart_callback = &uart_attach,
    .charger_callback = &charger_attach,
    .jig_callback = &jig_attach,
    .over_temperature_callback = &over_temperature,
#ifdef CONFIG_RT8969
    .charging_complete_callback = &charging_complete,
#else
    .charging_complete_callback = NULL,
#endif
    .over_voltage_callback = &over_voltage,
    .usb_power = &set_usb_power,
    .ex_init = rt8973_ex_init,
};
#endif

static ssize_t operating_mode_store(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               const char *buf, size_t len)
{
    uint32_t value = simple_strtoul(buf, NULL, 10);
    struct i2c_client* pClient = pDrvData->client;
    int32_t regCtrl1 = I2CRByte(RT8973_REG_CONTROL_1);
    pDrvData->operating_mode = value;
    if (value)
        regCtrl1 &= (~0x04); // Manual Contrl
    else
        regCtrl1 |= 0x04;// Auto Control
    I2CWByte(RT8973_REG_CONTROL_1,regCtrl1);
    return len;
}

static struct kobj_attribute accessory_attribute = (struct kobj_attribute)__ATTR_RO(accessory);
static struct kobj_attribute chip_id_attribute = (struct kobj_attribute)__ATTR_RO(chip_id);
static struct kobj_attribute driver_version_attribute = (struct kobj_attribute)__ATTR_RO(driver_version);
static struct kobj_attribute usbid_adc_attribute = (struct kobj_attribute)__ATTR_RO(usbid_adc);
static struct kobj_attribute factory_mode_attribute = (struct kobj_attribute)__ATTR_RO(factory_mode);
static struct kobj_attribute operating_mode_attribute = (struct kobj_attribute)__ATTR_RW(operating_mode);

static struct attribute* rt8973_attrs [] =
{
    &accessory_attribute.attr,
    &chip_id_attribute.attr,
    &driver_version_attribute.attr,
    &factory_mode_attribute.attr,
    &usbid_adc_attribute.attr,
    &operating_mode_attribute.attr,
    NULL,
};

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

static ssize_t enable_irq_store(struct kobject *kobj,
                               struct kobj_attribute *attr,
                               const char *buf, size_t len)
{
    uint32_t value = simple_strtoul(buf, NULL, 10);
    INFO("GPIO %d Value = %d\n",CONFIG_RTMUSC_INT_GPIO_NUMBER,gpio_get_value(CONFIG_RTMUSC_INT_GPIO_NUMBER));
    if (value == 1)
        enable_irq(pDrvData->irq);
    else
        disable_irq(pDrvData->irq);
    return len;
}

#define regRO(addr) \
    static ssize_t reg##addr##_show(struct kobject * kobj, struct kobj_attribute * attr, char * buf) \
    { return reg_addr_show(kobj, attr, buf, addr); }

#define regRW(addr) \
    static ssize_t reg##addr##_store(struct kobject * kobj,struct kobj_attribute * attr,const char * buf, size_t len) \
    { return reg_addr_store(kobj,attr,buf,len,addr);} \
    regRO(addr)

regRO(0x01)
regRW(0x02)
regRO(0x03)
regRO(0x04)
regRW(0x05)
regRO(0x07)
regRO(0x0A)
regRO(0x0B)
regRW(0x13)
regRW(0x14)
regRW(0x1B)


static struct kobj_attribute reg0x01_attribute = (struct kobj_attribute)__ATTR_RO(reg0x01);
static struct kobj_attribute reg0x02_attribute = (struct kobj_attribute)__ATTR_RW(reg0x02);
static struct kobj_attribute reg0x03_attribute = (struct kobj_attribute)__ATTR_RO(reg0x03);
static struct kobj_attribute reg0x04_attribute = (struct kobj_attribute)__ATTR_RO(reg0x04);
static struct kobj_attribute reg0x05_attribute = (struct kobj_attribute)__ATTR_RW(reg0x05);
static struct kobj_attribute reg0x07_attribute = (struct kobj_attribute)__ATTR_RO(reg0x07);
static struct kobj_attribute reg0x0A_attribute = (struct kobj_attribute)__ATTR_RO(reg0x0A);
static struct kobj_attribute reg0x0B_attribute = (struct kobj_attribute)__ATTR_RO(reg0x0B);
static struct kobj_attribute reg0x13_attribute = (struct kobj_attribute)__ATTR_RW(reg0x13);
static struct kobj_attribute reg0x14_attribute = (struct kobj_attribute)__ATTR_RW(reg0x14);
static struct kobj_attribute reg0x1B_attribute = (struct kobj_attribute)__ATTR_RW(reg0x1B);

static struct kobj_attribute enable_irq_attribute = (struct kobj_attribute)__ATTR_WO(enable_irq);
static struct attribute* rt8973_dbg_attrs [] =
{
    &reg0x01_attribute.attr,
    &reg0x02_attribute.attr,
    &reg0x03_attribute.attr,
    &reg0x04_attribute.attr,
    &reg0x05_attribute.attr,
    &reg0x07_attribute.attr,
    &reg0x0A_attribute.attr,
    &reg0x0B_attribute.attr,
    &reg0x13_attribute.attr,
    &reg0x14_attribute.attr,
    &reg0x1B_attribute.attr,
    &accessory_attribute.attr,
    &chip_id_attribute.attr,
    &driver_version_attribute.attr,
    &factory_mode_attribute.attr,
    &usbid_adc_attribute.attr,
    &operating_mode_attribute.attr,
    &enable_irq_attribute.attr,
    NULL,
};
static struct attribute_group rt8973_dbg_attrs_group =
{
    .name = "dbg",
    .attrs = rt8973_dbg_attrs,
};
#endif //CONFIG_RT_SYSFS_DBG

static const struct i2c_device_id richtek_musc_id[] =
{
    {"rt8973", 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, richtek_musc_id);
inline int32_t wait_for_interrupt(void)
{
    msleep(RT8973_WAIT_DELAY);
    return gpio_get_value(CONFIG_RTMUSC_INT_GPIO_NUMBER);
}

static void en_int_mask(uint8_t mask)
{
    struct i2c_client* pClient = pDrvData->client;
    int32_t regIntMask;
    regIntMask = I2CRByte(RT8973_REG_INTERRUPT_MASK);
    if (regIntMask<0)
    {
        ERR("Can't read RT8973_REG_INTERRUPT_MASK(0x%x)"
            ", return value = %x",RT8973_REG_INTERRUPT_MASK,regIntMask);
        return;
    }
    I2CWByte(RT8973_REG_INTERRUPT_MASK,regIntMask|mask);
}

static void dis_int_mask(uint8_t mask)
{
    struct i2c_client* pClient = pDrvData->client;
    int32_t regIntMask;
    regIntMask = I2CRByte(RT8973_REG_INTERRUPT_MASK);
    if (regIntMask<0)
    {
        ERR("Can't read RT8973_REG_INTERRUPT_MASK(0x%x)"
            ", return value = %x",RT8973_REG_INTERRUPT_MASK,regIntMask);
        return;
    }
    I2CWByte(RT8973_REG_INTERRUPT_MASK,regIntMask&(~mask));
}

#if EN_REV0_INT_BOUNCE_FIX
void en_adcchg_mask(void)
{
    if (((pDrvData->chip_id & 0xf8)>>3)==0)
    {//REV0
        INFO("Disable ADCCHG & ATTACH event\n");
        en_int_mask(0x61);// ENABLE ADCCHG, ATTACH MASK
    }
}

void dis_adcchg_mask(void)
{
    if (((pDrvData->chip_id & 0xf8)>>3)==0)
    {//REV0
        INFO("Enable ADCCHG & ATTACH event\n");
        dis_int_mask(0x61);// DISABLE ADCCHG, ATTACH MASK
    }
}
#endif
static int enable_interrupt(int32_t enable)
{
    struct i2c_client* pClient = pDrvData->client;
    int32_t regCtrl1;
    regCtrl1 = I2CRByte(RT8973_REG_CONTROL_1);
    if (enable)
        regCtrl1 &= (~0x01);
    else
        regCtrl1 |= 0x01;
    return I2CWByte(RT8973_REG_CONTROL_1,regCtrl1);

}

inline void do_attach_work(int32_t regIntFlag,int32_t regDev1,int32_t regDev2)
{
    int32_t regADC;
    int32_t regCtrl1;
    int32_t regIntFlag2;
    struct i2c_client* pClient = pDrvData->client;
    regIntFlag2 = I2CRByte(RT8973_REG_INT_FLAG2);
    if (regIntFlag&RT8973_INT_DCDTIMEOUT_MASK)
    {
        regADC = I2CRByte(RT8973_REG_ADC) & 0x1f;
        if (regADC == 0x1d || regADC == 0x1c
            || regADC == 0x19 || regADC == 0x18)
            {
                INFO("No VBUS JIG\n");
                switch(regADC)
                {
                case 0x1d: //Factory Mode : JIG UART ON = 1
                if (pDrvData->operating_mode)
                {
                    I2CWByte(RT8973_REG_MANUAL_SW1,0x6c);
                    I2CWByte(RT8973_REG_MANUAL_SW2,0x0c);
                }
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_ON_UART;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_ON_UART);
                break;
                case 0x1c: //Factory Mode : JIG UART OFF = 1
                if (pDrvData->operating_mode)
                {
                    I2CWByte(RT8973_REG_MANUAL_SW1,0x6c);
                    I2CWByte(RT8973_REG_MANUAL_SW2,0x04);
                }
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_OFF_UART;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_OFF_UART);
                break;
                case 0x19: //Factory Mode : JIG USB ON = 1
                if (pDrvData->operating_mode)
                {
                    I2CWByte(RT8973_REG_MANUAL_SW1,0x24);
                    I2CWByte(RT8973_REG_MANUAL_SW2,0x0c);
                }
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_ON_USB;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_ON_USB);
                break;
                case 0x18: //Factory Mode : JIG USB OFF= 1
                if (pDrvData->operating_mode)
                {
                    I2CWByte(RT8973_REG_MANUAL_SW1,0x6c);
                    I2CWByte(RT8973_REG_MANUAL_SW2,0x04);
                }
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_OFF_USB;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_OFF_USB);
                break;
                }
                en_int_mask(0x08);
                return;
            }
        INFO("Redo USB charger detection\n");
        regCtrl1 = I2CRByte(RT8973_REG_CONTROL_1);
        if (regCtrl1<0)
            ERR("I2C read error\n");
        if (DCDT_retry>=MAX_DCDT_retry)
        {
            WARNING("Exceed max DCD_T retry\n");
            en_int_mask(0x08);
            /* > DCD retry
            If UVLO ==0, this accessory should be a charger
            */
            if ((regIntFlag2&0x02)==0)
            {
                INFO("DCD_T retry failed : found VBUS ==> might be dock or unknown charger accessory\n");
                pDrvData->accessory_id = ID_CHARGER;
                if (platform_data.charger_callback)
                    platform_data.charger_callback(RT8973_ATTACHED);
            } else if (pDrvData->accessory_id == ID_CHARGER) // last accessory == CHARGER
            {
                INFO("Charger lost power\n");
                pDrvData->accessory_id = ID_UNKNOW;
                if (platform_data.charger_callback)
                    platform_data.charger_callback(RT8973_DETACHED);
            }
            else
            {
                pDrvData->accessory_id = ID_UNKNOW;
                WARNING("Detected unkown accessory!!\n");
            }
            return;
        }
        I2CWByte(RT8973_REG_CONTROL_1,(uint8_t)(regCtrl1 & 0x9f));
        msleep(RT8973_WAIT_DELAY);
        I2CWByte(RT8973_REG_CONTROL_1,(uint8_t)(regCtrl1 | 0x60));
        DCDT_retry++;
        return;
    }
    if (regIntFlag&RT8973_INT_CHGDET_MASK)
    {
	INFO("regDev1 = 0x%x;regDev2=0x%x\n",regDev1,regDev2);
	if (regDev1&0x20) //0x20
	{
		INFO("USB CDP connected!\n");
		pDrvData->accessory_id = ID_USB_CDP;
		if (platform_data.usb_cdp_callback)
			platform_data.usb_cdp_callback(RT8973_ATTACHED);
		if (pDrvData->operating_mode)
		{
			I2CWByte(RT8973_REG_MANUAL_SW1,0x24);
		}
		return;
	}
	else if (regDev1&0x50) //0x40 / 0x10
	{
		INFO("DCP/TA connected!\n");
		pDrvData->accessory_id = ID_CHARGER;
		if (platform_data.charger_callback)
			platform_data.charger_callback(RT8973_ATTACHED);
		return;
	}
	INFO("Unkown event!!\n");
	return;
    }
    regADC = I2CRByte(RT8973_REG_ADC)&0x1f;
    pDrvData->usbid_adc = regADC;
    if (regIntFlag&RT8973_INT_ADCCHG_MASK)
    {
        if (pDrvData->operating_mode)
        { // Manual Switch
            switch(regADC)
            {
                case 0x16: // UART cable
                I2CWByte(RT8973_REG_MANUAL_SW1,0x6c);
                pDrvData->accessory_id = ID_UART;
                EN_ADCCHG_MASK();
                if (platform_data.uart_callback)
                    platform_data.uart_callback(RT8973_ATTACHED);
                break;
                case 0x1d: //Factory Mode : JIG UART ON = 1
                I2CWByte(RT8973_REG_MANUAL_SW1,0x6c);
                I2CWByte(RT8973_REG_MANUAL_SW2,0x0c);
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_ON_UART;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_ON_UART);
                break;
                case 0x1c: //Factory Mode : JIG UART OFF = 1
                I2CWByte(RT8973_REG_MANUAL_SW1,0x6c);
                I2CWByte(RT8973_REG_MANUAL_SW2,0x04);
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_OFF_UART;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_OFF_UART);
                break;
                case 0x19: //Factory Mode : JIG USB ON = 1
                I2CWByte(RT8973_REG_MANUAL_SW1,0x24);
                I2CWByte(RT8973_REG_MANUAL_SW2,0x0c);
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_ON_USB;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_ON_USB);
                break;
                case 0x18: //Factory Mode : JIG USB OFF= 1
                I2CWByte(RT8973_REG_MANUAL_SW1,0x6c);
                I2CWByte(RT8973_REG_MANUAL_SW2,0x04);
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_OFF_USB;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_OFF_USB);
                break;
                case 0x1a:
                    // desk dock
                if ((regIntFlag2&0x02)==0)
                {
                    INFO("DCD_T retry failed : found VBUS ==> might be dock or unknown charger accessory\n");
                    pDrvData->accessory_id = ID_CHARGER;
                    if (platform_data.charger_callback)
                        platform_data.charger_callback(RT8973_ATTACHED);
                } else if (pDrvData->accessory_id == ID_CHARGER) // last accessory == CHARGER
                {
                    INFO("Charger lost power\n");
                    pDrvData->accessory_id = ID_UNKNOW;
                    if (platform_data.charger_callback)
                        platform_data.charger_callback(RT8973_DETACHED);
                }
                else
                {
                    pDrvData->accessory_id = ID_UNKNOW;
                    WARNING("Detected unkown accessory!!\n");
                }
                break;
                case 0x15:
                    // audio dock or smart hub dock or unknown accessory
                if ((regIntFlag2&0x02)==0)
                {
                    INFO("DCD_T retry failed : found VBUS ==> might be dock or unknown charger accessory\n");
                    pDrvData->accessory_id = ID_CHARGER;
                    if (platform_data.charger_callback)
                        platform_data.charger_callback(RT8973_ATTACHED);
                } else if (pDrvData->accessory_id == ID_CHARGER) // last accessory == CHARGER
                {
                    INFO("Charger lost power\n");
                    pDrvData->accessory_id = ID_UNKNOW;
                    if (platform_data.charger_callback)
                        platform_data.charger_callback(RT8973_DETACHED);
                }
                else
                {
                    pDrvData->accessory_id = ID_UNKNOW;
                    WARNING("Detected unkown accessory!!\n");
                }
                break;
                //Unknow accessory
                case 0x1b:
                case 0x1e:
                pDrvData->accessory_id = ID_UNKNOW;
                WARNING("Detected unkown accessory!!\n");
                break;
                case 0x00:
                 if (regDev1&0x01)
                { // OTG
                    I2CWByte(RT8973_REG_MANUAL_SW1,0x24);
                    I2CWByte(RT8973_REG_MANUAL_SW2,0x01);
                    pDrvData->accessory_id = ID_OTG;
                    if (platform_data.otg_callback)
                        platform_data.otg_callback(1);

                }
                else
                {
                    if (pDrvData->accessory_id == ID_CHARGER) // last accessory == CHARGER
                    {
                        INFO("Charger lost power\n");
                        pDrvData->accessory_id = ID_UNKNOW;
                        if (platform_data.charger_callback)
                            platform_data.charger_callback(RT8973_DETACHED);
                    }
                }
                break;
                case 0x17:
                pDrvData->accessory_id = ID_CHARGER;
                if (platform_data.charger_callback)
                     platform_data.charger_callback(RT8973_ATTACHED);
                break;
                default:
                if (pDrvData->accessory_id == ID_CHARGER) // last accessory == CHARGER
                {
                    INFO("Charger lost power\n");
                    pDrvData->accessory_id = ID_UNKNOW;
                    if (platform_data.charger_callback)
                        platform_data.charger_callback(RT8973_DETACHED);
                }
                pDrvData->accessory_id = ID_UNKNOW;
                WARNING("Unknow USB ID ADC = 0x%x\n",regADC);
            }
        }
        else
        { //automatic switch
            switch(regADC)
            {
                case 0x16: // UART cable
                 // auto switch -- ignore
                INFO("Auto Switch Mode UART cable\n");
                pDrvData->accessory_id = ID_UART;
                EN_ADCCHG_MASK();
                if (platform_data.uart_callback) {
					uart_connecting = 1;
					wake_lock(&pDrvData->uart_wakelock);					
                    platform_data.uart_callback(RT8973_ATTACHED);
				}
                break;
                case 0x1d: //Factory Mode : JIG UART ON = 1
                // auto switch -- ignore
                INFO("Auto Switch Mode JIG UART ON= 1\n");
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_ON_UART;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_ON_UART);
                break;
                case 0x1c: //Factory Mode : JIG UART OFF = 1
                // auto switch -- ignore
                INFO("Auto Switch Mode JIG UART OFF= 1\n");
                pDrvData->accessory_id = ID_JIG_UART;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_OFF_UART;
                EN_ADCCHG_MASK();
		if (platform_data.jig_callback) {
			wake_lock(&pDrvData->uart_wakelock);
			platform_data.jig_callback(1, RTMUSC_FM_BOOT_OFF_UART);
		}
                break;
                case 0x19: //Factory Mode : JIG USB ON = 1
                // auto switch -- ignore
                INFO("Auto Switch Mode JIG USB ON= 1\n");
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_ON_USB;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_ON_USB);
                break;
                case 0x18: //Factory Mode : JIG USB OFF= 1
                // auto switch -- ignore
                INFO("Auto Switch Mode JIG USB OFF= 1\n");
                pDrvData->accessory_id = ID_JIG;
                pDrvData->factory_mode = RTMUSC_FM_BOOT_OFF_USB;
                EN_ADCCHG_MASK();
                if (platform_data.jig_callback)
                    platform_data.jig_callback(1,RTMUSC_FM_BOOT_OFF_USB);
                break;
                case 0x1a:
                if ((regIntFlag2&0x02)==0)
                {
                    INFO("DCD_T retry failed : found VBUS ==> might be dock or unknown charger accessory\n");
                    pDrvData->accessory_id = ID_CHARGER;
                    if (platform_data.charger_callback)
                        platform_data.charger_callback(RT8973_ATTACHED);
                } else if (pDrvData->accessory_id == ID_CHARGER) // last accessory == CHARGER
                {
                    INFO("Charger lost power\n");
                    pDrvData->accessory_id = ID_UNKNOW;
                    if (platform_data.charger_callback)
                        platform_data.charger_callback(RT8973_DETACHED);
                }
                else
                {
                    pDrvData->accessory_id = ID_UNKNOW;
                    WARNING("Detected unkown accessory!!\n");
                }
                break;
                case 0x15:
                if ((regIntFlag2&0x02)==0)
                {
                    INFO("DCD_T retry failed : found VBUS ==> might be dock or unknown charger accessory\n");
                    pDrvData->accessory_id = ID_CHARGER;
                    if (platform_data.charger_callback)
                        platform_data.charger_callback(RT8973_ATTACHED);
                } else if (pDrvData->accessory_id == ID_CHARGER) // last accessory == CHARGER
                {
                    INFO("Charger lost power\n");
                    pDrvData->accessory_id = ID_UNKNOW;
                    if (platform_data.charger_callback)
                        platform_data.charger_callback(RT8973_DETACHED);
                }
                else
                {
                    pDrvData->accessory_id = ID_UNKNOW;
                    WARNING("Detected unkown accessory!!\n");
                }
                break;
                case 0x1b: //Unknow accessory
                case 0x1e:
                pDrvData->accessory_id = ID_UNKNOW;
                WARNING("Detected unkown accessory!!\n");
                break;
                case 0x00:
                if (regDev1&0x01)
                { // OTG
                    regCtrl1 = I2CRByte(RT8973_REG_CONTROL_1); /* in automatic mode, switch to manual mode */
                    regCtrl1 &= (~0x04); // Manual Contrl
                    I2CWByte(RT8973_REG_CONTROL_1,regCtrl1);
                    I2CWByte(RT8973_REG_MANUAL_SW1,0x24);
                    I2CWByte(RT8973_REG_MANUAL_SW2,0x01);
                    pDrvData->accessory_id = ID_OTG;
                    if (platform_data.otg_callback)
                        platform_data.otg_callback(1);

                }
                else
                {
                    if (pDrvData->accessory_id == ID_CHARGER) // last accessory == CHARGER
                    {
                        INFO("Charger lost power\n");
                        pDrvData->accessory_id = ID_UNKNOW;
                        if (platform_data.charger_callback)
                            platform_data.charger_callback(RT8973_DETACHED);
                    }
                }
                break;
                case 0x17:
                pDrvData->accessory_id = ID_CHARGER;
                if (platform_data.charger_callback)
                     platform_data.charger_callback(RT8973_ATTACHED);
                break;
                default:
                WARNING("Unknow USB ID ADC = 0x%x\n",regADC);
                if (pDrvData->accessory_id == ID_CHARGER) // last accessory == CHARGER
                {
                    INFO("Charger lost power\n");
                    pDrvData->accessory_id = ID_UNKNOW;
                    if (platform_data.charger_callback)
                        platform_data.charger_callback(RT8973_DETACHED);
                }
                pDrvData->accessory_id = ID_UNKNOW;
            }
        }
        return;
    }
    if (regDev1&0x04)
    {   //SDPort
        INFO("Standard USB Port connected!\n");
        pDrvData->accessory_id = ID_USB;
        if (platform_data.usb_callback)
            platform_data.usb_callback(RT8973_ATTACHED);
        if (pDrvData->operating_mode)
        {
            I2CWByte(RT8973_REG_MANUAL_SW1,0x24);
        }
        return;
    }
    INFO("Unknown event in Attached Routine\n");
}

inline void do_detach_work(int32_t regIntFlag)
{
    struct i2c_client* pClient = pDrvData->client;
    int32_t regCtrl1;
    if (regIntFlag&0x40)
    {
        if (pDrvData->operating_mode)
        {
            I2CWByte(RT8973_REG_MANUAL_SW1,0);
            I2CWByte(RT8973_REG_MANUAL_SW2,0);
        }

    }
    switch(pDrvData->accessory_id)
    {
        case ID_USB:
        if (platform_data.usb_callback)
            platform_data.usb_callback(RT8973_DETACHED);
        break;
	case ID_USB_CDP:
	if (platform_data.usb_cdp_callback)
	platform_data.usb_cdp_callback(RT8973_DETACHED);
	break;
        case ID_UART:
        if (platform_data.uart_callback) {
            platform_data.uart_callback(RT8973_DETACHED);
			uart_connecting = 0;
			wake_unlock(&pDrvData->uart_wakelock);	
	}
        break;
        case ID_JIG:
        if (platform_data.jig_callback) {
            platform_data.jig_callback(RT8973_DETACHED,pDrvData->factory_mode);
        }
        break;
        case ID_JIG_UART:
        if (platform_data.jig_callback) {
            platform_data.jig_callback(RT8973_DETACHED,pDrvData->factory_mode);
	    wake_unlock(&pDrvData->uart_wakelock);
	}
        break;
        case ID_CHARGER:
        if (platform_data.charger_callback)
            platform_data.charger_callback(RT8973_DETACHED);
        break;
        case ID_OTG:
        if (platform_data.otg_callback)
            platform_data.otg_callback(0);
        if (pDrvData->operating_mode==0)
        {
            regCtrl1 = I2CRByte(RT8973_REG_CONTROL_1);
            regCtrl1 |= 0x04; // Automatically Contrl
            I2CWByte(RT8973_REG_CONTROL_1,regCtrl1);
        }
	break;
        default:
        INFO("Unknown accessory detach\n");
        ;
    }
    pDrvData->accessory_id = ID_NONE;
    pDrvData->factory_mode = RTMUSC_FM_NONE;
    DCDT_retry = 0;
    I2CWByte(RT8973_REG_INTERRUPT_MASK,0x0);
    if (pDrvData->operating_mode)
    {
        I2CWByte(RT8973_REG_MANUAL_SW1,0);
        I2CWByte(RT8973_REG_MANUAL_SW2,0);
    }
}

static irqreturn_t rt8973musc_irq_handler(int irq, void *data);


static void rt8973musc_work(struct work_struct *work)
{
    int32_t regIntFlag;
    int32_t regDev1,regDev2;
    struct i2c_client* pClient = pDrvData->client;
    struct rtmus_platform_data *pdata = &platform_data;

    regIntFlag = I2CRByte(RT8973_REG_INT_FLAG);
    INFO("Interrupt Flag = 0x%x\n",regIntFlag);
    if (regIntFlag&RT8973_INT_ATTACH_MASK)
    {
        regDev1 = I2CRByte(RT8973_REG_DEVICE_1);
        regDev2 = I2CRByte(RT8973_REG_DEVICE_2);
        if (unlikely(regIntFlag&RT8973_INT_DETACH_MASK))
        {
            INFO("There is un-handled event!!\n");
            if (regDev1==0 && regDev2==0)
                do_detach_work(regIntFlag);
            else
                do_attach_work(regIntFlag,regDev1,regDev2);
        }
        else
        {
                do_attach_work(regIntFlag,regDev1,regDev2);
        }
    }
    else if (regIntFlag&RT8973_INT_DETACH_MASK)
    {
        do_detach_work(regIntFlag);
    }
    else
    {
        if (regIntFlag&0x80) // OTP
        {
            INFO("Warning : over temperature voltage\n");
            if (likely(pdata->over_temperature_callback))
                pdata->over_temperature_callback(1);
        }
        else if (regIntFlag&0x10) // OVP
        {
            INFO("Warning : VBUS over voltage\n");
            if (pdata->over_voltage_callback)
                pdata->over_voltage_callback(1);
        }
        else if (regIntFlag&0x08)//DCT =1 & attach = 0
        {
            INFO("only DCT = 1, ignore this event\n");
        }
        else if ((regIntFlag&0x20)!=(pDrvData->prev_int_flag&0x20))
        {
            INFO("triggered by connect = %d\n",(regIntFlag&0x20)?1:0);
        }
        else
        {
            if (pDrvData->prev_int_flag&0x80)
            {
                if (likely(pdata->over_temperature_callback))
                    pdata->over_temperature_callback(0);
            }
            else if (likely(pDrvData->prev_int_flag&0x10)) // OVP
            {
                if (pdata->over_voltage_callback)
                    pdata->over_voltage_callback(0);
            }
            else
            {
                INFO("Unknow event\n");
            }
        }
    }

    pDrvData->prev_int_flag = regIntFlag;
#if RT8973_IRQF_MODE == IRQF_TRIGGER_LOW // modify for samsung ivory
    enable_irq(pDrvData->irq);
#endif

}

static irqreturn_t rt8973musc_irq_handler(int irq, void *data)
{
	struct rt8973_data *pData = (struct rt8973_data*)data;
	wake_lock_timeout(&(pData->muic_wake_lock), 1*HZ);
#if RT8973_IRQF_MODE == IRQF_TRIGGER_LOW  // modify for samsung ivory
	disable_irq_nosync(irq);
#endif
	INFO("RT8973 interrupt triggered!\n");
	queue_work(rtmus_work_queue,&pData->work);
	return IRQ_HANDLED;
}

static bool init_reg_setting(void)
{
    struct i2c_client* pClient = pDrvData->client;
    int32_t regCtrl1;
    int count = 0;
    INFO("Initialize register setting!!\n");
    pDrvData->chip_id = I2CRByte(RT8973_REG_CHIP_ID);
    INFO("Chip ID is %d \n",pDrvData->chip_id);
    if  (pDrvData->chip_id<0)
    {
        ERR("I2C read error(reture %d)\n",pDrvData->chip_id);
        return false;
    }
    if ((pDrvData->chip_id&0x3)!=0x02)
    {
        ERR("Mismatch chip id, reture %d\n",pDrvData->chip_id);
        return false;
    }
    pDrvData->operating_mode = OPERATING_MODE;
    I2CWByte(RT8973_REG_RESET,0x01);
    msleep(RT8973_10M_DELAY);
    regCtrl1 = I2CRByte(RT8973_REG_CONTROL_1);
    INFO("reg_ctrl1 = 0x%x\n",regCtrl1);

    while(1)
    {
	if((regCtrl1!=0xe5 && regCtrl1!=0xc5) && (++count < 10))
	{
	I2CWByte(RT8973_REG_RESET,0x01);
	msleep(RT8973_10M_DELAY);
	regCtrl1 = I2CRByte(RT8973_REG_CONTROL_1);
	INFO("reg_ctrl1 = 0x%x, count = %d\n",regCtrl1,count);
	continue;
	}
	else
	{
	INFO("loop broken\n");
	count = 0;
	break;
	}
    }

    if (regCtrl1!=0xe5 && regCtrl1!=0xc5)
    {
	ERR("Reg Ctrl 1 != 0xE5 or 0xC5\n");
	return false;
    }
    if (pDrvData->operating_mode!=0)
    {
        regCtrl1 &= (~0x04);
        I2CWByte(RT8973_REG_CONTROL_1,regCtrl1);
    }
    pDrvData->prev_int_flag = 0;

    if (((pDrvData->chip_id & 0xf8)>>3)==0)
	{
		INFO("init_reg_setting I2C reset entered\n");
		regCtrl1 |= (0x08);
		I2CWByte(RT8973_REG_CONTROL_1,regCtrl1);
	}
    INFO("prev_int_flag = 0x%x\n",
         pDrvData->prev_int_flag);
    /*
    enable_interrupt(1);
    msleep(RT8973_WAIT_DELAY);
    */
    INFO("Set initial value OK\n");
    /*
    INFO("GPIO %d Value = %d\n",CONFIG_RTMUSC_INT_GPIO_NUMBER,
         gpio_get_value(CONFIG_RTMUSC_INT_GPIO_NUMBER));*/
    return true;
}

static void rt8973_init_func(struct work_struct *work)
{
    int err;
    INFO("rt8973_init_func request IRQ OK...\n");
    err = enable_irq_wake(pDrvData->irq);
    if (err < 0)
    {
        WARNING("enable_irq_wake(%d) failed for (%d)\n",pDrvData->irq, err);
    }
    enable_interrupt(1);
    msleep(RT8973_WAIT_DELAY);
    INFO("Set initial value OK\n");

}

static int rt8973musc_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
    int err;
    struct rt8973_data* drv_data;

    INFO("I2C is probing (%s)%d\n",id->name,(int32_t)id->driver_data);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
    {
        ERR("No Support for I2C_FUNC_SMBUS_BYTE_DATA\n");
        err = -ENODEV;
        goto i2c_check_functionality_fail;
    }

    set_cable_status = CABLE_TYPE_NONE;

    drv_data = kzalloc(sizeof(struct rt8973_data),GFP_KERNEL);
    drv_data->client = client;
        memcpy(&platform_data,&rtmus_pdata,sizeof(struct rtmus_platform_data));

    set_cable_status = CABLE_TYPE_NONE;
    pDrvData = drv_data;
    i2c_set_clientdata(client,drv_data);
    rtmus_work_queue = create_workqueue("rt8973mus_wq");
    INIT_WORK(&drv_data->work, rt8973musc_work);
    INIT_DELAYED_WORK(&drv_data->delayed_work, rt8973_init_func);

	uart_connecting = 0;
    if(platform_data.ex_init)
		platform_data.ex_init();

    schedule_delayed_work(&drv_data->delayed_work, msecs_to_jiffies(2700));

#if CONFIG_RTMUSC_IRQ_NUMBER<0
    client->irq = gpio_to_irq(CONFIG_RTMUSC_INT_GPIO_NUMBER);
#endif
    INFO("RT8973 irq # = %d\n",client->irq);

#ifdef CONFIG_RTMUSC_INT_CONFIG
    INFO("gpio pin # = %d\n",(int)CONFIG_RTMUSC_INT_GPIO_NUMBER);
    err = gpio_request(CONFIG_RTMUSC_INT_GPIO_NUMBER,"RT8973_EINT");         // Request for gpio pin
    if (err<0)
        WARNING("Request GPIO %d failed\n",(int)CONFIG_RTMUSC_INT_GPIO_NUMBER);
    err = gpio_direction_input(CONFIG_RTMUSC_INT_GPIO_NUMBER);
    if (err<0)
        WARNING("Set GPIO Direction to input : failed\n");
#endif // CONFIG_RTMUSC_INT_CONFIG
    pDrvData->irq = client->irq;

    wake_lock_init(&(drv_data->muic_wake_lock), WAKE_LOCK_SUSPEND, "muic_wakelock");
    wake_lock_init(&pDrvData->uart_wakelock,
                        WAKE_LOCK_SUSPEND, "uart-wakelock");
    err = usb_sysfs_init();
    if (err) {
              dev_err(&client->dev,"failed to create tsu6712 attribute group\n");
    }

#if RT8973_IRQF_MODE == IRQF_TRIGGER_LOW
    if (!init_reg_setting())
    {
        err = -EINVAL;
        goto init_fail;
    }
    err = request_irq(client->irq, rt8973musc_irq_handler, RT8973_IRQF_MODE, DEVICE_NAME, drv_data);
    if (err < 0)
    {

        ERR("request_irq(%d) failed for (%d)\n", client->irq, err);
        goto request_irq_fail;
    }
    INFO("request IRQ OK...\n");
    err = enable_irq_wake(client->irq);
    if (err < 0)
    {
        WARNING("enable_irq_wake(%d) failed for (%d)\n",client->irq, err);
    }
#else // modify for samsung ivory
    err = request_irq(client->irq, rt8973musc_irq_handler, RT8973_IRQF_MODE, DEVICE_NAME, drv_data);
    if (err < 0)
    {
        ERR("request_irq(%d) failed for (%d)\n", client->irq, err);
        goto request_irq_fail;
    }
    INFO("request IRQ OK...\n");
    /*err = enable_irq_wake(client->irq);
    if (err < 0)
    {
        WARNING("enable_irq_wake(%d) failed for (%d)\n",client->irq, err);
    }*/
    if (!init_reg_setting())
    {
        disable_irq(client->irq);
        free_irq(client->irq,drv_data);
        err = -EINVAL;
        goto init_fail;
    }
#endif
    pm_runtime_set_active(&client->dev);
    return 0;

request_irq_fail:
init_fail:
    wake_lock_destroy(&(drv_data->muic_wake_lock));
    wake_lock_destroy(&(pDrvData->uart_wakelock));
    cancel_delayed_work(&drv_data->delayed_work);
#ifdef CONFIG_RTMUSC_INT_CONFIG
    gpio_free(CONFIG_RTMUSC_INT_GPIO_NUMBER);
    destroy_workqueue(rtmus_work_queue);
    kfree(pDrvData);
    pDrvData = NULL;
#endif
i2c_check_functionality_fail:
    return err;
}

static int rt8973musc_remove(struct i2c_client *client)
{
    if (pDrvData)
    {
        disable_irq(pDrvData->irq);
        free_irq(pDrvData->irq,pDrvData);
#if CONFIG_RTMUSC_INT_CONFIG
		gpio_free(CONFIG_RTMUSC_INT_GPIO_NUMBER);
#endif // CONFIG_RTMUSC_INT_CONFIG
        wake_lock_destroy(&(pDrvData->muic_wake_lock));
        if (rtmus_work_queue)
            destroy_workqueue(rtmus_work_queue);
        kfree(pDrvData);
        wake_lock_destroy(&pDrvData->uart_wakelock);
        sysfs_remove_group(&client->dev.kobj, &tsu6712_group);
		kobject_put(usb_kobj);
        pDrvData = NULL;
    }
    return 0;
}

#ifdef CONFIG_PM

static void rt8973musc_shutdown(struct i2c_client *client)
{
    struct i2c_client *pClient = client;
    I2CWByte(RT8973_REG_RESET,0x01);
}

static int rt8973musc_resume(struct i2c_client *client)
{
	struct rtmus_platform_data *pdata = &platform_data;
	INFO("In rt8973musc_resume\n");
	if((ID_UART == pDrvData->accessory_id) || (ID_JIG_UART == pDrvData->accessory_id)){
                if (!wake_lock_active(&pDrvData->uart_wakelock))
                        wake_lock(&pDrvData->uart_wakelock);
                }
    if (pdata->usb_power)
        pdata->usb_power(1);
	return 0;
}

static int rt8973musc_suspend(struct i2c_client *client, pm_message_t state)
{
    struct rtmus_platform_data *pdata = &platform_data;
	/*if (device_may_wakeup(&client->dev) && client->irq)
		enable_irq_wake(client->irq);*/
    INFO("In rt8973musc_suspend\n");
    if (pdata->usb_power)
        pdata->usb_power(0);

	return 0;
}

#endif // CONFIG_PM

static struct i2c_driver rt8973_i2c_driver =
{
    .driver = {
        .name = RT8973_DRV_NAME,
    },
    .probe = rt8973musc_probe,
    .remove = rt8973musc_remove,
#ifdef CONFIG_PM
    .shutdown = rt8973musc_shutdown,
	.resume = rt8973musc_resume,
	.suspend = rt8973musc_suspend,
#endif // CONFIG_PM
    .id_table = richtek_musc_id,
};

static int __init rt8973_init(void)
{

	int ret;
    ret = i2c_add_driver(&rt8973_i2c_driver);
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
    rtmus_dev = platform_device_alloc(DEVICE_NAME,-1);

    if (!rtmus_dev)
    {
        WARNING("rtmus_dev = NULL\n");
        ret = -ENOMEM;
        goto alloc_device_fail;
    }
    ret = platform_device_add(rtmus_dev);
    if (ret)
    {
        WARNING("platform_device_add() failed!\n");
        goto sysfs_add_device_fail;
    }

    ret = rt_sysfs_create_files(&(rtmus_dev->dev.kobj),rt8973_attrs);
    if (ret)
        goto sysfs_create_fail;
#ifdef CONFIG_RT_SYSFS_DBG
    ret = sysfs_create_group(&(rtmus_dev->dev.kobj), &rt8973_dbg_attrs_group);
    if (ret)
        goto sysfs_create_fail;
#endif //CONFIG_RT_SYSFS_DBG

	INFO("RT8973 Module initialized.\n");
    return 0;
sysfs_create_fail:
    platform_device_put(rtmus_dev);
sysfs_add_device_fail:
    platform_device_unregister(rtmus_dev);
alloc_device_fail:
    i2c_del_driver(&rt8973_i2c_driver);
    ERR("RT8973 Module initialization failed.\n");
    return ret;
}
static void __exit rt8973_exit(void)
{
    if (rtmus_dev)
    {
        platform_device_put(rtmus_dev);
        platform_device_unregister(rtmus_dev);

    }
    i2c_del_driver(&rt8973_i2c_driver);
    INFO("RT8973 Module deinitialized.\n");
}
MODULE_AUTHOR("Patrick Chang <weichung.chang@gmail.com>");
MODULE_DESCRIPTION("Richtek micro USB switch device diver");
MODULE_LICENSE("GPL");
late_initcall(rt8973_init);
module_exit(rt8973_exit);
