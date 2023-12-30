// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 - 2024 Hailo Technologies Ltd. All rights reserved.
 */
#ifndef DRAM_DMA_IP_CONFIG_MACRO_H
#define DRAM_DMA_IP_CONFIG_MACRO_H


/*----------------------------------------------------------------------------------------------------*/
/*  CHANNEL_SRC_CFG0 : start_abort		*/
/*  Description: Start/Abort: When set to 1b, this bit launches the DMA transfer for this channel; appropriate
registers should have previously been set. If the transfer has not ended and this bit is set to
0, the transfer is stopped. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__SHIFT                                 (0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__WIDTH                                 (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__MASK                                  (0x00000001L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__RESET                                 (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__READ(reg_offset)                      \
			(((uint32_t)(reg_offset) & 0x00000001L) >> 0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__MODIFY(reg_offset, value)             \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | (((uint32_t)(value) << 0) & 0x00000001L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__SET(reg_offset)                       \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | ((uint32_t)(1) << 0))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__START_ABORT__CLR(reg_offset)                       \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | ((uint32_t)(0) << 0))

/*  CHANNEL_SRC_CFG0 : pause_resume		*/
/*  Description: Pause/Resume: When set to 1b, DMA transfer is paused. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__PAUSE_RESUME__SHIFT                                (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__PAUSE_RESUME__WIDTH                                (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__PAUSE_RESUME__MASK                                 (0x00000002L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__PAUSE_RESUME__RESET                                (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__PAUSE_RESUME__READ(reg_offset)                     \
			(((uint32_t)(reg_offset) & 0x00000002L) >> 1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__PAUSE_RESUME__MODIFY(reg_offset, value)            \
			(reg_offset) = (((reg_offset) & ~0x00000002L) | (((uint32_t)(value) << 1) & 0x00000002L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__PAUSE_RESUME__SET(reg_offset)                      \
			(reg_offset) = (((reg_offset) & ~0x00000002L) | ((uint32_t)(1) << 1))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__PAUSE_RESUME__CLR(reg_offset)                      \
			(reg_offset) = (((reg_offset) & ~0x00000002L) | ((uint32_t)(0) << 1))

/*  CHANNEL_SRC_CFG0 : abort_on_error		*/
/*  Description: Abort on Error: When set to 1b, DMA transfer on this channel is stopped if an error occurs. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__ABORT_ON_ERROR__SHIFT                              (2)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__ABORT_ON_ERROR__WIDTH                              (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__ABORT_ON_ERROR__MASK                               (0x00000004L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__ABORT_ON_ERROR__RESET                              (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__ABORT_ON_ERROR__READ(reg_offset)                   \
			(((uint32_t)(reg_offset) & 0x00000004L) >> 2)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__ABORT_ON_ERROR__MODIFY(reg_offset, value)          \
			(reg_offset) = (((reg_offset) & ~0x00000004L) | (((uint32_t)(value) << 2) & 0x00000004L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__ABORT_ON_ERROR__SET(reg_offset)                    \
			(reg_offset) = (((reg_offset) & ~0x00000004L) | ((uint32_t)(1) << 2))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__ABORT_ON_ERROR__CLR(reg_offset)                    \
			(reg_offset) = (((reg_offset) & ~0x00000004L) | ((uint32_t)(0) << 2))

/*  CHANNEL_SRC_CFG0 : reserved		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED__SHIFT                                    (3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED__WIDTH                                    (2)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED__MASK                                     (0x00000018L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED__RESET                                    (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED__READ(reg_offset)                         \
			(((uint32_t)(reg_offset) & 0x00000018L) >> 3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED__MODIFY(reg_offset, value)                \
			(reg_offset) = (((reg_offset) & ~0x00000018L) | (((uint32_t)(value) << 3) & 0x00000018L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED__SET(reg_offset)                          \
			(reg_offset) = (((reg_offset) & ~0x00000018L) | 0x00000018L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED__CLR(reg_offset)                          \
			(reg_offset) = (((reg_offset) & ~0x00000018L))

/*  CHANNEL_SRC_CFG0 : irq		*/
/*  Description: Bit [5]: DMA IRQ is required on error.
Bit [6]: DMA IRQ events should be reported on the AXI Domain 1 interface.
Bit [7]: DMA IRQ events should be reported on the AXI Domain 0 interface. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__IRQ__SHIFT                                         (5)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__IRQ__WIDTH                                         (3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__IRQ__MASK                                          (0x000000E0L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__IRQ__RESET                                         (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__IRQ__READ(reg_offset)                              \
			(((uint32_t)(reg_offset) & 0x000000E0L) >> 5)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__IRQ__MODIFY(reg_offset, value)                     \
			(reg_offset) = (((reg_offset) & ~0x000000E0L) | (((uint32_t)(value) << 5) & 0x000000E0L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__IRQ__SET(reg_offset)                               \
			(reg_offset) = (((reg_offset) & ~0x000000E0L) | 0x000000E0L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__IRQ__CLR(reg_offset)                               \
			(reg_offset) = (((reg_offset) & ~0x000000E0L))

/*  CHANNEL_SRC_CFG0 : reserved_1		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_1__SHIFT                                  (8)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_1__WIDTH                                  (3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_1__MASK                                   (0x00000700L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_1__RESET                                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_1__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x00000700L) >> 8)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_1__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x00000700L) | (((uint32_t)(value) << 8) & 0x00000700L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_1__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00000700L) | 0x00000700L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_1__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00000700L))

/*  CHANNEL_SRC_CFG0 : src_depth		*/
/*  Description: SRC_DEPTH defines the depth of the circular buffer allocated to the source descriptors.
A depth of 0 is equivalent to the maximum number of 2(G_DESCNUM_DEPTH_MAX)
descriptors.
For example, if G_DESCNUM_DEPTH_MAX is set to 8, than SRC_DEPTH can be configured
between 1 and 16.
If you are not using a circular buffer, this value is 0. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRC_DEPTH__SHIFT                                   (11)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRC_DEPTH__WIDTH                                   (4)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRC_DEPTH__MASK                                    (0x00007800L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRC_DEPTH__RESET                                   (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRC_DEPTH__READ(reg_offset)                        \
			(((uint32_t)(reg_offset) & 0x00007800L) >> 11)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRC_DEPTH__MODIFY(reg_offset, value)               \
			(reg_offset) = (((reg_offset) & ~0x00007800L) | (((uint32_t)(value) << 11) & 0x00007800L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRC_DEPTH__SET(reg_offset)                         \
			(reg_offset) = (((reg_offset) & ~0x00007800L) | 0x00007800L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRC_DEPTH__CLR(reg_offset)                         \
			(reg_offset) = (((reg_offset) & ~0x00007800L))

/*  CHANNEL_SRC_CFG0 : reserved_2		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_2__SHIFT                                  (15)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_2__WIDTH                                  (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_2__MASK                                   (0x00008000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_2__RESET                                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_2__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x00008000L) >> 15)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_2__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x00008000L) | (((uint32_t)(value) << 15) & 0x00008000L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_2__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00008000L) | ((uint32_t)(1) << 15))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__RESERVED_2__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00008000L) | ((uint32_t)(0) << 15))

/*  CHANNEL_SRC_CFG0 : srcdescnum_available		*/
/*  Description: SRCDESCNUM_AVAILABLE defines the number of source descriptors available.
This number is incremented as new descriptors become ready.
For example, if 4 descriptors are ready at the start of the DMA transfer, then
SRCDESCNUM_AVAILABLE is set to 4. If 2 more descriptors then become ready, the
number of available descriptors is 6 in total.
When the number of available descriptors exceeds the maximum number defined by
2(SRC_DEPTH), this counter loops back to 0. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRCDESCNUM_AVAILABLE__SHIFT                        (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRCDESCNUM_AVAILABLE__WIDTH                        (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRCDESCNUM_AVAILABLE__MASK                         (0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRCDESCNUM_AVAILABLE__RESET                        (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRCDESCNUM_AVAILABLE__READ(reg_offset)             \
			(((uint32_t)(reg_offset) & 0xFFFF0000L) >> 16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRCDESCNUM_AVAILABLE__MODIFY(reg_offset, value)    \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000U) | (((uint32_t)(value) << 16) & 0xFFFF0000U))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRCDESCNUM_AVAILABLE__SET(reg_offset)              \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000L) | 0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG0__SRCDESCNUM_AVAILABLE__CLR(reg_offset)              \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000L))

/*----------------------------------------------------------------------------------------------------*/
/*  CHANNEL_SRC_CFG1 : srcdescnum_processed		*/
/*  Description: SRCDESCNUM_PROCESSED defines the number of source descriptors that have been processed for this channel. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG1__SRCDESCNUM_PROCESSED__SHIFT                        (0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG1__SRCDESCNUM_PROCESSED__WIDTH                        (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG1__SRCDESCNUM_PROCESSED__MASK                         (0x0000FFFFL)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG1__SRCDESCNUM_PROCESSED__RESET                        (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG1__SRCDESCNUM_PROCESSED__READ(reg_offset)             \
			(((uint32_t)(reg_offset) & 0x0000FFFFL) >> 0)

/*  CHANNEL_SRC_CFG1 : srcdescnum_ongoing		*/
/*  Description: SRCDESCNUM_ONGOING defines the number of source descriptors currently being processed for this channel. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG1__SRCDESCNUM_ONGOING__SHIFT                          (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG1__SRCDESCNUM_ONGOING__WIDTH                          (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG1__SRCDESCNUM_ONGOING__MASK                           (0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG1__SRCDESCNUM_ONGOING__RESET                          (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG1__SRCDESCNUM_ONGOING__READ(reg_offset)               \
			(((uint32_t)(reg_offset) & 0xFFFF0000L) >> 16)

/*----------------------------------------------------------------------------------------------------*/
/*  CHANNEL_SRC_CFG2 : reserved		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED__SHIFT                                    (0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED__WIDTH                                    (3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED__MASK                                     (0x00000007L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED__RESET                                    (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED__READ(reg_offset)                         \
			(((uint32_t)(reg_offset) & 0x00000007L) >> 0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED__MODIFY(reg_offset, value)                \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | (((uint32_t)(value) << 0) & 0x00000007L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED__SET(reg_offset)                          \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | 0x00000007L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED__CLR(reg_offset)                          \
			(reg_offset) = (((reg_offset) & ~0x00000007L))

/*  CHANNEL_SRC_CFG2 : write_data_error		*/
/*  Description: This field is RW1C */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__WRITE_DATA_ERROR__SHIFT                            (3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__WRITE_DATA_ERROR__WIDTH                            (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__WRITE_DATA_ERROR__MASK                             (0x00000008L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__WRITE_DATA_ERROR__RESET                            (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__WRITE_DATA_ERROR__READ(reg_offset)                 \
			(((uint32_t)(reg_offset) & 0x00000008L) >> 3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__WRITE_DATA_ERROR__MODIFY(reg_offset, value)        \
			(reg_offset) = (((reg_offset) & ~0x00000008L) | (((uint32_t)(value) << 3) & 0x00000008L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__WRITE_DATA_ERROR__SET(reg_offset)                  \
			(reg_offset) = (((reg_offset) & ~0x00000008L) | ((uint32_t)(1) << 3))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__WRITE_DATA_ERROR__CLR(reg_offset)                  \
			(reg_offset) = (((reg_offset) & ~0x00000008L) | ((uint32_t)(0) << 3))

/*  CHANNEL_SRC_CFG2 : axi_write_slverr_or_decerr		*/
/*  Description: This field is RW1C */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_WRITE_SLVERR_OR_DECERR__SHIFT                  (4)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_WRITE_SLVERR_OR_DECERR__WIDTH                  (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_WRITE_SLVERR_OR_DECERR__MASK                   (0x00000010L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_WRITE_SLVERR_OR_DECERR__RESET                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_WRITE_SLVERR_OR_DECERR__READ(reg_offset)       \
			(((uint32_t)(reg_offset) & 0x00000010L) >> 4)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_WRITE_SLVERR_OR_DECERR__MODIFY(reg_offset, value) \
			(reg_offset) = (((reg_offset) & ~0x00000010L) | (((uint32_t)(value) << 4) & 0x00000010L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_WRITE_SLVERR_OR_DECERR__SET(reg_offset)        \
			(reg_offset) = (((reg_offset) & ~0x00000010L) | ((uint32_t)(1) << 4))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_WRITE_SLVERR_OR_DECERR__CLR(reg_offset)        \
			(reg_offset) = (((reg_offset) & ~0x00000010L) | ((uint32_t)(0) << 4))

/*  CHANNEL_SRC_CFG2 : reserved_1		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_1__SHIFT                                  (5)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_1__WIDTH                                  (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_1__MASK                                   (0x00000020L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_1__RESET                                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_1__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x00000020L) >> 5)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_1__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x00000020L) | (((uint32_t)(value) << 5) & 0x00000020L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_1__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00000020L) | ((uint32_t)(1) << 5))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_1__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00000020L) | ((uint32_t)(0) << 5))

/*  CHANNEL_SRC_CFG2 : axi_read_slverr		*/
/*  Description: This field is RW1C */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_SLVERR__SHIFT                             (6)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_SLVERR__WIDTH                             (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_SLVERR__MASK                              (0x00000040L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_SLVERR__RESET                             (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_SLVERR__READ(reg_offset)                  \
			(((uint32_t)(reg_offset) & 0x00000040L) >> 6)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_SLVERR__MODIFY(reg_offset, value)         \
			(reg_offset) = (((reg_offset) & ~0x00000040L) | (((uint32_t)(value) << 6) & 0x00000040L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_SLVERR__SET(reg_offset)                   \
			(reg_offset) = (((reg_offset) & ~0x00000040L) | ((uint32_t)(1) << 6))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_SLVERR__CLR(reg_offset)                   \
			(reg_offset) = (((reg_offset) & ~0x00000040L) | ((uint32_t)(0) << 6))

/*  CHANNEL_SRC_CFG2 : axi_read_decerr		*/
/*  Description: This field is RW1C */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_DECERR__SHIFT                             (7)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_DECERR__WIDTH                             (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_DECERR__MASK                              (0x00000080L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_DECERR__RESET                             (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_DECERR__READ(reg_offset)                  \
			(((uint32_t)(reg_offset) & 0x00000080L) >> 7)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_DECERR__MODIFY(reg_offset, value)         \
			(reg_offset) = (((reg_offset) & ~0x00000080L) | (((uint32_t)(value) << 7) & 0x00000080L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_DECERR__SET(reg_offset)                   \
			(reg_offset) = (((reg_offset) & ~0x00000080L) | ((uint32_t)(1) << 7))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__AXI_READ_DECERR__CLR(reg_offset)                   \
			(reg_offset) = (((reg_offset) & ~0x00000080L) | ((uint32_t)(0) << 7))

/*  CHANNEL_SRC_CFG2 : reserved_2		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_2__SHIFT                                  (8)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_2__WIDTH                                  (8)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_2__MASK                                   (0x0000FF00L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_2__RESET                                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_2__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x0000FF00L) >> 8)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_2__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x0000FF00L) | (((uint32_t)(value) << 8) & 0x0000FF00L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_2__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x0000FF00L) | 0x0000FF00L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__RESERVED_2__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x0000FF00L))

/*  CHANNEL_SRC_CFG2 : srcdesc_address_low		*/
/*  Description: SRCDESC_ADDRESS[31:16] defines the first descriptor address.
It must be aligned on a 64 KByte-boundary.
The 16 LSB bits are dropped from the 64-bit address. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__SRCDESC_ADDRESS_LOW__SHIFT                         (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__SRCDESC_ADDRESS_LOW__WIDTH                         (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__SRCDESC_ADDRESS_LOW__MASK                          (0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__SRCDESC_ADDRESS_LOW__RESET                         (0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__SRCDESC_ADDRESS_LOW__READ(reg_offset)              \
			(((uint32_t)(reg_offset) & 0xFFFF0000L) >> 16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__SRCDESC_ADDRESS_LOW__MODIFY(reg_offset, value)     \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000L) | (((uint32_t)(value) << 16) & 0xFFFF0000L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__SRCDESC_ADDRESS_LOW__SET(reg_offset)               \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000L) | 0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG2__SRCDESC_ADDRESS_LOW__CLR(reg_offset)               \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000L))

/*----------------------------------------------------------------------------------------------------*/
/*  CHANNEL_SRC_CFG3 : srcdesc_address_high		*/
/*  Description: SRCDESC_ADDRESS[63:32] */
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG3__SRCDESC_ADDRESS_HIGH__SHIFT                        (0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG3__SRCDESC_ADDRESS_HIGH__WIDTH                        (32)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG3__SRCDESC_ADDRESS_HIGH__MASK                         (0xFFFFFFFFL)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG3__SRCDESC_ADDRESS_HIGH__RESET                        (0xFFFFFFFFL)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG3__SRCDESC_ADDRESS_HIGH__READ(reg_offset)             \
			(((uint32_t)(reg_offset) & 0xFFFFFFFFL) >> 0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG3__SRCDESC_ADDRESS_HIGH__MODIFY(reg_offset, value)    \
			(reg_offset) = (((reg_offset) & ~0xFFFFFFFFU) | (((uint32_t)(value) << 0) & 0xFFFFFFFFU))
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG3__SRCDESC_ADDRESS_HIGH__SET(reg_offset)              \
			(reg_offset) = (((reg_offset) & ~0xFFFFFFFFL) | 0xFFFFFFFFL)
#define DRAM_DMA_IP_CONFIG__CHANNEL_SRC_CFG3__SRCDESC_ADDRESS_HIGH__CLR(reg_offset)              \
			(reg_offset) = (((reg_offset) & ~0xFFFFFFFFL))

/*----------------------------------------------------------------------------------------------------*/
/*  CHANNEL_DST_CFG0 : start_abort		*/
/*  Description: Start/Abort: When set to 1b, this bit launches the DMA transfer for this channel; appropriate
registers should have previously been set. If the transfer has not ended and this bit is set to
0, the transfer is stopped. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__START_ABORT__SHIFT                                 (0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__START_ABORT__WIDTH                                 (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__START_ABORT__MASK                                  (0x00000001L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__START_ABORT__RESET                                 (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__START_ABORT__READ(reg_offset)                      \
			(((uint32_t)(reg_offset) & 0x00000001L) >> 0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__START_ABORT__MODIFY(reg_offset, value)             \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | (((uint32_t)(value) << 0) & 0x00000001L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__START_ABORT__SET(reg_offset)                       \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | ((uint32_t)(1) << 0))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__START_ABORT__CLR(reg_offset)                       \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | ((uint32_t)(0) << 0))

/*  CHANNEL_DST_CFG0 : pause_resume		*/
/*  Description: Pause/Resume: When set to 1b, DMA transfer is paused. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__PAUSE_RESUME__SHIFT                                (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__PAUSE_RESUME__WIDTH                                (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__PAUSE_RESUME__MASK                                 (0x00000002L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__PAUSE_RESUME__RESET                                (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__PAUSE_RESUME__READ(reg_offset)                     \
			(((uint32_t)(reg_offset) & 0x00000002L) >> 1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__PAUSE_RESUME__MODIFY(reg_offset, value)            \
			(reg_offset) = (((reg_offset) & ~0x00000002L) | (((uint32_t)(value) << 1) & 0x00000002L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__PAUSE_RESUME__SET(reg_offset)                      \
			(reg_offset) = (((reg_offset) & ~0x00000002L) | ((uint32_t)(1) << 1))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__PAUSE_RESUME__CLR(reg_offset)                      \
			(reg_offset) = (((reg_offset) & ~0x00000002L) | ((uint32_t)(0) << 1))

/*  CHANNEL_DST_CFG0 : abort_on_error		*/
/*  Description: Abort on Error: When set to 1b, DMA transfer on this channel is stopped if an error occurs. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__ABORT_ON_ERROR__SHIFT                              (2)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__ABORT_ON_ERROR__WIDTH                              (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__ABORT_ON_ERROR__MASK                               (0x00000004L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__ABORT_ON_ERROR__RESET                              (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__ABORT_ON_ERROR__READ(reg_offset)                   \
			(((uint32_t)(reg_offset) & 0x00000004L) >> 2)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__ABORT_ON_ERROR__MODIFY(reg_offset, value)          \
			(reg_offset) = (((reg_offset) & ~0x00000004L) | (((uint32_t)(value) << 2) & 0x00000004L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__ABORT_ON_ERROR__SET(reg_offset)                    \
			(reg_offset) = (((reg_offset) & ~0x00000004L) | ((uint32_t)(1) << 2))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__ABORT_ON_ERROR__CLR(reg_offset)                    \
			(reg_offset) = (((reg_offset) & ~0x00000004L) | ((uint32_t)(0) << 2))

/*  CHANNEL_DST_CFG0 : reserved		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED__SHIFT                                    (3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED__WIDTH                                    (2)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED__MASK                                     (0x00000018L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED__RESET                                    (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED__READ(reg_offset)                         \
			(((uint32_t)(reg_offset) & 0x00000018L) >> 3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED__MODIFY(reg_offset, value)                \
			(reg_offset) = (((reg_offset) & ~0x00000018L) | (((uint32_t)(value) << 3) & 0x00000018L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED__SET(reg_offset)                          \
			(reg_offset) = (((reg_offset) & ~0x00000018L) | 0x00000018L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED__CLR(reg_offset)                          \
			(reg_offset) = (((reg_offset) & ~0x00000018L))

/*  CHANNEL_DST_CFG0 : irq		*/
/*  Description: Bit [5]: DMA IRQ is required on error.
Bit [6]: DMA IRQ events should be reported on the AXI Domain 1 interface.
Bit [7]: DMA IRQ events should be reported on the AXI Domain 0 interface. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__IRQ__SHIFT                                         (5)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__IRQ__WIDTH                                         (3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__IRQ__MASK                                          (0x000000E0L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__IRQ__RESET                                         (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__IRQ__READ(reg_offset)                              \
			(((uint32_t)(reg_offset) & 0x000000E0L) >> 5)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__IRQ__MODIFY(reg_offset, value)                     \
			(reg_offset) = (((reg_offset) & ~0x000000E0L) | (((uint32_t)(value) << 5) & 0x000000E0L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__IRQ__SET(reg_offset)                               \
			(reg_offset) = (((reg_offset) & ~0x000000E0L) | 0x000000E0L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__IRQ__CLR(reg_offset)                               \
			(reg_offset) = (((reg_offset) & ~0x000000E0L))

/*  CHANNEL_DST_CFG0 : reserved_1		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_1__SHIFT                                  (8)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_1__WIDTH                                  (3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_1__MASK                                   (0x00000700L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_1__RESET                                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_1__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x00000700L) >> 8)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_1__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x00000700L) | (((uint32_t)(value) << 8) & 0x00000700L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_1__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00000700L) | 0x00000700L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_1__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00000700L))

/*  CHANNEL_DST_CFG0 : dest_depth		*/
/*  Description: DEST_DEPTH defines the depth of the circular buffer allocated to the destination
descriptors.
reserved
A depth of 0 is equivalent to the maximum number of 2(G_DESCNUM_DEPTH_MAX)
descriptors.
For example, if G_DESCNUM_DEPTH_MAX is set to 8, than DEST_DEPTH can be
configured between 1 and 16.
If you are not using a circular buffer, this value is 0. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DEST_DEPTH__SHIFT                                  (11)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DEST_DEPTH__WIDTH                                  (4)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DEST_DEPTH__MASK                                   (0x00007800L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DEST_DEPTH__RESET                                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DEST_DEPTH__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x00007800L) >> 11)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DEST_DEPTH__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x00007800L) | (((uint32_t)(value) << 11) & 0x00007800L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DEST_DEPTH__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00007800L) | 0x00007800L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DEST_DEPTH__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00007800L))

/*  CHANNEL_DST_CFG0 : reserved_2		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_2__SHIFT                                  (15)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_2__WIDTH                                  (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_2__MASK                                   (0x00008000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_2__RESET                                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_2__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x00008000L) >> 15)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_2__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x00008000L) | (((uint32_t)(value) << 15) & 0x00008000L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_2__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00008000L) | ((uint32_t)(1) << 15))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__RESERVED_2__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00008000L) | ((uint32_t)(0) << 15))

/*  CHANNEL_DST_CFG0 : destdescnum_available		*/
/*  Description: DESTDESCNUM_AVAILABLE defines the number of destination descriptors available. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DESTDESCNUM_AVAILABLE__SHIFT                       (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DESTDESCNUM_AVAILABLE__WIDTH                       (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DESTDESCNUM_AVAILABLE__MASK                        (0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DESTDESCNUM_AVAILABLE__RESET                       (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DESTDESCNUM_AVAILABLE__READ(reg_offset)            \
			(((uint32_t)(reg_offset) & 0xFFFF0000L) >> 16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DESTDESCNUM_AVAILABLE__MODIFY(reg_offset, value)   \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000U) | (((uint32_t)(value) << 16) & 0xFFFF0000U))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DESTDESCNUM_AVAILABLE__SET(reg_offset)             \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000L) | 0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG0__DESTDESCNUM_AVAILABLE__CLR(reg_offset)             \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000L))

/*----------------------------------------------------------------------------------------------------*/
/*  CHANNEL_DST_CFG1 : destdescnum_processed		*/
/*  Description: DESTDESCNUM_PROCESSED defines the number of destination descriptors that have been processed for this channel. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG1__DESTDESCNUM_PROCESSED__SHIFT                       (0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG1__DESTDESCNUM_PROCESSED__WIDTH                       (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG1__DESTDESCNUM_PROCESSED__MASK                        (0x0000FFFFL)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG1__DESTDESCNUM_PROCESSED__RESET                       (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG1__DESTDESCNUM_PROCESSED__READ(reg_offset)            \
			(((uint32_t)(reg_offset) & 0x0000FFFFL) >> 0)

/*  CHANNEL_DST_CFG1 : destdescnum_ongoing		*/
/*  Description: DESTDESCNUM_ONGOING defines the number of destination descriptors currently being processed for this channel. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG1__DESTDESCNUM_ONGOING__SHIFT                         (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG1__DESTDESCNUM_ONGOING__WIDTH                         (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG1__DESTDESCNUM_ONGOING__MASK                          (0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG1__DESTDESCNUM_ONGOING__RESET                         (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG1__DESTDESCNUM_ONGOING__READ(reg_offset)              \
			(((uint32_t)(reg_offset) & 0xFFFF0000L) >> 16)

/*----------------------------------------------------------------------------------------------------*/
/*  CHANNEL_DST_CFG2 : reserved		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED__SHIFT                                    (0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED__WIDTH                                    (3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED__MASK                                     (0x00000007L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED__RESET                                    (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED__READ(reg_offset)                         \
			(((uint32_t)(reg_offset) & 0x00000007L) >> 0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED__MODIFY(reg_offset, value)                \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | (((uint32_t)(value) << 0) & 0x00000007L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED__SET(reg_offset)                          \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | 0x00000007L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED__CLR(reg_offset)                          \
			(reg_offset) = (((reg_offset) & ~0x00000007L))

/*  CHANNEL_DST_CFG2 : write_data_error		*/
/*  Description: This field is RW1C */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__WRITE_DATA_ERROR__SHIFT                            (3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__WRITE_DATA_ERROR__WIDTH                            (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__WRITE_DATA_ERROR__MASK                             (0x00000008L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__WRITE_DATA_ERROR__RESET                            (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__WRITE_DATA_ERROR__READ(reg_offset)                 \
			(((uint32_t)(reg_offset) & 0x00000008L) >> 3)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__WRITE_DATA_ERROR__MODIFY(reg_offset, value)        \
			(reg_offset) = (((reg_offset) & ~0x00000008L) | (((uint32_t)(value) << 3) & 0x00000008L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__WRITE_DATA_ERROR__SET(reg_offset)                  \
			(reg_offset) = (((reg_offset) & ~0x00000008L) | ((uint32_t)(1) << 3))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__WRITE_DATA_ERROR__CLR(reg_offset)                  \
			(reg_offset) = (((reg_offset) & ~0x00000008L) | ((uint32_t)(0) << 3))

/*  CHANNEL_DST_CFG2 : axi_write_slverr_or_decerr		*/
/*  Description: This field is RW1C */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_WRITE_SLVERR_OR_DECERR__SHIFT                  (4)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_WRITE_SLVERR_OR_DECERR__WIDTH                  (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_WRITE_SLVERR_OR_DECERR__MASK                   (0x00000010L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_WRITE_SLVERR_OR_DECERR__RESET                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_WRITE_SLVERR_OR_DECERR__READ(reg_offset)       \
			(((uint32_t)(reg_offset) & 0x00000010L) >> 4)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_WRITE_SLVERR_OR_DECERR__MODIFY(reg_offset, value) \
			(reg_offset) = (((reg_offset) & ~0x00000010L) | (((uint32_t)(value) << 4) & 0x00000010L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_WRITE_SLVERR_OR_DECERR__SET(reg_offset)        \
			(reg_offset) = (((reg_offset) & ~0x00000010L) | ((uint32_t)(1) << 4))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_WRITE_SLVERR_OR_DECERR__CLR(reg_offset)        \
			(reg_offset) = (((reg_offset) & ~0x00000010L) | ((uint32_t)(0) << 4))

/*  CHANNEL_DST_CFG2 : reserved_1		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_1__SHIFT                                  (5)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_1__WIDTH                                  (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_1__MASK                                   (0x00000020L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_1__RESET                                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_1__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x00000020L) >> 5)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_1__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x00000020L) | (((uint32_t)(value) << 5) & 0x00000020L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_1__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00000020L) | ((uint32_t)(1) << 5))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_1__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x00000020L) | ((uint32_t)(0) << 5))

/*  CHANNEL_DST_CFG2 : axi_read_slverr		*/
/*  Description: This field is RW1C */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_SLVERR__SHIFT                             (6)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_SLVERR__WIDTH                             (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_SLVERR__MASK                              (0x00000040L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_SLVERR__RESET                             (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_SLVERR__READ(reg_offset)                  \
			(((uint32_t)(reg_offset) & 0x00000040L) >> 6)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_SLVERR__MODIFY(reg_offset, value)         \
			(reg_offset) = (((reg_offset) & ~0x00000040L) | (((uint32_t)(value) << 6) & 0x00000040L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_SLVERR__SET(reg_offset)                   \
			(reg_offset) = (((reg_offset) & ~0x00000040L) | ((uint32_t)(1) << 6))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_SLVERR__CLR(reg_offset)                   \
			(reg_offset) = (((reg_offset) & ~0x00000040L) | ((uint32_t)(0) << 6))

/*  CHANNEL_DST_CFG2 : axi_read_decerr		*/
/*  Description: This field is RW1C */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_DECERR__SHIFT                             (7)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_DECERR__WIDTH                             (1)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_DECERR__MASK                              (0x00000080L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_DECERR__RESET                             (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_DECERR__READ(reg_offset)                  \
			(((uint32_t)(reg_offset) & 0x00000080L) >> 7)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_DECERR__MODIFY(reg_offset, value)         \
			(reg_offset) = (((reg_offset) & ~0x00000080L) | (((uint32_t)(value) << 7) & 0x00000080L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_DECERR__SET(reg_offset)                   \
			(reg_offset) = (((reg_offset) & ~0x00000080L) | ((uint32_t)(1) << 7))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__AXI_READ_DECERR__CLR(reg_offset)                   \
			(reg_offset) = (((reg_offset) & ~0x00000080L) | ((uint32_t)(0) << 7))

/*  CHANNEL_DST_CFG2 : reserved_2		*/
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_2__SHIFT                                  (8)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_2__WIDTH                                  (8)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_2__MASK                                   (0x0000FF00L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_2__RESET                                  (0x00000000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_2__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x0000FF00L) >> 8)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_2__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x0000FF00L) | (((uint32_t)(value) << 8) & 0x0000FF00L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_2__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x0000FF00L) | 0x0000FF00L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__RESERVED_2__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x0000FF00L))

/*  CHANNEL_DST_CFG2 : destdesc_address_low		*/
/*  Description: DESTDESC_ADDRESS[31:16] defines the first descriptor address.
It must be aligned on a 64 KByte-boundary.
The 16 LSB bits are dropped from the 64-bit address. */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__DESTDESC_ADDRESS_LOW__SHIFT                        (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__DESTDESC_ADDRESS_LOW__WIDTH                        (16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__DESTDESC_ADDRESS_LOW__MASK                         (0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__DESTDESC_ADDRESS_LOW__RESET                        (0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__DESTDESC_ADDRESS_LOW__READ(reg_offset)             \
			(((uint32_t)(reg_offset) & 0xFFFF0000L) >> 16)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__DESTDESC_ADDRESS_LOW__MODIFY(reg_offset, value)    \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000L) | (((uint32_t)(value) << 16) & 0xFFFF0000L))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__DESTDESC_ADDRESS_LOW__SET(reg_offset)              \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000L) | 0xFFFF0000L)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG2__DESTDESC_ADDRESS_LOW__CLR(reg_offset)              \
			(reg_offset) = (((reg_offset) & ~0xFFFF0000L))

/*----------------------------------------------------------------------------------------------------*/
/*  CHANNEL_DST_CFG3 : destdesc_address_high		*/
/*  Description: DESTDESC_ADDRESS[63:32] */
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG3__DESTDESC_ADDRESS_HIGH__SHIFT                       (0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG3__DESTDESC_ADDRESS_HIGH__WIDTH                       (32)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG3__DESTDESC_ADDRESS_HIGH__MASK                        (0xFFFFFFFFL)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG3__DESTDESC_ADDRESS_HIGH__RESET                       (0xFFFFFFFFL)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG3__DESTDESC_ADDRESS_HIGH__READ(reg_offset)            \
			(((uint32_t)(reg_offset) & 0xFFFFFFFFL) >> 0)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG3__DESTDESC_ADDRESS_HIGH__MODIFY(reg_offset, value)   \
			(reg_offset) = (((reg_offset) & ~0xFFFFFFFFU) | (((uint32_t)(value) << 0) & 0xFFFFFFFFU))
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG3__DESTDESC_ADDRESS_HIGH__SET(reg_offset)             \
			(reg_offset) = (((reg_offset) & ~0xFFFFFFFFL) | 0xFFFFFFFFL)
#define DRAM_DMA_IP_CONFIG__CHANNEL_DST_CFG3__DESTDESC_ADDRESS_HIGH__CLR(reg_offset)             \
			(reg_offset) = (((reg_offset) & ~0xFFFFFFFFL))

/*----------------------------------------------------------------------------------------------------*/
#endif /* DRAM_DMA_IP_CONFIG_MACRO_H */