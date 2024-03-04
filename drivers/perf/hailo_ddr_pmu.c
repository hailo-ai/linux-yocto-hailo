#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/scmi_protocol.h>

#include <linux/vmalloc.h>
#include <linux/notifier.h>
#include <linux/minmax.h>
#include <linux/sched.h>
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

#define DEFAULT_PARAMS ((struct scmi_hailo_ddr_start_measure_a2p) { \
		.sample_time_us = DEFAULT_SAMPLE_TIME_US, \
		.after_trigger_percentage = 50, \
		.is_freerunning = false, \
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

#define __INDEXED_ATTR_RW_MODE(_name, _mode, _index) {				\
	.device_attribute = { 							\
		.attr	= { .name = __stringify(_name),				\
				.mode = VERIFY_OCTAL_PERMISSIONS(_mode) },	\
		.show	= _name##_show,						\
		.store	= _name##_store,					\
	}, 									\
	.index	= _index,							\
}

#define INDEXED_DEVICE_ATTR_ADMIN_RW(_name, _index) \
	struct indexed_device_attribute dev_attr_##_name##_index = __INDEXED_ATTR_RW_MODE(_name, 0600, _index)

struct ddr_sample {
	uint32_t counters[4];
	uint64_t timestamp;
	bool triggered;
} __packed;

enum action_id {
	ACTION_NONE,
	ACTION_START,
	ACTION_STOP
};

struct action {
	enum action_id action_id;
	struct scmi_hailo_ddr_start_measure_a2p action_param;
};

struct hailo_pmu {
	struct pmu pmu;
	struct perf_event *event;
	struct platform_device *pdev;
	struct perf_output_handle handle;
	struct ddr_sample *scu_buf;
	void *tmp_buf;
	size_t tmp_buf_size;
	void *aux_buf;
	size_t aux_buf_remaining_size;
	size_t remaining_samples;
	size_t total_bytes_written;
	spinlock_t lock;
	const struct scmi_hailo_ops *scmi_ops;
	bool active;

	struct action action;
	struct scmi_hailo_ddr_start_measure_a2p params;
	bool active_counters[4];

	/*
	 * busy is true as long as both of the following three conditions are met:
	 * 1. started was called by the user.
	 * 2. the last ended notification has not been completely handled.
	 * 3. stop was not called by the user.
	 */
	bool busy;
	unsigned int started_initiated_count;
	unsigned int ended_handled_count;
	/*
	 * stop_requested is true as long as the following two conditions are met:
	 * 1. stop was called by the user.
	 * 2. busy is true.
	 */
	bool stop_requested;
};

static void hailo_pmu_lock(struct hailo_pmu *hailo_pmu)
{
	spin_lock(&hailo_pmu->lock);
}

static void hailo_pmu_unlock(struct hailo_pmu *hailo_pmu)
{
	spin_unlock(&hailo_pmu->lock);
}

static inline bool hailo_pmu_started_enter(struct hailo_pmu *hailo_pmu, struct perf_event *event)
{
	bool allowed;

	hailo_pmu_lock(hailo_pmu);
	allowed = !hailo_pmu->busy;
	if (allowed) {
		hailo_pmu->started_initiated_count++;
		hailo_pmu->busy = true;
		event->hw.state &= ~PERF_HES_STOPPED;
	}
	hailo_pmu_unlock(hailo_pmu);

	return allowed;
}

static inline void hailo_pmu_started_failed(struct hailo_pmu *hailo_pmu, struct perf_event *event)
{
	hailo_pmu_lock(hailo_pmu);
	BUG_ON(!hailo_pmu->busy);
	hailo_pmu->busy = false;
	event->hw.state |= PERF_HES_STOPPED;
	hailo_pmu->started_initiated_count--;
	hailo_pmu_unlock(hailo_pmu);
}

/* hailo_pmu_started_leave is not needed */

static inline bool hailo_pmu_stop_enter(struct hailo_pmu *hailo_pmu)
{
	bool allowed = false;

	hailo_pmu_lock(hailo_pmu);
	if (hailo_pmu->busy && !hailo_pmu->stop_requested) {
		hailo_pmu->stop_requested = true;
		allowed = true;
	}

	return allowed;
}

static inline void hailo_pmu_stop_leave(struct hailo_pmu *hailo_pmu)
{
	if (hailo_pmu->stop_requested &&
	    hailo_pmu->ended_handled_count == hailo_pmu->started_initiated_count) {
		BUG_ON(!hailo_pmu->busy);
		hailo_pmu->busy = false;
		hailo_pmu->stop_requested = false;
	}
	hailo_pmu_unlock(hailo_pmu);
}

static inline bool hailo_pmu_ended_enter(struct hailo_pmu *hailo_pmu)
{
	hailo_pmu_lock(hailo_pmu);
	BUG_ON(hailo_pmu->ended_handled_count == hailo_pmu->started_initiated_count);
	BUG_ON(!hailo_pmu->busy);

	return !hailo_pmu->stop_requested;
}

/* Assumed to be under lock */
static inline void hailo_pmu_ended_continue(struct hailo_pmu *hailo_pmu)
{
	/* Increase started_initiated_count before hailo_pmu_ended_leave so that we won't have:
	   hailo_pmu->ended_handled_count == hailo_pmu->started_initiated_count*/
	hailo_pmu->started_initiated_count++;

	BUG_ON(hailo_pmu->ended_handled_count == hailo_pmu->started_initiated_count);
}

static inline void hailo_pmu_ended_leave(struct hailo_pmu *hailo_pmu)
{
	hailo_pmu->ended_handled_count++;
	if (hailo_pmu->stop_requested &&
	    hailo_pmu->ended_handled_count == hailo_pmu->started_initiated_count) {
		BUG_ON(!hailo_pmu->busy);
		hailo_pmu->busy = false;
		hailo_pmu->stop_requested = false;
	}
	hailo_pmu_unlock(hailo_pmu);
}

static inline void hailo_pmu_start_measure_failed(struct hailo_pmu *hailo_pmu)
{
	hailo_pmu_lock(hailo_pmu);
	hailo_pmu->started_initiated_count--;
	if (hailo_pmu->stop_requested &&
	    hailo_pmu->ended_handled_count == hailo_pmu->started_initiated_count) {
		/* Might happen if hailo_pmu_get_action returned ACTION_START
		   then user requested stop and then start_measurement failed */
		BUG_ON(!hailo_pmu->busy);
		hailo_pmu->busy = false;
		hailo_pmu->stop_requested = false;
	}
	hailo_pmu_unlock(hailo_pmu);
}

static void hailo_pmu_get_action(struct hailo_pmu *hailo_pmu, enum action_id *action_id, struct scmi_hailo_ddr_start_measure_a2p *params)
{
	hailo_pmu_lock(hailo_pmu);
	*action_id = hailo_pmu->action.action_id;
	if (params != NULL) {
		*params = hailo_pmu->action.action_param;
	}

	hailo_pmu->action.action_id = ACTION_NONE;
	hailo_pmu_unlock(hailo_pmu);
}

static DECLARE_WAIT_QUEUE_HEAD(hailo_pmu_action_queue);


/* Assumed to be under lock */
static void hailo_pmu_set_action(struct hailo_pmu *hailo_pmu, enum action_id action_id, struct scmi_hailo_ddr_start_measure_a2p *params)
{
	int i;

	hailo_pmu->action.action_id = action_id;
	if (params != NULL) {
		hailo_pmu->action.action_param = *params;
		for (i = 0; i < NUMBER_OF_COUNTERS; i++) {
			if (!hailo_pmu->active_counters[i]) {
				/* This configuration effectively does not let any packet pass */
				hailo_pmu->action.action_param.filters[i].opcode = 0;
				hailo_pmu->action.action_param.filters[i].total = 0;
			}
		}
	}

	wake_up_interruptible(&hailo_pmu_action_queue);
}


/* Assumed to be under lock */
static void hailo_pmu_stop_capturing(struct hailo_pmu *hailo_pmu)
{
	perf_aux_output_end(&hailo_pmu->handle, hailo_pmu->total_bytes_written);
	perf_event_update_userpage(hailo_pmu->event);

	hailo_pmu->remaining_samples = 0;
	hailo_pmu->aux_buf_remaining_size = 0;
	hailo_pmu->total_bytes_written = 0;
	hailo_pmu->aux_buf = NULL;
	hailo_pmu->event->hw.state |= PERF_HES_STOPPED;
	hailo_pmu->event = NULL;
}

static int hailo_pmu_commands_sender(void *data)
{
	int ret = 0;
	struct hailo_pmu *hailo_pmu = data;
	enum action_id action_id;
	struct scmi_hailo_ddr_start_measure_a2p params;
	bool measurement_was_running;

	hailo_pmu->active = true;

	while (hailo_pmu->active) {
		wait_event_interruptible(hailo_pmu_action_queue,
					 (hailo_pmu->action.action_id != ACTION_NONE) || !hailo_pmu->active);
		hailo_pmu_get_action(hailo_pmu, &action_id, &params);

		switch (action_id) {
			case ACTION_START:
				ret = hailo_pmu->scmi_ops->start_measure(&params);
				if (ret) {
					hailo_pmu_start_measure_failed(hailo_pmu);
					pr_err("Failed to start DDR bandwidth measurement on rc: %d\n", ret);
					break;
				}
				break;
			case ACTION_STOP:
				ret = hailo_pmu->scmi_ops->stop_measure(&measurement_was_running);
				if (ret) {
					pr_err("Failed to stop DDR bandwidth measurement on rc: %d\n", ret);
					break;
				}
				/*
				 * It doesn't matter if we stopped an active measurement,
				 * no further action is required.
				 */
				break;
			case ACTION_NONE:
				/* Do nothing */
				break;
			default:
				break;
		}
	}

	return 0;
}

static int hailo_pmu_add(struct perf_event *event, int flags)
{
	struct hailo_pmu *hailo_pmu = container_of(event->pmu, struct hailo_pmu, pmu);
	bool allowed = hailo_pmu_started_enter(hailo_pmu, event);

	if (!allowed) {
		goto Exit;
	}

	hailo_pmu->event = event;

	hailo_pmu->total_bytes_written = 0;
	hailo_pmu->aux_buf = perf_aux_output_begin(&hailo_pmu->handle, hailo_pmu->event);
	if (hailo_pmu->aux_buf == NULL) {
		pr_err("Failed to allocate aux buffer\n");
		hailo_pmu_started_failed(hailo_pmu, event);
		return -ENOMEM;
	}
	hailo_pmu->aux_buf_remaining_size = hailo_pmu->handle.size;
	if (hailo_pmu->remaining_samples == 0) {
		/* If user did not limit the number of samples,
		   use the size of aux_buf as the limit. */
		hailo_pmu->remaining_samples = hailo_pmu->aux_buf_remaining_size / sizeof(struct ddr_sample);
	}

	hailo_pmu_set_action(hailo_pmu, ACTION_START, &hailo_pmu->params);

Exit:
	return 0;
}

static void hailo_pmu_start(struct perf_event *event, int flags)
{
	hailo_pmu_add(event, flags);
}

static void hailo_pmu_stop(struct perf_event *event, int flags)
{
	struct hailo_pmu *hailo_pmu = container_of(event->pmu, struct hailo_pmu, pmu);
	bool allowed;

	allowed = hailo_pmu_stop_enter(hailo_pmu);
	if (!allowed) {
		goto Exit;
	}

	hailo_pmu_set_action(hailo_pmu, ACTION_STOP, NULL);
	hailo_pmu_stop_capturing(hailo_pmu);


Exit:
	hailo_pmu_stop_leave(hailo_pmu);
}

static void *hailo_pmu_setup_aux(struct perf_event *event, void **pages,
								 int nr_pages, bool overwrite)
{
	int i;
	struct page **page_list;
	void *flat_buf;

	/* Map the pages into a flat contiguous buffer */

	page_list = kcalloc(nr_pages, sizeof(*page_list), GFP_KERNEL);
	if (!page_list)
		return NULL;

	for (i = 0; i < nr_pages; i++)
		page_list[i] = virt_to_page(pages[i]);

	flat_buf = vmap(page_list, nr_pages, VM_MAP, PAGE_KERNEL);

	kfree(page_list);

	return flat_buf;
}

static void hailo_pmu_free_aux(void *aux)
{
	vunmap(aux);
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
	HAILO_PMU_EVENT_ATTR(hailo_ddr_bw, 0),
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

static struct scmi_hailo_ddr_start_measure_a2p_filter *get_filter(struct device *dev, struct device_attribute *attr)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	return &pmu->params.filters[get_attribute_index(attr)];
}

#define COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(_name) \
	static INDEXED_DEVICE_ATTR_ADMIN_RW(_name, 0); \
	static INDEXED_DEVICE_ATTR_ADMIN_RW(_name, 1); \
	static INDEXED_DEVICE_ATTR_ADMIN_RW(_name, 2); \
	static INDEXED_DEVICE_ATTR_ADMIN_RW(_name, 3);

/* show attribute */
static ssize_t enabled_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	int index = get_attribute_index(attr);

	return sprintf(buf, "%d\n", pmu->active_counters[index]);
}
static ssize_t enabled_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hailo_pmu *pmu = dev_get_drvdata(dev);
	int index = get_attribute_index(attr);
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	pmu->active_counters[index] = val;

	return count;
}
COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(enabled);

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

	hailo_pmu->remaining_samples = number_of_samples;
	hailo_pmu->params.sample_time_us = sample_time_us;
	hailo_pmu->params.is_freerunning = running_mode;

	return 0;
}

/* mode attribute */
static ssize_t mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);

	if (filter->total)
		return sprintf(buf, "total\n");
	return sprintf(buf, "filter\n");
}
static ssize_t mode_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);

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
COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(mode);

/* route_id_base attribute */
static ssize_t route_id_base_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%08x\n", filter->routeidbase);
}
static ssize_t route_id_base_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u32 val;

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	filter->routeidbase = val;

	return count;
}
COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(route_id_base);

/* route_id_mask attribute */
static ssize_t route_id_mask_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%08x\n", filter->routeidmask);
}
static ssize_t route_id_mask_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u32 val;

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	filter->routeidmask = val;

	return count;
}
COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(route_id_mask);

/* opcode attribute */
static ssize_t opcode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%02x\n", filter->opcode);
}
static ssize_t opcode_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (val >= (1 << 4))
		return -EINVAL;

	filter->opcode = val;

	return count;
}
COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(opcode);

/* length attribute */
static ssize_t length_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%02x\n", filter->length);
}
static ssize_t length_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (val >= (1 << 4))
		return -EINVAL;

	filter->length = val;

	return count;
}
COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(length);

/* address_base attribute */
static ssize_t address_base_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u64 address_base = filter->addrbase_low +
		((u64)filter->addrbase_high << 32);

	return sprintf(buf, "0x%016llx\n", address_base);
}
static ssize_t address_base_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u64 address_base;

	if (kstrtou64(buf, 0, &address_base))
		return -EINVAL;

	if (address_base >= (1ULL << 36))
		return -EINVAL;

	filter->addrbase_low = address_base & 0xFFFFFFFF;

	return count;
}
COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(address_base);

static ssize_t window_size_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "0x%02x\n", filter->window_size);
}
static ssize_t window_size_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (val >= (1 << 6))
		return -EINVAL;

	filter->window_size = val;

	return count;
}
COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(window_size);

static ssize_t urgency_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);

	return sprintf(buf, "%x\n", filter->urgency);
}
static ssize_t urgency_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct scmi_hailo_ddr_start_measure_a2p_filter *filter = get_filter(dev, attr);
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (val >= (1 << 3))
		return -EINVAL;

	filter->urgency = val;

	return count;
}
COUNTERS_INDEXED_DEVICE_ATTR_ADMIN_RW(urgency);

#define COUNTER_ATTR_GROUP(_index) \
	struct attribute *hailo_pmu_counter##_index##_attrs[] = { \
		&dev_attr_enabled##_index.device_attribute.attr, \
		&dev_attr_mode##_index.device_attribute.attr, \
		&dev_attr_route_id_base##_index.device_attribute.attr, \
		&dev_attr_route_id_mask##_index.device_attribute.attr, \
		&dev_attr_opcode##_index.device_attribute.attr, \
		&dev_attr_length##_index.device_attribute.attr, \
		&dev_attr_address_base##_index.device_attribute.attr, \
		&dev_attr_window_size##_index.device_attribute.attr, \
		&dev_attr_urgency##_index.device_attribute.attr, \
		NULL, \
	}; \
	static const struct attribute_group hailo_pmu_counter##_index##_attr_group = { \
		.name = "counter"#_index, \
		.attrs = hailo_pmu_counter##_index##_attrs, \
	};

static COUNTER_ATTR_GROUP(0);
static COUNTER_ATTR_GROUP(1);
static COUNTER_ATTR_GROUP(2);
static COUNTER_ATTR_GROUP(3);

/* All attributes */

static const struct attribute_group *hailo_pmu_attr_groups[] = {
	&hailo_pmu_events_attr_group,
	&hailo_pmu_format_attr_group,
	&hailo_pmu_counter0_attr_group,
	&hailo_pmu_counter1_attr_group,
	&hailo_pmu_counter2_attr_group,
	&hailo_pmu_counter3_attr_group,
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

	ret = perf_pmu_register(&hailo_pmu->pmu, "hailo_ddr_pmu", -1);

	return ret;
}

struct contexted_notifier_block {
	struct notifier_block notifier_block;
	void *context;
};

static int hailo_pmu_trigger_notifier(struct notifier_block *nb, unsigned long event, void *report)
{
	return NOTIFY_OK;
}

struct contexted_notifier_block trigger_nb = {
	.notifier_block = {
		.notifier_call = hailo_pmu_trigger_notifier,
	},
};

static int hailo_pmu_ended_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct scmi_hailo_ddr_measurement_ended_notification *report;
	struct hailo_pmu *hailo_pmu;
	bool allowed;

	size_t number_of_samples;
	size_t number_of_samples_fit_in_aux_buf;
	size_t samples_size;

	report = data;
	hailo_pmu = ((struct contexted_notifier_block *)nb)->context;

	allowed = hailo_pmu_ended_enter(hailo_pmu);
	if (!allowed) {
		/* Stop has been requested before we finished measuring */
		goto Exit;
	}

	number_of_samples = (report->sample_end_index - report->sample_start_index + 1);
	number_of_samples_fit_in_aux_buf = hailo_pmu->aux_buf_remaining_size / sizeof(struct ddr_sample);

	/*
	 * In case we received more samples than the user requested,
	 * we need to adjust the number of samples to write to the aux buffer
	 */
	if (number_of_samples > hailo_pmu->remaining_samples)
		number_of_samples = hailo_pmu->remaining_samples;

	/*
	 * In case we received more samples than the aux buffer can hold,
	 * we need to adjust the number of samples to write to the aux buffer
	 */
	if (number_of_samples > number_of_samples_fit_in_aux_buf) {
		number_of_samples = number_of_samples_fit_in_aux_buf;
	}

	samples_size = number_of_samples * sizeof(struct ddr_sample);

	/* Copy the samples to the aux buffer and update current state */
	memcpy(hailo_pmu->tmp_buf, &hailo_pmu->scu_buf[report->sample_start_index], hailo_pmu->tmp_buf_size);
	memcpy(hailo_pmu->aux_buf, hailo_pmu->tmp_buf, samples_size);

	hailo_pmu->remaining_samples -= number_of_samples;
	hailo_pmu->total_bytes_written += samples_size;
	hailo_pmu->aux_buf_remaining_size -= samples_size;
	hailo_pmu->aux_buf = ((uint8_t *)hailo_pmu->aux_buf) + samples_size;

	/* Check if we should stop capturing */
	if (hailo_pmu->remaining_samples == 0 || hailo_pmu->aux_buf_remaining_size < sizeof(struct ddr_sample))  {
		goto Exit;
	}

	/* Continue capturing */
	hailo_pmu_ended_continue(hailo_pmu);
	hailo_pmu_set_action(hailo_pmu, ACTION_START, &hailo_pmu->params);

Exit:
	hailo_pmu_ended_leave(hailo_pmu);
	return NOTIFY_OK;
}

struct contexted_notifier_block ended_nb = {
	.notifier_block = {
		.notifier_call = hailo_pmu_ended_notifier,
	},
};

static int hailo_pmu_register_notifiers(struct hailo_pmu *hailo_pmu)
{
	int ret;

	trigger_nb.context = hailo_pmu;
	ret = hailo_pmu->scmi_ops->register_notifier(SCMI_HAILO_DDR_MEASUREMENT_TRIGGER_NOTIFICATION_ID, &trigger_nb.notifier_block);
	if (ret)
		return ret;

	ended_nb.context = hailo_pmu;
	ret = hailo_pmu->scmi_ops->register_notifier(SCMI_HAILO_DDR_MEASUREMENT_ENDED_NOTIFICATION_ID, &ended_nb.notifier_block);
	if (ret)
		return ret;

	return ret;
}

static int hailo_pmu_probe(struct platform_device *pdev)
{
	int ret;
	struct hailo_pmu *hailo_pmu = NULL;
	struct task_struct *thread;
	struct resource *res;

	/* Create driver */
	hailo_pmu = devm_kzalloc(&pdev->dev, sizeof(*hailo_pmu), GFP_KERNEL);
	if (hailo_pmu == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, hailo_pmu);

	hailo_pmu->params = DEFAULT_PARAMS;
	hailo_pmu->active_counters[0] = true;

	/* Initialize spinlock */
	spin_lock_init(&hailo_pmu->lock);

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

	/* Remap SCU shared buffer */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ddr_pmu_samples");
	if (!res) {
		pr_err("Failed to get SCU buffer resource\n");
		return -ENODEV;
	}

	hailo_pmu->scu_buf = devm_memremap(&pdev->dev, res->start, resource_size(res), MEMREMAP_WT);
	hailo_pmu->tmp_buf = devm_kmalloc(&pdev->dev, resource_size(res), GFP_KERNEL);
	hailo_pmu->tmp_buf_size = resource_size(res);
	if (IS_ERR(hailo_pmu->scu_buf)) {
		pr_err("Failed to remap SCU buffer: %ld\n", PTR_ERR(hailo_pmu->scu_buf));
		return PTR_ERR(hailo_pmu->scu_buf);
	}

	hailo_pmu->active = true;
	thread = kthread_run(hailo_pmu_commands_sender, hailo_pmu, "hailo_pmu_commands_sender");
	if (IS_ERR(thread)) {
		pr_err("Failed to create kthread: %ld\n", PTR_ERR(thread));
		hailo_pmu->active = false;
		return PTR_ERR(thread);
	}

	return ret;
}

static int hailo_pmu_remove(struct platform_device *pdev)
{
	struct hailo_pmu *hailo_pmu = platform_get_drvdata(pdev);

	hailo_pmu->active = false;

	/* Unregister the Hailo PMU */
	perf_pmu_unregister(&hailo_pmu->pmu);

	return 0;
}

static const struct of_device_id hailo_ddr_pmu_dt_ids[] = {
	{ .compatible = "hailo,ddr-pmu"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, hailo_ddr_pmu_dt_ids);

static struct platform_driver hailo_ddr_pmu_driver = {
	.driver         = {
		.name   = "hailo-ddr-pmu",
		.of_match_table = hailo_ddr_pmu_dt_ids,
		.suppress_bind_attrs = true,
	},
	.probe          = hailo_pmu_probe,
	.remove         = hailo_pmu_remove,
};

module_platform_driver(hailo_ddr_pmu_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Hailo PMU driver");
