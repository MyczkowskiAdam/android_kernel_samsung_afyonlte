/*
 * arch/arm/mach-shmobile/pmMacro.S
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

/********************************************/
/* Power Management Macros					*/
/* Macro List: 								*/
/*					PM_ES2_2_And_After*/
/********************************************/

/*==========================================*/
/*	Name: PM_ES2_2_And_After */
/*	Overview: Get PM PM_ES2_2_And_After */
/*==========================================*/
	.macro PM_ES2_2_And_After
	MRC		p15, 0, r7, c1, c0, 0	/* System Control Register */
	TST		r7, #0x1
	LDRNE	r4, =ram0ES_2_2_AndAfter
	LDREQ	r4, =ram0ES_2_2_AndAfterPhys
	LDR 	r4, [r4]
	CMP		r4, #0x0
	.endm

