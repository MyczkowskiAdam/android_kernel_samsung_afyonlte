/* Header file for exported stm_printk_xxx functions implemented
 * in drivers/char/stm_tty.c
 *
 * Copyright (C) 2012 Renesas Mobile Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __STM_TTY_H
#define __STM_TTY_H

#ifdef CONFIG_TTY_LL_STM
extern void stm_printk_str(uint32_t channel, const char *bptr);
extern void stm_printk_fmt_1x(uint32_t channel, const char *bptr,
		unsigned arg1);
extern void stm_printk_fmt_2x(uint32_t channel, const char *bptr,
		unsigned arg1, unsigned arg2);
#else
#define stm_printk_str(a, b)
#define stm_printk_fmt_1x(a, b, c)
#define stm_printk_fmt_2x(a, b, c, d)
#endif

#endif /* __SH_PFC_H */
