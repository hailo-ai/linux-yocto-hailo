#include "auxtrace.h"
#include "debug.h"
#include "event.h"
#include "evsel.h"
#include "session.h"

#include <linux/err.h>
#include <internal/lib.h>

#define MACHINE_PATH "/sys/devices/soc0/machine"
enum hailo_machines {
	HAILO15 = 0,
	HAILO15L,
	HAILO10H2,
};

static enum hailo_machines g_machine;

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

struct hailo_noc_process {
	u32 pmu_type;
	struct auxtrace auxtrace;
};

static int hailo_noc_process_event(struct perf_session *session __maybe_unused,
				   union perf_event *event __maybe_unused,
				   struct perf_sample *sample __maybe_unused,
				   struct perf_tool *tool __maybe_unused)

{
	return 0;
}

static int
hailo_noc_process_auxtrace_event(struct perf_session *session,
				 union perf_event *event,
				 struct perf_tool *tool __maybe_unused)
{
	size_t i = 0;
	int fd;
	size_t size = event->auxtrace.size;
	ssize_t read_size;
	void *data;
	uint32_t count;

	data = malloc(size);
	if (!data)
		return -ENOMEM;

	fd = perf_data__fd(session->data);
	if (fd < 0)
		return -EINVAL;

	/* Copy data */
	read_size = readn(fd, data, size);
	if (read_size < 0 || (size_t)read_size != size) {
		free(data);
		return -EINVAL;
	}

	if (g_machine == HAILO15) {
		count = size / sizeof(struct noc_sample_h15);
	} else if (g_machine == HAILO15L) {
		count = size / sizeof(struct noc_sample_h15l);
	} else {
		printf("invalid machine\n");
		return 1;
	}

	for (i = 0; i < count; ++i) {
		uint32_t duration_s, duration_us;
		char time_buf[20];
		uint32_t *noc_counters;
		uint32_t dsm_rx_counter, dsm_tx_counter, csm_counter;
		uint64_t timestamp;
		bool triggered, has_5_counters;

		if (g_machine == HAILO15) {
			struct noc_sample_h15 *sample =
				&((struct noc_sample_h15 *)data)[i];
			noc_counters = sample->noc_counters;
			dsm_rx_counter = sample->dsm_rx_counter;
			dsm_tx_counter = sample->dsm_tx_counter;
			csm_counter = sample->csm_counter;
			timestamp = sample->timestamp;
			triggered = sample->triggered;
			has_5_counters = false;
		} else if (g_machine == HAILO15L) {
			struct noc_sample_h15l *sample =
				&((struct noc_sample_h15l *)data)[i];
			noc_counters = sample->noc_counters;
			dsm_rx_counter = sample->dsm_rx_counter;
			dsm_tx_counter = sample->dsm_tx_counter;
			csm_counter = sample->csm_counter;
			timestamp = sample->timestamp;
			triggered = sample->triggered;
			// TODO: move to true when implementing DEV-2462
			has_5_counters = false;
		}

		duration_s = timestamp / 1000000000;
		duration_us = (timestamp % 1000000000) / 1000;
		snprintf(time_buf, sizeof(time_buf), "%lu.%06u", duration_s,
			 duration_us);

		printf("%-5d  %-13s  %-10u  %-10u  %-10u  %-10u  ", i, time_buf,
		       noc_counters[0], noc_counters[1], noc_counters[2],
		       noc_counters[3]);

		if (has_5_counters) {
			/* mercury has only 4 counters, so we don't print the fifth one */
			printf("%-7u  ", noc_counters[4]);
		}

		printf("%-10u  %-10u  %-10u  %s\n", dsm_rx_counter,
		       dsm_tx_counter, csm_counter,
		       triggered ? "<< triggered" : ".");
	}

	return 0;
}

static int hailo_noc_flush_events(struct perf_session *session __maybe_unused,
				  struct perf_tool *tool __maybe_unused)
{
	return 0;
}

static void hailo_noc_free_events(struct perf_session *session __maybe_unused)
{
}

static void hailo_noc_free(struct perf_session *session)
{
	struct hailo_noc_process *process = container_of(
		session->auxtrace, struct hailo_noc_process, auxtrace);
	free(process);
	session->auxtrace = NULL;
}

static bool hailo_noc_evsel_is_auxtrace(struct perf_session *session,
					struct evsel *evsel)
{
	struct hailo_noc_process *process = container_of(
		session->auxtrace, struct hailo_noc_process, auxtrace);
	return evsel->core.attr.type == process->pmu_type;
}

int hailo_noc_process_auxtrace_info(union perf_event *event,
				    struct perf_session *session)
{
	struct perf_record_auxtrace_info *auxtrace_info = &event->auxtrace_info;
	struct hailo_noc_process *process;
	FILE *fp;
	char buffer[15];

	fp = fopen(MACHINE_PATH, "r");
	if (!fp) {
		printf("Failed to open machine file\n");
		return 1;
	}

	if (fgets(buffer, sizeof(buffer), fp) == NULL) {
		printf("Failed to read machine file\n");
		fclose(fp);
		return 1;
	}
	fclose(fp);

	/* remove the '/n' char*/
	buffer[strcspn(buffer, "\n")] = 0;

	if (strcmp(buffer, "Hailo-15") == 0) {
		g_machine = HAILO15;
	} else if (strcmp(buffer, "Hailo-15l") == 0) {
		g_machine = HAILO15L;
	} else {
		printf("invalid machine: %s\n", buffer);
		return 1;
	}

	process = malloc(sizeof(*process));
	if (!process)
		return -ENOMEM;

	process->pmu_type = auxtrace_info->priv[0];

	process->auxtrace = (struct auxtrace){
		.process_event = hailo_noc_process_event,
		.process_auxtrace_event = hailo_noc_process_auxtrace_event,
		.flush_events = hailo_noc_flush_events,
		.free_events = hailo_noc_free_events,
		.free = hailo_noc_free,
		.evsel_is_auxtrace = hailo_noc_evsel_is_auxtrace,
	};

	session->auxtrace = &process->auxtrace;

	/* print the header only once */
	if (g_machine == HAILO15) {
		printf("index  time           counter0    counter1    counter2    counter3    dsm_rx      dsm_tx      csm         note\n");
	}
	if (g_machine == HAILO15L) {
		// TODO: When DEV-2462 is implemented, change the header to include 'total' counter
		// printf("index  time           counter0    counter1    counter2    counter3    total    dsm_rx      dsm_tx      csm         note\n");
		printf("index  time           counter0    counter1    counter2    counter3    dsm_rx      dsm_tx      csm         note\n");
	}

	return 0;
}