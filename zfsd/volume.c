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

#include "system.h"
#include <inttypes.h>
#include <stdlib.h>
#include "fh.h"
#include "memory.h"
#include "volume.h"

/* Hash table of volumes.  */
static htab_t volume_htab;

/* Hash function for volume N.  */
#define VOLUME_HASH(N) ((N)->id)

/* Hash function for volume X.  */

static hash_t
volume_hash (const void *x)
{
  return VOLUME_HASH ((volume) x);
}

/* Compare a volume X with ID *Y.  */

static int
volume_eq (const void *x, const void *y)
{
  volume n = (volume) x;
  unsigned int id = *(unsigned int *) y;

  return n->id == id;
}

/* Create volume structure and fill it with information.  */

volume
volume_create (unsigned id)
{
  volume vol;
  void **slot;

  vol = (volume) xmalloc (sizeof (struct volume_def));
  vol->id = id;
  vol->name = NULL;
  vol->master = NULL;
  vol->mountpoint = NULL;
  vol->flags = 0;
  vol->local_path = NULL;
  vol->size_limit = VOLUME_NO_LIMIT;

#ifdef ENABLE_CHECKING
  slot = htab_find_slot (volume_htab, &vol->id, NO_INSERT);
  if (slot)
    abort ();
#endif

  slot = htab_find_slot (volume_htab, &vol->id, INSERT);
  *slot = vol;

  return vol;
}

/* Set the information common for all volume types.  */

void
volume_set_common_info (volume vol, const char *name, const char *mountpoint,
			node master)
{
  set_string (&vol->name, name);
  set_string (&vol->mountpoint, mountpoint);
  vol->master = master;
  if (!(master->flags & NODE_LOCAL))
    vol->flags |= VOLUME_COPY;
  virtual_mountpoint_create (vol);
}

/* Set the information for a volume with local copy.  */

void
volume_set_local_info (volume vol, const char *local_path, uint64_t size_limit)
{
  set_string (&vol->local_path, local_path);
  vol->size_limit = size_limit;
  vol->flags |= VOLUME_LOCAL;
}

/* Initialize data structures in VOLUME.C.  */

void
initialize_volume_c ()
{
  volume_htab = htab_create (200, volume_hash, volume_eq, NULL);
}

/* Destroy data structures in VOLUME.C.  */

void
cleanup_volume_c ()
{
  htab_destroy (volume_htab);
}
