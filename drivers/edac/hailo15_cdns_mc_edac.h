/******************************************************************************
*  Legal notice:
* Copyright (C) 2012-2023 Cadence Design Systems, Inc.
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* 3. Neither the name of the copyright holder nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
******************************************************************************/

/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _CDNS_MC_EDAC_H
#define _CDNS_MC_EDAC_H

#include <linux/edac.h>
#include <linux/types.h>

#define CDNS_REG_MASK(_name)                                                   \
	GENMASK(_name##_OFFSET + _name##_WIDTH - 1, _name##_OFFSET)

/*!
 *	DDR controller regs
 */

/* DDR CTRL ID & Version */
#define CDNS_CTRL_CONTROLLER_ID_ADDR 0
#define CDNS_CTRL_CONTROLLER_ID_OFFSET 16
#define CDNS_CTRL_CONTROLLER_ID_WIDTH 16

/* MRRs (mode register reads) */
#define CDNS_CTRL_READ_MODEREG_ADDR 159
#define CDNS_CTRL_READ_MODEREG_OFFSET 8
#define CDNS_CTRL_READ_MODEREG_WIDTH 17

#define CDNS_CTRL_READ_MODEREG_ID_ADDR 159
#define CDNS_CTRL_READ_MODEREG_ID_OFFSET 8
#define CDNS_CTRL_READ_MODEREG_ID_WIDTH 8

#define CDNS_CTRL_READ_MODEREG_CS_ADDR 159
#define CDNS_CTRL_READ_MODEREG_CS_OFFSET 16
#define CDNS_CTRL_READ_MODEREG_CS_WIDTH 8

#define CDNS_CTRL_READ_MODEREG_TRIGGER_ADDR 159
#define CDNS_CTRL_READ_MODEREG_TRIGGER_OFFSET 24
#define CDNS_CTRL_READ_MODEREG_TRIGGER_WIDTH 1

/* MRs Data */
#define CDNS_CTRL_PERIPHERAL_MRR_DATA_ADDR 160
#define CDNS_CTRL_PERIPHERAL_MRR_DATA_OFFSET 0
#define CDNS_CTRL_PERIPHERAL_MRR_DATA_WIDTH 8

#define CDNS_CTRL_MEMDATA_RATIO_0_ADDR 290
#define CDNS_CTRL_MEMDATA_RATIO_0_OFFSET 8
#define CDNS_CTRL_MEMDATA_RATIO_0_WIDTH 3

#define CDNS_CTRL_MEMDATA_RATIO_1_ADDR 291
#define CDNS_CTRL_MEMDATA_RATIO_1_OFFSET 16
#define CDNS_CTRL_MEMDATA_RATIO_1_WIDTH 3

#define CDNS_CTRL_CS_MAP_ADDR 282
#define CDNS_CTRL_CS_MAP_OFFSET 16
#define CDNS_CTRL_CS_MAP_WIDTH 2

/* LPDDR MR8 regs per CS #0 */
#define CDNS_CTRL_MR8_DATA_0_ADDR 177
#define CDNS_CTRL_MR8_DATA_0_OFFSET 8
#define CDNS_CTRL_MR8_DATA_0_WIDTH 8

/* LPDDR MR8 regs per CS #0 fields */
#define CDNS_CTRL_MR8_DATA_0_TYPE_ADDR 177
#define CDNS_CTRL_MR8_DATA_0_TYPE_OFFSET 8
#define CDNS_CTRL_MR8_DATA_0_TYPE_WIDTH 2

#define CDNS_CTRL_MR8_DATA_0_DENSITY_ADDR 177
#define CDNS_CTRL_MR8_DATA_0_DENSITY_OFFSET 10
#define CDNS_CTRL_MR8_DATA_0_DENSITY_WIDTH 4

#define CDNS_CTRL_MR8_DATA_0_IO_WIDTH_ADDR 177
#define CDNS_CTRL_MR8_DATA_0_IO_WIDTH_OFFSET 14
#define CDNS_CTRL_MR8_DATA_0_IO_WIDTH_WIDTH 2

/* LPDDR MR8 regs per CS #1 */
#define CDNS_CTRL_MR8_DATA_1_ADDR 184
#define CDNS_CTRL_MR8_DATA_1_OFFSET 24
#define CDNS_CTRL_MR8_DATA_1_WIDTH 8

/* LPDDR MR8 regs per CS #1 fields */
#define CDNS_CTRL_MR8_DATA_1_TYPE_ADDR 184
#define CDNS_CTRL_MR8_DATA_1_TYPE_OFFSET 24
#define CDNS_CTRL_MR8_DATA_1_TYPE_WIDTH 8

#define CDNS_CTRL_MR8_DATA_1_DENSITY_ADDR 184
#define CDNS_CTRL_MR8_DATA_1_DENSITY_OFFSET 24
#define CDNS_CTRL_MR8_DATA_1_DENSITY_WIDTH 8

#define CDNS_CTRL_MR8_DATA_1_IO_WIDTH_ADDR 184
#define CDNS_CTRL_MR8_DATA_1_IO_WIDTH_OFFSET 24
#define CDNS_CTRL_MR8_DATA_1_IO_WIDTH_WIDTH 8

/* ECC Correctable info */
#define CDNS_CTRL_ECC_C_ADDR_LSB_ADDR 213
#define CDNS_CTRL_ECC_C_ADDR_LSB_OFFSET 0
#define CDNS_CTRL_ECC_C_ADDR_LSB_WIDTH 32

#define CDNS_CTRL_ECC_C_ADDR_MSB_ADDR 214
#define CDNS_CTRL_ECC_C_ADDR_MSB_OFFSET 0
#define CDNS_CTRL_ECC_C_ADDR_MSB_WIDTH 3

#define CDNS_CTRL_ECC_C_DATA_LSB_ADDR 215
#define CDNS_CTRL_ECC_C_DATA_LSB_OFFSET 0
#define CDNS_CTRL_ECC_C_DATA_LSB_WIDTH 32

#define CDNS_CTRL_ECC_C_DATA_MSB_ADDR 216
#define CDNS_CTRL_ECC_C_DATA_MSB_OFFSET 0
#define CDNS_CTRL_ECC_C_DATA_MSB_WIDTH 32

#define CDNS_CTRL_ECC_C_ID_ADDR 217
#define CDNS_CTRL_ECC_C_ID_OFFSET 8
#define CDNS_CTRL_ECC_C_ID_WIDTH 7

#define CDNS_CTRL_ECC_C_SYND_ADDR 214
#define CDNS_CTRL_ECC_C_SYND_OFFSET 8
#define CDNS_CTRL_ECC_C_SYND_WIDTH 8

/* ECC Un-Correctable info */
#define CDNS_CTRL_ECC_U_ADDR_LSB_ADDR 209
#define CDNS_CTRL_ECC_U_ADDR_LSB_OFFSET 0
#define CDNS_CTRL_ECC_U_ADDR_LSB_WIDTH 32

#define CDNS_CTRL_ECC_U_ADDR_MSB_ADDR 210
#define CDNS_CTRL_ECC_U_ADDR_MSB_OFFSET 0
#define CDNS_CTRL_ECC_U_ADDR_MSB_WIDTH 3

#define CDNS_CTRL_ECC_U_DATA_LSB_ADDR 211
#define CDNS_CTRL_ECC_U_DATA_LSB_OFFSET 0
#define CDNS_CTRL_ECC_U_DATA_LSB_WIDTH 32

#define CDNS_CTRL_ECC_U_DATA_MSB_ADDR 212
#define CDNS_CTRL_ECC_U_DATA_MSB_OFFSET 0
#define CDNS_CTRL_ECC_U_DATA_MSB_WIDTH 32

#define CDNS_CTRL_ECC_U_ID_ADDR 217
#define CDNS_CTRL_ECC_U_ID_OFFSET 0
#define CDNS_CTRL_ECC_U_ID_WIDTH 7

#define CDNS_CTRL_ECC_U_SYND_ADDR 210
#define CDNS_CTRL_ECC_U_SYND_OFFSET 8
#define CDNS_CTRL_ECC_U_SYND_WIDTH 8

/*!	global_error_info (8 bits):
		• Bit [7] = A queue/FIFO parity error has been detected. Refer to the fault_fifo_protection_status parameter for details on the cause of this interrupt.
		• Bit [6] = A logic replication compare or internal parity error has been detected.
		• Bit [5] = A programmable register parity error has been detected. Refer to the regport_param_parity_protection_status parameter for details on the cause of this interrupt.
		• Bit [4] = A timeout timer error has been detected. Refer to the timeout_timer_log parameter for details on the cause of this interrupt.
		• Bit [3] = A write data parity error, an overlapping write data parity error or an AXI command/address parity error has been detected. Refer to the axi_parity_error_status parameter for details on the cause of this interrupt.
		• Bit [2] = Multiple ECC events have occurred in this transaction.
		• Bit [1] = An uncorrectable ECC event has been detected.
		• Bit [0] = A correctable ECC event has been detected
		For each bit:
		• ’b0 = No error condition
		• ’b1 = Error condition occurred. Write a ’b1 to this bit to clear it.

	Output Signals:
	• ddr_controller_global_error_int_fatal    (for a un-correctable error):	global_error_info[7:1]
	• ddr_controller_global_error_int_nonfatal (for a correctable error):		global_error_info[0]

	global_error_mask (8 bits):
		• Active-high mask bits that control the output signals value on the ASIC interface:
		- ddr_controller_global_error_int_fatal			<-   ( !global_error_mask[7:1]  & global_error_info[7:1] )			
		- ddr_controller_global_error_int_nonfatal		<-   ( !global_error_mask[0]    & global_error_info[0] )
		For each bit:
		• ’b0 = Do not mask interrupt
		• ’b1 = Mask interrupt. This will prevent a set interrupt from causing the ddr_controller_global_error_int_fatal signal to be asserted.

	int_status:
	• int_status [3]: Set to ’b1 if __another__ correctable ECC event has been detected on a read operation, prior to the initial event being acknowledged.
	• int_status [4]: Set to ’b1 if __another__ un-correctable ECC event has been detected on a read operation, prior to the initial event being acknowledged.
	int_mask:
	• int_mask [4:3]: Mask field to inhibit assertion of ECC interrupt signals.
*/
#define CDNS_CTRL_GLOBAL_ERROR_INFO_ADDR 384
#define CDNS_CTRL_GLOBAL_ERROR_INFO_OFFSET 24
#define CDNS_CTRL_GLOBAL_ERROR_INFO_WIDTH 8

#define CDNS_CTRL_GLOBAL_ERROR_INFO_C_ECC_ADDR 384
#define CDNS_CTRL_GLOBAL_ERROR_INFO_C_ECC_OFFSET 24
#define CDNS_CTRL_GLOBAL_ERROR_INFO_C_ECC_WIDTH 1

#define CDNS_CTRL_GLOBAL_ERROR_INFO_U_ECC_ADDR 384
#define CDNS_CTRL_GLOBAL_ERROR_INFO_U_ECC_OFFSET 25
#define CDNS_CTRL_GLOBAL_ERROR_INFO_U_ECC_WIDTH 1

#define CDNS_CTRL_GLOBAL_ERROR_INFO_MULTI_ECC_EV_ADDR 384
#define CDNS_CTRL_GLOBAL_ERROR_INFO_MULTI_ECC_EV_OFFSET 26
#define CDNS_CTRL_GLOBAL_ERROR_INFO_MULTI_ECC_EV_WIDTH 1

#define CDNS_CTRL_GLOBAL_ERROR_MASK_ADDR 385
#define CDNS_CTRL_GLOBAL_ERROR_MASK_OFFSET 24
#define CDNS_CTRL_GLOBAL_ERROR_MASK_WIDTH 8

#define CDNS_CTRL_GLOBAL_ERROR_MASK_C_ECC_ADDR 384
#define CDNS_CTRL_GLOBAL_ERROR_MASK_C_ECC_OFFSET 24
#define CDNS_CTRL_GLOBAL_ERROR_MASK_C_ECC_WIDTH 1

#define CDNS_CTRL_GLOBAL_ERROR_MASK_U_ECC_ADDR 384
#define CDNS_CTRL_GLOBAL_ERROR_MASK_U_ECC_OFFSET 25
#define CDNS_CTRL_GLOBAL_ERROR_MASK_U_ECC_WIDTH 1

#define CDNS_CTRL_GLOBAL_ERROR_MASK_MULTI_ECC_EV_ADDR 384
#define CDNS_CTRL_GLOBAL_ERROR_MASK_MULTI_ECC_EV_OFFSET 26
#define CDNS_CTRL_GLOBAL_ERROR_MASK_MULTI_ECC_EV_WIDTH 1

/*! INT_STATUS/INT_ACK/INT_MASK:
	• Bit [35] = Logical OR of all lower bits.
	• Bit [34] = The controller has entered the software-requested mode.
	• Bit [33] = The ZQ calibration operation has resulted in a status bit being set. Refer to the ZQ_STATUS_LOG parameter for more information.
	• Bit [32] = The refresh operation has resulted in a status bit being set.
	• Bit [31] = The DFS operation has resulted in a status bit being set.
	• Bit [30] = The DFS hardware has completed all operations.
	• Bit [29] = The DFI tINIT_COMPLETE value has timed out. This value is specified in the TDFI_INIT_COMPLETE parameter.
	• Bit [28] = The user-initiated DLL resync has completed.
	• Bit [27] = A state change has been detected on the dfi_init_complete signal after initialization.
	• Bit [26] = The assertion of the INHIBIT_DRAM_CMD parameter has successfully inhibited the command queue and/or MRR traffic.
	• Bit [25] = The register interface-initiated mode register write has completed and another mode register write may be issued.
	• Bit [24] = A DQS oscillator measurement has been detected to be out of variance.
	• Bit [23] = A DQS oscillator measurement overflow has been detected.
	• Bit [22] = The DQS oscillator has updated the base values.
	• Bit [21] = The software-requested DQS Oscillator measurement has completed.
	• Bit [20] = A temperature alert condition (low or high temp) has been detected.
	• Bit [19] = The last automatic MRR of MR4 indicated a change in the device temperature or refresh rate (TUF bit set).
	• Bit [18] = The requested mode register read has completed. The chip and data can be read in the PERIPHERAL_MRR_DATA parameter.
	• Bit [17] = Error received from the PHY on the DFI bus.
	• Bit [16] = A DFI PHY Master Interface error has occurred. Error information can be found in the PHYMSTR_ERROR_STATUS parameter.
	• Bit [15] = An MRR error has occurred. Error information can be found in the MRR_ERROR_STATUS parameter.
	• Bit [14] = A DFI update error has occurred. Error information can be found in the UPDATE_ERROR_STATUS parameter.
	• Bit [13] = The user has programmed an invalid setting associated with user words per burst. Examples: Setting param_reduc when burst length = 2. A 1:2 MC:PHY clock ratio with burst length = 2.
	• Bit [12] = A wrap cycle crossing a DRAM page has been detected. This is unsupported & may result in memory data corruption.
	• Bit [11] = The BIST operation has been completed.
	• Bit [10] = The low power operation has been completed.
	• Bit [9] = The MC initialization has been completed.
	• Bit [8] = An error occurred on the port command channel.
	• Bit [7] = An ECC correctable error has been detected in a scrubbing read operation.
	• Bit [6] = The scrub operation triggered by setting param_ecc_scrub_start has completed.
	• Bit [5] = One or more ECC writeback commands could not be executed.
	• Bit [4] = Multiple uncorrectable ECC events have been detected.
	• Bit [3] = Multiple correctable ECC events have been detected.
	• Bit [2] = Multiple accesses outside the defined PHYSICAL memory space have occurred.
	• Bit [1] = A memory access outside the defined PHYSICAL memory space has occurred.
	• Bit [0] = The memory reset is valid on the DFI bus.
 */
#define CDNS_CTRL_INT_ACK_LSB_ADDR 300
#define CDNS_CTRL_INT_ACK_LSB_OFFSET 0
#define CDNS_CTRL_INT_ACK_LSB_WIDTH 32

#define CDNS_CTRL_INT_ACK_MSB_ADDR 301
#define CDNS_CTRL_INT_ACK_MSB_OFFSET 0
#define CDNS_CTRL_INT_ACK_MSB_WIDTH 3

#define CDNS_CTRL_INT_ACK_LSB_MODEREG_DONE_ADDR 300
#define CDNS_CTRL_INT_ACK_LSB_MODEREG_DONE_OFFSET 18
#define CDNS_CTRL_INT_ACK_LSB_MODEREG_DONE_WIDTH 1

#define CDNS_CTRL_INT_ACK_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED_ADDR 300
#define CDNS_CTRL_INT_ACK_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED_OFFSET 3
#define CDNS_CTRL_INT_ACK_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED_WIDTH 1

#define CDNS_CTRL_INT_ACK_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED_ADDR 302
#define CDNS_CTRL_INT_ACK_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED_OFFSET 4
#define CDNS_CTRL_INT_ACK_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED_WIDTH 1

#define CDNS_CTRL_INT_MASK_LSB_ADDR 302
#define CDNS_CTRL_INT_MASK_LSB_OFFSET 0
#define CDNS_CTRL_INT_MASK_LSB_WIDTH 32

#define CDNS_CTRL_INT_MASK_MSB_ADDR 303
#define CDNS_CTRL_INT_MASK_MSB_OFFSET 0
#define CDNS_CTRL_INT_MASK_MSB_WIDTH 4

#define CDNS_CTRL_INT_MASK_LSB_MODEREG_DONE_ADDR 302
#define CDNS_CTRL_INT_MASK_LSB_MODEREG_DONE_OFFSET 18
#define CDNS_CTRL_INT_MASK_LSB_MODEREG_DONE_WIDTH 1

#define CDNS_CTRL_INT_MASK_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED_ADDR 302
#define CDNS_CTRL_INT_MASK_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED_OFFSET 3
#define CDNS_CTRL_INT_MASK_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED_WIDTH 1

#define CDNS_CTRL_INT_MASK_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED_ADDR 302
#define CDNS_CTRL_INT_MASK_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED_OFFSET 4
#define CDNS_CTRL_INT_MASK_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED_WIDTH 1

#define CDNS_CTRL_INT_STATUS_LSB_ADDR 298
#define CDNS_CTRL_INT_STATUS_LSB_OFFSET 0
#define CDNS_CTRL_INT_STATUS_LSB_WIDTH 32

#define CDNS_CTRL_INT_STATUS_MSB_ADDR 299
#define CDNS_CTRL_INT_STATUS_MSB_OFFSET 0
#define CDNS_CTRL_INT_STATUS_MSB_WIDTH 4

#define CDNS_CTRL_INT_STATUS_LSB_MODEREG_DONE_ADDR 298
#define CDNS_CTRL_INT_STATUS_LSB_MODEREG_DONE_OFFSET 18
#define CDNS_CTRL_INT_STATUS_LSB_MODEREG_DONE_WIDTH 1

#define CDNS_CTRL_INT_STATUS_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED_ADDR 298
#define CDNS_CTRL_INT_STATUS_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED_OFFSET 3
#define CDNS_CTRL_INT_STATUS_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED_WIDTH 1

#define CDNS_CTRL_INT_STATUS_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED_ADDR  \
	298
#define CDNS_CTRL_INT_STATUS_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED_OFFSET \
	4
#define CDNS_CTRL_INT_STATUS_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED_WIDTH 1

/*! ECC Configuration Param */
#define CDNS_CTRL_INLINE_ECC_BANK_OFFSET_ADDR 205
#define CDNS_CTRL_INLINE_ECC_BANK_OFFSET_OFFSET 24
#define CDNS_CTRL_INLINE_ECC_BANK_OFFSET_WIDTH 3

#define CDNS_CTRL_ECC_DISABLE_W_UC_ERR_ADDR 208
#define CDNS_CTRL_ECC_DISABLE_W_UC_ERR_OFFSET 0
#define CDNS_CTRL_ECC_DISABLE_W_UC_ERR_WIDTH 1

#define CDNS_CTRL_ECC_ENABLE_ADDR 205
#define CDNS_CTRL_ECC_ENABLE_OFFSET 16
#define CDNS_CTRL_ECC_ENABLE_WIDTH 2

#define CDNS_CTRL_ECC_READ_CACHING_EN_ADDR 206
#define CDNS_CTRL_ECC_READ_CACHING_EN_OFFSET 0
#define CDNS_CTRL_ECC_READ_CACHING_EN_WIDTH 1

#define CDNS_CTRL_ECC_WRITEBACK_EN_ADDR 207
#define CDNS_CTRL_ECC_WRITEBACK_EN_OFFSET 24
#define CDNS_CTRL_ECC_WRITEBACK_EN_WIDTH 1

#define CDNS_CTRL_ECC_WRITE_COMBINING_EN_ADDR 206
#define CDNS_CTRL_ECC_WRITE_COMBINING_EN_OFFSET 8
#define CDNS_CTRL_ECC_WRITE_COMBINING_EN_WIDTH 1

/*! Scrubbing regs */
#define CDNS_CTRL_ECC_SCRUB_END_ADDR_LSB_ADDR 225
#define CDNS_CTRL_ECC_SCRUB_END_ADDR_LSB_OFFSET 0
#define CDNS_CTRL_ECC_SCRUB_END_ADDR_LSB_WIDTH 32

#define CDNS_CTRL_ECC_SCRUB_END_ADDR_MSB_ADDR 226
#define CDNS_CTRL_ECC_SCRUB_END_ADDR_MSB_OFFSET 0
#define CDNS_CTRL_ECC_SCRUB_END_ADDR_MSB_WIDTH 3

#define CDNS_CTRL_ECC_SCRUB_IDLE_CNT_ADDR 222
#define CDNS_CTRL_ECC_SCRUB_IDLE_CNT_OFFSET 16
#define CDNS_CTRL_ECC_SCRUB_IDLE_CNT_WIDTH 16

#define CDNS_CTRL_ECC_SCRUB_INTERVAL_ADDR 222
#define CDNS_CTRL_ECC_SCRUB_INTERVAL_OFFSET 0
#define CDNS_CTRL_ECC_SCRUB_INTERVAL_WIDTH 16

#define CDNS_CTRL_ECC_SCRUB_IN_PROGRESS_ADDR 221
#define CDNS_CTRL_ECC_SCRUB_IN_PROGRESS_OFFSET 0
#define CDNS_CTRL_ECC_SCRUB_IN_PROGRESS_WIDTH 1

#define CDNS_CTRL_ECC_SCRUB_LEN_ADDR 221
#define CDNS_CTRL_ECC_SCRUB_LEN_OFFSET 8
#define CDNS_CTRL_ECC_SCRUB_LEN_WIDTH 13

#define CDNS_CTRL_ECC_SCRUB_MODE_ADDR 221
#define CDNS_CTRL_ECC_SCRUB_MODE_OFFSET 24
#define CDNS_CTRL_ECC_SCRUB_MODE_WIDTH 1

#define CDNS_CTRL_ECC_SCRUB_START_ADDR 220
#define CDNS_CTRL_ECC_SCRUB_START_OFFSET 24
#define CDNS_CTRL_ECC_SCRUB_START_WIDTH 1

#define CDNS_CTRL_ECC_SCRUB_START_ADDRESS_LSB_ADDR 223
#define CDNS_CTRL_ECC_SCRUB_START_ADDRESS_LSB_OFFSET 0
#define CDNS_CTRL_ECC_SCRUB_START_ADDRESS_LSB_WIDTH 32

#define CDNS_CTRL_ECC_SCRUB_START_ADDRESS_MSB_ADDR 224
#define CDNS_CTRL_ECC_SCRUB_START_ADDRESS_MSB_OFFSET 0
#define CDNS_CTRL_ECC_SCRUB_START_ADDRESS_MSB_WIDTH 3

/*! U32 reg ops
 * [in] baseaddr: U32 regs array base address
 * [in] reg: register index in u32 regs array
 * [in] offset: field shift offset
 * [in] mask: field mask (shifted)
 */
static inline u32 _reg32_rd(const void __iomem *baseaddr, int reg, u32 offset,
			    u32 mask)
{
	return (((readl(baseaddr + reg)) & mask) >> offset);
}

static inline void _reg32_modify(void __iomem *baseaddr, int reg, u32 offset,
				 u32 mask, u32 val)
{
	u32 reg_val = ((readl(baseaddr + reg) & ~mask) | (val & mask));
	writel(reg_val, baseaddr + reg);
}

static inline void _reg32_clr(void __iomem *baseaddr, int reg, u32 offset,
			      u32 mask)
{
	u32 reg_val = (readl(baseaddr + reg) & ~mask);
	writel(reg_val, baseaddr + reg);
}

static inline void _reg32_set(void __iomem *baseaddr, int reg, u32 offset,
			      u32 mask)
{
	u32 reg_val = ((readl(baseaddr + reg) & ~mask) | (mask));
	writel(reg_val, baseaddr + reg);
}

/* DDR controller regs API */
#define ddr_ctrl_reg32_rd(_baseaddr, _reg)                                     \
	_reg32_rd(_baseaddr, (CDNS_CTRL_##_reg##_ADDR << 2),                   \
		  CDNS_CTRL_##_reg##_OFFSET, CDNS_REG_MASK(CDNS_CTRL_##_reg))
#define ddr_ctrl_reg32_modify(_baseaddr, _reg, _val)                           \
	_reg32_modify(_baseaddr, (CDNS_CTRL_##_reg##_ADDR << 2),               \
		      CDNS_CTRL_##_reg##_OFFSET,                               \
		      CDNS_REG_MASK(CDNS_CTRL_##_reg), _val)
#define ddr_ctrl_reg32_clr(_baseaddr, _reg)                                    \
	_reg32_clr(_baseaddr, (CDNS_CTRL_##_reg##_ADDR << 2),                  \
		   CDNS_CTRL_##_reg##_OFFSET, CDNS_REG_MASK(CDNS_CTRL_##_reg))
#define ddr_ctrl_reg32_set(_baseaddr, _reg)                                    \
	_reg32_set(_baseaddr, (CDNS_CTRL_##_reg##_ADDR << 2),                  \
		   CDNS_CTRL_##_reg##_OFFSET, CDNS_REG_MASK(CDNS_CTRL_##_reg))

#endif /* #ifndef _CDNS_MC_EDAC_H */
