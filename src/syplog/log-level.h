#ifndef LOG_LEVEL_H
#define LOG_LEVEL_H

/*! \file
    \brief Log levels and associated helper functions.

  Log level is in general verbosity of logger. 
  There are defined multiple log levels. They are ordered 
  in ascending order from the most important (LOG_EMERG the lowest) 
  to the least important (LOG_LOOPS the greatest).
  Messages with greater log level than actual log level (verbosity) 
  of logger won't be logged.
*/

/* Copyright (C) 2007 Jiri Zouhar

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

#define	_GNU_SOURCE

#include "log-constants.h"

/// typedef for log level
typedef uint32_t		log_level_t;

/// maximum length of stringified representation of log level
#define LOG_LEVEL_STRING_LEN	16

/// system is unusable 
#define LOG_EMERG       0
#define LOG_EMERG_NAME		"EMERGENCY"
/// action must be taken immediately
#define LOG_ALERT       1
#define LOG_ALERT_NAME		"ALERT"
/// critical conditions
#define LOG_CRIT        2
#define LOG_CRIT_NAME		"CRITICAL"
/// error conditions
#define LOG_ERR         3
#define LOG_ERR_NAME		"ERROR"
/// warning conditions
#define LOG_WARNING     4
#define LOG_WARNING_NAME	"WARNING"
/// normal but significant condition
#define LOG_NOTICE      5
#define LOG_NOTICE_NAME		"NOTICE"
/// informational
#define LOG_INFO        6
#define LOG_INFO_NAME		"INFO"
/// debug-level messages
#define LOG_DEBUG       7
#define LOG_DEBUG_NAME		"DEBUG"
/// locking info
#define LOG_LOCK        8
#define LOG_LOCK_NAME		"LOCK"
/// function entry and leave
#define LOG_FUNC        9
#define LOG_FUNC_NAME		"FUNCTION"
/// data changes
#define LOG_DATA       10
#define LOG_DATA_NAME		"DATA"
/// loops
#define LOG_LOOPS      11
#define LOG_LOOPS_NAME		"LOOPS"

/// unknown log level
#define LOG_UNKNOWN    12
#define LOG_UNKNOWN_NAME	"UNKONWN"

#define LOG_ALL			(uint32_t)-1
#define LOG_NONE		0


// TODO: fix in code
#define LOG_ERROR      LOG_ERR
#define LOG_TRACE      LOG_FUNC

/// Translates log_level_t to string.
/*! Translates log_level_t to user readable string
  @param level log level to translate
  @return constant string representing given log level or NULL
*/
static inline const char * log_level_to_name (log_level_t level)
{
  switch (level)
  {
    case LOG_EMERG: return LOG_EMERG_NAME;
    case LOG_ALERT: return LOG_ALERT_NAME;
    case LOG_CRIT: return LOG_CRIT_NAME;
    case LOG_ERR: return LOG_ERR_NAME;
    case LOG_WARNING: return LOG_WARNING_NAME;
    case LOG_NOTICE: return LOG_NOTICE_NAME;
    case LOG_INFO: return LOG_INFO_NAME;
    case LOG_DEBUG: return LOG_DEBUG_NAME;
    case LOG_FUNC: return LOG_FUNC_NAME;
    case LOG_LOCK: return LOG_LOCK_NAME;
    case LOG_DATA: return LOG_DATA_NAME;
    case LOG_LOOPS: return LOG_LOOPS_NAME;
    default: return LOG_UNKNOWN_NAME;
  }
}

/// Reads log_level_t from string.
/*! Read log_level_t from user readable string
  @param level_name log level name
  @return log level (or LOG_UNKNOWN)
*/
static inline log_level_t log_level_from_string (const char * level_name)
{
  if (strncmp (level_name, LOG_EMERG_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_EMERG;
  if (strncmp (level_name, LOG_ALERT_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_ALERT;
  if (strncmp (level_name, LOG_CRIT_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_CRIT;
  if (strncmp (level_name, LOG_ERR_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_ERR;
  if (strncmp (level_name, LOG_WARNING_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_WARNING;
  if (strncmp (level_name, LOG_NOTICE_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_NOTICE;
  if (strncmp (level_name, LOG_INFO_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_INFO;
  if (strncmp (level_name, LOG_DEBUG_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_DEBUG;
  if (strncmp (level_name, LOG_FUNC_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_FUNC;
  if (strncmp (level_name, LOG_LOCK_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_LOCK;
  if (strncmp (level_name, LOG_DATA_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_DATA;
  if (strncmp (level_name, LOG_LOOPS_NAME, LOG_LEVEL_STRING_LEN) == 0)
    return LOG_LOOPS;

  return LOG_UNKNOWN;
}


#ifdef DEBUG
  #define ENABLE_CHECKING               1
  #ifndef DEFAULT_LOG_LEVEL
    #define DEFAULT_LOG_LEVEL           7
  #endif
#else
  #ifndef DEFAULT_LOG_LEVEL
    #define DEFAULT_LOG_LEVEL           3
  #endif
#endif

#endif /*LOG_LEVEL_H*/
