/*
 * drivers/sec_hal/rt/inc/sec_serv_api.h
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

#if defined _SEC_SERV_API_H_
#error "Multiply included"
#endif
#define _SEC_SERV_API_H_

#include <linux/types.h>

/*!
 * This is the type of the status expected from a secure service.
 */
typedef uint32_t sec_serv_status_t;

/*!
 * RPC messages from Secure Environment
 */
#define SEC_SERV_RPC_ALLOC                      0x80000001
#define SEC_SERV_RPC_FREE                       0x80000002
#define SEC_SERV_RPC_RPMB                       0x80000003
#define SEC_SERV_RPC_STORAGE_INFO_GET           0x80000004
#define SEC_SERV_RPC_FS_ROOT                    0x80000005
#define SEC_SERV_RPC_FS_LOOKUP                  0x80000006
#define SEC_SERV_RPC_FS_READ                    0x80000007
#define SEC_SERV_RPC_FS_WRITE                   0x80000008
#define SEC_SERV_RPC_FS_CREATE                  0x80000009
#define SEC_SERV_RPC_FS_REMOVE                  0x8000000A
#define SEC_SERV_RPC_FS_MOVE                    0x8000000B
#define SEC_SERV_RPC_FS_SIZE                    0x8000000C
#define SEC_SERV_RPC_PROT_DATA_FREE             0x8000000D
#define SEC_SERV_RPC_PROT_DATA_ALLOC            0x8000000E
#define SEC_SERV_RPC_TRACE                      0x8000000F
/*!
 * Defined secure services. See [1].
 */
#define SEC_SERV_FORCE_RESET                    0x10000001
#define SEC_SERV_CERTIFICATE_REGISTER           0x10000002
#define SEC_SERV_ISSW_REGISTER                  0x10000003
#define SEC_SERV_IMEI_REQUEST                   0x10000004
#define SEC_SERV_MAC_ADDRESS_REQUEST            0x10000005
#define SEC_SERV_DEVICE_AUTH_DATA_REQUEST       0x10000006
#define SEC_SERV_DEVICE_AUTH_DATA_SIZE_REQUEST  0x10000007
#define SEC_SERV_KEY_INFO_REQUEST               0x10000008
#define SEC_SERV_RANDOM_DATA_REQUEST            0x10000009
#define SEC_SERV_RAT_BAND_INFO_REQUEST          0x1000000A
#define SEC_SERV_PP_FLAGS_REQUEST               0x1000000B
#define SEC_SERV_PP_FLAGS_SIZE_REQUEST          0x1000000C
#define SEC_SERV_INTEGRITY_CHECK                0x1000000D
#define SEC_SERV_PROT_DATA_REGISTER             0x1000000E
#define SEC_SERV_SIMLOCK_CHECK_LOCKS            0x1000000F
#define SEC_SERV_SIMLOCK_CHECK_ONE_LOCK         0x10000010
#define SEC_SERV_SIMLOCK_GET_STATE              0x10000011
#define SEC_SERV_RUNTIME_INIT                   0x10000012
#define SEC_SERV_SELFTEST                       0x10000013
#define SEC_SERV_RPC_ADDRESS                    0x10000014
#define SEC_SERV_L2_CACHE_CONTROL               0x10000015
#define SEC_SERV_PRODUCT_CONFIGURATION_REQUEST  0x10000016
#define SEC_SERV_DEBUG_CONTROL_DATA_REQUEST     0x10000017
#define SEC_SERV_MULTICORE_ENABLE               0x10000018
#define SEC_SERV_COMA_ENTRY                     0x10000019
#define SEC_SERV_POWER_OFF                      0x1000001A
#define SEC_SERV_MEMCPY_REQUEST                 0x1000001B
#define SEC_SERV_COMA_CPU_OFF                   0x1000001C
#define SEC_SERV_PUBLIC_CC42_KEY_INIT           0x1000001D
#define SEC_SERV_A3SP_STATE_INFO                0x1000001E // To be removed when references removed
#define SEC_SERV_A3SP_STATE_REQUEST             0x1000001E // This will replace the A3SP state info
#define SEC_SERV_DRM_OEMCRYPTO_IS_KEYBOX_VALID  0x1000001F
#define SEC_SERV_DRM_OEMCRYPTO_GET_DEVICE_ID    0x10000020
#define SEC_SERV_DRM_OEMCRYPTO_GET_KEYDATA      0x10000021
#define SEC_SERV_DRM_OEMCRYPTO_GET_RANDOM       0x10000022
#define SEC_SERV_DRM_OEMCRYPTO_SESSION_INIT     0x10000023
#define SEC_SERV_DRM_OEMCRYPTO_SESSION_TERMINATE        0x10000024
#define SEC_SERV_DRM_OEMCRYPTO_SET_ENTITLEMENT_KEY      0x10000025
#define SEC_SERV_DRM_OEMCRYPTO_DERIVE_CONTROL_WORD      0x10000026
#define SEC_SERV_DRM_OEMCRYPTO_DECRYPT_VIDEO            0x10000027
#define SEC_SERV_DRM_OEMCRYPTO_DECRYPT_AUDIO            0x10000028
#define SEC_SERV_DRM_OEMCRYPTO_WRAP_KEYBOX              0x10000029
#define SEC_SERV_DRM_OEMCRYPTO_INSTALL_KEYBOX           0x1000002A
#define SEC_SERV_RESERVE_MEDIA_AREA             0x1000002B
#define SEC_SERV_FREE_MEDIA_AREA                0x1000002C
#define SEC_SERV_RESET_INFO_ADDR_REGISTER       0x1000002D

#define SEC_SERV_PUBLIC_ID_REQUEST              0x1000002E
#define SEC_SERV_DEBUG_CONTROL_DATA_SET         0x1000002F
#define SEC_SERV_SIMLOCK_CONFIGURE              0x10000030
#define SEC_SERV_SIMLOCK_CONFIGURATION_REGISTER 0x10000031
#define SEC_SERV_IMEI_WRITE                     0x10000032
#define SEC_SERV_MAC_WRITE                      0x10000033
#define SEC_SERV_SIMLOCK_VERIFICATION_DATA_READ 0x10000034

#define SEC_SERV_RAMDUMP_ACTIVATE                               0x10000035
#define SEC_SERV_RAMDUMP_DEACTIVATE                             0x10000036
#define SEC_SERV_ARBMODE_SET                    0x10000039
#define SEC_SERV_SIMU_DS0_TEST                  0x10000050
#define SEC_SERV_SIMU_DS1_TEST                  0x10000051
#define SEC_SERV_SIMU_DSBUILD0_TEST             0x10000052
#define SEC_SERV_SIMU_DSBUILD1_TEST             0x10000053
// Fast call services (services run with core specific context)
#define SEC_SERV_FAST_CALL_SIG15_SUBMIT                         0x10000080
#define SEC_SERV_FAST_CALL_A3SP_STATE_REQUEST                   0x10000081

#define SEC_SERV_TEEC_InitializeContext         0x100000F0
#define SEC_SERV_TEEC_FinalizeContext           0x100000F1
#define SEC_SERV_TEEC_OpenSession               0x100000F2
#define SEC_SERV_TEEC_CloseSession              0x100000F3
#define SEC_SERV_TEEC_InvokeCommand             0x100000F4
#define SEC_SERV_TEEC_AllocateSharedMemory      0x100000F5
#define SEC_SERV_TEEC_ReleaseSharedMemory       0x100000F6
#define SEC_SERV_TEEC_RegisterSharedMemory      0x100000F7
#define SEC_SERV_TEEC_RequestCancellation       0x100000F8
#define SEC_SERV_TEE_DMEM_SDRAM_WORKAROUND      0x100000FF

/*!
 * Service status values
 */
#define SEC_SERV_STATUS_OK                      0x00000000
#define SEC_SERV_STATUS_FAIL                    0x00000001
#define SEC_SERV_STATUS_INVALID_INPUT           0x00000002
#define SEC_SERV_STATUS_INVALID_OUTPUT          0x00000003
#define SEC_SERV_STATUS_PROT_DATA_CERT_FAIL     0x00000004
#define SEC_SERV_STATUS_PROT_DATA_FAIL          0x00000005

/*
 * Protected data types
 */
#define SEC_PROT_TYPE_LIST                      0x00000001
#define SEC_PROT_TYPE_DATA                      0x00000002

/*
 * Spinlock types
 */
#define L2_SPINLOCK_TYPE_DEFAULT                0x00000001
#define L2_SPINLOCK_TYPE_TICKET                 0x00000002

#ifndef SECURE_ENVIRONMENT
/* These are return values from the dispatcher. */
#define SEC_ROM_RET_OK                          0x0001
#define SEC_ROM_RET_NON_SUPPORTED_SERV          0x0002
#define SEC_ROM_RET_FAIL                        0x0009
#endif

