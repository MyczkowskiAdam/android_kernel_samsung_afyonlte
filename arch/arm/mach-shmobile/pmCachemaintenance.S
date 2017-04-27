/*
 * arch/arm/mach-shmobile/pmCachemaintenance.S
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/****************************************************/
/* Power Management Cache maintenance macros	*/
/* Macro List:						*/
/*	L1_DataCacheCleanSW				*/
/*	L1_DataCacheInvalidateSW		*/
/*	L1_DataCacheCleanInvalidateSW	*/
/****************************************************/

/*==================================================*/
/*  Name: L1_DataCacheCleanSW						*/
/*  Overview: L1 data cache clean all by set/way.	*/
/*==================================================*/
	.macro L1_DataCacheCleanSW
	/* This macro is optimized for Cortex-A9. */
	/* L1 data cache is 4-way set-associative */
	/* and the cache line length is eight words. */
	/* CCSIDR.NumSets = 0xFF, CCSIDR.Associativity = 0x3, */
	/* CCSIDR.LineSize = 0x1.*/
	MOV		r4, #0x0
	MCR		p15, 2, r4, c0, c0, 0
	ISB
	MRC		p15, 1, r4, c0, c0, 0
	UBFX	r5, r4, #0xD, #0xF
	/* The format of register data for a set/way operation */
	/* Way = bit[31:30], Set = bit[12:5], Level = bit[3:1] */
	MOV		r4, r5, LSL #0x5		/* r4 = 0x00001FE0 */
	MOV		r8, #0x4
	ADD		r5, r4, r8, LSL #0x1C	/* r5 = r4 + 0x40000000 */
	MOV		r8, #0x8
	ADD		r6, r4, r8, LSL #0x1C	/* r6 = r4 + 0x80000000 */
	MOV		r8, #0xC
	ADD		r7, r4, r8, LSL #0x1C	/* r7 = r4 + 0xC0000000 */
1:
	/* Clean data or unified cache line by set/way. */
	MCR		p15, 0, r4, c7, c10, 2
	MCR		p15, 0, r5, c7, c10, 2
	MCR		p15, 0, r6, c7, c10, 2
	MCR		p15, 0, r7, c7, c10, 2
	SUBS	r4, r4, #0x20
	SUB		r5, r5, #0x20
	SUB		r6, r6, #0x20
	SUB		r7, r7, #0x20
	BPL		1b
	DSB
	.endm


/*======================================================*/
/*  Name: L1_DataCacheInvalidateSW					*/
/*  Overview: L1 data cache invalidate all by set/way.	*/
/*======================================================*/
	.macro L1_DataCacheInvalidateSW
	/* This macro is optimized for Cortex-A9. */
	/* L1 data cache is 4-way set-associative */
	/* and the cache line length is eight words. */
	/* CCSIDR.NumSets = 0xFF, CCSIDR.Associativity = 0x3, */
	/* CCSIDR.LineSize = 0x1. */
	MOV		r4, #0x0
	MCR		p15, 2, r4, c0, c0, 0
	ISB
	MRC		p15, 1, r4, c0, c0, 0
	UBFX	r5, r4, #0xD, #0xF
	/* The format of register data for a set/way operation */
	/* Way = bit[31:30], Set = bit[12:5], Level = bit[3:1] */
	MOV		r4, r5, LSL #0x5		/* r4 = 0x00001FE0 */
	MOV		r8, #0x4
	ADD		r5, r4, r8, LSL #0x1C	/* r5 = r4 + 0x40000000 */
	MOV		r8, #0x8
	ADD		r6, r4, r8, LSL #0x1C	/* r6 = r4 + 0x80000000 */
	MOV		r8, #0xC
	ADD		r7, r4, r8, LSL #0x1C	/* r7 = r4 + 0xC0000000 */
1:
	/* Invalidate data or unified cache line by set/way. */
	MCR		p15, 0, r4, c7, c6, 2
	MCR		p15, 0, r5, c7, c6, 2
	MCR		p15, 0, r6, c7, c6, 2
	MCR		p15, 0, r7, c7, c6, 2
	SUBS	r4, r4, #0x20
	SUB		r5, r5, #0x20
	SUB		r6, r6, #0x20
	SUB		r7, r7, #0x20
	BPL		1b
	DSB
	.endm


/*==============================================================*/
/*  Name:L1_DataCacheCleanInvalidateSW */
/*  Over View:L1 data cache clean + invalidate all by set/way. */
/*==============================================================*/
	.macro L1_DataCacheCleanInvalidateSW
	/* This macro is optimized for Cortex-A9. */
	/* L1 data cache is 4-way set-associative */
	/* and the cache line length is eight words. */
	/* CCSIDR.NumSets = 0xFF, CCSIDR.Associativity = 0x3, */
	/* CCSIDR.LineSize = 0x1. */
	MOV		r4, #0x0
	MCR		p15, 2, r4, c0, c0, 0
	ISB
	MRC		p15, 1, r4, c0, c0, 0
	UBFX	r5, r4, #0xD, #0xF
	/* The format of register data for a set/way operation */
	/* Way = bit[31:30], Set = bit[12:5], Level = bit[3:1] */
	MOV		r4, r5, LSL #0x5		/* r4 = 0x00001FE0 */
	MOV		r8, #0x4
	ADD		r5, r4, r8, LSL #0x1C	/* r5 = r4 + 0x40000000 */
	MOV		r8, #0x8
	ADD		r6, r4, r8, LSL #0x1C	/* r6 = r4 + 0x80000000 */
	MOV		r8, #0xC
	ADD		r7, r4, r8, LSL #0x1C	/* r7 = r4 + 0xC0000000 */
1:
	/* Clean and Invalidate data or unified cache line by set/way. */
	MCR		p15, 0, r4, c7, c14, 2
	MCR		p15, 0, r5, c7, c14, 2
	MCR		p15, 0, r6, c7, c14, 2
	MCR		p15, 0, r7, c7, c14, 2
	SUBS	r4, r4, #0x20
	SUB		r5, r5, #0x20
	SUB		r6, r6, #0x20
	SUB		r7, r7, #0x20
	BPL		1b
	DSB
	.endm

