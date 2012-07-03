#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h> 
#include "syscall_collector.h"

typedef struct syscall_entry_def
{
	uint32_t count;
	uint64_t total_time_usec;
	uint64_t last_start_time_usec;
	uint64_t worst_time_usec;
	
} syscall_entry;

syscall_entry entries[SYSCALL_OP_MAX];
#define SYSCALL_ENTRY_COUNT (sizeof(entries) / sizeof(syscall_entry))

void collector_init()
{
	int i;
	for (i = 0; i < SYSCALL_OP_MAX; ++i)
	{
		entries[i].count = 0;
		entries[i].total_time_usec = 0;
		entries[i].last_start_time_usec =0;
		entries[i].worst_time_usec = 0;
	}
}

const char * syscall_name[] =
	{
		"open",
		"close",
		"write",
		"mkdir",
		"rmdir",
		"unlink",
		"max"
	};

static uint64_t time64()
{
        struct timeval tv;
        uint64_t rv;
        gettimeofday (&tv, NULL);
        rv = tv.tv_sec;
        rv *= 1000000;
        rv += tv.tv_usec;
        return rv;
}

void collector_print()
{
	int i;
	for (i = 0; i < SYSCALL_OP_MAX; ++i)
	{
		if (entries[i].count == 0)
			continue;

		printf("%10s number of entryes: %10"PRIu32
			" time avg: %10"PRIu64".%"PRIu64"\n",
			syscall_name[i],
			entries[i].count,
			entries[i].total_time_usec / entries[i].count,
			entries[i].total_time_usec % entries[i].count
			);
	}
}

void collect(syscall_op op, syscall_state state)
{
	if (op >= SYSCALL_OP_MAX)
		return;

	uint64_t now = time64();
	if (state == SYSCALL_STATE_BEGIN)
	{
		entries[op].last_start_time_usec = now;
		return;
	}

	uint64_t took = now - entries[op].last_start_time_usec;
	
	if (entries[op].worst_time_usec == 0)
	{
		entries[op].worst_time_usec = took;
	}

	if (entries[op].worst_time_usec < took)
	{
		entries[op].worst_time_usec = took;
	}

	entries[op].total_time_usec += took;
	entries[op].count += 1;
}

