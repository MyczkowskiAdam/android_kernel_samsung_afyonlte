/*
 * include/memlog/memlog.h
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
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
#ifndef __MEMLOG_H__
#define __MEMLOG_H__
#include <mach/memory-r8a7373.h>

#ifdef CONFIG_MEMLOG
#ifndef __ASSEMBLY__
extern unsigned long memlog_capture;
extern void memory_log_proc(const char *name, unsigned long pid);
extern void memory_log_worker(unsigned long func_addr, unsigned long pid
		, int in);
extern void memory_log_irq(unsigned int irq, int in);
extern void memory_log_func(unsigned long func_id, int in);
extern void memory_log_dump_int(unsigned char dump_id, int dump_data);
extern void memory_log_timestamp(unsigned int id);

struct memlog_header {
	unsigned char proc_size;
	unsigned char proc_count;
	unsigned char irq_size;
	unsigned char irq_count;
	unsigned char func_size;
	unsigned char func_count;
	unsigned char dump_size;
	unsigned char dump_count;
	unsigned char timestamp_size;
	unsigned char timestamp_count;
	unsigned char reserved1;
	unsigned char reserved2;
} __packed;

struct proc_log_entry {
	unsigned long long time;
	unsigned long pid;
	unsigned long data[4];
} __packed;

struct irq_log_entry {
	unsigned long long time;
	unsigned long data;
} __packed;

struct func_log_entry {
	unsigned long long time;
	unsigned long data;
} __packed;

struct dump_log_entry {
	unsigned long long time;
	unsigned long id;
	int data;
} __packed;

enum timestamps {
	CMT15_TIMESTAMP = 0,
	RWDT_TIMESTAMP,
	FIQ_TIMESTAMP,
	COUNT_TIMESTAMP,
};
struct timestamp_entries {
	unsigned long long time[CONFIG_NR_CPUS][COUNT_TIMESTAMP];
} __packed;
#endif /* __ASSEMBLY__ */

#define MEMLOG_ADDRESS		SDRAM_MEMLOG_START_ADDRESS
#define MEMLOG_SIZE		\
	(SDRAM_MEMLOG_END_ADDRESS - SDRAM_MEMLOG_START_ADDRESS + 1)

#define MEMLOG_HEADER_SIZE	sizeof(struct memlog_header)
#define PROC_ENTRY_SIZE		sizeof(struct proc_log_entry)
#define IRQ_ENTRY_SIZE		sizeof(struct irq_log_entry)
#define FUNC_ENTRY_SIZE		sizeof(struct func_log_entry)
#define DUMP_ENTRY_SIZE		sizeof(struct dump_log_entry)
#define PM_ENTRY_SIZE		8 /* 32bit timer & 32 bit id */
#define CPU0_PM_SIZE		PM_ENTRY_SIZE
#define CPU1_PM_SIZE		PM_ENTRY_SIZE
#define TIMESTAMPS_SIZE		sizeof(struct timestamp_entries)

#define PROC_COUNT		18
#define IRQ_COUNT		42
#define FUNC_COUNT		42
#define DUMP_COUNT		31
#define PM_COUNT		1

#define CPU0_PROC_START_INDEX	MEMLOG_HEADER_SIZE
#define CPU1_PROC_START_INDEX	\
	(CPU0_PROC_START_INDEX + (PROC_ENTRY_SIZE * PROC_COUNT))
#define CPU0_IRQ_START_INDEX	\
	(CPU1_PROC_START_INDEX + (PROC_ENTRY_SIZE * PROC_COUNT))
#define CPU1_IRQ_START_INDEX	\
	(CPU0_IRQ_START_INDEX + (IRQ_ENTRY_SIZE * IRQ_COUNT))
#define CPU0_FUNC_START_INDEX	\
	(CPU1_IRQ_START_INDEX + (IRQ_ENTRY_SIZE * IRQ_COUNT))
#define CPU1_FUNC_START_INDEX	\
	(CPU0_FUNC_START_INDEX + (FUNC_ENTRY_SIZE * FUNC_COUNT))
#define CPU0_DUMP_START_INDEX	\
	(CPU1_FUNC_START_INDEX + (FUNC_ENTRY_SIZE * FUNC_COUNT))
#define CPU1_DUMP_START_INDEX	\
	(CPU0_DUMP_START_INDEX + (DUMP_ENTRY_SIZE * DUMP_COUNT))
#define CPU0_PM_START_INDEX	\
	(CPU1_DUMP_START_INDEX + (DUMP_ENTRY_SIZE * DUMP_COUNT))
#define CPU1_PM_START_INDEX	\
	(CPU0_PM_START_INDEX + (PM_ENTRY_SIZE * PM_COUNT))
#define TIMESTAMP_INDEX		\
	(CPU1_PM_START_INDEX + (PM_ENTRY_SIZE * PM_COUNT))
#define MEMLOG_END		\
	(TIMESTAMP_INDEX + TIMESTAMPS_SIZE)

#else
#ifndef __ASSEMBLY__
static inline void memory_log_proc(const char *name, unsigned long pid)
{
}
static inline void memory_log_worker(unsigned long func_addr, unsigned long pid
		, int in)
{
}
static inline void memory_log_irq(unsigned int irq, int in)
{
}
static inline void memory_log_func(unsigned long func_id, int in)
{
}
static inline void memory_log_dump_int(unsigned char dump_id, int dump_data)
{
}
static inline void memory_log_timestamp(unsigned int id)
{
}
#endif /* __ASSEMBLY__ */
#endif

#define IRQ_LOG_ENTRY_IN					0x01000000
#define FUNC_LOG_ENTRY_IN					0x00000001
#define PM_FUNC_ID_START_WFI					0x00000010
#define PM_FUNC_ID_START_WFI2					0x00000020
#define PM_FUNC_ID_START_CORESTANDBY				0x00000030
#define PM_FUNC_ID_START_CORESTANDBY2				0x00000040
#define PM_FUNC_ID_JUMP_SYSTEMSUSPEND				0x00000050

#define PM_FUNC_ID_EARLY_SUSPEND				0x00001060
#define PM_FUNC_ID_LATE_RESUME					0x00001070

#define PM_FUNC_ID_DPM_PREPARE					0x00002010
#define PM_FUNC_ID_DPM_SUSPEND					0x00002020
#define PM_FUNC_ID_DPM_SUSPEND_NOIRQ				0x00002030
#define PM_FUNC_ID_DPM_RESUME_NOIRQ				0x00002040
#define PM_FUNC_ID_DPM_RESUME					0x00002050
#define PM_FUNC_ID_DPM_COMPLETE					0x00002060

#define PM_FUNC_ID_SHMOBILE_SUSPEND_BEGIN			0x00003000
#define PM_FUNC_ID_SHMOBILE_SUSPEND_END				0x00003010
#define PM_FUNC_ID_SHMOBILE_SUSPEND_ENTER			0x00003020
#define PM_FUNC_ID_SHMOBILE_SUSPEND_PREPARE			0x00003030
#define PM_FUNC_ID_SHMOBILE_SUSPEND_PREPARE_LATE		0x00003040
#define PM_FUNC_ID_SHMOBILE_SUSPEND_WAKE			0x00003050

#define PM_FUNC_ID_SEC_HAL_PM_COMA_ENTRY			0x00004000
#define PM_FUNC_ID_PUB2SEC_DISPATCHER				0x00004010
#define PM_FUNC_ID_HW_SEC_ROM_PUB_BRIDGE			0x00004020

#define PM_DUMP_ID_DFS_FREQ					0x000001
#define PM_DUMP_ID_DFS_MINMAX_FREQ				0x000002
#define PM_DUMP_ID_ZB3DFS_FREQ_REQ				0x000003
#define PM_DUMP_ID_ZB3DFS_FREQ					0x000004

#define PM_PM_ID_ARMVECTOR					0x000001
#define PM_PM_ID_SUSPEND_IN					0x000002
#define PM_PM_ID_SUSPEND_OUT					0x000003
#define PM_PM_ID_CORESTANDBY_IN					0x000004
#define PM_PM_ID_CORESTANDBY_OUT				0x000005
#define PM_PM_ID_CORESTANDBY2_IN				0x000006
#define PM_PM_ID_CORESTANDBY2_OUT				0x000007
#define PM_PM_ID_HOTPLUG_IN					0x000008
#define PM_PM_ID_HOTPLUG_OUT					0x000009

#define PM_DUMP_ID_SET_SBSC_FREQ_ZB3_LOCK			0x000010
#define PM_DUMP_ID_SET_SBSC_FREQ_ZB3_UNLOCK			0x000011
#define PM_DUMP_ID_SET_CPU_FREQ_RETRY				0x000020
#define PM_DUMP_ID_SET_CPU_FREQ_ZB3_LOCK			0x000021
#define PM_DUMP_ID_SET_CPU_FREQ_ZB3_UNLOCK_1			0x000022
#define PM_DUMP_ID_SET_CPU_FREQ_ZB3_UNLOCK_2			0x000023
#define PM_DUMP_ID_SET_CPU_FREQ_ZB3_UNLOCK_3			0x000024
#define PM_DUMP_ID_SET_CPU_FREQ_ZB3_UNLOCK_4			0x000025
#define PM_DUMP_ID_SET_CPU_FREQ_ZS_LOCK				0x000026
#define PM_DUMP_ID_SET_CPU_FREQ_ZS_UNLOCK_1			0x000027
#define PM_DUMP_ID_SET_CPU_FREQ_ZS_UNLOCK_2			0x000028

#define PM_DUMP_ID_SUSPEND_SET_CLOCK_RETRY_1			0x000050
#define PM_DUMP_ID_SUSPEND_SET_CLOCK_RETRY_2			0x000051

#define PM_DUMP_ID_RWDT_COUNTER					0x0000E0

#define PM_DUMP_ID_SET_SBSC_FREQ_ZB3_LOCK_ERR			0x0000F0
#define PM_DUMP_ID_SET_CPU_FREQ_ZB3_LOCK_ERR			0x0000F1
#define PM_DUMP_ID_SET_CPU_FREQ_ZS_LOCK_ERR			0x0000F2
#define DMA_FUNC_ID_LD_CLEANUP 					0x0000F3
#define DMA_FUNC_ID_PENDING					0x0000F4
#define DMA_FUNC_ID_DO_TASKLET 					0x0000F5
#define DMA_FUNC_LD_QUEUE					0x0000F6	
#define DMA_START 						1
#define DMA_END							0
#define PM_DUMP_START	1
#define PM_DUMP_END	0
#endif
