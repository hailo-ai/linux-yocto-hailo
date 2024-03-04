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
#ifndef _ISP_FE_H
#define _ISP_FE_H

#include "ic_dev.h"
#define VIV_ISP_FE_DMA_TIME_DEBUG   0
enum {
	FEIOC_READ_REG = 0x200,
    FEIOC_WRITE_REG = 0x201,
    FEIOC_WRITE_TBL = 0x202,
	FEIOC_G_FE_STATUS = 0x203,
	FEIOC_S_FE_PARAMS = 0x204,
	FEIOC_S_FE_SWITCH = 0x205,
	FEIOC_S_FE_RESET  = 0x206,
	FEIOC_S_FE_FUSA_BUFF = 0x207,

};

int isp_fe_init(struct vvcam_fe_dev *dev);
int isp_fe_destory(struct vvcam_fe_dev *dev);
int isp_fe_read_reg(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t *val);
int isp_fe_write_reg(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t val);
int isp_fe_write_tbl(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t val);
int isp_fe_set_params(struct vvcam_fe_dev *dev, void __user *args);
int isp_fe_get_status(struct vvcam_fe_dev *dev, void __user *args);
int isp_fe_reset(struct vvcam_fe_dev *dev);

#endif
