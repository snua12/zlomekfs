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

/* Compute VERIFY for capability CAP.  */

static void
internal_cap_compute_verify (internal_cap cap)
{
  MD5Context ctx;

  MD5Init (&ctx);
  MD5Update (&ctx, (unsigned char *) &cap->local_cap.fh,
	     sizeof (cap->local_cap.fh));
  MD5Update (&ctx, (unsigned char *) &cap->local_cap.flags,
	     sizeof (cap->local_cap.flags));
  MD5Update (&ctx, (unsigned char *) cap->random, sizeof (cap->random));
  MD5Final ((unsigned char *) cap->local_cap.verify, &ctx);

  if (verbose >= 3)
    {
      fprintf (stderr, "Created verify ");
      print_hex_buffer (cap->local_cap.verify, ZFS_VERIFY_LEN, stderr);
    }
}

/* Verify capability CAP by comparing with ICAP.  */

static int32_t
verify_capability (zfs_cap *cap, internal_cap icap)
{
  if (memcmp (cap->verify, icap->local_cap.verify, ZFS_VERIFY_LEN) == 0)
    return ZFS_OK;

  if (verbose >= 3)
    {
      fprintf (stderr, "Using verify ");
      print_hex_buffer (cap->verify, ZFS_VERIFY_LEN, stderr);
      fprintf (stderr, "It should be ");
      print_hex_buffer (icap->local_cap.verify, ZFS_VERIFY_LEN, stderr);
    }

  return EBADF;
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
  wait_for_locked = ((*dentryp)->fh->level + level > LEVEL_EXCLUSIVE);
  if (wait_for_locked)
    {
      /* Mark the dentry so that nobody else can lock dentry before us.  */
      if (level > (*dentryp)->fh->level)
	(*dentryp)->fh->level = level;

      zfsd_mutex_unlock (&(*volp)->mutex);
      if (vdp && *vdp)
	zfsd_mutex_unlock (&(*vdp)->mutex);

      while ((*dentryp)->fh->level + level > LEVEL_EXCLUSIVE)
	zfsd_cond_wait (&(*dentryp)->fh->cond, &(*dentryp)->fh->mutex);
      zfsd_mutex_unlock (&(*dentryp)->fh->mutex);

      if (VIRTUAL_FH_P (tmp_cap->fh))
	zfsd_mutex_lock (&vd_mutex);
      r = find_capability_nolock (tmp_cap, icapp, volp, dentryp, vdp, true);
      if (VIRTUAL_FH_P (tmp_cap->fh) && vdp && !*vdp)
	zfsd_mutex_unlock (&vd_mutex);
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
  if (vdp && *vdp)
    {
      (*vdp)->busy = true;
      (*vdp)->users++;
    }

  if (!wait_for_locked)
    {
      release_dentry (*dentryp);
      zfsd_mutex_unlock (&(*volp)->mutex);
      if (vdp && *vdp)
	zfsd_mutex_unlock (&(*vdp)->mutex);

      if (VIRTUAL_FH_P (tmp_cap->fh))
	zfsd_mutex_lock (&vd_mutex);
      r = find_capability_nolock (tmp_cap, icapp, volp, dentryp, vdp, false);
      /* Following condition should not be needed.  */
      if (VIRTUAL_FH_P (tmp_cap->fh) && vdp && !*vdp)
	zfsd_mutex_unlock (&vd_mutex);
#ifdef ENABLE_CHECKING
      if (r != ZFS_OK)
	abort ();
#endif
    }

  return ZFS_OK;
}

/* Unlock dentry DENTRY and virtual directory VD.  */

void
internal_cap_unlock (volume vol, internal_dentry dentry, virtual_dir vd)
{
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
  if (vd)
    {
      CHECK_MUTEX_LOCKED (&vd_mutex);
      CHECK_MUTEX_LOCKED (&vd->mutex);
    }

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
      zfsd_mutex_unlock (&vd_mutex);
    }

  internal_dentry_unlock (vol, dentry);
}

/* Create a new capability for file handle fh with open flags FLAGS.  */

static internal_cap
internal_cap_create_fh (internal_fh fh, uint32_t flags)
{
  internal_cap cap;

  CHECK_MUTEX_LOCKED (&fh->mutex);
#ifdef ENABLE_CHECKING
  /* This should be handled in get_capability().  */
  if (fh->attr.type == FT_DIR && flags != O_RDONLY)
    abort ();
#endif

  zfsd_mutex_lock (&cap_mutex);
  cap = (internal_cap) pool_alloc (cap_pool);
  zfsd_mutex_unlock (&cap_mutex);

  if (!full_read (fd_urandom, cap->random, CAP_RANDOM_LEN))
    {
      zfsd_mutex_lock (&cap_mutex);
      pool_free (cap_pool, cap);
      zfsd_mutex_unlock (&cap_mutex);
      return NULL;
    }
  cap->local_cap.fh = fh->local_fh;
  cap->local_cap.flags = flags;
  zfs_fh_undefine (cap->master_cap.fh);
  zfs_cap_undefine (cap->master_cap);
  cap->busy = 1;
  internal_cap_compute_verify (cap);
  cap->next = fh->cap;
  fh->cap = cap;

  return cap;
}

/* Create a new capability for virtual directory VD with open flags FLAGS.  */

static internal_cap
internal_cap_create_vd (virtual_dir vd, uint32_t flags)
{
  internal_cap cap;

  CHECK_MUTEX_LOCKED (&vd->mutex);
#ifdef ENABLE_CHECKING
  /* This should be handled in get_capability().  */
  if (flags != O_RDONLY)
    abort ();
#endif

  zfsd_mutex_lock (&cap_mutex);
  cap = (internal_cap) pool_alloc (cap_pool);
  zfsd_mutex_unlock (&cap_mutex);

  if (!full_read (fd_urandom, cap->random, CAP_RANDOM_LEN))
    {
      zfsd_mutex_lock (&cap_mutex);
      pool_free (cap_pool, cap);
      zfsd_mutex_unlock (&cap_mutex);
      return NULL;
    }
  cap->local_cap.fh = vd->fh;
  cap->local_cap.flags = flags;
  zfs_fh_undefine (cap->master_cap.fh);
  zfs_cap_undefine (cap->master_cap);
  cap->busy = 1;
  internal_cap_compute_verify (cap);
  cap->next = NULL;
  vd->cap = cap;

  return cap;
}

/* Destroy capability CAP associated with internal file handle FH or
   virtual directory VD.  */

static void
internal_cap_destroy (internal_cap cap, internal_fh fh, virtual_dir vd)
{
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

#ifdef ENABLE_CHECKING
  if (cap->flags & ~O_ACCMODE)
    abort ();
#endif

  if (NON_EXIST_FH_P (cap->fh))
    return EINVAL;

  if ((VIRTUAL_FH_P (cap->fh) || CONFLICT_DIR_P (cap->fh))
      && cap->flags != O_RDONLY)
    return EISDIR;

  if (VIRTUAL_FH_P (cap->fh))
    zfsd_mutex_lock (&vd_mutex);

  r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd, delete_volume_p);
  if (r == ZFS_STALE)
    {
#ifdef ENABLE_CHECKING
      if (VIRTUAL_FH_P (cap->fh))
	abort ();
#endif
      r = refresh_fh (&cap->fh);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd, delete_volume_p);
    }
  if (r != ZFS_OK)
    {
      if (VIRTUAL_FH_P (cap->fh))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }

  if (vd && *vd)
    zfsd_mutex_unlock (&vd_mutex);
  else if (unlock_fh_mutex)
    zfsd_mutex_unlock (&fh_mutex);

  if (vd && *vd && *vol)
    {
      int32_t r2;

      if (!unlock_fh_mutex)
	zfsd_mutex_unlock (&fh_mutex);
      r2 = get_volume_root_dentry (*vol, dentry, unlock_fh_mutex);
      if (r2 != ZFS_OK)
	{
	  /* *VOL is the volume under *VD so we may lock it.  */
	  zfsd_mutex_lock (&volume_mutex);
	  zfsd_mutex_lock (&(*vol)->mutex);
	  zfsd_mutex_unlock (&volume_mutex);
	}
    }

  if (*dentry && (*dentry)->fh->attr.type == FT_DIR && cap->flags != O_RDONLY)
    {
      release_dentry (*dentry);
      if (*vd)
	zfsd_mutex_unlock (&(*vd)->mutex);
      zfsd_mutex_unlock (&(*vol)->mutex);
      return EISDIR;
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
  return ZFS_OK;
}

/* Return an internal capability for ZFS capability CAP and internal dentry
   DENTRY.  */

internal_cap
get_capability_no_zfs_fh_lookup (zfs_cap *cap, internal_dentry dentry,
				 uint32_t flags)
{
  internal_cap icap;

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
  return icap;
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

  if (VIRTUAL_FH_P (cap->fh))
    zfsd_mutex_lock (&vd_mutex);

  r = find_capability_nolock (cap, icapp, vol, dentry, vd, delete_volume_p);

  if (VIRTUAL_FH_P (cap->fh))
    zfsd_mutex_unlock (&vd_mutex);
  if (r == ZFS_OK && dentry && *dentry)
    zfsd_mutex_unlock (&fh_mutex);

  return r;
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

#ifdef ENABLE_CHECKING
  if (VIRTUAL_FH_P (cap->fh))
    CHECK_MUTEX_LOCKED (&vd_mutex);
#endif

  r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd, delete_volume_p);
  if (r == ZFS_STALE)
    {
#ifdef ENABLE_CHECKING
      if (VIRTUAL_FH_P (cap->fh))
	abort ();
#endif
      r = refresh_fh (&cap->fh);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd, delete_volume_p);
    }
  if (r != ZFS_OK)
    return r;

  if (vd && *vd && *vol)
    {
      int32_t r2;

      r2 = get_volume_root_dentry (*vol, dentry, false);
      if (r2 != ZFS_OK)
	{
	  /* *VOL is the volume under *VD so we may lock it.  */
	  zfsd_mutex_lock (&volume_mutex);
	  zfsd_mutex_lock (&(*vol)->mutex);
	  zfsd_mutex_unlock (&volume_mutex);
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
  return ZFS_OK;

out:
  if (*dentry)
    {
      release_dentry (*dentry);
      zfsd_mutex_unlock (&fh_mutex);
    }
  if (vd && *vd)
    zfsd_mutex_unlock (&(*vd)->mutex);
  if (*vol)
    zfsd_mutex_unlock (&(*vol)->mutex);

  return r;
}

/* Decrease the number of users of capability CAP associated with
   file handle FH or virtual directory VD
   and destroy the capability when the number of users becomes 0.  */

int32_t
put_capability (internal_cap cap, internal_fh fh, virtual_dir vd)
{
#ifdef ENABLE_CHECKING
  if (fh)
    CHECK_MUTEX_LOCKED (&fh->mutex);
  if (vd)
    CHECK_MUTEX_LOCKED (&vd->mutex);
#endif

  cap->busy--;
  if (cap->busy == 0)
    internal_cap_destroy (cap, fh, vd);

  return ZFS_OK;
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
