/* Capability functions.
   Copyright (C) 2003 Josef Zlomek

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
   or download it from http://www.gnu.org/licenses/gpl.html */

#ifndef CAP_H
#define CAP_H

#include "system.h"
#include "pthread.h"
#include "zfs_prot.h"
#include "fh.h"

/* Number of random bytes used to compute VERIFY.  */
#define CAP_RANDOM_LEN 16

typedef struct internal_cap_def
{
  pthread_mutex_t mutex;

  /* Capability for client.  */
  zfs_cap local_cap;

  /* Capability for server.  */
  zfs_cap master_cap;

  /* Random bytes.  */
  char random[CAP_RANDOM_LEN];

  /* Number of clients using this capability.  */
  unsigned int busy;

  /* Open file descriptor.  */
  int fd;

  /* Generation of open file descriptor.  */
  unsigned int generation;
} *internal_cap;

/* Data for file descriptor.  */
typedef struct internal_fd_data_def
{
  pthread_mutex_t mutex;
  int fd;			/* file descriptor */
  time_t last_use;		/* time of last use of the file descriptor */
  unsigned int generation;	/* generation of open file descriptor */
  int busy;			/* number of threads using file descriptor */
} internal_fd_data_t;
 
/* Mutex for cap_pool and cap_htab.  */
extern pthread_mutex_t cap_mutex;

extern internal_cap internal_cap_lookup (internal_fh fh, unsigned int mode);
extern internal_cap get_capability (internal_fh fh, unsigned int mode);
extern void put_capability (internal_cap cap);
extern void initialize_cap_c ();
extern void cleanup_cap_c ();

#endif
