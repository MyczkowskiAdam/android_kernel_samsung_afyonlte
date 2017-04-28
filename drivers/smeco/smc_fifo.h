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


#ifndef SMC_FIFO_H
#define SMC_FIFO_H


#define SMC_FIFO_ERROR_FIFO_FULL    0xFF
#define SMC_FIFO_EMPTY             -1
#define SMC_FIFO_READ_TO_EMPTY      0
#define SMC_FIFO_LENGTH_MULTIPLIER  2  /* 2 */


/**
 * FIFO cell item used in buffer.
 * Cell is aligned to 32B section for cache.
 */
typedef struct
{
    uint32_t data;           /* Address to data delivered to other CPU */
     int32_t length;         /* Length of data delivered to other CPU */
    uint32_t flags;          /* Optional control flags*/
     int32_t userdata1;      /* User data field */

     int32_t userdata2;      /* User data field  */
     int32_t userdata3;      /* User data field  */
     int32_t userdata4;      /* User data field  */
     int32_t userdata5;      /* User data field  */

} smc_fifo_cell_t;


typedef struct
{
        /* FIFO header data aligned two 32B sections for cache line alignment */

        /* FIFO write control */
    int32_t         write_index;        /* Index of the recently written item */
    int32_t         length;             /* Size of the FIFO ( count of FIFO cell items ) */
    uint32_t        write_counter;      /* For statistics and in trace for packet verification */
    int32_t         fill_write_2;       /* Not in use */

    int32_t         fill_write_3;       /* Not in use */
    int32_t         fill_write_4;       /* Not in use */
    int32_t         fill_write_5;       /* Not in use */
    int32_t         fill_write_6;       /* Not in use */

        /* FIFO read control */
    int32_t         read_index;         /* Index of the recently read item */
    uint32_t        read_counter;       /* For statistics and in trace for packet verification */
    int32_t         fill_read_2;        /* Not in use */
    int32_t         fill_read_3;        /* Not in use */

    int32_t         fill_read_4;        /* Not in use */
    int32_t         fill_read_5;        /* Not in use */
    int32_t         fill_read_6;        /* Not in use */
    int32_t         fill_read_7;        /* Not in use */

        /* FIFO data items (32B each) */
    smc_fifo_cell_t cell[1];

} smc_fifo_t;

#define FIFO_HEADER_GET_START_ADDRESS_WRITE(fifo)    ((uint32_t*)((uint32_t)fifo + (0 * sizeof(int32_t))))
#define FIFO_HEADER_GET_START_ADDRESS_READ(fifo)     ((uint32_t*)((uint32_t)fifo + (8 * sizeof(int32_t))))

#define FIFO_HEADER_GET_END_ADDRESS_WRITE(fifo)      ((FIFO_HEADER_GET_START_ADDRESS_WRITE(fifo) + 7))
#define FIFO_HEADER_GET_END_ADDRESS_READ(fifo)       ((FIFO_HEADER_GET_START_ADDRESS_READ(fifo)  + 7))

#define FIFO_HEADER_GET_START_ADDRESS_CELL(fifo, index)  ((uint32_t*)(&fifo->cell[index]))
#define FIFO_HEADER_GET_END_ADDRESS_CELL(fifo, index)    ((FIFO_HEADER_GET_START_ADDRESS_CELL(fifo, index) + 7))

#define FIFO_MEM_SIZE_GET(length)  ( sizeof(smc_fifo_t) + ((length)*sizeof(smc_fifo_cell_t)) )


void        smc_fifo_init_out( smc_fifo_t* p_fifo, int32_t length, uint8_t use_cache_control );
void        smc_fifo_init_in ( smc_fifo_t *p_fifo, int32_t length, uint8_t use_cache_control );
int32_t     smc_fifo_peek    ( smc_fifo_t* p_fifo, uint8_t use_cache_control );
uint32_t    smc_fifo_put     ( smc_fifo_t* p_fifo, uint32_t  data, int32_t  length );
uint32_t    smc_fifo_put_ext ( smc_fifo_t* p_fifo, uint32_t  data, int32_t  length, uint32_t  flags );

int32_t     smc_fifo_get     ( smc_fifo_t* p_fifo, uint32_t* data, int32_t* length, uint32_t* flags );

uint32_t    smc_fifo_put_cell( smc_fifo_t* p_fifo, smc_fifo_cell_t* cell, uint8_t use_cache_control );
int32_t     smc_fifo_get_cell( smc_fifo_t* p_fifo, smc_fifo_cell_t* cell, uint8_t use_cache_control );

uint32_t    smc_fifo_calculate_required_shared_mem( uint32_t fifo_len );
void        smc_fifo_dump     ( char* indent, smc_fifo_t* p_fifo, int32_t mem_offset );
void        smc_fifo_dump_data( smc_fifo_t* p_fifo );

uint8_t     smc_fifo_is_full  ( smc_fifo_t* p_fifo, uint8_t use_cache_control );

#endif /* EOF */

