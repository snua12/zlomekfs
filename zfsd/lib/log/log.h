/* ! \file \brief Logging functions.  */

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

#ifndef LOG_H
#define LOG_H


#include "system.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef ENABLE_DBUS
#include "dbus-provider.h"
#endif

#include "pthread-wrapper.h"

#include "syplog.h"

extern struct logger_def syplogger;

void update_node_name(void);

/* ! Redefine abort to be the verbose abort.  */
#define abort() verbose_abort(__FILE__, __LINE__)

#ifdef ENABLE_TRACE

	/* ! Print which function we are in with additional information.  */
#define TRACE(format, ...) message (LOG_FUNC, FACILITY_ZFSD,	       \
				    "TRACE %s() by %lu at %s:%d: " format "\n",\
				    __func__, (unsigned long) pthread_self (), \
				    __FILE__, __LINE__, ## __VA_ARGS__)

	/* ! Print the function name and return integer value.  */
#define RETURN_INT(RETVAL)						\
	  do {									\
	    int32_t _r = (int32_t) (RETVAL);					\
	    TRACE ("return %" PRIi32, _r);					\
	    return _r;								\
	  } while (0)

	/* ! Print the function name and return pointervalue.  */
#define RETURN_PTR(RETVAL)						\
	  do {									\
	    TRACE ("return %p", (void *) (RETVAL));				\
	    return (RETVAL);							\
	  } while (0)

	/* ! Print the function name and return bool value.  */
#define RETURN_BOOL(RETVAL)						\
	  do {									\
	    bool _r = (RETVAL);							\
	    TRACE ("return %d", _r);						\
	    return _r;								\
	  } while (0)

	/* ! Print the function name.  */
#define RETURN_VOID							\
	  do {									\
	    TRACE ("return");							\
	    return;								\
	  } while (0)


#else // ENABLE_TRACE not defined

#define TRACE(...)
#define RETURN_INT(RETVAL) return (RETVAL)
#define RETURN_PTR(RETVAL) return (RETVAL)
#define RETURN_BOOL(RETVAL) return (RETVAL)
#define RETURN_VOID return

#endif // ENABLE_TRACE

extern void zfs_openlog(int argc, const char **argv);

extern void zfs_closelog(void);

#define is_logger_arg(arg)	is_syplog_arg (arg)

#define message(level,facility,format...) do_log(&syplogger,level,facility, ## format)

/* ! Report an internal error.  */
extern void internal_error(const char *format, ...) ATTRIBUTE_NORETURN;

/* ! Report an "Aborted" internal error.  */
	 extern void verbose_abort(const char *file, int line) ATTRIBUTE_NORETURN;

#endif
