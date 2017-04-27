/*
 * drivers/sec_hal/rt/inc/tee_defs.h
 *
 * Copyright (c) 2012-2013, Renesas Mobile Corporation.
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
#ifndef TEE_DEFS_H
#define TEE_DEFS_H


/* ************************ HEADER (INCLUDE) SECTION *********************** */

/* Maximum size for shared memory */
#define TEEC_CONFIG_SHAREDMEM_MAX_SIZE 0x80000

typedef uint32_t TEEC_Result;

/*The operation was successful. */
#define TEEC_SUCCESS 0x00000000

/*Non-specific cause. */
#define TEEC_ERROR_GENERIC 0xFFFF0000

/*Access privileges are not sufficient. */
#define TEEC_ERROR_ACCESS_DENIED 0xFFFF0001

/*The operation was cancelled. */
#define TEEC_ERROR_CANCEL 0xFFFF0002

/*Concurrent accesses caused conflict. */
#define TEEC_ERROR_ACCESS_CONFLICT 0xFFFF0003

/*Too much data for the requested operation was passed. */
#define TEEC_ERROR_EXCESS_DATA 0xFFFF0004

/*Input data was of invalid format. */
#define TEEC_ERROR_BAD_FORMAT 0xFFFF0005

/*Input parameters were invalid. */
#define TEEC_ERROR_BAD_PARAMETERS 0xFFFF0006

/*Operation is not valid in the current state. */
#define TEEC_ERROR_BAD_STATE 0xFFFF0007

/*The requested data item is not found. */
#define TEEC_ERROR_ITEM_NOT_FOUND 0xFFFF0008

/*The requested operation should exist but is not yet implemented. */
#define TEEC_ERROR_NOT_IMPLEMENTED 0xFFFF0009

/*The requested operation is valid but is not supported in this Implementation. */
#define TEEC_ERROR_NOT_SUPPORTED 0xFFFF000A

/*Expected data was missing. */
#define TEEC_ERROR_NO_DATA 0xFFFF000B

/*System ran out of resources */
#define TEEC_ERROR_OUT_OF_MEMORY 0xFFFF000C

typedef struct
    {
    uint32_t timeLow;
    uint16_t timeMid;
    uint16_t timeHiAndVersion;
    uint8_t clockSeqAndNode[8];
    } TEEC_UUID;

typedef struct
    {
    /*<Implementation-Defined Type> imp;*/
    void * tag;
    void * hal_connection;
    } TEEC_Context_Data;

typedef struct
    {
    /*<Implementation-Defined Type> imp;*/
    TEEC_Context_Data imp;
    } TEEC_Context;

typedef struct
    {
    /*<Implementation-Defined Type> imp;*/
    void * tag;
    void * context;
    } TEEC_Session_Data;

typedef struct
    {
    /*<Implementation-Defined Type> imp;*/
    TEEC_Session_Data imp;
    } TEEC_Session;

typedef struct
    {
    void * tag;
    } TEEC_SharedMemoryData;

typedef struct
    {
    void*    buffer;
    size_t   size;
    uint32_t flags;
    TEEC_SharedMemoryData imp;
    } TEEC_SharedMemory;

typedef struct
    {
    void* buffer;
    size_t size;
    } TEEC_TempMemoryReference;

typedef struct
    {
    TEEC_SharedMemory* parent;
    size_t size;
    size_t offset;
    } TEEC_RegisteredMemoryReference;

typedef struct
    {
    uint32_t a;
    uint32_t b;
    } TEEC_Value;

typedef union
    {
    TEEC_TempMemoryReference tmpref;
    TEEC_RegisteredMemoryReference memref;
    TEEC_Value value;
    } TEEC_Parameter;

typedef struct 
    { 
    uint32_t started;
    uint32_t paramTypes;
    TEEC_Parameter params[4];
    /*<Implementation-Defined Type> imp;*/
    uint32_t imp;
    } TEEC_Operation;

/* The return code is an error that originated within the TEE Client API
implementation. */
#define TEEC_ORIGIN_API 0x00000001

/* The return code is an error that originated within the underlying communications stack linking the rich OS with the TEE.*/
#define TEEC_ORIGIN_COMMS 0x00000002

/* The return code is an error that originated within the common TEE code.*/
#define TEEC_ORIGIN_TEE 0x00000003

/* The return code originated within the Trusted Application code. This includes the case where the return code is a success.*/
#define TEEC_ORIGIN_TRUSTED_APP 0x00000004


/* No login data is provided. */
#define TEEC_LOGIN_PUBLIC 0x00000000

/* Login data about the user running the Client Application process is
provided. */
#define TEEC_LOGIN_USER 0x00000001

/* Login data about the group running the Client Application process is
provided. */
#define TEEC_LOGIN_GROUP 0x00000002

/* Login data about the running Client Application itself is provided. */
#define TEEC_LOGIN_APPLICATION 0x00000004




/*The Shared Memory can carry data from the Client Application to the Trusted
Application.*/
#define TEEC_MEM_INPUT 0x00000001

/*The Shared Memory can carry data from the Trusted Application to the Client Application.
All other flag values Reserved for Future Use*/
#define TEEC_MEM_OUTPUT 0x00000002

/*The Parameter is not used*/
#define TEEC_NONE 0x00000000

/*The Parameter is a TEEC_Value tagged as input.*/
#define TEEC_VALUE_INPUT 0x00000001

/*he Parameter is a TEEC_Value tagged as output.*/
#define TEEC_VALUE_OUTPUT 0x00000002

/*The Parameter is a TEEC_Value tagged as both as input and output, i.e., for
which both the behaviors of TEEC_VALUE_INPUT and TEEC_VALUE_OUTPUT apply.*/
#define TEEC_VALUE_INOUT 0x00000003

/*The Parameter is a TEEC_TempMemoryReference describing a region of memory which needs to be temporarily registered for the duration of the Operation and is tagged as input.*/
#define TEEC_MEMREF_TEMP_INPUT 0x00000005

/*Same as TEEC_MEMREF_TEMP_INPUT, but the Memory Reference is tagged as output.
The Implementation may update the size field to reflect the required output
size in some use cases.*/
#define TEEC_MEMREF_TEMP_OUTPUT 0x00000006

/* A Temporary Memory Reference tagged as both input and output, i.e., for which
both the behaviors of TEEC_MEMREF_TEMP_INPUT and TEEC_MEMREF_TEMP_OUTPUT
apply.*/
#define TEEC_MEMREF_TEMP_INOUT 0x00000007

/*The Parameter is a Registered Memory Reference that refers to the entirety of
its parent Shared Memory block. The parameter structure is a
TEEC_MemoryReference. In this structure, the Implementation MUST read only the
parent field and MAY update the size field when the operation completes.*/
#define TEEC_MEMREF_WHOLE 0x0000000C

/* A Registered Memory Reference structure that refers to a partial region of
its parent Shared Memory block and is tagged as input.*/
#define TEEC_MEMREF_PARTIAL_INPUT 0x0000000D

/* A Registered Memory Reference structure that refers to a partial region of
its parent Shared Memory block and is tagged as output. */
#define TEEC_MEMREF_PARTIAL_OUTPUT 0x0000000E


/* The Registered Memory Reference structure that refers to a partial region of
its parent Shared Memory block and is tagged as both input and output, i.e.,
for which both the behaviors of TEEC_MEMREF_PARTIAL_INPUT and
TEEC_MEMREF_PARTIAL_OUTPUT apply.*/
#define TEEC_MEMREF_PARTIAL_INOUT 0x0000000F


#define TEEC_PARAM_TYPES( param0Type, param1Type, param2Type, param3Type)   ((((param0Type) & 0xFF) <<  0) | \
                                                                             (((param1Type) & 0xFF) <<  8) | \
                                                                             (((param2Type) & 0xFF) << 16) | \
                                                                             (((param3Type) & 0xFF) << 24))

#define TEEC_PARAM_TYPE_GET(t, i) (((t) >> (i*8)) & 0xFF)

#endif /* TEE_DEFS_H */

