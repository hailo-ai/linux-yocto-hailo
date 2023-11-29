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
#include "hailo15_isp_awb0410.h"

static int hailo15_isp_awb0410_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);

	switch (ctrl->id) {
	case HAILO15_ISP_CID_AWB_ENABLE:
	case HAILO15_ISP_CID_AWB_RESET:
	case HAILO15_ISP_CID_AWB_MODE:
	case HAILO15_ISP_CID_AWB_DAMPING:
	case HAILO15_ISP_CID_AWB_ILLUM_INDEX:
	case HAILO15_ISP_CID_AWB_ILLUM_PROFILES:
	case HAILO15_ISP_CID_AWB_ILLUM_NUM:
	case HAILO15_ISP_CID_AWB_RG_PROJ_MAX:
	case HAILO15_ISP_CID_AWB_SKY_RG_PROJ_MAX:
	case HAILO15_ISP_CID_AWB_INDOOR_RG_PROJ_MIN:
	case HAILO15_ISP_CID_AWB_OUTDOOR_RG_PROJ_MIN:
	case HAILO15_ISP_CID_AWB_MEASURE_WINDOWN:
		ret = hailo15_isp_s_ctrl_event(isp_dev, isp_dev->ctrl_pad,
					       ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static int hailo15_isp_awb0410_g_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);

	switch (ctrl->id) {
	case HAILO15_ISP_CID_AWB_ENABLE:
	case HAILO15_ISP_CID_AWB_RESET:
	case HAILO15_ISP_CID_AWB_MODE:
	case HAILO15_ISP_CID_AWB_DAMPING:
	case HAILO15_ISP_CID_AWB_ILLUM_INDEX:
	case HAILO15_ISP_CID_AWB_ILLUM_PROFILES:
	case HAILO15_ISP_CID_AWB_ILLUM_NUM:
	case HAILO15_ISP_CID_AWB_RG_PROJ_MAX:
	case HAILO15_ISP_CID_AWB_SKY_RG_PROJ_MAX:
	case HAILO15_ISP_CID_AWB_INDOOR_RG_PROJ_MIN:
	case HAILO15_ISP_CID_AWB_OUTDOOR_RG_PROJ_MIN:
	case HAILO15_ISP_CID_AWB_MEASURE_WINDOWN:
		ret = hailo15_isp_g_ctrl_event(isp_dev, isp_dev->ctrl_pad,
					       ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static const struct v4l2_ctrl_ops hailo15_isp_awb0410_ctrl_ops = {
	.s_ctrl = hailo15_isp_awb0410_s_ctrl,
	.g_volatile_ctrl = hailo15_isp_awb0410_g_ctrl,
};

const struct v4l2_ctrl_config hailo15_isp_awb0410_ctrls[] = {
	{
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_enable",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_RESET,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_reset",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_MODE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_mode",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_DAMPING,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_damping",
		.step = 1,
		.min = 0,
		.max = 1,
	},
	{
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_ILLUM_INDEX,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_illum_index",
		.step = 1,
		.min = 0,
		.max = 10,
	},
	{
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_ILLUM_PROFILES,
		.type = V4L2_CTRL_TYPE_STRING,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_illum_profiles",
		.step = 1,
		.max = 10 * 20 - 1,
	},
	{
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_ILLUM_NUM,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_illum_num",
		.step = 1,
		.min = 0,
		.max = 10,
	},
	{
		/*float 0 ~ 5.0 not sure */
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_RG_PROJ_MAX,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_rg_proj_max",
		.step = 1,
		.min = 0,
		.max = 500,
	},
	{
		/*float 0 ~ 5.0 */
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_SKY_RG_PROJ_MAX,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_sky_rg_proj_max",
		.step = 1,
		.min = 0,
		.max = 500,
	},
	{
		/*float 0 ~ 5.0 */
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_INDOOR_RG_PROJ_MIN,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_indoor_rg_proj_min",
		.step = 1,
		.min = 0,
		.max = 500,
	},
	{
		/*float 0 ~ 5.0 */
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_OUTDOOR_RG_PROJ_MIN,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_outdoor_rg_proj_min",
		.step = 1,
		.min = 0,
		.max = 500,
	},
	{
		/* uint16_t 4x array */
		.ops = &hailo15_isp_awb0410_ctrl_ops,
		.id = HAILO15_ISP_CID_AWB_MEASURE_WINDOWN,
		.type = V4L2_CTRL_TYPE_U16,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_EXECUTE_ON_WRITE,
		.name = "isp_awb_measure_window",
		.step = 1,
		.min = 0,
		.max = 0xFFFF,
		.dims = { 4 },
	},
};

int hailo15_isp_awb0410_ctrl_count(void)
{
	return ARRAY_SIZE(hailo15_isp_awb0410_ctrls);
}

int hailo15_isp_awb0410_ctrl_create(struct hailo15_isp_device *isp_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hailo15_isp_awb0410_ctrls); i++) {
		v4l2_ctrl_new_custom(&isp_dev->ctrl_handler,
				     &hailo15_isp_awb0410_ctrls[i], NULL);
		if (isp_dev->ctrl_handler.error) {
			dev_err(isp_dev->dev,
				"reigster isp awb ctrl %s failed %d.\n",
				hailo15_isp_awb0410_ctrls[i].name,
				isp_dev->ctrl_handler.error);
		}
	}

	return 0;
}
