/*
 * drivers/sec_hal/rt/inc/sec_hal_rt.h
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
#ifndef SEC_HAL_RT_H
#define SEC_HAL_RT_H

/* ************************ HEADER (INCLUDE) SECTION ********************* */
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <inttypes.h>
#endif
#include "sec_hal_res.h"


/* ***************** MACROS, CONSTANTS, COMPILATION FLAGS **************** */
/* Size of key info */
#define SEC_HAL_KEY_INFO_SIZE                 32

/* Size of MAC in bytes */
#define SEC_HAL_MAC_SIZE                      6

/* Maximum index for mac address indices */
#define SEC_HAL_MAX_MAC_INDEX                 10

/* Maximum size of IMEI in bytes */
#define SEC_HAL_MAX_IMEI_SIZE                 16

/* Maximum number of bands */
#define SEC_HAL_MAX_BANDS                     5

/* Maximum length of simlock code */
#define SEC_HAL_MAX_SIMLOCK_CODE_LENGTH       17

/* Maximum number of supported simlock levels */
#define SEC_HAL_MAX_SIMLOCK_LEVELS_COUNT      5

/* Type of callback, protected data ID list requested. */
#define SEC_HAL_TYPE_PROT_ID_LIST             1

/* Type of callback, protected data requested. */
#define SEC_HAL_TYPE_PROT_DATA                2


/* ********************** STRUCTURES, TYPE DEFINITIONS ******************* */
/*!
 * This type defines the structure for key info.
 */
typedef struct
{
  uint8_t key_info[SEC_HAL_KEY_INFO_SIZE];
} sec_hal_key_info_t;

/*!
 * This type defines the structure for MAC address.
 */
typedef struct
{
  uint8_t mac_address[SEC_HAL_MAC_SIZE];
} sec_hal_mac_address_t;

/*!
 * This type defines the structure for IMEI. The IMEI is stored as
 * NUL terminated ASCII string containing 14 IMEI digits and a check digit.
 * Structure of this string is specified in the 3GPP TS 23.003 specification.
 * IMEI = TAC(8) + SN(6) + CTRL(1) + NUL(1)
 */
typedef struct
{
  char imei[SEC_HAL_MAX_IMEI_SIZE];
} sec_hal_imei_t;


#define SEC_HAL_RESCNT_COUNT 3

/*!
 * This type defines the runtime init information given by secenv.
 */


typedef struct {
  uint64_t commit_id;
  uint32_t reset_info[SEC_HAL_RESCNT_COUNT];
} sec_hal_init_info_t;

/*!
 * This type defines a callback function that must provide protected data
 * specific allocation services for Security HAL. Used as installed service,
 * i.e. client of this API will implement and install.
 *
 *
 * @param data_type             identifier of data type. Possible values are:
 *                                      SEC_HAL_TYPE_PROT_ID_LIST,
 *                                      SEC_HAL_TYPE_PROT_DATA
 *
 * @param prot_data_id          protected data id. If the first argument is
 *                                      SEC_HAL_TYPE_PROT_ID_LIST,
 *                              then this parameter defines which group's
 *                              data ID list is requested. If the first
 *                               argument is SEC_HAL_TYPE_PROT_DATA,
 *                              then this parameter contains the data ID of
 *                              the requested data.
 *
 * @param prot_data_ptr         start address of allocated protected data.
 *                              If the first argument is
 *                                       SEC_HAL_TYPE_PROT_ID_LIST,
 *                               then the protected data should be a list of
 *                               32-bit data IDs that belong to the data
 *                               group defined in the second argument.
 *                               If the first argument is
 *                                      SEC_HAL_TYPE_PROT_DATA,
 *                               then the protected data should contain
 *                               the protected data object.
 *
 * @param prot_data_size        protected data size, in bytes.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
typedef uint32_t (*sec_hal_data_alloc_handler_cb)(
	uint32_t data_type,
	uint32_t prot_data_id,
	void** prot_data_ptr,
	uint32_t* prot_data_size );

/*!
 * This type defines a callback function that must provide protected data
 * specific de-allocation services. Used as installed service,
 * i.e. client of this API will implement and install.
 *
 *
 * @param data_type[in]         identifier of data type. Possible values are:
 *                                      SEC_HAL_TYPE_PROT_ID_LIST,
 *                                      SEC_HAL_TYPE_PROT_DATA
 *
 * @param prot_data_id[in]      protected data id. If the first argument is
 *                                      SEC_HAL_TYPE_PROT_ID_LIST,
 *                              then this parameter defines which group's
 *                              data ID list is requested. If the
 *                              first argument is SEC_HAL_TYPE_PROT_DATA,
 *                              then this parameter contains the data ID of
 *                              the requested data.
 *
 * @param prot_data_ptr[in]     start address of to-be-freed protected data.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
typedef uint32_t (*sec_hal_data_free_handler_cb)(
	uint32_t data_type,
	uint32_t prot_data_id,
	void* prot_data_ptr );

typedef uint32_t (*sec_hal_rt_rpc_handler)(
	uint32_t id,
	uint32_t p1,
	uint32_t p2,
	uint32_t p3,
	uint32_t p4);

#define SEC_HAL_RPC_ALLOC                      0x80000001
#define SEC_HAL_RPC_FREE                       0x80000002
#define SEC_HAL_RPC_RPMB                       0x80000003
#define SEC_HAL_RPC_STORAGE_INFO_GET           0x80000004
#define SEC_HAL_RPC_FS_ROOT                    0x80000005
#define SEC_HAL_RPC_FS_LOOKUP                  0x80000006
#define SEC_HAL_RPC_FS_READ                    0x80000007
#define SEC_HAL_RPC_FS_WRITE                   0x80000008
#define SEC_HAL_RPC_FS_CREATE                  0x80000009
#define SEC_HAL_RPC_FS_REMOVE                  0x8000000A
#define SEC_HAL_RPC_FS_MOVE                    0x8000000B
#define SEC_HAL_RPC_FS_SIZE                    0x8000000C
#define SEC_HAL_RPC_PROT_DATA_FREE             0x8000000D
#define SEC_HAL_RPC_PROT_DATA_ALLOC            0x8000000E
#define SEC_HAL_RPC_TRACE                      0x8000000F


/* ************************** FUNCTION PROTOTYPES ************************ */
/*!
 * This function can be used to initialize the Security HAL. If successfully
 * executed then secure timer expiration value is received as an output.
 * That value should be used to initiate timer which should call the periodic
 * integrity check function after expiration.
 * 
 * Note: This function should not be called from interrupt context.
 *
 * @param sec_exp_time         Expiration time for the first 'period',
 *                              in milliseconds. This value defines
 *                              the amount of time that should pass before
 *                              first periodic integrity check. Function
 *                              to-be-used to make 'periodic' check is:
 *                                  sec_hal_periodic_integrity_check.
 *                              If the 'periodic' check is not done then
 *                              watchdog timer will cause HW reset.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_init(sec_hal_init_info_t *runtime_init);

/*!
 * This function can be used to install rpc handler to the secure side.
 * Handler will be called whenever secure side needs something special
 * from the public domain.
 */
uint32_t sec_hal_rt_install_rpc_handler(sec_hal_rt_rpc_handler fptr);

/*!
 * This function is used to retrieve key information from the secure side.
 * Key information contains data which can be used to create valid
 * certificates.
 *
 * @pre                         sec_hal_init - function called successfully.
 *
 * @param key_info              Pointer to a struct where the key info
 *                              is written.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_key_info_get(sec_hal_key_info_t *user_key_info_out);

/*!
 * This function can be used to register a certificate.
 * Certificate can be registered by copying the certificate
 * from the file system to a buffer in SDRAM and then providing
 * a pointer to the certificate and the certificate size to this function.
 * 
 * There are some differences between registering a SW image certificate
 * and registering other certificates.
 * When registering a SW image certificate:
 *           - The registration does not affect other Security HAL services.
 *             In other words, the other Security HAL services can be used
 *             even if the SW image certificate is not registered.
 *           - The SDRAM buffer is not needed by the Security HAL after
 *             the function returns, so the caller of the function may
 *             free the memory.
 *           - The image is authenticated when the SW image certificate
 *             is registered.
 *             The function does not return before the authentication
 *             is completed.
 * When registering other certificates:
 *           - Many Security HAL services require certificates.
 *             Therefore all the certificates (except SW image certificates)
 *             should be registered before using other Security HAL services.
 *           - The SDRAM buffer is needed by the Security HAL also after
 *             the function returns, so the caller of the function should
 *             never free the memory.
 * 
 * 
 * @pre                         sec_hal_init - function called successfully.
 * 
 * @param cert                  Pointer to the certificate buffer.
 * 
 * @param cert_size             Size of the certificate buffer.
 * 
 * @param data_ptr              Optional output parameter. The caller of
 *                              the function reserves a buffer for pointer
 *                              and this function writes a pointer
 *                              to that buffer. The actual data depends
 *                              on the use case. For example, the data could
 *                              be a pointer to a SW image, if SW image
 *                              certificate is registered using this
 *                              function. However, currently there are no
 *                              use cases for this parameter, so
 *                              data_ptr can always be set to NULL.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_cert_register(
	void *user_cert_in,
	uint32_t user_cert_size_in,
	uint32_t* user_obj_id_out);

/*!
 * This function can be used to register a data certificate.
 * Data certificate can be registered by copying the certificate
 * from the file system to a buffer in SDRAM and then providing
 * a pointer to the certificate and the certificate size to this function.
 * Also data itself must be provided as an input parameter.
 *
 * Difference to the 'sec_hal_cert_register' is that the certificate,
 * or the data, does not need to remain in SDRAM after registration.
 * Certificate is checked when registered, not later on in periodic checks.
 * Thus SW certificates cannot be registered with this function.
 *
 *
 * @pre                         sec_hal_init - function called successfully.
 *
 * @param cert                  Pointer to the certificate buffer.
 *
 * @param cert_size             Size of the certificate buffer.
 *
 * @param data                  Pointer to the actual data in SDRAM.
 *
 * @param data_size             Size of the actual data in SDRAM.
 *
 * @param data_ptr              Optional output parameter. With this
 *                              parameter additional information
 *                              about the certificate content can be
 *                              passed back to the caller. Can be NULL
 *                              if the caller does not want or need the
 *                              additional information. Usually this
 *                              parameter will be the OBJ_ID of the
 *                              registered certificate. Minimum of four
 *                              byte space needed if this parameter is
 *                              provided.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_data_cert_register(
	void *user_cert_in,
	uint32_t user_cert_size_in,
	void *user_data_in,
	uint32_t user_data_size_in,
	uint32_t *user_id_ptr_out);

/*!
 * This function can be used to get the MAC address of the device.
 * 
 * 
 * @pre                         sec_hal_init - function called successfully.
 *
 * @param user_index_in         Index of the requested MAC. Can be [0, 9].
 * 
 * @param mac_addr              Pointer to a struct where the MAC is written.
 * 
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_mac_address_get(
	uint32_t user_index_in,
	sec_hal_mac_address_t *user_mac_addr_out);

/*!
 * This function can be used to get the IMEI of the device.
 * 
 * 
 * @pre                         sec_hal_init - function called successfully.
 * 
 * @param imei                  Pointer to a struct where the IMEI is
 *                              written.
 * 
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_imei_get(
	sec_hal_imei_t *user_imei_out);

/*!
 * This function can be used to open all SIM lock levels.
 *
 *
 * @pre                         sec_hal_init - function called successfully.
 *
 * @param levels_mask           Bitmask defining which SIM lock levels should
 *                              be opened. The bit 0 (least significant bit)
 *                              of the mask means the level 1 of SIM lock.
 *                              The bit 1 stand for the level 2 and so on.
 *
 * @param unlock_codes          Pointer to a buffer which contains the unlock
 *                              codes separated with NULL character. The
 *                              number of bits in the first parameter should
 *                              describe which unlock codes are used for
 *                              unlocking. for example, if second and fourth
 *                              levels are to be opened, then first parameter
 *                              should be: 0x0A
 *                              and the buffer could contain the following:
 *                                      'ONE\0TWO\0THREE\0FOUR\0'
 *                              then only levels 2 and 4 would be unlocked,
 *                              assuming that the unlock codes were correct.
 *                              If the first and second parameter do not
 *                              match, SEC_HAL_RES_PARAM_ERROR is returned.
 *
 * @param post_lock_level_status Status of the SIM lock levels after this
 *                              operation, as a bitmask.
 *
 * @return uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_simlock_levels_open(
	uint32_t user_levels_mask_in,
	void* user_unlock_codes_in,
	uint32_t* user_post_lock_level_status_out);

/*!
 * This function can be used to open a SIM lock level when the user
 * has entered the unlock code. If the the given unlock code was right then
 * the return value will be SEC_HAL_RES_OK, and SIM lock status is updated.
 * If the unlock code is wrong then SEC_HAL_RES_FAIL is returned.
 * 
 * 
 * @pre                         sec_hal_init - function called successfully.
 * 
 * @param unlock_code           Pointer to a NUL terminated ASCII string.
 * 
 * @param lock_level            SIM lock level that the unlock code
 *                              should open.
 * 
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_simlock_level_open(
	char *user_unlock_code_in,
	uint8_t user_lock_level_in);

/*!
 * This function is used to get the size of the device authentication data.
 * 
 * 
 * @pre                         sec_hal_init - function called successfully.
 * 
 * @param input_data_size       The size of the input authentication data.
 * 
 * @param input_data            Pointer to a buffer which contains
 *                              the input authentication data.
 *                              The size of the buffer is: input_data_size.
 *
 * @param auth_data_size        Pointer to a variable where the output
 *                              authentication data size is stored.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_auth_data_size_get(
	void* user_input_data_in,
	uint32_t user_input_data_size_in,
	uint32_t* user_auth_data_size_out);

/*!
 * This function can be used to request the data that is required
 * in certificate creation.
 * 
 * 
 * @pre                         sec_hal_init - function called successfully.
 * 
 * @param input_data_size       The size of the input authentication data.
 * 
 * @param input_data            Pointer to a buffer which contains
 *                              the input authentication data.
 *                              The size of the buffer is: input_data_size.
 *
 * @param auth_data_size        The size of the requested authentication
 *                              data.
 *
 * @param auth_data             Pointer to a buffer where
 *                              the device authentication data is written.
 *                              The size of the buffer is: auth_data_size.
 * 
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_auth_data_get(
	void *user_input_data_in,
	uint32_t user_input_data_size_in,
	void *user_auth_data_out,
	uint32_t user_auth_data_size_in);

/*!
 * This function must be used to make a periodic check of SW certificate
 * protected files. If the periodic integrity check is not done in timely
 * manner then watchdog timer will cause HW reset.
 *
 *
 * @pre                         sec_hal_init - function called successfully.
 *
 * @param user_wdt_upd_out      Expiration time for the next 'period',
 *                              in milliseconds. This value defines
 *                              the amount of time that should pass before
 *                              next periodic check.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_periodic_integrity_check(
	uint32_t *user_wdt_upd_out);

/*!
 * This function can be used after initialization sequence to check if
 * all initial, and mandatory, security checks have completed successfully.
 *
 * @pre                         sec_hal_init - function called successfully.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 */
uint32_t sec_hal_rt_selftest(void);

/*!
 * This function can be used to request a key initialization of cc42 block
 * from the secure environment.
 *
 * @pre                         sec_hal_init - function called successfully.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 */
uint32_t sec_hal_rt_public_cc42_key_init(void);

/*!
 * This function can be used to send a3sp powerdomain noticifation event
 * to the secure environment.
 *
 * @pre                         sec_hal_init - function called successfully.
 *
 * @param request_in
 *
 * @param virt_allowed_out
 * 
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_a3sp_state_info(
		uint32_t request_in,
		uint32_t *virt_allowed_out);

/*!
 * This function can be used to send poweroff event to the secure
 * environment.
 *
 * @pre                         sec_hal_init - function called successfully.
 *
 * @param phys_dst_in
 * 
 * @param phys_src_in
 * 
 * @param size_in
 * 
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_rt_memcpy(
	void* phys_dst_in,
	void* phys_src_in,
	uint32_t size_in);

/*!
 * This function can be used to send coma entry event to the secure
 * environment. Caller must specify parameters which are used by
 * the secure environment during wakeup.
 *
 * @pre                         sec_hal_init - function called successfully.
 *
 * @param mode
 *
 * @param wakeup_address
 *
 * @param pll0
 *
 * @param zclk
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 *                              SEC_HAL_RES_PARAM_ERROR error if faulty args.
 */
uint32_t sec_hal_pm_coma_entry(
	uint32_t mode,
	uint32_t wakeup_address,
	uint32_t pll0,
	uint32_t zclk);

/*!
 * This function can be used to send poweroff event to the secure
 * environment.
 *
 * @pre                         sec_hal_init - function called successfully.
 *
 * @retval uint32_t             SEC_HAL_RES_OK if success, error otherwise.
 *                              SEC_HAL_RES_FAIL error if operation failed.
 */
uint32_t sec_hal_pm_poweroff(void);

/* ******************************** END ********************************** */

#endif /* SEC_HAL_RT_H */

