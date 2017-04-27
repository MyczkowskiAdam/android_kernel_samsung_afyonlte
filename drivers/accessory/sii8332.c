/***************************************************************************
*
*   Silicon Image SiI8332 MHL Transmitter Driver
*
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation version 2.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*****************************************************************************/

/*========================================================

daniel.lee@siliconimage.com

========================================================*/
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/gpio.h>

#include <asm/irq.h>
#include <linux/delay.h>

#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

#include "sii8332.h"
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/input.h>

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/poll.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/d2153/pmic.h>
#include <linux/regulator/consumer.h>

#define SYSFS_EVENT_FILENAME "evread"
/*#define SFEATURE_HDCP_SUPPORT*/
#define MIN_VERT_RATE 30
#define MAX_VERT_RATE 60
#define VIRT_ACTIVE_PIXELS_1080 1080
#define	SET_BIT(pdata, deviceID, offset, bitnumber)	\
	I2CReadModify(pdata, deviceID, offset, (1<<bitnumber), (1<<bitnumber))
#define	CLR_BIT(pdata, deviceID, offset, bitnumber)	\
	I2CReadModify(pdata, deviceID, offset, (1<<bitnumber), 0x00)
#define NUM_OF_VIDEO_MODE (sizeof(VideoModeInfo)/sizeof(VideoModeInfo[0]))

static int __devinit simg72_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devinit simg7A_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devinit simg92_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devinit simg9A_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int __devinit simgC8_probe(struct i2c_client *client, const struct i2c_device_id *id);

static int __devexit simg72_remove(struct i2c_client *client);
static int __devexit simg7A_remove(struct i2c_client *client);
static int __devexit simg92_remove(struct i2c_client *client);
static int __devexit simg9A_remove(struct i2c_client *client);
static int __devexit simgC8_remove(struct i2c_client *client);

static int device_open;
static struct device *hdmidev;
static struct device *switchdev;
int mhl_plugedin_state;
int outputformat;
struct mutex format_update_lock;

static struct mhl_tx *mhlglobal;
static int count;
static int resumed;

static const struct file_operations hdmi_fops = {
	.owner =    THIS_MODULE,
	.open =     hdmi_open,
	.release =  hdmi_release,

};

static struct miscdevice hdmi_miscdev = {
	MISC_DYNAMIC_MINOR,
	"hdmi",
	&hdmi_fops
};

static struct switch_dev s_dev = {
	.name = "hdmi",
	.state = 0
};

static DEVICE_ATTR(state, S_IRUGO, show_state_hdmi, NULL);
static DEVICE_ATTR(info, S_IRUGO, show_info, NULL);

#define MHL_FUNC_START pr_info("sii8332 %s START ~, %d\n", __func__, __LINE__)
#define PRINT_LOG pr_info("sii8332 %s, %d\n", __func__, __LINE__)
#define PRINT_ERROR \
	printk(KERN_INFO"[ERROR]sii8332: %s():%d failed!\n", __func__, __LINE__)


#define	MHL_MAX_RCP_KEY_CODE	(0x7F + 1)	/* inclusive*/
u8 rcpSupportTable[MHL_MAX_RCP_KEY_CODE] = {
	(MHL_DEV_LD_GUI),		/* 0x00 = Select*/
	(MHL_DEV_LD_GUI),		/* 0x01 = Up*/
	(MHL_DEV_LD_GUI),		/* 0x02 = Down*/
	(MHL_DEV_LD_GUI),		/* 0x03 = Left*/
	(MHL_DEV_LD_GUI),		/* 0x04 = Right*/
	0, 0, 0, 0,				/* 05-08 Reserved*/
	(MHL_DEV_LD_GUI),		/* 0x09 = Root Menu*/
	0, 0, 0,				/* 0A-0C Reserved*/
	(MHL_DEV_LD_GUI),		/* 0x0D = Select*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,/* 0E-1F Reserved*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	/* Numeric keys 0x20-0x29*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	0,						/* 0x2A = Dot*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	/* Enter key = 0x2B*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA |
	MHL_DEV_LD_TUNER),
	/* Clear key = 0x2C*/
	0, 0, 0,	/* 2D-2F Reserved*/
	(MHL_DEV_LD_TUNER),	/* 0x30 = Channel Up*/
	(MHL_DEV_LD_TUNER),	/* 0x31 = Channel Dn*/
	(MHL_DEV_LD_TUNER),	/* 0x32 = Previous Channel*/
	(MHL_DEV_LD_AUDIO),	/* 0x33 = Sound Select*/
	0,	/* 0x34 = Input Select*/
	0,	/* 0x35 = Show Information*/
	0,	/* 0x36 = Help*/
	0,	/* 0x37 = Page Up*/
	0,	/* 0x38 = Page Down*/
	0, 0, 0, 0, 0, 0, 0,	/* 0x39-0x3F Reserved*/
	0,	/* 0x40 = Undefined*/
	(MHL_DEV_LD_SPEAKER),/* 0x41 = Volume Up*/
	(MHL_DEV_LD_SPEAKER),/* 0x42 = Volume Down*/
	(MHL_DEV_LD_SPEAKER),/* 0x43 = Mute*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),/* 0x44 = Play*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO |
	MHL_DEV_LD_RECORD),/*0x45 = Stop*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO |
	MHL_DEV_LD_RECORD),/*0x46 = Pause*/
	(MHL_DEV_LD_RECORD),	/* 0x47 = Record*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),/* 0x48 = Rewind*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),/* 0x49 = Fast Forward*/
	(MHL_DEV_LD_MEDIA),	/* 0x4A = Eject*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO |
	MHL_DEV_LD_MEDIA),/*0x4B Forward*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO |
	MHL_DEV_LD_MEDIA),/*0x4C Backward*/
	0, 0, 0,	/* 4D-4F Reserved*/
	0,	/* 0x50 = Angle*/
	0,	/* 0x51 = Subpicture*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 52-5F Reserved*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	/*0x60 = Play Function*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),
	/*0x61 = Pause the Play Function*/
	(MHL_DEV_LD_RECORD),	/* 0x62 = Record Function*/
	(MHL_DEV_LD_RECORD),	/* 0x63 = Pause the Record Function*/
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),
	/*0x64 = Stop Function*/
	(MHL_DEV_LD_SPEAKER),	/* 0x65 = Mute Function*/
	(MHL_DEV_LD_SPEAKER),	/* 0x66 = Restore Mute Function*/
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x67-0x6F Undefined or reserved*/
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	/* 0x70-0x7F Undefined or reserved*/
};
video_mode_reg regList1920x1080[20] = {
	{PAGE_0, 0x00, 0x00} /**/
	, {PAGE_0, 0x01, 0x1D} /**/
	, {PAGE_3, 0x8F, 0xA1} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x7E} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x02} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x2C} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0xBE} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x0A} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x80} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x07} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0x38} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x04} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x04} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x05} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x65} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x04} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
};

video_mode_reg regList1024x768[20] = {
	{PAGE_0, 0x00, 0x00} /**/
	, {PAGE_0, 0x01, 0x1D} /**/
	, {PAGE_3, 0x8F, 0xA1} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0xA0} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x88} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x80} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x05} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x00} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x04} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0x00} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x03} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x03} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x06} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x26} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x03} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
};

video_mode_reg regList1280x720[20] = {
	{PAGE_0, 0x00, 0x00} /**/
	, {PAGE_0, 0x01, 0x1D} /**/
	, {PAGE_3, 0x8F, 0xA1} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x6E} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x28} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x72} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x06} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x00} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x05} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0xD0} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x02} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x05} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x05} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0xEE} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
};


video_mode_reg regList800x600[20] = {
	{PAGE_0, 0x00, 0x00} /**/
	, {PAGE_0, 0x01, 0x1D} /**/
	, {PAGE_3, 0x8F, 0xA1} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x28} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x80} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x20} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x04} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x20} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x03} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0x58} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x02} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x01} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x04} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x74} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
};

video_mode_reg regList720x576[20] = {
	{PAGE_0, 0x00, 0x8C} /**/
	, {PAGE_0, 0x01, 0x0A} /**/
	, {PAGE_3, 0x8F, 0xAD} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x0C} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x40} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x60} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x03} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0xD0} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x02} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0x40} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x02} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x05} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x05} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x71} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
};

video_mode_reg regList1920x1080i[20] = {
	{PAGE_0, 0x00, 0x00} /**/
	, {PAGE_0, 0x01, 0x1D} /**/
	, {PAGE_3, 0x8F, 0xA1} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x58} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x2C} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x98} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x08} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x80} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x07} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0x1C} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x02} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x02} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x05} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x32} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
};

video_mode_reg regList640x480[20] = {
	{PAGE_0, 0x00, 0xC4} /**/
	, {PAGE_0, 0x01, 0x09} /**/
	, {PAGE_3, 0x8F, 0xAD} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x10} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x60} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x20} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x03} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x80} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x02} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0xE0} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x01} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x0A} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x02} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x0D} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
};

video_mode_reg regList720x480[20] = {
	{PAGE_0, 0x00, 0x8C} /**/
	, {PAGE_0, 0x01, 0x0A} /**/
	, {PAGE_3, 0x8F, 0xAD} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x10} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x3E} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x5A} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x03} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0xD0} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x02} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0xE0} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x01} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x09} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x06} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x0D} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
};


video_mode_info VideoModeInfo[8] = {
	{{1920, 1080, 0x28,  32,   0, tsfDDROver2_2Lanes|tsfDDROver2_3Lanes } ,
	{{PAGE_0, 0x00, 0x00} /**/
	, {PAGE_0, 0x01, 0x1D} /**/
	, {PAGE_3, 0x8F, 0xA1} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x7E} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x02} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x2C} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0xBE} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x0A} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x80} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x07} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0x38} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x04} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x04} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x05} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x65} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x04} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
    }}

	, {{1024, 768, 0x18, 0x00, 0x00, 0},
	{{PAGE_0, 0x00, 0x00} /**/
	, {PAGE_0, 0x01, 0x1D} /**/
	, {PAGE_3, 0x8F, 0xA1} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0xA0} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x88} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x80} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x05} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x00} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x04} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0x00} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x03} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x03} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x06} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x26} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x03} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
    }}


	, {{1280, 720, 0x28, 4, 0x00, tsfDDROver2_2Lanes|tsfDDROver2_3Lanes  },
	{{PAGE_0, 0x00, 0x00} /**/
	, {PAGE_0, 0x01, 0x1D} /**/
	, {PAGE_3, 0x8F, 0xA1} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x6E} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x28} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x72} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x06} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x00} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x05} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0xD0} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x02} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x05} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x05} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0xEE} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
    }}

	, {{ 800, 600, 0x18, 0x00, 0x00, 0},
	{{PAGE_0, 0x00, 0x00} /**/
	, {PAGE_0, 0x01, 0x1D} /**/
	, {PAGE_3, 0x8F, 0xA1} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x28} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x80} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x20} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x04} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x20} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x03} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0x58} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x02} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x01} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x04} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x74} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
    }}

	, {{ 720, 576, 0x18,  17, 0x00, tsfDDROver2_1Lane},
	{{PAGE_0, 0x00, 0x8C} /**/
	, {PAGE_0, 0x01, 0x0A} /**/
	, {PAGE_3, 0x8F, 0xAD} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x0C} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x40} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x60} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x03} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0xD0} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x02} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0x40} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x02} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x05} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x05} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x71} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
    }}


	, {{1920, 540, 0x28, 5, 0x40, tsfDDROver2_2Lanes|tsfDDROver2_3Lanes  },
	{{PAGE_0, 0x00, 0x00} /**/
	, {PAGE_0, 0x01, 0x1D} /**/
	, {PAGE_3, 0x8F, 0xA1} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x58} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x2C} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x98} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x08} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x80} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x07} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0x1C} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x02} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x02} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x05} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x32} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
    }}

	, {{ 640, 480, 0x18, 1, 0x00, tsfDDROver2_1Lane},
	{{PAGE_0, 0x00, 0xC4} /**/
	, {PAGE_0, 0x01, 0x09} /**/
	, {PAGE_3, 0x8F, 0xAD} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x10} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x60} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x20} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x03} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0x80} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x02} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0xE0} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x01} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x0A} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x02} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x0D} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
    }}

	, {{ 720, 480, 0x18, 2, 0x00, tsfDDROver2_1Lane},
	{{PAGE_0, 0x00, 0x8C} /**/
	, {PAGE_0, 0x01, 0x0A} /**/
	, {PAGE_3, 0x8F, 0xAD} /* HS_POL, VS_POL active high*/
	, {PAGE_3, 0xD6, 0x10} /* HS Front Porch Low*/
	, {PAGE_3, 0xD7, 0x00} /* HS Front Porch High*/
	, {PAGE_3, 0xD8, 0x3E} /* HS Pulse Width Low*/
	, {PAGE_3, 0xD9, 0x00} /* HS Pulse Width High*/
	, {PAGE_3, 0xDA, 0x5A} /* H Total Pixels Low*/
	, {PAGE_3, 0xDB, 0x03} /* H Total Pixels High*/
	, {PAGE_3, 0xDC, 0xD0} /* H Active Pixels Low*/
	, {PAGE_3, 0xDD, 0x02} /* H Active Pixels High*/
	, {PAGE_3, 0xDE, 0xE0} /* V Active Lines Low*/
	, {PAGE_3, 0xDF, 0x01} /* V Active Lines High*/
	, {PAGE_3, 0xE0, 0x09} /* VS Front Porch Low*/
	, {PAGE_3, 0xE1, 0x00} /* VS Front Porch High*/
	, {PAGE_3, 0xE2, 0x06} /* VS Pulse Width Low*/
	, {PAGE_3, 0xE3, 0x00} /* VS Pulse Width High*/
	, {PAGE_3, 0xE4, 0x0D} /* V Total Lines Low*/
	, {PAGE_3, 0xE5, 0x02} /* V Total Lines High*/
	, {0xFF, 0xFF, 0x00} /* end of row*/
	} }
};

#ifdef CONFIG_HAS_EARLYSUSPEND
#if 0
static void mhl_early_suspend(struct early_suspend *h);
static void mhl_late_resume(struct early_suspend *h);
#endif
#endif

static struct i2c_device_id SIMG72_id[] = {
	{"SIMG72", 0},
	{}
};

static struct i2c_device_id SIMG7A_id[] = {
	{"SIMG7A", 0},
	{}
};

static struct i2c_device_id SIMG92_id[] = {
	{"SIMG92", 0},
	{}
};

static struct i2c_device_id SIMG9A_id[] = {
	{"SIMG9A", 0},
	{}
};

static struct i2c_device_id SIMGC8_id[] = {
	{"SIMGC8", 0},
	{}
};


#ifdef CONFIG_PM
static const struct dev_pm_ops mhl_pm_ops = {
	.suspend	= mhl_suspend,
	.resume		= mhl_resume,
};
#endif

static struct i2c_driver simg72_driver =
{
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "SIMG72",

	},
	.id_table	= SIMG72_id,
	.probe	= simg72_probe,
	.remove	= __devexit_p(simg72_remove),
	.command = NULL,
};

static struct i2c_driver simg7A_driver =
{
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "SIMG7A",

	},
	.id_table	= SIMG7A_id,
	.probe	= simg7A_probe,
	.remove	= __devexit_p(simg7A_remove),
	.command = NULL,
};

static struct i2c_driver simg92_driver =
{
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "SIMG92",

	},
	.id_table	= SIMG92_id,
	.probe	= simg92_probe,
	.remove	= __devexit_p(simg92_remove),
	.command = NULL,
};

static struct i2c_driver simg9A_driver =
{
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "SIMG9A",

	},
	.id_table	= SIMG9A_id,
	.probe	= simg9A_probe,
	.remove	= __devexit_p(simg9A_remove),
	.command = NULL,
};

static struct i2c_driver simgC8_driver =
{
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "SIMGC8",
		#ifdef CONFIG_PM
		.pm = &mhl_pm_ops,
		#endif
	},
	.id_table	= SIMGC8_id,
	.probe	= simgC8_probe,
	.remove	= __devexit_p(simgC8_remove),
	.command = NULL,
};


#define CONFIG_BCM_HDMI_DET_SWITCH

#ifdef CONFIG_BCM_HDMI_DET_SWITCH
static struct switch_dev ch_hdmi_audio_hpd_switch;
#endif

/****************************************************************************
*	name	=	setmhlstate
*	func	=	setting the MHL plugged in state
*	input	=	int state
*	output	=	None
*	return	=	ret
****************************************************************************/
void setmhlstate(int state)
{
	MHL_FUNC_START;
	/*printk("%s state = %d\n", __func__,state);*/
	switch_set_state(&s_dev, state);

#ifdef CONFIG_BCM_HDMI_DET_SWITCH
	switch_set_state(&ch_hdmi_audio_hpd_switch, (state ? 1 : (int)-1));
#endif

}

/****************************************************************************
*	name	=	hw_resst
*	func	=
*	input	=	struct mhl_platform_data *pdata
*	output	=	None
*	return	=	ret
****************************************************************************/
#if 0
static void hw_reset(struct mhl_platform_data *pdata)
{
	MHL_FUNC_START;
	/*printk("[MHL] %s : START\n", __func__);*/
	gpio_set_value(pdata->mhl_rst, 0);
	/* commented to reduce boot up time*/
	/*msleep(100);*/
	gpio_set_value(pdata->mhl_rst, 1);
}
#endif
/****************************************************************************
*	name	=	get_simgI2C_client
*	func	=
*	input	=	struct mhl_platform_data *pdata, u8 device_id
*	output	=	None
*	return	=	ret
****************************************************************************/
struct i2c_client* get_simgI2C_client(struct mhl_platform_data *pdata, u8 device_id)
{
	struct i2c_client *client_ptr;
	/*printk(KERN_INFO"\n get_simgI2C_client\n");*/

	if (device_id == 0x72)
		client_ptr = pdata->simg72_tx_client;
	else if (device_id == 0x7A)
		client_ptr = pdata->simg7A_tx_client;
	else if (device_id == 0x92)
		client_ptr = pdata->simg92_tx_client;
	else if (device_id == 0x9A)
		client_ptr = pdata->simg9A_tx_client;
	else if (device_id == 0xC8)
		client_ptr = pdata->simgC8_tx_client;
	else
		client_ptr = NULL;
	/*printk(KERN_INFO"\n get_simgI2C_client\n");*/
	return client_ptr;
}

/****************************************************************************
*	name = I2C_ReadByte
*	func =
*	input = struct mhl_platform_data *pdata,u8 deviceID,u8 offset,u8 *data
*	output = None
*	return = ret
****************************************************************************/
static int I2C_ReadByte(struct mhl_platform_data *pdata, u8 deviceID, u8 offset, u8 *data)
{
	int ret=0;
	struct i2c_client *client_ptr = get_simgI2C_client(pdata, deviceID);
	if (!data) {
		/*printk(KERN_INFO"[MHL]I2C_ReadByte error1 %x\n",deviceID);*/
		return -EINVAL;
	}
	if (!client_ptr) {
		/*printk(KERN_INFO"[MHL]I2C_ReadByte error2 %x\n",deviceID);*/
		return -EINVAL;
	}
	ret = i2c_smbus_read_byte_data(client_ptr,offset);
	if (ret < 0) {
		/*printk(KERN_INFO"[MHL]I2C_ReadByte error3 %x\n",deviceID);*/
		return ret;
	}
	*data = (ret & 0x000000FF);
	/*printk(KERN_INFO
	"\n sii8332: %s():(%d) ID:[%02x], offset:[%02x],*data:[%02x],\n",
	__func__,__LINE__, deviceID, offset, *data );*/
	return ret;
}
/**************************************************************************
*	name = I2C_ReadByte_block
*	func =
*	input = struct mhl_platform_data *pdata,u8 deviceID,u8 offset,u8 *data
*	output = None
*	return = ret
**************************************************************************/
static int I2C_ReadByte_block(struct mhl_platform_data *pdata, u8 deviceID, u8 offset, u8 length, u8 *data)
{
	int ret = 0;
	struct i2c_client *client_ptr = get_simgI2C_client(pdata, deviceID);
	if (!client_ptr) {
		/*printk(KERN_INFO"[MHL]I2C_ReadByte error2 %x\n",deviceID);*/
		return -EINVAL;
	}
	ret = i2c_smbus_read_i2c_block_data(client_ptr, offset, length, data);
	return ret;
}
/**************************************************************************
*	name = I2C_WriteByte_block
*	func =
*	input = struct mhl_platform_data *pdata,u8 deviceID,
*			u8 offset,u8 length,u8 *data
*	output = None
*	return = ret
****************************************************************************/
static int I2C_WriteByte_block(struct mhl_platform_data *pdata, u8 deviceID, u8 offset, u8 length, u8 *data)
{
	int ret = 0;
	struct i2c_client *client_ptr = get_simgI2C_client(pdata, deviceID);
	if (!client_ptr) {
		/*printk(KERN_INFO"[MHL]I2C_ReadByte error2 %x\n",deviceID);*/
		return -EINVAL;
	}
	ret = i2c_smbus_write_i2c_block_data(client_ptr, offset, length, data);
	return ret;
}
/****************************************************************************
*	name = I2C_WriteByte
*	func =
*	input = struct mhl_platform_data *pdata,u8 deviceID,u8 offset,u8 value
*	output = None
*	return = ret
****************************************************************************/
static int I2C_WriteByte(struct mhl_platform_data *pdata, u8 deviceID, u8 offset, u8 value)
{
	int ret;
	struct i2c_client *client_ptr = get_simgI2C_client(pdata, deviceID);
	if (client_ptr == NULL) {
		/*printk("I2C_WriteByte (client_ptr == NULL)\n");*/
		return 0;
	}
	/*printk(KERN_INFO
	"\n sii8332: %s():(%d) ID:[%02x], offset:[%02x], value:[%02x],\n",
	__func__,__LINE__, deviceID, offset, value );*/
	ret = i2c_smbus_write_byte_data(client_ptr, offset, value);
	/*printk("I2C_WriteByte : ret= %d\n",ret);*/
	return ret;
}
/****************************************************************************
*	name	=	I2CReadModify
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int I2CReadModify(struct mhl_platform_data *pdata, u8 deviceID, u8 Offset, u8 Mask, u8 Data)
{
	u8 rd;
	int ret;
	ret = I2C_ReadByte(pdata, deviceID, Offset, &rd);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	rd &= ~Mask;
	rd |= (Data & Mask);
	ret = I2C_WriteByte(pdata, deviceID, Offset, rd);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	return ret;
}

/****************************************************************************
*	name	=	ldo2_onoff
*	func	=
*	input	=	bool onoff
*	output	=	None
*	return	=	1 : Success  0: Failure
****************************************************************************/

static int ldo2_onoff(bool onoff)
{
	/* Power On */
	int ret = 0;
	struct regulator *prox_regulator = NULL;

	/*printk(KERN_INFO "[MHL] %s called",__func__);*/

	/* regulator init */
	prox_regulator = regulator_get(NULL, "vdd_mhl");
	if (!prox_regulator) {
		printk(KERN_ERR "[MHL] can not get prox_regulator (VDD_MHL_1.2V)\n");
		return 0;
	} else {
		if (onoff) {
			ret = regulator_set_voltage(prox_regulator,
				D2153_LDO2_VOLT_LOWER, D2153_LDO2_VOLT_UPPER);
			printk(KERN_INFO"[MHL]regulator_set_volt: %d\n", ret);
			ret = regulator_enable(prox_regulator);
			printk(KERN_INFO"[MHL]regulator_enable : %d\n", ret);
		} else {
			ret = regulator_disable(prox_regulator);
			printk(KERN_INFO"[MHL]regulator_disable : %d\n", ret);
		}
		regulator_put(prox_regulator);
		/*After Power Supply is supplied, about 1ms delay is required
		before issuing read/write commands */
		mdelay(10);
	}
	return 1;
}

/****************************************************************************
*	name	=	init_cbus_regs
*	func	=
*	input	=	struct mhl_platform_data *pdata
*	output	=	None
*	return	=	ret
****************************************************************************/
static int init_cbus_regs(struct mhl_platform_data *pdata)
{
	int ret = 0;
	u8 tmp = 0;
	/*Combined some i2c calls to reduce boot time*/
	u8 tmp1[16] = {
		DEVCAP_VAL_DEV_STATE, DEVCAP_VAL_MHL_VERSION,
		DEVCAP_VAL_DEV_CAT, DEVCAP_VAL_ADOPTER_ID_H,
		DEVCAP_VAL_ADOPTER_ID_L, DEVCAP_VAL_VID_LINK_MODE,
		DEVCAP_VAL_AUD_LINK_MODE, DEVCAP_VAL_VIDEO_TYPE,
		DEVCAP_VAL_LOG_DEV_MAP, DEVCAP_VAL_BANDWIDTH,
		DEVCAP_VAL_FEATURE_FLAG, DEVCAP_VAL_DEVICE_ID_H,
		DEVCAP_VAL_DEVICE_ID_L, DEVCAP_VAL_SCRATCHPAD_SIZE,
		DEVCAP_VAL_INT_STAT_SIZE, DEVCAP_VAL_RESERVED
	};

	/*printk(KERN_INFO "\n sii8332: %s():(%d)\n", __func__, __LINE__ );*/
	ret = I2C_WriteByte(pdata, PAGE_CBUS, 0x07, 0xF2);

	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	ret = I2C_WriteByte(pdata, PAGE_CBUS, 0x36, 0x0B);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	ret = I2C_WriteByte(pdata, PAGE_CBUS, 0x39, 0x30);
	if (ret < 0) {
		printk(KERN_INFO "[ERROR]sii8332:%s():%d failed !\n", __func__, __LINE__);
		return ret;
	}
	ret = I2C_WriteByte(pdata, PAGE_CBUS, 0x40, 0x03);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhlglobal->i2c_lock);
	ret = I2C_WriteByte_block(pdata, PAGE_CBUS, 0x80, 16, tmp1);
	mutex_unlock(&mhlglobal->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	ret = I2C_ReadByte(pdata, PAGE_CBUS, 0x31, &tmp);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	tmp = (tmp | 0x0C);
	ret = I2C_WriteByte(pdata, PAGE_CBUS, 0x31, tmp);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	ret = I2C_WriteByte(pdata, PAGE_CBUS, 0x22, 0x0F);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	ret = I2C_WriteByte(pdata, PAGE_CBUS, 0x30, 0x01);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	ret = I2CReadModify(pdata, PAGE_CBUS, 0x2E, BIT4, BIT4);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	/*printk(KERN_INFO "\n sii8332: %s():(%d)\n",
	__func__, __LINE__);*/

	I2C_WriteByte(pdata, PAGE_CBUS, 0x1F, 0x02);
	/*Heartbeat Max Fail Enable*/
	I2C_WriteByte(pdata, PAGE_CBUS, 0x07, DDC_XLTN_TIMEOUT_MAX_VAL | 0x06);
	/*CBUS Drive Strength*/
	I2C_WriteByte(pdata, PAGE_CBUS, 0x42, 0x06);
	/*CBUS DDC interface ignore segment pointer*/
	I2C_WriteByte(pdata, PAGE_CBUS, 0x36, 0x0C);
	/*I2C_WriteByte(0xC8, 0x44, 0x02);*/
	I2C_WriteByte(pdata, PAGE_CBUS, 0x3D, 0xFD);
	I2C_WriteByte(pdata, PAGE_CBUS, 0x1C, 0x00);
	I2C_WriteByte(pdata, PAGE_CBUS, 0x44, 0x00);

	return ret;
}
/****************************************************************************
*	name	=	cbus_reset
*	func	=
*	input	=	struct mhl_platform_data *pdata
*	output	=	None
*	return	=	ret
****************************************************************************/
static int cbus_reset(struct mhl_platform_data *pdata)
{
	int ret;
	/*printk(KERN_INFO "\n sii8332: %s():(%d)\n", __func__,__LINE__ );*/
	/* cbus reset start */
	/*Some I2C calls combined to reduce boot time*/

	u8 tmp1[4] = {0xFF, 0xFF, 0xFF, 0xFF};

	SET_BIT(pdata, PAGE_3, 0x00, 3);
	msleep(2);
	CLR_BIT(pdata, PAGE_3, 0x00, 3);

	/* interrupt #4 */
	ret = I2C_WriteByte(pdata, PAGE_3, 0x22, (BIT_INTR4_RGND_RDY));
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhlglobal->i2c_lock);
	ret = I2C_WriteByte_block(pdata, PAGE_CBUS, 0xE0, 4, tmp1);
	mutex_unlock(&mhlglobal->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhlglobal->i2c_lock);
	ret = I2C_WriteByte_block(pdata, PAGE_CBUS, 0xF0, 4, tmp1);
	mutex_unlock(&mhlglobal->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = I2C_WriteByte(pdata, PAGE_CBUS, 0xF3, 0xFF);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	/*set TPI mode...*/
	ret = I2C_WriteByte(pdata, PAGE_0, 0xC7, 0x00);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	/*Commenting msleep to reduce boot time*/
	/*msleep(100);*/

	ret = I2C_WriteByte(pdata, PAGE_0, 0x3C,
		(BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT
		| BIT_TPI_INTR_ST0_HDCP_VPRIME_VALUE_READY_EVENT
		| BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT
		| BIT_TPI_INTR_ST0_HOT_PLUG_EVENT));
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	/*printk(KERN_INFO "\n sii8332: %s():(%d)\n", __func__,__LINE__ );*/
	/*cbus reset end*/
	return ret;
}
/****************************************************************************
*	name	=	TX_GO2D3
*	func	=
*	input	=	struct mhl_platform_data *pdata
*	output	=	None
*	return	=	ret
****************************************************************************/
static int TX_GO2D3(struct mhl_platform_data *pdata)
{
	int ret = 0;
	u8 int4status;

	MHL_FUNC_START;

	/* mhl_int_count = 0; */

	ret = I2C_WriteByte(pdata, PAGE_3, 0x30, 0xD0);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	msleep(50);

	/*RG ADDITION to clear int4isr before going into D3 mode.*/

	ret = I2C_ReadByte(pdata, PAGE_3, 0x21, &int4status);
	if (ret < 0)
		PRINT_ERROR;

	if (int4status & 0x40) {
		/*if RGND interrupt is set clear it.*/
		/*printk("%s clearing interrupt\n",__func__);*/

		/*clear interrupt status...*/
		ret = I2C_WriteByte(pdata, PAGE_3, 0x21, 0x40);
		/*clear RGND before entering D3 mode.*/
		if (ret < 0)
			PRINT_ERROR;
		count++;
	}
	/*RG END OF ADDITION*/
	ret = CLR_BIT(pdata, PAGE_1, 0x3D, 0x00);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	/*printk(KERN_INFO"\n sii8332: %s():(%d)\n", __func__,__LINE__ );*/

	pdata->status.op_status = MHL_READY_RGND_DETECT;

	return ret;
}
/****************************************************************************
*	name	=	forceUSBIDswitchOpen
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int forceUSBIDswitchOpen(struct mhl_tx *mhl)
{
	int ret = 0;
	/*printk(KERN_INFO"\n sii8332: %s():(%d)\n",
					__func__, __LINE__ );*/
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x13, 0x8C);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		printk(KERN_INFO "[ERROR]sii8332:%s():%d failed!\n",
							__func__, __LINE__);
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2CReadModify(mhl->pdata, PAGE_3, 0x10, BIT0, 0x00);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2CReadModify(mhl->pdata, PAGE_3, 0x15, BIT6, BIT6);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x12, 0x86);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2CReadModify(mhl->pdata, PAGE_3, 0x20, BIT5 | BIT4, BIT4);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	/*printk(KERN_INFO "\n sii8332: %s():(%d)\n", __func__, __LINE__ );*/
	return ret;
}
/****************************************************************************
*	name	=	releaseUSBIDswitchOpen
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int releaseUSBIDswitchOpen(struct mhl_tx *mhl)
{
	int ret = 0;
	msleep(50);
	/*printk(KERN_INFO"\n sii8332: %s():(%d)\n", __func__, __LINE__ );*/
	mutex_lock(&mhl->i2c_lock);
	ret = I2CReadModify(mhl->pdata, PAGE_3, 0x15, BIT6, 0x00);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2CReadModify(mhl->pdata, PAGE_3, 0x10, BIT0, BIT0);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	/*printk(KERN_INFO "\n sii8332: %s():(%d)\n", __func__,__LINE__ );*/
	return ret;
}
/****************************************************************************
*	name	=	mhl_init_func
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int mhl_init_func(struct mhl_tx *mhl)
{
	int ret;

	/* Combined some I2C calls to reduce boot time*/
	u8 tmp1[3] = {0xBC, 0x03, 0x0A};
	u8 tmp2[4] = {0x27, 0xAD, 0x86, 0x8C};
	u8 tmp3[6] = {0xAC, 0x57, 0x11, 0x20, 0x82, 0x25};
	u8 tmp4[3] = {0x1C, 0x26, 0x32};

	/*printk(KERN_INFO "\n sii8332:%s():(%d)\n", __func__, __LINE__ );*/
	#ifdef SFEATURE_HDCP_SUPPORT
		mhl->hdcp_on_ready = false;
		mhl->hdcp_started = false;
		mhl->video_out_setting = false;
	#endif/*SFEATURE_HDCP_SUPPORT*/
	/*FIXME JC*/
	mhl->pdata->status.linkmode = 0x03;
	mhl->pdata->status.op_status = NO_MHL_STATUS;
	mhl->hdmi_sink = false;
	/*mhl->pdata->status.op_status = MHL_READY_RGND_DETECT;*/

	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x8E, 0x03);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	ret = I2C_WriteByte(mhl->pdata, PAGE_1, 0x3D, 0x3F);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x33, 0xC8);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte_block(mhl->pdata, PAGE_3, 0x35, 3, tmp1);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x7A, 0x00);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x0D, 0x02);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x80, 0x08);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0xF8, 0x4C);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = I2CReadModify(mhl->pdata, PAGE_2, 0x05, BIT5, 0x00);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte_block(mhl->pdata, PAGE_3, 0x10, 4, tmp2);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	msleep(50); /* don't remove sleep. for passing MHL CTS*/

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte_block(mhl->pdata, PAGE_3, 0x13, 6, tmp3);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0xCA, 0x10);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	/*for mipi 3 lanes*/
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte_block(mhl->pdata, PAGE_3, 0xA5, 3, tmp4);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x8E, 0x03);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = I2CReadModify(mhl->pdata, PAGE_0, 0x85, BIT0, 0x00);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x00, 0x84);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = cbus_reset(mhl->pdata);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
  }

	/* Commented */
	/*msleep(100);*/

	ret = init_cbus_regs(mhl->pdata);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	/*printk(KERN_INFO "\n sii8332: %s():(%d)\n", __func__,__LINE__ );*/
	return ret;
}

/****************************************************************************
*	name	=	switchToD0
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int switchToD0(struct mhl_tx *mhl)
{
	int ret = 0;
	/*printk(KERN_INFO "\n sii8332: %s():start (%d)\n",
	__func__, __LINE__ );*/
	ret = mhl_init_func(mhl);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2CReadModify(mhl->pdata, PAGE_3, 0x10, BIT1, 0x00);
	mutex_unlock(&mhl->i2c_lock);

	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1E, (BIT1 | BIT0), 0x00);
	mutex_unlock(&mhl->i2c_lock);

	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	return ret;
}

#ifdef SFEATURE_HDCP_SUPPORT
/****************************************************************************
*	name	=	HDCP_ON
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int HDCP_ON(struct mhl_tx *mhl)
{
	int ret = 0;

	/*printk(KERN_INFO "sii8332: %s():%d  HDCP_ON !!\n",
	__func__, __LINE__);*/
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x2A, PROTECTION_LEVEL_MAX);
	mutex_unlock(&mhl->i2c_lock);

	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mhl->hdcp_started = true;

#if 0
	mutex_lock(&mhl->i2c_lock);
	/*mhl->intr5_mask = (BIT_INTR5_PXL_FORMAT_CHG |
	BIT_INTR5_MHL_FIFO_OVERFLOW);*/
	mhl->intr5_mask = (BIT_INTR5_PXL_FORMAT_CHG);
	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x24, mhl->intr5_mask);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
	PRINT_ERROR;
		return ret;
	}
#endif

	return ret;
}
/****************************************************************************
*	name	=	HDCP_OFF
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int HDCP_OFF(struct mhl_tx *mhl)
{
	int ret = 0;

	if (mhl->hdcp_started) {
		/*printk(KERN_INFO"sii8332: %s():%d HDCP_OFF!!\n",
		__func__, __LINE__);*/
		mutex_lock(&mhl->i2c_lock);
		ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
			AV_MUTE_MASK, AV_MUTE_MUTED);
		mutex_unlock(&mhl->i2c_lock);

		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x2A,
			PROTECTION_LEVEL_MIN);
		mutex_unlock(&mhl->i2c_lock);

		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		mhl->hdcp_on_ready = false;
		mhl->hdcp_started = false;
	}
	return ret;
}
#endif/*SFEATURE_HDCP_SUPPORT*/

/****************************************************************************
*	name	=	SiiMhlTxRapkSend
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/

static void SiiMhlTxRapkSend(struct mhl_tx *mhl)
{
	/*printk(KERN_INFO "sii8332: %s():%d  !\n", __func__, __LINE__);*/
	mutex_lock(&mhl->cbus_cmd_lock);
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_MSC_MSG;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset = 0x00;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0x02;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[0] = MHL_MSC_MSG_RAPK;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[1] = 0x00;
	mhl->cmd_rx_cnt += 1;

	if (mhl->cmd_rx_cnt == 1)
		queue_work(mhl->cbus_cmd_wqs, &mhl->cbus_cmd_work);
	mutex_unlock(&mhl->cbus_cmd_lock);
}
/****************************************************************************
*	name	=	SiiMhlTxRcpkSend
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static void SiiMhlTxRcpkSend(struct mhl_tx *mhl, u8 keycode)
{
	/*printk(KERN_INFO "sii8332: %s():%d !\n", __func__, __LINE__);*/
	mutex_lock(&mhl->cbus_cmd_lock);
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_MSC_MSG;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset = 0x00;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0x02;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[0] = MHL_MSC_MSG_RCPK;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[1] = keycode;
	mhl->cmd_rx_cnt += 1;

	if (mhl->cmd_rx_cnt == 1) {
		queue_work(mhl->cbus_cmd_wqs, &mhl->cbus_cmd_work);
	}
	mutex_unlock(&mhl->cbus_cmd_lock);
}
/****************************************************************************
*	name	=	SiiMhlTxRcpeSend
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static void SiiMhlTxRcpeSend(struct mhl_tx *mhl, u8 erro_code, u8 key_code)
{
	/*printk(KERN_INFO "sii8332: %s():%d  !\n", __func__, __LINE__);*/
	mutex_lock(&mhl->cbus_cmd_lock);
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_MSC_MSG;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset = 0x00;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0x03;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[0] = MHL_MSC_MSG_RCPE;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[1] = erro_code;
	mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[2] = key_code;
	mhl->cmd_rx_cnt += 1;

	if (mhl->cmd_rx_cnt == 1)
		queue_work(mhl->cbus_cmd_wqs, &mhl->cbus_cmd_work);
	mutex_unlock(&mhl->cbus_cmd_lock);
}
/****************************************************************************
*	name	=	cbus_cmd_send
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int cbus_cmd_send(struct mhl_tx *mhl, u8 id)
{
	int ret = 0;
	u8 startbit = 0;
	/*printk(KERN_INFO "sii8332: %s():%d  !\n", __func__, __LINE__);*/
	ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x13,
		mhl->msc_cmd_q[id].offset);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x14,
	mhl->msc_cmd_q[id].buff[0]);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	/*printk("sii8332 CMD:%x, Offset:%x, D[0]:%x, D[1]:%x\n",
		mhl->msc_cmd_q[id].command,  mhl->msc_cmd_q[id].offset,
		mhl->msc_cmd_q[id].buff[0], mhl->msc_cmd_q[id].buff[1] );*/

	switch (mhl->msc_cmd_q[id].command) {
	case MHL_SET_INT:
		/*printk("sii8332: %s(): MHL_SET_INT\n", __func__);*/
		startbit = BIT_CBUS_MSC_WRITE_STAT_OR_SET_INT;
		break;

	case MHL_WRITE_STAT:
		/*printk("sii8332: %s(): MHL_WRITE_STAT\n", __func__);*/
		startbit = BIT_CBUS_MSC_WRITE_STAT_OR_SET_INT;
		break;

	case MHL_READ_DEVCAP:
		/*printk("sii8332: %s(): MHL_READ_DEVCAP\n", __func__);*/
		startbit = BIT_CBUS_MSC_READ_DEVCAP;
		break;

	case MHL_GET_STATE:
	case MHL_GET_VENDOR_ID:
	case MHL_SET_HPD:
	case MHL_CLR_HPD:
	case MHL_GET_SC1_ERRORCODE:
	case MHL_GET_DDC_ERRORCODE:
	case MHL_GET_MSC_ERRORCODE:
	case MHL_GET_SC3_ERRORCODE:
		ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x13,
			mhl->msc_cmd_q[id].command);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
		/*printk("Sii8332: %s(): start bit 0x01\n", __func__);*/
		startbit = 0x01;
		break;

	case MHL_MSC_MSG:
		/*printk("Sii8332: %s(): MHL_MSC_MSG\n", __func__);*/
		ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x15,
			mhl->msc_cmd_q[id].buff[1]);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
		ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x13,
			mhl->msc_cmd_q[id].command);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
		startbit = BIT_CBUS_MSC_MSG;
		break;

	case MHL_WRITE_BURST:
		/*will add code later...*/
		startbit = BIT_CBUS_MSC_WRITE_BURST;
		break;

	default:
		return ret;
		break;
	}

	ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x12, startbit);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	return ret;
}

/****************************************************************************
*	name	=	cbus_cmd_done_reaction
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int cbus_cmd_done_reaction(struct mhl_tx *mhl, u8 tx_cnt, u8 i)
{
	int ret = 0;
	u8 cap_data;

	/*printk(KERN_INFO "sii8332: %s():%d\n", __func__, __LINE__);*/

	switch (mhl->msc_cmd_q[(tx_cnt+i)].command)	{
	case MHL_WRITE_STAT:
		break;

	case MHL_READ_DEVCAP:
	switch (mhl->msc_cmd_q[(tx_cnt+i)].offset) {
	case MHL_CAP_MHL_VERSION:
			ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
			if (ret < 0) {
				printk(KERN_INFO"[ERROR]sii8332:%s():%d failed!\n",
						__func__, __LINE__);
				return ret;
			}
			mhl->pdata->rx_cap.mhl_ver = cap_data;
			break;

	case MHL_DEV_CATEGORY_OFFSET:
			ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS,
				0x16, &cap_data);
			if (ret < 0) {
				printk(KERN_INFO"[ERROR]sii8332:%s():%dfailed !\n",
				__func__, __LINE__);
				return ret;
			}

			cap_data = (0x1F & cap_data);

			switch (cap_data) {
			case MHL_DEV_CAT_SOURCE:
				mhl->pdata->rx_cap.dev_type = MHL_DEV_CAT_SOURCE;
				break;

			case MHL_SINK_W_POW:
				mhl->pdata->rx_cap.dev_type = MHL_SINK_W_POW;
				/*mhl tx doesn't need power out*/
				break;

			case MHL_SINK_WO_POW:
				mhl->pdata->rx_cap.dev_type = MHL_SINK_WO_POW;
				break;

			case MHL_DONGLE_W_POW:
				mhl->pdata->rx_cap.dev_type = MHL_DONGLE_W_POW;
				break;

			case MHL_DONGLE_WO_POW:
				mhl->pdata->rx_cap.dev_type = MHL_DONGLE_WO_POW;
				break;

			default:
				mhl->pdata->rx_cap.dev_type = cap_data;
				break;
			}
			break;

	case MHL_CAP_ADOPTER_ID_H:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
							__func__, __LINE__);
			return ret;
		}
		mhl->pdata->rx_cap.adopter_id = cap_data;
		mhl->pdata->rx_cap.adopter_id <<= 8;
		break;

	case MHL_CAP_ADOPTER_ID_L:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n", __func__, __LINE__);
			return ret;
		}
		mhl->pdata->rx_cap.adopter_id |= cap_data;
		break;

	case MHL_CAP_VID_LINK_MODE:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n", __func__, __LINE__);
			return ret;
		}

		mhl->pdata->rx_cap.vid_link_mode = (0x3F & cap_data);
		if (mhl->pdata->rx_cap.vid_link_mode & MHL_DEV_VID_LINK_SUPPRGB444) {
			/*printk(KERN_INFO "sii8332: %s():%d MHL_DEV_VID_LINK_SUPPRGB444\n",
								__func__, __LINE__);*/
		}
		if (mhl->pdata->rx_cap.vid_link_mode & MHL_DEV_VID_LINK_SUPPYCBCR444) {
			/*printk(KERN_INFO"sii8332: %s():%d MHL_DEV_VID_LINK_SUPPYCBCR444\n",
								__func__, __LINE__);*/
		}
		if (mhl->pdata->rx_cap.vid_link_mode & MHL_DEV_VID_LINK_SUPPYCBCR422) {
			/*printk(KERN_INFO "sii8332: %s():%d MHL_DEV_VID_LINK_SUPPYCBCR422\n",
								__func__, __LINE__);*/
		}
		if (mhl->pdata->rx_cap.vid_link_mode &	MHL_DEV_VID_LINK_SUPP_PPIXEL) {
			/*printk(KERN_INFO "sii8332: %s():%d MHL_DEV_VID_LINK_SUPP_PPIXEL\n",
								__func__, __LINE__);*/
		}
		break;

	case MHL_CAP_AUD_LINK_MODE:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
							__func__, __LINE__);
			return ret;
		}
		mhl->pdata->rx_cap.aud_link_mode = (0x03 & cap_data);
		break;

	case MHL_CAP_VIDEO_TYPE:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
						__func__, __LINE__);
			return ret;
		}
		mhl->pdata->rx_cap.video_type = (0x8F & cap_data);
		break;

	case MHL_CAP_LOG_DEV_MAP:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
					__func__, __LINE__);
		return ret;
		}
		mhl->pdata->rx_cap.log_dev_map = cap_data;
		break;

	case MHL_CAP_BANDWIDTH:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
						__func__, __LINE__);
			return ret;
		}
		mhl->pdata->rx_cap.bandwidth = cap_data;
		break;

	case MHL_CAP_FEATURE_FLAG:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
						__func__, __LINE__);
		return ret;
		}
		mhl->pdata->rx_cap.feature_flag = (0x07 & cap_data);
		mhl->pdata->rx_cap.rcp_support =
			(mhl->pdata->rx_cap.feature_flag &
			MHL_FEATURE_RCP_SUPPORT) ? true : false;
		mhl->pdata->rx_cap.rap_support =
			(mhl->pdata->rx_cap.feature_flag &
			MHL_FEATURE_RAP_SUPPORT) ? true : false;
		mhl->pdata->rx_cap.sp_support =
			(mhl->pdata->rx_cap.feature_flag &
			MHL_FEATURE_SP_SUPPORT) ? true : false;
		break;

	case MHL_CAP_DEVICE_ID_H:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
						__func__, __LINE__);
			return ret;
		}
		mhl->pdata->rx_cap.device_id = cap_data;
		mhl->pdata->rx_cap.device_id <<= 8;
		break;

	case MHL_CAP_DEVICE_ID_L:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
					__func__, __LINE__);
		return ret;
		}
		mhl->pdata->rx_cap.device_id |= cap_data;
		break;

	case MHL_CAP_SCRATCHPAD_SIZE:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
					__func__, __LINE__);
			return ret;
		}
		mhl->pdata->rx_cap.scratchpad_size = cap_data;
		break;

	case MHL_CAP_INT_STAT_SIZE:
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x16, &cap_data);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
					__func__, __LINE__);
			return ret;
		}
		mhl->pdata->rx_cap.int_stat_size = cap_data;
		break;

	case MHL_CAP_DEV_STATE:
	case MHL_CAP_RESERVED:
	default:
		break;
	}

	break;

	case MHL_MSC_MSG:
		if (mhl->msc_cmd_q[(tx_cnt+i)].buff[0] == MHL_MSC_MSG_RCPE) {
			mutex_lock(&mhl->cbus_cmd_lock);
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_MSC_MSG;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset = 0x00;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0x02;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[0] = MHL_MSC_MSG_RCPK;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[1] =
				mhl->msc_cmd_q[(tx_cnt+i)].buff[2];
			mhl->cmd_rx_cnt += 1;
			mutex_unlock(&mhl->cbus_cmd_lock);
		}

		break;

	case MHL_WRITE_BURST:
		break;

	case MHL_SET_INT:
		if ((mhl->msc_cmd_q[(tx_cnt+i)].offset == MHL_RCHANGE_INT) &&
			(mhl->msc_cmd_q[(tx_cnt+i)].buff[0] == MHL_INT_DSCR_CHG)) {
			/*printk(KERN_INFO"sii8332:%s():%d !\n", __func__, __LINE__);*/
		} else if ((mhl->msc_cmd_q[(tx_cnt+i)].offset == MHL_RCHANGE_INT)
		&& (mhl->msc_cmd_q[(tx_cnt+i)].buff[0] == MHL_INT_DCAP_CHG)) {
			/*printk(KERN_INFO "sii8332: %s():%d  MHL_STATUS_DCAP_RDY !\n",
			__func__, __LINE__);*/
			mhl->pdata->status.connected_ready |= MHL_STATUS_DCAP_RDY;
			mutex_lock(&mhl->cbus_cmd_lock);
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_WRITE_STAT;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset =
			MHL_STATUS_REG_CONNECTED_RDY;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0x01;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[0] =
			mhl->pdata->status.connected_ready;
			mhl->cmd_rx_cnt += 1;
			mutex_unlock(&mhl->cbus_cmd_lock);
		}
		break;

	default:
		break;
	}
	/*printk(KERN_INFO "sii8332: %s():%d  END\n", __func__, __LINE__);*/
	return ret;
}

/****************************************************************************
*	name	=	cbus_cmd_thread
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static void cbus_cmd_thread(struct work_struct *work)
{
	struct mhl_tx *mhl = container_of(work, struct mhl_tx, cbus_cmd_work);
	u8 tx_cnt = 0;
	u8 rx_cnt = 0;
	u8 i = 0;
	int ret = 0;
	/*bool flag = 0;*/

	/*printk(KERN_INFO "sii8332: %s():%d  START !!\n",
	__func__, __LINE__);*/
	mutex_lock(&mhl->cbus_cmd_lock);
	rx_cnt = mhl->cmd_rx_cnt;
	mutex_unlock(&mhl->cbus_cmd_lock);
	tx_cnt = mhl->cmd_tx_cnt;

	if (tx_cnt == rx_cnt) {
		/*printk(KERN_INFO "sii8332: %s():%d  !\n",
		__func__, __LINE__);*/
		mutex_lock(&mhl->cbus_cmd_lock);
		mhl->cmd_rx_cnt = 0;
		mhl->cmd_tx_cnt = 0;
		mutex_unlock(&mhl->cbus_cmd_lock);
		return;
	}

	do {
		for (i = 0; i < (rx_cnt - tx_cnt); i++) {
			/*printk(KERN_INFO "sii8332: %s():%d  !\n",
			__func__, __LINE__);*/
			mhl->msc_cmd_done_intr = MSC_SEND;

			ret = cbus_cmd_send(mhl, (tx_cnt+i));
			if (ret < 0) {
				PRINT_ERROR;
				return;
			}

			pr_info("sii8332 : wait event +\n");
			wait_event(mhl->cbus_hanler_wq,
			((mhl->msc_cmd_done_intr != MSC_SEND) ||
			(mhl->msc_cmd_abord != false) ||
			(mhl->pdata->status.op_status != MHL_DISCOVERY_SUCCESS)));

			pr_info("sii8332 : wait event -\n");
			/*printk(KERN_INFO "sii8332: %s():%d  !\n",
			__func__, __LINE__);*/
			/*printk("(msc_cmd_done_intr:%d) msc_cmd_abord:%d\n",*/
			/*mhl->msc_cmd_done_intr, mhl->msc_cmd_abord);*/

			if (mhl->pdata->status.op_status != MHL_DISCOVERY_SUCCESS) {
				mutex_lock(&mhl->cbus_cmd_lock);
				mhl->cmd_rx_cnt = 0;
				mhl->cmd_tx_cnt = 0;
				mutex_unlock(&mhl->cbus_cmd_lock);
				/*printk("(line:%d)Not MHL_DISCOVERY_SUCCESS\n", __LINE__);*/
				return;
			}
			if (mhl->msc_cmd_done_intr == MSC_DONE_ACK) {
				mhl->cmd_tx_cnt += 1;
				ret = cbus_cmd_done_reaction(mhl, tx_cnt, i);
				if (ret < 0) {
					PRINT_ERROR;
					return;
				}

			} else if (mhl->msc_cmd_abord == true) {
				mhl->msc_cmd_abord = false;
				/*printk("msleep 2S\n");*/
				msleep(2000);
			} else {
				mutex_lock(&mhl->cbus_cmd_lock);
				mhl->cmd_rx_cnt = 0;
				mhl->cmd_tx_cnt = 0;
				mutex_unlock(&mhl->cbus_cmd_lock);
				return;
			}
		}

		mutex_lock(&mhl->cbus_cmd_lock);
		rx_cnt = mhl->cmd_rx_cnt;
		mutex_unlock(&mhl->cbus_cmd_lock);
		tx_cnt = mhl->cmd_tx_cnt;
		if (tx_cnt == rx_cnt) {
			mutex_lock(&mhl->cbus_cmd_lock);
			mhl->cmd_rx_cnt = 0;
			mhl->cmd_tx_cnt = 0;
			mutex_unlock(&mhl->cbus_cmd_lock);
			return;
		}

	} while (tx_cnt != rx_cnt);
}
/****************************************************************************
*	name	=	mhl_rgnd_check_func
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int mhl_rgnd_check_func(struct mhl_tx *mhl)
{
	u8 int4status, rgnd_value;
	int ret = 0;

	/*MHL_FUNC_START;*/

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x21, &int4status);
	mutex_unlock(&mhl->i2c_lock);

	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	if (int4status == 0x00)
		return ret;

	mutex_lock(&mhl->i2c_lock);
	/*clear interrupt status...*/
	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x21, int4status);
	mutex_unlock(&mhl->i2c_lock);

	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	if (int4status & BIT_INTR4_RGND_RDY) {

		pr_info("sii8332 : BIT_INTR4_RGND_RDY\n");
		ret = switchToD0(mhl);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		mutex_lock(&mhl->i2c_lock);
		ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x1C, &rgnd_value);
		mutex_unlock(&mhl->i2c_lock);

		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		rgnd_value = (rgnd_value & 0x03);
		if (rgnd_value == 0x02) {
			/*printk(KERN_INFO "sii8332: %s():%d MHL device\n",
			__func__, __LINE__);*/
			/*mhl device...*/
			mutex_lock(&mhl->pdata->mhl_status_lock);
			mhl->pdata->status.op_status = MHL_RX_CONNECTED;
			mutex_unlock(&mhl->pdata->mhl_status_lock);

			mutex_lock(&mhl->i2c_lock);
			ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x22,
			(BIT_INTR4_MHL_EST | BIT_INTR4_USB_EST | BIT_INTR4_CBUS_LKOUT));
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
		} else  {
			/*printk(KERN_INFO "sii8332: %s():%d non MHL device:%02x\n",
			__func__, __LINE__, rgnd_value);*/
			mutex_lock(&mhl->pdata->mhl_status_lock);
			mhl->pdata->status.op_status = MHL_USB_CONNECTED;
			mutex_unlock(&mhl->pdata->mhl_status_lock);
			/* usb device... */
			mutex_lock(&mhl->i2c_lock);
			ret = I2CReadModify(mhl->pdata, PAGE_0, 0x18, BIT3, BIT3);
			mutex_unlock(&mhl->i2c_lock);

			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
		}
	}

	return ret;
}

/****************************************************************************
*	name	=	mhl_rx_connected_func
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/

static int mhl_rx_connected_func(struct mhl_tx *mhl)
{
	int ret = 0;
	u8 int4status;
	/*u8 cbus_status;*/

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x21, &int4status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
		__func__, __LINE__);
		return ret;
	}
	if (int4status == 0x00)
		return ret;
	mutex_lock(&mhl->i2c_lock);
	/*clear interrupt status...*/
	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x21, int4status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
		__func__, __LINE__);
		return ret;
	}
	if (int4status & BIT_INTR4_MHL_EST) {
		mutex_lock(&mhl->pdata->mhl_status_lock);
		mhl->pdata->status.op_status = MHL_DISCOVERY_SUCCESS;
		mutex_unlock(&mhl->pdata->mhl_status_lock);

		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x30, 0x10);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x07, 0xF2);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		mutex_lock(&mhl->i2c_lock);
		ret = I2CReadModify(mhl->pdata, PAGE_3, 0x10, BIT0, BIT0);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x22, (BIT_INTR4_CBUS_LKOUT |
			BIT_INTR4_CBUS_DISCONNECT | BIT_INTR4_SCDT_CHANGE));
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		mutex_lock(&mhl->i2c_lock);
		/* CBUS interrupt #1*/
		ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x09,
		(BIT2 | BIT3 | BIT4 | BIT5 | BIT6));
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		mutex_lock(&mhl->i2c_lock);
		/* CBUS interrupt #2*/
		ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x1F, (BIT2 | BIT3));
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		#if 0
		ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x3C,
		(BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT
		| BIT_TPI_INTR_ST0_HDCP_VPRIME_VALUE_READY_EVENT
		| BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT
		| BIT_TPI_INTR_ST0_HOT_PLUG_EVENT));
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
		#endif

		mutex_lock(&mhl->cbus_cmd_lock);
		mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_SET_INT;
		mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset = MHL_RCHANGE_INT;
		mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0x01;
		mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[0] = MHL_INT_DCAP_CHG;
		mhl->cmd_rx_cnt += 1;
		if (mhl->cmd_rx_cnt == 1)
			queue_work(mhl->cbus_cmd_wqs, &mhl->cbus_cmd_work);
		mutex_unlock(&mhl->cbus_cmd_lock);
	}

	if (int4status & BIT_INTR4_USB_EST) {
		mutex_lock(&mhl->pdata->mhl_status_lock);
		mhl->pdata->status.op_status = MHL_DISCOVERY_FAIL;
		mutex_unlock(&mhl->pdata->mhl_status_lock);

#ifdef SFEATURE_HDCP_SUPPORT
		mhl->video_out_setting = false;
		/*printk(KERN_INFO "sii8332: %s():%d  HDCP_OFF !!\n",
		__func__, __LINE__);*/
		mutex_lock(&mhl->i2c_lock);
		ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
		AV_MUTE_MASK, AV_MUTE_MUTED);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x2A, PROTECTION_LEVEL_MIN);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
	#endif /*SFEATURE_HDCP_SUPPORT*/
	return ret;
	}

	if (int4status & BIT4) {

		ret = forceUSBIDswitchOpen(mhl);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		ret = releaseUSBIDswitchOpen(mhl);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
	}

	return ret;
}
/****************************************************************************
*	name	=	cbusprocsess_error
*	func	=
*	input	=	struct mhl_platform_data *pdata, u8 status, bool *abort
*	output	=	None
*	return	=	ret
****************************************************************************/
static int cbusprocsess_error(struct mhl_platform_data *pdata,
										u8 status, bool *abort)
{
	int ret = 0;
	u8 ddcabortreason = 0;
	u8 msc_abort_intstatus = 0;
	status &= (BIT_MSC_ABORT | BIT_MSC_XFR_ABORT | BIT_DDC_ABORT);
	/*printk(KERN_INFO "Sii8332: %s():%d\n", __func__, __LINE__);*/
	if (status) {
		*abort = true;

		if (status & BIT_DDC_ABORT) {
			/*printk(KERN_INFO "Sii8332: %s():%d BIT_CBUS_DDC_ABRT\n",
			__func__, __LINE__);*/
			ret = I2C_ReadByte(pdata, PAGE_CBUS,
			0x0B, &ddcabortreason);
			if (ret < 0) {
				printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
				__func__, __LINE__);
				return ret;
			}
		}
		/*if ( status & BIT_MSC_ABORT) {
			printk(KERN_INFO "Sii8332: %s():%d BIT_CBUS_MSC_MR_ABRT\n",
			__func__, __LINE__);
			ret = I2C_ReadByte(pdata, PAGE_CBUS, 0x0E, 0xFF);
		}*/

		if (status & BIT_MSC_XFR_ABORT) {
			/*printk(KERN_INFO "Sii8332: %s():%d BIT_MSC_XFR_ABORT\n",
			__func__, __LINE__);*/
			ret = I2C_ReadByte(pdata, PAGE_CBUS, 0x0D, &msc_abort_intstatus);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			ret = I2C_WriteByte(pdata, PAGE_CBUS, 0x0D, msc_abort_intstatus);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
			#if 0
			if (msc_abort_intstatus) {
				/*printk(KERN_INFO
				"Sii8332:%s():%d CBUS::MSC Transfer ABORTED.Clearing 0x0D\n",
				__func__, __LINE__);*/
				if (CBUSABORT_BIT_REQ_MAXFAIL & msc_abort_intstatus)
					/*printk(KERN_INFO "Sii8332: %s():%d
					CBUSABORT_BIT_REQ_MAXFAIL\n", __func__, __LINE__);*/
				if (CBUSABORT_BIT_PROTOCOL_ERROR & msc_abort_intstatus)
					/*printk(KERN_INFO "Sii8332: %s():%d
					CBUSABORT_BIT_PROTOCOL_ERROR\n", __func__, __LINE__);*/
				if (CBUSABORT_BIT_REQ_TIMEOUT & msc_abort_intstatus)
					/*printk(KERN_INFO "Sii8332: %s():%d
					CBUSABORT_BIT_REQ_TIMEOUT\n", __func__, __LINE__);*/
				if (CBUSABORT_BIT_PEER_ABORTED & msc_abort_intstatus)
					/*printk(KERN_INFO "Sii8332: %s():%d
					CBUSABORT_BIT_PEER_ABORTED\n", __func__, __LINE__);*/
				if (CBUSABORT_BIT_UNDEFINED_OPCODE & msc_abort_intstatus)
					/*printk(KERN_INFO "Sii8332: %s():%d
					CBUSABORT_BIT_UNDEFINED_OPCODE\n", __func__, __LINE__);*/
			}
			#endif
		}
	}
	return ret;
}

#ifdef CONFIG_SII9234_RCP
static void rcp_uevent_report(struct mhl_tx *mhl, u8 key)
{
	if (!mhl->input_dev) {
		pr_err("%s: sii9234->input_dev is NULL & "
		       "skip rcp_report\n", __func__);
		return;
	}

	pr_info("rcp_uevent_report key: %d\n", key);
	input_report_key(mhl->input_dev, (unsigned int)key + 1, 1);
	input_report_key(mhl->input_dev, (unsigned int)key + 1, 0);
	input_sync(mhl->input_dev);
}

/*
 * is_rcp_code_valid: Validdates the recevied RCP key,
 * valid key is 1 to 1  map to fwk keylayer file sii9234_rcp.kl
 * located at (/system/usr/keylayout/sii9234_rcp.kl).
 *
 * New key support needs to be update is_rcp_key_code_valid at
 * driver side and /system/usr/keylayout/sii9234_rcp.kl at fwk side.
 */

static int is_rcp_key_code_valid(u8 key)
{
	switch (key + 1) {
		/*should resemble /system/usr/keylayout/sii9234_rcp.kl */
	case 1:		/* ENTER                WAKE_DROPPED */
	case 2:		/* DPAD_UP              WAKE_DROPPED */
	case 3:		/* DPAD_DOWN            WAKE_DROPPED */
	case 4:		/* DPAD_LEFT            WAKE_DROPPED */
	case 5:		/* DPAD_RIGHT           WAKE_DROPPED */
	case 10:		/* MENU                 WAKE_DROPPED */
	case 14:		/* BACK                 WAKE_DROPPED */
	case 33:		/* 0    */
	case 34:		/* 1    */
	case 35:		/* 2    */
	case 36:		/* 3    */
	case 37:		/* 4    */
	case 38:		/* 5    */
	case 39:		/* 6    */
	case 40:		/* 7    */
	case 41:		/* 8    */
	case 42:		/* 9    */
	case 43:		/* ENTER */
	case 45:		/* DEL  */
	case 69:		/* MEDIA_PLAY_PAUSE         WAKE */
	case 70:		/* MEDIA_STOP               WAKE */
	case 71:		/* MEDIA_PAUSE              WAKE */
	case 73:		/* MEDIA_REWIND             WAKE */
	case 74:		/* MEDIA_FAST_FORWARD       WAKE */
	case 76:		/* MEDIA_NEXT               WAKE */
	case 77:		/* MEDIA_PREVIOUS           WAKE */
		return 1;
	default:
		return 0;
	}

}

static void cbus_process_rcp_key(struct mhl_tx *mhl, u8 key)
{

	pr_info("cbus_process_rcp_key : key %d\n", key);

	if (is_rcp_key_code_valid(key)) {
		/* Report the key */
		rcp_uevent_report(mhl, key);
		/* Send the RCP ack */
		SiiMhlTxRcpkSend(mhl, key);

	} else {
		mhl->error_key = key;
		/*
		 * Send a RCPE(RCP Error Message) to Peer followed by
		 * RCPK with old key-code so that initiator(TV) can
		 * recognize failed key code.error code = 0x01 means
		 * Ineffective key code was received.
		 * See Table 21.(PRM)for details.
		 */
		SiiMhlTxRcpeSend(mhl, RCPE_INEEFECTIVE_KEY_CODE, key);
	}
}
#endif


/****************************************************************************
*	name	=	mhl_cbus_intr
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int mhl_cbus_intr(struct mhl_tx *mhl)
{
	int ret = 0;
	u8 intr_status = 0;
	u8 subcmd, msg_data;
	u8 intr[4];
	u8 tmp1[4];

	MHL_FUNC_START;

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x08, &intr_status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	if (intr_status) {
		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x08, intr_status);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
		if (intr_status & BIT3) {
			mutex_lock(&mhl->i2c_lock);
			ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x18, &subcmd);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			mutex_lock(&mhl->i2c_lock);
			ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x19, &msg_data);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
			/*printk(KERN_INFO"sii8332:%s():%d subcmd!\n", __func__,subcmd);*/
			switch (subcmd) {

			case MHL_MSC_MSG_RAP:
				if (MHL_RAP_CONTENT_ON == msg_data) {
					mutex_lock(&mhl->i2c_lock);
					ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
						TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK |
						OUTPUT_MODE_MASK, TMDS_OUTPUT_CONTROL_ACTIVE |
						AV_MUTE_NORMAL|OUTPUT_MODE_HDMI);
						mutex_unlock(&mhl->i2c_lock);
					if (ret < 0) {
						PRINT_ERROR;
						return ret;
					}
				} else if (MHL_RAP_CONTENT_OFF == msg_data) {
					mutex_lock(&mhl->i2c_lock);
					ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
						TMDS_OUTPUT_CONTROL_MASK | OUTPUT_MODE_MASK,
						TMDS_OUTPUT_CONTROL_POWER_DOWN | OUTPUT_MODE_DVI);
						mutex_unlock(&mhl->i2c_lock);
					if (ret < 0) {
						PRINT_ERROR;
						return ret;
					}
				}

				SiiMhlTxRapkSend(mhl);
				break;

		case	MHL_MSC_MSG_RCP:
			cbus_process_rcp_key(mhl, msg_data);
#if 0
			if (MHL_LOGICAL_DEVICE_MAP & rcpSupportTable[msg_data & 0x7F]) {
				SiiMhlTxRcpkSend(mhl, msg_data);
			}
			else{
				SiiMhlTxRcpeSend(mhl, RCPE_INEEFECTIVE_KEY_CODE, msg_data );
			}
			break;
#endif

		case	MHL_MSC_MSG_RCPK:
			break;

		case	MHL_MSC_MSG_RCPE:
			break;

		case	MHL_MSC_MSG_RAPK:
			break;

		default:
			/* Any freak value here would continue with no event to app*/
			break;
		}

    }

		if (intr_status & (BIT_MSC_ABORT | BIT_MSC_XFR_ABORT
								| BIT_DDC_ABORT)) {
			mutex_lock(&mhl->i2c_lock);
			ret = cbusprocsess_error(mhl->pdata, intr_status,
					&mhl->msc_cmd_abord);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
PRINT_ERROR;
				return ret;
			}

			if (mhl->msc_cmd_abord == true) {
				/*printk(KERN_INFO "sii8332: %s():%d msc_cmd_abord !\n",
				__func__, __LINE__);*/
				if (waitqueue_active(&mhl->cbus_hanler_wq)) {
					/*printk(KERN_INFO "sii8332: %s():%d wake_up\n",
					__func__, __LINE__);*/
					wake_up(&mhl->cbus_hanler_wq);
				}
			}

		}

		if (intr_status & BIT4) {
			mhl->msc_cmd_done_intr = MSC_DONE_ACK;
			if (waitqueue_active(&mhl->cbus_hanler_wq)) {
				/*printk(KERN_INFO "sii8332: %s():%d wake_up\n",
				__func__, __LINE__);*/
				wake_up(&mhl->cbus_hanler_wq);
			}
		}

		if (intr_status & BIT7) {
			mutex_lock(&mhl->i2c_lock);
			ret = I2CReadModify(mhl->pdata, PAGE_CBUS, 0x38, 0x0F, 0x00);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
		}
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x1E, &intr_status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	if (intr_status) {
		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_CBUS, 0x1E, intr_status);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
		if (intr_status & BIT0)
			/*printk(KERN_INFO "sii8332: %s():%d write burst done !\n",
			__func__, __LINE__);*/

		if (intr_status & BIT2) {

			memset(intr, 0x00, 4);

			mutex_lock(&mhl->i2c_lock);
			ret = I2C_ReadByte_block(mhl->pdata, PAGE_CBUS, 0xA0, 4, intr);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			mutex_lock(&mhl->i2c_lock);
			ret = I2C_WriteByte_block(mhl->pdata, PAGE_CBUS, 0xA0, 4, intr);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
			if (intr[0]) {
				if (MHL_INT_DCAP_CHG & intr[0]) {
					/*printk(KERN_INFO"sii8332:%s():%d MHL_INT_DCAP_CHG !\n",
					__func__, __LINE__);*/
					mutex_lock(&mhl->cbus_cmd_lock);
					mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_READ_DEVCAP;
					mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset =
							MHL_DEV_CATEGORY_OFFSET;
					mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0;
					mhl->cmd_rx_cnt += 1;

					if (mhl->cmd_rx_cnt == 1)
						queue_work(mhl->cbus_cmd_wqs, &mhl->cbus_cmd_work);
					mutex_unlock(&mhl->cbus_cmd_lock);
				}


			}

			if (MHL_INT_DSCR_CHG & intr[0]) {
					/*printk(KERN_INFO "sii8332:%s():%d MHL_INT_DSCR_CHG!\n",
					__func__, __LINE__);*/
			}

				if (MHL_INT_REQ_WRT & intr[0]) {
					/*printk(KERN_INFO "sii8332:%s():%d MHL_INT_REQ_WRT!\n",
					__func__, __LINE__);*/
					mutex_lock(&mhl->cbus_cmd_lock);
					mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_SET_INT;
					mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset = MHL_RCHANGE_INT;
					mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0x01;
					mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[0] = MHL_INT_GRT_WRT;
					mhl->cmd_rx_cnt += 1;
					if (mhl->cmd_rx_cnt == 1)
						queue_work(mhl->cbus_cmd_wqs, &mhl->cbus_cmd_work);
					mutex_unlock(&mhl->cbus_cmd_lock);
				}

				if (MHL_INT_GRT_WRT & intr[0]) {
					/*printk(KERN_INFO "sii8332: %s():%d MHL_INT_GRT_WRT!\n",
					__func__, __LINE__);*/
				}
			}

			if (intr[1] & MHL_INT_EDID_CHG) {
				/*printk(KERN_INFO "sii8332: %s():%d MHL_INT_EDID_CHG!\n",
				__func__, __LINE__);*/
				mhl->avi_work = true;
				mhl->avi_cmd = HPD_HIGH_EVENT;
				if (waitqueue_active(&mhl->avi_control_wq)) {
					/*printk(KERN_INFO "sii8332: %s():%d wake_up\n",
					__func__, __LINE__);*/
					wake_up(&mhl->avi_control_wq);
				} else
					queue_work(mhl->avi_control_wqs, &mhl->avi_control_work);
			}
		}

		if (intr_status & BIT3) {

			u8 tmp2[4] = {0xFF, 0xFF, 0xFF, 0xFF};
			memset(tmp1, 0x00, 4);

			mutex_lock(&mhl->i2c_lock);
			ret = I2C_ReadByte_block(mhl->pdata, PAGE_CBUS, 0xB0, 4, tmp1);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			mutex_lock(&mhl->i2c_lock);
			ret = I2C_WriteByte_block(mhl->pdata, PAGE_CBUS, 0xB0, 4, tmp2);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			if (MHL_STATUS_DCAP_RDY & tmp1[0]) {
				mutex_lock(&mhl->cbus_cmd_lock);
				mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_READ_DEVCAP;
				mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset =
						MHL_DEV_CATEGORY_OFFSET;
				mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0;
				mhl->cmd_rx_cnt += 1;

				if (mhl->cmd_rx_cnt == 1)
					queue_work(mhl->cbus_cmd_wqs, &mhl->cbus_cmd_work);
				mutex_unlock(&mhl->cbus_cmd_lock);
			}

		}
		if ((MHL_STATUS_PATH_ENABLED & tmp1[1]) &&
			!(mhl->pdata->status.linkmode & MHL_STATUS_PATH_ENABLED)) {
			/*printk(KERN_INFO
			"sii8332: %s():%d MHL_STATUS_PATH_ENABLED1\n", __func__, __LINE__);*/
			mhl->pdata->status.linkmode |= MHL_STATUS_PATH_ENABLED;
			mutex_lock(&mhl->cbus_cmd_lock);
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_WRITE_STAT;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset =
					MHL_STATUS_REG_LINK_MODE;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0x01;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[0] =
					mhl->pdata->status.linkmode;
			mhl->cmd_rx_cnt += 1;
			if (mhl->cmd_rx_cnt == 1)
				queue_work(mhl->cbus_cmd_wqs, &mhl->cbus_cmd_work);
			mutex_unlock(&mhl->cbus_cmd_lock);
		} else if (!(MHL_STATUS_PATH_ENABLED & tmp1[1]) &&
			(mhl->pdata->status.linkmode & MHL_STATUS_PATH_ENABLED)) {
			/*printk(KERN_INFO
			"sii8332: %s():%d MHL_STATUS_PATH_ENABLED2\n", __func__, __LINE__);*/
			mhl->pdata->status.linkmode &= ~MHL_STATUS_PATH_ENABLED;
			mutex_lock(&mhl->cbus_cmd_lock);
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].command = MHL_WRITE_STAT;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].offset =
					MHL_STATUS_REG_LINK_MODE;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].lenght = 0x01;
			mhl->msc_cmd_q[mhl->cmd_rx_cnt].buff[0] =
					mhl->pdata->status.linkmode;
			mhl->cmd_rx_cnt += 1;
			if (mhl->cmd_rx_cnt == 1)
				queue_work(mhl->cbus_cmd_wqs, &mhl->cbus_cmd_work);
			mutex_unlock(&mhl->cbus_cmd_lock);
		}

	return ret;
}
/****************************************************************************
*	name	=	get_ddc_access
*	func	=
*	input	=	struct mhl_tx *mhl, u8 *sys_ctrl, bool *result
*	output	=	None
*	return	=	ret
****************************************************************************/
static int get_ddc_access(struct mhl_tx *mhl, u8 *sys_ctrl, bool *result)
{
	int ret = 0;
	u8 timeoout = 50, tmp = 0;
	u8 tpi_controlImage = 0;

	*result = false;

	ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x1A, &tmp);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	*sys_ctrl = tmp;

	tmp |= DDC_BUS_REQUEST_REQUESTED;

	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x1A, tmp);

	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	while (timeoout--) {
		ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x1A, &tpi_controlImage);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		if (tpi_controlImage & DDC_BUS_GRANT_MASK) {
			tmp |= DDC_BUS_GRANT_GRANTED;
			ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x1A, tmp);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
			*result = true;
			return ret;
		}

		ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x1A, tmp);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
		msleep(200);
	}

	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x1A, tmp);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	return ret;
}

/****************************************************************************
*	name	=	release_ddc_access
*	func	=
*	input	=	struct mhl_tx *mhl, u8 sys_ctrl, bool *result
*	output	=	None
*	return	=	ret
****************************************************************************/
static int release_ddc_access(struct mhl_tx *mhl, u8 sys_ctrl, bool *result)
{
	int ret = 0;
	u8 timeoout = 50;
	u8 tpi_controlImage = 0;

	*result = false;

	sys_ctrl &= ~(DDC_BUS_REQUEST_MASK | DDC_BUS_GRANT_MASK);

	while (timeoout--) {
		ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x1A, sys_ctrl);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x1A, &tpi_controlImage);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		if (!(tpi_controlImage & (DDC_BUS_REQUEST_MASK | DDC_BUS_GRANT_MASK)))
		/*When 0x1A[2:1] read "0"*/
			*result = true;
	}

	/*printk(KERN_INFO "sii8332: %s():%d end!\n", __func__, __LINE__);*/
	return ret;
}

/****************************************************************************
*	name	=	avi_control_thread
*	func	=
*	input	=	struct work_struct *work
*	output	=	None
*	return	=	ret
****************************************************************************/
static void avi_control_thread(struct work_struct *work)
{
	struct mhl_tx *mhl =
		container_of(work, struct mhl_tx, avi_control_work);
	int ret = 0;
	u8 sys_ctrl = 0;
	bool result = 1;

	MHL_FUNC_START;
	/*printk(KERN_INFO "%s()\n", __func__);*/
	if (mhl->pdata->status.op_status == NO_MHL_STATUS) {

		printk(KERN_INFO"\n%s():%d mhl_power_on_set\n", __func__, __LINE__);
		/*FIXME JC*/
		/*mhl_onoff_ex(1);*/

#if 0
		sii8332_hw_onoff(1);

		sii8332_hw_reset();

		/* mhl->pdata->status.linkmode = 0x03; /*Commented in backported code*/
		/*printk(" %s calling mhl_init_1\n",__func__);*/
		ret = mhl_init_func(mhl);
		if (ret < 0) {
			PRINT_ERROR;
		}
#ifdef SFEATURE_HDCP_SUPPORT
		{
			  u8 data[5], NumOfOnes = 0;
			  u8 i;
			  memset(data, 0x00, 5);

			  ret = I2C_ReadByte_block(mhl->pdata, 0x72, 0x36, 5, data);

			  for (i = 0; i < 5; i++) {
				  while (data[i] != 0x00) {
					  if (data[i] & 0x01) {
						  NumOfOnes++;
					  }
					  data[i] >>= 1;
				  }
			  }

			if (NumOfOnes != 20) {
				mhl->aksv_available = false; }
			mhl->aksv_available = true;
		}
#endif /*SFEATURE_HDCP_SUPPORT*/
		/*mhl->hdmi_sink = false;
		mhl->pdata->status.op_status = MHL_READY_RGND_DETECT;*/

		ret = TX_GO2D3(mhl->pdata);
		if (ret < 0) {
			PRINT_ERROR;
		}
#endif

		/*printk(KERN_INFO"\n sii8332: %s():(%d)\n", __func__,__LINE__ );*/
	} else if (mhl->pdata->status.op_status == MHL_RX_DISCONNECTED) {


	} else { /* GED changes END here*/
		if (mhl->avi_work == true) {
			switch (mhl->avi_cmd) {
			case HPD_HIGH_EVENT:
#if 0/*edid*/
				ret = edid_read(mhl);
				if (ret < 0)
					return;
				/*After reading the EDID values, set output format,*/
				/*MHL State to true and then call MIPI input.*/
#endif /*edid*/
				mutex_lock(&format_update_lock);

				ret = get_ddc_access(mhl, &sys_ctrl, &result);
				mhl->edid_access_done = true;
				setmhlstate(MHL_STATE_ON);

				mutex_lock(&format_update_lock);
				ret = release_ddc_access(mhl, sys_ctrl, &result);

				MipiIsr(mhl);
				mutex_unlock(&format_update_lock);
				break;
			case HPD_LOW_EVENT:
				pr_info("sii8332: HPD_LOW_EVENT\n");
#ifdef SFEATURE_HDCP_SUPPORT
				mhl->video_out_setting = false;
				ret = HDCP_OFF(mhl);
				if (ret < 0) {
					PRINT_ERROR;
					return;
				}
#endif /*SFEATURE_HDCP_SUPPORT*/

				break;

			default:

				break;
			}
		}
	}
}

/****************************************************************************
*	name	=	CalculateGenericInfoFrameCheckSum
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
uint8_t CalculateGenericInfoFrameCheckSum(uint8_t *infoFrameData, uint8_t checkSum, uint8_t length)
{
	uint8_t i;
	for (i = 0; i < length; i++)
		checkSum += infoFrameData[i];
	checkSum = 0x100 - checkSum;
	return checkSum;
}

/****************************************************************************
*	name	=	set_audio_mode
*	func	=	Audio Mode registers. To be set after there is valid audio input.
				With out proper values Audio will not work.
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int set_audio_mode(struct mhl_tx *mhl)
{
	int ret = 0;
	/*printk(KERN_INFO "sii8332: %s():%d  !\n", __func__, __LINE__);*/

	/*Only for I2S interface... the below registers are
	related to I2S audio source. */
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x26, 0x90);
	/*first setting I2S interface and enabling mutebit,2 channel input*/
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x20, 0x90);/*Rising edge,
	256fs, word sync polarity low,SD justify left, SD direction Byte MSB,
	word sync to SD first bit shift.*/
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x1F, 0x80);/*SD PIN selected,*/
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x21, 0x00); /* 44.1KHZ*/
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x22, 0x00);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x23, 0x00);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x24, 0x00);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x25, 0x02);/*word lenght 16bit*/
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x27, 0x58);/* setting to
	16 bit 2 channel and 48 KHz*/
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	return ret;
}


/****************************************************************************
*	name	=	set_audio_infoframe
*	func	=	Audio Info frame to be set after setting audio mode registers.
				With out proper values Audio will not work.
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int set_audio_infoframe(struct mhl_tx *mhl)
{
	int ret = 0, checksum = 0;
	u8 data[14], i = 0;

	data[0] = 0x84;
	data[1] = 0x01;
	data[2] = 0x0A;
	for (i = 3; i < 14; i++)
		data[i] = 0;

	/* data[4] = 0x00; /*two ch*/

	for (i = 0; i < 14; i++)
		checksum += data[i];
	checksum = 0x100 - checksum;
	data[3] = checksum;

	/*printk(KERN_INFO "[]sii8332: %s():%d AUDIO Checksum :%02x!\n",
				 __func__, __LINE__, checksum);*/

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0xBF,
		BIT_TPI_INFO_EN|BIT_TPI_INFO_RPT | BIT_TPI_INFO_READ_FLAG_NO_READ |
		BIT_TPI_INFO_SEL_Audio);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	msleep(1); /*workaround for the late response of AVI info setting.*/

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte_block(mhl->pdata, PAGE_0, 0xC0, 14, &data[0]);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
		__func__, __LINE__);
		return ret;
	}
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x26, 0x80);
	/*first setting I2S interface and disabling mutebit, 2 channel input*/
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	return ret;
}

/****************************************************************************
*	name	=	color_format_set
*	func	=	Color format registers
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int color_format_set(struct mhl_tx *mhl)
{
	int ret = 0;
	/*printk("%s start\n",__func__);*/
	if (mhl->mipi_input_colorspace == INPUT_COLOR_SPACE_RGB) {
		if (mhl->hdmi_sink) {
			/*printk(KERN_INFO
			"sii8332: %s():%d BIT_TPI_OUTPUT_FORMAT_HDMI_TO_RGB\n",
			__func__, __LINE__);*/
			mhl->mhl_output_colorspace = BIT_TPI_OUTPUT_FORMAT_HDMI_TO_RGB;
		} else {
			/*printk(KERN_INFO
			"sii8332: %s():%d BIT_TPI_OUTPUT_FORMAT_DVI_TO_RGB\n",
			__func__, __LINE__);*/
			mhl->mhl_output_colorspace = BIT_TPI_OUTPUT_FORMAT_DVI_TO_RGB;
		}
	} else if (mhl->mipi_input_colorspace == INPUT_COLOR_SPACE_YCBCR444) {
		/*printk(KERN_INFO
		"sii8332: %s():%d INPUT_COLOR_SPACE_YCBCR444\n",
		__func__, __LINE__);*/
		mhl->mhl_output_colorspace = BIT_TPI_OUTPUT_FORMAT_YCbCr444;
	} else if (mhl->mipi_input_colorspace == INPUT_COLOR_SPACE_YCBCR422) {
		/*printk(KERN_INFO
		"sii8332: %s():%d INPUT_COLOR_SPACE_YCBCR422\n",
		__func__, __LINE__);*/
		mhl->mhl_output_colorspace = BIT_TPI_OUTPUT_FORMAT_YCbCr422;
	} else {
		mhl->mhl_output_colorspace = BIT_TPI_OUTPUT_FORMAT_HDMI_TO_RGB;
		/* setting to default HDMI*/
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2CReadModify(mhl->pdata, PAGE_0, 0x0A,
				BIT_TPI_OUTPUT_FORMAT_MASK, mhl->mhl_output_colorspace);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
	PRINT_ERROR;
		return ret;
	}
	return ret;
}

/****************************************************************************
*	name	=	set_avi_infoframe
*	func	=	Proper avi infoframe & audio infoframe must for proper output.
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int set_avi_infoframe(struct mhl_tx *mhl)
{
	int ret = 0, checksum = 0;
	u8 data[14], i = 0;

	memset(data, 0x00, 14);

	if (mhl->mhl_output_colorspace == BIT_TPI_OUTPUT_FORMAT_YCbCr444) {
		/*printk(KERN_INFO
		"sii8332: %s():%d BIT_TPI_OUTPUT_FORMAT_YCbCr444\n",
		__func__, __LINE__);*/
		data[1] = 0x40;
	} else if (mhl->mhl_output_colorspace == BIT_TPI_OUTPUT_FORMAT_YCbCr422) {
		/*printk(KERN_INFO
		"sii8332: %s():%d BIT_TPI_OUTPUT_FORMAT_YCbCr422\n",
		__func__, __LINE__);*/
		data[1] = 0x20;
	} else	{
		/*printk(KERN_INFO
		"sii8332: %s():%d BIT_TPI_OUTPUT_FORMAT_RGB\n", __func__, __LINE__);*/
		data[1] = 0x00;
	}
	data[2] = mhl->colorimetryAspectRatio;
	data[3] = 0x00;
	data[4] = mhl->inputVideoCode;
	data[5] = 0x00;
	data[6] = 0x00;
	data[7] = 0x00;
	data[8] = 0x00;
	data[9] = 0x00;
	data[10] = 0x00;
	data[11] = 0x00;
	data[12] = 0x00;
	data[13] = 0x00;

	checksum = 0x82 + 0x02 + 0x0D;  /* these are set by the hardware*/

	for (i = 1; i < 14; i++)
		checksum += data[i];

	checksum = 0x100 - checksum;
	data[0] = checksum;

	/*check_AVI_info_checksum(data);*/

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0xBF,
			BIT_TPI_INFO_EN | BIT_TPI_INFO_RPT |
			BIT_TPI_INFO_READ_FLAG_NO_READ | BIT_TPI_INFO_SEL_AVI);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}
	msleep(10); /*workaround for the late response of AVI info setting.*/

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte_block(mhl->pdata, PAGE_0, 0x0C, 14, &data[0]);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
		__func__, __LINE__);
		return ret;
	}
	return ret;
}

/*************************************************************************
*	name	=	set_ouptput_timings
*	func	=	Output timings calculated based on the input format.
				Stable MIPI input is validated.
*	input	=	struct mhl_tx *mhl, bool *input_check
*	output	=	None
*	return	=	ret
**************************************************************************/
static int set_ouptput_timings(struct mhl_tx *mhl, bool *input_check)
{
	int ret = 0;
	u8 rd_data = 0, i = 0, cnt = 0, tmp = 0;
	u16 h_res = 0, v_res = 0;
	video_mode_reg video_mode_set[20], video_mode_regs;
	bool outflag = 0;

	*input_check = false;
	/*printk("%s start\n",__func__);*/
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0xD5, 0x36);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2CReadModify(mhl->pdata, PAGE_3, 0x8E, BIT_MIP_DDR_OVER_2, 0);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	/*ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0xBC, 0x01);*/
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0xBC, 0x02);
	/*set Page by writting to TPI:0xBC, here page 2 is set*/
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	/*ret = I2C_WriteByte(mhl->pdata,PAGE_0, 0xBD, 0x09);*/
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0xBD, 0x24);
	/*select Indexed Offset	within page by writting to TPI:0xBD,
	here indexed register 24*/
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0xBE, &rd_data);
	/*Read/Write register value via: TPI:0xBE, here reading*/
	if (ret < 0) {
		PRINT_ERROR;
		mutex_unlock(&mhl->i2c_lock);
		return ret;
	}
	ret = (ret & 0x80) | (0x02);/*changing the identified bits,only
	Change bit [3:0] to binary 0010 for 16 bits word length and 48KHz*/
	mutex_unlock(&mhl->i2c_lock);

	if (rd_data&0x01) {
		/*printk(KERN_INFO "sii8332: %s():%d  Clock stable\n",
			* __func__, __LINE__);*/
	} else {
		/*printk(KERN_INFO "sii8332: %s():%d  Clock unstable\n",
			* __func__, __LINE__);*/
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0xBE, ret);/*Read/Write
	register value via: TPI:0xBE, here we are writting the ret value*/
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n"
		, __func__, __LINE__);
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	do {
		ret = I2CReadModify(mhl->pdata, PAGE_3, 0x96, BIT1, BIT1);
		if (ret < 0) {
			PRINT_ERROR;
			mutex_unlock(&mhl->i2c_lock);
			return ret;
		}

		ret = I2CReadModify(mhl->pdata, PAGE_3, 0x93,
		MIPI_DSI_FORMAT_RESOLUTION_CLEAR, MIPI_DSI_FORMAT_RESOLUTION_CLEAR);
		if (ret < 0) {
			PRINT_ERROR;
			mutex_unlock(&mhl->i2c_lock);
			return ret;
		}
		msleep(60);

		ret = I2CReadModify(mhl->pdata, PAGE_3, 0x96, BIT1, 0x00);
		if (ret < 0) {
			PRINT_ERROR;
			mutex_unlock(&mhl->i2c_lock);
			return ret;
		}
		ret = I2CReadModify(mhl->pdata, PAGE_3, 0x93,
				MIPI_DSI_FORMAT_RESOLUTION_CLEAR, 0);
		if (ret < 0) {
			PRINT_ERROR;
			mutex_unlock(&mhl->i2c_lock);
			return ret;
		}
		msleep(100);
		ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0xC3, &tmp);
		if (ret < 0) {
			PRINT_ERROR;
			mutex_unlock(&mhl->i2c_lock);
			return ret;
		}
		h_res = tmp;
		/*printk(KERN_INFO "sii8332: %s():%d  h_res1: %02x\n",
		__func__, __LINE__,h_res );*/
		h_res <<= 8;

		ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0xC2, &tmp);
		if (ret < 0) {
			PRINT_ERROR;
			mutex_unlock(&mhl->i2c_lock);
			return ret;
		}
		h_res |= tmp;
		/*printk(KERN_INFO "sii8332: %s():%d  h_res2: %02x\n",
		__func__, __LINE__,h_res );*/

		ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0xC5, &tmp);
		if (ret < 0) {
			PRINT_ERROR;
			mutex_unlock(&mhl->i2c_lock);
			return ret;
		}
		v_res = tmp;
		/*printk(KERN_INFO "sii8332: %s():%d  v_res1: %02x\n",
		__func__, __LINE__,v_res );     */
		v_res <<= 8;

		ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0xC4, &tmp);
		if (ret < 0) {
			PRINT_ERROR;
			mutex_unlock(&mhl->i2c_lock);
			return ret;
		}
		v_res |= tmp;
		/*printk(KERN_INFO "sii8332: %s():%d  v_res2: %02x\n",
		__func__, __LINE__,v_res );

		printk(KERN_INFO "sii8332: %s():%d  MIPI Input mode: %d * %d\n",
		__func__, __LINE__,h_res, v_res );
		printk(KERN_INFO "sii8332: %s():%d  NUM_OF_VIDEO_MODE: %d\n",
		__func__, __LINE__,NUM_OF_VIDEO_MODE );*/

		for (i = 0; i < NUM_OF_VIDEO_MODE; ++i) {
			if (h_res == VideoModeInfo[i].header.hRes) {
				if (v_res ==  VideoModeInfo[i].header.vRes) {
					mhl->colorimetryAspectRatio =
						VideoModeInfo[i].header.colorimetryAspectRatio;
					mhl->inputVideoCode =
						VideoModeInfo[i].header.inputVideoCode;
					mhl->interlaced = VideoModeInfo[i].header.interlaced;

					memset(video_mode_set, 0x00, sizeof(video_mode_regs) * 20);
					memcpy(video_mode_set, VideoModeInfo[i].regVals,
										sizeof(video_mode_regs) * 20);
					{
					u8 j;
					for (j = 0; j < 19; j++) {
						/*printk(KERN_INFO
						"sii8332: %s():%d  video	mode set [j]:%d\n",
						__func__, __LINE__, j );
						printk(KERN_INFO
						"sii8332:%s():%d ID:%02x, offset:%02x, value:%02x\n",
						__func__, __LINE__, video_mode_set[j].device_id,
						video_mode_set[j].offset, video_mode_set[j].value );*/
						ret = I2C_WriteByte(mhl->pdata,
						video_mode_set[j].device_id,
						video_mode_set[j].offset, video_mode_set[j].value);
						if (ret < 0) {
							PRINT_ERROR;
							mutex_unlock(&mhl->i2c_lock);
							return ret;
						}
					}
					}

					ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0xA5, 0x1C);
					if (ret < 0) {
						PRINT_ERROR;
						mutex_unlock(&mhl->i2c_lock);
						return ret;
					}

					ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0xA5, 0x26);
					if (ret < 0) {
						PRINT_ERROR;
						mutex_unlock(&mhl->i2c_lock);
						return ret;
					}

					ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0xA5, 0x32);
					if (ret < 0) {
						PRINT_ERROR;
						mutex_unlock(&mhl->i2c_lock);
						return ret;
					}

					ret = I2CReadModify(mhl->pdata, PAGE_3, 0x8E, 0x87, 0x03);
					if (ret < 0) {
						PRINT_ERROR;
						mutex_unlock(&mhl->i2c_lock);
						return ret;
					}

					outflag = true;
				}

			}

		}

		cnt++;

		if ((!outflag) && (cnt == 10)) {
			printk(KERN_INFO"[ERROR]sii8332:%s():%d can't find resolution!\n",
			__func__, __LINE__);
			cnt = 0;
			*input_check = false;
			mutex_unlock(&mhl->i2c_lock);
			return ret;
		}

	} while (!outflag);

	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0xD5, 0x37 | mhl->interlaced);
	if (ret < 0) {
		PRINT_ERROR;
		mutex_unlock(&mhl->i2c_lock);
		return ret;
	}
	mutex_unlock(&mhl->i2c_lock);
	*input_check = true;

	return ret;
}
/****************************************************************************
*	name	=	MipiIsr
*	func	=	MIPI Input
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/

static int MipiIsr(struct mhl_tx *mhl)
{
	u8 th_intr = 0;
	u8 temp;
	int ret = 0;
	u8 rd_data = 0, tmp = 0;
	bool input_check = 0;

	/*printk("%s start\n",__func__);*/

	/*TMDS output enable*/
	I2CReadModify(mhl->pdata, PAGE_0, 0x1A, TMDS_OUTPUT_CONTROL_MASK |
			AV_MUTE_MASK | OUTPUT_MODE_MASK, TMDS_OUTPUT_CONTROL_ACTIVE |
			AV_MUTE_NORMAL|OUTPUT_MODE_HDMI);

	/*read MIPI/DSI contol register*/
	I2C_ReadByte(mhl->pdata, PAGE_3, 0x92, &th_intr);

	/*pixel format changed*/
	I2C_WriteByte(mhl->pdata, PAGE_3, 0x92, th_intr | 0x40);
	I2C_WriteByte(mhl->pdata, PAGE_3, 0x92, th_intr);

	/*read pixel format*/
	I2C_ReadByte(mhl->pdata, PAGE_3, 0x94, &temp);
	temp &= 0x3f;

	/*printk("MIPI -> Format 0x%02X -\n", (int)temp);*/

	switch (temp) {
	case 0x0C:
			/*printk("LPPS 20-bit YCbCr 4:2:2\n");*/
			break;
	case 0x1C:
			/*printk("PPS 24-bit YCbCr 4:2:2\n");*/
			break;
	case 0x2C:
			/*printk("PPS 16-bit YCbCr 4:2:2\n");*/
			break;
	case 0x0E:
			/*printk("PPS 16-bit RGB 565\n");*/
			break;
	case 0x1E:
			/*printk("PPS 18-bit RGB 666\n");*/
			break;
	case 0x2E:
			/*printk("LPPS 18-bit RGB 666\n");*/
			break;
	case 0x3E:
			/*printk("PPS 24-bit RGB 888\n");*/
			break;
	default:
			/*printk("Unknown\n");*/
			break;
	}
	/*printk(KERN_INFO " %s()\n", __func__);*/
	I2CReadModify(mhl->pdata, PAGE_3, 0x8e, 0x80, 3);
	msleep(100);

	#ifdef SFEATURE_HDCP_SUPPORT
	mhl->video_out_setting = false;
	#endif/*SFEATURE_HDCP_SUPPORT*/
	mhl->hdmi_sink = true;
	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x40, &rd_data);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	/*printk("sii8332 : %s rd_data(0x%x)\n",__func__,rd_data);*/
	if ((rd_data & 0x01) == 0x01) {
		/*printk(KERN_INFO "sii8332: %s():%d  SCDT/CKDT O.K\n",
		__func__, __LINE__ );*/
		mutex_lock(&mhl->i2c_lock);
		ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x1A, &rd_data);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
#ifdef SFEATURE_HDCP_SUPPORT
		if (mhl->hdcp_started) {
			/*printk(KERN_INFO"sii8332:%s():%d HDCPON but SCDT change!\n"
				__func__, __LINE__ );*/
			mhl->video_out_setting = false;
			ret = HDCP_OFF(mhl);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			mutex_lock(&mhl->i2c_lock);
			ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
					TMDS_OUTPUT_CONTROL_MASK | OUTPUT_MODE_MASK,
					TMDS_OUTPUT_CONTROL_POWER_DOWN | OUTPUT_MODE_DVI);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
			mutex_lock(&mhl->i2c_lock);
			ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xC0);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			mhl->intr_tpi_mask = (BIT_TPI_INTR_ST0_HOT_PLUG_EVENT |
				BIT_TPI_INTR_ST0_BKSV_ERR | BIT_TPI_INTR_ST0_BKSV_DONE |
				BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT |
				BIT_TPI_INTR_ST0_HDCP_VPRIME_VALUE_READY_EVENT |
				BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT);

			ret = I2C_WriteByte(mhl->pdata, PAGE_0,
							0x3C, mhl->intr_tpi_mask);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			mutex_lock(&mhl->i2c_lock);
			ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xFC);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
			/*printk(KERN_INFO
			"sii8332: %s():%d TMDS_OUTPUT_CONTROL_ACTIVE, MUTE,"
			"OUTPUT_MODE_HDMI\n", __func__, __LINE__ );*/
			mutex_lock(&mhl->i2c_lock);
			ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
				TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK | OUTPUT_MODE_MASK,
				TMDS_OUTPUT_CONTROL_ACTIVE | AV_MUTE_MUTED|OUTPUT_MODE_HDMI);

			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
		}
#endif/*SFEATURE_HDCP_SUPPORT*/

		if ((rd_data & TMDS_OUTPUT_CONTROL_POWER_DOWN)) {
			/*printk(KERN_INFO
			"sii8332: %s():%d (rd_data &TMDS_OUTPUT_CONTROL_POWER_DOWN)\n",
			__func__, __LINE__ );*/
			if (mhl->hdmi_sink) {
				#ifdef SFEATURE_HDCP_SUPPORT
				mutex_lock(&mhl->i2c_lock);
				mhl->intr_tpi_mask = (BIT_TPI_INTR_ST0_HOT_PLUG_EVENT |
					BIT_TPI_INTR_ST0_BKSV_ERR | BIT_TPI_INTR_ST0_BKSV_DONE |
					BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT |
					BIT_TPI_INTR_ST0_HDCP_VPRIME_VALUE_READY_EVENT |
					BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT);
				ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x3C, mhl->intr_tpi_mask);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				#endif /*SFEATURE_HDCP_SUPPORT*/

				mutex_lock(&mhl->i2c_lock);
				ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xFC);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				/*printk(KERN_INFO
				"sii8332:%s():%d TMDS_OUTPUT_CONTROL_ACTIVE,"
				"MUTE, OUTPUT_MODE_HDMI\n", __func__, __LINE__ );*/
				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
				TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK | OUTPUT_MODE_MASK,
				TMDS_OUTPUT_CONTROL_ACTIVE | AV_MUTE_MUTED | OUTPUT_MODE_HDMI);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
			} else {
				/*printk("%s no hdmi->sink\n",__func__);*/
#ifdef SFEATURE_HDCP_SUPPORT
				mutex_lock(&mhl->i2c_lock);
				mhl->intr_tpi_mask = (BIT_TPI_INTR_ST0_HOT_PLUG_EVENT |
					BIT_TPI_INTR_ST0_BKSV_ERR | BIT_TPI_INTR_ST0_BKSV_DONE |
					BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT |
					BIT_TPI_INTR_ST0_HDCP_VPRIME_VALUE_READY_EVENT |
					BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT);
				ret = I2C_WriteByte(mhl->pdata, PAGE_0,
						0x3C, mhl->intr_tpi_mask);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
#endif /*SFEATURE_HDCP_SUPPORT*/

				mutex_lock(&mhl->i2c_lock);
				ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xFC);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				/*printk(KERN_INFO
				"sii8332:%s():%d TMDS_OUTPUT_CONTROL_ACTIVE,"
				" MUTE, OUTPUT_MODE_DVI\n", __func__, __LINE__ );*/
				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
				TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK | OUTPUT_MODE_MASK,
				TMDS_OUTPUT_CONTROL_ACTIVE | AV_MUTE_MUTED|OUTPUT_MODE_DVI);

				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
			}
		}
	} else {
		/*printk(KERN_INFO "sii8332: %s():%d  SCDT/CKDT NOT O.K !!\n",
		__func__, __LINE__ );*/
#ifdef SFEATURE_HDCP_SUPPORT
		mhl->video_out_setting = false;
		ret = HDCP_OFF(mhl);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
#endif /*SFEATURE_HDCP_SUPPORT*/

		mutex_lock(&mhl->i2c_lock);
		ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
				TMDS_OUTPUT_CONTROL_MASK | OUTPUT_MODE_MASK,
				TMDS_OUTPUT_CONTROL_POWER_DOWN | OUTPUT_MODE_DVI);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			printk(KERN_INFO "[ERROR]sii8332: %s():%d failed !\n",
			__func__, __LINE__);
			return ret;
		}

		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xC0);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

#ifdef SFEATURE_HDCP_SUPPORT
		mutex_lock(&mhl->i2c_lock);
		mhl->intr_tpi_mask = BIT_TPI_INTR_ST0_HOT_PLUG_EVENT;
		ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x3C, mhl->intr_tpi_mask);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
#endif /*SFEATURE_HDCP_SUPPORT*/
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x92, &rd_data);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

#if 0
	if (rd_data & BIT_MIPI_R_DSI_PXL_FORMAT_CHANGED)
		/*printk(KERN_INFO "sii8332: %s():%d
		BIT_MIPI_R_DSI_PXL_FORMAT_CHANGED\n", __func__, __LINE__);*/
	else
		/*printk(KERN_INFO "sii8332: %s():%d
		NOT BIT_MIPI_R_DSI_PXL_FORMAT_CHANGED\n", __func__, __LINE__);*/
#endif

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x92,
			rd_data | BIT_MIPI_W_DSI_PXL_FORMAT_CHANGED_IN);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x92, rd_data);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x94, &tmp);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	tmp &= 0x3F;

	switch (tmp)	{
	case 0x0C:
		/*printk(KERN_INFO "sii8332: %s():%d  LPPS 20-bit YCbCr 4:2:2\n",
		__func__, __LINE__);*/
		mhl->mipi_input_colorspace = INPUT_COLOR_SPACE_YCBCR422;
		break;

	case 0x1C:
		/*printk(KERN_INFO "sii8332: %s():%d  PPS 24-bit YCbCr 4:2:2\n",
		__func__, __LINE__);*/
		mhl->mipi_input_colorspace = INPUT_COLOR_SPACE_YCBCR422;
		break;

	case 0x2C:
		/*printk(KERN_INFO "sii8332: %s():%d  PPS 16-bit YCbCr 4:2:2\n",
		__func__, __LINE__);*/
		mhl->mipi_input_colorspace = INPUT_COLOR_SPACE_YCBCR422;
		break;

	case 0x0E:
		/*printk(KERN_INFO "sii8332: %s():%d  PPS 16-bit RGB 565\n",
		__func__, __LINE__);*/
		mhl->mipi_input_colorspace = INPUT_COLOR_SPACE_RGB;
		break;

	case 0x1E:
		/*printk(KERN_INFO "sii8332: %s():%d PPS 18-bit RGB 666\n",
		__func__, __LINE__);*/
		mhl->mipi_input_colorspace = INPUT_COLOR_SPACE_RGB;
		break;

	case 0x2E:
		/*printk(KERN_INFO "sii8332: %s():%d LPPS 18-bit RGB 666\n",
		__func__, __LINE__);*/
		mhl->mipi_input_colorspace = INPUT_COLOR_SPACE_RGB;
		break;

	case 0x3E:
		/*printk(KERN_INFO "sii8332: %s():%d PPS 24-bit RGB 888\n",
		__func__, __LINE__);*/
		mhl->mipi_input_colorspace = INPUT_COLOR_SPACE_RGB;
		break;

	default:
		/*printk(KERN_INFO "sii8332: %s():%d Unknown...\n",
		__func__, __LINE__);*/
		mhl->mipi_input_colorspace = INPUT_COLOR_SPACE_RGB;
		break;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x09, mhl->mipi_input_colorspace);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = set_ouptput_timings(mhl, &input_check);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	if (!input_check) {
		/*printk(KERN_INFO "sii8332: %s():%d MIPI input error\n",
		__func__, __LINE__);*/
		return ret;
	}

	ret = color_format_set(mhl);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	ret = set_audio_mode(mhl);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	if (mhl->hdmi_sink) {
		/*printk(KERN_INFO "sii8332: %s():%d Setting infoframe\n",
		__func__, __LINE__);*/
		ret = set_audio_infoframe(mhl);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		ret = set_avi_infoframe(mhl);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

#ifdef SFEATURE_HDCP_SUPPORT
		mhl->video_out_setting = true;
		/*printk(KERN_INFO "sii8332: %s():%d  hdcp_on_ready %d\n",
		__func__, __LINE__, mhl->hdcp_on_ready);*/
		if (mhl->hdcp_on_ready == true) {
			if (!mhl->hdcp_started) {
				ret = HDCP_ON(mhl);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
			}
		} else {
			mutex_lock(&mhl->i2c_lock);
			ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x29, &rd_data);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			if (rd_data & PROTECTION_TYPE_MASK) {
				/*printk(KERN_INFO "sii8332: %s():%d  hdcp_on_ready true\n",
				__func__, __LINE__);*/
				mhl->hdcp_on_ready = true;
				if (!mhl->hdcp_started) {
					ret = HDCP_ON(mhl);
					if (ret < 0) {
						PRINT_ERROR;
						return ret;
					}
				}
			}
		}

#else
		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xFC);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
		/*printk(KERN_INFO "sii8332: %s():%d  TMDS_OUTPUT_CONTROL_ACTIVE,"
			"AV_MUTE_NORMAL, OUTPUT_MODE_HDMI\n", __func__, __LINE__ );*/
		mutex_lock(&mhl->i2c_lock);
		ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
			TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK | OUTPUT_MODE_MASK,
			TMDS_OUTPUT_CONTROL_ACTIVE | AV_MUTE_NORMAL|OUTPUT_MODE_HDMI);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
#endif /*SFEATURE_HDCP_SUPPORT*/
	} else {
#ifdef SFEATURE_HDCP_SUPPORT
		mhl->video_out_setting = true;
		/*printk(KERN_INFO "sii8332: %s():%d  hdcp_on_ready %d\n",
		__func__, __LINE__, mhl->hdcp_on_ready);*/
		if (mhl->hdcp_on_ready == true) {
			if (!mhl->hdcp_started) {
				ret = HDCP_ON(mhl);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
			}
		} else {
			mutex_lock(&mhl->i2c_lock);
			ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x29, &rd_data);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			if (rd_data & PROTECTION_TYPE_MASK) {
				/*printk(KERN_INFO "sii8332: %s():%d  hdcp_on_ready true\n",
				__func__, __LINE__);*/
				mhl->hdcp_on_ready = true;
				if (!mhl->hdcp_started) {
					ret = HDCP_ON(mhl);
					if (ret < 0) {
						PRINT_ERROR;
						return ret;
					}
				}
			}
		}
#else
		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xFC);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
		mutex_lock(&mhl->i2c_lock);
		/*printk(KERN_INFO "sii8332: %s():%d  TMDS_OUTPUT_CONTROL_ACTIVE,"
			"AV_MUTE_NORMAL, OUTPUT_MODE_DVI\n", __func__, __LINE__ );*/
		ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
			TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK | OUTPUT_MODE_MASK,
			TMDS_OUTPUT_CONTROL_ACTIVE | AV_MUTE_NORMAL | OUTPUT_MODE_DVI);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
#endif /*SFEATURE_HDCP_SUPPORT*/
	}

	return ret;
}

/****************************************************************************
*	name	=	discovery_success
*	func	=
*	input	=	struct mhl_tx *mhl
*	output	=	None
*	return	=	ret
****************************************************************************/
static int discovery_success(struct mhl_tx *mhl)
{
	int ret;
	u8 int4status, int5status, tpi_status;
	count = 0;
	resumed = 1;
#ifdef SFEATURE_HDCP_SUPPORT
	u8 intr_status = 0;
#endif
	/*MHL_FUNC_START;*/

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x21, &int4status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x23, &int5status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x3D, &tpi_status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}

	/*irq_status(mhl);*/
	/*printk("sii8332 : %s int4status(0x%x), int5status(0x%x)
		tpi_status(0x%x)\n", __func__, int4status, int5status,tpi_status);*/
	if (int4status) {
		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x21, int4status);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}
		if (int4status & BIT_INTR4_SCDT_CHANGE) {
			/*printk(KERN_INFO "sii8332: %s():%d  BIT_INTR4_SCDT_CHANGE\n",
			__func__, __LINE__);*/
			I2CReadModify(mhl->pdata, PAGE_0, 0x1A, TMDS_OUTPUT_CONTROL_MASK |
				AV_MUTE_MASK | OUTPUT_MODE_MASK, TMDS_OUTPUT_CONTROL_ACTIVE |
				AV_MUTE_NORMAL|OUTPUT_MODE_HDMI);
		}
		if (int4status & BIT_INTR4_CBUS_DISCONNECT) {
			pr_info("sii8332 : BIT_INTR4_CBUS_DISCONNECT !!!\n");

			mhl->pdata->status.op_status = MHL_RX_DISCONNECTED;
#ifdef SFEATURE_HDCP_SUPPORT
			mhl->video_out_setting = false;
			ret = HDCP_OFF(mhl);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
#endif /*SFEATURE_HDCP_SUPPORT*/
		}
		if (int4status & BIT_INTR4_CBUS_LKOUT) {
			ret = forceUSBIDswitchOpen(mhl);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			ret = releaseUSBIDswitchOpen(mhl);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
		}
	}

	ret = mhl_cbus_intr(mhl);
	if (ret < 0) {
		PRINT_ERROR;
		return ret;
	}


	if (tpi_status) {
		mutex_lock(&mhl->i2c_lock);
		ret = I2C_WriteByte(mhl->pdata, PAGE_0, 0x3D, tpi_status);
		mutex_unlock(&mhl->i2c_lock);
		if (ret < 0) {
			PRINT_ERROR;
			return ret;
		}

		if (BIT_TPI_INTR_ST0_HOT_PLUG_EVENT & tpi_status) {
			/*printk(KERN_INFO
			"sii8332: %s():%d BIT_TPI_INTR_ST0_HOT_PLUG_EVENT\n",
			__func__, __LINE__);*/
			mutex_lock(&mhl->i2c_lock);
			ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x0D, &tpi_status);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
			if (tpi_status & BIT6) {
				/*printk(KERN_INFO "sii8332: %s():%d  HPD HIGH\n",
				__func__, __LINE__);*/
				mhl_plugedin_state = MHL_STATE_ON;
				mhl->avi_work = true;
				mhl->avi_cmd = HPD_HIGH_EVENT;
				if (waitqueue_active(&mhl->avi_control_wq)) {
					/*printk(KERN_INFO "sii8332: %s():%d wake_up\n",
					__func__, __LINE__);*/
					wake_up(&mhl->avi_control_wq);
				} else
					queue_work(mhl->avi_control_wqs, &mhl->avi_control_work);
			} else {
				/*printk(KERN_INFO "sii8332: %s():%d  HPD LOW\n",
				__func__, __LINE__);*/
				mhl_plugedin_state = MHL_STATE_OFF;
				setmhlstate(MHL_STATE_OFF);
				mhl->avi_work = true;
				mhl->avi_cmd = HPD_LOW_EVENT;
				if (waitqueue_active(&mhl->avi_control_wq)) {
					/*printk(KERN_INFO "sii8332: %s():%d wake_up\n",
					__func__, __LINE__);*/
					wake_up(&mhl->avi_control_wq);
				} else
					queue_work(mhl->avi_control_wqs, &mhl->avi_control_work);
			}
		}

#ifdef SFEATURE_HDCP_SUPPORT
	{
		u8 rd_data = 0;
		if ((mhl->intr_tpi_mask&BIT_TPI_INTR_ST0_BKSV_DONE) &&
			(BIT_TPI_INTR_ST0_BKSV_DONE & intr_status)) {
			/*printk(KERN_INFO"sii8332:%s():%d BIT_TPI_INTR_ST0_BKSV_DONE\n",
			__func__, __LINE__);*/
			mutex_lock(&mhl->i2c_lock);
			ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x29, &rd_data);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
			if (rd_data & PROTECTION_TYPE_MASK) {
				mhl->hdcp_on_ready = true;
				/*printk(KERN_INFO "sii8332: %s():%d  hdcp_on_ready true\n",
				__func__, __LINE__);*/
				if ((mhl->video_out_setting) && !mhl->hdcp_started) {
					ret = HDCP_ON(mhl);
					if (ret < 0) {
						PRINT_ERROR;
						return ret;
					}
				}
			} else {
				/*printk(KERN_INFO "sii8332: %s():%d  hdcp_on_ready false\n",
				__func__, __LINE__);*/
				mhl->hdcp_on_ready = false;
			}
		}
		if ((mhl->intr_tpi_mask & BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT)
			&& (BIT_TPI_INTR_ST0_HDCP_SECURITY_CHANGE_EVENT & intr_status)) {
			mutex_lock(&mhl->i2c_lock);
			ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x29, &rd_data);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}
			rd_data &= LINK_STATUS_MASK;

			switch (rd_data) {
			case LINK_STATUS_NORMAL:
				/*printk(KERN_INFO "sii8332:%s():%d  LINK_STATUS_NORMAL\n",
				__func__, __LINE__);*/
				break;

			case LINK_STATUS_LINK_LOST:
				/*printk(KERN_INFO "sii8332:%s():%d  LINK_STATUS_LINK_LOST\n",
				__func__, __LINE__);*/
				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
								AV_MUTE_MASK, AV_MUTE_MUTED);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
					TMDS_OUTPUT_CONTROL_MASK | OUTPUT_MODE_MASK,
					TMDS_OUTPUT_CONTROL_POWER_DOWN | OUTPUT_MODE_DVI);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				mutex_lock(&mhl->i2c_lock);
				ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xC0);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}

				mutex_lock(&mhl->i2c_lock);
				ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xFC);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
						TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK |
						OUTPUT_MODE_MASK, TMDS_OUTPUT_CONTROL_ACTIVE |
						AV_MUTE_MUTED | OUTPUT_MODE_HDMI);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				break;

			case LINK_STATUS_RENEGOTIATION_REQ:
				/*printk(KERN_INFO
				"sii8332: %s():%d LINK_STATUS_RENEGOTIATION_REQ\n",
				__func__, __LINE__);*/
				ret = HDCP_OFF(mhl);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				ret = HDCP_ON(mhl);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				break;

			case LINK_STATUS_LINK_SUSPENDED:
				/*printk(KERN_INFO
				"sii8332: %s():%d LINK_STATUS_LINK_SUSPENDED\n",
				__func__, __LINE__);*/
				ret = HDCP_ON(mhl);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				break;

			default:
				break;
			}
		}

		if ((mhl->intr_tpi_mask &
			BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT) &&
			(BIT_TPI_INTR_ST0_HDCP_AUTH_STATUS_CHANGE_EVENT & intr_status)) {

			mutex_lock(&mhl->i2c_lock);
			ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x29, &rd_data);
			mutex_unlock(&mhl->i2c_lock);
			if (ret < 0) {
				PRINT_ERROR;
				return ret;
			}

			switch ((rd_data & (EXTENDED_LINK_PROTECTION_MASK |
					LOCAL_LINK_PROTECTION_MASK))) {
			case (EXTENDED_LINK_PROTECTION_NONE |
				LOCAL_LINK_PROTECTION_NONE):
				/*printk(KERN_INFO
				"sii8332: %s():%d (EXTENDED_LINK_PROTECTION_NONE |"
				"LOCAL_LINK_PROTECTION_NONE)\n", __func__, __LINE__);*/
				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
								AV_MUTE_MASK, AV_MUTE_MUTED);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}

				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
					TMDS_OUTPUT_CONTROL_MASK | OUTPUT_MODE_MASK,
					TMDS_OUTPUT_CONTROL_POWER_DOWN | OUTPUT_MODE_DVI);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}

				mutex_lock(&mhl->i2c_lock);
				ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xC0);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}

				mutex_lock(&mhl->i2c_lock);
				ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xFC);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}

				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
						TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK |
						OUTPUT_MODE_MASK, TMDS_OUTPUT_CONTROL_ACTIVE |
						AV_MUTE_MUTED | OUTPUT_MODE_HDMI);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				break;

			case LOCAL_LINK_PROTECTION_SECURE:
				/*printk(KERN_INFO
				"sii8332: %s():%d LOCAL_LINK_PROTECTION_SECURE\n",
				__func__, __LINE__);*/
				mutex_lock(&mhl->i2c_lock);
				ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xFC);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				if (mhl->hdmi_sink) {
					/*printk(KERN_INFO
					"sii8332:%s():%d TMDS_OUTPUT_CONTROL_ACTIVE,"
					"AV_MUTE_NORMAL, OUTPUT_MODE_HDMI\n",
					__func__, __LINE__ );*/
					mutex_lock(&mhl->i2c_lock);
					ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
						TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK |
						OUTPUT_MODE_MASK, TMDS_OUTPUT_CONTROL_ACTIVE |
						AV_MUTE_NORMAL | OUTPUT_MODE_HDMI);
					mutex_unlock(&mhl->i2c_lock);
					if (ret < 0) {
						PRINT_ERROR;
						return ret;
					}
				} else {
					mutex_lock(&mhl->i2c_lock);
					/*printk(KERN_INFO
					"sii8332: %s():%d TMDS_OUTPUT_CONTROL_ACTIVE,"
					"AV_MUTE_NORMAL, OUTPUT_MODE_DVI\n",
					__func__, __LINE__ );*/
					ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
						TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK |
						OUTPUT_MODE_MASK, TMDS_OUTPUT_CONTROL_ACTIVE |
						AV_MUTE_NORMAL | OUTPUT_MODE_DVI);
					mutex_unlock(&mhl->i2c_lock);
					if (ret < 0) {
						PRINT_ERROR;
						return ret;
					}
				}
				break;

			case (EXTENDED_LINK_PROTECTION_SECURE |
						LOCAL_LINK_PROTECTION_SECURE):
				/*printk(KERN_INFO
				"sii8332: %s():%d (EXTENDED_LINK_PROTECTION_SECURE |"
				"LOCAL_LINK_PROTECTION_SECURE)\n", __func__, __LINE__);*/
				break;

			default:
				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
							AV_MUTE_MASK, AV_MUTE_MUTED);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}

				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
						TMDS_OUTPUT_CONTROL_MASK | OUTPUT_MODE_MASK,
						TMDS_OUTPUT_CONTROL_POWER_DOWN | OUTPUT_MODE_DVI);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}

				mutex_lock(&mhl->i2c_lock);
				ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xC0);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}

				mutex_lock(&mhl->i2c_lock);
				ret = I2C_WriteByte(mhl->pdata, PAGE_3, 0x31, 0xFC);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}

				mutex_lock(&mhl->i2c_lock);
				ret = I2CReadModify(mhl->pdata, PAGE_0, 0x1A,
					TMDS_OUTPUT_CONTROL_MASK | AV_MUTE_MASK |
					OUTPUT_MODE_MASK, TMDS_OUTPUT_CONTROL_ACTIVE |
					AV_MUTE_MUTED | OUTPUT_MODE_HDMI);
				mutex_unlock(&mhl->i2c_lock);
				if (ret < 0) {
					PRINT_ERROR;
					return ret;
				}
				break;
			}
		}

	}
#endif /*SFEATURE_HDCP_SUPPORT*/
	}
	return ret;
}

#if 0
static void irq_status(struct mhl_tx *mhl)
{
	u8 intMStatus;
	int ret;
	/*printk("%s\n",__func__);*/

	ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x75, &intMStatus);
	/* read status of interrupt register*/
	/*printk("Drv:INT1 MASK = %02X  ret : %d\n", intMStatus,ret);*/
	/*intMStatus &= 0x01; /*RG mask bit 0 */

	/*if (intMStatus)*/
	{
		/*printk("\nDrv: INT STILL LOW\n");*/
		/*RG addition check INT MASKS*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x75, &intMStatus);
		/*printk("Drv:INT1 MASK = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x76, &intMStatus);
		/*printk("Drv:INT2 MASK = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x77, &intMStatus);
		/*printk("Drv:INT3 MASK = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x22, &intMStatus);
		/*printk("Drv:INT4 MASK = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x24, &intMStatus);
		/*printk("Drv:INT5 MASK = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x09, &intMStatus);
		/*printk("Drv:CBUS1 MASK = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x1F, &intMStatus);
		/*printk("Drv: CBUS2 MASK = %02X  ret:%d\n", intMStatus,ret);*/
		/*RG addition can enable if INT line gets stuck low*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x71, &intMStatus);
		/*printk("Drv:INT1 Status = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x72, &intMStatus);
		/*printk("Drv:INT2 Status = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x73, &intMStatus);
		/*printk("Drv:INT3 Status = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x21, &intMStatus);
       /*printk("Drv:INT4 Status = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x23, &intMStatus);
       /*printk("Drv:INT5 Status = %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x08, &intMStatus);
       /*printk("Drv:CBUS INTR_1 Status = %02X ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x1E, &intMStatus);
       /*printk("Drv:CBUS INTR_2 Status: %02X  ret:%d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xA0, &intMStatus);
       /*printk("Drv:A0 INT Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xA1, &intMStatus);
       /*printk("Drv:A1 INT Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xA2, &intMStatus);
       /*printk("Drv:A2 INT Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xA3, &intMStatus);
       /*printk("Drv:A3 INT Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xB0, &intMStatus);
       /*printk("Drv:B0 STATUS Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xB1, &intMStatus);
       /*printk("Drv:B1 STATUS Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xB2, &intMStatus);
       /*printk("Drv:B2 STATUS Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xB3, &intMStatus);
       /*printk("Drv:B3 STATUS Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xE0, &intMStatus);
       /*printk("Drv:E0 STATUS Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xE1, &intMStatus);
       /*printk("Drv:E1 STATUS Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xE2, &intMStatus);
       /*printk("Drv:E2 STATUS Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xE3, &intMStatus);
       /*printk("Drv:E3 STATUS Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xF0, &intMStatus);
       /*printk("Drv:F0 INT Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xF1, &intMStatus);
       /*printk("Drv:F1 INT Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xF2, &intMStatus);
       /*printk("Drv:F2 INT Set = %02X  ret: %d\n", intMStatus,ret);*/
		ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0xF3, &intMStatus);
       /*printk("Drv:F3 INT Set = %02X  ret: %d\n", intMStatus,ret);*/

	}

}
#endif

void sii8332_hw_onoff(bool onoff)
{
	/*printk(KERN_INFO "sii8332: %s(%d)\n", __func__, onoff);*/

	gpio_set_value(mhlglobal->pdata->mhl_en, onoff);
}

void sii8332_hw_reset(void)
{
	/*printk(KERN_INFO "sii8332: %s()\n", __func__);*/
	usleep_range(10000, 20000);
	gpio_set_value(mhlglobal->pdata->mhl_rst, 0);
	usleep_range(10000, 20000);
	gpio_set_value(mhlglobal->pdata->mhl_rst, 1);
}


u8 mhl_onoff_ex(bool onoff)
{

	int ret;

	struct mhl_tx *mhl;

	pr_info("sii8332: %s(%s)\n", __func__, onoff ? "on" : "off");

	if (mhlglobal == NULL) {
		pr_info("sii8332: mhlglobal is NULL\n");
		return (int)-1;
	}

	mhl = mhlglobal;

	if (mhl->mhl_onoff == onoff) {
		pr_info("sii8332: %s already (%s)\n", __func__, onoff ? "on" : "off");
		return (int)-1;
	}

	mhl->mhl_onoff = onoff;

	if (onoff) {

		ret = ldo2_onoff(onoff);

		if (!ret) {
			printk(KERN_INFO " Failed to enable the LDO2");
		}

		sii8332_hw_onoff(onoff);

		sii8332_hw_reset();

		mhl_init_func(mhl);

#ifdef SFEATURE_HDCP_SUPPORT
		{
			  u8 data[5], NumOfOnes = 0;
			  u8 i;
			  memset(data, 0x00, 5);

			  ret = I2C_ReadByte_block(mhl->pdata, 0x72, 0x36, 5, data);

			  for (i = 0; i < 5; i++) {
				  while (data[i] != 0x00) {
					  if (data[i] & 0x01) {
						  NumOfOnes++;
					  }
					  data[i] >>= 1;
				  }
			  }

			if (NumOfOnes != 20) {
				mhl->aksv_available = false;
			}
			mhl->aksv_available = true;
		}
#endif /*SFEATURE_HDCP_SUPPORT*/

		TX_GO2D3(mhl->pdata);

		enable_irq_wake(mhl->pdata->irq);

		ret = 1;
	} else {
		disable_irq_wake(mhl->pdata->irq);

		sii8332_hw_onoff(onoff);

		ret = ldo2_onoff(onoff);

		if (!ret) {
			printk(KERN_INFO " Failed to disable the LDO2");
		}

		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(mhl_onoff_ex);

/****************************************************************************
*	name	=	mhl_irq_thread
*	func	=
*	input	=	int irq, void *data
*	output	=	None
*	return	=	ret
****************************************************************************/
static irqreturn_t mhl_irq_thread(int irq, void *data)
{
	struct mhl_tx *mhl = data;
	int ret;
	u8 int4status, local_op_status = 0;
	u8 tpi_status, cbus1_status, cbus2_status;

	MHL_FUNC_START;

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_3, 0x21, &int4status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return IRQ_HANDLED;

	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_0, 0x3D, &tpi_status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return IRQ_HANDLED;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x08, &cbus1_status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return IRQ_HANDLED;
	}

	mutex_lock(&mhl->i2c_lock);
	ret = I2C_ReadByte(mhl->pdata, PAGE_CBUS, 0x1E, &cbus2_status);
	mutex_unlock(&mhl->i2c_lock);
	if (ret < 0) {
		PRINT_ERROR;
		return IRQ_HANDLED;
	}


	pr_info("### sii8332: %s int4(0x%x) tpi(0x%x) cbus1(0x%x) cbus2(0x%x)\n",
		__func__, int4status, tpi_status, cbus1_status, cbus2_status);

	local_op_status = mhl->pdata->status.op_status;

	/*pr_info("sii8332: %s():%d local_op_status = %d\n",
		__func__, __LINE__, local_op_status);*/

	if (local_op_status == NO_MHL_STATUS) {
		/*printk(KERN_INFO "sii8332: %s():%d NO_MHL_STATUS !\n",
		__func__, __LINE__);*/
		return IRQ_HANDLED;
	}
	switch (local_op_status) {
	case MHL_READY_RGND_DETECT:

		/*pr_info("sii8332: MHL_READY_RGND_DETECT\n");*/

		ret = mhl_rgnd_check_func(mhl);
		if (ret < 0)
			return IRQ_HANDLED;
		if (mhl->pdata->status.op_status == MHL_USB_CONNECTED) {

			/*RGNG is not 1K ohm*/
			pr_info("sii8332 MHL_USB_CONNECTED\n");
			mhl->pdata->status.linkmode = 0x03;
			mhl->pdata->status.op_status = NO_MHL_STATUS;
			mhl->hdmi_sink = false;

			mhl_onoff_ex(0);

			return IRQ_HANDLED;
		}
		break;

	case MHL_RX_CONNECTED:
		/*printk(KERN_INFO "sii8332: %s():%d MHL_RX_CONNECTED\n",
		__func__, __LINE__);*/
		ret = mhl_rx_connected_func(mhl);
		if (mhl->pdata->status.op_status == MHL_DISCOVERY_FAIL) {

			/* RGND is 1K, but discovery failed, then retry discovery*/
			pr_info("sii8332 MHL_DISCOVERY_FAIL\n");
			/*printk(KERN_INFO "sii8332: %s():%d MHL_DISCOVERY_FAIL\n",
			__func__, __LINE__);*/
			mhl->pdata->status.linkmode = 0x03;

			mhl->pdata->status.op_status = NO_MHL_STATUS;
			ret = mhl_init_func(mhl);
			if (ret < 0)
				PRINT_ERROR;
			mhl->hdmi_sink = false;
			ret = TX_GO2D3(mhl->pdata);
			if (ret < 0)
				PRINT_ERROR;

			/*mhl->pdata->status.op_status = MHL_READY_RGND_DETECT;*/

			if (waitqueue_active(&mhl->cbus_hanler_wq)) {
				/*printk(KERN_INFO "sii8332: %s():%d wake_up\n",
				__func__, __LINE__);*/
				wake_up(&mhl->cbus_hanler_wq);
			}
			/* irq_status(mhl);*/
			return IRQ_HANDLED;
		}

		if (mhl->pdata->status.op_status == MHL_DISCOVERY_ON) {
			/*printk(KERN_INFO "sii8332: %s():%d MHL_DISCOVERY_ON\n", __func__, __LINE__);*/
			if (waitqueue_active(&mhl->cbus_hanler_wq)) {
				/*printk(KERN_INFO "sii8332: %s():%d wake_up\n", __func__, __LINE__);*/
				wake_up(&mhl->cbus_hanler_wq);
			}
		}

		if (mhl->pdata->status.op_status == MHL_RX_CONNECTED) {
			/*printk(KERN_INFO "sii8332: %s():%d MHL_RX_CONNECTED\n", __func__, __LINE__);*/
		}

		break;

	case MHL_DISCOVERY_SUCCESS:
		/*printk(KERN_INFO " sii8332: %s():\n", __func__);*/
		/*printk(KERN_INFO "sii8332: %s():%d MHL_DISCOVERY_SUCCESS\n",
		__func__, __LINE__);*/
		ret = discovery_success(mhl);
		/*irq_status(mhl);*/
		if (ret < 0) {
			PRINT_ERROR;
			return IRQ_HANDLED;;
		}

		if (mhl->pdata->status.op_status == MHL_RX_DISCONNECTED) {
			printk(KERN_INFO "sii8332: MHL_RX_DISCONNECTED\n");

			mhl_onoff_ex(0);

			return IRQ_HANDLED;
		}

		break;

	default:
			break;
	}

	/*printk("%s end\n",__func__);*/
	return IRQ_HANDLED;
}

/****************************************************************************
*	name	=	simg72_probe
*	func	=
*	input	=	struct i2c_client *client, const struct i2c_device_id *id
*	output	=	None
*	return	=	ret
****************************************************************************/
static int __devinit simg72_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tx_page0 *page0;
	/*printk("[MHL] %s : START\n", __func__);*/
	/*printk(KERN_INFO"\n simg72_probe\n");*/

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;
	page0 = kzalloc(sizeof(struct tx_page0), GFP_KERNEL);
	if (!page0) {
		dev_err(&client->dev, " page0 failed to allocate driver data\n");
		return -ENOMEM;
	}
	page0->pdata = client->dev.platform_data;
	page0->pdata->simg72_tx_client = client;

	page0->pdata->irq = gpio_to_irq(page0->pdata->mhl_int);

	/*printk(KERN_INFO"irq=%d, mhl_int=%d\n", page0->pdata->irq,
				page0->pdata->mhl_int);*/
	if (!page0->pdata) {
		/*printk(KERN_INFO"\n SIMG72 no platform data\n");*/
		kfree(page0);
		return -EINVAL;
	}

	i2c_set_clientdata(client, page0);
	return 0;
}

/****************************************************************************
*	name	=	simg7A_probe
*	func	=
*	input	=	struct i2c_client *client, const struct i2c_device_id *id
*	output	=	None
*	return	=	ret
****************************************************************************/
static int __devinit simg7A_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tx_page1 *page1;
	/*printk("[MHL] %s : START\n", __func__);*/
	/*printk(KERN_INFO"\n simg7A_probe\n");*/
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;
	page1 = kzalloc(sizeof(struct tx_page1), GFP_KERNEL);
	if (!page1) {
		dev_err(&client->dev, " page1 failed to allocate driver data\n");
		return -ENOMEM;
	}
	page1->pdata = client->dev.platform_data;
	page1->pdata->simg7A_tx_client = client;

	if (!page1->pdata) {
		/*printk(KERN_INFO"\n SIMG7A no platform data\n");*/
		kfree(page1);
		return -EINVAL;
	}
	i2c_set_clientdata(client, page1);
	return 0;
}

/****************************************************************************
*	name	=	simg92_probe
*	func	=
*	input	=	struct i2c_client *client, const struct i2c_device_id *id
*	output	=	None
*	return	=	ret
****************************************************************************/
static int __devinit simg92_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tx_page2 *page2;
	/*printk("[MHL] %s : START\n", __func__);*/
	/*printk(KERN_INFO"\n simg92_probe\n");*/

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;
	page2 = kzalloc(sizeof(struct tx_page2), GFP_KERNEL);
	if (!page2) {
		dev_err(&client->dev, " page2 failed to allocate driver data\n");
		return -ENOMEM;
	}
	page2->pdata = client->dev.platform_data;
	page2->pdata->simg92_tx_client = client;

	if (!page2->pdata) {
		/*printk(KERN_INFO"\n SIMG92 no platform data\n");*/
		kfree(page2);
		return -EINVAL;
	}
	i2c_set_clientdata(client, page2);

	return 0;
}

/****************************************************************************
*	name	=	simg9A_probe
*	func	=
*	input	=	struct i2c_client *client, const struct i2c_device_id *id
*	output	=	None
*	return	=	ret
****************************************************************************/
static int __devinit simg9A_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tx_page3 *page3;
	/*printk("[MHL] %s : START\n", __func__);*/

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;
	page3 = kzalloc(sizeof(struct tx_page3), GFP_KERNEL);
	if (!page3) {
		dev_err(&client->dev, " page3 failed to allocate driver data\n");
		return -ENOMEM;
	}

	page3->pdata = client->dev.platform_data;
	page3->pdata->simg9A_tx_client = client;
	if (!page3->pdata) {
		/*printk(KERN_INFO"\n SIMG9A no platform data\n");*/
		kfree(page3);
		return -EINVAL;
	}
	i2c_set_clientdata(client, page3);

	return 0;
}

/****************************************************************************
*	name	=	simgC8_probe
*	func	=
*	input	=	struct i2c_client *client, const struct i2c_device_id *id
*	output	=	None
*	return	=	ret
****************************************************************************/
static int __devinit simgC8_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mhl_tx *mhl;
	int ret =0;

#ifdef CONFIG_SII9234_RCP
	struct input_dev *input;
#endif

	/*MHL_FUNC_START;
	printk(KERN_INFO "[MHL] %s : START\n", __func__);*/

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;
	mhl = kzalloc(sizeof(struct mhl_tx), GFP_KERNEL);
	if (!mhl) {
		dev_err(&client->dev, " cbus failed to allocate driver data\n");
		return -ENOMEM;
	}
	mhl->pdata = client->dev.platform_data;
	mhl->pdata->simgC8_tx_client = client;
	mhl->pdata->status.op_status = NO_MHL_STATUS;
	mhl->edid_access_done = false;
	if (!mhl->pdata) {
		/*printk(KERN_INFO"\n SIMGC8 no platform data\n");*/
		kfree(mhl);
		return -EINVAL;
	}

#ifdef CONFIG_SII9234_RCP
	input = input_allocate_device();
	if (!input) {
		dev_err(&client->dev, "failed to allocate input device.\n");
		return -ENOMEM;
	}
#endif

	mutex_init(&mhl->i2c_lock);
	mutex_init(&mhl->cbus_cmd_lock);
	mutex_init(&mhl->pdata->mhl_status_lock);
	mutex_init(&format_update_lock);
	mhl->client = client;
	/* mhl->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	mhl->early_suspend.suspend = mhl_early_suspend;
	mhl->early_suspend.resume = mhl_late_resume;
	register_early_suspend(&mhl->early_suspend); */
	i2c_set_clientdata(client, mhl);
	init_waitqueue_head(&mhl->cbus_hanler_wq);
	init_waitqueue_head(&mhl->avi_control_wq);

	/*INIT_WORK(&mhl->mhl_power_on, mhl_power_on_set);*/
	INIT_WORK(&mhl->cbus_cmd_work, cbus_cmd_thread);
	INIT_WORK(&mhl->avi_control_work, avi_control_thread);

	/* Request GPIO functions
	* VCLK5 SII8332 Master Clock - GPIO_PORT219
	* MIPI HDMI
	* Port request for Master clock for MHL */

	mhl->cbus_cmd_wqs = create_singlethread_workqueue("cbus_cmd_wqs");
	if (!mhl->cbus_cmd_wqs)
		return -ENOMEM;

	mhl->avi_control_wqs = create_singlethread_workqueue("avi_control_wqs");
	if (!mhl->avi_control_wqs)
		return -ENOMEM;

	ret = request_threaded_irq(mhl->pdata->irq, NULL, mhl_irq_thread,
						IRQF_TRIGGER_FALLING , "cbus_intr", mhl);

	if (ret < 0) {
		/*printk(KERN_INFO "cbus irq regist fail\n");*/
		kfree(mhl);
		return ret;
	}

#ifdef CONFIG_SII9234_RCP
	/* indicate that we generate key events */
	/*
	set_bit(EV_KEY, input->evbit);
	bitmap_fill(input->keybit, KEY_MAX);

	mhl->input_dev = input;
	input_set_drvdata(input, mhl);
	input->name = "sii8332_rcp";
	input->id.bustype = BUS_I2C;

	ret = input_register_device(input);
	if (ret < 0) {
		dev_err(&client->dev, "fail to register input device\n");
		return -ENOMEM;
	}*/
#endif

	queue_work(mhl->avi_control_wqs, &mhl->avi_control_work);

	gpio_set_value(mhl->pdata->mhl_en, 0);

	mhlglobal = mhl;

	return 0;
}

#ifdef CONFIG_PM
/****************************************************************************
*	name	=	mhl_suspend
*	func	=	Suspends mhl when HDMI cable is not plugged in
*	input	=	struct i2c_client *i2cClient
*	output	=	None
*	return	=	0
****************************************************************************/
static int mhl_suspend(struct device *dev)
/*static int mhl_suspend(struct mhl_tx *mhl)*/
{
#if 0
	struct i2c_client *client = to_i2c_client(dev);
	struct mhl_tx *mhl = i2c_get_clientdata(client);
	printk("%s irq : %d\n", __func__, mhl->pdata->irq);
#endif

	count = 0;
	resumed = 0;
#if 0
	disable_irq_wake(mhl->pdata->irq);
	mhl_onoff_ex(0);
#endif
	return 0;
}

/****************************************************************************
*	name	=	mhl_resume
*	func	=	Resumes mhl when HDMI cable is plugged in
*	input	=	struct i2c_client *client
*	output	=	None
*	return	=	0
****************************************************************************/
static int mhl_resume(struct device *dev)
/*static int mhl_resume(struct mhl_tx *mhl)*/
{
#if 0
	/* device shall be suspended only when HDMI is not connected */
	struct i2c_client *client = to_i2c_client(dev);
	struct mhl_tx *mhl = i2c_get_clientdata(client);
	printk("%s irq : %d\n", __func__, mhl->pdata->irq);
#endif
	resumed = 1;
#if 0
	mhl_onoff_ex(1);
	sii9234_reset();
	mhl->pdata->status.op_status = NO_MHL_STATUS;
	hw_reset(mhl->pdata);
	enable_irq_wake(mhl->pdata->irq);
	queue_work(mhl->avi_control_wqs, &mhl->avi_control_work);
#endif
	return 0;
}
#else
# define mhl_suspend NULL
# define mhl_resume  NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#if 0
static void mhl_early_suspend(struct early_suspend *h)
{
	struct mhl_tx *mhl;
	mhl = container_of(h, struct mhl_tx, early_suspend);
	/*mhl_suspend(&mhl->client->dev);*/
}

static void mhl_late_resume(struct early_suspend *h)
{
	struct mhl_tx *mhl;
	mhl = container_of(h, struct mhl_tx, early_suspend);
	/*mhl_resume(&mhl->client->dev);*/
}
#endif
#endif
/****************************************************************************
*	name	=	hdmi_open
*	func	=
*	input	=	void
*	output	=	None
*	return	=	ret
****************************************************************************/
static int hdmi_open(struct inode *inode, struct file *filp)
{
	/*printk("[MHL] %s : START\n", __func__);*/
	if (device_open)
		return -EBUSY;
	device_open++;
	return 0;
}
/****************************************************************************
*	name	=	hdmi_release
*	func	=
*	input	=	void
*	output	=	None
*	return	=	ret
****************************************************************************/
static int hdmi_release(struct inode *inode, struct file *filp)
{
	/*printk("[MHL] %s : START\n", __func__);*/
	if (device_open)
		device_open--;
	return 0;
}
/****************************************************************************
*	name	=	hdmi_get_hpd_state
*	func	=	get Switcher driver state
*	input	=	void
*	output	=	None
*	return	=	ret
****************************************************************************/
int hdmi_get_hpd_state(void)
{
	/*printk("[MHL] %s : START\n", __func__);*/
	return s_dev.state;
}
/****************************************************************************
*	name	=	show_state_hdmi
*	func	=	Settinf Switcher driver state
*	input	=	struct device *dev,struct device_attribute *attr,char *buf
*	output	=	None
*	return	=	ret
****************************************************************************/
static ssize_t show_state_hdmi(struct device *dev,	struct device_attribute *attr, char *buf)
{
	int ret  = 0;
	/*printk("[MHL] %s : START\n", __func__);*/
	ret = sprintf(buf, "%d", mhl_plugedin_state);
	return ret;
}
/****************************************************************************
*	name	=	show_info
*	func	=	Settinf Switcher driver info
*	input	=	struct device *dev,struct device_attribute *attr,char *buf
*	output	=	None
*	return	=	ret
****************************************************************************/
static int show_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	int video_output_format;
	int ret = 0;

	char output_format[] = "[video_output_format]\n";
	/*char formatNumber[10];*/

	/*When HDMI is not plugged in, the video_output_format is set to 0*/
	if (mhl_plugedin_state == 0) {
		video_output_format = 0;
		/*sprintf(formatNumber,"%d",video_output_format);
		strcat(output_format,formatNumber);
		ret = sprintf(buf,"%s",output_format);*/
		ret = sprintf(buf, "%s%d", output_format, video_output_format);
	} else {
		/*printk("%s outputformat = %d\n",__func__,outputformat);*/
		video_output_format = outputformat;
		/*sprintf(formatNumber,"%d",video_output_format);

		strcat(output_format,formatNumber);
		ret = sprintf(buf,"%s",output_format);*/
		ret = sprintf(buf, "%s%d", output_format, video_output_format);
	}
	return ret;
}

static int __devexit simg72_remove(struct i2c_client *client)
{
	i2c_del_driver(&simg72_driver);
	return 0;
}

static int __devexit simg7A_remove(struct i2c_client *client)
{
	i2c_del_driver(&simg7A_driver);
	return 0;
}

static int __devexit simg92_remove(struct i2c_client *client)
{
	i2c_del_driver(&simg92_driver);
	return 0;
}

static int __devexit simg9A_remove(struct i2c_client *client)
{
	i2c_del_driver(&simg9A_driver);
	return 0;
}

static int __devexit simgC8_remove(struct i2c_client *client)
{
	/* Free the Port for CLOCK */
	/* struct mhl_tx *mhl = i2c_get_clientdata(client);*/
	/* unregister_early_suspend(&mhl->early_suspend);*/
	gpio_free(GPIO_PORT219);
	i2c_del_driver(&simgC8_driver);
	return 0;
}

/****************************************************************************
*	name	=	simg_init
*	func	=
*	input	=	void
*	output	=	None
*	return	=	ret
****************************************************************************/
static int __init simg_init(void)
{
	int ret=0;
	/*printk("[MHL] %s : START\n", __func__);*/
	ret = i2c_add_driver(&simg72_driver);
	if (ret != 0)
		goto err_exit1;

	ret = i2c_add_driver(&simg7A_driver);
	if (ret != 0)
		goto err_exit2;

	ret = i2c_add_driver(&simg92_driver);
	if (ret != 0)
		goto err_exit3;

	ret = i2c_add_driver(&simg9A_driver);
	if (ret != 0)
		goto err_exit4;

	ret = i2c_add_driver(&simgC8_driver);
	if (ret != 0)
		goto err_exit7;

	ret = misc_register(&hdmi_miscdev);
	if (ret)
		goto hdmi_init_out;

	ret = switch_dev_register(&s_dev);
	if (ret)
		goto hdmi_init_out;

#ifdef CONFIG_BCM_HDMI_DET_SWITCH
	ch_hdmi_audio_hpd_switch.name = "ch_hdmi_audio";

	ret  = switch_dev_register(&ch_hdmi_audio_hpd_switch);
	if (ret  < 0) {
		printk(KERN_ERR "HDMI: Device switch create failed\n");
		goto hdmi_init_out;
	}

	/* Assign initial state. */
	switch_set_state(&ch_hdmi_audio_hpd_switch, 0);
#endif



	hdmidev = hdmi_miscdev.this_device;
	switchdev = s_dev.dev;

	if (device_create_file(hdmidev, &dev_attr_state))
		dev_info(hdmidev, "Unable to create state attribute\n");
	if (device_create_file(switchdev, &dev_attr_info))
		dev_info(switchdev, "Unable to create switch info attribute\n");

	return 0;

err_exit7:
	/*printk(KERN_INFO"simgC8_driver fail\n");*/
	i2c_del_driver(&simgC8_driver);
err_exit4:
	/*printk(KERN_INFO"simg9A_driver fail\n");*/
	i2c_del_driver(&simg9A_driver);
err_exit3:
	/*printk(KERN_INFO"simg92_driver fail\n");*/
	i2c_del_driver(&simg92_driver);
err_exit2:
	/*printk(KERN_INFO"simg7A_driver fail\n");*/
	i2c_del_driver(&simg7A_driver);
err_exit1:
	/*printk(KERN_INFO"simg72_driver fail\n");*/
	i2c_del_driver(&simg72_driver);
hdmi_init_out:
	return ret;
}
/****************************************************************************
*	name	=	simg_exit
*	func	=
*	input	=	void
*	output	=	None
*	return	=	ret
****************************************************************************/
static void __exit simg_exit(void)
{
	i2c_del_driver(&simg72_driver);
	i2c_del_driver(&simg7A_driver);
	i2c_del_driver(&simg92_driver);
	i2c_del_driver(&simg9A_driver);
	i2c_del_driver(&simgC8_driver);
	device_remove_file(hdmidev, &dev_attr_state);
	device_remove_file(switchdev, &dev_attr_info);
	misc_deregister(&hdmi_miscdev);
	switch_dev_unregister(&s_dev);
}

/*SSG GED changed*/
subsys_initcall(simg_init);
module_exit(simg_exit);

MODULE_DESCRIPTION("Silicon Image MHL Transmitter driver");
MODULE_AUTHOR("Daniel(Philju) Lee <daniel.lee@siliconimage.com>");
MODULE_LICENSE("GPL");
