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
#include "node.h"

/* Volume description.  */
typedef struct volume_def
{
  unsigned id;			/* ID of the volume */
  char *name;			/* name of the volume */
  node master;			/* master node for the volume */
  char *mountpoint;		/* "mountpoint" for the volume on the cluster fs */
  int flags;			/* see VOLUME_* below */

  char *localpath;		/* directory with local copy of volume */
  uint64_t size_limit;		/* size limit for copy of volume */
} *volume;

/* Predefined volume IDs.  */
#define VOLUME_ID_NONE    0	/* ID of the non-existing 'root' volume */
#define VOLUME_ID_CONFIG  1	/* ID of 'config' volume */

/* Volume flags.  */
#define VOLUME_DELETE	1	/* the volume should be deleted from memory
				   datastructures  */
#define VOLUME_LOCAL	2	/* this volume is located on local node */
#define VOLUME_COPY	4	/* this is a copy of a volume */

/* Is the volume active (i.e. accessible)?  */
#define VOLUME_ACTIVE_P(V) ((V)->master->status != CONNECTION_NONE)

/* Function prototypes.  */
extern volume volume_create (unsigned id);

#endif
