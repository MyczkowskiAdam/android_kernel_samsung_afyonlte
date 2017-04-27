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

Version:       2    21-Dec-2011     Janne Mahosenaho
Status:        draft
Description :  More testcases added.
-------------------------------------------------------------------------------
Version:       1    11-Nov-2011     Heikki Siikaluoma
Status:        draft
Description :  File created
-------------------------------------------------------------------------------
*/
#endif

#include "smc_common_includes.h"

#include "smc_conf.h"
#include "smc_mdb.h"
#include "smc.h"
#include "smc_test.h"
#include "smc_test_fifo.h"
#include "smc_test_mdb.h"
#include "smc_trace.h"

#define MDB_TEST_MAP_REF(x) ((char)(( 0xFF0 & (uint32_t)ptrs[x]) >> 4))
#define MDB_TEST_BYTES   1024
#define MDB_TEST_PTRS    51
#define MDB_TEST_POOL    (MDB_TEST_PTRS - 1) * MDB_TEST_BYTES
#define MDB_TEST_RUNS    5

static void *ut_malloc(smc_channel_t* smc_channel, size_t sz);
static void ut_free(smc_channel_t* smc_channel, void *ptr, size_t oldsz);

    // Local mdb test case functions
uint8_t smc_test_case_function_mdb( uint8_t* test_input_data, uint16_t test_input_data_len )
{
    uint8_t test_result = SMC_ERROR;
    uint16_t data_index = 0;
    uint8_t test_case;
    
    void *map_pool;
    uint32_t map[20] =
                  { 
                    0x80000400,
                    0x80000410,
                    0x80000312,
                    0x80000100,
                    0x00000002,
                    0x80000312,
                    0x80000500,
                    0x00000000,
                    0x00000001,
                    0x80000020,
                    0x80000200,
                    0x00000006,
                    0x80000300,
                    0x80000200,
                    0x00000003,
                    0x00000004,
                    0x00000005,
                    0x00000007,
                    0x00000008,
                    0x00000009
                  };

    SMC_TEST_TRACE_PRINTF_INFO("MDB test case invoked, input data len %d", test_input_data_len);

    if ( test_input_data_len > 0 )
    {

        test_case = test_input_data[data_index];
        data_index++;
        test_input_data_len--;

        switch ( test_case )
        {
            //MDB create and destroy
            case 0x00:
            {
                test_result = smc_test_mdb_channel_test1();
            }
            break;
            
            // allocation and deallocation
            case 0x01:
            {
                test_result = smc_test_mdb_alloc_dealloc();
            }
            break;

            case 0x02:
            {
                test_result = smc_test_mdb_perf();
            }   
            break;

            case 0x03:
            {
                smc_channel_t smc_map_test_channel;
                map_pool = SMC_MALLOC( sizeof(void*) * 1024 * 60 );
                smc_mdb_create_pool_out( map_pool, sizeof(void*) * 1024 * 60 );
                smc_map_test_channel.smc_mdb_info = smc_mdb_channel_info_create();
                smc_map_test_channel.smc_mdb_info->pool_out = map_pool;
                smc_map_test_channel.smc_mdb_info->total_size_out = ( sizeof(void*) * 1024 * 60 );
                test_result = smc_test_mdb_map_test( map, 10, 10, &smc_map_test_channel );
                smc_mdb_info_destroy( smc_map_test_channel.smc_mdb_info );
                SMC_FREE( map_pool );
            }
            break;
            
            //address check
            case 0x04:
            {
                test_result = smc_test_mdb_address_check();                
                
            }
            break;

            // deallocation from wrong pool
            case 0x05:
            {
                test_result = smc_test_mdb_dealloc_wrong_pool();
            }
            break;

            // mdb_copy test
            case 0x06:
            {
                test_result = smc_test_mdb_copy_test(); 
            }
            break;

            case 0x07:
            {
                test_result = SMC_OK;
                smc_test_mdb_random_alloc();
            }
            break;

            default:
            {
                SMC_TEST_TRACE_PRINTF_ERROR("MDB: invalid test case 0x%02X", test_input_data[0]);
                test_result = SMC_ERROR;
                break;
            }
        }
    }
    else
    {
        SMC_TEST_TRACE_PRINTF_ERROR("MDB test case id missing (input data [0])");
        test_result = SMC_ERROR;
    }

    SMC_TEST_TRACE_PRINTF_INFO("MDB test case completed with return value 0x%02X", test_result);

    return test_result;
}

uint8_t smc_test_mdb_channel_test1()
{
    int8_t mdb_result;
    uint32_t size;
    void *pool;
    smc_channel_t smc_channel;
    
    mdb_result = 0;
    size = 1000 * 1024; /*SMC_BYTES_TO_32BIT(test_input_data[data_index]);*/
    pool = SMC_MALLOC(2 * size);
    SMC_TEST_TRACE_PRINTF_INFO("MDB create and destroy");
    assert(pool != NULL);

    mdb_result = smc_mdb_create_pool_out( pool, size);
    smc_channel.smc_mdb_info = smc_mdb_channel_info_create();
    smc_channel.smc_mdb_info->pool_out = pool;
    smc_channel.smc_mdb_info->total_size_out = size;
    smc_mdb_info_destroy( smc_channel.smc_mdb_info );

    SMC_FREE(pool);
    if (mdb_result >= 0)
    {
        return SMC_OK;
    }
    return SMC_ERROR;
}

uint8_t smc_test_mdb_address_check()
{
    uint8_t test_result;
    void* pool_out; 
    void* pool_in;
    uint8_t mdb_result;
    smc_channel_t smc_channel;

    pool_out = SMC_MALLOC(5000);
    pool_in  = SMC_MALLOC(6000);
    
    test_result = SMC_OK;
    mdb_result = FALSE;

    smc_mdb_create_pool_out( pool_out, 5000);

    /* The SMC channel must be initialized with the pool info */
    smc_channel.smc_mdb_info = smc_mdb_channel_info_create();
    smc_channel.smc_mdb_info->pool_out = pool_out;
    smc_channel.smc_mdb_info->total_size_out = 5000;
    smc_channel.smc_mdb_info->pool_in = pool_in;
    smc_channel.smc_mdb_info->total_size_in = 6000;

    mdb_result = smc_mdb_address_check(&smc_channel, (uint8_t*)pool_out + 200, SMC_MDB_OUT);
    if (mdb_result == FALSE)
    {
        test_result = SMC_ERROR;
    }
    mdb_result = smc_mdb_address_check(&smc_channel, (uint8_t*)pool_out + 5200, SMC_MDB_OUT);
    if (mdb_result != FALSE)
    {
        test_result = SMC_ERROR;
    }
    mdb_result = smc_mdb_address_check(&smc_channel, (uint8_t*)pool_in + 200, SMC_MDB_IN);
    if (mdb_result == FALSE)
    {
        test_result = SMC_ERROR;
    }
    mdb_result = smc_mdb_address_check(&smc_channel, (uint8_t*)pool_in + 6200, SMC_MDB_IN);
    if (mdb_result != FALSE)
    {
        test_result = SMC_ERROR;
    }

    smc_mdb_info_destroy( smc_channel.smc_mdb_info );
    SMC_FREE( pool_in );
    SMC_FREE( pool_out );

    return test_result;
}

uint8_t smc_test_mdb_alloc_dealloc( )
{

    uint8_t result;
    int test_run;
    int i, j;
    char ref;
    char *test_ptrs[MDB_TEST_PTRS];
    char *test_ptr;
    uint32_t total_alloc;
    void *pool; 
    smc_channel_t smc_channel;
    
    pool = SMC_MALLOC( MDB_TEST_POOL );

    result = SMC_OK;
    test_run = MDB_TEST_RUNS;
    total_alloc = 0;

    SMC_TEST_TRACE_PRINTF_INFO("MDB alloc and dealloc test");
    smc_mdb_create_pool_out(pool, MDB_TEST_POOL );
    smc_channel.smc_mdb_info = smc_mdb_channel_info_create();
    smc_channel.smc_mdb_info->pool_out = pool;

    while (test_run--)
    {
        ref = (char)(rand() & 0xFF);

        // allocate pointers from MDB pool until it is depleted
        for (i = 0; i < MDB_TEST_PTRS; i++)
        {

            test_ptrs[i] = (char*)smc_mdb_alloc( &smc_channel, MDB_TEST_BYTES );
            if (!test_ptrs[i])
            {
                total_alloc = i * MDB_TEST_BYTES;
                SMC_TEST_TRACE_PRINTF_INFO("MDB pool depleted. Allocated %d bytes.", total_alloc );
                break;
            }
            else if (i >= MDB_TEST_POOL / MDB_TEST_BYTES)
            {
                SMC_TEST_TRACE_PRINTF_INFO("MDB allocation from depleted pool returned pointer: 0x%08X, test FAIL.", (uint32_t)test_ptrs[i]);
                result = SMC_ERROR;
                break;
            }
            else
            {
                memset(test_ptrs[i], (char)((ref * i) & 0xFF), MDB_TEST_BYTES); 
            }
        }

        // free all allocated pointers
        for (i--;i >= 0; i--)
        {
            for (j = MDB_TEST_BYTES - 1; j >= 0; j--)
            {
                if (test_ptrs[i][j] != (char)((ref * i) & 0xFF))
                {
                    SMC_TEST_TRACE_PRINTF_INFO("MDB test_ptr[%d] corrupted! test_ptrs[%d][%d] = 0x%02X, expected 0x%02X", i, i, j, test_ptrs[i][j], (ref * i) & 0xFF );
                    result = SMC_ERROR;
                }
            }
            smc_mdb_free( &smc_channel, (void*)test_ptrs[i] );
        }
        
        if (result == SMC_ERROR)
        {
            break;
        }

        // try to allocate one big block to ensure that all pointers were freed and
        // the pool has not fragmented
        test_ptr = (char*)smc_mdb_alloc( &smc_channel, total_alloc );
        if (!test_ptr)
        {
            SMC_TEST_TRACE_PRINTF_INFO("MDB big block allocation test fail.");
            result = SMC_ERROR;
            break;
        }
        else
        {
            smc_mdb_free( &smc_channel, (void*)test_ptr );
        }

        SMC_TEST_TRACE_PRINTF_INFO("MDB allocation test run %d OK", MDB_TEST_RUNS - test_run);
    }

    //smc_mdb_destroy( ch_id );
    smc_mdb_info_destroy( smc_channel.smc_mdb_info );
    SMC_FREE( pool );
    return result;
}




uint8_t smc_test_mdb_perf()
{

#ifdef SMC_INCLUDE_PERF_TEST
#ifdef SMECO_MODEM
    uint32_t start_time;
    uint32_t stop_time;
    uint32_t *ptr[20];
    void *pool;
    
    SMC_TEST_TRACE_PRINTF_INFO("MDB allocator perf test");
    pool = SMC_MALLOC( MDB_TEST_POOL );
    
    smc_channel_t smc_channel;
    smc_channel.smc_mdb_info = smc_mdb_channel_info_create();
    smc_mdb_create_pool_out( pool, MDB_TEST_POOL );
    smc_channel.smc_mdb_info->pool_out = pool;
    smc_channel.smc_mdb_info->total_size_out = MDB_TEST_POOL;

    hw_arm_cp15_perf_cntr_init( 0, 0, 0 );
    hw_arm_cp15_perf_cntr_start();

    os_conditional_int_disable();
    start_time = hw_arm_cp15_perf_ccnt_read();


    ptr[0] = smc_mdb_alloc( &smc_channel, 128 );
    ptr[1] = smc_mdb_alloc( &smc_channel, 1280 );
    ptr[2] = smc_mdb_alloc( &smc_channel, 280 );
    ptr[3] = smc_mdb_alloc( &smc_channel, 2800 );
    ptr[4] = smc_mdb_alloc( &smc_channel, 128 );
    ptr[5] = smc_mdb_alloc( &smc_channel, 28 );
    ptr[6] = smc_mdb_alloc( &smc_channel, 4128 );
    ptr[7] = smc_mdb_alloc( &smc_channel, 1428 );
    ptr[8] = smc_mdb_alloc( &smc_channel, 328 );
    ptr[9] = smc_mdb_alloc( &smc_channel, 15280 );
    ptr[10] = smc_mdb_alloc( &smc_channel, 128 );
    ptr[11] = smc_mdb_alloc( &smc_channel, 1280 );
    ptr[12] = smc_mdb_alloc( &smc_channel, 280 );
    ptr[13] = smc_mdb_alloc( &smc_channel, 2800 );
    ptr[14] = smc_mdb_alloc( &smc_channel, 28 );
    ptr[15] = smc_mdb_alloc( &smc_channel, 128 );
    ptr[16] = smc_mdb_alloc( &smc_channel, 4128 );
    ptr[17] = smc_mdb_alloc( &smc_channel, 1428 );
    ptr[18] = smc_mdb_alloc( &smc_channel, 128 );
    ptr[19] = smc_mdb_alloc( &smc_channel, 5280 );
    stop_time = hw_arm_cp15_perf_ccnt_read();
    os_conditional_int_enable();

    SMC_TEST_TRACE_PRINTF_INFO("MDB allocation perf test 20 allocations: %d cycles, avg: %d cycles per allocation", stop_time - start_time, (stop_time - start_time) / 20);

    os_conditional_int_disable();
    start_time = hw_arm_cp15_perf_ccnt_read();

    smc_mdb_free( &smc_channel, ptr[0] );
    smc_mdb_free( &smc_channel, ptr[1] );
    smc_mdb_free( &smc_channel, ptr[2] );
    smc_mdb_free( &smc_channel, ptr[3] );
    smc_mdb_free( &smc_channel, ptr[4] );
    smc_mdb_free( &smc_channel, ptr[5] );
    smc_mdb_free( &smc_channel, ptr[6] );
    smc_mdb_free( &smc_channel, ptr[7] );
    smc_mdb_free( &smc_channel, ptr[8] );
    smc_mdb_free( &smc_channel, ptr[9] );
    smc_mdb_free( &smc_channel, ptr[10] );
    smc_mdb_free( &smc_channel, ptr[11] );
    smc_mdb_free( &smc_channel, ptr[12] );
    smc_mdb_free( &smc_channel, ptr[13] );
    smc_mdb_free( &smc_channel, ptr[14] );
    smc_mdb_free( &smc_channel, ptr[15] );
    smc_mdb_free( &smc_channel, ptr[16] );
    smc_mdb_free( &smc_channel, ptr[17] );
    smc_mdb_free( &smc_channel, ptr[18] );
    smc_mdb_free( &smc_channel, ptr[19] );
    stop_time = hw_arm_cp15_perf_ccnt_read();
    os_conditional_int_enable();

    SMC_TEST_TRACE_PRINTF_INFO("MDB allocation perf test 20 deallocations: %d cycles, avg: %d cycles per deallocation", stop_time - start_time, (stop_time - start_time) / 20);
    
    smc_mdb_info_destroy( smc_channel.smc_mdb_info );
    SMC_FREE( pool );
#else
    SMC_TEST_TRACE_PRINTF_INFO("MDB allocation perf test available only in modem");
#endif

#else
SMC_TEST_TRACE_PRINTF_INFO("MDB allocation perf test not enabled");
#endif
    return SMC_OK;
}

uint8_t smc_test_mdb_map_test( uint32_t *map, uint32_t allocs, uint32_t deallocs, smc_channel_t* smc_channel )
{
    int i, j, k;
    uint8_t result;
    uint32_t *sizes;
    size_t ptr_size;
    char **ptrs;

    ptrs =  SMC_MALLOC( sizeof(char*) * allocs );
    sizes = SMC_MALLOC( sizeof(uint32_t) * allocs );
    result = SMC_OK;;
    
    j = 0;

    for (i = 0; i < (allocs + deallocs); i++)
    {
        // msb == 1 -> allocation
        if (map[i] >> 31)
        {
            sizes[j] = map[i] & ~( 1U << 31 );
            ptrs[j] = (char*)smc_mdb_alloc( smc_channel, sizes[j]);
            

            if (ptrs[j] == NULL)
            {
                SMC_TEST_TRACE_PRINTF_ERROR("MDB allocation map test FAIL at map[%d] (0x%08X)", i, map[i]);
                result = SMC_ERROR;
                break;
            }

            if (smc_mdb_address_check(smc_channel, ptrs[j], SMC_MDB_OUT) == FALSE)
            {
                SMC_TEST_TRACE_PRINTF_ERROR("MDB allocation map test FAIL at map[%d] (0x%08X). Allocated pointer not in pool.", i, map[i]);
                result = SMC_ERROR;
                break;
            }
            
            ptr_size = tlsf_ptr_size(ptrs[j]);

            if (ptr_size < sizes[j])
            {
                SMC_TEST_TRACE_PRINTF_ERROR("MDB allocation map test FAIL at map[%d] (0x%08X). Allocated size: %d bytes, required minimum: %d bytes.", i, map[i], ptr_size, sizes[j]);
                result = SMC_ERROR;
                break;
            }

            memset( ptrs[j], MDB_TEST_MAP_REF(j), sizes[j] );
            j++;
        }
        // msb == 0 -> deallocation    
        else if (map[i] < j && ptrs[map[i]] != NULL)
        {
            for (k = sizes[map[i]] - 1; k >= 0; k--)
            {
                if(ptrs[map[i]][k] != MDB_TEST_MAP_REF(map[i]))
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("MDB allocation map test FAIL at prts[%d][%d] (0x%08X). Data corrupted! data: 0x%02X, extected: 0x%02X", map[i], k, (uint32_t) ptrs[map[i]], ptrs[j][k], MDB_TEST_MAP_REF(map[i]));
                    result = SMC_ERROR;
                    break;
                }
            }
            smc_mdb_free( smc_channel, ptrs[map[i]] );
            ptrs[map[i]] = NULL;
        }
        else
        {
            SMC_TEST_TRACE_PRINTF_ERROR("MDB allocation map test FAIL at map[%d] (0x%08X)", i, map[i]);
            result = SMC_ERROR;
            break;
        }
    }

    // free all pointers
    for (i = 0; i < allocs; i++)
    {
        if(ptrs[i] != NULL)
        {
            smc_mdb_free( smc_channel, ptrs[i] );
            ptrs[map[i]] = NULL;
        }
    }

    SMC_FREE( sizes );
    SMC_FREE( ptrs );

    return result;
}

uint8_t smc_test_mdb_dealloc_wrong_pool()
{
    uint8_t test_result;
    void *pool_ch0;
    void *pool_ch1;
    uint8_t *ptr;
    int i;
    smc_channel_t smc_channel0;
    smc_channel_t smc_channel1;

    pool_ch0 = SMC_MALLOC( 10 * 1024 );
    pool_ch1 = SMC_MALLOC( 10 * 1024 );

    smc_mdb_create_pool_out( pool_ch0, 10 * 1024 );
    smc_mdb_create_pool_out( pool_ch1, 10 * 1024 );
    
    smc_channel0.smc_mdb_info = smc_mdb_channel_info_create();
    smc_channel1.smc_mdb_info = smc_mdb_channel_info_create();

    smc_channel0.smc_mdb_info->pool_out = pool_ch0;
    smc_channel0.smc_mdb_info->total_size_out = 10 * 1024;
    smc_channel1.smc_mdb_info->pool_out = pool_ch1;
    smc_channel1.smc_mdb_info->total_size_out = 10 * 1024;

    test_result = SMC_OK;
    
    ptr = smc_mdb_alloc( &smc_channel0, 1024 );
    memset( ptr, 0x42, 1024 );
    smc_mdb_free( &smc_channel1, ptr );

    for (i = 0; i < 1024; i++)
    {
        if (ptr[i] != 0x42)
        {
            SMC_TEST_TRACE_PRINTF_ERROR("MDB pointer corrupted by deallocation from wrong pool");
            test_result = SMC_ERROR;
            break;
        }
    }

    smc_mdb_free( &smc_channel0, ptr );

    smc_mdb_info_destroy( smc_channel0.smc_mdb_info );
    smc_mdb_info_destroy( smc_channel1.smc_mdb_info );

    SMC_FREE( pool_ch0 );
    SMC_FREE( pool_ch1 );

    return test_result;
}

uint8_t smc_test_mdb_copy_test()
{
    uint8_t test_result;
    char *source;
    char *target;
    void *pool;
    int i, j;
    smc_channel_t smc_channel;

    test_result = SMC_OK;

    pool = SMC_MALLOC( 5 * 1024 );
    smc_mdb_create_pool_out( pool, 5 * 1024 );
    
    smc_channel.smc_mdb_info = smc_mdb_channel_info_create();
    smc_channel.smc_mdb_info->pool_out = pool;

    source = SMC_MALLOC( 1024 );
    target = smc_mdb_alloc( &smc_channel, 1024 );

    for (i = 0; (i < 1024 - 4) && (test_result == SMC_OK); i++)
    {
        memset( source, 0xFF, 1024 );
        memset( &source[4], 0x42, 1024 - (4 + i) );
        memset( target, 0xAA, 1024 );
        smc_mdb_copy( &target[4], &source[4], 1024 - (4 + i) );
        
        for (j = 0; j < 1024; j++)
        {
            if (j < 4 || j >= 1024 - i)
            {
                if (target[j] != 0xAA || source[j] != 0xFF)
                {
                    SMC_TEST_TRACE_PRINTF_ERROR("MDB copy test fail! target[%d]=0x%02X (expected 0xAA), source[%d]=0x%02X (expected 0xFF)", j, target[j], j, source[j]);
                    test_result = SMC_ERROR;
                    break;
                }
            }
            else if (target[j] != 0x42 || source[j] != 0x42)
            {
                SMC_TEST_TRACE_PRINTF_ERROR("MDB copy test fail! target[%d]=0x%02X (expected 0x42), source[%d]=0x%02X (expected 0x42)", j, target[j], j, source[j]);
                test_result = SMC_ERROR;
                break;
            }
        }
    }

    smc_mdb_free( &smc_channel, target );
    smc_mdb_info_destroy( smc_channel.smc_mdb_info );

    SMC_FREE( source );
    SMC_FREE( pool );

    return test_result;
}

static void *ut_malloc(smc_channel_t* smc_channel, size_t sz)
{
    uint8_t ref;
    void *pv;
    size_t up;

    ref = (uint8_t)((sz) & 0xFF);
    pv = smc_mdb_alloc(smc_channel, sz);
    assert(((uint32_t)pv & 31) == 0); // Cache aligned
    if (pv)
    {
        up = (sz + 31) & (~31);  // simulate DMA
        memset(pv, ref, up);
        SMC_TEST_TRACE_PRINTF_INFO("Requested %d bytes, allocated %d bytes, ref %d", sz, tlsf_ptr_size(pv), ref);
    }
    return pv;
}

static void ut_free(smc_channel_t* smc_channel, void *ptr, size_t oldsz)
{
    uint8_t ref;
    int i;
    uint8_t *pt;

    if (ptr)
    {
        pt = (uint8_t*)ptr;
        ref = (uint8_t)((oldsz) & 0xFF);
        for (i = 0; i < oldsz; ++i)
        {
            if (pt[i] != ref)
                SMC_TEST_TRACE_PRINTF_INFO("FATAL 0x%02X != 0x%02X @%d (0x%08X)",
                                            pt[i], ref, i, (uint32_t)ptr);
            assert(pt[i] == ref);
        }
        smc_mdb_free(smc_channel, ptr);
    }
}

#define MAX_ALLOCATIONS 123
#define MAX_ALLOC_SIZE  11003 /*31101*/
#define NR_LOOPS        200
#define TESTS_IN_LOOP   200

void smc_test_mdb_random_alloc(void)
{
    int i, j, rnd1, rnd2, rnd3;

    void *mdb_pool;
    void **references; // [MAX_ALLOCATIONS] = {0};
    size_t *sizes; //[MAX_ALLOCATIONS] = {0};
    smc_channel_t smc_channel;

#ifdef MDB_REALLOC_TEST
    void *pn;
    const int NRCASES = 5;
#else
    const int NRCASES = 2;
#endif

    references = SMC_MALLOC(MAX_ALLOCATIONS * sizeof(void*));
    memset(references, 0, MAX_ALLOCATIONS * sizeof(void*));
    sizes = SMC_MALLOC(MAX_ALLOCATIONS * sizeof(size_t));
    memset(sizes, 0, MAX_ALLOCATIONS * sizeof(size_t));
    mdb_pool = SMC_MALLOC(MAX_ALLOCATIONS * 1024 * sizeof(void*) );
    assert(mdb_pool != NULL); // no pool for channel

    SMC_TEST_TRACE_PRINTF_INFO("MDB MALLOC TEST: random alloc test started");

    smc_mdb_create_pool_out( mdb_pool, MAX_ALLOCATIONS * 1024 * sizeof(void*) );
    smc_channel.smc_mdb_info = smc_mdb_channel_info_create();
    smc_channel.smc_mdb_info->pool_out = mdb_pool;

    for (i = 0; i < NR_LOOPS; ++i)
    {
        for (j = 0; j < TESTS_IN_LOOP; j++)
        {
            rnd1 = (int)(rand() % MAX_ALLOCATIONS);
            rnd2 = (int)(rand() % MAX_ALLOC_SIZE);
            rnd3 = (int)(rand() % NRCASES);
            
            switch(rnd3)
            {
            case 0: // Allocate
                if (!references[rnd1] && rnd2 > 0)
                {
                    references[rnd1] = ut_malloc(&smc_channel, rnd2);
                    sizes[rnd1] = rnd2;
                }
                break;
            case 1: // Free
                ut_free(&smc_channel, references[rnd1], sizes[rnd1]);
                references[rnd1] = NULL;
                sizes[rnd1] = 0;
                break;
#ifdef MDB_REALLOC_TEST
            case 2: // Reallocate
                pn = ut_realloc(references[rnd1], rnd2, sizes[rnd1]);
                if (references[rnd1] != pn) // New alloc succeeded
                {
                    references[rnd1] = pn;
                    sizes[rnd1] = rnd2;
                }
                break;
            case 3: // Aligned alloc
                if (!references[rnd1])
                {
                    references[rnd1] = ut_aligned(rnd2, 32); /* XXX */
                    sizes[rnd1] = rnd2;
                }
                break;
#endif
            default:
                assert(0);
            }
        } /* endfor j */

        SMC_TEST_TRACE_PRINTF_INFO("MDB MALLOC TEST: run %d ends", i);
    } /* endfor i */

    /* Deallocate all */
    for (i = 0; i < MAX_ALLOCATIONS; ++i)
    {
        ut_free(&smc_channel, references[i], sizes[i]);
    }
    SMC_FREE(references);
    SMC_FREE(sizes);

    SMC_TEST_TRACE_PRINTF_INFO("MDB usable space at lowest: %d bytes (valid only if TLSF_STATISTICS enabled on smc_mdb.c)", smc_mdb_channel_free_space_min_get( &smc_channel ));
    
    smc_mdb_info_destroy( smc_channel.smc_mdb_info );
    SMC_FREE( mdb_pool );

    SMC_TEST_TRACE_PRINTF_INFO("MDB MALLOC TEST: task ends");
    
}

