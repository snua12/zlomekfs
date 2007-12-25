#ifndef MEDIUM_H
#define MEDIUM_H

/*! \file
    \brief Main medium access functions definitions. 

  Defines unified api for reading writing logs to distinct types of "media".
  Medium is represented as a structure (blackbox for user) which
  holds both state of medium and pointers to functions.
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


#include "log-constants.h"
#include "log-struct.h"

typedef enum medium_operation_def{
  NO_OPERATION = 0,
  READ_LOG,
  WRITE_LOG
} medium_operation;

/*! Enum of known media */
typedef enum
{
  /// don't use any medium - mainly used as "default" and "uninitialized"
  NO_MEDIUM = 0,
  /// write logs to shared memory
  SHM_MEDIUM = 1,
  /// write logs to file or read from
  FILE_MEDIUM = 3,  /*,
not implemented yet
  /// write logs to syslog
  SYSLOG_MEDIUM = 4,
  /// write logs to stdout
  PRINT_MEDIUM = 5,
  /// write logs to socket
  SOCKET_MEDIUM = 2,*/

} medium_type;

/*! Holds functions pointers and state of medium */
typedef struct medium_def
{
  /*! formater used for prints and parsing */
  struct formater_def * used_formatter;
  /*! discriminator of type_specific - what logger this actually is */
  medium_type type;
  /*! type specific data of medium like mem pointer or file handler */
  void * type_specific;
  /*! pointer to function for opening medium of specified type */
  syp_error (*open_medium) (struct medium_def *, int, const char **);
  /*! pointer to function for closing medium of specified type */
  syp_error (*close_medium) (struct medium_def *);
  /*! pointer to function which actually does operations on medium */
  syp_error (*access_medium) (struct medium_def *, log_struct);


  /*! position in circular log or number of read chars in non-circular */
  int64_t pos;

  /*! length of log */
  int64_t length;

  /*! if read or write */
  medium_operation kind;
} * medium;


#endif /*MEDIUM_H*/
