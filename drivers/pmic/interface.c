/*
 * drivers/pmic/interface.c
 *
 * PMIC Interface
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

#include <linux/slab.h>
#include <linux/pmic/pmic.h>
#include <linux/io.h>

#include <mach/common.h>

static LIST_HEAD(driver_work_list);
static DEFINE_SPINLOCK(pmic_lock);

struct pmic_device {
	struct device dev;
	struct pmic_device_ops *ops;
};

static struct pmic_device *pmic_dev;


/*
 * pmic_set_power_on: turn on a power resource
 * @resource: the resource to be turned on
 * return:
 *         =0: normal
 *         <0: error
 */

int pmic_set_power_on(int resource)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->set_power_on))
		return -ENODEV;

	ret = pmic_dev->ops->set_power_on(pmic_dev->dev.parent, resource);
	return ret;
}
EXPORT_SYMBOL_GPL(pmic_set_power_on);

/*
 * pmic_set_power_off: turn off a power resource
 * @resource: the resource to be turned off
 * return:
 *         =0: normal
 *         <0: error
 */
int pmic_set_power_off(int resource)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->set_power_off))
		return -ENODEV;

	ret = pmic_dev->ops->set_power_off(pmic_dev->dev.parent, resource);
	return ret;
}
EXPORT_SYMBOL_GPL(pmic_set_power_off);

/*
 * pmic_force_power_off: force off power resources
 * @resource: the power resource to turn off
 * return:
 *         =0: normal
 *         <0: error
 */
void pmic_force_power_off(int resource)
{

  if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->force_power_off)) {
		printk(KERN_ERR "%s: error -ENODEV\n", __func__);
		return;
  }

	pmic_dev->ops->force_power_off(pmic_dev->dev.parent, resource);
}
EXPORT_SYMBOL_GPL(pmic_force_power_off);

/*
 * pmic_set_voltage: set voltage to a power resource
 * @resource: the resource to be set
 * @voltage: the specified voltage
 * return:
 *         =0: normal
 *         <0: error
 */
int pmic_set_voltage(int resource, int voltage)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->set_voltage))
		return -ENODEV;

	ret = pmic_dev->ops->set_voltage(pmic_dev->dev.parent,
					resource, voltage);
	return ret;
}
EXPORT_SYMBOL_GPL(pmic_set_voltage);

/*
 * pmic_set_power_mode: set power mode to a power resource
 * @resource: the resource to be set
 * @pstate: the power state to set the power mode
 * @pmode: the specified power mode
 * return:
 *         =0: normal
 *         <0: error
 */
int pmic_set_power_mode(int resource, int pstate, int pmode)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->set_power_mode))
		return -ENODEV;

	ret = pmic_dev->ops->set_power_mode(pmic_dev->dev.parent,
					resource, pstate, pmode);
	return ret;
}
EXPORT_SYMBOL_GPL(pmic_set_power_mode);


/*
 * pmic_get_ext_device: get USB devices detected
 * Input: none
 * return:
 *         >=0: the connected devices
 *         <0: error
*/
int pmic_get_ext_device(void)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->get_ext_device))
		return E_DEVICE_NONE;

	ret = pmic_dev->ops->get_ext_device(pmic_dev->dev.parent);

	return ret;
}
EXPORT_SYMBOL_GPL(pmic_get_ext_device);

/*
 * pmic_set_current_limit: set the current limit for charging operation
 * @chrg_state: The charger state: (1/0)
 * @chrg_type: The charger type
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
int pmic_set_current_limit(int chrg_state, int chrg_type)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->set_current_limit))
		ret = -ENODEV;

	ret = pmic_dev->ops->set_current_limit(pmic_dev->dev.parent,
					chrg_state, chrg_type);
	return ret;
}
EXPORT_SYMBOL_GPL(pmic_set_current_limit);

/*
 * pmic_clk32k_enable: Turn oscillation clock to ON/OFF
 * @clk_res: Clock resource id: CLK32KAO(0), CLK32KG(1), CLK32KAUDIO(2)
 * @state: Clock status: TPS80032_STATE_ON(1), TPS80032_STATE_OFF(0)
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
int pmic_clk32k_enable(u8 clk_res, u8 state)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->clk32k_enable))
		ret = -ENODEV;

	ret = pmic_dev->ops->clk32k_enable(clk_res, state);
	return ret;
}
EXPORT_SYMBOL_GPL(pmic_clk32k_enable);

/*
 * pmic_read_register: read the current value of TPS80032 register
 * @addr: The address of register.
 * @slave: The I2C slave address.
 * return:
 *        >= 0: Value of register
 *        < 0: Error occurs
 */
int pmic_read_register(int slave, u8 addr)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->read_register))
		ret = -ENODEV;

	ret = pmic_dev->ops->read_register(pmic_dev->dev.parent, slave, addr);

	return ret;
}
EXPORT_SYMBOL_GPL(pmic_read_register);

/*
 * pmic_read: read the current value of TPS80032 register
 * @dev: The struct which handles the RTC TPS80032 driver.
 * @addr: The address of register.
 * @val: The return value of register.
 * return:
 *        >= 0: Value of register
 *        < 0: Error occurs
 */
int pmic_read(struct device *dev, u8 addr, uint8_t *val)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->read))
		ret = -ENODEV;

	ret = pmic_dev->ops->read(dev, addr);
	if (0 <= ret) {
		*val = ret;
		return 0;
	} else
		return ret;

}
EXPORT_SYMBOL_GPL(pmic_read);


/*
 * pmic_reads: read the current value of TPS80032 registers
 * @dev: The struct which handles the RTC TPS80032 driver.
 * @addr: The address of register.
 * @len: The number of registers is read.
 * @val: The value of TPS80032 registers is read.
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */
int pmic_reads(struct device *dev, u8 addr, int len, u8* val)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->reads))
		ret = -ENODEV;

	ret = pmic_dev->ops->reads(dev, addr, len, val);

	if (0 <= ret)
		return 0;
	else
		return ret;

}
EXPORT_SYMBOL_GPL(pmic_reads);

/*
 * pmic_write: write value to PMIC hardware register
 * @dev: The struct which handles the RTC TPS80032 driver.
 * @addr: The address of register.
 * @value: The value is written to PMIC hardware register
 * return:
 *        = 0: Normal termination
 *        < 0: Error occurs
 */
int pmic_write(struct device *dev, u8 addr, u8 value)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->write))
		ret = -ENODEV;

	ret = pmic_dev->ops->write(dev, addr, value);

	if (0 <= ret)
		return 0;
	else
		return ret;

}
EXPORT_SYMBOL_GPL(pmic_write);

/*
 * pmic_writes: write value to PMIC hardware registers
 * @dev: The struct which handles the RTC TPS80032 driver.
 * @addr: The address of register.
 * @len: The number of registers is wrote
 * @value: The value is written to PMIC hardware register
 * return:
 *        = 0: Normal termination
 *        < 0: Error occurs
 */
int pmic_writes(struct device *dev, u8 addr, int len, u8 *value)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->writes))
		ret = -ENODEV;

	ret = pmic_dev->ops->writes(dev, addr, len, value);

	if (0 <= ret)
		return 0;
	else
		return ret;

}
EXPORT_SYMBOL_GPL(pmic_writes);

/*
 * pmic_get_temp_status: read the current temperature of battery
 * input: void
 * return:
 *        > 0: the battery temperature
 *        <= 0: Error occurs
 */
int pmic_get_temp_status(void)
{
	int ret;
	if (pmic_dev && pmic_dev->ops && pmic_dev->ops->get_temp_status)
		ret = pmic_dev->ops->get_temp_status(pmic_dev->dev.parent);
	else
		ret = -ENODEV;

	return ret;
}
EXPORT_SYMBOL_GPL(pmic_get_temp_status);


/*
 * pmic_add_wait_queue: add a pmic_wait_queue object to double
 * linked list of PMIC driver
 * @queue: pmic_wait_queue structure object
 * return: void
 */
void pmic_add_wait_queue(struct pmic_wait_queue *queue)
{
	spin_lock(&pmic_lock);
	list_add_tail(&queue->list, &driver_work_list);
	spin_unlock(&pmic_lock);
}
EXPORT_SYMBOL_GPL(pmic_add_wait_queue);


/*
 * pmic_ext_device_changed: get the external device
 * @device: external device id
 * return: void
 */
void pmic_ext_device_changed(int device)
{
	struct pmic_wait_queue *queue;
	struct list_head *p;
	if (pmic_dev) {
		spin_lock(&pmic_lock);
		list_for_each(p, &driver_work_list) {
			queue = list_entry(p, struct pmic_wait_queue, list);
			if (device & queue->unmask) {
				if (queue->work_queue)
					queue_work(queue->work_queue,
								&queue->work);
				else
					schedule_work(&queue->work);

			}
		}
		spin_unlock(&pmic_lock);
	}
}
EXPORT_SYMBOL_GPL(pmic_ext_device_changed);

#ifdef PMIC_PT_TEST_ENABLE

/*
 * pmic_read_battery_status: read battery status at present
 * @property: Battery property which wants to receive status
 * return:
 *        >= 0: return value
 *        < 0: error occurs
 */
int pmic_read_battery_status(int property)
{
	int ret;
	if (!(pmic_dev && pmic_dev->ops && pmic_dev->ops->get_batt_status))
		return -ENODEV;

	ret = pmic_dev->ops->get_batt_status(pmic_dev->dev.parent, property);

	return ret;
}
EXPORT_SYMBOL_GPL(pmic_read_battery_status);

#endif

/*
 * pmic_device_register: register a device to PMIC interface
 * @dev: The struct which handles the device is registered to PMIC interface.
 * @ops: struct pmic_device_ops
 * return:
 *        = 0: Registration is successful
 *        < 0: Registration is failed
 */
int pmic_device_register(struct device *dev, struct pmic_device_ops *ops)
{
	int ret;

	if (pmic_dev) {
		ret = -EALREADY;
		goto err_device_register_already;
	}

	pmic_dev = kzalloc(sizeof(*pmic_dev), GFP_KERNEL);
	if (!pmic_dev) {
		ret = -ENOMEM;
		goto err_device_alloc_failed;
	}

	dev_set_name(&pmic_dev->dev, "pmic_interface%d", 0);
	pmic_dev->ops = ops;
	pmic_dev->dev.parent = dev;

	ret = device_register(&pmic_dev->dev);
	if (ret < 0)
		goto err_device_register_failed;

	return ret;

err_device_register_failed:
	kfree(pmic_dev);
err_device_alloc_failed:
err_device_register_already:
	return ret;
}
EXPORT_SYMBOL_GPL(pmic_device_register);

/*
 * pmic_device_unregister: unregister a device to PMIC interface
 * @dev: The struct which handles the device is unregistered to PMIC interface.
 * return:
 *        = 0: Unregistration is successful
 */
int pmic_device_unregister(struct device *dev)
{
	struct pmic_wait_queue *queue;
	struct list_head *p;

	spin_lock(&pmic_lock);
	list_for_each(p, &driver_work_list) {
		queue = list_entry(p, struct pmic_wait_queue, list);
		if (queue->work_queue)
			flush_workqueue(queue->work_queue);
		else
			flush_scheduled_work();

		list_del(&queue->list);
		kfree(queue);
	}
	spin_unlock(&pmic_lock);

	device_unregister(&pmic_dev->dev);
	kfree(pmic_dev);
	return 0;
}
EXPORT_SYMBOL_GPL(pmic_device_unregister);

MODULE_DESCRIPTION("PowerManagementIC interface functions");
MODULE_LICENSE("GPL");

