/* Logging functions.
   Copyright (C) 2003 Josef Zlomek

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

#include "system.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "pthread.h"
#include "log.h"

/* Level of verbosity.  Higher number means more messages.  */
int verbose = 2;

/* Thread ID of the main thread.  */
pthread_t main_thread;

/* Print message to F if LEVEL > VERBOSE.  */
void
message (int level, FILE * f, char *format, ...)
{
  va_list va;

  if (verbose < level)
    return;

  va_start (va, format);
  vfprintf (f, format, va);
  fflush (f);
  va_end (va);
}

/* Print the internal error message and exit.  */
void
internal_error (char *format, ...)
{
  va_list va;
#ifdef ENABLE_CHECKING
  int pid;
#endif

  va_start (va, format);
  fprintf (stderr, "\nInternal error: ");
  vfprintf (stderr, format, va);
  fprintf (stderr, "\n");
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

/* Report an "Aborted" internal error.  */
void
verbose_abort (const char *file, int line)
{
  internal_error ("Aborted, at %s:%d", file, line);
}
