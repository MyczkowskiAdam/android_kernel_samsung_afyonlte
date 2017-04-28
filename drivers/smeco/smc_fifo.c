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

Version:       10   11-Feb-2012     Heikki Siikaluoma
Status:        draft
Description :  Added FIFO information dump function.

Version:       7    19-Dec-2011     Heikki Siikaluoma
Status:        draft
Description :  FIFO interface functions to put and get whole cell data.

Version:       3    08-Nov-2011     Heikki Siikaluoma
Status:        draft
Description :  Platform independent code implemented

Version:       2    21-Oct-2011     Heik
ki Siikaluoma
Status:        draft
Description :  FIFO created

Version:       1    19-Oct-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#include "smc_common_includes.h"
#include "smc_trace.h"
#include "smc_fifo.h"
#include "smc.h"

void smc_fifo_init_out( smc_fifo_t* p_fifo, int32_t length, uint8_t use_cache_control )
{
    assert( p_fifo != NULL );

    SMC_TRACE_PRINTF_FIFO("smc_fifo_init_out: FIFO 0x%08X: set length %d, mem size=%d, Cache control %s",
            (uint32_t)p_fifo, length, FIFO_MEM_SIZE_GET(length), use_cache_control?"ENABLED":"DISABLED" );

    if( use_cache_control )
    {
        SMC_TRACE_PRINTF_FIFO("smc_fifo_init_in: FIFO 0x%08X: Cache control for write area", (uint32_t)p_fifo);

            /* Get the data from the cache */
        SMC_SHM_CACHE_INVALIDATE( FIFO_HEADER_GET_START_ADDRESS_WRITE(p_fifo), FIFO_HEADER_GET_END_ADDRESS_WRITE(p_fifo) );
    }

    SMC_SHM_WRITE32( &p_fifo->write_index,   0 );
    SMC_SHM_WRITE32( &p_fifo->length       , length );
    SMC_SHM_WRITE32( &p_fifo->write_counter, 0);

    p_fifo->fill_write_2       = 0;
    p_fifo->fill_write_3       = 0;
    p_fifo->fill_write_4       = 0;
    p_fifo->fill_write_5       = 0;
    p_fifo->fill_write_6       = 0;


    if( use_cache_control )
    {
        SMC_TRACE_PRINTF_FIFO("smc_fifo_init_out: FIFO 0x%08X: Cache control for write area", (uint32_t)p_fifo);

        SMC_SHM_CACHE_CLEAN( FIFO_HEADER_GET_START_ADDRESS_WRITE(p_fifo), FIFO_HEADER_GET_END_ADDRESS_WRITE(p_fifo) );

        SMC_SHM_CACHE_INVALIDATE( FIFO_HEADER_GET_START_ADDRESS_READ(p_fifo), FIFO_HEADER_GET_END_ADDRESS_READ(p_fifo) );
    }
    else
    {
        SMC_HW_ARM_MEMORY_SYNC(NULL);
    }

    SMC_SHM_WRITE32( &p_fifo->read_index, 0);
    SMC_SHM_WRITE32( &p_fifo->read_counter, 0);

    p_fifo->fill_read_2  = 0;
    p_fifo->fill_read_3  = 0;
    p_fifo->fill_read_4  = 0;
    p_fifo->fill_read_5  = 0;
    p_fifo->fill_read_6  = 0;
    p_fifo->fill_read_7  = 0;

    if( use_cache_control )
    {
        SMC_TRACE_PRINTF_FIFO("smc_fifo_init_out: FIFO 0x%08X: Cache control for read area", (uint32_t)p_fifo);
            /* Because we are initializing our own out channel the read pointer should be set to 0 */
        SMC_SHM_CACHE_CLEAN( FIFO_HEADER_GET_START_ADDRESS_READ(p_fifo), FIFO_HEADER_GET_END_ADDRESS_READ(p_fifo) );
    }
    else
    {
        SMC_HW_ARM_MEMORY_SYNC(NULL);
    }
}

void smc_fifo_init_in( smc_fifo_t* p_fifo, int32_t length, uint8_t use_cache_control )
{
    assert( p_fifo != NULL );

    SMC_TRACE_PRINTF_FIFO("smc_fifo_init_in: FIFO 0x%08X: set length %d, mem size=%d, Cache control %s",
            (uint32_t)p_fifo, length, FIFO_MEM_SIZE_GET(length), use_cache_control?"ENABLED":"DISABLED" );

    p_fifo->write_index        = 0;
    p_fifo->length             = length;
    p_fifo->write_counter      = 0;
    p_fifo->fill_write_2       = 0;
    p_fifo->fill_write_3       = 0;
    p_fifo->fill_write_4       = 0;
    p_fifo->fill_write_5       = 0;
    p_fifo->fill_write_6       = 0;

    if( use_cache_control )
    {
        SMC_TRACE_PRINTF_FIFO("smc_fifo_init_in: FIFO 0x%08X: Cache control for write area", (uint32_t)p_fifo);

        SMC_SHM_CACHE_CLEAN( FIFO_HEADER_GET_START_ADDRESS_WRITE(p_fifo), FIFO_HEADER_GET_END_ADDRESS_WRITE(p_fifo) );

        SMC_TRACE_PRINTF_FIFO("smc_fifo_init_in: FIFO 0x%08X: Cache control for read area", (uint32_t)p_fifo);

            /* Update data to mem from cache */
        SMC_SHM_CACHE_INVALIDATE( FIFO_HEADER_GET_START_ADDRESS_READ(p_fifo), FIFO_HEADER_GET_END_ADDRESS_READ(p_fifo) );
    }
    else
    {
        SMC_HW_ARM_MEMORY_SYNC(NULL);
    }

    SMC_SHM_WRITE32( &p_fifo->read_index, 0 );
    SMC_SHM_WRITE32( &p_fifo->read_counter, 0 );

    p_fifo->fill_read_2  = 0;
    p_fifo->fill_read_3  = 0;
    p_fifo->fill_read_4  = 0;
    p_fifo->fill_read_5  = 0;
    p_fifo->fill_read_6  = 0;
    p_fifo->fill_read_7  = 0;

    if( use_cache_control )
    {
        SMC_TRACE_PRINTF_FIFO("smc_fifo_init_in: FIFO 0x%08X: Cache control for read area...", (uint32_t)p_fifo);

        SMC_SHM_CACHE_CLEAN( FIFO_HEADER_GET_START_ADDRESS_READ(p_fifo), FIFO_HEADER_GET_END_ADDRESS_READ(p_fifo) );
    }
    else
    {
        SMC_HW_ARM_MEMORY_SYNC(NULL);
    }
}

uint32_t smc_fifo_put( smc_fifo_t* p_fifo, uint32_t data, int32_t length )
{
    return smc_fifo_put_ext(p_fifo, data, length, 0x00 );
}

uint32_t smc_fifo_put_ext( smc_fifo_t* p_fifo, uint32_t data, int32_t length, uint32_t flags )
{
    smc_fifo_cell_t cell;

    SMC_TRACE_PRINTF_FIFO("smc_fifo_put_ext: FIFO 0x%08X: Message 0x%08X, len %d, flags 0x%08X",
            (uint32_t)p_fifo, (uint32_t)data, length, flags);

    cell.data        = data;
    cell.length      = length;
    cell.flags       = flags;
    cell.userdata1   = 0;
    cell.userdata2   = 0;
    cell.userdata3   = 0;
    cell.userdata4   = 0;
    cell.userdata5   = 0;

    return smc_fifo_put_cell(p_fifo, &cell, FALSE);
}

/*
 * FIFO Send.
 * This is critical section and must be locked in caller.
 */
uint32_t smc_fifo_put_cell( smc_fifo_t* p_fifo, smc_fifo_cell_t* cell, uint8_t use_cache_control )
{
    uint32_t return_value = SMC_OK;
    uint32_t cell_index   = 0;
    int32_t  write_index  = 0;
    int32_t  read_index   = 0;
    int32_t  n_in_fifo    = 0;
    uint32_t write_cnt    = 0;
    uint32_t read_cnt     = 0;


    assert( p_fifo != NULL );
    assert( ((cell->data > 0) || (cell->length > 0) || (cell->flags > 0)) );

    if( use_cache_control == TRUE )
    {
        SMC_SHM_CACHE_INVALIDATE( FIFO_HEADER_GET_START_ADDRESS_READ(p_fifo), FIFO_HEADER_GET_END_ADDRESS_READ(p_fifo) );
    }

    write_index = p_fifo->write_index;
    write_cnt   = p_fifo->write_counter;

    read_index = SMC_SHM_READ32( &p_fifo->read_index );
    read_cnt   = SMC_SHM_READ32( &p_fifo->read_counter );

    n_in_fifo  = write_index - read_index;

    if( n_in_fifo < 0 )
    {
        SMC_TRACE_PRINTF_FIFO("smc_fifo_put_cell: n_in_fifo==%d, setting value based on length %d * multiplier %d", n_in_fifo,
                                                                                                                   p_fifo->length,
                                                                                                                   SMC_FIFO_LENGTH_MULTIPLIER);
        n_in_fifo += (p_fifo->length * SMC_FIFO_LENGTH_MULTIPLIER);
    }

    SMC_TRACE_PRINTF_FIFO("smc_fifo_put_cell: FIFO 0x%08X n_in_fifo=%d, fifo len %d (write_ind %d, read_ind %d): msg_ptr 0x%08X, len %d, flags 0x%08X",
            (uint32_t)p_fifo, n_in_fifo, p_fifo->length, write_index, read_index, cell->data, cell->length, cell->flags);

    if( n_in_fifo < p_fifo->length )
    {
        smc_fifo_cell_t* fifo_cell = NULL;

        cell_index = write_index;

        if ( cell_index >= p_fifo->length )
        {
            SMC_TRACE_PRINTF_FIFO("smc_fifo_put_cell: FIFO 0x%08X: cell index %d exceeds fifo len %d",
                    (uint32_t)p_fifo, cell_index, p_fifo->length);
            cell_index -= p_fifo->length;
        }

        fifo_cell = &p_fifo->cell[cell_index];

        SMC_TRACE_PRINTF_FIFO("smc_fifo_put_cell: FIFO 0x%08X: data to cell_index %d (0x%08X), write_index = %d, read index = %d",
                (uint32_t)p_fifo, cell_index, (uint32_t)fifo_cell, write_index, read_index);

        SMC_SHM_WRITE32( &fifo_cell->data,      cell->data );
        SMC_SHM_WRITE32( &fifo_cell->length,    cell->length );
        SMC_SHM_WRITE32( &fifo_cell->flags,     cell->flags );
        SMC_SHM_WRITE32( &fifo_cell->userdata1, cell->userdata1 );
        SMC_SHM_WRITE32( &fifo_cell->userdata2, cell->userdata2 );
        SMC_SHM_WRITE32( &fifo_cell->userdata3, cell->userdata3 );
        SMC_SHM_WRITE32( &fifo_cell->userdata4, cell->userdata4 );
        SMC_SHM_WRITE32( &fifo_cell->userdata5, cell->userdata5 );

        if ( ++write_index == (p_fifo->length * SMC_FIFO_LENGTH_MULTIPLIER) )
        {
            SMC_TRACE_PRINTF_FIFO("smc_fifo_put_cell: FIFO 0x%08X: write index to 0 from %d, FIFO len %d*%d",
                    (uint32_t)p_fifo, write_index, p_fifo->length, SMC_FIFO_LENGTH_MULTIPLIER);
            write_index = 0;
        }

        if( write_cnt < 0xFFFFFFFF)
        {
            write_cnt++;
        }
        else
        {
            write_cnt = 0;
        }

        SMC_SHM_WRITE32( &p_fifo->write_index, write_index );
        SMC_SHM_WRITE32( &p_fifo->write_counter, write_cnt );

        RD_TRACE_SEND5(TRA_SMC_FIFO_PUT, 4, &p_fifo,
                                         4, &cell->data,
                                         4, &cell->length,
                                         4, &cell->flags,
                                         4, &n_in_fifo);

        RD_TRACE_SEND4(TRA_SMC_FIFO_PUT_STATISTICS, 4, &p_fifo,
                                                    4, &write_cnt,
                                                    4, &read_cnt,
                                                    4, &n_in_fifo);

        SMC_TRACE_PRINTF_FIFO_PUT("fifoPtr 0x%08X, data 0x%08X, length %d, flags 0x%08X, itemsInFifo: %d, writecnt: %d, readcnt: %d",
            (uint32_t)p_fifo, (uint32_t)cell->data, cell->length, cell->flags, n_in_fifo, write_cnt, read_cnt);

        return_value = SMC_OK;
    }
    else
    {
        SMC_TRACE_PRINTF_DEBUG("smc_fifo_put_cell: ERROR: FIFO 0x%08X is FULL, n_in_fifo=%d, fifo len %d (WI: %d, RI: %d)",
                (uint32_t)p_fifo, n_in_fifo, p_fifo->length, write_index, read_index);

        return_value = SMC_FIFO_ERROR_FIFO_FULL;
    }

    if( use_cache_control )
    {
        SMC_SHM_CACHE_CLEAN( FIFO_HEADER_GET_START_ADDRESS_WRITE(p_fifo), FIFO_HEADER_GET_END_ADDRESS_WRITE(p_fifo) );
        SMC_SHM_CACHE_CLEAN( FIFO_HEADER_GET_START_ADDRESS_CELL( p_fifo, cell_index ), FIFO_HEADER_GET_END_ADDRESS_CELL( p_fifo, cell_index ) );
    }
    else
    {
        SMC_HW_ARM_MEMORY_SYNC(NULL);
    }

    SMC_TRACE_PRINTF_FIFO("smc_fifo_put_cell: FIFO 0x%08X put completed in index %d, new write_ind %d, read_ind %d, return 0x%02X",
            (uint32_t)p_fifo, cell_index, write_index, read_index, return_value);

    return return_value;
}

int32_t smc_fifo_get( smc_fifo_t* p_fifo, uint32_t* data, int32_t* length, uint32_t* flags )
{
    smc_fifo_cell_t cell;
    int32_t         ret_val = 0;

    ret_val = smc_fifo_get_cell(p_fifo, &cell, FALSE);

    *data   = cell.data;
    *length = cell.length;
    *flags  = cell.flags;

    return ret_val;
}

/*
 * There is only one reader for one fifo. Also the protocol allows writing
 * while reading is going on so no protection is needed in reading, except when cache is used.
 * NOTE: In SMP this must be locked in caller function.
 */
int32_t smc_fifo_get_cell( smc_fifo_t* p_fifo, smc_fifo_cell_t* cell, uint8_t use_cache_control )
{
    int32_t  packet_count_left = 0;
    int32_t  write_index       = 0;
    int32_t  read_index        = 0;
    uint32_t write_cnt         = 0;
    uint32_t read_cnt          = 0;

    assert( p_fifo != NULL );

    if( use_cache_control )
    {
        SMC_SHM_CACHE_INVALIDATE( FIFO_HEADER_GET_START_ADDRESS_WRITE(p_fifo), FIFO_HEADER_GET_END_ADDRESS_WRITE(p_fifo) );
    }

    SMC_TRACE_PRINTF_FIFO("smc_fifo_get_cell: FIFO 0x%08X get W/R from address 0x%08X/0x%08X", p_fifo, &p_fifo->write_index, &p_fifo->read_index);

    write_index = SMC_SHM_READ32( &p_fifo->write_index );
    write_cnt   = SMC_SHM_READ32( &p_fifo->write_counter);

    read_index  = SMC_SHM_READ32( &p_fifo->read_index  );
    read_cnt    = SMC_SHM_READ32( &p_fifo->read_counter );

    if ( (write_index - read_index) == 0 )
    {
        SMC_TRACE_PRINTF_FIFO_GET("read empty: fifoPtr: 0x%08X, readIndex: %d, writeIndex: %d, readCount: %d, writeCount: %d",
                (uint32_t)p_fifo, read_index, write_index, read_cnt, write_cnt);

        RD_TRACE_SEND5(TRA_SMC_FIFO_GET_EMPTY, 4, &p_fifo,
                                               4, &read_index,
                                               4, &write_index,
                                               4, &read_cnt,
                                               4, &write_cnt);
        cell->data        = 0;
        cell->length      = 0;
        cell->flags       = 0;
        cell->userdata1   = 0;
        cell->userdata2   = 0;
        cell->userdata3   = 0;
        cell->userdata4   = 0;
        cell->userdata5   = 0;

            /* Nothing to read */
        packet_count_left = SMC_FIFO_EMPTY;
    }
    else
    {
        int32_t          cell_index = read_index;
        smc_fifo_cell_t* fifo_cell  = NULL;

        if ( cell_index >= p_fifo->length )
        {
            SMC_TRACE_PRINTF_FIFO("smc_fifo_get_cell: FIFO 0x%08X cell index %d exceeds fifo len %d", (uint32_t)p_fifo, cell_index, p_fifo->length);
            cell_index -= p_fifo->length;
        }

        SMC_TRACE_PRINTF_FIFO("smc_fifo_get_cell: FIFO 0x%08X: data from cell_index %d (0x%08X), write_index = %d, read index = %d",
                               (uint32_t)p_fifo, cell_index, ((uint32_t)&p_fifo->cell[cell_index]), write_index , read_index);

        if( use_cache_control )
        {
            SMC_SHM_CACHE_INVALIDATE( FIFO_HEADER_GET_START_ADDRESS_CELL(p_fifo, cell_index), FIFO_HEADER_GET_END_ADDRESS_CELL(p_fifo, cell_index) );
        }

        fifo_cell = &p_fifo->cell[cell_index];

        if( fifo_cell->data   == 0   &&
            fifo_cell->length == -1  &&
            fifo_cell->flags  == 0 )
        {
            SMC_TRACE_PRINTF_ASSERT("smc_fifo_get_cell: NULL read from FIFO 0x%08X: cell[%d] (0x%08X-0x%08X) RI=%d WI=%d",
                    (uint32_t)p_fifo, cell_index,
                    (uint32_t)(FIFO_HEADER_GET_START_ADDRESS_CELL( p_fifo, cell_index )),
                    (uint32_t)(FIFO_HEADER_GET_END_ADDRESS_CELL( p_fifo, cell_index )),
                    read_index,
                    write_index);

            assert(0);
        }
        else
        {
            cell->data        = SMC_SHM_READ32( &fifo_cell->data );
            cell->length      = SMC_SHM_READ32( &fifo_cell->length );
            cell->flags       = SMC_SHM_READ32( &fifo_cell->flags );
            cell->userdata1   = SMC_SHM_READ32( &fifo_cell->userdata1 );
            cell->userdata2   = SMC_SHM_READ32( &fifo_cell->userdata2 );
            cell->userdata3   = SMC_SHM_READ32( &fifo_cell->userdata3 );
            cell->userdata4   = SMC_SHM_READ32( &fifo_cell->userdata4 );
            cell->userdata5   = SMC_SHM_READ32( &fifo_cell->userdata5 );

        }   /* If NULL read and no assert --> normal proceed but SMC read function handles the NULL item */

            /*
             * Reset fifo cell item values to verify
             * that data is updated correctly when put a new item
             */
        SMC_SHM_WRITE32( &fifo_cell->data  ,  0 );
        SMC_SHM_WRITE32( &fifo_cell->length, -1 );
        SMC_SHM_WRITE32( &fifo_cell->flags ,  0 );

        packet_count_left = write_index - read_index;

        if( packet_count_left < 0 )
        {
            packet_count_left += (p_fifo->length * SMC_FIFO_LENGTH_MULTIPLIER);
        }

        read_index++;
        packet_count_left--;

        if( read_cnt < 0xFFFFFFFF )
        {
            read_cnt++;
        }
        else
        {
            read_cnt = 0;
        }

        RD_TRACE_SEND5(TRA_SMC_FIFO_GET, 4, &p_fifo,
                                         4, &cell->data,
                                         4, &cell->length,
                                         4, &cell->flags,
                                         4, &packet_count_left);

        SMC_TRACE_PRINTF_FIFO_GET("fifoPtr 0x%08X, data 0x%08X, length %d, flags 0x%08X, itemsInFifo: %d, writecnt: %d, readcnt: %d",
            (uint32_t)p_fifo, (uint32_t)cell->data, cell->length, cell->flags, packet_count_left, write_cnt, read_cnt);

        if( read_index == (p_fifo->length * SMC_FIFO_LENGTH_MULTIPLIER) )
        {
            SMC_TRACE_PRINTF_FIFO("smc_fifo_get_cell: FIFO 0x%08X read index %d==fifo len %d * %d --> Set read index to 0",
                                    (uint32_t)p_fifo, read_index, p_fifo->length, SMC_FIFO_LENGTH_MULTIPLIER);
            read_index = 0;
        }
        else if( read_index > (p_fifo->length * SMC_FIFO_LENGTH_MULTIPLIER) )
        {
            SMC_TRACE_PRINTF_ASSERT("smc_fifo_get_cell: FIFO read overflow");
            assert(0);
        }

        SMC_SHM_WRITE32( &p_fifo->read_index,   read_index );
        SMC_SHM_WRITE32( &p_fifo->read_counter, read_cnt );

        RD_TRACE_SEND4(TRA_SMC_FIFO_GET_STATISTICS, 4, &p_fifo,
                                                    4, &write_cnt,
                                                    4, &read_cnt,
                                                    4, &packet_count_left);
        if( use_cache_control )
        {
            SMC_SHM_CACHE_CLEAN( FIFO_HEADER_GET_START_ADDRESS_CELL(p_fifo, cell_index), FIFO_HEADER_GET_END_ADDRESS_CELL(p_fifo, cell_index) );
            SMC_SHM_CACHE_CLEAN( FIFO_HEADER_GET_START_ADDRESS_READ(p_fifo), FIFO_HEADER_GET_END_ADDRESS_READ(p_fifo) );
        }
        else
        {
            SMC_HW_ARM_MEMORY_SYNC(NULL);
        }
    }

    return packet_count_left;
}


/*
 * Returns items in specified FIFO.
 * This is critical section and must be locked in caller function.
 */
int32_t smc_fifo_peek( smc_fifo_t *p_fifo, uint8_t use_cache_control )
{
    int32_t write_ind     = 0;
    int32_t items_in_fifo = 0;

    assert( p_fifo != NULL );

    if( use_cache_control )
    {
        SMC_SHM_CACHE_INVALIDATE( FIFO_HEADER_GET_START_ADDRESS_WRITE(p_fifo), FIFO_HEADER_GET_END_ADDRESS_WRITE(p_fifo) );
    }

    write_ind     = p_fifo->write_index;
    items_in_fifo = ( write_ind - p_fifo->read_index );

    if( items_in_fifo < 0 )
    {
        SMC_TRACE_PRINTF_FIFO("smc_fifo_peek: FIFO 0x%08X: Write index reset (%d)", (uint32_t)p_fifo, items_in_fifo);

        items_in_fifo = (p_fifo->length * SMC_FIFO_LENGTH_MULTIPLIER) - p_fifo->read_index + write_ind;
    }

    SMC_TRACE_PRINTF_FIFO("smc_fifo_peek: FIFO 0x%08X: items in FIFO %d (write_ind %d-read_ind %d) FIFO size %d (multiplier %d)",
            (uint32_t)p_fifo, items_in_fifo, write_ind, p_fifo->read_index, p_fifo->length, SMC_FIFO_LENGTH_MULTIPLIER);

    return items_in_fifo;
}

uint8_t smc_fifo_is_full( smc_fifo_t* p_fifo, uint8_t use_cache_control )
{
    return (smc_fifo_peek( p_fifo, use_cache_control) >= p_fifo->length);
}


uint32_t smc_fifo_calculate_required_shared_mem( uint32_t fifo_len )
{
    uint32_t required_mem = 0;

    required_mem = sizeof( smc_fifo_t );

    if( fifo_len > 1 )
    {
        required_mem += ( sizeof( smc_fifo_cell_t ) * (fifo_len-1) );
    }

    SMC_TRACE_PRINTF_FIFO("smc_fifo_calculate_required_shared_mem: FIFO with %d items requires %d bytes", fifo_len, required_mem);
    return required_mem;
}

void smc_fifo_dump(char* indent, smc_fifo_t* p_fifo, int32_t mem_offset )
{
    if( p_fifo != NULL )
    {
        uint32_t* address = 0x00000000;
        int32_t   fifo_unread = p_fifo->write_index-p_fifo->read_index;
        int       iDataLinesToPrint = 4;
        uint32_t iCounter = 0;

        if( fifo_unread < 0 ) fifo_unread *= -1;

        SMC_TRACE_PRINTF_ALWAYS("%sFIFO: 0x%08X, Size %d, SHM offset 0x%08X, uses %d bytes of SHM, %d unread items", indent,
                (uint32_t)p_fifo, p_fifo->length,
                (uint32_t)mem_offset, smc_fifo_calculate_required_shared_mem( p_fifo->length ),
                fifo_unread);

        SMC_TRACE_PRINTF_ALWAYS("%s  Header write: 0x%08X - 0x%08X (PHY-ADDR: 0x%08X - 0x%08X): write index %d, write counter %d", indent,
                    (uint32_t)(FIFO_HEADER_GET_START_ADDRESS_WRITE(p_fifo)), (uint32_t)FIFO_HEADER_GET_END_ADDRESS_WRITE(p_fifo),
                    ((uint32_t)FIFO_HEADER_GET_START_ADDRESS_WRITE(p_fifo)-mem_offset), ((uint32_t)FIFO_HEADER_GET_END_ADDRESS_WRITE(p_fifo)-mem_offset),
                    p_fifo->write_index, p_fifo->write_counter);

        address = FIFO_HEADER_GET_START_ADDRESS_WRITE(p_fifo);

        for(int i = 0; i < 1; i++)
        {
            SMC_TRACE_PRINTF_ALWAYS("%s       data[%d]: 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X", indent,i,
                                                        *(address+iCounter),     *(address+iCounter+1), *(address+iCounter+2), *(address+iCounter+3),
                                                        *(address+iCounter+4), *(address+iCounter+5), *(address+iCounter+6), *(address+iCounter+7));

            iCounter+=8;
        }

        SMC_TRACE_PRINTF_ALWAYS("%s   Header read: 0x%08X - 0x%08X (PHY-ADDR: 0x%08X - 0x%08X): read index %d, read counter %d", indent,
                (uint32_t)FIFO_HEADER_GET_START_ADDRESS_READ(p_fifo), (uint32_t)FIFO_HEADER_GET_END_ADDRESS_READ(p_fifo),
                ((uint32_t)FIFO_HEADER_GET_START_ADDRESS_READ(p_fifo)-mem_offset),
                ((uint32_t)FIFO_HEADER_GET_END_ADDRESS_READ(p_fifo)-mem_offset),
                p_fifo->read_index, p_fifo->read_counter);

        address = FIFO_HEADER_GET_START_ADDRESS_READ(p_fifo);

        iCounter = 0;
        for(int i = 0; i < 1; i++)
        {
            SMC_TRACE_PRINTF_ALWAYS("%s       data[%d]: 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X", indent,i,
                                                        *(address+iCounter),     *(address+iCounter+1), *(address+iCounter+2), *(address+iCounter+3),
                                                        *(address+iCounter+4), *(address+iCounter+5), *(address+iCounter+6), *(address+iCounter+7));

            iCounter+=8;
        }


        SMC_TRACE_PRINTF_ALWAYS("%sCell data area: 0x%08X - 0x%08X (PHY-ADDR: 0x%08X - 0x%08X)", indent,
                (uint32_t)FIFO_HEADER_GET_START_ADDRESS_CELL(p_fifo, 0),
                (uint32_t)FIFO_HEADER_GET_END_ADDRESS_CELL(p_fifo, p_fifo->length-1),
                ((uint32_t)FIFO_HEADER_GET_START_ADDRESS_CELL(p_fifo, 0)-mem_offset),
                ((uint32_t)FIFO_HEADER_GET_END_ADDRESS_CELL(p_fifo, p_fifo->length-1)-mem_offset));



        address = FIFO_HEADER_GET_START_ADDRESS_CELL(p_fifo, 0);

        iCounter = 0;
        for(int i = 0; i < iDataLinesToPrint; i++)
        {
            SMC_TRACE_PRINTF_ALWAYS("%s       data[%d]: 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X", indent,i,
                                                        *(address+iCounter),     *(address+iCounter+1), *(address+iCounter+2), *(address+iCounter+3),
                                                        *(address+iCounter+4), *(address+iCounter+5), *(address+iCounter+6), *(address+iCounter+7));

            iCounter+=8;
        }


    }
}

/**
 * Dump the FIFO data contents.
 */
void smc_fifo_dump_data( smc_fifo_t* p_fifo )
{
    if( p_fifo != NULL )
    {
        SMC_TRACE_PRINTF_ALWAYS("smc_fifo_dump_data: 0x%08X, len %d", (uint32_t)p_fifo, p_fifo->length);

        SMC_TRACE_PRINTF_ALWAYS_DATA((8 * sizeof(int32_t)), (uint8_t*)FIFO_HEADER_GET_START_ADDRESS_WRITE(p_fifo));
        SMC_TRACE_PRINTF_ALWAYS_DATA((8 * sizeof(int32_t)), (uint8_t*)FIFO_HEADER_GET_START_ADDRESS_READ(p_fifo));

        for(int i = 0; i < p_fifo->length; i++ )
        {
            SMC_TRACE_PRINTF_ALWAYS_DATA((8 * sizeof(int32_t)), (uint8_t*)FIFO_HEADER_GET_START_ADDRESS_CELL(p_fifo, i));
        }
    }
    else
    {
        SMC_TRACE_PRINTF_ALWAYS("smc_fifo_dump_data: FIFO is NULL");
    }
}

/* EOF */
