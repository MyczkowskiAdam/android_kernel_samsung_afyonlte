/* vcd_ctrl_func.h
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
#ifndef __VCD_CTRL_FUNC_H__
#define __VCD_CTRL_FUNC_H__

/*
 * constant definition
 */


/*
 * define macro declaration
 */


/*
 * enum declaration
 */
enum VCD_CTRL_FUNC_FEATURE {
	VCD_CTRL_FUNC_FEATURE_NONE			= 0x00000000,
	VCD_CTRL_FUNC_FEATURE_SET_BINARY_SPUV		= 0x00000001,
	VCD_CTRL_FUNC_FEATURE_SET_BINARY_PCM		= 0x00000002,
	VCD_CTRL_FUNC_FEATURE_SET_BINARY_DIAMOND	= 0x00000004,
	VCD_CTRL_FUNC_FEATURE_VCD			= 0x00000008,
	VCD_CTRL_FUNC_FEATURE_HW_PARAM			= 0x00000010,
	VCD_CTRL_FUNC_FEATURE_CALL			= 0x00000020,
	VCD_CTRL_FUNC_FEATURE_RECORD			= 0x00000040,
	VCD_CTRL_FUNC_FEATURE_PLAYBACK			= 0x00000080,
	VCD_CTRL_FUNC_FEATURE_STORED_PLAYBACK		= 0x20000000,
	VCD_CTRL_FUNC_FEATURE_AMHAL_STOP		= 0x40000000,
	VCD_CTRL_FUNC_FEATURE_ERROR			= 0x80000000
};

enum VCD_CTRL_FUNC_COMMAND {
	VCD_CTRL_FUNC_SET_BINARY_SPUV = 0,
	VCD_CTRL_FUNC_SET_BINARY_PCM,
	VCD_CTRL_FUNC_SET_BINARY_DIAMOND,
	VCD_CTRL_FUNC_GET_MSG_BUFFER,
	VCD_CTRL_FUNC_START_VCD,
	VCD_CTRL_FUNC_STOP_VCD,
	VCD_CTRL_FUNC_HW_PARAM,
	VCD_CTRL_FUNC_START_CALL,
	VCD_CTRL_FUNC_STOP_CALL,
	VCD_CTRL_FUNC_SET_UDATA,
	VCD_CTRL_FUNC_START_RECORD,
	VCD_CTRL_FUNC_STOP_RECORD,
	VCD_CTRL_FUNC_START_PLAYBACK,
	VCD_CTRL_FUNC_STOP_PLAYBACK,
	VCD_CTRL_FUNC_GET_RECORD_BUFFER,
	VCD_CTRL_FUNC_GET_PLAYBACK_BUFFER,
	VCD_CTRL_FUNC_WATCH_STOP_FW,
};


/*
 * structure declaration
 */


/*
 * table declaration
 */


/*
 * prototype declaration
 */
extern void vcd_ctrl_func_initialize(void);
extern int vcd_ctrl_func_check_sequence(unsigned int command);
extern int vcd_ctrl_func_convert_result(int result);
extern int vcd_ctrl_func_check_stop_vcd_need(void);
extern unsigned int vcd_ctrl_func_get_active_feature(void);
extern void vcd_ctrl_func_set_active_feature(unsigned int feature);
extern void vcd_ctrl_func_unset_active_feature(unsigned int feature);


#endif /* __VCD_CTRL_FUNC_H__ */
