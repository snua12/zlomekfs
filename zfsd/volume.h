/* Volume functions.
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

#ifndef VOLUME_H
#define VOLUME_H

#include "system.h"
#include <inttypes.h>
#include "fh.h"
#include "node.h"

/* Volume description.  */
struct volume_def
{
  unsigned int id;		/* ID of the volume */
  char *name;			/* name of the volume */
  node master;			/* master node for the volume */
  char *mountpoint;		/* "mountpoint" for the volume on cluster fs */
  int flags;			/* see VOLUME_* below */

  char *local_path;		/* directory with local copy of volume */
  uint64_t size_limit;		/* size limit for copy of volume */

  zfs_fh root_fh;		/* file handle of the remote root.  */
  virtual_dir root_vd;		/* virtual directory for the mountpoint */
  htab_t fh_htab;		/* hash table of used file handles,
				   searched by client_fh */
  htab_t fh_htab_name;		/* hash table of used file handles,
				   searched by(parent_fh, name) */
};

/* Predefined volume IDs.  */
#define VOLUME_ID_VIRTUAL 0	/* ID of the non-existing 'root' volume */
#define VOLUME_ID_CONFIG  1	/* ID of 'config' volume */

/* Volume flags.  */
#define VOLUME_DELETE	1	/* the volume should be deleted from memory
				   datastructures  */
#define VOLUME_LOCAL	2	/* this volume is located on local node */
#define VOLUME_COPY	4	/* this is a copy of a volume */

/* Is the volume active (i.e. accessible)?  */
#define VOLUME_ACTIVE_P(V) (((V)->flags & VOLUME_LOCAL) > 0		\
			    ||  ((V)->master->conn != CONNECTION_NONE	\
				 && (V)->master->auth == AUTHENTICATION_DONE))

/* Value of size limit indicating that the volume is not limited.  */
#define VOLUME_NO_LIMIT 0

/* Function prototypes.  */
extern volume volume_lookup (unsigned int id);
extern volume volume_create (unsigned int id);
extern void volume_destroy (volume vol);
extern void volume_set_common_info (volume vol, const char *name,
				    const char *mountpoint, node master);
extern void volume_set_local_info (volume vol, const char *local_path,
				   uint64_t size_limit);
extern void initialize_volume_c ();
extern void cleanup_volume_c ();

#endif
