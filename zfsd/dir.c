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
#include "fh.h"
#include "dir.h"
#include "log.h"
#include "memory.h"
#include "varray.h"
#include "volume.h"
#include "zfs_prot.h"

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int
zfs_extended_lookup (zfs_fh *fh, zfs_fh *dir, char *path)
{
  zfs_fh tmp;
  char *s;
  int r;

  tmp = (*path == '/') ? root_fh : *dir;
  while (*path)
    {
      while (*path == '/')
	path++;

      s = path;
      while (*path != 0 && *path != '/')
	path++;
      if (*path == '/')
	*path++ = 0;

      r = zfs_lookup (&tmp, &tmp, s);
      if (r)
	return r;
    }

  *fh = tmp;
  return 0;
}

/* Return the local path of file for file handle FH on volume VOL.  */

static char *
build_local_path (volume vol, internal_fh fh)
{
  internal_fh tmp;
  unsigned int n;
  varray v;

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

  return xstrconcat_varray (&v);
}

/* Return the local path of file NAME in directory FH on volume VOL.  */

static char *
build_local_path_name (volume vol, internal_fh fh, const char *name)
{
  internal_fh tmp;
  unsigned int n;
  varray v;

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

  return xstrconcat_varray (&v);
}

static int
local_lookup (zfs_fh *fh, internal_fh dir, const char *name, volume vol,
	      unsigned int *dev, unsigned int *ino)
{
  struct stat st;
  char *path;

  path = build_local_path_name (vol, dir, name);
  if (lstat (path, &st) != 0)
    return errno;

  fh->sid = dir->client_fh.sid;
  fh->vid = dir->client_fh.vid;
  fh->dev = st.st_dev;
  fh->ino = st.st_ino;

  return 0;
}

static int
remote_lookup (zfs_fh *fh, internal_fh dir, const char *name, volume vol,
	       unsigned int *dev, unsigned int *ino)
{
  return ESTALE;
}

static void
update_root (volume vol, internal_fh *ifh)
{
  zfs_fh new_root;

  new_root.sid = vol->master->id;
  new_root.vid = vol->id;
  new_root.dev = 1;
  new_root.ino = 1;
  
  /* FIXME: */
  if ((vol->flags & VOLUME_LOCAL) && !(vol->flags & VOLUME_COPY))
    {
      /* get local root zfs_fh */
    }
  else
    {
      /* get remote root zfs_fh */
    }

  if (!ZFS_FH_EQ (vol->root_fh, new_root))
    {
      htab_empty (vol->fh_htab_name);
      htab_empty (vol->fh_htab);

      vol->root_fh = new_root;
      *ifh = internal_fh_create (/*FIXME*/&vol->root_fh, &vol->root_fh, NULL,
				 vol, "");
    }
}

/* Lookup NAME in directory DIR and store it to FH. Return 0 on success.  */

int
zfs_lookup (zfs_fh *fh, zfs_fh *dir, const char *name)
{
  volume vol;
  internal_fh idir, ifh;
  virtual_dir vd, pvd;

  /* Lookup the DIR.  */
  if (!fh_lookup (dir, &vol, &idir, &pvd))
    return ESTALE;

  /* FIXME: update_directory - pokud to je mountpoint, zepta se na root_fh a
     pripadne zaktualizuje, upravi idir*/
  if (pvd && pvd->vol)
    update_root (pvd->vol, &idir);

  if (idir)
    {
      unsigned int dev;
      unsigned int ino;
      int r;

      if (vol->flags & VOLUME_LOCAL)
	r = local_lookup (fh, idir, name, vol, &dev, &ino);
      else
	r = remote_lookup (fh, idir, name, vol, &dev, &ino);
      if (r)
	return r;

      /* FIXME: update hash tables of fh. */
      ifh = fh_lookup_name (vol, idir, name);
      if (!ifh)
	ifh = internal_fh_create (fh, fh, idir, vol, name);
      return 0;
    }
  else	/* if (idir == NULL) */
    {
      vd = vd_lookup_name (pvd, name);
      if (vd)
	{
	  *fh = vd->fh;
	  return 0;
	}
      else
	abort ();
    }

#if 0

  /* Lookup the DIR.  */
  idir = (internal_fh) fh_lookup (dir);
  if (!idir)
    return ESTALE;

  if (VIRTUAL_FH_P (idir->client_fh) && !idir->vd->active && idir->vd->real_fh)
    idir = idir->vd->real_fh;
  else
    return ESTALE;

  /* Lookup the NAME in DIR.  */
  ifh = fh_lookup_name (idir, name);
  if (ifh && VIRTUAL_FH_P (ifh->client_fh) && ifh->vd->active)
    {
      *fh = ifh->client_fh;
      return 0;
    }

  vol = volume_lookup (idir->client_fh.vid);
  if (vol->flags & VOLUME_COPY)
    {
      /* update directory */
    }

  if (vol->flags & VOLUME_LOCAL)
    r = local_lookup (idir, fh, vol, &dev, &ino);
  else
    r = remote_lookup (idir, fh, vol, &dev, &ino);

  if (r)
    return r;



  if (!ifh)
    {
      if (VIRTUAL_FH_P (idir->client_fh))
	{
	}
    }
#endif
  return ESTALE;
}

int
zfs_open (zfs_fh *fh)
{

  return 0;
}

int
zfs_open_by_name (zfs_fh *fh, zfs_fh *dir, const char *name, int flags,
		  sattr *attr)
{
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
  return 0;
}

int
zfs_getattr (fattr *fa, zfs_fh *fh)
{
  return 0;
}

int
zfs_setattr (fattr *fa, zfs_fh *fh, unsigned int valid, sattr *sa)
{
  return 0;
}

int
zfs_close (zfs_fh *fh)
{
  return 0;
}
