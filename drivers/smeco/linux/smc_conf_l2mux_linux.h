/*
* Copyright (c) 2013, Renesas Mobile Corporation.
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

#if 0
/*
Change history:

Version:       3    25-Jan-2012     Heikki Siikaluoma
Status:        draft
Description :  L2MUX specific code added in this file.

Version:       1    04-Jan-2012     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif


#ifndef SMC_CONF_L2MUX_LINUX_H
#define SMC_CONF_L2MUX_LINUX_H

#include <linux/if_mhi.h>
#include <linux/mhi.h>
#include <linux/l2mux.h>
#include <linux/phonet.h>
#include <net/phonet/pn_dev.h>

#include "smc_linux.h"


#ifndef PN_DEV_HOST
    #define PN_DEV_HOST 0x00
#endif

#define SMC_CONF_L2MUX_LOCAL_CPU_ID  0xA9
#define SMC_CONF_L2MUX_REMOTE_CPU_ID 0xC0

    /* L2MUX control header size */
#define SMC_L2MUX_HEADER_SIZE        4

/*TODO Clean-up this (+comments from C file) #define SMC_L2MUX_HEADER_OVERHEAD    26 */    /* Overhead added to SKB while allocating */

#if( SMCTEST == TRUE )
    #define SMC_TEST_PHONET_DEVICE  0x70   /* Phonet messages to this device are routed to SMC test handler */
#endif

smc_t* get_smc_instance_l2mux( void );

#endif

/* EOF */

