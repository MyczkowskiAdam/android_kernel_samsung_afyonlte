/*
 * drivers/sec_hal/rt/src/sec_dispatch.c
 *
 * Copyright (c) 2012-2013, Renesas Mobile Corporation.
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

#include "sec_dispatch.h"
#include "sec_hal_rt_cmn.h"
#include "sec_serv_api.h"

#include <stdarg.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/hardirq.h>
#include <asm/cacheflush.h>


#define SEC_ROM_ICACHE_ENABLE_MASK              0x00000001
#define SEC_ROM_DCACHE_ENABLE_MASK              0x00000002
#define SEC_ROM_IRQ_ENABLE_MASK                 0x00000004
#define SEC_ROM_FIQ_ENABLE_MASK                 0x00000008
#define SEC_ROM_UL2_CACHE_ENABLE_MASK           0x00000010

#define MODE_USER                               0x10
#define MODE_FIQ                                0x11
#define MODE_IRQ                                0x12
#define MODE_SVC                                0x13
#define MODE_ABORT                              0x17
#define MODE_UNDEF                              0x1B
#define MODE_SYSTEM                             0x1F
#define MODE_MONITOR                            0x16
#define MODE_CLR_MASK                           0x1F
#define ARM_THUMB_MODE_MASK                     0x20
#define FIQ_IRQ_MASK                            0xC0
#define FIQ_MASK                                0x40
#define IRQ_MASK                                0x80


/*
 * only one process allowed simultaneously.
 * secside has failsafe spinlock.
 */
static DEFINE_MUTEX(disp_mutex);

/* bridge function decl. */
uint32_t hw_sec_rom_pub_bridge(uint32_t appl_id, uint32_t flags, va_list *);


/* ****************************************************************************
 * Function name      : raw_pub2sec_dispatcher
 * Description        : raw dispatcher, can be used from IRQ context.
 *                      service calls should short, to reduce possible IRQ lat.
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * ***************************************************************************/
uint32_t raw_pub2sec_dispatcher(uint32_t appl_id, uint32_t flags, ...)
{
	uint32_t pub_cpsr, rv = 0;
	va_list ap;

	/* Read current CPSR */
	__asm__ __volatile__("MRS %0, CPSR" : "=r"(pub_cpsr));

	/* FIQ won't be enabled in secure mode if FIQ is currently disabled */
	if (pub_cpsr & FIQ_MASK)
		flags &= ~SEC_ROM_FIQ_ENABLE_MASK;

	/* IRQ won't be enabled in secure mode if IRQ is currently disabled */
	if (pub_cpsr & IRQ_MASK)
		flags &= ~SEC_ROM_IRQ_ENABLE_MASK;

	va_start(ap, flags);
	/* 'Address of va_list' convention is inherited from prev. projects */
	rv = hw_sec_rom_pub_bridge(appl_id, flags, &ap);
	va_end(ap);

	return rv;
}


/* ****************************************************************************
 * Function name      : pub2sec_dispatcher
 * Description        : common dispatcher, for process context only.
 * Return value       : uint32
 *                      ==0 operation successful
 *                      failure otherwise.
 * ***************************************************************************/
uint32_t pub2sec_dispatcher(uint32_t appl_id, uint32_t flags, ...)
{
	uint32_t pub_cpsr, rv = 0;
	va_list ap;

	BUG_ON(in_atomic()); /* escalate wrong context. */
	mutex_lock(&disp_mutex);

	/* Read current CPSR */
	__asm__ __volatile__("MRS %0, CPSR" : "=r"(pub_cpsr));

	/* FIQ won't be enabled in secure mode if FIQ is currently disabled */
	if (pub_cpsr & FIQ_MASK)
		flags &= ~SEC_ROM_FIQ_ENABLE_MASK;

	/* IRQ won't be enabled in secure mode if IRQ is currently disabled */
	if (pub_cpsr & IRQ_MASK)
		flags &= ~SEC_ROM_IRQ_ENABLE_MASK;

	va_start(ap, flags);
	/* 'Address of va_list' convention is inherited from prev. projects */
	rv = hw_sec_rom_pub_bridge(appl_id, flags, &ap);
	va_end(ap);

	mutex_unlock(&disp_mutex);
	return rv;
}


/* MMU */
void* hw_mmu_physical_address_get(void* arg)
{
	va_list *va_ptr = (va_list *)arg;
	va_ptr->__ap = (void *)virt_to_phys((void *)va_ptr->__ap);
	return (void*)virt_to_phys(arg);
}


/* D-Cache */
/* < Clean D cache(s) */
#define HW_ARM_DCACHE_OP_CLEAN                0
/* < Invalidate D cache(s) */
#define HW_ARM_DCACHE_OP_INVALIDATE           1
/* < Clean & invalidate D cache(s) */
#define HW_ARM_DCACHE_OP_CLEAN_INVALIDATE     2
/* < Operate up to point of unification level (meaning L1 in R_Mobile_U) */
#define HW_ARM_DCACHE_LEVEL_POU               0
void hw_arm_dcache_maintenance(uint32_t op, uint32_t lvl)
{
	switch(op) {
	case HW_ARM_DCACHE_OP_CLEAN:
	case HW_ARM_DCACHE_OP_INVALIDATE:
	case HW_ARM_DCACHE_OP_CLEAN_INVALIDATE:
	default: break;
	}
}


/* L2-Cache */
void hw_arm_l2_cache_area_clean(void * virt_addr, int32_t size)
{
	__cpuc_flush_dcache_area(virt_addr, size);
}


/* ******************************** END ************************************ */
