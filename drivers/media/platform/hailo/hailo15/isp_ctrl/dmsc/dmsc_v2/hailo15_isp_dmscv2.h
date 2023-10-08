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

#ifndef __HAILO15_ISP_DMSC_V2_H__
#define __HAILO15_ISP_DMSC_V2_H__

#define HAILO15_ISP_CID_DMSC_ENABLE (HAILO15_ISP_CID_DMSC_BASE + 0x0000)
#define HAILO15_ISP_CID_DMSC_ENABLE_DEMOIRE (HAILO15_ISP_CID_DMSC_BASE + 0x0001)
#define HAILO15_ISP_CID_DMSC_ENABLE_DEPURPLE                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0002)
#define HAILO15_ISP_CID_DMSC_ENABLE_SHARPEN (HAILO15_ISP_CID_DMSC_BASE + 0x0003)
#define HAILO15_ISP_CID_DMSC_ENABLE_SHARPEN_LINE                               \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0004)
#define HAILO15_ISP_CID_DMSC_ENABLE_SKIN (HAILO15_ISP_CID_DMSC_BASE + 0x0005)
#define HAILO15_ISP_CID_DMSC_THRESHOLD (HAILO15_ISP_CID_DMSC_BASE + 0x0006)
#define HAILO15_ISP_CID_DMSC_DENOISE_STRENGTH                                  \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0007)
#define HAILO15_ISP_CID_DMSC_SHARPEN_SIZE (HAILO15_ISP_CID_DMSC_BASE + 0x0008)
#define HAILO15_ISP_CID_DMSC_INTERPOLAT_DIR_THR_MAX                            \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0009)
#define HAILO15_ISP_CID_DMSC_INTERPOLAT_DIR_THR_MIN                            \
	(HAILO15_ISP_CID_DMSC_BASE + 0x000A)
#define HAILO15_ISP_CID_DMSC_DEMOIRE_THR_T1 (HAILO15_ISP_CID_DMSC_BASE + 0x000B)
#define HAILO15_ISP_CID_DMSC_DEMOIRE_THR_T2_SHIFT                              \
	(HAILO15_ISP_CID_DMSC_BASE + 0x000C)
#define HAILO15_ISP_CID_DMSC_DEMOIRE_THR_R1 (HAILO15_ISP_CID_DMSC_BASE + 0x000D)
#define HAILO15_ISP_CID_DMSC_DEMOIRE_THR_R2 (HAILO15_ISP_CID_DMSC_BASE + 0x000E)
#define HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_R1                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x000F)
#define HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_R2                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0010)
#define HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_T1                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0011)
#define HAILO15_ISP_CID_DMSC_DEMOIRE_EDGE_T2_SHIFT                             \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0012)
#define HAILO15_ISP_CID_DMSC_DEMOIRE_AREA_THR                                  \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0013)
#define HAILO15_ISP_CID_DMSC_DEMOIRE_SAT_SHRINK                                \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0014)

#define HAILO15_ISP_CID_DMSC_SHARPEN_FAC_BLACK                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0015)
#define HAILO15_ISP_CID_DMSC_SHARPEN_FAC_WHITE                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0016)
#define HAILO15_ISP_CID_DMSC_SHARPEN_CLIP_BLACK                                \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0017)
#define HAILO15_ISP_CID_DMSC_SHARPEN_CLIP_WHITE                                \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0018)
#define HAILO15_ISP_CID_DMSC_SHARPEN_THR_T1 (HAILO15_ISP_CID_DMSC_BASE + 0x0019)
#define HAILO15_ISP_CID_DMSC_SHARPEN_THR_T2_SHIFT                              \
	(HAILO15_ISP_CID_DMSC_BASE + 0x001A)
#define HAILO15_ISP_CID_DMSC_SHARPEN_THR_T3 (HAILO15_ISP_CID_DMSC_BASE + 0x001B)
#define HAILO15_ISP_CID_DMSC_SHARPEN_THR_T4_SHIFT                              \
	(HAILO15_ISP_CID_DMSC_BASE + 0x001C)
#define HAILO15_ISP_CID_DMSC_SHARPEN_R1 (HAILO15_ISP_CID_DMSC_BASE + 0x001D)
#define HAILO15_ISP_CID_DMSC_SHARPEN_R2 (HAILO15_ISP_CID_DMSC_BASE + 0x001E)
#define HAILO15_ISP_CID_DMSC_SHARPEN_R3 (HAILO15_ISP_CID_DMSC_BASE + 0x001F)
#define HAILO15_ISP_CID_DMSC_SHARPEN_LINE_R1                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0020)
#define HAILO15_ISP_CID_DMSC_SHARPEN_LINE_R2                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0021)
#define HAILO15_ISP_CID_DMSC_SHARPEN_LINE_STRENGTH                             \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0022)
#define HAILO15_ISP_CID_DMSC_SHARPEN_LINE_SHIFT1                               \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0023)
#define HAILO15_ISP_CID_DMSC_SHARPEN_LINE_SHIFT2                               \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0024)
#define HAILO15_ISP_CID_DMSC_SHARPEN_LINE_T1                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0025)
#define HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_00                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0026)
#define HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_01                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0027)
#define HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_02                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0028)
#define HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_10                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0029)
#define HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_11                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x002A)
#define HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_12                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x002B)
#define HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_20                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x002C)
#define HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_21                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x002D)
#define HAILO15_ISP_CID_DMSC_SHARPEN_FILTER_22                                 \
	(HAILO15_ISP_CID_DMSC_BASE + 0x002E)
#define HAILO15_ISP_CID_DMSC_DEPURPLE_SAT_SHRINK                               \
	(HAILO15_ISP_CID_DMSC_BASE + 0x002F)
#define HAILO15_ISP_CID_DMSC_DEPURPLE_CBCR_MODE                                \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0030)
#define HAILO15_ISP_CID_DMSC_DEPURPLE_THRESHOLD                                \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0031)
#define HAILO15_ISP_CID_DMSC_SKIN_CB_THR_MAX                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0032)
#define HAILO15_ISP_CID_DMSC_SKIN_CB_THR_MIN                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0033)
#define HAILO15_ISP_CID_DMSC_SKIN_CR_THR_MAX                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0034)
#define HAILO15_ISP_CID_DMSC_SKIN_CR_THR_MIN                                   \
	(HAILO15_ISP_CID_DMSC_BASE + 0x0035)
#define HAILO15_ISP_CID_DMSC_SKIN_Y_THR_MAX (HAILO15_ISP_CID_DMSC_BASE + 0x0036)
#define HAILO15_ISP_CID_DMSC_SKIN_Y_THR_MIN (HAILO15_ISP_CID_DMSC_BASE + 0x0037)

int hailo15_isp_dmscv2_ctrl_count(void);
int hailo15_isp_dmscv2_ctrl_create(struct hailo15_isp_device *isp_dev);

#endif