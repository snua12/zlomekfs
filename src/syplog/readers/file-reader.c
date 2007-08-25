/*! \file
    \brief File reader implementation.  

  File reader takes logs from file and parses them to structures.
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

#define _GNU_SOURCE
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "file-reader.h"

/// Parse file reader specific params.
/*! Parse file reader specific params
 @param argc argv count
 @param argv std "main" format params (--log-file=/var/log/zfsd.log) (non NULL)
 @param settings reader structure where to apply given settings (non NULL)
 @return ERR_BAD_PARAMS on wrong argv or settings, NOERR otherwise
 */
syp_error file_reader_parse_params (int argc, const char ** argv, reader settings)
{
  // table with known param types
  const struct option option_table[] = 
  {
    {PARAM_READER_FN_LONG,	1, NULL,			PARAM_READER_FN_CHAR},
    {NULL, 0, NULL, 0}
  }; 
  
  int opt;
  extern int optind, opterr, optopt;
  
#ifdef ENABLE_CHECKING
  if (argv == NULL || settings == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  // initialize getopt index to params
  optind=1;

  while ( (opt = getopt_long (argc, (char**)argv, "", option_table, NULL)) != -1)
    switch(opt)
    {
      case PARAM_READER_FN_CHAR: // log file name
        strncpy ( ((file_reader_specific)(settings->type_specific))->file_name,
                  optarg, 
                  FILE_NAME_LEN);
        break;
      case '?':
      default:
        // skip unknown options
        break;
    }
	return NOERR;
}

/// Initializes file reader specific parts of reader structure
syp_error open_file_reader (reader target, int argc, char ** argv)
{
  syp_error ret_code = NOERR;
  file_reader_specific new_specific = NULL;

#ifdef ENABLE_CHECKING
  if (target == NULL || argv == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif
  
  new_specific = malloc (sizeof (struct file_reader_specific_def));
  if (new_specific == NULL)
  {
    goto FINISHING;
  }
  else
  {
    target->type_specific = new_specific;
    strncpy (new_specific->file_name, DEFAULT_FILE, FILE_NAME_LEN);
    new_specific->handler = NULL;
  }

  ret_code = file_reader_parse_params (argc, (const char **)argv, target);
  if (ret_code != NOERR)
  {
    goto FINISHING;
  }
  new_specific->handler = fopen (new_specific->file_name, "r");
  if (new_specific->handler == NULL)
  {
    ret_code = ERR_FILE_OPEN;
    goto FINISHING;
  }

  target->open_reader = open_file_reader;
  target->close_reader = close_file_reader;
  target->read_log = read_file_log;

FINISHING:
  if (ret_code != NOERR && new_specific!= NULL)
  {
    free (new_specific);
    target->type_specific = NULL;
  }

  return ret_code;
}

/// Close and destroys file reader specific parts of reader strucutre
syp_error close_file_reader (reader target){
#ifdef ENABLE_CHECKING
  if (target == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  fclose ( ((file_reader_specific)(target->type_specific))->handler);
  free (target->type_specific);
  target->type_specific = NULL;
  
  return NOERR;
}

/// Write log to file
syp_error read_file_log (reader target, log_struct log){
#ifdef ENABLE_CHECKING
  if (target == NULL || log == NULL)
    return ERR_BAD_PARAMS;
#endif
  // check boundaries
  if (feof (((file_reader_specific)target->type_specific)->handler) != 0)
  {
    return ERR_END_OF_FILE;
  }

  // read
  int32_t chars_read = target->input_parser->file_read (log, 
                                ((file_reader_specific)target->type_specific)->handler);
  if (chars_read > 0)
  {
  // move boundary
    target->pos += chars_read;
    return NOERR;
  }
  else
    return -chars_read;
}
