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

#include <string.h>
#include <stdio.h>

#include "formater-api.h"
#include "raw-formater.h"


/*! Definition of user readable formater type */
struct formater_def raw_formater = 
{
  .stream = raw_stream_format,
  .mem = raw_mem_format,
  .file = raw_file_format,
  .get_max_print_size = raw_max_print_size
};

/*! Format log to stream in user readable manner */
int32_t raw_stream_format (log_struct message, int socket)
{
#ifdef	ENABLE_CHECKING
  if (message == NULL)
  {
    return -ERR_BAD_PARAMS;
  }
#endif

  return -ERR_NOT_IMPLEMENTED;
}

/*! Format log to memory in user readable manner */
int32_t raw_mem_format (log_struct message, void * mem_addr)
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

/*! Format log to file in user readable manner */
int32_t raw_file_format (log_struct message, FILE * file)
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

/*! Returns maximum length (in bytes) of space occupied in target medium */
int32_t raw_max_print_size (void)
{
  return sizeof (struct log_struct_def);
}
