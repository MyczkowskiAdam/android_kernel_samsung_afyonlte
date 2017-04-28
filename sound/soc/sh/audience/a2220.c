/* drivers/i2c/chips/a2220.c - a2220 voice processor driver
 * sound/soc/sh/audience/a2220.c - a2220 voice processor driver
 *
 * Copyright (C) 2009 HTC Corporation.
 * Copyright (C) 2013 Renesas Mobile Corp.
 *
 * Complete rewrite,  anish kumar (anish.singh@samsung.com)
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
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <sound/a2220.h>
#include <linux/kthread.h>
#include <linux/atomic.h>

#include "a2220_b17832.h"
#include <linux/firmware.h>
#ifdef __A2220_ORGINAL__
#include <mach/msm_xo.h>
#endif /* __A2220_ORGINAL__ */

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <mach/common.h>
#include <mach/r8a7373.h>
#include "sound/soundpath/soundpath.h"
#include <linux/vcd/vcd.h>
#include <linux/platform_device.h>

#include <linux/module.h>

#define ENABLE_DIAG_IOCTLS	(0)

struct a2220_data {
	struct i2c_client *this_client;
	struct mutex lock;
	struct a2220_platform_data *pdata;
	struct miscdevice device;
	atomic_t opened;
	int suspended;
};

static int execute_cmdmsg(struct a2220_data *a2220, unsigned int);
static unsigned int a2220_NS_state = A2220_NS_STATE_AUTO;
static int a2220_current_config = A2220_PATH_SUSPEND;

static int a2220_enable_vclk4(void);
static int a2220_disable_vclk4(void);
static int a2220_i2c_cmd_execute(char *i2c_cmds, int size);
static int a2220_idle_suspend
	(unsigned int mode, unsigned int device, unsigned int ch_dev);
static int a2220_idle
	(unsigned int mode, unsigned int device, unsigned int ch_dev);
static int a2220_pass_through
	(unsigned int mode, unsigned int device, unsigned int ch_dev);
static int a2220_voice_process
	(unsigned int mode, unsigned int device, unsigned int ch_dev);
static int a2220_set_state
	(unsigned int mode, unsigned int device, unsigned int ch_dev);
static int a2220_set_nb_wb(unsigned int nb_wb);
static int a2220_set_algo_sample_rate(unsigned int rate);
static bool a2220_bt_mhl_check(unsigned int device);
static bool a2220_earpiece_incall_check
	(unsigned int mode, unsigned int device, unsigned int ch_dev);
static int a2220_wa_suspend(struct device *dev);
static int a2220_wa_resume(struct device *dev);
static int a2220_wa_runtime_suspend(struct device *dev);
static int a2220_wa_runtime_resume(struct device *dev);

static unsigned int g_a2220_current_state;
static unsigned int g_a2220_current_mode;
static unsigned int g_a2220_current_device;
static unsigned int g_a2220_suspend_state;
static unsigned int g_a2220_start;
static unsigned int g_a2220_vclk_adr;
static struct a2220_data *g_a2220_data;
static struct sndp_extdev_callback_func g_a2220_callback_func;
static struct clk *g_a2220_vclk4_clk;
static struct proc_dir_entry *g_a2220_parent;
unsigned int g_a2220_log_level;
static unsigned int g_a2220_current_nb_wb;
static unsigned int g_a2220_nb_wb;

/*
 * callback object
 */
static const struct dev_pm_ops a2220_dev_pm_ops = {
	.suspend		= a2220_wa_suspend,
	.resume			= a2220_wa_resume,
	.runtime_suspend	= a2220_wa_runtime_suspend,
	.runtime_resume		= a2220_wa_runtime_resume,
};

/*
 * device object
 */
static struct platform_device a2220_platform_device = {
	.name = "audience_a2220",
};

/*
 * driver object
 */
static struct platform_driver a2220_platform_driver = {
	.driver		= {
		.name	= "audience_a2220",
		.pm	= &a2220_dev_pm_ops,
		.probe = NULL,
		.remove = NULL,
	},
};


struct vp_ctxt {
	unsigned char *data;
	unsigned int img_size;
};

struct vp_ctxt the_vp;

#ifdef __A2220_ORGINAL__
static struct msm_xo_voter *xo;
#endif /* __A2220_ORGINAL__ */

#ifdef __A2220_ORGINAL__
static int xoclk_control(bool onoff)
{
	pr_debug("%s onoff %d\n", __func__, onoff);
	if (!xo) {
		pr_err("%s XO Clock is not available"
			" for Audience!!\n", __func__);
		return -EAGAIN;
	}

	if (onoff)
		msm_xo_mode_vote(xo, MSM_XO_MODE_ON);
	else
		msm_xo_mode_vote(xo, MSM_XO_MODE_OFF);
	return 0;
}
#endif /* __A2220_ORGINAL__ */


static int a2220_i2c_read(struct a2220_data *a2220, char *rxData, int length)
{
	int rc;
	struct i2c_msg msgs[] = {
		{
			.addr = a2220->this_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		},
	};

	rc = i2c_transfer(a2220->this_client->adapter, msgs, 1);
	if (rc < 0) {
		pr_err("%s: transfer error %d\n", __func__, rc);
		return rc;
	}

	{
		int i = 0;
		for (i = 0; i < length; i++)
			a2220_pr_debug("rx[%d] = %2x\n", i, rxData[i]);
	}
	return 0;
}

static int a2220_i2c_write(struct a2220_data *a2220, char *txData, int length)
{
	int rc;
	struct i2c_msg msg[] = {
		{
			.addr = a2220->this_client->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};

	rc = i2c_transfer(a2220->this_client->adapter, msg, 1);
	if (rc < 0) {
		pr_err("%s: transfer error %d\n", __func__, rc);
		return rc;
	}

	{
		int i = 0;
		for (i = 0; i < length; i++)
			a2220_pr_debug("tx[%d] = %2x\n", i, txData[i]);
	}
	return 0;
}

static int a2220_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct a2220_data *a2220 = container_of(file->private_data,
			struct a2220_data, device);

	if (!atomic_dec_and_test(&a2220->opened)) {
		pr_err("audience device busy\n");
		atomic_inc(&a2220->opened);
		return -EBUSY;
	}
	return rc;
}

static int a2220_release(struct inode *inode, struct file *file)
{
	struct a2220_data *a2220 = container_of(file->private_data,
			struct a2220_data, device);
	atomic_inc(&a2220->opened); /* release the device */
	return 0;
}

#ifdef AUDIENCE_BYPASS
#define A100_msg_mutimedia1   0x801C0000
#define A100_msg_mutimedia2   0x8026001F
#define A100_msg_mutimedia3   0x800C0B03
#define A100_msg_mutimedia4   0x800D0001
#define A100_msg_mutimedia5   0x800C0A03
#define A100_msg_mutimedia6   0x800D0001
#endif

unsigned char port_a_init_setting[] = {
	/* wordlength 16bit(default) */
	0x80, 0x0c, 0x0a, 0x00, 0x80, 0x0d, 0x00, 0x0f,
	/* delfromfstx Min(default) */
	0x80, 0x0c, 0x0a, 0x02, 0x80, 0x0d, 0x00, 0x00,
	/* delfromfstx Min(default) */
	0x80, 0x0c, 0x0a, 0x03, 0x80, 0x0d, 0x00, 0x00,
	/* latchedge PCM */
	0x80, 0x0c, 0x0a, 0x04, 0x80, 0x0d, 0x00, 0x03,
	/* endiannes little */
	0x80, 0x0c, 0x0a, 0x05, 0x80, 0x0d, 0x00, 0x00,
	/* tristateenable enable(default) */
	0x80, 0x0c, 0x0a, 0x06, 0x80, 0x0d, 0x00, 0x01,
	/* audio port mode PCM */
	0x80, 0x0c, 0x0a, 0x07, 0x80, 0x0d, 0x00, 0x00,
};

unsigned char port_c_init_setting[] = {
	/* wordlength 16bit(default) */
	0x80, 0x0c, 0x0c, 0x00, 0x80, 0x0d, 0x00, 0x0f,
	/* delfromfstx Min(default) */
	0x80, 0x0c, 0x0c, 0x02, 0x80, 0x0d, 0x00, 0x00,
	/* delfromfstx Min(default) */
	0x80, 0x0c, 0x0c, 0x03, 0x80, 0x0d, 0x00, 0x00,
	/* latchedge PCM */
	0x80, 0x0c, 0x0c, 0x04, 0x80, 0x0d, 0x00, 0x03,
	/* endiannes little */
	0x80, 0x0c, 0x0c, 0x05, 0x80, 0x0d, 0x00, 0x00,
	/* tristateenable enable(default) */
	0x80, 0x0c, 0x0c, 0x06, 0x80, 0x0d, 0x00, 0x01,
	/* audio port mode PCM */
	0x80, 0x0c, 0x0c, 0x07, 0x80, 0x0d, 0x00, 0x00,
};

unsigned char idle_to_pass_through[] = {
	0x80, 0x52, 0x00, 0x48,	/* enable pass through */
	0x80, 0x10, 0x00, 0x01,	/* sleep */
};

unsigned char idle_to_voice[] = {
	0x80, 0x52, 0x00, 0x48,	/* enable pass through */
	0x80, 0x1c, 0x00, 0x01,	/* enable voice process */
	0x80, 0x43, 0x00, 0x00,	/* check voice process */
};

unsigned char idle_to_voice_nb[] = {
	0x80, 0x52, 0x00, 0x48,	/* enable pass through */
	0x80, 0x1c, 0x00, 0x01,	/* enable voice process */
	0x80, 0x43, 0x00, 0x00,	/* check voice process */
	0x80, 0x4c, 0x00, 0x00,	/* Set Algo Sample Rate(NB) */
	0x80, 0x4f, 0x00, 0x00,	/* Get Change Status(success) */
};

unsigned char idle_to_voice_wb[] = {
	0x80, 0x52, 0x00, 0x48,	/* enable pass through */
	0x80, 0x1c, 0x00, 0x01,	/* enable voice process */
	0x80, 0x43, 0x00, 0x00,	/* check voice process */
	0x80, 0x4c, 0x00, 0x01,	/* Set Algo Sample Rate(WB) */
	0x80, 0x4f, 0x00, 0x00,	/* Get Change Status(success) */
};

unsigned char pass_through_to_idle[] = {
	0x80, 0x52, 0x00, 0x48,	/* enable pass through */
	0x80, 0x52, 0x00, 0x00,	/* disable pass through */
	0x80, 0x10, 0x00, 0x01,	/* sleep */
};

unsigned char pass_through_to_voice[] = {
	0x80, 0x1c, 0x00, 0x01,	/* enable voice process */
	0x80, 0x43, 0x00, 0x00,	/* check voice process */
};

unsigned char pass_through_to_voice_nb[] = {
	0x80, 0x1c, 0x00, 0x01,	/* enable voice process */
	0x80, 0x43, 0x00, 0x00,	/* check voice process */
	0x80, 0x4c, 0x00, 0x00,	/* Set Algo Sample Rate(NB) */
	0x80, 0x4f, 0x00, 0x00,	/* Get Change Status(success) */
};

unsigned char pass_through_to_voice_wb[] = {
	0x80, 0x1c, 0x00, 0x01,	/* enable voice process */
	0x80, 0x43, 0x00, 0x00,	/* check voice process */
	0x80, 0x4c, 0x00, 0x01,	/* Set Algo Sample Rate(WB) */
	0x80, 0x4f, 0x00, 0x00,	/* Get Change Status(success) */
};

unsigned char voice_to_idle[] = {
	0x80, 0x1c, 0x00, 0x00,	/* disable voice process */
	0x80, 0x43, 0x00, 0x00,	/* check voice process */
	0x80, 0x52, 0x00, 0x00,	/* disable pass through */
	0x80, 0x10, 0x00, 0x01,	/* sleep */
};

unsigned char voice_to_pass_through[] = {
	0x80, 0x1c, 0x00, 0x00,	/* disable voice process */
	0x80, 0x43, 0x00, 0x00,	/* check voice process */
	0x80, 0x10, 0x00, 0x01,	/* sleep */
};

unsigned char set_algo_nb[] = {
	0x80, 0x4c, 0x00, 0x00,	/* Set Algo Sample Rate(NB) */
	0x80, 0x4f, 0x00, 0x00,	/* Get Change Status(success) */
};

unsigned char set_algo_wb[] = {
	0x80, 0x4c, 0x00, 0x01,	/* Set Algo Sample Rate(WB) */
	0x80, 0x4f, 0x00, 0x00,	/* Get Change Status(success) */
};

static void a2220_i2c_sw_reset(struct a2220_data *a2220, unsigned int reset_cmd)
{
	int rc = 0;
	unsigned char msgbuf[4];

	msgbuf[0] = (reset_cmd >> 24) & 0xFF;
	msgbuf[1] = (reset_cmd >> 16) & 0xFF;
	msgbuf[2] = (reset_cmd >> 8) & 0xFF;
	msgbuf[3] = reset_cmd & 0xFF;

	pr_info("%s: %08x\n", __func__, reset_cmd);

	rc = a2220_i2c_write(a2220, msgbuf, 4);
	if (!rc)
		msleep(20);
}

	static ssize_t
a2220_hw_reset(struct a2220_data *a2220, struct a2220img *img)
{
	struct a2220img *vp = img;
	int rc, i, size, pass = 0;
	int remaining;
	int retry = RETRY_CNT;
	unsigned char *index;
	char buf[2];
	unsigned int msg;
	unsigned char *pMsg;
	unsigned char *i2c_cmds;

	pr_info("%s\n", __func__);
	while (retry--) {
		/* Reset A2220 chip */
		gpio_set_value(a2220->pdata->gpio_reset, 0);

		mdelay(1);

		/* Take out of reset */
		gpio_set_value(a2220->pdata->gpio_reset, 1);

		msleep(50); /* Delay before send I2C command */

		/* Boot Cmd to A2220 */
		buf[0] = A2220_msg_BOOT >> 8;
		buf[1] = A2220_msg_BOOT & 0xff;

		rc = a2220_i2c_write(a2220, buf, 2);
		if (rc < 0) {
			pr_err("%s: set boot mode error (%d retries left)\n",
					__func__, retry);
			continue;
		}

		mdelay(1);
		rc = a2220_i2c_read(a2220, buf, 1);
		if (rc < 0) {
			pr_err("%s: boot mode ack error (%d retries left)\n",
					__func__, retry);
			continue;
		}

		remaining = vp->img_size / 32;
		index = vp->buf;

		for (; remaining; remaining--, index += 32) {
			rc = a2220_i2c_write(a2220, index, 32);
			if (rc < 0)
				break;
		}

		if (rc >= 0 && vp->img_size % 32)
			rc = a2220_i2c_write(a2220, index, vp->img_size % 32);

		if (rc < 0) {
			pr_err("%s: fw load error %d (%d retries left)\n",
					__func__, rc, retry);
			continue;
		}

		msleep(20); /* Delay time before issue a Sync Cmd */

		for (i = 0; i < 10 ; i++)
			msleep(20);

		rc = execute_cmdmsg(a2220, A100_msg_Sync);
		if (rc < 0) {
			pr_err("%s: sync command error %d (%d retries left)\n",
					__func__, rc, retry);
			continue;
		}

		pass = 1;
		break;
	}

	/* port A */
	pMsg = (unsigned char *)&msg;
	i2c_cmds = port_a_init_setting;
	size = sizeof(port_a_init_setting);

	for (i = 0 ; i < size ; i += 4) {
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i+1];
		pMsg[1] = i2c_cmds[i+2];
		pMsg[0] = i2c_cmds[i+3];

		retry = RETRY_CNT;

		do {
			rc = execute_cmdmsg(a2220, msg);
			a2220_pr_info("portA retry:%d, size:%d, rc:%d\n"
				, retry, size, rc);
		} while ((rc < 0) && --retry);

		if (rc < 0)
			a2220_pr_err("portA setting Failed\n");
	}

	/* port C */
	pMsg = (unsigned char *)&msg;
	i2c_cmds = port_c_init_setting;
	size = sizeof(port_c_init_setting);

	for (i = 0 ; i < size ; i += 4) {
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i+1];
		pMsg[1] = i2c_cmds[i+2];
		pMsg[0] = i2c_cmds[i+3];

		retry = RETRY_CNT;

		do {
			rc = execute_cmdmsg(a2220, msg);
			a2220_pr_info("portC retry:%d, size:%d, rc:%d\n"
				, retry, size, rc);
		} while ((rc < 0) && --retry);

		if (rc < 0)
			a2220_pr_err("portC setting Failed\n");
	}

	/* pass setting */
	pMsg = (unsigned char *)&msg;
	if (g_a2220_current_state == A2220_STATE_IDLE) {
		if (g_a2220_suspend_state) {
			/*setting disable pass throungh */
			i2c_cmds = pass_through_to_idle;
			size = sizeof(pass_through_to_idle);
		} else {
			/*setting enable pass throungh */
			i2c_cmds = idle_to_pass_through;
			size = sizeof(idle_to_pass_through);
		}
	} else if (g_a2220_current_state == A2220_STATE_PASS_THROUGH) {
		/*setting enable pass throungh */
		i2c_cmds = idle_to_pass_through;
		size = sizeof(idle_to_pass_through);
	} else {
		if (g_a2220_current_nb_wb == VCD_CODEC_WB) {
			/* enable pass through & voice_process & WB */
			i2c_cmds = idle_to_voice_wb;
			size = sizeof(idle_to_voice_wb);
		} else {
			/* enable pass through & voice_process & NB */
			i2c_cmds = idle_to_voice_nb;
			size = sizeof(idle_to_voice_nb);
		}
	}

	for (i = 0 ; i < size ; i += 4) {
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i+1];
		pMsg[1] = i2c_cmds[i+2];
		pMsg[0] = i2c_cmds[i+3];

		retry = RETRY_CNT;

		do {
			rc = execute_cmdmsg(a2220, msg);
			a2220_pr_info("pass set retry:%d, size:%d, rc:%d\n"
				, retry, size, rc);
		} while ((rc < 0) && --retry);

		if (rc < 0)
			a2220_pr_err("pass setting Failed\n");
	}

	return rc;
}

unsigned char bypass_multimedia[] = {
	0x80, 0x52, 0x00, 0x48,
	0x80, 0x52, 0x00, 0x5C,
	0x80, 0x10, 0x00, 0x01,
};

int a2220_bootup_init(struct a2220_data *a2220, struct a2220img *pImg)
{
	struct a2220img *vp = pImg;
	int rc, pass = 0, size, i;
	unsigned int msg;
	int remaining;
	int retry = RETRY_CNT;
	unsigned char *index;
	unsigned char *pMsg;
	unsigned char *i2c_cmds;
	char buf[2];
#ifdef __A2220_ORGINAL__
	xoclk_control(true);
#endif /* __A2220_ORGINAL__ */

	mdelay(100);
	while (retry--) {
		/* Reset A2220 chip */
		gpio_set_value(a2220->pdata->gpio_reset, 0);

		mdelay(1);

		/* Take out of reset */
		gpio_set_value(a2220->pdata->gpio_reset, 1);

		msleep(50); /* Delay before send I2C command */

		/* Boot Cmd to A2220 */
		buf[0] = A2220_msg_BOOT >> 8;
		buf[1] = A2220_msg_BOOT & 0xff;

		rc = a2220_i2c_write(a2220, buf, 2);
		if (rc < 0) {
			pr_err("%s: set boot mode error (%d retries left)\n",
					__func__, retry);
			continue;
		}

		mdelay(1);
		rc = a2220_i2c_read(a2220, buf, 1);
		if (rc < 0) {
			pr_err("%s: boot mode ack error (%d retries left)\n",
					__func__, retry);
			continue;
		}


		remaining = vp->img_size / 32;
		index = vp->buf;
		pr_info("%s: starting to load image (%d passes)...\n",
				__func__,
				remaining + !!(vp->img_size % 32));

		for (; remaining; remaining--, index += 32) {
			rc = a2220_i2c_write(a2220, index, 32);
			if (rc < 0)
				break;
		}

		if (rc >= 0 && vp->img_size % 32)
			rc = a2220_i2c_write(a2220, index, vp->img_size % 32);

		if (rc < 0) {
			pr_err("%s: fw load error %d (%d retries left)\n",
					__func__, rc, retry);
			continue;
		}
		pr_info("%s:a2220_bootup_init 7\n", __func__);

		msleep(20); /* Delay time before issue a Sync Cmd */

		pr_info("%s:firmware loaded successfully\n", __func__);

		msleep(200);

		rc = execute_cmdmsg(a2220, A100_msg_Sync);
		if (rc < 0) {
			pr_err("%s: sync command error %d (%d retries left)\n",
					__func__, rc, retry);
			continue;
		}
		pr_info("%s:a2220_bootup_init 8\n", __func__);

		pass = 1;
		break;
	}

	/* port A */
	pMsg = (unsigned char *)&msg;
	i2c_cmds = port_a_init_setting;
	size = sizeof(port_a_init_setting);

	for (i = 0 ; i < size ; i += 4) {
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i+1];
		pMsg[1] = i2c_cmds[i+2];
		pMsg[0] = i2c_cmds[i+3];

		retry = RETRY_CNT;

		do {
			rc = execute_cmdmsg(a2220, msg);
			a2220_pr_info("portA retry:%d, size:%d, rc:%d\n"
				, retry, size, rc);
		} while ((rc < 0) && --retry);

		if (rc < 0)
			a2220_pr_err("portA setting Failed\n");
	}

	/* port C */
	pMsg = (unsigned char *)&msg;
	i2c_cmds = port_c_init_setting;
	size = sizeof(port_c_init_setting);

	for (i = 0 ; i < size ; i += 4) {
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i+1];
		pMsg[1] = i2c_cmds[i+2];
		pMsg[0] = i2c_cmds[i+3];

		retry = RETRY_CNT;

		do {
			rc = execute_cmdmsg(a2220, msg);
			a2220_pr_info("portC retry:%d, size:%d, rc:%d\n"
				, retry, size, rc);
		} while ((rc < 0) && --retry);

		if (rc < 0)
			a2220_pr_err("portC setting Failed\n");
	}

	/*setting enable pass throungh */
	pMsg = (unsigned char *)&msg;
	i2c_cmds = idle_to_pass_through;
	size = sizeof(idle_to_pass_through);

	for (i = 0 ; i < size ; i += 4) {
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i+1];
		pMsg[1] = i2c_cmds[i+2];
		pMsg[0] = i2c_cmds[i+3];

		retry = RETRY_CNT;

		do {
			rc = execute_cmdmsg(a2220, msg);
			a2220_pr_info("disable pass & sleep"
				"retry:%d, size:%d, rc:%d\n"
				, retry, size, rc);
		} while ((rc < 0) && --retry);

		if (rc < 0)
			a2220_pr_err("disable pass & sleep Failed\n");
	}

	a2220_disable_vclk4();
	g_a2220_current_state = A2220_STATE_IDLE;
	a2220_pr_info("- finish\n");

	pr_info("%s : a2220_bootup_init - finish\n", __func__);
#ifdef __A2220_ORGINAL__
	xoclk_control(false);
#endif /* __A2220_ORGINAL__ */
	return rc;
}

static unsigned char phonecall_receiver_nson[] = {
	0x80, 0x31, 0x00, 0x00,
};

static unsigned char ft_loopback[] = {
	0x80, 0x31, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x00,
	0x80, 0x1B, 0x01, 0x00,
	0x80, 0x1B, 0x02, 0x00,
	0x80, 0x1B, 0x03, 0x00,
	0x80, 0x1B, 0x04, 0x00,
	0x80, 0x1B, 0x05, 0x00,
	0x80, 0x1B, 0x06, 0x00,
	0x80, 0x1B, 0x07, 0x00,
	0x80, 0x15, 0x00, 0x00,
	0x80, 0x15, 0x01, 0x00,
	0x80, 0x15, 0x02, 0x00,
	0x80, 0x15, 0x03, 0x00,
	0x80, 0x15, 0x04, 0x00,
	0x80, 0x15, 0x05, 0x00,
	0x80, 0x15, 0x06, 0x00,
	0x80, 0x15, 0x07, 0x00,
	0x80, 0x17, 0x00, 0x4B,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x42,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x40,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x0D,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x20,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x1F,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x30,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x31,
	0x80, 0x18, 0x00, 0x00,
};

static unsigned char phonecall_receiver_nson_wb[] = {
	0x80, 0x31, 0x00, 0x02,
};

static unsigned char phonecall_receiver_nsoff[] = {
	0x80, 0x52, 0x00, 0x48,
	0x80, 0x52, 0x00, 0x5C,
	0x80, 0x10, 0x00, 0x01,
};

static unsigned char phonecall_headset[] = {
	0x80, 0x1C, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x03,
	0x80, 0x26, 0x00, 0x1A,
	0x80, 0x1B, 0x00, 0x00,
	0x80, 0x1B, 0x01, 0x00,
	0x80, 0x15, 0x04, 0x00,
	0x80, 0x15, 0x05, 0x00,
	0x80, 0x1B, 0x02, 0x00,
	0x80, 0x1B, 0x03, 0x00,
	0x80, 0x15, 0x06, 0x00,
	0x80, 0x15, 0x07, 0x00,
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x01,
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00,
};

static unsigned char phonecall_speaker[] = {
	0x80, 0x31, 0x00, 0x01,
};

static unsigned char phonecall_bt[] = {
	0x80, 0x17, 0x00, 0x02,
	0x80, 0x18, 0x00, 0x03,
	0x80, 0x26, 0x00, 0x06,
	0x80, 0x1C, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x00,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char phonecall_tty[] = {
	0x80, 0x26, 0x00, 0x15,
	0x80, 0x1C, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x00,
	0x80, 0x15, 0x00, 0xFB,
};

static unsigned char INT_MIC_recording_receiver[] = {
	0x80, 0x26, 0x00, 0x07,
	0x80, 0x1C, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x12,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char EXT_MIC_recording[] = {
	0x80, 0x26, 0x00, 0x15,
	0x80, 0x1C, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x12,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char INT_MIC_recording_speaker[] = {
	0x80, 0x17, 0x00, 0x02,
	0x80, 0x18, 0x00, 0x02,
	0x80, 0x1C, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x12,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char BACK_MIC_recording[] = {
	0x80, 0x17, 0x00, 0x02,
	0x80, 0x18, 0x00, 0x02,
	0x80, 0x26, 0x00, 0x15,
	0x80, 0x1C, 0x00, 0x01,
	0x80, 0x17, 0x00, 0x04,
	0x80, 0x18, 0x00, 0x01,
	0x80, 0x17, 0x00, 0x1A,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x00,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x12,
	0x80, 0x15, 0x00, 0x06,
};

static unsigned char vr_no_ns_receiver[] = {
	0x80, 0x17, 0x00, 0x02,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x1C, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x0C,
	0x80, 0x1B, 0x01, 0x0C,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char vr_no_ns_headset[] = {
	0x80, 0x17, 0x00, 0x02,
	0x80, 0x18, 0x00, 0x03,
	0x80, 0x26, 0x00, 0x15,
	0x80, 0x1C, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x12,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char vr_no_ns_speaker[] = {
	0x80, 0x17, 0x00, 0x02,
	0x80, 0x18, 0x00, 0x02,
	0x80, 0x1C, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x0C,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char vr_no_ns_bt[] = {
	0x80, 0x26, 0x00, 0x06,
	0x80, 0x1C, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x00,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char vr_ns_receiver[] = {
	0x80, 0x17, 0x00, 0x02,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x1C, 0x00, 0x01,
	0x80, 0x17, 0x00, 0x1A,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x04,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x00,
	0x80, 0x18, 0x00, 0x04,
	0x80, 0x1B, 0x00, 0x0C,
	0x80, 0x1B, 0x01, 0x0C,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char vr_ns_headset[] = {
	0x80, 0x17, 0x00, 0x02,
	0x80, 0x18, 0x00, 0x03,
	0x80, 0x26, 0x00, 0x15,
	0x80, 0x1C, 0x00, 0x01,
	0x80, 0x17, 0x00, 0x00,
	0x80, 0x18, 0x00, 0x02,
	0x80, 0x17, 0x00, 0x1A,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x04,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x12,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char vr_ns_speaker[] = {
	0x80, 0x17, 0x00, 0x02,
	0x80, 0x18, 0x00, 0x02,

	0x80, 0x1C, 0x00, 0x01,
	0x80, 0x17, 0x00, 0x00,
	0x80, 0x18, 0x00, 0x04,
	0x80, 0x17, 0x00, 0x04,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x1A,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x0C,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char vr_ns_bt[] = {
	0x80, 0x26, 0x00, 0x06,
	0x80, 0x1C, 0x00, 0x01,
	0x80, 0x17, 0x00, 0x00,
	0x80, 0x18, 0x00, 0x02,
	0x80, 0x17, 0x00, 0x04,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x17, 0x00, 0x1A,
	0x80, 0x18, 0x00, 0x00,
	0x80, 0x1B, 0x00, 0x00,
	0x80, 0x15, 0x00, 0x00,
};

static unsigned char suspend_mode[] = {
	0x80, 0x10, 0x00, 0x01
};

static unsigned char pcm_reset[] = {
	0x80, 0x31, 0x00, 0x00
};

static ssize_t chk_wakeup_a2220(struct a2220_data *a2220)
{
	int rc = 0, retry = 4;

	if (a2220->suspended == 1) {
		gpio_set_value(a2220->pdata->gpio_wakeup, 1);
		mdelay(1);
		gpio_set_value(a2220->pdata->gpio_wakeup, 0);
		msleep(30);
		do {
			rc = execute_cmdmsg(a2220, A100_msg_Sync);
		} while ((rc < 0) && --retry);

		if ((retry == 0) && (rc < 0)) {
			struct a2220img img;
			img.buf = a2220_firmware_buf;
			img.img_size = sizeof(a2220_firmware_buf);
			rc = a2220_hw_reset(a2220, &img);
		}
		if (rc < 0)
			pr_err("Audience HW Reset Failed rc %d\n", rc);
		if (rc < 0) {
			pr_err("%s: failed (%d)\n", __func__, rc);
			goto wakeup_sync_err;
		}
		a2220->suspended = 0;
	}
wakeup_sync_err:
	return rc;
}

static ssize_t a2220_wakeup_no_retry(struct a2220_data *a2220)
{
	int rc = 0;

	if (a2220->suspended == 1) {
		gpio_set_value(a2220->pdata->gpio_wakeup, 1);
		mdelay(1);
		gpio_set_value(a2220->pdata->gpio_wakeup, 0);
		msleep(30);
		rc = execute_cmdmsg(a2220, A100_msg_Sync);
		if (rc < 0) {
			pr_err("%s: failed (%d)\n", __func__, rc);
			goto wakeup_sync_err;
		}
		a2220->suspended = 0;
	}
wakeup_sync_err:
	return rc;
}

/* Filter commands according to noise suppression state forced by
 * A2220_SET_NS_STATE ioctl.
 *
 * For this function to operate properly, all configurations must include
 * both A100_msg_Bypass and Mic_Config commands even if default values
 * are selected or if Mic_Config is useless because VP is off
 */
int a2220_filter_vp_cmd(int cmd, int mode)
{
	int msg = (cmd >> 16) & 0xFFFF;
	int filtered_cmd = cmd;
	static int a2220_param_ID;

	if (a2220_NS_state == A2220_NS_STATE_AUTO)
		return cmd;

	switch (msg) {
	case A100_msg_Bypass:
		if (a2220_NS_state == A2220_NS_STATE_OFF)
			filtered_cmd = A2220_msg_VP_OFF;
		else
			filtered_cmd = A2220_msg_VP_ON;
		break;
	case A100_msg_SetAlgorithmParmID:
		a2220_param_ID = cmd & 0xFFFF;
		break;
	case A100_msg_SetAlgorithmParm:
		if (a2220_param_ID == Mic_Config) {
			if (a2220_NS_state == A2220_NS_STATE_CT)
				filtered_cmd = (msg << 16);
			else if (a2220_NS_state == A2220_NS_STATE_FT)
				filtered_cmd = (msg << 16) | 0x0002;
		}
		break;
	default:
		if (mode == A2220_CONFIG_VP)
			filtered_cmd = -1;
		break;
	}

	pr_info("%s: %x filtered = %x, a2220_NS_state %d, mode %d\n", __func__,
			cmd, filtered_cmd, a2220_NS_state, mode);

	return filtered_cmd;
}

int a2220_set_config(struct a2220_data *a2220, char newid, int mode)
{
	int i = 0, rc = 0, size = 0;
	int retry = 4;
	unsigned int sw_reset = 0;
	unsigned char *i2c_cmds;
	unsigned int msg;
	unsigned char *pMsg;

	pr_info("[AUD] new mode = %d\n", newid);
	if ((a2220->suspended) && (newid == A2220_PATH_SUSPEND))
		return rc;

	if ((a2220_current_config == newid) &&
		(a2220_current_config != A2220_PATH_PCMRESET)) {
		pr_info("already configured this path!!!\n");
		return rc;
	}

	rc = chk_wakeup_a2220(a2220);
	if (rc < 0)
		return rc;

	sw_reset = ((A100_msg_Reset << 16) | RESET_IMMEDIATE);

	switch (newid) {
	case A2220_PATH_INCALL_RECEIVER_NSON:
		i2c_cmds = phonecall_receiver_nson;
		size = sizeof(phonecall_receiver_nson);
		break;
	case A2220_PATH_INCALL_RECEIVER_NSON_WB:
		i2c_cmds = phonecall_receiver_nson_wb;
		size = sizeof(phonecall_receiver_nson_wb);
		break;
	case A2220_PATH_INCALL_RECEIVER_NSOFF:
		i2c_cmds = phonecall_receiver_nsoff;
		size = sizeof(phonecall_receiver_nsoff);
	break;
#ifdef AUDIENCE_BYPASS
	case A2220_PATH_BYPASS_MULTIMEDIA:
		pr_info("%s:A2220_PATH_BYPASS_MULTIMEDIA\n", __func__);
		i2c_cmds = bypass_multimedia;
		size = sizeof(bypass_multimedia);
		break;
#endif
	case A2220_PATH_INCALL_HEADSET:
		i2c_cmds = phonecall_headset;
		size = sizeof(phonecall_headset);
		break;
	case A2220_PATH_INCALL_SPEAKER:
		i2c_cmds = phonecall_speaker;
			size = sizeof(phonecall_speaker);
		break;
	case A2220_PATH_INCALL_BT:
		i2c_cmds = phonecall_bt;
			size = sizeof(phonecall_bt);
		break;
	case A2220_PATH_INCALL_TTY:
		i2c_cmds = phonecall_tty;
		size = sizeof(phonecall_tty);
		break;
	case A2220_PATH_VR_NO_NS_RECEIVER:
		i2c_cmds = vr_no_ns_receiver;
		size = sizeof(vr_no_ns_receiver);
		break;
	case A2220_PATH_VR_NO_NS_HEADSET:
		i2c_cmds = vr_no_ns_headset;
		size = sizeof(vr_no_ns_headset);
		break;
	case A2220_PATH_VR_NO_NS_SPEAKER:
		i2c_cmds = vr_no_ns_speaker;
		size = sizeof(vr_no_ns_speaker);
		break;
	case A2220_PATH_VR_NO_NS_BT:
		i2c_cmds = vr_no_ns_bt;
		size = sizeof(vr_no_ns_bt);
		break;
	case A2220_PATH_VR_NS_RECEIVER:
		i2c_cmds = vr_ns_receiver;
		size = sizeof(vr_ns_receiver);
		break;
	case A2220_PATH_VR_NS_HEADSET:
		i2c_cmds = vr_ns_headset;
		size = sizeof(vr_ns_headset);
		break;
	case A2220_PATH_VR_NS_SPEAKER:
		i2c_cmds = vr_ns_speaker;
		size = sizeof(vr_ns_speaker);
		break;
	case A2220_PATH_VR_NS_BT:
		i2c_cmds = vr_ns_bt;
		size = sizeof(vr_ns_bt);
		break;
	case A2220_PATH_RECORD_RECEIVER:
		i2c_cmds = INT_MIC_recording_receiver;
		size = sizeof(INT_MIC_recording_receiver);
		break;
	case A2220_PATH_RECORD_HEADSET:
		i2c_cmds = EXT_MIC_recording;
		size = sizeof(EXT_MIC_recording);
		break;
	case A2220_PATH_RECORD_SPEAKER:
		i2c_cmds = INT_MIC_recording_speaker;
		size = sizeof(INT_MIC_recording_speaker);
		break;
	case A2220_PATH_RECORD_BT:
		i2c_cmds = phonecall_bt;
		size = sizeof(phonecall_bt);
		break;
	case A2220_PATH_SUSPEND:
		i2c_cmds = (unsigned char *)suspend_mode;
		size = sizeof(suspend_mode);
		break;
	case A2220_PATH_CAMCORDER:
		i2c_cmds = BACK_MIC_recording;
		size = sizeof(BACK_MIC_recording);
		break;
	case A2220_PATH_PCMRESET:
		i2c_cmds = pcm_reset;
		size = sizeof(pcm_reset);
		msleep(30);
		break;
	case A2220_PATH_FT_LOOPBACK:
		i2c_cmds = ft_loopback;
		size = sizeof(ft_loopback);
		break;
	default:
		pr_err("%s: invalid cmd %d\n", __func__, newid);
		rc = -1;
		goto input_err;
		break;
	}

	a2220_current_config = newid;

	pr_info("%s: change to mode %d\n", __func__, newid);
	pr_info("%s: block write start (size = %d)\n", __func__, size);
	for (i = 1; i <= size; i++) {
		pr_info("%x ", *(i2c_cmds + i - 1));
		if (!(i % 4))
			pr_info("\n");
	}

	pMsg = (unsigned char *)&msg;

	for (i = 0 ; i < size ; i += 4) {
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i+1];
		pMsg[1] = i2c_cmds[i+2];
		pMsg[0] = i2c_cmds[i+3];

		retry = RETRY_CNT;

		do {
			rc = execute_cmdmsg(a2220, msg);
		} while ((rc < 0) && --retry);

		if ((retry == 0) && (rc < 0)) {
			struct a2220img img;
			img.buf = a2220_firmware_buf;
			img.img_size = sizeof(a2220_firmware_buf);
			rc = a2220_hw_reset(a2220, &img);
			if (rc < 0) {
				pr_err("Audience HW Reset Failed\n");
				return rc;
			}
		}

	}

input_err:
	return rc;
}

int execute_cmdmsg(struct a2220_data *a2220, unsigned int msg)
{
	int rc = 0;
	int retries, pass = 0;
	unsigned char msgbuf[4];
	unsigned char chkbuf[4];
	unsigned int sw_reset = 0;

	sw_reset = ((A100_msg_Reset << 16) | RESET_IMMEDIATE);

	msgbuf[0] = (msg >> 24) & 0xFF;
	msgbuf[1] = (msg >> 16) & 0xFF;
	msgbuf[2] = (msg >> 8) & 0xFF;
	msgbuf[3] = msg & 0xFF;

	a2220_pr_info("%x %x %x %x\n"
		, msgbuf[0], msgbuf[1], msgbuf[2], msgbuf[3]);
	memcpy(chkbuf, msgbuf, 4);

	rc = a2220_i2c_write(a2220, msgbuf, 4);
	if (rc < 0) {
		a2220_pr_err("error %d\n", rc);
		a2220_i2c_sw_reset(a2220, sw_reset);

		if (msg == A100_msg_Sleep) {
			a2220_pr_info("execute_cmdmsg "
				"...go to suspend first\n");
			a2220->suspended = 1;
			msleep(120);

		}
		return rc;
	}

	a2220_pr_info("execute_cmdmsg + 1\n");
	/* We don't need to get Ack after sending out a suspend command */
	if (msg == A100_msg_Sleep) {
		a2220_pr_info("...go to suspend first\n");
		a2220->suspended = 1;
		msleep(20);
		return rc;
	}
	a2220_pr_info("execute_cmdmsg + 2\n");

	retries = POLLING_RETRY_CNT;
	while (retries--) {
		rc = 0;
		memset(msgbuf, 0, sizeof(msgbuf));
		rc = a2220_i2c_read(a2220, msgbuf, 4);
		if (rc < 0) {
			a2220_pr_err("ack-read error %d (%d retries)\n"
				, rc, retries);
			continue;
		}

		a2220_pr_info("execute_cmdmsg + 3\n");

		if (msgbuf[0] == 0x80  && msgbuf[1] == chkbuf[1]) {
			pass = 1;
			a2220_pr_info("execute_cmdmsg + 4\n");
			a2220_pr_info("got ACK\n");
			break;
		} else if (msgbuf[0] == 0xff && msgbuf[1] == 0xff) {
			a2220_pr_err("illegal cmd %08x\n", msg);
			rc = -EINVAL;
			a2220_pr_info("execute_cmdmsg + 5\n");
		} else if (msgbuf[0] == 0x00 && msgbuf[1] == 0x00) {
			a2220_pr_info("not ready (%d retries)\n", retries);
			a2220_pr_info("execute_cmdmsg + 6\n");
			rc = -EBUSY;
		} else {
			a2220_pr_info("cmd/ack mismatch: (%d retries left)\n"
				, retries);
			a2220_pr_err("msgbuf[0] = %x\n", msgbuf[0]);
			a2220_pr_err("msgbuf[1] = %x\n", msgbuf[1]);
			a2220_pr_err("msgbuf[2] = %x\n", msgbuf[2]);
			a2220_pr_err("msgbuf[3] = %x\n", msgbuf[3]);
			a2220_pr_err("execute_cmdmsg + 7\n");
			rc = -EBUSY;
		}
		msleep(20); /* use polling */
	}

	if (!pass) {
		a2220_pr_err("failed execute cmd %08x (%d)\n", msg, rc);
		a2220_i2c_sw_reset(a2220, sw_reset);
	}

	a2220_pr_info("execute_cmdmsg - finish\n");

	return rc;
}

#if ENABLE_DIAG_IOCTLS
static int a2220_set_mic_state(struct a2220_data *a2220, char miccase)
{
	int rc = 0;
	unsigned int cmd_msg = 0;

	switch (miccase) {
	case 1: /* Mic-1 ON / Mic-2 OFF */
		cmd_msg = 0x80260007;
		break;
	case 2: /* Mic-1 OFF / Mic-2 ON */
		cmd_msg = 0x80260015;
		break;
	case 3: /* both ON */
		cmd_msg = 0x80260001;
		break;
	case 4: /* both OFF */
		cmd_msg = 0x80260006;
		break;
	default:
		pr_info("%s: invalid input %d\n", __func__, miccase);
		rc = -EINVAL;
		break;
	}
	rc = execute_cmdmsg(a2220, cmd_msg);
	return rc;
}

static int exe_cmd_in_file(struct a2220_data *a2220, unsigned char *incmd)
{
	int rc = 0;
	int i = 0;
	unsigned int cmd_msg = 0;
	unsigned char tmp = 0;

	for (i = 0; i < 4; i++) {
		tmp = *(incmd + i);
		cmd_msg |= (unsigned int)tmp;
		if (i != 3)
			cmd_msg = cmd_msg << 8;
	}
	rc = execute_cmdmsg(a2220, cmd_msg);
	if (rc < 0)
		pr_err("%s: cmd %08x error %d\n", __func__, cmd_msg, rc);
	return rc;
}
#endif /* ENABLE_DIAG_IOCTLS */

static int thread_start(void *a2220)
{
	int rc = 0;
	struct a2220img img;
	pr_info("%s\n", __func__);
	img.buf = a2220_firmware_buf;
	img.img_size = sizeof(a2220_firmware_buf);
	rc = a2220_bootup_init(a2220, &img);
	return rc;
}

static long a2220_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct a2220_data *a2220 = container_of(file->private_data,
			struct a2220_data, device);
	static struct task_struct *task;
	int rc = 0;
#if ENABLE_DIAG_IOCTLS
	char msg[4];
	int mic_cases = 0;
	int mic_sel = 0;
#endif
	int ns_state;
#ifdef __A2220_ORGINAL__
	xoclk_control(true);
#endif /* __A2220_ORGINAL__ */
	switch (cmd) {
	case A2220_BOOTUP_INIT:
		task = kthread_run(thread_start, a2220, "thread_start");
		if (IS_ERR(task)) {
			rc = PTR_ERR(task);
			task = NULL;
		}
		break;
	case A2220_SET_CONFIG:
		mutex_lock(&a2220->lock);
		rc = a2220_set_config(a2220, arg, A2220_CONFIG_FULL);
		if (rc < 0)
			goto handle_error;
		mutex_unlock(&a2220->lock);
			break;
	case A2220_SET_NS_STATE:
		mutex_lock(&a2220->lock);
		if (copy_from_user(&ns_state, argp, sizeof(ns_state))) {
			rc = -EFAULT;
			goto handle_error;
		}
		if (ns_state >= A2220_NS_NUM_STATES) {
			rc = -EINVAL;
			goto handle_error;
		}
		a2220_NS_state = ns_state;
		if (!a2220->suspended)
			a2220_set_config(a2220, a2220_current_config,
					A2220_CONFIG_VP);
		mutex_unlock(&a2220->lock);
		break;
	case A2220_SET_VCLK4:
		if (arg)
			rc = a2220_enable_vclk4();
		else
			a2220_disable_vclk4();
		break;
	case A2220_BT_MHL_NORMAL_START:
		rc = a2220_set_state(SNDP_MODE_NORMAL,
			SNDP_OUT_BLUETOOTH_SCO,
			SNDP_EXTDEV_START);
		break;
	case A2220_BT_MHL_RING_START:
		rc = a2220_set_state(SNDP_MODE_RING,
			SNDP_OUT_BLUETOOTH_SCO_HEADSET,
			SNDP_EXTDEV_START);
		break;
	case A2220_BT_MHL_INCOMM_START:
		rc = a2220_set_state(SNDP_MODE_INCOMM,
			SNDP_OUT_BLUETOOTH_SCO_CARKIT,
			SNDP_EXTDEV_START);
		break;
	case A2220_BT_MHL_INCALL_START:
		rc = a2220_set_state(SNDP_MODE_INCALL,
			SNDP_OUT_BLUETOOTH_A2DP_HEADPHONES,
			SNDP_EXTDEV_START);
		break;
	case A2220_BT_MHL_NORMAL_CHANGE:
		rc = a2220_set_state(SNDP_MODE_NORMAL,
			SNDP_OUT_BLUETOOTH_A2DP_SPEAKER,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_BT_MHL_RING_CHANGE:
		rc = a2220_set_state(SNDP_MODE_RING,
			SNDP_OUT_AUX_DIGITAL,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_BT_MHL_INCOMM_CHANGE:
		rc = a2220_set_state(SNDP_MODE_INCOMM,
			SNDP_IN_BLUETOOTH_SCO_HEADSET,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_BT_MHL_INCALL_CHANGE:
		rc = a2220_set_state(SNDP_MODE_INCALL,
			SNDP_IN_AUX_DIGITAL,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_BT_MHL_NORMAL_STOP:
		rc = a2220_set_state(SNDP_MODE_NORMAL,
			SNDP_OUT_BLUETOOTH_SCO,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_BT_MHL_RING_STOP:
		rc = a2220_set_state(SNDP_MODE_RING,
			SNDP_OUT_BLUETOOTH_SCO_HEADSET,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_BT_MHL_INCOMM_STOP:
		rc = a2220_set_state(SNDP_MODE_INCOMM,
			SNDP_OUT_BLUETOOTH_SCO_CARKIT,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_BT_MHL_INCALL_STOP:
		rc = a2220_set_state(SNDP_MODE_INCALL,
			SNDP_OUT_BLUETOOTH_A2DP_HEADPHONES,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_EARPIECE_NORMAL_START:
		rc = a2220_set_state(SNDP_MODE_NORMAL,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_START);
		break;
	case A2220_EARPIECE_RING_START:
		rc = a2220_set_state(SNDP_MODE_RING,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_START);
		break;
	case A2220_EARPIECE_INCOMM_START:
		rc = a2220_set_state(SNDP_MODE_INCOMM,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_START);
		break;
	case A2220_EARPIECE_INCALL_START:
		rc = a2220_set_state(SNDP_MODE_INCALL,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_START);
		break;
	case A2220_EARPIECE_NORMAL_CHANGE:
		rc = a2220_set_state(SNDP_MODE_NORMAL,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_EARPIECE_RING_CHANGE:
		rc = a2220_set_state(SNDP_MODE_RING,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_EARPIECE_INCOMM_CHANGE:
		rc = a2220_set_state(SNDP_MODE_INCOMM,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_EARPIECE_INCALL_CHANGE:
		rc = a2220_set_state(SNDP_MODE_INCALL,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_EARPIECE_NORMAL_STOP:
		rc = a2220_set_state(SNDP_MODE_NORMAL,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_EARPIECE_RING_STOP:
		rc = a2220_set_state(SNDP_MODE_RING,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_EARPIECE_INCOMM_STOP:
		rc = a2220_set_state(SNDP_MODE_INCOMM,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_EARPIECE_INCALL_STOP:
		rc = a2220_set_state(SNDP_MODE_INCALL,
			SNDP_OUT_EARPIECE,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_OTHER_NORMAL_START:
		rc = a2220_set_state(SNDP_MODE_NORMAL,
			SNDP_OUT_SPEAKER,
			SNDP_EXTDEV_START);
		break;
	case A2220_OTHER_RING_START:
		rc = a2220_set_state(SNDP_MODE_RING,
			SNDP_OUT_WIRED_HEADSET,
			SNDP_EXTDEV_START);
		break;
	case A2220_OTHER_INCOMM_START:
		rc = a2220_set_state(SNDP_MODE_INCOMM,
			SNDP_OUT_WIRED_HEADPHONE,
			SNDP_EXTDEV_START);
		break;
	case A2220_OTHER_INCALL_START:
		rc = a2220_set_state(SNDP_MODE_INCALL,
			SNDP_OUT_ANLG_DOCK_HEADSET,
			SNDP_EXTDEV_START);
		break;
	case A2220_OTHER_NORMAL_CHANGE:
		rc = a2220_set_state(SNDP_MODE_NORMAL,
			SNDP_OUT_DGTL_DOCK_HEADSET,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_OTHER_RING_CHANGE:
		rc = a2220_set_state(SNDP_MODE_RING,
			SNDP_OUT_FM_RADIO_TX,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_OTHER_INCOMM_CHANGE:
		rc = a2220_set_state(SNDP_MODE_INCOMM,
			SNDP_OUT_FM_RADIO_RX,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_OTHER_INCALL_CHANGE:
		rc = a2220_set_state(SNDP_MODE_INCALL,
			SNDP_IN_BUILTIN_MIC,
			SNDP_EXTDEV_CH_DEV);
		break;
	case A2220_OTHER_NORMAL_STOP:
		rc = a2220_set_state(SNDP_MODE_NORMAL,
			SNDP_IN_VOICE_CALL,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_OTHER_RING_STOP:
		rc = a2220_set_state(SNDP_MODE_RING,
			SNDP_IN_BACK_MIC,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_OTHER_INCOMM_STOP:
		rc = a2220_set_state(SNDP_MODE_INCOMM,
			SNDP_IN_USB_HEADSET,
			SNDP_EXTDEV_STOP);
		break;
	case A2220_OTHER_INCALL_STOP:
		rc = a2220_set_state(SNDP_MODE_INCALL,
			SNDP_IN_FM_RADIO_RX,
			SNDP_EXTDEV_STOP);
		break;

#if ENABLE_DIAG_IOCTLS
	case A2220_SET_MIC_ONOFF:
		mutex_lock(&a2220->lock);
		rc = chk_wakeup_a2220(a2220);
		if (rc < 0)
			goto handle_error;
			if (copy_from_user(&mic_cases,
					argp, sizeof(mic_cases))) {
				rc = -EFAULT;
			goto handle_error;
		}
		rc = a2220_set_mic_state(a2220, mic_cases);
		if (rc < 0) {
			pr_err("%s: A2220_SET_MIC_ONOFF %d error %d!\n",
					__func__, mic_cases, rc);
			goto handle_error;
		}
		mutex_unlock(&a2220->lock);
		break;
	case A2220_SET_MICSEL_ONOFF:
		mutex_lock(&a2220->lock);
			rc = chk_wakeup_a2220(a2220);
		if (rc < 0)
			goto handle_error;
		if (copy_from_user(&mic_sel, argp, sizeof(mic_sel))) {
			rc = -EFAULT;
			goto handle_error;
		}
		mutex_unlock(&a2220->lock);
	break;
	case A2220_READ_DATA:
		mutex_lock(&a2220->lock);
		rc = chk_wakeup_a2220(a2220);
		if (rc < 0)
			goto handle_error;
		rc = a2220_i2c_read(a2220, msg, 4);
		if (copy_to_user(argp, &msg, 4)) {
			rc = -EFAULT;
			goto handle_error;
		}
		mutex_unlock(&a2220->lock);
		break;
	case A2220_WRITE_MSG:
		mutex_lock(&a2220->lock);
		rc = chk_wakeup_a2220(a2220);
		if (rc < 0)
			goto handle_error;
		if (copy_from_user(msg, argp, sizeof(msg))) {
				rc = -EFAULT;
			goto handle_error;
		}
		rc = a2220_i2c_write(a2220, msg, 4);
		mutex_unlock(&a2220->lock);
		break;
	case A2220_SYNC_CMD:
		mutex_lock(&a2220->lock);
		rc = chk_wakeup_a2220(a2220);
		if (rc < 0)
			goto handle_error;
		msg[0] = 0x80;
		msg[1] = 0x00;
		msg[2] = 0x00;
			msg[3] = 0x00;
		rc = a2220_i2c_write(a2220, msg, 4);
		mutex_unlock(&a2220->lock);
		break;
	case A2220_SET_CMD_FILE:
		mutex_lock(&a2220->lock);
		rc = chk_wakeup_a2220(a2220);
		if (rc < 0)
			goto handle_error;
		if (copy_from_user(msg, argp, sizeof(msg))) {
			rc = -EFAULT;
			goto handle_error;
		}
			rc = exe_cmd_in_file(msg);
		mutex_unlock(&a2220->lock);
		break;
#endif /* ENABLE_DIAG_IOCTLS */
	default:
		pr_err("%s: invalid command %d\n", __func__, _IOC_NR(cmd));
		rc = -EINVAL;
		goto handle_error;
	}
#ifdef __A2220_ORGINAL__
	xoclk_control(false);
#endif /* __A2220_ORGINAL__ */
	return rc;
handle_error:
	mutex_unlock(&a2220->lock);
	pr_err("%s rc[%d]\n", __func__, rc);
	return rc;
}

static const struct file_operations a2220_fops = {
	.owner = THIS_MODULE,
	.open = a2220_open,
	.release = a2220_release,
	.unlocked_ioctl = a2220_ioctl,
};

static int a2220_read_proc(char *page, char **start, off_t offset,
					int count, int *eof, void *data)
{
	int len = 0;

	len = snprintf(page, count, "0x%x\n", g_a2220_log_level);

	return len;
}

static int a2220_write_proc(struct file *filp, const char *buffer,
					unsigned long len, void *data)
{
	int rc = 0;
	unsigned char buf[11] = {0};
	unsigned int code = 0;

	if (11 <= len) {
		/* size over */
		return len;
	}

	if (copy_from_user(buf, (void __user *)buffer, len)) {
		/* failed copy_from_user */
		return len;
	}

	rc = kstrtouint(buf, 0, &code);
	if (0 != rc) {
		/* kstrtouint failed */
		return len;
	}

	if (!(A2220_LOG_LEVEL_CHECK & code))
		g_a2220_log_level = code;

	return len;
}

static int a2220_create_proc_entry(void)
{
	int rc = 0;
	struct proc_dir_entry *log_level = NULL;

	 g_a2220_parent = proc_mkdir("a2220", NULL);
	if (NULL != g_a2220_parent) {
		log_level = create_proc_entry("log_level",
				(S_IFREG | S_IRUGO | S_IWUGO), g_a2220_parent);
		if (NULL != log_level) {
			log_level->read_proc  = a2220_read_proc;
			log_level->write_proc = a2220_write_proc;
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
	remove_proc_entry("a2220", NULL);
	g_a2220_parent = NULL;
	return rc;
}

static void a2220_remove_proc_entry(void)
{
	if (NULL != g_a2220_parent) {
		remove_proc_entry("log_level", g_a2220_parent);
		remove_proc_entry("a2220", NULL);
		g_a2220_parent = NULL;
	}
}

static int a2220_enable_vclk4(void)
{
	int ret = 0;
	struct clk *main_clk = NULL;

	a2220_pr_func_start();

	if (!g_a2220_vclk4_clk) {
		g_a2220_vclk4_clk = clk_get(NULL, "vclk4_clk");

		if (IS_ERR(g_a2220_vclk4_clk)) {
			ret = IS_ERR(g_a2220_vclk4_clk);
			a2220_pr_err("clk_get(vclk4_clk) failed[%d]", ret);
			goto err_clk_get_vclk4;
		}

		main_clk = clk_get(NULL, "main_clk");

		if (IS_ERR(main_clk)) {
			ret = IS_ERR(main_clk);
			a2220_pr_err("clk_get(main_clk) failed[%d]", ret);
			goto err_clk_get_vclk4;
		}

		/* vclk4_clk */
		ret = clk_set_parent(g_a2220_vclk4_clk, main_clk);

		if (0 != ret) {
			a2220_pr_err("clk_set_parent() failed[%d]", ret);
			goto err_enable_vclk4;
		}

		ret = clk_set_rate(g_a2220_vclk4_clk, 13000000);

		if (0 != ret) {
			a2220_pr_err("clk_set_rate() failed[%d]", ret);
			goto err_enable_vclk4;
		}

		ret = clk_enable(g_a2220_vclk4_clk);

		if (0 != ret) {
			a2220_pr_err("clk_enable() failed[%d]", ret);
			goto err_enable_vclk4;
		}

		clk_put(main_clk);

		/* wait until vclk4 is stable */
		usleep_range(1, 1);
	} else {
		a2220_pr_info("already enabled vclk.\n");
	}

	a2220_pr_info("enable vclk complete.\n");
	a2220_pr_debug("g_a2220_vclk_adr = 0x%08x.\n"
		, ioread32(g_a2220_vclk_adr));

	a2220_pr_func_end("ret.[%d]\n", ret);
	return ret;

err_enable_vclk4:
	clk_put(g_a2220_vclk4_clk);
	clk_put(main_clk);
err_clk_get_vclk4:
	g_a2220_vclk4_clk = NULL;
	a2220_pr_err("ret[%d]", ret);
	return ret;
}

static int a2220_disable_vclk4(void)
{
	int ret = 0;

	a2220_pr_func_start();

	if (g_a2220_vclk4_clk) {
		clk_disable(g_a2220_vclk4_clk);

		clk_put(g_a2220_vclk4_clk);

		g_a2220_vclk4_clk = NULL;
	} else {
		a2220_pr_info("already disabled vclk.\n");
	}

	a2220_pr_info("disable vclk complete.\n");
	a2220_pr_debug("g_a2220_vclk_adr = 0x%08x.\n"
		, ioread32(g_a2220_vclk_adr));

	a2220_pr_func_end("ret.[%d]\n", ret);
	return ret;
}

static int a2220_gpio_setup(int gpio)
{
	int ret = 0;

	a2220_pr_func_start("gpio[%d]\n", gpio);

	/* gpio request */
	gpio_request(gpio, NULL);

	if (0 != ret) {
		a2220_pr_err("request(%d) failed[%d]", gpio, ret);
		goto err_gpio_request;
	}

	/* gpio direction output */
	ret = gpio_direction_output(gpio, 1);

	if (0 != ret) {
		a2220_pr_err("direction_output(%d) failed[%d]", gpio, ret);
		goto err_gpio_request;
	}

	a2220_pr_func_end("ret.[%d]\n", ret);
	return ret;

err_gpio_request:
	a2220_pr_err("ret.[%d]", ret);
	return ret;
}

static int a2220_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;
	struct a2220_data *a2220;
	static struct a2220_platform_data *pdata;

	a2220_pr_func_start();

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		a2220_pr_err("platform data is NULL\n");
		goto err_alloc_data_failed;
	}

#ifdef __A2220_ORGINAL__
	if (!pdata->a2220_hw_init) {
		a2220_pr_err("a2220_hw_init is NULL\n");
		goto err_alloc_data_failed;
	}

	rc = pdata->a2220_hw_init();
	if (rc < 0)
		goto err_alloc_data_failed;
#endif /* __A2220_ORGINAL__ */

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		a2220_pr_err("i2c check functionality error\n");
		rc = -ENODEV;
		goto err_alloc_data_failed;
	}

	a2220 = kzalloc(sizeof(*a2220), GFP_KERNEL);
	if (!a2220) {
		a2220_pr_err("failed to allocate memory for module data\n");
		rc = -ENOMEM;
		goto err_alloc_data_failed;
	}

	a2220->pdata = client->dev.platform_data;
	mutex_init(&a2220->lock);

	i2c_set_clientdata(client, a2220);
	a2220->this_client = client;

	a2220->device.minor = MISC_DYNAMIC_MINOR;
	a2220->device.name = "audience_a2220";
	a2220->device.fops = &a2220_fops;

	rc = misc_register(&a2220->device);
	if (rc) {
		a2220_pr_err("a2220_device register failed.\n");
		goto err_mem_alloc_failed;
	}

	g_a2220_vclk_adr = (unsigned int)ioremap(VCLKCR4Phys, 0x4);

	/* enable vclk4 */
	rc = a2220_enable_vclk4();

	if (0 != rc) {
		a2220_pr_err("a2220_enable_vclk4() failed.[%d]\n", rc);
		goto err_misc_register_failed;
	}

	/* gpio request & dirction output */
	rc = a2220_gpio_setup(a2220->pdata->gpio_reset);

	if (0 != rc) {
		a2220_pr_err("a2220_gpio_setup(%d) failed.[%d]\n"
			, a2220->pdata->gpio_reset, rc);
		goto err_clk_enable_failed;
	}

	/* gpio request & dirction output */
	rc = a2220_gpio_setup(a2220->pdata->gpio_wakeup);

	if (0 != rc) {
		a2220_pr_err("a2220_gpio_setup(%d) failed.[%d]\n"
			, a2220->pdata->gpio_reset, rc);
		goto err_reset_gpio_request_failed;
	}

	/* Wakeup signal H -> L */
	gpio_set_value(a2220->pdata->gpio_wakeup, 0);
	msleep(30);

	rc = a2220_create_proc_entry();
	if (0 != rc) {
		a2220_pr_err("create_proc_entry failed.[%d]\n", rc);
		goto err_reset_gpio_request_failed;
	}

	atomic_set(&a2220->opened, 1);
#ifdef __A2220_ORGINAL__
	xo = msm_xo_get(MSM_XO_CXO, "audio_driver");
	if (!xo) {
		pr_err("please check the xo driver,something is wrong!!");
		rc = -EAGAIN;
		goto err_misc_register_failed;
	}
#endif /* __A2220_ORGINAL__ */

	/* copy to static table pointer */
	g_a2220_data = a2220;

	/* set to nb/wb mode */
	g_a2220_nb_wb = VCD_CODEC_WB;
	g_a2220_current_nb_wb = VCD_CODEC_NB;

	/* set callback function */
	g_a2220_callback_func.set_state = a2220_set_state;
	g_a2220_callback_func.set_nb_wb = a2220_set_nb_wb;
	sndp_extdev_regist_callback(&g_a2220_callback_func);

	rc = platform_device_register(&a2220_platform_device);
	if (0 != rc) {
		a2220_pr_err("platform_device_register failed.[%d]\n", rc);
		goto err_reset_gpio_request_failed;
	}

	rc = platform_driver_register(&a2220_platform_driver);
	if (0 != rc) {
		a2220_pr_err("platform_driver_register failed.[%d]\n", rc);
		goto err_pf_device_reg;
	}

	kthread_run(thread_start, a2220, "thread_start");

	a2220_pr_info("- finish\n");
	a2220_pr_func_end("rc.[%d]\n", rc);
	return 0;

err_pf_device_reg:
	platform_device_unregister(&a2220_platform_device);
err_reset_gpio_request_failed:
	gpio_free(a2220->pdata->gpio_reset);
err_clk_enable_failed:
	a2220_disable_vclk4();
err_misc_register_failed:
	misc_deregister(&a2220->device);
err_mem_alloc_failed:
	mutex_destroy(&a2220->lock);
	kfree(a2220);
err_alloc_data_failed:
	pr_err("a2220_probe - failed!!!\n");
	return rc;
}

static int a2220_remove(struct i2c_client *client)
{
	struct a2220_data *a2220 = i2c_get_clientdata(client);

	a2220_pr_func_start();

	platform_driver_unregister(&a2220_platform_driver);
	platform_device_unregister(&a2220_platform_device);
	a2220_remove_proc_entry();
	iounmap((void *)g_a2220_vclk_adr);
	gpio_free(a2220->pdata->gpio_reset);
	gpio_free(a2220->pdata->gpio_wakeup);

	misc_deregister(&a2220->device);
	mutex_destroy(&a2220->lock);
#ifdef __A2220_ORGINAL__
	if (xo) {
		msm_xo_put(xo);
		xo = NULL;
	}
#endif /* __A2220_ORGINAL__ */
	kfree(a2220);
	a2220_pr_func_end();
	return 0;
}

static int a2220_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int a2220_resume(struct i2c_client *client)
{
	return 0;
}

static int a2220_wa_suspend(struct device *dev)
{
	int rc = 0, size = 0;
	unsigned char *i2c_cmds;

	a2220_pr_func_start("dev.[%p]\n", dev);

	mutex_lock(&g_a2220_data->lock);

	if (g_a2220_current_state == A2220_STATE_IDLE) {
		if (g_a2220_suspend_state) {
			/* nop */
			a2220_pr_debug("arleady suspend state.\n");
		} else {
			a2220_pr_debug("suspend.\n");

			/* clock on */
			rc = a2220_enable_vclk4();
			if (0 != rc) {
				/* suspend state */
				g_a2220_suspend_state = 1;
				goto rtn;
			}

			/* wakeup */
			rc = a2220_wakeup_no_retry(g_a2220_data);
			if (rc < 0) {
				/* suspend state */
				g_a2220_suspend_state = 1;
				/* clock off */
				a2220_disable_vclk4();
				goto rtn;
			}
			/*setting disable pass throungh */
			i2c_cmds = pass_through_to_idle;
			size = sizeof(pass_through_to_idle);

			/* suspend state */
			g_a2220_suspend_state = 1;

			/* execute i2c command */
			rc = a2220_i2c_cmd_execute(i2c_cmds, size);

			/* clock off */
			a2220_disable_vclk4();
		}
	}
rtn:
	mutex_unlock(&g_a2220_data->lock);
	a2220_pr_func_end();
	return 0;
}

static int a2220_wa_resume(struct device *dev)
{
	int rc = 0, size = 0;
	unsigned char *i2c_cmds;

	a2220_pr_func_start("dev.[%p]\n", dev);

	mutex_lock(&g_a2220_data->lock);

	if (A2220_STATE_IDLE == g_a2220_current_state) {
		if (g_a2220_suspend_state) {
			if (a2220_bt_mhl_check(g_a2220_current_device)) {
				/* nop */
			} else {
				/* clock on */
				rc = a2220_enable_vclk4();
				if (0 != rc)
					goto rtn;

				/* wakeup */
				rc = a2220_wakeup_no_retry(g_a2220_data);
				if (rc < 0) {
					/* clock off */
					a2220_disable_vclk4();
					goto rtn;
				}

				/*setting enable pass throungh */
				i2c_cmds = idle_to_pass_through;
				size = sizeof(idle_to_pass_through);

				/* not suspend state */
				g_a2220_suspend_state = 0;

				/* execute i2c command */
				rc = a2220_i2c_cmd_execute(i2c_cmds, size);

				/* clock off */
				a2220_disable_vclk4();
			}
		}
	}
rtn:
	mutex_unlock(&g_a2220_data->lock);
	a2220_pr_func_end();
	return 0;
}

static int a2220_wa_runtime_suspend(struct device *dev)
{
	int ret = 0;

	a2220_pr_func_start("dev.[%p]\n", dev);

	mutex_lock(&g_a2220_data->lock);

	if (A2220_STATE_IDLE != g_a2220_current_state)
		ret = -1;

	mutex_unlock(&g_a2220_data->lock);

	a2220_pr_func_end("ret.[%d]\n", ret);
	return ret;
}

static int a2220_wa_runtime_resume(struct device *dev)
{
	a2220_pr_func_start("dev.[%p]\n", dev);
	/* nop */
	a2220_pr_func_end();
	return 0;
}

static const struct i2c_device_id a2220_id[] = {
	{ "audience_a2220", 0 },
	{ }
};

static struct i2c_driver a2220_driver = {
	.probe = a2220_probe,
	.remove = a2220_remove,
	.suspend = a2220_suspend,
	.resume	= a2220_resume,
	.id_table = a2220_id,
	.driver = {
		.name = "audience_a2220",
	},
};

static int a2220_i2c_cmd_execute(char *i2c_cmds, int size)
{
	int i = 0;
	int ret = 0;
	int retry = RETRY_CNT;
	unsigned int msg;
	unsigned char *pMsg;

	a2220_pr_func_start("i2c_cmds[%p], size[%d].\n", i2c_cmds, size);

	for (i = 1; i <= size; i++) {
		a2220_pr_info("%x ", *(i2c_cmds + i - 1));
		if (!(i % 4))
			a2220_pr_info("\n");
	}

	pMsg = (unsigned char *)&msg;

	for (i = 0 ; i < size ; i += 4) {
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i+1];
		pMsg[1] = i2c_cmds[i+2];
		pMsg[0] = i2c_cmds[i+3];

		retry = RETRY_CNT;

		do {
			ret = execute_cmdmsg(g_a2220_data, msg);
		} while ((ret < 0) && --retry);

		if ((retry == 0) && (ret < 0)) {
			struct a2220img img;
			img.buf = a2220_firmware_buf;
			img.img_size = sizeof(a2220_firmware_buf);
			ret = a2220_hw_reset(g_a2220_data, &img);
			if (ret < 0) {
				a2220_pr_err("Audience HW Reset Failed.[%d]\n"
					, ret);
				return ret;
			}
			i = -4;
		}
	}

	a2220_pr_func_end("ret[%d]\n", ret);
	return ret;
}

static int a2220_idle_suspend
	(unsigned int mode, unsigned int device, unsigned int ch_dev)
{
	int ret = 0;
	int size = 0;
	unsigned char *i2c_cmds;

	a2220_pr_func_start("mode[0x%x] device[0x%x] ch_dev[0x%x]\n"
		, mode, device, ch_dev);

	if (a2220_bt_mhl_check(device)) {
		/* nop */
		goto rtn;
	} else if (ch_dev == SNDP_EXTDEV_STOP) {
		/* nop */
		goto rtn;
	} else if (a2220_earpiece_incall_check(mode, device, ch_dev)) {
		/* clock on */
		ret = a2220_enable_vclk4();
		if (0 != ret)
			goto vclk4_err;

		/* wakeup */
		ret = chk_wakeup_a2220(g_a2220_data);
		if (ret < 0)
			goto i2c_err;
		/* check set NB/WB type */
		if (g_a2220_nb_wb == VCD_CODEC_WB) {
			/* enable pass through & voice_process & WB */
			i2c_cmds = idle_to_voice_wb;
			size = sizeof(idle_to_voice_wb);
		} else {
			/* enable pass through & voice_process & NB */
			i2c_cmds = idle_to_voice_nb;
			size = sizeof(idle_to_voice_nb);
		}

		/* not suspend state */
		g_a2220_suspend_state = 0;

		/* state update */
		g_a2220_current_state = A2220_STATE_VOICE_PROCESS;

		/* execute i2c command */
		ret = a2220_i2c_cmd_execute(i2c_cmds, size);
	} else {
		/* clock on */
		ret = a2220_enable_vclk4();
		if (0 != ret)
			goto vclk4_err;

		/* wakeup */
		ret = chk_wakeup_a2220(g_a2220_data);
		if (ret < 0)
			goto i2c_err;

		/* set pass through & sleep */
		i2c_cmds = idle_to_pass_through;
		size = sizeof(idle_to_pass_through);

		/* not suspend state */
		g_a2220_suspend_state = 0;

		/* state update */
		g_a2220_current_state = A2220_STATE_PASS_THROUGH;

		/* execute i2c command */
		ret = a2220_i2c_cmd_execute(i2c_cmds, size);

		/* clock off */
		a2220_disable_vclk4();
	}

rtn:
	a2220_pr_func_end("ret.[%d]\n", ret);
	return ret;

i2c_err:
	a2220_disable_vclk4();
vclk4_err:
	a2220_pr_err("ret.[%d]\n", ret);
	return ret;
}

static int a2220_idle
	(unsigned int mode, unsigned int device, unsigned int ch_dev)
{
	int ret = 0;
	int size = 0;
	unsigned char *i2c_cmds;

	a2220_pr_func_start("mode[0x%x] device[0x%x] ch_dev[0x%x]\n"
		, mode, device, ch_dev);

	if (a2220_bt_mhl_check(device)) {
		/* clock on */
		ret = a2220_enable_vclk4();
		if (0 != ret)
			goto vclk4_err;

		/* wakeup */
		ret = chk_wakeup_a2220(g_a2220_data);
		if (ret < 0)
			goto i2c_err;

		/*setting disable pass throungh */
		i2c_cmds = pass_through_to_idle;
		size = sizeof(pass_through_to_idle);

		/* suspend state */
		g_a2220_suspend_state = 1;

		/* execute i2c command */
		ret = a2220_i2c_cmd_execute(i2c_cmds, size);

		/* clock off */
		a2220_disable_vclk4();

	} else if (ch_dev == SNDP_EXTDEV_STOP) {
		/* nop */
		goto rtn;
	} else if (a2220_earpiece_incall_check(mode, device, ch_dev)) {
		/* clock on */
		ret = a2220_enable_vclk4();
		if (0 != ret)
			goto vclk4_err;

		/* wakeup */
		ret = chk_wakeup_a2220(g_a2220_data);
		if (ret < 0)
			goto i2c_err;

		/* check set NB/WB type */
		if (g_a2220_nb_wb == VCD_CODEC_WB) {
			/* enable pass through & voice_process & WB */
			i2c_cmds = idle_to_voice_wb;
			size = sizeof(idle_to_voice_wb);
		} else {
			/* enable pass through & voice_process & NB */
			i2c_cmds = idle_to_voice_nb;
			size = sizeof(idle_to_voice_nb);
		}
		/* state update */
		g_a2220_current_state = A2220_STATE_VOICE_PROCESS;

		/* execute i2c command */
		ret = a2220_i2c_cmd_execute(i2c_cmds, size);

	} else {
		/* state update */
		g_a2220_current_state = A2220_STATE_PASS_THROUGH;
	}

rtn:
	a2220_pr_func_end("ret.[%d]\n", ret);
	return ret;

i2c_err:
	a2220_disable_vclk4();
vclk4_err:
	a2220_pr_err("ret.[%d]\n", ret);
	return ret;
}

static int a2220_pass_through
	(unsigned int mode, unsigned int device, unsigned int ch_dev)
{
	int ret = 0;
	int size = 0;
	unsigned char *i2c_cmds;

	a2220_pr_func_start("mode[0x%x] device[0x%x] ch_dev[0x%x]\n"
		, mode, device, ch_dev);

	if (a2220_bt_mhl_check(device)) {
		/* clock on */
		ret = a2220_enable_vclk4();
		if (0 != ret)
			goto vclk4_err;

		/* wakeup */
		ret = chk_wakeup_a2220(g_a2220_data);
		if (ret < 0)
			goto i2c_err;

		/*setting disable pass throungh */
		i2c_cmds = pass_through_to_idle;
		size = sizeof(pass_through_to_idle);

		/* suspend state */
		g_a2220_suspend_state = 1;

		/* state update */
		g_a2220_current_state = A2220_STATE_IDLE;

		/* execute i2c command */
		ret = a2220_i2c_cmd_execute(i2c_cmds, size);

		/* clock off */
		a2220_disable_vclk4();
	} else if (ch_dev == SNDP_EXTDEV_STOP) {
		/* state update */
		g_a2220_current_state = A2220_STATE_IDLE;
	} else if (a2220_earpiece_incall_check(mode, device, ch_dev)) {
		/* clock on */
		ret = a2220_enable_vclk4();
		if (0 != ret)
			goto vclk4_err;

		/* wakeup */
		ret = chk_wakeup_a2220(g_a2220_data);
		if (ret < 0)
			goto i2c_err;

		/* check set NB/WB type */
		if (g_a2220_nb_wb == VCD_CODEC_WB) {
			/* enable pass through & voice_process */
			i2c_cmds = pass_through_to_voice_wb;
			size = sizeof(pass_through_to_voice_wb);
		} else {
			/* enable pass through & voice_process */
			i2c_cmds = pass_through_to_voice_nb;
			size = sizeof(pass_through_to_voice_nb);
		}
		/* state update */
		g_a2220_current_state = A2220_STATE_VOICE_PROCESS;

		/* execute i2c command */
		ret = a2220_i2c_cmd_execute(i2c_cmds, size);

	} else {
		/* nop */
		a2220_pr_info("now pass through.\n");
	}

	a2220_pr_func_end("ret[%d]\n", ret);
	return ret;

i2c_err:
	a2220_disable_vclk4();
vclk4_err:
	a2220_pr_err("ret.[%d]\n", ret);
	return ret;
}

static int a2220_voice_process
	(unsigned int mode, unsigned int device, unsigned int ch_dev)
{
	int ret = 0, size = 0;
	unsigned char *i2c_cmds;

	a2220_pr_func_start("mode[0x%x] device[0x%x] ch_dev[0x%x]\n"
		, mode, device, ch_dev);

	/* now executing */
	if (a2220_earpiece_incall_check(mode, device, ch_dev)) {
		/* nop */
		goto rtn;
	}

	if (a2220_bt_mhl_check(device)) {
		/* set command */
		i2c_cmds = voice_to_idle;
		size = sizeof(voice_to_idle);

		/* suspend state */
		g_a2220_suspend_state = 1;

		/* state update */
		g_a2220_current_state = A2220_STATE_IDLE;
	} else if (ch_dev == SNDP_EXTDEV_STOP) {
		/* set command */
		i2c_cmds = voice_to_pass_through;
		size = sizeof(voice_to_pass_through);

		/* state update */
		g_a2220_current_state = A2220_STATE_IDLE;
	} else {
		/* set command */
		i2c_cmds = voice_to_pass_through;
		size = sizeof(voice_to_pass_through);

		/* state update */
		g_a2220_current_state = A2220_STATE_PASS_THROUGH;

	}

	/* execute i2c command */
	ret = a2220_i2c_cmd_execute(i2c_cmds, size);

	/* clock off */
	a2220_disable_vclk4();

	/* init current NB/WB type */
	g_a2220_current_nb_wb = VCD_CODEC_NB;

rtn:
	a2220_pr_func_end("ret[%d]\n", ret);
	return ret;
}

static int a2220_set_state
	(unsigned int mode, unsigned int device, unsigned int ch_dev)
{
	int ret = 0;

	a2220_pr_func_start("mode[0x%x] device[0x%x] ch_dev[0x%x]\n"
		, mode, device, ch_dev);

	if ((g_a2220_current_mode == mode) &&
		(g_a2220_current_device == device) &&
		(g_a2220_start == ch_dev)) {
		a2220_pr_info("already configured this path!!!\n");
		goto rtn;
	}

	mutex_lock(&g_a2220_data->lock);

	if (g_a2220_current_state == A2220_STATE_IDLE) {
		/* IDLE to XXX */
		if (g_a2220_suspend_state)
			ret = a2220_idle_suspend(mode, device, ch_dev);
		else
			ret = a2220_idle(mode, device, ch_dev);
	} else if (g_a2220_current_state == A2220_STATE_PASS_THROUGH) {
		/* PASS THROUGH to XXX */
		ret = a2220_pass_through(mode, device, ch_dev);
	} else {
		/* VOICE PROCESS to XXX */
		ret = a2220_voice_process(mode, device, ch_dev);
	}

	/* update current events */
	g_a2220_current_mode = mode;
	g_a2220_current_device = device;
	g_a2220_start = ch_dev;

	a2220_pr_debug("g_a2220_current_mode[0x%x]\n"
		, g_a2220_current_mode);
	a2220_pr_debug("g_a2220_current_device[0x%x]\n"
		, g_a2220_current_device);
	a2220_pr_debug("g_a2220_start[0x%x]\n"
		, g_a2220_start);
	a2220_pr_debug("g_a2220_current_state[0x%x]\n"
		, g_a2220_current_state);

	mutex_unlock(&g_a2220_data->lock);

rtn:
	a2220_pr_func_end("ret[%d]\n", ret);
	return ret;
}

static int a2220_set_nb_wb(unsigned int nb_wb)
{
	int ret = 0;

	a2220_pr_func_start("nb_wb[0x%x]\n", nb_wb);

	if (g_a2220_current_nb_wb == nb_wb) {
		a2220_pr_info("already configured this path!!!\n");
		goto rtn;
	}

	mutex_lock(&g_a2220_data->lock);

	g_a2220_nb_wb = nb_wb;

	/* Set Algo Sample Rate & Get Change Status */
	if (g_a2220_current_state == A2220_STATE_VOICE_PROCESS)
		ret = a2220_set_algo_sample_rate(nb_wb);

	mutex_unlock(&g_a2220_data->lock);

rtn:
	a2220_pr_func_end("ret[%d]\n", ret);
	return ret;
}

static int a2220_set_algo_sample_rate(unsigned int rate)
{
	int ret = 0, size = 0;
	unsigned char *i2c_cmds;

	a2220_pr_func_start("rate[0x%x]\n", rate);

	/* Set Algo Sample Rate & Get Change Status */
	if (rate == VCD_CODEC_WB) {
		i2c_cmds = set_algo_wb;
		size = sizeof(set_algo_wb);
	} else {
		i2c_cmds = set_algo_nb;
		size = sizeof(set_algo_nb);
	}

	/* update current events */
	g_a2220_current_nb_wb = rate;

	/* execute i2c command */
	ret = a2220_i2c_cmd_execute(i2c_cmds, size);

	a2220_pr_debug("g_a2220_current_nb_wb[0x%x]\n"
		, g_a2220_current_nb_wb);

	a2220_pr_func_end("ret[%d]\n", ret);
	return ret;
}

static bool a2220_bt_mhl_check(unsigned int device)
{
	bool bt_mhl = false;

	a2220_pr_func_start("device[0x%x]\n", device);

	if ((device == SNDP_OUT_BLUETOOTH_SCO) ||
		(device == SNDP_OUT_BLUETOOTH_SCO_HEADSET) ||
		(device == SNDP_OUT_BLUETOOTH_SCO_CARKIT) ||
		(device == SNDP_OUT_BLUETOOTH_A2DP) ||
		(device == SNDP_OUT_BLUETOOTH_A2DP_HEADPHONES) ||
		(device == SNDP_OUT_BLUETOOTH_A2DP_SPEAKER) ||
		(device == SNDP_OUT_AUX_DIGITAL) ||
		(device == SNDP_IN_BLUETOOTH_SCO_HEADSET) ||
		(device == SNDP_IN_AUX_DIGITAL)) {
		bt_mhl = true;
	}

	a2220_pr_func_end("bt_mhl[%d]\n", (unsigned int)bt_mhl);
	return bt_mhl;
}

static bool a2220_earpiece_incall_check
	(unsigned int mode, unsigned int device, unsigned int ch_dev)
{
	bool earpiece_incall = false;

	a2220_pr_func_start("mode[0x%x] device[0x%x] ch_dev[0x%x]\n"
		, mode, device, ch_dev);

	if (((ch_dev == SNDP_EXTDEV_START) ||
		(ch_dev == SNDP_EXTDEV_CH_DEV)) &&
		(device == SNDP_OUT_EARPIECE) &&
		(mode == SNDP_MODE_INCALL)) {
		earpiece_incall = true;
	}

	a2220_pr_func_end("earpiece_incall[%d]\n"
			, (unsigned int)earpiece_incall);
	return earpiece_incall;
}

static int __init a2220_init(void)
{
	g_a2220_log_level = A2220_LOG_ERR;
	return i2c_add_driver(&a2220_driver);
}

static void __exit a2220_exit(void)
{
	i2c_del_driver(&a2220_driver);
}

module_init(a2220_init);
module_exit(a2220_exit);

MODULE_DESCRIPTION("A2220 voice processor driver");
MODULE_LICENSE("GPL");
