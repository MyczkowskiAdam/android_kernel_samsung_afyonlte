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

Version:       1    19-Oct-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#ifndef SMC_TEST_FIFO_H
#define SMC_TEST_FIFO_H


typedef struct
{
    void *ptr;
    int   length;
    int   flags;
} fifo_test_sample_t;

#define MAX_FIFO_SIZE          32
#define MAX_SAMPLES           (MAX_FIFO_SIZE+1)
#define TEST_CASE_FACTOR       100


#define SMC_TEST_CREATE_FIFO( name, length )       \
uint8_t _##name##_smc_fifoa_[ sizeof(smc_fifo_t) +     \
(length)*sizeof(smc_fifo_cell_t) ];                \
smc_fifo_t *name = (smc_fifo_t *)_##name##_smc_fifoa_

uint8_t smc_test_case_function_fifo( uint8_t* test_input_data, uint16_t test_input_data_len );


#endif /* End Of File */

