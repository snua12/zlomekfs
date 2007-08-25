#ifndef FILE_READER_H
#define FILE_READER_H

/*! \file
    \brief File reader functions definitions.  */

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

#include "reader-api.h"

/// default file to read logs from. used when no file given
#define DEFAULT_FILE	"/var/log/zfsd.log"

/// name of reader for translation from options (--type=file)
#define	FILE_READER_NAME	"file"

/// parameter name of output file name (where to write logs)
#define PARAM_READER_FN_LONG	"log-file"
/// short parameter name for PARAM_READER_FN_LONG - can be used only inside code for now
#define PARAM_READER_FN_CHAR	't'

/*! Structure that holds internal state info specific for file reader. */
typedef struct file_reader_specific_def {
  /// handler of opened file
  FILE * handler;
  /// name of file to write logs to (absolute or relative path)
  char file_name[FILE_NAME_LEN];
}* file_reader_specific;


/*! Parse params and initialize file reader
  @see open_reader
  @see reader-api.h
*/
syp_error open_file_reader (struct reader_def * target, int argc, char ** argv);

/*! Close file reader and free file reader specific structures
  @see close_reader
  @see reader-api.h
*/
syp_error close_file_reader (struct reader_def * target);

/*! Write log into a file.
  @see open_reader
  @see reader-api.h
*/
syp_error read_file_log (struct reader_def * target, log_struct log);

#endif /*FILE_READER_H*/
