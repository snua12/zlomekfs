#ifndef FILE_MEDIUM_H
#define FILE_MEDIUM_H

/*! \file
    \brief File medium functions definitions.  */

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
#define _GNU_SOURCE
#include <stdio.h>
#undef _GNU_SOURCE

#include "log-constants.h"
#include "medium-api.h"

/// default file to read logs from or write to. used when no file given
#define DEFAULT_FILE	"/var/log/zfsd.log"

/// name of medium for translation from options (--medium=file)
#define	FILE_MEDIUM_NAME	"file"

/// parameter name of input / output file name
#define PARAM_MEDIUM_FN_LONG	"log-file"
/// short parameter name for PARAM_READER_FN_LONG - can be used only inside code for now
#define PARAM_MEDIUM_FN_CHAR	't'

/*! Structure that holds internal state info specific for file medium. */
typedef struct file_medium_def {
  /// handler of opened file
  FILE * handler;
  /// name of file to write logs to (absolute or relative path)
  char file_name[FILE_NAME_LEN];
}* file_medium;


/*! Parse params and initialize file medium
  @see open_medium
  @see medium-api.h
*/
syp_error open_file_medium (struct medium_def * target, int argc, char ** argv);

/*! Close file medium and free file medium specific structures
  @see close_medium
  @see medium-api.h
*/
syp_error close_file_medium (struct medium_def * target);

/*! do operation on log
  @see access_medium
  @see medium-api.h
*/
syp_error file_access (struct medium_def * target, log_struct log);

/*! prints file options help to fd.
  @see print_medium_help
  @see medium-api.h
*/
void print_file_medium_help (int fd, int tabs);

#endif /*FILE_MEDIUM_H*/
