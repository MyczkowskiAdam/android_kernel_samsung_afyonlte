/* vcd_ctrl.c
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
#include <linux/semaphore.h>
#include <linux/timer.h>

#include "linux/vcd/vcd_common.h"
#include "linux/vcd/vcd_interface.h"
#include "linux/vcd/vcd_spuv.h"
#include "vcd_ctrl.h"
#include "vcd_ctrl_func.h"

/*
 * global variable declaration
 */
int g_vcd_ctrl_result;
unsigned int g_vcd_ctrl_call_type;
unsigned int g_vcd_ctrl_record_mode;
unsigned int g_vcd_ctrl_playback_mode;
unsigned int g_vcd_ctrl_call_kind;
int g_vcd_ctrl_is_stop_fw;
struct vcd_playback_option g_vcd_ctrl_stored_playback_option;
struct timer_list g_vcd_ctrl_timer_list;
unsigned int g_vcd_ctrl_timer_status;
DEFINE_SEMAPHORE(g_vcd_ctrl_semaphore);

/* ========================================================================= */
/* For AMHAL functions                                                       */
/* ========================================================================= */

/**
 * @brief	get binary buffer function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_get_binary_buffer(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	g_vcd_ctrl_result = vcd_spuv_get_binary_buffer();

	vcd_pr_end_control_function("g_vcd_ctrl_result[%x].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	set binary preprocessing function.
 *
 * @param	file_path	binary file path.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_set_binary_preprocessing(char *file_path)
{
	int binary_kind = VCD_BINARY_SPUV;
	unsigned int command = VCD_CTRL_FUNC_SET_BINARY_SPUV;

	vcd_pr_start_control_function("file_path[%p].\n", file_path);

	/* execute spuv function */
	binary_kind = vcd_spuv_set_binary_preprocessing(file_path);
	if (VCD_BINARY_SPUV == binary_kind)
		command = VCD_CTRL_FUNC_SET_BINARY_SPUV;
	else if (VCD_BINARY_PCM == binary_kind)
		command = VCD_CTRL_FUNC_SET_BINARY_PCM;
	else
		command = VCD_CTRL_FUNC_SET_BINARY_DIAMOND;

	/* check sequence */
	g_vcd_ctrl_result =
		vcd_ctrl_func_check_sequence(command);
	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_control_info("check sequence[%d].\n",
			g_vcd_ctrl_result);
		goto rtn;
	}

rtn:
	vcd_pr_end_control_function("g_vcd_ctrl_result[%x].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	set binary main function.
 *
 * @param	write_size	size.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_set_binary_main(unsigned int write_size)
{
	vcd_pr_start_control_function("write_size[%d].\n", write_size);

	/* execute spuv function */
	g_vcd_ctrl_result = vcd_spuv_set_binary_main(write_size);

	vcd_pr_end_control_function("g_vcd_ctrl_result[%x].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	set binary postprocessing function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_set_binary_postprocessing(void)
{
	int binary_kind = VCD_BINARY_SPUV;
	vcd_pr_start_control_function();

	/* execute spuv function */
	binary_kind = vcd_spuv_set_binary_postprocessing();
	if (VCD_BINARY_SPUV == binary_kind)
		/* update active status */
		vcd_ctrl_func_set_active_feature(
			VCD_CTRL_FUNC_FEATURE_SET_BINARY_SPUV);
	else if (VCD_BINARY_PCM == binary_kind)
		/* update active status */
		vcd_ctrl_func_set_active_feature(
			VCD_CTRL_FUNC_FEATURE_SET_BINARY_PCM);
	else
		/* update active status */
		vcd_ctrl_func_set_active_feature(
			VCD_CTRL_FUNC_FEATURE_SET_BINARY_DIAMOND);

	vcd_pr_end_control_function("g_vcd_ctrl_result[%x].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	get msg buffer function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_get_msg_buffer(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	g_vcd_ctrl_result = vcd_spuv_get_msg_buffer();

	vcd_pr_end_control_function("g_vcd_ctrl_result[%x].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	get asyncrnous return area function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_get_async_area(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	g_vcd_ctrl_result = vcd_spuv_get_async_area();

	vcd_pr_end_control_function("g_vcd_ctrl_result[%x].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	get asyncrnous return area function.
 *
 * @param	adr	asyncrnous address.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_free_async_area(unsigned int adr)
{
	vcd_pr_start_control_function("adr[0x%08x].\n", adr);

	/* execute spuv function */
	g_vcd_ctrl_result = vcd_spuv_free_async_area(adr);

	vcd_pr_end_control_function("g_vcd_ctrl_result[%x].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	vcd start function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_start_vcd(void)
{
	vcd_pr_start_control_function();

	/* update active status */
	vcd_ctrl_func_unset_active_feature(VCD_CTRL_FUNC_FEATURE_AMHAL_STOP);

	/* check sequence */
	g_vcd_ctrl_result =
		vcd_ctrl_func_check_sequence(VCD_CTRL_FUNC_START_VCD);
	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_control_info("check sequence[%d].\n",
			g_vcd_ctrl_result);
		g_vcd_ctrl_result =
			vcd_ctrl_func_convert_result(g_vcd_ctrl_result);
		goto rtn;
	}

	/* execute spuv function */
	g_vcd_ctrl_result = vcd_spuv_start_vcd();
	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_err("start vcd[%d].\n", g_vcd_ctrl_result);

#ifdef __VCD_PROC_IF_ENABLE__
		/* update result */
		g_vcd_ctrl_result = VCD_ERR_SYSTEM;
#endif /* __VCD_PROC_IF_ENABLE__ */

		goto rtn;
	}

	/* update status */
	g_vcd_ctrl_is_stop_fw = VCD_ENABLE;

	/* update active status */
	vcd_ctrl_func_set_active_feature(VCD_CTRL_FUNC_FEATURE_VCD);

rtn:
	vcd_pr_end_control_function("g_vcd_ctrl_result[%d].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	vcd stop function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_stop_vcd(void)
{
	unsigned int active_feature = VCD_CTRL_FUNC_FEATURE_NONE;
	unsigned int init_feature = VCD_CTRL_FUNC_FEATURE_NONE;

	vcd_pr_start_control_function();

	/* update active status */
	vcd_ctrl_func_set_active_feature(VCD_CTRL_FUNC_FEATURE_AMHAL_STOP);

	/* semaphore start */
	down(&g_vcd_ctrl_semaphore);

	/* operation state confirmation */
	active_feature = vcd_ctrl_func_get_active_feature();
	if (VCD_CTRL_FUNC_FEATURE_STORED_PLAYBACK & active_feature) {
		/* update active status */
		vcd_ctrl_func_unset_active_feature(
				VCD_CTRL_FUNC_FEATURE_STORED_PLAYBACK);
		/* stop stored timer */
		vcd_ctrl_stop_stored_playback_timer();
		/* notification fw stop */
		vcd_stop_fw_only_sound();
	}

	/* check sequence */
	g_vcd_ctrl_result =
		vcd_ctrl_func_check_sequence(VCD_CTRL_FUNC_STOP_VCD);
	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_control_info("check sequence[%d].\n",
			g_vcd_ctrl_result);
		g_vcd_ctrl_result =
			vcd_ctrl_func_convert_result(g_vcd_ctrl_result);
		goto rtn;
	}

	/* execute spuv function */
	g_vcd_ctrl_result = vcd_spuv_stop_vcd();
	if (VCD_ERR_NONE != g_vcd_ctrl_result)
		vcd_pr_control_info("vcd_spuv_stop_vcd[%d].\n",
			g_vcd_ctrl_result);

	/* delete call kind */
	g_vcd_ctrl_call_kind = VCD_CALL_KIND_CALL;
	/* init call type */
	g_vcd_ctrl_call_type = 0;

#ifdef __VCD_PROC_IF_ENABLE__
	/* update result */
	g_vcd_ctrl_result = VCD_ERR_NONE;
#endif /* __VCD_PROC_IF_ENABLE__ */

	/* update active status */
	init_feature = ~(VCD_CTRL_FUNC_FEATURE_AMHAL_STOP |
			VCD_CTRL_FUNC_FEATURE_SET_BINARY_SPUV |
			VCD_CTRL_FUNC_FEATURE_SET_BINARY_PCM |
			VCD_CTRL_FUNC_FEATURE_SET_BINARY_DIAMOND);

	vcd_ctrl_func_unset_active_feature(init_feature);

rtn:
	/* semaphore end */
	up(&g_vcd_ctrl_semaphore);

	vcd_pr_end_control_function("g_vcd_ctrl_result[%d].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	set hw param function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_set_hw_param(void)
{
	unsigned int active_feature = VCD_CTRL_FUNC_FEATURE_NONE;

	vcd_pr_start_control_function();

	/* check sequence */
	g_vcd_ctrl_result =
		vcd_ctrl_func_check_sequence(VCD_CTRL_FUNC_HW_PARAM);
	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_control_info("check sequence[%d].\n",
			g_vcd_ctrl_result);
		g_vcd_ctrl_result =
			vcd_ctrl_func_convert_result(g_vcd_ctrl_result);
		goto rtn;
	}

	/* execute spuv function */
	g_vcd_ctrl_result = vcd_spuv_set_hw_param();
	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_err("set hw param[%d].\n", g_vcd_ctrl_result);
		goto rtn;
	}

	/* update active status */
	vcd_ctrl_func_set_active_feature(VCD_CTRL_FUNC_FEATURE_HW_PARAM);

	/* operation state confirmation */
	active_feature = vcd_ctrl_func_get_active_feature();
	if (VCD_CTRL_FUNC_FEATURE_STORED_PLAYBACK & active_feature)
		vcd_ctrl_start_playback(&g_vcd_ctrl_stored_playback_option);

rtn:
	vcd_pr_end_control_function("g_vcd_ctrl_result[%d].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	start call function.
 *
 * @param	call_kind	cs/1khz tone/pcm loopback/vif loopback.
 * @param	mode		loopback mode.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_start_call(int call_kind, int mode)
{
	unsigned int active_feature = VCD_CTRL_FUNC_FEATURE_NONE;
	unsigned int notify_stop_fw = VCD_DISABLE;

	vcd_pr_start_control_function();

	/* check sequence */
	g_vcd_ctrl_result =
		vcd_ctrl_func_check_sequence(VCD_CTRL_FUNC_START_CALL);
	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_control_info("check sequence[%d].\n",
			g_vcd_ctrl_result);
		g_vcd_ctrl_result =
			vcd_ctrl_func_convert_result(g_vcd_ctrl_result);
		goto rtn;
	}

	/* execute spuv function */
	if (VCD_CALL_KIND_CALL == call_kind) {
		/* get call type (CS/VoIP) */
		g_vcd_ctrl_call_type = vcd_spuv_get_call_type();
		if (VCD_CALL_TYPE_VOIP == g_vcd_ctrl_call_type) {
			/* operation state confirmation */
			active_feature = vcd_ctrl_func_get_active_feature();
			/* check record */
			if (VCD_CTRL_FUNC_FEATURE_RECORD & active_feature) {
				g_vcd_ctrl_result = vcd_ctrl_stop_record();
				if (VCD_ERR_NONE != g_vcd_ctrl_result) {
					vcd_pr_err("stop record[%d].\n",
						g_vcd_ctrl_result);
					goto rtn;
				}
				notify_stop_fw = VCD_ENABLE;
			}
			/* check playback */
			if (VCD_CTRL_FUNC_FEATURE_PLAYBACK & active_feature) {
				g_vcd_ctrl_result = vcd_ctrl_stop_playback();
				if (VCD_ERR_NONE != g_vcd_ctrl_result) {
					vcd_pr_err("stop playback[%d].\n",
						g_vcd_ctrl_result);
					goto rtn;
				}
				notify_stop_fw = VCD_ENABLE;
			}
			/* notification for sound */
			if (VCD_ENABLE == notify_stop_fw)
				vcd_stop_fw_only_sound();
		}
		g_vcd_ctrl_result = vcd_spuv_start_call();
	} else if (VCD_CALL_KIND_1KHZ == call_kind)
		g_vcd_ctrl_result = vcd_spuv_start_1khz_tone();
	else if (VCD_CALL_KIND_PCM_LB == call_kind)
		g_vcd_ctrl_result = vcd_spuv_start_pcm_loopback(mode);
	else if (VCD_CALL_KIND_VIF_LB == call_kind)
		g_vcd_ctrl_result = vcd_spuv_start_bbif_loopback(mode);

	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_err("start call[%d].\n", g_vcd_ctrl_result);
		goto rtn;
	}

	/* save call kind */
	g_vcd_ctrl_call_kind = call_kind;

	/* update active status */
	vcd_ctrl_func_set_active_feature(VCD_CTRL_FUNC_FEATURE_CALL);

	/* notification fw stop */
	vcd_start_fw();

rtn:
	vcd_pr_end_control_function("g_vcd_ctrl_result[%d].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	stop call function.
 *
 * @param	call_kind	cs/1khz tone/pcm loopback/vif loopback.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_stop_call(int call_kind)
{
	vcd_pr_start_control_function();

	/* check sequence */
	g_vcd_ctrl_result =
		vcd_ctrl_func_check_sequence(VCD_CTRL_FUNC_STOP_CALL);
	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_control_info("check sequence[%d].\n",
			g_vcd_ctrl_result);
		goto rtn;
	}

	/* execute spuv function */
	if (VCD_CALL_KIND_CALL == call_kind) {
		g_vcd_ctrl_result = vcd_spuv_stop_call();
		if (VCD_CALL_TYPE_VOIP == g_vcd_ctrl_call_type)
			vcd_spuv_trigger_count_log(
				(VCD_LOG_TRIGGER_REC | VCD_LOG_TRIGGER_PLAY));
		/* init call type */
		g_vcd_ctrl_call_type = 0;
	} else if (VCD_CALL_KIND_1KHZ == call_kind)
		g_vcd_ctrl_result = vcd_spuv_stop_1khz_tone();
	else if (VCD_CALL_KIND_PCM_LB == call_kind)
		g_vcd_ctrl_result = vcd_spuv_stop_pcm_loopback();
	else if (VCD_CALL_KIND_VIF_LB == call_kind)
		g_vcd_ctrl_result = vcd_spuv_stop_bbif_loopback();

	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_err("stop call[%d].\n", g_vcd_ctrl_result);
		goto rtn;
	}

	/* update active status */
	vcd_ctrl_func_unset_active_feature(VCD_CTRL_FUNC_FEATURE_CALL);

rtn:
	vcd_pr_end_control_function("g_vcd_ctrl_result[%d].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	set udata function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_set_udata(void)
{
	vcd_pr_start_control_function();

	/* check sequence */
	g_vcd_ctrl_result =
		vcd_ctrl_func_check_sequence(VCD_CTRL_FUNC_SET_UDATA);
	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_control_info("check sequence[%d].\n",
			g_vcd_ctrl_result);
		g_vcd_ctrl_result =
			vcd_ctrl_func_convert_result(g_vcd_ctrl_result);
		goto rtn;
	}

	/* execute spuv function */
	g_vcd_ctrl_result = vcd_spuv_set_udata();
	if (VCD_ERR_NONE != g_vcd_ctrl_result) {
		vcd_pr_err("set udata[%d].\n", g_vcd_ctrl_result);
		goto rtn;
	}

rtn:
	vcd_pr_end_control_function("g_vcd_ctrl_result[%d].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	get status function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_get_status(void)
{
	unsigned int active_feature = VCD_CTRL_FUNC_FEATURE_NONE;

	vcd_pr_start_control_function();

	/* operation state confirmation */
	active_feature = vcd_ctrl_func_get_active_feature();

	if (VCD_CTRL_FUNC_FEATURE_ERROR & active_feature) {
		g_vcd_ctrl_result = VCD_CTRL_STATUS_ERROR;
	} else if (VCD_CTRL_FUNC_FEATURE_CALL & active_feature) {
		if ((VCD_CTRL_FUNC_FEATURE_RECORD & active_feature) &&
			(VCD_CTRL_FUNC_FEATURE_PLAYBACK & active_feature)) {
			g_vcd_ctrl_result =
				VCD_CTRL_STATUS_CALL_RECORD_PLAYBACK;
		} else if (VCD_CTRL_FUNC_FEATURE_RECORD & active_feature) {
			g_vcd_ctrl_result = VCD_CTRL_STATUS_CALL_RECORD;
		} else if (VCD_CTRL_FUNC_FEATURE_PLAYBACK & active_feature) {
			g_vcd_ctrl_result = VCD_CTRL_STATUS_CALL_PLAYBACK;
		} else {
			g_vcd_ctrl_result = VCD_CTRL_STATUS_CALL;
		}
	} else if (VCD_CTRL_FUNC_FEATURE_RECORD & active_feature) {
		if (VCD_CTRL_FUNC_FEATURE_PLAYBACK & active_feature)
			g_vcd_ctrl_result = VCD_CTRL_STATUS_RECORD_PLAYBACK;
		else
			g_vcd_ctrl_result = VCD_CTRL_STATUS_RECORD;
	} else if (VCD_CTRL_FUNC_FEATURE_PLAYBACK & active_feature) {
		g_vcd_ctrl_result = VCD_CTRL_STATUS_PLAYBACK;
	} else if (VCD_CTRL_FUNC_FEATURE_HW_PARAM & active_feature) {
		g_vcd_ctrl_result = VCD_CTRL_STATUS_READY;
	} else if (VCD_CTRL_FUNC_FEATURE_VCD & active_feature) {
		g_vcd_ctrl_result = VCD_CTRL_STATUS_ACTIVE;
	} else {
		g_vcd_ctrl_result = VCD_CTRL_STATUS_STANDBY;
	}

	vcd_pr_end_control_function("g_vcd_ctrl_result[%d].\n",
		g_vcd_ctrl_result);
	return;
}


/**
 * @brief	get result function.
 *
 * @param	none.
 *
 * @retval	result	processing result.
 */
int vcd_ctrl_get_result(void)
{
	vcd_pr_start_control_function();

	vcd_pr_end_control_function("g_vcd_ctrl_result[%d].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	cheak semantics function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
int vcd_ctrl_check_semantics(void)
{
	int ret = VCD_ERR_NONE;
	unsigned int active_feature = VCD_CTRL_FUNC_FEATURE_NONE;

	vcd_pr_start_control_function();

	active_feature = vcd_ctrl_func_get_active_feature();

	if (VCD_CTRL_FUNC_FEATURE_AMHAL_STOP & active_feature)
		ret = VCD_ERR_NOT_ACTIVE;

	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/* ========================================================================= */
/* For Sound driver functions                                                */
/* ========================================================================= */

/**
 * @brief	start record function.
 *
 * @param[in]	option	pointer of record option structure.
 *
 * @retval	VCD_ERR_NONE	successful.
 * @retval	others		result of called function.
 */
int vcd_ctrl_start_record(struct vcd_record_option *option)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function("option[%p].\n", option);
	vcd_pr_control_info("option.mode[%d].\n", option->mode);

	/* check sequence */
	ret = vcd_ctrl_func_check_sequence(VCD_CTRL_FUNC_START_RECORD);
	if (VCD_ERR_NONE != ret) {
		vcd_pr_control_info("check sequence[%d].\n", ret);
		ret = vcd_ctrl_func_convert_result(ret);
		goto rtn;
	}

	if (VCD_CALL_TYPE_VOIP == g_vcd_ctrl_call_type) {
		ret = VCD_ERR_BUSY;
		goto rtn;
	} else {
		/* execute spuv function */
		ret = vcd_spuv_start_record(option);
		if (VCD_ERR_NONE != ret) {
			vcd_pr_err("start record[%d].\n", ret);
			goto rtn;
		}
	}

	/* update active status */
	vcd_ctrl_func_set_active_feature(VCD_CTRL_FUNC_FEATURE_RECORD);

rtn:
	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/**
 * @brief	stop record function.
 *
 * @param	none.
 *
 * @retval	VCD_ERR_NONE	successful.
 * @retval	others		result of called function.
 */
int vcd_ctrl_stop_record(void)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function();

	/* check sequence */
	ret = vcd_ctrl_func_check_sequence(VCD_CTRL_FUNC_STOP_RECORD);
	if (VCD_ERR_NONE != ret) {
		vcd_pr_control_info("check sequence[%d].\n", ret);
		ret = vcd_ctrl_func_convert_result(ret);
		goto rtn;
	}

	/* execute spuv function */
	ret = vcd_spuv_stop_record();
	if (VCD_ERR_NONE != ret) {
		vcd_pr_err("stop record[%d].\n", ret);
		/* update result */
		ret = VCD_ERR_NONE;
	}

	/* update active status */
	vcd_ctrl_func_unset_active_feature(VCD_CTRL_FUNC_FEATURE_RECORD);

	/* check need stop vcd */
	ret = vcd_ctrl_func_check_stop_vcd_need();
	if (VCD_ERR_NONE == ret)
		vcd_ctrl_stop_vcd();
	else
		ret = VCD_ERR_NONE;

rtn:
	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/**
 * @brief	start playback function.
 *
 * @param[in]	option	pointer of playback option structure.
 *
 * @retval	VCD_ERR_NONE	successful.
 * @retval	others		result of called function.
 */
int vcd_ctrl_start_playback(struct vcd_playback_option *option)
{
	int ret = VCD_ERR_NONE;
	unsigned int active_feature = VCD_CTRL_FUNC_FEATURE_NONE;


	vcd_pr_start_control_function("option[%p].\n", option);
	vcd_pr_control_info("option.mode[%d].\n", option->mode);

	/* check sequence */
	ret = vcd_ctrl_func_check_sequence(VCD_CTRL_FUNC_START_PLAYBACK);
	if (VCD_ERR_NONE != ret) {
		vcd_pr_control_info("check sequence[%d].\n", ret);

		if (VCD_ERR_NOT_ACTIVE == ret) {
			/* operation state confirmation */
			active_feature = vcd_ctrl_func_get_active_feature();

			if (VCD_CTRL_FUNC_FEATURE_VCD & active_feature) {
				/* update active status */
				vcd_ctrl_func_set_active_feature(
					VCD_CTRL_FUNC_FEATURE_STORED_PLAYBACK);
				g_vcd_ctrl_stored_playback_option.
					mode = option->mode;
				g_vcd_ctrl_stored_playback_option.
					beginning_buffer =
					option->beginning_buffer;
				/* start stored timer */
				vcd_ctrl_start_stored_playback_timer();
				ret = VCD_ERR_NONE;
				goto rtn;
			}
		}

		ret = vcd_ctrl_func_convert_result(ret);
		goto rtn;
	}

	/* update active status */
	vcd_ctrl_func_unset_active_feature(
		VCD_CTRL_FUNC_FEATURE_STORED_PLAYBACK);

	/* stop stored timer */
	vcd_ctrl_stop_stored_playback_timer();

	if (VCD_CALL_TYPE_VOIP == g_vcd_ctrl_call_type) {
		ret = VCD_ERR_BUSY;
		goto rtn;
	} else {
		/* execute spuv function */
		ret = vcd_spuv_start_playback(option, g_vcd_ctrl_call_kind);
		if (VCD_ERR_NONE != ret) {
			vcd_pr_err("start playback[%d].\n", ret);
			goto rtn;
		}
	}

	/* update active status */
	vcd_ctrl_func_set_active_feature(VCD_CTRL_FUNC_FEATURE_PLAYBACK);

rtn:
	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/**
 * @brief	stop playback function.
 *
 * @param	none.
 *
 * @retval	VCD_ERR_NONE	successful.
 * @retval	others		result of called function.
 */
int vcd_ctrl_stop_playback(void)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function();

	/* update active status */
	vcd_ctrl_func_unset_active_feature(
		VCD_CTRL_FUNC_FEATURE_STORED_PLAYBACK);

	/* stop stored timer */
	vcd_ctrl_stop_stored_playback_timer();

	/* check sequence */
	ret = vcd_ctrl_func_check_sequence(VCD_CTRL_FUNC_STOP_PLAYBACK);
	if (VCD_ERR_NONE != ret) {
		vcd_pr_control_info("check sequence[%d].\n", ret);
		ret = vcd_ctrl_func_convert_result(ret);
		goto rtn;
	}

	/* execute spuv function */
	ret = vcd_spuv_stop_playback();
	if (VCD_ERR_NONE != ret) {
		vcd_pr_err("stop playback[%d].\n", ret);
		/* update result */
		ret = VCD_ERR_NONE;
	}

	/* update active status */
	vcd_ctrl_func_unset_active_feature(VCD_CTRL_FUNC_FEATURE_PLAYBACK);

	/* check need stop vcd */
	ret = vcd_ctrl_func_check_stop_vcd_need();
	if (VCD_ERR_NONE == ret)
		vcd_ctrl_stop_vcd();
	else
		ret = VCD_ERR_NONE;

rtn:
	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}

/**
 * @brief	get record buffer function.
 *
 * @param[out]	info	pointer of record buffer info structure.
 *
 * @retval	none.
 */
void vcd_ctrl_get_record_buffer(struct vcd_record_buffer_info *info)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_get_record_buffer(info);

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	get playback buffer function.
 *
 * @param[out]	info	pointer of playback buffer info structure.
 *
 * @retval	none.
 */
void vcd_ctrl_get_playback_buffer(struct vcd_playback_buffer_info *info)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_get_playback_buffer(info, g_vcd_ctrl_call_kind);

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	get VoIP UL buffer function.
 *
 * @param[out]	info	pointer of VoIP UL buffer info structure.
 *
 * @retval	none.
 */
void vcd_ctrl_get_voip_ul_buffer(struct vcd_voip_ul_buffer_info *info)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_get_voip_ul_buffer(info);

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	get VoIP DL buffer function.
 *
 * @param[out]	info	pointer of VoIP DL buffer info structure.
 *
 * @retval	none.
 */
void vcd_ctrl_get_voip_dl_buffer(struct vcd_voip_dl_buffer_info *info)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_get_voip_dl_buffer(info);

	vcd_pr_end_control_function();
	return;
}


/* ========================================================================= */
/* For spuv functions                                                        */
/* ========================================================================= */

/**
 * @brief	rec_trigger notification function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_rec_trigger(void)
{
	unsigned int active_feature = VCD_CTRL_FUNC_FEATURE_NONE;
	unsigned int buf_size = 0;

	vcd_pr_start_control_function();

	active_feature = vcd_ctrl_func_get_active_feature();

	if ((VCD_CTRL_FUNC_FEATURE_CALL & active_feature) &&
		(VCD_CALL_TYPE_VOIP == g_vcd_ctrl_call_type)) {
		/* VoIP UL path route */
		vcd_spuv_voip_ul(&buf_size);
		/* update VoIP UL buffer ID */
		vcd_spuv_update_voip_ul_buffer_id();
		/* notification buffer update */
		vcd_voip_ul_callback(buf_size);
	} else if (VCD_CTRL_FUNC_FEATURE_RECORD & active_feature) {
		/* notification buffer update */
		vcd_complete_buffer();
	} else {
		/* record is not active */
	}

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	play_trigger notification function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_play_trigger(void)
{
	unsigned int active_feature = VCD_CTRL_FUNC_FEATURE_NONE;
	unsigned int buf_size = 0;

	vcd_pr_start_control_function();

	active_feature = vcd_ctrl_func_get_active_feature();

	if ((VCD_CTRL_FUNC_FEATURE_CALL & active_feature) &&
		(VCD_CALL_TYPE_VOIP == g_vcd_ctrl_call_type)) {
		/* VoIP DL path route */
		vcd_spuv_voip_dl(&buf_size);
		/* update VoIP DL buffer ID */
		vcd_spuv_update_voip_dl_buffer_id();
		/* notification buffer update */
		vcd_voip_dl_callback(buf_size);
	} else if (VCD_CTRL_FUNC_FEATURE_PLAYBACK & active_feature) {
		/* normal playback route */
		if (VCD_CALL_KIND_CALL != g_vcd_ctrl_call_kind)
			vcd_spuv_pt_playback();
		/* notification buffer update */
		vcd_beginning_buffer();
	} else {
		/* playback is not active */
	}

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	codec type notification function.
 *
 * @param	codec_type.	VCD_CODEC_WB(0)
 *				VCD_CODEC_NB(1)
 *
 * @retval	none.
 */
void vcd_ctrl_codec_type_ind(unsigned int codec_type)
{
	vcd_pr_start_control_function();

	/* notification codec type */
	vcd_codec_type_ind(codec_type);

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	fw stop notification function.
 *
 * @param	result	result.
 *
 * @retval	none.
 */
void vcd_ctrl_stop_fw(int result)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function("result[%d].\n", result);

	/* semaphore start */
	down(&g_vcd_ctrl_semaphore);

	/* check execute */
	if (VCD_DISABLE == g_vcd_ctrl_is_stop_fw)
		goto rtn;

	/* update feature */
	vcd_ctrl_func_set_active_feature(VCD_CTRL_FUNC_FEATURE_ERROR);

	/* update active status */
	vcd_ctrl_func_unset_active_feature(
		VCD_CTRL_FUNC_FEATURE_STORED_PLAYBACK);

	/* stop stored timer */
	vcd_ctrl_stop_stored_playback_timer();

	/* notification fw stop */
	vcd_stop_fw(result);

	/* check need stop vcd */
	ret = vcd_ctrl_func_check_stop_vcd_need();
	if (VCD_ERR_NONE == ret)
		vcd_ctrl_error_stop_vcd();

	/* update status */
	g_vcd_ctrl_is_stop_fw = VCD_DISABLE;

rtn:
	/* semaphore end */
	up(&g_vcd_ctrl_semaphore);

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	UDATA notification function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_udata_ind(void)
{
	vcd_pr_start_control_function();

	/* notification udata_ind */
	vcd_udata_ind();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	start clkgen timing notification function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_start_clkgen(void)
{
	vcd_pr_start_control_function();

	/* notification start clkgen timing */
	vcd_start_clkgen();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	stop clkgen timing notification function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_stop_clkgen(void)
{
	vcd_pr_start_control_function();

	/* notification stop clkgen timing */
	vcd_stop_clkgen();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	wait set path function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_wait_path(void)
{
	vcd_pr_start_control_function();

	/* wait set path */
	vcd_wait_path();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	get semaphore function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_get_semaphore(void)
{
	vcd_pr_start_control_function();

	/* semaphore start */
	vcd_get_semaphore();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	release semaphore function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_release_semaphore(void)
{
	vcd_pr_start_control_function();

	/* semaphore end */
	vcd_release_semaphore();

	vcd_pr_end_control_function();
	return;
}


/* ========================================================================= */
/* Driver functions                                                          */
/* ========================================================================= */

/**
 * @brief	driver suspend function.
 *
 * @param	none.
 *
 * @retval	VCD_ERR_NONE	successful.
 * @retval	VCD_ERR_BUSY	vcd is operating.
 */
int vcd_ctrl_suspend(void)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function();

	/* nop */

	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}

/**
 * @brief	driver resume function.
 *
 * @param	none.
 *
 * @retval	VCD_ERR_NONE	successful.
 */
int vcd_ctrl_resume(void)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function();

	/* nop */

	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/**
 * @brief	driver runtime suspend function.
 *
 * @param	none.
 *
 * @retval	VCD_ERR_NONE	successful.
 * @retval	VCD_ERR_BUSY	vcd is operating.
 */
int vcd_ctrl_runtime_suspend(void)
{
	int ret = VCD_ERR_NONE;
	vcd_pr_start_control_function();

	/* nop */

	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/**
 * @brief	driver runtime resume function.
 *
 * @param	none.
 *
 * @retval	VCD_ERR_NONE	successful.
 */
int vcd_ctrl_runtime_resume(void)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function();

	/* nop */

	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/**
 * @brief	driver probe function.
 *
 * @param	none.
 *
 * @retval	VCD_ERR_NONE	successful.
 * @retval	others		result of called function.
 */
int vcd_ctrl_probe(void)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function();

	/* execute spuv function */
	ret = vcd_spuv_ioremap();
	if (VCD_ERR_NONE != ret)
		goto rtn;

	/* execute spuv function */
	ret = vcd_spuv_get_fw_buffer();
	if (VCD_ERR_NONE != ret)
		goto rtn;

	/* execute spuv function */
	ret = vcd_spuv_create_queue();
	if (VCD_ERR_NONE != ret)
		goto rtn;

	/* initialize */
	vcd_ctrl_func_initialize();
	g_vcd_ctrl_result = VCD_ERR_NONE;

	/* register initialize */
	vcd_spuv_init_register();

	/* resampler init */
	ret = vcd_spuv_resampler_init();
	if (VCD_ERR_NONE != ret)
		goto rtn;

	/* ipc semaphore initialize */
	vcd_spuv_ipc_semaphore_init();

rtn:
	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/**
 * @brief	driver remove function.
 *
 * @param	none.
 *
 * @retval	VCD_ERR_NONE	successful.
 * @retval	others		result of called function.
 */
int vcd_ctrl_remove(void)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function();

	/* resampler close */
	vcd_spuv_resampler_close();

	/* execute spuv function */
	vcd_spuv_destroy_queue();

	/* execute spuv function */
	vcd_spuv_free_fw_buffer();

	/* execute spuv function */
	vcd_spuv_iounmap();

	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/* ========================================================================= */
/* Dump functions                                                            */
/* ========================================================================= */

/**
 * @brief	dump status function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_status(void)
{
	unsigned int active_feature = VCD_CTRL_FUNC_FEATURE_NONE;

	vcd_pr_start_control_function();

	/* operation state confirmation */
	active_feature = vcd_ctrl_func_get_active_feature();

	/* control status dump */
	vcd_pr_registers_dump("g_vcd_ctrl_active_feature      [%08x].\n",
		active_feature);

	/* execute spuv function */
	vcd_spuv_dump_status();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump registers function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_registers(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_registers();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump HPB registers function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_hpb_registers(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_hpb_registers();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump cpg registers function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_cpg_registers(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_cpg_registers();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump crmu registers function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_crmu_registers(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_crmu_registers();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump gtu registers function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_gtu_registers(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_gtu_registers();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump voiceif registers function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_voiceif_registers(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_voiceif_registers();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump intcvo registers function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_intcvo_registers(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_intcvo_registers();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump spuv registers function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_spuv_registers(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_spuv_registers();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump dsp0 registers function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_dsp0_registers(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_dsp0_registers();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump memories function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_memories(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_memories();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump pram0 memory function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_pram0_memory(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_pram0_memory();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump xram0 memory function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_xram0_memory(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_xram0_memory();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump yram0 memory function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_yram0_memory(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_yram0_memory();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump dspio memory function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_dspio_memory(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_dspio_memory();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump sdram static area memory function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_sdram_static_area_memory(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_sdram_static_area_memory();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump fw static buffer memory function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_fw_static_buffer_memory(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_fw_static_buffer_memory();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump spuv crashlog function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_spuv_crashlog(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_spuv_crashlog();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	dump diamond memory function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_dump_diamond_memory(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_dump_diamond_memory();

	vcd_pr_end_control_function();
	return;
}


/* ========================================================================= */
/* Debug functions                                                           */
/* ========================================================================= */

/**
 * @brief	start calc trigger function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_calc_trigger_start(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_calc_trigger_start();

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	stop calc trigger function.
 *
 * @param	none.
 *
 * @retval	none.
 */
void vcd_ctrl_calc_trigger_stop(void)
{
	vcd_pr_start_control_function();

	/* execute spuv function */
	vcd_spuv_calc_trigger_stop();

	vcd_pr_end_control_function();
	return;
}


/* ========================================================================= */
/* Internal functions                                                        */
/* ========================================================================= */

/**
 * @brief	vcd error stop function.
 *
 * @param	none.
 *
 * @retval	ret	result.
 */
static int vcd_ctrl_error_stop_vcd(void)
{
	unsigned int init_feature = VCD_CTRL_FUNC_FEATURE_NONE;

	vcd_pr_start_control_function();

	/* execute spuv function */
	g_vcd_ctrl_result = vcd_spuv_stop_vcd();
	if (VCD_ERR_NONE != g_vcd_ctrl_result)
		vcd_pr_control_info("vcd_spuv_stop_vcd[%d].\n",
			g_vcd_ctrl_result);

	/* delete call kind */
	g_vcd_ctrl_call_kind = VCD_CALL_KIND_CALL;

#ifdef __VCD_PROC_IF_ENABLE__
	/* update result */
	g_vcd_ctrl_result = VCD_ERR_NONE;
#endif /* __VCD_PROC_IF_ENABLE__ */

	/* update active status */
	init_feature = ~(VCD_CTRL_FUNC_FEATURE_AMHAL_STOP |
			VCD_CTRL_FUNC_FEATURE_SET_BINARY_SPUV |
			VCD_CTRL_FUNC_FEATURE_SET_BINARY_PCM |
			VCD_CTRL_FUNC_FEATURE_SET_BINARY_DIAMOND);

	vcd_ctrl_func_unset_active_feature(init_feature);

	vcd_pr_end_control_function("g_vcd_ctrl_result[%d].\n",
		g_vcd_ctrl_result);
	return g_vcd_ctrl_result;
}


/**
 * @brief	start playback timer function.
 *
 * @param	none.
 *
 * @retval	none.
 */
static void vcd_ctrl_start_stored_playback_timer(void)
{
	vcd_pr_start_control_function();

	/* check status */
	if (VCD_ENABLE == g_vcd_ctrl_timer_status)
		goto rtn;

	/* set timer */
	init_timer(&g_vcd_ctrl_timer_list);
	g_vcd_ctrl_timer_list.function =
			(void *)vcd_ctrl_stored_playback_timer_cb;
	g_vcd_ctrl_timer_list.data = 0;
	g_vcd_ctrl_timer_list.expires =
			jiffies + VCD_CTRL_STORED_PLAYBACK_TIMER;
	add_timer(&g_vcd_ctrl_timer_list);

	/* set status */
	g_vcd_ctrl_timer_status = VCD_ENABLE;
rtn:
	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	stop playback timer function.
 *
 * @param	none.
 *
 * @retval	none.
 */
static void vcd_ctrl_stop_stored_playback_timer(void)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function();

	if (VCD_DISABLE == g_vcd_ctrl_timer_status)
		goto rtn;

	ret = del_timer(&g_vcd_ctrl_timer_list);
	if (0 == ret)
		vcd_pr_control_info("timer not start.\n");

	/* set status */
	g_vcd_ctrl_timer_status = VCD_DISABLE;

rtn:
	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	stored playback timeout function.
 *
 * @param	none.
 *
 * @retval	none.
 */
static void vcd_ctrl_stored_playback_timer_cb(void)
{
	vcd_pr_start_control_function();

	vcd_pr_err("stored playback timeout.\n");

	/* update active status */
	vcd_ctrl_func_unset_active_feature(
			VCD_CTRL_FUNC_FEATURE_STORED_PLAYBACK);

	/* notification fw stop */
	vcd_stop_fw_only_sound();

	/* set status */
	g_vcd_ctrl_timer_status = VCD_DISABLE;

	vcd_pr_end_control_function();
	return;
}
