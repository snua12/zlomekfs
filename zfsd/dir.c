/* Directory operations.
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
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>
#include "pthread.h"
#include "fh.h"
#include "dir.h"
#include "file.h"
#include "log.h"
#include "memory.h"
#include "thread.h"
#include "varray.h"
#include "volume.h"
#include "network.h"
#include "zfs_prot.h"
#include "user-group.h"

/* Return the local path of file for dentry DENTRY on volume VOL.  */

char *
build_local_path (volume vol, internal_dentry dentry)
{
  internal_dentry tmp;
  unsigned int n;
  varray v;
  char *r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  /* Count the number of strings which will be concatenated.  */
  n = 1;
  for (tmp = dentry; tmp->parent; tmp = tmp->parent)
    n += 2;

  varray_create (&v, sizeof (char *), n);
  VARRAY_USED (v) = n;
  for (tmp = dentry; tmp->parent; tmp = tmp->parent)
    {
      n--;
      VARRAY_ACCESS (v, n, char *) = tmp->name;
      n--;
      VARRAY_ACCESS (v, n, char *) = "/";
    }
  VARRAY_ACCESS (v, 0, char *) = vol->local_path;

  r = xstrconcat_varray (&v);
  varray_destroy (&v);

  return r;
}

/* Return the local path of file NAME in directory DENTRY on volume VOL.  */

char *
build_local_path_name (volume vol, internal_dentry dentry, const char *name)
{
  internal_dentry tmp;
  unsigned int n;
  varray v;
  char *r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  /* Count the number of strings which will be concatenated.  */
  n = 3;
  for (tmp = dentry; tmp->parent; tmp = tmp->parent)
    n += 2;

  varray_create (&v, sizeof (char *), n);
  VARRAY_USED (v) = n;
  n--;
  VARRAY_ACCESS (v, n, char *) = (char *) name;
  n--;
  VARRAY_ACCESS (v, n, char *) = "/";
  for (tmp = dentry; tmp->parent; tmp = tmp->parent)
    {
      n--;
      VARRAY_ACCESS (v, n, char *) = tmp->name;
      n--;
      VARRAY_ACCESS (v, n, char *) = "/";
    }
  VARRAY_ACCESS (v, 0, char *) = vol->local_path;

  r = xstrconcat_varray (&v);
  varray_destroy (&v);

  return r;
}

/* Recursively unlink the file PATH on volume VOL.  */

bool
recursive_unlink (const char *path, volume vol)
{
  internal_dentry dentry;
  zfs_fh fh;
  struct stat st;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (lstat (path, &st) != 0)
    return errno == ENOENT;

  if ((st.st_mode & S_IFMT) != S_IFDIR)
    {
      if (unlink (path) != 0)
	return errno == ENOENT;
    }
  else
    {
      DIR *d;
      struct dirent *de;

      d = opendir (path);
      if (!d)
	return errno == ENOENT;

      while ((de = readdir (d)) != NULL)
	{
	  char *new_path;

	  new_path = xstrconcat (3, path, "/", de->d_name);
	  recursive_unlink (new_path, vol);
	  free (new_path);
	}
      closedir (d);

      if (rmdir (path) != 0)
	return errno == ENOENT;
    }

  /* Destroy dentry associated with the file.  */
  fh.sid = this_node->id;
  fh.vid = vol->id;
  fh.dev = st.st_dev;
  fh.ino = st.st_ino;

  dentry = dentry_lookup (vol, &fh);
  if (dentry)
    {
      zfsd_mutex_lock (&dentry->fh->mutex);
      internal_dentry_destroy (dentry, vol);
    }

  return true;
}

/* Check whether we can perform file system change operation on NAME in
   virtual directory PVD.  Resolve whether the is a volume mapped on PVD
   whose mounpoint name is not NAME and if so return ZFS_OK and store
   the internal dentry of the root of volume to DIR.  */

int32_t
validate_operation_on_virtual_directory (virtual_dir pvd, string *name,
					 internal_dentry *dir)
{
  virtual_dir vd;

  CHECK_MUTEX_LOCKED (&vd_mutex);
  CHECK_MUTEX_LOCKED (&pvd->mutex);
  if (pvd->vol)
    CHECK_MUTEX_LOCKED (&pvd->vol->mutex);

  vd = vd_lookup_name (pvd, name->str);
  if (vd)
    {
      /* Virtual directory tree is read only for users.  */
      if (pvd->vol)
	zfsd_mutex_unlock (&pvd->vol->mutex);
      zfsd_mutex_unlock (&pvd->mutex);
      zfsd_mutex_unlock (&vd->mutex);
      return EROFS;
    }
  else if (!pvd->vol)
    {
      zfsd_mutex_unlock (&pvd->mutex);
      return EROFS;
    }
  else
    {
      int32_t r = get_volume_root_dentry (pvd->vol, dir);
      if (r != ZFS_OK)
	{
	  zfsd_mutex_unlock (&pvd->vol->mutex);
	  zfsd_mutex_unlock (&pvd->mutex);
	  return r;
	}
      zfsd_mutex_unlock (&pvd->mutex);
    }

  return ZFS_OK;
}

/* Store the local file handle of root of volume VOL to LOCAL_FH
   and its attributes to ATTR.  */

static int32_t
get_volume_root_local (volume vol, zfs_fh *local_fh, fattr *attr)
{
  struct stat st;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (stat (vol->local_path, &st) != 0)
    return errno;

  if ((st.st_mode & S_IFMT) != S_IFDIR)
    return ENOTDIR;

  local_fh->sid = this_node->id;
  local_fh->vid = vol->id;
  local_fh->dev = st.st_dev;
  local_fh->ino = st.st_ino;
  fattr_from_struct_stat (attr, &st);

  return ZFS_OK;
}

/* Store the remote file handle of root of volume VOL to REMOTE_FH
   and its attributes to ATTR.  */

static int32_t
get_volume_root_remote (volume vol, zfs_fh *remote_fh, fattr *attr)
{
  volume_root_args args;
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  args.vid = vol->id;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_volume_root_client (t, &args, vol->master, &fd);
  if (r == ZFS_OK)
    {
      if (!decode_zfs_fh (&t->dc_reply, remote_fh)
	  || !decode_fattr (&t->dc_reply, attr)
	  || !finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);

  if (r == ZFS_OK && attr->type != FT_DIR)
    return ENOTDIR;
  return r;
}

/* Get file handle of root of volume VOL, store the local file handle to
   LOCAL_FH and master's file handle to MASTER_FH, if defined.  */

static int32_t
get_volume_root (volume vol, zfs_fh *local_fh, zfs_fh *master_fh, fattr *attr)
{
  int32_t r = ZFS_OK;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (vol->master == this_node)
    {
      /* The volume is managed by this node.  */
      if (local_fh)
	{
	  r = get_volume_root_local (vol, local_fh, attr);
	  if (r != ZFS_OK)
	    return r;
	}

      if (master_fh)
	zfs_fh_undefine (*master_fh);
    }
  else if (vol->local_path)
    {
      /* There is a copy of volume on this node.  */
      if (local_fh)
	{
	  r = get_volume_root_local (vol, local_fh, attr);
	  if (r != ZFS_OK)
	    return r;

	  if (master_fh)
	    {
	      fattr tmp;
	      r = get_volume_root_remote (vol, master_fh, &tmp);
	      if (r < ZFS_OK)
		{
		  zfs_fh_undefine (*master_fh);
		  r = ZFS_OK;
		}
	    }
	}
      else if (master_fh)
	{
	  r = get_volume_root_remote (vol, master_fh, attr);
	  if (r < ZFS_OK)
	    {
	      zfs_fh_undefine (*master_fh);
	      r = ZFS_OK;
	    }
	}
    }
  else
    {
      /* The volume is completelly remote.  */
      if (local_fh)
	{
	  r = get_volume_root_remote (vol, local_fh, attr);
	  if (r != ZFS_OK)
	    return r;

	  if (master_fh)
	    memcpy (master_fh, local_fh, sizeof (zfs_fh));
	}
      else if (master_fh)
	{
	  r = get_volume_root_remote (vol, master_fh, attr);
	}
    }

  return r;
}

/* Update root of volume VOL, create an internal file handle for it and store
   it to IFH.  */

int32_t
get_volume_root_dentry (volume vol, internal_dentry *dentry)
{
  zfs_fh local_fh, master_fh;
  fattr attr;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  r = get_volume_root (vol, &local_fh, &master_fh, &attr);
  if (r != ZFS_OK)
    return r;

  if (vol->root_dentry == NULL
      || !ZFS_FH_EQ (vol->root_dentry->fh->local_fh, local_fh)
      || (!ZFS_FH_EQ (vol->root_dentry->fh->master_fh, master_fh)
	  && !zfs_fh_undefined (vol->root_dentry->fh->master_fh)))
    {
      if (vol->root_dentry)
	{
	  zfsd_mutex_lock (&vol->root_dentry->fh->mutex);
	  /* FIXME? delete only FHs which are not open now?  */
	  internal_dentry_destroy (vol->root_dentry, vol);
	}

      vol->root_dentry = internal_dentry_create (&local_fh, &master_fh, vol,
						 NULL, "", &attr);
    }
  else if (zfs_fh_undefined (vol->root_dentry->fh->master_fh))
    {
      zfsd_mutex_lock (&vol->root_dentry->fh->mutex);
      vol->root_dentry->fh->master_fh = master_fh;
    }
  else
    zfsd_mutex_lock (&vol->root_dentry->fh->mutex);

  *dentry = vol->root_dentry;
  return ZFS_OK;
}

/* Convert attributes from STRUCT STAT ST to FATTR ATTR.  */

void
fattr_from_struct_stat (fattr *attr, struct stat *st)
{
  attr->version = 0;		/* FIXME */
  attr->dev = st->st_dev;
  attr->ino = st->st_ino;
  attr->mode = st->st_mode & (S_IRWXU | S_ISUID | S_ISGID | S_ISVTX);
  attr->nlink = st->st_nlink;
  attr->uid = map_uid_node2zfs (st->st_uid);
  attr->gid = map_gid_node2zfs (st->st_gid);
  attr->rdev = st->st_rdev;
  attr->size = st->st_size;
  attr->blocks = st->st_blocks;
  attr->blksize = st->st_blksize;
  attr->atime = st->st_atime;
  attr->mtime = st->st_mtime;
  attr->ctime = st->st_ctime;

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
}

/* Get attributes of local file PATH and store them to ATTR.  */

int32_t
local_getattr_path (fattr *attr, char *path)
{
  struct stat st;
  int32_t r;

  r = lstat (path, &st);
  if (r != 0)
    return errno;

  fattr_from_struct_stat (attr, &st);
  return ZFS_OK;
}

/* Get attributes of local file DENTRY on volume VOL
   and store them to ATTR.  */

int32_t
local_getattr (fattr *attr, internal_dentry dentry, volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  path = build_local_path (vol, dentry);
  r = local_getattr_path (attr, path);
  free (path);

  return r;
}

/* Get attributes of remote file DENTRY on volume VOL
   and store them to ATTR.  */

int32_t
remote_getattr (fattr *attr, internal_dentry dentry, volume vol)
{
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dentry->fh->master_fh))
    abort ();
#endif

  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_getattr_client (t, &dentry->fh->master_fh, vol->master, &fd);
  if (r == ZFS_OK)
    {
      if (!decode_fattr (&t->dc_reply, attr)
	  || !finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);

  return r;
}

/* Get attributes for file with handle FH and store them to FA.  */

int32_t
zfs_getattr (fattr *fa, zfs_fh *fh)
{
  volume vol;
  internal_dentry dentry;
  virtual_dir vd;
  int32_t r;
  int retry = 0;

zfs_getattr_retry:
  
  /* Lookup FH.  */
  r = zfs_fh_lookup (fh, &vol, &dentry, &vd);
  if (r != ZFS_OK)
    return r;

  if (!dentry)
    {
      if (vol)
	{
	  r = get_volume_root_dentry (vol, &dentry);
	  zfsd_mutex_unlock (&vd->mutex);
	  if (r != ZFS_OK)
	    {
	      zfsd_mutex_unlock (&vol->mutex);
	      return r;
	    }
	}
      else
	{
	  *fa = vd->attr;
	  zfsd_mutex_unlock (&vd->mutex);
	  return ZFS_OK;
	}
    }

  if (vol->local_path)
    r = local_getattr (&dentry->fh->attr, dentry, vol);
  else if (vol->master != this_node)
    r = remote_getattr (&dentry->fh->attr, dentry, vol);
  else
    abort ();

  if (r == ZFS_OK)
    *fa = dentry->fh->attr;

  zfsd_mutex_unlock (&dentry->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (fh);
      if (r == ZFS_OK)
	goto zfs_getattr_retry;
    }

  return ZFS_OK;
}

/* Set attributes of local file PATH according to SA,
   reget attributes and store them to FA.  */

int32_t
local_setattr_path (fattr *fa, char *path, sattr *sa)
{
  if (sa->mode != (uint32_t) -1)
    {
      if (chmod (path, sa->mode) != 0)
	return errno;
    }

  if (sa->uid != (uint32_t) -1 || sa->gid != (uint32_t) -1)
    {
      if (lchown (path, map_uid_zfs2node (sa->uid),
		  map_gid_zfs2node (sa->gid)) != 0)
	return errno;
    }

  if (sa->size != (uint64_t) -1)
    {
      if (truncate (path, sa->size) != 0)
	return errno;
    }

  if (sa->atime != (zfs_time) -1 || sa->mtime != (zfs_time) -1)
    {
      struct utimbuf t;

      t.actime = sa->atime;
      t.modtime = sa->mtime;
      if (utime (path, &t) != 0)
	return errno;
    }

  if (fa)
    return local_getattr_path (fa, path);
  return ZFS_OK;
}

/* Set attributes of local file DENTRY on volume VOL according to SA,
   reget attributes and store them to FA.  */

int32_t
local_setattr (fattr *fa, internal_dentry dentry, sattr *sa, volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  path = build_local_path (vol, dentry);
  r = local_setattr_path (fa, path, sa);
  free (path);

  return r;
}

/* Set attributes of remote file DENTRY on volume VOL according to SA,
   reget attributes and store them to FA.  */

static int32_t
remote_setattr (fattr *fa, internal_fh fh, sattr *sa, volume vol)
{
  sattr_args args;
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (fh->master_fh))
    abort ();
#endif

  args.file = fh->master_fh;
  args.attr = *sa;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_setattr_client (t, &args, vol->master, &fd);
  if (r == ZFS_OK)
    {
      if (!decode_fattr (&t->dc_reply, fa)
	  || !finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);

  return r;
}

/* Set attributes of file with handle FH according to SA, reget attributes
   and store them to FA.  */

int32_t
zfs_setattr (fattr *fa, zfs_fh *fh, sattr *sa)
{
  volume vol;
  internal_dentry dentry;
  int32_t r;
  int retry = 0;

  /* Virtual directory tree is read only for users.  */
  if (VIRTUAL_FH_P (*fh))
    return EROFS;

zfs_setattr_retry:

  /* Lookup FH.  */
  r = zfs_fh_lookup (fh, &vol, &dentry, NULL);
  if (r != ZFS_OK)
    return r;

  if (vol->local_path)
    r = local_setattr (fa, dentry, sa, vol);
  else if (vol->master != this_node)
    r = remote_setattr (fa, dentry->fh, sa, vol);
  else
    abort ();

  /* Update cached file attributes.  */
  if (r == ZFS_OK)
    dentry->fh->attr = *fa;

  zfsd_mutex_unlock (&dentry->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (fh);
      if (r == ZFS_OK)
	goto zfs_setattr_retry;
    }

  return r;
}

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int32_t
zfs_extended_lookup (dir_op_res *res, zfs_fh *dir, char *path)
{
  string str;
  int32_t r;

  res->file = (*path == '/') ? root_fh : *dir;
  while (*path)
    {
      while (*path == '/')
	path++;

      str.str = path;
      while (*path != 0 && *path != '/')
	path++;
      if (*path == '/')
	*path++ = 0;
      str.len = strlen (str.str);

      r = zfs_lookup (res, &res->file, &str);
      if (r)
	return r;
    }

  return ZFS_OK;
}

/* Lookup file handle of local file NAME in directory DIR on volume VOL
   and store it to FH.  */

static int32_t
local_lookup (dir_op_res *res, internal_dentry dir, string *name, volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = local_getattr_path (&res->attr, path);
  free (path);
  if (r != ZFS_OK)
    return r;

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;
  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;

  return ZFS_OK;
}

/* Lookup file handle of remote file NAME in directory DIR on volume VOL
   and store it to FH.  */

static int32_t
remote_lookup (dir_op_res *res, internal_fh dir, string *name, volume vol)
{
  dir_op_args args;
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&dir->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->master_fh))
    abort ();
#endif

  args.dir = dir->master_fh;
  args.name = *name;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_lookup_client (t, &args, vol->master, &fd);
  if (r == ZFS_OK)
    {
      if (!decode_dir_op_res (&t->dc_reply, res)
	  || !finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);
  return r;
}

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int32_t
zfs_lookup (dir_op_res *res, zfs_fh *dir, string *name)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  dir_op_res master_res;
  int32_t r;
  int retry = 0;

zfs_lookup_retry:

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&volume_mutex);
      if (VIRTUAL_FH_P (*dir))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }
  zfsd_mutex_unlock (&volume_mutex);

  if (pvd)
    {
      virtual_dir vd;

      CHECK_MUTEX_LOCKED (&pvd->mutex);
      if (vol)
	CHECK_MUTEX_LOCKED (&vol->mutex);

      vd = vd_lookup_name (pvd, name->str);
      zfsd_mutex_unlock (&vd_mutex);
      if (vd)
	{
	  res->file = vd->fh;
	  res->attr = vd->attr;
	  if (vol)
	    zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&pvd->mutex);
	  zfsd_mutex_unlock (&vd->mutex);
	  return ZFS_OK;
	}

      /* !vd */
      if (vol)
	{
	  r = get_volume_root_dentry (vol, &idir);
	  if (r != ZFS_OK)
	    {
	      zfsd_mutex_unlock (&vol->mutex);
	      zfsd_mutex_unlock (&pvd->mutex);
	      return r;
	    }
	  zfsd_mutex_unlock (&pvd->mutex);
	}
      else
	{
	  zfsd_mutex_unlock (&pvd->mutex);
	  return ENOENT;
	}
    }

  if (!idir)
    abort ();

  if (idir->fh->attr.type != FT_DIR)
    {
      zfsd_mutex_unlock (&idir->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return ENOTDIR;
    }

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      zfsd_mutex_unlock (&idir->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  /* TODO: update directory */

  CHECK_MUTEX_LOCKED (&idir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (vol->local_path)
    {
      r = local_lookup (res, idir, name, vol);
      if (r == ZFS_OK)
	zfs_fh_undefine (master_res.file);
    }
  else if (vol->master != this_node)
    {
      r = remote_lookup (res, idir->fh, name, vol);
      if (r == ZFS_OK)
	master_res.file = res->file;
    }
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      /* Update internal file handles in htab.  */
      dentry = dentry_lookup_name (vol, idir, name->str);
      if (dentry)
	{
	  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

	  if (!ZFS_FH_EQ (dentry->fh->local_fh, res->file)
	      || (!ZFS_FH_EQ (dentry->fh->master_fh, master_res.file)
		  && !zfs_fh_undefined (dentry->fh->master_fh)))
	    {
	      internal_dentry_destroy (dentry, vol);
	      dentry = internal_dentry_create (&res->file, &master_res.file,
					       vol, idir, name->str,
					       &res->attr);
	    }
	  else if (zfs_fh_undefined (dentry->fh->master_fh))
	    {
	      dentry->fh->master_fh = master_res.file;
	    }
	}
      else
	dentry = internal_dentry_create (&res->file, &master_res.file, vol,
					 idir, name->str, &res->attr);
      zfsd_mutex_unlock (&dentry->fh->mutex);
    }

  zfsd_mutex_unlock (&idir->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (dir);
      if (r == ZFS_OK)
	goto zfs_lookup_retry;
    }

  return r;
}

/* Create directory NAME in local directory DIR on volume VOL, set owner,
   group and permitions according to ATTR.  */

int32_t
local_mkdir (dir_op_res *res, internal_dentry dir, string *name, sattr *attr,
	     volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = mkdir (path, attr->mode);
  if (r != 0)
    {
      free (path);
      return errno;
    }

  r = local_setattr_path (&res->attr, path, attr);
  free (path);
  if (r != ZFS_OK)
    return r;

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;
  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;

  return ZFS_OK;
}

/* Create directory NAME in remote directory DIR on volume VOL, set owner,
   group and permitions according to ATTR.  */

int32_t
remote_mkdir (dir_op_res *res, internal_fh dir, string *name, sattr *attr,
	      volume vol)
{
  mkdir_args args;
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->master_fh))
    abort ();
#endif

  args.where.dir = dir->master_fh;
  args.where.name = *name;
  args.attr = *attr;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_mkdir_client (t, &args, vol->master, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_dir_op_res (&t->dc_reply, res)
	  || !finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);
  return r;
}

/* Create directory NAME in directory DIR, set owner, group and permitions
   according to ATTR.  */

int32_t
zfs_mkdir (dir_op_res *res, zfs_fh *dir, string *name, sattr *attr)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  dir_op_res master_res;
  int32_t r;
  int retry = 0;

zfs_mkdir_retry:

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&volume_mutex);
      if (VIRTUAL_FH_P (*dir))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }
  zfsd_mutex_unlock (&volume_mutex);

  if (pvd)
    {
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      zfsd_mutex_unlock (&idir->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  attr->size = (uint64_t) -1;
  attr->atime = (zfs_time) -1;
  attr->mtime = (zfs_time) -1;

  if (vol->local_path)
    {
      r = local_mkdir (res, idir, name, attr, vol);
      if (r == ZFS_OK)
	zfs_fh_undefine (master_res.file);
    }
  else if (vol->master != this_node)
    {
      r = remote_mkdir (res, idir->fh, name, attr, vol);
      if (r == ZFS_OK)
	master_res.file = res->file;
    }
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      /* Update internal file handle in htab.  */
      dentry = dentry_lookup_name (vol, idir, name->str);
      if (dentry)
	{
	  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

	  internal_dentry_destroy (dentry, vol);
	}
      dentry = internal_dentry_create (&res->file, &master_res.file, vol,
				       idir, name->str, &res->attr);
      zfsd_mutex_unlock (&dentry->fh->mutex);
    }

  zfsd_mutex_unlock (&idir->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (dir);
      if (r == ZFS_OK)
	goto zfs_mkdir_retry;
    }

  return r;
}

/* Remove local directory NAME from directory DIR on volume VOL.  */

static int32_t
local_rmdir (internal_dentry dir, string *name, volume vol)
{
  char *path;
  struct stat st;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = lstat (path, &st);
  if (r != 0)
    {
      free (path);
      return errno;
    }
  r = rmdir (path);
  free (path);
  if (r == 0)
    {
      if (!delete_metadata (vol, st.st_dev, st.st_ino))
	vol->flags |= VOLUME_DELETE;
    }
  else
    return errno;

  return ZFS_OK;
}

/* Remove remote directory NAME from directory DIR on volume VOL.  */

static int32_t
remote_rmdir (internal_fh dir, string *name, volume vol)
{
  dir_op_args args;
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->master_fh))
    abort ();
#endif

  args.dir = dir->master_fh;
  args.name = *name;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_rmdir_client (t, &args, vol->master, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);
  return r;
}

/* Remove directory NAME from directory DIR.  */

int32_t
zfs_rmdir (zfs_fh *dir, string *name)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  int32_t r;
  int retry = 0;

zfs_rmdir_retry:

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&volume_mutex);
      if (VIRTUAL_FH_P (*dir))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }
  zfsd_mutex_unlock (&volume_mutex);

  if (pvd)
    {
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      zfsd_mutex_unlock (&idir->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  if (vol->local_path)
    r = local_rmdir (idir, name, vol);
  else if (vol->master != this_node)
    r = remote_rmdir (idir->fh, name, vol);
  else
    abort ();

  /* Delete the internal file handle of the deleted directory.  */
  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      dentry = dentry_lookup_name (vol, idir, name->str);
      if (dentry)
	internal_dentry_destroy (dentry, vol);
    }

  zfsd_mutex_unlock (&idir->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (dir);
      if (r == ZFS_OK)
	goto zfs_rmdir_retry;
    }

  return r;
}

/* Rename local file FROM_NAME in directory FROM_DIR to file TO_NAME
   in directory TO_DIR on volume VOL.  */

static int32_t
local_rename (internal_dentry from_dir, string *from_name,
	      internal_dentry to_dir, string *to_name, volume vol)
{
  char *path1, *path2;
  int32_t r;

  CHECK_MUTEX_LOCKED (&from_dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&to_dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  path1 = build_local_path_name (vol, from_dir, from_name->str);
  path2 = build_local_path_name (vol, to_dir, to_name->str);
  r = rename (path1, path2);
  free (path1);
  free (path2);
  if (r != 0)
    return errno;

  return ZFS_OK;
}

/* Rename remote file FROM_NAME in directory FROM_DIR to file TO_NAME
   in directory TO_DIR on volume VOL.  */

static int32_t
remote_rename (internal_fh from_dir, string *from_name,
	       internal_fh to_dir, string *to_name, volume vol)
{
  rename_args args;
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&from_dir->mutex);
  CHECK_MUTEX_LOCKED (&to_dir->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (from_dir->master_fh))
    abort ();
  if (zfs_fh_undefined (to_dir->master_fh))
    abort ();
#endif

  args.from.dir = from_dir->master_fh;
  args.from.name = *from_name;
  args.to.dir = to_dir->master_fh;
  args.to.name = *to_name;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_rename_client (t, &args, vol->master, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);
  return r;
}

/* Rename file FROM_NAME in directory FROM_DIR to file TO_NAME
   in directory TO_DIR.  */

int32_t
zfs_rename (zfs_fh *from_dir, string *from_name,
	    zfs_fh *to_dir, string *to_name)
{
  volume vol;
  internal_dentry dentry1, dentry2;
  virtual_dir vd1, vd2;
  int32_t r;
  int retry = 0;

zfs_rename_retry:

  /* Lookup FROM_DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (from_dir, &vol, &dentry1, &vd1);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&volume_mutex);
      zfsd_mutex_unlock (&vd_mutex);
      return r;
    }
  zfsd_mutex_unlock (&volume_mutex);
  if (!vol)
    {
      /* FROM_DIR is a virtual directory without volume under it.  */
      zfsd_mutex_unlock (&vd1->mutex);
      zfsd_mutex_unlock (&vd_mutex);
      return EROFS;
    }
  /* Temporarily unlock IFH1, we are still holding VOL->MUTEX so we are
     allowed to lock it again.  */
  if (dentry1)
    zfsd_mutex_unlock (&dentry1->fh->mutex);
  /* Similarly, we are holding VD_MUTEX so unlock VD1->MUTEX.  */
  if (vd1)
    zfsd_mutex_unlock (&vd1->mutex);

  if (VIRTUAL_FH_P (*to_dir))
    {
      vd2 = vd_lookup (to_dir);
      if (!vd2)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  return ENOENT;
	}
      zfsd_mutex_lock (&vd2->mutex);
      if (vd2->vol != vol)
	{
	  r = vd2->vol ? EXDEV : EROFS;
	  zfsd_mutex_unlock (&vd2->mutex);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  return r;
	}
    }
  else
    {
      vd2 = NULL;
      if (vol->id != to_dir->vid)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  return EXDEV;
	}

      dentry2 = dentry_lookup (vol, to_dir);
      if (!dentry2)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  return ESTALE;
	}
    }

  /* At this point, both file handles are for the same volume.  */
  if (vd2)
    {
      r = validate_operation_on_virtual_directory (vd2, to_name, &dentry2);
      if (r != ZFS_OK)
	{
	  zfsd_mutex_unlock (&vd_mutex);
	  return r;
	}
      zfsd_mutex_unlock (&dentry2->fh->mutex);
    }
  if (vd1)
    {
      zfsd_mutex_lock (&vd1->mutex);
      r = validate_operation_on_virtual_directory (vd1, to_name, &dentry1);
      if (r != ZFS_OK)
	{
	  zfsd_mutex_unlock (&vd_mutex);
	  return r;
	}
      zfsd_mutex_unlock (&dentry1->fh->mutex);
    }
  zfsd_mutex_unlock (&vd_mutex);

  /* Hide ".zfs" in root of volumes.  */
  if ((!dentry1->parent && strncmp (from_name->str, ".zfs", 5) == 0)
      || (!dentry2->parent && strncmp (to_name->str, ".zfs", 5) == 0))
    {
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  zfsd_mutex_lock (&dentry1->fh->mutex);
  if (dentry1 != dentry2)
    zfsd_mutex_lock (&dentry2->fh->mutex);

  if (dentry1->fh->master_fh.dev != dentry2->fh->master_fh.dev)
    {
      zfsd_mutex_unlock (&dentry1->fh->mutex);
      zfsd_mutex_unlock (&dentry2->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return EXDEV;
    }

  if (vol->local_path)
    r = local_rename (dentry1, from_name, dentry2, to_name, vol);
  else if (vol->master != this_node)
    r = remote_rename (dentry1->fh, from_name, dentry2->fh, to_name, vol);
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      /* Delete the dentry which is in the place where we are moving to.  */
      dentry = dentry_lookup_name (vol, dentry2, to_name->str);
      if (dentry)
	{
	  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

	  internal_dentry_destroy (dentry, vol);
	}

      /* Move the dentry.  */
      dentry = dentry_lookup_name (vol, dentry1, from_name->str);
      if (!dentry)
	r = ENOENT;
      else
	{
	  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

	  if (!internal_dentry_move (dentry, vol, dentry2, to_name->str))
	    r = EINVAL;
	  zfsd_mutex_unlock (&dentry->fh->mutex);
	}
    }

  zfsd_mutex_unlock (&dentry1->fh->mutex);
  if (dentry1 != dentry2)
    zfsd_mutex_unlock (&dentry2->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (from_dir);
      if (r == ZFS_OK)
	r = refresh_path (to_dir);
      if (r == ZFS_OK)
	goto zfs_rename_retry;
    }

  return r;
}

/* Link local file FROM to be a file with NAME in directory DIR
   on volume VOL.  */

static int32_t
local_link (internal_dentry from, internal_dentry dir, string *name, volume vol)
{
  char *path1, *path2;
  int32_t r;

  CHECK_MUTEX_LOCKED (&from->fh->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  path1 = build_local_path (vol, from);
  path2 = build_local_path_name (vol, dir, name->str);
  r = link (path1, path2);
  free (path1);
  free (path2);
  if (r != 0)
    return errno;

  return ZFS_OK;
}

/* Link remote file FROM to be a file with NAME in directory DIR
   on volume VOL.  */

static int32_t
remote_link (internal_fh from, internal_fh dir, string *name, volume vol)
{
  link_args args;
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&from->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (from->master_fh))
    abort ();
  if (zfs_fh_undefined (dir->master_fh))
    abort ();
#endif

  args.from = from->master_fh;
  args.to.dir = dir->master_fh;
  args.to.name = *name;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_link_client (t, &args, vol->master, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);
  return r;
}

/* Link file FROM to be a file with NAME in directory DIR.  */

int32_t
zfs_link (zfs_fh *from, zfs_fh *dir, string *name)
{
  volume vol;
  internal_dentry dentry1, dentry2;
  virtual_dir vd1, vd2;
  int32_t r;
  int retry = 0;

zfs_link_retry:

  /* Lookup FROM.  */
  zfsd_mutex_lock (&volume_mutex);
  zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (from, &vol, &dentry1, &vd1);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&volume_mutex);
      zfsd_mutex_unlock (&vd_mutex);
      return r;
    }
  zfsd_mutex_unlock (&volume_mutex);
  if (!vol)
    {
      /* FROM is a virtual directory without volume under it.  */
      zfsd_mutex_unlock (&vd1->mutex);
      zfsd_mutex_unlock (&vd_mutex);
      return EROFS;
    }

  /* Temporarily unlock IFH1, we are still holding VOL->MUTEX so we are
     allowed to lock it again.  */
  if (dentry1)
    zfsd_mutex_unlock (&dentry1->fh->mutex);
  /* We do not need VD1 to be locked anymore.  */
  if (vd1)
    zfsd_mutex_unlock (&vd1->mutex);

  if (VIRTUAL_FH_P (*dir))
    {
      vd2 = vd_lookup (dir);
      if (!vd2)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  return ENOENT;
	}
      zfsd_mutex_lock (&vd2->mutex);
      if (vd2->vol != vol)
	{
	  r = vd2->vol ? EXDEV : EROFS;
	  zfsd_mutex_unlock (&vd2->mutex);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  return r;
	}
    }
  else
    {
      vd2 = NULL;
      if (vol->id != dir->vid)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  return EXDEV;
	}

      dentry2 = dentry_lookup (vol, dir);
      if (!dentry2)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  return ESTALE;
	}
    }

  /* At this point, both file handles are for the same volume.  */

  if (vd2)
    {
      r = validate_operation_on_virtual_directory (vd2, name, &dentry2);
      if (r != ZFS_OK)
	{
	  zfsd_mutex_unlock (&vd_mutex);
	  return r;
	}

      CHECK_MUTEX_LOCKED (&dentry2->fh->mutex);

      if (vd1)
	dentry1 = dentry2;
      else if (dentry1 != dentry2)
	zfsd_mutex_lock (&dentry1->fh->mutex);
    }
  else if (vd1)
    {
      r = get_volume_root_dentry (vol, &dentry1);
      if (r != ZFS_OK)
	{
	  zfsd_mutex_unlock (&vd_mutex);
	  return EROFS;
	}

      CHECK_MUTEX_LOCKED (&dentry1->fh->mutex);

      if (dentry1 != dentry2)
	zfsd_mutex_lock (&dentry2->fh->mutex);
    }
  else
    {
      zfsd_mutex_lock (&dentry1->fh->mutex);
      if (dentry1 != dentry2)
	zfsd_mutex_lock (&dentry2->fh->mutex);
    }
  zfsd_mutex_unlock (&vd_mutex);

  /* Hide ".zfs" in the root of the volume.  */
  if (!dentry2->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      zfsd_mutex_unlock (&dentry1->fh->mutex);
      if (dentry1 != dentry2)
	zfsd_mutex_unlock (&dentry2->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  if (dentry1->fh->master_fh.dev != dentry2->fh->master_fh.dev)
    {
      zfsd_mutex_unlock (&dentry1->fh->mutex);
      zfsd_mutex_unlock (&dentry2->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return EXDEV;
    }

  if (vol->local_path)
    r = local_link (dentry1, dentry2, name, vol);
  else if (vol->master != this_node)
    r = remote_link (dentry1->fh, dentry2->fh, name, vol);
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      /* Delete internal file handle in htab because it is outdated.  */
      dentry = dentry_lookup_name (vol, dentry2, name->str);
      if (dentry)
	{
	  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

	  internal_dentry_destroy (dentry, vol);
	}
      dentry = internal_dentry_link (dentry1->fh, vol, dentry2, name->str);
      /* DENTRY->FH->MUTEX is already locked
	 because DENTRY->FH == DENTRY1->FH.  */
    }

  zfsd_mutex_unlock (&dentry1->fh->mutex);
  if (dentry1 != dentry2)
    zfsd_mutex_unlock (&dentry2->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (from);
      if (r == ZFS_OK)
	r = refresh_path (dir);
      if (r == ZFS_OK)
	goto zfs_link_retry;
    }

  return r;
}

/* Delete local file NAME from directory DIR on volume VOL.  */

static int32_t
local_unlink (internal_dentry dir, string *name, volume vol)
{
  char *path;
  struct stat st;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = lstat (path, &st);
  if (r != 0)
    {
      free (path);
      return errno;
    }
  r = unlink (path);
  free (path);
  if (r == 0)
    {
      if (!delete_metadata (vol, st.st_dev, st.st_ino))
	vol->flags |= VOLUME_DELETE;
    }
  else
    return errno;

  return ZFS_OK;
}

/* Delete remote file NAME from directory DIR on volume VOL.  */

static int32_t
remote_unlink (internal_fh dir, string *name, volume vol)
{
  dir_op_args args;
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->master_fh))
    abort ();
#endif

  args.dir = dir->master_fh;
  args.name = *name;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_unlink_client (t, &args, vol->master, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);
  return r;
}

/* Remove directory NAME from directory DIR.  */

int32_t
zfs_unlink (zfs_fh *dir, string *name)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  int32_t r;
  int retry = 0;

zfs_unlink_retry:

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&volume_mutex);
      if (VIRTUAL_FH_P (*dir))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }
  zfsd_mutex_unlock (&volume_mutex);

  if (pvd)
    {
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      zfsd_mutex_unlock (&idir->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  if (vol->local_path)
    r = local_unlink (idir, name, vol);
  else if (vol->master != this_node)
    r = remote_unlink (idir->fh, name, vol);
  else
    abort ();

  /* Delete the internal file handle of the deleted directory.  */
  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      dentry = dentry_lookup_name (vol, idir, name->str);
      if (dentry)
	internal_dentry_destroy (dentry, vol);
    }

  zfsd_mutex_unlock (&idir->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (dir);
      if (r == ZFS_OK)
	goto zfs_unlink_retry;
    }

  return r;
}

/* Read local symlink FILE on volume VOL.  */

int32_t
local_readlink (read_link_res *res, internal_dentry file, volume vol)
{
  char *path;
  char buf[ZFS_MAXDATA + 1];
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&file->fh->mutex);

  path = build_local_path (vol, file);
  zfsd_mutex_unlock (&vol->mutex);
  r = readlink (path, buf, ZFS_MAXDATA);
  free (path);
  if (r < 0)
    return errno;

  buf[r] = 0;
  res->path.str = xstrdup (buf);
  res->path.len = r;

  return ZFS_OK;
}

/* Read remote symlink FH on volume VOL.  */

int32_t
remote_readlink (read_link_res *res, internal_fh fh, volume vol)
{
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (fh->master_fh))
    abort ();
#endif

  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&node_mutex);
  zfsd_mutex_unlock (&vol->mutex);
  r = zfs_proc_readlink_client (t, &fh->master_fh, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_zfs_path (&t->dc_reply, &res->path))
	r = ZFS_INVALID_REPLY;
      else if (!finish_decoding (&t->dc_reply))
	{
	  free (res->path.str);
	  r = ZFS_INVALID_REPLY;
	}
    }
  else if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);
  return r;
}

/* Read symlink FH.  */

int32_t
zfs_readlink (read_link_res *res, zfs_fh *fh)
{
  volume vol;
  internal_dentry dentry;
  int32_t r;
  int retry = 0;

  if (VIRTUAL_FH_P (*fh))
    return EINVAL;

zfs_readlink_retry:

  /* Lookup FH.  */
  r = zfs_fh_lookup (fh, &vol, &dentry, NULL);
  if (r != ZFS_OK)
    return r;

  if (vol->local_path)
    r = local_readlink (res, dentry, vol);
  else if (vol->master != this_node)
    r = remote_readlink (res, dentry->fh, vol);
  else
    abort ();

  zfsd_mutex_unlock (&dentry->fh->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (fh);
      if (r == ZFS_OK)
	goto zfs_readlink_retry;
    }

  return r;
}

/* Create local symlink NAME in directory DIR on volume VOL pointing to TO,
   set its attributes according to ATTR.  */

int32_t
local_symlink (internal_dentry dir, string *name, string *to, sattr *attr,
	       volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = symlink (to->str, path);
  if (r != 0)
    {
      free (path);
      return errno;
    }

  r = local_setattr_path (NULL, path, attr);
  free (path);
  if (r != ZFS_OK)
    return r;

  return ZFS_OK;
}

/* Create remote symlink NAME in directory DIR on volume VOL pointing to TO,
   set its attributes according to ATTR.  */

int32_t
remote_symlink (internal_fh dir, string *name, string *to, sattr *attr,
		volume vol)
{
  symlink_args args;
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->master_fh))
    abort ();
#endif

  args.from.dir = dir->master_fh;
  args.from.name = *name;
  args.to = *to;
  args.attr = *attr;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_symlink_client (t, &args, vol->master, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);
  return r;
}

/* Create symlink NAME in directory DIR pointing to TO,
   set its attributes according to ATTR.  */

int32_t
zfs_symlink (zfs_fh *dir, string *name, string *to, sattr *attr)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  int32_t r;
  int retry = 0;

zfs_symlink_retry:

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&volume_mutex);
      if (VIRTUAL_FH_P (*dir))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }
  zfsd_mutex_unlock (&volume_mutex);

  if (pvd)
    {
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      zfsd_mutex_unlock (&idir->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  attr->mode = (uint32_t) -1;
  attr->size = (uint64_t) -1;
  attr->atime = (zfs_time) -1;
  attr->mtime = (zfs_time) -1;

  if (vol->local_path)
    r = local_symlink (idir, name, to, attr, vol);
  else if (vol->master != this_node)
    r = remote_symlink (idir->fh, name, to, attr, vol);
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      /* Delete internal file handle in htab because it is outdated.  */
      dentry = dentry_lookup_name (vol, idir, name->str);
      if (dentry)
	internal_dentry_destroy (dentry, vol);
    }

  zfsd_mutex_unlock (&idir->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (dir);
      if (r == ZFS_OK)
	goto zfs_symlink_retry;
    }

  return r;
}

/* Create local special file NAME of type TYPE in directory DIR,
   set the attributes according to ATTR.
   If device is being created RDEV is its number.  */

int32_t
local_mknod (internal_dentry dir, string *name, sattr *attr, ftype type,
	     uint32_t rdev, volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = mknod (path, attr->mode | ftype2mode[type], rdev);
  if (r != 0)
    {
      free (path);
      return errno;
    }

  r = local_setattr_path (NULL, path, attr);
  free (path);
  if (r != ZFS_OK)
    return r;

  return ZFS_OK;
}

/* Create remote special file NAME of type TYPE in directory DIR,
   set the attributes according to ATTR.
   If device is being created RDEV is its number.  */

int32_t
remote_mknod (internal_fh dir, string *name, sattr *attr, ftype type,
	      uint32_t rdev, volume vol)
{
  mknod_args args;
  thread *t;
  int32_t r;
  int fd;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->master_fh))
    abort ();
#endif

  args.where.dir = dir->master_fh;
  args.where.name = *name;
  args.attr = *attr;
  args.type = type;
  args.rdev = rdev;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_mknod_client (t, &args, vol->master, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (&t->dc_reply, fd);
  return r;
}

/* Create special file NAME of type TYPE in directory DIR,
   set the attributes according to ATTR.
   If device is being created RDEV is its number.  */

int32_t
zfs_mknod (zfs_fh *dir, string *name, sattr *attr, ftype type,
	   uint32_t rdev)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  int32_t r;
  int retry = 0;

zfs_mknod_retry:

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
  if (r != ZFS_OK)
    {
      zfsd_mutex_unlock (&volume_mutex);
      if (VIRTUAL_FH_P (*dir))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }
  zfsd_mutex_unlock (&volume_mutex);

  if (pvd)
    {
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      zfsd_mutex_unlock (&idir->fh->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  attr->size = (uint64_t) -1;
  attr->atime = (zfs_time) -1;
  attr->mtime = (zfs_time) -1;

  if (vol->local_path)
    {
      r = local_mknod (idir, name, attr, type, rdev, vol);
    }
  else if (vol->master != this_node)
    {
      r = remote_mknod (idir->fh, name, attr, type, rdev, vol);
    }
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      /* Delete internal file handle in htab because it is outdated.  */
      dentry = dentry_lookup_name (vol, idir, name->str);
      if (dentry)
	{
	  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

	  internal_dentry_destroy (dentry, vol);
	}
    }

  zfsd_mutex_unlock (&idir->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  if (r == ESTALE && retry < 1)
    {
      retry++;
      r = refresh_path (dir);
      if (r == ZFS_OK)
	goto zfs_mknod_retry;
    }

  return r;
}

/* Recursively refresh path to DIR on volume VOL and lookup NAME.
   Store result of REMOTE_LOOKUP to RES (unused).  */

static int32_t
refresh_path_1 (dir_op_res *res, internal_dentry dir, char *name, volume vol)
{
  int32_t r;
  string s;

  if (dir == NULL)
    return ENOENT;

  s.str = name;
  s.len = strlen (name);

  zfsd_mutex_lock (&dir->fh->mutex);
  r = remote_lookup (res, dir->fh, &s, vol);
  if (r == ESTALE)
    {
      r = refresh_path_1 (res, dir->parent, dir->name, vol);
      if (r == ZFS_OK)
	r = remote_lookup (res, dir->fh, &s, vol);
    }
  zfsd_mutex_unlock (&dir->fh->mutex);

  return r;
}

/* Refresh file handles on path to ZFS_FH FH.  */

int32_t
refresh_path (zfs_fh *fh)
{
  dir_op_res res;
  internal_dentry dentry;
  volume vol;
  int32_t r;

  if (VIRTUAL_FH_P (*fh))
    return EINVAL;

  r = zfs_fh_lookup (fh, &vol, &dentry, NULL);
  if (r != ZFS_OK)
    return r;

  r = refresh_path_1 (&res, dentry->parent, dentry->name, vol);

  zfsd_mutex_unlock (&dentry->fh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  return r;
}
