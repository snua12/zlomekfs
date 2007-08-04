#ifndef WRITER_H
#define WRITER_H

/*! \file
    \brief Main writer functions definitions. 

  Defines unified api for writing logs to distinct types of "media".
  Writer is represented as a structure (blackbox for user) which
  holds both state of writer and pointers to functions.
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


/*! Enum of known writers */
typedef enum
{
  /// don't use any writer - mainly used as "default" and "uninitialized"
  NO_WRITER = 0,
  /// write logs to file
  FILE_WRITER = 3  /*,
not implemented yet
  /// write logs to syslog
  SYSLOG_WRITER = 4,
  /// write logs to stdout
  PRINT_WRITER = 5
  /// write logs to shared memory
  SHM_WRITER = 1,
  /// write logs to socket
  SOCKET_WRITER = 2,*/

} writer_type;

/*! Holds functions pointers and state of writer */
typedef struct writer_def
{
  /*! formater used for prints */
  struct formater_def * output_printer;
  /*! discriminator of type_specific - what logger this actually is */
  writer_type type;
  /*! type specific data of writer like mem pointer or file handler */
  void * type_specific;
  /*! pointer to function for opening writer of specified type */
  syp_error (*open_writer) (struct writer_def *, int, char **);
  /*! pointer to function for closing writer of specified type */
  syp_error (*close_writer) (struct writer_def *);
  /*! pointer to function which actually writes log through writer */
  syp_error (*write_log) (struct writer_def *, log_struct);

  /*! maximum length of log. when non zero, log is used as circular */
  int64_t length;
  /*! position in circular log or number of printed chars in non-circular */
  int64_t pos;
} * writer;


#endif /*WRITER_H*/
