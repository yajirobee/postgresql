
#include "postgres.h"

#include <time.h>
#include "utils/trace.h"

typedef struct {
	uint64_t time;
	char event_type;
	uint32_t nvalues;
} common_record_t;

typedef struct {
	common_record_t common;
	uint64_t val1, val2, val3, val4, val5;
} trace_record_t;

// private variables
#define MAX_FILENAME_LEN 1024
static FILE *trace_file = NULL;
static bool atexit_registered = false;
static uint64_t basetime;

// on-memory trace buffer
#define TRACE_RECORD_REGION (1 << 30L)
static trace_record_t *records = NULL;
static trace_record_t *cur_record = NULL;

void
start_trace(void)
{
	if (!enable_iotracer && !enable_buckettracer)
		return;

	if (trace_file != NULL)
		return;

	ereport(LOG, (errmsg("entered start_trace: pid = %d", getpid())));

	// set trace_file
	{
		const char *save_dir;
		char filename[MAX_FILENAME_LEN];
		char *cp;

		Assert(trace_file == NULL);
		bzero(filename, MAX_FILENAME_LEN * sizeof(char));
		if ((save_dir = getenv("PGTRACE")) == NULL)
			save_dir = ".";
		if (strlen(save_dir) > MAX_FILENAME_LEN)
			ereport(ERROR, (errmsg("too long save_dir: %s", save_dir)));
		strncpy(filename, save_dir, strlen(save_dir));
		cp = filename + strlen(save_dir);
		*cp = '/';
		cp ++;
		if (MAX_FILENAME_LEN - (cp - filename) < 20)
			ereport(ERROR, (errmsg("too long save_dir: %s", save_dir)));
		sprintf(cp, "trace_%d.log", getpid());
	    if ((trace_file = fopen(filename, "w+")) == NULL)
			ereport(ERROR, (errmsg("failed to open in write mode: %s", filename)));

		ereport(LOG, (errmsg("trace log file opened: %s", filename)));
	}

	// set atexit
	if (!atexit_registered)
	{
		atexit(stop_trace);
		atexit_registered = true;
	}

	// set basetime
	{
		struct timespec tp;
		if (clock_gettime(CLOCK_REALTIME, &tp) != 0)
			ereport(ERROR, (errmsg("clock_gettime(2) failed.")));
		basetime = tp.tv_sec * 1000000000L + tp.tv_nsec;
	}

	if ((records = malloc(TRACE_RECORD_REGION)) == NULL)
		ereport(ERROR, (errmsg("malloc(3) failed.")));
	cur_record = records;
}

void
stop_trace(void)
{
	if (trace_file != NULL)
	{
		int res;
		trace_flush();
		free(records);
		res = fflush(trace_file);
		if (res != 0)
			ereport(ERROR, (errmsg("[%d] trace_flush failed: errno = %d",
								   getpid(), res)));
		else
			ereport(DEBUG1, (errmsg("[%d] trace_flush succeeded",
									getpid())));
		res = fclose(trace_file);
		if (res != 0)
			ereport(ERROR, (errmsg("[%d] stop_trace failed: errno = %d",
								   getpid(), res)));
		else
			ereport(INFO, (errmsg("[%d] stop_trace succeeded",
								  getpid())));
		trace_file = NULL;
	}
}

void
trace_flush(void)
{
	int i;
	if (trace_file != NULL)
	{
		trace_record_t *record;
		Assert(cur_record >= records);
		for (record = records; record < cur_record; record = __next_record(record)) {
			fprintf(trace_file,
					"%lx\t" // timestamp in nanosec
					"%c", // event type
					record->common.time,
					record->common.event_type);
			if (record->common.nvalues >= 1)
				fprintf(trace_file, "\t%lx", record->val1);
			if (record->common.nvalues >= 2)
				fprintf(trace_file, "\t%lx", record->val2);
			if (record->common.nvalues >= 3)
				fprintf(trace_file, "\t%lx", record->val3);
			if (record->common.nvalues >= 4)
				fprintf(trace_file, "\t%lx", record->val4);
			if (record->common.nvalues >= 5)
				fprintf(trace_file, "\t%lx", record->val5);
			fprintf(trace_file, "\n");
		}
		cur_record = records;
	}
}

#define TRACE_RECORD_SIZE(nvalues)				\
	(sizeof(common_record_t) + sizeof(uint64_t) * nvalues)

static inline trace_record_t*
__next_record(trace_record_t* record)
{
	return (trace_record_t*)(((char*) record) + TRACE_RECORD_SIZE(record->common.nvalues));
}

static inline void
__trace_event_set_common(trace_event_t event, uint32_t nvalues)
{
	struct timespec tp;
	uint64_t time;

	if (clock_gettime(CLOCK_REALTIME, &tp) != 0)
		ereport(ERROR, (errmsg("clock_gettime(2) failed.")));

	time = tp.tv_sec * 1000000000L + tp.tv_nsec;
	time -= basetime;

	cur_record->common.time = time;
	cur_record->common.event_type = event;
	cur_record->common.nvalues = nvalues;
}

void
__trace_event1(trace_event_t event,
			   uint64_t val1)
{
	if (trace_file != NULL) {
		if ((uint64_t) cur_record - (uint64_t) records < (uint64_t) TRACE_RECORD_SIZE(1))
			trace_flush();
		Assert((uint64_t) cur_record - (uint64_t) records < TRACE_RECORD_REGION);

		__trace_event_set_common(event, 1);
		cur_record->val1 = val1;

		cur_record = __next_record(cur_record);
	}
}

void
__trace_event2(trace_event_t event,
			   uint64_t val1, uint64_t val2)
{
	if (trace_file != NULL) {
		if ((uint64_t) cur_record - (uint64_t) records < (uint64_t) TRACE_RECORD_SIZE(2))
			trace_flush();
		Assert((uint64_t) cur_record - (uint64_t) records < TRACE_RECORD_REGION);

		__trace_event_set_common(event, 1);
		cur_record->val1 = val1;
		cur_record->val2 = val2;

		cur_record = __next_record(cur_record);
	}
}

void
__trace_event3(trace_event_t event,
			   uint64_t val1, uint64_t val2, uint64_t val3)
{
	if (trace_file != NULL) {
		if ((uint64_t) cur_record - (uint64_t) records < (uint64_t) TRACE_RECORD_SIZE(3))
			trace_flush();
		Assert((uint64_t) cur_record - (uint64_t) records < TRACE_RECORD_REGION);

		__trace_event_set_common(event, 1);
		cur_record->val1 = val1;
		cur_record->val2 = val2;
		cur_record->val3 = val3;

		cur_record = __next_record(cur_record);
	}
}

void
__trace_event4(trace_event_t event,
			   uint64_t val1, uint64_t val2, uint64_t val3, uint64_t val4)
{
	if (trace_file != NULL) {
		if ((uint64_t) cur_record - (uint64_t) records < (uint64_t) TRACE_RECORD_SIZE(4))
			trace_flush();
		Assert((uint64_t) cur_record - (uint64_t) records < TRACE_RECORD_REGION);

		__trace_event_set_common(event, 1);
		cur_record->val1 = val1;
		cur_record->val2 = val2;
		cur_record->val3 = val3;
		cur_record->val4 = val4;

		cur_record = __next_record(cur_record);
	}
}

void
__trace_event5(trace_event_t event,
			   uint64_t val1, uint64_t val2, uint64_t val3, uint64_t val4, uint64_t val5)
{
	if (trace_file != NULL) {
		if ((uint64_t) cur_record - (uint64_t) records < (uint64_t) TRACE_RECORD_SIZE(5))
			trace_flush();
		Assert((uint64_t) cur_record - (uint64_t) records < TRACE_RECORD_REGION);

		__trace_event_set_common(event, 1);
		cur_record->val1 = val1;
		cur_record->val2 = val2;
		cur_record->val3 = val3;
		cur_record->val4 = val4;
		cur_record->val5 = val5;

		cur_record = __next_record(cur_record);
	}
}
