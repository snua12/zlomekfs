/* Volume functions.
   Copyright (C) 2003, 2004 Josef Zlomek

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
#include "pthread.h"
#include "fh.h"
#include "hashtab.h"
#include "hashfile.h"
#include "memory.h"
#include "volume.h"
#include "network.h"
#include "metadata.h"

/* Hash table of volumes.  */
static htab_t volume_htab;

/* Mutex for table of volumes.  */
pthread_mutex_t volume_mutex;

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
  uint32_t id = *(uint32_t *) y;

  return vol->id == id;
}

/* Return the volume with volume ID == ID.  */

volume
volume_lookup (uint32_t id)
{
  volume vol;

  zfsd_mutex_lock (&volume_mutex);
  vol = (volume) htab_find_with_hash (volume_htab, &id, VOLUME_HASH_ID (id));
  if (vol)
    zfsd_mutex_lock (&vol->mutex);
  zfsd_mutex_unlock (&volume_mutex);

  return vol;
}

/* Return the volume with volume ID == ID.  */

volume
volume_lookup_nolock (uint32_t id)
{
  volume vol;

  CHECK_MUTEX_LOCKED (&volume_mutex);

  vol = (volume) htab_find_with_hash (volume_htab, &id, VOLUME_HASH_ID (id));
  if (vol)
    zfsd_mutex_lock (&vol->mutex);

  return vol;
}

/* Create volume structure and fill it with information.  */

volume
volume_create (uint32_t id)
{
  volume vol;
  void **slot;

  CHECK_MUTEX_LOCKED (&volume_mutex);

  vol = (volume) xmalloc (sizeof (struct volume_def));
  vol->id = id;
  vol->name = NULL;
  vol->master = NULL;
  vol->mountpoint = NULL;
  vol->delete_p = false;
  vol->n_locked_fhs = 0;
  vol->local_path = NULL;
  vol->size_limit = VOLUME_NO_LIMIT;
  vol->root_dentry = NULL;
  vol->root_vd = NULL;
  vol->metadata = NULL;

  zfsd_mutex_init (&vol->mutex);
  zfsd_mutex_lock (&vol->mutex);

  slot = htab_find_slot_with_hash (volume_htab, &vol->id, VOLUME_HASH (vol),
				   INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = vol;

  return vol;
}

/* Destroy volume VOL and free memory associated with it.
   This function expects volume_mutex to be locked.  */

static void
volume_destroy (volume vol)
{
  void **slot;

  CHECK_MUTEX_LOCKED (&vd_mutex);
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&volume_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

#ifdef ENABLE_CHECKING
  if (vol->n_locked_fhs > 0)
    abort ();
#endif

  if (vol->root_dentry)
    {
      zfsd_mutex_lock (&vol->root_dentry->fh->mutex);
      internal_dentry_destroy (vol->root_dentry, false);
    }

  virtual_mountpoint_destroy (vol);

  close_volume_metadata (vol);

  slot = htab_find_slot_with_hash (volume_htab, &vol->id, VOLUME_HASH (vol),
				   NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (volume_htab, slot);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_destroy (&vol->mutex);

  if (vol->local_path)
    free (vol->local_path);
  free (vol->mountpoint);
  free (vol->name);
  free (vol);
}

/* Destroy volume VOL and free memory associated with it.
   Destroy dentries while volume_mutex is unlocked.
   This function expects fh_mutex to be locked.  */

void
volume_delete (volume vol)
{
  uint32_t vid = vol->id;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

#ifdef ENABLE_CHECKING
  if (vol->n_locked_fhs > 0)
    abort ();
#endif

  /* Destroy dentries on volume.  */
  if (vol->root_dentry)
    {
      internal_dentry dentry;

      dentry = vol->root_dentry;
      zfsd_mutex_lock (&dentry->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      internal_dentry_destroy (dentry, true);
    }
  else
    zfsd_mutex_unlock (&vol->mutex);

  /* Destroy volume.  */
  zfsd_mutex_unlock (&fh_mutex);
  zfsd_mutex_lock (&vd_mutex);
  zfsd_mutex_lock (&fh_mutex);
  zfsd_mutex_lock (&volume_mutex);
  vol = volume_lookup_nolock (vid);
  if (vol)
    volume_destroy (vol);
  zfsd_mutex_unlock (&volume_mutex);
  zfsd_mutex_unlock (&vd_mutex);
}

/* Set the information common for all volume types.  */

void
volume_set_common_info (volume vol, const char *name, const char *mountpoint,
			node master)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);

  set_string (&vol->name, name);
  set_string (&vol->mountpoint, mountpoint);
  vol->master = master;
  virtual_mountpoint_create (vol);
}

/* Set the information for a volume with local copy.  */

bool
volume_set_local_info (volume vol, const char *local_path, uint64_t size_limit)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);

  set_string (&vol->local_path, local_path);
  vol->size_limit = size_limit;

  return init_volume_metadata (vol);
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
debug_volumes (void)
{
  print_volumes (stderr);
}

/* Initialize data structures in VOLUME.C.  */

void
initialize_volume_c (void)
{
  zfsd_mutex_init (&volume_mutex);
  volume_htab = htab_create (200, volume_hash, volume_eq, NULL, &volume_mutex);
}

/* Destroy data structures in VOLUME.C.  */

void
cleanup_volume_c (void)
{
  void **slot;

  zfsd_mutex_lock (&vd_mutex);
  zfsd_mutex_lock (&fh_mutex);
  zfsd_mutex_lock (&volume_mutex);
  HTAB_FOR_EACH_SLOT (volume_htab, slot,
    {
      volume vol = (volume) *slot;

      zfsd_mutex_lock (&vol->mutex);
      volume_destroy ((volume) *slot);
    });
  htab_destroy (volume_htab);
  zfsd_mutex_unlock (&volume_mutex);
  zfsd_mutex_destroy (&volume_mutex);
  zfsd_mutex_unlock (&fh_mutex);
  zfsd_mutex_unlock (&vd_mutex);
}
