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

/// system is unusable 
#define LOG_EMERG       0
/// action must be taken immediately
#define LOG_ALERT       1
/// critical conditions
#define LOG_CRIT        2
/// error conditions
#define LOG_ERR         3
/// warning conditions
#define LOG_WARNING     4
/// normal but significant condition
#define LOG_NOTICE      5
/// informational
#define LOG_INFO        6
/// debug-level messages
#define LOG_DEBUG       7
/// locking info
#define LOG_LOCK        8
/// function entry and leave
#define LOG_FUNC        9
/// data changes
#define LOG_DATA       10
/// loops
#define LOG_LOOPS      11

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
    case LOG_EMERG:
      return "EMERGENCY";
    case LOG_ALERT:
      return "ALERT";
    case LOG_CRIT:
      return "CRITICAL";
    case LOG_ERR:
      return "ERROR";
    case LOG_WARNING:
      return "WARNING";
    case LOG_NOTICE:
      return "NOTICE";
    case LOG_INFO:
      return "INFO";
    case LOG_DEBUG:
      return "DEBUG";
    case LOG_FUNC:
      return "FUNCTION";
    case LOG_LOCK:
      return "LOCK";
    case LOG_DATA:
      return "DATA";
    case LOG_LOOPS:
      return "LOOPS";
    default:
      return "UNKONWN";
  }
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
