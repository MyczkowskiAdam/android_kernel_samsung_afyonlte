/* soundpath.h
 *
 * Copyright (C) 2011-2013 Renesas Mobile Corp.
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


#ifndef __SOUNDPATH_H__
#define __SOUNDPATH_H__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/wait.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>


/* #define  __SNDP_NORMAL_PLAY_CLKGEN_MASTER */
/* #define  __SNDP_INCALL_CLKGEN_MASTER      */

/* PCM direction kind */
enum sndp_direction {
	SNDP_PCM_OUT,			/* out(playback) */
	SNDP_PCM_IN,			/* in(capture)   */
	SNDP_PCM_DIRECTION_MAX,
};

/* Port kind */
enum sndp_port_kind {
	SNDP_PCM_PORTA,
	SNDP_PCM_PORTB,
};

/* Work queue processing table */
struct sndp_workqueue {
	struct list_head top;
	spinlock_t lock;
	wait_queue_head_t wait;
	wait_queue_head_t finish;
	struct task_struct *task;
};

struct sndp_stop {
	struct snd_pcm_substream	fsi_substream;
	struct snd_soc_dai		fsi_dai;
};

struct sndp_work_info {
	struct work_struct		work;		 /* Work             */
	struct snd_pcm_substream	*save_substream; /* Substream        */
	struct snd_soc_dai		*save_dai;	 /* Dai              */
	struct sndp_stop		stop;		 /* for Stop process */
	u_int				new_value;	 /* PCM value (NEW)  */
	u_int				old_value;	 /* PCM value (OLD)  */
	u_int				save_status;
	struct list_head		link;
	void				(*func)(struct sndp_work_info *work);
	int				status;
	atomic_long_t			data;
	struct sndp_work_info		*del_work;
};

#ifdef __SOUNDPATHLOGICAL_NO_EXTERN__
#define SOUNDPATH_NO_EXTERN
#else
#define SOUNDPATH_NO_EXTERN		extern
#endif

/**
 ** ************************************************** *
 ***  For Audience interface                          **
 ** ************************************************** *
 */
/* use Audience information */
struct sndp_extdev_callback_func {
	int (*set_state)(unsigned int mode, unsigned int device, unsigned int dev_chg);
	int (*set_nb_wb) (unsigned int nb_wb);
};

/* change device */
enum sndp_extdev_ch_type {
	SNDP_EXTDEV_NONE,
	SNDP_EXTDEV_START,
	SNDP_EXTDEV_STOP,
	SNDP_EXTDEV_CH_DEV,
};

/* PT status */
enum sndp_pt_status_type {
	SNDP_PT_NOT_STARTED,	/* PT not started */
	SNDP_PT_LOOPBACK_START,	/* PT Loopback test start */
	SNDP_PT_DEVCHG_START,	/* PT device change test start */
};

SOUNDPATH_NO_EXTERN int sndp_init(
	struct snd_soc_dai_driver *fsi_port_dai_driver,
	struct snd_soc_platform_driver *fsi_soc_platform,
	struct snd_soc_card *fsi_soc_card);

SOUNDPATH_NO_EXTERN int sndp_soc_put(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

SOUNDPATH_NO_EXTERN int sndp_soc_get_voice_out_volume(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
SOUNDPATH_NO_EXTERN int sndp_soc_put_voice_out_volume(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
SOUNDPATH_NO_EXTERN int sndp_soc_get_playback_mute(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);
SOUNDPATH_NO_EXTERN int sndp_soc_put_playback_mute(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol);

SOUNDPATH_NO_EXTERN void sndp_exit(void);
SOUNDPATH_NO_EXTERN void sndp_work_portb_start(struct work_struct *work);

SOUNDPATH_NO_EXTERN void sndp_set_portb_start(bool flag);

SOUNDPATH_NO_EXTERN void sndp_call_playback_normal(void);

SOUNDPATH_NO_EXTERN struct sndp_workqueue	*g_sndp_queue_main;

SOUNDPATH_NO_EXTERN struct wake_lock g_sndp_wake_lock_idle;
SOUNDPATH_NO_EXTERN struct wake_lock g_sndp_wake_lock_suspend;

SOUNDPATH_NO_EXTERN u_int g_sndp_log_level;
SOUNDPATH_NO_EXTERN u_int g_sndp_mode;

/* for Production test Variable */
SOUNDPATH_NO_EXTERN int g_pt_start;	/* Production test start flag */
SOUNDPATH_NO_EXTERN u_int g_pt_device;	/* Production test device */

SOUNDPATH_NO_EXTERN struct semaphore g_sndp_wait_free[SNDP_PCM_DIRECTION_MAX];

/* audience Set Callback function */
SOUNDPATH_NO_EXTERN void sndp_extdev_regist_callback(
				struct sndp_extdev_callback_func *func);

/* for Production Test (Loopback test) */
SOUNDPATH_NO_EXTERN int sndp_pt_loopback(
				u_int mode, u_int device, u_int dev_chg);

/* for Production Test (Device change) */
SOUNDPATH_NO_EXTERN int sndp_pt_device_change(u_int dev, u_int onoff);

/* queue work initialize function */
SOUNDPATH_NO_EXTERN void sndp_work_initialize(
					struct sndp_work_info *work,
					void (*func)(struct sndp_work_info *),
					struct sndp_work_info *del_work);
/* destroy workqueue function */
SOUNDPATH_NO_EXTERN void sndp_workqueue_destroy(struct sndp_workqueue *wq);
/* create workqueue function */
SOUNDPATH_NO_EXTERN struct sndp_workqueue *sndp_workqueue_create(char *taskname);
/* enqueue workqueue function */
SOUNDPATH_NO_EXTERN void sndp_workqueue_enqueue(
	struct sndp_workqueue *wq, struct sndp_work_info *work);

/* start : define for debug */
#ifndef DEBUG
#define DEBUG
#endif

#define __PRN_SNDP__
/* #define __SNDP_ROUTE_DEBUG__ */

/* end : define for debug */

#define SNDP_DRV_NAME		"sndp"
#define LOG_LEVEL		"log_level"
#define CALL_STATUS_SWITCH	"call_status"
#define SNDP_REG_DUMP		"reg_dump"

#define LOG_NO_PRINT		(0x00)
#define LOG_ERR_PRINT		(0x01)
#define LOG_PROC_PRINT		(0x02)
#define LOG_DEBUG_PRINT		(0x03)
#define LOG_FUNC_PRINT		(0x04)

#define LOG_BIT_REG_DUMP	(0x10)
#define LOG_BIT_TIME_DEL	(0x20)
#define LOG_BIT_DMESG		(0x80)

#define LOG_LEVEL_MAX		(0xffffffff)
#define LOG_BYTE_LOW(sw)	((sw) & 0x0000000f)

#define GET_PROCESS_TIME(tv)	do_gettimeofday(&tv)

#define REG_DUMP_ALL		(0x00)
#define REG_DUMP_MAXIM		(0x01)
#define REG_DUMP_FSI		(0x02)
#define REG_DUMP_CLKGEN		(0x04)
#define REG_DUMP_SCUW		(0x08)

/* #define SOUND_TEST */
#define NO_INTURRUPT
#ifdef SOUND_TEST
SOUNDPATHLOGICAL_NO_EXTERN	void sndp_path_test_pm_runtime_get_sync(void);
SOUNDPATHLOGICAL_NO_EXTERN	void sndp_path_test_pm_runtime_put_sync(void);
SOUNDPATHLOGICAL_NO_EXTERN	void sndp_path_test_sndp_init(void);
#endif /* SOUND_TEST */

#ifdef __PRN_SNDP__

#define sndp_log_reg_dump(fmt, ...)					\
do {									\
	if (g_sndp_log_level & LOG_BIT_REG_DUMP)			\
		(g_sndp_log_level & LOG_BIT_DMESG) ?			\
			pr_err(fmt, ##__VA_ARGS__) :			\
			pr_alert(fmt, ##__VA_ARGS__);			\
} while (0)

#define sndp_log_ver(fmt, ...)						\
do {									\
	if (LOG_ERR_PRINT <= LOG_BYTE_LOW(g_sndp_log_level)) {		\
		if (g_sndp_log_level & LOG_BIT_TIME_DEL) {		\
			(g_sndp_log_level & LOG_BIT_DMESG) ?		\
				pr_err(SNDP_DRV_NAME " : Ver " fmt,	\
					##__VA_ARGS__) :		\
				pr_alert(SNDP_DRV_NAME " : Ver " fmt,	\
					##__VA_ARGS__);			\
		} else {						\
			struct timeval tv;				\
			GET_PROCESS_TIME(tv);				\
			tv.tv_usec /= USEC_PER_MSEC;			\
			(g_sndp_log_level & LOG_BIT_DMESG) ?		\
				pr_err("[%5ld.%03ld] " SNDP_DRV_NAME	\
					" : Ver " fmt, tv.tv_sec,	\
					tv.tv_usec, ##__VA_ARGS__) :	\
				pr_alert("[%5ld.%03ld] " SNDP_DRV_NAME	\
					" : Ver " fmt, tv.tv_sec,	\
					tv.tv_usec, ##__VA_ARGS__);	\
		}							\
	}								\
} while (0)

#define sndp_log_err(fmt, ...)						\
do {									\
	if (LOG_ERR_PRINT <= LOG_BYTE_LOW(g_sndp_log_level)) {		\
		if (g_sndp_log_level & LOG_BIT_TIME_DEL) {		\
			(g_sndp_log_level & LOG_BIT_DMESG) ?		\
				pr_err(SNDP_DRV_NAME " : %s(): "	\
					fmt, __func__, ##__VA_ARGS__) :	\
				pr_alert(SNDP_DRV_NAME " : %s(): "	\
					fmt, __func__, ##__VA_ARGS__);	\
		} else {						\
			struct timeval tv;				\
			GET_PROCESS_TIME(tv);				\
			tv.tv_usec /= USEC_PER_MSEC;			\
			(g_sndp_log_level & LOG_BIT_DMESG) ?		\
				pr_err("[%5ld.%03ld] " SNDP_DRV_NAME	\
					" : %s(): " fmt, tv.tv_sec,	\
					tv.tv_usec, __func__,		\
					##__VA_ARGS__) :		\
				pr_alert("[%5ld.%03ld] " SNDP_DRV_NAME	\
					" : %s(): " fmt, tv.tv_sec,	\
					tv.tv_usec, __func__,		\
					##__VA_ARGS__);			\
		}							\
	}								\
} while (0)

#define sndp_log_info(fmt, ...)						\
do {									\
	if (LOG_PROC_PRINT <= LOG_BYTE_LOW(g_sndp_log_level)) {		\
		if (g_sndp_log_level & LOG_BIT_TIME_DEL) {		\
			(g_sndp_log_level & LOG_BIT_DMESG) ?		\
				pr_err(SNDP_DRV_NAME " : %s(): "	\
					fmt, __func__, ##__VA_ARGS__) :	\
				pr_alert(SNDP_DRV_NAME " : %s(): "	\
					fmt, __func__, ##__VA_ARGS__);	\
		} else {						\
			struct timeval tv;				\
			GET_PROCESS_TIME(tv);				\
			tv.tv_usec /= USEC_PER_MSEC;			\
			(g_sndp_log_level & LOG_BIT_DMESG) ?		\
				pr_err("[%5ld.%03ld] " SNDP_DRV_NAME	\
					" : %s(): " fmt, tv.tv_sec,	\
					tv.tv_usec, __func__,		\
					##__VA_ARGS__) :		\
				pr_alert("[%5ld.%03ld] " SNDP_DRV_NAME	\
					" : %s(): " fmt, tv.tv_sec,	\
					tv.tv_usec, __func__,		\
					##__VA_ARGS__);			\
		}							\
	}								\
} while (0)

#define sndp_log_debug(fmt, ...)					\
do {									\
	if (LOG_DEBUG_PRINT <= LOG_BYTE_LOW(g_sndp_log_level)) {	\
		if (g_sndp_log_level & LOG_BIT_TIME_DEL) {		\
			(g_sndp_log_level & LOG_BIT_DMESG) ?		\
				pr_err(SNDP_DRV_NAME " : %s(): "	\
					fmt, __func__, ##__VA_ARGS__) :	\
				pr_alert(SNDP_DRV_NAME " : %s(): "	\
					fmt, __func__, ##__VA_ARGS__);	\
		} else {						\
			struct timeval tv;				\
			GET_PROCESS_TIME(tv);				\
			tv.tv_usec /= USEC_PER_MSEC;			\
			(g_sndp_log_level & LOG_BIT_DMESG) ?		\
				pr_err("[%5ld.%03ld] " SNDP_DRV_NAME	\
					" : %s(): " fmt, tv.tv_sec,	\
					tv.tv_usec, __func__,		\
					##__VA_ARGS__) :		\
				pr_alert("[%5ld.%03ld] " SNDP_DRV_NAME	\
					" : %s(): " fmt, tv.tv_sec,	\
					tv.tv_usec, __func__,		\
					##__VA_ARGS__);			\
		}							\
	}								\
} while (0)

#define sndp_log_debug_func(fmt, ...)					\
do {									\
	if (LOG_FUNC_PRINT <= LOG_BYTE_LOW(g_sndp_log_level)) {		\
		if (g_sndp_log_level & LOG_BIT_TIME_DEL) {		\
			(g_sndp_log_level & LOG_BIT_DMESG) ?		\
				pr_err(SNDP_DRV_NAME " : %s(): "	\
					fmt, __func__, ##__VA_ARGS__) :	\
				pr_alert(SNDP_DRV_NAME " : %s(): "	\
					fmt, __func__, ##__VA_ARGS__);	\
		} else {						\
			struct timeval tv;				\
			GET_PROCESS_TIME(tv);				\
			tv.tv_usec /= USEC_PER_MSEC;			\
			(g_sndp_log_level & LOG_BIT_DMESG) ?		\
				pr_err("[%5ld.%03ld] " SNDP_DRV_NAME	\
					" : %s(): " fmt, tv.tv_sec,	\
					tv.tv_usec, __func__,		\
					##__VA_ARGS__) :		\
				pr_alert("[%5ld.%03ld] " SNDP_DRV_NAME	\
					" : %s(): " fmt, tv.tv_sec,	\
					tv.tv_usec, __func__,		\
					##__VA_ARGS__);			\
		}							\
	}								\
} while (0)

#else /* != __PRN_SNDP__ */

#define sndp_log_ver(fmt, ...)			do { } while (0)
#define sndp_log_err(fmt, ...)			do { } while (0)
#define sndp_log_info(fmt, ...)			do { } while (0)
#define sndp_log_debug(fmt, ...)		do { } while (0)
#define sndp_log_debug_func(fmt, ...)		do { } while (0)
#define sndp_log_reg_dump(fmt, ...)		do { } while (0)
#define sndp_log_dump(fmt, ...)			do { } while (0)

#endif	/* __PRN_SNDP__ */

#define sndp_log_always_err(fmt, ...)					\
do {									\
	struct timeval tv;						\
	GET_PROCESS_TIME(tv);						\
	pr_alert("[%5ld.%06ld] " SNDP_DRV_NAME " : %s(): " fmt,		\
			tv.tv_sec, tv.tv_usec, __func__, ##__VA_ARGS__);\
} while (0)

/*
 * Constant definitions
 */
/* On/Off */
#define ERROR_NONE	(0)

#define SNDP_ON		(1)
#define SNDP_OFF	(0)

/* Start/Stop */
#define SNDP_START	(1)
#define SNDP_STOP	(0)

#define STAT_OFF	(0)
#define STAT_ON		(1)

/* Sampling Rate */
#define SNDP_NORMAL_RATE	(48000)
#define SNDP_CALL_RATE		(16000)

/*!
  @brief	D2153 <==> WM1811 change board revision
*/
#define D2153_INTRODUCE_BOARD_REV		5

enum sndp_hw_audio {
	SNDP_HW_CLKGEN,
	SNDP_HW_FSI,
	SNDP_HW_SCUW,
};


enum sndp_incall_kind {
	SNDP_RC5T7316_INCALL_3G,
	SNDP_RC5T7316_INCALL_2G,
	SNDP_RC5T7316_INCALL_MAX,
};


/**
 ** ************************************************** *
 ***  Struct declaration                              **
 ** ************************************************** *
 */
/* Function table */
struct ctrl_func_tbl {
	u_int		uiValue;
	void		(*func)(const u_int);
};

/* Routing type of the stream, in during a call */
enum sndp_stream_route_type {
	/* Normal route */
	SNDP_ROUTE_NORMAL = 0,
	/* Playback path, switched to the FSI */
	SNDP_ROUTE_PLAY_CHANGED = 0x01,
	/* Started the dummy playing */
	SNDP_ROUTE_PLAY_DUMMY = 0x02,
	/* Started the dummy recording */
	SNDP_ROUTE_CAP_DUMMY = 0x04,
	/* Started the voip dummy playing */
	SNDP_ROUTE_PLAY_INCOMM_DUMMY = 0x08,
	/* Started the voip dummy recording */
	SNDP_ROUTE_CAP_INCOMM_DUMMY = 0x10,
};

/* Device type */
enum sndp_device_type {
	SNDP_NO_DEVICE = 0,
	SNDP_SPEAKER = 0x1,		/* SPEAKER */
	SNDP_WIREDHEADSET = 0x2,	/* WIRED_HEADSET */
	SNDP_EARPIECE = 0x4,		/* EARPIECE */
	SNDP_BLUETOOTHSCO = 0x8,	/* BLUETOOTH_SCO */
	SNDP_AUXDIGITAL = 0x10,		/* AUX_DIGITAL(HDMI) */
	SNDP_BUILTIN_MIC = 0x20,	/* MIC */
	SNDP_WIREDHEADPHONE = 0x40,	/* WIRED_HEADPHONE */
	SNDP_FM_RADIO_RX = 0x80,	/* FM_RADIO_RX */
	SNDP_FM_RADIO_TX = 0x100,	/* FM_RADIO_TX */
};

/* Mode types */
enum sndp_mode_type {
	SNDP_MODE_NORMAL,		/* MODE_NORMAL */
	SNDP_MODE_RING,			/* MODE_RINGTONE */
	SNDP_MODE_INCALL,		/* MODE_IN_CALL */
	SNDP_MODE_INCOMM,		/* MODE_IN_COMMUNICATION */
	SNDP_MODE_INIT,			/* init */
	SNDP_MODE_MAX,
};

#define SNDP_DIRECTION_BIT		(20)
#define SNDP_DEVICE_BIT			(4)
#define SNDP_MODE_BIT			(0)
#define SNDP_VALUE_INIT			(0xff000000 + SNDP_MODE_INIT)

enum sndp_audio_devices {
	/* output devices */
	SNDP_OUT_EARPIECE = 0x1,
	SNDP_OUT_SPEAKER = 0x2,
	SNDP_OUT_WIRED_HEADSET = 0x4,
	SNDP_OUT_WIRED_HEADPHONE = 0x8,
	SNDP_OUT_BLUETOOTH_SCO = 0x10,
	SNDP_OUT_BLUETOOTH_SCO_HEADSET = 0x20,
	SNDP_OUT_BLUETOOTH_SCO_CARKIT = 0x40,
	SNDP_OUT_BLUETOOTH_A2DP = 0x80,
	SNDP_OUT_BLUETOOTH_A2DP_HEADPHONES = 0x100,
	SNDP_OUT_BLUETOOTH_A2DP_SPEAKER = 0x200,
	SNDP_OUT_AUX_DIGITAL = 0x400,
	SNDP_OUT_ANLG_DOCK_HEADSET = 0x800,
	SNDP_OUT_DGTL_DOCK_HEADSET = 0x1000,
	SNDP_OUT_FM_RADIO_TX = 0x2000,
	SNDP_OUT_FM_RADIO_RX = 0x4000,
	/* input devices */
	SNDP_IN_COMMUNICATION = 0x10000,
	SNDP_IN_AMBIENT = 0x20000,
	SNDP_IN_BUILTIN_MIC = 0x40000,
	SNDP_IN_BLUETOOTH_SCO_HEADSET = 0x80000,
	SNDP_IN_WIRED_HEADSET = 0x100000,
	SNDP_IN_AUX_DIGITAL = 0x200000,
	SNDP_IN_VOICE_CALL = 0x400000,
	SNDP_IN_BACK_MIC = 0x800000,
	SNDP_IN_USB_HEADSET = 0x1000000,
	SNDP_IN_FM_RADIO_RX = 0x2000000,
};

#define SNDP_GET_DIRECTION_VAL(val)				\
	((u_int)(val) >= ((u_int)1 << SNDP_DIRECTION_BIT))

#define SNDP_GET_AUDIO_DEVICE(val)				\
	((u_int)(val) >> SNDP_DEVICE_BIT)

#define SNDP_BLUETOOTH_FREQUENCY_NARROW_BAND	(0x00000000)
#define SNDP_BLUETOOTH_FREQUENCY_WIDE_BAND	(0x00008000)
#define SNDP_BLUETOOTH_FREQUENCY_MASK		(0xffff7fff)

#define SNDP_GET_BLUETOOTH_FREQUENCY(val)			\
	((u_int)(val) & SNDP_BLUETOOTH_FREQUENCY_WIDE_BAND)

static inline u_int SNDP_GET_DEVICE_VAL(u_int val)
{
	u_int	audio_val = SNDP_GET_AUDIO_DEVICE(val);
	u_int	ret = SNDP_NO_DEVICE;

	if (audio_val & SNDP_OUT_SPEAKER)
		ret |= SNDP_SPEAKER;

	if (audio_val & SNDP_OUT_EARPIECE)
		ret |= SNDP_EARPIECE;

	if ((audio_val & SNDP_OUT_WIRED_HEADSET) ||
	    (audio_val & SNDP_IN_WIRED_HEADSET))
		ret |= SNDP_WIREDHEADSET;

	if ((audio_val & SNDP_OUT_BLUETOOTH_SCO)	 ||
	    (audio_val & SNDP_OUT_BLUETOOTH_SCO_HEADSET) ||
	    (audio_val & SNDP_OUT_BLUETOOTH_SCO_CARKIT)  ||
	    (audio_val & SNDP_IN_BLUETOOTH_SCO_HEADSET))
		ret |= SNDP_BLUETOOTHSCO;

	if ((audio_val & SNDP_OUT_AUX_DIGITAL) ||
	    (audio_val & SNDP_IN_AUX_DIGITAL))
		ret |= SNDP_AUXDIGITAL;

	if (audio_val & SNDP_IN_BUILTIN_MIC)
		ret |= SNDP_BUILTIN_MIC;

	if (audio_val & SNDP_OUT_WIRED_HEADPHONE)
		ret |= SNDP_WIREDHEADPHONE;

	if ((audio_val & SNDP_OUT_FM_RADIO_RX) ||
	    (audio_val & SNDP_IN_FM_RADIO_RX))
		ret |= SNDP_FM_RADIO_RX;

	if (audio_val & SNDP_OUT_FM_RADIO_TX)
		ret |= SNDP_FM_RADIO_TX;

	return ret;
}

#define SNDP_GET_MODE_VAL(val)					\
	(((u_int)(val) & ~((0xffffffff >> SNDP_DEVICE_BIT)	\
				<< SNDP_DEVICE_BIT)) >> SNDP_MODE_BIT)

#define SNDP_GET_VALUE(dev, mod)				\
	(((u_int)(dev) << SNDP_DEVICE_BIT) | ((u_int)(mod) << SNDP_MODE_BIT))

#ifdef __SNDP_NORMAL_PLAY_CLKGEN_MASTER
#define SNDP_IS_FSI_MASTER_DEVICE(device)	\
	((device & SNDP_BLUETOOTHSCO) ||	\
	 (device & SNDP_FM_RADIO_TX)  ||	\
	 (device & SNDP_FM_RADIO_RX)  ||	\
	 (device & SNDP_AUXDIGITAL))
#else /* !__SNDP_NORMAL_PLAY_CLKGEN_MASTER */
#define SNDP_IS_FSI_MASTER_DEVICE(device)	\
	((device & SNDP_SPEAKER)	||	\
	 (device & SNDP_WIREDHEADSET)	||	\
	 (device & SNDP_WIREDHEADPHONE)	||	\
	 (device & SNDP_EARPIECE)	||	\
	 (device & SNDP_BLUETOOTHSCO)	||	\
	 (device & SNDP_AUXDIGITAL)	||	\
	 (device & SNDP_FM_RADIO_TX)	||	\
	 (device & SNDP_FM_RADIO_RX)	||	\
	 (device & SNDP_BUILTIN_MIC))
#endif /* __SNDP_NORMAL_PLAY_CLKGEN_MASTER */


/* 0x00000010 */
#define SNDP_PLAYBACK_EARPIECE_NORMAL			\
	SNDP_GET_VALUE(SNDP_OUT_EARPIECE, SNDP_MODE_NORMAL)
/* 0x00000011 */
#define SNDP_PLAYBACK_EARPIECE_RINGTONE			\
	SNDP_GET_VALUE(SNDP_OUT_EARPIECE, SNDP_MODE_RING)
/* 0x00000012 */
#define SNDP_PLAYBACK_EARPIECE_INCALL			\
	SNDP_GET_VALUE(SNDP_OUT_EARPIECE, SNDP_MODE_INCALL)
/* 0x00000013 */
#define SNDP_PLAYBACK_EARPIECE_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_OUT_EARPIECE, SNDP_MODE_INCOMM)
/* 0x00000020 */
#define SNDP_PLAYBACK_SPEAKER_NORMAL			\
	SNDP_GET_VALUE(SNDP_OUT_SPEAKER, SNDP_MODE_NORMAL)
/* 0x00000021 */
#define SNDP_PLAYBACK_SPEAKER_RINGTONE			\
	SNDP_GET_VALUE(SNDP_OUT_SPEAKER, SNDP_MODE_RING)
/* 0x00000022 */
#define SNDP_PLAYBACK_SPEAKER_INCALL			\
	SNDP_GET_VALUE(SNDP_OUT_SPEAKER, SNDP_MODE_INCALL)
/* 0x00000023 */
#define SNDP_PLAYBACK_SPEAKER_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_OUT_SPEAKER, SNDP_MODE_INCOMM)
/* 0x00000040 */
#define SNDP_PLAYBACK_HEADSET_NORMAL			\
	SNDP_GET_VALUE(SNDP_OUT_WIRED_HEADSET, SNDP_MODE_NORMAL)
/* 0x00000041 */
#define SNDP_PLAYBACK_HEADSET_RINGTONE			\
	SNDP_GET_VALUE(SNDP_OUT_WIRED_HEADSET, SNDP_MODE_RING)
/* 0x00000042 */
#define SNDP_PLAYBACK_HEADSET_INCALL			\
	SNDP_GET_VALUE(SNDP_OUT_WIRED_HEADSET, SNDP_MODE_INCALL)
/* 0x00000043 */
#define SNDP_PLAYBACK_HEADSET_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_OUT_WIRED_HEADSET, SNDP_MODE_INCOMM)
/* 0x00000080 */
#define SNDP_PLAYBACK_HEADPHONE_NORMAL			\
	SNDP_GET_VALUE(SNDP_OUT_WIRED_HEADPHONE, SNDP_MODE_NORMAL)
/* 0x00000081 */
#define SNDP_PLAYBACK_HEADPHONE_RINGTONE		\
	SNDP_GET_VALUE(SNDP_OUT_WIRED_HEADPHONE, SNDP_MODE_RING)
/* 0x00000082 */
#define SNDP_PLAYBACK_HEADPHONE_INCALL			\
	SNDP_GET_VALUE(SNDP_OUT_WIRED_HEADPHONE, SNDP_MODE_INCALL)
/* 0x00000083 */
#define SNDP_PLAYBACK_HEADPHONE_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_OUT_WIRED_HEADPHONE, SNDP_MODE_INCOMM)
/* 0x00000060 */
#define SNDP_PLAYBACK_SPEAKER_HEADSET_NORMAL		\
	SNDP_GET_VALUE((SNDP_OUT_SPEAKER | SNDP_OUT_WIRED_HEADSET), SNDP_MODE_NORMAL)
/* 0x00000061 */
#define SNDP_PLAYBACK_SPEAKER_HEADSET_RINGTONE		\
	SNDP_GET_VALUE((SNDP_OUT_SPEAKER | SNDP_OUT_WIRED_HEADSET), SNDP_MODE_RING)
/* 0x00000062 */
#define SNDP_PLAYBACK_SPEAKER_HEADSET_INCALL		\
	SNDP_GET_VALUE((SNDP_OUT_SPEAKER | SNDP_OUT_WIRED_HEADSET), SNDP_MODE_INCALL)
/* 0x00000063 */
#define SNDP_PLAYBACK_SPEAKER_HEADSET_INCOMMUNICATION	\
	SNDP_GET_VALUE((SNDP_OUT_SPEAKER | SNDP_OUT_WIRED_HEADSET), SNDP_MODE_INCOMM)
/* 0x000000a0 */
#define SNDP_PLAYBACK_SPEAKER_HEADPHONE_NORMAL		\
	SNDP_GET_VALUE((SNDP_OUT_SPEAKER | SNDP_OUT_WIRED_HEADPHONE), SNDP_MODE_NORMAL)
/* 0x000000a1 */
#define SNDP_PLAYBACK_SPEAKER_HEADPHONE_RINGTONE	\
	SNDP_GET_VALUE((SNDP_OUT_SPEAKER | SNDP_OUT_WIRED_HEADPHONE), SNDP_MODE_RING)
/* 0x000000a2 */
#define SNDP_PLAYBACK_SPEAKER_HEADPHONE_INCALL		\
	SNDP_GET_VALUE((SNDP_OUT_SPEAKER | SNDP_OUT_WIRED_HEADPHONE), SNDP_MODE_INCALL)
/* 0x000000a3 */
#define SNDP_PLAYBACK_SPEAKER_HEADPHONE_INCOMMUNICATION	\
	SNDP_GET_VALUE((SNDP_OUT_SPEAKER | SNDP_OUT_WIRED_HEADPHONE), SNDP_MODE_INCOMM)
/* 0x00000100 */
#define SNDP_PLAYBACK_BLUETOOTH_NORMAL			\
	SNDP_GET_VALUE(SNDP_OUT_BLUETOOTH_SCO, SNDP_MODE_NORMAL)
/* 0x00000101 */
#define SNDP_PLAYBACK_BLUETOOTH_RINGTONE		\
	SNDP_GET_VALUE(SNDP_OUT_BLUETOOTH_SCO, SNDP_MODE_RING)
/* 0x00000102 */
#define SNDP_PLAYBACK_BLUETOOTH_INCALL			\
	SNDP_GET_VALUE(SNDP_OUT_BLUETOOTH_SCO, SNDP_MODE_INCALL)
/* 0x00000103 */
#define SNDP_PLAYBACK_BLUETOOTH_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_OUT_BLUETOOTH_SCO, SNDP_MODE_INCOMM)
/* 0x00004000 */
#define SNDP_PLAYBACK_AUXDIGITAL_NORMAL			\
	SNDP_GET_VALUE(SNDP_OUT_AUX_DIGITAL, SNDP_MODE_NORMAL)
/* 0x00004001 */
#define SNDP_PLAYBACK_AUXDIGITAL_RINGTONE		\
	SNDP_GET_VALUE(SNDP_OUT_AUX_DIGITAL, SNDP_MODE_RING)
/* 0x00004002 */
#define SNDP_PLAYBACK_AUXDIGITAL_INCALL			\
	SNDP_GET_VALUE(SNDP_OUT_AUX_DIGITAL, SNDP_MODE_INCALL)
/* 0x00004003 */
#define SNDP_PLAYBACK_AUXDIGITAL_INCOMMUNICATION	\
	SNDP_GET_VALUE(SNDP_OUT_AUX_DIGITAL, SNDP_MODE_INCOMM)
/* 0x00040000 */
#define SNDP_PLAYBACK_FMTX_NORMAL			\
	SNDP_GET_VALUE(SNDP_OUT_FM_RADIO_TX, SNDP_MODE_NORMAL)
/* 0x00040001 */
#define SNDP_PLAYBACK_FMTX_RINGTONE			\
	SNDP_GET_VALUE(SNDP_OUT_FM_RADIO_TX, SNDP_MODE_RING)
/* 0x00040002 */
#define SNDP_PLAYBACK_FMTX_INCALL			\
	SNDP_GET_VALUE(SNDP_OUT_FM_RADIO_TX, SNDP_MODE_INCALL)
/* 0x00040003 */
#define SNDP_PLAYBACK_FMTX_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_OUT_FM_RADIO_TX, SNDP_MODE_INCOMM)
/* 0x00020020 */
#define SNDP_PLAYBACK_FMRX_SPEAKER_NORMAL		\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_SPEAKER), SNDP_MODE_NORMAL)
/* 0x00020021 */
#define SNDP_PLAYBACK_FMRX_SPEAKER_RINGTONE		\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_SPEAKER), SNDP_MODE_RING)
/* 0x00020022 */
#define SNDP_PLAYBACK_FMRX_SPEAKER_INCALL		\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_SPEAKER), SNDP_MODE_INCALL)
/* 0x00020023 */
#define SNDP_PLAYBACK_FMRX_SPEAKER_INCOMMUNICATION	\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_SPEAKER), SNDP_MODE_INCOMM)
/* 0x00020040 */
#define SNDP_PLAYBACK_FMRX_HEADSET_NORMAL		\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_WIRED_HEADSET), SNDP_MODE_NORMAL)
/* 0x00020041 */
#define SNDP_PLAYBACK_FMRX_HEADSET_RINGTONE		\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_WIRED_HEADSET), SNDP_MODE_RING)
/* 0x00020042 */
#define SNDP_PLAYBACK_FMRX_HEADSET_INCALL		\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_WIRED_HEADSET), SNDP_MODE_INCALL)
/* 0x00020043 */
#define SNDP_PLAYBACK_FMRX_HEADSET_INCOMMUNICATION	\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_WIRED_HEADSET), SNDP_MODE_INCOMM)
/* 0x00020080 */
#define SNDP_PLAYBACK_FMRX_HEADPHONE_NORMAL		\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_WIRED_HEADPHONE), SNDP_MODE_NORMAL)
/* 0x00020081 */
#define SNDP_PLAYBACK_FMRX_HEADPHONE_RINGTONE		\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_WIRED_HEADPHONE), SNDP_MODE_RING)
/* 0x00020082 */
#define SNDP_PLAYBACK_FMRX_HEADPHONE_INCALL		\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_WIRED_HEADPHONE), SNDP_MODE_INCALL)
/* 0x00020083 */
#define SNDP_PLAYBACK_FMRX_HEADPHONE_INCOMMUNICATION	\
	SNDP_GET_VALUE((SNDP_OUT_FM_RADIO_RX | SNDP_OUT_WIRED_HEADPHONE), SNDP_MODE_INCOMM)
/* 0x01000000 */
#define SNDP_CAPTURE_HEADSET_NORMAL			\
	SNDP_GET_VALUE(SNDP_IN_WIRED_HEADSET, SNDP_MODE_NORMAL)
/* 0x01000001 */
#define SNDP_CAPTURE_HEADSET_RINGTONE			\
	SNDP_GET_VALUE(SNDP_IN_WIRED_HEADSET, SNDP_MODE_RING)
/* 0x01000002 */
#define SNDP_CAPTURE_HEADSET_INCALL			\
	SNDP_GET_VALUE(SNDP_IN_WIRED_HEADSET, SNDP_MODE_INCALL)
/* 0x01000003 */
#define SNDP_CAPTURE_HEADSET_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_IN_WIRED_HEADSET, SNDP_MODE_INCOMM)
/* 0x00800000 */
#define SNDP_CAPTURE_BLUETOOTH_NORMAL			\
	SNDP_GET_VALUE(SNDP_IN_BLUETOOTH_SCO_HEADSET, SNDP_MODE_NORMAL)
/* 0x00800001 */
#define SNDP_CAPTURE_BLUETOOTH_RINGTONE			\
	SNDP_GET_VALUE(SNDP_IN_BLUETOOTH_SCO_HEADSET, SNDP_MODE_RING)
/* 0x00800002 */
#define SNDP_CAPTURE_BLUETOOTH_INCALL			\
	SNDP_GET_VALUE(SNDP_IN_BLUETOOTH_SCO_HEADSET, SNDP_MODE_INCALL)
/* 0x00800003 */
#define SNDP_CAPTURE_BLUETOOTH_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_IN_BLUETOOTH_SCO_HEADSET, SNDP_MODE_INCOMM)
/* 0x00400000 */
#define SNDP_CAPTURE_MIC_NORMAL				\
	SNDP_GET_VALUE(SNDP_IN_BUILTIN_MIC, SNDP_MODE_NORMAL)
/* 0x00400001 */
#define SNDP_CAPTURE_MIC_RINGTONE			\
	SNDP_GET_VALUE(SNDP_IN_BUILTIN_MIC, SNDP_MODE_RING)
/* 0x00400002 */
#define SNDP_CAPTURE_MIC_INCALL				\
	SNDP_GET_VALUE(SNDP_IN_BUILTIN_MIC, SNDP_MODE_INCALL)
/* 0x00400003 */
#define SNDP_CAPTURE_MIC_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_IN_BUILTIN_MIC, SNDP_MODE_INCOMM)
/* 0x04000000 */
#define SNDP_CAPTURE_VOICE_NORMAL			\
	SNDP_GET_VALUE(SNDP_IN_VOICE_CALL, SNDP_MODE_NORMAL)
/* 0x04000001 */
#define SNDP_CAPTURE_VOICE_RINGTONE			\
	SNDP_GET_VALUE(SNDP_IN_VOICE_CALL, SNDP_MODE_RING)
/* 0x04000002 */
#define SNDP_CAPTURE_VOICE_INCALL			\
	SNDP_GET_VALUE(SNDP_IN_VOICE_CALL, SNDP_MODE_INCALL)
/* 0x04000003 */
#define SNDP_CAPTURE_VOICE_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_IN_VOICE_CALL, SNDP_MODE_INCOMM)
/* 0x20000000 */
#define SNDP_CAPTURE_FMRX_NORMAL			\
	SNDP_GET_VALUE(SNDP_IN_FM_RADIO_RX, SNDP_MODE_NORMAL)
/* 0x20000001 */
#define SNDP_CAPTURE_FMRX_RINGTONE			\
	SNDP_GET_VALUE(SNDP_IN_FM_RADIO_RX, SNDP_MODE_RING)
/* 0x20000002 */
#define SNDP_CAPTURE_FMRX_INCALL			\
	SNDP_GET_VALUE(SNDP_IN_FM_RADIO_RX, SNDP_MODE_INCALL)
/* 0x20000003 */
#define SNDP_CAPTURE_FMRX_INCOMMUNICATION		\
	SNDP_GET_VALUE(SNDP_IN_FM_RADIO_RX, SNDP_MODE_INCOMM)


/**
 ** ************************************************** *
 ***  For AudioLSI interface                          **
 ** ************************************************** *
 */
/* use AudioLsi information */
struct sndp_codec_info {
	int (*set_device)(const u_long device, const u_int pcm_value,
		u_int power);
	int (*get_device)(u_long *device);
	int (*set_volum)(const u_long device, const u_int volume);
	int (*get_volume)(const u_long device, u_int *volume);
	int (*set_mute)(const u_int mute);
	int (*get_mute)(u_int *mute);
	int (*set_speaker_amp)(const u_int value);
	void (*set_soc_controls)(struct snd_kcontrol_new *controls,
				u_int array_size);

	u_long out_dev_all;
	u_long in_dev_all;
	u_long dev_none;
	u_long dev_playback_speaker;
	u_long dev_playback_earpiece;
	u_long dev_playback_headphones;
	u_long dev_capture_mic;
	u_long dev_capture_headset_mic;
	u_int codec_valume;
	u_int mute_enable;
	u_int mute_disable;
	u_int speaker_enable;
	u_int speaker_disable;

	u_int power_on;
	u_int power_off;
};

/* PCM name suffix table */
struct sndp_pcm_name_suffix {
	const u_int key;
	const char *suffix;
};

#define SNDP_PCM_NAME_MAX_LEN	(128)
#define SNDP_OUT_PCM_SUFFIX	"AndroidPlayback"
#define SNDP_IN_PCM_SUFFIX	"AndroidCapture"

/* Device name */
static const struct sndp_pcm_name_suffix device_suffix[] = {
	{ SNDP_OUT_EARPIECE,			"_Earpiece"		},
	{ SNDP_OUT_SPEAKER,			"_Speaker"		},
	{ SNDP_OUT_WIRED_HEADSET,		"_Headset"		},
	{ SNDP_OUT_WIRED_HEADPHONE,		"_Headphone"		},
	{ SNDP_OUT_BLUETOOTH_SCO,		"_Bluetooth"		},
	{ SNDP_OUT_BLUETOOTH_SCO_HEADSET,	"_Bluetooth"		},
	{ SNDP_OUT_AUX_DIGITAL,			"_Hdmi"			},
	{ SNDP_OUT_FM_RADIO_TX,			"_Fmtx"			},
	{ SNDP_OUT_FM_RADIO_RX,			"_Fmrx"			},
	{ SNDP_IN_WIRED_HEADSET,		"_Headset"		},
	{ SNDP_IN_BUILTIN_MIC,			"_Mic"			},
	{ SNDP_IN_BLUETOOTH_SCO_HEADSET,	"_Bluetooth"		},
	{ SNDP_IN_VOICE_CALL,			"_Voicecall"		},
	{ SNDP_IN_FM_RADIO_RX,			"_Fmrx"			},
};

/* Mode name */
static const struct sndp_pcm_name_suffix mode_suffix[] = {
	{ SNDP_MODE_NORMAL,			"_normal"		},
	{ SNDP_MODE_RING,			"_ringtone"		},
	{ SNDP_MODE_INCALL,			"_incall"		},
	{ SNDP_MODE_INCOMM,			"_incommunication"	},
};


/*!
   @brief PCM name generator

   @param[in]	uiValue		PCM value
   @param[out]	cName		PCM name

   @retval	none
 */

static inline void sndp_pcm_name_generate(const u_int uiValue, char *cName)
{
	int	iCnt;
	u_int	uiDevice;
	u_int	uiMode;


	/* Get mode type */
	uiMode = SNDP_GET_MODE_VAL(uiValue);

	/* Get device type */
	uiDevice = SNDP_GET_AUDIO_DEVICE(uiValue);

	/* Direction type */
	strcpy(cName,
	       (SNDP_GET_DIRECTION_VAL(uiValue)) ?
		SNDP_IN_PCM_SUFFIX : SNDP_OUT_PCM_SUFFIX);

	/* Device name */
	for (iCnt = 0; ARRAY_SIZE(device_suffix) > iCnt; iCnt++) {
		if (device_suffix[iCnt].key & uiDevice)
			strcat(cName, device_suffix[iCnt].suffix);
	}

	/* Mode name */
	for (iCnt = 0; ARRAY_SIZE(mode_suffix) > iCnt; iCnt++) {
		if (mode_suffix[iCnt].key == uiMode) {
			strcat(cName, mode_suffix[iCnt].suffix);
			break;
		}
	}
}

#endif /* __SOUNDPATH_H__ */

