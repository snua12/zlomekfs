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
#include <sys/types.h>
#include <sys/stat.h>
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

/* Return the local path of file for file handle FH on volume VOL.  */

char *
build_local_path (volume vol, internal_fh fh)
{
  internal_fh tmp;
  unsigned int n;
  varray v;
  char *r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  /* Count the number of strings which will be concatenated.  */
  n = 1;
  for (tmp = fh; tmp->parent; tmp = tmp->parent)
    n += 2;

  varray_create (&v, sizeof (char *), n);
  VARRAY_USED (v) = n;
  for (tmp = fh; tmp->parent; tmp = tmp->parent)
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

/* Return the local path of file NAME in directory FH on volume VOL.  */

char *
build_local_path_name (volume vol, internal_fh fh, const char *name)
{
  internal_fh tmp;
  unsigned int n;
  varray v;
  char *r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  /* Count the number of strings which will be concatenated.  */
  n = 3;
  for (tmp = fh; tmp->parent; tmp = tmp->parent)
    n += 2;

  varray_create (&v, sizeof (char *), n);
  VARRAY_USED (v) = n;
  n--;
  VARRAY_ACCESS (v, n, char *) = (char *) name;
  n--;
  VARRAY_ACCESS (v, n, char *) = "/";
  for (tmp = fh; tmp->parent; tmp = tmp->parent)
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

/* Check whether we can perform file system change operation on NAME in
   virtual directory PVD.  Resolve whether the is a volume mapped on PVD
   whose mounpoint name is not NAME and if so return ZFS_OK and store
   the internal_fh of the root of volume to IDIR.  */

int
validate_operation_on_virtual_directory (virtual_dir pvd, string *name,
					 internal_fh *idir)
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
      int r = update_volume_root (pvd->vol, idir);
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

static int
get_volume_root_local (volume vol, zfs_fh *local_fh, fattr *attr)
{
  CHECK_MUTEX_LOCKED (&vol->mutex);

  /* The volume (or its copy) is located on this node.  */
  if (vol->local_path)
    {
      struct stat st;

      if (stat (vol->local_path, &st) != 0)
	return errno;

      local_fh->sid = this_node->id;
      local_fh->vid = vol->id;
      local_fh->dev = st.st_dev;
      local_fh->ino = st.st_ino;
      fattr_from_struct_stat (attr, &st, vol);
    }
  else
    abort ();

  if (attr->type != FT_DIR)
    return ENOTDIR;
  return ZFS_OK;
}

/* Store the remote file handle of root of volume VOL to REMOTE_FH
   and its attributes to ATTR.  */

static int
get_volume_root_remote (volume vol, zfs_fh *remote_fh, fattr *attr)
{
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  /* The volume is completelly remote or we have a copy of the volume.
     Call the remote function only when we need the file handle.  */
  if (vol->master != this_node)
    {
      volume_root_args args;
      thread *t;

      t = (thread *) pthread_getspecific (thread_data_key);
      args.vid = vol->id;
      zfsd_mutex_lock (&node_mutex);
      zfsd_mutex_lock (&vol->master->mutex);
      zfsd_mutex_unlock (&node_mutex);
      r = zfs_proc_volume_root_client (t, &args, vol->master);
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
	recycle_dc_to_volume_master (&t->dc_reply, vol);
    }
  else
    abort ();

  if (r == ZFS_OK && attr->type != FT_DIR)
    return ENOTDIR;
  return r;
}

/* Get file handle of root of volume VOL, store the local file handle to
   LOCAL_FH and master's file handle to MASTER_FH, if defined.  */

static int
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

	  if (master_fh)
	    memcpy (master_fh, local_fh, sizeof (zfs_fh));
	}
      else if (master_fh)
	{
	  r = get_volume_root_local (vol, master_fh, attr);
	}
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
	    }
	}
      else if (master_fh)
	{
	  r = get_volume_root_remote (vol, master_fh, attr);
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

int
update_volume_root (volume vol, internal_fh *ifh)
{
  zfs_fh local_fh, master_fh;
  fattr attr;
  int r;

  CHECK_MUTEX_LOCKED (&vol->mutex);

  r = get_volume_root (vol, &local_fh, &master_fh, &attr);
  if (r != ZFS_OK)
    return r;

  if (vol->root_fh == NULL
      || !ZFS_FH_EQ (vol->root_fh->local_fh, local_fh)
      || !ZFS_FH_EQ (vol->root_fh->master_fh, master_fh))
    {
      /* FIXME? delete only FHs which are not open now?  */
      htab_empty (vol->fh_htab_name);
      htab_empty (vol->fh_htab);

      vol->root_fh = internal_fh_create (&local_fh, &master_fh, NULL,
					 vol, "", &attr);
    }
  else
    zfsd_mutex_lock (&vol->root_fh->mutex);

  *ifh = vol->root_fh;
  return ZFS_OK;
}

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

  /* Lookup FH.  */
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

/* Set attributes of local file PATH on volume VOL according to SA,
   reget attributes and store them to FA.  */

static int
local_setattr_path (fattr *fa, char *path, sattr *sa, volume vol)
{
  if (sa->mode != (unsigned int) -1)
    {
      if (chmod (path, sa->mode) != 0)
	return errno;
    }

  if (sa->uid != (unsigned int) -1 || sa->gid != (unsigned int) -1)
    {
      if (lchown (path, sa->uid, sa->gid) != 0)
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
    return local_getattr (fa, path, vol);
  return ZFS_OK;
}

/* Set attributes of local file FH on volume VOL according to SA,
   reget attributes and store them to FA.  */

static int
local_setattr (fattr *fa, internal_fh fh, sattr *sa, volume vol)
{
  char *path;
  int r;

  CHECK_MUTEX_LOCKED (&fh->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  path = build_local_path (vol, fh);
  r = local_setattr_path (fa, path, sa, vol);
  free (path);
  return r;
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
    recycle_dc_to_volume_master (&t->dc_reply, vol);
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

  /* Lookup FH.  */
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

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int
zfs_extended_lookup (dir_op_res *res, zfs_fh *dir, char *path)
{
  string str;
  int r;

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

static int
local_lookup (dir_op_res *res, internal_fh dir, string *name, volume vol)
{
  char *path;
  int r;

  CHECK_MUTEX_LOCKED (&dir->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = local_getattr (&res->attr, path, vol);
  free (path);
  if (r != ZFS_OK)
    return r;

  res->file.sid = dir->local_fh.sid;
  res->file.vid = dir->local_fh.vid;
  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;

  return ZFS_OK;
}

/* Lookup file handle of remote file NAME in directory DIR on volume VOL
   and store it to FH.  */

static int
remote_lookup (dir_op_res *res, internal_fh dir, string *name, volume vol)
{
  dir_op_args args;
  thread *t;
  int32_t r;

  CHECK_MUTEX_LOCKED (&dir->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  args.dir = dir->master_fh;
  args.name = *name;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_lookup_client (t, &args, vol->master);
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
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int
zfs_lookup (dir_op_res *res, zfs_fh *dir, string *name)
{
  volume vol;
  internal_fh idir;
  virtual_dir pvd;
  dir_op_res master_res;

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  if (!fh_lookup_nolock (dir, &vol, &idir, &pvd))
    {
      zfsd_mutex_unlock (&volume_mutex);
      if (VIRTUAL_FH_P (*dir))
	zfsd_mutex_unlock (&vd_mutex);
      return ESTALE;
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
	  int r;

	  r = update_volume_root (vol, &idir);
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

  /* TODO: update directory */

  if (idir)
    {
      int r;

      CHECK_MUTEX_LOCKED (&idir->mutex);
      CHECK_MUTEX_LOCKED (&vol->mutex);

      if (vol->local_path)
	{
	  r = local_lookup (res, idir, name, vol);
	  if (r == ZFS_OK)
	    {
	      if (vol->master == this_node)
		master_res.file = res->file;
	      else
		r = remote_lookup (&master_res, idir, name, vol);
	    }
	}
      else if (vol->master != this_node)
	{
	  r = remote_lookup (res, idir, name, vol);
	  if (r == ZFS_OK)
	    master_res.file = res->file;
	}
      else
	abort ();

      if (r == ZFS_OK)
	{
	  internal_fh ifh;

	  /* Update internal file handles in htab.  */
	  ifh = fh_lookup_name (vol, idir, name->str);
	  if (ifh)
	    {
	      CHECK_MUTEX_LOCKED (&ifh->mutex);

	      if (!ZFS_FH_EQ (ifh->local_fh, res->file)
		  || !ZFS_FH_EQ (ifh->master_fh, master_res.file))
		{
		  internal_fh_destroy (ifh, vol);
		  ifh = internal_fh_create (&res->file, &master_res.file, idir,
					    vol, name->str, &res->attr);
		}
	    }
	  else
	    ifh = internal_fh_create (&res->file, &master_res.file, idir, vol,
				      name->str, &res->attr);
	  zfsd_mutex_unlock (&ifh->mutex);
	}

      zfsd_mutex_unlock (&idir->mutex);
      zfsd_mutex_unlock (&vol->mutex);
      return r;
    }
  else
    abort ();

  return ESTALE;
}

/* Create directory NAME in local directory DIR on volume VOL, set owner,
   group and permitions according to ATTR.  */

static int
local_mkdir (dir_op_res *res, internal_fh dir, string *name, sattr *attr,
	     volume vol)
{
  char *path;
  int r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = mkdir (path, attr->mode);
  if (r != 0)
    {
      free (path);
      return errno;
    }

  r = local_setattr_path (&res->attr, path, attr, vol);
  free (path);
  if (r != ZFS_OK)
    return r;

  res->file.sid = dir->local_fh.sid;
  res->file.vid = dir->local_fh.vid;
  res->file.dev = res->attr.dev;
  res->file.ino = res->attr.ino;

  return ZFS_OK;
}

/* Create directory NAME in remote directory DIR on volume VOL, set owner,
   group and permitions according to ATTR.  */

static int
remote_mkdir (dir_op_res *res, internal_fh dir, string *name, sattr *attr,
	      volume vol)
{
  mkdir_args args;
  thread *t;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);

  args.where.dir = dir->master_fh;
  args.where.name = *name;
  args.attr = *attr;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_mkdir_client (t, &args, vol->master);

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
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Create directory NAME in directory DIR, set owner, group and permitions
   according to ATTR.  */

int
zfs_mkdir (dir_op_res *res, zfs_fh *dir, string *name, sattr *attr)
{
  volume vol;
  internal_fh idir;
  virtual_dir pvd;
  dir_op_res master_res;
  int r = ZFS_OK;

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  if (!fh_lookup_nolock (dir, &vol, &idir, &pvd))
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
    {
      r = local_mkdir (res, idir, name, attr, vol);
      if (r == ZFS_OK)
	{
	  if (vol->master == this_node)
	    master_res.file = res->file;
	  else
	    r = remote_mkdir (&master_res, idir, name, attr, vol);
	}
    }
  else if (vol->master != this_node)
    {
      r = remote_mkdir (res, idir, name, attr, vol);
      if (r == ZFS_OK)
	master_res.file = res->file;
    }
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_fh ifh;

      /* Update internal file handles in htab.  */
      ifh = fh_lookup_name (vol, idir, name->str);
      if (ifh)
	{
	  CHECK_MUTEX_LOCKED (&ifh->mutex);

	  internal_fh_destroy (ifh, vol);
	  ifh = internal_fh_create (&res->file, &master_res.file, idir,
				    vol, name->str, &res->attr);
	}
      else
	ifh = internal_fh_create (&res->file, &master_res.file, idir,
				  vol, name->str, &res->attr);
      zfsd_mutex_unlock (&ifh->mutex);
    }

  zfsd_mutex_unlock (&idir->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  return r;
}

/* Remove local directory NAME from directory DIR on volume VOL.  */

static int
local_rmdir (internal_fh dir, string *name, volume vol)
{
  char *path;
  int r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = rmdir (path);
  free (path);
  if (r != 0)
    return errno;

  return ZFS_OK;
}

/* Remove remote directory NAME from directory DIR on volume VOL.  */

static int
remote_rmdir (internal_fh dir, string *name, volume vol)
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
  r = zfs_proc_rmdir_client (t, &args, vol->master);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Remove directory NAME from directory DIR.  */

int
zfs_rmdir (zfs_fh *dir, string *name)
{
  volume vol;
  internal_fh idir;
  virtual_dir pvd;
  int r = ZFS_OK;

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  if (!fh_lookup_nolock (dir, &vol, &idir, &pvd))
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

  if (vol->local_path)
    r = local_rmdir (idir, name, vol);
  else if (vol->master != this_node)
    r = remote_rmdir (idir, name, vol);
  else
    abort ();

  /* Delete the internal file handle of the deleted directory.  */
  if (r == ZFS_OK)
    {
      internal_fh ifh;

      ifh = fh_lookup_name (vol, idir, name->str);
      zfsd_mutex_unlock (&idir->mutex);
      if (ifh)
	internal_fh_destroy (ifh, vol);
    }
  else
    zfsd_mutex_unlock (&idir->mutex);

  zfsd_mutex_unlock (&vol->mutex);

  return r;
}

/* Rename local file FROM_NAME in directory FROM_DIR to file TO_NAME
   in directory TO_DIR on volume VOL.  */

static int
local_rename (internal_fh from_dir, string *from_name,
	      internal_fh to_dir, string *to_name, volume vol)
{
  char *path1, *path2;
  int r;

  CHECK_MUTEX_LOCKED (&from_dir->mutex);
  CHECK_MUTEX_LOCKED (&to_dir->mutex);
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

static int
remote_rename (internal_fh from_dir, string *from_name,
	       internal_fh to_dir, string *to_name, volume vol)
{
  rename_args args;
  thread *t;
  int32_t r;

  CHECK_MUTEX_LOCKED (&from_dir->mutex);
  CHECK_MUTEX_LOCKED (&to_dir->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  args.from.dir = from_dir->master_fh;
  args.from.name = *from_name;
  args.to.dir = to_dir->master_fh;
  args.to.name = *to_name;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_rename_client (t, &args, vol->master);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Rename file FROM_NAME in directory FROM_DIR to file TO_NAME
   in directory TO_DIR.  */

int
zfs_rename (zfs_fh *from_dir, string *from_name,
	    zfs_fh *to_dir, string *to_name)
{
  volume vol1 = NULL, vol2 = NULL;
  internal_fh ifh1 = NULL, ifh2 = NULL;
  virtual_dir vd1 = NULL, vd2 = NULL;
  int r = ZFS_OK;

  /* Lookup FROM_DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  zfsd_mutex_lock (&vd_mutex);
  if (!fh_lookup_nolock (from_dir, &vol1, &ifh1, &vd1))
    {
      zfsd_mutex_unlock (&volume_mutex);
      zfsd_mutex_unlock (&vd_mutex);
      return ESTALE;
    }
  if (!vol1)
    {
      r = EROFS;
      goto zfs_rename_error_unlock_volume;
    }
  /* If directory handles are the same unlock IFH1 and VD1 for the same reason
     as VOL1.  */
  if (!ZFS_FH_EQ (*from_dir, *to_dir))
    {
      /* Because LINK operation is allowed only within same volume
	 we may unlock VOL1.  Moreover we have to do so to avoid deadlock.  */
      zfsd_mutex_unlock (&vol1->mutex);

      /* Lookup TO_DIR.  */
      if (!fh_lookup_nolock (to_dir, &vol2, &ifh2, &vd2))
	{
	  r = ESTALE;
	  goto zfs_rename_error_unlock_volume;
	}
      zfsd_mutex_unlock (&volume_mutex);

      if (!vol2)
	{
	  r = EROFS;
	  goto zfs_rename_error_unlock_vd;
	}
      if (vol1 != vol2)
	{
	  r = EXDEV;
	  goto zfs_rename_error_unlock_vd;
	}
    }
  else
    {
      zfsd_mutex_unlock (&volume_mutex);
      vd2 = vd1;
      ifh2 = ifh1;
      vol2 = vol1;
    }

  zfsd_mutex_unlock (&volume_mutex);
  /* Check validity of the operation.  */
  if (vd1)
    {
      r = validate_operation_on_virtual_directory (vd1, from_name, &ifh1);
      if (r != ZFS_OK)
	goto zfs_rename_error_unlock_vd;
      zfsd_mutex_unlock (&vd1->mutex);
      vd1 = NULL;
    }
  if (vd2)
    {
      if (vd2 != vd1)
	{
	  r = validate_operation_on_virtual_directory (vd2, to_name, &ifh2);
	  if (r != ZFS_OK)
	    goto zfs_rename_error_unlock_vd;
	  zfsd_mutex_unlock (&vd2->mutex);
	}
      else
	ifh2 = ifh1;
      vd2 = NULL;
    }
  zfsd_mutex_unlock (&vd_mutex);
  if (ifh1->master_fh.dev != ifh2->master_fh.dev)
    {
      r = EXDEV;
      goto zfs_rename_error;
    }

  if (vol1->local_path)
    r = local_rename (ifh1, from_name, ifh2, to_name, vol1);
  else if (vol1->master != this_node)
    r = remote_rename (ifh1, from_name, ifh2, to_name, vol1);
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_fh ifh;

      /* Delete internal file handle in htab because it is outdated.  */
      /* FIXME? move the internal_fh to another directory? */
      ifh = fh_lookup_name (vol1, ifh1, to_name->str);
      if (ifh)
	{
	  CHECK_MUTEX_LOCKED (&ifh->mutex);

	  internal_fh_destroy (ifh, vol1);
	}
      ifh = fh_lookup_name (vol2, ifh2, to_name->str);
      if (ifh)
	{
	  CHECK_MUTEX_LOCKED (&ifh->mutex);

	  internal_fh_destroy (ifh, vol2);
	}
    }

  zfsd_mutex_unlock (&ifh1->mutex);
  zfsd_mutex_unlock (&ifh2->mutex);
  zfsd_mutex_unlock (&vol2->mutex);

  return r;

zfs_rename_error_unlock_volume:
  zfsd_mutex_unlock (&volume_mutex);

zfs_rename_error_unlock_vd:
  zfsd_mutex_unlock (&vd_mutex);

zfs_rename_error:
  if (ifh1)
    zfsd_mutex_unlock (&ifh1->mutex);
  if (ifh2 && ifh2 != ifh1)
    zfsd_mutex_unlock (&ifh2->mutex);
  if (vd1)
    zfsd_mutex_unlock (&vd1->mutex);
  if (vd2 && vd2 != vd1)
    zfsd_mutex_unlock (&vd2->mutex);
  if (vol2)
    zfsd_mutex_unlock (&vol2->mutex);

  return r;
}

/* Link local file FROM to be a file with NAME in directory DIR
   on volume VOL.  */

static int
local_link (internal_fh from, internal_fh dir, string *name, volume vol)
{
  char *path1, *path2;
  int r;

  CHECK_MUTEX_LOCKED (&from->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);
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

static int
remote_link (internal_fh from, internal_fh dir, string *name, volume vol)
{
  link_args args;
  thread *t;
  int32_t r;

  CHECK_MUTEX_LOCKED (&from->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);
  CHECK_MUTEX_LOCKED (&vol->mutex);

  args.from = from->master_fh;
  args.to.dir = dir->master_fh;
  args.to.name = *name;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_link_client (t, &args, vol->master);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Link file FROM to be a file with NAME in directory DIR.  */

int
zfs_link (zfs_fh *from, zfs_fh *dir, string *name)
{
  volume vol1 = NULL, vol2 = NULL;
  internal_fh ifh1 = NULL, ifh2 = NULL;
  virtual_dir vd1 = NULL, vd2 = NULL;
  int r = ZFS_OK;

  /* Lookup FROM.  */
  zfsd_mutex_lock (&volume_mutex);
  zfsd_mutex_lock (&vd_mutex);
  if (!fh_lookup_nolock (from, &vol1, &ifh1, &vd1))
    {
      zfsd_mutex_unlock (&volume_mutex);
      zfsd_mutex_unlock (&vd_mutex);
      return ESTALE;
    }
  if (!vol1)
    {
      r = EROFS;
      goto zfs_link_error_unlock_volume;
    }
  /* Because LINK operation is allowed only within same volume
     we may unlock VOL1.  Moreover we have to do so to avoid deadlock.  */
  zfsd_mutex_unlock (&vol1->mutex);

  /* Lookup DIR.  */
  if (!fh_lookup_nolock (dir, &vol2, &ifh2, &vd2))
    {
      r = ESTALE;
      goto zfs_link_error_unlock_volume;
    }
  zfsd_mutex_unlock (&volume_mutex);
  if (!vol2)
    {
      r = EROFS;
      goto zfs_link_error_unlock_vd;
    }
  if (vol1 != vol2)
    {
      r = EXDEV;
      goto zfs_link_error_unlock_vd;
    }

  /* Check validity of the operation.  */
  if (vd1 && vd1->vol)
    {
      r = update_volume_root (vd1->vol, &ifh1);
      if (r == ZFS_OK)
	{
	  zfsd_mutex_unlock (&vd1->mutex);
	  vd1 = NULL;
	}
    }
  if (vd1)
    {
      r = EROFS;
      goto zfs_link_error_unlock_vd;
    }
  if (vd2)
    {
      r = validate_operation_on_virtual_directory (vd2, name, &ifh2);
      if (r != ZFS_OK)
	goto zfs_link_error_unlock_vd;
      zfsd_mutex_unlock (&vd2->mutex);
      vd2 = NULL;
    }
  zfsd_mutex_unlock (&vd_mutex);
  if (ifh1->master_fh.dev != ifh2->master_fh.dev)
    {
      r = EXDEV;
      goto zfs_link_error;
    }

  if (vol1->local_path)
    r = local_link (ifh1, ifh2, name, vol1);
  else if (vol1->master != this_node)
    r = remote_link (ifh1, ifh2, name, vol1);
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_fh ifh;

      /* Delete internal file handle in htab because it is outdated.  */
      ifh = fh_lookup_name (vol2, ifh2, name->str);
      if (ifh)
	{
	  CHECK_MUTEX_LOCKED (&ifh->mutex);

	  internal_fh_destroy (ifh, vol2);
	}
    }

  zfsd_mutex_unlock (&ifh1->mutex);
  zfsd_mutex_unlock (&ifh2->mutex);
  zfsd_mutex_unlock (&vol2->mutex);

  return r;

zfs_link_error_unlock_volume:
  zfsd_mutex_unlock (&volume_mutex);

zfs_link_error_unlock_vd:
  zfsd_mutex_unlock (&vd_mutex);

zfs_link_error:
  if (ifh1)
    zfsd_mutex_unlock (&ifh1->mutex);
  if (ifh2)
    zfsd_mutex_unlock (&ifh2->mutex);
  if (vd1)
    zfsd_mutex_unlock (&vd1->mutex);
  if (vd2)
    zfsd_mutex_unlock (&vd2->mutex);
  if (vol2)
    zfsd_mutex_unlock (&vol2->mutex);

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
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
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

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  if (!fh_lookup_nolock (dir, &vol, &idir, &pvd))
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

/* Read local symlink FH on volume VOL.  */

static int
local_readlink (read_link_res *res, internal_fh fh, volume vol)
{
  char *path;
  char buf[ZFS_MAXDATA + 1];
  int r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  path = build_local_path (vol, fh);
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

static int
remote_readlink (read_link_res *res, internal_fh fh, volume vol)
{
  thread *t;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&fh->mutex);

  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_readlink_client (t, &fh->master_fh, vol->master);

  if (r == ZFS_OK)
    {
      if (!decode_zfs_path (&t->dc_reply, &res->path)
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

/* Read symlink FH.  */

int
zfs_readlink (read_link_res *res, zfs_fh *fh)
{
  volume vol;
  internal_fh ifh;
  int r = ZFS_OK;

  if (VIRTUAL_FH_P (*fh))
    return EINVAL;

  /* Lookup FH.  */
  if (!fh_lookup (fh, &vol, &ifh, NULL))
    return ESTALE;

  if (vol->local_path)
    r = local_readlink (res, ifh, vol);
  else if (vol->master != this_node)
    r = remote_readlink (res, ifh, vol);
  else
    abort ();

  zfsd_mutex_unlock (&ifh->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  return r;
}

/* Create local symlink NAME in directory DIR on volume VOL pointing to TO,
   set its attributes according to ATTR.  */

static int
local_symlink (internal_fh dir, string *name, string *to, sattr *attr,
	       volume vol)
{
  char *path;
  int r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = symlink (path, to->str);
  if (r != 0)
    {
      free (path);
      return errno;
    }

  r = local_setattr_path (NULL, path, attr, vol);
  free (path);
  if (r != ZFS_OK)
    return r;

  return ZFS_OK;
}

/* Create remote symlink NAME in directory DIR on volume VOL pointing to TO,
   set its attributes according to ATTR.  */

static int
remote_symlink (internal_fh dir, string *name, string *to, sattr *attr,
		volume vol)
{
  symlink_args args;
  thread *t;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);

  args.from.dir = dir->master_fh;
  args.from.name = *name;
  args.to = *to;
  args.attr = *attr;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_symlink_client (t, &args, vol->master);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Create symlink NAME in directory DIR pointing to TO,
   set its attributes according to ATTR.  */

int
zfs_symlink (zfs_fh *dir, string *name, string *to, sattr *attr)
{
  volume vol;
  internal_fh idir;
  virtual_dir pvd;
  int r = ZFS_OK;

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  if (!fh_lookup_nolock (dir, &vol, &idir, &pvd))
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
    r = local_symlink (idir, name, to, attr, vol);
  else if (vol->master != this_node)
    r = remote_symlink (idir, name, to, attr, vol);
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_fh ifh;

      /* Delete internal file handle in htab because it is outdated.  */
      ifh = fh_lookup_name (vol, idir, name->str);
      if (ifh)
	{
	  CHECK_MUTEX_LOCKED (&ifh->mutex);

	  internal_fh_destroy (ifh, vol);
	}
      zfsd_mutex_unlock (&ifh->mutex);
    }

  zfsd_mutex_unlock (&idir->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  return r;
}

/* Create local special file NAME of type TYPE in directory DIR,
   set the attributes according to ATTR.
   If device is being created RDEV is its number.  */

static int
local_mknod (internal_fh dir, string *name, sattr *attr, ftype type,
	     unsigned int rdev, volume vol)
{
  char *path;
  int r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);

  path = build_local_path_name (vol, dir, name->str);
  r = mknod (path, attr->mode | ftype2mode[type], rdev);
  if (r != 0)
    {
      free (path);
      return errno;
    }

  r = local_setattr_path (NULL, path, attr, vol);
  free (path);
  if (r != ZFS_OK)
    return r;

  return ZFS_OK;
}

/* Create remote special file NAME of type TYPE in directory DIR,
   set the attributes according to ATTR.
   If device is being created RDEV is its number.  */

static int
remote_mknod (internal_fh dir, string *name, sattr *attr, ftype type,
	      unsigned int rdev, volume vol)
{
  mknod_args args;
  thread *t;
  int32_t r;

  CHECK_MUTEX_LOCKED (&vol->mutex);
  CHECK_MUTEX_LOCKED (&dir->mutex);

  args.where.dir = dir->master_fh;
  args.where.name = *name;
  args.attr = *attr;
  args.type = type;
  args.rdev = rdev;
  t = (thread *) pthread_getspecific (thread_data_key);

  zfsd_mutex_lock (&node_mutex);
  zfsd_mutex_lock (&vol->master->mutex);
  zfsd_mutex_unlock (&node_mutex);
  r = zfs_proc_mknod_client (t, &args, vol->master);

  if (r >= ZFS_LAST_DECODED_ERROR)
    {
      if (!finish_decoding (&t->dc_reply))
	r = ZFS_INVALID_REPLY;
    }

  if (r >= ZFS_ERROR_HAS_DC_REPLY)
    recycle_dc_to_volume_master (&t->dc_reply, vol);
  return r;
}

/* Create special file NAME of type TYPE in directory DIR,
   set the attributes according to ATTR.
   If device is being created RDEV is its number.  */

int
zfs_mknod (zfs_fh *dir, string *name, sattr *attr, ftype type,
	   unsigned int rdev)
{
  volume vol;
  internal_fh idir;
  virtual_dir pvd;
  int r = ZFS_OK;

  /* Lookup DIR.  */
  zfsd_mutex_lock (&volume_mutex);
  if (VIRTUAL_FH_P (*dir))
    zfsd_mutex_lock (&vd_mutex);
  if (!fh_lookup_nolock (dir, &vol, &idir, &pvd))
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
    {
      r = local_mknod (idir, name, attr, type, rdev, vol);
      if (r == ZFS_OK)
	{
	  if (vol->master != this_node)
	    r = remote_mknod (idir, name, attr, type, rdev, vol);
	}
    }
  else if (vol->master != this_node)
    {
      r = remote_mknod (idir, name, attr, type, rdev, vol);
    }
  else
    abort ();

  if (r == ZFS_OK)
    {
      internal_fh ifh;

      /* Delete internal file handle in htab because it is outdated.  */
      ifh = fh_lookup_name (vol, idir, name->str);
      if (ifh)
	{
	  CHECK_MUTEX_LOCKED (&ifh->mutex);

	  internal_fh_destroy (ifh, vol);
	}
      zfsd_mutex_unlock (&ifh->mutex);
    }

  zfsd_mutex_unlock (&idir->mutex);
  zfsd_mutex_unlock (&vol->mutex);

  return r;
}
