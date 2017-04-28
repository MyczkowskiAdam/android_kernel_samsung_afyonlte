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

Version:       1    08-Nov-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMCLIB_H
#define SMCLIB_H


#include <linux/types.h>
#include <dlfcn.h>

#define SMECO_LIB_NAME "/system/lib/libsmeco.so"


#define SMC_FUNC_INITIALIZE     "smc_initialize"
#define SMC_FUNC_SEND           "smc_send"

    // TODO Add test build flag
#define SMC_FUNC_TEST_START     "smc_test_handler_start"

    /**
     * API function declarations
     */
typedef void (*smc_initialize)(void);

    // uint8_t smc_test_handler_start(uint8_t* test_data_input, uint16_t test_data_input_len)
typedef uint8_t (*smc_lib_test_handler_start)(uint16_t, uint16_t, uint8_t*);

static inline void* smeco_lib_open_file( char* file )
{
    void *lib_handle = dlopen(file, RTLD_LAZY);

    return lib_handle;
}

static inline void* smeco_lib_open( void )
{
    return smeco_lib_open_file( SMECO_LIB_NAME );
}

static inline smc_initialize get_func_smc_initialize( void* smeco_lib_handle )
{
    return (smc_initialize)dlsym(smeco_lib_handle, SMC_FUNC_INITIALIZE);
}

static inline smc_lib_test_handler_start get_func_smc_test_handler_start( void* smeco_lib_handle )
{
    return (smc_lib_test_handler_start)dlsym(smeco_lib_handle, SMC_FUNC_TEST_START);
}

static inline void smeco_lib_close( void* smeco_lib_handle )
{
   dlclose(smeco_lib_handle);
}

#ifdef __cplusplus
}
#endif

#endif
