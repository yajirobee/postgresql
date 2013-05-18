#ifndef TRACE_H
#define TRACE_H

#include "postgres.h"

extern bool enable_iotracer;

typedef enum {
	EVENT_IO_START = 'S',
	EVENT_IO_FINISH = 'F'
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
