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
#include <utime.h>
#include <errno.h>
#include <time.h>
#include "pthread.h"
#include "constant.h"
#include "memory.h"
#include "fh.h"
#include "file.h"
#include "dir.h"
#include "cap.h"

/* The array of data for each file descriptor.  */
internal_fd_data_t *internal_fd_data;

/* Convert attributes from STRUCT STAT ST to FATTR ATTR for file on volume
   VOL.  */

void
fattr_from_struct_stat (fattr *attr, struct stat *st, volume vol)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);

  switch (st->st_mode & S_IFMT)
    {
      case S_IFSOCK:
	attr->type = FT_SOCK;
	break;

      case S_IFLNK:
	attr->type = FT_LNK;
	break;

      case S_IFREG:
	attr->type = FT_REG;
	break;

      case S_IFBLK:
	attr->type = FT_BLK;
	break;

      case S_IFDIR:
	attr->type = FT_DIR;
	break;

      case S_IFCHR:
	attr->type = FT_CHR;
	break;

      case S_IFIFO:
	attr->type = FT_FIFO;
	break;

      default:
	attr->type = FT_BAD;
	break;
    }

  attr->mode = st->st_mode & (S_IRWXU | S_ISUID | S_ISGID | S_ISVTX);
  attr->nlink = st->st_nlink;
  attr->uid = st->st_uid;  /* FIXME: translate UID */
  attr->gid = st->st_gid;  /* FIXME: translate GID */
  attr->rdev = st->st_rdev;
  attr->size = st->st_size;
  attr->blocks = st->st_blocks;
  attr->blksize = st->st_blksize;
  attr->generation = 0;	/* FIXME? how? */
  attr->fversion = 0;		/* FIXME */
  attr->sid = this_node->id;
  attr->vid = vol->id;
  attr->dev = st->st_dev;
  attr->ino = st->st_ino;
  attr->atime = st->st_atime;
  attr->mtime = st->st_mtime;
  attr->ctime = st->st_ctime;
}

/* Get attributes of local file PATH on volume VOL and store them to ATTR.  */

int
local_getattr (fattr *attr, char *path, volume vol)
{
  struct stat st;
  int r;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  r = lstat (path, &st);
  if (r != 0)
    return errno;

  fattr_from_struct_stat (attr, &st, vol);
  return ZFS_OK;
}

/* Get attributes for file with handle FH and store them to FA.  */

int
zfs_getattr (fattr *fa, zfs_fh *fh)
{
  volume vol;
  internal_fh ifh;
  virtual_dir vd;

  /* Lookup the FH.  */
  if (!fh_lookup (fh, &vol, &ifh, &vd))
    return ESTALE;

  /* TODO: Update file and fattr.  */

  if (vd)
    {
      *fa = vd->attr;
      zfsd_mutex_unlock (&vd->mutex);
    }
  else /* if (ifh) */
    {
      *fa = ifh->attr;
      zfsd_mutex_unlock (&ifh->mutex);
    }
  zfsd_mutex_unlock (&vol->mutex);

  return ZFS_OK;
}

/* Set attributes of local file fh on volume VOL according to SA,
   reget attributes and store them to FA.  */

static int
local_setattr (fattr *fa, internal_fh fh, sattr *sa, volume vol)
{
  char *path;
  int r;

  CHECK_MUTEX_LOCKED (&fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  path = build_local_path (vol, fh);
  if (sa->mode != (unsigned int) -1)
    {
      if (chmod (path, sa->mode) != 0)
	goto local_setattr_error;
    }

  if (sa->uid != (unsigned int) -1 || sa->gid != (unsigned int) -1)
    {
      if (lchown (path, sa->uid, sa->gid) != 0)
	goto local_setattr_error;
    }

  if (sa->size != (uint64_t) -1)
    {
      if (truncate (path, sa->size) != 0)
	goto local_setattr_error;
    }

  if (sa->atime != (zfs_time) -1 || sa->mtime != (zfs_time) -1)
    {
      struct utimbuf t;

      t.actime = sa->atime;
      t.modtime = sa->mtime;
      if (utime (path, &t) != 0)
	goto local_setattr_error;
    }

  r = local_getattr (fa, path, vol);
  free (path);
  return r;

local_setattr_error:
  free (path);
  return errno;
}

/* Set attributes of remote file fh on volume VOL according to SA,
   reget attributes and store them to FA.  */

static int
remote_setattr (fattr *fa, internal_fh fh, sattr *sa, volume vol)
{
  sattr_args args;
  thread *t;
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  args.file = fh->master_fh;
  args.attr = *sa;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_setattr_client (t, &args, vol->master);
  if (r == ZFS_OK)
    {
      if (!decode_fattr (&t->dc, fa)
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

/* Set attributes of file with handle FH according to SA, reget attributes
   and store them to FA.  */

int
zfs_setattr (fattr *fa, zfs_fh *fh, sattr *sa)
{
  volume vol;
  internal_fh ifh;
  int r = ZFS_OK;

  /* Virtual directory tree is read only for users.  */
  if (VIRTUAL_FH_P (*fh))
    return EROFS;

  /* Lookup the file.  */
  if (!fh_lookup (fh, &vol, &ifh, NULL))
    return ESTALE;

  if (vol->local_path)
    r = local_setattr (fa, ifh, sa, vol);
  else if (vol->master != this_node)
    r = remote_setattr (fa, ifh, sa, vol);
  else
    abort ();

  /* Update cached file attributes.  */
  if (r == ZFS_OK)
    ifh->attr = *fa;

  zfsd_mutex_unlock (&ifh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  return r;
}

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

/* Delete local file NAME from directory DIR on volume VOL.  */

static int
local_unlink (internal_fh dir, string *name, volume vol)
{
  char *path;
  int r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = unlink (path);
  free (path);
  if (r != 0)
    return errno;

  return ZFS_OK;
}

/* Delete remote file NAME from directory DIR on volume VOL.  */

static int
remote_unlink (internal_fh dir, string *name, volume vol)
{
  dir_op_args args;
  thread *t;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);

  args.dir = dir->master_fh;
  args.name = *name;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_unlink_client (t, &args, vol->master);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc))
	return ZFS_INVALID_REPLY;
    }

  return r;
}

/* Remove directory NAME from directory DIR.  */

int
zfs_unlink (zfs_fh *dir, string *name)
{
  volume vol;
  internal_fh idir;
  virtual_dir pvd;
  int r = ZFS_OK;

  /* Lookup the file.  */
  if (!fh_lookup (dir, &vol, &idir, &pvd))
    return ESTALE;

  if (pvd)
    {
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      if (r != ZFS_OK)
	return r;
    }

  if (vol->local_path)
    r = local_unlink (idir, name, vol);
  else if (vol->master != this_node)
    r = remote_unlink (idir, name, vol);
  else
    abort ();

  /* Delete the internal file handle of the deleted directory.  */
  if (r == ZFS_OK)
    {
      internal_fh ifh;

      ifh = fh_lookup_name (vol, idir, name->str);
      if (ifh)
	internal_fh_destroy (ifh, vol);
    }

  zfsd_mutex_unlock (&idir->mutex);
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

#if 0
static void
really_local_open (internal_fh fh, int flags, mode_t mode)
{
}

#include <errno.h>
/* Open internal file handle FH.  */
int
local_open (internal_fh fh, int flags, mode_t mode)
{
  int fd;
  char *path;

  if (fh->fd >= 0)
    {
      pthread_mutex_lock (&internal_fd_data[fh->fd].mutex);
      if (fh->generation == internal_fd_data[fh->fd].generation)
	{
	  internal_fd_data[fh->fd].busy++;
	  pthread_mutex_unlock (&internal_fd_data[fh->fd].mutex);
	  return ZFS_OK;
	}
      pthread_mutex_unlock (&internal_fd_data[fh->fd].mutex);
    }

/*  fd = open (*/
}

int
local_read (internal_fh fh, void *buf, uint64_t pos, unsigned int count)
{
}

int
local_close (internal_fh fh)
{
  int r = ZFS_OK;

  if (fh->fd < 0)
    return EBADF;

  pthread_mutex_lock (&internal_fd_data[fh->fd].mutex);
  if (fh->generation == internal_fd_data[fh->fd].generation)
    {
      internal_fd_data[fh->fd].busy--;
      if (internal_fd_data[fh->fd].busy == 0)
	{
	  internal_fd_data[fh->fd].fd = -1;
	  internal_fd_data[fh->fd].generation++;
	  do
	    {
	      r = close (fh->fd);
	    }
	  while (r < 0 && errno == EINTR);
	  if (r < 0)
	    r = errno;
	}
    }

  pthread_mutex_unlock (&internal_fd_data[fh->fd].mutex);
  return r;
}
#endif

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
