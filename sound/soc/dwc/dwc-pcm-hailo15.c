/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ALSA SoC Synopsys PIO PCM for Hailo15 I2S driver
 *
 * sound/soc/dwc/dwc-pcm-hailo15.c
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/io.h>
#include <linux/rcupdate.h>
#include <linux/math.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include "local.h"

#define BUFFER_BYTES_MAX	(3 * 2 * 8 * PERIOD_BYTES_MIN)
#define PERIOD_BYTES_MIN	4096
#define PERIODS_MIN		2

#ifdef DWC_PCM_HAILO15_DEBUG

static void hexdump_ring(struct u16_sample_ring *ring)
{
	u32 i;
	char ring_str[128];
	int rc = 0;

	printk("buf virt[%p] phys[%llu]: head[%d] tail[%d] count[%d]:\n",
	       ring->buf,
		   virt_to_phys(ring->buf),
		   ring->head,
		   ring->tail,
	       ring->count);
	for (i = 0; i < ring->size; i++) {
		if ((i % 4) == 0) {
			rc += sprintf(ring_str + rc, "\t%08d: ", i);
		}
		rc += sprintf(ring_str + rc, "%04X ", U16_SAMPLE_RING_VAL(ring, i));
		if ((i + 1) % 4 == 0) {
			printk("%s\n", ring_str);
			rc = 0;
		}
	}
}

/*! @brief Calculate the RMS of last @ref count samples.
 *
 *  @param [in] ring: pointer to sample ring
 *  @param [in] count: numer of samples
 *
 *  @return u64: The average power of the samples.
 */
u64 calc_last_samples_rms(struct u16_sample_ring *ring, u32 count)
{
	u32 i, ring_sample_idx = ring->head - 1;
	s64 sum = 0;
	s16 val;

	for (i = 0; i < count; i++) {
		val = (s16)U16_SAMPLE_RING_VAL(ring, ring_sample_idx - i);
		sum += val * val;
	}

	return int_sqrt64(sum/count);
}

#endif

/*! @brief check if sample index equal to one of the pace starting paces offset.
 *
 *  @param [in] sample_idx: sample index
 *  @param [in] pace_info: reference pace pattern to be checkef for @ref sample_idx
 *
 *  @return bool: true (sample index belongs to a specific pace pattern), otherwise false.
 */
static bool is_sample_idx_start_of_pace(u32 sample_idx, struct hailo15_pace_pattern_info *pattern)
{
	u32 win_idx, i;

	if (sample_idx <= pattern->offset) {
		return false;
	}

	win_idx = ((sample_idx - pattern->offset) % pattern->pace_sum);
	for (i = 0; i < pattern->pace_cnt; i++) {
		if (win_idx == pattern->pace_range[i].start) {
			return true;
		}
	}
	return false;
}

static bool sample_drop_check(u32 sample_idx, struct hailo15_pace_pattern_info *pattern)
{
	return is_sample_idx_start_of_pace(sample_idx, pattern);
}

static bool sample_dup_check(u32 sample_idx, struct hailo15_pace_pattern_info *pattern)
{
	return is_sample_idx_start_of_pace(sample_idx, pattern);
}


/*! @brief Auxilary function which used to queue record samples until samples pace pattern sync is achieved.
 *
 *  @param [in] ring: sample ring pointer
 *  @param [in] val: u16 sample value
 *
 *  @return none
 */
static void sample_enqueue_u16(struct u16_sample_ring *ring, u16 val)
{
	ring->buf[ring->head] = val;
	ring->head = ((ring->head + 1) & U16_SAMPLE_RING_MASK(ring));
	ring->count++;
	if (ring->count == ring->size + 1) {
		ring->tail = ((ring->tail + 1) & U16_SAMPLE_RING_MASK(ring));
		ring->count = ring->size;
	}
}

/*! @brief Check if sample value matched to a specific pace pattern.
 *  @note Auxilary function used by @ref check_pattern_sync.
 *
 *  @param [in] pattern: pointer to a samples pace pattern.
 *  @param [in] ring: pointer to samples ring
 *  @param [in] sample_idx: current global sample index from controller fifo.
 *
 *  @return true (match), otherwise false.
 */
static bool is_sample_following_pattern(struct hailo15_pace_pattern_info *pattern,
                                        struct u16_sample_ring *ring,
										int sample_idx)
{
	u16 sample_val = (pattern->sample_cmp == SAMPLE_CMP_TO_PREV_SAMPLE) ? U16_SAMPLE_RING_VAL(ring, (sample_idx - 1)) : 0;
	return (U16_SAMPLE_RING_VAL(ring, sample_idx) == sample_val);
}

/*! @brief Check if samples pace pattern exist.
 *         The pattern should repeat itself #repetitions times as required in
 *         hailo15_pace_pattern_info::pattern_repetitions.
 *
 *  @param [in] pattern: pointer to a samples pace pattern.
 *  @param [in] ring: pointer to samples ring
 *  @param [in] sample_idx: current global sample index from controller fifo.
 *
 *  @return 0 (sync found), otherwise false.
 */
static int check_pattern_sync(struct hailo15_pace_pattern_info *pattern,
                              struct u16_sample_ring *ring,
							  int sample_idx)
{
	int i, pr, ring_sample_idx = ring->head - 1;

	if (ring->count < pattern->pace_sum * pattern->pattern_repetitions) {
		return -EAGAIN;
	}

	/* Check current sample */
	if (!is_sample_following_pattern(pattern, ring, ring_sample_idx)) {
		return -EAGAIN;
	}

	/* Check all pattern start offsets samples */
	for (pr = 0; pr < pattern->pattern_repetitions; pr++) {
		for (i = pattern->pace_cnt - 1; i >= 0; i--) {
			ring_sample_idx -= pattern->pace[i];
			if (!is_sample_following_pattern(pattern, ring, ring_sample_idx)) {
				return -EAGAIN;
			}
		}
	}

#ifdef DWC_PCM_HAILO15_DEBUG
	printk("Single pattern sync at sample_idx[%d]: RMS of last %d samples: %lld\n",
		sample_idx,
		pattern->pace_sum * pattern->pattern_repetitions,
		calc_last_samples_rms(ring, pattern->pace_sum * pattern->pattern_repetitions));
	ring_sample_idx = ring->head - 1;
	printk("ring_sample[%d]: val[%x] prev-val[%x]\n",
		   ring_sample_idx,
	       U16_SAMPLE_RING_VAL(ring, ring_sample_idx),
	       U16_SAMPLE_RING_VAL(ring, ring_sample_idx - 1));
	for (pr = 0; pr < pattern->pattern_repetitions; pr++) {
		for (i = pattern->pace_cnt - 1; i >= 0; i--) {
			ring_sample_idx -= pattern->pace[i];
			printk("pattern-repetition[%d] pace[%d] ring_sample[%d]: val[%x] prev-val[%x]\n",
			       pr, i, ring_sample_idx,
			       U16_SAMPLE_RING_VAL(ring, ring_sample_idx),
			       U16_SAMPLE_RING_VAL(ring, ring_sample_idx - 1));
		}
	}
	hexdump_ring(ring);
#endif

	pattern->offset = sample_idx;
	return 0;
}

/*!
 * @brief Hailo15 PCM playback function that duplicates samples according to hailo15 playback samples pace pattern.
 */
unsigned int dw_pcm_hailo15_tx_16(struct dw_i2s_dev *dev,
                                  struct snd_pcm_runtime *runtime,
                                  unsigned int tx_ptr,
                                  bool *period_elapsed)
{
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;
	const u16(*p)[2] = (void *)runtime->dma_area;
	unsigned int period_pos = tx_ptr % runtime->period_size;
	int i;

	for (i = 0; i < dev->fifo_th; i++) {
		if (data->tx_dup_flag) {
			/* duplicate sample p[tx_ptr][] */
			iowrite32(p[tx_ptr][0], dev->i2s_base + LRBR_LTHR(0));
			iowrite32(p[tx_ptr][1], dev->i2s_base + RRBR_RTHR(0));
			data->tx_dup_flag = false;
			/* Update indexes */
			data->tx_sample_cnt++;
			period_pos++;
			if (++tx_ptr >= runtime->buffer_size)
				tx_ptr = 0;
			continue;
		}

		iowrite32(p[tx_ptr][0], dev->i2s_base + LRBR_LTHR(0));
		iowrite32(p[tx_ptr][1], dev->i2s_base + RRBR_RTHR(0));
		data->tx_dup_flag = sample_dup_check(data->tx_sample_cnt, &data->tx_pace_pattern_info);
		if (unlikely(data->tx_dup_flag)) {
			/* duplicate p[tx_ptr][] sample, by not incrementing tx_ptr and incrementing only i */
			continue;
		}

		/* Update indexes */
		data->tx_sample_cnt++;
		period_pos++;
		if (++tx_ptr >= runtime->buffer_size)
			tx_ptr = 0;
	}
	*period_elapsed = period_pos >= runtime->period_size;
	return tx_ptr;
}

/*!
 * @brief Hailo15 PCM record function that removes samples according to hailo15 playback samples pace pattern.
 */
unsigned int dw_pcm_hailo15_rx_16(struct dw_i2s_dev *dev,
                                  struct snd_pcm_runtime *runtime,
                                  unsigned int rx_ptr,
                                  bool *period_elapsed)
{
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;
	u16(*p)[2] = (void *)runtime->dma_area;
	unsigned int period_pos = rx_ptr % runtime->period_size;
	int i;
	bool drop_sample_flag = false;

	for (i = 0; i < dev->fifo_th; i++, data->rx_sample_cnt++) {
		if (data->rx_pace_pattern_synced == true) {
			drop_sample_flag = sample_drop_check(data->rx_sample_cnt, &data->rx_pace_pattern_info);
		}
		p[rx_ptr][0] = ioread32(dev->i2s_base + LRBR_LTHR(0));
		p[rx_ptr][1] = ioread32(dev->i2s_base + RRBR_RTHR(0));
		if (unlikely(drop_sample_flag)) {
			/* ignore p[rx_ptr][] by not incrementing rx_ptr and inrementing only i */
			continue;
		}

		if (unlikely(data->rx_pace_pattern_synced == false)) {
			sample_enqueue_u16(&data->l_rx_sync_ring , p[rx_ptr][0]);
			sample_enqueue_u16(&data->r_rx_sync_ring , p[rx_ptr][1]);
			if (0 == check_pattern_sync(&data->rx_pace_pattern_info, &data->l_rx_sync_ring , data->rx_sample_cnt)) {
				data->rx_pace_pattern_synced = true;
			} else if (0 == check_pattern_sync(&data->rx_pace_pattern_info, &data->r_rx_sync_ring , data->rx_sample_cnt)) {
				data->rx_pace_pattern_synced = true;
			}
		}
		/* Update indexes */
		period_pos++;
		if (++rx_ptr >= runtime->buffer_size)
			rx_ptr = 0;
	}
	*period_elapsed = period_pos >= runtime->period_size;
	return rx_ptr;
}

/*!
 * @brief Hailo15 PCM playback function that:
 * 		  - Prepares block of samples for SCU.
 *        - Duplicate samples according to hailo15 playback samples pace pattern.
 */
unsigned int dw_pcm_hailo15_scu_tx_blk(struct dw_i2s_dev *dev,
                                       struct snd_pcm_runtime *runtime,
                                       unsigned int tx_ptr,
                                       bool *period_elapsed)
{
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;
	const u16(*p)[2] = (void *)runtime->dma_area;
	unsigned int period_pos = tx_ptr % runtime->period_size;
	int i;
	struct dwc_i2s_dma_block_desc *desc = DWC_I2S_DMA_BLK_RING__CURR_DESC_PTR(data->scu_dw_i2s_dma.playback_ring);
	u16(*scu_dma_blk_samples)[2];

	if (unlikely(DWC_I2S_DMA_BLK_DESC__STATE_GET(desc) == DWC_I2S_DMA_BLK_STATE_FULL)) {
		data->scu_dw_i2s_dma.playback_overrun++;
		*period_elapsed = period_pos >= runtime->period_size;
		return tx_ptr;
	}

	scu_dma_blk_samples = DWC_I2S_DMA_BLK_RING__CURR_SAMPLES_BUF(data->scu_dw_i2s_dma.playback_ring);
	for (i = 0; i < BLOCK_SAMPLE_BUF_SIZE; i++) {
		if (data->tx_dup_flag) {
			/* duplicate sample p[tx_ptr][] */
			scu_dma_blk_samples[i][0] = p[tx_ptr][0];
			scu_dma_blk_samples[i][1] = p[tx_ptr][1];
			data->tx_dup_flag = false;
			/* Update indexes */
            data->tx_sample_cnt++;
            period_pos++;
            if (++tx_ptr >= runtime->buffer_size)
                tx_ptr = 0;
            continue;
		}

        scu_dma_blk_samples[i][0] = p[tx_ptr][0];
        scu_dma_blk_samples[i][1] = p[tx_ptr][1];
        data->tx_dup_flag = sample_dup_check(data->tx_sample_cnt, &data->tx_pace_pattern_info);
        if (unlikely(data->tx_dup_flag)) {
			/* duplicate p[tx_ptr][] sample, by not incrementing tx_ptr and inrementing only i */
            continue;
        }
		/* Update indexes */
        data->tx_sample_cnt++;
        period_pos++;
        if (++tx_ptr >= runtime->buffer_size)
            tx_ptr = 0;
	}
    /* Block is full */
    DWC_I2S_DMA_BLK_DESC__STATE_SET(desc, DWC_I2S_DMA_BLK_STATE_FULL); /* Update current block as full*/
    DWC_I2S_DMA_BLK_RING__CURR_IDX_INC(data->scu_dw_i2s_dma.playback_ring); /* Inc to next block */

	*period_elapsed = period_pos >= runtime->period_size;
	return tx_ptr;
}

/*!
 * @brief Hailo15 PCM Record function that:
 * 		  - If Block from SCU is ready (FULL).
 *          1) Tries to find hailo15 record samples pace pattern.
 *          2) if record samples pace pattern found, then it will start removing samples
 *             according to hailo15 record samples pace pattern.
 */
unsigned int dw_pcm_hailo15_scu_rx_blk(struct dw_i2s_dev *dev,
                                       struct snd_pcm_runtime *runtime,
                                       unsigned int rx_ptr,
                                       bool *period_elapsed)
{
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;
	struct dwc_i2s_dma_block_desc *desc = DWC_I2S_DMA_BLK_RING__CURR_DESC_PTR(data->scu_dw_i2s_dma.record_ring);
	u16(*scu_dma_blk_samples)[2];
	u16(*p)[2] = (void *)runtime->dma_area;
	unsigned int period_pos = rx_ptr % runtime->period_size;
	int i;
	bool drop_sample_flag = false;

	if (unlikely(DWC_I2S_DMA_BLK_DESC__STATE_GET(desc) == DWC_I2S_DMA_BLK_STATE_EMPTY)) {
		data->scu_dw_i2s_dma.record_underrun++;
       	*period_elapsed = period_pos >= PERIOD_BYTES_MIN;
    	return rx_ptr;
    }

	scu_dma_blk_samples = (void *)DWC_I2S_DMA_BLK_RING__CURR_SAMPLES_BUF(data->scu_dw_i2s_dma.record_ring);
	for (i = 0; i < BLOCK_SAMPLE_BUF_SIZE; i++, data->rx_sample_cnt++) {
		if (data->rx_pace_pattern_synced == true) {
			drop_sample_flag = sample_drop_check(data->rx_sample_cnt, &data->rx_pace_pattern_info);
		}
		p[rx_ptr][0] = scu_dma_blk_samples[i][0];
		p[rx_ptr][1] = scu_dma_blk_samples[i][1];
		if (unlikely(drop_sample_flag)) {
			/* ignore p[rx_ptr][] by not incrementing rx_ptr and inrementing only i */
			continue;
		}

		if (unlikely(data->rx_pace_pattern_synced == false)) {
			sample_enqueue_u16(&data->l_rx_sync_ring , p[rx_ptr][0]);
			sample_enqueue_u16(&data->r_rx_sync_ring , p[rx_ptr][1]);
			if (0 == check_pattern_sync(&data->rx_pace_pattern_info, &data->l_rx_sync_ring , data->rx_sample_cnt)) {
				data->rx_pace_pattern_synced = true;
			} else if (0 == check_pattern_sync(&data->rx_pace_pattern_info, &data->r_rx_sync_ring , data->rx_sample_cnt)) {
				data->rx_pace_pattern_synced = true;
			}
		}
		/* Update indexes */
		period_pos++;
		if (++rx_ptr >= runtime->buffer_size)
			rx_ptr = 0;
	}
	DWC_I2S_DMA_BLK_DESC__STATE_SET(desc, DWC_I2S_DMA_BLK_STATE_EMPTY);	/* Update current block as empty */
	DWC_I2S_DMA_BLK_RING__CURR_IDX_INC(data->scu_dw_i2s_dma.record_ring); /* Inc current idx */

	*period_elapsed = period_pos >= runtime->period_size;
	return rx_ptr;
}

/*!
 * @brief hrtimer callback function that every H15_BLK_POLL_INTERVAL_NSEC poll for rx/tx block from and to SCU.
 */
enum hrtimer_restart scu_dma_blk_poll_timer_cb(struct hrtimer *timer)
{
	struct hailo15_priv_data *h15_priv_data = container_of(timer, struct hailo15_priv_data, scu_dma_blk_poll_timer);
	struct dw_i2s_dev *dev = h15_priv_data->dev;

	if (h15_priv_data->playback_poll_state_flag) {
		dw_pcm_push_tx(dev);
	}

	if (h15_priv_data->record_poll_state_flag) {
		dw_pcm_pop_rx(dev);
	}

	hrtimer_forward_now(timer, ns_to_ktime(H15_BLK_POLL_INTERVAL_NSEC));
    return HRTIMER_RESTART;
}

int dw_pcm_hailo15_open(struct snd_soc_component *component,
                        struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct dw_i2s_dev *dev = snd_soc_dai_get_drvdata(asoc_rtd_to_cpu(rtd, 0));
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		data->tx_sample_cnt = 0;
		data->tx_dup_flag = false;
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		data->rx_sample_cnt = 0;
		data->rx_pace_pattern_synced = false;
		data->l_rx_sync_ring .head = 0;
		data->l_rx_sync_ring .tail = 0;
		data->l_rx_sync_ring .count = 0;
		data->r_rx_sync_ring .head = 0;
		data->r_rx_sync_ring .tail = 0;
		data->r_rx_sync_ring .count = 0;
	}

	snd_soc_set_runtime_hwparams(substream, &dw_pcm_hardware);
	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	runtime->private_data = dev;

	return 0;
}

int dw_pcm_hailo15_scu_dma_open(struct snd_soc_component *component,
                                struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct dw_i2s_dev *dev = snd_soc_dai_get_drvdata(asoc_rtd_to_cpu(rtd, 0));
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;
	int i;

	snd_soc_set_runtime_hwparams(substream, &dw_pcm_hardware);
	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	runtime->private_data = dev;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		data->tx_sample_cnt = 0;
		data->tx_dup_flag = false;
		data->scu_dw_i2s_dma.playback_ring.idx = 0;
		data->scu_dw_i2s_dma.playback_ring.sample_idx = 0;
		for (i = 0; i < DWC_I2S_DMA_BLK_RING_SIZE; i++) {
			data->scu_dw_i2s_dma.shmem->playback.desc[i].state = DWC_I2S_DMA_BLK_STATE_EMPTY;
		}
		data->scu_dw_i2s_dma.shmem->playback.reset = 1;
		data->playback_poll_state_flag = true;
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		data->rx_sample_cnt = 0;
		data->rx_pace_pattern_synced = false;
		data->l_rx_sync_ring .head = 0;
		data->l_rx_sync_ring .tail = 0;
		data->l_rx_sync_ring .count = 0;
		data->r_rx_sync_ring .head = 0;
		data->r_rx_sync_ring .tail = 0;
		data->r_rx_sync_ring .count = 0;
		data->scu_dw_i2s_dma.record_ring.idx = 0;
		data->scu_dw_i2s_dma.record_ring.sample_idx = 0;
		for (i = 0; i < DWC_I2S_DMA_BLK_RING_SIZE; i++) {
			data->scu_dw_i2s_dma.shmem->record.desc[i].state = DWC_I2S_DMA_BLK_STATE_EMPTY;
		}
		data->scu_dw_i2s_dma.shmem->record.reset = 1;
		data->record_poll_state_flag = true;
	}

	if (data->playback_poll_state_flag ^ data->record_poll_state_flag) {
		/* Start polling timer */
		hrtimer_init(&data->scu_dma_blk_poll_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		data->scu_dma_blk_poll_timer.function = scu_dma_blk_poll_timer_cb;
		hrtimer_start(&data->scu_dma_blk_poll_timer, ns_to_ktime(H15_BLK_POLL_INTERVAL_NSEC/2), HRTIMER_MODE_REL);
	}
	return 0;
}

int dw_pcm_hailo15_scu_dma_close(struct snd_soc_component *component,
                                 struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct dw_i2s_dev *dev = snd_soc_dai_get_drvdata(asoc_rtd_to_cpu(rtd, 0));
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;

	if (data->playback_poll_state_flag ^ data->record_poll_state_flag) {
		hrtimer_cancel(&data->scu_dma_blk_poll_timer);
	}
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		data->record_poll_state_flag = false;
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		data->playback_poll_state_flag = false;
	}
	synchronize_rcu();
	return 0;
}

int dw_pcm_hailo15_hw_params(struct snd_soc_component *component,
                             struct snd_pcm_substream *substream,
                             struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dw_i2s_dev *dev = runtime->private_data;

	switch (params_channels(hw_params)) {
	case 2:
		break;
	default:
		dev_err(dev->dev, "invalid channels number\n");
		return -EINVAL;
	}

	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dev->tx_fn = dw_pcm_hailo15_tx_16;
		dev->rx_fn = dw_pcm_hailo15_rx_16;
		break;
	default:
		dev_err(dev->dev, "invalid format\n");
		return -EINVAL;
	}

	return 0;
}

int dw_pcm_hailo15_scu_dma_hw_params(struct snd_soc_component *component,
                                     struct snd_pcm_substream *substream,
                                     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dw_i2s_dev *dev = runtime->private_data;

	switch (params_channels(hw_params)) {
	case 2:
		break;
	default:
		dev_err(dev->dev, "invalid channels number\n");
		return -EINVAL;
	}

	switch (params_format(hw_params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dev->tx_fn = dw_pcm_hailo15_scu_tx_blk;
		dev->rx_fn = dw_pcm_hailo15_scu_rx_blk;
		break;
	default:
		dev_err(dev->dev, "invalid format\n");
		return -EINVAL;
	}

	return 0;
}
