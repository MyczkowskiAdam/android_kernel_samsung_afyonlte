/*
 * R-Mobile CMT (Compare Match Timer) support
 *
 * Copyright (C) 2012  Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 */
#ifndef __ARCH_MACH_SHMOBILE_CMT_H
#define __ARCH_MACH_SHMOBILE_CMT_H

#include <linux/ioport.h>

/*
 * CMT (Compare Match Timer): What's changed in R-Mobile U2
 *
 *               R-Mobile           R-Mobile          Notes
 * H'E613_xxxx   APE5R              U2
 * -----------------------------------------------------------------
 *      0x0000   CMSTR   (16-bit)   CMSTR0 (32-bit)   (*1)
 *      0x0010   CMCSR0  (16-bit)   CMCSR0 (32-bit)   (*2)
 *      0x0014   CMCNT0  (32-bit)   CMCNT0 (32-bit)
 *      0x0018   CMCOR0  (32-bit)   CMCOR0 (32-bit)
 *
 *     (0x0010)  -                  -
 *      0x0020   CMCSR1  (16-bit)   -
 *      0x0024   CMCNT1  (32-bit)   -
 *      0x0028   CMCOR1  (32-bit)   -
 *
 *     (0x0020)  -                  -
 *      0x0030   CMCSR2  (16-bit)   -
 *      0x0034   CMCNT2  (32-bit)   -
 *      0x0038   CMCOR2  (32-bit)   -
 *
 *        :        :                  :
 *
 *      0x0100   -                  CMSTR1 (32-bit)   (*1)
 *      0x0110   CMCSRH0 (32-bit)   CMCSR1 (32-bit)
 *      0x0114   CMCNTH0 (32-bit)   CMCNT1 (32-bit)
 *      0x0118   CMCORH0 (32-bit)   CMCOR1 (32-bit)
 *
 *     (0x0110)  -                  -
 *      0x0120   CMCSRH1 (32-bit)   -
 *      0x0124   CMCNTH1 (32-bit)   -
 *      0x0128   CMCORH1 (32-bit)   -
 *
 *      0x0200   -                  CMSTR2 (32-bit)   (*1)
 *      0x0210   -                  CMCSR2 (32-bit)
 *      0x0214   -                  CMCNT2 (32-bit)
 *      0x0218   -                  CMCOR2 (32-bit)
 *
 *        :        :                  :
 *
 *      0x1000   -                  CMCLKE (32-bit)   (*3)
 *
 * Notes:
 *
 * (*1) CMSTR used to be a shared register withih a CMT device, but
 *      changed to be one of per-channel registers in R-Mobile U2
 *
 * (*2) CMCSR used to be 16-bit wide, but in R-Mobile U2 all per-
 *      channel registers are 32-bit wide
 *
 * (*3) CMCLKE has been introduced in R-Mobile U2 to enable/disable
 *      clock supply to each channel in a CMT device
 */
#define CMSTR		0x00
#define CMCSR		0x10
#define CMCNT		0x14
#define CMCOR		0x18

#define CMF		(1 << 15)	/* compare match flag */
#define OVF		(1 << 14)	/* overflow flag */
#define CMM		(1 << 8)	/* compare match mode */
#define CMR		(2 << 4)	/* compare match request */
#define DVBIVD		(1 << 3)	/* debug mode */

#ifndef __ASSEMBLY__
struct cmt_timer_clock {
	const char *name;
	unsigned int divisor;
	unsigned long min_delta_ticks;
};

#define CKS(_name, _divisor, _min_delta)	\
	{ .name = _name, .divisor = _divisor, .min_delta_ticks = _min_delta}

struct cmt_timer_config {
	struct resource res[2];

	char *name;
	int timer_bit;

	struct cmt_timer_clock *cks_table;
	unsigned short cks_num;
	unsigned short cks;
	unsigned long cmcsr_init;
};

extern spinlock_t cmt_lock;

int cmt_clockevent_init(struct cmt_timer_config *cfg, int num,
			unsigned long cmstr_base, unsigned long cmclke_base);

#endif /* #ifndef __ASSEMBLY__ */
#endif /* __ARCH_MACH_SHMOBILE_CMT_H */
