/* File handle functions.
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
#include "cap.h"
#include "alloc-pool.h"
#include "crc32.h"
#include "hashtab.h"
#include "fibheap.h"
#include "log.h"
#include "memory.h"
#include "network.h"
#include "varray.h"
#include "journal.h"
#include "metadata.h"
#include "zfs_prot.h"
#include "user-group.h"
#include "dir.h"

/* File handle of ZFS root.  */
zfs_fh root_fh = {NODE_NONE, VOLUME_ID_VIRTUAL, VIRTUAL_DEVICE, ROOT_INODE, 1};

/* Static undefined ZFS file handle.  */
zfs_fh undefined_fh;

/* The virtual directory root.  */
static virtual_dir root;

/* Allocation pool for file handles.  */
static alloc_pool fh_pool;

/* Allocation pool for dentries.  */
static alloc_pool dentry_pool;

/* Hash table of used file handles, searched by local_fh.  */
htab_t fh_htab;

/* Hash table of used dentries, searched by fh->local_fh.  */
htab_t dentry_htab;

/* Hash table of used dentries, searched by (parent->fh->local_fh, name).  */
htab_t dentry_htab_name;

/* Mutes for file handles and dentries.  */
pthread_mutex_t fh_mutex;

/* Key for array of locked file handles.  */
static pthread_key_t lock_info_key;

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
  (crc32_update (crc32_buffer ((VD)->name.str, (VD)->name.len),		\
		 &(VD)->parent->fh, sizeof (zfs_fh)))

static internal_dentry make_space_in_conflict_dir (volume *volp,
						   internal_dentry *conflictp,
						   bool exists, zfs_fh *fh);

/* Return the fibheap key for dentry DENTRY.  */

static fibheapkey_t
dentry_key (internal_dentry dentry)
{
  if (CONFLICT_DIR_P (dentry->fh->local_fh))
    {
      internal_dentry tmp;
      unsigned int i;

      for (i = 0; i < VARRAY_USED (dentry->fh->subdentries); i++)
	{
	  tmp = VARRAY_ACCESS (dentry->fh->subdentries, i, internal_dentry);
	  if ((tmp->fh->cap || tmp->fh->level != LEVEL_UNLOCKED)
	      && tmp->next == tmp)
	    return FIBHEAPKEY_MAX;
	}
    }

  if ((dentry->fh->cap || dentry->fh->level != LEVEL_UNLOCKED)
       && dentry->next == dentry)
    return FIBHEAPKEY_MAX;

  return (fibheapkey_t) dentry->last_use;
}

/* Return true if dentry DENTRY should have a node in CLEANUP_DENTRY_HEAP.  */

static bool
dentry_should_have_cleanup_node (internal_dentry dentry)
{
  TRACE ("");

  /* Root dentry can't be deleted.  */
  if (!dentry->parent)
    return false;

  if (dentry->deleted)
    return false;

  if (CONFLICT_DIR_P (dentry->fh->local_fh))
    {
      internal_dentry tmp;
      unsigned int i;

      for (i = 0; i < VARRAY_USED (dentry->fh->subdentries); i++)
	{
	  tmp = VARRAY_ACCESS (dentry->fh->subdentries, i, internal_dentry);
	  if (tmp->fh->attr.type == FT_DIR
	      && VARRAY_USED (tmp->fh->subdentries) != 0)
	    return false;
	}

      return true;
    }

  /* Directory dentry which has some subdentries can't be deleted.  */
  if (dentry->fh->attr.type == FT_DIR
      && VARRAY_USED (dentry->fh->subdentries) != 0)
    return false;

  return true;
}

/* Update the cleanup node of dentry DENTRY.  */

static void
dentry_update_cleanup_node (internal_dentry dentry)
{
  TRACE ("");
#ifdef ENABLE_CHECKING
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#endif

  if (dentry->parent && CONFLICT_DIR_P (dentry->parent->fh->local_fh))
    {
      zfsd_mutex_lock (&cleanup_dentry_mutex);
      if (dentry->heap_node)
	{
	  fibheap_delete_node (cleanup_dentry_heap, dentry->heap_node);
	  dentry->heap_node = NULL;
	}
      zfsd_mutex_unlock (&cleanup_dentry_mutex);
      dentry = dentry->parent;
    }

  dentry->last_use = time (NULL);
  zfsd_mutex_lock (&cleanup_dentry_mutex);
  if (dentry_should_have_cleanup_node (dentry))
    {
      if (dentry->heap_node)
	{
	  fibheap_replace_key (cleanup_dentry_heap, dentry->heap_node,
			       dentry_key (dentry));
	}
      else
	{
	  dentry->heap_node = fibheap_insert (cleanup_dentry_heap,
					      dentry_key (dentry), dentry);
	}
    }
  else
    {
      if (dentry->heap_node)
	{
	  fibheap_delete_node (cleanup_dentry_heap, dentry->heap_node);
	  dentry->heap_node = NULL;
	}
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
cleanup_unused_dentries (void)
{
  fibheapkey_t threshold;
  internal_dentry dentry;
  zfs_fh fh[1024];
  int i, n;

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

	  for (i = 0; i < n; i++)
	    {
	      zfsd_mutex_lock (&fh_mutex);

	      dentry = dentry_lookup (&fh[i]);
	      if (!dentry)
		{
		  zfsd_mutex_unlock (&fh_mutex);
		  continue;
		}

	      /* We may have added a dentry to it
		 while CLEANUP_DENTRY_MUTEX was unlocked.  */
	      if (dentry_should_have_cleanup_node (dentry))
		{
		  release_dentry (dentry);
		  zfsd_mutex_unlock (&fh_mutex);
		  continue;
		}

	      /* We may have looked up DENTRY again
		 so we may have updated LAST_USE
		 or there are capabilities associated with
		 the file handle and this is its only dentry.  */
	      if (dentry_key (dentry) >= threshold)
		{
		  /* Reinsert the file handle to heap.  */
		  dentry_update_cleanup_node (dentry);
		  release_dentry (dentry);
		  zfsd_mutex_unlock (&fh_mutex);
		  continue;
		}

	      internal_dentry_destroy (dentry, true);
	      zfsd_mutex_unlock (&fh_mutex);
	    }
	}
    }
  while (n > 0);
}

/* Main function of thread freeing file handles unused for a long time.  */

static void *
cleanup_dentry_thread_main (ATTRIBUTE_UNUSED void *data)
{
  thread_disable_signals ();
  pthread_setspecific (thread_name_key, "IFH cleanup thread");

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

  return NULL;
}

/* Hash function for internal file handle X.  */
static hash_t
internal_fh_hash (const void *x)
{
  return INTERNAL_FH_HASH ((internal_fh) x);
}

/* Hash function for internal dentry X, computed from fh->local_fh.  */

static hash_t
internal_dentry_hash (const void *x)
{
  return INTERNAL_DENTRY_HASH ((internal_dentry) x);
}

/* Hash function for internal dentry X, computed from parent->fh and name.  */

static hash_t
internal_dentry_hash_name (const void *x)
{
  return INTERNAL_DENTRY_HASH_NAME ((internal_dentry) x);
}

/* Compare an internal file handle XX with client's file handle YY.  */

static int
internal_fh_eq (const void *xx, const void *yy)
{
  zfs_fh *x = &((internal_fh) xx)->local_fh;
  zfs_fh *y = (zfs_fh *) yy;

  return (x->ino == y->ino && x->dev == y->dev
	  && x->vid == y->vid && x->sid == y->sid
	  && x->gen == y->gen);
}

/* Set array of lock info for current thread to LI. */

void
set_lock_info (lock_info *li)
{
  int i;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (pthread_setspecific (lock_info_key, li))
    abort ();
#else
  pthread_setspecific (lock_info_key, li);
#endif

  for (i = 0; i < MAX_LOCKED_FILE_HANDLES; i++)
    {
      li[i].fh = NULL;
      li[i].level = LEVEL_UNLOCKED;
    }
}

/* Add file handle FH locked to level LEVEL to list of file handles
   owned by current thread.  */

void
set_owned (internal_fh fh, unsigned int level)
{
  lock_info *li;
  int i;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh->mutex);

  li = (lock_info *) pthread_getspecific (lock_info_key);
#ifdef ENABLE_CHECKING
  if (level != LEVEL_SHARED && level != LEVEL_EXCLUSIVE)
    abort ();
  if (!li)
    abort ();
#endif

  for (i = 0; i < MAX_LOCKED_FILE_HANDLES; i++)
    {
      if (li[i].fh == NULL)
	{
#ifdef ENABLE_CHECKING
	  if (li[i].level != LEVEL_UNLOCKED)
	    abort ();
#endif
	  li[i].fh = fh;
	  li[i].level = level;
	  return;
	}
    }

#ifdef ENABLE_CHECKING
    abort ();
#endif
}

/* Remove file handle FH from list of file handles owned by current thread.  */

static void
clear_owned (internal_fh fh)
{
  lock_info *li;
  int i;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh->mutex);

  li = (lock_info *) pthread_getspecific (lock_info_key);
#ifdef ENABLE_CHECKING
  if (!li)
    abort ();
#endif

  for (i = 0; i < MAX_LOCKED_FILE_HANDLES; i++)
    {
      if (li[i].fh == fh)
	{
#ifdef ENABLE_CHECKING
	  if (li[i].level != LEVEL_SHARED && li[i].level != LEVEL_EXCLUSIVE)
	    abort ();
#endif
	  li[i].fh = NULL;
	  li[i].level = LEVEL_UNLOCKED;
	  return;
	}
    }

#ifdef ENABLE_CHECKING
  abort ();
#endif
}

/* Return true if file handle FH is owned by current thread.  */

static bool
is_owned (internal_fh fh)
{
  lock_info *li;
  int i;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh->mutex);

  li = (lock_info *) pthread_getspecific (lock_info_key);
#ifdef ENABLE_CHECKING
  if (!li)
    abort ();
#endif

  for (i = 0; i < MAX_LOCKED_FILE_HANDLES; i++)
    {
      if (li[i].fh == fh)
	return true;
    }

  return false;
}

/* Return the level which file handle is locked by current thread.  */

static unsigned int
get_level (internal_fh fh)
{
  lock_info *li;
  int i;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh->mutex);

  li = (lock_info *) pthread_getspecific (lock_info_key);
#ifdef ENABLE_CHECKING
  if (!li)
    abort ();
#endif

  for (i = 0; i < MAX_LOCKED_FILE_HANDLES; i++)
    {
      if (li[i].fh == fh)
	return li[i].level;
    }

  return LEVEL_UNLOCKED;
}

/* Compare an internal file handle XX with client's file handle YY.  */

static int
internal_dentry_eq (const void *xx, const void *yy)
{
  zfs_fh *x = &((internal_dentry) xx)->fh->local_fh;
  zfs_fh *y = (zfs_fh *) yy;

  return (x->ino == y->ino && x->dev == y->dev
	  && x->vid == y->vid && x->sid == y->sid
	  && x->gen == y->gen);
}

/* Compare two internal file handles XX and YY whether they have same parent
   and file name.  */

static int
internal_dentry_eq_name (const void *xx, const void *yy)
{
  internal_dentry x = (internal_dentry) xx;
  internal_dentry y = (internal_dentry) yy;

  return (x->parent == y->parent
	  && x->name.len == y->name.len
  	  && strcmp (x->name.str, y->name.str) == 0);
}

/* Find the internal file handle or virtual directory for zfs_fh FH
   and set *VOLP, *DENTRYP and VDP according to it.
   If DELETE_VOLUME_P is true and the volume should be deleted do not
   lookup the file handle and delete the volume if there are no file handles
   locked on it.  */

int32_t
zfs_fh_lookup (zfs_fh *fh, volume *volp, internal_dentry *dentryp,
	       virtual_dir *vdp, bool delete_volume_p)
{
  int32_t res;

  TRACE ("");

  if (VIRTUAL_FH_P (*fh))
    zfsd_mutex_lock (&vd_mutex);

  res = zfs_fh_lookup_nolock (fh, volp, dentryp, vdp, delete_volume_p);

  if (VIRTUAL_FH_P (*fh))
    zfsd_mutex_unlock (&vd_mutex);
  else if (res == ZFS_OK)
    zfsd_mutex_unlock (&fh_mutex);

  return res;
}

/* Find the internal file handle or virtual directory for zfs_fh FH
   and set *VOLP, *DENTRYP and VDP according to it.
   This function is similar to FH_LOOKUP but the big locks must be locked.
   If DELETE_VOLUME_P is true and the volume should be deleted do not
   lookup the file handle and delete the volume if there are no file handles
   locked on it.  */

int32_t
zfs_fh_lookup_nolock (zfs_fh *fh, volume *volp, internal_dentry *dentryp,
		      virtual_dir *vdp, bool delete_volume_p)
{
  hash_t hash = ZFS_FH_HASH (fh);

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (fh->gen == 0)
    abort ();
#endif

  if (VIRTUAL_FH_P (*fh))
    {
      virtual_dir vd;

      CHECK_MUTEX_LOCKED (&vd_mutex);

      vd = (virtual_dir) htab_find_with_hash (vd_htab, fh, hash);
      if (!vd)
	return ENOENT;

      zfsd_mutex_lock (&vd->mutex);
#ifdef ENABLE_CHECKING
      if (vd->deleted > 0 && !vd->busy)
	abort ();
#endif

      if (volp)
	{
	  zfsd_mutex_lock (&volume_mutex);
	  if (vd->vol)
	    zfsd_mutex_lock (&vd->vol->mutex);
	  zfsd_mutex_unlock (&volume_mutex);
	  *volp = vd->vol;
	}
      if (dentryp)
	*dentryp = NULL;
      *vdp = vd;
    }
  else
    {
      volume vol = NULL;
      internal_dentry dentry;

      zfsd_mutex_lock (&fh_mutex);

      if (volp)
	{
	  vol = volume_lookup (fh->vid);
	  if (!vol)
	    {
	      zfsd_mutex_unlock (&fh_mutex);
	      return ENOENT;
	    }
	  if (delete_volume_p && vol->delete_p)
	    {
	      if (vol->n_locked_fhs == 0)
		volume_delete (vol);
	      else
		zfsd_mutex_unlock (&vol->mutex);
	      zfsd_mutex_unlock (&fh_mutex);
	      return ENOENT;
	    }
#ifdef ENABLE_CHECKING
	  if (!delete_volume_p && vol->n_locked_fhs == 0)
	    abort ();
#endif

	  if (!vol->local_path.str && !volume_master_connected (vol))
	    {
	      zfsd_mutex_unlock (&vol->mutex);
	      zfsd_mutex_unlock (&fh_mutex);
	      return ESTALE;
	    }
	}

      dentry = (internal_dentry) htab_find_with_hash (dentry_htab, fh, hash);
      if (!dentry)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	  return ZFS_STALE;
	}

      acquire_dentry (dentry);
#ifdef ENABLE_CHECKING
      if (volp && vol->local_path.str && vol->master == this_node
	  && !zfs_fh_undefined (dentry->fh->meta.master_fh))
	abort ();
#endif

      if (volp)
	*volp = vol;
      *dentryp = dentry;
      if (vdp)
	*vdp = NULL;
    }

  return ZFS_OK;
}

/* Lock DENTRY and update time of last use.  */

void
acquire_dentry (internal_dentry dentry)
{
  TRACE ("");

  zfsd_mutex_lock (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (dentry->deleted)
    abort ();
#endif
  dentry_update_cleanup_node (dentry);
}

/* Update time of last use of DENTRY and unlock it.  */

void
release_dentry (internal_dentry dentry)
{
  TRACE ("");
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  dentry_update_cleanup_node (dentry);
  zfsd_mutex_unlock (&dentry->fh->mutex);
}

/* Return virtual directory for file handle FH.  */

virtual_dir
vd_lookup (zfs_fh *fh)
{
  virtual_dir vd;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vd_mutex);

  vd = (virtual_dir) htab_find_with_hash (vd_htab, fh, ZFS_FH_HASH (fh));
  if (vd)
    {
      zfsd_mutex_lock (&vd->mutex);
#ifdef ENABLE_CHECKING
      if (vd->deleted > 0 && !vd->busy)
	abort ();
#endif
    }

  return vd;
}

/* Return the virtual directory for NAME in virtual directory PARENT.  */

virtual_dir
vd_lookup_name (virtual_dir parent, string *name)
{
  virtual_dir vd;
  struct virtual_dir_def tmp_vd;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vd_mutex);
  CHECK_MUTEX_LOCKED (&parent->mutex);

  tmp_vd.parent = parent;
  tmp_vd.name = *name;

  vd = (virtual_dir) htab_find (vd_htab_name, &tmp_vd);
  if (vd)
    {
      zfsd_mutex_lock (&vd->mutex);
#ifdef ENABLE_CHECKING
      if (vd->deleted > 0 && !vd->busy)
	abort ();
#endif
    }

  return vd;
}

/* Return the internal dentry for file handle FH.  */

internal_dentry
dentry_lookup (zfs_fh *fh)
{
  internal_dentry dentry;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);

#ifdef ENABLE_CHECKING
  if (fh->gen == 0)
    abort ();
#endif

  dentry = (internal_dentry) htab_find_with_hash (dentry_htab, fh,
						  ZFS_FH_HASH (fh));
  if (dentry)
    acquire_dentry (dentry);

  return dentry;
}

/* Return the internal dentry for NAME in directory PARENT.  */

internal_dentry
dentry_lookup_name (internal_dentry parent, string *name)
{
  struct internal_dentry_def tmp;
  internal_dentry dentry;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&parent->fh->mutex);

  tmp.parent = parent;
  tmp.name = *name;

  dentry = (internal_dentry) htab_find (dentry_htab_name, &tmp);
  if (dentry)
    acquire_dentry (dentry);

  return dentry;
}

/* Lock dentry *DENTRYP on volume *VOLP to level LEVEL.
   Store the local ZFS file handle to TMP_FH.  */

int32_t
internal_dentry_lock (unsigned int level, volume *volp,
		      internal_dentry *dentryp, zfs_fh *tmp_fh)
{
  int32_t r;
  bool wait_for_locked;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (volp == NULL)
    abort ();
  if (dentryp == NULL)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*dentryp)->fh->mutex);
#ifdef ENABLE_CHECKING
  if (level > LEVEL_EXCLUSIVE)
    abort ();
#endif

  message (4, stderr, "FH %p LOCK %u, by %lu at %s:%d\n",
	   (void *) (*dentryp)->fh, level, (unsigned long) pthread_self (),
	   __FILE__, __LINE__);

  *tmp_fh = (*dentryp)->fh->local_fh;
  wait_for_locked = ((*dentryp)->fh->level + level > LEVEL_EXCLUSIVE);
  if (wait_for_locked)
    {
      /* Mark the dentry so that nobody else can lock dentry before us.  */
      if (level > (*dentryp)->fh->level)
	(*dentryp)->fh->level = level;

      zfsd_mutex_unlock (&(*volp)->mutex);

      while ((*dentryp)->fh->level + level > LEVEL_EXCLUSIVE)
	zfsd_cond_wait (&(*dentryp)->fh->cond, &(*dentryp)->fh->mutex);
      zfsd_mutex_unlock (&(*dentryp)->fh->mutex);

      r = zfs_fh_lookup_nolock (tmp_fh, volp, dentryp, NULL, true);
      if (r != ZFS_OK)
	return r;
    }

  message (4, stderr, "FH %p LOCKED %u, by %lu at %s:%d\n",
	   (void *) (*dentryp)->fh, level, (unsigned long) pthread_self (),
	   __FILE__, __LINE__);

  (*dentryp)->fh->level = level;
  (*dentryp)->fh->users++;
  (*volp)->n_locked_fhs++;
  set_owned ((*dentryp)->fh, level);

  if (!wait_for_locked)
    {
      release_dentry (*dentryp);
      zfsd_mutex_unlock (&(*volp)->mutex);

      r = zfs_fh_lookup_nolock (tmp_fh, volp, dentryp, NULL, false);
#ifdef ENABLE_CHECKING
      if (r != ZFS_OK)
	abort ();
#endif
    }

  return ZFS_OK;
}

/* Unlock dentry DENTRY.  */

void
internal_dentry_unlock (volume vol, internal_dentry dentry)
{
  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (dentry->fh->level == LEVEL_UNLOCKED)
    abort ();
  if (dentry->fh->users == 0)
    abort ();
#endif

  message (4, stderr, "FH %p UNLOCK, by %lu at %s:%d\n",
	   (void *) dentry->fh, (unsigned long) pthread_self (),
	   __FILE__, __LINE__);

  vol->n_locked_fhs--;
  zfsd_mutex_unlock (&vol->mutex);
  dentry->fh->users--;
  clear_owned (dentry->fh);
  if (dentry->fh->users == 0)
    {
      dentry->fh->level = LEVEL_UNLOCKED;
      destroy_unused_capabilities (dentry->fh);
      if (dentry->deleted)
	{
	  internal_dentry_destroy (dentry, true);
	}
      else
	{
	  zfsd_cond_signal (&dentry->fh->cond);
	  release_dentry (dentry);
	}
    }
  else
    release_dentry (dentry);
  zfsd_mutex_unlock (&fh_mutex);
}

/* Lock 2 dentries on volume *VOLP, lock *DENTRY1P to level LEVEL1 and
   *DENTRY2P to level LEVEL2.  Use TMP_FH1 and TMP_FH2 to lookup them.  */

int32_t
internal_dentry_lock2 (unsigned int level1, unsigned int level2, volume *volp,
		       internal_dentry *dentry1p, internal_dentry *dentry2p,
		       zfs_fh *tmp_fh1, zfs_fh *tmp_fh2)
{
  int32_t r, r2;

  TRACE ("");
#ifdef ENABLE_CHECKING
  /* internal_dentry_lock2 should be used only by zfs_link and zfs_rename
     thus the files should be on the same device.  */
  if (tmp_fh1->sid != tmp_fh2->sid
      || tmp_fh1->vid != tmp_fh2->vid
      || tmp_fh1->dev != tmp_fh2->dev)
    abort ();
#endif

  if (tmp_fh1->ino == tmp_fh2->ino)
    {
      r = internal_dentry_lock ((level1 > level2 ? level1 : level2),
				volp, dentry1p, tmp_fh1);
      if (r != ZFS_OK)
	return r;

      *dentry2p = *dentry1p;
      return ZFS_OK;
    }

  if (tmp_fh1->ino < tmp_fh2->ino)
    {
      release_dentry (*dentry2p);

      r = internal_dentry_lock (level1, volp, dentry1p, tmp_fh1);
      if (r != ZFS_OK)
	return r;

      release_dentry (*dentry1p);
      zfsd_mutex_unlock (&(*volp)->mutex);
      zfsd_mutex_unlock (&fh_mutex);

      r = zfs_fh_lookup (tmp_fh2, volp, dentry2p, NULL, true);
      if (r != ZFS_OK)
	goto out1;

      r = internal_dentry_lock (level2, volp, dentry2p, tmp_fh2);
      if (r != ZFS_OK)
	{
out1:
	  r2 = zfs_fh_lookup_nolock (tmp_fh1, volp, dentry1p, NULL, false);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  internal_dentry_unlock (*volp, *dentry1p);
	  return r;
	}

      release_dentry (*dentry2p);
      zfsd_mutex_unlock (&(*volp)->mutex);
      zfsd_mutex_unlock (&fh_mutex);
    }
  else /* if (tmp_fh1->ino > tmp_fh2->ino) */
    {
      release_dentry (*dentry1p);

      r = internal_dentry_lock (level2, volp, dentry2p, tmp_fh2);
      if (r != ZFS_OK)
	return r;

      release_dentry (*dentry2p);
      zfsd_mutex_unlock (&(*volp)->mutex);
      zfsd_mutex_unlock (&fh_mutex);

      r = zfs_fh_lookup (tmp_fh1, volp, dentry1p, NULL, true);
      if (r != ZFS_OK)
	goto out2;

      r = internal_dentry_lock (level1, volp, dentry1p, tmp_fh1);
      if (r != ZFS_OK)
	{
out2:
	  r2 = zfs_fh_lookup_nolock (tmp_fh2, volp, dentry2p, NULL, false);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  internal_dentry_unlock (*volp, *dentry2p);
	  return r;
	}

      release_dentry (*dentry1p);
      zfsd_mutex_unlock (&(*volp)->mutex);
      zfsd_mutex_unlock (&fh_mutex);
    }

  /* Lookup dentries again.  */
  r2 = zfs_fh_lookup_nolock (tmp_fh1, volp, dentry1p, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  *dentry2p = dentry_lookup (tmp_fh2);
#ifdef ENABLE_CHECKING
  if (!*dentry2p)
    abort ();
#endif

  return ZFS_OK;
}

/* Set master file handle of file handle FH on volume VOL to MASTER_FH.  */

bool
set_master_fh (volume vol, internal_fh fh, zfs_fh *master_fh)
{
  TRACE ("");
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  if (zfs_fh_undefined (*master_fh))
    return true;

  if (INTERNAL_FH_HAS_LOCAL_PATH (fh))
    return set_metadata_master_fh (vol, fh, master_fh);

  fh->meta.master_fh = *master_fh;
  return true;
}

/* Clear metadata in file handle FH.  */

static void
clear_meta (internal_fh fh)
{
  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh->mutex);

  memset (&fh->meta, 0, offsetof (metadata, master_fh));
  zfs_fh_undefine (fh->meta.master_fh);
}

/* Create a new internal file handle on volume VOL with local file handle
   LOCAL_FH, remote file handle MASTER_FH, attributes ATTR, lock it to level
   LEVEL and store it to hash tables.  */

static internal_fh
internal_fh_create (zfs_fh *local_fh, zfs_fh *master_fh, fattr *attr,
		    metadata *meta, volume vol, unsigned int level)
{
  internal_fh fh;
  void **slot;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  fh = (internal_fh) pool_alloc (fh_pool);
  fh->local_fh = *local_fh;
  fh->attr = *attr;
  fh->cap = NULL;
  fh->ndentries = 0;
  fh->updated = NULL;
  fh->modified = NULL;
  fh->interval_tree_users = 0;
  fh->journal = NULL;
  fh->level = level;
  fh->users = 0;
  fh->fd = -1;
  fh->generation = 0;
  fh->flags = 0;

  message (4, stderr, "FH %p CREATED, by %lu\n", (void *) fh,
	   (unsigned long) pthread_self ());

  if (fh->attr.type == FT_DIR)
    varray_create (&fh->subdentries, sizeof (internal_dentry), 16);

  zfsd_mutex_init (&fh->mutex);
  zfsd_mutex_lock (&fh->mutex);

  if (level != LEVEL_UNLOCKED)
    {
#ifdef ENABLE_CHECKING
      if (level != LEVEL_SHARED && level != LEVEL_EXCLUSIVE)
	abort ();
#endif
      fh->users++;
      vol->n_locked_fhs++;
      set_owned (fh, level);
    }

  slot = htab_find_slot_with_hash (fh_htab, &fh->local_fh,
				   INTERNAL_FH_HASH (fh), INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = fh;

  if (INTERNAL_FH_HAS_LOCAL_PATH (fh))
    {
#ifdef ENABLE_CHECKING
      if (local_fh->dev != meta->dev
	  || local_fh->ino != meta->ino
	  || local_fh->gen != meta->gen)
	abort ();
      if (meta->slot_status != VALID_SLOT)
	abort ();
#endif
      fh->meta = *meta;
      set_attr_version (&fh->attr, &fh->meta);
      attr->version = fh->attr.version;

      if (fh->attr.type == FT_DIR)
	{
	  fh->journal = journal_create (5, &fh->mutex);
	  if (!read_journal (vol, fh))
	    vol->delete_p = true;
	}
    }
  else
    clear_meta (fh);

  if (!vol->delete_p
      && !set_master_fh (vol, fh, master_fh))
    {
      vol->delete_p = true;
      clear_meta (fh);
    }

  return fh;
}

/* Destroy almost everything of the internal file handle FH
   except mutex and file handle itself.  */

static void
internal_fh_destroy_stage1 (internal_fh fh)
{
  void **slot;
  internal_cap cap, next;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

#ifdef ENABLE_CHECKING
  if (fh->ndentries != 0)
    abort ();
#endif

  message (4, stderr, "FH %p DESTROY, by %lu\n", (void *) fh,
	   (unsigned long) pthread_self ());

  /* Destroy capabilities associated with file handle.  */
  for (cap = fh->cap; cap; cap = next)
    {
      next = cap->next;
      cap->busy = 1;
      put_capability (cap, fh, NULL);
    }

  if (fh->attr.type == FT_DIR)
    varray_destroy (&fh->subdentries);

  if (fh->journal)
    journal_destroy (fh->journal);

  slot = htab_find_slot_with_hash (fh_htab, &fh->local_fh,
				   INTERNAL_FH_HASH (fh), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (fh_htab, slot);
}

/* Destroy the rest of the internal file handle FH, i.e. the mutex
   and file handle itself.  */

static void
internal_fh_destroy_stage2 (internal_fh fh)
{
  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  message (4, stderr, "FH %p DESTROYED, by %lu\n", (void *) fh,
	   (unsigned long) pthread_self ());

  zfsd_mutex_unlock (&fh->mutex);
  zfsd_mutex_destroy (&fh->mutex);
  pool_free (fh_pool, fh);
}

/* Print the contents of hash table HTAB to file F.  */

void
print_fh_htab (FILE *f)
{
  void **slot;

  HTAB_FOR_EACH_SLOT (fh_htab, slot)
    {
      internal_fh fh = (internal_fh) *slot;

      fprintf (f, "[%u,%u,%u,%u,%u] ", fh->local_fh.sid, fh->local_fh.vid,
	       fh->local_fh.dev, fh->local_fh.ino, fh->local_fh.gen);
      fprintf (f, "[%u,%u,%u,%u,%u] ", fh->meta.master_fh.sid,
	       fh->meta.master_fh.vid, fh->meta.master_fh.dev,
	       fh->meta.master_fh.ino, fh->meta.master_fh.gen);
      fprintf (f, "L%d ", fh->level);
      fprintf (f, "\n");
    }
}

/* Print the contents of hash table of filehandles HTAB to STDERR.  */

void
debug_fh_htab (void)
{
  print_fh_htab (stderr);
}

/* Print subdentries of dentry DENTRY to file F.  */

void
print_subdentries (FILE *f, internal_dentry dentry)
{
  unsigned int i;
  internal_dentry subdentry;

  if (dentry->fh->attr.type != FT_DIR)
    return;

  for (i = 0; i < VARRAY_USED (dentry->fh->subdentries); i++)
    {
      subdentry = VARRAY_ACCESS (dentry->fh->subdentries, i, internal_dentry);

      fprintf (f, "%s [%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32
	       "]\n", subdentry->name.str, subdentry->fh->local_fh.sid,
	       subdentry->fh->local_fh.vid, subdentry->fh->local_fh.dev,
	       subdentry->fh->local_fh.ino, subdentry->fh->local_fh.gen);
    }
}

/* Print subdentries of dentry DENTRY to STDERR.  */

void
debug_subdentries (internal_dentry dentry)
{
  print_subdentries (stderr, dentry);
}

/* Add DENTRY to list of dentries of PARENT.  */

static void
internal_dentry_add_to_dir (internal_dentry parent, internal_dentry dentry)
{
  void **slot;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (!parent)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&parent->fh->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

#ifdef ENABLE_CHECKING
  if (dentry->parent)
    abort ();
#endif
  dentry->parent = parent;

  dentry->dentry_index = VARRAY_USED (parent->fh->subdentries);
  VARRAY_PUSH (parent->fh->subdentries, dentry, internal_dentry);
  dentry_update_cleanup_node (parent);
  dentry_update_cleanup_node (dentry);

  slot = htab_find_slot (dentry_htab_name, dentry, INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = dentry;
}

/* Delete DENTRY from the list of dentries of its parent.  */

static void
internal_dentry_del_from_dir (internal_dentry dentry)
{
  internal_dentry top;
  void **slot;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  if (!dentry->parent)
    return;

  CHECK_MUTEX_LOCKED (&dentry->parent->fh->mutex);

  top = VARRAY_TOP (dentry->parent->fh->subdentries, internal_dentry);
  VARRAY_ACCESS (dentry->parent->fh->subdentries, dentry->dentry_index,
		 internal_dentry) = top;
  VARRAY_POP (dentry->parent->fh->subdentries);
  top->dentry_index = dentry->dentry_index;

  slot = htab_find_slot (dentry_htab_name, dentry, NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (dentry_htab_name, slot);

  dentry_update_cleanup_node (dentry->parent);
  dentry->parent = NULL;
}

/* Create a new internal dentry NAME in directory PARENT on volume VOL and
   internal file handle for local file handle LOCAL_FH and master file handle
   MASTER_FH with attributes ATTR and store it to hash tables.
   Lock the newly created file handle to level LEVEL.  */

static internal_dentry
internal_dentry_create (zfs_fh *local_fh, zfs_fh *master_fh, volume vol,
			internal_dentry parent, string *name, fattr *attr,
			metadata *meta, unsigned int level)
{
  internal_dentry dentry;
  internal_fh fh;
  void **slot;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (parent)
    CHECK_MUTEX_LOCKED (&parent->fh->mutex);
#endif

  dentry = (internal_dentry) pool_alloc (dentry_pool);
  dentry->parent = NULL;
  xstringdup (&dentry->name, name);
  dentry->next = dentry;
  dentry->prev = dentry;
  dentry->last_use = time (NULL);
  dentry->heap_node = NULL;
  dentry->deleted = false;

  /* Find the internal file handle in hash table, create it if it does not
     exist.  */
  if (CONFLICT_DIR_P (*local_fh))
    {
      do
	{
	  vol->last_conflict_ino++;
	  if (vol->last_conflict_ino == 0)
	    vol->last_conflict_ino++;

	  local_fh->ino = vol->last_conflict_ino;
	  slot = htab_find_slot_with_hash (fh_htab, local_fh,
					   ZFS_FH_HASH (local_fh), INSERT);
	}
      while (*slot);
    }
  else
    {
      slot = htab_find_slot_with_hash (fh_htab, local_fh,
				       ZFS_FH_HASH (local_fh), INSERT);
    }
  if (!*slot)
    {
      fh = internal_fh_create (local_fh, master_fh, attr, meta, vol, level);
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
      dentry_update_cleanup_node (dentry);
      internal_dentry_add_to_dir (parent, dentry);

      if (INTERNAL_FH_HAS_LOCAL_PATH (fh))
	{
	  if (!metadata_hardlink_insert (vol, &fh->local_fh,
					 parent->fh->local_fh.dev,
					 parent->fh->local_fh.ino, name))
	    {
	      vol->delete_p = true;
	    }
	}
    }

  slot = htab_find_slot_with_hash (dentry_htab, &fh->local_fh,
				   INTERNAL_DENTRY_HASH (dentry), INSERT);
  if (*slot)
    {
      internal_dentry old = (internal_dentry) *slot;

      dentry->next = old->next;
      dentry->prev = old;
      old->next->prev = dentry;
      old->next = dentry;

      if (parent)
	{
	  /* Lower the fibheap keys if they are FIBHEAPKEY_MAX.  */
	  if (dentry->heap_node && dentry->heap_node->key == FIBHEAPKEY_MAX)
	    dentry_update_cleanup_node (dentry);
	  for (old = dentry->next; old != dentry; old = old->next)
	    if (old->heap_node && old->heap_node->key == FIBHEAPKEY_MAX)
	      dentry_update_cleanup_node (old);
	}
    }
  *slot = dentry;

  return dentry;
}

/* Return dentry for file NAME in directory DIR on volume VOL.
   If it does not exist create it.  Update its local file handle to
   LOCAL_FH, master file handle to MASTER_FH and attributes to ATTR.  */

internal_dentry
get_dentry (zfs_fh *local_fh, zfs_fh *master_fh, volume vol,
	    internal_dentry dir, string *name, fattr *attr, metadata *meta)
{
  internal_dentry dentry;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (dir)
    {
      dentry = dentry_lookup_name (dir, name);
      if (dentry && CONFLICT_DIR_P (dentry->fh->local_fh))
	{
	  internal_dentry sdentry;

	  sdentry = add_file_to_conflict_dir (vol, dentry, true, local_fh, attr,
					      meta);
	  if (try_resolve_conflict (dentry))
	    {
	      dentry = dentry_lookup_name (dir, name);
#ifdef ENABLE_CHECKING
	      if (dentry && CONFLICT_DIR_P (dentry->fh->local_fh))
		abort ();
#endif
	    }
	  else
	    {
	      release_dentry (dentry);
	      acquire_dentry (sdentry);
	      return sdentry;
	    }
	}
    }
  else
    {
      dentry = vol->root_dentry;
      if (dentry)
	{
	  acquire_dentry (dentry);
#ifdef ENABLE_CHECKING
	  if (!REGULAR_FH_P (dentry->fh->local_fh))
	    abort ();
#endif
	}
    }

  if (dentry)
    {
      CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

      if (!ZFS_FH_EQ (dentry->fh->local_fh, *local_fh)
	  || (!ZFS_FH_EQ (dentry->fh->meta.master_fh, *master_fh)
	      && !zfs_fh_undefined (dentry->fh->meta.master_fh)
	      && !zfs_fh_undefined (*master_fh)))
	{
	  uint32_t vid = 0;
	  zfs_fh tmp;
	  unsigned int level;

	  if (dir)
	    {
#ifdef ENABLE_CHECKING
	      if (dir->fh->level == LEVEL_UNLOCKED
		  && dentry->fh->level == LEVEL_UNLOCKED)
		abort ();
#endif
	      tmp = dir->fh->local_fh;
	      release_dentry (dir);
	    }
	  else
	    vid = vol->id;
	  zfsd_mutex_unlock (&vol->mutex);

	  level = get_level (dentry->fh);
	  internal_dentry_destroy (dentry, true);

	  if (dir)
	    {
	      int32_t r;

	      zfsd_mutex_unlock (&fh_mutex);
	      r = zfs_fh_lookup_nolock (&tmp, &vol, &dir, NULL, false);
#ifdef ENABLE_CHECKING
	      if (r != ZFS_OK)
		abort ();
#endif
	    }
	  else
	    {
	      vol = volume_lookup (vid);
	    }
	  dentry = internal_dentry_create (local_fh, master_fh, vol, dir, name,
					   attr, meta, level);
	}
      else
	{
	  if (zfs_fh_undefined (dentry->fh->meta.master_fh))
	    set_master_fh (vol, dentry->fh, master_fh);

	  if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
	    set_attr_version (attr, &dentry->fh->meta);
	  dentry->fh->attr = *attr;
	}
    }
  else
    dentry = internal_dentry_create (local_fh, master_fh, vol, dir, name,
				     attr, meta, LEVEL_UNLOCKED);

  if (!dir)
    vol->root_dentry = dentry;

  return dentry;
}

/* Destroy dentry NAME in directory DIR (whose file handle is DIR_FH)
   on volume VOL.  */

void
delete_dentry (volume *volp, internal_dentry *dirp, string *name,
	       zfs_fh *dir_fh)
{
  internal_dentry dentry;
  int32_t r2;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*dirp)->fh->mutex);
#ifdef ENABLE_CHECKING
  if ((*dirp)->fh->level == LEVEL_UNLOCKED)
    abort();
  if (CONFLICT_DIR_P (*dir_fh))
    abort ();
#endif

  dentry = dentry_lookup_name (*dirp, name);
  if (dentry)
    {
      if (CONFLICT_DIR_P (dentry->fh->local_fh))
	{
	  zfs_fh tmp_fh;
	  internal_dentry subdentry;

	  release_dentry (*dirp);
	  zfsd_mutex_unlock (&(*volp)->mutex);
	  subdentry = dentry_lookup_name (dentry, &this_node->name);
	  if (subdentry)
	    {
	      tmp_fh = dentry->fh->local_fh;
	      release_dentry (dentry);

	      internal_dentry_destroy (subdentry, true);

	      dentry = dentry_lookup (&tmp_fh);
	      if (!try_resolve_conflict (dentry))
		release_dentry (dentry);
	    }
	  else
	    release_dentry (dentry);
	}
      else
	{
	  release_dentry (*dirp);
	  zfsd_mutex_unlock (&(*volp)->mutex);

	  internal_dentry_destroy (dentry, true);
	}

      zfsd_mutex_unlock (&fh_mutex);
      r2 = zfs_fh_lookup_nolock (dir_fh, volp, dirp, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif
    }
}

/* Create a new internal dentry NAME in directory PARENT on volume VOL for
   file DENTRY.  */

internal_dentry
internal_dentry_link (internal_dentry orig, volume vol,
		      internal_dentry parent, string *name)
{
  internal_dentry dentry, old;
  void **slot;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (!parent)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&orig->fh->mutex);
  CHECK_MUTEX_LOCKED (&parent->fh->mutex);

#ifdef ENABLE_CHECKING
  dentry = dentry_lookup_name (parent, name);
  if (dentry)
    abort ();
#endif

  dentry = (internal_dentry) pool_alloc (dentry_pool);
  dentry->parent = NULL;
  xstringdup (&dentry->name, name);
  dentry->fh = orig->fh;
  orig->fh->ndentries++;
  dentry->next = dentry;
  dentry->prev = dentry;
  dentry->last_use = time (NULL);
  dentry->heap_node = NULL;
  dentry->deleted = false;

  dentry_update_cleanup_node (dentry);
  internal_dentry_add_to_dir (parent, dentry);

  slot = htab_find_slot_with_hash (dentry_htab, &orig->fh->local_fh,
				   INTERNAL_DENTRY_HASH (dentry), INSERT);
  if (*slot)
    {
      old = (internal_dentry) *slot;
      dentry->next = old->next;
      dentry->prev = old;
      old->next->prev = dentry;
      old->next = dentry;
    }
#ifdef ENABLE_CHECKING
  else
    abort ();
#endif
  *slot = dentry;

  return dentry;
}

/* Move internal dentry for file FROM_NAME in FROM_DIR to be a subdentry
   of TO_DIR with name TO_NAME on volume VOL.  */

void
internal_dentry_move (volume vol, internal_dentry from_dir, string *from_name,
		      internal_dentry to_dir, string *to_name)
{
  internal_dentry dentry, dentry2, sdentry;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&from_dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&to_dir->fh->mutex);

  dentry = dentry_lookup_name (from_dir, from_name);
  if (!dentry)
    return;

#ifdef ENABLE_CHECKING
  if (1)
    {
      internal_dentry tmp;

      /* Check whether we are not moving DENTRY to its subtree.  */
      for (tmp = to_dir; tmp; tmp = tmp->parent)
	if (tmp == dentry)
	  abort ();
    }
#endif

  /* Delete DENTRY from FROM_DIR's directory entries.  */
  if (CONFLICT_DIR_P (dentry->fh->local_fh))
    {
      sdentry = dentry_lookup_name (dentry, &this_node->name);
#ifdef ENABLE_CHECKING
      if (!sdentry)
	abort ();
#endif
      internal_dentry_del_from_dir (sdentry);

      if (!try_resolve_conflict (dentry))
	release_dentry (dentry);
      dentry = sdentry;
    }
  else
    internal_dentry_del_from_dir (dentry);

  /* Insert DENTRY to TO_DIR.  */
#ifdef ENABLE_CHECKING
  dentry2 = dentry_lookup_name (to_dir, to_name);
  if (dentry2)
    abort ();
#endif

  free (dentry->name.str);
  xstringdup (&dentry->name, to_name);
  internal_dentry_add_to_dir (to_dir, dentry);

  release_dentry (dentry);
}

/* Destroy internal dentry DENTRY.  Clear vol->root_dentry if
   CLEAR_VOLUME_ROOT.  */

void
internal_dentry_destroy (internal_dentry dentry, bool clear_volume_root)
{
  zfs_fh tmp_fh;
  void **slot;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  tmp_fh = dentry->fh->local_fh;

  if (dentry->fh->attr.type == FT_DIR)
    {
      /* Destroy subtree first.  */
      while (VARRAY_USED (dentry->fh->subdentries))
	{
	  internal_dentry subdentry;
	  internal_dentry tmp1, tmp2;

	  subdentry = VARRAY_TOP (dentry->fh->subdentries, internal_dentry);
	  zfsd_mutex_lock (&subdentry->fh->mutex);
	  zfsd_mutex_unlock (&dentry->fh->mutex);
	  internal_dentry_destroy (subdentry, false);

	  tmp1 = dentry_lookup (&tmp_fh);
	  tmp2 = tmp1;
	  do
	    {
	      if (tmp2 == dentry)
		break;
	      tmp2 = tmp2->next;
	    }
	  while (tmp2 != tmp1);

	  /* DENTRY could not be found, it is already deleted.  */
	  if (tmp2 != dentry)
	    return;
	}
    }

  /* At this point, DENTRY is always a leaf.  */

#ifdef ENABLE_CHECKING
  if (dentry->fh->level != LEVEL_UNLOCKED && dentry->deleted)
    abort ();
#endif

  /* If we are holding the lock unlock it first.  */
  if (is_owned (dentry->fh))
    {
      volume vol;

      message (4, stderr, "FH %p DELETE, by %lu\n", (void *) dentry->fh,
	       (unsigned long) pthread_self ());

      vol = volume_lookup (tmp_fh.vid);
      vol->n_locked_fhs--;
      zfsd_mutex_unlock (&vol->mutex);

      dentry->fh->users--;
      clear_owned (dentry->fh);
      if (dentry->fh->users == 0)
	dentry->fh->level = LEVEL_UNLOCKED;
    }

  if (dentry->fh->level != LEVEL_UNLOCKED)
    {
#ifdef ENABLE_CHECKING
      internal_dentry tmp1, tmp2;
#endif
      internal_fh fh = dentry->fh;

      do
	{
	  zfsd_mutex_unlock (&fh->mutex);

	  /* FH can't be deleted while it is locked.  */
	  zfsd_cond_wait (&fh->cond, &fh_mutex);

#ifdef ENABLE_CHECKING
	  tmp1 = dentry_lookup (&tmp_fh);
	  tmp2 = tmp1;
	  do
	    {
	      if (tmp2 == dentry)
		break;
	      tmp2 = tmp2->next;
	    }
	  while (tmp2 != tmp1);

	  if (tmp2 != dentry)
	    abort ();
#else
	  /* Because FH could not be deleted we can lock it again.  */
	  zfsd_mutex_lock (&fh->mutex);
#endif
	}
      while (dentry->fh->level != LEVEL_UNLOCKED);
    }

  if (dentry->deleted)
    {
      /* There already is a thread which tries to delete DENTRY.  */
      zfsd_mutex_unlock (&dentry->fh->mutex);
      return;
    }

  /* Mark DENTRY as deleted and wake up other threads trying to delete it.  */
  dentry->deleted = true;
  zfsd_cond_broadcast (&dentry->fh->cond);

  dentry_update_cleanup_node (dentry);

  if (dentry->parent)
    {
      internal_dentry parent = dentry->parent;

      zfsd_mutex_lock (&parent->fh->mutex);
      internal_dentry_del_from_dir (dentry);
      zfsd_mutex_unlock (&parent->fh->mutex);
    }
  else if (clear_volume_root)
    {
      volume vol;

      vol = volume_lookup (dentry->fh->local_fh.vid);
      if (vol)
	{
	  vol->root_dentry = NULL;
	  zfsd_mutex_unlock (&vol->mutex);
	}
    }

  slot = htab_find_slot_with_hash (dentry_htab, &dentry->fh->local_fh,
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
      htab_clear_slot (dentry_htab, slot);
      internal_fh_destroy_stage1 (dentry->fh);
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
    }

  /* Let other threads waiting for DENTRY to finish using DENTRY.  */
  if (CONFLICT_DIR_P (dentry->fh->local_fh))
    {
      zfsd_mutex_unlock (&fh_mutex);
      local_invalidate (dentry);
    }
  else
    {
      zfsd_mutex_unlock (&dentry->fh->mutex);
      zfsd_mutex_unlock (&fh_mutex);
    }

  /* Because FH could not be destroyed yet we can lock it again.  */
  zfsd_mutex_lock (&fh_mutex);
  zfsd_mutex_lock (&dentry->fh->mutex);

  /* At this moment, we are the only thread which wants do to something
     with DENTRY (at least if pthread_mutex is just).  */

  if (dentry->next == dentry)
    internal_fh_destroy_stage2 (dentry->fh);
  else
    zfsd_mutex_unlock (&dentry->fh->mutex);

  free (dentry->name.str);
  pool_free (dentry_pool, dentry);
}

/* Create conflict directory for local file handle LOCAL_FH with attributes
   according to ATTR and name NAME in directory DIR on volume VOL.
   If such conflict directory already exists update the local file handle
   and attributes and return it.  */

internal_dentry
create_conflict (volume vol, internal_dentry dir, string *name,
		 zfs_fh *local_fh, fattr *attr)
{
  zfs_fh tmp_fh;
  fattr tmp_attr;
  internal_dentry conflict, dentry;
  node nod;

  TRACE ("");
#ifdef ENABLE_CHECKING
  /* Two directories can't be in conflict, neither the volume root can.  */
  if (!dir)
    abort ();
#endif

again:
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  dentry = dentry_lookup_name (dir, name);
  if (dentry && CONFLICT_DIR_P (dentry->fh->local_fh))
    return dentry;

#ifdef ENABLE_CHECKING
  if (dir->fh->level == LEVEL_UNLOCKED)
    abort ();
#endif

  if (dentry)
    {
      if (!ZFS_FH_EQ (dentry->fh->local_fh, *local_fh))
	{
	  tmp_fh = dir->fh->local_fh;
	  release_dentry (dir);
	  zfsd_mutex_unlock (&vol->mutex);

	  internal_dentry_destroy (dentry, true);
	  dentry = NULL;
	  zfsd_mutex_unlock (&fh_mutex);

	  /* This succeeds because DIR was locked so it can't have been
	     deleted meanwhile.  */
	  zfs_fh_lookup_nolock (&tmp_fh, &vol, &dir, NULL, false);
	}
      else
	internal_dentry_del_from_dir (dentry);
    }

  tmp_fh.sid = NODE_NONE;
  tmp_fh.vid = vol->id;
  tmp_fh.dev = VIRTUAL_DEVICE;
  tmp_fh.ino = vol->last_conflict_ino;
  tmp_fh.gen = 1;
  tmp_attr.dev = tmp_fh.dev;
  tmp_attr.ino = tmp_fh.ino;
  tmp_attr.version = 0;
  tmp_attr.type = FT_DIR;
  tmp_attr.mode = S_IRWXU | S_IRWXG | S_IRWXO;
  tmp_attr.nlink = 4;
  tmp_attr.uid = attr->uid;
  tmp_attr.gid = attr->gid;
  tmp_attr.rdev = 0;
  tmp_attr.size = 0;
  tmp_attr.blocks = 0;
  tmp_attr.blksize = 4096;
  tmp_attr.atime = time (NULL);
  tmp_attr.ctime = tmp_attr.atime;
  tmp_attr.mtime = tmp_attr.atime;

  conflict = internal_dentry_create (&tmp_fh, &undefined_fh, vol, dir,
				     name, &tmp_attr, NULL, LEVEL_UNLOCKED);

  if (dentry)
    {
      free (dentry->name.str);
      nod = node_lookup (local_fh->sid);
#ifdef ENABLE_CHECKING
      if (!nod)
	abort ();
#endif
      xstringdup (&dentry->name, &nod->name);
      zfsd_mutex_unlock (&nod->mutex);

      internal_dentry_add_to_dir (conflict, dentry);

      /* Invalidate DENTRY.  */
      tmp_fh = dir->fh->local_fh;
      release_dentry (dir);
      release_dentry (conflict);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&fh_mutex);
      local_invalidate (dentry);

      /* This succeeds because DIR was locked so it can't have been
	 deleted meanwhile.  */
      zfs_fh_lookup_nolock (&tmp_fh, &vol, &dir, NULL, false);
      goto again;
    }

  return conflict;
}

/* If there is a dentry in place for file FH in conflict directrory CONFLICT
   on volume VOL delete it and return NULL.
   If FH is already there return its dentry.  */

static internal_dentry
make_space_in_conflict_dir (volume *volp, internal_dentry *conflictp,
			    bool exists, zfs_fh *fh)
{
  zfs_fh tmp_fh;
  internal_dentry dentry;
  unsigned int i;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*conflictp)->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!CONFLICT_DIR_P ((*conflictp)->fh->local_fh))
    abort ();
  if ((*conflictp)->fh->attr.type != FT_DIR)
    abort ();
  if ((*conflictp)->fh->level == LEVEL_UNLOCKED
      && (*conflictp)->parent->fh->level == LEVEL_UNLOCKED)
    abort ();
  if (CONFLICT_DIR_P (*fh))
    abort ();
  if (exists && (*volp)->id != fh->vid)
    abort ();
#endif

  for (i = 0; i < VARRAY_USED ((*conflictp)->fh->subdentries); i++)
    {
      dentry = VARRAY_ACCESS ((*conflictp)->fh->subdentries, i,
			      internal_dentry);
      acquire_dentry (dentry);

#ifdef ENABLE_CHECKING
      if (CONFLICT_DIR_P (dentry->fh->local_fh))
	abort ();
#endif
      if (dentry->fh->local_fh.sid == fh->sid)
	{
	  if (!exists || !ZFS_FH_EQ (dentry->fh->local_fh, *fh))
	    {
	      tmp_fh = (*conflictp)->fh->local_fh;
	      release_dentry (*conflictp);
	      zfsd_mutex_unlock (&(*volp)->mutex);

	      internal_dentry_destroy (dentry, true);

	      *volp = volume_lookup (tmp_fh.vid);
	      *conflictp = dentry_lookup (&tmp_fh);

	      return NULL;
	    }
	  else
	    return dentry;
	}
      else
	release_dentry (dentry);
    }

  return NULL;
}

/* Add a dentry to conflict dir CONFLICT on volume VOL.
   If EXISTS is true the file really exists so create a dentry with file handle
   FH, attributes ATTR and metadata META; otherwise create a virtual symlink
   representing non-existing file.  */

internal_dentry
add_file_to_conflict_dir (volume vol, internal_dentry conflict, bool exists,
			  zfs_fh *fh, fattr *attr, metadata *meta)
{
  zfs_fh tmp_fh;
  internal_dentry dentry;
  node nod;
  string *name;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&conflict->fh->mutex);

  dentry = make_space_in_conflict_dir (&vol, &conflict, exists, fh);
  if (dentry)
    {
      if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
	set_attr_version (attr, &dentry->fh->meta);
      dentry->fh->attr = *attr;
      release_dentry (dentry);
      return dentry;
    }
#ifdef ENABLE_CHECKING
  else
    {
      if (!vol)
	abort ();
      if (!conflict)
	abort ();
    }
#endif

  nod = vol->master;
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);

  if (exists)
    {
      zfs_fh *master_fh;

      if (fh->sid == this_node->id)
	{
	  name = &this_node->name;
	  master_fh = &undefined_fh;
	}
      else
	{
	  name = &nod->name;
	  master_fh = fh;
	}

      dentry = internal_dentry_create (fh, master_fh, vol, conflict, name,
				       attr, meta, LEVEL_UNLOCKED);
    }
  else
    {
      if (fh->sid == this_node->id)
	{
	  name = &this_node->name;
	  tmp_fh.sid = this_node->id;
	  tmp_fh.ino = nod->id;
	}
      else
	{
	  /* This should never happen because initially we do not create
	     non-existing remote dentry.  Then, user can manipulate the type
	     of local dentry only, or completelly delete the remote dentry.  */
	  abort ();

	  name = &nod->name;
	  tmp_fh.sid = nod->id;
	  tmp_fh.ino = this_node->id;
	}
      tmp_fh.vid = VOLUME_ID_VIRTUAL;
      tmp_fh.dev = VIRTUAL_DEVICE;
      tmp_fh.gen = 1;
      attr->dev = tmp_fh.dev;
      attr->ino = tmp_fh.ino;
      attr->version = 0;
      attr->type = FT_LNK;
      attr->mode = S_IRWXU | S_IRWXG | S_IRWXO;
      attr->nlink = 1;
      attr->uid = attr->uid;
      attr->gid = attr->gid;
      attr->rdev = 0;
      attr->size = name->len;
      attr->blocks = 0;
      attr->blksize = 4096;
      attr->atime = time (NULL);
      attr->ctime = attr->atime;
      attr->mtime = attr->atime;
      dentry = internal_dentry_create (&tmp_fh, &undefined_fh, vol, conflict,
				       name, attr, NULL, LEVEL_UNLOCKED);
    }

  zfsd_mutex_unlock (&nod->mutex);
  release_dentry (dentry);
  return dentry;
}

/* Try resolve CONFLICT, return true if it was resolved.  */

bool
try_resolve_conflict (internal_dentry conflict)
{
  internal_dentry dentry1, dentry2, parent;
  string swp;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&conflict->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!conflict->parent)
    abort ();
#endif

  switch (VARRAY_USED (conflict->fh->subdentries))
    {
      case 0:
	internal_dentry_destroy (conflict, true);
	return true;

      case 1:
	dentry1 = VARRAY_ACCESS (conflict->fh->subdentries, 0,
				 internal_dentry);
	acquire_dentry (dentry1);
	if (REGULAR_FH_P (dentry1->fh->local_fh))
	  {
	    if (INTERNAL_FH_HAS_LOCAL_PATH (dentry1->fh))
	      {
		parent = conflict->parent;
		acquire_dentry (parent);
		internal_dentry_del_from_dir (dentry1);
		internal_dentry_del_from_dir (conflict);

		swp = dentry1->name;
		dentry1->name = conflict->name;
		conflict->name = swp;

		internal_dentry_add_to_dir (parent, dentry1);
		release_dentry (parent);
		release_dentry (dentry1);
		internal_dentry_destroy (conflict, false);
	      }
	    else
	      {
		release_dentry (dentry1);
		internal_dentry_destroy (conflict, true);
	      }
	  }
	else if (NON_EXIST_FH_P (dentry1->fh->local_fh))
	  {
	    release_dentry (dentry1);
	    internal_dentry_destroy (conflict, true);
	  }
#ifdef ENABLE_CHECKING
	else
	  abort ();
#endif
	return true;

      case 2:
	dentry1 = VARRAY_ACCESS (conflict->fh->subdentries, 0,
				 internal_dentry);
	dentry2 = VARRAY_ACCESS (conflict->fh->subdentries, 1,
				 internal_dentry);

	acquire_dentry (dentry1);
	acquire_dentry (dentry2);
#ifdef ENABLE_CHECKING
	if (!REGULAR_FH_P (dentry1->fh->local_fh)
	    && !NON_EXIST_FH_P (dentry1->fh->local_fh))
	  abort ();
	if (!REGULAR_FH_P (dentry2->fh->local_fh)
	    && !NON_EXIST_FH_P (dentry2->fh->local_fh))
	  abort ();
#endif

	if (REGULAR_FH_P (dentry1->fh->local_fh)
	    && REGULAR_FH_P (dentry2->fh->local_fh))
	  {
	    release_dentry (dentry1);
	    release_dentry (dentry2);
	    return false;
	  }
	if (NON_EXIST_FH_P (dentry1->fh->local_fh)
	    && NON_EXIST_FH_P (dentry2->fh->local_fh))
	  {
	    release_dentry (dentry1);
	    release_dentry (dentry2);
	    internal_dentry_destroy (conflict, false);
	    return true;
	  }
	release_dentry (dentry1);
	release_dentry (dentry2);
	break;

      default:
	abort ();
    }

  return false;
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
	  && x->vid == y->vid && x->sid == y->sid
	  /* && x->gen == y->gen */);
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
	  && x->name.len == y->name.len
	  && strcmp (x->name.str, y->name.str) == 0);
}

/* Create a new virtual directory NAME in virtual directory PARENT.  */

virtual_dir
virtual_dir_create (virtual_dir parent, const char *name)
{
  virtual_dir vd;
  static uint32_t last_virtual_ino;
  void **slot;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vd_mutex);
  CHECK_MUTEX_LOCKED (&parent->mutex);

  last_virtual_ino++;
  if (last_virtual_ino <= ROOT_INODE)
    last_virtual_ino = ROOT_INODE + 1;

  vd = (virtual_dir) pool_alloc (vd_pool);
  vd->fh.sid = NODE_NONE;
  vd->fh.vid = VOLUME_ID_VIRTUAL;
  vd->fh.dev = VIRTUAL_DEVICE;
  vd->fh.ino = last_virtual_ino;
  vd->fh.gen = 1;
  vd->parent = parent;
  xmkstring (&vd->name, name);
  vd->vol = NULL;
  vd->cap = NULL;
  virtual_dir_set_fattr (vd);
  vd->n_mountpoints = 0;
  vd->busy = false;
  vd->users = 0;
  vd->deleted = 0;

  zfsd_mutex_init (&vd->mutex);
  zfsd_mutex_lock (&vd->mutex);

  varray_create (&vd->subdirs, sizeof (virtual_dir), 16);
  vd->subdir_index = VARRAY_USED (parent->subdirs);
  VARRAY_PUSH (parent->subdirs, vd, virtual_dir);
  vd->parent->attr.nlink++;
  vd->parent->attr.ctime = vd->parent->attr.mtime = time (NULL);

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
  unsigned int count;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vd_mutex);
  CHECK_MUTEX_LOCKED (&vd->mutex);

  /* Check the path to root.  */
  count = 1;
  for (; vd; vd = parent)
    {
      if (vd->busy)
	{
	  vd->deleted++;
	  zfsd_mutex_unlock (&vd->mutex);
	  return;
	}

      parent = vd->parent;
      if (parent)
	zfsd_mutex_lock (&parent->mutex);
      if (vd->deleted > 1)
	count += vd->deleted - 1;
#ifdef ENABLE_CHECKING
      if (vd->n_mountpoints < count)
	abort ();
#endif
      vd->n_mountpoints -= count;
      if (vd->n_mountpoints == 0)
	{
	  virtual_dir top;


	  /* Destroy capability associated with virtual directroy.  */
	  if (vd->cap)
	    {
	      vd->cap->busy = 1;
	      put_capability (vd->cap, NULL, vd);
	    }

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
	  free (vd->name.str);
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
virtual_root_create (void)
{
  virtual_dir root;
  void **slot;

  TRACE ("");

  zfsd_mutex_lock (&vd_mutex);
  root = (virtual_dir) pool_alloc (vd_pool);
  root->fh = root_fh;
  root->parent = NULL;
  xmkstring (&root->name, "");
  varray_create (&root->subdirs, sizeof (virtual_dir), 16);
  root->subdir_index = 0;
  root->vol = NULL;
  root->cap = NULL;
  virtual_dir_set_fattr (root);
  root->n_mountpoints = 1;
  root->busy = false;
  root->users = 0;
  root->deleted = 0;

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

  TRACE ("");

  zfsd_mutex_lock (&vd_mutex);
  zfsd_mutex_lock (&root->mutex);

  /* Destroy capability associated with virtual directroy.  */
  if (root->cap)
    {
      root->cap->busy = 1;
      put_capability (root->cap, NULL, root);
    }

#ifdef ENABLE_CHECKING
  if (VARRAY_USED (root->subdirs))
    abort ();
#endif
  varray_destroy (&root->subdirs);

  slot = htab_find_slot_with_hash (vd_htab, &root->fh,
				   VIRTUAL_DIR_HASH (root), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif
  htab_clear_slot (vd_htab, slot);
  free (root->name.str);
  zfsd_mutex_unlock (&root->mutex);
  zfsd_mutex_destroy (&root->mutex);
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

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vol->mutex);

  mountpoint = xstrdup (vol->mountpoint);
  varray_create (&subpath, sizeof (string), 8);

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
      string str;

      parent = vd;
      s = VARRAY_ACCESS (subpath, i, char *);

      str.str = s;
      str.len = strlen (s);
      vd = vd_lookup_name (parent, &str);
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
  TRACE ("");
  CHECK_MUTEX_LOCKED (&vd_mutex);

  zfsd_mutex_lock (&vol->root_vd->mutex);
  virtual_dir_destroy (vol->root_vd);
}

/* Set the file attributes of virtual directory VD.  */

void
virtual_dir_set_fattr (virtual_dir vd)
{
  TRACE ("");

  vd->attr.dev = vd->fh.dev;
  vd->attr.ino = vd->fh.ino;
  vd->attr.version = 0;
  vd->attr.type = FT_DIR;
  vd->attr.mode = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
  vd->attr.nlink = 2;
  vd->attr.uid = DEFAULT_ZFS_UID;
  vd->attr.gid = DEFAULT_ZFS_GID;
  vd->attr.rdev = 0;
  vd->attr.size = 0;
  vd->attr.blocks = 0;
  vd->attr.blksize = 4096;
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

  fprintf (f, "'%s'", vd->name.str);
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
debug_virtual_tree (void)
{
  print_virtual_tree (stderr);
}

/* Initialize data structures in FH.C.  */

void
initialize_fh_c (void)
{
  zfs_fh_undefine (undefined_fh);

  /* Data structures for file handles and dentries.  */
  zfsd_mutex_init (&fh_mutex);
  pthread_key_create (&lock_info_key, NULL);
  fh_pool = create_alloc_pool ("fh_pool", sizeof (struct internal_fh_def),
			       1023, &fh_mutex);
  dentry_pool = create_alloc_pool ("dentry_pool",
				   sizeof (struct internal_dentry_def),
				   1023, &fh_mutex);
  fh_htab = htab_create (250, internal_fh_hash, internal_fh_eq, NULL,
			 &fh_mutex);
  dentry_htab = htab_create (250, internal_dentry_hash, internal_dentry_eq,
			     NULL, &fh_mutex);
  dentry_htab_name = htab_create (250, internal_dentry_hash_name,
				  internal_dentry_eq_name, NULL, &fh_mutex);

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
  if (pthread_create (&cleanup_dentry_thread, NULL, cleanup_dentry_thread_main,
		      NULL))
    {
      message (-1, stderr, "pthread_create() failed\n");
    }

  root = virtual_root_create ();
}

/* Destroy data structures in FH.C.  */

void
cleanup_fh_c (void)
{
  virtual_root_destroy (root);

  wait_for_thread_to_die (&cleanup_dentry_thread, NULL);

  /* Data structures for file handles and dentries.  */
  zfsd_mutex_lock (&fh_mutex);
#ifdef ENABLE_CHECKING
  if (fh_pool->elts_free < fh_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in fh_pool.\n",
	     fh_pool->elts_allocated - fh_pool->elts_free);
  if (dentry_pool->elts_free < dentry_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in dentry_pool.\n",
	     dentry_pool->elts_allocated - dentry_pool->elts_free);
#endif
  htab_destroy (fh_htab);
  htab_destroy (dentry_htab);
  htab_destroy (dentry_htab_name);
  free_alloc_pool (fh_pool);
  free_alloc_pool (dentry_pool);
  zfsd_mutex_unlock (&fh_mutex);
  zfsd_mutex_destroy (&fh_mutex);
  pthread_key_delete (lock_info_key);

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
