/*
 * Generic GPIO API and pinmux table support
 *
 * Copyright (c) 2008  Magnus Damm
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef __ASM_ARCH_GPIO_H
#define __ASM_ARCH_GPIO_H

#include <linux/kernel.h>
#include <linux/errno.h>

#include <mach/r8a7373.h>
#include <linux/sh_pfc.h>
#include <linux/io.h>

#ifdef CONFIG_GPIOLIB

/* gpio address calculation */
#define GPIO_PORTCR(n) ({		 \
((n) < 96) ? (GPIO_BASE + 0x0000 + (n)) : \
((n) < 128) ? (GPIO_BASE + 0x0000 + (n)) :  \
((n) < 144) ? (GPIO_BASE + 0x1000 + (n)) : \
((n) < 192) ? NULL : \
((n) < 320) ? (GPIO_BASE + 0x2000 + (n)) : \
((n) < 328) ? (GPIO_BASE + 0x2000 + (n)) : NULL; })

/* GPIO Settings - PULMD (Pull OFF/Pull DOWN/Pull UP) */
#define PORTn_CR_PULL_NOT_SET	-1
#define PORTn_CR_PULL_OFF	0
#define PORTn_CR_PULL_DOWN	1
#define PORTn_CR_PULL_UP	2

/* GPIO Settings - IE/OE */
#define PORTn_CR_DIRECTION_NOT_SET	(-1)
#define PORTn_CR_DIRECTION_NONE		0
#define PORTn_CR_DIRECTION_OUTPUT	1
#define PORTn_CR_DIRECTION_INPUT	2
#define PORTn_CR_BI_DIRECTIONAL		3

/* GPIO Settings - Output data level High/Low */
#define PORTn_OUTPUT_LEVEL_NOT_SET      (-1)
#define PORTn_OUTPUT_LEVEL_LOW          0
#define PORTn_OUTPUT_LEVEL_HIGH         1

/* GPIO Mask Values */
#define GPIO_DIRECTION_NONE     0xcf
#define GPIO_PULL_OFF           0x3f
#define GPIO_PULL_UP            0xc0
#define GPIO_PULL_DOWN          0x80
#define GPIO_BIDIRECTION        0x30
#define INPUT           0x20
#define OUTPUT          0x10
#define FUNCTION_0      0x00
#define FUNCTION_1      0x01

struct portn_gpio_setting {
	u32	port_fn;	/* Pin function select*/
	s32	pull;		/* Pull Off/Down/Up */
	s32	direction;	/* Input/Output direction */
	/* It become enable only when direction is output. */
	s32 output_level;
};

struct portn_gpio_setting_info {
	u32	flag;	/* 0: no change required on suspend
			   1: change required on suspend*/
	u32	port;	/* GPIO port number */
	/* GPIO settings to be retained on resume */
	struct portn_gpio_setting active;
	/* GPIO settings on suspending state */
	struct portn_gpio_setting inactive;
};
static inline int irq_to_gpio(unsigned int irq)
{
	return -ENOSYS;
}

#else

#define __ARM_GPIOLIB_COMPLEX

#endif /* CONFIG_GPIOLIB */

/*
 * GPIO API supplement
 *
 * Generic GPIO library lacks of some functions; it doesn't provide any
 * APIs to specify pull-up or pull-down of the port, nor an API to disable
 * both the input and the output of the port.  These supplementary APIs
 * are to fill in the missing piece of generic GPIO APIs.
 *
 * Note that these APIs are supposed to be used _after_ primary port
 * configuration of each port has been done using generic GPIO APIs.
 */

static inline void gpio_direction_none(unsigned long reg)
{
	__raw_writeb(0, reg);
}

static inline void gpio_pull_off(unsigned long reg)
{
	__raw_writeb(__raw_readb(reg) & 0x3f, reg);
}

static inline void gpio_pull_up(unsigned long reg)
{
	__raw_writeb((__raw_readb(reg) & 0x3f) | 0xc0, reg);
}

static inline void gpio_pull_down(unsigned long reg)
{
	__raw_writeb((__raw_readb(reg) & 0x3f) | 0x80, reg);
}
static inline int gpio_get_value(unsigned gpio)
{
	return __gpio_get_value(gpio);
}

static inline void gpio_set_value(unsigned gpio, int value)
{
	__gpio_set_value(gpio, value);
}

static inline int gpio_cansleep(unsigned gpio)
{
	return __gpio_cansleep(gpio);
}

static inline int gpio_to_irq(unsigned gpio)
{
	return __gpio_to_irq(gpio);
}

extern void gpio_direction_none_port(int gpio);
extern void gpio_pull_off_port(int gpio);
extern void gpio_pull_up_port(int gpio);
extern void gpio_pull_down_port(int gpio);
extern void gpio_bidirection_port(int gpio);
extern void unused_gpio_port_init(int gpio);
extern void gpio_set_portncr_value(unsigned int port_count,
	struct portn_gpio_setting_info *gpio_setting_info, int suspend_mode);
extern void gpiopd_hwspinlock_init(struct hwspinlock *hwlock);

#endif /* __ASM_ARCH_GPIO_H */
