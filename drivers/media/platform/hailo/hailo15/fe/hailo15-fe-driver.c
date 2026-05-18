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

#include <asm/io.h>

#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/mm.h>

#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/rwsem.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>

#include "isp_ioctl.h"
#include "isp_fe.h"
#include "../hailo15-isp-hw-defs.h"

#define VIVCAM_FE_NAME "vivfe"
#define VIVCAM_FE_MAXCNT 2

#ifndef ISP_MIV2
#define ISP_MIV2
#endif

#define VI_IRCL_OFFSET 0x0014
#define VI_DPCL_OFFSET 0x0018
#define VI_ISP_IMSC_OFFSET 0x5bc
#define VI_MI_IMSC_OFFSET 0x16c0
#define VIV_ISP_FE_DMA_TIMOUT_MS	1000
#define VIV_ISP_FE_DEBUG_DUMP		0
#define VIV_ISP_FE_UNUSED_OFFSET	((uint32_t)-1)

static bool fe0_enable = true;
module_param(fe0_enable, bool, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(fe0_enable, "FE0(command buffer) enable for ISP");

static bool fe1_enable = false;
module_param(fe1_enable, bool, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(fe1_enable, "FE1(command buffer) enable for ISP");

static struct vvcam_fe_dev* fe_dev;

/*ctrl_regs must be sorted in ascending order*/
static uint16_t ctrl_regs[] = {
	0x0200, 0x0300, 0x0400, 0x0680, 0x0700, 0x0750, 0x0800, 0x0c00,
	0x1000, 0x1100, 0x1300, 0x1310, 0x142c, 0x14e4, 0x1600,
	0x2000, 0x2200, 0x2300, 0x2580, 0x2600, 0x26A0, 0x2700, 0x295c,
	0x2a00, 0x2db0, 0x2e00, 0x3000, 0x3100, 0x3200, /*0x3300,*/ 0x3500,
	0x3700, 0x3788, 0x3800, 0x3900, 0x391C, 0x3A00, 0x3E00, 0x4000,
	0x4600, 0x4b00, 0x5000, 0x5100, 0x5300, 0x5500, 0x5600,
	0x5A00, 0x5B00, 0x5B04, 0x5B08, 0x5B0C, 0x5B10, 0x5B14, 0x5B18,
	0x5B1C, 0x5b20, 0x5b40, 0x5Bf4,
	0x5D00, 0x5F00, 0x6000, 0x6100, 0x6300, 0x6304, 0x6400, 0x6700,
	0x6800, 0x6a00, 0x6a80, 0x6b00, 0x6d00, 0x6d80, 0x6E80, 0x7000,
	0x7100, 0x7700
};

struct vvcam_fe_driver_dev {
	struct cdev cdev;
	dev_t devt;
	struct class *class;
	struct mutex vvmutex;
	unsigned int device_idx;
	void *private;
};

static unsigned int vvcam_fe_major;
static unsigned int vvcam_fe_minor;
static struct class *vvcam_fe_class;
static unsigned int fe_register_index;


// forward declaration
static void isp_fe_perform_post_fe_writes(struct vvcam_fe_dev *dev);
static int isp_fe_clean_wo_bits(struct vvcam_fe_dev *dev);
static int isp_fe_read_vdid_reg_from_full_cmd_buf(struct isp_fe_context *fe, uint8_t vdid, uint32_t offset, uint32_t *val);
static int isp_fe_write_vdid_reg_to_full_cmd_buf(struct isp_fe_context *fe, uint8_t vdid, uint32_t offset, uint32_t val);

static void isp_fe_raw_write_reg(struct vvcam_fe_dev *dev, u32 offset, u32 val)
{
	if (offset >= ISP_FE_REG_OFFSET_BYTE_MAX)
		return;
	__raw_writel(val, dev->base + offset);
	//isp_info("%s	addr 0x%08x val 0x%08x\n", __func__, offset, val);
}

static uint32_t isp_fe_raw_read_reg(struct vvcam_fe_dev *dev, u32 offset)
{
	uint32_t val = 0;

	if (offset >= ISP_FE_REG_OFFSET_BYTE_MAX)
		return 0;

	val = __raw_readl(dev->base + offset);
	//isp_info("%s addr 0x%08x val 0x%08x\n", __func__, offset, val);

	return val;
}

static int isp_fe_get_full_cmdbuf_cmd(struct isp_fe_context *fe, uint32_t offset,
			      	      uint8_t vdid, union isp_fe_cmd_u **out_cmd)
{
	uint32_t mapped_index;

	if (!fe || !out_cmd) {
		pr_err("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (vdid >= fe->vdid_num) {
		pr_err("%s: invalid vdid %d\n", __func__, vdid);
		return -EINVAL;
	}

	if (offset < ISP_FE_REG_OFFSET_BYTE_MAX) {
		mapped_index = (uint32_t)fe->hash_map_tbl[offset / ISP_FE_REG_SIZE];
	} else {
		mapped_index = offset / ISP_FE_REG_SIZE;
	}

	/* Validate mapped_index is within buffer bounds */
	if (mapped_index >= fe->fe_buff[vdid].refresh_full_regs.cmd_num_max) {
		pr_err("%s: mapped_index %u out of bounds (max %u) for offset 0x%x\n",
		       __func__, mapped_index, fe->fe_buff[vdid].refresh_full_regs.cmd_num_max, offset);
		return -EINVAL;
	}

	*out_cmd = &fe->fe_buff[vdid].refresh_full_regs.cmd_buffer[mapped_index];
	return 0;
}

#if VIV_ISP_FE_DEBUG_DUMP == 1
static void isp_fe_cmdbuffer_dump(struct isp_fe_cmd_buffer_t *refresh_regs)
{
	int j;
	isp_info("enter %s\n", __func__);
	isp_info("curr_cmd_num=%x\n", refresh_regs->curr_cmd_num);
	for (j = 0; j < refresh_regs->curr_cmd_num; j++) {
		if (refresh_regs->cmd_buffer[j].cmd_wreg.fe_cmd.cmd.op_code ==
			ISP_FE_CMD_REG_WRITE) {
			isp_info("0x%04x=0x%08x\n",
				refresh_regs->cmd_buffer[j].cmd_wreg.fe_cmd.cmd.start_reg_addr,
				refresh_regs->cmd_buffer[j].cmd_wreg.w_data);
		}
	}
	isp_info("\n");
}

static void isp_fe_cmdbuffer_parse(struct isp_fe_cmd_buffer_t *refresh_regs)
{
	union isp_fe_cmd_u *cmd_buffer;
	uint32_t i;
	isp_info("enter %s\n", __func__);

	cmd_buffer = refresh_regs->cmd_buffer;

	for (i = 0; i < refresh_regs->curr_cmd_num; i++) {
		switch (cmd_buffer[i].cmd_wreg.fe_cmd.cmd.op_code) {
		case ISP_FE_CMD_REG_WRITE:
			isp_info("off %04x: WR 0x%04x len=%d all=0x%08x data=0x%08x\n",
				i,
				cmd_buffer[i].cmd_wreg.fe_cmd.cmd.start_reg_addr,
				cmd_buffer[i].cmd_wreg.fe_cmd.cmd.length,
				cmd_buffer[i].cmd_wreg.fe_cmd.all,
				cmd_buffer[i].cmd_wreg.w_data);
			break;
		case ISP_FE_CMD_END:
			isp_info("off %04x: END all=0x%08x\n", i, cmd_buffer[i].cmd_end.fe_cmd.all);
			break;
		case ISP_FE_CMD_NOP:
			isp_info("off %04x: NOP all=0x%08x data=0x%08x\n", i,
				cmd_buffer[i].cmd_nop.fe_cmd.all, cmd_buffer[i].cmd_nop.rsv1);
			break;
		default:
			isp_info("off %04x: ERR all=0x%08x\n", i, cmd_buffer[i].cmd_wreg.fe_cmd.all);
			break;
		}
	}
}
#endif

static void isp_fe_buffer_memset(union isp_fe_cmd_u *cmd_buffer, u32 buffer_num)
{
	int32_t i;
	struct isp_fe_cmd_nop_t cmd_nop;
	if (cmd_buffer) {
		cmd_nop.fe_cmd.cmd.rsv0 = 0;
		cmd_nop.fe_cmd.cmd.op_code = ISP_FE_CMD_NOP;
		cmd_nop.rsv1 = 0;
		for (i = 0; i < buffer_num; i++) {
			cmd_buffer[i].cmd_nop = cmd_nop;
		}
	}
}

static inline void isp_fe_fill_write_cmd(struct isp_fe_cmd_wreg_t *cmd, uint32_t offset, uint32_t val)
{
	cmd->fe_cmd.cmd.start_reg_addr = offset;
	cmd->fe_cmd.cmd.length = 1;
	cmd->fe_cmd.cmd.rsv = 0;
	cmd->fe_cmd.cmd.op_code = ISP_FE_CMD_REG_WRITE;
	cmd->w_data = val;
}

static int isp_fe_get_reg_def_val(struct vvcam_fe_dev *dev)
{
	uint32_t i;
	uint8_t vdid;
	uint32_t offset, ctrl_index = 0;
	uint32_t ctrl_num = ARRAY_SIZE(ctrl_regs);
	struct isp_fe_context *fe = &dev->fe;
	union isp_fe_cmd_u *cmd;
	int ret;
	isp_info("enter %s\n", __func__);

	for (vdid = 0; vdid < fe->vdid_num; vdid++) {
		ctrl_index = 0;
		for (i = 0; i < ISP_FE_REG_NUM; i++) {
			//read the default register value from isp hardware
			//before isp startup
			offset = i * ISP_FE_REG_SIZE;
			// Skip reading the histogram value because all 256 histogram values are written to the same register.
			// Reading this register would require 256 consecutive reads to ensure the index matches the value.
			if (offset == fe->general_ctrl.isp_hist) {
				continue;
			}

			fe->reg_buffer[i] = isp_fe_raw_read_reg(dev, offset);

			ret = isp_fe_get_full_cmdbuf_cmd(fe, offset, vdid, &cmd);
			if (ret) {
				pr_err("%s: invalid cmd for offset 0x%x during init, ret=%d\n", __func__, offset, ret);
				return ret;
			}
			/*set the ctrl registers with default value */
			if (ctrl_index < ctrl_num && ctrl_regs[ctrl_index] == offset) {
				isp_fe_fill_write_cmd(&cmd->cmd_wreg, offset, fe->reg_buffer[i]);
				ctrl_index++;
			} else {
				// for non-control, we want the data to be correct when reading
				// without setting the command to a write command
				cmd->cmd_wreg.w_data = fe->reg_buffer[i];
			}

		}

		//set lut register to nop in full cmd buffer
		for (i = 0; i < fe->fe_buff[vdid].tbl_reg_addr_num; i++) {
			ret = isp_fe_get_full_cmdbuf_cmd(fe, fe->fe_buff[vdid].tbl_buffer[i].tbl_reg_offset, vdid, &cmd);
			if (ret) {
				pr_err("%s: invalid cmd for LUT offset 0x%x, ret=%d\n", __func__,
				       fe->fe_buff[vdid].tbl_buffer[i].tbl_reg_offset, ret);
				return ret;
			}
			isp_fe_buffer_memset(cmd, 1);
		}
#if VIV_ISP_FE_DEBUG_DUMP == 1
		isp_fe_cmdbuffer_dump(&fe->fe_buff[vdid].refresh_full_regs);
		//isp_fe_cmdbuffer_parse(&fe->fe_buff[vdid].refresh_full_regs);
#endif
	}

	return 0;
}

static int is_start_fe_dma(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t val, bool *out_start)
{				//init start or MCM start
	int ret = 0;
	bool en_updated = false;
	struct isp_fe_context *fe = &dev->fe;
	uint32_t mi_ctrl_val;

	if (vdid >= fe->vdid_num) {
		pr_err("%s error: vdid %d is out of range (max=%d)\n", __func__, vdid, fe->vdid_num);
		return -EINVAL;
	}
//		  isp_info("enter %s\n", __func__);

//Case 1: normal
	if (fe->general_ctrl.isp_ctrl == offset &&
		(val & fe->general_ctrl.isp_ctrl_mask[0]) ==
		fe->general_ctrl.isp_ctrl_mask[0]) {
		fe->fe_buff[vdid].isp_ctrl_enable = 1;
		en_updated = true;
	}
#if defined(ISP_MIV2)
	if (fe->general_ctrl.mi_imsc3 == offset &&
		(val & fe->general_ctrl.mi_imsc3_mask) == fe->general_ctrl.mi_imsc3_mask) {	//enable irq
		ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, fe->general_ctrl.mi_ctrl, &mi_ctrl_val);
		if (ret) {
			pr_err("%s: reading mi_ctrl (0x%x) failed, ret=%d\n", __func__, fe->general_ctrl.mi_ctrl, ret);
			return ret;
		}
		if (mi_ctrl_val != 0) {
			fe->fe_buff[vdid].isp_mi_enable = 1;
			en_updated = true;
		}
	}
#elif defined(ISP_MIV1)
	if (fe->general_ctrl.mi_init == offset &&
		(val & fe->general_ctrl.mi_init_mask) ==
		fe->general_ctrl.mi_init_mask) {
		ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, fe->general_ctrl.mi_ctrl, &mi_ctrl_val);
		if (ret) {
			pr_err("%s: reading mi_ctrl (0x%x) failed, ret=%d\n", __func__, fe->general_ctrl.mi_ctrl, ret);
			return ret;
		}
		if ((mi_ctrl_val & fe->general_ctrl.mi_ctrl_mask) != 0) {
			fe->fe_buff[vdid].isp_mi_enable = 1;	//disable in stop function
			en_updated = true;
		}
	}
#endif

	if (en_updated == true &&
		fe->fe_buff[vdid].isp_ctrl_enable == 1 &&
		fe->fe_buff[vdid].isp_mi_enable == 1) {
		*out_start = true;
		return 0;
	}
//case 3: TPG
	if (fe->general_ctrl.isp_tpg_ctrl == offset &&
		(val & fe->general_ctrl.isp_tpg_ctrl_mask) ==
		fe->general_ctrl.isp_tpg_ctrl_mask) {
		*out_start = true;
		return 0;
	}

	*out_start = false;
	return 0;
}

static int is_stop_isp(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t val, bool *out_stop)
{				//stop
	bool en_updated = false;
	struct isp_fe_context *fe = &dev->fe;
#if defined(ISP_MIV1)
	int ret;
	uint32_t mi_ctrl_val;
#endif

	if (vdid >= fe->vdid_num) {
		pr_err("%s error: vdid %d is out of range (max=%d)\n", __func__, vdid, fe->vdid_num);
		return -EINVAL;
	}

//Case 1: normal
	if (fe->general_ctrl.isp_ctrl == offset &&
		(val & fe->general_ctrl.isp_ctrl_mask[0]) !=
		fe->general_ctrl.isp_ctrl_mask[0]) {
		fe->fe_buff[vdid].isp_ctrl_enable = 0;
		en_updated = true;
	}
#if defined(ISP_MIV2)
	if (fe->general_ctrl.mi_ctrl == offset && val == 0x00) {
		fe->fe_buff[vdid].isp_mi_enable = 0;
		en_updated = true;
	}
#elif defined(ISP_MIV1)
	if (fe->general_ctrl.mi_init == offset &&
		(val & fe->general_ctrl.mi_init_mask) ==
		fe->general_ctrl.mi_init_mask) {
		ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, fe->general_ctrl.mi_ctrl, &mi_ctrl_val);
		if (ret) {
			pr_err("%s: reading mi_ctrl (0x%x) failed, ret=%d\n", __func__, fe->general_ctrl.mi_ctrl, ret);
			return ret;
		}
		if ((mi_ctrl_val & fe->general_ctrl.mi_ctrl_mask) == 0) {
			fe->fe_buff[vdid].isp_mi_enable = 0;
			en_updated = true;
		}
	}
#endif

	if (en_updated == true &&
		fe->fe_buff[vdid].isp_ctrl_enable == 0 &&
		fe->fe_buff[vdid].isp_mi_enable == 0) {
		*out_stop = true;
		return 0;
	}
//case 3: TPG
	if (fe->general_ctrl.isp_tpg_ctrl == offset &&
		(val & fe->general_ctrl.isp_tpg_ctrl_mask) == 0) {
		*out_stop = true;
		return 0;
	}

	*out_stop = false;
	return 0;
}

bool isp_fe_is_non_fe_control_register(uint32_t offset)
{
	return (MIV2_MIS == offset || MIV2_ICR == offset ||
		MIV2_MIS1 == offset || MIV2_ICR1 == offset ||
		MIV2_MIS2 == offset || MIV2_ICR2 == offset ||
		MIV2_MIS3 == offset || MIV2_ICR3 == offset ||
		ISP_MIS == offset || ISP_ICR == offset ||
		MIPI_MIS == offset || MIPI_ICR == offset) ||
		(offset >= 0x1200 && offset <= 0x1294) ||
		(offset >= 0x1610 && offset <= 0x1668) ||
		(offset >= 0x3300 && offset <= 0x33FC) ||
		(offset >= 0x500c && offset <= 0x5060) ||
		(offset >= 0x5700 && offset <= 0x5894) ||
		(offset >= 0x5900 && offset <= 0x5924) ||
		(offset >= 0x5980 && offset <= 0x59A4);
}
EXPORT_SYMBOL(isp_fe_is_non_fe_control_register);

bool isp_fe_is_fe_control_register(uint32_t offset)
{
	// @hailo - reading fe_ctrl is only for debugging purposes
	return FE_MIS == offset || FE_ICR == offset || FE_CTRL == offset;
}
EXPORT_SYMBOL(isp_fe_is_fe_control_register);

bool isp_fe_is_control_register(uint32_t offset)
{
	return isp_fe_is_non_fe_control_register(offset) || isp_fe_is_fe_control_register(offset);
}
EXPORT_SYMBOL(isp_fe_is_control_register);

bool isp_fe_is_vdid_register(uint32_t offset)
{
	return !isp_fe_is_control_register(offset);
}
EXPORT_SYMBOL(isp_fe_is_vdid_register);

static bool isp_fe_is_status_reg(struct isp_fe_context *fe, uint32_t offset)
{
	uint32_t i, reg_index, reg_offset;

	for (i = 0; i < (uint32_t)fe->status_regs_num; i++) {
		reg_offset = fe->status_regs[i].reg_base;
		for (reg_index = 0; reg_index < fe->status_regs[i].reg_num; reg_index++) {
			if (offset == reg_offset)
				return true;
			reg_offset += fe->status_regs[i].reg_step;
		}
	}
	return false;
}

static void __isp_fe_read_control_reg(struct vvcam_fe_dev *dev, uint32_t offset, uint32_t *val)
{
	//read register directly
	*val = isp_fe_raw_read_reg(dev, offset);

	// NOTE: originally reading CPU registers would also update
	// their copy in the command buffer. We removed it since these are control
	// registers and should not be in the FE command buffer in the first place.
}

static int __isp_fe_read_vdid_reg(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t *val)
{
	struct isp_fe_context *fe = &dev->fe;
	uint32_t hw_val;
	int ret;


	/* Histogram special handling */
	if (offset == fe->general_ctrl.isp_hist && offset != VIV_ISP_FE_UNUSED_OFFSET) {
		if (vdid == fe->curr_vdid && fe->fe_buff[vdid].rd_index == 0) {
			/* First read (index 0) on current VDID: read all N values from HW */
			uint32_t i;

			for (i = 0; i < fe->fe_buff[vdid].fixed_reg_rd_num; i++)
				fe->fe_buff[vdid].fixed_reg_buffer[i] =
					isp_fe_raw_read_reg(dev, fe->general_ctrl.isp_hist);
		}
		/* Return from fixed_reg_buffer (just populated if index was 0 and current VDID) */
		*val = fe->fe_buff[vdid].fixed_reg_buffer[fe->fe_buff[vdid].rd_index++];
		if (fe->fe_buff[vdid].rd_index == fe->fe_buff[vdid].fixed_reg_rd_num)
			fe->fe_buff[vdid].rd_index = 0;
		return 0;
	}

	if (vdid == fe->curr_vdid && isp_fe_is_status_reg(fe, offset)) {
		/* Status register on current VDID: read from HW, update command buffer */
		hw_val = isp_fe_raw_read_reg(dev, offset);
		ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, offset, hw_val);
		if (ret) {
			pr_err("%s: failed to update status reg 0x%x in cmd buf, ret=%d\n", __func__, offset, ret);
			return ret;
		}
	}

	return isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, offset, val);
}

static void __isp_fe_write_control_reg(struct vvcam_fe_dev *dev, uint32_t offset, uint32_t val)
{
	isp_fe_raw_write_reg(dev, offset, val);

	//MCM+FE+HDR workaroud.
	if (offset == 0x5700) {
		mdelay(1);
	}
}

static int isp_fe_write_vdid_reg_to_full_cmd_buf(struct isp_fe_context *fe, uint8_t vdid, uint32_t offset, uint32_t val)
{
	int ret;
	union isp_fe_cmd_u *cmd;

	ret = isp_fe_get_full_cmdbuf_cmd(fe, offset, vdid, &cmd);
	if (ret) {
		pr_err("%s: invalid cmd for offset 0x%x, ret=%d\n", __func__, offset, ret);
		return ret;
	}
	isp_fe_fill_write_cmd(&cmd->cmd_wreg, offset, val);
	return 0;
}

static int isp_fe_read_vdid_reg_from_full_cmd_buf(struct isp_fe_context *fe, uint8_t vdid, uint32_t offset, uint32_t *val)
{
	union isp_fe_cmd_u *cmd;
	int ret;

	ret = isp_fe_get_full_cmdbuf_cmd(fe, offset, vdid, &cmd);
	if (ret) {
		pr_err("%s: invalid cmd for offset 0x%x, ret=%d\n", __func__, offset, ret);
		return ret;
	}
	*val = cmd->cmd_wreg.w_data;

	return 0;
}

static int isp_fe_write_vdid_reg_to_partial_cmd_buf(struct isp_fe_context *fe, uint8_t vdid, uint32_t offset, uint32_t val)
{
	struct isp_fe_cmd_buffer_t *part_cmd_buffer;

	if (vdid >= fe->vdid_num) {
		pr_err("%s error: vdid %d is out of range (max=%d)\n", __func__, vdid, fe->vdid_num);
		return -EINVAL;
	}

	part_cmd_buffer = &fe->fe_buff[vdid].refresh_part_regs;

	if (part_cmd_buffer->curr_cmd_num >= ISP_FE_REG_PART_REFRESH_NUM) {
		// partial buffer is used until full.
		// if it is full, full command buffer is used instead.
		return 0;
	}
	isp_fe_fill_write_cmd(&part_cmd_buffer->cmd_buffer[part_cmd_buffer->curr_cmd_num].cmd_wreg, offset, val);
	part_cmd_buffer->curr_cmd_num++;
	return 0;
}

static int __isp_fe_write_tbl(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t val)
{
	int ret = 0;
	struct isp_fe_cmd_wreg_t *tbl_cmd_buffer;
	struct isp_fe_context *fe = &dev->fe;
	uint8_t tbl_index;
	uint16_t params_index;

	tbl_index = fe->fe_buff[vdid].prev_index;
	do {//optimize the search algorithm
		if (fe->fe_buff[vdid].tbl_buffer[tbl_index].tbl_reg_offset == offset) {
			tbl_cmd_buffer = fe->fe_buff[vdid].tbl_buffer[tbl_index].tbl_cmd_buffer;
			params_index = fe->fe_buff[vdid].tbl_buffer[tbl_index].curr_index++;
			if (fe->fe_buff[vdid].tbl_buffer[tbl_index].curr_index >=
				fe->fe_buff[vdid].tbl_buffer[tbl_index].params_num) {
				fe->fe_buff[vdid].tbl_buffer[tbl_index].curr_index = 0;
			}
			isp_fe_fill_write_cmd(&tbl_cmd_buffer[params_index], offset, val);
			fe->fe_buff[vdid].prev_index = tbl_index;	//for next time search
			ret = isp_fe_write_vdid_reg_to_partial_cmd_buf(fe, vdid, offset, val);
			if (ret) {
				pr_err("%s error: failed to write to partial cmd buffer for vdid %d, offset 0x%08x\n", __func__, vdid, offset);
				return ret;
			}
			break;
		}
		tbl_index++;	//(tbl_index + 1) % fe->tbl_reg_addr_num;
		if (tbl_index == fe->fe_buff[vdid].tbl_reg_addr_num) {
			tbl_index = 0;
		}
	} while (tbl_index != fe->fe_buff[vdid].prev_index);

	return ret;
}

static void isp_fe_buffer_append_end(union isp_fe_cmd_u *cmd_buffer, u32 *curr_num)
{
	uint8_t append_num = 0;
	if ((*curr_num & 0x01) == 0) {
		cmd_buffer[0].cmd_nop.fe_cmd.cmd.rsv0 = 0;
		cmd_buffer[0].cmd_nop.fe_cmd.cmd.op_code = ISP_FE_CMD_NOP;
		cmd_buffer[0].cmd_nop.rsv1 = 0;
		*curr_num = *curr_num + 1;
		append_num = 1;
	}
	cmd_buffer[append_num].cmd_end.fe_cmd.cmd.event_id = 0x1A;
	cmd_buffer[append_num].cmd_end.fe_cmd.cmd.rsv0 = 0;
	cmd_buffer[append_num].cmd_end.fe_cmd.cmd.event_enable = 1;
	cmd_buffer[append_num].cmd_end.fe_cmd.cmd.rsv1 = 0;
	cmd_buffer[append_num].cmd_end.fe_cmd.cmd.op_code = ISP_FE_CMD_END;
	cmd_buffer[append_num].cmd_end.rsv2 = 0;
	*curr_num = *curr_num + 1;
}

static void isp_fe_shd_to_immediately(struct isp_fe_context *fe, u32 offset, u32 *val)
{
	if (offset == fe->general_ctrl.isp_ctrl) {
		if ((*val) & (fe->general_ctrl.isp_ctrl_mask[1] | fe->general_ctrl.isp_ctrl_mask[2])) {
			*val &= ~(fe->general_ctrl.isp_ctrl_mask[1] | fe->general_ctrl.isp_ctrl_mask[2]);
			*val |= fe->general_ctrl.isp_ctrl_mask[3];
		}
	} else if (offset == fe->general_ctrl.isp_stitching_ctrl ||
				offset == fe->general_ctrl.isp_stitching_ctrl1 ||
				offset == fe->general_ctrl.isp_stitching_ctrl2 ||
				offset == fe->general_ctrl.isp_stitching_ctrl3) {	//isp_stitching0_ctrl ~ isp_stitching3_ctrl
		if ((*val) & (fe->general_ctrl.stitching_ctrl_mask[0] | fe->general_ctrl.stitching_ctrl_mask[1])) {
			*val &= ~(fe->general_ctrl.stitching_ctrl_mask[0] | fe->general_ctrl.stitching_ctrl_mask[1]);
			*val |= fe->general_ctrl.stitching_ctrl_mask[2];
		}
	} else if (offset == fe->general_ctrl.mi_hdr_ctrl) {
		if (*val & fe->general_ctrl.mi_hdr_ctrl_mask[0]) {
			*val &= ~fe->general_ctrl.mi_hdr_ctrl_mask[0];
			*val |= (fe->general_ctrl.mi_hdr_ctrl_mask[0] << 1);
		}
		if (*val & fe->general_ctrl.mi_hdr_ctrl_mask[1]) {
			*val &= ~fe->general_ctrl.mi_hdr_ctrl_mask[1];
			*val |= (fe->general_ctrl.mi_hdr_ctrl_mask[1] << 1);
		}
	} else if (offset == fe->general_ctrl.denoise3d_ctrl[0]
		   || offset == fe->general_ctrl.denoise3d_ctrl[1]) {
		if (*val & fe->general_ctrl.denoise3d_ctrl_mask[0]) {
			*val &= ~fe->general_ctrl.denoise3d_ctrl_mask[0];
			*val |= fe->general_ctrl.denoise3d_ctrl_mask[1];
		}
		if (*val & fe->general_ctrl.denoise3d_ctrl_mask[2]) {
			*val &= ~fe->general_ctrl.denoise3d_ctrl_mask[2];
			*val |= fe->general_ctrl.denoise3d_ctrl_mask[3];
		}
	}
}

static void isp_fe_mcm_busid_filter(struct isp_fe_context *fe, u32 offset, u32 *val)
{
#if defined(ISP_MIV2)
	uint8_t mcm_rd_id, mcm_wr0_id, mcm_wr1_id;
	if (offset == fe->general_ctrl.mi_mcm_bus_id) {
		*val |= fe->general_ctrl.mi_mcm_bus_id_mask[3];

		mcm_wr0_id = (uint8_t) (((*val) & fe->general_ctrl.mi_mcm_bus_id_mask[0]) >> fe->general_ctrl.mi_mcm_bus_id_shift[0]);
		mcm_wr1_id = (uint8_t) (((*val) & fe->general_ctrl.mi_mcm_bus_id_mask[1]) >> fe->general_ctrl.mi_mcm_bus_id_shift[1]);
		if ((~mcm_wr0_id) != mcm_wr1_id) {	//user the different rd id from wr id
			mcm_rd_id = ~mcm_wr0_id;
		} else {
			mcm_rd_id = mcm_wr0_id ^ 0x80;
		}

		*val &= ~(fe->general_ctrl.mi_mcm_bus_id_mask[2]);
		*val |= (mcm_rd_id << fe->general_ctrl.mi_mcm_bus_id_shift[2]);
	}
#endif
}


static int isp_fe_handle_pre_transaction(struct vvcam_fe_dev *dev, uint8_t vdid)
{
	int ret;
	struct isp_fe_context *fe = &dev->fe;

	reinit_completion(&fe->fe_completion);

	// @hailo - we have logic that prevents register writes while an FE transaction is in progress
	// disable mi_imsc so we don't handle interrupts while in fe transaction
	// also disable mi_imsc in the cmd buffer because the FE transaction will write the mask back
	/* make sure mask writes from cmd buffer that were not written to HW are also saved */
	isp_fe_raw_write_reg(dev, VI_MI_IMSC_OFFSET, 0);
	ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, VI_MI_IMSC_OFFSET, &fe->saved_mi_imsc);
	if (ret) {
		pr_err("%s: failed to read MI_IMSC, ret=%d\n", __func__, ret);
		return ret;
	}
	ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, VI_MI_IMSC_OFFSET, 0);
	if (ret) {
		pr_err("%s: failed to write MI_IMSC, ret=%d\n", __func__, ret);
		return ret;
	}

	// disable isp_imsc so we don't handle interrupts while in fe transaction
	// also disable isp_imsc in the cmd buffer because the FE transaction will write the mask back
	/* make sure mask writes from cmd buffer that were not written to HW are also saved */
	isp_fe_raw_write_reg(dev, VI_ISP_IMSC_OFFSET, 0);
	ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, VI_ISP_IMSC_OFFSET, &fe->saved_isp_imsc);
	if (ret) {
		pr_err("%s: failed to read ISP_IMSC, ret=%d\n", __func__, ret);
		return ret;
	}
	ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, VI_ISP_IMSC_OFFSET, 0);
	if (ret) {
		pr_err("%s: failed to write ISP_IMSC, ret=%d\n", __func__, ret);
		return ret;
	}

	// set FE transaction is active while isp_irq is diabled
	disable_irq(fe->isp_irq);
	atomic_set(&dev->fe_transaction_active, 1);
	enable_irq(fe->isp_irq);

	fe->state = ISP_FE_STATE_RUNNING;

	return 0;
}

static void isp_fe_log_dma_info(struct vvcam_fe_dev *dev, uint8_t vdid)
{
	struct isp_fe_context *fe = &dev->fe;
#if VIV_ISP_FE_DMA_TIME_DEBUG == 1
	u64 dma_t_ns;

	dma_t_ns = fe->last_t_end_ns - fe->last_t_ns;
	if (fe->prev_vdid == vdid) {
		if (fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num < ISP_FE_REG_PART_REFRESH_NUM) {
			isp_info("fe dma time num 0x%x %lldns\n",
				fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num, dma_t_ns);
		} else {
			isp_info("fe dma time num 0x%x %lldns\n",
				fe->fe_buff[vdid].refresh_full_regs.curr_cmd_num, dma_t_ns);
		}
	} else {
		isp_info("fe dma time num 0x%x %lldns\n",
				fe->fe_buff[vdid].refresh_full_regs.curr_cmd_num, dma_t_ns);
	}
#else
	if (fe->prev_vdid == vdid) {
		if (fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num < ISP_FE_REG_PART_REFRESH_NUM) {
			isp_info("fe dma num 0x%04x\n",
				fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num);
		} else {
			isp_info("fe dma num 0x%04x\n",
				fe->fe_buff[vdid].refresh_full_regs.curr_cmd_num);
		}
	} else {
		isp_info("fe dma num 0x%04x\n",
				fe->fe_buff[vdid].refresh_full_regs.curr_cmd_num);
	}
#endif
}

static int isp_fe_wait_transaction_handle_post(struct vvcam_fe_dev *dev, uint8_t vdid)
{
	struct isp_fe_context *fe = &dev->fe;
	int ret;

	if (vdid >= fe->vdid_num) {
		pr_err("%s error: vdid %d is out of range (max=%d)\n", __func__, vdid, fe->vdid_num);
		return -EINVAL;
	}

	// Wait for completion before returning...
	if (wait_for_completion_timeout(&fe->fe_completion, msecs_to_jiffies(VIV_ISP_FE_DMA_TIMOUT_MS)) == 0) {
		WARN(1, "fe transaction timeout! FE is stuck\n");
		return -ETIMEDOUT;
	}

	isp_fe_log_dma_info(dev, vdid);
	ret = isp_fe_clean_wo_bits(dev);
	if (ret) {
		isp_err("%s: failed to clean wo bits, ret=%d\n", __func__, ret);
		return ret;
	}
	fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num = 0;

	atomic_set(&dev->fe_transaction_active, 0);

	isp_fe_perform_post_fe_writes(dev);

	isp_fe_raw_write_reg(dev, VI_MI_IMSC_OFFSET, fe->saved_mi_imsc);
	isp_fe_raw_write_reg(dev, VI_ISP_IMSC_OFFSET, fe->saved_isp_imsc);

	// restore mi_imsc from command buffer
	ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, VI_MI_IMSC_OFFSET, fe->saved_mi_imsc);
	if (ret) {
		pr_err("%s: failed to write MI_IMSC, ret=%d\n", __func__, ret);
		return ret;
	}
	fe->saved_mi_imsc = 0;

	// restore isp_imsc from command buffer
	ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, VI_ISP_IMSC_OFFSET, fe->saved_isp_imsc);
	if (ret) {
		pr_err("%s: failed to write ISP_IMSC, ret=%d\n", __func__, ret);
		return ret;
	}
	fe->saved_isp_imsc = 0;

	fe->prev_vdid = vdid;
	fe->state = ISP_FE_STATE_WAITING;

	return 0;
}

static int isp_fe_set_dma(struct vvcam_fe_dev *dev, uint8_t vdid, struct isp_fe_cmd_buffer_t *refresh_regs)
{
#if defined(ISP_MIV2)
	u32 miv2_mcm_bus_id;
#endif
	struct isp_fe_context *fe = &dev->fe;
	int ret;

	isp_info("enter %s\n", __func__);

	isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_ctrl, 0);

#if defined(ISP_MIV2)
	ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, fe->general_ctrl.mi_mcm_bus_id, &miv2_mcm_bus_id);
	if (ret) {
		pr_err("%s: failed to read mi_mcm_bus_id from full cmd buffer, ret=%d\n", __func__, ret);
		return ret;
	}
	isp_fe_mcm_busid_filter(fe, fe->general_ctrl.mi_mcm_bus_id, &miv2_mcm_bus_id);
	ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, fe->general_ctrl.mi_mcm_bus_id, miv2_mcm_bus_id);
	if (ret) {
		pr_err("%s: failed to write mi_mcm_bus_id to full cmd buffer, ret=%d\n", __func__, ret);
		return ret;
	}
	isp_fe_raw_write_reg(dev, fe->general_ctrl.mi_mcm_bus_id, miv2_mcm_bus_id);	//0x0DCABD1E
#endif

	ret = isp_fe_handle_pre_transaction(dev, vdid);
	if (ret) {
		pr_err("%s: failed in pre-transaction handling, ret=%d\n", __func__, ret);
		return ret;
	}

	isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_ctrl, 0x00000001);
	isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_imsc, 0x00000001);
	isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_dma_ad, refresh_regs->cmd_dma_addr);
	isp_info("refresh_regs->cmd_dma_addr=%08llx\n", refresh_regs->cmd_dma_addr);
	isp_info("refresh_regs->curr_cmd_num=%08x\n", refresh_regs->curr_cmd_num);
	fe->curr_vdid = vdid;
#if VIV_ISP_FE_DMA_TIME_DEBUG == 1
	fe->last_t_ns = ktime_get_ns();
#endif
	isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_dma_start, 0x00010000 | refresh_regs->curr_cmd_num);

	return isp_fe_wait_transaction_handle_post(dev, vdid);
}

/*call when irq*/
static int isp_fe_writeback_status(struct vvcam_fe_dev *dev)
{//3A statics HIST64
	uint32_t i, reg_index, offset, reg_val;
	uint8_t vdid;
	struct isp_fe_context *fe = &dev->fe;
	int ret;

	vdid = fe->curr_vdid;

	if (vdid >= fe->vdid_num) {
		pr_err("%s error: vdid %d is out of range (max=%d)\n", __func__, vdid, fe->vdid_num);
		return -EINVAL;
	}

	for (i = 0; i < (uint32_t) fe->status_regs_num; i++) {
		offset = fe->status_regs[i].reg_base;
		for (reg_index = 0; reg_index < fe->status_regs[i].reg_num; reg_index++) {
			reg_val = isp_fe_raw_read_reg(dev, offset);
			ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, offset, reg_val);
			if (ret) {
				pr_err("%s: failed to write to full cmd buffer for offset 0x%x, ret=%d\n", __func__, offset, ret);
				return ret;
			}
			offset = offset + fe->status_regs[i].reg_step;
		}
	}

	if (fe->general_ctrl.isp_hist != VIV_ISP_FE_UNUSED_OFFSET) {
		for (i = 0; i < fe->fe_buff[vdid].fixed_reg_rd_num; i++) {
			fe->fe_buff[vdid].fixed_reg_buffer[i] = isp_fe_raw_read_reg(dev, fe->general_ctrl.isp_hist);
		}
	}

	return 0;
}

static int isp_fe_clean_wo_bits(struct vvcam_fe_dev *dev)
{
	uint8_t vdid, i;
	struct isp_fe_context *fe = &dev->fe;
	int ret;
	uint32_t val;

	isp_info("enter %s\n", __func__);
	vdid = fe->curr_vdid;

	if (fe->general_ctrl.mi_hdr_ctrl != VIV_ISP_FE_UNUSED_OFFSET) {
		ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, fe->general_ctrl.mi_hdr_ctrl, &val);
		if (ret) {
			pr_err("%s: reading mi_hdr_ctrl (0x%x) failed, ret=%d\n", __func__, fe->general_ctrl.mi_hdr_ctrl, ret);
			return ret;
		}
		val &= ~(1 << 7);
		ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, fe->general_ctrl.mi_hdr_ctrl, val);
		if (ret) {
			pr_err("%s: writing mi_hdr_ctrl (0x%x) failed, ret=%d\n", __func__, fe->general_ctrl.mi_hdr_ctrl, ret);
			return ret;
		}

	}
	if (fe->general_ctrl.mi_ctrl != VIV_ISP_FE_UNUSED_OFFSET) {
		ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, fe->general_ctrl.mi_ctrl, &val);
		if (ret) {
			pr_err("%s: reading mi_ctrl (0x%x) failed, ret=%d\n", __func__, fe->general_ctrl.mi_ctrl, ret);
			return ret;
		}
		val &= ~fe->general_ctrl.isp_wo_bits_mask[1];
		ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, fe->general_ctrl.mi_ctrl, val);
		if (ret) {
			pr_err("%s: writing mi_ctrl (0x%x) failed, ret=%d\n", __func__, fe->general_ctrl.mi_ctrl, ret);
			return ret;
		}
		//clean the read dma start in mi ctrl,fill nop
		isp_fe_buffer_memset(&fe->fe_buff[vdid].refresh_full_regs.cmd_buffer[fe->special_reg_base], ISP_FE_SPECIAL_REG_NUM);
	}

	for (i = 0; i < fe->vdid_num; i++) {
		ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, i, 0x1600, &val);
		if (ret) {
			return ret;
		}
		val &= ~0x46;
		ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, i, 0x1600, val);
		if (ret) {
			return ret;
		}
		ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, i, 0x5000, &val);
		if (ret) {
			return ret;
		}
		val &= ~0x26;
		ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, i, 0x5000, val);
		if (ret) {
			return ret;
		}
	}
	return 0;
}


/* this is called once when starting the stream with FE and once in the end of the stream */
static int isp_fe_update_cmd(struct vvcam_fe_dev *dev)
{
	uint8_t vdid;
	uint32_t curr_cmd_num;
	struct isp_fe_context *fe = &dev->fe;
	isp_info("enter %s\n", __func__);
	vdid = fe->curr_vdid;

	curr_cmd_num = fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num;
	/* use partial or full command buffer depending on number of registers in the partial buffer */
	if (curr_cmd_num < ISP_FE_REG_PART_REFRESH_NUM) {
		//refresh part registers
		isp_fe_buffer_append_end(&fe->fe_buff[vdid].refresh_part_regs.cmd_buffer[curr_cmd_num],
					&fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num);
#if VIV_ISP_FE_DEBUG_DUMP == 1
		isp_fe_cmdbuffer_dump(&fe->fe_buff[vdid].refresh_part_regs);
#endif
		//config dma
		return isp_fe_set_dma(dev, vdid, &fe->fe_buff[vdid].refresh_part_regs);
	} else {		//refresh the whole registers
		//config dma
#if VIV_ISP_FE_DEBUG_DUMP == 1
		isp_fe_cmdbuffer_dump(&fe->fe_buff[vdid].refresh_full_regs);
		//isp_fe_cmdbuffer_parse(&fe->fe_buff[vdid].refresh_full_regs);
#endif
		return isp_fe_set_dma(dev, vdid, &fe->fe_buff[vdid].refresh_full_regs);
	}
}

static int isp_fe_register_post_fe_write(struct vvcam_fe_dev *dev, uint32_t offset, uint32_t val, enum isp_fe_post_offsets idx)
{
	struct isp_fe_context *fe = &dev->fe;

	if (idx >= ISP_FE_POST_OFFSET_MAX || idx < 0) {
		isp_err("%s error: idx %d is invalid!", __func__, idx);
		return -EINVAL;
	}

	fe->post_fe_modify_reg_offset[idx] = offset;
	fe->post_fe_modify_reg_value[idx] = val;
	return 0;
}

static void isp_fe_perform_post_fe_writes(struct vvcam_fe_dev *dev)
{
	struct isp_fe_context *fe = &dev->fe;
	int reg;
	int i;

	for (i = 0; i < ISP_FE_POST_OFFSET_MAX; i++) {
		if (fe->post_fe_modify_reg_offset[i] != -1 && fe->post_fe_modify_reg_value[i] != -1) {
			reg = isp_fe_raw_read_reg(dev, fe->post_fe_modify_reg_offset[i]);
			reg |= fe->post_fe_modify_reg_value[i];
			isp_fe_raw_write_reg(dev, fe->post_fe_modify_reg_offset[i], reg);

			fe->post_fe_modify_reg_offset[i] = fe->post_fe_modify_reg_value[i] = -1;
		}
	}
}

static int __isp_fe_write_vdid_reg(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t val)
{
	uint32_t mcm_path_enable, mcm_path_enable_mask, mi_ctrl_val, part_val, vi_dpcl_val;
	uint32_t vi_if_select, vi_if_select_mask;
	uint8_t i;
	bool start, stop, update_immediately = false;
	struct isp_fe_context *fe = &dev->fe;
	int ret;


	isp_fe_mcm_busid_filter(fe, offset, &val);
	//mask some ctrl enable and shadow enable bits update immediately
	isp_fe_shd_to_immediately(fe, offset, &val);

	if ((offset >= 0x1600 && offset <= 0x160c) || (offset >= 0x5000 && offset <= 0x5008)) {
		for (i = 0; i < fe->vdid_num; i++) {
			ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, i, offset, val);
			if (ret) {
				pr_err("%s: failed to write to full cmd buffer for offset 0x%x, ret=%d\n", __func__, offset, ret);
				return ret;
			}
		}
	}

	if (fe->general_ctrl.mi_ctrl == offset) {
		switch (vdid) {
		case 0:
			mcm_path_enable = val & (1 << 6);
			mcm_path_enable_mask = ~(1 << 6);
			break;
		case 1:
			mcm_path_enable = val & (1 << 7);
			mcm_path_enable_mask = ~(1 << 7);
			break;
		case 2:
			mcm_path_enable = val & (1 << 17);
			mcm_path_enable_mask = ~(1 << 17);
			break;
		case 3:
			mcm_path_enable = val & (1 << 18);
			mcm_path_enable_mask = ~(1 << 18);
			break;
		default:
			mcm_path_enable = 0;
			mcm_path_enable_mask = ~0;
			break;
		}

		for (i = 0; i < fe->vdid_num; i++) {
			ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, i, offset, &mi_ctrl_val);
			if (ret < 0) {
				pr_err("%s: failed to read from full cmd buffer for vdid %d, ret=%d\n", __func__, i, ret);
				return ret;
			}
			mi_ctrl_val &= mcm_path_enable_mask;
			mi_ctrl_val |= mcm_path_enable;
			ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, i, offset, mi_ctrl_val);
			if (ret < 0) {
				pr_err("%s: failed to write to full cmd buffer for vdid %d, ret=%d\n", __func__, i, ret);
				return ret;
			}
		}

		// If MI_CTRL bits 0/1/11 (certain paths enabled) or bit 12 (sp2_raw_rdma_start)
		if ((val & 0x1803000) != 0) {
			// Disable sp2_raw_rdma_start_con (continuous mode)
			val &= ~((1 << 13) | (1 << 24));
			update_immediately = true;
		}

		// mcm read dma start (MI_CTRL_MCM_RAW_RDMA_START in isp-hw) - should be post-FE to prevent inner race within FE.
		// This is a HW issue in FE - so instead of writing this value to command buffer, we use raw write for it post-FE.
		if (val & (1 << 15)) {
			val &= ~(1 << 15);	// disable MCM read dma start
			ret = isp_fe_register_post_fe_write(dev, offset, 1 << 15, ISP_FE_POST_OFFSET_MCM_RAW_RDMA_START);
			if (ret) {
				pr_err("%s: failed to register post fe write for MCM_RAW_RDMA_START, ret=%d\n", __func__, ret);
				return ret;
			}
		}
	}

	if (offset == VI_DPCL_OFFSET) {
		//if_select0~if_select3 is the same value in each full buffer
		vi_if_select_mask = (3<<26) | (3<<24) | (3<<16) | (3<<8);
		vi_if_select = val & vi_if_select_mask;
		for (i = 0; i < fe->vdid_num; i++) {
			ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, i, VI_DPCL_OFFSET, &vi_dpcl_val);
			if (ret) {
				pr_err("%s: failed to read VI_DPCL_OFFSET for vdid %d, ret=%d\n", __func__, i, ret);
				return ret;
			}
			vi_dpcl_val = (vi_dpcl_val & (~vi_if_select_mask)) | vi_if_select;
			ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, i, VI_DPCL_OFFSET, vi_dpcl_val);
			if (ret) {
				pr_err("%s: failed to write VI_DPCL_OFFSET for vdid %d, ret=%d\n", __func__, i, ret);
				return ret;
			}
		}
	}

	//reset isp
	//TODO
	part_val = val;
	//move the read dma to the last step
	if (update_immediately) {	//sp2 read dma
		ret = isp_fe_register_post_fe_write(dev, offset, val, ISP_FE_POST_OFFSET_SP2_RAW_RDMA_START);
		if (ret) {
			pr_err("%s: failed to register post fe write for SP2_RAW_RDMA_START, ret=%d\n", __func__, ret);
			return ret;
		}
		val &= ~((1 << 12) | (1 << 23));
	}

	//fe->reg_buffer[offset / ISP_FE_REG_SIZE] = val;
	ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, offset, val);
	if (ret) {
		pr_err("%s: failed to write to full cmd buffer for offset 0x%x, ret=%d\n", __func__, offset, ret);
		return ret;
	}

	ret = isp_fe_write_vdid_reg_to_partial_cmd_buf(fe, vdid, offset, part_val);
	if (ret) {
		pr_err("%s: failed to write to partial cmd buffer, ret=%d\n", __func__, ret);
		return ret;
	}

	ret = is_start_fe_dma(dev, vdid, offset, val, &start);
	if (ret) {
		pr_err("%s: failed to check if start fe dma for offset 0x%x, val 0x%x, ret=%d\n", __func__, offset, val, ret);
		return ret;
	}
	if (fe->running_num) {
		ret = is_stop_isp(dev, vdid, offset, val, &stop);
		if (ret) {
			pr_err("%s: failed to check if stop isp for offset 0x%x, val 0x%x, ret=%d\n", __func__, offset, val, ret);
			return ret;
		}
	} else {
		stop = false;
	}

	if (fe->running_num < fe->vdid_num && start == true && fe->virt_running[vdid] == false) {
		fe->running_num++;
		fe->virt_running[vdid] = true;
	}

	if (fe->state == ISP_FE_STATE_WAITING && stop && fe->virt_running[vdid]) {
		fe->running_num--;
		fe->virt_running[vdid] = false;
	}

	if (fe->state == ISP_FE_STATE_READY && start) {
		fe->curr_vdid = vdid;
		ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, VI_DPCL_OFFSET, &vi_dpcl_val);
		if (ret) {
			pr_err("%s: failed to read VI_DPCL_OFFSET, ret=%d\n", __func__, ret);
			return ret;
		}
		vi_dpcl_val &= (~(0xf << 20));
		vi_dpcl_val |= (vdid << 20) | (vdid << 22);
		ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, vdid, VI_DPCL_OFFSET, vi_dpcl_val);
		if (ret) {
			pr_err("%s: failed to write VI_DPCL_OFFSET, ret=%d\n", __func__, ret);
			return ret;
		}

		ret = isp_fe_update_cmd(dev);
		if (ret) {
			pr_err("%s: failed to update cmd, ret=%d\n", __func__, ret);
			return ret;
		}
	} else if (fe->state == ISP_FE_STATE_WAITING && stop && fe->running_num == 0) {
		ret = isp_fe_update_cmd(dev);
		if (ret) {
			pr_err("%s: failed to update cmd on stop, ret=%d\n", __func__, ret);
			return ret;
		}
	}

	return 0;
}

static int isp_fe_get_vdid(struct vvcam_fe_dev *dev, uint8_t *vdid)
{
	struct isp_fe_context *fe = &dev->fe;
	if (vdid) {
		*vdid = fe->curr_vdid;
	}
	return 0;
}

static int __isp_fe_get_status(struct vvcam_fe_dev *dev, struct isp_fe_status_t *status)
{
	struct isp_fe_context *fe = &dev->fe;
	isp_info("enter %s\n", __func__);

	status->enable = (uint8_t) fe->enable;
	status->status = (uint32_t) fe->state;
	status->curr_channel = (uint32_t) fe->curr_vdid;
	return 0;
}

static int isp_fe_update_sensor_id(struct vvcam_fe_dev *dev, struct isp_fe_switch_t *fe_switch)
{
	uint8_t i;
	uint32_t vi_dpcl_val;
	struct isp_fe_context *fe = &dev->fe;
	int ret;
	isp_info("enter %s\n", __func__);

	//update the current and next sensor id when the channel not need to be switch
	vi_dpcl_val = isp_fe_raw_read_reg(dev, VI_DPCL_OFFSET);
	vi_dpcl_val &= ~(0xf << 20);
	vi_dpcl_val |= (fe_switch->next_vdid[0] << 20) | (fe_switch->next_vdid[1] << 22);
	isp_fe_raw_write_reg(dev, VI_DPCL_OFFSET, vi_dpcl_val);

	//update the current and next sensor id in the full registers
	for (i = 0; i < fe->vdid_num; i++) {
		ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, i, VI_DPCL_OFFSET, &vi_dpcl_val);
		if (ret) {
			pr_err("%s: failed to read VI_DPCL_OFFSET for vdid %d, ret=%d\n", __func__, i, ret);
			return ret;
		}
		vi_dpcl_val &= ~(0xf << 20);
		vi_dpcl_val |= (fe_switch->next_vdid[0] << 20) | (fe_switch->next_vdid[1] << 22);
		ret = isp_fe_write_vdid_reg_to_full_cmd_buf(fe, i, VI_DPCL_OFFSET, vi_dpcl_val);
		if (ret) {
			pr_err("%s: failed to write VI_DPCL_OFFSET for vdid %d, ret=%d\n", __func__, i, ret);
			return ret;
		}
	}

	return 0;
}


static int isp_fe_buffer_append_isp_update(struct vvcam_fe_dev *dev)
{
	struct isp_fe_context *fe = &dev->fe;
	uint8_t vdid = fe->curr_vdid;
	uint32_t isp_ctrl_val;
	int ret;

	ret = isp_fe_read_vdid_reg_from_full_cmd_buf(fe, vdid, fe->general_ctrl.isp_ctrl, &isp_ctrl_val);
	if (ret) {
		pr_err("%s: invalid cmd for isp_ctrl, ret=%d\n", __func__, ret);
		return ret;
	}

	isp_ctrl_val |= fe->general_ctrl.isp_ctrl_mask[3];

	return isp_fe_write_vdid_reg_to_partial_cmd_buf(fe, vdid, fe->general_ctrl.isp_ctrl, isp_ctrl_val);
}

static int __isp_fe_switch(struct vvcam_fe_dev *dev, struct isp_fe_switch_t *fe_switch)
{
	uint8_t vdid;
	uint32_t part_cmd_num;
	struct isp_fe_context *fe = &dev->fe;
	int ret;

	isp_info("enter %s\n", __func__);

	ret = isp_fe_update_sensor_id(dev, fe_switch);
	if (ret) {
		pr_err("%s: failed to update sensor id, ret=%d\n", __func__, ret);
		return ret;
	}

	vdid = fe->curr_vdid;
	isp_info("%s vdid value = %d, next id value = %d\n", __func__, vdid, fe_switch->next_vdid[0]);

	ret = isp_fe_register_post_fe_write(dev, fe->general_ctrl.mi_ctrl, 1 << 15, ISP_FE_POST_OFFSET_MCM_RAW_RDMA_START);
	if (ret) {
		pr_err("%s: failed to register post fe write for MCM_RAW_RDMA_START, ret=%d\n", __func__, ret);
		return ret;
	}

	if (vdid == fe_switch->next_vdid[0]) {
		if (fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num == 0) {	//no register to be updated
			return 0;
		}

		ret = isp_fe_handle_pre_transaction(dev, vdid);
		if (ret) {
			pr_err("%s: failed in pre-transaction handling, ret=%d\n", __func__, ret);
			return ret;
		}

		part_cmd_num = fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num;
#if 0
		if (part_cmd_num < ISP_FE_REG_PART_REFRESH_NUM - 1) {
#else
		if (0) {
#endif
			//refresh part registers
			ret = isp_fe_buffer_append_isp_update(dev);
			if (ret) {
				pr_err("%s: failed to append isp update cmd into part cmd buffer for vdid %d, ret=%d\n", __func__, vdid, ret);
				return ret;
			}
			part_cmd_num++;
			isp_fe_buffer_append_end(&fe->fe_buff[vdid].refresh_part_regs.cmd_buffer[part_cmd_num],
						&fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num);
			//isp_info("%s: refresh_part_regs.curr_cmd_num=0x%08x\n", __func__, fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num);
			//config dma
#if VIV_ISP_FE_DEBUG_DUMP == 1
			isp_fe_cmdbuffer_dump(&fe->fe_buff[vdid].refresh_part_regs);
#endif
			isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_ctrl, 0x00000001);
			isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_dma_ad, (u32) fe->fe_buff[vdid].refresh_part_regs.cmd_dma_addr);
#if VIV_ISP_FE_DMA_TIME_DEBUG == 1
			fe->last_t_ns = ktime_get_ns();
#endif
			isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_dma_start, 0x00010000 | fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num);
		} else {	//refresh the whole registers
			//config dma
#if VIV_ISP_FE_DEBUG_DUMP == 1
			isp_fe_cmdbuffer_dump(&fe->fe_buff[vdid].refresh_full_regs);
#endif

			//isp_info("%s: refresh_full_regs.curr_cmd_num=0x%08x\n", __func__, fe->fe_buff[vdid].refresh_full_regs.curr_cmd_num);
			isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_ctrl, 0x00000001);
			isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_dma_ad, (u32) fe->fe_buff[vdid].refresh_full_regs.cmd_dma_addr);
#if VIV_ISP_FE_DMA_TIME_DEBUG == 1
			fe->last_t_ns = ktime_get_ns();
#endif
			isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_dma_start, 0x00010000 | fe->fe_buff[vdid].refresh_full_regs.curr_cmd_num);
		}
	} else {
		/* VDID is changing - writeback current status regs before switching */
		ret = isp_fe_writeback_status(dev);
		if (ret) {
			pr_err("%s: failed to writeback status, ret=%d\n", __func__, ret);
			return ret;
		}

		fe->curr_vdid = fe_switch->next_vdid[0];
		vdid = fe_switch->next_vdid[0];

		ret = isp_fe_handle_pre_transaction(dev, vdid);
		if (ret) {
			pr_err("%s: failed in pre-transaction handling, ret=%d\n", __func__, ret);
			return ret;
		}

#if VIV_ISP_FE_DEBUG_DUMP == 1
		isp_fe_cmdbuffer_dump(&fe->fe_buff[vdid].refresh_full_regs);
#endif

		//refresh the whole registers
		//config dma
		//isp_info("%s: refresh_full_regs.curr_cmd_num=0x%08x\n", __func__, fe->refresh_full_regs.curr_cmd_num);

		isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_ctrl, 0x00000001);
		isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_dma_ad, (u32) fe->fe_buff[vdid].refresh_full_regs.cmd_dma_addr);
#if VIV_ISP_FE_DMA_TIME_DEBUG == 1
		fe->last_t_ns = ktime_get_ns();
#endif
		isp_fe_raw_write_reg(dev, fe->general_ctrl.fe_dma_start, 0x00010000 | fe->fe_buff[vdid].refresh_full_regs.curr_cmd_num);
	}

	return isp_fe_wait_transaction_handle_post(dev, vdid);
}

static int __isp_fe_set_params(struct vvcam_fe_dev *dev, struct isp_fe_params_t *params)
{
	int ret;
	int i;
	uint8_t vdid;
	uint32_t curr_cmd_num;
	uint16_t offset, mapped_index, index = 0, ctrl_index = 0;
	static uint16_t ctrl_regs_num = ARRAY_SIZE(ctrl_regs);

	struct isp_fe_context *fe = &dev->fe;
	pr_info("enter %s\n", __func__);

	if (params->max_vd_caps > ISP_FE_VIRT_MAXCNT) {
		isp_err("%s: alloc fe buff error!\n", __func__);
		return -1;
	}
	fe->vdid_num = params->max_vd_caps;

	if (!dev->dev->dma_mask) {
		dev->dev->dma_mask = &dev->dev->coherent_dma_mask;
	}

	ret = dma_set_coherent_mask(dev->dev, DMA_BIT_MASK(32));
	if (ret < 0) {
		isp_err("%s: Cannot configure coherent dma mask!\n", __func__);
		return -1;
	}

	fe->general_ctrl = params->general_ctrl;

	fe->fe_buff = kzalloc(sizeof(struct isp_fe_buff_t) * params->max_vd_caps, GFP_KERNEL);
	if (!fe->fe_buff) {
		isp_err("%s: alloc fe buff error!\n", __func__);
		return -1;
	}

	fe->running_num = 0;
	for (vdid = 0; vdid < ISP_FE_VIRT_MAXCNT; vdid++) {
		fe->virt_running[vdid] = false;
	}

	for (vdid = 0; vdid < fe->vdid_num; vdid++) {
		fe->fe_buff[vdid].rd_index = 0;
		fe->fe_buff[vdid].fixed_reg_rd_num = ISP_FE_FIXED_OFFSET_READ_NUM;
		fe->fe_buff[vdid].fixed_reg_buffer = kmalloc(fe->fe_buff[vdid].fixed_reg_rd_num * sizeof(uint32_t), GFP_KERNEL);
		if (!fe->fe_buff[vdid].fixed_reg_buffer) {
			isp_err("%s: alloc fixed_reg_buffer %d error!\n", __func__, vdid);
			goto alloc_fixed_buf_err;
		}
	}

	for (vdid = 0; vdid < fe->vdid_num; vdid++) {
		fe->fe_buff[vdid].refresh_part_regs.cmd_num_max = ISP_FE_REG_PART_REFRESH_NUM;
		fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num = 0;
		fe->fe_buff[vdid].refresh_part_regs.cmd_buffer = dma_alloc_coherent(dev->dev,
								sizeof(union isp_fe_cmd_u) * fe->fe_buff[vdid].refresh_part_regs.cmd_num_max + 1,
								&fe->fe_buff[vdid].refresh_part_regs.cmd_dma_addr, GFP_KERNEL);
		isp_info("%s:%d refresh_part_regs.cmd_num_max=0x%08x\n", __func__, __LINE__, fe->fe_buff[vdid].refresh_part_regs.cmd_num_max);
		isp_info("%s:%d refresh_part_regs.cmd_buffer=%p\n", __func__, __LINE__, fe->fe_buff[vdid].refresh_part_regs.cmd_buffer);
		isp_info("%s:%d refresh_part_regs.cmd_dma_addr=0x%llx\n", __func__, __LINE__, fe->fe_buff[vdid].refresh_part_regs.cmd_dma_addr);
		if (!fe->fe_buff[vdid].refresh_part_regs.cmd_buffer) {
			isp_err("%s: alloc refresh_part_regs error!\n", __func__);
			goto alloc_part_err;
		}
		isp_fe_buffer_memset(fe->fe_buff[vdid].refresh_part_regs.cmd_buffer, fe->fe_buff[vdid].refresh_part_regs.curr_cmd_num);
	}

	fe->special_reg_base = ISP_FE_FULL_BUFFER_NUM +params->tbl_total_params_num;
	for (vdid = 0; vdid < fe->vdid_num; vdid++) {
		fe->fe_buff[vdid].tbl_reg_addr_num = params->tbl_reg_addr_num;
		fe->fe_buff[vdid].tbl_total_params_num = params->tbl_total_params_num;
		fe->fe_buff[vdid].refresh_full_regs.cmd_num_max = ISP_FE_FULL_BUFFER_NUM +
								params->tbl_total_params_num + ISP_FE_SPECIAL_REG_NUM + 2;	//2--end or nop+end flag
		fe->fe_buff[vdid].refresh_full_regs.cmd_buffer = dma_alloc_coherent(dev->dev,
								sizeof(union isp_fe_cmd_u) * fe->fe_buff[vdid].refresh_full_regs.cmd_num_max,
								&fe->fe_buff[vdid].refresh_full_regs.cmd_dma_addr, GFP_KERNEL);
		isp_info("%s:%d tbl_total_params_num=0x%08x\n", __func__, __LINE__, fe->fe_buff[vdid].tbl_total_params_num);
		isp_info("%s:%d refresh_full_regs.cmd_num_max=0x%08x\n", __func__, __LINE__, fe->fe_buff[vdid].refresh_full_regs.cmd_num_max);
		isp_info("%s:%d refresh_full_regs.cmd_buffer=%p\n", __func__, __LINE__, fe->fe_buff[vdid].refresh_full_regs.cmd_buffer);
		isp_info("%s:%d refresh_full_regs.cmd_dma_addr=0x%llx\n", __func__, __LINE__, fe->fe_buff[vdid].refresh_full_regs.cmd_dma_addr);
		if (!fe->fe_buff[vdid].refresh_full_regs.cmd_buffer) {
			isp_err("%s: alloc refresh_all_regs error!\n", __func__);
			goto alloc_full_err;
		}

		isp_fe_buffer_memset(fe->fe_buff[vdid].refresh_full_regs.cmd_buffer,
					fe->fe_buff[vdid].refresh_full_regs.cmd_num_max);
		curr_cmd_num = ISP_FE_FULL_BUFFER_NUM + fe->fe_buff[vdid].tbl_total_params_num + ISP_FE_SPECIAL_REG_NUM;
		fe->fe_buff[vdid].refresh_full_regs.curr_cmd_num = curr_cmd_num;
		isp_fe_buffer_append_end(&fe->fe_buff[vdid].refresh_full_regs.cmd_buffer[curr_cmd_num],
					&fe->fe_buff[vdid].refresh_full_regs.curr_cmd_num);
	}

	for (vdid = 0; vdid < fe->vdid_num; vdid++) {
		for (i = 0; i < params->tbl_reg_addr_num; i++) {
			fe->fe_buff[vdid].tbl_buffer[i].curr_index = 0;
			fe->fe_buff[vdid].tbl_buffer[i].params_num = params->tbl[i].params_num;
			fe->fe_buff[vdid].tbl_buffer[i].tbl_reg_offset = params->tbl[i].tbl_reg_offset;
			if (i == 0) {
				fe->fe_buff[vdid].tbl_buffer[i].tbl_cmd_buffer =
					(struct isp_fe_cmd_wreg_t *)(fe->fe_buff[vdid].refresh_full_regs.cmd_buffer + ISP_FE_FULL_BUFFER_NUM);
			} else {
				fe->fe_buff[vdid].tbl_buffer[i].tbl_cmd_buffer =
					fe->fe_buff[vdid].tbl_buffer[i - 1].tbl_cmd_buffer + fe->fe_buff[vdid].tbl_buffer[i - 1].params_num;
			}
		}
	}

	fe->status_regs_num = params->status_regs_num;
	for (i = 0; i < params->status_regs_num; i++) {
		fe->status_regs[i] = params->status_regs[i];
	}

	for (offset = 0; offset < ISP_FE_REG_OFFSET_BYTE_MAX; offset += ISP_FE_REG_SIZE) {
		if (ctrl_index < ctrl_regs_num) {
			if (offset == ctrl_regs[ctrl_index]) {
				if (offset == fe->general_ctrl.mi_ctrl) {
					mapped_index = ISP_FE_FULL_BUFFER_NUM - 1 - ctrl_index - 30;
				} else if (offset == 0x5000) {
					mapped_index = ISP_FE_FULL_BUFFER_NUM - 1 - ctrl_index + 30;
				} else {
					mapped_index = ISP_FE_FULL_BUFFER_NUM - 1 - ctrl_index;
				}
				ctrl_index++;
			} else if (offset < ctrl_regs[ctrl_index]) {
				mapped_index = index;
			}
		} else {
			mapped_index = index;
		}
		fe->hash_map_tbl[index++] = mapped_index;
	}

	isp_info("exit %s\n", __func__);

	return 0;

alloc_full_err:
	for (vdid = 0; vdid < fe->vdid_num; vdid++) {
		if (fe->fe_buff[vdid].refresh_full_regs.cmd_buffer) {
			dma_free_coherent(dev->dev, sizeof(union isp_fe_cmd_u) * fe->fe_buff[vdid].refresh_full_regs.cmd_num_max,
					fe->fe_buff[vdid].refresh_full_regs.cmd_buffer, fe->fe_buff[vdid].refresh_full_regs.cmd_dma_addr);
			fe->fe_buff[vdid].refresh_full_regs.cmd_buffer = NULL;
		}
	}

alloc_part_err:
	for (vdid = 0; vdid < fe->vdid_num; vdid++) {
		if (fe->fe_buff[vdid].refresh_part_regs.cmd_buffer) {
			dma_free_coherent(dev->dev, sizeof(union isp_fe_cmd_u) * fe->fe_buff[vdid].refresh_part_regs.cmd_num_max + 1,
					fe->fe_buff[vdid].refresh_part_regs.cmd_buffer, fe->fe_buff[vdid].refresh_part_regs.cmd_dma_addr);
			fe->fe_buff[vdid].refresh_part_regs.cmd_buffer = NULL;
		}
	}

alloc_fixed_buf_err:
	for (vdid = 0; vdid < fe->vdid_num; vdid++) {
		kfree(fe->fe_buff[vdid].fixed_reg_buffer);
	}

	kfree(fe->fe_buff);
	return -ENOMEM;

}

static int __isp_fe_reset(struct vvcam_fe_dev *dev)
{
	struct isp_fe_context *fe = &dev->fe;
	int vdid = 0;
	int i = 0;
	uint32_t isp_ctrl;

	isp_info("enter %s\n", __func__);

	if (fe->running_num != 0) {
		//isp reset
		isp_fe_raw_write_reg(dev, VI_IRCL_OFFSET, 0xFFFFFFBF);
		mdelay(1);
		isp_fe_raw_write_reg(dev, VI_IRCL_OFFSET, 0);

		//disable isp
		isp_ctrl = isp_fe_raw_read_reg(dev, fe->general_ctrl.isp_ctrl);
		isp_ctrl &= ~fe->general_ctrl.isp_ctrl_mask[0];
		isp_ctrl |= fe->general_ctrl.isp_ctrl_mask[1] | fe->general_ctrl.isp_ctrl_mask[3];
		isp_fe_raw_write_reg(dev, fe->general_ctrl.isp_ctrl, isp_ctrl);
		fe->running_num = 0;
	}

	atomic_set(&dev->fe_transaction_active, 0);

	//the default id value is set to invaild.
	fe->prev_vdid = VIV_INVALID_VDID;
	fe->curr_vdid = VIV_INVALID_VDID;
	for (i = 0; i < ISP_FE_POST_OFFSET_MAX; i++) {
		fe->post_fe_modify_reg_offset[i] = -1;
		fe->post_fe_modify_reg_value[i] = -1;
	}
	memset(&(fe->general_ctrl), 0, sizeof(struct isp_fe_reg_t));

	if (fe->fe_buff) {
		for (vdid = 0; vdid < fe->vdid_num; vdid++) {
			if (fe->fe_buff[vdid].refresh_full_regs.cmd_buffer) {
				dma_free_coherent(dev->dev, sizeof(union isp_fe_cmd_u) * fe->fe_buff[vdid].refresh_full_regs.cmd_num_max,
						fe->fe_buff[vdid].refresh_full_regs.cmd_buffer,
						fe->fe_buff[vdid].refresh_full_regs.cmd_dma_addr);

				fe->fe_buff[vdid].refresh_full_regs.cmd_buffer = NULL;
			}

			if (fe->fe_buff[vdid].refresh_part_regs.cmd_buffer) {
				dma_free_coherent(dev->dev, sizeof(union isp_fe_cmd_u) * fe->fe_buff[vdid].refresh_part_regs.cmd_num_max + 1,
						fe->fe_buff[vdid].refresh_part_regs.cmd_buffer,
						fe->fe_buff[vdid].refresh_part_regs.cmd_dma_addr);

				fe->fe_buff[vdid].refresh_part_regs.cmd_buffer = NULL;
			}

			if (fe->fe_buff[vdid].fixed_reg_buffer) {
				kfree(fe->fe_buff[vdid].fixed_reg_buffer);
				fe->fe_buff[vdid].fixed_reg_buffer = NULL;
			}

		}
		kfree(fe->fe_buff);
		fe->fe_buff = NULL;
	}

	isp_info("exit %s\n", __func__);

	return 0;
}

int isp_fe_init(struct vvcam_fe_dev *dev)
{
	int i = 0;
	struct isp_fe_context *fe = &dev->fe;
	isp_info("enter %s\n", __func__);

	if (fe->reg_buffer) {
		kfree(fe->reg_buffer);
	}

	if (fe->hash_map_tbl) {
		kfree(fe->hash_map_tbl);
	}

	fe->reg_buffer = kmalloc(ISP_FE_REG_OFFSET_BYTE_MAX, GFP_KERNEL);
	if (!fe->reg_buffer) {
		isp_err("%s: alloc reg_buffer error!\n", __func__);
		return -ENOMEM;
	}

	fe->hash_map_tbl = kmalloc(ISP_FE_REG_NUM * sizeof(uint16_t), GFP_KERNEL);
	if (!fe->hash_map_tbl) {
		isp_err("%s: alloc hash_map_tbl error!\n", __func__);
		goto hash_map_tbl_err;
	}

	init_completion(&fe->fe_completion);

	fe->prev_vdid = VIV_INVALID_VDID;
	fe->state = ISP_FE_STATE_INIT;
	for (i = 0; i < ISP_FE_POST_OFFSET_MAX; i++) {
		fe->post_fe_modify_reg_offset[i] = -1;
		fe->post_fe_modify_reg_value[i] = -1;
	}

	return 0;

hash_map_tbl_err:
	if (fe->reg_buffer) {
		kfree(fe->reg_buffer);
	}
	return -ENOMEM;
}

int isp_fe_destory(struct vvcam_fe_dev *dev)
{
	uint8_t vdid;
	struct isp_fe_context *fe = &dev->fe;
	isp_info("enter %s\n", __func__);

	fe->state = ISP_FE_STATE_EXIT;

	if (fe->fe_buff) {
		for (vdid = 0; vdid < fe->vdid_num; vdid++) {
			if (fe->fe_buff[vdid].refresh_full_regs.cmd_buffer) {
				dma_free_coherent(dev->dev, sizeof(union isp_fe_cmd_u) * fe->fe_buff[vdid].refresh_full_regs.cmd_num_max,
						fe->fe_buff[vdid].refresh_full_regs.cmd_buffer,
						fe->fe_buff[vdid].refresh_full_regs.cmd_dma_addr);
				fe->fe_buff[vdid].refresh_full_regs.cmd_buffer = NULL;
			}

			if (fe->fe_buff[vdid].refresh_part_regs.cmd_buffer) {
				dma_free_coherent(dev->dev, sizeof(union isp_fe_cmd_u) * fe->fe_buff[vdid].refresh_part_regs.cmd_num_max + 1,
						fe->fe_buff[vdid].refresh_part_regs.cmd_buffer,
						fe->fe_buff[vdid].refresh_part_regs.cmd_dma_addr);
				fe->fe_buff[vdid].refresh_part_regs.cmd_buffer = NULL;
			}

			if (fe->fe_buff[vdid].fixed_reg_buffer) {
				kfree(fe->fe_buff[vdid].fixed_reg_buffer);
				fe->fe_buff[vdid].fixed_reg_buffer = NULL;
			}
		}
		kfree(fe->fe_buff);
		fe->fe_buff = NULL;
	}

	if (fe->hash_map_tbl) {
		kfree(fe->hash_map_tbl);
		fe->hash_map_tbl = NULL;
	}

	if (fe->reg_buffer) {
		kfree(fe->reg_buffer);
		fe->reg_buffer = NULL;
	}

	return 0;
}

/* === IRQ context ops: no mutex, no state checks === */

static int isp_fe_ops_irq_read_fe_control_reg(struct vvcam_fe_dev *dev, uint32_t offset, uint32_t *val)
{
	if (WARN(!in_interrupt(), "%s called in non-interrupt context", __func__))
		return -EINVAL;
	if (WARN(!isp_fe_is_fe_control_register(offset), "%s called with invalid FE control register offset: 0x%08x", __func__, offset))
		return -EINVAL;
	*val = isp_fe_raw_read_reg(dev, offset);
	return 0;
}

static int isp_fe_ops_irq_read_control_reg(struct vvcam_fe_dev *dev, uint32_t offset, uint32_t *val)
{
	if (WARN(!in_interrupt(), "%s called in non-interrupt context", __func__))
		return -EINVAL;
	if (WARN(!isp_fe_is_control_register(offset), "%s called with invalid control register offset: 0x%08x", __func__, offset))
		return -EINVAL;
	if (WARN(atomic_read(&dev->fe_transaction_active), "%s called with active FE transaction", __func__))
		return -EINVAL;
	*val = isp_fe_raw_read_reg(dev, offset);
	return 0;
}

static int isp_fe_ops_irq_write_control_reg(struct vvcam_fe_dev *dev, uint32_t offset, uint32_t val)
{
	if (WARN(!in_interrupt(), "%s called in non-interrupt context", __func__))
		return -EINVAL;
	if (WARN(!isp_fe_is_non_fe_control_register(offset), "%s called with invalid non-FE control register offset: 0x%08x", __func__, offset))
		return -EINVAL;
	if (WARN(atomic_read(&dev->fe_transaction_active), "%s called with active FE transaction", __func__))
		return -EINVAL;
	isp_fe_raw_write_reg(dev, offset, val);
	return 0;
}

/* === API layer: mutex + state management === */

static int isp_fe_read_control_reg(struct vvcam_fe_dev *dev, uint32_t offset, uint32_t *val)
{
	int ret;
	struct vvcam_fe_driver_dev *pdriver_dev = platform_get_drvdata(to_platform_device(dev->dev));

	mutex_lock(&pdriver_dev->vvmutex);
	if (dev->fe.state != ISP_FE_STATE_GOT_BUFFER &&
	    dev->fe.state != ISP_FE_STATE_READY &&
	    dev->fe.state != ISP_FE_STATE_WAITING) {
		isp_err("%s: called in invalid state (%s), offset=0x%08x\n", __func__, isp_fe_state_to_str(dev->fe.state), offset);
		ret = -EINVAL;
		goto unlock;
	}
	if (!isp_fe_is_control_register(offset)) {
		isp_err("%s: invalid control register offset: 0x%08x\n", __func__, offset);
		ret = -EINVAL;
		goto unlock;
	}

	__isp_fe_read_control_reg(dev, offset, val);
	ret = 0;
unlock:
	mutex_unlock(&pdriver_dev->vvmutex);
	return ret;
}

static int isp_fe_write_control_reg(struct vvcam_fe_dev *dev, uint32_t offset, uint32_t val)
{
	int ret;
	struct vvcam_fe_driver_dev *pdriver_dev = platform_get_drvdata(to_platform_device(dev->dev));

	mutex_lock(&pdriver_dev->vvmutex);
	if (dev->fe.state == ISP_FE_STATE_GOT_BUFFER) {
		ret = isp_fe_get_reg_def_val(dev);
		if (ret) {
			isp_err("%s: failed to init cmd buffer, ret=%d\n", __func__, ret);
			goto unlock;
		}
		dev->fe.state = ISP_FE_STATE_READY;
	}
	if (dev->fe.state != ISP_FE_STATE_READY &&
	    dev->fe.state != ISP_FE_STATE_WAITING) {
		isp_err("%s: called in invalid state (%s), offset=0x%08x\n", __func__, isp_fe_state_to_str(dev->fe.state), offset);
		ret = -EINVAL;
		goto unlock;
	}
	if (!isp_fe_is_control_register(offset)) {
		isp_err("%s: invalid control register offset: 0x%08x\n", __func__, offset);
		ret = -EINVAL;
		goto unlock;
	}

	__isp_fe_write_control_reg(dev, offset, val);
	ret = 0;
unlock:
	mutex_unlock(&pdriver_dev->vvmutex);
	return ret;
}

static int isp_fe_read_vdid_reg(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t *val)
{
	int ret;
	struct vvcam_fe_driver_dev *pdriver_dev = platform_get_drvdata(to_platform_device(dev->dev));

	mutex_lock(&pdriver_dev->vvmutex);
	if (dev->fe.state == ISP_FE_STATE_GOT_BUFFER) {
		ret = isp_fe_get_reg_def_val(dev);
		if (ret) {
			isp_err("%s: failed to init cmd buffer, ret=%d\n", __func__, ret);
			goto unlock;
		}
		dev->fe.state = ISP_FE_STATE_READY;
	}
	if (dev->fe.state != ISP_FE_STATE_READY &&
	    dev->fe.state != ISP_FE_STATE_WAITING) {
		isp_err("%s: called in invalid state (%s), vdid=%d, offset=0x%08x\n", __func__, isp_fe_state_to_str(dev->fe.state), vdid, offset);
		ret = -EINVAL;
		goto unlock;
	}
	if (!isp_fe_is_vdid_register(offset)) {
		isp_err("%s: invalid VDID register offset: 0x%08x\n", __func__, offset);
		ret = -EINVAL;
		goto unlock;
	}
	if (vdid >= dev->fe.vdid_num) {
		isp_err("%s: vdid %d is out of range (max=%d)\n", __func__, vdid, dev->fe.vdid_num);
		ret = -EINVAL;
		goto unlock;
	}
	ret = __isp_fe_read_vdid_reg(dev, vdid, offset, val);
	if (ret)
		dev->fe.state = ISP_FE_STATE_ERROR;
unlock:
	mutex_unlock(&pdriver_dev->vvmutex);
	return ret;
}

static int isp_fe_write_vdid_reg(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t val)
{
	int ret;
	struct vvcam_fe_driver_dev *pdriver_dev = platform_get_drvdata(to_platform_device(dev->dev));

	mutex_lock(&pdriver_dev->vvmutex);
	if (dev->fe.state == ISP_FE_STATE_GOT_BUFFER) {
		ret = isp_fe_get_reg_def_val(dev);
		if (ret) {
			isp_err("%s: failed to init cmd buffer, ret=%d\n", __func__, ret);
			goto unlock;
		}
		dev->fe.state = ISP_FE_STATE_READY;
	}
	if (dev->fe.state != ISP_FE_STATE_READY &&
	    dev->fe.state != ISP_FE_STATE_WAITING) {
		isp_err("%s: called in invalid state (%s), vdid=%d, offset=0x%08x\n", __func__, isp_fe_state_to_str(dev->fe.state), vdid, offset);
		ret = -EINVAL;
		goto unlock;
	}
	if (!isp_fe_is_vdid_register(offset)) {
		isp_err("%s: invalid VDID register offset: 0x%08x\n", __func__, offset);
		ret = -EINVAL;
		goto unlock;
	}
	if (vdid >= dev->fe.vdid_num) {
		isp_err("%s: vdid %d is out of range (max=%d)\n", __func__, vdid, dev->fe.vdid_num);
		ret = -EINVAL;
		goto unlock;
	}
	ret = __isp_fe_write_vdid_reg(dev, vdid, offset, val);
	if (ret)
		dev->fe.state = ISP_FE_STATE_ERROR;
unlock:
	mutex_unlock(&pdriver_dev->vvmutex);
	return ret;
}

int isp_fe_write_tbl(struct vvcam_fe_dev *dev, uint8_t vdid, uint32_t offset, uint32_t val)
{
	int ret;
	struct vvcam_fe_driver_dev *pdriver_dev = platform_get_drvdata(to_platform_device(dev->dev));

	mutex_lock(&pdriver_dev->vvmutex);
	if (dev->fe.state != ISP_FE_STATE_READY &&
	    dev->fe.state != ISP_FE_STATE_WAITING) {
		isp_err("%s: called in invalid state (%s)\n", __func__, isp_fe_state_to_str(dev->fe.state));
		ret = -EINVAL;
		goto unlock;
	}
	if (vdid >= dev->fe.vdid_num) {
		isp_err("%s: vdid %d is out of range (max=%d)\n", __func__, vdid, dev->fe.vdid_num);
		ret = -EINVAL;
		goto unlock;
	}
	ret = __isp_fe_write_tbl(dev, vdid, offset, val);
	if (ret)
		dev->fe.state = ISP_FE_STATE_ERROR;
unlock:
	mutex_unlock(&pdriver_dev->vvmutex);
	return ret;
}

static int isp_fe_switch(struct vvcam_fe_dev *dev, struct isp_fe_switch_t *fe_switch)
{
	int ret;
	struct vvcam_fe_driver_dev *pdriver_dev = platform_get_drvdata(to_platform_device(dev->dev));

	mutex_lock(&pdriver_dev->vvmutex);
	if (dev->fe.state != ISP_FE_STATE_READY &&
	    dev->fe.state != ISP_FE_STATE_WAITING) {
		isp_err("%s: called in invalid state (%s)\n", __func__, isp_fe_state_to_str(dev->fe.state));
		ret = -EINVAL;
		goto unlock;
	}
	ret = __isp_fe_switch(dev, fe_switch);
	if (ret)
		dev->fe.state = ISP_FE_STATE_ERROR;
unlock:
	mutex_unlock(&pdriver_dev->vvmutex);
	return ret;
}

int isp_fe_get_status(struct vvcam_fe_dev *dev, struct isp_fe_status_t *status)
{
	int ret;
	struct vvcam_fe_driver_dev *pdriver_dev = platform_get_drvdata(to_platform_device(dev->dev));

	mutex_lock(&pdriver_dev->vvmutex);
	if (dev->fe.state == ISP_FE_STATE_EXIT) {
		isp_err("%s: called in invalid state (EXIT)\n", __func__);
		ret = -EINVAL;
		goto unlock;
	}
	ret = __isp_fe_get_status(dev, status);
unlock:
	mutex_unlock(&pdriver_dev->vvmutex);
	return ret;
}

int isp_fe_reset(struct vvcam_fe_dev *dev)
{
	int ret;
	struct vvcam_fe_driver_dev *pdriver_dev = platform_get_drvdata(to_platform_device(dev->dev));

	mutex_lock(&pdriver_dev->vvmutex);
	if (dev->fe.state != ISP_FE_STATE_INIT &&
	    dev->fe.state != ISP_FE_STATE_GOT_BUFFER &&
	    dev->fe.state != ISP_FE_STATE_READY &&
	    dev->fe.state != ISP_FE_STATE_WAITING &&
	    dev->fe.state != ISP_FE_STATE_ERROR) {
		isp_err("%s: called in invalid state (%s)\n", __func__, isp_fe_state_to_str(dev->fe.state));
		ret = -EINVAL;
		goto unlock;
	}
	ret = __isp_fe_reset(dev);
	if (ret)
		dev->fe.state = ISP_FE_STATE_ERROR;
	else
		dev->fe.state = ISP_FE_STATE_INIT;
unlock:
	mutex_unlock(&pdriver_dev->vvmutex);
	return ret;
}

int isp_fe_set_params(struct vvcam_fe_dev *dev, struct isp_fe_params_t *params)
{
	int ret;
	struct vvcam_fe_driver_dev *pdriver_dev = platform_get_drvdata(to_platform_device(dev->dev));

	mutex_lock(&pdriver_dev->vvmutex);
	if (dev->fe.state != ISP_FE_STATE_INIT &&
	    dev->fe.state != ISP_FE_STATE_GOT_BUFFER &&
	    dev->fe.state != ISP_FE_STATE_READY &&
	    dev->fe.state != ISP_FE_STATE_WAITING) {
		isp_err("%s: called in invalid state (%s)\n", __func__, isp_fe_state_to_str(dev->fe.state));
		ret = -EINVAL;
		goto unlock;
	}
	if (dev->fe.state != ISP_FE_STATE_INIT) {
		ret = __isp_fe_reset(dev);
		if (ret) {
			isp_err("%s: reset error %d\n", __func__, ret);
			dev->fe.state = ISP_FE_STATE_ERROR;
			goto unlock;
		}
		dev->fe.state = ISP_FE_STATE_INIT;
	}
	ret = __isp_fe_set_params(dev, params);
	if (!ret)
		dev->fe.state = ISP_FE_STATE_GOT_BUFFER;
unlock:
	mutex_unlock(&pdriver_dev->vvmutex);
	return ret;
}

static int vvcam_fe_dma_irq(struct vvcam_fe_dev *dev)
{
	u32 isp_fe_mis;

	if (WARN_ONCE((dev->fe.state == ISP_FE_STATE_INIT || dev->fe.state == ISP_FE_STATE_EXIT),
				  "fe dma irq handler called in unexpected state (%s)\n", isp_fe_state_to_str(dev->fe.state))) {
		return -EINVAL;
	}

	isp_fe_mis = isp_fe_raw_read_reg(dev, dev->fe.general_ctrl.fe_mis);
	if (isp_fe_mis) {
		isp_fe_raw_write_reg(dev, dev->fe.general_ctrl.fe_icr, isp_fe_mis);
		if (WARN_ONCE(dev->fe.state != ISP_FE_STATE_RUNNING, "fe dma irq fired in unexpected state (%s)\n", isp_fe_state_to_str(dev->fe.state))) {
			return -EINVAL;
		}
		if (isp_fe_mis & 0x01) {
#if VIV_ISP_FE_DMA_TIME_DEBUG == 1
			dev->fe.last_t_end_ns = ktime_get_ns();
#endif
			isp_fe_raw_write_reg(dev, dev->fe.general_ctrl.fe_ctrl, 0);
			complete_all(&dev->fe.fe_completion);
		}
	}

	return 0;
}

static int vvcam_fe_open(struct inode *inode, struct file *file)
{
	struct vvcam_fe_driver_dev *pdriver_dev;
	pdriver_dev = container_of(inode->i_cdev, struct vvcam_fe_driver_dev, cdev);
	file->private_data = pdriver_dev;
	return 0;
}

static long vvcam_fe_ioctl(struct file *file, unsigned int cmd, unsigned long args)
{
	long ret = 0;
	struct vvcam_fe_driver_dev *pdriver_dev;
	struct vvcam_fe_dev *pfe_dev;

	pdriver_dev = file->private_data;
	if (pdriver_dev == NULL) {
		pr_err("%s:file private is null point error\n", __func__);
		return -ENOMEM;
	}

	pfe_dev = pdriver_dev->private;

	switch (cmd) {
	case FEIOC_READ_REG: {
		struct isp_fe_virt_reg_t reg;

		if (copy_from_user(&reg, (void __user *)args, sizeof(reg)))
			return -EFAULT;
		if (isp_fe_is_control_register(reg.offset))
			ret = isp_fe_read_control_reg(pfe_dev, reg.offset, &reg.val);
		else
			ret = isp_fe_read_vdid_reg(pfe_dev, reg.vdid, reg.offset, &reg.val);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)args, &reg, sizeof(reg)))
			return -EFAULT;
		break;
	}
	case FEIOC_WRITE_REG: {
		struct isp_fe_virt_reg_t reg;

		if (copy_from_user(&reg, (void __user *)args, sizeof(reg)))
			return -EFAULT;
		if (isp_fe_is_control_register(reg.offset))
			ret = isp_fe_write_control_reg(pfe_dev, reg.offset, reg.val);
		else
			ret = isp_fe_write_vdid_reg(pfe_dev, reg.vdid, reg.offset, reg.val);
		break;
	}
	case FEIOC_WRITE_TBL: {
		struct isp_fe_virt_reg_t reg;

		if (copy_from_user(&reg, (void __user *)args, sizeof(reg)))
			return -EFAULT;
		ret = isp_fe_write_tbl(pfe_dev, reg.vdid, reg.offset, reg.val);
		break;
	}
	case FEIOC_G_FE_STATUS: {
		struct isp_fe_status_t fe_status;

		ret = isp_fe_get_status(pfe_dev, &fe_status);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)args, &fe_status, sizeof(fe_status)))
			return -EFAULT;
		break;
	}
	case FEIOC_S_FE_RESET:
		ret = isp_fe_reset(pfe_dev);
		break;
	case FEIOC_S_FE_PARAMS: {
		struct isp_fe_params_t fe_params;

		if (copy_from_user(&fe_params, (void __user *)args, sizeof(fe_params)))
			return -EFAULT;
		ret = isp_fe_set_params(pfe_dev, &fe_params);
		break;
	}
	case FEIOC_S_FE_SWITCH: {
		struct isp_fe_switch_t fe_switch;

		memset(&fe_switch, 0, sizeof(fe_switch));
		if (copy_from_user(&fe_switch, (void __user *)args, sizeof(fe_switch)))
			return -EFAULT;
		ret = isp_fe_switch(pfe_dev, &fe_switch);
		break;
	}
	default:
		ret = -EINVAL;
		isp_err("unsupported command 0x%04x", cmd);
		break;
	}

	return ret;
}

static int vvcam_fe_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct vvcam_fe_driver_dev *pdriver_dev;
	struct vvcam_fe_dev *pfe_dev;

	pdriver_dev = container_of(inode->i_cdev, struct vvcam_fe_driver_dev, cdev);
	pfe_dev = pdriver_dev->private;

	ret = isp_fe_reset(pfe_dev);
	if (ret)
		pr_err("%s: release fe error(%d)!\n", __func__, ret);

	return ret;
}

static int vvcam_fe_mmap(struct file *pFile, struct vm_area_struct *vma)
{
	int ret = 0;
	struct vvcam_fe_driver_dev *pdriver_dev = pFile->private_data;
	struct vvcam_fe_dev *pfe_dev = pdriver_dev->private;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long phy_base = 0;
	unsigned long pfn_start = 0;

	phy_base = pfe_dev->phybase;
	pfn_start = (phy_base >> PAGE_SHIFT) + vma->vm_pgoff;

	pr_info("%s:phy_base: 0x%lx, vir_addr:0x%lx,size: 0x%lx\n", __func__, phy_base, vma->vm_start, size);
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, pfn_start, size, vma->vm_page_prot)) {
		pr_err("-->%s: remap_pfn_range error!\n", __func__);
		return -EIO;
	}

	return ret;
}

void hailo15_fe_get_dev(struct vvcam_fe_dev** dev){
	*dev = fe_dev;
}
EXPORT_SYMBOL(hailo15_fe_get_dev);

void hailo15_fe_set_address_space_base(struct vvcam_fe_dev* fe_dev, void __iomem* base){
	fe_dev->base = base;
}
EXPORT_SYMBOL(hailo15_fe_set_address_space_base);

static struct file_operations vvcam_fe_fops = {
	.owner = THIS_MODULE,
	.open = vvcam_fe_open,
	.release = vvcam_fe_release,
	.unlocked_ioctl = vvcam_fe_ioctl,
	.mmap = vvcam_fe_mmap,
};

static int vvcam_fe_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct vvcam_fe_driver_dev *pdriver_dev;
	struct vvcam_fe_dev *pfe_dev;

	pr_info("enter %s\n", __func__);

	if (pdev->id >= VIVCAM_FE_MAXCNT) {
		pr_err("%s:pdev id is %d error\n", __func__, pdev->id);
		return -EINVAL;
	}

	pdriver_dev = devm_kzalloc(&pdev->dev, sizeof(struct vvcam_fe_driver_dev), GFP_KERNEL);
	if (pdriver_dev == NULL) {
		pr_err("%s:alloc struct vvcam_fe_driver_dev error\n", __func__);
		return -ENOMEM;
	}
	memset(pdriver_dev, 0, sizeof(struct vvcam_fe_driver_dev));
	pr_info("%s:isp[%d]: pdriver_dev =0x%px\n", __func__, pdev->id, pdriver_dev);

	pfe_dev = devm_kzalloc(&pdev->dev, sizeof(struct vvcam_fe_dev), GFP_KERNEL);
	if (pfe_dev == NULL) {
		pr_err("%s:alloc struct vvcam_fe_dev error\n", __func__);
		return -ENOMEM;
	}
	memset(pfe_dev, 0, sizeof(struct vvcam_fe_dev));
	pr_info("%s:isp[%d]: psensor_dev =0x%px\n", __func__, pdev->id, pfe_dev);

	pdriver_dev->private = pfe_dev;
	pdriver_dev->device_idx = pdev->id;
	pfe_dev->id = pdev->id;
	pfe_dev->dev = &pdev->dev;

	mutex_init(&pdriver_dev->vvmutex);
	platform_set_drvdata(pdev, pdriver_dev);

	if (fe_register_index == 0) {
		if (vvcam_fe_major == 0) {
			ret = alloc_chrdev_region(&pdriver_dev->devt, 0, VIVCAM_FE_MAXCNT, VIVCAM_FE_NAME);
			if (ret != 0) {
				pr_err("%s:alloc_chrdev_region error\n", __func__);
				return ret;
			}
			vvcam_fe_major = MAJOR(pdriver_dev->devt);
			vvcam_fe_minor = MINOR(pdriver_dev->devt);
		} else {

			pdriver_dev->devt = MKDEV(vvcam_fe_major, vvcam_fe_minor);
			ret = register_chrdev_region(pdriver_dev->devt, VIVCAM_FE_MAXCNT, VIVCAM_FE_NAME);
			if (ret) {
				pr_err("%s:register_chrdev_region error\n", __func__);
				return ret;
			}
		}
		vvcam_fe_class = class_create(THIS_MODULE, VIVCAM_FE_NAME);
		if (IS_ERR(vvcam_fe_class)) {
			pr_err("%s[%d]:class_create error!\n", __func__, __LINE__);
			return -EINVAL;
		}
	}
	pdriver_dev->devt = MKDEV(vvcam_fe_major, vvcam_fe_minor + pdev->id);

	cdev_init(&pdriver_dev->cdev, &vvcam_fe_fops);
	ret = cdev_add(&pdriver_dev->cdev, pdriver_dev->devt, 1);
	if (ret) {
		pr_err("%s[%d]:cdev_add error!\n", __func__, __LINE__);
		return ret;
	}
	pdriver_dev->class = vvcam_fe_class;
	device_create(pdriver_dev->class, NULL, pdriver_dev->devt, pdriver_dev, "%s%d", VIVCAM_FE_NAME, pdev->id);

	if (pdriver_dev->device_idx == 0) {
		pfe_dev->fe.enable = fe0_enable;
	} else if (pdriver_dev->device_idx == 1) {
		pfe_dev->fe.enable = fe1_enable;
	} else {
		pfe_dev->fe.enable = false;
	}
	if (pfe_dev->fe.enable == true) {
		ret = isp_fe_init(pfe_dev);
		if (ret) {
			pr_err("%s: isp_fe_init failed, ret=%d\n", __func__, ret);
			return ret;
		}
	}

	pfe_dev->fe_get_vdid = isp_fe_get_vdid;
	pfe_dev->fe_dma_irq = vvcam_fe_dma_irq;
	pfe_dev->irq_read_fe_control_reg = isp_fe_ops_irq_read_fe_control_reg;
	pfe_dev->irq_read_control_reg = isp_fe_ops_irq_read_control_reg;
	pfe_dev->irq_write_control_reg = isp_fe_ops_irq_write_control_reg;
	pfe_dev->read_control_reg = isp_fe_read_control_reg;
	pfe_dev->write_control_reg = isp_fe_write_control_reg;
	pfe_dev->read_vdid_reg = isp_fe_read_vdid_reg;
	pfe_dev->write_vdid_reg = isp_fe_write_vdid_reg;
	pfe_dev->fe_switch = isp_fe_switch;

	fe_register_index++;
	fe_dev = pfe_dev;
	pr_info("exit %s\n", __func__);
	return ret;
}

static int vvcam_fe_remove(struct platform_device *pdev)
{
	int ret = 0;
	struct vvcam_fe_driver_dev *pdriver_dev;
	struct vvcam_fe_dev *pfe_dev;

	pr_info("enter %s\n", __func__);
	fe_register_index--;
	pdriver_dev = platform_get_drvdata(pdev);

	pfe_dev = (struct vvcam_fe_dev *)pdriver_dev->private;
	if (pfe_dev->fe.enable == true) {
		ret = isp_fe_destory(pfe_dev);
		if (ret)
			pr_err("%s: isp_fe_destory failed, ret=%d\n", __func__, ret);
	}

	cdev_del(&pdriver_dev->cdev);
	device_destroy(pdriver_dev->class, pdriver_dev->devt);
	unregister_chrdev_region(pdriver_dev->devt, VIVCAM_FE_MAXCNT);
	if (fe_register_index == 0) {
		class_destroy(pdriver_dev->class);
	}
	return ret;
}

static struct platform_driver vvcam_fe_driver = {
	.probe = vvcam_fe_probe,
	.remove = vvcam_fe_remove,
	.driver = {
		.name = VIVCAM_FE_NAME,
	}
};

static void vvcam_fe_pdev_release(struct device *dev)
{
	pr_info("enter %s\n", __func__);
}

static struct platform_device vvcam_fe0_pdev = {
	.name = VIVCAM_FE_NAME,
	.id = 0,
	.dev = {
		.release = vvcam_fe_pdev_release,
	}
};

static struct platform_device vvcam_fe1_pdev = {
	.name = VIVCAM_FE_NAME,
	.id = 1,
	.dev = {
		.release = vvcam_fe_pdev_release,
	}
};

static int __init vvcam_fe_init(void)
{
	int ret = 0;

	pr_info("enter %s\n", __func__);

	if (fe0_enable == true) {
		ret = platform_device_register(&vvcam_fe0_pdev);
		if (ret) {
			pr_err("register platform device0 failed.\n");
			return ret;
		}
	}

	if (fe1_enable == true) {
		ret = platform_device_register(&vvcam_fe1_pdev);
		if (ret) {
			pr_err("register platform device1 failed.\n");
			return ret;
		}
	}

	ret = platform_driver_register(&vvcam_fe_driver);
	if (ret) {
		pr_err("register platform driver failed.\n");
		return ret;
	}

	return ret;
}

static void __exit vvcam_fe_exit(void)
{
	pr_info("enter %s\n", __func__);

	platform_driver_unregister(&vvcam_fe_driver);

	if (fe0_enable == true) {
		platform_device_unregister(&vvcam_fe0_pdev);
	}

	if (fe1_enable == true) {
		platform_device_unregister(&vvcam_fe1_pdev);
	}

}

module_init(vvcam_fe_init);
module_exit(vvcam_fe_exit);

MODULE_DESCRIPTION("ISP-FE");
MODULE_LICENSE("GPL");
