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
#include "fh.h"
#include "file.h"
#include "dir.h"

void
fattr_from_struct_stat (fattr *attr, struct stat *st, volume vol)
{
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
    *fa = vd->attr;
  else /* if (ifh) */
    *fa = ifh->attr;

  return ZFS_OK;
}

/* Set attributes of local file fh according to SA, reget attributes
   (use volume VOL) and store them to FA.  */

static int
local_setattr (fattr *fa, internal_fh fh, sattr *sa, volume vol)
{
  char *path;
  int r;

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

/* Set attributes of remote file fh according to SA, reget attributes
   and store them to FA.  */

static int
remote_setattr (fattr *fa, internal_fh fh, sattr *sa)
{
  sattr_args args;
  thread *t;
  node nod;
  int32_t r;

  args.file = fh->master_fh;
  args.attr = *sa;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  nod = node_lookup (fh->master_fh.sid);
  zfsd_mutex_unlock (&node_mutex);
  if (!nod)
    return ENOENT;

  r = zfs_proc_setattr_client (t, &args, nod);
  zfsd_mutex_unlock (&nod->mutex);
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
    r = remote_setattr (fa, ifh, sa);
  else
    abort ();

  /* Update cached file attributes.  */
  if (r == ZFS_OK)
    ifh->attr = *fa;

  return r;
}

/*****************************************************************************/

int
zfs_open (zfs_fh *fh)
{

  return ZFS_OK;
}

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

int
zfs_close (zfs_fh *fh)
{
  return ZFS_OK;
}
