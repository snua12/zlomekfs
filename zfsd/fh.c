/* File handle functions.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include "pthread.h"
#include "fh.h"
#include "alloc-pool.h"
#include "crc32.h"
#include "hashtab.h"
#include "fibheap.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "varray.h"
#include "metadata.h"
#include "zfs_prot.h"
#include "user-group.h"

/* File handle of ZFS root.  */
zfs_fh root_fh = {NODE_ANY, VOLUME_ID_VIRTUAL, VIRTUAL_DEVICE, ROOT_INODE};

/* The virtual directory root.  */
static virtual_dir root;

/* Allocation pool for file handles.  */
static alloc_pool fh_pool;

/* Mutex for fh_pool.  */
static pthread_mutex_t fh_pool_mutex;

/* Allocation pool for dentries.  */
static alloc_pool dentry_pool;

/* Mutex for dentry_pool.  */
static pthread_mutex_t dentry_pool_mutex;

/* Allocation pool for virtual directories ("mountpoints").  */
static alloc_pool vd_pool;

/* Hash table of virtual directories, searched by fh.  */
htab_t vd_htab;

/* Hash table of virtual directories, searched by (parent->fh, name).  */
static htab_t vd_htab_name;

/* Mutex for virtual directories.  */
pthread_mutex_t vd_mutex;

/* Heap holding internal file handles will be automatically freed
   when unused for a long time.  */
fibheap cleanup_dentry_heap;

/* Mutex protecting CLEANUP_FH_*.  */
pthread_mutex_t cleanup_dentry_mutex;

/* Thread ID of thread freeing file handles unused for a long time.  */
pthread_t cleanup_dentry_thread;

/* This mutex is locked when cleanup fh thread is in sleep.  */
pthread_mutex_t cleanup_dentry_thread_in_syscall;

/* Hash function for internal file handle FH.  */
#define INTERNAL_FH_HASH(FH)						\
  (ZFS_FH_HASH (&(FH)->local_fh))

/* Hash function for virtual_dir VD, computed from fh.  */
#define VIRTUAL_DIR_HASH(VD)						\
  (ZFS_FH_HASH (&(VD)->fh))

/* Hash function for virtual_dir VD, computed from (parent->fh, name).  */
#define VIRTUAL_DIR_HASH_NAME(VD)					\
  (crc32_update (crc32_string ((VD)->name),				\
		 &(VD)->parent->fh, sizeof (zfs_fh)))

/* Add ZFS_FH of internal dentry DENTRY with key DENTRY->LAST_USE
   to heap CLEANUP_DENTRY_HEAP.  */

static void
cleanup_dentry_insert_node (internal_dentry dentry)
{
#ifdef ENABLE_CHECKING
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#endif

  zfsd_mutex_lock (&cleanup_dentry_mutex);
  if (!dentry->heap_node)
    {
      fibheapkey_t key;

      key = dentry->ncap > 0 ? FIBHEAPKEY_MAX : (fibheapkey_t) dentry->last_use;
      dentry->heap_node = fibheap_insert (cleanup_dentry_heap, key, dentry);
    }
  zfsd_mutex_unlock (&cleanup_dentry_mutex);
}

/* Replace key of node DENTRY->HEAP_NODE to DENTRY->LAST_USE.  */

static void
cleanup_dentry_update_node (internal_dentry dentry)
{
#ifdef ENABLE_CHECKING
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#endif

  zfsd_mutex_lock (&cleanup_dentry_mutex);
  if (dentry->heap_node)
    {
      fibheapkey_t key;

      key = dentry->ncap > 0 ? FIBHEAPKEY_MAX : (fibheapkey_t) dentry->last_use;
      fibheap_replace_key (cleanup_dentry_heap, dentry->heap_node, key);
    }
  zfsd_mutex_unlock (&cleanup_dentry_mutex);
}

/* Delete IFH->HEAP_NODE from CLEANUP_FH_HEAP and set it to NULL.  */

static void
cleanup_dentry_delete_node (internal_dentry dentry)
{
#ifdef ENABLE_CHECKING
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#endif

  zfsd_mutex_lock (&cleanup_dentry_mutex);
  if (dentry->heap_node)
    {
      fibheap_delete_node (cleanup_dentry_heap, dentry->heap_node);
      dentry->heap_node = NULL;
    }
  zfsd_mutex_unlock (&cleanup_dentry_mutex);
}

/* Compare the volume IDs of ZFS_FHs P1 and P2.  */

static int
cleanup_unused_dentries_compare (const void *p1, const void *p2)
{
  const zfs_fh *fh1 = (const zfs_fh *) p1;
  const zfs_fh *fh2 = (const zfs_fh *) p2;

  if (fh1->vid == fh2->vid)
    return 0;
  else if (fh1->vid < fh2->vid)
    return -1;
  return 1;
}

/* Free internal dentries unused for at least MAX_INTERNAL_DENTRY_UNUSED_TIME
   seconds.  */

static void
cleanup_unused_dentries ()
{
  fibheapkey_t threshold;
  internal_dentry dentry;
  zfs_fh fh[1024];
  int i, j, n;

  threshold = (fibheapkey_t) time (NULL) - MAX_INTERNAL_DENTRY_UNUSED_TIME;
  do
    {
      zfsd_mutex_lock (&cleanup_dentry_mutex);
      for (n = 0; n < 1024; n++)
	{
	  if (cleanup_dentry_heap->nodes == 0)
	    break;

	  dentry = (internal_dentry) fibheap_min (cleanup_dentry_heap);
#ifdef ENABLE_CHECKING
	  if (!dentry)
	    abort ();
#endif
	  if (fibheap_min_key (cleanup_dentry_heap) >= threshold)
	    break;

	  fibheap_extract_min (cleanup_dentry_heap);

	  /* We have to clear DENTRY->HEAP_NODE while the CLEANUP_DENTRY_MUTEX
	     is still locked. Moreover we have to copy the ZFS_FH because
	     the internal dentry may be freed as soon as we unlock
	     CLEANUP_DENTRY_MUTEX.  Later we have to lookup the internal dentry
	     and do nothing if it already does not exist.  */
	  dentry->heap_node = NULL;
	  fh[n] = dentry->fh->local_fh;
	}
      zfsd_mutex_unlock (&cleanup_dentry_mutex);
      if (n)
	{
	  message (3, stderr, "Freeing %d nodes\n", n);
	  qsort (fh, n, sizeof (zfs_fh), cleanup_unused_dentries_compare);

	  zfsd_mutex_lock (&volume_mutex);
	  for (i = 0; i < n; i = j)
	    {
	      volume vol;
	      uint32_t vid;

	      vid = fh[i].vid;
	      vol = volume_lookup (vid);
	      if (vol)
		{
		  for (j = i; j < n && fh[j].vid == vid; j++)
		    {
		      internal_dentry parent;

		      dentry = dentry_lookup (vol, &fh[j]);
		      if (!dentry)
			continue;

		      /* We may have added a dentry to it
			 while CLEANUP_DENTRY_MUTEX was unlocked.  */
		      if (dentry->fh->attr.type == FT_DIR
			  && VARRAY_USED (dentry->fh->subdentries) > 0)
			continue;

		      /* We may have looked DENTRY up again
			 so we may have updated LAST_USE
			 or there are capabilities associated with
			 the file handle.  */
		      if ((fibheapkey_t) dentry->last_use >= threshold
			  || dentry->ncap > 0)
			{
			  /* Reinsert the file handle to heap.  */
			  zfsd_mutex_lock (&dentry->fh->mutex);
			  cleanup_dentry_insert_node (dentry);
			  zfsd_mutex_unlock (&dentry->fh->mutex);
			  continue;
			}

		      parent = dentry->parent;

		      if (parent)
			zfsd_mutex_lock (&parent->fh->mutex);

		      zfsd_mutex_lock (&dentry->fh->mutex);
		      internal_dentry_destroy (dentry, vol);

		      if (parent)
			zfsd_mutex_unlock (&parent->fh->mutex);
		    }
		  zfsd_mutex_unlock (&vol->mutex);
		}
	      else
		{
		  /* Skip the file handles from the same volume.  */
		  for (j = i; j < n && fh[j].vid == vid; j++)
		    ;
		}
	    }
	  zfsd_mutex_unlock (&volume_mutex);
	}
    }
  while (n > 0);
}

/* Main function of thread freeing file handles unused for a long time.  */

void *
cleanup_dentry_thread_main (ATTRIBUTE_UNUSED void *data)
{
  thread_disable_signals ();

  while (get_running ())
    {
      zfsd_mutex_lock (&cleanup_dentry_thread_in_syscall);
      if (get_running ())
	sleep (1);
      zfsd_mutex_unlock (&cleanup_dentry_thread_in_syscall);
      if (!get_running ())
	break;

      cleanup_unused_dentries ();
    }

  /* Disable signaling this thread. */
  zfsd_mutex_lock (&running_mutex);
  cleanup_dentry_thread = 0;
  zfsd_mutex_unlock (&running_mutex);

  zfsd_mutex_destroy (&cleanup_dentry_thread_in_syscall);
  return NULL;
}

/* Hash function for internal file handle X.  */
hash_t
internal_fh_hash (const void *x)
{
  return INTERNAL_FH_HASH ((internal_fh) x);
}

/* Hash function for internal dentry X, computed from fh->local_fh.  */

hash_t
internal_dentry_hash (const void *x)
{
  return INTERNAL_DENTRY_HASH ((internal_dentry) x);
}

/* Hash function for internal dentry X, computed from parent->fh and name.  */

hash_t
internal_dentry_hash_name (const void *x)
{
  return INTERNAL_DENTRY_HASH_NAME ((internal_dentry) x);
}

/* Compate an internal file handle XX with client's file handle YY.  */

int
internal_fh_eq (const void *xx, const void *yy)
{
  zfs_fh *x = &((internal_fh) xx)->local_fh;
  zfs_fh *y = (zfs_fh *) yy;

  return (x->ino == y->ino && x->dev == y->dev
	  && x->vid == y->vid && x->sid == y->sid);
}

/* Compare an internal file handle XX with client's file handle YY.  */

int
internal_dentry_eq (const void *xx, const void *yy)
{
  zfs_fh *x = &((internal_dentry) xx)->fh->local_fh;
  zfs_fh *y = (zfs_fh *) yy;

  return (x->ino == y->ino && x->dev == y->dev
	  && x->vid == y->vid && x->sid == y->sid);
}

/* Compare two internal file handles XX and YY whether they have same parent
   and file name.  */

int
internal_dentry_eq_name (const void *xx, const void *yy)
{
  internal_dentry x = (internal_dentry) xx;
  internal_dentry y = (internal_dentry) yy;

  return (x->parent == y->parent
  	  && strcmp (x->name, y->name) == 0);
}

/* Find the internal file handle or virtual directory for zfs_fh FH
   and set *VOLP, *DENTRYP and VDP according to it.  */

int32_t
zfs_fh_lookup (zfs_fh *fh, volume *volp, internal_dentry *dentryp,
	       virtual_dir *vdp)
{
  int32_t res;

  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*fh))
    zfsd_mutex_lock (&vd_mutex);

  res = zfs_fh_lookup_nolock (fh, volp, dentryp, vdp);

  zfsd_mutex_unlock (&volume_mutex);
  if (VIRTUAL_FH_P (*fh))
    zfsd_mutex_unlock (&vd_mutex);

  return res;
}

/* Find the internal file handle or virtual directory for zfs_fh FH
   and set *VOLP, *DENTRYP and VDP according to it.
   This function is similar to FH_LOOKUP but the big locks must be locked.  */

int32_t
zfs_fh_lookup_nolock (zfs_fh *fh, volume *volp, internal_dentry *dentryp,
		      virtual_dir *vdp)
{
  hash_t hash = ZFS_FH_HASH (fh);

  CHECK_MUTEX_LOCKED (&volume_mutex);

  if (VIRTUAL_FH_P (*fh))
    {
      virtual_dir vd;

      CHECK_MUTEX_LOCKED (&vd_mutex);

      vd = (virtual_dir) htab_find_with_hash (vd_htab, fh, hash);
      if (!vd)
	return ENOENT;

      zfsd_mutex_lock (&vd->mutex);
      if (vd->vol)
	zfsd_mutex_lock (&vd->vol->mutex);

      *volp = vd->vol;
      if (dentryp)
	*dentryp = NULL;
      *vdp = vd;
    }
  else
    {
      volume vol;
      internal_dentry dentry;

      vol = volume_lookup (fh->vid);
      if (!vol)
	return ENOENT;

      if (!volume_active_p (vol))
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  return ESTALE;
	}

      dentry = (internal_dentry) htab_find_with_hash (vol->dentry_htab, fh,
						      hash);
      if (!dentry)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  return ESTALE;
	}

      zfsd_mutex_lock (&dentry->fh->mutex);
      dentry->last_use = time (NULL);
      cleanup_dentry_update_node (dentry);

      *volp = vol;
      *dentryp = dentry;
      if (vdp)
	*vdp = NULL;
    }

  return ZFS_OK;
}

/* Return the virtual directory for NAME in virtual directory PARENT.  */

virtual_dir
vd_lookup_name (virtual_dir parent, const char *name)
{
  virtual_dir vd;
  struct virtual_dir_def tmp_vd;

  CHECK_MUTEX_LOCKED (&vd_mutex);
  CHECK_MUTEX_LOCKED (&parent->mutex);

  tmp_vd.parent = parent;
  tmp_vd.name = (char *) name;
  vd = (virtual_dir) htab_find (vd_htab_name, &tmp_vd);
  if (vd)
    zfsd_mutex_lock (&vd->mutex);

  return vd;
}

/* Return the internal dentry for NAME in directory PARENT on volume VOL.  */

internal_dentry
dentry_lookup_name (volume vol, internal_dentry parent, const char *name)
{
  struct internal_dentry_def tmp;
  internal_dentry dentry;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&parent->fh->mutex);

  tmp.parent = parent;
  tmp.name = (char *) name;
  dentry = (internal_dentry) htab_find (vol->dentry_htab_name, &tmp);
  if (dentry)
    {
      zfsd_mutex_lock (&dentry->fh->mutex);
      dentry->last_use = time (NULL);
      cleanup_dentry_update_node (dentry);
    }

  return dentry;
}

/* Create a new internal file handle and store it to hash tables.  */

static internal_fh
internal_fh_create (zfs_fh *local_fh, zfs_fh *master_fh, fattr *attr,
		    volume vol)
{
  internal_fh fh;
  void **slot;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  zfsd_mutex_lock (&fh_pool_mutex);
  fh = (internal_fh) pool_alloc (fh_pool);
  zfsd_mutex_unlock (&fh_pool_mutex);
  fh->local_fh = *local_fh;
  fh->master_fh = *master_fh;
  fh->attr = *attr;
  fh->ndentries = 0;
  fh->updated = NULL;
  fh->modified = NULL;

  if (fh->attr.type == FT_DIR)
    varray_create (&fh->subdentries, sizeof (internal_dentry), 16);

  zfsd_mutex_init (&fh->mutex);
  zfsd_mutex_lock (&fh->mutex);

  slot = htab_find_slot_with_hash (vol->fh_htab, &fh->local_fh,
				   INTERNAL_FH_HASH (fh), INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = fh;

  if (vol->local_path)
    {
      if (!init_metadata (vol, fh)
	  || !update_metadata (vol, fh))
	vol->flags |= VOLUME_DELETE;
    }

  return fh;
}

/* Destroy the internal file handle FH.  */

static void
internal_fh_destroy (internal_fh fh, volume vol)
{
  void **slot;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

#ifdef ENABLE_CHECKING
  if (fh->ndentries != 0)
    abort ();
#endif

  if (fh->attr.type == FT_DIR)
    varray_destroy (&fh->subdentries);

  slot = htab_find_slot_with_hash (vol->fh_htab, &fh->local_fh,
				   INTERNAL_FH_HASH (fh), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (vol->fh_htab, slot);

  if (vol->local_path)
    {
      if (!(vol->flags & VOLUME_DELETE)
	  && !update_metadata (vol, fh))
	vol->flags |= VOLUME_DELETE;
    }

  zfsd_mutex_unlock (&fh->mutex);
  zfsd_mutex_destroy (&fh->mutex);
  zfsd_mutex_lock (&fh_pool_mutex);
  pool_free (fh_pool, fh);
  zfsd_mutex_unlock (&fh_pool_mutex);
}

/* Print the contents of hash table HTAB to file F.  */

void
print_fh_htab (FILE *f, htab_t htab)
{
  void **slot;

  HTAB_FOR_EACH_SLOT (htab, slot,
    {
      internal_fh fh = (internal_fh) *slot;

      fprintf (f, "[%u,%u,%u,%u] ", fh->local_fh.sid, fh->local_fh.vid,
	       fh->local_fh.dev, fh->local_fh.ino);
      fprintf (f, "[%u,%u,%u,%u] ", fh->master_fh.sid, fh->master_fh.vid,
	       fh->master_fh.dev, fh->master_fh.ino);
      fprintf (f, "\n");
    });
}

/* Print the contents of hash table of filehandles HTAB to STDERR.  */

void
debug_fh_htab (htab_t htab)
{
  print_fh_htab (stderr, htab);
}

/* Create a new internal dentry NAME in directory PARENT on volume VOL and
   internal file handle for local file handle LOCAL_FH and master file handle
   MASTER_FH with attributes ATTR and store it to hash tables.  */

internal_dentry
internal_dentry_create (zfs_fh *local_fh, zfs_fh *master_fh, volume vol,
			internal_dentry parent, char *name, fattr *attr)
{
  internal_dentry dentry;
  internal_fh fh;
  void **slot;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  if (parent)
    CHECK_MUTEX_LOCKED (&parent->fh->mutex);

  zfsd_mutex_lock (&dentry_pool_mutex);
  dentry = (internal_dentry) pool_alloc (dentry_pool);
  zfsd_mutex_unlock (&dentry_pool_mutex);
  dentry->parent = parent;
  dentry->name = xstrdup (name);
  dentry->next = dentry;
  dentry->prev = dentry;
  dentry->ncap = 0;
  dentry->last_use = time (NULL);
  dentry->heap_node = NULL;

  /* Find the internal file handle in hash table, create it if it does not
     exist.  */
  slot = htab_find_slot_with_hash (vol->fh_htab, local_fh,
				   ZFS_FH_HASH (local_fh), INSERT);
  if (!*slot)
    {
      fh = internal_fh_create (local_fh, master_fh, attr, vol);
      *slot = fh;
    }
  else
    {
      fh = (internal_fh) *slot;
      zfsd_mutex_lock (&fh->mutex);
      fh->attr = *attr;
    }

  dentry->fh = fh;
  fh->ndentries++;

  if (parent)
    {
      cleanup_dentry_insert_node (dentry);
      dentry->dentry_index = VARRAY_USED (parent->fh->subdentries);
      VARRAY_PUSH (parent->fh->subdentries, dentry, internal_dentry);
      cleanup_dentry_delete_node (parent);
    }

  slot = htab_find_slot_with_hash (vol->dentry_htab, &fh->local_fh,
				   INTERNAL_DENTRY_HASH (dentry), INSERT);
  if (*slot)
    {
      internal_dentry old = (internal_dentry) *slot;

      dentry->next = old->next;
      dentry->prev = old;
      old->next->prev = dentry;
      old->next = dentry;
    }
  *slot = dentry;

  if (parent)
    {
      slot = htab_find_slot (vol->dentry_htab_name, dentry, INSERT);
#ifdef ENABLE_CHECKING
      if (*slot)
	abort ();
#endif
      *slot = dentry;
    }

  return dentry;
}

/* Create a new internal dentry NAME in directory PARENT on volume VOL for
   internal file handle FH.  */

internal_dentry
internal_dentry_link (internal_fh fh, volume vol,
		      internal_dentry parent, char *name)
{
  internal_dentry dentry;
  void **slot;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  if (parent)
    CHECK_MUTEX_LOCKED (&parent->fh->mutex);

  zfsd_mutex_lock (&dentry_pool_mutex);
  dentry = (internal_dentry) pool_alloc (dentry_pool);
  zfsd_mutex_unlock (&dentry_pool_mutex);
  dentry->parent = parent;
  dentry->name = xstrdup (name);
  dentry->fh = fh;
  fh->ndentries++;
  dentry->next = dentry;
  dentry->prev = dentry;
  dentry->ncap = 0;
  dentry->last_use = time (NULL);
  dentry->heap_node = NULL;

  if (parent)
    {
      cleanup_dentry_insert_node (dentry);
      dentry->dentry_index = VARRAY_USED (parent->fh->subdentries);
      VARRAY_PUSH (parent->fh->subdentries, dentry, internal_dentry);
      cleanup_dentry_delete_node (parent);
    }

  slot = htab_find_slot_with_hash (vol->dentry_htab, &fh->local_fh,
				   INTERNAL_DENTRY_HASH (dentry), INSERT);
  if (*slot)
    {
      internal_dentry old = (internal_dentry) *slot;

      dentry->next = old->next;
      dentry->prev = old;
      old->next->prev = dentry;
      old->next = dentry;
    }
  *slot = dentry;

  if (parent)
    {
      slot = htab_find_slot (vol->dentry_htab_name, dentry, INSERT);
#ifdef ENABLE_CHECKING
      if (*slot)
	abort ();
#endif
      *slot = dentry;
    }

  return dentry;
}

/* Move internal dentry DENTRY to be a subdentry of DIR with name NAME
   on volume VOL.  Return true on success.  */

bool
internal_dentry_move (internal_dentry dentry, volume vol,
		      internal_dentry dir, char *name)
{
  void **slot;
  internal_dentry tmp;
  internal_dentry top;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!dentry->parent)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&dentry->parent->fh->mutex);

  /* Check whether we are not moving DENTRY to its subtree.  */
  for (tmp = dir; tmp; tmp = tmp->parent)
    if (tmp == dentry)
      return false;

  /* Delete DENTRY from parent's directory entries.  */
  top = VARRAY_TOP (dentry->parent->fh->subdentries, internal_dentry);
  VARRAY_ACCESS (dentry->parent->fh->subdentries, dentry->dentry_index,
		 internal_dentry) = top;
  VARRAY_POP (dentry->parent->fh->subdentries);
  top->dentry_index = dentry->dentry_index;

  /* Delete from table searched by parent + name.  */
  slot = htab_find_slot (vol->dentry_htab_name, dentry, NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (vol->dentry_htab_name, slot);

  if (dentry->parent->parent
      && VARRAY_USED (dentry->parent->fh->subdentries) == 0)
    {
      /* PARENT is not root and is a leaf.  */
      cleanup_dentry_insert_node (dentry->parent);
    }

  free (dentry->name);
  dentry->name = xstrdup (name);
  dentry->parent = dir;

  /* Insert DENTRY to DIR.  */
  dentry->dentry_index = VARRAY_USED (dir->fh->subdentries);
  VARRAY_PUSH (dir->fh->subdentries, dentry, internal_dentry);
  cleanup_dentry_delete_node (dir);

  /* Insert to table searched by parent + name.  */
  slot = htab_find_slot (vol->dentry_htab_name, dentry, INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = dentry;

  return true;
}

/* Destroy internal dentry DENTRY on volume VOL.  */

void
internal_dentry_destroy (internal_dentry dentry, volume vol)
{
  void **slot;
  internal_dentry top;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  if (dentry->fh->attr.type == FT_DIR)
    {
      int i;

      /* Destroy subtree first.  */
      for (i = VARRAY_USED (dentry->fh->subdentries) - 1; i >= 0; i--)
	{
	  internal_dentry sdentry;

	  sdentry = VARRAY_ACCESS (dentry->fh->subdentries, (unsigned int) i,
				   internal_dentry);
	  zfsd_mutex_lock (&sdentry->fh->mutex);
	  internal_dentry_destroy (sdentry, vol);
	}
    }

  /* At this point, DENTRY is always a leaf.  */
  cleanup_dentry_delete_node (dentry);

  if (dentry->parent)
    {
      CHECK_MUTEX_LOCKED (&dentry->parent->fh->mutex);

      /* Remove DENTRY from parent's directory entries.  */
      top = VARRAY_TOP (dentry->parent->fh->subdentries, internal_dentry);
      VARRAY_ACCESS (dentry->parent->fh->subdentries, dentry->dentry_index,
		     internal_dentry) = top;
      VARRAY_POP (dentry->parent->fh->subdentries);
      top->dentry_index = dentry->dentry_index;

      /* Delete from table searched by parent + name.  */
      slot = htab_find_slot (vol->dentry_htab_name, dentry, NO_INSERT);
#ifdef ENABLE_CHECKING
      if (!slot)
	abort ();
#endif
      htab_clear_slot (vol->dentry_htab_name, slot);

      if (dentry->parent->parent
	  && VARRAY_USED (dentry->parent->fh->subdentries) == 0)
	{
	  /* PARENT is not root and is a leaf.  */
	  cleanup_dentry_insert_node (dentry->parent);
	}
    }

  slot = htab_find_slot_with_hash (vol->dentry_htab, &dentry->fh->local_fh,
				   INTERNAL_DENTRY_HASH (dentry), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif

  dentry->fh->ndentries--;
  if (dentry->next == dentry)
    {
#ifdef ENABLE_CHECKING
      if (dentry->fh->ndentries != 0)
	abort ();
#endif
      htab_clear_slot (vol->dentry_htab, slot);
      internal_fh_destroy (dentry->fh, vol);
    }
  else
    {
#ifdef ENABLE_CHECKING
      if (dentry->fh->ndentries == 0)
	abort ();
#endif
      dentry->next->prev = dentry->prev;
      dentry->prev->next = dentry->next;
      *slot = dentry->next;
      zfsd_mutex_unlock (&dentry->fh->mutex);
    }

  free (dentry->name);
  zfsd_mutex_lock (&dentry_pool_mutex);
  pool_free (dentry_pool, dentry);
  zfsd_mutex_unlock (&dentry_pool_mutex);
}

/* Hash function for virtual_dir X, computed from FH.  */

static hash_t
virtual_dir_hash (const void *x)
{
  virtual_dir vd = (virtual_dir) x;

#ifdef ENABLE_CHECKING
  if (!VIRTUAL_FH_P (vd->fh))
    abort ();
#endif

  return VIRTUAL_DIR_HASH (vd);
}

/* Hash function for virtual_dir X, computed from (PARENT->FH, NAME).  */

static hash_t
virtual_dir_hash_name (const void *x)
{
  virtual_dir vd = (virtual_dir) x;

#ifdef ENABLE_CHECKING
  if (!vd->parent || !VIRTUAL_FH_P (vd->parent->fh))
    abort ();
#endif

  return VIRTUAL_DIR_HASH_NAME (vd);
}

/* Compare a virtual directory XX with client's file handle YY.  */

static int
virtual_dir_eq (const void *xx, const void *yy)
{
  zfs_fh *x = &((virtual_dir) xx)->fh;
  zfs_fh *y = (zfs_fh *) yy;

#ifdef ENABLE_CHECKING
  if (!VIRTUAL_FH_P (*x))
    abort ();
  if (!VIRTUAL_FH_P (*y))
    abort ();
#endif
  return (x->ino == y->ino && x->dev == y->dev
	  && x->vid == y->vid && x->sid == y->sid);
}

/* Compare two virtual directories XX and YY whether they have same parent
   and file name.  */

static int
virtual_dir_eq_name (const void *xx, const void *yy)
{
  virtual_dir x = (virtual_dir) xx;
  virtual_dir y = (virtual_dir) yy;

#ifdef ENABLE_CHECKING
  if (!VIRTUAL_FH_P (x->fh))
    abort ();
  if (!y->parent || !VIRTUAL_FH_P (y->parent->fh))
    abort ();
#endif

  return (x->parent == y->parent
	  && strcmp (x->name, y->name) == 0);
}

/* Create a new virtual directory NAME in virtual directory PARENT.  */

virtual_dir
virtual_dir_create (virtual_dir parent, const char *name)
{
  virtual_dir vd;
  static uint32_t last_virtual_ino;
  void **slot;

  CHECK_MUTEX_LOCKED (&vd_mutex);
  CHECK_MUTEX_LOCKED (&parent->mutex);

  last_virtual_ino++;
  if (last_virtual_ino == 0)
    last_virtual_ino++;

  vd = (virtual_dir) pool_alloc (vd_pool);
  vd->fh.sid = NODE_ANY;
  vd->fh.vid = VOLUME_ID_VIRTUAL;
  vd->fh.dev = VIRTUAL_DEVICE;
  vd->fh.ino = last_virtual_ino;
  vd->parent = parent;
  vd->name = xstrdup (name);
  virtual_dir_set_fattr (vd);

  zfsd_mutex_init (&vd->mutex);
  zfsd_mutex_lock (&vd->mutex);

  varray_create (&vd->subdirs, sizeof (virtual_dir), 16);
  vd->subdir_index = VARRAY_USED (parent->subdirs);
  VARRAY_PUSH (parent->subdirs, vd, virtual_dir);
  vd->parent->attr.nlink++;
  vd->parent->attr.ctime = vd->parent->attr.mtime = time (NULL);

  vd->n_mountpoints = 0;
  vd->vol = NULL;

  slot = htab_find_slot_with_hash (vd_htab, &vd->fh,
				   VIRTUAL_DIR_HASH (vd), INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = vd;

  slot = htab_find_slot (vd_htab_name, vd, INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = vd;

  return vd;
}

/* Delete a virtual directory VD from all hash tables and free it.  */

void
virtual_dir_destroy (virtual_dir vd)
{
  virtual_dir parent;
  void **slot;

  CHECK_MUTEX_LOCKED (&vd_mutex);
  CHECK_MUTEX_LOCKED (&vd->mutex);

  /* Check the path to root.  */
  for (; vd; vd = parent)
    {
      parent = vd->parent;
      if (parent)
	zfsd_mutex_lock (&parent->mutex);
      vd->n_mountpoints--;
      if (vd->n_mountpoints == 0)
	{
	  virtual_dir top;

#ifdef ENABLE_CHECKING
	  if (VARRAY_USED (vd->subdirs))
	    abort ();
#endif
	  varray_destroy (&vd->subdirs);

	  /* Remove VD from parent's subdirectories.  */
	  top = VARRAY_TOP (vd->parent->subdirs, virtual_dir);
	  VARRAY_ACCESS (vd->parent->subdirs, vd->subdir_index, virtual_dir)
	    = top;
	  VARRAY_POP (vd->parent->subdirs);
	  top->subdir_index = vd->subdir_index;
	  vd->parent->attr.nlink--;
	  vd->parent->attr.ctime = vd->parent->attr.mtime = time (NULL);

	  /* Delete the virtual_fh from the table of virtual directories.  */
	  slot = htab_find_slot (vd_htab_name, vd, NO_INSERT);
#ifdef ENABLE_CHECKING
	  if (!slot)
	    abort ();
#endif
	  htab_clear_slot (vd_htab_name, slot);
	  slot = htab_find_slot_with_hash (vd_htab, &vd->fh,
					   VIRTUAL_DIR_HASH (vd), NO_INSERT);
#ifdef ENABLE_CHECKING
	  if (!slot)
	    abort ();
#endif
	  htab_clear_slot (vd_htab, slot);
	  free (vd->name);
	  zfsd_mutex_unlock (&vd->mutex);
	  zfsd_mutex_destroy (&vd->mutex);
	  pool_free (vd_pool, vd);
	}
      else
	zfsd_mutex_unlock (&vd->mutex);
    }
}

/* Create the virtual root directory.  */

virtual_dir
virtual_root_create ()
{
  virtual_dir root;
  void **slot;

  zfsd_mutex_lock (&vd_mutex);
  root = (virtual_dir) pool_alloc (vd_pool);
  root->fh = root_fh;
  root->parent = NULL;
  root->name = xstrdup ("");
  varray_create (&root->subdirs, sizeof (virtual_dir), 16);
  root->subdir_index = 0;
  root->n_mountpoints = 1;
  root->vol = NULL;
  virtual_dir_set_fattr (root);

  zfsd_mutex_init (&root->mutex);

  /* Insert the root into hash table.  */
  slot = htab_find_slot_with_hash (vd_htab, &root->fh,
				   VIRTUAL_DIR_HASH (root), INSERT);
  *slot = root;
  zfsd_mutex_unlock (&vd_mutex);

  return root;
}

/* Destroy virtual root directory.  */

void
virtual_root_destroy (virtual_dir root)
{
  void **slot;

  free (root->name);

#ifdef ENABLE_CHECKING
  if (VARRAY_USED (root->subdirs))
    abort ();
#endif
  varray_destroy (&root->subdirs);

  zfsd_mutex_lock (&vd_mutex);
  slot = htab_find_slot_with_hash (vd_htab, &root->fh,
				   VIRTUAL_DIR_HASH (root), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (vd_htab, slot);
  pool_free (vd_pool, root);
  zfsd_mutex_unlock (&vd_mutex);
}

/* Create the virtual mountpoint for volume VOL.  */

virtual_dir
virtual_mountpoint_create (volume vol)
{
  varray subpath;
  virtual_dir vd, parent, tmp;
  char *s, *mountpoint;
  unsigned int i;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  mountpoint = xstrdup (vol->mountpoint);
  varray_create (&subpath, sizeof (char *), 16);

  /* Split the path.  */
  s = mountpoint;
  while (*s != 0)
    {
      while (*s == '/')
	s++;

      if (*s == 0)
	break;

      VARRAY_PUSH (subpath, s, char *);
      while (*s != 0 && *s != '/')
	s++;
      if (*s == '/')
	*s++ = 0;
    }

  /* Create the components of the path.  */
  zfsd_mutex_lock (&vd_mutex);
  vd = root;
  zfsd_mutex_lock (&root->mutex);
  for (i = 0; i < VARRAY_USED (subpath); i++)
    {
      parent = vd;
      s = VARRAY_ACCESS (subpath, i, char *);

      vd = vd_lookup_name (parent, s);
      if (!vd)
	vd = virtual_dir_create (parent, s);
#ifdef ENABLE_CHECKING
      if (!VIRTUAL_FH_P (vd->fh))
	abort ();
#endif
      zfsd_mutex_unlock (&parent->mutex);
    }
  varray_destroy (&subpath);
  vd->vol = vol;
  vol->root_vd = vd;
  zfsd_mutex_unlock (&vd->mutex);

  /* Increase the count of volumes in subtree.  */
  for (tmp = vd; tmp; tmp = tmp->parent)
    {
      zfsd_mutex_lock (&tmp->mutex);
      tmp->n_mountpoints++;
      zfsd_mutex_unlock (&tmp->mutex);
    }
  zfsd_mutex_unlock (&vd_mutex);

  free (mountpoint);

  return vd;
}

/* Destroy the virtual mountpoint of volume VOL.  */

void
virtual_mountpoint_destroy (volume vol)
{
  zfsd_mutex_lock (&vd_mutex);
  zfsd_mutex_lock (&vol->root_vd->mutex);
  virtual_dir_destroy (vol->root_vd);
  zfsd_mutex_unlock (&vd_mutex);
}

/* Set the file attributes of virtual directory VD.  */

void
virtual_dir_set_fattr (virtual_dir vd)
{
  vd->attr.type = FT_DIR;
  vd->attr.mode = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
  vd->attr.nlink = 2;
  vd->attr.uid = DEFAULT_ZFS_UID;
  vd->attr.gid = DEFAULT_ZFS_GID;
  vd->attr.rdev = 0;
  vd->attr.size = 0;
  vd->attr.blocks = 0;
  vd->attr.blksize = 4096;
  vd->attr.generation = 0;
  vd->attr.fversion = 0;
  vd->attr.sid = vd->fh.sid;
  vd->attr.vid = vd->fh.vid;
  vd->attr.dev = vd->fh.dev;
  vd->attr.ino = vd->fh.ino;
  vd->attr.atime = time (NULL);
  vd->attr.mtime = vd->attr.atime;
  vd->attr.ctime = vd->attr.atime;
}

/* Print the virtual directory VD and its subdirectories to file F
   indented by INDENT spaces.  */

static void
print_virtual_tree_node (FILE *f, virtual_dir vd, unsigned int indent)
{
  unsigned int i;

  for (i = 0; i < indent; i++)
    fputc (' ', f);

  fprintf (f, "'%s'", vd->name);
  if (vd->vol)
    fprintf (f, "; VOLUME = '%s'", vd->vol->name);
  fputc ('\n', f);

  for (i = 0; i < VARRAY_USED (vd->subdirs); i++)
    print_virtual_tree_node (f, VARRAY_ACCESS (vd->subdirs, i, virtual_dir),
			     indent + 1);
}

/* Print the virtual tree to file F.  */

void
print_virtual_tree (FILE *f)
{
  print_virtual_tree_node (f, root, 0);
}

/* Print the virtual tree to STDERR.  */

void
debug_virtual_tree ()
{
  print_virtual_tree (stderr);
}

/* Initialize data structures in FH.C.  */

void
initialize_fh_c ()
{
  /* Data structures for file handles.  */
  zfsd_mutex_init (&fh_pool_mutex);
  fh_pool = create_alloc_pool ("fh_pool", sizeof (struct internal_fh_def),
			       1023, &fh_pool_mutex);

  /* Data structures for dentries.  */
  zfsd_mutex_init (&dentry_pool_mutex);
  dentry_pool = create_alloc_pool ("dentry_pool",
				   sizeof (struct internal_dentry_def),
				   1023, &dentry_pool_mutex);

  /* Data structures for virtual directories.  */
  zfsd_mutex_init (&vd_mutex);
  vd_pool = create_alloc_pool ("vd_pool", sizeof (struct virtual_dir_def),
			       127, &vd_mutex);
  vd_htab = htab_create (100, virtual_dir_hash, virtual_dir_eq, NULL,
			 &vd_mutex);
  vd_htab_name = htab_create (100, virtual_dir_hash_name, virtual_dir_eq_name,
			      NULL, &vd_mutex);

  /* Data structures for cleanup of file handles.  */
  zfsd_mutex_init (&cleanup_dentry_mutex);
  cleanup_dentry_heap = fibheap_new (1020, &cleanup_dentry_mutex);
  if (pthread_create (&cleanup_dentry_thread, NULL, cleanup_dentry_thread_main, NULL))
    {
      message (-1, stderr, "pthread_create() failed\n");
    }

  root = virtual_root_create ();
}

/* Destroy data structures in FH.C.  */

void
cleanup_fh_c ()
{
  virtual_root_destroy (root);

  /* Data structures for file handles.  */
  zfsd_mutex_lock (&fh_pool_mutex);
#ifdef ENABLE_CHECKING
  if (fh_pool->elts_free < fh_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in fh_pool.\n",
	     fh_pool->elts_allocated - fh_pool->elts_free);
#endif
  free_alloc_pool (fh_pool);
  zfsd_mutex_unlock (&fh_pool_mutex);
  zfsd_mutex_destroy (&fh_pool_mutex);

  /* Data structures dentries.  */
  zfsd_mutex_lock (&dentry_pool_mutex);
#ifdef ENABLE_CHECKING
  if (dentry_pool->elts_free < dentry_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in dentry_pool.\n",
	     dentry_pool->elts_allocated - dentry_pool->elts_free);
#endif
  free_alloc_pool (dentry_pool);
  zfsd_mutex_unlock (&dentry_pool_mutex);
  zfsd_mutex_destroy (&dentry_pool_mutex);

  /* Data structures for virtual directories.  */
  zfsd_mutex_lock (&vd_mutex);
  htab_destroy (vd_htab_name);
  htab_destroy (vd_htab);
#ifdef ENABLE_CHECKING
  if (vd_pool->elts_free < vd_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in vd_pool.\n",
	     vd_pool->elts_allocated - vd_pool->elts_free);
#endif
  free_alloc_pool (vd_pool);
  zfsd_mutex_unlock (&vd_mutex);
  zfsd_mutex_destroy (&vd_mutex);

  /* Data structures for cleanup of file handles.  */
  zfsd_mutex_lock (&cleanup_dentry_mutex);
  fibheap_delete (cleanup_dentry_heap);
  zfsd_mutex_unlock (&cleanup_dentry_mutex);
  zfsd_mutex_destroy (&cleanup_dentry_mutex);
}
