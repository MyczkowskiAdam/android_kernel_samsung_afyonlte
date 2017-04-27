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

#ifndef SMC_CONFIG_CONTROL_H
#define SMC_CONFIG_CONTROL_H

#define SMC_CONFIG_USER_CONTROL   "SMC_CONTROL"


/*#include "smc_config_control_r8a73734_wge31.h"*/

#if( SMC_CURRENT_PRODUCT_CONFIG == SMC_CONFIG_EOS2 )
    #include "smc_config_control_r8a73734_wge31.h"
#elif( SMC_CURRENT_PRODUCT_CONFIG == SMC_CONFIG_EOS3 )
    #include "smc_config_control_u3ca.h"
#elif( SMC_CURRENT_PRODUCT_CONFIG == SMC_CONFIG_EOS3_WGE31 )
    #include "smc_config_control_u3.h"
#else
    #error "Invalid SMC product configuration"
#endif



    /*
     * Common control channel configuration function prototypes.
     *
     * TODO THE PRODUCT SPECIFIC BUILD FLAG NEEDED
     */

smc_instance_conf_t* smc_instance_conf_get_control( char* smc_user_name, char* config_name );


#endif
