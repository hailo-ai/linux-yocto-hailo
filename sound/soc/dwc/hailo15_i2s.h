/* SPDX-License-Identifier: GPL-2.0 */
 /*
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef HAILO15_I2S_H
#define HAILO15_I2S_H

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include "hailo15_i2s_dma_shmem.h"

#define H15_BLK_POLL_INTERVAL_NSEC 1000000

/*!
 * @enum hailo15_sample_cmp
 * Enumerates the possible comparison options for samples.
 */
enum hailo15_sample_cmp {
	SAMPLE_CMP_UNINIT,	/*< Uninitialized comparison option. */
	SAMPLE_CMP_TO_ZERO, /*< Compare to zero. */
	SAMPLE_CMP_TO_PREV_SAMPLE, /*< Compare to the previous sample. */
};

/*!
 * @struct hailo15_pace_pattern_info
 * Represents information about a samples pace pattern for recording and playback.
 * - Recording: there is a need to remove samples indexes that follows the pattern rule represented by this struct.
 * - playback: there is a need to duplicate samples inexes that follows the pattern rule represented by this struct.
 */
struct hailo15_pace_pattern_info {
	u32 offset; /*< Starting sample idx */
	u32 pace_cnt; /*< Pace array size */
	u32 *pace; /*< Pointer to an array of #pace_cnt paces */
	u32 pace_sum; /*< Sum of all (#pace_cnt) pace array elements */
	u32 pattern_repetitions; /*< number of consecutive pattern repetitions required for recording synchronization */
	struct {
		u32 start; /*< Start sample index of the pace range */
		u32 end; /*< End sample index of the pace range */
	} *pace_range; /*<  Pace range description per pace element. */
	enum hailo15_sample_cmp sample_cmp; /*< @ref hailo15_sample_cmp */
};

/*!
 * @struct u16 sample ring.
 *         Used by hailo15 driver to store recorded samples until samples pace pattern (@ref hailo15_pace_pattern_info) is found.
 */
struct u16_sample_ring {
	u16 *buf; /*< pointer to starting ring address */
	int size; /*< Ring size */

	int head; /*< ring head idx */
	int tail; /*< ring tail idx */
	int count; /*< stored number of samples */
};

/*!
 * @brief auxilary macro definition operations for @ref u16_sample_ring
 */
#define U16_SAMPLE_RING_MASK(ring) (ring->size - 1)
#define U16_SAMPLE_RING_VAL(ring, i) (ring->buf[(i) & U16_SAMPLE_RING_MASK(ring)])

/*!
 * @brief Hailo15 private data for handling playback/recording through SCU.
 */
struct hailo15_scu_dw_i2s_dma {
    struct dwc_i2s_dma_shmem_map *shmem; /*< Pointer to shmem area. */
    struct dwc_i2s_dma_block_ring record_ring; /*< I2S DMA record block ring managment. */
    struct dwc_i2s_dma_block_ring playback_ring; /*< I2S DMA playback block ring managment. */
	u32 playback_overrun; /*< playback ovverrun count - times that no empty block is available to SCU */
	u32 record_underrun; /*< record ovverrun count - times that no full block is available from SCU */
};

/*!
 * @struct hailo15_priv_data
 * @brief Hailo15 private data.
 */
struct hailo15_priv_data {
    /* playback/Record pace pattern info */
	struct hailo15_pace_pattern_info rx_pace_pattern_info; /*< Record pace pattern info */
	struct hailo15_pace_pattern_info tx_pace_pattern_info; /*< playback pace pattern info */

    /* Left/Right rings which stores maximu U16_SAMPLE_RING_SIZE samples.
	 * It is used by the driver to sync on the expected pace pattern.
	 * Note, It is only used at the begining of recording until sync is accomplished.
	 */
	struct u16_sample_ring l_rx_sync_ring ; /*< Left channel recording ring buffer */
	struct u16_sample_ring r_rx_sync_ring ; /*< Right channel recording ring buffer */
	bool rx_pace_pattern_synced; /*< Pace pattern synchronized flag */
	bool tx_dup_flag;	/*< Duplicate previous sample from previous filled playback block */

    /* Recording/playback sample count, used for applying the pace pattern */
	u32 rx_sample_cnt;
	u32 tx_sample_cnt;

    /* I2S SCU DMA */
	struct hailo15_scu_dw_i2s_dma scu_dw_i2s_dma; /*< private data for handling playback/recording through SCU*/

    /* I2S SCU DMA block polling */
	struct hrtimer scu_dma_blk_poll_timer; /*< polling timer that handles block for playback/recording to anf from SCU. */
	bool playback_poll_state_flag; /*< playback poll state flag: Active(true) */
	bool record_poll_state_flag; /*< Record poll state flag: Active(true) */

	/* I2S statistics proc entry. Used only by "hailo15-designware-i2s" & "hailo15-designware-i2s-scu-dma" compatible drivers. */
	struct proc_dir_entry *proc_dir;
	/* I2S controller FIFO rx/tx overflow counters. Used only by "hailo15-designware-i2s" compatible drivers. */
	unsigned int hw_tx_overrun;
	unsigned int hw_rx_overrun;

	struct dw_i2s_dev *dev;
};

struct platform_device;
int hailo15_extra_probe(struct platform_device *pdev);
int hailo15_proc_entries_create(struct platform_device *pdev);
int hailo15_proc_entries_remove(struct platform_device *pdev);

#if IS_ENABLED(CONFIG_SND_DESIGNWARE_PCM)
struct dw_i2s_dev;
struct snd_pcm_runtime;
struct snd_soc_component;
struct snd_pcm_substream;
struct snd_pcm_hw_params;
extern const struct snd_pcm_hardware dw_pcm_hardware;
enum hrtimer_restart scu_dma_blk_poll_timer_cb(struct hrtimer *timer);
unsigned int dw_pcm_hailo15_scu_rx_blk(struct dw_i2s_dev *dev,
                                       struct snd_pcm_runtime *runtime,
                                       unsigned int rx_ptr,
                                       bool *period_elapsed);
unsigned int dw_pcm_hailo15_scu_tx_blk(struct dw_i2s_dev *dev,
                                       struct snd_pcm_runtime *runtime,
                                       unsigned int tx_ptr,
                                       bool *period_elapsed);
unsigned int dw_pcm_hailo15_rx_16(struct dw_i2s_dev *dev,
                                  struct snd_pcm_runtime *runtime,
                                  unsigned int rx_ptr,
                                  bool *period_elapsed);
unsigned int dw_pcm_hailo15_tx_16(struct dw_i2s_dev *dev,
                                  struct snd_pcm_runtime *runtime,
                                  unsigned int tx_ptr,
                                  bool *period_elapsed);
int dw_pcm_hailo15_open(struct snd_soc_component *component,
                        struct snd_pcm_substream *substream);
int dw_pcm_hailo15_scu_dma_open(struct snd_soc_component *component,
                                struct snd_pcm_substream *substream);
int dw_pcm_hailo15_scu_dma_close(struct snd_soc_component *component,
                                struct snd_pcm_substream *substream);
int dw_pcm_hailo15_hw_params(struct snd_soc_component *component,
                             struct snd_pcm_substream *substream,
                             struct snd_pcm_hw_params *hw_params);
int dw_pcm_hailo15_scu_dma_hw_params(struct snd_soc_component *component,
                                     struct snd_pcm_substream *substream,
                                     struct snd_pcm_hw_params *hw_params);
#endif

#ifdef CONFIG_SND_DESIGNWARE_HAILO15_STATS
void per_cpu_do_irq_interval_stats__add(int stream);
void per_cpu_do_irq_execution_stats__start(int stream);
void per_cpu_do_irq_execution_stats__end(int stream);
#endif

#endif /* HAILO15_I2S_H */
