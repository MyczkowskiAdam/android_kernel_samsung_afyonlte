/*
 * Driver for Samsung S5K4ECGX  Camera
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* for debug */
/* #undef DEBUG */
#define DEBUG 1
/* #define DEBUG */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/gpio.h>
#include <linux/clk.h>

#include <media/soc_camera.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-ctrls.h>
#include <media/sh_mobile_csi2.h>

#include <media/sh_mobile_rcu.h>
int flash_check =0;
static ssize_t subcamtype_S5K4ECGX_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *sensorname = "S5K4ECGX";
	return sprintf(buf, "%s\n", sensorname);
}

static ssize_t subcamfw_S5K4ECGX_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *sensorfw = "S5K4ECGX N";
	return sprintf(buf, "%s\n", sensorfw);
}

static ssize_t maincamflash_S5K4ECGX_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	if ((0 >= count) || !buf)
		return 0;
	if (buf[0] == '0')
	{
		sh_mobile_rcu_flash(0);
		flash_check = 0;
	}
	else
	{
		sh_mobile_rcu_flash(1);
		flash_check = 1;
	}
	return count;
}

static ssize_t subvendorid_S5K4ECGX_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_MACH_AFYONLTE
	char *vendorid = "0x0305";
#else
	char *vendorid = "0x0302";
#endif
	return sprintf(buf, "%s\n", vendorid);
}

static DEVICE_ATTR(rear_camtype, 0644, subcamtype_S5K4ECGX_show, NULL);
static DEVICE_ATTR(rear_camfw, 0644, subcamfw_S5K4ECGX_show, NULL);
static DEVICE_ATTR(rear_flash, 0664, NULL, maincamflash_S5K4ECGX_store);
static DEVICE_ATTR(rear_vendorid, 0644, subvendorid_S5K4ECGX_show, NULL);

struct S5K4ECGX_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

struct S5K4ECGX {
	struct v4l2_subdev		subdev;
	struct v4l2_ctrl_handler hdl;
	const struct S5K4ECGX_datafmt	*fmt;
	unsigned int			width;
	unsigned int			height;
};

static const struct S5K4ECGX_datafmt S5K4ECGX_colour_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SGBRG10_1X10,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SGRBG10_1X10,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SRGGB10_1X10,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_UYVY8_2X8,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_VYUY8_2X8,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_YUYV8_2X8,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_YVYU8_2X8,	V4L2_COLORSPACE_SRGB},
};

static struct S5K4ECGX *to_S5K4ECGX(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client),
					struct S5K4ECGX, subdev);
}

/* Find a data format by a pixel code in an array */
static const struct S5K4ECGX_datafmt *S5K4ECGX_find_datafmt(
					enum v4l2_mbus_pixelcode code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(S5K4ECGX_colour_fmts); i++)
		if (S5K4ECGX_colour_fmts[i].code == code)
			return S5K4ECGX_colour_fmts + i;

	return NULL;
}

/* select nearest higher resolution for capture */
static void S5K4ECGX_res_roundup(u32 *width, u32 *height)
{
	int i;
	int res_x[] = { 640, 1280, 1280, 2560 };
	int res_y[] = { 480, 720, 960, 1920 };

	for (i = 0; i < ARRAY_SIZE(res_x); i++) {
		if (res_x[i] >= *width && res_y[i] >= *height) {
			*width = res_x[i];
			*height = res_y[i];
			return;
		}
	}

	*width = res_x[3];
	*height = res_y[3];
}

static int S5K4ECGX_try_fmt(struct v4l2_subdev *sd,
	       struct v4l2_mbus_framefmt *mf)
{
	const struct S5K4ECGX_datafmt *fmt = S5K4ECGX_find_datafmt(mf->code);

	dev_dbg(sd->v4l2_dev->dev, "%s(%u)\n", __func__, mf->code);

	if (!fmt) {
		mf->code	= S5K4ECGX_colour_fmts[0].code;
		mf->colorspace	= S5K4ECGX_colour_fmts[0].colorspace;
	}

	dev_dbg(sd->v4l2_dev->dev, "in: mf->width = %d, height = %d\n",
		mf->width, mf->height);
	S5K4ECGX_res_roundup(&mf->width, &mf->height);
	dev_dbg(sd->v4l2_dev->dev, "out: mf->width = %d, height = %d\n",
		mf->width, mf->height);
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int S5K4ECGX_s_fmt(struct v4l2_subdev *sd,
	     struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct S5K4ECGX *priv = to_S5K4ECGX(client);

	dev_dbg(sd->v4l2_dev->dev, "%s(%u)\n", __func__, mf->code);

	/* MIPI CSI could have changed the format, double-check */
	if (!S5K4ECGX_find_datafmt(mf->code)) {
		dev_err(sd->v4l2_dev->dev, "%s -EINVAL\n", __func__);
		return -EINVAL;
	}

	S5K4ECGX_try_fmt(sd, mf);

	priv->fmt	= S5K4ECGX_find_datafmt(mf->code);
	priv->width	= mf->width;
	priv->height	= mf->height;

	return 0;
}

static int S5K4ECGX_g_fmt(struct v4l2_subdev *sd,
	     struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct S5K4ECGX *priv = to_S5K4ECGX(client);

	const struct S5K4ECGX_datafmt *fmt = priv->fmt;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->width	= priv->width;
	mf->height	= priv->height;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int S5K4ECGX_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct S5K4ECGX *priv = to_S5K4ECGX(client);
	struct v4l2_rect *rect = &a->c;

	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	rect->top	= 0;
	rect->left	= 0;
	rect->width	= priv->width;
	rect->height	= priv->height;

	return 0;
}

static int S5K4ECGX_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct S5K4ECGX *priv = to_S5K4ECGX(client);

	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= priv->width;
	a->bounds.height		= priv->height;
	dev_dbg(&client->dev, "crop: width = %d, height = %d\n",
		a->bounds.width, a->bounds.height);
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int S5K4ECGX_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if ((unsigned int)index >= ARRAY_SIZE(S5K4ECGX_colour_fmts))
		return -EINVAL;

	*code = S5K4ECGX_colour_fmts[index].code;
	return 0;
}

static int S5K4ECGX_g_chip_ident(struct v4l2_subdev *sd,
		    struct v4l2_dbg_chip_ident *id)
{
	/* check i2c device */
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct i2c_msg msg[2];
	unsigned char send_buf[2];
	unsigned char rcv_buf[2];
	int loop = 0;
	int ret = 0;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = (char *) send_buf;
	/* FW Sensor ID Support */
	send_buf[0] = 0x01;
	send_buf[1] = 0x5A;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags & I2C_M_TEN;
	msg[1].flags |= I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = rcv_buf;

	for (loop = 0; loop < 5; loop++) {
		ret = i2c_transfer(client->adapter, msg, 2);
		if (0 <= ret)
			break;
	}
	if (0 > ret) {
		dev_err(&client->dev, "%s :Read Error(%d)\n", __func__, ret);
		id->ident = V4L2_IDENT_NONE;
	} else {
		dev_dbg(&client->dev, "%s :Read OK\n", __func__);
		id->ident = V4L2_IDENT_S5K4ECGX;
	}
	id->revision = 0;

	return 0;
}

/* Request bus settings on camera side */
static int S5K4ECGX_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
/*	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);*/

	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_2_LANE |
		V4L2_MBUS_CSI2_CHANNEL_0 |
		V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;

	return 0;
}

/* Alter bus settings on camera side */
static int S5K4ECGX_s_mbus_config(struct v4l2_subdev *sd,
				const struct v4l2_mbus_config *cfg)
{
	return 0;
}

static int S5K4ECGX_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_GET_TUNING:
#ifdef CONFIG_SOC_CAMERA_S5K4ECGX_TUNING
		ctrl->value = 1;
#else
		ctrl->value = 0;
#endif
		/* no break */
	default:
		return 0;
	}
	return -ENOIOCTLCMD;
}

static struct v4l2_subdev_video_ops S5K4ECGX_subdev_video_ops = {
	.s_mbus_fmt	= S5K4ECGX_s_fmt,
	.g_mbus_fmt	= S5K4ECGX_g_fmt,
	.try_mbus_fmt	= S5K4ECGX_try_fmt,
	.enum_mbus_fmt	= S5K4ECGX_enum_fmt,
	.g_crop		= S5K4ECGX_g_crop,
	.cropcap	= S5K4ECGX_cropcap,
	.g_mbus_config	= S5K4ECGX_g_mbus_config,
	.s_mbus_config	= S5K4ECGX_s_mbus_config,
};

static struct v4l2_subdev_core_ops S5K4ECGX_subdev_core_ops = {
	.g_chip_ident	= S5K4ECGX_g_chip_ident,
	.g_ctrl		= S5K4ECGX_g_ctrl,
};

static struct v4l2_subdev_ops S5K4ECGX_subdev_ops = {
	.core	= &S5K4ECGX_subdev_core_ops,
	.video	= &S5K4ECGX_subdev_video_ops,
};

static int S5K4ECGX_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

struct v4l2_ctrl_ops S5K4ECGX_ctrl_ops = {
	.s_ctrl = S5K4ECGX_s_ctrl,
};

static int S5K4ECGX_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct S5K4ECGX *priv;
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);
	int ret = 0;

	dev_dbg(&client->dev, "%s():\n", __func__);

	if (!icl) {
		dev_err(&client->dev, "S5K4ECGX: missing platform data!\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev,
			"S5K4ECGX: Failed to allocate memory for private data!\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &S5K4ECGX_subdev_ops);
	v4l2_ctrl_handler_init(&priv->hdl, 4);
	v4l2_ctrl_new_std(&priv->hdl, &S5K4ECGX_ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&priv->hdl, &S5K4ECGX_ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&priv->hdl, &S5K4ECGX_ctrl_ops,
			V4L2_CID_GAIN, 0, 127, 1, 66);
	v4l2_ctrl_new_std(&priv->hdl, &S5K4ECGX_ctrl_ops,
			V4L2_CID_AUTO_WHITE_BALANCE, 0, 1, 1, 1);
	priv->subdev.ctrl_handler = &priv->hdl;
	if (priv->hdl.error) {
		int err = priv->hdl.error;

		kfree(priv);
		return err;
	}

	priv->width	= 640;
	priv->height	= 480;
	priv->fmt	= &S5K4ECGX_colour_fmts[0];
	ret = v4l2_ctrl_handler_setup(&priv->hdl);
	if (0 > ret) {
		dev_err(&client->dev,
			"S5K4ECGX: v4l2_ctrl_handler_setup Error(%d)\n", ret);
		kfree(priv);
		return ret;
	}

	if (cam_class_init == false) {
		dev_dbg(&client->dev,
			"Start create class for factory test mode !\n");
		camera_class = class_create(THIS_MODULE, "camera");
		cam_class_init = true;
	}

	if (camera_class) {
		dev_dbg(&client->dev, "Create main camera device !\n");

		sec_main_cam_dev = device_create(camera_class,
						NULL, 0, NULL, "rear");
		if (IS_ERR(sec_main_cam_dev)) {
			dev_err(&client->dev,
				"Failed to create device"
				"(sec_main_cam_dev)!\n");
		}

		if (device_create_file(sec_main_cam_dev,
					&dev_attr_rear_camtype) < 0) {
			dev_err(&client->dev,
				"failed to create main camera "
				"device file, %s\n",
				dev_attr_rear_camtype.attr.name);
		}
		if (device_create_file(sec_main_cam_dev,
					&dev_attr_rear_camfw) < 0) {
			dev_err(&client->dev,
				"failed to create main camera "
				"device file, %s\n",
				dev_attr_rear_camfw.attr.name);
		}
		if (device_create_file(sec_main_cam_dev,
					&dev_attr_rear_flash) < 0) {
			dev_err(&client->dev,
				"failed to create main camera "
				"device file, %s\n",
				dev_attr_rear_flash.attr.name);
		}
		if (device_create_file(sec_main_cam_dev,
					&dev_attr_rear_vendorid) < 0) {
			dev_err(&client->dev,
				"failed to create main camera "
				"device file, %s\n",
				dev_attr_rear_vendorid.attr.name);
		}
	}

	return ret;
}

static int S5K4ECGX_remove(struct i2c_client *client)
{
	struct S5K4ECGX *priv = to_S5K4ECGX(client);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);

	v4l2_device_unregister_subdev(&priv->subdev);
	if (icl->free_bus)
		icl->free_bus(icl);
	v4l2_ctrl_handler_free(&priv->hdl);
	kfree(priv);

	return 0;
}

static const struct i2c_device_id S5K4ECGX_id[] = {
	{ "S5K4ECGX", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, S5K4ECGX_id);

static struct i2c_driver S5K4ECGX_i2c_driver = {
	.driver = {
		.name = "S5K4ECGX",
	},
	.probe		= S5K4ECGX_probe,
	.remove		= S5K4ECGX_remove,
	.id_table	= S5K4ECGX_id,
};

module_i2c_driver(S5K4ECGX_i2c_driver);

MODULE_DESCRIPTION("Samsung S5K4ECGX Camera driver");
MODULE_AUTHOR("Renesas Mobile Corp.");
MODULE_LICENSE("GPL v2");
