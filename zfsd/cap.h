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
#include <inttypes.h>
#include <stdio.h>
#include "pthread.h"
#include "zfs_prot.h"

/* Number of random bytes used to compute VERIFY.  */
#define CAP_RANDOM_LEN 16

/* Mark the ZFS capability CAP to be undefined.  */
#define zfs_cap_undefine(CAP) ((CAP).flags = UINT32_MAX)

/* Return true if the ZFS capability CAP is undefined.  */
#define zfs_cap_undefined(CAP) ((CAP).flags == UINT32_MAX)

typedef struct internal_cap_def *internal_cap;

struct internal_cap_def
{
  pthread_mutex_t mutex;

  /* Capability for client.  */
  zfs_cap local_cap;

  /* Capability for server.  */
  zfs_cap master_cap;

  /* Next capability for the ZFS file handle in the chain.  */
  internal_cap next;

  /* Random bytes.  */
  char random[CAP_RANDOM_LEN];

  /* Number of clients using this capability.  */
  unsigned int busy;

  /* Open file descriptor.  */
  int fd;

  /* Generation of open file descriptor.  */
  unsigned int generation;
};

#include "fh.h"

extern internal_cap internal_cap_lookup (zfs_cap *cap);
extern int32_t get_capability (zfs_cap *cap, internal_cap *icapp, volume *vol,
			       internal_dentry *dentry, virtual_dir *vd);
extern internal_cap get_capability_no_zfs_fh_lookup (zfs_cap *cap,
						     internal_dentry dentry);
extern int32_t find_capability (zfs_cap *cap, internal_cap *icapp, volume *vol,
				internal_dentry *dentry, virtual_dir *vd);
extern int32_t find_capability_nolock (zfs_cap *cap, internal_cap *icapp,
				       volume *vol, internal_dentry *dentry,
				       virtual_dir *vd);
extern int32_t put_capability (internal_cap cap, internal_fh fh,
			       virtual_dir vd);

extern void initialize_cap_c ();
extern void cleanup_cap_c ();

#endif
