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

#include <stdio.h>

#include "log-struct.h"
#include "log-constants.h"
#include "user-readable-formater.h"
#include "raw-formater.h"

/*! Function type for formating log to socket-like targets.
  @param message log message to print (non NULL)
  @param socket open ip socket. At maximum "max_print_size" chars will be written
  @return number of bytes printed or -syp_error on error
*/
typedef int32_t (*stream_format) (log_struct message, int socket);

/*! Function type for formating log to memory.
  @param message log message to print (non NULL)
  @param mem_addr pointer to memory where to write log. It should have enough free space according to the actual formater (returned by max_print_size)
  @return number of bytes printed or -syp_error on error
*/
typedef int32_t (*mem_format) (log_struct message, void * mem_addr);

/*! Function type for formating log to file.
  @param message log message to print (non NULL)
  @param file open file handler. At maximum "max_print_size" chars will be written.
  @return number of bytes printed or -syp_error on error
*/
typedef int32_t (*file_format) (log_struct message,  FILE * file);

/*! Returns maximum size used in target by one log print.
  @param message log message to print (non NULL)
  @param file open file handler. At maximum "max_print_size" chars will be written.
  @return number of maximum bytes printed for one log print or -syp_error on error
*/
typedef int32_t (*max_print_size) (void);

typedef struct formater_def
{
  stream_format stream;
  mem_format mem;
  file_format file;
  max_print_size get_max_print_size;
} * formater;

/*! Returns pointer to formater according to name.
  @param name user readable formater name (used in "constructor" parameters)
  @return pointer to static structure holding formater function pointers or NULL on error (unknown name, ...)
*/
formater formater_for_name (const char * name);





#endif /*FORMATER_API_H*/
