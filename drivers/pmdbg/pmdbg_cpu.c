/*
 * pmdbg_cpu.c
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel_stat.h>
#include <mach/pm.h>
#include "pmdbg_api.h"
#include "pmdbg_hw.h"
#include "pmdbg_cpu.h"
#include <asm/page.h>

static struct kernel_cpustat prev_usage[PMDBG_MAX_CPUS];
static int mon_count;
static int mon_dcount;
static cputime64_t prev_time;
static cputime64_t start_time;
static char outbuf[4096];
static int outbuf_len ;

static char pmdbg_enable_cpu_profile;

LOCAL_DECLARE_MOD(cpu, pmdbg_cpu_init, pmdbg_cpu_exit);

DECLARE_CMD(enable_cpu_profile, enable_cpu_profile_cmd);
DECLARE_CMD(disable_cpu_profile, disable_cpu_profile_cmd);

int pmdbg_cpu_init(void)
{
	FUNC_MSG_IN;
	pmdbg_enable_cpu_profile = 0;

	ADD_CMD(cpu, enable_cpu_profile);
	ADD_CMD(cpu, disable_cpu_profile);

	FUNC_MSG_RET(0);
}

void pmdbg_cpu_exit(void)
{
	FUNC_MSG_IN;
	DEL_CMD(cpu, disable_cpu_profile);
	DEL_CMD(cpu, enable_cpu_profile);

	FUNC_MSG_OUT;
}

static int percent100(cputime64_t val, cputime64_t period)
{
	return (int)(100*(long)val/(long)period);
}

static int getfreq(void)
{
	unsigned int mul;
	unsigned int div;
	unsigned int z_clk;
	unsigned int freqcrb;
	unsigned int scgcr;
	unsigned int step;

	mul = ((rreg32(CPG_PLL0CR) & PLLCR_STC_MASK) >> PLLCR_BIT24_SHIFT) + 1;
	freqcrb = rreg32(CPG_FRQCRB);
	div = (freqcrb & FRQCRB_ZFC_MASK) >> FRQCRB_BIT24_SHIFT;
	scgcr = rreg32(CPG_SCGCR);

	if ((freqcrb & FRQCRB_ZSEL_MASK) == ZFC_RATIO_1_1) {
		div = 1;
	} else {
		switch (div) {
		case 0:
			div = 2;	break;
		case 1:
			div = 3;	break;
		case 2:
			div = 4;	break;
		case 3:
			div = 6;	break;
		case 4:
			div = 8;	break;
		case 5:
			div = 12;	break;
		case 6:
			div = 16;	break;
		case 8:
			div = 24;	break;
		case 11:
			div = 48;	break;
		default:
			div = 1;	break;
		}
	}

	z_clk = (26 * mul) / div;

	if ((scgcr & SCGCR_MANUAL_MASK) == SCGCR_ENABLE_MCLK) {
		if ((scgcr & SCGCR_CKMODE_MASK) == SCGCR_GATING_MODE) {
			switch ((scgcr & SCGCR_WAVEFORM_MASK)) {
			case 0x0000FFFF:
				step = 16;	break;
			case 0x0000FFFE:
				step = 15;	break;
			case 0x0000FEFE:
				step = 14;	break;
			case 0x0000FEEE:
				step = 13;	break;
			case 0x0000EEEE:
				step = 12;	break;
			case 0x0000EEEA:
				step = 11;	break;
			case 0x0000EAEA:
				step = 10;	break;
			case 0x0000EAAA:
				step =  9;	break;
			case 0x0000AAAA:
				step =  8;	break;
			case 0x0000AA54:
				step =  7;	break;
			case 0x0000A4A4:
				step =  6;	break;
			case 0x00009248:
				step =  5;	break;
			case 0x00008888:
				step =  4;	break;
			case 0x00008420:
				step =  3;	break;
			case 0x00008080:
				step =  2;	break;
			case 0x00008000:
				step =  1;	break;
			default:
				step = 16;	break;
			}
		} else if ((scgcr & SCGCR_CKMODE_MASK) == SCGCR_GENERATE_MODE) {
			switch ((scgcr & SCGCR_WAVEFORM_MASK)) {
			case 0x0000AAAA:
				step =  8;	break;
			case 0x0000AB56:
				step =  7;	break;
			case 0x0000B6B6:
				step =  6;	break;
			case 0x0000DB6C:
				step =  5;	break;
			case 0x0000CCCC:
				step =  4;	break;
			case 0x0000E738:
				step =  3;	break;
			case 0x0000F0F0:
				step =  2;	break;
			case 0x0000FF00:
				step =  1;	break;
			default:
				step = 16;	break;
			}
		} else {
			step = 16;
		}
		z_clk = (z_clk * step) / 16;
	}
	return z_clk;
}

void pmdbg_mon(int cpum, unsigned int max_load,
		unsigned int load0, unsigned int load1,
		unsigned int cur, unsigned int req)
{
	unsigned int cpu;
	cputime64_t cur_time, delta_time;
	cputime64_t busy_time[PMDBG_MAX_CPUS];
	struct kernel_cpustat delta_usage[PMDBG_MAX_CPUS];
	static int freq;
	static int cur_time_msec;
	unsigned int usage;

	cur_time = jiffies64_to_cputime64(get_jiffies_64());

	if (start_time == 0) {
		start_time = cur_time;
		for (cpu = 0; cpu < PMDBG_MAX_CPUS; cpu++) {
			for (usage = CPUTIME_USER; usage < NR_STATS; usage++)
				prev_usage[cpu].cpustat[usage] =
					kcpustat_cpu(cpu).cpustat[usage];
		}
		prev_time = cur_time;
		return;
	}

	cur_time_msec = jiffies_to_msecs((long)cur_time - (long)start_time);
	if (cur_time_msec < MON_WAIT_TIME)
		return;

	freq = getfreq();
	sprintf(bufres, "MONF%d: %8d %3d %3d %3d %3d %3d %3d",
		cpum,
		cur_time_msec,
		max_load,
		load0,
		load1,
		freq,
		cur,
		req);
	MSG_INFO("%s", bufres);
	mon_count++;
	if (mon_count >= MON_PERIOD)
		mon_count = 0;
	else
		return;

	delta_time = cur_time - prev_time;
	if (delta_time == 0) {
		sprintf(bufres, "<0>MONS %8ld %8d : %8ld %8d : skip",
			(long)delta_time,
			jiffies_to_msecs(delta_time),
			(long)cur_time - (long)start_time,
			cur_time_msec);
		MSG_INFO("%s", bufres);
		return;
	}

	for (cpu = 0; cpu < PMDBG_MAX_CPUS; cpu++) {
		for (usage = CPUTIME_USER; usage < NR_STATS; usage++) {
			delta_usage[cpu].cpustat[usage] =
				kcpustat_cpu(cpu).cpustat[usage]
				- prev_usage[cpu].cpustat[usage];

			prev_usage[cpu].cpustat[usage] =
				kcpustat_cpu(cpu).cpustat[usage];
		}
		busy_time[cpu] = delta_usage[cpu].cpustat[CPUTIME_USER];
		busy_time[cpu] += delta_usage[cpu].cpustat[CPUTIME_SYSTEM];
		busy_time[cpu] += delta_usage[cpu].cpustat[CPUTIME_IRQ];
		busy_time[cpu] += delta_usage[cpu].cpustat[CPUTIME_SOFTIRQ];
		busy_time[cpu] += delta_usage[cpu].cpustat[CPUTIME_STEAL];
		busy_time[cpu] += delta_usage[cpu].cpustat[CPUTIME_NICE];
	}

	prev_time = cur_time;

	outbuf_len += sprintf(outbuf + outbuf_len,
		"<0>MON: %8ld %8d : %8ld %8d",
		(long)delta_time,
		jiffies_to_msecs(delta_time),
		(long)cur_time - (long)start_time,
		cur_time_msec);

	for (cpu = 0; cpu < PMDBG_MAX_CPUS; cpu++) {
		outbuf_len += sprintf(outbuf + outbuf_len,
		" : %8d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d",
		cur_time_msec,
		freq,
		percent100(busy_time[cpu], delta_time),
		percent100(delta_usage[cpu].cpustat[CPUTIME_USER], delta_time),
		percent100(delta_usage[cpu].cpustat[CPUTIME_NICE], delta_time),
		percent100(delta_usage[cpu].cpustat[CPUTIME_SYSTEM],
								delta_time),
		percent100(delta_usage[cpu].cpustat[CPUTIME_SOFTIRQ],
								delta_time),
		percent100(delta_usage[cpu].cpustat[CPUTIME_IRQ], delta_time),
		percent100(delta_usage[cpu].cpustat[CPUTIME_IOWAIT],
								delta_time),
		percent100(delta_usage[cpu].cpustat[CPUTIME_STEAL], delta_time),
		percent100(delta_usage[cpu].cpustat[CPUTIME_GUEST], delta_time),
		percent100(delta_usage[cpu].cpustat[CPUTIME_GUEST_NICE],
								delta_time),
		percent100(delta_usage[cpu].cpustat[CPUTIME_IDLE], delta_time));
	}

	mon_dcount++;

	if (mon_dcount >= MON_DPERIOD) {
		MSG_INFO("%s", outbuf);
		outbuf_len = 0;
		mon_dcount = 0;
	}

	return;
}

char pmdbg_get_enable_cpu_profile(void)
{
	/* enable:1, disable:0 */
	return pmdbg_enable_cpu_profile;
}

static int enable_cpu_profile_cmd(char *para, int size)
{
	char *s = bufres;

	FUNC_MSG_IN;

	pmdbg_enable_cpu_profile = 1;

	mon_count = 0;
	mon_dcount = 0;
	prev_time = 0;
	start_time = 0;
	outbuf_len = 0;

	s += sprintf(s, "CPU: CPU profile log output Enabled");
	MSG_INFO("%s", bufres);

	FUNC_MSG_RET(0);
}

static int disable_cpu_profile_cmd(char *para, int size)
{
	char *s = bufres;

	FUNC_MSG_IN;

	pmdbg_enable_cpu_profile = 0;

	s += sprintf(s, "CPU: CPU profile log output Disabled");
	MSG_INFO("%s", bufres);

	FUNC_MSG_RET(0);
}
