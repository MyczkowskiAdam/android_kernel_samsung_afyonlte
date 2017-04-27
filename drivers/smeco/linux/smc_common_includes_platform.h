/*
* Copyright (c) 2013, Renesas Mobile Corporation.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#if 0
/*
Change history:

Version:       4    29-Feb-2012     Heikki Siikaluoma
Status:        draft
Description :  Linux Kernel timer functions for SMC

Version:       1    22-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMC_COMMON_INCLUDES_PLATFORM_H
#define SMC_COMMON_INCLUDES_PLATFORM_H

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/highmem.h>

  /* Linux kernel specific definitions */
#define TRUE                   1
#define FALSE                  0
#define SMC_MALLOC(size)       kmalloc(size, GFP_KERNEL)
#define SMC_MALLOC_IRQ(size)   kmalloc(size, GFP_ATOMIC)
#define SMC_FREE(p)            kfree(p)

#define SMC_SLEEP_MS(time_ms)  msleep(time_ms)

#define SMC_TIMESTAMP_GET      0

#ifndef CHAR_BIT
    #define CHAR_BIT           8
#endif

#ifndef NULL
    #define NULL               0x00000000
#endif

#ifndef SMCTEST
    #define SMCTEST            FALSE
#endif


#define assert( param )        BUG_ON( !(param) )

uint32_t rand(void);

void     smc_printf_data_linux_kernel(int length, uint8_t* data);

#endif

