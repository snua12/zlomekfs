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
#include "hashtab.h"
#include "memory.h"
#include "volume.h"

/* Hash table of volumes.  */
static htab_t volume_htab;

/* Hash function for volume ID ID.  */
#define VOLUME_HASH_ID(ID) (ID)

/* Hash function for volume N.  */
#define VOLUME_HASH(V) ((V)->id)

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
  volume vol = (volume) x;
  unsigned int id = *(unsigned int *) y;

  return vol->id == id;
}

/* Return the volume with volume ID == ID.  */

volume
volume_lookup (unsigned int id)
{
  return (volume) htab_find_with_hash (volume_htab, &id, VOLUME_HASH_ID (id));
}

/* Create volume structure and fill it with information.  */

volume
volume_create (unsigned int id)
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
  vol->local_root_fh = root_fh;
  vol->master_root_fh = root_fh;
  vol->root_vd = NULL;
  vol->fh_htab = htab_create (250, internal_fh_hash, internal_fh_eq,
			      internal_fh_del);
  vol->fh_htab_name = htab_create (250, internal_fh_hash_name,
				   internal_fh_eq_name, NULL);

#ifdef ENABLE_CHECKING
  slot = htab_find_slot_with_hash (volume_htab, &vol->id, VOLUME_HASH (vol),
				   NO_INSERT);
  if (slot)
    abort ();
#endif

  slot = htab_find_slot_with_hash (volume_htab, &vol->id, VOLUME_HASH (vol),
				   INSERT);
  *slot = vol;

  return vol;
}

/* Destroy volume VOL and free memory associated with it.  */

void
volume_destroy (volume vol)
{
  void **slot;

  virtual_mountpoint_destroy (vol);

  htab_destroy (vol->fh_htab_name);
  htab_destroy (vol->fh_htab);

  slot = htab_find_slot_with_hash (volume_htab, &vol->id, VOLUME_HASH (vol),
				   NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (volume_htab, slot);
  
  free (vol);
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

  /* FIXME: */
  if ((vol->flags & VOLUME_LOCAL) && !(vol->flags & VOLUME_COPY))
    {
      /* get local root zfs_fh */
    }
  else
    {
      /* get remote root zfs_fh */
    }
}

/* Set the information for a volume with local copy.  */

void
volume_set_local_info (volume vol, const char *local_path, uint64_t size_limit)
{
  set_string (&vol->local_path, local_path);
  vol->size_limit = size_limit;
  vol->flags |= VOLUME_LOCAL;
}

/* Print the information about volume VOL to file F.  */

void
print_volume (FILE *f, volume vol)
{
  fprintf (f, "%u %s %s\n", vol->id, vol->name, vol->mountpoint);
}

/* Print the information about all volumes to file F.  */

void
print_volumes (FILE *f)
{
  void **slot;

  HTAB_FOR_EACH_SLOT (volume_htab, slot, print_volume (f, (volume) *slot));
}

/* Print the information about volume VOL to STDERR.  */

void
debug_volume (volume vol)
{
  print_volume (stderr, vol);
}

/* Print the information about all volumes to STDERR.  */

void
debug_volumes ()
{
  print_volumes (stderr);
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
  void **slot;

  HTAB_FOR_EACH_SLOT (volume_htab, slot, volume_destroy ((volume) *slot));

  htab_destroy (volume_htab);
}
