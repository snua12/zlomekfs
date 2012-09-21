/*! \file syscall_collector/syscall_collector.c
 *  \brief Collect statistic for some file syscalls
 *  \author Ales Snuparek
 *
 *
 * This test creates a "searching in depth" tree of directories that on
 * the leaf level include files. Then remove asresarovou structure.
 * For the following operations: open, read, write, close mkdir and
 * rmdir is measured by the mean duration of these operations.
 * This test uses posix API.
 */

/* Copyright (C) 2008, 2012 Ales Snuparek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/time.h> 
#include <math.h>
#include "syscall_collector.h"

#ifdef __CYGWIN__
#include <windows.h>
/*! \brief returns system time */
static uint64_t time64()
{
	LARGE_INTEGER li;
	if(!QueryPerformanceFrequency(&li))
	{
		printf("QueryPerformanceFrequency failed!\n");
	}

	uint64_t pc_freq = li.QuadPart;

	QueryPerformanceCounter(&li);
	return (li.QuadPart * 1000 * 1000) / pc_freq;
}
#else
/*! \brief returns system time */
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
#endif

/*! \brief structure for collecting syscall statistic */
typedef struct syscall_entry_def
{
	uint32_t count; /**< count of performed syscalls */ 
	uint64_t total_time_usec; /**< total time spend in syscall */
	uint64_t total_time_square_usec; /**< total time spend in syscall ^ 2 */
	uint64_t last_start_time_usec; /**< time spend in last syscall */
	uint64_t worst_time_usec; /**< worst time spend in syscall */
	
} syscall_entry;

/*! \brief array for collecting syscall statistic */
syscall_entry entries[SYSCALL_OP_MAX];

/*! \brief number of syscall */
#define SYSCALL_ENTRY_COUNT (sizeof(entries) / sizeof(syscall_entry))

/*! \brief initializes syscall collector internal structures */
void collector_init()
{
	int i;
	for (i = 0; i < SYSCALL_OP_MAX; ++i)
	{
		entries[i].count = 0;
		entries[i].total_time_usec = 0;
		entries[i].total_time_square_usec = 0;
		entries[i].last_start_time_usec =0;
		entries[i].worst_time_usec = 0;
	}
}

/*! \brief conversion array from enum to syscall string name */
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

/*! \brief print syscall collector results */
void collector_print()
{
	int i;
	for (i = 0; i < SYSCALL_OP_MAX; ++i)
	{
		if (entries[i].count == 0)
			continue;

		uint64_t count = entries[i].count;
				double std_dev = 0;
		if (count > 0)
		{
			// compute avg
			double avg = entries[i].total_time_usec / entries[i].count;
			// compute standard deviation
			 std_dev = sqrt((entries[i].total_time_square_usec - (count * avg * avg)) / (count - 1));
		}

		printf("%10s number of entryes: %10"PRIu32
			" time avg: %10"PRIu64
			" std dev: %10.0lf""\n",
			syscall_name[i],
			entries[i].count,
			(entries[i].total_time_usec) / (entries[i].count),
			std_dev
			);
	}
}

/*! \brief call before and after syscall inorder to collect syscall time consumption */
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
	entries[op].total_time_square_usec += (took * took);
	entries[op].count += 1;
}

