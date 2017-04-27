/*
 * Delay loops based on the OpenRISC implementation.
 *
 * Copyright (C) 2012 ARM Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/timex.h>

/*
 * Default to the loop-based delay implementation.
 */
struct arm_delay_ops arm_delay_ops = {
	.delay		= __loop_delay,
	.const_udelay	= __loop_const_udelay,
	.udelay		= __loop_udelay,
};

static struct delay_timer *delay_timer;
static bool delay_calibrated;

/*
 * Oh, if only we had a cycle counter...
 */
static void delay_loop(unsigned long loops)
{
	asm volatile(
	"1:	subs %0, %0, #1 \n"
	"	bhi 1b		\n"
	: /* No output */
	: "r" (loops)
	);
}


int read_current_timer(unsigned long *timer_val)
{
	return delay_timer ? delay_timer->read_current_timer(timer_val) : -ENXIO;
}

#ifdef ARCH_HAS_READ_CURRENT_TIMER
/*
 * Assumes read_current_timer() is monotonically increasing
 * across calls and wraps at most once within MAX_UDELAY_MS.
 */
void read_current_timer_delay_loop(unsigned long loops)
{
	unsigned long bclock, now;

	read_current_timer(&bclock);
	do {
		read_current_timer(&now);
	} while ((now - bclock) < loops);
}
#endif

void (*delay_fn)(unsigned long) = delay_loop;


static void __timer_delay(unsigned long cycles)
{
	cycles_t start = get_cycles();

	while ((get_cycles() - start) < cycles)
		cpu_relax();
}

static void __timer_const_udelay(unsigned long xloops)
{
	unsigned long long loops = xloops;
	loops *= loops_per_jiffy;
	__timer_delay(loops >> UDELAY_SHIFT);
}

static void __timer_udelay(unsigned long usecs)
{
	__timer_const_udelay(usecs * UDELAY_MULT);
}

void __init register_current_timer_delay(struct delay_timer *timer)
{
	if (!delay_calibrated) {
		pr_info("Switching to timer-based delay loop (freq=%lu)\n", timer->freq);
		delay_timer			= timer;
		lpj_fine			= (timer->freq + HZ/2) / HZ;
		loops_per_jiffy			= lpj_fine;
		arm_delay_ops.delay		= __timer_delay;
		arm_delay_ops.const_udelay	= __timer_const_udelay;
		arm_delay_ops.udelay		= __timer_udelay;
		delay_calibrated		= true;
	} else {
		pr_info("Ignoring duplicate/late registration of read_current_timer delay\n");
	}
}

unsigned long __cpuinit calibrate_delay_is_known(void)
{
	delay_calibrated = true;
	return lpj_fine;
}
