/*! \file
    \brief Shared memory reader implementation.  

  Shm medium handles low level access to shared memory for readers and writers.
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
#include <errno.h>

#undef _GNU_SOURCE

#include "shm-medium.h"

void print_shm_medium_help (int fd, int tabs)
{
  if (fd == 0)
    fd = 1;
  
  tabize_print (tabs, fd, "shm medium writes log to shared memory segment (reads from).\n");
  tabize_print (tabs, fd, "shm medium options:\n");
  tabs ++;
  
  tabize_print (tabs, fd, "--%s=value, -%c value\tshared memory segment key\n",
    PARAM_SHM_KEY_LONG, PARAM_SHM_KEY_CHAR);
  
}

  // table with known param types
static const struct option option_table[] = 
  {
    {PARAM_SHM_KEY_LONG,	1, NULL,			PARAM_SHM_KEY_CHAR},
    {NULL, 0, NULL, 0}
  }; 

bool_t is_shm_medium_arg (const char * arg)
{
  return opt_table_contains ((struct option *)option_table, arg);
}

/// Parse shm medium specific params
/*! Parse shm medium specific params
 @param argc argv count
 @param argv std "main" format params (--segment-key=1024) (non NULL)
 @param settings medium structure where to apply given settings (non NULL)
 @return ERR_BAD_PARAMS on wrong argv or settings, NOERR otherwise
 */
syp_error shm_medium_parse_params (int argc, const char ** argv, medium settings)
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
      case PARAM_SHM_KEY_CHAR: // log file name
        ((shm_medium)(settings->type_specific))->segment_key = atoi (optarg);
        break;
      case '?':
      default:
        // skip unknown options
        break;
    }
	return NOERR;
}

#define	READ_PERMISSIONS	440
#define	WRITE_PERMISSIONS	660

/// Initializes shm specific parts of reader structure
syp_error open_shm_medium (medium target, int argc, const char ** argv)
{
  syp_error ret_code = NOERR;
  shm_medium new_specific = NULL;
  int permissions = 0;

#ifdef ENABLE_CHECKING
  if (target == NULL || argv == NULL)
  {
    return ERR_BAD_PARAMS;
  }

  if (target->kind == NO_OPERATION)
  {
    return ERR_NOT_INITIALIZED;
  }
#endif
  
  if (target->length <= 0)
    target->length = DEFAULT_SHM_SIZE;
  target->pos = 0;
  new_specific = malloc (sizeof (struct shm_medium_def));
  if (new_specific == NULL)
  {
    ret_code = ERR_SYSTEM;
    goto FINISHING;
  }
  else
  {
    memset (new_specific, 0, sizeof(struct shm_medium_def));

    target->type_specific = new_specific;
    new_specific->segment_key = DEFAULT_SHM_KEY;
    new_specific->shm_start = NULL;
    new_specific->shmid = INVALID_SHM_ID;
  }

  ret_code = shm_medium_parse_params (argc, (const char **)argv, target);
  if (ret_code != NOERR)
  {
    goto FINISHING;
  }

  switch (target->kind)
  {
    case READ_LOG:
      permissions = WRITE_PERMISSIONS;
      break;
    case WRITE_LOG:
      permissions = WRITE_PERMISSIONS;
      break;
    default:
      permissions = 0;
      break;
  }

  // NOTE: SILENT shrinkage
  if (target->length > SHMMAX)
    target->length = SHMMAX;
  new_specific->shmid = shmget (new_specific->segment_key, 
                                target->length,
                                permissions);
  if (new_specific->shmid == INVALID_SHM_ID)
  {
    ret_code = sys_to_syp_error (errno);
    goto FINISHING;
  }

  new_specific->shm_start = shmat (new_specific->shmid, NULL, 0);
  if (new_specific->shm_start == (void *) -1)
  {
    new_specific->shm_start = NULL;
    ret_code = sys_to_syp_error (errno);
    goto FINISHING;
  }

  target->open_medium = open_shm_medium;
  target->close_medium = close_shm_medium;
  target->access_medium = shm_access;

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
syp_error close_shm_medium (medium target){
#ifdef ENABLE_CHECKING
  if (target == NULL)
  {
    return ERR_BAD_PARAMS;
  }
#endif

  shmdt (((shm_medium)target->type_specific)->shm_start);
  free (target->type_specific);
  target->type_specific = NULL;
  
  return NOERR;
}

syp_error shm_access (medium target, log_struct log)
{
#ifdef ENABLE_CHECKING
  if (target == NULL || log == NULL)
    return ERR_BAD_PARAMS;
  if (target->kind == NO_OPERATION ||target->used_formatter == NULL ||
    target->type_specific == NULL || log == NULL)
    return ERR_NOT_INITIALIZED;

#endif
  // TODO: implement
  // check boundaries
  if (target->length > 0 && 
      target->length - target->pos < target->used_formatter->get_max_print_size())
  {
  // move to front
    target->pos = 0;
  }

  int32_t chars_accessed = 0;

  switch (target->kind)
  {
    case READ_LOG:
      chars_accessed = target->used_formatter->mem_read (log,
        ((shm_medium)target->type_specific)->shm_start + target->pos);
      break;
    case WRITE_LOG:
      chars_accessed = target->used_formatter->mem_write (log,
        ((shm_medium)target->type_specific)->shm_start + target->pos);
      break;
    default:
      break;


  }
  if (chars_accessed >= 0)
  {
  // move boundary
    target->pos += target->used_formatter->get_max_print_size();
    return NOERR;
  }
  else
    return -chars_accessed;
}
