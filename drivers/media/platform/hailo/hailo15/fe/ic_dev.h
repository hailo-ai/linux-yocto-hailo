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
#ifndef _IC_DEV_H_
#define _IC_DEV_H_

#ifndef __KERNEL__
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#define copy_from_user(a, b, c) isp_copy_data(a, b, c)
#define copy_to_user(a, b, c) isp_copy_data(a, b, c)

#if defined(HAL_CMODEL) || defined(HAL_ALTERA)
#include <hal/hal_api.h>
void ic_set_hal(HalHandle_t hal);
#endif
#endif

#include "isp_irq_queue.h"
#include "fe_dev.h"

#define IRQADDR(x) (pirqaddr->x##_offset)
#define UPDATESHADOWS 0

#ifdef ENABLE_IRQ
extern u32 ISP_BUF_GAP;
#define ISP_MI_PATH_ID_MAX 9
#endif

// #ifndef ISP_LSC_V2_TILE_MODE
// #define ISP_LSC_V2_TILE_MODE
// #endif

struct isp_reg_t {
	u32 offset;
	u32 val;
};

struct isp_reg_mem_info {
	u64 phy_base;
	u32 reg_size;
};

struct isp_misirq_regaddr {
	u32 isp_ctrl_offset;
	u32 isp_mis_offset;
	u32 isp_icr_offset;
	u32 isp_imsc_offset;

	u32 miv_mis_offset;
	u32 miv_icr_offset;
	u32 miv2_mis1_offset;
	u32 miv2_icr1_offset;
	u32 miv2_mis2_offset;
	u32 miv2_icr2_offset;
	u32 miv2_mis3_offset;
	u32 miv2_icr3_offset;
	u32 mi_mis_hdr1_offset;
	u32 mi_icr_hdr1_offset;
#ifdef ENABLE_IRQ
	u32 mi_status_offset;
#endif
};

struct isp_mi_data_path_context {
	bool enable;
	u32 out_mode;	   /**< output format */
	u32 in_mode;		/**< input format */
	u32 data_layout;	/**< layout of data */
	u32 data_alignMode;	/**< align mode of data */
	u32 in_width;
	u32 in_height;
	u32 out_width;
	u32 out_height;
	bool hscale;
	bool vscale;
	int pixelformat;
	u32 yuv_bit;
	u32 tilemode_crop_width;	//using for calculate llength,tilemode_crop_width = isp0-crop-outwidth + isp1-crop-outwidth, isp0 and isp1 are same.
	int mi_remainder;
};

#ifdef ENABLE_IRQ
struct isp_vvcam_mi_context {
	struct isp_mi_data_path_context path[ISP_MI_PATH_ID_MAX];
	u32 burst_len;
	u32 mi_path_num;
	bool tileMode;		//indicate whether if mi need to work in tile mode.
};

struct isp_mrv_mi_irq_mask_context {
	u32 FrameEndMask;
	u32 ErrorMask;
	u32 FifoFullMask;
};
#endif

struct isp_gamma_out_context {
	bool enableWB, enableGamma;
	bool changed;
	u32 mode;
	u32 curve[17];
};

struct isp_extmem_info {
	u64 addr;
	u64 size;
	u64 user_rmem_size;
};

struct isp_buffer_context {
	u32 type;
	u32 path;
	u32 addr_y, addr_cb, addr_cr;
	u32 size_y, size_cb, size_cr;
};

struct isp_miv_info {
	bool miv2_open;
	bool mis2_open;
};

struct isp_irq_data {
	uint32_t addr;
	uint32_t val;
	uint32_t nop[14];
};




struct vvcam_ic_dev {
	void __iomem *base;
	void __iomem *reset;
	void *rmem;
	u64 user_rmem_size;
	u64 phybase;
	struct device *dev;
#ifdef ISP8000NANO_V1802
	struct regmap *mix_gpr;
#endif
#if defined(__KERNEL__) && defined(ENABLE_IRQ)
	struct vvbuf_ctx *bctx;
	struct vb2_dc_buf *mi_buf[ISP_MI_PATH_ID_MAX];
	struct vb2_dc_buf *mi_buf_shd[ISP_MI_PATH_ID_MAX];
	int (*alloc) (struct vvcam_ic_dev *dev,
			  struct isp_buffer_context *buf);
	int (*free) (struct vvcam_ic_dev *dev, struct vb2_dc_buf *buf);
	int *state;
#endif
	void (*post_event) (struct vvcam_ic_dev *dev, void *data, size_t size);
#if UPDATESHADOWS
	struct isp_gamma_out_context gamma_out;
	bool update_gamma_en;
#endif
	struct isp_misirq_regaddr irqaddr;
	u32 isp_mis;
	int id;
#ifdef ENABLE_IRQ
	struct isp_vvcam_mi_context mi;
	struct isp_mrv_mi_irq_mask_context irq_mask;
#endif
	struct isp_miv_info miv_info;

	isp_mis_list_t circle_list;	//The irq circle list
	int set_mivirqaddr;

	struct vvcam_fe_dev *fe;//registered by fe driver
	int refcount;
};

void ic_write_reg(struct vvcam_ic_dev *dev, u32 offset, u32 val);
u32 ic_read_reg(struct vvcam_ic_dev *dev, u32 offset);
void ic_write_tbl(struct vvcam_ic_dev *dev, u32 offset, u32 val);

#endif /* _IC_DEV_H_ */
