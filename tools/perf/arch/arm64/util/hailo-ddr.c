#include <errno.h>
#include <time.h>
#include <internal/lib.h> // page_size

#include <stdint.h>

#include "../../../util/hailo-ddr.h"

#include "../../../util/auxtrace.h"
#include "../../../util/debug.h"
#include "../../../util/evlist.h"
#include "../../../util/record.h"
#include "../../../util/session.h"

#define KiB(x) ((x) * 1024)
#define MiB(x) ((x) * 1024 * 1024)
#define HAILO_DDR_DATA_ALIGNMENT	(1)
#define HAILO_DDR_AUXTRACE_PRIV_SIZE	sizeof(uint64_t)

static int hailo_ddr_recording_options(struct auxtrace_record *record,
				       struct evlist *evlist,
				       struct record_opts *opts)
{
	struct perf_pmu *pmu = record->pmu;
	struct evsel *hailo_ddr_evsel = NULL;
	struct evsel *evsel;

	if (!perf_event_paranoid_check(-1))
		return -EPERM;

	record->evlist = evlist;
	
	evlist__for_each_entry(evlist, evsel) {
		if (evsel->core.attr.type == pmu->type) {
			if (hailo_ddr_evsel) {
				pr_err("Only one event allowed on a Hailo DDR PMU\n");
				return -EINVAL;
			}

			evsel->core.attr.sample_period = 1;
			evsel->core.attr.freq = 0;
			evsel->core.system_wide = 1;
			opts->full_auxtrace = true;
			hailo_ddr_evsel = evsel;
		}
	}

	if (opts->auxtrace_mmap_pages == 0) {
		opts->auxtrace_mmap_pages = MiB(32) / page_size;
	}

	/*
	 * To obtain the auxtrace buffer file descriptor, the auxtrace event
	 * must come first.
	 */
	evlist__to_front(evlist, hailo_ddr_evsel);
	evsel__set_sample_bit(hailo_ddr_evsel, TIME);

	return 0;
}

static size_t hailo_ddr_info_priv_size(struct auxtrace_record *itr __maybe_unused,
				       struct evlist *evlist __maybe_unused)
{
	return HAILO_DDR_AUXTRACE_PRIV_SIZE;
}

static int hailo_ddr_info_fill(struct auxtrace_record *record,
			       struct perf_session *session,
			       struct perf_record_auxtrace_info *auxtrace_info,
			       size_t priv_size)
{
	struct perf_pmu *pmu = record->pmu;

	if (priv_size != HAILO_DDR_AUXTRACE_PRIV_SIZE)
		return -EINVAL;

	if (!session->evlist->core.nr_mmaps)
		return -EINVAL;

	auxtrace_info->type = PERF_AUXTRACE_HAILO_DDR;
	auxtrace_info->priv[0] = pmu->type;

	return 0;
}

static void hailo_ddr_record_free(struct auxtrace_record *record)
{
	free(record);
}

static u64 hailo_ddr_reference(struct auxtrace_record *record __maybe_unused)
{
	return 0;
}

static int hailo_ddr__read_finish(struct auxtrace_record *itr, int idx)
{
	return auxtrace_record__read_finish(itr, idx);
}

struct auxtrace_record *hailo_ddr_recording_init(int *err, struct perf_pmu *hailo_ddr_pmu)
{
	struct auxtrace_record *record;

	if (!hailo_ddr_pmu) {
		*err = -ENODEV;
		return NULL;
	}

	record = malloc(sizeof(*record));
	if (record == NULL) {
		*err = -ENOMEM;
		return NULL;
	}

	record->recording_options = hailo_ddr_recording_options;
	record->info_priv_size = hailo_ddr_info_priv_size;
	record->info_fill = hailo_ddr_info_fill;
	record->free = hailo_ddr_record_free;
	record->reference = hailo_ddr_reference;
	record->read_finish = hailo_ddr__read_finish;
	record->alignment = HAILO_DDR_DATA_ALIGNMENT;
	record->pmu = hailo_ddr_pmu;
	*err = 0;
	return record;
}
