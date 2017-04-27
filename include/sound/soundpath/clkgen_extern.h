/* clkgen_extern.h
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
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


#ifndef __CLKGEN_EXTERN_H__
#define __CLKGEN_EXTERN_H__

#include <linux/kernel.h>

#ifdef __CLKGEN_CTRL_NO_EXTERN__
#define CLKGEN_CTRL_NO_EXTERN
#else
#define CLKGEN_CTRL_NO_EXTERN	extern
#endif

/* CLKGEN start */
CLKGEN_CTRL_NO_EXTERN int clkgen_start(const u_int uiValue, const int iRate, const u_int rate);
/* CLKGEN stop */
CLKGEN_CTRL_NO_EXTERN void clkgen_stop(void);
/* CLKGEN registers dump */
CLKGEN_CTRL_NO_EXTERN void clkgen_reg_dump(void);

#ifdef SOUND_TEST
CLKGEN_CTRL_NO_EXTERN void clkgen_play_test_start_a(void);
CLKGEN_CTRL_NO_EXTERN void clkgen_play_test_stop_a(void);
CLKGEN_CTRL_NO_EXTERN void clkgen_rec_test_start_a(void);
CLKGEN_CTRL_NO_EXTERN void clkgen_rec_test_stop_a(void);
CLKGEN_CTRL_NO_EXTERN void clkgen_voice_test_start_a(void);
CLKGEN_CTRL_NO_EXTERN void clkgen_voice_test_stop_a(void);
#endif /* SOUNT_TEST */


#endif /* __CLKGEN_EXTERN_H__ */


