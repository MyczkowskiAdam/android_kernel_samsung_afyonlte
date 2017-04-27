/*
*arch/arm/mach-shmobile/tz_comm/sec_msg.h

*Copyright (C) 2013 Renesas Mobile Corporation

*This program is free software; you can redistribute it and/or modify
*it under the terms of the GNU General Public License as published by
*the Free Software Foundation; version 2 of the License.

*This program is distributed in the hope that it will be useful,
*but WITHOUT ANY WARRANTY; without even the implied warranty of
*MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*GNU General Public License for more details.

*You should have received a copy of the GNU General Public License
*along with this program; if not, write to the Free Software
*Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#if defined _SEC_MSG_H_
#error "Multiply included"
#endif

#define _SEC_MSG_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <inttypes.h>
#endif


#define SEC_MSG_BYTE_ORDER_LE   0x00
#define SEC_MSG_BYTE_ORDER_BE   0x01

#define SEC_MSG_OBJECT_ID_NONE  0x00
#define SEC_MSG_PARAM_ID_NONE   0x00

/*!
 * This is the type definition of a secure message. The message itself
 * is always accessed through a handle, so the structure definition
 * is not visible to the user.
 */
typedef struct sec_msg sec_msg_t;

/*!
 * This type is the handle used for sequentially accessing
 * the elements of a message.
 */
typedef struct {
  sec_msg_t *msg;
  uint16_t offset;
} sec_msg_handle_t;

/*!
 * This type indicates the status of message handling.
 */
typedef uint32_t sec_msg_status_t;

/*!
 * Returned status values.
 */
#define SEC_MSG_STATUS_OK                       0
#define SEC_MSG_STATUS_FAIL                     1
#define SEC_MSG_STATUS_PARAM_OUT_OF_RANGE       2
#define SEC_MSG_STATUS_PARAM_NULL               3
#define SEC_MSG_STATUS_PARAM_SIZE_MISMATCH      4
#define SEC_MSG_STATUS_PARAM_EMPTY              5
#define SEC_MSG_STATUS_NULL_PTR                 6
#define SEC_MSG_STATUS_INVALID_PTR              7


/*!
 * Callback used for copying data to client.
 */
typedef unsigned long (*cb_memcpy)(void *dst, const void *src, unsigned long sz);


/*!
 * Message allocation function. This will allocate a zero-initialized
 * 4-byte aligned space for the requested size. This function is only to
 * be used outside of Secure Environment when creating messages
 * (input and output) to be delivered to the Secure Environment.
 *
 * @param[in,out] handle
 *      Handle for accessing message.
 *      This function will set this to the first element in the message.
 *
 * @param[in] size
 *      Size of the message data.
 *
 * @param[in] object_id
 *      Object identifier (reserved).
 *
 * @param[in] msg_version
 *      Service specific message version.
 *
 * @param[in] byte_order
 *      Byte order used in message.
 *
 * @retval sec_msg_t *
 *      Allocated message. NULL if allocation failed.
 *
 */
sec_msg_t *sec_msg_alloc(sec_msg_handle_t *handle,
                         const uint16_t size,
                         const uint32_t object_id,
                         const uint8_t msg_version,
                         const uint8_t byte_order);

/*!
 * Secure message deallocation.
 *
 * @param[in]
 *      msg     Message to free.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_free(sec_msg_t *msg);

/*!
 * Begin accessing a message by setting up the given message handle to the
 * first element in the message. In Secure Environment, this will also check
 * that message msg is entirely within a valid memory area.
 *
 * @param[in,out] handle
 *      Handle for message accesss.
 * @param[in] msg
 *      Message to access.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_open(sec_msg_handle_t *handle,
                              const sec_msg_t *msg);

/*!
 * End accessing a message. This function is mostly used in the Secure
 * Environment to end message processing.
 *
 * @param[in,out] handle
 *      Handle for message access.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_close(sec_msg_handle_t *handle);

/*!
 * Rewind message handle in order to start accessing a message
 * from first element.
 *
 * @param[in,out] handle
 *      Handle for message access.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 */
sec_msg_status_t sec_msg_rewind(sec_msg_handle_t *handle);

/*!
 * Get the byte order used in the message.
 *
 * @param[in] msg
 *      Message.
 * @param[in,out] byte_order
 *      Message byte order.
 *      Possible values:
 *      SEC_MSG_BYTE_ORDER_LE
 *      SEC_MSG_BYTE_ORDER_BE
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_byte_order_get(const sec_msg_t *msg,
                                        uint8_t *byte_order);

/*!
 * Get the size of the message.
 *
 * @param[in] msg
 *      Message to examine.
 * @param[in,out] msg_size
 *      Size of the message.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_size_get(const sec_msg_t *msg,
                                  uint16_t *msg_size);

/*!
 * Get the message version.
 *
 * @param[in] msg
 *      Message.
 * @param[in,out] version
 *      Version.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 */
sec_msg_status_t sec_msg_version_get(const sec_msg_t *msg,
                                     uint8_t *version);

/*!
 * Get the object id of message.
 *
 * @param[in] msg
 *      Message.
 * @param[in,out] object_id
 *      Object identifier in message.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_object_id_get(const sec_msg_t *msg,
                                       uint32_t *object_id);

/*!
 * This function is used for determining the (unaligned) size of a parameter.
 * This should be used for all parameters in order to determine the size
 * of the message data part.
 *
 * @param[in] data_length
 *      Length of data
 *
 * @retval uint16_t
 *      Aligned length of data.
 *
 */
uint16_t sec_msg_param_size(const uint16_t data_length);

/*!
 * Get the size of data stored in an element.
 *
 * @param[in] handle
 *      Handle for message access.
 * @param[in,out] param_size
 *      Data size.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_size_get(const sec_msg_handle_t *handle,
                                        uint16_t *param_size);

/*!
 * Get the id stored in the element.
 *
 * @param[in] handle
 *      Handle for message access.
 * @param[in,out] param_id
 *      Parameter id.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_id_get(const sec_msg_handle_t *handle,
                                      uint8_t *param_id);

/*!
 * Write data to message.
 *
 * @param[in,out] handle
 *      Handle to message. Handle is forwarded to the next element when
 *      operation completes successfully.
 * @param[in] data
 *      Data to write.
 * @param[in] param_id
 *      Parameter id.
 * @param[in] data_length
 *      Length of data to write.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_write(sec_msg_handle_t *handle,
                                     const void *data,
                                     const uint16_t data_length,
                                     const uint8_t param_id);
/* Internal, to-be-used only in special circumstances. */
sec_msg_status_t _sec_msg_param_write(cb_memcpy f_ptr,
                                      sec_msg_handle_t *handle,
                                      const void *data,
                                      const uint16_t data_length,
                                      const uint8_t param_id);

/*!
 * Write byte of data to message.
 *
 * @param[in,out] handle
 *      Handle to message. Handle is forwarded to the next element when
 *      operation completes successfully.
 * @param[in] data
 *      Data to write.
 * @param[in] param_id
 *      Parameter id
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_write8(sec_msg_handle_t *handle,
                                      const uint8_t data,
                                      const uint8_t param_id);

/*!
 * Write 2 bytes of data to message.
 *
 * @param[in,out] handle
 *      Handle to message. Handle is forwarded to the next element when
 *      operation completes successfully.
 * @param[in] data
 *      Data to write.
 * @param[in] param_id
 *      Parameter id
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_write16(sec_msg_handle_t *handle,
                                       const uint16_t data,
                                       const uint8_t param_id);

/*!
 * Write 4 bytes of data to message.
 *
 * @param[in,out] handle
 *      Handle to message. Handle is forwarded to the next element when
 *      operation completes successfully.
 * @param[in] data
 *      Data to write.
 * @param[in] param_id
 *      Parameter id
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_write32(sec_msg_handle_t *handle,
                                       const uint32_t data,
                                       const uint8_t param_id);
/* Internal, to-be-used only in special circumstances. */
sec_msg_status_t _sec_msg_param_write32(cb_memcpy f_ptr,
                                        sec_msg_handle_t *handle,
                                        const uint32_t data,
                                        const uint8_t param_id);

/*!
 * Write 8 bytes of data to message.
 *
 * @param[in,out] handle
 *      Handle to message. Handle is forwarded to the next element when
 *      operation completes successfully.
 * @param[in] data
 *      Data to write.
 * @param[in] param_id
 *      Parameter id.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_write64(sec_msg_handle_t *handle,
                                       const uint64_t data,
                                       const uint8_t param_id);

/*!
 * Read a pointer to an arbitrary length data.
 *
 * @param[in,out] handle
 *      Handle to message.
 * @param[in,out] param_ptr
 *      Read pointer.
 * @param[in,out] data_length
 *      Byte length of data within element will be written here by the function.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_ptr_read(sec_msg_handle_t *handle,
                                        void **param_ptr,
                                        uint16_t *data_length);

/*!
 * Read an arbitrary length of data from message.
 *
 * @param[in,out] handle
 *      Handle to message. Handle is forwarded to the next element when
 *      operation completes successfully.
 * @param[in,out] readbuf
 *      Read buffer.
 * @param[in] length
 *      Length of read buffer.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_read(sec_msg_handle_t *handle,
                                    void *readbuf,
                                    const uint16_t length);
/* Internal, to-be-used only in special circumstances. */
sec_msg_status_t _sec_msg_param_read(cb_memcpy f_ptr,
                                     sec_msg_handle_t *handle,
                                     void *readbuf,
                                     const uint16_t length);

/*!
 * Read byte from message.
 *
 * @param[in,out] handle
 *      Handle to message. Handle is forwarded to the next element when
 *      operation completes successfully.
 * @param[in,out] param
 *      Read parameter.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_read8(sec_msg_handle_t *handle,
                                     uint8_t *param);

/*!
 * Read 2 bytes from message.
 *
 * @param[in,out] handle
 *      Handle to message. Handle is forwarded to the next element when
 *      operation completes successfully.
 * @param[in,out] param
 *      Read parameter.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_read16(sec_msg_handle_t *handle,
                                      uint16_t *param);

/*!
 * Read 4 bytes from message.
 *
 * @param[in,out] handle
 *      Handle to message. Handle is forwarded to the next element when
 *      operation completes successfully.
 * @param[in,out] param
 *      Read parameter.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_read32(sec_msg_handle_t *handle,
                                      uint32_t *param);

/*!
 * Read 8 bytes from message.
 *
 * @param[in,out] handle
 *      Handle to message. Handle is forwarded to the next element when
 *      operation completes successfully.
 * @param[in,out] param
 *      Read parameter.
 *
 * @retval sec_msg_status_t
 *      Status of the operation.
 *
 */
sec_msg_status_t sec_msg_param_read64(sec_msg_handle_t *handle,
                                      uint64_t *param);

