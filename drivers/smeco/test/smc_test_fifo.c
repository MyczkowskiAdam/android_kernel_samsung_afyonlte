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

#include "smc_common_includes.h"

#include "smc_conf.h"
#include "smc_fifo.h"
#include "smc.h"
#include "smc_test.h"
#include "smc_test_fifo.h"
#include "smc_trace.h"


static void smc_fifo_make_test_sample( fifo_test_sample_t *s );
static void smc_fifo_init_test_samples( void );
static void smc_fifo_put_test_sample( fifo_test_sample_t *s );
static void smc_fifo_get_test_sample( fifo_test_sample_t *s );


fifo_test_sample_t smc_test_sample[ MAX_SAMPLES ];
int smc_test_samples;
int smc_test_sample_in;
int smc_test_sample_out;


    // Test case functions
static int smc_start_fifo_test_single_cpu(int iFifoLenStart, int iFifoLenEnd);

SMC_TEST_CREATE_FIFO( smc_test_fifo, MAX_FIFO_SIZE );

uint8_t smc_test_case_function_fifo( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t test_result = SMC_ERROR;

    SMC_TEST_TRACE_PRINTF_INFO("FIFO test case invoked, input data len %d", test_input_data_len);

    if( test_input_data_len > 0 )
    {
        switch( test_input_data[0] )
        {
            case 0x00:
            {
                int iStartLen = 0;
                int iEndLen   = 0;

                if( test_input_data_len > 1 )
                {
                    iStartLen = test_input_data[1];

                    if( test_input_data_len > 2 )
                    {
                        iEndLen = test_input_data[2];
                    }
                }

                SMC_TEST_TRACE_PRINTF_INFO("FIFO smc_start_fifo_test_single_cpu test case starting: FIFO test start=%d, end=%d...", iStartLen, iEndLen);

                test_result = smc_start_fifo_test_single_cpu(iStartLen, iEndLen);
                break;
            }
            default:
            {
                SMC_TEST_TRACE_PRINTF_ERROR("FIFO: Invalid test case 0x%02X", test_input_data[0]);
                test_result = SMC_ERROR;
                break;
            }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_ERROR("FIFO test case id missing (input data [0])");
        test_result = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("FIFO test case completed with return value 0x%02X", test_result);

    return test_result;
}


static void smc_fifo_make_test_sample( fifo_test_sample_t *s )
{
    do
    {
        s->ptr = (void *)rand();
        s->length = rand();
        s->flags  = rand();
    } while ( s->ptr == 0 || s->length == 0 );
}

static void smc_fifo_init_test_samples( void )
{
    smc_test_samples    = 0;
    smc_test_sample_in  = 0;
    smc_test_sample_out = 0;
}

static void smc_fifo_put_test_sample( fifo_test_sample_t *s )
{
    assert( smc_test_samples <= MAX_SAMPLES );

    smc_test_sample[smc_test_sample_in].ptr    = s->ptr;
    smc_test_sample[smc_test_sample_in].length = s->length;
    smc_test_sample[smc_test_sample_in].flags  = s->flags;

    ++smc_test_samples;

    if ( ++smc_test_sample_in == MAX_SAMPLES )
    {
        smc_test_sample_in = 0;
    }
}

static void smc_fifo_get_test_sample( fifo_test_sample_t *s )
{
    assert( smc_test_samples > 0 );

    s->ptr    = smc_test_sample[smc_test_sample_out].ptr;
    s->length = smc_test_sample[smc_test_sample_out].length;
    s->flags  = smc_test_sample[smc_test_sample_out].flags;

    --smc_test_samples;

    if ( ++smc_test_sample_out == MAX_SAMPLES )
    {
        smc_test_sample_out = 0;
    }
}

static int smc_start_fifo_test_single_cpu(int iFifoLenStart, int iFifoLenEnd)
{
    SMC_TEST_TRACE_PRINTF_INFO( "FIFO: START FIFO TEST...");

    if( iFifoLenStart <= 0 ) iFifoLenStart = 1;

    if( iFifoLenEnd <= 0 ) iFifoLenEnd = MAX_FIFO_SIZE;

    for( int fifo_len = iFifoLenStart; fifo_len <= iFifoLenEnd; fifo_len++ )
    {
        int max_fill       = 0;
        int fill           = 0;
        int32_t fifo_count = 0;

        SMC_TEST_TRACE_PRINTF_INFO( "====================================================================");
        SMC_TEST_TRACE_PRINTF_INFO( "FIFO: INIT 0x%08X len %d ", (uint32_t)smc_test_fifo, fifo_len );
        smc_fifo_init_out( smc_test_fifo, fifo_len, FALSE );

        /*SMC_TEST_TRACE_PRINTF_INFO( "FIFO: init_test_samples..." );*/

        smc_fifo_init_test_samples();

        /*SMC_TEST_TRACE_PRINTF_INFO( "FIFO: Test samples initialized");*/

        for ( int test_case = 0; test_case < fifo_len * TEST_CASE_FACTOR; test_case++ )
        {
            if ( rand() & 1 )
            {
                fifo_test_sample_t s;

                /* put */
                SMC_TEST_TRACE_PRINTF_INFO( "FIFO: PUT --> (case %d, fifo size %d, fill %d)", test_case, fifo_len, fill );

                smc_fifo_make_test_sample( &s );

                if ( fill < fifo_len )
                {
                    uint32_t ret = 0;

                    ++fill;

                    if ( fill > max_fill )
                    {
                        max_fill = fill;
                    }

                    smc_fifo_put_test_sample( &s );

                    SMC_TEST_TRACE_PRINTF_INFO( "FIFO: PUT > 0x%08X: %4d flags: 0x%08X", (uint32_t)s.ptr, s.length, s.flags );

                    ret = smc_fifo_put_ext( smc_test_fifo, (uint32_t)(s.ptr), s.length, (uint32_t)s.flags );

                    if( ret == SMC_FIFO_ERROR_FIFO_FULL )
                    {
                        SMC_TEST_TRACE_PRINTF_ERROR( "FIFO: FIFO is FULL");
                        return SMC_ERROR;
                    }
                }
                else
                {
                    SMC_TEST_TRACE_PRINTF_INFO( "FIFO: PUT --> (case %d, fifo size %d, fill %d) [FIFO FULL]", test_case, fifo_len, fill );
                }
            }
            else
            {
                /* get */
                SMC_TEST_TRACE_PRINTF_INFO( "FIFO: GET <-- (case %d, fifo size %d, fill %d)", test_case, fifo_len, fill );
                fifo_count = smc_fifo_peek( smc_test_fifo, FALSE );

                if ( fifo_count > 0 )
                {
                    fifo_test_sample_t s;
                    fifo_test_sample_t sref;

                    smc_fifo_get_test_sample( &sref );
                    --fill;

                    /*SMC_TEST_TRACE_PRINTF_INFO( "FIFO: GET FROM FIFO (count %d, fifo size %d)...", fifo_count, fifo_len);*/

                    smc_fifo_get( smc_test_fifo, (uint32_t*)&(s.ptr), &(s.length), (uint32_t*)&(s.flags));

                    SMC_TEST_TRACE_PRINTF_INFO( "FIFO: GET < 0x%08X: %4d flags: 0x%08X", (uint32_t)s.ptr, s.length, s.flags );

                    if ( s.ptr != sref.ptr || s.length != sref.length || s.flags != sref.flags)
                    {
                        SMC_TEST_TRACE_PRINTF_ERROR( "FIFO: PTR: 0x%08X != 0x%08X OR LEN: %d != %d OR FLAGS: 0x%08X != 0x%08X", (uint32_t)s.ptr, (uint32_t)sref.ptr, s.length, sref.length, s.flags, sref.flags );
                        return SMC_ERROR;
                    }
                }
                else
                {
                    // Too much trace if enabled
                    SMC_TEST_TRACE_PRINTF_INFO("FIFO: GET <-- (case %d) [FIFO EMPTY (count %d, fifo size %d, fill %d)]", test_case, fifo_count, fifo_len, fill );
                }
            }

            /*SMC_TEST_TRACE_PRINTF_INFO( "FIFO: Read until empty..." );*/

            fifo_count = smc_fifo_peek( smc_test_fifo, FALSE );

            while( fifo_count > 0 )
            {
                fifo_test_sample_t s;
                fifo_test_sample_t sref;

                smc_fifo_get_test_sample( &sref );
                --fill;

                /*SMC_TEST_TRACE_PRINTF_INFO( "FIFO: GET FROM FIFO (count %d, fifo size %d)...", fifo_count, fifo_len);*/

                smc_fifo_get( smc_test_fifo, (uint32_t*)&(s.ptr), &(s.length), (uint32_t*)&(s.flags));

                SMC_TEST_TRACE_PRINTF_INFO( "FIFO: GET < 0x%08X: %4d flags: 0x%08X", (uint32_t)s.ptr, s.length, s.flags );

                if ( s.ptr != sref.ptr || s.length != sref.length || s.flags != sref.flags)
                {
                    SMC_TEST_TRACE_PRINTF_ERROR( "FIFO: PTR: 0x%08X != 0x%08X OR LEN: %d != %d OR FLAGS: 0x%08X != 0x%08X", (uint32_t)s.ptr, (uint32_t)sref.ptr, s.length, sref.length, s.flags, sref.flags );
                    return SMC_ERROR;
                }

                fifo_count = smc_fifo_peek( smc_test_fifo, FALSE );
            }

            /*SMC_TEST_TRACE_PRINTF_INFO( "FIFO: Read empty, check that fill is zero.." );*/

            if( fill != 0 )
            {
                SMC_TEST_TRACE_PRINTF_ERROR( "FIFO: Fill state %d is invalid, read and write counts does not match", fill );
                return SMC_ERROR;
            }
        }

        SMC_TEST_TRACE_PRINTF_INFO( "FIFO TEST COMPLETED: FIFO Len %2d, MAX Fill %2d, Test Case Factor %d", fifo_len, max_fill, TEST_CASE_FACTOR );
        SMC_TEST_TRACE_PRINTF_INFO( "====================================================================");
    }

    return SMC_OK;
}



/* EOF */


