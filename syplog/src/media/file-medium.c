/*! \file
    \brief File medium implementation.  

  File medium takes logs from file and parses them to structures (or vice versa).
*/

/* Copyright (C) 2007, 2008 Jiri Zouhar

   This file is part of Syplog.

   Syplog is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   Syplog is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License along with
   Syplog; see the file COPYING.  If not, write to the Free Software Foundation,
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

#include "file-medium.h"


void print_file_medium_help (int fd, int tabs)
{
  if (fd == 0)
    fd = 1;
  
  tabize_print (tabs, fd, "file medium writes logs to file (reads from).\n");
  tabize_print (tabs, fd, "file medium options:\n");
  tabs ++;
  
  tabize_print (tabs, fd, "--%s=value, -%c value\tfile name\n",
    PARAM_MEDIUM_FN_LONG, PARAM_MEDIUM_FN_CHAR);
  
}

// table with known param types
static const struct option option_table[] = 
{
  {PARAM_MEDIUM_FN_LONG,	1, NULL,			PARAM_MEDIUM_FN_CHAR},
  {NULL, 0, NULL, 0}
}; 


bool_t is_file_medium_arg (const char * arg)
{
  return opt_table_contains ((struct option *)option_table, arg);
}

/// Parse file medium specific params.
/*! Parse file medium specific params
 @param argc argv count
 @param argv std "main" format params (--log-file=/var/log/zfsd.log) (non NULL)
 @param settings medium structure where to apply given settings (non NULL)
 @return ERR_BAD_PARAMS on wrong argv or settings, NOERR otherwise
 */
syp_error file_medium_parse_params (int argc, const char ** argv, medium settings)
{
  int opt;
  extern int opterr, optopt;
  
#ifdef ENABLE_CHECKING
  if (argv == NULL || settings == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  // initialize getopt index to params
  optind=0;

  while ( (opt = getopt_long (argc, (char**)argv, "", option_table, NULL)) != -1)
    switch(opt)
    {
      case PARAM_MEDIUM_FN_CHAR: // log file name
        strncpy ( ((file_medium)(settings->type_specific))->file_name,
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

/// Initializes file medium specific parts of medium structure
syp_error open_file_medium (medium target, int argc, const char ** argv)
{
  syp_error ret_code = NOERR;
  file_medium new_specific = NULL;

#ifdef ENABLE_CHECKING
  if (target == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif
  
  new_specific = malloc (sizeof (struct file_medium_def));
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

  if (argv != NULL)
  	ret_code = file_medium_parse_params (argc, (const char **)argv, target);
  if (ret_code != NOERR)
  {
    goto FINISHING;
  }

  switch (target->kind)
  {
    case READ_LOG:
      new_specific->handler = fopen (new_specific->file_name, "r");
      break;
    case WRITE_LOG:
      new_specific->handler = fopen (new_specific->file_name, "r+");
      if (new_specific->handler == NULL)
        // heuristic for non-existing file FIXME: find appropriate errno and do this only upon it
        new_specific->handler = fopen (new_specific->file_name, "w+");
      break;
    default:
      ret_code = ERR_NOT_INITIALIZED;
      goto FINISHING;
      break;
  }

  
  if (new_specific->handler == NULL)
  {
    ret_code = ERR_FILE_OPEN;
    goto FINISHING;
  }

  if (target->kind == WRITE_LOG)
  {
    // set cursor in file to boundaries
    long pos = ftell (new_specific->handler);
    target->pos = pos;
    if (target->length > 0)
    {
      if (target->length - pos < target->used_formatter->get_max_print_size())
      {
        if (fseek (new_specific->handler, 0, SEEK_SET) == SYS_NOERR)
          target->pos = 0;
       }
    }
    else
    {
      fseek (new_specific->handler, 0, SEEK_END);
    }
  }


  target->open_medium = open_file_medium;
  target->close_medium = close_file_medium;
  target->access_medium = file_access;

FINISHING:
  if (ret_code != NOERR && new_specific!= NULL)
  {
    free (new_specific);
    target->type_specific = NULL;
  }

  return ret_code;
}

/// Close and destroys file medium specific parts of medium strucutre
syp_error close_file_medium (medium target){
#ifdef ENABLE_CHECKING
  if (target == NULL)
  {
    return ERR_BAD_PARAMS;
  }
  if (target->kind == NO_OPERATION)
    return ERR_NOT_INITIALIZED;
#endif

  fclose ( ((file_medium)(target->type_specific))->handler);
  free (target->type_specific);
  target->type_specific = NULL;
  
  return NOERR;
}

/// do operation
syp_error file_access (medium target, log_struct log){
#ifdef ENABLE_CHECKING
  if (target == NULL || log == NULL)
    return ERR_BAD_PARAMS;
  if (target->kind == NO_OPERATION ||target->used_formatter == NULL ||
    target->type_specific == NULL || log == NULL)
    return ERR_NOT_INITIALIZED;
#endif
  // check boundaries
  if (feof (((file_medium)target->type_specific)->handler) != 0)
  {
    clearerr (((file_medium)target->type_specific)->handler);
    return ERR_END_OF_LOG;
  }

  int chars_accessed = 0;
  switch (target->kind)
  {
    case READ_LOG: // read
      chars_accessed = target->used_formatter->file_read (log, 
        ((file_medium)target->type_specific)->handler);
      break;
    case WRITE_LOG: // write
      chars_accessed = target->used_formatter->file_write (log, 
        ((file_medium)target->type_specific)->handler);
      break;
    case NO_OPERATION:
    default:
      break;
  }

  if (chars_accessed > 0)
  {
  // move boundary
    target->pos += chars_accessed;
    return NOERR;
  }
  else
    return -chars_accessed;
}
