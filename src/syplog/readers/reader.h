#ifndef READER_H
#define READER_H

/*! \file
    \brief Main reader functions definitions. 

  Defines unified api for writing logs to distinct types of "media".
  Writer is represented as a structure (blackbox for user) which
  holds both state of reader and pointers to functions.
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
#include "formater-api.h"
#include "log-struct.h"


/*! Enum of known readers */
typedef enum
{
  /// don't use any reader - mainly used as "default" and "uninitialized"
  NO_READER = 0,
  /// write logs to file
  FILE_READER = 3  /*,
not implemented yet
  /// write logs to syslog
  SYSLOG_READER = 4,
  /// write logs to stdout
  PRINT_READER = 5
  /// write logs to shared memory
  SHM_READER = 1,
  /// write logs to socket
  SOCKET_READER = 2,*/

} reader_type;

/*! Holds functions pointers and state of reader */
typedef struct reader_def
{
  /*! formater used for prints */
  struct formater_def * input_parser;
  /*! discriminator of type_specific - what logger this actually is */
  reader_type type;
  /*! type specific data of reader like mem pointer or file handler */
  void * type_specific;
  /*! pointer to function for opening reader of specified type */
  syp_error (*open_reader) (struct reader_def *, int, char **);
  /*! pointer to function for closing reader of specified type */
  syp_error (*close_reader) (struct reader_def *);
  /*! pointer to function which actually writes log through reader */
  syp_error (*read_log) (struct reader_def *, log_struct);


  /*! position in circular log or number of read chars in non-circular */
  int64_t pos;

  /*! length of log */
  int64_t length;
} * reader;


#endif /*READER_H*/
