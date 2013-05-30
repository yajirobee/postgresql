
#include "postgres.h"

#include <time.h>
#include "utils/trace.h"

typedef struct {
	uint64_t time;
	char event_type;
	uint64_t relid;
	uint64_t blocknum;
} trace_record_t;

// private variables
#define MAX_FILENAME_LEN 1024
static FILE *trace_file = NULL;
static bool atexit_registered = false;
static uint64_t basetime;

// on-memory trace buffer
#define TRACE_RECORD_REGION (1 << 30L)
#define NUM_TRACE_RECORD (TRACE_RECORD_REGION / sizeof(trace_record_t))
static trace_record_t *records = NULL;
static trace_record_t *cur_record = NULL;

void
start_trace(void)
{
	if (!enable_iotracer)
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

	records = (trace_record_t *) calloc(NUM_TRACE_RECORD, sizeof(trace_record_t));
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
	if (trace_file != NULL)
	{
		trace_record_t *record;
		Assert(cur_record >= records);
		for (record = records; record < cur_record; record++) {
			fprintf(trace_file,
					"%lx\t" // timestamp in nanosec
					"%c\t" // event type
					"%lx\t" // relid
					"%lx\n", // event type
					record->time,
					record->event_type,
					record->relid,
					record->blocknum);
		}
		cur_record = records;
	}
}

void
__trace_event(trace_event_t event, uint64_t relid, uint64_t blocknum)
{
	if (trace_file != NULL) {
		struct timespec tp;
		uint64_t time;

		Assert((uint64_t) cur_record - (uint64_t) records < TRACE_RECORD_REGION);

		if (clock_gettime(CLOCK_REALTIME, &tp) != 0)
			ereport(ERROR, (errmsg("clock_gettime(2) failed.")));

		time = tp.tv_sec * 1000000000L + tp.tv_nsec;
		time -= basetime;

		cur_record->time = time;
		cur_record->event_type = event;
		cur_record->relid = relid;
		cur_record->blocknum = blocknum;
		cur_record++;
		if ((uint64_t) cur_record - (uint64_t) records >= TRACE_RECORD_REGION)
			trace_flush();
	}
}
