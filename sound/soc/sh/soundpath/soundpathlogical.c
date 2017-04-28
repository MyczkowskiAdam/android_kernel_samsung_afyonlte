/* soundpathlogical.c
 *
 * Copyright (C) 2012-2013 Renesas Mobile Corp.
 * All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define __SOUNDPATHLOGICAL_NO_EXTERN__

#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/pm_runtime.h>
#include <linux/stat.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <mach/pm.h>

/* #include <linux/vcd/vcd.h> */

#include <sound/soundpath/soundpath.h>
#include <sound/soundpath/common_extern.h>
#include <sound/soundpath/scuw_extern.h>
#include <sound/soundpath/fsi_extern.h>
#include <sound/soundpath/clkgen_extern.h>
#include <sound/soundpath/call_extern.h>
#include <sound/sh_fsi.h>
#include <sound/fsi_d2153.h>
#include "soundpathlogical.h"

/*
 *
 * DEFINE Definitions
 *
 */


/*
 *
 * Global Data Declarations
 *
 */

/* Operation mode string */
static const char *const g_sndpdrv_op_modes_texts[] = {
	"PCM"
};

#if 0
/* for SoC */
static const struct soc_enum g_sndpdrv_op_modes_enum
	= SOC_ENUM_SINGLE(0,
			  0,
			  ARRAY_SIZE(g_sndpdrv_op_modes_texts),
			  g_sndpdrv_op_modes_texts);

/* for Kcontrol */
static struct snd_kcontrol_new g_sndpdrv_controls[] = {
	SNDPDRV_SOC_ENUM_EXT("Path", g_sndpdrv_op_modes_enum),
	SNDPDRV_SOC_SINGLE("Earpiece Volume" , 0, 0, SNDPDRV_VOICE_VOL_MAX, 0, sndp_soc_get_voice_out_volume, sndp_soc_put_voice_out_volume),
	SNDPDRV_SOC_SINGLE("Speaker Volume"  , 0, 0, SNDPDRV_VOICE_VOL_MAX, 0, sndp_soc_get_voice_out_volume, sndp_soc_put_voice_out_volume),
	SNDPDRV_SOC_SINGLE("Headphone Volume", 0, 0, SNDPDRV_VOICE_VOL_MAX, 0, sndp_soc_get_voice_out_volume, sndp_soc_put_voice_out_volume),
	SNDPDRV_SOC_SINGLE("Bluetooth Volume", 0, 0, SNDPDRV_VOICE_VOL_MAX, 0, sndp_soc_get_voice_out_volume, sndp_soc_put_voice_out_volume),
	SNDPDRV_SOC_SINGLE("Capture Volume"  , 0, 0, SNDPDRV_VOICE_VOL_MAX, 0, sndp_soc_capture_volume, sndp_soc_capture_volume),
	SNDPDRV_SOC_SINGLE("Capture Switch"  , 0, 0, 1, 0, sndp_soc_get_capture_mute, sndp_soc_put_capture_mute),
	SNDPDRV_SOC_SINGLE("Earpiece Switch" , 0, 0, 1, 0, sndp_soc_get_playback_mute, sndp_soc_put_playback_mute),
};
#endif

/* Mode change table */
const struct sndp_mode_trans g_sndp_mode_map[SNDP_MODE_MAX][SNDP_MODE_MAX] = {
	[SNDP_MODE_INIT][SNDP_MODE_NORMAL]  =	{SNDP_PROC_NO,
							SNDP_MODE_NORMAL},
	[SNDP_MODE_INIT][SNDP_MODE_RING]    =	{SNDP_PROC_NO,
							SNDP_STAT_RINGTONE},
	[SNDP_MODE_INIT][SNDP_MODE_INCALL]  =	{SNDP_PROC_CALL_START,
							SNDP_STAT_IN_CALL},
	[SNDP_MODE_INIT][SNDP_MODE_INCOMM]  =	{SNDP_PROC_INCOMM_START,
							SNDP_STAT_IN_COMM},
	[SNDP_MODE_NORMAL][SNDP_MODE_NORMAL] =	{SNDP_PROC_DEV_CHANGE,
							SNDP_STAT_NOT_CHG},
	[SNDP_MODE_NORMAL][SNDP_MODE_RING]   =	{SNDP_PROC_NO,
							SNDP_STAT_RINGTONE},
	[SNDP_MODE_NORMAL][SNDP_MODE_INCALL] =	{SNDP_PROC_CALL_START,
							SNDP_STAT_IN_CALL},
	[SNDP_MODE_NORMAL][SNDP_MODE_INCOMM] =	{SNDP_PROC_INCOMM_START,
							SNDP_STAT_IN_COMM},
	[SNDP_MODE_RING][SNDP_MODE_NORMAL]   =	{SNDP_PROC_NO,
							SNDP_STAT_NORMAL},
	[SNDP_MODE_RING][SNDP_MODE_RING]     =	{SNDP_PROC_DEV_CHANGE,
							SNDP_STAT_NOT_CHG},
	[SNDP_MODE_RING][SNDP_MODE_INCALL]   =	{SNDP_PROC_CALL_START,
							SNDP_STAT_IN_CALL},
	[SNDP_MODE_RING][SNDP_MODE_INCOMM]   =	{SNDP_PROC_INCOMM_START,
							SNDP_STAT_IN_COMM},
	[SNDP_MODE_INCALL][SNDP_MODE_NORMAL] =	{SNDP_PROC_CALL_STOP,
							SNDP_STAT_NORMAL},
	[SNDP_MODE_INCALL][SNDP_MODE_RING]   =	{SNDP_PROC_CALL_STOP,
							SNDP_STAT_RINGTONE},
	[SNDP_MODE_INCALL][SNDP_MODE_INCALL] =	{SNDP_PROC_DEV_CHANGE,
							SNDP_STAT_NOT_CHG},
	[SNDP_MODE_INCALL][SNDP_MODE_INCOMM] =	{SNDP_PROC_CALL_STOP |
						 SNDP_PROC_INCOMM_START,
							SNDP_STAT_IN_COMM},
	[SNDP_MODE_INCOMM][SNDP_MODE_NORMAL] =	{SNDP_PROC_INCOMM_STOP,
							SNDP_STAT_NORMAL},
	[SNDP_MODE_INCOMM][SNDP_MODE_RING]   =	{SNDP_PROC_INCOMM_STOP,
							SNDP_STAT_RINGTONE},
	[SNDP_MODE_INCOMM][SNDP_MODE_INCALL] =	{SNDP_PROC_INCOMM_STOP |
						 SNDP_PROC_CALL_START,
							SNDP_STAT_IN_CALL},
	[SNDP_MODE_INCOMM][SNDP_MODE_INCOMM] =	{SNDP_PROC_DEV_CHANGE,
							SNDP_STAT_NOT_CHG},
};

/* Function table */
static struct sndp_dai_func g_sndp_dai_func = {
	.fsi_startup		= NULL,
	.fsi_shutdown		= NULL,
	.fsi_trigger		= NULL,
	.fsi_set_fmt		= NULL,
	.fsi_hw_params		= NULL,
	.fsi_pointer		= NULL,
	.fsi_hw_free		= NULL,
};

/* Main Process table */
/* 0:Playback/1:Capture */
struct sndp_main g_sndp_main[SNDP_PCM_DIRECTION_MAX];

/* FSI format */
static u_int g_sndp_fsi_format;

/* Work queue processing table */
/* Voice start */
static struct sndp_work_info g_sndp_work_voice_start;
/* Voice stop */
static struct sndp_work_info g_sndp_work_voice_stop;
/* Device change for voice */
static struct sndp_work_info g_sndp_work_voice_dev_chg;
/* Device change for fm */
static struct sndp_work_info g_sndp_work_fm_radio_dev_chg;
/* Playback start for soundpath */
static struct sndp_work_info g_sndp_work_play_start;
/* Capture start for soundpath */
static struct sndp_work_info g_sndp_work_capture_start;
/* Playback stop for soundpath */
static struct sndp_work_info g_sndp_work_play_stop;
/* Capture stop for soundpath */
static struct sndp_work_info g_sndp_work_capture_stop;
/* Start during a call playback */
static struct sndp_work_info g_sndp_work_call_playback_start;
/* Start during a call capture */
static struct sndp_work_info g_sndp_work_call_capture_start;
/* Stop during a call playback */
static struct sndp_work_info g_sndp_work_call_playback_stop;
/* Stop during a call capture */
static struct sndp_work_info g_sndp_work_call_capture_stop;
/* Start during a fm playback */
static struct sndp_work_info g_sndp_work_fm_playback_start;
/* Start during a fm capture */
static struct sndp_work_info g_sndp_work_fm_capture_start;
/* Stop during a fm playback */
static struct sndp_work_info g_sndp_work_fm_playback_stop;
/* Stop during a fm capture */
static struct sndp_work_info g_sndp_work_fm_capture_stop;

/* VCD_COMMAND_WATCH_STOP_FW process */
static struct sndp_work_info g_sndp_work_watch_stop_fw;
/* VCD_COMMAND_WATCH_STOP_CLOCK process */
static struct sndp_work_info g_sndp_work_watch_stop_clk;
/* FM Radio start */
static struct sndp_work_info g_sndp_work_fm_radio_start;
/* FM Radio stop */
static struct sndp_work_info g_sndp_work_fm_radio_stop;
/* Start during a playback incommunication */
static struct sndp_work_info g_sndp_work_call_playback_incomm_start;
/* Start during a capture incommunication */
static struct sndp_work_info g_sndp_work_call_capture_incomm_start;
/* Stop during a playback incommunication */
static struct sndp_work_info g_sndp_work_call_playback_incomm_stop;
/* Stop during a capture incommunication */
static struct sndp_work_info g_sndp_work_call_capture_incomm_stop;

/* Playback incommunication start */
static struct sndp_work_info g_sndp_work_play_incomm_start;
/* Playback incommunication stop */
static struct sndp_work_info g_sndp_work_play_incomm_stop;
/* Capture incommunication start */
static struct sndp_work_info g_sndp_work_capture_incomm_start;
/* Capture incommunication stop */
static struct sndp_work_info g_sndp_work_capture_incomm_stop;

/* All down link mute control */
static struct sndp_work_info g_sndp_work_all_dl_mute;
static bool g_dl_mute_flg;

/* hw free for wake up */
static struct sndp_work_info g_sndp_work_hw_free[SNDP_PCM_DIRECTION_MAX];
static wait_queue_head_t     g_sndp_hw_free_wait[SNDP_PCM_DIRECTION_MAX];
static bool		     g_sndp_hw_free_condition[SNDP_PCM_DIRECTION_MAX];

/* shutdown for wake up */
static struct sndp_work_info g_sndp_work_shutdown[SNDP_PCM_DIRECTION_MAX];
static wait_queue_head_t     g_sndp_shutdown_wait[SNDP_PCM_DIRECTION_MAX];
static bool		     g_sndp_shutdown_condition[SNDP_PCM_DIRECTION_MAX];

/* for Power control */
static int g_sndp_power_status = SNDP_POWER_INIT;
static struct device *g_sndp_power_domain;
static size_t g_sndp_power_domain_count;

/* for Wake Lock */
DEFINE_SPINLOCK(lock);
static u_int g_wake_lock_count;

/* for Proc control */
static struct proc_dir_entry *g_sndp_parent;

/* for Register Dump Log */
static int g_sndp_now_direction = SNDP_PCM_DIRECTION_MAX;

/* Running state of the Playback or Capture */
static int g_sndp_playrec_flg = E_IDLE;
static int g_sndp_incomm_playrec_flg = E_IDLE;

/* Routing type of the stream, in during a call */
int g_sndp_stream_route = SNDP_ROUTE_NORMAL;

/* for FSI DAI OPS */
static struct snd_soc_dai_ops sndp_fsi_dai_ops = {
	.startup    = sndp_fsi_startup,
	.shutdown   = sndp_fsi_shutdown,
	.trigger    = sndp_fsi_trigger,
	.set_fmt    = sndp_fsi_set_fmt,
	.hw_params  = sndp_fsi_hw_params,
};

/* pointer receive log cycle counter */
u_int g_sndp_log_cycle_counter[SNDP_PCM_DIRECTION_MAX];

/* for CODEC off */
static struct snd_kcontrol *g_kcontrol;

/* for wait path */
DECLARE_WAIT_QUEUE_HEAD(g_sndp_start_call_queue);
static u_int g_sndp_start_call_wait;

#if defined(DEBUG) && defined(__PRN_SNDP__)
/* Status */
static const struct sndp_pcm_name_suffix status_list[] = {
	{ SNDP_STAT_NOT_CHG,		"SNDP_STAT_NOT_CHG"		},
	{ SNDP_STAT_NORMAL,		"SNDP_STAT_NORMAL"		},
	{ SNDP_STAT_RINGTONE,		"SNDP_STAT_RINGTONE"		},
	{ SNDP_STAT_IN_CALL,		"SNDP_STAT_IN_CALL"		},
	{ SNDP_STAT_IN_CALL_PLAY,	"SNDP_STAT_IN_CALL_PLAY"	},
	{ SNDP_STAT_IN_CALL_CAP,	"SNDP_STAT_IN_CALL_CAP"		},
	{ SNDP_STAT_IN_COMM,		"SNDP_STAT_IN_COMM"		},
};
#endif

static int g_call_playback_stop;

static uint g_bluetooth_band_frequency;

static uint g_loopplay;
static bool g_dfs_mode_min_flag;

/* Callback function for audience */
static struct sndp_extdev_callback_func *g_sndp_extdev_callback;

/* for PM ctrl check */
static int g_pm_cnt;

/*!
   @brief Print Log informs of data receiving

   @param[in]	stream		Direction type
   @param[in]	frame		Frame information
   @param[out]	none

   @retval	none
 */
#if defined(DEBUG) && defined(__PRN_SNDP__)
static inline void sndp_log_data_rcv_indicator(
	const int stream,
	snd_pcm_uframes_t frame)
{
	/* Log level check */
	if (LOG_DEBUG_PRINT > LOG_BYTE_LOW(g_sndp_log_level))
		return;

	/* Receive data notification log is not output */
	if (0 == LOG_GET_CYCLE_COUNT_MAX())
		return;

	/* Specified number of cycles */
	if (LOG_GET_CYCLE_COUNT_MAX() == g_sndp_log_cycle_counter[stream]) {
		/* Initialize the cycle counter */
		LOG_INIT_CYCLE_COUNT(stream);
	}

	/* Receive data notification log is output */
	if (0 == g_sndp_log_cycle_counter[stream]) {
		if (SNDP_PCM_OUT == stream)
			sndp_log_err("OUT : Pointer [%ld]\n", frame);
		else
			sndp_log_err("IN  : Pointer [%ld]\n", frame);
	}

	g_sndp_log_cycle_counter[stream]++;
}
#else
#define sndp_log_data_rcv_indicator(stream, frame)		\
	do { } while (0)
#endif


/*!
   @brief Print Status change

   @param[in]	uiBefore	Before status
   @param[in]	uiAfter		After status
   @param[out]	none

   @retval	none
 */

#if defined(DEBUG) && defined(__PRN_SNDP__)

static inline void sndp_print_status_change(
	const u_int uiBefore,
	const u_int uiAfter)
{
	sndp_log_debug("STATUS CHANGE: %s(%d) -> %s(%d)\n",
		(SNDP_STAT_IN_COMM >= uiBefore) ?
			status_list[uiBefore].suffix : "***",
		uiBefore,
		(SNDP_STAT_IN_COMM >= uiAfter) ?
			status_list[uiAfter].suffix : "***",
		uiAfter);
}
#else
#define sndp_print_status_change(uiBefore, uiAfter) \
	do { } while (0)
#endif

/*!
   @brief Wake lock control

   @param[in]	kind		Wake lock kind
   @param[out]	none

   @retval	none
 */
void sndp_wake_lock(const enum sndp_wake_lock_kind kind)
{
	u_long	flags;

	sndp_log_debug_func("start\n");
	sndp_log_debug("kind[%d] count[%d]\n", kind, g_wake_lock_count);

	spin_lock_irqsave(&lock, flags);

	switch (kind) {
	/* for Lock */
	case E_LOCK:
		if (0 == g_wake_lock_count)
			wake_lock(&g_sndp_wake_lock_suspend);

		g_wake_lock_count++;
		break;

	/* for UnLock */
	case E_UNLOCK:
		if (0 < g_wake_lock_count) {
			g_wake_lock_count--;
			if (0 == g_wake_lock_count)
				wake_unlock(&g_sndp_wake_lock_suspend);
		}
		break;

	/* for UnLock(Forced) */
	case E_FORCE_UNLOCK:
		if (0 < g_wake_lock_count) {
			wake_unlock(&g_sndp_wake_lock_suspend);
			g_wake_lock_count = 0;
		}
		break;

	default:
		sndp_log_err("kind error[%d]\n", kind);
		break;
	}

	spin_unlock_irqrestore(&lock, flags);

	sndp_log_info("count[%d]\n", g_wake_lock_count);
	sndp_log_debug_func("end\n");
}

/*!
   @brief PM runtime get/put control

   @param[in]	kind		PM runtime kind
   @param[in]	new_value	New value
   @param[in]	other_value	value of other direction
   @param[out]	none

   @retval	none
 */
static int sndp_pm_runtime_sync(const enum sndp_pm_runtime_kind kind,
				const u_int new_value,
				const u_int other_value)
{
	int			ret;
	int			err_flag = ERROR_NONE;

	sndp_log_debug_func("start\n");
	sndp_log_debug("kind[%d] count[%d]\n", kind, g_pm_cnt);

	switch (kind) {
	case E_PM_GET:
		if (0 == g_pm_cnt) {
			/* Enable the power domain */
			ret = pm_runtime_get_sync(g_sndp_power_domain);
			if (!(0 == ret || 1 == ret)) {  /* 0:success 1:active */
				sndp_log_err("modules power on error[%d]\n",
					ret);
				err_flag = ret;
			} else {
				/* for PM ctrl check */
				g_pm_cnt++;
				sndp_log_info("pm:get:%d\n", g_pm_cnt);
			}
		} else {
			sndp_log_info("pm:get: no proc[%d]\n", g_pm_cnt);
		}
		break;
	case E_PM_PUT:
		sndp_log_info("new[0x%08x] other[0x%08x] flg[0x%02x]\n",
				new_value, other_value, g_sndp_playrec_flg);
		if ((0 < g_pm_cnt) &&
		   ((SNDP_VALUE_INIT == new_value) ||
		     (SNDP_MODE_NORMAL == SNDP_GET_MODE_VAL(new_value)) ||
		     (SNDP_MODE_RING == SNDP_GET_MODE_VAL(new_value))) &&
		    ((SNDP_VALUE_INIT == other_value) ||
		     (SNDP_MODE_NORMAL == SNDP_GET_MODE_VAL(other_value)) ||
		     (SNDP_MODE_RING == SNDP_GET_MODE_VAL(other_value))) &&
		    (E_IDLE == g_sndp_playrec_flg)) {
			for ( ; 0 < g_pm_cnt; g_pm_cnt--) {
				/* Disable the power domain */
				ret = pm_runtime_put_sync(g_sndp_power_domain);
				if (ERROR_NONE != ret)
					err_flag = ret;
			}
			sndp_log_info("pm:put:%d\n", g_pm_cnt);

			if (ERROR_NONE != err_flag)
				sndp_log_info("modules power off error[%d]\n",
					ret);
		} else {
			sndp_log_info("pm:put: no proc[%d]\n", g_pm_cnt);
		}
		break;
	default:
		sndp_log_err("kind error[%d]\n", kind);
		break;
	}

	sndp_log_debug_func("end\n");
	return err_flag;
}

/*!
   @brief Sound path driver init function (from module_init)

   @param[in]	fsi_port_dai		DAI for FSI(CPU DAI)
   @param[in]	fsi_soc_platform	Structure for FSI SoC platform
   @param[out]	none

   @retval	0			Successful
   @retval	-EFAULT			Other error
 */
int sndp_init(struct snd_soc_dai_driver *fsi_port_dai_driver,
	struct snd_soc_platform_driver *fsi_soc_platform,
	struct snd_soc_card *fsi_soc_card)
{
	int			iRet = -EFAULT;
	int			iCnt = 0;
	struct proc_dir_entry	*entry = NULL;
	struct proc_dir_entry	*reg_dump_entry = NULL;

	sndp_log_debug_func("start\n");

	g_pt_start = SNDP_PT_NOT_STARTED;

	g_sndp_power_domain = NULL;
	/* Main Process table init */
	for (iCnt = 0; iCnt < SNDP_PCM_DIRECTION_MAX; iCnt++) {
		memset(&g_sndp_main[iCnt].arg, 0, sizeof(struct sndp_arg));
		g_sndp_main[iCnt].status = SNDP_STAT_NORMAL;
		g_sndp_main[iCnt].old_value = SNDP_VALUE_INIT;
		g_sndp_log_cycle_counter[iCnt] = 0;
	}

	/* FSI fotmat */
	g_sndp_fsi_format = 0;

	/* for Proc control */
	g_sndp_parent = proc_mkdir(SNDP_DRV_NAME, NULL);
	if (NULL != g_sndp_parent) {
		/* create file for log level entry */
		entry = create_proc_entry(LOG_LEVEL,
					  S_IRUGO | S_IWUGO,
					  g_sndp_parent);
		if (NULL != entry) {
			entry->read_proc  = sndp_proc_read;
			entry->write_proc = sndp_proc_write;
		} else {
			sndp_log_always_err("create_proc_entry(LOG) failed\n");
			goto mkproc_sub_err;
		}

		/* create file for register dump */
		reg_dump_entry = create_proc_entry(SNDP_REG_DUMP,
						   S_IRUGO | S_IWUGO,
						   g_sndp_parent);
		if (NULL != reg_dump_entry) {
			reg_dump_entry->read_proc = sndp_proc_reg_dump_read;
			reg_dump_entry->write_proc = sndp_proc_reg_dump_write;
		} else {
			sndp_log_always_err("create_proc_entry(REG) failed\n");
			goto mkproc_sub_err;
		}

	} else {
		sndp_log_always_err("create failed for proc parrent\n");
		goto mkproc_err;
	}

	/* for Log output */
	g_sndp_log_level = LOG_NO_PRINT;

	/* Initializing work queue processing */
	sndp_work_initialize(&g_sndp_work_voice_start,
				sndp_work_voice_start,
				NULL);
	sndp_work_initialize(&g_sndp_work_voice_stop,
				sndp_work_voice_stop,
				NULL);
	sndp_work_initialize(&g_sndp_work_voice_dev_chg,
				sndp_work_voice_dev_chg,
				NULL);
	sndp_work_initialize(&g_sndp_work_fm_radio_dev_chg,
				sndp_work_fm_radio_dev_chg,
				NULL);
	sndp_work_initialize(&g_sndp_work_play_start,
				sndp_work_play_start,
				&g_sndp_work_play_stop);
	sndp_work_initialize(&g_sndp_work_capture_start,
				sndp_work_capture_start,
				&g_sndp_work_capture_stop);
	sndp_work_initialize(&g_sndp_work_play_stop,
				sndp_work_play_stop,
				&g_sndp_work_play_start);
	sndp_work_initialize(&g_sndp_work_capture_stop,
				sndp_work_capture_stop,
				&g_sndp_work_capture_start);
	sndp_work_initialize(&g_sndp_work_call_playback_start,
				sndp_work_call_playback_start,
				&g_sndp_work_call_playback_stop);
	sndp_work_initialize(&g_sndp_work_call_capture_start,
				sndp_work_call_capture_start,
				&g_sndp_work_call_capture_stop);
	sndp_work_initialize(&g_sndp_work_call_playback_stop,
				sndp_work_call_playback_stop,
				&g_sndp_work_call_playback_start);
	sndp_work_initialize(&g_sndp_work_call_capture_stop,
				sndp_work_call_capture_stop,
				&g_sndp_work_call_capture_start);
	sndp_work_initialize(&g_sndp_work_fm_playback_start,
				sndp_work_fm_playback_start,
				&g_sndp_work_fm_playback_stop);
	sndp_work_initialize(&g_sndp_work_fm_capture_start,
				sndp_work_fm_capture_start,
				&g_sndp_work_fm_capture_stop);
	sndp_work_initialize(&g_sndp_work_fm_playback_stop,
				sndp_work_fm_playback_stop,
				&g_sndp_work_fm_playback_start);
	sndp_work_initialize(&g_sndp_work_fm_capture_stop,
				sndp_work_fm_capture_stop,
				&g_sndp_work_fm_capture_start);
	sndp_work_initialize(&g_sndp_work_watch_stop_fw,
				sndp_work_watch_stop_fw,
				NULL);
	sndp_work_initialize(&g_sndp_work_watch_stop_clk,
				sndp_work_watch_stop_clk,
				NULL);
	sndp_work_initialize(&g_sndp_work_fm_radio_start,
				sndp_work_fm_radio_start,
				NULL);
	sndp_work_initialize(&g_sndp_work_fm_radio_stop,
				sndp_work_fm_radio_stop,
				NULL);
	sndp_work_initialize(&g_sndp_work_play_incomm_start,
				sndp_work_play_incomm_start,
				NULL);
	sndp_work_initialize(&g_sndp_work_play_incomm_stop,
				sndp_work_play_incomm_stop,
				NULL);
	sndp_work_initialize(&g_sndp_work_capture_incomm_start,
				sndp_work_capture_incomm_start,
				NULL);
	sndp_work_initialize(&g_sndp_work_capture_incomm_stop,
				sndp_work_capture_incomm_stop,
				NULL);
	sndp_work_initialize(&g_sndp_work_call_playback_incomm_start,
				sndp_work_call_playback_incomm_start,
				&g_sndp_work_call_playback_incomm_stop);
	sndp_work_initialize(&g_sndp_work_call_capture_incomm_start,
				sndp_work_call_capture_incomm_start,
				&g_sndp_work_call_capture_incomm_stop);
	sndp_work_initialize(&g_sndp_work_call_playback_incomm_stop,
				sndp_work_call_playback_incomm_stop,
				&g_sndp_work_call_playback_incomm_start);
	sndp_work_initialize(&g_sndp_work_call_capture_incomm_stop,
				sndp_work_call_capture_incomm_stop,
				&g_sndp_work_call_capture_incomm_start);
	sndp_work_initialize(&g_sndp_work_all_dl_mute,
				sndp_work_all_dl_mute,
				NULL);

	for (iCnt = 0; SNDP_PCM_DIRECTION_MAX > iCnt; iCnt++) {
		sndp_work_initialize(&g_sndp_work_hw_free[iCnt],
						sndp_work_hw_free, NULL);
		init_waitqueue_head(&g_sndp_hw_free_wait[iCnt]);
		g_sndp_hw_free_condition[iCnt] = false;

		sndp_work_initialize(&g_sndp_work_shutdown[iCnt],
						sndp_work_shutdown, NULL);
		init_waitqueue_head(&g_sndp_shutdown_wait[iCnt]);
		g_sndp_shutdown_condition[iCnt] = false;

		sema_init(&g_sndp_wait_free[iCnt], 1);
	}

	atomic_set(&g_sndp_watch_start_clk, 0);
	atomic_set(&g_sndp_watch_stop_clk, 0);

	/* Get buffer for incommunication */
	call_get_incomm_buffer();
	/*
	 * VCD_COMMAND_WATCH_STOP_FW registration
	 */
	sndp_regist_watch();

	/* initialize all down link mute control flag */
	g_dl_mute_flg = false;

	/* ioremap */
	iRet = common_ioremap();
	if (ERROR_NONE != iRet)
		goto ioremap_err;

	/* FSI master */
	common_set_fsi2cr(SNDP_NO_DEVICE, STAT_ON);

	/* Replaced of function pointers. */
	g_sndp_dai_func.fsi_startup = fsi_port_dai_driver->ops->startup;
	g_sndp_dai_func.fsi_shutdown = fsi_port_dai_driver->ops->shutdown;
	g_sndp_dai_func.fsi_trigger = fsi_port_dai_driver->ops->trigger;
	g_sndp_dai_func.fsi_set_fmt = fsi_port_dai_driver->ops->set_fmt;
	g_sndp_dai_func.fsi_hw_params = fsi_port_dai_driver->ops->hw_params;
	g_sndp_dai_func.fsi_pointer = fsi_soc_platform->ops->pointer;
	g_sndp_dai_func.fsi_hw_free = fsi_soc_platform->ops->hw_free;

	fsi_port_dai_driver[SNDP_PCM_PORTA].ops = &sndp_fsi_dai_ops;
	fsi_port_dai_driver[SNDP_PCM_PORTB].ops = &sndp_fsi_dai_ops;

	fsi_soc_platform->ops->pointer = sndp_fsi_pointer;
	fsi_soc_platform->ops->hw_free = sndp_fsi_hw_free;
	fsi_set_run_time(sndp_fsi_suspend, sndp_fsi_resume);

	/* Power domain setting */
	iRet = power_domain_devices("snd-soc-fsi",
				    &g_sndp_power_domain,
				    &g_sndp_power_domain_count);
	if (ERROR_NONE != iRet) {
		sndp_log_err("Modules not found ... [iRet=%d]\n", iRet);
		goto power_domain_err;
	}

	/* RuntimePM */
	pm_runtime_enable(g_sndp_power_domain);
	iRet = pm_runtime_resume(g_sndp_power_domain);
	g_pm_cnt = 0;
	if (iRet < 0) {
		sndp_log_err("pm_runtime_resume failed\n");
		goto power_domain_err;
	}

	/* create work queue for call */
	iRet = call_create_workque();
	if (ERROR_NONE != iRet) {
		sndp_log_err("work queue create error.\n");
		goto workque_create_err;
	}

	/* create work queue */
	g_sndp_queue_main = sndp_workqueue_create("sndp_queue_main");
	if (NULL == g_sndp_queue_main) {
		sndp_log_err("Queue create error.\n");
		iRet = -ENOMEM;
		goto workque_create_err;
	}

	/* Initialize bluetooth band frequency */
	g_bluetooth_band_frequency = 8000;

	g_loopplay = 0;

	/* Wake lock init */
	wake_lock_init(&g_sndp_wake_lock_suspend,
		       WAKE_LOCK_SUSPEND,
		       "snd-soc-fsi");

	g_dfs_mode_min_flag = false;

	sndp_log_debug_func("end\n");
	return ERROR_NONE;

ioremap_err:
mkproc_sub_err:
workque_create_err:
	remove_proc_entry(SNDP_DRV_NAME, NULL);
power_domain_err:
	if (g_sndp_power_domain) {
		pm_runtime_disable(g_sndp_power_domain);
		g_sndp_power_domain = NULL;
	}
mkproc_err:
	return iRet;

}


/*!
   @brief Sound path driver exit function (from module_exit)

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void sndp_exit(void)
{
	/* iounmap */
	common_iounmap();

	/* RuntimePM */
	if (g_sndp_power_domain) {
		pm_runtime_disable(g_sndp_power_domain);
		g_sndp_power_domain = NULL;
	}

	/* Proc entry remove */
	if (NULL != g_sndp_parent)
		remove_proc_entry(SNDP_DRV_NAME, NULL);

	/* Destroy work queue for call */
	call_destroy_workque();

	/* Destroy work queue */
	if (NULL != g_sndp_queue_main) {
		sndp_workqueue_destroy(g_sndp_queue_main);
		g_sndp_queue_main = NULL;
	}

	/* Wake lock destroy */
	wake_lock_destroy(&g_sndp_wake_lock_suspend);
}


/*!
   @brief PROC read function

   @param[out]	page	Page data
   @param[-]	start	Not use
   @param[-]	offset	Not use
   @param[-]	count	Not use
   @param[out]	eof	EOF flag
   @param[-]	data	Not use

   @retval	read data length
 */
static int sndp_proc_read(
	char *page,
	char **start,
	off_t offset,
	int count,
	int *eof,
	void *data)
{
	int	iLen = sprintf(page, "0x%08x\n", (int)g_sndp_log_level);

	*eof = 1;
	return iLen;
}


/*!
   @brief PROC write function

   @param[-]	filp	Not use
   @param[in]	buffer	Write data
   @param[in]	count	Data length
   @param[-]	data	Not use

   @retval		Write data count
 */
static int sndp_proc_write(
	struct file *filp,
	const char __user *buffer,
	unsigned long count,
	void *data)
{
	unsigned long long int	uiIn;
	static unsigned char	proc_buf[32];

	memset(proc_buf, 0, sizeof(proc_buf));
	/* copy, from user */
	if (copy_from_user((void *)proc_buf, (void __user *)buffer, count)) {
		sndp_log_err("copy_from_user failed.\n");
		return -EFAULT;
	}

	if (kstrtoull(proc_buf, 0, &uiIn)) {
		sndp_log_err("kstrtoull error\n");
		return -EFAULT;
	}

	if (10 == ((u_int)uiIn & LOG_LEVEL_MAX)) {
		g_dfs_mode_min_flag = true;
		sndp_log_info("SET DFS MIN DISABLE\n");
		return count;
	} else if (11 == ((u_int)uiIn & LOG_LEVEL_MAX)) {
		g_dfs_mode_min_flag = false;
		sndp_log_info("SET DFS HIGH\n");
		return count;
	}

	g_sndp_log_level = (u_int)uiIn & LOG_LEVEL_MAX;
	LOG_INIT_CYCLE_COUNT(SNDP_PCM_OUT);

	return count;
}


/*!
   @brief PROC read function for register dump

   @param[out]  page    Page data
   @param[-]    start   Not use
   @param[-]    offset  Not use
   @param[-]    count   Not use
   @param[out]  eof     EOF flag
   @param[-]    data    Not use

   @retval      0
 */
static int sndp_proc_reg_dump_read(
	char *page,
	char **start,
	off_t offset,
	int count,
	int *eof,
	void *data)
{
	return ERROR_NONE;
}


/*!
   @brief PROC write function for register dump

   @param[-]	filp	Not use
   @param[in]	buffer	Write data
   @param[in]	count	Data length
   @param[-]	data	Not use

   @retval	Write data length
 */
static int sndp_proc_reg_dump_write(
	struct file *filp,
	const char __user *buffer,
	unsigned long count,
	void *data)
{
	int			iRet = ERROR_NONE;
	u_long			ulIn;
	u_long			log_level_back = 0;
	static unsigned char	proc_buf[32];

	sndp_log_debug_func("start\n");

	log_level_back = LOG_BIT_REG_DUMP & g_sndp_log_level;
	g_sndp_log_level |= LOG_BIT_REG_DUMP;

	memset(proc_buf, 0, sizeof(proc_buf));
	/* copy, from user */
	if (copy_from_user((void *)proc_buf, (void __user *)buffer, count)) {
		iRet = -EFAULT;
		sndp_log_err("copy_from_user failed.\n");
		return iRet;
	}

	iRet = kstrtoul(proc_buf, 0, &ulIn);
	if (0 != iRet) {
		sndp_log_err("kstrtoul error(ret=%d)\n", iRet);
		return iRet;
	}

	sndp_log_reg_dump("ulIn : 0x%08lx\n", ulIn);

	g_sndp_mode = ((u_int)ulIn & 0xffff0000) >> 16;
	ulIn &= 0xffff;

	/* for FSI */
	if ((REG_DUMP_ALL == ulIn) || (REG_DUMP_FSI & ulIn)) {
		if (SNDP_PCM_DIRECTION_MAX != g_sndp_now_direction)
			fsi_reg_dump(GET_OLD_VALUE(g_sndp_now_direction));
		else
			sndp_log_reg_dump("FSI register is not ready.\n");
	}

	/* for CLKGEN */
	if ((REG_DUMP_ALL == ulIn) || (REG_DUMP_CLKGEN & ulIn)) {
		if (SNDP_PCM_DIRECTION_MAX != g_sndp_now_direction)
			clkgen_reg_dump();
		else
			sndp_log_reg_dump("CLKGEN register is not ready.\n");
	}

	/* for SCUW */
	if ((REG_DUMP_ALL == ulIn) || (REG_DUMP_SCUW & ulIn)) {
		if ((SNDP_MODE_INCALL == SNDP_GET_MODE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT))) ||
			(SNDP_MODE_INCOMM == SNDP_GET_MODE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT)))) {
			if ((SNDP_PCM_DIRECTION_MAX != g_sndp_now_direction) &&
				(!(SNDP_ROUTE_PLAY_CHANGED & g_sndp_stream_route)))
				scuw_reg_dump();
			else
				sndp_log_reg_dump("SCUW register is not ready.\n");
		}
	}

	g_sndp_log_level &= ~LOG_BIT_REG_DUMP | log_level_back;

	sndp_log_debug_func("%s out\n", __func__);

	return count;
}

#if 0
/*!
   @brief Element information check function

   @param[in]	kcontrol	Kcontrol structure
   @param[out]	uinfo		Element information structure

   @retval	0		Successful
 */
static int sndp_soc_info(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct soc_enum	*e = (struct soc_enum *)kcontrol->private_value;


	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = e->max;

	if (uinfo->value.enumerated.item > e->max - 1)
		uinfo->value.enumerated.item = e->max - 1;

	strcpy(uinfo->value.enumerated.name,
	       e->texts[uinfo->value.enumerated.item]);

	return ERROR_NONE;
}


/*!
   @brief GET callback function for hooks control

   @param[-]	kcontrol	Not use
   @param[-]	ucontrol	Not use

   @retval	0		Successful
 */
static int sndp_soc_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return ERROR_NONE;
}
#endif

/*!
   @brief PUT callback function for hooks control

   @param[-]	kcontrol	Not use
   @param[in]	ucontrol	Element data

   @retval	0		Successful
   @retval	-EINVAL		Invalid argument
 */
int sndp_soc_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	char cPcm[SNDP_PCM_NAME_MAX_LEN];
	u_int	uiValue;
	u_int	uiProcess;
	u_int	uiDirection;
	u_int	uiMode;
	u_int	uiNextStatus;
	u_int	uiSaveStatus;
	u_int	uiOldValue;
	u_int	old_mode;
	u_int	new_btband;
	int	iRet = ERROR_NONE;

	sndp_log_debug_func("start\n");

	/* Invalid error */
	if (NULL == ucontrol)
		return -EINVAL;

	/* Gets the value of PCM */
	uiValue = ucontrol->value.integer.value[0];
	if (SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(uiValue)) {
		new_btband = SNDP_GET_BLUETOOTH_FREQUENCY(uiValue);
		if (new_btband) {
			/* WIDE */
			g_bluetooth_band_frequency = 16000;
			sndp_log_info("BT WIDE\n");
		} else {
			/* NARROW */
			g_bluetooth_band_frequency = 8000;
			sndp_log_info("BT NARROW\n");
		}
		/* "bluetooth_band_frequency" setting removes it */
		/* after confirmation.                           */
		uiValue &= SNDP_BLUETOOTH_FREQUENCY_MASK;
	}

	/* Show the PCM name */
	memset(cPcm, '\0', sizeof(cPcm));
	sndp_pcm_name_generate(uiValue, cPcm);
	sndp_log_info("PCM: %s\n", cPcm);

	/* Gets the direction (Playback/Capture) */
	uiDirection = SNDP_GET_DIRECTION_VAL(uiValue);

	/* Get Old Value */
	uiOldValue = GET_OLD_VALUE(uiDirection);
	old_mode   = SNDP_GET_MODE_VAL(uiOldValue);

	/* Gets the mode (NORMAL/RINGTONE/INCALL/INCOMMUNICATION) */
	uiMode = SNDP_GET_MODE_VAL(uiValue);

	if ((SNDP_MODE_INCALL != uiMode) && (SNDP_MODE_INCOMM != uiMode)) {
		g_sndp_start_call_wait = 0;
		/* Initialization of the firmware status flag */
		atomic_set(&g_call_watch_start_fw, 0);
	}
	if (SNDP_MODE_INCOMM != uiMode) {
		g_call_incomm_cb[SNDP_PCM_OUT] = true;
		g_call_incomm_cb[SNDP_PCM_IN] = true;
		atomic_set(&g_call_watch_stop_fw, 0);
	}

	sndp_log_debug("g_sndp_start_call_wait = %d\n", g_sndp_start_call_wait);

	/* Identify the process for changing modes (Old mode -> New mode) */
	uiProcess = g_sndp_mode_map[old_mode][uiMode].next_proc;

	/* Identify the next status for changing modes */
	uiNextStatus = g_sndp_mode_map[old_mode][uiMode].next_status;

	/* Save now status */
	uiSaveStatus = GET_SNDP_STATUS(uiDirection);

	/* Save Kcontrol */
	g_kcontrol = kcontrol;

	/* Status Change */
	if (SNDP_STAT_NOT_CHG != uiNextStatus) {
		/* Change to new Status */
		sndp_print_status_change(uiSaveStatus, uiNextStatus);
		SET_SNDP_STATUS(uiDirection, uiNextStatus);
	}

	/* Direction check */
	if (SNDP_PCM_OUT == uiDirection) {
		/* FM Radio stop process */
		if ((SNDP_VALUE_INIT != uiOldValue) &&
		    (SNDP_FM_RADIO_RX & SNDP_GET_DEVICE_VAL(uiOldValue)) &&
		    (!(SNDP_FM_RADIO_RX & SNDP_GET_DEVICE_VAL(uiValue)))) {
			g_sndp_now_direction = SNDP_PCM_DIRECTION_MAX;

			/* Stop Capture running */
			g_sndp_playrec_flg &= ~E_FM_PLAY;

			if (!((E_FM_PLAY | E_FM_CAP) & g_sndp_playrec_flg)) {
				/* Wake Lock */
				sndp_wake_lock(E_LOCK);

				/* work queue for FM Radio stop */
				g_sndp_work_fm_radio_stop.old_value =
								uiOldValue;
				g_sndp_work_fm_radio_stop.new_value =
								uiValue;

				sndp_workqueue_enqueue(g_sndp_queue_main,
						&g_sndp_work_fm_radio_stop);
			}
		}

		/* FM Radio start process */
		if ((SNDP_FM_RADIO_RX & SNDP_GET_DEVICE_VAL(uiValue)) &&
		    !(SNDP_FM_RADIO_RX & SNDP_GET_DEVICE_VAL(uiOldValue))) {
			g_sndp_now_direction = SNDP_PCM_OUT;

			/* Running Capture */
			g_sndp_playrec_flg |= E_FM_PLAY;

			if (!(E_FM_CAP & g_sndp_playrec_flg)) {
				/* Wake Lock */
				sndp_wake_lock(E_LOCK);

				/* work queue for FM Radio start */
				g_sndp_work_fm_radio_start.new_value = uiValue;
				sndp_workqueue_enqueue(g_sndp_queue_main,
						&g_sndp_work_fm_radio_start);
			}

			/* Saving the type of PCM */
			SET_OLD_VALUE(uiDirection, uiValue);
			sndp_log_debug_func("end\n");
			return ERROR_NONE;
		}

		/* FM Radio device change */
		if ((SNDP_VALUE_INIT != uiOldValue) &&
		    (SNDP_FM_RADIO_RX & SNDP_GET_DEVICE_VAL(uiValue)) &&
		    (SNDP_FM_RADIO_RX & SNDP_GET_DEVICE_VAL(uiOldValue))) {
			/* Wake Lock */
			sndp_wake_lock(E_LOCK);

			/* work queue for FM Radio start */
			g_sndp_work_fm_radio_dev_chg.new_value = uiValue;
			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_fm_radio_dev_chg);

			/* Saving the type of PCM */
			SET_OLD_VALUE(uiDirection, uiValue);
			sndp_log_debug_func("end\n");
			return ERROR_NONE;
		}
	}

	/* Direction check */
	if ((SNDP_PCM_IN == uiDirection) &&
	    (SNDP_MODE_INCOMM != uiMode) &&
	    (SNDP_MODE_INCOMM != old_mode)) {
		/* FM Radio start process */
		if (SNDP_FM_RADIO_RX & SNDP_GET_DEVICE_VAL(uiValue)) {
			g_sndp_playrec_flg |= E_FM_CAP;
			if (!(E_FM_PLAY & g_sndp_playrec_flg)) {
				/* Wake Lock */
				sndp_wake_lock(E_LOCK);

				/* work queue for FM Radio start */
				g_sndp_work_fm_radio_start.new_value = uiValue;
				sndp_workqueue_enqueue(g_sndp_queue_main,
						&g_sndp_work_fm_radio_start);
			}
		}

		/* Saving the type of PCM */
		SET_OLD_VALUE(uiDirection, uiValue);
		sndp_log_debug_func("end\n");
		return ERROR_NONE;
	}

	/* for Register dump debug */
	g_sndp_now_direction =
		((SNDP_MODE_INCALL == uiMode) || (SNDP_MODE_INCOMM == uiMode)) ?
		SNDP_PCM_OUT : SNDP_PCM_DIRECTION_MAX;

	/* Processing for each process */
	/* SNDP_PROC_CALL_STOP */
	if (uiProcess & SNDP_PROC_CALL_STOP) {
		/* Call + Recording is not running */
		if ((SNDP_PCM_OUT == uiDirection) &&
		    (!(g_status & REC_STATUS))) {

			/* Wake Lock */
			sndp_wake_lock(E_LOCK);

			/* Registered in the work queue for call stop */
			g_sndp_work_voice_stop.old_value = uiOldValue;
			g_sndp_work_voice_stop.new_value = uiValue;

			sndp_workqueue_enqueue(g_sndp_queue_main,
						&g_sndp_work_voice_stop);

			g_sndp_now_direction = SNDP_PCM_DIRECTION_MAX;
		}
	}

	/* Processing for each process */
	if (uiProcess & SNDP_PROC_INCOMM_STOP) {

			/* Wake Lock */
			sndp_wake_lock(E_LOCK);

			if (SNDP_PCM_OUT == uiDirection) {
				/* Registered in the work queue for play incomm stop */
				g_sndp_work_play_incomm_stop.old_value = uiOldValue;
				g_sndp_work_play_incomm_stop.new_value = uiValue;

				sndp_workqueue_enqueue(g_sndp_queue_main,
							&g_sndp_work_play_incomm_stop);
			} else {
				/* Registered in the work queue for capture incomm stop */
				g_sndp_work_capture_incomm_stop.old_value = uiOldValue;
				g_sndp_work_capture_incomm_stop.new_value = uiValue;

				sndp_workqueue_enqueue(g_sndp_queue_main,
						&g_sndp_work_capture_incomm_stop);
			}
	}

	/* SNDP_PROC_CALL_START */
	if (uiProcess & SNDP_PROC_CALL_START) {
		if (SNDP_PCM_OUT == uiDirection) {
			/* for Register dump debug */
			g_sndp_now_direction = SNDP_PCM_OUT;

			if (SNDP_MODE_INCOMM ==
				SNDP_GET_MODE_VAL(GET_OLD_VALUE(SNDP_PCM_IN))) {
				/* Dummy record start (VoIP) */
				if (!(SNDP_ROUTE_CAP_INCOMM_DUMMY &
							g_sndp_stream_route))
					call_change_incomm_rec();
			}

			/* Wake Lock */
			sndp_wake_lock(E_LOCK);

			/* Registered in the work queue for call start */
			g_sndp_work_voice_start.new_value = uiValue;
			g_sndp_work_voice_start.save_status = uiSaveStatus;
			sndp_workqueue_enqueue(g_sndp_queue_main,
						&g_sndp_work_voice_start);
		}
	}

	if (uiProcess & SNDP_PROC_INCOMM_START) {
		if (((SNDP_PCM_OUT == uiDirection) &&
		     (SNDP_MODE_INCOMM != SNDP_GET_MODE_VAL(GET_OLD_VALUE(SNDP_PCM_IN)))) ||
		    ((SNDP_PCM_IN == uiDirection) &&
		     (SNDP_MODE_INCOMM != SNDP_GET_MODE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT))))) {

			if (g_dfs_mode_min_flag) {
				sndp_log_info("disable dfs mode min\n");
				disable_dfs_mode_min();
			} else {
				sndp_log_info("stop cpufreq\n");
				iRet = stop_cpufreq();
				if (ERROR_NONE != iRet)
					sndp_log_err("stop_cpufreq ret[%d]\n",
									iRet);
			}

		}
		/* Wake Lock */
		sndp_wake_lock(E_LOCK);

		if (SNDP_PCM_OUT == uiDirection) {
			/* Registered in the work queue for play incomm stop */
			g_sndp_work_play_incomm_start.new_value = uiValue;
			g_sndp_work_play_incomm_start.save_status = uiSaveStatus;

			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_play_incomm_start);
		} else {
			/* Registered in the work queue for capture incomm stop */
			g_sndp_work_capture_incomm_start.new_value = uiValue;

			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_capture_incomm_start);
		}
	}

	/* SNDP_PROC_DEV_CHANGE */
	if (uiProcess & SNDP_PROC_DEV_CHANGE) {
		/* Modes other than the INIT */
		if (SNDP_VALUE_INIT != uiOldValue) {
			/* Change the device type */
			if ((SNDP_GET_DEVICE_VAL(uiOldValue) !=
					SNDP_GET_DEVICE_VAL(uiValue))) {
				if ((SNDP_MODE_INCALL == uiMode) ||
					(SNDP_MODE_INCOMM == uiMode) ||
					(SNDP_GET_DEVICE_VAL(uiOldValue) &
						SNDP_SPEAKER &
						SNDP_GET_DEVICE_VAL(uiValue))) {

					/* Wake Lock */
					sndp_wake_lock(E_LOCK);

					/*
					* Registered in the work queue for
					* voice call device change
					*/
					sndp_log_debug(
						"uiValue %08x  old_value %08x\n",
						uiValue, uiOldValue);

					g_sndp_work_voice_dev_chg.new_value =
						uiValue;

					g_sndp_work_voice_dev_chg.old_value =
						uiOldValue;

					sndp_workqueue_enqueue(
						g_sndp_queue_main,
						&g_sndp_work_voice_dev_chg);
				}
			}
		}
	}

	/* Saving the type of PCM */
	SET_OLD_VALUE(uiDirection, uiValue);

	sndp_log_debug_func("end\n");
	return ERROR_NONE;
}


/*!
   @brief GET callback function for hooks control(Volume setting)

   @param[-]	kcontrol	Not use
   @param[in]	ucontrol	Element data

   @retval	0		Successful
   @retval	-EIO		kernel-side error
 */
int sndp_soc_get_voice_out_volume(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	u_int	uiVal = 0;

	/* Return the current settings */
	ucontrol->value.enumerated.item[0] = uiVal;

	return ERROR_NONE;
}


/*!
   @brief PUT callback function for hooks control(Volume setting)

   @param[-]	kcontrol	Not use
   @param[in]	ucontrol	Element data

   @retval	0		Successful
   @retval	-EINVAL		Invalid argument
 */
int sndp_soc_put_voice_out_volume(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return ERROR_NONE;
}


#if 0
/*!
   @brief callback function for hooks control(Volume setting)

   @param[-]	kcontrol	Not use
   @param[in]	ucontrol	Element data

   @retval	0		Successful
 */
static int sndp_soc_capture_volume(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* Not processing */
	return ERROR_NONE;
}


/*!
   @brief GET callback function for hooks control(Capture Mute setting)

   @param[-]	kcontrol	Not use
   @param[in]	ucontrol	Element data

   @retval	0		Successful
 */
static int sndp_soc_get_capture_mute(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return ERROR_NONE;
}


/*!
   @brief PUT callback function for hooks control(Capture Mute setting)

   @param[-]	kcontrol	Not use
   @param[in]	ucontrol	Element data

   @retval	0		Successful
   @retval	-EINVAL		Invalid argument
 */
static int sndp_soc_put_capture_mute(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	return ERROR_NONE;
}
#endif

/*!
   @brief GET callback function for hooks control(Playback Mute setting)

   @param[-]	kcontrol	Not use
   @param[in]	ucontrol	Element data

   @retval	0		Successful
 */
int sndp_soc_get_playback_mute(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	/* Return the current settings */
	ucontrol->value.enumerated.item[0] = !(g_dl_mute_flg);

	return ERROR_NONE;
}

/*!
   @brief PUT callback function for hooks control(Playback Mute setting)

   @param[-]	kcontrol	Not use
   @param[in]	ucontrol	Element data

   @retval	0		Successful
   @retval	-EINVAL		Invalid argument
 */
int sndp_soc_put_playback_mute(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	int	iRet = ERROR_NONE;
	u_int	iInDev = SNDP_NO_DEVICE;
	u_int	iOutDev = SNDP_NO_DEVICE;


	/* Device get from old_value */
	if (SNDP_VALUE_INIT != GET_OLD_VALUE(SNDP_PCM_IN))
		iInDev = SNDP_GET_DEVICE_VAL(GET_OLD_VALUE(SNDP_PCM_IN));
	if (SNDP_VALUE_INIT != GET_OLD_VALUE(SNDP_PCM_OUT))
		iOutDev = SNDP_GET_DEVICE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT));

	/* update all down link mute control flag */
	g_dl_mute_flg = !(ucontrol->value.enumerated.item[0]);

	sndp_log_debug("MUTE=%s\n",
		(false == g_dl_mute_flg) ? "false" : "true");

	/* Control to output mute on/off,         */
	/* when during a call or FM reproduction. */
	if ((SNDP_MODE_INCALL ==
		SNDP_GET_MODE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT))) ||
	    (SNDP_FM_RADIO_RX & iInDev) || (SNDP_FM_RADIO_RX & iOutDev))
		sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_all_dl_mute);

	return iRet;
}


/*!
   @brief System suspend function

   @param[in]	dev	Device
   @param[out]	none

   @retval	ERROR_NONE
 */
static int sndp_fsi_suspend(struct device *dev)
{
	u_int	iInDev = SNDP_NO_DEVICE;
	u_int	iOutDev = SNDP_NO_DEVICE;

	sndp_log_debug_func("start\n");

	/* Device get from old_value */
	if (SNDP_VALUE_INIT != GET_OLD_VALUE(SNDP_PCM_IN))
		iInDev = SNDP_GET_DEVICE_VAL(GET_OLD_VALUE(SNDP_PCM_IN));

	if (SNDP_VALUE_INIT != GET_OLD_VALUE(SNDP_PCM_OUT))
		iOutDev = SNDP_GET_DEVICE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT));

	sndp_log_info("InDev[0x%08X] OutDev[0x%08X]\n", iInDev, iOutDev);

	/* Otherwise only IN_CALL, for processing and Other than FM playback */
	if ((SNDP_MODE_INCALL !=
		SNDP_GET_MODE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT))) &&
	    ((!(SNDP_FM_RADIO_RX & iInDev)) &&
	     (!(SNDP_FM_RADIO_RX & iOutDev)))) {
		if (SNDP_POWER_SUSPEND != g_sndp_power_status) {
			g_sndp_power_status = SNDP_POWER_SUSPEND;
		}
	}

	sndp_log_debug_func("end\n");

	return ERROR_NONE;
}


/*!
   @brief System resume function

   @param[in]	dev	Device
   @param[out]	none

   @retval	ERROR_NONE
 */
static int sndp_fsi_resume(struct device *dev)
{
	u_int	iInDev = SNDP_NO_DEVICE;
	u_int	iOutDev = SNDP_NO_DEVICE;

	sndp_log_debug_func("start\n");

	/* Device get from old_value */
	if (SNDP_VALUE_INIT != GET_OLD_VALUE(SNDP_PCM_IN))
		iInDev = SNDP_GET_DEVICE_VAL(GET_OLD_VALUE(SNDP_PCM_IN));
	if (SNDP_VALUE_INIT != GET_OLD_VALUE(SNDP_PCM_OUT))
		iOutDev = SNDP_GET_DEVICE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT));

	sndp_log_info("InDev[0x%08X] OutDev[0x%08X]\n",
			iInDev, iOutDev);

	/* Otherwise only IN_CALL, for processing and Other than FM playback */
	if ((SNDP_MODE_INCALL !=
		SNDP_GET_MODE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT))) &&
	    ((!(SNDP_FM_RADIO_RX & iInDev)) &&
	     (!(SNDP_FM_RADIO_RX & iOutDev)))) {
		if (SNDP_POWER_RESUME != g_sndp_power_status) {
			g_sndp_power_status = SNDP_POWER_RESUME;
		}
	}

	sndp_log_debug_func("end\n");

	return ERROR_NONE;
}


/*!
   @brief FSI startup function

   @param[in]	substream	PCM substream structure
   @param[in]	dai		Digital audio interface structure

   @retval	0		Successful
 */
static int sndp_fsi_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	int	iRet = ERROR_NONE;

	sndp_log_debug_func("start\n");

	sndp_log_info("%s  old_value = 0x%08X\n",
		(SNDP_PCM_OUT == substream->stream) ? "PLAYBACK" : "CAPTURE",
		GET_OLD_VALUE(substream->stream));

	/* Playback or Capture, than the Not processing */
	if ((SNDP_PCM_OUT != substream->stream) &&
		(SNDP_PCM_IN != substream->stream)) {
		return iRet;
	}

	/* To store information about Substream and DAI */
	g_sndp_main[substream->stream].arg.fsi_substream = substream;
	g_sndp_main[substream->stream].arg.fsi_dai = dai;

	sndp_log_debug_func("end\n");
	return iRet;
}


/*!
   @brief Work queue function for shutdown

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_shutdown(struct sndp_work_info *work)
{
	u_int		in_old_val = work->old_value;
	struct snd_pcm_substream *substream = work->save_substream;
	u_int		direction = substream->stream;

	sndp_log_debug_func("start\n");

	sndp_log_info("%s  old_value = 0x%08X\n",
		(SNDP_PCM_OUT == direction) ? "PLAYBACK" : "CAPTURE",
		GET_OLD_VALUE(direction));

	/* Playback or Capture, than the Not processing */
	if ((SNDP_PCM_OUT != direction) && (SNDP_PCM_IN != direction))
		goto shutdown_wake_up;

	if (SNDP_PCM_IN == direction) {
		if (SNDP_MODE_INCOMM == SNDP_GET_MODE_VAL(in_old_val)) {
			g_sndp_incomm_playrec_flg &= ~E_CAP;
			if (!g_sndp_incomm_playrec_flg) {
				/* To register a work queue */
				/* to stop processing Playback */
				sndp_work_incomm_stop(
					in_old_val, SNDP_VALUE_INIT);
			}
		} else if (g_sndp_playrec_flg & E_FM_CAP) {
			g_sndp_playrec_flg &= ~E_FM_CAP;
			if (!(g_sndp_playrec_flg & E_FM_PLAY))
				sndp_work_fm_radio_stop(work);
		}

		SET_OLD_VALUE(SNDP_PCM_IN, SNDP_VALUE_INIT);
		SET_SNDP_STATUS(SNDP_PCM_IN, SNDP_STAT_NORMAL);
	}

	/* To store information about Substream and DAI */
	g_sndp_main[direction].arg.fsi_substream = substream;
	g_sndp_main[direction].arg.fsi_dai = work->save_dai;

shutdown_wake_up:
	g_sndp_shutdown_condition[direction] = true;
	wake_up(&g_sndp_shutdown_wait[direction]);

	sndp_log_debug_func("end\n");
}

/*!
   @brief FSI shutdown function

   @param[in]	substream	PCM substream structure
   @param[in]	dai		Digital audio interface structure

   @retval	none
 */
static void sndp_fsi_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	u_int		direction = substream->stream;
	int		ret;

	sndp_log_debug_func("start\n");

	g_sndp_shutdown_condition[direction] = false;
	g_sndp_work_shutdown[direction].save_substream = substream;
	g_sndp_work_shutdown[direction].save_dai = dai;
	g_sndp_work_shutdown[direction].old_value = GET_OLD_VALUE(SNDP_PCM_IN);
	g_sndp_work_shutdown[direction].new_value = GET_OLD_VALUE(SNDP_PCM_IN);

	sndp_workqueue_enqueue(g_sndp_queue_main,
			&g_sndp_work_shutdown[direction]);
	sndp_log_info("enqueue\n");

	ret = wait_event_timeout(
		g_sndp_shutdown_wait[direction],
		g_sndp_shutdown_condition[direction],
		msecs_to_jiffies(SNDP_WAIT_MAX));

	sndp_log_info("complete ret[%d]\n", ret);

	sndp_log_debug_func("end\n");
}

/*!
   @brief FSI trigger function

   @param[in]	substream	PCM substream structure
   @param[in]	cmd		Trigger command type
				(SNDRV_PCM_TRIGGER_START/
				 SNDRV_PCM_TRIGGER_STOP)
   @param[in]	dai		Digital audio interface structure

   @retval	0		Successful
 */
static int sndp_fsi_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai)
{
	int			iRet = ERROR_NONE;
	struct sndp_stop	*stop;
	struct sndp_arg		*arg;
	char			cPcm[SNDP_PCM_NAME_MAX_LEN];
	struct snd_pcm_runtime	*runtime;

	sndp_log_debug_func("start\n");

	if (!substream) {
		sndp_log_info("substream is NULL");
		return iRet;
	}
	if (!substream->runtime) {
		sndp_log_info("runtime is NULL");
		return iRet;
	}

	runtime = substream->runtime;

	sndp_log_info("%s val[0x%X] %s\n",
		(SNDP_PCM_OUT == substream->stream) ? "PLAYBACK" : "CAPTURE",
		GET_OLD_VALUE(substream->stream),
		(SNDRV_PCM_TRIGGER_START == cmd) ?
					"TRIGGER_START" : "TRIGGER_STOP");

	/* Playback or Capture, than the Not processing */
	if ((SNDP_PCM_OUT != substream->stream) &&
		(SNDP_PCM_IN != substream->stream)) {
		return iRet;
	}

	/* Initialize the cycle counter */
	LOG_INIT_CYCLE_COUNT(substream->stream);

	/* for Production Test (Loopback) */
	if ((SNDP_PT_LOOPBACK_START == g_pt_start) || (1 == g_loopplay)) {
		if (SNDRV_PCM_TRIGGER_START == cmd)
			g_loopplay = 1;

		/* Same Call process route */
		sndp_call_trigger(substream,
				cmd,
				dai,
				GET_OLD_VALUE(substream->stream));

		if (SNDRV_PCM_TRIGGER_STOP == cmd)
			g_loopplay = 0;

		goto pt_route_end;
	}

	/* Checking Mode */
	switch (SNDP_GET_MODE_VAL(GET_OLD_VALUE(substream->stream))) {
	case SNDP_MODE_INCALL:
		/* Call process route */
		sndp_call_trigger(substream,
				  cmd,
				  dai,
				  GET_OLD_VALUE(substream->stream));
		break;
	case SNDP_MODE_INCOMM:
		/* VoIP process route */
		sndp_incomm_trigger(substream,
				  cmd,
				  dai,
				  GET_OLD_VALUE(substream->stream));
		break;
	default:
		if (SNDP_FM_RADIO_RX &
		    SNDP_GET_DEVICE_VAL(GET_OLD_VALUE(substream->stream))) {
			sndp_fm_trigger(substream,
					  cmd,
					  dai,
					  GET_OLD_VALUE(substream->stream));
		} else {
			/* MM Playback or MM Capture process route */
			switch (cmd) {
			case SNDRV_PCM_TRIGGER_START:	/* TRIGGER_START */
				/* Display the name of PCM */
				sndp_pcm_name_generate(
					GET_OLD_VALUE(substream->stream), cPcm);
				sndp_log_debug(
					"#Trigger start[MM] PCM: %s [0x%08X]\n",
					cPcm, GET_OLD_VALUE(substream->stream));

				sndp_log_debug("buff_size %ld  period_size %ld"
						"periods %d  frame_bits %d\n",
						runtime->buffer_size,
						runtime->period_size,
						runtime->periods,
						runtime->frame_bits);

				/* Wake Lock */
				sndp_wake_lock(E_LOCK);

				/* A work queue processing */
				if (SNDP_PCM_OUT == substream->stream) {
					sndp_workqueue_enqueue(
						g_sndp_queue_main,
						&g_sndp_work_play_start);

					/* for Register dump debug */
					g_sndp_now_direction = SNDP_PCM_OUT;
				} else {
					sndp_workqueue_enqueue(
						g_sndp_queue_main,
						&g_sndp_work_capture_start);

					/* for Register dump debug */
					g_sndp_now_direction = SNDP_PCM_IN;
				}
				break;

			case SNDRV_PCM_TRIGGER_STOP:	/* TRIGGER_STOP */
				sndp_log_debug("#Trigger stop[MM]\n");

				arg = &g_sndp_main[substream->stream].arg;
				/* FSI trigger stop process */
				fsi_set_trigger_stop(arg->fsi_substream,
						     false);

				/* A work queue processing */
				if (SNDP_PCM_OUT == substream->stream) {
					stop = &g_sndp_work_play_stop.stop;

					stop->fsi_substream =
							*arg->fsi_substream;

					stop->fsi_dai = *arg->fsi_dai;

					sndp_workqueue_enqueue(
						g_sndp_queue_main,
						&g_sndp_work_play_stop);
				} else {
					stop = &g_sndp_work_capture_stop.stop;

					stop->fsi_substream =
							*arg->fsi_substream;

					stop->fsi_dai = *arg->fsi_dai;

					sndp_workqueue_enqueue(
						g_sndp_queue_main,
						&g_sndp_work_capture_stop);
				}
				break;
			default:
				sndp_log_debug("playback trigger none.\n");
				break;
			}
		}
	}

pt_route_end:

	sndp_log_debug_func("end\n");
	return iRet;
}


/*!
   @brief FSI Set format function

   @param[in]	dai	Digital audio interface structure
   @param[in]	fmt	Format type

   @retval	0	Successful
 */
static int sndp_fsi_set_fmt(struct snd_soc_dai *dai, u_int fmt)
{
	sndp_log_debug_func("start\n");
	sndp_log_debug("fmt = 0x%08X\n", fmt);

	/* To store information about Hardware parameters */
	g_sndp_fsi_format = fmt;

	sndp_log_debug_func("end\n");
	return ERROR_NONE;
}


/*!
   @brief FSI HW Parameters function

   @param[in]	substream	PCM substream structure
   @param[in]	params		HW Parameters structure
   @param[in]	dai		Digital audio interface structure

   @retval	0		Successful
 */
static int sndp_fsi_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	sndp_log_debug_func("start\n");
	sndp_log_info("hw_params dir[%d] %d\n",
			substream->stream, params_rate(params));

	/* To store information about Hardware parameters */
	g_sndp_main[substream->stream].arg.fsi_params = *params;

	sndp_log_debug_func("end\n");
	return ERROR_NONE;
}


/*!
   @brief FSI pointer function

   @param[in]	substream	PCM substream structure

   @retval	Data position
 */
static snd_pcm_uframes_t sndp_fsi_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t	iRet = 0;

	if (SNDP_MODE_INCALL ==
		SNDP_GET_MODE_VAL(GET_OLD_VALUE(substream->stream))) {
		/* Not in a call */

		/* VCD is dead */
		if ((SNDP_ROUTE_PLAY_CHANGED & g_sndp_stream_route) &&
		    (SNDP_PCM_OUT == substream->stream)) {
			iRet = g_sndp_dai_func.fsi_pointer(substream);

		/* VCD is alive */
		} else {
			if (g_call_playback_stop)
				iRet = 0;
			else
				iRet = call_pcmdata_pointer(substream);
		}
	} else if (SNDP_MODE_INCOMM ==
		SNDP_GET_MODE_VAL(GET_OLD_VALUE(substream->stream))) {
		if (((SNDP_PCM_OUT == substream->stream) &&
			(g_status & PLAY_INCOMM_STATUS)) ||
			((SNDP_PCM_IN == substream->stream) &&
			(g_status & REC_INCOMM_STATUS)))
			iRet = call_incomm_pcmdata_pointer(substream);
		else
			iRet = 0;
	} else {
		if (0 == g_loopplay)
			/* During a call */
			iRet = g_sndp_dai_func.fsi_pointer(substream);
		else
			/* PT loopback */
			iRet = call_pcmdata_pointer(substream);
	}

	sndp_log_data_rcv_indicator(substream->stream, iRet);
	return iRet;
}

/*!
   @brief Work queue function for hw free

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_hw_free(struct sndp_work_info *work)
{
	int ret;
	int direction = work->save_substream->stream;

	sndp_log_debug_func("start\n");

	ret = down_interruptible(&g_sndp_wait_free[direction]);
	if (0 != ret)
		sndp_log_err("down_interruptible ret[%d]\n", ret);

	g_sndp_hw_free_condition[direction] = true;
	wake_up(&g_sndp_hw_free_wait[direction]);

	up(&g_sndp_wait_free[direction]);

	sndp_log_debug_func("end\n");
}

static int sndp_fsi_hw_free(struct snd_pcm_substream *substream)
{
	int	ret;

	sndp_log_debug_func("start\n");

	if ((NULL != substream->runtime) &&
	    (NULL != substream->runtime->dma_area)) {
		g_sndp_hw_free_condition[substream->stream] = false;
		g_sndp_work_hw_free[substream->stream].save_substream
			= substream;

		sndp_workqueue_enqueue(g_sndp_queue_main,
			&g_sndp_work_hw_free[substream->stream]);
		sndp_log_info("enqueue\n");

		ret = wait_event_timeout(
			g_sndp_hw_free_wait[substream->stream],
			g_sndp_hw_free_condition[substream->stream],
			msecs_to_jiffies(SNDP_WAIT_MAX));

		sndp_log_info("complete ret[%d]\n", ret);

		ret = g_sndp_dai_func.fsi_hw_free(substream);
	}
	sndp_log_debug_func("end\n");

	return ERROR_NONE;
}

/*!
   @brief During a call trigger function

   @param[in]	substream	PCM substream structure
   @param[in]	cmd		Trigger command type
				(SNDRV_PCM_TRIGGER_START/
				 SNDRV_PCM_TRIGGER_STOP)
   @param[in]	dai		Digital audio interface structure
   @param[in]	value		PCM value

   @retval	none
 */
static void sndp_call_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai,
	u_int value)
{
	sndp_log_debug_func("start\n");

	/* Branch processing for each command (TRIGGER_START/TRIGGER_STOP) */
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:	/* TRIGGER_START */
		sndp_log_debug("call_%s_start\n",
				(SNDP_PCM_OUT == substream->stream) ?
						"playback" : "record");

		/* Wake Lock */
		sndp_wake_lock(E_LOCK);

		/* For during a call playback */
		if (SNDP_PCM_OUT == substream->stream) {
			/*
			 * Status change
			 * (from SNDP_STAT_IN_CALL to SNDP_STAT_IN_CALL_PLAY)
			 */
			sndp_print_status_change(GET_SNDP_STATUS(SNDP_PCM_OUT),
						 SNDP_STAT_IN_CALL_PLAY);

			SET_SNDP_STATUS(SNDP_PCM_OUT, SNDP_STAT_IN_CALL_PLAY);

			/*
			 * To register a work queue to start processing
			 * during a call playback
			 */
			g_sndp_work_call_playback_start.save_substream =
								substream;

			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_call_playback_start);

		/* For during a call capture */
		} else {
			/*
			 * Status change
			 * (from SNDP_STAT_IN_CALL to SNDP_STAT_IN_CALL_CAP)
			 */
			sndp_print_status_change(GET_SNDP_STATUS(SNDP_PCM_IN),
						 SNDP_STAT_IN_CALL_CAP);

			SET_SNDP_STATUS(SNDP_PCM_IN, SNDP_STAT_IN_CALL_CAP);

			/*
			 * To register a work queue to start processing
			 * during a call capture
			 */
			g_sndp_work_call_capture_start.save_substream =
								substream;

			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_call_capture_start);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:	/* TRIGGER_STOP */
		sndp_log_debug("call_%s_stop\n",
				(SNDP_PCM_OUT == substream->stream) ?
						"playback" : "record");

		/* For during a call playback */
		if (SNDP_PCM_OUT == substream->stream) {
			if (SNDP_ROUTE_PLAY_CHANGED & g_sndp_stream_route)
				fsi_set_trigger_stop(substream, false);
			else
				g_call_playback_stop = true;

			/*
			 * Status change
			 * (from SNDP_STAT_IN_CALL_PLAY to SNDP_STAT_IN_CALL)
			 */
			sndp_print_status_change(GET_SNDP_STATUS(SNDP_PCM_OUT),
						 SNDP_STAT_IN_CALL);

			SET_SNDP_STATUS(SNDP_PCM_OUT, SNDP_STAT_IN_CALL);

			/*
			 * To register a work queue to stop processing
			 * during a call playback
			 */
			g_sndp_work_call_playback_stop.stop.fsi_substream =
								*substream;

			g_sndp_work_call_playback_stop.stop.fsi_dai = *dai;

			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_call_playback_stop);

		/* For during a call capture */
		} else {
			/*
			 * Status change
			 * (from SNDP_STAT_IN_CALL_CAP to SNDP_STAT_IN_CALL)
			 */
			sndp_print_status_change(GET_SNDP_STATUS(SNDP_PCM_IN),
						 SNDP_STAT_IN_CALL);

			SET_SNDP_STATUS(SNDP_PCM_IN, SNDP_STAT_IN_CALL);

			/* To register a work queue to stop */
			/* processing During a call capture */
			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_call_capture_stop);
		}
		break;

	default:
		sndp_log_debug("Trigger none.\n");
		break;
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief During a fm trigger function

   @param[in]	substream	PCM substream structure
   @param[in]	cmd		Trigger command type
				(SNDRV_PCM_TRIGGER_START/
				 SNDRV_PCM_TRIGGER_STOP)
   @param[in]	dai		Digital audio interface structure
   @param[in]	value		PCM value

   @retval	none
 */
static void sndp_fm_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai,
	u_int value)
{
	struct snd_pcm_runtime	*runtime = substream->runtime;

	sndp_log_debug_func("start\n");

	/* Branch processing for each command (TRIGGER_START/TRIGGER_STOP) */
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:	/* TRIGGER_START */
		sndp_log_debug("fm_%s_start\n",
				(SNDP_PCM_OUT == substream->stream) ?
						"playback" : "record");

		/* Wake Lock */
		sndp_wake_lock(E_LOCK);

		/* For during a fm playback */
		if (SNDP_PCM_OUT == substream->stream) {

			sndp_log_info("buffer_size %ld  period_size %ld  "
				"periods %d  frame_bits %d\n",
				runtime->buffer_size, runtime->period_size,
				runtime->periods, runtime->frame_bits);
			/*
			 * To register a work queue to start processing
			 * during a fm playback
			 */
			g_sndp_work_fm_playback_start.save_substream =
								substream;

			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_fm_playback_start);

		/* For during a fm capture */
		} else {

			g_sndp_work_fm_capture_start.save_substream =
								substream;

			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_fm_capture_start);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:	/* TRIGGER_STOP */
		sndp_log_debug("fm_%s_stop\n",
				(SNDP_PCM_OUT == substream->stream) ?
						"playback" : "record");

		/* FSI trigger stop process */
		fsi_set_trigger_stop(substream, false);

		/* For during a fm playback */
		if (SNDP_PCM_OUT == substream->stream) {
			/*
			 * To register a work queue to stop processing
			 * during a fm playback
			 */
			g_sndp_work_fm_playback_stop.stop.fsi_substream =
								*substream;

			g_sndp_work_fm_playback_stop.stop.fsi_dai = *dai;

			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_fm_playback_stop);

		/* For during a fm capture */
		} else {
			/* To register a work queue to stop */
			/* processing During a fm capture */
			g_sndp_work_fm_capture_stop.stop.fsi_substream =
								*substream;

			g_sndp_work_fm_capture_stop.stop.fsi_dai = *dai;

			sndp_workqueue_enqueue(g_sndp_queue_main,
					&g_sndp_work_fm_capture_stop);
		}
		break;

	default:
		sndp_log_debug("Trigger none.\n");
		break;
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief During a VoIP trigger function

   @param[in]	substream	PCM substream structure
   @param[in]	cmd		Trigger command type
				(SNDRV_PCM_TRIGGER_START/
				 SNDRV_PCM_TRIGGER_STOP)
   @param[in]	dai		Digital audio interface structure
   @param[in]	value		PCM value

   @retval	none
 */
static void sndp_incomm_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai,
	u_int value)
{
	char			cPcm[SNDP_PCM_NAME_MAX_LEN];
	struct snd_pcm_runtime	*runtime = substream->runtime;

	sndp_log_debug_func("start\n");

	/* Branch processing for each command (TRIGGER_START/TRIGGER_STOP) */
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:	/* TRIGGER_START */
		/* Display the name of PCM */
		sndp_pcm_name_generate(
			GET_OLD_VALUE(substream->stream), cPcm);
		sndp_log_info("PCM: %s [0x%08X]\n",
			cPcm, GET_OLD_VALUE(substream->stream));

		sndp_log_info("buffer_size %ld  period_size %ld  "
			"periods %d  frame_bits %d\n",
			runtime->buffer_size, runtime->period_size,
			runtime->periods, runtime->frame_bits);

		sndp_log_debug("call_%s_incomm_start\n",
				(SNDP_PCM_OUT == substream->stream) ?
						"playback" : "record");

		/* Wake Lock */
		sndp_wake_lock(E_LOCK);

		/* For during a call playback */
		if (SNDP_PCM_OUT == substream->stream) {
			/*
			 * To register a work queue to start processing
			 * during a playback incommunication
			 */
			g_sndp_work_call_playback_incomm_start.save_substream =
								substream;
			g_call_sampling_rate[SNDP_PCM_OUT] = params_rate(
				&g_sndp_main[SNDP_PCM_OUT].arg.fsi_params);

			sndp_workqueue_enqueue(g_sndp_queue_main,
				&g_sndp_work_call_playback_incomm_start);

		/* For during a call capture */
		} else {
			/*
			 * To register a work queue to start processing
			 * during a capture incommunication
			 */
			g_sndp_work_call_capture_incomm_start.save_substream =
								substream;
			g_call_sampling_rate[SNDP_PCM_IN] = params_rate(
				&g_sndp_main[SNDP_PCM_IN].arg.fsi_params);

			sndp_workqueue_enqueue(g_sndp_queue_main,
				&g_sndp_work_call_capture_incomm_start);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:	/* TRIGGER_STOP */
		sndp_log_debug("call_%s_incomm_stop\n",
				(SNDP_PCM_OUT == substream->stream) ?
						"playback" : "record");

		/* For during a playback incommunication */
		if (SNDP_PCM_OUT == substream->stream) {
			/*
			 * To register a work queue to stop processing
			 * during a call playback
			 */
			g_sndp_work_call_playback_incomm_stop.stop.fsi_substream =
								*substream;

			g_sndp_work_call_playback_incomm_stop.stop.fsi_dai =
								*dai;

			sndp_workqueue_enqueue(g_sndp_queue_main,
				&g_sndp_work_call_playback_incomm_stop);

		/* For during a capture incommunication */
		} else {
			/* To register a work queue to stop */
			/* processing During a call capture */
			sndp_workqueue_enqueue(g_sndp_queue_main,
				&g_sndp_work_call_capture_incomm_stop);
		}
		break;

	default:
		sndp_log_debug("Trigger none.\n");
		break;
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Voice Start

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_voice_start(struct sndp_work_info *work)
{
	int		iRet = ERROR_NONE;
	u_int		uiDirection = SNDP_GET_DIRECTION_VAL(work->new_value);

	struct snd_soc_codec *codec =
		(struct snd_soc_codec *)g_kcontrol->private_data;
	struct snd_soc_card *card = codec->card;

	sndp_log_debug_func("start\n");

	sndp_log_info("Get\n");
	/* Enable the power domain */
	iRet = sndp_pm_runtime_sync(E_PM_GET,
			SNDP_VALUE_INIT, SNDP_VALUE_INIT);
	if (!(0 == iRet || 1 == iRet)) {  /* 0:success 1:active */
		/* Revert the status */
		sndp_print_status_change(GET_SNDP_STATUS(uiDirection),
					 work->save_status);
		SET_SNDP_STATUS(uiDirection, work->save_status);
		return;
	}

#ifdef __SNDP_INCALL_CLKGEN_MASTER
	/* CLKGEN master setting */
	common_set_fsi2cr(SNDP_NO_DEVICE, STAT_OFF);
#else /* !__SNDP_INCALL_CLKGEN_MASTER */
	if (!(SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(work->new_value)))
		/* FSI master setting */
		common_set_pll22(work->new_value,
				STAT_ON,
				g_bluetooth_band_frequency);
	else
		/* CLKGEN master setting */
		common_set_fsi2cr(SNDP_NO_DEVICE, STAT_OFF);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

	/* Standby restraint */
	iRet = fsi_d2153_enable_ignore_suspend(card, 0);
	if (ERROR_NONE != iRet) {
		sndp_log_err("ignore_suspend error(code=%d)\n", iRet);
		goto start_err;
	}

	/* start SCUW */
	iRet = scuw_start(work->new_value, g_bluetooth_band_frequency);
	if (ERROR_NONE != iRet) {
		sndp_log_err("scuw start error(code=%d)\n", iRet);
		goto start_err;
	}

#ifndef __SNDP_INCALL_CLKGEN_MASTER
	if (!(SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(work->new_value))) {
		wait_event_interruptible_timeout(
				g_watch_start_clk_queue,
				atomic_read(&g_sndp_watch_start_clk),
				msecs_to_jiffies(SNDP_WATCH_CLK_TIME_OUT));

		if (0 == atomic_read(&g_sndp_watch_start_clk))
			sndp_log_err("watch clk timeout\n");
	}
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

	/* start FSI */
	iRet = fsi_start(work->new_value, 1);
	if (ERROR_NONE == iRet) {
		/* all down link mute control */
		fsi_all_dl_mute_ctrl(g_dl_mute_flg);
	} else {
		sndp_log_err("fsi start error(code=%d)\n", iRet);
		goto start_err;
	}

#ifndef __SNDP_INCALL_CLKGEN_MASTER
	if (SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(work->new_value)) {
		wait_event_interruptible_timeout(
				g_watch_start_clk_queue,
				atomic_read(&g_sndp_watch_start_clk),
				msecs_to_jiffies(SNDP_WATCH_CLK_TIME_OUT));

		if (0 == atomic_read(&g_sndp_watch_start_clk))
			sndp_log_err("watch clk timeout\n");
	}
#else /* !__SNDP_INCALL_CLKGEN_MASTER */
	wait_event_interruptible_timeout(
		g_watch_start_clk_queue, atomic_read(&g_sndp_watch_start_clk),
		msecs_to_jiffies(SNDP_WATCH_CLK_TIME_OUT));

	if (0 == atomic_read(&g_sndp_watch_start_clk))
		sndp_log_err("watch clk timeout\n");
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

	/* start CLKGEN */
	iRet = clkgen_start(work->new_value, 0, g_bluetooth_band_frequency);
	if (ERROR_NONE != iRet) {
		sndp_log_err("clkgen start error(code=%d)\n", iRet);
		goto start_err;
	}

	/* FSI FIFO reset */
	if (!(SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(work->new_value)))
		fsi_fifo_reset(SNDP_PCM_PORTA);
	else
		fsi_fifo_reset(SNDP_PCM_PORTB);

	/* Set FSIIR_FSIF */
	scuw_set_fsiir();

	/* Output device ON */
	fsi_d2153_set_dac_power(g_kcontrol, 1);

	/* Input device ON */
	fsi_d2153_set_adc_power(g_kcontrol, 1);

	sndp_extdev_set_state(SNDP_GET_MODE_VAL(work->new_value),
		SNDP_GET_AUDIO_DEVICE(work->new_value),
		SNDP_EXTDEV_START);

start_err:
	g_sndp_start_call_wait = 1;
	wake_up_interruptible(&g_sndp_start_call_queue);
	/* Wake Unlock */
	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Voice Stop

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_voice_stop(struct sndp_work_info *work)
{
	int			iRet = ERROR_NONE;

	struct snd_soc_codec *codec =
		(struct snd_soc_codec *)g_kcontrol->private_data;
	struct snd_soc_card *card = codec->card;

	sndp_log_debug_func("start\n");

        sndp_log_info("sndp_work_voice_stop: Waiting for g_watch_stop_clk for %d msecs..\n",
                        SNDP_WATCH_CLK_TIME_OUT);

	g_sndp_start_call_wait = 0;

	if (SNDP_MODE_INCOMM != SNDP_GET_MODE_VAL(work->new_value)) {
		wait_event_interruptible_timeout(
			g_watch_stop_clk_queue, atomic_read(&g_sndp_watch_stop_clk),
			msecs_to_jiffies(SNDP_WATCH_CLK_TIME_OUT));

		if (0 == atomic_read(&g_sndp_watch_stop_clk))
			sndp_log_err("watch clk timeout\n");
	}

	/* stop SCUW */
	scuw_stop();

	/* stop FSI */
	fsi_stop(1);

	/* stop CLKGEN */
	clkgen_stop();

	sndp_extdev_set_state(SNDP_GET_MODE_VAL(work->old_value),
			     SNDP_GET_AUDIO_DEVICE(work->old_value),
			     SNDP_EXTDEV_STOP);

	sndp_log_info("Put\n");
	/* Disable the power domain */
	iRet = sndp_pm_runtime_sync(E_PM_PUT, work->new_value,
				GET_OTHER_VALUE(work->old_value));

#ifdef __SNDP_INCALL_CLKGEN_MASTER
	/* FSI2CR initialize */
	common_set_fsi2cr(SNDP_NO_DEVICE, STAT_ON);
#else /* !__SNDP_INCALL_CLKGEN_MASTER */
	/* FSI master process */
	common_set_pll22(work->old_value,
			STAT_OFF,
			g_bluetooth_band_frequency);

	/* FSI2CR initialize */
	common_set_fsi2cr(SNDP_NO_DEVICE, STAT_ON);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

	/* Release standby restraint */
	iRet = fsi_d2153_disable_ignore_suspend(card, 0);
	if (ERROR_NONE != iRet)
		sndp_log_err("release ignore_suspend error(code=%d)\n", iRet);

	/* Wake Force Unlock */
	sndp_wake_lock(E_FORCE_UNLOCK);

        sndp_log_info("sndp_work_voice_stop-->end\n");

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Device change (IN_CALL)

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_voice_dev_chg(struct sndp_work_info *work)
{
	int			iRet = ERROR_NONE;
	u_int			old_dev = SNDP_NO_DEVICE;
	u_int			new_dev = SNDP_NO_DEVICE;

	sndp_log_debug_func("start\n");

	/* get Device type */
	old_dev = SNDP_GET_DEVICE_VAL(work->old_value);
	new_dev = SNDP_GET_DEVICE_VAL(work->new_value);

	/* AudioLSI -> BT(SCO) */
	if ((SNDP_BLUETOOTHSCO != old_dev) &&
	    (SNDP_BLUETOOTHSCO == new_dev)) {
		iRet = sndp_work_voice_dev_chg_audioic_to_bt(
			work->old_value, work->new_value);
		if (ERROR_NONE != iRet)
			sndp_log_err(
				"voice_dev_chg_to_bt error(code=%d)\n", iRet);
	/* BT(SCO) -> AudioLSI */
	} else if ((SNDP_BLUETOOTHSCO == old_dev) &&
		  (SNDP_BLUETOOTHSCO != new_dev)) {
		iRet = sndp_work_voice_dev_chg_bt_to_audioic(
			work->old_value, work->new_value);
		if (ERROR_NONE != iRet)
			sndp_log_err(
				"voice_dev_chg_bt_to_audioic error(code=%d)\n",
				iRet);
	/* AudioLSI -> AudioLSI */
	} else if ((SNDP_BLUETOOTHSCO != old_dev) &&
		  (SNDP_BLUETOOTHSCO != new_dev)) {
		iRet = sndp_work_voice_dev_chg_in_audioic(
			work->old_value, work->new_value);
		if (ERROR_NONE != iRet)
			sndp_log_err(
				"voice_dev_chg_in_audioic error(code=%d)\n",
				iRet);
	/* BT(SCO) -> BT(SCO) */
	} else {
		/* Without processing */
	}

	sndp_extdev_set_state(SNDP_GET_MODE_VAL(work->new_value),
			     SNDP_GET_AUDIO_DEVICE(work->new_value),
			     SNDP_EXTDEV_CH_DEV);

	/* Wake Unlock */
	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Device change AudioLSI -> BT-SCO (IN_CALL)
	  Subfunction of the sndp_work_voice_dev_chg()

   @param[in]	old_value	last PCM value
   @param[in]	new_value	new PCM value
   @param[out]

   @retval	0		Successful
 */
static int sndp_work_voice_dev_chg_audioic_to_bt(
	const u_int old_value,
	const u_int new_value)
{
	int	iRet = ERROR_NONE;


	sndp_log_debug_func("start\n");

	/* Input device OFF */
	fsi_d2153_set_adc_power(g_kcontrol, 0);

	/* stop SCUW */
	scuw_stop();

	/* stop FSI */
	fsi_stop(1);

	/* stop CLKGEN */
	clkgen_stop();

#ifndef __SNDP_INCALL_CLKGEN_MASTER
	/* CLKGEN master setting */
	common_set_pll22(old_value, STAT_OFF, g_bluetooth_band_frequency);
	common_set_fsi2cr(SNDP_NO_DEVICE, STAT_OFF);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

	/* start SCUW */
	iRet = scuw_start(new_value, g_bluetooth_band_frequency);
	if (ERROR_NONE != iRet)
		sndp_log_err("scuw start error(code=%d)\n", iRet);

	/* start FSI */
	iRet = fsi_start(new_value, 1);
	if (ERROR_NONE == iRet)
		/* all down link mute control */
		fsi_all_dl_mute_ctrl(g_dl_mute_flg);
	else
		sndp_log_err("fsi start error(code=%d)\n", iRet);

	/* start CLKGEN */
	iRet = clkgen_start(new_value, 0, g_bluetooth_band_frequency);
	if (ERROR_NONE != iRet)
		sndp_log_err("clkgen start error(code=%d)\n", iRet);

	/* FSI FIFO reset */
	fsi_fifo_reset(SNDP_PCM_PORTB);

	/* Set FSIIR_FSIF */
	scuw_set_fsiir();

	sndp_log_debug_func("end\n");

	return ERROR_NONE;
}


/*!
   @brief Device change BT-SCO -> AudioLSI (IN_CALL)
	  Subfunction of the sndp_work_voice_dev_chg()

   @param[in]	old_value	last PCM value
   @param[in]	new_value	new PCM value
   @param[out]

   @retval	0		Successful
 */
static int sndp_work_voice_dev_chg_bt_to_audioic(
	const u_int old_value,
	const u_int new_value)
{
	int	iRet = ERROR_NONE;

	sndp_log_debug_func("start\n");

	/* stop SCUW */
	scuw_stop();

	/* stop FSI */
	fsi_stop(1);

	/* stop CLKGEN */
	clkgen_stop();

#ifndef __SNDP_INCALL_CLKGEN_MASTER
	/* FSI master setting */
	common_set_pll22(new_value,
			STAT_ON,
			g_bluetooth_band_frequency);

	common_set_fsi2cr(SNDP_NO_DEVICE, STAT_ON);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

	/* start SCUW */
	iRet = scuw_start(new_value, g_bluetooth_band_frequency);
	if (ERROR_NONE != iRet)
		sndp_log_err("scuw start error(code=%d)\n", iRet);

	/* start FSI */
	iRet = fsi_start(new_value, 1);
	if (ERROR_NONE == iRet)
		/* all down link mute control */
		fsi_all_dl_mute_ctrl(g_dl_mute_flg);
	else
		sndp_log_err("fsi start error(code=%d)\n", iRet);

	/* start CLKGEN */
	iRet = clkgen_start(new_value, 0, g_bluetooth_band_frequency);
	if (ERROR_NONE != iRet)
		sndp_log_err("clkgen start error(code=%d)\n", iRet);

	/* FSI FIFO reset */
	fsi_fifo_reset(SNDP_PCM_PORTA);

	/* Set FSIIR_FSIF */
	scuw_set_fsiir();

	/* Output device ON */
	fsi_d2153_set_dac_power(g_kcontrol, 1);

	/* Input device ON */
	fsi_d2153_set_adc_power(g_kcontrol, 1);

	sndp_log_debug_func("end\n");

	return ERROR_NONE;
}


/*!
   @brief Device change AudioLSI -> AudioLSI (IN_CALL)
	  Subfunction of the sndp_work_voice_dev_chg()

   @param[in]	old_value	last PCM value
   @param[in]	new_value	new PCM value
   @param[out]

   @retval	0		Successful
 */
static int sndp_work_voice_dev_chg_in_audioic(
	const u_int old_value,
	const u_int new_value)
{
	sndp_log_debug_func("start\n");
	/* Output device ON */
	fsi_d2153_set_dac_power(g_kcontrol, 1);

	if (SNDP_PCM_IN == SNDP_GET_DIRECTION_VAL(new_value))
		/* Input device ON */
		fsi_d2153_set_adc_power(g_kcontrol, 1);

	sndp_log_debug_func("end\n");

	return ERROR_NONE;
}


/*!
   @brief Work queue function for FM Radio Device change

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_fm_radio_dev_chg(struct sndp_work_info *work)
{

	sndp_log_debug_func("start\n");

	msleep(600);  /* to avoid audio shock according to ear,spk volume level */

	/* Output device ON */
	fsi_d2153_set_dac_power(g_kcontrol, 1);

	sndp_extdev_set_state(SNDP_GET_MODE_VAL(work->new_value),
		SNDP_GET_AUDIO_DEVICE(work->new_value),
		SNDP_EXTDEV_CH_DEV);

	/* Wake Unlock */
	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for SoundPath Setting(Playback)

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_play_start(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Running Playback */
	g_sndp_playrec_flg |= E_PLAY;

	/* To register a work queue to start processing Playback */
	sndp_work_start(SNDP_PCM_OUT);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for SoundPath Setting(Capture)

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_capture_start(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Running Capture */
	g_sndp_playrec_flg |= E_CAP;

	/* To register a work queue to start processing Capture */
	sndp_work_start(SNDP_PCM_IN);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for SoundPath Stop(Playback)

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_play_stop(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Stop Playback runnning */
	g_sndp_playrec_flg &= ~E_PLAY;

	/* To register a work queue to stop processing Playback */
	sndp_work_stop(work, SNDP_PCM_OUT);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for SoundPath Stop(Capture)

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_capture_stop(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Stop Capture running */
	g_sndp_playrec_flg &= ~E_CAP;

	/* To register a work queue to stop processing Capture */
	sndp_work_stop(work, SNDP_PCM_IN);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for SoundPath Setting(Playback incommunication)

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_play_incomm_start(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");
	/* Standby restraint */

	/* Running Playback */
	g_sndp_incomm_playrec_flg |= E_PLAY;

	sndp_work_incomm_start(work->new_value, work->save_status);

	/* Wake Unlock */
	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for SoundPath Stop(Playback incommunication)

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_play_incomm_stop(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Stop Playback runnning */
	g_sndp_incomm_playrec_flg &= ~E_PLAY;

	sndp_log_info("g_sndp_incomm_playrec_flg[0x%02x]\n",
					g_sndp_incomm_playrec_flg);

	if (!g_sndp_incomm_playrec_flg) {
		/* To register a work queue to stop processing Playback */
		sndp_work_incomm_stop(work->old_value, work->new_value);
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for SoundPath Setting(Capture incommunication)

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_capture_incomm_start(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Running Capture */
	g_sndp_incomm_playrec_flg |= E_CAP;

	/* Input device ON */
	fsi_d2153_set_adc_power(g_kcontrol, 1);

	/* Wake Unlock */
	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for SoundPath Stop(Capture incommunication)

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_capture_incomm_stop(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Stop Capture running */
	g_sndp_incomm_playrec_flg &= ~E_CAP;

	if (!g_sndp_incomm_playrec_flg) {
		/* To register a work queue to stop processing Playback */
		sndp_work_incomm_stop(work->old_value, work->new_value);
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Incommunication Start

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_incomm_start(const u_int new_value,
					const u_int save_status)
{
	int	ret = ERROR_NONE;
	u_int	dir = SNDP_GET_DIRECTION_VAL(new_value);

	struct snd_soc_codec *codec =
		(struct snd_soc_codec *)g_kcontrol->private_data;
	struct snd_soc_card *card = codec->card;

	sndp_log_debug_func("start\n");

	sndp_log_info("Get\n");
	/* Enable the power domain */
	ret = sndp_pm_runtime_sync(E_PM_GET,
			SNDP_VALUE_INIT, SNDP_VALUE_INIT);
	if (!(0 == ret || 1 == ret)) {  /* 0:success 1:active */
		/* Revert the status */
		sndp_print_status_change(GET_SNDP_STATUS(dir), save_status);
		SET_SNDP_STATUS(dir, save_status);
		return;
	}

	ret = fsi_d2153_enable_ignore_suspend(card, 0);
	if (ERROR_NONE != ret) {
		sndp_log_err("ignore_suspend error(code=%d)\n", ret);
		goto start_err;
	}
#ifdef __SNDP_INCALL_CLKGEN_MASTER
	/* CLKGEN master setting */
	common_set_fsi2cr(SNDP_NO_DEVICE, STAT_OFF);
#else /* !__SNDP_INCALL_CLKGEN_MASTER */
	if (!(SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(new_value)))
		/* FSI master setting */
		common_set_pll22(new_value,
				STAT_ON,
				g_bluetooth_band_frequency);
	else
		/* CLKGEN master setting */
		common_set_fsi2cr(SNDP_NO_DEVICE, STAT_OFF);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

	/* start SCUW */
	ret = scuw_start(new_value, g_bluetooth_band_frequency);
	if (ERROR_NONE != ret) {
		sndp_log_err("scuw start error(code=%d)\n", ret);
		goto start_err;
	}

#ifndef __SNDP_INCALL_CLKGEN_MASTER
	if (SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(new_value)) {
		wait_event_interruptible_timeout(
			g_watch_start_clk_queue,
			atomic_read(&g_sndp_watch_start_clk),
			msecs_to_jiffies(SNDP_WATCH_CLK_TIME_OUT));

		if (0 == atomic_read(&g_sndp_watch_start_clk))
			sndp_log_err("watch clk timeout\n");
	}
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

	/* start FSI */
	ret = fsi_start(new_value, 1);
	if (ERROR_NONE != ret) {
		sndp_log_err("fsi start error(code=%d)\n", ret);
		goto start_err;
	}

#ifdef __SNDP_INCALL_CLKGEN_MASTER
	wait_event_interruptible_timeout(
		g_watch_start_clk_queue, atomic_read(&g_sndp_watch_start_clk),
		msecs_to_jiffies(SNDP_WATCH_CLK_TIME_OUT));

	if (0 == atomic_read(&g_sndp_watch_start_clk))
		sndp_log_err("watch clk timeout\n");

#endif /* __SNDP_INCALL_CLKGEN_MASTER */

	/* start CLKGEN */
	ret = clkgen_start(new_value, 0, g_bluetooth_band_frequency);
	if (ERROR_NONE != ret) {
		sndp_log_err("clkgen start error(code=%d)\n", ret);
		goto start_err;
	}

	/* FSI FIFO reset */
	if (!(SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(new_value)))
		fsi_fifo_reset(SNDP_PCM_PORTA);
	else
		fsi_fifo_reset(SNDP_PCM_PORTB);

	/* Set FSIIR_FSIF */
	scuw_set_fsiir();

	/* Output device ON */
	fsi_d2153_set_dac_power(g_kcontrol, 1);

	sndp_extdev_set_state(SNDP_GET_MODE_VAL(new_value),
			     SNDP_GET_AUDIO_DEVICE(new_value),
			     SNDP_EXTDEV_START);


start_err:
	g_sndp_start_call_wait = 1;
	wake_up_interruptible(&g_sndp_start_call_queue);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Incommunication Stop

   @param[in]	old_value	PCM Value
   @param[out]	none

   @retval	none
 */
static void sndp_work_incomm_stop(const u_int old_value, const u_int new_value)
{
	int	ret = ERROR_NONE;

	struct snd_soc_codec *codec =
		(struct snd_soc_codec *)g_kcontrol->private_data;
	struct snd_soc_card *card = codec->card;

	sndp_log_debug_func("start\n");

/*	g_sndp_start_call_wait = 0;*/
	if (SNDP_MODE_INCALL != SNDP_GET_MODE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT))) {
		/* stop SCUW */
		scuw_stop();

		/* stop FSI */
		fsi_stop(1);

		/* stop CLKGEN */
		clkgen_stop();

		sndp_extdev_set_state(SNDP_GET_MODE_VAL(old_value),
					SNDP_GET_AUDIO_DEVICE(old_value),
					SNDP_EXTDEV_STOP);
	}

	sndp_log_info("Put\n");
	/* Disable the power domain */
	ret = sndp_pm_runtime_sync(E_PM_PUT,
			new_value, GET_OTHER_VALUE(old_value));

	if (SNDP_MODE_INCALL != SNDP_GET_MODE_VAL(GET_OLD_VALUE(SNDP_PCM_OUT))) {
#ifdef __SNDP_INCALL_CLKGEN_MASTER
		/* CLKGEN master process */
		common_set_fsi2cr(SNDP_NO_DEVICE, STAT_ON);
#else /* !__SNDP_INCALL_CLKGEN_MASTER */
		if (!(SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(old_value)))
			/* FSI master process */
			common_set_pll22(old_value,
					STAT_OFF,
					g_bluetooth_band_frequency);
		else
			/* CLKGEN master process */
			common_set_fsi2cr(SNDP_NO_DEVICE, STAT_ON);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

		/* Release standby restraint */
		ret = fsi_d2153_disable_ignore_suspend(card, 0);
		if (ERROR_NONE != ret)
			sndp_log_err("release ignore_suspend error(code=%d)\n", ret);

		if (g_dfs_mode_min_flag) {
			sndp_log_info("enable dfs mode min\n");
			enable_dfs_mode_min();
		} else {
			sndp_log_info("start cpufreq()\n");
			start_cpufreq();
		}
	} else {
		sndp_log_debug("OUT=IN_CALL\n");
	}

	/* Wake Force Unlock */
	sndp_wake_lock(E_FORCE_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Switching path of the sound during a Call + Playback

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void sndp_call_playback_normal(void)
{
	sndp_log_debug_func("start\n");

	if (!(SNDP_ROUTE_PLAY_CHANGED & g_sndp_stream_route)) {
		sndp_path_switching(GET_OLD_VALUE(SNDP_PCM_OUT));
		g_sndp_stream_route |= SNDP_ROUTE_PLAY_CHANGED;
	}

	sndp_log_debug_func("end\n");
}

/*!
   @brief Work queue function for Start during a call playback

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_call_playback_start(struct sndp_work_info *work)
{
	int			iRet = ERROR_NONE;

	sndp_log_debug_func("start\n");

	/* Call + Playback start request */
	iRet = call_playback_start(work->save_substream);

	if (ERROR_NONE != iRet)
		/* Switching path of the sound during a Call + Playback */
		sndp_call_playback_normal();

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Start during a call capture

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_call_capture_start(struct sndp_work_info *work)
{
	int			iRet = ERROR_NONE;

	sndp_log_debug_func("start\n");

	/* Call + Capture start request */
	iRet = call_record_start(work->save_substream);

	if (ERROR_NONE != iRet) {
		/* Dummy capture start */
		if (!(SNDP_ROUTE_CAP_DUMMY & g_sndp_stream_route)) {
			call_change_dummy_rec();
			g_sndp_stream_route |= SNDP_ROUTE_CAP_DUMMY;
		}
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Stop during a call playback

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_call_playback_stop(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Call + Playback stop request */
	call_playback_stop();

	/* Back-out path of the sound during a Call + Playback */
	if (SNDP_ROUTE_PLAY_CHANGED & g_sndp_stream_route) {
		sndp_path_backout(GET_OLD_VALUE(SNDP_PCM_OUT));
		g_sndp_stream_route &= ~SNDP_ROUTE_PLAY_CHANGED;
	}

	g_call_playback_stop = false;

	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Stop during a call capture

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_call_capture_stop(struct sndp_work_info *work)
{
	u_int in_old_val = GET_OLD_VALUE(SNDP_PCM_IN);
	u_int out_old_val = GET_OLD_VALUE(SNDP_PCM_OUT);
	int			ret;

	sndp_log_debug_func("start\n");

	if (SNDP_ROUTE_CAP_DUMMY & g_sndp_stream_route) {
		/* Dummy capture stop */
		g_sndp_stream_route &= ~SNDP_ROUTE_CAP_DUMMY;
	}

	/* Call + Capture stop request */
	call_record_stop();

	if ((!(SNDP_ROUTE_PLAY_CHANGED & g_sndp_stream_route)) &&
		(SNDP_MODE_INCALL == SNDP_GET_MODE_VAL(in_old_val))) {
		if (SNDP_MODE_NORMAL == SNDP_GET_MODE_VAL(out_old_val)) {
			/*
			 * Voice stop and Normal device change
			 * (Post-processing of this function)
			 */

			/* Input device OFF */
			fsi_d2153_set_adc_power(g_kcontrol, 0);
			sndp_after_of_work_call_capture_stop(in_old_val, out_old_val);
		} else if (SNDP_MODE_INCOMM == SNDP_GET_MODE_VAL(out_old_val)) {
			sndp_wake_lock(E_FORCE_UNLOCK);
		}
	}

	sndp_log_info("Put\n");
	/* Disable the power domain */
	ret = sndp_pm_runtime_sync(E_PM_PUT, in_old_val, out_old_val);

	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Start during a fm playback

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_fm_playback_start(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* To register a work queue to start processing Playback */
	sndp_fm_work_start(SNDP_PCM_OUT);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Start during a fm capture

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_fm_capture_start(struct sndp_work_info *work)
{

	sndp_log_debug_func("start\n");

	/* To register a work queue to start processing Capture */
	sndp_fm_work_start(SNDP_PCM_IN);

	sndp_log_debug_func("end\n");

}


/*!
   @brief Work queue function for Stop during a fm playback

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_fm_playback_stop(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* To register a work queue to stop processing Playback */
	sndp_fm_work_stop(work, SNDP_PCM_OUT);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Stop during a fm capture

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_fm_capture_stop(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* To register a work queue to stop processing Capture */
	sndp_fm_work_stop(work, SNDP_PCM_IN);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Start during a playback incommunication

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_call_playback_incomm_start(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/*  Playback incommunication start request */
	call_playback_incomm_start(work->save_substream);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Start during a capture incommunication

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_call_capture_incomm_start(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Capture incommunication start request */
	call_record_incomm_start(work->save_substream);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Stop during a playback incommunication

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_call_playback_incomm_stop(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Playback incommunication stop request */
	call_playback_incomm_stop();

	/* Wake Unlock */
	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for Stop during a capture incommunication

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_call_capture_incomm_stop(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");


	/* Capture incommunication stop request */
	call_record_incomm_stop();

	/* Wake Unlock */
	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


#if 0
/**
 * Not used function.
 */
 */
/*!

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_pt_playback_start(struct sndp_work_info *work)
{
	int	iRet = ERROR_NONE;

	sndp_log_debug_func("start\n");

	/* Running Playback */
	g_sndp_playrec_flg |= E_PLAY;

	/* set Audience state (start) */
#if 0
	sndp_a2220_set_state(SNDP_MODE_NORMAL, g_pt_device, SNDP_A2220_START);
#endif

	/* FSI Trigger start */
	if (NULL != g_sndp_dai_func.fsi_trigger) {
		sndp_log_debug("fsi_dai_trigger start in PT\n");

		iRet = g_sndp_dai_func.fsi_trigger(
			g_sndp_main[SNDP_PCM_OUT].arg.fsi_substream,
			SNDRV_PCM_TRIGGER_START,
			g_sndp_main[SNDP_PCM_OUT].arg.fsi_dai);

		if (ERROR_NONE != iRet)
			sndp_log_err("fsi_trigger error(code=%d)\n", iRet);
	}

	sndp_log_debug_func("end\n");
}
#endif

#if 0
/**
 * Not used function.
 */
/*!
   @brief Work queue function for Stop during a PT playback

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_pt_playback_stop(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* Stop Playback runnning */
	g_sndp_playrec_flg &= ~E_PLAY;

	/* FSI Trigger stop */
	if (NULL != g_sndp_dai_func.fsi_trigger) {
		sndp_log_debug("fsi_dai_trigger stop in PT\n");
		g_sndp_dai_func.fsi_trigger(&(work->stop.fsi_substream),
					    SNDRV_PCM_TRIGGER_STOP,
					    &(work->stop.fsi_dai));
	}

	/* set Audience state (stop) */
#if 0
	sndp_a2220_set_state(SNDP_MODE_NORMAL, g_pt_device, SNDP_A2220_STOP);
#endif

	/* Wake Unlock or Force Unlock */
	sndp_wake_lock((g_sndp_playrec_flg) ? E_UNLOCK : E_FORCE_UNLOCK);

	sndp_log_debug_func("end\n");
}
#endif

/*!
   @brief Work queue processing for all down link mute control

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_all_dl_mute(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");
	sndp_log_info("all_dl_mute=%s\n",
		(false == g_dl_mute_flg) ? "false" : "true");

	/* Control to output mute on/off,         */
	/* when during a call or FM reproduction. */
	fsi_all_dl_mute_ctrl(g_dl_mute_flg);

	sndp_log_debug_func("end\n");
}


/*!
   @brief VCD_COMMAND_WATCH_STOP_FW registration process

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void sndp_regist_watch(void)
{
	int	iRet = ERROR_NONE;
	struct call_vcd_callback	func;


	sndp_log_debug_func("start\n");

	/* Standby restraint */
	func.callback_start_fw = sndp_watch_start_fw_cb;
	func.callback_stop_fw = sndp_watch_stop_fw_cb;
	func.callback_start_clk = sndp_watch_start_clk_cb;
	func.callback_stop_clk = sndp_watch_stop_clk_cb;
	func.callback_wait_path = sndp_wait_path_cb;
	func.callback_codec_type = sndp_codec_type_cb;

	iRet = call_regist_watch(&func);
	if (ERROR_NONE  != iRet)
		sndp_log_err("VCD watch command set error(code=%d)\n", iRet);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue processing for VCD_COMMAND_WATCH_STOP_FW process

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_watch_stop_fw(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	/* During a Call + Capture */
	if (SNDP_STAT_IN_CALL_CAP == GET_SNDP_STATUS(SNDP_PCM_IN)) {
		/* Dummy capture start */
		if (!(SNDP_ROUTE_CAP_DUMMY & g_sndp_stream_route)) {
			call_change_dummy_rec();
			g_sndp_stream_route |= SNDP_ROUTE_CAP_DUMMY;
		}
	}

	/* During a Call + Playback */
	if (SNDP_STAT_IN_CALL_PLAY == GET_SNDP_STATUS(SNDP_PCM_OUT)) {
		/* Switching path of the sound during a Call + Playback */
		if (!(SNDP_ROUTE_PLAY_CHANGED & g_sndp_stream_route)) {
			call_change_dummy_play();
		}
	}

	/* During a VoIP */
	if ((SNDP_STAT_IN_COMM == GET_SNDP_STATUS(SNDP_PCM_OUT)) ||
		(SNDP_STAT_IN_COMM == GET_SNDP_STATUS(SNDP_PCM_IN))) {
		/* Initialization of the firmware status flag */
		atomic_set(&g_call_watch_stop_fw, 1);
		/* Dummy play start (VoIP) */
		if (SNDP_STAT_IN_COMM == GET_SNDP_STATUS(SNDP_PCM_OUT))
			call_change_incomm_play();

		/* Dummy record start (VoIP) */
		if (SNDP_STAT_IN_COMM == GET_SNDP_STATUS(SNDP_PCM_IN))
			call_change_incomm_rec();
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief Watch start Firmware notification callback function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void sndp_watch_start_fw_cb(void)
{
	sndp_log_debug_func("start\n");
	sndp_log_info("FW was started <incall or incommunication>\n");

	atomic_set(&g_call_watch_start_fw, 1);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Wait set path notification callback function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void sndp_wait_path_cb(void)
{
	sndp_log_debug_func("start\n");

	/*
	 * Registered in the work queue for
	 * VCD_COMMAND_WAIT_PATH process
	 */
	wait_event_interruptible(g_sndp_start_call_queue,
				g_sndp_start_call_wait);
	sndp_log_info("Wake up\n");

	sndp_log_debug_func("end\n");
}


/*!
   @brief Watch stop Firmware notification callback function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void sndp_watch_stop_fw_cb(void)
{
	sndp_log_debug_func("start\n");
	sndp_log_info("VCD is close <incall or incommunication>\n");

	/*
	 * Registered in the work queue for
	 * VCD_COMMAND_WATCH_STOP_FW process
	 */
	sndp_workqueue_enqueue(g_sndp_queue_main, &g_sndp_work_watch_stop_fw);

	sndp_log_debug_func("end\n");
}

/*!
   @brief Watch codec type(WB/NB) callback function

   @param[in]	codec_type	WB/NB
   @param[out]	none

   @retval	none
 */
static void sndp_codec_type_cb(u_int codec_type)
{
	int			ret;

	sndp_log_debug_func("start\n");

	if (!g_sndp_extdev_callback) {
		sndp_log_info("struct address is NULL\n");
		return;
	}

	if (g_sndp_extdev_callback->set_nb_wb) {
		sndp_log_info("extdev set_nb_wb\n");
		ret = g_sndp_extdev_callback->set_nb_wb(codec_type);
		if (ERROR_NONE != ret)
			sndp_log_err("set_nb_wb error [%d]\n", ret);
	} else {
		sndp_log_info("set_nb_wb is NULL\n");
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for FM Radio start

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_fm_radio_start(struct sndp_work_info *work)
{
	int			iRet = ERROR_NONE;
	u_int			dev = SNDP_GET_DEVICE_VAL(work->new_value);

	struct snd_soc_codec *codec =
		(struct snd_soc_codec *)g_kcontrol->private_data;
	struct snd_soc_card *card = codec->card;

	sndp_log_debug_func("start\n");

	/* Standby restraint */
	iRet = fsi_d2153_enable_ignore_suspend(card, 0);
	if (ERROR_NONE != iRet) {
		sndp_log_err("ignore_suspend error(code=%d)\n", iRet);
		goto start_err;
	}

	sndp_log_info("Get\n");
	/* Enable the power domain */
	iRet = sndp_pm_runtime_sync(E_PM_GET,
			SNDP_VALUE_INIT, SNDP_VALUE_INIT);

	if (!((E_FM_PLAY & g_sndp_playrec_flg) &&
	      (E_FM_CAP  & g_sndp_playrec_flg))) {

		if (SNDP_IS_FSI_MASTER_DEVICE(dev)) {
			/* FSI master */
			common_set_pll22(work->new_value,
					 STAT_ON,
					 g_bluetooth_band_frequency);
		} else {
			sndp_log_err("FM CLKGEN master not supported\n");
		}
	}

	/* start SCUW */
	iRet = scuw_start(work->new_value, g_bluetooth_band_frequency);
	if (ERROR_NONE != iRet) {
		sndp_log_err("scuw start error(code=%d)\n", iRet);
		goto start_err;
	}

	/* start FSI */
	if ((E_PLAY | E_CAP) & g_sndp_playrec_flg)
		iRet = fsi_start(work->new_value, 0);
	else
		iRet = fsi_start(work->new_value, 1);
	if (ERROR_NONE == iRet) {
		/* all down link mute control */
		fsi_all_dl_mute_ctrl(g_dl_mute_flg);
	} else {
		sndp_log_err("fsi start error(code=%d)\n", iRet);
		goto start_err;
	}

	/* start CLKGEN */
	iRet = clkgen_start(work->new_value,
			    SNDP_NORMAL_RATE,
			    g_bluetooth_band_frequency);
	if (ERROR_NONE != iRet) {
		sndp_log_err("clkgen start error(code=%d)\n", iRet);
		goto start_err;
	}

	/* Output device ON */
	fsi_d2153_set_dac_power(g_kcontrol, 1);

	sndp_extdev_set_state(SNDP_GET_MODE_VAL(work->new_value),
		SNDP_GET_AUDIO_DEVICE(work->new_value),
		SNDP_EXTDEV_START);

start_err:
	/* Wake Unlock */
	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue function for FM Radio stop

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_fm_radio_stop(struct sndp_work_info *work)
{
	int			iRet = ERROR_NONE;
	u_int			dev = SNDP_GET_DEVICE_VAL(work->new_value);

	struct snd_soc_codec *codec =
		(struct snd_soc_codec *)g_kcontrol->private_data;
	struct snd_soc_card *card = codec->card;

	sndp_log_debug_func("start\n");

	/* Disable the power domain */
	if (!((E_FM_PLAY | E_FM_CAP) & g_sndp_playrec_flg)) {
		/* stop SCUW */
		scuw_stop();

		if ((E_PLAY | E_CAP) & g_sndp_playrec_flg) {
			/* stop FSI */
			fsi_stop(0);
		} else {
			/* stop FSI */
			fsi_stop(1);
			/* stop CLKGEN */
			clkgen_stop();
		}

		if (SNDP_IS_FSI_MASTER_DEVICE(dev)) {
			if ((E_PLAY & E_CAP) & g_sndp_playrec_flg)
				/* FSI master */
				common_set_pll22(work->old_value,
						 STAT_OFF,
						 g_bluetooth_band_frequency);
		} else {
			sndp_log_err("FM CLKGEN master not supported\n");
		}

		sndp_extdev_set_state(SNDP_GET_MODE_VAL(work->old_value),
				     SNDP_GET_AUDIO_DEVICE(work->old_value),
				     SNDP_EXTDEV_STOP);
	}

	sndp_log_info("Put\n");
	/* Disable the power domain */
	iRet = sndp_pm_runtime_sync(E_PM_PUT, work->new_value,
					GET_OTHER_VALUE(work->old_value));

	/* Release standby restraint */
	iRet = fsi_d2153_disable_ignore_suspend(card, 0);
	if (ERROR_NONE != iRet)
		sndp_log_err("release ignore_suspend error(code=%d)\n", iRet);
	/* Wake Force Unlock */
	sndp_wake_lock(((E_FM_PLAY | E_FM_CAP) & g_sndp_playrec_flg)
						? E_UNLOCK : E_FORCE_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief wake up start clkgen callback function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void sndp_watch_start_clk_cb(void)
{
	sndp_log_debug_func("start\n");

	atomic_set(&g_sndp_watch_start_clk, 1);
	wake_up_interruptible(&g_watch_start_clk_queue);
	atomic_set(&g_sndp_watch_stop_clk, 0);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Work queue processing for VCD_COMMAND_WATCH_STOP_CLOCK process

   @param[in]	work	work queue structure
   @param[out]	none

   @retval	none
 */
static void sndp_work_watch_stop_clk(struct sndp_work_info *work)
{
	sndp_log_debug_func("start\n");

	if (!(E_CAP  & g_sndp_playrec_flg))
		/* Input device OFF */
		fsi_d2153_set_adc_power(g_kcontrol, 0);

	sndp_log_debug_func("end\n");
}


/*!
   @brief wake up stop clkgen callback function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void sndp_watch_stop_clk_cb(void)
{
	sndp_log_debug_func("start\n");
        sndp_log_err("sndp_watch_stop_clk_cb->Start");

	atomic_set(&g_sndp_watch_stop_clk, 1);
	wake_up_interruptible(&g_watch_stop_clk_queue);
	atomic_set(&g_sndp_watch_start_clk, 0);

	sndp_workqueue_enqueue(g_sndp_queue_main, &g_sndp_work_watch_stop_clk);

        sndp_log_err("sndp_watch_stop_clk_cb->End");

	sndp_log_debug_func("end\n");
}

/*!
   @brief Switching path of the sound during a Call + Playback

   @param[in]	uiValue		PCM value
   @param[out]	none

   @retval	none
 */
static void sndp_path_switching(const u_int uiValue)
{
	sndp_log_debug_func("start\n");
	sndp_log_info("uiValue[0x%08X]\n", uiValue);

	/* stop SCUW */
	scuw_stop();

	/* stop FSI */
	fsi_stop(1);

	/* stop CLKGEN */
	clkgen_stop();

	/* for Register dump debug */
	/* g_sndp_now_direction = SNDP_PCM_OUT; */

	/* Running Playback */
	g_sndp_playrec_flg |= E_PLAY;

	/* To register a work queue to start processing Playback */
	sndp_work_start(SNDP_PCM_OUT);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Back-out path of the sound during a Call + Playback

   @param[in]	uiValue		PCM value
   @param[out]	none

   @retval	none
 */
static void sndp_path_backout(const u_int uiValue)
{
	int			iRet = ERROR_NONE;
	struct sndp_stop	*stop;
	struct sndp_arg		*arg;

	sndp_log_debug_func("start\n");
	sndp_log_info("uiValue[0x%08X]\n", uiValue);

	arg = &g_sndp_main[SNDP_PCM_OUT].arg;
	/* FSI trigger stop process */
	fsi_set_trigger_stop(arg->fsi_substream, false);

	/* Init register dump log flag for debug */
	/* g_sndp_now_direction = SNDP_PCM_DIRECTION_MAX; */

	stop = &g_sndp_work_play_stop.stop;

	stop->fsi_substream = *arg->fsi_substream;
	if (NULL == &stop->fsi_substream)
		sndp_log_debug_func("#### stop->fsi_substream is NULL.\n");

	stop->fsi_dai =	*arg->fsi_dai;
	if (NULL == &stop->fsi_dai)
		sndp_log_debug_func("#### stop->fsi_dai is NULL.\n");

	/* Stop Playback runnning */
	g_sndp_playrec_flg &= ~E_PLAY;

	/* To register a work queue to stop processing Playback */
	sndp_work_stop(&g_sndp_work_play_stop, SNDP_PCM_OUT);

	/* start SCUW */
	iRet = scuw_start(uiValue, g_bluetooth_band_frequency);
	if (ERROR_NONE != iRet)
		sndp_log_err("scuw start error(code=%d)\n", iRet);

	/* start FSI */
	iRet = fsi_start(uiValue, 1);
	if (ERROR_NONE == iRet)
		/* all down link mute control */
		fsi_all_dl_mute_ctrl(g_dl_mute_flg);
	else
		sndp_log_err("fsi start error(code=%d)\n", iRet);

	/* start CLKGEN */
	iRet = clkgen_start(uiValue, 0, g_bluetooth_band_frequency);
	if (ERROR_NONE != iRet)
		sndp_log_err("clkgen start error(code=%d)\n", iRet);

	/* FSI FIFO reset */
	if (!(SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(uiValue)))
		fsi_fifo_reset(SNDP_PCM_PORTA);
	else
		fsi_fifo_reset(SNDP_PCM_PORTB);

	/* Set FSIIR_FSIF */
	scuw_set_fsiir();

	sndp_log_debug_func("end\n");
}


/*!
   @brief SoundPath Start and HardWare Parameter settings

   @param[in]	direction	SNDP_PCM_OUT/SNDP_PCM_IN
   @param[out]	none

   @retval	none
 */
static void sndp_work_start(const int direction)
{
	int	iRet = ERROR_NONE;
	u_int	uiValue;
	u_int	dev;

	sndp_log_debug_func("start\n");
	sndp_log_info("dir[%d]\n", direction);

	uiValue = GET_OLD_VALUE(direction);
	dev = SNDP_GET_DEVICE_VAL(uiValue);

	sndp_log_info("Get\n");
	/* Enable the power domain */
	iRet = sndp_pm_runtime_sync(E_PM_GET,
			SNDP_VALUE_INIT, SNDP_VALUE_INIT);

	if (!((E_PLAY & g_sndp_playrec_flg) &&
	      (E_CAP  & g_sndp_playrec_flg))) {
		if (0 == iRet || 1 == iRet) {
			if (!((E_FM_PLAY | E_FM_CAP) & g_sndp_playrec_flg))
				/* CPG soft reset */
				fsi_soft_reset();
		}
	}

	/* FSI slave setting ON for switch */
	if (SNDP_MODE_INCALL == SNDP_GET_MODE_VAL(uiValue)) {
#ifdef __SNDP_INCALL_CLKGEN_MASTER
		fsi_set_slave(true);
#else /* !__SNDP_INCALL_CLKGEN_MASTER */
		if (!(SNDP_BLUETOOTHSCO & SNDP_GET_DEVICE_VAL(uiValue)))
			fsi_set_slave(false);
		else
			fsi_set_slave(true);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */
	} else {
		/* FSI master */
		if (SNDP_IS_FSI_MASTER_DEVICE(dev)) {
			common_set_pll22(uiValue,
					 STAT_ON,
					 g_bluetooth_band_frequency);
		} else {
			fsi_set_slave(true);
			common_set_fsi2cr(dev, STAT_OFF);
		}
	}

	fsi_clk_start(g_sndp_main[direction].arg.fsi_substream);

	/* FSI startup */
	if (NULL != g_sndp_dai_func.fsi_startup) {
		sndp_log_debug("fsi_dai_startup\n");
		if (false == (dev & SNDP_BLUETOOTHSCO)) {
			iRet = g_sndp_dai_func.fsi_startup(
				g_sndp_main[direction].arg.fsi_substream,
				g_sndp_main[direction].arg.fsi_dai);
		} else {
			iRet = fsi_dai_startup_bt(
				g_sndp_main[direction].arg.fsi_substream,
				g_sndp_main[direction].arg.fsi_dai,
				g_bluetooth_band_frequency);
		}
		if (ERROR_NONE != iRet) {
			sndp_log_err("fsi_dai_startup error(code=%d)\n", iRet);
			return;
		}
	}

	/* Set FSI format */
	if (NULL != g_sndp_dai_func.fsi_set_fmt) {
		sndp_log_debug("fsi_dai_set_fmt\n");
		iRet = g_sndp_dai_func.fsi_set_fmt(
					g_sndp_main[direction].arg.fsi_dai,
					g_sndp_fsi_format);
		if (ERROR_NONE != iRet) {
			sndp_log_err("fsi_dai_set_fmt error(code=%d)\n", iRet);
			return;
		}
	}

	/* Set FSI hardware parameters */
	if (NULL != g_sndp_dai_func.fsi_hw_params) {
		sndp_log_debug("hw_params [params = %d]\n",
			params_rate(&g_sndp_main[direction].arg.fsi_params));
		g_sndp_dai_func.fsi_hw_params(
				g_sndp_main[direction].arg.fsi_substream,
				&g_sndp_main[direction].arg.fsi_params,
				g_sndp_main[direction].arg.fsi_dai);
	}

	/* start CLKGEN */
	if (SNDP_MODE_INCALL != SNDP_GET_MODE_VAL(uiValue)) {
		iRet = clkgen_start(uiValue,
				    SNDP_NORMAL_RATE,
				    g_bluetooth_band_frequency);
	} else {
		/* set mode INCALL -> NORMAL */
		uiValue &= 0xFFFFFFFC;
		iRet = clkgen_start(uiValue,
				    SNDP_CALL_RATE,
				    g_bluetooth_band_frequency);
	}
	if (ERROR_NONE != iRet) {
		sndp_log_err("clkgen start error(code=%d)\n", iRet);
		return;
	}

	/* Output device ON */
	if (SNDP_PCM_OUT == direction)
		fsi_d2153_set_dac_power(g_kcontrol, 1);
	/* Input device ON */
	if (SNDP_PCM_IN == direction)
		fsi_d2153_set_adc_power(g_kcontrol, 1);

	if (SNDP_PT_NOT_STARTED == g_pt_start) {
		if (SNDP_MODE_INCALL != SNDP_GET_MODE_VAL(uiValue))
			sndp_extdev_set_state(SNDP_GET_MODE_VAL(uiValue),
					SNDP_GET_AUDIO_DEVICE(uiValue),
					SNDP_EXTDEV_START);
	}

	/* FSI Trigger start */
	if (NULL != g_sndp_dai_func.fsi_trigger) {
		sndp_log_debug("fsi_dai_trigger start\n");
		iRet = g_sndp_dai_func.fsi_trigger(
				g_sndp_main[direction].arg.fsi_substream,
				SNDRV_PCM_TRIGGER_START,
				g_sndp_main[direction].arg.fsi_dai);
		if (ERROR_NONE != iRet)
			sndp_log_err("fsi_trigger error(code=%d)\n", iRet);
	}

	sndp_log_info("end\n");
}


/*!
   @brief SoundPath Stop

   @param[in]	work		work queue structure
   @param[in]	direction	SNDP_PCM_OUT/SNDP_PCM_IN
   @param[out]	none

   @retval	none
 */
static void sndp_work_stop(
	struct sndp_work_info *work,
	const int direction)
{
	u_int			ret = ERROR_NONE;
	u_int			uiValue;
	u_int			dev;

	sndp_log_debug_func("start\n");
	sndp_log_info("dir[%d]\n", direction);

	if (SNDP_PCM_IN == direction) {
		/* Input device OFF */
		fsi_d2153_set_adc_power(g_kcontrol, 0);
	}

	if (SNDP_PCM_OUT == direction) {
		/* Output device OFF */
		fsi_d2153_set_dac_power(g_kcontrol, 0);
	}
	uiValue = GET_OLD_VALUE(direction);
	dev = SNDP_GET_DEVICE_VAL(uiValue);

	/* FSI Trigger stop */
	if (NULL != g_sndp_dai_func.fsi_trigger) {
		sndp_log_debug("fsi_dai_trigger stop\n");
		g_sndp_dai_func.fsi_trigger(&(work->stop.fsi_substream),
					    SNDRV_PCM_TRIGGER_STOP,
					    &(work->stop.fsi_dai));
	}

	/* FSI DAI Shutdown */
	if (NULL != g_sndp_dai_func.fsi_shutdown) {
		sndp_log_debug("fsi_dai_shutdown\n");
		g_sndp_dai_func.fsi_shutdown(&(work->stop.fsi_substream),
					     &(work->stop.fsi_dai));
	}

	/* Disable the power domain */
	if (!((E_PLAY | E_CAP) & g_sndp_playrec_flg)) {
		/* Init register dump log flag for debug */
		g_sndp_now_direction = SNDP_PCM_DIRECTION_MAX;

		if (!((E_FM_PLAY | E_FM_CAP) & g_sndp_playrec_flg))
			/* stop CLKGEN */
			clkgen_stop();

		/* FSI slave setting ON for switch */
		if (SNDP_MODE_INCALL == SNDP_GET_MODE_VAL(uiValue)) {
			fsi_set_slave(false);
		} else {
			/* FSI master */
			if (SNDP_IS_FSI_MASTER_DEVICE(dev)) {
				if (!g_sndp_playrec_flg) {
					common_set_pll22(uiValue,
						STAT_OFF,
						g_bluetooth_band_frequency);
					fsi_soft_reset();
				}
			} else {
				/* FSI slave setting OFF */
				fsi_set_slave(false);
				common_set_fsi2cr(dev, STAT_ON);
			}
		}

		if ((SNDP_MODE_INCALL != SNDP_GET_MODE_VAL(uiValue)) &&
		    (SNDP_PT_NOT_STARTED == g_pt_start)) {
			sndp_extdev_set_state(SNDP_GET_MODE_VAL(uiValue),
					     SNDP_GET_AUDIO_DEVICE(uiValue),
					     SNDP_EXTDEV_STOP);
		}
	}

	sndp_log_info("Put\n");
	/* Disable the power domain */
	ret = sndp_pm_runtime_sync(E_PM_PUT, uiValue,
					GET_OTHER_VALUE(uiValue));

	fsi_clk_stop(&(work->stop.fsi_substream));

	/* Wake Unlock or Force Unlock */
	sndp_wake_lock(((E_PLAY | E_CAP) & g_sndp_playrec_flg)
						? E_UNLOCK : E_FORCE_UNLOCK);

	sndp_log_info("end\n");
}


/*!
   @brief SoundPath Start and HardWare Parameter settings

   @param[in]	direction	SNDP_PCM_OUT/SNDP_PCM_IN
   @param[out]	none

   @retval	none
 */
static void sndp_fm_work_start(const int direction)
{
	int	iRet = ERROR_NONE;

	sndp_log_debug_func("start\n");

	/* FSI Trigger in FM radio start */
	sndp_log_debug("fsi_dai_trigger_in_fm start\n");
	iRet = fsi_dai_trigger_in_fm(
			g_sndp_main[direction].arg.fsi_substream,
			SNDRV_PCM_TRIGGER_START,
			g_sndp_main[direction].arg.fsi_dai);
	if (ERROR_NONE != iRet)
		sndp_log_err("fsi_trigger_in_fm error(code=%d)\n", iRet);

	sndp_log_debug_func("end\n");
}


/*!
   @brief SoundPath Stop

   @param[in]	work		work queue structure
   @param[in]	direction	SNDP_PCM_OUT/SNDP_PCM_IN
   @param[out]	none

   @retval	none
 */
static void sndp_fm_work_stop(
	struct sndp_work_info *work,
	const int direction)
{
	u_int	uiValue;

	sndp_log_debug_func("start\n");

	uiValue = GET_OLD_VALUE(direction);

	/* FSI Trigger stop */
	sndp_log_debug("fsi_dai_trigger_in_fm stop\n");
	fsi_dai_trigger_in_fm(&(work->stop.fsi_substream),
				    SNDRV_PCM_TRIGGER_STOP,
				    &(work->stop.fsi_dai));

	/* Wake Unlock */
	sndp_wake_lock(E_UNLOCK);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Voice stop and Normal device change <br>
	  (Post-processing of this sndp_work_call_capture_stop())

   @param[in]	iInValue	PCM value for INPUT side
   @param[in]	iOutValue	PCM value for OUTPUT side
   @param[out]	none

   @retval	none
 */
static void sndp_after_of_work_call_capture_stop(
	const u_int iInValue,
	const u_int iOutValue)
{
	int	iRet = ERROR_NONE;

	sndp_log_debug_func("start\n");
	sndp_log_info("iInValue[0x%08X] iOutValue[0x%08X]\n",
							iInValue, iOutValue);
	/*
	 * A process similar to sndp_work_voice_stop()
	 */

	/* stop SCUW */
	scuw_stop();

	/* stop FSI */
	fsi_stop(1);

	/* stop CLKGEN */
	clkgen_stop();

	sndp_log_info("Put\n");
	/* Disable the power domain */
	iRet = sndp_pm_runtime_sync(E_PM_PUT, iInValue, iOutValue);

	/* Wake Force Unlock */
	sndp_wake_lock(E_FORCE_UNLOCK);

	sndp_log_debug_func("end\n");
}

/*!
   @brief audience Set Callback function

   @param[in]	Structure address for callback function
   @param[out]	none

   @retval	none
 */
void sndp_extdev_regist_callback(struct sndp_extdev_callback_func *func)
{
	sndp_log_debug_func("start\n");

	g_sndp_extdev_callback = func;
	sndp_log_info("callback address [%p]\n", g_sndp_extdev_callback);

	sndp_log_debug_func("end\n");
}
EXPORT_SYMBOL(sndp_extdev_regist_callback);

/*!
   @brief audience set_state

   @param[in]	mode	mode on PCM value
   @param[in]	device	device on PCM value
   @param[in]	dev_chg
   @param[out]	none

   @retval	none
 */
static void sndp_extdev_set_state(unsigned int mode, unsigned int device, unsigned int dev_chg)
{
	int			ret = ERROR_NONE;

	sndp_log_debug_func("start\n");

	if (!g_sndp_extdev_callback) {
		sndp_log_debug("struct address is NULL\n");
		return;
	}

	if (g_sndp_extdev_callback->set_state) {
		sndp_log_info("extdev m[%d] d[%d] c[%d]\n",
				mode, device, dev_chg);
		ret = g_sndp_extdev_callback->set_state(mode, device, dev_chg);
		if (ERROR_NONE != ret)
			sndp_log_err("set_state error [%d]\n", ret);
	} else {
		sndp_log_debug("set_state is NULL\n");
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief for Production Test Interface (Loopback)

   @param[in]  mode        Mode classification
   @param[in]  device      Device classification
   @param[in]  dev_chg     Audience status classification

   @retval 0       Successful
   @retval -ENODEV     Not prepared for Audience callback function
   @retval Other       Return value from Audience callback function
 */
int sndp_pt_loopback(u_int mode, u_int device, u_int dev_chg)
{
	int iRet = ERROR_NONE;


	sndp_log_debug_func("start\n");

	/* Change PT start status */
	if (SNDP_EXTDEV_START == dev_chg)
		g_pt_start = SNDP_PT_LOOPBACK_START;
	else if (SNDP_EXTDEV_STOP == dev_chg)
		g_pt_start = SNDP_PT_NOT_STARTED;

	if (SNDP_EXTDEV_START == dev_chg) {
#ifdef __SNDP_INCALL_CLKGEN_MASTER
		/* CLKGEN master setting */
		common_set_fsi2cr(SNDP_NO_DEVICE, STAT_OFF);
#else /* !__SNDP_INCALL_CLKGEN_MASTER */
		/* FSI master setting */
		common_set_pll22(SNDP_PLAYBACK_EARPIECE_INCALL,
				 STAT_ON,
				 g_bluetooth_band_frequency);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */

		/* start SCUW */
		iRet = scuw_start(SNDP_PLAYBACK_EARPIECE_INCALL,
				g_bluetooth_band_frequency);
		if (ERROR_NONE != iRet) {
			sndp_log_err("scuw start error(code=%d)\n", iRet);
			return iRet;
		}

		/* start FSI */
		iRet = fsi_start(SNDP_PLAYBACK_EARPIECE_INCALL, 1);
		if (ERROR_NONE != iRet) {
			sndp_log_err("fsi start error(code=%d)\n", iRet);
			return iRet;
		}

		/* start CLKGEN */
		iRet = clkgen_start(SNDP_PLAYBACK_EARPIECE_INCALL,
				0,
				g_bluetooth_band_frequency);
		if (ERROR_NONE != iRet) {
			sndp_log_err("clkgen start error(code=%d)\n", iRet);
			return iRet;
		}

		/* FSI FIFO reset */
		fsi_fifo_reset(SNDP_PCM_PORTA);

		/* Set FSIIR_FSIF */
		scuw_set_fsiir();

		/* Output device ON */
		fsi_d2153_set_dac_power(g_kcontrol, 1);

		/* Input device ON */
		fsi_d2153_set_adc_power(g_kcontrol, 1);

		sndp_log_info("call extdev set_state\n");
		sndp_extdev_set_state(mode, device, dev_chg);

	} else {

		sndp_log_info("call extdev set_state\n");
		sndp_extdev_set_state(mode, device, dev_chg);

		/* Output device OFF */
		fsi_d2153_set_dac_power(g_kcontrol, 0);

		/* Input device OFF */
		fsi_d2153_set_adc_power(g_kcontrol, 0);

		/* stop SCUW */
		scuw_stop();

		/* stop FSI */
		fsi_stop(1);

		/* stop CLKGEN */
		clkgen_stop();

#ifdef __SNDP_INCALL_CLKGEN_MASTER
		/* CLKGEN master process */
		common_set_fsi2cr(SNDP_NO_DEVICE, STAT_ON);
#else /* !__SNDP_INCALL_CLKGEN_MASTER */
		/* FSI master process */
		common_set_pll22(SNDP_PLAYBACK_EARPIECE_INCALL,
				STAT_OFF,
				g_bluetooth_band_frequency);
#endif /* __SNDP_INCALL_CLKGEN_MASTER */
	}

	g_sndp_now_direction = (SNDP_EXTDEV_START == dev_chg) ?
		SNDP_PCM_OUT : SNDP_PCM_DIRECTION_MAX;

	sndp_log_debug_func("end\n");

	return iRet;
}
EXPORT_SYMBOL(sndp_pt_loopback);


/*!
   @brief for Production Test Interface (Device change)

   @param[in]  dev         Device classification
   @param[in]  onoff       PT start/stop flag

   @retval 0           Successful
   @retval -ENODEV     Not prepared for Audience callback function
   @retval Other       Return value from Audience callback function
 */
int sndp_pt_device_change(u_int dev, u_int onoff)
{
	u_int new_state = SNDP_EXTDEV_NONE;

	sndp_log_debug_func("start\n");
	sndp_log_debug("dev=%d, onoff=%d\n", dev, onoff);

	/* set Audience state */
	if ((SNDP_PT_NOT_STARTED == g_pt_start) && (SNDP_ON == onoff))
		new_state = SNDP_EXTDEV_START;
	else if ((SNDP_PT_NOT_STARTED == g_pt_start) && (SNDP_OFF == onoff))
		new_state = SNDP_EXTDEV_STOP;
	else if ((SNDP_PT_DEVCHG_START == g_pt_start) && (SNDP_ON == onoff))
		new_state = SNDP_EXTDEV_CH_DEV;
	else if ((SNDP_PT_DEVCHG_START == g_pt_start) && (SNDP_OFF == onoff))
		new_state = SNDP_EXTDEV_STOP;

	sndp_extdev_set_state(SNDP_MODE_NORMAL, dev, new_state);
	g_pt_start = (SNDP_ON == onoff) ?
		SNDP_PT_DEVCHG_START : SNDP_PT_NOT_STARTED;

	g_sndp_now_direction = (SNDP_ON == onoff) ?
		SNDP_PCM_OUT : SNDP_PCM_DIRECTION_MAX;

	/* Output device ON */
	fsi_d2153_set_dac_power(g_kcontrol, 1);

	sndp_log_debug_func("end\n");

	return ERROR_NONE;
}
EXPORT_SYMBOL(sndp_pt_device_change);


#ifdef SOUND_TEST

#define SYSC_REG_MAX	(0x0084)

/* SYSC base address */
u_long g_sysc_Base;

/* Path test pm_runtime get function */
void sndp_path_test_pm_runtime_get_sync(void)
{
	u_int reg;
	int i;


/*	fsi_set_callback(sndp_fsi_interrupt); */
/*	pm_runtime_get_sync(g_sndp_power_domain); */

	/* Get SYSC Logical Address */
	g_sysc_Base = (u_long)ioremap_nocache(SYSC0_BASEPhys, SYSC_REG_MAX);
	if (0 >= g_sysc_Base) {
		printk(KERN_WARNING "%s SYSC ioremap failed\n", __func__);
		return;
	}

	/* PSTR */
	reg = ioread32(SYSC_PSTR);
	if (reg & (1 << 8)) {
		printk(KERN_WARNING "%s supplied to A4MP\n", __func__);
		return;
	}

	/* WakeUp */
	iowrite32((1 << 8), SYSC_SWUCR);
	for (i = 0; i < 500; i++) {
		reg = ioread32(SYSC_SWUCR);
		reg &= (1 << 8);
		if (!reg)
			break;
	}
	if (500 == i)
		printk(KERN_WARNING "%s Wake up error\n", __func__);

}


/* Path test pm_runtime put function */
void sndp_path_test_pm_runtime_put_sync(void)
{
/*	pm_runtime_put_sync(g_sndp_power_domain); */

	/* Release SYSC Logical Address */
	if (0 < g_sysc_Base) {
		iounmap((void *)g_sysc_Base);
		g_sysc_Base = 0;
	}
}


/* Path test sndp_init */
void sndp_path_test_sndp_init(void)
{
	int			iRet = -EFAULT;
	struct proc_dir_entry	*entry = NULL;
	struct proc_dir_entry	*reg_dump_entry = NULL;

	printk(KERN_WARNING"start\n");
#if 0
	/* Logical address base */
	g_fsi_Base = 0;
	g_scuw_Base = 0;
	g_clkgen_Base = 0;

	/* for Proc control */
	g_sndp_parent = proc_mkdir(SNDP_DRV_NAME, NULL);
	if (NULL != g_sndp_parent) {
		/* create file for log level entry */
		entry = create_proc_entry(
				LOG_LEVEL,
				S_IRUGO | S_IWUGO,
				g_sndp_parent);
		if (NULL != entry) {
			entry->read_proc  = sndp_proc_read;
			entry->write_proc = sndp_proc_write;
		} else {
			printk(KERN_WARNING"create_proc_entry(LOG) failed\n");
		}

		/* create file for register dump */
		reg_dump_entry = create_proc_entry(
						SNDP_REG_DUMP,
						S_IRUGO | S_IWUGO,
						g_sndp_parent);
		if (NULL != reg_dump_entry) {
			reg_dump_entry->read_proc = sndp_proc_reg_dump_read;
			reg_dump_entry->write_proc = sndp_proc_reg_dump_write;
		} else {
			printk(KERN_WARNING"create_proc_entry(REG) failed\n");
			remove_proc_entry(LOG_LEVEL, g_sndp_parent);
			remove_proc_entry(CALL_STATUS_SWITCH, g_sndp_parent);
		}

	} else {
		printk(KERN_WARNING "create failed for proc parrent\n");
	}

	/* for Log output */
	g_sndp_log_level = 0x14;

	/* ioremap */
	iRet = common_ioremap();
	if (ERROR_NONE != iRet)
		printk(KERN_WARNING "ioremap error\n");
#endif
}


static void sndp_fsi_interrupt(void)
{
/*	fsi_test_interrupt_a(); */
}

#endif /* SOUND_TEST */


 /**
 * @brief	queue work initialize function.
 *
 * @param[in]	work	queue work.
 * @param[in]	func	queue function.
 *
 * @retval	none.
 */
void sndp_work_initialize(
			struct sndp_work_info *work,
			void (*func)(struct sndp_work_info *),
			struct sndp_work_info *del_work)
{
	sndp_log_debug_func("work[%p]func[%p]\n", work, func);

	INIT_LIST_HEAD(&work->link);
	work->func = func;
	work->status = 0;
	work->del_work = del_work;

	sndp_log_debug_func("end\n");
	return;
}

/**
 * @brief	destroy workqueue function.
 *
 * @param[in]	wq	workqueue.
 *
 * @retval	none.
 */
void sndp_workqueue_destroy(struct sndp_workqueue *wq)
{
	unsigned long flags;

	sndp_log_debug_func("wq[%p]\n", wq);

	if (wq == NULL) {
		/* report error */
	} else {
		/* request task stop */
		if (wq->task)
			kthread_stop(wq->task);

		/* wakeup pending thread */
		spin_lock_irqsave(&wq->lock, flags);

		while (!list_empty(&wq->top)) {
			struct list_head *list;
			struct sndp_work_info *work = NULL;

			list_for_each(list, &wq->top)
			{
				work = list_entry(list,
					struct sndp_work_info, link);
				break;
			}
			if (work) {
				work->status = 1;
				list_del_init(&work->link);
			}
		}
		spin_unlock_irqrestore(&wq->lock, flags);

		wake_up_interruptible_all(&wq->wait);

		kfree(wq);
	}

	sndp_log_debug_func("end\n");
	return;
}


/**
 * @brief	workqueue thread function.
 *
 * @param[in]	arg	workqueue.
 *
 * @retval	VCD_ERR_NONE.
 */
inline int sndp_workqueue_thread(void *arg)
{
	struct sndp_workqueue *wq = (struct sndp_workqueue *)arg;
	unsigned long flags;

	sndp_log_debug_func("arg[%p]\n", arg);

	/* set schedule */
	sndp_set_schedule();

	/* dev->th_events already initialized 0. */
	while (!kthread_should_stop()) {
		struct sndp_work_info *work = NULL;
		void   (*func)(struct sndp_work_info *);

		wait_event_interruptible(wq->wait, !list_empty(&wq->top));

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&wq->lock, flags);
		while (!list_empty(&wq->top)) {
			work = list_first_entry(&wq->top,
				struct sndp_work_info, link);

			func = work->func;
			work_clear_pending(work);
			list_del_init(&work->link);
			spin_unlock_irqrestore(&wq->lock, flags);

			(*func)(work);

			spin_lock_irqsave(&wq->lock, flags);
			work->status = 1;
			wake_up_all(&wq->finish);
		}
		spin_unlock_irqrestore(&wq->lock, flags);
	}

	sndp_log_debug_func("end\n");
	return 0;
}


/**
 * @brief	create workqueue function.
 *
 * @param[in]	taskname	queue name.
 *
 * @retval	wq		workqueue.
 */
struct sndp_workqueue *sndp_workqueue_create(char *taskname)
{
	struct sndp_workqueue *wq;

	sndp_log_debug_func("start\n");

	wq = kmalloc(sizeof(*wq), GFP_KERNEL);

	if (wq == NULL) {
		sndp_log_err("kmalloc error. .\n");
	} else {
		memset(wq, 0, sizeof(*wq));

		INIT_LIST_HEAD(&wq->top);
		spin_lock_init(&wq->lock);
		init_waitqueue_head(&wq->wait);
		init_waitqueue_head(&wq->finish);

		wq->task = kthread_run(sndp_workqueue_thread,
				     wq,
				     taskname);
		if (IS_ERR(wq->task)) {
			kfree(wq);
			wq = NULL;
		}
	}

	sndp_log_debug_func("wq[%p]\n", wq);
	return wq;
}


/**
 * @brief	enqueue workqueue function.
 *
 * @param[in]	wq	workqueue.
 * @param[in]	work	queue work.
 *
 * @retval	none.
 */
void sndp_workqueue_enqueue(
	struct sndp_workqueue *wq, struct sndp_work_info *work)
{
	unsigned long flags;

/*	sndp_log_debug_func("wq[%p]work[%p]\n", wq, work); */

	if (wq && work) {
		spin_lock_irqsave(&wq->lock, flags);
		if (!test_and_set_bit(
			WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
			list_add_tail(&work->link, &wq->top);
		} else if (NULL != work->del_work) {
			work_clear_pending(work->del_work);
			list_del_init(&work->del_work->link);
			sndp_log_err("Cancel work.\n");
		} else {
			sndp_log_err("Already pending work.\n");
		}
		spin_unlock_irqrestore(&wq->lock, flags);
		wake_up_interruptible(&wq->wait);
	} else {
		sndp_log_err("parameter error. wq[%p]work[%p].\n", wq, work);
	}

/*	sndp_log_debug_func("end\n"); */
	return;
}

/**
 * @brief	set scheduler function.
 *
 * @param	none.
 *
 * @retval	none.
 */
static void sndp_set_schedule(void)
{
	struct sched_param param = { .sched_priority = 75 };

	sndp_log_debug_func("start\n");

	sched_setscheduler(current, SCHED_FIFO, &param);

	sndp_log_debug_func("end\n");
	return;
}
