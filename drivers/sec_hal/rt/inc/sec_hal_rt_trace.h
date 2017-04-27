/*
 * drivers/sec_hal/rt/inc/sec_hal_trace.h
 *
 * Copyright (c) 2010-2013, Renesas Mobile Corporation.
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
#ifndef SEC_HAL_TRACE_H
#define SEC_HAL_TRACE_H


#ifndef MODNAME
#define MODNAME "SEC_HAL"
#endif /* MODNAME */


#if (defined(SEC_HAL_TRACE_TO_KERNEL) && \
    !defined(SEC_HAL_TRACE_LOCAL_DISABLE))
/* Tracing macros for kernel space tracing.*/
#include <linux/kernel.h>
#include <linux/sched.h>

/* use at entry point, outside of main function */
#define SEC_HAL_TRACE_BUFFER_SIZE 128
#define SEC_HAL_TRACE_DEF_GLOBALS
#define SEC_HAL_TRACE_INIT() do{}while(0)
#define SEC_HAL_TRACE_ALLOC(val, lit) const char* val = lit;
#define SEC_HAL_TRACE_LEVEL1 KERN_INFO
#define SEC_HAL_TRACE_LEVEL2 KERN_INFO
#define SEC_HAL_TRACE_LEVEL3 KERN_INFO
#define SEC_HAL_TRACE_FUNCTION printk

#define SEC_HAL_TRACE_ENTRY() \
do{SEC_HAL_TRACE_FUNCTION(SEC_HAL_TRACE_LEVEL1 "%s[%i,%i]:%s>\n",\
   MODNAME, current->tgid, current->pid, __FUNCTION__);}while(0)

#define SEC_HAL_TRACE_EXIT() \
do{SEC_HAL_TRACE_FUNCTION(SEC_HAL_TRACE_LEVEL1 "%s[%i,%i]:%s<\n",\
   MODNAME, current->tgid, current->pid, __FUNCTION__);}while(0)

#define SEC_HAL_TRACE_EXIT_INFO(fmt, ...) \
do{SEC_HAL_TRACE_ALLOC(k_val, fmt)\
   char newfmt[SEC_HAL_TRACE_BUFFER_SIZE] = {0};\
   sprintf(newfmt, SEC_HAL_TRACE_LEVEL2 "%s[%i,%i]:%s< %s\n",\
   MODNAME, current->tgid, current->pid, __FUNCTION__, k_val);\
   SEC_HAL_TRACE_FUNCTION(newfmt, ##__VA_ARGS__);}while(0)

#define SEC_HAL_TRACE(fmt, ...) \
do{SEC_HAL_TRACE_ALLOC(k_val, fmt)\
   char newfmt[SEC_HAL_TRACE_BUFFER_SIZE] = {0};\
   sprintf(newfmt, SEC_HAL_TRACE_LEVEL2 "%s[%i,%i]:%s: %s\n",\
   MODNAME, current->tgid, current->pid, __FUNCTION__, k_val);\
   SEC_HAL_TRACE_FUNCTION(newfmt, ##__VA_ARGS__);}while(0)

#define SEC_HAL_TRACE_INT(lit, integer) \
do{SEC_HAL_TRACE_ALLOC(k_val, lit)\
   SEC_HAL_TRACE_FUNCTION(SEC_HAL_TRACE_LEVEL2 "%s[%i,%i]:%s: %s == %d\n",\
   MODNAME, current->tgid, current->pid, __FUNCTION__,\
   k_val, integer);}while(0)

#define SEC_HAL_TRACE_HEX(lit, hex) \
do{SEC_HAL_TRACE_ALLOC(k_val, lit)\
   SEC_HAL_TRACE_FUNCTION(SEC_HAL_TRACE_LEVEL2 "%s[%i,%i]:%s: %s == 0x%X\n",\
   MODNAME, current->tgid, current->pid, __FUNCTION__, k_val, hex);}while(0)

#define SEC_HAL_TRACE_STR(lit, str) \
do{SEC_HAL_TRACE_ALLOC(k_val, lit)\
   SEC_HAL_TRACE_FUNCTION(SEC_HAL_TRACE_LEVEL2 "%s[%i,%i]:%s: %s == %s\n",\
   MODNAME, current->tgid, current->pid, __FUNCTION__, k_val, str);}while(0)

uint32_t sec_trace_msg_to_string(const void *msg, char *out_str);
#define SEC_HAL_TRACE_SECMSG(msg_ptr, res) \
do{char sec_hal_trace_buffer[SEC_HAL_TRACE_BUFFER_SIZE];\
   sprintf(sec_hal_trace_buffer, "SEC_MSG:");\
   res = sec_trace_msg_to_string(msg_ptr, sec_hal_trace_buffer+8);\
   SEC_HAL_TRACE_FUNCTION("%s\n", sec_hal_trace_buffer);}while(0)

#elif(defined(SEC_HAL_TRACE_TO_STDOUT) && \
     !defined(SEC_HAL_TRACE_LOCAL_DISABLE))
/* Tracing macros for workstation env. */
#include <stdio.h>
#include <string.h>

/* use at entry point, outside of main function */
#define SEC_HAL_TRACE_BUFFER_SIZE 128
#define SEC_HAL_TRACE_DEF_GLOBALS
#define SEC_HAL_TRACE_INIT() do{}while(0)
#define SEC_HAL_TRACE_ALLOC(val, lit) const char* val = lit;
#define SEC_HAL_TRACE_LEVEL1
#define SEC_HAL_TRACE_LEVEL2
#define SEC_HAL_TRACE_LEVEL3
#define SEC_HAL_TRACE_FUNCTION printf

#define SEC_HAL_TRACE_ENTRY \
do{char sec_hal_trace_buffer[SEC_HAL_TRACE_BUFFER_SIZE];\
   sprintf(sec_hal_trace_buffer, "%s>\n", __FUNCTION__);\
   SEC_HAL_TRACE_FUNCTION("%s", sec_hal_trace_buffer);}while(0)

#define SEC_HAL_TRACE_EXIT \
do{char sec_hal_trace_buffer[SEC_HAL_TRACE_BUFFER_SIZE];\
   sprintf(sec_hal_trace_buffer, "%s<\n", __FUNCTION__);\
   SEC_HAL_TRACE_FUNCTION("%s", sec_hal_trace_buffer);}while(0)

#define SEC_HAL_TRACE_EXIT_INFO(fmt) \
do{SEC_HAL_TRACE_ALLOC(k_val, fmt)\
   char sec_hal_trace_buffer[SEC_HAL_TRACE_BUFFER_SIZE];\
   sprintf(sec_hal_trace_buffer, "%s< %s\n", __FUNCTION__, k_val);\
   SEC_HAL_TRACE_FUNCTION("%s", sec_hal_trace_buffer);}while(0)

#define SEC_HAL_TRACE(fmt) \
do{SEC_HAL_TRACE_ALLOC(k_val, fmt)\
   char sec_hal_trace_buffer[SEC_HAL_TRACE_BUFFER_SIZE];\
   sprintf(sec_hal_trace_buffer, "%s: %s\n", __FUNCTION__, k_val);\
   SEC_HAL_TRACE_FUNCTION("%s", sec_hal_trace_buffer);}while(0)

#define SEC_HAL_TRACE_INT(lit, integer) \
do{SEC_HAL_TRACE_ALLOC(k_val, lit)\
   char sec_hal_trace_buffer[SEC_HAL_TRACE_BUFFER_SIZE];\
   sprintf(sec_hal_trace_buffer, "%s: %s == %d\n", \
   __FUNCTION__, k_val, integer);\
   SEC_HAL_TRACE_FUNCTION("%s", sec_hal_trace_buffer);}while(0)

#define SEC_HAL_TRACE_HEX(lit, hex) \
do{SEC_HAL_TRACE_ALLOC(k_val, lit)\
   char sec_hal_trace_buffer[SEC_HAL_TRACE_BUFFER_SIZE];\
   sprintf(g_sec_hal_trace_buffer, "%s: %s == 0x%X\n",\
   __FUNCTION__, k_val, hex);\
   SEC_HAL_TRACE_FUNCTION("%s", sec_hal_trace_buffer);}while(0)

#define SEC_HAL_TRACE_STR(lit, str) \
do{SEC_HAL_TRACE_ALLOC(k_val, lit)\
   char sec_hal_trace_buffer[SEC_HAL_TRACE_BUFFER_SIZE];\
   sprintf(sec_hal_trace_buffer, "%s: %s == %s\n",\
   __FUNCTION__, k_val, str);\
   SEC_HAL_TRACE_FUNCTION("%s", sec_hal_trace_buffer);}while(0)

uint32_t sec_trace_msg_to_string(const void *msg, char *out_str);
#define SEC_HAL_TRACE_SECMSG(msg_ptr, res) \
do{char sec_hal_trace_buffer[SEC_HAL_TRACE_BUFFER_SIZE];\
   sprintf(sec_hal_trace_buffer, "SEC_MSG:");\
   res = sec_trace_msg_to_string(msg_ptr, sec_hal_trace_buffer+8);\
   SEC_HAL_TRACE_FUNCTION("%s\n", sec_hal_trace_buffer);}while(0)

#else /* (defined SEC_HAL_TRACE_TO_KERNEL) */

#define SEC_HAL_TRACE_BUFFER_SIZE 0
#define SEC_HAL_TRACE_DEF_GLOBALS
#define SEC_HAL_TRACE_INIT()
#define SEC_HAL_TRACE_ALLOC(val, lit)
#define SEC_HAL_TRACE_LEVEL1
#define SEC_HAL_TRACE_LEVEL2
#define SEC_HAL_TRACE_LEVEL3
#define SEC_HAL_TRACE_FUNCTION
#define SEC_HAL_TRACE_ENTRY() do{}while(0)
#define SEC_HAL_TRACE_EXIT() do{}while(0)
#define SEC_HAL_TRACE_EXIT_INFO(fmt, ...) do{}while(0)
#define SEC_HAL_TRACE(fmt, ...) do{}while(0)
#define SEC_HAL_TRACE_INT(lit, integer) do{}while(0)
#define SEC_HAL_TRACE_HEX(lit, hex) do{}while(0)
#define SEC_HAL_TRACE_STR(lit, str) do{}while(0)
#define SEC_HAL_TRACE_SECMSG(msg_ptr, res)

#endif /* (defined SEC_HAL_TRACE_TO_KERNEL) */


#endif /* SEC_HAL_TRACE_H_ */
