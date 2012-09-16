/*! \file \brief Logging functions.  */

/* Copyright (C) 2003, 2004, 2010 Josef Zlomek, Rastislav Wartiak

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

#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <syslog.h>
#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#include "node.h"
#include "configuration.h"
#include "syplog.h"

#include "pthread-wrapper.h"

struct logger_def syplogger;

void zfs_openlog(int argc, const char **argv)
{
	int saved_opterr = opterr;
	opterr = 0;

	syp_error ret_code = open_log(&syplogger, "UNDEF", argc, argv);
	if (ret_code != NOERR)
	{
		printf("Bad params for logger initialization %d: %s\n", ret_code,
			   syp_error_to_string(ret_code));

		// fallback logger initialization
		ret_code = open_log(&syplogger, "UNDEF", 0, NULL);
		if (ret_code != NOERR)
		{
			printf("could not initialize logger %d: %s\n", ret_code,
				   syp_error_to_string(ret_code));
		}
	}

	opterr = saved_opterr;
}

void update_node_name(void)
{
	syp_error ret_code = NOERR;
	if (zfs_config.this_node.node_name.str != NULL)
		ret_code = set_node_name(&syplogger, zfs_config.this_node.node_name.str);
	else
		ret_code = set_node_name(&syplogger, "UNDEF");

	if (ret_code != NOERR)
		message(LOG_WARNING, FACILITY_LOG, "could not set node_name %d: %s\n",
				ret_code, syp_error_to_string(ret_code));
}

void zfs_closelog(void)
{
	close_log(&syplogger);
}

#ifdef HAVE_EXECINFO_H
#define HAVE_SHOW_STACKFRAME 1
/*! Print stack */
static void show_stackframe(void)
{
	void *trace[16];
	char **messages = (char **)NULL;
	int i, trace_size = 0;

	trace_size = backtrace(trace, 16);
	messages = backtrace_symbols(trace, trace_size);
	message(LOG_EMERG, FACILITY_ALL, "[bt] Execution path:\n");
	for (i = 0; i < trace_size; ++i)
		message(LOG_EMERG, FACILITY_ALL, "[bt] %s\n", messages[i]);
	free(messages);
}
#endif

/*! Print the internal error message and exit.  */
void internal_error(const char *format, ...)
{
	va_list va;
	char msg[1024];
#ifdef ENABLE_CHECKING
	int pid = getpid();
#endif

	va_start(va, format);


	message(LOG_EMERG, FACILITY_ALL,
			"Zfsd terminating due to internal error...\n");

	va_start(va, format);
	vsnprintf(msg, 1024, format, va);
	va_end(va);

	message(LOG_EMERG, FACILITY_ALL, msg);
#ifdef HAVE_SHOW_STACKFRAME
	show_stackframe();
#endif
	sleep(2);

#ifdef ENABLE_CHECKING
	kill(pid, SIGABRT);
#endif

	/* Exit because in case of failure the state of data structures may be
	   inconsistent.  */
	exit(EXIT_FAILURE);
}

/*! Report an "Aborted" internal error.  */
void verbose_abort(const char *file, int line)
{
	internal_error("Aborted by %p, at %s:%d\n", pthread_self(),
				   file, line);
}
