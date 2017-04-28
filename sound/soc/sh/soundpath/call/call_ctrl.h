/* call_ctrl.h
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

#ifndef __CALL_CTRL_H__
#define __CALL_CTRL_H__

#include <linux/kernel.h>

#include <linux/vcd/vcd.h>

/*
 *
 * DEFINE Definitions
 *
 */
#define CALL_WAIT_TIME	(20)


/*
 *
 * STRUCTURE Definitions
 *
 */

/* Vocoder buf index */
enum call_data_side {
	DATA_SIDE_0 = 0,
	DATA_SIDE_1,
	DATA_SIDE_MAX
};

enum call_dummy_play_info {
	CALL_DUMMY_PLAY_YET = 0,
	CALL_DUMMY_PLAY,
	CALL_DUMMY_PLAY_FW
};

/* Data information */
struct call_pcm_info {
	int		next_pd_side;
	long		buffer_len;
	long		byte_offset;
	int		period_len;
	int		period;
	u_int		save_buf_size;
};


/* Data information */
struct call_incomm_pcm_info {
	int		next_pd_side;
	long		buffer_len;
	long		byte_offset;
	int		period_len;
	int		period;
};

/*
 *
 * PROTOTYPE Declarations
 *
 */

/* PCM information initialization function */
static void call_pcm_info_init(
	struct snd_pcm_substream *substream,
	enum call_status kind);
/* PCM information initialization function */
static void call_incomm_pcm_info_init(
	struct snd_pcm_substream *substream,
	enum call_status kind);
/* PCM information initialization function */
static void call_incomm_pcm_info_init(
	struct snd_pcm_substream *substream,
	enum call_status kind);
/* Vocoder API call function */
static int call_vcd_execute(int command, void *arg);
/* Playback data setting function */
static void call_playback_data_set(void);
/* Playback data setting function for Production-test(Loopback) */
static void call_playback_data_set_for_pt(void);
/* Capture data reaping function */
static void call_record_data_set(void);

/* Playback incommunication data setting function */
static void call_playback_incomm_data_set(unsigned int buf_size);
/* Capture incommunication data reaping function */
static void call_record_incomm_data_set(unsigned int buf_size);

/* play dummy(before start fw) change function */
static void call_change_dummy_play_before_fw(void);

/* Callback for Playback */
static void call_playback_cb(void);
/* Callback for Capture */
static void call_record_cb(void);
/* Callback for Playback incommunication */
static void call_playback_incomm_cb(unsigned int buf_size);
/* Callback for Capture incommunication */
static void call_record_incomm_cb(unsigned int buf_size);

/* Work queue process function */
static void call_work_dummy_rec(struct sndp_work_info *work);
static bool call_work_play_wait_event(struct sndp_work_info *work);
static void call_work_before_start_fw(struct sndp_work_info *work);
static void call_work_dummy_play(struct sndp_work_info *work);

/* Dummy playback incommunication data setting function */
static void call_playback_incomm_dummy(unsigned int buf_size);
/* Dummy record incommunication data setting function */
static void call_record_incomm_dummy(unsigned int buf_size);

/* Dummy playing for incommunication */
static void call_work_playback_incomm_dummy_set(struct sndp_work_info *work);
/* Dummy recording for incommunication */
static void call_work_record_incomm_dummy_set(struct sndp_work_info *work);

/* Wait queue initialization function */
static DECLARE_WAIT_QUEUE_HEAD(g_call_wait_in);
static DECLARE_WAIT_QUEUE_HEAD(g_call_wait_out);

#endif /* __CALL_CTRL_H__ */
