/* Capability functions.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "pthread.h"
#include "cap.h"
#include "crc32.h"
#include "md5.h"
#include "alloc-pool.h"
#include "hashtab.h"
#include "fh.h"
#include "dir.h"
#include "file.h"
#include "log.h"
#include "random.h"
#include "volume.h"
#include "util.h"
#include "zfs_prot.h"

/* Allocation pool for capabilities.  */
static alloc_pool cap_pool;

/* Mutex for cap_pool.  */
static pthread_mutex_t cap_mutex;

/*! Compute the verification for the capability and return true on success.
    \param cap Capability which the verification should be computed for.  */

static bool
internal_cap_compute_verify (internal_cap cap)
{
  MD5Context ctx;
  unsigned char random[CAP_RANDOM_LEN];

  TRACE ("");

  if (!full_read (fd_urandom, random, sizeof (random)))
    RETURN_BOOL (false);

  MD5Init (&ctx);
  MD5Update (&ctx, (unsigned char *) &cap->local_cap.fh,
	     sizeof (cap->local_cap.fh));
  MD5Update (&ctx, (unsigned char *) &cap->local_cap.flags,
	     sizeof (cap->local_cap.flags));
  MD5Update (&ctx, random, sizeof (random));
  MD5Final ((unsigned char *) cap->local_cap.verify, &ctx);

  if (verbose >= 3)
    {
      fprintf (stderr, "Created verify ");
      print_hex_buffer (cap->local_cap.verify, ZFS_VERIFY_LEN, stderr);
    }

  RETURN_BOOL (true);
}

/* Verify capability CAP by comparing with ICAP.  */

static int32_t
verify_capability (zfs_cap *cap, internal_cap icap)
{
  TRACE ("");

  if (memcmp (cap->verify, icap->local_cap.verify, ZFS_VERIFY_LEN) == 0)
    RETURN_INT (ZFS_OK);

  if (verbose >= 3)
    {
      fprintf (stderr, "Using verify ");
      print_hex_buffer (cap->verify, ZFS_VERIFY_LEN, stderr);
      fprintf (stderr, "It should be ");
      print_hex_buffer (icap->local_cap.verify, ZFS_VERIFY_LEN, stderr);
    }

  RETURN_INT (EBADF);
}

/* Lock dentry *DENTRYP on volume *VOLP with capability *ICAPP and virtual
   directory *VDP to level LEVEL.
   Store the local ZFS file handle to TMP_FH.  */

int32_t
internal_cap_lock (unsigned int level, internal_cap *icapp, volume *volp,
		   internal_dentry *dentryp, virtual_dir *vdp,
		   zfs_cap *tmp_cap)
{
  int32_t r;
  bool wait_for_locked;
  unsigned int id;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (volp == NULL)
    abort ();
  if (dentryp == NULL)
    abort ();
  if (vdp && *vdp)
    CHECK_MUTEX_LOCKED (&(*vdp)->mutex);
  CHECK_MUTEX_LOCKED (&(*volp)->mutex);
  CHECK_MUTEX_LOCKED (&(*dentryp)->fh->mutex);
  if (level > LEVEL_EXCLUSIVE)
    abort ();
#endif

  message (4, stderr, "FH %p LOCK %u, by %lu at %s:%d\n",
	   (void *) (*dentryp)->fh, level, (unsigned long) pthread_self (),
	   __FILE__, __LINE__);

  *tmp_cap = (*icapp)->local_cap;
  id = (*dentryp)->fh->id2assign++;
  wait_for_locked = ((*dentryp)->fh->level + level > LEVEL_EXCLUSIVE);
  if (wait_for_locked)
    {
      zfsd_mutex_unlock (&(*volp)->mutex);
      if (vdp && *vdp)
	zfsd_mutex_unlock (&(*vdp)->mutex);

      while (!(*dentryp)->deleted
	     && ((*dentryp)->fh->id2run != id
		 || (*dentryp)->fh->level + level > LEVEL_EXCLUSIVE))
	zfsd_cond_wait (&(*dentryp)->fh->cond, &(*dentryp)->fh->mutex);
      zfsd_mutex_unlock (&(*dentryp)->fh->mutex);

      r = find_capability_nolock (tmp_cap, icapp, volp, dentryp, vdp, true);
      if (r != ZFS_OK)
	RETURN_INT (r);
    }

  message (4, stderr, "FH %p LOCKED %u, by %lu at %s:%d\n",
	   (void *) (*dentryp)->fh, level, (unsigned long) pthread_self (),
	   __FILE__, __LINE__);

  (*dentryp)->fh->level = level;
  (*dentryp)->fh->users++;
  (*dentryp)->users++;
  (*volp)->n_locked_fhs++;
  set_owned (*dentryp, level);
  if (vdp && *vdp)
    {
      (*vdp)->busy = true;
      (*vdp)->users++;
    }

  (*dentryp)->fh->id2run++;
  if (level != LEVEL_EXCLUSIVE)
    zfsd_cond_broadcast (&(*dentryp)->fh->cond);

  if (!wait_for_locked)
    {
      release_dentry (*dentryp);
      zfsd_mutex_unlock (&(*volp)->mutex);
      if (vdp && *vdp)
	zfsd_mutex_unlock (&(*vdp)->mutex);

      r = find_capability_nolock (tmp_cap, icapp, volp, dentryp, vdp, false);
#ifdef ENABLE_CHECKING
      if (r != ZFS_OK)
	abort ();
#endif
    }

  RETURN_INT (ZFS_OK);
}

/* Unlock dentry DENTRY and virtual directory VD.  */

void
internal_cap_unlock (volume vol, internal_dentry dentry, virtual_dir vd)
{
  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (vd)
    CHECK_MUTEX_LOCKED (&vd->mutex);
#endif

  if (vd)
    {
      vd->users--;
      if (vd->users == 0)
	{
	  vd->busy = false;
	  if (vd->deleted > 0)
	    virtual_dir_destroy (vd);
	  else
	    zfsd_mutex_unlock (&vd->mutex);
	}
      else
	zfsd_mutex_unlock (&vd->mutex);
    }

  internal_dentry_unlock (vol, dentry);
}

/* Create a new capability for file handle fh with open flags FLAGS.  */

static internal_cap
internal_cap_create_fh (internal_fh fh, uint32_t flags)
{
  internal_cap cap;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh->mutex);
#ifdef ENABLE_CHECKING
  /* This should be handled in get_capability().  */
  if (fh->attr.type == FT_DIR && flags != O_RDONLY)
    abort ();
#endif

  zfsd_mutex_lock (&cap_mutex);
  cap = (internal_cap) pool_alloc (cap_pool);
  zfsd_mutex_unlock (&cap_mutex);

  cap->local_cap.fh = fh->local_fh;
  cap->local_cap.flags = flags;
  if (!internal_cap_compute_verify (cap))
    {
      zfsd_mutex_lock (&cap_mutex);
      pool_free (cap_pool, cap);
      zfsd_mutex_unlock (&cap_mutex);
      RETURN_PTR (NULL);
    }
  zfs_fh_undefine (cap->master_cap.fh);
  zfs_cap_undefine (cap->master_cap);
  cap->busy = 1;
  cap->master_busy = 0;
  cap->master_close_p = false;
  cap->next = fh->cap;
  fh->cap = cap;

  RETURN_PTR (cap);
}

/* Create a new capability for virtual directory VD with open flags FLAGS.  */

static internal_cap
internal_cap_create_vd (virtual_dir vd, uint32_t flags)
{
  internal_cap cap;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vd->mutex);
#ifdef ENABLE_CHECKING
  /* This should be handled in get_capability().  */
  if (flags != O_RDONLY)
    abort ();
#endif

  zfsd_mutex_lock (&cap_mutex);
  cap = (internal_cap) pool_alloc (cap_pool);
  zfsd_mutex_unlock (&cap_mutex);

  cap->local_cap.fh = vd->fh;
  cap->local_cap.flags = flags;
  if (!internal_cap_compute_verify (cap))
    {
      zfsd_mutex_lock (&cap_mutex);
      pool_free (cap_pool, cap);
      zfsd_mutex_unlock (&cap_mutex);
      RETURN_PTR (NULL);
    }
  zfs_fh_undefine (cap->master_cap.fh);
  zfs_cap_undefine (cap->master_cap);
  cap->busy = 1;
  cap->master_busy = 0;
  cap->master_close_p = false;
  cap->next = NULL;
  vd->cap = cap;

  RETURN_PTR (cap);
}

/* Destroy capability CAP associated with internal file handle FH or
   virtual directory VD.  */

static void
internal_cap_destroy (internal_cap cap, internal_fh fh, virtual_dir vd)
{
  TRACE ("");

  if (vd)
    {
      CHECK_MUTEX_LOCKED (&vd->mutex);
#ifdef ENABLE_CHECKING
      if (vd->cap != cap)
	abort ();
#endif

      vd->cap = NULL;
    }
  else
    {
      internal_cap icap, prev;

      CHECK_MUTEX_LOCKED (&fh->mutex);
#ifdef ENABLE_CHECKING
      if (fh->cap == NULL)
	abort ();
#endif

      if (cap == fh->cap)
	{
	  icap = cap;
	  fh->cap = cap->next;
	}
      else
	{
	  for (prev = fh->cap, icap = prev->next; icap; icap = icap->next)
	    {
	      if (icap == cap)
		{
		  prev->next = cap->next;
		  break;
		}
	      prev = icap;
	    }
	}

#ifdef ENABLE_CHECKING
      if (icap == NULL)
	abort ();
#endif

    }

  if (fh && !fh->cap)
    local_close (fh);

  zfsd_mutex_lock (&cap_mutex);
  pool_free (cap_pool, cap);
  zfsd_mutex_unlock (&cap_mutex);
}

/* Destroy all unused capabilities associated with file handle FH.  */

void
destroy_unused_capabilities (internal_fh fh)
{
  internal_cap cap, next;
  internal_cap *prevp;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh->mutex);

  prevp = &fh->cap;
  for (cap = fh->cap; cap; cap = next)
    {
      next = cap->next;

      if (cap->busy == 0)
	{
	  zfsd_mutex_lock (&cap_mutex);
	  pool_free (cap_pool, cap);
	  zfsd_mutex_unlock (&cap_mutex);

	  *prevp = next;
	}
      else
	prevp = &cap->next;
    }

  if (fh->cap == NULL)
    local_close (fh);
}

/* Get an internal capability CAP and store it to ICAPP. Store capability's
   volume to VOL, internal file handle IFH and virtual directory to VD.
   Create a new internal capability if it does not exist.  */

int32_t
get_capability (zfs_cap *cap, internal_cap *icapp, volume *vol,
		internal_dentry *dentry, virtual_dir *vd,
		bool unlock_fh_mutex, bool delete_volume_p)
{
  internal_cap icap;
  int32_t r;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (cap->flags & ~O_ACCMODE)
    abort ();
#endif

  if (NON_EXIST_FH_P (cap->fh))
    RETURN_INT (EINVAL);

  if ((VIRTUAL_FH_P (cap->fh) || CONFLICT_DIR_P (cap->fh))
      && cap->flags != O_RDONLY)
    RETURN_INT (EISDIR);

  r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd, delete_volume_p);
  if (r == ZFS_STALE)
    {
#ifdef ENABLE_CHECKING
      if (VIRTUAL_FH_P (cap->fh))
	abort ();
#endif
      r = refresh_fh (&cap->fh);
      if (r != ZFS_OK)
	RETURN_INT (r);
      r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd, delete_volume_p);
    }
  if (r != ZFS_OK)
    RETURN_INT (r);

  if (unlock_fh_mutex)
    zfsd_mutex_unlock (&fh_mutex);

  if (vd && *vd && *vol)
    {
      int32_t r2;

      if (!unlock_fh_mutex)
	zfsd_mutex_unlock (&fh_mutex);

      r2 = get_volume_root_dentry (*vol, dentry, unlock_fh_mutex);
      if (r2 != ZFS_OK)
	{
	  zfsd_mutex_unlock (&(*vd)->mutex);
	  r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd, delete_volume_p);
	  if (r != ZFS_OK)
	    RETURN_INT (r);

	  if (unlock_fh_mutex)
	    zfsd_mutex_unlock (&fh_mutex);
	}
    }

  if (*dentry && (*dentry)->fh->attr.type == FT_DIR && cap->flags != O_RDONLY)
    {
      release_dentry (*dentry);
      if (*vd)
	zfsd_mutex_unlock (&(*vd)->mutex);
      zfsd_mutex_unlock (&(*vol)->mutex);
      RETURN_INT (EISDIR);
    }

  if (vd && *vd)
    {
      if ((*vd)->cap)
	{
	  icap = (*vd)->cap;
	  icap->busy++;
	}
      else
	icap = internal_cap_create_vd (*vd, cap->flags);
    }
  else
    {
      for (icap = (*dentry)->fh->cap; icap; icap = icap->next)
	if (icap->local_cap.flags == cap->flags)
	  break;

      if (icap)
	icap->busy++;
      else
	icap = internal_cap_create_fh ((*dentry)->fh, cap->flags);
    }

  *icapp = icap;
  memcpy (cap->verify, icap->local_cap.verify, ZFS_VERIFY_LEN);
  RETURN_INT (ZFS_OK);
}

/* Return an internal capability for ZFS capability CAP and internal dentry
   DENTRY.  */

internal_cap
get_capability_no_zfs_fh_lookup (zfs_cap *cap, internal_dentry dentry,
				 uint32_t flags)
{
  internal_cap icap;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  for (icap = dentry->fh->cap; icap; icap = icap->next)
    if (icap->local_cap.flags == flags)
      break;

  if (icap)
    icap->busy++;
  else
    icap = internal_cap_create_fh (dentry->fh, flags);

  /* Set elements of CAP except VERIFY, VERIFY will be set in zfs_create.  */
  cap->fh = icap->local_cap.fh;
  cap->flags = icap->local_cap.flags;
  RETURN_PTR (icap);
}

/* Find an internal capability CAP and store it to ICAPP. Store capability's
   volume to VOL, internal dentry DENTRY and virtual directory to VD.
   If DELETE_VOLUME_P is true and the volume should be deleted do not
   lookup the file handle and delete the volume if there are no file handles
   locked on it.  */

int32_t
find_capability (zfs_cap *cap, internal_cap *icapp, volume *vol,
		 internal_dentry *dentry, virtual_dir *vd,
		 bool delete_volume_p)
{
  int32_t r;

  TRACE ("");

  r = find_capability_nolock (cap, icapp, vol, dentry, vd, delete_volume_p);
  if (r == ZFS_OK)
    zfsd_mutex_unlock (&fh_mutex);

  RETURN_INT (r);
}

/* Find an internal capability CAP and store it to ICAPP. Store capability's
   volume to VOL, internal dentry DENTRY and virtual directory to VD.
   If DELETE_VOLUME_P is true and the volume should be deleted do not
   lookup the file handle and delete the volume if there are no file handles
   locked on it.
   This function is similar to FIND_CAPABILITY but does not lock big locks.  */

int32_t
find_capability_nolock (zfs_cap *cap, internal_cap *icapp,
			volume *vol, internal_dentry *dentry, virtual_dir *vd,
			bool delete_volume_p)
{
  internal_cap icap;
  int32_t r;

  TRACE ("");

  r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd, delete_volume_p);
  if (r == ZFS_STALE)
    {
#ifdef ENABLE_CHECKING
      if (VIRTUAL_FH_P (cap->fh))
	abort ();
#endif
      r = refresh_fh (&cap->fh);
      if (r != ZFS_OK)
	RETURN_INT (r);
      r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd, delete_volume_p);
    }
  if (r != ZFS_OK)
    RETURN_INT (r);

  if (vd && *vd && *vol)
    {
      int32_t r2;

      zfsd_mutex_unlock (&fh_mutex);
      r2 = get_volume_root_dentry (*vol, dentry, false);
      if (r2 != ZFS_OK)
	{
	  zfsd_mutex_unlock (&(*vd)->mutex);
	  r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd, delete_volume_p);
	  if (r != ZFS_OK)
	    RETURN_INT (r);
	}
    }

  if (vd && *vd)
    {
      icap = (*vd)->cap;
    }
  else
    {
      for (icap = (*dentry)->fh->cap; icap; icap = icap->next)
	if (icap->local_cap.flags == cap->flags)
	  break;
    }

  if (!icap)
    {
      r = EBADF;
      goto out;
    }

  r = verify_capability (cap, icap);
  if (r != ZFS_OK)
    goto out;

  *icapp = icap;
  RETURN_INT (ZFS_OK);

out:
  if (*dentry)
    release_dentry (*dentry);
  if (vd && *vd)
    zfsd_mutex_unlock (&(*vd)->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  if (*vol)
    zfsd_mutex_unlock (&(*vol)->mutex);

  RETURN_INT (r);
}

/* Decrease the number of users of capability CAP associated with
   file handle FH or virtual directory VD
   and destroy the capability when the number of users becomes 0.  */

int32_t
put_capability (internal_cap cap, internal_fh fh, virtual_dir vd)
{
  TRACE ("");
#ifdef ENABLE_CHECKING
  if (fh)
    CHECK_MUTEX_LOCKED (&fh->mutex);
  if (vd)
    CHECK_MUTEX_LOCKED (&vd->mutex);
#endif

  cap->busy--;
  if (cap->busy == 0)
    {
      if (!fh || fh->users == 0)
	internal_cap_destroy (cap, fh, vd);
    }

  RETURN_INT (ZFS_OK);
}

/* Initialize data structures in CAP.C.  */

void
initialize_cap_c (void)
{
  zfsd_mutex_init (&cap_mutex);
  cap_pool = create_alloc_pool ("cap_pool", sizeof (struct internal_cap_def),
				250, &cap_mutex);
}

/* Destroy data structures in CAP.C.  */

void
cleanup_cap_c (void)
{
  zfsd_mutex_lock (&cap_mutex);
#ifdef ENABLE_CHECKING
  if (cap_pool->elts_free < cap_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in cap_pool.\n",
	     cap_pool->elts_allocated - cap_pool->elts_free);
#endif
  free_alloc_pool (cap_pool);
  zfsd_mutex_unlock (&cap_mutex);
  zfsd_mutex_destroy (&cap_mutex);
}
