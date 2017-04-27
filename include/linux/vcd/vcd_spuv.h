/* vcd_spuv.h
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
#ifndef __VCD_SPUV_H__
#define __VCD_SPUV_H__

/*
 * constant definition
 */


/*
 * define macro declaration
 */


/*
 * enum declaration
 */


/*
 * structure declaration
 */


/*
 * table declaration
 */


/*
 * prototype declaration
 */
extern int vcd_spuv_ioremap(void);
extern void vcd_spuv_iounmap(void);
extern void vcd_spuv_init_register(void);
extern void vcd_spuv_ipc_semaphore_init(void);
extern int vcd_spuv_get_fw_buffer(void);
extern void vcd_spuv_free_fw_buffer(void);
extern int vcd_spuv_get_binary_buffer(void);
extern int vcd_spuv_set_binary_preprocessing(char *file_path);
extern int vcd_spuv_set_binary_main(unsigned int write_size);
extern int vcd_spuv_set_binary_postprocessing(void);
extern int vcd_spuv_get_msg_buffer(void);
extern int vcd_spuv_get_async_area(void);
extern int vcd_spuv_free_async_area(unsigned int adr);
extern int vcd_spuv_start_vcd(void);
extern int vcd_spuv_stop_vcd(void);
extern int vcd_spuv_set_hw_param(void);
extern int vcd_spuv_start_call(void);
extern int vcd_spuv_stop_call(void);
extern int vcd_spuv_set_udata(void);
extern int vcd_spuv_start_record(struct vcd_record_option *option);
extern int vcd_spuv_stop_record(void);
extern int vcd_spuv_start_playback(struct vcd_playback_option *option,
						unsigned int call_kind);
extern int vcd_spuv_stop_playback(void);
extern void vcd_spuv_get_record_buffer(struct vcd_record_buffer_info *info);
extern void vcd_spuv_get_playback_buffer(struct vcd_playback_buffer_info *info,
						unsigned int call_kind);
extern void vcd_spuv_get_voip_ul_buffer(struct vcd_voip_ul_buffer_info *info);
extern void vcd_spuv_get_voip_dl_buffer(struct vcd_voip_dl_buffer_info *info);
extern void vcd_spuv_init_record_buffer_id(void);
extern void vcd_spuv_init_playback_buffer_id(void);
extern int vcd_spuv_get_call_type(void);
extern void vcd_spuv_voip_ul(unsigned int *buf_size);
extern void vcd_spuv_voip_dl(unsigned int *buf_size);
extern void vcd_spuv_update_voip_ul_buffer_id(void);
extern void vcd_spuv_update_voip_dl_buffer_id(void);
extern int vcd_spuv_resampler_init(void);
extern int vcd_spuv_resampler_close(void);
extern void vcd_spuv_pt_playback(void);

extern int vcd_spuv_start_1khz_tone(void);
extern int vcd_spuv_stop_1khz_tone(void);
extern int vcd_spuv_start_pcm_loopback(int mode);
extern int vcd_spuv_stop_pcm_loopback(void);
extern int vcd_spuv_start_bbif_loopback(int mode);
extern int vcd_spuv_stop_bbif_loopback(void);
extern int vcd_spuv_set_trace_select(void);

extern int vcd_spuv_create_queue(void);
extern void vcd_spuv_destroy_queue(void);

extern void vcd_spuv_dump_status(void);
extern void vcd_spuv_dump_registers(void);
extern void vcd_spuv_dump_hpb_registers(void);
extern void vcd_spuv_dump_cpg_registers(void);
extern void vcd_spuv_dump_crmu_registers(void);
extern void vcd_spuv_dump_gtu_registers(void);
extern void vcd_spuv_dump_voiceif_registers(void);
extern void vcd_spuv_dump_intcvo_registers(void);
extern void vcd_spuv_dump_spuv_registers(void);
extern void vcd_spuv_dump_dsp0_registers(void);
extern void vcd_spuv_dump_memories(void);
extern void vcd_spuv_dump_pram0_memory(void);
extern void vcd_spuv_dump_xram0_memory(void);
extern void vcd_spuv_dump_yram0_memory(void);
extern void vcd_spuv_dump_dspio_memory(void);
extern void vcd_spuv_dump_sdram_static_area_memory(void);
extern void vcd_spuv_dump_fw_static_buffer_memory(void);
extern void vcd_spuv_dump_spuv_crashlog(void);
extern void vcd_spuv_dump_diamond_memory(void);

extern void vcd_spuv_calc_trigger_start(void);
extern void vcd_spuv_calc_trigger_stop(void);
extern void vcd_spuv_trigger_count_log(unsigned int type);

#endif /* __VCD_SPUV_H__ */
