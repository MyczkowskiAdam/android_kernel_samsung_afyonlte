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
#ifndef __VCD_IF_H__
#define __VCD_IF_H__

/*
 * constant definition
 */
#define VCD_PROC_FILE_NAME_EXEC_PROC	"exec_process"
#define VCD_PROC_FILE_NAME_LOG_LEVEL	"log_level"
#define VCD_PROC_FILE_NAME_EXEC_FUNC	"exec_func"

#define VCD_PROC_BUF_SIZE		32

#define VCD_POLL_READ_OK		1
#define VCD_POLL_READ_NG		0

#define VCD_IF_GET_MSG_BUFFER_LOG	"[ <- AMHAL] GET_MSG_BUFFER\n"
#define VCD_IF_SET_BINARY_BUF_LOG	"[ <- AMHAL] SET_BINARY(BUF)\n"
#define VCD_IF_SET_BINARY_PRE_LOG	"[ <- AMHAL] SET_BINARY(PRE)\n"
#define VCD_IF_SET_BINARY_MAIN_LOG	"[ <- AMHAL] SET_BINARY(MAIN)\n"
#define VCD_IF_SET_BINARY_POST_LOG	"[ <- AMHAL] SET_BINARY(POST)\n"
#define VCD_IF_START_VCD_LOG		"[ <- AMHAL] START_VCD\n"
#define VCD_IF_STOP_VCD_LOG		"[ <- AMHAL] STOP_VCD\n"
#define VCD_IF_SET_HW_PARAM_LOG		"[ <- AMHAL] SET_HW_PARAM\n"
#define VCD_IF_START_CALL_LOG		"[ <- AMHAL] START_CALL\n"
#define VCD_IF_STOP_CALL_LOG		"[ <- AMHAL] STOP_CALL\n"
#define VCD_IF_SET_UDATA_LOG		"[ <- AMHAL] SET_UDATA\n"
#define VCD_IF_GET_STATUS_LOG		"[ <- AMHAL] GET_STATUS\n"
#define VCD_IF_SET_CALL_MODE_LOG	"[ <-  PT  ] SET_CALL_MODE\n"
#define VCD_IF_START_RECORD_LOG		"[ <- SOUND] START_RECORD\n"
#define VCD_IF_STOP_RECORD_LOG		"[ <- SOUND] STOP_RECORD\n"
#define VCD_IF_START_PLAYBACK_LOG	"[ <- SOUND] START_PLAYBACK\n"
#define VCD_IF_STOP_PLAYBACK_LOG	"[ <- SOUND] STOP_PLAYBACK\n"
#define VCD_IF_GET_RECORD_BUFFER_LOG	"[ <- SOUND] GET_RECORD_BUFFER\n"
#define VCD_IF_GET_PLAYBACK_BUFFER_LOG	"[ <- SOUND] GET_PLAYBACK_BUFFER\n"
#define VCD_IF_GET_VOIP_UL_BUFFER_LOG	"[ <- SOUND] GET_VOIP_UL_BUFFER\n"
#define VCD_IF_GET_VOIP_DL_BUFFER_LOG	"[ <- SOUND] GET_VOIP_DL_BUFFER\n"
#define VCD_IF_WATCH_FW_LOG		"[ <- SOUND] WATCH_FW\n"
#define VCD_IF_WATCH_FW_PT_LOG		"[ <-  PT  ] WATCH_FW_PT\n"
#define VCD_IF_WATCH_CLKGEN_LOG		"[ <- SOUND] WATCH_CLKGEN\n"
#define VCD_IF_WATCH_CLKGEN_PT_LOG	"[ <-  PT  ] WATCH_CLKGEN\n"
#define VCD_IF_WATCH_CODEC_TYPE_LOG	"[ <- SOUND] WATCH_CODEC_TYPE\n"
#define VCD_IF_WAIT_PATH_LOG		"[ <- SOUND] WAIT_PATH\n"

/*
 * define macro declaration
 */


/*
 * enum declaration
 */
enum VCD_PROC_IF {
	VCD_PROC_IF_GET_MSG_BUFFER = 0,
	VCD_PROC_IF_START_VCD,
	VCD_PROC_IF_STOP_VCD,
	VCD_PROC_IF_SET_HW_PARAM,
	VCD_PROC_IF_START_CALL,
	VCD_PROC_IF_STOP_CALL,
	VCD_PROC_IF_RESERVE_0,
	VCD_PROC_IF_RESERVE_1,
	VCD_PROC_IF_RESERVE_2,
	VCD_PROC_IF_SET_UDATA,
	VCD_PROC_IF_GET_STATUS
};

enum VCD_DEBUG_COMMAND {
	VCD_DEBUG_DUMP_STATUS = VCD_COMMAND_MAX,
	VCD_DEBUG_DUMP_REGISTERS,
	VCD_DEBUG_DUMP_HPB_REGISTERS,
	VCD_DEBUG_DUMP_CPG_REGISTERS,
	VCD_DEBUG_DUMP_CRMU_REGISTERS,
	VCD_DEBUG_DUMP_GTU_REGISTERS,
	VCD_DEBUG_DUMP_VOICEIF_REGISTERS,
	VCD_DEBUG_DUMP_INTCVO_REGISTERS,
	VCD_DEBUG_DUMP_SPUV_REGISTERS,
	VCD_DEBUG_DUMP_DSP0_REGISTERS,
	VCD_DEBUG_DUMP_MEMORIES,
	VCD_DEBUG_DUMP_PRAM0_MEMORY,
	VCD_DEBUG_DUMP_XRAM0_MEMORY,
	VCD_DEBUG_DUMP_YRAM0_MEMORY,
	VCD_DEBUG_DUMP_DSPIO_MEMORY,
	VCD_DEBUG_DUMP_SDRAM_STATIC_AREA_MEMORY,
	VCD_DEBUG_DUMP_FW_STATIC_BUFFER_MEMORY,
	VCD_DEBUG_DUMP_FW_CRASHLOG,
	VCD_DEBUG_DUMP_DIAMOND_MEMORY,
	VCD_DEBUG_SET_CALL_MODE,
	VCD_DEBUG_SET_1KHZ_TONE_MODE,
	VCD_DEBUG_SET_PCM_LOOPBACK_MODE,
	VCD_DEBUG_SET_VIF_LOOPBACK_MODE,
	VCD_DEBUG_SET_MODE_0,
	VCD_DEBUG_SET_MODE_1,
	VCD_DEBUG_SET_MODE_2,
	VCD_DEBUG_SET_MODE_3,
	VCD_DEBUG_CALC_TRIGGER_START,
	VCD_DEBUG_CALC_TRIGGER_STOP,
};


/*
 * structure declaration
 */
struct vcd_execute_func {
	unsigned int	command;
	int		(*func)(void *arg);
};

struct vcd_async_wait {
	wait_queue_head_t	read_q;
	atomic_t		readable;
};


/*
 * table declaration
 */


/*
 * prototype declaration
 */

/* Internal public notification functions */
void vcd_complete_buffer(void);
void vcd_beginning_buffer(void);
void vcd_start_fw(void);
void vcd_stop_fw(int result);
void vcd_stop_fw_only_sound(void);
void vcd_codec_type_ind(unsigned int codec_type);
void vcd_udata_ind(void);
void vcd_start_clkgen(void);
void vcd_wait_path(void);
void vcd_voip_ul_callback(unsigned int buf_size);
void vcd_voip_dl_callback(unsigned int buf_size);
void vcd_get_semaphore(void);
void vcd_release_semaphore(void);

/* Internal functions */
static int vcd_get_binary_buffer(void);
static int vcd_set_binary_preprocessing(char *file_path);
static int vcd_set_binary_main(unsigned int write_size);
static int vcd_set_binary_postprocessing(void);
static int vcd_get_msg_buffer(void);
static int vcd_get_async_area(void);
static int vcd_free_async_area(void);
static int vcd_start_vcd(void);
static int vcd_stop_vcd(void);
static int vcd_set_hw_param(void);
static int vcd_start_call(void);
static int vcd_stop_call(void);
static int vcd_set_udata(void);
static void vcd_get_status(void);

static int vcd_set_call_mode(void *arg);
static void vcd_async_notify(unsigned int cb_type, int result);
static int vcd_start_record(void *arg);
static int vcd_stop_record(void *arg);
static int vcd_start_playback(void *arg);
static int vcd_stop_playback(void *arg);
static int vcd_get_record_buffer(void *arg);
static int vcd_get_playback_buffer(void *arg);
static int vcd_watch_fw(void *arg);
static int vcd_watch_fw_pt(void *arg);
static int vcd_watch_clkgen(void *arg);
static int vcd_watch_clkgen_pt(void *arg);
static int vcd_watch_codec_type(void *arg);
static int vcd_set_wait_path(void *arg);
static int vcd_get_voip_ul_buffer(void *arg);
static int vcd_get_voip_dl_buffer(void *arg);
static int vcd_set_voip_callback(void *arg);

/* Proc functions */
static int vcd_read_exec_proc(char *page, char **start, off_t offset,
					int count, int *eof, void *data);
static int vcd_write_exec_proc(struct file *filp, const char *buffer,
					unsigned long len, void *data);
static int vcd_read_log_level(char *page, char **start, off_t offset,
					int count, int *eof, void *data);
static int vcd_write_log_level(struct file *filp, const char *buffer,
					unsigned long len, void *data);
static int vcd_read_exec_func(char *page, char **start, off_t offset,
					int count, int *eof, void *data);
static int vcd_write_exec_func(struct file *filp, const char *buffer,
					unsigned long len, void *data);

static int vcd_debug_execute(unsigned int command);
static void vcd_debug_complete_buffer(void);
static void vcd_debug_beginning_buffer(void);
static void vcd_debug_watch_start_fw(void);
static void vcd_debug_watch_stop_fw(void);
static void vcd_debug_watch_start_clkgen(void);
static void vcd_debug_watch_stop_clkgen(void);
static void vcd_debug_watch_codec_type(unsigned int codec_type);

static int vcd_create_proc_entry(void);
static void vcd_remove_proc_entry(void);

/* Driver functions */
static int vcd_probe(void);
static void vcd_remove(void);
static int vcd_suspend(struct device *dev);
static int vcd_resume(struct device *dev);
static int vcd_runtime_suspend(struct device *dev);
static int vcd_runtime_resume(struct device *dev);
static int vcd_fops_open(struct inode *inode, struct file *filp);
static int vcd_fops_release(struct inode *inode, struct file *filp);
static int vcd_fops_mmap(struct file *fp, struct vm_area_struct *vma);
static long vcd_fops_ioctl
	(struct file *fp, unsigned int cmd, unsigned long arg);
static unsigned int vcd_fops_poll
	(struct file *fp, struct poll_table_struct *wait);

static int __init vcd_module_init(void);
static void __exit vcd_module_exit(void);

#endif /* __VCD_IF_H__ */
