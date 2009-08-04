#ifndef FORMATTER_API_H
#define FORMATTER_API_H

/*! \file
    \brief Api for formating functions.

  Formater is set of functions for "printing" log in some format
  to different targets. It should give (if possible) the same
  result upon every target (memory, file, socket, etc).
  
  Some formatter can write log as user readable string,
  other as a raw data or xml.

*/

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
#include <stdio.h>
#undef _GNU_SOURCE

#include "log-struct.h"
#include "log-constants.h"
#include "user-readable-formatter.h"
#include "raw-formatter.h"

#define DEFAULT_FORMATTER	&user_readable_formatter

/*! Function type for formating log to socket-like targets.
  @param message log message to print (non NULL)
  @param socket open ip socket. At maximum "max_print_size" chars will be written
  @return number of bytes printed or -syp_error on error
*/
typedef int32_t (*stream_write_format) (log_struct message, int socket);

/*! Function type for formating log to memory.
  @param message log message to print (non NULL)
  @param mem_addr pointer to memory where to write log. It should have enough free space according to the actual formatter (returned by max_print_size)
  @return number of bytes printed or -syp_error on error
*/
typedef int32_t (*mem_write_format) (log_struct message, void * mem_addr);

/*! Function type for formating log to file.
  @param message log message to print (non NULL)
  @param file open file handler. At maximum "max_print_size" chars will be written.
  @return number of bytes printed or -syp_error on error
*/
typedef int32_t (*file_write_format) (log_struct message,  FILE * file);


/*! Function type for reading logs from socket-like media.
  @param message log message to fill (non NULL)
  @param socket open ip socket.
  @return number of bytes read or -syp_error on error
*/
typedef int32_t (*stream_read_format) (log_struct message, int socket);

/*! Function type for reading logs from memory.
  @param message log message to fill (non NULL)
  @param mem_addr pointer to memory where log is stored (non NULL).
  @return number of bytes read or -syp_error on error
*/
typedef int32_t (*mem_read_format) (log_struct message, void * mem_addr);

/*! Function type for reading logs from file.
  @param message log message to fill (non NULL)
  @param file open file handler.
  @return number of bytes read or -syp_error on error
*/
typedef int32_t (*file_read_format) (log_struct message,  FILE * file);


/*! Returns maximum size used in target by one log print.
  @param message log message to print (non NULL)
  @param file open file handler. At maximum "max_print_size" chars will be written.
  @return number of maximum bytes printed for one log print or -syp_error on error
*/
typedef int32_t (*max_print_size) (void);

typedef struct formatter_def
{
  stream_write_format stream_write;
  mem_write_format mem_write;
  file_write_format file_write;

  stream_read_format stream_read;
  mem_read_format mem_read;
  file_read_format file_read;

  max_print_size get_max_print_size;
} * formatter;

/*! Returns pointer to formatter according to name.
  @param name user readable formatter name (used in "constructor" parameters)
  @return pointer to static structure holding formatter function pointers or NULL on error (unknown name, ...)
*/
formatter formatter_for_name (const char * name);

/*! prints formatters options help to fd.
  @param fd file descriptor to which to write help
  @param tabs how much tabs prepend to help
*/
void print_formatters_help (int fd, int tabs);





#endif /*FORMATTER_API_H*/
