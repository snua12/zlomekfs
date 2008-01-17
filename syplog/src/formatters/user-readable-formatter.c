/*! \file
    \brief User readable formatter implementation.  */

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

#include "formatter-api.h"
#include "user-readable-formatter.h"

/*! Definition of user readable formatter type */
struct formatter_def user_readable_formatter = 
{
  .stream_write = user_readable_stream_write,
  .mem_write = user_readable_mem_write,
  .file_write = user_readable_file_write,

  .stream_read = user_readable_stream_read,
  .mem_read = user_readable_mem_read,
  .file_read = user_readable_file_read,

  .get_max_print_size = user_readable_max_print_size
};

/// Print message to string buffer
/*! Print message to string buffer in format defined for user readable formatter
  @param message non NULL pointer to log struct
  @param buffer non NULL string buffer with length at least buffer_len
  @param buffer_len maximal number of chars printed to buffer
  @return number of chars printed or -syp_error on error
*/
int32_t fill_buffer (const log_struct message, char * buffer, int32_t buffer_len)
{

#ifdef ENABLE_CHECKING
  if (message == NULL || buffer == NULL || buffer_len <= 0)
    return -ERR_BAD_PARAMS;
#endif

  char time_str[TIME_STRING_LEN] = "";
  char timezone_str[TIMEZONE_STRING_LEN] = "";
  time_to_string (&(message->time), time_str, TIME_STRING_LEN);
  timezone_to_string (message->timezone, timezone_str, TIMEZONE_STRING_LEN);
  int32_t chars_printed = 0;

  chars_printed = snprintf (buffer, buffer_len, "%s\t%s\t%lu/%s\t%s\t%s\t%s\t%s\t%s\n", 
           message->hostname,
           message->node_name,
           message->thread_id,
           message->thread_name,
           facility_to_name (message->facility),
           log_level_to_name (message->level),
           time_str,
           timezone_str,
           message->message
           );
  if (chars_printed <=0)
    return -ERR_SYSTEM;
  else
    return chars_printed;

}

/// Read message from string buffer
/*! Read message from string buffer in format defined for user readable formatter
  @param message non NULL pointer to log struct
  @param buffer non NULL string buffer containing log message in format defined for user readable formatter
  @return number of chars read or -syp_error on error
*/
int32_t read_buffer (log_struct message, const char * buffer)
{

#ifdef ENABLE_CHECKING
  if (message == NULL || buffer == NULL)
    return -ERR_BAD_PARAMS;
#endif

  char time_str[TIME_STRING_LEN] = "";
  char timezone_str[TIMEZONE_STRING_LEN] = "";
  char facility_str[FACILITY_STRING_LEN] = "";
  char log_level_str[LOG_LEVEL_STRING_LEN] = "";
  int32_t chars_read = 0;

  chars_read = sscanf (buffer, "%s\t%s\t%ld/%s\t%s\t%s\t%s\t%s\t%s\n", 
           message->hostname,
           message->node_name,
           &message->thread_id,
           message->thread_name,
           facility_str,
           log_level_str,
           time_str,
           timezone_str,
           message->message
           );
  if (chars_read <=0)
    return -ERR_SYSTEM;

  time_from_string (time_str, &(message->time));
  timezone_from_string (timezone_str, &(message->timezone));
  message->facility = facility_from_string (facility_str);
  message->level = log_level_from_string (log_level_str);


    return chars_read;

}


/*! Format log to stream in user readable manner */
int32_t user_readable_stream_write (log_struct message, int socket)
{
  return -ERR_NOT_IMPLEMENTED;
}

/*! Format log to memory in user readable manner */
int32_t user_readable_mem_write (log_struct message, void * mem_addr)
{

#ifdef ENABLE_CHECKING
  if (message == NULL || mem_addr == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  return fill_buffer (message, (char *) mem_addr, MAX_LOG_STRING_SIZE);
}

/*! Format log to file in user readable manner */
int32_t user_readable_file_write (log_struct message, FILE * file)
{
#ifdef ENABLE_CHECKING
  if (message == NULL || file == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  char buffer[MAX_LOG_STRING_SIZE] = "";

  int32_t chars_printed = fill_buffer (message, buffer, MAX_LOG_STRING_SIZE);
  if (chars_printed < 0)
  {
    return chars_printed;
  }

  chars_printed = fwrite (buffer, 1, chars_printed, file);

  if (chars_printed > 0)
    return chars_printed;
  else
    return -ERR_SYSTEM;
}


/*! Read log from stream in user readable manner */
int32_t user_readable_stream_read (log_struct message, int socket)
{
  return -ERR_NOT_IMPLEMENTED;
}

/*! Read log from memory in user readable manner */
int32_t user_readable_mem_read (log_struct message, void * mem_addr)
{

#ifdef ENABLE_CHECKING
  if (message == NULL || mem_addr == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  return read_buffer (message, (char *) mem_addr);
}

/*! Read log from file in user readable manner */
int32_t user_readable_file_read (log_struct message, FILE * file)
{
#ifdef ENABLE_CHECKING
  if (message == NULL || file == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  char buffer[MAX_LOG_STRING_SIZE] = "";

  int32_t chars_read = 0;
  int32_t chars_parsed = 0;

  chars_read = fread (buffer, 1, MAX_LOG_STRING_SIZE, file);
  if (ferror (file) != 0)
  {
    return -ERR_SYSTEM;
  }

  chars_parsed = read_buffer (message, buffer);
  if (chars_parsed < 0)
  {
    fseek (file, -chars_read, SEEK_CUR);
    return chars_parsed;
  }
  else
  {
    fseek (file, -chars_read + chars_parsed, SEEK_CUR);
    return chars_parsed;
  }

}


/*! Returns maximum length (in bytes) of space occupied in target medium */
int32_t user_readable_max_print_size (void)
{
  return MAX_LOG_STRING_SIZE;
}
