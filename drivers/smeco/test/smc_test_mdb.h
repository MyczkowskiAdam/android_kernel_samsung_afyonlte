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

Version:       1    11-Nov-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMC_TEST_MDB_H
#define SMC_TEST_MDB_H

uint8_t smc_test_case_function_mdb( uint8_t* test_input_data, uint16_t test_input_data_len );
uint8_t smc_test_mdb_channel_test1( void );
uint8_t smc_test_mdb_address_check( void );
uint8_t smc_test_mdb_alloc_dealloc( void );
uint8_t smc_test_mdb_dealloc_wrong_pool( void );
uint8_t smc_test_mdb_copy_test( void ); 
uint8_t smc_test_mdb_perf( void );
uint8_t smc_test_mdb_map_test( uint32_t *map, uint32_t allocs, uint32_t deallocs, smc_channel_t* smc_channel );
void smc_test_mdb_random_alloc(void);

#endif
