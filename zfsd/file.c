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
#include "data-coding.h"
#include "fh.h"
#include "file.h"
#include "dir.h"
#include "cap.h"
#include "volume.h"

/* int getdents(unsigned int fd, struct dirent *dirp, unsigned int count); */
_syscall3(int, getdents, uint, fd, struct dirent *, dirp, uint, count)

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
    r = close_capability (icap, vol);
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
read_local_dir (DC *dc, internal_cap cap, internal_fh fh, virtual_dir vd,
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
read_remote_dir (DC *dc, internal_cap cap, readdir_data *data, volume vol)
{
  read_dir_args args;
  thread *t;
  int32_t r;

  /* FIXME: if we can;t connect but we are in virtual directory, list the
     virtual directory. */
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
      if (t->dc.max_length > dc->cur_length)
	{
	  memcpy (dc->current, t->dc.current,
		  t->dc.max_length - t->dc.cur_length);
	  dc->current += t->dc.max_length - t->dc.cur_length;
	  dc->cur_length += t->dc.max_length - t->dc.cur_length;
	}
      else
	r = ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc))
	return ZFS_INVALID_REPLY;
    }

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

  zfsd_mutex_lock (&cap_mutex);
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (cap->fh))
    zfsd_mutex_lock (&vd_mutex);
  r = find_capability_nolock (cap, &icap, &vol, &ifh, &vd);
  zfsd_mutex_unlock (&volume_mutex);
  zfsd_mutex_unlock (&cap_mutex);
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
      r = read_local_dir (dc, icap, ifh, vd, &data, vol);
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
      r = read_remote_dir (dc, icap, &data, vol);
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
      memcpy (dc->current, t->dc.current,
	      t->dc.max_length - t->dc.cur_length);
      dc->current += t->dc.max_length - t->dc.cur_length;
      dc->cur_length += t->dc.max_length - t->dc.cur_length;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc))
	r = ZFS_INVALID_REPLY;
    }

  if (r != ZFS_OK)
    encode_status (dc, r);

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
      if (decode_write_res (&t->dc, res)
	  || !finish_decoding (&t->dc))
	return ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc))
	return ZFS_INVALID_REPLY;
    }

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
