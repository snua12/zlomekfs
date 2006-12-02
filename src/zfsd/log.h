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

#ifndef LOG_H
#define LOG_H

#include "system.h"
#include <stdio.h>
#include <stdlib.h>
#include "pthread.h"

#ifdef USE_SYSLOG
	#include <syslog.h>
#else
	#define LOG_EMERG       0       /* system is unusable */
	#define LOG_ALERT       1       /* action must be taken immediately */
	#define LOG_CRIT        2       /* critical conditions */
	#define LOG_ERR         3       /* error conditions */
	#define LOG_WARNING     4       /* warning conditions */
	#define LOG_NOTICE      5       /* normal but significant condition */
	#define LOG_INFO        6       /* informational */
	#define LOG_DEBUG       7       /* debug-level messages */
#endif


#ifdef DEBUG
	#define ENABLE_CHECKING			1
	#ifndef DEFAULT_VERBOSITY
		#define DEFAULT_VERBOSITY		255	
	#endif
#else
	#ifndef DEFAULT_VERBOSITY
		#define DEFAULT_VERBOSITY	0
	#endif
#endif

/*! Redefine abort to be the verbose abort.  */
#define abort() verbose_abort(__FILE__, __LINE__)

#define TRACE2(format, ...) message (1, stderr,				       \
				    "TRACE %s() by %lu at %s:%d: " format "\n",\
				    __func__, (unsigned long) pthread_self (), \
				    __FILE__, __LINE__, ## __VA_ARGS__)

#ifdef ENABLE_CHECKING

	/*! Print which function we are in with additional information.  */
	#define TRACE(format, ...) message (4, stderr,				       \
				    "TRACE %s() by %lu at %s:%d: " format "\n",\
				    __func__, (unsigned long) pthread_self (), \
				    __FILE__, __LINE__, ## __VA_ARGS__)

	/*! Print the function name and return integer value.  */
	#define RETURN_INT(RETVAL)						\
	  do {									\
	    int32_t _r = (int32_t) (RETVAL);					\
	    TRACE ("return %" PRIi32, _r);					\
	    return _r;								\
	  } while (0)

	/*! Print the function name and return pointervalue.  */
	#define RETURN_PTR(RETVAL)						\
	  do {									\
	    TRACE ("return %p", (void *) (RETVAL));				\
	    return (RETVAL);							\
	  } while (0)

	/*! Print the function name and return bool value.  */
	#define RETURN_BOOL(RETVAL)						\
	  do {									\
	    bool _r = (RETVAL);							\
	    TRACE ("return %d", _r);						\
	    return _r;								\
	  } while (0)

	/*! Print the function name.  */
	#define RETURN_VOID							\
	  do {									\
	    TRACE ("return");							\
	    return;								\
	  } while (0)


#else //ENABLE_CHECKING not defined

	#define TRACE(...)
	#define RETURN_INT(RETVAL) return (RETVAL)
	#define RETURN_PTR(RETVAL) return (RETVAL)
	#define RETURN_BOOL(RETVAL) return (RETVAL)
	#define RETURN_VOID return

#endif //ENABLE_CHECKING

extern void zfs_openlog(void);

extern void zfs_closelog(void);

/*! Level of verbosity.  Higher number means more messages.  */
extern int verbose;

/*! Print message to F if LEVEL > VERBOSE.  */
extern void message (int level, FILE *f, const char *format, ...)
  ATTRIBUTE_PRINTF_3;

/*! Report an internal error.  */
extern void internal_error (const char *format, ...) ATTRIBUTE_NORETURN;

/*! Report an "Aborted" internal error.  */
extern void verbose_abort (const char *file, int line) ATTRIBUTE_NORETURN;

#endif
