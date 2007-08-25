/*! \file
    \brief Main reader functions.  

  Implementation of reader type independent functions,
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

#include "reader.h"
#include "file-reader.h"

/*! Holds translations from stringified names of readers
    to reader_type discriminator
*/
struct 
{
  /// unified name of reader type
  char * name;
  /// reader type discriminator
  reader_type type;
}  reader_translation_table [] = 
{
/*
  {NO_READER_NAME,	NO_READER},
  {SHM_READER_NAME,	SHM_READER},
  {SOCKET_READER_NAME,	SOCKET_READER},
*/
  {FILE_READER_NAME,	FILE_READER},
/*
  {SYSLOG_READER_NAME,	SYSLOG_READER},
  {PRINT_READER_NAME,	PRINT_READER},
*/
  {NULL,		-1}
};

/// Translates reader type name to reader_type.
/*! Translates reader type name to reader_type discriminator.
  @param reader_type_name unified type name of reader
  @return type discriminator (reader_type) or NO_READER
*/
reader_type reader_name_to_enum (const char * reader_type_name) 
{
  // index to reader_translation_table for loop
  int table_index = 0;
#ifdef ENABLE_CHECKING
  if (reader_type_name == NULL)
    return NO_READER;
#endif
  
  // try to find type name in table
  for (table_index = 0;
       reader_translation_table[table_index].name != NULL;
       table_index ++)
  {
    if (strncmp(reader_type_name, reader_translation_table[table_index].name,
                READER_NAME_LEN) == 0)
      break;
  }

  // if we end inside of table (not a terminator) we found a reader
  if (table_index >= 0 && reader_translation_table[table_index].name != NULL)
  {
    return reader_translation_table[table_index].type;
  }
  else
    return NO_READER;
}

/// Parse type independent parameters of reader to structure
/*! Parses parameters of reader and initialize settings 
  @param argc number of items in argv
  @param argv parameters in std format as in "main" (non NULL)
  @param settings pointer to struct, where to store settings given in argv
  @return ERR_BAD_PARAMS on invalid argument in argv, NOERR otherwise
*/
syp_error reader_parse_params(int argc, const char ** argv, reader settings)
{

  // table of known options
  const struct option option_table[] = 
  {
    {PARAM_READER_TYPE_LONG,	1, NULL,			PARAM_READER_TYPE_CHAR},
    {PARAM_READER_FMT_LONG,	1, NULL,			PARAM_READER_FMT_CHAR},

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
      case PARAM_READER_TYPE_CHAR: // reader type
        settings->type = reader_name_to_enum (optarg);
        break;
      case PARAM_READER_FMT_CHAR: // formater type
        settings->input_parser = formater_for_name (optarg);
        if (settings->input_parser == NULL)
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

/// Initialize reader structure.
syp_error open_reader (struct reader_def * target, int argc, char ** argv)
{
  syp_error ret_code = NOERR;

#ifdef ENABLE_CHECKING
  if (argv == NULL || target == NULL)
    return ERR_BAD_PARAMS;
#endif
  
  //TODO init

  ret_code = reader_parse_params (argc, (const char **)argv, target);
  if (ret_code != NOERR)
  {
    goto FINISHING;
  }
  
  switch( target->type)
  {
    case NO_READER:
      break;
    case FILE_READER:
      ret_code = open_file_reader (target, argc, argv);
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

/// Close reader and free internal pointers
syp_error close_reader (reader target)
{
  syp_error ret_code = NOERR;

#ifdef ENABLE_CHECKING
  if (target == NULL || target->close_reader == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  ret_code = target->close_reader (target);
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
syp_error read_log (reader target, log_struct log)
{
#ifdef ENABLE_CHECKING
  if (target == NULL || target->read_log == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  return target->read_log (target, log);
}


