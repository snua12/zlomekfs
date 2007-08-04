/*! \file
    \brief Main writer functions.  

  Implementation of writer type independent functions,
  mainly used as type switches, initializers, etc.
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

#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#include "writer.h"
#include "file-writer.h"

/*! Holds translations from stringified names of writers
    to writer_type discriminator
*/
struct 
{
  /// unified name of writer type
  char * name;
  /// writer type discriminator
  writer_type type;
}  writer_translation_table [] = 
{
/*
  {NO_WRITER_NAME,	NO_WRITER},
  {SHM_WRITER_NAME,	SHM_WRITER},
  {SOCKET_WRITER_NAME,	SOCKET_WRITER},
*/
  {FILE_WRITER_NAME,	FILE_WRITER},
/*
  {SYSLOG_WRITER_NAME,	SYSLOG_WRITER},
  {PRINT_WRITER_NAME,	PRINT_WRITER},
*/
  {NULL,		-1}
};

/// Translates writer type name to writer_type.
/*! Translates writer type name to writer_type discriminator.
  @param writer_type_name unified type name of writer
  @return type discriminator (writer_type) or NO_WRITER
*/
writer_type writer_name_to_enum (const char * writer_type_name) 
{
  // index to writer_translation_table for loop
  int table_index = 0;
#ifdef ENABLE_CHECKING
  if (writer_type_name == NULL)
    return NO_WRITER;
#endif
  
  // try to find type name in table
  for (table_index = 0;
       writer_translation_table[table_index].name != NULL;
       table_index ++)
  {
    if (strncmp(writer_type_name, writer_translation_table[table_index].name,
                WRITER_NAME_LEN) == 0)
      break;
  }

  // if we end inside of table (not a terminator) we found a writer
  if (table_index >= 0 && writer_translation_table[table_index].name != NULL)
  {
    return writer_translation_table[table_index].type;
  }
  else
    return NO_WRITER;
}

/// Parse type independent parameters of writer to structure
/*! Parses parameters of writer and initialize settings 
  @param argc number of items in argv
  @param argv parameters in std format as in "main" (non NULL)
  @param settings pointer to struct, where to store settings given in argv
  @return ERR_BAD_PARAMS on invalid argument in argv, NOERR otherwise
*/
syp_error writer_parse_params(int argc, const char ** argv, writer settings)
{

  // table of known options
  const struct option option_table[] = 
  {
    {PARAM_WRITER_LS_LONG,	1, NULL,			PARAM_WRITER_LS_CHAR},
    {PARAM_WRITER_TYPE_LONG,	1, NULL,			PARAM_WRITER_TYPE_CHAR},
    {PARAM_WRITER_FMT_LONG,	1, NULL,			PARAM_WRITER_FMT_CHAR},

    {NULL, 0, NULL, 0}
  }; 

  int opt;
  extern int optind, opterr, optopt;

#ifdef ENABLE_CHECKING
  if (argv == NULL || settings == NULL)
    return ERR_BAD_PARAMS;
#endif

  // we need to "init" structures of getopt library
  optind=0;
	
  while ((opt = getopt_long(argc, (char**)argv, "", option_table, NULL)) != -1)
    switch(opt){
      case PARAM_WRITER_TYPE_CHAR: // writer type
        settings->type = writer_name_to_enum (optarg);
        break;
      case PARAM_WRITER_LS_CHAR: // log size
        settings->length = strtoll(optarg, NULL, 10);
        if (errno != NOERR)
        {
          return ERR_BAD_PARAMS;
        }
        break;
      case PARAM_WRITER_FMT_CHAR: // formater type
        settings->output_printer = formater_for_name (optarg);
        if (settings->output_printer == NULL)
        {
          return ERR_BAD_PARAMS;
        }
        break;
      case '?':
      default:
          return ERR_BAD_PARAMS;
        break;
    }

  return NOERR;
}

/// Initialize writer structure.
syp_error open_writer (struct writer_def * target, int argc, char ** argv)
{
  syp_error ret_code = NOERR;

#ifdef ENABLE_CHECKING
  if (argv == NULL || target == NULL)
    return ERR_BAD_PARAMS;
#endif
  
  //TODO init

  ret_code = writer_parse_params (argc, (const char **)argv, target);
  if (ret_code != NOERR)
  {
    goto FINISHING;
  }
  
  switch( target->type)
  {
    case NO_WRITER:
      break;
    case FILE_WRITER:
      ret_code = open_file_writer (target, argc, argv);
      if (ret_code != NOERR)
      {
        goto FINISHING;
      }
      break;

    default:
      break;
  }

FINISHING:
  return ret_code;
}

/// Close writer and free internal pointers
syp_error close_writer (writer target)
{
  syp_error ret_code = NOERR;

#ifdef ENABLE_CHECKING
  if (target == NULL || target->close_writer == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  ret_code = target->close_writer (target);
  if (ret_code != NOERR)
  {
    goto FINISHING;
  }
  else
  {
    free (target);
    target = NULL;
  }

FINISHING:

  return ret_code;
}

/// Write message to log.
syp_error write_log (writer target, log_struct log)
{
#ifdef ENABLE_CHECKING
  if (target == NULL || target->write_log == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  return target->write_log (target, log);
}


