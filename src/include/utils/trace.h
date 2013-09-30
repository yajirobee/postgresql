#ifndef TRACE_H
#define TRACE_H

#include "postgres.h"

extern bool enable_iotracer, enable_buckettracer;

typedef enum {
	EVENT_READIO_START = 'r',
	EVENT_READIO_FINISH = 'R',
	EVENT_WRITEIO_START = 'w',
	EVENT_WRITEIO_FINISH = 'W',
	EVENT_BUCKET_CREATE = "C",
	EVENT_BUCKET_SCAN = "S"
} trace_event_t;

void start_trace(void);
void stop_trace(void);
void trace_flush(void);
void __trace_event1(trace_event_t event,
					uint64_t val1);
void __trace_event2(trace_event_t event,
					uint64_t val1, uint64_t val2);
void __trace_event3(trace_event_t event,
					uint64_t val1, uint64_t val2, uint64_t val3);
void __trace_event4(trace_event_t event,
					uint64_t val1, uint64_t val2, uint64_t val3, uint64_t val4);
void __trace_event5(trace_event_t event,
					uint64_t val1, uint64_t val2, uint64_t val3, uint64_t val4, uint64_t val5);

#define iotrace_event(event, relid, blocknum)	\
	{											\
	if (enable_iotracer)						\
		__trace_event2(event, relid, blocknum);	\
	}

#define buckettrace_event(event, val1, val2)	\
	{											\
	if (enable_buckettracer)					\
		__trace_event2(event, val1, val2);		\
	}

#endif // TRACE_H
