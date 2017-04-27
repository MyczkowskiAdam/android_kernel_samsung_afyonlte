/*
 * drivers/sec_hal/rt/test/src/sec_hal_test_dispatcher.c
 *
 * Copyright (c) 2010-2013, Renesas Mobile Corporation.
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

#include "sec_hal_rt.h"
#define SEC_HAL_TRACE_LOCAL_ENABLE
#include "sec_hal_rt_trace.h"
#include "sec_hal_rt_cmn.h"
#include "sec_hal_cmn.h"
#include "sec_serv_api.h"
#include "sec_msg.h"
#include "tee_defs.h"
#include "sec_hal_tee.h"
#include <stdarg.h>
#include <linux/string.h>
#include <linux/slab.h>


/* ***************** MACROS, CONSTANTS, COMPILATION FLAGS ****************** */
/* local macros */
#define DATA_SIZE_MAX 50
#define PPFLAGS_COUNT 10

#define OEMCrypto_SUCCESS                             0x00000000
#define OEMCrypto_ERROR_INIT_FAILED                   0x00000001
#define OEMCrypto_ERROR_TERMINATE_FAILED              0x00000002
#define OEMCrypto_ERROR_ENTER_SECURE_PLAYBACK_FAILED  0x00000003
#define OEMCrypto_ERROR_EXIT_SECURE_PLAYBACK_FAILED   0x00000004
#define OEMCrypto_ERROR_SHORT_BUFFER                  0x00000005
#define OEMCrypto_ERROR_NO_DEVICE_KEY                 0x00000006
#define OEMCrypto_ERROR_NO_ASSET_KEY                  0x00000007
#define OEMCrypto_ERROR_KEYBOX_INVALID                0x00000008
#define OEMCrypto_ERROR_NO_KEYDATA                    0x00000009
#define OEMCrypto_ERROR_NO_CW                         0x00000010
#define OEMCrypto_ERROR_DECRYPT_FAILED                0x00000011
#define OEMCrypto_ERROR_WRITE_KEYBOX                  0x00000012
#define OEMCrypto_ERROR_WRAP_KEYBOX                   0x00000013
#define OEMCrypto_ERROR_BAD_MAGIC                     0x00000014
#define OEMCrypto_ERROR_BAD_CRC                       0x00000015
#define OEMCrypto_ERROR_NO_DEVICEID                   0x00000016
#define OEMCrypto_ERROR_RNG_FAILED                    0x00000017
#define OEMCrypto_ERROR_RNG_NOT_SUPPORTED             0x00000018
#define OEMCrypto_ERROR_SETUP                         0x00000019

/* volatile data */
static uint32_t g_status = SEC_SERV_STATUS_OK;
static uint32_t g_disp_status = SEC_ROM_RET_OK;
static uint32_t g_cert_address = 0x00;
static uint32_t g_data_address = 0x00;
static char g_simlock_unlock_codes[5][17];
static uint32_t g_rpc_handler;
static uint32_t g_coma_params[4] = {0};
static uint32_t g_memcpy_params[3] = {0};
static uint32_t g_a3sp_state_info[2] = {};
static void *sec_msg_ptr = 0;

/* const data */
static const uint8_t k_auth_data[DATA_SIZE_MAX] =
    {0,1,2,3,4,5,6,7,8,9,
     10,11,12,13,14,15,16,17,18,19,
     20,21,22,23,24,25,26,27,28,29,
     30,31,32,33,34,35,36,37,38,39,
     40,41,42,43,44,45,46,47,48,49};
static const uint8_t k_mac_address[SEC_HAL_MAC_SIZE] =
    {0xa1,0xb2,0xc3,0xd4,0xe5,0xf6};
static const char k_imei[SEC_HAL_MAX_IMEI_SIZE] =
    {'a','b','c','d','e','f','g','h','1','2','3','4','5','6','7',0};
static const char k_keyinfo[SEC_HAL_KEY_INFO_SIZE] =
    {'A','B','C','D','E','F','G','H',
     'I','J','K','L','M','N','O','P',
     'Q','R','S','T','X','Y','0','1',
     '2','3','4','5','6','7','8','9'};
static const uint32_t k_default_timeout_value = 30000;
static const uint32_t k_objid = 12;
static const char *k_correct_siml_unlock_code[] = {
    "code1",
    "code2",
    "code3",
    "code4",
    "code5"
};
/* These are SHA256 values from above codes. Convert to bin still needed. */
static const char *k_siml_verify_hash[] = {
    "9583a5e3de1040c177c921abe53d4b28845129d8b11c8a83a59900045325de65",
    "9d1e2cc14ea6a083da32d783f91367d8500de9244e8c17b54ad2ed3535bea4ec",
    "ed2b1b8a1c244ad3d53084882c6ee966fb9cd1a94786787f9ef1b0a07bae20dd",
    "c6681476a09bb796de70214be4b319928a3a818272388f56777f912d881050b5",
    "9b9d3b164d165d17f786b3c474b4bba628e3378fe607da8860b19cc980eacda3"
};
static const char *k_master_hash =
    "531f6d0840e65e0ab309ca0cf2db2f518d39df370f62dbee23690e5f813ac31a";
static const uint32_t k_drm_default_session_id = 127;
static const uint8_t k_drm_device_id[] =
    {'A','B','C','D','E','F','G','H',
     'I','J','K','L','M','N','O','P',
     'Q','R','S','T','X','Y','0','1',
     '2','3','4','5','6','7','8','9',
     'A','B','C','D','E','F','G','H',
     'I','J','K','L','M','N','O','P',
     'Q','R','S','T','X','Y','0','1',
     '2','3','4','5','6','7','8','9'};
static const uint32_t k_reset_info[] = {0x1122UL, 0x3344UL, 0x5566UL};
static const uint8_t k_pub_id[SEC_HAL_MAX_PUBLIC_ID_LENGTH] =
     {0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,0x00u,
      0xF0u,0xF1u,0xF2u,0xF3u,0xF4u,0xF5u,0xF6u,0xF7u,0xF8u,0xF9u};

void test_set_status(uint32_t status)
{
    g_status = status;
}

void test_set_disp_status(uint32_t status)
{
    g_disp_status = status;
}

int test_assert_random_number(int size, uint8_t* random_number)
{
    int i = 0, ret = 1;
    while(i<size && i<DATA_SIZE_MAX && ret)
    {
        ret = (k_auth_data[i] == random_number[i]);
        i++;
    }
    return ret;
}

int test_assert_key_info(uint8_t* key_info)
{
    int i = 0, ret = 1;
    while(i<SEC_HAL_KEY_INFO_SIZE && ret)
    {
        ret = (k_keyinfo[i]==key_info[i]);
        i++;
    }
    return ret;
}

int test_assert_cert_address(uint32_t *address)
{
    return (address == (uint32_t*)g_cert_address);
}

int test_assert_data_address(uint32_t *address)
{
    return (address == (uint32_t*)g_data_address);
}

int test_assert_objid(uint32_t objid)
{
    return (objid == k_objid);
}

int test_assert_mac_address(uint8_t* mac_address)
{
    int i = 0, ret = 1;
    while(i<SEC_HAL_MAC_SIZE && ret)
    {
        ret = (k_mac_address[i]==mac_address[i]);
        i++;
    }
    return ret;
}

int test_assert_imei(char* imei)
{
    int i = 0, ret = 1;
    while(i<SEC_HAL_MAX_IMEI_SIZE && ret)
    {
        ret = (k_imei[i]==imei[i]);
        i++;
    }
    return ret;
}

int test_assert_auth_data(uint8_t* input_buf,
                          uint8_t input_len,
                          uint8_t* output_buf)
{
    /* output should be two times the input */
    int i = 0, ret = 1;
    while(i<input_len && ret) /* first half */
    {
        ret = (input_buf[i]==output_buf[i]);
        i++;
    }
    i = 0;
    while(i<input_len && ret)/* second half */
    {
        ret = (input_buf[i]==output_buf[input_len+i]);
        i++;
    }
    return ret;
}

int test_assert_exp_time(uint32_t exp_time)
{
    return (k_default_timeout_value == exp_time);
}

int test_assert_rpc_handler(void* func_ptr)
{
    return (g_rpc_handler == (uint32_t)func_ptr);
}

int test_assert_coma_params(uint32_t param0, uint32_t param1, uint32_t param2, uint32_t param3)
{
    return (g_coma_params[0] == param0 &&
            g_coma_params[1] == param1 &&
            g_coma_params[2] == param2 &&
            g_coma_params[3] == param3);
}

int test_assert_a3sp_state_params(uint32_t param0, uint32_t param1)
{
    return (g_a3sp_state_info[0] == param0 &&
            g_a3sp_state_info[1] == param1);
}

int test_assert_memcpy_params(uint32_t param0, uint32_t param1, uint32_t param2)
{
    return (g_memcpy_params[0] == param0 &&
            g_memcpy_params[1] == param1 &&
            g_memcpy_params[2] == param2);
}

static const char* k_callback_hash = "5a36b50698fc9cec36087c93980d6580aa494d1b";
static int is_callback_req(const char* data, uint32_t* offs)
{
    int i = 0, ret = 1, len = strlen(k_callback_hash);

    while (ret && len > i) {
        ret = (k_callback_hash[i] == data[i]);
        i++;
    }
    if(offs) {
        *offs = (uint32_t)len;
    }

    return ret;
}
typedef uint32_t (*callback)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
typedef struct
{
  uint32_t id;
  uint32_t p1;
  uint32_t p2;
  uint32_t p3;
  uint32_t p4;
} callback_args_t;

/* ****************************************************************************
** ***************************************************************************/
uint32_t raw_pub2sec_dispatcher(uint32_t service_id, uint32_t flags, ...);
uint32_t raw_pub2sec_dispatcher(uint32_t service_id, uint32_t flags, ...)
{
	return 0;
}

/* ****************************************************************************
** ***************************************************************************/
uint32_t pub2sec_dispatcher(uint32_t service_id, uint32_t flags, ...);
uint32_t pub2sec_dispatcher(uint32_t service_id, uint32_t flags, ...)
{
    uint32_t ret_status = g_disp_status;
    sec_msg_t* sec_msg_out_msg;
    sec_msg_t* sec_msg_in_msg;
    sec_msg_handle_t out_handle, in_handle;
    va_list list;

    SEC_HAL_TRACE_ENTRY();
    va_start(list,flags);

    /*uint32_t spare_param = */ va_arg(list, uint32_t); /* spare not used. */
    sec_msg_out_msg = va_arg(list, sec_msg_t*);
    sec_msg_in_msg = va_arg(list, sec_msg_t*);
    sec_msg_open(&out_handle, (sec_msg_t *)SEC_HAL_MEM_PHY2VIR_FUNC(sec_msg_out_msg));
    sec_msg_open(&in_handle, (sec_msg_t *)SEC_HAL_MEM_PHY2VIR_FUNC(sec_msg_in_msg));



    switch(service_id)
    {
        case SEC_SERV_CERTIFICATE_REGISTER:
        {
            SEC_HAL_TRACE("SEC_SERV_CERTIFICATE_REGISTER");
            /* store cert address */
            sec_msg_param_read32(&in_handle, &g_cert_address);
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
        } break;
        case SEC_SERV_MAC_ADDRESS_REQUEST:
        {
            uint32_t input_data_size = 0x00;
            SEC_HAL_TRACE("SEC_SERV_MAC_ADDRESS_REQUEST");
            /* read input data size */
            sec_msg_param_read32(&in_handle, &input_data_size);
            /* write status */
            sec_msg_param_write32(&out_handle,
                    SEC_HAL_MAC_SIZE == input_data_size ?
                            g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            /* write macaddr */
            sec_msg_param_write(&out_handle,
                                k_mac_address,
                                SEC_HAL_MAC_SIZE, 0);
        } break;
        case SEC_SERV_IMEI_REQUEST:
        {
            uint32_t input_data_size = 0x00;
            SEC_HAL_TRACE("SEC_SERV_IMEI_REQUEST");
            /* read input data size */
            sec_msg_param_read32(&in_handle, &input_data_size);
            /* write status */
            sec_msg_param_write32(&out_handle,
                    SEC_HAL_MAX_IMEI_SIZE == input_data_size ?
                            g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            /* write imei */
            sec_msg_param_write(&out_handle,
                                k_imei,
                                SEC_HAL_MAX_IMEI_SIZE, 0);
        } break;
        case SEC_SERV_DEVICE_AUTH_DATA_REQUEST:
        {
            uint32_t offset = 0, ret = 0;
            uint8_t buffer[256] = {0};
            uint32_t input_data_size = 0x00;
            uint32_t auth_data_size = 0x00;
            SEC_HAL_TRACE("SEC_SERV_DEVICE_AUTH_DATA_REQUEST");
            /* read input data size */
            sec_msg_param_read32(&in_handle, &input_data_size);
            /* read input data */
            sec_msg_param_read(&in_handle, buffer, input_data_size);
            memcpy(buffer+input_data_size, buffer, input_data_size);
            /* read auth data size */
            sec_msg_param_read32(&in_handle, &auth_data_size);

            if(is_callback_req(buffer, &offset) && g_rpc_handler)
            {
                callback cb = (callback)g_rpc_handler;
                callback_args_t args;
                memcpy(&args, buffer+offset, sizeof(callback_args_t));
                if(SEC_SERV_RPC_ALLOC == args.id)
                {
                    ret = (*cb)(SEC_SERV_RPC_ALLOC, args.p1, args.p2, args.p3, args.p4);
                    if(!args.p4) /*to enable out of mem testing*/
                    {
                        (*cb)(SEC_SERV_RPC_FREE, ret, args.p2, args.p3, args.p4);
                    }
                }
                else if(SEC_SERV_RPC_PROT_DATA_ALLOC == args.id)
                {
                    ret = (*cb)(SEC_SERV_RPC_PROT_DATA_ALLOC, args.p1, args.p2, args.p3, args.p4);
                    if(!args.p4) /*to enable out of mem testing*/
                    {
                        (*cb)(SEC_SERV_RPC_PROT_DATA_FREE, ret, args.p2, args.p3, args.p4);
                    }
                }
                else{ret = 0;}
            }
            else
            {
                ret = (auth_data_size == 2*input_data_size);
            }

            /* write status */
            sec_msg_param_write32(&out_handle,
                    ret ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            /* write data */
            sec_msg_param_write(&out_handle, buffer, auth_data_size, 0);
        } break;
        case SEC_SERV_DEVICE_AUTH_DATA_SIZE_REQUEST:
        {
            uint32_t input_data_size = 0x00;
            SEC_HAL_TRACE("SEC_SERV_DEVICE_AUTH_DATA_SIZE_REQUEST");
            /* read input data size */
            sec_msg_param_read32(&in_handle, &input_data_size);
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
            /* write data size */
            sec_msg_param_write32(&out_handle, 2*input_data_size, 0);
        } break;
        case SEC_SERV_KEY_INFO_REQUEST:
        {
            uint32_t input_data_size = 0x00;
            SEC_HAL_TRACE("SEC_SERV_KEY_INFO_REQUEST");
            /* read input data size */
            sec_msg_param_read32(&in_handle, &input_data_size);
            /* write status */
            sec_msg_param_write32(&out_handle,
                    SEC_HAL_KEY_INFO_SIZE == input_data_size ?
                            g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            /* write data size */
            sec_msg_param_write(&out_handle,
                                k_keyinfo,
                                SEC_HAL_KEY_INFO_SIZE, 0);
        } break;
        case SEC_SERV_RANDOM_DATA_REQUEST:
        {
            uint32_t input_data_size = 0x00;
            SEC_HAL_TRACE("SEC_SERV_RANDOM_DATA_REQUEST");
            /* read input data size */
            sec_msg_param_read32(&in_handle, &input_data_size);
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
            /* write data */
            sec_msg_param_write(&out_handle,
                                k_auth_data,
                                (input_data_size<DATA_SIZE_MAX?input_data_size:DATA_SIZE_MAX), 0);
        } break;
       case SEC_SERV_INTEGRITY_CHECK:
        {
            uint32_t input_data_size = 0x00;
            SEC_HAL_TRACE("SEC_SERV_INTEGRITY_CHECK");
            /* read input data size */
            sec_msg_param_read32(&in_handle, &input_data_size);
            /* write status */
            sec_msg_param_write32(&out_handle,
                    sizeof(uint32_t) == input_data_size ?
                            g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            /* write data */
            sec_msg_param_write32(&out_handle, k_default_timeout_value, 0);
        } break;
        case SEC_SERV_PROT_DATA_REGISTER:
        {
            uint32_t data_size = 0x00;
            SEC_HAL_TRACE("SEC_SERV_PROT_DATA_REGISTER");
            /* store cert address & data address */
            sec_msg_param_read32(&in_handle, &g_cert_address);
            /* store data address */
            sec_msg_param_read32(&in_handle, &g_data_address);
            /* store data size */
            sec_msg_param_read32(&in_handle, &data_size);
            /* write status */
            sec_msg_param_write32(&out_handle, g_data_address ?
                    (data_size?g_status:SEC_SERV_STATUS_INVALID_INPUT) :
                    g_status, 0);
            /* write status */
            sec_msg_param_write32(&out_handle, k_objid, 0);
        } break;
        case SEC_SERV_SIMLOCK_CHECK_LOCKS:
        {
            int index = 0;
            uint32_t status = 0x00;
            uint32_t level_status = 0x00;
            uint32_t param_size = 0x00;
            SEC_HAL_TRACE("SEC_SERV_SIMLOCK_CHECK_LOCKS");
            /* read codes and write status */
            while (index < 5 && SEC_SERV_STATUS_OK == status)
            {
                sec_msg_param_size_get(&in_handle, (void*)&param_size);
                status = sec_msg_param_read(&in_handle, g_simlock_unlock_codes[index++], param_size);
            }
            sec_msg_param_write32(&out_handle,
                    (SEC_SERV_STATUS_OK == status &&
                     strlen(g_simlock_unlock_codes[0]) &&
                     strlen(g_simlock_unlock_codes[1]) &&
                     strlen(g_simlock_unlock_codes[2]) &&
                     strlen(g_simlock_unlock_codes[3]) &&
                     strlen(g_simlock_unlock_codes[4])) ?
                    g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            /* write simlock level status bitmask */
            index = 0;
            while (index < 5)
            {
                level_status |= (!strcmp(g_simlock_unlock_codes[index],
                                         k_correct_siml_unlock_code[index]) ?
                                         0x0 : 0x1<<index);
                index++;
            }
            sec_msg_param_write32(&out_handle, level_status, 0);
            memset(g_simlock_unlock_codes, 0, sizeof(char)*17*5);/*empty code array */
        } break;
        case SEC_SERV_SIMLOCK_CHECK_ONE_LOCK:
        {
            uint8_t level = 0;
            uint32_t status = 0x00;
            uint32_t param_size = 0x00;
            SEC_HAL_TRACE("SEC_SERV_SIMLOCK_CHECK_ONE_LOCK");
            /* read level */
            status = sec_msg_param_read8(&in_handle, &level);
            if (level <= 5 && SEC_SERV_STATUS_OK == status)
            {
                /* read code */
                sec_msg_param_size_get(&in_handle, (void*)&param_size);
                status = sec_msg_param_read(&in_handle,
                         g_simlock_unlock_codes[level-1], param_size);
            }
            if (SEC_SERV_STATUS_OK == status)
            {
                status = (!strcmp(g_simlock_unlock_codes[level-1],
                                  k_correct_siml_unlock_code[level-1]) ?
                                  g_status : SEC_SERV_STATUS_FAIL );
            }
            /* write status */
            sec_msg_param_write32(&out_handle, status, 0);
            memset(g_simlock_unlock_codes, 0, sizeof(char)*17*5);/*empty code array */
        } break;
        case SEC_SERV_SIMLOCK_GET_STATE:
        {
            SEC_HAL_TRACE("SEC_SERV_SIMLOCK_GET_STATE");
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
            /* write simlock level status bitmask */
            sec_msg_param_write32(&out_handle, 0x07, 0);
            sec_msg_param_write32(&out_handle, 0x06, 0);
            sec_msg_param_write32(&out_handle, 0x05, 0);
        } break;
        case SEC_SERV_RUNTIME_INIT:
        {
            uint32_t st[2] = {0}, commit_id_len = 0, reset_info_len = 0;
            uint64_t commit_id;
            commit_id = 0x123456789abcdef;
#if 0
            sec_msg_handle_t dummy;
            if (sec_msg_ptr == 0)
                sec_msg_ptr = sec_msg_alloc(&dummy, 1024, 0, 0, 0);
#endif
            SEC_HAL_TRACE("SEC_SERV_RUNTIME_INIT");
            st[0] = sec_msg_param_read32(&in_handle, &commit_id_len);
            st[1] = sec_msg_param_read32(&in_handle, &reset_info_len);
            sec_msg_param_write32(&out_handle,
                    (SEC_MSG_STATUS_OK == st[0] &&
                     SEC_MSG_STATUS_OK == st[1]) ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            sec_msg_param_write32(&out_handle,
                    sizeof(uint64_t), 0);
            sec_msg_param_write(&out_handle, &commit_id, sizeof(uint64_t), 0);
            sec_msg_param_write32(&out_handle, 3*sizeof(uint32_t), 0);
            sec_msg_param_write(&out_handle, k_reset_info, 3*sizeof(uint32_t), 0);

         } break;
        case SEC_SERV_SELFTEST:
        {
#if 0
            sec_msg_handle_t hnd;
            callback cb = (callback)g_rpc_handler;
            static const char *fmt = "%s > 0x%08X";
            static const char *func = "function";
            if (sec_msg_ptr && cb) {
                sec_msg_open(&hnd, (sec_msg_t *)sec_msg_ptr);
                sec_msg_param_write(&hnd, fmt, strlen(fmt)+1, 0);
                sec_msg_param_write(&hnd, func, strlen(func)+1, 0);
                sec_msg_param_write32(&hnd, 0x4411, 0);
                (*cb)(SEC_SERV_RPC_TRACE, (uint32_t) sec_msg_ptr, 0, 0, 0);
            }
#endif
            SEC_HAL_TRACE("SEC_SERV_SELFTEST");
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
            /* write selftest result */
            sec_msg_param_write32(&out_handle, g_status, 0);
        } break;
        case SEC_SERV_RPC_ADDRESS:
        {
            SEC_HAL_TRACE("SEC_SERV_RPC_ADDRESS");
            /* read input (phys)address */
            sec_msg_param_read32(&in_handle, &g_rpc_handler);
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
            /* write rpc result */
            sec_msg_param_write32(&out_handle, g_status, 0);
        } break;
        case SEC_SERV_COMA_ENTRY:
        {
            SEC_HAL_TRACE("SEC_SERV_COMA_ENTRY");
            /* read input params */
            sec_msg_param_read32(&in_handle, &g_coma_params[0]);
            sec_msg_param_read32(&in_handle, &g_coma_params[1]);
            sec_msg_param_read32(&in_handle, &g_coma_params[2]);
            sec_msg_param_read32(&in_handle, &g_coma_params[3]);
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
        } break;
        case SEC_SERV_POWER_OFF:
        {
            SEC_HAL_TRACE("SEC_SERV_POWER_OFF");
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
        } break;
        case SEC_SERV_MEMCPY_REQUEST:
        {
            SEC_HAL_TRACE("SEC_SERV_MEMCPY_REQUEST");
            /* read input params */
            sec_msg_param_read32(&in_handle, &g_memcpy_params[0]);
            sec_msg_param_read32(&in_handle, &g_memcpy_params[1]);
            sec_msg_param_read32(&in_handle, &g_memcpy_params[2]);
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
        }break;
        case SEC_SERV_COMA_CPU_OFF:
        {
            SEC_HAL_TRACE("SEC_SERV_COMA_CPU_OFF");
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
        }break;
        case SEC_SERV_PUBLIC_CC42_KEY_INIT:
        {
            SEC_HAL_TRACE("SEC_SERV_PUBLIC_CC42_KEY_INIT");
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
        }break;
        case SEC_SERV_A3SP_STATE_INFO:
        {
            SEC_HAL_TRACE("SEC_SERV_A3SP_STATE_INFO");
            sec_msg_param_read32(&in_handle, &g_a3sp_state_info[0]);
            g_a3sp_state_info[1] = g_a3sp_state_info[0] + 1;
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
            /* write 'allowed' */
            sec_msg_param_write32(&out_handle, g_a3sp_state_info[1], 0);
        }break;
        case SEC_SERV_RESERVE_MEDIA_AREA:
        {
            uint32_t id = 0xFF, start = 0, end = 0, st[3] = {0};
            SEC_HAL_TRACE("SEC_SERV_RESERVE_MEDIA_AREA");
            st[0] = sec_msg_param_read32(&in_handle, &id);
            st[1] = sec_msg_param_read32(&in_handle, &start);
            st[2] = sec_msg_param_read32(&in_handle, &end);
            sec_msg_param_write32(&out_handle,
                    (SEC_MSG_STATUS_OK == st[0] &&
                     SEC_MSG_STATUS_OK == st[1] &&
                     SEC_MSG_STATUS_OK == st[2] &&
                     start && end) ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;
        case SEC_SERV_FREE_MEDIA_AREA:
        {
            uint32_t id = 0xFF, st = 0;
            SEC_HAL_TRACE("SEC_SERV_FREE_MEDIA_AREA");
            st = sec_msg_param_read32(&in_handle, &id);
            sec_msg_param_write32(&out_handle,
                    SEC_MSG_STATUS_OK == st ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;
        case SEC_SERV_PUBLIC_ID_REQUEST:
        {
            uint32_t sz = 0, st = 0;
            SEC_HAL_TRACE("SEC_SERV_PUBLIC_ID_REQUEST");
            st = sec_msg_param_read32(&in_handle, &sz);
            sec_msg_param_write32(&out_handle,
                    SEC_HAL_MAX_PUBLIC_ID_LENGTH == sz ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            sec_msg_param_write(&out_handle, k_pub_id, SEC_HAL_MAX_PUBLIC_ID_LENGTH, 0);
        }break;
#ifdef CONFIG_ARM_SEC_HAL_DRM_WVN
        case SEC_SERV_DRM_OEMCRYPTO_SESSION_INIT:
        {
            uint32_t pid;
            SEC_HAL_TRACE("SEC_SERV_DRM_OEMCRYPTO_SESSION_INIT");
            sec_msg_param_read32(&in_handle, &pid);
            /* write status */
            sec_msg_param_write32(&out_handle, g_status, 0);
            /* write 'session_id' */
            sec_msg_param_write32(&out_handle, k_drm_default_session_id, 0);
        }break;
        case SEC_SERV_DRM_OEMCRYPTO_SESSION_TERMINATE:
        {
            uint32_t count, session_id;
            SEC_HAL_TRACE("SEC_SERV_DRM_OEMCRYPTO_SESSION_TERMINATE");
            sec_msg_param_read32(&in_handle, &count);
            sec_msg_param_read32(&in_handle, &session_id);
            /* write status */
            sec_msg_param_write32(&out_handle,
                    ((count == 1) && (session_id == k_drm_default_session_id)) ?
                    g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;
        case SEC_SERV_DRM_OEMCRYPTO_SET_ENTITLEMENT_KEY:
        {
            uint32_t session_id, key_len, st;
            uint8_t key[16] = {0};
            SEC_HAL_TRACE("SEC_SERV_DRM_OEMCRYPTO_SET_ENTITLEMENT_KEY");
            sec_msg_param_read32(&in_handle, &session_id);
            sec_msg_param_read32(&in_handle, &key_len);
            st = sec_msg_param_read(&in_handle, &key, key_len);
            SEC_HAL_TRACE("key[0,1,2,3]: 0x%x,0x%x,0x%x,0x%x", key[0],key[1],key[2],key[3]);
            /* write status */
            sec_msg_param_write32(&out_handle,
                    ((SEC_MSG_STATUS_OK == st) &&
                     (session_id == k_drm_default_session_id) && (key_len == 16) && 
                     (key[0] && key[1] && key[2] && key[3])) ?
                    g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            sec_msg_param_write32(&out_handle, SEC_HAL_RES_OK, 0);
        }break;
        case SEC_SERV_DRM_OEMCRYPTO_DERIVE_CONTROL_WORD:
        {
            uint32_t session_id, ecm_len, st;
            uint8_t ecm[32] = {0};
            SEC_HAL_TRACE("SEC_SERV_DRM_OEMCRYPTO_DERIVE_CONTROL_WORD");
            sec_msg_param_read32(&in_handle, &session_id);
            sec_msg_param_read32(&in_handle, &ecm_len);
            st = sec_msg_param_read(&in_handle, &ecm, ecm_len);
            SEC_HAL_TRACE("ecm[0,1,2,3]: 0x%x,0x%x,0x%x,0x%x", ecm[0],ecm[1],ecm[2],ecm[3]);
           /* write status */
            sec_msg_param_write32(&out_handle,
                    ((SEC_MSG_STATUS_OK == st) &&
                     (session_id == k_drm_default_session_id) && (ecm_len == 32) && 
                     (ecm[0] && ecm[1] && ecm[2] && ecm[3])) ?
                    g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            sec_msg_param_write32(&out_handle, SEC_HAL_RES_OK, 0);
            sec_msg_param_write32(&out_handle, 0x07, 0); /* flags */
        }break;
        case SEC_SERV_DRM_OEMCRYPTO_DECRYPT_VIDEO:
        {
            uint32_t session_id, st;
            uint32_t iv_len = 0, input_len = 0, input_addr = 0, output_hnd = 0, output_offset = 0;
            uint8_t iv_in[16] = {0};
            uint8_t iv_out[16] = {0};
            int index = 16;
            SEC_HAL_TRACE("SEC_SERV_DRM_OEMCRYPTO_DECRYPT_VIDEO");
            sec_msg_param_read32(&in_handle, &session_id);
            sec_msg_param_read32(&in_handle, &iv_len);
            st = sec_msg_param_read(&in_handle, &iv_in, iv_len);
            sec_msg_param_read32(&in_handle, &input_len);
            sec_msg_param_read32(&in_handle, &input_addr);
            sec_msg_param_read32(&in_handle, &output_hnd);
            sec_msg_param_read32(&in_handle, &output_offset);
            SEC_HAL_TRACE("iv_in[0,1,2,3]: 0x%x,0x%x,0x%x,0x%x", iv_in[0],iv_in[1],iv_in[2],iv_in[3]);
            SEC_HAL_TRACE("[input_addr, output_hnd, output_offset]: 0x%x, 0x%x, 0x%x", input_addr, output_hnd, output_offset);
           /* write status */
            sec_msg_param_write32(&out_handle,
                    ((SEC_MSG_STATUS_OK == st) &&
                     (session_id == k_drm_default_session_id) &&
                     (iv_in[0] && iv_in[1] && iv_in[2] && iv_in[3])) &&
                     (input_len && input_addr && output_hnd) ?
                    g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            sec_msg_param_write32(&out_handle, SEC_HAL_RES_OK, 0);
            while(index > 0){ iv_out[index-1] = iv_in[16-index]; index--; }
            sec_msg_param_write32(&out_handle, 8, 0); /* outsize */
            sec_msg_param_write(&out_handle, iv_out, 16, 0); /* iv_out */
        }break;
        case SEC_SERV_DRM_OEMCRYPTO_DECRYPT_AUDIO:
        {
            uint32_t session_id, st;
            uint32_t iv_len = 0, input_len = 0, input_addr = 0, output_addr = 0;
            uint8_t iv_in[16] = {0};
            uint8_t iv_out[16] = {0};
            int index = 16;
            SEC_HAL_TRACE("SEC_SERV_DRM_OEMCRYPTO_DECRYPT_AUDIO");
            sec_msg_param_read32(&in_handle, &session_id);
            sec_msg_param_read32(&in_handle, &iv_len);
            st = sec_msg_param_read(&in_handle, &iv_in, iv_len);
            sec_msg_param_read32(&in_handle, &input_len);
            sec_msg_param_read32(&in_handle, &input_addr);
            sec_msg_param_read32(&in_handle, &output_addr);
            SEC_HAL_TRACE("iv_in[0,1,2,3]: 0x%x,0x%x,0x%x,0x%x", iv_in[0],iv_in[1],iv_in[2],iv_in[3]);
            SEC_HAL_TRACE("[input_addr, output_addr]: 0x%x, 0x%x", input_addr, output_addr);
            /* write status */
            sec_msg_param_write32(&out_handle,
                    ((SEC_MSG_STATUS_OK == st) &&
                     (session_id == k_drm_default_session_id) &&
                     (iv_in[0] && iv_in[1] && iv_in[2] && iv_in[3])) &&
                     (input_len && input_addr && output_addr) ?
                    g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            sec_msg_param_write32(&out_handle, SEC_HAL_RES_OK, 0);
            while(index > 0){ iv_out[index-1] = iv_in[16-index]; index--; }
            sec_msg_param_write32(&out_handle, 8, 0); /* outsize */
            sec_msg_param_write(&out_handle, iv_out, 16, 0); /* iv_out */
        }break;
        case SEC_SERV_DRM_OEMCRYPTO_GET_DEVICE_ID:
        {
            uint32_t id_len = 0, st;
            SEC_HAL_TRACE("SEC_SERV_DRM_OEMCRYPTO_GET_DEVICE_ID");
            st = sec_msg_param_read32(&in_handle, &id_len);
            sec_msg_param_write32(&out_handle,
                    (SEC_MSG_STATUS_OK == st) && id_len ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            sec_msg_param_write32(&out_handle, SEC_HAL_RES_OK, 0);
            sec_msg_param_write32(&out_handle, 32, 0); /* actual length */
            sec_msg_param_write(&out_handle, k_drm_device_id, 32, 0);
        }break;
        case SEC_SERV_DRM_OEMCRYPTO_GET_KEYDATA:
        {
            uint32_t keyd_len = 0, st;
            SEC_HAL_TRACE("SEC_SERV_DRM_OEMCRYPTO_GET_KEYDATA");
            st = sec_msg_param_read32(&in_handle, &keyd_len);
            sec_msg_param_write32(&out_handle,
                    (SEC_MSG_STATUS_OK == st) && keyd_len ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            sec_msg_param_write32(&out_handle, SEC_HAL_RES_OK, 0);
            sec_msg_param_write32(&out_handle, 32, 0); /* actual length */
            sec_msg_param_write(&out_handle, k_drm_device_id, 32, 0);
        }break;
        case SEC_SERV_DRM_OEMCRYPTO_WRAP_KEYBOX:
        {
            uint32_t kbox_size = 0, trs_key_size = 0, wrap_buffer_size = 0, st[3] = {SEC_MSG_STATUS_OK};
            void *kbox = 0, *trs_key = 0;

            SEC_HAL_TRACE("SEC_SERV_DRM_OEMCRYPTO_WRAP_KEYBOX");
            st[0] = sec_msg_param_read32(&in_handle, &kbox_size);
            if (kbox_size && (kbox = kmalloc(kbox_size, GFP_KERNEL))) {
                st[1] = sec_msg_param_read(&in_handle, kbox, kbox_size);
            }
            else{
                st[1] = SEC_MSG_STATUS_PARAM_EMPTY;
            }
            st[2] = sec_msg_param_read32(&in_handle, &wrap_buffer_size);

            /* Optional params */
            sec_msg_param_read32(&in_handle, &trs_key_size);
            if (trs_key_size && (trs_key = kmalloc(trs_key_size, GFP_KERNEL))) {
                sec_msg_param_read(&in_handle, trs_key, trs_key_size);
            }

            sec_msg_param_write32(&out_handle,
                     (SEC_MSG_STATUS_OK == st[0]) &&
                     (SEC_MSG_STATUS_OK == st[1]) &&
                     (SEC_MSG_STATUS_OK == st[2]) ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            sec_msg_param_write32(&out_handle,
                     wrap_buffer_size ? OEMCrypto_SUCCESS : OEMCrypto_ERROR_SHORT_BUFFER, 0);
            sec_msg_param_write32(&out_handle, 32, 0);
            sec_msg_param_write(&out_handle, k_drm_device_id, 32, 0);

            SEC_HAL_TRACE("st0: 0x%X, st1: 0x%X, st2: 0x%X", st[0], st[1], st[2]);
            SEC_HAL_TRACE("kbox_size: %d, trs_key_size: %d, wrap_buffer_size: %d",
                     kbox_size, trs_key_size, wrap_buffer_size);
            kfree(trs_key);
            kfree(kbox);
        }break;
        case SEC_SERV_DRM_OEMCRYPTO_INSTALL_KEYBOX:
        {
            uint32_t kbox_size = 0, st;
            SEC_HAL_TRACE("SEC_SERV_DRM_OEMCRYPTO_INSTALL_KEYBOX");
            st = sec_msg_param_read32(&in_handle, &kbox_size);
            sec_msg_param_write32(&out_handle,
                    (SEC_MSG_STATUS_OK == st) && kbox_size ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            sec_msg_param_write32(&out_handle, SEC_HAL_RES_OK, 0);
        }break;
#endif /* CONFIG_ARM_SEC_HAL_DRM_WVN */
        case SEC_SERV_SIMLOCK_CONFIGURE:
        {
            uint32_t msg[3], paddr, sz, emask, maxatt = 0x00, st = SEC_MSG_STATUS_OK;

            SEC_HAL_TRACE("SEC_SERV_SIMLOCK_CONFIGURE");
            msg[0] = sec_msg_param_read32(&in_handle, &sz);
            msg[1] = sec_msg_param_read32(&in_handle, &paddr);
            msg[2] = sec_msg_param_read32(&in_handle, &emask);


            if (msg[0] != SEC_MSG_STATUS_OK
                || msg[1] != SEC_MSG_STATUS_OK
                || msg[2] != SEC_MSG_STATUS_OK
                || !sz || !paddr || !emask) {
                st = SEC_MSG_STATUS_FAIL;
            }
            else {
                int i;
                for (i = 0;
                     i < 5 && st == SEC_MSG_STATUS_OK;
                     i++) {
                     char code[20] = {0};
                     st = sec_msg_param_read(&in_handle, &code, 20);
                     SEC_HAL_TRACE("st:%u, code: %s, k_correct: %s", st, code, k_correct_siml_unlock_code[i]);
                     if ((emask & (0x1<<i)) && strcmp(code, k_correct_siml_unlock_code[i])) {
                        st = SEC_MSG_STATUS_FAIL;
                        break;
                     }
                }
            }
            sec_msg_param_read32(&in_handle, &maxatt);
            SEC_HAL_TRACE("sz: %u, paddr: 0x%X, emask: 0x%X, maxatt: %u", sz, paddr, emask, maxatt);
            sec_msg_param_write32(&out_handle,
                (SEC_MSG_STATUS_OK == st) ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            msg[0] = sec_msg_param_write32(&out_handle, 0x0D, 0);
            if (msg[0] != SEC_MSG_STATUS_OK) { ret_status = SEC_ROM_RET_FAIL; }
        }break;
        case SEC_SERV_SIMLOCK_CONFIGURATION_REGISTER:
        {
            uint32_t msg[2], sz, paddr;
            SEC_HAL_TRACE("SEC_SERV_SIMLOCK_CONFIGURATION_REGISTER");
            msg[0] = sec_msg_param_read32(&in_handle, &sz);
            msg[1] = sec_msg_param_read32(&in_handle, &paddr);
            sec_msg_param_write32(&out_handle,
                (msg[0] == SEC_MSG_STATUS_OK &&
                 msg[1] == SEC_MSG_STATUS_OK && paddr && sz) ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;
        case SEC_SERV_SIMLOCK_VERIFICATION_DATA_READ:
        {
            uint32_t msg[2] = {0}, i = 0, len = 0, st = 0;
            uint8_t empty[32] = {0};
            SEC_HAL_TRACE("SEC_SERV_SIMLOCK_VERIFICATION_DATA_READ");
            msg[0] = sec_msg_param_read32(&in_handle, &len);
            SEC_HAL_TRACE("len: %u", len);
            st = sec_msg_param_write32(&out_handle,
                (msg[0] == SEC_MSG_STATUS_OK && len <= 32) ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
            /* master_hash is always the first. */
            sec_msg_param_write32(&out_handle, len, 0);
            sec_msg_param_write(&out_handle, k_master_hash, len, 0);
            /* level1 hash is left empty so that we can also test not locked level */
            msg[0] = sec_msg_param_write32(&out_handle, 0, 0);
            msg[1] = sec_msg_param_write(&out_handle, empty, len, 0);
            for (i = 1; st == SEC_MSG_STATUS_OK && i < 5 && msg[0] == SEC_MSG_STATUS_OK && msg[1] == SEC_MSG_STATUS_OK; i++) {
                msg[0] = sec_msg_param_write32(&out_handle, len, 0);
                msg[1] = sec_msg_param_write(&out_handle, k_siml_verify_hash[i], len, 0);
            }
            if (st != SEC_MSG_STATUS_OK || msg[0] != SEC_MSG_STATUS_OK || msg[1] != SEC_MSG_STATUS_OK) {
                ret_status = SEC_ROM_RET_FAIL;
            }
        }break;
        case SEC_SERV_IMEI_WRITE:
        {
            uint32_t msg[2];
            sec_hal_imei_t imei;
            uint8_t code[20] = {0};

            SEC_HAL_TRACE("SEC_SERV_IMEI_WRITE");
            msg[0] = sec_msg_param_read(&in_handle, &imei.imei, SEC_HAL_MAX_IMEI_SIZE);
            msg[1] = sec_msg_param_read(&in_handle, &code, 20);
            sec_msg_param_write32(&out_handle,
                (msg[0] == SEC_MSG_STATUS_OK &&
                 msg[1] == SEC_MSG_STATUS_OK) ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;
        case SEC_SERV_MAC_WRITE:
        {
            uint32_t msg[3], idx = 0x00;
            sec_hal_mac_address_t mac;
            uint8_t code[20] = {0};

            SEC_HAL_TRACE("SEC_SERV_MAC_WRITE");
            msg[0] = sec_msg_param_read(&in_handle, &mac.mac_address, SEC_HAL_MAC_SIZE);
            msg[1] = sec_msg_param_read32(&in_handle, &idx);
            msg[2] = sec_msg_param_read(&in_handle, &code, 20);
            sec_msg_param_write32(&out_handle,
                (msg[0] == SEC_MSG_STATUS_OK &&
                 msg[1] == SEC_MSG_STATUS_OK &&
                 msg[2] == SEC_MSG_STATUS_OK) ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;
        case SEC_SERV_TEEC_InitializeContext:
        {
            uint32_t st = 0;
            uint32_t phys_name_ptr;
            uint32_t name_size;
            uint32_t phys_context_ptr;
            uint32_t name_ptr;
            uint32_t context_ptr;
            TEEC_Context * context;

            SEC_HAL_TRACE("SEC_SERV_TEEC_InitializeContext");

            st = sec_msg_param_read32(&in_handle, &name_size);
            SEC_HAL_TRACE("name_size 0x%x", name_size);

            st = sec_msg_param_read32(&in_handle, &phys_name_ptr);
            name_ptr = (uint32_t) phys_to_virt(phys_name_ptr);
            SEC_HAL_TRACE("name %s", (char *)name_ptr);

            st = sec_msg_param_read32(&in_handle, &phys_context_ptr);
            SEC_HAL_TRACE("phys_context_ptr 0x%x", phys_context_ptr);
            context_ptr = (uint32_t) phys_to_virt(phys_context_ptr);
            SEC_HAL_TRACE("context_ptr 0x%x", context_ptr);
            context = (TEEC_Context *) context_ptr;

            context->imp.tag = (void *)0x2020;
            sec_msg_param_write32(&out_handle,
                    SEC_MSG_STATUS_OK == st ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;

        case SEC_SERV_TEEC_FinalizeContext:
        {
            uint32_t st = 0;
            uint32_t phys_context_ptr;
            uint32_t context_ptr;
            uint32_t * context;

            SEC_HAL_TRACE("SEC_SERV_TEEC_FinalizeContext");

            st = sec_msg_param_read32(&in_handle, &phys_context_ptr);
            SEC_HAL_TRACE("phys_context_ptr 0x%x", phys_context_ptr);
            context_ptr = (uint32_t) phys_to_virt(phys_context_ptr);
            SEC_HAL_TRACE("context_ptr 0x%x", context_ptr);
            context=(uint32_t*) context_ptr;

            SEC_HAL_TRACE("*context: 0x%x", *context);

            sec_msg_param_write32(&out_handle,
                    SEC_MSG_STATUS_OK == st ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;

        case SEC_SERV_TEEC_OpenSession:
        {
            uint32_t st = 0;

        /* 1 ) context */
        /* 2 ) session */
        /* 3 ) destination */
        /* 4 ) connectionMethod */
        /* 5 ) connectionData (NULL) */
        /* 6 ) operation (NULL) */
        /* 7 ) returnOrigin (NULL) */
            uint32_t phys_context_ptr;
            uint32_t context_ptr;
            uint32_t * context;

            uint32_t phys_session_ptr;
            uint32_t session_ptr;
            TEEC_Session * session;

            uint32_t phys_destination_ptr;
            uint32_t destination_ptr;
            uint32_t * destination;

            uint32_t connection_method;

            SEC_HAL_TRACE("SEC_SERV_TEEC_OpenSession");

            st = sec_msg_param_read32(&in_handle, &phys_context_ptr);
            SEC_HAL_TRACE("phys_context_ptr 0x%x", phys_context_ptr);
            context_ptr = (uint32_t) phys_to_virt(phys_context_ptr);
            SEC_HAL_TRACE("context_ptr 0x%x", context_ptr);
            context=(uint32_t*) context_ptr;

            SEC_HAL_TRACE("*context: 0x%x", *context);


            st = sec_msg_param_read32(&in_handle, &phys_session_ptr);
            SEC_HAL_TRACE("phys_session_ptr 0x%x", phys_session_ptr);

            session_ptr = (uint32_t) phys_to_virt(phys_session_ptr);
            SEC_HAL_TRACE("session_ptr 0x%x", session_ptr);
            session = (TEEC_Session *) session_ptr;

            session->imp.tag = (void *)0x3030;

            st = sec_msg_param_read32(&in_handle, &phys_destination_ptr);
            SEC_HAL_TRACE("phys_destination_ptr 0x%x", phys_destination_ptr);
            destination_ptr = (uint32_t) phys_to_virt(phys_destination_ptr);
            SEC_HAL_TRACE("destination_ptr 0x%x", destination_ptr);
            destination=(uint32_t*) destination_ptr;



            st = sec_msg_param_read32(&in_handle, &connection_method);
            SEC_HAL_TRACE("connection_method 0x%x", connection_method);


            sec_msg_param_write32(&out_handle,
                    SEC_MSG_STATUS_OK == st ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;
        case SEC_SERV_TEEC_CloseSession:
        {
            uint32_t st = 0;
            uint32_t phys_session_ptr;
            uint32_t session_ptr;
            uint32_t * session;

            SEC_HAL_TRACE("SEC_SERV_TEEC_CloseSession");

            st = sec_msg_param_read32(&in_handle, &phys_session_ptr);
            SEC_HAL_TRACE("phys_session_ptr 0x%x", phys_session_ptr);
            session_ptr = (uint32_t) phys_to_virt(phys_session_ptr);
            SEC_HAL_TRACE("session_ptr 0x%x", session_ptr);
            session=(uint32_t*) session_ptr;

            SEC_HAL_TRACE("*session: 0x%x", *session);

            sec_msg_param_write32(&out_handle,
                    SEC_MSG_STATUS_OK == st ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;
        case SEC_SERV_TEEC_InvokeCommand:
        {
            uint32_t st = 0;

		/* write content to the input msg */
        /* 1 ) session */
        /* 2 ) commandID */
        /* 3 ) operation */
        /* 4 ) returnOrigin */

            uint32_t phys_session_ptr;
            uint32_t session_ptr;
            uint32_t * session;

            uint32_t phys_operation_ptr;
            uint32_t operation_ptr;
            TEEC_Operation * operation;

            uint32_t phys_returnOrigin_ptr;
            uint32_t returnOrigin_ptr;
            uint32_t * returnOrigin;

            uint32_t commandID;

            uint32_t i;

            SEC_HAL_TRACE("SEC_SERV_TEEC_InvokeCommand");


            st = sec_msg_param_read32(&in_handle, &phys_session_ptr);
            SEC_HAL_TRACE("phys_session_ptr 0x%x", phys_session_ptr);
            session_ptr = (uint32_t) phys_to_virt(phys_session_ptr);
            SEC_HAL_TRACE("session_ptr 0x%x", session_ptr);
            session=(uint32_t*) session_ptr;
            st = sec_msg_param_read32(&in_handle, &commandID);
            SEC_HAL_TRACE("commandID: 0x%x", commandID);


            st = sec_msg_param_read32(&in_handle, &phys_operation_ptr);

            SEC_HAL_TRACE("phys_operation_ptr 0x%x", phys_operation_ptr);
            if(phys_operation_ptr != 0)
                {
                operation_ptr = (uint32_t) phys_to_virt(phys_operation_ptr);
                SEC_HAL_TRACE("operation_ptr 0x%x", operation_ptr);
                operation = (TEEC_Operation *) operation_ptr;

                for(i=0;i<4;i++)
                    {
                    uint32_t param_type;
                    SEC_HAL_TRACE("i: 0x%x", i);

/*                    param_type = sec_hal_tee_get_param_type(i,operation->paramTypes);*/
                    param_type = TEEC_PARAM_TYPE_GET(operation->paramTypes, i);
                    SEC_HAL_TRACE("param_type: 0x%x", param_type);

                    }

                if(commandID==7)
                    {
                    /* This command is memcpy from memref1 to memref2*/
                    TEEC_SharedMemory *inputSM;
                    TEEC_SharedMemory *outputSM;

                    if(TEEC_PARAM_TYPE_GET(operation->paramTypes, 0) ==  TEEC_MEMREF_WHOLE &&
                       TEEC_PARAM_TYPE_GET(operation->paramTypes, 1) ==  TEEC_MEMREF_WHOLE )
                        {
                        inputSM = (TEEC_SharedMemory *) phys_to_virt((phys_addr_t) operation->params[0].memref.parent);
                        outputSM = (TEEC_SharedMemory *) phys_to_virt((phys_addr_t) operation->params[1].memref.parent);
                        SEC_HAL_TRACE("outputSM 0x%x",outputSM);
                        SEC_HAL_TRACE("inputSM 0x%x",inputSM);
                        SEC_HAL_TRACE("phys_to_virt(outputSM->buffer) 0x%x",phys_to_virt((phys_addr_t)outputSM->buffer));
                        SEC_HAL_TRACE("phys_to_virt(inputSM->buffer) 0x%x",phys_to_virt((phys_addr_t)inputSM->buffer));

                        memcpy(phys_to_virt((phys_addr_t)outputSM->buffer),phys_to_virt((phys_addr_t)inputSM->buffer),inputSM->size);
                        }
                    else if(TEEC_PARAM_TYPE_GET(operation->paramTypes, 0) == TEEC_MEMREF_PARTIAL_INPUT &&
                            TEEC_PARAM_TYPE_GET(operation->paramTypes, 1) == TEEC_MEMREF_PARTIAL_OUTPUT )
                        {
                        inputSM = phys_to_virt((phys_addr_t)operation->params[0].memref.parent);
                        outputSM = phys_to_virt((phys_addr_t)operation->params[1].memref.parent);
                        SEC_HAL_TRACE("outputSM 0x%x",outputSM);
                        SEC_HAL_TRACE("inputSM 0x%x",inputSM);
                        SEC_HAL_TRACE("phys_to_virt(outputSM->buffer) 0x%x",phys_to_virt((phys_addr_t)outputSM->buffer));
                        SEC_HAL_TRACE("phys_to_virt(inputSM->buffer) 0x%x",phys_to_virt((phys_addr_t)inputSM->buffer));

                        memcpy(phys_to_virt((phys_addr_t)outputSM->buffer) + operation->params[1].memref.offset,
                        phys_to_virt((phys_addr_t)inputSM->buffer) + operation->params[0].memref.offset,
                        inputSM->size);
                        }
                    else if(TEEC_PARAM_TYPE_GET(operation->paramTypes, 0) == TEEC_MEMREF_PARTIAL_INOUT )
                        {
                        TEEC_SharedMemory *inoutSM;
                        inoutSM = phys_to_virt((phys_addr_t) operation->params[0].memref.parent);
                        SEC_HAL_TRACE("inoutSM 0x%x",inoutSM);
                        memcpy(phys_to_virt((phys_addr_t)inoutSM->buffer),
                        phys_to_virt((phys_addr_t)inoutSM->buffer) + (inoutSM->size/2),
                        (inoutSM->size/2));
                        }
                    }
                if(commandID==8)
                    {
                    /* This is add of numbers command */
                    SEC_HAL_TRACE("operation->params[0].value.a: %d",operation->params[0].value.a);
                    SEC_HAL_TRACE("operation->params[1].value.a: %d",operation->params[1].value.a);
                    operation->params[2].value.a = operation->params[0].value.a + operation->params[1].value.a;
                    SEC_HAL_TRACE("operation->params[2].value.a: %d",operation->params[2].value.a);
                    }
                if(commandID==9)
                    {
                    /* Get properties test */

                    operation->params[0].value.a = 0x00000000;
                    operation->params[0].value.b = 0x11111111;
                    operation->params[1].value.a = 0x22222222;
                    operation->params[1].value.b = 0x33333333;
                    operation->params[2].value.a = 0x44444444;
                    operation->params[2].value.b = 0x55555555;
                    operation->params[3].value.a = 0x66666666;
                    operation->params[3].value.b = 0x77777777;
                    }

                }

            st = sec_msg_param_read32(&in_handle, &phys_returnOrigin_ptr);
            SEC_HAL_TRACE("phys_returnOrigin_ptr 0x%x", phys_returnOrigin_ptr);
            if(phys_returnOrigin_ptr != 0)
                {
                returnOrigin_ptr = (uint32_t) phys_to_virt(phys_returnOrigin_ptr);
                SEC_HAL_TRACE("returnOrigin_ptr 0x%x", returnOrigin_ptr);
                returnOrigin=(uint32_t*) returnOrigin_ptr;
                }


            sec_msg_param_write32(&out_handle,
                    SEC_MSG_STATUS_OK == st ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;
        case SEC_SERV_TEEC_RegisterSharedMemory:
        {
            uint32_t st = 0;
            uint32_t phys_context_ptr;
            uint32_t context_ptr;
            uint32_t * context;

            uint32_t phys_shmem_ptr;
            uint32_t shmem_ptr;
            TEEC_SharedMemory *shmem;


            SEC_HAL_TRACE("SEC_SERV_TEEC_RegisterSharedMemory");

            st = sec_msg_param_read32(&in_handle, &phys_context_ptr);
            SEC_HAL_TRACE("phys_context_ptr 0x%x", phys_context_ptr);
            context_ptr = (uint32_t) phys_to_virt(phys_context_ptr);
            SEC_HAL_TRACE("context_ptr 0x%x", context_ptr);
            context=(uint32_t*) context_ptr;

            SEC_HAL_TRACE("*context: 0x%x", *context);

            st = sec_msg_param_read32(&in_handle, &phys_shmem_ptr);
            SEC_HAL_TRACE("phys_shmem_ptr 0x%x", phys_shmem_ptr);
            shmem_ptr = (uint32_t) phys_to_virt(phys_shmem_ptr);
            SEC_HAL_TRACE("shmem_ptr 0x%x", shmem_ptr);
            shmem = (TEEC_SharedMemory *) shmem_ptr;

            shmem->imp.tag = (void *)0x12345678;

            SEC_HAL_TRACE("shmem->size: 0x%x", shmem->size);
            SEC_HAL_TRACE("shmem->flags: 0x%x", shmem->flags);
            SEC_HAL_TRACE("shmem->buffer: 0x%x", shmem->buffer);
            SEC_HAL_TRACE("shmem->imp: 0x%x", shmem->imp);


            sec_msg_param_write32(&out_handle,
                    SEC_MSG_STATUS_OK == st ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;

        case SEC_SERV_TEEC_ReleaseSharedMemory:
        {

            uint32_t st = 0;
            uint32_t phys_shmem_ptr;
            uint32_t shmem_ptr;
            TEEC_SharedMemory *shmem;


            SEC_HAL_TRACE("SEC_SERV_TEEC_ReleaseSharedMemory");

            st = sec_msg_param_read32(&in_handle, &phys_shmem_ptr);
            SEC_HAL_TRACE("phys_shmem_ptr 0x%x", phys_shmem_ptr);
            shmem_ptr = (uint32_t) phys_to_virt(phys_shmem_ptr);
            SEC_HAL_TRACE("shmem_ptr 0x%x", shmem_ptr);
            shmem = (TEEC_SharedMemory *) shmem_ptr;

            SEC_HAL_TRACE("shmem->size: 0x%x", shmem->size);
            SEC_HAL_TRACE("shmem->flags: 0x%x", shmem->flags);
            SEC_HAL_TRACE("shmem->buffer: 0x%x", shmem->buffer);
            SEC_HAL_TRACE("shmem->imp: 0x%x", shmem->imp);

            sec_msg_param_write32(&out_handle,
                    SEC_MSG_STATUS_OK == st ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;

        case SEC_SERV_RESET_INFO_ADDR_REGISTER:
        {

            uint32_t st = 0;
            uint32_t size;
            uint32_t addr;
            sec_reset_info * reset_info;


            SEC_HAL_TRACE("SEC_SERV_RESET_INFO_ADDR_REGISTER");

            st = sec_msg_param_read32(&in_handle, &size);
            SEC_HAL_TRACE("size 0x%x", size);

            st = sec_msg_param_read32(&in_handle, &addr);
            SEC_HAL_TRACE("addr 0x%x", addr);

            reset_info = phys_to_virt(addr);
            reset_info->hw_reset_type = 0x12341234;
            reset_info->reason = 0x43214321;
            reset_info->link_register = 0x98769876;


            sec_msg_param_write32(&out_handle,
                    SEC_MSG_STATUS_OK == st ? g_status : SEC_SERV_STATUS_INVALID_INPUT, 0);
        }break;


        default:
        {
            SEC_HAL_TRACE("SSA_SEC_PUB_DISPATCHER:default");
            (void) sec_msg_ptr;
            ret_status = SEC_ROM_RET_NON_SUPPORTED_SERV;
        } break;
    }

    sec_msg_close(&in_handle);
    sec_msg_close(&out_handle);
    va_end(list);

    SEC_HAL_TRACE_EXIT();
    return ret_status;
}

/* ******************************** END ************************************ */

