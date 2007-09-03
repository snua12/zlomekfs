/*! \file
    \brief Shared memory reader implementation.  

  Shm reader parses logs from shared memory segment into log structure.
  TODO: describe behaviour of fixed sizes (of max size)
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

#undef _GNU_SOURCE

#include "file-reader.h"

/// Parse shm reader specific params.
/*! Parse shm reader specific params
 @param argc argv count
 @param argv std "main" format params (--segment-key=1024) (non NULL)
 @param settings reader structure where to apply given settings (non NULL)
 @return ERR_BAD_PARAMS on wrong argv or settings, NOERR otherwise
 */
syp_error shm_reader_parse_params (int argc, const char ** argv, reader settings)
{
  // table with known param types
  const struct option option_table[] = 
  {
    {PARAM_READER_SK_LONG,	1, NULL,			PARAM_READER_SK_CHAR},
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
  optind=0;

  while ( (opt = getopt_long (argc, (char**)argv, "", option_table, NULL)) != -1)
    switch(opt)
    {
      case PARAM_READER_SK_CHAR: // log file name
        (shm_reader_specific)(settings->type_specific))->segment_key = atoi (optarg);
        break;
      case '?':
      default:
        // skip unknown options
        break;
    }
	return NOERR;
}

/// Initializes shm reader specific parts of reader structure
syp_error open_shm_reader (reader target, int argc, char ** argv)
{
  syp_error ret_code = NOERR;
  shm_reader_specific new_specific = NULL;

#ifdef ENABLE_CHECKING
  if (target == NULL || argv == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif
  
  if (target->length <= 0)
    target->length = DEFAULT_SHM_LOG_SIZE;
  target->pos = 0;
  new_specific = malloc (sizeof (struct shm_reader_specific_def));
  if (new_specific == NULL)
  {
    goto FINISHING;
  }
  else
  {
    target->type_specific = new_specific;
    new_specific->segment_key = DEFAULT_SHM_KEY;
    new_specific->shm_start = NULL;
    new_specific->shmid = INVALID_SHM_ID;
  }

  ret_code = shm_reader_parse_params (argc, (const char **)argv, target);
  if (ret_code != NOERR)
  {
    goto FINISHING;
  }
  
  // NOTE: SILENT shrinkage
  if (target->length > SHMMAX)
    target->length = SHMMAX;
  new_specific->shmid = shmget (new_specific->segment_key, 
                                target->length,
                                440);
  if (new_specific->shmid == INVALID_SHM_ID)
  {
    ret_code = system_to_syp_error (errno);
    goto FINISHING;
  }

  new_specific->shm_start = shmat (new_specific->shmid, NULL, 0);
  if (new_specific->shm_start == (void *) -1)
  {
    new_specific->shm_start = NULL;
    ret_code = system_to_syp_error (errno);
    goto FINISHING;
  }

  target->open_reader = open_shm_reader;
  target->close_reader = close_shm_reader;
  target->read_log = read_shm_log;

FINISHING:
  if (ret_code != NOERR && new_specific!= NULL)
  {
    if (new_specific->shm_start != NULL)
      shmdt (new_specific->shm_start);
    free (new_specific);
    target->type_specific = NULL;
  }

  return ret_code;
}

/// Close and destroys shm reader specific parts of reader strucutre
syp_error close_shm_reader (reader target){
#ifdef ENABLE_CHECKING
  if (target == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  shmdt (new_specific->shm_start);
  free (target->type_specific);
  target->type_specific = NULL;
  
  return NOERR;
}

/// Read log from shared memory segment
syp_error read_shm_log (reader target, log_struct log){
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
    target->pos = 0;
  }
  // read
  int32_t chars_read = target->input_parser->mem_read (log,
                                ((shm_reader_specific)target->type_specific)->shm_start + pos);
  if (chars_read > 0)
  {
  // move boundary
    target->pos += target->input_parser->get_max_print_size();
    return NOERR;
  }
  else
    return -chars_read;
}
