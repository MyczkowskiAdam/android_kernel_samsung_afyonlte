/* vcd_ctrl_func.c
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
#include "linux/vcd/vcd_common.h"
#include "vcd_ctrl_func.h"


/*
 * global variable declaration
 */
unsigned int g_vcd_ctrl_active_feature;
DEFINE_SPINLOCK(g_vcd_ctrl_lock);

/* ========================================================================= */
/* Control internal public functions                                         */
/* ========================================================================= */


/**
 * @brief	ctrl_func initialize function.
 *
 * @param	none.
 *
 * @retval	VCD_ERR_NONE		successful.
 * @retval	VCD_ERR_NOW_EXECUTION	operation now in progress.
 */
void vcd_ctrl_func_initialize(void)
{
	vcd_pr_start_control_function();

	g_vcd_ctrl_active_feature = VCD_CTRL_FUNC_FEATURE_NONE;

	vcd_pr_end_control_function();
	return;
}


/**
 * @brief	check sequence function.
 *
 * @param[in]	command	execute command.
 *
 * @retval	VCD_ERR_NONE			successful.
 * @retval	VCD_ERR_BUSY			during use.
 * @retval	VCD_ERR_NOT_ACTIVE		unavailable.
 * @retval	VCD_ERR_ALREADY_EXECUTION	operation already in progress.
 * @retval	VCD_ERR_SYSTEM			problem is occurring.
 */
int vcd_ctrl_func_check_sequence(unsigned int command)
{
	int ret = VCD_ERR_NONE;
	unsigned int feature = VCD_CTRL_FUNC_FEATURE_NONE;

	vcd_pr_start_control_function("command[%d].\n", command);

	feature = vcd_ctrl_func_get_active_feature();

	if (VCD_CTRL_FUNC_FEATURE_ERROR & feature) {
		if (VCD_CTRL_FUNC_STOP_VCD == command)
			goto rtn;
		ret = VCD_ERR_SYSTEM;
		goto rtn;
	}

	switch (command) {
	case VCD_CTRL_FUNC_SET_BINARY_SPUV:
		if (VCD_CTRL_FUNC_FEATURE_SET_BINARY_SPUV & feature)
			ret = VCD_ERR_ALREADY_EXECUTION;
		break;
	case VCD_CTRL_FUNC_SET_BINARY_PCM:
		if (VCD_CTRL_FUNC_FEATURE_SET_BINARY_PCM & feature)
			ret = VCD_ERR_ALREADY_EXECUTION;
		break;
	case VCD_CTRL_FUNC_SET_BINARY_DIAMOND:
		if (VCD_CTRL_FUNC_FEATURE_SET_BINARY_DIAMOND & feature)
			ret = VCD_ERR_ALREADY_EXECUTION;
		break;
	case VCD_CTRL_FUNC_GET_MSG_BUFFER:
		break;
	case VCD_CTRL_FUNC_START_VCD:
		if (VCD_CTRL_FUNC_FEATURE_VCD & feature)
			ret = VCD_ERR_ALREADY_EXECUTION;
		break;
	case VCD_CTRL_FUNC_STOP_VCD:
		if (!(VCD_CTRL_FUNC_FEATURE_VCD & feature)) {
			ret = VCD_ERR_ALREADY_EXECUTION;
		} else if (VCD_CTRL_FUNC_FEATURE_CALL & feature) {
			ret = VCD_ERR_BUSY;
		} else if ((VCD_CTRL_FUNC_FEATURE_RECORD & feature) ||
			(VCD_CTRL_FUNC_FEATURE_PLAYBACK & feature)) {
			/* hold */
			ret = VCD_ERR_ALREADY_EXECUTION;
		}
		break;
	case VCD_CTRL_FUNC_HW_PARAM:
		if (!(VCD_CTRL_FUNC_FEATURE_VCD & feature))
			ret = VCD_ERR_NOT_ACTIVE;
		break;
	case VCD_CTRL_FUNC_START_CALL:
		if (!(VCD_CTRL_FUNC_FEATURE_HW_PARAM & feature))
			ret = VCD_ERR_NOT_ACTIVE;
		else if (VCD_CTRL_FUNC_FEATURE_CALL & feature)
			ret = VCD_ERR_ALREADY_EXECUTION;
		else if (VCD_CTRL_FUNC_FEATURE_AMHAL_STOP & feature)
			ret = VCD_ERR_NOT_ACTIVE;
		break;
	case VCD_CTRL_FUNC_STOP_CALL:
		if (!(VCD_CTRL_FUNC_FEATURE_CALL & feature))
			ret = VCD_ERR_ALREADY_EXECUTION;
		break;
	case VCD_CTRL_FUNC_SET_UDATA:
		if (!(VCD_CTRL_FUNC_FEATURE_VCD & feature))
			ret = VCD_ERR_NOT_ACTIVE;
		break;
	case VCD_CTRL_FUNC_START_RECORD:
		if (!(VCD_CTRL_FUNC_FEATURE_HW_PARAM & feature))
			ret = VCD_ERR_NOT_ACTIVE;
		else if (VCD_CTRL_FUNC_FEATURE_RECORD & feature)
			ret = VCD_ERR_ALREADY_EXECUTION;
		break;
	case VCD_CTRL_FUNC_STOP_RECORD:
		if (!(VCD_CTRL_FUNC_FEATURE_RECORD & feature))
			ret = VCD_ERR_ALREADY_EXECUTION;
		break;
	case VCD_CTRL_FUNC_START_PLAYBACK:
		if (!(VCD_CTRL_FUNC_FEATURE_HW_PARAM & feature))
			ret = VCD_ERR_NOT_ACTIVE;
		else if (VCD_CTRL_FUNC_FEATURE_PLAYBACK & feature)
			ret = VCD_ERR_ALREADY_EXECUTION;
		break;
	case VCD_CTRL_FUNC_STOP_PLAYBACK:
		if (!(VCD_CTRL_FUNC_FEATURE_PLAYBACK & feature))
			ret = VCD_ERR_ALREADY_EXECUTION;
		break;
	case VCD_CTRL_FUNC_GET_RECORD_BUFFER:
		break;
	case VCD_CTRL_FUNC_GET_PLAYBACK_BUFFER:
		break;
	case VCD_CTRL_FUNC_WATCH_STOP_FW:
		break;
	default:
		break;
	}

rtn:
	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/**
 * @brief	convert result function.
 *
 * @param[in]	result	data sources.
 *
 * @retval	VCD_ERR_NONE	successful.
 * @retval	others		input data.
 */
int vcd_ctrl_func_convert_result(int result)
{
	int ret = VCD_ERR_NONE;

	vcd_pr_start_control_function("result[%d].\n", result);

	if (VCD_ERR_ALREADY_EXECUTION != result)
		ret = result;

	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/**
 * @brief	check stop vcd need function.
 *
 * @param	none.
 *
 * @retval	VCD_ERR_NONE	need 'stop vcd'.
 * @retval	VCD_ERR_BUSY	not need 'stop vcd'.
 */
int vcd_ctrl_func_check_stop_vcd_need(void)
{
	int ret = VCD_ERR_BUSY;
	unsigned int feature = VCD_CTRL_FUNC_FEATURE_NONE;

	vcd_pr_start_control_function();

	feature = vcd_ctrl_func_get_active_feature();

	if (VCD_CTRL_FUNC_FEATURE_AMHAL_STOP & feature) {
		if (VCD_CTRL_FUNC_FEATURE_ERROR & feature)
			/* for vcd_ctrl_stop_fw */
			ret = VCD_ERR_NONE;
		else if ((!(VCD_CTRL_FUNC_FEATURE_RECORD & feature)) &&
			(!(VCD_CTRL_FUNC_FEATURE_PLAYBACK & feature)))
			/* for vcd_ctrl_stop_record/playback */
			ret = VCD_ERR_NONE;
	}

	vcd_pr_end_control_function("ret[%d].\n", ret);
	return ret;
}


/* ========================================================================= */
/* Driver status functions                                                   */
/* ========================================================================= */

/**
 * @brief	get active feature function.
 *
 * @param	none.
 *
 * @retval	active feature.
 */
unsigned int vcd_ctrl_func_get_active_feature(void)
{
	unsigned int feature = VCD_CTRL_FUNC_FEATURE_NONE;

	spin_lock(&g_vcd_ctrl_lock);
	vcd_pr_start_control_function();

	feature = g_vcd_ctrl_active_feature;

	vcd_pr_end_control_function("active feature[0x%08x].\n", feature);

	spin_unlock(&g_vcd_ctrl_lock);
	return feature;
}


/**
 * @brief	set active feature function.
 *
 * @param[in]	feature	set feature.
 *
 * @retval	none.
 */
void vcd_ctrl_func_set_active_feature(unsigned int feature)
{
	spin_lock(&g_vcd_ctrl_lock);

	vcd_pr_start_control_function("feature[0x%08x].\n", feature);

	vcd_pr_status_change("active feature[0x%08x] -> [0x%08x].\n",
		g_vcd_ctrl_active_feature,
		(g_vcd_ctrl_active_feature | feature));
	g_vcd_ctrl_active_feature = g_vcd_ctrl_active_feature | feature;

	vcd_pr_end_control_function();

	spin_unlock(&g_vcd_ctrl_lock);

	return;
}


/**
 * @brief	unset active feature function.
 *
 * @param[in]	feature	unset feature.
 *
 * @retval	none.
 */
void vcd_ctrl_func_unset_active_feature(unsigned int feature)
{
	spin_lock(&g_vcd_ctrl_lock);

	vcd_pr_start_control_function("feature[0x%08x].\n", feature);

	vcd_pr_status_change("active feature[0x%08x] -> [0x%08x].\n",
		g_vcd_ctrl_active_feature,
		(g_vcd_ctrl_active_feature & ~feature));
	g_vcd_ctrl_active_feature = g_vcd_ctrl_active_feature & ~feature;

	vcd_pr_end_control_function();

	spin_unlock(&g_vcd_ctrl_lock);

	return;
}
