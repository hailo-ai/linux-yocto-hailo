/****************************************************************************
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2023 Vivante Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 *
 * The GPL License (GPL)
 *
 * Copyright (c) 2014-2023 Vivante Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;
 *
 *****************************************************************************
 *
 * Note: This software is released under dual MIT and GPL licenses. A
 * recipient may use this file under the terms of either the MIT license or
 * GPL License. If you wish to use only one license not the other, you can
 * indicate your decision by deleting one of the above license notices in your
 * version of this file.
 *
 *****************************************************************************/

#include <media/v4l2-ioctl.h>
#include "hailo15-isp.h"
#include <isp_ctrl/hailo15_isp_ctrl.h>
#include "hailo15_isp_dmscv2.h"

static int hailo15_isp_dmscv2_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);

	switch (ctrl->id) {
	case HAILO15_ISP_CID_DMSC_ENABLE:
	case HAILO15_ISP_CID_DMSC_ENABLE_DEMOIRE:
	case HAILO15_ISP_CID_DMSC_ENABLE_DEPURPLE:
	case HAILO15_ISP_CID_DMSC_ENABLE_SHARPEN:
	case HAILO15_ISP_CID_DMSC_ENABLE_SHARPEN_LINE:
	case HAILO15_ISP_CID_DMSC_ENABLE_SKIN:
	case HAILO15_ISP_CID_DMSC_THRESHOLD:
	case HAILO15_ISP_CID_DMSC_DENOISE_STRENGTH:
	case HAILO15_ISP_CID_DMSC_SHARPEN_SIZE:
	case HAILO15_ISP_CID_DMSC_INTERPOLAT_DIR_THR_MAX:
	case HAILO15_ISP_CID_DMSC_INTERPOLAT_DIR_THR_MIN:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_THR_T1:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_THR_T2_SHIFT:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_THR_R1:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_THR_R2:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_R1:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_R2:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_T1:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_T2_SHIFT:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_AREA_THR:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_SAT_SHRINK:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FAC_BLACK:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FAC_WHITE:
	case HAILO15_ISP_CID_DMSC_SHARPEN_CLIP_BLACK:
	case HAILO15_ISP_CID_DMSC_SHARPEN_CLIP_WHITE:
	case HAILO15_ISP_CID_DMSC_SHARPEN_THR_T1:
	case HAILO15_ISP_CID_DMSC_SHARPEN_THR_T2_SHIFT:
	case HAILO15_ISP_CID_DMSC_SHARPEN_THR_T3:
	case HAILO15_ISP_CID_DMSC_SHARPEN_THR_T4_SHIFT:
	case HAILO15_ISP_CID_DMSC_SHARPEN_R1:
	case HAILO15_ISP_CID_DMSC_SHARPEN_R2:
	case HAILO15_ISP_CID_DMSC_SHARPEN_R3:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_R1:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_R2:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_STRENGTH:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_SHIFT1:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_SHIFT2:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_T1:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_00:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_01:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_02:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_10:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_11:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_12:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_20:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_21:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_22:
	case HAILO15_ISP_CID_DMSC_DEPURPLE_SAT_SHRINK:
	case HAILO15_ISP_CID_DMSC_DEPURPLE_CBCR_MODE:
	case HAILO15_ISP_CID_DMSC_DEPURPLE_THRESHOLD:
	case HAILO15_ISP_CID_DMSC_SKIN_CB_THR_MAX:
	case HAILO15_ISP_CID_DMSC_SKIN_CB_THR_MIN:
	case HAILO15_ISP_CID_DMSC_SKIN_CR_THR_MAX:
	case HAILO15_ISP_CID_DMSC_SKIN_CR_THR_MIN:
	case HAILO15_ISP_CID_DMSC_SKIN_Y_THR_MAX:
	case HAILO15_ISP_CID_DMSC_SKIN_Y_THR_MIN:
		ret = hailo15_isp_s_ctrl_event(isp_dev, isp_dev->ctrl_pad,
					       ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static int hailo15_isp_dmscv2_g_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);

	switch (ctrl->id) {
	case HAILO15_ISP_CID_DMSC_ENABLE:
	case HAILO15_ISP_CID_DMSC_ENABLE_DEMOIRE:
	case HAILO15_ISP_CID_DMSC_ENABLE_DEPURPLE:
	case HAILO15_ISP_CID_DMSC_ENABLE_SHARPEN:
	case HAILO15_ISP_CID_DMSC_ENABLE_SHARPEN_LINE:
	case HAILO15_ISP_CID_DMSC_ENABLE_SKIN:
	case HAILO15_ISP_CID_DMSC_THRESHOLD:
	case HAILO15_ISP_CID_DMSC_DENOISE_STRENGTH:
	case HAILO15_ISP_CID_DMSC_SHARPEN_SIZE:
	case HAILO15_ISP_CID_DMSC_INTERPOLAT_DIR_THR_MAX:
	case HAILO15_ISP_CID_DMSC_INTERPOLAT_DIR_THR_MIN:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_THR_T1:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_THR_T2_SHIFT:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_THR_R1:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_THR_R2:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_R1:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_R2:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_T1:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_T2_SHIFT:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_AREA_THR:
	case HAILO15_ISP_CID_DMSC_DEMOIRE_SAT_SHRINK:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FAC_BLACK:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FAC_WHITE:
	case HAILO15_ISP_CID_DMSC_SHARPEN_CLIP_BLACK:
	case HAILO15_ISP_CID_DMSC_SHARPEN_CLIP_WHITE:
	case HAILO15_ISP_CID_DMSC_SHARPEN_THR_T1:
	case HAILO15_ISP_CID_DMSC_SHARPEN_THR_T2_SHIFT:
	case HAILO15_ISP_CID_DMSC_SHARPEN_THR_T3:
	case HAILO15_ISP_CID_DMSC_SHARPEN_THR_T4_SHIFT:
	case HAILO15_ISP_CID_DMSC_SHARPEN_R1:
	case HAILO15_ISP_CID_DMSC_SHARPEN_R2:
	case HAILO15_ISP_CID_DMSC_SHARPEN_R3:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_R1:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_R2:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_STRENGTH:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_SHIFT1:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_SHIFT2:
	case HAILO15_ISP_CID_DMSC_SHARPEN_LINE_T1:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_00:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_01:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_02:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_10:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_11:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_12:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_20:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_21:
	case HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_22:
	case HAILO15_ISP_CID_DMSC_DEPURPLE_SAT_SHRINK:
	case HAILO15_ISP_CID_DMSC_DEPURPLE_CBCR_MODE:
	case HAILO15_ISP_CID_DMSC_DEPURPLE_THRESHOLD:
	case HAILO15_ISP_CID_DMSC_SKIN_CB_THR_MAX:
	case HAILO15_ISP_CID_DMSC_SKIN_CB_THR_MIN:
	case HAILO15_ISP_CID_DMSC_SKIN_CR_THR_MAX:
	case HAILO15_ISP_CID_DMSC_SKIN_CR_THR_MIN:
	case HAILO15_ISP_CID_DMSC_SKIN_Y_THR_MAX:
	case HAILO15_ISP_CID_DMSC_SKIN_Y_THR_MIN:
		ret = hailo15_isp_g_ctrl_event(isp_dev, isp_dev->ctrl_pad,
					       ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static const struct v4l2_ctrl_ops hailo15_isp_dmscv2_ctrl_ops = {
	.s_ctrl = hailo15_isp_dmscv2_s_ctrl,
	.g_volatile_ctrl = hailo15_isp_dmscv2_g_ctrl,
};

const struct v4l2_ctrl_config hailo15_isp_dmscv2_ctrls[] = {
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_enable",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_ENABLE_DEMOIRE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_enable_demoire",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_ENABLE_DEPURPLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_enable_depurple",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_ENABLE_SHARPEN,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_enable_sharpen",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_ENABLE_SHARPEN_LINE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_enable_sharpen_line",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_ENABLE_SKIN,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_enable_skin",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_THRESHOLD,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_threshold",
		.step = 1,
		.min = 0,
		.max = 0xFF,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DENOISE_STRENGTH,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_denoise_strength",
		.step = 1,
		.min = 0,
		.max = 32,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_SIZE,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_size",
		.step = 1,
		.min = 0,
		.max = 16,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_INTERPOLAT_DIR_THR_MAX,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_interpolat_dir_thr_max",
		.step = 1,
		.min = 0,
		.max = 4095,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_INTERPOLAT_DIR_THR_MIN,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_interpolat_dir_thr_min",
		.step = 1,
		.min = 0,
		.max = 4095,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEMOIRE_THR_T1,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_demoire_thr_t1",
		.step = 1,
		.min = 0,
		.max = 255,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEMOIRE_THR_T2_SHIFT,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_demoire_thr_t2_shift",
		.step = 1,
		.min = 0,
		.max = 8,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEMOIRE_THR_R1,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_demoire_thr_r1",
		.step = 1,
		.min = 0,
		.max = 256,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEMOIRE_THR_R2,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_demoire_thr_r2",
		.step = 1,
		.min = 0,
		.max = 256,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_R1,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_demoire_edge_r1",
		.step = 1,
		.min = 0,
		.max = 256,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_R2,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_demoire_edge_r2",
		.step = 1,
		.min = 0,
		.max = 256,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_T1,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_demoire_edge_t1",
		.step = 1,
		.min = 0,
		.max = 511,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_T2_SHIFT,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_demoire_edge_t2_shift",
		.step = 1,
		.min = 0,
		.max = 9,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEMOIRE_AREA_THR,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_demoire_area_thr",
		.step = 1,
		.min = 0,
		.max = 32,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEMOIRE_SAT_SHRINK,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_demoire_sat_shrink",
		.step = 1,
		.min = 0,
		.max = 32,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FAC_BLACK,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_fac_black",
		.step = 1,
		.min = 0,
		.max = 511,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FAC_WHITE,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_fac_white",
		.step = 1,
		.min = 0,
		.max = 511,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_CLIP_BLACK,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_clip_black",
		.step = 1,
		.min = 0,
		.max = 2047,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_CLIP_WHITE,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_clip_white",
		.step = 1,
		.min = 0,
		.max = 2047,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_THR_T1,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_thr_t1",
		.step = 1,
		.min = 0,
		.max = 2047,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_THR_T2_SHIFT,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_thr_t2_shift",
		.step = 1,
		.min = 0,
		.max = 11,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_THR_T3,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_thr_t3",
		.step = 1,
		.min = 0,
		.max = 2047,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_THR_T4_SHIFT,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_thr_t4_shift",
		.step = 1,
		.min = 0,
		.max = 11,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_R1,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_r1",
		.step = 1,
		.min = 0,
		.max = 256,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_R2,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_r2",
		.step = 1,
		.min = 0,
		.max = 256,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_R3,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_r3",
		.step = 1,
		.min = 0,
		.max = 256,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_LINE_R1,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_line_r1",
		.step = 1,
		.min = 0,
		.max = 256,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_LINE_R2,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_line_r2",
		.step = 1,
		.min = 0,
		.max = 256,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_LINE_STRENGTH,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_line_strength",
		.step = 1,
		.min = 0,
		.max = 4095,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_LINE_SHIFT1,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_line_shift1",
		.step = 1,
		.min = 0,
		.max = 10,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_LINE_SHIFT2,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_line_shift2",
		.step = 1,
		.min = 0,
		.max = 10,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_LINE_T1,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_line_t1",
		.step = 1,
		.min = 0,
		.max = 2047,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_00,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_filter_00",
		.step = 1,
		.min = 0,
		.max = 34,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_01,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_filter_01",
		.step = 1,
		.min = 0,
		.max = 34,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_02,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_filter_02",
		.step = 1,
		.min = 0,
		.max = 34,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_10,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_filter_10",
		.step = 1,
		.min = 0,
		.max = 34,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_11,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_filter_11",
		.step = 1,
		.min = 0,
		.max = 34,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_12,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_filter_12",
		.step = 1,
		.min = 0,
		.max = 34,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_20,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_filter_20",
		.step = 1,
		.min = 0,
		.max = 34,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_21,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_filter_21",
		.step = 1,
		.min = 0,
		.max = 34,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_22,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_sharpen_filter_22",
		.step = 1,
		.min = 0,
		.max = 34,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEPURPLE_SAT_SHRINK,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_depurple_sat_shrink",
		.step = 1,
		.min = 0,
		.max = 8,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEPURPLE_CBCR_MODE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_depurple_cbcr_mode",
		.step = 1,
		.min = 0,
		.max = 3,
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_DEPURPLE_THRESHOLD,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_depurple_threshold",
		.step = 1,
		.min = 0,
		.max = 255,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SKIN_CB_THR_MAX,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_skin_cb_thr_max",
		.step = 1,
		.min = 0,
		.max = 4095,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SKIN_CB_THR_MIN,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_skin_cb_thr_min",
		.step = 1,
		.min = 0,
		.max = 4095,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SKIN_CR_THR_MAX,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_skin_cr_thr_max",
		.step = 1,
		.min = 0,
		.max = 4095,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SKIN_CR_THR_MIN,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_skin_cr_thr_min",
		.step = 1,
		.min = 0,
		.max = 4095,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SKIN_Y_THR_MAX,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_skin_y_thr_max",
		.step = 1,
		.min = 0,
		.max = 4095,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dmscv2_ctrl_ops,
		.id = HAILO15_ISP_CID_DMSC_SKIN_Y_THR_MIN,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_dmsc_skin_y_thr_min",
		.step = 1,
		.min = 0,
		.max = 4095,
		.dims = { 1 },
	},
};

int hailo15_isp_dmscv2_ctrl_count(void)
{
	return ARRAY_SIZE(hailo15_isp_dmscv2_ctrls);
}

int hailo15_isp_dmscv2_ctrl_create(struct hailo15_isp_device *isp_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hailo15_isp_dmscv2_ctrls); i++) {
		v4l2_ctrl_new_custom(&isp_dev->ctrl_handler,
				     &hailo15_isp_dmscv2_ctrls[i], NULL);
		if (isp_dev->ctrl_handler.error) {
			dev_err(isp_dev->dev,
				"reigster isp dmsc ctrl %s failed %d.\n",
				hailo15_isp_dmscv2_ctrls[i].name,
				isp_dev->ctrl_handler.error);
		}
	}

	return 0;
}
