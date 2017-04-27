/*
 * iccom_drv_standby_private.h
 *     Inter Core Communication standby private header file.
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
#ifndef __ICCOM_DRV_STANDBY_PRIVATE_H__
#define __ICCOM_DRV_STANDBY_PRIVATE_H__

#include <linux/io.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/ioctl.h>

#define RTCTL_STS_STANDBY                   (0x0)
#define RTCTL_STS_ACTIVE                    (0x1)
#define RTCTL_STS_RTBOOT                    (0x2)
#define RTCTL_STS_CHANGING_ACTIVE           (0x3)
#define RTCTL_STS_CHANGING_STANDBY          (0x4)
#define RTCTL_STS_FATAL_ERROR               (0x5)

/* SYSC */
#define RTCTL_A3R_STATE						(0x2000)		/* PSTR(power domain area to multimedia IPs) */
#define RTCTL_RT_CLOCK_STOP					(0x200)			/* RESCNT */
/* CPG */
#define RTCTL_RT_MOD_STPSR0_SET				(0xE0420000)	/* RMSTPCR0 */
#define RTCTL_RT_MOD_STPSR2_SET				(0x00002000)	/* RMSTPCR2 */
/* MFIS common */
#define MFIS_INIT_REG						(0xFFFFFFFF)
/* MFIS(GSR) */
#define MFISGSR_REQ_COMP					(0x00000001)
#define MFISGSR_STANDBY_NG					(0x10)
#define MFISGSR_STANDBY_RECONITION_FLAG		(0x80)
#define MFISGSR_RECOGNITION					(0x80)
/* MFIS(IICR) */
#define MFISIICR_INIT						(0x00000010)
#define MFISIICR_RTACTIVE					(0x00000020)

#define MAX_POLLING_CNT						(60000)

#define MSLEEP_TIM_VALUE					(1)
#define UDELAY_TIM_VALUE					(100)

#define reg_modify32(clrmask, setmask, addr) \
	writel(((readl(addr) & ~(clrmask)) | (setmask)), addr)

extern void iccom_rtctl_standby_ng(void);
extern void iccom_rtctl_standby_ng_cancel(void);
extern int iccom_rtctl_set_lcd_status(void *handle, int lcd_status);
extern int iccom_rtctl_watch_rt_state(void);
extern int iccom_rtctl_change_rt_state_active(void);
extern int rtctl_change_rt_state_standby(void);
extern int rtctl_change_rt_state_active(void);
extern void rtctl_intcs_mask(void);
extern void rtctl_intcs_mask_in_active(void);
extern void rtctl_mfis_initialize(void);
extern int iccom_rtctl_wait_rt_state_active(void);
extern void iccom_rtctl_set_rt_state(void);
extern int iccom_rtctl_initilize(void);
extern void iccom_rtctl_finalize(void);
extern int rtctl_initilize(void);
extern void iccom_rtctl_set_rt_fatal_error(void);
extern int iccom_rtctl_get_state(void);
extern int iccom_rtctl_before_send_cmd_check_standby(int function_id);
extern void iccom_rtctl_after_send_cmd_check_standby(int function_id);
extern int iccom_rtctl_ioctl_standby_ng_cancel(void);
extern int iccom_rtctl_ioctl_standby_ng(void);
extern int iccom_rtctl_check_interrupt_active(void);
extern bool rtctl_check_standby_enable(void);

#endif /* __ICCOM_DRV_STANDBY_PRIVATE_H__ */
