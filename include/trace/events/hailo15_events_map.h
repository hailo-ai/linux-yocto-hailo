
#ifndef _TRACE_HAILO_EVENTS_MAP_H
#define _TRACE_HAILO_EVENTS_MAP_H

#define HAILO15_EVENT_LIST_ARRAY(X) \
	X(HAILO15_EVENT_START_STREAMING) \
	X(HAILO15_EVENT_STOP_STREAMING) \
	X(HAILO15_EVENT_EMPTY_QUEUE) \
	X(HAILO15_EVENT_FULL_QUEUE)

#define HAILO15_EVENT_ENUM(name) name,
#define HAILO15_EVENT_STRINGIFY(name) #name,

enum hailo15_event_id {
	HAILO15_EVENT_LIST_ARRAY(HAILO15_EVENT_ENUM)
	NUM_HAILO15_EVENTS
};

const char* hailo15_get_event_name(enum hailo15_event_id event_id);

#define HAILO15_EVENT_NAME(event_id) hailo15_get_event_name(event_id)

#endif