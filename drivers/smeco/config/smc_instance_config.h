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

Version:       2    12-Feb-2013     Heikki Siikaluoma
Status:        draft
Description :  Support to EOS3 U3CA product

Version:       1    27-Sep-2012     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMC_INSTANCE_CONFIG_H
#define SMC_INSTANCE_CONFIG_H


    /* ---------------------------------------------------
     * Supported product configurations
     * NOTE
     *: Product configuration is selected in the makefile
     *
     */
#define SMC_CONFIG_NONE         0
#define SMC_CONFIG_EOS2         1
#define SMC_CONFIG_EOS3         2              /* U3CA with wgemodem 5.0 */
#define SMC_CONFIG_EOS3_WGE31   3              /* U3   with wgemodem 3.1 */


#ifndef SMC_RUNTIME_TRACES_ENABLED
    #define SMC_RUNTIME_TRACES_ENABLED   FALSE
#endif


#ifdef SMECO_MODEM

  #ifdef SMC_TARGET_PRODUCT_EOS2
            /* WGEM31 based target products */
      #if(SMC_TARGET_PRODUCT_EOS2==2)
          #define SMC_CURRENT_PRODUCT_CONFIG     SMC_CONFIG_EOS2
      #elif(SMC_TARGET_PRODUCT_EOS2==3)
          #define SMC_CURRENT_PRODUCT_CONFIG     SMC_CONFIG_EOS3_WGE31
      #else
          #error "Invalid SMC_TARGET_PRODUCT_EOS2 value for SMC configuration"
      #endif
  #elif( defined(SMC_TARGET_PRODUCT_EOS3) )
          /* WGEM50 based target products */
      #if(SMC_TARGET_PRODUCT_EOS3==1)
          #define SMC_CURRENT_PRODUCT_CONFIG     SMC_CONFIG_EOS3
      #else
          #error "No proper EOS3 SMC target product configuration selected"
      #endif
  #else
      /* Using WGEM 3.1 EOS2 as a default configuration (for backwared compatibility reasons) */
      #define SMC_CURRENT_PRODUCT_CONFIG         SMC_CONFIG_EOS2
  #endif

#elif( defined(SMECO_LINUX_KERNEL) )

    /* Current Configurations for the APE from Linux kernel configuration
     * These flags are defined in the SMC main Makefile
     *
     * CONFIG_ARCH_R8A73734             EOS2        -> SMC_CONFIG_ARCH_R8A73734
     * CONFIG_ARCH_R8A7373              EOS2        ->
     * CONFIG_ARCH_R8A73724             EOS3 U3     -> SMC_CONFIG_ARCH_R8A73724 + SMC_TARGET_PRODUCT_EOS3_U3
     * CONFIG_ARCH_R8A7372A             EOS3 U3CA   -> SMC_CONFIG_ARCH_R8A73724 + SMC_TARGET_PRODUCT_EOS3_U3CA
     *
     */

  #if( defined(SMC_CONFIG_ARCH_R8A7373) || defined(SMC_CONFIG_ARCH_R8A73734) )
      /* EOS2 configuration */
      #define SMC_CURRENT_PRODUCT_CONFIG           SMC_CONFIG_EOS2

  #elif( defined(SMC_CONFIG_ARCH_R8A73724) )
      /* EOS3 U3 configuration */
      #define SMC_CURRENT_PRODUCT_CONFIG           SMC_CONFIG_EOS3_WGE31
  #elif( defined(SMC_CONFIG_ARCH_R8A7372A) )
      /* EOS3 U3CA configuration */
      #define SMC_CURRENT_PRODUCT_CONFIG           SMC_CONFIG_EOS3
  #else
      /* <Unknown> configuration */
     #warning "---- No target product defined, using default configuration ----"

     #define SMC_CURRENT_PRODUCT_CONFIG           SMC_CONFIG_EOS2
  #endif
#endif


#ifdef SMC_CURRENT_PRODUCT_CONFIG
    /* -----------------------------------------------
     * Select the product configuration header for SMC
     */
#if( SMC_CURRENT_PRODUCT_CONFIG == SMC_CONFIG_EOS2 )
    #include "smc_instance_config_r8a73734_wge31.h"
#elif( SMC_CURRENT_PRODUCT_CONFIG == SMC_CONFIG_EOS3 )
    #include "smc_instance_config_u3ca.h"
#elif( SMC_CURRENT_PRODUCT_CONFIG == SMC_CONFIG_EOS3_WGE31 )
    #include "smc_instance_config_u3.h"
#else
    #error "Invalid SMC product configuration"
#endif

#else
    #error "SMC_CURRENT_PRODUCT_CONFIG not defined"
#endif



#endif  /* #ifndef SMC_INSTANCE_CONFIG_H */

/* End of file */
