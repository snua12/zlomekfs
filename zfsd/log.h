/* Logging functions.
   Copyright (C) 2003, 2004 Josef Zlomek

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

#ifndef LOG_H
#define LOG_H

#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include "pthread.h"

#define abort() verbose_abort(__FILE__, __LINE__)

#ifdef ENABLE_CHECKING

/* Print which function we are in with additional information.  */
#define TRACE(format, ...) message (4, stderr,				       \
				    "TRACE %s() by %lu at %s:%d: " format "\n",\
				    __func__, (unsigned long) pthread_self (), \
				    __FILE__, __LINE__, ## __VA_ARGS__)

/* Print the function name and return value.  */
#define RETURN_INT(RETVAL)						\
  do {									\
    int32_t r = (int32_t) (RETVAL);					\
    TRACE ("return %" PRIi32, r);					\
    return r;								\
  } while (0)

#define RETURN_PTR(RETVAL)						\
  do {									\
    void *r = (RETVAL);							\
    TRACE ("return %p", r);						\
    return r;								\
  } while (0)

#define RETURN_BOOL(RETVAL)						\
  do {									\
    bool r = (RETVAL);							\
    TRACE ("return %d", r);						\
    return r;								\
  } while (0)

#define RETURN_VOID							\
  do {									\
    TRACE ("return");							\
    return;								\
  } while (0)

#else

#define TRACE(...)
#define RETURN_INT(RETVAL) return (RETVAL)
#define RETURN_PTR(RETVAL) return (RETVAL)
#define RETURN_BOOL(RETVAL) return (RETVAL)
#define RETURN_VOID return

#endif

/* Level of verbosity.  Higher number means more messages.  */
extern int verbose;

/* Thread ID of the main thread.  */
extern pthread_t main_thread;

/* Print message to F if LEVEL > VERBOSE.  */
extern void message (int level, FILE *f, char *format, ...) ATTRIBUTE_PRINTF_3;

/* Report an internal error.  */
extern void internal_error (char *format, ...) ATTRIBUTE_NORETURN;

/* Report an "Aborted" internal error.  */
extern void verbose_abort (const char *file, int line) ATTRIBUTE_NORETURN;

#endif
