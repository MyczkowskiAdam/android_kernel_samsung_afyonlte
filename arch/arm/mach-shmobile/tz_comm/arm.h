 /*************************************************************************
  **     Copyright (C) 2010 Nokia Corporation. All rights reserved.      **
  **                                                                     **
  ** Permission is hereby granted, free of charge, to any person         **
  ** obtaining a copy of this software and associated documentation      **
  ** files (the "Software"), to deal in the Software without             **
  ** restriction, including without limitation the rights to use, copy,  **
  ** modify, merge, publish, distribute, sublicense, and/or sell copies  **
  ** of the Software, and to permit persons to whom the Software is      **
  ** furnished to do so, subject to the following conditions:            **
  **                                                                     **
  ** The above copyright notice and this permission notice shall be      **
  ** included in all copies or substantial portions of the Software.     **
  ** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,     **
  ** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF  **
  ** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND               **
  ** NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS **
  ** BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN  **
  ** ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN   **
  ** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE    **
  ** SOFTWARE.                                                           **
  **                                                                     **
  *************************************************************************
  **                                                                     **
  ** File:  arm.inc                                                      **
  **                                                                     **
  ** Desc:  ARM includes                                                 **
  **                                                                     **
  *************************************************************************/

 
  ARM specific definitions
 
.equ MODE_USER,                                0x10    10000b
.equ MODE_FIQ,                                 0x11    10001b
.equ MODE_IRQ,                                 0x12    10010b
.equ MODE_SVC,                                 0x13    10011b
.equ MODE_ABORT,                               0x17    10111b
.equ MODE_UNDEF,                               0x1B    11011b
.equ MODE_SYSTEM,                              0x1F    11111b
.equ MODE_MONITOR,                             0x16    10110b
.equ MODE_CLR_MASK,                            0x1F    11111b

.equ ARM_THUMB_MODE_MASK,                      0x20    100000b   ARM = 0, THUMB = 1

.equ FIQ_IRQ_MASK,                             0xC0    11000000b
.equ FIQ_MASK,                                 0x40    01000000b
.equ IRQ_MASK,                                 0x80    10000000b


  ABSTRACT, ARM includes
.equ HW_ARM_CP15_CIR_READ,                     0  /*          c0,  c0,  1 */
.equ HW_ARM_CP15_CONTROL_READ,                 1  /*          c1,  c0,  0 */
.equ HW_ARM_CP15_CONTROL_WRITE,                2  /*          c1,  c0,  0 */
.equ HW_ARM_CP15_DRAIN_WRITE_BUFFER,           3  /*          c7,  c10, 4 */
.equ HW_ARM_CP15_CLEAN_INVALIDATE_D_CACHE,     4  /* 926 ONLY !!! special */
.equ HW_ARM_CP15_FLUSH_D_CACHE_ENTRY_MVA,      5  /*          c7,  c6,  1 */
.equ HW_ARM_CP15_FLUSH_I_CACHE,                6  /*          c7,  c5,  0 */
.equ HW_ARM_CP15_CLEAN_D_CACHE,                7  /* 926 ONLY !!! special */

 /*----------------------------------------------------------------------------
  * ARM925 and ARM926 CP15 register definitions
  *----------------------------------------------------------------------------
  */


 /* ---------------------------------------------------------------------------
  * HW_ARM_CP15_CIR_READ                           0           c0,  c0,  1
  * ---------------------------------------------------------------------------
  */
.equ HW_ARM_CP15_CIR_I_LINE_LEN_MASK,          0x00000003
.equ HW_ARM_CP15_CIR_I_MBIT_MASK,              0x00000004
.equ HW_ARM_CP15_CIR_I_ASSOC_MASK,             0x00000038
.equ HW_ARM_CP15_CIR_I_SIZE_MASK,              0x000003C0

.equ HW_ARM_CP15_CIR_D_LINE_LEN_MASK,          0x00003000
.equ HW_ARM_CP15_CIR_D_MBIT_MASK,              0x00004000
.equ HW_ARM_CP15_CIR_D_ASSOC_MASK,             0x00038000
.equ HW_ARM_CP15_CIR_D_SIZE_MASK,              0x003C0000

.equ HW_ARM_CP15_CIR_S_MASK,                   0x01000000
.equ HW_ARM_CP15_CIR_CTYPE_MASK,               0x1E000000

 /* ---------------------------------------------------------------------------
  * HW_ARM_CP15_CONTROL_READ                       1           c1,  c0,  0
  * HW_ARM_CP15_CONTROL_WRITE                      2           c1,  c0,  0
  * ---------------------------------------------------------------------------
  */

.equ HW_ARM_CP15_CR_M_MASK,                    (1<<0)
.equ HW_ARM_CP15_CR_A_MASK,                    (1<<1)
.equ HW_ARM_CP15_CR_C_MASK,                    (1<<2)

.equ HW_ARM_CP15_CR_B_MASK,                    (1<<7)

.equ HW_ARM_CP15_CR_SW_MASK,                   (1<<10)
.equ HW_ARM_CP15_CR_Z_MASK,                    (1<<11)
.equ HW_ARM_CP15_CR_I_MASK,                    (1<<12)
.equ HW_ARM_CP15_CR_V_MASK,                    (1<<13)
.equ HW_ARM_CP15_CR_RR_MASK,                   (1<<14)

.equ HW_ARM_CP15_CR_HA_MASK,                   (1<<17)

.equ HW_ARM_CP15_CR_FI_MASK,                   (1<<21)
.equ HW_ARM_CP15_CR_U_MASK,                    (1<<22)

.equ HW_ARM_CP15_CR_VE_MASK,                   (1<<24)
.equ HW_ARM_CP15_CR_EE_MASK,                   (1<<25)

.equ HW_ARM_CP15_CR_NMFI_MASK,                 (1<<27)
.equ HW_ARM_CP15_CR_TRE_MASK,                  (1<<28)
.equ HW_ARM_CP15_CR_AFE_MASK,                  (1<<29)
.equ HW_ARM_CP15_CR_TE_MASK,                   (1<<30)

 ****************************************************************************
 
        SET_ARM_CP15_V_BIT
 
            This macro sets V bit of ARM926 CP15 register to 'High exception
            vectors'
 
 ****************************************************************************

         .macro SET_ARM_CP15_V_BIT  reg
 
         ORR     \reg, \reg, #0x2000         set V bit to 'High exception vect'
 
         .endm

 
 ****************************************************************************
 
        ENABLE_INTS_FROM_ASIC
 
            This macro enables interrupts from register that ishould be checked
            as part of secure mode entry conditions with other than TrustZone
            based security solutions. Note that interrupts shall remain
            disabled by some other means (e.g. ARM926 CPSR register bits) also
            after calling this macro.
 
 ****************************************************************************
        .macro ENABLE_INTS_FROM_ASIC reg1, reg2

         .extern asic_ints_enable_physical
         LDR     \reg1, =asic_ints_enable_physical    Enable ints
         MOV     \reg2, #1                            from ASIC
         STR     \reg2, [\reg1, #0] 

        .endm

 
 ****************************************************************************
 
        DISABLE_INTS_FROM_ASIC
 
            This macro disables interrupts from register that should be checked
            as part of secure mode entry conditions with other than TrustZone
            based security solutions. Note that interrupts shall also
            be disabled by some other means (e.g. ARM926 CPSR register bits).
            As this macro is called with MMU disabled, register shall
            be accessed using physical address.
 
 ****************************************************************************
        .macro DISABLE_INTS_FROM_ASIC reg1, reg2

         .extern asic_ints_disable_physical
         LDR     \reg1, =asic_ints_disable_physical   Disable ints 
         MOV     \reg2, #1                            from ASIC
         STR     \reg2, [\reg1, #0] 

        .endm


 
 ****************************************************************************
 
        SET_CP15_DEBUG_OVERRIDE_VALUE
 
            This macro modifies CP15 debug override register value for secure
            mode entry.
 
 ****************************************************************************
        .macro SET_CP15_DEBUG_OVERRIDE_VALUE reg

        ORR     \reg, \reg, #0x00010000       Disable prefetching

        .endm


 
 ****************************************************************************
 
        CHANGE_TO_ARM_MODE
 
            This macro changes ARM926 from 16-bit Thumb instruction mode to
            32-bit ARM instruction mode
 
 ****************************************************************************
        .macro CHANGE_TO_ARM_MODE
        B       5f
        .align
5:      BX      PC                        Change to ARM mode
        NOP
        .endm

 
 ****************************************************************************
 
        CHANGE_TO_THUMB_MODE
 
            This macro changes ARM926 from 32-bit ARM instruction mode to
            16-bit Thumb instruction mode. A name of one register has to be
            given as parameter. Original content of this register will be lost.
 
 ****************************************************************************
        .macro CHANGE_TO_THUMB_MODE reg
        MOV     \reg, PC
        ADD     \reg, \reg, #5                Change state to Thumb mode
        BX      \reg
        .endm


/* vim: set autoindent shiftwidth=8 smarttab expandtab : */
/* -*- mode: C  c-basic-indent: 8  indent-tabs-mode: nil -*- */
