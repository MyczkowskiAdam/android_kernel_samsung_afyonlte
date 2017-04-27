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

Version:       1    27-Sep-2012     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMC_CONFIG_L2MUX_H
#define SMC_CONFIG_L2MUX_H

#define SMC_CONFIG_USER_L2MUX       "L2MUX"


    /* L2MUX Queue mapping for protocols */
#define SMC_L2MUX_QUEUE_1_PHONET     0
#define SMC_L2MUX_QUEUE_2_MHI        1
#define SMC_L2MUX_QUEUE_3_MHDP       2


    /*
     * Product specific configuration header files
     *
     */

#if( SMC_CURRENT_PRODUCT_CONFIG == SMC_CONFIG_EOS2 )
    #include "smc_config_l2mux_r8a73734_wge31.h"
#elif( SMC_CURRENT_PRODUCT_CONFIG == SMC_CONFIG_EOS3 )
    #include "smc_config_l2mux_u3ca.h"
#elif( SMC_CURRENT_PRODUCT_CONFIG == SMC_CONFIG_EOS3_WGE31 )
    #include "smc_config_l2mux_u3.h"
#else
    #error "Invalid SMC product configuration"
#endif


    /*
     * Common L2MUX configuration function prototypes.
     */

smc_instance_conf_t* smc_instance_conf_get_l2mux( char* smc_user_name, char* config_name );

#endif
