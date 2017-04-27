/*
 * iccom_hw.h
 *	 H/W address mapping definition header file.
 *
 * Copyright (C) 2012-2013 Renesas Electronics Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef __ICCOM_HW_H__
#define __ICCOM_HW_H__

#include <mach/hardware.h>

/* interrupt number */
#ifdef RMU2
#define INT_ICCOM                   (32+126)
#else
#define INT_ICCOM                   90
#endif

/* HPBREG register define */
#define ICCOMGSR_INIT			0x00000000
#define ICCOMGSR_INIT_REQ		0x00000001
#define ICCOMGSR_FATAL_COMP		0x00000001

#define ICCOMSCR_INIT			0x00000043
#define ICCOMSCR_ENDIAN			0x00000001

#define ICCOMIICR_INIT			0x00000000
#define ICCOMIICR_INT			0x00000001
#define ICCOMIICR_READ			0x00000002
#define ICCOMIICR_DATA_TOP		0x00000010
#define ICCOMIICR_DATA_MID		0x00000000
#define ICCOMIICR_DATA_BTM		0x00000020
#define ICCOMIICR_DATA_ONLY		0x00000030
#define ICCOMIICR_CMD1			0x00000000
#define ICCOMIICR_CMD2			0x00000040

#define ICCOMIICR_RTBOOT		0x00000010

#define ICCOMIICR_DATA_BIT		ICCOMIICR_DATA_ONLY
#define ICCOMIICR_CMD_BIT		ICCOMIICR_CMD2

#define ICCOMIICR_BIT_SHIFT_READ	1
#define ICCOMIICR_BIT_SHIFT_DATA	4
#define ICCOMIICR_BIT_SHIFT_CMD		6

#define ICCOMEICR_INIT			0x00000000
#define ICCOMEICR_INT			0x00000001
#define ICCOMEICR_WRITE			0x00000002
#define ICCOMEICR_INIT_COMP		0x00000004
#define ICCOMEICR_DATA_TOP		0x00000010
#define ICCOMEICR_DATA_MID		0x00000000
#define ICCOMEICR_DATA_BTM		0x00000020
#define ICCOMEICR_DATA_ONLY		0x00000030
#define ICCOMEICR_CMD1			0x00000000
#define ICCOMEICR_CMD2			0x00000040
#define ICCOMEICR_FATAL			0x00000080

#define ICCOMEICR_DATA_BIT		ICCOMEICR_DATA_ONLY
#define ICCOMEICR_CMD_BIT		ICCOMEICR_CMD2

#define ICCOMEICR_BIT_SHIFT_WRITE   1
#define ICCOMEICR_BIT_SHIFT_INIT	2
#define ICCOMEICR_BIT_SHIFT_DATA	4
#define ICCOMEICR_BIT_SHIFT_CMD		6
#define ICCOMEICR_BIT_SHIFT_FATAL   7

#define ICCOMCMDR_INIT			0x00000000

#define ICCOMCSR_INIT			0x00000080
#define ICCOMCSR_CMD1			0x00000001
#define ICCOMCSR_CMD2			0x00000002

/* MFIS */
#ifdef RMU2
#define MFIS_BASE_ADDR  IO_ADDRESS(0xE6260000)

#define MFIS_GSR        (MFIS_BASE_ADDR + 0x00000004)
#define MFIS_IICR       (MFIS_BASE_ADDR + 0x00000010)
#define MFIS_EICR       (MFIS_BASE_ADDR + 0x00000014)
#define MFIS_SM4        (MFIS_BASE_ADDR + 0x00000080)
#else
#define MFIS_BASE_ADDR  IO_ADDRESS(0xE6A60000)

#define MFIS_IDX		(MFIS_BASE_ADDR + 0x00000000)
#define MFIS_GSR		(MFIS_BASE_ADDR + 0x00000004)
#define MFIS_SCR		(MFIS_BASE_ADDR + 0x00000008)
#define MFIS_MCR		(MFIS_BASE_ADDR + 0x0000000C)
#define MFIS_IICR		(MFIS_BASE_ADDR + 0x00000010)
#define MFIS_EICR		(MFIS_BASE_ADDR + 0x00000014)
#define MFIS_ADR		(MFIS_BASE_ADDR + 0x00000018)
#define MFIS_DATA		(MFIS_BASE_ADDR + 0x0000001C)
#define MFIS_SM4		(MFIS_BASE_ADDR + 0x00000080)
#endif
/* MFIS register macro */
#define RD_ICCOMGSR()   readl(MFIS_GSR)
#define WT_ICCOMGSR(x)  writel(x, MFIS_GSR)
#ifndef RMU2
#define RD_ICCOMSCR()   readl(MFIS_SCR)
#define WT_ICCOMSCR(x)  writel(x, MFIS_SCR)
#endif
#define RD_ICCOMIICR()  readl(MFIS_IICR)
#define WT_ICCOMIICR(x) writel(x, MFIS_IICR)
#define RD_ICCOMEICR()  readl(MFIS_EICR)
#define WT_ICCOMEICR(x) writel(x, MFIS_EICR)
#define RD_ICCOMCSR()   readl(MFIS_SM4)
#define WT_ICCOMCSR(x)  writel(x, MFIS_SM4)

#endif  /* __ICCOM_HW_H__ */
