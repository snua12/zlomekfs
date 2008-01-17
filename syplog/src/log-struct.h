#ifndef LOG_STRUCT_H
#define LOG_STRUCT_H

/*! \file
    \brief Definition of in-memory log structure.  

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

#include <pthread.h>
#include <sys/time.h>

#include "log-constants.h"
#include "facility.h"
#include "log-level.h"

// FIXME -use std constant

/// max length of intiger printed to string
#define	INT_STRING_SIZE	15
/// max length of long long intiger printed to string
#define LONG_LONG_STRING_SIZE	30
/// max length of time printed to string
#define	TIME_STRING_LEN	20
/// max length of timezone printed to string
#define	TIMEZONE_STRING_LEN	10

/// max length of log printed to string
#define MAX_LOG_STRING_SIZE	INT_STRING_SIZE + LONG_LONG_STRING_SIZE + HOSTNAME_LEN \
                        + NODE_NAME_LEN + THREAD_NAME_LEN + LOG_MESSAGE_LEN \
                        + TIME_STRING_LEN + TIMEZONE_STRING_LEN + FACILITY_STRING_LEN \
                        + LOG_LEVEL_STRING_LEN

/*! Structure holding all informations about logging event */
typedef struct log_struct_def {
  /// severity of event
  log_level_t level;
  /// which part of application apply message to
  facility_t facility;
  /// user given info about event
  char message[LOG_MESSAGE_LEN];
  /// id of thread which has generated this event
  pthread_t thread_id;
  /// thread name (if applicable)
  char thread_name[THREAD_NAME_LEN];
  /// time when event occured
  struct timeval time;

  /* unused */
  /// name of zfs node
  char node_name[NODE_NAME_LEN];
  /// hostname of machine logging
  char hostname[HOSTNAME_LEN];
  /// timezone ;)
  uint64_t timezone;
  /* /unused */
} *log_struct;

/// static initializer of log struct - initialize all fields to defaults
#define	LOG_STRUCT_STATIC_INITIALIZER	{ 	\
	.level = LOG_LOOPS,			\
	.facility = FACILITY_NOTHING,		\
	.message = "",				\
	.thread_id = 0,				\
	.thread_name = "",			\
	.node_name = "",			\
	.hostname = "",				\
	.timezone = 0				\
					}






#endif /*LOG_STRUCT_H*/
