/*
 * Driver for the SH-Mobile MIPI CSI-2 unit
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
 *
 * Copyright (C) 2010, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/sh_clk.h>

#include <media/sh_mobile_ceu.h>
#include <media/sh_mobile_csi2.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>

#define CSI2_POWAREA_MNG_ENABLE

#ifdef CSI2_POWAREA_MNG_ENABLE
#include <rtapi/system_pwmng.h>
#endif

#define RTAPI_CAMERA_ENABLE

#ifdef RTAPI_CAMERA_ENABLE
#include <rtapi/camera.h>
#endif

#define SH_CSI2_DEBUG		(0)
#define SH_CSI2_ERROR_RESET	(0)

#define SH_CSI2_TREF		(0x00)
#define SH_CSI2_SRST		(0x04)
#define SH_CSI2_PHYCNT		(0x08)
#define SH_CSI2_CHKSUM		(0x0C)
#define SH_CSI2_VCDT		(0x10)
#define SH_CSI2_PHYCNT2		(0x48)
#define SH_CSI2_PHYCNT3		(0x20)
#define SH_CSI2_INTSTATE	(0x38)
#define SH_CSI2_INTEN		(0x30)
#define SH_CSI2_INTEN_ALL	(0x5F5F)
#define SH_CSI2_INT_ERROR	(SH_CSI2_INTEN_ALL)
#define SH_CSI2_INT_RESET_ERROR	(0x25F)
#define SH_CSI2_OUT		(0x24)

struct sh_csi2 {
	struct v4l2_subdev		subdev;
	struct list_head		list;
	unsigned int			irq;
	unsigned long			mipi_flags;
	void __iomem			*base;
	void __iomem			*intcs_base;
	struct platform_device		*pdev;
	struct sh_csi2_client_config	*client;
	int				strm_on;
	spinlock_t			lock;
	unsigned int			err_cnt;
	int				power;
	int				first_power;
#if SH_CSI2_DEBUG
	int				vd_s_cnt;
	int				vd_e_cnt;
	int				shp_cnt;
	int				lnp_cnt;
#endif /* SH_CSI2_DEBUG */
};

static void sh_csi2_hwinit(struct sh_csi2 *priv);
static void sh_csi2_l_power(struct sh_csi2 *priv, int power_on);

static int sh_csi2_stream(struct sh_csi2 *priv, int enable)
{
	volatile u32 tmp = 0;

	if (0 != enable) {
		/* stream ON */
		tmp = ioread32(priv->base + SH_CSI2_INTSTATE);
		iowrite32(tmp, priv->base + SH_CSI2_INTSTATE);
		if (tmp & 0x53)
			printk(KERN_ALERT "CSI Error(stream)(0x%08X)\n", tmp);

		if (priv->client->phy == SH_CSI2_PHY_MAIN)
			tmp = 0;
		else
			tmp = (1 << 30);

		iowrite32(tmp, priv->base + SH_CSI2_PHYCNT3);

		iowrite32(0, priv->base + SH_CSI2_INTEN);
#if SH_CSI2_DEBUG
		priv->vd_s_cnt = 0;
		priv->vd_e_cnt = 0;
		priv->shp_cnt = 0;
		priv->lnp_cnt = 0;
#endif /* SH_CSI2_DEBUG */

		tmp = 0x10;
		if (priv->client->lanes & 0xF)
			tmp |= priv->client->lanes & 0xF;
		else
			/* Default - both lanes */
			tmp |= 3;

		iowrite32(tmp, priv->base + SH_CSI2_PHYCNT);
	} else {
		/* stream OFF */
		iowrite32(0, priv->base + SH_CSI2_INTEN);

		iowrite32(0x00000000, priv->base + SH_CSI2_PHYCNT);
	}

	return 0;
}

static int sh_csi2_try_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct sh_csi2 *priv = container_of(sd, struct sh_csi2, subdev);
	struct sh_csi2_pdata *pdata = priv->pdev->dev.platform_data;

	if (mf->width > 8188)
		mf->width = 8188;
	else if (mf->width & 1)
		mf->width &= ~1;

	switch (pdata->type) {
	case SH_CSI2C:
		switch (mf->code) {
		case V4L2_MBUS_FMT_UYVY8_2X8:		/* YUV422 */
		case V4L2_MBUS_FMT_YUYV8_1_5X8:		/* YUV420 */
		case V4L2_MBUS_FMT_Y8_1X8:		/* RAW8 */
		case V4L2_MBUS_FMT_SBGGR8_1X8:
		case V4L2_MBUS_FMT_SGRBG8_1X8:
		case V4L2_MBUS_FMT_SGBRG8_1X8:
		case V4L2_MBUS_FMT_SRGGB8_1X8:
		case V4L2_MBUS_FMT_SBGGR10_1X10:
		case V4L2_MBUS_FMT_SGBRG10_1X10:
		case V4L2_MBUS_FMT_SGRBG10_1X10:
		case V4L2_MBUS_FMT_SRGGB10_1X10:
		case V4L2_MBUS_FMT_SBGGR12_1X12:
		case V4L2_MBUS_FMT_SGBRG12_1X12:
		case V4L2_MBUS_FMT_SGRBG12_1X12:
		case V4L2_MBUS_FMT_SRGGB12_1X12:
			break;
		default:
			/* All MIPI CSI-2 devices
			 * must support one of primary formats */
			mf->code = V4L2_MBUS_FMT_YUYV8_2X8;
		}
		break;
	case SH_CSI2I:
		switch (mf->code) {
		case V4L2_MBUS_FMT_Y8_1X8:		/* RAW8 */
		case V4L2_MBUS_FMT_SBGGR8_1X8:
		case V4L2_MBUS_FMT_SGRBG8_1X8:
		case V4L2_MBUS_FMT_SBGGR10_1X10:	/* RAW10 */
		case V4L2_MBUS_FMT_SBGGR12_1X12:	/* RAW12 */
			break;
		default:
			/* All MIPI CSI-2 devices
			 * must support one of primary formats */
			mf->code = V4L2_MBUS_FMT_SBGGR8_1X8;
		}
		break;
	}

	return 0;
}

static irqreturn_t sh_mobile_csi2_irq(int irq, void *data)
{
	struct sh_csi2 *priv = data;
	u32 intstate = ioread32(priv->base + SH_CSI2_INTSTATE);
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	iowrite32(intstate, priv->base + SH_CSI2_INTSTATE);
	if (intstate & SH_CSI2_INT_ERROR) {
		priv->err_cnt++;
		printk(KERN_INFO "CSI Interrupt(0x%08X)\n", intstate);
	}
#if SH_CSI2_ERROR_RESET
	if (intstate & SH_CSI2_INT_RESET_ERROR) {
		u32 reg_vcdt = 0;
		u32 reg_inten = 0;
		printk(KERN_ALERT "CSI HW Error Reset3(0x%08X)\n", intstate);
		reg_vcdt = ioread32(priv->base + SH_CSI2_VCDT);
		reg_inten = ioread32(priv->base + SH_CSI2_INTEN);
		sh_csi2_hwinit(priv);
		iowrite32(reg_vcdt, priv->base + SH_CSI2_VCDT);
		sh_csi2_stream(priv, priv->strm_on);
		iowrite32(reg_inten, priv->base + SH_CSI2_INTEN);
	}
#endif /* SH_CSI2_ERROR_RESET */

#if SH_CSI2_DEBUG
	if (intstate & (1 << 26)) {
		if (0 == (priv->vd_s_cnt % 10))
			printk(KERN_ALERT "VD_S = %d\n", priv->vd_s_cnt);
		priv->vd_s_cnt++;
	}
	if (intstate & (1 << 25)) {
		if (0 == (priv->vd_e_cnt % 10))
			printk(KERN_ALERT "VD_E = %d\n", priv->vd_e_cnt);
		priv->vd_e_cnt++;
	}
	if (intstate & (1 << 17)) {
		if (0 == (priv->shp_cnt % 100))
			printk(KERN_ALERT "SHP = %d\n", priv->shp_cnt);
		priv->shp_cnt++;
	}
	if (intstate & (1 << 16)) {
		if (0 == (priv->lnp_cnt % 1000))
			printk(KERN_ALERT "LNP = %d\n", priv->lnp_cnt);
		priv->lnp_cnt++;
	}
#endif /* SH_CSI2_DEBUG */

	spin_unlock_irqrestore(&priv->lock, flags);

	return IRQ_HANDLED;
}

/*
 * We have done our best in try_fmt to try and tell the sensor, which formats
 * we support. If now the configuration is unsuitable for us we can only
 * error out.
 */
static int sh_csi2_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *mf)
{
	struct sh_csi2 *priv = container_of(sd, struct sh_csi2, subdev);
	u32 tmp = (priv->client->channel & 3) << 8;

	dev_dbg(sd->v4l2_dev->dev, "%s(%u)\n", __func__, mf->code);
	if (mf->width > 8188 || mf->width & 1)
		return -EINVAL;

	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
		tmp |= 0x1e;	/* YUV422 8 bit */
		break;
	case V4L2_MBUS_FMT_YUYV8_1_5X8:
		tmp |= 0x18;	/* YUV420 8 bit */
		break;
	case V4L2_MBUS_FMT_RGB555_2X8_PADHI_BE:
		tmp |= 0x21;	/* RGB555 */
		break;
	case V4L2_MBUS_FMT_RGB565_2X8_BE:
		tmp |= 0x22;	/* RGB565 */
		break;
	case V4L2_MBUS_FMT_Y8_1X8:
	case V4L2_MBUS_FMT_SBGGR8_1X8:
	case V4L2_MBUS_FMT_SGRBG8_1X8:
	case V4L2_MBUS_FMT_SGBRG8_1X8:
	case V4L2_MBUS_FMT_SRGGB8_1X8:
		tmp |= 0x2a;	/* RAW8 */
		break;
	case V4L2_MBUS_FMT_SBGGR10_1X10:
	case V4L2_MBUS_FMT_SGBRG10_1X10:
	case V4L2_MBUS_FMT_SGRBG10_1X10:
	case V4L2_MBUS_FMT_SRGGB10_1X10:
		tmp |= 0x2b;	/* RAW10 */
		break;
	case V4L2_MBUS_FMT_SBGGR12_1X12:
	case V4L2_MBUS_FMT_SGBRG12_1X12:
	case V4L2_MBUS_FMT_SGRBG12_1X12:
	case V4L2_MBUS_FMT_SRGGB12_1X12:
		tmp |= 0x2c;	/* RAW12 */
		break;
	default:
		return -EINVAL;
	}

	iowrite32(tmp, priv->base + SH_CSI2_VCDT);

	return 0;
}

static int sh_csi2_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->flags = V4L2_MBUS_PCLK_SAMPLE_RISING |
		V4L2_MBUS_HSYNC_ACTIVE_HIGH | V4L2_MBUS_VSYNC_ACTIVE_HIGH |
		V4L2_MBUS_MASTER | V4L2_MBUS_DATA_ACTIVE_HIGH;
	cfg->type = V4L2_MBUS_PARALLEL;

	return 0;
}

static int sh_csi2_s_mbus_config(struct v4l2_subdev *sd,
				const struct v4l2_mbus_config *cfg)
{
	struct sh_csi2 *priv = container_of(sd, struct sh_csi2, subdev);
	struct soc_camera_device *icd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_subdev *client_sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_config client_cfg = {.type = V4L2_MBUS_CSI2,
						.flags = priv->mipi_flags};

	return v4l2_subdev_call(client_sd, video, s_mbus_config, &client_cfg);
}

static int sh_csi2_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sh_csi2 *priv = container_of(sd, struct sh_csi2, subdev);
	struct sh_csi2_pdata *pdata = priv->pdev->dev.platform_data;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	if (1 == enable) {
		printk(KERN_ALERT "%s stream on\n", __func__);
		if (pdata->local_reset)
			pdata->local_reset(priv, 1);

		/* stream ON */
		iowrite32(SH_CSI2_INTEN_ALL, priv->base + SH_CSI2_INTEN);
		iowrite16(ioread16(priv->intcs_base + pdata->ipr) |
			(pdata->ipr_set), priv->intcs_base + pdata->ipr);
		iowrite8(pdata->imcr_set, priv->intcs_base + pdata->imcr);
		dev_dbg(&priv->pdev->dev,
			"> IPR(0x%x)=0x04%x, IMCR(0x%x)=0x02%x\n",
			pdata->ipr, ioread16(priv->intcs_base + pdata->ipr),
			pdata->imcr, ioread8(priv->intcs_base + pdata->imcr));

		priv->strm_on = 1;
		priv->err_cnt = 0;

		if (ioread32(priv->base + SH_CSI2_OUT) & 0x1)
			iowrite32(0x0, priv->base + SH_CSI2_OUT);
	} else if (0 == enable) {
		/* stream OFF */
		printk(KERN_ALERT "%s stream off\n", __func__);
		iowrite8(pdata->imcr_set, priv->intcs_base +
			pdata->imcr - 0x40);
		priv->strm_on = 0;

		if (pdata->local_reset)
			pdata->local_reset(priv, 0);
	} else {
		/* force stream off */
		dev_warn(&priv->pdev->dev, "%s force stream off\n", __func__);
		iowrite32(0x1, priv->base + SH_CSI2_SRST);
		iowrite32(0x1, priv->base + SH_CSI2_OUT);
		iowrite32(0x0, priv->base + SH_CSI2_SRST);
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

static int sh_csi2_g_input_status(struct v4l2_subdev *sd, u32 *status)
{
	struct sh_csi2 *priv = container_of(sd, struct sh_csi2, subdev);
	unsigned long flags;

	if (!status) {
		dev_err(&priv->pdev->dev, "status is NULL.\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&priv->lock, flags);
	*status = priv->err_cnt;
	priv->err_cnt = 0;
	spin_unlock_irqrestore(&priv->lock, flags);
	return 0;
}

static struct v4l2_subdev_video_ops sh_csi2_subdev_video_ops = {
	.s_mbus_fmt	= sh_csi2_s_fmt,
	.try_mbus_fmt	= sh_csi2_try_fmt,
	.s_stream	= sh_csi2_s_stream,
	.g_mbus_config	= sh_csi2_g_mbus_config,
	.s_mbus_config	= sh_csi2_s_mbus_config,
	.g_input_status	= sh_csi2_g_input_status,
};

static void sh_csi2_hwinit(struct sh_csi2 *priv)
{
	struct sh_csi2_pdata *pdata = priv->pdev->dev.platform_data;
	__u32 tmp = 0x10; /* Enable MIPI CSI clock lane */

	/* Reflect registers immediately */
	iowrite32(0x00000001, priv->base + SH_CSI2_TREF);
	/* reset CSI2 hardware */
	iowrite32(0x00000001, priv->base + SH_CSI2_SRST);
	udelay(5);
	iowrite32(0x00000000, priv->base + SH_CSI2_SRST);

	iowrite32(0x00000000, priv->base + SH_CSI2_PHYCNT);

	tmp = 0;
	if (pdata->flags & SH_CSI2_ECC)
		tmp |= 2;
	if (pdata->flags & SH_CSI2_CRC)
		tmp |= 1;
	iowrite32(tmp, priv->base + SH_CSI2_CHKSUM);

}

static int sh_csi2_client_connect(struct sh_csi2 *priv)
{
	struct sh_csi2_pdata *pdata = priv->pdev->dev.platform_data;
	struct soc_camera_device *icd = v4l2_get_subdev_hostdata(&priv->subdev);
	struct v4l2_subdev *client_sd = soc_camera_to_subdev(icd);
	struct device *dev = v4l2_get_subdevdata(&priv->subdev);
	struct v4l2_mbus_config cfg;
	unsigned long common_flags, csi2_flags;
	struct sh_csi2_pdata *csi_info;
	int i, ret;

	if (priv->client)
		return -EBUSY;

	for (i = 0; i < pdata->num_clients; i++)
		if (&pdata->clients[i].pdev->dev == icd->pdev)
			break;

	dev_dbg(dev, "%s(%p): found #%d\n", __func__, dev, i);

	if (i == pdata->num_clients)
		return -ENODEV;

	/* Check if we can support this camera */
	csi2_flags = V4L2_MBUS_CSI2_CONTINUOUS_CLOCK | V4L2_MBUS_CSI2_1_LANE;

	switch (pdata->type) {
	case SH_CSI2C:
		if (pdata->clients[i].lanes != 1)
			csi2_flags |= V4L2_MBUS_CSI2_2_LANE;
		break;
	case SH_CSI2I:
		switch (pdata->clients[i].lanes) {
		default:
			csi2_flags |= V4L2_MBUS_CSI2_4_LANE;
		case 3:
			csi2_flags |= V4L2_MBUS_CSI2_3_LANE;
		case 2:
			csi2_flags |= V4L2_MBUS_CSI2_2_LANE;
		}
	}

	cfg.type = V4L2_MBUS_CSI2;
	ret = v4l2_subdev_call(client_sd, video, g_mbus_config, &cfg);
	if (ret == -ENOIOCTLCMD)
		common_flags = csi2_flags;
	else if (!ret)
		common_flags = soc_mbus_config_compatible(&cfg,
							csi2_flags);
	else
		common_flags = 0;

	if (!common_flags)
		return -EINVAL;

	/* All good: camera MIPI configuration supported */
	priv->mipi_flags = common_flags;
	priv->client = pdata->clients + i;

	pm_runtime_get_sync(dev);

	sh_csi2_hwinit(priv);
	csi_info = priv->pdev->dev.platform_data;
	sh_csi2_stream(csi_info->priv, 1);

	return 0;
}

static void sh_csi2_client_disconnect(struct sh_csi2 *priv)
{
	if (!priv->client)
		return;

	sh_csi2_stream(priv, 0);

	priv->client = NULL;

	pm_runtime_put(v4l2_get_subdevdata(&priv->subdev));
}


static int sh_csi2_s_power(struct v4l2_subdev *sd, int on)
{
	struct sh_csi2 *priv = container_of(sd, struct sh_csi2, subdev);

	priv->power = on;
	if (on) {
		sh_csi2_l_power(priv, on);
		return sh_csi2_client_connect(priv);
	}

	priv->first_power = 0;
	sh_csi2_client_disconnect(priv);
	sh_csi2_l_power(priv, on);
	return 0;
}


static struct v4l2_subdev_core_ops sh_csi2_subdev_core_ops = {
	.s_power	= sh_csi2_s_power,
};

static struct v4l2_subdev_ops sh_csi2_subdev_ops = {
	.core	= &sh_csi2_subdev_core_ops,
	.video	= &sh_csi2_subdev_video_ops,
};

static __devinit int sh_csi2_probe(struct platform_device *pdev)
{
	struct resource *res;
	unsigned int irq;
	int ret;
	struct sh_csi2 *priv;
	/* Platform data specify the PHY, lanes, ECC, CRC */
	struct sh_csi2_pdata *pdata = pdev->dev.platform_data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	/* Interrupt unused so far */
	irq = platform_get_irq(pdev, 0);

	if (!res || (int)irq <= 0 || !pdata) {
		dev_err(&pdev->dev, "Not enough CSI2 platform resources.\n");
		return -ENODEV;
	}

	/* TODO: Add support for CSI2I. Careful: different register layout! */
	if (pdata->type != SH_CSI2C) {
		dev_err(&pdev->dev, "Only CSI2C supported ATM.\n");
		return -EINVAL;
	}

	priv = kzalloc(sizeof(struct sh_csi2), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->irq = irq;

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		if (!(pdata->flags & SH_CSI2_MULTI)) {
			dev_err(&pdev->dev, "CSI2 register region already claimed\n");
			ret = -EBUSY;
			goto ereqreg;
		}
	}

	priv->base = ioremap(res->start, resource_size(res));
	if (!priv->base) {
		ret = -ENXIO;
		dev_err(&pdev->dev, "Unable to ioremap CSI2 registers.\n");
		goto eremap;
	}

	priv->pdev = pdev;

	pdata->priv = priv;
	platform_set_drvdata(pdev, priv);

	v4l2_subdev_init(&priv->subdev, &sh_csi2_subdev_ops);
	v4l2_set_subdevdata(&priv->subdev, &pdev->dev);

	snprintf(priv->subdev.name, V4L2_SUBDEV_NAME_SIZE, "%s.mipi-csi",
			dev_name(pdata->v4l2_dev->dev));
	ret = v4l2_device_register_subdev(pdata->v4l2_dev, &priv->subdev);
	dev_dbg(&pdev->dev,
		"%s(%p): ret(register_subdev) = %d\n", __func__, priv, ret);
	if (ret < 0)
		goto esdreg;

	pm_runtime_enable(&pdev->dev);

	spin_lock_init(&priv->lock);

	priv->strm_on = 0;
	priv->err_cnt = 0;
#if SH_CSI2_DEBUG
	priv->vd_s_cnt = 0;
	priv->vd_e_cnt = 0;
	priv->shp_cnt = 0;
	priv->lnp_cnt = 0;
#endif /* SH_CSI2_DEBUG */

	dev_dbg(&pdev->dev, "CSI2 probed.\n");

	return 0;

esdreg:
	iounmap(priv->base);
eremap:
	release_mem_region(res->start, resource_size(res));
ereqreg:
	kfree(priv);

	return ret;
}

static __devexit int sh_csi2_remove(struct platform_device *pdev)
{
	struct sh_csi2 *priv = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		printk(KERN_ERR "platform_get_resource is NULL.\n");
		return -ENOMEM;
	}
	v4l2_device_unregister_subdev(&priv->subdev);
	pm_runtime_disable(&pdev->dev);
	iounmap(priv->base);
	release_mem_region(res->start, resource_size(res));
	platform_set_drvdata(pdev, NULL);
	kfree(priv);

	return 0;
}

static struct platform_driver __refdata sh_csi2_pdrv = {
	.remove	= __devexit_p(sh_csi2_remove),
	.probe	= sh_csi2_probe,
	.driver	= {
		.name	= "sh-mobile-csi2",
		.owner	= THIS_MODULE,
	},
};

static void sh_csi2_l_power(struct sh_csi2 *priv, int power_on)
{
	struct clk *csi_clk;
	struct clk *meram_clk;
	struct clk *icb_clk;
	struct sh_csi2_pdata *csi_info;
#ifdef CSI2_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_param powarea_end_notify;
	system_pmg_delete pmg_delete;
#endif
#ifdef RTAPI_CAMERA_ENABLE
	void *camera_handle;
	struct camera_prm_param camera_notify;
	struct camera_prm_delete cam_delete;
#endif
	int ret;
	csi_info = priv->pdev->dev.platform_data;

	csi_clk = clk_get(NULL, csi_info->cmod_name);
	if (IS_ERR(csi_clk)) {
		printk(KERN_ALERT"%s clk_get(%s)", __func__,
			csi_info->cmod_name);
		return;
	}
	meram_clk = clk_get(NULL, "meram");
	if (IS_ERR(csi_clk)) {
		printk(KERN_ALERT"%s clk_get(meram)", __func__);
		return;
	}
	icb_clk = clk_get(NULL, "icb");
	if (IS_ERR(icb_clk)) {
		printk(KERN_ALERT"%s clk_get(icb)", __func__);
		return;
	}

	if (csi_info->priv) {
#ifdef RTAPI_CAMERA_ENABLE
		camera_handle = camera_new();
		camera_notify.handle = camera_handle;
#endif
#ifdef CSI2_POWAREA_MNG_ENABLE
		system_handle = system_pwmng_new();
#endif
		if (power_on) {
#ifdef RTAPI_CAMERA_ENABLE
			printk(KERN_INFO "Start A3R power area(CSI2)\n");
			ret = camera_start_notify(&camera_notify);
			if (SMAP_LIB_CAMERA_OK != ret)
				printk(KERN_ERR
				"camera_start_notify err[%d]!\n", ret);
#endif
#ifdef CSI2_POWAREA_MNG_ENABLE
			printk(KERN_INFO "Start A4LC power area(CSI2)\n");
			/* Notifying the Beginning of Using Power Area */
			powarea_start_notify.handle = system_handle;
			powarea_start_notify.powerarea_name =
				RT_PWMNG_POWERAREA_A4LC;
			ret = system_pwmng_powerarea_start_notify(
				&powarea_start_notify);
			if (SMAP_LIB_PWMNG_OK != ret)
				printk(KERN_ERR
					"powarea_start_notify err[%d]!\n", ret);
#endif
			ret = clk_enable(icb_clk);
			if (0 != ret) {
				printk(
				KERN_ALERT "%s :clk_enable(icb) error(%d)",
				__func__, ret);
			}
			ret = clk_enable(meram_clk);
			if (0 != ret) {
				printk(
				KERN_ALERT "%s :clk_enable(meram) error(%d)",
				__func__, ret);
			}
			ret = clk_enable(csi_clk);
			if (0 != ret) {
				printk(
				KERN_ALERT "%s :clk_enable(%s) error(%d)",
				__func__, csi_info->cmod_name, ret);
			}

			if (request_irq(priv->irq, sh_mobile_csi2_irq,
				IRQF_DISABLED, dev_name(&priv->pdev->dev),
				priv)) {
				dev_err(&priv->pdev->dev,
					"Unable to register CSI interrupt.\n");
			}
			priv->intcs_base = ioremap_nocache(0xFFD50000, 0x1000);
		} else {
			iounmap(priv->intcs_base);
			free_irq(priv->irq, priv);
			clk_disable(csi_clk);
			clk_disable(meram_clk);
			clk_disable(icb_clk);
#ifdef CSI2_POWAREA_MNG_ENABLE
			printk(KERN_INFO "End A4LC power area(CSI2)\n");
			/* Notifying the Beginning of Using Power Area */
			powarea_end_notify.handle = system_handle;
			powarea_end_notify.powerarea_name =
				RT_PWMNG_POWERAREA_A4LC;
			ret = system_pwmng_powerarea_end_notify(
				&powarea_end_notify);
			if (SMAP_LIB_PWMNG_OK != ret)
				printk(KERN_ERR
					"powarea_end_notify err[%d]!\n", ret);
#endif
#ifdef RTAPI_CAMERA_ENABLE
			printk(KERN_INFO "End A3R power area(CSI2)\n");
			ret = camera_end_notify(&camera_notify);
			if (SMAP_LIB_CAMERA_OK != ret)
				printk(KERN_ERR
				"camera_end_notify err[%d]!\n", ret);
#endif
		}
#ifdef RTAPI_CAMERA_ENABLE
		cam_delete.handle = camera_handle;
		camera_delete(&cam_delete);
#endif
#ifdef CSI2_POWAREA_MNG_ENABLE
		pmg_delete.handle = system_handle;
		system_pwmng_delete(&pmg_delete);
#endif
	}
	clk_put(csi_clk);
	clk_put(meram_clk);
	clk_put(icb_clk);
}

int sh_csi2__l_reset(void *handle, int reset)
{
	struct sh_csi2 *priv = (struct sh_csi2 *)handle;
	u32 reg_vcdt = 0;
	if (reset) {
		reg_vcdt = ioread32(priv->base + SH_CSI2_VCDT);
		sh_csi2_hwinit(priv);
		iowrite32(reg_vcdt, priv->base + SH_CSI2_VCDT);
		sh_csi2_stream(priv, 1);
	} else {
		sh_csi2_stream(priv, 0);
	}
	return 0;
}

void sh_csi2_power(struct device *dev, int power_on)
{
	struct soc_camera_link *icl;
	struct sh_csi2_pdata *csi_info;
	struct sh_csi2 *priv;

	if (!dev) {
		printk(KERN_ERR "%s :not device\n", __func__);
		return;
	}
	icl = (struct soc_camera_link *) dev->platform_data;
	csi_info = (struct sh_csi2_pdata *) icl->priv;
	priv = (struct sh_csi2 *)csi_info->priv;

	if (priv && priv->power && priv->first_power) {
		dev_info(&priv->pdev->dev, "local reset route(%d)\n", power_on);
		sh_csi2__l_reset(priv, power_on);
	} else if (priv && !priv->first_power) {
		priv->first_power = 1;
	}
}

module_platform_driver(sh_csi2_pdrv);

MODULE_DESCRIPTION("SH-Mobile MIPI CSI-2 driver");
MODULE_AUTHOR("Guennadi Liakhovetski <g.liakhovetski@gmx.de>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:sh-mobile-csi2");
