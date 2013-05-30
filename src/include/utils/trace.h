#ifndef TRACE_H
#define TRACE_H

#include "postgres.h"

extern bool enable_iotracer;

typedef enum {
	EVENT_READIO_START = 'r',
	EVENT_READIO_FINISH = 'R',
	EVENT_WRITEIO_START = 'w',
	EVENT_WRITEIO_FINISH = 'W'
} trace_event_t;

void start_trace(void);
void stop_trace(void);
void trace_flush(void);
void __trace_event(trace_event_t event, uint64_t relid, uint64_t blocknum);

#define trace_event(event, relid, blocknum)		\
	{											\
	if (enable_iotracer)						\
		__trace_event(event, relid, blocknum);	\
	}

#endif // TRACE_H
