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

#include "system.h"
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
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

/* Hash table of capabilities.  */
static htab_t cap_htab;

/* Mutex for cap_pool and cap_htab.  */
pthread_mutex_t cap_mutex;

#define ZFS_CAP_HASH(CAP)						\
  (crc32_update (crc32_buffer (&(CAP).fh, sizeof (zfs_fh)),		\
		 &(CAP).flags, sizeof ((CAP).flags)))

#define INTERNAL_CAP_HASH(CAP) ZFS_CAP_HASH ((CAP)->local_cap)

/* Hash function for internal capability X.  */

static hash_t
internal_cap_hash (const void *x)
{
  return INTERNAL_CAP_HASH ((internal_cap) x);
}

/* Compare an internal capability XX with client's capability YY.  */

static int
internal_cap_eq (const void *xx, const void *yy)
{
  zfs_cap *x = &((internal_cap) xx)->local_cap;
  zfs_cap *y = (zfs_cap *) yy;

  return (ZFS_FH_EQ (x->fh, y->fh) && x->flags == x->flags);
}

/* Find capability for internal file handle FH and open flags FLAGS.  */

internal_cap
internal_cap_lookup (zfs_cap *cap)
{
  internal_cap icap;

  CHECK_MUTEX_LOCKED (&cap_mutex);

  icap = (internal_cap) htab_find_with_hash (cap_htab, cap,
					    ZFS_CAP_HASH (*cap));
  if (icap)
    zfsd_mutex_lock (&icap->mutex);

  return icap;
}

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
  if (verbose >= 3)
    {
      fprintf (stderr, "Using verify ");
      print_hex_buffer (cap->verify, ZFS_VERIFY_LEN, stderr);
      fprintf (stderr, "It should be ");
      print_hex_buffer (icap->local_cap.verify, ZFS_VERIFY_LEN, stderr);
    }

  return (memcmp (cap->verify, icap->local_cap.verify, ZFS_VERIFY_LEN) == 0
	  ? ZFS_OK
	  : EBADF);
}

/* Create a new capability for internal dentry DENTRY with open flags FLAGS.  */

static internal_cap
internal_cap_create_dentry (internal_dentry dentry, uint32_t flags)
{
  internal_cap cap;
  void **slot;

  CHECK_MUTEX_LOCKED (&cap_mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  /* This should be handled in get_capability().  */
  if (dentry->fh->attr.type == FT_DIR && flags != O_RDONLY)
    abort ();
#endif

  cap = (internal_cap) pool_alloc (cap_pool);

  if (!full_read (fd_urandom, cap->random, CAP_RANDOM_LEN))
    {
      pool_free (cap_pool, cap);
      return NULL;
    }
  cap->local_cap.fh = dentry->fh->local_fh;
  cap->master_cap.fh = dentry->fh->master_fh;
  cap->local_cap.flags = flags;
  cap->master_cap.flags = flags;
  cap->busy = 1;
  cap->fd = -1;
  cap->generation = 0;
  dentry->ncap++;
  internal_cap_compute_verify (cap);
  zfsd_mutex_init (&cap->mutex);
  zfsd_mutex_lock (&cap->mutex);

  slot = htab_find_slot_with_hash (cap_htab, &cap->local_cap,
				   INTERNAL_CAP_HASH (cap), INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = cap;

  return cap;
}

/* Create a new capability for virtual directory VD with open flags FLAGS.  */

static internal_cap
internal_cap_create_vd (virtual_dir vd, uint32_t flags)
{
  internal_cap cap;
  void **slot;

  CHECK_MUTEX_LOCKED (&cap_mutex);
#ifdef ENABLE_CHECKING
  /* This should be handled in get_capability().  */
  if (flags != O_RDONLY)
    abort ();
#endif

  cap = (internal_cap) pool_alloc (cap_pool);

  if (!full_read (fd_urandom, cap->random, CAP_RANDOM_LEN))
    {
      pool_free (cap_pool, cap);
      return NULL;
    }
  cap->local_cap.fh = vd->fh;
  cap->master_cap.fh = vd->fh;
  cap->local_cap.flags = flags;
  cap->master_cap.flags = flags;
  cap->busy = 1;
  cap->fd = -1;
  cap->generation = 0;
  internal_cap_compute_verify (cap);
  zfsd_mutex_init (&cap->mutex);
  zfsd_mutex_lock (&cap->mutex);

  slot = htab_find_slot_with_hash (cap_htab, &cap->local_cap,
				   INTERNAL_CAP_HASH (cap), INSERT);
#ifdef ENABLE_CHECKING
  if (*slot)
    abort ();
#endif
  *slot = cap;

  return cap;
}

/* Destroy capability CAP associated with internal dentry DENTRY.  */

static void
internal_cap_destroy (internal_cap cap, internal_dentry dentry)
{
  void **slot;

  CHECK_MUTEX_LOCKED (&cap_mutex);
  CHECK_MUTEX_LOCKED (&cap->mutex);

  if (dentry)
    {
      CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

      dentry->ncap--;
    }

  if (cap->fd >= 0)
    local_close (cap);

  slot = htab_find_slot_with_hash (cap_htab, &cap->local_cap,
				   INTERNAL_CAP_HASH (cap), NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif

  htab_clear_slot (cap_htab, slot);
  zfsd_mutex_unlock (&cap->mutex);
  zfsd_mutex_destroy (&cap->mutex);
  pool_free (cap_pool, cap);
}

/* Get an internal capability CAP and store it to ICAPP. Store capability's
   volume to VOL, internal file handle IFH and virtual directory to VD.
   Create a new internal capability if it does not exist.  */

int32_t
get_capability (zfs_cap *cap, internal_cap *icapp,
		volume *vol, internal_dentry *dentry, virtual_dir *vd)
{
  internal_cap icap;
  int32_t r;

#ifdef ENABLE_CHECKING
  if (cap->flags & ~O_ACCMODE)
    abort ();
#endif

  if (VIRTUAL_FH_P (cap->fh) && cap->flags != O_RDONLY)
    return EISDIR;

  r = zfs_fh_lookup (&cap->fh, vol, dentry, vd);
  if (r != ZFS_OK)
    return r;

  if (*vd && *vol)
    get_volume_root_dentry (*vol, dentry);

  if (*dentry && (*dentry)->fh->attr.type == FT_DIR && cap->flags != O_RDONLY)
    {
      zfsd_mutex_unlock (&(*dentry)->fh->mutex);
      if (*vd)
	zfsd_mutex_unlock (&(*vd)->mutex);
      if (*vol)
	zfsd_mutex_unlock (&(*vol)->mutex);
      return EISDIR;
    }

  zfsd_mutex_lock (&cap_mutex);
  icap = internal_cap_lookup (cap);
  if (icap)
    icap->busy++;
  else if (*vd)
    icap = internal_cap_create_vd (*vd, cap->flags);
  else
    icap = internal_cap_create_dentry (*dentry, cap->flags);
  zfsd_mutex_unlock (&cap_mutex);

  *icapp = icap;
  return ZFS_OK;
}

/* Return an internal capability for ZFS capability CAP and internal dentry
   DENTRY.  */

internal_cap
get_capability_no_zfs_fh_lookup (zfs_cap *cap, internal_dentry dentry)
{
  internal_cap icap;

  zfsd_mutex_lock (&cap_mutex);
  icap = internal_cap_lookup (cap);
  if (icap)
    icap->busy++;
  else
    icap = internal_cap_create_dentry (dentry, cap->flags);
  zfsd_mutex_unlock (&cap_mutex);

  return icap;
}

/* Find an internal capability CAP and store it to ICAPP. Store capability's
   volume to VOL, internal dentry DENTRY and virtual directory to VD.
   Create a new internal capability if it does not exist.  */

int32_t
find_capability (zfs_cap *cap, internal_cap *icapp,
		 volume *vol, internal_dentry *dentry, virtual_dir *vd)
{
  int32_t r;

  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (cap->fh))
    zfsd_mutex_lock (&vd_mutex);
  zfsd_mutex_lock (&cap_mutex);

  r = find_capability_nolock (cap, icapp, vol, dentry, vd);

  zfsd_mutex_unlock (&volume_mutex);
  if (VIRTUAL_FH_P (cap->fh))
    zfsd_mutex_unlock (&vd_mutex);
  zfsd_mutex_unlock (&cap_mutex);

  return r;
}

/* Find an internal capability CAP and store it to ICAPP. Store capability's
   volume to VOL, internal dentry DENTRY and virtual directory to VD.
   Create a new internal capability if it does not exist.
   This function is similar to FIND_CAPABILITY but does not lock big locks.  */

int32_t
find_capability_nolock (zfs_cap *cap, internal_cap *icapp,
			volume *vol, internal_dentry *dentry, virtual_dir *vd)
{
  internal_cap icap;
  int32_t r;

  CHECK_MUTEX_LOCKED (&volume_mutex);
  if (VIRTUAL_FH_P (cap->fh))
    CHECK_MUTEX_LOCKED (&vd_mutex);
  CHECK_MUTEX_LOCKED (&cap_mutex);

  icap = internal_cap_lookup (cap);
  if (!icap)
    return EBADF;

  r = verify_capability (cap, icap);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&icap->mutex);
      return r;
    }

  r = zfs_fh_lookup_nolock (&cap->fh, vol, dentry, vd);
  if (r != ZFS_OK)
    {
      internal_cap_destroy (icap, NULL);
      return r;
    }

  if (vd && *vd && *vol)
    get_volume_root_dentry (*vol, dentry);

  *icapp = icap;
  return ZFS_OK;
}

/* Decrease the number of users of capability CAP and destroy the capability
   when the number of users becomes 0.  */

int32_t
put_capability (internal_cap cap, internal_dentry dentry)
{
  CHECK_MUTEX_LOCKED (&cap_mutex);
  CHECK_MUTEX_LOCKED (&cap->mutex);

  cap->busy--;
  if (cap->busy == 0)
    internal_cap_destroy (cap, dentry);
  else
    zfsd_mutex_unlock (&cap->mutex);

  return ZFS_OK;
}

/* Initialize data structures in CAP.C.  */

void
initialize_cap_c ()
{
  zfsd_mutex_init (&cap_mutex);
  cap_pool = create_alloc_pool ("cap_pool", sizeof (struct internal_cap_def),
				250, &cap_mutex);
  cap_htab = htab_create (200, internal_cap_hash, internal_cap_eq, NULL,
			  &cap_mutex);
}

/* Destroy data structures in CAP.C.  */

void
cleanup_cap_c ()
{
  void **slot;

  zfsd_mutex_lock (&cap_mutex);
  HTAB_FOR_EACH_SLOT (cap_htab, slot,
    {
      internal_cap cap = (internal_cap) *slot;

      zfsd_mutex_lock (&cap->mutex);
      internal_cap_destroy (cap, NULL);
    });
  htab_destroy (cap_htab);

#ifdef ENABLE_CHECKING
  if (cap_pool->elts_free < cap_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in cap_pool.\n",
	     cap_pool->elts_allocated - cap_pool->elts_free);
#endif
  free_alloc_pool (cap_pool);
  zfsd_mutex_unlock (&cap_mutex);
  zfsd_mutex_destroy (&cap_mutex);
}
