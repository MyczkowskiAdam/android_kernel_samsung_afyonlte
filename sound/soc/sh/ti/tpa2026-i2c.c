/*****************************************************************************
*  Copyright 2001 - 2008 Broadcom Corporation.  All rights reserved.
*  Copyright (C) 2013 Renesas Mobile Corp.
*
*  Unless you and Broadcom execute a separate written software license
*  agreement governing use of this software, this software is licensed to you
*  under the terms of the GNU General Public License version 2, available at
*  http://www.gnu.org/licenses/old-license/gpl-2.0.html (the "GPL").
*
*  Notwithstanding the above, under no circumstances may you combine this
*  software in any way with any other Broadcom software provided under a
*  license other than the GPL, without Broadcom's express prior written
*  consent.
*
*****************************************************************************/

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>

#include "sound/soundpath/soundpath.h"
#define CONFIG_TPA2026_I2C_DBG
#include "sound/tpa2026-i2c.h"

struct tpa2026_i2c_data *g_tpa2026_i2c_data;
static struct proc_dir_entry *g_tpa2026_i2c_parent;
unsigned int g_tpa2026_i2c_log_level;
unsigned int g_tpa2026_i2c_amp_state;

/**
 * @brief	i2c write function.
 *
 * @param	buf	write data.
 * @param	len	data length.
 *
 * @retval	none.
 */
static int tpa2026_i2c_write(char *buf, int len)
{
	int rc;
	struct i2c_msg msg[] = {
		{
			.addr	= g_tpa2026_i2c_data->i2c_client->addr,
			.flags	= 0,
			.len	= len,
			.buf	= buf,
		}
	};

	tpa2026_i2c_pr_func_start();

	rc = i2c_transfer(g_tpa2026_i2c_data->i2c_client->adapter, msg, 1);
	/* delay 1ms */
	usleep_range(1000, 1000);


	if (0 > rc)
		tpa2026_i2c_pr_err("i2c write error. rc[%d].\n", rc);

	tpa2026_i2c_pr_func_end();
	return rc;
}


/**
 * @brief	i2c write wrapper function.
 *
 * @param	reg	register address.
 * @param	value	write value.
 *
 * @retval	none.
 */
static int tpa2026_i2c_write_device(u8 reg, u8 value)
{
	int rc;
	unsigned char data[2] = {0};

	tpa2026_i2c_pr_func_start();

	data[0] = reg;
	data[1] = value;

	rc = tpa2026_i2c_write(data, 2);
	if (0 > rc)
		tpa2026_i2c_pr_err("reg[0x%02x] value[0x%02x].\n", reg, value);

	tpa2026_i2c_pr_func_end();
	return rc;
}


/**
 * @brief	amp on function.
 *
 * @param	none.
 *
 * @retval	none.
 */
static void tpa2026_i2c_amp_on(void)
{
	tpa2026_i2c_pr_func_start();

	if (TPA2026_I2C_DISABLE == g_tpa2026_i2c_amp_state) {
		/* sdz L -> H */
		gpio_set_value(g_tpa2026_i2c_data->pdata->gpio_shdn, 1);
		/* delay 1ms */
		usleep_range(1000, 1000);
		/* speaker and SWS enable. NGF desable */
		tpa2026_i2c_write_device(0x01, 0xC2);
#if 0
		/* tpa2026 default */
		tpa2026_i2c_write_device(0x02, 0x05);
		tpa2026_i2c_write_device(0x03, 0x0B);
		tpa2026_i2c_write_device(0x04, 0x00);
		tpa2026_i2c_write_device(0x05, 0x06);
		tpa2026_i2c_write_device(0x06, 0x3A);
		tpa2026_i2c_write_device(0x07, 0xC2);
		/* speaker, SWS and NGF enable. */
		tpa2026_i2c_write_device(0x01, 0xC3);
#endif
		/* delay 5ms */
		usleep_range(5000, 5000);
	}

	/* status update */
	g_tpa2026_i2c_amp_state = TPA2026_I2C_ENABLE;

	tpa2026_i2c_pr_func_end();
	return;
}


/**
 * @brief	amp shutdown function.
 *
 * @param	none.
 *
 * @retval	none.
 */
static void tpa2026_i2c_amp_shutdown(void)
{
	tpa2026_i2c_pr_func_start();

	if (TPA2026_I2C_ENABLE == g_tpa2026_i2c_amp_state) {
		/* shutdown */
		tpa2026_i2c_write_device(0x01, 0x22);
		/* sdz H -> L */
		gpio_set_value(g_tpa2026_i2c_data->pdata->gpio_shdn, 0);
	}

	/* status update */
	g_tpa2026_i2c_amp_state = TPA2026_I2C_DISABLE;

	tpa2026_i2c_pr_func_end();
	return;
}


#ifdef CONFIG_TPA2026_I2C_DBG
/* TPA2026 seemed not to support reading register. */
/* Anyway, it's better to have reading function. */
static unsigned int sysfs_reg;
static unsigned int sysfs_val;

/* usage: cat /sys/bus/i2c/devices/i2c-x/regwrite */
static ssize_t show_regwrite(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	unsigned int reg, val;

	reg = sysfs_reg;
	val = sysfs_val;

	tpa2026_i2c_pr_debug("register[0x%x], value[0x%x].\n", reg, val);

	return sprintf(buf, "register[0x%x],value[0x%x].\n", reg, val);
}

/* usage: echo reg val > /sys/bus/i2c/devices/i2c-x/regwrite */
/* example: # echo 0 0xff > /sys/bus/i2c/devices/i2c-x/regwrite */
static ssize_t store_regwrite(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int reg, val;

	sscanf(buf, "%x %x ", &reg, &val);
	sysfs_reg = reg;
	sysfs_val = val;
	tpa2026_i2c_pr_debug("register[0x%X], value[0x%X].\n", reg, val);

	/* put the i2c writing code here. */
	/* for debugging, first comment out. */
	ret = tpa2026_i2c_write_device(reg, val);
	if (ret < 0)
		tpa2026_i2c_pr_err("ret[%d].\n", ret);

	return count;
}

static DEVICE_ATTR(regwrite, 0644, show_regwrite, store_regwrite);
/* If adb-shell is open in user space, you may need to change mode to 0666. */
static struct attribute *tpa2026_i2c_attrs[] = {
	&dev_attr_regwrite.attr,
	NULL
};

static const struct attribute_group tpa2026_i2c_attr_group = {
	.attrs = tpa2026_i2c_attrs,
};
#endif	/* CONFIG_TPA2026_I2C_DBG */


/**
 * @brief	sound callback function.
 *
 * @param	mode	mode on PCM value.
 * @param	device	device on PCM value.
 * @param	ch_dev	ch_type.
 *
 * @retval	0	successful.
 * @retval	other	failed.
 */
static int tpa2026_i2c_set_state
		(unsigned int mode, unsigned int device, unsigned int ch_dev)
{
	int rc = 0;

	tpa2026_i2c_pr_func_start("mode[%d] device[%d] ch_dev[%d].\n",
						mode, device, ch_dev);
	if (TPA2026_I2C_INPUT_DEVICE & device)
		goto rtn;

	if (SNDP_OUT_SPEAKER & device) {
		if (SNDP_EXTDEV_STOP == ch_dev)
			tpa2026_i2c_amp_shutdown();
		else
			tpa2026_i2c_amp_on();
	} else {
#if 0 // 20130425 there is no sound after voice search. 
		tpa2026_i2c_amp_shutdown();
#endif
	}

rtn:
	tpa2026_i2c_pr_func_end("rc[%d].\n", rc);
	return rc;
}


/**
 * @brief	gpio initialize function.
 *
 * @param	gpio	gpio.
 *
 * @retval	0	successful.
 * @retval	other	failed.
 */
static int tpa2026_i2c_gpio_init(int gpio)
{
	int rc = 0;

	tpa2026_i2c_pr_func_start("gpio[%d].\n", gpio);

	/* gpio request */
	rc = gpio_request(gpio, NULL);
	if (0 != rc) {
		tpa2026_i2c_pr_err("request(%d) failed[%d].\n", gpio, rc);
		goto rtn;
	}

	/* gpio direction output */
	rc = gpio_direction_output(gpio, 1);
	if (0 != rc) {
		tpa2026_i2c_pr_err("direction_output(%d) failed[%d].\n",
							gpio, rc);
		goto err_gpio_request;
	}

	goto rtn;

err_gpio_request:
	gpio_free(gpio);
rtn:
	tpa2026_i2c_pr_func_end("rc[%d].\n", rc);
	return rc;
}


/**
 * @brief	log level read function.
 *
 * @param[in]	page	write position.
 * @param[in]	start	unused.
 * @param[in]	offset	unused.
 * @param[in]	count	maximum write length.
 * @param[in]	eof	unused.
 * @param[in]	data	unused.
 *
 * @retval	len	write length.
 */
static int tpa2026_i2c_read_log_level(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	int len = 0;

	len = snprintf(page, count, "0x%x\n", g_tpa2026_i2c_log_level);

	return len;
}


/**
 * @brief	log level write function.
 *
 * @param[in]	filp	unused.
 * @param[in]	buffer	user data.
 * @param[in]	len	length of data.
 * @param[in]	data	unused.
 *
 * @retval	len	read length.
 */
static int tpa2026_i2c_write_log_level(struct file *filp, const char *buffer,
					unsigned long len, void *data)
{
	int rc = 0;
	unsigned char buf[11] = {0};
	unsigned int code = 0;

	if (11 <= len) {
		/* size over */
		return len;
	}

	if (copy_from_user((void *)buf, (void __user *)buffer, len)) {
		/* failed copy_from_user */
		return len;
	}

	rc = kstrtouint(buf, 0, &code);
	if (0 != rc) {
		/* kstrtouint failed */
		return len;
	}

	if (!(TPA2026_I2C_LOG_LEVEL_CHECK & code))
		g_tpa2026_i2c_log_level = code;

	return len;
}


/**
 * @brief	create proc entry function.
 *
 * @param	none.
 *
 * @retval	0	successful.
 * @retval	-EACCES	failed.
 */
static int tpa2026_i2c_create_proc_entry(void)
{
	int rc = 0;
	struct proc_dir_entry *log_level = NULL;

	tpa2026_i2c_pr_func_start();

	/* make directory /proc/--- */
	g_tpa2026_i2c_parent = proc_mkdir(TPA2026_I2C_DRIVER_NAME, NULL);
	if (NULL != g_tpa2026_i2c_parent) {
		/* create file for log level */
		log_level = create_proc_entry(TPA2026_I2C_LOG_FILE_NAME,
				(S_IFREG | S_IRUGO | S_IWUGO),
				g_tpa2026_i2c_parent);
		if (NULL != log_level) {
			log_level->read_proc  = tpa2026_i2c_read_log_level;
			log_level->write_proc = tpa2026_i2c_write_log_level;
		} else {
			tpa2026_i2c_pr_err(
				"create failed for log level file.\n");
			remove_proc_entry(TPA2026_I2C_DRIVER_NAME, NULL);
			g_tpa2026_i2c_parent = NULL;
			rc = -EACCES;
		}
	} else {
		tpa2026_i2c_pr_err("create failed for proc parent.\n");
		rc = -EACCES;
	}

	tpa2026_i2c_pr_func_end("rc[%d].\n", rc);
	return rc;
}


/**
 * @brief	remove proc entry function.
 *
 * @param	none.
 *
 * @retval	none.
 */
static void tpa2026_i2c_remove_proc_entry(void)
{
	tpa2026_i2c_pr_func_start();

	if (NULL != g_tpa2026_i2c_parent) {
		remove_proc_entry(TPA2026_I2C_LOG_FILE_NAME,
						g_tpa2026_i2c_parent);
		remove_proc_entry(TPA2026_I2C_DRIVER_NAME, NULL);
		g_tpa2026_i2c_parent = NULL;
	}

	tpa2026_i2c_pr_func_end();
}


/**
 * @brief	suspend notification receive function.
 *
 * @param	dev	unused.
 *
 * @retval	rc	result.
 */
static int tpa2026_i2c_suspend(struct device *dev)
{
	int rc = 0;

	tpa2026_i2c_pr_func_start();

	/* nop */

	tpa2026_i2c_pr_func_end();
	return rc;
}


/**
 * @brief	suspend notification receive function.
 *
 * @param	dev	unused.
 *
 * @retval	rc	result.
 */
static int tpa2026_i2c_resume(struct device *dev)
{
	int rc = 0;

	tpa2026_i2c_pr_func_start();

	/* nop */

	tpa2026_i2c_pr_func_end();
	return rc;
}


/**
 * @brief	runtime suspend notification receive function.
 *
 * @param	dev	unused.
 *
 * @retval	rc	result.
 */
static int tpa2026_i2c_runtime_suspend(struct device *dev)
{
	int rc = 0;

	tpa2026_i2c_pr_func_start();

	/* nop */

	tpa2026_i2c_pr_func_end();
	return rc;
}


/**
 * @brief	runtime suspend notification receive function.
 *
 * @param	dev	unused.
 *
 * @retval	rc	result.
 */
static int tpa2026_i2c_runtime_resume(struct device *dev)
{
	int rc = 0;

	tpa2026_i2c_pr_func_start();

	/* nop */

	tpa2026_i2c_pr_func_end();
	return rc;
}


/*
 * device object
 */
static struct platform_device tpa2026_i2c_platform_device = {
	.name = TPA2026_I2C_DRIVER_NAME,
};


/*
 * pm object
 */
static const struct dev_pm_ops tpa2026_i2c_dev_pm_ops = {
	.suspend		= tpa2026_i2c_suspend,
	.resume			= tpa2026_i2c_resume,
	.runtime_suspend	= tpa2026_i2c_runtime_suspend,
	.runtime_resume		= tpa2026_i2c_runtime_resume,
};


/*
 * driver object
 */
static struct platform_driver tpa2026_i2c_platform_driver = {
	.driver		= {
		.name	= TPA2026_I2C_DRIVER_NAME,
		.pm	= &tpa2026_i2c_dev_pm_ops,
		.probe = NULL,
		.remove = NULL,
	},
};


/**
 * @brief	module init function.
 *
 * @param	client	i2c_client.
 * @param	id	i2c_device_id.
 *
 * @retval	0	successful.
 * @retval	other	failed.
 */
static int tpa2026_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	int rc = 0;
	struct tpa2026_i2c_data *tpa2026;
	static struct tpa2026_i2c_platform_data *pdata;
	static struct sndp_extdev_callback_func callback_func;

	tpa2026_i2c_pr_func_start();

	pdata = i2c->dev.platform_data;
	if (NULL == pdata) {
		tpa2026_i2c_pr_err("platform data is NULL.\n");
		goto exit;
	}


	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		tpa2026_i2c_pr_err("i2c check functionality error.\n");
		rc = -ENODEV;
		goto exit;
	}

	tpa2026 = kzalloc(sizeof(*tpa2026), GFP_KERNEL);
	if (!tpa2026) {
		tpa2026_i2c_pr_err(
			"failed to allocate memory for module data.\n");
		rc = -ENOMEM;
		goto exit;
	}

	tpa2026->pdata = i2c->dev.platform_data;

	/* set i2c */
	i2c_set_clientdata(i2c, tpa2026);

	/* register tpa2026_i2c_data */
	tpa2026->i2c_client = i2c;
	tpa2026->device.minor = MISC_DYNAMIC_MINOR;
	tpa2026->device.name = TPA2026_I2C_DRIVER_NAME;

	/* misc register */
	rc = misc_register(&tpa2026->device);
	if (rc) {
		tpa2026_i2c_pr_err("tpa2026 device register failed.\n");
		goto err_misc_reg_failed;
	}

	/* gpio request & dirction output */
	rc = tpa2026_i2c_gpio_init(tpa2026->pdata->gpio_shdn);
	if (0 != rc) {
		tpa2026_i2c_pr_err(
			"tpa2026_i2c_gpio_init(%d) failed. rc[%d].\n",
			tpa2026->pdata->gpio_shdn, rc);
		goto err_clk_enable_failed;
	}

	/* platform device register */
	rc = platform_device_register(&tpa2026_i2c_platform_device);
	if (0 != rc) {
		tpa2026_i2c_pr_err(
			"platform_device_register failed. rc[%d].\n", rc);
		goto err_pf_device_reg_failed;
	}

	/* platform driver register */
	rc = platform_driver_register(&tpa2026_i2c_platform_driver);
	if (0 != rc) {
		tpa2026_i2c_pr_err(
			"platform_driver_register failed. rc[%d].\n", rc);
		goto err_pf_driver_reg_failed;
	}

	/* create proc */
	rc = tpa2026_i2c_create_proc_entry();
	if (0 != rc) {
		tpa2026_i2c_pr_err("create_proc_entry failed. rc[%d].\n", rc);
		goto err_create_proc_failed;
	}

	/* sdz H -> L */
	gpio_set_value(tpa2026->pdata->gpio_shdn, 0);

	/* copy to static table pointer */
	g_tpa2026_i2c_data = tpa2026;

	/* set callback function */
	callback_func.set_state = tpa2026_i2c_set_state;
	callback_func.set_nb_wb = NULL;
	sndp_extdev_regist_callback(&callback_func);

#ifdef CONFIG_TPA2026_I2C_DBG
	rc = sysfs_create_group(&g_tpa2026_i2c_data->i2c_client->dev.kobj,
					&tpa2026_i2c_attr_group);
	tpa2026_i2c_pr_info("[sysfs_create_group[%d].\n", rc);
#endif	/* CONFIG_TPA2026_I2C_DBG */

	goto exit;

err_create_proc_failed:
	platform_driver_unregister(&tpa2026_i2c_platform_driver);
err_pf_driver_reg_failed:
	platform_device_unregister(&tpa2026_i2c_platform_device);
err_pf_device_reg_failed:
	gpio_free(tpa2026->pdata->gpio_shdn);
err_clk_enable_failed:
	misc_deregister(&tpa2026->device);
err_misc_reg_failed:
	kfree(tpa2026);
exit:
	tpa2026_i2c_pr_func_end("rc[%d].\n", rc);
	return rc;
}


/**
 * @brief	module exit function.
 *
 * @param	client	i2c_client.
 *
 * @retval	0	result.
 */
static int tpa2026_i2c_remove(struct i2c_client *client)
{
	struct tpa2026_i2c_data *tpa2026 = i2c_get_clientdata(client);

	tpa2026_i2c_pr_func_start();

	platform_driver_unregister(&tpa2026_i2c_platform_driver);
	platform_device_unregister(&tpa2026_i2c_platform_device);

	tpa2026_i2c_remove_proc_entry();

	gpio_free(tpa2026->pdata->gpio_shdn);

	misc_deregister(&tpa2026->device);

#ifdef CONFIG_TPA2026_I2C_DBG
	sysfs_remove_group(&g_tpa2026_i2c_data->i2c_client->dev.kobj,
						&tpa2026_i2c_attr_group);
#endif	/* CONFIG_TPA2026_I2C_DBG */

	kfree(tpa2026);

	tpa2026_i2c_pr_func_end();
	return 0;
}


/*
 * i2c id
 */
static const struct i2c_device_id tpa2026_i2c_id[] = {
	{TPA2026_I2C_DRIVER_NAME, 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, tpa2026_i2c_id);


/*
 * i2c object
 */
static struct i2c_driver tpa2026_i2c_driver = {
	.driver = {
		.name = TPA2026_I2C_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
	.probe = tpa2026_i2c_probe,
	.remove = tpa2026_i2c_remove,
	.id_table = tpa2026_i2c_id,
};


/**
 * @brief	module init function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
static int __init tpa2026_i2c_init(void)
{
	g_tpa2026_i2c_log_level = TPA2026_I2C_LOG_ERR;
	return i2c_add_driver(&tpa2026_i2c_driver);
}


/**
 * @brief	module exit function.
 *
 * @param	none.
 *
 * @retval	none.
 */
static void __exit tpa2026_i2c_exit(void)
{
	i2c_del_driver(&tpa2026_i2c_driver);
}

module_init(tpa2026_i2c_init);
module_exit(tpa2026_i2c_exit);

MODULE_DESCRIPTION("tpa2026 amp driver");
MODULE_AUTHOR("SAMSUNG");
MODULE_LICENSE("GPL");
