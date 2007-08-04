#ifndef FILE_WRITER_H
#define FILE_WRITER_H

/*! \file
    \brief File writer functions definitions.  */

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

#include "writer-api.h"

/// default file to write logs in. used when no file given
#define DEFAULT_FILE	"/var/log/zfsd.log"

/// name of writer for translation from options (--type=file)
#define	FILE_WRITER_NAME	"file"

/// parameter name of output file name (where to write logs)
#define PARAM_WRITER_FN_LONG	"output-file"
/// short parameter name for PARAM_WRITER_FN_LONG - can be used only inside code now
#define PARAM_WRITER_FN_CHAR	't'

/*! Structure that holds internal state info specific for file writer. */
typedef struct file_writer_specific_def {
  /// handler of opened file
  FILE * handler;
  /// name of file to write logs to (absolute or relative path)
  char file_name[FILE_NAME_LEN];
}* file_writer_specific;


/*! Parse params and initialize file writer
  @see open_writer
  @see writer-api.h
*/
syp_error open_file_writer (struct writer_def * target, int argc, char ** argv);

/*! Close file writer and free file writer specific structures
  @see close_writer
  @see writer-api.h
*/
syp_error close_file_writer (struct writer_def * target);

/*! Write log into a file.
  @see open_writer
  @see writer-api.h
*/
syp_error write_file_log (struct writer_def * target, log_struct log);

#endif /*FILE_WRITER_H*/
