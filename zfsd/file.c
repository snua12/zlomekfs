/* File operations.
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
#include <unistd.h>
#include <inttypes.h>
#include <linux/types.h>
#include <linux/dirent.h>
#include <linux/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "pthread.h"
#include "constant.h"
#include "memory.h"
#include "alloc-pool.h"
#include "fibheap.h"
#include "hashtab.h"
#include "varray.h"
#include "data-coding.h"
#include "fh.h"
#include "file.h"
#include "dir.h"
#include "config.h"
#include "cap.h"
#include "volume.h"
#include "metadata.h"
#include "network.h"
#include "md5.h"
#include "update.h"

/* int getdents(unsigned int fd, struct dirent *dirp, unsigned int count); */
_syscall3(int, getdents, uint, fd, struct dirent *, dirp, uint, count)

/* The array of data for each file descriptor.  */
internal_fd_data_t *internal_fd_data;

/* Heap of opened file descriptors.  */
static fibheap opened;

/* Mutex protecting access to OPENED.  */
static pthread_mutex_t opened_mutex;

/* Alloc pool for directory entries.  */
static alloc_pool dir_entry_pool;

/* Mutex protecting DIR_ENTRY_POOL.  */
static pthread_mutex_t dir_entry_mutex;

/* Initialize data for file descriptor of file handle FH.  */

static void
init_fh_fd_data (internal_fh fh)
{
  TRACE ("");
#ifdef ENABLE_CHECKING
  if (fh->fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&opened_mutex);
  CHECK_MUTEX_LOCKED (&internal_fd_data[fh->fd].mutex);

  internal_fd_data[fh->fd].fd = fh->fd;
  internal_fd_data[fh->fd].generation++;
  fh->generation = internal_fd_data[fh->fd].generation;
  internal_fd_data[fh->fd].heap_node
    = fibheap_insert (opened, (fibheapkey_t) time (NULL),
		      &internal_fd_data[fh->fd]);
}

/* Close file descriptor FD of local file.  */

static void
close_local_fd (int fd)
{
  TRACE ("");
#ifdef ENABLE_CHECKING
  if (fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&opened_mutex);
  CHECK_MUTEX_LOCKED (&internal_fd_data[fd].mutex);

#ifdef ENABLE_CHECKING
  if (internal_fd_data[fd].fd < 0)
    abort ();
#endif
  internal_fd_data[fd].fd = -1;
  internal_fd_data[fd].generation++;
  close (fd);
  if (internal_fd_data[fd].heap_node)
    {
      fibheap_delete_node (opened, internal_fd_data[fd].heap_node);
      internal_fd_data[fd].heap_node = NULL;
    }
  zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
}

/* Wrapper for open. If open fails because of too many open file descriptors
   it closes a file descriptor unused for longest time.  */

static int
safe_open (const char *pathname, uint32_t flags, uint32_t mode)
{
  int fd;

  TRACE ("");

retry_open:
  fd = open (pathname, flags, mode);
  if ((fd < 0 && errno == EMFILE)
      || (fd >= 0 && fibheap_size (opened) >= (unsigned int) max_local_fds))
    {
      internal_fd_data_t *fd_data;

      zfsd_mutex_lock (&opened_mutex);
      fd_data = (internal_fd_data_t *) fibheap_extract_min (opened);
#ifdef ENABLE_CHECKING
      if (!fd_data && fibheap_size (opened) > 0)
	abort ();
#endif
      if (fd_data)
	{
	  zfsd_mutex_lock (&fd_data->mutex);
	  fd_data->heap_node = NULL;
	  if (fd_data->fd >= 0)
	    close_local_fd (fd_data->fd);
	  else
	    zfsd_mutex_unlock (&fd_data->mutex);
	}
      zfsd_mutex_unlock (&opened_mutex);
      if (fd_data)
	goto retry_open;
    }

  return fd;
}

/* If local file for file handle FH is opened return true and lock
   INTERNAL_FD_DATA[FH->FD].MUTEX.  */

static bool
capability_opened_p (internal_fh fh)
{
  TRACE ("");

  if (fh->fd < 0)
    return false;

  zfsd_mutex_lock (&opened_mutex);
  zfsd_mutex_lock (&internal_fd_data[fh->fd].mutex);
  if (fh->generation != internal_fd_data[fh->fd].generation)
    {
      zfsd_mutex_unlock (&internal_fd_data[fh->fd].mutex);
      zfsd_mutex_unlock (&opened_mutex);
      return false;
    }

  internal_fd_data[fh->fd].heap_node
    = fibheap_replace_key (opened, internal_fd_data[fh->fd].heap_node,
			   (fibheapkey_t) time (NULL));
  zfsd_mutex_unlock (&opened_mutex);
  return true;
}

/* Open local file for dentry DENTRY with additional FLAGS on volume VOL.  */

static int32_t
capability_open (int *fd, uint32_t flags, internal_dentry dentry, volume vol)
{
  string path;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (flags & O_CREAT)
    abort ();
#endif

  /* Some flags were specified so close the file descriptor first.  */
  if (flags)
    local_close (dentry->fh);

  else if (capability_opened_p (dentry->fh))
    {
      *fd = dentry->fh->fd;
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&fh_mutex);
      return ZFS_OK;
    }

  if (dentry->fh->attr.type == FT_DIR)
    flags |= O_RDONLY;
  else
    flags |= O_RDWR;

  build_local_path (&path, vol, dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  dentry->fh->fd = safe_open (path.str, flags, 0);
  free (path.str);
  if (dentry->fh->fd >= 0)
    {
      zfsd_mutex_lock (&opened_mutex);
      zfsd_mutex_lock (&internal_fd_data[dentry->fh->fd].mutex);
      init_fh_fd_data (dentry->fh);
      zfsd_mutex_unlock (&opened_mutex);
      *fd = dentry->fh->fd;
      release_dentry (dentry);
      return ZFS_OK;
    }
  release_dentry (dentry);

  if (errno == ENOENT || errno == ENOTDIR)
    return ESTALE;

  return errno;
}

/* Close local file for internal file handle FH.  */

int32_t
local_close (internal_fh fh)
{
  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh->mutex);

  if (fh->fd >= 0)
    {
      zfsd_mutex_lock (&opened_mutex);
      zfsd_mutex_lock (&internal_fd_data[fh->fd].mutex);
      if (fh->generation == internal_fd_data[fh->fd].generation)
	close_local_fd (fh->fd);
      else
	zfsd_mutex_unlock (&internal_fd_data[fh->fd].mutex);
      zfsd_mutex_unlock (&opened_mutex);
      fh->fd = -1;
    }

  return ZFS_OK;
}

/* Close remote file for internal capability CAP for dentry DENTRY
   on volume VOL.  */

int32_t
remote_close (internal_cap cap, internal_dentry dentry, volume vol)
{
  zfs_cap args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_cap_undefined (cap->master_cap))
    abort ();
  if (zfs_fh_undefined (cap->master_cap.fh))
    abort ();
#endif

  args = cap->master_cap;

  release_dentry (dentry);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);
  zfsd_mutex_unlock (&vol->mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_close_client (t, &args, nod, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Create local file NAME in directory DIR on volume VOL with open flags FLAGS,
   set file attributes according to ATTR.  Store the newly opened file
   descriptor to FDP, create results to RES and metadata to META.
   If file already exists set EXISTS.  */

int32_t
local_create (create_res *res, int *fdp, internal_dentry dir, string *name,
	      uint32_t flags, sattr *attr, volume vol, metadata *meta,
	      bool *exists)
{
  struct stat st;
  string path;
  int32_t r;
  bool existed;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;

  build_local_path_name (&path, vol, dir, name);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  existed = (lstat (path.str, &st) == 0);
  if (exists)
    *exists = existed;

  attr->mode = GET_MODE (attr->mode);
  r = safe_open (path.str, O_RDWR | (flags & ~O_ACCMODE), attr->mode);
  if (r < 0)
    {
      free (path.str);
      if (errno == ENOENT || errno == ENOTDIR)
	return ESTALE;
      return errno;
    }
  *fdp = r;

  r = local_setattr_path (&res->attr, &path, attr);
  if (r != ZFS_OK)
    {
      close (*fdp);
      if (!*exists)
	unlink (path.str);
      free (path.str);
      return r;
    }

  free (path.str);
  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;

  vol = volume_lookup (res->file.vid);
#ifdef ENABLE_CHECKING
  if (!vol)
    abort ();
#endif

  meta->modetype = GET_MODETYPE (res->attr.mode, res->attr.type);
  meta->uid = res->attr.uid;
  meta->gid = res->attr.gid;
  if (!lookup_metadata (vol, &res->file, meta, true))
    MARK_VOLUME_DELETE (vol);
  else if (!existed)
    {
      if (!zfs_fh_undefined (meta->master_fh)
	  && !delete_metadata_of_created_file (vol, &res->file, meta))
	MARK_VOLUME_DELETE (vol);
    }
  zfsd_mutex_unlock (&vol->mutex);

  return ZFS_OK;
}

/* Create remote file NAME in directory DIR with open flags FLAGS,
   set file attributes according to ATTR.  */

int32_t
remote_create (create_res *res, internal_dentry dir, string *name,
	      uint32_t flags, sattr *attr, volume vol)
{
  create_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
#endif

  args.where.dir = dir->fh->meta.master_fh;
  args.where.name = *name;
  args.flags = flags;
  args.attr = *attr;

  release_dentry (dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_create_client (t, &args, vol->master, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_create_res (t->dc_reply, res)
	  || !finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }
  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Create file NAME in directory DIR with open flags FLAGS,
   set file attributes according to ATTR.  */

int32_t
zfs_create (create_res *res, zfs_fh *dir, string *name,
	    uint32_t flags, sattr *attr)
{
  create_res master_res;
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  zfs_fh tmp_fh;
  metadata meta;
  int32_t r, r2;
  int fd;
  bool exists;

  TRACE ("");

  /* When O_CREAT is NOT set the function zfs_open is called.
     Force O_CREAT to be set here.  */
  flags |= O_CREAT;

  r = validate_operation_on_zfs_fh (dir, EROFS, EINVAL);
  if (r != ZFS_OK)
    return r;

  /* Lookup DIR.  */
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd, true);
  if (r == ZFS_STALE)
    {
#ifdef ENABLE_CHECKING
      if (VIRTUAL_FH_P (*dir))
	abort ();
#endif
      r = refresh_fh (dir);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd, true);
    }
  if (r != ZFS_OK)
    {
      if (VIRTUAL_FH_P (*dir))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }

  if (pvd)
    {
      r = validate_operation_on_virtual_directory (pvd, name, &idir, EROFS);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }
  else
    zfsd_mutex_unlock (&fh_mutex);

  /* Hide special dirs in the root of the volume.  */
  if (SPECIAL_DIR_P (idir, name->str))
    {
      release_dentry (idir);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  if (idir->fh->meta.flags & METADATA_SHADOW_TREE)
    {
      release_dentry (idir);
      zfsd_mutex_unlock (&vol->mutex);
      return EPERM;
    }

  attr->mode = GET_MODE (attr->mode);
  attr->size = (uint64_t) -1;
  attr->atime = (zfs_time) -1;
  attr->mtime = (zfs_time) -1;

  r = internal_dentry_lock (LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  exists = false;
  if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
    {
      r = update_fh_if_needed (&vol, &idir, &tmp_fh);
      if (r != ZFS_OK)
	return r;
      r = local_create (res, &fd, idir, name, flags, attr, vol, &meta, &exists);
      if (r == ZFS_OK)
	zfs_fh_undefine (master_res.file);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_create (res, idir, name, flags, attr, vol);
      if (r == ZFS_OK)
	master_res.file = res->file;
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &idir, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      internal_cap icap;
      internal_dentry dentry;

      dentry = get_dentry (&res->file, &master_res.file, vol, idir, name,
			   &res->attr, &meta);
      icap = get_capability_no_zfs_fh_lookup (&res->cap, dentry,
					      flags & O_ACCMODE);

      if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
	{
	  /* Remote file is not open.  */
	  zfs_fh_undefine (icap->master_cap.fh);
	  zfs_cap_undefine (icap->master_cap);

	  if (vol->master != this_node)
	    {
	      if (!exists)
		{
		  if (!add_journal_entry (vol, idir->fh, &dentry->fh->local_fh,
					  &dentry->fh->meta.master_fh, name,
					  JOURNAL_OPERATION_ADD))
		    MARK_VOLUME_DELETE (vol);
		}
	    }
	  if (!inc_local_version (vol, idir->fh))
	    MARK_VOLUME_DELETE (vol);

	  if (vol->master != this_node)
	    {
	      if (load_interval_trees (vol, dentry->fh))
		{
		  local_close (dentry->fh);
		  dentry->fh->fd = fd;
		  memcpy (res->cap.verify, icap->local_cap.verify,
			  ZFS_VERIFY_LEN);

		  zfsd_mutex_lock (&opened_mutex);
		  zfsd_mutex_lock (&internal_fd_data[fd].mutex);
		  init_fh_fd_data (dentry->fh);
		  zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
		  zfsd_mutex_unlock (&opened_mutex);
		}
	      else
		{
		  MARK_VOLUME_DELETE (vol);
		  r = ZFS_METADATA_ERROR;
		  local_close (dentry->fh);
		  close (fd);
		}
	    }
	  else
	    {
	      local_close (dentry->fh);
	      dentry->fh->fd = fd;
	      memcpy (res->cap.verify, icap->local_cap.verify, ZFS_VERIFY_LEN);

	      zfsd_mutex_lock (&opened_mutex);
	      zfsd_mutex_lock (&internal_fd_data[fd].mutex);
	      init_fh_fd_data (dentry->fh);
	      zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
	      zfsd_mutex_unlock (&opened_mutex);
	    }
	}
      else if (vol->master != this_node)
	{
	  icap->master_cap = res->cap;
	  memcpy (res->cap.verify, icap->local_cap.verify, ZFS_VERIFY_LEN);
	}

      release_dentry (dentry);
    }

  internal_dentry_unlock (vol, idir);

  return r;
}

/* Open local file for dentry with open flags FLAGS on volume VOL.  */

int32_t
local_open (uint32_t flags, internal_dentry dentry, volume vol)
{
  int32_t r;
  int fd;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  r = capability_open (&fd, flags, dentry, vol);
  if (r == ZFS_OK)
    zfsd_mutex_unlock (&internal_fd_data[fd].mutex);

  return r;
}

/* Open remote file for capability ICAP (whose internal dentry is DENTRY)
   with open flags FLAGS on volume VOL.  Store ZFS capability to CAP.  */

int32_t
remote_open (zfs_cap *cap, internal_cap icap, uint32_t flags,
	     internal_dentry dentry, volume vol)
{
  open_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dentry->fh->meta.master_fh))
    abort ();
#endif

  /* Initialize capability.  */
  icap->master_cap.fh = dentry->fh->meta.master_fh;
  icap->master_cap.flags = icap->local_cap.flags;

  args.file = icap->master_cap.fh;
  args.flags = icap->master_cap.flags | flags;

  release_dentry (dentry);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_open_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_zfs_cap (t->dc_reply, cap)
	  || !finish_decoding (t->dc_reply))
	{
	  recycle_dc_to_fd (t->dc_reply, fd);
	  return ZFS_INVALID_REPLY;
	}
    }
  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Open file handle FH with open flags FLAGS and return capability in CAP.  */

int32_t
zfs_open (zfs_cap *cap, zfs_fh *fh, uint32_t flags)
{
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  virtual_dir vd;
  zfs_cap tmp_cap, remote_cap;
  int32_t r, r2;
  bool remote_call = false;

  TRACE ("");

  /* When O_CREAT is set the function zfs_create is called.
     The flag is superfluous here.  */
  flags &= ~O_CREAT;

  r = validate_operation_on_zfs_fh (fh, ((flags & O_ACCMODE) == O_RDONLY
					 ? ZFS_OK : EISDIR), EINVAL);
  if (r != ZFS_OK)
    return r;

  cap->fh = *fh;
  cap->flags = flags & O_ACCMODE;
  r = get_capability (cap, &icap, &vol, &dentry, &vd, true, true);
  if (r != ZFS_OK)
    return r;

  if (!dentry)
    {
      /* We are opening a pure virtual directory.  */
      if (vol)
	zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&vd->mutex);
      return ZFS_OK;
    }

  if (CONFLICT_DIR_P (dentry->fh->local_fh))
    {
      /* We are opening a conflict directory.  */
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      if (vd)
	zfsd_mutex_unlock (&vd->mutex);
      return ZFS_OK;
    }

  r = internal_cap_lock (LEVEL_SHARED, &icap, &vol, &dentry, &vd, &tmp_cap);
  if (r != ZFS_OK)
    return r;

  if (vd)
    {
      zfsd_mutex_unlock (&vd->mutex);
      zfsd_mutex_unlock (&vd_mutex);
    }

  flags &= ~O_ACCMODE;
  if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
    {
      r = update_cap_if_needed (&icap, &vol, &dentry, &vd, &tmp_cap);
      if (r != ZFS_OK)
	return r;

      if (vol->master != this_node)
	{
	  switch (dentry->fh->attr.type)
	    {
	      case FT_REG:
		if (load_interval_trees (vol, dentry->fh))
		  {
		    r = local_open (flags, dentry, vol);
		  }
		else
		  {
		    MARK_VOLUME_DELETE (vol);
		    r = ZFS_METADATA_ERROR;
		  }
		break;

	      case FT_DIR:
		r = local_open (flags, dentry, vol);
		break;

	      case FT_BLK:
	      case FT_CHR:
	      case FT_SOCK:
	      case FT_FIFO:
		if (volume_master_connected (vol))
		  {
		    zfsd_mutex_unlock (&fh_mutex);
		    r = remote_open (&remote_cap, icap, flags, dentry, vol);
		    remote_call = true;
		  }
		else
		  r = local_open (flags, dentry, vol);
		break;

	      default:
		abort ();
	    }
	}
      else
	r = local_open (flags, dentry, vol);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_open (&remote_cap, icap, flags, dentry, vol);
      remote_call = true;
    }
  else
    abort ();

  if (VIRTUAL_FH_P (tmp_cap.fh))
    zfsd_mutex_lock (&vd_mutex);
  r2 = find_capability_nolock (&tmp_cap, &icap, &vol, &dentry, &vd, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      if (remote_call)
	icap->master_cap = remote_cap;
    }
  else
    {
      if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh) && vol->master != this_node)
	{
	  if (dentry->fh->attr.type == FT_REG
	      && !save_interval_trees (vol, dentry->fh))
	    {
	      MARK_VOLUME_DELETE (vol);
	      r = ZFS_METADATA_ERROR;
	    }
	}

      put_capability (icap, dentry->fh, vd);
    }

  internal_cap_unlock (vol, dentry, vd);

  return r;
}

/* Close capability CAP.  */

int32_t
zfs_close (zfs_cap *cap)
{
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  virtual_dir vd;
  zfs_cap tmp_cap;
  int32_t r, r2;

  TRACE ("");

  r = validate_operation_on_zfs_fh (&cap->fh, ZFS_OK, EINVAL);
  if (r != ZFS_OK)
    return r;

  r = find_capability (cap, &icap, &vol, &dentry, &vd, true);
  if (r != ZFS_OK)
    return r;

  if (!dentry)
    {
      /* We are closing a pure virtual directory.  */
      put_capability (icap, NULL, vd);
      if (vol)
	zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&vd->mutex);
      return ZFS_OK;
    }

  if (CONFLICT_DIR_P (dentry->fh->local_fh))
    {
      /* We are closing a conflict directory.  */
      put_capability (icap, dentry->fh, vd);
      if (vd)
	zfsd_mutex_unlock (&vd->mutex);
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return ZFS_OK;
    }

  r = internal_cap_lock (LEVEL_SHARED, &icap, &vol, &dentry, &vd, &tmp_cap);
  if (r != ZFS_OK)
    return r;

  if (vd)
    {
      zfsd_mutex_unlock (&vd->mutex);
      zfsd_mutex_unlock (&vd_mutex);
    }
  if (dentry)
    zfsd_mutex_unlock (&fh_mutex);

  if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
    {
      if (!zfs_cap_undefined (icap->master_cap)
	  && (dentry->fh->attr.type == FT_BLK
	      || dentry->fh->attr.type == FT_CHR
	      || dentry->fh->attr.type == FT_SOCK
	      || dentry->fh->attr.type == FT_FIFO))
	{
	  r = remote_close (icap, dentry, vol);

	  r2 = find_capability (&tmp_cap, &icap, &vol, &dentry, &vd, true);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif
	}
      else
	r = ZFS_OK;

      if (icap->busy == 1)
	{
	  if (vol->master != this_node)
	    {
	      if (dentry->fh->attr.type == FT_REG
		  && !save_interval_trees (vol, dentry->fh))
		MARK_VOLUME_DELETE (vol);
	    }
	  zfsd_mutex_unlock (&vol->mutex);
	  r = local_close (dentry->fh);
	}
      else
	{
	  zfsd_mutex_unlock (&vol->mutex);
	}
      release_dentry (dentry);
    }
  else if (vol->master != this_node)
    r = remote_close (icap, dentry, vol);
  else
    abort ();

  if (VIRTUAL_FH_P (tmp_cap.fh))
    zfsd_mutex_lock (&vd_mutex);
  r2 = find_capability_nolock (&tmp_cap, &icap, &vol, &dentry, &vd, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  /* Reread config file.  */
  if (cap->fh.vid == VOLUME_ID_CONFIG
      && (cap->flags == O_WRONLY || cap->flags == O_RDWR))
    add_reread_config_request_dentry (dentry);

  if (r == ZFS_OK)
    put_capability (icap, dentry->fh, vd);

  internal_cap_unlock (vol, dentry, vd);

  return r;
}

/* Encode one directory entry (INO, COOKIE, NAME[NAME_LEN]) to DC LIST->BUFFER.
   Additional data is passed in DATA.  */

bool
filldir_encode (uint32_t ino, int32_t cookie, char *name, uint32_t name_len,
		dir_list *list, readdir_data *data)
{
  DC *dc = (DC *) list->buffer;
  char *old_pos;
  unsigned int old_len;
  dir_entry entry;

#ifdef ENABLE_CHECKING
  if (name[0] == 0)
    abort ();
#endif

  entry.ino = ino;
  entry.cookie = cookie;
  entry.name.str = name;
  entry.name.len = name_len;

  /* Try to encode ENTRY to DC.  */
  old_pos = dc->cur_pos;
  old_len = dc->cur_length;
  if (!encode_dir_entry (dc, &entry)
      || data->written + dc->cur_length - old_len > data->count)
    {
      /* There is not enough space in DC to encode ENTRY.  */
      dc->cur_pos = old_pos;
      dc->cur_length = old_len;
      return false;
    }
  else
    {
      list->n++;
      data->written += dc->cur_length - old_len;
    }
  return true;
}

/* Store one directory entry (INO, COOKIE, NAME[NAME_LEN]) to array
   LIST->BUFFER.  */

bool
filldir_array (uint32_t ino, int32_t cookie, char *name, uint32_t name_len,
	       dir_list *list, ATTRIBUTE_UNUSED readdir_data *data)
{
  dir_entry *entries = (dir_entry *) list->buffer;

  if (list->n >= ZFS_MAX_DIR_ENTRIES)
    return false;

  entries[list->n].ino = ino;
  entries[list->n].cookie = cookie;
  entries[list->n].name.str = (char *) xmemdup (name, name_len + 1);
  entries[list->n].name.len = name_len;
  list->n++;
  return true;
}

/* Hash function for directory entry ENTRY.  */
#define FILLDIR_HTAB_HASH(ENTRY)					\
  crc32_buffer ((ENTRY)->name.str, (ENTRY)->name.len)

/* Hash function for directory entry X being inserted for filldir htab.  */

hash_t
filldir_htab_hash (const void *x)
{
  return FILLDIR_HTAB_HASH ((dir_entry *) x);
}

/* Compare directory entries XX and YY.  */

int
filldir_htab_eq (const void *xx, const void *yy)
{
  const dir_entry *x = (const dir_entry *) xx;
  const dir_entry *y = (const dir_entry *) yy;

  return (x->name.len == y->name.len
	  && memcmp (x->name.str, y->name.str, x->name.len) == 0);
}

/* Free directory entry XX.  */

void
filldir_htab_del (void *xx)
{
  dir_entry *x = (dir_entry *) xx;

  free (x->name.str);
  zfsd_mutex_lock (&dir_entry_mutex);
  pool_free (dir_entry_pool, x);
  zfsd_mutex_unlock (&dir_entry_mutex);
}

/* Store one directory entry (INO, COOKIE, NAME[NAME_LEN]) to hash table
   LIST->BUFFER.  */

bool
filldir_htab (uint32_t ino, int32_t cookie, char *name, uint32_t name_len,
	      dir_list *list, ATTRIBUTE_UNUSED readdir_data *data)
{
  filldir_htab_entries *entries = (filldir_htab_entries *) list->buffer;
  dir_entry *entry;
  void **slot;

  entries->last_cookie = cookie;

  /* Do not add "." and "..".  */
  if (name[0] == '.'
      && (name[1] == 0
	  || (name[1] == '.'
	      && name[2] == 0)))
    return true;

  zfsd_mutex_lock (&dir_entry_mutex);
  entry = (dir_entry *) pool_alloc (dir_entry_pool);
  zfsd_mutex_unlock (&dir_entry_mutex);
  entry->ino = ino;
  entry->cookie = cookie;
  entry->name.str = (char *) xmemdup (name, name_len + 1);
  entry->name.len = name_len;

  slot = htab_find_slot_with_hash (entries->htab, entry,
				   FILLDIR_HTAB_HASH (entry),
				   INSERT);
  if (*slot)
    {
      htab_clear_slot (entries->htab, slot);
      list->n--;
    }

  *slot = entry;
  list->n++;

  return true;
}

/* Read DATA->COUNT bytes from virtual directory VD starting at position
   COOKIE.  Store directory entries to LIST using function FILLDIR.  */

static bool
read_virtual_dir (dir_list *list, virtual_dir vd, int32_t cookie,
		  readdir_data *data, filldir_f filldir)
{
  uint32_t ino;
  unsigned int i;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vd_mutex);
  CHECK_MUTEX_LOCKED (&vd->mutex);

  if (cookie > 0)
    return true;

  switch (cookie)
    {
      case 0:
	cookie--;
	if (!(*filldir) (vd->fh.ino, cookie, ".", 1, list, data))
	  return false;
	/* Fallthru.  */

      case -1:
	if (vd->parent)
	  {
	    zfsd_mutex_lock (&vd->parent->mutex);
	    ino = vd->parent->fh.ino;
	    zfsd_mutex_unlock (&vd->parent->mutex);
	  }
	else
	  ino = vd->fh.ino;

	cookie--;
	if (!(*filldir) (ino, cookie, "..", 2, list, data))
	  return false;
	/* Fallthru.  */

      default:
	for (i = -cookie - 2; i < VARRAY_USED (vd->subdirs); i++)
	  {
	    virtual_dir svd;

	    svd = VARRAY_ACCESS (vd->subdirs, i, virtual_dir);
	    zfsd_mutex_lock (&svd->mutex);
	    cookie--;
	    if (!(*filldir) (svd->fh.ino, cookie, svd->name.str,
			     svd->name.len, list, data))
	      {
		zfsd_mutex_unlock (&svd->mutex);
		return false;
	      }
	    zfsd_mutex_unlock (&svd->mutex);
	  }
	if (i >= VARRAY_USED (vd->subdirs))
	  list->eof = 1;
	break;
    }

  return true;
}

/* Read DATA->COUNT bytes from conflict directory IDIR on volume VOL
   starting at position COOKIE.  Store directory entries to LIST using
   function FILLDIR.  */

static int32_t
read_conflict_dir (dir_list *list, internal_dentry idir, virtual_dir vd,
		   int32_t cookie, readdir_data *data, volume vol,
		   filldir_f filldir)
{
  uint32_t ino;
  unsigned int i;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&idir->fh->mutex);

  if (vd)
    {
      if (!read_virtual_dir (list, vd, cookie, data, filldir))
	return (list->n == 0) ? EINVAL : ZFS_OK;
      if (cookie < 2)
	cookie = 2;
    }

  list->eof = 0;
  if (cookie < 0)
    cookie = 0;

  switch (cookie)
    {
      case 0:
	cookie++;
	if (!(*filldir) (idir->fh->local_fh.ino, cookie, ".", 1, list, data))
	  return false;
	/* Fallthru.  */

      case 1:
	if (idir->parent)
	  {
	    zfsd_mutex_lock (&idir->parent->fh->mutex);
	    ino = idir->parent->fh->local_fh.ino;
	    zfsd_mutex_unlock (&idir->parent->fh->mutex);
	  }
	else
	  {
	    virtual_dir pvd;

	    /* This is safe because the virtual directory can't be destroyed
	       while volume is locked.  */
	    pvd = vol->root_vd->parent ? vol->root_vd->parent : vol->root_vd;
	    ino = pvd->fh.ino;
	  }

	cookie++;
	if (!(*filldir) (ino, cookie, "..", 2, list, data))
	  return false;
	/* Fallthru.  */

      default:
	for (i = cookie - 2; i < VARRAY_USED (idir->fh->subdentries); i++)
	  {
	    internal_dentry dentry;

	    dentry = VARRAY_ACCESS (idir->fh->subdentries, i, internal_dentry);
	    zfsd_mutex_lock (&dentry->fh->mutex);

	    if (vd)
	      {
		virtual_dir svd;
		svd = vd_lookup_name (vd, &dentry->name);
		if (svd)
		  {
		    zfsd_mutex_unlock (&svd->mutex);
		    zfsd_mutex_unlock (&dentry->fh->mutex);
		    continue;
		  }
	      }

	    cookie++;
	    if (!(*filldir) (dentry->fh->local_fh.ino, cookie,
			     dentry->name.str, dentry->name.len, list, data))
	      {
		zfsd_mutex_unlock (&dentry->fh->mutex);
		return false;
	      }
	    zfsd_mutex_unlock (&dentry->fh->mutex);
	  }
	if (i >= VARRAY_USED (idir->fh->subdentries))
	  list->eof = 1;
	break;
    }

  return true;
}

/* Read COUNT bytes from local directory with DENTRY and virtual directory VD
   on volume VOL starting at position COOKIE.
   Store directory entries to LIST using function FILLDIR.  */

int32_t
local_readdir (dir_list *list, internal_dentry dentry, virtual_dir vd,
	       int32_t cookie, readdir_data *data, volume vol,
	       filldir_f filldir)
{
  char buf[ZFS_MAXDATA];
  int32_t r, pos;
  struct dirent *de;
  int fd;
  bool local_volume_root;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (vol)
    CHECK_MUTEX_LOCKED (&vol->mutex);
  if (dentry)
    {
      CHECK_MUTEX_LOCKED (&fh_mutex);
      CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
    }
  if (vd)
    {
      CHECK_MUTEX_LOCKED (&vd_mutex);
      CHECK_MUTEX_LOCKED (&vd->mutex);
    }
#endif

  if (vd)
    {
      if (!read_virtual_dir (list, vd, cookie, data, filldir))
	{
	  zfsd_mutex_unlock (&vd->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  if (dentry)
	    {
	      release_dentry (dentry);
	      zfsd_mutex_unlock (&fh_mutex);
	    }
	  if (vol)
	    zfsd_mutex_unlock (&vol->mutex);
	  return (list->n == 0) ? EINVAL : ZFS_OK;
	}

      if (!dentry)
	{
	  zfsd_mutex_unlock (&vd->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  if (vol)
	    zfsd_mutex_unlock (&vol->mutex);
	}
    }

  if (dentry)
    {
      local_volume_root = LOCAL_VOLUME_ROOT_P (dentry);

      r = capability_open (&fd, 0, dentry, vol);
      if (r != ZFS_OK)
	goto out;

      list->eof = 0;
      if (cookie < 0)
	cookie = 0;

      r = lseek (fd, cookie, SEEK_SET);
      if (r < 0)
	{
	  zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
	  r = errno;
	  goto out;
	}

      while (1)
	{
	  r = getdents (fd, (struct dirent *) buf, ZFS_MAXDATA);
	  if (r <= 0)
	    {
	      zfsd_mutex_unlock (&internal_fd_data[fd].mutex);

	      /* Comment from glibc: On some systems getdents fails with ENOENT when
		 open directory has been rmdir'd already.  POSIX.1 requires that we
		 treat this condition like normal EOF.  */
	      if (r < 0 && errno == ENOENT)
		r = 0;

	      if (r == 0)
		{
		  list->eof = 1;
		  r = ZFS_OK;
		  goto out;
		}

	      /* EINVAL means that buffer was too small.  */
	      r = (errno == EINVAL && list->n > 0) ? ZFS_OK : errno;
	      goto out;
	    }

	  for (pos = 0; pos < r; pos += de->d_reclen)
	    {
	      de = (struct dirent *) &buf[pos];
	      cookie = de->d_off;

	      /* Hide special dirs in the root of the volume.  */
	      if (local_volume_root && SPECIAL_NAME_P (de->d_name))
		continue;

	      if (vd)
		{
		  virtual_dir svd;
		  string name;

		  /* Hide "." and "..".  */
		  if (de->d_name[0] == '.'
		      && (de->d_name[1] == 0
			  || (de->d_name[1] == '.'
			      && de->d_name[2] == 0)))
		    continue;

		  /* Hide files which have the same name like some virtual
		     directory.  */
		  name.str = de->d_name;
		  name.len = strlen (de->d_name);
		  svd = vd_lookup_name (vd, &name);
		  if (svd)
		    {
		      zfsd_mutex_unlock (&svd->mutex);
		      continue;
		    }
		}
	      if (!(*filldir) (de->d_ino, cookie, de->d_name,
			       strlen (de->d_name), list, data))
		{
		  zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
		  r = ZFS_OK;
		  goto out;
		}
	    }
	}

out:
      if (vd)
	{
	  zfsd_mutex_unlock (&vd->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	}
      return r;
    }

  return ZFS_OK;
}

/* Read COUNT bytes from remote directory CAP of dentry DENTRY on volume VOL
   starting at position COOKIE.
   Store directory entries to LIST using function FILLDIR.  */

int32_t
remote_readdir (dir_list *list, internal_cap cap, internal_dentry dentry,
		int32_t cookie, readdir_data *data, volume vol,
		filldir_f filldir)
{
  read_dir_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_cap_undefined (cap->master_cap))
    abort ();
  if (zfs_fh_undefined (cap->master_cap.fh))
    abort ();
#endif

  args.cap = cap->master_cap;
  args.cookie = cookie;
  args.count = data->count;

  release_dentry (dentry);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);
  zfsd_mutex_unlock (&vol->mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_readdir_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (filldir == &filldir_encode)
	{
	  DC *dc = (DC *) list->buffer;

	  if (!decode_dir_list (t->dc_reply, list))
	    r = ZFS_INVALID_REPLY;
	  else if (t->dc_reply->max_length > t->dc_reply->cur_length)
	    {
	      memcpy (dc->cur_pos, t->dc_reply->cur_pos,
		      t->dc_reply->max_length - t->dc_reply->cur_length);
	      dc->cur_pos += t->dc_reply->max_length - t->dc_reply->cur_length;
	      dc->cur_length += t->dc_reply->max_length - t->dc_reply->cur_length;
	    }
	}
      else if (filldir == &filldir_array)
	{
	  if (!decode_dir_list (t->dc_reply, list))
	    r = ZFS_INVALID_REPLY;
	  else
	    {
	      uint32_t i;
	      dir_entry *entries = (dir_entry *) list->buffer;

	      if (list->n <= ZFS_MAX_DIR_ENTRIES)
		{
		  for (i = 0; i < list->n; i++)
		    {
		      if (!decode_dir_entry (t->dc_reply, &entries[i]))
			{
			  list->n = i;
			  r = ZFS_INVALID_REPLY;
			  break;
			}
		      else
			xstringdup (&entries[i].name, &entries[i].name);
		    }
		  if (!finish_decoding (t->dc_reply))
		    r = ZFS_INVALID_REPLY;
		}
	      else
		r = ZFS_INVALID_REPLY;
	    }
	}
      else if (filldir == &filldir_htab)
	{
	  dir_list tmp;

	  if (!decode_dir_list (t->dc_reply, &tmp))
	    r = ZFS_INVALID_REPLY;
	  else
	    {
	      uint32_t i;

	      list->eof = tmp.eof;
	      for (i = 0; i < tmp.n; i++)
		{
		  filldir_htab_entries *entries
		    = (filldir_htab_entries *) list->buffer;
		  dir_entry *entry;
		  void **slot;

		  zfsd_mutex_lock (&dir_entry_mutex);
		  entry = (dir_entry *) pool_alloc (dir_entry_pool);
		  zfsd_mutex_unlock (&dir_entry_mutex);

		  if (!decode_dir_entry (t->dc_reply, entry))
		    {
		      r = ZFS_INVALID_REPLY;
		      zfsd_mutex_lock (&dir_entry_mutex);
		      pool_free (dir_entry_pool, entry);
		      zfsd_mutex_unlock (&dir_entry_mutex);
		      break;
		    }

		  entries->last_cookie = entry->cookie;

		  /* Do not add "." and "..".  */
		  if (entry->name.str[0] == '.'
		      && (entry->name.str[1] == 0
			  || (entry->name.str[1] == '.'
			      && entry->name.str[2] == 0)))
		    {
		      zfsd_mutex_lock (&dir_entry_mutex);
		      pool_free (dir_entry_pool, entry);
		      zfsd_mutex_unlock (&dir_entry_mutex);
		      continue;
		    }

		  xstringdup (&entry->name, &entry->name);
		  slot = htab_find_slot_with_hash (entries->htab, entry,
						   FILLDIR_HTAB_HASH (entry),
						   INSERT);
		  if (*slot)
		    {
		      htab_clear_slot (entries->htab, slot);
		      list->n--;
		    }

		  *slot = entry;
		  list->n++;
		}
	      if (!finish_decoding (t->dc_reply))
		r = ZFS_INVALID_REPLY;
	    }
	}
      else
	abort ();
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Read COUNT bytes from directory CAP starting at position COOKIE.
   Store directory entries to LIST using function FILLDIR.  */

int32_t
zfs_readdir (dir_list *list, zfs_cap *cap, int32_t cookie, uint32_t count,
	     filldir_f filldir)
{
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  virtual_dir vd;
  readdir_data data;
  zfs_cap tmp_cap;
  int32_t r, r2;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (list->n != 0
      || list->eof != 0
      || list->buffer == 0)
    abort ();
#endif

  if (cap->flags != O_RDONLY)
    return EBADF;

  r = validate_operation_on_zfs_fh (&cap->fh, ZFS_OK, EINVAL);
  if (r != ZFS_OK)
    return r;

  if (VIRTUAL_FH_P (cap->fh))
    zfsd_mutex_lock (&vd_mutex);
  r = find_capability_nolock (cap, &icap, &vol, &dentry, &vd, true);
  if (VIRTUAL_FH_P (cap->fh) && !vd)
    zfsd_mutex_unlock (&vd_mutex);
  if (r != ZFS_OK)
    return r;

  if (dentry)
    {
      zfsd_mutex_unlock (&fh_mutex);
      if (vd)
	zfsd_mutex_unlock (&vd_mutex);
      if (dentry->fh->attr.type != FT_DIR)
	{
	  if (vd)
	    zfsd_mutex_unlock (&vd->mutex);
	  release_dentry (dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	  return ENOTDIR;
	}

      r = internal_cap_lock (LEVEL_SHARED, &icap, &vol, &dentry, &vd, &tmp_cap);
      if (r != ZFS_OK)
	return r;
    }

  data.written = 0;
  data.count = (count > ZFS_MAXDATA) ? ZFS_MAXDATA : count;

  if (dentry && CONFLICT_DIR_P (dentry->fh->local_fh))
    {
      if (!read_conflict_dir (list, dentry, vd, cookie, &data, vol, filldir))
	r = (list->n == 0) ? EINVAL : ZFS_OK;
      else
	r = ZFS_OK;
      if (vd)
	{
	  zfsd_mutex_unlock (&vd->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	}
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&fh_mutex);
    }
  else if (!dentry || INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
    {
      r = local_readdir (list, dentry, vd, cookie, &data, vol, filldir);
    }
  else if (vol->master != this_node)
    {
      if (vd)
	{
	  zfsd_mutex_unlock (&vd->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	}
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_readdir (list, icap, dentry, cookie, &data, vol, filldir);
    }
  else
    abort ();

  /* Cleanup decoded directory entries on error.  */
  if (r != ZFS_OK && list->n > 0)
    {
      if (filldir == &filldir_array)
	{
	  uint32_t i;
	  dir_entry *entries = (dir_entry *) list->buffer;

	  for (i = 0; i < list->n; i++)
	    free (entries[i].name.str);
	}
      else if (filldir == &filldir_htab)
	{
	  filldir_htab_entries *entries
	    = (filldir_htab_entries *) list->buffer;

	  htab_empty (entries->htab);
	}
    }

  if (dentry)
    {
      if (VIRTUAL_FH_P (tmp_cap.fh))
	zfsd_mutex_lock (&vd_mutex);
      r2 = find_capability_nolock (&tmp_cap, &icap, &vol, &dentry, &vd, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      internal_cap_unlock (vol, dentry, vd);
    }

  return r;
}

/* Read COUNT bytes from offset OFFSET of local file DENTRY on volume VOL.
   Store data to BUFFER and count to RCOUNT.  */

static int32_t
local_read (uint32_t *rcount, void *buffer, internal_dentry dentry,
	    uint64_t offset, uint32_t count, volume vol)
{
  int32_t r;
  int fd;
  bool regular_file;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  regular_file = dentry->fh->attr.type == FT_REG;
  r = capability_open (&fd, 0, dentry, vol);
  if (r != ZFS_OK)
    return r;

  if (regular_file || offset != (uint64_t) -1)
    {
      r = lseek (fd, offset, SEEK_SET);
      if (r < 0)
	{
	  zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
	  return errno;
	}
    }

  r = read (fd, buffer, count);
  if (r < 0)
    {
      zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
      return errno;
    }

  *rcount = r;

  zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
  return ZFS_OK;
}

/* Read COUNT bytes from offset OFFSET of remote file with capability CAP
   of dentry DENTRY on volume VOL.
   Store data to BUFFER and count to RCOUNT.  */

static int32_t
remote_read (uint32_t *rcount, void *buffer, internal_cap cap,
	     internal_dentry dentry, uint64_t offset, uint32_t count,
	     volume vol)
{
  read_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_cap_undefined (cap->master_cap))
    abort ();
  if (zfs_fh_undefined (cap->master_cap.fh))
    abort ();
#endif

  args.cap = cap->master_cap;
  args.offset = offset;
  args.count = count;

  release_dentry (dentry);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);
  zfsd_mutex_unlock (&vol->mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_read_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_uint32_t (t->dc_reply, rcount)
	  || t->dc_reply->cur_length + *rcount != t->dc_reply->max_length)
	r = ZFS_INVALID_REPLY;
      else
	memcpy (buffer, t->dc_reply->cur_pos, *rcount);
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Align the range of length COUNT starting at OFFSET and store the bounds of
   resulting range to START and END.  */

static void
align_range (uint64_t offset, uint32_t count, uint64_t *start, uint64_t *end)
{
  uint64_t block;

  /* First check whether the range is contained in a block of size
     ZFS_UPDATED_BLOCK_SIZE aligned to ZFS_UPDATED_BLOCK_SIZE.  */
  block = offset / ZFS_UPDATED_BLOCK_SIZE * ZFS_UPDATED_BLOCK_SIZE;
  if (offset + count <= block + ZFS_UPDATED_BLOCK_SIZE)
    {
      *start = block;
      *end = block + ZFS_UPDATED_BLOCK_SIZE;
      return;
    }

  /* Then check whether the range is contained in a block of size
     ZFS_UPDATED_BLOCK_SIZE aligned to ZFS_MODIFIED_BLOCK_SIZE.  */
  block = offset / ZFS_MODIFIED_BLOCK_SIZE * ZFS_MODIFIED_BLOCK_SIZE;
  if (offset + count <= block + ZFS_UPDATED_BLOCK_SIZE)
    {
      *start = block;
      *end = block + ZFS_UPDATED_BLOCK_SIZE;
      return;
    }

  /* Finally enlarge the range to be ZFS_UPDATED_BLOCK_SIZE long.  */
  *start = offset;
  *end = offset + (count <= ZFS_UPDATED_BLOCK_SIZE
		   ? ZFS_UPDATED_BLOCK_SIZE : count);
}

/* Read COUNT bytes from file CAP at offset OFFSET, sotre the count of bytes
   read to RCOUNT and the data to BUFFER.  If UPDATE is true update the
   local file on copied volume.  */

int32_t
zfs_read (uint32_t *rcount, void *buffer,
	  zfs_cap *cap, uint64_t offset, uint32_t count, bool update)
{
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  zfs_cap tmp_cap;
  int32_t r, r2;

  TRACE ("offset = %" PRIu64 " count = %" PRIu32, offset, count);

  if (count > ZFS_MAXDATA)
    return EINVAL;

  if (cap->flags != O_RDONLY && cap->flags != O_RDWR)
    return EBADF;

  if (VIRTUAL_FH_P (cap->fh))
    return EISDIR;

  r = validate_operation_on_zfs_fh (&cap->fh, EISDIR, EINVAL);
  if (r != ZFS_OK)
    return r;

  r = find_capability (cap, &icap, &vol, &dentry, NULL, true);
  if (r != ZFS_OK)
    return r;

  if (dentry->fh->attr.type == FT_DIR)
    {
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return EISDIR;
    }

  r = internal_cap_lock (LEVEL_SHARED, &icap, &vol, &dentry, NULL, &tmp_cap);
  if (r != ZFS_OK)
    return r;

  if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
    {
      if (zfs_fh_undefined (dentry->fh->meta.master_fh))
	r = local_read (rcount, buffer, dentry, offset, count, vol);
      else if (dentry->fh->attr.type == FT_REG && update)
	{
	  varray blocks;
	  uint64_t start;
	  uint64_t end;
	  uint64_t offset2;
	  unsigned int i;
	  bool complete;

	  align_range (offset, count, &start, &end);
	  get_blocks_for_updating (dentry->fh, start, end, &blocks);

	  complete = true;
	  offset2 = offset + count;
	  for (i = 0; i < VARRAY_USED (blocks); i++)
	    {
	      if (offset2 <= VARRAY_ACCESS (blocks, i, interval).start)
		break;

	      if (VARRAY_ACCESS (blocks, i, interval).start <= offset
		  && offset < VARRAY_ACCESS (blocks, i, interval).end)
		{
		  complete = false;
		  break;
		}
	      else if (VARRAY_ACCESS (blocks, i, interval).start < offset2
		       && offset2 <= VARRAY_ACCESS (blocks, i, interval).end)
		{
		  complete = false;
		  break;
		}
	    }

	  if (complete)
	    {
	      r = local_read (rcount, buffer, dentry, offset, count, vol);
	    }
	  else
	    {
	      release_dentry (dentry);
	      zfsd_mutex_unlock (&vol->mutex);
	      zfsd_mutex_unlock (&fh_mutex);

	      r = update_file_blocks (&tmp_cap, &blocks);
	      if (r == ZFS_OK)
		{
		  r2 = find_capability_nolock (&tmp_cap, &icap, &vol, &dentry,
					       NULL, false);
#ifdef ENABLE_CHECKING
		  if (r2 != ZFS_OK)
		    abort ();
#endif

		  r = local_read (rcount, buffer, dentry, offset, count, vol);
		}
	    }

	  varray_destroy (&blocks);
	}
      else
	{
	  switch (dentry->fh->attr.type)
	    {
	      case FT_REG:
		r = local_read (rcount, buffer, dentry, offset, count, vol);
		break;

	      case FT_BLK:
	      case FT_CHR:
	      case FT_SOCK:
	      case FT_FIFO:
		if (!zfs_cap_undefined (icap->master_cap))
		  {
		    zfsd_mutex_unlock (&fh_mutex);
		    r = remote_read (rcount, buffer, icap, dentry, offset,
				     count, vol);
		  }
		else
		  r = local_read (rcount, buffer, dentry, offset, count, vol);
		break;

	      default:
		abort ();
	    }
	}
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_read (rcount, buffer, icap, dentry, offset, count, vol);
    }
  else
    abort ();

  r2 = find_capability_nolock (&tmp_cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  internal_cap_unlock (vol, dentry, NULL);

  return r;
}

/* Write DATA to offset OFFSET of local file DENTRY on volume VOL.  */

static int32_t
local_write (write_res *res, internal_dentry dentry,
	     uint64_t offset, data_buffer *data, volume vol)
{
  int32_t r;
  int fd;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  r = capability_open (&fd, 0, dentry, vol);
  if (r != ZFS_OK)
    return r;

  r = lseek (fd, offset, SEEK_SET);
  if (r < 0)
    {
      zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
      return errno;
    }

  r = write (fd, data->buf, data->len);
  if (r < 0)
    {
      zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
      return errno;
    }
  res->written = r;

  zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
  return ZFS_OK;
}

/* Write to remote file with capability CAP of dentry DENTRY
   on volume VOL.  */

static int32_t
remote_write (write_res *res, internal_cap cap, internal_dentry dentry,
	      write_args *args, volume vol)
{
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_cap_undefined (cap->master_cap))
    abort ();
  if (zfs_fh_undefined (cap->master_cap.fh))
    abort ();
#endif

  args->cap = cap->master_cap;

  release_dentry (dentry);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);
  zfsd_mutex_unlock (&vol->mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_write_client (t, args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_write_res (t->dc_reply, res)
	  || !finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Write to file.  */

int32_t
zfs_write (write_res *res, write_args *args)
{
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  zfs_cap tmp_cap;
  int32_t r, r2;

  TRACE ("");

  if (args->data.len > ZFS_MAXDATA)
    return EINVAL;

  if (args->cap.flags != O_WRONLY && args->cap.flags != O_RDWR)
    return EBADF;

  if (VIRTUAL_FH_P (args->cap.fh))
    return EISDIR;

  r = validate_operation_on_zfs_fh (&args->cap.fh, EINVAL, EINVAL);
  if (r != ZFS_OK)
    return r;

  r = find_capability (&args->cap, &icap, &vol, &dentry, NULL, true);
  if (r != ZFS_OK)
    return r;

#ifdef ENABLE_CHECKING
  /* We did not allow directory to be opened for writing so there should be
     no capability for writing to directory.  */
  if (dentry->fh->attr.type == FT_DIR)
    abort ();
#endif

  r = internal_cap_lock (LEVEL_SHARED, &icap, &vol, &dentry, NULL, &tmp_cap);
  if (r != ZFS_OK)
    return r;

  if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
    {
      if (zfs_fh_undefined (dentry->fh->meta.master_fh))
	r = local_write (res, dentry, args->offset, &args->data, vol);
      else
	{
	  switch (dentry->fh->attr.type)
	    {
	      case FT_REG:
		r = local_write (res, dentry, args->offset, &args->data, vol);
		break;

	      case FT_BLK:
	      case FT_CHR:
	      case FT_SOCK:
	      case FT_FIFO:
		if (!zfs_cap_undefined (icap->master_cap))
		  {
		    zfsd_mutex_unlock (&fh_mutex);
		    r = remote_write (res, icap, dentry, args, vol);
		  }
		else
		  r = local_write (res, dentry, args->offset, &args->data, vol);
		break;

	      default:
		abort ();
	    }
	}
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_write (res, icap, dentry, args, vol);
    }
  else
    abort ();

  r2 = find_capability_nolock (&tmp_cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh)
	  && dentry->fh->attr.type == FT_REG)
	{
	  if (!set_metadata_flags (vol, dentry->fh,
				   dentry->fh->meta.flags | METADATA_MODIFIED))
	    {
	      MARK_VOLUME_DELETE (vol);
	    }
	  else
	    {
	      if (!zfs_fh_undefined (dentry->fh->meta.master_fh))
		{
		  uint64_t start, end;
		  varray blocks;
		  unsigned int i;

		  start = (args->offset / ZFS_MODIFIED_BLOCK_SIZE
			   * ZFS_MODIFIED_BLOCK_SIZE);
		  end = ((args->offset + args->data.len
			  + ZFS_MODIFIED_BLOCK_SIZE - 1)
			 / ZFS_MODIFIED_BLOCK_SIZE * ZFS_MODIFIED_BLOCK_SIZE);

		  interval_tree_intersection (dentry->fh->updated, start, end,
					      &blocks);

		  start = args->offset;
		  end = args->offset + args->data.len;
		  for (i = 0; i < VARRAY_USED (blocks); i++)
		    {
		      if (VARRAY_ACCESS (blocks, i, interval).end < start)
			continue;
		      if (VARRAY_ACCESS (blocks, i, interval).start > end)
			break;

		      /* Now the interval is joinable with [START, END).  */
		      if (VARRAY_ACCESS (blocks, i, interval).start < start)
			start = VARRAY_ACCESS (blocks, i, interval).start;
		      if (VARRAY_ACCESS (blocks, i, interval).end > end)
			end = VARRAY_ACCESS (blocks, i, interval).end;
		    }

		  if (!append_interval (vol, dentry->fh,
					METADATA_TYPE_UPDATED, start, end))
		    MARK_VOLUME_DELETE (vol);
		  if (!append_interval (vol, dentry->fh,
					METADATA_TYPE_MODIFIED, start, end))
		    MARK_VOLUME_DELETE (vol);

		  varray_destroy (&blocks);
		}
	    }
	}
    }

  internal_cap_unlock (vol, dentry, NULL);

  return r;
}

/* Read complete contents of local directory FH and store it to ENTRIES.  */

int32_t
full_local_readdir (zfs_fh *fh, filldir_htab_entries *entries)
{
  int32_t r, r2;
  zfs_cap cap;
  internal_dentry dentry;
  internal_cap icap;
  volume vol;
  dir_list list;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (!REGULAR_FH_P (*fh))
    abort ();
#endif

  cap.fh = *fh;
  cap.flags = O_RDONLY;

  /* Open directory.  */
  r2 = get_capability (&cap, &icap, &vol, &dentry, NULL, false, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  r = local_open (0, dentry, vol);

  r2 = find_capability_nolock (&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r != ZFS_OK)
    {
      put_capability (icap, dentry->fh, NULL);
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&fh_mutex);
      return r;
    }

  /* Read directory.  */
  entries->htab = htab_create (32, filldir_htab_hash,
			       filldir_htab_eq, filldir_htab_del, NULL);
  entries->last_cookie = 0;

  do
    {
      list.n = 0;
      list.eof = false;
      list.buffer = entries;
      r = local_readdir (&list, dentry, NULL, entries->last_cookie,
			 NULL, vol, &filldir_htab);
      zfsd_mutex_unlock (&vol->mutex);
      if (r != ZFS_OK)
	{
	  local_close (dentry->fh);
	  put_capability (icap, dentry->fh, NULL);
	  release_dentry (dentry);
	  return r;
	}
      release_dentry (dentry);

      r2 = find_capability_nolock (&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif
    }
  while (list.eof == 0);

  /* Close directory.  */
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = local_close (dentry->fh);
  put_capability (icap, dentry->fh, NULL);
  release_dentry (dentry);
  return r;
}

/* Read complete contents of remote directory FH and store it to ENTRIES.  */

int32_t
full_remote_readdir (zfs_fh *fh, filldir_htab_entries *entries)
{
  int32_t r, r2;
  zfs_cap cap;
  internal_dentry dentry;
  internal_cap icap;
  volume vol;
  dir_list list;
  readdir_data data;
  zfs_cap remote_cap;

  TRACE ("");
#ifdef ENABLE_CHECKING
  if (!REGULAR_FH_P (*fh))
    abort ();
#endif

  cap.fh = *fh;
  cap.flags = O_RDONLY;

  /* Open directory.  */
  r2 = get_capability (&cap, &icap, &vol, &dentry, NULL, true, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  r = remote_open (&remote_cap, icap, 0, dentry, vol);

  r2 = find_capability (&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r != ZFS_OK)
    {
      put_capability (icap, dentry->fh, NULL);
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return r;
    }
  icap->master_cap = remote_cap;

  /* Read directory.  */
  entries->htab = htab_create (32, filldir_htab_hash,
			       filldir_htab_eq, filldir_htab_del, NULL);
  entries->last_cookie = 0;

  do
    {
      list.n = 0;
      list.eof = false;
      list.buffer = entries;
      data.written = 0;
      data.count = ZFS_MAXDATA;
      r = remote_readdir (&list, icap, dentry, entries->last_cookie,
			  &data, vol, &filldir_htab);

      r2 = find_capability (&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
#endif

      if (r != ZFS_OK)
	{
	  int32_t r2;

	  remote_close (icap, dentry, vol);

	  r2 = find_capability (&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  put_capability (icap, dentry->fh, NULL);
	  release_dentry (dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	  return r;
	}
    }
  while (list.eof == 0);

  /* Close directory.  */
  r = remote_close (icap, dentry, vol);

  r2 = find_capability (&cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  put_capability (icap, dentry->fh, NULL);
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  return r;
}

/* Read as many bytes as possible of block of local file CAP starting at OFFSET
   which is COUNT bytes long, store the data to BUFFER and the number of bytes
   read to RCOUNT.  */

int32_t
full_local_read (uint32_t *rcount, void *buffer, zfs_cap *cap,
		 uint64_t offset, uint32_t count)
{
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  uint32_t n_read;
  uint32_t total;
  int32_t r;

  TRACE ("");

  for (total = 0; total < count; total += n_read)
    {
      r = find_capability (cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r != ZFS_OK)
	abort ();
#endif

#ifdef ENABLE_CHECKING
      if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh)
	    && vol->master != this_node))
	abort ();
#endif

      r = local_read (&n_read, (char *) buffer + total, dentry,
		      offset + total, count - total, vol);
      if (r != ZFS_OK)
	return r;

      if (n_read == 0)
	break;
    }

  *rcount = total;
  return ZFS_OK;
}

/* Read as many bytes as possible of block of local file DENTRY with capability
   CAP starting at OFFSET which is COUNT bytes long, store the data to BUFFER
   and the number of bytes read to RCOUNT.  */

int32_t
full_local_read_dentry (uint32_t *rcount, void *buffer, zfs_cap *cap,
			internal_dentry dentry, uint64_t offset, uint32_t count)
{
  volume vol;
  internal_cap icap;
  uint32_t n_read;
  uint32_t total;
  int32_t r, r2;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  for (total = 0; total < count; total += n_read)
    {
      r = local_read (&n_read, (char *) buffer + total, dentry,
		      offset + total, count - total, vol);

      r2 = find_capability (cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
      if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh)
	    && vol->master != this_node))
	abort ();
#endif

      if (r != ZFS_OK)
	return r;

      if (n_read == 0)
	break;
    }

  *rcount = total;
  return ZFS_OK;
}

/* Read as many bytes as possible of block of remote file CAP starting at OFFSET
   which is COUNT bytes long, store the data to BUFFER and the number of bytes
   read to RCOUNT.  */

int32_t
full_remote_read (uint32_t *rcount, void *buffer, zfs_cap *cap,
		  uint64_t offset, uint32_t count)
{
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  uint32_t n_read;
  uint32_t total;
  int32_t r;

  TRACE ("");

  for (total = 0; total < count; total += n_read)
    {
      r = find_capability (cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r != ZFS_OK)
	abort ();
#endif

#ifdef ENABLE_CHECKING
      if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh)
	    && vol->master != this_node))
	abort ();
#endif

      r = remote_read (&n_read, (char *) buffer + total, icap, dentry,
		       offset + total, count - total, vol);
      if (r != ZFS_OK)
	return r;

      if (n_read == 0)
	break;
    }

  *rcount = total;
  return ZFS_OK;
}

/* Write as many bytes as possible from BUFFER of length COUNT to local file
   CAP starting at OFFSET.  Store the number of bytes read to RCOUNT.  */

int32_t
full_local_write (uint32_t *rcount, void *buffer, zfs_cap *cap,
		  uint64_t offset, uint32_t count)
{
  volume vol;
  internal_cap icap;
  internal_dentry dentry;
  data_buffer data;
  write_res res;
  uint32_t total;
  int32_t r;

  for (total = 0; total < count; total += res.written)
    {
      r = find_capability_nolock (cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r != ZFS_OK)
	abort ();
#endif

#ifdef ENABLE_CHECKING
      if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh)
	    && vol->master != this_node))
	abort ();
#endif

      data.len = count - total;
      data.buf = (char *) buffer + total;
      r = local_write (&res, dentry, offset + total, &data, vol);
      if (r != ZFS_OK)
	return r;

      if (res.written == 0)
	break;
    }

  *rcount = total;
  return ZFS_OK;
}

/* Write as many bytes as possible from BUFFER of length COUNT to remote file
   DENTRY with capability CAP and ICAP starting at OFFSET.
   Store the number of bytes read to RCOUNT.  */

int32_t
full_remote_write_dentry (uint32_t *rcount, void *buffer, zfs_cap *cap,
			  internal_cap icap, internal_dentry dentry,
			  uint64_t offset, uint32_t count)
{
  volume vol;
  write_args args;
  write_res res;
  uint32_t total;
  int32_t r, r2;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  for (total = 0; total < count; total += res.written)
    {
      args.offset = offset + total;
      args.data.len = count - total;
      args.data.buf = (char *) buffer + total;
      r = remote_write (&res, icap, dentry, &args, vol);

      r2 = find_capability (cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
      if (r2 != ZFS_OK)
	abort ();
      if (!(INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh)
	    && vol->master != this_node))
	abort ();
#endif

      if (r != ZFS_OK)
	return r;

      if (res.written == 0)
	break;
    }

  *rcount = total;
  return ZFS_OK;
}

/* Compute MD5 sum for ARGS->COUNT ranges starting at ARGS->OFFSET[i] with
   length ARGS->LENGTH[i] of local file ARGS->CAP and store them (together
   with the information about ranges) to RES.  */

int32_t
local_md5sum (md5sum_res *res, md5sum_args *args)
{
  internal_dentry dentry;
  uint32_t i;
  MD5Context context;
  unsigned char buf[ZFS_MAXDATA];
  int32_t r;
  uint32_t total;
  uint32_t count;

  TRACE ("");

  zfsd_mutex_lock (&fh_mutex);
  dentry = dentry_lookup (&args->cap.fh);
  zfsd_mutex_unlock (&fh_mutex);
  if (!dentry)
    return ZFS_STALE;

  res->count = 0;
  res->size = dentry->fh->attr.size;
  release_dentry (dentry);

  for (i = 0; i < args->count; i++)
    {
      MD5Init (&context);
      for (total = 0; total < args->length[i]; total += count)
	{
	  r = zfs_read (&count, buf, &args->cap, args->offset[i] + total,
			args->length[i] - total, false);
	  if (r != ZFS_OK)
	    return r;

	  if (count == 0)
	    break;

	  MD5Update (&context, buf, count);
	}

      if (total > 0)
	{
	  res->offset[res->count] = args->offset[i];
	  res->length[res->count] = total;
	  MD5Final (res->md5sum[res->count], &context);
	  res->count++;
	}
    }

  return ZFS_OK;
}

/* Compute MD5 sum for ARGS->COUNT ranges starting at ARGS->OFFSET[i] with
   length ARGS->LENGTH[i] of remote file ARGS->CAP and store them (together
   with the information about ranges) to RES.  */

int32_t
remote_md5sum (md5sum_res *res, md5sum_args *args)
{
  volume vol;
  node nod;
  internal_cap icap;
  internal_dentry dentry;
  zfs_fh fh;
  thread *t;
  int32_t r;
  int fd;

  TRACE ("");

  r = find_capability (&args->cap, &icap, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r != ZFS_OK)
    abort ();
#endif

#ifdef ENABLE_CHECKING
  if (zfs_cap_undefined (icap->master_cap))
    abort ();
  if (zfs_fh_undefined (icap->master_cap.fh))
    abort ();
#endif

  if (dentry->fh->attr.type != FT_REG)
    {
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return EINVAL;
    }

  nod = vol->master;
  fh = args->cap.fh;
  args->cap = icap->master_cap;

  release_dentry (dentry);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);
  zfsd_mutex_unlock (&vol->mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_md5sum_client (t, args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_md5sum_res (t->dc_reply, res)
	  || !finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);

  return r;
}

/* Reread remote config file PATH (relative path WRT volume root).  */

void
remote_reread_config (string *path, node nod)
{
  reread_config_args args;
  thread *t;
  int32_t r;
  int fd;

  TRACE ("");
  CHECK_MUTEX_LOCKED (&nod->mutex);

  args.path = *path;

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_reread_config_client (t, &args, nod, &fd);

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
}

/* Initialize data structures in FILE.C.  */

void
initialize_file_c (void)
{
  int i;

  zfsd_mutex_init (&opened_mutex);
  opened = fibheap_new (max_local_fds, &opened_mutex);

  zfsd_mutex_init (&dir_entry_mutex);
  dir_entry_pool = create_alloc_pool ("dir_entry", sizeof (dir_entry), 1020,
				      &dir_entry_mutex);

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
cleanup_file_c (void)
{
  while (fibheap_size (opened) > 0)
    {
      internal_fd_data_t *fd_data;

      zfsd_mutex_lock (&opened_mutex);
      fd_data = (internal_fd_data_t *) fibheap_extract_min (opened);
#ifdef ENABLE_CHECKING
      if (!fd_data && fibheap_size (opened) > 0)
	abort ();
#endif
      if (fd_data)
	{
	  zfsd_mutex_lock (&fd_data->mutex);
	  fd_data->heap_node = NULL;
	  if (fd_data->fd >= 0)
	    close_local_fd (fd_data->fd);
	  else
	    zfsd_mutex_unlock (&fd_data->mutex);
	}
      zfsd_mutex_unlock (&opened_mutex);
    }

  zfsd_mutex_lock (&dir_entry_mutex);
#ifdef ENABLE_CHECKING
  if (dir_entry_pool->elts_free < dir_entry_pool->elts_allocated)
    message (2, stderr, "Memory leak (%u elements) in dir_entry_pool.\n",
	     dir_entry_pool->elts_allocated - dir_entry_pool->elts_free);
#endif
  free_alloc_pool (dir_entry_pool);
  zfsd_mutex_unlock (&dir_entry_mutex);
  zfsd_mutex_destroy (&dir_entry_mutex);

  zfsd_mutex_lock (&opened_mutex);
  fibheap_delete (opened);
  zfsd_mutex_unlock (&opened_mutex);
  zfsd_mutex_destroy (&opened_mutex);

  free (internal_fd_data);
}
