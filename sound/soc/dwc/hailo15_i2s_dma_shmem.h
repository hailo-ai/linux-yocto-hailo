/* SPDX-License-Identifier: GPL-2.0 */
 /*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef HAILO15_I2S_DMA_SHMEM_H
#define HAILO15_I2S_DMA_SHMEM_H

#define BLOCK_SAMPLE_BUF_SIZE (128)
#define DWC_I2S_DMA_BLK_RING_SIZE (3)

/*!
 * @brief I2S DMA block descriptor state.
 */
enum dwc_i2s_dma_block_state {
    DWC_I2S_DMA_BLK_STATE_EMPTY, /*!< block is empty. */
    DWC_I2S_DMA_BLK_STATE_FULL, /*!< block is full. */
};

/*!
 * @brief I2S DMA block.
 *
 * Two dimension sample array: (L-ch-sm #1, R-ch-sm #1), (L-ch-sm #2, R-ch-sm #2), ...
 */
struct dwc_i2s_dma_block {
    uint16_t samples[BLOCK_SAMPLE_BUF_SIZE][2];
} __attribute__((aligned(8)));

/*!
 * @brief I2S DMA block descriptor.
 */
struct dwc_i2s_dma_block_desc {
    enum dwc_i2s_dma_block_state state; /*!< 0: block empty, 1: block full. */
} __attribute__((aligned(8)));

/*!
 * @brief I2S DMA playback stats.
 *
 * @note Updated by SCU only.
 */
struct dwc_i2s_dma_playback_stats {
    uint32_t sw_underrun; /*!< number of times where no full block is available from A53. */
    uint32_t hw_overrun; /*!< number of times I2S controller raise Tx FIFO overrun. */
} __attribute__((aligned(8)));

/*!
 * @brief I2S DMA record stats.
 *
 * @note Updated by SCU only.
 */
struct dwc_i2s_dma_record_stats {
    uint32_t sw_overrun; /*!< number of times where no empty block is available to A53. */
    uint32_t hw_overrun; /*!< number of times I2S controller raise Rx FIFO underrun. */
} __attribute__((aligned(8)));

/*!
 * @brief I2S IRQ interval/execution stats.
 */
struct scu_i2s_irq_stat {
    uint64_t start;
    uint64_t sum;
    uint64_t max;
    uint64_t min;
    uint64_t cnt;
};

struct irq_stats {
    struct scu_i2s_irq_stat interval;
    struct scu_i2s_irq_stat execution;
} __attribute__((aligned(8)));

/*!
 * @brief I2S DMA shmem map description.
 */
struct dwc_i2s_dma_shmem_map {
    struct {
        struct dwc_i2s_dma_block block[DWC_I2S_DMA_BLK_RING_SIZE]; /*!< record block array. */
        struct dwc_i2s_dma_record_stats stats; /*!< record stats. */
        struct dwc_i2s_dma_block_desc desc[DWC_I2S_DMA_BLK_RING_SIZE]; /*!< record block description array. */
        uint32_t reset;
    } record;
    struct {
        struct dwc_i2s_dma_block block[DWC_I2S_DMA_BLK_RING_SIZE]; /*!< playback block array. */
        struct dwc_i2s_dma_playback_stats stats; /*!< playback stats. */
        struct dwc_i2s_dma_block_desc desc[DWC_I2S_DMA_BLK_RING_SIZE]; /*!< playback block description array. */
        uint32_t reset;
    } playback;
    struct irq_stats irq_rx_stats;
    struct irq_stats irq_tx_stats;
};

/*!
 * @brief I2S DMA block ring managment struct.
 */
struct dwc_i2s_dma_block_ring {
    struct dwc_i2s_dma_block *block; /*!< pointer to starting shmem block array. */
    struct dwc_i2s_dma_block_desc *desc; /*!< pointer to starting shmem block descriptor array. */
    int32_t idx; /*!< current used block/desc index. */
    int32_t sample_idx; /*!< next sample index to handle. */
};

#define DWC_I2S_DMA_BLK_RING__CURR_DESC_PTR(block_ring) (&block_ring.desc[block_ring.idx])
#define DWC_I2S_DMA_BLK_RING__CURR_IDX_INC(block_ring) \
    (block_ring.idx = ((block_ring.idx + 1) % DWC_I2S_DMA_BLK_RING_SIZE))
#define DWC_I2S_DMA_BLK_RING__CURR_IDX_RESET(block_ring) (block_ring.idx = 0)
#define DWC_I2S_DMA_BLK_RING__CURR_SAMPLES_BUF(block_ring) (block_ring.block[block_ring.idx].samples)

#define DWC_I2S_DMA_BLK_DESC__STATE_GET(block_desc_ptr) (block_desc_ptr->state)
#define DWC_I2S_DMA_BLK_DESC__STATE_SET(block_desc_ptr, _state) (block_desc_ptr->state = _state)

#endif /* HAILO15_I2S_DMA_SHMEM_H */
