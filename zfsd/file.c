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
#include <unistd.h>
#include <inttypes.h>
#include <linux/types.h>
#include <linux/dirent.h>
#include <linux/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "pthread.h"
#include "constant.h"
#include "memory.h"
#include "fibheap.h"
#include "data-coding.h"
#include "fh.h"
#include "file.h"
#include "dir.h"
#include "cap.h"
#include "volume.h"
#include "network.h"

/* int getdents(unsigned int fd, struct dirent *dirp, unsigned int count); */
_syscall3(int, getdents, uint, fd, struct dirent *, dirp, uint, count)

/* The array of data for each file descriptor.  */
internal_fd_data_t *internal_fd_data;

/* Array of opened file descriptors.  */
static fibheap opened;

/* Mutex protecting access to OPENED and NOPENED.  */
static pthread_mutex_t opened_mutex;

/* Initialize data for file descriptor of capability CAP.  */

static void
init_cap_fd_data (internal_cap cap)
{
#ifdef ENABLE_CHECKING
  if (cap->fd < 0)
    abort ();
#endif
  CHECK_MUTEX_LOCKED (&opened_mutex);
  CHECK_MUTEX_LOCKED (&internal_fd_data[cap->fd].mutex);

  internal_fd_data[cap->fd].fd = cap->fd;
  internal_fd_data[cap->fd].generation++;
  cap->generation = internal_fd_data[cap->fd].generation;
  internal_fd_data[cap->fd].heap_node
    = fibheap_insert (opened, (fibheapkey_t) time (NULL),
		      &internal_fd_data[cap->fd]);
}

/* Close file descriptor FD of local file.  */

static void
close_local_fd (int fd)
{
  CHECK_MUTEX_LOCKED (&opened_mutex);
#ifdef ENABLE_CHECKING
  if (fd < 0)
    abort ();
#endif

  zfsd_mutex_lock (&internal_fd_data[fd].mutex);
  internal_fd_data[fd].fd = -1;
  internal_fd_data[fd].generation++;
  close (fd);
  fibheap_delete_node (opened, internal_fd_data[fd].heap_node);
  internal_fd_data[fd].heap_node = NULL;
  zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
}

/* Wrapper for open. If open fails because of too many open file descriptors
   it closes a file descriptor unused for longest time.  */

static int
safe_open (const char *pathname, unsigned int flags, unsigned int mode)
{
  int fd;

retry_open:
  fd = open (pathname, flags, mode);
  if ((fd < 0 && errno == EMFILE)
      || (fd >= 0 && fibheap_size (opened) >= max_local_fds))
    {
      internal_fd_data_t *fd_data;

      zfsd_mutex_lock (&opened_mutex);
      fd_data = (internal_fd_data_t *) fibheap_extract_min (opened);
      if (fd_data && fd_data->fd >= 0)
	close_local_fd (fd_data->fd);
      zfsd_mutex_unlock (&opened_mutex);
      if (fd_data)
	goto retry_open;
    }

  if (fd < 0)
    return errno;

  return fd;
}

/* If local file for capability CAP is opened return true and lock
   INTERNAL_FD_DATA[CAP->FD].MUTEX.  */

bool
capability_opened_p (internal_cap cap)
{
  CHECK_MUTEX_LOCKED (&cap->mutex);

  if (cap->fd < 0)
    return false;

  zfsd_mutex_lock (&opened_mutex);
  zfsd_mutex_lock (&internal_fd_data[cap->fd].mutex);
  if (cap->generation != internal_fd_data[cap->fd].generation)
    {
      zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
      zfsd_mutex_unlock (&opened_mutex);
      return false;
    }

  fibheap_replace_key (opened, internal_fd_data[cap->fd].heap_node,
		       (fibheapkey_t) time (NULL));
  zfsd_mutex_unlock (&opened_mutex);
  return true;
}

/* Close local file for internal capability CAP on volume VOL.  */

int
local_close (internal_cap cap)
{
  CHECK_MUTEX_LOCKED (&cap->mutex);

  if (cap->fd >= 0)
    {
      zfsd_mutex_lock (&opened_mutex);
      close_local_fd (cap->fd);
      zfsd_mutex_unlock (&opened_mutex);
      cap->fd = -1;
    }

  return ZFS_OK;
}

/* Close remote file for internal capability CAP on volume VOL.  */

static int
remote_close (internal_cap cap, volume vol)
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
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
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
  local_close (cap);

  path = build_local_path (vol, fh);
  cap->fd = open (path, cap->local_cap.flags | flags);
  free (path);
  if (cap->fd >= 0)
    {
      zfsd_mutex_lock (&opened_mutex);
      zfsd_mutex_lock (&internal_fd_data[cap->fd].mutex);
      init_cap_fd_data (cap);
      zfsd_mutex_unlock (&opened_mutex);
      return ZFS_OK;
    }

  return errno;
}

/* Create local file NAME in directory DIR with open flags FLAGS,
   set file attributes according to ATTR.  Store the newly opened file
   descriptor to FDP.  */

int
local_create (create_res *res, int *fdp, internal_fh dir, string *name,
	      unsigned int flags, sattr *attr, volume vol)
{
  char *path;
  int r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = safe_open (path, flags, attr->mode);
  if (r < 0)
    {
      free (path);
      return errno;
    }
  *fdp = r;

  attr->mode = (unsigned int) -1;
  r = local_setattr_path (&res->attr, path, attr, vol);
  free (path);
  if (r != ZFS_OK)
    {
      close (*fdp);
      return r;
    }

  res->file.sid = dir->local_fh.sid;
  res->file.vid = dir->local_fh.vid;
  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;
  res->cap.fh = res->file;
  res->cap.flags = flags & O_ACCMODE;

  return ZFS_OK;
}

/* Create remote file NAME in directory DIR with open flags FLAGS,
   set file attributes according to ATTR.  */

int
remote_create (create_res *res, internal_fh dir, string *name,
	      unsigned int flags, sattr *attr, volume vol)
{
  create_args args;
  thread *t;
  int32_t r;

  args.where.dir = dir->master_fh;
  args.where.name = *name;
  args.flags = flags;
  args.attr = *attr;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_create_client (t, &args, vol->master);

  if (r == ZFS_OK)
    {
      if (!decode_create_res (&t->dc_reply, res)
	  || !finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }
  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Create file NAME in directory DIR with open flags FLAGS,
   set file attributes according to ATTR.  */

int
zfs_create (create_res *res, zfs_fh *dir, string *name,
	    unsigned int flags, sattr *attr)
{
  volume vol;
  internal_fh idir;
  virtual_dir pvd;
  int r;
  int fd;

  /* When O_CREAT is NOT set the function zfs_open is called.
     Force O_CREAT to be set here.  */
  flags |= O_CREAT;

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  if (!zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd))
    {
      zfsd_mutex_unlock (&volume_mutex);
      if (VIRTUAL_FH_P (*dir))
	zfsd_mutex_unlock (&vd_mutex);
      return ESTALE;
    }
  zfsd_mutex_unlock (&volume_mutex);

  if (pvd)
    {
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }

  attr->size = (uint64_t) -1;
  attr->atime = (zfs_time) -1;
  attr->mtime = (zfs_time) -1;

  if (vol->local_path)
    r = local_create (res, &fd, idir, name, flags, attr, vol);
  else if (vol->master != this_node)
    r = remote_create (res, idir, name, flags, attr, vol);
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_cap icap;
      internal_fh ifh;

      /* Update internal file handle and capability.  */
      ifh = fh_lookup_name (vol, idir, name->str);
      if (ifh)
	{
	  CHECK_MUTEX_LOCKED (&ifh->mutex);

	  internal_fh_destroy (ifh, vol);
	  ifh = internal_fh_create (&res->file, &res->file, idir,
				    vol, name->str, &res->attr);
	}
      else
	ifh = internal_fh_create (&res->file, &res->file, idir,
				  vol, name->str, &res->attr);
      icap = get_capability_no_zfs_fh_lookup (&res->cap, ifh);

      if (vol->local_path)
	{
	  local_close (icap);
	  icap->fd = fd;

	  zfsd_mutex_lock (&opened_mutex);
	  zfsd_mutex_lock (&internal_fd_data[fd].mutex);
	  init_cap_fd_data (icap);
	  zfsd_mutex_unlock (&internal_fd_data[fd].mutex);
	  zfsd_mutex_unlock (&opened_mutex);
	}

      zfsd_mutex_unlock (&ifh->mutex);
      zfsd_mutex_unlock (&icap->mutex);
    }

  zfsd_mutex_unlock (&idir->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  return r;
}

/* Open local file for capability ICAP (whose internal file handle is FH)
   with open flags FLAGS on volume VOL.  Store ZFS capability to CAP.  */

static int
local_open (zfs_cap *cap, internal_cap icap, unsigned int flags,
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
remote_open (zfs_cap *cap, internal_cap icap, unsigned int flags, volume vol)
{
  open_args args;
  thread *t;
  int32_t r;

  args.file = icap->master_cap.fh;
  args.flags = icap->master_cap.flags | flags;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_open_client (t, &args, vol->master);

  if (r == ZFS_OK)
    {
      if (!decode_zfs_cap (&t->dc_reply, cap)
	  || !finish_decoding (&t->dc_reply))
	{
	  recycle_dc_to_volume_master (&t->dc_reply, vol);
	  return ZFS_INVALID_REPLY;
	}

      icap->master_cap = *cap;
    }
  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Open file handle FH with open flags FLAGS and return capability in CAP.  */

int
zfs_open (zfs_cap *cap, zfs_fh *fh, unsigned int flags)
{
  volume vol;
  internal_cap icap;
  internal_fh ifh;
  virtual_dir vd;
  int r;

  /* When O_CREAT is set the function zfs_create is called.
     The flag is superfluous here.  */
  flags &= ~O_CREAT;

  cap->fh = *fh;
  cap->flags = flags & O_ACCMODE;
  r = get_capability (cap, &icap, &vol, &ifh, &vd);
  if (r != ZFS_OK)
    return r;

  if (!ifh)
    {
      zfsd_mutex_unlock (&icap->mutex);
      if (vol)
	zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&vd->mutex);
      return ZFS_OK;
    }
  if (vd)
    zfsd_mutex_unlock (&vd->mutex);

  if (vol->local_path)
    r = local_open (cap, icap, flags & ~O_ACCMODE, ifh, vol);
  else if (vol->master != this_node)
    r = remote_open (cap, icap, flags & ~O_ACCMODE, vol);
  else
    abort ();

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

  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (cap->fh))
    zfsd_mutex_lock (&vd_mutex);
  zfsd_mutex_lock (&cap_mutex);
  r = find_capability_nolock (cap, &icap, &vol, &ifh, &vd);
  zfsd_mutex_unlock (&volume_mutex);
  if (VIRTUAL_FH_P (cap->fh))
    zfsd_mutex_unlock (&vd_mutex);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&cap_mutex);
      return r;
    }

  if (!ifh)
    {
      put_capability (icap);
      zfsd_mutex_unlock (&cap_mutex);
      if (vol)
	zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&vd->mutex);
      return ZFS_OK;
    }
  else
    zfsd_mutex_unlock (&ifh->mutex);
  if (vd)
    zfsd_mutex_unlock (&vd->mutex);

  if (icap->busy == 1)
    {
      if (vol->local_path)
	r = local_close (icap);
      else if (vol->master != this_node)
	r = remote_close (icap, vol);
      else
	abort ();
    }
  else
    r = ZFS_OK;

  put_capability (icap);
  zfsd_mutex_unlock (&cap_mutex);
  zfsd_mutex_unlock (&vol->mutex);
  return r;
}

/* Data for supplementary functions for readdir.  */

typedef struct readdir_data_def
{
  unsigned int written;
  unsigned int count;
  int cookie;
  dir_list list;
} readdir_data;

/* Add one directory entry to DC.  */

static bool
filldir (DC *dc, unsigned int ino, char *name, unsigned int name_len,
	 readdir_data *data)
{
  char *old_pos = dc->current;
  unsigned int old_len = dc->cur_length;
  dir_entry entry;

#ifdef ENABLE_CHECKING
  if (name[0] == 0)
    abort ();
#endif

  entry.ino = ino;
  entry.cookie = data->cookie;
  entry.name.str = name;
  entry.name.len = name_len;

  /* Try to encode ENTRY to DC.  */
  if (!encode_dir_entry (dc, &entry)
      || data->written + dc->cur_length - old_len > data->count)
    {
      /* There is not enough space in DC to encode ENTRY.  */
      dc->current = old_pos;
      dc->cur_length = old_len;
      return false;
    }
  else
    {
      data->list.n++;
      data->written += dc->cur_length - old_len;
    }
  return true;
}

/* Read COUNT bytes from virtual directory CAP starting at position COOKIE.  */

static bool
read_virtual_dir (DC *dc, virtual_dir vd, readdir_data *data)
{
  unsigned int ino;
  unsigned int i;

  CHECK_MUTEX_LOCKED (&vd->mutex);
#ifdef ENABLE_CHECKING
  if (data->cookie > 0)
    abort ();
#endif

  switch (data->cookie)
    {
      case 0:
	data->cookie--;
	if (!filldir (dc, vd->fh.ino, ".", 1, data))
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

	data->cookie--;
	if (!filldir (dc, vd->fh.ino, "..", 2, data))
	  return false;
	/* Fallthru.  */

      default:
	for (i = -data->cookie - 2; i < VARRAY_USED (vd->subdirs); i++)
	  {
	    virtual_dir svd;

	    svd = VARRAY_ACCESS (vd->subdirs, i, virtual_dir);
	    zfsd_mutex_lock (&svd->mutex);
	    data->cookie--;
	    if (!filldir (dc, svd->fh.ino, svd->name, strlen (svd->name),
			  data))
	      {
		zfsd_mutex_unlock (&svd->mutex);
		return false;
	      }
	    zfsd_mutex_unlock (&svd->mutex);

	  }
	if (i == VARRAY_USED (vd->subdirs))
	  data->list.eof = 1;
	break;
    }

  return true;
}

/* Read COUNT bytes from local directory CAP starting at position COOKIE.  */

static int
local_readdir (DC *dc, internal_cap cap, internal_fh fh, virtual_dir vd,
	       readdir_data *data, volume vol)
{
  char buf[ZFS_MAXDATA];
  int r, pos;
  struct dirent *de;

  if (vd)
    {
      if (!read_virtual_dir (dc, vd, data))
	{
	  return (data->list.n == 0) ? EINVAL : ZFS_OK;
	}
    }

  if (fh)
    {
      if (!capability_opened_p (cap))
	{
	  r = capability_open (cap, 0, fh, vol);
	  if (r != ZFS_OK)
	    return r;
	}

      if (data->cookie < 0)
	data->cookie = 0;
      r = lseek (cap->fd, data->cookie, SEEK_SET);
      if (r < 0)
	{
	  zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
	  return errno;
	}

      r = getdents (cap->fd, (struct dirent *) buf,
		    data->count - data->written);
      if (r <= 0)
	{
	  zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);

	  /* Comment from glibc: On some systems getdents fails with ENOENT when
	     open directory has been rmdir'd already.  POSIX.1 requires that we
	     treat this condition like normal EOF.  */
	  if (r < 0 && errno == ENOENT)
	    r = 0;

	  if (r == 0)
	    {
	      data->list.eof = 1;
	      return ZFS_OK;
	    }

	  /* EINVAL means that (DATA.COUNT - DATA.WRITTEN) was too low.  */
	  return (errno == EINVAL) ? ZFS_OK : errno;
	}

      for (pos = 0; pos < r; pos += de->d_reclen)
	{
	  de = (struct dirent *) &buf[pos];
	  data->cookie = de->d_off;
	  if (vd)
	    {
	      virtual_dir svd;

	      if (de->d_name[0] == '.'
		  && (de->d_name[1] == 0
		      || (de->d_name[1] == '.'
			  && de->d_name[2] == 0)))
		continue;

	      svd = vd_lookup_name (vd, de->d_name);
	      if (svd)
		{
		  zfsd_mutex_unlock (&svd->mutex);
		  continue;
		}
	    }
	  if (!filldir (dc, de->d_ino, de->d_name, strlen (de->d_name), data))
	    break;
	}
      zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
    }

  return ZFS_OK;
}

/* Read COUNT bytes from remote directory CAP starting at position COOKIE.  */

static int
remote_readdir (DC *dc, internal_cap cap, readdir_data *data, volume vol)
{
  read_dir_args args;
  thread *t;
  int32_t r;

  args.cap = cap->master_cap;
  args.cookie = data->cookie;
  args.count = data->count;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_readdir_client (t, &args, vol->master);

  if (r == ZFS_OK)
    {
      if (t->dc_reply.max_length > dc->cur_length)
	{
	  memcpy (dc->current, t->dc_reply.current,
		  t->dc_reply.max_length - t->dc_reply.cur_length);
	  dc->current += t->dc_reply.max_length - t->dc_reply.cur_length;
	  dc->cur_length += t->dc_reply.max_length - t->dc_reply.cur_length;
	}
      else
	r = ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Read COUNT bytes from directory CAP starting at position COOKIE.  */

int
zfs_readdir (DC *dc, zfs_cap *cap, int cookie, unsigned int count)
{
  volume vol;
  internal_cap icap;
  internal_fh ifh;
  virtual_dir vd;
  readdir_data data;
  int r;
  char *status_pos, *cur_pos;
  int status_len, cur_len;

  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (cap->fh))
    zfsd_mutex_lock (&vd_mutex);
  zfsd_mutex_lock (&cap_mutex);
  r = find_capability_nolock (cap, &icap, &vol, &ifh, &vd);
  zfsd_mutex_unlock (&cap_mutex);
  zfsd_mutex_unlock (&volume_mutex);
  if (r == ZFS_OK)
    {
      if (ifh && ifh->attr.type != FT_DIR)
	{
	  if (vd)
	    {
	      zfsd_mutex_unlock (&vd->mutex);
	      zfsd_mutex_unlock (&vd_mutex);
	    }
	  zfsd_mutex_unlock (&ifh->mutex);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&icap->mutex);
	  encode_status (dc, ENOTDIR);
	  return ENOTDIR;
	}
    }
  if (r != ZFS_OK)
    {
      if (VIRTUAL_FH_P (cap->fh))
	zfsd_mutex_unlock (&vd_mutex);
      encode_status (dc, r);
      return r;
    }

  data.written = 0;
  data.count = (count > ZFS_MAXDATA) ? ZFS_MAXDATA : count;
  data.cookie = cookie;
  data.list.n = 0;
  data.list.eof = 0;

  status_pos = dc->current;
  status_len = dc->cur_length;
  encode_status (dc, ZFS_OK);

  if (!ifh || vol->local_path)
    {
      encode_dir_list (dc, &data.list);
      r = local_readdir (dc, icap, ifh, vd, &data, vol);
      if (vd)
	{
	  zfsd_mutex_unlock (&vd->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	}
      if (ifh)
	zfsd_mutex_unlock (&ifh->mutex);
      if (vol)
	zfsd_mutex_unlock (&vol->mutex);
    }
  else if (vol->master != this_node)
    {
      r = remote_readdir (dc, icap, &data, vol);
      if (vd)
	{
	  zfsd_mutex_unlock (&vd->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	}
      if (ifh)
	zfsd_mutex_unlock (&ifh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
    }
  else
    abort ();
  zfsd_mutex_unlock (&icap->mutex);

  cur_pos = dc->current;
  cur_len = dc->cur_length;
  dc->current = status_pos;
  dc->cur_length = status_len;
  encode_status (dc, r);
  if (r == ZFS_OK)
    {
      if (!ifh || vol->local_path)
	encode_dir_list (dc, &data.list);
      dc->current = cur_pos;
      dc->cur_length = cur_len;
    }

  return r;
}

/* Read COUNT bytes from offset OFFSET of local file with capability CAP
   and file handle FH on volume VOL.
   Store data to DC.  */

static int
local_read (DC *dc, internal_cap cap, internal_fh fh, uint64_t offset,
	    unsigned int count, volume vol)
{
  data_buffer buf;
  int r;

  if (!capability_opened_p (cap))
    {
      r = capability_open (cap, 0, fh, vol);
      if (r != ZFS_OK)
	return r;
    }

  r = lseek (cap->fd, offset, SEEK_SET);
  if (r < 0)
    {
      zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
      return errno;
    }

  r = read (cap->fd, buf.buf, count);
  if (r < 0)
    {
      zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
      return errno;
    }

  buf.len = r;
  encode_status (dc, ZFS_OK);
  encode_data_buffer (dc, &buf);

  zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
  return ZFS_OK;
}

/* Read COUNT bytes from offset OFFSET of remote file with capability CAP
   on volume VOL.
   Store data to DC.  */

static int
remote_read (DC *dc, internal_cap cap, uint64_t offset,
	     unsigned int count, volume vol)
{
  read_args args;
  thread *t;
  int32_t r;

  args.cap = cap->master_cap;
  args.offset = offset;
  args.count = count;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_read_client (t, &args, vol->master);

  if (r == ZFS_OK)
    {
      encode_status (dc, ZFS_OK);
      memcpy (dc->current, t->dc_reply.current,
	      t->dc_reply.max_length - t->dc_reply.cur_length);
      dc->current += t->dc_reply.max_length - t->dc_reply.cur_length;
      dc->cur_length += t->dc_reply.max_length - t->dc_reply.cur_length;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r != ZFS_OK)
    encode_status (dc, r);

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Read COUNT bytes from file CAP at offset OFFSET.  */

int
zfs_read (DC *dc, zfs_cap *cap, uint64_t offset, unsigned int count)
{
  volume vol;
  internal_cap icap;
  internal_fh ifh;
  int r;

  if (count > ZFS_MAXDATA)
    {
      encode_status (dc, EINVAL);
      return EINVAL;
    }

  if (VIRTUAL_FH_P (cap->fh))
    {
      encode_status (dc, EISDIR);
      return EISDIR;
    }

  r = find_capability (cap, &icap, &vol, &ifh, NULL);
  if (r != ZFS_OK)
    {
      encode_status (dc, r);
      return r;
    }

  if (ifh->attr.type == FT_DIR)
    {
      zfsd_mutex_unlock (&ifh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&icap->mutex);
      encode_status (dc, EISDIR);
      return EISDIR;
    }

  if (vol->local_path)
    r = local_read (dc, icap, ifh, offset, count, vol);
  else if (vol->master != this_node)
    r = remote_read (dc, icap, offset, count, vol);
  else
    abort ();
  zfsd_mutex_unlock (&ifh->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&icap->mutex);

  return r;
}

/* Write DATA to offset OFFSET of local file with capability CAP
   and file handle FH on volume VOL.  */

static int
local_write (write_res *res, internal_cap cap, internal_fh fh, uint64_t offset,
	      data_buffer *data, volume vol)
{
  int r;

  if (!capability_opened_p (cap))
    {
      r = capability_open (cap, 0, fh, vol);
      if (r != ZFS_OK)
	return r;
    }

  r = lseek (cap->fd, offset, SEEK_SET);
  if (r < 0)
    {
      zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
      return errno;
    }

  r = write (cap->fd, data->buf, data->len);
  if (r < 0)
    {
      zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
      return errno;
    }
  res->written = r;

  zfsd_mutex_unlock (&internal_fd_data[cap->fd].mutex);
  return ZFS_OK;
}

/* Write to remote file with capability CAP on volume VOL.  */

static int
remote_write (write_res *res, internal_cap cap, write_args *args, volume vol)
{
  thread *t;
  int32_t r;

  args->cap = cap->master_cap;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_write_client (t, args, vol->master);

  if (r == ZFS_OK)
    {
      if (!decode_write_res (&t->dc_reply, res)
	  || !finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Write to file.  */

int
zfs_write (write_res *res, write_args *args)
{
  volume vol;
  internal_cap icap;
  internal_fh ifh;
  int r;

  if (args->data.len > ZFS_MAXDATA)
    return EINVAL;

  if (VIRTUAL_FH_P (args->cap.fh))
    return EISDIR;

  r = find_capability (&args->cap, &icap, &vol, &ifh, NULL);
  if (r != ZFS_OK)
    return r;

#ifdef ENABLE_CHECKING
  /* We did not allow directory to be opened for writing so there should be
     no capability for writing to directory.  */
  if (ifh->attr.type == FT_DIR)
    abort ();
#endif

  if (vol->local_path)
    r = local_write (res, icap, ifh, args->offset, &args->data, vol);
  else if (vol->master != this_node)
    r = remote_write (res, icap, args, vol);
  else
    abort ();
  zfsd_mutex_unlock (&ifh->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&icap->mutex);

  return r;
}

/* Initialize data structures in FILE.C.  */

void
initialize_file_c ()
{
  int i;

  zfsd_mutex_init (&opened_mutex);
  opened = fibheap_new (max_local_fds, &opened_mutex);

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
  while (fibheap_size (opened) > 0)
    {
      internal_fd_data_t *fd_data;

      zfsd_mutex_lock (&opened_mutex);
      fd_data = (internal_fd_data_t *) fibheap_extract_min (opened);
      if (fd_data && fd_data->fd >= 0)
	close_local_fd (fd_data->fd);
      zfsd_mutex_unlock (&opened_mutex);
    }
  zfsd_mutex_lock (&opened_mutex);
  fibheap_delete (opened);
  zfsd_mutex_unlock (&opened_mutex);
  zfsd_mutex_destroy (&opened_mutex);
  free (internal_fd_data);
}
