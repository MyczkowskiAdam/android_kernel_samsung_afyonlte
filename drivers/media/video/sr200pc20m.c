/*
 * Driver for Samsung SR200PC20M VGA Camera
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

static ssize_t maincamtype_SR200PC20M_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *sensorname = "SR200PC20M";
	return sprintf(buf, "%s\n", sensorname);
}

static ssize_t maincamfw_SR200PC20M_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char *sensorfw = "N";
	return sprintf(buf, "%s\n", sensorfw);
}

static DEVICE_ATTR(rear_camtype, 0644, maincamtype_SR200PC20M_show, NULL);
static DEVICE_ATTR(rear_camfw, 0644, maincamfw_SR200PC20M_show, NULL);

struct SR200PC20M_datafmt {
	enum v4l2_mbus_pixelcode	code;
	enum v4l2_colorspace		colorspace;
};

struct SR200PC20M {
	struct v4l2_subdev		subdev;
	struct v4l2_ctrl_handler hdl;
	const struct SR200PC20M_datafmt	*fmt;
	unsigned int			width;
	unsigned int			height;
};

static const struct SR200PC20M_datafmt SR200PC20M_colour_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SGBRG10_1X10,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SGRBG10_1X10,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SRGGB10_1X10,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_UYVY8_2X8,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_VYUY8_2X8,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_YUYV8_2X8,	V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_YVYU8_2X8,	V4L2_COLORSPACE_SRGB},
};

static inline int sr200pc20m_read(struct i2c_client *client,
	unsigned char subaddr, unsigned char *data)
{
	unsigned char buf[2];
	int err = 0;
	struct i2c_msg msg = {client->addr, 0, 1, buf};

	buf[0] = subaddr;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
		dev_err(&client->dev, "%s: %d register read fail\n",
							__func__, __LINE__);

	msg.flags = I2C_M_RD;
	msg.len = 1;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (unlikely(err < 0))
		dev_err(&client->dev, "%s: %d register read fail\n",
							__func__, __LINE__);

/*	dev_err(&client->dev, "\n\n\n%X %X\n\n\n", buf[0], buf[1]); */

	*data = buf[0];

	return err;
}

static inline int sr200pc20m_write(struct i2c_client *client,
						unsigned short packet)
{
	unsigned char buf[2];

	int err = 0;
	int retry_count = 5;

	struct i2c_msg msg = {
		.addr   = client->addr,
		.flags  = 0,
		.buf    = buf,
		.len    = 2,
	};

	if (!client->adapter) {
		dev_err(&client->dev,
			"%s: can't search i2c client adapter\n", __func__);
		return -EIO;
	}

	while (retry_count--) {
		*(unsigned long *)buf = cpu_to_be16(packet);
		err = i2c_transfer(client->adapter, &msg, 1);
		if (likely(err == 1))
			break;
		mdelay(10);
	}

	if (unlikely(err < 0)) {
		dev_err(&client->dev, "%s: 0x%08x write failed err = %d\n",
					__func__, (unsigned int)packet, err);
		return err;
	}

	return (err != 1) ? -1 : 0;
}

static struct SR200PC20M *to_SR200PC20M(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client),
				struct SR200PC20M, subdev);
}

/* Find a data format by a pixel code in an array */
static const struct SR200PC20M_datafmt *SR200PC20M_find_datafmt(
					enum v4l2_mbus_pixelcode code)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(SR200PC20M_colour_fmts); i++)
		if (SR200PC20M_colour_fmts[i].code == code)
			return SR200PC20M_colour_fmts + i;

	return NULL;
}

/* select nearest higher resolution for capture */
static void SR200PC20M_res_roundup(u32 *width, u32 *height)
{
	int i;
	int res_x[] = { 320, 640};
	int res_y[] = { 240, 480};

	for (i = 0; i < ARRAY_SIZE(res_x); i++) {
		if (res_x[i] >= *width && res_y[i] >= *height) {
			*width = res_x[i];
			*height = res_y[i];
			return;
		}
	}

	*width = res_x[1];
	*height = res_y[1];
}

static int SR200PC20M_try_fmt(struct v4l2_subdev *sd,
	       struct v4l2_mbus_framefmt *mf)
{
	const struct SR200PC20M_datafmt *fmt = SR200PC20M_find_datafmt(mf->code);

	dev_dbg(sd->v4l2_dev->dev, "%s(%u)\n", __func__, mf->code);

	if (!fmt) {
		mf->code	= SR200PC20M_colour_fmts[0].code;
		mf->colorspace	= SR200PC20M_colour_fmts[0].colorspace;
	}

	dev_dbg(sd->v4l2_dev->dev, "in: mf->width = %d, height = %d\n",
		mf->width, mf->height);
	SR200PC20M_res_roundup(&mf->width, &mf->height);
	dev_dbg(sd->v4l2_dev->dev, "out: mf->width = %d, height = %d\n",
		mf->width, mf->height);
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int SR200PC20M_s_fmt(struct v4l2_subdev *sd,
	     struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct SR200PC20M *priv = to_SR200PC20M(client);

	dev_dbg(sd->v4l2_dev->dev, "%s(%u)\n", __func__, mf->code);

	/* MIPI CSI could have changed the format, double-check */
	if (!SR200PC20M_find_datafmt(mf->code)) {
		dev_err(sd->v4l2_dev->dev, "%s -EINVAL\n", __func__);
		return -EINVAL;
	}

	SR200PC20M_try_fmt(sd, mf);

	priv->fmt	= SR200PC20M_find_datafmt(mf->code);
	priv->width	= mf->width;
	priv->height	= mf->height;

	return 0;
}

static int SR200PC20M_g_fmt(struct v4l2_subdev *sd,
	     struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct SR200PC20M *priv = to_SR200PC20M(client);

	const struct SR200PC20M_datafmt *fmt = priv->fmt;

	mf->code	= fmt->code;
	mf->colorspace	= fmt->colorspace;
	mf->width	= priv->width;
	mf->height	= priv->height;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int SR200PC20M_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct SR200PC20M *priv = to_SR200PC20M(client);
	struct v4l2_rect *rect = &a->c;

	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	rect->top	= 0;
	rect->left	= 0;
	rect->width	= priv->width;
	rect->height	= priv->height;

	return 0;
}

static int SR200PC20M_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct SR200PC20M *priv = to_SR200PC20M(client);

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

static int SR200PC20M_enum_fmt(struct v4l2_subdev *sd, unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if ((unsigned int)index >= ARRAY_SIZE(SR200PC20M_colour_fmts))
		return -EINVAL;

	*code = SR200PC20M_colour_fmts[index].code;
	return 0;
}

static int SR200PC20M_g_chip_ident(struct v4l2_subdev *sd,
		    struct v4l2_dbg_chip_ident *id)
{
	/* check i2c device */
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char rcv_buf[1];
	int ret = 0;

	ret = sr200pc20m_write(client, 0x0300); /* Page 0 */
	if (0 > ret)
		dev_err(&client->dev, "%s :Write Error(%d)\n", __func__, ret);
	ret = sr200pc20m_read(client, 0x04, rcv_buf);
	/* device id = P0(0x00) address 0x04 = 0xB8 */

	if (0 > ret) {
		dev_err(&client->dev, "%s :Read Error(%d)\n", __func__, ret);
		id->ident = V4L2_IDENT_NONE;
	} else {
		dev_dbg(&client->dev, "%s :SR200PC20M OK(%02x)\n", __func__,
			rcv_buf[0]);
		id->ident = V4L2_IDENT_SR200PC20M;
	}
	id->revision = 0;

	return 0;
}

static int SR200PC20M_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_GET_TUNING:
#ifdef CONFIG_SOC_CAMERA_SR200PC20M_TUNING
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

/* Request bus settings on camera side */
static int SR200PC20M_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
/*	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);*/

	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_1_LANE |
		V4L2_MBUS_CSI2_CHANNEL_0 |
		V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;

	return 0;
}

/* Alter bus settings on camera side */
static int SR200PC20M_s_mbus_config(struct v4l2_subdev *sd,
				const struct v4l2_mbus_config *cfg)
{
	return 0;
}

static struct v4l2_subdev_video_ops SR200PC20M_subdev_video_ops = {
	.s_mbus_fmt	= SR200PC20M_s_fmt,
	.g_mbus_fmt	= SR200PC20M_g_fmt,
	.try_mbus_fmt	= SR200PC20M_try_fmt,
	.enum_mbus_fmt	= SR200PC20M_enum_fmt,
	.g_crop		= SR200PC20M_g_crop,
	.cropcap	= SR200PC20M_cropcap,
	.g_mbus_config	= SR200PC20M_g_mbus_config,
	.s_mbus_config	= SR200PC20M_s_mbus_config,
};

static struct v4l2_subdev_core_ops SR200PC20M_subdev_core_ops = {
	.g_chip_ident	= SR200PC20M_g_chip_ident,
	.g_ctrl		= SR200PC20M_g_ctrl,
};

static struct v4l2_subdev_ops SR200PC20M_subdev_ops = {
	.core	= &SR200PC20M_subdev_core_ops,
	.video	= &SR200PC20M_subdev_video_ops,
};

static int SR200PC20M_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

struct v4l2_ctrl_ops SR200PC20M_ctrl_ops = {
	.s_ctrl = SR200PC20M_s_ctrl,
};

static int SR200PC20M_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct SR200PC20M *priv;
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);
	int ret = 0;

	dev_dbg(&client->dev, "%s():\n", __func__);

	if (!icl) {
		dev_err(&client->dev, "SR200PC20M: missing platform data!\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&client->dev,
			"SR200PC20M: Failed to allocate memory for private data!\n");
		return -ENOMEM;
	}

	v4l2_i2c_subdev_init(&priv->subdev, client, &SR200PC20M_subdev_ops);
	v4l2_ctrl_handler_init(&priv->hdl, 4);
	v4l2_ctrl_new_std(&priv->hdl, &SR200PC20M_ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&priv->hdl, &SR200PC20M_ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&priv->hdl, &SR200PC20M_ctrl_ops,
			V4L2_CID_GAIN, 0, 127, 1, 66);
	v4l2_ctrl_new_std(&priv->hdl, &SR200PC20M_ctrl_ops,
			V4L2_CID_AUTO_WHITE_BALANCE, 0, 1, 1, 1);
	priv->subdev.ctrl_handler = &priv->hdl;
	if (priv->hdl.error) {
		int err = priv->hdl.error;

		kfree(priv);
		return err;
	}

	priv->width	= 640;
	priv->height	= 480;
	priv->fmt	= &SR200PC20M_colour_fmts[0];
	ret = v4l2_ctrl_handler_setup(&priv->hdl);
	if (0 > ret) {
		dev_err(&client->dev, "v4l2_ctrl_handler_setup Error(%d)\n",
			ret);
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
		dev_dbg(&client->dev, "Create Sub camera device !\n");

		sec_sub_cam_dev = device_create(camera_class,
						NULL, 0, NULL, "front");
		if (IS_ERR(sec_sub_cam_dev)) {
			dev_err(&client->dev,
				"Failed to create device(sec_sub_cam_dev)!\n");
		}

		if (device_create_file(sec_sub_cam_dev,
					&dev_attr_rear_camtype) < 0) {
			dev_err(&client->dev,
				"failed to create sub camera device file, %s\n",
				dev_attr_rear_camtype.attr.name);
		}
		if (device_create_file(sec_sub_cam_dev,
					&dev_attr_rear_camfw) < 0) {
			dev_err(&client->dev,
				"failed to create sub camera device file, %s\n",
				dev_attr_rear_camfw.attr.name);
		}
	}

	return ret;
}

static int SR200PC20M_remove(struct i2c_client *client)
{
	struct SR200PC20M *priv = to_SR200PC20M(client);
	struct soc_camera_link *icl = soc_camera_i2c_to_link(client);

	v4l2_device_unregister_subdev(&priv->subdev);
	if (icl->free_bus)
		icl->free_bus(icl);
	v4l2_ctrl_handler_free(&priv->hdl);
	kfree(priv);

	return 0;
}

static const struct i2c_device_id SR200PC20M_id[] = {
	{ "SR200PC20M", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, SR200PC20M_id);

static struct i2c_driver SR200PC20M_i2c_driver = {
	.driver = {
		.name = "SR200PC20M",
	},
	.probe		= SR200PC20M_probe,
	.remove		= SR200PC20M_remove,
	.id_table	= SR200PC20M_id,
};

module_i2c_driver(SR200PC20M_i2c_driver);

MODULE_DESCRIPTION("Samsung SR200PC20M Camera driver");
MODULE_AUTHOR("Renesas Mobile Corp.");
MODULE_LICENSE("GPL v2");

