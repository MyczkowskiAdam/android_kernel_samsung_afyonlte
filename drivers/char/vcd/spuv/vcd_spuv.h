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
/* spuv status */
#define VCD_SPUV_STATUS_NONE			0x00000000
#define VCD_SPUV_STATUS_WAIT_ACK		0x00000001
#define VCD_SPUV_STATUS_WAIT_REQ		0x00000002
#define VCD_SPUV_STATUS_NEED_ACK		0x00000004
#define VCD_SPUV_STATUS_SYSTEM_ERROR		0x00000008

/* spuv AINT value */
#define VCD_SPUV_AINT_REQ			0x00000001
#define VCD_SPUV_AINT_ACK			0x00000002

/* spuv IEVENTC value */
#define VCD_SPUV_IEVENTC			0x00001111

/* spuv fw interface id */
#define VCD_SPUV_INTERFACE_ID			0x5C
#define VCD_SPUV_INTERFACE_ID_ACK_ONLY		0

/* spuv fw record/playback gain */
#define VCD_SPUV_GAIN_DISABLE			0
#define VCD_SPUV_GAIN_ENABLE			10

/* spuv fw loopback delay */
#define VCD_SPUV_LOOPBACK_DELAY_0		0
#define VCD_SPUV_LOOPBACK_DELAY_500		25

/* spuv fw codec type */
#define VCD_SPUV_CODEC_FR			0
#define VCD_SPUV_CODEC_HR			1
#define VCD_SPUV_CODEC_EFR			2
#define VCD_SPUV_CODEC_AMR			3
#define VCD_SPUV_CODEC_WBAMR			4

/* spuv fw message id (to spuv) */
#define VCD_SPUV_HW_PARAMETERS_IND		0x04
#define VCD_SPUV_ACTIVE_REQ			0x10
#define VCD_SPUV_SPEECH_START_REQ		0x11
#define VCD_SPUV_SPEECH_STOP_REQ		0x12
#define VCD_SPUV_VOICE_RECORDING_START_REQ	0x13
#define VCD_SPUV_VOICE_RECORDING_STOP_REQ	0x14
#define VCD_SPUV_VOICE_PLAYING_START_REQ	0x15
#define VCD_SPUV_VOICE_PLAYING_STOP_REQ		0x16
#define VCD_SPUV_1KHZ_TONE_START_REQ		0x17
#define VCD_SPUV_1KHZ_TONE_STOP_REQ		0x18
#define VCD_SPUV_PCM_LOOPBACK_START_REQ		0x19
#define VCD_SPUV_PCM_LOOPBACK_STOP_REQ		0x1A
#define VCD_SPUV_BBIF_LOOPBACK_START_REQ	0x1B
#define VCD_SPUV_BBIF_LOOPBACK_STOP_REQ		0x1C
#define VCD_SPUV_TRACE_SELECT_REQ		0x1D
#define VCD_SPUV_UDATA_REQ			0x1E

/* spuv fw message length (to spuv) */
#define VCD_SPUV_HW_PARAMETERS_LENGTH		5
#define VCD_SPUV_ACTIVE_LENGTH			2
#define VCD_SPUV_SPEECH_START_LENGTH		3
#define VCD_SPUV_VOIP_SPEECH_START_LENGTH	8
#define VCD_SPUV_SPEECH_STOP_LENGTH		2
#define VCD_SPUV_VOICE_RECORDING_START_LENGTH	7
#define VCD_SPUV_VOICE_RECORDING_STOP_LENGTH	2
#define VCD_SPUV_VOICE_PLAYING_START_LENGTH	9
#define VCD_SPUV_VOICE_PLAYING_STOP_LENGTH	2
#define VCD_SPUV_1KHZ_TONE_START_LENGTH		2
#define VCD_SPUV_1KHZ_TONE_STOP_LENGTH		2
#define VCD_SPUV_PCM_LOOPBACK_START_LENGTH	4
#define VCD_SPUV_PCM_LOOPBACK_STOP_LENGTH	2
#define VCD_SPUV_BBIF_LOOPBACK_START_LENGTH	3
#define VCD_SPUV_BBIF_LOOPBACK_STOP_LENGTH	2
#define VCD_SPUV_TRACE_SELECT_LENGTH		3
#define VCD_SPUV_UDATA_LENGTH			3

/* spuv fw message id (from spuv) */
#define VCD_SPUV_BOOT_COMPLETE_IND		0x00
#define VCD_SPUV_SYSTEM_ERROR_IND		0x01
#define VCD_SPUV_SYSTEM_INFO_IND		0x02
#define VCD_SPUV_TRIGGER_PLAY_IND		0x03
#define VCD_SPUV_TRIGGER_REC_IND		0x05
#define VCD_SPUV_CODEC_TYPE_IND			0x06
#define VCD_SPUV_ACTIVE_CNF			0x20
#define VCD_SPUV_SPEECH_START_CNF		0x21
#define VCD_SPUV_SPEECH_STOP_CNF		0x22
#define VCD_SPUV_VOICE_RECORDING_START_CNF	0x23
#define VCD_SPUV_VOICE_RECORDING_STOP_CNF	0x24
#define VCD_SPUV_VOICE_PLAYING_START_CNF	0x25
#define VCD_SPUV_VOICE_PLAYING_STOP_CNF		0x26
#define VCD_SPUV_1KHZ_TONE_START_CNF		0x27
#define VCD_SPUV_1KHZ_TONE_STOP_CNF		0x28
#define VCD_SPUV_PCM_LOOPBACK_START_CNF		0x29
#define VCD_SPUV_PCM_LOOPBACK_STOP_CNF		0x2A
#define VCD_SPUV_BBIF_LOOPBACK_START_CNF	0x2B
#define VCD_SPUV_BBIF_LOOPBACK_STOP_CNF		0x2C
#define VCD_SPUV_TRACE_SELECT_CNF		0x2D
#define VCD_SPUV_UDATA_IND			0x2E

/* spuv fw message log */
#define VCD_SPUV_HW_PARAMETERS_IND_LOG		\
		"[ -> SPUV ] SPUV_HW_PARAMETERS_IND\n"
#define VCD_SPUV_ACTIVE_REQ_LOG			\
		"[ -> SPUV ] SPUV_ACTIVE_REQ\n"
#define VCD_SPUV_SPEECH_START_REQ_CS_LOG	\
		"[ -> SPUV ] SPUV_SPEECH_START_REQ CS\n"
#define VCD_SPUV_SPEECH_START_REQ_VOIP_LOG	\
		"[ -> SPUV ] SPUV_SPEECH_START_REQ VOIP\n"
#define VCD_SPUV_SPEECH_START_REQ_VOLTE_LOG	\
		"[ -> SPUV ] SPUV_SPEECH_START_REQ VOLTE\n"
#define VCD_SPUV_SPEECH_START_REQ_VT_LOG	\
		"[ -> SPUV ] SPUV_SPEECH_START_REQ VT\n"
#define VCD_SPUV_SPEECH_START_REQ_UNKNOWN_LOG	\
		"[ -> SPUV ] SPUV_SPEECH_START_REQ UNKNOWN\n"
#define VCD_SPUV_SPEECH_STOP_REQ_LOG		\
		"[ -> SPUV ] SPUV_SPEECH_STOP_REQ\n"
#define VCD_SPUV_VOICE_RECORDING_START_REQ_LOG	\
		"[ -> SPUV ] SPUV_VOICE_RECORDING_START_REQ\n"
#define VCD_SPUV_VOICE_RECORDING_STOP_REQ_LOG	\
		"[ -> SPUV ] SPUV_VOICE_RECORDING_STOP_REQ\n"
#define VCD_SPUV_VOICE_PLAYING_START_REQ_LOG	\
		"[ -> SPUV ] SPUV_VOICE_PLAYING_START_REQ\n"
#define VCD_SPUV_VOICE_PLAYING_STOP_REQ_LOG	\
		"[ -> SPUV ] SPUV_VOICE_PLAYING_STOP_REQ\n"
#define VCD_SPUV_1KHZ_TONE_START_REQ_LOG	\
		"[ -> SPUV ] SPUV_1KHZ_TONE_START_REQ\n"
#define VCD_SPUV_1KHZ_TONE_STOP_REQ_LOG		\
		"[ -> SPUV ] SPUV_1KHZ_TONE_STOP_REQ\n"
#define VCD_SPUV_PCM_LOOPBACK_START_REQ_LOG	\
		"[ -> SPUV ] SPUV_PCM_LOOPBACK_START_REQ\n"
#define VCD_SPUV_PCM_LOOPBACK_STOP_REQ_LOG	\
		"[ -> SPUV ] SPUV_PCM_LOOPBACK_STOP_REQ\n"
#define VCD_SPUV_BBIF_LOOPBACK_START_REQ_LOG	\
		"[ -> SPUV ] SPUV_BBIF_LOOPBACK_START_REQ\n"
#define VCD_SPUV_BBIF_LOOPBACK_STOP_REQ_LOG	\
		"[ -> SPUV ] SPUV_BBIF_LOOPBACK_STOP_REQ\n"
#define VCD_SPUV_TRACE_SELECT_REQ_LOG		\
		"[ -> SPUV ] SPUV_TRACE_SELECT_REQ\n"
#define VCD_SPUV_UDATA_REQ_LOG			\
		"[ -> SPUV ] SPUV_UDATA_REQ\n"
#define VCD_SPUV_BOOT_COMPLETE_IND_LOG		\
		"[ <- SPUV ] SPUV_BOOT_COMPLETE_IND\n"
#define VCD_SPUV_SYSTEM_ERROR_IND_LOG		\
		"[ <- SPUV ] SPUV_SYSTEM_ERROR_IND\n"
#define VCD_SPUV_SYSTEM_INFO_IND_LOG		\
		"[ <- SPUV ] SPUV_SYSTEM_INFO_IND\n"
#define VCD_SPUV_TRIGGER_PLAY_IND_LOG		\
		"[ <- SPUV ] SPUV_TRIGGER_PLAY_IND\n"
#define VCD_SPUV_TRIGGER_REC_IND_LOG		\
		"[ <- SPUV ] SPUV_TRIGGER_REC_IND\n"
#define VCD_SPUV_CODEC_TYPE_IND_LOG		\
		"[ <- SPUV ] SPUV_CODEC_TYPE_IND\n"
#define VCD_SPUV_ACTIVE_CNF_LOG			\
		"[ <- SPUV ] SPUV_ACTIVE_CNF\n"
#define VCD_SPUV_SPEECH_START_CNF_LOG		\
		"[ <- SPUV ] SPUV_SPEECH_START_CNF\n"
#define VCD_SPUV_SPEECH_STOP_CNF_LOG		\
		"[ <- SPUV ] SPUV_SPEECH_STOP_CNF\n"
#define VCD_SPUV_VOICE_RECORDING_START_CNF_LOG	\
		"[ <- SPUV ] SPUV_VOICE_RECORDING_START_CNF\n"
#define VCD_SPUV_VOICE_RECORDING_STOP_CNF_LOG	\
		"[ <- SPUV ] SPUV_VOICE_RECORDING_STOP_CNF\n"
#define VCD_SPUV_VOICE_PLAYING_START_CNF_LOG	\
		"[ <- SPUV ] SPUV_VOICE_PLAYING_START_CNF\n"
#define VCD_SPUV_VOICE_PLAYING_STOP_CNF_LOG	\
		"[ <- SPUV ] SPUV_VOICE_PLAYING_STOP_CNF\n"
#define VCD_SPUV_1KHZ_TONE_START_CNF_LOG	\
		"[ <- SPUV ] SPUV_1KHZ_TONE_START_CNF\n"
#define VCD_SPUV_1KHZ_TONE_STOP_CNF_LOG		\
		"[ <- SPUV ] SPUV_1KHZ_TONE_STOP_CNF\n"
#define VCD_SPUV_PCM_LOOPBACK_START_CNF_LOG	\
		"[ <- SPUV ] SPUV_PCM_LOOPBACK_START_CNF\n"
#define VCD_SPUV_PCM_LOOPBACK_STOP_CNF_LOG	\
		"[ <- SPUV ] SPUV_PCM_LOOPBACK_STOP_CNF\n"
#define VCD_SPUV_BBIF_LOOPBACK_START_CNF_LOG	\
		"[ <- SPUV ] SPUV_BBIF_LOOPBACK_START_CNF\n"
#define VCD_SPUV_BBIF_LOOPBACK_STOP_CNF_LOG	\
		"[ <- SPUV ] SPUV_BBIF_LOOPBACK_STOP_CNF\n"
#define VCD_SPUV_TRACE_SELECT_CNF_LOG		\
		"[ <- SPUV ] SPUV_TRACE_SELECT_CNF\n"
#define VCD_SPUV_UDATA_IND_LOG			\
		"[ <- SPUV ] SPUV_UDATA_IND\n"
#define VCD_SPUV_ACK_LOG			\
		"[ <- SPUV ] ACK\n"

/* spuv fw timeout log */
#define VCD_SPUV_TIMEOUT_LOG		\
		"[ <- SPUV ] TIME OUT\n"

/* spuv fw result */
#define VCD_SPUV_FW_RESULT_SUCCESS		0
#define VCD_SPUV_FW_RESULT_ERROR		1

/* voip watchdog timer */
#define VCD_SPUV_FW_WATCHDOG_TIMER		1000

/*
 * define macro declaration
 */
#define VCD_SPUV_SPI_NO				gic_spi(71)


/*
 * structure declaration
 */
struct vcd_spuv_workqueue {
	struct list_head top;
	spinlock_t lock;
	wait_queue_head_t wait;
	wait_queue_head_t finish;
	struct task_struct *task;
};

struct vcd_spuv_work;

struct vcd_spuv_work {
	struct list_head link;
	void (*func)(void);
	int status;
	atomic_long_t data;
};

struct vcd_spuv_info {
	struct timer_list timer_list;
	unsigned int timer_status;
	unsigned int watchdog_status;
	spinlock_t watchdog_lock;
	unsigned int status;
	spinlock_t status_lock;
	unsigned int irq_status;
	unsigned int wait_fw_if_id;
	unsigned int wait_fw_msg_id;
	int fw_result;
};

struct vcd_spuv_set_binary_info {
	unsigned int binary_kind;
	unsigned int top_logical_address;
	unsigned int top_physical_address;
	unsigned int write_address;
	unsigned int total_size;
	unsigned int max_size;
};


/*
 * table declaration
 */


/*
 * prototype declaration
 */

/* Internal public functions */
int vcd_spuv_ioremap(void);
void vcd_spuv_iounmap(void);
void vcd_spuv_init_register(void);
void vcd_spuv_ipc_semaphore_init(void);
int vcd_spuv_get_fw_buffer(void);
void vcd_spuv_free_fw_buffer(void);
int vcd_spuv_get_binary_buffer(void);
int vcd_spuv_set_binary_preprocessing(char *file_path);
int vcd_spuv_set_binary_main(unsigned int write_size);
int vcd_spuv_set_binary_postprocessing(void);
int vcd_spuv_get_msg_buffer(void);
int vcd_spuv_get_async_area(void);
int vcd_ctrl_free_async_area(unsigned int adr);
int vcd_spuv_start_vcd(void);
int vcd_spuv_stop_vcd(void);
int vcd_spuv_set_hw_param(void);
int vcd_spuv_start_call(void);
int vcd_spuv_stop_call(void);
int vcd_spuv_set_udata(void);
int vcd_spuv_start_record(struct vcd_record_option *option);
int vcd_spuv_stop_record(void);
int vcd_spuv_start_playback(struct vcd_playback_option *option,
					unsigned int call_kind);
int vcd_spuv_stop_playback(void);
void vcd_spuv_get_record_buffer(struct vcd_record_buffer_info *info);
void vcd_spuv_get_playback_buffer(struct vcd_playback_buffer_info *info,
					unsigned int call_kind);
void vcd_spuv_get_voip_ul_buffer(struct vcd_voip_ul_buffer_info *info);
void vcd_spuv_get_voip_dl_buffer(struct vcd_voip_dl_buffer_info *info);
void vcd_spuv_init_record_buffer_id(void);
void vcd_spuv_init_playback_buffer_id(void);
int vcd_spuv_get_call_type(void);
void vcd_spuv_voip_ul(unsigned int *buf_size);
void vcd_spuv_voip_dl(unsigned int *buf_size);
void vcd_spuv_update_voip_ul_buffer_id(void);
void vcd_spuv_update_voip_dl_buffer_id(void);
int vcd_spuv_resampler_init(void);
int vcd_spuv_resampler_close(void);
void vcd_spuv_pt_playback(void);

int vcd_spuv_start_1khz_tone(void);
int vcd_spuv_stop_1khz_tone(void);
int vcd_spuv_start_pcm_loopback(int mode);
int vcd_spuv_stop_pcm_loopback(void);
int vcd_spuv_start_bbif_loopback(int mode);
int vcd_spuv_stop_bbif_loopback(void);
int vcd_spuv_set_trace_select(void);

/* Interrupt functions */
static int vcd_spuv_request_irq(void);
static void vcd_spuv_free_irq(void);
static int vcd_spuv_set_irq(int validity);
static irqreturn_t vcd_spuv_irq_handler(int irq, void *dev_id);

/* Queue functions */
static void vcd_spuv_work_initialize(
	struct vcd_spuv_work *work, void (*func)(void));
static void vcd_spuv_workqueue_destroy(struct vcd_spuv_workqueue *wq);
static inline int vcd_spuv_workqueue_thread(void *arg);
static struct vcd_spuv_workqueue *vcd_spuv_workqueue_create(char *taskname);
static void vcd_spuv_workqueue_enqueue(
	struct vcd_spuv_workqueue *wq, struct vcd_spuv_work *work);
int vcd_spuv_create_queue(void);
void vcd_spuv_destroy_queue(void);
static void vcd_spuv_set_schedule(void);
static void vcd_spuv_interrupt_ack(void);
static void vcd_spuv_interrupt_req(void);
static void vcd_spuv_watchdog_timeout(void);
static void vcd_spuv_rec_trigger(void);
static void vcd_spuv_play_trigger(void);
static void vcd_spuv_codec_type_ind(unsigned int codec_type);
static void vcd_spuv_system_error(void);
static void vcd_spuv_udata_ind(void);
static void vcd_spuv_watchdog_timer_cb(void);
static void vcd_spuv_start_watchdog_timer(void);
static void vcd_spuv_stop_watchdog_timer(void);
static int vcd_spuv_is_log_enable(unsigned int msg);
static void vcd_spuv_interface_log(unsigned int msg);

/* FW info functions */
static void vcd_spuv_set_wait_fw_info(unsigned int fw_id, unsigned int msg_id);
static void vcd_spuv_check_wait_fw_info(unsigned int fw_id,
			unsigned int msg_id, unsigned int result);

/* Status functions */
static unsigned int vcd_spuv_get_status(void);
static void vcd_spuv_set_status(unsigned int status);
static void vcd_spuv_unset_status(unsigned int status);
static int vcd_spuv_check_result(void);

/* Dump functions */
void vcd_spuv_dump_status(void);
void vcd_spuv_dump_registers(void);
void vcd_spuv_dump_hpb_registers(void);
void vcd_spuv_dump_cpg_registers(void);
void vcd_spuv_dump_crmu_registers(void);
void vcd_spuv_dump_gtu_registers(void);
void vcd_spuv_dump_voiceif_registers(void);
void vcd_spuv_dump_intcvo_registers(void);
void vcd_spuv_dump_spuv_registers(void);
void vcd_spuv_dump_dsp0_registers(void);
void vcd_spuv_dump_memories(void);
void vcd_spuv_dump_pram0_memory(void);
void vcd_spuv_dump_xram0_memory(void);
void vcd_spuv_dump_yram0_memory(void);
void vcd_spuv_dump_dspio_memory(void);
void vcd_spuv_dump_sdram_static_area_memory(void);
void vcd_spuv_dump_fw_static_buffer_memory(void);
void vcd_spuv_dump_spuv_crashlog(void);
void vcd_spuv_dump_diamond_memory(void);

/* Debug functions */
void vcd_spuv_calc_trigger_start(void);
void vcd_spuv_calc_trigger_stop(void);
void vcd_spuv_trigger_count_log(unsigned int type);

#endif /* __VCD_SPUV_H__ */
