#ifndef LOG_CONSTANTS_H
#define LOG_CONSTANTS_H

/* ! \file \brief Logger specific constants.  */

/* Copyright (C) 2007, 2008, 2010 Jiri Zouhar, Rastislav Wartiak

   This file is part of Syplog.

   Syplog is free software; you can redistribute it and/or modify it under the 
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   Syplog is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with Syplog; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#ifndef  __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#if defined(__APPLE__) && defined(__MACH__)
	#define PRtvSEC "ld"
	#define PRtvUSEC PRIi32
#else
	#define PRtvSEC "ld"
	#define PRtvUSEC "ld"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "syp-error.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define UNUSED	__attribute__ ((__unused__))

#ifndef ENABLE_CHECKING
#define CHECKING_UNUSED UNUSED
#else
#define CHECKING_UNUSED
#endif

	/// maximal length of log message (user given string)
#define	LOG_MESSAGE_LEN	1024

	/// maximal length of thread name
#define	THREAD_NAME_LEN	32
	/// maximal length of node name FIXME: use std define from zfs

#define	NODE_NAME_LEN	64
	/// maximal length of hostname
#define	HOSTNAME_LEN	255

	/// maximal length of user medium name
#define MEDIUM_NAME_LEN 32
	/// maximal length of formatter name
#define FORMATTER_NAME_LEN 32

	/// maximal length of filename (absolute or relative path)
#define	FILE_NAME_LEN		128

	/// typedef for unsigned 32 bit number
	typedef unsigned int uint32_t;
	/// typedef for signed 32 bit number
	typedef int int32_t;

	/// boolean type
	typedef int bool_t;

	/// true ;)
#define	TRUE	1
	/// false ;)
#define	FALSE	0

	/* ! Translate time stored in struct timeval to user readable string (unix 
	   time ;) @param local_time non NULL, valid timeval structure @param
	   buffer string buffer to which time should be printed (non NULL) @param
	   buffer_len length of buffer (maximum chars to print to it) @return
	   number of chars printed or -syp_error */
	static inline int32_t time_to_string(struct timeval *local_time, 
	char *buffer, uint32_t buffer_len)
	{
		int32_t chars_printed = 0;

#ifdef ENABLE_CHECKING
		if (local_time == NULL || buffer == NULL || buffer_len == 0)
			return -ERR_BAD_PARAMS;
#endif

		  chars_printed =
			snprintf(buffer, buffer_len, "%"PRtvSEC":%06"PRtvUSEC, local_time->tv_sec,
					 local_time->tv_usec);

		if (chars_printed > 0)
			  return chars_printed;
		else
			  return -ERR_SYSTEM;
	}

	/* ! read time from string to struct timeval @param local_time non NULL,
	   timeval structure @param buffer string buffer from which to read time
	   (non NULL) @return number of chars printed or -syp_error */
	static inline int32_t time_from_string(const char *buffer,
										   struct timeval *local_time)
	{
		int32_t chars_read = 0;

#ifdef ENABLE_CHECKING
		if (local_time == NULL || buffer == NULL)
			return -ERR_BAD_PARAMS;
#endif

		chars_read =
			sscanf(buffer, "%"PRtvSEC":%"PRtvUSEC, &(local_time->tv_sec),
				   &(local_time->tv_usec));

		if (chars_read > 0)
			return chars_read;
		else
			return -ERR_SYSTEM;
	}

	/* ! Translate timezone (+-sec from greenwich) to string @param
	   local_timezone +-sec from greenwich @param buffer non NULL buffer where 
	   to print timezone to @param buffer_len length of buffer (maximum chars
	   to print to it) @return number of chars printed or -syp_error */
	static inline int32_t timezone_to_string(uint64_t local_timezone,
											 char *buffer, uint32_t buffer_len)
	{
		int32_t chars_printed = 0;
#ifdef ENABLE_CHECKING
		if (buffer == NULL || buffer_len == 0)
			return -ERR_BAD_PARAMS;
#endif
		chars_printed = snprintf(buffer, buffer_len, "%" PRIu64, local_timezone);

		if (chars_printed > 0)
			return chars_printed;
		else
			return -ERR_SYSTEM;
	}

	/* ! Read timezone (+-sec from greenwich) from string @param
	   local_timezone +-sec from greenwich (non NULL pointer) @param buffer
	   non NULL buffer with timezone @return number of chars read or
	   -syp_error */
	static inline int32_t timezone_from_string(const char *buffer,
											   uint64_t * local_timezone)
	{
		int32_t chars_read = 0;
#ifdef ENABLE_CHECKING
		if (buffer == NULL || local_timezone == NULL)
			return -ERR_BAD_PARAMS;
#endif
		chars_read = sscanf(buffer, "%" PRIu64, local_timezone);

		if (chars_read > 0)
			return chars_read;
		else
			return -ERR_SYSTEM;
	}

/** Print formatted string to file descriptor
  Prepend tabs tabs to it.
 * @param tabs how many tabs prepend to string
 * @param fmt format as in case of printf
 * @param ... auxiliary parameters to fmt
*/
	static inline void tabize_print(int tabs, int fd, const char *fmt, ...)
	{
		va_list ap;

		va_start(ap, fmt);

		for (; tabs > 0; tabs--)
			dprintf(fd, "\t");
		vdprintf(fd, fmt, ap);

		va_end(ap);
	}

/** Checks, if arg is present in table.
 * @param option_table option table as given to getopt_long
 * @param arg argument as given from command line
    in format "--" option_table[x].name "=" value
 * @return TRUE if argument found, FALSE otherwise
*/
	static inline bool_t opt_table_contains(const struct option *option_table,
											const char *arg)
	{
		if (strlen(arg) < 2)
			return FALSE;

		int table_index = 0;
		const char *substr = NULL;
		for (table_index = 0; option_table[table_index].name != NULL;
			 table_index++)
		{
			substr = strstr(arg, option_table[table_index].name);
			if (substr != NULL && substr == arg + 2 &&
				substr[strlen(option_table[table_index].name)] == '=')
				return TRUE;
		}

		return FALSE;
	}

#ifdef __cplusplus
}
#endif

#endif /* LOG_CONSTANTS_H */
