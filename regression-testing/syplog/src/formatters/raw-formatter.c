/*! \file
    \brief Raw formatter implementation.  */

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Syplog.

   Syplog is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Syplog is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   Syplog; see the file COPYING.  If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA;
   or download it from http://www.gnu.org/licenses/gpl.html 
*/

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#define _GNU_SOURCE

#include "formatter-api.h"
#include "raw-formatter.h"


/*! Definition of raw formatter type */
struct formatter_def raw_formatter = 
{
  .stream_write = raw_stream_write,
  .mem_write = raw_mem_write,
  .file_write = raw_file_write,

  .stream_read = raw_stream_read,
  .mem_read = raw_mem_read,
  .file_read = raw_file_read,

  .get_max_print_size = raw_max_print_size
};

#ifndef ENABLE_CHECKING
#define CHECKING_UNUSED UNUSED
#else
#define CHECKING_UNUSED
#endif

/*! Format log to stream in raw format */
int32_t raw_stream_write (log_struct message CHECKING_UNUSED, int socket UNUSED)
{
#ifdef	ENABLE_CHECKING
  if (message == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  return -ERR_NOT_IMPLEMENTED;
}

/*! Format log to memory in raw format */
int32_t raw_mem_write (log_struct message, void * mem_addr)
{
#ifdef	ENABLE_CHECKING
  if (message == NULL || mem_addr == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  memcpy (mem_addr, message, sizeof (message));

  return sizeof (message);
}

/*! Format log to file in raw format */
int32_t raw_file_write (log_struct message, FILE * file)
{
#ifdef	ENABLE_CHECKING
  if (message == NULL || file == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  int32_t chars_printed = 0;

  chars_printed = fwrite (message, 1, sizeof (struct log_struct_def), file);

  if (chars_printed > 0)
    return chars_printed;
  else
    return -ERR_SYSTEM;
}


/*! Read log from stream in raw format */
int32_t raw_stream_read (log_struct message CHECKING_UNUSED, int socket UNUSED)
{
#ifdef	ENABLE_CHECKING
  if (message == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  return -ERR_NOT_IMPLEMENTED;
}

/*! Read log from memory in raw format */
int32_t raw_mem_read (log_struct message, void * mem_addr)
{
#ifdef	ENABLE_CHECKING
  if (message == NULL || mem_addr == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  memcpy (message, mem_addr, sizeof (message));

  return sizeof (message);
}

/*! Read log from file in raw format */
int32_t raw_file_read (log_struct message, FILE * file)
{
#ifdef	ENABLE_CHECKING
  if (message == NULL || file == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  int32_t chars_read = 0;

  chars_read = fread (message, 1, sizeof (struct log_struct_def), file);

  if (chars_read > 0)
    return chars_read;
  else
    return -ERR_SYSTEM;
}


/*! Returns maximum length (in bytes) of space occupied in target medium */
int32_t raw_max_print_size (void)
{
  return sizeof (struct log_struct_def);
}
