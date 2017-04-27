/*
 * arch/arm/mach-shmobile/include/mach/rmc_ramdump.h
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
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

#ifndef __RMC_RAMDUMP_H__
#define __RMC_RAMDUMP_H__

#include <linux/types.h>
#include <linux/init.h>
#include <mach/pm.h>
#include "../../pmRegisterDef.h"

enum hw_register_width {
	HW_REG_8BIT	= 1,
	HW_REG_16BIT	= 2,
	HW_REG_32BIT	= 4,
};
struct hw_register_range {
	phys_addr_t start;
	/* End address is included in the reading range */
	phys_addr_t end;
	enum hw_register_width width;
	/* todo: at the moment addresses are incremented by 4 bytes
	 * but int he future there might be need to increment by byte. */
	unsigned int inc;
	/* Power area which needs to be on before reading registers
	 * This is PTSR register bit mask */
	unsigned int pa;
	/* This one of the module stop registers */
	void __iomem *msr;
	/* This is module stop register bit mask */
	unsigned int msb;
};

/* can we find these from some header? */
#define MSTPST525 (1 << 25)
#define MSTO007 (1 << 7)


/* Hardware-Register info */
struct	hw_register_dump {
	/* -----	SBSC1 ----- */
	ulong		SBSC_Setting_00[72];
	ulong		SBSC_Setting_01[17];
	ulong		SBSC_Mon_Setting;
	ulong		SBSC_PHY_Setting_00[2];
	ulong		SBSC_PHY_Setting_01;
	ulong		SBSC_PHY_Setting_02[28];
	/* -----	CPG ----- */
	ulong		CPG_Setting_00[129];
	ulong		CPG_Setting_01[97];
	/* -----	RWDT ----- */
	ulong		RWDT_Condition[3];
	/* -----	SWDT ----- */
	ulong		SWDT_Condition[3];
	/* -----	Secure Up-time Clock ----- */
	ulong		SUTC_Condition[4];
	/* -----	CMT15 ----- */
	ulong		CMT15_Condition[4];
	/* -----	SYSC ----- */
	ulong		SYSC_Setting_00_0[32];
	ulong		SYSCPSTR;
	ulong		SYSC_Setting_00_1[31];
	ulong		SYSC_Setting_01[32];
	ulong		SYSC_Rescnt[3];
	/* -----	DBG ----- */
	ulong		DBG_Setting[3];
	/* -----	GIC ----- */
	ulong		GIC_Setting[2];
	/* -----	L2C-310 ----- */
	ulong		PL310_Setting[2];
	/* -----	INTC-SYS ----- */
	ulong		INTC_SYS_Info[4];
	/* -----	INTC-BB ----- */
	ulong		INTC_BB_Info[2];
	/* -----	ICC0 ----- */
	ulong		IIC0_Setting[3];
	/* -----	ICCB ----- */
	ulong		IICB_Setting[3];
	/* -----	IPMMU ----- */
	ulong		IPMMU_Setting[64];
};

struct ramdump_plat_data {
	unsigned long reg_dump_base;
	size_t reg_dump_size;
	/* this is the size for each core */
	size_t core_reg_dump_size;
	u32 num_resources;
	struct hw_register_range *hw_register_range;
	struct map_desc *io_desc;
	u32 io_desc_size;
};



#endif /*__RMC_RAMDUMP_H__*/
