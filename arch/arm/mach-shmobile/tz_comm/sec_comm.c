/*
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
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>

#include "stdarg.h"
#include "stdbool.h"
#include "sec_hal_rt.h"

/*#include "sec_hal_rt_cmn.h"*/
/*
There should be no need for these, rt interface to be used. (Maybe later to be
split so only certain functions allowed for direct call)
#include "sec_hal_rt_trace.h"
#include "sec_serv_api.h"
#include "sec_dispatch.h"
*/

//#include "arm.h"
//#include "cache_mmu_if.h"
//#include "ssa.h"
//#include "sec_dispatch.h"
//#include "sec_msg.h"
//#include "sec_serv_api.h"

#include <asm/memory.h>
#include <asm/sec_comm.h>

#define PAGE_OFFSET UL(CONFIG_PAGE_OFFSET)
//#define PLAT_PHYS_OFFSET UL(CONFIG_PAGE_OFFSET)


uint32_t  sec_service_integrity_check(void);
#if 0
/*no need for rpc here*/
static uint32_t sec_comm_rpc_handler(uint32_t id, sec_msg_t * out_msg, sec_msg_t * in_msg);
static uint32_t _set_rpc_handler(uint32_t (*newhandler)(uint32_t, sec_msg_t*, sec_msg_t*));
#endif

/* void  hw_arm_dcache_maintenance(uint32_t operation, uint32_t level)
{


}

void * hw_mmu_physical_address_get(const volatile void* arg)
{
	return ((void*)((arg)-PAGE_OFFSET+PLAT_PHYS_OFFSET));
}
*/


#if 0
uint32_t set_rpc_handler(void)
{
  return _set_rpc_handler(sec_comm_rpc_handler);
}
#endif


#if 0
uint32_t sec_l2_cache_enable()
{
  sec_msg_handle_t h_req;


  sec_msg_t * q;
  sec_msg_t * p = 0;

printk("sec_l2_cache_enable ++ \n");
printk(" Malloc param_sz = %d \n",sec_msg_param_size(sizeof(uint32_t)));

  q = sec_msg_alloc(&h_req,
                    sec_msg_param_size(sizeof(uint32_t))
// in case of additional parameters, just add corresponding sec_msg_param_size()'s
                    ,
                    SEC_MSG_OBJECT_ID_NONE,
                    0,
                    SEC_MSG_BYTE_ORDER_LE);


// In this case data is not returned by the secure environment, the return value of
// _hw_sec_rom_pub_dispatcher tells the status.
//
//  p = sec_msg_alloc(&h_resp,
//                    ...,
//                    SEC_MSG_OBJECT_ID_NONE,
//                    0,
//                    SEC_MSG_BYTE_ORDER_LE);


   printk("Calling sec api write input buf 0x%x\n",q);
  sec_msg_param_write32(&h_req,
                        (uint32_t)true,
                        SEC_MSG_PARAM_ID_NONE);


  uint32_t rv = sec_dispatcher(SEC_SERV_L2_CACHE_CONTROL,
                                           0,   //
                                           0,   // spare
                                           p,   // pointer to response (output) message
                                           q);  // pointer to input message


//  sec_msg_free(q);
//sec_msg_free(p);
printk("sec_l2_cache_enable --i");
  return rv;
}
#endif

uint32_t  sec_service_integrity_check()
{
      uint32_t retval;
      printk("sec_service_integrity_check ++ \n");
      sec_hal_rt_periodic_integrity_check(&retval);
      printk("sec_hal_rt_periodic_integrity_check returned %d \n",retval);
	return retval;
}

#if 0
static uint32_t sec_comm_rpc_handler(uint32_t id, sec_msg_t * out_msg, sec_msg_t * in_msg)
{
  (void)id;
  (void)out_msg;
  (void)in_msg;

  return 0;
}
#endif

#if 0
static uint32_t _set_rpc_handler(uint32_t (*newhandler)(uint32_t, sec_msg_t*, sec_msg_t*))
{
  sec_msg_handle_t h_req;
//sec_msg_handle_t h_resp;

  sec_msg_t * q;
  sec_msg_t * p = 0;

  q = sec_msg_alloc(&h_req,
                    sec_msg_param_size(sizeof(uint32_t (*)(uint32_t, sec_msg_t*, sec_msg_t*)))
// in case of additional parameters, just add corresponding sec_msg_param_size()'s
                    ,
                    SEC_MSG_OBJECT_ID_NONE,
                    0,
                    SEC_MSG_BYTE_ORDER_LE);

// In this case data is not returned by the secure environment, the return value of
// _hw_sec_rom_pub_dispatcher tells the status.
//
//  p = sec_msg_alloc(&h_resp,
//                    ...,
//                    SEC_MSG_OBJECT_ID_NONE,
//                    0,
//                    SEC_MSG_BYTE_ORDER_LE);

  sec_msg_param_write32(&h_req,
                        (uint32_t)SEC_HAL_MEM_VIR2PHY_FUNC((void*)sec_comm_rpc_handler),
                        SEC_MSG_PARAM_ID_NONE);

  uint32_t rv = sec_dispatcher(SEC_SERV_RPC_ADDRESS,
                                           0,   //
                                           0,   // spare
                                           p,   // pointer to response (output) message
                                           q);  // pointer to input message

  sec_msg_free(q);
//sec_msg_free(p);

  return rv;
}
#endif



// EOF
