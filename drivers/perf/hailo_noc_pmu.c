#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/scmi_protocol.h>

#include <linux/vmalloc.h>
#include <linux/notifier.h>
#include <linux/minmax.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/kstrtox.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/mod_devicetable.h>
#include <linux/device.h>

#include <linux/soc/hailo/scmi_hailo_protocol.h>
#include <linux/soc/hailo/scmi_hailo_ops.h>

#include <asm/page.h>

#define DEFAULT_SAMPLE_TIME_US (50)
#define MINIMUM_SAMPLE_TIME_US (30)
#define NUMBER_OF_COUNTERS (4)

#define DEFAULT_FILTER \
	{ \
		.total = false, \
		.routeidmask = 0, \
		.window_size = -1, \
		.opcode = 0xF, \
		.status = 0x3, \
		.length = 0xF, \
		.urgency = 0, \
	}

#define DEFAULT_START_PARAMS ((struct scmi_hailo_noc_start_measure_a2p) { \
		.sample_time_us = DEFAULT_SAMPLE_TIME_US, \
		.after_trigger_percentage = 50, \
		.is_freerunning = false, \
		.csm_enabled = true, \
		.dsm_rx_enabled = true, \
		.dsm_tx_enabled = true, \
		.num_counters = NUMBER_OF_COUNTERS, \
		.filters = { \
			/* Filter 1 */ \
			{ \
				.total = true, \
				.routeidmask = 0, \
				.window_size = -1, \
				.opcode = 0xF, \
				.status = 0x3, \
				.length = 0xF, \
				.urgency = 0, \
			}, \
			/* Filter 2 */  \
			DEFAULT_FILTER, \
			/* Filter 3 */  \
			DEFAULT_FILTER, \
			/* Filter 4 */  \
			DEFAULT_FILTER, \
		} \
	})

struct indexed_device_attribute {
	struct device_attribute device_attribute;
	int index;
};

#define __INDEXED_ATTR_RW_MODE(_name, _category, _mode, _index) {				\
	.device_attribute = { 							\
		.attr	= { .name = __stringify(_name),				\
				.mode = VERIFY_OCTAL_PERMISSIONS(_mode) },	\
		.show	= _name##_##_category##_show,						\
		.store	= _name##_##_category##_store,					\
	}, 									\
	.index	= _index,							\
}

#define INDEXED_DEVICE_ATTR_ADMIN_RW(_name, _category, _index) \
	struct indexed_device_attribute dev_attr_##_name##_##_category##_index = __INDEXED_ATTR_RW_MODE(_name, _category, 0600, _index)

struct noc_sample_h15 {
	uint32_t noc_counters[4];
	uint32_t dsm_rx_counter;
	uint32_t dsm_tx_counter;
	uint32_t csm_counter;
	uint64_t timestamp;
	bool triggered;
} __packed;

struct noc_sample_h15l {
	uint32_t noc_counters[5];
	uint32_t dsm_rx_counter;
	uint32_t dsm_tx_counter;
	uint32_t csm_counter;
	uint64_t timestamp;
	bool triggered;
} __packed;

struct measurement_params {
	struct scmi_hailo_noc_start_measure_a2p start_params;
	bool active_counters[4];
	bool limit_samples;
	unsigned int sample_count_limit; // if limit_samples is true, this is the limit
};

struct hailo_pmu_aux_buffer {
	char *data;
	unsigned long size;
};

struct hailo_pmu {
	struct pmu pmu;
	struct platform_device *pdev;
	void *scu_buf;

	/*
		The NOC measurements are recorded by the SCU processor, and we treat it as a HW PMU, and thus we use the aux buffer interface of the perf framework.
		We communicate with the SCU Processor via the Hailo-SCMI intefrace to start and stop the measurements, and get a notification when measurements end.
		We use a new aux buffer for each measurement of the SCU processor, which is at most 200 samples, and we copy the data from the SCU processor to the aux buffer.

		The PMU interface calls our start and stop callbacks from atomic context, and the aux buffers are expected to always be allocated either there
		or in 'interrupt context', and it is expected for the driver to finish with the aux buffer before the stop callback returns.

		The SCMI interface (API calls and notifications) cannot be called from atomic context.

		To deal with these constraints, we implement two synchronization mechanisms:
		1. For the atomic PMU context, we implement a spinlock called 'pmu_lock' to protect from concurrent access to the PMU side from different cores.
		2. For the SCMI interface, we use a workqueue to handle queueing the start and stop requests from the PMU atomic context, and to queue
		   'measurement-ended' notifications. When we handle the measurement-ended notification, we want to copy the counters data to the aux buffer
		   and allocate a new aux buffer, but we have to be in an atomic PMU context, so we disable IRQs and lock the pmu_lock, just for the time we copy the data.


		We only allow 1 measurement to run at a time (from a single core), and we set the 'pmu_busy' when we have accepted a start request, and until
		the stop request.
		We also don't allow a new measurement to start until the SCMI interface has finished handling the 'measurement-ended' notifications from the last measurement,
		and sending the last 'stop measurement' request to the SCU processor.
		We set the 'scmi_busy' flag when we have accepted a start request in the atomic context, and we clear it from the SCMI context when the last measurement has ended.
	 */

	/* this lock protects access to the pmu atomic side */
	spinlock_t pmu_lock;

	/* these variables are used by the PMU atomic context */
	bool pmu_busy;
	struct perf_event *event;

	/* these variables are used by the SCMI/preemptive context */
	bool scmi_busy; // set by PMU context, cleared by SCMI context
	struct measurement_params params; // changable by userspace
	struct measurement_params active_params; // used by current running measurement
	unsigned int started_initiated_count;
	unsigned int ended_handled_count;
	bool stop_requested;
	const struct scmi_hailo_ops *scmi_ops;
	unsigned int remaining_samples; // used if limit_samples is true

	/* SCMI work-queue */
	struct workqueue_struct* scmi_wq;
	struct work_struct scmi_stop_work;
	struct work_struct scmi_start_work;

	uint32_t sample_size;
};

struct hailo_pmu_scmi_measurement_ended_work
{
	struct work_struct work;
	struct hailo_pmu *hailo_pmu;
	struct scmi_hailo_noc_measurement_ended_notification report;
};

void hailo_pmu_scmi_halt(struct hailo_pmu *hailo_pmu)
{
	if (hailo_pmu->ended_handled_count == hailo_pmu->started_initiated_count) {
		BUG_ON(!hailo_pmu->scmi_busy);
		hailo_pmu->scmi_busy = false;
		hailo_pmu->stop_requested = false;
	}
}

void hailo_pmu_scmi_start_measure(struct hailo_pmu *hailo_pmu)
{
	int ret = 0;

	BUG_ON(hailo_pmu->ended_handled_count != hailo_pmu->started_initiated_count);
	ret = hailo_pmu->scmi_ops->start_measure(&hailo_pmu->active_params.start_params);
	if (ret) {
		pr_err("Failed to start NoC bandwidth measurement on rc: %d\n", ret);
		hailo_pmu_scmi_halt(hailo_pmu);
		return;
	}

	hailo_pmu->started_initiated_count++;
}

void hailo_pmu_scmi_start_work_handle(struct work_struct *work)
{
	struct hailo_pmu *hailo_pmu = (struct hailo_pmu *)container_of(work, struct hailo_pmu, scmi_start_work);
	int i;

	// we have to copy the params to active params,
	// because the params can be changed by the user while the measurement is running
	// and we want to keep the active params consistent with the current running measurement
	hailo_pmu->active_params = hailo_pmu->params;
	for (i = 0; i < NUMBER_OF_COUNTERS; i++) {
		if (!hailo_pmu->active_params.active_counters[i]) {
			/* This configuration effectively does not let any packet pass */
			hailo_pmu->active_params.start_params.filters[i].opcode = 0;
			hailo_pmu->active_params.start_params.filters[i].total = 0;
		}
	}

	if (hailo_pmu->active_params.limit_samples) {
		hailo_pmu->remaining_samples = hailo_pmu->active_params.sample_count_limit;
	}

	hailo_pmu_scmi_start_measure(hailo_pmu);
}

void hailo_pmu_scmi_stop_work_handle(struct work_struct *work)
{
	struct hailo_pmu *hailo_pmu = (struct hailo_pmu *)container_of(work, struct hailo_pmu, scmi_stop_work);
	bool measurement_was_running;
	int ret = 0;

	hailo_pmu->stop_requested = true;

	ret = hailo_pmu->scmi_ops->stop_measure(&measurement_was_running);
	if (ret) {
		pr_err("Failed to stop NoC bandwidth measurement on rc: %d\n", ret);
	}

	hailo_pmu_scmi_halt(hailo_pmu);
}

// this function is called from SCMI context, so we disable IRQs and lock the pmu_lock to be in PMU atomic context
void hailo_pmu_fill_aux(struct hailo_pmu *hailo_pmu, const volatile void __iomem *scu_buf, unsigned int number_of_samples, unsigned int *actually_written)
{
	unsigned long flags;
	unsigned long aux_buf_remaining_size;
	unsigned long write_size;
	unsigned long head_offset;
	unsigned long first_part_size;
	struct perf_output_handle handle;
	struct hailo_pmu_aux_buffer *aux_buf;

	*actually_written = 0;

	local_irq_save(flags);
	spin_lock(&hailo_pmu->pmu_lock);

	if (!hailo_pmu->pmu_busy) {
		goto unlock;
	}

	aux_buf = perf_aux_output_begin(&handle, hailo_pmu->event);
	if (aux_buf == NULL) {
		pr_err("hailo_pmu_fill_aux: failed to allocate aux buffer\n");
		goto unlock;
	}

	aux_buf_remaining_size = handle.size / hailo_pmu->sample_size;
	if (number_of_samples > aux_buf_remaining_size) {
		number_of_samples = aux_buf_remaining_size;
	}

	/* Copy the samples to the aux buffer and update current state */
	write_size = number_of_samples * hailo_pmu->sample_size;
	head_offset = handle.head % aux_buf->size; /* head pointer in handle is stored without modulo */
	first_part_size = min(aux_buf->size - head_offset, write_size);
	memcpy_fromio(aux_buf->data + head_offset, scu_buf, first_part_size);
	if (first_part_size < write_size) {
		memcpy_fromio(aux_buf->data, scu_buf + first_part_size, write_size - first_part_size);
	}

	*actually_written = number_of_samples;
	perf_aux_output_end(&handle, write_size);


unlock:
	spin_unlock(&hailo_pmu->pmu_lock);
	local_irq_restore(flags);
}

void hailo_pmu_scmi_measurement_ended_work_handle(struct work_struct *work) {
	struct hailo_pmu_scmi_measurement_ended_work *measurement_ended_work = (struct hailo_pmu_scmi_measurement_ended_work *)container_of(work, struct hailo_pmu_scmi_measurement_ended_work, work);
	struct hailo_pmu *hailo_pmu = measurement_ended_work->hailo_pmu;
	struct scmi_hailo_noc_measurement_ended_notification *report = &measurement_ended_work->report;
	unsigned int number_of_samples;
	unsigned int written_samples;
	void* scu_buf;

	BUG_ON(hailo_pmu->ended_handled_count + 1 != hailo_pmu->started_initiated_count);
	BUG_ON(!hailo_pmu->scmi_busy);

	hailo_pmu->ended_handled_count++;

	if (hailo_pmu->stop_requested) {
		/* Stop has been requested before we finished measuring */
		hailo_pmu_scmi_halt(hailo_pmu);
		goto Exit;
	}

	number_of_samples = (report->sample_end_index - report->sample_start_index + 1);
	/*
	 * In case we received more samples than the user requested,
	 * we need to adjust the number of samples to write to the aux buffer
	 */
	if (hailo_pmu->active_params.limit_samples) {
		if (number_of_samples > hailo_pmu->remaining_samples) {
			number_of_samples = hailo_pmu->remaining_samples;
		}
	}

	scu_buf = (void *)((char *)(hailo_pmu->scu_buf) + (hailo_pmu->sample_size * report->sample_start_index));
	hailo_pmu_fill_aux(hailo_pmu, scu_buf, number_of_samples, &written_samples);

	if (hailo_pmu->active_params.limit_samples) {
		BUG_ON(written_samples > hailo_pmu->remaining_samples);
		hailo_pmu->remaining_samples -= written_samples;
	}

	/* Continue capturing */
	hailo_pmu_scmi_start_measure(hailo_pmu);

Exit:
	kfree(measurement_ended_work);
}

static int hailo_pmu_add(struct perf_event *event, int flags)
{
	struct hailo_pmu *hailo_pmu = container_of(event->pmu, struct hailo_pmu, pmu);
	int ret = 0;

	spin_lock(&hailo_pmu->pmu_lock);

	if (hailo_pmu->pmu_busy || hailo_pmu->scmi_busy) {
		ret = -EBUSY;
		goto unlock;
	}

	hailo_pmu->event = event;

	hailo_pmu->event->hw.state &= ~PERF_HES_STOPPED;
	hailo_pmu->pmu_busy = true;
	hailo_pmu->scmi_busy = true;

	queue_work(hailo_pmu->scmi_wq, &hailo_pmu->scmi_start_work);
	ret = 0;

unlock:
	spin_unlock(&hailo_pmu->pmu_lock);

	return 0;
}

static void hailo_pmu_start(struct perf_event *event, int flags)
{
	hailo_pmu_add(event, flags);
}

static void hailo_pmu_stop(struct perf_event *event, int flags)
{
	struct hailo_pmu *hailo_pmu = container_of(event->pmu, struct hailo_pmu, pmu);

	spin_lock(&hailo_pmu->pmu_lock);

	if (!hailo_pmu->pmu_busy) {
		goto unlock;
	}

	hailo_pmu->event->hw.state |= PERF_HES_STOPPED;
	perf_event_update_userpage(hailo_pmu->event);

	hailo_pmu->event = NULL;

	queue_work(hailo_pmu->scmi_wq, &hailo_pmu->scmi_stop_work);

	hailo_pmu->pmu_busy = false;

unlock:
	spin_unlock(&hailo_pmu->pmu_lock);
}

static void *hailo_pmu_setup_aux(struct perf_event *event, void **pages,
								 int nr_pages, bool overwrite)
{
	int i;
	struct page **page_list;
	void *flat_buf;

	struct hailo_pmu_aux_buffer *aux_buf = kzalloc(sizeof(*aux_buf), GFP_KERNEL);
	if (!aux_buf)
		return NULL;

	/* Map the pages into a flat contiguous buffer */
	page_list = kcalloc(nr_pages, sizeof(*page_list), GFP_KERNEL);
	if (!page_list)
		return NULL;

	for (i = 0; i < nr_pages; i++)
		page_list[i] = virt_to_page(pages[i]);

	flat_buf = vmap(page_list, nr_pages, VM_MAP, PAGE_KERNEL);

	kfree(page_list);


	aux_buf->data = flat_buf;
	aux_buf->size = nr_pages * PAGE_SIZE;

	return aux_buf;
}

static void hailo_pmu_free_aux(void *aux)
{
	struct hailo_pmu_aux_buffer *aux_buf = aux;

	vunmap(aux_buf->data);
	kfree(aux_buf);
}

static ssize_t hailo_pmu_sysfs_show(struct device *dev, struct device_attribute *attr, char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);
	return sysfs_emit(page, "event=0x%02llx\n", pmu_attr->id);
}

/*********************
 * Define attributes *
 *********************/

/* Event attributes*/

#define HAILO_PMU_EVENT_ATTR(_name, _config)		\
	PMU_EVENT_ATTR_ID(_name, hailo_pmu_sysfs_show, _config)

static struct attribute *hailo_pmu_event_attrs[] = {
	HAILO_PMU_EVENT_ATTR(hailo_noc_bw, 0),
	NULL,
};

static const struct attribute_group hailo_pmu_events_attr_group = {
	.name = "events",
	.attrs = hailo_pmu_event_attrs,
};

/* Format attributes */

#define HAILO_PMU_FORMAT_ATTR_NUM_SAMPLES_MASK (GENMASK_ULL(31, 0))
PMU_FORMAT_ATTR(num_samples, "config:0-31");
#define HAILO_PMU_FORMAT_ATTR_SAMPLE_TIME_MASK (GENMASK_ULL(63, 32))
PMU_FORMAT_ATTR(sample_time_us, "config:32-63");
#define HAILO_PMU_FORMAT_ATTR_EVENT_MASK (GENMASK_ULL(7, 0))
PMU_FORMAT_ATTR(event, "config1:0-7");
#define HAILO_PMU_FORMAT_ATTR_RUNNING_MODE_MASK (GENMASK_ULL(8, 8))
PMU_FORMAT_ATTR(running_mode, "config1:8");


static struct attribute *hailo_pmu_format_attrs[] = {
	&format_attr_num_samples.attr,
	&format_attr_sample_time_us.attr,
	&format_attr_event.attr,
	&format_attr_running_mode.attr,
	NULL,
};

static const struct attribute_group hailo_pmu_format_attr_group = {
	.name = "format",
	.attrs = hailo_pmu_format_attrs,
};

/* Counter attributes */

static int get_attribute_index(struct device_attribute *attr)
{
	int index = ((struct indexed_device_attribute *)attr)->index;

	BUG_ON(index < 0 || index >= NUMBER_OF_COUNTERS);

	return index;
}

static struct scmi_hailo_noc_start_measure_a2p_filter *get_filter(struct device *dev, struct device_attribute *attr)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	return &pmu->params.start_params.filters[get_attribute_index(attr)];
}

#define NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(_name) \
	static INDEXED_DEVICE_ATTR_ADMIN_RW(_name, noc, 0); \
	static INDEXED_DEVICE_ATTR_ADMIN_RW(_name, noc, 1); \
	static INDEXED_DEVICE_ATTR_ADMIN_RW(_name, noc, 2); \
	static INDEXED_DEVICE_ATTR_ADMIN_RW(_name, noc, 3);

/* show attribute */
static ssize_t enabled_noc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	int index = get_attribute_index(attr);

	return sprintf(buf, "%d\n", pmu->params.active_counters[index]);
}
static ssize_t enabled_noc_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	int index = get_attribute_index(attr);
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	pmu->params.active_counters[index] = val;

	return count;
}
NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(enabled);

static int hailo_pmu_event_init(struct perf_event *event)
{
	u32 number_of_samples;
	u32 sample_time_us;
	bool running_mode;
	struct hailo_pmu *hailo_pmu = container_of(event->pmu, struct hailo_pmu, pmu);
	u64 config;

	/* Get the number of samples */
	config = event->attr.config;
	number_of_samples = FIELD_GET(HAILO_PMU_FORMAT_ATTR_NUM_SAMPLES_MASK, config);

	/* Get the sample interval time */
	config = event->attr.config;
	sample_time_us = FIELD_GET(HAILO_PMU_FORMAT_ATTR_SAMPLE_TIME_MASK, config);
	if (sample_time_us == 0)
		sample_time_us = DEFAULT_SAMPLE_TIME_US;

	/* Get running mode */
	config = event->attr.config1;
	if (FIELD_GET(HAILO_PMU_FORMAT_ATTR_RUNNING_MODE_MASK, config) == 1) {
		running_mode = true;
	} else {
		running_mode = false;
	}

	if (sample_time_us < MINIMUM_SAMPLE_TIME_US)
		return -EINVAL;

	hailo_pmu->params.sample_count_limit = number_of_samples;
	hailo_pmu->params.limit_samples = (number_of_samples > 0); // only limit if number_of_samples > 0
	hailo_pmu->params.start_params.sample_time_us = sample_time_us;
	hailo_pmu->params.start_params.is_freerunning = running_mode;

	return 0;
}

/* mode attribute */
static ssize_t mode_noc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);

	if (filter->total)
		return sprintf(buf, "total\n");
	return sprintf(buf, "filter\n");
}
static ssize_t mode_noc_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);

	/* sizeof - 1 to ignore last character (in case it is '\n') */
	if (strncmp(buf, "total", sizeof("total") - 1) == 0) {
		filter->total = true;
	} else if (strncmp(buf, "filter", sizeof("filter") - 1) == 0) {
		filter->total = false;
	} else {
		/* Only total and filter are supported */
		return -EINVAL;
	}

	return count;
}
NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(mode);

/* route_id_base attribute */
static ssize_t route_id_base_noc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%08x\n", filter->routeidbase);
}
static ssize_t route_id_base_noc_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u32 val;

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	filter->routeidbase = val;

	return count;
}
NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(route_id_base);

/* route_id_mask attribute */
static ssize_t route_id_mask_noc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%08x\n", filter->routeidmask);
}
static ssize_t route_id_mask_noc_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u32 val;

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	filter->routeidmask = val;

	return count;
}
NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(route_id_mask);

/* opcode attribute */
static ssize_t opcode_noc_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%02x\n", filter->opcode);
}
static ssize_t opcode_noc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (val >= (1 << 4))
		return -EINVAL;

	filter->opcode = val;

	return count;
}
NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(opcode);

/* length attribute */
static ssize_t length_noc_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%02x\n", filter->length);
}
static ssize_t length_noc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (val >= (1 << 4))
		return -EINVAL;

	filter->length = val;

	return count;
}
NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(length);

/* address_base attribute */
static ssize_t address_base_noc_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u64 address_base = filter->addrbase_low +
		((u64)filter->addrbase_high << 32);

	return sprintf(buf, "0x%016llx\n", address_base);
}
static ssize_t address_base_noc_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u64 address_base;

	if (kstrtou64(buf, 0, &address_base))
		return -EINVAL;

	if (address_base >= (1ULL << 36))
		return -EINVAL;

	filter->addrbase_low = address_base & 0xFFFFFFFF;
	filter->addrbase_high = (address_base >> 32) & 0xF;

	return count;
}
NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(address_base);

static ssize_t window_size_noc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%02x\n", filter->window_size);
}
static ssize_t window_size_noc_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (val >= (1 << 6))
		return -EINVAL;

	filter->window_size = val;

	return count;
}
NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(window_size);

static ssize_t urgency_noc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "%x\n", filter->urgency);
}
static ssize_t urgency_noc_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (val >= (1 << 3))
		return -EINVAL;

	filter->urgency = val;

	return count;
}
NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(urgency);

/* status attribute */
static ssize_t status_noc_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%x\n", filter->status);
}
static ssize_t status_noc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct scmi_hailo_noc_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (val >= (1 << 2))
		return -EINVAL;

	filter->status = val;

	return count;
}
NOC_COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(status);

#define NOC_COUNTER_ATTR_GROUP(_index) \
	struct attribute *hailo_pmu_noc_counter##_index##_attrs[] = { \
		&dev_attr_enabled_noc##_index.device_attribute.attr, \
		&dev_attr_mode_noc##_index.device_attribute.attr, \
		&dev_attr_route_id_base_noc##_index.device_attribute.attr, \
		&dev_attr_route_id_mask_noc##_index.device_attribute.attr, \
		&dev_attr_opcode_noc##_index.device_attribute.attr, \
		&dev_attr_length_noc##_index.device_attribute.attr, \
		&dev_attr_address_base_noc##_index.device_attribute.attr, \
		&dev_attr_window_size_noc##_index.device_attribute.attr, \
		&dev_attr_urgency_noc##_index.device_attribute.attr, \
		&dev_attr_status_noc##_index.device_attribute.attr, \
		NULL, \
	}; \
	static const struct attribute_group hailo_pmu_noc_counter##_index##_attr_group = { \
		.name = "counter"#_index, \
		.attrs = hailo_pmu_noc_counter##_index##_attrs, \
	};

static NOC_COUNTER_ATTR_GROUP(0);
static NOC_COUNTER_ATTR_GROUP(1);
static NOC_COUNTER_ATTR_GROUP(2);
static NOC_COUNTER_ATTR_GROUP(3);

/* CSM enabled */
static ssize_t csm_enabled_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	int value = pmu->params.start_params.csm_enabled;

	return sprintf(buf, "%d\n", value);
}
static ssize_t csm_enabled_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	pmu->params.start_params.csm_enabled = val;

	return count;
}
struct device_attribute dev_attr_csm_enabled =
	__ATTR(enabled, 0600, csm_enabled_show, csm_enabled_store);
struct attribute *hailo_pmu_csm_counter_attrs[] = {
	&dev_attr_csm_enabled.attr,
	NULL,
};
static const struct attribute_group hailo_pmu_csm_counter_attr_group = {
	.name = "csm",
	.attrs = hailo_pmu_csm_counter_attrs,
};


/* DSM RX enabled*/
static ssize_t dsm_rx_enabled_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	int value = pmu->params.start_params.dsm_rx_enabled;

	return sprintf(buf, "%d\n", value);
}
static ssize_t dsm_rx_enabled_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	pmu->params.start_params.dsm_rx_enabled = val;

	return count;
}
struct device_attribute dev_attr_dsm_rx_enabled =
	__ATTR(enabled, 0600, dsm_rx_enabled_show, dsm_rx_enabled_store);
struct attribute *hailo_pmu_dsm_rx_counter_attrs[] = {
	&dev_attr_dsm_rx_enabled.attr,
	NULL,
};
static const struct attribute_group hailo_pmu_dsm_rx_counter_attr_group = {
	.name = "dsm_rx",
	.attrs = hailo_pmu_dsm_rx_counter_attrs,
};


/* DSM TX enabled */
static ssize_t dsm_tx_enabled_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	int value = pmu->params.start_params.dsm_tx_enabled;

	return sprintf(buf, "%d\n", value);
}
static ssize_t dsm_tx_enabled_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	pmu->params.start_params.dsm_tx_enabled = val;

	return count;
}
struct device_attribute dev_attr_dsm_tx_enabled =
	__ATTR(enabled, 0600, dsm_tx_enabled_show, dsm_tx_enabled_store);
struct attribute *hailo_pmu_dsm_tx_counter_attrs[] = {
	&dev_attr_dsm_tx_enabled.attr,
	NULL,
};
static const struct attribute_group hailo_pmu_dsm_tx_counter_attr_group = {
	.name = "dsm_tx",
	.attrs = hailo_pmu_dsm_tx_counter_attrs,
};

/* All attributes */

static const struct attribute_group *hailo_pmu_attr_groups[] = {
	&hailo_pmu_events_attr_group,
	&hailo_pmu_format_attr_group,
	&hailo_pmu_noc_counter0_attr_group,
	&hailo_pmu_noc_counter1_attr_group,
	&hailo_pmu_noc_counter2_attr_group,
	&hailo_pmu_noc_counter3_attr_group,
	&hailo_pmu_csm_counter_attr_group,
	&hailo_pmu_dsm_rx_counter_attr_group,
	&hailo_pmu_dsm_tx_counter_attr_group,
	NULL,
};

static int hailo_pmu_register_perf(struct hailo_pmu *hailo_pmu)
{
	int ret;

	hailo_pmu->pmu = (struct pmu) {
		.module = THIS_MODULE,
		.attr_groups = hailo_pmu_attr_groups,
		.event_init = hailo_pmu_event_init,
		.add = hailo_pmu_add,
		.del = hailo_pmu_stop,
		.start = hailo_pmu_start,
		.stop = hailo_pmu_stop,
		.setup_aux = hailo_pmu_setup_aux,
		.free_aux = hailo_pmu_free_aux,
		.capabilities = PERF_PMU_CAP_EXCLUSIVE | PERF_PMU_CAP_AUX_OUTPUT | PERF_PMU_CAP_AUX_NO_SG,
		.task_ctx_nr = perf_invalid_context
	};

	ret = perf_pmu_register(&hailo_pmu->pmu, "hailo_noc_pmu", -1);

	return ret;
}

struct contexted_notifier_block {
	struct notifier_block notifier_block;
	void *context;
};

static int hailo_pmu_scmi_trigger_notifier(struct notifier_block *nb, unsigned long event, void *report)
{
	return NOTIFY_OK;
}

struct contexted_notifier_block trigger_nb = {
	.notifier_block = {
		.notifier_call = hailo_pmu_scmi_trigger_notifier,
	},
};

static int hailo_pmu_scmi_ended_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct hailo_pmu *hailo_pmu = ((struct contexted_notifier_block *)nb)->context;
	struct hailo_pmu_scmi_measurement_ended_work *measure_ended_work = kzalloc(sizeof(struct hailo_pmu_scmi_measurement_ended_work), GFP_KERNEL);

	INIT_WORK(&measure_ended_work->work, hailo_pmu_scmi_measurement_ended_work_handle);
	measure_ended_work->hailo_pmu = hailo_pmu;
	measure_ended_work->report = *((struct scmi_hailo_noc_measurement_ended_notification *)data);

	queue_work(hailo_pmu->scmi_wq, &measure_ended_work->work);

	return NOTIFY_OK;
}

struct contexted_notifier_block ended_nb = {
	.notifier_block = {
		.notifier_call = hailo_pmu_scmi_ended_notifier,
	},
};

static int hailo_pmu_register_notifiers(struct hailo_pmu *hailo_pmu)
{
	int ret;

	trigger_nb.context = hailo_pmu;
	ret = hailo_pmu->scmi_ops->register_notifier(SCMI_HAILO_NOC_MEASUREMENT_TRIGGER_NOTIFICATION_ID, &trigger_nb.notifier_block);
	if (ret)
		return ret;

	ended_nb.context = hailo_pmu;
	ret = hailo_pmu->scmi_ops->register_notifier(SCMI_HAILO_NOC_MEASUREMENT_ENDED_NOTIFICATION_ID, &ended_nb.notifier_block);
	if (ret)
		return ret;

	return ret;
}

static int hailo_pmu_probe(struct platform_device *pdev)
{
	int ret;
	struct hailo_pmu *hailo_pmu = NULL;
	const char *compat;

	/* Create driver */
	hailo_pmu = devm_kzalloc(&pdev->dev, sizeof(*hailo_pmu), GFP_KERNEL);
	if (hailo_pmu == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, hailo_pmu);

	/* Set default config */
	hailo_pmu->params.start_params = DEFAULT_START_PARAMS;
	hailo_pmu->params.active_counters[0] = true;

	/* Initialize spinlock */
	spin_lock_init(&hailo_pmu->pmu_lock);

	/* Get the SCMI Hailo protocol ops */
	hailo_pmu->scmi_ops = scmi_hailo_get_ops();
	if (IS_ERR(hailo_pmu->scmi_ops)) {
		return PTR_ERR(hailo_pmu->scmi_ops);
	}

	/* Register perf */
	ret = hailo_pmu_register_perf(hailo_pmu);
	if (ret) {
		pr_err("Failed to register perf: %d\n", ret);
		return ret;
	}

	ret = hailo_pmu_register_notifiers(hailo_pmu);
	if (ret) {
		pr_err("Failed to register notifiers: %d\n", ret);
		return ret;
	}

	if (of_property_read_string(pdev->dev.of_node, "compatible", &compat) != 0) {
		dev_err(&pdev->dev, "Failed to get device compatible\n");
		return -EINVAL;
	}

	if (strcmp(compat, "hailo,hailo15-noc-pmu") == 0) {
		hailo_pmu->sample_size = sizeof(struct noc_sample_h15);
	}
	else if (strcmp(compat, "hailo,hailo15l-noc-pmu") == 0) {
		hailo_pmu->sample_size = sizeof(struct noc_sample_h15l);
	}
	else if (strcmp(compat, "hailo,hailo12l") == 0) {
		dev_err(&pdev->dev, "noc tools doesn't work for hailo12l\n");
		return -EINVAL;
	}
	else {
		dev_err(&pdev->dev, "Invalid compatible\n");
		return -EINVAL;
	}

	/* Remap SCU shared buffer */
	hailo_pmu->scu_buf = devm_platform_ioremap_resource_byname(pdev, "noc_pmu_samples");
	if (IS_ERR(hailo_pmu->scu_buf)) {
		pr_err("Failed to remap SCU buffer: %ld\n", PTR_ERR(hailo_pmu->scu_buf));
		return PTR_ERR(hailo_pmu->scu_buf);
	}

	hailo_pmu->scmi_wq = alloc_ordered_workqueue("hailo_noc_scmi_wq", WQ_HIGHPRI);
	INIT_WORK(&hailo_pmu->scmi_start_work, hailo_pmu_scmi_start_work_handle);
	INIT_WORK(&hailo_pmu->scmi_stop_work, hailo_pmu_scmi_stop_work_handle);

	return ret;
}

static int hailo_pmu_remove(struct platform_device *pdev)
{
	struct hailo_pmu *hailo_pmu = platform_get_drvdata(pdev);

	flush_workqueue(hailo_pmu->scmi_wq);
	destroy_workqueue(hailo_pmu->scmi_wq);

	/* Unregister the Hailo PMU */
	perf_pmu_unregister(&hailo_pmu->pmu);

	return 0;
}

static const struct of_device_id hailo_noc_pmu_dt_ids[] = {
	{ .compatible = "hailo,hailo15-noc-pmu"},
	{ .compatible = "hailo,hailo15l-noc-pmu"},
	{ .compatible = "hailo,hailo12l-noc-pmu"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, hailo_noc_pmu_dt_ids);

static struct platform_driver hailo_noc_pmu_driver = {
	.driver = {
		.name   = "hailo-noc-pmu",
		.of_match_table = hailo_noc_pmu_dt_ids,
		.suppress_bind_attrs = true,
	},
	.probe  = hailo_pmu_probe,
	.remove = hailo_pmu_remove,
};

module_platform_driver(hailo_noc_pmu_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hailo PMU driver");
