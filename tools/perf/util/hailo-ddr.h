#ifndef INCLUDE__PERF_HAILO_DDR_H__
#define INCLUDE__PERF_HAILO_DDR_H__

#include "auxtrace.h"

#define HAILO_DDR_PMU ("hailo_ddr_pmu")

struct auxtrace_record *hailo_ddr_recording_init(int *err, struct perf_pmu *hailo_ddr_pmu);
int hailo_ddr_process_auxtrace_info(union perf_event *event, struct perf_session *session);

#endif /* INCLUDE__PERF_HAILO_DDR_H__ */
