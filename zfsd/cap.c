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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "pthread.h"
#include "cap.h"
#include "crc32.h"
#include "alloc-pool.h"
#include "hashtab.h"
#include "fh.h"
#include "log.h"
#include "random.h"
#include "util.h"
#include "memory.h"
#include "zfs_prot.h"

/* The array of data for each file descriptor.  */
internal_fd_data_t *internal_fd_data;

/* Allocation pool for capabilities.  */
static alloc_pool cap_pool;

/* Hash table of capabilities.  */
static htab_t cap_htab;

/* Mutex for cap_pool and cap_htab.  */
pthread_mutex_t cap_mutex;

#define ZFS_CAP_HASH(CAP)						\
  (crc32_update (crc32_buffer (&(CAP).fh, sizeof (zfs_fh)),		\
		 &(CAP).mode, sizeof ((CAP).mode)))

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

  return (ZFS_FH_EQ (x->fh, y->fh) && x->mode == x->mode);
}

/* Find capability for internal file handle FH and open mode MODE.  */

internal_cap
internal_cap_lookup (internal_fh fh, unsigned int mode)
{
  zfs_cap tmp_cap;
  internal_cap cap;

  CHECK_MUTEX_LOCKED (&cap_mutex);

  tmp_cap.fh = fh->local_fh;
  tmp_cap.mode = mode;
  cap = (internal_cap) htab_find_with_hash (cap_htab, &tmp_cap,
					    ZFS_CAP_HASH (tmp_cap));
  if (cap)
    zfsd_mutex_lock (&cap->mutex);

  return cap;
}

/* Create a new capability for internal file handle FH with open mode MODE.  */

static internal_cap
internal_cap_create (internal_fh fh, unsigned int mode)
{
  internal_cap cap;
  void **slot;

  CHECK_MUTEX_LOCKED (&cap_mutex);
#ifdef ENABLE_CHECKING
  /* This should be handled in ZFS "open".  */
  if (fh->attr.type == FT_DIR && mode != O_RDONLY)
    abort ();
#endif

  cap = (internal_cap) pool_alloc (cap_pool);

  if (!full_read (fd_urandom, cap->random, CAP_RANDOM_LEN))
    {
      pool_free (cap_pool, cap);
      return NULL;
    }
  cap->local_cap.fh = fh->local_fh;
  cap->master_cap.fh = fh->master_fh;
  cap->local_cap.mode = mode;
  cap->master_cap.mode = mode;
  cap->busy = 1;
  zfsd_mutex_init (&cap->mutex);
  zfsd_mutex_lock (&cap->mutex);

#ifdef ENABLE_CHECKING
  slot = htab_find_slot_with_hash (cap_htab, &cap->local_cap,
				   INTERNAL_CAP_HASH (cap), NO_INSERT);
  if (slot)
    abort ();
#endif
  slot = htab_find_slot_with_hash (cap_htab, &cap->local_cap,
				   INTERNAL_CAP_HASH (cap), INSERT);
  *slot = cap;

  return cap;
}

/* Destroy capability CAP.  */

static void
internal_cap_destroy (internal_cap cap)
{
  void **slot;

  CHECK_MUTEX_LOCKED (&cap_mutex);
  CHECK_MUTEX_LOCKED (&cap->mutex);

  slot = htab_find_slot_with_hash (cap_htab, cap, INTERNAL_CAP_HASH (cap),
				   NO_INSERT);
#ifdef ENABLE_CHECKING
  if (!slot)
    abort ();
#endif

  htab_clear_slot (cap_htab, slot);
  zfsd_mutex_unlock (&cap->mutex);
  zfsd_mutex_destroy (&cap->mutex);
  pool_free (cap_pool, *slot);
}

/* Get a capability for internal file handle FH with open mode MODE.  Create
   a new one if it does not exist.  */

internal_cap
get_capability (internal_fh fh, unsigned int mode)
{
  internal_cap cap;

#ifdef ENABLE_CHECKING
  if (mode & ~O_ACCMODE)
    abort ();
#endif

  zfsd_mutex_lock (&cap_mutex);
  cap = internal_cap_lookup (fh, mode);
  if (cap)
    cap->busy++;
  else
    cap = internal_cap_create (fh, mode);
  zfsd_mutex_unlock (&cap_mutex);

  return cap;
}

/* Decrease the number of users of capability CAP and destroy the capability
   when the number of users becomes 0.  */

int
put_capability (zfs_cap *zcap)
{
  internal_cap cap;

  zfsd_mutex_lock (&cap_mutex);
  cap = (internal_cap) htab_find_with_hash (cap_htab, zcap,
					    ZFS_CAP_HASH (*zcap));
  if (!cap)
    {
      zfsd_mutex_unlock (&cap_mutex);
      return EBADF;
    }
  zfsd_mutex_lock (&cap->mutex);
  cap->busy--;
  if (cap->busy == 0)
    internal_cap_destroy (cap);
  else
    zfsd_mutex_unlock (&cap->mutex);
  zfsd_mutex_unlock (&cap_mutex);

  return ZFS_OK;
}

/* Initialize data structures in CAP.C.  */

void
initialize_cap_c ()
{
  int i;

  /* Data for each file descriptor.  */
  internal_fd_data
    = (internal_fd_data_t *) xcalloc (max_nfd, sizeof (internal_fd_data_t));
  for (i = 0; i < max_nfd; i++)
    {
      zfsd_mutex_init (&internal_fd_data[i].mutex);
      internal_fd_data[i].fd = -1;
    }

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
      internal_cap_destroy (cap);
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

  free (internal_fd_data);
}
