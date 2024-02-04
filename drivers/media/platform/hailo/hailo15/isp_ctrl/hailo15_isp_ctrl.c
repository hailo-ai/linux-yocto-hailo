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
#include "hailo15_isp_ctrl.h"

#include "af/afv1/hailo15_isp_afv1.h"
#include "ae/aev2/hailo15_isp_aev2.h"
#include "awb/awb_0410/hailo15_isp_awb0410.h"
#include "gc/gc_v2/hailo15_isp_gcv2.h"
#include "2dnr/2dnr/hailo15_isp_2dnr.h"
#include "wdr/wdr_v4/hailo15_isp_wdrv4.h"
#include "ee/ee/hailo15_isp_ee.h"
#include "cproc/cproc/hailo15_isp_cproc.h"
#include "ca/ca/hailo15_isp_ca.h"
#include "cac/cac/hailo15_isp_cac.h"
#include "3dnr/3dnr_v2/hailo15_isp_3dnrv2.h"
#include "bls/bls/hailo15_isp_bls.h"
#include "dg/dg/hailo15_isp_dg.h"
#include "dmsc/dmsc_v2/hailo15_isp_dmscv2.h"
#include "dpcc/dpcc/hailo15_isp_dpcc.h"
#include "dpf/dpf/hailo15_isp_dpf.h"
#include "lsc/lsc/hailo15_isp_lsc.h"
#include "wb/wb/hailo15_isp_wb.h"
#include "dci/dci/hailo15_isp_dci.h"
#include "hdr/hailo15_isp_hdr.h"

int hailo15_isp_ctrl_init(struct hailo15_isp_device *isp_dev)
{
	uint32_t ctrl_count = 0;

	ctrl_count += hailo15_isp_aev2_ctrl_count();
	ctrl_count += hailo15_isp_awb0410_ctrl_count();
	ctrl_count += hailo15_isp_gcv2_ctrl_count();
	ctrl_count += hailo15_isp_2dnr_ctrl_count();
	ctrl_count += hailo15_isp_wdrv4_ctrl_count();
	ctrl_count += hailo15_isp_ee_ctrl_count();
	ctrl_count += hailo15_isp_cproc_ctrl_count();
	ctrl_count += hailo15_isp_ca_ctrl_count();
	ctrl_count += hailo15_isp_cac_ctrl_count();
	ctrl_count += hailo15_isp_3dnrv2_ctrl_count();
	ctrl_count += hailo15_isp_bls_ctrl_count();
	ctrl_count += hailo15_isp_dg_ctrl_count();
	ctrl_count += hailo15_isp_dmscv2_ctrl_count();
	ctrl_count += hailo15_isp_dpcc_ctrl_count();
	ctrl_count += hailo15_isp_dpf_ctrl_count();
	ctrl_count += hailo15_isp_lsc_ctrl_count();
	ctrl_count += hailo15_isp_wb_ctrl_count();
	ctrl_count += hailo15_isp_dci_ctrl_count();
	ctrl_count += hailo15_isp_afv1_ctrl_count();
	ctrl_count += hailo15_isp_hdr_ctrl_count();

	pr_info("%s - calling v4l2_ctrl_handler_init with count %u\n", __func__,
		ctrl_count);
	v4l2_ctrl_handler_init(&isp_dev->ctrl_handler, ctrl_count);

	hailo15_isp_aev2_ctrl_create(isp_dev);
	hailo15_isp_awb0410_ctrl_create(isp_dev);
	hailo15_isp_gcv2_ctrl_create(isp_dev);
	hailo15_isp_2dnr_ctrl_create(isp_dev);
	hailo15_isp_wdrv4_ctrl_create(isp_dev);
	hailo15_isp_ee_ctrl_create(isp_dev);
	hailo15_isp_cproc_ctrl_create(isp_dev);
	hailo15_isp_ca_ctrl_create(isp_dev);
	hailo15_isp_cac_ctrl_create(isp_dev);
	hailo15_isp_3dnrv2_ctrl_create(isp_dev);
	hailo15_isp_bls_ctrl_create(isp_dev);
	hailo15_isp_dg_ctrl_create(isp_dev);
	hailo15_isp_dmscv2_ctrl_create(isp_dev);
	hailo15_isp_dpcc_ctrl_create(isp_dev);
	hailo15_isp_dpf_ctrl_create(isp_dev);
	hailo15_isp_lsc_ctrl_create(isp_dev);
	hailo15_isp_wb_ctrl_create(isp_dev);
	hailo15_isp_dci_ctrl_create(isp_dev);
	hailo15_isp_afv1_ctrl_create(isp_dev);
	hailo15_isp_hdr_ctrl_create(isp_dev);
	isp_dev->sd.ctrl_handler = &isp_dev->ctrl_handler;

	pr_info("%s - done\n", __func__);

	return 0;
}

int hailo15_isp_ctrl_destroy(struct hailo15_isp_device *isp_dev)
{
	v4l2_ctrl_handler_free(&isp_dev->ctrl_handler);

	return 0;
}
