#ifndef MEDIUM_API_H
#define MEDIUM_API_H

/*! \file
    \brief Api for media access functions.  

  Medium is set of functions and settings describing the method
  how and from where to read or write logs;

  Medium state is dependent on type (file, memory, socket reader).
  Medium uses formatters to parse logs
 
  @see formatter-api.h
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

#include "formatters/formatter-api.h"
#include "medium.h"

#define PARAM_MEDIUM_FMT_LONG	"formatter"
#define PARAM_MEDIUM_FMT_CHAR	'f'

#define	PARAM_MEDIUM_TYPE_LONG	"medium"
#define	PARAM_MEDIUM_TYPE_CHAR	'm'

#define PARAM_MEDIUM_OP_LONG	"operation"
#define PARAM_MEDIUM_OP_CHAR	'o'

#define PARAM_MEDIUM_SIZE_LONG	"log_size"
#define PARAM_MEDIUM_SIZE_CHAR	's'

#define	OPERATION_NAME_LEN	6

#define	OPERATION_READ_NAME	"read"
#define	OPERATION_WRITE_NAME	"write"

/*! Initializes medium according to parameters.
  @param argv the same format as in "main", but parses only medium specific options
  @param argc argv item count
  @param target non NULL pointer to medium structure to initialize
  @return std errors. When error is returned, structure is in undefined state
          but internal pointers are freed
*/
syp_error open_medium (struct medium_def * target, int  argc, const char ** argv);

/*! Check if argument is recognized by some medium
  @param arg command line argument (in format --argument_name=value)
  @return TRUE in case of recognition, FALSE otherwise
*/
bool_t is_medium_arg (const char * arg);

/*! Close reader and free internal pointers.
  @param target non NULL pointer to initialized medium
  @return std errors. When error is returned, structure is in undefined state
          but most likeli internal pointers are freed
*/
syp_error close_medium (struct medium_def * target);

/*! Access Medium. Depends on kind of access this means read log or write log
  the type of operation depends on initializing values of medium

  @param target initialized medium (non NULL)
  @param log message to fill or write (non NULL)
  @return std errors. On error, message may not be written.
*/
syp_error access_medium (struct medium_def * target, log_struct log);

/*! prints media options help to fd.
  @param fd file descriptor to which to write help
  @param tabs how much tabs prepend to help
*/
void print_media_help (int fd, int tabs);

#endif /*MEDIUM_API_H*/
