/* drivers/misc/fm34_we395.c - fm34_we395 voice processor driver
 * sound/soc/sh/fortmedia/fm34_we395.c - fm34_we395 voice processor driver
 *
 * Copyright (C) 2012 Samsung Corporation.
 * Copyright (C) 2013 Renesas Mobile Corp.
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

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/module.h>

#include "fm34_we395.h"
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <mach/common.h>
#include <mach/r8a7373.h>
#include "sound/soundpath/soundpath.h"
#include <linux/platform_device.h>
#include <linux/vcd/vcd.h>

/* Global parameters */
static struct i2c_client *g_fm34_client;
static struct fm34_platform_data *g_fm34_pdata;
struct mutex g_fm34_lock;
static struct clk *g_fm34_vclk4_clk;
int g_fm34_event;
int g_fm34_curt_status;
int g_fm34_curt_band;
static struct proc_dir_entry *g_fm34_parent;
unsigned int g_fm34_log_level = FM34_LOG_ALL;
static bool g_fm34_is_vclk4_enable;
static void __iomem *g_fm34_vclk_adr;
static struct sndp_extdev_callback_func g_fm34_callback_func;

/* Function prototype */
static int fm34_suspend(struct device *dev);
static int fm34_resume(struct device *dev);
static int fm34_runtime_suspend(struct device *dev);
static int fm34_runtime_resume(struct device *dev);

static int fm34_nop(void);
static int fm34_error(void);
static int fm34_set_voice_mode(void);
static int fm34_set_idle_mode(void);
static int fm34_set_HS_mode(void);
static int fm34_set_bypass_mode(void);
static void fm34_set_gpio_pwdn_enable(void);
static void fm34_set_gpio_pwdn_disable(void);
static void fm34_set_gpio_bp_enable(void);
static void fm34_set_gpio_bp_disable(void);
static void fm34_set_gpio_parameter_rst(void);

static int fm34_enable_vclk4(void);
static void fm34_disable_vclk4(void);

static int fm34_create_event
	(unsigned int mode, unsigned int device, unsigned int ch_dev);
static int fm34_set_mode
	(unsigned int mode, unsigned int device, unsigned int ch_dev);
static int fm34_set_nb_wb(unsigned int nb_wb);


/*
 * callback object
 */
static const struct dev_pm_ops fm34_dev_pm_ops = {
	.suspend		= fm34_suspend,
	.resume			= fm34_resume,
	.runtime_suspend	= fm34_runtime_suspend,
	.runtime_resume		= fm34_runtime_resume,
};

/*
 * device object
 */
static struct platform_device fm34_platform_device = {
	.name = FM34_MODULE_NAME,
};

/*
 * driver object
 */
static struct platform_driver fm34_platform_driver = {
	.driver		= {
		.name	= FM34_MODULE_NAME,
		.pm	= &fm34_dev_pm_ops,
		.probe	= NULL,
		.remove	= NULL,
	},
};

/*
 * event type
 */
enum fm34_event_type {
	FM34_EVENT_IDLE = 0,
	FM34_EVENT_BYPASS,
	FM34_EVENT_VOICE,
	FM34_EVENT_MAX
};

/*
 * status type
 */
enum fm34_status_type {
	FM34_STATUS_IDLE = 0,
	FM34_STATUS_BYPASS,
	FM34_STATUS_VOICE,
	FM34_STATUS_MAX,
	FM34_STATUS_NOP
};

typedef int (*fm34_state_func)(void);

struct fm34_table {
	fm34_state_func exec_func;
	int next_state;
};

/* State table */
struct fm34_table fm34_nop_table = {
	fm34_nop, FM34_STATUS_NOP
};
struct fm34_table fm34_idle_table = {
	fm34_set_idle_mode, FM34_STATUS_IDLE
};
struct fm34_table fm34_idle_to_bypass_table = {
	fm34_nop, FM34_STATUS_BYPASS
};
struct fm34_table fm34_bypass_table = {
	fm34_set_idle_mode, FM34_STATUS_BYPASS
};
struct fm34_table fm34_bypass_to_idle_table = {
	fm34_nop, FM34_STATUS_IDLE
};
struct fm34_table fm34_voice_table = {
	fm34_set_voice_mode, FM34_STATUS_VOICE
};
struct fm34_table fm34_error_table = {
	fm34_error, FM34_STATUS_NOP
};


struct fm34_table *fm34_state_table[FM34_STATUS_MAX][FM34_EVENT_MAX] = {
	/* FM34_STATUS_IDLE */
	{
		&fm34_nop_table,		/* FM34_EVENT_IDLE */
		&fm34_idle_to_bypass_table,	/* FM34_EVENT_BYPASS */
		&fm34_voice_table,		/* FM34_EVENT_VOICE */
	},
	/* FM34_STATUS_BYPASS */
	{
		&fm34_bypass_to_idle_table,	/* FM34_EVENT_IDLE */
		&fm34_nop_table,		/* FM34_EVENT_BYPASS */
		&fm34_voice_table,		/* FM34_EVENT_VOICE */
	},
	/* FM34_STATUS_VOICE */
	{
		&fm34_idle_table,	/* FM34_EVENT_IDLE */
		&fm34_bypass_table,	/* FM34_EVENT_BYPASS */
		&fm34_nop_table,	/* FM34_EVENT_VOICE */
	},
};

static void fm34_i2c_debug(unsigned char *i2c_cmds, int size)
{
	int i = 0;

	/* Output i2c parameters. */
	for (i = 0; i < size; i += 1) {
		fm34_pr_info("i2c_cmds[%d/%d] = 0x%x\n",
			i, (size - 1), i2c_cmds[i]);
	}
}

static int fm34_i2c_write(char *txData, int length)
{
	int rc;

	struct i2c_msg msg[] = {
		{
			.addr = g_fm34_client->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};

	fm34_pr_func_start();

	if (FM34_LOG_INFO & g_fm34_log_level)
		fm34_i2c_debug(txData, length);

	rc = i2c_transfer(g_fm34_client->adapter, msg, 1);
	if (rc < 0) {
		fm34_pr_err("transfer error[%d]\n", rc);
		return rc;
	}

	fm34_pr_func_end();
	return 0;
}

static void fm34_set_gpio_pwdn_enable(void)
{
	int val = gpio_get_value(g_fm34_pdata->gpio_pwdn);

	fm34_pr_func_start();

	/* PWDN# low */
	if (val == FM34_PWDN_DISABLE) {
		gpio_set_value(g_fm34_pdata->gpio_pwdn, FM34_PWDN_ENABLE);
		fm34_pr_debug("gpio_pwdn[0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_pwdn));

		usleep_range(1000, 1000);
	}

	fm34_pr_func_end();
	return;
}

static void fm34_set_gpio_pwdn_disable(void)
{
	int val = gpio_get_value(g_fm34_pdata->gpio_pwdn);

	fm34_pr_func_start();

	/* PWDN# high */
	if (val == FM34_PWDN_ENABLE) {
		gpio_set_value(g_fm34_pdata->gpio_pwdn, FM34_PWDN_DISABLE);
		fm34_pr_debug("gpio_pwdn[0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_pwdn));

		usleep_range(30000, 30000);
	}

	fm34_pr_func_end();
	return;
}

static void fm34_set_gpio_bp_enable(void)
{
	int val = gpio_get_value(g_fm34_pdata->gpio_bp);

	fm34_pr_func_start();

	/* BP# low */
	if (val == FM34_BP_DISABLE) {
		gpio_set_value(g_fm34_pdata->gpio_bp, FM34_BP_ENABLE);
		fm34_pr_debug("gpio_bp[0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_bp));
	}

	fm34_pr_func_end();
	return;
}

static void fm34_set_gpio_bp_disable(void)
{
	int val = gpio_get_value(g_fm34_pdata->gpio_bp);

	fm34_pr_func_start();

	/* BP# high */
	if (val == FM34_BP_ENABLE) {
		gpio_set_value(g_fm34_pdata->gpio_bp, FM34_BP_DISABLE);
		fm34_pr_debug("gpio_bp[0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_bp));
	}

	fm34_pr_func_end();
	return;
}

static void fm34_set_gpio_parameter_rst(void)
{
	fm34_pr_func_start();

	if (g_fm34_pdata->gpio_rst) {
		/* RST# low */
		gpio_set_value(g_fm34_pdata->gpio_rst, FM34_RST_ENABLE);
		fm34_pr_debug("gpio_rst[0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_rst));

		usleep_range(10000, 10000);

		/* RST# high */
		gpio_set_value(g_fm34_pdata->gpio_rst, FM34_RST_DISABLE);
		fm34_pr_debug("gpio_rst[0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_rst));

		usleep_range(1000, 1000);
	}

	fm34_pr_func_end();
	return;
}

static void fm34_wakeup(void)
{
	fm34_pr_func_start();

	/* PWDN# release */
	fm34_set_gpio_pwdn_disable();

	/* BP# release */
	fm34_set_gpio_bp_disable();

	/* parameter rst */
	fm34_set_gpio_parameter_rst();

	fm34_pr_func_end();
	return;
}

static int fm34_set_bypass_mode(void)
{
	int rc = 0, size = 0;
	unsigned char *i2c_cmds;

	fm34_pr_func_start();

	i2c_cmds = bypass_cmd;
	size = sizeof(bypass_cmd);

	/* bypass mode parameter send */
	rc = fm34_i2c_write(i2c_cmds, size);
	if (rc < 0)
		fm34_pr_err("failed return [%d]\n", rc);

	fm34_pr_func_end();
	return rc;
}

static int fm34_set_HS_mode(void)
{
	int rc = 0, size = 0;
	unsigned char *i2c_cmds;

	fm34_pr_func_start();

	/* select WB/NB parameter */
	if (g_fm34_curt_band == VCD_CODEC_NB) {
		fm34_pr_debug("---- NB [0x%08x]\n", g_fm34_curt_band);
		i2c_cmds = _HS_Mode_NB;
		size = sizeof(_HS_Mode_NB);
	} else {
		fm34_pr_debug("---- WB [0x%08x]\n", g_fm34_curt_band);
		i2c_cmds = _HS_Mode_WB;
		size = sizeof(_HS_Mode_WB);
	}

	/* HS mode parameter send */
	rc = fm34_i2c_write(i2c_cmds, size);
	if (rc < 0)
		fm34_pr_err("failed return [%d]\n", rc);

	fm34_pr_func_end();
	return rc;
}

static int fm34_set_voice_mode(void)
{
	int rc = 0;
	fm34_pr_func_start();

	fm34_wakeup();

	/* vclk4 enable */
	rc = fm34_enable_vclk4();
	if (rc < 0) {
		fm34_pr_err("vclk4 enable failed return [%d]\n", rc);
		goto err_voice_vclk4;
	}

	usleep_range(10000, 10000);

	/* HS mode i2c setting */
	rc = fm34_set_HS_mode();
	if (rc < 0) {
		fm34_pr_err("HS mode failed return [%d]\n", rc);
		goto err_voice_hsmode;
	}

	fm34_pr_func_end();
	return 0;

err_voice_vclk4:
	fm34_disable_vclk4();
err_voice_hsmode:
	fm34_set_gpio_bp_enable();
	fm34_set_gpio_pwdn_enable();

	fm34_pr_func_end();
	return rc;
}

static int fm34_set_idle_mode(void)
{
	int rc = 0;
	fm34_pr_func_start();

	fm34_wakeup();

	/* vclk4 enable */
	rc = fm34_enable_vclk4();
	if (rc < 0) {
		fm34_pr_err("vclk4 enable failed return [%d]\n", rc);
		goto err_idle_vclk4;
	}

	usleep_range(10000, 10000);

	/* Bypass mode i2c setting */
	rc = fm34_set_bypass_mode();
	if (rc < 0)
		fm34_pr_err("bypass mode failed return [%d]\n", rc);

	/* disable vclk4 */
	fm34_disable_vclk4();

	usleep_range(10000, 10000);

err_idle_vclk4:
	fm34_set_gpio_bp_enable();
	fm34_set_gpio_pwdn_enable();

	fm34_pr_func_end();
	return rc;
}

static int fm34_nop(void)
{
	fm34_pr_func_start();

	/* Kept last status. */

	fm34_pr_func_end();
	return 0;
}

static int fm34_error(void)
{
	fm34_pr_func_start();

	/* Status change error. */
	/* It should be called in case of state change error. */

	fm34_pr_debug("g_fm34_curt_status[%d] g_fm34_event[%d]\n",
			g_fm34_curt_status, g_fm34_event);
	fm34_pr_func_end();
	return -EINVAL;
}

static int fm34_suspend(struct device *dev)
{
	return 0;
}

static int fm34_resume(struct device *dev)
{
	return 0;
}

static int fm34_runtime_suspend(struct device *dev)
{
	return 0;
}

static int fm34_runtime_resume(struct device *dev)
{
	return 0;
}

static bool fm34_is_bt_mhl(unsigned int device)
{
	bool bt_mhl = false;

	fm34_pr_func_start();

	/* Check device from sound driver. */
	if ((device & SNDP_OUT_BLUETOOTH_SCO) ||
		(device & SNDP_OUT_BLUETOOTH_SCO_HEADSET) ||
		(device & SNDP_OUT_BLUETOOTH_SCO_CARKIT) ||
		(device & SNDP_OUT_BLUETOOTH_A2DP) ||
		(device & SNDP_OUT_BLUETOOTH_A2DP_HEADPHONES) ||
		(device & SNDP_OUT_BLUETOOTH_A2DP_SPEAKER) ||
		(device & SNDP_OUT_AUX_DIGITAL) ||
		(device & SNDP_IN_BLUETOOTH_SCO_HEADSET) ||
		(device & SNDP_IN_AUX_DIGITAL)) {
		bt_mhl = true;
	}

	fm34_pr_func_end();
	return bt_mhl;
}

static int fm34_create_event
	(unsigned int mode, unsigned int device, unsigned int ch_dev)
{
	int event;

	if (fm34_is_bt_mhl(device)) {
		/* event update */
		event = FM34_EVENT_IDLE;
	} else {
		if ((SNDP_EXTDEV_STOP == ch_dev)) {
			/* event update */
			event = FM34_EVENT_IDLE;
		} else {
			/* earpiece incall */
			if ((SNDP_MODE_INCALL == mode) &&
			    (SNDP_OUT_EARPIECE == device)) {
				/* event update */
				event = FM34_EVENT_VOICE;
			} else {
				/* event update */
				event = FM34_EVENT_BYPASS;
			}
		}
	}
	fm34_pr_debug("event[%d]\n", event);
	return event;
}

static int fm34_set_mode
	(unsigned int mode, unsigned int device, unsigned int ch_dev)
{
	int ret = 0;
#if FM34_BYPASS_ONLY
#else
	struct fm34_table *table;
#endif
	fm34_pr_if_start("mode[0x%x] device[0x%x] ch_dev[0x%x]\n",
			mode, device, ch_dev);

	mutex_lock(&g_fm34_lock);

#if FM34_BYPASS_ONLY
	/* Change path (bypass-mode). */
	ret = fm34_set_hw_bypass_mode();
	g_fm34_curt_status = FM34_STATUS_BYPASS;
#else

	/* Create next event. */
	g_fm34_event = fm34_create_event(mode, device, ch_dev);

	fm34_pr_debug("status[%d] event[%d]\n",
		g_fm34_curt_status, g_fm34_event);

	/* Get state table. */
	table = fm34_state_table[g_fm34_curt_status][g_fm34_event];

	/* Call function of state table. */
	ret = table->exec_func();
	if ((0 == ret) && (FM34_STATUS_NOP != table->next_state)) {
		fm34_pr_debug("status change -- [%d] -> [%d]\n",
				g_fm34_curt_status, table->next_state);
		/* Update state. */
		g_fm34_curt_status = table->next_state;
	} else {
		/* Maintain current state */
		fm34_pr_debug("do not change status[%d]\n", ret);
	}
#endif
	mutex_unlock(&g_fm34_lock);

	fm34_pr_if_end("ret[%d]\n", ret);
	return ret;
}

static int fm34_set_nb_wb(unsigned int nb_wb)
{
	int ret = 0;

	fm34_pr_if_start("nb_wb[0x%x]\n", nb_wb);

	/* Check BAND setting from sound driver */
	if (g_fm34_curt_band == nb_wb) {
		fm34_pr_debug("already configured this path!!!\n");
		return 0;
	}

	mutex_lock(&g_fm34_lock);

	fm34_pr_debug("status[%d] event[%d]\n",
		g_fm34_curt_status, g_fm34_event);

	g_fm34_curt_band = nb_wb;

	/* Change table if FM34 is a VOICE state */
	if (g_fm34_curt_status == FM34_STATUS_VOICE) {
		ret = fm34_set_HS_mode();
		if (ret != 0)
			return ret;
	}

	mutex_unlock(&g_fm34_lock);

	fm34_pr_if_end("ret[%d]\n", ret);
	return 0;
}

static int fm34_gpio_init(void)
{
	int ret = 0;

	fm34_pr_func_start();

	if ((g_fm34_pdata->gpio_rst) &&
		(g_fm34_pdata->gpio_pwdn) &&
		(g_fm34_pdata->gpio_bp)) {

		gpio_request(g_fm34_pdata->gpio_rst, "FM34_RESET");
		gpio_direction_output(g_fm34_pdata->gpio_rst,
			FM34_RST_DISABLE);
		gpio_set_value(g_fm34_pdata->gpio_rst,
			FM34_RST_ENABLE);
		fm34_pr_debug("gpio_rst[0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_rst));
		usleep_range(10000, 10000);

		gpio_request(g_fm34_pdata->gpio_pwdn, "FM34_PWDN");
		gpio_direction_output(g_fm34_pdata->gpio_pwdn,
			FM34_PWDN_DISABLE);
		gpio_set_value(g_fm34_pdata->gpio_pwdn,
			FM34_PWDN_DISABLE);
		fm34_pr_debug("gpio_pwdn[0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_pwdn));

		gpio_request(g_fm34_pdata->gpio_bp, "FM34_BYPASS");
		gpio_direction_output(g_fm34_pdata->gpio_bp,
			FM34_BP_DISABLE);
		gpio_set_value(g_fm34_pdata->gpio_bp,
			FM34_BP_DISABLE);
		fm34_pr_debug("gpio_bp[0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_bp));
	} else {
		ret = -EINVAL;
		fm34_pr_err("gpio not active.[%d]\n", ret);
	}

	fm34_pr_func_end();
	return ret;
}

static void fm34_gpio_free(void)
{
	fm34_pr_func_start();

	if ((g_fm34_pdata->gpio_rst) &&
		(g_fm34_pdata->gpio_pwdn) &&
		(g_fm34_pdata->gpio_bp)) {

		gpio_free(g_fm34_pdata->gpio_rst);
		gpio_free(g_fm34_pdata->gpio_pwdn);
		gpio_free(g_fm34_pdata->gpio_bp);
	}

	fm34_pr_func_end();
}

static int fm34_read_proc(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	int len = 0;

	len = snprintf(page, count, "0x%x\n", g_fm34_log_level);

	return len;
}

static int fm34_write_proc(struct file *filp, const char __user *buffer,
					unsigned long len, void *data)
{
	int rc = 0;
	unsigned char buf[11] = {0};
	unsigned int code = 0;

	if (11 <= len) {
		/* size over */
		return len;
	}

	if (copy_from_user(buf, buffer, len)) {
		/* failed copy_from_user */
		return len;
	}

	rc = kstrtouint(buf, 0, &code);
	if (0 != rc) {
		/* kstrtouint failed */
		return len;
	}

	/* Set log level. */
	if (!(FM34_LOG_LEVEL_CHECK & code))
		g_fm34_log_level = code;

	return len;
}
static int fm34_read_exec_proc(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	return count;
}

static int fm34_write_exec_proc(struct file *filp, const char __user *buffer,
					unsigned long len, void *data)
{
	int rc = 0;
	unsigned char buf[11] = {0};
	unsigned int code = 0;

	if (11 <= len) {
		/* size over */
		return len;
	}

	if (copy_from_user(buf, buffer, len)) {
		/* failed copy_from_user */
		return len;
	}

	rc = kstrtouint(buf, 0, &code);
	if (0 != rc) {
		/* kstrtouint failed */
		return len;
	}

	switch (code) {
	case 0x0:
		/* Change path (bypass-mode). */
		rc = fm34_set_idle_mode();
		if (rc < 0)
			fm34_pr_err("idle mode failed [%d]\n", rc);

		g_fm34_event = FM34_EVENT_IDLE;
		g_fm34_curt_status = FM34_STATUS_IDLE;
		break;
	case 0x1:
		/* Change path (bypass-mode). */
		rc = fm34_set_voice_mode();
		if (rc < 0)
			fm34_pr_err("voice mode failed [%d]\n", rc);

		g_fm34_event = FM34_EVENT_VOICE;
		g_fm34_curt_status = FM34_STATUS_VOICE;
		break;
	case 0x2:
		break;
	case 0x3:
		rc = fm34_set_nb_wb(VCD_CODEC_WB);
		if (rc < 0)
			fm34_pr_err("WB mode failed [%d]\n", rc);
		break;
	case 0x4:
		rc = fm34_set_nb_wb(VCD_CODEC_NB);
		if (rc < 0)
			fm34_pr_err("WB mode failed [%d]\n", rc);
		break;
	case 0x5:
		/* Change path (voice process). */
		rc = fm34_set_HS_mode();
		if (rc < 0)
			fm34_pr_err("HS mode failed [%d]\n", rc);

		g_fm34_curt_status = FM34_STATUS_VOICE;
		break;
	case 0x6:
		/* FM34 parameter area initialized. */
		rc = fm34_set_bypass_mode();
		if (rc < 0)
			fm34_pr_err("bypass setting failed [%d]\n", rc);
		break;
	case 0x10:
		fm34_pr_debug("g_fm34_event      [0x%08x]\n",
			g_fm34_event);
		fm34_pr_debug("g_fm34_curt_status[0x%08x]\n",
			g_fm34_curt_status);
		fm34_pr_debug("g_fm34_curt_band  [0x%08x]\n",
			g_fm34_curt_band);
		fm34_pr_debug("g_fm34_vclk_adr   [0x%08x]\n",
			ioread32(g_fm34_vclk_adr));
		fm34_pr_debug("g_fm34_log_level  [0x%08x]\n",
			g_fm34_log_level);
		fm34_pr_debug("gpio_rst          [0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_rst));
		fm34_pr_debug("gpio_pwdn         [0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_pwdn));
		fm34_pr_debug("gpio_bp           [0x%08x]\n",
			gpio_get_value(g_fm34_pdata->gpio_bp));
		break;
	default:
		break;
	}

	return len;
}

static int fm34_create_proc_entry(void)
{
	int rc = 0;
	struct proc_dir_entry *log_level = NULL;
	struct proc_dir_entry *exec = NULL;

	/* Create FM34 directory */
	 g_fm34_parent = proc_mkdir(FM34_MODULE_NAME, NULL);
	if (NULL != g_fm34_parent) {
		/* Log level procfile create.  */
		log_level = create_proc_entry("log_level",
				(S_IFREG | S_IRUGO | S_IWUGO), g_fm34_parent);
		if (NULL != log_level) {
			log_level->read_proc  = fm34_read_proc;
			log_level->write_proc = fm34_write_proc;
		} else {
			rc = -EACCES;
			goto rm_dir;
		}

		/* Exec procfile create.  */
		exec = create_proc_entry("exec",
				(S_IFREG | S_IRUGO | S_IWUGO), g_fm34_parent);
		if (NULL != exec) {
			exec->read_proc  = fm34_read_exec_proc;
			exec->write_proc = fm34_write_exec_proc;
		} else {
			rc = -EACCES;
			goto rm_dir;
		}

	} else {
		rc = -EACCES;
		return rc;
	}
	return rc;

rm_dir:
	remove_proc_entry(FM34_MODULE_NAME, NULL);
	g_fm34_parent = NULL;
	return rc;
}

static void fm34_remove_proc_entry(void)
{
	/* Delete FM34 directory */
	if (NULL != g_fm34_parent) {
		remove_proc_entry("exec", g_fm34_parent);
		remove_proc_entry("log_level", g_fm34_parent);
		remove_proc_entry(FM34_MODULE_NAME, NULL);
		g_fm34_parent = NULL;
	}
}

static int fm34_enable_vclk4(void)
{
	int ret = 0;

	fm34_pr_func_start();

	if (g_fm34_is_vclk4_enable == false) {
		ret = clk_enable(g_fm34_vclk4_clk);
		if (0 != ret) {
			fm34_pr_err("clk_enable error[%d]\n", ret);
			return ret;
		}
		g_fm34_is_vclk4_enable = true;
		usleep_range(1000, 1000);
	} else {
		fm34_pr_debug("already enable vclk4.\n");
	}

	fm34_pr_debug("g_fm34_vclk_adr[0x%08x]\n", ioread32(g_fm34_vclk_adr));

	fm34_pr_func_end();
	return 0;
}

static void fm34_disable_vclk4(void)
{
	fm34_pr_func_start();

	if (g_fm34_is_vclk4_enable == true) {
		clk_disable(g_fm34_vclk4_clk);
		g_fm34_is_vclk4_enable = false;
	} else {
		fm34_pr_debug("already disabled vclk4.\n");
	}

	fm34_pr_debug("g_fm34_vclk_adr[0x%08x]\n", ioread32(g_fm34_vclk_adr));

	fm34_pr_func_end();
}

static int fm34_bootup_init(void)
{
	int ret = 0;

	fm34_pr_func_start();

	/* enable vclk4 */
	ret = fm34_enable_vclk4();
	if (0 == ret) {
		/* fm34 reset */
		if (g_fm34_pdata->gpio_rst) {
			msleep(20);
			gpio_set_value(g_fm34_pdata->gpio_rst,
				FM34_RST_ENABLE);
			fm34_pr_debug("gpio_rst[0x%08x]\n",
				gpio_get_value(g_fm34_pdata->gpio_rst));
			msleep(20);
			gpio_set_value(g_fm34_pdata->gpio_rst,
				FM34_RST_DISABLE);
			fm34_pr_debug("gpio_rst[0x%08x]\n",
				gpio_get_value(g_fm34_pdata->gpio_rst));
			msleep(50);
		}

		/* disable vclk4 */
		fm34_disable_vclk4();
	}

	fm34_pr_func_end();
	return ret;
}

static int fm34_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct clk *local_main_clk = NULL;
	static struct fm34_platform_data *pdata;

	fm34_pr_func_start();

	/* Check platform data. */
	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		ret = -EINVAL;
		fm34_pr_err("platform data is NULL[%d]\n", ret);
		goto err_alloc_data_failed;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ret = -EINVAL;
		fm34_pr_err("i2c check functionality error[%d]\n", ret);
		goto err_alloc_data_failed;
	}

	/* Keep platform data of global area. */
	g_fm34_pdata = client->dev.platform_data;

	i2c_set_clientdata(client, NULL);
	g_fm34_client = client;

	/* Mutex init. */
	mutex_init(&g_fm34_lock);

	/* GPIO initialized. */
	ret = fm34_gpio_init();
	if (0 != ret) {
		fm34_pr_err("gpio init error[%d]\n", ret);
		goto err_init_gpio_failed;
	}

	g_fm34_vclk_adr = ioremap(VCLKCR4_PHYS, FM34_VCLKCR4_REGSIZE);

	if (!g_fm34_vclk_adr) {
		ret = -EINVAL;
		fm34_pr_err("vclk4 ioremap error\n");
		goto err_ioremap_vclk4_failed;
	}

	g_fm34_vclk4_clk = clk_get(NULL, "vclk4_clk");
	if (IS_ERR(g_fm34_vclk4_clk)) {
		ret = IS_ERR(g_fm34_vclk4_clk);
		fm34_pr_err("clk_get(vclk) error[%d]\n", ret);
		goto err_clk_get_vclk4;
	}

	local_main_clk = clk_get(NULL, "main_clk");
	if (IS_ERR(local_main_clk)) {
		ret = IS_ERR(local_main_clk);
		fm34_pr_err("clk_get(main) error[%d]\n", ret);
		goto err_clk_get_main;
	}

	ret = clk_set_parent(g_fm34_vclk4_clk, local_main_clk);
	if (0 != ret) {
		fm34_pr_err("clk_set_parent error[%d]\n", ret);
		goto err_clk_set_parent;
	}

	ret = clk_set_rate(g_fm34_vclk4_clk, 13000000);
	if (0 != ret) {
		fm34_pr_err("clk_set_rate error[%d]\n", ret);
		goto err_clk_set_parent;
	}

	clk_put(local_main_clk);
	local_main_clk = NULL;

	/* FM34 reset. */
	ret = fm34_bootup_init();
	if (0 != ret) {
		fm34_pr_err("bootup init error[%d]\n", ret);
		goto err_clk_set_parent;
	}

	/* set callback function */
	g_fm34_callback_func.set_state = fm34_set_mode;
	g_fm34_callback_func.set_nb_wb = fm34_set_nb_wb;

	/* Register sound driver callback */
	sndp_extdev_regist_callback(&g_fm34_callback_func);

	/* Create platform driver. */
	ret = platform_device_register(&fm34_platform_device);
	if (0 != ret) {
		fm34_pr_err("platform_device_register failed.[%d]\n",
			 ret);
		goto err_clk_set_parent;
	}

	ret = platform_driver_register(&fm34_platform_driver);
	if (0 != ret) {
		fm34_pr_err("platform_driver_register failed.[%d]\n",
			ret);
		goto err_pf_driver_failed;
	}

	/* Create proc. */
	ret = fm34_create_proc_entry();
	if (0 != ret) {
		fm34_pr_err("create_proc_entry failed.[%d]\n", ret);
		goto err_create_proc_failed;
	}

	fm34_pr_func_end();

	g_fm34_log_level = FM34_LOG_ERR;

	/* Change path (bypass-mode). */
	ret = fm34_set_idle_mode();
	if (ret < 0) {
		fm34_pr_err("bypass setting failed [%d]\n", ret);
		goto err_bypass_mode_failed;
	}

	/* Global parameter initialized. */
	g_fm34_event = 0;
	g_fm34_curt_status = FM34_STATUS_IDLE;
	g_fm34_curt_band = VCD_CODEC_WB;

	return 0;

err_bypass_mode_failed:
	fm34_remove_proc_entry();
err_create_proc_failed:
	platform_driver_unregister(&fm34_platform_driver);
err_pf_driver_failed:
	platform_device_unregister(&fm34_platform_device);
err_clk_set_parent:
	if (local_main_clk != NULL)
		clk_put(local_main_clk);
err_clk_get_main:
	clk_put(g_fm34_vclk4_clk);
err_clk_get_vclk4:
	iounmap(g_fm34_vclk_adr);
err_ioremap_vclk4_failed:
	fm34_gpio_free();
err_init_gpio_failed:
	mutex_destroy(&g_fm34_lock);
err_alloc_data_failed:
	fm34_pr_func_end();
	return ret;
}

static int fm34_remove(struct i2c_client *client)
{
	struct fm34_platform_data *pfm34data = i2c_get_clientdata(client);

	fm34_pr_func_start();

	/* register function remove */
	fm34_remove_proc_entry();
	platform_driver_unregister(&fm34_platform_driver);
	platform_device_unregister(&fm34_platform_device);
	fm34_disable_vclk4();
	clk_put(g_fm34_vclk4_clk);
	iounmap(g_fm34_vclk_adr);
	fm34_gpio_free();
	mutex_destroy(&g_fm34_lock);
	kfree(pfm34data);

	fm34_pr_func_end();
	return 0;
}

static const struct i2c_device_id fm34_id[] = {
	{ FM34_MODULE_NAME, 0 },
	{ }
};

static struct i2c_driver fm34_driver = {
	.probe = fm34_probe,
	.remove = fm34_remove,
	.id_table = fm34_id,
	.driver = {
		.name = FM34_MODULE_NAME,
	},
};

static int __init fm34_init(void)
{
	return i2c_add_driver(&fm34_driver);
}

static void __exit fm34_exit(void)
{
	i2c_del_driver(&fm34_driver);
}

module_init(fm34_init);
module_exit(fm34_exit);

MODULE_DESCRIPTION("fm34 voice processor driver");
MODULE_LICENSE("GPL");
