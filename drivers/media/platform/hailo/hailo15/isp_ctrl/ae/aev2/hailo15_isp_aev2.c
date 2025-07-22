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
#include "hailo15_isp_aev2.h"

static int hailo15_isp_aev2_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);

	switch (ctrl->id) {
	case HAILO15_ISP_CID_AE_ENABLE:
	case HAILO15_ISP_CID_AE_RESET:
	case HAILO15_ISP_CID_AE_SEM_MODE:
	case HAILO15_ISP_CID_AE_SETPOINT:
	case HAILO15_ISP_CID_AE_DAMPOVER:
	case HAILO15_ISP_CID_AE_DAMPUNDER:
	case HAILO15_ISP_CID_AE_TOLORANCE:
	case HAILO15_ISP_CID_AE_FLICKER_PERIOD:
	case HAILO15_ISP_CID_AE_AFPS:
	case HAILO15_ISP_CID_AE_HIST:
	case HAILO15_ISP_CID_AE_LUMA:
	case HAILO15_ISP_CID_AE_OBJECT_REGION:
	case HAILO15_ISP_CID_AE_ROI_WEIGHT:
	case HAILO15_ISP_CID_AE_ROI:
	case HAILO15_ISP_CID_AE_GAIN:
	case HAILO15_ISP_CID_AE_SENSOR_GAIN:
	case HAILO15_ISP_CID_AE_INTEGRATION_TIME:
	case HAILO15_ISP_CID_AE_IRIS:
	case HAILO15_ISP_CID_AE_IRIS_LIMITS:
	case HAILO15_ISP_CID_AE_HIST_MODE:
	case HAILO15_ISP_CID_AE_HIST_WINDOW:
	case HAILO15_ISP_CID_AE_HIST_WEIGHT:
	case HAILO15_ISP_CID_AE_HIST64_CHANNEL:
	case HAILO15_ISP_CID_AE_HIST64_MODE:
	case HAILO15_ISP_CID_AE_EXP_WINDOW:
	case HAILO15_ISP_CID_AE_HCG:
	case HAILO15_ISP_CID_AE_STEP_FACTOR_LIMIT:
	case HAILO15_ISP_CID_AE_SP_RATIO:
	case HAILO15_ISP_CID_AE_LEF_THRESHOLD:
		pr_debug("%s - got s_ctrl with id: 0x%x\n", __func__, ctrl->id);
		ret = hailo15_isp_s_ctrl_event(isp_dev, isp_dev->ctrl_pad,
					       ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static int hailo15_isp_aev2_g_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);

	switch (ctrl->id) {
	case HAILO15_ISP_CID_AE_ENABLE:
	case HAILO15_ISP_CID_AE_RESET:
	case HAILO15_ISP_CID_AE_SEM_MODE:
	case HAILO15_ISP_CID_AE_SETPOINT:
	case HAILO15_ISP_CID_AE_DAMPOVER:
	case HAILO15_ISP_CID_AE_DAMPUNDER:
	case HAILO15_ISP_CID_AE_TOLORANCE:
	case HAILO15_ISP_CID_AE_FLICKER_PERIOD:
	case HAILO15_ISP_CID_AE_AFPS:
	case HAILO15_ISP_CID_AE_HIST:
	case HAILO15_ISP_CID_AE_LUMA:
	case HAILO15_ISP_CID_AE_OBJECT_REGION:
	case HAILO15_ISP_CID_AE_ROI_WEIGHT:
	case HAILO15_ISP_CID_AE_ROI:
	case HAILO15_ISP_CID_AE_GAIN:
	case HAILO15_ISP_CID_AE_SENSOR_GAIN:
	case HAILO15_ISP_CID_AE_INTEGRATION_TIME:
	case HAILO15_ISP_CID_AE_EXP_STATUS:
	case HAILO15_ISP_CID_AE_IRIS:
	case HAILO15_ISP_CID_AE_IRIS_LIMITS:
	case HAILO15_ISP_CID_AE_FPS:
	case HAILO15_ISP_CID_AE_CONVERGED:
	case HAILO15_ISP_CID_AE_HIST_MODE:
	case HAILO15_ISP_CID_AE_HIST_WINDOW:
	case HAILO15_ISP_CID_AE_HIST_WEIGHT:
	case HAILO15_ISP_CID_AE_HIST64_CHANNEL:
	case HAILO15_ISP_CID_AE_HIST64_MODE:
	case HAILO15_ISP_CID_AE_HDR_GAINS:
	case HAILO15_ISP_CID_AE_HDR_INTEGRATION_TIMES:
	case HAILO15_ISP_CID_AE_EXP_INPUT:
	case HAILO15_ISP_CID_AE_EXP_WINDOW:
	case HAILO15_ISP_CID_AE_HCG:
	case HAILO15_ISP_CID_AE_STEP_FACTOR_LIMIT:
	case HAILO15_ISP_CID_AE_SP_RATIO:
	case HAILO15_ISP_CID_AE_LEF_THRESHOLD:
	case HAILO15_ISP_CID_AE_SEM_SETPOINT:
	case HAILO15_ISP_CID_AE_DYNAMIC_RANGE:
	case HAILO15_ISP_CID_AE_HIST64:
	case HAILO15_ISP_CID_AE_GAIN_LIMITS:
	case HAILO15_ISP_CID_AE_INTEGRATION_TIME_LIMITS:
		pr_debug("%s - got g_ctrl with id: 0x%x\n", __func__, ctrl->id);
		ret = hailo15_isp_g_ctrl_event(isp_dev, isp_dev->ctrl_pad,
					       ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static const struct v4l2_ctrl_ops hailo15_isp_aev2_ctrl_ops = {
	.s_ctrl = hailo15_isp_aev2_s_ctrl,
	.g_volatile_ctrl = hailo15_isp_aev2_g_ctrl,
};

const struct v4l2_ctrl_config hailo15_isp_aev2_ctrls[] = {
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_enable",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_RESET,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_reset",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_CONVERGED,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_converged",
		.step = 1,
		.min = 0,
		.max = 1,
	},

	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_SEM_MODE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_sem_mode",
		.step = 1,
		.min = 0,
		.max = 2,
	},
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_FLICKER_PERIOD,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_flicker_period",
		.step = 1,
		.min = 0,
		.max = 2,
	},
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_AFPS,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_afps",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		/* float 0~255 */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_SETPOINT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_setpoint",
		.step = 1,
		.min = 0,
		.max = 255,
	},
	{
		/* float 0~100 */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_TOLORANCE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_tolerance",
		.step = 1,
		.min = 0,
		.max = 100,
	},
	{
		/* float 0~1 */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_DAMPOVER,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_dampover",
		.step = 1,
		.min = 0,
		.max = 100,
	},
	{
		/* float 0~1 */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_DAMPUNDER,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_dampunder",
		.step = 1,
		.min = 0,
		.max = 100,
	},
	{
		/* float 16x32bit */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_ROI_WEIGHT,
		.type = V4L2_CTRL_TYPE_U32,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_roi_weight",
		.step = 1,
		.min = 0,
		.max = 0xFFFFFFFF,
		.dims = { 25 },
	},
	{
		/* int 16*4*32bit */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_ROI,
		.type = V4L2_CTRL_TYPE_U32,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_roi",
		.step = 1,
		.min = 0,
		.max = 0xFFFFFFFF,
		.dims = { 25, 4, 0, 0 },
	},
	{
		/* uint32_t array 256*32bit */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_HIST,
		.type = V4L2_CTRL_TYPE_U32,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_hist",
		.step = 1,
		.min = 0,
		.max = 0xFFFFFFFF,
		.dims = { 256 },
	},
	{
		/* uint32_t array 32*32bit */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_HIST64,
		.type = V4L2_CTRL_TYPE_U32,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_hist64",
		.step = 1,
		.min = 0,
		.max = 0xFFFFFFFF,
		.dims = { 32 },
	},
	{
		/* uint8_t array 25*8bit */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_LUMA,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_luma",
		.step = 1,
		.min = 0,
		.max = 0xFF,
		.dims = { 25 },
	},
	{
		/* uint8_t array 25*8bit*/
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_OBJECT_REGION,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_object_region",
		.step = 1,
		.min = 0,
		.max = 0xFF,
		.dims = { 25 },
	},
	{
		/* float 0.00 ~ 99.99 */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_GAIN,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_gain",
		.step = 1,
		.min = 1024,
		.max = 0x7FFFFFFF,
		.def = 1024,
	},
	{
		/* float 0.000001 ~ 0.999999 */
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_INTEGRATION_TIME,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_integration_time",
		.step = 1,
		.min = 1,
		.max = 999999,
		.def = 1,
	},
	{ /* uint8_t array 4096 * 4 bytes */
	  .ops = &hailo15_isp_aev2_ctrl_ops,
	  .id = HAILO15_ISP_CID_AE_EXP_STATUS,
	  .type = V4L2_CTRL_TYPE_U8,
	  .flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	  .name = "isp_ae_exp_status",
	  .step = 1,
	  .min = 0,
	  .max = 0xFF,
	  .dims = { 4096 * 4 }
	},
	{
		/* float 1.00 ~  999.99*/
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_IRIS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_iris",
		.step = 1,
		.min = 100,
		.max = 99999,
		.def = 100,
	},
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_IRIS_LIMITS,
		.type = V4L2_CTRL_TYPE_U32,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_iris_limits",
		.step = 1,
		.min = 0,
		.max = 0xffffffff,
		.dims = { 2 },
	},
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_FPS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_fps",
		.step = 1,
		.min = 0,
		.max = 100,
	},
	{
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_HIST_MODE,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_hist_mode",
        .step = 1,
        .min  = 1,
        .max  = 5,
        .def  = 1,
    },
	{
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_HIST64_MODE,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_hist64_mode",
        .step = 1,
        .min  = 0,
        .max  = 5,
        .def  = 1,
    },
    {
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_HIST64_CHANNEL,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_hist64_channel",
        .step = 1,
        .min  = 0,
        .max  = 7,
        .def  = 1,
    },
    {
        /* uint16_t array 4*16bit*/
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_HIST_WINDOW,
        .type = V4L2_CTRL_TYPE_U16,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_hist_window",
        .step = 1,
        .min  = 0,
        .max  = 0xFFFF,
        .dims = {4},
    },
    {
        /* uint8_t array 25*8bit*/
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_HIST_WEIGHT,
        .type = V4L2_CTRL_TYPE_U8,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_hist_weight",
        .step = 1,
        .min  = 0,
        .max  = 0x10,
        .dims = {25},
    },
    {
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_EXP_INPUT,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_exp_input",
        .step = 1,
        .min  = 0,
        .max  = 2,
        .def  = 1,
    },
    {
        /* uint16_t array 4*16bit*/
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_EXP_WINDOW,
        .type = V4L2_CTRL_TYPE_U16,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_exp_window",
        .step = 1,
        .min  = 0,
        .max  = 0xFFFF,
        .dims = {4},
    },
    {
        /* float 0.00 ~ 99.99 */
        .ops = &hailo15_isp_aev2_ctrl_ops,
        .id = HAILO15_ISP_CID_AE_SENSOR_GAIN,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_sensor_gain",
        .step = 1,
        .min = 1024,
        .max = 0x7FFFFFFF,
        .def = 1024,
    },
	{
        /* integer array (3 elements - long, short, very short) */
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_HDR_GAINS,
        .type = V4L2_CTRL_TYPE_U32,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_hdr_gains",
        .step = 1,
        .min  = 1024,
        .max  = 0x7FFFFFFF,
        .dims = {3},
        .def = 1024,
    },
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_GAIN_LIMITS,
		.type = V4L2_CTRL_TYPE_U32,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_gain_limits",
		.step = 1,
		.min = 0,
		.max = 0xffffffff,
		.dims = { 2 },
	},
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_INTEGRATION_TIME_LIMITS,
		.type = V4L2_CTRL_TYPE_U32,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_integration_time_limits",
		.step = 1,
		.min = 0,
		.max = 0xffffffff,
		.dims = { 2 },
	},
	{
        /* float array 0.000001 ~ 0.999999, with 3 elements (long, short, very short) */
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_HDR_INTEGRATION_TIMES,
        .type = V4L2_CTRL_TYPE_U32,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_hdr_integration_times",
        .step = 1,
        .min = 1,
        .max = 999999,
        .dims = {3},
        .def = 1,
    },
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_HCG,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_hcg",
		.step = 1,
		.min = 0,
		.max = 1,
	},
    {
        /* float 1.00 ~ 100.00 */
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_STEP_FACTOR_LIMIT,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_step_factor_limit",
        .step = 1,
        .min  = 100,
        .max  = 10000,
        .def  = 240,
    },
    {
		/* float 1.00 ~ 255.00 */
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_SP_RATIO,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_sp_ratio",
        .step = 1,
        .min  = 100,
        .max  = 25500,
        .def  = 1600,
    },
    {
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_LEF_THRESHOLD,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_lef_threshold",
        .step = 1,
        .min  = 0,
        .max  = 255,
		.def  = 20,
    },
	{
		.ops = &hailo15_isp_aev2_ctrl_ops,
		.id = HAILO15_ISP_CID_AE_SEM_SETPOINT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_ae_sem_setpoint",
		.step = 1,
		.min = 0,
		.max = 255,
	},
	{
        .ops  = &hailo15_isp_aev2_ctrl_ops,
        .id   = HAILO15_ISP_CID_AE_DYNAMIC_RANGE,
        .type = V4L2_CTRL_TYPE_INTEGER,
        .flags= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
        .name = "isp_ae_dynamic_range",
        .step = 1,
        .min  = 0,
        .max  = 2,
        .def  = 0,
    },
};

int hailo15_isp_aev2_ctrl_count(void)
{
	return ARRAY_SIZE(hailo15_isp_aev2_ctrls);
}

int hailo15_isp_aev2_ctrl_create(struct hailo15_isp_device *isp_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hailo15_isp_aev2_ctrls); i++) {
		v4l2_ctrl_new_custom(&isp_dev->ctrl_handler,
				     &hailo15_isp_aev2_ctrls[i], NULL);
		if (isp_dev->ctrl_handler.error) {
			dev_err(isp_dev->dev,
				"reigster isp ae ctrl %s failed %d.\n",
				hailo15_isp_aev2_ctrls[i].name,
				isp_dev->ctrl_handler.error);
		}
	}

	return 0;
}
