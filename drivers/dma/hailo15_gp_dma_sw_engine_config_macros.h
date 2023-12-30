// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019 - 2024 Hailo Technologies Ltd. All rights reserved.
 */
#ifndef DRAM_DMA_SW_ENGINE_CONFIG_MACRO_H
#define DRAM_DMA_SW_ENGINE_CONFIG_MACRO_H


/*----------------------------------------------------------------------------------------------------*/
/*  ENGINE_AP_INTR_MASK : val		*/
/*  Description: INT register bit per channel indicating sharedbus interrupt */
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_MASK__VAL__SHIFT                                 (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_MASK__VAL__WIDTH                                 (16)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_MASK__VAL__MASK                                  (0x0000FFFFL)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_MASK__VAL__RESET                                 (0x00000000L)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_MASK__VAL__READ(reg_offset)                      \
			(((uint32_t)(reg_offset) & 0x0000FFFFL) >> 0)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_MASK__VAL__MODIFY(reg_offset, value)             \
			(reg_offset) = (((reg_offset) & ~0x0000FFFFU) | (((uint32_t)(value) << 0) & 0x0000FFFFU))
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_MASK__VAL__SET(reg_offset)                       \
			(reg_offset) = (((reg_offset) & ~0x0000FFFFL) | 0x0000FFFFL)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_MASK__VAL__CLR(reg_offset)                       \
			(reg_offset) = (((reg_offset) & ~0x0000FFFFL))

/*----------------------------------------------------------------------------------------------------*/
/*  ENGINE_AP_INTR_STATUS : val		*/
/*  Description: INT register bit per channel indicating sharedbus interrupt */
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_STATUS__VAL__SHIFT                               (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_STATUS__VAL__WIDTH                               (16)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_STATUS__VAL__MASK                                (0x0000FFFFL)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_STATUS__VAL__RESET                               (0x00000000L)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_STATUS__VAL__READ(reg_offset)                    \
			(((uint32_t)(reg_offset) & 0x0000FFFFU) >> 0)

/*----------------------------------------------------------------------------------------------------*/
/*  ENGINE_AP_INTR_W1C : val		*/
/*  Description: INT register bit per channel indicating sharedbus interrupt */
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1C__VAL__SHIFT                                  (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1C__VAL__WIDTH                                  (16)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1C__VAL__MASK                                   (0x0000FFFFL)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1C__VAL__RESET                                  (0x00000000L)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1C__VAL__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x0000FFFFL) >> 0)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1C__VAL__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x0000FFFFU) | (((uint32_t)(value) << 0) & 0x0000FFFFU))
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1C__VAL__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x0000FFFFL) | 0x0000FFFFL)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1C__VAL__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x0000FFFFL))

/*----------------------------------------------------------------------------------------------------*/
/*  ENGINE_AP_INTR_W1S : val		*/
/*  Description: INT register bit per channel indicating sharedbus interrupt */
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1S__VAL__SHIFT                                  (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1S__VAL__WIDTH                                  (16)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1S__VAL__MASK                                   (0x0000FFFFL)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1S__VAL__RESET                                  (0x00000000L)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1S__VAL__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x0000FFFFL) >> 0)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1S__VAL__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x0000FFFFL) | (((uint32_t)(value) << 0) & 0x0000FFFFL))
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1S__VAL__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x0000FFFFL) | 0x0000FFFFL)
#define DRAM_DMA_SW_ENGINE_CONFIG__ENGINE_AP_INTR_W1S__VAL__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x0000FFFFL))

/*----------------------------------------------------------------------------------------------------*/
/*  VDMASOFTRESET : val		*/
/*  Description: Apply soft reset to vDMA. Must be cleared in order to release vDMA from soft reset. */
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMASOFTRESET__VAL__SHIFT                                       (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMASOFTRESET__VAL__WIDTH                                       (1)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMASOFTRESET__VAL__MASK                                        (0x00000001L)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMASOFTRESET__VAL__RESET                                       (0x00000000L)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMASOFTRESET__VAL__READ(reg_offset)                            \
			(((uint32_t)(reg_offset) & 0x00000001L) >> 0)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMASOFTRESET__VAL__MODIFY(reg_offset, value)                   \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | (((uint32_t)(value) << 0) & 0x00000001L))
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMASOFTRESET__VAL__SET(reg_offset)                             \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | ((uint32_t)(1) << 0))
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMASOFTRESET__VAL__CLR(reg_offset)                             \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | ((uint32_t)(0) << 0))

/*----------------------------------------------------------------------------------------------------*/
/*  VDMA_SHAREDBUS : ap_mask		*/
/*  Description: Bit mask on vDMA Sharedbus interrupt source for AP */
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMA_SHAREDBUS__AP_MASK__SHIFT                                  (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMA_SHAREDBUS__AP_MASK__WIDTH                                  (4)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMA_SHAREDBUS__AP_MASK__MASK                                   (0x0000000FL)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMA_SHAREDBUS__AP_MASK__RESET                                  (0x00000002L)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMA_SHAREDBUS__AP_MASK__READ(reg_offset)                       \
			(((uint32_t)(reg_offset) & 0x0000000FL) >> 0)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMA_SHAREDBUS__AP_MASK__MODIFY(reg_offset, value)              \
			(reg_offset) = (((reg_offset) & ~0x0000000FL) | (((uint32_t)(value) << 0) & 0x0000000FL))
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMA_SHAREDBUS__AP_MASK__SET(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x0000000FL) | 0x0000000FL)
#define DRAM_DMA_SW_ENGINE_CONFIG__VDMA_SHAREDBUS__AP_MASK__CLR(reg_offset)                        \
			(reg_offset) = (((reg_offset) & ~0x0000000FL))

/*----------------------------------------------------------------------------------------------------*/
/*  READTOQOSVALUE : val		*/
/*  Description: The QOS toward DDR-AXI master for low priority read. */
#define DRAM_DMA_SW_ENGINE_CONFIG__READTOQOSVALUE__VAL__SHIFT                                      (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__READTOQOSVALUE__VAL__WIDTH                                      (3)
#define DRAM_DMA_SW_ENGINE_CONFIG__READTOQOSVALUE__VAL__MASK                                       (0x00000007L)
#define DRAM_DMA_SW_ENGINE_CONFIG__READTOQOSVALUE__VAL__RESET                                      (0x00000000L)
#define DRAM_DMA_SW_ENGINE_CONFIG__READTOQOSVALUE__VAL__READ(reg_offset)                           \
			(((uint32_t)(reg_offset) & 0x00000007L) >> 0)
#define DRAM_DMA_SW_ENGINE_CONFIG__READTOQOSVALUE__VAL__MODIFY(reg_offset, value)                  \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | (((uint32_t)(value) << 0) & 0x00000007L))
#define DRAM_DMA_SW_ENGINE_CONFIG__READTOQOSVALUE__VAL__SET(reg_offset)                            \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | 0x00000007L)
#define DRAM_DMA_SW_ENGINE_CONFIG__READTOQOSVALUE__VAL__CLR(reg_offset)                            \
			(reg_offset) = (((reg_offset) & ~0x00000007L))

/*----------------------------------------------------------------------------------------------------*/
/*  WRITETOQOSVALUE : val		*/
/*  Description: The QOS toward DDR-AXI master for low priority write. */
#define DRAM_DMA_SW_ENGINE_CONFIG__WRITETOQOSVALUE__VAL__SHIFT                                     (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__WRITETOQOSVALUE__VAL__WIDTH                                     (3)
#define DRAM_DMA_SW_ENGINE_CONFIG__WRITETOQOSVALUE__VAL__MASK                                      (0x00000007L)
#define DRAM_DMA_SW_ENGINE_CONFIG__WRITETOQOSVALUE__VAL__RESET                                     (0x00000000L)
#define DRAM_DMA_SW_ENGINE_CONFIG__WRITETOQOSVALUE__VAL__READ(reg_offset)                          \
			(((uint32_t)(reg_offset) & 0x00000007L) >> 0)
#define DRAM_DMA_SW_ENGINE_CONFIG__WRITETOQOSVALUE__VAL__MODIFY(reg_offset, value)                 \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | (((uint32_t)(value) << 0) & 0x00000007L))
#define DRAM_DMA_SW_ENGINE_CONFIG__WRITETOQOSVALUE__VAL__SET(reg_offset)                           \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | 0x00000007L)
#define DRAM_DMA_SW_ENGINE_CONFIG__WRITETOQOSVALUE__VAL__CLR(reg_offset)                           \
			(reg_offset) = (((reg_offset) & ~0x00000007L))

/*----------------------------------------------------------------------------------------------------*/
/*  DESCREADQOSVALUE : val		*/
/*  Description: The QOS toward DDR-desc-AXI master for read. */
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCREADQOSVALUE__VAL__SHIFT                                    (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCREADQOSVALUE__VAL__WIDTH                                    (3)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCREADQOSVALUE__VAL__MASK                                     (0x00000007L)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCREADQOSVALUE__VAL__RESET                                    (0x00000002L)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCREADQOSVALUE__VAL__READ(reg_offset)                         \
			(((uint32_t)(reg_offset) & 0x00000007L) >> 0)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCREADQOSVALUE__VAL__MODIFY(reg_offset, value)                \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | (((uint32_t)(value) << 0) & 0x00000007L))
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCREADQOSVALUE__VAL__SET(reg_offset)                          \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | 0x00000007L)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCREADQOSVALUE__VAL__CLR(reg_offset)                          \
			(reg_offset) = (((reg_offset) & ~0x00000007L))

/*----------------------------------------------------------------------------------------------------*/
/*  DESCWRITEQOSVALUE : val		*/
/*  Description: The QOS toward DDR-desc-AXI master for write. */
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCWRITEQOSVALUE__VAL__SHIFT                                   (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCWRITEQOSVALUE__VAL__WIDTH                                   (3)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCWRITEQOSVALUE__VAL__MASK                                    (0x00000007L)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCWRITEQOSVALUE__VAL__RESET                                   (0x00000002L)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCWRITEQOSVALUE__VAL__READ(reg_offset)                        \
			(((uint32_t)(reg_offset) & 0x00000007L) >> 0)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCWRITEQOSVALUE__VAL__MODIFY(reg_offset, value)               \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | (((uint32_t)(value) << 0) & 0x00000007L))
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCWRITEQOSVALUE__VAL__SET(reg_offset)                         \
			(reg_offset) = (((reg_offset) & ~0x00000007L) | 0x00000007L)
#define DRAM_DMA_SW_ENGINE_CONFIG__DESCWRITEQOSVALUE__VAL__CLR(reg_offset)                         \
			(reg_offset) = (((reg_offset) & ~0x00000007L))

/*----------------------------------------------------------------------------------------------------*/
/*  AUTO_ADDRESS_ERR_CB_INDICATION : enable		*/
/*  Description: default is 1, meaning the address error is enabled, to hide the address error indication, set to 0 */
#define DRAM_DMA_SW_ENGINE_CONFIG__AUTO_ADDRESS_ERR_CB_INDICATION__ENABLE__SHIFT                   (0)
#define DRAM_DMA_SW_ENGINE_CONFIG__AUTO_ADDRESS_ERR_CB_INDICATION__ENABLE__WIDTH                   (1)
#define DRAM_DMA_SW_ENGINE_CONFIG__AUTO_ADDRESS_ERR_CB_INDICATION__ENABLE__MASK                    (0x00000001L)
#define DRAM_DMA_SW_ENGINE_CONFIG__AUTO_ADDRESS_ERR_CB_INDICATION__ENABLE__RESET                   (0x00000001L)
#define DRAM_DMA_SW_ENGINE_CONFIG__AUTO_ADDRESS_ERR_CB_INDICATION__ENABLE__READ(reg_offset)        \
			(((uint32_t)(reg_offset) & 0x00000001L) >> 0)
#define DRAM_DMA_SW_ENGINE_CONFIG__AUTO_ADDRESS_ERR_CB_INDICATION__ENABLE__MODIFY(reg_offset, value) \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | (((uint32_t)(value) << 0) & 0x00000001L))
#define DRAM_DMA_SW_ENGINE_CONFIG__AUTO_ADDRESS_ERR_CB_INDICATION__ENABLE__SET(reg_offset)         \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | ((uint32_t)(1) << 0))
#define DRAM_DMA_SW_ENGINE_CONFIG__AUTO_ADDRESS_ERR_CB_INDICATION__ENABLE__CLR(reg_offset)         \
			(reg_offset) = (((reg_offset) & ~0x00000001L) | ((uint32_t)(0) << 0))


#endif /* DRAM_DMA_SW_ENGINE_CONFIG_MACRO_H */
