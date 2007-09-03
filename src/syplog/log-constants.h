#ifndef LOG_CONSTANTS_H
#define LOG_CONSTANTS_H

/*! \file
    \brief Logger specific constants.  */

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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <linux/types.h>

#undef _GNU_SOURCE

#include "syp-error.h"

/// maximal length of log message (user given string)
#define	LOG_MESSAGE_LEN	1024

/// maximal length of thread name
#define	THREAD_NAME_LEN	32
/// maximal length of node name FIXME: use std define from zfs

#define	NODE_NAME_LEN	64
/// maximal length of hostname
#define	HOSTNAME_LEN	255

/// maximal length of user writer name
#define WRITER_NAME_LEN 32
/// maximal length of formater name
#define FORMATER_NAME_LEN 32
/// maximal length of reader name
#define READER_NAME_LEN 32

/// maximal length of filename (absolute or relative path)
#define	FILE_NAME_LEN		128

/// typedef for unsigned 32 bit number
typedef	unsigned int uint32_t;
/// typedef for signed 32 bit number
typedef int int32_t;

/* defined in linux/types.h
/// typedef for signed 64 bit number
typedef long long int64_t;

/// typedef for unsigned 64 bit number
typedef unsigned long long uint64_t;

*/

/// boolean type
typedef int bool_t;

/// true ;)
#define	TRUE	1
/// false ;)
#define	FALSE	0

/*! Translate time stored in struct timeval to user readable string (unix time ;)
  @param local_time non NULL, valid timeval structure
  @param buffer string buffer to which time should be printed (non NULL)
  @param buffer_len length of buffer (maximum chars to print to it)
  @return number of chars printed or -syp_error
*/
static inline int32_t time_to_string (struct timeval * local_time, char * buffer, uint32_t buffer_len)
{
  int32_t chars_printed = 0;

#ifdef ENABLE_CHECKING
  if (time == NULL || buffer == NULL || buffer_len == 0)
    return -ERR_BAD_PARAMS;
#endif

  chars_printed = snprintf (buffer, buffer_len, "%ld:%ld", local_time->tv_sec, local_time->tv_usec);

  if (chars_printed > 0)
    return chars_printed;
  else
    return -ERR_SYSTEM;
}

/*! read time from string to struct timeval
  @param local_time non NULL, timeval structure
  @param buffer string buffer from which to read time (non NULL)
  @return number of chars printed or -syp_error
*/
static inline int32_t time_from_string (const char * buffer, struct timeval * local_time)
{
  int32_t chars_read = 0;

#ifdef ENABLE_CHECKING
  if (time == NULL || buffer == NULL || buffer_len == 0)
    return -ERR_BAD_PARAMS;
#endif

  chars_read = sscanf (buffer, "%ld:%ld", &(local_time->tv_sec), &(local_time->tv_usec));

  if (chars_read > 0)
    return chars_read;
  else
    return -ERR_SYSTEM;
}

/*! Translate timezone (+-sec from greenwich) to string
  @param local_timezone +-sec from greenwich
  @param buffer non NULL buffer where to print timezone to
  @param buffer_len length of buffer (maximum chars to print to it)
  @return number of chars printed or -syp_error
*/
static inline int32_t timezone_to_string (uint64_t local_timezone, char * buffer, uint32_t buffer_len)
{
  int32_t chars_printed = 0;
#ifdef ENABLE_CHECKING
  if (buffer == NULL || buffer_len == 0)
    return -ERR_BAD_PARAMS;
#endif
  chars_printed = snprintf (buffer, buffer_len, "%lu", local_timezone);

  if (chars_printed > 0)
    return chars_printed;
  else
    return -ERR_SYSTEM;
}

/*! Read timezone (+-sec from greenwich) from string
  @param local_timezone +-sec from greenwich (non NULL pointer)
  @param buffer non NULL buffer with timezone
  @return number of chars read or -syp_error
*/
static inline int32_t timezone_from_string (const char * buffer, uint64_t * local_timezone)
{
  int32_t chars_read = 0;
#ifdef ENABLE_CHECKING
  if (buffer == NULL || local_timezone == NULL)
    return -ERR_BAD_PARAMS;
#endif
  chars_read = sscanf (buffer, "%lu", local_timezone);

  if (chars_read > 0)
    return chars_read;
  else
    return -ERR_SYSTEM;
}


#endif /*LOG_CONSTANTS_H*/
