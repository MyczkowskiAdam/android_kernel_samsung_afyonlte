/* call_ctrl.c
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


#define __CALL_CTRL_NO_EXTERN__

#include <linux/jiffies.h>
#include <asm/atomic.h>
#include <sound/soundpath/call_extern.h>

#include "call_ctrl.h"

/*
 *
 * GLOBAL DATA Definitions
 *
 */

/* Playback buf address from Vocoder */
static u_int g_call_play_data_addr[DATA_SIDE_MAX] = {0, 0};
/* Record buf address from Vocoder */
static u_int g_call_rec_data_addr[DATA_SIDE_MAX] = {0, 0};
/* Stream information from linux kernel */
static struct snd_pcm_substream *g_call_substream[SNDP_PCM_DIRECTION_MAX];
/* Data information for Playback and Capture */
static struct call_pcm_info g_call_pcm_info[SNDP_PCM_DIRECTION_MAX];

/* Playback incommunication buf address from Vocoder */
static u_int g_call_play_incomm_data_addr[DATA_SIDE_MAX] = {0, 0};
/* Record incommunication buf address from Vocoder */
static u_int g_call_rec_incomm_data_addr[DATA_SIDE_MAX] = {0, 0};
/* Stream information from linux kernel */
static struct snd_pcm_substream *g_call_incomm_substream[SNDP_PCM_DIRECTION_MAX] = {
	NULL, NULL
};
/* Data information for Playback and Capture */
static struct call_pcm_info g_call_incomm_pcm_info[SNDP_PCM_DIRECTION_MAX] = {
	{ next_pd_side : DATA_SIDE_0, },
	{ next_pd_side : DATA_SIDE_0, },
};

/* Call status */
enum call_status g_status = IDLE_STATUS;
/* Uplink status */
static bool g_call_play_uplink;
/* Dummy play/record flag */
static bool g_call_dummy_rec;
static enum call_dummy_play_info g_call_dummy_play;

/* Work queue */
struct sndp_workqueue	*g_call_queue_in;
struct sndp_workqueue	*g_call_queue_out;

/* Temporary area information(Play) */
static u8 g_call_playback_buff[VCD_PLAYBACK_BUFFER_SIZE];
static u8 g_call_playback_buff_for_pt[VCD_PLAYBACK_PT_BUFFER_SIZE * 2];
static long g_call_playback_len;

/* Temporary area information(Incomm Play) */
static u8 g_call_playback_incomm_buff[4096];
static long g_call_playback_incomm_len;

/* Temporary area information(Rec) */
static u8 g_call_record_buff[VCD_RECORD_BUFFER_SIZE];
static long g_call_record_len;
static long g_call_dummy_record_len;
static long g_call_dummy_play_len;

/* Temporary area information(Incomm Rec) */
static u8 g_call_record_incomm_buff[4096];
static long g_call_record_incomm_len;

#ifdef DEBUG
static bool g_call_cb_debug[SNDP_PCM_DIRECTION_MAX] = { true, true };
#endif

static struct sndp_work_info g_call_work_before_start_fw;

/* Work queue initialization function */
static struct sndp_work_info g_call_work_in;
static struct sndp_work_info g_call_work_out;
static struct sndp_work_info g_call_work_playback_incomm_dummy_set;
static struct sndp_work_info g_call_work_record_incomm_dummy_set;

static struct vcd_playback_option g_vcd_ply_opt;

/* Firmware abnormality state */
extern int g_sndp_stream_route;

/*!
   @brief Speech + Playback Start function

   @param[in]	substream	PCM substream structure
   @param[out]	none

   @retval	0		Successful
   @retval	-EPERM		Not ready
 */
int call_playback_start(struct snd_pcm_substream *substream)
{
	/* Local variable declaration */
	int				ret = ERROR_NONE;
	struct vcd_playback_buffer_info	vcd_ply_buf;

	sndp_log_debug_func("start\n");

	/* Status update */
	g_status |= PLAY_STATUS;

#ifdef DEBUG
	g_call_cb_debug[SNDP_PCM_OUT] = true;
#endif

	/* Playback data information initialize */
	call_pcm_info_init(substream, PLAY_STATUS);

	/* Get playback buffer */
	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_GET_PLAYBACK_BUFFER\n");
	ret = call_vcd_execute((int)VCD_COMMAND_GET_PLAYBACK_BUFFER,
			       (void *)&vcd_ply_buf);
	if (ERROR_NONE != ret) {
		sndp_log_err("Error COMMAND_GET_PLAYBACK_BUFFER(code=%d)\n",
			     ret);
		g_call_pcm_info[SNDP_PCM_OUT].byte_offset = 16;
		/* Status update */
		g_status &= ~PLAY_STATUS;
		return -EPERM;
	}

	/* Data pointer setting */
	g_call_play_data_addr[DATA_SIDE_0] =
		(u_int)vcd_ply_buf.playback_buffer[DATA_SIDE_0];
	g_call_play_data_addr[DATA_SIDE_1] =
		(u_int)vcd_ply_buf.playback_buffer[DATA_SIDE_1];

	/* Play mode set */
	if (0 == g_sndp_mode) {
		/*
		 * DownLink
		 * UL : Speech Enable,  Playback Disable
		 * DL : Speech Enable,  Playback Enable
		 */
		if (false == g_call_play_uplink) {
			g_vcd_ply_opt.mode = VCD_PLAYBACK_MODE_0;
		/*
		 * UpLink
		 * UL : Speech Disable, Playback Enable
		 * DL : Speech Disable, Playback Disable
		 */
		} else {
			g_vcd_ply_opt.mode = VCD_PLAYBACK_MODE_1;
		}
	/* For debug, Force mode change */
	} else {
		g_vcd_ply_opt.mode = g_sndp_mode - 1;
	}

	/* Callback pointer set */
	g_vcd_ply_opt.beginning_buffer = call_playback_cb;

	/* Play data set */
	if (SNDP_PT_NOT_STARTED == g_pt_start)
		call_playback_data_set();
	else
		call_playback_data_set_for_pt();

	/* Start speech playback */
	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_START_PLAYBACK\n");

	ret = call_vcd_execute((int)VCD_COMMAND_START_PLAYBACK,
			       (void *)&g_vcd_ply_opt);

	if (ERROR_NONE != ret) {
		sndp_log_err("VCD_COMMAND_START_PLAYBACK return (code=%d)\n",
				ret);
		if (atomic_read(&g_call_watch_start_fw)) {
			/* after firmware starting */
			sndp_log_info("path switch\n");
			g_call_pcm_info[SNDP_PCM_OUT].byte_offset = 16;
			/* Status update */
			g_status &= ~PLAY_STATUS;
			ret = -EPERM;
		} else {
			/* before firmware starting */
			sndp_log_info("dummy play\n");
			call_change_dummy_play_before_fw();
			ret = ERROR_NONE;
		}
	}

	sndp_log_debug_func("end\n");
	return ret;
}


/*!
   @brief Speech + Playback Stop function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void call_playback_stop(void)
{
	sndp_log_debug_func("start\n");

	/* Dummy status check */
	if (CALL_DUMMY_PLAY_YET != g_call_dummy_play) {
		g_call_dummy_play = CALL_DUMMY_PLAY_YET;
		wake_up_interruptible(&g_call_wait_out);
		sndp_log_info("DUMMY_PLAY OFF [%d]\n", g_call_dummy_play);
		return;
	}

	/* Stop speech playback */
	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_STOP_PLAYBACK\n");
	(void)call_vcd_execute((int)VCD_COMMAND_STOP_PLAYBACK, NULL);

	/* If cross Vocoder callback, Data information clear */
	g_call_playback_len = 0;
	g_call_pcm_info[SNDP_PCM_OUT].byte_offset = 0;

	/* Status update */
	g_status &= ~PLAY_STATUS;

	sndp_log_debug_func("end\n");
}


/*!
   @brief Speech + Record Start function

   @param[in]	substream	PCM substream structure
   @param[out]	none

   @retval	0		Successful
   @retval	-EPERM		Not ready
 */
int call_record_start(struct snd_pcm_substream *substream)
{
	/* Local variable declaration */
	int				ret = ERROR_NONE;
	struct vcd_record_option	vcd_rec_opt;
	struct vcd_record_buffer_info	vcd_rec_buf;

	sndp_log_debug_func("start\n");

	/* Status update */
	g_status |= REC_STATUS;

#ifdef DEBUG
	g_call_cb_debug[SNDP_PCM_IN] = true;
#endif

	/* Record information clear */
	call_pcm_info_init(substream, REC_STATUS);

	/* Get record buffer */
	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_GET_RECORD_BUFFER\n");
	ret = call_vcd_execute((int)VCD_COMMAND_GET_RECORD_BUFFER,
			       (void *)&vcd_rec_buf);
	if (ERROR_NONE != ret) {
		sndp_log_err("Error COMMAND_GET_RECORD_BUFFER(code=%d)\n",
			     ret);
		/* Status update */
		g_status &= ~REC_STATUS;
		return -EPERM;
	}

	/* Data pointer setting */
	g_call_rec_data_addr[DATA_SIDE_0] =
		(u_int)vcd_rec_buf.record_buffer[DATA_SIDE_0];
	g_call_rec_data_addr[DATA_SIDE_1] =
		(u_int)vcd_rec_buf.record_buffer[DATA_SIDE_1];

	/*
	 * Rec mode set
	 * UL : Enable, DL : Enable
	 */
	vcd_rec_opt.mode = VCD_RECORD_MODE_0;

	/* Callback pointer set */
	vcd_rec_opt.complete_buffer = call_record_cb;

	/* Start speech record */
	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_START_RECORD\n");

	ret = call_vcd_execute((int)VCD_COMMAND_START_RECORD,
			       (void *)&vcd_rec_opt);

	if (ERROR_NONE != ret) {
		sndp_log_err("Error COMMAND_START_RECORD(code=%d)\n", ret);
		/* Status update */
		g_status &= ~REC_STATUS;
		ret = -EPERM;
	}

	sndp_log_debug_func("end\n");
	return ret;
}


/*!
   @brief Speech + Record Stop function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void call_record_stop(void)
{
	sndp_log_debug_func("start\n");

	/* Dummy status check */
	if (g_call_dummy_rec) {
		g_call_dummy_rec = false;
		wake_up_interruptible(&g_call_wait_in);
		sndp_log_info("DUMMY_REC OFF [%02x]\n", g_call_dummy_rec);
		return;
	}

	/* Stop speech record */
	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_STOP_RECORD\n");
	(void)call_vcd_execute((int)VCD_COMMAND_STOP_RECORD, NULL);

	/* If cross Vocoder callback, Data information clear */
	g_call_record_len = 0;
	g_call_pcm_info[SNDP_PCM_IN].byte_offset = 0;

	/* Status update */
	g_status &= ~REC_STATUS;

	sndp_log_debug_func("end\n");
}

/*!
   @brief Voip(Playback) Start function

   @param[in]	substream	PCM substream structure
   @param[out]	none

   @retval	none
 */
int call_playback_incomm_start(struct snd_pcm_substream *substream)
{
	int                             ret = ERROR_NONE;

	sndp_log_debug_func("start\n");
	/* Voip data information initialize */
	call_incomm_pcm_info_init(substream, PLAY_INCOMM_STATUS);

	/* Status update */
	g_status |= PLAY_INCOMM_STATUS;

	if (g_call_incomm_cb[SNDP_PCM_OUT] ||
		atomic_read(&g_call_watch_stop_fw))
		call_change_incomm_play();

	sndp_log_debug_func("end\n");
	return ret;
}

/*!
   @brief Void(Playback) Stop function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void call_playback_incomm_stop(void)
{
	sndp_log_debug_func("start\n");

	/* Status update */
	g_status &= ~PLAY_INCOMM_STATUS;
	if (SNDP_ROUTE_PLAY_INCOMM_DUMMY & g_sndp_stream_route) {
		g_sndp_stream_route &= ~SNDP_ROUTE_PLAY_INCOMM_DUMMY;
		wake_up_interruptible(&g_call_wait_out);
	}

	/* If cross Vocoder callback, Data information clear */
	g_call_incomm_substream[SNDP_PCM_OUT] = NULL;
	g_call_playback_incomm_len = 0;
	g_call_incomm_pcm_info[SNDP_PCM_OUT].byte_offset = 0;
	g_call_incomm_pcm_info[SNDP_PCM_OUT].next_pd_side = DATA_SIDE_0;

	sndp_log_debug_func("end\n");
}

/*!
   @brief Voip(Record) Start function

   @param[in]	substream	PCM substream structure
   @param[out]	none

   @retval	none
 */
int call_record_incomm_start(struct snd_pcm_substream *substream)
{
	int                             ret = ERROR_NONE;

	sndp_log_debug_func("start\n");

	/* Record information clear */
	call_incomm_pcm_info_init(substream, REC_INCOMM_STATUS);

	/* Status update */
	g_status |= REC_INCOMM_STATUS;

	if (g_call_incomm_cb[SNDP_PCM_IN] ||
		atomic_read(&g_call_watch_stop_fw))
		call_change_incomm_rec();

	sndp_log_debug_func("end\n");
	return ret;
}

/*!
   @brief Voip(Record) Stop function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void call_record_incomm_stop(void)
{
	sndp_log_debug_func("start\n");

	/* Status update */
	g_status &= ~REC_INCOMM_STATUS;
	if (SNDP_ROUTE_CAP_INCOMM_DUMMY & g_sndp_stream_route) {
		g_sndp_stream_route &= ~SNDP_ROUTE_CAP_INCOMM_DUMMY;
		wake_up_interruptible(&g_call_wait_in);
	}

	/* If cross Vocoder callback, Data information clear */
	g_call_incomm_substream[SNDP_PCM_IN] = NULL;
	g_call_record_incomm_len = 0;
	g_call_incomm_pcm_info[SNDP_PCM_IN].byte_offset = 0;
	g_call_incomm_pcm_info[SNDP_PCM_IN].next_pd_side = DATA_SIDE_0;
	
	sndp_log_debug_func("end\n");
}

/*!
   @brief Speech + Playback/Record buffer offset return function

   @param[in]	none
   @param[out]	none

   @retval	PCM data buffer offset
 */
snd_pcm_uframes_t call_pcmdata_pointer(struct snd_pcm_substream *substream)
{
	long location = g_call_pcm_info[substream->stream].byte_offset;

#ifdef DEBUG
	/* If offset < 16Byte, Reduce the amount of log. */
	if ((g_call_pcm_info[substream->stream].buffer_len - 16) <=
	    location) {
		sndp_log_info("%ld frames (%ld bytes)\n",
			bytes_to_frames(substream->runtime, location),
			location);
	}

#endif
	return bytes_to_frames(substream->runtime, location);
}


/*!
   @brief VOCODER Set Callback function for VCD Watch

   @param[in]	Callback function address
   @param[out]	none

   @retval	0	Successful
 */
int call_regist_watch(struct call_vcd_callback *func)
{
	/* Local variable declaration */
	int ret = ERROR_NONE;
	struct vcd_watch_fw_info watch_fw_info;
	struct vcd_watch_clkgen_info watch_clkgen_info;
	struct vcd_wait_path_info wait_path_info;
	struct vcd_voip_callback voip_callback;
	struct vcd_watch_codec_type_info watch_codec_type_info;

	sndp_log_debug_func("start\n");

	/* Set callback for Vocoder */
	watch_fw_info.start_fw = func->callback_start_fw;
	watch_fw_info.stop_fw = func->callback_stop_fw;

	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_WATCH_FW\n");
	ret = call_vcd_execute((int)VCD_COMMAND_WATCH_FW,
			       (void *)&watch_fw_info);
	if (ERROR_NONE > ret) {
		sndp_log_err("Error COMMAND_WATCH_FW(code=%d)\n", ret);
		return -EPERM;
	}

	/* Set callback for Vocoder */
	watch_clkgen_info.start_clkgen = func->callback_start_clk;
	watch_clkgen_info.stop_clkgen = func->callback_stop_clk;

	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_WATCH_CLKGEN\n");
	ret = call_vcd_execute((int)VCD_COMMAND_WATCH_CLKGEN,
			       (void *)&watch_clkgen_info);
	if (ERROR_NONE > ret) {
		sndp_log_err(
		"Error VCD_COMMAND_WATCH_CLKGEN(code=%d)\n", ret);
		return -EPERM;
	}

	/* Set callback for Vocoder */
	wait_path_info.wait_path = func->callback_wait_path;

	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_WAIT_PATH\n");
	ret = call_vcd_execute((int)VCD_COMMAND_WAIT_PATH,
			       (void *)&wait_path_info);
	if (ERROR_NONE > ret) {
		sndp_log_err(
		"Error VCD_COMMAND_WAIT_PATH(code=%d)\n", ret);
		return -EPERM;
	}

	/* Set Incomm for Vocoder */
	voip_callback.voip_ul_callback = call_record_incomm_cb;
	voip_callback.voip_dl_callback = call_playback_incomm_cb;

	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_SET_VOIP_CALLBACK\n");
	ret = call_vcd_execute((int)VCD_COMMAND_SET_VOIP_CALLBACK,
		        (void *)&voip_callback);
	if (ERROR_NONE > ret) {
		sndp_log_err(
		"Error VCD_COMMAND_SET_VOIP_CALLBACK(code=%d)\n", ret);
		return -EPERM;
	}

	/* Set callback for Vocoder */
	watch_codec_type_info.codec_type_ind = func->callback_codec_type;

	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_WATCH_CODEC_TYPE\n");
	ret = call_vcd_execute((int)VCD_COMMAND_WATCH_CODEC_TYPE,
			       (void *)&watch_codec_type_info);
	if (ERROR_NONE > ret) {
		sndp_log_err(
		"Error VCD_COMMAND_WATCH_CODEC_TYPE(code=%d)\n", ret);
		return -EPERM;
	}

	sndp_log_debug_func("end\n");
	return ERROR_NONE;
}


/*!
   @brief Record dummy change function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void call_change_dummy_rec(void)
{
	sndp_log_debug_func("start\n");

	/* Dummy rec ON */
	g_call_dummy_rec = true;
	g_call_dummy_record_len = g_call_record_len;
	sndp_workqueue_enqueue(g_call_queue_in, &g_call_work_in);

	/* If Record status ON */
	if (REC_STATUS & g_status)
		g_status &= ~REC_STATUS;

	sndp_log_debug_func("end\n");
}


/*!
   @brief play dummy(before start fw) change function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void call_change_dummy_play_before_fw(void)
{
	sndp_log_debug_func("start\n");

	/* Dummy rec ON */
	g_call_dummy_play = CALL_DUMMY_PLAY_FW;
	g_call_dummy_play_len = g_call_playback_len;
	sndp_workqueue_enqueue(g_call_queue_out, &g_call_work_before_start_fw);

	/* If Record status ON */
	if (PLAY_STATUS & g_status)
		g_status &= ~PLAY_STATUS;

	sndp_log_debug_func("end\n");
}


/*!
   @brief Play dummy change function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void call_change_dummy_play(void)
{
	sndp_log_debug_func("start\n");

	/* Dummy rec ON */
	g_call_dummy_play = CALL_DUMMY_PLAY;
	g_call_dummy_play_len = g_call_playback_len;
	sndp_workqueue_enqueue(g_call_queue_out, &g_call_work_out);

	/* If Record status ON */
	if (PLAY_STATUS & g_status)
		g_status &= ~PLAY_STATUS;

	sndp_log_debug_func("end\n");
}

/*!
   @brief create work queue

   @param[in]	none
   @param[out]	none

   @retval	0		Successful
   @retval	-ENOMEM		create_workqueue error.
 */
int call_create_workque(void)
{
	sndp_log_debug_func("start\n");

	sndp_work_initialize(&g_call_work_in, call_work_dummy_rec, NULL);
	sndp_work_initialize(&g_call_work_out, call_work_dummy_play, NULL);
	sndp_work_initialize(&g_call_work_before_start_fw,
		  call_work_before_start_fw, NULL);
	sndp_work_initialize(&g_call_work_playback_incomm_dummy_set,
		  call_work_playback_incomm_dummy_set, NULL);
	sndp_work_initialize(&g_call_work_record_incomm_dummy_set,
		  call_work_record_incomm_dummy_set, NULL);

	/* create work queue */
	g_call_queue_in = sndp_workqueue_create("sndp_queue_in");
	if (NULL == g_call_queue_in) {
		sndp_log_err("in queue create error.\n");
		return -ENOMEM;
	}

	/* create work queue */
	g_call_queue_out = sndp_workqueue_create("sndp_queue_out");
	if (NULL == g_call_queue_out) {
		sndp_log_err("out queue create error.\n");
		return -ENOMEM;
	}

	sndp_log_debug_func("end\n");
	return ERROR_NONE;
}


/*!
   @brief destroy work queue

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void call_destroy_workque(void)
{
	sndp_log_debug_func("start\n");

	/* destroy work queue */
	if (NULL != g_call_queue_in) {
		sndp_workqueue_destroy(g_call_queue_in);
		g_call_queue_in = NULL;
	}

	/* destroy work queue */
	if (NULL != g_call_queue_out) {
		sndp_workqueue_destroy(g_call_queue_out);
		g_call_queue_out = NULL;
	}

	sndp_log_debug_func("end\n");
}


/*!
   @brief PCM information initialization function

   @param[in]	substream	PCM substream structure
   @param[in]	kind		Direction(playback/capture)
   @param[out]	none

   @retval	none
 */
static void call_pcm_info_init(
	struct snd_pcm_substream *substream,
	enum call_status kind)
{
	/* Local variable declaration */
	int			direction;
	struct snd_pcm_runtime	*runtime = substream->runtime;

	sndp_log_debug_func("start\n");

	direction = (PLAY_STATUS == kind) ? SNDP_PCM_OUT : SNDP_PCM_IN;

	/* Set substream */
	g_call_substream[direction] = substream;

	/* Pcm information clear */
	g_call_pcm_info[direction].buffer_len =
		runtime->buffer_size * runtime->frame_bits / 8;

	g_call_pcm_info[direction].byte_offset = 0;

	g_call_pcm_info[direction].next_pd_side = DATA_SIDE_0;

	g_call_pcm_info[direction].period_len =
		frames_to_bytes(runtime, runtime->period_size);

	g_call_pcm_info[direction].period = 0;

	sndp_log_info("buffer_len %ld (buffer_size %ld) period_len %d\n",
		g_call_pcm_info[direction].buffer_len,
		runtime->buffer_size,
		g_call_pcm_info[direction].period_len);

	sndp_log_debug_func("end\n");
}


/*!
   @brief Vocoder API call function

   @param[in]	Vocoder api command
   @param[in]	Vocoder api parameter
   @param[out]	none

   @retval	Vocoder api result
 */
static int call_vcd_execute(int command, void *arg)
{
	/* Local variable declaration */
	int				ret = ERROR_NONE;
	struct vcd_execute_command	vcd_exec;

	sndp_log_debug_func("start\n");

	/* Vocoder parameter set */
	vcd_exec.command = command;
	vcd_exec.arg = arg;

	/* Vocoder API call */
	ret = vcd_execute(&vcd_exec);

	sndp_log_debug_func("end\n");
	return ret;
}


/*!
   @brief Speech + Playback Data set function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void call_playback_data_set(void)
{
	/* Local variable declaration */
	long			call_playback_len = 0;
	struct call_pcm_info	*pcm_info = &g_call_pcm_info[SNDP_PCM_OUT];
	int			next_pd_side = pcm_info->next_pd_side;
	struct snd_pcm_runtime	*runtime;

	/* If firmware is abnormality */
	if (SNDP_ROUTE_PLAY_CHANGED & g_sndp_stream_route) {
		sndp_log_info("Firmware is abnormality");
		memset((void *)g_call_play_data_addr[DATA_SIDE_0],
			'\0', VCD_PLAYBACK_BUFFER_SIZE);
		memset((void *)g_call_play_data_addr[DATA_SIDE_1],
			'\0', VCD_PLAYBACK_BUFFER_SIZE);
		return;
	}

	/* If Playback status OFF */
	if (!(PLAY_STATUS & g_status)) {
		sndp_log_info("Playback not ready");
		return;
	}

	/* If process had been driver closed. */
	if (NULL == g_call_substream[SNDP_PCM_OUT]->runtime) {
		sndp_log_info("runtime is NULL\n");
		return;
	}

	runtime = g_call_substream[SNDP_PCM_OUT]->runtime;

	if (NULL == runtime->dma_area) {
		sndp_log_info("dma_area is NULL\n");
		return;
	}

	/* Copy the data from temporary area. */
	if (0 < g_call_playback_len) {
		/* Set data */
		memcpy((void *)g_call_play_data_addr[next_pd_side],
		       (void *)g_call_playback_buff,
		       g_call_playback_len);

		/* Clear temporary area. */
		memset(g_call_playback_buff, '\0', g_call_playback_len);
		/* Clear temporary information. */
		pcm_info->period = 0;
		call_playback_len = g_call_playback_len;
		g_call_playback_len = 0;
	}

	/* Set data, If Buffer <= Data */
	if ((VCD_PLAYBACK_BUFFER_SIZE - call_playback_len) <=
		(pcm_info->buffer_len - pcm_info->byte_offset)) {
		/* Set data */
		memcpy((void *)(g_call_play_data_addr[next_pd_side] +
							call_playback_len),
			runtime->dma_area + pcm_info->byte_offset,
			VCD_PLAYBACK_BUFFER_SIZE - call_playback_len);

		/* Offset update */
		pcm_info->byte_offset += VCD_PLAYBACK_BUFFER_SIZE - call_playback_len;

		/* If Buffer > Data */
		if (VCD_PLAYBACK_BUFFER_SIZE >
			(pcm_info->buffer_len - pcm_info->byte_offset)) {
			/*
			 * Length of the temporary area
			 * to update information.
			 */
			g_call_playback_len =
				pcm_info->buffer_len - pcm_info->byte_offset;

			/* Copy the data to temporary area. */
			memcpy((void *)g_call_playback_buff,
				runtime->dma_area + pcm_info->byte_offset,
				g_call_playback_len);

			/* Offset update */
			pcm_info->byte_offset += g_call_playback_len;
		}
	/* Set data, If Buffer > Data */
	} else {
		/* 0 padding */
		memset((void *)(g_call_play_data_addr[next_pd_side] +
							call_playback_len),
			'\0',
			VCD_PLAYBACK_BUFFER_SIZE - call_playback_len);

		/* Set data */
		memcpy((void *)(g_call_play_data_addr[next_pd_side] +
							call_playback_len),
			runtime->dma_area + pcm_info->byte_offset,
			pcm_info->buffer_len - pcm_info->byte_offset);

		/* Offset update */
		pcm_info->byte_offset +=
			(pcm_info->buffer_len - pcm_info->byte_offset);
	}

	/* If it reaches the period of data */
	if ((0 < call_playback_len) ||
	    (pcm_info->byte_offset >=
		(pcm_info->period_len * (pcm_info->period + 1)))) {

		/* Period update */
		pcm_info->period =
			pcm_info->byte_offset / pcm_info->period_len;

		if (runtime->periods == pcm_info->period) {
			/* Update again, the period */
			pcm_info->period = 0;
			pcm_info->byte_offset = 0;
		}

		/* Notification period to ALSA */
		snd_pcm_period_elapsed(g_call_substream[SNDP_PCM_OUT]);
	}

	/* Set next position */
	pcm_info->next_pd_side = (DATA_SIDE_0 == pcm_info->next_pd_side) ?
						DATA_SIDE_1 : DATA_SIDE_0;
}


/*!
   @brief Speech + Playback Data set function for Production-test(Loopback)

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void call_playback_data_set_for_pt(void)
{
	/* Local variable declaration */
	long			call_playback_len = 0;
	struct call_pcm_info	*pcm_info = &g_call_pcm_info[SNDP_PCM_OUT];
	int			next_pd_side = pcm_info->next_pd_side;
	struct snd_pcm_runtime	*runtime;
	int			iCnt;

	/* If firmware is abnormality */
	if (SNDP_ROUTE_PLAY_CHANGED & g_sndp_stream_route) {
		sndp_log_info("Firmware is abnormality");
		memset((void *)g_call_play_data_addr[DATA_SIDE_0],
			'\0', VCD_PLAYBACK_PT_BUFFER_SIZE);
		memset((void *)g_call_play_data_addr[DATA_SIDE_1],
			'\0', VCD_PLAYBACK_PT_BUFFER_SIZE);
		return;
	}

	/* If Playback status OFF */
	if (!(PLAY_STATUS & g_status)) {
		sndp_log_info("Playback not ready");
		return;
	}

	/* If process had been driver closed. */
	if (NULL == g_call_substream[SNDP_PCM_OUT]->runtime) {
		sndp_log_info("runtime is NULL\n");
		return;
	}

	runtime = g_call_substream[SNDP_PCM_OUT]->runtime;

	if (NULL == runtime->dma_area) {
		sndp_log_info("dma_area is NULL\n");
		return;
	}

	/* Copy the data from temporary area. */
	if (0 < g_call_playback_len) {
		/* Set data */
		for (iCnt = 0; iCnt < g_call_playback_len / 4; iCnt++)
			memcpy((void *)(g_call_play_data_addr[next_pd_side] + iCnt * 2),
			       (void *)(g_call_playback_buff_for_pt + iCnt * 4),
				2);

		/* Clear temporary area. */
		memset(g_call_playback_buff_for_pt, '\0', g_call_playback_len);
		/* Clear temporary information. */
		pcm_info->period = 0;
		call_playback_len = g_call_playback_len;
		g_call_playback_len = 0;
	}

	/* Set data, If Buffer <= Data */
	if ((VCD_PLAYBACK_PT_BUFFER_SIZE * 2 - call_playback_len) <=
		(pcm_info->buffer_len - pcm_info->byte_offset)) {
		/* Set data */
		for (iCnt = 0;
		     iCnt < (VCD_PLAYBACK_PT_BUFFER_SIZE * 2 - call_playback_len) / 4;
		     iCnt++)
			memcpy((void *)(g_call_play_data_addr[next_pd_side] + (call_playback_len / 2) + (iCnt * 2)),
				(runtime->dma_area + pcm_info->byte_offset) + iCnt * 4,
				2);

		/* Offset update */
		pcm_info->byte_offset += VCD_PLAYBACK_PT_BUFFER_SIZE * 2 - call_playback_len;

		/* If Buffer > Data */
		if (VCD_PLAYBACK_PT_BUFFER_SIZE * 2 >
			(pcm_info->buffer_len - pcm_info->byte_offset)) {
			/*
			 * Length of the temporary area
			 * to update information.
			 */
			g_call_playback_len =
				pcm_info->buffer_len - pcm_info->byte_offset;

			/* Copy the data to temporary area. */
			memcpy((void *)g_call_playback_buff_for_pt,
				runtime->dma_area + pcm_info->byte_offset,
				g_call_playback_len);

			/* Offset update */
			pcm_info->byte_offset += g_call_playback_len;
		}
	/* Set data, If Buffer > Data */
	} else {
		/* 0 padding */
		memset((void *)(g_call_play_data_addr[next_pd_side] +
							call_playback_len / 2),
			'\0',
			VCD_PLAYBACK_PT_BUFFER_SIZE - call_playback_len / 2);

		/* Set data */
		for (iCnt = 0; iCnt < (pcm_info->buffer_len - pcm_info->byte_offset) / 4; iCnt++)
			memcpy((void *)(g_call_play_data_addr[next_pd_side] + (call_playback_len / 2) + iCnt * 2),
				(runtime->dma_area + pcm_info->byte_offset) + iCnt * 4,
				2);

		/* Offset update */
		pcm_info->byte_offset +=
			(pcm_info->buffer_len - pcm_info->byte_offset);
	}

	/* If it reaches the period of data */
	if ((0 < call_playback_len) ||
	    (pcm_info->byte_offset >=
		(pcm_info->period_len * (pcm_info->period + 1)))) {
		/* Period update */
		pcm_info->period =
			pcm_info->byte_offset / pcm_info->period_len;

		if (runtime->periods == pcm_info->period) {
			/* Update again, the period */
			pcm_info->period = 0;
			pcm_info->byte_offset = 0;
		}

		/* Notification period to ALSA */
		snd_pcm_period_elapsed(g_call_substream[SNDP_PCM_OUT]);
	}

	/* Set next position */
	pcm_info->next_pd_side = (DATA_SIDE_0 == pcm_info->next_pd_side) ?
						DATA_SIDE_1 : DATA_SIDE_0;
}


/*!
   @brief Speech + Record Data set function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void call_record_data_set(void)
{
	/* Local variable declaration */
	struct call_pcm_info	*pcm_info = &g_call_pcm_info[SNDP_PCM_IN];
	int			next_pd_side = pcm_info->next_pd_side;
	struct snd_pcm_runtime	*runtime;

	/* If Record status OFF */
	if (!(REC_STATUS & g_status)) {
		sndp_log_info("Record not ready");
		return;
	}

	/* If process had been driver closed. */
	if (NULL == g_call_substream[SNDP_PCM_IN]->runtime) {
		sndp_log_info("runtime is NULL\n");
		return;
	}

	runtime = g_call_substream[SNDP_PCM_IN]->runtime;

	if (NULL == runtime->dma_area) {
		sndp_log_info("dma_area is NULL\n");
		return;
	}

	/* Get data, If Buffer >= Data */
	if ((g_call_record_len + VCD_RECORD_BUFFER_SIZE) <=
		(pcm_info->buffer_len - pcm_info->byte_offset)) {
		/* Copy the data from temporary area. */
		if (0 < g_call_record_len) {
			/* Get data */
			memcpy((runtime->dma_area + pcm_info->byte_offset),
			       (void *)g_call_record_buff,
			       g_call_record_len);

			/* Clear temporary area and information. */
			memset(g_call_record_buff, '\0', g_call_record_len);
		}

		/* Get data */
		memcpy((void *)(runtime->dma_area +
				pcm_info->byte_offset +
				g_call_record_len),
		       (void *)g_call_rec_data_addr[next_pd_side],
		       VCD_RECORD_BUFFER_SIZE);

		/* Offset update */
		pcm_info->byte_offset +=
			(g_call_record_len + VCD_RECORD_BUFFER_SIZE);
		g_call_record_len = 0;

	/* Get data, If Buffer < Data */
	} else {
		/* Get data */
		memcpy((void *)(runtime->dma_area + pcm_info->byte_offset),
		       (void *)g_call_rec_data_addr[next_pd_side],
		       pcm_info->buffer_len - pcm_info->byte_offset);

		/* Length of the temporary area to update information. */
		g_call_record_len = VCD_RECORD_BUFFER_SIZE -
			(pcm_info->buffer_len - pcm_info->byte_offset);

		/* Copy the data to temporary area. */
		memcpy(g_call_record_buff,
		       (void *)(g_call_rec_data_addr[next_pd_side] +
			       (pcm_info->buffer_len - pcm_info->byte_offset)),
		       g_call_record_len);

		/* Offset update */
		pcm_info->byte_offset +=
			(pcm_info->buffer_len - pcm_info->byte_offset);
	}

	/* If it reaches the period of data */
	if ((0 < g_call_record_len) ||
	    (pcm_info->byte_offset >=
		(pcm_info->period_len * (pcm_info->period + 1)))) {

		/* Period update */
		pcm_info->period =
			pcm_info->byte_offset / pcm_info->period_len;

		if (runtime->periods == pcm_info->period) {
			/* Update again, the period */
			pcm_info->period = 0;
			pcm_info->byte_offset = 0;
		}

		/* Notification period to ALSA */
		snd_pcm_period_elapsed(g_call_substream[SNDP_PCM_IN]);
	}

	/* Set next position */
	pcm_info->next_pd_side = (DATA_SIDE_0 == pcm_info->next_pd_side) ?
						DATA_SIDE_1 : DATA_SIDE_0;
}

/*!
   @brief Playback incommunication Data set function

   @param[in]	buffer size
   @param[out]	none

   @retval	none
 */
static void call_playback_incomm_data_set(unsigned int buf_size)
{
	/* Local variable declaration */
	long			call_playback_len = 0;
	struct call_pcm_info	*pcm_info = &g_call_incomm_pcm_info[SNDP_PCM_OUT];
	int			next_pd_side = pcm_info->next_pd_side;
	struct snd_pcm_runtime	*runtime;
	int			ret;

	ret = down_interruptible(&g_sndp_wait_free[SNDP_PCM_OUT]);
	if (0 != ret)
		sndp_log_err("down_interruptible ret[%d]\n", ret);

	/* If process had been driver closed. */
	if (NULL == g_call_incomm_substream[SNDP_PCM_OUT])
		goto no_proc;
	if (NULL == g_call_incomm_substream[SNDP_PCM_OUT]->runtime)
		goto no_proc;

	runtime = g_call_incomm_substream[SNDP_PCM_OUT]->runtime;

	if (NULL == runtime->dma_area)
		goto no_proc;

	/* Copy the data from temporary area. */
	if (0 < g_call_playback_incomm_len) {
		/* Set data */
		memcpy((void *)g_call_play_incomm_data_addr[next_pd_side],
		       (void *)g_call_playback_incomm_buff,
		       g_call_playback_incomm_len);

		/* Clear temporary information. */
		pcm_info->period = 0;
		call_playback_len = g_call_playback_incomm_len;
		g_call_playback_incomm_len = 0;
	}

	/* Set data, If Buffer <= Data */
	if ((buf_size - call_playback_len) <=
		(pcm_info->buffer_len - pcm_info->byte_offset)) {
		/* Set data */
		memcpy((void *)(g_call_play_incomm_data_addr[next_pd_side] +
							call_playback_len),
			runtime->dma_area + pcm_info->byte_offset,
			buf_size - call_playback_len);

		/* Offset update */
		pcm_info->byte_offset += buf_size - call_playback_len;

		/* If Buffer > Data */
		if (buf_size >
			(pcm_info->buffer_len - pcm_info->byte_offset)) {
			/*
			 * Length of the temporary area
			 * to update information.
			 */
			g_call_playback_incomm_len =
				pcm_info->buffer_len - pcm_info->byte_offset;

			/* Copy the data to temporary area. */
			memcpy((void *)g_call_playback_incomm_buff,
				runtime->dma_area + pcm_info->byte_offset,
				g_call_playback_incomm_len);

			/* Offset update */
			pcm_info->byte_offset += g_call_playback_incomm_len;
		}
	/* Set data, If Buffer > Data */
	} else {
		/* Set data */
		memcpy((void *)(g_call_play_incomm_data_addr[next_pd_side] +
							call_playback_len),
			runtime->dma_area + pcm_info->byte_offset,
			pcm_info->buffer_len - pcm_info->byte_offset);

		/* Offset update */
		pcm_info->byte_offset +=
			(pcm_info->buffer_len - pcm_info->byte_offset);
	}

	/* If it reaches the period of data */
	if ((0 < call_playback_len) ||
	    (pcm_info->byte_offset >=
		(pcm_info->period_len * (pcm_info->period + 1)))) {

		/* Period update */
		pcm_info->period =
			pcm_info->byte_offset / pcm_info->period_len;

		if (runtime->periods == pcm_info->period) {
			/* Update again, the period */
			pcm_info->period = 0;
			pcm_info->byte_offset = 0;
		}

		/* Notification period to ALSA */
		snd_pcm_period_elapsed(g_call_incomm_substream[SNDP_PCM_OUT]);
	}

	/* Set next position */
	pcm_info->next_pd_side = (DATA_SIDE_0 == pcm_info->next_pd_side) ?
						DATA_SIDE_1 : DATA_SIDE_0;

no_proc:
	up(&g_sndp_wait_free[SNDP_PCM_OUT]);
}


/*!
   @brief Record incommunication Data set function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void call_record_incomm_data_set(unsigned int buf_size)
{
	/* Local variable declaration */
	struct call_pcm_info	*pcm_info = &g_call_incomm_pcm_info[SNDP_PCM_IN];
	int			next_pd_side = pcm_info->next_pd_side;
	struct snd_pcm_runtime	*runtime;
	int			ret;

	ret = down_interruptible(&g_sndp_wait_free[SNDP_PCM_IN]);
	if (0 != ret)
		sndp_log_err("down_interruptible ret[%d]\n", ret);

	/* If process had been driver closed. */
	if (NULL == g_call_incomm_substream[SNDP_PCM_IN])
		goto no_proc;
	if (NULL == g_call_incomm_substream[SNDP_PCM_IN]->runtime)
		goto no_proc;

	runtime = g_call_incomm_substream[SNDP_PCM_IN]->runtime;

	if (NULL == runtime->dma_area)
		goto no_proc;

	/* Get data, If Buffer >= Data */
	if ((g_call_record_incomm_len + buf_size) <=
		(pcm_info->buffer_len - pcm_info->byte_offset)) {
		/* Copy the data from temporary area. */
		if (0 < g_call_record_incomm_len) {
			/* Get data */
			memcpy((runtime->dma_area + pcm_info->byte_offset),
			       (void *)g_call_record_incomm_buff,
			       g_call_record_incomm_len);
		}

		/* Get data */
		memcpy((void *)(runtime->dma_area +
				pcm_info->byte_offset +
				g_call_record_incomm_len),
		       (void *)g_call_rec_incomm_data_addr[next_pd_side],
		       buf_size);

		/* Offset update */
		pcm_info->byte_offset += (g_call_record_incomm_len + buf_size);
		g_call_record_incomm_len = 0;

	/* Get data, If Buffer < Data */
	} else {
		/* Get data */
		memcpy((void *)(runtime->dma_area + pcm_info->byte_offset),
		       (void *)g_call_rec_incomm_data_addr[next_pd_side],
		       pcm_info->buffer_len - pcm_info->byte_offset);

		/* Length of the temporary area to update information. */
		g_call_record_incomm_len = buf_size -
			(pcm_info->buffer_len - pcm_info->byte_offset);

		/* Copy the data to temporary area. */
		memcpy(g_call_record_incomm_buff,
		       (void *)(g_call_rec_incomm_data_addr[next_pd_side] +
			       (pcm_info->buffer_len - pcm_info->byte_offset)),
		       g_call_record_incomm_len);

		/* Offset update */
		pcm_info->byte_offset +=
			(pcm_info->buffer_len - pcm_info->byte_offset);
	}

	/* If it reaches the period of data */
	if ((0 < g_call_record_incomm_len) ||
	    (pcm_info->byte_offset >=
		(pcm_info->period_len * (pcm_info->period + 1)))) {

		/* Period update */
		pcm_info->period =
			pcm_info->byte_offset / pcm_info->period_len;

		if (runtime->periods == pcm_info->period) {
			/* Update again, the period */
			pcm_info->period = 0;
			pcm_info->byte_offset = 0;
		}

		/* Notification period to ALSA */
		snd_pcm_period_elapsed(g_call_incomm_substream[SNDP_PCM_IN]);
	}

	/* Set next position */
	pcm_info->next_pd_side = (DATA_SIDE_0 == pcm_info->next_pd_side) ?
						DATA_SIDE_1 : DATA_SIDE_0;

no_proc:
	up(&g_sndp_wait_free[SNDP_PCM_IN]);
}

/*!
   @brief Speech + Playback Callback function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void call_playback_cb(void)
{
#ifdef DEBUG
	if (g_call_cb_debug[SNDP_PCM_OUT]) {
		sndp_log_info("first call for vocoder\n");
		g_call_cb_debug[SNDP_PCM_OUT] = false;
	}
#endif
	/* Data set */
	if (SNDP_PT_NOT_STARTED == g_pt_start)
		call_playback_data_set();
	else
		call_playback_data_set_for_pt();
}


/*!
   @brief Speech + Record Callback function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void call_record_cb(void)
{
#ifdef DEBUG
	if (g_call_cb_debug[SNDP_PCM_IN]) {
		sndp_log_info("first call for vocoder\n");
		g_call_cb_debug[SNDP_PCM_IN] = false;
	}
#endif
	/* Data get */
	call_record_data_set();
}

/*!
   @brief Playback incommunication Callback function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void call_playback_incomm_cb(unsigned int buf_size)
{
	if (g_call_incomm_cb[SNDP_PCM_OUT]) {
		sndp_log_info("first call for vocoder\n");

		g_call_incomm_pcm_info[SNDP_PCM_OUT].next_pd_side = DATA_SIDE_0;
		g_call_incomm_pcm_info[SNDP_PCM_OUT].save_buf_size = buf_size;
		g_call_incomm_cb[SNDP_PCM_OUT] = false;
	}

	if (SNDP_ROUTE_PLAY_INCOMM_DUMMY & g_sndp_stream_route) {
		g_sndp_stream_route &= ~SNDP_ROUTE_PLAY_INCOMM_DUMMY;
		wake_up_interruptible(&g_call_wait_out);
	}

	if (g_status & PLAY_INCOMM_STATUS)
		/* Data set */
		call_playback_incomm_data_set(buf_size);
	else
		/* Dummy */
		call_playback_incomm_dummy(buf_size);
}

/*!
   @brief Record incommunication Callback function

   @param[in]	none
   @param[out]	none

   @retval	none
 */
static void call_record_incomm_cb(unsigned int buf_size)
{
	if (g_call_incomm_cb[SNDP_PCM_IN]) {
		sndp_log_info("first call for vocoder\n");

		g_call_incomm_pcm_info[SNDP_PCM_IN].next_pd_side = DATA_SIDE_0;
		g_call_incomm_pcm_info[SNDP_PCM_IN].save_buf_size = buf_size;
		g_call_incomm_cb[SNDP_PCM_IN] = false;
	}

	if (SNDP_ROUTE_CAP_INCOMM_DUMMY & g_sndp_stream_route) {
		g_sndp_stream_route &= ~SNDP_ROUTE_CAP_INCOMM_DUMMY;
		wake_up_interruptible(&g_call_wait_in);
	}

	if (g_status & REC_INCOMM_STATUS)
		/* Data get */
		call_record_incomm_data_set(buf_size);
	else
		/* Dummy */
		call_record_incomm_dummy(buf_size);
}


/*!
   @brief Work queue Dammy Record process function

   @param[in]	Work queue
   @param[out]	none

   @retval	none
 */
static void call_work_dummy_rec(struct sndp_work_info *work)
{
	/* Local variable declaration */
	long wait_ret;
	struct call_pcm_info	*pcm_info = &g_call_pcm_info[SNDP_PCM_IN];
	struct snd_pcm_runtime	*runtime;
	int			ret;

	/* sndp_log_debug_func("start\n"); */

	/* From work queue */
	if (NULL != work) {
		/* 20msec wait */
		wait_ret = wait_event_interruptible_timeout(
				g_call_wait_in,
				!(g_call_dummy_rec),
				msecs_to_jiffies(CALL_WAIT_TIME));

		/* Status check */
		if ((!(g_call_dummy_rec)) || (0 != wait_ret)) {
			sndp_log_info("dummy flag %02x\n", g_call_dummy_rec);
			return;
		}
	}

	ret = down_interruptible(&g_sndp_wait_free[SNDP_PCM_IN]);
	if (0 != ret)
		sndp_log_err("down_interruptible ret[%d]\n", ret);

	/* If process had been driver closed. */
	if (NULL == g_call_substream[SNDP_PCM_IN]->runtime)
		goto no_proc;

	runtime = g_call_substream[SNDP_PCM_IN]->runtime;

	if (NULL == runtime->dma_area)
		goto no_proc;

	/* Get data, If Buffer >= Data */
	if ((g_call_dummy_record_len + VCD_RECORD_BUFFER_SIZE) <=
		(pcm_info->buffer_len - pcm_info->byte_offset)) {
		/* Copy the data from temporary area. */
		if (0 < g_call_dummy_record_len) {
			/* Get 0 padding data */
			memset((runtime->dma_area + pcm_info->byte_offset),
			       0,
			       g_call_dummy_record_len);

		}

		/* Get 0 padding data */
		memset((void *)(runtime->dma_area +
				pcm_info->byte_offset +
				g_call_dummy_record_len),
		       0,
		       VCD_RECORD_BUFFER_SIZE);

		/* Offset update */
		pcm_info->byte_offset +=
			(g_call_dummy_record_len + VCD_RECORD_BUFFER_SIZE);

		/* Clear temporary information. */
		g_call_dummy_record_len = 0;

	/* Get data, If Buffer < Data */
	} else {
		/* Get 0 padding data */
		memset((void *)(runtime->dma_area + pcm_info->byte_offset),
		       0,
		       pcm_info->buffer_len - pcm_info->byte_offset);

		/* Length of the temporary area to update information. */
		g_call_dummy_record_len = VCD_RECORD_BUFFER_SIZE -
			(pcm_info->buffer_len - pcm_info->byte_offset);

		/* Offset update */
		pcm_info->byte_offset +=
			(pcm_info->buffer_len - pcm_info->byte_offset);
	}

	/* If it reaches the period of data */
	if ((0 < g_call_dummy_record_len) ||
	    (pcm_info->byte_offset >=
		(pcm_info->period_len * (pcm_info->period + 1)))) {

		/* Period update */
		pcm_info->period =
			pcm_info->byte_offset / pcm_info->period_len;

		if (runtime->periods == pcm_info->period) {
			/* Update again, the period */
			pcm_info->period = 0;
			pcm_info->byte_offset = 0;
		}

		/* Notification period to ALSA */
		snd_pcm_period_elapsed(g_call_substream[SNDP_PCM_IN]);
	}

	/* Work queue, Queuing again */
	if (g_call_dummy_rec)
		sndp_workqueue_enqueue(g_call_queue_in, &g_call_work_in);

no_proc:
	up(&g_sndp_wait_free[SNDP_PCM_IN]);

	/* sndp_log_debug_func("end\n"); */
}


/**
 * @brief wait event for play proc.
 *
 * @param[in]   *work   work struct.
 *
 * @retval none.
 */
static bool call_work_play_wait_event(struct sndp_work_info *work)
{
	long wait_ret;

	if (NULL != work) {
		wait_ret = wait_event_interruptible_timeout(
			g_call_wait_out,
			(CALL_DUMMY_PLAY_YET == g_call_dummy_play),
			msecs_to_jiffies(CALL_WAIT_TIME));

		/* Status check */
		if ((CALL_DUMMY_PLAY_YET == g_call_dummy_play) || (0 != wait_ret)) {
			sndp_log_info("status %d\n", g_call_dummy_play);
			return true;
		}
	}

	return false;
}

/**
 * @brief before start fw proc.
 *
 * @param[in]   *work   work struct.
 *
 * @retval none.
 */
static void call_work_before_start_fw(struct sndp_work_info *work)
{
	int			ret;
	struct call_pcm_info	*pcm_info = &g_call_pcm_info[SNDP_PCM_OUT];

	/* sndp_log_debug_func("start\n"); */

	if (call_work_play_wait_event(work))
		return;

	if (atomic_read(&g_call_watch_start_fw)) {
		g_call_dummy_play = CALL_DUMMY_PLAY_YET;
		g_status |= PLAY_STATUS;
		/* Play data set */
		if (SNDP_PT_NOT_STARTED == g_pt_start)
			call_playback_data_set();
		else
			call_playback_data_set_for_pt();

		/* Start speech playback */
		sndp_log_info(
			"vcd_execute() cmd=VCD_COMMAND_START_PLAYBACK\n");

		ret = call_vcd_execute((int)VCD_COMMAND_START_PLAYBACK,
				       (void *)&g_vcd_ply_opt);
		if (ERROR_NONE != ret) {
			sndp_log_debug("path switch\n");
			/* Status update */
			g_status &= ~PLAY_STATUS;
			sndp_call_playback_normal();
		}

		return;
	}

	call_work_dummy_play(NULL);

	/* Set next position */
	pcm_info->next_pd_side = (DATA_SIDE_0 == pcm_info->next_pd_side) ?
						DATA_SIDE_1 : DATA_SIDE_0;

	if (CALL_DUMMY_PLAY_FW == g_call_dummy_play) {
		sndp_workqueue_enqueue(g_call_queue_out,
					&g_call_work_before_start_fw);
	}

	/* sndp_log_debug_func("end\n"); */
}

/**
 * @brief queue out proc.
 *
 * @param[in]   *work   work struct.
 *
 * @retval none.
 */
static void call_work_dummy_play(struct sndp_work_info *work)
{
	long call_playback_len = 0;
	struct call_pcm_info	*pcm_info = &g_call_pcm_info[SNDP_PCM_OUT];
	struct snd_pcm_runtime	*runtime;
	int			ret;

	/* sndp_log_debug_func("start\n"); */

	if (call_work_play_wait_event(work))
		return;

	ret = down_interruptible(&g_sndp_wait_free[SNDP_PCM_OUT]);
	if (0 != ret)
		sndp_log_err("down_interruptible ret[%d]\n", ret);

	if (NULL == g_call_substream[SNDP_PCM_OUT]->runtime)
		goto no_proc;

	runtime = g_call_substream[SNDP_PCM_OUT]->runtime;

	if (NULL == runtime->dma_area)
		goto no_proc;

	if (0 < g_call_dummy_play_len) {
		pcm_info->byte_offset = g_call_dummy_play_len;
		call_playback_len = g_call_dummy_play_len;
		g_call_dummy_play_len = 0;
	}

	if ((VCD_PLAYBACK_BUFFER_SIZE - call_playback_len) <=
		(pcm_info->buffer_len - pcm_info->byte_offset)) {
		pcm_info->byte_offset += VCD_PLAYBACK_BUFFER_SIZE;

		if (VCD_PLAYBACK_BUFFER_SIZE >
			(pcm_info->buffer_len - pcm_info->byte_offset)) {
			g_call_playback_len =
				pcm_info->buffer_len - pcm_info->byte_offset;

			pcm_info->byte_offset += g_call_playback_len;
		}
	} else {
		pcm_info->byte_offset +=
			(pcm_info->buffer_len - pcm_info->byte_offset);
	}

	if ((0 < call_playback_len) ||
		(pcm_info->byte_offset >=
			(pcm_info->period_len * (pcm_info->period + 1)))) {

		pcm_info->period =
			pcm_info->byte_offset / pcm_info->period_len;
		if (runtime->periods == pcm_info->period) {
			pcm_info->period = 0;
			pcm_info->byte_offset = 0;
		}
		snd_pcm_period_elapsed(g_call_substream[SNDP_PCM_OUT]);
	}

	if (CALL_DUMMY_PLAY == g_call_dummy_play) {
		sndp_workqueue_enqueue(g_call_queue_out, &g_call_work_out);
	}

no_proc:
	up(&g_sndp_wait_free[SNDP_PCM_OUT]);
	/* sndp_log_debug_func("end\n"); */
}


/**
   @brief Dummy playback incommunication Data set function

   @param[in]	buffer size
   @param[out]	none

   @retval	none
 */
static void call_playback_incomm_dummy(unsigned int buf_size)
{
	struct call_pcm_info	*pcm_info = &g_call_incomm_pcm_info[SNDP_PCM_OUT];

	/* Set next position */
	pcm_info->next_pd_side = (DATA_SIDE_0 == pcm_info->next_pd_side) ?
						DATA_SIDE_1 : DATA_SIDE_0;

//	sndp_log_debug_func("end\n");
}


/*!
   @brief Dummy record incommunication Data set function

   @param[in]	buffer size
   @param[out]	none

   @retval	none
 */
static void call_record_incomm_dummy(unsigned int buf_size)
{
	struct call_pcm_info	*pcm_info = &g_call_incomm_pcm_info[SNDP_PCM_IN];

/*	sndp_log_debug_func("start\n");*/

	/* Set next position */
	pcm_info->next_pd_side = (DATA_SIDE_0 == pcm_info->next_pd_side) ?
						DATA_SIDE_1 : DATA_SIDE_0;

	/* sndp_log_debug_func("end\n"); */
}


/*!
   @brief Void get buffer function

   @param[in]	none
   @param[out]	none

   @retval	0		Successful
   @retval	-EPERM		Not ready
 */
void call_get_incomm_buffer(void)
{
	/* Local variable declaration */
	int                             ret = ERROR_NONE;
	struct vcd_voip_ul_buffer_info	vcd_voip_ul_buf;
	struct vcd_voip_dl_buffer_info	vcd_voip_dl_buf;

	sndp_log_debug_func("start\n");

	/* Get Voip UL buffer */
	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_GET_VOIP_UL_BUFFER\n");
	ret = call_vcd_execute((int)VCD_COMMAND_GET_VOIP_UL_BUFFER,
				(void *)&vcd_voip_ul_buf);
	if (ERROR_NONE != ret)
		sndp_log_err("Error COMMAND_GET_VOIP_UL_BUFFER(code=%d)\n",
			     ret);

	/* Data pointer setting */
	g_call_rec_incomm_data_addr[DATA_SIDE_0] =
		(u_int)vcd_voip_ul_buf.voip_ul_buffer[DATA_SIDE_0];
	g_call_rec_incomm_data_addr[DATA_SIDE_1] =
		(u_int)vcd_voip_ul_buf.voip_ul_buffer[DATA_SIDE_1];

	/* Get Voip DL buffer */
	sndp_log_info("vcd_execute() cmd=VCD_COMMAND_GET_VOIP_DL_BUFFER\n");
	ret = call_vcd_execute((int)VCD_COMMAND_GET_VOIP_DL_BUFFER,
				(void *)&vcd_voip_dl_buf);
	if (ERROR_NONE != ret)
		sndp_log_err("Error COMMAND_GET_VOIP_DL_BUFFER(code=%d)\n",
			     ret);

	/* Data pointer setting */
	g_call_play_incomm_data_addr[DATA_SIDE_0] =
		(u_int)vcd_voip_dl_buf.voip_dl_buffer[DATA_SIDE_0];
	g_call_play_incomm_data_addr[DATA_SIDE_1] =
		(u_int)vcd_voip_dl_buf.voip_dl_buffer[DATA_SIDE_1];

	sndp_log_debug_func("end\n");
}


/*!
   @brief PCM information initialization function

   @param[in]	substream	PCM substream structure
   @param[in]	kind		Direction(playback/capture)
   @param[out]	none

   @retval	none
 */
static void call_incomm_pcm_info_init(
	struct snd_pcm_substream *substream,
	enum call_status kind)
{
	/* Local variable declaration */
	int			direction;
	struct snd_pcm_runtime	*runtime = substream->runtime;

	sndp_log_debug_func("start\n");

	direction = (PLAY_INCOMM_STATUS == kind) ? SNDP_PCM_OUT : SNDP_PCM_IN;

	/* Set substream */
	g_call_incomm_substream[direction] = substream;

	/* Pcm information clear */
	g_call_incomm_pcm_info[direction].buffer_len =
		runtime->buffer_size * runtime->frame_bits / 8;

	g_call_incomm_pcm_info[direction].byte_offset = 0;

	g_call_incomm_pcm_info[direction].period_len =
		frames_to_bytes(runtime, runtime->period_size);

	g_call_incomm_pcm_info[direction].period = 0;

	sndp_log_debug_func("end\n");
}

/*!
   @brief VoIP buffer offset return function

   @param[in]	none
   @param[out]	none

   @retval	PCM data buffer offset
 */
snd_pcm_uframes_t call_incomm_pcmdata_pointer(struct snd_pcm_substream *substream)
{
	long location = g_call_incomm_pcm_info[substream->stream].byte_offset;

#ifdef DEBUG
	/* If offset < 16Byte, Reduce the amount of log. */
	if ((g_call_incomm_pcm_info[substream->stream].buffer_len - 16) <=
		location) {
		sndp_log_info("%ld frames (%ld bytes)\n",
		bytes_to_frames(substream->runtime, location),
		location);
	}
#endif
	return bytes_to_frames(substream->runtime, location);
}

/*!
   @brief Change dummy playing for incommunication

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void call_change_incomm_play(void)
{
	sndp_log_debug_func("start\n");

	sndp_log_info("src [%d]\n", g_call_sampling_rate[SNDP_PCM_OUT]);
	if (!(SNDP_ROUTE_PLAY_INCOMM_DUMMY & g_sndp_stream_route)) {
		g_sndp_stream_route |= SNDP_ROUTE_PLAY_INCOMM_DUMMY;
		sndp_workqueue_enqueue(g_call_queue_out,
				&g_call_work_playback_incomm_dummy_set);
	}

	sndp_log_debug_func("end\n");
}

/*!
   @brief Dummy playing for incommunication

   @param[in]   *work   work struct.

   @retval	none
 */
static void call_work_playback_incomm_dummy_set(struct sndp_work_info *work)
{
	/* Local variable declaration */
	long			call_playback_len = 0;
	struct call_pcm_info
			*pcm_info = &g_call_incomm_pcm_info[SNDP_PCM_OUT];
	struct snd_pcm_runtime	*runtime;
	u_int			buf_size;
	long wait_ret;
	int			ret;

	ret = down_interruptible(&g_sndp_wait_free[SNDP_PCM_OUT]);
	if (0 != ret)
		sndp_log_err("down_interruptible ret[%d]\n", ret);

	/* If process had been driver closed. */
	if (NULL == g_call_incomm_substream[SNDP_PCM_OUT])
		goto no_proc;
	if (NULL == g_call_incomm_substream[SNDP_PCM_OUT]->runtime)
		goto no_proc;

	runtime = g_call_incomm_substream[SNDP_PCM_OUT]->runtime;

	if (NULL == runtime->dma_area)
		goto no_proc;

	if (g_call_incomm_cb[SNDP_PCM_OUT])
		buf_size = g_call_sampling_rate[SNDP_PCM_OUT] * 2 / 50;
	else
		buf_size = pcm_info->save_buf_size;

	if (0 < g_call_playback_incomm_len) {
		/* Clear temporary area. */
		pcm_info->period = 0;
		call_playback_len = g_call_playback_incomm_len;
		g_call_playback_incomm_len = 0;
	}

	/* If Buffer <= Data */
	if ((buf_size - call_playback_len) <=
		(pcm_info->buffer_len - pcm_info->byte_offset)) {
		/* Offset update */
		pcm_info->byte_offset += buf_size - call_playback_len;

		/* If Buffer > Data */
		if (buf_size >
			(pcm_info->buffer_len - pcm_info->byte_offset)) {
			/*
			 * Length of the temporary area
			 * to update information.
			 */
			g_call_playback_incomm_len =
				pcm_info->buffer_len - pcm_info->byte_offset;
			/* Offset update */
			pcm_info->byte_offset += g_call_playback_incomm_len;
		}
	/* Set data, If Buffer > Data */
	} else {
		/* Offset update */
		pcm_info->byte_offset +=
			(pcm_info->buffer_len - pcm_info->byte_offset);
	}

	/* If it reaches the period of data */
	if ((0 < call_playback_len) ||
	    (pcm_info->byte_offset >=
		(pcm_info->period_len * (pcm_info->period + 1)))) {

		/* Period update */
		pcm_info->period =
			pcm_info->byte_offset / pcm_info->period_len;

		if (runtime->periods == pcm_info->period) {
			/* Update again, the period */
			pcm_info->period = 0;
			pcm_info->byte_offset = 0;
		}

		/* Notification period to ALSA */
		snd_pcm_period_elapsed(g_call_incomm_substream[SNDP_PCM_OUT]);
	}

no_proc:
	up(&g_sndp_wait_free[SNDP_PCM_OUT]);

	wait_ret = wait_event_interruptible_timeout(
		g_call_wait_out,
		!(SNDP_ROUTE_PLAY_INCOMM_DUMMY & g_sndp_stream_route),
		msecs_to_jiffies(CALL_WAIT_TIME));

	/* Status check */
	if (!(PLAY_INCOMM_STATUS & g_status) || (0 != wait_ret)) {
		sndp_log_info("status 0x%02x  route 0x%02x\n",
					g_status, g_sndp_stream_route);
		return;
	}

	sndp_workqueue_enqueue(g_call_queue_out,
				&g_call_work_playback_incomm_dummy_set);
}

/*!
   @brief Change dummy recording for incommunication

   @param[in]	none
   @param[out]	none

   @retval	none
 */
void call_change_incomm_rec(void)
{
	sndp_log_debug_func("start\n");

	sndp_log_info("src [%d]\n", g_call_sampling_rate[SNDP_PCM_IN]);
	if (!(SNDP_ROUTE_CAP_INCOMM_DUMMY & g_sndp_stream_route)) {
		g_sndp_stream_route |= SNDP_ROUTE_CAP_INCOMM_DUMMY;
		sndp_workqueue_enqueue(g_call_queue_out,
					&g_call_work_record_incomm_dummy_set);
	}

	sndp_log_debug_func("end\n");
}

/*!
   @brief Dummy recording for incommunication

   @param[in]   *work   work struct.

   @retval	none
 */
static void call_work_record_incomm_dummy_set(struct sndp_work_info *work)
{
	/* Local variable declaration */
	struct call_pcm_info	*pcm_info =
					&g_call_incomm_pcm_info[SNDP_PCM_IN];
	struct snd_pcm_runtime	*runtime;
	u_int			buf_size;
	long wait_ret;
	int			ret;

	ret = down_interruptible(&g_sndp_wait_free[SNDP_PCM_IN]);
	if (0 != ret)
		sndp_log_err("down_interruptible ret[%d]\n", ret);

	/* If process had been driver closed. */
	if (NULL == g_call_incomm_substream[SNDP_PCM_IN])
		goto no_proc;
	if (NULL == g_call_incomm_substream[SNDP_PCM_IN]->runtime)
		goto no_proc;

	runtime = g_call_incomm_substream[SNDP_PCM_IN]->runtime;

	if (NULL == runtime->dma_area)
		goto no_proc;

	if (g_call_incomm_cb[SNDP_PCM_IN])
		buf_size = g_call_sampling_rate[SNDP_PCM_IN] * 2 / 50;
	else
		buf_size = pcm_info->save_buf_size;

	if ((buf_size / 2) <=
		(runtime->status->hw_ptr - runtime->control->appl_ptr))
		goto no_proc;

	/* If Buffer >= Data */
	if ((g_call_record_incomm_len + buf_size) <=
		(pcm_info->buffer_len - pcm_info->byte_offset)) {
		if (0 < g_call_record_incomm_len) {
			memset((runtime->dma_area + pcm_info->byte_offset),
				0, g_call_record_incomm_len);
			g_call_record_incomm_len = 0;
		}

		memset((void *)(runtime->dma_area +
				pcm_info->byte_offset +
				g_call_record_incomm_len), 0, buf_size);
		/* Offset update */
		pcm_info->byte_offset += (g_call_record_incomm_len + buf_size);
	/* If Buffer < Data */
	} else {
		memset((void *)(runtime->dma_area + pcm_info->byte_offset),
			0, pcm_info->buffer_len - pcm_info->byte_offset);
		/* Offset update */
		pcm_info->byte_offset +=
			(pcm_info->buffer_len - pcm_info->byte_offset);

		/* Length of the temporary area to update information. */
		g_call_record_incomm_len = buf_size -
			(pcm_info->buffer_len - pcm_info->byte_offset);
	}

	/* If it reaches the period of data */
	if ((0 < g_call_record_incomm_len) ||
	    (pcm_info->byte_offset >=
		(pcm_info->period_len * (pcm_info->period + 1)))) {

		/* Period update */
		pcm_info->period =
			pcm_info->byte_offset / pcm_info->period_len;

		if (runtime->periods == pcm_info->period) {
			/* Update again, the period */
			pcm_info->period = 0;
			pcm_info->byte_offset = 0;
		}

		/* Notification period to ALSA */
		snd_pcm_period_elapsed(g_call_incomm_substream[SNDP_PCM_IN]);
	}

no_proc:
	up(&g_sndp_wait_free[SNDP_PCM_IN]);

	wait_ret = wait_event_interruptible_timeout(
		g_call_wait_in,
		!(SNDP_ROUTE_CAP_INCOMM_DUMMY & g_sndp_stream_route),
		msecs_to_jiffies(CALL_WAIT_TIME));

	/* Status check */
	if (!(REC_INCOMM_STATUS & g_status) || (0 != wait_ret)) {
		sndp_log_info("status 0x%02x  route 0x%02x\n",
					g_status, g_sndp_stream_route);
		return;
	}

	sndp_workqueue_enqueue(g_call_queue_in,
				&g_call_work_record_incomm_dummy_set);
}
