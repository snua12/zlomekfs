/* Directory operations.
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
#include "data-coding.h"
#include "volume.h"
#include "network.h"
#include "zfs_prot.h"
#include "user-group.h"
#include "update.h"

/* Return the local path of file for dentry DENTRY on volume VOL.  */

void
build_local_path (string *dst, volume vol, internal_dentry dentry)
{
  internal_dentry tmp;
  unsigned int n;
  varray v;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
    abort ();
#endif

  /* Count the number of strings which will be concatenated.  */
  n = 1;
  for (tmp = dentry; tmp->parent; tmp = tmp->parent)
    if (!CONFLICT_DIR_P (tmp->fh->local_fh))
      n += 2;

  varray_create (&v, sizeof (string), n);
  VARRAY_USED (v) = n;
  for (tmp = dentry; tmp->parent; tmp = tmp->parent)
    {
      if (CONFLICT_DIR_P (tmp->fh->local_fh))
	n += 2;

      n--;
      VARRAY_ACCESS (v, n, string) = tmp->name;
      n--;
      VARRAY_ACCESS (v, n, string).str = "/";
      VARRAY_ACCESS (v, n, string).len = 1;
    }
  VARRAY_ACCESS (v, 0, string) = vol->local_path;

  xstringconcat_varray (dst, &v);
  varray_destroy (&v);
}

/* Return the local path of file NAME in directory DENTRY on volume VOL.  */

void
build_local_path_name (string *dst, volume vol, internal_dentry dentry,
		       string *name)
{
  internal_dentry tmp;
  unsigned int n;
  varray v;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#ifdef ENABLE_CHECKING
  if (!INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
    abort ();
#endif

  /* Count the number of strings which will be concatenated.  */
  n = 3;
  for (tmp = dentry; tmp->parent; tmp = tmp->parent)
    if (!CONFLICT_DIR_P (tmp->fh->local_fh))
      n += 2;

  varray_create (&v, sizeof (string), n);
  VARRAY_USED (v) = n;
  n--;
  VARRAY_ACCESS (v, n, string) = *name;
  n--;
  VARRAY_ACCESS (v, n, string).str = "/";
  VARRAY_ACCESS (v, n, string).len = 1;
  for (tmp = dentry; tmp->parent; tmp = tmp->parent)
    {
      if (CONFLICT_DIR_P (tmp->fh->local_fh))
	n += 2;

      n--;
      VARRAY_ACCESS (v, n, string) = tmp->name;
      n--;
      VARRAY_ACCESS (v, n, string).str = "/";
      VARRAY_ACCESS (v, n, string).len = 1;
    }
  VARRAY_ACCESS (v, 0, string) = vol->local_path;

  xstringconcat_varray (dst, &v);
  varray_destroy (&v);
}

/* Return a pointer into PATH where path relative to volume root starts.  */

static void
local_path_to_relative_path (string *dst, volume vol, string *path)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);

  if (path->str == NULL)
    {
      dst->str = NULL;
      dst->len = 0;
      return;
    }

#ifdef ENABLE_CHECKING
  if (path->len < vol->local_path.len)
    abort ();
  if (memcmp (path->str, vol->local_path.str, vol->local_path.len) != 0)
    abort ();
#endif

  dst->len = path->len - vol->local_path.len;
  dst->str = (char *) xmemdup (path->str + vol->local_path.len, dst->len + 1);
}

/* Return short file name from the path PATH.  */

static void
file_name_from_path (string *dst, string *path)
{
#ifdef ENABLE_CHECKING
  if (path->str[0] != '/')
    abort ();
#endif

  for (dst->str = path->str + path->len; *dst->str != '/'; dst->str--)
    ;
  dst->str++;

  dst->len = path->str + path->len - dst->str;
}

/* Check whether parent of file PATH exists and return ESTALE if it does not
   exist.  */

static int32_t
parent_exists (string *path)
{
  struct stat st;
  int32_t r;
  string file;

  file_name_from_path (&file, path);
  file.str[-1] = 0;
  r = lstat (path->str[0] ? path->str : "/", &st);
  file.str[-1] = '/';

  if (r != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
	return ESTALE;
      return errno;
    }

  return ZFS_OK;
}

/* Recursively unlink the file NAME with path PATH on volume with ID == VID.  */

static bool
recursive_unlink_1 (string *path, string *name, uint32_t vid,
		    struct stat *parent_st)
{
  volume vol;
  internal_dentry dentry;
  zfs_fh fh;
  metadata meta;
  bool r;
  struct stat st;

  if (lstat (path->str, &st) != 0)
    return errno == ENOENT;

  if ((st.st_mode & S_IFMT) != S_IFDIR)
    {
      if (unlink (path->str) != 0)
	{
	  r = errno == ENOENT;
	  goto out;
	}
    }
  else
    {
      DIR *d;
      struct dirent *de;

      d = opendir (path->str);
      if (!d)
	{
	  r = errno == ENOENT;
	  goto out;
	}

      while ((de = readdir (d)) != NULL)
	{
	  string new_path;
	  string new_name;
	  unsigned int len;

	  /* Skip "." and "..".  */
	  if (de->d_name[0] == '.'
	      && (de->d_name[1] == 0
		  || (de->d_name[1] == '.'
		      && de->d_name[2] == 0)))
	    continue;

	  len = strlen (de->d_name);
	  append_file_name (&new_path, path, de->d_name, len);
	  new_name.str = new_path.str + new_path.len - len;
	  new_name.len = len;
	  r = recursive_unlink_1 (&new_path, &new_name, vid, &st);
	  free (new_path.str);
	  if (!r)
	    {
	      closedir (d);
	      return false;
	    }
	}
      closedir (d);

      if (rmdir (path->str) != 0)
	{
	  r = errno == ENOENT;
	  goto out;
	}
    }

  vol = volume_lookup (vid);
  if (vol)
    {
      if (!delete_metadata (vol, st.st_dev, st.st_ino,
			    parent_st->st_dev, parent_st->st_ino, name))
	vol->delete_p = true;
      zfsd_mutex_unlock (&vol->mutex);
    }

  r = true;

out:
  /* Destroy dentry associated with the file.  */
  fh.sid = this_node->id;	/* FIXME: race condition? */
  fh.vid = vid;
  fh.dev = st.st_dev;
  fh.ino = st.st_ino;
  get_metadata (volume_lookup (vid), &fh, &meta);

  zfsd_mutex_lock (&fh_mutex);
  dentry = dentry_lookup (&fh);
  if (dentry)
    internal_dentry_destroy (dentry, true);
  zfsd_mutex_unlock (&fh_mutex);

  return r;
}

/* Recursivelly unlink the file PATH on volume with ID == VID.
   SHADOW is true when the PATH is in shadow.  */

bool
recursive_unlink (string *path, uint32_t vid, bool shadow)
{
  string filename;
  struct stat parent_st;

#ifdef ENABLE_CHECKING
  if (path->str[0] != '/')
    abort ();
#endif

  if (shadow)
    {
      parent_st.st_dev = 0;
      parent_st.st_ino = 0;
    }
  else
    {
      file_name_from_path (&filename, path);
      filename.str[-1] = 0;
      if (lstat (path->str[0] ? path->str : "/", &parent_st) != 0
	  && errno != ENOENT)
	{
	  return false;
	}
      filename.str[-1] = '/';
    }

  return recursive_unlink_1 (path, &filename, vid, &parent_st);
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
#ifdef ENABLE_CHECKING
  if (pvd->vol)
    CHECK_MUTEX_LOCKED (&pvd->vol->mutex);
#endif

  vd = vd_lookup_name (pvd, name);
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
      int32_t r;
      volume vol = pvd->vol;

      zfsd_mutex_unlock (&pvd->mutex);
      r = get_volume_root_dentry (vol, dir, true);
      if (r != ZFS_OK)
	return r;
    }

  return ZFS_OK;
}

/* Check whether we can perform operation on ZFS file handle FH.
   If DENY_CONFLICT is true return error when we are trying to do an operation
   on a special file handle created because of conflict.
   If FH is a file handle of non-existing file return NON_EXIST_ERROR.  */

int32_t
validate_operation_on_zfs_fh (zfs_fh *fh, uint32_t conflict_error,
			      uint32_t non_exist_error)
{
  if (!request_from_this_node ())
    {
      if (CONFLICT_DIR_P (*fh))
	return EINVAL;
      if (NON_EXIST_FH_P (*fh))
	return EINVAL;
    }
  else
    {
      if (CONFLICT_DIR_P (*fh))
	return conflict_error;
      if (NON_EXIST_FH_P (*fh))
	return non_exist_error;
    }

  return ZFS_OK;
}

/* Convert attributes from STRUCT STAT ST to FATTR ATTR.  */

static void
fattr_from_struct_stat (fattr *attr, struct stat *st)
{
  attr->version = 0;
  attr->dev = st->st_dev;
  attr->ino = st->st_ino;
  attr->mode = st->st_mode & (S_IRWXU | S_IRWXG | S_IRWXO
			      | S_ISUID | S_ISGID | S_ISVTX);
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

/* Store the local file handle of root of volume VOL to LOCAL_FH
   and its attributes to ATTR.  */

static int32_t
get_volume_root_local (volume vol, zfs_fh *local_fh, fattr *attr,
		       metadata *meta)
{
  struct stat st;
  char *path;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  local_fh->sid = this_node->id;	/* FIXME: race condition? */
  local_fh->vid = vol->id;

  path = xstrdup (vol->local_path.str);
  zfsd_mutex_unlock (&vol->mutex);
  if (stat (path, &st) != 0)
    {
      free (path);
      return errno;
    }
  free (path);

  if ((st.st_mode & S_IFMT) != S_IFDIR)
    return ENOTDIR;

  local_fh->dev = st.st_dev;
  local_fh->ino = st.st_ino;
  get_metadata (volume_lookup (local_fh->vid), local_fh, meta);
  fattr_from_struct_stat (attr, &st);

  return ZFS_OK;
}

/* Store the remote file handle of root of volume VOL to REMOTE_FH
   and its attributes to ATTR.  */

int32_t
get_volume_root_remote (volume vol, zfs_fh *remote_fh, fattr *attr)
{
  volume_root_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  args.vid = vol->id;

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_volume_root_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_zfs_fh (t->dc_reply, remote_fh)
	  || !decode_fattr (t->dc_reply, attr)
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

  if (r == ZFS_OK && attr->type != FT_DIR)
    return ENOTDIR;
  return r;
}

/* Update root of volume VOL, create an internal file handle for it and store
   it to IFH.  */

int32_t
get_volume_root_dentry (volume vol, internal_dentry *dentry,
			bool unlock_fh_mutex)
{
  zfs_fh local_fh, master_fh;
  metadata meta;
  uint32_t vid;
  fattr attr;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  vid = vol->id;

  if (vol->delete_p)
    {
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_lock (&fh_mutex);
      vol = volume_lookup (vid);
      if (vol)
	volume_delete (vol);
      zfsd_mutex_unlock (&fh_mutex);
      return ENOENT;
    }

  if (vol->local_path.str)
    {
      r = get_volume_root_local (vol, &local_fh, &attr, &meta);
      if (r == ZFS_OK)
	{
	  zfs_fh_undefine (master_fh);
	  if (vol->master != this_node && zfs_fh_undefined (meta.master_fh))
	    {
	      fattr remote_attr;

	      zfsd_mutex_lock (&fh_mutex);
	      vol = volume_lookup (vid);
	      zfsd_mutex_unlock (&fh_mutex);
	      if (!vol)
		return ENOENT;

	      get_volume_root_remote (vol, &master_fh, &remote_attr);
	    }
	}
    }
  else if (vol->master != this_node)
    {
      r = get_volume_root_remote (vol, &master_fh, &attr);
      if (r == ZFS_OK)
	local_fh = master_fh;
    }
  else
    abort ();

  if (r != ZFS_OK)
    return r;

  zfsd_mutex_lock (&fh_mutex);
  vol = volume_lookup (vid);
  if (!vol)
    {
      zfsd_mutex_unlock (&fh_mutex);
      return ENOENT;
    }

  get_dentry (&local_fh, &master_fh, vol, NULL, &empty_string, &attr, &meta);

  if (unlock_fh_mutex)
    zfsd_mutex_unlock (&fh_mutex);

  *dentry = vol->root_dentry;
  return ZFS_OK;
}

/* Get attributes of local file PATH and store them to ATTR.  */

int32_t
local_getattr_path (fattr *attr, string *path)
{
  struct stat st;
  int32_t r;

  r = lstat (path->str, &st);
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
  string path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  build_local_path (&path, vol, dentry);
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = local_getattr_path (attr, &path);
  free (path.str);

  if (r == ENOENT || r == ENOTDIR)
    return ESTALE;

  return r;
}

/* Get attributes of remote file DENTRY on volume VOL
   and store them to ATTR.  */

int32_t
remote_getattr (fattr *attr, internal_dentry dentry, volume vol)
{
  zfs_fh args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dentry->fh->meta.master_fh))
    abort ();
#endif

  args = dentry->fh->meta.master_fh;

  release_dentry (dentry);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_getattr_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_fattr (t->dc_reply, attr)
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

/* Get attributes for file with handle FH and store them to FA.  */

int32_t
zfs_getattr (fattr *fa, zfs_fh *fh)
{
  volume vol;
  internal_dentry dentry;
  virtual_dir vd;
  zfs_fh tmp_fh;
  int32_t r, r2;

  r = validate_operation_on_zfs_fh (fh, ZFS_OK, ZFS_OK);
  if (r != ZFS_OK)
    return r;

  /* Lookup FH.  */
  r = zfs_fh_lookup (fh, &vol, &dentry, &vd, true);
  if (r == ZFS_STALE)
    {
      r = refresh_fh (fh);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup (fh, &vol, &dentry, &vd, true);
    }
  if (r != ZFS_OK)
    return r;

  if (vd)
    {
      if (vol)
	{
	  zfsd_mutex_unlock (&vd->mutex);
	  r = get_volume_root_dentry (vol, &dentry, true);
	  if (r != ZFS_OK)
	    return r;
	}
      else
	{
	  *fa = vd->attr;
	  zfsd_mutex_unlock (&vd->mutex);
	  return ZFS_OK;
	}
    }

  if (CONFLICT_DIR_P (dentry->fh->local_fh)
      || NON_EXIST_FH_P (dentry->fh->local_fh))
    {
      *fa = dentry->fh->attr;
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return ZFS_OK;
    }

  r = internal_dentry_lock (LEVEL_SHARED, &vol, &dentry, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
    {
      UPDATE_FH_IF_NEEDED (vol, dentry, tmp_fh);
      r = local_getattr (fa, dentry, vol);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_getattr (fa, dentry, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      /* Update cached file attributes.  */
      if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
	set_attr_version (fa, &dentry->fh->meta);
      dentry->fh->attr = *fa;
    }

  internal_dentry_unlock (vol, dentry);

  return ZFS_OK;
}

/* Set attributes of local file PATH according to SA,
   reget attributes and store them to FA.  */

int32_t
local_setattr_path (fattr *fa, string *path, sattr *sa)
{
  if (sa->size != (uint64_t) -1)
    {
      if (truncate (path->str, sa->size) != 0)
	return errno;
    }

  if (sa->mode != (uint32_t) -1)
    {
      sa->mode &= (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
      if (chmod (path->str, sa->mode) != 0)
	return errno;
    }

  if (sa->uid != (uint32_t) -1 || sa->gid != (uint32_t) -1)
    {
      if (lchown (path->str, map_uid_zfs2node (sa->uid),
		  map_gid_zfs2node (sa->gid)) != 0)
	return errno;
    }

  if (sa->atime != (zfs_time) -1 || sa->mtime != (zfs_time) -1)
    {
      struct utimbuf t;

      t.actime = sa->atime;
      t.modtime = sa->mtime;
      if (utime (path->str, &t) != 0)
	return errno;
    }

  return local_getattr_path (fa, path);
}

/* Set attributes of local file DENTRY on volume VOL according to SA,
   reget attributes and store them to FA.  */

int32_t
local_setattr (fattr *fa, internal_dentry dentry, sattr *sa, volume vol)
{
  string path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  build_local_path (&path, vol, dentry);
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = local_setattr_path (fa, &path, sa);
  free (path.str);

  if (r == ENOENT || r == ENOTDIR)
    return ESTALE;

  return r;
}

/* Set attributes of remote file DENTRY on volume VOL according to SA,
   reget attributes and store them to FA.  */

static int32_t
remote_setattr (fattr *fa, internal_dentry dentry, sattr *sa, volume vol)
{
  sattr_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dentry->fh->meta.master_fh))
    abort ();
#endif

  args.file = dentry->fh->meta.master_fh;
  args.attr = *sa;

  release_dentry (dentry);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_setattr_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_fattr (t->dc_reply, fa)
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

/* Set attributes of file with handle FH according to SA, reget attributes
   and store them to FA.  */

int32_t
zfs_setattr (fattr *fa, zfs_fh *fh, sattr *sa)
{
  volume vol;
  internal_dentry dentry;
  virtual_dir vd;
  zfs_fh tmp_fh;
  int32_t r, r2;

  r = validate_operation_on_zfs_fh (fh, EROFS, EROFS);
  if (r != ZFS_OK)
    return r;

  /* Lookup FH.  */
  r = zfs_fh_lookup (fh, &vol, &dentry, &vd, true);
  if (r == ZFS_STALE)
    {
      r = refresh_fh (fh);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup (fh, &vol, &dentry, &vd, true);
    }
  if (r != ZFS_OK)
    return r;

  if (vd)
    {
      if (vol)
	{
	  zfsd_mutex_unlock (&vd->mutex);
	  r = get_volume_root_dentry (vol, &dentry, true);
	  if (r != ZFS_OK)
	    return r;
	}
      else
	{
	  zfsd_mutex_unlock (&vd->mutex);
	  return EROFS;
	}
    }

  r = internal_dentry_lock (LEVEL_SHARED, &vol, &dentry, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (sa->mode != (uint32_t) -1)
    sa->mode &= (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);

  if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
    {
      UPDATE_FH_IF_NEEDED (vol, dentry, tmp_fh);
      r = local_setattr (fa, dentry, sa, vol);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_setattr (fa, dentry, sa, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      /* Update cached file attributes.  */
      if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
	set_attr_version (fa, &dentry->fh->meta);
      dentry->fh->attr = *fa;
    }

  internal_dentry_unlock (vol, dentry);

  return r;
}

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int32_t
zfs_extended_lookup (dir_op_res *res, zfs_fh *dir, char *path)
{
  string str;
  int32_t r;

  res->file = *dir;
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

int32_t
local_lookup (dir_op_res *res, internal_dentry dir, string *name, volume vol,
	      metadata *meta)
{
  string path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;

  build_local_path_name (&path, vol, dir, name);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r = parent_exists (&path);
  if (r != ZFS_OK)
    {
      free (path.str);
      return r;
    }

  r = local_getattr_path (&res->attr, &path);
  free (path.str);
  if (r != ZFS_OK)
    return r;

  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;
  get_metadata (volume_lookup (res->file.vid), &res->file, meta);

  return ZFS_OK;
}

/* Lookup file handle of remote file NAME in directory DIR on volume VOL
   and store it to FH.  */

int32_t
remote_lookup (dir_op_res *res, internal_dentry dir, string *name, volume vol)
{
  dir_op_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
#endif

  args.dir = dir->fh->meta.master_fh;
  args.name = *name;

  release_dentry (dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_lookup_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_dir_op_res (t->dc_reply, res)
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

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int32_t
zfs_lookup (dir_op_res *res, zfs_fh *dir, string *name)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  dir_op_res master_res;
  zfs_fh tmp_fh;
  metadata meta;
  int32_t r, r2;

  r = validate_operation_on_zfs_fh (dir, ZFS_OK, EINVAL);
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
      virtual_dir vd;

      CHECK_MUTEX_LOCKED (&pvd->mutex);
#ifdef ENABLE_CHECKING
      if (vol)
	CHECK_MUTEX_LOCKED (&vol->mutex);
#endif

      if (strcmp (name->str, ".") == 0)
	{
	  res->file = pvd->fh;
	  res->attr = pvd->attr;
	  if (vol)
	    zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&pvd->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  return ZFS_OK;
	}
      else if (strcmp (name->str, "..") == 0)
	{
	  vd = pvd->parent ? pvd->parent : pvd;
	  res->file = vd->fh;
	  res->attr = vd->attr;
	  if (vol)
	    zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&pvd->mutex);
	  zfsd_mutex_unlock (&vd_mutex);
	  return ZFS_OK;
	}

      vd = vd_lookup_name (pvd, name);
      if (vd)
	{
	  if (vol)
	    zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&pvd->mutex);

	  res->file = vd->fh;
	  res->attr = vd->attr;

	  if (vd->vol)
	    {
	      vol = vd->vol;
	      zfsd_mutex_lock (&volume_mutex);
	      zfsd_mutex_lock (&vol->mutex);
	      zfsd_mutex_unlock (&volume_mutex);
	      zfsd_mutex_unlock (&vd->mutex);
	      zfsd_mutex_unlock (&vd_mutex);

	      r = get_volume_root_dentry (vol, &idir, true);
	      if (r != ZFS_OK)
		{
		  /* If there was an error return the attributes
		     of virtual file.  */
		  return ZFS_OK;
		}

	      res->attr = idir->fh->attr;
	      release_dentry (idir);
	      zfsd_mutex_unlock (&vol->mutex);
	    }
	  else
	    {
	      zfsd_mutex_unlock (&vd_mutex);
	      zfsd_mutex_unlock (&vd->mutex);
	    }
	  return ZFS_OK;
	}
      zfsd_mutex_unlock (&vd_mutex);

      /* !vd */
      zfsd_mutex_unlock (&pvd->mutex);
      if (vol)
	{
	  r = get_volume_root_dentry (vol, &idir, true);
	  if (r != ZFS_OK)
	    return r;
#ifdef ENABLE_CHECKING
	  if (idir->fh->attr.type != FT_DIR)
	    abort ();
#endif
	}
      else
	return ENOENT;
    }
  else
    {
      if (idir->fh->attr.type != FT_DIR)
	{
	  release_dentry (idir);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	  return ENOTDIR;
	}

      if (strcmp (name->str, ".") == 0)
	{
	  res->file = idir->fh->local_fh;
	  res->attr = idir->fh->attr;
	  release_dentry (idir);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	  return ZFS_OK;
	}
      else if (strcmp (name->str, "..") == 0)
	{
	  if (idir->parent)
	    {
	      res->file = idir->parent->fh->local_fh;
	      res->attr = idir->parent->fh->attr;
	      release_dentry (idir);
	    }
	  else
	    {
	      release_dentry (idir);
	      /* This is safe because the virtual directory can't be destroyed
		 while volume is locked.  */
	      pvd = vol->root_vd->parent ? vol->root_vd->parent : vol->root_vd;
	      res->file = pvd->fh;
	      res->attr = pvd->attr;
	    }
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	  return ZFS_OK;
	}

      if (CONFLICT_DIR_P (idir->fh->local_fh))
	{
	  internal_dentry dentry;

	  dentry = dentry_lookup_name (idir, name);
	  if (dentry)
	    {
	      res->file = dentry->fh->local_fh;
	      res->attr = dentry->fh->attr;
	      release_dentry (dentry);
	    }
	  release_dentry (idir);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	  return dentry ? ZFS_OK : ENOENT;
	}

      zfsd_mutex_unlock (&fh_mutex);
    }

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      release_dentry (idir);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  CHECK_MUTEX_LOCKED (&idir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  r = internal_dentry_lock (LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_lookup (res, idir, name, vol, &meta);
      if (r == ZFS_OK)
	zfs_fh_undefine (master_res.file);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_lookup (res, idir, name, vol);
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
      internal_dentry dentry;

      dentry = get_dentry (&res->file, &master_res.file, vol, idir, name,
			   &res->attr, &meta);
      release_dentry (dentry);
    }

  internal_dentry_unlock (vol, idir);

  return r;
}

/* Create directory NAME in local directory DIR on volume VOL, set owner,
   group and permitions according to ATTR.  */

int32_t
local_mkdir (dir_op_res *res, internal_dentry dir, string *name, sattr *attr,
	     volume vol, metadata *meta)
{
  string path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;

  build_local_path_name (&path, vol, dir, name);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  attr->mode &= (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
  r = mkdir (path.str, attr->mode);
  if (r != 0)
    {
      free (path.str);
      if (errno == ENOENT || errno == ENOTDIR)
	return ESTALE;
      return errno;
    }

  r = local_setattr_path (&res->attr, &path, attr);
  free (path.str);
  if (r != ZFS_OK)
    return r;

  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;
  get_metadata (volume_lookup (res->file.vid), &res->file, meta);

  return ZFS_OK;
}

/* Create directory NAME in remote directory DIR on volume VOL, set owner,
   group and permitions according to ATTR.  */

int32_t
remote_mkdir (dir_op_res *res, internal_dentry dir, string *name, sattr *attr,
	      volume vol)
{
  mkdir_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
#endif

  args.where.dir = dir->fh->meta.master_fh;
  args.where.name = *name;
  args.attr = *attr;

  release_dentry (dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_mkdir_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_dir_op_res (t->dc_reply, res)
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

/* Create directory NAME in directory DIR, set owner, group and permitions
   according to ATTR.  */

int32_t
zfs_mkdir (dir_op_res *res, zfs_fh *dir, string *name, sattr *attr)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  dir_op_res master_res;
  zfs_fh tmp_fh;
  metadata meta;
  int32_t r, r2;

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
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }
  else
    zfsd_mutex_unlock (&fh_mutex);

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      release_dentry (idir);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  attr->mode &= (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
  attr->size = (uint64_t) -1;
  attr->atime = (zfs_time) -1;
  attr->mtime = (zfs_time) -1;

  r = internal_dentry_lock (LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_mkdir (res, idir, name, attr, vol, &meta);
      if (r == ZFS_OK)
	zfs_fh_undefine (master_res.file);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_mkdir (res, idir, name, attr, vol);
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
      internal_dentry dentry;

      dentry = get_dentry (&res->file, &master_res.file, vol, idir, name,
			   &res->attr, &meta);
      if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
	{
	  if (vol->master != this_node)
	    {
	      if (!add_journal_entry (vol, idir->fh, &dentry->fh->local_fh,
				      &dentry->fh->meta.master_fh, name,
				      JOURNAL_OPERATION_ADD))
		vol->delete_p = true;
	    }
	  if (!inc_local_version (vol, idir->fh))
	    vol->delete_p = true;
	  if (!set_metadata (vol, dentry->fh, dentry->fh->meta.flags,
			     dentry->fh->meta.local_version + 1, 0))
	    vol->delete_p = true;
	}
      release_dentry (dentry);
    }

  internal_dentry_unlock (vol, idir);

  return r;
}

/* Remove local directory NAME from directory DIR on volume VOL,
   store the stat structure of NAME to ST and path to PATHP.  */

static int32_t
local_rmdir (struct stat *st, string *pathp,
	     internal_dentry dir, string *name, volume vol)
{
  string path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  build_local_path_name (&path, vol, dir, name);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r = parent_exists (&path);
  if (r != ZFS_OK)
    {
      free (path.str);
      return r;
    }

  r = lstat (path.str, st);
  if (r != 0)
    {
      free (path.str);
      return errno;
    }
  r = rmdir (path.str);

  if (r != 0)
    {
      free (path.str);
      return errno;
    }

  *pathp = path;
  return ZFS_OK;
}

/* Remove remote directory NAME from directory DIR on volume VOL.  */

static int32_t
remote_rmdir (internal_dentry dir, string *name, volume vol)
{
  dir_op_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
#endif

  args.dir = dir->fh->meta.master_fh;
  args.name = *name;

  release_dentry (dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_rmdir_client (t, &args, nod, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Remove directory NAME from directory DIR.  */

int32_t
zfs_rmdir (zfs_fh *dir, string *name)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  struct stat st;
  string path;
  zfs_fh tmp_fh;
  int32_t r, r2;

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
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }
  else
    zfsd_mutex_unlock (&fh_mutex);

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      release_dentry (idir);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  r = internal_dentry_lock (LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  path.str = NULL;
  path.len = 0;
  if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_rmdir (&st, &path, idir, name, vol);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_rmdir (idir, name, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &idir, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  /* Delete the internal file handle of the deleted directory.  */
  if (r == ZFS_OK)
    {
      delete_dentry (&vol, &idir, name, &tmp_fh);

      if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
	{
	  string filename;
	  struct stat parent_st;

	  if (vol->master != this_node)
	    {
	      if (!add_journal_entry_st (vol, idir->fh, &st, name,
					 JOURNAL_OPERATION_DEL))
		vol->delete_p = true;
	    }

#ifdef ENABLE_CHECKING
	  if (path.str == NULL)
	    abort ();
#endif

	  file_name_from_path (&filename, &path);
	  filename.str[-1] = 0;
	  if (lstat (path.str[0] ? path.str : "/", &parent_st) == 0)
	    {
	      if (!delete_metadata (vol, st.st_dev, st.st_ino,
				    parent_st.st_dev, parent_st.st_ino,
				    &filename))
		vol->delete_p = true;
	    }
	  filename.str[-1] = '/';

	  if (!inc_local_version (vol, idir->fh))
	    vol->delete_p = true;
	}
    }

  if (path.str)
    free (path.str);

  internal_dentry_unlock (vol, idir);

  return r;
}

/* Rename local file FROM_NAME in directory FROM_DIR to file TO_NAME
   in directory TO_DIR on volume VOL.
   Store the stat structure of original file TO_NAME to ST_OLD
   and path to PATHP.  Store the stat structure of the new file TO_NAME
   to ST_NEW.  */

static int32_t
local_rename (struct stat *st_old, struct stat *st_new, string *pathp,
	      internal_dentry from_dir, string *from_name,
	      internal_dentry to_dir, string *to_name, volume vol)
{
  string path1, path2;
  int32_t r;

  CHECK_MUTEX_LOCKED (&from_dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&to_dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh_mutex);

  build_local_path_name (&path1, vol, from_dir, from_name);
  build_local_path_name (&path2, vol, to_dir, to_name);
  release_dentry (from_dir);
  if (to_dir->fh != from_dir->fh)
    release_dentry (to_dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r = parent_exists (&path1);
  if (r != ZFS_OK)
    {
      free (path1.str);
      free (path2.str);
      return r;
    }
  if (to_dir != from_dir)
    {
      r = parent_exists (&path2);
      if (r != ZFS_OK)
	{
	  free (path1.str);
	  free (path2.str);
	  return r;
	}
    }

  r = lstat (path1.str, st_new);
  if (r != 0)
    {
      free (path1.str);
      free (path2.str);
      return errno;
    }

  r = lstat (path2.str, st_old);
  if (r != 0)
    {
      /* PATH2 does not exist.  */
      r = rename (path1.str, path2.str);
      free (path1.str);
      free (path2.str);
      if (r != 0)
	return errno;
      pathp->str = NULL;
      pathp->len = 0;
    }
  else
    {
      /* PATH2 exists.  */
      r = rename (path1.str, path2.str);
      free (path1.str);
      if (r != 0)
	{
	  free (path2.str);
	  return errno;
	}
      *pathp = path2;
    }

  return ZFS_OK;
}

/* Rename remote file FROM_NAME in directory FROM_DIR to file TO_NAME
   in directory TO_DIR on volume VOL.  */

static int32_t
remote_rename (internal_dentry from_dir, string *from_name,
	       internal_dentry to_dir, string *to_name, volume vol)
{
  rename_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&from_dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&to_dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (from_dir->fh->meta.master_fh))
    abort ();
  if (zfs_fh_undefined (to_dir->fh->meta.master_fh))
    abort ();
#endif

  args.from.dir = from_dir->fh->meta.master_fh;
  args.from.name = *from_name;
  args.to.dir = to_dir->fh->meta.master_fh;
  args.to.name = *to_name;

  release_dentry (from_dir);
  if (to_dir->fh != from_dir->fh)
    release_dentry (to_dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_rename_client (t, &args, nod, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Rename file FROM_NAME in directory FROM_DIR to file TO_NAME
   in directory TO_DIR.  */

int32_t
zfs_rename (zfs_fh *from_dir, string *from_name,
	    zfs_fh *to_dir, string *to_name)
{
  volume vol;
  internal_dentry from_dentry, to_dentry;
  virtual_dir vd;
  struct stat st_old, st_new;
  string path;
  zfs_fh tmp_from, tmp_to;
  int32_t r, r2;

  r = validate_operation_on_zfs_fh (from_dir, EROFS, EINVAL);
  if (r != ZFS_OK)
    return r;

  r = validate_operation_on_zfs_fh (to_dir, EROFS, EINVAL);
  if (r != ZFS_OK)
    return r;

  /* Lookup TO_DIR.  */
  if (VIRTUAL_FH_P (*to_dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (to_dir, &vol, &to_dentry, &vd, true);
  if (r == ZFS_STALE)
    {
#ifdef ENABLE_CHECKING
      if (VIRTUAL_FH_P (*to_dir))
	abort ();
#endif
      r = refresh_fh (to_dir);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup_nolock (to_dir, &vol, &to_dentry, &vd, true);
    }
  if (r != ZFS_OK)
    {
      if (VIRTUAL_FH_P (*to_dir))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }

  if (vd)
    {
      r = validate_operation_on_virtual_directory (vd, to_name, &to_dentry);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }
  else
    zfsd_mutex_unlock (&fh_mutex);

  /* Hide ".zfs" in the root of the volume.  */
  if (!to_dentry->parent && strncmp (to_name->str, ".zfs", 5) == 0)
    {
      release_dentry (to_dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  tmp_to = to_dentry->fh->local_fh;
  release_dentry (to_dentry);
  zfsd_mutex_unlock (&vol->mutex);

  /* Lookup FROM_DIR.  */
  if (VIRTUAL_FH_P (*from_dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (from_dir, &vol, &from_dentry, &vd, true);
  if (r == ZFS_STALE)
    {
#ifdef ENABLE_CHECKING
      if (VIRTUAL_FH_P (*from_dir))
	abort ();
#endif
      r = refresh_fh (from_dir);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup_nolock (from_dir, &vol, &from_dentry, &vd, true);
    }
  if (r != ZFS_OK)
    {
      if (VIRTUAL_FH_P (*from_dir))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }

  if (vd)
    {
      r = validate_operation_on_virtual_directory (vd, from_name, &from_dentry);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }
  else
    zfsd_mutex_unlock (&fh_mutex);

  /* Hide ".zfs" in the root of the volume.  */
  if (!from_dentry->parent && strncmp (from_name->str, ".zfs", 5) == 0)
    {
      release_dentry (from_dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  tmp_from = from_dentry->fh->local_fh;
  release_dentry (from_dentry);
  zfsd_mutex_unlock (&vol->mutex);

  /* FROM_DIR and TO_DIR must be on same device.  */
  if (tmp_from.dev != tmp_to.dev
      || tmp_from.vid != tmp_to.vid
      || tmp_from.sid != tmp_to.sid)
    return EXDEV;

  /* Lookup dentries.  */
  r = zfs_fh_lookup_nolock (&tmp_from, &vol, &from_dentry, NULL, true);
  if (r != ZFS_OK)
    return r;

  if (tmp_from.ino != tmp_to.ino)
    {
      to_dentry = dentry_lookup (&tmp_to);
      if (!to_dentry)
	{
	  release_dentry (from_dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	  return ESTALE;
	}
    }
  else
    to_dentry = from_dentry;

  /* Check whether we are not moving a directory into its subdirectory.  */
  if (from_dentry != to_dentry)
    {
      internal_dentry tmp;

      for (tmp = to_dentry; tmp; tmp = tmp->parent)
	if (tmp->parent == from_dentry
	    && strcmp (tmp->name.str, from_name->str) == 0)
	  {
	    release_dentry (from_dentry);
	    release_dentry (to_dentry);
	    zfsd_mutex_unlock (&vol->mutex);
	    zfsd_mutex_unlock (&fh_mutex);
	    return EINVAL;
	  }
      if (from_dentry->parent == to_dentry
	  && strcmp (from_dentry->name.str, to_name->str) == 0)
	{
	  release_dentry (from_dentry);
	  release_dentry (to_dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	  return ENOTEMPTY;
	}
    }

  zfsd_mutex_unlock (&fh_mutex);

  r = internal_dentry_lock2 (LEVEL_EXCLUSIVE, LEVEL_EXCLUSIVE, &vol,
			     &from_dentry, &to_dentry, &tmp_from, &tmp_to);
  if (r != ZFS_OK)
    return r;

  path.str = NULL;
  path.len = 0;
  if (INTERNAL_FH_HAS_LOCAL_PATH (from_dentry->fh))
    {
      UPDATE_FH_IF_NEEDED_2 (vol, to_dentry, from_dentry, tmp_to, tmp_from);
      if (tmp_from.ino != tmp_to.ino)
	UPDATE_FH_IF_NEEDED_2 (vol, from_dentry, to_dentry, tmp_from, tmp_to);
      r = local_rename (&st_old, &st_new, &path, from_dentry, from_name,
			to_dentry, to_name, vol);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_rename (from_dentry, from_name, to_dentry, to_name, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_to, &vol, &to_dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      delete_dentry (&vol, &to_dentry, to_name, &tmp_to);

      if (tmp_from.ino != tmp_to.ino)
	{
	  from_dentry = dentry_lookup (&tmp_from);
#ifdef ENABLE_CHECKING
	  if (!from_dentry)
	    abort ();
#endif
	}
      else
	from_dentry = to_dentry;

      /* Move the dentry if it exists.  */
      internal_dentry_move (vol, from_dentry, from_name, to_dentry,
			    to_name);

      if (INTERNAL_FH_HAS_LOCAL_PATH (from_dentry->fh))
	{
	  zfs_fh fh;

	  if (path.str)
	    {
	      string filename;
	      struct stat parent_st;

	      if (vol->master != this_node)
		{
		  if (!add_journal_entry_st (vol, to_dentry->fh, &st_old,
					     to_name, JOURNAL_OPERATION_DEL))
		    vol->delete_p = true;
		}

	      file_name_from_path (&filename, &path);
	      filename.str[-1] = 0;
	      if (lstat (path.str[0] ? path.str : "/", &parent_st) == 0)
		{
		  if (!delete_metadata (vol, st_old.st_dev, st_old.st_ino,
					parent_st.st_dev, parent_st.st_ino,
					&filename))
		    vol->delete_p = true;
		}
	      filename.str[-1] = '/';
	    }

	  fh.dev = st_new.st_dev;
	  fh.ino = st_new.st_ino;
	  if (!metadata_hardlink_replace (vol, &fh,
					  from_dentry->fh->local_fh.dev,
					  from_dentry->fh->local_fh.ino,
					  from_name,
					  to_dentry->fh->local_fh.dev,
					  to_dentry->fh->local_fh.ino,
					  to_name))
	    vol->delete_p = true;

	  if (vol->master != this_node)
	    {
	      if (!add_journal_entry_st (vol, from_dentry->fh, &st_new,
					 from_name, JOURNAL_OPERATION_DEL))
		vol->delete_p = true;
	      if (!add_journal_entry_st (vol, to_dentry->fh, &st_new,
					 to_name, JOURNAL_OPERATION_ADD))
		vol->delete_p = true;
	    }

	  if (!inc_local_version (vol, from_dentry->fh))
	    vol->delete_p = true;
	  if (!inc_local_version (vol, to_dentry->fh))
	    vol->delete_p = true;
	}

      if (to_dentry != from_dentry)
	release_dentry (from_dentry);
    }

  if (path.str)
    free (path.str);

  internal_dentry_unlock (vol, to_dentry);
  if (tmp_from.ino != tmp_to.ino)
    {
      r2 = zfs_fh_lookup_nolock (&tmp_from, &vol, &from_dentry, NULL, false);
      if (r2 == ZFS_OK)
	internal_dentry_unlock (vol, from_dentry);
    }

  return r;
}

/* Link local file FROM to be a file with NAME in directory DIR
   on volume VOL.  */

static int32_t
local_link (internal_dentry from, internal_dentry dir, string *name, volume vol)
{
  struct stat st;
  string path1, path2;
  int32_t r;

  CHECK_MUTEX_LOCKED (&from->fh->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh_mutex);

  build_local_path (&path1, vol, from);
  build_local_path_name (&path2, vol, dir, name);
  release_dentry (from);
  if (dir->fh != from->fh)
    release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r = lstat (path1.str, &st);
  if (r != 0)
    {
      free (path1.str);
      free (path2.str);
      if (errno == ENOENT || errno == ENOTDIR)
	return ESTALE;
      return errno;
    }

  r = link (path1.str, path2.str);
  free (path1.str);
  free (path2.str);
  if (r != 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
	return ESTALE;
      return errno;
    }

  return ZFS_OK;
}

/* Link remote file FROM to be a file with NAME in directory DIR
   on volume VOL.  */

static int32_t
remote_link (internal_dentry from, internal_dentry dir, string *name, volume vol)
{
  link_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&from->fh->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (from->fh->meta.master_fh))
    abort ();
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
#endif

  args.from = from->fh->meta.master_fh;
  args.to.dir = dir->fh->meta.master_fh;
  args.to.name = *name;

  release_dentry (from);
  if (dir->fh != from->fh)
    release_dentry (dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_link_client (t, &args, nod, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Link file FROM to be a file with NAME in directory DIR.  */

int32_t
zfs_link (zfs_fh *from, zfs_fh *dir, string *name)
{
  volume vol;
  internal_dentry from_dentry, dir_dentry;
  virtual_dir vd;
  zfs_fh tmp_from, tmp_dir;
  int32_t r, r2;

  if (VIRTUAL_FH_P (*from))
    return EROFS;

  r = validate_operation_on_zfs_fh (from, EROFS, EROFS);
  if (r != ZFS_OK)
    return r;

  r = validate_operation_on_zfs_fh (dir, EROFS, EINVAL);
  if (r != ZFS_OK)
    return r;

  /* Lookup FROM.  */
  r = zfs_fh_lookup (from, &vol, &from_dentry, NULL, true);
  if (r == ZFS_STALE)
    {
      r = refresh_fh (from);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup (from, &vol, &from_dentry, NULL, true);
    }
  if (r != ZFS_OK)
    return r;

  if (from_dentry->fh->attr.type == FT_DIR)
    {
      /* Can't link a directory.  */
      release_dentry (from_dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return EPERM;
    }

  tmp_from = from_dentry->fh->local_fh;
  release_dentry (from_dentry);
  zfsd_mutex_unlock (&vol->mutex);

  /* Lookup DIR.  */
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &dir_dentry, &vd, true);
  if (r == ZFS_STALE)
    {
#ifdef ENABLE_CHECKING
      if (VIRTUAL_FH_P (*dir))
	abort ();
#endif
      r = refresh_fh (dir);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup_nolock (dir, &vol, &dir_dentry, &vd, true);
    }
  if (r != ZFS_OK)
    {
      if (VIRTUAL_FH_P (*dir))
	zfsd_mutex_unlock (&vd_mutex);
      return r;
    }

  if (vd)
    {
      r = validate_operation_on_virtual_directory (vd, name, &dir_dentry);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }
  else
    zfsd_mutex_unlock (&fh_mutex);

  /* Hide ".zfs" in the root of the volume.  */
  if (!dir_dentry->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      release_dentry (dir_dentry);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  tmp_dir = dir_dentry->fh->local_fh;
  release_dentry (dir_dentry);
  zfsd_mutex_unlock (&vol->mutex);

  /* FROM and DIR must be on same device.  */
  if (tmp_from.dev != tmp_dir.dev
      || tmp_from.vid != tmp_dir.vid
      || tmp_from.sid != tmp_dir.sid)
    return EXDEV;

  /* Lookup dentries.  */
  r = zfs_fh_lookup_nolock (&tmp_from, &vol, &from_dentry, NULL, true);
  if (r != ZFS_OK)
    return r;

  if (tmp_from.ino != tmp_dir.ino)
    {
      dir_dentry = dentry_lookup (&tmp_dir);
      if (!dir_dentry)
	{
	  release_dentry (from_dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	  return ZFS_STALE;
	}
    }
  else
    dir_dentry = from_dentry;

  zfsd_mutex_unlock (&fh_mutex);

  r = internal_dentry_lock2 (LEVEL_EXCLUSIVE, LEVEL_EXCLUSIVE, &vol,
			     &from_dentry, &dir_dentry, &tmp_from, &tmp_dir);
  if (r != ZFS_OK)
    return r;

  if (INTERNAL_FH_HAS_LOCAL_PATH (from_dentry->fh))
    {
      UPDATE_FH_IF_NEEDED_2 (vol, dir_dentry, from_dentry, tmp_dir, tmp_from);
      if (tmp_from.ino != tmp_dir.ino)
	UPDATE_FH_IF_NEEDED_2 (vol, from_dentry, dir_dentry, tmp_from, tmp_dir);
      r = local_link (from_dentry, dir_dentry, name, vol);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_link (from_dentry, dir_dentry, name, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_dir, &vol, &dir_dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      delete_dentry (&vol, &dir_dentry, name, &tmp_dir);

      if (tmp_from.ino != tmp_dir.ino)
	{
	  from_dentry = dentry_lookup (&tmp_from);
#ifdef ENABLE_CHECKING
	  if (!from_dentry)
	    abort ();
#endif
	}
      else
	from_dentry = dir_dentry;

      internal_dentry_link (from_dentry, vol, dir_dentry, name);

      if (INTERNAL_FH_HAS_LOCAL_PATH (dir_dentry->fh))
	{
	  if (!metadata_hardlink_insert (vol, &from_dentry->fh->local_fh,
					 dir_dentry->fh->local_fh.dev,
					 dir_dentry->fh->local_fh.ino, name))
	    vol->delete_p = true;
	  if (vol->master != this_node)
	    {
	      if (!add_journal_entry (vol, dir_dentry->fh,
				      &from_dentry->fh->local_fh,
				      &from_dentry->fh->meta.master_fh,
				      name, JOURNAL_OPERATION_ADD))
		vol->delete_p = true;
	    }
	  if (!inc_local_version (vol, dir_dentry->fh))
	    vol->delete_p = true;
	}

      if (dir_dentry != from_dentry)
	release_dentry (from_dentry);
    }

  internal_dentry_unlock (vol, dir_dentry);
  if (tmp_from.ino != tmp_dir.ino)
    {
      r2 = zfs_fh_lookup_nolock (&tmp_from, &vol, &from_dentry, NULL, false);
      if (r2 == ZFS_OK)
	internal_dentry_unlock (vol, from_dentry);
    }

  return r;
}

/* Delete local file NAME from directory DIR on volume VOL.
   Store the stat structure of NAME to ST and path to PATHP.  */

static int32_t
local_unlink (struct stat *st, string *pathp,
	      internal_dentry dir, string *name, volume vol)
{
  string path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  build_local_path_name (&path, vol, dir, name);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r = parent_exists (&path);
  if (r != ZFS_OK)
    {
      free (path.str);
      return r;
    }

  r = lstat (path.str, st);
  if (r != 0)
    {
      free (path.str);
      return errno;
    }
  r = unlink (path.str);

  if (r != 0)
    {
      free (path.str);
      return errno;
    }

  *pathp = path;
  return ZFS_OK;
}

/* Delete remote file NAME from directory DIR on volume VOL.  */

static int32_t
remote_unlink (internal_dentry dir, string *name, volume vol)
{
  dir_op_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
#endif

  args.dir = dir->fh->meta.master_fh;
  args.name = *name;

  release_dentry (dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_unlink_client (t, &args, nod, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Remove directory NAME from directory DIR.  */

int32_t
zfs_unlink (zfs_fh *dir, string *name)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  struct stat st;
  string path;
  zfs_fh tmp_fh;
  int32_t r, r2;

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
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }
  else
    zfsd_mutex_unlock (&fh_mutex);

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      release_dentry (idir);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  r = internal_dentry_lock (LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  path.str = NULL;
  path.len = 0;
  if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_unlink (&st, &path, idir, name, vol);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_unlink (idir, name, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &idir, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  /* Delete the internal file handle of the deleted directory.  */
  if (r == ZFS_OK)
    {
      delete_dentry (&vol, &idir, name, &tmp_fh);

      if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
	{
	  string filename;
	  struct stat parent_st;

	  if (vol->master != this_node)
	    {
	      if (!add_journal_entry_st (vol, idir->fh, &st, name,
					 JOURNAL_OPERATION_DEL))
		vol->delete_p = true;
	    }

#ifdef ENABLE_CHECKING
	  if (path.str == NULL)
	    abort ();
#endif

	  file_name_from_path (&filename, &path);
	  filename.str[-1] = 0;
	  if (lstat (path.str[0] ? path.str : "/", &parent_st) == 0)
	    {
	      if (!delete_metadata (vol, st.st_dev, st.st_ino,
				    parent_st.st_dev, parent_st.st_ino,
				    &filename))
		vol->delete_p = true;
	    }
	  filename.str[-1] = '/';

	  if (!inc_local_version (vol, idir->fh))
	    vol->delete_p = true;
	}
    }

  if (path.str)
    free (path.str);

  internal_dentry_unlock (vol, idir);

  return r;
}

/* Read local symlink FILE on volume VOL.  */

int32_t
local_readlink (read_link_res *res, internal_dentry file, volume vol)
{
  string path;
  char buf[ZFS_MAXDATA + 1];
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&file->fh->mutex);

  build_local_path (&path, vol, file);
  release_dentry (file);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = readlink (path.str, buf, ZFS_MAXDATA);
  free (path.str);
  if (r < 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
	return ESTALE;
      return errno;
    }

  buf[r] = 0;
  res->path.str = (char *) xmemdup (buf, r + 1);
  res->path.len = r;

  return ZFS_OK;
}

/* Read local symlink NAME in directroy DIR on volume VOL.  */

int32_t
local_readlink_name (read_link_res *res, internal_dentry dir, string *name,
		     volume vol)
{
  string path;
  char buf[ZFS_MAXDATA + 1];
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  build_local_path_name (&path, vol, dir, name);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = readlink (path.str, buf, ZFS_MAXDATA);
  free (path.str);
  if (r < 0)
    {
      if (errno == ENOENT || errno == ENOTDIR)
	return ESTALE;
      return errno;
    }

  buf[r] = 0;
  res->path.str = (char *) xmemdup (buf, r + 1);
  res->path.len = r;

  return ZFS_OK;
}

/* Read remote symlink FILE on volume VOL.  */

int32_t
remote_readlink (read_link_res *res, internal_dentry file, volume vol)
{
  zfs_fh args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&file->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (file->fh->meta.master_fh))
    abort ();
#endif

  args = file->fh->meta.master_fh;

  release_dentry (file);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_readlink_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_zfs_path (t->dc_reply, &res->path)
	  || !finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
      else
	xstringdup (&res->path, &res->path);
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

/* Read remote symlink FH on volume VOL.  */

int32_t
remote_readlink_zfs_fh (read_link_res *res, zfs_fh *fh, volume vol)
{
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_readlink_client (t, fh, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_zfs_path (t->dc_reply, &res->path)
	  || !finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
      else
	xstringdup (&res->path, &res->path);
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

/* Read symlink FH.  */

int32_t
zfs_readlink (read_link_res *res, zfs_fh *fh)
{
  volume vol;
  internal_dentry dentry;
  zfs_fh tmp_fh;
  int32_t r, r2;

  if (VIRTUAL_FH_P (*fh) || CONFLICT_DIR_P (*fh))
    return EINVAL;

  if (NON_EXIST_FH_P (*fh))
    {
      node nod;

      nod = node_lookup (fh->ino);
      if (!nod)
	return ESTALE;

      xstringdup (&res->path, &nod->name);
      zfsd_mutex_unlock (&nod->mutex);

      return ZFS_OK;
    }

  /* Lookup FH.  */
  r = zfs_fh_lookup (fh, &vol, &dentry, NULL, true);
  if (r == ZFS_STALE)
    {
      r = refresh_fh (fh);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup (fh, &vol, &dentry, NULL, true);
    }
  if (r != ZFS_OK)
    return r;

  r = internal_dentry_lock (LEVEL_SHARED, &vol, &dentry, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (INTERNAL_FH_HAS_LOCAL_PATH (dentry->fh))
    r = local_readlink (res, dentry, vol);
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_readlink (res, dentry, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &dentry, NULL, false);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  internal_dentry_unlock (vol, dentry);

  return r;
}

/* Create local symlink NAME in directory DIR on volume VOL pointing to TO,
   set its attributes according to ATTR.  */

int32_t
local_symlink (dir_op_res *res, internal_dentry dir, string *name, string *to,
	       sattr *attr, volume vol, metadata *meta)
{
  string path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;

  build_local_path_name (&path, vol, dir, name);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  r = parent_exists (&path);
  if (r != ZFS_OK)
    {
      free (path.str);
      return r;
    }

  r = symlink (to->str, path.str);
  if (r != 0)
    {
      free (path.str);
      return errno;
    }

  r = local_setattr_path (&res->attr, &path, attr);
  free (path.str);
  if (r != ZFS_OK)
    return r;

  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;
  get_metadata (volume_lookup (res->file.vid), &res->file, meta);

  return ZFS_OK;
}

/* Create remote symlink NAME in directory DIR on volume VOL pointing to TO,
   set its attributes according to ATTR.  */

int32_t
remote_symlink (dir_op_res *res, internal_dentry dir, string *name, string *to,
		sattr *attr, volume vol)
{
  symlink_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
#endif

  args.from.dir = dir->fh->meta.master_fh;
  args.from.name = *name;
  args.to = *to;
  args.attr = *attr;

  release_dentry (dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_symlink_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_dir_op_res (t->dc_reply, res)
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

/* Create symlink NAME in directory DIR pointing to TO,
   set its attributes according to ATTR.  */

int32_t
zfs_symlink (dir_op_res *res, zfs_fh *dir, string *name, string *to,
	     sattr *attr)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  dir_op_res master_res;
  zfs_fh tmp_fh;
  metadata meta;
  int32_t r, r2;

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
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }
  else
    zfsd_mutex_unlock (&fh_mutex);

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      release_dentry (idir);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  attr->mode = (uint32_t) -1;
  attr->size = (uint64_t) -1;
  attr->atime = (zfs_time) -1;
  attr->mtime = (zfs_time) -1;

  r = internal_dentry_lock (LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_symlink (res, idir, name, to, attr, vol, &meta);
      if (r == ZFS_OK)
	zfs_fh_undefine (master_res.file);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_symlink (res, idir, name, to, attr, vol);
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
      internal_dentry dentry;

      dentry = get_dentry (&res->file, &master_res.file, vol, idir, name,
			   &res->attr, &meta);
      if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
	{
	  if (vol->master != this_node)
	    {
	      if (!add_journal_entry (vol, idir->fh, &dentry->fh->local_fh,
				      &dentry->fh->meta.master_fh, name,
				      JOURNAL_OPERATION_ADD))
		vol->delete_p = true;
	    }
	  if (!inc_local_version (vol, idir->fh))
	    vol->delete_p = true;
	  if (!set_metadata (vol, dentry->fh, dentry->fh->meta.flags,
			     dentry->fh->meta.local_version + 1, 0))
	    vol->delete_p = true;
	}
      release_dentry (dentry);
    }

  internal_dentry_unlock (vol, idir);

  return r;
}

/* Create local special file NAME of type TYPE in directory DIR,
   set the attributes according to ATTR.
   If device is being created RDEV is its number.  */

int32_t
local_mknod (dir_op_res *res, internal_dentry dir, string *name, sattr *attr,
	     ftype type, uint32_t rdev, volume vol, metadata *meta)
{
  string path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;

  build_local_path_name (&path, vol, dir, name);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  attr->mode &= (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
  r = mknod (path.str, attr->mode | ftype2mode[type], rdev);
  if (r != 0)
    {
      free (path.str);
      if (errno == ENOENT || errno == ENOTDIR)
	return ESTALE;
      return errno;
    }

  r = local_setattr_path (&res->attr, &path, attr);
  free (path.str);
  if (r != ZFS_OK)
    return r;

  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;
  get_metadata (volume_lookup (res->file.vid), &res->file, meta);

  return ZFS_OK;
}

/* Create remote special file NAME of type TYPE in directory DIR,
   set the attributes according to ATTR.
   If device is being created RDEV is its number.  */

int32_t
remote_mknod (dir_op_res *res, internal_dentry dir, string *name, sattr *attr,
	      ftype type, uint32_t rdev, volume vol)
{
  mknod_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
#endif

  args.where.dir = dir->fh->meta.master_fh;
  args.where.name = *name;
  args.attr = *attr;
  args.type = type;
  args.rdev = rdev;

  release_dentry (dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_mknod_client (t, &args, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_dir_op_res (t->dc_reply, res)
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

/* Create special file NAME of type TYPE in directory DIR,
   set the attributes according to ATTR.
   If device is being created RDEV is its number.  */

int32_t
zfs_mknod (dir_op_res *res, zfs_fh *dir, string *name, sattr *attr, ftype type,
	   uint32_t rdev)
{
  volume vol;
  internal_dentry idir;
  virtual_dir pvd;
  dir_op_res master_res;
  zfs_fh tmp_fh;
  metadata meta;
  int32_t r, r2;

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
      r = validate_operation_on_virtual_directory (pvd, name, &idir);
      zfsd_mutex_unlock (&vd_mutex);
      if (r != ZFS_OK)
	return r;
    }
  else
    zfsd_mutex_unlock (&fh_mutex);

  /* Hide ".zfs" in the root of the volume.  */
  if (!idir->parent && strncmp (name->str, ".zfs", 5) == 0)
    {
      release_dentry (idir);
      zfsd_mutex_unlock (&vol->mutex);
      return EACCES;
    }

  attr->mode &= (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
  attr->size = (uint64_t) -1;
  attr->atime = (zfs_time) -1;
  attr->mtime = (zfs_time) -1;

  r = internal_dentry_lock (LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_mknod (res, idir, name, attr, type, rdev, vol, &meta);
      if (r == ZFS_OK)
	zfs_fh_undefine (master_res.file);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_mknod (res, idir, name, attr, type, rdev, vol);
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
      internal_dentry dentry;

      dentry = get_dentry (&res->file, &master_res.file, vol, idir, name,
			   &res->attr, &meta);
      if (INTERNAL_FH_HAS_LOCAL_PATH (idir->fh))
	{
	  if (vol->master != this_node)
	    {
	      if (!add_journal_entry (vol, idir->fh, &dentry->fh->local_fh,
				      &dentry->fh->meta.master_fh, name,
				      JOURNAL_OPERATION_ADD))
		vol->delete_p = true;
	    }
	  if (!inc_local_version (vol, idir->fh))
	    vol->delete_p = true;
	  if (!set_metadata (vol, dentry->fh, dentry->fh->meta.flags,
			     dentry->fh->meta.local_version + 1, 0))
	    vol->delete_p = true;
	}
      release_dentry (dentry);
    }

  internal_dentry_unlock (vol, idir);

  return r;
}

/* Check whether local file FH on volume VOL exists.  */

int32_t
local_file_info (file_info_res *res, zfs_fh *fh, volume vol)
{
  string path;

  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (!vol->local_path.str)
    abort ();
#endif

  get_local_path_from_metadata (&path, vol, fh);
  if (!path.str)
    return ESTALE;

  local_path_to_relative_path (&res->path, vol, &path);
  free (path.str);

  return ZFS_OK;
}

/* Check whether remote file for FH on volume VOL exists.  */

int32_t
remote_file_info (file_info_res *res, zfs_fh *fh, volume vol)
{
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (*fh))
    abort ();
#endif

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_file_info_client (t, fh, nod, &fd);

  if (r == ZFS_OK)
    {
      if (!decode_zfs_path (t->dc_reply, &res->path)
	  || !finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
      else
	res->path.str = (char *) xmemdup (res->path.str, res->path.len + 1);
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

/* Check whether local file FH exists.  */

int32_t
zfs_file_info (file_info_res *res, zfs_fh *fh)
{
  volume vol;
  internal_dentry dentry;
  zfs_fh tmp_fh;
  int32_t r;

  if (!REGULAR_FH_P (*fh))
    return EINVAL;

  vol = volume_lookup (fh->vid);
  if (!vol)
    return ESTALE;

  if (vol->local_path.str)
    {
      r = local_file_info (res, fh, vol);
      zfsd_mutex_unlock (&vol->mutex);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&vol->mutex);

      r = zfs_fh_lookup (fh, &vol, &dentry, NULL, true);
      if (r != ZFS_OK)
	return r;

      tmp_fh = dentry->fh->meta.master_fh;
      release_dentry (dentry);
      r = remote_file_info (res, &tmp_fh, vol);
    }

  return r;
}

/* Name the local file handle FH as NAME in directory DIR on volume VOL
   by moving the file or linking it.  */

int32_t
local_reintegrate_add (volume vol, internal_dentry dir, string *name,
		       zfs_fh *fh)
{
  metadata meta;
  int32_t r;
  unsigned int n;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  n = metadata_n_hardlinks (vol, fh, &meta);
  if (n == 0)
    {
      release_dentry (dir);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&fh_mutex);
      return ENOENT;
    }

  if (meta.flags & METADATA_SHADOW)
    {
      if (!move_from_shadow (vol, fh, dir, name))
	return ZFS_UPDATE_FAILED;
    }
  else
    {
      string old_path, new_path;
      string old_name, new_name;
      fattr attr;
      uint32_t vid;
      uint32_t new_parent_dev;
      uint32_t new_parent_ino;
      struct stat old_parent_st;

      build_local_path_name (&new_path, vol, dir, name);
      vid = vol->id;
      new_parent_dev = dir->fh->local_fh.dev;
      new_parent_ino = dir->fh->local_fh.ino;
      release_dentry (dir);
      zfsd_mutex_unlock (&fh_mutex);

      get_local_path_from_metadata (&old_path, vol, fh);
      zfsd_mutex_unlock (&vol->mutex);
      if (!old_path.str)
	{
	  free (new_path.str);
	  return ENOENT;
	}

      if (!recursive_unlink (&new_path, vid, false))
	{
	  free (old_path.str);
	  free (new_path.str);
	  return ZFS_UPDATE_FAILED;
	}

      r = local_getattr_path (&attr, &old_path);
      if (r != ZFS_OK)
	{
	  free (old_path.str);
	  free (new_path.str);
	  return r;
	}

      file_name_from_path (&new_name, &new_path);
      if (attr.type == FT_DIR)
	{
	  file_name_from_path (&old_name, &old_path);
	  old_name.str[-1] = 0;
	  if (lstat (old_path.str[0] ? old_path.str : "/", &old_parent_st) != 0)
	    {
	      free (old_path.str);
	      free (new_path.str);
	      return errno;
	    }
	  old_name.str[-1] = '/';

	  if (rename (old_path.str, new_path.str) != 0)
	    {
	      free (old_path.str);
	      free (new_path.str);
	      return errno;
	    }

	  vol = volume_lookup (vid);
	  if (!vol)
	    {
	      free (old_path.str);
	      free (new_path.str);
	      return ESTALE;
	    }

	  if (!metadata_hardlink_replace (vol, fh, old_parent_st.st_dev,
					  old_parent_st.st_ino, &old_name,
					  new_parent_dev, new_parent_ino,
					  &new_name))
	    {
	      vol->delete_p = true;
	      zfsd_mutex_unlock (&vol->mutex);
	      free (old_path.str);
	      free (new_path.str);
	      return ZFS_UPDATE_FAILED;
	    }
	  zfsd_mutex_unlock (&vol->mutex);
	  free (old_path.str);
	  free (new_path.str);
	}
      else
	{
	  if (link (old_path.str, new_path.str) != 0)
	    {
	      free (old_path.str);
	      free (new_path.str);
	      return errno;
	    }

	  vol = volume_lookup (vid);
	  if (!vol)
	    {
	      free (old_path.str);
	      free (new_path.str);
	      return ENOENT;
	    }

	  if (!metadata_hardlink_insert (vol, fh, new_parent_dev,
					 new_parent_ino, &new_name))
	    {
	      vol->delete_p = true;
	      zfsd_mutex_unlock (&vol->mutex);
	      free (old_path.str);
	      free (new_path.str);
	      return ZFS_UPDATE_FAILED;
	    }
	  zfsd_mutex_unlock (&vol->mutex);
	  free (old_path.str);
	  free (new_path.str);
	}
    }

  return ZFS_OK;
}

/* Name the remote file handle FH as NAME in directory DIR on volume VOL
   by moving the file or linking it.  */

int32_t
remote_reintegrate_add (volume vol, internal_dentry dir, string *name,
			zfs_fh *fh)
{
  reintegrate_add_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
#endif

  args.fh = *fh;
  args.dir = dir->fh->meta.master_fh;
  args.name = *name;

  release_dentry (dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_reintegrate_add_client (t, &args, nod, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Name the file handle FH as NAME in directory DIR
   by moving the file or linking it.  */

int32_t
zfs_reintegrate_add (zfs_fh *fh, zfs_fh *dir, string *name)
{
  volume vol;
  internal_dentry idir, dentry;
  int32_t r;

  if (!REGULAR_FH_P (*fh))
    return EINVAL;

  if (!REGULAR_FH_P (*dir))
    return EINVAL;

  r = zfs_fh_lookup_nolock (dir, &vol, &idir, NULL, true);
  if (r == ZFS_STALE)
    {
      r = refresh_fh (dir);
      if (r != ZFS_OK)
	return r;
      r = zfs_fh_lookup_nolock (dir, &vol, &idir, NULL, true);
    }
  if (r != ZFS_OK)
    return r;

  if (vol->local_path.str)
    r = local_reintegrate_add (vol, idir, name, fh);
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_reintegrate_add (vol, idir, name, fh);
    }
  else
    abort ();

  zfsd_mutex_lock (&fh_mutex);
  idir = dentry_lookup (dir);
  if (idir)
    {
      dentry = dentry_lookup_name (idir, name);
      release_dentry (idir);
      if (dentry)
	internal_dentry_destroy (dentry, true);
    }
  dentry = dentry_lookup (fh);
  if (dentry)
    internal_dentry_destroy (dentry, true);
  zfsd_mutex_unlock (&fh_mutex);

  return r;
}

/* If DESTROY_P delete local file DENTRY and its subtree,
   otherwise move it to shadow.  */

int32_t
local_reintegrate_del (volume vol, internal_dentry dentry, bool destroy_p)
{
  metadata meta;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  if (destroy_p
      || metadata_n_hardlinks (vol, &dentry->fh->local_fh, &meta) > 1)
    {
      if (!delete_tree (dentry, vol))
	return ZFS_UPDATE_FAILED;
    }
  else
    {
      if (!move_to_shadow (vol, dentry))
	return ZFS_UPDATE_FAILED;
    }

  return ZFS_OK;
}

/* If DESTROY_P delete remote file NAME and its subtree from directory DIR,
   otherwise move it to shadow.  */

int32_t
remote_reintegrate_del (volume vol, internal_dentry dir, string *name,
			bool destroy_p)
{
  reintegrate_del_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
#ifdef ENABLE_CHECKING
  if (zfs_fh_undefined (dir->fh->meta.master_fh))
    abort ();
#endif

  args.dir = dir->fh->meta.master_fh;
  args.name = *name;
  args.destroy_p = destroy_p;

  release_dentry (dir);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_reintegrate_del_client (t, &args, nod, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* If DESTROY_P delete file NAME and its subtree from directory DIR,
   otherwise move it to shadow.  */

int32_t
zfs_reintegrate_del (zfs_fh *dir, string *name, bool destroy_p)
{
  dir_op_res res;
  volume vol;
  internal_dentry idir, dentry;
  int32_t r, r2;

  if (!REGULAR_FH_P (*dir))
    return EINVAL;

  r = zfs_lookup (&res, dir, name);
  if (r != ZFS_OK)
    return r;

  r = zfs_fh_lookup_nolock (dir, &vol, &idir, NULL, true);
  if (r != ZFS_OK)
    return r;

  if (vol->local_path.str)
    {
      dentry = dentry_lookup_name (idir, name);
      release_dentry (idir);
      if (!dentry)
	{
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);
	  return ESTALE;
	}

      r = local_reintegrate_del (vol, dentry, destroy_p);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_reintegrate_del (vol, idir, name, destroy_p);

      r2 = zfs_fh_lookup_nolock (dir, &vol, &idir, NULL, true);
      if (r2 == ZFS_OK)
	{
	  dentry = dentry_lookup_name (idir, name);
	  release_dentry (idir);
	  zfsd_mutex_unlock (&vol->mutex);

	  if (dentry)
	    internal_dentry_destroy (dentry, true);

	  zfsd_mutex_unlock (&fh_mutex);
	}
    }
  else
    abort ();

  return r;
}

/* Set version of local file handle FH with dentry DENTRY on volume VOL
   to VERSION.  */

int32_t
local_reintegrate_set (zfs_fh *fh, uint64_t version, internal_dentry dentry,
		       volume vol)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (dentry)
    CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
#endif

  if (dentry)
    {
      dentry->fh->meta.local_version = version;
      if (!vol->is_copy)
	dentry->fh->meta.master_version = version;
      set_attr_version (&dentry->fh->attr, &dentry->fh->meta);
      if (!flush_metadata (vol, &dentry->fh->meta))
	{
	  vol->delete_p = true;
	  release_dentry (dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	  return ZFS_UPDATE_FAILED;
	}
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
    }
  else
    {
      metadata meta;

      if (!get_metadata (vol, fh, &meta))
	return ZFS_UPDATE_FAILED;

      meta.local_version = version;
      if (!vol->is_copy)
	meta.master_version = version;

      if (!flush_metadata (vol, &meta))
	{
	  vol->delete_p = true;
	  zfsd_mutex_unlock (&vol->mutex);
	  return ZFS_UPDATE_FAILED;
	}
    }

  return ZFS_OK;
}

/* Set version of remote file handle FH with dentry DENTRY on volume VOL
   to VERSION.  */

int32_t
remote_reintegrate_set (zfs_fh *fh, uint64_t version, internal_dentry dentry,
			volume vol)
{
  reintegrate_set_args args;
  thread *t;
  int32_t r;
  int fd;
  node nod = vol->master;

  CHECK_MUTEX_LOCKED (&vol->mutex);
#ifdef ENABLE_CHECKING
  if (dentry)
    {
      CHECK_MUTEX_LOCKED (&dentry->fh->mutex);
      if (zfs_fh_undefined (dentry->fh->meta.master_fh))
	abort ();
    }
#endif

  args.fh = *fh;
  args.version = version;

  if (dentry)
    {
      dentry->fh->attr.version = version;
      release_dentry (dentry);
    }
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_reintegrate_set_client (t, &args, nod, &fd);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_fd (t->dc_reply, fd);
  return r;
}

/* Set version of file handle FH to VERSION.  */

int32_t
zfs_reintegrate_set (zfs_fh *fh, uint64_t version)
{
  volume vol;
  internal_dentry dentry;
  int32_t r;

  if (!REGULAR_FH_P (*fh))
    return EINVAL;

  r = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL, true);
  if (r != ZFS_OK)
    vol = volume_lookup (fh->vid);

  if (!vol)
    return ESTALE;

  if (fh->sid == this_node->id)
    r = local_reintegrate_set (fh, version, dentry, vol);
  else
    r = remote_reintegrate_set (fh, version, dentry, vol);

  return r;
}

/* Refresh file handle FH.  */

int32_t
refresh_fh (zfs_fh *fh)
{
  internal_dentry dentry;
  volume vol;
  zfs_fh volume_root_fh;
  file_info_res info;
  dir_op_res res;
  int32_t r;

  if (!REGULAR_FH_P (*fh))
    return EINVAL;

  r = zfs_file_info (&info, fh);
  if (r != ZFS_OK)
    return r;

  vol = volume_lookup (fh->vid);
  if (!vol)
    {
      free (info.path.str);
      return ESTALE;
    }

  r = get_volume_root_dentry (vol, &dentry, true);
  if (r != ZFS_OK)
    {
      free (info.path.str);
      return r;
    }

  volume_root_fh = dentry->fh->local_fh;
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);

  r = zfs_extended_lookup (&res, &volume_root_fh, info.path.str);
  free (info.path.str);

  return r;
}
