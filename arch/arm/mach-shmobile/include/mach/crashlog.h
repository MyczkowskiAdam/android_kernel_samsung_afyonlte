/*
 * arch/arm/mach-shmobile/include/mach/crashlog.h
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef __ARCH_SHMOBILE_CRASHLOG_H__
#define __ARCH_SHMOBILE_CRASHLOG_H__

#include <mach/memory-r8a7373.h>

#define RMC_LOCAL_VERSION "150612"              /* ddmmyy (release time)*/

#define TMPLOG_ADDRESS	(SDRAM_CRASHLOG_START_ADDR + 0x200)
#define TMPLOG_END_ADDRESS     (TMPLOG_ADDRESS + 0x40000)
#define TMPLOG_TOTAL_SIZE      (TMPLOG_END_ADDRESS - TMPLOG_ADDRESS)


#define CRASHLOG_R_LOCAL_VER_LOCATE             (SDRAM_CRASHLOG_START_ADDR + 0)
#define CRASHLOG_R_LOCAL_VER_LENGTH             32

#define CRASHLOG_KMSG_LOCATE		(SDRAM_CRASHLOG_START_ADDR + 0x20)
#define CRASHLOG_LOGCAT_MAIN_LOCATE	(SDRAM_CRASHLOG_START_ADDR + 0x30)
#define CRASHLOG_LOGCAT_EVENT_LOCATE	(SDRAM_CRASHLOG_START_ADDR + 0x40)
#define CRASHLOG_LOGCAT_RADIO_LOCATE	(SDRAM_CRASHLOG_START_ADDR + 0x50)
#define CRASHLOG_LOGCAT_SYSTEM_LOCATE	(SDRAM_CRASHLOG_START_ADDR + 0x60)

extern unsigned long log_buf_address;
extern unsigned long log_buf_len_address;
extern unsigned long log_end_address;
extern unsigned long logged_chars_address;

extern unsigned long log_main_buffer_address;
extern unsigned long log_main_size_address;
extern unsigned long log_main_w_off_address;
extern unsigned long log_main_head_address;

extern unsigned long log_events_buffer_address;
extern unsigned long log_events_size_address;
extern unsigned long log_events_w_off_address;
extern unsigned long log_events_head_address;

extern unsigned long log_radio_buffer_address;
extern unsigned long log_radio_size_address;
extern unsigned long log_radio_w_off_address;
extern unsigned long log_radio_head_address;

extern unsigned long log_system_buffer_address;
extern unsigned long log_system_size_address;
extern unsigned long log_system_w_off_address;
extern unsigned long log_system_head_address;

extern void crashlog_kmsg_init(void);
extern void crashlog_logcat_init(void);

void crashlog_r_local_ver_write(char *soft_version);
void crashlog_reset_log_write(void);
void crashlog_init_tmplog(void);
#endif /* __ARCH_SHMOBILE_CRASHLOG_H__ */
