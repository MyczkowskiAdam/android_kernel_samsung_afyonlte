/*
 * Header for the SH-Mobile Watchdog Timer
 *
 * Copyright (C) 2011  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _SH_MOBILE_WDT_H
#define _SH_MOBILE_WDT_H

struct sh_mobile_wdt_pdata {
	int	count_width;	/* 8 or 16 */
	bool	use_internal;	/* 1:Clear internal, 0:Clear user space */
	int	timeout;	/* timeout sec */
	void	(*wdt_irq_handler)(void); /* expire handler */
};

#endif
