/*
 * Debugging macro include header
 *
 * Copyright (C) 2011  Renesas Electronics Corporation
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
 */

#include <mach/r8a7373.h>
#define SCFSR		0x14
#define SCFTDR		0x20
#define FSR_TEND	(1 << 6)

#define DEBUG_UART_BASE		CONFIG_DEBUG_LL_SH_SCI_BASE

#define DEBUG_UART_BASE_PHY	0xe6450000
#define DEBUG_UART_BASE_VIRT	IO_ADDRESS(DEBUG_UART_BASE_PHY)

	.macro	addruart, rp, rv, tmp
	ldr	\rp, =DEBUG_UART_BASE_PHY
	ldr	\rv, =DEBUG_UART_BASE_VIRT
	.endm

	.macro	senduart, rd, rx
	strb	\rd, [\rx, #SCFTDR]
	.endm

	.macro	busyuart, rd, rx
1001:	ldrh	\rd, [\rx, #SCFSR]
	and	\rd, \rd, #FSR_TEND
	cmp	\rd, #FSR_TEND
	bne	1001b
	.endm

	.macro	waituart, rd, rx
	.endm
