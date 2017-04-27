/*
 * TPS80032 RTC Driver
 *
 * Copyright (C) 2012 Renesas Electronics Corporation
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <mach/r8a7373.h>
#include <linux/slab.h>
#include <linux/pmic/pmic.h>

#ifdef CONFIG_PMIC_INTERFACE
	#include <linux/pmic/pmic-tps80032.h>
	#define rtc_read(dev, addr, val) \
				pmic_read(dev, addr, val)

	#define rtc_write(dev, addr, val) \
				pmic_write(dev, addr, val)

	#define rtc_reads(dev, addr, len, val) \
				pmic_reads(dev, addr, len, val)

	#define rtc_writes(dev, addr, len, val) \
				pmic_writes(dev, addr, len, val)
#else
	#include <linux/mfd/tps80031.h>
	#define rtc_read(dev, addr, val) \
				tps80031_read(dev, 1, addr, val)

	#define rtc_write(dev, addr, val) \
				tps80031_write(dev, 1, addr, val)

	#define rtc_reads(dev, addr, len, val) \
				tps80031_reads(dev, 1, addr, len, val)

	#define rtc_writes(dev, addr, len, val) \
				tps80031_writes(dev, 1, addr, len, val)
#endif


#define RTC_SECONDS_REG			0x00
#define RTC_ALARM_SECONDS_REG	0x08
#define RTC_CTRL_REG			0x10
#define RTC_STATUS_REG			0x11
#define RTC_INTERRUPTS_REG		0x12
#define RTC_RESET_STATUS_REG	0x16

#define STOP_RTC				0x01
#define ALARM_INT_STATUS		0x40
#define ENABLE_ALARM_INT		0x08

/* Based year that RTC system refers is 1900,
 * 100 is used as OFFSET in TPS80032 to set the initial year is 2000
*/
#define RTC_YEAR_OFFSET			100
#define RTC_POR_YEAR			0
#define RTC_POR_MONTH			1
#define RTC_POR_DAY				1

static DEFINE_MUTEX(set_alarm_lock);
static DEFINE_MUTEX(set_time_lock);
static DEFINE_MUTEX(cntrol1_lock);
static DEFINE_MUTEX(cntrol2_lock);

struct rtc_tps80032_data {
	struct rtc_device *rtc;
	struct device *dev;
};

static int tps80032_alarm_irq_enable(struct device *dev, unsigned int enabled);

/*
 * tps80032_rtc_stop: Stop RTC Hardware
 * @dev:  i2c_client device
 * return:
 *        0: Normal operation
 *       <0: Error occurs
 */

static int tps80032_rtc_stop(struct device *dev)
{
	int ret;
	u8 val;
	u8 reg;

	dev_dbg(dev->parent, ">>> %s start\n", __func__);

	/*Read control register*/
	ret = rtc_read(dev->parent, RTC_CTRL_REG, &reg);
	if (ret < 0) {
		dev_err(dev->parent, "failed to read RTC_CTRL reg\n");
		return ret;
	}
	val = (u8) reg & (~STOP_RTC);

	/*Stop RTC*/
	ret = rtc_write(dev->parent, RTC_CTRL_REG, val);
	if (ret < 0) {
		dev_err(dev->parent, "failed to stop RTC\n");
		return ret;
	}

	dev_dbg(dev->parent, "<<< %s end\n", __func__);
	return ret;
}

/*
 * tps80032_rtc_start: Start RTC Harware
 * @dev:  i2c_client device
 * return:
 *        0: Normal operation
 *       <0: Error occurs
 */

static int tps80032_rtc_start(struct device *dev)
{
	int ret;
	u8 reg;
	u8 val;

	dev_dbg(dev->parent, ">>> %s start\n", __func__);

	/*Read control register*/
	ret = rtc_read(dev->parent, RTC_CTRL_REG, &reg);
	if (ret < 0) {
		dev_err(dev->parent, "failed to read RTC_CTRL reg\n");
		return ret;
	}

	val = (u8) reg | STOP_RTC;

	/*Start RTC*/
	ret = rtc_write(dev->parent, RTC_CTRL_REG, val);
	if (ret < 0) {
		dev_err(dev->parent, "failed to stop RTC\n");
		return ret;
	}

	dev_dbg(dev->parent, "<<< %s end\n", __func__);
	return ret;
}

/*
 * tps80032_rtc_irq: Interrupt handler
 * @irq: interrupt ID request
 * @dev_id: device ID
 * return:
 *        IRQ_HANDLED: always return IRQ_HANDLED
 */

static irqreturn_t tps80032_rtc_irq(int irq, void *dev_id)
{
	struct device *dev = dev_id;
	struct rtc_tps80032_data *data = dev_get_drvdata(dev);

	int ret;
	u8 reg;
	u8 val;

	/*dummy read RTC_STATUS for updating the register*/
	ret = rtc_read(dev->parent, RTC_STATUS_REG, &reg);
	if (ret < 0) {
		dev_err(data->dev, "unable to read RTC_STATUS reg\n");
		goto exit;
	}

	mutex_lock(&cntrol2_lock);

	/*read RTC_STATUS for updating the register*/
	ret = rtc_read(dev->parent, RTC_STATUS_REG, &reg);
	if (ret < 0) {
		dev_err(data->dev, "unable to read RTC_STATUS reg\n");
		goto exit;
	}

	val = (reg & (~ALARM_INT_STATUS)) | ALARM_INT_STATUS;
	/*Clear interrupt status*/
	ret = rtc_write(dev->parent, RTC_STATUS_REG, val);
	if (ret < 0) {
		dev_err(data->dev, "unable to clear set Alarm INT status\n");
		goto exit;
	}
	/*Only notify to user in case alarm interrupt is raised*/
	rtc_update_irq(data->rtc, 1, RTC_IRQF | RTC_AF);
	ret = IRQ_HANDLED;
exit:
	mutex_unlock(&cntrol2_lock);
	dev_dbg(data->dev, "<<< %s end\n", __func__);

	return ret;
}

/*
 * dec2bcd: Convert decimal number to setting value to RTC register
 * @dec: decimal number
 * return:
 *        u8: setting value to RTC register
 */

static u8 dec2bcd(u8 dec)
{
	return ((dec/10) << 4) + (dec % 10);
}

/*
 * bcd2dec: Convert value read from RTC register to decimal number
 * @bcd: value read from RTC register
 * return:
 *        u8: decimal number
 */

static u8 bcd2dec(u8 bcd)
{
	return (bcd >> 4) * 10 + (bcd & 0xF);
}

/*
 * tps80032_rtc_read_time: Get time from RTC register
 * @dev: i2c_client device
 * @tm: rtc_time struture, store the time read from RTC register
 * return:
 *        0: Normal operation
 *       <0: Error occurs
 */

static int tps80032_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	u8 val[7];
	int ret = 0;

	dev_dbg(dev->parent, ">>> %s start\n", __func__);

	mutex_lock(&set_time_lock);
	/*Read system time from RTC hardware*/
	ret = rtc_reads(dev->parent, RTC_SECONDS_REG, 7, val);
	if (ret < 0) {
		dev_err(dev->parent, "failed reading time\n");
		mutex_unlock(&set_time_lock);
		goto exit;
	}

	/*Convert the register setting value to decimal value*/
	tm->tm_sec  = bcd2dec(val[0] & 0x7F);
	tm->tm_min  = bcd2dec(val[1] & 0x7F);
	tm->tm_hour = bcd2dec(val[2] & 0x3F);
	tm->tm_mday = bcd2dec(val[3] & 0x3F);
	tm->tm_mon  = bcd2dec(val[4] & 0x1F) - 1;
	tm->tm_year = bcd2dec(val[5] & 0xFF) + RTC_YEAR_OFFSET;
	tm->tm_wday = bcd2dec(val[6] & 0x07);

	mutex_unlock(&set_time_lock);
	dev_dbg(dev->parent, "<<< %s end\n", __func__);

exit:
	return ret;
}

/*
 * tps80032_rtc_set_time: Set time to RTC register
 * @dev: i2c_client device
 * @tm: rtc_time struture, store the time to writer to RTC register
 * return:
 *        0: Normal operation
 *       <0: Error occurs
 */

static int tps80032_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	u8 val[7];
	int ret;

	dev_dbg(dev->parent, ">>> %s start\n", __func__);

	mutex_lock(&set_time_lock);

	/*Convert decimal value to register setting value*/
	val[0] = dec2bcd(tm->tm_sec);
	val[1] = dec2bcd(tm->tm_min);
	val[2] = dec2bcd(tm->tm_hour);
	val[3] = dec2bcd(tm->tm_mday);
	val[4] = dec2bcd(tm->tm_mon + 1);
	val[5] = dec2bcd(tm->tm_year % RTC_YEAR_OFFSET);
	val[6] = dec2bcd(tm->tm_wday);

	/*Stop RTC before setting time*/
	ret = tps80032_rtc_stop(dev);
	if (ret < 0)
		goto exit;

	ret = rtc_writes(dev->parent, RTC_SECONDS_REG, 7, val);

	if (ret < 0) {
		dev_err(dev->parent, "failed to program new time\n");
		goto exit;
	}

	ret = tps80032_rtc_start(dev);

exit:
	mutex_unlock(&set_time_lock);
	dev_dbg(dev->parent, "<<< %s end\n", __func__);
	return ret;
}

/*
 * tps80032_rtc_read_alarm: Get alarm from RTC register
 * @dev: i2c_client device
 * @alarm: rtc_wkalrm struture, store the alarm configuration read
 * from RTC register
 * return:
 *        0: Normal operation
 *       <0: Error occurs
 */

static int tps80032_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	int ret = 0;
	u8 val[6];

	dev_dbg(dev->parent, ">>> %s start\n", __func__);

	mutex_lock(&set_alarm_lock);

	ret = rtc_reads(dev->parent, RTC_ALARM_SECONDS_REG, 6, val);
	if (ret < 0) {
		dev_err(dev->parent, "failed to read alarm reg\n");
		goto exit;
	}

	/*/*Convert the register setting value to decimal value*/
	alarm->time.tm_sec = bcd2dec(val[0] & 0x7F);
	alarm->time.tm_min = bcd2dec(val[1] & 0x7F);
	alarm->time.tm_hour = bcd2dec(val[2] & 0x3F);
	alarm->time.tm_mday = bcd2dec(val[3] & 0x3F);
	alarm->time.tm_mon = bcd2dec(val[4] & 0x1F) - 1;
	alarm->time.tm_year = bcd2dec(val[5]) + RTC_YEAR_OFFSET;

exit:
	mutex_unlock(&set_alarm_lock);
	dev_dbg(dev->parent, "<<< %s end\n", __func__);
	return ret;
}

/*
 * tps80032_rtc_set_alarm: Set alarm to  RTC register
 * @dev: i2c_client device
 * @alarm: rtc_wkalrm struture, store the alarm configuration read from
 * RTC register
 * return:
 *        0: Normal operation
 *       <0: Error occurs
 */

static int tps80032_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	int ret = 0;
	struct rtc_time rtc_now;
	unsigned long now;
	unsigned long alarm_time;
	unsigned char val[6];

	dev_dbg(dev->parent, ">>> %s start\n", __func__);

	mutex_lock(&set_alarm_lock);
	ret = tps80032_rtc_read_time(dev, &rtc_now);
	if (ret < 0) {
		dev_err(dev->parent, "failed to read time\n");
		goto exit;
	}

	rtc_tm_to_time(&rtc_now, &now);
	rtc_tm_to_time(&alarm->time, &alarm_time);

	if (now < alarm_time) {
		/*Convert decimal value to register setting value*/
		val[0] = dec2bcd(alarm->time.tm_sec);
		val[1] = dec2bcd(alarm->time.tm_min);
		val[2] = dec2bcd(alarm->time.tm_hour);
		val[3] = dec2bcd(alarm->time.tm_mday);
		val[4] = dec2bcd(alarm->time.tm_mon + 1);
		val[5] = dec2bcd(alarm->time.tm_year % RTC_YEAR_OFFSET);

		ret = rtc_writes(dev->parent, RTC_ALARM_SECONDS_REG, 6, val);
		if (ret < 0) {
			dev_err(dev->parent, "failed to write alarm reg\n");
		} else {
			ret = tps80032_alarm_irq_enable(dev, 1);
			if (ret < 0)
				dev_err(dev->parent, "failed to enable alarm interrupt\n");
		}
	} else
		ret = -EINVAL;

exit:
	mutex_unlock(&set_alarm_lock);
	dev_dbg(dev->parent, "<<< %s end\n", __func__);
	return ret;
}

/*
 * tps80032_alarm_irq_enable: Enable/Disable alarm interrupt
 * @dev: i2c_client device
 * @enabled: parameter to specify enable/disable alarm interrupt
 * return:
 *        0: Normal operation
 *       <0: Error occurs
 */

static int tps80032_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	u8 reg;
	u8 val = 0;
	int ret = 0;

	dev_dbg(dev->parent, ">>> %s start\n", __func__);

	mutex_lock(&cntrol1_lock);
	if ((0 == enabled) || (1 == enabled)) {
		ret = rtc_read(dev->parent, RTC_INTERRUPTS_REG, &reg);
		if (ret < 0) {
			dev_err(dev->parent, "failed to read interrupt reg\n");
			goto exit;
		}

		if ((1 == enabled) && !(reg & ENABLE_ALARM_INT)) {
			val = reg | ENABLE_ALARM_INT;
			ret = rtc_write(dev->parent, RTC_INTERRUPTS_REG, val);
			if (ret < 0)
				dev_err(dev->parent, "failed to write interrupt reg\n");
		} else if ((0 == enabled) && (reg & ENABLE_ALARM_INT)) {
			val = reg & (~ENABLE_ALARM_INT);
			ret = rtc_write(dev->parent, RTC_INTERRUPTS_REG, val);
			if (ret < 0)
				dev_err(dev->parent, "failed to write interrupt reg\n");
		} else
			ret = 0;
	} else
		ret = -EINVAL;

exit:
	mutex_unlock(&cntrol1_lock);
	dev_dbg(dev->parent, "<<< %s end\n", __func__);
	return ret;
}

static struct rtc_class_ops tps80032_rtcops = {
	.read_time          = tps80032_rtc_read_time,
	.set_time           = tps80032_rtc_set_time,
	.read_alarm         = tps80032_rtc_read_alarm,
	.set_alarm          = tps80032_rtc_set_alarm,
	.alarm_irq_enable   = tps80032_alarm_irq_enable
};

/*
 * tps80032_rtc_init_irq: Initialize interrup
 * @data: information of rtc_tps80032_data structure
 * return:
 *        0: Normal operation
 *       <0: Error occurs
 */

int tps80032_rtc_init_irq(struct device *dev, struct rtc_tps80032_data *data)
{
#ifdef CONFIG_PMIC_INTERFACE
	struct tps80032_rtc_platform_data *pdata = dev->platform_data;
#else
	struct tps80031_rtc_platform_data *pdata = dev->platform_data;
#endif

	int ret;

	if (pdata->irq < 0) {
		dev_err(dev->parent, "no IRQ specified, wakeup is disabled\n");
		return 0;
	}

	/*Enable alarm interrupt*/
	ret = rtc_write(dev->parent, RTC_INTERRUPTS_REG, ENABLE_ALARM_INT);
	if (ret < 0) {
		dev_err(dev->parent, "unable to program RTC_INTERRUPTS reg\n");
		return ret;
	}

	ret = request_threaded_irq(pdata->irq, NULL,\
			tps80032_rtc_irq, IRQF_ONESHOT, "INT_RTC", dev);

	if (ret < 0) {
		dev_err(dev->parent, ">>>request_irq failed (IRQ = %d)\n",
							pdata->irq);
		return ret;
	}

	device_init_wakeup(dev, 1);

	enable_irq_wake(pdata->irq);

	return 0;
}

/*
 * tps80032_rtc_probe: probe function for rtc driver, called by I2C driver
 * @client: The I2C client device
 * @id: The I2C ID
 * return:
 *        = 0: Normal operation
 *        < 0: Error occurs
 */

static int __devinit tps80032_rtc_probe(struct platform_device *pdev)
{
	int ret;
	struct rtc_tps80032_data *data;
	struct rtc_time tm;
	u8 val;
	u8 reg;

	data = kzalloc(sizeof(struct rtc_tps80032_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->rtc = rtc_device_register(pdev->name,
			&pdev->dev, &tps80032_rtcops, THIS_MODULE);

	if (IS_ERR(data->rtc)) {
		ret = PTR_ERR(data->rtc);
		goto err;
	}

	data->dev = &pdev->dev;
	/*Dummy read for RTC_STATUS_REG*/
	ret = rtc_read((&pdev->dev)->parent, RTC_STATUS_REG, &reg);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to read time program RTC_STATUS reg\n");
		goto err;
	}

	/*If time is POR values, set initial time is 2012/10/11 */

	ret = tps80032_rtc_read_time(&pdev->dev, &tm);

	if (ret < 0) {
		dev_err(&pdev->dev, "unable to read time\n");
		goto err;
	}

	if (((RTC_YEAR_OFFSET + RTC_POR_YEAR) == tm.tm_year) &&
		(RTC_POR_MONTH == (tm.tm_mon + 1)) &&
		(RTC_POR_DAY == tm.tm_mday)) {
		tm.tm_year = 2012;
		tm.tm_mon = 10;
		tm.tm_mday = 11;
		ret = tps80032_rtc_set_time(&pdev->dev, &tm);
		if (ret < 0) {
			dev_err(&pdev->dev, "unable to set time\n");
			goto err;
		}
	}

	/*Clear alarm status*/
	val = ALARM_INT_STATUS;
	ret = rtc_write((&pdev->dev)->parent, RTC_STATUS_REG, val);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to program RTC_STATUS reg\n");
		goto err;
	}

	dev_set_drvdata(&pdev->dev, data);

	/*WA: Temporarily disabled as there is an issue with system IRQ mapping.*/
	/*
	ret = tps80032_rtc_init_irq(&pdev->dev, data);
	if (ret < 0) {
		dev_err(&pdev->dev, ">>>Fail to initialize irq for RTC\n");
		goto err;
	}
	*/
	return 0;

err:
	if (data->rtc)
		rtc_device_unregister(data->rtc);
	kfree(data);
	return ret;
}

/*
 * tps80032_rtc_remove: Remove function for RTC driver
 * @client: The I2C client device.
 * return:
 *        = 0: Normal operation
 */

static int __devexit tps80032_rtc_remove(struct platform_device *pdev)
{
	struct rtc_tps80032_data *data = dev_get_drvdata(&pdev->dev);

	struct device *dev = &pdev->dev;

#ifdef CONFIG_PMIC_INTERFACE
	struct tps80032_rtc_platform_data *pdata = dev->platform_data;
#else
	struct tps80031_rtc_platform_data *pdata = dev->platform_data;
#endif

	if (data) {
		if (data->rtc) {
			rtc_device_unregister(data->rtc);
			free_irq(pdata->irq, data);
		}
		kfree(data);
	}
	return 0;
}

static struct platform_driver tps80032_rtc_driver = {
	.driver	= {
		.name	= "rtc_tps80032",
		.owner	= THIS_MODULE,
	},
	.probe	= tps80032_rtc_probe,
	.remove	= __devexit_p(tps80032_rtc_remove),
};
/*
 * tps80032_rtc_init: Initialize RTC driver
 * @void
 * return:
 *        = 0: Normal termination
 *        < 0: Error occurs
 */

static __init int tps80032_rtc_init(void)
{
	return platform_driver_register(&tps80032_rtc_driver);
}
module_init(tps80032_rtc_init);

/*
 * tps80032_rtc_exit: Free RTC driver
 * @void
 * return: None
 */

static __exit void tps80032_rtc_exit(void)
{
	platform_driver_unregister(&tps80032_rtc_driver);
}
module_exit(tps80032_rtc_exit);

MODULE_DESCRIPTION("tps80032 RTC Driver");
MODULE_LICENSE("GPL");
