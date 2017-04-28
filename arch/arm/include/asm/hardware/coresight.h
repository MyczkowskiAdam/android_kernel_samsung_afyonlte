/*
 * linux/arch/arm/include/asm/hardware/coresight.h
 *
 * CoreSight components' registers
 *
 * Copyright (C) 2009 Nokia Corporation.
 * Alexander Shishkin
 *
 * Copyright (C) 2012 Renesas Mobile.
 * Philippe Gobaille
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_HARDWARE_CORESIGHT_H
#define __ASM_HARDWARE_CORESIGHT_H

#define TRACER_ACCESSED_BIT	0
#define TRACER_RUNNING_BIT	1
#define TRACER_CYCLE_ACC_BIT	2
#define TRACER_TRACE_DATA_BIT	3
#define TRACER_TIMESTAMP_BIT	4
#define TRACER_BRANCHOUTPUT_BIT	5
#define TRACER_RETURN_STACK_BIT	6
#define TRACER_ACCESSED		BIT(TRACER_ACCESSED_BIT)
#define TRACER_RUNNING		BIT(TRACER_RUNNING_BIT)
#define TRACER_CYCLE_ACC	BIT(TRACER_CYCLE_ACC_BIT)
#define TRACER_TRACE_DATA	BIT(TRACER_TRACE_DATA_BIT)
#define TRACER_TIMESTAMP	BIT(TRACER_TIMESTAMP_BIT)
#define TRACER_BRANCHOUTPUT	BIT(TRACER_BRANCHOUTPUT_BIT)
#define TRACER_RETURN_STACK	BIT(TRACER_RETURN_STACK_BIT)

#define TRACER_TIMEOUT 10000

#define etm_writel(t, id, v, x) \
	(__raw_writel((v), (t)->etm_regs[(id)] + (x)))
#define etm_readl(t, id, x) (__raw_readl((t)->etm_regs[(id)] + (x)))

/* CoreSight Management Registers */
#define CSMR_LOCKACCESS 0xfb0
#define CSMR_LOCKSTATUS 0xfb4
#define CSMR_AUTHSTATUS 0xfb8
#define CSMR_DEVID	0xfc8
#define CSMR_DEVTYPE	0xfcc
/* CoreSight Component Registers */
#define CSCR_CLASS	0xff4

#define UNLOCK_MAGIC	0xc5acce55

/* ETM control register, "ETM Architecture", 3.3.1 */
#define ETMR_CTRL		0
#define ETMCTRL_POWERDOWN	1
#define ETMCTRL_PROGRAM		(1 << 10)
#define ETMCTRL_PORTSEL		(1 << 11)
#define ETMCTRL_CONTEXTIDSIZE(x) (((x) & 3) << 14)
#define ETMCTRL_PORTMASK1	(7 << 4)
#define ETMCTRL_PORTMASK2	(1 << 21)
#define ETMCTRL_PORTMASK	(ETMCTRL_PORTMASK1 | ETMCTRL_PORTMASK2)
#define ETMCTRL_PORTSIZE(x) ((((x) & 7) << 4) | (!!((x) & 8)) << 21)
#define ETMCTRL_DO_CPRT		(1 << 1)
#define ETMCTRL_DATAMASK	(3 << 2)
#define ETMCTRL_DATA_DO_DATA	(1 << 2)
#define ETMCTRL_DATA_DO_ADDR	(1 << 3)
#define ETMCTRL_DATA_DO_BOTH	(ETMCTRL_DATA_DO_DATA | ETMCTRL_DATA_DO_ADDR)
#define ETMCTRL_BRANCH_OUTPUT	(1 << 8)
#define ETMCTRL_CYCLEACCURATE	(1 << 12)
#define ETMCTRL_TIMESTAMP_EN	(1 << 28)
#define ETMCTRL_RETURN_STACK_EN	(1 << 29)

/* ETM configuration code register */
#define ETMR_CONFCODE		(0x04)
#define ETMCCR_ETMIDR_PRESENT	BIT(31)

/* ETM trace start/stop resource control register */
#define ETMR_TRACESSCTRL	(0x18)

/* ETM trigger event register */
#define ETMR_TRIGEVT		(0x08)

/* address access type register bits, "ETM architecture",
 * table 3-27 */
/* - access type */
#define ETMAAT_IFETCH		0
#define ETMAAT_IEXEC		1
#define ETMAAT_IEXECPASS	2
#define ETMAAT_IEXECFAIL	3
#define ETMAAT_DLOADSTORE	4
#define ETMAAT_DLOAD		5
#define ETMAAT_DSTORE		6
/* - comparison access size */
#define ETMAAT_JAVA		(0 << 3)
#define ETMAAT_THUMB		(1 << 3)
#define ETMAAT_ARM		(3 << 3)
/* - data value comparison control */
#define ETMAAT_NOVALCMP		(0 << 5)
#define ETMAAT_VALMATCH		(1 << 5)
#define ETMAAT_VALNOMATCH	(3 << 5)
/* - exact match */
#define ETMAAT_EXACTMATCH	(1 << 7)
/* - context id comparator control */
#define ETMAAT_IGNCONTEXTID	(0 << 8)
#define ETMAAT_VALUE1		(1 << 8)
#define ETMAAT_VALUE2		(2 << 8)
#define ETMAAT_VALUE3		(3 << 8)
/* - security level control */
#define ETMAAT_IGNSECURITY	(0 << 10)
#define ETMAAT_NSONLY		(1 << 10)
#define ETMAAT_SONLY		(2 << 10)

#define ETMR_COMP_VAL(x)	(0x40 + (x) * 4)
#define ETMR_COMP_ACC_TYPE(x)	(0x80 + (x) * 4)

/* ETM status register, "ETM Architecture", 3.3.2 */
#define ETMR_STATUS		(0x10)
#define ETMST_OVERFLOW		BIT(0)
#define ETMST_PROGBIT		BIT(1)
#define ETMST_STARTSTOP		BIT(2)
#define ETMST_TRIGGER		BIT(3)

#define etm_progbit(t)		(etm_readl((t), ETMR_STATUS) & ETMST_PROGBIT)
#define etm_started(t)		(etm_readl((t), ETMR_STATUS) & ETMST_STARTSTOP)
#define etm_triggered(t)	(etm_readl((t), ETMR_STATUS) & ETMST_TRIGGER)

#define ETMR_TRACEENCTRL2	0x1c
#define ETMR_TRACEENCTRL	0x24
#define ETMTE_INCLEXCL		BIT(24)
#define ETMR_TRACEENEVT		0x20

#define ETMR_VIEWDATAEVT	0x30
#define ETMR_VIEWDATACTRL1	0x34
#define ETMR_VIEWDATACTRL2	0x38
#define ETMR_VIEWDATACTRL3	0x3c
#define ETMVDC3_EXCLONLY	BIT(16)

#define ETMCTRL_OPTS		(ETMCTRL_DO_CPRT)

#define ETMR_ID			0x1e4
#define ETMIDR_VERSION(x)	(((x) >> 4) & 0xff)
#define ETMIDR_VERSION_3_1	0x21
#define ETMIDR_VERSION_PFT_1_0	0x30

#define ETMR_CCE		0x1e8
#define ETMCCER_RETURN_STACK_IMPLEMENTED	BIT(23)
#define ETMCCER_TIMESTAMPING_IMPLEMENTED	BIT(22)

#define ETMR_TRACEIDR		0x200

/* ETM management registers, "ETM Architecture", 3.5.24 */
#define ETMMR_OSLAR	0x300
#define ETMMR_OSLSR	0x304
#define ETMMR_OSSRR	0x308
#define ETMMR_PDSR	0x314

/* ETB registers, "CoreSight Components TRM", 9.3 */
#define ETBR_DEPTH		0x04
#define ETBR_STATUS		0x0c
#define ETBR_READMEM		0x10
#define ETBR_READADDR		0x14
#define ETBR_WRITEADDR		0x18
#define ETBR_TRIGGERCOUNT	0x1c
#define ETBR_CTRL		0x20
#define ETBR_FORMATTERCTRL	0x304
#define ETBFF_ENFTC		1
#define ETBFF_ENFCONT		BIT(1)
#define ETBFF_FONFLIN		BIT(4)
#define ETBFF_MANUAL_FLUSH	BIT(6)
#define ETBFF_TRIGIN		BIT(8)
#define ETBFF_TRIGEVT		BIT(9)
#define ETBFF_TRIGFL		BIT(10)
#define ETBFF_STOPFL		BIT(12)

#define etb_writel(t, v, x) \
	(__raw_writel((v), (t)->etb_regs + (x)))
#define etb_readl(t, x) (__raw_readl((t)->etb_regs + (x)))

#define etm_lock(t, id) \
	do { etm_writel((t), (id), 0, CSMR_LOCKACCESS); } while (0)
#define etm_unlock(t, id) \
	do { etm_writel((t), (id), UNLOCK_MAGIC, CSMR_LOCKACCESS); } while (0)

#define etb_lock(t) do { etb_writel((t), 0, CSMR_LOCKACCESS); } while (0)
#define etb_unlock(t) \
	do { etb_writel((t), UNLOCK_MAGIC, CSMR_LOCKACCESS); } while (0)

/* CoreSight System Trace Macrocell TRM ARM DDI 0444B */
#define STM_G_DMTS	0x00	/* data, marked with timestamp, guaranteed */
#define STM_G_DM	0x08	/* data, marked, guaranteed */
#define STM_G_DTS	0x10	/* data, timestamp, guaranteed */
#define STM_G_D		0x18	/* data, guaranteed */
#define STM_G_FLAGTS	0x60	/* flag with timestamp, guaranteed */
#define STM_G_FLAG	0x68	/* flag, guaranteed */
#define STM_G_TRIGTS	0x70	/* trigger with timestamp, guaranteed */
#define STM_G_TRIG	0x78	/* trigger, guaranteed */
#define STM_I_DMTS	0x80	/* data, marked with timestamp, invariant */
#define STM_I_DM	0x88	/* data, marked, invariant */
#define STM_I_DTS	0x90	/* data, timestamp, invariant */
#define STM_I_D		0x98	/* data, invariant */
#define STM_I_FLAGTS 	0xE0	/* flag with timestamp, invariant */
#define STM_I_FLAG	0xE8	/* flag, invariant */
#define STM_I_TRIGTS 	0xF0	/* trigger with timestamp, invariant */
#define STM_I_TRIG	0xF8	/* trigger, invariant */
#define STM_PORT_SIZE	0x100

#define STM_SPER	0xE00	/* Stimulus Port Enable */
#define STM_SPTER	0xE20	/* Stimulus Port Trigger Enable */
#define STM_PRIVMASKR	0xE40	/* Trace Privilege */
#define STM_SPSCR	0xE60	/* Stimulus Port Select Configuration */
#define STM_SPMSCR	0xE64	/* Stimulus Port Master Select Configuration */
#define STM_SPOVERRIDER	0xE68	/* Stimulus Port Override */
#define STM_SPMOVERRIDER 0xE6C	/* Stimulus Port Master Override */
#define STM_SPTRIGCSR	0xE70	/* Stimulus Port Trigger Control and Status */
#define STM_TCSR	0xE80	/* Trace Control and Status */
#define STM_TCSR_EN	1
#define STM_TCSR_TSEN	(1 << 1)
#define STM_TCSR_SYNCEN	(1 << 2)
#define STM_TCSR_BUSY	(1 << 23)
#define STM_TCSR_TRACEID_MSK (~(0x7F << 16))
#define STM_TSSTIMR	0xE84	/* Timestamp Stimulus */
#define STM_TSFREQR	0xE8C	/* Timestamp Frequency */
#define STM_SYNCR	0xE90	/* Synchronization Control */
#define STM_AUXCR	0xE94	/* Auxiliary Control */

/* CoreSight Trace Memory Controller, ARM DDI 0461B */
#define TMC_RSZ		0x004	/* RAM Size */
#define TMC_STS		0x00C	/* Status */
#define TMC_STS_RDY	(1 << 2)

#define TMC_RRD		0x010	/* RAM Read Data */
#define TMC_RRP		0x014	/* RAM Read Pointer */
#define TMC_RWP		0x018	/* RAM Write Pointer */
#define TMC_TRG		0x01C	/* Trigger Counter */
#define TMC_CTL		0x020	/* Control */
#define TMC_CTL_EN	1

#define TMC_RWD		0x024	/* RAM Write Data */
#define TMC_MODE	0x028	/* Mode */
#define TMC_MODE_FIFOHW	2
#define TMC_MODE_FIFOSW	1
#define TMC_MODE_CIRC	0

#define TMC_LBUFLEVEL	0x02C	/* Latched Buffer Fill Level */
#define TMC_CBUFLEVEL	0x030	/* Current Buffer Fill Level */
#define TMC_BUFWM	0x034	/* Buffer Level Water Mark */
#define TMC_RRPHI	0x038	/* RAM Read Pointer High */
#define TMC_RWPHI	0x03C	/* RAM Write Pointer High */
#define TMC_AXICTL	0x110	/* AXI Control */
#define	TMC_AXI_BURST_BIT	8
#define	TMC_AXI_SGT_BIT		7
#define	TMC_AXI_WR_ALLOC_BIT	5
#define	TMC_AXI_RD_ALLOC_BIT	4
#define	TMC_AXI_CACHEABLE_BIT	3
#define	TMC_AXI_BUFFERABLE_BIT	2
#define	TMC_AXI_NOSECU_BIT	1
#define	TMC_AXI_PRIVILEGED_BIT	0

#define TMC_DBALO	0x118	/* Data Buffer Address Low */
#define TMC_DBAHI	0x11C	/* Data Buffer Address High */
#define TMC_FFSR	0x300	/* Formatter and Flush Status */
#define TMC_FFCR	0x304	/* Formatter and Flush Control */
#define TMC_FFCR_ENFT	1
#define TMC_FFCR_ENTI	(1 << 1)
#define TMC_FFCR_FONFIIN	(1 << 4)
#define TMC_FFCR_FONTRIGEVT	(1 << 5)
#define TMC_FFCR_FLUSHMAN	(1 << 6)
#define TMC_FFCR_TRIGONTRIGIN	(1 << 8)
#define TMC_FFCR_TRIGONTRIGEVT	(1 << 9)
#define TMC_FFCR_TRIGONFI	(1 << 10)
#define TMC_FFCR_STOPONFI	(1 << 12)
#define TMC_FFCR_STOPONTRIGEVT	(1 << 13)
#define TMC_FFCR_DRAINBUF	(1 << 14)
#define TMC_PSCR	0x308	/* Periodic Synchronization Counter */
#define TMC_CLAIMSET	0xFA0	/* Claim Tag Set */
#define TMC_CLAIMCLR	0xFA4	/* Claim Tag Clear */

/* CoreSight Components Technical Reference Manual, ARM DDI 0314H */
#define CSTF_CTL	0x000	/* Funnel Control */
#define CSTF_PRIO	0x004	/* Priority Control */
#define CSTF_PRIO_SET(reg,port,pri) \
	do {__raw_writel((__raw_readl(reg + CSTF_PRIO) & ~(7 << ((port)*3))) | ((pri) << ((port)*3)),reg + CSTF_PRIO); } while (0)

#define CSTF_PORT_ENABLE(reg,port) \
	do {	unsigned int en = __raw_readl(reg + CSTF_CTL); \
		__raw_writel(en | (1 << (port)), reg + CSTF_CTL); } while (0)
#define CSTF_PORT_DISABLE(reg,port) \
	do {	unsigned int en = __raw_readl(reg + CSTF_CTL); \
		__raw_writel(en & ~(1 << (port)), reg + CSTF_CTL); } while (0)

/* Cross Trigger Interface */
#define	CTI_CONTROL	0x000	/* CTI Control */
#define CTI_CONTROL_GLBEN 1
#define	CTI_INTACK	0x010	/* CTI Interrupt Acknowledge */
#define	CTI_APPSET	0x014	/* CTI Application Trigger Set */
#define	CTI_APPCLEAR	0x018	/* CTI Application Trigger Clear */
#define	CTI_APPPULSE	0x01C	/* CTI Application Pulse */
#define	CTI_INEN0	0x020	/* CTI Trigger to Channel 0 Enable */
#define	CTI_INEN1	0x024	/* CTI Trigger to Channel 1 Enable */
#define	CTI_INEN2	0x028	/* CTI Trigger to Channel 2 Enable */
#define	CTI_INEN3	0x02C	/* CTI Trigger to Channel 3 Enable */
#define	CTI_INEN4	0x030	/* CTI Trigger to Channel 4 Enable */
#define	CTI_INEN5	0x034	/* CTI Trigger to Channel 5 Enable */
#define	CTI_INEN6	0x038	/* CTI Trigger to Channel 6 Enable */
#define	CTI_INEN7	0x03C	/* CTI Trigger to Channel 7 Enable */
#define	CTI_OUTEN0	0x0A0	/* CTI Trigger to Channel 0 Enable */
#define	CTI_OUTEN1	0x0A4	/* CTI Trigger to Channel 1 Enable */
#define	CTI_OUTEN2	0x0A8	/* CTI Trigger to Channel 2 Enable */
#define	CTI_OUTEN3	0x0AC	/* CTI Trigger to Channel 3 Enable */
#define	CTI_OUTEN4	0x0B0	/* CTI Trigger to Channel 4 Enable */
#define	CTI_OUTEN5	0x0B4	/* CTI Trigger to Channel 5 Enable */
#define	CTI_OUTEN6	0x0B8	/* CTI Trigger to Channel 6 Enable */
#define	CTI_OUTEN7	0x0BC	/* CTI Trigger to Channel 7 Enable */
#define	CTI_TRIGINSTATUS  0x130	/* CTI Trigger In Status */
#define	CTI_TRIGOUTSTATUS 0x134	/* CTI Trigger Out Status */
#define	CTI_CHINSTATUS	0x138	/* CTI Channel In Status */
#define	CTI_CHOUTSTATUS	0x13C	/* CTI Channel Out Status */
#define	CTI_GATE	0x140	/* CTI Channel Gate */
#define	CTI_ASICCTL	0x144	/* External Multiplexor Control */
#define	CTI_ITCHINACK	0xEDC
#define	CTI_ITTRIGINACK	0xEE0
#define	CTI_ITCHOUT	0xEE4
#define	CTI_ITTRIGOUT	0xEE8
#define	CTI_ITCHOUTACK	0xEEC
#define	CTI_ITTRIGOUTACK 0xEF0
#define	CTI_ITCHIN	0xEF4
#define	CTI_ITTRIGIN	0xEF8

#endif /* __ASM_HARDWARE_CORESIGHT_H */

