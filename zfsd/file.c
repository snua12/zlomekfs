/* File operations.
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
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "pthread.h"
#include "constant.h"
#include "memory.h"
#include "fh.h"
#include "file.h"
#include "dir.h"
#include "cap.h"
#include "volume.h"

/* The array of data for each file descriptor.  */
internal_fd_data_t *internal_fd_data;

/* If local file for capability CAP is opened return true and lock
   INTERNAL_FD_DATA[CAP->FD].MUTEX.  */

bool
capability_opened_p (internal_cap cap)
{
  CHECK_MUTEX_LOCKED (&cap->mutex);

  if (cap->fd < 0)
    return false;

  zfsd_mutex_lock (&internal_fd_data[cap->fd].mutex);
  if (cap->generation != internal_fd_data[cap->fd].generation)
    {
      zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
      return false;
    }

  internal_fd_data[cap->fd].last_use = time (NULL);
  return true;
}

/* Close local file for internal capability CAP on volume VOL.  */

static int
close_local_capability (internal_cap cap)
{
  if (cap->fd >= 0)
    {
      zfsd_mutex_lock (&internal_fd_data[cap->fd].mutex);
      internal_fd_data[cap->fd].fd = -1;
      internal_fd_data[cap->fd].generation++;
      close (cap->fd);
      zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
      cap->fd = -1;
    }

  return ZFS_OK;
}

/* Close local file for internal capability CAP on volume VOL.  */

static int
close_remote_capability (internal_cap cap, volume vol)
{
  thread *t;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&cap->mutex);

  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_close_client (t, &cap->master_cap, vol->master);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc))
	return ZFS_INVALID_REPLY;
    }

  return r;
}

/* Close internal capability CAP on volume VOL.  */

static int
close_capability (internal_cap cap, volume vol)
{
  int r;

  if (vol->local_path)
    r = close_local_capability (cap);
  else if (vol->master != this_node)
    r = close_remote_capability (cap, vol);
  else
    abort ();

  return ZFS_OK;
}

/* Open local file for capability CAP (whose internal file handle is FH)
   with additional FLAGS on volume VOL.  */

static int
capability_open (internal_cap cap, unsigned int flags, internal_fh fh,
		 volume vol)
{
  char *path;

  CHECK_MUTEX_LOCKED (&cap->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (flags & O_CREAT)
    abort ();
#endif

  /* Close the old file descriptor, new one will be opened.  */
  close_local_capability (cap);

  path = build_local_path (vol, fh);
  cap->fd = open (path, cap->local_cap.flags | flags);
  free (path);
  if (cap->fd >= 0)
    {
      zfsd_mutex_lock (&internal_fd_data[cap->fd].mutex);
      internal_fd_data[cap->fd].fd = cap->fd;
      internal_fd_data[cap->fd].generation++;
      internal_fd_data[cap->fd].last_use = time (NULL);
      cap->generation = internal_fd_data[cap->fd].generation;
      return ZFS_OK;
    }

  return errno;
}

/* Open local file for capability ICAP (whose internal file handle is FH)
   with open flags FLAGS on volume VOL.  Store ZFS capability to CAP.  */

static int
open_local_capability (zfs_cap *cap, internal_cap icap, unsigned int flags,
		       internal_fh fh, volume vol)
{
  int r;

  r = capability_open (icap, flags, fh, vol);
  if (r != ZFS_OK)
    return r;

  zfsd_mutex_unlock (&internal_fd_data[icap->fd].mutex);
  *cap = icap->local_cap;
  return ZFS_OK;
}

/* Open remote file for capability ICAP with open flags FLAGS on volume VOL.
   Store ZFS capability to CAP.  */

static int
open_remote_capability (zfs_cap *cap, internal_cap icap, unsigned int flags,
			volume vol)
{
  open_fh_args args;
  thread *t;
  int32_t r;

  args.file = icap->master_cap.fh;
  args.flags = icap->master_cap.flags | flags;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_open_by_fh_client (t, &args, vol->master);

  if (r == ZFS_OK)
    {
      if (!decode_zfs_cap (&t->dc, cap)
	  || !finish_decoding (&t->dc))
	return ZFS_INVALID_REPLY;

      icap->master_cap = *cap;
    }
  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc))
	return ZFS_INVALID_REPLY;
    }

  return r;
}

/* Open file for capability ICAP (whose internal file handle is FH)
   with open flags FLAGS on volume VOL.  Store ZFS capability to CAP.  */

static int
open_capability (zfs_cap *cap, internal_cap icap, unsigned int flags,
		 internal_fh fh, volume vol)
{
  int r;

  CHECK_MUTEX_LOCKED (&icap->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (vol->local_path)
    r = open_local_capability (cap, icap, flags, fh, vol);
  else if (vol->master != this_node)
    r = open_remote_capability (cap, icap, flags, vol);
  else
    abort ();

  return r;
}

/* Open file handle FH with open flags FLAGS and return capability in CAP.  */

int
zfs_open_by_fh (zfs_cap *cap, zfs_fh *fh, unsigned int flags)
{
  volume vol;
  internal_cap icap;
  internal_fh ifh;
  virtual_dir vd;
  int r;

  /* When O_CREAT is set the function zfs_open_by_name is called.
     The flag is superfluous here.  */
  flags &= ~O_CREAT;

  cap->fh = *fh;
  cap->flags = flags & O_ACCMODE;
  zfsd_mutex_lock (&cap_mutex);
  r = get_capability (cap, &icap, &vol, &ifh, &vd);
  zfsd_mutex_unlock (&cap_mutex);
  if (r != ZFS_OK)
    return r;

  if (vd)
    {
      zfsd_mutex_unlock (&icap->mutex);
      if (vol)
	zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&vd->mutex);
      return ZFS_OK;
    }

  r = open_capability (cap, icap, flags & ~O_ACCMODE, ifh, vol);
  zfsd_mutex_unlock (&icap->mutex);
  zfsd_mutex_unlock (&ifh->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  return r;
}

/* Close capability CAP.  */

int
zfs_close (zfs_cap *cap)
{
  volume vol;
  internal_cap icap;
  internal_fh ifh;
  virtual_dir vd;
  int r;

  zfsd_mutex_lock (&cap_mutex);
  r = find_capability (cap, &icap, &vol, &ifh, &vd);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&cap_mutex);
      return r;
    }

  if (vd)
    {
      put_capability (icap);
      zfsd_mutex_unlock (&cap_mutex);
      if (vol)
	zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&vd->mutex);
      return ZFS_OK;
    }

  if (icap->busy == 1)
    r = close_capability (icap, vol);
  else
    r = ZFS_OK;

  put_capability (icap);
  zfsd_mutex_unlock (&cap_mutex);
  zfsd_mutex_unlock (&ifh->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  return r;
}

/* Initialize data structures in FILE.C.  */

void
initialize_file_c ()
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
}

/* Destroy data structures in CAP.C.  */

void
cleanup_file_c ()
{
  free (internal_fd_data);
}

/*****************************************************************************/

int
zfs_open_by_name (zfs_fh *fh, zfs_fh *dir, const char *name, int flags,
		  sattr *attr)
{
#if 0
  int r;

  if (!(flags & O_CREAT))
    {
      r = zfs_lookup (fh, dir, name);
      if (r)
	return r;

      return zfs_open (fh);
    }
  else
    {
      /* FIXME: finish */
    }
#endif
  return ZFS_OK;
}
