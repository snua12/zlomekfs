/*! \file
    \brief File writer implementation.  

  File writer takes logs and prints them to defined file.
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

#undef _GNU_SOURCE

#include "file-writer.h"

/// Parse file writer specific params.
/*! Parse file writer specific params
 @param argc argv count
 @param argv std "main" format params (--log-file=/var/log/zfsd.log) (non NULL)
 @param settings writer structure where to apply given settings (non NULL)
 @return ERR_BAD_PARAMS on wrong argv or settings, NOERR otherwise
 */
syp_error file_writer_parse_params (int argc, const char ** argv, writer settings)
{
  // table with known param types
  const struct option option_table[] = 
  {
    {PARAM_WRITER_FN_LONG,	1, NULL,			PARAM_WRITER_FN_CHAR},
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
      case PARAM_WRITER_FN_CHAR: // log file name
        strncpy ( ((file_writer_specific)(settings->type_specific))->file_name,
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

/// Initializes file writer specific parts of writer structure
syp_error open_file_writer (writer target, int argc, char ** argv)
{
  syp_error ret_code = NOERR;
  file_writer_specific new_specific = NULL;

#ifdef ENABLE_CHECKING
  if (target == NULL || argv == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif
  
  new_specific = malloc (sizeof (struct file_writer_specific_def));
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

  ret_code = file_writer_parse_params (argc, (const char **)argv, target);
  if (ret_code != NOERR)
  {
    goto FINISHING;
  }
  new_specific->handler = fopen (new_specific->file_name, "r+");
  if (new_specific->handler == NULL)
    // heuristic for non-existing file FIXME: find appropriate errno and do this only upon it
    new_specific->handler = fopen (new_specific->file_name, "w+");
  if (new_specific->handler == NULL)
  {
    ret_code = ERR_FILE_OPEN;
    goto FINISHING;
  }

  // set cursor in file to boundaries
  long pos = ftell (new_specific->handler);
  target->pos = pos;
  if (target->length > 0)
  {
    if (target->length - pos < target->output_printer->get_max_print_size())
    {
      if (fseek (new_specific->handler, 0, SEEK_SET) == SYS_NOERR)
        target->pos = 0;
    }
  }
  else
  {
    fseek (new_specific->handler, 0, SEEK_END);
  }

  target->open_writer = open_file_writer;
  target->close_writer = close_file_writer;
  target->write_log = write_file_log;

FINISHING:
  if (ret_code != NOERR && new_specific!= NULL)
  {
    free (new_specific);
    target->type_specific = NULL;
  }

  return ret_code;
}

/// Close and destroys file writer specific parts of writer strucutre
syp_error close_file_writer (writer target){
#ifdef ENABLE_CHECKING
  if (target == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  fclose ( ((file_writer_specific)(target->type_specific))->handler);
  free (target->type_specific);
  target->type_specific = NULL;
  
  return NOERR;
}

/// Write log to file
syp_error write_file_log (writer target, log_struct log){
#ifdef ENABLE_CHECKING
  if (target == NULL || log == NULL)
    return ERR_BAD_PARAMS;
#endif
  // TODO: implement
  // check boundaries
  if (target->length > 0 && 
      target->length - target->pos < target->output_printer->get_max_print_size())
  {
  // move to front
    if (fseek (((file_writer_specific)target->type_specific)->handler,
               0, SEEK_SET) == SYS_NOERR)
      target->pos = 0;
    else
      return ERR_SYSTEM;
  }
  // append
  int32_t chars_printed = target->output_printer->file_write (log, 
                                ((file_writer_specific)target->type_specific)->handler);
  if (chars_printed > 0)
  {
  // move boundary
    target->pos += chars_printed;
    return NOERR;
  }
  else
    return -chars_printed;
}
