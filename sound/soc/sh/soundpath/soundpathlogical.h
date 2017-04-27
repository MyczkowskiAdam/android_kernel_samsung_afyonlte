/* soundpathlogical.h
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

#ifndef __SOUNDPATHLOGICAL_H__
#define __SOUNDPATHLOGICAL_H__

#include <linux/kernel.h>
#include <linux/vcd/vcd.h>

#include <mach/common.h>

/*
 *
 * EXTERN Declarations
 *
 */
/* Trigger stop parameter setting for FSI */
extern void fsi_clk_start(struct snd_pcm_substream *substream);
extern void fsi_clk_stop(struct snd_pcm_substream *substream);

extern void fsi_set_trigger_stop(
	struct snd_pcm_substream *substream,
	bool flag);
/* FSI PM RUNTIME control */
extern void fsi_set_run_time(
	void *sndp_fsi_suspend,
	void *sndp_fsi_resume);

extern void fsi_set_slave(const bool slave);

extern int fsi_dai_startup_bt(
		struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai,
		u_int rate);
extern int fsi_dai_trigger_in_fm(
		struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai);

/*
 *
 * PROTOTYPE Declarations
 *
 */

/* Proc read for sndp (DEBUG LOG) */
static int sndp_proc_read(
	char *page,
	char **start,
	off_t offset,
	int count,
	int *eof,
	void *data);
/* Proc write for sndp (DEBUG LOG) */
static int sndp_proc_write(
	struct file *filp,
	const char __user *buffer,
	unsigned long count,
	void *data);
/* Proc read for Register dump (DEBUG LOG) */
static int sndp_proc_reg_dump_read(
	char *page,
	char **start,
	off_t offset,
	int count,
	int *eof,
	void *data);
/* Proc write for Register dump (DEBUG LOG) */
static int sndp_proc_reg_dump_write(
	struct file *filp,
	const char __user *buffer,
	unsigned long count,
	void *data);

#if 0
/* SOC INFO */
static int sndp_soc_info(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo);
/* SOC GET */
static int sndp_soc_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

/* SOC functions */
static int sndp_soc_capture_volume(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
static int sndp_soc_get_capture_mute(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
static int sndp_soc_put_capture_mute(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
#endif

/* FSI control functions */
static int sndp_fsi_startup(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai);
static void sndp_fsi_shutdown(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai);
static int sndp_fsi_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai);
static int sndp_fsi_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai);
static int sndp_fsi_set_fmt(struct snd_soc_dai *dai, u_int fmt);
static snd_pcm_uframes_t sndp_fsi_pointer(
	struct snd_pcm_substream *substream);
static int sndp_fsi_suspend(struct device *dev);
static int sndp_fsi_resume(struct device *dev);
static int sndp_fsi_hw_free(struct snd_pcm_substream *substream);

/* PATH Switchging / Back-out control functions */
static void sndp_path_switching(const u_int uiValue);
static void sndp_path_backout(const u_int uiValue);

/* Trigger process for Call */
static void sndp_call_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai,
	u_int value);

/* Trigger process for VoIP */
static void sndp_incomm_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai,
	u_int value);

/* Trigger process for fm */
static void sndp_fm_trigger(
	struct snd_pcm_substream *substream,
	int cmd,
	struct snd_soc_dai *dai,
	u_int value);

#if 0
/* Next set device type, to identify */
static u_long sndp_get_next_devices(const u_int uiValue);
#endif

/* Work queue processing for call start */
static void sndp_work_voice_start(struct sndp_work_info *work);
/* Work queue processing for call stop */
static void sndp_work_voice_stop(struct sndp_work_info *work);
/* Work queue processing for voice call device change */
static void sndp_work_voice_dev_chg(struct sndp_work_info *work);
/* Work queue processing for fm device change */
static void sndp_work_fm_radio_dev_chg(struct sndp_work_info *work);

/* Work queue processing for Playback start */
static void sndp_work_play_start(struct sndp_work_info *work);
/* Work queue processing for Capture start */
static void sndp_work_capture_start(struct sndp_work_info *work);
/* Work queue processing for Playback stop */
static void sndp_work_play_stop(struct sndp_work_info *work);
/* Work queue processing for Capture stop */
static void sndp_work_capture_stop(struct sndp_work_info *work);

/* Work queue processing for Start during a call playback */
static void sndp_work_call_playback_start(struct sndp_work_info *work);
/* Work queue processing for Start during a call capture */
static void sndp_work_call_capture_start(struct sndp_work_info *work);
/* Work queue processing for Stop during a call playback */
static void sndp_work_call_playback_stop(struct sndp_work_info *work);
/* Work queue processing for Stop during a call capture */
static void sndp_work_call_capture_stop(struct sndp_work_info *work);

/* Work queue processing for Start during a playback incommunication */
static void sndp_work_call_playback_incomm_start(struct sndp_work_info *work);
/* Work queue function for Start during a capture incommunication */
static void sndp_work_call_capture_incomm_start(struct sndp_work_info *work);
/* Work queue function for Stop during a playback incommunication */
static void sndp_work_call_playback_incomm_stop(struct sndp_work_info *work);
/* Work queue function for Stop during a capture incommunication */
static void sndp_work_call_capture_incomm_stop(struct sndp_work_info *work);


/* Work queue processing for Start during a fm playback */
static void sndp_work_fm_playback_start(struct sndp_work_info *work);
/* Work queue processing for Start during a fm capture */
static void sndp_work_fm_capture_start(struct sndp_work_info *work);
/* Work queue processing for Stop during a fm playback */
static void sndp_work_fm_playback_stop(struct sndp_work_info *work);
/* Work queue processing for Stop during a fm capture */
static void sndp_work_fm_capture_stop(struct sndp_work_info *work);

/* VCD_COMMAND_WATCH_STOP_FW registration process */
static void sndp_regist_watch(void);
/* Work queue processing for VCD_COMMAND_WATCH_STOP_FW process */
static void sndp_work_watch_stop_fw(struct sndp_work_info *work);
/* Work queue processing for VCD_COMMAND_WATCH_STOP_CLOCK process */
static void sndp_work_watch_stop_clk(struct sndp_work_info *work);

/* Work queue processing for FM Radio start */
static void sndp_work_fm_radio_start(struct sndp_work_info *work);
/* Work queue processing for FM Radio stop */
static void sndp_work_fm_radio_stop(struct sndp_work_info *work);

/* Work queue processing for playback incommunication start */
static void sndp_work_play_incomm_start(struct sndp_work_info *work);
/* Work queue processing for playback incommunication stop */
static void sndp_work_play_incomm_stop(struct sndp_work_info *work);
/* Work queue processing for capture incommunication start */
static void sndp_work_capture_incomm_start(struct sndp_work_info *work);
/* Work queue processing for capture incommunication stop */
static void sndp_work_capture_incomm_stop(struct sndp_work_info *work);
/* Work queue processing for all down link mute control */
static void sndp_work_all_dl_mute(struct sndp_work_info *work);

/* SoundPath start / stop control functions */
/* Normal */
static void sndp_work_start(const int direction);
static void sndp_work_stop(
	struct sndp_work_info *work,
	const int direction);
/* Incommunication */
static void sndp_work_incomm_start(const u_int new_value,
					const u_int save_status);
static void sndp_work_incomm_stop(
		const u_int old_value, const u_int new_value);
/* SoundPath start / stop control functions */
static void sndp_fm_work_start(const int direction);
static void sndp_fm_work_stop(
	struct sndp_work_info *work,
	const int direction);

/* Voice stop and Normal device change */
/* (Post-processing of this sndp_work_call_capture_stop()) */
static void sndp_after_of_work_call_capture_stop(
	const u_int iInValue,
	const u_int iOutValue);

/* Watch start Firmware notification callback function */
static void sndp_watch_start_fw_cb(void);

/* Watch stop Firmware notification callback function */
static void sndp_watch_stop_fw_cb(void);

/* Watch codec type(WB/NB) callback function */
static void sndp_codec_type_cb(u_int codec_type);

/* Wake up start clkgen callback function */
static void sndp_watch_start_clk_cb(void);

/* Wake up stop clkgen callback function */
static void sndp_watch_stop_clk_cb(void);

/* Wake up wait path callback function */
static void sndp_wait_path_cb(void);

/* Subfunction of the sndp_work_voice_dev_chg() */
/* AudioLSI -> BT-SCO */
static int sndp_work_voice_dev_chg_audioic_to_bt(
	const u_int old_value,
	const u_int new_value);
/* BT-SCO -> AudioLSI */
static int sndp_work_voice_dev_chg_bt_to_audioic(
	const u_int old_value,
	const u_int new_value);
/* AudioLSI ->AudioLSI */
static int sndp_work_voice_dev_chg_in_audioic(
	const u_int old_value,
	const u_int new_value);

/* Work queue function for hw free */
static void sndp_work_hw_free(struct sndp_work_info *work);

/* Work queue function for shutdown */
static void sndp_work_shutdown(struct sndp_work_info *work);

/*
 *
 * MACRO Declarations
 *
 */

/* SOC Interface */
#define SNDPDRV_SOC_ENUM_EXT(xname, xenum)			\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,			\
	.name = xname,						\
	.info = sndp_soc_info,					\
	.get = sndp_soc_get,					\
	.put = sndp_soc_put,					\
	.private_value = (unsigned long)&xenum			\
}

#define SNDPDRV_SOC_SINGLE(xname, reg, shift, max, invert, xget, xput)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,				\
	.name = xname,							\
	.info = snd_soc_info_volsw,					\
	.get = xget,							\
	.put = xput,							\
	.private_value = SOC_SINGLE_VALUE(reg, shift, max, invert)	\
}

/* for informs of data receiving */
#define LOG_INIT_CYCLE_COUNT(stream)	(g_sndp_log_cycle_counter[stream] = 0)
#define LOG_GET_CYCLE_COUNT_MAX()	(g_sndp_log_level >> 16)

/* SoundDriver Status SET/GET */
#define SET_SNDP_STATUS(stream, set_status)	\
	g_sndp_main[stream].status = set_status
#define GET_SNDP_STATUS(stream)		(g_sndp_main[stream].status)

/* Old value (PCM) SET/GET */
#define SET_OLD_VALUE(stream, value)	(g_sndp_main[stream].old_value = value)
#define GET_OLD_VALUE(stream)		(g_sndp_main[stream].old_value)
#define GET_OTHER_VALUE(value)						\
	(GET_OLD_VALUE((SNDP_PCM_OUT ==					\
		SNDP_GET_DIRECTION_VAL(value)) ? SNDP_PCM_IN : SNDP_PCM_OUT))

/*
 *
 * DEFINE Definitions
 *
 */
#define SNDPDRV_VOICE_VOL_MAX	(25)	/* Volume MAX value */

#define SNDP_WAIT_MAX	(3000)		/* TRIGGER STOP Wait time max */

/* Wait time max for wait queue for start voice process wake up */
#define SNDP_WATCH_CLK_TIME_OUT		(1000)

#define SNDP_BOARD_REV_OUTSIDE_RANGE_5	(0x05)

#define IS_DIALOG_BOARD_REV(r)        (SNDP_BOARD_REV_OUTSIDE_RANGE_5 <= r)

/*
 *
 * STRUCTURE Definitions
 *
 */

/* Process ID, when mode change by soc_put. */
enum sndp_proc_e {
	SNDP_PROC_NO			= 0x0000, /* No action               */
	SNDP_PROC_SHUTDOWN		= 0x0001, /* HW Shutdown(FSI)        */
	SNDP_PROC_START			= 0x0002, /* Soundpath setting       */
	SNDP_PROC_CALL_START		= 0x0004, /* Call on                 */
	SNDP_PROC_CALL_STOP		= 0x0008, /* Disconnect              */
	SNDP_PROC_WATCH_STOP_FW		= 0x0010, /* Stop watch VCD          */
	SNDP_PROC_DEV_CHANGE		= 0x0020, /* Device change for call  */
	SNDP_PROC_INCOMM_START		= 0x0040, /* incommunication start   */
	SNDP_PROC_INCOMM_STOP		= 0x0080, /* incommunication stop    */
};

/* Status */
enum sndp_stat_e {
	SNDP_STAT_NOT_CHG = 0,		/* 0: No transition           */
	SNDP_STAT_NORMAL,		/* 1: NORMAL                  */
	SNDP_STAT_RINGTONE,		/* 2: RINGTONE                */
	SNDP_STAT_IN_CALL,		/* 3: IN_CALL                 */
	SNDP_STAT_IN_CALL_PLAY,		/* 4: Recording during a call */
	SNDP_STAT_IN_CALL_CAP,		/* 5: Capturing during a call */
	SNDP_STAT_IN_COMM,		/* 6: INCOMMUNICATION         */
};

/* Power status */
enum sndp_power_status {
	SNDP_POWER_INIT = 0,	/* 0: Initial */
	SNDP_POWER_RESUME,	/* 1: Resume  */
	SNDP_POWER_SUSPEND,	/* 2: Suspend */
};

/* Running state of the Playback or Capture */
enum sndp_play_rec_state {
	E_IDLE		= 0x0,	/* Idle                           */
	E_PLAY		= 0x1,	/* Running Playback process       */
	E_CAP		= 0x2,	/* Running Capture process        */
	E_FM_PLAY	= 0x4,	/* Running Playback process of FM */
	E_FM_CAP	= 0x8,	/* Running Capture process of FM  */
};

/* Wake lock kind */
enum sndp_wake_lock_kind {
	E_LOCK = 0,			/* to Wake Lock   */
	E_UNLOCK,			/* to Wake Unlock */
	E_FORCE_UNLOCK,			/* to Wake Unlock Forced */
};

/* PM runtime kind */
enum sndp_pm_runtime_kind {
	E_PM_GET = 0,			/* call pm_runtime_get_sync */
	E_PM_PUT,			/* call pm_runtime_put_sync */
};

/* Function pointer typedef declarations */
typedef int (*sndp_dai_startup)(struct snd_pcm_substream *,
				struct snd_soc_dai *);
typedef void (*sndp_dai_shutdown_fsi)(struct snd_pcm_substream *,
				      struct snd_soc_dai *);
typedef int (*sndp_dai_trigger)(struct snd_pcm_substream *,
				int,
				struct snd_soc_dai *);
typedef int (*sndp_dai_hw_params)(struct snd_pcm_substream *,
				  struct snd_pcm_hw_params *,
				  struct snd_soc_dai *);
typedef snd_pcm_uframes_t (*sndp_pointer)(struct snd_pcm_substream *substream);
typedef int (*sndp_dai_set_fmt)(struct snd_soc_dai *, u_int);
typedef int (*sndp_hw_free)(struct snd_pcm_substream *);

/* Function table */
struct sndp_dai_func {
	sndp_dai_startup	fsi_startup;
	sndp_dai_shutdown_fsi	fsi_shutdown;
	sndp_dai_trigger	fsi_trigger;
	sndp_dai_set_fmt	fsi_set_fmt;
	sndp_dai_hw_params	fsi_hw_params;
	sndp_pointer		fsi_pointer;
	sndp_hw_free		fsi_hw_free;
};

/* ARGs table */
struct sndp_arg {
	struct snd_pcm_substream	*fsi_substream;
	struct snd_soc_dai		*fsi_dai;
	struct snd_pcm_hw_params	fsi_params;
};

/* Main processing table */
struct sndp_main {
	struct sndp_arg		arg;		/* ARGs               */
	u_int			status;		/* SoundDriver status */
	u_int			old_value;	/* Old value (PCM)    */
};

/* Mode transition table */
struct sndp_mode_trans {
	u_int next_proc;		/* Implementation processing  */
	u_int next_status;		/* After the transition state */
};

static void sndp_wake_lock(const enum sndp_wake_lock_kind kind);
static int sndp_pm_runtime_sync(const enum sndp_pm_runtime_kind kind,
				const u_int new_value,
				const u_int other_value);

/* Wait queue for start voice process wake up */
static DECLARE_WAIT_QUEUE_HEAD(g_watch_start_clk_queue);
static atomic_t g_sndp_watch_start_clk;

/* Wait queue for stop voice process wake up */
static DECLARE_WAIT_QUEUE_HEAD(g_watch_stop_clk_queue);
static atomic_t g_sndp_watch_stop_clk;

/* audience Callback */
static void sndp_extdev_set_state(
		unsigned int mode, unsigned int device, unsigned int dev_chg);


/* SOUND_TEST */
#ifdef SOUND_TEST
extern void fsi_set_callback(callback_function func);
static void sndp_fsi_interrupt(void);
#endif
/* SOUND_TEST */

/* set scheduler function */
static void sndp_set_schedule(void);

#endif /* __SOUNDPATHLOGICAL_H__ */


