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
   or download it from http://www.gnu.org/licenses/gpl.html
   */

#include "system.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "log.h"

/* Level of verbosity.  Higher number means more messages.  */
int verbose = 2;

/* Local function prototypes.  */
static void internal_error(char *format, ...);

/* Print message to F if LEVEL > VERBOSE.  */
void
message(int level, FILE *f, char *format, ...)
{
  va_list va;
  
  if (verbose < level)
    return;
  
  va_start(va, format);
  vfprintf(f, format, va);
  fflush(f);
  va_end(va);
}

/* Print the internal error message and exit.  */
static void
internal_error(char *format, ...)
{
  va_list va;

  va_start(va, format);
  fprintf(stderr, "\nInternal error");
  vfprintf(stderr, format, va);
  fprintf(stderr, "\n");
  va_end(va);

  exit (EXIT_FAILURE);
}

/* Report an internal error.  */
void
verbose_abort(const char *file, int line)
{
  internal_error(" at %s:%d: Aborted", file, line);
}

/* Report the signal caught.  */
void
fatal_sighandler(int signum)
{
  internal_error(": %s", strsignal(signum));
}
