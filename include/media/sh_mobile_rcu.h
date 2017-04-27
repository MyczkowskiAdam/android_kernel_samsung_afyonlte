/*
 * Copyright (C) 2012 Renesas Mobile Corp.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __ASM_SH_MOBILE_RCU_H__
#define __ASM_SH_MOBILE_RCU_H__

#define SH_RCU_FLAG_HSYNC_LOW		(1 << 0) /* Default High if possible */
#define SH_RCU_FLAG_VSYNC_LOW		(1 << 1) /* Default High if possible */

#define SH_RCU_OUTPUT_MEM			0x00	/* Memory output */
#define SH_RCU_OUTPUT_ISP			0x01	/* ISP output (default) */
#define SH_RCU_OUTPUT_MEM_ISP			0x03	/* Memory and ISP output */
#define SH_RCU_OUTPUT_MEM_EXT			0x04	/* Memory Extended */

#define SH_RCU_OUTPUT_OFFSET_OFF		(0)
#define SH_RCU_OUTPUT_OFFSET_32B		(1)

#define SH_RCU_OUTPUT_ENHANCING			(0)
#define SH_RCU_OUTPUT_PACKING			(1)

#define SH_RCU_OUTPUT_SDRAM			(0)
#define SH_RCU_OUTPUT_MERAM			(1)

#define SH_RCU_OUTPUT_ISP_FULL			(0x00)
#define SH_RCU_OUTPUT_ISP_THINNED_AVERAGE	(0x09)
#define SH_RCU_OUTPUT_ISP_THINNED		(0x0B)

#define SH_RCU_LED_ON				(1)
#define SH_RCU_LED_OFF				(0)
#define SH_RCU_LED_MODE_NORM			(0)
#define SH_RCU_LED_MODE_PRE			(1<<1)

struct device;
struct resource;

struct sh_mobile_rcu_companion {
	u32		num_resources;
	struct resource	*resource;
	int		id;
	void		*platform_data;
};

struct sh_mobile_rcu_info {
	unsigned long flags;
	int max_width;
	int max_height;
	struct sh_mobile_rcu_companion *csi2;
	char *mod_name;
	int (*led)(int, int);
};

void sh_mobile_rcu_flash(int led);

void sh_mobile_rcu_event_time_func(unsigned short id);
void sh_mobile_rcu_event_time_data(unsigned short id, unsigned int data);

#endif /* __ASM_SH_MOBILE_RCU_H__ */
