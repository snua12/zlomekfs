/*! \file
    \brief Logging functions.  */

/* Copyright (C) 2003, 2004 Josef Zlomek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   ZFS; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html */

#include "log.h"
#include "system.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#include "node.h"
#include "syplog.h"
#include "pthread.h"


struct logger_def syplogger;

void zfs_openlog(int  argc, char ** argv)
{
  syp_error ret_code = open_log (&syplogger, "STILL UNDEFINED", argc, argv);
  if (ret_code != NOERR)
  {
    printf ("Bad params for logger initialization %d: %s\n", ret_code,
            syp_error_to_string (ret_code));

    ret_code = open_log (&syplogger, "STILL_UNDEFINED", 0, NULL);
  }

  if (ret_code != NOERR)
  {
    printf ("could not initialize logger %d: %s\n", ret_code,
            syp_error_to_string (ret_code));
  }
}

void update_node_name (void)
{
  syp_error ret_code = NOERR;
  if (node_name.str != NULL)
    ret_code = set_node_name ( &syplogger, node_name.str);
  else
    ret_code = set_node_name (&syplogger, "STILL UNDEFINED");

  if (ret_code != NOERR)
    message (LOG_WARNING, NULL, "could not set node_name %d: %s\n", ret_code,
             syp_error_to_string (ret_code));
}

void zfs_closelog(void)
{
  close_log (&syplogger);
}


/*! Print the internal error message and exit.  */
void
internal_error (const char *format, ...)
{
  va_list va;
#ifdef ENABLE_CHECKING
  int pid;
#endif

  va_start (va, format);

#ifdef USE_SYSLOG
  syslog(LOG_EMERG,"Zfsd terminating due to internal error...");
  vsyslog(LOG_EMERG,format,va);
#else
  fprintf (stderr, "\nInternal error: ");
  vfprintf (stderr, format, va);
  fprintf (stderr, "\n");
#endif

  va_end (va);

#ifdef ENABLE_CHECKING
  pid = fork ();
  if (pid == 0)
    {
      char buf[24];

      sprintf (buf, "%d", getppid ());
      execlp ("gdb", "gdb", "zfsd", buf, NULL);
    }
  else if (pid > 0)
    {
      waitpid (pid, NULL, 0);
    }
#endif

  /* Exit because in case of failure the state of data structures may be
     inconsistent.  */
  exit (EXIT_FAILURE);
}

/*! Report an "Aborted" internal error.  */
void
verbose_abort (const char *file, int line)
{
  internal_error ("Aborted by %lu, at %s:%d", (unsigned long) pthread_self (),
		  file, line);
}
