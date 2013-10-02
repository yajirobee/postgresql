
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
	uint64_t values[5];
} trace_record_t;

// on-memory trace buffer
#define TRACE_RECORD_REGION (1 << 30L)

// private variables
#define MAX_FILENAME_LEN 1024
static FILE *trace_file = NULL;
static bool atexit_registered = false;
static uint64_t basetime;
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
	int res;

	if (NULL == trace_file)
		return;

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

void
trace_flush(void)
{
	int i;
	trace_record_t *record;

	if (NULL == trace_file)
		return;

	Assert(cur_record >= records);
	for (record = records; record < cur_record; record = __next_record(record)) {
		fprintf(trace_file,
				"%lx\t" // timestamp in nanosec
				"%c", // event type
				record->common.time,
				record->common.event_type);
		for (i = 0; i < record->common.nvalues; i++)
			fprintf(trace_file, "\t%lx", record->values[i]);
		fprintf(trace_file, "\n");
	}
	cur_record = records;
}

#define TRACE_RECORD_SIZE(nvalues)				\
	(sizeof(common_record_t) + sizeof(uint64_t) * nvalues)

static inline trace_record_t*
__next_record(trace_record_t* record)
{
	return (trace_record_t*)(((char*) record) + TRACE_RECORD_SIZE(record->common.nvalues));
}

void
__trace_event(trace_event_t event, uint32_t nvalues, uint64_t values[])
{
	int i;
	struct timespec tp;
	uint64_t time;

	if (NULL == trace_file)
		return;

	if ((uint64_t) cur_record - (uint64_t) records < (uint64_t) TRACE_RECORD_SIZE(nvalues))
		trace_flush();
	Assert((uint64_t) cur_record - (uint64_t) records < TRACE_RECORD_REGION);

	if (clock_gettime(CLOCK_REALTIME, &tp) != 0)
		ereport(ERROR, (errmsg("clock_gettime(2) failed.")));
	time = tp.tv_sec * 1000000000L + tp.tv_nsec;
	time -= basetime;

	cur_record->common.time = time;
	cur_record->common.event_type = event;
	cur_record->common.nvalues = nvalues;

	for (i = 0; i < nvalues; i++)
		cur_record->values[i] = values[i];

	cur_record = __next_record(cur_record);
}
