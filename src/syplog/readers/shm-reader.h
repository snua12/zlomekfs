#ifndef SHM_READER_H
#define SHM_READER_H

/*! \file
    \brief Shared memory reader functions definitions.  */

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
#include <sys/ipc.h>
#include <sys/shm.h>

#include "reader-api.h"

/// invalid shared memory segment id (according to man 2 shmget)
#define	INVALID_SHM_ID	-1

/// default size of shared memory segment (log size)
#define DEFAULT_SHM_LOG_SIZE	4096

/// default key to shm segment. used when no key given
#define DEFAULT_SHM_KEY	4224

/// name of reader for translation from options (--type=file)
#define	SHM_READER_NAME	"shm"

/// parameter name of output file name (where to write logs)
#define PARAM_READER_SK_LONG	"shm-key"
/// short parameter name for PARAM_READER_FN_LONG - can be used only inside code now
#define PARAM_READER_FN_CHAR	'k'

/*! Structure that holds internal state info specific for shm reader. */
typedef struct shm_reader_specific_def {
  /// shared memory segment start
  void * shm_start;
  /// segment identifier
  int shmid;
  /// key to shared memory segment
  key_t segment_key;

}* file_reader_specific;


/*! Parse params and initialize shm reader
  @see open_reader
  @see reader-api.h
*/
syp_error open_shm_reader (struct reader_def * target, int argc, char ** argv);

/*! Close shm reader and free shm reader specific structures.
  Doesn't destroy shm segment.
  @see close_reader
  @see reader-api.h
*/
syp_error close_shm_reader (struct reader_def * target);

/*! read log into a shared memory segment.
  @see open_reader
  @see reader-api.h
*/
syp_error read_shm_log (struct reader_def * target, log_struct log);

#endif /*SHM_READER_H*/
