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
#ifndef _ISP_IOC_H_
#define _ISP_IOC_H_
#include <linux/ioctl.h>
#include "ic_dev.h"

#define ISP_IOC_CMD_TYPE 0x100

enum {
	ISPIOC_GET_MIS = 0x100,
	ISPIOC_S_MIS_IRQADDR = 0x101,
	ISPIOC_D_MIS_IRQADDR = 0x102,
	ISPIOC_G_QUERY_EXTMEM = 0x103,
	ISPIOC_G_REG_PHYBASE = 0x104,
	ISPIOC_WRITE_REG = 0x105,
	ISPIOC_READ_REG = 0x106,
	ISPIOC_S_MIV_INFO = 0x107,
	ISPIOC_S_MI_PATCH_NUM = 0x108,
	ISPIOC_S_MI_IRQ_MASK = 0x109,
	ISPIOC_S_MP_34BIT = 0x10A,
	ISPIOC_WRITE_TBL = 0x1C0,
	ISPIOC_RST_QUEUE     	   = 0x1C1,
};


extern bool g_enable_mp_34bit;

#define CONFIG_VSI_ISP_DEBUG 0
#if CONFIG_VSI_ISP_DEBUG == 1
#define isp_info(fmt, ...)  pr_info(fmt, ##__VA_ARGS__)
#define isp_debug(fmt, ...)  pr_debug(fmt, ##__VA_ARGS__)
#define isp_err(fmt, ...)  pr_err(fmt, ##__VA_ARGS__)
#else
#define isp_info(fmt, ...)
#define isp_debug(fmt, ...)
#define isp_err(fmt, ...)  pr_err(fmt, ##__VA_ARGS__)
#endif

#ifdef __KERNEL__
int clean_dma_buffer(struct vvcam_ic_dev *dev);
irqreturn_t isp_hw_isr(int irq, void *data);
void isp_clear_interrupts(struct vvcam_ic_dev *dev);
#endif

long isp_priv_ioctl(struct vvcam_ic_dev *dev, unsigned int cmd, void __user *args);
long isp_copy_data(void *dst, void *src, int size);
int vvcam_isp_register_fe(struct vvcam_fe_dev *pfe_dev);
int vvcam_isp_unregister_fe(struct vvcam_fe_dev *pfe_dev);

/* internal functions, can called by v4l2 video device and ioctl */

#endif /* _ISP_IOC_H_ */
