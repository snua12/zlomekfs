#ifndef SHM_WRITER_H
#define SHM_WRITER_H

/*! \file
    \brief Shared memory writer functions definitions.  */

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

#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdio.h>

#undef _GNU_SOURCE


#include "writer-api.h"

#ifndef SHMMAX
#define SHMMAX	0x2000000
#endif	/*SHMMAX*/

/// invalid shared memory segment id (according to man 2 shmget)
#define	INVALID_SHM_ID	-1

/// default size of shared memory segment (log size)
#define DEFAULT_SHM_LOG_SIZE	4096

/// default key to shm segment. used when no key given
#define DEFAULT_SHM_KEY	4224

/// name of writer for translation from options (--type=file)
#define	SHM_WRITER_NAME	"shm"

/// parameter name of output file name (where to write logs)
#define PARAM_WRITER_SK_LONG	"shm-key"
/// short parameter name for PARAM_WRITER_FN_LONG - can be used only inside code now
#define PARAM_WRITER_SK_CHAR	'k'

/*! Structure that holds internal state info specific for shm writer. */
typedef struct shm_writer_specific_def {
  /// shared memory segment start
  void * shm_start;
  /// segment identifier
  int shmid;
  /// key to shared memory segment
  key_t segment_key;

}* shm_writer_specific;


/*! Parse params and initialize shm writer
  @see open_writer
  @see writer-api.h
*/
syp_error open_shm_writer (struct writer_def * target, int argc, char ** argv);

/*! Close shm writer and free shm writer specific structures.
  Doesn't destroy shm segment.
  @see close_writer
  @see writer-api.h
*/
syp_error close_shm_writer (struct writer_def * target);

/*! Write log into a shared memory segment.
  @see open_writer
  @see writer-api.h
*/
syp_error write_shm_log (struct writer_def * target, log_struct log);

#endif /*SHM_WRITER_H*/
