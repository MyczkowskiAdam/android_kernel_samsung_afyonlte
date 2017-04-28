
/*
 * arch/arm/mach-shmobile/include/mach/setup-u2usb.h
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
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
 *
 */
#ifndef __ASM_ARCH_U2USB_H
#define __ASM_ARCH_U2USB_H __FILE__


#define USB_SPWDAT		IO_ADDRESS(HSUSB_BASE_PHYS + 0x013A)
#define USB_SPCTRL		IO_ADDRESS(HSUSB_BASE_PHYS + 0x013C)
#define USB_SPRDAT		IO_ADDRESS(HSUSB_BASE_PHYS + 0x013E)
#define USB_SPEXADDR		IO_ADDRESS(HSUSB_BASE_PHYS + 0x0140)
#define USB_SPWR		0x0001
#define USB_SPRD		0x0002
#define USB_SPADDR		IO_ADDRESS(HSUSB_BASE_PHYS + 0x0138)

extern struct platform_device tusb1211_device;
extern struct platform_device usb_host_device;
extern struct platform_device usbhs_func_device;
extern struct platform_device usbhs_func_device_d2153;

extern void __init USBGpio_init(void);

#endif /* __ASM_ARCH_U2USB_H */
