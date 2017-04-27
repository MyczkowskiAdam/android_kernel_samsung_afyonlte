/* vcd_ctrl.h
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
#ifndef __VCD_CTRL_H__
#define __VCD_CTRL_H__

/*
 * constant definition
 */

/* vcd status */
#define VCD_CTRL_STATUS_STANDBY			\
			VCD_CTRL_STATUS_NOT_START
#define VCD_CTRL_STATUS_ACTIVE			\
			VCD_CTRL_STATUS_STARTED_DSP
#define VCD_CTRL_STATUS_READY			\
			VCD_CTRL_STATUS_STARTED_DSP
#define VCD_CTRL_STATUS_RECORD			\
			VCD_CTRL_STATUS_STARTED_RECORD_PLAYBACK
#define VCD_CTRL_STATUS_PLAYBACK		\
			VCD_CTRL_STATUS_STARTED_RECORD_PLAYBACK
#define VCD_CTRL_STATUS_RECORD_PLAYBACK		\
			VCD_CTRL_STATUS_STARTED_RECORD_PLAYBACK
#define VCD_CTRL_STATUS_CALL			\
			VCD_CTRL_STATUS_STARTED_CALL
#define VCD_CTRL_STATUS_CALL_RECORD		\
			VCD_CTRL_STATUS_STARTED_CALL_RECORD_PLAYBACK
#define VCD_CTRL_STATUS_CALL_PLAYBACK		\
			VCD_CTRL_STATUS_STARTED_CALL_RECORD_PLAYBACK
#define VCD_CTRL_STATUS_CALL_RECORD_PLAYBACK	\
			VCD_CTRL_STATUS_STARTED_CALL_RECORD_PLAYBACK
#define VCD_CTRL_STATUS_ERROR			\
			VCD_CTRL_STATUS_ERROR_OCCURS


/* stored playback timeout value */
#define VCD_CTRL_STORED_PLAYBACK_TIMER	(1000 * HZ / 1000)

/*
 * define macro declaration
 */


/*
 * enum declaration
 */
enum VCD_CTRL_STATUS {
	VCD_CTRL_STATUS_ERROR_OCCURS = -1,
	VCD_CTRL_STATUS_NOT_START = 0,
	VCD_CTRL_STATUS_STARTED_DSP,
	VCD_CTRL_STATUS_STARTED_RECORD_PLAYBACK,
	VCD_CTRL_STATUS_STARTED_CALL,
	VCD_CTRL_STATUS_STARTED_CALL_RECORD_PLAYBACK,
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

/* For AMHAL functions */
int vcd_ctrl_get_binary_buffer(void);
int vcd_ctrl_set_binary_preprocessing(char *file_path);
int vcd_ctrl_set_binary_main(unsigned int write_size);
int vcd_ctrl_set_binary_postprocessing(void);
int vcd_ctrl_get_msg_buffer(void);
int vcd_ctrl_get_async_area(void);
int vcd_ctrl_free_async_area(unsigned int adr);
int vcd_ctrl_start_vcd(void);
int vcd_ctrl_stop_vcd(void);
int vcd_ctrl_set_hw_param(void);
int vcd_ctrl_start_call(int call_type, int mode);
int vcd_ctrl_stop_call(int call_type);
int vcd_ctrl_set_udata(void);
void vcd_ctrl_get_status(void);
int vcd_ctrl_get_result(void);
int vcd_ctrl_check_semantics(void);

/* For Sound driver functions */
int vcd_ctrl_start_record(struct vcd_record_option *option);
int vcd_ctrl_stop_record(void);
int vcd_ctrl_start_playback(struct vcd_playback_option *option);
int vcd_ctrl_stop_playback(void);
void vcd_ctrl_get_record_buffer(struct vcd_record_buffer_info *info);
void vcd_ctrl_get_playback_buffer(struct vcd_playback_buffer_info *info);
void vcd_ctrl_get_voip_ul_buffer(struct vcd_voip_ul_buffer_info *info);
void vcd_ctrl_get_voip_dl_buffer(struct vcd_voip_dl_buffer_info *info);

/* For spuv functions */
void vcd_ctrl_rec_trigger(void);
void vcd_ctrl_play_trigger(void);
void vcd_ctrl_codec_type_ind(unsigned int codec_type);
void vcd_ctrl_stop_fw(int result);
void vcd_ctrl_udata_ind(void);
void vcd_ctrl_start_clkgen(void);
void vcd_ctrl_stop_clkgen(void);
void vcd_ctrl_wait_path(void);
void vcd_ctrl_get_semaphore(void);
void vcd_ctrl_release_semaphore(void);

/* Driver functions */
int vcd_ctrl_suspend(void);
int vcd_ctrl_resume(void);
int vcd_ctrl_runtime_suspend(void);
int vcd_ctrl_runtime_resume(void);
int vcd_ctrl_probe(void);
int vcd_ctrl_remove(void);

/* Dump functions */
void vcd_ctrl_dump_status(void);
void vcd_ctrl_dump_registers(void);
void vcd_ctrl_dump_hpb_registers(void);
void vcd_ctrl_dump_cpg_registers(void);
void vcd_ctrl_dump_crmu_registers(void);
void vcd_ctrl_dump_gtu_registers(void);
void vcd_ctrl_dump_voiceif_registers(void);
void vcd_ctrl_dump_intcvo_registers(void);
void vcd_ctrl_dump_spuv_registers(void);
void vcd_ctrl_dump_dsp0_registers(void);
void vcd_ctrl_dump_memories(void);
void vcd_ctrl_dump_pram0_memory(void);
void vcd_ctrl_dump_xram0_memory(void);
void vcd_ctrl_dump_yram0_memory(void);
void vcd_ctrl_dump_dspio_memory(void);
void vcd_ctrl_dump_sdram_static_area_memory(void);
void vcd_ctrl_dump_fw_static_buffer_memory(void);
void vcd_ctrl_dump_spuv_crashlog(void);
void vcd_ctrl_dump_diamond_memory(void);

/* Debug functions */
void vcd_ctrl_calc_trigger_start(void);
void vcd_ctrl_calc_trigger_stop(void);

/* Internal functions */
static int vcd_ctrl_error_stop_vcd(void);
static void vcd_ctrl_start_stored_playback_timer(void);
static void vcd_ctrl_stop_stored_playback_timer(void);
static void vcd_ctrl_stored_playback_timer_cb(void);

#endif /* __VCD_CTRL_H__ */
