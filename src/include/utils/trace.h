#ifndef TRACE_H
#define TRACE_H

#include "postgres.h"

extern bool enable_iotracer, enable_buckettracer;

typedef enum {
	EVENT_READIO_START = 'r',
	EVENT_READIO_FINISH = 'R',
	EVENT_WRITEIO_START = 'w',
	EVENT_WRITEIO_FINISH = 'W',
	EVENT_BUCKET_SCAN = "S"
} trace_event_t;

extern void start_trace(void);
extern void stop_trace(void);
extern void trace_flush(void);
extern void __trace_event(trace_event_t event, uint32_t nvalues, uint64_t values[])

#define trace_event1(flg, event, val1)			\
	{											\
	if (flg)									\
	{											\
		uint64_t values[1] = {val1};			\
		__trace_event(event, 1, values);		\
	}

#define trace_event2(flg, event, val1, val2)	\
	{											\
	if (flg)									\
	{											\
		uint64_t values[2] = {val1, val2};		\
		__trace_event(event, 2, values);		\
	}

#define trace_event3(flg, event, val1, val2, val3)	\
	{												\
	if (flg)										\
	{												\
		uint64_t values[3] = {val1, val2, val3};	\
		__trace_event(event, 3, values);			\
	}

#define trace_event3(flg, event, val1, val2, val3, val4)	\
	{														\
	if (flg)												\
	{														\
		uint64_t values[4] = {val1, val2, val3, val4};		\
		__trace_event(event, 4, values);					\
	}

#define trace_event3(flg, event, val1, val2, val3, val4, val5)	\
	{															\
	if (flg)													\
	{															\
		uint64_t values[5] = {val1, val2, val3, val4, val5};	\
		__trace_event(event, 5, values);						\
	}

#endif // TRACE_H
