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
#include "hailo15_isp_general.h"

static int hailo15_isp_general_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return -ENOTSUPP;
}

static int hailo15_isp_general_g_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	int sink_pad;
	struct hailo15_isp_device *isp_dev = container_of(
		ctrl->handler, struct hailo15_isp_device, ctrl_handler);
	sink_pad = HAILO15_ISP_SOURCE_PAD_TO_ISP_SINK_PAD(isp_dev->ctrl_pad);
	if (sink_pad < 0) {
		pr_err("%s - invalid sink pad: %d, isp_dev->ctrl_pad: %d\n", __func__, sink_pad, isp_dev->ctrl_pad);
		return -EINVAL;
	}

	switch (ctrl->id) {
	case HAILO15_ISP_CID_GENERAL_STREAMING:
		pr_debug("%s - got g_ctrl with id: 0x%x\n", __func__, ctrl->id);
		ctrl->val = atomic_read(&isp_dev->streaming_started[sink_pad]);
		break;

	case HAILO15_ISP_CID_GENERAL_3A_UNIX_EPOCH:
		ret = hailo15_isp_g_ctrl_event(isp_dev, sink_pad, ctrl);
		break;

	default:
		dev_err(isp_dev->dev, "unknow v4l2 ctrl id %d\n", ctrl->id);
		return -EACCES;
	}

	return ret;
}

static const struct v4l2_ctrl_ops hailo15_isp_general_ctrl_ops = {
	.s_ctrl = hailo15_isp_general_s_ctrl,
	.g_volatile_ctrl = hailo15_isp_general_g_ctrl,
};

static const struct v4l2_ctrl_config hailo15_isp_general_ctrls[] = {
	{
		.ops = &hailo15_isp_general_ctrl_ops,
		.id = HAILO15_ISP_CID_GENERAL_STREAMING,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.flags = V4L2_CTRL_FLAG_VOLATILE |
			 V4L2_CTRL_FLAG_READ_ONLY,
		.name = "isp_general_streaming",
		.step = 1,
		.min = 0,
		.max = 1,
	},
    {
        .ops = &hailo15_isp_general_ctrl_ops,
        .id = HAILO15_ISP_CID_GENERAL_3A_UNIX_EPOCH,
        .type = V4L2_CTRL_TYPE_U32,
        .flags = V4L2_CTRL_FLAG_VOLATILE |
                V4L2_CTRL_FLAG_READ_ONLY,
        .name = "isp_general_3a_unix_epoch",
        .step = 1,
        .min = 0,
        .max = 4294967295,
        .dims = { 1 },
    }
};

int hailo15_isp_general_ctrl_count(void)
{
	return ARRAY_SIZE(hailo15_isp_general_ctrls);
}

int hailo15_isp_general_ctrl_create(struct hailo15_isp_device *isp_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hailo15_isp_general_ctrls); i++) {
		v4l2_ctrl_new_custom(&isp_dev->ctrl_handler,
				     &hailo15_isp_general_ctrls[i], NULL);
		if (isp_dev->ctrl_handler.error) {
			dev_err(isp_dev->dev,
				"reigster isp general ctrl %s failed %d.\n",
				hailo15_isp_general_ctrls[i].name,
				isp_dev->ctrl_handler.error);
		}
	}

	return 0;
}
