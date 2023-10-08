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
#include "hailo15_isp_2dnr.h"

static int hailo15_isp_2dnr_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);

	switch (ctrl->id) {
	case HAILO15_ISP_CID_2DNR_ENABLE:
	case HAILO15_ISP_CID_2DNR_MODE:
	case HAILO15_ISP_CID_2DNR_PRE_GAMMA_STRENGTH:
	case HAILO15_ISP_CID_2DNR_STRENGTH:
	case HAILO15_ISP_CID_2DNR_SIGMA:
	case HAILO15_ISP_CID_2DNR_GAIN:
	case HAILO15_ISP_CID_2DNR_INTEGRATION:
		ret = hailo15_isp_s_ctrl_event(isp_dev, isp_dev->ctrl_pad,
					       ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static int hailo15_isp_2dnr_g_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);

	switch (ctrl->id) {
	case HAILO15_ISP_CID_2DNR_ENABLE:
	case HAILO15_ISP_CID_2DNR_MODE:
	case HAILO15_ISP_CID_2DNR_PRE_GAMMA_STRENGTH:
	case HAILO15_ISP_CID_2DNR_STRENGTH:
	case HAILO15_ISP_CID_2DNR_SIGMA:
	case HAILO15_ISP_CID_2DNR_GAIN:
	case HAILO15_ISP_CID_2DNR_INTEGRATION:
		ret = hailo15_isp_g_ctrl_event(isp_dev, isp_dev->ctrl_pad,
					       ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static const struct v4l2_ctrl_ops hailo15_isp_2dnr_ctrl_ops = {
	.s_ctrl = hailo15_isp_2dnr_s_ctrl,
	.g_volatile_ctrl = hailo15_isp_2dnr_g_ctrl,
};

const struct v4l2_ctrl_config hailo15_isp_2dnr_ctrls[] = {
	{
		.ops = &hailo15_isp_2dnr_ctrl_ops,
		.id = HAILO15_ISP_CID_2DNR_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_2dnr_enable",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		/* manual/auto */
		.ops = &hailo15_isp_2dnr_ctrl_ops,
		.id = HAILO15_ISP_CID_2DNR_MODE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_2dnr_mode",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_2dnr_ctrl_ops,
		.id = HAILO15_ISP_CID_2DNR_PRE_GAMMA_STRENGTH,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_2dnr_pre_gamma_strength",
		.step = 1,
		.min = 0,
		.max = 127,
	},
	{
		.ops = &hailo15_isp_2dnr_ctrl_ops,
		.id = HAILO15_ISP_CID_2DNR_STRENGTH,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_2dnr_strength",
		.step = 1,
		.min = 0,
		.max = 127,
	},
	{
		/* float 0.1 ~ 12.0*/
		.ops = &hailo15_isp_2dnr_ctrl_ops,
		.id = HAILO15_ISP_CID_2DNR_SIGMA,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_2dnr_sigma",
		.step = 1,
		.min = 10,
		.max = 1200,
		.def = 10,
	},
	{ /* float */
	  .ops = &hailo15_isp_2dnr_ctrl_ops,
	  .id = HAILO15_ISP_CID_2DNR_GAIN,
	  .type = V4L2_CTRL_TYPE_INTEGER,
	  .flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	  .name = "isp_2dnr_gain",
	  .step = 1,
	  .min = 0,
	  .max = 2000 },
	{ /* float */
	  .ops = &hailo15_isp_2dnr_ctrl_ops,
	  .id = HAILO15_ISP_CID_2DNR_INTEGRATION,
	  .type = V4L2_CTRL_TYPE_INTEGER,
	  .flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
	  .name = "isp_2dnr_integration_time",
	  .step = 1,
	  .min = 0,
	  .max = 2000 },
};

int hailo15_isp_2dnr_ctrl_count(void)
{
	return ARRAY_SIZE(hailo15_isp_2dnr_ctrls);
}

int hailo15_isp_2dnr_ctrl_create(struct hailo15_isp_device *isp_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hailo15_isp_2dnr_ctrls); i++) {
		v4l2_ctrl_new_custom(&isp_dev->ctrl_handler,
				     &hailo15_isp_2dnr_ctrls[i], NULL);
		if (isp_dev->ctrl_handler.error) {
			dev_err(isp_dev->dev,
				"reigster isp 2dnr ctrl %s failed %d.\n",
				hailo15_isp_2dnr_ctrls[i].name,
				isp_dev->ctrl_handler.error);
		}
	}

	return 0;
}
