/*
 * arch/arm/mach-shmobile/include/mach/sbsc.h
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
#ifndef __ASM_ARCH_SBSC_H
#define __ASM_ARCH_SBSC_H __FILE__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>

#define SBSC_SDWCRC0A		0x40
#define SBSC_SDWCRC1A		0x44
#define SBSC_SDWCRC2A		0x64
#define SBSC_SDWCR00A		0x48
#define SBSC_SDWCR01A		0x4C
#define SBSC_SDWCR10A		0x50
#define SBSC_SDWCR11A		0x54
#define SBSC_SDMRACR1A_OFFSET	0x88
#define SBSC_SAPRICR0		0x240
#define SBSC_SAPRICR1		0x70

void __init shmobile_sbsc_init(void);
u32 shmobile_sbsc_read_reg32(u32 offset);
void shmobile_sbsc_write_reg32(u32 val, u32 offset);
void shmobile_sbsc_update_param(struct sbsc_param *param);

/*Shall be called under cpg lock only*/
void shmobile_set_ape_req_freq(unsigned int freq);
unsigned int shmobile_get_ape_req_freq(void);
unsigned int shmobile_get_modem_req_freq(void);
unsigned int shmobile_get_pll_reprogram(void);

extern void cpg_init_bbfrqcrd(void);
extern struct sbsc_param zb3_lut[];
extern void cpg_enable_sbsc_change_for_modem(void);
extern int shmobile_acquire_cpg_lock(unsigned long *flags,
	unsigned int timeout);
extern int shmobile_release_cpg_lock(unsigned long *flags);
extern int shmobile_hwspin_lock_timeout_nospin(struct hwspinlock *hwlock,
	unsigned int timeout);
extern u32 shmobile_get_lock_cpg_nospin(void);
#define SBSC_BASE		(0xFE400000U)
#define SBSC_SIZE		0x1200

#define SBSC_SDMRA_DONE     (0x00000000)
#define SBSC_SDMRACR1A_ZQ   (0x0000560A)

#endif /* __ASM_ARCH_SBSC_H */

