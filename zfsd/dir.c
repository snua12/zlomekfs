/* Directory operations.
   Copyright (C) 2003-2004 Josef Zlomek

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
#include "update.h"

/* Return the local path of file for dentry DENTRY on volume VOL.  */

char *
build_local_path (volume vol, internal_dentry dentry)
{
  internal_dentry tmp;
  unsigned int n;
  varray v;
  char *r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
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
build_local_path_name (volume vol, internal_dentry dentry, char *name)
{
  internal_dentry tmp;
  unsigned int n;
  varray v;
  char *r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
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

/* Return path relative to volume root of file DENTRY.  */

char *
build_relative_path (internal_dentry dentry)
{
  internal_dentry tmp;
  unsigned int n;
  varray v;
  char *r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  /* Count the number of strings which will be concatenated.  */
  n = 0;
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

  r = xstrconcat_varray (&v);
  varray_destroy (&v);

  return r;
}

/* Return path relative to volume root of file NAME in directory DENTRY.  */

char *
build_relative_path_name (internal_dentry dentry, char *name)
{
  internal_dentry tmp;
  unsigned int n;
  varray v;
  char *r;

  CHECK_MUTEX_LOCKED (&fh_mutex);
  CHECK_MUTEX_LOCKED (&dentry->fh->mutex);

  /* Count the number of strings which will be concatenated.  */
  n = 2;
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

  r = xstrconcat_varray (&v);
  varray_destroy (&v);

  return r;
}

/* Return a pointer into PATH where path relative to volume root starts.  */

char *
local_path_to_relative_path (volume vol, char *path)
{
  int i;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  for (i = 0; vol->local_path[i] == path[i]; i++)
    ;
#ifdef ENABLE_CHECKING
  /* Now we should be at the end of VOL->LOCAL_PATH.  */
  if (vol->local_path[i])
    abort ();
#endif
  return path + i;
}

/* Recursively unlink the file PATH on volume with ID == VID.  */

bool
recursive_unlink (const char *path, uint32_t vid)
{
  volume vol;
  internal_dentry dentry;
  zfs_fh fh;
  bool r;
  struct stat st;

  if (lstat (path, &st) != 0)
    return errno == ENOENT;

  if ((st.st_mode & S_IFMT) != S_IFDIR)
    {
      if (unlink (path) != 0)
	{
	  r = errno == ENOENT;
	  goto out;
	}
    }
  else
    {
      DIR *d;
      struct dirent *de;

      d = opendir (path);
      if (!d)
	{
	  r = errno == ENOENT;
	  goto out;
	}

      while ((de = readdir (d)) != NULL)
	{
	  char *new_path;

	  /* Skip "." and "..".  */
	  if (de->d_name[0] == '.'
	      && (de->d_name[1] == 0
		  || (de->d_name[1] == '.'
		      && de->d_name[2] == 0)))
	    continue;

	  new_path = xstrconcat (3, path, "/", de->d_name);
	  r = recursive_unlink (new_path, vid);
	  free (new_path);
	  if (!r)
	    {
	      closedir (d);
	      return false;
	    }
	}
      closedir (d);

      if (rmdir (path) != 0)
	{
	  r = errno == ENOENT;
	  goto out;
	}
    }

  vol = volume_lookup (vid);
  if (vol)
    {
      if (!delete_metadata (vol, st.st_dev, st.st_ino))
	vol->flags |= VOLUME_DELETE;
      zfsd_mutex_unlock (&vol->mutex);
    }

  r = true;

out:
  /* Destroy dentry associated with the file.  */
  fh.sid = this_node->id;
  fh.vid = vid;
  fh.dev = st.st_dev;
  fh.ino = st.st_ino;

  zfsd_mutex_lock (&fh_mutex);
  dentry = dentry_lookup (&fh);
  if (dentry)
    internal_dentry_destroy (dentry, true);
  zfsd_mutex_unlock (&fh_mutex);

  return r;
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
      int32_t r;
      volume vol = pvd->vol;

      zfsd_mutex_unlock (&pvd->mutex);
      r = get_volume_root_dentry (vol, dir, true);
      if (r != ZFS_OK)
	return r;
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
get_volume_root_local (volume vol, zfs_fh *local_fh, fattr *attr)
{
  struct stat st;
  char *path;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  local_fh->sid = this_node->id;
  local_fh->vid = vol->id;

  path = xstrdup (vol->local_path);
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

/* Get file handle of root of volume with id VID, store the local file handle
   to LOCAL_FH and master's file handle to MASTER_FH, if defined.  */

static int32_t
get_volume_root (uint32_t vid, zfs_fh *local_fh, zfs_fh *master_fh,
		 fattr *attr)
{
  volume vol;
  int32_t r;

  if (local_fh)
    {
      vol = volume_lookup (vid);
      if (!vol)
	return ENOENT;

      r = get_volume_root_local (vol, local_fh, attr);
      if (r != ZFS_OK)
	return r;

      if (master_fh)
	{
	  fattr tmp;

	  vol = volume_lookup (vid);
	  if (!vol)
	    return ENOENT;

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
      vol = volume_lookup (vid);
      if (!vol)
	return ENOENT;

      r = get_volume_root_remote (vol, master_fh, attr);
      if (r != ZFS_OK)
	return r;
    }

  return ZFS_OK;
}

/* Update root of volume VOL, create an internal file handle for it and store
   it to IFH.  */

int32_t
get_volume_root_dentry (volume vol, internal_dentry *dentry,
			bool unlock_fh_mutex)
{
  zfs_fh local_fh, master_fh;
  zfs_fh *local_fhp, *master_fhp;
  uint32_t vid;
  fattr attr;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  vid = vol->id;

  if (vol->flags & VOLUME_DELETE)
    {
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_lock (&fh_mutex);
      vol = volume_lookup (vid);
      if (vol)
	volume_delete (vol);
      zfsd_mutex_unlock (&fh_mutex);
      return ENOENT;
    }

  local_fhp = (vol->local_path ? &local_fh : NULL);
  master_fhp = (vol->master != this_node ? &master_fh : NULL);
  zfsd_mutex_unlock (&vol->mutex);

  r = get_volume_root (vid, local_fhp, master_fhp, &attr);
  if (r != ZFS_OK)
    return r;

  zfsd_mutex_lock (&fh_mutex);
  vol = volume_lookup (vid);
  if (!vol)
    {
      zfsd_mutex_unlock (&fh_mutex);
      return ENOENT;
    }

  if (local_fhp && !master_fhp)
    zfs_fh_undefine (master_fh);

  if (!local_fhp && master_fhp)
    local_fh = master_fh;

  *dentry = get_dentry (&local_fh, &master_fh, vol, NULL, "", &attr);

  if (unlock_fh_mutex)
    zfsd_mutex_unlock (&fh_mutex);

  return ZFS_OK;
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
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = local_getattr_path (attr, path);
  free (path);

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
  if (zfs_fh_undefined (dentry->fh->master_fh))
    abort ();
#endif

  args = dentry->fh->master_fh;

  release_dentry (dentry);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_getattr_client (t, &args, nod, &fd);

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
  zfs_fh tmp_fh;
  int32_t r, r2;
  int retry = 0;

zfs_getattr_retry:

  /* Lookup FH.  */
  r = zfs_fh_lookup (fh, &vol, &dentry, &vd);
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

  r = internal_dentry_lock (LEVEL_SHARED, &vol, &dentry, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (vol->local_path)
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

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      /* Update cached file attributes.  */
      if (vol->local_path)
	set_attr_version (fa, &dentry->fh->meta);
      dentry->fh->attr = *fa;
    }

  internal_dentry_unlock (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
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

  return local_getattr_path (fa, path);
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
  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = local_setattr_path (fa, path, sa);
  free (path);

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
  if (zfs_fh_undefined (dentry->fh->master_fh))
    abort ();
#endif

  args.file = dentry->fh->master_fh;
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
  virtual_dir vd;
  zfs_fh tmp_fh;
  int32_t r, r2;
  int retry = 0;

zfs_setattr_retry:

  /* Lookup FH.  */
  r = zfs_fh_lookup (fh, &vol, &dentry, &vd);
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

  if (vol->local_path)
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

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      /* Update cached file attributes.  */
      if (vol->local_path)
	set_attr_version (fa, &dentry->fh->meta);
      dentry->fh->attr = *fa;
    }

  internal_dentry_unlock (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
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

int32_t
local_lookup (dir_op_res *res, internal_dentry dir, string *name, volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&dir->fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;

  path = build_local_path_name (vol, dir, name->str);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = local_getattr_path (&res->attr, path);
  free (path);
  if (r != ZFS_OK)
    return r;

  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;

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
  if (zfs_fh_undefined (dir->fh->master_fh))
    abort ();
#endif

  args.dir = dir->fh->master_fh;
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
  zfs_fh tmp_fh;
  int32_t r, r2;
  int retry = 0;

zfs_lookup_retry:

  /* Lookup DIR.  */
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
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
      if (vol)
	CHECK_MUTEX_LOCKED (&vol->mutex);

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
      zfsd_mutex_unlock (&pvd->mutex);
      if (vol)
	{
	  r = get_volume_root_dentry (vol, &idir, true);
	  if (r != ZFS_OK)
	    return r;
	}
      else
	return ENOENT;
    }
  else
    zfsd_mutex_unlock (&fh_mutex);

  if (idir->fh->attr.type != FT_DIR)
    {
      release_dentry (idir);
      zfsd_mutex_unlock (&vol->mutex);
      return ENOTDIR;
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

  if (strcmp (name->str, ".") == 0)
    {
      res->file = idir->fh->local_fh;
      res->attr = idir->fh->attr;
      release_dentry (idir);
      zfsd_mutex_unlock (&vol->mutex);
      return ZFS_OK;
    }
  else if (strcmp (name->str, "..") == 0)
    {
      if (idir->parent)
	{
	  res->file = idir->parent->fh->local_fh;
	  res->attr = idir->parent->fh->attr;
	  release_dentry (idir);
	  zfsd_mutex_unlock (&vol->mutex);
	}
      else
	{
	  release_dentry (idir);
	  /* This is safe because the virtual directory can't be destroyed
	     while volume is locked.  */
	  pvd = vol->root_vd->parent ? vol->root_vd->parent : vol->root_vd;
	  res->file = pvd->fh;
	  res->attr = pvd->attr;
	  zfsd_mutex_unlock (&vol->mutex);
	}
      return ZFS_OK;
    }

  r = internal_dentry_lock (LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (vol->local_path)
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_lookup (res, idir, name, vol);
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

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &idir, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      dentry = get_dentry (&res->file, &master_res.file, vol, idir, name->str,
			   &res->attr);
      release_dentry (dentry);
    }

  internal_dentry_unlock (idir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
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

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;

  path = build_local_path_name (vol, dir, name->str);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
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

  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;

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
  if (zfs_fh_undefined (dir->fh->master_fh))
    abort ();
#endif

  args.where.dir = dir->fh->master_fh;
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
  zfs_fh tmp_fh;
  int32_t r, r2;
  int retry = 0;

zfs_mkdir_retry:

  /* Lookup DIR.  */
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
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

  attr->size = (uint64_t) -1;
  attr->atime = (zfs_time) -1;
  attr->mtime = (zfs_time) -1;

  r = internal_dentry_lock (LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (vol->local_path)
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_mkdir (res, idir, name, attr, vol);
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

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &idir, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      dentry = get_dentry (&res->file, &master_res.file, vol, idir, name->str,
			   &res->attr);
      if (vol->local_path)
	{
	  if (!inc_local_version (vol, idir->fh))
	    vol->flags |= VOLUME_DELETE;
	  if (!set_metadata (vol, dentry->fh, dentry->fh->meta.flags,
			     dentry->fh->meta.local_version + 1, 0))
	    vol->flags |= VOLUME_DELETE;
	}
      release_dentry (dentry);
    }

  internal_dentry_unlock (idir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
    {
      retry++;
      r = refresh_path (dir);
      if (r == ZFS_OK)
	goto zfs_mkdir_retry;
    }

  return r;
}

/* Remove local directory NAME from directory DIR on volume VOL,
   store the stat structure of NAME to ST.  */

static int32_t
local_rmdir (struct stat *st, internal_dentry dir, string *name, volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  path = build_local_path_name (vol, dir, name->str);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = lstat (path, st);
  if (r != 0)
    {
      free (path);
      return errno;
    }
  r = rmdir (path);
  free (path);
  if (r != 0)
    return errno;

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
  if (zfs_fh_undefined (dir->fh->master_fh))
    abort ();
#endif

  args.dir = dir->fh->master_fh;
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
  struct stat st;
  zfs_fh tmp_fh;
  int32_t r, r2;
  int retry = 0;

zfs_rmdir_retry:

  /* Lookup DIR.  */
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
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

  if (vol->local_path)
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_rmdir (&st, idir, name, vol);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_rmdir (idir, name, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &idir, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  /* Delete the internal file handle of the deleted directory.  */
  if (r == ZFS_OK)
    {
      DESTROY_DENTRY (vol, idir, name->str, tmp_fh);

      if (vol->local_path)
	{
	  if (!delete_metadata (vol, st.st_dev, st.st_ino))
	    vol->flags |= VOLUME_DELETE;
	  if (!inc_local_version (vol, idir->fh))
	    vol->flags |= VOLUME_DELETE;
	}
    }

  internal_dentry_unlock (idir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
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
  release_dentry (from_dir);
  if (to_dir->fh != from_dir->fh)
    release_dentry (to_dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
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
  if (zfs_fh_undefined (from_dir->fh->master_fh))
    abort ();
  if (zfs_fh_undefined (to_dir->fh->master_fh))
    abort ();
#endif

  args.from.dir = from_dir->fh->master_fh;
  args.from.name = *from_name;
  args.to.dir = to_dir->fh->master_fh;
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
  internal_dentry from_dentry, to_dentry;
  virtual_dir vd;
  zfs_fh tmp_from, tmp_to;
  int32_t r, r2;
  int retry = 0;

  /* Lookup TO_DIR.  */
  if (VIRTUAL_FH_P (*to_dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (to_dir, &vol, &to_dentry, &vd);
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
  r = zfs_fh_lookup_nolock (from_dir, &vol, &from_dentry, &vd);
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

zfs_rename_retry:
  /* Lookup dentries.  */
  r = zfs_fh_lookup_nolock (&tmp_from, &vol, &from_dentry, NULL);
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
	  return ZFS_STALE;
	}
    }
  else
    to_dentry = from_dentry;

  zfsd_mutex_unlock (&fh_mutex);

  r = internal_dentry_lock2 (LEVEL_EXCLUSIVE, LEVEL_EXCLUSIVE, &vol,
			     &from_dentry, &to_dentry, &tmp_from, &tmp_to);
  if (r != ZFS_OK)
    return r;

  if (vol->local_path)
    {
      UPDATE_FH_IF_NEEDED_2 (vol, to_dentry, from_dentry, tmp_to, tmp_from);
      if (tmp_from.ino != tmp_to.ino)
	UPDATE_FH_IF_NEEDED_2 (vol, from_dentry, to_dentry, tmp_from, tmp_to);
      r = local_rename (from_dentry, from_name, to_dentry, to_name, vol);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_rename (from_dentry, from_name, to_dentry, to_name, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_to, &vol, &to_dentry, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      DESTROY_DENTRY (vol, to_dentry, to_name->str, tmp_to);

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
      dentry = dentry_lookup_name (from_dentry, from_name->str);
      if (dentry)
	{
	  if (!internal_dentry_move (dentry, vol, to_dentry, to_name->str))
	    r = EINVAL;
	  release_dentry (dentry);
	}

      if (vol->local_path)
	{
	  if (!inc_local_version (vol, from_dentry->fh))
	    vol->flags |= VOLUME_DELETE;
	  if (!inc_local_version (vol, to_dentry->fh))
	    vol->flags |= VOLUME_DELETE;
	}
    }
  else
    {
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
    }

  internal_dentry_unlock (to_dentry);
  if (to_dentry != from_dentry)
    internal_dentry_unlock (from_dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
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
  release_dentry (from);
  if (dir->fh != from->fh)
    release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
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
  if (zfs_fh_undefined (from->fh->master_fh))
    abort ();
  if (zfs_fh_undefined (dir->fh->master_fh))
    abort ();
#endif

  args.from = from->fh->master_fh;
  args.to.dir = dir->fh->master_fh;
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
  internal_dentry from_dentry, dir_dentry;
  virtual_dir vd;
  zfs_fh tmp_from, tmp_dir;
  int32_t r, r2;
  int retry = 0;

  /* Lookup FROM.  */
  if (VIRTUAL_FH_P (*from))
    return EPERM;

  r = zfs_fh_lookup (from, &vol, &from_dentry, NULL);
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
  r = zfs_fh_lookup_nolock (dir, &vol, &dir_dentry, &vd);
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

zfs_link_retry:
  /* Lookup dentries.  */
  r = zfs_fh_lookup_nolock (&tmp_from, &vol, &from_dentry, NULL);
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

  if (vol->local_path)
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

  r2 = zfs_fh_lookup_nolock (&tmp_dir, &vol, &dir_dentry, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      DESTROY_DENTRY (vol, dir_dentry, name->str, tmp_dir);

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

      internal_dentry_link (from_dentry->fh, vol, dir_dentry, name->str);

      if (vol->local_path)
	{
	  if (!inc_local_version (vol, dir_dentry->fh))
	    vol->flags |= VOLUME_DELETE;
	}
    }
  else
    {
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
    }

  internal_dentry_unlock (dir_dentry);
  if (dir_dentry != from_dentry)
    internal_dentry_unlock (from_dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
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

/* Delete local file NAME from directory DIR on volume VOL,
   store the stat structure of NAME to ST.  */

static int32_t
local_unlink (struct stat *st, internal_dentry dir, string *name, volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  path = build_local_path_name (vol, dir, name->str);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = lstat (path, st);
  if (r != 0)
    {
      free (path);
      return errno;
    }
  r = unlink (path);
  free (path);
  if (r != 0)
    return errno;

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
  if (zfs_fh_undefined (dir->fh->master_fh))
    abort ();
#endif

  args.dir = dir->fh->master_fh;
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
  struct stat st;
  zfs_fh tmp_fh;
  int32_t r, r2;
  int retry = 0;

zfs_unlink_retry:

  /* Lookup DIR.  */
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
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

  if (vol->local_path)
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_unlink (&st, idir, name, vol);
    }
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_unlink (idir, name, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &idir, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  /* Delete the internal file handle of the deleted directory.  */
  if (r == ZFS_OK)
    {
      DESTROY_DENTRY (vol, idir, name->str, tmp_fh);

      if (vol->local_path)
	{
	  if (!delete_metadata (vol, st.st_dev, st.st_ino))
	    vol->flags |= VOLUME_DELETE;
	  if (!inc_local_version (vol, idir->fh))
	    vol->flags |= VOLUME_DELETE;
	}
    }

  internal_dentry_unlock (idir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
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
  release_dentry (file);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = readlink (path, buf, ZFS_MAXDATA);
  free (path);
  if (r < 0)
    return errno;

  buf[r] = 0;
  res->path.str = xstrdup (buf);
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
  if (zfs_fh_undefined (file->fh->master_fh))
    abort ();
#endif

  args = file->fh->master_fh;

  release_dentry (file);
  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&nod->mutex);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&node_mutex);

  t = (thread *) pthread_getspecific (thread_data_key);
  r = zfs_proc_readlink_client (t, &args, nod, &fd);

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
  zfs_fh tmp_fh;
  int32_t r, r2;
  int retry = 0;

  if (VIRTUAL_FH_P (*fh))
    return EINVAL;

zfs_readlink_retry:

  /* Lookup FH.  */
  r = zfs_fh_lookup (fh, &vol, &dentry, NULL);
  if (r != ZFS_OK)
    return r;

  r = internal_dentry_lock (LEVEL_SHARED, &vol, &dentry, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (vol->local_path)
    r = local_readlink (res, dentry, vol);
  else if (vol->master != this_node)
    {
      zfsd_mutex_unlock (&fh_mutex);
      r = remote_readlink (res, dentry, vol);
    }
  else
    abort ();

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  internal_dentry_unlock (dentry);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
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
local_symlink (dir_op_res *res, internal_dentry dir, string *name, string *to,
	       sattr *attr, volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;

  path = build_local_path_name (vol, dir, name->str);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = symlink (to->str, path);
  if (r != 0)
    {
      free (path);
      return errno;
    }

  r = local_setattr_path (&res->attr, path, attr);
  free (path);
  if (r != ZFS_OK)
    return r;

  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;

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
  if (zfs_fh_undefined (dir->fh->master_fh))
    abort ();
#endif

  args.from.dir = dir->fh->master_fh;
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
  int32_t r, r2;
  int retry = 0;

zfs_symlink_retry:

  /* Lookup DIR.  */
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
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

  if (vol->local_path)
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_symlink (res, idir, name, to, attr, vol);
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

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &idir, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      dentry = get_dentry (&res->file, &master_res.file, vol, idir, name->str,
			   &res->attr);
      if (vol->local_path)
	{
	  if (!inc_local_version (vol, idir->fh))
	    vol->flags |= VOLUME_DELETE;
	  if (!set_metadata (vol, dentry->fh, dentry->fh->meta.flags,
			     dentry->fh->meta.local_version + 1, 0))
	    vol->flags |= VOLUME_DELETE;
	}
      release_dentry (dentry);
    }

  internal_dentry_unlock (idir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
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
local_mknod (dir_op_res *res, internal_dentry dir, string *name, sattr *attr,
	     ftype type, uint32_t rdev, volume vol)
{
  char *path;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->fh->mutex);

  res->file.sid = dir->fh->local_fh.sid;
  res->file.vid = dir->fh->local_fh.vid;

  path = build_local_path_name (vol, dir, name->str);
  release_dentry (dir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);
  r = mknod (path, attr->mode | ftype2mode[type], rdev);
  if (r != 0)
    {
      free (path);
      return errno;
    }

  r = local_setattr_path (&res->attr, path, attr);
  free (path);
  if (r != ZFS_OK)
    return r;

  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;

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
  if (zfs_fh_undefined (dir->fh->master_fh))
    abort ();
#endif

  args.where.dir = dir->fh->master_fh;
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
  int32_t r, r2;
  int retry = 0;

zfs_mknod_retry:

  /* Lookup DIR.  */
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  r = zfs_fh_lookup_nolock (dir, &vol, &idir, &pvd);
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

  attr->size = (uint64_t) -1;
  attr->atime = (zfs_time) -1;
  attr->mtime = (zfs_time) -1;

  r = internal_dentry_lock (LEVEL_EXCLUSIVE, &vol, &idir, &tmp_fh);
  if (r != ZFS_OK)
    return r;

  if (vol->local_path)
    {
      UPDATE_FH_IF_NEEDED (vol, idir, tmp_fh);
      r = local_mknod (res, idir, name, attr, type, rdev, vol);
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

  r2 = zfs_fh_lookup_nolock (&tmp_fh, &vol, &idir, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  if (r == ZFS_OK)
    {
      internal_dentry dentry;

      dentry = get_dentry (&res->file, &master_res.file, vol, idir, name->str,
			   &res->attr);
      if (vol->local_path)
	{
	  if (!inc_local_version (vol, idir->fh))
	    vol->flags |= VOLUME_DELETE;
	  if (!set_metadata (vol, dentry->fh, dentry->fh->meta.flags,
			     dentry->fh->meta.local_version + 1, 0))
	    vol->flags |= VOLUME_DELETE;
	}
      release_dentry (dentry);
    }

  internal_dentry_unlock (idir);
  zfsd_mutex_unlock (&vol->mutex);
  zfsd_mutex_unlock (&fh_mutex);

  if (r == ZFS_STALE && retry < 1)
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
  r = remote_lookup (res, dir, &s, vol);
  if (r == ZFS_STALE)
    {
      r = refresh_path_1 (res, dir->parent, dir->name, vol);
      if (r == ZFS_OK)
	r = remote_lookup (res, dir, &s, vol);
    }
  release_dentry (dir);

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

  release_dentry (dentry);
  zfsd_mutex_unlock (&vol->mutex);

  return r;
}

/* Refresh master file handles on path to DENTRY on volume VOL.  */

int32_t
refresh_master_fh (zfs_fh *fh)
{
  volume vol;
  internal_dentry dentry;
  int32_t r, r2;
  int retry = 0;

#ifdef ENABLE_CHECKING
  if (VIRTUAL_FH_P (*fh))
    abort ();
#endif

  r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
  if (r2 != ZFS_OK)
    abort ();
#endif

  /* Refresh remote path to DENTRY.  */
  if (zfs_fh_undefined (dentry->fh->master_fh))
    {
      if (dentry->parent)
	{
	  zfs_fh parent_fh;
	  internal_dentry parent;
	  dir_op_res res;
	  string s;

	  zfsd_mutex_lock (&dentry->parent->fh->mutex);
	  parent_fh = dentry->parent->fh->local_fh;
	  release_dentry (dentry->parent);
	  release_dentry (dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	  zfsd_mutex_unlock (&fh_mutex);

	  r = refresh_master_fh (&parent_fh);
	  if (r != ZFS_OK)
	    return r;

retry_lookup:
	  r2 = zfs_fh_lookup_nolock (fh, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  s.str = dentry->name;
	  s.len = strlen (dentry->name);
	  parent = dentry->parent;
	  zfsd_mutex_lock (&parent->fh->mutex);
	  parent_fh = parent->fh->local_fh;
	  release_dentry (dentry);
	  zfsd_mutex_unlock (&fh_mutex);
	  r = remote_lookup (&res, parent, &s, vol);

	  if (r == ZFS_STALE && retry < 1)
	    {
	      retry++;
	      r = refresh_path (&parent_fh);
	      if (r == ZFS_OK)
		goto retry_lookup;
	    }

	  if (r != ZFS_OK)
	    return r;

	  r2 = zfs_fh_lookup (fh, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  dentry->fh->master_fh = res.file;
	  release_dentry (dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	}
      else
	{
	  fattr fa;
	  zfs_fh tmp_fh;

	  release_dentry (dentry);
	  zfsd_mutex_unlock (&fh_mutex);

	  r = get_volume_root_remote (vol, &tmp_fh, &fa);
	  if (r != ZFS_OK)
	    return r;

	  r2 = zfs_fh_lookup (fh, &vol, &dentry, NULL);
#ifdef ENABLE_CHECKING
	  if (r2 != ZFS_OK)
	    abort ();
#endif

	  dentry->fh->master_fh = tmp_fh;
	  release_dentry (dentry);
	  zfsd_mutex_unlock (&vol->mutex);
	}
    }
  else
    {
      release_dentry (dentry);
      zfsd_mutex_unlock (&vol->mutex);
      zfsd_mutex_unlock (&fh_mutex);
    }

  return ZFS_OK;
}
