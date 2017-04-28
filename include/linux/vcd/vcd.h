/* vcd.h
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
#ifndef __VCD_H__
#define __VCD_H__

#ifndef __KERNEL__
#else	/* __KERNEL__ */

/*
 * constant definition
 */
#define VCD_RECORD_BUFFER_SIZE		640
#define VCD_PLAYBACK_BUFFER_SIZE	640
#define VCD_PLAYBACK_PT_BUFFER_SIZE	1920

/*
 * define macro declaration
 */


/*
 * enum declaration
 */
enum VCD_COMMAND {
	VCD_COMMAND_SET_CALL_MODE = 0,
	VCD_COMMAND_START_RECORD,
	VCD_COMMAND_STOP_RECORD,
	VCD_COMMAND_START_PLAYBACK,
	VCD_COMMAND_STOP_PLAYBACK,
	VCD_COMMAND_GET_RECORD_BUFFER,
	VCD_COMMAND_GET_PLAYBACK_BUFFER,
	VCD_COMMAND_WATCH_FW,
	VCD_COMMAND_WATCH_CLKGEN,
	VCD_COMMAND_WATCH_CODEC_TYPE,
	VCD_COMMAND_WAIT_PATH,
	VCD_COMMAND_GET_VOIP_UL_BUFFER,
	VCD_COMMAND_GET_VOIP_DL_BUFFER,
	VCD_COMMAND_SET_VOIP_CALLBACK,

	VCD_COMMAND_MAX
};

enum VCD_CALL_KIND {
	VCD_CALL_KIND_CALL = 0,
	VCD_CALL_KIND_1KHZ,
	VCD_CALL_KIND_PCM_LB,
	VCD_CALL_KIND_VIF_LB
};

enum VCD_RECORD_MODE {
	VCD_RECORD_MODE_0 = 0,
	VCD_RECORD_MODE_1,
	VCD_RECORD_MODE_2
};

enum VCD_PLAYBACK_MODE {
	VCD_PLAYBACK_MODE_0 = 0,
	VCD_PLAYBACK_MODE_1,
	VCD_PLAYBACK_MODE_2
};

enum VCD_LOOPBACK_MODE {
	VCD_LOOPBACK_MODE_INTERFACE = 0,
	VCD_LOOPBACK_MODE_PCM,
	VCD_LOOPBACK_MODE_DELAY
};

enum VCD_CODEC_TYPE {
	VCD_CODEC_WB = 0,
	VCD_CODEC_NB
};
/*
 * structure declaration
 */
struct vcd_execute_command {
	unsigned int command;
	void *arg;
};

struct vcd_call_option {
	unsigned int call_kind;
	unsigned int loopback_mode;
};

struct vcd_record_option {
	unsigned int mode;
	void *complete_buffer;
};

struct vcd_playback_option {
	unsigned int mode;
	void *beginning_buffer;
};

struct vcd_record_buffer_info {
	unsigned int *record_buffer[2];
};

struct vcd_playback_buffer_info {
	unsigned int *playback_buffer[2];
};

struct vcd_watch_fw_info {
	void *start_fw;
	void *stop_fw;
};

struct vcd_watch_clkgen_info {
	void *start_clkgen;
	void *stop_clkgen;
};

typedef void (*codec_type_callback)(unsigned int);
struct vcd_watch_codec_type_info {
	codec_type_callback codec_type_ind;
};

struct vcd_wait_path_info {
	void *wait_path;
};

struct vcd_voip_ul_buffer_info {
	unsigned int *voip_ul_buffer[2];
};

struct vcd_voip_dl_buffer_info {
	unsigned int *voip_dl_buffer[2];
};

struct vcd_voip_callback {
	void *voip_ul_callback;
	void *voip_dl_callback;
};

typedef void (*vcd_voip_callback) (unsigned int buf_size);

/*
 * table declaration
 */


/*
 * prototype declaration
 */

/* for sound */
extern int vcd_execute(const struct vcd_execute_command *args);

/* for audio ic */
extern int vcd_execute_test_call(const struct vcd_execute_command *args);



#endif	/* __KERNEL__ */
#endif	/* __VCD_H__ */
