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

/* Mutex for cap_pool and cap_htab.  */
extern pthread_mutex_t cap_mutex;

extern internal_cap internal_cap_lookup (zfs_cap *cap);
extern int get_capability (zfs_cap *cap, internal_cap *icapp, volume *vol,
			   internal_fh *ifh, virtual_dir *vd);
extern internal_cap get_capability_no_zfs_fh_lookup (zfs_cap *cap,
						     internal_fh fh);
extern int find_capability (zfs_cap *cap, internal_cap *icapp, volume *vol,
			    internal_fh *ifh, virtual_dir *vd);
extern int find_capability_nolock (zfs_cap *cap, internal_cap *icapp,
				   volume *vol, internal_fh *ifh,
				   virtual_dir *vd);
extern int put_capability (internal_cap cap, internal_fh fh);
extern bool internal_cap_opened_p (internal_cap cap);
extern int internal_cap_open (internal_cap cap, unsigned int flags,
			      internal_fh fh, volume vol);
extern void initialize_cap_c ();
extern void cleanup_cap_c ();

#endif
