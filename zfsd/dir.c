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
#include "zfs_prot.h"

/* Return the local path of file for file handle FH on volume VOL.  */

char *
build_local_path (volume vol, internal_fh fh)
{
  internal_fh tmp;
  unsigned int n;
  varray v;
  char *r;

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

/* Store the local file handle of root of volume VOL to LOCAL_FH
   and its attributes to ATTR.  */

static int
get_volume_root_local (volume vol, zfs_fh *local_fh, fattr *attr)
{
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

  return ZFS_OK;
}

/* Store the remote file handle of root of volume VOL to REMOTE_FH
   and its attributes to ATTR.  */

static int
get_volume_root_remote (volume vol, zfs_fh *remote_fh, fattr *attr)
{
  int32_t r;

  /* The volume is completelly remote or we have a copy of the volume.
     Call the remote function only when we need the file handle.  */
  if (vol->master != this_node)
    {
      volume_root_args args;
      thread *t;

      t = (thread *) pthread_getspecific (server_thread_key);
      args.vid = vol->id;
      zfsd_mutex_lock (&vol->master->mutex);	/* FIXME: temporary */
      r = zfs_proc_volume_root_client (t, &args, vol->master);
      zfsd_mutex_unlock (&vol->master->mutex);	/* FIXME: temporary */
      if (r == ZFS_OK)
	{
	  if (!decode_zfs_fh (&t->u.server.dc, remote_fh)
	      || !decode_fattr (&t->u.server.dc, attr)
	      || !finish_decoding (&t->u.server.dc))
	    return ZFS_INVALID_REPLY;
	}
    }
  else
    abort ();

  return r;
}

/* Get file handle of root of volume VOL, store the local file handle to
   LOCAL_FH and master's file handle to MASTER_FH, if defined.  */

static int
get_volume_root (volume vol, zfs_fh *local_fh, zfs_fh *master_fh, fattr *attr)
{
  int32_t r = ZFS_OK;

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

  if (!ZFS_FH_EQ (vol->local_root_fh, local_fh)
      || !ZFS_FH_EQ (vol->master_root_fh, master_fh))
    {
      /* FIXME? delete only FHs which are not open now?  */
      htab_empty (vol->fh_htab_name);
      htab_empty (vol->fh_htab);

      vol->local_root_fh = local_fh;
      vol->master_root_fh = master_fh;
      *ifh = internal_fh_create (&local_fh, &master_fh, NULL, vol, "", &attr);
    }

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

  path = build_local_path_name (vol, dir, name->str);
  r = local_getattr (&res->attr, path, vol);
  free (path);
  if (r != ZFS_OK)
    return errno;

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

  args.dir = dir->master_fh;
  args.name = *name;
  t = (thread *) pthread_getspecific (server_thread_key);

//  zfsd_mutex_lock (&vol->master->mutex);
  r = zfs_proc_lookup_client (t, &args, vol->master);
//  zfsd_mutex_unlock (&vol->master->mutex);
  if (r == ZFS_OK)
    {
      if (!decode_dir_op_res (&t->u.server.dc, res)
	  || !finish_decoding (&t->u.server.dc))
	return ZFS_INVALID_REPLY;
    }

  return r;
}

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int
zfs_lookup (dir_op_res *res, zfs_fh *dir, string *name)
{
  volume vol;
  internal_fh idir, ifh;
  virtual_dir vd, pvd;
  dir_op_res master_res;

  /* Lookup the DIR.  */
  if (!fh_lookup (dir, &vol, &idir, &pvd))
    return ESTALE;

  if (pvd)
    {
      vd = vd_lookup_name (pvd, name->str);
      if (vd)
	{
	  res->file = vd->fh;
	  res->attr = vd->attr;
	  return ZFS_OK;
	}

      if (pvd->vol)
	{
	  int r;

	  zfsd_mutex_lock (&pvd->vol->mutex);	/* FIXME: temporary */
	  r = update_volume_root (pvd->vol, &idir);
	  zfsd_mutex_unlock (&pvd->vol->mutex);	/* FIXME: temporary */
	  if (r != ZFS_OK)
	    return r;
	}
      else
	return ENOENT;
    }

  /* TODO: update directory */

  if (idir)
    {
      int r;

      if (vol->local_path)
	{
	  r = local_lookup (res, idir, name, vol);
	  if (r != ZFS_OK)
	    return r;

	  if (vol->master == this_node)
	    master_res.file = res->file;
	  else
	    {
	      r = remote_lookup (&master_res, idir, name, vol);
	      if (r != ZFS_OK)
		return r;
	    }
	}
      else if (vol->master != this_node)
	{
	  r = remote_lookup (res, idir, name, vol);
	  if (r != ZFS_OK)
	    return r;

	  master_res.file = res->file;
	}
      else
	abort ();

      /* Update internal file handles in htab.  */
      zfsd_mutex_lock (&vol->mutex);	/* FIXME: temporary */
      ifh = fh_lookup_name (vol, idir, name->str);
      if (ifh)
	{
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
      zfsd_mutex_unlock (&vol->mutex);	/* FIXME: temporary */

      return ZFS_OK;
    }
  else
    abort ();

  return ESTALE;
}
