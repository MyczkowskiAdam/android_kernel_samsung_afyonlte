/* call_extern.h
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


#ifndef __CALL_EXTERN_H__
#define __CALL_EXTERN_H__

#include <linux/kernel.h>
#include <sound/soundpath/soundpath.h>

#ifdef __CALL_CTRL_NO_EXTERN__
#define CALL_CTRL_NO_EXTERN
#else
#define CALL_CTRL_NO_EXTERN	extern
#endif

/* Vocoder status */
enum call_status {
	IDLE_STATUS		= 0x00,
	CALL_STATUS		= 0x01,
	PLAY_STATUS		= 0x02,
	REC_STATUS		= 0x04,
	PLAY_INCOMM_STATUS	= 0x08,
	REC_INCOMM_STATUS	= 0x10
};

CALL_CTRL_NO_EXTERN enum call_status g_status;
CALL_CTRL_NO_EXTERN atomic_t g_call_watch_start_fw;
CALL_CTRL_NO_EXTERN atomic_t g_call_watch_stop_fw;

typedef void (*callback_func)(void);
typedef void (*callback_func_arg1)(u_int);

struct call_vcd_callback {
	callback_func		callback_start_fw;
	callback_func		callback_stop_fw;
	callback_func	 	callback_start_clk;
	callback_func		callback_stop_clk;
	callback_func	 	callback_wait_path;
	callback_func_arg1	callback_codec_type;
};

CALL_CTRL_NO_EXTERN bool g_call_incomm_cb[SNDP_PCM_DIRECTION_MAX];
CALL_CTRL_NO_EXTERN u_int g_call_sampling_rate[SNDP_PCM_DIRECTION_MAX];

/* Call initialization function */
CALL_CTRL_NO_EXTERN void call_init(
	callback_func rat_cb,
	callback_func control_cb);
/* Speech Start function */
CALL_CTRL_NO_EXTERN void call_speech_start(void);
/* Speech Stop function */
CALL_CTRL_NO_EXTERN void call_speech_stop(void);
/* Speech Force Stop function */
CALL_CTRL_NO_EXTERN void call_force_stop(void);
/* Speech + Playback Start function */
CALL_CTRL_NO_EXTERN int call_playback_start(
	struct snd_pcm_substream *substream);
/* Speech + Playback Stop function */
CALL_CTRL_NO_EXTERN void call_playback_stop(void);
/* Speech + Record Start function */
CALL_CTRL_NO_EXTERN int call_record_start(struct snd_pcm_substream *substream);
/* Speech + Record Stop function */
CALL_CTRL_NO_EXTERN void call_record_stop(void);

/* Voip(Playback) Start function */
CALL_CTRL_NO_EXTERN int call_playback_incomm_start(
        struct snd_pcm_substream *substream);
/* Voip(Playback) Stop function */
CALL_CTRL_NO_EXTERN void call_playback_incomm_stop(void);
/* Voip(Record) Start function */
CALL_CTRL_NO_EXTERN int call_record_incomm_start(struct snd_pcm_substream *substream);
/* Voip(Record) Stop function */
CALL_CTRL_NO_EXTERN void call_record_incomm_stop(void);

/* Speech + Playback/Record buffer offset return function */
CALL_CTRL_NO_EXTERN snd_pcm_uframes_t call_pcmdata_pointer(
	struct snd_pcm_substream *substream);
/* VOCODER Set VQA mode function */
CALL_CTRL_NO_EXTERN int call_set_vqa(u_int value);
/* VOCODER Set Callback function for VCD Watch function */
CALL_CTRL_NO_EXTERN int call_regist_watch(struct call_vcd_callback *func);
/* Record dummy change function */
CALL_CTRL_NO_EXTERN void call_change_dummy_rec(void);
CALL_CTRL_NO_EXTERN void call_change_dummy_play(void);
/* Create work queue function */
CALL_CTRL_NO_EXTERN int call_create_workque(void);
/* Destroy work queue function */
CALL_CTRL_NO_EXTERN void call_destroy_workque(void);
/* Void get buffer function */
CALL_CTRL_NO_EXTERN void call_get_incomm_buffer(void);
/* VoIP buffer offset return function */
CALL_CTRL_NO_EXTERN snd_pcm_uframes_t call_incomm_pcmdata_pointer(struct snd_pcm_substream *substream);

/* Change dummy playing for incommunication */
CALL_CTRL_NO_EXTERN void call_change_incomm_play(void);
/* Change dummy recording for incommunication */
CALL_CTRL_NO_EXTERN void call_change_incomm_rec(void);

#endif /* __CALL_EXTERN_H__ */

