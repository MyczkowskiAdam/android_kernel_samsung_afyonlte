/*
arch/arm/include/asm/sec_comm.h

Copyright (C) 2013 Renesas Mobile Corporation

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#if defined _SEC_COMM_H_
#error "Multiply included"
#endif

//#define _SEC_COMM_H_

//uint32_t set_rpc_handler(void);
//uint32_t sec_l2_cache_enable(void);

extern uint32_t sec_l2_cache_enable(void);
extern int sec_hal_mem_msg_area_init(void);
uint32_t  sec_service_integrity_check(void);
extern void sec_service_rt_multicore_enable(void);
/// EOF
