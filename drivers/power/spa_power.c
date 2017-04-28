/*
 * Samsung Power and Charger.
 *
 * drivers/power/spa_power.c
 *
 * Drivers for samsung battery and charger.
 * (distributed from spa.c and linear-power.c)
 *
 * Copyright (C) 2012, Samsung Electronics.
 *
 * This program is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>
#include <linux/sched.h>

#include <mach/common.h>

#define SPA_DEBUG_FEATURE 1
#define BATT_TYPE "SDI_SDI"
#define CONFIG_SEC_BATT_EXT_ATTRS
#define SPA_FAKE_FULL_CAPACITY
#include <linux/spa_power.h>

#ifdef CONFIG_BATTERY_D2153
#include <linux/d2153/d2153_battery.h>
extern int d2153_battery_set_status(int type, int status);
#endif



#define FIRST_CHG_CURR_ABSORBING_SHOCK 100

#define MSEC    1
#define SECOND_BY_MSEC  1000*MSEC
#define MINUTE_BY_MSEC  60*SECOND_BY_MSEC
#define HOUR_BY_MSEC    60*MINUTE_BY_MSEC

/* Debugging */
#define SPA_DBG_LEVEL0 0U /* no log */
#define SPA_DBG_LEVEL1 1U /* minimum log */
#define SPA_DBG_LEVEL2 2U
#define SPA_DBG_LEVEL3 3U
#define SPA_DBG_LEVEL4 4U /* maximum log */
#define SPA_DBG_LEVEL_INTERNAL SPA_DBG_LEVEL3
#define SPA_DBG_LEVEL_OUT SPA_DBG_LEVEL1
#define SPA_DBG_LOG_SIZE	4096*100

/* Dual logging. */
#define pr_spa_dbg(lvl, args...) \
	do { \
		if ( SPA_DBG_LEVEL_OUT >= SPA_DBG_##lvl ) \
		{ \
				pr_info(args); \
		} \
		if ( SPA_DBG_LEVEL_INTERNAL >= SPA_DBG_##lvl ) \
		{ \
				spa_log_internal(args); \
		} \
	} while(0)

static char spa_log_buffer[SPA_DBG_LOG_SIZE];
static unsigned int spa_log_offset=0;

#define SPA_PROBE_STATUS_BEGIN	0
#define SPA_PROBE_STATUS_READY	1
static unsigned char probe_status = SPA_PROBE_STATUS_BEGIN;

static void spa_log_internal(const char *log, ...)
{
	va_list args;
	int t_size = 0, l_size = 0, r_size = 0;

	char tbuf[50], *tp;
	unsigned tlen;
	unsigned long long t;
	unsigned long nanosec_rem;
	unsigned long flags;

	preempt_disable();
	raw_local_irq_save(flags);
	/* time stamp : make time stamp string */
	{
		int this_cpu = smp_processor_id();

		t = cpu_clock(this_cpu);
		nanosec_rem = do_div(t, 1000000000);
		tlen = sprintf(tbuf, "[%5lu.%6lu]", (unsigned long)t, nanosec_rem / 1000);
	}
	raw_local_irq_restore(flags);
	preempt_enable();

	t_size = SPA_DBG_LOG_SIZE-1;
	l_size = 300;
	r_size = t_size - spa_log_offset;

	if (r_size <= 0)
		spa_log_offset =0;
	if (r_size < l_size)
		spa_log_offset =0;

	/* Put time stamp */
	{
		unsigned int temp_offset=0;
		char *temp_pos;
		for (tp = tbuf; tp < tbuf+tlen; tp++) {
			temp_offset = tp - tbuf;
			temp_pos = spa_log_buffer + spa_log_offset + temp_offset;
			*temp_pos =*tp;
		}
		spa_log_offset += tlen;
	}
	va_start(args, log);
	spa_log_offset += vsnprintf(spa_log_buffer+spa_log_offset, INT_MAX, log, args);
	va_end(args);
}


#if defined(SPA_DEBUG_FEATURE)

struct {
	int evt;
	int data;
} spa_evt_log[255];
unsigned int spa_evt_idx=0;
static ssize_t spa_power_attrs_show(struct device *pdev, struct device_attribute *attr, char *buf);
static ssize_t spa_power_attrs_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t count);

enum{
		SPA_POWER_PROP_CHARGER_TYPE,
		SPA_POWER_PROP_CHARGING_CURRENT,
		SPA_POWER_CHARGING_STATUS,
		SPA_POWER_PROP_EOC_CURRENT,
		SPA_POWER_PROP_TOP_VOLTAGE,
		SPA_POWER_PROP_LOWBATT_VOLTAGE,
		SPA_POWER_PROP_CHARGE_EXP_TIME,
		SPA_POWER_PROP_TIMES_EXPIRED,
		SPA_POWER_PROP_FULL_CHARGED,
		SPA_POWER_PROP_BATT_TYPE,
		SPA_POWER_PROP_BATT_HEALTH,
		SPA_POWER_PROP_BATT_TEMP,
		SPA_POWER_PROP_BATT_TEMP_AVG,
		SPA_POWER_PROP_BATT_TEMP_ADC,
		SPA_POWER_PROP_BATT_VOLTAGE,
		SPA_POWER_PROP_BATT_VOLTAGE_AVG,
		SPA_POWER_PROP_BATT_CAPACITY,
		SPA_POWER_PROP_BATT_VF,
		SPA_POWER_PROP_BATT_UPDATE_INTERVAL,
		SPA_POWER_PROP_ALL,
		SPA_POWER_PROP_DBG_SIMUL,
		SPA_POWER_PROP_EVT_LOG,
};


static struct device_attribute spa_power_attrs[]=
{
	__ATTR(charger_type, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(charging_current, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(charging_status, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(eoc_current, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(top_voltage, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(lowbatt_voltage, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(charge_exp_time, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(times_expired, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(full_charged, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(batt_type, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(batt_health, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(batt_temp, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(batt_temp_avg, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(batt_temp_adc, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(batt_voltage, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(batt_voltage_avg, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(batt_capacity, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(batt_vf, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(batt_update_interval, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(all_prop, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(dbg_simul, 0644, spa_power_attrs_show, spa_power_attrs_store),
	__ATTR(evt_log, 0644, spa_power_attrs_show, spa_power_attrs_store),
};
#endif

/* Power supply class name */
#define POWER_SUPPLY_BATTERY "battery"
#define POWER_SUPPLY_WALL "ac"
#define POWER_SUPPLY_USB "usb"


static struct spa_power_desc *g_spa_power;

static int spa_set_charge(struct spa_power_desc *spa_power_iter, unsigned int act);
static int spa_get_charger_type(struct spa_power_desc *spa_power_iter);
static int spa_get_batt_temp(struct spa_power_desc *spa_power_iter);
static int spa_get_batt_temp_avg(struct spa_power_desc *spa_power_iter);
static int spa_get_batt_temp_adc(struct spa_power_desc *spa_power_iter);
static int spa_get_batt_capacity(struct spa_power_desc *spa_power_iter);
static int spa_set_fg_reset(struct spa_power_desc *spa_power_iter, int dval);
static int spa_get_batt_voltage(struct spa_power_desc *spa_power_iter);
static int spa_get_batt_volt_avg(struct spa_power_desc *spa_power_iter);
static int spa_get_batt_vf_status(struct spa_power_desc *spa_power_iter);
static void spa_update_power_supply_charger(struct spa_power_desc *spa_power_iter, unsigned int prop);
static void spa_update_power_supply_battery(struct spa_power_desc *spa_power_iter, unsigned int prop);
static void spa_stop_charge_timer(SPA_CHARGING_STATUS_T endtype, void *data);
static int spa_start_charge_timer(charge_timer_t duration, void *data);
static int spa_do_status(struct spa_power_desc *spa_power_iter, unsigned char machine, unsigned int phase, unsigned status);

#if defined(SPA_DEBUG_FEATURE)
static ssize_t spa_power_attrs_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	ssize_t count=0;
	struct spa_power_desc *spa_power_iter = g_spa_power;

	const ptrdiff_t off = attr-spa_power_attrs;

	unsigned int view_all=0;

	if(!spa_power_iter)return 0;

	switch(off)
	{
		case SPA_POWER_PROP_ALL:
			view_all=1;
		case SPA_POWER_PROP_CHARGER_TYPE:
			{
				count  +=  scnprintf(buf+count, PAGE_SIZE-count, "charger_type = %d\n", spa_power_iter->charger_info.charger_type);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_CHARGING_CURRENT:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "charging_current = %d\n", spa_power_iter->charger_info.charging_current);
			}
			if (!view_all)
				break;
		case SPA_POWER_CHARGING_STATUS:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "charging_status = %d, %d\n", spa_power_iter->charging_status.phase, spa_power_iter->charging_status.status);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_EOC_CURRENT:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "eoc_current = %d\n", spa_power_iter->charger_info.eoc_current);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_TOP_VOLTAGE:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "top_voltage = %d\n", spa_power_iter->charger_info.top_voltage);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_LOWBATT_VOLTAGE:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "low_batt_voltage = %d\n", spa_power_iter->charger_info.lowbatt_voltage);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_CHARGE_EXP_TIME:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "charge_exp_time = %d\n", spa_power_iter->charger_info.charge_expire_time);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_TIMES_EXPIRED:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "times_expired = %d\n", spa_power_iter->charger_info.times_expired);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_BATT_TYPE:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "batt_type = %s\n", spa_power_iter->batt_info.type);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_BATT_HEALTH:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "batt_health = %d\n", spa_power_iter->batt_info.health);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_BATT_TEMP:
			{
				int i = 0;
				for (i = 0; i < ADC_RUNNING_AVG_SIZE ; i++) {
					count += scnprintf(buf+count, PAGE_SIZE-count, "batt_temp[i] = %d\n", spa_power_iter->temp_reading.container[i]);
				}
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_BATT_TEMP_AVG:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "batt_temp_avg = %d\n", spa_power_iter->batt_info.temp);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_BATT_TEMP_ADC:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "batt_temp_adc = %d\n", spa_power_iter->batt_info.temp_adc);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_BATT_VOLTAGE:
			{
				int i = 0;
				for (i = 0; i < ADC_RUNNING_AVG_SIZE ; i++) {
					count += scnprintf(buf+count, PAGE_SIZE-count, "batt_voltage[i] = %d\n", spa_power_iter->volt_reading.container[i]);
				}
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_BATT_VOLTAGE_AVG:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "batt_voltage_avg = %d\n", spa_power_iter->batt_info.voltage);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_BATT_CAPACITY:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "batt_capacity = %d\n", spa_power_iter->batt_info.capacity);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_BATT_VF:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "batt_vf = %d\n", spa_power_iter->batt_info.vf_status);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_BATT_UPDATE_INTERVAL:
			{
				count += scnprintf(buf+count, PAGE_SIZE-count, "batt_update_interval = %d\n", spa_power_iter->batt_info.update_interval);
			}
			if (!view_all)
				break;
		case SPA_POWER_PROP_EVT_LOG:
			{
				int i = 0;
				count += sprintf(buf+count, "evt_index=%d\n", spa_evt_idx);
				for (i = 0 ; i < 255 ; i++) {
					count += sprintf(buf+count, "evt_log[%d]=%d,%d\n", i, spa_evt_log[i].evt, spa_evt_log[i].data);
				}
			}
			break;
		default:
			break;
	}
	return count;
}

static ssize_t spa_power_attrs_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct spa_power_desc *spa_power_iter = g_spa_power;
	int intval;

	const ptrdiff_t off = attr-spa_power_attrs;

	if(!spa_power_iter)return 0;

	if(off == SPA_POWER_PROP_DBG_SIMUL)
	{
		sscanf(buf, "%d", &intval);
		spa_power_iter->dbg_simul=intval;
		return count;
	}

	if( spa_power_iter->dbg_simul != 1)
	{
		return 0;
	}

	switch(off)
	{
		case SPA_POWER_PROP_CHARGE_EXP_TIME:
			sscanf(buf, "%d", &intval);
			spa_power_iter->charger_info.charge_expire_time = intval * MINUTE_BY_MSEC;

			cancel_delayed_work_sync(&spa_power_iter->spa_expire_charge_work);
			schedule_delayed_work(&spa_power_iter->spa_expire_charge_work,
					msecs_to_jiffies(spa_power_iter->charger_info.charge_expire_time));
			break;
		case SPA_POWER_PROP_FULL_CHARGED:
			sscanf(buf, "%d", &intval);
			if (intval == 1) {
				spa_event_handler(SPA_EVT_EOC, NULL);
			} else if (intval == 2) {
				SPA_CHARGING_STATUS_T status = {0, 0};
				spa_stop_charge_timer(status, spa_power_iter);
				spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_FULL, SPA_STATUS_FULL_FORCE);
				spa_start_charge_timer(CHARGE_TIMER_90MIN, spa_power_iter);
			}
			break;
		case SPA_POWER_PROP_BATT_TEMP:
			sscanf(buf, "%d", &intval);
			spa_power_iter->batt_info.temp = intval;
			break;
		case SPA_POWER_PROP_BATT_CAPACITY:
			sscanf(buf, "%d", &intval);
			spa_power_iter->batt_info.capacity = intval;
         spa_update_power_supply_battery(spa_power_iter, POWER_SUPPLY_PROP_CAPACITY);
			break;
		case SPA_POWER_PROP_BATT_VF:
			sscanf(buf, "%d", &intval);
			spa_power_iter->batt_info.vf_status = intval;
			break;
	}

	cancel_delayed_work_sync(&spa_power_iter->battery_work);
	schedule_delayed_work(&spa_power_iter->battery_work, msecs_to_jiffies(5000));
	return count;
}
#endif

/*for CTIA test*/
#if defined(CONFIG_TARGET_LOCALE_USA)
#define USE_NONE			0
#define USE_CALL			(0x1 << 0)
#define USE_VIDEO			(0x1 << 1)
#define USE_MUSIC			(0x1 << 2)
#define USE_BROWSER		(0x1 << 3)
#define USE_HOTSPOT			(0x1 << 4)
#define USE_CAMERA			(0x1 << 5)
#define USE_DATA_CALL		(0x1 << 6)
#define USE_GPS			(0x1 << 7)
#define USE_LTE			(0x1 << 8)
#define USE_WIFI			(0x1 << 9)

static void sec_bat_use_module(struct spa_power_desc *spa_power_iter,
						int module, int enable)
{
	struct spa_power_data *pdata = spa_power_iter->pdata;

	pr_info("/BATT/ enable = %x, moduel = %x\n", enable, module);

	/* ignore duplicated deactivation of same event  */
	if ((!enable && spa_power_iter->batt_use == USE_NONE) || (enable && spa_power_iter->batt_use)) {
		pr_info("/BATT/ ignore duplicated same event\n");
		//spa_power_iter->batt_use = module;
		return;
	}

	cancel_delayed_work_sync(&spa_power_iter->battery_work);

	if (enable) {
		spa_power_iter->batt_use = module;

		spa_power_iter->charger_info.suspend_temp_cold
			= pdata->event_suspend_temp_cold;
		spa_power_iter->charger_info.recovery_temp_cold
			= pdata->event_recovery_temp_cold;
		spa_power_iter->charger_info.suspend_temp_hot
			= pdata->event_suspend_temp_hot;
		spa_power_iter->charger_info.recovery_temp_hot
			= pdata->event_recovery_temp_hot;
	} else {
		spa_power_iter->batt_use = USE_NONE;

		spa_power_iter->charger_info.suspend_temp_cold
			= pdata->suspend_temp_cold;
		spa_power_iter->charger_info.recovery_temp_cold
			= pdata->recovery_temp_cold;
		spa_power_iter->charger_info.suspend_temp_hot
			= pdata->suspend_temp_hot;
		spa_power_iter->charger_info.recovery_temp_hot
			= pdata->recovery_temp_hot;

		pr_info("/BATT/ nothing to clear\n");
		}


	schedule_delayed_work(&spa_power_iter->battery_work, msecs_to_jiffies(0));
}
#endif

#if defined(CONFIG_SEC_BATT_EXT_ATTRS)
enum
{
	SS_BATT_LP_CHARGING,
	//SS_BATT_CHARGING_SOURCE,
	SS_BATT_TEMP_AVER,
	SS_BATT_TEMP_ADC_AVER,
	SS_BATT_TYPE,
	SS_BATT_READ_ADJ_SOC,
	SS_BATT_READ_RAW_SOC,
	SS_BATT_RESET_SOC,
	SS_BATT_VOL_AVER,
	/*for CTIA test*/
#if defined(CONFIG_TARGET_LOCALE_USA)
	BATT_WCDMA_CALL,
	BATT_GSM_CALL,
	BATT_CALL,
	BATT_VIDEO,
	BATT_MUSIC,
	BATT_BROWSER,
	BATT_HOTSPOT,
	BATT_CAMERA,
	BATT_DATA_CALL,
	BATT_GPS,
	BATT_LTE,
	BATT_WIFI,
	BATT_USE,
#endif
	SPA_POWER_PROP_SIOP_LEVEL,
};
static ssize_t ss_batt_ext_attrs_show(struct device *pdev, struct device_attribute *attr, char *buf);
static ssize_t ss_batt_ext_attrs_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t count);

static struct device_attribute ss_batt_ext_attrs[]=
{
	__ATTR(batt_lp_charging, 0644, ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	//__ATTR(batt_charging_source, 0644, ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(batt_temp_aver, 0644, ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(batt_temp_adc_aver, 0644, ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(batt_type, 0644, ss_batt_ext_attrs_show, NULL),
	__ATTR(batt_read_adj_soc, 0644, ss_batt_ext_attrs_show , NULL),
	__ATTR(batt_read_raw_soc, 0644, ss_batt_ext_attrs_show , NULL),
	__ATTR(batt_reset_soc, 0664, NULL, ss_batt_ext_attrs_store),
	__ATTR(batt_vol_aver, 0664, ss_batt_ext_attrs_show, NULL),
		/*for CTIA test*/
#if defined(CONFIG_TARGET_LOCALE_USA)
	__ATTR(call, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(video, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(music, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(browser, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(hotspot, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(camera, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(data_call, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(gps, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(lte, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(wifi, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
	__ATTR(batt_use, S_IRUGO | (S_IWUSR | S_IWGRP), ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
#endif
	__ATTR(siop_level, 0644, ss_batt_ext_attrs_show, ss_batt_ext_attrs_store),
};

unsigned int lp_boot_mode;
static int get_boot_mode(char *str)
{
	get_option(&str, &lp_boot_mode);

	return 1;
}
__setup("lpcharge=", get_boot_mode);
static ssize_t ss_batt_ext_attrs_show(struct device *pdev, struct device_attribute *attr, char *buf)
{
	ssize_t count=0;
	unsigned int lp_charging=0;

	struct spa_power_desc *spa_power_iter=g_spa_power;

	const ptrdiff_t off = attr-ss_batt_ext_attrs;

	if(!spa_power_iter)return 0;

	switch(off)
	{
		case SS_BATT_LP_CHARGING:
			lp_charging = spa_power_iter->lp_charging;
			count += scnprintf(buf+count, PAGE_SIZE-count, "%d\n", lp_charging);
			break;
#if 0
		case SS_BATT_CHARGING_SOURCE:
				count += scnprintf(buf+count, PAGE_SIZE-count, "%d\n", spa_power_iter->charger_info.charger_type);
			break;
#endif
#if 1
		case SS_BATT_TEMP_AVER:
			count += scnprintf(buf+count, PAGE_SIZE-count, "%d\n", spa_power_iter->batt_info.temp);
			break;
		case SS_BATT_TEMP_ADC_AVER:
			count += scnprintf(buf+count, PAGE_SIZE-count, "%d\n", spa_power_iter->batt_info.temp_adc);
			break;
#else	// to test hardware
		case SS_BATT_TEMP_AVER:
			count+=scnprintf(buf+count, PAGE_SIZE-count, "%d\n", d2153_get_rf_temperature());
			break;
		case SS_BATT_TEMP_ADC_AVER:
			count+=scnprintf(buf+count, PAGE_SIZE-count, "%d\n", d2153_battery_read_status(D2153_BATTERY_TEMP_HPA));
			break;
#endif
		case SS_BATT_TYPE:
			count += scnprintf(buf+count, PAGE_SIZE-count, "%s\n", spa_power_iter->batt_info.type);
			break;
		case SS_BATT_READ_ADJ_SOC:
			count += scnprintf(buf+count, PAGE_SIZE-count, "%d\n", spa_power_iter->batt_info.capacity);
			break;
		case SS_BATT_READ_RAW_SOC:
			count+=scnprintf(buf+count, PAGE_SIZE-count, "%d\n", (spa_power_iter->batt_info.capacity * 100));
			break;
		case SS_BATT_VOL_AVER:
			count+=scnprintf(buf+count, PAGE_SIZE-count, "%d\n", (spa_power_iter->batt_info.voltage));
			break;

		/*for CTIA test*/
#if defined(CONFIG_TARGET_LOCALE_USA)
		case BATT_WCDMA_CALL:
		case BATT_GSM_CALL:
		case BATT_CALL:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				(spa_power_iter->batt_use & USE_CALL) ? 1 : 0);
			break;
		case BATT_VIDEO:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				(spa_power_iter->batt_use & USE_VIDEO) ? 1 : 0);
			break;
		case BATT_MUSIC:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				(spa_power_iter->batt_use & USE_MUSIC) ? 1 : 0);
			break;
		case BATT_BROWSER:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				(spa_power_iter->batt_use & USE_BROWSER) ? 1 : 0);
			break;
		case BATT_HOTSPOT:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				(spa_power_iter->batt_use & USE_HOTSPOT) ? 1 : 0);
			break;
		case BATT_CAMERA:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				(spa_power_iter->batt_use & USE_CAMERA) ? 1 : 0);
			break;
		case BATT_DATA_CALL:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				(spa_power_iter->batt_use & USE_DATA_CALL) ? 1 : 0);
			break;
		case BATT_GPS:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				(spa_power_iter->batt_use & USE_GPS) ? 1 : 0);
			break;
		case BATT_LTE:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				(spa_power_iter->batt_use & USE_GPS) ? 1 : 0);
			break;
		case BATT_WIFI:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				(spa_power_iter->batt_use & BATT_WIFI) ? 1 : 0);
			break;
		case BATT_USE:
			count += scnprintf(buf + count, PAGE_SIZE - count, "%d\n",
				spa_power_iter->batt_use);
			break;
#endif
		/*** emd CTIA test */
		case SPA_POWER_PROP_SIOP_LEVEL:
			count+=scnprintf(buf+count, PAGE_SIZE-count, "%d\n", spa_power_iter->batt_info.siop_level);
			break;
		default:
			break;
	}

	return count;
}

static ssize_t ss_batt_ext_attrs_store(struct device *pdev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct spa_power_desc *spa_power_iter=g_spa_power;
	const ptrdiff_t off = attr-ss_batt_ext_attrs;
	struct power_supply *ps;
	int intval;

	if(!spa_power_iter) {
		pr_spa_dbg(LEVEL1, "%s : spa_power_iter is NULL\n", __func__);
		return 0;
	}
	
	ps = power_supply_get_by_name(spa_power_iter->charger_info.charger_name);

	switch(off)
	{
		case SS_BATT_RESET_SOC:
			{
				int val;
				sscanf(buf, "%d", &val);
				spa_set_fg_reset(spa_power_iter, val);
				spa_power_iter->new_gathering.temperature = 1;
				spa_power_iter->new_gathering.voltage = 1;
				cancel_delayed_work_sync(&spa_power_iter->battery_work);
				schedule_delayed_work(&spa_power_iter->battery_work, msecs_to_jiffies(0));
				msleep(500);
			}
			break;
			/*for CTIA test*/
#if defined(CONFIG_TARGET_LOCALE_USA)
		case BATT_WCDMA_CALL:
		case BATT_GSM_CALL:
		case BATT_CALL:
			if (sscanf(buf, "%d\n", &x) == 1) {
				sec_bat_use_module(spa_power_iter, USE_CALL, x);
			}
			pr_debug("[BAT]:%s: camera = %d\n", __func__, x);
			break;
		case BATT_VIDEO:
			if (sscanf(buf, "%d\n", &x) == 1) {
				sec_bat_use_module(spa_power_iter, USE_VIDEO, x);
			}
			pr_debug("[BAT]:%s: mp3 = %d\n", __func__, x);
			break;
		case BATT_MUSIC:
			if (sscanf(buf, "%d\n", &x) == 1) {
				sec_bat_use_module(spa_power_iter, USE_MUSIC, x);
			}
			pr_debug("[BAT]:%s: video = %d\n", __func__, x);
			break;
		case BATT_BROWSER:
			if (sscanf(buf, "%d\n", &x) == 1) {
				sec_bat_use_module(spa_power_iter, USE_BROWSER, x);
			}
			pr_debug("[BAT]:%s: voice call 2G = %d\n", __func__, x);
			break;
		case BATT_HOTSPOT:
			if (sscanf(buf, "%d\n", &x) == 1) {
				sec_bat_use_module(spa_power_iter, USE_HOTSPOT, x);
			}
			pr_debug("[BAT]:%s: voice call 3G = %d\n", __func__, x);
			break;
		case BATT_CAMERA:
			if (sscanf(buf, "%d\n", &x) == 1) {
				sec_bat_use_module(spa_power_iter, USE_CAMERA, x);
			}
			pr_debug("[BAT]:%s: data call = %d\n", __func__, x);
			break;
		case BATT_DATA_CALL:
			if (sscanf(buf, "%d\n", &x) == 1) {
				sec_bat_use_module(spa_power_iter, USE_DATA_CALL, x);
			}
			pr_debug("[BAT]:%s: wifi = %d\n", __func__, x);
			break;
		case BATT_GPS:
			if (sscanf(buf, "%d\n", &x) == 1) {
				sec_bat_use_module(spa_power_iter, USE_GPS, x);
			}
			break;
		case BATT_LTE:
			if (sscanf(buf, "%d\n", &x) == 1) {
				sec_bat_use_module(spa_power_iter, USE_LTE, x);
			}
			pr_debug("[BAT]:%s: gps = %d\n", __func__, x);
			break;
		case BATT_WIFI:
			if (sscanf(buf, "%d\n", &x) == 1) {
				sec_bat_use_module(spa_power_iter, USE_WIFI, x);
			}
			pr_debug("[BAT]:%s: gps = %d\n", __func__, x);
			break;
#endif
		case SPA_POWER_PROP_SIOP_LEVEL:
			sscanf(buf, "%d", &intval);
			spa_power_iter->batt_info.siop_level = intval;
			printk("siop_level : %d \n", intval);

			if(spa_power_iter->batt_info.siop_level != 0)
				spa_do_status(spa_power_iter, SPA_MACHINE_SIOP, POWER_SUPPLY_STATUS_CHARGING, SPA_STATUS_NONE);
			else
				spa_do_status(spa_power_iter, SPA_MACHINE_SIOP, POWER_SUPPLY_STATUS_DISCHARGING, SPA_STATUS_NONE);
			break;
	}

	return count;
}

#endif

#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
static void spa_back_charging_work(struct work_struct *work)
{
	struct power_supply *ps;
	union power_supply_propval value;
	struct spa_power_desc *spa_power_iter = container_of(work, struct spa_power_desc, back_charging_work.work);

	pr_spa_dbg(LEVEL3, "%s : enter \n", __func__);

	ps = power_supply_get_by_name(spa_power_iter->charger_info.charger_name);

	pr_spa_dbg(LEVEL3, "%s : Actual discharging. after %d mins\n", __func__, spa_power_iter->pdata->backcharging_time);
	value.intval = POWER_SUPPLY_STATUS_DISCHARGING;
	ps->set_property(ps, POWER_SUPPLY_PROP_STATUS, &value);

	spa_stop_charge_timer(spa_power_iter->charging_status, spa_power_iter);

	if(spa_power_iter->charging_status.phase == POWER_SUPPLY_STATUS_FULL)
	{
		spa_power_iter->charging_status.status = SPA_STATUS_NONE;
		
#ifdef CONFIG_D2153_EOC_CTRL
		d2153_battery_set_status(D2153_STATUS_EOC_CTRL, D2153_BAT_CHG_BACKCHG_FULL);
#endif
	}
	else
	{
		pr_spa_dbg(LEVEL3, "%s: Wrong case !!!\n",__func__);
	}

	pr_spa_dbg(LEVEL3, "%s : leave \n", __func__);
}
#endif

static int spa_set_charge(struct spa_power_desc *spa_power_iter, unsigned int act)
{
	struct spa_power_data *pdata = spa_power_iter->pdata;
	struct power_supply *ps;
	union power_supply_propval value;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	ps = power_supply_get_by_name(spa_power_iter->charger_info.charger_name);

	if(act == SPA_CMD_CHARGE)
	{
		// 1. Charging current
		if(spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_MAINS ||
				spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_USB_DCP)
		{
			spa_power_iter->charger_info.charging_current=pdata->charging_cur_wall;
		}
		else if(spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_USB ||
				spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_USB_ACA)
		{
			spa_power_iter->charger_info.charging_current=pdata->charging_cur_usb;
		} 
		else if(spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_USB_CDP )
		{
			spa_power_iter->charger_info.charging_current=pdata->charging_cur_cdp_usb;	
		}
		else {
			pr_spa_dbg(LEVEL3, "%s : charger type error\n", __func__);
		}

        	spa_power_iter->charger_info.charging_current = 
			spa_power_iter->charger_info.charging_current * spa_power_iter->batt_info.siop_level / 100;

        	if ((spa_power_iter->charger_info.charging_current > 0) && 
			(spa_power_iter->charger_info.charging_current < spa_power_iter->pdata->charging_cur_usb))
               		spa_power_iter->charger_info.charging_current = spa_power_iter->pdata->charging_cur_usb;
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
		// 2. eoc current
		if( spa_power_iter->charging_status.phase == POWER_SUPPLY_STATUS_FULL ) {
			spa_power_iter->charger_info.eoc_current = pdata->recharging_eoc;
		} else {
			spa_power_iter->charger_info.eoc_current = pdata->eoc_current;
		}
#endif
		/* 4. Queue work for Setting Fast Charging current */
		cancel_delayed_work_sync(&spa_power_iter->fast_charging_work);

		if(spa_power_iter->batt_info.siop_level != 0)
		queue_delayed_work(spa_power_iter->spa_workqueue, &spa_power_iter->fast_charging_work, msecs_to_jiffies(50));


		pr_spa_dbg(LEVEL1, "%s : Charging!! current=%d, eoc_cur=%d\n",
				__func__,
				spa_power_iter->charger_info.charging_current,
				spa_power_iter->charger_info.eoc_current);
	}
	else if(act == SPA_CMD_DISCHARGE) // discharging
	{
		/* 1. stop charging */
		spa_power_iter->charger_info.charging_current=0;
		cancel_delayed_work_sync(&spa_power_iter->fast_charging_work);
		value.intval = POWER_SUPPLY_STATUS_DISCHARGING;
		ps->set_property(ps, POWER_SUPPLY_PROP_STATUS, &value);
		cancel_delayed_work_sync(&spa_power_iter->fast_charging_work);
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
		cancel_delayed_work_sync(&spa_power_iter->back_charging_work);
#endif
		pr_spa_dbg(LEVEL2, "%s : Discharging!! ", __func__ );
	}
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	else if(act == SPA_CMD_DELAYED_DISCHARGE)
	{
		// 1. stop charging
		spa_power_iter->charger_info.charging_current=0;
		cancel_delayed_work_sync(&spa_power_iter->fast_charging_work);
		cancel_delayed_work_sync(&spa_power_iter->back_charging_work);
		schedule_delayed_work(&spa_power_iter->back_charging_work, msecs_to_jiffies(MINUTE_BY_MSEC*spa_power_iter->pdata->backcharging_time));
		pr_spa_dbg(LEVEL2, "%s : Delayed Discharging!! delay time = %d", __func__,  spa_power_iter->pdata->backcharging_time);
	}
#endif
	else
	{
		// 1. stop charging
		spa_power_iter->charger_info.charging_current=0;
		value.intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		ps->set_property(ps, POWER_SUPPLY_PROP_STATUS, &value);
		pr_spa_dbg(LEVEL1, "%s : Not charging!! ", __func__);
	}

	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return 0;
}

static int spa_get_charger_type(struct spa_power_desc *spa_power_iter)
{
	struct power_supply *ps;
	union power_supply_propval value;
	int ret = 0;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	pr_spa_dbg(LEVEL1, "%s : charger name = %s\n", __func__,
					spa_power_iter->charger_info.charger_name);
	ps = power_supply_get_by_name(spa_power_iter->charger_info.charger_name);
	if (ps == NULL) {
		pr_spa_dbg(LEVEL1, "%s : ps is NULL\n", __func__);
		return -ENODATA;
	}

	ret = ps->get_property(ps, POWER_SUPPLY_PROP_TYPE, &value);
	if (ret < 0)
		return ret;

	pr_spa_dbg(LEVEL2, "%s : charger type = %d\n", __func__, value.intval);
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);

	return value.intval;
}

static int spa_get_batt_temp(struct spa_power_desc *spa_power_iter)
{
	union power_supply_propval value;

	int temp_adc, i, temp=0;
	struct spa_temp_tb *batt_temp_tb = spa_power_iter->pdata->batt_temp_tb;
	unsigned int batt_temp_tb_len = spa_power_iter->pdata->batt_temp_tb_len;
	temp_adc = spa_get_batt_temp_adc(spa_power_iter);

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	for (i = 0 ; i < batt_temp_tb_len - 1 ; i++) {
		if (batt_temp_tb[i].adc >= temp_adc
			&& batt_temp_tb[i+1].adc <= temp_adc) {
			int temp_diff, adc_diff, inc_comp;
			temp_diff = (batt_temp_tb[i+1].temp - batt_temp_tb[i].temp);
			adc_diff = (batt_temp_tb[i+1].adc - batt_temp_tb[i].adc);
			inc_comp = batt_temp_tb[i].temp - ((temp_diff * batt_temp_tb[i].adc) / adc_diff);
			temp = ((temp_adc * temp_diff) / adc_diff) + inc_comp;

			return temp;
		}
	}
	pr_spa_dbg(LEVEL3, "%s : temperature = %d\n", __func__, value.intval);
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);

	if(batt_temp_tb[0].adc < temp_adc)
		return batt_temp_tb[0].temp;
	else if(batt_temp_tb[batt_temp_tb_len - 1].adc > temp_adc)
		return batt_temp_tb[batt_temp_tb_len - 1].temp;
	else
		return temp;
}

static int spa_get_batt_temp_avg(struct spa_power_desc *spa_power_iter)
{
	int i=0;
	int index = spa_power_iter->temp_reading.index;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

#if (SPA_DBG_LEVEL_OUT >= 3)
	if (spa_get_batt_temp(spa_power_iter) < 0) {
		pr_err("%s : under 0 temperature\n", __func__);
	}
#endif

	pr_spa_dbg(LEVEL4, "%s : avg_size = %d\n", __func__,\
					ADC_RUNNING_AVG_SIZE);
	if (spa_power_iter->new_gathering.temperature == 1) {
		pr_spa_dbg(LEVEL2, "%s : temperature =>  gather initial values\n", __func__);
		spa_power_iter->temp_reading.sum = 0;
		for (i = 0; i < ADC_RUNNING_AVG_SIZE ; i++) {
			spa_power_iter->temp_reading.container[i]=spa_get_batt_temp(spa_power_iter);
			spa_power_iter->temp_reading.sum += spa_power_iter->temp_reading.container[i];
		}
		spa_power_iter->temp_reading.index=0;
		spa_power_iter->new_gathering.temperature=0;
	} else {
		spa_power_iter->temp_reading.sum -= spa_power_iter->temp_reading.container[index];
		spa_power_iter->temp_reading.container[index] = spa_get_batt_temp(spa_power_iter);
		spa_power_iter->temp_reading.sum  +=  spa_power_iter->temp_reading.container[index];
		spa_power_iter->temp_reading.index = (index + 1) % ADC_RUNNING_AVG_SIZE;
	}

	spa_power_iter->temp_reading.avg = spa_power_iter->temp_reading.sum >> ADC_RUNNING_AVG_SHIFT;

#if (SPA_DBG_LEVEL_OUT >= SPA_DBG_LEVEL3)
	{
		printk(KERN_INFO "%s : temperature(idx=%d)", __func__, spa_power_iter->temp_reading.index);
		for (i = 0; i < ADC_RUNNING_AVG_SIZE ; i++) {
			printk(KERN_INFO "[%d]=%d,", i, spa_power_iter->temp_reading.container[i]);
		}
		printk(KERN_INFO "\n");
	}
#endif
	pr_spa_dbg(LEVEL3, "%s : temp_sum = %d, temp_avg = %d\n", __func__,
				spa_power_iter->temp_reading.sum, spa_power_iter->temp_reading.avg);

	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return spa_power_iter->temp_reading.avg;
}

static int spa_get_batt_temp_adc(struct spa_power_desc *spa_power_iter)
{
	struct power_supply *ps;
	union power_supply_propval value;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	ps = power_supply_get_by_name(spa_power_iter->charger_info.charger_name);

	ps->get_property(ps, POWER_SUPPLY_PROP_BATT_TEMP_ADC, &value);

	pr_spa_dbg(LEVEL4, "%s : temp_adc = %d\n", __func__, value.intval);

	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return value.intval;
}

static int spa_get_batt_capacity(struct spa_power_desc *spa_power_iter)
{
	struct power_supply *ps;
	union power_supply_propval value;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	ps = power_supply_get_by_name(spa_power_iter->charger_info.charger_name);

	ps->get_property(ps, POWER_SUPPLY_PROP_CAPACITY, &value);

	pr_spa_dbg(LEVEL3, "%s : capacity = %d\n", __func__, value.intval);

	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return value.intval;
}

static int spa_set_fg_reset(struct spa_power_desc *spa_power_iter, int dval)
{
	struct power_supply *ps;
	union power_supply_propval value;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	ps = power_supply_get_by_name(spa_power_iter->charger_info.charger_name);
	value.intval=dval;
	ps->set_property(ps, POWER_SUPPLY_PROP_CAPACITY, &value);

	pr_spa_dbg(LEVEL2, "%s : reset soc, %d\n", __func__, value.intval);

	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return 0;
}


static int spa_get_batt_voltage(struct spa_power_desc *spa_power_iter)
{
	unsigned int retry_cnt = 0;
	int pass_cond = 1;
	struct power_supply *ps;
	union power_supply_propval value;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	ps = power_supply_get_by_name(spa_power_iter->charger_info.charger_name);

	do
	{
		ps->get_property(ps, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
		if(value.intval < 2800 || value.intval > 4400)
		{
			pass_cond=0;
		}

		if (retry_cnt++ >= 3)
			break;
	}
	while(!pass_cond);

	pr_spa_dbg(LEVEL4, "%s : voltage = %d\n", __func__, value.intval);
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return value.intval;
}

static int spa_get_batt_volt_avg(struct spa_power_desc *spa_power_iter)
{
	int i = 0;
	int index = spa_power_iter->volt_reading.index;
	int readed_volt = 0;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);
	readed_volt = spa_get_batt_voltage(spa_power_iter);

	if (readed_volt < 0) {
		pr_err("%s : wrong voltage\n", __func__);
	}

	pr_spa_dbg(LEVEL4, "%s : avg_size = %d\n", __func__,
			ADC_RUNNING_AVG_SIZE);
	if (spa_power_iter->new_gathering.voltage == 1) {
		pr_spa_dbg(LEVEL2, "%s : voltage is 0 and gather initial"\
				"values\n", __func__);
		spa_power_iter->volt_reading.sum = 0;
		for (i = 0; i < ADC_RUNNING_AVG_SIZE ; i++) {
			spa_power_iter->volt_reading.container[i]=spa_get_batt_voltage(spa_power_iter);
			spa_power_iter->volt_reading.sum += spa_power_iter->volt_reading.container[i];
		}
		spa_power_iter->volt_reading.index = 0;
		spa_power_iter->new_gathering.voltage = 0;
	} else {
		/* handling exceptional voltage. */
		spa_power_iter->volt_reading.sum -= spa_power_iter->volt_reading.container[index];
		spa_power_iter->volt_reading.container[index] = readed_volt;
		spa_power_iter->volt_reading.sum += spa_power_iter->volt_reading.container[index];
		spa_power_iter->volt_reading.index = (index + 1) % ADC_RUNNING_AVG_SIZE;
	}
	spa_power_iter->volt_reading.prev_val = readed_volt;

	spa_power_iter->volt_reading.avg = spa_power_iter->volt_reading.sum >> ADC_RUNNING_AVG_SHIFT;

#if (SPA_DBG_LEVEL_OUT >= SPA_DBG_LEVEL3)
	{
		printk(KERN_INFO "%s : voltage ", __func__);
		for (i = 0; i < ADC_RUNNING_AVG_SIZE ; i++) {
			printk(KERN_INFO "[%d]=%d,", i, spa_power_iter->volt_reading.container[i]);
		}
		printk(KERN_INFO "\n");
	}
#endif
	pr_spa_dbg(LEVEL3, "%s : voltage_avg = %d\n", __func__, spa_power_iter->volt_reading.avg);

	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return spa_power_iter->volt_reading.avg;
}

int spa_get_batt_voltage_extern(void)
{
	if (g_spa_power == NULL)
		return -1;

	return g_spa_power->batt_info.voltage;
}
EXPORT_SYMBOL(spa_get_batt_voltage_extern);

static int spa_get_batt_vf_status(struct spa_power_desc *spa_power_iter)
{
	struct power_supply *ps;
	union power_supply_propval value;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	ps = power_supply_get_by_name(spa_power_iter->charger_info.charger_name);

	ps->get_property(ps, POWER_SUPPLY_PROP_PRESENT, &value);

	pr_spa_dbg(LEVEL4, "%s : vf_status = %d\n", __func__, value.intval);

	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return value.intval;
}
static int spa_do_status(struct spa_power_desc *spa_power_iter, unsigned char machine, unsigned int phase, unsigned int status)
{
	struct spa_power_data *pdata = spa_power_iter->pdata;
	charge_timer_t charge_timer_duration = pdata->charge_timer_limit;
	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	if (machine == SPA_MACHINE_NORMAL) {
		if (phase == POWER_SUPPLY_STATUS_DISCHARGING)  /* DISCHARGING */
		{
			spa_set_charge(spa_power_iter, SPA_CMD_DISCHARGE);
			spa_power_iter->charging_status.phase = POWER_SUPPLY_STATUS_DISCHARGING;
			spa_power_iter->charging_status.status = SPA_STATUS_NONE;
			spa_power_iter->batt_info.update_interval = SPA_BATT_UPDATE_INTERVAL;
			spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_GOOD;

			//Initialize SIOP LEVEL as 100 in case of Discharging
			spa_power_iter->batt_info.siop_level = 100;

			if (status == SPA_STATUS_SUSPEND_OVP) {
				spa_power_iter->charging_status.status = SPA_STATUS_SUSPEND_OVP;
				spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
				spa_update_power_supply_battery(spa_power_iter, POWER_SUPPLY_PROP_HEALTH);
			}

			spa_stop_charge_timer(spa_power_iter->charging_status, spa_power_iter);

			pr_spa_dbg(LEVEL1, "%s : Do discharging\n", __func__);
		} else if (phase == POWER_SUPPLY_STATUS_CHARGING) /* CHARGING */
		{
			if (spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_BATTERY) {
				pr_spa_dbg(LEVEL2, "%s : Wrong type of charger\n", __func__);
				return -1;
			}
			spa_set_charge(spa_power_iter, SPA_CMD_CHARGE);
			spa_power_iter->charging_status.phase=POWER_SUPPLY_STATUS_CHARGING;
			spa_power_iter->charging_status.status=SPA_STATUS_NONE;

			spa_power_iter->batt_info.update_interval = SPA_BATT_UPDATE_INTERVAL_WHILE_CHARGING;
			spa_start_charge_timer(charge_timer_duration, spa_power_iter);
			pr_spa_dbg(LEVEL1, "%s : Do charging\n", __func__);

		} else if (phase == POWER_SUPPLY_STATUS_FULL) /* FULL */
		{
			if (spa_power_iter->charging_status.phase != POWER_SUPPLY_STATUS_FULL) { /* just now full-charged after normal charging. */
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
				spa_set_charge(spa_power_iter, SPA_CMD_DELAYED_DISCHARGE);
				spa_power_iter->charging_status.phase = POWER_SUPPLY_STATUS_FULL;
				spa_power_iter->charging_status.status = SPA_STATUS_BACKCHARGING;
#else
				spa_set_charge(spa_power_iter, SPA_CMD_DISCHARGE);
				spa_power_iter->charging_status.phase = POWER_SUPPLY_STATUS_FULL;
				spa_power_iter->charging_status.status = SPA_STATUS_NONE;
				spa_stop_charge_timer(spa_power_iter->charging_status, spa_power_iter);
#endif
				pr_spa_dbg(LEVEL1, "%s : Do full charged - normal full \n", __func__);
			}
			else if(spa_power_iter->charging_status.phase == POWER_SUPPLY_STATUS_FULL)
			{
				if(status == SPA_STATUS_FULL_RECHARGE)
				{ // recharging after full-charged
					spa_set_charge(spa_power_iter, SPA_CMD_CHARGE);
					spa_power_iter->charging_status.status = SPA_STATUS_FULL_RECHARGE;
					spa_start_charge_timer(charge_timer_duration, spa_power_iter);
					pr_spa_dbg(LEVEL1, "%s : Do recharging after full charged\n", __func__);
				} else { /* full-charged after recharging. */
#if 0  //defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
					spa_set_charge(spa_power_iter, SPA_CMD_DELAYED_DISCHARGE);
					spa_power_iter->charging_status.status = SPA_STATUS_FULL_RECHARGE_BACK_CHARGING;
#else
					spa_set_charge(spa_power_iter, SPA_CMD_DISCHARGE);
					spa_power_iter->charging_status.status = SPA_STATUS_NONE;
#endif
					spa_stop_charge_timer(spa_power_iter->charging_status, spa_power_iter);
					pr_spa_dbg(LEVEL1, "%s : Do full charged - recharge full\n", __func__);
				}
			}
		} else if (phase == POWER_SUPPLY_STATUS_NOT_CHARGING) /* NOT CHARGING */
		{
			pr_spa_dbg(LEVEL1, "%s : Do not charging\n", __func__);
			spa_set_charge(spa_power_iter, SPA_CMD_DISCHARGE);
			spa_power_iter->charging_status.phase = POWER_SUPPLY_STATUS_NOT_CHARGING;
			if (status == SPA_STATUS_SUSPEND_OVP) {
				spa_power_iter->charging_status.status = SPA_STATUS_SUSPEND_OVP;
				spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
			} else if (status == SPA_STATUS_SUSPEND_TEMP_HOT) {
				spa_power_iter->charging_status.status = SPA_STATUS_SUSPEND_TEMP_HOT;
				spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_OVERHEAT;
			} else if (status == SPA_STATUS_SUSPEND_TEMP_COLD) {
				spa_power_iter->charging_status.status = SPA_STATUS_SUSPEND_TEMP_COLD;
				spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_COLD;
			} else if (status == SPA_STATUS_VF_INVALID) {
				spa_power_iter->charging_status.status = SPA_STATUS_VF_INVALID;
				spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
			}

			spa_stop_charge_timer(spa_power_iter->charging_status, spa_power_iter);

			spa_update_power_supply_battery(spa_power_iter, POWER_SUPPLY_PROP_HEALTH);

			pr_spa_dbg(LEVEL1, "%s : Do suspend, reason = %d\n", __func__, status);
		}
	} else if (machine == SPA_MACHINE_FULL_CHARGE_TIMER) {
		if (phase == POWER_SUPPLY_STATUS_FULL) {
			if (status == SPA_STATUS_FULL_FORCE) /* FORCE FULL */
			{
				spa_set_charge(spa_power_iter, SPA_CMD_DISCHARGE);
				if(spa_power_iter->batt_info.capacity == 100)	// hw require new spec
					spa_power_iter->charging_status.phase = POWER_SUPPLY_STATUS_FULL;
				spa_power_iter->charging_status.status = SPA_STATUS_FULL_FORCE;
				pr_spa_dbg(LEVEL1, "%s : Do full charged - force full\n", __func__);
			} else if (status == SPA_STATUS_FULL_RECHARGE) /* RECHARGE AFTER FORCE FULL */
			{
				spa_set_charge(spa_power_iter, SPA_CMD_CHARGE);
				spa_power_iter->charging_status.status = SPA_STATUS_FULL_RECHARGE;
				pr_spa_dbg(LEVEL1, "%s : Do recharging after full charged\n", __func__);
			}
		}
	} else if (machine == SPA_MACHINE_SIOP) {
		if (phase == POWER_SUPPLY_STATUS_CHARGING) {
			if((spa_power_iter->charging_status.phase == POWER_SUPPLY_STATUS_CHARGING)||
				(spa_power_iter->charging_status.status == SPA_STATUS_FULL_RECHARGE))
			spa_set_charge(spa_power_iter, SPA_CMD_CHARGE);
		}
		else if(phase == POWER_SUPPLY_STATUS_DISCHARGING) {	
			spa_set_charge(spa_power_iter, SPA_CMD_DISCHARGE);
		}
		else {
			//error;
		}
	} else { /* wrong case */
	}
	spa_update_power_supply_battery(spa_power_iter, POWER_SUPPLY_PROP_STATUS);
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);

	return 0;
}

static int spa_start_charge_timer(charge_timer_t duration, void *data)
{
	struct spa_power_desc *spa_power_iter = (struct spa_power_desc *)data;
	unsigned char is_start_timer = 1;

	pr_spa_dbg(LEVEL1, "%s : enter. Duration: %d, is_start_timer = %d\n",
					__func__, duration, is_start_timer);

	switch(duration)
	{
		case CHARGE_TIMER_90MIN:
			spa_power_iter->charger_info.charge_expire_time = 90 * MINUTE_BY_MSEC;
			break;
		case CHARGE_TIMER_5HOUR:
			spa_power_iter->charger_info.charge_expire_time = 5 * HOUR_BY_MSEC;
			break;
		case CHARGE_TIMER_6HOUR:
			spa_power_iter->charger_info.charge_expire_time = 6 * HOUR_BY_MSEC;
			break;
		case CHARGE_TIMER_10HOUR:
			spa_power_iter->charger_info.charge_expire_time = 10 * HOUR_BY_MSEC;
			break;

		default:
			is_start_timer = 0;
			break;
	}

	if (is_start_timer == 1)
	{
		cancel_delayed_work_sync(&spa_power_iter->spa_expire_charge_work);
		schedule_delayed_work(&spa_power_iter->spa_expire_charge_work,
				msecs_to_jiffies(spa_power_iter->charger_info.charge_expire_time));
		pr_spa_dbg(LEVEL2, "%s : start full charge timer\n", __func__);
	} else {
		return -1;
	}

	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return 0;
}

static void spa_stop_charge_timer(SPA_CHARGING_STATUS_T endtype, void *data)
{
	struct spa_power_desc *spa_power_iter = (struct spa_power_desc *)data;

	pr_spa_dbg(LEVEL1, "%s : enter\n", __func__);

	cancel_delayed_work_sync(&spa_power_iter->spa_expire_charge_work);

	pr_spa_dbg(LEVEL2, "%s : stop full charge timer\n", __func__);
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
}

static void spa_expire_charge_timer(struct work_struct *work)
{
	struct spa_power_desc *spa_power_iter = container_of(work,
			struct spa_power_desc, spa_expire_charge_work.work);

	volatile unsigned int times_expired=0;

	pr_spa_dbg(LEVEL1, "%s : enter\n", __func__);

	spa_power_iter->charger_info.times_expired++;
	times_expired = spa_power_iter->charger_info.times_expired % 2;

	if (times_expired == 0) /* after first expired, duration is 90 mins */
	{
		spa_power_iter->charger_info.charge_expire_time = 90 * MINUTE_BY_MSEC;
		spa_do_status(spa_power_iter, SPA_MACHINE_FULL_CHARGE_TIMER, POWER_SUPPLY_STATUS_FULL, SPA_STATUS_FULL_RECHARGE);
	} else if (times_expired == 1) /* preparing next timer, 30 secs delay */
	{
		spa_power_iter->charger_info.charge_expire_time = 30 * SECOND_BY_MSEC;
		spa_do_status(spa_power_iter, SPA_MACHINE_FULL_CHARGE_TIMER,  POWER_SUPPLY_STATUS_FULL, SPA_STATUS_FULL_FORCE);
	}
	schedule_delayed_work(&spa_power_iter->spa_expire_charge_work,
			msecs_to_jiffies(spa_power_iter->charger_info.charge_expire_time));
	pr_spa_dbg(LEVEL1, "%s : leave. charge_expire_time = %d\n", __func__,
			spa_power_iter->charger_info.charge_expire_time);
}

#if defined(SPA_FAKE_FULL_CAPACITY)
static int spa_fake_capacity(struct spa_power_desc *spa_power_iter)
{
	int fake_capacity=(spa_power_iter->batt_info.fake_capacity > 100) ? 100:spa_power_iter->batt_info.fake_capacity;
	switch(spa_power_iter->batt_info.fakemode)
	{
		case SPA_FAKE_CAP_NONE:
			break;
		case SPA_FAKE_CAP_DEC:
			fake_capacity = spa_power_iter->batt_info.fake_capacity--;
			pr_spa_dbg(LEVEL2, "%s : local f_cap = %d, f_cap=%d\n", __func__, fake_capacity, spa_power_iter->batt_info.fake_capacity);
			if (spa_power_iter->batt_info.capacity >= fake_capacity)
				spa_power_iter->batt_info.fakemode=SPA_FAKE_CAP_NONE;
			break;
		case SPA_FAKE_CAP_INC:
			if (spa_power_iter->batt_info.capacity >= spa_power_iter->batt_info.fake_capacity)
				spa_power_iter->batt_info.fakemode=SPA_FAKE_CAP_NONE;
			break;
	}

	if (spa_power_iter->batt_info.fakemode == SPA_FAKE_CAP_NONE)
		return spa_power_iter->batt_info.capacity;
	else
		return fake_capacity;
}
#endif

static void spa_update_batt_info(struct spa_power_desc *spa_power_iter, unsigned int prop)
{
	struct power_supply *ps;
	union power_supply_propval value;
	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	ps = power_supply_get_by_name(POWER_SUPPLY_BATTERY);

	switch(prop)
	{
		case POWER_SUPPLY_PROP_STATUS:
			value.intval = spa_power_iter->charging_status.phase;
			ps->set_property(ps, POWER_SUPPLY_PROP_STATUS, &value);
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			value.intval= spa_power_iter->batt_info.voltage;
			pr_spa_dbg(LEVEL2, "%s : voltage = %d\n", __func__, spa_power_iter->batt_info.voltage);
			ps->set_property(ps, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
#if defined(SPA_FAKE_FULL_CAPACITY)
			value.intval = spa_fake_capacity(spa_power_iter);
#else
			value.intval = spa_power_iter->batt_info.capacity;
#endif
			if (spa_power_iter->charging_status.phase == POWER_SUPPLY_STATUS_FULL) {
				value.intval = 100;
				pr_spa_dbg(LEVEL1, "%s : notified capacity=%d\n",
                         __func__, value.intval);
			}
#if 0
			if(spa_power_iter->charger_info.charger_type != POWER_SUPPLY_TYPE_BATTERY && spa_power_iter->batt_info.vf_status == 0 )
			{ // batterry removed
#if defined(REPORT_0_WHEN_NOBATT)
				value.intval = 0;
#else
		/* To avoid low battery warning when Power supply connected */
				if (value.intval < 16)
					value.intval = 16;
#endif
			}
#endif
			ps->set_property(ps, POWER_SUPPLY_PROP_CAPACITY, &value);
			break;
		case POWER_SUPPLY_PROP_TEMP:
			value.intval = spa_power_iter->batt_info.temp;
			ps->set_property(ps, POWER_SUPPLY_PROP_TEMP, &value);
			break;
		case POWER_SUPPLY_PROP_BATT_TEMP_ADC:
			value.intval = spa_power_iter->batt_info.temp_adc;
			ps->set_property(ps, POWER_SUPPLY_PROP_BATT_TEMP_ADC, &value);
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			value.intval = spa_power_iter->batt_info.health;
			ps->set_property(ps, POWER_SUPPLY_PROP_HEALTH, &value);
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			if(spa_power_iter->charging_status.status == SPA_STATUS_VF_INVALID)
				value.intval = 0;
			else
				value.intval = 1;

			ps->set_property(ps, POWER_SUPPLY_PROP_PRESENT, &value);
			break;
	}
}

static void spa_update_power_supply_battery(struct spa_power_desc *spa_power_iter, unsigned int prop)
{
	struct power_supply *ps;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	ps = power_supply_get_by_name(POWER_SUPPLY_BATTERY);

	spa_update_batt_info(spa_power_iter, prop);

	power_supply_changed(ps);
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
}

static void spa_update_power_supply_charger(struct spa_power_desc *spa_power_iter, unsigned int prop)
{
	struct power_supply *ps;
	union power_supply_propval value;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	if (spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_MAINS ||
			spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_USB_DCP) {
		ps = power_supply_get_by_name(POWER_SUPPLY_WALL);
		value.intval = 1;
		ps->set_property(ps, POWER_SUPPLY_PROP_ONLINE, &value);
		pr_spa_dbg(LEVEL1, "%s : Charger Online : WALL TYPE\n", __func__);
	} else if (spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_USB ||
			spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_USB_CDP) {
		ps = power_supply_get_by_name(POWER_SUPPLY_USB);
		value.intval = 1;
		ps->set_property(ps, POWER_SUPPLY_PROP_ONLINE, &value);
		pr_spa_dbg(LEVEL1, "%s : Charger Online : USB TYPE\n", __func__);
	}
	else if(spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_USB_ACA)
	{
		ps = power_supply_get_by_name(POWER_SUPPLY_WALL);
		value.intval = 1;
		ps->set_property(ps, POWER_SUPPLY_PROP_ONLINE, &value);
		pr_spa_dbg(LEVEL1, "%s : Charger Online : ACA TYPE\n", __func__);
	}
	else
	{
		ps = power_supply_get_by_name(POWER_SUPPLY_WALL);
		value.intval = 0;
		ps->set_property(ps, POWER_SUPPLY_PROP_ONLINE, &value);
		power_supply_changed(ps);

		ps = power_supply_get_by_name(POWER_SUPPLY_USB);
		value.intval = 0;
		ps->set_property(ps, POWER_SUPPLY_PROP_ONLINE, &value);

		pr_spa_dbg(LEVEL1, "%s : Charger Offline\n", __func__);
	}


	power_supply_changed(ps);
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
}

static void spa_fast_charging_work(struct work_struct *work)
{
	struct spa_power_desc *spa_power_iter = container_of(work,
						struct spa_power_desc, fast_charging_work.work);
	struct power_supply *ps;
	union power_supply_propval value;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	ps = power_supply_get_by_name(spa_power_iter->charger_info.charger_name);


	value.intval = spa_power_iter->charger_info.charging_current;
	ps->set_property(ps, POWER_SUPPLY_PROP_CURRENT_NOW, &value);

	/* 2. eoc current*/
	value.intval = spa_power_iter->charger_info.eoc_current;
	ps->set_property(ps, POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &value);

	/*3. charging now.*/
	value.intval = POWER_SUPPLY_STATUS_CHARGING;
	ps->set_property(ps, POWER_SUPPLY_PROP_STATUS, &value);

	pr_spa_dbg(LEVEL2, "%s : Fast Charging!! current=%d, eoc_cur=%d\n",\
		__func__, spa_power_iter->charger_info.charging_current,
				spa_power_iter->charger_info.eoc_current);
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
}

static void spa_batt_work(struct work_struct *work)
{
	struct spa_power_desc *spa_power_iter = container_of(work,
							struct spa_power_desc, battery_work.work);
#if !defined(SPA_TEMPERATURE_INT)
	struct spa_power_data *pdata = spa_power_iter->pdata;
#endif

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	if (spa_power_iter->dbg_simul != 1) {
		if(spa_power_iter->init_progress > SPA_INIT_PROGRESS_DONE)
		{
			spa_power_iter->batt_info.temp = spa_get_batt_temp(spa_power_iter);
			spa_power_iter->batt_info.voltage = spa_get_batt_voltage(spa_power_iter);
		}
		else
		{
			spa_power_iter->batt_info.temp = spa_get_batt_temp_avg(spa_power_iter);
			spa_power_iter->batt_info.voltage = spa_get_batt_volt_avg(spa_power_iter);
		}

		spa_power_iter->batt_info.temp_adc = spa_get_batt_temp_adc(spa_power_iter);
		spa_power_iter->batt_info.capacity = spa_get_batt_capacity(spa_power_iter);
		spa_power_iter->batt_info.vf_status = spa_get_batt_vf_status(spa_power_iter);
	}

	pr_spa_dbg(LEVEL1, "%s : temp=%d, temp_adc=%d, capacity=%d, voltage=%d, vf_status=%d\n", __func__,
			spa_power_iter->batt_info.temp,
			spa_power_iter->batt_info.temp_adc,
			spa_power_iter->batt_info.capacity,
			spa_power_iter->batt_info.voltage,
			spa_power_iter->batt_info.vf_status);
	pr_spa_dbg(LEVEL1, "%s : charger type = %d, charging status =  %d, %d\n", __func__,
			spa_power_iter->charger_info.charger_type,
			spa_power_iter->charging_status.phase, spa_power_iter->charging_status.status);


	//YJ.Choi put below code temporary to avoid not power off device with 0% capacity
	if(spa_power_iter->batt_info.capacity == 0)
		wake_lock_timeout(&spa_power_iter->spa_holdwakelock, 10*HZ);

	// update power supply battery,
	spa_update_power_supply_battery(spa_power_iter, POWER_SUPPLY_PROP_TEMP);
	spa_update_power_supply_battery(spa_power_iter, POWER_SUPPLY_PROP_BATT_TEMP_ADC);
	spa_update_power_supply_battery(spa_power_iter, POWER_SUPPLY_PROP_CAPACITY);
	spa_update_power_supply_battery(spa_power_iter, POWER_SUPPLY_PROP_VOLTAGE_NOW);
	spa_update_power_supply_battery(spa_power_iter, POWER_SUPPLY_PROP_HEALTH);
	spa_update_power_supply_battery(spa_power_iter,
						POWER_SUPPLY_PROP_PRESENT);

	/* check recharge condition */
	if (spa_power_iter->charging_status.phase == POWER_SUPPLY_STATUS_FULL) {
		if (spa_power_iter->charging_status.status != SPA_STATUS_FULL_RECHARGE
				&& spa_power_iter->charging_status.status != SPA_STATUS_FULL_FORCE
				&& spa_power_iter->charging_status.status != SPA_STATUS_BACKCHARGING){
				if (spa_power_iter->batt_info.voltage < spa_power_iter->charger_info.recharge_voltage) {
						/* recharge condition has met. */
						spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, spa_power_iter->charging_status.phase, SPA_STATUS_FULL_RECHARGE);
				}
		}
	}

	/* check temperature */
#if !defined(SPA_TEMPERATURE_INT)
#if defined(CONFIG_TARGET_LOCALE_USA)
	if( spa_power_iter->charger_info.charger_type != POWER_SUPPLY_TYPE_BATTERY &&
		spa_power_iter->charging_status.status != SPA_STATUS_SUSPEND_TEMP_HOT &&
			spa_power_iter->batt_info.temp > pdata->suspend_temp_hot) { /* hot temperature */
		spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_NOT_CHARGING, SPA_STATUS_SUSPEND_TEMP_HOT);
	} else if (spa_power_iter->charger_info.charger_type != POWER_SUPPLY_TYPE_BATTERY &&
		spa_power_iter->charging_status.status != SPA_STATUS_SUSPEND_TEMP_COLD &&
			spa_power_iter->batt_info.temp < pdata->suspend_temp_cold) { /* cold temperature */
		spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_NOT_CHARGING, SPA_STATUS_SUSPEND_TEMP_COLD);
	} else if (spa_power_iter->charging_status.status == SPA_STATUS_SUSPEND_TEMP_HOT &&
			spa_power_iter->batt_info.temp < pdata->recovery_temp_hot) { /* recovery from hot temperature */
		spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_GOOD;
		spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_CHARGING, SPA_STATUS_NONE);
	} else if (spa_power_iter->charging_status.status == SPA_STATUS_SUSPEND_TEMP_COLD &&
			spa_power_iter->batt_info.temp > pdata->recovery_temp_cold) { /* recovery from hot temperature */
		spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_GOOD;
		spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_CHARGING, SPA_STATUS_NONE);
	}
#else
		if( spa_power_iter->charger_info.charger_type != POWER_SUPPLY_TYPE_BATTERY &&
			spa_power_iter->charging_status.status != SPA_STATUS_SUSPEND_TEMP_HOT &&
				spa_power_iter->batt_info.temp > pdata->suspend_temp_hot)
		{ // hot temperature
			spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_NOT_CHARGING, SPA_STATUS_SUSPEND_TEMP_HOT);
		}
		else if( spa_power_iter->charger_info.charger_type != POWER_SUPPLY_TYPE_BATTERY &&
			spa_power_iter->charging_status.status != SPA_STATUS_SUSPEND_TEMP_COLD &&
				spa_power_iter->batt_info.temp < pdata->suspend_temp_cold)
		{ // cold temperature
			spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_NOT_CHARGING, SPA_STATUS_SUSPEND_TEMP_COLD);
		}
		else if( spa_power_iter->charging_status.status == SPA_STATUS_SUSPEND_TEMP_HOT &&
				spa_power_iter->batt_info.temp < pdata->recovery_temp_hot)
		{ // recovery from hot temperature
			spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_GOOD;
			spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_CHARGING, SPA_STATUS_NONE);
		}
		else if( spa_power_iter->charging_status.status == SPA_STATUS_SUSPEND_TEMP_COLD &&
				spa_power_iter->batt_info.temp > pdata->recovery_temp_cold)
		{ // recovery from hot temperature
			spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_GOOD;
			spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_CHARGING, SPA_STATUS_NONE);
		}
#endif

#endif

	/* check vf */
	if (spa_power_iter->charger_info.charger_type != POWER_SUPPLY_TYPE_BATTERY
		&& spa_power_iter->batt_info.vf_status == 0) {
		/* vf is not valid */
		pr_spa_dbg(LEVEL1, "%s : vf=%d, battery removed\n", __func__, spa_power_iter->batt_info.vf_status);
		spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_NOT_CHARGING, SPA_STATUS_VF_INVALID);
	}

	schedule_delayed_work(&spa_power_iter->battery_work,
			msecs_to_jiffies(spa_power_iter->batt_info.update_interval));
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
}

int spa_event_handler(int evt, void *data)
{
	struct spa_power_desc *spa_power_iter = g_spa_power;
	int ret = 0;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	if(spa_power_iter == NULL || probe_status != SPA_PROBE_STATUS_READY )
	{ // not initialised yet. queue the event to be handled surely.
		pr_spa_dbg(LEVEL2, "%s : event has come before init.\n",__func__);
		return -1;
	}

	spa_evt_log[(spa_evt_idx++)%255].evt=evt;
	spa_evt_log[(spa_evt_idx)%255].data = (int)data;

	switch(evt)
	{
		case SPA_EVT_CHARGER:
			/* Insert
			 * get charter type
			 * start charging (set eoc current, charging current by charger type)
			 * run full charge timer
			 * change update interval for battery and reschedule.
			 * set status.
			 * set wakelock to avoid to enter sleep
			 */
			spa_power_iter->charger_info.charger_type = (int)data;
			pr_spa_dbg(LEVEL1, "%s : SPV_EVT_CHARGER - %d\n", __func__, spa_power_iter->charger_info.charger_type);

			if (spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_BATTERY) { /* remove case */
				spa_update_power_supply_charger(spa_power_iter, POWER_SUPPLY_PROP_ONLINE);
#if defined(SPA_FAKE_FULL_CAPACITY)
				if (spa_power_iter->charging_status.phase == POWER_SUPPLY_STATUS_FULL) {
					if (spa_power_iter->batt_info.capacity < 100) {
						spa_power_iter->batt_info.fakemode=SPA_FAKE_CAP_DEC;
						spa_power_iter->batt_info.fake_capacity=100;
					}
				} else if (spa_power_iter->batt_info.fakemode != SPA_FAKE_CAP_NONE)
					spa_power_iter->batt_info.fakemode = (spa_power_iter->batt_info.fakemode == SPA_FAKE_CAP_DEC) ? SPA_FAKE_CAP_INC : SPA_FAKE_CAP_DEC;
#endif
				wake_lock_timeout(&spa_power_iter->spa_wakelock, 3*HZ);

				spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_DISCHARGING, SPA_STATUS_NONE);
			} else { /* insert case */
				spa_update_power_supply_charger(spa_power_iter, POWER_SUPPLY_PROP_ONLINE);
#if defined(SPA_FAKE_FULL_CAPACITY)
				if (spa_power_iter->batt_info.fakemode != SPA_FAKE_CAP_NONE) {
					spa_power_iter->batt_info.fakemode = (spa_power_iter->batt_info.fakemode == SPA_FAKE_CAP_DEC) ? SPA_FAKE_CAP_INC : SPA_FAKE_CAP_DEC;
					if (spa_power_iter->batt_info.fakemode == SPA_FAKE_CAP_INC && spa_power_iter->batt_info.fake_capacity < 100) {
						spa_power_iter->batt_info.fake_capacity++;
					}
				}
#endif
				spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_CHARGING, SPA_STATUS_NONE);
				wake_lock(&spa_power_iter->spa_wakelock);
			}

			break;
		case SPA_EVT_EOC:
			/* stop charging
			 * stop full charge timer
			 * start watching recharge voltage.
			 * set status as full charged with capacity.
			 */
			if (spa_power_iter->charger_info.charger_type != POWER_SUPPLY_TYPE_BATTERY) {
				pr_spa_dbg(LEVEL1, "%s : SPV_EVT_EOC\n", __func__);

			/* ugly patch for recharge feature due to capacity wrong action. */
			if (spa_power_iter->batt_info.capacity >= 98)
				spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_FULL, SPA_STATUS_NONE);
			} else {
				pr_spa_dbg(LEVEL1, "%s : wrong eoc, charger is not connected!!\n", __func__);
				ret = -1;
			}
			break;
		case SPA_EVT_TEMP:
			/* stop charging */
			/* set status as suspend */
			if (spa_power_iter->charger_info.charger_type != POWER_SUPPLY_TYPE_BATTERY) {
				int temp_status = (int)data;
				pr_spa_dbg(LEVEL1, "%s : SPV_EVT_TEMP - %d\n", __func__, temp_status);
				if (temp_status == POWER_SUPPLY_HEALTH_OVERHEAT)
					spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_NOT_CHARGING, SPA_STATUS_SUSPEND_TEMP_HOT);
				if (temp_status == POWER_SUPPLY_HEALTH_COLD)
					spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_NOT_CHARGING, SPA_STATUS_SUSPEND_TEMP_COLD);
				if (temp_status == POWER_SUPPLY_HEALTH_GOOD) {
					spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_GOOD;
					spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_CHARGING, SPA_STATUS_NONE);
				}
			}
			break;
		case SPA_EVT_OVP:
			if (spa_power_iter->charger_info.charger_type != POWER_SUPPLY_TYPE_BATTERY) {
				int ovp_status = (int)data;
				pr_spa_dbg(LEVEL1, "%s : SPV_EVT_OVP - %d\n", __func__, ovp_status);

				if (ovp_status == 1) {
					spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_DISCHARGING, SPA_STATUS_SUSPEND_OVP);
				} else {
					spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_GOOD;
					spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_CHARGING, SPA_STATUS_NONE);
				}
			}
			break;
		case SPA_EVT_LOWBATT:
			break;
		case SPA_EVT_CAPACITY:
			{
				if(spa_power_iter->dbg_simul != 1)
				{
					if(spa_power_iter->init_progress==SPA_INIT_PROGRESS_DONE)
					spa_power_iter->batt_info.capacity = (int)data;
				}
			}
			break;
	}
	cancel_delayed_work_sync(&spa_power_iter->battery_work);
	schedule_delayed_work(&spa_power_iter->battery_work, msecs_to_jiffies(500));
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return ret;
}
EXPORT_SYMBOL(spa_event_handler);

static void spa_init_config(struct spa_power_desc *spa_power_iter)
{
	struct spa_power_data *pdata =
		(struct spa_power_data *)spa_power_iter->pdata;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);
	/* charger init values */
	spa_power_iter->charger_info.charger_name=pdata->charger_name;
	spa_power_iter->charger_info.charger_type = POWER_SUPPLY_TYPE_BATTERY;
	spa_power_iter->charger_info.charging_current=0;
	spa_power_iter->charger_info.eoc_current = pdata->eoc_current;
	spa_power_iter->charger_info.recharge_voltage = pdata->recharge_voltage;
	spa_power_iter->charger_info.lowbatt_voltage=3400;
	spa_power_iter->charger_info.top_voltage = pdata->regulated_vol;
	spa_power_iter->charger_info.charge_expire_time = (5 * HOUR_BY_MSEC);
	spa_power_iter->charger_info.times_expired = 0;
	spa_power_iter->charging_status.phase = POWER_SUPPLY_STATUS_DISCHARGING;
	spa_power_iter->charging_status.status = SPA_STATUS_NONE;
#if defined(CONFIG_TARGET_LOCALE_USA)
	if(lp_boot_mode)
	{
		spa_power_iter->charger_info.suspend_temp_cold
			= pdata->lpm_suspend_temp_cold;
		spa_power_iter->charger_info.recovery_temp_cold
			= pdata->lpm_recovery_temp_cold;
		spa_power_iter->charger_info.suspend_temp_hot
			= pdata->lpm_suspend_temp_hot;
		spa_power_iter->charger_info.recovery_temp_hot
			= pdata->lpm_recovery_temp_hot;
	}else{
		spa_power_iter->charger_info.suspend_temp_cold
			= pdata->suspend_temp_cold;
		spa_power_iter->charger_info.recovery_temp_cold
			= pdata->recovery_temp_cold;
		spa_power_iter->charger_info.suspend_temp_hot
			= pdata->suspend_temp_hot;
		spa_power_iter->charger_info.recovery_temp_hot
			= pdata->recovery_temp_hot;
	}
#endif
	/* battery init values
	 * should not call function, some function are still not assigned at this time in bcmpmu.
	 * filling values would be done in work.
	*/
	spa_power_iter->batt_info.temp=0;
	spa_power_iter->batt_info.temp_adc=0;
	spa_power_iter->batt_info.type = "SDI_SDI";
	spa_power_iter->batt_info.health = POWER_SUPPLY_HEALTH_GOOD;
	spa_power_iter->batt_info.capacity = 1;
	spa_power_iter->batt_info.technology = POWER_SUPPLY_TECHNOLOGY_LION;
	spa_power_iter->batt_info.voltage = 0;
	spa_power_iter->batt_info.vf_status = 1;
	spa_power_iter->batt_info.update_interval=SPA_BATT_UPDATE_INTERVAL_INIT;

	spa_power_iter->init_progress=SPA_INIT_PROGRESS_START;

#if defined(SPA_FAKE_FULL_CAPACITY)
	spa_power_iter->batt_info.fakemode=SPA_FAKE_CAP_NONE;
	spa_power_iter->batt_info.fake_capacity=100;
#endif

	/* lp charging */
	spa_power_iter->lp_charging = lp_boot_mode;
	pr_spa_dbg(LEVEL1, "%s : lpcharging=%d, lp_boot_mode=%d\n", __func__, spa_power_iter->lp_charging, lp_boot_mode);

	/* init containers */
	spa_power_iter->volt_reading.prev_val=0;

	spa_power_iter->new_gathering.temperature=1;
	spa_power_iter->new_gathering.voltage=1;

	spa_power_iter->dbg_lvl=SPA_DBG_LEVEL_OUT;
	spa_power_iter->dbg_simul=0;


	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
}

static int spa_init_progress(struct spa_power_desc *spa_power_iter)
{

	switch(spa_power_iter->init_progress)
	{
		case SPA_INIT_PROGRESS_STEP5:
			spa_power_iter->batt_info.update_interval = SPA_BATT_UPDATE_INTERVAL_INIT5;
			break;
		case SPA_INIT_PROGRESS_STEP4:
			spa_power_iter->batt_info.update_interval = SPA_BATT_UPDATE_INTERVAL_INIT4;
			spa_power_iter->charger_info.charger_type = spa_get_charger_type(spa_power_iter);
			break;
		case SPA_INIT_PROGRESS_STEP3:
			spa_power_iter->batt_info.update_interval = SPA_BATT_UPDATE_INTERVAL_INIT3;
			break;
		case SPA_INIT_PROGRESS_STEP2:
			spa_power_iter->batt_info.update_interval = SPA_BATT_UPDATE_INTERVAL_INIT2;
			break;
		case SPA_INIT_PROGRESS_STEP1:
			spa_power_iter->batt_info.update_interval = SPA_BATT_UPDATE_INTERVAL_INIT1;
			break;
		case SPA_INIT_PROGRESS_STEP0:
		default:
			spa_power_iter->batt_info.update_interval = SPA_BATT_UPDATE_INTERVAL;
			break;
	}

	if (spa_power_iter->init_progress <= SPA_INIT_PROGRESS_DONE)
		return SPA_INIT_PROGRESS_DONE;

	return spa_power_iter->init_progress--;
}

static void spa_delayed_init_work(struct work_struct *work)
{

	unsigned int init_progress;
	struct spa_power_desc *spa_power_iter = container_of(work,
						struct spa_power_desc, delayed_init_work.work);

	struct power_supply *ps;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);
	ps = power_supply_get_by_name("battery");

	if (ps == NULL) {
		pr_spa_dbg(LEVEL2, "%s : waiting spa_ps\n", __func__);
		schedule_delayed_work(&spa_power_iter->delayed_init_work, msecs_to_jiffies(50));
		return;
	}

	init_progress = spa_init_progress(spa_power_iter);

	if (SPA_INIT_PROGRESS_START == init_progress) {
		pr_spa_dbg(LEVEL1, "%s : SPA_INIT_PROGRESS_START\n",__func__);
		/*MUIC init time is after spa_power init(sub initcall)*/
#if 0
		spa_power_iter->charger_info.charger_type = spa_get_charger_type(spa_power_iter);

		if(spa_power_iter->charger_info.charger_type != POWER_SUPPLY_TYPE_BATTERY)
		{
			spa_event_handler(SPA_EVT_CHARGER, (void *)(spa_power_iter->charger_info.charger_type));
#ifdef CONFIG_BATTERY_D2153
			d2153_battery_set_status(D2153_STATUS_CHARGING, 1);
#endif
		}
#ifdef CONFIG_BATTERY_D2153
		else
			d2153_battery_set_status(D2153_STATUS_CHARGING, 0);
#endif
		// dummy, temporary before actual charger detection in case of power off charging
		if(spa_power_iter->lp_charging == 1 && spa_power_iter->charger_info.charger_type == POWER_SUPPLY_TYPE_BATTERY)
		{
			spa_power_iter->charger_info.charger_type = POWER_SUPPLY_TYPE_USB;
		}
#endif
#if 0 //def CONFIG_BATTERY_D2153
		if(spa_power_iter->lp_charging)
			d2153_battery_set_status(D2153_STATUS_CHARGING, 1);
		else
			d2153_battery_set_status(D2153_STATUS_CHARGING, 0);
#endif
		schedule_delayed_work(&spa_power_iter->battery_work,
			msecs_to_jiffies(0));
		schedule_delayed_work(&spa_power_iter->delayed_init_work,
			msecs_to_jiffies( (unsigned int)(SPA_INIT_PROGRESS_DURATION / SPA_INIT_PROGRESS_START)) );
	}
	else if(SPA_INIT_PROGRESS_DONE == init_progress)
	{
		if(spa_power_iter->charging_status.phase == POWER_SUPPLY_STATUS_CHARGING
			|| spa_power_iter->charging_status.phase == POWER_SUPPLY_STATUS_FULL)
		{
		    spa_power_iter->batt_info.update_interval = SPA_BATT_UPDATE_INTERVAL_WHILE_CHARGING;
		    cancel_delayed_work_sync(&spa_power_iter->battery_work);
		    schedule_delayed_work(&spa_power_iter->battery_work, msecs_to_jiffies(0));
			pr_spa_dbg(LEVEL1, "%s : SPA_INIT_PROGRESS_DONE charging\n",__func__);
		}
		else if(spa_power_iter->charging_status.phase == POWER_SUPPLY_STATUS_DISCHARGING)
		{
			spa_do_status(spa_power_iter, SPA_MACHINE_NORMAL, POWER_SUPPLY_STATUS_DISCHARGING, SPA_STATUS_NONE);
			pr_spa_dbg(LEVEL1, "%s : SPA_INIT_PROGRESS_DONE discharging\n",__func__);
		}
		pr_spa_dbg(LEVEL1, "%s : SPA_INIT_PROGRESS_DONE\n",__func__);
	}
	else
	{
		schedule_delayed_work(&spa_power_iter->delayed_init_work,
				msecs_to_jiffies((unsigned int)(SPA_INIT_PROGRESS_DURATION / SPA_INIT_PROGRESS_START)));
	}
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);

}

extern int spa_ps_init(struct platform_device *pdev);
static int spa_power_probe(struct platform_device *pdev)
{
	int ret=0;
	struct spa_power_desc *spa_power_iter=NULL;

	pr_spa_dbg(LEVEL3,"%s : enter \n", __func__);

	ret = spa_ps_init(pdev);
	if(ret < 0)
	{
		pr_spa_dbg(LEVEL1, "%s: faile to init spa_ps\n", __func__);
	}
	spa_power_iter = kzalloc(sizeof(struct spa_power_desc), GFP_KERNEL);
	if (spa_power_iter == NULL) {
		pr_err("%s : memory allocation failure\n", __func__);
		return -ENOMEM;
	}

	g_spa_power=spa_power_iter;
	spa_power_iter->pdata=(struct spa_power_data *)pdev->dev.platform_data;

	/* Initialsing wakelock */
	wake_lock_init(&spa_power_iter->spa_wakelock, WAKE_LOCK_SUSPEND, "spa_charge");
	wake_lock_init(&spa_power_iter->acc_wakelock, WAKE_LOCK_SUSPEND, "acc_wakelock");
	wake_lock_init(&spa_power_iter->spa_holdwakelock, WAKE_LOCK_SUSPEND, "spa_holdpoweroff");

	/* Create workqueue */
	spa_power_iter->spa_workqueue = create_singlethread_workqueue("spa_power_wq");
	if (spa_power_iter->spa_workqueue == NULL) {
		pr_err("%s : Failed to create workqueue\n", __func__);
		ret = -ENOMEM;
		goto label_SPA_POWER_PROBE_ERROR;
	}

	/* Init work */
	INIT_DELAYED_WORK(&spa_power_iter->battery_work, spa_batt_work);
	INIT_DELAYED_WORK(&spa_power_iter->delayed_init_work, spa_delayed_init_work);
	INIT_DELAYED_WORK(&spa_power_iter->fast_charging_work, spa_fast_charging_work);
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	INIT_DELAYED_WORK(&spa_power_iter->back_charging_work, spa_back_charging_work);
#endif
	INIT_DELAYED_WORK(&spa_power_iter->spa_expire_charge_work, spa_expire_charge_timer);

	spa_init_config(spa_power_iter);

	platform_set_drvdata(pdev, spa_power_iter);

	{
		int i = 0;
		for (i = 0; i < ARRAY_SIZE(spa_power_attrs) ; i++) {
			device_create_file(&pdev->dev, &spa_power_attrs[i]);
		}
	}

#if defined(CONFIG_SEC_BATT_EXT_ATTRS)
	{
		struct power_supply *ps;

		ps = power_supply_get_by_name("battery");

		if(ps != NULL)
		{
			int i;
			for(i=0; i < ARRAY_SIZE(ss_batt_ext_attrs) ; i++)
			{
				device_create_file(ps->dev, &ss_batt_ext_attrs[i]);
			}
		}
	}
#endif

	schedule_delayed_work(&spa_power_iter->delayed_init_work, msecs_to_jiffies(50));
	spa_power_iter->batt_info.siop_level = 100;
	probe_status = SPA_PROBE_STATUS_READY;

#ifdef CONFIG_BATTERY_D2153
	d2153_battery_start();
#endif

	goto label_SPA_POWER_PROBE_SUCCESS;

label_SPA_POWER_PROBE_ERROR:
	kfree(spa_power_iter);
	return ret;
label_SPA_POWER_PROBE_SUCCESS:
	pr_spa_dbg(LEVEL3, "%s : leave \n", __func__);

	return 0;
}

static int __devexit spa_power_remove(struct platform_device *pdev)
{
	struct spa_power_desc *spa_power_iter = platform_get_drvdata(pdev);

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);
	cancel_delayed_work_sync(&spa_power_iter->battery_work);
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	cancel_delayed_work_sync(&spa_power_iter->back_charging_work);
#endif
	cancel_delayed_work_sync(&spa_power_iter->spa_expire_charge_work);

	destroy_workqueue(spa_power_iter->spa_workqueue);

	wake_lock_destroy(&spa_power_iter->spa_wakelock);
	wake_lock_destroy(&spa_power_iter->spa_holdwakelock);

	{
		int i=0;
		for(i=0; i < ARRAY_SIZE(spa_power_attrs) ; i++)
		{
			device_remove_file(&pdev->dev, &spa_power_attrs[i]);
		}
	}

	kfree(spa_power_iter);
	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return 0;
}

static int spa_power_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct spa_power_desc *spa_power_iter = g_spa_power;

	pr_spa_dbg(LEVEL4,"%s : enter \n", __func__);
	// To Do :
	if(spa_power_iter)
	{
		cancel_delayed_work_sync(&spa_power_iter->battery_work);
	}
	pr_spa_dbg(LEVEL4, "%s : leave \n", __func__);
	return 0;
}

static int spa_power_resume(struct platform_device *pdev)
{
	struct spa_power_desc *spa_power_iter = g_spa_power;

	pr_spa_dbg(LEVEL4, "%s : enter\n", __func__);

	if (spa_power_iter) {
		spa_power_iter->new_gathering.temperature=1;
		spa_power_iter->new_gathering.voltage=1;
		cancel_delayed_work_sync(&spa_power_iter->battery_work);
		schedule_delayed_work(&spa_power_iter->battery_work, msecs_to_jiffies(0));
	}

	pr_spa_dbg(LEVEL4, "%s : leave\n", __func__);
	return 0;
}

static struct platform_driver spa_power_driver = {
	.probe = spa_power_probe,
	.remove = spa_power_remove,
	.suspend = spa_power_suspend,
	.resume = spa_power_resume,
	.driver = {
		.name = "spa_power",
		.owner = THIS_MODULE,
	},
};

static int __init spa_power_init(void)
{
	int ret = 0;
	ret = platform_driver_register(&spa_power_driver);
	return ret;
}

static void __exit spa_power_exit(void)
{
	platform_driver_unregister(&spa_power_driver);
}

#if 0
//module_init(spa_power_init);
subsys_initcall_sync(spa_power_init);
#else
device_initcall(spa_power_init);
#endif
module_exit(spa_power_exit);

MODULE_AUTHOR("kc45.kim@samsung.com");
MODULE_DESCRIPTION("Samsung Battery and Charger driver for common use");
MODULE_LICENSE("GPL");
