 /*
 * drivers/pmic/pmic-tps80032.c
 *
 * TPS80032 PMIC Driver
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

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/atomic.h>
#include <mach/r8a7373.h>
#include <linux/io.h>
#include <mach/common.h>
#include <linux/jiffies.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/pmic/pmic.h>
#include <linux/pmic/pmic-tps80032.h>
#include <linux/hwspinlock.h>
#include <linux/ctype.h>

#ifdef PMIC_DEBUG_ENABLE
#define PMIC_DEBUG_MSG(...) printk(KERN_DEBUG __VA_ARGS__)
#else
#define PMIC_DEBUG_MSG(...) while (0)
#endif

#define PMIC_ERROR_MSG(...) printk(KERN_ERR __VA_ARGS__)
#define I2C_READ i2c_smbus_read_byte_data
#define I2C_WRITE i2c_smbus_write_byte_data
#define I2C_WRITE_BLOCK i2c_smbus_write_i2c_block_data

static struct timer_list bat_timer;
static void tps80032_battery_timer_handler(unsigned long data);
static short BAT_VOLT_THRESHOLD[100];
static int LDO_VOLT_VALUE[24];
static int LDO_STATE_REG[7];
static int LDO_VOLT_REG[7];
static struct input_dev *button_dev;
static u8 key_count;
static u8 clk_state[3] = {0};
static int num_vbat[5];
static int num_volt;
static int state_sleep[7];
static int volt_sleep[7];
static u8 portcr_val_backup[NUM_PORT];
static int output_backup[3];
static u8 portcr_val[NUM_PORT] = {
	GPIO_PULL_OFF | OUTPUT | FUNCTION_0, /*PORT 0*/
	GPIO_PULL_UP | INPUT | FUNCTION_0, /*PORT 28*/
	GPIO_PULL_UP | OUTPUT | FUNCTION_0, /*PORT 35*/
	GPIO_PULL_DOWN | OUTPUT | FUNCTION_0, /*PORT 141*/
	GPIO_PULL_DOWN | FUNCTION_1, /*PORT 202*/
};

struct hwspinlock *r8a73734_hwlock_pmic;

/* Define for interrupt + suspend flag */
unsigned long suspend_flag;
unsigned long interrupt_flag;

static wait_queue_head_t tps80032_modem_reset_event;
static struct task_struct *tps80032_modem_reset_thread;
static atomic_t modem_reset_handing = ATOMIC_INIT(0);
static void tps80032_chrg_ctrl_work(void);
static void tps80032_int_chrg_work(void);
static void tps80032_vac_charger_work(void);
static void tps80032_interrupt_work(void);
static void tps80032_ext_work(void);
static struct class *power_ctrl_class;
static struct device_type power_ctrl_dev_type;

#ifdef PMIC_NON_VOLATILE_ENABLE
static struct tps80032_irq_data tps80032_irqs[30];
#else
static struct tps80032_irq_data tps80032_irqs[] = {
	[TPS80032_INT_PWRON]            = TPS80032_IRQ(A, 0),
	[TPS80032_INT_RPWRON]           = TPS80032_IRQ(A, 1),
	[TPS80032_INT_SYS_VLOW]         = TPS80032_IRQ(A, 2),
	[TPS80032_INT_RTC_ALARM]        = TPS80032_IRQ(A, 3),
	[TPS80032_INT_RTC_PERIOD]       = TPS80032_IRQ(A, 4),
	[TPS80032_INT_HOT_DIE]          = TPS80032_IRQ(A, 5),
	[TPS80032_INT_VXX_SHORT]        = TPS80032_IRQ(A, 6),
	[TPS80032_INT_SPDURATION]       = TPS80032_IRQ(A, 7),
	[TPS80032_INT_WATCHDOG]         = TPS80032_IRQ(B, 0),
	[TPS80032_INT_BAT]              = TPS80032_IRQ(B, 1),
	[TPS80032_INT_SIM]              = TPS80032_IRQ(B, 2),
	[TPS80032_INT_MMC]              = TPS80032_IRQ(B, 3),
	[TPS80032_INT_RES]              = TPS80032_IRQ(B, 4),
	[TPS80032_INT_GPADC_RT]         = TPS80032_IRQ(B, 5),
	[TPS80032_INT_GPADC_SW2_EOC]    = TPS80032_IRQ(B, 6),
	[TPS80032_INT_CC_AUTOCAL]       = TPS80032_IRQ(B, 7),
	[TPS80032_INT_ID_WKUP]          = TPS80032_IRQ(C, 0),
	[TPS80032_INT_VBUSS_WKUP]       = TPS80032_IRQ(C, 1),
	[TPS80032_INT_ID]               = TPS80032_IRQ(C, 2),
	[TPS80032_INT_VBUS]             = TPS80032_IRQ(C, 3),
	[TPS80032_INT_CHRG_CTRL]        = TPS80032_IRQ(C, 4),
	[TPS80032_INT_EXT_CHRG]         = TPS80032_IRQ(C, 5),
	[TPS80032_INT_INT_CHRG]         = TPS80032_IRQ(C, 6),
	[TPS80032_INT_RES2]             = TPS80032_IRQ(C, 7),
	[TPS80032_INT_BAT_TEMP_OVRANGE] = TPS80032_IRQ_SEC(C, 4,
				CHRG_CTRL, MBAT_TEMP, BAT_TEMP),
	[TPS80032_INT_BAT_REMOVED]      = TPS80032_IRQ_SEC(C, 4,
			CHRG_CTRL, MBAT_REMOVED, BAT_REMOVED),
	[TPS80032_INT_VBUS_DET]         = TPS80032_IRQ_SEC(C, 4,
				CHRG_CTRL, MVBUS_DET, VBUS_DET),
	[TPS80032_INT_VAC_DET]          = TPS80032_IRQ_SEC(C, 4,
				CHRG_CTRL, MVAC_DET, VAC_DET),
	[TPS80032_INT_FAULT_WDG]        = TPS80032_IRQ_SEC(C, 4,
				CHRG_CTRL, MFAULT_WDG, FAULT_WDG),
	[TPS80032_INT_LINCH_GATED]      = TPS80032_IRQ_SEC(C, 4,
			CHRG_CTRL, MLINCH_GATED, LINCH_GATED),
};
#endif

struct tps80032_data {
	struct device *dev;
	int device;
	int charger;
	int bat_volt;
	int bat_temp;
	int hpa_temp;
	int bat_over_volt;
	int bat_over_temp;
	int bat_capacity;
	int bat_presence;
	int en_charger;
	int cin_limit;
	int vbus_det;
	int vac_det;
	int board_rev;
	u32 irq_en;
	u8 mask_reg[3];
	u8 mask_cache[3];
	u8 cont_int_mask_reg;
	u8 cont_int_en;
	u8 cont_int_mask_cache;
	int rscounter[RESOURCE_COUNTER_MAX];
	struct mutex smps1_lock;
	struct mutex smps2_lock;
	struct mutex smps3_lock;
	struct mutex smps4_lock;
	struct mutex smps5_lock;
	struct mutex ldo1_lock;
	struct mutex ldo2_lock;
	struct mutex ldo3_lock;
	struct mutex ldo4_lock;
	struct mutex ldo5_lock;
	struct mutex ldo6_lock;
	struct mutex ldo7_lock;
	struct mutex force_off_lock;
	struct mutex vbus_lock;
	struct mutex rscounter_lock;
	struct mutex irq_lock;
	spinlock_t pmic_lock;
	struct workqueue_struct *queue;
	struct i2c_client *client_power;
	struct i2c_client *client_battery;
	struct i2c_client *client_dvs;
	struct i2c_client *client_jtag;
	struct work_struct update_work;
	struct work_struct resume_work;
	struct irq_chip irq_chip;
	int irq_base;
};


static struct tps80032_data *data;
/* Declare variable for LDO SysFS control */
struct power_ctrl control_ldo[RESOURCE_POWER_CTRL_PROP_MAX];
static struct device_attribute power_ctrl_attrs[];
static const struct attribute_group *power_ctrl_attr_groups[];
static struct attribute_group power_ctrl_attr_group;
static struct attribute *__power_ctrl_attrs[];

#define POWER_CTRL_ATTR(_name)				\
{							\
	.attr = { .name = #_name },			\
	.show = power_ctrl_show_property,		\
	.store = power_ctrl_store_property,		\
}

static enum power_ctrl_property pmic_power_props[] = {
	POWER_CTRL_PROP_STATE,
	POWER_CTRL_PROP_STATE_SLEEP,
	POWER_CTRL_PROP_VOLTAGE,
	POWER_CTRL_PROP_VOLTAGE_SLEEP,
};

/*
 * power_ctrl_show_property:
 * input: void
 * return: void
 */
static ssize_t power_ctrl_show_property(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct power_ctrl *pwctrl = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_ctrl_attrs;
	union power_ctrl_propval value;

	static char *state_text[] = {
		"UNKNOWN", "ON", "OFF"
	};

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	ret = pwctrl->get_property(pwctrl, off, &value);

	if (0 > ret) {
		PMIC_ERROR_MSG("Driver has no data for `%s' property\n",
				attr->attr.name);
		return ret;
	}

	if ((POWER_CTRL_PROP_STATE == off)
		|| (POWER_CTRL_PROP_STATE_SLEEP == off))
		return sprintf(buf, "%s\n", state_text[value.intval]);

	if (POWER_CTRL_PROP_VOLTAGE == off)
		return sprintf(buf, "%dmV\n", value.intval);

	if ((POWER_CTRL_PROP_VOLTAGE_SLEEP == off)
		&& (0 != value.intval))
		return sprintf(buf, "%dmV\n", value.intval);
	else if (POWER_CTRL_PROP_VOLTAGE_SLEEP == off)
		return sprintf(buf, "%s\n", state_text[value.intval]);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return sprintf(buf, "%d\n", value.intval);
}

/*
 * power_ctrl_store_property:
 */
static ssize_t power_ctrl_store_property(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret = 0;
	struct power_ctrl *pwctrl = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_ctrl_attrs;
	union power_ctrl_propval value;
	long long_val;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = kstrtol(buf, 10, &long_val);
	if (ret < 0)
		return ret;

	value.intval = long_val;

	ret = pwctrl->set_property(pwctrl, off, &value);
	if (ret < 0)
		return ret;


	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return count;
}

/*
 * power_ctrl_attr_is_visible:
 */
static mode_t power_ctrl_attr_is_visible(struct kobject *kobj,
					struct attribute *attr, int attrno)

{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct power_ctrl *pwctrl = dev_get_drvdata(dev);
	mode_t mode = S_IRUSR | S_IRGRP | S_IROTH;
	int i;
	int property;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	for (i = 0; i < pwctrl->num_properties; i++) {
		property = pwctrl->properties[i];

		if (property == attrno) {
			if (pwctrl->property_is_writeable &&
			pwctrl->property_is_writeable(pwctrl, property) > 0)
				mode |= S_IWUSR;

			return mode;
		}
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;
}

static char *kstruprdup(const char *str, gfp_t gfp)
{
	char *ret, *ustr;

	ustr = ret = kmalloc(strlen(str) + 1, gfp);

	if (!ret)
		return NULL;

	while (*str)
		*ustr++ = toupper(*str++);

	*ustr = 0;

	return ret;
}

/*
 * power_ctrl_uevent: get all ldo information
 * @dev: The device struct.
 * @env: kobj uevent environment.
 * return:
 *        = 0: Normal operation
 */
int power_ctrl_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct device_attribute *attr;
	char *line;
	int ret = 0, j;
	char *prop_buf;
	char *attrname;
	struct power_ctrl *pwctrl = dev_get_drvdata(dev);

	ret = add_uevent_var(env, "POWER_CTRL_NAME=%s", pwctrl->name);
	if (ret)
		return ret;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return -ENOMEM;

	for (j = 0; j < pwctrl->num_properties; j++) {

		attr = &power_ctrl_attrs[pwctrl->properties[j]];

		ret = power_ctrl_show_property(dev, attr, prop_buf);
		if (ret == -ENODEV || ret == -ENODATA) {
			ret = 0;
			continue;
		}

		if (ret < 0)
			goto out;

		line = strchr(prop_buf, '\n');
		if (line)
			*line = 0;

		attrname = kstruprdup(attr->attr.name, GFP_KERNEL);
		if (!attrname) {
			ret = -ENOMEM;
			goto out;
		}

		ret = add_uevent_var(env, "POWER_CTRL_%s=%s",
					attrname, prop_buf);
		kfree(attrname);
		if (ret)
			goto out;
	}

out:
	free_page((unsigned long)prop_buf);

	return ret;
}

/*
 * power_ctrl_dev_release: Release ldo control device
 * @dev: Device structure.
 * return: void
 */
static void power_ctrl_dev_release(struct device *dev)
{
	pr_debug("device: '%s': %s\n", dev_name(dev), __func__);
	kfree(dev);
}

/*
 * power_ctrl_register: register ldo control device
 * @parent: Parent device structure.
 * @lctl: The ldo control structure.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int power_ctrl_register(struct device *parent, struct power_ctrl *lctl)
{
	struct device *dev;
	int rc;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	device_initialize(dev);

	dev->class = power_ctrl_class;
	dev->type = &power_ctrl_dev_type;
	dev->parent = parent;
	dev->release = power_ctrl_dev_release;
	dev_set_drvdata(dev, lctl);
	lctl->dev = dev;

	rc = kobject_set_name(&dev->kobj, "%s", lctl->name);
	if (rc)
		goto kobject_set_name_failed;

	rc = device_add(dev);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return rc;

kobject_set_name_failed:
	put_device(dev);
	return rc;
}
/*
 * power_ctrl_unregister: deregister ldo control device
 * @lctl: The ldo control structure.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static void power_ctrl_unregister(struct power_ctrl *lctl)
{
	device_unregister(lctl->dev);
}


/*
 * tps80032_ldo_get_volt: get the current voltage value of LDO resource
 * input:
 *        ldo: the LDO resource
 * return:
 *        > 0: the current voltage
 */
static int tps80032_ldo_get_volt(int ldo)
{
	int ret = 0;

	/* Read the current state of LDO */
	ret = I2C_READ(data->client_power, LDO_VOLT_REG[ldo-1]);

	if (0 <= ret)
		ret = (ret & (MSK_BIT_0 | MSK_BIT_1 | MSK_BIT_2
					| MSK_BIT_3 | MSK_BIT_4));
	return LDO_VOLT_VALUE[ret - 1];
}

/*
 * tps80032_ldo_set_volt: se new voltage value for LDO resource
 * input: void
 *        ldo: the LDO resource
 * return:
 *        = 0: Success
 *        < 0: Error occurs
 */
static int tps80032_ldo_set_volt(int ldo, int volt)
{
	int ret = 0;
	int n = 0;
	int in_volt = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	n = volt / 100 - 10;
	in_volt = volt - 937 - 99 * n;

	switch (ldo) {
	case 1:
		ret = pmic_set_voltage(E_POWER_VIO_SD, in_volt);
		break;
	case 2:
		ret = pmic_set_voltage(E_POWER_VDIG_RF, in_volt);
		break;
	case 4:
		ret = pmic_set_voltage(E_POWER_VMIPI, in_volt);
		break;
	case 5:
		ret = pmic_set_voltage(E_POWER_VANA_MM, in_volt);
		break;
	case 6:
		ret = pmic_set_voltage(E_POWER_VMMC, in_volt);
		break;
	case 7:
		ret = pmic_set_voltage(E_POWER_VUSIM1, in_volt);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_ldo_get_state: get the current state of LDO resource
 * input: void
 *        ldo: the LDO resource
 * return:
 *        = 1: State ON
 *        = 2: State OFF
 *        = 0: State UNKNOWN
 */
static int tps80032_ldo_get_state(int ldo)
{
	int ret = 0;

	/* Read the current voltage of LDO */
	ret = I2C_READ(data->client_power, LDO_STATE_REG[ldo-1]);

	if (0 > ret)
		return 0;
	if (0 == (ret & (MSK_BIT_0 | MSK_BIT_1)))
		return 2;
	else
		return 1;
}

/*
 * tps80032_ldo_set_state: set the new state for LDO resource
 * input: void
 *        ldo: the LDO resource
 * return:
 *        = 0: Success
 *        < 0: Error occurs
 */
static int tps80032_ldo_set_state(int ldo, int state)
{
	int ret = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	switch (ldo) {
	case 1:
		if (0 == state)
			ret = pmic_set_power_off(E_POWER_VIO_SD);
		else
			ret = pmic_set_power_on(E_POWER_VIO_SD);
		break;
	case 5:
		if (0 == state)
			ret = pmic_set_power_off(E_POWER_VANA_MM);
		else
			ret = pmic_set_power_on(E_POWER_VANA_MM);
		break;
	case 6:
		if (0 == state)
			ret = pmic_set_power_off(E_POWER_VMMC);
		else
			ret = pmic_set_power_on(E_POWER_VMMC);
		break;
	case 7:
		if (0 == state)
			ret = pmic_set_power_off(E_POWER_VUSIM1);
		else
			ret = pmic_set_power_on(E_POWER_VUSIM1);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_ldo_get_state_sleep: Get state of LDO before entering sleep mode
 * input: void
 * return:
 *        = 1: State ON
 *        = 2: State OFF
 *        = 0: State UNKNOWN
 */
static int tps80032_ldo_get_state_sleep(int ldo)
{
	int ret;

	/* Get the state of LDO before entering sleep mode */
	ret = state_sleep[ldo - 1];

	return ret;
}

/*
 * tps80032_ldo_get_volt_sleep: Get voltage of LDO before entering sleep mode
 * input: void
 * return:
 *        > 0: the current voltage
 */
static int tps80032_ldo_get_volt_sleep(int ldo)
{
	int ret;

	/* Get the voltage of LDO before entering sleep mode */
	ret = volt_sleep[ldo - 1];

	return ret;
}

/*
 * tps80032_power_get_property
 */
static int tps80032_power_get_property(struct power_ctrl *pctl,
				enum power_ctrl_property pcp,
				union power_ctrl_propval *val)
{
	int ldo_ctrl = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	if (0 == (strcmp(pctl->name, "ldo1")))
		ldo_ctrl = 1;
	else if (0 == (strcmp(pctl->name, "ldo2")))
		ldo_ctrl = 2;
	else if (0 == (strcmp(pctl->name, "ldo3")))
		ldo_ctrl = 3;
	else if (0 == (strcmp(pctl->name, "ldo4")))
		ldo_ctrl = 4;
	else if (0 == (strcmp(pctl->name, "ldo5")))
		ldo_ctrl = 5;
	else if (0 == (strcmp(pctl->name, "ldo6")))
		ldo_ctrl = 6;
	else if (0 == (strcmp(pctl->name, "ldo7")))
		ldo_ctrl = 7;

	switch (pcp) {
	case POWER_CTRL_PROP_STATE:
		val->intval = tps80032_ldo_get_state(ldo_ctrl);
		break;
	case POWER_CTRL_PROP_STATE_SLEEP:
		val->intval = tps80032_ldo_get_state_sleep(ldo_ctrl);
		break;
	case POWER_CTRL_PROP_VOLTAGE:
		val->intval = tps80032_ldo_get_volt(ldo_ctrl);
		break;
	case POWER_CTRL_PROP_VOLTAGE_SLEEP:
		val->intval = tps80032_ldo_get_volt_sleep(ldo_ctrl);
		break;
	default:
		val->intval = 0;
		break;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;
}

/*
 * tps80032_power_set_property
 */
static int tps80032_power_set_property(struct power_ctrl *pctl,
			enum power_ctrl_property pcp,
			const union power_ctrl_propval *val)
{
	int ret = 0;
	int ldo_ctrl = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	if (0 == (strcmp(pctl->name, "ldo1")))
		ldo_ctrl = 1;
	else if (0 == (strcmp(pctl->name, "ldo2")))
		ldo_ctrl = 2;
	else if (0 == (strcmp(pctl->name, "ldo4")))
		ldo_ctrl = 4;
	else if (0 == (strcmp(pctl->name, "ldo5")))
		ldo_ctrl = 5;
	else if (0 == (strcmp(pctl->name, "ldo6")))
		ldo_ctrl = 6;
	else if (0 == (strcmp(pctl->name, "ldo7")))
		ldo_ctrl = 7;

	switch (pcp) {
	case POWER_CTRL_PROP_STATE:
		if ((val->intval == 0) || (val->intval == 1))
			ret = tps80032_ldo_set_state(ldo_ctrl, val->intval);
		break;
	case POWER_CTRL_PROP_VOLTAGE:
		if ((val->intval > 999) && (val->intval < 3301))
			ret = tps80032_ldo_set_volt(ldo_ctrl, val->intval);
		break;
	default:
		ret = -EPERM;
		break;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_power_is_writeable
 */
static int tps80032_power_is_writeable(struct power_ctrl *pctl,
				enum power_ctrl_property pcp)
{
	int ldo_ctrl = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	if (0 == (strcmp(pctl->name, "ldo1")))
		ldo_ctrl = 1;
	else if (0 == (strcmp(pctl->name, "ldo2")))
		ldo_ctrl = 2;
	else if (0 == (strcmp(pctl->name, "ldo3")))
		ldo_ctrl = 3;
	else if (0 == (strcmp(pctl->name, "ldo4")))
		ldo_ctrl = 4;
	else if (0 == (strcmp(pctl->name, "ldo5")))
		ldo_ctrl = 5;
	else if (0 == (strcmp(pctl->name, "ldo6")))
		ldo_ctrl = 6;
	else if (0 == (strcmp(pctl->name, "ldo7")))
		ldo_ctrl = 7;

	switch (pcp) {
	case POWER_CTRL_PROP_STATE:
		if ((1 == ldo_ctrl) || (5 == ldo_ctrl)
			|| (6 == ldo_ctrl) || (7 == ldo_ctrl))
			return 1;
		else
			return 0;
	case POWER_CTRL_PROP_VOLTAGE:
		if (3 == ldo_ctrl)
			return 0;
		else
			return 1;
	default:
		break;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;
}

/*
 * tps80032_get_hw_sem_timeout() - lock an hwspinlock with timeout limit
 * @hwlock: the hwspinlock to be locked
 * @timeout: timeout value in msecs
 */
static int tps80032_get_hwsem_timeout(struct hwspinlock *hwlock,
		unsigned int time_out)
{
#if 1 /* temp W/A */
	return 1;
#else
	int ret;
	unsigned long expire;

	expire = msecs_to_jiffies(time_out) + jiffies;

	for (;;) {
		/* Try to take the hwspinlock */
		ret = hwspin_trylock_nospin(hwlock);
		if (ret == 0)
			break;

		/*
		 * The lock is already taken, try again
		 */
		if (time_is_before_eq_jiffies(expire))
			return -ETIMEDOUT;

		/*
		 * Wait 1 millisecond for another round
		 */
		mdelay(1);
	}

	return ret;
#endif
}

/*
 * tps80032_force_release_hwsem() - force to release hw semaphore
 * @hwsem_id: Hardware semaphore ID
		0x01: AP Realtime side
 *		0x40: AP System side
 *		0x93: Baseband side
 * return: void
 */
static void tps80032_force_release_hwsem(u8 hwsem_id)
{
	void *ptr;
	u32 value = 0;
	unsigned long expire = msecs_to_jiffies(5) + jiffies;

	/*Check input hwsem_id*/
	switch (hwsem_id) {
	case RT_CPU_SIDE:
	case SYS_CPU_SIDE:
	case BB_CPU_SIDE:
		break;
	default:
		return;
	}

	ptr = ioremap(HPB_SEM_PMICPhys, 4);
	value = ioread32(ptr);
	iounmap(ptr);

	PMIC_ERROR_MSG("%s: ID (0x%x) is using HW semaphore\n", \
			__func__, value >> 24);

	if ((value >> 24) != hwsem_id)
		return;

	/*enable master access*/
	ptr = ioremap(HPB_SEM_MPACCTLPhys, 4);
	for (;;) {

		/* Try to enable master access */
		iowrite32(0xC0000000, ptr);
		value = ioread32(ptr);
		if (value == 0xC0000000) {
			iounmap(ptr);
			break;
		}

		/*
		 * Cannot enable master access, try again
		 */
		if (time_is_before_eq_jiffies(expire)) {
			iounmap(ptr);
			return;
		}

		/*
		 * Wait 50 nanosecond for another round
		 */
		ndelay(50);
	}

	/*Force clear HW sem*/
	expire = msecs_to_jiffies(5) + jiffies;

	ptr = ioremap(HPB_SEM_PMICPhys, 4);
	for (;;) {
		/* Try to force clear HW sem */
		iowrite32(0, ptr);
		value = ioread32(ptr);
		if (value == 0x0) {
			iounmap(ptr);
			PMIC_ERROR_MSG(
		"%s: Force to release HW sem from ID 0x%x) successful\n",
				__func__, hwsem_id);
			break;
		}

		/*
		 * Cannot force clear HW sem, try again
		 */
		if (time_is_before_eq_jiffies(expire)) {
			iounmap(ptr);
			PMIC_ERROR_MSG(
				"%s: Fail to release HW sem from ID (0x%x)\n",
					__func__, hwsem_id);
			break;
		}

		/*
		 * Wait 50 nanosecond for another round
		 */
		ndelay(50);
	}

	/*Disable master access*/
	expire = msecs_to_jiffies(5) + jiffies;
	ptr = ioremap(HPB_SEM_MPACCTLPhys, 4);
	for (;;) {
		/* Try to disable master access */
		iowrite32(0, ptr);
		value = ioread32(ptr);
		if (value == 0x0) {
			iounmap(ptr);
			break;
		}

		/*
		 * Cannot disable master access, try again
		 */
		if (time_is_before_eq_jiffies(expire)) {
			iounmap(ptr);
			return;
		}

		/*
		 * Wait 50 nanosecond for another round
		 */
		ndelay(50);
	}
}

/*
 * tps80032_force_release_swsem() - force to release sw semaphore
 * @swsem_id: Software semaphore ID
		0x01: AP Realtime side
 *		0x40: AP System side
 *		0x93: Baseband side
 * return: void
 */
static void tps80032_force_release_swsem(u8 swsem_id)
{
	u32 lock_id;
	unsigned long expire = msecs_to_jiffies(10) + jiffies;

	/*Check input swsem_id*/
	switch (swsem_id) {
	case RT_CPU_SIDE:
	case SYS_CPU_SIDE:
	case BB_CPU_SIDE:
		break;
	default:
		return;
	}

	/* Check which CPU (Real time or Baseband or System) is using SW sem*/
	lock_id = hwspin_get_lock_id_nospin(r8a73734_hwlock_pmic);

	PMIC_ERROR_MSG("%s: ID (0x%x) is using SW semaphore\n", \
				__func__, lock_id);

	if (lock_id != swsem_id)
		return;

	for (;;) {
		/* Try to force to unlock SW sem*/
#if 0 /* tmp W/A */
		hwspin_unlock_nospin(r8a73734_hwlock_pmic);
#endif

		lock_id = hwspin_get_lock_id_nospin(r8a73734_hwlock_pmic);
		if (lock_id == 0) {
			PMIC_ERROR_MSG(
		"%s: Forcing to release SW sem from ID (0x%x) is successful\n",
				__func__, swsem_id);
			break;
		}

		/*
		 * Cannot force to unlock SW sem, try again
		 */
		if (time_is_before_eq_jiffies(expire)) {
			PMIC_ERROR_MSG(
				"%s: Fail to release HW sem from ID (0x%x)\n",
					__func__, swsem_id);
			return;
		}

		/*
		 * Wait 100 nanosecond for another round
		 */
		ndelay(100);
	}

}

/*
 * tps80032_handle_modem_reset: Handle modem reset
 * return: void
 */
void tps80032_handle_modem_reset()
{
	atomic_set(&modem_reset_handing, 1);
	wake_up_interruptible(&tps80032_modem_reset_event);
}
EXPORT_SYMBOL(tps80032_handle_modem_reset);

/*
 * tps80032_modem_reset: start thread to handle modem reset
 * @ptr:
 * return: 0
 */
static int tps80032_modem_thread(void *ptr)
{
	while (!kthread_should_stop()) {
		wait_event_interruptible(tps80032_modem_reset_event,
					atomic_read(&modem_reset_handing));

		tps80032_force_release_hwsem(BB_CPU_SIDE);
		tps80032_force_release_swsem(BB_CPU_SIDE);

		atomic_set(&modem_reset_handing, 0);
	}
	return 0;
}

/*
 * tps80032_init_timer: inital the battery timer
 * @data: The struct which handles the TPS80032 data.
 * return: void
 */
static void tps80032_init_timer(struct tps80032_data *data)
{
	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	init_timer(&bat_timer);
	bat_timer.function = tps80032_battery_timer_handler;
	bat_timer.data = (unsigned long)data;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
}


/*
 * tps80032_battery_timer_handler: handle the battery timer interrupt
 * @data:
 * return: void
 */
static void tps80032_battery_timer_handler(unsigned long data)
{
	struct tps80032_data *pdata = (struct tps80032_data *)data;
	unsigned long flags;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* Lock spinlock */
	spin_lock_irqsave(&pdata->pmic_lock, flags);
	/* Check flag */
	if (suspend_flag == 1) {
		/* Unlock spinlock */
		spin_unlock_irqrestore(&pdata->pmic_lock, flags);
		/* Do not add timer */
	} else {
		/* Unlock spinlock */
		spin_unlock_irqrestore(&pdata->pmic_lock, flags);
		/* Add timer */
		queue_work(pdata->queue, &pdata->update_work);
		bat_timer.expires =
			jiffies + msecs_to_jiffies(CONST_TIMER_UPDATE);
		add_timer(&bat_timer);
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}

/*
 * tps80032_irq_lock: handle the mutex for interrupt
 * @data: The struct irq_data
 * return: void
 */
static void tps80032_irq_lock(struct irq_data *data)
{
	struct tps80032_data *tps80032 = irq_data_get_irq_chip_data(data);
	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	mutex_lock(&tps80032->irq_lock);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}

/*
 * tps80032_irq_enable: enable interrupt
 * @data: The struct irq_data
 * return: void
 */
static void tps80032_irq_enable(struct irq_data *data)
{
	struct tps80032_data *tps80032 = irq_data_get_irq_chip_data(data);
	unsigned int __irq = data->irq - tps80032->irq_base;
	const struct tps80032_irq_data *irq_data = &tps80032_irqs[__irq];

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (irq_data->is_sec_int) {
		tps80032->cont_int_mask_reg &= ~(1 << irq_data->int_mask_bit);
		tps80032->cont_int_en |= (1 << irq_data->int_mask_bit);
		tps80032->mask_reg[irq_data->mask_reg] &=
					~(1 << irq_data->mask_mask);
		tps80032->irq_en |= (1 << irq_data->parent_int);
	} else
		tps80032->mask_reg[irq_data->mask_reg] &=
					~(1 << irq_data->mask_mask);

	tps80032->irq_en |= (1 << __irq);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

	return;
}

/*
 * tps80032_irq_disable: disable interrupt
 * @data: The struct irq_data
 * return: void
 */
static void tps80032_irq_disable(struct irq_data *data)
{
	struct tps80032_data *tps80032 = irq_data_get_irq_chip_data(data);

	unsigned int __irq = data->irq - tps80032->irq_base;
	const struct tps80032_irq_data *irq_data = &tps80032_irqs[__irq];

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (irq_data->is_sec_int) {
		tps80032->cont_int_mask_reg |= (1 << irq_data->int_mask_bit);
		tps80032->cont_int_en &= ~(1 << irq_data->int_mask_bit);
		if (!tps80032->cont_int_en) {
			tps80032->mask_reg[irq_data->mask_reg] |=
						(1 << irq_data->mask_mask);
			tps80032->irq_en &= ~(1 << irq_data->parent_int);
		}
		tps80032->irq_en &= ~(1 << __irq);
	} else
		tps80032->mask_reg[irq_data->mask_reg] |=
					(1 << irq_data->mask_mask);

	tps80032->irq_en &= ~(1 << __irq);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}

/*
 * tps80032_irq_sync_unlock: unclock mutex for interrupt
 * @data: The struct irq_data
 * return: void
 */
static void tps80032_irq_sync_unlock(struct irq_data *data)
{
	struct tps80032_data *tps80032 = irq_data_get_irq_chip_data(data);
	int i = 0;
	int ret = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	for (i = 0; i < ARRAY_SIZE(tps80032->mask_reg); i++) {
		if (tps80032->mask_reg[i] != tps80032->mask_cache[i]) {
			ret = I2C_WRITE(tps80032->client_battery,
			HW_REG_INT_MSK_LINE_STS_A + i, tps80032->mask_reg[i]);
			if (!WARN_ON(ret)) {
				ret = I2C_WRITE(tps80032->client_battery,
						HW_REG_INT_MSK_STS_A + i,
							tps80032->mask_reg[i]);
				if (!WARN_ON(ret))
					tps80032->mask_cache[i] =
						tps80032->mask_reg[i];
			}
		}
	}

	if (tps80032->cont_int_mask_reg != tps80032->cont_int_mask_cache) {
		ret = I2C_WRITE(tps80032->client_battery,
				HW_REG_CONTROLLER_INT_MASK + i,
					tps80032->cont_int_mask_reg);
		if (!WARN_ON(ret))
			tps80032->cont_int_mask_cache =
						tps80032->cont_int_mask_reg;
	}

	mutex_unlock(&tps80032->irq_lock);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}

/*
 * tps80032_interrupt_work: handle the PMIC interrupt
 * return: void
 */
static void tps80032_interrupt_work(void)
{
	int sts_a = 0;
	int sts_c = 0;
	int ret = 0;
	int i = 0;
	int i2c_try = 1;
	u8 reg[3] = {0};

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	reg[0] = MSK_DISABLE;
	reg[1] = MSK_DISABLE;
	reg[2] = MSK_DISABLE;

	/* Define the interrupt source */
	/* Read status interrupt A */
	do {
		ret = I2C_READ(data->client_battery, HW_REG_INT_STS_A);
		if (0 > ret) {
			i2c_try++;
			msleep_interruptible(1);
		}
	} while ((0 > ret) && (i2c_try <= CONST_I2C_RETRY));

	/* If can NOT read I2C */
	if (CONST_I2C_RETRY < i2c_try)
		PMIC_ERROR_MSG("ERROR: %s can not read interrupt source A\n",
				__func__);

	/* If can read I2C - Update value of interrupt register A */
	if (0 <= ret)
		sts_a = ret & MSK_GET_INT_SRC_A;

	/* Reset value for I2C retry in error case */
	i2c_try = 1;

	/* Read status interrupt C */
	do {
		ret = I2C_READ(data->client_battery, HW_REG_INT_STS_C);
		if (0 > ret) {
			i2c_try++;
			msleep_interruptible(1);
		}
	} while ((0 > ret) && (i2c_try <= CONST_I2C_RETRY));

	/* If can NOT read I2C */
	if (CONST_I2C_RETRY < i2c_try)
		PMIC_ERROR_MSG("ERROR: %s can not read interrupt source C\n",
				__func__);

	/* If can read I2C - Update value of interrupt register C */
	if (0 <= ret)
		sts_c = ret & MSK_GET_INT_SRC_C;

	/* Reset value for I2C retry in error case */
	i2c_try = 1;

	/* Clear interrupt sources */
	do {
		ret = I2C_WRITE_BLOCK(data->client_battery, HW_REG_INT_STS_A,
				3, reg);
		if (0 > ret) {
			i2c_try++;
			msleep_interruptible(1);
		}
	} while ((0 > ret) && (i2c_try <= CONST_I2C_RETRY));

	/* If can NOT write I2C */
	if (CONST_I2C_RETRY < i2c_try)
		PMIC_ERROR_MSG("ERROR: %s can not clear interrupt sources\n",
				__func__);

	/* Reset value for I2C retry in error case */
	i2c_try = 1;

	/* Process interrupt source */
	/* interrupt source relate to RTC */
	if ((0 != (MSK_BIT_3 & sts_a)) || (0 != (MSK_BIT_4 & sts_a)))
		for (i = TPS80032_INT_RTC_ALARM;
					i <= TPS80032_INT_RTC_PERIOD; i++)
			handle_nested_irq(data->irq_base + i);

	/* interrupt source relate to power key */
	if (0 != (MSK_BIT_7 & sts_a)) {
		if (!key_count) {
			key_count = 1;
			input_event(button_dev, EV_KEY, KEY_POWER, key_count);
			input_sync(button_dev);
			PMIC_DEBUG_MSG("Onkey pressed...\n");
		}
	}

	if (0 != (MSK_BIT_0 & sts_a)) {
		key_count = (key_count + 1) % 2;
		input_event(button_dev, EV_KEY, KEY_POWER, key_count);
		input_sync(button_dev);
		PMIC_DEBUG_MSG("Onkey %s...\n",
			key_count == 0 ? "released" : "pressed");
	}

	if (0 != (MSK_BIT_4 & sts_c)) {
		/* interrupt source relate to charge controller */
		tps80032_chrg_ctrl_work();
	}  else if (0 != (MSK_BIT_6 & sts_c)) {
#ifdef PMIC_FUELGAUGE_ENABLE
		/* interrupt source relate to internal charge */
		tps80032_int_chrg_work();
#endif
	} else if ((0 != (MSK_BIT_0 & sts_c)) || (0 != (MSK_BIT_2 & sts_c))) {
		/* interrupt source relate to external device */
		tps80032_ext_work();
		handle_nested_irq(data->irq_base + TPS80032_INT_ID_WKUP);
		handle_nested_irq(data->irq_base + TPS80032_INT_ID);
		/* Clear interrupt for USB ID */
		do {
			ret = I2C_WRITE(data->client_battery,
				HW_REG_USB_ID_INT_LATCH_CLR, (~MSK_DISABLE));
			if (0 > ret) {
				i2c_try++;
				msleep_interruptible(1);
			}
		} while ((0 > ret) && (i2c_try <= CONST_I2C_RETRY));

		/* If can NOT write I2C */
		if (CONST_I2C_RETRY < i2c_try)
			PMIC_ERROR_MSG(
				"ERROR: %s can not clear USB ID interrupt\n",
					__func__);
	}

#ifdef CONFIG_PMIC_BAT_INTERFACE
	/* Notify when hava an interrupt signal */
	pmic_power_supply_changed(E_USB_STATUS_CHANGED
				|E_BATTERY_STATUS_CHANGED);
#endif

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}


/*
 * tps80032_interrupt_handler: Interrupt handler for TPS80032
 * @irq: The interrupt id request
 * @dev_id: The device ID
 * return:
 *        irqreturn_t: the interrupt request handler.
 */
static irqreturn_t tps80032_interrupt_handler(int irq, void *dev_id)
{
	unsigned long flags;

	PMIC_DEBUG_MSG("%s: irq=%d\n", __func__, irq);
	/* Lock spinlock */
	spin_lock_irqsave(&data->pmic_lock, flags);

	/* Check flag */
	if (suspend_flag == 1) {
		/* Unlock spinlock */
		spin_unlock_irqrestore(&data->pmic_lock, flags);
		/* Clear interrupt flag */
		interrupt_flag = 0;
		return IRQ_HANDLED;
	}

	/* Set interrupt flag */
	interrupt_flag = 1;
	/* Unlock spinlock */
	spin_unlock_irqrestore(&data->pmic_lock, flags);

	tps80032_interrupt_work();

	/* Clear interrupt flag */
	spin_lock_irqsave(&data->pmic_lock, flags);
	interrupt_flag = 0;
	/* Unlock spinlock */
	spin_unlock_irqrestore(&data->pmic_lock, flags);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return IRQ_HANDLED;
}

/*
 * tps80032_check_state_valid: check the valid of power state
 * @pstate: The power state.
 * return:
 *			>=0: the valid power state
 *			<0 : error code
 */
static int tps80032_check_state_valid(int pstate)
{
	int ret;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	switch (pstate) {
	case E_POWER_OFF:
		ret = 0x00;
		break;
	case E_POWER_ON:
		ret = 0x01;
		break;
	case E_POWER_SLEEP:
		ret = -ENOTSUPP;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_set_smps4_power_state: change power state for SMPS4
 * @dev: an i2c client
 * @pstate: The power state.
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_smps4_power_state(struct device *dev, int pstate)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	mutex_lock(&data->smps4_lock);
	ret = tps80032_check_state_valid(pstate);
	if (0 > ret)
		goto exit;

	/* pstate now contains value corresponding to register value. */
	pstate = ret;

	ret = I2C_READ(client, HW_REG_SMPS4_CFG_STATE);
	if (0 > ret)
		goto exit;

	if (pstate != (ret & MSK_POWER_STATE)) {
		val = ret & (~MSK_POWER_STATE);
		val |= pstate;
		ret = I2C_WRITE(client, HW_REG_SMPS4_CFG_STATE, val);
	} else {
		ret = 0;	/* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->smps4_lock);
	return ret;
}

/*
 * tps80032_set_ldo1_power_state: change power state for LDO1
 * @dev: an i2c client
 * @pstate: The power state.
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo1_power_state(struct device *dev, int pstate)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	mutex_lock(&data->ldo1_lock);
	ret = tps80032_check_state_valid(pstate);

	if (0 > ret)
		goto exit;

	/* pstate now contains value corresponding to register value. */
	pstate = ret;

	ret = I2C_READ(client, HW_REG_LDO1_CFG_STATE);
	if (0 > ret)
		goto exit;

	if (pstate != (ret & MSK_POWER_STATE)) {
		val = ret & (~MSK_POWER_STATE);
		val |= pstate ;
		ret = I2C_WRITE(client, HW_REG_LDO1_CFG_STATE, val);
	} else {
		ret = 0;	/* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo1_lock);
	return ret;
}

/*
 * tps80032_set_ldo5_power_state: change power state for LDO5
 * @dev: an i2c client
 * @pstate: The power state.
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo5_power_state(struct device *dev, int pstate)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	mutex_lock(&data->ldo5_lock);
	ret = tps80032_check_state_valid(pstate);
	if (0 > ret)
		goto exit;

	/* pstate now contains value corresponding to register value. */
	pstate = ret;

	ret = I2C_READ(client, HW_REG_LDO5_CFG_STATE);
	if (0 > ret)
		goto exit;

	if (pstate != (ret & MSK_POWER_STATE)) {
		val = ret & (~MSK_POWER_STATE);
		val |= pstate;
		ret = I2C_WRITE(client, HW_REG_LDO5_CFG_STATE, val);
	} else {
		ret = 0;	/* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo5_lock);
	return ret;
}

/*
 * tps80032_set_ldo6_power_state: change power state for LDO6
 * @dev: an i2c client
 * @pstate: The power state.
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo6_power_state(struct device *dev, int pstate)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	mutex_lock(&data->ldo6_lock);
	ret = tps80032_check_state_valid(pstate);
	if (0 > ret)
		goto exit;

	/* pstate now contains value corresponding to register value. */
	pstate = ret;

	ret = I2C_READ(client, HW_REG_LDO6_CFG_STATE);
	if (0 > ret)
		goto exit;

	if (pstate != (ret & MSK_POWER_STATE)) {
		val = ret & (~MSK_POWER_STATE);
		val |= pstate;
		ret = I2C_WRITE(client, HW_REG_LDO6_CFG_STATE, val);
	} else {
		ret = 0;	/* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo6_lock);
	return ret;
}

/*
 * tps80032_set_ldo7_power_state: change power state for LDO7
 * @dev: an i2c client
 * @pstate: The power state.
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo7_power_state(struct device *dev, int pstate)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	mutex_lock(&data->ldo5_lock);
	ret = tps80032_check_state_valid(pstate);
	if (0 > ret)
		goto exit;

	/* pstate now contains value corresponding to register value. */
	pstate = ret;

	ret = I2C_READ(client, HW_REG_LDO7_CFG_STATE);
	if (0 > ret)
		goto exit;

	if (pstate != (ret & MSK_POWER_STATE)) {
		val = ret & (~MSK_POWER_STATE);
		val |= pstate;
		ret = I2C_WRITE(client, HW_REG_LDO7_CFG_STATE, val);
	} else {
		ret = 0;	/* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo5_lock);
	return ret;
}


/*
 * tps80032_set_power_on: turn on a power resource
 * @dev: an i2c client
 * @resource: the resource to be turned on
 * return:
 *         =0: normal
 *         <0: error
 */
static int tps80032_set_power_on(struct device *dev, int resource)
{
	int ret = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	mutex_lock(&data->rscounter_lock);
	if ((data->rscounter[resource] == 0)
	   || (RESOURCE_COUNTER_MAX <= resource)) {
		switch (resource) {
		case E_POWER_VCORE:
		case E_POWER_VIO2:
		case E_POWER_VIO1:
		case E_POWER_VCORE_RF:
		case E_POWER_VDIG_RF:
		case E_POWER_VDDR:
		case E_POWER_VMIPI:
		case E_POWER_ALL:
			ret = -ENOTSUPP;
			break;
		case E_POWER_VANA1_RF:
			ret = tps80032_set_smps4_power_state(dev, E_POWER_ON);
			break;
		case E_POWER_VIO_SD:
			ret = tps80032_set_ldo1_power_state(dev, E_POWER_ON);
			break;
		case E_POWER_VANA_MM:
			ret = tps80032_set_ldo5_power_state(dev, E_POWER_ON);
			break;
		case E_POWER_VMMC:
			ret = tps80032_set_ldo6_power_state(dev, E_POWER_ON);
			break;
		case E_POWER_VUSIM1:
			ret = tps80032_set_ldo7_power_state(dev, E_POWER_ON);
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}
	data->rscounter[resource] = data->rscounter[resource] + 1;
	mutex_unlock(&data->rscounter_lock);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}



/*
 * tps80032_set_power_off: turn off a power resource
 * @dev: an i2c client
 * @resource: the resource to be turned off
 * return:
 *         =0: normal
 *         <0: error
 */
static int tps80032_set_power_off(struct device *dev, int resource)
{
	int ret = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	mutex_lock(&data->rscounter_lock);
	if (data->rscounter[resource] > 0)
		data->rscounter[resource] = data->rscounter[resource] - 1;

	if ((data->rscounter[resource] == 0)
	   || (RESOURCE_COUNTER_MAX <= resource)) {
		switch (resource) {
		case E_POWER_VCORE:
		case E_POWER_VIO2:
		case E_POWER_VIO1:
		case E_POWER_VCORE_RF:
		case E_POWER_VDIG_RF:
		case E_POWER_VDDR:
		case E_POWER_VMIPI:
		case E_POWER_ALL:
			ret = -ENOTSUPP;
			break;
		case E_POWER_VANA1_RF:
			ret = tps80032_set_smps4_power_state(dev, E_POWER_OFF);
			break;
		case E_POWER_VIO_SD:
			ret = tps80032_set_ldo1_power_state(dev, E_POWER_OFF);
			break;
		case E_POWER_VANA_MM:
			ret = tps80032_set_ldo5_power_state(dev, E_POWER_OFF);
			break;
		case E_POWER_VMMC:
			ret = tps80032_set_ldo6_power_state(dev, E_POWER_OFF);
			break;
		case E_POWER_VUSIM1:
			ret = tps80032_set_ldo7_power_state(dev, E_POWER_OFF);
			break;
		default:
			ret = -EINVAL;
			break;
		}
	}
	mutex_unlock(&data->rscounter_lock);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}


/*
 * tps80032_force_off_hw: force off PMIC hardware, including all power resources
 * @dev: an i2c client
 * return: void
*/
static void tps80032_force_off_hw(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	I2C_WRITE(client, HW_REG_PHOENIX_DEV_ON, MSK_BIT_0);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
}


/*
 * tps80032_force_power_off: force off power resources
 * @dev: an i2c client
 * @resource: the power resource to turn off
 * return:
 *         =0: normal
 *         <0: error
*/
static void tps80032_force_power_off(struct device *dev, int resource)
{
	int count = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* Reset counter of power resource */
	mutex_lock(&data->rscounter_lock);
	if (resource == E_POWER_ALL) {
		while (count < RESOURCE_COUNTER_MAX) {
			data->rscounter[count] = 0;
			count = count + 1;
		}
	} else {
		data->rscounter[resource] = 0;
	}
	mutex_unlock(&data->rscounter_lock);

	switch (resource) {
	case E_POWER_VCORE:
	case E_POWER_VIO2:
	case E_POWER_VIO1:
	case E_POWER_VCORE_RF:
	case E_POWER_VDIG_RF:
	case E_POWER_VDDR:
	case E_POWER_VMIPI:
		break;
	case E_POWER_VANA1_RF:
		(void)tps80032_set_smps4_power_state(dev, E_POWER_OFF);
		break;
	case E_POWER_VIO_SD:
		(void)tps80032_set_ldo1_power_state(dev, E_POWER_OFF);
		break;
	case E_POWER_VANA_MM:
		(void)tps80032_set_ldo5_power_state(dev, E_POWER_OFF);
		break;
	case E_POWER_VMMC:
		(void)tps80032_set_ldo6_power_state(dev, E_POWER_OFF);
		break;
	case E_POWER_VUSIM1:
		(void)tps80032_set_ldo7_power_state(dev, E_POWER_OFF);
		break;
	case E_POWER_ALL:
		tps80032_force_off_hw(dev);
		break;
	default:
		break;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}

/*
 * tps80032_check_ldo_voltage_valid: check valid of voltage for LDO resources
 * @voltage: The setting voltage
 * return:
 *			>=0: the valid voltage
 *			<0 : error code
 */
static int tps80032_check_ldo_voltage_valid(int voltage)
{
	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if ((voltage >= E_SMPS_VOLTAGE_0_0000V)
	   && (voltage <= E_SMPS_VOLTAGE_2_1000V))
		return -ENOTSUPP;

	if ((voltage < E_SMPS_VOLTAGE_0_0000V)
	   || (voltage > E_LDO_VOLTAGE_3_3000V))
		return -EINVAL;

	voltage -= E_SMPS_VOLTAGE_2_1000V;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return voltage;
}


/*
 * tps80032_set_ldo1_voltage: set voltage for LDO1
 * @dev: an i2c client
 * @voltage: The setting voltage
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo1_voltage(struct device *dev, int voltage)
{
	int val;
	int bit;
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	bit = tps80032_check_ldo_voltage_valid(voltage);
	if (-EINVAL == bit)
		return -EINVAL;

	if (-ENOTSUPP == bit)
		return -ENOTSUPP;

	mutex_lock(&data->ldo1_lock);
	ret = I2C_READ(client, HW_REG_LDO1_CFG_VOLTAGE);
	if (0 > ret)
		goto exit;

	if (bit != (ret & MSK_POWER_VOLTAGE)) {
		val = ret & ~MSK_POWER_VOLTAGE;
		val |= bit;
		ret = I2C_WRITE(client, HW_REG_LDO1_CFG_VOLTAGE, val);
	} else {
		ret = 0; /* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo1_lock);
	return ret;
}

/*
 * tps80032_set_ldo2_voltage: set voltage for LDO2
 * @dev: an i2c client
 * @voltage: The setting voltage
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo2_voltage(struct device *dev, int voltage)
{
	int val;
	int bit;
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	bit = tps80032_check_ldo_voltage_valid(voltage);
	if (-EINVAL == bit)
		return -EINVAL;

	if (-ENOTSUPP == bit)
		return -ENOTSUPP;

	mutex_lock(&data->ldo2_lock);
	ret = I2C_READ(client, HW_REG_LDO2_CFG_VOLTAGE);
	if (0 > ret)
		goto exit;

	if (bit != (ret & MSK_POWER_VOLTAGE)) {
		val = ret & ~MSK_POWER_VOLTAGE;
		val |= bit;
		ret = I2C_WRITE(client, HW_REG_LDO2_CFG_VOLTAGE, val);
	} else {
		ret = 0; /* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo2_lock);
	return ret;
}

/*
 * tps80032_set_ldo4_voltage: set voltage for LDO4
 * @dev: an i2c client
 * @voltage: The setting voltage
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo4_voltage(struct device *dev, int voltage)
{
	int val;
	int bit;
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	bit = tps80032_check_ldo_voltage_valid(voltage);
	if (-EINVAL == bit)
		return -EINVAL;

	if (-ENOTSUPP == bit)
		return -ENOTSUPP;

	mutex_lock(&data->ldo4_lock);
	ret = I2C_READ(client, HW_REG_LDO4_CFG_VOLTAGE);
	if (0 > ret)
		goto exit;

	if (bit != (ret & MSK_POWER_VOLTAGE)) {
		val = ret & ~MSK_POWER_VOLTAGE;
		val |= bit;
		ret = I2C_WRITE(client, HW_REG_LDO4_CFG_VOLTAGE, val);
	} else {
		ret = 0; /* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo4_lock);
	return ret;
}

/*
 * tps80032_set_ldo5_voltage: set voltage for LDO5
 * @dev: an i2c client
 * @voltage: The setting voltage
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo5_voltage(struct device *dev, int voltage)
{
	int val;
	int bit;
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	bit = tps80032_check_ldo_voltage_valid(voltage);
	if (bit == -EINVAL)
		return -EINVAL;

	if (-ENOTSUPP == bit)
		return -ENOTSUPP;

	mutex_lock(&data->ldo5_lock);
	ret = I2C_READ(client, HW_REG_LDO5_CFG_VOLTAGE);
	if (0 > ret)
		goto exit;

	if (bit != (ret & MSK_POWER_VOLTAGE)) {
		val = ret & ~MSK_POWER_VOLTAGE;
		val |= bit;
		ret = I2C_WRITE(client, HW_REG_LDO5_CFG_VOLTAGE, val);
	} else {
		ret = 0; /* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo5_lock);
	return ret;
}

/*
 * tps80032_set_ldo6_voltage: set voltage for LDO6
 * @dev: an i2c client
 * @voltage: The setting voltage
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo6_voltage(struct device *dev, int voltage)
{
	int val;
	int bit;
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	bit = tps80032_check_ldo_voltage_valid(voltage);
	if (bit == -EINVAL)
		return -EINVAL;

	if (-ENOTSUPP == bit)
		return -ENOTSUPP;

	mutex_lock(&data->ldo6_lock);
	ret = I2C_READ(client, HW_REG_LDO6_CFG_VOLTAGE);
	if (0 > ret)
		goto exit;

	if (bit != (ret & MSK_POWER_VOLTAGE)) {
		val = ret & ~MSK_POWER_VOLTAGE;
		val |= bit;
		ret = I2C_WRITE(client, HW_REG_LDO6_CFG_VOLTAGE, val);
	} else {
		ret = 0; /* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo6_lock);
	return ret;
}

/*
 * tps80032_set_ldo7_voltage: set voltage for LDO7
 * @dev: an i2c client
 * @voltage: The setting voltage
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo7_voltage(struct device *dev, int voltage)
{
	int val;
	int bit;
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	bit = tps80032_check_ldo_voltage_valid(voltage);
	if (bit == -EINVAL)
		return -EINVAL;

	if (-ENOTSUPP == bit)
		return -ENOTSUPP;

	mutex_lock(&data->ldo7_lock);
	ret = I2C_READ(client, HW_REG_LDO7_CFG_VOLTAGE);
	if (0 > ret)
		goto exit;

	if (bit != (ret & MSK_POWER_VOLTAGE)) {
		val = ret & ~MSK_POWER_VOLTAGE;
		val |= bit;
		ret = I2C_WRITE(client, HW_REG_LDO7_CFG_VOLTAGE, val);
	} else {
		ret = 0; /* Nothing to do */
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo7_lock);
	return ret;
}



/*
 * tps80032_set_voltage: set voltage to a power resource
 * @dev: an i2c client
 * @resource: the resource to be set
 * @voltage: the specified voltage
 * return:
 *         =0: normal
 *         <0: error
 */
static int tps80032_set_voltage(struct device *dev, int resource, int voltage)
{
	int ret;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	switch (resource) {
	case E_POWER_VCORE:
	case E_POWER_VIO2:
	case E_POWER_VIO1:
	case E_POWER_VANA1_RF:
	case E_POWER_VCORE_RF:
	case E_POWER_ALL:
	case E_POWER_VDDR:
		ret = -ENOTSUPP;
		break;
	case E_POWER_VIO_SD: /* LDO1 */
		ret = tps80032_set_ldo1_voltage(dev, voltage);
		break;
	case E_POWER_VDIG_RF: /* LDO2 */
		ret = tps80032_set_ldo2_voltage(dev, voltage);
		break;
	case E_POWER_VMIPI:   /* LDO4 */
		ret = tps80032_set_ldo4_voltage(dev, voltage);
		break;
	case E_POWER_VANA_MM: /* LDO5 */
		ret = tps80032_set_ldo5_voltage(dev, voltage);
		break;
	case E_POWER_VMMC:    /* LDO6 */
		ret = tps80032_set_ldo6_voltage(dev, voltage);
		break;
	case E_POWER_VUSIM1:  /* LDO7 */
		ret = tps80032_set_ldo7_voltage(dev, voltage);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}


/*
 * tps80032_set_smps1_power_mode: change power mode for SMPS1
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_smps1_power_mode(struct device *dev,
		int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->smps1_lock);
	ret = I2C_READ(client, HW_REG_SMPS1_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_FORCE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;

	case E_LDO_PMODE_AMS:
	case E_LDO_PMODE_ACTIVE:
		ret = -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_SMPS1_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->smps1_lock);
	return ret;
}

/*
 * tps80032_set_smps2_power_mode: change power mode for SMPS2
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_smps2_power_mode(struct device *dev,
		int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->smps2_lock);
	ret = I2C_READ(client, HW_REG_SMPS2_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_FORCE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
	case E_LDO_PMODE_ACTIVE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_SMPS2_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->smps2_lock);
	return ret;
}

/*
 * tps80032_set_smps3_power_mode: change power mode for SMPS3
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_smps3_power_mode(struct device *dev,
		int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->smps3_lock);
	ret = I2C_READ(client, HW_REG_SMPS3_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_FORCE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
	case E_LDO_PMODE_ACTIVE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_SMPS3_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->smps3_lock);
	return ret;
}

/*
 * tps80032_set_smps4_power_mode: change power mode for SMPS4
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_smps4_power_mode(struct device *dev,
		int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->smps4_lock);
	ret = I2C_READ(client, HW_REG_SMPS4_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_FORCE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
	case E_LDO_PMODE_ACTIVE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_SMPS4_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->smps4_lock);
	return ret;
}

/*
 * tps80032_set_smps5_power_mode: change power mode for SMPS5
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_smps5_power_mode(struct device *dev,
		int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->smps5_lock);
	ret = I2C_READ(client, HW_REG_SMPS5_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_FORCE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
	case E_LDO_PMODE_ACTIVE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_SMPS5_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->smps5_lock);
	return ret;
}

/*
 * tps80032_set_ldo1_power_mode: change power mode for LDO1
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo1_power_mode(struct device *dev,
		int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->ldo1_lock);
	ret = I2C_READ(client, HW_REG_LDO1_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_ACTIVE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
	case E_SMPS_PMODE_FORCE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_LDO1_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo1_lock);
	return ret;
}

/*
 * tps80032_set_ldo2_power_mode: change power mode for LDO2
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo2_power_mode(struct device *dev,
		int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->ldo2_lock);
	ret = I2C_READ(client, HW_REG_LDO2_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_ACTIVE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
	case E_SMPS_PMODE_FORCE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_LDO2_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo2_lock);
	return ret;
}

/*
 * tps80032_set_ldo3_power_mode: change power mode for LDO3
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo3_power_mode(struct device *dev,
		int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->ldo3_lock);
	ret = I2C_READ(client, HW_REG_LDO3_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_ACTIVE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
	case E_SMPS_PMODE_FORCE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_LDO3_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo3_lock);
	return ret;
}

/*
 * tps80032_set_ldo4_power_mode: change power mode for LDO4
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo4_power_mode(struct device *dev,
		int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->ldo4_lock);
	ret = I2C_READ(client, HW_REG_LDO4_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_ACTIVE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;

	case E_SMPS_PMODE_AUTO:
	case E_SMPS_PMODE_FORCE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_LDO4_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo4_lock);
	return ret;
}

/*
 * tps80032_set_ldo5_power_mode: change power mode for LDO5
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo5_power_mode(struct device *dev,
		int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->ldo5_lock);
	ret = I2C_READ(client, HW_REG_LDO5_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_ACTIVE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
	case E_SMPS_PMODE_FORCE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_LDO5_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo5_lock);
	return ret;
}

/*
 * tps80032_set_ldo6_power_mode: change power mode for LDO6
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo6_power_mode(struct device *dev,
				int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->ldo6_lock);
	ret = I2C_READ(client, HW_REG_LDO6_CFG_TRANS);
	if (0 > ret)
		goto exit;


	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_ACTIVE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
	case E_SMPS_PMODE_FORCE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_LDO6_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo6_lock);
	return ret;
}

/*
 * tps80032_set_ldo7_power_mode: change power mode for LDO7
 * @dev: an i2c client
 * @pstate: The setting state want to change power mode
 * @pmode: The setting mode
 * return:
 *			=0: change successful
 *			<0: change failed
 */
static int tps80032_set_ldo7_power_mode(struct device *dev,
					int pstate, int pmode)
{
	int ret;
	int val;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (E_POWER_OFF == pstate) {
		ret = -ENOTSUPP;
		goto exit;
	}

	mutex_lock(&data->ldo7_lock);
	ret = I2C_READ(client, HW_REG_LDO7_CFG_TRANS);
	if (0 > ret)
		goto exit;

	switch (pmode) {
	case E_PMODE_OFF:
		if (pstate == E_POWER_ON) {
			ret = -ENOTSUPP;
			goto exit;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret & ~MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_AMS:
		if (pstate == E_POWER_ON) {
			val = ret & ~MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			val = ret | MSK_BIT_2;
		} else if (pstate == (E_POWER_ON | E_POWER_SLEEP)) {
			val = ret & ~MSK_BIT_1;
			val = ret | MSK_BIT_2;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_LDO_PMODE_ACTIVE:
		if (pstate == E_POWER_ON) {
			val = ret | MSK_BIT_1;
		} else if (pstate == E_POWER_SLEEP) {
			ret = -ENOTSUPP;
			goto exit;
		} else {
			ret = -EINVAL;
			goto exit;
		}
		break;
	case E_SMPS_PMODE_AUTO:
	case E_SMPS_PMODE_FORCE:
		ret =  -ENOTSUPP;
		goto exit;
		break;
	default:
		ret = -EINVAL;
		goto exit;
	}

	ret = I2C_WRITE(client, HW_REG_LDO7_CFG_TRANS, val);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

exit:
	mutex_unlock(&data->ldo7_lock);
	return ret;
}


/*
 * tps80032_set_power_mode: set power mode to a power resource
 * @dev: an i2c client
 * @resource: the resource to be set
 * @pmode: the specified power mode
 * return:
 *         =0: normal
 *         <0: error
 */
static int tps80032_set_power_mode(struct device *dev, int resource,
					int pstate, int pmode)
{
	int ret;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	switch (resource) {
	case E_POWER_VCORE:     /* SMPS1 */
		ret = tps80032_set_smps1_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VIO2:      /* SMPS2 */
		ret = tps80032_set_smps2_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VIO1:      /* SMPS3 */
		ret = tps80032_set_smps3_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VANA1_RF:  /* SMPS4 */
		ret = tps80032_set_smps4_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VCORE_RF:  /* SMPS5 */
		ret = tps80032_set_smps5_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VIO_SD: /* LDO1 */
		ret = tps80032_set_ldo1_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VDIG_RF: /* LDO2 */
		ret = tps80032_set_ldo2_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VDDR: /* LDO3 */
		ret = tps80032_set_ldo3_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VMIPI:   /* LDO4 */
		ret = tps80032_set_ldo4_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VANA_MM: /* LDO5 */
		ret = tps80032_set_ldo5_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VMMC:    /* LDO6 */
		ret = tps80032_set_ldo6_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_VUSIM1:  /* LDO7 */
		ret = tps80032_set_ldo7_power_mode(dev, pstate, pmode);
		break;
	case E_POWER_ALL:
		ret = -ENOTSUPP;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}


/*
 * tps80032_set_vbus: enable or disable 5V USB OTG VBUS power supply
 * @dev: an i2c client
 * @enable:
 *          enable = 1: enable 5V VBUS power supply
 *          enable = 0: disable 5V VBUS power supply
 * return:
 *         =0: normal
 *         <0: error
 */
static int tps80032_set_vbus(struct device *dev, int enable)
{
	int ret;
	int val_mode = 0;
	int val_dcdc = 0;
	int val_boost = 0;
	int cur_mode;
	int cur_dcdc;
	int cur_boost;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	mutex_lock(&data->vbus_lock);

	if ((0 != enable) && (1 != enable)) {
		ret = -EINVAL;
		goto exit;
	}

	/* read OPA_MODE bit */
	ret = I2C_READ(client, HW_REG_CHARGERUSB_CTRL1);
	if (0 > ret)
		goto exit;

	cur_mode = ret;

	/* read VSYS_SW_CTRL bit */
	ret = I2C_READ(client, HW_REG_CHARGERUSB_VSYSREG);
	if (0 > ret)
		goto exit;

	cur_dcdc = ret;

	/* read BST_HW_PR_DIS bit */
	ret = I2C_READ(client, HW_REG_CHARGERUSB_CTRL3);
	if (0 > ret)
		goto exit;

	cur_boost = ret;

	/* If current mode is boost mode and there's a charger mode request */
	if (0 == enable) {
		/* set charger mode */
		val_mode = cur_mode & ~MSK_BIT_6;
		/* Enable DC-DC Tracking-Mode */
		val_dcdc = cur_dcdc & ~MSK_BIT_7;
		/* Stop boost mode */
		val_boost = cur_boost & ~MSK_BIT_5;
	} else if (1 == enable) {
		/* Set boost mode */
		val_mode = cur_mode | MSK_BIT_6;
		/* Disable DC-DC Tracking-Mode */
		val_dcdc = cur_dcdc | MSK_BIT_7;
		/* Start boost mode */
		val_boost = cur_boost | MSK_BIT_5;
	}

	ret = I2C_WRITE(client, HW_REG_CHARGERUSB_CTRL1, val_mode);
	if (0 > ret)
		goto exit;

	ret = I2C_WRITE(client, HW_REG_CHARGERUSB_VSYSREG, val_dcdc);
	if (0 > ret)
		goto exit;

	ret = I2C_WRITE(client, HW_REG_CHARGERUSB_CTRL3, val_boost);
	if (0 > ret)
		goto exit;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
exit:
	mutex_unlock(&data->vbus_lock);
	return ret;
}

/*
 * tps80032_get_ext_device: get USB devices detected
 * @dev: an i2c client
 * return:
 *         >=0: the connected devices
 *         <0: error
*/
static int tps80032_get_ext_device(struct device *dev)
{
	int device;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (!data)
		device = E_DEVICE_NONE;
	else
		device = data->device;

	PMIC_DEBUG_MSG("%s: device=0x%x\n", __func__, device);
	return device;
}


/*
 * tps80032_check_bat_low_1: check the current capacity is low or critical
 * and notify to PMIC interface
 * @old_cap: The previous battery capacity.
 * @new_cap: The current battery capacity.
 * return: void
 */
static void tps80032_check_bat_low_1(int old_cap, int new_cap)
{
	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (((THR_BAT_NORMAL <= old_cap) && (THR_BAT_NORMAL > new_cap))
		|| ((THR_BAT_LOW <= old_cap) && (THR_BAT_NORMAL > old_cap)
				&& (THR_BAT_LOW > new_cap))) {
			/* Notify the battery change */
			pmic_power_supply_changed(E_BATTERY_STATUS_CHANGED);
	} else {
		/* Do nothing */
		return;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
}


/*
 * tps80032_check_bat_low_2: check the current capacity is low or critical
 * and notify to PMIC interface
 * @old: The old state of USB charger
 * @new: The new data of battery
 * return: void
 */
static void tps80032_check_bat_low_2(int old_charger, struct tps80032_data *new)
{
	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* If the previous state is charging and current state is discharging*/
	if ((1 == old_charger) && (0 == new->charger)
		&& (new->bat_capacity < THR_BAT_NORMAL)) {
		/* Notify the battery low  */
		pmic_power_supply_changed(E_BATTERY_STATUS_CHANGED);
	} else {
		/* Do nothing */
		return;
	}
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
}


/*
 * tps80032_gpadc_correct_temp: correct the battery temperature
 * @temp: the battery temperature
 * return:
 *        > 0: the battery temperature after correct
 */
static int tps80032_gpadc_correct_temp(struct tps80032_data *data, int temp)
{
	int d1, d2;
	int offset, gain;
	int ret_trim1, ret_trim2, ret_trim3, ret_trim4;
	int sign_trim1, sign_trim2;
	int ret = 0;
	int result = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/*HPB lock*/
	ret = tps80032_get_hwsem_timeout(r8a73734_hwlock_pmic, CONST_HPB_WAIT);
	if (ret < 0) {
		PMIC_ERROR_MSG("%s:lock is already taken.\n", __func__);
		return -EBUSY;
	}

	ret_trim1 = I2C_READ(data->client_jtag, HW_REG_GPADC_TRIM1);
	if (0 > ret_trim1) {
		result = ret_trim1;
		goto exit;
	}

	ret_trim2 = I2C_READ(data->client_jtag, HW_REG_GPADC_TRIM2);
	if (0 > ret_trim2) {
		result = ret_trim2;
		goto exit;
	}

	ret_trim3 = I2C_READ(data->client_jtag, HW_REG_GPADC_TRIM3);
	if (0 > ret_trim3) {
		result = ret_trim3;
		goto exit;
	}

	ret_trim4 = I2C_READ(data->client_jtag, HW_REG_GPADC_TRIM4);
	if (0 > ret_trim4) {
		result = ret_trim4;
		goto exit;
	}

	/*HPB unlock*/
#if 0 /* temp W/A */
	hwspin_unlock_nospin(r8a73734_hwlock_pmic);
#endif

	sign_trim1 = ret_trim1 & MSK_BIT_0;
	sign_trim2 = ret_trim2 & MSK_BIT_0;

	ret_trim1 = ret_trim1 & (MSK_BIT_2 | MSK_BIT_1);
	ret_trim2 = ret_trim2 & (MSK_BIT_2 | MSK_BIT_1);
	ret_trim3 = ret_trim3 & (MSK_BIT_4 | MSK_BIT_3 | MSK_BIT_2
					   | MSK_BIT_1 | MSK_BIT_0);

	ret_trim4 = ret_trim4 & (MSK_BIT_5 | MSK_BIT_4 | MSK_BIT_3
					   | MSK_BIT_2 | MSK_BIT_1 | MSK_BIT_0);

	if (0 == sign_trim1) {
		d1 = ret_trim3 * 4 + ret_trim1;
	} else {
		d1 = ret_trim3 * 4 + ret_trim1;
		d1 = -d1;
	}

	if (0 == sign_trim2) {
		d2 = ret_trim4 * 4 + ret_trim2;
	} else {
		d2 = ret_trim4 * 4 + ret_trim2;
		d2 = -d2;
	}

	gain = 1 + ((d2 - d1) / (CONST_X2 - CONST_X1));
	offset = d1 - (gain - 1) * CONST_X1;

	if (0 != gain)
		result = (temp - offset) / gain;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return result;

exit:
	/*HPB unlock*/
#if 0 /* temp W/A */
	hwspin_unlock_nospin(r8a73734_hwlock_pmic);
#endif

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return result;
}

/*
 * tps80032_gpadc_correct_voltage: correct the battery voltage
 * @volt: the battery voltage
 * return:
 *        > 0: the battery voltage after correct
 */
static int tps80032_gpadc_correct_voltage(struct tps80032_data *data, int volt)
{
	int d1, d2;
	int ret_temp1, ret_temp2;
	int ret_trim1, ret_trim2, ret_trim3, ret_trim4, ret_trim5, ret_trim6;
	int sign_trim1, sign_trim2, sign_trim5, sign_trim6;
	int offset, gain;
	int ret = 0;
	int result = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/*HPB lock*/
	ret = tps80032_get_hwsem_timeout(r8a73734_hwlock_pmic, CONST_HPB_WAIT);
	if (ret < 0) {
		PMIC_ERROR_MSG("%s:lock is already taken.\n", __func__);
		return -EBUSY;
	}

	ret_trim1 = I2C_READ(data->client_jtag, HW_REG_GPADC_TRIM1);
	if (0 > ret_trim1) {
		result = ret_trim1;
		goto exit;
	}

	ret_trim2 = I2C_READ(data->client_jtag, HW_REG_GPADC_TRIM2);
	if (0 > ret_trim2) {
		result = ret_trim2;
		goto exit;
	}

	ret_trim3 = I2C_READ(data->client_jtag, HW_REG_GPADC_TRIM3);
	if (0 > ret_trim3) {
		result = ret_trim3;
		goto exit;
	}

	ret_trim4 = I2C_READ(data->client_jtag, HW_REG_GPADC_TRIM4);
	if (0 > ret_trim4) {
		result = ret_trim4;
		goto exit;
	}

	ret_trim5 = I2C_READ(data->client_jtag, HW_REG_GPADC_TRIM5);
	if (0 > ret_trim5) {
		result = ret_trim5;
		goto exit;
	}

	ret_trim6 = I2C_READ(data->client_jtag, HW_REG_GPADC_TRIM6);
	if (0 > ret_trim6) {
		result = ret_trim6;
		goto exit;
	}

	sign_trim1 = ret_trim1 & MSK_BIT_0;
	sign_trim2 = ret_trim2 & MSK_BIT_0;
	sign_trim5 = ret_trim5 & MSK_BIT_0;
	sign_trim6 = ret_trim6 & MSK_BIT_0;

	ret_trim1 = ret_trim1 & (MSK_BIT_2 | MSK_BIT_1);
	ret_trim2 = ret_trim2 & (MSK_BIT_2 | MSK_BIT_1);
	ret_trim3 = ret_trim3 & (MSK_BIT_4 | MSK_BIT_3 | MSK_BIT_2
					   | MSK_BIT_1 | MSK_BIT_0);

	ret_trim4 = ret_trim4 & (MSK_BIT_5 | MSK_BIT_4 | MSK_BIT_3
					   | MSK_BIT_2 | MSK_BIT_1 | MSK_BIT_0);

	ret_trim5 = ret_trim5 & (MSK_BIT_6 | MSK_BIT_5 | MSK_BIT_4
					   | MSK_BIT_3 | MSK_BIT_2 | MSK_BIT_1);

	ret_trim6 = ret_trim6 & (MSK_BIT_7 | MSK_BIT_6 | MSK_BIT_5
					   | MSK_BIT_4 | MSK_BIT_3
					   | MSK_BIT_2 | MSK_BIT_1);

	ret_temp1 = ret_trim3 * 4 + ret_trim1;
	ret_temp2 = ret_trim5;

	if (1 == sign_trim1)
		ret_temp1 = -ret_temp1;

	if (1 == sign_trim5)
		ret_temp2 = -ret_temp2;

	d1 = ret_temp1 + ret_temp2;

	ret_temp1 = ret_trim4 * 4 + ret_trim2;
	ret_temp2 = ret_trim6;

	if (1 == sign_trim2)
		ret_temp1 = -ret_temp1;

	if (1 == sign_trim6)
		ret_temp2 = -ret_temp2;

	d2 = ret_temp1 + ret_temp2;

	gain = 1 + ((d2 - d1) / (CONST_X2 - CONST_X1));
	offset = d1 - (gain - 1) * CONST_X1;

	if (0 != gain)
		result = (volt - offset) / gain;

exit:
	/*HPB unlock*/
#if 0 /* temp W/A */
	hwspin_unlock_nospin(r8a73734_hwlock_pmic);
#endif

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return result;
}

/*
 * tps80032_read_hpa_temp: read the HPA temperature
 * @client: The I2C client device.
 * return:
 *        > 0: Battery temperature
 *        = 0: Error occurs
 */
static int tps80032_read_hpa_temp(struct i2c_client *client)
{
	int result = 0;
	int ret;
	int count_timer = 0;
	int ret_MSB, ret_LSB;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/*HPB lock*/
	ret = tps80032_get_hwsem_timeout(r8a73734_hwlock_pmic, CONST_HPB_WAIT);
	if (ret < 0) {
		PMIC_ERROR_MSG("%s:lock is already taken.\n", __func__);
		return -EBUSY;
	}

	/*Set 5V scaler and other internal ADC reference */
	ret = I2C_WRITE(client, HW_REG_GPADC_CTRL, 0x6B);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Enable GPADC */
	ret = I2C_WRITE(client, HW_REG_TOGGLE1, MSK_GPADC);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Select TEMP measurement channel */
	ret = I2C_WRITE(client, HW_REG_GPSELECT_ISB, 0x04);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	msleep(20);

	/*Start GPADC */
	ret = I2C_WRITE(client, HW_REG_CTRL_P1, 0x08);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Wait for ADC interrupt */
	while (count_timer <= CONST_WAIT_TIME) {
		schedule();

		/* Check ADC interrupt bit */
		ret = I2C_READ(client, HW_REG_CTRL_P1);
		if (0 > ret) {
			result = ret;
			goto disable;
		} else if (0 != (ret & MSK_BIT_1)) {
			/* Conversion finished */
			break;
		} else {
			count_timer++;
			/* Do nothing */
		}
	}

	if (CONST_WAIT_TIME < count_timer) {
		/* Time out */
		PMIC_DEBUG_MSG("%s: measurement conversion failed\n", __func__);
		result =  -1;
		goto disable;
	}

	/*Read the VBAT conversion result */
	ret = I2C_READ(client, HW_REG_GPCH0_MSB);
	if (0 > ret) {
		result = ret;
		goto disable;
	} else {
		ret_MSB = ret;
	}

	if (0x10 == ret_MSB) {
		PMIC_ERROR_MSG("%s: collision occurs.", __func__);
		result = -EAGAIN;
		goto exit;
	}

	ret = I2C_READ(client, HW_REG_GPCH0_LSB);
	if (0 > ret) {
		result = ret;
		goto disable;
	} else {
		ret_LSB = ret;
	}

	/*Correct the result */
	result = ((ret_MSB & 0x0F)<<8) | ret_LSB;

disable:
	/*Disable GPADC */
	I2C_WRITE(client, HW_REG_TOGGLE1, 0x01);
	I2C_WRITE(client, HW_REG_GPADC_CTRL, 0x00);
	I2C_WRITE(client, HW_REG_GPADC_CTRL2, 0x00);

exit:
	/*HPB unlock*/
#if 0 /* temp W/A */
	hwspin_unlock_nospin(r8a73734_hwlock_pmic);
#endif

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return result;
}

/*
 * tps80032_read_bat_temp: read the battery temperature
 * @client: The I2C client device.
 * return:
 *        > 0: Battery temperature
 *        = 0: Error occurs
 */
static int tps80032_read_bat_temp(struct i2c_client *client)
{
	int result = 0;
	int ret;
	int count_timer = 0;
	int ret_MSB, ret_LSB;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/*HPB lock*/
	ret = tps80032_get_hwsem_timeout(r8a73734_hwlock_pmic, CONST_HPB_WAIT);
	if (ret < 0) {
		PMIC_ERROR_MSG("%s:lock is already taken.\n", __func__);
		return -EBUSY;
	}

	/*Set 5V scaler and other internal ADC reference */
	ret = I2C_WRITE(client, HW_REG_GPADC_CTRL, 0x6B);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Enable GPADC */
	ret = I2C_WRITE(client, HW_REG_TOGGLE1, MSK_GPADC);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Select TEMP measurement channel */
	ret = I2C_WRITE(client, HW_REG_GPSELECT_ISB, 0x01);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	msleep(20);

	/*Start GPADC */
	ret = I2C_WRITE(client, HW_REG_CTRL_P1, 0x08);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Wait for ADC interrupt */
	while (count_timer <= CONST_WAIT_TIME) {
		schedule();

		/* Check ADC interrupt bit */
		ret = I2C_READ(client, HW_REG_CTRL_P1);
		if (0 > ret) {
			result = ret;
			goto disable;
		} else if (0 != (ret & MSK_BIT_1)) {
			/* Conversion finished */
			break;
		} else {
			count_timer++;
			/* Do nothing */
		}
	}

	if (CONST_WAIT_TIME < count_timer) {
		/* Time out */
		PMIC_DEBUG_MSG("%s: measurement conversion failed\n", __func__);
		result =  -1;
		goto disable;
	}

	/*Read the VBAT conversion result */
	ret = I2C_READ(client, HW_REG_GPCH0_MSB);
	if (0 > ret) {
		result = ret;
		goto disable;
	} else {
		ret_MSB = ret;
	}

	if (0x10 == ret_MSB) {
		PMIC_ERROR_MSG("%s: collision occurs.", __func__);
		result = -EAGAIN;
		goto exit;
	}

	ret = I2C_READ(client, HW_REG_GPCH0_LSB);
	if (0 > ret) {
		result = ret;
		goto disable;
	} else {
		ret_LSB = ret;
	}

	/*Correct the result */
	result = ((ret_MSB & 0x0F)<<8) | ret_LSB;

disable:
	/*Disable GPADC */
	I2C_WRITE(client, HW_REG_TOGGLE1, 0x01);
	I2C_WRITE(client, HW_REG_GPADC_CTRL, 0x00);
	I2C_WRITE(client, HW_REG_GPADC_CTRL2, 0x00);

exit:
	/*HPB unlock*/
#if 0 /* temp W/A */
	hwspin_unlock_nospin(r8a73734_hwlock_pmic);
#endif

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return result;
}

/*
 * tps80032_read_bat_volt: read the battery voltage
 * @client: The I2C client device.
 * return:
 *        > 0: Battery voltage
 *        = 0: Error occurs
 */
static int tps80032_read_bat_volt(struct i2c_client *client)
{
	int result = 0;
	int ret, count_timer = 0;
	int ret_MSB, ret_LSB;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/*HPB lock*/
	ret = tps80032_get_hwsem_timeout(r8a73734_hwlock_pmic, CONST_HPB_WAIT);
	if (ret < 0) {
		PMIC_ERROR_MSG("%s:lock is already taken.\n", __func__);
		return -EBUSY;
	}

	/*Set 5V scaler and other internal ADC reference */
	ret = I2C_WRITE(client, HW_REG_GPADC_CTRL, 0x6B);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Set 5V scaler and enable VBAT */
	ret = I2C_WRITE(client, HW_REG_GPADC_CTRL2, 0x0C);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Enable GPADC */
	ret = I2C_WRITE(client, HW_REG_TOGGLE1, MSK_GPADC);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Select VBAT measurement channel */
	ret = I2C_WRITE(client, HW_REG_GPSELECT_ISB, 0x12);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Start GPADC */
	ret = I2C_WRITE(client, HW_REG_CTRL_P1, 0x08);
	if (0 > ret) {
		result = ret;
		goto exit;
	}

	/*Wait for ADC interrupt */
	while (count_timer <= CONST_WAIT_TIME) {
		schedule();

		/* Check ADC interrupt bit */
		ret = I2C_READ(client, HW_REG_CTRL_P1);
		if (0 > ret) {
			result = ret;
			goto disable;
		} else if (0 != (ret & MSK_BIT_1)) {
			/* Conversion finished */
			break;
		} else {
			count_timer++;
			/* Do nothing */
		}
	}

	if (CONST_WAIT_TIME < count_timer) {
		/* Time out */
		PMIC_DEBUG_MSG("%s: measurement conversion failed\n", __func__);
		result =  -1;
		goto disable;
	}

	/*Read the VBAT conversion result */
	ret = I2C_READ(client, HW_REG_GPCH0_MSB);
	if (0 > ret) {
		result = ret;
		goto disable;
	} else {
		ret_MSB = ret;
	}

	if (0x10 == ret_MSB) {
		PMIC_ERROR_MSG("%s: collision occurs.", __func__);
		result = -EAGAIN;
		goto exit;
	}

	ret = I2C_READ(client, HW_REG_GPCH0_LSB);
	if (0 > ret) {
		result = ret;
		goto disable;
	} else {
		ret_LSB = ret;
	}

	/*Correct the result */
	result = ((ret_MSB & 0x0F)<<8) | ret_LSB;
	result = (result * 5000) / 4096;

disable:
	/*Disable GPADC */
	I2C_WRITE(client, HW_REG_TOGGLE1, 0x01);
	I2C_WRITE(client, HW_REG_GPADC_CTRL, 0x00);
	I2C_WRITE(client, HW_REG_GPADC_CTRL2, 0x00);

exit:
	/*HPB unlock*/
#if 0 /* temp W/A */
	hwspin_unlock_nospin(r8a73734_hwlock_pmic);
#endif

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

	return result;
}


/*
 * tps80032_calc_bat_capacity: calculate the battery capacity
 * @client: The I2C client device.
 * return:
 *        > 0: Battery capacity
 *        = 0: Error occurs
 */
static int tps80032_calc_bat_capacity(struct i2c_client *client)
{
	int ret_vbat;
	int i;
	int soc;
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret_vbat = data->bat_volt;

	for (i = 99; i > 0; i--)
		if (BAT_VOLT_THRESHOLD[i] <= ret_vbat)
			break;

	soc = i + 1;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return soc;
}

/*
 * tps80032_disable_charger: disable charger when something wrong happended
 * @data:
 * return: 0
 */
static int tps80032_disable_charger(struct tps80032_data *data)
{
	int ret = 0;
	int ret_ctrl = 0;

	/* Read charger controller register */
	ret = I2C_READ(data->client_battery, HW_REG_CONTROLLER_CTRL1);
	if (0 > ret)
		return ret;

	ret_ctrl = ret;

	/* Read the value of current limit setting before disable charger */
	ret = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_CINLIMIT);
	if (0 > ret)
		return ret;
	else
		data->cin_limit = ret;

	/* Disable charger */
	ret = I2C_WRITE(data->client_battery,
	HW_REG_CONTROLLER_CTRL1, (ret_ctrl & (~(MSK_BIT_4 | MSK_BIT_5))));
	if (0 > ret)
		return ret;

	return 0;
}

/*
 * tps80032_en_charger: enable charger when something wrong happended
 * @data:
 * return: 0
 */
static int tps80032_en_charger(struct tps80032_data *data)
{
	int ret = 0;
	int ret_ctrl = 0;
	int ret_stat1 = 0;
	int ret_ovp = 0;

	/* Read charger controller register */
	ret_ctrl = I2C_READ(data->client_battery, HW_REG_CONTROLLER_CTRL1);
	if (0 > ret_ctrl)
		return ret_ctrl;

	/* Check if interrupt source */
	ret_stat1 = I2C_READ(data->client_battery, HW_REG_CONTROLLER_STAT1);
	if (0 > ret_stat1)
		return ret_stat1;

	/* Check status of battery voltage */
	ret_ovp = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_STATUS_INT1);
	if (0 > ret_ovp)
		return ret_ovp;

	/* Charger is present and charger is disable*/
	if ((0 != (ret_stat1 & (MSK_BIT_2 | MSK_BIT_3)))
	    && (0 == (ret_ctrl & MSK_BIT_4)) && (0 == (ret_ovp & MSK_BIT_3))) {
		/* Enable charger */
		ret = I2C_WRITE(data->client_battery,
			HW_REG_CONTROLLER_CTRL1, (ret_ctrl | MSK_BIT_4));
		if (0 > ret)
			return ret;

		/* Enable EN_LINCH */
		ret = I2C_READ(data->client_battery, HW_REG_CONTROLLER_CTRL1);
		if (0 > ret)
			return ret;

		ret = I2C_WRITE(data->client_battery,
				HW_REG_CONTROLLER_CTRL1, ret | MSK_BIT_5);
		if (0 > ret)
			return ret;
	}

	return 0;
}

/*
 * tps80032_save_setting: restore the charger setting for
 * enable charger before disable charger
 * @client: The I2C client device.
 * @state: The state of charger.
 *			0: new charger is plugged
 *			1: restore the setting for current limit
 * return:
 *	= 0: normal operation
 *	< 0: error occurs
 */
static int tps80032_save_setting(struct i2c_client *client, int state)
{
	int ret_ctrl;
	int ret;

	/* Select charge source */
	ret = I2C_READ(data->client_battery, HW_REG_CONTROLLER_CTRL1);
	if (0 > ret)
		return ret;

	ret_ctrl = ret;

	ret = I2C_READ(data->client_battery, HW_REG_CONTROLLER_STAT1);
	if (0 > ret)
		return ret;

	if (0 != (ret & MSK_BIT_2))
		/* Charger source is USB charger */
		ret_ctrl = ret_ctrl & (~MSK_BIT_3);
	else
		/* Charger source is AC charger */
		ret_ctrl = ret_ctrl | MSK_BIT_3;

	ret = I2C_WRITE(data->client_battery,
			HW_REG_CONTROLLER_CTRL1, ret_ctrl);
	if (0 > ret)
		return ret;

	/* Setting for constant voltage (CV) for full-charge phase */
	ret = I2C_WRITE(data->client_battery,
			HW_REG_CHARGERUSB_VOREG, CONST_VOREG);
	if (0 > ret)
		return ret;

	/* Setting for constant current (CC) for full-charge phase */
	ret = I2C_WRITE(data->client_battery,
			HW_REG_CHARGERUSB_VICHRG, CONST_VICHRG);
	if (0 > ret)
		return ret;

	/* Setting for constant current (CC) for pre-charge phase */
	ret = I2C_WRITE(data->client_battery,
			HW_REG_CHARGERUSB_VICHRG_PC, CONST_VICHRG_PC);
	if (0 > ret)
		return ret;

	if (1 == state) {
		/* Set current limit */
		ret = I2C_WRITE(data->client_battery,
				HW_REG_CHARGERUSB_CINLIMIT, data->cin_limit);
		if (0 > ret)
			return ret;

	} else {
		/* Current limit is set after */
	}

	/* Enable Charge current termination interrupt */
	ret = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_CTRL1);
	if (0 > ret)
		return ret;

	ret |= MSK_BIT_4;

	/* Set 1 to TERM bit at CHARGERUSB_CTRL1 (0xE8) register */
	ret = I2C_WRITE(data->client_battery, HW_REG_CHARGERUSB_CTRL1, ret);
	if (0 > ret)
		return ret;

	/* Enable charge one feature */
	ret = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_CTRL3);
	if (0 > ret)
		return ret;

	ret &= (~MSK_BIT_6);

	/* Set 0 to CHARGE_ONCE bit at CHARGERUSB_CTRL3 (0xEA) register */
	ret = I2C_WRITE(data->client_battery, HW_REG_CHARGERUSB_CTRL3, ret);
	if (0 > ret)
		return ret;

	return 0;
}

/*
 * tps80032_bat_update: update all battery information
 * @data: The struct which handles the TPS80032 data.
 * return: void
 */
static void tps80032_bat_update(struct tps80032_data *data)
{
	int ret, old_data, old_charger;
	int notify = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* Update all battery information */
	/* Read the state of USB charger */
	old_data = data->charger;
	old_charger = data->charger;
	ret = I2C_READ(data->client_battery, HW_REG_CONTROLLER_STAT1);
	if (0 > ret) {
		/* Do nothing */
	} else {
		if (0 == (ret & MSK_BIT_2))
			data->charger = 0;
		else
			data->charger = 1;

		/* Read the state of battery */
		if (0 == (ret & MSK_BIT_1))
			data->bat_presence = 1;
		else
			data->bat_presence = 0;

		/* Read the state of battery temperature is over or not */
		if (0 == (ret & MSK_BIT_0)) {
			if (1 == data->bat_over_temp) {
#ifdef PMIC_CHARGE_ENABLE
				/* Enable charger */
				tps80032_en_charger(data);
#endif
				/* Update status of battery over-temp */
				data->bat_over_temp = 0;
#ifdef PMIC_CHARGE_ENABLE
				/* Restore setting for charging */
				tps80032_save_setting(data->client_battery, 1);
#endif
			}
		} else {
			if (0 == data->bat_over_temp) {
#ifdef PMIC_CHARGE_ENABLE
				/* Disable charger */
				tps80032_disable_charger(data);
#endif
				/* Update status of battery over-temp */
				data->bat_over_temp = 1;
			}
		}
	}

	/* check the change of usb charger */
	if (old_data != data->charger)
		notify = 1;

	/* Read the state of EN_CHARGER bit */
	ret = I2C_READ(data->client_battery, HW_REG_CONTROLLER_CTRL1);
	if ((0 == (ret & MSK_BIT_4)) && (0 <= ret))
		data->en_charger = 0;
	else if (0 <= ret)
		data->en_charger = 1;

	/* Read the state of battery voltage is over or not */
	ret = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_STATUS_INT1);
	if (0 > ret)
		data->bat_over_volt = -1;
	else if (0 == (ret & MSK_BIT_3))
		data->bat_over_volt = 0;
	else
		data->bat_over_volt = 1;

	/* Get old value of battery voltage */
	old_data = data->bat_volt;

	/* Read the battery voltage */
	ret = tps80032_read_bat_volt(data->client_battery);

	/* Correct the battery voltage */
	if (0 < ret)
		ret = tps80032_gpadc_correct_voltage(data, ret);

	if (0 < ret) {
		num_vbat[num_volt] = ret;
		/* Increase value of VBAT */
		num_volt++;
	}

	if (4 == num_volt) {
		data->bat_volt = (num_vbat[0] + num_vbat[1] +
				  num_vbat[2] + num_vbat[3])/4;

		/* Get old value of battery capacity */
		old_data = data->bat_capacity;

		/* Calculate the battery capacity */
		ret = tps80032_calc_bat_capacity(data->client_battery);

		/* Update battery capacity */
		data->bat_capacity = ret;

		/* check the battery capacity is low or critical */
		if (20 > ret) {
			tps80032_check_bat_low_1(old_data, ret);
			tps80032_check_bat_low_2(old_charger, data);
		}

		/* check the change of battery capacity */
		if ((old_data != data->bat_capacity) && (1 != notify))
			notify = 1;

		/* Reset counter for battery voltage */
		num_volt = 0;
	}

	/* check the change of battery capacity */
	if ((old_data != data->bat_volt) && (1 != notify))
		notify = 1;

	/* Get old value of battery temperature */
	old_data = data->bat_temp;

	/* Read the battery temperature */
	ret = tps80032_read_bat_temp(data->client_battery);

	/* Correct the battery temp */
	if (0 < ret)
		ret = tps80032_gpadc_correct_temp(data, ret);

	if (0 < ret)
		data->bat_temp = ret;

	/* check the change of battery capacity */
	if ((old_data != data->bat_temp) && (1 != notify))
		notify = 1;

	/* Read the HPA temperature */
	ret = tps80032_read_hpa_temp(data->client_battery);

	/* Correct the HPA temp */
	if (0 < ret)
		ret = tps80032_gpadc_correct_temp(data, ret);

	if (0 < ret)
		data->hpa_temp = ret;

	/* Notify if there have any change */
	if (0 != notify)
		pmic_power_supply_changed(E_USB_STATUS_CHANGED
					 |E_BATTERY_STATUS_CHANGED);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}

/* WORK QUEUE */


/*
 * tps80032_ext_work: define the external device which is
 * inserted and notify the presence of external device
 * return: void
 */
static void tps80032_ext_work(void)
{
	int ret;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = I2C_READ(data->client_battery, HW_REG_USB_ID_INT_SRC);
	if (0 > ret) {
		PMIC_DEBUG_MSG("%s: I2C_READ failed err=%d\n",\
				__func__, ret);
		return;
	}

	ret &= MSK_GET_EXT_DEVICE;

	if (MSK_BIT_3 == ret)
		data->device = E_DEVICE_ACA_ADEVICERID_A;
	else if (MSK_BIT_2 == ret)
		data->device = E_DEVICE_ACA_ADEVICERID_B;
	else if (MSK_BIT_1 == ret)
		data->device = E_DEVICE_ACA_ADEVICERID_C;

	pmic_ext_device_changed(data->device);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}


/*
 * tps80032_update_work: update all battery information
 * @work: The struct work.
 * return: void
 */
static void tps80032_update_work(struct work_struct *work)
{
	struct tps80032_data *data = container_of(work,
					struct tps80032_data, update_work);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* call bat_updat function */
	tps80032_bat_update(data);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}

/*
 * tps80032_int_chrg_work: update all battery information
 * and notify the change of battery state
 * return: void
 */
static void tps80032_int_chrg_work(void)
{
	int ret = 0;
	int ret_stat = 0;
	int ret_sts = 0;
	int val = 0;
	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* Check status of battery voltage */
	ret = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_STATUS_INT1);
	if (0 > ret)
		return;

	/* If charge full */
	ret_sts = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_STATUS_INT2);
	if (0 > ret_sts)
		return;

	/* If battery is full */
	/* Do nothing - disable auto by HW */

	if (0 != (ret & MSK_BIT_3)) {
		/* If battery voltage is over-volt */
#ifdef PMIC_CHARGE_ENABLE
		/* Read value of current limit setting before disable charger */
		ret = I2C_READ(data->client_battery,
					HW_REG_CHARGERUSB_CINLIMIT);
		if (0 > ret)
			return;
		else
			data->cin_limit = ret;

		/* Disable charger */
		ret = I2C_READ(data->client_battery, HW_REG_CONTROLLER_CTRL1);
		if (0 > ret)
			return;

		ret = I2C_WRITE(data->client_battery,
				HW_REG_CONTROLLER_CTRL1, ret & (~(MSK_BIT_4)));
		if (0 > ret)
			return;

#endif
		/* Update battery over-volt status */
		data->bat_over_volt = 1;
	} else if (1 == data->bat_over_volt) {
		/* If battery voltage is not over-volt */
		/* Update battery over-volt status */
		data->bat_over_volt = 0;
#ifdef PMIC_CHARGE_ENABLE
		/* Check the state of charger */
		ret_stat = I2C_READ(data->client_battery,
					HW_REG_CONTROLLER_STAT1);
		if (0 > ret_stat)
			return;

		/* If charger is present */
		if (0 != (ret_stat & (MSK_BIT_2 | MSK_BIT_3))) {
			ret = I2C_READ(data->client_battery,
					HW_REG_CONTROLLER_CTRL1);
			if (0 > ret)
				return;

			/* Charger is disabled by over-voltage/not over-temp */
			if ((0 == (ret & MSK_BIT_4))
				&& (0 == (ret_stat & MSK_BIT_0))) {
				/* Enable charger */
				val = ret | MSK_BIT_4;
				ret = I2C_WRITE(data->client_battery,
						HW_REG_CONTROLLER_CTRL1, val);
				if (0 > ret)
					return;

				/* Restore the setting for charging */
				tps80032_save_setting(data->client_battery, 1);
			} else {
				/* Do nothing */
			}
		}
#endif
	}

	/* call bat_updat function */
	tps80032_bat_update(data);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}



/*
 * tps80032_chrg_ctrl_work: update all battery information
 * and notify the change of battery state
 * return: void
 */
static void tps80032_chrg_ctrl_work(void)
{
	int ret_stat1 = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* Check if interrupt source */
	ret_stat1 = I2C_READ(data->client_battery, HW_REG_CONTROLLER_STAT1);
	if (0 > ret_stat1)
		return;

#ifdef PMIC_FUELGAUGE_ENABLE
	if (data->vac_det != (ret_stat1 & MSK_BIT_3)) {
		/* interrupt source relate to external charger */
#ifdef PMIC_CHARGE_ENABLE
		tps80032_vac_charger_work();
#endif
		/* Update status of VAC_DET */
		data->vac_det = ret_stat1 & MSK_BIT_3;
	}
#endif

	if (data->vbus_det != (ret_stat1 & MSK_BIT_2)) {
		/* interrupt source relate to external charger */
		handle_nested_irq(data->irq_base + TPS80032_INT_VBUSS_WKUP);
		handle_nested_irq(data->irq_base + TPS80032_INT_VBUS);
		/* Update status of VBUS_DET */
		data->vbus_det = ret_stat1 & MSK_BIT_2;
	}

#ifdef PMIC_FUELGAUGE_ENABLE
	/* call bat_updat function */
	tps80032_bat_update(data);
#endif

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}

/*
 * tps80032_vac_charger_work: detect VAC charger and set
 * current limit when VAC charger is plugged
 * return: void
 */
static void tps80032_vac_charger_work(void)
{
	int ret;
	int ret_stat1;
	int ret_ctrl1;
	int ret_ovp;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* Check status of VAC_DET and VBUS_DET bits */
	ret_stat1 = I2C_READ(data->client_battery, HW_REG_CONTROLLER_STAT1);
	if (0 > ret_stat1)
		return;

	/* Read current value of CONTROLLER_CTRL1 register */
	ret_ctrl1 = I2C_READ(data->client_battery, HW_REG_CONTROLLER_CTRL1);
	if (0 > ret_ctrl1)
		return;

	/* Check status of battery voltage */
	ret_ovp = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_STATUS_INT1);
	if (0 > ret_ovp)
		return;

	/* Check VAC_DET bit value */
	if (0 != (ret_stat1 & MSK_BIT_3)) {
		/* If VAC charger is present */
		/* Set configuration for VAC charger current limit */
		ret = I2C_WRITE(data->client_battery,
			HW_REG_CHARGERUSB_CINLIMIT, CONST_VAC_CURRENT_LIMIT);
		if (0 > ret) {
			PMIC_DEBUG_MSG(
				"%s: I2C_WRITE failed err=%d\n", __func__, ret);
			return;
		}

		/* Select charge source */
		ret_ctrl1 = ret_ctrl1 | MSK_BIT_3;

		/* If battery is not over-temp and over-volt */
		if ((0 == (ret_stat1 & MSK_BIT_0))
			&& (0 == (ret_ovp & MSK_BIT_3))) {
			/* Enable charger */
			ret_ctrl1 = ret_ctrl1 | MSK_BIT_4;

			ret = I2C_WRITE(data->client_battery,
					HW_REG_CONTROLLER_CTRL1, ret_ctrl1);
			if (0 > ret) {
				PMIC_DEBUG_MSG(
				"%s: I2C_WRITE failed err=%d\n", __func__, ret);
				return;
			}

			/* Restore the setting for charging */
			tps80032_save_setting(data->client_battery, 0);
		} else {
			/* Do not enable charger */
		}
	} else if ((0 != (ret_stat1 & MSK_BIT_2))) {
		/* If VAC charger is not present and USB charger is present */
		/* Select charge source is USB */
		ret_ctrl1 = ret_ctrl1 & (~MSK_BIT_3);

		/* If battery is not over-temp and over-volt */
		if ((0 == (ret_stat1 & MSK_BIT_0))
			&& (0 == (ret_ovp & MSK_BIT_3))) {
			/* Enable charger */
			ret_ctrl1 = ret_ctrl1 | MSK_BIT_4;
			ret = I2C_WRITE(data->client_battery,
					HW_REG_CONTROLLER_CTRL1, ret_ctrl1);
			if (0 > ret) {
				PMIC_DEBUG_MSG(
				"%s: I2C_WRITE failed err=%d\n", __func__, ret);
				return;
			}

			/* Restore the setting for charging */
			tps80032_save_setting(data->client_battery, 1);
		} else {
			/* Do not enable charger */
		}
	} else {
		/* If both VAC charger and USB charger are not present */
		/* Disable charger */
		ret_ctrl1 = ret_ctrl1 & (~(MSK_BIT_4));
		ret = I2C_WRITE(data->client_battery,
					HW_REG_CONTROLLER_CTRL1, ret_ctrl1);
		if (0 > ret) {
			PMIC_DEBUG_MSG(
			"%s: I2C_WRITE failed err=%d\n", __func__, ret);
			return;
		}
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

	return;
}

/*
 * tps80032_resume_work: restart battery timer
 * @work: The struct work.
 * return: void
 */
static void tps80032_resume_work(struct work_struct *work)
{
	PMIC_DEBUG_MSG("%s: name=%s addr=0x%x\n", \
			__func__, data->client_battery->name, \
			data->client_battery->addr);

	/* Restart the battery timer */
	if (!timer_pending(&bat_timer)) {
		bat_timer.expires  = jiffies +
					msecs_to_jiffies(CONST_TIMER_UPDATE);
		add_timer(&bat_timer);
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
}

/*
 * tps80032_get_usb_online: get the presence of USB charger
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        = 1: USB charger is presence
 *        = 0: USB charger is not presence
 */
static int tps80032_get_usb_online(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = data->charger;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_get_bat_status: get the status of battery
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *	POWER_SUPPLY_STATUS_FULL: status of battery is full
 *	POWER_SUPPLY_STATUS_CHARGING: status of battery is charging
 *	POWER_SUPPLY_STATUS_DISCHARGING: status of battery is discharging
 *	POWER_SUPPLY_STATUS_NOT_CHARGING: status of battery is not_discharging
 *	POWER_SUPPLY_STATUS_UNKNOWN:  fail to get battery device data
 */
static int tps80032_get_bat_status(struct device *dev)
{
	int ret = 0;
	int ret_cap, ret_charger, ret_bat, ret_en_charger, ret_ovp;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret_cap = data->bat_capacity;
	ret_charger = data->charger;
	ret_bat = data->bat_presence;
	ret_en_charger = data->en_charger;
	ret_ovp = data->bat_over_volt;

	if (0 > ret_cap) {
		/* return UNKNOWN status */
		ret = POWER_SUPPLY_STATUS_UNKNOWN;
	} else if (THR_BAT_FULL == ret_cap) {
		/* return Full status */
		ret = POWER_SUPPLY_STATUS_FULL;
	} else if (0 == ret_en_charger) {
		/* return DISCHARGING status */
		ret = POWER_SUPPLY_STATUS_DISCHARGING;
	} else if (((1 == ret_charger) && (0 == ret_bat)) || (1 == ret_ovp)) {
		/* return NOT_CHARGING status */
		ret = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else if (1 == ret_en_charger) {
		/* return CHARGING status */
		ret = POWER_SUPPLY_STATUS_CHARGING;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;

}

/*
 * tps80032_get_bat_health: get the health of battery
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        POWER_SUPPLY_HEALTH_OVERHEAT: the health of battery is overheat
 *        POWER_SUPPLY_HEALTH_OVERVOLTAGE: the health of battery is overvoltage
 *        POWER_SUPPLY_HEALTH_GOOD: the health of battery is good
 *        POWER_SUPPLY_HEALTH_COLD: the health of battery is cold
 *        POWER_SUPPLY_HEALTH_UNKNOWN:  fail to get battery device data
 */
static int tps80032_get_bat_health(struct device *dev)
{
	int ret;
	int ret_overvolt, ret_overtemp, ret_temp;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret_overvolt = data->bat_over_volt;
	ret_overtemp = data->bat_over_temp;
	ret_temp = data->bat_temp;

	if ((0 > ret_overvolt) || (0 > ret_overtemp)) {
		/* return UNKNOWN health  */
		ret = POWER_SUPPLY_HEALTH_UNKNOWN;
	} else if (1 == ret_overvolt) {
		/* return OVERVOLTAGE health  */
		ret = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	} else if (1 == ret_overtemp) {
		/* return OVERHEAT health  */
		ret = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if (CONST_0C_DEGREE <= ret_temp) {
		/* return COLD health */
		ret = POWER_SUPPLY_HEALTH_COLD;
	} else {
		/* return GOOD health */
		ret = POWER_SUPPLY_HEALTH_GOOD;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}


/*
 * tps80032_get_bat_present: get the presence of battery
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        = 1: Battery is presence
 *        = 0: Battery is not presence
 */
static int tps80032_get_bat_present(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = data->bat_presence;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

	/* return the presence of battery */
	return ret;
}

/*
 * tps80032_get_bat_temperature: get the battery temperature
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        > 0: the battery temperature
 *        = 0: Error occurs
 */
static int tps80032_get_bat_temperature(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = data->bat_temp;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_get_hpa_temperature: get the HPA temperature
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        > 0: the HPA temperature
 */
static int tps80032_get_hpa_temperature(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = data->hpa_temp;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_get_bat_capacity: get the remaining capacity in percent unit
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        > 0: the remaining capacity in percent unit.
 *        = 0: Error occurs
 */
static int tps80032_get_bat_capacity(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = data->bat_capacity;

	if (0 > ret)
		ret = 0;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_get_bat_capacity_level: get the level of remaining capacity
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        POWER_SUPPLY_CAPACITY_LEVEL_FULL : the capacity level is FULL
 *        POWER_SUPPLY_CAPACITY_LEVEL_HIGH : the capacity level is HIGH
 *        POWER_SUPPLY_CAPACITY_LEVEL_NORMAL : the capacity level is NORMAL
 *        POWER_SUPPLY_CAPACITY_LEVEL_LOW : the capacity level is LOW
 *        POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL : the capacity level is CRITICAL
 *        POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN:  fail to get battery device data
 */

static int tps80032_get_bat_capacity_level(struct device *dev)
{
	int ret;
	int capacity;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	capacity = data->bat_capacity;

	if (0 > capacity) {
		/* return UNKNOWN level */
		ret = POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	} else if (THR_BAT_FULL == capacity) {
		/* return FULL level */
		ret = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	} else if (THR_BAT_HIGH <= capacity) {
		/* return HIGH level */
		ret = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	} else if (THR_BAT_NORMAL <= capacity) {
		/* return NORMAL level */
		ret = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	} else if (THR_BAT_LOW <= capacity) {
		/* return LOW level */
		ret = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	} else {
		/* return CRITICAL level */
		ret = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;

}


/*
 * tps80032_get_bat_voltage: get the battery voltage
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        > 0: the voltage value of battery.
 *        = 0: Error occurs
 */
static int tps80032_get_bat_voltage(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = data->bat_volt;

	if (0 > ret)
		ret = 0;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}


/*
 * tps80032_stop_charging: control the battery charging
 * @dev: The struct which handles the TPS80032 driver.
 * @stop: The control command to stop or restart
 * return:
 *        < 0: Error occurs
 *        = 0: Success
 */
static int tps80032_stop_charging(struct device *dev, int stop)
{
	int ret, cal;
	int ret_cur_limit;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

#ifdef PMIC_CHARGE_ENABLE
	if ((1 != stop) && (0 != stop)) {
		/* Return error */
		return -EINVAL;
	} else {
		/* read the current value of CONTROLLER_CTRL1 register */
		ret = I2C_READ(client, HW_REG_CONTROLLER_CTRL1);
		if (0 > ret) {
			return ret;
		} else if (((1 == stop) && (0 != (ret & MSK_BIT_4)))
				|| ((0 == stop) && (0 == (ret & MSK_BIT_4)))) {
			/* Do nothing */
		} else {
			if (0 == stop) {
				/* Disable charger */
				cal  = ret & (~(MSK_BIT_4));

				ret_cur_limit = I2C_READ(client,
						HW_REG_CHARGERUSB_CINLIMIT);
				if (0 > ret_cur_limit)
					return ret_cur_limit;
				else
					data->cin_limit = ret_cur_limit;

			} else {
				/* Enable charger */
				cal = ret | MSK_BIT_4;
			}

			/* write the new value of EN_CHARGER bit */
			ret = I2C_WRITE(client, HW_REG_CONTROLLER_CTRL1, cal);
			if (0 > ret)
				return ret;

			/* Restore setting for charging */
			if (1 == stop)
				tps80032_save_setting(client, 1);
		}
	}
#endif
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;
}

/*
 * tps80032_correct_temp: change temperature unit from 0.1K to 0.1C
 * @temp: the battery temperature in 0.1K unit
 * return:
 *        the battery temperature in 0.1C unit
 */
static int tps80032_correct_temp(int temp)
{
	int ret = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* change temperature unit from 0.1K to 0.1C */
	if ((0 < temp) && (temp < 901))
		ret = (10100 - 6 * temp)/10;
	else if (temp < 2507)
		ret = (7800 - 3 * temp)/10;
	else
		ret = (15400 - 6 * temp)/10;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}


/*
 * tps80032_correct_voltage: change voltage unit from mV to microV
 * @vol: the voltage value in mV unit
 * return:
 *       >0: the voltage value in microV unit
 */
static int tps80032_correct_voltage(int vol)
{
	int ret = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* change voltage unit from mV to microV */
	ret = vol*CONST_CONVERT_VOLT;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}


/*
 * tps80032_correct_capacity: correct the battery capacity
 * @capacity: the voltage value in mV unit
 * return:
 *       >0: the correct capacity
 */
static int tps80032_correct_capacity(int capacity)
{
	int ret = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = capacity;

	if (THR_BAT_FULL < ret)
		ret = THR_BAT_FULL;
	else if (0 >= ret)
		ret = CONST_BAT_MIN;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}



/* COMMON FUNCTION */

/*
 * tps80032_read_register: read the current value of TPS80032 register
 * @dev: The struct which handles the TPS80032 driver.
 * @slave: The I2C slave address.
 * @addr: The address of register.
 * return:
 *        >= 0: Value of register
 *        < 0: Error occurs
 */
static int tps80032_read_register(struct device *dev, int slave, u8 addr)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	switch (slave) {
	case E_SLAVE_RTC_POWER:
		ret = I2C_READ(data->client_power, addr);
		break;
	case E_SLAVE_BATTERY:
		ret = I2C_READ(data->client_battery, addr);
		break;
	case E_SLAVE_DVS:
		ret = I2C_READ(data->client_dvs, addr);
		break;
	case E_SLAVE_JTAG:
		ret = I2C_READ(data->client_jtag, addr);
		break;
	default:
		ret = -EINVAL;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_set_current_limit: enable/disable charger when the
 * charger is present/not present
 * set current limit for charging operation
 * @dev: an i2c client
 * @chrg_state: The charger state: (1/0)
 * @chrg_type: The charger type
 *        < 0: Error occurs
 *        = 0: Normal operation
 */
static int tps80032_set_current_limit(struct device *dev,
			int chrg_state, int chrg_type)
{
	int ret = 0;
	int ret_stat1 = 0;
	int ret_ctrl1 = 0;
	int ret_ovp = 0;
	int val_limit = 0;

	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

#ifdef PMIC_CHARGE_ENABLE
	if ((0 != chrg_state) && (1 != chrg_state))
		return -EINVAL;

	if ((0 > chrg_type) || (4 < chrg_type))
		return -EINVAL;

	/* Check status of charger */
	ret_stat1 = I2C_READ(data->client_battery, HW_REG_CONTROLLER_STAT1);
	if (0 > ret_stat1)
		return ret_stat1;

	ret_ctrl1 = I2C_READ(data->client_battery, HW_REG_CONTROLLER_CTRL1);
	if (0 > ret_ctrl1)
		return ret_ctrl1;

	if (0 == chrg_state) {
		/* If USB charger is not present */
		if (0 == (ret_stat1 & MSK_BIT_3)) {
			/* VAC charger is not present */
			/* Disable charger */
			ret_ctrl1 = ret_ctrl1 & (~MSK_BIT_4);

			ret = I2C_WRITE(data->client_battery,
					HW_REG_CONTROLLER_CTRL1, ret_ctrl1);
			if (0 > ret)
				return ret;

		} else {
			/* VAC charger is present */
			/* Do nothing */
			return 0;
		}
	} else {
		/* If USB charger is present */
		/* Set current limit for USB charger */
		switch (chrg_type) {
		case USB_SDP_FULL_SPEED:
			val_limit = 0x01; /* 100mA */
			break;
		case USB_SDP_HIGH_SPEED:
			val_limit = 0x09; /* 500mA */
			break;
		case USB_CDP_FULL_SPEED:
			val_limit = 0x2E; /* 1500mA */
			break;
		case USB_CDP_HIGH_SPEED:
			val_limit = 0x28; /* 900mA */
			break;
		case USB_DCP:
			val_limit = 0x2E; /* 1500mA */
			break;
		default:
			val_limit = 0x01; /* 100mA */
			break;
		}
		/* Save the current limit for USB charger setting */
		data->cin_limit = val_limit;

		/* Check state of VAC charger */
		if (0 == (ret_stat1 & MSK_BIT_3)) {
			/* VAC charger is not present */
			ret = I2C_WRITE(data->client_battery,
					HW_REG_CHARGERUSB_CINLIMIT, val_limit);
			if (0 > ret)
				return ret;

			/* Select charger source is VBUS */
			ret_ctrl1 = ret_ctrl1 & (~MSK_BIT_3);

			/* Check battery is over-temp or over-volt */
			ret_ovp = I2C_READ(data->client_battery,
						HW_REG_CHARGERUSB_STATUS_INT1);
			if (0 > ret_ovp)
				return ret;

			if ((0 == (ret_ovp & MSK_BIT_3))
				&& (0 == (ret_stat1 & MSK_BIT_0))) {
				/* Enable charger */
				ret_ctrl1 = ret_ctrl1 | MSK_BIT_4;
				ret = I2C_WRITE(data->client_battery,
					HW_REG_CONTROLLER_CTRL1, ret_ctrl1);
				if (0 > ret)
					return ret;

				/* Restore setting for charging */
				tps80032_save_setting(data->client_battery, 0);
			} else {
				/* Do not enable charger */
			}

		} else {
			/* VAC charger is present */
			/* Do nothing */
			return 0;
		}
	}

#endif
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;
}


/*
 * tps80032_clk32k_enable: Turn oscillation clock to ON/OFF
 * @clk_res:
 *		CLK32KAO
 *		CLK32KG
 *		CLK32KAUDIO
 * @state:
 *		TPS80032_STATE_ON
 *		TPS80032_STATE_OFF
 * Return:
 *        < 0: Error occurs
 *        = 0: Normal operation
 */
static int tps80032_clk32k_enable(u8 clk_res, u8 state)
{
	int ret = 0;
	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/*Check validity of clock resource*/
	if (clk_res > 2) {
		ret = -EINVAL;
		PMIC_DEBUG_MSG("%s: Invalid clock resource id (%d)<<<\n", \
					__func__, clk_res);
		goto exit;
	}

	/*Check validity of clock status*/
	if (state > 1) {
		ret = -EINVAL;
		PMIC_DEBUG_MSG("%s: Invalid clock status (%d)<<<\n",\
				__func__, state);
		goto exit;
	}

	if (NULL == data) {
		ret = -EINVAL;
		PMIC_DEBUG_MSG("%s: I2C client is not initialized <<<\n",\
				__func__);
		goto exit;
	}

	/*Clock status corresponding to clock resource does not change*/
	if (clk_state[clk_res] == state) {
		ret = 0;
		goto exit;
	}

	switch (clk_res) {
	case CLK32KAO:
		ret = I2C_WRITE(data->client_power,
				HW_REG_CLK32KAO_CFG_STATE, state);
		if (ret < 0) {
			PMIC_DEBUG_MSG(">>> %s(), line % d, error(%d)\n", \
						__func__, __LINE__, ret);
			goto exit;
		}
		clk_state[CLK32KAO] = state;
		break;

	case CLK32KG:
		ret = I2C_WRITE(data->client_power,
				HW_REG_CLK32KG_CFG_STATE, state);
		if (ret < 0) {
			PMIC_DEBUG_MSG(">>> %s(), line % d, error(%d)\n", \
						__func__, __LINE__, ret);
			goto exit;
		}

		ret = I2C_WRITE(data->client_power,
				HW_REG_CLK32KG_CFG_TRANS, state);
		if (ret < 0) {
			PMIC_DEBUG_MSG(">>> %s(), line % d, error(%d)\n", \
						__func__, __LINE__, ret);
			goto exit;
		}

		clk_state[CLK32KG] = state;
		break;

	case CLK32KAUDIO:
		ret = I2C_WRITE(data->client_power,
				HW_REG_CLK32KAUDIO_CFG_STATE, state);
		if (ret < 0) {
			PMIC_DEBUG_MSG(">>> %s(), line % d, error(%d)\n", \
						__func__, __LINE__, ret);
			goto exit;
		}

		ret = I2C_WRITE(data->client_power,
				HW_REG_CLK32KAUDIO_CFG_TRANS, state);
		if (ret < 0) {
			PMIC_DEBUG_MSG(">>> %s(), line % d, error(%d)\n", \
						__func__, __LINE__, ret);
			goto exit;
		}

		clk_state[CLK32KAUDIO] = state;
		break;

	default:
		break;
	}

	ret = 0;
exit:
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}


/*
 * tps80032_read: read the current value of TPS80032 register
 * @dev: The struct which handles the TPS80032 driver.
 * @addr: The address of register.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_read(struct device *dev, u8 addr)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = I2C_READ(client, addr);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}


/*
 * tps80032_reads: read the current value of TPS80032 registers
 * @dev: The struct which handles the TPS80032 driver.
 * @addr: The address of register.
 * @len: The number of registers is read.
 * @val: The values of registers.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_reads(struct device *dev, u8 addr, int len, u8 *val)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = i2c_smbus_read_i2c_block_data(client, addr, len, val);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_write: write the value to TPS80032 register
 * @dev: The struct which handles the TPS80032 driver.
 * @addr: The address of register.
 * @val: The value for setting to register.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_write(struct device *dev, u8 addr, u8 val)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = I2C_WRITE(client, addr, val);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_writes: write the values to TPS80032 registers
 * @dev: The struct which handles the TPS80032 driver.
 * @addr: The address of register.
 * @len: The number of registers is wrote.
 * @val: The values for setting to registers.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_writes(struct device *dev, u8 addr, int len, u8 *val)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	ret = i2c_smbus_write_i2c_block_data(client, addr, len, val);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_get_temp_status: read the current temperature of battery
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        > 0: Battery temperature
 *        = 0: Error occurs
 */
static int tps80032_get_temp_status(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (NULL != data)
		ret = data->bat_temp;
	else
		ret = 0;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

#ifdef PMIC_PT_TEST_ENABLE

/*
 * tps80032_get_batt_status: read the current temperature of battery
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        > 0: Battery temperature
 *        = 0: Error occurs
 */
static int tps80032_get_batt_status(struct device *dev, int property)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (NULL != data) {
		switch (property) {
		case E_BATT_PROP_ONLINE:
			return tps80032_get_usb_online(
					&data->client_battery->dev);
		case E_BATT_PROP_STATUS:
			return tps80032_get_bat_status(
					&data->client_battery->dev);
		case E_BATT_PROP_HEALTH:
			return tps80032_get_bat_health(
					&data->client_battery->dev);
		case E_BATT_PROP_PRESENT:
			return tps80032_get_bat_present(
					&data->client_battery->dev);
		case E_BATT_PROP_TECHNOLOGY:
			return 0;		/* Not supported by TPS80032 */
		case E_BATT_PROP_CAPACITY:
			return tps80032_get_bat_capacity(
					&data->client_battery->dev);
		case E_BATT_PROP_VOLTAGE:
			return tps80032_get_bat_voltage(
					&data->client_battery->dev);
		case E_BATT_PROP_TEMP:
			return tps80032_get_bat_temperature(
					&data->client_battery->dev);
		}
	} else {
		ret = -1;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

#endif

/* struct pmic_device_ops */
static struct pmic_device_ops tps80032_power_ops = {
	.set_power_on = tps80032_set_power_on,
	.set_power_off = tps80032_set_power_off,
	.set_voltage = tps80032_set_voltage,
	.set_power_mode = tps80032_set_power_mode,
	.force_power_off = tps80032_force_power_off,
	.get_ext_device = tps80032_get_ext_device,
	.read_register = tps80032_read_register,
#ifdef PMIC_FUELGAUGE_ENABLE
	.set_current_limit = tps80032_set_current_limit,
#else
	.set_current_limit = NULL,
#endif
	.clk32k_enable = tps80032_clk32k_enable,
	.read = tps80032_read,
	.reads = tps80032_reads,
	.write = tps80032_write,
	.writes = tps80032_writes,
	.get_temp_status = tps80032_get_temp_status,
#ifdef PMIC_PT_TEST_ENABLE
	.get_batt_status = tps80032_get_batt_status,
#endif
};

/* struct tps80032_vbus_ops */
static struct usb_otg_pmic_device_ops tps80032_vbus_ops = {
	.set_vbus = tps80032_set_vbus,
};

#ifdef PMIC_FUELGAUGE_ENABLE
/* struct tps80032_power_battery_ops */
static struct pmic_battery_ops tps80032_power_battery_ops = {
	.get_usb_online = tps80032_get_usb_online,
	.get_bat_status = tps80032_get_bat_status,
	.get_bat_health = tps80032_get_bat_health,
	.get_bat_present = tps80032_get_bat_present,
	.get_bat_technology = NULL,
	.get_bat_capacity = tps80032_get_bat_capacity,
	.get_bat_capacity_level = tps80032_get_bat_capacity_level,
	.get_bat_temperature = tps80032_get_bat_temperature,
	.get_hpa_temperature = tps80032_get_hpa_temperature,
	.get_bat_voltage = tps80032_get_bat_voltage,
	.get_bat_time_to_empty = NULL,
	.get_bat_time_to_full = NULL,
	.stop_charging = tps80032_stop_charging,
};

/* struct tps80032_correct_ops */
struct battery_correct_ops tps80032_correct_ops = {
	.correct_capacity_func = tps80032_correct_capacity,
	.correct_time_to_empty_func = NULL,
	.correct_time_to_full_func = NULL,
	.correct_status_func = NULL,
	.correct_temp_func = tps80032_correct_temp,
	.correct_voltage_func = tps80032_correct_voltage,
};
#endif

/*
 * tps80032_ldo_get_volt_sleep: get the LODs voltage before entering sleep mode
 * input: void
 * return: void
 */
static void tps80032_ldo_get_volt_suspend(void)
{
	int ret = 0;
	int i = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	/* Read LDO voltage registers */
	for (i = 0; i < 7; i++) {
		ret = I2C_READ(data->client_power, LDO_VOLT_REG[i]);
		if (0 <= ret)
			ret = (ret & (MSK_BIT_0 | MSK_BIT_1 | MSK_BIT_2
					| MSK_BIT_3 | MSK_BIT_4));
		volt_sleep[i] = LDO_VOLT_VALUE[ret - 1];
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}

/*
 * tps80032_ldo_get_state_sleep: get the LODs state before entering sleep mode
 * input: void
 * return: void
 */
static void tps80032_ldo_get_state_suspend(void)
{
	int ret = 0;
	int i = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	/* Read LDO state registers */
	for (i = 0; i < 7; i++) {
		ret = I2C_READ(data->client_power, LDO_STATE_REG[i]);
		if (0 > ret)
			state_sleep[i] = 0;
		else if (0 == (ret & (MSK_BIT_0 | MSK_BIT_1)))
			state_sleep[i] = 2;
		else
			state_sleep[i] = 1;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}


/*
 * tps80032_power_suspend: power suppend event
 * @dev: The device struct.
 * return:
 *        = 0: Normal operation
 */
static int tps80032_power_suspend(struct device *dev)
{
	int ret = 0;
	int i = 0;
	struct tps80032_platform_data *pdata = data->dev->platform_data;
	int val = 0;
	unsigned long flags;
	int error_flag = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);

	PMIC_DEBUG_MSG(">>> %s: name=%s addr=0x%x\n",
		__func__, data->client_power->name, data->client_power->addr);

	/* Lock spinlock */
	spin_lock_irqsave(&data->pmic_lock, flags);
	/* Set flag */
	suspend_flag = 1;
	spin_unlock_irqrestore(&data->pmic_lock, flags);

	/* Wait until interrupt_flag == 0 */
	while (interrupt_flag == 1)
		schedule();

	/* Disable IRQ28 */
	disable_irq(pint2irq(CONST_INT_ID));

#ifdef PMIC_FUELGAUGE_ENABLE
	/* Disable timer of PMIC */
	del_timer_sync(&bat_timer);

	/* Cancel workqueue */
	cancel_work_sync(&(data->update_work));
#endif

	/* Get status of LDO before entering sleep mode */
	tps80032_ldo_get_state_suspend();
	tps80032_ldo_get_volt_suspend();
	/* Reset key counter */
	key_count = 0;

	/* Back up and set gpio port*/
	/* Back up gpio data value */
	output_backup[0] = gpio_get_value(pdata->pin_gpio[0]); /*MSECURE*/
	output_backup[1] = gpio_get_value(pdata->pin_gpio[2]); /*NRESWARM*/
	output_backup[2] = gpio_get_value(pdata->pin_gpio[3]); /*GPADC_START*/

	/* Set gpio port control */
	for (i = 0; i < NUM_PORT; i++) {
		u8 temp;
		temp = pdata->get_portcr_value(pdata->portcr[i]);
		portcr_val_backup[i] = temp;
		pdata->set_portcr_value(portcr_val[i], pdata->portcr[i]);
	}
	/* Set gpio data value */
	gpio_set_value(pdata->pin_gpio[0], GPIO_HIGH); /*MSECURE*/
	gpio_set_value(pdata->pin_gpio[2], GPIO_HIGH); /*NRESWARM*/
	gpio_set_value(pdata->pin_gpio[3], GPIO_LOW); /*GPADC_START*/
	/* Disable GPADC */
	ret = I2C_WRITE(data->client_battery, HW_REG_TOGGLE1, 0x11);
	if (0 > ret) {
		/* Do nothing */
		error_flag = 1;
		PMIC_ERROR_MSG("%s: I2C_WRITE failed err=%d\n",
			__func__, ret);
	}

	/* Disable GPADC_VREF */
	ret = I2C_WRITE(data->client_battery, HW_REG_GPADC_CTRL, 0x00);
	if (0 > ret) {
		/* Do nothing */
		error_flag = 1;
		PMIC_ERROR_MSG("%s: I2C_WRITE failed err=%d\n",
			__func__, ret);
	}

	ret = I2C_READ(data->client_battery, HW_REG_GPADC_CTRL2);
	if (0 > ret) {
		/* Do nothing */
		error_flag = 1;
		PMIC_ERROR_MSG("%s: I2C_READ failed err=%d\n",
			__func__, ret);
	}

	val = ret & 0xF3;
	ret = I2C_WRITE(data->client_battery, HW_REG_GPADC_CTRL2, val);
	if (0 > ret) {
		/* Do nothing */
		error_flag = 1;
		PMIC_ERROR_MSG("%s: I2C_WRITE failed err=%d\n",
			__func__, ret);
	}

	if (1 == error_flag) {
		spin_lock_irqsave(&data->pmic_lock, flags);

		/* Set flag is 0 if error occur */
		suspend_flag = 0;

		/* Unlock spinlock */
		spin_unlock_irqrestore(&data->pmic_lock, flags);

		PMIC_DEBUG_MSG("%s end <<<\n", __func__);
		return 0;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;
}

/*
 * tps80032_power_resume: power resume event
 * @dev: The device struct.
 * return:
 *        = 0: Normal operation
 */
static int tps80032_power_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tps80032_data *data = i2c_get_clientdata(client);
	struct tps80032_platform_data *pdata = data->dev->platform_data;
	int i = 0;

	PMIC_DEBUG_MSG(">>> %s: name=%s addr=0x%x\n",
			__func__, client->name, client->addr);

	/* Clear flag */
	suspend_flag = 0;

	/* Enable IRQ28 */
	enable_irq(pint2irq(CONST_INT_ID));

	/* Restore gpio port control */
	for (i = 0; i < NUM_PORT; i++)
		pdata->set_portcr_value(portcr_val_backup[i], pdata->portcr[i]);

	/* Restore gpio data value */
	gpio_set_value(pdata->pin_gpio[0], output_backup[0]); /*MSECURE*/
	gpio_set_value(pdata->pin_gpio[2], output_backup[1]); /*NRESWARM*/
	gpio_set_value(pdata->pin_gpio[3], output_backup[2]); /*GPADC_START*/
#ifdef PMIC_FUELGAUGE_ENABLE
	queue_work(data->queue, &data->resume_work);
#endif

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;
}


/*
 * tps80032_init_power_hw: init hardware for power supply management
 * @data: The struct which handles the TPS80032 data.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_init_power_hw(struct tps80032_data *data)
{
	int ret = 0;
	int val = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* Check board revision and setting for LDO5 voltage */
	if (BOARD_REV_0_4 <= u2_get_board_rev())
		val = E_LDO_VOLTAGE_3_3000V;
	else if (BOARD_REV_0_3 <= u2_get_board_rev())
		val = E_LDO_VOLTAGE_3_0000V;
	else
		val = E_LDO_VOLTAGE_2_5000V;

	ret = I2C_WRITE(data->client_power, HW_REG_LDO5_CFG_VOLTAGE,
	tps80032_check_ldo_voltage_valid(val));
	if (0 > ret)
		return ret;

	/* Set default voltage (1.8V) for LDO7 */
	ret = I2C_WRITE(data->client_power, HW_REG_LDO7_CFG_VOLTAGE,
		tps80032_check_ldo_voltage_valid(E_LDO_VOLTAGE_1_8000V));
	if (0 > ret)
		return ret;

	/* Assign SMSP1 (VCORE) and SMPS5 (VCORE_RF) into Group1 */
	ret = I2C_WRITE(data->client_power, HW_REG_PREQ1_RES_ASS_A,
				MSK_PREQ1_ASS_A);
	if (0 > ret)
		return ret;

	/* Assign LDOLN into Group1 */
	ret = I2C_WRITE(data->client_power, HW_REG_PREQ1_RES_ASS_B,
				MSK_PREQ1_ASS_B);
	if (0 > ret)
		return ret;

	/* Assign LDOLN into Group3 */
	ret = I2C_WRITE(data->client_power, HW_REG_PREQ3_RES_ASS_B,
				MSK_PREQ3_ASS_B);
	if (0 > ret)
		return ret;

	/* Set state "OFF" in mode "SLEEP" for LDOLN */
	ret = I2C_WRITE(data->client_power, HW_REG_LDOLN_CFG_TRANS,
				CONST_LDOLN_CFG_TRANS);
	if (0 > ret)
		return ret;

	/* Turn off SMPS4 */
	ret = I2C_WRITE(data->client_power, HW_REG_SMPS4_CFG_STATE,
				tps80032_check_state_valid(E_POWER_OFF));
	if (0 > ret)
		return ret;

	/* Assign SMPS4 into Group2 */
	ret = I2C_WRITE(data->client_power, HW_REG_PREQ2_RES_ASS_A,
				MSK_PREQ2_ASS_A);
	if (0 > ret)
		return ret;

	/* Unmask for PREQ1, PREQ2 and PREQ3 control signal */
	ret = I2C_WRITE(data->client_power, HW_REG_PHOENIX_MSK_TRANSITION,
				MSK_TRANSITION);
	if (0 > ret)
		return ret;

	/* Configure VRTC is standard power mode */
	ret = I2C_READ(data->client_power, HW_REG_BBSPOR_CFG);
	if (0 > ret)
		return ret;

	val = ret & (~MSK_BIT_6);

	ret = I2C_WRITE(data->client_power, HW_REG_BBSPOR_CFG, val);
	if (0 > ret)
		return ret;

	/* Set backup battery end of charge voltage is 3.15V */
	ret = I2C_READ(data->client_power, HW_REG_BBSPOR_CFG);
	if (0 > ret)
		return ret;

	val = (ret | MSK_BIT_2) & (~MSK_BIT_1);

	ret = I2C_WRITE(data->client_power, HW_REG_BBSPOR_CFG, val);
	if (0 > ret)
		return ret;

	/* Enable backup battery charging */
	ret = I2C_READ(data->client_power, HW_REG_BBSPOR_CFG);
	if (0 > ret)
		return ret;

	val = ret | MSK_BIT_3;

	ret = I2C_WRITE(data->client_power, HW_REG_BBSPOR_CFG, val);
	if (0 > ret)
		return ret;

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_init_battery_hw: init hardware for battery management
 * @data: The struct which handles the TPS80032 data.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_init_battery_hw(struct tps80032_data *data)
{
	int ret = 0;
	int ret_stat = 0;
	int ret_ovp = 0;
	int val = 0;
	int charger_stat;
	int ret_vsysmin_hi = 0;
	int ret_voreg = 0;
	int i = 0;
	int VOREG[63] = {
		3500, 3520, 3540, 3560, 3580, 3600, 3620, 3640, 3660, 3680,
		3700, 3720, 3740, 3760, 3780, 3800, 3820, 3840, 3860, 3880,
		3900, 3920, 3940, 3960, 3980, 4000, 4020, 4060, 4080, 4100,
		4120, 4140, 4160, 4180, 4200, 4220, 4240, 4260, 4280, 4300,
		4320, 4340, 4360, 4380, 4400, 4420, 4440, 4460, 4480, 4500,
		4520, 4540, 4560, 4580, 4600, 4620, 4640, 4660, 4680, 4700,
		4720, 4740, 4760};

	int VSYS[53] = {0,
		2050, 2100, 2150, 2200, 2250, 2300, 2350, 2400, 2450, 2500,
		2550, 2600, 2650, 2700, 2750, 2800, 2850, 2900, 2950, 3000,
		3050, 3100, 3150, 3200, 3250, 3300, 3350, 3400, 3450, 3500,
		3550, 3600, 3650, 3700, 3750, 3800, 3850, 3900, 3950, 4000,
		4050, 4100, 4150, 4200, 4250, 4300, 4350, 4400, 4450, 4500,
		4550, 4600};

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/* Setting for voltage limit */
	ret = I2C_WRITE(data->client_battery,
				HW_REG_CHARGERUSB_CTRLLIMIT1, CONST_CTRLLIMIT1);
	if (0 > ret)
		return ret;

	/* Setting for current limit */
	ret = I2C_WRITE(data->client_battery,
				HW_REG_CHARGERUSB_CTRLLIMIT2, CONST_CTRLLIMIT2);
	if (0 > ret)
		return ret;

	/* Lock setting for voltage and current limit */
	ret = I2C_READ(data->client_battery,
				HW_REG_CHARGERUSB_CTRLLIMIT2);
	if (0 > ret)
		return ret;
	val = ret | MSK_BIT_4;
	ret = I2C_WRITE(data->client_battery,
				HW_REG_CHARGERUSB_CTRLLIMIT2, val);
	if (0 > ret)
		return ret;

	/* Setting for constant voltage (CV) for full-charge phase */
	ret = I2C_WRITE(data->client_battery, HW_REG_CHARGERUSB_VOREG,
				CONST_VOREG);
	if (0 > ret)
		return ret;

	ret_voreg = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_VOREG);
	if (0 > ret_voreg) {
		PMIC_ERROR_MSG("%s:%d read I2C failed err=%d\n", \
					__func__, __LINE__, ret_voreg);
		return ret_voreg;
	}

	/* Setting for constant current (CC) for full-charge phase */
	ret = I2C_WRITE(data->client_battery, HW_REG_CHARGERUSB_VICHRG,
				CONST_VICHRG);
	if (0 > ret)
		return ret;

	/* Setting for constant current (CC) for pre-charge phase */
	ret = I2C_WRITE(data->client_battery, HW_REG_CHARGERUSB_VICHRG_PC,
				CONST_VICHRG_PC);
	if (0 > ret)
		return ret;

	/* Setting for CONTROLLER_VSEL_COMP register */
	ret = I2C_WRITE(data->client_battery, HW_REG_CONTROLLER_VSEL_COMP,
				CONST_VSEL_COMP);
	if (0 > ret)
		return ret;

	/* Setting for HW_REG_CHARGERUSB_CTRL2 register */
	ret = I2C_WRITE(data->client_battery, HW_REG_CHARGERUSB_CTRL2,
				CONST_CHRG_CTRL2);
	if (0 > ret)
		return ret;

	/* Set 3.8V for VSYS */
	ret = I2C_READ(data->client_battery, HW_REG_CONTROLLER_CTRL2);
	if (0 > ret)
		return ret;

	ret = I2C_WRITE(data->client_battery, HW_REG_CONTROLLER_CTRL2,
				ret | MSK_BIT_3);
	if (0 > ret)
		return ret;

	/* Setting for low voltage limitation */
	ret = I2C_WRITE(data->client_power, HW_REG_VSYSMIN_HI_THRESHOLD,
				CONST_VSYSMIN_HI);
	if (0 > ret)
		return ret;

	/* Set value for BAT_VOLT_THRESHOLD array */
	ret_vsysmin_hi = I2C_READ(data->client_power,
				HW_REG_VSYSMIN_HI_THRESHOLD);
	if (0 > ret_vsysmin_hi) {
		PMIC_ERROR_MSG("%s:%d read I2C failed err=%d\n", \
					__func__, __LINE__, ret_vsysmin_hi);
		return ret_vsysmin_hi;
	}

	for (i = 0; i < 100; i++)
		BAT_VOLT_THRESHOLD[i] = (short)((((VOREG[ret_voreg] -
		VSYS[ret_vsysmin_hi])*(i+1)) + 50)/100+VSYS[ret_vsysmin_hi]);

	/* Setting for low voltage limitation */
	ret = I2C_WRITE(data->client_power, HW_REG_VBATMIN_HI_THRESHOLD,
			CONST_VBATMIN_HI);
	if (0 > ret)
		return ret;

	/* Enable Charge current termination interrupt */
	ret = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_CTRL1);
	if (0 > ret)
		return ret;

	ret |= MSK_BIT_4;

	/* Set 1 to TERM bit at CHARGERUSB_CTRL1 (0xE8) register */
	ret = I2C_WRITE(data->client_battery, HW_REG_CHARGERUSB_CTRL1, ret);
	if (0 > ret)
		return ret;

	/* Enable charge one feature */
	ret = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_CTRL3);
	if (0 > ret)
		return ret;

	ret &= (~MSK_BIT_6);

	/* Set 0 to CHARGE_ONCE bit at CHARGERUSB_CTRL3 (0xEA) register */
	ret = I2C_WRITE(data->client_battery, HW_REG_CHARGERUSB_CTRL3, ret);
	if (0 > ret)
		return ret;

#ifdef PMIC_CHARGE_ENABLE
	/* Check status of battery voltage */
	ret_ovp = I2C_READ(data->client_battery, HW_REG_CHARGERUSB_STATUS_INT1);
	if (0 > ret_ovp)
		return ret_ovp;

	/* Enable charger if charger is present at the boot up of driver */
	ret_stat = I2C_READ(data->client_battery, HW_REG_CONTROLLER_STAT1);
	if (0 > ret_stat) {
		return ret_stat;
	} else {
		charger_stat = ret_stat & (MSK_BIT_2 | MSK_BIT_3);
		data->vbus_det = charger_stat & MSK_BIT_2;
		data->vac_det = charger_stat & MSK_BIT_3;
		if (0 != charger_stat) {
			ret = I2C_READ(data->client_battery,
					HW_REG_CONTROLLER_CTRL1);

			if (0 > ret)
				return ret;

			/* If battery is not over-temp and over-volt */
			if ((0 == (ret_stat & MSK_BIT_0))
			   && (0 == (ret_ovp & MSK_BIT_3))) {
				/* Enable charger */
				val = ret | MSK_BIT_4;
			} else {
				/* Do not enable charger */
			}

			/* Set source charge */
			if (0 == (charger_stat & MSK_BIT_3)) {
				/* If VAC Charger is not present */
				val = val & (~MSK_BIT_3);
			} else {
				/* If VAC Charger is present */
				val = val | MSK_BIT_3;
			}

			/* Enable charger */
			ret = I2C_WRITE(data->client_battery,
				HW_REG_CONTROLLER_CTRL1, val);
			if (0 > ret)
				return ret;

			/* Set default setting for current limit */
			ret = I2C_WRITE(data->client_battery,
			HW_REG_CHARGERUSB_CINLIMIT, CONST_DEF_CURRENT_LIMIT);
			if (0 > ret) {
				PMIC_DEBUG_MSG(
				"%s: I2C_WRITE failed err=%d\n", __func__, ret);
				return ret;
			}

		} else {
			ret = I2C_READ(data->client_battery,
				HW_REG_CONTROLLER_CTRL1);

			if (0 > ret)
				return ret;

			/* Disable charger */
			val = ret & (~(MSK_BIT_4));
			ret = I2C_WRITE(data->client_battery,
					HW_REG_CONTROLLER_CTRL1, val);
			if (0 > ret)
				return ret;
		}
	}
#endif
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}


/*
 * tps80032_init_irq: init interrupt for PMIC driver
 * @data: The struct which handles the TPS80032 data.
 * @irq: The ID interrupt of I2C driver
 * @irq_base: The ID Base interrupt of PMIC driver
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_init_irq(struct tps80032_data *data, int irq, int irq_base)
{
	int i, ret;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	if (!irq_base)
		return -EINVAL;

	/* Mask interrupt line signal A */
	ret = I2C_WRITE(data->client_battery, HW_REG_INT_MSK_LINE_STS_A,
				MSK_INT_LINE_A);
	if (0 > ret)
		return ret;

	/* Mask interrupt line signal B */
	ret = I2C_WRITE(data->client_battery, HW_REG_INT_MSK_LINE_STS_B,
				MSK_INT_LINE_B);
	if (0 > ret)
		return ret;

	/* Mask interrupt line signal C */
	ret = I2C_WRITE(data->client_battery, HW_REG_INT_MSK_LINE_STS_C,
				MSK_INT_LINE_C);
	if (0 > ret)
		return ret;

	/* Mask interrupt signal A */
	ret = I2C_WRITE(data->client_battery, HW_REG_INT_MSK_STS_A,
				MSK_INT_SRC_A);
	if (0 > ret)
		return ret;

	/* Mask interrupt signal B */
	ret = I2C_WRITE(data->client_battery, HW_REG_INT_MSK_STS_B,
				MSK_INT_SRC_B);
	if (0 > ret)
		return ret;

	/* Mask interrupt signal C */
	ret = I2C_WRITE(data->client_battery, HW_REG_INT_MSK_STS_C,
				MSK_INT_SRC_C);
	if (0 > ret)
		return ret;

	/* Mask value for CONTROLLER_INT register */
	ret = I2C_WRITE(data->client_battery, HW_REG_CONTROLLER_INT_MASK,
				MSK_CONTROLLER_INT);
	if (0 > ret)
		return ret;

	/* Mask value for CHARGERUSB_INT register */
	ret = I2C_WRITE(data->client_battery, HW_REG_CHARGERUSB_INT_MASK,
				MSK_CHARGERUSB_INT);
	if (0 > ret)
		return ret;

#ifdef CONFIG_USB_OTG
	/* Setting for USB ID interrupt */
	ret = I2C_WRITE(data->client_power, HW_REG_LDOUSB_CFG_STATE, 0x01);
	if (0 > ret)
		return ret;

	ret = I2C_WRITE(data->client_power, HW_REG_MISC2, 0x10);
	if (0 > ret)
		return ret;

	ret = I2C_WRITE(data->client_battery, HW_REG_USB_ID_CTRL_SET, 0x44);
	if (0 > ret)
		return ret;

	ret = I2C_WRITE(data->client_battery, HW_REG_USB_ID_EN_LO_SET, 0x01);
	if (0 > ret)
		return ret;

	ret = I2C_WRITE(data->client_battery, HW_REG_USB_ID_EN_HI_SET, 0x01);
	if (0 > ret)
		return ret;
#else
	/* Turn off LDOUSB */
	ret = I2C_WRITE(data->client_power, HW_REG_LDOUSB_CFG_STATE, 0x00);
	if (0 > ret)
		return ret;
#endif
	/* Clear all interrupt source */
	ret = I2C_WRITE(data->client_battery, HW_REG_INT_STS_A, MSK_DISABLE);
	if (0 > ret)
		return ret;

	ret = I2C_WRITE(data->client_battery, HW_REG_INT_STS_B, MSK_DISABLE);
	if (0 > ret)
		return ret;

	ret = I2C_WRITE(data->client_battery, HW_REG_INT_STS_C, MSK_DISABLE);
	if (0 > ret)
		return ret;

	ret = irq_alloc_descs(irq_base, irq_base, TPS80032_INT_NR, -1);
	if (ret < 1) {
		PMIC_ERROR_MSG("%s: unable to allocate %u irqs: %d\n",
					__func__, TPS80032_INT_NR, ret);
		if (ret == 0)
			ret = -EINVAL;
		return ret;
	}

	data->irq_base = ret;

	data->irq_chip.name = "tps80032-battery";
	data->irq_chip.irq_enable = tps80032_irq_enable;
	data->irq_chip.irq_disable = tps80032_irq_disable;
	data->irq_chip.irq_bus_lock = tps80032_irq_lock;
	data->irq_chip.irq_bus_sync_unlock = tps80032_irq_sync_unlock;

	for (i = 0; i < TPS80032_INT_NR; i++) {
		int __irq = i + data->irq_base;
		irq_set_chip_data(__irq, data);
		irq_set_chip_and_handler(__irq, &data->irq_chip,
					handle_simple_irq);
		irq_set_nested_thread(__irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(__irq, IRQF_VALID);
#endif
	}

	ret = request_threaded_irq(irq, NULL, tps80032_interrupt_handler,
				IRQF_SHARED, "tps80032", data);

	if (!ret)
		enable_irq_wake(irq);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * __remove_subdev: remove subdevice of PMIC TPS80032
 * @dev: The struct which handles the TPS80032 driver.
 * return:
 *        = 0: Normal operation
 */
static int __remove_subdev(struct device *dev, void *unused)
{
	platform_device_unregister(to_platform_device(dev));
	return 0;
}

/*
 * tps80032_remove_subdevs: remove subdevice of PMIC TPS80032
 * @data: The struct which handles the TPS80032 data.
 * return:
 *        = 0: Normal operation
 */
static int tps80032_remove_subdevs(struct tps80032_data *data)
{
	return device_for_each_child(data->dev, NULL, __remove_subdev);
}

/*
 * tps80032_add_subdevs: add subdevices of PMIC TPS80032
 * @data: The struct which handles the TPS80032 data.
 * @pdata: The struct which handles the TPS80032 flatform data.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int __devinit tps80032_add_subdevs(struct tps80032_data *data,
				struct tps80032_platform_data *pdata)
{
	struct tps80032_subdev_info *subdev;
	struct platform_device *pdev;
	int i, ret = 0;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	for (i = 0; i < pdata->num_subdevs; i++) {
		subdev = &pdata->subdevs[i];
		/* subdev->name = rtc-tps80032 */
		pdev = platform_device_alloc(subdev->name, subdev->id);

		pdev->dev.parent = data->dev;
		pdev->dev.platform_data = subdev->platform_data;

		ret = platform_device_add(pdev);
		if (ret)
			goto failed;
	}

	return 0;

failed:
	tps80032_remove_subdevs(data);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * tps80032_register_intput_event: register an input event for PWR_KEY
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_register_intput_event(void)
{
	int error;
	PMIC_DEBUG_MSG(">>> %s start\n", __func__);
	button_dev = input_allocate_device();
	if (!button_dev) {
		PMIC_ERROR_MSG("Not enough memory...\n");
		error = -ENOMEM;
		goto err_free_dev;
	}

	button_dev->name = "pmic_key";
	button_dev->phys = "gpio-keys/input0";
	button_dev->id.bustype = BUS_HOST;

	input_set_capability(button_dev, EV_KEY, KEY_POWER);

	error = input_register_device(button_dev);
	if (error) {
		PMIC_ERROR_MSG("Failed to register device\n");
		goto err_free_dev;
	}
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;

err_free_dev:
	input_free_device(button_dev);
	return error;
}

#ifdef PMIC_NON_VOLATILE_ENABLE
/*
 * tps80032_init_usb_id: Init value for USB ID interrupt
 * return: void
 */
static void tps80032_init_usb_id(void)
{
	struct tps80032_irq_data tps80032_irqs_tmp[] = {
		[TPS80032_INT_PWRON]		= TPS80032_IRQ(A, 0),
		[TPS80032_INT_RPWRON]		= TPS80032_IRQ(A, 1),
		[TPS80032_INT_SYS_VLOW]		= TPS80032_IRQ(A, 2),
		[TPS80032_INT_RTC_ALARM]	= TPS80032_IRQ(A, 3),
		[TPS80032_INT_RTC_PERIOD]	= TPS80032_IRQ(A, 4),
		[TPS80032_INT_HOT_DIE]		= TPS80032_IRQ(A, 5),
		[TPS80032_INT_VXX_SHORT]	= TPS80032_IRQ(A, 6),
		[TPS80032_INT_SPDURATION]	= TPS80032_IRQ(A, 7),
		[TPS80032_INT_WATCHDOG]		= TPS80032_IRQ(B, 0),
		[TPS80032_INT_BAT]		= TPS80032_IRQ(B, 1),
		[TPS80032_INT_SIM]		= TPS80032_IRQ(B, 2),
		[TPS80032_INT_MMC]		= TPS80032_IRQ(B, 3),
		[TPS80032_INT_RES]		= TPS80032_IRQ(B, 4),
		[TPS80032_INT_GPADC_RT]		= TPS80032_IRQ(B, 5),
		[TPS80032_INT_GPADC_SW2_EOC]	= TPS80032_IRQ(B, 6),
		[TPS80032_INT_CC_AUTOCAL]	= TPS80032_IRQ(B, 7),
		[TPS80032_INT_ID_WKUP]		= TPS80032_IRQ(C, 0),
		[TPS80032_INT_VBUSS_WKUP]	= TPS80032_IRQ(C, 1),
		[TPS80032_INT_ID]		= TPS80032_IRQ(C, 2),
		[TPS80032_INT_VBUS]		= TPS80032_IRQ(C, 3),
		[TPS80032_INT_CHRG_CTRL]	= TPS80032_IRQ(C, 4),
		[TPS80032_INT_EXT_CHRG]		= TPS80032_IRQ(C, 5),
		[TPS80032_INT_INT_CHRG]		= TPS80032_IRQ(C, 6),
		[TPS80032_INT_RES2]		= TPS80032_IRQ(C, 7),
		[TPS80032_INT_BAT_TEMP_OVRANGE]	= TPS80032_IRQ_SEC(C, 4,
					CHRG_CTRL, MBAT_TEMP, BAT_TEMP),
		[TPS80032_INT_BAT_REMOVED]	= TPS80032_IRQ_SEC(C, 4,
					CHRG_CTRL, MBAT_REMOVED, BAT_REMOVED),
		[TPS80032_INT_VBUS_DET]		= TPS80032_IRQ_SEC(C, 4,
					CHRG_CTRL, MVBUS_DET, VBUS_DET),
		[TPS80032_INT_VAC_DET]		= TPS80032_IRQ_SEC(C, 4,
					CHRG_CTRL, MVAC_DET, VAC_DET),
		[TPS80032_INT_FAULT_WDG]	= TPS80032_IRQ_SEC(C, 4,
					CHRG_CTRL, MFAULT_WDG, FAULT_WDG),
		[TPS80032_INT_LINCH_GATED]	= TPS80032_IRQ_SEC(C, 4,
					CHRG_CTRL, MLINCH_GATED, LINCH_GATED),
	};

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	memcpy((void *)&tps80032_irqs[0].mask_reg,
		(void *)&tps80032_irqs_tmp[0].mask_reg,
				sizeof(struct tps80032_irq_data)*30);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return;
}
#endif

/*
 * tps80032_power_probe: probe function for power supply management
 * @client: The I2C client device.
 * @id: The I2C ID.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_power_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret = 0;
	int count = 0;
	int i = 0;
	int j = 0;
	char *ldo_name[] = {
			"ldo1", "ldo2", "ldo3", "ldo4",
			"ldo5", "ldo6", "ldo7"
			};

	struct tps80032_platform_data *pdata = client->dev.platform_data;

	PMIC_DEBUG_MSG(">>>%s: name=%s addr=0x%x\n",
		__func__, client->name, client->addr);

	if (!pdata) {
		PMIC_ERROR_MSG("Tps80032 requires platform data\n");
		return -ENOTSUPP;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (NULL == data) {
		PMIC_ERROR_MSG("%s:%d kzalloc failed err=%d\n",\
			__func__, __LINE__, ret);
		return -ENOMEM;
	}

	/* init initial value */
	mutex_init(&data->smps1_lock);
	mutex_init(&data->smps2_lock);
	mutex_init(&data->smps3_lock);
	mutex_init(&data->smps4_lock);
	mutex_init(&data->smps5_lock);
	mutex_init(&data->ldo1_lock);
	mutex_init(&data->ldo2_lock);
	mutex_init(&data->ldo3_lock);
	mutex_init(&data->ldo4_lock);
	mutex_init(&data->ldo5_lock);
	mutex_init(&data->ldo6_lock);
	mutex_init(&data->ldo7_lock);
	mutex_init(&data->vbus_lock);
	mutex_init(&data->force_off_lock);
	mutex_init(&data->rscounter_lock);
	mutex_init(&data->irq_lock);
	spin_lock_init(&data->pmic_lock);

	tps80032_modem_reset_thread = NULL;
	key_count = 0;
	num_volt = 0;

	/* Init flags */
	suspend_flag = 0;
	interrupt_flag = 0;

	data->device = E_DEVICE_NONE;
	data->charger = 0;
	data->bat_volt = 0;
	data->bat_temp = 0;
	data->hpa_temp = 0;
	data->bat_over_volt = 0;
	data->bat_over_temp = 0;
	data->bat_capacity = 0;
	data->bat_presence = 0;
	data->en_charger = 0;
	data->cin_limit = 0;
	data->vbus_det = 0;
	data->vac_det = 0;

	while (count < RESOURCE_COUNTER_MAX) {
		data->rscounter[count] = 0;
		count = count + 1;
	}

	/* Init value for LDO SysFs control */
	for (i = 0; i < 7; i++) {
		state_sleep[i] = 0;
		volt_sleep[i] = 0;
	}

	LDO_STATE_REG[0] = HW_REG_LDO1_CFG_STATE;
	LDO_STATE_REG[1] = HW_REG_LDO2_CFG_STATE;
	LDO_STATE_REG[2] = HW_REG_LDO3_CFG_STATE;
	LDO_STATE_REG[3] = HW_REG_LDO4_CFG_STATE;
	LDO_STATE_REG[4] = HW_REG_LDO5_CFG_STATE;
	LDO_STATE_REG[5] = HW_REG_LDO6_CFG_STATE;
	LDO_STATE_REG[6] = HW_REG_LDO7_CFG_STATE;
	LDO_VOLT_REG[0] = HW_REG_LDO1_CFG_VOLTAGE;
	LDO_VOLT_REG[1] = HW_REG_LDO2_CFG_VOLTAGE;
	LDO_VOLT_REG[2] = HW_REG_LDO3_CFG_VOLTAGE;
	LDO_VOLT_REG[3] = HW_REG_LDO4_CFG_VOLTAGE;
	LDO_VOLT_REG[4] = HW_REG_LDO5_CFG_VOLTAGE;
	LDO_VOLT_REG[5] = HW_REG_LDO6_CFG_VOLTAGE;
	LDO_VOLT_REG[6] = HW_REG_LDO7_CFG_VOLTAGE;

	/* Init value for LDO_VOLT_VALUE struct */
	for (i = 0; i < 24; i++)
		if (0 == i)
			LDO_VOLT_VALUE[i] = 1000;
		else
			LDO_VOLT_VALUE[i] = LDO_VOLT_VALUE[i-1] + 100;

	data->dev = &client->dev;

	/* Set client power */
	data->client_power = client;

	/* Create work queue */
	data->queue  = create_singlethread_workqueue("tps80032_int_dev_queue");
	if (!data->queue) {
		ret = -ENOMEM;
		PMIC_ERROR_MSG(
			"%s:%d create_singlethread_workqueue failed err=%d\n",
				__func__, __LINE__, ret);
		goto err_workqueue_alloc;
	}

	/* init work queue */
#ifdef PMIC_FUELGAUGE_ENABLE
	INIT_WORK(&data->resume_work, tps80032_resume_work);
	INIT_WORK(&data->update_work, tps80032_update_work);
#endif

	i2c_set_clientdata(client, data);

#ifdef PMIC_NON_VOLATILE_ENABLE
	virt_addr = ioremap(MAP_BASE_NV + MAP_BASE_PMIC_NV, MAP_SIZE_PMIC_NV);
	if (!virt_addr) {
		ret = -ENOMEM;
		goto err_ioremap_virt;
	}

	/* Init value for USB ID interrupt */
	tps80032_init_usb_id();
#endif

	/* Register into PMIC interface */
	ret = pmic_device_register(&client->dev, &tps80032_power_ops);
	if (0 > ret) {
		PMIC_ERROR_MSG("%s:%d pmic_device_register failed err=%d\n", \
					__func__, __LINE__, ret);
		goto err_device_register;
	}


	 /*patch*/
	 tps80032_clk32k_enable(CLK32KG, TPS80032_STATE_ON);

	/* Init hardware */
	ret = tps80032_init_power_hw(data);
	if (0 > ret) {
		PMIC_ERROR_MSG("%s:%d tps80032_init_power_hw failed err=%d\n", \
					__func__, __LINE__, ret);
		goto err_init_hw;
	}

	/* Add subdevice */
	ret = tps80032_add_subdevs(data, pdata);
	if (ret) {
		PMIC_ERROR_MSG("%s:%d add devices failed err=%d\n", \
					__func__, __LINE__, ret);
		goto err_init_hw;
	}

	/*Initililaze all ldo power domain*/
	for (i = 0; i < RESOURCE_POWER_CTRL_PROP_MAX; i++) {
		control_ldo[i].properties = pmic_power_props;
		control_ldo[i].num_properties = ARRAY_SIZE(pmic_power_props);
		control_ldo[i].get_property = tps80032_power_get_property;
		control_ldo[i].name = ldo_name[i];
		control_ldo[i].set_property = tps80032_power_set_property;
		control_ldo[i].property_is_writeable =
					tps80032_power_is_writeable;

		ret = power_ctrl_register(&client->dev, &control_ldo[i]);
		if (ret)
			goto err_init_power_contrl;
	}

	/* Register input event */
	ret = tps80032_register_intput_event();
	if (ret) {
		PMIC_ERROR_MSG("%s:%d Register input event failed err=%d\n", \
				__func__, __LINE__, ret);
		goto err_init_input_event;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;

err_init_input_event:
	for (j = 0; j < i; j++)
		power_ctrl_unregister(&control_ldo[j]);
err_init_hw:
err_init_power_contrl:
	pmic_device_unregister(&client->dev);
err_device_register:
#ifdef PMIC_NON_VOLATILE_ENABLE
	iounmap(virt_addr);
	virt_addr = NULL;
err_ioremap_virt:
#endif
	destroy_workqueue(data->queue);
err_workqueue_alloc:
	kfree(data);
	return ret;
}

/*
 * tps80032_power_remove: remove function for power supply management
 * @client: The I2C client device.
 * return:
 *        = 0: Normal operation
 */
static int tps80032_power_remove(struct i2c_client *client)
{
	struct tps80032_data *data = i2c_get_clientdata(client);
	struct tps80032_platform_data *pdata = client->dev.platform_data;
	int i = 0;

	/*Release ldo power control*/
	for (i = 0; i < RESOURCE_POWER_CTRL_PROP_MAX; i++)
		power_ctrl_unregister(&control_ldo[i]);

	gpio_free(pdata->pin_gpio[1]);	/* free GPIO_PORT28 */
	free_irq(pint2irq(CONST_INT_ID), data);	/* free interrupt */
	pmic_device_unregister(&client->dev);
	destroy_workqueue(data->queue);
	kfree(data);
	/* Free input device */
	input_unregister_device(button_dev);
	input_free_device(button_dev);
	button_dev = NULL;
	return 0;
}


/*
 * tps80032_battery_probe: probe function for battery management
 * @client: The I2C client device.
 * @id: The I2C ID.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_battery_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret = 0;

	PMIC_DEBUG_MSG(">>> %s: name=%s addr=0x%x\n", \
				__func__, client->name, client->addr);

	if (NULL == data) {
		PMIC_ERROR_MSG("%s:%d data is failed in location\n", \
				__func__, __LINE__);
		return -ENOMEM;
	}

	/* Set client battery and data*/
	data->client_battery = client;
	i2c_set_clientdata(client, data);

#ifdef PMIC_FUELGAUGE_ENABLE
	/* Init hardware configuration*/
	ret = tps80032_init_battery_hw(data);
	if (0 > ret) {
		PMIC_ERROR_MSG("%s: init_battery_hw failed err=%d\n", \
				__func__, ret);
		goto err_init_hw;
	}
#endif

	/* Register into USB OTG VBUS interface */
	ret = usb_otg_pmic_device_register(&client->dev, &tps80032_vbus_ops);
	if (0 > ret) {
		PMIC_ERROR_MSG("%s: usb_otg register failed err=%d\n", \
				__func__, ret);
		goto err_device_USB_register;
	}

#ifdef PMIC_FUELGAUGE_ENABLE
	/* Register battery device */
	ret = pmic_battery_device_register(&client->dev,
					&tps80032_power_battery_ops);
	if (0 > ret) {
		PMIC_ERROR_MSG("%s: pmic battery register failed err=%d\n", \
				__func__, ret);
		goto err_battery_device_register;
	}
#endif

#ifdef PMIC_FUELGAUGE_ENABLE
	/* Register correct device */
	ret = pmic_battery_register_correct_func(&tps80032_correct_ops);
	if (0 > ret) {
		PMIC_ERROR_MSG("%s: failed err=%d\n", __func__, ret);
		goto err_correct_device_register;
	}
#endif

	/* Init IRQ*/
	ret = tps80032_init_irq(data, client->irq, IRQPIN_IRQ_BASE + 64);
	if (0 > ret) {
		PMIC_ERROR_MSG("%s:%d tps80032_init_irq failed err=%d\n",\
				__func__, __LINE__, ret);
		goto err_request_irq;
	}

#ifdef PMIC_FUELGAUGE_ENABLE
	tps80032_init_timer(data);
	bat_timer.expires  = jiffies +
			msecs_to_jiffies(CONST_TIMER_UPDATE);
	add_timer(&bat_timer);

#endif

	/* Init thread to handle modem reset */
	init_waitqueue_head(&tps80032_modem_reset_event);
	tps80032_modem_reset_thread = kthread_run(tps80032_modem_thread,
					NULL, "tps80032_modem_reset_thread");
	if (NULL == tps80032_modem_reset_thread) {
		ret = -ENOMEM;
		PMIC_ERROR_MSG("%s:%d tps80032_modem_reset_thread failed\n",\
				__func__, __LINE__);
		goto err_request_irq;
	}


	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;

err_request_irq:
	free_irq(pint2irq(CONST_INT_ID), data);
#ifdef PMIC_FUELGAUGE_ENABLE
err_init_hw:
	pmic_battery_unregister_correct_func();
err_correct_device_register:
	pmic_battery_device_unregister(&client->dev);
err_battery_device_register:
#endif
	usb_otg_pmic_device_unregister(&client->dev);
err_device_USB_register:
	return ret;
}

/*
 * tps80032_battery_remove: remove function for battery management
 * @client: The I2C client device.
 * return:
 *        = 0: Normal operation
 */
static int tps80032_battery_remove(struct i2c_client *client)
{

	/* Stop thread handling modem reset */
	if (tps80032_modem_reset_thread != NULL) {
		kthread_stop(tps80032_modem_reset_thread);
		tps80032_modem_reset_thread = NULL;
	}

#ifdef PMIC_FUELGAUGE_ENABLE
	pmic_battery_unregister_correct_func();
	pmic_battery_device_unregister(&client->dev);
#endif

	usb_otg_pmic_device_unregister(&client->dev);
	return 0;
}

/*
 * tps80032_dvs_probe: probe function for DVS driver
 * @client: The I2C client device.
 * @id: The I2C ID.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_dvs_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	PMIC_DEBUG_MSG(">>>%s: name=%s addr=0x%x\n",\
			__func__, client->name, client->addr);

	if (NULL == data) {
		PMIC_ERROR_MSG("%s:%d data is failed in location\n",\
				__func__, __LINE__);
		return -ENOMEM;
	}

	/* Set client dvs and data*/
	data->client_dvs = client;
	i2c_set_clientdata(client, data);

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;
}

/*
 * tps80032_dvs_remove: remove function for DVS driver
 * @client: The I2C client device.
 * return:
 *        = 0: Normal operation
 */
static int tps80032_dvs_remove(struct i2c_client *client)
{
	return 0;
}

/*
 * tps80032_jtag_probe: probe function for Jtag driver
 * @client: The I2C client device.
 * @id: The I2C ID.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
static int tps80032_jtag_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	PMIC_DEBUG_MSG(">>>%s: name=%s addr=0x%x\n", __func__, \
				client->name, client->addr);

	if (NULL == data) {
		PMIC_ERROR_MSG("%s:%d data is failed in location\n",\
				__func__, __LINE__);
		return -ENOMEM;
	}

	/* Set client dvs and data*/
	data->client_jtag = client;
	i2c_set_clientdata(client, data);
	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

	return 0;
}

/* Init value for LDO SysFS control */
static struct device_attribute power_ctrl_attrs[] = {
	POWER_CTRL_ATTR(state),
	POWER_CTRL_ATTR(state_sleep),
	POWER_CTRL_ATTR(voltage),
	POWER_CTRL_ATTR(voltage_sleep),
};

static struct attribute_group power_ctrl_attr_group = {
	.attrs = __power_ctrl_attrs,
	.is_visible = power_ctrl_attr_is_visible,
};

static struct attribute *
	__power_ctrl_attrs[ARRAY_SIZE(power_ctrl_attrs) + 1];

static const struct attribute_group *power_ctrl_attr_groups[] = {
	&power_ctrl_attr_group,
	NULL,
};

/*
 * power_ctrl_init_attrs: initialize attribute for all ldo
 * @dev_type: Device type.
 * return: void
 */
void power_ctrl_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = power_ctrl_attr_groups;

	for (i = 0; i < ARRAY_SIZE(power_ctrl_attrs); i++)
		__power_ctrl_attrs[i] = &power_ctrl_attrs[i].attr;
}


/*
 * tps80032_jtag_remove: remove function for Jtag driver
 * @client: The I2C client device.
 * return:
 *        = 0: Normal operation
 */
static int tps80032_jtag_remove(struct i2c_client *client)
{
	return 0;
}

/* Struct I2C ID for power supply driver */
static const struct i2c_device_id tps80032_power_id[] = {
	{ "tps80032-power", 0 }, {{0}, 0},
};

/* Struct I2C ID for battery driver */
static const struct i2c_device_id tps80032_battery_id[] = {
	{ "tps80032-battery", 0 }, {{0}, 0},
};

/* Struct I2C ID for DVS driver */
static const struct i2c_device_id tps80032_dvs_id[] = {
	{ "tps80032-dvs", 0 }, {{0}, 0},
};

/* Struct I2C ID for JTAG driver */
static const struct i2c_device_id tps80032_jtag_id[] = {
	{ "tps80032-jtag", 0 }, {{0}, 0},
};

/* Struct power device ops for power supply driver */
static const struct dev_pm_ops tps80032_power_pm_ops = {
	.suspend	= tps80032_power_suspend,
	.resume		= tps80032_power_resume,
};

/* Struct I2C driver for power supply driver */
static struct i2c_driver tps80032_power_driver = {
	.driver = {
		.name	= "tps80032-power",
		.pm = &tps80032_power_pm_ops,
	},
	.probe		= tps80032_power_probe,
	.remove		= tps80032_power_remove,
	.id_table	= tps80032_power_id,
};

/* Struct I2C driver for battery driver */
static struct i2c_driver tps80032_battery_driver = {
	.driver = {
		.name	= "tps80032-battery",
	},
	.probe		= tps80032_battery_probe,
	.remove		= tps80032_battery_remove,
	.id_table	= tps80032_battery_id,
};

/* Struct I2C driver for DVS driver */
static struct i2c_driver tps80032_dvs_driver = {
	.driver = {
		.name	= "tps80032-dvs",
	},
	.probe		= tps80032_dvs_probe,
	.remove		= tps80032_dvs_remove,
	.id_table	= tps80032_dvs_id,
};

/* Struct I2C driver for JTAG driver */
static struct i2c_driver tps80032_jtag_driver = {
	.driver = {
		.name	= "tps80032-jtag",
	},
	.probe		= tps80032_jtag_probe,
	.remove		= tps80032_jtag_remove,
	.id_table	= tps80032_jtag_id,
};

/*
 * tps80032_power_init: Initialize all resource of this module
 * @void
 * return:
 *        = 0: Normal termination
 *        < 0: Error occurs
 */
static int __init tps80032_power_init(void)
{
	int ret;

	PMIC_DEBUG_MSG(">>> %s start\n", __func__);

	/*Initialize hw spinlock*/
	r8a73734_hwlock_pmic = hwspin_lock_request_specific(SMGP000_PMIC);
	if (r8a73734_hwlock_pmic == NULL) {
		PMIC_ERROR_MSG(
		"Unable to register hw spinlock for pmic driver\n");
		return -EIO;
	}

	power_ctrl_class = class_create(THIS_MODULE, "power_control");
	if (power_ctrl_class == NULL) {
		PMIC_ERROR_MSG("Unable to create ldo_control class\n");
		ret = -EIO;
		goto err_power_ctrl_class;
	}

	power_ctrl_class->dev_uevent = power_ctrl_uevent;
	power_ctrl_init_attrs(&power_ctrl_dev_type);

	ret = i2c_add_driver(&tps80032_power_driver);
	if (0 != ret) {
		PMIC_ERROR_MSG("Unable to register tps80032_power driver\n");
		goto err_power_driver;
	}

	ret = i2c_add_driver(&tps80032_battery_driver);
	if (0 != ret) {
		PMIC_ERROR_MSG("Unable to register tps80032_battery driver\n");
		goto err_battery_driver;
	}

	ret = i2c_add_driver(&tps80032_dvs_driver);
	if (0 != ret) {
		PMIC_ERROR_MSG("Unable to register tps80032_dvs driver\n");
		goto err_dvs_driver;
	}

	ret = i2c_add_driver(&tps80032_jtag_driver);
	if (0 != ret) {
		PMIC_ERROR_MSG("Unable to register tps80032_jtag driver\n");
		goto err_jtag_driver;
	}

	PMIC_DEBUG_MSG("%s end <<<\n", __func__);

	return 0;

err_jtag_driver:
	i2c_del_driver(&tps80032_dvs_driver);
err_dvs_driver:
	i2c_del_driver(&tps80032_battery_driver);
err_battery_driver:
	i2c_del_driver(&tps80032_power_driver);
err_power_driver:
	class_destroy(power_ctrl_class);
err_power_ctrl_class:
	hwspin_lock_free(r8a73734_hwlock_pmic);
	return ret;
}

/*
 * tps80032_power_exit: free all resources of this module
 * @void
 * return: void
 */
static void __exit tps80032_power_exit(void)
{
	i2c_del_driver(&tps80032_power_driver);
	i2c_del_driver(&tps80032_battery_driver);
	i2c_del_driver(&tps80032_dvs_driver);
	i2c_del_driver(&tps80032_jtag_driver);
	class_destroy(power_ctrl_class);
	hwspin_lock_free(r8a73734_hwlock_pmic);
}

subsys_initcall(tps80032_power_init);
module_exit(tps80032_power_exit);

MODULE_DESCRIPTION("TPS80032 power department driver");
MODULE_LICENSE("GPL");
