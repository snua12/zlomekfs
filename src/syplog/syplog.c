/*! \file
    \brief Main routines.  
    
  Log opening, closing and using main functions.
*/

/* Copyright (C) 2007 Jiri Zouhar

   This file is part of Syplog.

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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#undef _GNU_SOURCE

#include "syplog.h"
#include "media/file-medium.h"
#include "control/listener.h"

void print_syplog_help (int fd, int tabs)
{
  if (fd == 0)
    fd = 1;
  
  tabize_print (tabs, fd, "logging specific options:\n");
  
  print_media_help (fd, tabs +1);
  
  tabize_print (tabs, fd, "defaults are:\n");
  tabize_print (tabs, fd, "--" PARAM_MEDIUM_TYPE_LONG "=" FILE_MEDIUM_NAME "\n");
  tabize_print (tabs, fd, "--" PARAM_MEDIUM_FMT_LONG "=" RAW_FORMATTER_NAME "\n");
  tabize_print (tabs, fd, "--" PARAM_MEDIUM_OP_LONG "=" OPERATION_WRITE_NAME "\n");

}

bool_t is_syplog_arg (const char * arg)
{
  return is_medium_arg (arg);
}

/// Set actual verbosity of logger
syp_error set_log_level (logger glogger, log_level_t level)
{
  glogger->log_level = level;
  
  return NOERR;
}

/// get actual log level (verbosity) of logger.
log_level_t get_log_level (logger glogger)
{
  return glogger->log_level;
}

/// Returns actual verbosity of logger
syp_error get_log_level_to (logger glogger, log_level_t * level)
{
  *level = glogger->log_level;
  
  return NOERR;
}

/// Turn on logging for messages from facility "facility"
syp_error set_facility (logger glogger, facility_t facility)
{
  pthread_mutex_lock (&(glogger->mutex));
  glogger->facilities  = facility_add (glogger->facilities, facility);
  pthread_mutex_unlock (&(glogger->mutex));

  return NOERR;
}

/// Set facilities logging policy.
syp_error set_facilities (logger glogger, facility_t facilities)
{
  glogger->facilities = facilities;
  
  return NOERR;
}

/// Turn off logging for messages from facility "facility"
syp_error reset_facility (logger glogger, facility_t facility)
{
  pthread_mutex_lock (&(glogger->mutex));
  glogger->facilities  = facility_del (glogger->facilities, facility);
  pthread_mutex_unlock (&(glogger->mutex));

  return NOERR;
}

/// Get actual facilities logging policy
syp_error get_facilities (logger glogger, facility_t * facilities)
{
  *facilities = glogger->facilities;
  
  return NOERR;
}


syp_error set_hostname_locked (logger glogger, const char * hostname)
{
  strncpy (glogger->hostname, hostname, HOSTNAME_LEN);
  return NOERR;
}
/// Set cached hostname.
syp_error set_hostname (logger glogger, const char * hostname)
{
#ifdef ENABLE_CHECKING
  if (glogger == NULL || hostname == NULL)
    return ERR_BAD_PARAMS;
#endif

  pthread_mutex_lock (&(glogger->mutex));
  set_hostname_locked (glogger, hostname);
  pthread_mutex_unlock (&(glogger->mutex));
  
  return NOERR;
}

syp_error set_timezone_locked (logger glogger, uint64_t timezone)
{
  glogger->timezone = timezone;
  return NOERR;
}

/// Set cached timezone
syp_error set_timezone (logger glogger, uint64_t timezone)
{
#ifdef ENABLE_CHECKING
  if (glogger == NULL)
    return ERR_BAD_PARAMS;
#endif

  pthread_mutex_lock (&(glogger->mutex));
  set_timezone_locked (glogger, timezone);
  pthread_mutex_unlock (&(glogger->mutex));
  
  return NOERR;
}

syp_error set_node_name_locked (logger glogger, const char * node_name)
{
  strncpy (glogger->node_name, node_name, NODE_NAME_LEN);
  return NOERR;
}
/// Set cached node name.
syp_error set_node_name (logger glogger, const char * node_name)
{
#ifdef ENABLE_CHECKING
  if (glogger == NULL || node_name == NULL)
    return ERR_BAD_PARAMS;
#endif

  pthread_mutex_lock (&(glogger->mutex));
  set_node_name_locked (glogger, node_name);
  pthread_mutex_unlock (&(glogger->mutex));
  
  return NOERR;
}


/// Default options for creating writer. Used when NULL argv is given to open_log
static char * default_options [] = 
{
"syplog",
"--" PARAM_MEDIUM_TYPE_LONG "=" FILE_MEDIUM_NAME,
"--" PARAM_MEDIUM_FMT_LONG "=" RAW_FORMATTER_NAME,
"--" PARAM_MEDIUM_OP_LONG "=" OPERATION_WRITE_NAME
};

/// default_options table row count
static int default_option_count = 3;

/// Opens log and initializes logger structure
syp_error open_log (logger glogger,  const char * node_name, int argc, const char ** argv)
{
  syp_error ret_code = NOERR;
  int sys_ret_code = 0;

  // from time.h, must be refreshed by calling tzset () before use
  extern int daylight;
  extern long timezone;
  extern char *tzname[2];

// we don't need exhaustive checks here, the default values should be correct
#ifdef ENABLE_CHECKING
  if (glogger == NULL || node_name == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  pthread_mutex_init (&(glogger->mutex), NULL);
  pthread_mutex_lock (&(glogger->mutex));

  ret_code = set_facilities ( glogger, FACILITY_ALL);
#ifdef ENABLE_CHECKING
  if (ret_code != NOERR)
    goto FINISHING;
#endif

  ret_code = set_log_level (glogger, DEFAULT_LOG_LEVEL);
#ifdef ENABLE_CHECKING
  if (ret_code != NOERR)
    goto FINISHING;
#endif

  ret_code = set_node_name_locked (glogger, node_name);
#ifdef ENABLE_CHECKING
  if (ret_code != NOERR)
    goto FINISHING;
#endif
  
  // load enviroment variable TZ to extern long timezone
  tzset ();
  ret_code = set_timezone_locked (glogger, timezone);
#ifdef ENABLE_CHECKING
  if (ret_code != NOERR)
    goto FINISHING;
#endif

  sys_ret_code = gethostname (glogger->hostname, HOSTNAME_LEN);
  if (sys_ret_code != SYS_NOERR)
  {
    ret_code = sys_to_syp_error (sys_ret_code);
    goto FINISHING;
  }
  
  if (argv == NULL)
  {
    argv = (const char **)default_options;
    argc = default_option_count;
  }
  
  ret_code = open_medium (&glogger->printer, argc, argv);
  if (ret_code != NOERR)
    goto FINISHING;
  
  FINISHING:
    pthread_mutex_unlock (&(glogger->mutex));
    if (ret_code != NOERR)
      pthread_mutex_destroy (&(glogger->mutex));

    return ret_code;

}

/// Logs message through initialized logger
syp_error do_log (logger glogger, log_level_t level, facility_t facility, const char *format, ...)
{
  struct log_struct_def log = LOG_STRUCT_STATIC_INITIALIZER;
  va_list ap;
  
  if (level > glogger->log_level)
    return NOERR;
  
  if (facility_get_state (glogger->facilities, facility) != TRUE)
  {
    return NOERR;
  }
  
  log.level = level;
  log.facility = facility;
  gettimeofday (&(log.time), NULL);
  strncpy (log.hostname, glogger->hostname, HOSTNAME_LEN);
  strncpy (log.node_name, glogger->node_name, NODE_NAME_LEN);
  log.timezone = glogger->timezone;
  log.thread_id = pthread_self();
  
  va_start (ap, format);
  
  vsnprintf (log.message, LOG_MESSAGE_LEN, format, ap);
  
  va_end (ap);
  
  access_medium (&(glogger->printer), &log);
  
  return NOERR;

}

/// Close log, deinitialize structure and free internal data (not structure itself)
syp_error close_log (logger glogger)
{
  pthread_mutex_lock (&(glogger->mutex));
  close_medium (&(glogger->printer));
  pthread_mutex_unlock (&(glogger->mutex));

  pthread_mutex_destroy (&(glogger->mutex));

  return NOERR;

}
