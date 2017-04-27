/*
 * iccom_drv_standby_main.c
 *    Inter Core Communication Standby Main function file.
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

#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irqreturn.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/mfis.h>
#include <linux/jiffies.h>

#include <mach/r8a7373.h>

#include <screen_display.h>

#include "log_kernel.h"
#include "iccom_drv.h"
#include "iccom_drv_id.h"
#include "iccom_hw.h"
#include "iccom_drv_common.h"
#include "iccom_drv_private.h"
#include "iccom_drv_standby.h"
#include "iccom_drv_standby_private.h"



static int count_standby_ng;
static struct semaphore semaphore_count_standby_ng;
static struct semaphore semaphore_standby_flag;
static int status_lcd_now;
static unsigned long standby_flag;
static int status_rt_now;
static void *display_handle;

static bool rtctl_check_rt_recognize_standby(void);
static bool rtctl_check_rt_confirm_standby(void);
static bool rtctl_check_rt_start(void);
static bool rtctl_check_rt_active(void);

static void rtctl_boot_timeout_error(void);
static void rtctl_change_active_timeout_error(void);
static void rtctl_change_standby_timeout_error(void);

#define INT_MFIS (32+126)

spinlock_t spin_lock_hpb;

#define RTCTL_INTCRT_BASE2		(0xFFD20000)
#define RTCTL_INTCRT_SIZE2		(0x200)
static unsigned long rtctl_intcrt_base2;
#define RTCTL_INTCRT_IMR0S		(rtctl_intcrt_base2 + 0x0080)
#define RTCTL_INTCRT_IMR12S		(rtctl_intcrt_base2 + 0x00B0)
#define RTCTL_INTCRT_IMR0SA		(rtctl_intcrt_base2 + 0x0180)
#define RTCTL_INTCRT_IMR12SA	(rtctl_intcrt_base2 + 0x01B0)
#define RTCTL_INTCRT_BASE5		(0xFFD50000)
#define RTCTL_INTCRT_SIZE5		(0x200)
static unsigned long rtctl_intcrt_base5;
#define RTCTL_INTCRT_IMR0S3		(rtctl_intcrt_base5 + 0x0080)
#define RTCTL_INTCRT_IMR12S3	(rtctl_intcrt_base5 + 0x00B0)
#define RTCTL_INTCRT_IMR0SA3	(rtctl_intcrt_base5 + 0x0180)
#define RTCTL_INTCRT_IMR12SA3	(rtctl_intcrt_base5 + 0x01B0)


/******************************************************************************/
/* Function   : iccom_rtctl_standby_ng                                        */
/* Description:                                                               */
/******************************************************************************/
void iccom_rtctl_standby_ng(void)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	ICCOM_DOWN_TIMEOUT(&semaphore_count_standby_ng);
	count_standby_ng++;
	MSG_MED("[ICCOMK]   |[%s] : count_standby_ng = %d\n",
			__func__, count_standby_ng);
	up(&semaphore_count_standby_ng);

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_rtctl_standby_ng_cancel                                 */
/* Description:                                                               */
/******************************************************************************/
void iccom_rtctl_standby_ng_cancel(void)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	ICCOM_DOWN_TIMEOUT(&semaphore_count_standby_ng);
	count_standby_ng--;
	MSG_MED("[ICCOMK]   |[%s] : count_standby_ng = %d\n",
			 __func__, count_standby_ng);
	if (0 > count_standby_ng) {
		MSG_HIGH("[ICCOMK]OUT|[%s] count_standby_ng = %d\n",
			 __func__, count_standby_ng);
		panic("count_standby_ng = (%d) Error\n", count_standby_ng);
	}

	up(&semaphore_count_standby_ng);
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_rtctl_set_lcd_status                                    */
/* Description:                                                               */
/******************************************************************************/
int iccom_rtctl_set_lcd_status(
	void	*handle,
	int		lcd_status
){
	MSG_MED("[ICCOMK]IN |[%s] : lcd_status = (%d)\n", __func__, lcd_status);

	ICCOM_DOWN_TIMEOUT(&semaphore_count_standby_ng);
	if (status_lcd_now == lcd_status) {
		up(&semaphore_count_standby_ng);
		MSG_HIGH("[ICCOMK]OUT|[%s] : ret = (%d)\n", __func__, SMAP_OK);
		return SMAP_OK;
	}
	status_lcd_now = lcd_status;
	MSG_MED("[ICCOMK]   |[%s] : status_lcd_now = %d\n", __func__,
				status_lcd_now);
	up(&semaphore_count_standby_ng);
	MSG_MED("[ICCOMK]OUT|[%s] : ret = (%d)\n", __func__, SMAP_OK);
	return SMAP_OK;
}

/******************************************************************************/
/* Function   : iccom_rtctl_watch_rt_state                                    */
/* Description:                                                               */
/******************************************************************************/
int iccom_rtctl_watch_rt_state(void)
{
	int ret_code = 0;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	ICCOM_DOWN_TIMEOUT(&semaphore_standby_flag);
	if (standby_flag == 1) {
		MSG_HIGH("[ICCOMK]OUT|[%s] : ret = (%d)\n", __func__,
				SMAP_LIB_STCON_BUSY);
		up(&semaphore_standby_flag);
		return SMAP_LIB_STCON_BUSY;
	}

	standby_flag = 1;
	MSG_MED("[ICCOMK]   |[%s] : standby_flag = %d\n", __func__,
				(u32)standby_flag);
	up(&semaphore_standby_flag);

	if (RTCTL_STS_STANDBY  == status_rt_now) {
		ICCOM_DOWN_TIMEOUT(&semaphore_standby_flag);
		standby_flag = 0;
		MSG_MED("[ICCOMK]   |[%s] : standby_flag = %d\n", __func__,
				(u32)standby_flag);
		up(&semaphore_standby_flag);
		MSG_MED("[ICCOMK]OUT|[%s] : ret = (%d)\n", __func__, SMAP_OK);
		return SMAP_OK;
	}

	ret_code = rtctl_change_rt_state_standby();
	MSG_MED("[ICCOMK]   |[%s] : rtctl_change_rt_state_standby = %d\n",
		 __func__, ret_code);

	ICCOM_DOWN_TIMEOUT(&semaphore_standby_flag);
	standby_flag = 0;
	MSG_MED("[ICCOMK]   |[%s] : standby_flag = %d\n", __func__,
				(u32)standby_flag);
	up(&semaphore_standby_flag);
	MSG_MED("[ICCOMK]OUT|[%s] : ret_code = (%d)\n", __func__, ret_code);
	return ret_code;
}

/******************************************************************************/
/* Function   : iccom_rtctl_change_rt_state_active                            */
/* Description:                                                               */
/******************************************************************************/
int iccom_rtctl_change_rt_state_active(
	void
){
	int ret_code = SMAP_OK;
	int lcd_ret = SMAP_OK;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_set_lcd_refresh lcd_refresh;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	ICCOM_DOWN_TIMEOUT(&semaphore_count_standby_ng);
	if (RTCTL_STS_RTBOOT == status_rt_now) {
		up(&semaphore_count_standby_ng);
		MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__,
				status_rt_now);

		status_rt_now = RTCTL_STS_ACTIVE;
		MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__, status_rt_now);
		disp_stop_lcd.handle = display_handle;
		disp_stop_lcd.output_mode = RT_DISPLAY_LCD1;

		lcd_ret = screen_display_stop_lcd(&disp_stop_lcd);
		MSG_HIGH("[ICCOMK]OUT|[%s] lcd_ret = (%d)\n",
			 __func__, lcd_ret);
		MSG_HIGH("[ICCOMK]OUT|[%s] ret = (%d)\n",
			 __func__, SMAP_OK);
		return ret_code;
	}
	up(&semaphore_count_standby_ng);

	ICCOM_DOWN_TIMEOUT(&semaphore_standby_flag);
	if (standby_flag == 1) {
		MSG_HIGH("[ICCOMK]OUT|[%s] ret = (%d)\n",
			 __func__, SMAP_LIB_STCON_BUSY);
		up(&semaphore_standby_flag);
		return SMAP_LIB_STCON_BUSY;
	}

	standby_flag = 1;
	MSG_MED("[ICCOMK]   |[%s] : standby_flag = %d\n", __func__,
					(u32)standby_flag);
	up(&semaphore_standby_flag);

	if (RTCTL_STS_ACTIVE == status_rt_now) {
		ICCOM_DOWN_TIMEOUT(&semaphore_standby_flag);
		standby_flag = 0;
		MSG_MED("[ICCOMK]   |[%s] : standby_flag = %d\n", __func__,
					(u32)standby_flag);
		up(&semaphore_standby_flag);
		MSG_MED("[ICCOMK]OUT|[%s] ret = (%d)\n", __func__, SMAP_OK);
		return SMAP_OK;
	}

	ret_code = rtctl_change_rt_state_active();

	ICCOM_DOWN_TIMEOUT(&semaphore_count_standby_ng);
	MSG_MED("[ICCOMK]   |[%s] : status_lcd_now = %d\n",
			__func__, status_lcd_now);
	if (status_lcd_now == ICCOM_DRV_STATE_LCD_REFRESH) {
		up(&semaphore_count_standby_ng);

		lcd_refresh.handle = display_handle;
		lcd_refresh.output_mode  = RT_DISPLAY_LCD1;
		lcd_refresh.refresh_mode = RT_DISPLAY_REFRESH_OFF;

		lcd_ret = screen_display_set_lcd_refresh(&lcd_refresh);
		MSG_MED("[ICCOMK]   |[%s] : disp_set_lcd_refresh_mode = %d\n",
			__func__, lcd_ret);
	} else {
		up(&semaphore_count_standby_ng);
	}

	ICCOM_DOWN_TIMEOUT(&semaphore_standby_flag);
	standby_flag = 0;
	MSG_MED("[ICCOMK]   |[%s] : standby_flag = %d\n", __func__,
					(u32)standby_flag);
	up(&semaphore_standby_flag);
	MSG_MED("[ICCOMK]OUT|[%s] ret_code = (%d)\n", __func__, ret_code);
	return ret_code;
}

/******************************************************************************/
/* Function   : rtctl_change_rt_state_standby                                 */
/* Description:                                                               */
/******************************************************************************/
int rtctl_change_rt_state_standby(void)
{
	bool recognize_standby = false;
	bool check_standby = false;
	int recognize_cnt;
	int check_cnt;
	unsigned long  reg_mstpsr = 0;
	unsigned long hpb_lock_flags;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	status_rt_now = RTCTL_STS_CHANGING_STANDBY;
	MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__, status_rt_now);

	recognize_cnt = MAX_POLLING_CNT;
	check_cnt = MAX_POLLING_CNT;
	do {
		recognize_standby = rtctl_check_rt_recognize_standby();
		MSG_MED("[ICCOMK]   |[%s] : recognize_standby = %d\n", __func__,
					recognize_standby);
			if (recognize_standby)
				break;

		udelay(UDELAY_TIM_VALUE);
		recognize_cnt--;
		MSG_MED("[ICCOMK]   |[%s] : recognize_cnt = %d\n", __func__ ,
						recognize_cnt);
	} while (recognize_cnt > 0);

	if (false == recognize_standby) {
		rtctl_change_standby_timeout_error();
	}

	reg_modify32(MFISGSR_RECOGNITION, 0, MFIS_GSR);
	MSG_MED("[ICCOMK]   |[%s] : MFIS_GSR = 0x%08x\n", __func__, readl(MFIS_GSR));

	do {
		check_standby = rtctl_check_rt_confirm_standby();
		MSG_MED("[ICCOMK]   |[%s] : check_standby=%d\n", __func__,
					check_standby);
			if (check_standby)
				break;

		udelay(UDELAY_TIM_VALUE);
		check_cnt--;
		MSG_MED("[ICCOMK]   |[%s] : check_cnt = %d\n", __func__ ,
					check_cnt);
	} while (check_cnt > 0);

	if (false == check_standby) {
		rtctl_change_standby_timeout_error();
	}
	switch (status_rt_now) {
	case RTCTL_STS_ACTIVE:
	case RTCTL_STS_CHANGING_ACTIVE:
		MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n",
			__func__, status_rt_now);
		MSG_HIGH("[ICCOMK]OUT|[%s] : ret = %d\n", __func__ ,
				SMAP_LIB_STCON_INUSE);
		return SMAP_LIB_STCON_INUSE;
	case RTCTL_STS_CHANGING_STANDBY:
		MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__,
				status_rt_now);
		break;
	default:
		MSG_HIGH("[ICCOMK]OUT|[%s] : status_rt_now = %d\n", __func__,
				status_rt_now);
		panic("%s : status_rt_now = %d\n", __func__, status_rt_now);
	}


	spin_lock_irqsave(&spin_lock_hpb, hpb_lock_flags);
	MSG_LOW("[ICCOMK]   |[%s] : spin_lock_irqsave\n", __func__);

	reg_modify32(0, RTCTL_RT_CLOCK_STOP, RESCNT);

	MSG_MED("[ICCOMK]   |[%s] : RESCNT = 0x%08x\n", __func__,
					readl(RESCNT));
	spin_unlock_irqrestore(&spin_lock_hpb, hpb_lock_flags);
	MSG_LOW("[ICCOMK]   |[%s] : spin_unlock_irqrestore\n", __func__);

	disable_irq(INT_MFIS);
	MSG_MED("[ICCOMK]   |[%s] : disable_irq\n", __func__);

	reg_modify32(0, RTCTL_RT_MOD_STPSR0_SET , RMSTPCR0);
	MSG_MED("[ICCOMK]   |[%s] : MSTPSR0 = 0x%08x\n", __func__ ,
				readl(MSTPSR0));
	readl(RMSTPCR0);
	readl(RMSTPCR0);
	reg_mstpsr = readl(MSTPSR0);
	reg_mstpsr = (reg_mstpsr & RTCTL_RT_MOD_STPSR0_SET);
	MSG_MED("[ICCOMK]   |[%s] : MSTPSR0 = 0x%08x\n", __func__,
				readl(MSTPSR0));

/*
	if (RTCTL_RT_MOD_STPSR0_SET != reg_mstpsr) {
		panic("rtctl_change_rt_state_standby error MSTPSR0 = 0x%08x\n",
			readl(MSTPSR0));
	}
 */

	reg_modify32(0, RTCTL_RT_MOD_STPSR2_SET , RMSTPCR2);
	MSG_MED("[ICCOMK]   |[%s] : MSTPSR2 = 0x%08x\n", __func__ ,
				readl(MSTPSR2));
	readl(RMSTPCR2);
	readl(RMSTPCR2);
	reg_mstpsr = readl(MSTPSR2);
	reg_mstpsr = (reg_mstpsr & RTCTL_RT_MOD_STPSR2_SET);
	MSG_MED("[ICCOMK]   |[%s] : MSTPSR2 = 0x%08x\n", __func__,
				readl(MSTPSR2));

/*
	if (RTCTL_RT_MOD_STPSR2_SET != reg_mstpsr) {
		panic("rtctl_change_rt_state_standby error MSTPSR2 = 0x%08x\n",
			readl(MSTPSR2));
	}
 */

	status_rt_now = RTCTL_STS_STANDBY ;
	MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__, status_rt_now);
	MSG_MED("[ICCOMK]OUT|[%s] ret = (%d)\n", __func__, SMAP_OK);
	return SMAP_OK;
}

/******************************************************************************/
/* Function   : rtctl_check_rt_recognize_standby                              */
/* Description:                                                               */
/******************************************************************************/
static bool rtctl_check_rt_recognize_standby()
{
	unsigned long reg_mfis = 0;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	reg_mfis = readl(MFIS_GSR);
	MSG_MED("[ICCOMK]   |[%s] : MFIS_GSR = 0x%08x\n", __func__,
			readl(MFIS_GSR));
	reg_mfis = (reg_mfis & MFISGSR_STANDBY_RECONITION_FLAG);
	if (0 != reg_mfis) {
		MSG_MED("[ICCOMK]OUT|[%s] : (%d)\n", __func__, 1);
		return true;
	} else {
		MSG_MED("[ICCOMK]OUT|[%s] : (%d)\n", __func__, 0);
		return false;
	}
}

/******************************************************************************/
/* Function   : rtctl_check_rt_confirm_standby                                */
/* Description:                                                               */
/******************************************************************************/
static bool rtctl_check_rt_confirm_standby()
{
	unsigned long reg_pstr = 0;
	unsigned long reg_mfis = 0;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	reg_pstr = readl(PSTR);
	MSG_MED("[ICCOMK]   |[%s] : PSTR = 0x%08x\n", __func__, readl(PSTR));
	reg_pstr = (reg_pstr & RTCTL_A3R_STATE);
	if (0 == reg_pstr) {
		MSG_MED("[ICCOMK]OUT|[%s] : (%d)\n", __func__, 1);
		return true;
	}

	reg_mfis = readl(MFIS_GSR);
	MSG_MED("[ICCOMK]   |[%s] : MFIS_GSR = %d\n", __func__, readl(MFIS_GSR));
	reg_mfis = (reg_mfis & MFISGSR_STANDBY_NG);
	if (0 != reg_mfis) {
		reg_modify32(MFISGSR_STANDBY_NG, 0 , MFIS_GSR);
		if (RTCTL_STS_CHANGING_STANDBY == status_rt_now) {
			MSG_MED("[ICCOMK]OUT|[%s] : (%d)\n", __func__, 1);
			status_rt_now = RTCTL_STS_ACTIVE;
			MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__,
					status_rt_now);
			return true;
		}
	}
	MSG_MED("[ICCOMK]OUT|[%s] : (%d)\n", __func__, 0);
	return false;
}

/******************************************************************************/
/* Function   : rtctl_change_rt_state_active                                  */
/* Description:                                                               */
/******************************************************************************/
int rtctl_change_rt_state_active(void)
{
	bool start_ret = false;
	bool active_ret = false;
	unsigned long start_loop = MAX_POLLING_CNT;
	unsigned long active_loop = MAX_POLLING_CNT;
	unsigned long hpb_lock_flags;
	unsigned long mfis_eicr;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	status_rt_now = RTCTL_STS_CHANGING_ACTIVE;
	MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__, status_rt_now);

	reg_modify32(RTCTL_RT_MOD_STPSR0_SET, 0, RMSTPCR0);
	readl(RMSTPCR0);
	readl(RMSTPCR0);
	MSG_MED("[ICCOMK]   |[%s] : CPGA_MSTPSR0 = 0x%08x\n", __func__,
				readl(MSTPSR0));

	reg_modify32(RTCTL_RT_MOD_STPSR2_SET, 0, RMSTPCR2);
	readl(RMSTPCR2);
	readl(RMSTPCR2);
	MSG_MED("[ICCOMK]   |[%s] : CPGA_MSTPSR2 = 0x%08x\n", __func__,
				readl(MSTPSR2));

	MSG_MED("[ICCOMK]   |[%s] : rtctl_intcs_mask_in_active()\n", __func__);
	rtctl_intcs_mask_in_active();
	MSG_MED("[ICCOMK]   |[%s] : rtctl_mfis_initialize()\n", __func__);
	rtctl_mfis_initialize();

	enable_irq(INT_MFIS);
	MSG_MED("[ICCOMK]   |[%s] : enable_irq\n", __func__);
	writel(MFISIICR_INIT, MFIS_IICR);
	MSG_MED("[ICCOMK]   |[%s] : MFIS_IICR = 0x%08x\n", __func__, readl(MFIS_IICR));

	spin_lock_irqsave(&spin_lock_hpb, hpb_lock_flags);
	MSG_LOW("[ICCOMK]   |[%s] : spin_lock_irqsave\n", __func__);

	reg_modify32(RTCTL_RT_CLOCK_STOP, 0 , RESCNT);
	MSG_MED("[ICCOMK]   |[%s] : RESCNT = 0x%08x\n", __func__,
			readl(RESCNT));
	spin_unlock_irqrestore(&spin_lock_hpb, hpb_lock_flags);
	MSG_LOW("[ICCOMK]   |[%s] : spin_unlock_irqrestore\n", __func__);

	do {
		start_ret = rtctl_check_rt_start();
		MSG_MED("[ICCOMK]   |[%s] : start_ret = %d\n", __func__,
				start_ret);
			if (start_ret)
				break;

		udelay(UDELAY_TIM_VALUE);
			start_loop--;
		MSG_MED("[ICCOMK]   |[%s] : start_loop = %d\n", __func__,
					(u32)start_loop);
	} while (start_loop > 0);

	if (false == start_ret) {
		rtctl_change_active_timeout_error();
	}

	{
		unsigned long long tim;
		unsigned long *addr_status;
		tim = local_clock();
		addr_status = (unsigned long *)g_iccom_command_area;
		addr_status[1] = (unsigned long)(tim>>32);
		addr_status[2] = (unsigned long)(tim & 0xFFFFFFFF);
	}

	writel(MFISGSR_REQ_COMP, MFIS_GSR);
	MSG_MED("[ICCOMK]   |[%s] : MFIS_GSR = 0x%08x\n", __func__, readl(MFIS_GSR));
	writel(MFISIICR_RTACTIVE, MFIS_IICR);
	MSG_MED("[ICCOMK]   |[%s] : MFIS_IICR = 0x%08x\n", __func__, readl(MFIS_IICR));

	do {
		mfis_eicr = readl(MFIS_EICR);

		active_ret = rtctl_check_rt_active();
		MSG_MED("[ICCOMK]   |[%s] : active_ret = %d\n", __func__, active_ret);
			if (active_ret)
				break;

		udelay(UDELAY_TIM_VALUE);
		active_loop--;
		MSG_MED("[ICCOMK]   |[%s] : active_loop = %d\n", __func__,
					(u32)active_loop);
	} while (active_loop > 0);

	if (false == active_ret) {
		rtctl_change_active_timeout_error();
	}
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
	return SMAP_OK;
}

/******************************************************************************/
/* Function   : rtctl_intcs_mask_in_active                                    */
/* Description:                                                               */
/******************************************************************************/
void rtctl_intcs_mask_in_active(void)
{
	unsigned long reg;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	for (reg = RTCTL_INTCRT_IMR0SA; reg <= RTCTL_INTCRT_IMR12SA; reg += 4) {
		writeb(0xFF, reg);
	}

	for (reg = RTCTL_INTCRT_IMR0SA3; reg <= RTCTL_INTCRT_IMR12SA3; reg += 4) {
		writeb(0xFF, reg);
	}

	for (reg = RTCTL_INTCRT_IMR0S; reg <= RTCTL_INTCRT_IMR12S; reg += 4) {
		writeb(0xFF, reg);
	}

	for (reg = RTCTL_INTCRT_IMR0S3; reg <= RTCTL_INTCRT_IMR12S3; reg += 4) {
		writeb(0xFF, reg);
	}

	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : rtctl_mfis_initialize                                         */
/* Description:                                                               */
/******************************************************************************/
void rtctl_mfis_initialize(void)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	reg_modify32(MFIS_INIT_REG, 0, MFIS_EICR);
	readl(MFIS_EICR);
	reg_modify32(MFIS_INIT_REG, 0, MFIS_IICR);
	reg_modify32(MFIS_INIT_REG, 0, MFIS_GSR);
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : rtctl_check_rt_start                                          */
/* Description:                                                               */
/******************************************************************************/
static bool rtctl_check_rt_start(void)
{
	unsigned long reg_iicr = 0;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	reg_iicr = readl(MFIS_IICR);
	MSG_MED("[ICCOMK]   |[%s] : MFIS_IICR = 0x%08x\n", __func__, readl(MFIS_IICR));
	if (0 == reg_iicr) {
		MSG_MED("[ICCOMK]OUT|[%s] : (%d)\n", __func__, 1);
		return true;
	} else {
		MSG_MED("[ICCOMK]OUT|[%s] : (%d)\n", __func__, 0);
		return false;
	}
}

/******************************************************************************/
/* Function   : rtctl_check_rt_active                                         */
/* Description:                                                               */
/******************************************************************************/
static bool rtctl_check_rt_active(void)
{

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	if (RTCTL_STS_ACTIVE == status_rt_now) {
		MSG_MED("[ICCOMK]OUT|[%s] : (%d)\n", __func__, 1);
		return true;
	} else {
		MSG_MED("[ICCOMK]OUT|[%s] : (%d)\n", __func__, 0);
		return false;
	}
}

/******************************************************************************/
/* Function   : iccom_rtctl_ioctl_standby_ng_cancel                           */
/* Description:                                                               */
/******************************************************************************/
int iccom_rtctl_ioctl_standby_ng_cancel(void)
{

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	iccom_rtctl_standby_ng_cancel();
	MSG_MED("[ICCOMK]OUT|[%s] : ret = SMAP_OK\n", __func__);

	mfis_drv_eco_suspend();

	return SMAP_OK;
}

/******************************************************************************/
/* Function   : iccom_rtctl_ioctl_standby_ng                                  */
/* Description:                                                               */
/******************************************************************************/
int iccom_rtctl_ioctl_standby_ng(void)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	iccom_rtctl_standby_ng();
	MSG_MED("[ICCOMK]OUT|[%s] : ret = SMAP_OK\n", __func__);
	return SMAP_OK;
}

/******************************************************************************/
/* Function   : iccom_rtctl_wait_rt_state_active                              */
/* Description:                                                               */
/******************************************************************************/
int iccom_rtctl_wait_rt_state_active(void)
{
	bool ret_code = false;
	unsigned long loop = MAX_POLLING_CNT;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	while (1) {
		MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__, status_rt_now);
		switch (status_rt_now) {
		case RTCTL_STS_ACTIVE:
			MSG_MED("[ICCOMK]OUT|[%s] : ret = (%d)\n",
					__func__, SMAP_OK);
			return SMAP_OK;
		case RTCTL_STS_RTBOOT:
			do {
				ret_code = rtctl_check_rt_active();
				MSG_MED("[ICCOMK]   |[%s] : ret_code = %d\n", __func__, ret_code);
				if (ret_code)
					break;

				udelay(UDELAY_TIM_VALUE);
				loop--;
				MSG_MED("[ICCOMK]   |[%s] : loop = %d\n", __func__,
						(u32)loop);
			} while (loop > 0);

			if (false == ret_code) {
				rtctl_boot_timeout_error();

			}
			break;
		case RTCTL_STS_STANDBY:
			ret_code = mfis_drv_resume();
			if (!ret_code) {
				MSG_ERROR("[%s] mfis_drv_resume() ret:%d\n", __func__, ret_code);
			}
			break;
		case RTCTL_STS_CHANGING_ACTIVE:
		case RTCTL_STS_CHANGING_STANDBY:
			msleep(MSLEEP_TIM_VALUE);
			break;
		case RTCTL_STS_FATAL_ERROR:
			MSG_HIGH("[ICCOMK]OUT|[%s] : (%d)\n",
				__func__, SMAP_ERROR_DIED);
			return SMAP_ERROR_DIED;
		}
	}
}

/******************************************************************************/
/* Function   : iccom_rtctl_initilize                                         */
/* Description:                                                               */
/******************************************************************************/
int iccom_rtctl_initilize(void)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	status_rt_now = RTCTL_STS_RTBOOT;
	MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__, status_rt_now);

	spin_lock_init(&spin_lock_hpb);
	MSG_LOW("[ICCOMK]   |[%s] : spin_lock_init\n", __func__);

	init_MUTEX(&semaphore_count_standby_ng);
	init_MUTEX(&semaphore_standby_flag);
	count_standby_ng = 0;
	MSG_MED("[ICCOMK]   |[%s] : count_standby_ng = %d\n",
			__func__, count_standby_ng);
	standby_flag = 0;
	MSG_MED("[ICCOMK]   |[%s] : standby_flag = %d\n", __func__,
				(u32)standby_flag);
	status_lcd_now = ICCOM_DRV_STATE_LCD_ON;
	MSG_MED("[ICCOMK]   |[%s] : status_lcd_now = %d\n",
			__func__, status_lcd_now);

	rtctl_intcrt_base2 = (unsigned long)ioremap(RTCTL_INTCRT_BASE2, RTCTL_INTCRT_SIZE2);
	rtctl_intcrt_base5 = (unsigned long)ioremap(RTCTL_INTCRT_BASE5, RTCTL_INTCRT_SIZE5);

	display_handle = screen_display_new();

	if (NULL == display_handle) {
		MSG_HIGH("[ICCOMK]OUT|[%s] ret = %d\n", __func__, -1);
		return SMAP_NG;
	}
	MSG_MED("[ICCOMK]OUT|[%s] ret = %d\n", __func__, 0);

	return SMAP_OK;
}

/******************************************************************************/
/* Function   : iccom_rtctl_finalize                                          */
/* Description:                                                               */
/******************************************************************************/
void iccom_rtctl_finalize(void)
{
	iounmap((void *)rtctl_intcrt_base2);
	iounmap((void *)rtctl_intcrt_base5);

	return;
}

/******************************************************************************/
/* Function   : iccom_rtctl_set_rt_state                                      */
/* Description:                                                               */
/******************************************************************************/
void iccom_rtctl_set_rt_state(void)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__, status_rt_now);

	ICCOM_DOWN_TIMEOUT(&semaphore_count_standby_ng);

	switch (status_rt_now) {
	case RTCTL_STS_CHANGING_ACTIVE:
		status_rt_now = RTCTL_STS_ACTIVE;
		MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__, status_rt_now);
		up(&semaphore_count_standby_ng);
		break;
	case RTCTL_STS_RTBOOT:
		status_rt_now = RTCTL_STS_ACTIVE;
		up(&semaphore_count_standby_ng);
		MSG_MED("[ICCOMK]   |[%s] : status_rt_now = %d\n", __func__, status_rt_now);
		break;
	default:
		up(&semaphore_count_standby_ng);
		panic("[%s] : RTDomain State ERROR : State : %d\n", __func__, status_rt_now);
		break;
	}
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_rtctl_set_rt_fatal_error                                */
/* Description:                                                               */
/******************************************************************************/
void iccom_rtctl_set_rt_fatal_error(void)
{
	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	status_rt_now = RTCTL_STS_FATAL_ERROR;
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : iccom_rtctl_before_send_cmd_check_standby                     */
/* Description:                                                               */
/******************************************************************************/
int iccom_rtctl_before_send_cmd_check_standby(
	int function_id
){

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	MSG_MED("[ICCOMK]   |[%s] : function_id = %d\n", __func__, function_id);
	if (EVENT_STATUS_STANDBYCONTROL == function_id) {
		if (RTCTL_STS_STANDBY  == status_rt_now) {
			MSG_MED("[ICCOMK]   |[%s] : status_rt_now = RTCTL_STS_STANDBY\n",
					__func__);
			MSG_HIGH("[ICCOMK]OUT|[%s] : ret = SMAP_ALREADY_STANDBY\n",
					__func__);
			return SMAP_ALREADY_STANDBY;
		}
	} else {
		MSG_MED("[ICCOMK]   |[%s] : iccom_rtctl_standby_ng() start\n", __func__);

		iccom_rtctl_standby_ng();

		MSG_MED("[ICCOMK]   |[%s] : iccom_rtctl_standby_ng() end\n", __func__);
	}
	MSG_MED("[ICCOMK]OUT|[%s] : ret = SMAP_OK\n", __func__);
	return SMAP_OK;
}

/******************************************************************************/
/* Function   : iccom_rtctl_after_send_cmd_check_standby                      */
/* Description:                                                               */
/******************************************************************************/
void iccom_rtctl_after_send_cmd_check_standby(
	int function_id
){

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);
	if (EVENT_STATUS_STANDBYCONTROL != function_id) {
		MSG_MED("[ICCOMK]   |[%s] : iccom_rtctl_standby_ng_cancel() start\n",
				__func__);
		iccom_rtctl_standby_ng_cancel();
		MSG_MED("[ICCOMK]   |[%s] : iccom_rtctl_standby_ng_cancel() end\n",
				__func__);
	}
	MSG_MED("[ICCOMK]OUT|[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : rtctl_boot_timeout_error                                      */
/* Description:                                                               */
/******************************************************************************/
static void rtctl_boot_timeout_error(void)
{

	MSG_HIGH("[ICCOMK]IN |[%s]\n", __func__);
	panic("RTDomain Boot NG : Time Out Error\n");
	MSG_HIGH("[ICCOMK]IN |[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : rtctl_change_active_timeout_error                             */
/* Description:                                                               */
/******************************************************************************/
static void rtctl_change_active_timeout_error(void)
{
	unsigned long	*addr_status;
	addr_status = (unsigned long *)g_iccom_command_area;

	MSG_HIGH("[ICCOMK]IN |[%s]\n", __func__);
	MSG_ERROR("[ICCOMK]ERR RTDomain Active Status |[%ld]\n", *addr_status);
	panic("RTDomain Change Active NG : Time Out Error\n");
	MSG_HIGH("[ICCOMK]IN |[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : rtctl_change_standby_timeout_error                            */
/* Description:                                                               */
/******************************************************************************/
static void rtctl_change_standby_timeout_error(void)
{

	MSG_HIGH("[ICCOMK]IN |[%s]\n", __func__);
	panic("RTDomain Change Standby NG : Time Out Error\n");
	MSG_HIGH("[ICCOMK]IN |[%s]\n", __func__);
}

/******************************************************************************/
/* Function   : rtctl_check_standby_enable                                    */
/* Description:                                                               */
/******************************************************************************/
bool rtctl_check_standby_enable(void)
{
	bool ret;

	MSG_MED("[ICCOMK]IN |[%s]\n", __func__);

	ret = false;

	MSG_MED("[ICCOMK]   |[%s] : count_standby_ng = %d\n", __func__, ret);

	ICCOM_DOWN_TIMEOUT(&semaphore_count_standby_ng);
	if (0 == count_standby_ng) {
		ret = true;
	} else {
		ret = false;
	}
	up(&semaphore_count_standby_ng);

	MSG_MED("[ICCOMK]OUT |[%s] ret = %d\n", __func__, ret);
	return ret;
}

