/*! \file
    \brief Print medium implementation.

  Print medium prints logs to standard output.
*/

/* Copyright (C) 2010 Rastislav Wartiak

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

#include "print-medium.h"


void print_print_medium_help (int fd, int tabs)
{
  if (fd == 0)
    fd = 1;

  tabize_print (tabs, fd, "print medium writes logs to standard output.\n");
  tabize_print (tabs, fd, "print medium options:\n");
  tabs ++;

}

// table with known param types
static const struct option option_table[] =
{
  {NULL, 0, NULL, 0}
};


/// Parse print medium specific params.
/*! Parse print medium specific params
 @param argc argv count
 @param argv ignored now
 @param settings medium structure where to apply given settings (non NULL)
 @return ERR_BAD_PARAMS on wrong argv or settings, NOERR otherwise
 */
syp_error print_medium_parse_params (int argc, const char ** argv, medium settings)
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
      case '?':
      default:
        // skip unknown options
        break;
    }
	return NOERR;
}

/// Initializes print medium specific parts of medium structure
syp_error open_print_medium (medium target, int argc, const char ** argv)
{
  syp_error ret_code = NOERR;
  print_medium new_specific = NULL;

#ifdef ENABLE_CHECKING
  if (target == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  new_specific = malloc (sizeof (struct print_medium_def));
  if (new_specific == NULL)
  {
    goto FINISHING;
  }
  else
  {
    target->type_specific = new_specific;
    new_specific->handler = NULL;
  }

  if (argv != NULL)
  	ret_code = print_medium_parse_params (argc, (const char **)argv, target);
  if (ret_code != NOERR)
  {
    goto FINISHING;
  }

  switch (target->kind)
  {
    case READ_LOG:
      new_specific->handler = stdin;
      break;
    case WRITE_LOG:
      new_specific->handler = stdout;
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

  target->open_medium = open_print_medium;
  target->close_medium = close_print_medium;
  target->access_medium = print_access;

FINISHING:
  if (ret_code != NOERR && new_specific!= NULL)
  {
    free (new_specific);
    target->type_specific = NULL;
  }

  return ret_code;
}

/// Close and destroys print medium specific parts of medium strucutre
syp_error close_print_medium (medium target){
#ifdef ENABLE_CHECKING
  if (target == NULL)
  {
    return ERR_BAD_PARAMS;
  }
  if (target->kind == NO_OPERATION)
    return ERR_NOT_INITIALIZED;
#endif

  free (target->type_specific);
  target->type_specific = NULL;

  return NOERR;
}

/// do operation
syp_error print_access (medium target, log_struct log){
#ifdef ENABLE_CHECKING
  if (target == NULL || log == NULL)
    return ERR_BAD_PARAMS;
  if (target->kind == NO_OPERATION ||target->used_formatter == NULL ||
    target->type_specific == NULL || log == NULL)
    return ERR_NOT_INITIALIZED;
#endif
  // check boundaries
  int chars_accessed = 0;
  switch (target->kind)
  {
    case READ_LOG: // read
      chars_accessed = target->used_formatter->file_read (log,
        ((print_medium)target->type_specific)->handler);
      break;
    case WRITE_LOG: // write
      chars_accessed = target->used_formatter->file_write (log,
        ((print_medium)target->type_specific)->handler);
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
  else if (chars_accessed <= 0)
    return -chars_accessed;
}
