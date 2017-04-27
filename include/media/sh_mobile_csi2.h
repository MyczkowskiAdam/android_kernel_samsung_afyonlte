/*
 * Driver header for the SH-Mobile MIPI CSI-2 unit
 *
 * Copyright (C) 2012 Renesas Mobile Corp.
 * All rights reserved.
 *
 * Copyright (C) 2010, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef SH_MIPI_CSI
#define SH_MIPI_CSI

#include <linux/list.h>

enum sh_csi2_phy {
	SH_CSI2_PHY_MAIN,
	SH_CSI2_PHY_SUB,
};

enum sh_csi2_type {
	SH_CSI2C,
	SH_CSI2I,
};

#define SH_CSI2_CRC	(1 << 0)
#define SH_CSI2_ECC	(1 << 1)

#define SH_CSI2_MULTI	(1 << 3)

struct platform_device;

struct sh_csi2_client_config {
	enum sh_csi2_phy phy;
	unsigned char lanes;		/* bitmask[3:0] */
	unsigned char channel;		/* 0..3 */
	struct platform_device *pdev;	/* client platform device */
};

struct device;
struct v4l2_device;

struct sh_csi2_pdata {
	enum sh_csi2_type type;
	unsigned int flags;
	struct sh_csi2_client_config *clients;
	int num_clients;
	unsigned int ipr;
	unsigned short ipr_set;
	unsigned int imcr;
	unsigned char imcr_set;
	void	*priv;
	char *cmod_name;
	int (*local_reset)(void*, int);
	struct v4l2_device *v4l2_dev;
	void (*log_output)(void*, int);
	void *log_data;
};

void sh_csi2_power(struct device *dev, int power_on);
int sh_csi2__l_reset(void *handle, int reset);

#endif
