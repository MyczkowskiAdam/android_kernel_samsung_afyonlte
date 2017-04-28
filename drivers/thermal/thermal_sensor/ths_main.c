/*
 * Thermal Sensor Driver
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <mach/r8a7373.h>
#include <mach/pm.h>
#include <linux/hwspinlock.h>
#include <linux/kmsg_dump.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include "ths_user.h"
#include "ths_main.h"
#include "ths_hardware.h"

#ifndef TRUE
    #define TRUE 1
#endif

#ifndef FALSE
    #define FALSE 0
#endif

#define TRESV			0x00000008
				/* Temperature sensor reset enable bit */
#define HPB_TIMEOUT		1
					/* Timeout value 1 msec */
#define ANALOG_WAIT		63
					/* Rounded delay time of 62.5usec
		(2cycles of RCLK (32KHz) for actual analog reflected */

#ifdef CONFIG_HAS_EARLYSUSPEND
#define EARLY_SUSPEND_MAX_TRY 1
/* ths_wait_wq: Work_queue used to wait power domain to be turned off
   during early suspend */
struct workqueue_struct *ths_wait_wq;
static DECLARE_DEFERRED_WORK(ths_work, NULL);

/* early_suspend_try: Number of attempt to early suspend ths device */
static int early_suspend_try = EARLY_SUSPEND_MAX_TRY;
#endif

enum temp_control {
	E_HOTPLUG = 0x1,
	E_DFS     = 0x2
};

unsigned int dfs_ctrl;
unsigned int hotplug_ctrl;
struct thermal_sensor *ths;
int suspend_state = FALSE;
int is_clk_enable = TRUE;

/* Define the functions of Temperature control part */
static void ths_enable_reset_signal(void);
static void ths_initialize_hardware(void);
#ifdef CONFIG_HAS_EARLYSUSPEND
static int ths_early_suspend(struct early_suspend *h);
static int ths_late_resume(struct early_suspend *h);
#else
static int ths_suspend(struct device *dev);
static int ths_resume(struct device *dev);
#endif
static void ths_cpg_clock_supply(int enable);
static int ths_initialize_platform_data(struct platform_device *pdev);
static int ths_initialize_resource(struct platform_device *pdev);
static void ths_dfs_control(int level_freq);
static void ths_start_cpu(int cpu_id);
static void ths_stop_cpu(int cpu_id);
static irqreturn_t ths_isr(int irq, void *dev_id);
static void ths_work_tj0(struct work_struct *work);
static void ths_work_tj1(struct work_struct *work);
static void ths_work_tj2(struct work_struct *work);
static void ths_work_tj3(struct work_struct *work);
static int ths_start_module(struct platform_device *pdev);
static void ths_stop_module(struct platform_device *pdev);
static int __devinit ths_probe(struct platform_device *pdev);
static int __devexit ths_remove(struct platform_device *pdev);
static int __init ths_init(void);
static void __exit ths_exit(void);

/* Implement the functions of Temperature control part */

/*
 * __ths_get_cur_temp: get the current temperature of LSI
 *  @ths_id  : index of Thermal Sensor device (THS0 or THS1)
 *  @cur_temp: This value shows that the actual LSI temperature is
			in range of [cur_temp-5, cur_temp]
			E.g: cur_temp is 45. It means the current temperature is
			in range from 40 to 45 degree.
 * return:
 *		-EINVAL (-22): invalid argument
 *		-ENXIO   (-6): Thermal Sensor device is IDLE state
 *		-EACCES (-13): Permission denied due to driver in suspend state
 *		0			 : Get current temperature successfully
 */
 
int __ths_get_cur_temp(unsigned int ths_id, int *cur_temp)
{
	int ret = 0;
	u32 ctemp = 0; /* Store value of CTEMP */

	THS_DEBUG_MSG(">>> %s start\n", __func__);

	if (TRUE == suspend_state) {
		THS_DEBUG_MSG("Error! Thermal Sensor driver is suspended.\n");
		ret = -EACCES;
		goto ths_error;
	}

	if ((ths_id != 0) && (ths_id != 1)) {
		THS_DEBUG_MSG("Error! Index of Thermal Sensor is invalid.\n");
		ret = -EINVAL;
		goto ths_error;
	}

	if (E_IDLE == ths->pdata[ths_id].current_mode) {
		THS_DEBUG_MSG("Error! Thermal Sensor device %d is Idle.\n",
			      ths_id);
		ret = -ENXIO;
		goto ths_error;
	}

	ctemp     = get_register_32(THSSR(ths_id)) & CTEMP_MASK;
	*cur_temp = REG2TEMP(ctemp);
	THS_DEBUG_MSG("%s end <<<\n", __func__);

ths_error:

	return ret;
}

/*
 * __ths_set_op_mode: set operation mode of Thermal Sensor device
 *  @ths_mode: operation mode of Thermal Sensor device
 *  @ths_id  : index of Thermal Sensor device (THS0 or THS1)
 * return:
 *		0			 : set new operation mode successfully
 *		-EINVAL	(-22): invalid argument
 *		-EACCES (-13): Permission denied
 */
 
int __ths_set_op_mode(enum mode ths_mode, unsigned int ths_id)
{
	int ret = 0;

	THS_DEBUG_MSG(">>> %s start\n", __func__);

	if (TRUE == suspend_state) {
		THS_DEBUG_MSG("Error! Thermal Sensor driver is suspended.\n");
		ret = -EACCES;
		goto ths_error;
	}

	if ((ths_id != 0) && (ths_id != 1)) {
		THS_DEBUG_MSG("Error! Index of Thermal Sensor is invalid.\n");
		ret = -EINVAL;
		goto ths_error;
	}

	if (ths_mode == ths->pdata[ths_id].current_mode) {
		/* Same as current current mode, do nothing */
		return 0;
	}

	switch (ths_mode) {
	case E_NORMAL_1:	/* Set THS#ths_id to Normal 1 operation */
		modify_register_32(THSCR(ths_id), 0, THIDLE1 | THIDLE0);
		break;
	case E_NORMAL_2:	/* Set THS#ths_id to Normal 2 operation */
		modify_register_32(THSCR(ths_id), THIDLE1, THIDLE0);
		break;
	case E_IDLE:		/* Set THS#ths_id to Idle operation */
		modify_register_32(THSCR(ths_id), THIDLE1 | THIDLE0, 0);
		udelay(ANALOG_WAIT);	/* Wait for actual analog reflected */
		break;
	default:
		dev_err(&ths->pdev->dev, "Thermal Sensor operation mode is invalid.\n");
		ret = -EINVAL;
		goto ths_error;
	}

	mutex_lock(&ths->sensor_mutex);

	ths->pdata[ths_id].current_mode = (int)ths_mode;

	mutex_unlock(&ths->sensor_mutex);

	THS_DEBUG_MSG("%s end <<<\n", __func__);

ths_error:

	return ret;
}

/*
 * __ths_get_op_mode: get the current operation mode of Thermal Sensor device.
 *		@ths_id: index of Thermal Sensor device (THS0 or THS1)
 * return:
 *		E_NORMAL_1 (0): is equivalent to NORMAL 1 mode.
 *		E_NORMAL_2 (1): is equivalent to NORMAL 2 mode.
 *		E_IDLE     (2): is equivalent to IDLE mode
 *		-EINVAL  (-22): invalid argument (different from 0 and 1)
 *		-EACCES  (-13): Permission denied
 */
int __ths_get_op_mode(unsigned int ths_id)
{
	int ret = 0;

	THS_DEBUG_MSG(">>> %s start\n", __func__);

	if (TRUE == suspend_state) {
		THS_DEBUG_MSG("Error! Thermal Sensor driver is suspended.\n");
		ret = -EACCES;
		goto ths_error;
	}

	if ((ths_id != 0) && (ths_id != 1)) {
		THS_DEBUG_MSG("Error! Index of Thermal Sensor is invalid.\n");
		ret = -EINVAL;
		goto ths_error;
	}

	mutex_lock(&ths->sensor_mutex);

	ret = ths->pdata[ths_id].current_mode;

	mutex_unlock(&ths->sensor_mutex);

	THS_DEBUG_MSG("%s end <<<\n", __func__);

ths_error:

	return ret;
}

/*
 * ths_enable_reset_signal: enable reset request
 * return:
 *		None
 */
 
static void ths_enable_reset_signal(void)
{
	unsigned int value;

	THS_DEBUG_MSG(">>> %s start\n", __func__);

	/* Enable reset signal */
	value  = __raw_readl(RESCNT);
	value &= ~TRESV;
	value |= TRESV;
	__raw_writel(value, RESCNT);

	THS_DEBUG_MSG("%s end <<<\n", __func__);
}
/*
 * ths_initialize_hardware: initialize the operation for Thermal Sensor devices
 * return:
 *		None
 */
 
static void ths_initialize_hardware(void)
{
	int ret = 0;

	THS_DEBUG_MSG(">>> %s start\n", __func__);

	/* Disable chattering restraint function */
	set_register_32(FILONOFF0_RW_32B, FILONOFF_CHATTERING_DI);
	set_register_32(FILONOFF1_RW_32B, FILONOFF_CHATTERING_DI);

	/*
	 * Set detection mode for both THSs:
	 *	+ Tj1, Tj2, Tj3 are rising
	 *	+ Tj0 is falling
	 */
	set_register_32(POSNEG0_RW_32B, POSNEG_DETECTION);
	set_register_32(POSNEG1_RW_32B, POSNEG_DETECTION);

	/* Clear Interrupt Status register */
	set_register_32(STR_RW_32B, TJST_ALL_CLEAR);

	/*
	 * Set operation mode for THS0/THS1: Normal 1 mode
	 * and TSC decides a value of CPTAP automatically
	 */
	modify_register_32(THSCR0_RW_32B, CPCTL, THIDLE1 | THIDLE0);
	modify_register_32(THSCR1_RW_32B, CPCTL, THIDLE1 | THIDLE0);

	/* Enable all interrupts in THS0 and only INTDT3 in THS1 */
	set_register_32(ENR_RW_32B, TJ13_EN | TJ03_EN | TJ02_EN |
					TJ01_EN | TJ00_EN);

	/* Loop until getting the lock */
	for (;;) {
		/* Take the lock, spin for 1 msec if it's already taken */
		ret = hwspin_lock_timeout_irq(r8a7373_hwlock_sysc, HPB_TIMEOUT);
		if (0 == ret) {
			THS_DEBUG_MSG("Get lock successfully\n");
			break;
		}
	}

	/* Enable temperature sensor reset request in SYSC */
	ths_enable_reset_signal();

	/* Release the lock */
	hwspin_unlock_irq(r8a7373_hwlock_sysc);

	/* Set thresholds  (reset and raising interrupts) for THS0 and THS1 */
	set_register_32(INTCTLR0_RW_32B, CTEMP3_HEX | CTEMP2_HEX | CTEMP1_HEX
					|CTEMP0_HEX);
	set_register_32(INTCTLR1_RW_32B, CTEMP3_HEX);

	/*
	 * Mask to output THOUT signal for both THS0/1
	 * Mask to output reset signal when Tj > Tj3 for both THS0/1
	 * If CONFIG_THS_RESET is enabled, reset will be enabled in ISR for Tj3	 
	 */
	modify_register_32(PORTRST_MASK_RW_32B,
					TJ13PORT_MSK | TJ03PORT_MSK | TJ03RST_MSK | TJ13RST_MSK, 0);

	/* Wait for THS operating */
	udelay(300);	/* 300us */

	/* Get current temp to judge which Tj will be monitored */
	__ths_get_cur_temp(0, &ret);

	THS_DEBUG_MSG("%s Current temp:%d\n", __func__, ret);

	if (ret <= CTEMP0_DEC) {
		/* Mask Tj3, Tj2, Tj0; Un-mask Tj1 to monitor Tj1 */
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK | TJ02INT_MSK | TJ00INT_MSK,
											TJ01INT_MSK);
	} else if ((CTEMP0_DEC < ret) && (ret <= CTEMP1_DEC)) {
		/* Mask Tj3, Tj2; Un-mask Tj1, Tj0 to monitor Tj1, Tj0 */
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK | TJ02INT_MSK,
											TJ00INT_MSK | TJ01INT_MSK);
	} else if ((ret > CTEMP1_DEC) && (ret <= CTEMP2_DEC)) {
		/* Mask Tj3; Un-mask Tj2, Tj1, Tj0 to monitor Tj2, Tj1, Tj0 */
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK,
											TJ00INT_MSK | TJ01INT_MSK | TJ02INT_MSK);
	} else if (ret > CTEMP2_DEC) {
		/* Mask Tj1; Un-mask Tj0, Tj2, Tj3 to monitor Tj0, Tj2, Tj3*/
		modify_register_32(INT_MASK_RW_32B, TJ01INT_MSK,
											TJ00INT_MSK | TJ02INT_MSK | TJ03INT_MSK);
	}

	/* Mask Tj0/1/2/3 in THS1 to not output them to INTC */
	modify_register_32(INT_MASK_RW_32B, TJ13INT_MSK | TJ12INT_MSK |
					TJ11INT_MSK | TJ10INT_MSK, 0);

	set_register_32(STR_RW_32B, TJST_ALL_CLEAR);

	THS_DEBUG_MSG("%s end <<<\n", __func__);
}

/*
 * ths_cpg_clock_supply: Enable or disable the CPG clock supply of the THS
 * @bool enable: 1 = enable clock supply
 *		 0 = disable clock supply
 */
static void ths_cpg_clock_supply(int enable)
{
	u32 value;

	THS_DEBUG_MSG("%s: %s CPG clock supply\n",
			__func__, (enable ? "Enable" : "Disable"));

	value = ioread32(SMSTPCR5);
	if (enable)
		value &= ~THS_CLK_SUPPLY_BIT;
	else
		value |= THS_CLK_SUPPLY_BIT;

	iowrite32(value, SMSTPCR5);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
/*
 * ths_early_suspend_wq: work function executed during early suspend
 */
static void ths_early_suspend_wq(struct work_struct *work)
{
	u_int reg;

	mutex_lock(&ths->sensor_mutex);
	if (suspend_state) {
		mutex_unlock(&ths->sensor_mutex);
		THS_DEBUG_MSG("%s: device already suspended\n", __func__);
		return;
	}
	mutex_unlock(&ths->sensor_mutex);

	/* Check power domain status SGX / RealTime */
	reg = ioread32(PSTR);
	if (reg & (POWER_A3SG | POWER_A3R)) {
		THS_DEBUG_MSG("%s: waiting for power domains (sysc_pstr=%u)to be turned off (%d)\n",
					__func__, reg, early_suspend_try);

		if (early_suspend_try == 0) {
			early_suspend_try = EARLY_SUSPEND_MAX_TRY;
			THS_ERROR_MSG(
					"%s:Thermal sensor not suspended, some power domains remains\n"
					, __func__);
			return;
		}

		/* Queue work for 2 seconds */
		queue_delayed_work_on(0, ths_wait_wq, &ths_work,
					usecs_to_jiffies(2000*1000));
		early_suspend_try = early_suspend_try - 1;
		return;
	}

	early_suspend_try = EARLY_SUSPEND_MAX_TRY;

	if (ioread32(MMSTPCR5) & THS_CLK_SUPPLY_BIT) {
		/* Update the last mode only if Modem CPG clk is OFF */
		THS_DEBUG_MSG("%s: Modem CPG clock is OFF\n", __func__);

		ths->pdata[0].last_mode = ths->pdata[0].current_mode;
		ths->pdata[1].last_mode = ths->pdata[1].current_mode;

		if (ths->pdata[0].current_mode != E_IDLE)
			__ths_set_op_mode(E_IDLE, 0);

		if (ths->pdata[1].current_mode != E_IDLE)
			__ths_set_op_mode(E_IDLE, 1);

		clk_disable(ths->clk);
		is_clk_enable = FALSE;
	}

	ths_cpg_clock_supply(0);

	mutex_lock(&ths->sensor_mutex);
	suspend_state = TRUE;
	mutex_unlock(&ths->sensor_mutex);

	THS_DEBUG_MSG("%s : Thermal sensors suspended.\n", __func__);

	return;
}

/*
 * ths_early_suspend: suspend thermal sensor device during early suspend
 * of the platform
 * @h: a struct early_suspend, initialized during probe sequence
 * return: 0 if success
 */
static int ths_early_suspend(struct early_suspend *h)
{
	THS_DEBUG_MSG("%s: Enter - Add ths_early_suspend to the work queue\n",
						__func__);
	cancel_work_sync(&ths->tj3_work);
	cancel_work_sync(&ths->tj2_work);
	cancel_work_sync(&ths->tj1_work);
	cancel_work_sync(&ths->tj0_work);

	queue_delayed_work_on(0, ths_wait_wq, &ths_work,
						usecs_to_jiffies(500*1000));

	THS_DEBUG_MSG("%s: Done - Early_suspend added to the work queue\n",
	__func__);

	return 0;
}

/*
 * ths_late_resume: resume thermal sensor device during late resume of
 * the platform
 * @h: a struct early_suspend, initialized during probe sequence
 * return: 0 if success
 */
static int ths_late_resume(struct early_suspend *h)
{
	int ret = 0;
	THS_DEBUG_MSG("%s: Enter\n", __func__);

	mutex_lock(&ths->sensor_mutex);
	if (!suspend_state) {
		mutex_unlock(&ths->sensor_mutex);
		THS_DEBUG_MSG("%s: device already resumed\n", __func__);
		return 0;
	}

	ths_cpg_clock_supply(1);

	suspend_state = FALSE;
	mutex_unlock(&ths->sensor_mutex);

	if (!is_clk_enable) {
		clk_enable(ths->clk);
		is_clk_enable = TRUE;
	}

	__ths_set_op_mode((enum mode)ths->pdata[0].last_mode, 0);
	__ths_set_op_mode((enum mode)ths->pdata[1].last_mode, 1);
	/* Wait for THS operating */
	udelay(300);    /* 300us */

	/* Get current temp to judge which Tj will be monitored */
	__ths_get_cur_temp(0, &ret);

	THS_DEBUG_MSG("%s Current temp:%d\n", __func__, ret);

	if (ret <= CTEMP0_DEC) {
		/* Mask Tj3, Tj2, Tj0; Un-mask Tj1 to monitor Tj1 */
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK | TJ02INT_MSK | TJ00INT_MSK,
											TJ01INT_MSK);
	} else if ((CTEMP0_DEC < ret) && (ret <= CTEMP1_DEC)) {
		/* Mask Tj3, Tj2; Un-mask Tj1, Tj0 to monitor Tj1, Tj0 */
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK | TJ02INT_MSK,
											TJ00INT_MSK | TJ01INT_MSK);
	} else if ((ret > CTEMP1_DEC) && (ret <= CTEMP2_DEC)) {
		/* Mask Tj3; Un-mask Tj2, Tj1, Tj0 to monitor Tj2, Tj1, Tj0 */
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK,
											TJ00INT_MSK | TJ01INT_MSK | TJ02INT_MSK);
	} else if (ret > CTEMP2_DEC) {
		/* Mask Tj1; Un-mask Tj0, Tj2, Tj3 to monitor Tj0, Tj2, Tj3*/
		modify_register_32(INT_MASK_RW_32B, TJ01INT_MSK,
											TJ00INT_MSK | TJ02INT_MSK | TJ03INT_MSK);
	}

	THS_DEBUG_MSG("%s : Thermal sensors resumed.\n", __func__);

	return 0;
}
#endif /* End CONFIG_HAS_EARLYSUSPEND */

/*
 * ths_suspend: change the current state of Thermal Sensor device to IDLE state
 *  @dev: a struct device
 * return: 0: successful suspend
 */
static int ths_suspend(struct device *dev)
{

	THS_DEBUG_MSG("%s: Enter\n", __func__);

	if (suspend_state) {
		THS_DEBUG_MSG("%s: device already suspended\n", __func__);
		return 0;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	/* Cancel potential early suspend Work Queue */
	cancel_delayed_work_sync(&ths_work);
	early_suspend_try = EARLY_SUSPEND_MAX_TRY;
#endif /* End CONFIG_HAS_EARLYSUSPEND */
	cancel_work_sync(&ths->tj3_work);
	cancel_work_sync(&ths->tj2_work);
	cancel_work_sync(&ths->tj1_work);
	cancel_work_sync(&ths->tj0_work);
	cancel_delayed_work_sync(&ths->work);

	if (ioread32(MMSTPCR5) & THS_CLK_SUPPLY_BIT) {
		/* Update the last mode only if Modem CPG clk is OFF */
		THS_DEBUG_MSG("%s: Modem CPG clock is OFF\n", __func__);

		ths->pdata[0].last_mode = ths->pdata[0].current_mode;
		ths->pdata[1].last_mode = ths->pdata[1].current_mode;

		if (ths->pdata[0].current_mode != E_IDLE)
			__ths_set_op_mode(E_IDLE, 0);

		if (ths->pdata[1].current_mode != E_IDLE)
			__ths_set_op_mode(E_IDLE, 1);

		clk_disable(ths->clk);
		is_clk_enable = FALSE;
	}

	ths_cpg_clock_supply(0);

	mutex_lock(&ths->sensor_mutex);
	suspend_state = TRUE;
	mutex_unlock(&ths->sensor_mutex);

	THS_DEBUG_MSG("%s : Thermal sensors suspended.\n", __func__);

	return 0;
}

/*
 * ths_resume: change the state of Thermal Sensor device to its state as
 *				before system suspends
 *  @dev: a struct device
 * return: successful resume
 */
static int ths_resume(struct device *dev)
{
	int ret = 0;
	THS_DEBUG_MSG("%s: Enter\n", __func__);

	if (!suspend_state) {
		THS_DEBUG_MSG("%s: device already resumed\n", __func__);
		return 0;
	}

	ths_cpg_clock_supply(1);

	if (!is_clk_enable) {
		clk_enable(ths->clk);
		is_clk_enable = TRUE;
	}

	mutex_lock(&ths->sensor_mutex);
	suspend_state = FALSE;
	mutex_unlock(&ths->sensor_mutex);

	__ths_set_op_mode((enum mode)ths->pdata[0].last_mode, 0);
	__ths_set_op_mode((enum mode)ths->pdata[1].last_mode, 1);

	/* Wait for THS operating */
	udelay(300);    /* 300us */

	/* Get current temp to judge which Tj will be monitored */
	__ths_get_cur_temp(0, &ret);

	THS_DEBUG_MSG("%s Current temp:%d\n", __func__, ret);

	if (ret <= CTEMP0_DEC) {
		/* Mask Tj3, Tj2, Tj0; Un-mask Tj1 to monitor Tj1 */
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK | TJ02INT_MSK | TJ00INT_MSK,
											TJ01INT_MSK);
	} else if ((CTEMP0_DEC < ret) && (ret <= CTEMP1_DEC)) {
		/* Mask Tj3, Tj2; Un-mask Tj1, Tj0 to monitor Tj1, Tj0 */
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK | TJ02INT_MSK,
											TJ00INT_MSK | TJ01INT_MSK);
	} else if ((ret > CTEMP1_DEC) && (ret <= CTEMP2_DEC)) {
		/* Mask Tj3; Un-mask Tj2, Tj1, Tj0 to monitor Tj2, Tj1, Tj0 */
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK,
											TJ00INT_MSK | TJ01INT_MSK | TJ02INT_MSK);
	} else if (ret > CTEMP2_DEC) {
		/* Mask Tj1; Un-mask Tj0, Tj2, Tj3 to monitor Tj0, Tj2, Tj3*/
		modify_register_32(INT_MASK_RW_32B, TJ01INT_MSK,
											TJ00INT_MSK | TJ02INT_MSK | TJ03INT_MSK);
	}

	schedule_delayed_work(&ths->work, msecs_to_jiffies(1000));

	THS_DEBUG_MSG("%s : Thermal sensors resumed.\n", __func__);
	return 0;
}

/*
 * ths_initialize_platform_data: initialize Thermal Sensor platform data
 *  @pdev: a struct platform_device
 * return:
 *		0				: Initialize successfully
 *		-EINVAL(-22)	: No such device or address is registered
 *		-ENOMEM (-12)	: Can't allocate driver data
 */
static int ths_initialize_platform_data(struct platform_device *pdev)
{
	THS_DEBUG_MSG(">>> %s start\n", __func__);

	if (!pdev->dev.platform_data) {
		dev_err(&pdev->dev, "Error! No platform data defined\n");
		return -EINVAL;
	}

	ths = kzalloc(sizeof(struct thermal_sensor), GFP_KERNEL);

	if (NULL == ths) {
		dev_err(&pdev->dev, "Error! Failed to allocate driver data\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, ths);
	ths->pdev = pdev;
	memcpy(ths->pdata, pdev->dev.platform_data,
			2 * sizeof(struct thermal_sensor_data));


	THS_DEBUG_MSG("%s end <<<\n", __func__);

	return 0;
}

/*
 * ths_initialize_resource: Get base address of Thermal Sensor module
 *							then remap I/O memory.
 *  @pdev: a struct platform_device
 * return:
 *		0			: Initialize successfully
 *		-ENXIO (-6)	: No such device or address
 */
static int ths_initialize_resource(struct platform_device *pdev)
{
	struct resource *rs = NULL;

	THS_DEBUG_MSG(">>> %s start\n", __func__);

	rs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == rs) {
		dev_err(&pdev->dev, "Error! Failed to get I/O memory for Thermal Sensor module\n");
		return -ENXIO;
	}

	ths->iomem_base = ioremap_nocache(rs->start, resource_size(rs));
	if (NULL == ths->iomem_base) {
		dev_err(&pdev->dev, "Error! Failed to remap I/O memory for Thermal Sensor module\n");
		return -ENXIO;
	}

	THS_DEBUG_MSG("%s end <<<\n", __func__);

	return 0;
}

/*
 * ths_dfs_control: control DFS function when CPU temperature higher
 *					or lower than thresholds
 *	@level_freq: has three values (LIMIT_LOW/LIMIT_MID/LIMIT_NONE)
 * return:
 *		None
 */
static void ths_dfs_control(int level_freq)
{
	THS_DEBUG_MSG(">>> %s start\n", __func__);

#ifdef CONFIG_SHMOBILE_CPUFREQ
	limit_max_cpufreq(level_freq);
#else
	THS_DEBUG_MSG("%s DFS is disabled <<<\n", __func__);
#endif /* CONFIG_SHMOBILE_CPUFREQ */

	THS_DEBUG_MSG("%s end <<<\n", __func__);
}

/*
 * ths_start_cpu: start CPU0/1 when CPU temperature lower than thresholds
 *	@cpu_id: index of CPU (CPU0 or CPU1)
 * return:
 *		None
 */
static void ths_start_cpu(int cpu_id)
{
	int err = 0;
	THS_DEBUG_MSG(">>> %s start\n", __func__);

#ifdef CONFIG_HOTPLUG_ARCH_R8A7373
	if (cpu_online(cpu_id) != 1)
#ifdef CONFIG_HOTPLUG_CPU_MGR
		err = cpu_up_manager(cpu_id, THS_HOTPLUG_ID);
#else /*!defined(CONFIG_HOTPLUG_CPU_MGR)*/
		err = cpu_up(cpu_id);
#endif /*CONFIG_HOTPLUG_CPU_MGR*/
	if (err)
		printk(KERN_ERR "%s: CPU %d start() failed. error = %d\n",
					__func__, cpu_id, err);
#else
	THS_DEBUG_MSG("%s HOTPLUG_CPU is disabled <<<\n", __func__);
#endif /* CONFIG_HOTPLUG_ARCH_R8A7373 */

	THS_DEBUG_MSG("%s end <<<\n", __func__);
}

/*
 * ths_stop_cpu: stop CPU0/1 when CPU temperature higher than thresholds
 *	@cpu_id: index of CPU (CPU0 or CPU1)
 * return:
 *		None
 */
static void ths_stop_cpu(int cpu_id)
{
	int err = 0;
	THS_DEBUG_MSG(">>> %s start\n", __func__);

#ifdef CONFIG_HOTPLUG_ARCH_R8A7373
	if (1 == cpu_online(cpu_id))
#ifdef CONFIG_HOTPLUG_CPU_MGR
		err = cpu_down_manager(cpu_id, THS_HOTPLUG_ID);
#else /*!defined(CONFIG_HOTPLUG_CPU_MGR)*/
		err = cpu_down(cpu_id);
#endif /*CONFIG_HOTPLUG_CPU_MGR*/
	if (err)
		printk(KERN_ERR "%s: CPU %d stop() failed. error = %d\n",
					__func__, cpu_id, err);
#else
	THS_DEBUG_MSG("%s HOTPLUG_CPU is disabled <<<\n", __func__);
#endif /* CONFIG_HOTPLUG_ARCH_R8A7373 */

	THS_DEBUG_MSG("%s end <<<\n", __func__);
}

/*
 * ths_isr: Interrupt handler in case of INTDT3/2/1/0 interrupts
 *  @irq  : Irq number for Thermal Sensor module
 *  @dev_id: ID of device registered
 * return:
 *		IRQ_HANDLED : successfully
 */
static irqreturn_t ths_isr(int irq, void *dev_id)
{
	int intr_status = -1;
#ifdef CONFIG_THS_DEBUG_ENABLE
	int temp;
#endif
	THS_DEBUG_MSG(">>> %s start\n", __func__);
	
	disable_irq_nosync(irq);

	intr_status = get_register_32(STR_RW_32B);
	THS_DEBUG_MSG(">>>STR_RW_32B (Before clear) = %x (Hex)\n",
					get_register_32(STR_RW_32B));

	THS_DEBUG_MSG("Current temp THS0 (%d) = %d\n",
			__ths_get_cur_temp(0, &temp), temp);

	if ((TJ03ST == (intr_status & TJ03ST)) ||
		(TJ13ST == (intr_status & TJ13ST))) {
		/* INTDT3 interrupt occurs */
		/* Un-mask INTDT0 interrupt to output to INTC(THS0) */
		printk(KERN_INFO"%s: THS IRQ called: [Tj2 -> Tj3]\n", __func__);
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK | TJ02INT_MSK |	TJ01INT_MSK,
											TJ00INT_MSK);

#ifdef CONFIG_THS_RESET
		printk(KERN_INFO"%s:!!!!!!!!!!  OVERHEAT - SHUTTING DOWN  !!!!!!!!!!!!!\n", __func__);
		kmsg_dump(KMSG_DUMP_PANIC);
		/* Unmask TJ3 reset masks to reset the device */
		modify_register_32(PORTRST_MASK_RW_32B, TJ13PORT_MSK | TJ03PORT_MSK,
						TJ13RST_MSK | TJ03RST_MSK);
		queue_work(ths->queue, &ths->tj3_work);
#else
		queue_work(ths->queue, &ths->tj3_work);
#endif
	} else if (TJ02ST == (intr_status & TJ02ST)) {
		/* INTDT2 interrupt occurs */
		/* Un-mask INTDT0/3 interrupt to output to INTC(THS0) */
		printk(KERN_INFO"%s: THS IRQ called: [Tj1 -> Tj2]\n", __func__);
		modify_register_32(INT_MASK_RW_32B, TJ02INT_MSK | TJ01INT_MSK,
											TJ00INT_MSK | TJ03INT_MSK);
		queue_work(ths->queue, &ths->tj2_work);
	} else if (TJ01ST == (intr_status & TJ01ST)) {
		/* INTDT1 interrupt occurs */
		/* Un-mask INTDT0/2 interrupts to output to INTC (THS0) */
		printk(KERN_INFO"%s: THS IRQ called: [Tj0 -> Tj1]\n", __func__);
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK | TJ01INT_MSK,
											TJ00INT_MSK | TJ02INT_MSK);
		queue_work(ths->queue, &ths->tj1_work);
	} else if (TJ00ST == (intr_status & TJ00ST)) {
		/* INTDT0 interrupt occurs */
		/* Un-mask INTDT1 interrupt to output to INTC (THS0) */
		printk(KERN_INFO"%s: THS IRQ called: [Tj0]\n", __func__);
		modify_register_32(INT_MASK_RW_32B, TJ03INT_MSK | TJ02INT_MSK | TJ00INT_MSK,
											TJ01INT_MSK);
		queue_work(ths->queue, &ths->tj0_work);
	}

	/* Clear Interrupt Status register */
	/* For RSTFLG bit - HRM mentions as "It's cleared in 0 writing after 
 	 * Tj*ST bits in STR register were cleared".
 	 * It was observed that this bit was not reset when the entire register 
 	 * was reset once. So calling the same function to clear RSTFLG bit */
	set_register_32(STR_RW_32B, TJST_ALL_CLEAR);
	set_register_32(STR_RW_32B, TJST_ALL_CLEAR);

	THS_DEBUG_MSG(">>>STR_RW_32B (After clear) = %x (Hex)\n",
					get_register_32(STR_RW_32B));
	THS_DEBUG_MSG("%s end <<<\n", __func__);

	return IRQ_HANDLED;
}

static void ths_work_queue(struct work_struct *dwork)
{
	int temp1 = 0, temp2 = 0;

	__ths_get_cur_temp(0, &temp1);
	printk(KERN_INFO"Current THS0 Temp:%d degrees Celsius\n", temp1);

	__ths_get_cur_temp(1, &temp2);
	printk(KERN_INFO"Current THS1 Temp:%d degrees Celsius\n", temp2);

	schedule_delayed_work(&ths->work, msecs_to_jiffies(30000));
}

/*
 * ths_work_tj0:
 *	1/ configure DFS driver to limit the maximum frequency
 *		in OVERDRIVE(option) - MAX - MID - LOW - Very LOW frequency
 *		(1456.0[MHz] - 1196.0[MHz] - 598.0[MHz]
 *			- 299.0[MHz] - 199.3[MHz])
 *		in case of CTEMP <= CTEMP0
 *	2/ Start CPU1
 *
 *  @work: a struct work_struct
 * return:
 *		None
 */
static void ths_work_tj0(struct work_struct *work)
{
	THS_DEBUG_MSG(">>> %s start\n", __func__);

#ifdef CONFIG_THS_CPU_HOTPLUG
	if (E_HOTPLUG == hotplug_ctrl) {
		ths_start_cpu(1); /* Start CPU1 */
		hotplug_ctrl = ~E_HOTPLUG;
	}
#endif /* CONFIG_THS_CPU_HOTPLUG */

#ifdef CONFIG_THS_DFS
	if (E_DFS == dfs_ctrl) {
		ths_dfs_control(LIMIT_NONE);
		dfs_ctrl = ~E_DFS;
	}
#endif /* CONFIG_THS_DFS */

	enable_irq(ths->ths_irq);

	THS_DEBUG_MSG("%s end <<<\n", __func__);
}

/*
 * ths_work_tj1:
 *  1/ configure DFS driver to limit the maximum frequency
 *		in MID - LOW - Very LOW frequency in case of CTEMP > CTEMP1
 *	2/ Stop CPU1
 *
 *  @work: a struct work_struct
 * return:
 *		None
 */
static void ths_work_tj1(struct work_struct *work)
{
	THS_DEBUG_MSG(">>> %s start\n", __func__);

#ifdef CONFIG_THS_CPU_HOTPLUG
	if (E_HOTPLUG != hotplug_ctrl) {
		ths_stop_cpu(1); /* Stop CPU1 */
		hotplug_ctrl = E_HOTPLUG;
	}
#elif defined CONFIG_THS_DFS
	if (E_DFS != dfs_ctrl) {
		ths_dfs_control(LIMIT_MID);
		dfs_ctrl = E_DFS;
	}
#endif /* CONFIG_THS_CPU_HOTPLUG */

	enable_irq(ths->ths_irq);

	THS_DEBUG_MSG("%s end <<<\n", __func__);
}

/*
 * ths_work_tj2:
 *  1/ configure DFS driver to limit the maximum frequency
 *		in LOW - Very LOW frequency in case of CTEMP > CTEMP2
 *	2/ Stop CPU1 or configure DFS driver to limit the maximum frequency
 *		in MID - LOW - Very LOW frequency in case of CTEMP > CTEMP1
 *
 *  @work: a struct work_struct
 * return:
 *		None
 */
static void ths_work_tj2(struct work_struct *work)
{
	THS_DEBUG_MSG(">>> %s start\n", __func__);

#ifdef CONFIG_THS_CPU_HOTPLUG
	if (E_HOTPLUG != hotplug_ctrl) {
		ths_stop_cpu(1); /* Control CPU1 */
		hotplug_ctrl = E_HOTPLUG;
	} else {
		THS_DEBUG_MSG("%s: CPU1 was already stopped in tj1_work!\n",
		__func__);
	}
#endif /* CONFIG_THS_CPU_HOTPLUG */

#ifdef CONFIG_THS_DFS
#ifdef CONFIG_THS_CPU_HOTPLUG
	ths_dfs_control(LIMIT_MID);
#else
	ths_dfs_control(LIMIT_LOW);
#endif /* CONFIG_THS_CPU_HOTPLUG */
	dfs_ctrl = E_DFS;
#endif /* CONFIG_THS_DFS */

	enable_irq(ths->ths_irq);

	THS_DEBUG_MSG("%s end <<<\n", __func__);

}

/*
 * ths_work_tj3:
 *	1/ Stop CPU1
 *
 *  @work: a struct work_struct
 * return:
 *		None
 */
static void ths_work_tj3(struct work_struct *work)
{
	THS_DEBUG_MSG(">>> %s start\n", __func__);

#ifdef CONFIG_THS_CPU_HOTPLUG
	if (E_HOTPLUG != hotplug_ctrl) {
		ths_stop_cpu(1); /* Control CPU1 */
		hotplug_ctrl = E_HOTPLUG;
	} else {
		THS_DEBUG_MSG("%s CPU1 was already stopped in tj2_work!\n",
						__func__);
	}
#endif /* CONFIG_THS_CPU_HOTPLUG */

	enable_irq(ths->ths_irq);

	THS_DEBUG_MSG("%s end <<<\n", __func__);

}


/*
 * ths_start_module: Activate Thermal sensor module
 * return:
 *		-EINVAL (-22)	: Invalid clock
 *		Other			: Other errors
 */
static int ths_start_module(struct platform_device *pdev)
{
	int ret = 0;
	char clk_name[50];

	THS_DEBUG_MSG(">>> %s start\n", __func__);

	snprintf(clk_name, sizeof(clk_name), "thermal_sensor.%d", pdev->id);
	ths->clk = clk_get(&pdev->dev, clk_name);
	if (IS_ERR(ths->clk)) {
		ret = PTR_ERR(ths->clk);
		dev_err(&pdev->dev, "Error! Failed to get Thermal Sensor clock\n");
		goto error;
	}

	ret = clk_enable(ths->clk);
	if (ret < 0) {
		ret = -EINVAL;
		dev_err(&pdev->dev, "Error! Failed to enable Thermal Sensor clock\n");
	}

	is_clk_enable = TRUE;

error:
	THS_DEBUG_MSG("%s end <<<\n", __func__);
	return ret;
}

/*
 * ths_stop_module: Stop Thermal sensor module
 * return:
 *		None
 */
static void ths_stop_module(struct platform_device *pdev)
{
	THS_DEBUG_MSG(">>> %s start\n", __func__);

	clk_disable(ths->clk);
	clk_put(ths->clk);

	is_clk_enable = FALSE;

	THS_DEBUG_MSG("%s end <<<\n", __func__);
}

/*
 * ths_probe: Register Thermal sensor platform driver
 *  @pdev: a struct platform_device
 * return:
 *		0				: Register successfully
 *		Other error code: Register unsuccessfully
 */
static int __devinit ths_probe(struct platform_device *pdev)
{
	int ret = -1;

	THS_DEBUG_MSG(">>> %s start\n", __func__);

	/* Register miscdevice (initialize user interface) */
	ret = misc_register(&ths_user);
	if (ret < 0) {
		dev_err(&pdev->dev, "Error! Can't register miscdevice (user interface)\n");
		goto error_0;
	}

	/* Get default Thermal Sensor driver data: operation mode */
	ret = ths_initialize_platform_data(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Error! Can't initialize platform data\n");
		goto error_1;
	}

	/* Get default driver resource: mapping memory */
	ret = ths_initialize_resource(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Error! Can't initialize resource\n");
		goto error_2;
	}

	/* Initialize mutex */
	mutex_init(&ths->sensor_mutex);

	/* Mark that there is no DFS or HOTPLUG control by THS*/
	dfs_ctrl = 0;
	hotplug_ctrl = 0;

	INIT_DELAYED_WORK(&ths->work, ths_work_queue);

	/* Register callback function for a work to processing
		when Tj > Tj2/Tj1 or Tj < Tj0 */
	INIT_WORK(&ths->tj0_work, ths_work_tj0);
	INIT_WORK(&ths->tj1_work, ths_work_tj1);
	INIT_WORK(&ths->tj2_work, ths_work_tj2);
	INIT_WORK(&ths->tj3_work, ths_work_tj3);

	/* Create a new queue for bottom-half processing */
	ths->queue = create_singlethread_workqueue("ths_queue");
	if (ths->queue == NULL) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Error! Failed to Create Work Queue\n");
		goto error_3;
	}

    /* Activate Thermal Sensor module */
    ret = ths_start_module(pdev);
    if (ret < 0) {
        dev_err(&pdev->dev, "Error! Can not start Thermal Sensor module\n");
        goto error_3;
    }

    /* Initialize the operation of Thermal Sensor device */
    ths_initialize_hardware();

    ths->ths_irq = platform_get_irq(pdev, 0);
    if (ths->ths_irq < 0) {
        ret = ths->ths_irq;
        dev_err(&pdev->dev, "Error! Failed to get irq\n");
        goto error_4;
    }
    ret = request_irq(ths->ths_irq, ths_isr, 0, pdev->name, pdev);
    if (ret < 0) {
        dev_err(&pdev->dev, "Error! Failed to request IRQ\n");
        goto error_4;
    }

#ifdef CONFIG_HAS_EARLYSUSPEND
	/* Register early suspend struct */
	ths->early_suspend.suspend = (void *)ths_early_suspend;
	ths->early_suspend.resume = (void *)ths_late_resume;
	register_early_suspend(&ths->early_suspend);
	ths_wait_wq = alloc_ordered_workqueue("ths_wait_wq", 0);
	INIT_DELAYED_WORK(&ths_work, ths_early_suspend_wq);
#endif

	schedule_delayed_work(&ths->work, msecs_to_jiffies(500));

	THS_DEBUG_MSG("%s end (Normal case) <<<\n", __func__);
	return 0;

error_4:
	ths_stop_module(pdev);
error_3:
	mutex_destroy(&ths->sensor_mutex);
	iounmap(ths->iomem_base);
error_2:
	platform_set_drvdata(pdev, NULL);
	kfree(ths);
error_1:
	misc_deregister(&ths_user);
error_0:
	THS_DEBUG_MSG("%s end (Error case) <<<\n", __func__);

	return ret;
}

/*
 * ths_remove: Deregister Thermal sensor platform driver
 *  @pdev: a struct platform_device
 * return:
 *		0				: Deregister successfully
 *		Other error code: Deregister unsuccessfully
 */
static int __devexit ths_remove(struct platform_device *pdev)
{
	THS_DEBUG_MSG(">>> %s start\n", __func__);

	ths_stop_module(pdev);
	free_irq(platform_get_irq(pdev, 0), pdev);
	mutex_destroy(&ths->sensor_mutex);
	iounmap(ths->iomem_base);
	platform_set_drvdata(pdev, NULL);
	kfree(ths);
	misc_deregister(&ths_user);

	THS_DEBUG_MSG("%s end <<<\n", __func__);
	return 0;
}

/* Power managed during early and late suspend/resume */
static const struct dev_pm_ops ths_dev_pm_ops = {
	.suspend = ths_suspend,
	.resume  = ths_resume,
};

static struct platform_driver ths_driver = {
	.probe  = ths_probe,
	.remove = __devexit_p(ths_remove),
	.driver = {
		.name = "thermal_sensor",
		.pm   = &ths_dev_pm_ops,
	}
};

/*
 * ths_init: Register Thermal sensor module
 *  @ths_driver: Thermal sensor platform driver
 * return:
 *		0				: Register successfully
 *		Other error code: Register unsuccessfully
 */
static int __init ths_init(void)
{
	return platform_driver_register(&ths_driver);
}

/*
 * ths_exit: Deregister Thermal sensor module
 *  @ths_driver: Thermal sensor platform driver
 * return:
 *		None
 */
static void __exit ths_exit(void)
{
	platform_driver_unregister(&ths_driver);
}


module_init(ths_init);
module_exit(ths_exit);

MODULE_AUTHOR("Renesas Mobile Corporation");
MODULE_DESCRIPTION("THERMAL SENSOR DRIVER");
MODULE_LICENSE("GPL");
