/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ALSA SoC Synopsys I2S Audio Layer support for Hailo15
 *
 * sound/soc/dwc/dwc-i2s-hailo15.c
 *
 * Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include "local.h"

#ifdef CONFIG_SND_DESIGNWARE_HAILO15_STATS

struct per_cpu_stats {
	ktime_t start;
	ktime_t sum;
	ktime_t max;
	ktime_t min;
	s64 cnt;
};

DEFINE_PER_CPU(struct per_cpu_stats, per_cpu_ktime_irq_interval_rx);
DEFINE_PER_CPU(struct per_cpu_stats, per_cpu_ktime_irq_interval_tx);
DEFINE_PER_CPU(struct per_cpu_stats, per_cpu_ktime_irq_rx_execution);
DEFINE_PER_CPU(struct per_cpu_stats, per_cpu_ktime_irq_tx_execution);

void per_cpu_do_irq_interval_stats__add(int stream)
{
	struct per_cpu_stats *stats = this_cpu_ptr((stream == SNDRV_PCM_STREAM_CAPTURE) ?
										&per_cpu_ktime_irq_interval_rx :
										&per_cpu_ktime_irq_interval_tx);

	ktime_t ktime_current = ktime_get();
	ktime_t ktime_diff;

	if(unlikely(stats->start == 0)) {
		stats->start = ktime_current;
		return;
	}
	stats->cnt += 1;

	ktime_diff = ktime_sub_ns(ktime_current, stats->start);
	stats->sum = ktime_add_ns(stats->sum, ktime_diff);
	/* Update current as the new start */
	stats->start = ktime_current;

	if(unlikely(stats->min == 0) && unlikely(stats->max == 0)) {
		stats->max = ktime_diff;
		stats->min = ktime_diff;
		return;
	}

	if (ktime_after(ktime_diff, stats->max)) {
		stats->max = ktime_diff;
	}

	if (ktime_before(ktime_diff, stats->min)) {
		stats->min = ktime_diff;
	}
}

void per_cpu_do_irq_execution_stats__start(int stream)
{
	struct per_cpu_stats *stats = this_cpu_ptr((stream == SNDRV_PCM_STREAM_CAPTURE) ?
										&per_cpu_ktime_irq_rx_execution :
										&per_cpu_ktime_irq_tx_execution);

	stats->start = ktime_get();
	stats->cnt += 1;
}

void per_cpu_do_irq_execution_stats__end(int stream)
{
	struct per_cpu_stats *stats = this_cpu_ptr((stream == SNDRV_PCM_STREAM_CAPTURE) ?
										&per_cpu_ktime_irq_rx_execution :
										&per_cpu_ktime_irq_tx_execution);
	ktime_t ktime_end = ktime_get();
	ktime_t ktime_diff;

	ktime_diff = ktime_sub_ns(ktime_end, stats->start);
	stats->sum = ktime_add_ns(stats->sum, ktime_diff);

	if(unlikely(stats->min == 0) && unlikely(stats->max == 0)) {
		stats->min = ktime_diff;
		stats->max = ktime_diff;
		return;
	}

	if (ktime_after(ktime_diff, stats->max)) {
		stats->max = ktime_diff;
	}
	if (ktime_before(ktime_diff, stats->min)) {
		stats->min = ktime_diff;
	}
}

static int proc_scu_irq_stats(struct seq_file *s, struct scu_i2s_irq_stat *stats)
{
	seq_printf(s, "\tmin %-15lld, max %-15lld, avg %-15lld\n",
		stats->min, stats->max, (stats->cnt > 0) ? ktime_divns(stats->sum, stats->cnt) : 0);

	return 0;
}

static int proc_cpu_irq_stats(struct seq_file *s, int cpu, struct per_cpu_stats *stats)
{
	seq_printf(s, "\tCPU: %d: min %-15lld, max %-15lld, avg %-15lld\n",
		cpu, stats->min, stats->max, (stats->cnt > 0) ? ktime_divns(stats->sum, stats->cnt) : 0);

	return 0;
}

#endif /* CONFIG_SND_DESIGNWARE_HAILO15_STATS */

/*!
 * @brief dump driver stats.
 */
int proc_stats(struct seq_file *s, void *v)
{
	struct dw_i2s_dev *dev = PDE_DATA(file_inode(s->file));
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;
#ifdef CONFIG_SND_DESIGNWARE_HAILO15_STATS
	int cpu;
#endif
	if (dev->cfg_id == CONFIG_ID_DW_I2S_HAILO15_SCU_DMA) {
	    seq_printf(s, "playback: sw-underrun %15u, hw-overrun %15u\n",
			data->scu_dw_i2s_dma.shmem->playback.stats.sw_underrun,
			data->scu_dw_i2s_dma.shmem->playback.stats.hw_overrun);
	    seq_printf(s, "capture:  sw-overrun %15u,  hw-overrun %15u\n",
			data->scu_dw_i2s_dma.shmem->record.stats.sw_overrun,
			data->scu_dw_i2s_dma.shmem->record.stats.hw_overrun);
	} else {
	    seq_printf(s, "Overrun stats: rx %u, tx %u:\n", data->hw_rx_overrun, data->hw_tx_overrun);
	}

#ifdef CONFIG_SND_DESIGNWARE_HAILO15_STATS
    seq_printf(s, "IRQ %s rx execution time (nsec):\n",
		(dev->cfg_id == CONFIG_ID_DW_I2S_HAILO15_SCU_DMA) ? "blk" : "");
	for_each_possible_cpu(cpu) {
		struct per_cpu_stats *stats = per_cpu_ptr(&per_cpu_ktime_irq_rx_execution, cpu);
		proc_cpu_irq_stats(s, cpu, stats);
	}
    seq_printf(s, "IRQ %s tx execution time (nsec):\n",
		(dev->cfg_id == CONFIG_ID_DW_I2S_HAILO15_SCU_DMA) ? "blk" : "");
	for_each_possible_cpu(cpu) {
		struct per_cpu_stats *stats = per_cpu_ptr(&per_cpu_ktime_irq_tx_execution, cpu);
		proc_cpu_irq_stats(s, cpu, stats);
	}
    seq_printf(s, "IRQ %s rx interval time (nsec):\n",
	(dev->cfg_id == CONFIG_ID_DW_I2S_HAILO15_SCU_DMA) ? "blk" : "");
	for_each_possible_cpu(cpu) {
		struct per_cpu_stats *stats = per_cpu_ptr(&per_cpu_ktime_irq_interval_rx, cpu);
		proc_cpu_irq_stats(s, cpu, stats);
	}
    seq_printf(s, "IRQ %s tx interval time (nsec):\n",
		(dev->cfg_id == CONFIG_ID_DW_I2S_HAILO15_SCU_DMA) ? "blk" : "");
	for_each_possible_cpu(cpu) {
		struct per_cpu_stats *stats = per_cpu_ptr(&per_cpu_ktime_irq_interval_tx, cpu);
		proc_cpu_irq_stats(s, cpu, stats);
	}

	if (dev->cfg_id == CONFIG_ID_DW_I2S_HAILO15_SCU_DMA) {
		seq_printf(s, "SCU IRQ rx execution time (nsec):\n");
		proc_scu_irq_stats(s, &data->scu_dw_i2s_dma.shmem->irq_rx_stats.execution);

		seq_printf(s, "SCU IRQ tx execution time (nsec):\n");
		proc_scu_irq_stats(s, &data->scu_dw_i2s_dma.shmem->irq_tx_stats.execution);

		seq_printf(s, "SCU IRQ rx interval time (nsec):\n");
		proc_scu_irq_stats(s, &data->scu_dw_i2s_dma.shmem->irq_rx_stats.interval);

		seq_printf(s, "SCU IRQ tx interval time (nsec):\n");
		proc_scu_irq_stats(s, &data->scu_dw_i2s_dma.shmem->irq_tx_stats.interval);
	}
#endif /* CONFIG_SND_DESIGNWARE_HAILO15_STATS */

	return 0;
}

int proc_stats_fopen(struct inode *inode, struct file *file)
{
    return single_open(file, proc_stats, NULL);
};

static const struct proc_ops proc_stats_fops = {
	.proc_open	= proc_stats_fopen,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

/*!
 * @brief proc for clearing stats.
 *        E.g.: echo 1 > /proc/h15-snd-<device-name>/stats_clear
 */
static ssize_t proc_write_cache_clear(struct file *filp, const char __user *buf, size_t count, loff_t *offp)
{
#ifdef CONFIG_SND_DESIGNWARE_HAILO15_STATS
    int cpu;
#endif
    int ret;
    unsigned long long clear;
	struct dw_i2s_dev *dev = PDE_DATA(file_inode(filp));
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;
	struct dwc_i2s_dma_shmem_map *shmem = data->scu_dw_i2s_dma.shmem;

    ret = kstrtoull_from_user(buf, count, 10, &clear);
    if (ret) {
        dev_err(dev->dev, "Invalid decimal input, rc = %d\n", ret);
        return ret;
    } else if (clear) {
        dev_info(dev->dev, "Clearing stats\n");
		if (dev->cfg_id == CONFIG_ID_DW_I2S_HAILO15_SCU_DMA) {
			/* Reset playback/record stats */
			shmem->record.stats = (struct dwc_i2s_dma_record_stats){ 0 };
			shmem->playback.stats = (struct dwc_i2s_dma_playback_stats){ 0 };
#ifdef CONFIG_SND_DESIGNWARE_HAILO15_STATS
			/* Reset IRQ Rx/Tx execution/interval stats */
			shmem->irq_rx_stats = (struct irq_stats){ 0 };
			shmem->irq_tx_stats = (struct irq_stats){ 0 };
#endif /* CONFIG_SND_DESIGNWARE_HAILO15_STATS */
		} else {
			data->hw_rx_overrun = 0;
			data->hw_tx_overrun = 0;
		}
#ifdef CONFIG_SND_DESIGNWARE_HAILO15_STATS
		for_each_possible_cpu(cpu) {
			*per_cpu_ptr(&per_cpu_ktime_irq_rx_execution, cpu) = (struct per_cpu_stats){ 0 };
			*per_cpu_ptr(&per_cpu_ktime_irq_tx_execution, cpu) = (struct per_cpu_stats){ 0 };
			*per_cpu_ptr(&per_cpu_ktime_irq_interval_rx, cpu) = (struct per_cpu_stats){ 0 };
			*per_cpu_ptr(&per_cpu_ktime_irq_interval_tx, cpu) = (struct per_cpu_stats){ 0 };
		}
#endif /* CONFIG_SND_DESIGNWARE_HAILO15_STATS */
    }
	*offp = count;
	return count;
}

static const struct proc_ops proc_stats_clear_fops = {
	.proc_write	= proc_write_cache_clear,
};

/*!
 * @brief Create all proc entries
 *        - /proc/<device-name>
 *        - /proc/<device-name>/stats
 *        - /proc/<device-name>/stats_clear
 *
 * @return 0(ok), 1(failed)
 */
int hailo15_proc_entries_create(struct platform_device *pdev)
{
    struct proc_dir_entry *entry;
	struct dw_i2s_dev *dev = dev_get_drvdata(&pdev->dev);
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;
	char proc_dir_name[100] = "h15-snd-";

	strlcat(proc_dir_name, pdev->name, sizeof(proc_dir_name));

	dev_info(dev->dev, "creating proc dir %s ...\n", proc_dir_name);

    /* Create proc dir /proc/h15-snd-<device-name> */
    data->proc_dir = proc_mkdir(proc_dir_name, NULL);
    if (data->proc_dir == NULL) {
        dev_err(dev->dev, "Failed to create proc dir %s\n", proc_dir_name);
        return 1;
    }

    /* Create proc dir /proc/h15-snd-<device-name>/stats */
    entry = proc_create_data("stats", 0, data->proc_dir, &proc_stats_fops, dev);
    if (!entry) {
        dev_err(dev->dev, "Unable to create /proc/%s/stats\n", pdev->name);
        return 1;
    }

    /* Create proc dir /proc/h15-snd-<device-name>/stats_clear */
    entry = proc_create_data("stats_clear",  0, data->proc_dir, &proc_stats_clear_fops, dev);
    if (!entry) {
        dev_err(dev->dev, "Unable to create /proc/%s/stats_clear\n", pdev->name);
        return 1;
    }

    return 0;
}

/*!
 * @brief Remove all proc entries
 *        - /proc/<device-name>
 *        - /proc/<device-name>/stats
 *        - /proc/<device-name>/stats_clear
 *
 * @return 0(ok), 1(failed)
 */
int hailo15_proc_entries_remove(struct platform_device *pdev)
{
	struct dw_i2s_dev *dev = dev_get_drvdata(&pdev->dev);
	struct hailo15_priv_data *data = (struct hailo15_priv_data *)dev->priv;
	char proc_dir_name[100] = "h15-snd-";

	strlcat(proc_dir_name, pdev->name, sizeof(proc_dir_name));

    remove_proc_entry("stats_clear",  data->proc_dir);
    remove_proc_entry("stats",  	  data->proc_dir);
    remove_proc_entry("proc_dir_name",NULL);

    return 0;
}

static void ring_setup(struct dwc_i2s_dma_block_ring *ring, struct dwc_i2s_dma_block *block, struct dwc_i2s_dma_block_desc *desc)
{
	int i;

	ring->block = block;
	ring->desc = desc;
	ring->idx = 0;
	ring->sample_idx = 0;
	for (i = 0; i < DWC_I2S_DMA_BLK_RING_SIZE; i++) {
		ring->desc[i].state = DWC_I2S_DMA_BLK_STATE_EMPTY;
	}
}

static void shmem_reset(struct dwc_i2s_dma_shmem_map *shmem)
{
	/* Reset IRQ Rx/Tx execution/interval stats */
	shmem->irq_rx_stats = (struct irq_stats){ 0 };
	shmem->irq_tx_stats = (struct irq_stats){ 0 };

	/* Reset playback/record stats */
	shmem->record.stats = (struct dwc_i2s_dma_record_stats){ 0 };
	shmem->playback.stats = (struct dwc_i2s_dma_playback_stats){ 0 };

	/* Reset signals */
	shmem->record.reset = 0;
	shmem->playback.reset = 0;
}

static int __hailo15_extra_common_probe(struct platform_device *pdev, u32 stream, struct hailo15_priv_data *data)
{
	int i, ret;
	struct hailo15_pace_pattern_info *pace_info = (stream == SNDRV_PCM_STREAM_CAPTURE) ? &data->rx_pace_pattern_info : &data->tx_pace_pattern_info;
	const char *of_sample_pace_str = (stream == SNDRV_PCM_STREAM_CAPTURE) ? "rx-sample-pace" : "tx-sample-pace";
	const char *property_value;

	if (stream == SNDRV_PCM_STREAM_CAPTURE) {
		ret = of_property_read_u32(pdev->dev.of_node, "rx-sample-pace-pattern-repetitions", &pace_info->pattern_repetitions);
		if (ret) {
			return ret;
		}
		ret = of_property_read_string(pdev->dev.of_node, "rx-sample-cmp-to", &property_value);
		if (ret == 0) {
			if (!strcmp(property_value, "zero")) {
				pace_info->sample_cmp = SAMPLE_CMP_TO_ZERO;
			} else if (!strcmp(property_value, "prev-val")) {
				pace_info->sample_cmp = SAMPLE_CMP_TO_PREV_SAMPLE;
			} else {
				return -EINVAL;
			}
		} else {
			return ret;
		}
	} else {
		if (of_property_read_bool(pdev->dev.of_node, "tx-sample-offset")) {
			ret = of_property_read_u32(pdev->dev.of_node, "tx-sample-offset", &pace_info->offset);
			if (ret) {
				return ret;
			}
		}
	}

	pace_info->pace_cnt = of_property_count_u32_elems(pdev->dev.of_node, of_sample_pace_str);
	if (pace_info->pace_cnt == -EINVAL) {
		return -EINVAL;
	}
    pace_info->pace = devm_kzalloc(&pdev->dev, sizeof(*(pace_info->pace)) * pace_info->pace_cnt, GFP_KERNEL);
    if (!pace_info->pace) {
        return -ENOMEM;
	}
	ret = of_property_read_u32_array(pdev->dev.of_node, of_sample_pace_str, pace_info->pace, pace_info->pace_cnt);
	if (ret) {
		return ret;
	}
    pace_info->pace_range = devm_kzalloc(&pdev->dev, sizeof(*(pace_info->pace_range)) * pace_info->pace_cnt, GFP_KERNEL);
    if (!pace_info->pace_range) {
        return -ENOMEM;
	}
	for(i = 0; i < pace_info->pace_cnt; i++) {
		pace_info->pace_range[i].start = (i==0) ? 0 : pace_info->pace_range[i-1].end + 1;
		pace_info->pace_range[i].end = pace_info->pace_range[i].start + pace_info->pace[i] - 1;
		pace_info->pace_sum += pace_info->pace[i];
	}

	return 0;
}

int hailo15_extra_probe(struct platform_device *pdev)
{
	int ret;
	struct resource res;
	struct device_node *np;
	resource_size_t size;
	struct hailo15_priv_data *data;
	struct dw_i2s_dev *dev = dev_get_drvdata(&pdev->dev);
	u32 ring_size;

    /* Allocate Hailo15 specific private data */
    data = devm_kzalloc(&pdev->dev, sizeof(struct hailo15_priv_data), GFP_KERNEL);
    if (!data) {
        return -ENOMEM;
	}
	ret = __hailo15_extra_common_probe(pdev, SNDRV_PCM_STREAM_CAPTURE, data);
	if (ret) {
		return ret;
	}
	ret = __hailo15_extra_common_probe(pdev, SNDRV_PCM_STREAM_PLAYBACK, data);
	if (ret) {
		return ret;
	}

	if (data->rx_pace_pattern_info.pace_sum * data->rx_pace_pattern_info.pattern_repetitions == 0) {
		dev_err(&pdev->dev, "Invalid value of pace sum or pace pattern sync times, both must be != 0\n");
		return EINVAL;
	}

	ring_size = roundup_pow_of_two(data->rx_pace_pattern_info.pace_sum * data->rx_pace_pattern_info.pattern_repetitions);
	data->l_rx_sync_ring.size = ring_size;
	data->r_rx_sync_ring.size = ring_size;

	data->l_rx_sync_ring.buf = devm_kzalloc(&pdev->dev, ring_size * sizeof(u16), GFP_KERNEL);
	if (!data->l_rx_sync_ring.buf) {
		return -ENOMEM;
	}

	data->r_rx_sync_ring.buf = devm_kzalloc(&pdev->dev, ring_size * sizeof(u16), GFP_KERNEL);
	if (!data->r_rx_sync_ring.buf) {
		return -ENOMEM;
	}

	data->dev = dev;

	if (dev->cfg_id == CONFIG_ID_DW_I2S_HAILO15_SCU_DMA) {
		np = of_parse_phandle(pdev->dev.of_node, "shmem", 0);
		if (!of_device_is_compatible(np, "hailo,hailo15-i2s-shmem")) {
			dev_err(&pdev->dev, "invalid shmem compatability of_node\n");
			return -ENXIO;
		}

		ret = of_address_to_resource(np, 0, &res);
		of_node_put(np);
		if (ret) {
			dev_err(&pdev->dev, "failed to get I2S scu dma shmem\n");
			return ret;
		}

		size = resource_size(&res);
		data->scu_dw_i2s_dma.shmem = devm_ioremap(&pdev->dev, res.start, size);
		if (!data->scu_dw_i2s_dma.shmem) {
			dev_err(&pdev->dev, "failed to ioremap I2S scu dma shmem\n");
			return -EADDRNOTAVAIL;
		}

		/* Setup record/playback block rings */
		ring_setup(&data->scu_dw_i2s_dma.record_ring,
			   data->scu_dw_i2s_dma.shmem->record.block,
			   data->scu_dw_i2s_dma.shmem->record.desc);
		ring_setup(&data->scu_dw_i2s_dma.playback_ring,
			   data->scu_dw_i2s_dma.shmem->playback.block,
			   data->scu_dw_i2s_dma.shmem->playback.desc);

		/* Setup shmem area */
		shmem_reset(data->scu_dw_i2s_dma.shmem);
	}

	/* Store Hailo15 specific private data and store it inside driver data (struct dw_i2s_dev). */
	dev->priv = data;

	return 0;
}
