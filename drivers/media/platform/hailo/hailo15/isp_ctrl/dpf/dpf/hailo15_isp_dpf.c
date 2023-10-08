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
#include "hailo15_isp_dpf.h"

static int hailo15_isp_dpf_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);

	switch (ctrl->id) {
	case HAILO15_ISP_CID_DPF_ENABLE:
	case HAILO15_ISP_CID_DPF_GREEN_SIGMA:
	case HAILO15_ISP_CID_DPF_RED_BLUE_SIGMA:
	case HAILO15_ISP_CID_DPF_GRADIENT:
	case HAILO15_ISP_CID_DPF_OFFSET:
	case HAILO15_ISP_CID_DPF_BOUND_MIN:
	case HAILO15_ISP_CID_DPF_DIVISION_FACTOR:
		ret = hailo15_isp_s_ctrl_event(isp_dev, isp_dev->ctrl_pad,
					       ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static int hailo15_isp_dpf_g_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);

	switch (ctrl->id) {
	case HAILO15_ISP_CID_DPF_ENABLE:
	case HAILO15_ISP_CID_DPF_GREEN_SIGMA:
	case HAILO15_ISP_CID_DPF_RED_BLUE_SIGMA:
	case HAILO15_ISP_CID_DPF_GRADIENT:
	case HAILO15_ISP_CID_DPF_OFFSET:
	case HAILO15_ISP_CID_DPF_BOUND_MIN:
	case HAILO15_ISP_CID_DPF_DIVISION_FACTOR:
		ret = hailo15_isp_g_ctrl_event(isp_dev, isp_dev->ctrl_pad,
					       ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static const struct v4l2_ctrl_ops hailo15_isp_dpf_ctrl_ops = {
	.s_ctrl = hailo15_isp_dpf_s_ctrl,
	.g_volatile_ctrl = hailo15_isp_dpf_g_ctrl,
};

const struct v4l2_ctrl_config hailo15_isp_dpf_ctrls[] = {
	{
		.ops = &hailo15_isp_dpf_ctrl_ops,
		.id = HAILO15_ISP_CID_DPF_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = " isp_dpf_enable",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_dpf_ctrl_ops,
		.id = HAILO15_ISP_CID_DPF_GREEN_SIGMA,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = " isp_dpf_green_sigma",
		.step = 1,
		.min = 1,
		.max = 255,
		.def = 1,
		.dims = { 1 },
	},
	{
		.ops = &hailo15_isp_dpf_ctrl_ops,
		.id = HAILO15_ISP_CID_DPF_RED_BLUE_SIGMA,
		.type = V4L2_CTRL_TYPE_U8,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = " isp_dpf_red_blue_sigma",
		.step = 1,
		.min = 1,
		.max = 255,
		.def = 1,
		.dims = { 1 },
	},
	{
		/* float 0.0 ~ 128.0 */
		.ops = &hailo15_isp_dpf_ctrl_ops,
		.id = HAILO15_ISP_CID_DPF_GRADIENT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = " isp_dpf_gradient",
		.step = 1,
		.min = 0,
		.max = 12800,
	},
	{
		/* float -128.0 ~ 128.0 */
		.ops = &hailo15_isp_dpf_ctrl_ops,
		.id = HAILO15_ISP_CID_DPF_OFFSET,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = " isp_dpf_offset",
		.step = 1,
		.min = -12800,
		.max = 12800,
	},
	{
		/* float 0.0 ~ 128.0 */
		.ops = &hailo15_isp_dpf_ctrl_ops,
		.id = HAILO15_ISP_CID_DPF_BOUND_MIN,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = " isp_dpf_bound_min",
		.step = 1,
		.min = 0,
		.max = 12800,
	},
	{
		/* float 0.0 ~ 64.0 */
		.ops = &hailo15_isp_dpf_ctrl_ops,
		.id = HAILO15_ISP_CID_DPF_DIVISION_FACTOR,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = " isp_dpf_division_factor",
		.step = 1,
		.min = 0,
		.max = 6400,
	},
};

int hailo15_isp_dpf_ctrl_count(void)
{
	return ARRAY_SIZE(hailo15_isp_dpf_ctrls);
}

int hailo15_isp_dpf_ctrl_create(struct hailo15_isp_device *isp_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hailo15_isp_dpf_ctrls); i++) {
		v4l2_ctrl_new_custom(&isp_dev->ctrl_handler,
				     &hailo15_isp_dpf_ctrls[i], NULL);
		if (isp_dev->ctrl_handler.error) {
			dev_err(isp_dev->dev,
				"reigster isp dpf ctrl %s failed %d.\n",
				hailo15_isp_dpf_ctrls[i].name,
				isp_dev->ctrl_handler.error);
		}
	}

	return 0;
}
