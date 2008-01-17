#ifndef SHM_MEDIUM_H
#define SHM_MEDIUM_H

/*! \file
    \brief Shared memory accessor (for reader and writer) functions definitions.  */

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

#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#undef _GNU_SOURCE

#include "medium-api.h"
#include "syp-error.h"

#ifndef SHMMAX
#define	SHMMAX 0x2000000
#endif

/// invalid shared memory segment id (according to man 2 shmget)
#define	INVALID_SHM_ID	-1

/// default size of shared memory segment (log size)
#define DEFAULT_SHM_SIZE	4096

/// default key to shm segment. used when no key given
#define DEFAULT_SHM_KEY	4224

/// name of reader for translation from options (--type=file)
#define	SHM_MEDIUM_NAME	"shm"

/// parameter name of shm segment key
#define PARAM_SHM_KEY_LONG	"shm-key"
/// short parameter name for PARAM_SHM_KEY_LONG - can be used only inside code now
#define PARAM_SHM_KEY_CHAR	'k'

/*! Structure that holds internal state info specific for shm medium. */
typedef struct shm_medium_def {
  /// shared memory segment start
  void * shm_start;
  /// segment identifier
  int shmid;
  /// key to shared memory segment
  key_t segment_key;

}* shm_medium;

/*! Check if argument is recognized by shared memory medium
  @param arg command line argument (in format --argument_name=value)
  @return TRUE in case of recognition, FALSE otherwise
*/
bool_t is_shm_medium_arg (const char * arg);


/*! Parse params and initialize shm segment
  @see open_medium
  @see medium-api.h
*/
syp_error open_shm_medium (struct medium_def * target, int argc, const char ** argv);

/*! Close shm medium and free shm medium internals.
  Doesn't destroy shm segment.
  @see close_medium
  @see medium-api.h
*/
syp_error close_shm_medium (struct medium_def * target);

/*! read log from a shared memory segment.
  @see access_medium
  @see medium-api.h
*/
syp_error shm_access (struct medium_def * source, log_struct log);

/*! prints shm options help to fd.
  @see print_medium_help
  @see medium-api.h
*/
void print_shm_medium_help (int fd, int tabs);

#endif /*SHM_MEDIUM_H*/
