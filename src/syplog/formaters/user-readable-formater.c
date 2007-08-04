/*! \file
    \brief User readable formater implementation.  */

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

#include "formater-api.h"
#include "user-readable-formater.h"

/*! Definition of user readable formater type */
struct formater_def user_readable_formater = 
{
  .stream = user_readable_stream_format,
  .mem = user_readable_mem_format,
  .file = user_readable_file_format,
  .get_max_print_size = user_readable_max_print_size
};

/// Print message to string buffer
/*! Print message to string buffer in format defined for user readable formater
  @param message non NULL pointer to log struct
  @param buffer non NULL string buffer with length at least buffer_len
  @param buffer_len maximal number of chars printed to buffer
  @return number of chars printed or -syp_error on error
*/
int32_t fill_buffer (log_struct message, char * buffer, int32_t buffer_len)
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

  chars_printed = snprintf (buffer, buffer_len, "%s\t%s\t%ld/%s\t%s\t%s\t%s\t%s\t%s\n", 
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

/*! Format log to stream in user readable manner */
int32_t user_readable_stream_format (log_struct message, int socket)
{
  return -ERR_NOT_IMPLEMENTED;
}

/*! Format log to memory in user readable manner */
int32_t user_readable_mem_format (log_struct message, void * mem_addr)
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
int32_t user_readable_file_format (log_struct message, FILE * file)
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

  chars_printed = fprintf("%s", buffer);

  if (chars_printed > 0)
    return chars_printed;
  else
    return -ERR_SYSTEM;
}

/*! Returns maximum length (in bytes) of space occupied in target medium */
int32_t user_readable_max_print_size (void)
{
  return MAX_LOG_STRING_SIZE;
}
