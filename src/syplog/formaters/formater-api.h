#ifndef FORMATER_API_H
#define FORMATER_API_H

/*! \file
    \brief Api for formating functions.

  Formater is set of functions for "printing" log in some format
  to different targets. It should give (if possible) the same
  result upon every target (memory, file, socket, etc).
  
  Some formater can write log as user readable string,
  other as a raw data or xml.

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
#undef _GNU_SOURCE

#include "log-struct.h"
#include "log-constants.h"
#include "user-readable-formater.h"
#include "raw-formater.h"

#define DEFAULT_FORMATER	&user_readable_formater

/*! Function type for formating log to socket-like targets.
  @param message log message to print (non NULL)
  @param socket open ip socket. At maximum "max_print_size" chars will be written
  @return number of bytes printed or -syp_error on error
*/
typedef int32_t (*stream_write_format) (log_struct message, int socket);

/*! Function type for formating log to memory.
  @param message log message to print (non NULL)
  @param mem_addr pointer to memory where to write log. It should have enough free space according to the actual formater (returned by max_print_size)
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

typedef struct formater_def
{
  stream_write_format stream_write;
  mem_write_format mem_write;
  file_write_format file_write;

  stream_read_format stream_read;
  mem_read_format mem_read;
  file_read_format file_read;

  max_print_size get_max_print_size;
} * formater;

/*! Returns pointer to formater according to name.
  @param name user readable formater name (used in "constructor" parameters)
  @return pointer to static structure holding formater function pointers or NULL on error (unknown name, ...)
*/
formater formater_for_name (const char * name);

/*! prints formaters options help to fd.
  @param fd file descriptor to which to write help
  @param tabs how much tabs prepend to help
*/
void print_formaters_help (int fd, int tabs);





#endif /*FORMATER_API_H*/
