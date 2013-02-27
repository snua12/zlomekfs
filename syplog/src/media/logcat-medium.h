#ifndef SHM_MEDIUM_H
#define SHM_MEDIUM_H

/* ! \file \brief Shared memory accessor (for reader and writer) functions
   definitions.  */

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Syplog.

   Syplog is free software; you can redistribute it and/or modify it under the 
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   Syplog is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with Syplog; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */


#include <stdio.h>

#include "medium-api.h"
#include "syp-error.h"

#define LOGCAT_MEDIUM_NAME "logcat"

/* ! Check if argument is recognized by shared memory medium @param arg
   command line argument (in format --argument_name=value) @return TRUE in
   case of recognition, FALSE otherwise */
bool_t is_logcat_medium_arg(const char *arg);


/* ! Parse params and initialize logcat segment @see open_medium @see
   medium-api.h */
syp_error open_logcat_medium(struct medium_def *target, int argc,
						  const char **argv);

/* ! Close logcat medium and free logcat medium internals. Doesn't destroy logcat
   segment. @see close_medium @see medium-api.h */
syp_error close_logcat_medium(struct medium_def *target);

/* ! read log from a shared memory segment. @see access_medium @see
   medium-api.h */
syp_error logcat_access(struct medium_def *source, log_struct log);

/* ! prints logcat options help to fd. @see print_medium_help @see medium-api.h */
void print_logcat_medium_help(int fd, int tabs);

#endif /* SHM_MEDIUM_H */
