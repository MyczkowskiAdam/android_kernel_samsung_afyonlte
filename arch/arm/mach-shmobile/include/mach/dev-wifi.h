/*
 * ~/arch/arm/mach-shmobile/board-u2evm-wifi.h
 */
/*
 * Copyright (C) 2011 Renesas Mobile Corporation.
 * Copyright (C) 2011 Renesas Design Vietnam Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <mach/gpio.h>

/* Define GPIO aliases */
#define GPIO_WLAN_REG_ON 260
#define GPIO_WLAN_OOB_IRQ 98
#define WLAN_OOB_IRQ_CR GPIO_PORTCR(98)

/* Enable debug messages */
#define U2EVM_WIFI_DEBUG_MSG

#ifdef U2EVM_WIFI_DEBUG_MSG
#define WIFI_DEBUG(...) printk(KERN_INFO __VA_ARGS__)
#define WIFI_ERROR(...) printk(KERN_ERR __VA_ARGS__)
#else
#define WIFI_DEBUG(...) while(0)
#define WIFI_ERROR(...) while(0)
#endif

int __init renesas_wlan_init(void);
